add_executable(lexus_head_unit_unit_tests
    service/connection_state_machine_test.cpp
    service/service_library_smoke_test.cpp
    service/signal_sample_test.cpp
    service/signal_store_test.cpp
    service/staleness_monitor_test.cpp
    service/steady_clock_test.cpp
)

target_link_libraries(lexus_head_unit_unit_tests
    PRIVATE
        lexus_head_unit_service
        lexus_head_unit_build_settings
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(lexus_head_unit_unit_tests
    DISCOVERY_TIMEOUT 30
)
