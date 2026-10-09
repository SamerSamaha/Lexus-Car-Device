add_executable(lexus_head_unit_scenario_tests
    fault_injection_scenarios_test.cpp
    reconnect_after_emulator_restart_test.cpp
)

target_link_libraries(lexus_head_unit_scenario_tests
    PRIVATE
        lexus_head_unit_test_support
        lexus_head_unit_build_settings
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(lexus_head_unit_scenario_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS scenario TIMEOUT 300
)
