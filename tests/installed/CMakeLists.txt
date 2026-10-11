add_test(NAME ProgramsRunOutsideBuildTree
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/run_outside_build_tree.sh"
        $<TARGET_FILE:lexus_head_unit_vehicle_data_app>
        $<TARGET_FILE:lexus_head_unit_hub_app>
)
set_tests_properties(ProgramsRunOutsideBuildTree PROPERTIES
    LABELS hmi
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    TIMEOUT 60
)

add_test(NAME VehicleDataAppUnitsFromTheCommandLine
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/units_from_the_command_line.sh"
        $<TARGET_FILE:lexus_head_unit_vehicle_data_app>
)
set_tests_properties(VehicleDataAppUnitsFromTheCommandLine PROPERTIES
    LABELS hmi
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    TIMEOUT 60
)
