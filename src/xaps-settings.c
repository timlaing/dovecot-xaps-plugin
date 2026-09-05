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
#include <settings.h>
#include <settings-parser.h>

#include "xaps-settings.h"

#undef DEF
#define DEF(type, name) \
    SETTING_DEFINE_STRUCT_##type("xaps_"#name, xaps_##name, struct xaps_settings)

static const struct setting_define xaps_setting_defines[] = {
    DEF(STR_NOVARS, url),
    DEF(STR_NOVARS, user_lookup),
    SETTING_DEFINE_LIST_END
};

static const struct xaps_settings xaps_default_settings = {
    .xaps_url         = "",
    .xaps_user_lookup = "",
};

const struct setting_parser_info xaps_setting_parser_info = {
    .name = "xaps",

    .defines  = xaps_setting_defines,
    .defaults = &xaps_default_settings,

    .struct_size = sizeof(struct xaps_settings),
    /* pool_offset1: Dovecot convention — 1 + offsetof pool field so the
       settings framework can allocate string values into this pool. */
    .pool_offset1 = 1 + offsetof(struct xaps_settings, pool),
};

int xaps_settings_get(struct event *event,
                      const struct xaps_settings **set_r,
                      const char **error_r)
{
    if (settings_get(event, &xaps_setting_parser_info, 0, set_r, error_r) < 0)
        return -1;

    if (*(*set_r)->xaps_url == '\0') {
        *error_r = "xaps_url is required";
        settings_free(*set_r);
        *set_r = NULL;
        return -1;
    }

    return 0;
}
