add_executable(lexus_head_unit_integration_tests
    service_against_each_source_test.cpp
)

target_include_directories(lexus_head_unit_integration_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(lexus_head_unit_integration_tests
    PRIVATE
        lexus_head_unit_service
        lexus_head_unit_fake_source
        lexus_head_unit_build_settings
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(lexus_head_unit_integration_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS integration
)
