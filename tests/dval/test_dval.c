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

static dval_t *make_readme_f(dval_t *x)
{
    dval_t *sinx   = dv_sin(x);
    dval_t *exp_sx = dv_exp(sinx);
    dval_t *x2     = dv_pow_d(x, 2.0);
    dval_t *term2  = dv_mul_d(x2, 3.0);
    dval_t *f0     = dv_add(exp_sx, term2);
    dval_t *f      = dv_sub_d(f0, 7.0);

    dv_free(sinx);
    dv_free(exp_sx);
    dv_free(x2);
    dv_free(term2);
    dv_free(f0);

    return f;
}

static int run_readme_example(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");
    dval_t *f;
    dval_t *df_dx;
    const dval_t *d2f_dx;
    qfloat_t f_val;
    qfloat_t d1_val;
    qfloat_t d2_val;

    if (!x)
        return 1;

    f = make_readme_f(x);
    if (!f) {
        dv_free(x);
        return 1;
    }

    df_dx = dv_create_deriv(f, x);
    if (!df_dx) {
        dv_free(f);
        dv_free(x);
        return 1;
    }

    d2f_dx = dv_get_deriv(df_dx, x);
    if (!d2f_dx) {
        dv_free(df_dx);
        dv_free(f);
        dv_free(x);
        return 1;
    }

    f_val  = dv_eval_qf(f);
    d1_val = dv_eval_qf(df_dx);
    d2_val = dv_eval_qf(d2f_dx);

    printf("f(x)    = "); dv_print(f);
    printf("f'(x)   = "); dv_print(df_dx);
    printf("f''(x)  = "); dv_print(d2f_dx);

    qf_printf("\nAt x = 1.25:\n");
    qf_printf("f(x)    = %.34q\n", f_val);
    qf_printf("f'(x)   = %.34q\n", d1_val);
    qf_printf("f''(x)  = %.34q\n", d2_val);

    dv_free(df_dx);
    dv_free(f);
    dv_free(x);
    return 0;
}

static int run_readme_from_string_example(void)
{
    dval_t *x     = dv_new_named_var_d(1.25, "x");
    dval_t *sinx  = dv_sin(x);
    dval_t *esinx = dv_exp(sinx);
    dval_t *x2    = dv_pow_d(x, 2.0);
    dval_t *t     = dv_mul_d(x2, 3.0);
    dval_t *t2    = dv_sub_d(t, 7.0);
    dval_t *f     = dv_add(esinx, t2);
    dval_t *df_dx;
    const dval_t *d2f_dx;
    qfloat_t f_val;
    qfloat_t d1_val;
    qfloat_t d2_val;

    if (!x || !sinx || !esinx || !x2 || !t || !t2 || !f) {
        dv_free(f);
        dv_free(t2); dv_free(t); dv_free(x2); dv_free(esinx); dv_free(sinx);
        dv_free(x);
        return 1;
    }

    df_dx = dv_create_deriv(f, x);
    if (!df_dx) {
        dv_free(f);
        dv_free(t2); dv_free(t); dv_free(x2); dv_free(esinx); dv_free(sinx);
        dv_free(x);
        return 1;
    }

    d2f_dx = dv_get_deriv(df_dx, x);
    if (!d2f_dx) {
        dv_free(df_dx);
        dv_free(f);
        dv_free(t2); dv_free(t); dv_free(x2); dv_free(esinx); dv_free(sinx);
        dv_free(x);
        return 1;
    }

    f_val  = dv_eval_qf(f);
    d1_val = dv_eval_qf(df_dx);
    d2_val = dv_eval_qf(d2f_dx);

    printf("f(x)    = "); dv_print(f);
    printf("f'(x)   = "); dv_print(df_dx);
    printf("f''(x)  = "); dv_print(d2f_dx);

    qf_printf("\nAt x = 1.25:\n");
    qf_printf("f(x)    = %.34q\n", f_val);
    qf_printf("f'(x)   = %.34q\n", d1_val);
    qf_printf("f''(x)  = %.34q\n", d2_val);

    dv_free(df_dx);
    dv_free(f);
    dv_free(t2); dv_free(t); dv_free(x2); dv_free(esinx); dv_free(sinx);
    dv_free(x);
    return 0;
}

static int run_readme_partial_example(void)
{
    dval_t *x  = dv_new_named_var_d(1.0, "x");
    dval_t *y  = dv_new_named_var_d(2.0, "y");
    dval_t *x2 = dv_pow_d(x, 2.0);
    dval_t *xy = dv_mul(x, y);
    dval_t *y2 = dv_pow_d(y, 2.0);
    dval_t *t0 = dv_add(x2, xy);
    dval_t *f  = dv_add(t0, y2);
    dval_t *df_dx;
    dval_t *df_dy;
    dval_t *d2f_dxdy;
    const dval_t *p;

    if (!x || !y || !x2 || !xy || !y2 || !t0 || !f) {
        dv_free(f); dv_free(t0); dv_free(y2); dv_free(xy); dv_free(x2);
        dv_free(y); dv_free(x);
        return 1;
    }

    df_dx    = dv_create_deriv(f, x);
    df_dy    = dv_create_deriv(f, y);
    d2f_dxdy = dv_create_2nd_deriv(f, x, y);
    if (!df_dx || !df_dy || !d2f_dxdy) {
        dv_free(d2f_dxdy);
        dv_free(df_dy);
        dv_free(df_dx);
        dv_free(f); dv_free(t0); dv_free(y2); dv_free(xy); dv_free(x2);
        dv_free(y); dv_free(x);
        return 1;
    }

    qf_printf("At x=1, y=2:\n");
    qf_printf("f          = %.34q\n", dv_eval_qf(f));
    qf_printf("∂f/∂x      = %.34q\n", dv_eval_qf(df_dx));
    qf_printf("∂f/∂y      = %.34q\n", dv_eval_qf(df_dy));
    qf_printf("∂²f/∂x∂y   = %.34q\n", dv_eval_qf(d2f_dxdy));

    p = dv_get_deriv(f, x);
    if (!p) {
        dv_free(d2f_dxdy);
        dv_free(df_dy);
        dv_free(df_dx);
        dv_free(f); dv_free(t0); dv_free(y2); dv_free(xy); dv_free(x2);
        dv_free(y); dv_free(x);
        return 1;
    }

    dv_set_val_d(x, 3.0);
    qf_printf("\nAfter x=3:\n");
    qf_printf("∂f/∂x      = %.34q\n", dv_eval_qf(df_dx));
    qf_printf("∂f/∂y      = %.34q\n", dv_eval_qf(df_dy));

    dv_free(d2f_dxdy);
    dv_free(df_dy);
    dv_free(df_dx);
    dv_free(f); dv_free(t0); dv_free(y2); dv_free(xy); dv_free(x2);
    dv_free(y); dv_free(x);
    return 0;
}

static int run_README_md_example(void)
{
    if (run_readme_example() != 0)
        return 1;
    putchar('\n');
    if (run_readme_from_string_example() != 0)
        return 1;
    putchar('\n');
    if (run_readme_partial_example() != 0)
        return 1;
    return 0;
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

    printf(C_YELLOW "\nRunning README examples...\n" C_RESET);
    run_README_md_example();

    return TESTS_EXIT_CODE();
}
