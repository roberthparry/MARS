#include "test_dval.h"

void test_second_deriv_var(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *df  = dv_create_deriv(x, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x} | x=2", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(x);
    dv_free(df);
}

void test_second_deriv_neg(void)
{
    dval_t *x   = dv_new_var_d(3.0);
    dval_t *f   = dv_neg(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{-x} | x=3", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_add_d(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *f   = dv_add_d(x, 5.0);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x+5} | x=2", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_mul_d(void)
{
    dval_t *x   = dv_new_var_d(4.0);
    dval_t *f   = dv_mul_d(x, 7.0);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{7x} | x=4", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_div_d(void)
{
    dval_t *x   = dv_new_var_d(9.0);
    dval_t *f   = dv_div_d(x, 3.0);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x/3} | x=9", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_x2(void)
{
    dval_t *x   = dv_new_var_d(3.0);
    dval_t *f   = dv_mul(x, x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t expect = qf_from_double(2.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x^2} | x=3", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_x3(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *f   = dv_pow_d(x, 3.0);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t expect = qf_from_double(12.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x^3} | x=2", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_pow_xy(void)
{
    dval_t *x = dv_new_var_d(2.0);

    dval_t *x2  = dv_mul(x, x);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *y   = dv_add(x2, one);

    dval_t *f   = dv_pow(x, y);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X  = qf_from_double(2.0);
    qfloat_t X2 = qf_mul(X, X);

    qfloat_t lnX  = qf_log(X);
    qfloat_t lnX2 = qf_mul(lnX, lnX);

    qfloat_t g    = qf_add(X2, qf_from_double(1.0));
    qfloat_t fx   = qf_pow(X, g);

    qfloat_t term1 = X2;
    qfloat_t term2 = qf_mul(qf_from_double(4.0), qf_mul(X2, lnX2));
    qfloat_t term3 = qf_mul(qf_add(qf_mul(qf_from_double(4.0), X2), qf_from_double(6.0)), lnX);
    qfloat_t term4 = qf_from_double(5.0);

    qfloat_t poly = qf_add(qf_add(term1, term2), qf_add(term3, term4));

    qfloat_t expect = qf_mul(fx, poly);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x^(x^2+1)} | x=2", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(y);
    dv_free(one);
    dv_free(x2);
    dv_free(x);
}

void test_second_deriv_sin(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_sin(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t expect = qf_neg(qf_sin(qf_from_double(0.5)));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sin(x)} | x=0.5", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_cos(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_cos(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t expect = qf_neg(qf_cos(qf_from_double(0.5)));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{cos(x)} | x=0.5", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_tan(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_tan(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X = qf_from_double(0.5);
    qfloat_t sec2 = qf_div(qf_from_double(1.0), qf_mul(qf_cos(X), qf_cos(X)));
    qfloat_t expect = qf_mul(qf_from_double(2.0), qf_mul(sec2, qf_tan(X)));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{tan(x)} | x=0.5", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_sinh(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_sinh(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t expect = qf_sinh(qf_from_double(0.5));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sinh(x)} | x=0.5", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_cosh(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_cosh(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t expect = qf_cosh(qf_from_double(0.5));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{cosh(x)} | x=0.5", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_tanh(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_tanh(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X = qf_from_double(0.5);
    qfloat_t t = qf_tanh(X);

    qfloat_t expect = qf_mul(qf_from_double(-2.0), qf_mul(t, qf_sub(qf_from_double(1.0), qf_mul(t, t))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{tanh(x)} | x=0.5", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_asin(void)
{
    dval_t *x   = dv_new_var_d(0.25);
    dval_t *f   = dv_asin(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t denom = qf_sqrt(qf_sub(qf_from_double(1.0), qf_mul(X, X)));

    qfloat_t expect = qf_div(X, qf_mul(denom, qf_mul(denom, denom)));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{asin(x)} | x=0.25", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_acos(void)
{
    dval_t *x   = dv_new_var_d(0.25);
    dval_t *f   = dv_acos(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t denom = qf_sqrt(qf_sub(qf_from_double(1.0), qf_mul(X, X)));

    qfloat_t expect = qf_mul(qf_from_double(-1.0), qf_div(X, qf_mul(denom, qf_mul(denom, denom))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{acos(x)} | x=0.25", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_atan(void)
{
    dval_t *x   = dv_new_var_d(0.25);
    dval_t *f   = dv_atan(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X = qf_from_double(0.25);
    qfloat_t denom = qf_add(qf_from_double(1.0), qf_mul(X, X));

    qfloat_t expect = qf_mul(qf_from_double(-2.0), qf_mul(X, qf_div(qf_from_double(1.0), qf_mul(denom, denom))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{atan(x)} | x=0.25", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_atan2(void)
{
    dval_t *x   = dv_new_var_d(0.25);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *f   = dv_atan2(x, one);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X = qf_from_double(0.25);

    qfloat_t denom = qf_add(qf_from_double(1.0), qf_mul(X, X));

    qfloat_t expect = qf_mul(qf_from_double(-2.0), qf_mul(X, qf_div(qf_from_double(1.0), qf_mul(denom, denom))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{atan2(x,1)} | x=0.25", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(one);
    dv_free(x);
}

void test_second_deriv_asinh(void)
{
    dval_t *x   = dv_new_var_d(0.25);
    dval_t *f   = dv_asinh(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X = qf_from_double(0.25);

    qfloat_t denom = qf_sqrt(qf_add(qf_from_double(1.0), qf_mul(X, X)));

    qfloat_t expect = qf_mul(qf_from_double(-1.0), qf_mul(X, qf_div(qf_from_double(1.0), qf_mul(denom, qf_mul(denom, denom)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{asinh(x)} | x=0.25", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_acosh(void)
{
    dval_t *x   = dv_new_var_d(1.25);
    dval_t *f   = dv_acosh(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X = qf_from_double(1.25);

    qfloat_t denom1 = qf_sqrt(qf_sub(X, qf_from_double(1.0)));
    qfloat_t denom2 = qf_sqrt(qf_add(X, qf_from_double(1.0)));

    qfloat_t denom = qf_mul(denom1, denom2);

    qfloat_t expect = qf_mul(qf_from_double(-1.0), qf_mul(X, qf_div(qf_from_double(1.0), qf_mul(denom, qf_mul(denom, denom)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{acosh(x)} | x=1.25", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_atanh(void)
{
    dval_t *x   = dv_new_var_d(0.25);
    dval_t *f   = dv_atanh(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X = qf_from_double(0.25);

    qfloat_t denom = qf_sub(qf_from_double(1.0), qf_mul(X, X));

    qfloat_t expect = qf_mul(qf_from_double(2.0), qf_mul(X, qf_div(qf_from_double(1.0), qf_mul(denom, denom))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{atanh(x)} | x=0.25", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_exp(void)
{
    dval_t *x   = dv_new_var_d(1.5);
    dval_t *f   = dv_exp(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X = qf_from_double(1.5);
    qfloat_t expect = qf_exp(X);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{exp(x)} | x=1.5", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_log(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *f   = dv_log(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t expect = qf_mul(qf_from_double(-1.0), qf_div(qf_from_double(1.0), qf_mul(qf_from_double(2.0), qf_from_double(2.0))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{log(x)} | x=2", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_sqrt(void)
{
    dval_t *x   = dv_new_var_d(4.0);
    dval_t *f   = dv_sqrt(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X = qf_from_double(4.0);

    qfloat_t sqrtX = qf_sqrt(X);
    qfloat_t expect = qf_mul(qf_from_double(-1.0), qf_div(qf_from_double(1.0), qf_mul(qf_from_double(4.0),
                           qf_mul(sqrtX, qf_mul(sqrtX, sqrtX)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sqrt(x)} | x=4", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_composite(void)
{
    dval_t *x   = dv_new_var_d(1.0);
    dval_t *sx  = dv_sin(x);
    dval_t *ex  = dv_exp(x);
    dval_t *f   = dv_mul(sx, ex);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X = qf_from_double(1.0);

    qfloat_t expect = qf_add(qf_mul(qf_neg(qf_sin(X)), qf_exp(X)), qf_add(qf_mul(qf_cos(X), qf_exp(X)),
                           qf_add(qf_mul(qf_cos(X), qf_exp(X)), qf_mul(qf_sin(X), qf_exp(X)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sin(x)*exp(x)} | x=1", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(sx);
    dv_free(ex);
    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_sin_log(void)
{
    dval_t *x   = dv_new_var(qf_from_string("1.3"));
    dval_t *sx  = dv_sin(x);
    dval_t *lx  = dv_log(x);
    dval_t *f   = dv_mul(sx, lx);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X = qf_from_string("1.3");

    qfloat_t expect = qf_add(qf_mul(qf_neg(qf_sin(X)), qf_log(X)),
                           qf_add(qf_mul(qf_cos(X), qf_div(qf_from_double(1.0), X)),
                           qf_add(qf_mul(qf_cos(X), qf_div(qf_from_double(1.0), X)),
                                  qf_mul(qf_sin(X), qf_mul(qf_from_double(-1.0),
                                  qf_div(qf_from_double(1.0), qf_mul(X, X)))))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sin(x)*log(x)} | x=1.3", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(sx);
    dv_free(lx);
    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_exp_tanh(void)
{
    dval_t *x   = dv_new_var(qf_from_string("0.7"));
    dval_t *ex  = dv_exp(x);
    dval_t *tx  = dv_tanh(x);
    dval_t *f   = dv_mul(ex, tx);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X = qf_from_string("0.7");
    qfloat_t t = qf_tanh(X);

    qfloat_t expect = qf_add(qf_mul(qf_exp(X), t), qf_add(qf_mul(qf_from_double(2.0),
                           qf_mul(qf_exp(X), qf_sub(qf_from_double(1.0), qf_mul(t, t)))),
                           qf_mul(qf_exp(X), qf_mul(qf_from_double(-2.0), qf_mul(t, qf_sub(qf_from_double(1.0), qf_mul(t, t)))))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{exp(x)*tanh(x)} | x=0.7", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(ex);
    dv_free(tx);
    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_sqrt_sin_x2(void)
{
    dval_t *x   = dv_new_var(qf_from_string("1.1"));
    dval_t *x2  = dv_mul(x, x);
    dval_t *sqx = dv_sqrt(x);
    dval_t *sx2 = dv_sin(x2);
    dval_t *f   = dv_mul(sqx, sx2);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X  = qf_from_string("1.1");
    qfloat_t X2 = qf_mul(X, X);

    /* f = sqrt(x)*sin(x²), using (fg)'' = f''g + 2f'g' + fg''
     * u = sqrt(x):  u' = 1/(2√x),  u'' = -1/(4x^(3/2))
     * v = sin(x²):  v' = 2x·cos(x²),  v'' = 2·cos(x²) - 4x²·sin(x²)
     */
    qfloat_t sqrtX = qf_sqrt(X);
    qfloat_t X3_2  = qf_mul(sqrtX, qf_mul(sqrtX, sqrtX));  /* x^(3/2) */

    /* u''·v */
    qfloat_t term1 = qf_mul(qf_neg(qf_div(qf_from_double(1.0), qf_mul(qf_from_double(4.0), X3_2))), qf_sin(X2));

    /* 2·u'·v' = 2·(1/(2√x))·(2x·cos(x²)) = 2√x·cos(x²) */
    qfloat_t term2 = qf_mul(qf_mul(qf_from_double(2.0), sqrtX), qf_cos(X2));

    /* u·v'' = √x·(2·cos(x²) - 4x²·sin(x²)) */
    qfloat_t term3 =
        qf_mul(sqrtX, qf_add(qf_mul(qf_from_double(2.0), qf_cos(X2)), qf_mul(qf_from_double(-4.0), qf_mul(X2, qf_sin(X2)))));

    qfloat_t expect = qf_add(qf_add(term1, term2), term3);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sqrt(x)*sin(x^2)} | x=1.1", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(sqx);
    dv_free(sx2);
    dv_free(df);
    dv_free(f);
    dv_free(x2);
    dv_free(x);
}

void test_second_deriv_log_cosh(void)
{
    dval_t *x   = dv_new_var(qf_from_string("0.9"));
    dval_t *cx  = dv_cosh(x);
    dval_t *f   = dv_log(cx);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X = qf_from_string("0.9");

    /* d²/dx²{log(cosh(x))} = sech²(x) = 1 - tanh²(x) */
    qfloat_t t = qf_tanh(X);
    qfloat_t expect = qf_sub(qf_from_double(1.0), qf_mul(t, t));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{log(cosh(x))} | x=0.9", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(cx);
    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_x2_exp_negx(void)
{
    dval_t *x   = dv_new_var(qf_from_string("1.7"));
    dval_t *xm  = dv_neg(x);
    dval_t *ex  = dv_exp(xm);
    dval_t *x2  = dv_mul(x, x);
    dval_t *f   = dv_mul(x2, ex);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X    = qf_from_string("1.7");
    qfloat_t e_mx = qf_exp(qf_neg(X));

    /* d²/dx²{x²·e^{-x}} = e^{-x}·(x² - 4x + 2) */
    qfloat_t expect = qf_mul(e_mx, qf_add(qf_mul(X, X), qf_add(qf_mul(qf_from_double(-4.0), X), qf_from_double(2.0))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x^2*exp(-x)} | x=1.7", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x2);
    dv_free(ex);
    dv_free(xm);
    dv_free(x);
}

void test_second_deriv_atan_x_over_sqrt(void)
{
    dval_t *x   = dv_new_var(qf_from_string("0.8"));

    dval_t *x2  = dv_mul(x, x);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *sum = dv_add(one, x2);
    dval_t *den = dv_sqrt(sum);
    dval_t *g   = dv_div(one, den);

    dval_t *u   = dv_mul(x, g);
    dval_t *f   = dv_atan(u);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t X = qf_from_string("0.8");

    /* d²/dx²{atan(x/√(1+x²))} = -x(5+6x²) / ((1+x²)^(3/2)·(1+2x²)²) */
    qfloat_t X2           = qf_mul(X, X);
    qfloat_t one_p_x2     = qf_add(qf_from_double(1.0), X2);
    qfloat_t one_p_2x2    = qf_add(qf_from_double(1.0), qf_mul(qf_from_double(2.0), X2));
    qfloat_t five_p_6x2   = qf_add(qf_from_double(5.0), qf_mul(qf_from_double(6.0), X2));
    qfloat_t sqrtX_       = qf_sqrt(one_p_x2);
    qfloat_t numer        = qf_neg(qf_mul(X, five_p_6x2));
    qfloat_t denom        = qf_mul(qf_mul(sqrtX_, one_p_x2), qf_mul(one_p_2x2, one_p_2x2));
    qfloat_t expect       = qf_div(numer, denom);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{atan(x/sqrt(1+x^2))} | x=0.8", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(sum);
    dv_free(df);
    dv_free(f);
    dv_free(u);
    dv_free(g);
    dv_free(den);
    dv_free(one);
    dv_free(x2);
    dv_free(x);
}

/* Special function second derivative tests */

void test_second_deriv_abs(void)
{
    dval_t *x   = dv_new_var(qf_from_string("0.8"));
    dval_t *f   = dv_abs(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/dx²{|x|} = 0 for x != 0 */
    qfloat_t expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{|x|} | x=0.8", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_hypot(void)
{
    dval_t *x   = dv_new_var_d(3.0);
    dval_t *yc  = dv_new_const_d(4.0);
    dval_t *f   = dv_hypot(x, yc);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/dx²{hypot(x,y)} = y^2 / hypot(x,y)^3 at x=3, y=4: 16/125 */
    qfloat_t X = qf_from_double(3.0);
    qfloat_t Y = qf_from_double(4.0);
    qfloat_t h = qf_hypot(X, Y);
    qfloat_t expect = qf_div(qf_mul(Y, Y), qf_mul(h, qf_mul(h, h)));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{hypot(x,4)} | x=3", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(yc);
    dv_free(x);
}

void test_second_deriv_erf(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_erf(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/dx²{erf(x)} = -4x/sqrt(pi) * exp(-x^2) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t expect = qf_mul(qf_div(qf_from_double(-4.0), qf_sqrt(QF_PI)),
                           qf_mul(X, qf_exp(qf_neg(qf_mul(X, X)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{erf(x)} | x=0.5", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_erfc(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_erfc(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/dx²{erfc(x)} = 4x/sqrt(pi) * exp(-x^2) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t expect = qf_mul(qf_div(qf_from_double(4.0), qf_sqrt(QF_PI)),
                           qf_mul(X, qf_exp(qf_neg(qf_mul(X, X)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{erfc(x)} | x=0.5", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_gamma(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *f   = dv_gamma(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/dx²{gamma(x)} = gamma(x) * (digamma(x)^2 + trigamma(x))
       at x=2: gamma(2)=1, digamma(2)=1-gamma_euler, trigamma(2)=pi^2/6-1 */
    qfloat_t X   = qf_from_double(2.0);
    qfloat_t g   = qf_gamma(X);
    qfloat_t d   = qf_digamma(X);
    qfloat_t t   = qf_sub(qf_div(qf_mul(QF_PI, QF_PI), qf_from_double(6.0)), qf_from_double(1.0));
    qfloat_t expect = qf_mul(g, qf_add(qf_mul(d, d), t));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{gamma(x)} | x=2", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_lgamma(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *f   = dv_lgamma(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/dx²{lgamma(x)} = trigamma(x); trigamma(2) = pi^2/6 - 1 */
    qfloat_t expect = qf_sub(qf_div(qf_mul(QF_PI, QF_PI), qf_from_double(6.0)),
                           qf_from_double(1.0));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{lgamma(x)} | x=2", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_normal_pdf(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_normal_pdf(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/dx²{phi(x)} = (x^2 - 1) * phi(x) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t expect = qf_mul(qf_sub(qf_mul(X, X), qf_from_double(1.0)),
                           qf_normal_pdf(X));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{phi(x)} | x=0.5", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_normal_cdf(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_normal_cdf(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/dx²{Phi(x)} = -x * phi(x) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t expect = qf_neg(qf_mul(X, qf_normal_pdf(X)));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{Phi(x)} | x=0.5", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_normal_logpdf(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_normal_logpdf(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/dx²{log phi(x)} = -1 */
    qfloat_t expect = qf_from_double(-1.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{log phi(x)} | x=0.5", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_ei(void)
{
    dval_t *x   = dv_new_var_d(1.0);
    dval_t *f   = dv_ei(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/dx²{Ei(x)} = exp(x)*(x-1)/x^2; at x=1: 0 */
    qfloat_t X = qf_from_double(1.0);
    qfloat_t expect = qf_div(qf_mul(qf_exp(X), qf_sub(X, qf_from_double(1.0))),
                           qf_mul(X, X));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{Ei(x)} | x=1", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_e1(void)
{
    dval_t *x   = dv_new_var_d(1.0);
    dval_t *f   = dv_e1(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/dx²{E1(x)} = exp(-x)*(x+1)/x^2; at x=1: 2/e */
    qfloat_t X = qf_from_double(1.0);
    qfloat_t expect = qf_div(qf_mul(qf_exp(qf_neg(X)), qf_add(X, qf_from_double(1.0))),
                           qf_mul(X, X));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{E1(x)} | x=1", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_erfinv(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_erfinv(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/dx²{erfinv(x)} = (π/2) * erfinv(x) * exp(2*erfinv(x)^2) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t u = qf_erfinv(X);
    qfloat_t expect = qf_mul(qf_mul(QF_PI, qf_from_double(0.5)),
                           qf_mul(u, qf_exp(qf_mul(qf_from_double(2.0), qf_mul(u, u)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{erfinv(x)} | x=0.5", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_erfcinv(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_erfcinv(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/dx²{erfcinv(x)} = (π/2) * erfcinv(x) * exp(2*erfcinv(x)^2) */
    qfloat_t X = qf_from_double(0.5);
    qfloat_t v = qf_erfcinv(X);
    qfloat_t expect = qf_mul(qf_mul(QF_PI, qf_from_double(0.5)),
                           qf_mul(v, qf_exp(qf_mul(qf_from_double(2.0), qf_mul(v, v)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{erfcinv(x)} | x=0.5", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_lambert_w0(void)
{
    dval_t *x   = dv_new_var_d(1.0);
    dval_t *f   = dv_lambert_w0(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/dx²{W0(x)} = -W0^2 * (2 + W0) / (x^2 * (1 + W0)^3) */
    qfloat_t X  = qf_from_double(1.0);
    qfloat_t W  = qf_lambert_w0(X);
    qfloat_t W1 = qf_add(qf_from_double(1.0), W);
    qfloat_t W2 = qf_add(qf_from_double(2.0), W);
    qfloat_t expect = qf_neg(qf_div(qf_mul(qf_mul(W, W), W2),
                                  qf_mul(qf_mul(X, X),
                                         qf_mul(W1, qf_mul(W1, W1)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{W0(x)} | x=1", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_lambert_wm1(void)
{
    dval_t *x   = dv_new_var(qf_from_string("-0.1"));
    dval_t *f   = dv_lambert_wm1(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/dx²{Wm1(x)} = -Wm1^2 * (2 + Wm1) / (x^2 * (1 + Wm1)^3) */
    qfloat_t X  = qf_from_string("-0.1");
    qfloat_t W  = qf_lambert_wm1(X);
    qfloat_t W1 = qf_add(qf_from_double(1.0), W);
    qfloat_t W2 = qf_add(qf_from_double(2.0), W);
    qfloat_t expect = qf_neg(qf_div(qf_mul(qf_mul(W, W), W2),
                                  qf_mul(qf_mul(X, X),
                                         qf_mul(W1, qf_mul(W1, W1)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{Wm1(x)} | x=-0.1", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_beta(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *bc  = dv_new_const_d(3.0);
    dval_t *f   = dv_beta(x, bc);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/da²{beta(a,b)} = beta(a,b) * [(ψ(a)-ψ(a+b))² + ψ'(a) - ψ'(a+b)] */
    qfloat_t A   = qf_from_double(2.0);
    qfloat_t B   = qf_from_double(3.0);
    qfloat_t ApB = qf_add(A, B);
    qfloat_t d_a = qf_sub(qf_digamma(A),   qf_digamma(ApB));
    qfloat_t t_a = qf_sub(qf_trigamma(A),  qf_trigamma(ApB));
    qfloat_t expect = qf_mul(qf_beta(A, B),
                           qf_add(qf_mul(d_a, d_a), t_a));

    check_q_at(__FILE__, __LINE__, 1, "d²/da²{beta(a,3)} | a=2", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(bc);
    dv_free(x);
}

void test_second_deriv_logbeta(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *bc  = dv_new_const_d(3.0);
    dval_t *f   = dv_logbeta(x, bc);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/da²{logbeta(a,b)} = ψ'(a) - ψ'(a+b) */
    qfloat_t A   = qf_from_double(2.0);
    qfloat_t B   = qf_from_double(3.0);
    qfloat_t expect = qf_sub(qf_trigamma(A), qf_trigamma(qf_add(A, B)));

    check_q_at(__FILE__, __LINE__, 1, "d²/da²{logbeta(a,3)} | a=2", dv_eval_qf(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(bc);
    dv_free(x);
}

void test_second_derivatives(void)
{
    RUN_TEST(test_second_deriv_var, __func__);
    RUN_TEST(test_second_deriv_neg, __func__);
    RUN_TEST(test_second_deriv_add_d, __func__);
    RUN_TEST(test_second_deriv_mul_d, __func__);
    RUN_TEST(test_second_deriv_div_d, __func__);
    RUN_TEST(test_second_deriv_x2, __func__);
    RUN_TEST(test_second_deriv_x3, __func__);
    RUN_TEST(test_second_deriv_pow_xy, __func__);
    RUN_TEST(test_second_deriv_sin, __func__);
    RUN_TEST(test_second_deriv_cos, __func__);
    RUN_TEST(test_second_deriv_tan, __func__);
    RUN_TEST(test_second_deriv_sinh, __func__);
    RUN_TEST(test_second_deriv_cosh, __func__);
    RUN_TEST(test_second_deriv_tanh, __func__);
    RUN_TEST(test_second_deriv_asin, __func__);
    RUN_TEST(test_second_deriv_acos, __func__);
    RUN_TEST(test_second_deriv_atan, __func__);
    RUN_TEST(test_second_deriv_atan2, __func__);
    RUN_TEST(test_second_deriv_asinh, __func__);
    RUN_TEST(test_second_deriv_acosh, __func__);
    RUN_TEST(test_second_deriv_atanh, __func__);
    RUN_TEST(test_second_deriv_exp, __func__);
    RUN_TEST(test_second_deriv_log, __func__);
    RUN_TEST(test_second_deriv_sqrt, __func__);
    RUN_TEST(test_second_deriv_composite, __func__);
    RUN_TEST(test_second_deriv_sin_log, __func__);
    RUN_TEST(test_second_deriv_exp_tanh, __func__);
    RUN_TEST(test_second_deriv_sqrt_sin_x2, __func__);
    RUN_TEST(test_second_deriv_log_cosh, __func__);
    RUN_TEST(test_second_deriv_x2_exp_negx, __func__);
    RUN_TEST(test_second_deriv_atan_x_over_sqrt, __func__);
    RUN_TEST(test_second_deriv_abs, __func__);
    RUN_TEST(test_second_deriv_hypot, __func__);
    RUN_TEST(test_second_deriv_erf, __func__);
    RUN_TEST(test_second_deriv_erfc, __func__);
    RUN_TEST(test_second_deriv_gamma, __func__);
    RUN_TEST(test_second_deriv_lgamma, __func__);
    RUN_TEST(test_second_deriv_normal_pdf, __func__);
    RUN_TEST(test_second_deriv_normal_cdf, __func__);
    RUN_TEST(test_second_deriv_normal_logpdf, __func__);
    RUN_TEST(test_second_deriv_ei, __func__);
    RUN_TEST(test_second_deriv_e1, __func__);
    RUN_TEST(test_second_deriv_erfinv, __func__);
    RUN_TEST(test_second_deriv_erfcinv, __func__);
    RUN_TEST(test_second_deriv_lambert_w0, __func__);
    RUN_TEST(test_second_deriv_lambert_wm1, __func__);
    RUN_TEST(test_second_deriv_beta, __func__);
    RUN_TEST(test_second_deriv_logbeta, __func__);
    RUN_TEST(test_second_deriv_digamma, __func__);
}
