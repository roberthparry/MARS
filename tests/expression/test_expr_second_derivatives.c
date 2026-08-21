#include "test_expr.h"

void test_second_deriv_var(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    expr_t *df = expr_create_deriv(x, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x} | x=2", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(x);
    expr_free(df);
}

void test_second_deriv_neg(void)
{
    expr_t *x = test_expr_new_var_d(3.0);
    expr_t *f = expr_neg(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{-x} | x=3", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_add_d(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    expr_t *f = expr_add_d(x, 5.0);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x+5} | x=2", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_mul_d(void)
{
    expr_t *x = test_expr_new_var_d(4.0);
    expr_t *f = expr_mul_d(x, 7.0);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{7x} | x=4", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_div_d(void)
{
    expr_t *x = test_expr_new_var_d(9.0);
    expr_t *f = expr_div_d(x, 3.0);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x/3} | x=9", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_x2(void)
{
    expr_t *x = test_expr_new_var_d(3.0);
    expr_t *f = expr_mul(x, x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t expect = qf_from_double(2.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x^2} | x=3", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_x3(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    expr_t *f = expr_pow_d(x, 3.0);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t expect = qf_from_double(12.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x^3} | x=2", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_pow_xy(void)
{
    expr_t *x = test_expr_new_var_d(2.0);

    expr_t *x2 = expr_mul(x, x);
    expr_t *one = test_expr_new_const_d(1.0);
    expr_t *y = expr_add(x2, one);

    expr_t *f = expr_pow_xp(x, y);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_double(2.0);
    qfloat_t X2 = qf_mul(X, X);

    qfloat_t lnX = qf_log(X);
    qfloat_t lnX2 = qf_mul(lnX, lnX);

    qfloat_t g = qf_add(X2, qf_from_double(1.0));
    qfloat_t fx = qf_pow(X, g);

    qfloat_t term1 = X2;
    qfloat_t term2 = qf_mul(qf_from_double(4.0), qf_mul(X2, lnX2));
    qfloat_t term3 = qf_mul(qf_add(qf_mul(qf_from_double(4.0), X2), qf_from_double(6.0)), lnX);
    qfloat_t term4 = qf_from_double(5.0);

    qfloat_t poly = qf_add(qf_add(term1, term2), qf_add(term3, term4));

    qfloat_t expect = qf_mul(fx, poly);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x^(x^2+1)} | x=2", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(y);
    expr_free(one);
    expr_free(x2);
    expr_free(x);
}

void test_second_deriv_sin(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_sin(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t expect = qf_neg(qf_sin(qf_from_double(0.5)));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sin(x)} | x=0.5", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_cos(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_cos(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t expect = qf_neg(qf_cos(qf_from_double(0.5)));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{cos(x)} | x=0.5", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_tan(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_tan(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_double(0.5);
    qfloat_t sec2 = qf_div(qf_from_double(1.0), qf_mul(qf_cos(X), qf_cos(X)));
    qfloat_t expect = qf_mul(qf_from_double(2.0), qf_mul(sec2, qf_tan(X)));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{tan(x)} | x=0.5", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_sinh(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_sinh(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t expect = qf_sinh(qf_from_double(0.5));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sinh(x)} | x=0.5", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_cosh(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_cosh(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t expect = qf_cosh(qf_from_double(0.5));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{cosh(x)} | x=0.5", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_tanh(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_tanh(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_double(0.5);
    qfloat_t t = qf_tanh(X);

    qfloat_t expect = qf_mul(qf_from_double(-2.0), qf_mul(t, qf_sub(qf_from_double(1.0), qf_mul(t, t))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{tanh(x)} | x=0.5", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_asin(void)
{
    expr_t *x = test_expr_new_var_d(0.25);
    expr_t *f = expr_asin(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t denom = qf_sqrt(qf_sub(qf_from_double(1.0), qf_mul(X, X)));

    qfloat_t expect = qf_div(X, qf_mul(denom, qf_mul(denom, denom)));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{asin(x)} | x=0.25", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_acos(void)
{
    expr_t *x = test_expr_new_var_d(0.25);
    expr_t *f = expr_acos(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t denom = qf_sqrt(qf_sub(qf_from_double(1.0), qf_mul(X, X)));

    qfloat_t expect = qf_mul(qf_from_double(-1.0), qf_div(X, qf_mul(denom, qf_mul(denom, denom))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{acos(x)} | x=0.25", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_atan(void)
{
    expr_t *x = test_expr_new_var_d(0.25);
    expr_t *f = expr_atan(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t denom = qf_add(qf_from_double(1.0), qf_mul(X, X));

    qfloat_t expect = qf_mul(qf_from_double(-2.0), qf_mul(X, qf_div(qf_from_double(1.0), qf_mul(denom, denom))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{atan(x)} | x=0.25", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_atan2(void)
{
    expr_t *x = test_expr_new_var_d(0.25);
    expr_t *one = test_expr_new_const_d(1.0);
    expr_t *f = expr_atan2(x, one);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_double(0.25);

    qfloat_t denom = qf_add(qf_from_double(1.0), qf_mul(X, X));

    qfloat_t expect = qf_mul(qf_from_double(-2.0), qf_mul(X, qf_div(qf_from_double(1.0), qf_mul(denom, denom))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{atan2(x,1)} | x=0.25", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(one);
    expr_free(x);
}

void test_second_deriv_asinh(void)
{
    expr_t *x = test_expr_new_var_d(0.25);
    expr_t *f = expr_asinh(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_double(0.25);

    qfloat_t denom = qf_sqrt(qf_add(qf_from_double(1.0), qf_mul(X, X)));

    qfloat_t expect =
        qf_mul(qf_from_double(-1.0), qf_mul(X, qf_div(qf_from_double(1.0), qf_mul(denom, qf_mul(denom, denom)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{asinh(x)} | x=0.25", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_acosh(void)
{
    expr_t *x = test_expr_new_var_d(1.25);
    expr_t *f = expr_acosh(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_double(1.25);

    qfloat_t denom1 = qf_sqrt(qf_sub(X, qf_from_double(1.0)));
    qfloat_t denom2 = qf_sqrt(qf_add(X, qf_from_double(1.0)));

    qfloat_t denom = qf_mul(denom1, denom2);

    qfloat_t expect =
        qf_mul(qf_from_double(-1.0), qf_mul(X, qf_div(qf_from_double(1.0), qf_mul(denom, qf_mul(denom, denom)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{acosh(x)} | x=1.25", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_atanh(void)
{
    expr_t *x = test_expr_new_var_d(0.25);
    expr_t *f = expr_atanh(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_double(0.25);

    qfloat_t denom = qf_sub(qf_from_double(1.0), qf_mul(X, X));

    qfloat_t expect = qf_mul(qf_from_double(2.0), qf_mul(X, qf_div(qf_from_double(1.0), qf_mul(denom, denom))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{atanh(x)} | x=0.25", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_exp(void)
{
    expr_t *x = test_expr_new_var_d(1.5);
    expr_t *f = expr_exp(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_double(1.5);
    qfloat_t expect = qf_exp(X);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{exp(x)} | x=1.5", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_log(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    expr_t *f = expr_log(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t expect =
        qf_mul(qf_from_double(-1.0), qf_div(qf_from_double(1.0), qf_mul(qf_from_double(2.0), qf_from_double(2.0))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{log(x)} | x=2", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_sqrt(void)
{
    expr_t *x = test_expr_new_var_d(4.0);
    expr_t *f = expr_sqrt(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_double(4.0);

    qfloat_t sqrtX = qf_sqrt(X);
    qfloat_t expect =
        qf_mul(qf_from_double(-1.0),
               qf_div(qf_from_double(1.0), qf_mul(qf_from_double(4.0), qf_mul(sqrtX, qf_mul(sqrtX, sqrtX)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sqrt(x)} | x=4", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_composite(void)
{
    expr_t *x = test_expr_new_var_d(1.0);
    expr_t *sx = expr_sin(x);
    expr_t *ex = expr_exp(x);
    expr_t *f = expr_mul(sx, ex);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_double(1.0);

    qfloat_t expect = qf_add(
        qf_mul(qf_neg(qf_sin(X)), qf_exp(X)),
        qf_add(qf_mul(qf_cos(X), qf_exp(X)), qf_add(qf_mul(qf_cos(X), qf_exp(X)), qf_mul(qf_sin(X), qf_exp(X)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sin(x)*exp(x)} | x=1", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(sx);
    expr_free(ex);
    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_sin_log(void)
{
    expr_t *x = test_expr_new_var_qf(qf_from_string("1.3"));
    expr_t *sx = expr_sin(x);
    expr_t *lx = expr_log(x);
    expr_t *f = expr_mul(sx, lx);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_string("1.3");

    qfloat_t expect = qf_add(
        qf_mul(qf_neg(qf_sin(X)), qf_log(X)),
        qf_add(qf_mul(qf_cos(X), qf_div(qf_from_double(1.0), X)),
               qf_add(qf_mul(qf_cos(X), qf_div(qf_from_double(1.0), X)),
                      qf_mul(qf_sin(X), qf_mul(qf_from_double(-1.0), qf_div(qf_from_double(1.0), qf_mul(X, X)))))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sin(x)*log(x)} | x=1.3", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(sx);
    expr_free(lx);
    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_exp_tanh(void)
{
    expr_t *x = test_expr_new_var_qf(qf_from_string("0.7"));
    expr_t *ex = expr_exp(x);
    expr_t *tx = expr_tanh(x);
    expr_t *f = expr_mul(ex, tx);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_string("0.7");
    qfloat_t t = qf_tanh(X);

    qfloat_t expect = qf_add(
        qf_mul(qf_exp(X), t),
        qf_add(qf_mul(qf_from_double(2.0), qf_mul(qf_exp(X), qf_sub(qf_from_double(1.0), qf_mul(t, t)))),
               qf_mul(qf_exp(X), qf_mul(qf_from_double(-2.0), qf_mul(t, qf_sub(qf_from_double(1.0), qf_mul(t, t)))))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{exp(x)*tanh(x)} | x=0.7", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(ex);
    expr_free(tx);
    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_sqrt_sin_x2(void)
{
    expr_t *x = test_expr_new_var_qf(qf_from_string("1.1"));
    expr_t *x2 = expr_mul(x, x);
    expr_t *sqx = expr_sqrt(x);
    expr_t *sx2 = expr_sin(x2);
    expr_t *f = expr_mul(sqx, sx2);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_string("1.1");
    qfloat_t X2 = qf_mul(X, X);

    /* f = sqrt(x)*sin(x²), using (fg)'' = f''g + 2f'g' + fg''
     * u = sqrt(x):  u' = 1/(2√x),  u'' = -1/(4x^(3/2))
     * v = sin(x²):  v' = 2x·cos(x²),  v'' = 2·cos(x²) - 4x²·sin(x²)
     */
    qfloat_t sqrtX = qf_sqrt(X);
    qfloat_t X3_2 = qf_mul(sqrtX, qf_mul(sqrtX, sqrtX)); /* x^(3/2) */

    /* u''·v */
    qfloat_t term1 = qf_mul(qf_neg(qf_div(qf_from_double(1.0), qf_mul(qf_from_double(4.0), X3_2))), qf_sin(X2));

    /* 2·u'·v' = 2·(1/(2√x))·(2x·cos(x²)) = 2√x·cos(x²) */
    qfloat_t term2 = qf_mul(qf_mul(qf_from_double(2.0), sqrtX), qf_cos(X2));

    /* u·v'' = √x·(2·cos(x²) - 4x²·sin(x²)) */
    qfloat_t term3 = qf_mul(
        sqrtX, qf_add(qf_mul(qf_from_double(2.0), qf_cos(X2)), qf_mul(qf_from_double(-4.0), qf_mul(X2, qf_sin(X2)))));

    qfloat_t expect = qf_add(qf_add(term1, term2), term3);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sqrt(x)*sin(x^2)} | x=1.1", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(sqx);
    expr_free(sx2);
    expr_free(df);
    expr_free(f);
    expr_free(x2);
    expr_free(x);
}

void test_second_deriv_log_cosh(void)
{
    expr_t *x = test_expr_new_var_qf(qf_from_string("0.9"));
    expr_t *cx = expr_cosh(x);
    expr_t *f = expr_log(cx);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_string("0.9");

    /* d²/dx²{log(cosh(x))} = sech²(x) = 1 - tanh²(x) */
    qfloat_t t = qf_tanh(X);
    qfloat_t expect = qf_sub(qf_from_double(1.0), qf_mul(t, t));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{log(cosh(x))} | x=0.9", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(cx);
    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_x2_exp_negx(void)
{
    expr_t *x = test_expr_new_var_qf(qf_from_string("1.7"));
    expr_t *xm = expr_neg(x);
    expr_t *ex = expr_exp(xm);
    expr_t *x2 = expr_mul(x, x);
    expr_t *f = expr_mul(x2, ex);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_string("1.7");
    qfloat_t e_mx = qf_exp(qf_neg(X));

    /* d²/dx²{x²·e^{-x}} = e^{-x}·(x² - 4x + 2) */
    qfloat_t expect = qf_mul(e_mx, qf_add(qf_mul(X, X), qf_add(qf_mul(qf_from_double(-4.0), X), qf_from_double(2.0))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x^2*exp(-x)} | x=1.7", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x2);
    expr_free(ex);
    expr_free(xm);
    expr_free(x);
}

void test_second_deriv_atan_x_over_sqrt(void)
{
    expr_t *x = test_expr_new_var_qf(qf_from_string("0.8"));

    expr_t *x2 = expr_mul(x, x);
    expr_t *one = test_expr_new_const_d(1.0);
    expr_t *sum = expr_add(one, x2);
    expr_t *den = expr_sqrt(sum);
    expr_t *g = expr_div(one, den);

    expr_t *u = expr_mul(x, g);
    expr_t *f = expr_atan(u);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    qfloat_t X = qf_from_string("0.8");

    /* d²/dx²{atan(x/√(1+x²))} = -x(5+6x²) / ((1+x²)^(3/2)·(1+2x²)²) */
    qfloat_t X2 = qf_mul(X, X);
    qfloat_t one_p_x2 = qf_add(qf_from_double(1.0), X2);
    qfloat_t one_p_2x2 = qf_add(qf_from_double(1.0), qf_mul(qf_from_double(2.0), X2));
    qfloat_t five_p_6x2 = qf_add(qf_from_double(5.0), qf_mul(qf_from_double(6.0), X2));
    qfloat_t sqrtX_ = qf_sqrt(one_p_x2);
    qfloat_t numer = qf_neg(qf_mul(X, five_p_6x2));
    qfloat_t denom = qf_mul(qf_mul(sqrtX_, one_p_x2), qf_mul(one_p_2x2, one_p_2x2));
    qfloat_t expect = qf_div(numer, denom);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{atan(x/sqrt(1+x^2))} | x=0.8", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(sum);
    expr_free(df);
    expr_free(f);
    expr_free(u);
    expr_free(g);
    expr_free(den);
    expr_free(one);
    expr_free(x2);
    expr_free(x);
}

/* Special function second derivative tests */

void test_second_deriv_abs(void)
{
    expr_t *x = test_expr_new_var_qf(qf_from_string("0.8"));
    expr_t *f = expr_abs(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/dx²{|x|} = 0 for x != 0 */
    qfloat_t expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{|x|} | x=0.8", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_hypot(void)
{
    expr_t *x = test_expr_new_var_d(3.0);
    expr_t *yc = test_expr_new_const_d(4.0);
    expr_t *f = expr_hypot(x, yc);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/dx²{hypot(x,y)} = y^2 / hypot(x,y)^3 at x=3, y=4: 16/125 */
    qfloat_t X = qf_from_double(3.0);
    qfloat_t Y = qf_from_double(4.0);
    qfloat_t h = qf_hypot(X, Y);
    qfloat_t expect = qf_div(qf_mul(Y, Y), qf_mul(h, qf_mul(h, h)));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{hypot(x,4)} | x=3", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(yc);
    expr_free(x);
}

void test_second_deriv_erf(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_erf(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/dx²{erf(x)} = -4x/sqrt(pi) * exp(-x^2) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t expect = qf_mul(qf_div(qf_from_double(-4.0), qf_sqrt(QF_PI)), qf_mul(X, qf_exp(qf_neg(qf_mul(X, X)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{erf(x)} | x=0.5", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_erfc(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_erfc(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/dx²{erfc(x)} = 4x/sqrt(pi) * exp(-x^2) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t expect = qf_mul(qf_div(qf_from_double(4.0), qf_sqrt(QF_PI)), qf_mul(X, qf_exp(qf_neg(qf_mul(X, X)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{erfc(x)} | x=0.5", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_gamma(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    expr_t *f = expr_gamma(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/dx²{gamma(x)} = gamma(x) * (digamma(x)^2 + trigamma(x))
       at x=2: gamma(2)=1, digamma(2)=1-gamma_euler, trigamma(2)=pi^2/6-1 */
    qfloat_t X = qf_from_double(2.0);
    qfloat_t g = qf_gamma(X);
    qfloat_t d = qf_digamma(X);
    qfloat_t t = qf_sub(qf_div(qf_mul(QF_PI, QF_PI), qf_from_double(6.0)), qf_from_double(1.0));
    qfloat_t expect = qf_mul(g, qf_add(qf_mul(d, d), t));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{gamma(x)} | x=2", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_lgamma(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    expr_t *f = expr_lgamma(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/dx²{lgamma(x)} = trigamma(x); trigamma(2) = pi^2/6 - 1 */
    qfloat_t expect = qf_sub(qf_div(qf_mul(QF_PI, QF_PI), qf_from_double(6.0)), qf_from_double(1.0));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{lgamma(x)} | x=2", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_normal_pdf(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_normal_pdf(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/dx²{phi(x)} = (x^2 - 1) * phi(x) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t expect = qf_mul(qf_sub(qf_mul(X, X), qf_from_double(1.0)), qf_normal_pdf(X));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{phi(x)} | x=0.5", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_normal_cdf(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_normal_cdf(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/dx²{Phi(x)} = -x * phi(x) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t expect = qf_neg(qf_mul(X, qf_normal_pdf(X)));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{Phi(x)} | x=0.5", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_normal_logpdf(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_normal_logpdf(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/dx²{log phi(x)} = -1 */
    qfloat_t expect = qf_from_double(-1.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{log phi(x)} | x=0.5", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_Ei(void)
{
    expr_t *x = test_expr_new_var_d(1.0);
    expr_t *f = expr_Ei(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/dx²{Ei(x)} = exp(x)*(x-1)/x^2; at x=1: 0 */
    qfloat_t X = qf_from_double(1.0);
    qfloat_t expect = qf_div(qf_mul(qf_exp(X), qf_sub(X, qf_from_double(1.0))), qf_mul(X, X));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{Ei(x)} | x=1", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_E1(void)
{
    expr_t *x = test_expr_new_var_d(1.0);
    expr_t *f = expr_E1(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/dx²{E1(x)} = exp(-x)*(x+1)/x^2; at x=1: 2/e */
    qfloat_t X = qf_from_double(1.0);
    qfloat_t expect = qf_div(qf_mul(qf_exp(qf_neg(X)), qf_add(X, qf_from_double(1.0))), qf_mul(X, X));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{E1(x)} | x=1", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_erfinv(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_erfinv(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/dx²{erfinv(x)} = (π/2) * erfinv(x) * exp(2*erfinv(x)^2) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t u = qf_erfinv(X);
    qfloat_t expect =
        qf_mul(qf_mul(QF_PI, qf_from_double(0.5)), qf_mul(u, qf_exp(qf_mul(qf_from_double(2.0), qf_mul(u, u)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{erfinv(x)} | x=0.5", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_erfcinv(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *f = expr_erfcinv(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/dx²{erfcinv(x)} = (π/2) * erfcinv(x) * exp(2*erfcinv(x)^2) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t v = qf_erfcinv(X);
    qfloat_t expect =
        qf_mul(qf_mul(QF_PI, qf_from_double(0.5)), qf_mul(v, qf_exp(qf_mul(qf_from_double(2.0), qf_mul(v, v)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{erfcinv(x)} | x=0.5", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_lambert_w0(void)
{
    expr_t *x = test_expr_new_var_d(1.0);
    expr_t *f = expr_lambert_w0(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/dx²{W0(x)} = -W0^2 * (2 + W0) / (x^2 * (1 + W0)^3) */
    qfloat_t X = qf_from_double(1.0);
    qfloat_t W = qf_lambert_w0(X);
    qfloat_t W1 = qf_add(qf_from_double(1.0), W);
    qfloat_t W2 = qf_add(qf_from_double(2.0), W);
    qfloat_t expect = qf_neg(qf_div(qf_mul(qf_mul(W, W), W2), qf_mul(qf_mul(X, X), qf_mul(W1, qf_mul(W1, W1)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{W0(x)} | x=1", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_gammainv(void)
{
    qfloat_t X = qf_gamma(qf_from_double(2.5));
    expr_t *x = test_expr_new_var_qf(X);
    expr_t *f = expr_gammainv(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* y = gammainv(x), y'' = -(ψ'(y) + ψ(y)^2) / (x^2 ψ(y)^3) */
    qfloat_t y = qf_gammainv(X);
    qfloat_t psi = qf_digamma(y);
    qfloat_t psi1 = qf_trigamma(y);
    qfloat_t numer = qf_add(psi1, qf_mul(psi, psi));
    qfloat_t denom = qf_mul(qf_mul(X, X), qf_mul(psi, qf_mul(psi, psi)));
    qfloat_t expect = qf_neg(qf_div(numer, denom));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{gammainv(x)} | x=gamma(2.5)", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_lambert_wm1(void)
{
    expr_t *x = test_expr_new_var_qf(qf_from_string("-0.1"));
    expr_t *f = expr_lambert_wm1(x);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/dx²{Wm1(x)} = -Wm1^2 * (2 + Wm1) / (x^2 * (1 + Wm1)^3) */
    qfloat_t X = qf_from_string("-0.1");
    qfloat_t W = qf_lambert_wm1(X);
    qfloat_t W1 = qf_add(qf_from_double(1.0), W);
    qfloat_t W2 = qf_add(qf_from_double(2.0), W);
    qfloat_t expect = qf_neg(qf_div(qf_mul(qf_mul(W, W), W2), qf_mul(qf_mul(X, X), qf_mul(W1, qf_mul(W1, W1)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{Wm1(x)} | x=-0.1", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

void test_second_deriv_beta(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    expr_t *bc = test_expr_new_const_d(3.0);
    expr_t *f = expr_beta(x, bc);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/da²{beta(a,b)} = beta(a,b) * [(ψ(a)-ψ(a+b))² + ψ'(a) - ψ'(a+b)] */
    qfloat_t A = qf_from_double(2.0);
    qfloat_t B = qf_from_double(3.0);
    qfloat_t ApB = qf_add(A, B);
    qfloat_t d_a = qf_sub(qf_digamma(A), qf_digamma(ApB));
    qfloat_t t_a = qf_sub(qf_trigamma(A), qf_trigamma(ApB));
    qfloat_t expect = qf_mul(qf_beta(A, B), qf_add(qf_mul(d_a, d_a), t_a));

    check_q_at(__FILE__, __LINE__, 1, "d²/da²{beta(a,3)} | a=2", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(bc);
    expr_free(x);
}

void test_second_deriv_logbeta(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    expr_t *bc = test_expr_new_const_d(3.0);
    expr_t *f = expr_logbeta(x, bc);
    expr_t *df = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/da²{logbeta(a,b)} = ψ'(a) - ψ'(a+b) */
    qfloat_t A = qf_from_double(2.0);
    qfloat_t B = qf_from_double(3.0);
    qfloat_t expect = qf_sub(qf_trigamma(A), qf_trigamma(qf_add(A, B)));

    check_q_at(__FILE__, __LINE__, 1, "d²/da²{logbeta(a,3)} | a=2", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    expr_free(df);
    expr_free(f);
    expr_free(bc);
    expr_free(x);
}

void test_second_derivatives(void)
{
    TEST_RUN_SUBTEST(test_second_deriv_var, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_neg, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_add_d, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_mul_d, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_div_d, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_x2, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_x3, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_pow_xy, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_sin, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_cos, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_tan, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_sinh, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_cosh, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_tanh, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_asin, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_acos, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_atan, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_atan2, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_asinh, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_acosh, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_atanh, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_exp, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_log, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_sqrt, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_composite, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_sin_log, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_exp_tanh, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_sqrt_sin_x2, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_log_cosh, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_x2_exp_negx, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_atan_x_over_sqrt, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_abs, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_hypot, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_erf, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_erfc, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_gamma, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_lgamma, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_gammainv, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_normal_pdf, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_normal_cdf, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_normal_logpdf, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_Ei, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_E1, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_erfinv, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_erfcinv, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_lambert_w0, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_lambert_wm1, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_beta, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_logbeta, NULL);
    TEST_RUN_SUBTEST(test_second_deriv_digamma, NULL);
}
