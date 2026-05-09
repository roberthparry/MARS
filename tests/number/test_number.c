#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#define TEST_CONFIG_MAIN
#include "test_number.h"

int tests_main(void)
{
    RUN_TEST_CASE(run_number_parse_tests);
    RUN_TEST_CASE(run_number_exact_backend_tests);
    RUN_TEST_CASE(run_number_fixed_precision_tests);
    RUN_TEST_CASE(run_number_multiprecision_tests);
    RUN_TEST_CASE(run_number_promotion_tests);
    RUN_TEST_CASE(run_number_constant_tests);
    RUN_TEST_CASE(run_number_formatting_tests);
    RUN_TEST_CASE(run_number_special_function_tests);
    RUN_TEST_CASE(run_number_public_api_tests);
    return 0;
}
