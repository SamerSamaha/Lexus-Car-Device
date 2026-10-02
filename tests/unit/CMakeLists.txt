# Unit tests (test tier T1): GoogleTest, one executable for all unit tests.

add_executable(lexus_head_unit_unit_tests
    service/service_library_smoke_test.cpp
)

target_link_libraries(lexus_head_unit_unit_tests
    PRIVATE
        lexus_head_unit_service
        lexus_head_unit_build_settings
        GTest::gtest_main
)

# Registers every TEST in the executable as its own ctest test. The list is
# read from the built executable, so a new TEST needs no change in this file.
include(GoogleTest)
gtest_discover_tests(lexus_head_unit_unit_tests
    DISCOVERY_TIMEOUT 30
)
