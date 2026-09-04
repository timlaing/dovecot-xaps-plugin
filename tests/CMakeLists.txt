cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(xaps_tests C)

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

    # Put stubs directory FIRST so our shadow headers are found instead of real Dovecot
    target_include_directories(${test_name} PRIVATE
        ${STUBS_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_compile_features(${test_name} PRIVATE c_std_99)
    set_target_properties(${test_name} PROPERTIES
        C_STANDARD 99
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
