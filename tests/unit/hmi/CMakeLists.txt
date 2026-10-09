add_executable(lexus_head_unit_view_model_tests
    diagnostics_view_model_test.cpp
    view_models_test.cpp
)

set_target_properties(lexus_head_unit_view_model_tests PROPERTIES AUTOMOC ON)

target_link_libraries(lexus_head_unit_view_model_tests
    PRIVATE
        lexus_head_unit_hmi_viewmodels
        lexus_head_unit_build_settings
        Qt6::Core
        Qt6::Test
        GTest::gtest
)

include(GoogleTest)
gtest_discover_tests(lexus_head_unit_view_model_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS hmi ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
)

add_executable(lexus_head_unit_hub_view_model_tests
    hub_view_model_test.cpp
)

set_target_properties(lexus_head_unit_hub_view_model_tests PROPERTIES AUTOMOC ON)

target_include_directories(lexus_head_unit_hub_view_model_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/unit/hub
)

target_link_libraries(lexus_head_unit_hub_view_model_tests
    PRIVATE
        lexus_head_unit_hub_viewmodels
        lexus_head_unit_build_settings
        Qt6::Core
        Qt6::Test
        GTest::gtest
)

gtest_discover_tests(lexus_head_unit_hub_view_model_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS hmi ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
)

add_executable(lexus_head_unit_power_status_model_tests
    power_status_model_test.cpp
    ${CMAKE_SOURCE_DIR}/tests/hmi_hub/fake_power_status_reader.h
)

set_target_properties(lexus_head_unit_power_status_model_tests PROPERTIES AUTOMOC ON)

target_include_directories(lexus_head_unit_power_status_model_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/hmi_hub
)

target_link_libraries(lexus_head_unit_power_status_model_tests
    PRIVATE
        lexus_head_unit_hub_viewmodels
        lexus_head_unit_build_settings
        Qt6::Core
        Qt6::Test
        GTest::gtest
)

gtest_discover_tests(lexus_head_unit_power_status_model_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS hmi ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
)

add_executable(lexus_head_unit_measurement_probe_tests
    latency_probe_test.cpp
)

set_target_properties(lexus_head_unit_measurement_probe_tests PROPERTIES AUTOMOC ON)

target_link_libraries(lexus_head_unit_measurement_probe_tests
    PRIVATE
        lexus_head_unit_hmi_viewmodels
        lexus_head_unit_process_support
        lexus_head_unit_build_settings
        Qt6::Core
        GTest::gtest
)

gtest_discover_tests(lexus_head_unit_measurement_probe_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS hmi ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
)
