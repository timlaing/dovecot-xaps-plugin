/*
 * Mock implementations of Dovecot functions for unit testing.
 * These are linked instead of the real Dovecot library.
 */
#include "dovecot_stubs/dovecot_stubs.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

/* ---- pool / memory ---- */
void *i_new_impl(size_t size) {
    void *p = calloc(1, size);
    assert(p != NULL);
    return p;
}

void *i_malloc(size_t size) {
    void *p = malloc(size);
    assert(p != NULL);
    return p;
}

void i_free(void *ptr) {
    free(ptr);
}

char *i_strdup(const char *str) {
    if (str == NULL) return NULL;
    return strdup(str);
}

char *i_strdup_until(const char *start, const char *end) {
    size_t len = (size_t)(end - start);
    char *s = malloc(len + 1);
    assert(s != NULL);
    memcpy(s, start, len);
    s[len] = '\0';
    return s;
}

pool_t pool_alloconly_create(const char *name ATTR_UNUSED, size_t min_size) {
    return calloc(1, min_size > 64 ? min_size : 64);
}

void pool_unref(pool_t *pool) {
    if (*pool != NULL) { free(*pool); *pool = NULL; }
}

char *p_strdup(pool_t pool ATTR_UNUSED, const char *str) {
    if (str == NULL) return NULL;
    return strdup(str);
}

char *t_strdup_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    char *buf = malloc(n + 1);
    assert(buf != NULL);
    va_start(args, fmt);
    vsnprintf(buf, n + 1, fmt, args);
    va_end(args);
    return buf;
}

/* ---- string_t ---- */
string_t *str_new(pool_t pool ATTR_UNUSED, size_t initial_size) {
    string_t *str = calloc(1, sizeof(*str));
    str->alloc = initial_size > 0 ? initial_size : 64;
    str->str = malloc(str->alloc);
    str->str[0] = '\0';
    return str;
}

static void str_grow(string_t *str, size_t need) {
    if (str->used + need + 1 <= str->alloc) return;
    while (str->used + need + 1 > str->alloc) str->alloc *= 2;
    str->str = realloc(str->str, str->alloc);
}

void str_append(string_t *str, const char *cstr) {
    size_t len = strlen(cstr);
    str_grow(str, len);
    memcpy(str->str + str->used, cstr, len);
    str->used += len;
    str->str[str->used] = '\0';
}

void str_append_len(string_t *str, const void *data, size_t len) {
    str_grow(str, len);
    memcpy(str->str + str->used, data, len);
    str->used += len;
    str->str[str->used] = '\0';
}

void str_printfa(string_t *str, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    str_grow(str, (size_t)n);
    va_start(args, fmt);
    vsnprintf(str->str + str->used, str->alloc - str->used, fmt, args);
    va_end(args);
    str->used += (size_t)n;
}

const char *str_c(const string_t *str) {
    return str->str;
}

size_t str_len(const string_t *str) {
    return str->used;
}

const char *str_data(const string_t *str) {
    return str->str;
}

void str_free(string_t **str) {
    if (str != NULL && *str != NULL) {
        free((*str)->str);
        free(*str);
        *str = NULL;
    }
}

/* ---- istream ---- */
struct istream *i_stream_create_from_data(const void *data, size_t size) {
    struct istream *stream = calloc(1, sizeof(*stream));
    unsigned char *buf = malloc(size);
    memcpy(buf, data, size);
    stream->buffer = buf;
    stream->buffer_size = size;
    stream->skip = 0;
    stream->refcount = 1;
    return stream;
}

void i_stream_ref(struct istream *stream) {
    stream->refcount++;
}

void i_stream_unref(struct istream **stream) {
    if (*stream == NULL) return;
    (*stream)->refcount--;
    if ((*stream)->refcount <= 0) {
        free((void*)(*stream)->buffer);
        free(*stream);
        *stream = NULL;
    }
}

int i_stream_read_data(struct istream *stream, const unsigned char **data_r,
                       size_t *size_r, size_t threshold ATTR_UNUSED) {
    if (stream->skip >= stream->buffer_size) return 0;
    *data_r = stream->buffer + stream->skip;
    *size_r = stream->buffer_size - stream->skip;
    return (int)*size_r;
}

ssize_t i_stream_read(struct istream *stream) {
    if (stream->skip >= stream->buffer_size) return -1;  /* EOF */
    return (ssize_t)(stream->buffer_size - stream->skip);
}

void i_stream_skip(struct istream *stream, size_t count) {
    if (stream->skip + count <= stream->buffer_size)
        stream->skip += count;
}

const char *i_stream_get_error(const struct istream *stream) {
    return stream->stream_errno != 0 ? "mock stream error" : "";
}

/* ---- http_url ---- */
static bool test_http_url_parse_fail;

void test_set_http_url_parse_fail(bool fail) { test_http_url_parse_fail = fail; }

int http_url_parse(const char *url, const char *default_host ATTR_UNUSED,
                   unsigned int flags ATTR_UNUSED, pool_t pool,
                   struct http_url **url_r, const char **error_r) {
    if (test_http_url_parse_fail) {
        *url_r = NULL;
        *error_r = "mock url parse failure";
        return -1;
    }
    struct http_url *u = calloc(1, sizeof(*u));
    /* Simple parse: just store the URL string as path */
    u->path = strdup(url);
    u->host = strdup("localhost");
    u->port = 11619;
    u->scheme = strdup("http");
    (void)pool;
    *url_r = u;
    *error_r = NULL;
    return 0;
}

const char *http_url_to_string(const struct http_url *url, pool_t pool ATTR_UNUSED) {
    return url->path;
}

/* ---- http_client ---- */
static bool test_http_client_init_fail;

void test_set_http_client_init_fail(bool fail) {
    test_http_client_init_fail = fail;
}

int http_client_init_auto(struct event *event ATTR_UNUSED,
                          struct http_client **client_r,
                          const char **error_r) {
    if (test_http_client_init_fail) {
        *client_r = NULL;
        *error_r = "mock http client init failure";
        return -1;
    }
    *client_r = (void*)0xDEADBEEF; /* non-NULL sentinel */
    *error_r = NULL;
    return 0;
}

static http_client_request_callback_t saved_callback;
static void *saved_callback_ctx;
static char last_payload[4096] = "";
static unsigned int test_wait_status = 200;

void test_set_wait_status(unsigned int status) { test_wait_status = status; }

/* Expose the most recent HTTP request payload (as text) for assertions. */
static void test_capture_payload(const unsigned char *data, size_t len) {
    if (len >= sizeof(last_payload)) len = sizeof(last_payload) - 1;
    memcpy(last_payload, data, len);
    last_payload[len] = '\0';
}

const char *test_get_last_payload(void) { return last_payload; }

struct http_client_request *
http_client_request_url(struct http_client *client ATTR_UNUSED,
                        const char *method ATTR_UNUSED,
                        const struct http_url *url ATTR_UNUSED,
                        http_client_request_callback_t callback,
                        void *context) {
    saved_callback = callback;
    saved_callback_ctx = context;
    return (void*)0xCAFEBABE; /* non-NULL sentinel */
}

void http_client_request_add_header(struct http_client_request *req ATTR_UNUSED,
                                    const char *key ATTR_UNUSED,
                                    const char *value ATTR_UNUSED) {
}

void http_client_request_set_payload(struct http_client_request *req ATTR_UNUSED,
                                     struct istream *payload ATTR_UNUSED,
                                     bool get_ownership ATTR_UNUSED) {
    if (payload != NULL && payload->buffer != NULL) {
        test_capture_payload(payload->buffer, payload->buffer_size);
    }
}

void http_client_request_set_event(struct http_client_request *req ATTR_UNUSED,
                                   struct event *event ATTR_UNUSED) {
}

void http_client_request_submit(struct http_client_request *req ATTR_UNUSED) {
}

void http_client_wait(struct http_client *client ATTR_UNUSED) {
    /* Simulate a successful HTTP response */
    if (saved_callback != NULL) {
        static const char topic[] = "com.apple.mobilemail-topic-cert-abc123";
        struct istream *payload =
            i_stream_create_from_data(topic, sizeof(topic) - 1);
        struct http_response resp = {
            .status = test_wait_status,
            .status_line = "OK",
            .payload = payload,
        };
        saved_callback(&resp, saved_callback_ctx);
        i_stream_unref(&payload);
    }
}

void http_client_deinit(struct http_client **client) {
    *client = NULL;
}

const char *http_response_get_message(const struct http_response *response) {
    return response->status_line;
}

/* ---- json ---- */
void json_append_escaped(string_t *str, const char *src) {
    /* Minimal: just append as-is (not real JSON escaping, but sufficient for tests) */
    str_append(str, src);
}

/* ---- settings ---- */
void settings_info_register(const struct setting_parser_info *info ATTR_UNUSED) {
}

/* Test-controllable settings values (NULL = use defaults). */
static const char *test_settings_url;
static const char *test_settings_user_lookup;
static bool test_settings_get_fail;

void test_set_settings_url(const char *url) { test_settings_url = url; }
void test_set_settings_user_lookup(const char *lookup) {
    test_settings_user_lookup = lookup;
}
void test_set_settings_get_fail(bool fail) { test_settings_get_fail = fail; }

int settings_get_impl(struct event *event ATTR_UNUSED,
                      const struct setting_parser_info *info,
                      unsigned int flags ATTR_UNUSED,
                      const char *source_filename ATTR_UNUSED,
                      unsigned int source_linenum ATTR_UNUSED,
                      const void **set_r, const char **error_r) {
    if (test_settings_get_fail) {
        *set_r = NULL;
        *error_r = "mock settings failure";
        return -1;
    }
    /* Allocate a struct matching the parser_info's struct_size */
    void *set = calloc(1, info->struct_size);
    /* Copy defaults if present */
    if (info->defaults != NULL) {
        memcpy(set, info->defaults, info->struct_size);
    }
    /* Override the string fields the test requested. We locate each field
       by walking the parser's #define table (key -> struct offset). */
    if (test_settings_url != NULL || test_settings_user_lookup != NULL) {
        const struct setting_define *def;
        for (def = info->defines; def && def->key != NULL; def++) {
            const char **slot = (const char **)((char *)set + def->offset);
            if (test_settings_url != NULL &&
                strcmp(def->key, "xaps_url") == 0) {
                *slot = strdup(test_settings_url);
            } else if (test_settings_user_lookup != NULL &&
                       strcmp(def->key, "xaps_user_lookup") == 0) {
                *slot = strdup(test_settings_user_lookup);
            }
        }
    }
    *set_r = set;
    *error_r = NULL;
    return 0;
}

void settings_free(const void *set) {
    free((void*)set);
}

/* ---- logging ---- */
static char last_error[1024] = "";
static char last_debug[1024] = "";

const char *test_get_last_error(void) { return last_error; }
const char *test_get_last_debug(void) { return last_debug; }
void test_reset_logs(void) { last_error[0] = '\0'; last_debug[0] = '\0';
    last_payload[0] = '\0'; }

void i_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_error, sizeof(last_error), fmt, args);
    va_end(args);
}

void i_debug(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_debug, sizeof(last_debug), fmt, args);
    va_end(args);
}

void i_warning(const char *fmt, ...) {
    (void)fmt;
}

void i_assert_fail(const char *condition, const char *file, unsigned int line) {
    fprintf(stderr, "Assertion failed: %s at %s:%u\n", condition, file, line);
    abort();
}

/* ---- mail_user ---- */
bool mail_user_is_plugin_loaded(struct mail_user *user ATTR_UNUSED,
                                struct module *module ATTR_UNUSED) {
    return TRUE;
}

/* ---- imap arg helpers ---- */
bool imap_arg_get_astring(const struct imap_arg *arg, const char **str_r) {
    if (arg->type == IMAP_ARG_ATOM || arg->type == IMAP_ARG_STRING) {
        *str_r = arg->_data.str;
        return TRUE;
    }
    return FALSE;
}

bool imap_arg_get_list(const struct imap_arg *arg, const struct imap_arg **list_r) {
    if (arg->type == IMAP_ARG_LIST) {
        *list_r = arg->_data.list;
        return TRUE;
    }
    return FALSE;
}

/* ---- imap client ---- */
static const struct imap_arg *test_args;
static unsigned int test_arg_count;
static bool test_read_args_fail;
static struct imap_arg empty_arg = { .type = IMAP_ARG_NIL };

void test_set_imap_args(const struct imap_arg *args, unsigned int count) {
    test_args = args;
    test_arg_count = count;
}

void test_set_read_args_fail(bool fail) { test_read_args_fail = fail; }

bool client_read_args(struct client_command_context *cmd ATTR_UNUSED,
                      unsigned int count ATTR_UNUSED,
                      unsigned int max_args ATTR_UNUSED,
                      const struct imap_arg **args_r) {
    if (test_read_args_fail) return FALSE;
    if (test_args != NULL) {
        *args_r = test_args;
        return TRUE;
    }
    *args_r = &empty_arg;
    return TRUE;
}

void client_send_command_error(struct client_command_context *cmd ATTR_UNUSED,
                               const char *msg ATTR_UNUSED) {
}

void client_send_line(struct client *client ATTR_UNUSED, const char *line ATTR_UNUSED) {
}

void client_send_tagline(struct client_command_context *cmd ATTR_UNUSED,
                         const char *line ATTR_UNUSED) {
}

/* ---- command registration ---- */
void command_register(const char *name ATTR_UNUSED,
                      bool (*handler)(struct client_command_context *) ATTR_UNUSED,
                      unsigned int flags ATTR_UNUSED) {
}

void command_unregister(const char *name ATTR_UNUSED) {
}

/* ---- push notification ---- */
/* push_notification_events — the global events array (defined here) */
struct push_notification_events_array push_notification_events = { NULL, 0, 0 };

void test_set_push_events(const struct push_notification_event **events,
                          unsigned int count) {
    push_notification_events.arr = events;
    push_notification_events.count = count;
    push_notification_events.alloc = count;
}

void push_notification_driver_register(struct push_notification_driver *driver ATTR_UNUSED) {
}

void push_notification_driver_unregister(struct push_notification_driver *driver ATTR_UNUSED) {
}

void push_notification_driver_debug(const char *label ATTR_UNUSED,
                                    struct mail_user *user ATTR_UNUSED,
                                    const char *fmt ATTR_UNUSED, ...) {
}

void push_notification_event_init(struct push_notification_driver_txn *dtxn ATTR_UNUSED,
                                  const char *event_name ATTR_UNUSED,
                                  void *config ATTR_UNUSED,
                                  struct event *event ATTR_UNUSED) {
}

/* ---- strnum ---- */
bool str_to_uint(const char *str, unsigned int *num_r) {
    char *end;
    unsigned long v = strtoul(str, &end, 10);
    if (*end != '\0' || end == str) return false;
    *num_r = (unsigned int)v;
    return true;
}

bool str_to_int(const char *str, int *num_r) {
    char *end;
    long v = strtol(str, &end, 10);
    if (*end != '\0' || end == str) return false;
    *num_r = (int)v;
    return true;
}

/* ---- imap_client_created_hook_set (global) ---- */
static imap_client_created_func_t *current_hook;

imap_client_created_func_t *
imap_client_created_hook_set(imap_client_created_func_t *hook) {
    imap_client_created_func_t *prev = current_hook;
    current_hook = hook;
    return prev;
}
