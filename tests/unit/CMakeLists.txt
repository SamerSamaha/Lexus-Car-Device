add_executable(lexus_head_unit_unit_tests
    elm327/elm327_obd_source_test.cpp
    elm327/reconnect_backoff_test.cpp
    hardware/fake_byte_transport_test.cpp
    hardware/file_descriptor_byte_transport_test.cpp
    hardware/fake_source_test.cpp
    obd/command_allowlist_test.cpp
    obd/elm327_protocol_malformed_input_test.cpp
    obd/elm327_protocol_test.cpp
    obd/obd_pid_decoder_test.cpp
    obd/supported_pid_set_test.cpp
    service/connection_state_machine_test.cpp
    service/key_value_configuration_test.cpp
    service/service_library_smoke_test.cpp
    service/signal_sample_test.cpp
    service/signal_store_feeder_test.cpp
    service/signal_store_test.cpp
    service/staleness_monitor_test.cpp
    service/steady_clock_test.cpp
)

target_link_libraries(lexus_head_unit_unit_tests
    PRIVATE
        lexus_head_unit_service
        lexus_head_unit_elm327_source
        lexus_head_unit_fake_source
        lexus_head_unit_obd
        lexus_head_unit_transport
        lexus_head_unit_build_settings
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(lexus_head_unit_unit_tests
    DISCOVERY_TIMEOUT 30
)
