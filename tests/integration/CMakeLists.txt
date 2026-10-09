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
        LEXUS_HEAD_UNIT_DBC_PATH="${CMAKE_SOURCE_DIR}/dbc/simulated_vehicle.dbc"
)

target_link_libraries(lexus_head_unit_test_support
    PUBLIC
        lexus_head_unit_can_source
        lexus_head_unit_replay_source
        lexus_head_unit_elm327_source
        lexus_head_unit_fake_source
        lexus_head_unit_service
        GTest::gtest
    PRIVATE
        lexus_head_unit_build_settings
)

add_executable(lexus_head_unit_integration_tests
    diagnostics_against_emulator_test.cpp
    trip_replay_test.cpp
    hub_process_cycle_test.cpp
    pid_discovery_test.cpp
    record_replay_test.cpp
    service_against_each_source_test.cpp
)

target_compile_definitions(lexus_head_unit_integration_tests
    PRIVATE
        LEXUS_HEAD_UNIT_FAKE_APP_PATH="$<TARGET_FILE:lexus_head_unit_fake_app>"
)
add_dependencies(lexus_head_unit_integration_tests lexus_head_unit_fake_app)

target_link_libraries(lexus_head_unit_integration_tests
    PRIVATE
        lexus_head_unit_hub_core
        lexus_head_unit_test_support
        lexus_head_unit_build_settings
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(lexus_head_unit_integration_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS integration
)

if(LEXUS_HEAD_UNIT_BUILD_HMI)
    add_executable(lexus_head_unit_hub_integration_tests
        hub_end_to_end_test.cpp
    )

    target_compile_definitions(lexus_head_unit_hub_integration_tests
        PRIVATE
            LEXUS_HEAD_UNIT_FAKE_APP_PATH="$<TARGET_FILE:lexus_head_unit_fake_app>"
            LEXUS_HEAD_UNIT_HUB_PATH="$<TARGET_FILE:lexus_head_unit_hub_app>"
    )
    add_dependencies(lexus_head_unit_hub_integration_tests
        lexus_head_unit_fake_app
        lexus_head_unit_hub_app
    )

    target_link_libraries(lexus_head_unit_hub_integration_tests
        PRIVATE
            lexus_head_unit_hub_viewmodels
            lexus_head_unit_build_settings
            Qt6::Core
            Qt6::Network
            GTest::gtest
    )

    gtest_discover_tests(lexus_head_unit_hub_integration_tests
        DISCOVERY_TIMEOUT 30
        PROPERTIES
            LABELS integration
            ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;QT_QPA_PLATFORM=offscreen"
            TIMEOUT 120
    )
endif()

if(LEXUS_HEAD_UNIT_BUILD_HMI)
    add_executable(lexus_head_unit_dbus_integration_tests
        vehicle_data_dbus_test.cpp
    )

    target_compile_definitions(lexus_head_unit_dbus_integration_tests
        PRIVATE
            LEXUS_HEAD_UNIT_SERVICE_PATH="$<TARGET_FILE:lexus_head_unit_vehicle_data_service_app>"
            LEXUS_HEAD_UNIT_INTERFACE_XML="${CMAKE_SOURCE_DIR}/src/service_dbus/interface/io.github.samersamaha.LexusHeadUnit.VehicleData1.xml"
    )
    add_dependencies(lexus_head_unit_dbus_integration_tests
        lexus_head_unit_vehicle_data_service_app
    )

    target_link_libraries(lexus_head_unit_dbus_integration_tests
        PRIVATE
            lexus_head_unit_service_dbus
            lexus_head_unit_build_settings
            Qt6::Core
            Qt6::DBus
            GTest::gtest
    )

    gtest_discover_tests(lexus_head_unit_dbus_integration_tests
        DISCOVERY_TIMEOUT 30
        PROPERTIES
            LABELS integration
            ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
            TIMEOUT 120
    )
endif()
