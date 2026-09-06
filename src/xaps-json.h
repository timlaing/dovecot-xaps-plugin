#ifndef XAPS_JSON_H
#define XAPS_JSON_H

/* Dovecot 2.4 splits JSON string escaping into json-generator.h; Dovecot 2.3
   ships the same json_append_escaped() helpers in json-parser.h. */
#ifdef XAPS_HAVE_JSON_GENERATOR
#  include <json-generator.h>
#else
#  include <json-parser.h>
#endif

#endif