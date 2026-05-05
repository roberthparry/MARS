#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#define TEST_CONFIG_MAIN
#include "test_dval.h"

#pragma GCC diagnostic ignored "-Wunused-function"

/* ------------------------------------------------------------------------- */
/* Compact qfloat_t comparison (kept exactly as-is, but using harness colours) */
/* ------------------------------------------------------------------------- */

void check_q_at(const char *file, int line, int col,
                const char *label, qfloat_t got, qfloat_t expect)
{
    qfloat_t diff = qf_sub(got, expect);
    double abs_err = fabs(qf_to_double(diff));
    double exp_d   = fabs(qf_to_double(expect));

    double rel_err = (exp_d > 0)? abs_err / exp_d : abs_err;

    const double ABS_TOL = 2e-30;
    const double REL_TOL = 2e-30;

    if (abs_err < ABS_TOL || rel_err < REL_TOL) {
        printf("%s%sPASS%s %-32s  got=", C_BOLD, C_GREEN, C_RESET, label);
        qf_printf("%.34q", got);
        printf("\n");
        return;
    }

    printf("%s%sFAIL%s %s: %s:%d:%d: got=", C_BOLD, C_RED, C_RESET, label, file, line, col);
    TEST_FAIL();

    qf_printf("%.34q", got);

    printf(" expect=");
    qf_printf("%.34q", expect);

    printf(" diff=");
    qf_printf("%.34q", diff);

    printf("\n");

    /* integrate with harness */
    tests_failed++;
}

void print_expr_of(const dval_t *f)
{
    char *s = dv_to_string(f, style_EXPRESSION);
    printf(C_YELLOW "     = %s\n" C_RESET, s);
    free(s);
}

/* ------------------------------------------------------------------------- */
/* Arithmetic tests                                                          */
/* ------------------------------------------------------------------------- */

int tests_main(void)
{
    /* ---------------- Arithmetic ---------------- */
    TEST_SECTION("Arithmetic");
    RUN_TEST_CASE(test_arithmetic);

    /* ---------------- _d variants ---------------- */
    TEST_SECTION("_d variants");
    RUN_TEST_CASE(test_d_variants);

    /* ---------------- Math functions ------------- */
    TEST_SECTION("Math functions");
    RUN_TEST_CASE(test_maths_functions);

    /* ---------------- First derivatives ---------- */
    TEST_SECTION("First derivatives");
    RUN_TEST_CASE(test_first_derivatives);

    /* ---------------- Second derivatives --------- */
    TEST_SECTION("Second derivatives");
    RUN_TEST_CASE(test_second_derivatives);

    TEST_SECTION("dval_t to_string Tests");
    RUN_TEST_CASE(test_dval_t_to_string);

    TEST_SECTION("dval_t from_string Tests");
    RUN_TEST_CASE(test_dval_t_from_string);

    TEST_SECTION("Partial derivatives");
    RUN_TEST_CASE(test_partial_derivatives);

    TEST_SECTION("dval_pattern helpers");
    RUN_TEST_CASE(test_dval_pattern_helpers);

    TEST_SECTION("Runtime regressions");
    RUN_TEST_CASE(test_runtime_regressions);

    TEST_SECTION("Reverse mode");
    RUN_TEST_CASE(test_reverse_mode);

    TEST_SECTION("README.md example");
    RUN_TEST_CASE(test_README_md_example);

    return TESTS_EXIT_CODE();
}
