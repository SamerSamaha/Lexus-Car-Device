# Tests that need a real vcan0 interface (the Pi; deploy/setup_vcan.sh). Built and registered
# everywhere; where vcan0 is missing they skip with the ticket in the reason (D-003).
add_executable(lexus_head_unit_vcan_tests
    vcan_source_test.cpp
)

target_include_directories(lexus_head_unit_vcan_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/integration
)

target_compile_definitions(lexus_head_unit_vcan_tests
    PRIVATE
        LEXUS_HEAD_UNIT_DBC_PATH="${CMAKE_SOURCE_DIR}/dbc/simulated_vehicle.dbc"
)

target_link_libraries(lexus_head_unit_vcan_tests
    PRIVATE
        lexus_head_unit_can_source
        lexus_head_unit_build_settings
        GTest::gtest_main
)

gtest_discover_tests(lexus_head_unit_vcan_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS vcan
)
