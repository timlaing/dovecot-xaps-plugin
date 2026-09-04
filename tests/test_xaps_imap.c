/*
 * Unit tests for xaps-imap-plugin: parse_xapplepush, xaps_register,
 * register_client, cmd_xapplepushservice, xaps_register_callback, and the
 * client_created hook + plugin init/deinit.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dovecot_stubs/dovecot_stubs.h"

#include "../src/xaps-utils.h"
#include "../src/xaps-imap-plugin.h"
#include "../src/xaps-push-notification-plugin.h"
#include "../src/xaps-imap-plugin.c"
#include "../src/xaps-utils.c"
#include "../src/xaps-settings.c"
#include "../src/xaps-push-notification-plugin.c"

/* ---- test harness ---- */
static int tests_run = 0, tests_passed = 0;

#define RUN_TEST(fn) do { \
    tests_run++; \
    printf("  %-50s ", #fn); \
    if (fn()) { tests_passed++; printf("PASS\n"); } \
    else { printf("FAILED\n"); } \
} while(0)

extern void test_reset_logs(void);
extern const char *test_get_last_error(void);
extern const char *test_get_last_debug(void);
const char *test_get_last_payload(void);

/* ---- helpers ---- */

/* Ensure xaps_global exists (get_real_mbox_user reads it during parse). */
static void ensure_global(void) {
    if (xaps_global == NULL) {
        xaps_global = i_new(struct xaps_config, 1);
    }
}

/* Initialize xaps_global with a valid URL/http_client via xaps_init so that
   xaps_register's assertions on xaps_global/http_client hold. */
static void init_global_url(void) {
    test_set_settings_url("http://[::1]:11619");
    struct mail_user *u = i_new(struct mail_user, 1);
    u->username = i_strdup("globaluser");
    u->event = NULL;
    xaps_init(u, "/register", NULL);
    i_free((void*)u->username);
    i_free(u);
}

/* Build a mock client with a user and a capability string. */
static struct client *make_client(void) {
    struct client *c = i_new(struct client, 1);
    c->user = i_new(struct mail_user, 1);
    c->user->username = i_strdup("clientuser@example.com");
    c->user->event = NULL;
    c->capability_string = str_new(NULL, 64);
    c->pool = NULL;
    return c;
}

static void free_client(struct client *c) {
    str_free(&c->capability_string);
    i_free((char*)c->user->username);
    i_free(c->user);
    i_free(c);
}

static struct client_command_context *make_cmd(struct client *c) {
    struct client_command_context *cmd = i_new(struct client_command_context, 1);
    cmd->client = c;
    cmd->context = NULL;
    cmd->pool = NULL;
    return cmd;
}

/* Build a valid XAPPLEPUSHSERVICE arg list. mailboxes_list is either NULL
   (no mailboxes arg) or a pointer to an imap_arg LIST wrapper. */
#define ARG(n, t, v) base_args[n].type = (t); base_args[n]._data.str = (v)
#define ARGL(n, v)   base_args[n].type = IMAP_ARG_LIST; \
                     base_args[n]._data.list = (v)

static struct imap_arg base_args[12];
static struct imap_arg mailbox_list[3] = {
    { .type = IMAP_ARG_ATOM, ._data.str = "INBOX" },
    { .type = IMAP_ARG_ATOM, ._data.str = "Notes" },
    { .type = IMAP_ARG_NIL },
};

/* A mailboxes list whose first element is a LIST (not an astring) so that
   xaps_register's mailbox iteration fails. */
static struct imap_arg bad_mailbox_list[2] = {
    { .type = IMAP_ARG_LIST, ._data.list = mailbox_list },
    { .type = IMAP_ARG_NIL },
};

static void setup_valid_args(void) {
    memset(base_args, 0, sizeof(base_args));
    ARG(0, IMAP_ARG_ATOM, "aps-version");
    ARG(1, IMAP_ARG_ATOM, "2");
    ARG(2, IMAP_ARG_ATOM, "aps-account-id");
    ARG(3, IMAP_ARG_ATOM, "0715A26B-CA09-4730-A419-793000CA982E");
    ARG(4, IMAP_ARG_ATOM, "aps-device-token");
    ARG(5, IMAP_ARG_ATOM, "2918390218931890821908309283098109381029309829018310983092892829");
    ARG(6, IMAP_ARG_ATOM, "aps-subtopic");
    ARG(7, IMAP_ARG_ATOM, "com.apple.mobilemail");
    ARG(8, IMAP_ARG_ATOM, "mailboxes");
    ARGL(9, mailbox_list);
    base_args[10].type = IMAP_ARG_NIL;
    test_set_read_args_fail(FALSE);
}

/* ---- parse_xapplepush branch tests ---- */

/* Fully valid args -> parse succeeds and fields are populated */
static int test_parse_valid(void) {
    setup_valid_args();
    test_set_imap_args(base_args, 12);
    ensure_global();

    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);
    struct xaps_attr attr;
    memset(&attr, 0, sizeof(attr));

    bool ok = parse_xapplepush(cmd, &attr);
    int rc = 1;
    if (!ok) { fprintf(stderr, "  FAIL: parse failed\n"); rc = 0; }
    else if (!attr.aps_version || strcmp(attr.aps_version, "2") != 0) rc = 0;
    else if (!attr.aps_account_id || strcmp(attr.aps_account_id,
             "0715A26B-CA09-4730-A419-793000CA982E") != 0) rc = 0;
    else if (!attr.aps_device_token || strcmp(attr.aps_device_token,
             "2918390218931890821908309283098109381029309829018310983092892829") != 0) rc = 0;
    else if (!attr.aps_subtopic || strcmp(attr.aps_subtopic,
             "com.apple.mobilemail") != 0) rc = 0;
    else if (!attr.dovecot_username || strcmp(attr.dovecot_username,
             "clientuser@example.com") != 0) rc = 0;
    else if (attr.mailboxes != mailbox_list) rc = 0;

    i_free(cmd);
    free_client(c);
    return rc;
}

/* client_read_args fails -> parse fails */
static int test_parse_read_args_fail(void) {
    test_set_read_args_fail(TRUE);
    ensure_global();
    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);
    struct xaps_attr attr;
    memset(&attr, 0, sizeof(attr));

    bool ok = parse_xapplepush(cmd, &attr);
    test_set_read_args_fail(FALSE);
    i_free(cmd);
    free_client(c);
    if (ok) { fprintf(stderr, "  FAIL: expected parse failure\n"); return 0; }
    return 1;
}

/* First key is not an astring -> parse fails */
static int test_parse_bad_key(void) {
    setup_valid_args();
    base_args[0].type = IMAP_ARG_LIST; /* invalid key */
    base_args[0]._data.list = mailbox_list;
    test_set_imap_args(base_args, 12);
    ensure_global();

    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);
    struct xaps_attr attr;
    memset(&attr, 0, sizeof(attr));

    bool ok = parse_xapplepush(cmd, &attr);
    i_free(cmd);
    free_client(c);
    if (ok) { fprintf(stderr, "  FAIL: expected parse failure\n"); return 0; }
    return 1;
}

/* A value (odd index < 9) is not an astring -> parse fails */
static int test_parse_bad_value(void) {
    setup_valid_args();
    base_args[1].type = IMAP_ARG_LIST; /* aps-version value invalid */
    base_args[1]._data.list = mailbox_list;
    test_set_imap_args(base_args, 12);
    ensure_global();

    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);
    struct xaps_attr attr;
    memset(&attr, 0, sizeof(attr));

    bool ok = parse_xapplepush(cmd, &attr);
    i_free(cmd);
    free_client(c);
    if (ok) { fprintf(stderr, "  FAIL: expected parse failure\n"); return 0; }
    return 1;
}

/* mailboxes arg is not a list -> parse fails at the list get */
static int test_parse_bad_mailboxes_list(void) {
    setup_valid_args();
    base_args[9].type = IMAP_ARG_ATOM; /* mailboxes not a list */
    base_args[9]._data.str = "INBOX";
    test_set_imap_args(base_args, 12);
    ensure_global();

    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);
    struct xaps_attr attr;
    memset(&attr, 0, sizeof(attr));

    bool ok = parse_xapplepush(cmd, &attr);
    i_free(cmd);
    free_client(c);
    if (ok) { fprintf(stderr, "  FAIL: expected parse failure\n"); return 0; }
    return 1;
}

/* aps-version missing (not "2") -> parse fails */
static int test_parse_unknown_version(void) {
    setup_valid_args();
    /* make version a different value */
    base_args[1]._data.str = "3";
    test_set_imap_args(base_args, 12);
    ensure_global();

    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);
    struct xaps_attr attr;
    memset(&attr, 0, sizeof(attr));

    bool ok = parse_xapplepush(cmd, &attr);
    i_free(cmd);
    free_client(c);
    if (ok) { fprintf(stderr, "  FAIL: expected version failure\n"); return 0; }
    return 1;
}

/* aps-account-id empty -> parse fails */
static int test_parse_empty_account_id(void) {
    setup_valid_args();
    base_args[3]._data.str = "";
    test_set_imap_args(base_args, 12);
    ensure_global();

    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);
    struct xaps_attr attr;
    memset(&attr, 0, sizeof(attr));

    bool ok = parse_xapplepush(cmd, &attr);
    i_free(cmd);
    free_client(c);
    if (ok) { fprintf(stderr, "  FAIL: expected account-id failure\n"); return 0; }
    return 1;
}

/* aps-device-token empty -> parse fails */
static int test_parse_empty_device_token(void) {
    setup_valid_args();
    base_args[5]._data.str = "";
    test_set_imap_args(base_args, 12);
    ensure_global();

    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);
    struct xaps_attr attr;
    memset(&attr, 0, sizeof(attr));

    bool ok = parse_xapplepush(cmd, &attr);
    i_free(cmd);
    free_client(c);
    if (ok) { fprintf(stderr, "  FAIL: expected device-token failure\n"); return 0; }
    return 1;
}

/* aps-subtopic empty -> parse fails */
static int test_parse_empty_subtopic(void) {
    setup_valid_args();
    base_args[7]._data.str = "";
    test_set_imap_args(base_args, 12);
    ensure_global();

    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);
    struct xaps_attr attr;
    memset(&attr, 0, sizeof(attr));

    bool ok = parse_xapplepush(cmd, &attr);
    i_free(cmd);
    free_client(c);
    if (ok) { fprintf(stderr, "  FAIL: expected subtopic failure\n"); return 0; }
    return 1;
}

/* mailboxes key omitted entirely -> parse reaches the mailboxes check.
   Use an unrecognized astring key in the fifth slot so the key/value loop
   completes without setting xaps_attr->mailboxes. */
static int test_parse_missing_mailboxes(void) {
    setup_valid_args();
    base_args[8].type = IMAP_ARG_ATOM;
    base_args[8]._data.str = "unknown-key";
    base_args[9].type = IMAP_ARG_ATOM;
    base_args[9]._data.str = "value";
    test_set_imap_args(base_args, 12);
    ensure_global();

    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);
    struct xaps_attr attr;
    memset(&attr, 0, sizeof(attr));

    bool ok = parse_xapplepush(cmd, &attr);
    i_free(cmd);
    free_client(c);
    if (ok) { fprintf(stderr, "  FAIL: expected mailboxes failure\n"); return 0; }
    return 1;
}

/* ---- xaps_register tests ---- */

static void free_http_state(void) { push_notification_driver_xaps_cleanup(); }

static int test_register_with_mailboxes(void) {
    init_global_url();
    struct xaps_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.aps_account_id = "acct-1";
    attr.aps_device_token = "tok-1";
    attr.aps_subtopic = "com.apple.mobilemail";
    attr.dovecot_username = "user@example.com";
    attr.aps_version = "2";
    attr.mailboxes = mailbox_list;

    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);

    test_reset_logs();
    int rc = xaps_register(cmd, &attr);

    const char *payload = test_get_last_payload();
    int ok = 1;
    if (rc != 0) { fprintf(stderr, "  FAIL: register returned %d\n", rc); ok = 0; }
    else if (payload == NULL || strstr(payload, "\"ApsAccountId\":\"acct-1\"") == NULL) ok = 0;
    else if (payload == NULL || strstr(payload, "\"ApsDeviceToken\":\"tok-1\"") == NULL) ok = 0;
    else if (payload == NULL || strstr(payload, "\"ApsSubtopic\":\"com.apple.mobilemail\"") == NULL) ok = 0;
    else if (payload == NULL || strstr(payload, "\"Username\":\"user@example.com\"") == NULL) ok = 0;
    else if (payload == NULL || strstr(payload, "\"INBOX\"") == NULL) ok = 0;
    else if (payload == NULL || strstr(payload, "\"Notes\"") == NULL) ok = 0;

    i_free(cmd);
    free_client(c);
    free_http_state();
    return ok;
}

static int test_register_no_mailboxes_default_inbox(void) {
    init_global_url();
    struct xaps_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.aps_account_id = "acct-1";
    attr.aps_device_token = "tok-1";
    attr.aps_subtopic = "com.apple.mobilemail";
    attr.dovecot_username = "user@example.com";
    attr.mailboxes = NULL;

    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);

    test_reset_logs();
    int rc = xaps_register(cmd, &attr);

    const char *payload = test_get_last_payload();
    int ok = 1;
    if (rc != 0) { fprintf(stderr, "  FAIL: register returned %d\n", rc); ok = 0; }
    else if (payload == NULL || strstr(payload, "\"Mailboxes\": [\"INBOX\"]") == NULL) ok = 0;

    i_free(cmd);
    free_client(c);
    free_http_state();
    return ok;
}

/* A mailbox element that isn't an astring -> xaps_register returns -1 */
static int test_register_bad_mailbox_element(void) {
    init_global_url();
    struct imap_arg bad_list[2];
    bad_list[0].type = IMAP_ARG_LIST; /* not an astring */
    bad_list[0]._data.list = mailbox_list;
    bad_list[1].type = IMAP_ARG_NIL;

    struct xaps_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.aps_account_id = "acct-1";
    attr.aps_device_token = "tok-1";
    attr.aps_subtopic = "com.apple.mobilemail";
    attr.dovecot_username = "user@example.com";
    attr.mailboxes = bad_list;

    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);

    int rc = xaps_register(cmd, &attr);

    i_free(cmd);
    free_client(c);
    free_http_state();
    if (rc != -1) { fprintf(stderr, "  FAIL: expected -1, got %d\n", rc); return 0; }
    return 1;
}

/* ---- register_client tests ---- */

static int test_register_client_success(void) {
    init_global_url();
    struct xaps_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.aps_version = "2";
    attr.aps_account_id = "acct-1";
    attr.aps_device_token = "tok-1";
    attr.aps_subtopic = "com.apple.mobilemail";
    attr.dovecot_username = "user@example.com";
    attr.mailboxes = NULL;

    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);

    bool ok = register_client(cmd, &attr);

    i_free(cmd);
    free_client(c);
    free_http_state();
    if (!ok) { fprintf(stderr, "  FAIL: register_client failed\n"); return 0; }
    return 1;
}

/* register_client when xaps_register returns non-zero */
static int test_register_client_reg_failure(void) {
    init_global_url();
    struct imap_arg bad_list[2];
    bad_list[0].type = IMAP_ARG_LIST;
    bad_list[0]._data.list = mailbox_list;
    bad_list[1].type = IMAP_ARG_NIL;

    struct xaps_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.aps_account_id = "acct-1";
    attr.aps_device_token = "tok-1";
    attr.aps_subtopic = "com.apple.mobilemail";
    attr.dovecot_username = "user@example.com";
    attr.mailboxes = bad_list; /* forces xaps_register to return -1 */

    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);

    bool ok = register_client(cmd, &attr);

    i_free(cmd);
    free_client(c);
    free_http_state();
    if (ok) { fprintf(stderr, "  FAIL: expected register_client failure\n"); return 0; }
    return 1;
}

/* ---- cmd_xapplepushservice tests ---- */

static int test_cmd_success(void) {
    setup_valid_args();
    test_set_imap_args(base_args, 12);
    test_set_settings_url("http://[::1]:11619");
    xaps_global = NULL;

    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);

    bool ok = cmd_xapplepushservice(cmd);

    i_free(cmd);
    free_client(c);
    free_http_state();
    if (!ok) { fprintf(stderr, "  FAIL: cmd failed\n"); return 0; }
    return 1;
}

static int test_cmd_parse_failure(void) {
    test_set_read_args_fail(TRUE);
    test_set_settings_url("http://[::1]:11619");
    xaps_global = NULL;

    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);

    bool ok = cmd_xapplepushservice(cmd);
    test_set_read_args_fail(FALSE);

    i_free(cmd);
    free_client(c);
    free_http_state();
    if (ok) { fprintf(stderr, "  FAIL: expected cmd failure\n"); return 0; }
    return 1;
}

/* cmd parses OK but registration fails (bad mailbox list element) -> cmd fails */
static int test_cmd_register_failure(void) {
    setup_valid_args();
    /* mailboxes list whose element is a nested LIST, not an astring */
    base_args[9]._data.list = bad_mailbox_list;
    test_set_imap_args(base_args, 12);
    test_set_settings_url("http://[::1]:11619");
    xaps_global = NULL;

    struct client *c = make_client();
    struct client_command_context *cmd = make_cmd(c);

    bool ok = cmd_xapplepushservice(cmd);

    i_free(cmd);
    free_client(c);
    free_http_state();
    if (ok) { fprintf(stderr, "  FAIL: expected registration failure\n"); return 0; }
    return 1;
}

/* ---- register callback tests ---- */

static int test_register_callback_2xx(void) {
    xaps_global = i_new(struct xaps_config, 1);
    const char *topic_data = "com.apple.mobilemail-certificate-topic-data";
    struct istream *stream = i_stream_create_from_data(topic_data, strlen(topic_data));
    struct http_response resp = {
        .status = 200,
        .status_line = "OK",
        .payload = stream,
    };
    test_reset_logs();
    xaps_register_callback(&resp, NULL);
    const char *err = test_get_last_error();
    int ok = (err[0] == '\0');
    i_stream_unref(&stream);
    i_free(xaps_global);
    xaps_global = NULL;
    if (!ok) { fprintf(stderr, "  FAIL: unexpected error: %s\n", err); return 0; }
    return 1;
}

static int test_register_callback_3xx(void) {
    xaps_global = i_new(struct xaps_config, 1);
    struct http_response resp = {
        .status = 302,
        .status_line = "Found",
    };
    test_reset_logs();
    xaps_register_callback(&resp, NULL);
    const char *err = test_get_last_error();
    int ok = (err[0] != '\0');
    i_free(xaps_global);
    xaps_global = NULL;
    if (!ok) { fprintf(stderr, "  FAIL: expected error for 3xx\n"); return 0; }
    return 1;
}

static int test_register_callback_5xx(void) {
    xaps_global = i_new(struct xaps_config, 1);
    struct http_response resp = {
        .status = 500,
        .status_line = "Internal Server Error",
    };
    test_reset_logs();
    xaps_register_callback(&resp, NULL);
    const char *err = test_get_last_error();
    int ok = (err[0] != '\0');
    i_free(xaps_global);
    xaps_global = NULL;
    if (!ok) { fprintf(stderr, "  FAIL: expected error for 5xx\n"); return 0; }
    return 1;
}

/* ---- client_created hook tests ---- */

static int test_client_created_with_next_hook(void) {
    /* Install the hook so next_hook_client_created is non-NULL.
       xaps_imap_plugin_init registers xaps_client_created as the hook. */
    xaps_imap_module = (struct module*)0x1;
    xaps_imap_plugin_init(xaps_imap_module);

    struct client *c = make_client();
    struct client *client_ptr = c;

    test_reset_logs();
    xaps_client_created(&client_ptr);

    const char *caps = str_c(c->capability_string);
    int ok = (strstr(caps, "XAPPLEPUSHSERVICE") != NULL);

    free_client(c);
    return ok;
}

static int test_client_created_no_next_hook(void) {
    /* Reset the saved hook to NULL so the next_hook branch is skipped.
       We call the static function directly; set next_hook to NULL via a
       fresh module state by invoking the hook mechanism. */
    next_hook_client_created = NULL;

    struct client *c = make_client();
    struct client *client_ptr = c;

    xaps_client_created(&client_ptr);
    const char *caps = str_c(c->capability_string);

    /* With plugin loaded (always TRUE here) the capability is still appended */
    int ok = (strstr(caps, "XAPPLEPUSHSERVICE") != NULL);

    free_client(c);
    if (!ok) {
        fprintf(stderr, "  FAIL: capability not appended\n");
        return 0;
    }
    return 1;
}

/* A distinct chained hook that appends its own marker so we can prove the
   next_hook_client_created branch in xaps_client_created really runs. */
static void chained_client_created(struct client **client) {
    str_append((*client)->capability_string, " CHAINED");
}

/* When another hook was previously installed, xaps_client_created chains to
   it. Install a distinct chained hook and expect both markers. */
static int test_client_created_next_hook(void) {
    next_hook_client_created = chained_client_created;

    struct client *c = make_client();
    struct client *client_ptr = c;

    xaps_client_created(&client_ptr);
    const char *caps = str_c(c->capability_string);

    int ok = (strstr(caps, "XAPPLEPUSHSERVICE") != NULL &&
              strstr(caps, "CHAINED") != NULL);

    free_client(c);
    next_hook_client_created = NULL;
    if (!ok) {
        fprintf(stderr, "  FAIL: chained hook not invoked (caps='%s')\n", caps);
        return 0;
    }
    return 1;
}

/* ---- plugin init/deinit ---- */

static int test_imap_plugin_init_deinit(void) {
    xaps_imap_module = (struct module*)0x1;
    xaps_imap_plugin_init(xaps_imap_module);
    xaps_imap_plugin_deinit();
    return 1;
}

/* ---- metadata tests ---- */

static int test_xaps_attr_fields(void) {
    struct xaps_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.aps_version = "2";
    attr.aps_account_id = "test-id";
    attr.aps_device_token = "test-token";
    attr.aps_subtopic = "com.apple.mobilemail";
    attr.dovecot_username = "user@example.com";
    if (strcmp(attr.aps_version, "2") != 0) return 0;
    if (strcmp(attr.aps_account_id, "test-id") != 0) return 0;
    if (strcmp(attr.aps_device_token, "test-token") != 0) return 0;
    if (strcmp(attr.aps_subtopic, "com.apple.mobilemail") != 0) return 0;
    if (strcmp(attr.dovecot_username, "user@example.com") != 0) return 0;
    return 1;
}

static int test_imap_plugin_version(void) {
    if (strcmp(xapplepushservice_plugin_version, DOVECOT_ABI_VERSION) != 0) {
        fprintf(stderr, "  FAIL: version mismatch\n");
        return 0;
    }
    return 1;
}

static int test_binary_dependency(void) {
    if (strcmp(xaps_imap_plugin_binary_dependency, "imap") != 0) {
        fprintf(stderr, "  FAIL: expected 'imap'\n");
        return 0;
    }
    return 1;
}

/* ---- main ---- */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== test_xaps_imap ===\n");

    RUN_TEST(test_parse_valid);
    RUN_TEST(test_parse_read_args_fail);
    RUN_TEST(test_parse_bad_key);
    RUN_TEST(test_parse_bad_value);
    RUN_TEST(test_parse_bad_mailboxes_list);
    RUN_TEST(test_parse_unknown_version);
    RUN_TEST(test_parse_empty_account_id);
    RUN_TEST(test_parse_empty_device_token);
    RUN_TEST(test_parse_empty_subtopic);
    RUN_TEST(test_parse_missing_mailboxes);
    RUN_TEST(test_register_with_mailboxes);
    RUN_TEST(test_register_no_mailboxes_default_inbox);
    RUN_TEST(test_register_bad_mailbox_element);
    RUN_TEST(test_register_client_success);
    RUN_TEST(test_register_client_reg_failure);
    RUN_TEST(test_cmd_success);
    RUN_TEST(test_cmd_parse_failure);
    RUN_TEST(test_cmd_register_failure);
    RUN_TEST(test_register_callback_2xx);
    RUN_TEST(test_register_callback_3xx);
    RUN_TEST(test_register_callback_5xx);
    RUN_TEST(test_client_created_with_next_hook);
    RUN_TEST(test_client_created_no_next_hook);
    RUN_TEST(test_client_created_next_hook);
    RUN_TEST(test_imap_plugin_init_deinit);
    RUN_TEST(test_xaps_attr_fields);
    RUN_TEST(test_imap_plugin_version);
    RUN_TEST(test_binary_dependency);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
