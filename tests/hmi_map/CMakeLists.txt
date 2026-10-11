qt_add_executable(lexus_head_unit_map_hmi_tests
    map_hmi_test_main.cpp
    map_test_driver.h
)

lexus_head_unit_add_map_qml_module(lexus_head_unit_map_hmi_tests)

target_link_libraries(lexus_head_unit_map_hmi_tests
    PRIVATE
        lexus_head_unit_hmi_map
        lexus_head_unit_build_settings
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickTest
)

add_test(NAME MapQuickTests
    COMMAND lexus_head_unit_map_hmi_tests -input "${CMAKE_CURRENT_SOURCE_DIR}"
)
set_tests_properties(MapQuickTests PROPERTIES
    LABELS hmi
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;ASAN_OPTIONS=detect_leaks=0"
    TIMEOUT 120
)
