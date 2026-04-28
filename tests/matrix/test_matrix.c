#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#define TEST_CONFIG_MAIN
#include "test_matrix.h"

/* ------------------------------------------------------------------ tests_main */
int tests_main(void)
{
    run_matrix_core_tests();
    run_matrix_function_tests();
    run_matrix_function_regression_tests();
    run_matrix_fromstring_tests();
    run_matrix_tostring_tests();
    run_matrix_output_tests();
    run_matrix_readme_example();

    clear_matrix_input_context();
    return tests_failed;
}
