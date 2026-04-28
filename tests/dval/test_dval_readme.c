#include "test_dval.h"

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

static void test_readme_example(void) {
    /* Create a named variable x with initial value 1.25 */
    dval_t *x = dv_new_named_var_d(1.25, "x");

    /* Build expression:
         f(x) = exp(sin(x)) + 3*x^2 - 7
    */
    dval_t *f = make_readme_f(x);

    /* First derivative (owning) */
    dval_t *df_dx = dv_create_deriv(f, x);   /* df/dx */

    /* Second derivative (borrowed) */
    const dval_t *d2f_dx = dv_get_deriv(df_dx, x);  /* d²f/dx² */

    /* Evaluate */
    qfloat_t f_val    = dv_eval_qf(f);
    qfloat_t d1_val   = dv_eval_qf(df_dx);
    qfloat_t d2_val   = dv_eval_qf(d2f_dx);

    /* Print symbolic forms */
    printf("f(x)    = "); dv_print(f);
    printf("f'(x)   = "); dv_print(df_dx);
    printf("f''(x)  = "); dv_print(d2f_dx);

    /* Print numeric results */
    qf_printf("\nAt x = 1.25:\n");
    qf_printf("f(x)    = %.34q\n", f_val);
    qf_printf("f'(x)   = %.34q\n", d1_val);
    qf_printf("f''(x)  = %.34q\n", d2_val);

    /* Free owning handles */
    dv_free(df_dx);
    dv_free(f);
    dv_free(x);
}

static void test_readme_from_string_example(void) {
    /* Build f(x) = exp(sin(x)) + 3*x^2 - 7 programmatically so we hold the
     * wrt pointer needed by the derivative API. */
    dval_t *x     = dv_new_named_var_d(1.25, "x");
    dval_t *sinx  = dv_sin(x);
    dval_t *esinx = dv_exp(sinx);
    dval_t *x2    = dv_pow_d(x, 2.0);
    dval_t *t     = dv_mul_d(x2, 3.0);
    dval_t *t2    = dv_sub_d(t, 7.0);
    dval_t *f     = dv_add(esinx, t2);

    /* First derivative (owning handle) */
    dval_t *df_dx = dv_create_deriv(f, x);

    /* Second derivative (borrowed — owned by df_dx, must not be freed) */
    const dval_t *d2f_dx = dv_get_deriv(df_dx, x);

    /* Evaluate */
    qfloat_t f_val  = dv_eval_qf(f);
    qfloat_t d1_val = dv_eval_qf(df_dx);
    qfloat_t d2_val = dv_eval_qf(d2f_dx);

    /* Print symbolic forms */
    printf("f(x)    = "); dv_print(f);
    printf("f'(x)   = "); dv_print(df_dx);
    printf("f''(x)  = "); dv_print(d2f_dx);

    /* Print numeric results */
    qf_printf("\nAt x = 1.25:\n");
    qf_printf("f(x)    = %.34q\n", f_val);
    qf_printf("f'(x)   = %.34q\n", d1_val);
    qf_printf("f''(x)  = %.34q\n", d2_val);

    dv_free(df_dx);
    dv_free(f);
    dv_free(t2); dv_free(t); dv_free(x2); dv_free(esinx); dv_free(sinx);
    dv_free(x);
}

static void test_readme_partial_example(void) {
    /* f(x, y) = x² + x·y + y²  — mirrors the "Partial Derivatives" example in dval.md */
    dval_t *x  = dv_new_named_var_d(1.0, "x");
    dval_t *y  = dv_new_named_var_d(2.0, "y");

    dval_t *x2 = dv_pow_d(x, 2.0);
    dval_t *xy = dv_mul(x, y);
    dval_t *y2 = dv_pow_d(y, 2.0);
    dval_t *t0 = dv_add(x2, xy);
    dval_t *f  = dv_add(t0, y2);

    dval_t *df_dx    = dv_create_deriv(f, x);
    dval_t *df_dy    = dv_create_deriv(f, y);
    dval_t *d2f_dxdy = dv_create_2nd_deriv(f, x, y);

    check_q_at(__FILE__, __LINE__, 1, "f(1,2)        = 7", dv_eval_qf(f),        qf_from_double(7.0));
    check_q_at(__FILE__, __LINE__, 1, "∂f/∂x(1,2)   = 4", dv_eval_qf(df_dx),    qf_from_double(4.0));
    check_q_at(__FILE__, __LINE__, 1, "∂f/∂y(1,2)   = 5", dv_eval_qf(df_dy),    qf_from_double(5.0));
    check_q_at(__FILE__, __LINE__, 1, "∂²f/∂x∂y     = 1", dv_eval_qf(d2f_dxdy), qf_from_double(1.0));

    /* borrowed pointer — same result, no extra free */
    const dval_t *p = dv_get_deriv(f, x);
    check_q_at(__FILE__, __LINE__, 1, "dv_get_deriv == dv_create_deriv", dv_eval_qf(p), qf_from_double(4.0));

    /* update x=3; cached partial graphs recompute automatically */
    dv_set_val_d(x, 3.0);
    check_q_at(__FILE__, __LINE__, 1, "∂f/∂x(3,2)   = 8", dv_eval_qf(df_dx), qf_from_double(8.0));
    check_q_at(__FILE__, __LINE__, 1, "∂f/∂y(3,2)   = 7", dv_eval_qf(df_dy), qf_from_double(7.0));

    dv_free(d2f_dxdy);
    dv_free(df_dy);
    dv_free(df_dx);
    dv_free(f); dv_free(t0); dv_free(y2); dv_free(xy); dv_free(x2);
    dv_free(y); dv_free(x);
}

void test_README_md_example(void) {
    RUN_TEST(test_readme_example,             __func__);
    RUN_TEST(test_readme_from_string_example, __func__);
    RUN_TEST(test_readme_partial_example,     __func__);
}

/* ------------------------------------------------------------------------- */
/* Partial derivative tests                                                  */
