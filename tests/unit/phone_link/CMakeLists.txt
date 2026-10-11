add_executable(lexus_head_unit_phone_link_tests
    phone_link_test.cpp
)

target_link_libraries(lexus_head_unit_phone_link_tests
    PRIVATE
        lexus_head_unit_phone_link
        lexus_head_unit_build_settings
        GTest::gtest_main
)

add_executable(lexus_head_unit_phone_link_tls_tests
    phone_link_tls_test.cpp
)

target_compile_definitions(lexus_head_unit_phone_link_tls_tests
    PRIVATE
        LEXUS_HEAD_UNIT_REPOSITORY_ROOT="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(lexus_head_unit_phone_link_tls_tests
    PRIVATE
        lexus_head_unit_phone_link
        lexus_head_unit_build_settings
        Qt6::Network
        Qt6::Test
        GTest::gtest
)

include(GoogleTest)
gtest_discover_tests(lexus_head_unit_phone_link_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS unit
)
gtest_discover_tests(lexus_head_unit_phone_link_tls_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS integration ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
)
