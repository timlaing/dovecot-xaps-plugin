cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

set(XAPS_VERSION "1.0.0" CACHE STRING "Version used for release packages")
project(dovecot-xaps-plugin VERSION ${XAPS_VERSION} LANGUAGES C)

if (APPLE)
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_client_read_args")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_client_send_command_error")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_client_send_line")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_client_send_tagline")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_command_register")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_command_unregister")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_http_client_init_auto")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_http_client_wait")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_imap_client_created_hook_set")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_push_notification_driver_debug")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_push_notification_driver_register")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_push_notification_driver_unregister")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_push_notification_event_init")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_push_notification_events")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_settings_free")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_settings_get")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_settings_info_register")
endif ()

find_path(DOVECOT_INCLUDE_DIR NAMES lib.h PATH_SUFFIXES dovecot)
find_library(LIBDOVECOT NAMES dovecot PATH_SUFFIXES dovecot)
find_library(LIBDOVECOTSTORAGE NAMES dovecot-storage PATH_SUFFIXES dovecot)

if (NOT DOVECOT_INCLUDE_DIR OR NOT LIBDOVECOT OR NOT LIBDOVECOTSTORAGE)
    message(FATAL_ERROR "Dovecot development headers and libraries were not found")
endif ()

include_directories(${DOVECOT_INCLUDE_DIR})

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(lib25_xaps_push_notification_plugin MODULE xaps-utils.c xaps-settings.c xaps-push-notification-plugin.c)
add_library(lib25_xaps_imap_plugin MODULE xaps-utils.c xaps-settings.c xaps-imap-plugin.c)
add_library(xaps_settings MODULE xaps-settings.c xaps-settings-plugin.c)

target_link_libraries(lib25_xaps_push_notification_plugin ${LIBDOVECOT} ${LIBDOVECOTSTORAGE})
target_link_libraries(lib25_xaps_imap_plugin ${LIBDOVECOT} ${LIBDOVECOTSTORAGE})
target_link_libraries(xaps_settings ${LIBDOVECOT})

set_target_properties(lib25_xaps_push_notification_plugin PROPERTIES PREFIX "")
set_target_properties(lib25_xaps_imap_plugin PROPERTIES PREFIX "")

install(TARGETS lib25_xaps_push_notification_plugin DESTINATION /usr/lib/dovecot/modules)
install(TARGETS lib25_xaps_imap_plugin DESTINATION /usr/lib/dovecot/modules)
install(TARGETS xaps_settings DESTINATION /usr/lib/dovecot/modules/settings)
install(FILES xaps.conf DESTINATION /etc/dovecot/conf.d RENAME 95-xaps.conf)
install(FILES LICENSE DESTINATION /usr/share/doc/dovecot-xaps-plugin RENAME copyright)
install(FILES CHANGELOG.md DESTINATION /usr/share/doc/dovecot-xaps-plugin)

find_program(GZIP_EXECUTABLE gzip)
if (NOT GZIP_EXECUTABLE)
    message(FATAL_ERROR "gzip is required to build the Debian changelog")
endif ()
string(TIMESTAMP DEBIAN_CHANGELOG_DATE "%a, %d %b %Y %H:%M:%S +0000" UTC)
configure_file(packaging/changelog.in changelog.Debian @ONLY)
execute_process(
    COMMAND "${GZIP_EXECUTABLE}" -9 -n -c "${CMAKE_CURRENT_BINARY_DIR}/changelog.Debian"
    OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/changelog.Debian.gz"
    RESULT_VARIABLE GZIP_RESULT)
if (NOT GZIP_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to compress the Debian changelog")
endif ()
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/changelog.Debian.gz"
        DESTINATION /usr/share/doc/dovecot-xaps-plugin)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "dovecot-xaps-plugin")
set(CPACK_PACKAGE_VENDOR "dovecot-xaps-plugin contributors")
set(CPACK_PACKAGE_CONTACT "dovecot-xaps-plugin contributors <noreply@github.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Apple push notification plugins for Dovecot")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
    "Dovecot plugins implementing the XAPPLEPUSHSERVICE IMAP extension and
 forwarding mail events to the companion xapsd APNS daemon.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/timlaing/dovecot-xaps-plugin")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_DEBIAN_PACKAGE_VERSION "${PROJECT_VERSION}-1")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_SET_DESTDIR ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "dovecot-core (>= 2.4.2), libc6")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SECTION "mail")
set(CPACK_STRIP_FILES TRUE)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/conffiles")

include(CPack)
