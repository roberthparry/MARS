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
    printf(C_BOLD C_CYAN "=== Arithmetic ===\n" C_RESET);
    RUN_TEST(test_arithmetic, NULL);

    /* ---------------- _d variants ---------------- */
    printf(C_BOLD C_CYAN "=== _d variants ===\n" C_RESET);
    RUN_TEST(test_d_variants, NULL);

    /* ---------------- Math functions ------------- */
    printf(C_BOLD C_CYAN "=== Math functions ===\n" C_RESET);
    RUN_TEST(test_maths_functions, NULL);

    /* ---------------- First derivatives ---------- */
    printf(C_BOLD C_CYAN "=== First derivatives ===\n" C_RESET);
    RUN_TEST(test_first_derivatives, NULL);

    /* ---------------- Second derivatives --------- */
    printf(C_BOLD C_CYAN "=== Second derivatives ===\n" C_RESET);
    RUN_TEST(test_second_derivatives, NULL);

    printf(C_BOLD C_CYAN "=== dval_t to_string Tests ===\n" C_RESET);
    RUN_TEST(test_dval_t_to_string, NULL);

    printf(C_BOLD C_CYAN "=== dval_t from_string Tests ===\n" C_RESET);
    RUN_TEST(test_dval_t_from_string, NULL);

    printf(C_BOLD C_CYAN "=== Partial derivatives ===\n" C_RESET);
    RUN_TEST(test_partial_derivatives, NULL);

    printf(C_BOLD C_CYAN "=== dval_pattern helpers ===\n" C_RESET);
    RUN_TEST(test_dval_pattern_helpers, NULL);

    printf(C_BOLD C_CYAN "=== Runtime regressions ===\n" C_RESET);
    RUN_TEST(test_runtime_regressions, NULL);

    printf(C_BOLD C_CYAN "=== Reverse mode ===\n" C_RESET);
    RUN_TEST(test_reverse_mode, NULL);

    printf(C_BOLD C_CYAN "=== README.md example ===\n" C_RESET);
    RUN_TEST(test_README_md_example, NULL);

    return tests_failed;
}
