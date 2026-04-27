#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#define TEST_HARNESS_IMPLEMENTATION
#include "test_matrix.h"

/* ------------------------------------------------------------------ tests_main */
int tests_main(void)
{
    run_matrix_core_tests();
    run_matrix_function_tests();
    run_matrix_function_regression_tests();

    clear_matrix_input_context();
    return tests_failed;
}
