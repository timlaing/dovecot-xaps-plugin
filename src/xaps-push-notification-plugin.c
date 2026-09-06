/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Stefan Arentz <stefan@arentz.ca>
 * Copyright (c) 2017 Frederik Schwan <frederik dot schwan at linux dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <config.h>
#include <lib.h>
#include <mail-storage.h>
#include <mail-storage-private.h>
#include <push-notification-events.h>
#include <push-notification-txn-mbox.h>
#include <push-notification-txn-msg.h>
#include <push-notification-event-messagenew.h>
#include <push-notification-event-messageappend.h>
#include <http-client.h>
#include <http-url.h>
#ifdef XAPS_HAVE_SETTINGS_FRAMEWORK
#include <settings.h>
#endif

#include "xaps-json.h"
#include "xaps-push-notification-plugin.h"
#include "xaps-settings.h"
#include "xaps-utils.h"

const char *xaps_plugin_version = DOVECOT_ABI_VERSION;

/*
 * Prepare message handling.
 * On return of false, the event gets dismissed for this driver
 */
static bool xaps_plugin_begin_txn(struct push_notification_driver_txn *dtxn) {
    const struct push_notification_event *const *event;
    struct push_notification_event_messagenew_config *eventMessagenewConfig;
    struct push_notification_event_messageappend_config *eventMessageappendConfig;

    push_notification_driver_debug(XAPS_LOG_LABEL, dtxn->ptxn->muser, "begin_txn: user: %s mailbox: %s",
                                   dtxn->ptxn->muser->username, dtxn->ptxn->mbox->name);

    // we have to initialize each event
    // the MessageNew event needs a config to appear in the process_msg function
    // so it's handled separately
    array_foreach(&push_notification_events, event) {
        if (strcmp((*event)->name,"MessageNew") == 0) {
            eventMessagenewConfig = p_new(dtxn->ptxn->pool, struct push_notification_event_messagenew_config, 1);
            // Take what you can, give nothing back
            eventMessagenewConfig->flags = PUSH_NOTIFICATION_MESSAGE_HDR_DATE |
                            PUSH_NOTIFICATION_MESSAGE_HDR_FROM |
                            PUSH_NOTIFICATION_MESSAGE_HDR_TO |
                            PUSH_NOTIFICATION_MESSAGE_HDR_SUBJECT |
                            PUSH_NOTIFICATION_MESSAGE_BODY_SNIPPET;
            push_notification_event_init(dtxn, "MessageNew", eventMessagenewConfig
#ifdef XAPS_HAVE_PUSH_EVENT_INIT_EVENT
                                         , dtxn->ptxn->event
#endif
            );
        } else if (strcmp((*event)->name,"MessageAppend") == 0) {
            eventMessageappendConfig = p_new(dtxn->ptxn->pool, struct push_notification_event_messageappend_config, 1);
            // Take what you can, give nothing back
            eventMessageappendConfig->flags = PUSH_NOTIFICATION_MESSAGE_HDR_DATE |
                            PUSH_NOTIFICATION_MESSAGE_HDR_FROM |
                            PUSH_NOTIFICATION_MESSAGE_HDR_TO |
                            PUSH_NOTIFICATION_MESSAGE_HDR_SUBJECT |
                            PUSH_NOTIFICATION_MESSAGE_BODY_SNIPPET;
            push_notification_event_init(dtxn, "MessageAppend", eventMessageappendConfig
#ifdef XAPS_HAVE_PUSH_EVENT_INIT_EVENT
                                         , dtxn->ptxn->event
#endif
            );
        } else {
            push_notification_event_init(dtxn, (*event)->name, NULL
#ifdef XAPS_HAVE_PUSH_EVENT_INIT_EVENT
                                         , dtxn->ptxn->event
#endif
            );
        }
    }
    return TRUE;
}


/**
 * Notify the backend daemon of an incoming mail. Right now we tell
 * the daemon the username and the mailbox in which a new email was
 * posted. The daemon can then lookup the user and see if any of the
 * devices want to receive a notification for that mailbox.
 */
static void xaps_notify(struct push_notification_driver_txn *dtxn, struct push_notification_txn_msg *msg) {
    struct push_notification_txn_event *const *event;
    struct http_client_request *http_req;
    string_t *str;
    struct istream *payload;

    xaps_init(dtxn->ptxn->muser, "/notify", dtxn->ptxn->pool);

    if (xaps_global == NULL || xaps_global->http_url == NULL ||
        xaps_global->http_client == NULL) {
        push_notification_driver_debug(XAPS_LOG_LABEL, dtxn->ptxn->muser,
            "Skipping notification: xaps not configured (missing xaps_url?)");
        return;
    }

    http_req = http_client_request_url(
            xaps_global->http_client, "POST", xaps_global->http_url,
            push_notification_driver_xaps_http_callback, dtxn->duser->context);
    http_client_request_set_event(http_req, dtxn->ptxn->event);
    http_client_request_add_header(http_req, "Content-Type",
                                   "application/json; charset=utf-8");

    str = str_new(default_pool, 256);
    str_append(str, "{\"Username\":\"");
    json_append_escaped(str, get_real_mbox_user(dtxn->ptxn->muser));
    str_append(str, "\",\"Mailbox\":\"");
    json_append_escaped(str, msg->mailbox);
    str_append(str, "\"");

    if (array_is_created(&msg->eventdata)) {
        str_append(str, ",\"Events\": [");
        int count = 0;
        array_foreach(&msg->eventdata, event) {
            if (count) {
                str_append(str, ",");
            }
            str_append(str, "\"");
            json_append_escaped(str, (*event)->event->event->name);
            str_append(str, "\"");
            count++;
        }
        str_append(str, "]");
    }
    str_append(str, "}");

    i_debug("Sending notification: %s", str_c(str));

    payload = i_stream_create_from_data(str_data(str), str_len(str));
    i_stream_add_destroy_callback(payload, str_free_i, str);
    http_client_request_set_payload(http_req, payload, FALSE);

    http_client_request_submit(http_req);
    i_stream_unref(&payload);
}

void push_notification_driver_xaps_http_callback(const struct http_response *response, void *context) {
    switch (response->status / 100) {
        case 2:
            // Success.
            i_debug("Notification sent successfully: %s", http_response_get_message(response));
            break;

        default:
            if (response->status == 404) {
                i_debug("Notification sent successfully, but no registered device found: %s", http_response_get_message(response));
            } else {
                // Error.
                i_error("Error when sending notification: %s", http_response_get_message(response));
            }
            break;
    }
}

// push-notification driver definition

const char *xaps_plugin_dependencies[] = { "push_notification", NULL };

extern struct push_notification_driver push_notification_driver_xaps;

int xaps_push_plugin_init(struct mail_user *muser ATTR_UNUSED,
                 pool_t pPool ATTR_UNUSED,
                 const char *name ATTR_UNUSED,
                 void **pVoid ATTR_UNUSED,
                 const char **pString ATTR_UNUSED) {
//    xaps_init(muser, "/notify", pPool);
    return 0;
}

#ifdef XAPS_LEGACY_DOVECOT
int xaps_push_plugin_init_legacy(struct push_notification_driver_config *config ATTR_UNUSED,
                       struct mail_user *muser ATTR_UNUSED,
                       pool_t pPool ATTR_UNUSED,
                       void **pVoid ATTR_UNUSED,
                       const char **pString ATTR_UNUSED) {
//    xaps_init(muser, "/notify", pPool);
    return 0;
}
#endif

void xaps_plugin_deinit(struct push_notification_driver_user *duser) {
    push_notification_driver_xaps_deinit(duser);
}

void xaps_plugin_cleanup(void) {
    push_notification_driver_xaps_cleanup();
}

struct push_notification_driver push_notification_driver_xaps = {
        .name = "xaps",
        .v = {
                .init =
#ifdef XAPS_LEGACY_DOVECOT
                    xaps_push_plugin_init_legacy,
#else
                    xaps_push_plugin_init,
#endif
                .begin_txn = xaps_plugin_begin_txn,
                .process_msg = xaps_notify,
                .deinit = xaps_plugin_deinit,
                .cleanup = xaps_plugin_cleanup,
        }
};

// plugin init and deinit

void xaps_push_notification_plugin_init(struct module *module ATTR_UNUSED) {
#ifdef XAPS_HAVE_SETTINGS_FRAMEWORK
    settings_info_register(&xaps_setting_parser_info);
#endif
    push_notification_driver_register(&push_notification_driver_xaps);
}

void xaps_push_notification_plugin_deinit(void) {
    push_notification_driver_unregister(&push_notification_driver_xaps);
}
