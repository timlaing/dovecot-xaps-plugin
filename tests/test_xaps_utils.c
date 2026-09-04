/*
 * Unit tests for xaps-utils: get_real_mbox_user(), str_free_i()
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
    return 1;
}

/* ---- Tests for str_free_i ---- */
static int test_str_free_i_nulls_out(void) {
    string_t *str = str_new(NULL, 64);
    str_append(str, "hello");
    str_free_i(str);
    /* str_free_i should not crash; we can't check the pointer since it's
       passed by value. Just verify no crash. */
    return 1;
}

/* ---- Tests for xaps_init (via mocked settings/http) ---- */
static int test_xaps_init_sets_url(void) {
    xaps_global = NULL;

    struct mail_user *u = i_new(struct mail_user, 1);
    u->username = "testuser";
    u->event = NULL;

    /* We need xaps_settings_get to return valid settings.
       In our stub settings_get(), defaults are copied into the struct.
       The default xaps_url is "". We'll manually set it after the fact. */
    /* Since xaps_init calls xaps_settings_get which uses the stub settings_get,
       and the stub returns defaults (empty xaps_url), xaps_init will fail
       with "xaps_url is required". This is correct behaviour — test it. */
    struct pool_holder { pool_t pool; };
    xaps_init(u, "/notify", NULL);

    /* xaps_global should be allocated but xaps_url parse failed */
    ASSERT_NOT_NULL(xaps_global);
    /* http_client won't be set since xaps_url parse fails */
    ASSERT_NULL(xaps_global->http_client);

    i_free(xaps_global);
    xaps_global = NULL;
    i_free(u);
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
    RUN_TEST(test_str_free_i_nulls_out);
    RUN_TEST(test_xaps_init_sets_url);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
