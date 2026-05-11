#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#define TEST_CONFIG_MAIN
#include "test_matrix.h"

static int run_readme_example(void)
{
    qcomplex_t A_vals[4] = {
        qc_make(qf_from_double(2.0), qf_from_double(0.0)),
        qc_make(qf_from_double(1.0), qf_from_double(1.0)),
        qc_make(qf_from_double(1.0), qf_from_double(-1.0)),
        qc_make(qf_from_double(3.0), qf_from_double(0.0))
    };
    matrix_t *A = mat_create_qc(2, 2, A_vals);

    qcomplex_t eigenvalues[2];
    matrix_t *evecs = NULL;

    if (!A)
        return 1;

    if (mat_eigendecompose(A, eigenvalues, &evecs) != 0 || !evecs)
    {
        mat_free(A);
        mat_free(evecs);
        return 1;
    }

    qc_printf("eigenvalue[0] = %z\n", eigenvalues[0]);
    qc_printf("eigenvalue[1] = %z\n", eigenvalues[1]);

    mat_free(A);
    mat_free(evecs);
    return 0;
}

static int run_readme_string_quantum_example(void)
{
    mat_bindings_t *bindings = NULL;
    number_t delta = num_create_from_double(1.5);
    number_t omega = num_create_from_double(0.25);
    matrix_t *H = mat_from_string(
        "(@DELTA, @OMEGA; @OMEGA, -@DELTA)",
        &bindings);
    matrix_t *H2 = NULL;
    matrix_t *P = NULL;
    dval_t *evals[2] = {NULL, NULL};
    dval_t *trace = NULL;
    dval_t *c2 = NULL;

    if (!H)
        return 1;

    if (!mat_bindings_get(bindings, "@DELTA") ||
        !mat_bindings_get(bindings, "@OMEGA")) {
        num_destroy(&omega);
        num_destroy(&delta);
        mat_bindings_free(bindings);
        mat_free(H);
        return 1;
    }

    dv_set_val_num(mat_bindings_get(bindings, "@DELTA"), delta);
    dv_set_val_num(mat_bindings_get(bindings, "@OMEGA"), omega);

    H2 = mat_pow_int(H, 2);
    P = mat_charpoly(H);
    if (mat_eigenvalues(H, evals) != 0 || !H2 || !P) {
        num_destroy(&omega);
        num_destroy(&delta);
        for (size_t i = 0; i < 2; ++i)
            dv_free(evals[i]);
        mat_free(P);
        mat_free(H2);
        mat_bindings_free(bindings);
        mat_free(H);
        return 1;
    }

    if (mat_trace(H, &trace) != 0) {
        num_destroy(&omega);
        num_destroy(&delta);
        for (size_t i = 0; i < 2; ++i)
            dv_free(evals[i]);
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
        dv_free(trace);
        for (size_t i = 0; i < 2; ++i)
            dv_free(evals[i]);
        mat_bindings_free(bindings);
        mat_free(P);
        mat_free(H2);
        mat_free(H);
        return 1;
    }

    mat_printf("H = %ml\n", H);
    mat_printf("H² = %m\n", H2);
    printf("tr(H) = %s\n", dv_to_string(trace, style_EXPRESSION));
    printf("charpoly constant term = %s\n", dv_to_string(c2, style_EXPRESSION));
    printf("eigenvalues = %s, %s\n",
           dv_to_string(evals[0], style_EXPRESSION),
           dv_to_string(evals[1], style_EXPRESSION));

    dv_free(trace);
    for (size_t i = 0; i < 2; ++i)
        dv_free(evals[i]);
    num_destroy(&omega);
    num_destroy(&delta);
    mat_bindings_free(bindings);
    mat_free(P);
    mat_free(H2);
    mat_free(H);
    return 0;
}

static void run_matrix_readme_example(void)
{
    printf(C_BOLD C_YELLOW "\n=== README Examples ===\n" C_RESET);
    printf(C_BOLD C_WHITE "\nexample 1:\n" C_RESET);
    (void)run_readme_example();
    printf("\n");
    printf(C_BOLD C_WHITE "example 2:\n" C_RESET);
    (void)run_readme_string_quantum_example();
    printf("\n");
}

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
    return TESTS_EXIT_CODE();
}
