#include "test_matrix.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);
static bool test_matrix_suite_setup(void);
TEST_SUITE_SETUP(test_matrix_suite_setup);

static bool test_matrix_suite_setup(void)
{
    test_register_validity_checker("matrix-double-default", matrix_validity_contract_double_default());
    test_register_validity_checker("matrix-mp-real-default", matrix_validity_contract_mp_real_default());
    test_register_validity_checker("matrix-complex-default", matrix_validity_contract_complex_default());
    return TEST_REQUIRE_VALIDITY_CHECKER("matrix-double-default") &&
           TEST_REQUIRE_VALIDITY_CHECKER("matrix-mp-real-default") &&
           TEST_REQUIRE_VALIDITY_CHECKER("matrix-complex-default");
}

static int run_readme_example(void)
{
    number_t A_vals[4] = {num_create_from_long(2), num_create_from_string("1 + i"), num_create_from_string("1 - i"),
                          num_create_from_long(3)};
    matrix_t *A = mat_create(2, 2, A_vals);

    number_t eigenvalues[2] = {NUM_ZERO, NUM_ZERO};
    matrix_t *evecs = NULL;

    if (!A) {
        for (size_t i = 0; i < 4; ++i)
            num_destroy(&A_vals[i]);
        num_destroy(&eigenvalues[0]);
        num_destroy(&eigenvalues[1]);
        return 1;
    }

    if (mat_eigendecompose(A, eigenvalues, &evecs) != 0 || !evecs) {
        for (size_t i = 0; i < 4; ++i)
            num_destroy(&A_vals[i]);
        num_destroy(&eigenvalues[0]);
        num_destroy(&eigenvalues[1]);
        mat_free(A);
        mat_free(evecs);
        return 1;
    }

    num_printf("eigenvalue[0] = %N\n", eigenvalues[0]);
    num_printf("eigenvalue[1] = %N\n", eigenvalues[1]);

    num_destroy(&eigenvalues[0]);
    num_destroy(&eigenvalues[1]);
    for (size_t i = 0; i < 4; ++i)
        num_destroy(&A_vals[i]);
    mat_free(A);
    mat_free(evecs);
    return 0;
}

static int run_readme_string_quantum_example(void)
{
    mat_bindings_t *bindings = NULL;
    number_t delta = num_create_from_double(1.5);
    number_t omega = num_create_from_double(0.25);
    matrix_t *H = mat_from_string_expr("(@DELTA, @OMEGA; @OMEGA, -@DELTA)", &bindings);
    matrix_t *H2 = NULL;
    matrix_t *P = NULL;
    expr_t *evals[2] = {NULL, NULL};
    expr_t *trace = NULL;
    expr_t *c2 = NULL;
    char *trace_s = NULL;
    char *c2_s = NULL;
    char *eval0_s = NULL;
    char *eval1_s = NULL;

    if (!H)
        return 1;

    if (!mat_bindings_get(bindings, "@DELTA") || !mat_bindings_get(bindings, "@OMEGA")) {
        num_destroy(&omega);
        num_destroy(&delta);
        mat_bindings_free(bindings);
        mat_free(H);
        return 1;
    }

    expr_set_val(mat_bindings_get(bindings, "@DELTA"), delta);
    expr_set_val(mat_bindings_get(bindings, "@OMEGA"), omega);

    H2 = mat_pow_int(H, 2);
    P = mat_charpoly(H);
    if (mat_eigenvalues_expr(H, evals) != 0 || !H2 || !P) {
        num_destroy(&omega);
        num_destroy(&delta);
        for (size_t i = 0; i < 2; ++i)
            expr_free(evals[i]);
        mat_free(P);
        mat_free(H2);
        mat_bindings_free(bindings);
        mat_free(H);
        return 1;
    }

    if (mat_trace_expr(H, &trace) != 0) {
        num_destroy(&omega);
        num_destroy(&delta);
        for (size_t i = 0; i < 2; ++i)
            expr_free(evals[i]);
        mat_bindings_free(bindings);
        mat_free(P);
        mat_free(H2);
        mat_free(H);
        return 1;
    }
    mat_get(P, 2, 0, &c2);

    if (!trace || !c2 || !evals[0] || !evals[1]) {
        num_destroy(&omega);
        num_destroy(&delta);
        expr_free(trace);
        for (size_t i = 0; i < 2; ++i)
            expr_free(evals[i]);
        mat_bindings_free(bindings);
        mat_free(P);
        mat_free(H2);
        mat_free(H);
        return 1;
    }

    trace_s = expr_to_string(trace, style_EXPRESSION);
    c2_s = expr_to_string(c2, style_EXPRESSION);
    eval0_s = expr_to_string(evals[0], style_EXPRESSION);
    eval1_s = expr_to_string(evals[1], style_EXPRESSION);
    if (!trace_s || !c2_s || !eval0_s || !eval1_s) {
        free(trace_s);
        free(c2_s);
        free(eval0_s);
        free(eval1_s);
        expr_free(trace);
        for (size_t i = 0; i < 2; ++i)
            expr_free(evals[i]);
        num_destroy(&omega);
        num_destroy(&delta);
        mat_bindings_free(bindings);
        mat_free(P);
        mat_free(H2);
        mat_free(H);
        return 1;
    }

    mat_printf("H = %ml\n", H);
    mat_printf("H² = %m\n", H2);
    printf("tr(H) = %s\n", trace_s);
    printf("charpoly constant term = %s\n", c2_s);
    printf("eigenvalues = %s, %s\n", eval0_s, eval1_s);

    free(trace_s);
    free(c2_s);
    free(eval0_s);
    free(eval1_s);
    expr_free(trace);
    for (size_t i = 0; i < 2; ++i)
        expr_free(evals[i]);
    num_destroy(&omega);
    num_destroy(&delta);
    mat_bindings_free(bindings);
    mat_free(P);
    mat_free(H2);
    mat_free(H);
    return 0;
}

static void test_readme_example_hermitian_eigendecomposition(void)
{
    check_bool("README example 1 runs", run_readme_example() == 0);
}

static void test_readme_example_string_quantum(void)
{
    check_bool("README example 2 runs", run_readme_string_quantum_example() == 0);
}

/* ------------------------------------------------------------------ tests_main */
int tests_main(void)
{
    TEST_SECTION("Core");
    TEST_RUN_IN_GROUP(run_matrix_core_tests, tests, NULL);

    TEST_SECTION("Functions");
    TEST_RUN_IN_GROUP(run_matrix_function_tests, tests, NULL);
    TEST_RUN_IN_GROUP(run_matrix_function_regression_tests, tests, NULL);

    TEST_SECTION("String Parsing");
    TEST_RUN_IN_GROUP(run_matrix_fromstring_tests, tests, NULL);

    TEST_SECTION("String Formatting");
    TEST_RUN_IN_GROUP(run_matrix_tostring_tests, tests, NULL);

    TEST_SECTION("Output");
    TEST_RUN_IN_GROUP(run_matrix_output_tests, tests, NULL);

    TEST_SECTION("README");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_example_hermitian_eigendecomposition, readme_examples,
                                  "matrix,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_example_string_quantum, readme_examples, "matrix,readme,output");

    clear_matrix_input_context();
    return TESTS_EXIT_CODE();
}
