/**
 * @file test_number.c
 * @brief Generic number suite assembly and setup.
 *
 * Registers backend, exactness, precision, formatting and special-function groups under the shared test harness.
 * README examples are dispatched after the ordinary numeric checks.
 *
 * Used by the project test harness for regression verification. Select cases through tests/test_config.json and
 * run suites sequentially; this source is not part of the installed library.
 */

#include <stdio.h>

#include "test_number.h"

static bool test_number_suite_setup(void);

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);
TEST_SUITE_SETUP(test_number_suite_setup);

static bool test_number_suite_setup(void)
{
    test_register_validity_checker("number-exact", number_validity_contract_exact());
    return TEST_REQUIRE_VALIDITY_CHECKER("number-exact");
}

int tests_main(void)
{
    TEST_SECTION("Parsing");
    TEST_RUN_IN_GROUP(run_number_parse_tests, tests, "number,parse");

    TEST_SECTION("Exact Backends");
    TEST_RUN_IN_GROUP(run_number_exact_backend_tests, tests, "number,exact");

    TEST_SECTION("Fixed Precision");
    TEST_RUN_IN_GROUP(run_number_fixed_precision_tests, tests, "number,fixed-precision");

    TEST_SECTION("Multiprecision");
    TEST_RUN_IN_GROUP(run_number_multiprecision_tests, tests, "number,multiprecision");

    TEST_SECTION("Promotion");
    TEST_RUN_IN_GROUP(run_number_promotion_tests, tests, "number,promotion");

    TEST_SECTION("Constants");
    TEST_RUN_IN_GROUP(run_number_constant_tests, tests, "number,constants");

    TEST_SECTION("Formatting");
    TEST_RUN_IN_GROUP(run_number_formatting_tests, tests, "number,formatting");

    TEST_SECTION("Special Functions");
    TEST_RUN_IN_GROUP(run_number_special_function_tests, tests, "number,special-functions");
    TEST_RUN_IN_GROUP(run_number_bessel_k_ownership_tests, tests, "number,bessel-k,scope,ownership");
    TEST_RUN_IN_GROUP(run_number_bessel_y_qfloat_ownership_tests, tests, "number,bessel-y,qfloat,ownership");
    TEST_RUN_IN_GROUP(run_number_bessel_y_qcomplex_ownership_tests, tests, "number,bessel-y,qcomplex,ownership");
    TEST_RUN_IN_GROUP(run_number_bessel_y_scope_lifetime_tests, tests, "number,bessel-y,scope,ownership");
    TEST_RUN_IN_GROUP(run_number_nonfinite_series_tests, tests, "number,special-functions,nonfinite");

    TEST_SECTION("Backend Parity");
    TEST_RUN_IN_GROUP(run_number_backend_parity_tests, tests, "number,backend-parity");

    TEST_SECTION("Public API");
    TEST_RUN_IN_GROUP(run_number_public_api_tests, tests, "number,public-api");

    /* README examples intentionally run last because they produce output. */
    TEST_SECTION("README Output Examples");
    printf(C_YELLOW "\nRunning number README-equivalent examples...\n" C_RESET);
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(run_number_readme_example_tests, readme_examples, "number,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(run_number_readme_mersenne_prime_search, readme_examples,
                                  "number,readme,mersenne,output");

    return TEST_EXIT_CODE();
}
