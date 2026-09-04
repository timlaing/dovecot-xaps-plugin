/*
 * Unit tests for xaps-push-notification-plugin: HTTP callback handling,
 * begin_txn event configuration, event notification payload building,
 * and plugin lifecycle entry points.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dovecot_stubs/dovecot_stubs.h"

/* Include source under test */
#include "../src/xaps-push-notification-plugin.h"
#include "../src/xaps-utils.h"
#include "../src/xaps-push-notification-plugin.c"
#include "../src/xaps-utils.c"
#include "../src/xaps-settings.c"

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

/* Build a minimal mock mail_user. */
static struct mail_user *make_user(const char *username, const char **userdb_fields) {
    struct mail_user *u = i_new(struct mail_user, 1);
    u->username = i_strdup(username);
    u->userdb_fields = userdb_fields;
    u->event = NULL;
    return u;
}

/* Build a minimal push_notification_event with the given name. The struct
   is allocated and returned; caller keeps it alive for the test. */
static struct push_notification_event *make_event(const char *name) {
    struct push_notification_event *e = i_new(struct push_notification_event, 1);
    e->name = name;
    return e;
}

/* Build a driver txn with a mail user and optional message fields. */
static struct push_notification_driver_txn make_txn(const char *username) {
    struct push_notification_driver_txn dtxn;
    memset(&dtxn, 0, sizeof(dtxn));
    dtxn.duser = i_new(struct push_notification_driver_user, 1);
    dtxn.duser->context = (void*)0x11;
    /* ptxn is an anonymous struct pointer in the stub; allocate a matching
       chunk and fill the fields we use. */
    dtxn.ptxn = (void*)i_new_impl(sizeof(dtxn.ptxn->muser) +
                                  sizeof(dtxn.ptxn->mbox) +
                                  sizeof(dtxn.ptxn->pool) +
                                  sizeof(dtxn.ptxn->event));
    dtxn.ptxn->muser = make_user(username, NULL);
    dtxn.ptxn->mbox = i_new(struct mailbox, 1);
    dtxn.ptxn->mbox->name = "INBOX";
    dtxn.ptxn->pool = NULL;
    dtxn.ptxn->event = NULL;
    return dtxn;
}

static void free_txn(struct push_notification_driver_txn *dtxn) {
    i_free((char*)dtxn->ptxn->muser->username);
    i_free(dtxn->ptxn->muser);
    i_free((void*)dtxn->ptxn->mbox);
    i_free(dtxn->ptxn);
    i_free(dtxn->duser);
}

/* ---- HTTP callback tests ---- */

static int test_callback_2xx_no_error(void) {
    struct http_response resp = {
        .status = 200,
        .status_line = "OK",
    };
    test_reset_logs();
    push_notification_driver_xaps_http_callback(&resp, NULL);
    const char *err = test_get_last_error();
    if (err[0] != '\0') {
        fprintf(stderr, "  FAIL: unexpected error: %s\n", err);
        return 0;
    }
    return 1;
}

static int test_callback_404_no_error(void) {
    struct http_response resp = {
        .status = 404,
        .status_line = "Not Found",
    };
    test_reset_logs();
    push_notification_driver_xaps_http_callback(&resp, NULL);
    const char *err = test_get_last_error();
    if (err[0] != '\0') {
        fprintf(stderr, "  FAIL: unexpected error: %s\n", err);
        return 0;
    }
    return 1;
}

static int test_callback_5xx_error(void) {
    struct http_response resp = {
        .status = 500,
        .status_line = "Internal Server Error",
    };
    test_reset_logs();
    push_notification_driver_xaps_http_callback(&resp, NULL);
    const char *err = test_get_last_error();
    if (err[0] == '\0') {
        fprintf(stderr, "  FAIL: expected error for 5xx\n");
        return 0;
    }
    return 1;
}

static int test_callback_400_error(void) {
    struct http_response resp = {
        .status = 400,
        .status_line = "Bad Request",
    };
    test_reset_logs();
    push_notification_driver_xaps_http_callback(&resp, NULL);
    const char *err = test_get_last_error();
    if (err[0] == '\0') {
        fprintf(stderr, "  FAIL: expected error for 4xx\n");
        return 0;
    }
    return 1;
}

static int test_callback_2xx_debug_msg(void) {
    struct http_response resp = {
        .status = 201,
        .status_line = "Created",
    };
    test_reset_logs();
    push_notification_driver_xaps_http_callback(&resp, NULL);
    const char *dbg = test_get_last_debug();
    if (strstr(dbg, "Created") == NULL) {
        fprintf(stderr, "  FAIL: expected success debug, got '%s'\n", dbg);
        return 0;
    }
    return 1;
}

static int test_callback_404_debug_msg(void) {
    struct http_response resp = {
        .status = 404,
        .status_line = "no device",
    };
    test_reset_logs();
    push_notification_driver_xaps_http_callback(&resp, NULL);
    const char *dbg = test_get_last_debug();
    if (dbg[0] == '\0') {
        fprintf(stderr, "  FAIL: expected 404 debug msg\n");
        return 0;
    }
    return 1;
}

/* ---- begin_txn tests ---- */

/* begin_txn with no registered events -> no crash; init is called with debug */
static int test_begin_txn_no_events(void) {
    test_set_push_events(NULL, 0);
    struct push_notification_driver_txn dtxn = make_txn("alice");
    bool ok = xaps_plugin_begin_txn(&dtxn);
    free_txn(&dtxn);
    if (!ok) {
        fprintf(stderr, "  FAIL: begin_txn returned false\n");
        return 0;
    }
    return 1;
}

/* begin_txn with MessageNew / MessageAppend / other -> exercises all branches */
static int test_begin_txn_with_events(void) {
    struct push_notification_event *mn = make_event("MessageNew");
    struct push_notification_event *ma = make_event("MessageAppend");
    struct push_notification_event *other = make_event("FlagChange");
    const struct push_notification_event *events[] = { mn, ma, other };
    test_set_push_events(events, 3);

    struct push_notification_driver_txn dtxn = make_txn("bob");
    bool ok = xaps_plugin_begin_txn(&dtxn);

    test_set_push_events(NULL, 0);
    free_txn(&dtxn);
    i_free(mn); i_free(ma); i_free(other);
    if (!ok) {
        fprintf(stderr, "  FAIL: begin_txn returned false\n");
        return 0;
    }
    return 1;
}

/* ---- notify tests ---- */

/* notify: no eventdata -> payload without "Events" key */
static int test_notify_no_eventdata(void) {
    test_set_settings_url("http://[::1]:11619");

    struct push_notification_driver_txn dtxn = make_txn("carol@example.com");
    struct push_notification_txn_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.mailbox = "INBOX";
    msg.eventdata.arr = NULL;   /* array_not_created */
    msg.eventdata.count = 0;

    test_reset_logs();
    xaps_notify(&dtxn, &msg);

    const char *payload = test_get_last_payload();
    if (payload == NULL || *payload == '\0') {
        fprintf(stderr, "  FAIL: no payload captured\n");
        free_txn(&dtxn);
        push_notification_driver_xaps_cleanup();
        return 0;
    }
    if (strstr(payload, "\"Username\":\"carol@example.com\"") == NULL) {
        fprintf(stderr, "  FAIL: username missing in payload: %s\n", payload);
        free_txn(&dtxn);
        push_notification_driver_xaps_cleanup();
        return 0;
    }
    if (strstr(payload, "\"Mailbox\":\"INBOX\"") == NULL) {
        fprintf(stderr, "  FAIL: mailbox missing in payload: %s\n", payload);
        free_txn(&dtxn);
        push_notification_driver_xaps_cleanup();
        return 0;
    }
    if (strstr(payload, "\"Events\"") != NULL) {
        fprintf(stderr, "  FAIL: Events should not be present: %s\n", payload);
        free_txn(&dtxn);
        push_notification_driver_xaps_cleanup();
        return 0;
    }

    free_txn(&dtxn);
    push_notification_driver_xaps_cleanup();
    return 1;
}

/* notify: with eventdata -> payload includes escaped event names */
static int test_notify_with_eventdata(void) {
    test_set_settings_url("http://[::1]:11619");

    struct push_notification_driver_txn dtxn = make_txn("dave@example.com");

    struct push_notification_event ge = { .name = "MessageNew" };
    struct push_notification_event_config ec = { .event = &ge, .config = NULL };
    struct push_notification_txn_event te = { .event = &ec };

    struct push_notification_txn_event *events[2] = { &te, &te };
    struct push_notification_txn_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.mailbox = "INBOX";
    /* eventdata.arr is the void** array of txn_event pointers */
    msg.eventdata.arr = (void**)events;
    msg.eventdata.count = 2;
    msg.eventdata.alloc = 2;

    test_reset_logs();
    xaps_notify(&dtxn, &msg);

    const char *payload = test_get_last_payload();
    if (payload == NULL || *payload == '\0') {
        fprintf(stderr, "  FAIL: no payload captured\n");
        free_txn(&dtxn);
        push_notification_driver_xaps_cleanup();
        return 0;
    }
    if (strstr(payload, "\"Events\": [\"MessageNew\",\"MessageNew\"]") == NULL) {
        fprintf(stderr, "  FAIL: Events list wrong: %s\n", payload);
        free_txn(&dtxn);
        push_notification_driver_xaps_cleanup();
        return 0;
    }

    free_txn(&dtxn);
    push_notification_driver_xaps_cleanup();
    return 1;
}

/* notify with an unconfigured xaps (settings fail) -> skips without crashing */
static int test_notify_unconfigured(void) {
    xaps_global = NULL;
    test_set_settings_get_fail(TRUE);

    struct push_notification_driver_txn dtxn = make_txn("noone@example.com");
    struct push_notification_txn_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.mailbox = "INBOX";

    test_reset_logs();
    xaps_notify(&dtxn, &msg);

    test_set_settings_get_fail(FALSE);
    const char *err = test_get_last_error();
    int ok = (err[0] != '\0' && test_get_last_payload()[0] == '\0');

    free_txn(&dtxn);
    push_notification_driver_xaps_cleanup();
    if (!ok) { fprintf(stderr, "  FAIL: expected notified skip (no payload)\n"); return 0; }
    return 1;
}

/* ---- lifecycle tests ---- */

static int test_plugin_init(void) {
    int rc = xaps_push_plugin_init(NULL, NULL, "xaps", NULL, NULL);
    if (rc != 0) {
        fprintf(stderr, "  FAIL: plugin init returned %d\n", rc);
        return 0;
    }
    return 1;
}

static int test_plugin_deinit(void) {
    struct push_notification_driver_user duser;
    memset(&duser, 0, sizeof(duser));
    xaps_plugin_deinit(&duser);
    return 1;
}

static int test_plugin_cleanup(void) {
    xaps_global = NULL;
    xaps_plugin_cleanup();
    return 1;
}

static int test_push_notification_plugin_init_deinit(void) {
    xaps_push_notification_plugin_init(NULL);
    xaps_push_notification_plugin_deinit();
    return 1;
}

/* Driver struct has correct name */
static int test_driver_name(void) {
    if (strcmp(push_notification_driver_xaps.name, "xaps") != 0) {
        fprintf(stderr, "  FAIL: expected name 'xaps', got '%s'\n",
                push_notification_driver_xaps.name);
        return 0;
    }
    return 1;
}

/* Driver struct has all callbacks set */
static int test_driver_callbacks_set(void) {
    if (push_notification_driver_xaps.v.init == NULL) {
        fprintf(stderr, "  FAIL: init callback is NULL\n");
        return 0;
    }
    if (push_notification_driver_xaps.v.begin_txn == NULL) {
        fprintf(stderr, "  FAIL: begin_txn callback is NULL\n");
        return 0;
    }
    if (push_notification_driver_xaps.v.process_msg == NULL) {
        fprintf(stderr, "  FAIL: process_msg callback is NULL\n");
        return 0;
    }
    if (push_notification_driver_xaps.v.deinit == NULL) {
        fprintf(stderr, "  FAIL: deinit callback is NULL\n");
        return 0;
    }
    if (push_notification_driver_xaps.v.cleanup == NULL) {
        fprintf(stderr, "  FAIL: cleanup callback is NULL\n");
        return 0;
    }
    return 1;
}

/* Plugin version matches DOVECOT_ABI_VERSION */
static int test_plugin_version(void) {
    if (strcmp(xaps_plugin_version, DOVECOT_ABI_VERSION) != 0) {
        fprintf(stderr, "  FAIL: version mismatch\n");
        return 0;
    }
    return 1;
}

/* Dependencies list declares push_notification */
static int test_plugin_dependencies(void) {
    if (xaps_plugin_dependencies[0] == NULL ||
        strcmp(xaps_plugin_dependencies[0], "push_notification") != 0) {
        fprintf(stderr, "  FAIL: dependencies wrong\n");
        return 0;
    }
    if (xaps_plugin_dependencies[1] != NULL) {
        fprintf(stderr, "  FAIL: dependencies should terminate with NULL\n");
        return 0;
    }
    return 1;
}

/* ---- main ---- */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== test_xaps_push_notification ===\n");

    RUN_TEST(test_callback_2xx_no_error);
    RUN_TEST(test_callback_404_no_error);
    RUN_TEST(test_callback_5xx_error);
    RUN_TEST(test_callback_400_error);
    RUN_TEST(test_callback_2xx_debug_msg);
    RUN_TEST(test_callback_404_debug_msg);
    RUN_TEST(test_begin_txn_no_events);
    RUN_TEST(test_begin_txn_with_events);
    RUN_TEST(test_notify_no_eventdata);
    RUN_TEST(test_notify_with_eventdata);
    RUN_TEST(test_notify_unconfigured);
    RUN_TEST(test_plugin_init);
    RUN_TEST(test_plugin_deinit);
    RUN_TEST(test_plugin_cleanup);
    RUN_TEST(test_push_notification_plugin_init_deinit);
    RUN_TEST(test_driver_name);
    RUN_TEST(test_driver_callbacks_set);
    RUN_TEST(test_plugin_version);
    RUN_TEST(test_plugin_dependencies);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
