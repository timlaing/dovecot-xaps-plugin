# Dovecot version "port profiles".
#
# One source tree builds .debs for different Dovecot major versions. Which
# Dovecot major to target is selected with:
#
#   cmake -S . -B build -DXAPS_DOVECOT_MAJOR=2.3    # Dovecot 2.3
#   cmake -S . -B build -DXAPS_DOVECOT_MAJOR=2.4    # Dovecot 2.4 (default)
#
# Each profile maps the Dovecot major version to:
#   * module library prefix (e.g. lib25_ for Dovecot 2.4, lib20_ for 2.3)
#   * feature macros compiled into the plugin source
#   * Debian package dependency floor and Debian revision suffix
#   * the Dovecot configuration template to ship as 95-xaps.conf
#
# The forward (Dovecot 2.4+) behavior is the default for every feature macro,
# so a future Dovecot 3.x profile only overrides what a real ABI change
# forces, and the plugin sources never hardcode Dovecot version numbers.
set(XAPS_DOVECOT_MAJOR "2.4" CACHE STRING
    "Dovecot major version to build for (supported: 2.4, 2.3)")

set(XAPS_SUPPORTED_MAJORS "2.4" "2.3")
if (NOT XAPS_DOVECOT_MAJOR IN_LIST XAPS_SUPPORTED_MAJORS)
    message(FATAL_ERROR
        "XAPS_DOVECOT_MAJOR=${XAPS_DOVECOT_MAJOR} is not supported "
        "(supported: ${XAPS_SUPPORTED_MAJORS})")
endif ()

# Forward profile (Dovecot 2.4 and any later 2.x/3.x major unless a profile
# below overrides it).
set(XAPS_MODULE_PREFIX "lib25")
set(XAPS_DEBIAN_CORE_DEP "2.4.0")
set(XAPS_DEBIAN_REVISION "1")
set(XAPS_CONF_SOURCE "xaps.conf")

# Feature macros used by the plugin sources. The forward behavior is enabled
# by defining these macros.
set(XAPS_HAVE_SETTINGS_FRAMEWORK TRUE)      # 2.4 settings framework + settings_info_register()
set(XAPS_HAVE_HTTP_CLIENT_INIT_AUTO TRUE)   # http_client_init_auto() vs http_client_init(&set)
set(XAPS_HAVE_PUSH_EVENT_INIT_EVENT TRUE)   # 4th event arg to push_notification_event_init()
set(XAPS_HAVE_JSON_GENERATOR TRUE)          # json-generator.h (2.3 ships json-parser.h instead)

if (XAPS_DOVECOT_MAJOR STREQUAL "2.3")
    set(XAPS_MODULE_PREFIX "lib20")
    set(XAPS_DEBIAN_CORE_DEP "2.3.0")
    set(XAPS_DEBIAN_REVISION "1~dov23")
    set(XAPS_CONF_SOURCE "packaging/xaps-23.conf")

    # Dovecot 2.3 predates the 2.4 settings framework and reads plugin
    # settings from the plugin {} block via mail_user_plugin_getenv().
    set(XAPS_HAVE_SETTINGS_FRAMEWORK FALSE)
    set(XAPS_HAVE_HTTP_CLIENT_INIT_AUTO FALSE)
    set(XAPS_HAVE_PUSH_EVENT_INIT_EVENT FALSE)
    set(XAPS_HAVE_JSON_GENERATOR FALSE)
endif ()

set(XAPS_COMPILE_DEFINITIONS "")
if (XAPS_HAVE_SETTINGS_FRAMEWORK)
    list(APPEND XAPS_COMPILE_DEFINITIONS XAPS_HAVE_SETTINGS_FRAMEWORK)
else ()
    # Marks 2.3-only code paths (legacy push-notification driver init
    # signature, plugin {} settings). Undefined for every other profile.
    list(APPEND XAPS_COMPILE_DEFINITIONS XAPS_LEGACY_DOVECOT)
endif ()
if (XAPS_HAVE_HTTP_CLIENT_INIT_AUTO)
    list(APPEND XAPS_COMPILE_DEFINITIONS XAPS_HAVE_HTTP_CLIENT_INIT_AUTO)
endif ()
if (XAPS_HAVE_PUSH_EVENT_INIT_EVENT)
    list(APPEND XAPS_COMPILE_DEFINITIONS XAPS_HAVE_PUSH_EVENT_INIT_EVENT)
endif ()
if (XAPS_HAVE_JSON_GENERATOR)
    list(APPEND XAPS_COMPILE_DEFINITIONS XAPS_HAVE_JSON_GENERATOR)
endif ()

# The settings module is only shipped for the settings-framework (2.4+)
# majors; Dovecot 2.3 has no /modules/settings directory.
set(XAPS_BUILD_SETTINGS_MODULE "${XAPS_HAVE_SETTINGS_FRAMEWORK}")

message(STATUS "Dovecot port profile: ${XAPS_DOVECOT_MAJOR} "
               "(module prefix ${XAPS_MODULE_PREFIX}, "
               "core dependency dovecot-core (>= ${XAPS_DEBIAN_CORE_DEP}))")