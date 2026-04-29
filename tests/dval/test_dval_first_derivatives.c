#include "test_dval.h"

void test_deriv_const(void)
{
    dval_t *x  = dv_new_var_d(0.0);  /* dummy wrt — const ignores it */
    dval_t *c  = dv_new_const_d(5.0);
    dval_t *f  = c;
    dval_t *df = dv_create_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{5}", dv_eval_qf(dv_get_deriv(df, x)), qf_from_double(0.0));
    print_expr_of(df);

    dv_free(df);
    dv_free(c);
    dv_free(x);
}

void test_deriv_var(void)
{
    dval_t *x = dv_new_var_d(2.0);
    const dval_t *dx = dv_get_deriv(x, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x} | x=2", dv_eval_qf(dx), qf_from_double(1.0));
    print_expr_of(dx);

    dv_free(x);
}

void test_deriv_wrt_const_is_nan(void)
{
    dval_t *x = dv_new_named_var_d(2.0, "x");
    dval_t *c = dv_new_named_const_d(3.0, "c");
    dval_t *df = dv_create_deriv(x, c);
    const dval_t *borrowed = dv_get_deriv(x, c);

    if (!(df && qf_isnan(dv_eval_qf(df)))) {
        printf(C_RED "  FAIL: d/dc{x} via dv_create_deriv should be NaN" C_RESET "\n");
        TEST_FAIL();
    } else {
        printf(C_GREEN "  OK: d/dc{x} via dv_create_deriv is NaN" C_RESET "\n");
    }

    if (!(borrowed && qf_isnan(dv_eval_qf(borrowed)))) {
        printf(C_RED "  FAIL: d/dc{x} via dv_get_deriv should be NaN" C_RESET "\n");
        TEST_FAIL();
    } else {
        printf(C_GREEN "  OK: d/dc{x} via dv_get_deriv is NaN" C_RESET "\n");
    }

    dv_free(df);
    dv_free(c);
    dv_free(x);
}

void test_deriv_neg(void)
{
    dval_t *x = dv_new_var_d(3.0);
    dval_t *f = dv_neg(x);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{-x} | x=3", dv_eval_qf(df), qf_from_double(-1.0));
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_add_d(void)
{
    dval_t *x = dv_new_var_d(2.0);
    dval_t *f = dv_add_d(x, 5.0);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x+5} | x=2", dv_eval_qf(df), qf_from_double(1.0));
    print_expr_of(df);

    dv_free(x);
    dv_free(f);
}

void test_deriv_mul_d(void)
{
    dval_t *x = dv_new_var_d(4.0);
    dval_t *f = dv_mul_d(x, 7.0);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{7x} | x=4", dv_eval_qf(df), qf_from_double(7.0));
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_div_d(void)
{
    dval_t *x = dv_new_var_d(9.0);
    dval_t *f = dv_div_d(x, 3.0);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x/3} | x=9", dv_eval_qf(df), qf_div(qf_from_double(1.0), qf_from_double(3.0)));
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_x2(void)
{
    dval_t *x = dv_new_var_d(3.0);
    dval_t *f = dv_mul(x, x);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^2} | x=3", dv_eval_qf(df), qf_from_double(6.0));
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_pow3(void)
{
    dval_t *x = dv_new_var_d(2.0);
    dval_t *f = dv_pow_d(x, 3.0);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^3} | x=2", dv_eval_qf(df), qf_from_double(12.0));
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_pow_xy(void)
{
    dval_t *x = dv_new_var_d(2.0);

    dval_t *x2  = dv_mul(x, x);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *y   = dv_add(x2, one);

    dval_t *f   = dv_pow(x, y);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t X = qf_from_double(2.0);
    qfloat_t yval = qf_add(qf_mul(X, X), qf_from_double(1.0));
    qfloat_t fval = qf_pow(X, yval);

    qfloat_t term1 = qf_mul(qf_from_double(4.0), qf_log(X));
    qfloat_t term2 = qf_div(yval, qf_from_double(2.0));
    qfloat_t expect = qf_mul(fval, qf_add(term1, term2));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^(x^2+1)} | x=2", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(y);
    dv_free(one);
    dv_free(x2);
    dv_free(x);
}

void test_deriv_sin(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_sin(x);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sin(x)} | x=0.5", dv_eval_qf(df), qf_cos(qf_from_double(0.5)));
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_cos(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_cos(x);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{cos(x)} | x=0.5", dv_eval_qf(df), qf_neg(qf_sin(qf_from_double(0.5))));
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_tan(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_tan(x);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t X = qf_from_double(0.5);
    qfloat_t c = qf_cos(X);
    qfloat_t expect = qf_div(qf_from_double(1.0), qf_mul(c, c));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{tan(x)} | x=0.5", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_sinh(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_sinh(x);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sinh(x)} | x=0.5", dv_eval_qf(df), qf_cosh(qf_from_double(0.5)));
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_cosh(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_cosh(x);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{cosh(x)} | x=0.5", dv_eval_qf(df), qf_sinh(qf_from_double(0.5)));
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_tanh(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_tanh(x);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t X = qf_from_double(0.5);
    qfloat_t t = qf_tanh(X);
    qfloat_t expect = qf_sub(qf_from_double(1.0), qf_mul(t, t));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{tanh(x)} | x=0.5", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_asin(void)
{
    dval_t *x = dv_new_var_d(0.25);
    dval_t *f = dv_asin(x);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t expect = qf_div(qf_from_double(1.0), qf_sqrt(qf_sub(qf_from_double(1.0), qf_mul(X, X))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{asin(x)} | x=0.25", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_acos(void)
{
    dval_t *x = dv_new_var_d(0.25);
    dval_t *f = dv_acos(x);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t expect = qf_neg(qf_div(qf_from_double(1.0), qf_sqrt(qf_sub(qf_from_double(1.0), qf_mul(X, X)))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{acos(x)} | x=0.25", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_atan(void)
{
    dval_t *x = dv_new_var_d(0.25);
    dval_t *f = dv_atan(x);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t expect = qf_div(qf_from_double(1.0), qf_add(qf_from_double(1.0), qf_mul(X, X)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{atan(x)} | x=0.25", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_atan2(void)
{
    dval_t *x = dv_new_var_d(0.25);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *f = dv_atan2(x, one);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t expect = qf_div(qf_from_double(1.0), qf_add(qf_from_double(1.0), qf_mul(X, X)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{atan2(x,1)} | x=0.25", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(one);
    dv_free(x);
}

void test_deriv_asinh(void)
{
    dval_t *x = dv_new_var_d(0.25);
    dval_t *f = dv_asinh(x);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t expect = qf_div(qf_from_double(1.0), qf_sqrt(qf_add(qf_from_double(1.0), qf_mul(X, X))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{asinh(x)} | x=0.25", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_acosh(void)
{
    dval_t *x = dv_new_var_d(1.25);
    dval_t *f = dv_acosh(x);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t X = qf_from_double(1.25);
    qfloat_t expect = qf_div(qf_from_double(1.0), qf_mul(qf_sqrt(qf_sub(X, qf_from_double(1.0))),
                           qf_sqrt(qf_add(X, qf_from_double(1.0)))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{acosh(x)} | x=1.25", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_atanh(void)
{
    dval_t *x = dv_new_var_d(0.25);
    dval_t *f = dv_atanh(x);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t expect = qf_div(qf_from_double(1.0), qf_sub(qf_from_double(1.0), qf_mul(X, X)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{atanh(x)} | x=0.25", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_exp(void)
{
    dval_t *x = dv_new_var_d(1.5);
    dval_t *f = dv_exp(x);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{exp(x)} | x=1.5", dv_eval_qf(df), qf_exp(qf_from_double(1.5)));
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_log(void)
{
    dval_t *x = dv_new_var_d(2.0);
    dval_t *f = dv_log(x);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t expect = qf_div(qf_from_double(1.0), qf_from_double(2.0));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{log(x)} | x=2", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_sqrt(void)
{
    dval_t *x = dv_new_var_d(4.0);
    dval_t *f = dv_sqrt(x);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t expect = qf_div(qf_from_double(1.0), qf_mul(qf_from_double(2.0), qf_sqrt(qf_from_double(4.0))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sqrt(x)} | x=4", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_composite(void)
{
    dval_t *x  = dv_new_var_d(1.0);
    dval_t *sx = dv_sin(x);
    dval_t *ex = dv_exp(x);
    dval_t *f  = dv_mul(sx, ex);
    dv_free(sx);
    dv_free(ex);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t X = qf_from_double(1.0);
    qfloat_t expect = qf_add(qf_mul(qf_cos(X), qf_exp(X)), qf_mul(qf_sin(X), qf_exp(X)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sin(x)*exp(x)} | x=1", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_sin_log(void)
{
    dval_t *x  = dv_new_var(qf_from_string("1.3"));
    dval_t *sx = dv_sin(x);
    dval_t *lx = dv_log(x);
    dval_t *f  = dv_mul(sx, lx);
    dv_free(sx);
    dv_free(lx);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t X = qf_from_string("1.3");
    qfloat_t expect = qf_add(qf_mul(qf_cos(X), qf_log(X)), qf_mul(qf_sin(X), qf_div(qf_from_double(1.0), X)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sin(x)*log(x)} | x=1.3", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_exp_tanh(void)
{
    dval_t *x  = dv_new_var(qf_from_string("0.7"));
    dval_t *ex = dv_exp(x);
    dval_t *tx = dv_tanh(x);
    dval_t *f  = dv_mul(ex, tx);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t X = qf_from_string("0.7");
    qfloat_t t = qf_tanh(X);

    qfloat_t expect = qf_add(qf_mul(qf_exp(X), t), qf_mul(qf_exp(X), qf_sub(qf_from_double(1.0), qf_mul(t, t))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{exp(x)*tanh(x)} | x=0.7", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(ex);
    dv_free(tx);
    dv_free(f);
    dv_free(x);
}

void test_deriv_sqrt_sin_x2(void)
{
    dval_t *x  = dv_new_var(qf_from_string("1.1"));
    dval_t *x2 = dv_mul(x, x);
    dval_t *sqx  = dv_sqrt(x);
    dval_t *sx2  = dv_sin(x2);
    dval_t *f  = dv_mul(sqx, sx2);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t X  = qf_from_string("1.1");
    qfloat_t X2 = qf_mul(X, X);

    qfloat_t term1 = qf_mul(qf_div(qf_from_double(1.0), qf_mul(qf_from_double(2.0), qf_sqrt(X))), qf_sin(X2));

    qfloat_t term2 = qf_mul(qf_sqrt(X), qf_mul(qf_cos(X2), qf_mul(qf_from_double(2.0), X)));

    qfloat_t expect = qf_add(term1, term2);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sqrt(x)*sin(x^2)} | x=1.1", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(sqx);
    dv_free(sx2);
    dv_free(f);
    dv_free(x2);
    dv_free(x);
}

void test_deriv_log_cosh(void)
{
    dval_t *x  = dv_new_var(qf_from_string("0.9"));
    dval_t *cx = dv_cosh(x);
    dval_t *f  = dv_log(cx);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t X = qf_from_string("0.9");
    qfloat_t expect = qf_tanh(X);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{log(cosh(x))} | x=0.9", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(cx);
    dv_free(f);
    dv_free(x);
}

void test_deriv_x2_exp_negx(void)
{
    dval_t *x   = dv_new_var(qf_from_string("1.7"));
    dval_t *xm  = dv_neg(x);
    dval_t *ex  = dv_exp(xm);
    dval_t *x2  = dv_mul(x, x);
    dval_t *f   = dv_mul(x2, ex);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t X    = qf_from_string("1.7");
    qfloat_t e_mx = qf_exp(qf_neg(X));

    qfloat_t expect = qf_mul(e_mx, qf_add(qf_mul(qf_from_double(2.0), X), qf_mul(qf_from_double(-1.0), qf_mul(X, X))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^2*exp(-x)} | x=1.7", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x2);
    dv_free(ex);
    dv_free(xm);
    dv_free(x);
}

void test_deriv_atan_x_over_sqrt(void)
{
    dval_t *x   = dv_new_var(qf_from_string("0.8"));

    dval_t *x2  = dv_mul(x, x);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *sum = dv_add(one, x2);
    dval_t *den = dv_sqrt(sum);
    dval_t *g   = dv_div(one, den);

    dval_t *u   = dv_mul(x, g);
    dval_t *f   = dv_atan(u);
    const dval_t *df = dv_get_deriv(f, x);

    qfloat_t X = qf_from_string("0.8");

    qfloat_t expect = qf_div(qf_from_double(1.0), qf_mul(qf_sqrt(qf_add(qf_from_double(1.0), qf_mul(X, X))),
                           qf_add(qf_from_double(1.0), qf_mul(qf_from_double(2.0), qf_mul(X, X)))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{atan(x/sqrt(1+x^2))} | x=0.8", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(sum);
    dv_free(f);
    dv_free(u);
    dv_free(g);
    dv_free(den);
    dv_free(one);
    dv_free(x2);
    dv_free(x);
}

/* Special function first derivative tests */

void test_deriv_abs(void)
{
    dval_t *x  = dv_new_var(qf_from_string("0.8"));
    dval_t *f  = dv_abs(x);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/dx{|x|} = sign(x) = 1 at x=0.8 */
    check_q_at(__FILE__, __LINE__, 1, "d/dx{|x|} | x=0.8", dv_eval_qf(df), qf_from_double(1.0));
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_hypot(void)
{
    dval_t *x  = dv_new_var_d(3.0);
    dval_t *yc = dv_new_const_d(4.0);
    dval_t *f  = dv_hypot(x, yc);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/dx{hypot(x,4)} = x/hypot(x,4) = 3/5 at x=3 */
    qfloat_t X = qf_from_double(3.0);
    qfloat_t Y = qf_from_double(4.0);
    qfloat_t expect = qf_div(X, qf_hypot(X, Y));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{hypot(x,4)} | x=3", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(yc);
    dv_free(x);
}

void test_deriv_erf(void)
{
    dval_t *x  = dv_new_var_d(0.5);
    dval_t *f  = dv_erf(x);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/dx{erf(x)} = (2/sqrt(pi)) * exp(-x^2) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t expect = qf_mul(qf_div(qf_from_double(2.0), qf_sqrt(QF_PI)),
                           qf_exp(qf_neg(qf_mul(X, X))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{erf(x)} | x=0.5", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_erfc(void)
{
    dval_t *x  = dv_new_var_d(0.5);
    dval_t *f  = dv_erfc(x);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/dx{erfc(x)} = -(2/sqrt(pi)) * exp(-x^2) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t expect = qf_neg(qf_mul(qf_div(qf_from_double(2.0), qf_sqrt(QF_PI)),
                                  qf_exp(qf_neg(qf_mul(X, X)))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{erfc(x)} | x=0.5", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_erfinv(void)
{
    dval_t *x  = dv_new_var_d(0.5);
    dval_t *f  = dv_erfinv(x);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/dx{erfinv(x)} = sqrt(pi)/2 * exp(erfinv(x)^2) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t u = qf_erfinv(X);
    qfloat_t expect = qf_mul(qf_mul(qf_sqrt(QF_PI), qf_from_double(0.5)),
                           qf_exp(qf_mul(u, u)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{erfinv(x)} | x=0.5", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_erfcinv(void)
{
    dval_t *x  = dv_new_var_d(0.5);
    dval_t *f  = dv_erfcinv(x);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/dx{erfcinv(x)} = -sqrt(pi)/2 * exp(erfcinv(x)^2) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t v = qf_erfcinv(X);
    qfloat_t expect = qf_neg(qf_mul(qf_mul(qf_sqrt(QF_PI), qf_from_double(0.5)),
                                  qf_exp(qf_mul(v, v))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{erfcinv(x)} | x=0.5", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_gamma(void)
{
    dval_t *x  = dv_new_var_d(2.0);
    dval_t *f  = dv_gamma(x);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/dx{gamma(x)} = gamma(x) * digamma(x) */
    qfloat_t X = qf_from_double(2.0);
    qfloat_t expect = qf_mul(qf_gamma(X), qf_digamma(X));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{gamma(x)} | x=2", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_lgamma(void)
{
    dval_t *x  = dv_new_var_d(2.0);
    dval_t *f  = dv_lgamma(x);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/dx{lgamma(x)} = digamma(x) */
    qfloat_t X = qf_from_double(2.0);
    qfloat_t expect = qf_digamma(X);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{lgamma(x)} | x=2", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_digamma(void)
{
    dval_t *x  = dv_new_var_d(2.0);
    dval_t *f  = dv_digamma(x);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/dx{digamma(x)} = trigamma(x); trigamma(2) = pi^2/6 - 1 */
    qfloat_t pi2_over_6 = qf_div(qf_mul(QF_PI, QF_PI), qf_from_double(6.0));
    qfloat_t expect = qf_sub(pi2_over_6, qf_from_double(1.0));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{digamma(x)} | x=2", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_lambert_w0(void)
{
    dval_t *x  = dv_new_var_d(1.0);
    dval_t *f  = dv_lambert_w0(x);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/dx{W0(x)} = W0(x) / (x * (1 + W0(x))) */
    qfloat_t X = qf_from_double(1.0);
    qfloat_t w = qf_lambert_w0(X);
    qfloat_t expect = qf_div(w, qf_mul(X, qf_add(qf_from_double(1.0), w)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{W0(x)} | x=1", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_lambert_wm1(void)
{
    dval_t *x  = dv_new_var(qf_from_string("-0.1"));
    dval_t *f  = dv_lambert_wm1(x);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/dx{Wm1(x)} = Wm1(x) / (x * (1 + Wm1(x))) */
    qfloat_t X = qf_from_string("-0.1");
    qfloat_t w = qf_lambert_wm1(X);
    qfloat_t expect = qf_div(w, qf_mul(X, qf_add(qf_from_double(1.0), w)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{Wm1(x)} | x=-0.1", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_normal_pdf(void)
{
    dval_t *x  = dv_new_var_d(0.5);
    dval_t *f  = dv_normal_pdf(x);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/dx{phi(x)} = -x * phi(x) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t expect = qf_neg(qf_mul(X, qf_normal_pdf(X)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{phi(x)} | x=0.5", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_normal_cdf(void)
{
    dval_t *x  = dv_new_var_d(0.5);
    dval_t *f  = dv_normal_cdf(x);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/dx{Phi(x)} = phi(x) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t expect = qf_normal_pdf(X);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{Phi(x)} | x=0.5", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_normal_logpdf(void)
{
    dval_t *x  = dv_new_var_d(0.5);
    dval_t *f  = dv_normal_logpdf(x);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/dx{log phi(x)} = -x */
    qfloat_t expect = qf_from_double(-0.5);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{log phi(x)} | x=0.5", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_ei(void)
{
    dval_t *x  = dv_new_var_d(1.0);
    dval_t *f  = dv_ei(x);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/dx{Ei(x)} = exp(x)/x */
    qfloat_t X = qf_from_double(1.0);
    qfloat_t expect = qf_div(qf_exp(X), X);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{Ei(x)} | x=1", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_e1(void)
{
    dval_t *x  = dv_new_var_d(1.0);
    dval_t *f  = dv_e1(x);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/dx{E1(x)} = -exp(-x)/x */
    qfloat_t X = qf_from_double(1.0);
    qfloat_t expect = qf_neg(qf_div(qf_exp(qf_neg(X)), X));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{E1(x)} | x=1", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_beta(void)
{
    dval_t *x  = dv_new_var_d(2.0);
    dval_t *bc = dv_new_const_d(3.0);
    dval_t *f  = dv_beta(x, bc);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/da{beta(a,b)} = beta(a,b) * (digamma(a) - digamma(a+b)) */
    qfloat_t A = qf_from_double(2.0);
    qfloat_t B = qf_from_double(3.0);
    qfloat_t expect = qf_mul(qf_beta(A, B),
                           qf_sub(qf_digamma(A), qf_digamma(qf_add(A, B))));

    check_q_at(__FILE__, __LINE__, 1, "d/da{beta(a,3)} | a=2", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(bc);
    dv_free(x);
}

void test_deriv_logbeta(void)
{
    dval_t *x  = dv_new_var_d(2.0);
    dval_t *bc = dv_new_const_d(3.0);
    dval_t *f  = dv_logbeta(x, bc);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/da{logbeta(a,b)} = digamma(a) - digamma(a+b) */
    qfloat_t A = qf_from_double(2.0);
    qfloat_t B = qf_from_double(3.0);
    qfloat_t expect = qf_sub(qf_digamma(A), qf_digamma(qf_add(A, B)));

    check_q_at(__FILE__, __LINE__, 1, "d/da{logbeta(a,3)} | a=2", dv_eval_qf(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(bc);
    dv_free(x);
}

/* ------------------------------------------------------------------------- */
/* Second derivative tests                                                    */
/* ------------------------------------------------------------------------- */

void test_first_derivatives(void)
{
    RUN_TEST(test_deriv_const, __func__);
    RUN_TEST(test_deriv_var, __func__);
    RUN_TEST(test_deriv_wrt_const_is_nan, __func__);
    RUN_TEST(test_deriv_neg, __func__);
    RUN_TEST(test_deriv_add_d, __func__);
    RUN_TEST(test_deriv_mul_d, __func__);
    RUN_TEST(test_deriv_div_d, __func__);
    RUN_TEST(test_deriv_x2, __func__);
    RUN_TEST(test_deriv_pow3, __func__);
    RUN_TEST(test_deriv_pow_xy, __func__);
    RUN_TEST(test_deriv_sin, __func__);
    RUN_TEST(test_deriv_cos, __func__);
    RUN_TEST(test_deriv_tan, __func__);
    RUN_TEST(test_deriv_sinh, __func__);
    RUN_TEST(test_deriv_cosh, __func__);
    RUN_TEST(test_deriv_tanh, __func__);
    RUN_TEST(test_deriv_asin, __func__);
    RUN_TEST(test_deriv_acos, __func__);
    RUN_TEST(test_deriv_atan, __func__);
    RUN_TEST(test_deriv_atan2, __func__);
    RUN_TEST(test_deriv_asinh, __func__);
    RUN_TEST(test_deriv_acosh, __func__);
    RUN_TEST(test_deriv_atanh, __func__);
    RUN_TEST(test_deriv_exp, __func__);
    RUN_TEST(test_deriv_log, __func__);
    RUN_TEST(test_deriv_sqrt, __func__);
    RUN_TEST(test_deriv_composite, __func__);
    RUN_TEST(test_deriv_sin_log, __func__);
    RUN_TEST(test_deriv_exp_tanh, __func__);
    RUN_TEST(test_deriv_sqrt_sin_x2, __func__);
    RUN_TEST(test_deriv_log_cosh, __func__);
    RUN_TEST(test_deriv_x2_exp_negx, __func__);
    RUN_TEST(test_deriv_atan_x_over_sqrt, __func__);
    RUN_TEST(test_deriv_abs, __func__);
    RUN_TEST(test_deriv_hypot, __func__);
    RUN_TEST(test_deriv_erf, __func__);
    RUN_TEST(test_deriv_erfc, __func__);
    RUN_TEST(test_deriv_erfinv, __func__);
    RUN_TEST(test_deriv_erfcinv, __func__);
    RUN_TEST(test_deriv_gamma, __func__);
    RUN_TEST(test_deriv_lgamma, __func__);
    RUN_TEST(test_deriv_digamma, __func__);
    RUN_TEST(test_deriv_trigamma, __func__);
    RUN_TEST(test_deriv_lambert_w0, __func__);
    RUN_TEST(test_deriv_lambert_wm1, __func__);
    RUN_TEST(test_deriv_normal_pdf, __func__);
    RUN_TEST(test_deriv_normal_cdf, __func__);
    RUN_TEST(test_deriv_normal_logpdf, __func__);
    RUN_TEST(test_deriv_ei, __func__);
    RUN_TEST(test_deriv_e1, __func__);
    RUN_TEST(test_deriv_beta, __func__);
    RUN_TEST(test_deriv_logbeta, __func__);
}
