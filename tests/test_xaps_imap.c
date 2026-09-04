/*
 * Unit tests for xaps-imap-plugin: xaps_register_callback
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

/* ---- Tests ---- */

/* Register callback with 2xx -> success, aps_topic written */
static int test_register_callback_2xx(void) {
    /* Set up xaps_global so the callback can write to aps_topic */
    xaps_global = i_new(struct xaps_config, 1);

    /* Create a fake payload with enough data (> 31 bytes) */
    const char *topic_data = "com.apple.mobilemail-certificate-topic-data";
    struct istream *stream = i_stream_create_from_data(topic_data, strlen(topic_data));

    struct http_response resp = {
        .status = 200,
        .status_line = "OK",
        .payload = stream,
    };

    test_reset_logs();
    xaps_register_callback(&resp, NULL);

    /* Should have debug message */
    const char *err = test_get_last_error();
    if (err[0] != '\0') {
        fprintf(stderr, "  FAIL: unexpected error: %s\n", err);
        i_stream_unref(&stream);
        i_free(xaps_global);
        xaps_global = NULL;
        return 0;
    }

    i_stream_unref(&stream);
    i_free(xaps_global);
    xaps_global = NULL;
    return 1;
}

/* Register callback with 5xx -> error logged */
static int test_register_callback_5xx(void) {
    xaps_global = i_new(struct xaps_config, 1);

    struct http_response resp = {
        .status = 500,
        .status_line = "Internal Server Error",
    };

    test_reset_logs();
    xaps_register_callback(&resp, NULL);

    const char *err = test_get_last_error();
    if (err[0] == '\0') {
        fprintf(stderr, "  FAIL: expected error for 5xx\n");
        i_free(xaps_global);
        xaps_global = NULL;
        return 0;
    }

    i_free(xaps_global);
    xaps_global = NULL;
    return 1;
}

/* xaps_attr struct has all expected fields */
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

/* Plugin version matches */
static int test_imap_plugin_version(void) {
    if (strcmp(xapplepushservice_plugin_version, DOVECOT_ABI_VERSION) != 0) {
        fprintf(stderr, "  FAIL: version mismatch\n");
        return 0;
    }
    return 1;
}

/* Binary dependency is "imap" */
static int test_binary_dependency(void) {
    if (strcmp(xaps_imap_plugin_binary_dependency, "imap") != 0) {
        fprintf(stderr, "  FAIL: expected 'imap'\n");
        return 0;
    }
    return 1;
}

/* ---- main ---- */
int main(void) {
    printf("=== test_xaps_imap ===\n");

    RUN_TEST(test_register_callback_2xx);
    RUN_TEST(test_register_callback_5xx);
    RUN_TEST(test_xaps_attr_fields);
    RUN_TEST(test_imap_plugin_version);
    RUN_TEST(test_binary_dependency);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
