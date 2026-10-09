# The cantools oracle of REQ-005 (DN-027). DbcOracleGenerate runs tools/dbc_oracle.py with the
# test-only virtual environment's python3 (tools/setup_test_venv.sh); the comparison test needs
# its output. A missing environment makes DbcOracleGenerate fail, so it is never skipped quietly.
set(LEXUS_HEAD_UNIT_TEST_PYTHON "$ENV{HOME}/.venvs/lexus-car-device/bin/python3"
    CACHE FILEPATH "python3 of the test-only virtual environment (tools/setup_test_venv.sh)")
if(NOT EXISTS "${LEXUS_HEAD_UNIT_TEST_PYTHON}")
    message(WARNING "No test-only Python at ${LEXUS_HEAD_UNIT_TEST_PYTHON}; run "
                    "tools/setup_test_venv.sh, or the DBC oracle test will fail")
endif()

set(dbc_oracle_csv "${CMAKE_CURRENT_BINARY_DIR}/dbc_oracle.csv")

add_test(NAME DbcOracleGenerate
    COMMAND "${LEXUS_HEAD_UNIT_TEST_PYTHON}" -I "${CMAKE_SOURCE_DIR}/tools/dbc_oracle.py"
            --dbc "${CMAKE_SOURCE_DIR}/dbc/simulated_vehicle.dbc"
            --frames 10000 --seed 20261009 --output "${dbc_oracle_csv}"
)
set_tests_properties(DbcOracleGenerate PROPERTIES
    FIXTURES_SETUP dbc_oracle
    LABELS oracle
)

add_executable(lexus_head_unit_dbc_oracle_tests
    dbc_oracle_test.cpp
)

target_compile_definitions(lexus_head_unit_dbc_oracle_tests
    PRIVATE
        LEXUS_HEAD_UNIT_DBC_PATH="${CMAKE_SOURCE_DIR}/dbc/simulated_vehicle.dbc"
)

target_link_libraries(lexus_head_unit_dbc_oracle_tests
    PRIVATE
        lexus_head_unit_can
        lexus_head_unit_build_settings
        GTest::gtest_main
)

gtest_discover_tests(lexus_head_unit_dbc_oracle_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES
        FIXTURES_REQUIRED dbc_oracle
        LABELS oracle
        ENVIRONMENT "LEXUS_HEAD_UNIT_DBC_ORACLE_CSV=${dbc_oracle_csv}"
)
