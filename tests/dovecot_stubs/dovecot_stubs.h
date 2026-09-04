/*
 * Comprehensive Dovecot type stubs for unit testing.
 * Provides all types, macros, and function declarations needed by the
 * xaps plugin source files so they compile without the real Dovecot SDK.
 */
#ifndef DOVECOT_STUBS_H
#define DOVECOT_STUBS_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>

/* ---- compiler attributes ---- */
#ifndef ATTR_UNUSED
#define ATTR_UNUSED __attribute__((unused))
#endif
#ifndef ATTR_PRINTF
#define ATTR_PRINTF(n1, n2) __attribute__((format(printf, n1, n2)))
#endif
#ifndef ATTR_NONNULL
#define ATTR_NONNULL __attribute__((nonnull))
#endif
#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif
#define STMT_START do {
#define STMT_END   } while(0)

/* ---- standard constants ---- */
#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* ---- pool / memory ---- */
typedef void *pool_t;

pool_t pool_alloconly_create(const char *name, size_t min_size);
void pool_unref(pool_t *pool);
#define p_new(pool, type, num) ((type *)calloc((num), sizeof(type)))
char *p_strdup(pool_t pool, const char *str);

void *i_new_impl(size_t size);
#define i_new(type, num) ((type *)i_new_impl((size_t)(num) * sizeof(type)))
void *i_malloc(size_t size);
void i_free(void *ptr);
char *i_strdup(const char *str);
char *i_strdup_until(const char *start, const char *end);
#define i_free_and_null(ptr) do { i_free(ptr); (ptr) = NULL; } while(0)

#define default_pool NULL

/* ---- string_t ---- */
typedef struct _string_t {
    char *str;
    size_t used;
    size_t alloc;
} string_t;

string_t *str_new(pool_t pool, size_t initial_size);
void str_append(string_t *str, const char *cstr);
void str_append_len(string_t *str, const char *data, size_t len);
void str_printfa(string_t *str, const char *fmt, ...) ATTR_PRINTF(2, 3);
const char *str_c(const string_t *str);
size_t str_len(const string_t *str);
const char *str_data(const string_t *str);
void str_free(string_t **str);

/* ---- buffer (minimal) ---- */
typedef struct buffer {
    unsigned char *data;
    size_t used;
    size_t alloc;
    pool_t pool;
} buffer_t;

/* ---- array (macro-based, matches Dovecot layout) ---- */
#define ARRAY_DEFINE(name, elem_type) \
    struct { \
        elem_type *arr; \
        unsigned int count; \
        unsigned int alloc; \
    } name

#define ARRAY_DEFINE_STATIC(name, elem_type) \
    static ARRAY_DEFINE(name, elem_type)

#define ARRAY_INIT  { NULL, 0, 0 }
#define EMPTY_ARRAY { NULL, 0, 0 }

/* array iteration — matches Dovecot semantics: the array storage is cast to
   void* for the initial assignment (implicit void* -> typed-pointer), and the
   end pointer is computed via char* byte arithmetic.  The loops never actually
   execute in unit tests because the arrays are empty. */
#define array_foreach(_arr, elem) \
    for ((elem) = (void *)(_arr)->arr; \
         (elem) != (const char *)(const void *)(_arr)->arr + \
             (size_t)(_arr)->count * sizeof(*(elem)); \
         (elem)++)

#define array_is_created(_arr) ((_arr)->arr != NULL)

/* ---- hash_table (opaque) ---- */
struct hash_table;
struct hash_table *hash_table_create(size_t expected_count, unsigned int flags);
void hash_table_destroy(struct hash_table **table);
void hash_table_insert(struct hash_table *table, const void *key, void *value);
void *hash_table_lookup(struct hash_table *table, const void *key);
bool hash_table_remove(struct hash_table *table, const void *key);

/* ---- istream ---- */
struct istream {
    pool_t pool;
    const unsigned char *buffer;
    size_t buffer_size;
    size_t skip;
    int refcount;
};

void i_stream_ref(struct istream *stream);
void i_stream_unref(struct istream **stream);
struct istream *i_stream_create_from_data(const void *data, size_t size);
int i_stream_read_data(struct istream *stream, const unsigned char **data_r,
                       size_t *size_r, size_t threshold);
#define i_stream_add_destroy_callback(stream, callback, context)

/* ---- ostream (opaque) ---- */
struct ostream;

/* ---- event (opaque) ---- */
struct event;

/* ---- http_url ---- */
struct http_url {
    const char *path;
    const char *host;
    unsigned int port;
    const char *scheme;
};

#define HTTP_URL_ALLOW_USERINFO_PART (1 << 0)

int http_url_parse(const char *url, const char *default_host,
                   unsigned int flags, pool_t pool,
                   struct http_url **url_r, const char **error_r);
const char *http_url_to_string(const struct http_url *url, pool_t pool);

/* ---- http_client ---- */
struct http_client;
struct http_client_request;

struct http_response {
    unsigned int status;
    const char *status_line;
    struct istream *payload;
    struct istream *headers;
};

typedef void (*http_client_request_callback_t)(const struct http_response *response,
                                               void *context);

int http_client_init_auto(struct event *event, struct http_client **client_r,
                           const char **error_r);
struct http_client_request *
http_client_request_url(struct http_client *client, const char *method,
                        const struct http_url *url,
                        http_client_request_callback_t callback, void *context);
void http_client_request_add_header(struct http_client_request *req,
                                    const char *key, const char *value);
void http_client_request_set_payload(struct http_client_request *req,
                                     struct istream *payload, bool get_ownership);
void http_client_request_set_event(struct http_client_request *req,
                                   struct event *event);
void http_client_request_submit(struct http_client_request *req);
void http_client_wait(struct http_client *client);
void http_client_deinit(struct http_client **client);
const char *http_response_get_message(const struct http_response *response);

/* ---- json ---- */
void json_append_escaped(string_t *str, const char *src);

/* ---- settings ---- */
struct setting_define {
    int type;
    const char *key;
    unsigned int offset;
};

struct setting_parser_info {
    const char *name;
    const struct setting_define *defines;
    const void *defaults;
    unsigned int struct_size;
    unsigned int pool_offset1;
};

void settings_info_register(const struct setting_parser_info *info);
int settings_get_impl(struct event *event, const struct setting_parser_info *info,
                      unsigned int flags, const char *source_filename,
                      unsigned int source_linenum, const void **set_r,
                      const char **error_r);
void settings_free(const void *set);

/* Mirror Dovecot's settings_get() macro: it injects __FILE__/__LINE__ and
   casts the out-param pointer to (void*), hiding the incompatible-pointer
   conversion that strict compilers would otherwise reject. */
#define settings_get(event, info, flags, set_r, error_r) \
    settings_get_impl((event), (info), (flags), __FILE__, __LINE__, \
                      (void *)(set_r), (error_r))

#define SETTING_DEFINE_STRUCT_STR_NOVARS(key, field, struct_type) \
    { 0, key, offsetof(struct_type, field) }
#define SETTING_DEFINE_LIST_END { -1, NULL, 0 }

/* ---- logging ---- */
void i_error(const char *fmt, ...) ATTR_PRINTF(1, 2);
void i_debug(const char *fmt, ...) ATTR_PRINTF(1, 2);
void i_warning(const char *fmt, ...) ATTR_PRINTF(1, 2);
void i_assert_fail(const char *condition, const char *file, unsigned int line)
    __attribute__((noreturn));
#define i_assert(cond) \
    do { if (!(cond)) i_assert_fail(#cond, __FILE__, __LINE__); } while(0)

/* ---- mail_user ---- */
struct mail_namespace;

struct mail_user {
    pool_t pool;
    const char *username;
    const char **userdb_fields;
    struct event *event;
    struct mail_namespace *namespaces;
};

/* ---- module (opaque) ---- */
struct module {
    const char *name;
};

bool mail_user_is_plugin_loaded(struct mail_user *user, struct module *module);

/* ---- imap arg types ---- */
enum imap_arg_type {
    IMAP_ARG_NIL = 0,
    IMAP_ARG_ATOM,
    IMAP_ARG_STRING,
    IMAP_ARG_LITERAL,
    IMAP_ARG_LIST,
    IMAP_ARG_ELIST,
    IMAP_ARG_LITERAL_SIZE,
};

struct imap_arg {
    enum imap_arg_type type;
    union {
        const char *str;
        const struct imap_arg *list;
        uint64_t literal_size;
    } _data;
};

#define IMAP_ARG_IS_EOL(arg) ((arg)->type == IMAP_ARG_NIL)

bool imap_arg_get_astring(const struct imap_arg *arg, const char **str_r);
bool imap_arg_get_list(const struct imap_arg *arg, const struct imap_arg **list_r);

/* ---- imap client ---- */
struct client {
    struct mail_user *user;
    string_t *capability_string;
    pool_t pool;
    struct event *event;
};

struct client_command_context {
    struct client *client;
    void *context;
    pool_t pool;
};

bool client_read_args(struct client_command_context *cmd, unsigned int count,
                      unsigned int max_args, const struct imap_arg **args_r);
void client_send_command_error(struct client_command_context *cmd, const char *msg);
void client_send_line(struct client *client, const char *line);
void client_send_tagline(struct client_command_context *cmd, const char *line);

/* Real Dovecot declares this as a function type (not a function pointer
   type) so that the source's `func_t *next_hook` idiom compiles. */
typedef void imap_client_created_func_t(struct client **client);
imap_client_created_func_t *imap_client_created_hook_set(imap_client_created_func_t *hook);

/* ---- command registration ---- */
void command_register(const char *name,
                      bool (*handler)(struct client_command_context *),
                      unsigned int flags);
void command_unregister(const char *name);

/* ---- push notification ---- */
struct push_notification_driver_user {
    void *context;
};

struct push_notification_event_config;

struct push_notification_driver_txn {
    struct push_notification_driver_user *duser;
    struct {
        struct mail_user *muser;
        struct mailbox *mbox;
        pool_t pool;
        struct event *event;
    } *ptxn;
};

struct push_notification_txn_msg {
    const char *mailbox;
    ARRAY_DEFINE(eventdata, void *);
};

struct push_notification_txn_event {
    struct push_notification_event_config *event;
    void *data;
};

struct push_notification_event_config {
    const struct push_notification_event *event;
    struct event *log_event;
    void *config;
};

struct push_notification_event {
    const char *name;
    struct {
        void *(*default_config)(void);
    } init;
};

struct push_notification_event_messagenew_config {
    unsigned int flags;
};

struct push_notification_event_messageappend_config {
    unsigned int flags;
};

#define PUSH_NOTIFICATION_MESSAGE_HDR_DATE       (1 << 0)
#define PUSH_NOTIFICATION_MESSAGE_HDR_FROM      (1 << 1)
#define PUSH_NOTIFICATION_MESSAGE_HDR_TO        (1 << 2)
#define PUSH_NOTIFICATION_MESSAGE_HDR_SUBJECT   (1 << 3)
#define PUSH_NOTIFICATION_MESSAGE_BODY_SNIPPET  (1 << 4)

/* push_notification_events — the global events array. Declared here with a
   named struct type; defined in dovecot_stubs.c. */
struct push_notification_events_array {
    const struct push_notification_event **arr;
    unsigned int count;
    unsigned int alloc;
};
extern struct push_notification_events_array push_notification_events;

struct push_notification_driver {
    const char *name;
    struct {
        int (*init)(struct mail_user *, pool_t, const char *, void **, const char **);
        bool (*begin_txn)(struct push_notification_driver_txn *);
        void (*process_msg)(struct push_notification_driver_txn *,
                            struct push_notification_txn_msg *);
        void (*deinit)(struct push_notification_driver_user *);
        void (*cleanup)(void);
    } v;
};

void push_notification_driver_register(struct push_notification_driver *driver);
void push_notification_driver_unregister(struct push_notification_driver *driver);
void push_notification_driver_debug(const char *label, struct mail_user *user,
                                    const char *fmt, ...) ATTR_PRINTF(3, 4);
void push_notification_event_init(struct push_notification_driver_txn *dtxn,
                                  const char *event_name, void *config,
                                  struct event *event);

/* ---- mailbox (minimal) ---- */
struct mailbox {
    const char *name;
};

/* ---- strnum ---- */
bool str_to_uint(const char *str, unsigned int *num_r);
bool str_to_int(const char *str, int *num_r);

/* ---- t_strdup_printf (temp pool printf) ---- */
char *t_strdup_printf(const char *fmt, ...) ATTR_PRINTF(1, 2);

/* ---- test hooks (implemented in dovecot_stubs.c) ----
   These let tests drive the otherwise-Dovecot-controlled inputs. */
void test_set_settings_url(const char *url);
void test_set_settings_user_lookup(const char *lookup);
void test_set_settings_get_fail(bool fail);
void test_set_http_url_parse_fail(bool fail);
void test_set_http_client_init_fail(bool fail);
void test_set_imap_args(const struct imap_arg *args, unsigned int count);
void test_set_read_args_fail(bool fail);
const char *test_get_last_payload(void);
void test_set_push_events(const struct push_notification_event **events,
                          unsigned int count);

#endif /* DOVECOT_STUBS_H */
