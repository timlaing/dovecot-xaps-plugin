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

#include <lib.h>
#include <http-client.h>
#include <istream.h>
#include <str.h>
#include <push-notification-drivers.h>
#include <push-notification-events.h>

#ifndef DOVECOT_XAPS_PLUGIN_XAPS_H
#define DOVECOT_XAPS_PLUGIN_XAPS_H

#define XAPS_LOG_LABEL "XAPS Push Notification: "

const char *get_real_mbox_user(struct mail_user *muser);

/* This is data that is shared by all plugin users. */
struct xaps_config {
    struct http_url *http_url;
    struct http_client *http_client;
    const char *user_lookup;
    const unsigned char *aps_topic;
};

extern struct xaps_config *xaps_global;

void str_free_i(string_t *str);

void xaps_init(struct mail_user *muser, const char *http_path, pool_t pPool);

void push_notification_driver_xaps_deinit(struct push_notification_driver_user *duser ATTR_UNUSED);

void push_notification_driver_xaps_cleanup(void);

#endif
