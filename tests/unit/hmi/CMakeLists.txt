add_executable(lexus_head_unit_view_model_tests
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
