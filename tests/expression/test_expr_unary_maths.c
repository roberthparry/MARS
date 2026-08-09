#include "test_expr.h"

void test_sin(void)
{
    expr_t *p = test_expr_new_var_d(0.5);
    expr_t *f = expr_sin(p);

    check_q_at(__FILE__, __LINE__, 1, "sin(0.5)", expr_eval_qf(f), qf_sin(qf_from_double(0.5)));
    print_expr_of(f);

    expr_free(f);
    expr_free(p);
}

void test_cos(void)
{
    expr_t *c = test_expr_new_var_d(0.5);
    expr_t *f = expr_cos(c);

    check_q_at(__FILE__, __LINE__, 1, "cos(0.5)", expr_eval_qf(f), qf_cos(qf_from_double(0.5)));
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

void test_tan(void)
{
    expr_t *c = test_expr_new_var_d(0.5);
    expr_t *f = expr_tan(c);

    check_q_at(__FILE__, __LINE__, 1, "tan(0.5)", expr_eval_qf(f), qf_tan(qf_from_double(0.5)));
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

static void check_expr_string_qf(const char *label,
                                 const char *input,
                                 qfloat_t expected)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string(input, &bindings);

    ASSERT_NOT_NULL(expr);
    check_q_at(__FILE__, __LINE__, 1, label, expr_eval_qf(expr), expected);

    expr_free(expr);
    expr_bindings_free(bindings);
}

void test_haversine_family(void)
{
    check_expr_string_qf("versin(0) = 0",
                         "{ versin(0) }", QF_ZERO);
    check_expr_string_qf("vercos(0) = 2",
                         "{ vercos(0) }", QF_TWO);
    check_expr_string_qf("coversin(0) = 1",
                         "{ coversin(0) }", QF_ONE);
    check_expr_string_qf("covercos(0) = 1",
                         "{ covercos(0) }", QF_ONE);
    check_expr_string_qf("haversin(0) = 0",
                         "{ haversin(0) }", QF_ZERO);
    check_expr_string_qf("havercos(0) = 1",
                         "{ havercos(0) }", QF_ONE);
    check_expr_string_qf("hacoversin(0) = 1/2",
                         "{ hacoversin(0) }", QF_HALF);
    check_expr_string_qf("hacovercos(0) = 1/2",
                         "{ hacovercos(0) }", QF_HALF);
    check_expr_string_qf("arcversin(0) = 0",
                         "{ arcversin(0) }", QF_ZERO);
    check_expr_string_qf("arcvercos(2) = 0",
                         "{ arcvercos(2) }", QF_ZERO);
    check_expr_string_qf("arccoversin(1) = 0",
                         "{ arccoversin(1) }", QF_ZERO);
    check_expr_string_qf("arccovercos(1) = 0",
                         "{ arccovercos(1) }", QF_ZERO);
    check_expr_string_qf("archaversin(0) = 0",
                         "{ archaversin(0) }", QF_ZERO);
    check_expr_string_qf("archavercos(1) = 0",
                         "{ archavercos(1) }", QF_ZERO);
    check_expr_string_qf("archacoversin(1/2) = 0",
                         "{ archacoversin(1/2) }", QF_ZERO);
    check_expr_string_qf("archacovercos(1/2) = 0",
                         "{ archacovercos(1/2) }", QF_ZERO);
}

void test_sinh(void)
{
    expr_t *c = test_expr_new_var_d(0.5);
    expr_t *f = expr_sinh(c);

    check_q_at(__FILE__, __LINE__, 1, "sinh(0.5)", expr_eval_qf(f), qf_sinh(qf_from_double(0.5)));
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

void test_cosh(void)
{
    expr_t *c = test_expr_new_var_d(0.5);
    expr_t *f = expr_cosh(c);

    check_q_at(__FILE__, __LINE__, 1, "cosh(0.5)", expr_eval_qf(f), qf_cosh(qf_from_double(0.5)));
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

void test_tanh(void)
{
    expr_t *c = test_expr_new_const_d(0.5);
    expr_t *f = expr_tanh(c);

    check_q_at(__FILE__, __LINE__, 1, "tanh(0.5)", expr_eval_qf(f), qf_tanh(qf_from_double(0.5)));
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

void test_asin(void)
{
    expr_t *c = test_expr_new_var_d(0.25);
    expr_t *f = expr_asin(c);

    check_q_at(__FILE__, __LINE__, 1, "asin(0.25)", expr_eval_qf(f), qf_asin(qf_from_double(0.25)));
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

void test_acos(void)
{
    expr_t *c = test_expr_new_var_d(0.25);
    expr_t *f = expr_acos(c);

    check_q_at(__FILE__, __LINE__, 1, "acos(0.25)", expr_eval_qf(f), qf_acos(qf_from_double(0.25)));
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

void test_atan(void)
{
    expr_t *c = test_expr_new_var_d(0.25);
    expr_t *f = expr_atan(c);

    check_q_at(__FILE__, __LINE__, 1, "atan(0.25)", expr_eval_qf(f), qf_atan(qf_from_double(0.25)));
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

void test_atan2(void)
{
    expr_t *base = test_expr_new_var_d(2.0);
    expr_t *expo = test_expr_new_const_d(3.0);
    expr_t *f    = expr_atan2(base, expo);

    check_q_at(__FILE__, __LINE__, 1, "atan2(2,3)", expr_eval_qf(f), qf_atan2(qf_from_double(2.0), qf_from_double(3.0)));
    print_expr_of(f);

    expr_free(f);
    expr_free(expo);
    expr_free(base);
}

void test_asinh(void)
{
    expr_t *c = test_expr_new_var_d(0.25);
    expr_t *f = expr_asinh(c);

    check_q_at(__FILE__, __LINE__, 1, "asinh(0.25)", expr_eval_qf(f), qf_asinh(qf_from_double(0.25)));
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

void test_acosh(void)
{
    expr_t *c = test_expr_new_var_d(1.25);
    expr_t *f = expr_acosh(c);

    check_q_at(__FILE__, __LINE__, 1, "acosh(1.25)", expr_eval_qf(f), qf_acosh(qf_from_double(1.25)));
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

void test_atanh(void)
{
    expr_t *c = test_expr_new_var_d(0.25);
    expr_t *f = expr_atanh(c);

    check_q_at(__FILE__, __LINE__, 1, "atanh(0.25)", expr_eval_qf(f), qf_atanh(qf_from_double(0.25)));
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

void test_exp(void)
{
    expr_t *c = test_expr_new_var_d(1.5);
    expr_t *f = expr_exp(c);

    check_q_at(__FILE__, __LINE__, 1, "exp(1.5)", expr_eval_qf(f), qf_exp(qf_from_double(1.5)));
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

void test_log(void)
{
    expr_t *c = test_expr_new_var_d(1.5);
    expr_t *f = expr_log(c);

    check_q_at(__FILE__, __LINE__, 1, "log(1.5)", expr_eval_qf(f), qf_log(qf_from_double(1.5)));
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

void test_log10(void)
{
    expr_t *c = test_expr_new_var_d(1000.0);
    expr_t *f = expr_log10(c);

    check_q_at(__FILE__, __LINE__, 1, "log10(1000)", expr_eval_qf(f), qf_from_double(3.0));
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

void test_sqrt(void)
{
    expr_t *c = test_expr_new_var_d(2.0);
    expr_t *f = expr_sqrt(c);

    check_q_at(__FILE__, __LINE__, 1, "sqrt(2)", expr_eval_qf(f), qf_sqrt(qf_from_double(2.0)));
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

void test_floor(void)
{
    expr_t *c = test_expr_new_var_d(1.75);
    expr_t *f = expr_floor(c);

    check_q_at(__FILE__, __LINE__, 1, "floor(1.75)", expr_eval_qf(f), qf_from_double(1.0));
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

void test_ceil(void)
{
    expr_t *c = test_expr_new_var_d(1.25);
    expr_t *f = expr_ceil(c);

    check_q_at(__FILE__, __LINE__, 1, "ceil(1.25)", expr_eval_qf(f), qf_from_double(2.0));
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

void test_pow_d(void)
{
    expr_t *base = test_expr_new_var_d(2.0);
    expr_t *f    = expr_pow_d(base, 3.0);

    check_q_at(__FILE__, __LINE__, 1, "2^3(d)", expr_eval_qf(f), qf_pow(qf_from_double(2.0), qf_from_double(3.0)));
    print_expr_of(f);

    expr_free(f);
    expr_free(base);
}

void test_pow_d_complex(void)
{
    number_t z0 = num_create_from_string("1 + 2i");
    number_t expected = num_create_from_string("-3 + 4i");
    expr_t *base = expr_new_var(z0);
    expr_t *f = expr_pow_d(base, 2.0);
    number_t got = expr_eval(f);

    ASSERT_EXPR_NUMBER_EQ(got, expected);
    print_expr_of(f);

    num_destroy(&got);
    expr_free(f);
    expr_free(base);
    num_destroy(&expected);
    num_destroy(&z0);
}

void test_pow(void)
{
    expr_t *base = test_expr_new_var_d(2.0);
    expr_t *expo = test_expr_new_const_d(3.0);
    expr_t *f    = expr_pow_xp(base, expo);

    check_q_at(__FILE__, __LINE__, 1, "2^3", expr_eval_qf(f), qf_pow(qf_from_double(2.0), qf_from_double(3.0)));
    print_expr_of(f);

    expr_free(f);
    expr_free(expo);
    expr_free(base);
}

/* Special functions */

void test_abs(void)
{
    /* abs(-3) = 3 exactly */
    expr_t *c = test_expr_new_var_d(-3.0);
    expr_t *f = expr_abs(c);
    check_q_at(__FILE__, __LINE__, 1, "abs(-3) = 3", expr_eval_qf(f), qf_from_double(3.0));
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* abs(0.7) = 0.7 */
    c = test_expr_new_var_d(0.7);
    f = expr_abs(c);
    check_q_at(__FILE__, __LINE__, 1, "abs(0.7) = 0.7", expr_eval_qf(f), qf_from_double(0.7));
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* abs(-x) = abs(x) symmetry at x=1.5 */
    expr_t *cp = test_expr_new_const_d(1.5);
    expr_t *cn = test_expr_new_const_d(-1.5);
    expr_t *fp = expr_abs(cp);
    expr_t *fn = expr_abs(cn);
    check_q_at(__FILE__, __LINE__, 1, "abs(-1.5) = abs(1.5)", expr_eval_qf(fn), expr_eval_qf(fp));
    expr_free(fp); expr_free(fn); expr_free(cp); expr_free(cn);
}

void test_hypot(void)
{
    /* hypot(3,4) = 5 — Pythagorean triple */
    expr_t *a = test_expr_new_var_d(3.0);
    expr_t *b = test_expr_new_const_d(4.0);
    expr_t *f = expr_hypot(a, b);
    check_q_at(__FILE__, __LINE__, 1, "hypot(3,4) = 5", expr_eval_qf(f), qf_from_double(5.0));
    print_expr_of(f);
    expr_free(f); expr_free(b); expr_free(a);

    /* hypot(5,12) = 13 — Pythagorean triple */
    a = test_expr_new_var_d(5.0);
    b = test_expr_new_const_d(12.0);
    f = expr_hypot(a, b);
    check_q_at(__FILE__, __LINE__, 1, "hypot(5,12) = 13", expr_eval_qf(f), qf_from_double(13.0));
    print_expr_of(f);
    expr_free(f); expr_free(b); expr_free(a);

    /* hypot(a,b) = hypot(b,a) symmetry */
    a = test_expr_new_const_d(2.0);
    b = test_expr_new_const_d(7.0);
    expr_t *fab = expr_hypot(a, b);
    expr_t *fba = expr_hypot(b, a);
    check_q_at(__FILE__, __LINE__, 1, "hypot(2,7) = hypot(7,2)", expr_eval_qf(fab), expr_eval_qf(fba));
    expr_free(fab); expr_free(fba); expr_free(a); expr_free(b);
}

void test_maths_functions(void)
{
    TEST_RUN_SUBTEST(test_sin, NULL);
    TEST_RUN_SUBTEST(test_cos, NULL);
    TEST_RUN_SUBTEST(test_tan, NULL);
    TEST_RUN_SUBTEST(test_haversine_family, NULL);
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
    TEST_RUN_SUBTEST(test_dilog_polylog, NULL);
    TEST_RUN_SUBTEST(test_bessel, NULL);
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
