#include "test_number.h"

#include <stdio.h>

static bool test_number_suite_setup(void);

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);
TEST_SUITE_SETUP(test_number_suite_setup);

static bool test_number_suite_setup(void)
{
    test_register_validity_checker("number-exact",
                                   number_validity_contract_exact());
    return TEST_REQUIRE_VALIDITY_CHECKER("number-exact");
}

int tests_main(void)
{
    TEST_SECTION("Parsing");
    TEST_RUN_CASE(run_number_parse_tests, "number,parse");

    TEST_SECTION("Exact Backends");
    TEST_RUN_CASE(run_number_exact_backend_tests, "number,exact");

    TEST_SECTION("Fixed Precision");
    TEST_RUN_CASE(run_number_fixed_precision_tests, "number,fixed-precision");

    TEST_SECTION("Multiprecision");
    TEST_RUN_CASE(run_number_multiprecision_tests, "number,multiprecision");

    TEST_SECTION("Promotion");
    TEST_RUN_CASE(run_number_promotion_tests, "number,promotion");

    TEST_SECTION("Constants");
    TEST_RUN_CASE(run_number_constant_tests, "number,constants");

    TEST_SECTION("Formatting");
    TEST_RUN_CASE(run_number_formatting_tests, "number,formatting");

    TEST_SECTION("Special Functions");
    TEST_RUN_CASE(run_number_special_function_tests,
                  "number,special-functions");

    TEST_SECTION("Backend Parity");
    TEST_RUN_CASE(run_number_backend_parity_tests, "number,backend-parity");

    TEST_SECTION("Public API");
    TEST_RUN_CASE(run_number_public_api_tests, "number,public-api");

    /* README examples intentionally run last because they produce output. */
    TEST_SECTION("README Output Examples");
    printf(C_YELLOW "\nRunning number README-equivalent examples...\n" C_RESET);
    TEST_RUN_OUTPUT_TAGS(run_number_readme_example_tests,
                         "number,readme,output");
    TEST_RUN_OUTPUT_TAGS(run_number_readme_mersenne_prime_search,
                         "number,readme,mersenne,output");

    return TEST_EXIT_CODE();
}
