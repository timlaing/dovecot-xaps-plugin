/*
 * Unit tests for xaps-push-notification-plugin: HTTP callback
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

/* ---- Tests ---- */

/* HTTP callback with 2xx status should not log an error */
static int test_callback_2xx_no_error(void) {
    struct http_response resp = {
        .status = 200,
        .status_line = "OK",
    };
    test_reset_logs();
    push_notification_driver_xaps_http_callback(&resp, NULL);
    /* Should have debug message, no error */
    const char *err = test_get_last_error();
    if (err[0] != '\0') {
        fprintf(stderr, "  FAIL: unexpected error: %s\n", err);
        return 0;
    }
    return 1;
}

/* HTTP callback with 404 should not be treated as an error */
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

/* HTTP callback with 5xx should log an error */
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

/* HTTP callback with 400 should log an error */
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

/* ---- main ---- */
int main(void) {
    printf("=== test_xaps_push_notification ===\n");

    RUN_TEST(test_callback_2xx_no_error);
    RUN_TEST(test_callback_404_no_error);
    RUN_TEST(test_callback_5xx_error);
    RUN_TEST(test_callback_400_error);
    RUN_TEST(test_driver_name);
    RUN_TEST(test_driver_callbacks_set);
    RUN_TEST(test_plugin_version);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
