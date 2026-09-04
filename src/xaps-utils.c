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
#include <http-client.h>
#include <http-url.h>
#include <json-generator.h>
#include <settings.h>
#include <str.h>
#include <strescape.h>
#include <mail-storage-private.h>

#include <push-notification-plugin.h>
#include <push-notification-drivers.h>
#include <push-notification-txn-msg.h>

#include "xaps-settings.h"
#include "xaps-utils.h"

struct xaps_config *xaps_global;

// get the real name for users who are actually an alias
const char *get_real_mbox_user(struct mail_user *muser) {
    const char *username = muser->username;
    if (xaps_global->user_lookup != NULL && *xaps_global->user_lookup != '\0') {
        const char *lookup_key = xaps_global->user_lookup;
        size_t key_len = strlen(lookup_key);
        if (muser->userdb_fields != NULL) {
            for (unsigned int i = 0; muser->userdb_fields[i] != NULL; i++) {
                const char *field = muser->userdb_fields[i];
                if (strncmp(field, lookup_key, key_len) == 0 &&
                    field[key_len] == '=') {
                    username = field + key_len + 1;
                    break;
                }
            }
        }
    }
    return username;
}

/* Callback needed for i_stream_add_destroy_callback() in
   push_notification_driver_ox_process_msg. */
void str_free_i(string_t *str)
{
    str_free(&str);
}



void xaps_init(struct mail_user *muser, const char *http_path, pool_t pPool ATTR_UNUSED) {
    const char *error;
    const struct xaps_settings *xaps_set;

    if (xaps_global == NULL) {
        xaps_global = i_new(struct xaps_config, 1);
        xaps_global->pool = pool_alloconly_create("xaps config", 1024);
    }

    if (xaps_settings_get(muser->event, &xaps_set, &error) < 0) {
        i_error("xaps: Failed to get settings: %s", error);
        return;
    }

    /* Keep the parsed URL in our own pool so it outlives the transient
       transaction/command pool the caller provided. */
    if (xaps_global->http_url == NULL) {
        int ret = http_url_parse(xaps_set->xaps_url, NULL,
                                 HTTP_URL_ALLOW_USERINFO_PART,
                                 xaps_global->pool,
                                 &xaps_global->http_url, &error);
        if (ret != 0) {
            i_error("xaps: Failed to parse xaps_url '%s': %s",
                    xaps_set->xaps_url, error);
            settings_free(xaps_set);
            return;
        }
    }
    xaps_global->http_url->path = p_strdup(xaps_global->pool, http_path);

    if (xaps_global->user_lookup == NULL && *xaps_set->xaps_user_lookup != '\0') {
        xaps_global->user_lookup = p_strdup(xaps_global->pool,
                                            xaps_set->xaps_user_lookup);
    }

    settings_free(xaps_set);

    if (xaps_global->http_client == NULL) {
        if (http_client_init_auto(muser->event, &xaps_global->http_client, &error) < 0) {
            i_error("xaps: Failed to initialize HTTP client: %s", error);
            return;
        }
    }
}

void push_notification_driver_xaps_deinit(struct push_notification_driver_user *duser ATTR_UNUSED) {
    if (xaps_global != NULL) {
        if (xaps_global->http_client != NULL)
            http_client_wait(xaps_global->http_client);
    }
}

void push_notification_driver_xaps_cleanup(void)
{
    if (xaps_global != NULL) {
        if (xaps_global->http_client != NULL) {
            http_client_deinit(&xaps_global->http_client);
        }
        if (xaps_global->pool != NULL) {
            pool_unref(&xaps_global->pool);
        }
        i_free_and_null(xaps_global);
    }
}
