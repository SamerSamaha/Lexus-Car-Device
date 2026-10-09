qt_add_executable(lexus_head_unit_hub_hmi_tests
    hub_hmi_test_main.cpp
)

lexus_head_unit_add_hub_qml_module(lexus_head_unit_hub_hmi_tests)

target_include_directories(lexus_head_unit_hub_hmi_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/unit/hub
)

target_link_libraries(lexus_head_unit_hub_hmi_tests
    PRIVATE
        lexus_head_unit_hub_viewmodels
        lexus_head_unit_build_settings
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickTest
)

add_test(NAME HubQuickTests
    COMMAND lexus_head_unit_hub_hmi_tests -input "${CMAKE_CURRENT_SOURCE_DIR}"
)
set_tests_properties(HubQuickTests PROPERTIES
    LABELS hmi
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;ASAN_OPTIONS=detect_leaks=0"
    TIMEOUT 120
)
