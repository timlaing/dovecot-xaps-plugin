cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

# Only initialize a standalone project when configured directly
# (cmake -S tests); as a subdirectory of the main build the parent
# project() already owns the build.
if(NOT PROJECT_NAME)
    project(xaps_tests C)
endif()

# Tests use Dovecot stubs that mirror the forward (2.4+) API. Select that
# profile when configured standalone so the same feature macros are defined.
if (NOT DEFINED XAPS_COMPILE_DEFINITIONS)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/dovecot-port.cmake)
endif ()
foreach(XAPS_DEFINITION ${XAPS_COMPILE_DEFINITIONS})
    add_definitions(-D${XAPS_DEFINITION})
endforeach()

enable_testing()

# Test sources (each produces its own test executable)
set(TEST_SOURCES
    test_xaps_utils.c
    test_xaps_push_notification.c
    test_xaps_imap.c
)

# Dovecot stubs implementation (mock functions)
set(STUBS_SOURCE dovecot_stubs.c)

# Stubs include directory (shadow real Dovecot headers)
set(STUBS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dovecot_stubs")

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src} ${STUBS_SOURCE})

    # Override include dirs so the stubs shadow real Dovecot headers even when
    # tests are built as part of the main build (which adds DOVECOT_INCLUDE_DIR).
    set_property(TARGET ${test_name} PROPERTY INCLUDE_DIRECTORIES
        ${STUBS_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_compile_features(${test_name} PRIVATE c_std_99)
    set_target_properties(${test_name} PROPERTIES
        C_STANDARD 99
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
endforeach()
