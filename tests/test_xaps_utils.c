/*
 * Unit tests for xaps-utils: get_real_mbox_user(), str_free_i(), xaps_init(),
 * and the push-notification driver deinit/cleanup helpers.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Pull in the stubs so all Dovecot types resolve */
#include "dovecot_stubs/dovecot_stubs.h"

/* Source under test — we include the .c files directly to test internal functions */
#include "../src/xaps-settings.h"
#include "../src/xaps-settings.c"
#include "../src/xaps-utils.c"

/* ---- minimal test harness ---- */
static int tests_run = 0, tests_passed = 0;

extern void test_reset_logs(void);
extern const char *test_get_last_error(void);
extern const char *test_get_last_debug(void);

#define ASSERT_STREQ(a, b) do { \
    const char *_a = (a), *_b = (b); \
    if (strcmp(_a, _b) != 0) { \
        fprintf(stderr, "  FAIL: expected \"%s\", got \"%s\"\n", _b, _a); \
        return 0; \
    } \
} while(0)

#define ASSERT_NULL(ptr) do { \
    if ((ptr) != NULL) { \
        fprintf(stderr, "  FAIL: expected NULL\n"); \
        return 0; \
    } \
} while(0)

#define ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        fprintf(stderr, "  FAIL: expected non-NULL\n"); \
        return 0; \
    } \
} while(0)

#define RUN_TEST(fn) do { \
    tests_run++; \
    printf("  %-50s ", #fn); \
    if (fn()) { tests_passed++; printf("PASS\n"); } \
    else { printf("FAILED\n"); } \
} while(0)

/* ---- Helper: create a mock mail_user ---- */
static struct mail_user *make_user(const char *username, const char **userdb_fields) {
    struct mail_user *u = i_new(struct mail_user, 1);
    u->username = i_strdup(username);
    u->userdb_fields = userdb_fields;
    u->event = NULL;
    return u;
}

/* ---- Tests for get_real_mbox_user ---- */

/* No user_lookup configured -> returns original username */
static int test_no_user_lookup(void) {
    xaps_global = i_new(struct xaps_config, 1);
    xaps_global->user_lookup = NULL;

    struct mail_user *u = make_user("alice@example.com", NULL);
    const char *result = get_real_mbox_user(u);
    ASSERT_STREQ(result, "alice@example.com");

    i_free((char*)u->username);
    i_free(u);
    i_free(xaps_global);
    xaps_global = NULL;
    return 1;
}

/* user_lookup is empty string -> returns original username */
static int test_empty_user_lookup(void) {
    xaps_global = i_new(struct xaps_config, 1);
    xaps_global->user_lookup = "";

    struct mail_user *u = make_user("bob@example.com", NULL);
    const char *result = get_real_mbox_user(u);
    ASSERT_STREQ(result, "bob@example.com");

    i_free((char*)u->username);
    i_free(u);
    i_free(xaps_global);
    xaps_global = NULL;
    return 1;
}

/* user_lookup configured but userdb_fields is NULL -> returns original username */
static int test_null_userdb_fields(void) {
    xaps_global = i_new(struct xaps_config, 1);
    xaps_global->user_lookup = "mail";

    struct mail_user *u = make_user("carol@example.com", NULL);
    const char *result = get_real_mbox_user(u);
    ASSERT_STREQ(result, "carol@example.com");

    i_free((char*)u->username);
    i_free(u);
    i_free(xaps_global);
    xaps_global = NULL;
    return 1;
}

/* user_lookup configured, key found in userdb_fields */
static int test_lookup_key_found(void) {
    xaps_global = i_new(struct xaps_config, 1);
    xaps_global->user_lookup = "mail";

    const char *fields[] = { "uid=1000", "mail=alice@example.com", NULL };
    struct mail_user *u = make_user("alice", fields);
    const char *result = get_real_mbox_user(u);
    ASSERT_STREQ(result, "alice@example.com");

    i_free((char*)u->username);
    i_free(u);
    i_free(xaps_global);
    xaps_global = NULL;
    return 1;
}

/* user_lookup configured, key NOT found -> returns original username */
static int test_lookup_key_not_found(void) {
    xaps_global = i_new(struct xaps_config, 1);
    xaps_global->user_lookup = "mail";

    const char *fields[] = { "uid=1000", "shell=/bin/bash", NULL };
    struct mail_user *u = make_user("dave", fields);
    const char *result = get_real_mbox_user(u);
    ASSERT_STREQ(result, "dave");

    i_free((char*)u->username);
    i_free(u);
    i_free(xaps_global);
    xaps_global = NULL;
    return 1;
}

/* user_lookup key is a prefix of another key but not an exact match */
static int test_lookup_partial_prefix(void) {
    xaps_global = i_new(struct xaps_config, 1);
    xaps_global->user_lookup = "mail";

    const char *fields[] = { "mailbox=/var/mail/eve", NULL };
    struct mail_user *u = make_user("eve", fields);
    const char *result = get_real_mbox_user(u);
    /* "mailbox" != "mail" (mailbox doesn't have '=' at position 4) */
    ASSERT_STREQ(result, "eve");

    i_free((char*)u->username);
    i_free(u);
    i_free(xaps_global);
    xaps_global = NULL;
    return 1;
}

/* userdb_fields has a field with key= but empty value */
static int test_lookup_empty_value(void) {
    xaps_global = i_new(struct xaps_config, 1);
    xaps_global->user_lookup = "mail";

    const char *fields[] = { "mail=", NULL };
    struct mail_user *u = make_user("frank", fields);
    const char *result = get_real_mbox_user(u);
    ASSERT_STREQ(result, "");

    i_free((char*)u->username);
    i_free(u);
    i_free(xaps_global);
    xaps_global = NULL;
    return 1;
}

/* lookup key itself appears as a prefix with '=' immediately after key length,
   but multiple fields: the correct match is picked. */
static int test_lookup_middle_field(void) {
    xaps_global = i_new(struct xaps_config, 1);
    xaps_global->user_lookup = "mail";

    const char *fields[] = { "uid=1000", "mail=grace@example.com", "gid=1000", NULL };
    struct mail_user *u = make_user("grace", fields);
    const char *result = get_real_mbox_user(u);
    ASSERT_STREQ(result, "grace@example.com");

    i_free((char*)u->username);
    i_free(u);
    i_free(xaps_global);
    xaps_global = NULL;
    return 1;
}

/* ---- Tests for str_free_i ---- */
static int test_str_free_i_nulls_out(void) {
    string_t *str = str_new(NULL, 64);
    str_append(str, "hello");
    str_free_i(str);
    return 1;
}

/* ---- Tests for xaps_init ---- */

/* xaps_init with default (empty) settings -> settings_get fails and logs error */
static int test_xaps_init_missing_url(void) {
    xaps_global = NULL;
    test_set_settings_url(NULL);

    struct mail_user *u = make_user("testuser", NULL);
    test_reset_logs();
    xaps_init(u, "/notify", NULL);

    const char *err = test_get_last_error();
    if (err[0] == '\0') {
        fprintf(stderr, "  FAIL: expected error for missing url\n");
        i_free((char*)u->username); i_free(u);
        push_notification_driver_xaps_cleanup();
        return 0;
    }
    /* xaps_global becomes NULL because settings failed before allocation? No:
       xaps_global is allocated first, then settings. So it's non-NULL. */
    i_free((char*)u->username);
    i_free(u);
    push_notification_driver_xaps_cleanup();
    return 1;
}

/* xaps_init success path: valid URL, user_lookup set */
static int test_xaps_init_success_with_lookup(void) {
    xaps_global = NULL;
    test_set_settings_url("http://[::1]:11619");
    test_set_settings_user_lookup("mail");

    struct mail_user *u = make_user("testuser", NULL);
    xaps_init(u, "/notify", NULL);

    ASSERT_NOT_NULL(xaps_global);
    ASSERT_NOT_NULL(xaps_global->http_client);
    ASSERT_NOT_NULL(xaps_global->http_url);
    ASSERT_STREQ(xaps_global->http_url->path, "/notify");
    ASSERT_STREQ(xaps_global->user_lookup, "mail");

    i_free((char*)u->username);
    i_free(u);
    push_notification_driver_xaps_cleanup();
    return 1;
}

/* xaps_init success path: user_lookup empty -> NULL */
static int test_xaps_init_success_no_lookup(void) {
    xaps_global = NULL;
    /* Note: settings defaults keep xaps_user_lookup as "" unless overridden */
    test_set_settings_url("http://[::1]:11619");
    test_set_settings_user_lookup("");

    struct mail_user *u = make_user("testuser", NULL);
    xaps_init(u, "/notify", NULL);

    ASSERT_NOT_NULL(xaps_global);
    ASSERT_NULL(xaps_global->user_lookup);
    ASSERT_NOT_NULL(xaps_global->http_client);

    i_free((char*)u->username);
    i_free(u);
    push_notification_driver_xaps_cleanup();
    return 1;
}

/* xaps_init success path reuses an existing http_client (no re-init) */
static int test_xaps_init_reuses_http_client(void) {
    xaps_global = i_new(struct xaps_config, 1);
    xaps_global->http_client = (void*)0x1234;
    test_set_settings_url("http://[::1]:11619");

    struct mail_user *u = make_user("testuser", NULL);
    xaps_init(u, "/register", NULL);

    ASSERT_NOT_NULL(xaps_global->http_url);
    ASSERT_STREQ(xaps_global->http_url->path, "/register");
    /* http_client should still be the hand-set sentinel (pointer equality) */
    if (xaps_global->http_client != (void*)0x1234) {
        fprintf(stderr, "  FAIL: http_client was re-initialized\n");
        i_free((char*)u->username); i_free(u);
        push_notification_driver_xaps_cleanup();
        return 0;
    }

    i_free((char*)u->username);
    i_free(u);
    push_notification_driver_xaps_cleanup();
    return 1;
}

/* xaps_url is valid but http_url_parse fails -> error logged, no http_url */
static int test_xaps_init_url_parse_failure(void) {
    xaps_global = NULL;
    test_set_settings_url("http://[::1]:11619");
    test_set_http_url_parse_fail(TRUE);

    struct mail_user *u = make_user("testuser", NULL);
    test_reset_logs();
    xaps_init(u, "/notify", NULL);

    const char *err = test_get_last_error();
    int ok = (err[0] != '\0');
    if (xaps_global == NULL || xaps_global->http_url != NULL ||
        xaps_global->http_client != NULL)
        ok = 0;
    if (!ok)
        fprintf(stderr, "  FAIL: expected url parse failure path\n");

    test_set_http_url_parse_fail(FALSE);
    i_free((char*)u->username);
    i_free(u);
    push_notification_driver_xaps_cleanup();
    return ok;
}

/* xaps_url is valid but http_client_init_auto fails -> error logged */
static int test_xaps_init_client_init_failure(void) {
    xaps_global = NULL;
    test_set_settings_url("http://[::1]:11619");
    test_set_http_client_init_fail(TRUE);

    struct mail_user *u = make_user("testuser", NULL);
    test_reset_logs();
    xaps_init(u, "/notify", NULL);

    const char *err = test_get_last_error();
    int ok = (err[0] != '\0');
    if (xaps_global == NULL || xaps_global->http_client != NULL)
        ok = 0;
    if (!ok)
        fprintf(stderr, "  FAIL: expected http client init failure path\n");

    test_set_http_client_init_fail(FALSE);
    i_free((char*)u->username);
    i_free(u);
    push_notification_driver_xaps_cleanup();
    return ok;
}

/* xaps_settings_get returns -1 when settings_get itself fails */
static int test_settings_get_failure(void) {
    test_set_settings_get_fail(TRUE);
    test_reset_logs();

    const struct xaps_settings *set_r = NULL;
    const char *error_r = NULL;
    int rc = xaps_settings_get(NULL, &set_r, &error_r);

    int ok = (rc == -1 && error_r != NULL);
    if (!ok)
        fprintf(stderr, "  FAIL: expected settings_get failure path\n");

    test_set_settings_get_fail(FALSE);
    return ok;
}

/* deinit with NULL global -> no crash */
static int test_deinit_null_global(void) {
    xaps_global = NULL;
    push_notification_driver_xaps_deinit(NULL);
    return 1;
}

/* deinit with global and http_client -> waits */
static int test_deinit_with_client(void) {
    xaps_global = i_new(struct xaps_config, 1);
    xaps_global->http_client = (void*)0x1234;
    push_notification_driver_xaps_deinit(NULL);
    i_free(xaps_global);
    xaps_global = NULL;
    return 1;
}

/* cleanup with NULL global -> no crash */
static int test_cleanup_null_global(void) {
    xaps_global = NULL;
    push_notification_driver_xaps_cleanup();
    return 1;
}

/* cleanup with global and http_client -> deinit http and free */
static int test_cleanup_with_client(void) {
    xaps_global = i_new(struct xaps_config, 1);
    xaps_global->http_client = (void*)0x1234;
    push_notification_driver_xaps_cleanup();
    ASSERT_NULL(xaps_global);
    return 1;
}

/* ---- main ---- */
int main(void) {
    printf("=== test_xaps_utils ===\n");

    RUN_TEST(test_no_user_lookup);
    RUN_TEST(test_empty_user_lookup);
    RUN_TEST(test_null_userdb_fields);
    RUN_TEST(test_lookup_key_found);
    RUN_TEST(test_lookup_key_not_found);
    RUN_TEST(test_lookup_partial_prefix);
    RUN_TEST(test_lookup_empty_value);
    RUN_TEST(test_lookup_middle_field);
    RUN_TEST(test_str_free_i_nulls_out);
    RUN_TEST(test_xaps_init_missing_url);
    RUN_TEST(test_xaps_init_success_with_lookup);
    RUN_TEST(test_xaps_init_success_no_lookup);
    RUN_TEST(test_xaps_init_reuses_http_client);
    RUN_TEST(test_xaps_init_url_parse_failure);
    RUN_TEST(test_xaps_init_client_init_failure);
    RUN_TEST(test_settings_get_failure);
    RUN_TEST(test_deinit_null_global);
    RUN_TEST(test_deinit_with_client);
    RUN_TEST(test_cleanup_null_global);
    RUN_TEST(test_cleanup_with_client);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
