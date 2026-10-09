add_library(lexus_head_unit_test_support STATIC
    emulator_process.cpp
)

target_include_directories(lexus_head_unit_test_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(lexus_head_unit_test_support
    PUBLIC
        LEXUS_HEAD_UNIT_REPOSITORY_ROOT="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(lexus_head_unit_test_support
    PUBLIC
        lexus_head_unit_elm327_source
        lexus_head_unit_fake_source
        lexus_head_unit_service
        GTest::gtest
    PRIVATE
        lexus_head_unit_build_settings
)

add_executable(lexus_head_unit_integration_tests
    pid_discovery_test.cpp
    service_against_each_source_test.cpp
)

target_link_libraries(lexus_head_unit_integration_tests
    PRIVATE
        lexus_head_unit_test_support
        lexus_head_unit_build_settings
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(lexus_head_unit_integration_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS integration
)
