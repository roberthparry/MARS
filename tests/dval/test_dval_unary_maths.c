#include "test_dval.h"

void test_sin(void)
{
    dval_t *p = test_dv_new_var_d(0.5);
    dval_t *f = dv_sin(p);

    check_q_at(__FILE__, __LINE__, 1, "sin(0.5)", dv_eval_qf(f), qf_sin(qf_from_double(0.5)));
    print_expr_of(f);

    dv_free(f);
    dv_free(p);
}

void test_cos(void)
{
    dval_t *c = test_dv_new_var_d(0.5);
    dval_t *f = dv_cos(c);

    check_q_at(__FILE__, __LINE__, 1, "cos(0.5)", dv_eval_qf(f), qf_cos(qf_from_double(0.5)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_tan(void)
{
    dval_t *c = test_dv_new_var_d(0.5);
    dval_t *f = dv_tan(c);

    check_q_at(__FILE__, __LINE__, 1, "tan(0.5)", dv_eval_qf(f), qf_tan(qf_from_double(0.5)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_sinh(void)
{
    dval_t *c = test_dv_new_var_d(0.5);
    dval_t *f = dv_sinh(c);

    check_q_at(__FILE__, __LINE__, 1, "sinh(0.5)", dv_eval_qf(f), qf_sinh(qf_from_double(0.5)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_cosh(void)
{
    dval_t *c = test_dv_new_var_d(0.5);
    dval_t *f = dv_cosh(c);

    check_q_at(__FILE__, __LINE__, 1, "cosh(0.5)", dv_eval_qf(f), qf_cosh(qf_from_double(0.5)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_tanh(void)
{
    dval_t *c = test_dv_new_const_d(0.5);
    dval_t *f = dv_tanh(c);

    check_q_at(__FILE__, __LINE__, 1, "tanh(0.5)", dv_eval_qf(f), qf_tanh(qf_from_double(0.5)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_asin(void)
{
    dval_t *c = test_dv_new_var_d(0.25);
    dval_t *f = dv_asin(c);

    check_q_at(__FILE__, __LINE__, 1, "asin(0.25)", dv_eval_qf(f), qf_asin(qf_from_double(0.25)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_acos(void)
{
    dval_t *c = test_dv_new_var_d(0.25);
    dval_t *f = dv_acos(c);

    check_q_at(__FILE__, __LINE__, 1, "acos(0.25)", dv_eval_qf(f), qf_acos(qf_from_double(0.25)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_atan(void)
{
    dval_t *c = test_dv_new_var_d(0.25);
    dval_t *f = dv_atan(c);

    check_q_at(__FILE__, __LINE__, 1, "atan(0.25)", dv_eval_qf(f), qf_atan(qf_from_double(0.25)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_atan2(void)
{
    dval_t *base = test_dv_new_var_d(2.0);
    dval_t *expo = test_dv_new_const_d(3.0);
    dval_t *f    = dv_atan2(base, expo);

    check_q_at(__FILE__, __LINE__, 1, "atan2(2,3)", dv_eval_qf(f), qf_atan2(qf_from_double(2.0), qf_from_double(3.0)));
    print_expr_of(f);

    dv_free(f);
    dv_free(expo);
    dv_free(base);
}

void test_asinh(void)
{
    dval_t *c = test_dv_new_var_d(0.25);
    dval_t *f = dv_asinh(c);

    check_q_at(__FILE__, __LINE__, 1, "asinh(0.25)", dv_eval_qf(f), qf_asinh(qf_from_double(0.25)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_acosh(void)
{
    dval_t *c = test_dv_new_var_d(1.25);
    dval_t *f = dv_acosh(c);

    check_q_at(__FILE__, __LINE__, 1, "acosh(1.25)", dv_eval_qf(f), qf_acosh(qf_from_double(1.25)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_atanh(void)
{
    dval_t *c = test_dv_new_var_d(0.25);
    dval_t *f = dv_atanh(c);

    check_q_at(__FILE__, __LINE__, 1, "atanh(0.25)", dv_eval_qf(f), qf_atanh(qf_from_double(0.25)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_exp(void)
{
    dval_t *c = test_dv_new_var_d(1.5);
    dval_t *f = dv_exp(c);

    check_q_at(__FILE__, __LINE__, 1, "exp(1.5)", dv_eval_qf(f), qf_exp(qf_from_double(1.5)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_log(void)
{
    dval_t *c = test_dv_new_var_d(1.5);
    dval_t *f = dv_log(c);

    check_q_at(__FILE__, __LINE__, 1, "log(1.5)", dv_eval_qf(f), qf_log(qf_from_double(1.5)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_log10(void)
{
    dval_t *c = test_dv_new_var_d(1000.0);
    dval_t *f = dv_log10(c);

    check_q_at(__FILE__, __LINE__, 1, "log10(1000)", dv_eval_qf(f), qf_from_double(3.0));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_sqrt(void)
{
    dval_t *c = test_dv_new_var_d(2.0);
    dval_t *f = dv_sqrt(c);

    check_q_at(__FILE__, __LINE__, 1, "sqrt(2)", dv_eval_qf(f), qf_sqrt(qf_from_double(2.0)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_floor(void)
{
    dval_t *c = test_dv_new_var_d(1.75);
    dval_t *f = dv_floor(c);

    check_q_at(__FILE__, __LINE__, 1, "floor(1.75)", dv_eval_qf(f), qf_from_double(1.0));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_ceil(void)
{
    dval_t *c = test_dv_new_var_d(1.25);
    dval_t *f = dv_ceil(c);

    check_q_at(__FILE__, __LINE__, 1, "ceil(1.25)", dv_eval_qf(f), qf_from_double(2.0));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_pow_d(void)
{
    dval_t *base = test_dv_new_var_d(2.0);
    dval_t *f    = dv_pow_d(base, 3.0);

    check_q_at(__FILE__, __LINE__, 1, "2^3(d)", dv_eval_qf(f), qf_pow(qf_from_double(2.0), qf_from_double(3.0)));
    print_expr_of(f);

    dv_free(f);
    dv_free(base);
}

void test_pow_d_complex(void)
{
    number_t z0 = num_create_from_string("1 + 2i");
    number_t expected = num_create_from_string("-3 + 4i");
    dval_t *base = dv_new_var(z0);
    dval_t *f = dv_pow_d(base, 2.0);
    number_t got = dv_eval(f);

    ASSERT_DVAL_NUMBER_EQ(got, expected);
    print_expr_of(f);

    num_destroy(&got);
    dv_free(f);
    dv_free(base);
    num_destroy(&expected);
    num_destroy(&z0);
}

void test_pow(void)
{
    dval_t *base = test_dv_new_var_d(2.0);
    dval_t *expo = test_dv_new_const_d(3.0);
    dval_t *f    = dv_pow_dv(base, expo);

    check_q_at(__FILE__, __LINE__, 1, "2^3", dv_eval_qf(f), qf_pow(qf_from_double(2.0), qf_from_double(3.0)));
    print_expr_of(f);

    dv_free(f);
    dv_free(expo);
    dv_free(base);
}

/* Special functions */

void test_abs(void)
{
    /* abs(-3) = 3 exactly */
    dval_t *c = test_dv_new_var_d(-3.0);
    dval_t *f = dv_abs(c);
    check_q_at(__FILE__, __LINE__, 1, "abs(-3) = 3", dv_eval_qf(f), qf_from_double(3.0));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* abs(0.7) = 0.7 */
    c = test_dv_new_var_d(0.7);
    f = dv_abs(c);
    check_q_at(__FILE__, __LINE__, 1, "abs(0.7) = 0.7", dv_eval_qf(f), qf_from_double(0.7));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* abs(-x) = abs(x) symmetry at x=1.5 */
    dval_t *cp = test_dv_new_const_d(1.5);
    dval_t *cn = test_dv_new_const_d(-1.5);
    dval_t *fp = dv_abs(cp);
    dval_t *fn = dv_abs(cn);
    check_q_at(__FILE__, __LINE__, 1, "abs(-1.5) = abs(1.5)", dv_eval_qf(fn), dv_eval_qf(fp));
    dv_free(fp); dv_free(fn); dv_free(cp); dv_free(cn);
}

void test_hypot(void)
{
    /* hypot(3,4) = 5 — Pythagorean triple */
    dval_t *a = test_dv_new_var_d(3.0);
    dval_t *b = test_dv_new_const_d(4.0);
    dval_t *f = dv_hypot(a, b);
    check_q_at(__FILE__, __LINE__, 1, "hypot(3,4) = 5", dv_eval_qf(f), qf_from_double(5.0));
    print_expr_of(f);
    dv_free(f); dv_free(b); dv_free(a);

    /* hypot(5,12) = 13 — Pythagorean triple */
    a = test_dv_new_var_d(5.0);
    b = test_dv_new_const_d(12.0);
    f = dv_hypot(a, b);
    check_q_at(__FILE__, __LINE__, 1, "hypot(5,12) = 13", dv_eval_qf(f), qf_from_double(13.0));
    print_expr_of(f);
    dv_free(f); dv_free(b); dv_free(a);

    /* hypot(a,b) = hypot(b,a) symmetry */
    a = test_dv_new_const_d(2.0);
    b = test_dv_new_const_d(7.0);
    dval_t *fab = dv_hypot(a, b);
    dval_t *fba = dv_hypot(b, a);
    check_q_at(__FILE__, __LINE__, 1, "hypot(2,7) = hypot(7,2)", dv_eval_qf(fab), dv_eval_qf(fba));
    dv_free(fab); dv_free(fba); dv_free(a); dv_free(b);
}

void test_maths_functions(void)
{
    TEST_RUN_SUBTEST(test_sin, NULL);
    TEST_RUN_SUBTEST(test_cos, NULL);
    TEST_RUN_SUBTEST(test_tan, NULL);
    TEST_RUN_SUBTEST(test_sinh, NULL);
    TEST_RUN_SUBTEST(test_cosh, NULL);
    TEST_RUN_SUBTEST(test_tanh, NULL);
    TEST_RUN_SUBTEST(test_asin, NULL);
    TEST_RUN_SUBTEST(test_acos, NULL);
    TEST_RUN_SUBTEST(test_atan, NULL);
    TEST_RUN_SUBTEST(test_atan2, NULL);
    TEST_RUN_SUBTEST(test_asinh, NULL);
    TEST_RUN_SUBTEST(test_acosh, NULL);
    TEST_RUN_SUBTEST(test_atanh, NULL);
    TEST_RUN_SUBTEST(test_exp, NULL);
    TEST_RUN_SUBTEST(test_log, NULL);
    TEST_RUN_SUBTEST(test_log10, NULL);
    TEST_RUN_SUBTEST(test_sqrt, NULL);
    TEST_RUN_SUBTEST(test_floor, NULL);
    TEST_RUN_SUBTEST(test_ceil, NULL);
    TEST_RUN_SUBTEST(test_pow_d, NULL);
    TEST_RUN_SUBTEST(test_pow_d_complex, NULL);
    TEST_RUN_SUBTEST(test_pow, NULL);
    TEST_RUN_SUBTEST(test_abs, NULL);
    TEST_RUN_SUBTEST(test_hypot, NULL);
    TEST_RUN_SUBTEST(test_erf, NULL);
    TEST_RUN_SUBTEST(test_erfc, NULL);
    TEST_RUN_SUBTEST(test_erfinv, NULL);
    TEST_RUN_SUBTEST(test_erfcinv, NULL);
    TEST_RUN_SUBTEST(test_gamma, NULL);
    TEST_RUN_SUBTEST(test_gammainv, NULL);
    TEST_RUN_SUBTEST(test_lgamma, NULL);
    TEST_RUN_SUBTEST(test_digamma, NULL);
    TEST_RUN_SUBTEST(test_trigamma, NULL);
    TEST_RUN_SUBTEST(test_polygamma, NULL);
    TEST_RUN_SUBTEST(test_lambert_w0, NULL);
    TEST_RUN_SUBTEST(test_lambert_wm1, NULL);
    TEST_RUN_SUBTEST(test_normal_pdf, NULL);
    TEST_RUN_SUBTEST(test_normal_cdf, NULL);
    TEST_RUN_SUBTEST(test_normal_logpdf, NULL);
    TEST_RUN_SUBTEST(test_ei, NULL);
    TEST_RUN_SUBTEST(test_e1, NULL);
    TEST_RUN_SUBTEST(test_beta, NULL);
    TEST_RUN_SUBTEST(test_logbeta, NULL);
    TEST_RUN_SUBTEST(test_gammainc, NULL);
    TEST_RUN_SUBTEST(test_beta_pdf, NULL);
    TEST_RUN_SUBTEST(test_logbeta_pdf, NULL);
    TEST_RUN_SUBTEST(test_binomial, NULL);
}
