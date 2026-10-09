add_executable(lexus_head_unit_dbus_types_tests
    dbus_types_test.cpp
)

target_link_libraries(lexus_head_unit_dbus_types_tests
    PRIVATE
        lexus_head_unit_service_dbus
        lexus_head_unit_build_settings
        GTest::gtest_main
)

gtest_discover_tests(lexus_head_unit_dbus_types_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
)
