#include "test_expr.h"

void test_deriv_const(void)
{
    expr_t *x = test_expr_new_var_d(0.0); /* dummy wrt — const ignores it */
    expr_t *c = test_expr_new_const_d(5.0);
    expr_t *f = c;
    expr_t *df = expr_create_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{5}", expr_eval_qf(expr_get_deriv(df, x)), qf_from_double(0.0));
    print_expr_of(df);

    expr_free(df);
    expr_free(c);
    expr_free(x);
}

void test_deriv_var(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    const expr_t *dx = expr_get_deriv(x, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x} | x=2", expr_eval_qf(dx), qf_from_double(1.0));
    print_expr_of(dx);

    expr_free(x);
}

void test_deriv_wrt_const_is_nan(void)
{
    expr_t *x = test_expr_new_named_var_d(2.0, "x");
    expr_t *c = test_expr_new_named_const_d(3.0, "c");
    expr_t *df = expr_create_deriv(x, c);
    const expr_t *borrowed = expr_get_deriv(x, c);

    if (!(df && qf_isnan(expr_eval_qf(df)))) {
        printf(C_RED "  FAIL: d/dc{x} via expr_create_deriv should be NaN" C_RESET "\n");
        TEST_FAIL();
    } else {
        printf(C_GREEN "  OK: d/dc{x} via expr_create_deriv is NaN" C_RESET "\n");
    }

    if (!(borrowed && qf_isnan(expr_eval_qf(borrowed)))) {
        printf(C_RED "  FAIL: d/dc{x} via expr_get_deriv should be NaN" C_RESET "\n");
        TEST_FAIL();
    } else {
        printf(C_GREEN "  OK: d/dc{x} via expr_get_deriv is NaN" C_RESET "\n");
    }

    expr_free(df);
    expr_free(c);
    expr_free(x);
}

void test_deriv_neg(void)
{
    expr_t *x = test_expr_new_var_d(3.0);
    expr_t *f = expr_neg(x);
    const expr_t *df = expr_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{-x} | x=3", expr_eval_qf(df), qf_from_double(-1.0));
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_add_d(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    expr_t *f = expr_add_d(x, 5.0);
    const expr_t *df = expr_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x+5} | x=2", expr_eval_qf(df), qf_from_double(1.0));
    print_expr_of(df);

    expr_free(x);
    expr_free(f);
}

void test_deriv_mul_d(void)
{
    expr_t *x = test_expr_new_var_d(4.0);
    expr_t *f = expr_mul_d(x, 7.0);
    const expr_t *df = expr_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{7x} | x=4", expr_eval_qf(df), qf_from_double(7.0));
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_div_d(void)
{
    expr_t *x = test_expr_new_var_d(9.0);
    expr_t *f = expr_div_d(x, 3.0);
    const expr_t *df = expr_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x/3} | x=9", expr_eval_qf(df),
               qf_div(qf_from_double(1.0), qf_from_double(3.0)));
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_x2(void)
{
    expr_t *x = test_expr_new_var_d(3.0);
    expr_t *f = expr_mul(x, x);
    const expr_t *df = expr_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^2} | x=3", expr_eval_qf(df), qf_from_double(6.0));
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_pow3(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    expr_t *f = expr_pow_d(x, 3.0);
    const expr_t *df = expr_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^3} | x=2", expr_eval_qf(df), qf_from_double(12.0));
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_pow_xy(void)
{
    expr_t *x = test_expr_new_var_d(2.0);

    expr_t *x2 = expr_mul(x, x);
    expr_t *one = test_expr_new_const_d(1.0);
    expr_t *y = expr_add(x2, one);

    expr_t *f = expr_pow_xp(x, y);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t X = qf_from_double(2.0);
    qfloat_t yval = qf_add(qf_mul(X, X), qf_from_double(1.0));
    qfloat_t fval = qf_pow(X, yval);

    qfloat_t term1 = qf_mul(qf_from_double(4.0), qf_log(X));
    qfloat_t term2 = qf_div(yval, qf_from_double(2.0));
    qfloat_t expect = qf_mul(fval, qf_add(term1, term2));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^(x^2+1)} | x=2", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(y);
    expr_free(one);
    expr_free(x2);
    expr_free(x);
}

void test_deriv_sin(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_sin(x);
    const expr_t *df = expr_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sin(x)} | x=0.5", expr_eval_qf(df), qf_cos(qf_from_double(0.5)));
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_cos(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_cos(x);
    const expr_t *df = expr_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{cos(x)} | x=0.5", expr_eval_qf(df), qf_neg(qf_sin(qf_from_double(0.5))));
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_tan(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_tan(x);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t X = qf_from_double(0.5);
    qfloat_t c = qf_cos(X);
    qfloat_t expect = qf_div(qf_from_double(1.0), qf_mul(c, c));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{tan(x)} | x=0.5", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_sinh(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_sinh(x);
    const expr_t *df = expr_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sinh(x)} | x=0.5", expr_eval_qf(df), qf_cosh(qf_from_double(0.5)));
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_cosh(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_cosh(x);
    const expr_t *df = expr_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{cosh(x)} | x=0.5", expr_eval_qf(df), qf_sinh(qf_from_double(0.5)));
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_tanh(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_tanh(x);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t X = qf_from_double(0.5);
    qfloat_t t = qf_tanh(X);
    qfloat_t expect = qf_sub(qf_from_double(1.0), qf_mul(t, t));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{tanh(x)} | x=0.5", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_asin(void)
{
    expr_t *x = test_expr_new_var_d(0.25);
    expr_t *f = expr_asin(x);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t expect = qf_div(qf_from_double(1.0), qf_sqrt(qf_sub(qf_from_double(1.0), qf_mul(X, X))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{asin(x)} | x=0.25", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_acos(void)
{
    expr_t *x = test_expr_new_var_d(0.25);
    expr_t *f = expr_acos(x);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t expect = qf_neg(qf_div(qf_from_double(1.0), qf_sqrt(qf_sub(qf_from_double(1.0), qf_mul(X, X)))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{acos(x)} | x=0.25", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_atan(void)
{
    expr_t *x = test_expr_new_var_d(0.25);
    expr_t *f = expr_atan(x);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t expect = qf_div(qf_from_double(1.0), qf_add(qf_from_double(1.0), qf_mul(X, X)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{atan(x)} | x=0.25", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_atan2(void)
{
    expr_t *x = test_expr_new_var_d(0.25);
    expr_t *one = test_expr_new_const_d(1.0);
    expr_t *f = expr_atan2(x, one);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t expect = qf_div(qf_from_double(1.0), qf_add(qf_from_double(1.0), qf_mul(X, X)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{atan2(x,1)} | x=0.25", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(one);
    expr_free(x);
}

void test_deriv_asinh(void)
{
    expr_t *x = test_expr_new_var_d(0.25);
    expr_t *f = expr_asinh(x);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t expect = qf_div(qf_from_double(1.0), qf_sqrt(qf_add(qf_from_double(1.0), qf_mul(X, X))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{asinh(x)} | x=0.25", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_acosh(void)
{
    expr_t *x = test_expr_new_var_d(1.25);
    expr_t *f = expr_acosh(x);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t X = qf_from_double(1.25);
    qfloat_t expect = qf_div(qf_from_double(1.0),
                             qf_mul(qf_sqrt(qf_sub(X, qf_from_double(1.0))), qf_sqrt(qf_add(X, qf_from_double(1.0)))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{acosh(x)} | x=1.25", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_atanh(void)
{
    expr_t *x = test_expr_new_var_d(0.25);
    expr_t *f = expr_atanh(x);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t expect = qf_div(qf_from_double(1.0), qf_sub(qf_from_double(1.0), qf_mul(X, X)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{atanh(x)} | x=0.25", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_exp(void)
{
    expr_t *x = test_expr_new_var_d(1.5);
    expr_t *f = expr_exp(x);
    const expr_t *df = expr_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{exp(x)} | x=1.5", expr_eval_qf(df), qf_exp(qf_from_double(1.5)));
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_log(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    expr_t *f = expr_log(x);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t expect = qf_div(qf_from_double(1.0), qf_from_double(2.0));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{log(x)} | x=2", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_log10(void)
{
    expr_t *x = test_expr_new_var_d(10.0);
    expr_t *f = expr_log10(x);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t expect = qf_div(qf_from_double(1.0), qf_mul(qf_from_double(10.0), QF_LN10));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{log10(x)} | x=10", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_sqrt(void)
{
    expr_t *x = test_expr_new_var_d(4.0);
    expr_t *f = expr_sqrt(x);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t expect = qf_div(qf_from_double(1.0), qf_mul(qf_from_double(2.0), qf_sqrt(qf_from_double(4.0))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sqrt(x)} | x=4", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_floor_and_ceil(void)
{
    expr_t *x = test_expr_new_var_d(1.25);
    expr_t *floor_x = expr_floor(x);
    expr_t *ceil_x = expr_ceil(x);
    const expr_t *dfloor = expr_get_deriv(floor_x, x);
    const expr_t *dceil = expr_get_deriv(ceil_x, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{floor(x)} | x=1.25", expr_eval_qf(dfloor), qf_from_double(0.0));
    check_q_at(__FILE__, __LINE__, 1, "d/dx{ceil(x)} | x=1.25", expr_eval_qf(dceil), qf_from_double(0.0));

    expr_free(ceil_x);
    expr_free(floor_x);
    expr_free(x);
}

void test_deriv_composite(void)
{
    expr_t *x = test_expr_new_var_d(1.0);
    expr_t *sx = expr_sin(x);
    expr_t *ex = expr_exp(x);
    expr_t *f = expr_mul(sx, ex);
    expr_free(sx);
    expr_free(ex);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t X = qf_from_double(1.0);
    qfloat_t expect = qf_add(qf_mul(qf_cos(X), qf_exp(X)), qf_mul(qf_sin(X), qf_exp(X)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sin(x)*exp(x)} | x=1", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_sin_log(void)
{
    expr_t *x = test_expr_new_var_qf(qf_from_string("1.3"));
    expr_t *sx = expr_sin(x);
    expr_t *lx = expr_log(x);
    expr_t *f = expr_mul(sx, lx);
    expr_free(sx);
    expr_free(lx);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t X = qf_from_string("1.3");
    qfloat_t expect = qf_add(qf_mul(qf_cos(X), qf_log(X)), qf_mul(qf_sin(X), qf_div(qf_from_double(1.0), X)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sin(x)*log(x)} | x=1.3", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_exp_tanh(void)
{
    expr_t *x = test_expr_new_var_qf(qf_from_string("0.7"));
    expr_t *ex = expr_exp(x);
    expr_t *tx = expr_tanh(x);
    expr_t *f = expr_mul(ex, tx);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t X = qf_from_string("0.7");
    qfloat_t t = qf_tanh(X);

    qfloat_t expect = qf_add(qf_mul(qf_exp(X), t), qf_mul(qf_exp(X), qf_sub(qf_from_double(1.0), qf_mul(t, t))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{exp(x)*tanh(x)} | x=0.7", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(ex);
    expr_free(tx);
    expr_free(f);
    expr_free(x);
}

void test_deriv_sqrt_sin_x2(void)
{
    expr_t *x = test_expr_new_var_qf(qf_from_string("1.1"));
    expr_t *x2 = expr_mul(x, x);
    expr_t *sqx = expr_sqrt(x);
    expr_t *sx2 = expr_sin(x2);
    expr_t *f = expr_mul(sqx, sx2);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t X = qf_from_string("1.1");
    qfloat_t X2 = qf_mul(X, X);

    qfloat_t term1 = qf_mul(qf_div(qf_from_double(1.0), qf_mul(qf_from_double(2.0), qf_sqrt(X))), qf_sin(X2));

    qfloat_t term2 = qf_mul(qf_sqrt(X), qf_mul(qf_cos(X2), qf_mul(qf_from_double(2.0), X)));

    qfloat_t expect = qf_add(term1, term2);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sqrt(x)*sin(x^2)} | x=1.1", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(sqx);
    expr_free(sx2);
    expr_free(f);
    expr_free(x2);
    expr_free(x);
}

void test_deriv_log_cosh(void)
{
    expr_t *x = test_expr_new_var_qf(qf_from_string("0.9"));
    expr_t *cx = expr_cosh(x);
    expr_t *f = expr_log(cx);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t X = qf_from_string("0.9");
    qfloat_t expect = qf_tanh(X);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{log(cosh(x))} | x=0.9", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(cx);
    expr_free(f);
    expr_free(x);
}

void test_deriv_x2_exp_negx(void)
{
    expr_t *x = test_expr_new_var_qf(qf_from_string("1.7"));
    expr_t *xm = expr_neg(x);
    expr_t *ex = expr_exp(xm);
    expr_t *x2 = expr_mul(x, x);
    expr_t *f = expr_mul(x2, ex);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t X = qf_from_string("1.7");
    qfloat_t e_mx = qf_exp(qf_neg(X));

    qfloat_t expect = qf_mul(e_mx, qf_add(qf_mul(qf_from_double(2.0), X), qf_mul(qf_from_double(-1.0), qf_mul(X, X))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^2*exp(-x)} | x=1.7", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x2);
    expr_free(ex);
    expr_free(xm);
    expr_free(x);
}

void test_deriv_atan_x_over_sqrt(void)
{
    expr_t *x = test_expr_new_var_qf(qf_from_string("0.8"));

    expr_t *x2 = expr_mul(x, x);
    expr_t *one = test_expr_new_const_d(1.0);
    expr_t *sum = expr_add(one, x2);
    expr_t *den = expr_sqrt(sum);
    expr_t *g = expr_div(one, den);

    expr_t *u = expr_mul(x, g);
    expr_t *f = expr_atan(u);
    const expr_t *df = expr_get_deriv(f, x);

    qfloat_t X = qf_from_string("0.8");

    qfloat_t expect =
        qf_div(qf_from_double(1.0), qf_mul(qf_sqrt(qf_add(qf_from_double(1.0), qf_mul(X, X))),
                                           qf_add(qf_from_double(1.0), qf_mul(qf_from_double(2.0), qf_mul(X, X)))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{atan(x/sqrt(1+x^2))} | x=0.8", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(sum);
    expr_free(f);
    expr_free(u);
    expr_free(g);
    expr_free(den);
    expr_free(one);
    expr_free(x2);
    expr_free(x);
}

/* Special function first derivative tests */

void test_deriv_abs(void)
{
    expr_t *x = test_expr_new_var_qf(qf_from_string("0.8"));
    expr_t *f = expr_abs(x);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{|x|} = sign(x) = 1 at x=0.8 */
    check_q_at(__FILE__, __LINE__, 1, "d/dx{|x|} | x=0.8", expr_eval_qf(df), qf_from_double(1.0));
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_hypot(void)
{
    expr_t *x = test_expr_new_var_d(3.0);
    expr_t *yc = test_expr_new_const_d(4.0);
    expr_t *f = expr_hypot(x, yc);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{hypot(x,4)} = x/hypot(x,4) = 3/5 at x=3 */
    qfloat_t X = qf_from_double(3.0);
    qfloat_t Y = qf_from_double(4.0);
    qfloat_t expect = qf_div(X, qf_hypot(X, Y));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{hypot(x,4)} | x=3", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(yc);
    expr_free(x);
}

void test_deriv_erf(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_erf(x);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{erf(x)} = (2/sqrt(pi)) * exp(-x^2) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t expect = qf_mul(qf_div(qf_from_double(2.0), qf_sqrt(QF_PI)), qf_exp(qf_neg(qf_mul(X, X))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{erf(x)} | x=0.5", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_erfc(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_erfc(x);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{erfc(x)} = -(2/sqrt(pi)) * exp(-x^2) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t expect = qf_neg(qf_mul(qf_div(qf_from_double(2.0), qf_sqrt(QF_PI)), qf_exp(qf_neg(qf_mul(X, X)))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{erfc(x)} | x=0.5", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_erfinv(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_erfinv(x);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{erfinv(x)} = sqrt(pi)/2 * exp(erfinv(x)^2) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t u = qf_erfinv(X);
    qfloat_t expect = qf_mul(qf_mul(qf_sqrt(QF_PI), qf_from_double(0.5)), qf_exp(qf_mul(u, u)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{erfinv(x)} | x=0.5", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_erfcinv(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_erfcinv(x);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{erfcinv(x)} = -sqrt(pi)/2 * exp(erfcinv(x)^2) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t v = qf_erfcinv(X);
    qfloat_t expect = qf_neg(qf_mul(qf_mul(qf_sqrt(QF_PI), qf_from_double(0.5)), qf_exp(qf_mul(v, v))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{erfcinv(x)} | x=0.5", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_gamma(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    expr_t *f = expr_gamma(x);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{gamma(x)} = gamma(x) * digamma(x) */
    qfloat_t X = qf_from_double(2.0);
    qfloat_t expect = qf_mul(qf_gamma(X), qf_digamma(X));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{gamma(x)} | x=2", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_lgamma(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    expr_t *f = expr_lgamma(x);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{lgamma(x)} = digamma(x) */
    qfloat_t X = qf_from_double(2.0);
    qfloat_t expect = qf_digamma(X);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{lgamma(x)} | x=2", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_digamma(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    expr_t *f = expr_digamma(x);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{digamma(x)} = trigamma(x); trigamma(2) = pi^2/6 - 1 */
    qfloat_t pi2_over_6 = qf_div(qf_mul(QF_PI, QF_PI), qf_from_double(6.0));
    qfloat_t expect = qf_sub(pi2_over_6, qf_from_double(1.0));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{digamma(x)} | x=2", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_gammainv(void)
{
    qfloat_t X = qf_gamma(qf_from_double(2.5));
    expr_t *x = test_expr_new_var_qf(X);
    expr_t *f = expr_gammainv(x);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{gammainv(x)} = 1 / (x * digamma(gammainv(x))) */
    qfloat_t y = qf_gammainv(X);
    qfloat_t expect = qf_div(qf_from_double(1.0), qf_mul(X, qf_digamma(y)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{gammainv(x)} | x=gamma(2.5)", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_lambert_w0(void)
{
    expr_t *x = test_expr_new_var_d(1.0);
    expr_t *f = expr_lambert_w0(x);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{W0(x)} = W0(x) / (x * (1 + W0(x))) */
    qfloat_t X = qf_from_double(1.0);
    qfloat_t w = qf_lambert_w0(X);
    qfloat_t expect = qf_div(w, qf_mul(X, qf_add(qf_from_double(1.0), w)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{W0(x)} | x=1", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_lambert_wm1(void)
{
    expr_t *x = test_expr_new_var_qf(qf_from_string("-0.1"));
    expr_t *f = expr_lambert_wm1(x);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{Wm1(x)} = Wm1(x) / (x * (1 + Wm1(x))) */
    qfloat_t X = qf_from_string("-0.1");
    qfloat_t w = qf_lambert_wm1(X);
    qfloat_t expect = qf_div(w, qf_mul(X, qf_add(qf_from_double(1.0), w)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{Wm1(x)} | x=-0.1", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_normal_pdf(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_normal_pdf(x);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{phi(x)} = -x * phi(x) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t expect = qf_neg(qf_mul(X, qf_normal_pdf(X)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{phi(x)} | x=0.5", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_normal_cdf(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_normal_cdf(x);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{Phi(x)} = phi(x) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t expect = qf_normal_pdf(X);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{Phi(x)} | x=0.5", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_normal_logpdf(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_normal_logpdf(x);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{log phi(x)} = -x */
    qfloat_t expect = qf_from_double(-0.5);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{log phi(x)} | x=0.5", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_ei(void)
{
    expr_t *x = test_expr_new_var_d(1.0);
    expr_t *f = expr_ei(x);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{Ei(x)} = exp(x)/x */
    qfloat_t X = qf_from_double(1.0);
    qfloat_t expect = qf_div(qf_exp(X), X);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{Ei(x)} | x=1", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_e1(void)
{
    expr_t *x = test_expr_new_var_d(1.0);
    expr_t *f = expr_e1(x);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{E1(x)} = -exp(-x)/x */
    qfloat_t X = qf_from_double(1.0);
    qfloat_t expect = qf_neg(qf_div(qf_exp(qf_neg(X)), X));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{E1(x)} | x=1", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(x);
}

void test_deriv_beta(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    expr_t *bc = test_expr_new_const_d(3.0);
    expr_t *f = expr_beta(x, bc);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/da{beta(a,b)} = beta(a,b) * (digamma(a) - digamma(a+b)) */
    qfloat_t A = qf_from_double(2.0);
    qfloat_t B = qf_from_double(3.0);
    qfloat_t expect = qf_mul(qf_beta(A, B), qf_sub(qf_digamma(A), qf_digamma(qf_add(A, B))));

    check_q_at(__FILE__, __LINE__, 1, "d/da{beta(a,3)} | a=2", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(bc);
    expr_free(x);
}

void test_deriv_logbeta(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    expr_t *bc = test_expr_new_const_d(3.0);
    expr_t *f = expr_logbeta(x, bc);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/da{logbeta(a,b)} = digamma(a) - digamma(a+b) */
    qfloat_t A = qf_from_double(2.0);
    qfloat_t B = qf_from_double(3.0);
    qfloat_t expect = qf_sub(qf_digamma(A), qf_digamma(qf_add(A, B)));

    check_q_at(__FILE__, __LINE__, 1, "d/da{logbeta(a,3)} | a=2", expr_eval_qf(df), expect);
    print_expr_of(df);

    expr_free(f);
    expr_free(bc);
    expr_free(x);
}

void test_deriv_gammainc(void)
{
    expr_t *s = test_expr_new_const_d(1.0);
    expr_t *x = test_expr_new_var_d(1.0);
    expr_t *lower = expr_gammainc_lower(s, x);
    expr_t *upper = expr_gammainc_upper(s, x);
    expr_t *P = expr_gammainc_P(s, x);
    expr_t *Q = expr_gammainc_Q(s, x);
    qfloat_t exp_neg_one = qf_exp(qf_neg(qf_from_double(1.0)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{gammainc_lower(1,x)} | x=1", expr_eval_qf(expr_get_deriv(lower, x)),
               exp_neg_one);
    check_q_at(__FILE__, __LINE__, 1, "d/dx{gammainc_upper(1,x)} | x=1", expr_eval_qf(expr_get_deriv(upper, x)),
               qf_neg(exp_neg_one));
    check_q_at(__FILE__, __LINE__, 1, "d/dx{gammainc_P(1,x)} | x=1", expr_eval_qf(expr_get_deriv(P, x)), exp_neg_one);
    check_q_at(__FILE__, __LINE__, 1, "d/dx{gammainc_Q(1,x)} | x=1", expr_eval_qf(expr_get_deriv(Q, x)),
               qf_neg(exp_neg_one));

    expr_free(Q);
    expr_free(P);
    expr_free(upper);
    expr_free(lower);
    expr_free(x);
    expr_free(s);
}

void test_deriv_beta_pdf(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *a = test_expr_new_const_d(2.0);
    expr_t *b = test_expr_new_const_d(3.0);
    expr_t *f = expr_beta_pdf(x, a, b);
    const expr_t *df = expr_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{beta_pdf(x,2,3)} | x=0.5", expr_eval_qf(df), qf_from_double(-3.0));
    print_expr_of(df);

    expr_free(f);
    expr_free(b);
    expr_free(a);
    expr_free(x);
}

void test_deriv_logbeta_pdf(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *a = test_expr_new_const_d(2.0);
    expr_t *b = test_expr_new_const_d(3.0);
    expr_t *f = expr_logbeta_pdf(x, a, b);
    const expr_t *df = expr_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{logbeta_pdf(x,2,3)} | x=0.5", expr_eval_qf(df), qf_from_double(-2.0));
    print_expr_of(df);

    expr_free(f);
    expr_free(b);
    expr_free(a);
    expr_free(x);
}

void test_deriv_binomial(void)
{
    expr_t *n = test_expr_new_var_d(5.0);
    expr_t *k = test_expr_new_const_d(2.0);
    expr_t *f = expr_binomial(n, k);
    const expr_t *df = expr_get_deriv(f, n);

    check_q_at(__FILE__, __LINE__, 1, "d/dn{binomial(n,2)} | n=5", expr_eval_qf(df), qf_from_double(4.5));
    print_expr_of(df);

    expr_free(f);
    expr_free(k);
    expr_free(n);
}

/* ------------------------------------------------------------------------- */
/* Second derivative tests                                                    */
/* ------------------------------------------------------------------------- */

void test_first_derivatives(void)
{
    TEST_RUN_SUBTEST(test_deriv_const, NULL);
    TEST_RUN_SUBTEST(test_deriv_var, NULL);
    TEST_RUN_SUBTEST(test_deriv_wrt_const_is_nan, NULL);
    TEST_RUN_SUBTEST(test_deriv_neg, NULL);
    TEST_RUN_SUBTEST(test_deriv_add_d, NULL);
    TEST_RUN_SUBTEST(test_deriv_mul_d, NULL);
    TEST_RUN_SUBTEST(test_deriv_div_d, NULL);
    TEST_RUN_SUBTEST(test_deriv_x2, NULL);
    TEST_RUN_SUBTEST(test_deriv_pow3, NULL);
    TEST_RUN_SUBTEST(test_deriv_pow_xy, NULL);
    TEST_RUN_SUBTEST(test_deriv_sin, NULL);
    TEST_RUN_SUBTEST(test_deriv_cos, NULL);
    TEST_RUN_SUBTEST(test_deriv_tan, NULL);
    TEST_RUN_SUBTEST(test_deriv_sinh, NULL);
    TEST_RUN_SUBTEST(test_deriv_cosh, NULL);
    TEST_RUN_SUBTEST(test_deriv_tanh, NULL);
    TEST_RUN_SUBTEST(test_deriv_asin, NULL);
    TEST_RUN_SUBTEST(test_deriv_acos, NULL);
    TEST_RUN_SUBTEST(test_deriv_atan, NULL);
    TEST_RUN_SUBTEST(test_deriv_atan2, NULL);
    TEST_RUN_SUBTEST(test_deriv_asinh, NULL);
    TEST_RUN_SUBTEST(test_deriv_acosh, NULL);
    TEST_RUN_SUBTEST(test_deriv_atanh, NULL);
    TEST_RUN_SUBTEST(test_deriv_exp, NULL);
    TEST_RUN_SUBTEST(test_deriv_log, NULL);
    TEST_RUN_SUBTEST(test_deriv_log10, NULL);
    TEST_RUN_SUBTEST(test_deriv_sqrt, NULL);
    TEST_RUN_SUBTEST(test_deriv_floor_and_ceil, NULL);
    TEST_RUN_SUBTEST(test_deriv_composite, NULL);
    TEST_RUN_SUBTEST(test_deriv_sin_log, NULL);
    TEST_RUN_SUBTEST(test_deriv_exp_tanh, NULL);
    TEST_RUN_SUBTEST(test_deriv_sqrt_sin_x2, NULL);
    TEST_RUN_SUBTEST(test_deriv_log_cosh, NULL);
    TEST_RUN_SUBTEST(test_deriv_x2_exp_negx, NULL);
    TEST_RUN_SUBTEST(test_deriv_atan_x_over_sqrt, NULL);
    TEST_RUN_SUBTEST(test_deriv_abs, NULL);
    TEST_RUN_SUBTEST(test_deriv_hypot, NULL);
    TEST_RUN_SUBTEST(test_deriv_erf, NULL);
    TEST_RUN_SUBTEST(test_deriv_erfc, NULL);
    TEST_RUN_SUBTEST(test_deriv_erfinv, NULL);
    TEST_RUN_SUBTEST(test_deriv_erfcinv, NULL);
    TEST_RUN_SUBTEST(test_deriv_gamma, NULL);
    TEST_RUN_SUBTEST(test_deriv_lgamma, NULL);
    TEST_RUN_SUBTEST(test_deriv_digamma, NULL);
    TEST_RUN_SUBTEST(test_deriv_gammainv, NULL);
    TEST_RUN_SUBTEST(test_deriv_trigamma, NULL);
    TEST_RUN_SUBTEST(test_deriv_lambert_w0, NULL);
    TEST_RUN_SUBTEST(test_deriv_lambert_wm1, NULL);
    TEST_RUN_SUBTEST(test_deriv_normal_pdf, NULL);
    TEST_RUN_SUBTEST(test_deriv_normal_cdf, NULL);
    TEST_RUN_SUBTEST(test_deriv_normal_logpdf, NULL);
    TEST_RUN_SUBTEST(test_deriv_ei, NULL);
    TEST_RUN_SUBTEST(test_deriv_e1, NULL);
    TEST_RUN_SUBTEST(test_deriv_beta, NULL);
    TEST_RUN_SUBTEST(test_deriv_logbeta, NULL);
    TEST_RUN_SUBTEST(test_deriv_gammainc, NULL);
    TEST_RUN_SUBTEST(test_deriv_beta_pdf, NULL);
    TEST_RUN_SUBTEST(test_deriv_logbeta_pdf, NULL);
    TEST_RUN_SUBTEST(test_deriv_binomial, NULL);
}
