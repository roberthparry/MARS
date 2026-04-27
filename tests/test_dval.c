// test_dval.c — rewritten to use the new test harness (Option B)

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#include "test_harness.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "dval.h"
#include "qfloat.h"

#pragma GCC diagnostic ignored "-Wunused-function"

/* ------------------------------------------------------------------------- */
/* Compact qfloat_t comparison (kept exactly as-is, but using harness colours) */
/* ------------------------------------------------------------------------- */

static void check_q_at(const char *file, int line, int col,
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

static void print_expr_of(const dval_t *f)
{
    char *s = dv_to_string(f, style_EXPRESSION);
    printf(C_YELLOW "     = %s\n" C_RESET, s);
    free(s);
}

/* ------------------------------------------------------------------------- */
/* Arithmetic tests                                                          */
/* ------------------------------------------------------------------------- */

void test_add(void)
{
    dval_t *x0 = dv_new_var_d(2.0);
    dval_t *x1 = dv_new_var_d(3.0);
    dval_t *f = dv_add(x0, x1);

    check_q_at(__FILE__, __LINE__, 1, "2+3", dv_eval(f), qf_from_double(5));
    print_expr_of(f);

    dv_free(f);
    dv_free(x0);
    dv_free(x1);
}

void test_sub(void)
{
    dval_t *x0 = dv_new_var_d(10);
    dval_t *c0  = dv_new_const_d(4);
    dval_t *f   = dv_sub(x0, c0);

    check_q_at(__FILE__, __LINE__, 1, "10-4", dv_eval(f), qf_from_double(6));
    print_expr_of(f);

    dv_free(f);
    dv_free(c0);
    dv_free(x0);
}

void test_mul(void)
{
    dval_t *c7 = dv_new_var_d(7);
    dval_t *c6 = dv_new_const_d(6);
    dval_t *f  = dv_mul(c7, c6);

    check_q_at(__FILE__, __LINE__, 1, "7*6", dv_eval(f), qf_from_double(42));
    print_expr_of(f);

    dv_free(f);
    dv_free(c6);
    dv_free(c7);
}

void test_div(void)
{
    dval_t *c7  = dv_new_var_d(7);
    dval_t *c22 = dv_new_const_d(22);
    dval_t *f   = dv_div(c22, c7);

    check_q_at(__FILE__, __LINE__, 1, "22/7", dv_eval(f), qf_div(qf_from_double(22), qf_from_double(7)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c22);
    dv_free(c7);
}

void test_mixed(void)
{
    dval_t *two   = dv_new_var_d(2);
    dval_t *three = dv_new_var_d(3);
    dval_t *ten   = dv_new_var_d(10);
    dval_t *four  = dv_new_var_d(4);

    dval_t *add_2_3 = dv_add(two, three);
    dval_t *sub_10_4 = dv_sub(ten, four);
    dval_t *mul_add_sub = dv_mul(add_2_3, sub_10_4);
    dval_t *f = dv_div(mul_add_sub, two);

    check_q_at(__FILE__, __LINE__, 1, "mixed", dv_eval(f), qf_from_double(15));
    print_expr_of(f);

    dv_free(f);
    dv_free(mul_add_sub);
    dv_free(sub_10_4);
    dv_free(add_2_3);
    dv_free(two);
    dv_free(three);
    dv_free(ten);
    dv_free(four);
}

/* ------------------------------------------------------------------------- */
/* _d variants                                                                */
/* ------------------------------------------------------------------------- */

void test_add_d(void)
{
    dval_t *ten = dv_new_const_d(10);
    dval_t *f   = dv_add_d(ten, 2.5);

    check_q_at(__FILE__, __LINE__, 1, "10+2.5", dv_eval(f), qf_from_double(12.5));
    print_expr_of(f);

    dv_free(f);
    dv_free(ten);
}

void test_mul_d(void)
{
    dval_t *three = dv_new_const_d(3);
    dval_t *f     = dv_mul_d(three, 4);

    check_q_at(__FILE__, __LINE__, 1, "3*4", dv_eval(f), qf_from_double(12));
    print_expr_of(f);

    dv_free(f);
    dv_free(three);
}

void test_div_d(void)
{
    dval_t *nine = dv_new_const_d(9);
    dval_t *f    = dv_div_d(nine, 3);

    check_q_at(__FILE__, __LINE__, 1, "9/3", dv_eval(f), qf_from_double(3));
    print_expr_of(f);

    dv_free(f);
    dv_free(nine);
}

/* ------------------------------------------------------------------------- */
/* Mathematical function tests                                                */
/* ------------------------------------------------------------------------- */

void test_sin(void)
{
    dval_t *p = dv_new_var_d(0.5);
    dval_t *f = dv_sin(p);

    check_q_at(__FILE__, __LINE__, 1, "sin(0.5)", dv_eval(f), qf_sin(qf_from_double(0.5)));
    print_expr_of(f);

    dv_free(f);
    dv_free(p);
}

void test_cos(void)
{
    dval_t *c = dv_new_var_d(0.5);
    dval_t *f = dv_cos(c);

    check_q_at(__FILE__, __LINE__, 1, "cos(0.5)", dv_eval(f), qf_cos(qf_from_double(0.5)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_tan(void)
{
    dval_t *c = dv_new_var_d(0.5);
    dval_t *f = dv_tan(c);

    check_q_at(__FILE__, __LINE__, 1, "tan(0.5)", dv_eval(f), qf_tan(qf_from_double(0.5)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_sinh(void)
{
    dval_t *c = dv_new_var_d(0.5);
    dval_t *f = dv_sinh(c);

    check_q_at(__FILE__, __LINE__, 1, "sinh(0.5)", dv_eval(f), qf_sinh(qf_from_double(0.5)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_cosh(void)
{
    dval_t *c = dv_new_var_d(0.5);
    dval_t *f = dv_cosh(c);

    check_q_at(__FILE__, __LINE__, 1, "cosh(0.5)", dv_eval(f), qf_cosh(qf_from_double(0.5)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_tanh(void)
{
    dval_t *c = dv_new_const_d(0.5);
    dval_t *f = dv_tanh(c);

    check_q_at(__FILE__, __LINE__, 1, "tanh(0.5)", dv_eval(f), qf_tanh(qf_from_double(0.5)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_asin(void)
{
    dval_t *c = dv_new_var_d(0.25);
    dval_t *f = dv_asin(c);

    check_q_at(__FILE__, __LINE__, 1, "asin(0.25)", dv_eval(f), qf_asin(qf_from_double(0.25)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_acos(void)
{
    dval_t *c = dv_new_var_d(0.25);
    dval_t *f = dv_acos(c);

    check_q_at(__FILE__, __LINE__, 1, "acos(0.25)", dv_eval(f), qf_acos(qf_from_double(0.25)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_atan(void)
{
    dval_t *c = dv_new_var_d(0.25);
    dval_t *f = dv_atan(c);

    check_q_at(__FILE__, __LINE__, 1, "atan(0.25)", dv_eval(f), qf_atan(qf_from_double(0.25)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_atan2(void)
{
    dval_t *base = dv_new_var_d(2.0);
    dval_t *expo = dv_new_const_d(3.0);
    dval_t *f    = dv_atan2(base, expo);

    check_q_at(__FILE__, __LINE__, 1, "atan2(2,3)", dv_eval(f), qf_atan2(qf_from_double(2.0), qf_from_double(3.0)));
    print_expr_of(f);

    dv_free(f);
    dv_free(expo);
    dv_free(base);
}

void test_asinh(void)
{
    dval_t *c = dv_new_var_d(0.25);
    dval_t *f = dv_asinh(c);

    check_q_at(__FILE__, __LINE__, 1, "asinh(0.25)", dv_eval(f), qf_asinh(qf_from_double(0.25)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_acosh(void)
{
    dval_t *c = dv_new_var_d(1.25);
    dval_t *f = dv_acosh(c);

    check_q_at(__FILE__, __LINE__, 1, "acosh(1.25)", dv_eval(f), qf_acosh(qf_from_double(1.25)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_atanh(void)
{
    dval_t *c = dv_new_var_d(0.25);
    dval_t *f = dv_atanh(c);

    check_q_at(__FILE__, __LINE__, 1, "atanh(0.25)", dv_eval(f), qf_atanh(qf_from_double(0.25)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_exp(void)
{
    dval_t *c = dv_new_var_d(1.5);
    dval_t *f = dv_exp(c);

    check_q_at(__FILE__, __LINE__, 1, "exp(1.5)", dv_eval(f), qf_exp(qf_from_double(1.5)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_log(void)
{
    dval_t *c = dv_new_var_d(1.5);
    dval_t *f = dv_log(c);

    check_q_at(__FILE__, __LINE__, 1, "log(1.5)", dv_eval(f), qf_log(qf_from_double(1.5)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_sqrt(void)
{
    dval_t *c = dv_new_var_d(2.0);
    dval_t *f = dv_sqrt(c);

    check_q_at(__FILE__, __LINE__, 1, "sqrt(2)", dv_eval(f), qf_sqrt(qf_from_double(2.0)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c);
}

void test_pow_d(void)
{
    dval_t *base = dv_new_var_d(2.0);
    dval_t *f    = dv_pow_d(base, 3.0);

    check_q_at(__FILE__, __LINE__, 1, "2^3(d)", dv_eval(f), qf_pow(qf_from_double(2.0), qf_from_double(3.0)));
    print_expr_of(f);

    dv_free(f);
    dv_free(base);
}

void test_pow(void)
{
    dval_t *base = dv_new_var_d(2.0);
    dval_t *expo = dv_new_const_d(3.0);
    dval_t *f    = dv_pow(base, expo);

    check_q_at(__FILE__, __LINE__, 1, "2^3", dv_eval(f), qf_pow(qf_from_double(2.0), qf_from_double(3.0)));
    print_expr_of(f);

    dv_free(f);
    dv_free(expo);
    dv_free(base);
}

/* Special functions */

void test_abs(void)
{
    /* abs(-3) = 3 exactly */
    dval_t *c = dv_new_var_d(-3.0);
    dval_t *f = dv_abs(c);
    check_q_at(__FILE__, __LINE__, 1, "abs(-3) = 3", dv_eval(f), qf_from_double(3.0));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* abs(0.7) = 0.7 */
    c = dv_new_var_d(0.7);
    f = dv_abs(c);
    check_q_at(__FILE__, __LINE__, 1, "abs(0.7) = 0.7", dv_eval(f), qf_from_double(0.7));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* abs(-x) = abs(x) symmetry at x=1.5 */
    dval_t *cp = dv_new_const_d(1.5);
    dval_t *cn = dv_new_const_d(-1.5);
    dval_t *fp = dv_abs(cp);
    dval_t *fn = dv_abs(cn);
    check_q_at(__FILE__, __LINE__, 1, "abs(-1.5) = abs(1.5)", dv_eval(fn), dv_eval(fp));
    dv_free(fp); dv_free(fn); dv_free(cp); dv_free(cn);
}

void test_hypot(void)
{
    /* hypot(3,4) = 5 — Pythagorean triple */
    dval_t *a = dv_new_var_d(3.0);
    dval_t *b = dv_new_const_d(4.0);
    dval_t *f = dv_hypot(a, b);
    check_q_at(__FILE__, __LINE__, 1, "hypot(3,4) = 5", dv_eval(f), qf_from_double(5.0));
    print_expr_of(f);
    dv_free(f); dv_free(b); dv_free(a);

    /* hypot(5,12) = 13 — Pythagorean triple */
    a = dv_new_var_d(5.0);
    b = dv_new_const_d(12.0);
    f = dv_hypot(a, b);
    check_q_at(__FILE__, __LINE__, 1, "hypot(5,12) = 13", dv_eval(f), qf_from_double(13.0));
    print_expr_of(f);
    dv_free(f); dv_free(b); dv_free(a);

    /* hypot(a,b) = hypot(b,a) symmetry */
    a = dv_new_const_d(2.0);
    b = dv_new_const_d(7.0);
    dval_t *fab = dv_hypot(a, b);
    dval_t *fba = dv_hypot(b, a);
    check_q_at(__FILE__, __LINE__, 1, "hypot(2,7) = hypot(7,2)", dv_eval(fab), dv_eval(fba));
    dv_free(fab); dv_free(fba); dv_free(a); dv_free(b);
}

void test_erf(void)
{
    /* erf(0) = 0 exactly */
    dval_t *c = dv_new_var_d(0.0);
    dval_t *f = dv_erf(c);
    check_q_at(__FILE__, __LINE__, 1, "erf(0) = 0", dv_eval(f), qf_from_double(0.0));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* erf(-x) = -erf(x) odd symmetry at x=0.8 */
    dval_t *cp = dv_new_const_d(0.8);
    dval_t *cn = dv_new_const_d(-0.8);
    dval_t *fp = dv_erf(cp);
    dval_t *fn = dv_erf(cn);
    check_q_at(__FILE__, __LINE__, 1, "erf(-0.8) = -erf(0.8)",
               dv_eval(fn), qf_neg(dv_eval(fp)));
    dv_free(fp); dv_free(fn); dv_free(cp); dv_free(cn);

    /* erf(x) + erfc(x) = 1 identity at x=0.6 */
    qfloat_t X   = qf_from_string("0.6");
    qfloat_t sum = qf_add(qf_erf(X), qf_erfc(X));
    check_q_at(__FILE__, __LINE__, 1, "erf(0.6) + erfc(0.6) = 1", sum, qf_from_double(1.0));
}

void test_erfc(void)
{
    /* erfc(0) = 1 exactly */
    dval_t *c = dv_new_var_d(0.0);
    dval_t *f = dv_erfc(c);
    check_q_at(__FILE__, __LINE__, 1, "erfc(0) = 1", dv_eval(f), qf_from_double(1.0));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* erfc(x) = 1 - erf(x) at x=1.2 */
    qfloat_t X   = qf_from_double(1.2);
    qfloat_t lhs = qf_erfc(X);
    qfloat_t rhs = qf_sub(qf_from_double(1.0), qf_erf(X));
    check_q_at(__FILE__, __LINE__, 1, "erfc(1.2) = 1 - erf(1.2)", lhs, rhs);

    /* erfc(-x) = 2 - erfc(x) at x=0.5 */
    X   = qf_from_double(0.5);
    lhs = qf_erfc(qf_neg(X));
    rhs = qf_sub(qf_from_double(2.0), qf_erfc(X));
    check_q_at(__FILE__, __LINE__, 1, "erfc(-0.5) = 2 - erfc(0.5)", lhs, rhs);
}

void test_erfinv(void)
{
    /* erfinv(0) = 0 exactly */
    dval_t *c = dv_new_var_d(0.0);
    dval_t *f = dv_erfinv(c);
    check_q_at(__FILE__, __LINE__, 1, "erfinv(0) = 0", dv_eval(f), qf_from_double(0.0));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* erf(erfinv(y)) = y  (round-trip) at y=0.7 */
    qfloat_t Y  = qf_div(qf_from_double(7.0), qf_from_double(10.0));
    qfloat_t rt = qf_erf(qf_erfinv(Y));
    check_q_at(__FILE__, __LINE__, 1, "erf(erfinv(0.7)) = 0.7", rt, Y);

    /* erfinv(-y) = -erfinv(y) odd symmetry */
    Y         = qf_from_double(0.5);
    qfloat_t lhs = qf_erfinv(qf_neg(Y));
    qfloat_t rhs = qf_neg(qf_erfinv(Y));
    check_q_at(__FILE__, __LINE__, 1, "erfinv(-0.5) = -erfinv(0.5)", lhs, rhs);
}

void test_erfcinv(void)
{
    /* erfcinv(1) = 0 exactly (erfc(0) = 1) */
    dval_t *c = dv_new_var_d(1.0);
    dval_t *f = dv_erfcinv(c);
    check_q_at(__FILE__, __LINE__, 1, "erfcinv(1) = 0", dv_eval(f), qf_from_double(0.0));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* erfc(erfcinv(y)) = y (round-trip) at y=0.4 */
    qfloat_t Y  = qf_div(qf_from_double(4.0), qf_from_double(10.0));
    qfloat_t rt = qf_erfc(qf_erfcinv(Y));
    check_q_at(__FILE__, __LINE__, 1, "erfc(erfcinv(0.4)) = 0.4", rt, Y);

    /* erfcinv(y) = erfinv(1-y) identity at y=0.6 */
    Y          = qf_from_string("0.6");
    qfloat_t lhs = qf_erfcinv(Y);
    qfloat_t rhs = qf_erfinv(qf_sub(qf_from_double(1.0), Y));
    check_q_at(__FILE__, __LINE__, 1, "erfcinv(0.6) = erfinv(0.4)", lhs, rhs);
}

void test_gamma(void)
{
    /* Γ(1) = 0! = 1 exactly */
    dval_t *c = dv_new_var_d(1.0);
    dval_t *f = dv_gamma(c);
    check_q_at(__FILE__, __LINE__, 1, "gamma(1) = 1", dv_eval(f), qf_from_double(1.0));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* Γ(3) = 2! = 2 exactly */
    c = dv_new_var_d(3.0);
    f = dv_gamma(c);
    check_q_at(__FILE__, __LINE__, 1, "gamma(3) = 2", dv_eval(f), qf_from_double(2.0));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* Γ(0.5) = sqrt(π) */
    c = dv_new_var_d(0.5);
    f = dv_gamma(c);
    check_q_at(__FILE__, __LINE__, 1, "gamma(0.5) = sqrt(pi)", dv_eval(f), qf_sqrt(QF_PI));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* Γ(x+1) = x·Γ(x) recurrence at x=2.5 */
    qfloat_t X   = qf_from_double(2.5);
    qfloat_t lhs = qf_gamma(qf_add(X, qf_from_double(1.0)));
    qfloat_t rhs = qf_mul(X, qf_gamma(X));
    check_q_at(__FILE__, __LINE__, 1, "gamma(3.5) = 2.5*gamma(2.5)", lhs, rhs);
}

void test_lgamma(void)
{
    /* lgamma(1) = log(1) = 0 exactly */
    dval_t *c = dv_new_var_d(1.0);
    dval_t *f = dv_lgamma(c);
    check_q_at(__FILE__, __LINE__, 1, "lgamma(1) = 0", dv_eval(f), qf_from_double(0.0));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* lgamma(3) = log(2) */
    c = dv_new_const_d(3.0);
    f = dv_lgamma(c);
    check_q_at(__FILE__, __LINE__, 1, "lgamma(3) = log(2)",
               dv_eval(f), qf_log(qf_from_double(2.0)));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* lgamma(x) = log(gamma(x)) at x=2.5 */
    qfloat_t X   = qf_from_double(2.5);
    qfloat_t lhs = qf_lgamma(X);
    qfloat_t rhs = qf_log(qf_gamma(X));
    check_q_at(__FILE__, __LINE__, 1, "lgamma(2.5) = log(gamma(2.5))", lhs, rhs);
}

void test_digamma(void)
{
    /* ψ(2) - ψ(1) = 1 (recurrence ψ(x+1) = ψ(x) + 1/x, x=1) */
    qfloat_t d1 = qf_digamma(qf_from_double(1.0));
    qfloat_t d2 = qf_digamma(qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "digamma(2) - digamma(1) = 1",
               qf_sub(d2, d1), qf_from_double(1.0));

    /* ψ(3) - ψ(2) = 1/2 (recurrence at x=2) */
    qfloat_t d3 = qf_digamma(qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "digamma(3) - digamma(2) = 0.5",
               qf_sub(d3, d2), qf_from_double(0.5));

    /* reflection: ψ(1-x) - ψ(x) = π·cot(πx) at x=1/4 */
    qfloat_t X   = qf_from_double(0.25);
    qfloat_t lhs = qf_sub(qf_digamma(qf_sub(qf_from_double(1.0), X)), qf_digamma(X));
    qfloat_t rhs = qf_mul(QF_PI, qf_div(qf_cos(qf_mul(QF_PI, X)),
                                      qf_sin(qf_mul(QF_PI, X))));
    check_q_at(__FILE__, __LINE__, 1, "digamma(3/4) - digamma(1/4) = pi*cot(pi/4)", lhs, rhs);
}

void test_lambert_w0(void)
{
    /* W₀(0) = 0 exactly */
    dval_t *c = dv_new_var_d(0.0);
    dval_t *f = dv_lambert_w0(c);
    check_q_at(__FILE__, __LINE__, 1, "lambert_w0(0) = 0", dv_eval(f), qf_from_double(0.0));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* W₀(e) = 1 — use qfloat_t e so the input is accurate to ~33 digits */
    c = dv_new_var(qf_exp(qf_from_double(1.0)));
    f = dv_lambert_w0(c);
    check_q_at(__FILE__, __LINE__, 1, "lambert_w0(e) = 1", dv_eval(f), qf_from_double(1.0));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* W₀(x)·exp(W₀(x)) = x — defining equation verified at x=2 */
    qfloat_t X   = qf_from_double(2.0);
    qfloat_t W   = qf_lambert_w0(X);
    qfloat_t lhs = qf_mul(W, qf_exp(W));
    check_q_at(__FILE__, __LINE__, 1, "W0(2)*exp(W0(2)) = 2", lhs, X);
}

void test_lambert_wm1(void)
{
    /* W_{-1}(x)·exp(W_{-1}(x)) = x — defining equation at x=-0.1 */
    qfloat_t X   = qf_from_string("-0.1");
    qfloat_t W   = qf_lambert_wm1(X);
    qfloat_t lhs = qf_mul(W, qf_exp(W));
    check_q_at(__FILE__, __LINE__, 1, "Wm1(-0.1)*exp(Wm1(-0.1)) = -0.1", lhs, X);

    /* W_{-1}(x)·exp(W_{-1}(x)) = x — defining equation at x=-0.3 */
    X   = qf_from_string("-0.3");
    W   = qf_lambert_wm1(X);
    lhs = qf_mul(W, qf_exp(W));
    check_q_at(__FILE__, __LINE__, 1, "Wm1(-0.3)*exp(Wm1(-0.3)) = -0.3", lhs, X);
}

void test_normal_pdf(void)
{
    /* phi(0) = 1/sqrt(2π) exactly */
    dval_t *c = dv_new_var_d(0.0);
    dval_t *f = dv_normal_pdf(c);
    qfloat_t expect = qf_div(qf_from_double(1.0),
                           qf_sqrt(qf_mul(qf_from_double(2.0), QF_PI)));
    check_q_at(__FILE__, __LINE__, 1, "normal_pdf(0) = 1/sqrt(2pi)", dv_eval(f), expect);
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* phi(x) = exp(-x^2/2)/sqrt(2pi) at x=1 */
    qfloat_t X   = qf_from_double(1.0);
    qfloat_t lhs = qf_normal_pdf(X);
    qfloat_t rhs = qf_div(qf_exp(qf_div(qf_neg(qf_mul(X, X)), qf_from_double(2.0))),
                        qf_sqrt(qf_mul(qf_from_double(2.0), QF_PI)));
    check_q_at(__FILE__, __LINE__, 1, "normal_pdf(1) = exp(-1/2)/sqrt(2pi)", lhs, rhs);

    /* phi(-x) = phi(x) even symmetry at x=0.8 */
    X = qf_from_string("0.8");
    check_q_at(__FILE__, __LINE__, 1, "normal_pdf(-0.8) = normal_pdf(0.8)",
               qf_normal_pdf(qf_neg(X)), qf_normal_pdf(X));
}

void test_normal_cdf(void)
{
    /* Φ(0) = 0.5 exactly by symmetry */
    dval_t *c = dv_new_var_d(0.0);
    dval_t *f = dv_normal_cdf(c);
    check_q_at(__FILE__, __LINE__, 1, "normal_cdf(0) = 0.5", dv_eval(f), qf_from_double(0.5));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* Φ(-x) + Φ(x) = 1 reflection at x=1 */
    qfloat_t X   = qf_from_double(1.0);
    qfloat_t sum = qf_add(qf_normal_cdf(qf_neg(X)), qf_normal_cdf(X));
    check_q_at(__FILE__, __LINE__, 1, "Phi(-1) + Phi(1) = 1", sum, qf_from_double(1.0));

    /* Φ(x) = 0.5*(1 + erf(x/sqrt(2))) at x=1.5 */
    X          = qf_from_double(1.5);
    qfloat_t lhs = qf_normal_cdf(X);
    qfloat_t rhs = qf_mul(qf_from_double(0.5),
                        qf_add(qf_from_double(1.0),
                               qf_erf(qf_div(X, qf_sqrt(qf_from_double(2.0))))));
    check_q_at(__FILE__, __LINE__, 1, "Phi(1.5) = 0.5*(1+erf(1.5/sqrt2))", lhs, rhs);
}

void test_normal_logpdf(void)
{
    /* log phi(0) = -0.5*log(2pi) */
    qfloat_t expect = qf_neg(qf_mul(qf_from_double(0.5),
                                  qf_log(qf_mul(qf_from_double(2.0), QF_PI))));
    dval_t *c = dv_new_var_d(0.0);
    dval_t *f = dv_normal_logpdf(c);
    check_q_at(__FILE__, __LINE__, 1, "normal_logpdf(0) = -log(2pi)/2", dv_eval(f), expect);
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* log phi(x) = log(phi(0)) - x^2/2  at x=1.2 */
    qfloat_t X   = qf_from_double(1.2);
    qfloat_t lhs = qf_normal_logpdf(X);
    qfloat_t rhs = qf_sub(qf_normal_logpdf(qf_from_double(0.0)),
                        qf_mul(qf_from_double(0.5), qf_mul(X, X)));
    check_q_at(__FILE__, __LINE__, 1, "logpdf(1.2) = logpdf(0) - 1.2^2/2", lhs, rhs);

    /* log phi(x) = log(phi(x))  consistency at x=0.5 */
    X   = qf_from_double(0.5);
    lhs = qf_normal_logpdf(X);
    rhs = qf_log(qf_normal_pdf(X));
    check_q_at(__FILE__, __LINE__, 1, "normal_logpdf(0.5) = log(normal_pdf(0.5))", lhs, rhs);
}

void test_ei(void)
{
    /* Verify Ei against qf_ei at x=1 and x=2 */
    qfloat_t X1 = qf_from_double(1.0);
    qfloat_t X2 = qf_from_double(2.0);
    dval_t *c1 = dv_new_var_d(1.0); dval_t *f1 = dv_ei(c1);
    dval_t *c2 = dv_new_var_d(2.0); dval_t *f2 = dv_ei(c2);
    check_q_at(__FILE__, __LINE__, 1, "ei(1) via qfloat_t", dv_eval(f1), qf_ei(X1));
    check_q_at(__FILE__, __LINE__, 1, "ei(2) via qfloat_t", dv_eval(f2), qf_ei(X2));
    print_expr_of(f1);
    print_expr_of(f2);
    dv_free(f1); dv_free(c1); dv_free(f2); dv_free(c2);

    /* Ei'(x) = exp(x)/x — verify at x=1: Ei'(1) = e */
    qfloat_t deriv_at_1 = qf_exp(qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "ei'(1)=exp(1)/1=e (deriv check)",
               deriv_at_1, qf_div(qf_exp(qf_from_double(1.0)), qf_from_double(1.0)));
}

void test_e1(void)
{
    /* E₁(x) at x=1 */
    dval_t *c = dv_new_var_d(1.0);
    dval_t *f = dv_e1(c);
    check_q_at(__FILE__, __LINE__, 1, "e1(1) via qfloat_t", dv_eval(f), qf_e1(qf_from_double(1.0)));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* E₁(x) at x=0.5 */
    c = dv_new_var_d(0.5);
    f = dv_e1(c);
    check_q_at(__FILE__, __LINE__, 1, "e1(0.5) via qfloat_t",
               dv_eval(f), qf_e1(qf_from_double(0.5)));
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* E₁'(1) = -exp(-1)/1 = -1/e */
    qfloat_t deriv_e1_at_1 = qf_neg(qf_div(qf_exp(qf_neg(qf_from_double(1.0))),
                                          qf_from_double(1.0)));
    qfloat_t expect = qf_neg(qf_exp(qf_neg(qf_from_double(1.0))));
    check_q_at(__FILE__, __LINE__, 1, "e1'(1) = -exp(-1)/1 = -1/e", deriv_e1_at_1, expect);
}

void test_beta(void)
{
    /* B(1,1) = 1 exactly (∫₀¹ 1 dt = 1) */
    dval_t *a = dv_new_var_d(1.0);
    dval_t *b = dv_new_const_d(1.0);
    dval_t *f = dv_beta(a, b);
    check_q_at(__FILE__, __LINE__, 1, "beta(1,1) = 1", dv_eval(f), qf_from_double(1.0));
    print_expr_of(f);
    dv_free(f); dv_free(b); dv_free(a);

    /* B(2,3) = Γ(2)Γ(3)/Γ(5) = 1·2/24 = 1/12 exactly */
    a = dv_new_var_d(2.0);
    b = dv_new_const_d(3.0);
    f = dv_beta(a, b);
    check_q_at(__FILE__, __LINE__, 1, "beta(2,3) = 1/12",
               dv_eval(f), qf_div(qf_from_double(1.0), qf_from_double(12.0)));
    dv_free(f); dv_free(b); dv_free(a);

    /* B(a,b) = B(b,a) symmetry at (2,5) */
    qfloat_t A = qf_from_double(2.0), B = qf_from_double(5.0);
    check_q_at(__FILE__, __LINE__, 1, "beta(2,5) = beta(5,2)",
               qf_beta(A, B), qf_beta(B, A));

    /* B(a,b) = Γ(a)Γ(b)/Γ(a+b) at (1.5, 2.5) */
    A          = qf_from_double(1.5);
    B          = qf_from_double(2.5);
    qfloat_t lhs = qf_beta(A, B);
    qfloat_t rhs = qf_div(qf_mul(qf_gamma(A), qf_gamma(B)),
                        qf_gamma(qf_add(A, B)));
    check_q_at(__FILE__, __LINE__, 1, "beta(1.5,2.5) = Gamma(1.5)*Gamma(2.5)/Gamma(4)", lhs, rhs);
}

void test_logbeta(void)
{
    /* logbeta(1,1) = log(1) = 0 exactly */
    dval_t *a = dv_new_var_d(1.0);
    dval_t *b = dv_new_const_d(1.0);
    dval_t *f = dv_logbeta(a, b);
    check_q_at(__FILE__, __LINE__, 1, "logbeta(1,1) = 0", dv_eval(f), qf_from_double(0.0));
    print_expr_of(f);
    dv_free(f); dv_free(b); dv_free(a);

    /* logbeta(2,3) = log(1/12) = -log(12) */
    a = dv_new_var_d(2.0);
    b = dv_new_const_d(3.0);
    f = dv_logbeta(a, b);
    check_q_at(__FILE__, __LINE__, 1, "logbeta(2,3) = -log(12)",
               dv_eval(f), qf_neg(qf_log(qf_from_double(12.0))));
    print_expr_of(f);
    dv_free(f); dv_free(b); dv_free(a);

    /* logbeta(a,b) = log(beta(a,b)) at (3,2) */
    qfloat_t A   = qf_from_double(3.0), B = qf_from_double(2.0);
    qfloat_t lhs = qf_logbeta(A, B);
    qfloat_t rhs = qf_log(qf_beta(A, B));
    check_q_at(__FILE__, __LINE__, 1, "logbeta(3,2) = log(beta(3,2))", lhs, rhs);

    /* logbeta(a,b) = lgamma(a)+lgamma(b)-lgamma(a+b) at (1.5,2.5) */
    A   = qf_from_double(1.5);
    B   = qf_from_double(2.5);
    lhs = qf_logbeta(A, B);
    rhs = qf_sub(qf_add(qf_lgamma(A), qf_lgamma(B)),
                 qf_lgamma(qf_add(A, B)));
    check_q_at(__FILE__, __LINE__, 1, "logbeta(1.5,2.5) = lgamma(1.5)+lgamma(2.5)-lgamma(4)", lhs, rhs);
}

void test_trigamma(void)
{
    /* ψ'(1) = π²/6  — exact, classical result */
    dval_t *c = dv_new_var_d(1.0);
    dval_t *f = dv_trigamma(c);
    qfloat_t expect = qf_div(qf_mul(QF_PI, QF_PI), qf_from_double(6.0));
    check_q_at(__FILE__, __LINE__, 1, "trigamma(1) = pi^2/6", dv_eval(f), expect);
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* ψ'(2) = π²/6 - 1  (recurrence ψ'(2) = ψ'(1) - 1/1²) */
    c = dv_new_var_d(2.0);
    f = dv_trigamma(c);
    expect = qf_sub(qf_div(qf_mul(QF_PI, QF_PI), qf_from_double(6.0)),
                    qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "trigamma(2) = pi^2/6 - 1", dv_eval(f), expect);
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* ψ'(1/2) = π²/2  — exact, reflection formula */
    c = dv_new_const_d(0.5);
    f = dv_trigamma(c);
    expect = qf_div(qf_mul(QF_PI, QF_PI), qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "trigamma(1/2) = pi^2/2", dv_eval(f), expect);
    print_expr_of(f);
    dv_free(f); dv_free(c);

    /* Recurrence: ψ'(x) - ψ'(x+1) = 1/x² at x=3: ψ'(3) - ψ'(4) = 1/9 */
    qfloat_t t3 = qf_trigamma(qf_from_double(3.0));
    qfloat_t t4 = qf_trigamma(qf_from_double(4.0));
    check_q_at(__FILE__, __LINE__, 1, "trigamma(3) - trigamma(4) = 1/9",
               qf_sub(t3, t4), qf_div(qf_from_double(1.0), qf_from_double(9.0)));
}

void test_deriv_trigamma(void)
{
    dval_t *x  = dv_new_var_d(3.0);
    dval_t *f  = dv_trigamma(x);
    const dval_t *df = dv_get_deriv(f, x);

    /* d/dx{ψ'(x)} = ψ''(x) (tetragamma) — verify via qf_tetragamma */
    qfloat_t expect = qf_tetragamma(qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "d/dx{trigamma(x)} | x=3", dv_eval(df), expect);
    print_expr_of(df);

    /* Cross-check against recurrence: ψ''(3) = ψ''(4) - 2/27 */
    qfloat_t tet4 = qf_tetragamma(qf_from_double(4.0));
    qfloat_t via_recurrence = qf_sub(tet4, qf_div(qf_from_double(2.0), qf_from_double(27.0)));
    check_q_at(__FILE__, __LINE__, 1, "ψ''(3) = ψ''(4) - 2/27 (recurrence)", expect, via_recurrence);

    dv_free(f);
    dv_free(x);
}

void test_second_deriv_digamma(void)
{
    /* digamma has a symbolic derivative (trigamma), so the second derivative
     * evaluates to qf_tetragamma(x₀) via deriv_trigamma. */
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *f   = dv_digamma(x);
    dval_t *df  = dv_create_deriv(f, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    /* d²/dx²{ψ(x)} = ψ''(x) = tetragamma(x); at x=2: ψ''(2) = ψ''(1) - 2
     * ψ''(1) = -2ζ(3) so we just use qf_tetragamma to get the expected value */
    qfloat_t expect = qf_tetragamma(qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{digamma(x)} | x=2", dv_eval(ddf), expect);
    print_expr_of(ddf);

    /* Cross-check via recurrence: ψ''(2) = ψ''(3) - 2/8 */
    qfloat_t via_recurrence = qf_sub(qf_tetragamma(qf_from_double(3.0)),
                                   qf_div(qf_from_double(2.0), qf_from_double(8.0)));
    check_q_at(__FILE__, __LINE__, 1, "ψ''(2) = ψ''(3) - 1/4 (recurrence)", expect, via_recurrence);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

/* ------------------------------------------------------------------------- */
/* First derivative tests                                                     */
/* ------------------------------------------------------------------------- */

void test_deriv_const(void)
{
    dval_t *x  = dv_new_var_d(0.0);  /* dummy wrt — const ignores it */
    dval_t *c  = dv_new_const_d(5.0);
    dval_t *f  = c;
    dval_t *df = dv_create_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{5}", dv_eval(dv_get_deriv(df, x)), qf_from_double(0.0));
    print_expr_of(df);

    dv_free(df);
    dv_free(c);
    dv_free(x);
}

void test_deriv_var(void)
{
    dval_t *x = dv_new_var_d(2.0);
    const dval_t *dx = dv_get_deriv(x, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x} | x=2", dv_eval(dx), qf_from_double(1.0));
    print_expr_of(dx);

    dv_free(x);
}

void test_deriv_neg(void)
{
    dval_t *x = dv_new_var_d(3.0);
    dval_t *f = dv_neg(x);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{-x} | x=3", dv_eval(df), qf_from_double(-1.0));
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_add_d(void)
{
    dval_t *x = dv_new_var_d(2.0);
    dval_t *f = dv_add_d(x, 5.0);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x+5} | x=2", dv_eval(df), qf_from_double(1.0));
    print_expr_of(df);

    dv_free(x);
    dv_free(f);
}

void test_deriv_mul_d(void)
{
    dval_t *x = dv_new_var_d(4.0);
    dval_t *f = dv_mul_d(x, 7.0);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{7x} | x=4", dv_eval(df), qf_from_double(7.0));
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_div_d(void)
{
    dval_t *x = dv_new_var_d(9.0);
    dval_t *f = dv_div_d(x, 3.0);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x/3} | x=9", dv_eval(df), qf_div(qf_from_double(1.0), qf_from_double(3.0)));
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_x2(void)
{
    dval_t *x = dv_new_var_d(3.0);
    dval_t *f = dv_mul(x, x);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^2} | x=3", dv_eval(df), qf_from_double(6.0));
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_pow3(void)
{
    dval_t *x = dv_new_var_d(2.0);
    dval_t *f = dv_pow_d(x, 3.0);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^3} | x=2", dv_eval(df), qf_from_double(12.0));
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^(x^2+1)} | x=2", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sin(x)} | x=0.5", dv_eval(df), qf_cos(qf_from_double(0.5)));
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_cos(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_cos(x);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{cos(x)} | x=0.5", dv_eval(df), qf_neg(qf_sin(qf_from_double(0.5))));
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{tan(x)} | x=0.5", dv_eval(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_sinh(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_sinh(x);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sinh(x)} | x=0.5", dv_eval(df), qf_cosh(qf_from_double(0.5)));
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_cosh(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_cosh(x);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{cosh(x)} | x=0.5", dv_eval(df), qf_sinh(qf_from_double(0.5)));
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{tanh(x)} | x=0.5", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{asin(x)} | x=0.25", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{acos(x)} | x=0.25", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{atan(x)} | x=0.25", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{atan2(x,1)} | x=0.25", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{asinh(x)} | x=0.25", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{acosh(x)} | x=1.25", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{atanh(x)} | x=0.25", dv_eval(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(x);
}

void test_deriv_exp(void)
{
    dval_t *x = dv_new_var_d(1.5);
    dval_t *f = dv_exp(x);
    const dval_t *df = dv_get_deriv(f, x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{exp(x)} | x=1.5", dv_eval(df), qf_exp(qf_from_double(1.5)));
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{log(x)} | x=2", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sqrt(x)} | x=4", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sin(x)*exp(x)} | x=1", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sin(x)*log(x)} | x=1.3", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{exp(x)*tanh(x)} | x=0.7", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sqrt(x)*sin(x^2)} | x=1.1", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{log(cosh(x))} | x=0.9", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^2*exp(-x)} | x=1.7", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{atan(x/sqrt(1+x^2))} | x=0.8", dv_eval(df), expect);
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
    check_q_at(__FILE__, __LINE__, 1, "d/dx{|x|} | x=0.8", dv_eval(df), qf_from_double(1.0));
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{hypot(x,4)} | x=3", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{erf(x)} | x=0.5", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{erfc(x)} | x=0.5", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{erfinv(x)} | x=0.5", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{erfcinv(x)} | x=0.5", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{gamma(x)} | x=2", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{lgamma(x)} | x=2", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{digamma(x)} | x=2", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{W0(x)} | x=1", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{Wm1(x)} | x=-0.1", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{phi(x)} | x=0.5", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{Phi(x)} | x=0.5", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{log phi(x)} | x=0.5", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{Ei(x)} | x=1", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/dx{E1(x)} | x=1", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/da{beta(a,3)} | a=2", dv_eval(df), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d/da{logbeta(a,3)} | a=2", dv_eval(df), expect);
    print_expr_of(df);

    dv_free(f);
    dv_free(bc);
    dv_free(x);
}

/* ------------------------------------------------------------------------- */
/* Second derivative tests                                                    */
/* ------------------------------------------------------------------------- */

void test_second_deriv_var(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *df  = dv_create_deriv(x, x);
    const dval_t *ddf = dv_get_deriv(df, x);

    qfloat_t expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x} | x=2", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{-x} | x=3", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x+5} | x=2", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{7x} | x=4", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x/3} | x=9", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x^2} | x=3", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x^3} | x=2", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x^(x^2+1)} | x=2", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sin(x)} | x=0.5", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{cos(x)} | x=0.5", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{tan(x)} | x=0.5", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sinh(x)} | x=0.5", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{cosh(x)} | x=0.5", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{tanh(x)} | x=0.5", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{asin(x)} | x=0.25", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{acos(x)} | x=0.25", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{atan(x)} | x=0.25", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{atan2(x,1)} | x=0.25", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{asinh(x)} | x=0.25", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{acosh(x)} | x=1.25", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{atanh(x)} | x=0.25", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{exp(x)} | x=1.5", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{log(x)} | x=2", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sqrt(x)} | x=4", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sin(x)*exp(x)} | x=1", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sin(x)*log(x)} | x=1.3", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{exp(x)*tanh(x)} | x=0.7", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sqrt(x)*sin(x^2)} | x=1.1", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{log(cosh(x))} | x=0.9", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x^2*exp(-x)} | x=1.7", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{atan(x/sqrt(1+x^2))} | x=0.8", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{|x|} | x=0.8", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{hypot(x,4)} | x=3", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{erf(x)} | x=0.5", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{erfc(x)} | x=0.5", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{gamma(x)} | x=2", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{lgamma(x)} | x=2", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{phi(x)} | x=0.5", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{Phi(x)} | x=0.5", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{log phi(x)} | x=0.5", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{Ei(x)} | x=1", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{E1(x)} | x=1", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{erfinv(x)} | x=0.5", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{erfcinv(x)} | x=0.5", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{W0(x)} | x=1", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{Wm1(x)} | x=-0.1", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/da²{beta(a,3)} | a=2", dv_eval(ddf), expect);
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

    check_q_at(__FILE__, __LINE__, 1, "d²/da²{logbeta(a,3)} | a=2", dv_eval(ddf), expect);
    print_expr_of(ddf);

    dv_free(df);
    dv_free(f);
    dv_free(bc);
    dv_free(x);
}

/* ------------------------------------------------------------------------- */
/* dv_to_string Tests                                                        */
/* ------------------------------------------------------------------------- */

/* Detect whether a string is multiline */
static int is_multiline(const char *s)
{
    return s && strchr(s, '\n');
}

/* Print aligned multiline blocks */
static void print_multiline(const char *label, const char *s)
{
    /* Pad label to fixed width so got/expected align */
    int base_indent = fprintf(stderr, "  %-8s ", label);

    if (!s) {
        fprintf(stderr, "(null)\n");
        return;
    }

    const char *p = s;
    int first = 1;

    while (*p) {
        if (!first) {
            /* indent continuation lines to same column */
            for (int i = 0; i < base_indent; i++)
                fputc(' ', stderr);
        }

        const char *nl = strchr(p, '\n');
        if (nl) {
            fwrite(p, 1, nl - p + 1, stderr);
            p = nl + 1;
        } else {
            fprintf(stderr, "%s\n", p);
            break;
        }

        first = 0;
    }
}

/* PASS with optional separator */
static void to_string_pass(const char *msg, const char *got, const char *expected)
{
    fprintf(stderr, C_BOLD C_GREEN "PASS " C_RESET "%s\n" C_RESET, msg);

    int multi = is_multiline(got) || is_multiline(expected);

    print_multiline("got", got);

    if (multi)
        fprintf(stderr, "  ───────────────────────────────\n");

    print_multiline("expected", expected);
}

static void to_string_fail(const char *file, int line, int col, const char *msg, const char *got, const char *expected)
{
    fprintf(stderr, C_BOLD C_RED "FAIL" C_RESET " %s: " C_RED "%s:%d:%d\n" C_RESET, msg, file, line, col);
    TEST_FAIL();

    int multi = is_multiline(got) || is_multiline(expected);

    print_multiline("got", got);

    if (multi)
        fprintf(stderr, "  ───────────────────────────────\n");

    print_multiline("expected", expected);
}

/* Compare two strings ignoring trailing whitespace */
static int str_eq(const char *a, const char *b)
{
    size_t la = strlen(a);
    size_t lb = strlen(b);

    while (la > 0 && (a[la-1] == '\n' || a[la-1] == '\r' || a[la-1] == ' '  || a[la-1] == '\t'))
        --la;

    while (lb > 0 && (b[lb-1] == '\n' || b[lb-1] == '\r' || b[lb-1] == ' '  || b[lb-1] == '\t'))
        --lb;

    return la == lb && memcmp(a, b, la) == 0;
}

static void test_to_string_basic_const_expr(void)
{
    dval_t *c = dv_new_const_d(3.5);
    char *got = dv_to_string(c, style_EXPRESSION);

    const char *expect = "{ c = 3.5 }";

    if (str_eq(got, expect))
        to_string_pass("basic const (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic const (EXPR)", got, expect);

    free(got);
    dv_free(c);
}

static void test_to_string_basic_const_func(void)
{
    dval_t *c = dv_new_const_d(3.5);
    char *got = dv_to_string(c, style_FUNCTION);

    const char *expect = "c = 3.5\n"
                         "return c\n";

    if (str_eq(got, expect))
        to_string_pass("basic const (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic const (FUNC)", got, expect);

    free(got);
    dv_free(c);
}

void test_to_string_basic_const(void)
{
    RUN_TEST(test_to_string_basic_const_expr, __func__);
    RUN_TEST(test_to_string_basic_const_func, __func__);
}

/* ============================================================
 * BASIC VAR
 * ============================================================ */

static void test_to_string_basic_var_expr(void)
{
    dval_t *x = dv_new_named_var_d(42.0, "x");
    char *got = dv_to_string(x, style_EXPRESSION);

    const char *expect = "{ x | x = 42 }";

    if (str_eq(got, expect))
        to_string_pass("basic var (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic var (EXPR)", got, expect);

    free(got);
    dv_free(x);
}

static void test_to_string_basic_var_func(void)
{
    dval_t *x = dv_new_named_var_d(42.0, "x");
    char *got = dv_to_string(x, style_FUNCTION);

    const char *expect = "x = 42\n"
                         "return x\n";

    if (str_eq(got, expect))
        to_string_pass("basic var (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic var (FUNC)", got, expect);

    free(got);
    dv_free(x);
}

void test_to_string_basic_var(void)
{
    RUN_TEST(test_to_string_basic_var_expr, __func__);
    RUN_TEST(test_to_string_basic_var_func, __func__);
}

/* ============================================================
 * ADDITION
 * ============================================================ */

static void test_to_string_addition_expr(void)
{
    dval_t *x = dv_new_named_var_d(1, "x");
    dval_t *y = dv_new_named_var_d(2, "y");
    dval_t *f = dv_add(x, y);

    char *got = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ x + y | x = 1, y = 2 }";

    if (str_eq(got, expect))
        to_string_pass("addition (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "addition (EXPR)", got, expect);

    free(got);
    dv_free(x);
    dv_free(y);
    dv_free(f);
}

static void test_to_string_addition_func(void)
{
    dval_t *x = dv_new_named_var_d(1, "x");
    dval_t *y = dv_new_named_var_d(2, "y");
    dval_t *f = dv_add(x, y);

    char *got = dv_to_string(f, style_FUNCTION);
    const char *expect = "x = 1\n"
                         "y = 2\n"
                         "expr(x,y) = x + y\n"
                         "return expr(x,y)\n";

    if (str_eq(got, expect))
        to_string_pass("addition (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "addition (FUNC)", got, expect);

    free(got);
    dv_free(x);
    dv_free(y);
    dv_free(f);
}

void test_to_string_addition(void)
{
    RUN_TEST(test_to_string_addition_expr, __func__);
    RUN_TEST(test_to_string_addition_func, __func__);
}

/* ============================================================
 * NESTED MUL + ADD
 * ============================================================ */

static void test_to_string_nested_mul_add_expr(void)
{
    dval_t *x = dv_new_named_var_d(2, "x");
    dval_t *y = dv_new_named_var_d(3, "y");
    dval_t *z = dv_new_named_var_d(4, "z");

    dval_t *xy = dv_mul(x, y);
    dval_t *f  = dv_add(xy, z);

    char *got = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ xy + z | x = 2, y = 3, z = 4 }";

    if (str_eq(got, expect))
        to_string_pass("nested mul+add (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "nested mul+add (EXPR)", got, expect);

    free(got);
    dv_free(xy);
    dv_free(x);
    dv_free(y);
    dv_free(z);
    dv_free(f);
}

static void test_to_string_nested_mul_add_func(void)
{
    dval_t *x = dv_new_named_var_d(2, "x");
    dval_t *y = dv_new_named_var_d(3, "y");
    dval_t *z = dv_new_named_var_d(4, "z");

    dval_t *xy = dv_mul(x, y);
    dval_t *f  = dv_add(xy, z);

    char *got = dv_to_string(f, style_FUNCTION);
    const char *expect = "x = 2\n"
                         "y = 3\n"
                         "z = 4\n"
                         "expr(x,y,z) = x*y + z\n"
                         "return expr(x,y,z)\n";

    if (str_eq(got, expect))
        to_string_pass("nested mul+add (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "nested mul+add (FUNC)", got, expect);

    free(got);
    dv_free(xy);
    dv_free(x);
    dv_free(y);
    dv_free(z);
    dv_free(f);
}

void test_to_string_nested_mul_add(void)
{
    RUN_TEST(test_to_string_nested_mul_add_expr, __func__);
    RUN_TEST(test_to_string_nested_mul_add_func, __func__);
}

static void test_to_string_atan2_expr(void)
{
    dval_t *x = dv_new_named_var_d(2, "x");
    dval_t *y = dv_new_named_var_d(3, "y");

    dval_t *f = dv_atan2(x, y);

    char *got = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ atan2(x, y) | x = 2, y = 3 }";

    if (str_eq(got, expect))
        to_string_pass("atan2 (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "atan2 (EXPR)", got, expect);

    free(got);
    dv_free(x);
    dv_free(y);
    dv_free(f);
}

static void test_to_string_atan2_func(void)
{
    dval_t *x = dv_new_named_var_d(2, "x");
    dval_t *y = dv_new_named_var_d(3, "y");

    dval_t *f = dv_atan2(x, y);

    char *got = dv_to_string(f, style_FUNCTION);
    const char *expect = "x = 2\n"
                         "y = 3\n"
                         "expr(x,y) = atan2(x, y)\n"
                         "return expr(x,y)\n";

    if (str_eq(got, expect))
        to_string_pass("atan2 (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "atan2 (FUNC)", got, expect);

    free(got);
    dv_free(x);
    dv_free(y);
    dv_free(f);
}

void test_to_string_atan2(void)
{
    RUN_TEST(test_to_string_atan2_expr, __func__);
    RUN_TEST(test_to_string_atan2_func, __func__);
}

/* ============================================================
 * POW SUPERSCRIPT
 * ============================================================ */

static void test_to_string_pow_superscript_expr(void)
{
    dval_t *x = dv_new_named_var_d(2, "x");
    dval_t *f = dv_pow_d(x, 3);

    char *got = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ x³ | x = 2 }";

    if (str_eq(got, expect))
        to_string_pass("pow superscript (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "pow superscript (EXPR)", got, expect);

    free(got);
    dv_free(x);
    dv_free(f);
}

static void test_to_string_pow_superscript_func(void)
{
    dval_t *x = dv_new_named_var_d(2, "x");
    dval_t *f = dv_pow_d(x, 3);

    char *got = dv_to_string(f, style_FUNCTION);
    const char *expect = "x = 2\n"
                         "expr(x) = x^3\n"
                         "return expr(x)\n";

    if (str_eq(got, expect))
        to_string_pass("pow superscript (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "pow superscript (FUNC)", got, expect);

    free(got);
    dv_free(x);
    dv_free(f);
}

void test_to_string_pow_superscript(void)
{
    RUN_TEST(test_to_string_pow_superscript_expr, __func__);
    RUN_TEST(test_to_string_pow_superscript_func, __func__);
}

/* ============================================================
 * UNARY SIN
 * ============================================================ */

static void test_to_string_unary_sin_expr(void)
{
    dval_t *x = dv_new_named_var_d(0.5, "x");
    dval_t *f = dv_sin(x);

    char *got = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ sin(x) | x = 0.5 }";

    if (str_eq(got, expect))
        to_string_pass("unary sin (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "unary sin (EXPR)", got, expect);

    free(got);
    dv_free(x);
    dv_free(f);
}

static void test_to_string_unary_sin_func(void)
{
    dval_t *x = dv_new_named_var_d(0.5, "x");
    dval_t *f = dv_sin(x);

    char *got = dv_to_string(f, style_FUNCTION);
    const char *expect = "x = 0.5\n"
                         "expr(x) = sin(x)\n"
                         "return expr(x)\n";

    if (str_eq(got, expect))
        to_string_pass("unary sin (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "unary sin (FUNC)", got, expect);

    free(got);
    dv_free(x);
    dv_free(f);
}

void test_to_string_unary_sin(void)
{
    RUN_TEST(test_to_string_unary_sin_expr, __func__);
    RUN_TEST(test_to_string_unary_sin_func, __func__);
}

/* ============================================================
 * FUNCTION STYLE (identity)
 * ============================================================ */

static void test_to_string_function_style_expr(void)
{
    dval_t *x = dv_new_named_var_d(10, "x");
    char *got = dv_to_string(x, style_EXPRESSION);

    const char *expect = "{ x | x = 10 }";

    if (str_eq(got, expect))
        to_string_pass("function style identity (EXPR)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "function style identity (EXPR)", got, expect);

    free(got);
    dv_free(x);
}

static void test_to_string_function_style_func(void)
{
    dval_t *x = dv_new_named_var_d(10, "x");
    char *got = dv_to_string(x, style_FUNCTION);

    const char *expect = "x = 10\n"
                         "return x\n";

    if (str_eq(got, expect))
        to_string_pass("function style identity (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "function style identity (FUNC)", got, expect);

    free(got);
    dv_free(x);
}

void test_to_string_function_style(void)
{
    RUN_TEST(test_to_string_function_style_expr, __func__);
    RUN_TEST(test_to_string_function_style_func, __func__);
}

/* ============================================================
 * SPECIAL FUNCTIONS — round-trip for all 18 new ops
 * ============================================================ */

/* check_roundtrip is defined later in the from_string section */
static void check_roundtrip(const char *label, dval_t *f, int line);

void test_to_string_special_functions(void)
{
    /* Unary functions */
    { dval_t *x = dv_new_named_var_d(-3.0, "x"); check_roundtrip("to_string: abs(x)",           dv_abs(x),           __LINE__); dv_free(x); }
    { dval_t *x = dv_new_named_var_d( 0.5, "x"); check_roundtrip("to_string: erf(x)",           dv_erf(x),           __LINE__); dv_free(x); }
    { dval_t *x = dv_new_named_var_d( 0.5, "x"); check_roundtrip("to_string: erfc(x)",          dv_erfc(x),          __LINE__); dv_free(x); }
    { dval_t *x = dv_new_named_var_d( 0.5, "x"); check_roundtrip("to_string: erfinv(x)",        dv_erfinv(x),        __LINE__); dv_free(x); }
    { dval_t *x = dv_new_named_var_d( 0.5, "x"); check_roundtrip("to_string: erfcinv(x)",       dv_erfcinv(x),       __LINE__); dv_free(x); }
    { dval_t *x = dv_new_named_var_d( 3.0, "x"); check_roundtrip("to_string: gamma(x)",         dv_gamma(x),         __LINE__); dv_free(x); }
    { dval_t *x = dv_new_named_var_d( 3.0, "x"); check_roundtrip("to_string: lgamma(x)",        dv_lgamma(x),        __LINE__); dv_free(x); }
    { dval_t *x = dv_new_named_var_d( 1.0, "x"); check_roundtrip("to_string: digamma(x)",       dv_digamma(x),       __LINE__); dv_free(x); }
    { dval_t *x = dv_new_named_var_d( 1.0, "x"); check_roundtrip("to_string: lambert_w0(x)",    dv_lambert_w0(x),    __LINE__); dv_free(x); }
    { dval_t *x = dv_new_named_var_d(-0.2, "x"); check_roundtrip("to_string: lambert_wm1(x)",   dv_lambert_wm1(x),   __LINE__); dv_free(x); }
    { dval_t *x = dv_new_named_var_d( 0.0, "x"); check_roundtrip("to_string: normal_pdf(x)",    dv_normal_pdf(x),    __LINE__); dv_free(x); }
    { dval_t *x = dv_new_named_var_d( 0.0, "x"); check_roundtrip("to_string: normal_cdf(x)",    dv_normal_cdf(x),    __LINE__); dv_free(x); }
    { dval_t *x = dv_new_named_var_d( 0.0, "x"); check_roundtrip("to_string: normal_logpdf(x)", dv_normal_logpdf(x), __LINE__); dv_free(x); }
    { dval_t *x = dv_new_named_var_d( 1.0, "x"); check_roundtrip("to_string: ei(x)",            dv_ei(x),            __LINE__); dv_free(x); }
    { dval_t *x = dv_new_named_var_d( 1.0, "x"); check_roundtrip("to_string: e1(x)",            dv_e1(x),            __LINE__); dv_free(x); }
    /* Binary functions */
    {
        dval_t *x = dv_new_named_var_d(2.0, "x");
        dval_t *y = dv_new_named_var_d(3.0, "y");
        dval_t *f = dv_beta(x, y);    dv_free(x); dv_free(y);
        check_roundtrip("to_string: beta(x,y)", f, __LINE__);
    }
    {
        dval_t *x = dv_new_named_var_d(2.0, "x");
        dval_t *y = dv_new_named_var_d(3.0, "y");
        dval_t *f = dv_logbeta(x, y); dv_free(x); dv_free(y);
        check_roundtrip("to_string: logbeta(x,y)", f, __LINE__);
    }
    {
        dval_t *x = dv_new_named_var_d(3.0, "x");
        dval_t *y = dv_new_named_var_d(4.0, "y");
        dval_t *f = dv_hypot(x, y);   dv_free(x); dv_free(y);
        check_roundtrip("to_string: hypot(x,y)", f, __LINE__);
    }
}

/* ============================================================
 * TEST SUITE RUNNER
 * ============================================================ */

void test_to_string_all(void)
{
    RUN_TEST(test_to_string_basic_const, __func__);
    RUN_TEST(test_to_string_basic_var, __func__);
    RUN_TEST(test_to_string_addition, __func__);
    RUN_TEST(test_to_string_nested_mul_add, __func__);
    RUN_TEST(test_to_string_atan2, __func__);
    RUN_TEST(test_to_string_pow_superscript, __func__);
    RUN_TEST(test_to_string_unary_sin, __func__);
    RUN_TEST(test_to_string_function_style, __func__);
    RUN_TEST(test_to_string_special_functions, __func__);
}

/* ============================================================
 *  make_expr_01        x*x
 * ============================================================ */
static dval_t *make_expr_01(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul(x, x);   /* x*x */

    dv_free(x);
    return t1;
}

/* ============================================================
 *  make_expr_02        x*x*x
 * ============================================================ */
static dval_t *make_expr_02(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul(x, x);   /* x*x      */
    dval_t *t2 = dv_mul(t1, x);  /* x*x*x    */

    dv_free(x);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_03        π * x^2
 * ============================================================ */
static dval_t *make_expr_03(void)
{
    dval_t *x   = dv_new_named_var_d(1.25, "x");
    dval_t *pi  = dv_new_named_const(QF_PI, "@pi");

    dval_t *t1 = dv_pow_d(x, 2.0);   /* x^2      */
    dval_t *t2 = dv_mul(pi, t1);     /* π * x^2  */

    dv_free(x);
    dv_free(pi);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_04        x*x + x*x
 * ============================================================ */
static dval_t *make_expr_04(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul(x, x);       /* x*x        */
    dval_t *t2 = dv_mul(x, x);       /* x*x        */
    dval_t *t3 = dv_add(t1, t2);     /* x*x + x*x  */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_05        x*x + 3*x*x + 7
 * ============================================================ */
static dval_t *make_expr_05(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul(x, x);        /* x*x          */
    dval_t *t2 = dv_mul_d(t1, 3.0);   /* 3*x*x        */
    dval_t *t3 = dv_add(t1, t2);      /* x*x+3*x*x    */
    dval_t *t4 = dv_add_d(t3, 7.0);   /* x*x+3*x*x+7  */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_06        2*x - 5*x
 * ============================================================ */
static dval_t *make_expr_06(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul_d(x, 2.0);   /* 2*x   */
    dval_t *t2 = dv_mul_d(x, 5.0);   /* 5*x   */
    dval_t *t3 = dv_sub(t1, t2);     /* 2*x-5*x */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_07        x^2 * x^3
 * ============================================================ */
static dval_t *make_expr_07(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_pow_d(x, 2.0);   /* x^2      */
    dval_t *t2 = dv_pow_d(x, 3.0);   /* x^3      */
    dval_t *t3 = dv_mul(t1, t2);     /* x^2*x^3  */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_08        x^2 * x * x^4
 * ============================================================ */
static dval_t *make_expr_08(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_pow_d(x, 2.0);   /* x^2        */
    dval_t *t2 = dv_mul(t1, x);      /* x^2*x      */
    dval_t *t3 = dv_pow_d(x, 4.0);   /* x^4        */
    dval_t *t4 = dv_mul(t2, t3);     /* x^2*x*x^4  */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_09        x^2 * y^3 * x
 * ============================================================ */
static dval_t *make_expr_09(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");
    dval_t *y = dv_new_named_var_d(1.25, "y");

    dval_t *t1 = dv_pow_d(x, 2.0);   /* x^2        */
    dval_t *t2 = dv_pow_d(y, 3.0);   /* y^3        */
    dval_t *t3 = dv_mul(t1, t2);     /* x^2*y^3    */
    dval_t *t4 = dv_mul(t3, x);      /* x^2*y^3*x  */

    dv_free(x);
    dv_free(y);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_10        3*x^2 * 4*x
 * ============================================================ */
static dval_t *make_expr_10(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_pow_d(x, 2.0);    /* x^2        */
    dval_t *t2 = dv_mul_d(t1, 3.0);   /* 3*x^2      */
    dval_t *t3 = dv_mul_d(x, 4.0);    /* 4*x        */
    dval_t *t4 = dv_mul(t2, t3);      /* 3*x^2*4*x  */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_11        3*x * 2*y * x^2   → 6 x^3 y
 * ============================================================ */
static dval_t *make_expr_11(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");
    dval_t *y = dv_new_named_var_d(1.25, "y");

    dval_t *t1 = dv_mul_d(x, 3.0);      /* 3*x     */
    dval_t *t2 = dv_mul_d(y, 2.0);      /* 2*y     */
    dval_t *t3 = dv_mul(t1, t2);        /* 3*x*2*y */
    dval_t *t4 = dv_pow_d(x, 2.0);      /* x^2     */
    dval_t *t5 = dv_mul(t3, t4);        /* 3*x*2*y*x^2 */

    dv_free(x);
    dv_free(y);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_12        x*x*y*x   → x^3 y
 * ============================================================ */
static dval_t *make_expr_12(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");
    dval_t *y = dv_new_named_var_d(1.25, "y");

    dval_t *t1 = dv_mul(x, x);      /* x*x     */
    dval_t *t2 = dv_mul(t1, y);     /* x*x*y   */
    dval_t *t3 = dv_mul(t2, x);     /* x*x*y*x */

    dv_free(x);
    dv_free(y);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_13        3*x
 * ============================================================ */
static dval_t *make_expr_13(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul_d(x, 3.0);   /* 3*x */

    dv_free(x);
    return t1;
}

/* ============================================================
 *  make_expr_14        3*x*x
 * ============================================================ */
static dval_t *make_expr_14(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul(x, x);       /* x*x   */
    dval_t *t2 = dv_mul_d(t1, 3.0);  /* 3*x*x */

    dv_free(x);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_15        6*x
 * ============================================================ */
static dval_t *make_expr_15(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul_d(x, 6.0);   /* 6*x */

    dv_free(x);
    return t1;
}

/* ============================================================
 *  make_expr_16        7*x^2
 * ============================================================ */
static dval_t *make_expr_16(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_pow_d(x, 2.0);    /* x^2     */
    dval_t *t2 = dv_mul_d(t1, 7.0);   /* 7*x^2   */

    dv_free(x);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_17        2*x*y
 * ============================================================ */
static dval_t *make_expr_17(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");
    dval_t *y = dv_new_named_var_d(1.25, "y");

    dval_t *t1 = dv_mul(x, y);        /* x*y     */
    dval_t *t2 = dv_mul_d(t1, 2.0);   /* 2*x*y   */

    dv_free(x);
    dv_free(y);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_18        sin(x) * cos(x)
 * ============================================================ */
static dval_t *make_expr_18(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_sin(x);       /* sin(x) */
    dval_t *t2 = dv_cos(x);       /* cos(x) */
    dval_t *t3 = dv_mul(t1, t2);  /* sin(x)*cos(x) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_19        cos(x) * exp(x)
 * ============================================================ */
static dval_t *make_expr_19(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_cos(x);       /* cos(x) */
    dval_t *t2 = dv_exp(x);       /* exp(x) */
    dval_t *t3 = dv_mul(t1, t2);  /* cos(x)*exp(x) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_20        exp(x) * x*x   → x^2 * exp(x)
 * ============================================================ */
static dval_t *make_expr_20(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_exp(x);       /* exp(x) */
    dval_t *t2 = dv_mul(x, x);    /* x*x    */
    dval_t *t3 = dv_mul(t2, t1);  /* x*x*exp(x) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_21        3*exp(x) * x^2
 * ============================================================ */
static dval_t *make_expr_21(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_exp(x);          /* exp(x)     */
    dval_t *t2 = dv_mul_d(t1, 3.0);  /* 3*exp(x)   */
    dval_t *t3 = dv_pow_d(x, 2.0);   /* x^2        */
    dval_t *t4 = dv_mul(t2, t3);     /* 3*exp(x)*x^2 */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_22        sin(x) * x^2
 * ============================================================ */
static dval_t *make_expr_22(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_sin(x);          /* sin(x) */
    dval_t *t2 = dv_pow_d(x, 2.0);   /* x^2    */
    dval_t *t3 = dv_mul(t1, t2);     /* sin(x)*x^2 */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_23        x*sin(x)*x   → x^2 * sin(x)
 * ============================================================ */
static dval_t *make_expr_23(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_sin(x);       /* sin(x) */
    dval_t *t2 = dv_mul(x, t1);   /* x*sin(x) */
    dval_t *t3 = dv_mul(t2, x);   /* x*sin(x)*x */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_24        exp(sin(x))
 * ============================================================ */
static dval_t *make_expr_24(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_sin(x);       /* sin(x) */
    dval_t *t2 = dv_exp(t1);      /* exp(sin(x)) */

    dv_free(x);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_25        cos(x) * exp(sin(x))
 * ============================================================ */
static dval_t *make_expr_25(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_cos(x);       /* cos(x) */
    dval_t *t2 = dv_sin(x);       /* sin(x) */
    dval_t *t3 = dv_exp(t2);      /* exp(sin(x)) */
    dval_t *t4 = dv_mul(t1, t3);  /* cos(x)*exp(sin(x)) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_26        x*x * exp(sin(x))
 * ============================================================ */
static dval_t *make_expr_26(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_mul(x, x);    /* x*x */
    dval_t *t2 = dv_sin(x);       /* sin(x) */
    dval_t *t3 = dv_exp(t2);      /* exp(sin(x)) */
    dval_t *t4 = dv_mul(t1, t3);  /* x*x*exp(sin(x)) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_27        exp(sin(x)) * exp(cos(x))
 * ============================================================ */
static dval_t *make_expr_27(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_sin(x);       /* sin(x) */
    dval_t *t2 = dv_exp(t1);      /* exp(sin(x)) */
    dval_t *t3 = dv_cos(x);       /* cos(x) */
    dval_t *t4 = dv_exp(t3);      /* exp(cos(x)) */
    dval_t *t5 = dv_mul(t2, t4);  /* exp(sin(x))*exp(cos(x)) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_28        exp(x^2) * exp(3*x^2)
 * ============================================================ */
static dval_t *make_expr_28(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_pow_d(x, 2.0);     /* x^2       */
    dval_t *t2 = dv_exp(t1);           /* exp(x^2)  */
    dval_t *t3 = dv_mul_d(t1, 3.0);    /* 3*x^2     */
    dval_t *t4 = dv_exp(t3);           /* exp(3*x^2) */
    dval_t *t5 = dv_mul(t2, t4);       /* exp(x^2)*exp(3*x^2) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_29        exp(x) * exp(2*x)
 * ============================================================ */
static dval_t *make_expr_29(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_exp(x);          /* exp(x)   */
    dval_t *t2 = dv_mul_d(x, 2.0);   /* 2*x      */
    dval_t *t3 = dv_exp(t2);         /* exp(2*x) */
    dval_t *t4 = dv_mul(t1, t3);     /* exp(x)*exp(2*x) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_30        exp(sin(x)) * exp(cos(x)) * exp(x)
 * ============================================================ */
static dval_t *make_expr_30(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_sin(x);       /* sin(x) */
    dval_t *t2 = dv_exp(t1);      /* exp(sin(x)) */
    dval_t *t3 = dv_cos(x);       /* cos(x) */
    dval_t *t4 = dv_exp(t3);      /* exp(cos(x)) */
    dval_t *t5 = dv_exp(x);       /* exp(x) */
    dval_t *t6 = dv_mul(t2, t4);  /* exp(sin(x))*exp(cos(x)) */
    dval_t *t7 = dv_mul(t6, t5);  /* exp(sin(x))*exp(cos(x))*exp(x) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    dv_free(t5);
    dv_free(t6);
    return t7;
}

/* ============================================================
 *  make_expr_31        π * sin(x)
 * ============================================================ */
static dval_t *make_expr_31(void)
{
    dval_t *x  = dv_new_named_var_d(1.25, "x");
    dval_t *pi = dv_new_named_const(QF_PI, "@pi");

    dval_t *t1 = dv_sin(x);       /* sin(x)   */
    dval_t *t2 = dv_mul(pi, t1);  /* π*sin(x) */

    dv_free(x);
    dv_free(pi);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_32        τ * cos(x)
 * ============================================================ */
static dval_t *make_expr_32(void)
{
    dval_t *x   = dv_new_named_var_d(1.25, "x");
    dval_t *tau = dv_new_named_const(QF_2PI, "@tau");

    dval_t *t1 = dv_cos(x);        /* cos(x)   */
    dval_t *t2 = dv_mul(tau, t1);  /* τ*cos(x) */

    dv_free(x);
    dv_free(tau);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_33        e * x^2
 * ============================================================ */
static dval_t *make_expr_33(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");
    dval_t *e = dv_new_named_const(QF_E, "e");

    dval_t *t1 = dv_pow_d(x, 2.0);   /* x^2    */
    dval_t *t2 = dv_mul(e, t1);      /* e*x^2  */

    dv_free(x);
    dv_free(e);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_34        π * τ * e
 * ============================================================ */
static dval_t *make_expr_34(void)
{
    dval_t *pi  = dv_new_named_const(QF_PI, "@pi");
    dval_t *tau = dv_new_named_const(QF_2PI, "@tau");
    dval_t *e   = dv_new_named_const(QF_E, "e");

    dval_t *t1 = dv_mul(pi, tau);   /* π*τ   */
    dval_t *t2 = dv_mul(t1, e);     /* π*τ*e */

    dv_free(pi);
    dv_free(tau);
    dv_free(e);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_35        π * x * τ * y
 * ============================================================ */
static dval_t *make_expr_35(void)
{
    dval_t *x   = dv_new_named_var_d(1.25, "x");
    dval_t *y   = dv_new_named_var_d(1.25, "y");
    dval_t *pi  = dv_new_named_const(QF_PI, "@pi");
    dval_t *tau = dv_new_named_const(QF_2PI, "@tau");

    dval_t *t1 = dv_mul(pi, x);      /* π*x     */
    dval_t *t2 = dv_mul(t1, tau);    /* π*x*τ   */
    dval_t *t3 = dv_mul(t2, y);      /* π*x*τ*y */

    dv_free(x);
    dv_free(y);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_36        e^(x) * π
 * ============================================================ */
static dval_t *make_expr_36(void)
{
    dval_t *x  = dv_new_named_var_d(1.25, "x");
    dval_t *pi = dv_new_named_const(QF_PI, "@pi");

    dval_t *t1 = dv_exp(x);       /* exp(x) */
    dval_t *t2 = dv_mul(t1, pi);  /* exp(x)*π */

    dv_free(x);
    dv_free(pi);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_37        τ * exp(x^2)
 * ============================================================ */
static dval_t *make_expr_37(void)
{
    dval_t *x   = dv_new_named_var_d(1.25, "x");
    dval_t *tau = dv_new_named_const(QF_2PI, "@tau");

    dval_t *t1 = dv_pow_d(x, 2.0);   /* x^2        */
    dval_t *t2 = dv_exp(t1);         /* exp(x^2)   */
    dval_t *t3 = dv_mul(tau, t2);    /* τ*exp(x^2) */

    dv_free(x);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_38        e * sin(x) * cos(y)
 * ============================================================ */
static dval_t *make_expr_38(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");
    dval_t *y = dv_new_named_var_d(1.25, "y");
    dval_t *e = dv_new_named_const(QF_E, "e");

    dval_t *t1 = dv_sin(x);       /* sin(x) */
    dval_t *t2 = dv_cos(y);       /* cos(y) */
    dval_t *t3 = dv_mul(t1, t2);  /* sin(x)*cos(y) */
    dval_t *t4 = dv_mul(e, t3);   /* e*sin(x)*cos(y) */

    dv_free(x);
    dv_free(y);
    dv_free(e);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_39        π * exp(τ * x)
 * ============================================================ */
static dval_t *make_expr_39(void)
{
    dval_t *x   = dv_new_named_var_d(1.25, "x");
    dval_t *pi  = dv_new_named_const(QF_PI, "@pi");
    dval_t *tau = dv_new_named_const(QF_2PI, "@tau");

    dval_t *t1 = dv_mul(tau, x);   /* τ*x        */
    dval_t *t2 = dv_exp(t1);       /* exp(τ*x)   */
    dval_t *t3 = dv_mul(pi, t2);   /* π*exp(τ*x) */

    dv_free(x);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_40        e^(π*x) * τ
 * ============================================================ */
static dval_t *make_expr_40(void)
{
    dval_t *x   = dv_new_named_var_d(1.25, "x");
    dval_t *pi  = dv_new_named_const(QF_PI, "@pi");
    dval_t *tau = dv_new_named_const(QF_2PI, "@tau");

    dval_t *t1 = dv_mul(pi, x);   /* π*x      */
    dval_t *t2 = dv_exp(t1);      /* exp(π*x) */
    dval_t *t3 = dv_mul(t2, tau); /* exp(π*x)*τ */

    dv_free(x);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_41        sin(π * x)
 * ============================================================ */
static dval_t *make_expr_41(void)
{
    dval_t *x  = dv_new_named_var_d(1.25, "x");
    dval_t *pi = dv_new_named_const(QF_PI, "@pi");

    dval_t *t1 = dv_mul(pi, x);   /* π*x       */
    dval_t *t2 = dv_sin(t1);      /* sin(π*x)  */

    dv_free(x);
    dv_free(pi);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_42        cos(τ * x)
 * ============================================================ */
static dval_t *make_expr_42(void)
{
    dval_t *x   = dv_new_named_var_d(1.25, "x");
    dval_t *tau = dv_new_named_const(QF_2PI, "@tau");

    dval_t *t1 = dv_mul(tau, x);  /* τ*x       */
    dval_t *t2 = dv_cos(t1);      /* cos(τ*x)  */

    dv_free(x);
    dv_free(tau);
    dv_free(t1);
    return t2;
}

/* ============================================================
 *  make_expr_43        exp(π * τ * x)
 * ============================================================ */
static dval_t *make_expr_43(void)
{
    dval_t *x   = dv_new_named_var_d(1.25, "x");
    dval_t *pi  = dv_new_named_const(QF_PI, "@pi");
    dval_t *tau = dv_new_named_const(QF_2PI, "@tau");

    dval_t *t1 = dv_mul(pi, tau);  /* π*τ      */
    dval_t *t2 = dv_mul(t1, x);    /* π*τ*x    */
    dval_t *t3 = dv_exp(t2);       /* exp(π*τ*x) */

    dv_free(x);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_44        sin(x) + cos(x) + exp(x)
 * ============================================================ */
static dval_t *make_expr_44(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");

    dval_t *t1 = dv_sin(x);       /* sin(x) */
    dval_t *t2 = dv_cos(x);       /* cos(x) */
    dval_t *t3 = dv_add(t1, t2);  /* sin(x)+cos(x) */
    dval_t *t4 = dv_exp(x);       /* exp(x) */
    dval_t *t5 = dv_add(t3, t4);  /* sin(x)+cos(x)+exp(x) */

    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_45        x + y + π + τ + e
 * ============================================================ */
static dval_t *make_expr_45(void)
{
    dval_t *x   = dv_new_named_var_d(1.25, "x");
    dval_t *y   = dv_new_named_var_d(1.25, "y");
    dval_t *pi  = dv_new_named_const(QF_PI, "@pi");
    dval_t *tau = dv_new_named_const(QF_2PI, "@tau");
    dval_t *e   = dv_new_named_const(QF_E, "e");

    dval_t *t1 = dv_add(x, y);     /* x+y     */
    dval_t *t2 = dv_add(t1, pi);   /* x+y+π   */
    dval_t *t3 = dv_add(t2, tau);  /* x+y+π+τ */
    dval_t *t4 = dv_add(t3, e);    /* x+y+π+τ+e */

    dv_free(x);
    dv_free(y);
    dv_free(pi);
    dv_free(tau);
    dv_free(e);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return t4;
}

/* ============================================================
 *  make_expr_46        x*y + π*x + τ*y + e
 * ============================================================ */
static dval_t *make_expr_46(void)
{
    dval_t *x   = dv_new_named_var_d(1.25, "x");
    dval_t *y   = dv_new_named_var_d(1.25, "y");
    dval_t *pi  = dv_new_named_const(QF_PI, "@pi");
    dval_t *tau = dv_new_named_const(QF_2PI, "@tau");
    dval_t *e   = dv_new_named_const(QF_E, "e");

    dval_t *t1 = dv_mul(x, y);     /* x*y     */
    dval_t *t2 = dv_mul(pi, x);    /* π*x     */
    dval_t *t3 = dv_mul(tau, y);   /* τ*y     */
    dval_t *t4 = dv_add(t1, t2);   /* x*y + π*x */
    dval_t *t5 = dv_add(t4, t3);   /* x*y + π*x + τ*y */
    dval_t *t6 = dv_add(t5, e);    /* x*y + π*x + τ*y + e */

    dv_free(x);
    dv_free(y);
    dv_free(pi);
    dv_free(tau);
    dv_free(e);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    dv_free(t5);
    return t6;
}

/* ============================================================
 *  make_expr_47        (x + π) * (y + τ)
 * ============================================================ */
static dval_t *make_expr_47(void)
{
    dval_t *x   = dv_new_named_var_d(1.25, "x");
    dval_t *y   = dv_new_named_var_d(1.25, "y");
    dval_t *pi  = dv_new_named_const(QF_PI, "@pi");
    dval_t *tau = dv_new_named_const(QF_2PI, "@tau");

    dval_t *t1 = dv_add(x, pi);     /* x+π */
    dval_t *t2 = dv_add(y, tau);    /* y+τ */
    dval_t *t3 = dv_mul(t1, t2);    /* (x+π)*(y+τ) */

    dv_free(x);
    dv_free(y);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    return t3;
}

/* ============================================================
 *  make_expr_48        exp(x + π) * exp(y + τ)
 * ============================================================ */
static dval_t *make_expr_48(void)
{
    dval_t *x   = dv_new_named_var_d(1.25, "x");
    dval_t *y   = dv_new_named_var_d(1.25, "y");
    dval_t *pi  = dv_new_named_const(QF_PI, "@pi");
    dval_t *tau = dv_new_named_const(QF_2PI, "@tau");

    dval_t *t1 = dv_add(x, pi);     /* x+π */
    dval_t *t2 = dv_exp(t1);        /* exp(x+π) */
    dval_t *t3 = dv_add(y, tau);    /* y+τ */
    dval_t *t4 = dv_exp(t3);        /* exp(y+τ) */
    dval_t *t5 = dv_mul(t2, t4);    /* exp(x+π)*exp(y+τ) */

    dv_free(x);
    dv_free(y);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_49        sin(x + π) * cos(y + τ)
 * ============================================================ */
static dval_t *make_expr_49(void)
{
    dval_t *x   = dv_new_named_var_d(1.25, "x");
    dval_t *y   = dv_new_named_var_d(1.25, "y");
    dval_t *pi  = dv_new_named_const(QF_PI, "@pi");
    dval_t *tau = dv_new_named_const(QF_2PI, "@tau");

    dval_t *t1 = dv_add(x, pi);     /* x+π */
    dval_t *t2 = dv_sin(t1);        /* sin(x+π) */
    dval_t *t3 = dv_add(y, tau);    /* y+τ */
    dval_t *t4 = dv_cos(t3);        /* cos(y+τ) */
    dval_t *t5 = dv_mul(t2, t4);    /* sin(x+π)*cos(y+τ) */

    dv_free(x);
    dv_free(y);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    return t5;
}

/* ============================================================
 *  make_expr_50        exp(sin(x + π) + cos(y + τ))
 * ============================================================ */
static dval_t *make_expr_50(void)
{
    dval_t *x   = dv_new_named_var_d(1.25, "x");
    dval_t *y   = dv_new_named_var_d(1.25, "y");
    dval_t *pi  = dv_new_named_const(QF_PI, "@pi");
    dval_t *tau = dv_new_named_const(QF_2PI, "@tau");

    dval_t *t1 = dv_add(x, pi);     /* x+π */
    dval_t *t2 = dv_sin(t1);        /* sin(x+π) */
    dval_t *t3 = dv_add(y, tau);    /* y+τ */
    dval_t *t4 = dv_cos(t3);        /* cos(y+τ) */
    dval_t *t5 = dv_add(t2, t4);    /* sin(x+π)+cos(y+τ) */
    dval_t *t6 = dv_exp(t5);        /* exp(sin(x+π)+cos(y+τ)) */

    dv_free(x);
    dv_free(y);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    dv_free(t4);
    dv_free(t5);
    return t6;
}

static void test_expressions(void)
{
    /* ============================================================
     *  Test table (all 50 entries)
     * ============================================================ */
    struct {
        const char *src;
        dval_t *(*make)(void);
        const char *expected_expr;
        const char *expected_func;
        int line;   /* NEW: source line of this test entry */
    } tests[] = {
        /* 01 */
        {
            "x*x",
            make_expr_01,
            "{ x² | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = x^2\n"
            "return expr(x)",
            __LINE__
        },

        /* 02 */
        {
            "x*x*x",
            make_expr_02,
            "{ x³ | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = x^3\n"
            "return expr(x)",
            __LINE__
        },

        /* 03 */
        {
            "π * x^2",
            make_expr_03,
            "{ πx² | x = 1.25; π = 3.141592653589793238462643383279505 }",
            "x = 1.25\n"
            "π = 3.141592653589793238462643383279505\n"
            "expr(x,π) = π*x^2\n"
            "return expr(x,π)",
            __LINE__
        },

        /* 04 */
        {
            "x*x + x*x",
            make_expr_04,
            "{ 2x² | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = 2*x^2\n"
            "return expr(x)",
            __LINE__
        },

        /* 05 */
        {
            "x*x + 3*x*x + 7",
            make_expr_05,
            "{ 4x² + 7 | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = 4*x^2 + 7\n"
            "return expr(x)",
            __LINE__
        },

        /* 06 */
        {
            "2*x - 5*x",
            make_expr_06,
            "{ -3x | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = -3*x\n"
            "return expr(x)",
            __LINE__
        },

        /* 07 */
        {
            "x^2 * x^3",
            make_expr_07,
            "{ x⁵ | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = x^5\n"
            "return expr(x)",
            __LINE__
        },

        /* 08 */
        {
            "x^2 * x * x^4",
            make_expr_08,
            "{ x⁷ | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = x^7\n"
            "return expr(x)",
            __LINE__
        },

        /* 09 */
        {
            "x^2 * y^3 * x",
            make_expr_09,
            "{ x³y³ | x = 1.25, y = 1.25 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "expr(x,y) = x^3*y^3\n"
            "return expr(x,y)",
            __LINE__
        },

        /* 10 */
        {
            "3*x^2 * 4*x",
            make_expr_10,
            "{ 12x³ | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = 12*x^3\n"
            "return expr(x)",
            __LINE__
        },

        /* 11 */
        {
            "3*x * 2*y * x^2",
            make_expr_11,
            "{ 6x³y | x = 1.25, y = 1.25 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "expr(x,y) = 6*x^3*y\n"
            "return expr(x,y)",
            __LINE__
        },

        /* 12 */
        {
            "x*x*y*x",
            make_expr_12,
            "{ x³y | x = 1.25, y = 1.25 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "expr(x,y) = x^3*y\n"
            "return expr(x,y)",
            __LINE__
        },

        /* 13 */
        {
            "3*x",
            make_expr_13,
            "{ 3x | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = 3*x\n"
            "return expr(x)",
            __LINE__
        },

        /* 14 */
        {
            "3*x*x",
            make_expr_14,
            "{ 3x² | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = 3*x^2\n"
            "return expr(x)",
            __LINE__
        },

        /* 15 */
        {
            "6*x",
            make_expr_15,
            "{ 6x | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = 6*x\n"
            "return expr(x)",
            __LINE__
        },

        /* 16 */
        {
            "7*x^2",
            make_expr_16,
            "{ 7x² | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = 7*x^2\n"
            "return expr(x)",
            __LINE__
        },

        /* 17 */
        {
            "2*x*y",
            make_expr_17,
            "{ 2xy | x = 1.25, y = 1.25 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "expr(x,y) = 2*x*y\n"
            "return expr(x,y)",
            __LINE__
        },

        /* 18 */
        {
            "sin(x)*cos(x)",
            make_expr_18,
            "{ sin(x)·cos(x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = sin(x)*cos(x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 19 */
        {
            "cos(x)*exp(x)",
            make_expr_19,
            "{ cos(x)·exp(x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = cos(x)*exp(x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 20 */
        {
            "exp(x)*x*x",
            make_expr_20,
            "{ x²·exp(x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = x^2*exp(x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 21 */
        {
            "3*exp(x)*x^2",
            make_expr_21,
            "{ 3x²·exp(x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = 3*x^2*exp(x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 22 */
        {
            "sin(x)*x^2",
            make_expr_22,
            "{ x²·sin(x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = x^2*sin(x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 23 */
        {
            "x*sin(x)*x",
            make_expr_23,
            "{ x²·sin(x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = x^2*sin(x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 24 */
        {
            "exp(sin(x))",
            make_expr_24,
            "{ exp(sin(x)) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = exp(sin(x))\n"
            "return expr(x)",
            __LINE__
        },

        /* 25 */
        {
            "cos(x)*exp(sin(x))",
            make_expr_25,
            "{ cos(x)·exp(sin(x)) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = cos(x)*exp(sin(x))\n"
            "return expr(x)",
            __LINE__
        },

        /* 26 */
        {
            "x*x*exp(sin(x))",
            make_expr_26,
            "{ x²·exp(sin(x)) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = x^2*exp(sin(x))\n"
            "return expr(x)",
            __LINE__
        },

        /* 27 */
        {
            "exp(sin(x))*exp(cos(x))",
            make_expr_27,
            "{ exp(sin(x) + cos(x)) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = exp(sin(x) + cos(x))\n"
            "return expr(x)",
            __LINE__
        },

        /* 28 */
        {
            "exp(x^2)*exp(3*x^2)",
            make_expr_28,
            "{ exp(4x²) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = exp(4*x^2)\n"
            "return expr(x)",
            __LINE__
        },

        /* 29 */
        {
            "exp(x)*exp(2*x)",
            make_expr_29,
            "{ exp(3x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = exp(3*x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 30 */
        {
            "exp(sin(x))*exp(cos(x))*exp(x)",
            make_expr_30,
            "{ exp(sin(x) + cos(x) + x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = exp(sin(x) + cos(x) + x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 31 */
        {
            "π*sin(x)",
            make_expr_31,
            "{ π·sin(x) | x = 1.25; π = 3.141592653589793238462643383279505 }",
            "x = 1.25\n"
            "π = 3.141592653589793238462643383279505\n"
            "expr(x,π) = π*sin(x)\n"
            "return expr(x,π)",
            __LINE__
        },

        /* 32 */
        {
            "τ*cos(x)",
            make_expr_32,
            "{ τ·cos(x) | x = 1.25; τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,τ) = τ*cos(x)\n"
            "return expr(x,τ)",
            __LINE__
        },

        /* 33 */
        {
            "e*x^2",
            make_expr_33,
            "{ ex² | x = 1.25; e = 2.718281828459045235360287471352664 }",
            "x = 1.25\n"
            "e = 2.718281828459045235360287471352664\n"
            "expr(x,e) = e*x^2\n"
            "return expr(x,e)",
            __LINE__
        },

        /* 34 */
        {
            "π*τ*e",
            make_expr_34,
            "{ πτe | π = 3.141592653589793238462643383279505, τ = 6.283185307179586476925286766559011, e = 2.718281828459045235360287471352664 }",
            "π = 3.141592653589793238462643383279505\n"
            "τ = 6.283185307179586476925286766559011\n"
            "e = 2.718281828459045235360287471352664\n"
            "expr(π,τ,e) = π*τ*e\n"
            "return expr(π,τ,e)",
            __LINE__
        },

        /* 35 */
        {
            "π*x*τ*y",
            make_expr_35,
            "{ πτxy | x = 1.25, y = 1.25; π = 3.141592653589793238462643383279505, τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "π = 3.141592653589793238462643383279505\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,y,π,τ) = π*τ*x*y\n"
            "return expr(x,y,π,τ)",
            __LINE__
        },

        /* 36 */
        {
            "exp(x)*π",
            make_expr_36,
            "{ π·exp(x) | x = 1.25; π = 3.141592653589793238462643383279505 }",
            "x = 1.25\n"
            "π = 3.141592653589793238462643383279505\n"
            "expr(x,π) = π*exp(x)\n"
            "return expr(x,π)",
            __LINE__
        },

        /* 37 */
        {
            "τ*exp(x^2)",
            make_expr_37,
            "{ τ·exp(x²) | x = 1.25; τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,τ) = τ*exp(x^2)\n"
            "return expr(x,τ)",
            __LINE__
        },

        /* 38 */
        {
            "e*sin(x)*cos(y)",
            make_expr_38,
            "{ e·sin(x)·cos(y) | x = 1.25, y = 1.25; e = 2.718281828459045235360287471352664 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "e = 2.718281828459045235360287471352664\n"
            "expr(x,y,e) = e*sin(x)*cos(y)\n"
            "return expr(x,y,e)",
            __LINE__
        },

        /* 39 */
        {
            "π*exp(τ*x)",
            make_expr_39,
            "{ π·exp(τx) | x = 1.25; π = 3.141592653589793238462643383279505, τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "π = 3.141592653589793238462643383279505\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,π,τ) = π*exp(τ*x)\n"
            "return expr(x,π,τ)",
            __LINE__
        },

        /* 40 */
        {
            "exp(π*x)*τ",
            make_expr_40,
            "{ τ·exp(πx) | x = 1.25; π = 3.141592653589793238462643383279505, τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "π = 3.141592653589793238462643383279505\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,π,τ) = τ*exp(π*x)\n"
            "return expr(x,π,τ)",
            __LINE__
        },

        /* 41 */
        {
            "sin(π*x)",
            make_expr_41,
            "{ sin(πx) | x = 1.25; π = 3.141592653589793238462643383279505 }",
            "x = 1.25\n"
            "π = 3.141592653589793238462643383279505\n"
            "expr(x,π) = sin(π*x)\n"
            "return expr(x,π)",
            __LINE__
        },

        /* 42 */
        {
            "cos(τ*x)",
            make_expr_42,
            "{ cos(τx) | x = 1.25; τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,τ) = cos(τ*x)\n"
            "return expr(x,τ)",
            __LINE__
        },

        /* 43 */
        {
            "exp(π*τ*x)",
            make_expr_43,
            "{ exp(πτx) | x = 1.25; π = 3.141592653589793238462643383279505, τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "π = 3.141592653589793238462643383279505\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,π,τ) = exp(π*τ*x)\n"
            "return expr(x,π,τ)",
            __LINE__
        },

        /* 44 */
        {
            "sin(x)+cos(x)+exp(x)",
            make_expr_44,
            "{ sin(x) + cos(x) + exp(x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = sin(x) + cos(x) + exp(x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 45 */
        {
            "x + y + π + τ + e",
            make_expr_45,
            "{ x + y + π + τ + e | x = 1.25, y = 1.25; π = 3.141592653589793238462643383279505, τ = 6.283185307179586476925286766559011, e = 2.718281828459045235360287471352664 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "π = 3.141592653589793238462643383279505\n"
            "τ = 6.283185307179586476925286766559011\n"
            "e = 2.718281828459045235360287471352664\n"
            "expr(x,y,π,τ,e) = x + y + π + τ + e\n"
            "return expr(x,y,π,τ,e)",
            __LINE__
        },

        /* 46 */
        {
            "x*y + π*x + τ*y + e",
            make_expr_46,
            "{ xy + πx + τy + e | x = 1.25, y = 1.25; π = 3.141592653589793238462643383279505, τ = 6.283185307179586476925286766559011, e = 2.718281828459045235360287471352664 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "π = 3.141592653589793238462643383279505\n"
            "τ = 6.283185307179586476925286766559011\n"
            "e = 2.718281828459045235360287471352664\n"
            "expr(x,y,π,τ,e) = x*y + π*x + τ*y + e\n"
            "return expr(x,y,π,τ,e)",
            __LINE__
        },

        /* 47 */
        {
            "(x+π)*(y+τ)",
            make_expr_47,
            "{ (x + π)·(y + τ) | x = 1.25, y = 1.25; π = 3.141592653589793238462643383279505, τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "π = 3.141592653589793238462643383279505\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,y,π,τ) = (x + π)*(y + τ)\n"
            "return expr(x,y,π,τ)",
            __LINE__
        },

        /* 48 */
        {
            "exp(x+π)*exp(y+τ)",
            make_expr_48,
            "{ exp(x + y + π + τ) | x = 1.25, y = 1.25; π = 3.141592653589793238462643383279505, τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "π = 3.141592653589793238462643383279505\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,y,π,τ) = exp(x + y + π + τ)\n"
            "return expr(x,y,π,τ)",
            __LINE__
        },

        /* 49 */
        {
            "sin(x+π)*cos(y+τ)",
            make_expr_49,
            "{ sin(x + π)·cos(y + τ) | x = 1.25, y = 1.25; π = 3.141592653589793238462643383279505, τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "π = 3.141592653589793238462643383279505\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,y,π,τ) = sin(x + π)*cos(y + τ)\n"
            "return expr(x,y,π,τ)",
            __LINE__
        },

        /* 50 */
        {
            "exp(sin(x+π) + cos(y+τ))",
            make_expr_50,
            "{ exp(sin(x + π) + cos(y + τ)) | x = 1.25, y = 1.25; π = 3.141592653589793238462643383279505, τ = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "π = 3.141592653589793238462643383279505\n"
            "τ = 6.283185307179586476925286766559011\n"
            "expr(x,y,π,τ) = exp(sin(x + π) + cos(y + τ))\n"
            "return expr(x,y,π,τ)",
            __LINE__
        },
    };

    /* ============================================================
     *  Test loop — formatted with bold PASS/FAIL and file:line
     * ============================================================ */
    const int N = (int)(sizeof(tests) / sizeof(tests[0]));

    for (int i = 0; i < N; i++) {
        dval_t *f = tests[i].make();

        char *got_expr = dv_to_string(f, style_EXPRESSION);
        char *got_func = dv_to_string(f, style_FUNCTION);

        int ok_expr = strcmp(got_expr, tests[i].expected_expr) == 0;
        int ok_func = strcmp(got_func, tests[i].expected_func) == 0;

        /* ---------------- EXPR block ---------------- */
        if (ok_expr) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (EXPR)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (EXPR): " C_RED "%s:%d:1\n" C_RESET,
                   tests[i].src, __FILE__, tests[i].line);
            TEST_FAIL();
        }

        printf(C_BOLD "  got      " C_RESET "%s\n", got_expr);
        printf(C_BOLD "  expected " C_RESET "%s\n", tests[i].expected_expr);

        /* ---------------- FUNC block ---------------- */
        if (ok_func) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (FUNC)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (FUNC): " C_RED "%s:%d:1\n" C_RESET,
                   tests[i].src, __FILE__, tests[i].line);
            TEST_FAIL();
        }

        /* got block */
        {
            const char *p = got_func;
            const char *nl;
            printf(C_BOLD "  got      " C_RESET);
            while ((nl = strchr(p, '\n'))) {
                fwrite(p, 1, nl - p, stdout);
                printf("\n           ");
                p = nl + 1;
            }
            printf("%s\n", p);
        }

        printf("  ───────────────────────────────\n");

        /* expected block */
        {
            const char *p = tests[i].expected_func;
            const char *nl;
            printf(C_BOLD "  expected " C_RESET);
            while ((nl = strchr(p, '\n'))) {
                fwrite(p, 1, nl - p, stdout);
                printf("\n           ");
                p = nl + 1;
            }
            printf("%s\n", p);
        }

        printf("\n");

        free(got_expr);
        free(got_func);
        dv_free(f);
    }
}

/* ============================================================
 *  Builders for unnamed-variable tests (U01–U06)
 * ============================================================ */

/* U01: x₀² */
static dval_t *make_expr_u01(void)
{
    dval_t *x  = dv_new_var_d(1.25);
    dval_t *f  = dv_mul(x, x);
    dv_free(x);
    return f;
}

/* U02: x₀³ */
static dval_t *make_expr_u02(void)
{
    dval_t *x  = dv_new_var_d(1.25);
    dval_t *t1 = dv_mul(x, x);
    dval_t *f  = dv_mul(t1, x);
    dv_free(x);
    dv_free(t1);
    return f;
}

/* U03: x₀³x₁³  (mirrors test 09, but with unnamed vars) */
static dval_t *make_expr_u03(void)
{
    dval_t *x  = dv_new_var_d(1.25);
    dval_t *y  = dv_new_var_d(1.25);
    dval_t *t1 = dv_pow_d(x, 2.0);
    dval_t *t2 = dv_pow_d(y, 3.0);
    dval_t *t3 = dv_mul(t1, t2);
    dval_t *f  = dv_mul(t3, x);
    dv_free(x);
    dv_free(y);
    dv_free(t1);
    dv_free(t2);
    dv_free(t3);
    return f;
}

/* U04: 2x₀²  (coefficient stays numeric after simplification) */
static dval_t *make_expr_u04(void)
{
    dval_t *x  = dv_new_var_d(1.25);
    dval_t *t1 = dv_mul(x, x);
    dval_t *t2 = dv_mul(x, x);
    dval_t *f  = dv_add(t1, t2);
    dv_free(x);
    dv_free(t1);
    dv_free(t2);
    return f;
}

/* U05: sin(x₀)·cos(x₀) */
static dval_t *make_expr_u05(void)
{
    dval_t *x  = dv_new_var_d(1.25);
    dval_t *sx = dv_sin(x);
    dval_t *cx = dv_cos(x);
    dval_t *f  = dv_mul(sx, cx);
    dv_free(x);
    dv_free(sx);
    dv_free(cx);
    return f;
}

/* U06: exp(sin(x₀) + cos(x₀))  (exp merge) */
static dval_t *make_expr_u06(void)
{
    dval_t *x  = dv_new_var_d(1.25);
    dval_t *sx = dv_sin(x);
    dval_t *cx = dv_cos(x);
    dval_t *t1 = dv_exp(sx);
    dval_t *t2 = dv_exp(cx);
    dval_t *f  = dv_mul(t1, t2);
    dv_free(x);
    dv_free(sx);
    dv_free(cx);
    dv_free(t1);
    dv_free(t2);
    return f;
}

/* ============================================================
 *  Builders for manually-subscripted constant tests (C01–C04)
 *
 *  Callers pass "c\xE2\x82\x80" (c₀) and "c\xE2\x82\x81" (c₁)
 *  as the name argument to dv_new_named_const so the names are
 *  simple (letter + subscript digit) — they won't be bracketed.
 * ============================================================ */

/* C01: c₀x₀²  (named const × unnamed var²) */
static dval_t *make_expr_c01(void)
{
    dval_t *x  = dv_new_var_d(1.25);
    dval_t *c  = dv_new_named_const(QF_PI, "c\xE2\x82\x80");
    dval_t *x2 = dv_pow_d(x, 2.0);
    dval_t *f  = dv_mul(c, x2);
    dv_free(x);
    dv_free(c);
    dv_free(x2);
    return f;
}

/* C02: c₀·sin(x₀)  (named const × function — needs separator) */
static dval_t *make_expr_c02(void)
{
    dval_t *x  = dv_new_var_d(1.25);
    dval_t *c  = dv_new_named_const(QF_E, "c\xE2\x82\x80");
    dval_t *sx = dv_sin(x);
    dval_t *f  = dv_mul(c, sx);
    dv_free(x);
    dv_free(c);
    dv_free(sx);
    return f;
}

/* C03: x₀ + x₁ + c₀ */
static dval_t *make_expr_c03(void)
{
    dval_t *x  = dv_new_var_d(1.25);
    dval_t *y  = dv_new_var_d(1.25);
    dval_t *c  = dv_new_named_const(QF_PI, "c\xE2\x82\x80");
    dval_t *t1 = dv_add(x, y);
    dval_t *f  = dv_add(t1, c);
    dv_free(x);
    dv_free(y);
    dv_free(c);
    dv_free(t1);
    return f;
}

/* C04: c₀x₀ + c₁  (two named consts with unnamed var; tests multi-const bindings) */
static dval_t *make_expr_c04(void)
{
    dval_t *x  = dv_new_var_d(1.25);
    dval_t *c0 = dv_new_named_const(QF_PI, "c\xE2\x82\x80");
    dval_t *c1 = dv_new_named_const(QF_E,  "c\xE2\x82\x81");
    dval_t *t1 = dv_mul(c0, x);
    dval_t *f  = dv_add(t1, c1);
    dv_free(x);
    dv_free(c0);
    dv_free(c1);
    dv_free(t1);
    return f;
}

/* ============================================================
 *  Builders for multi-character name tests (L01–L09)
 * ============================================================ */

/* L01: [radius]² */
static dval_t *make_expr_l01(void)
{
    dval_t *r = dv_new_named_var_d(1.25, "radius");
    dval_t *f = dv_pow_d(r, 2.0);
    dv_free(r);
    return f;
}

/* L02: [base]·[height]  (two multi-char vars — separator needed) */
static dval_t *make_expr_l02(void)
{
    dval_t *base   = dv_new_named_var_d(1.25, "base");
    dval_t *height = dv_new_named_var_d(1.25, "height");
    dval_t *f      = dv_mul(base, height);
    dv_free(base);
    dv_free(height);
    return f;
}

/* L03: [pi]·[radius]²  (multi-char named const × multi-char named var²) */
static dval_t *make_expr_l03(void)
{
    dval_t *r  = dv_new_named_var_d(1.25, "radius");
    dval_t *pi = dv_new_named_const(QF_PI, "pi");
    dval_t *r2 = dv_pow_d(r, 2.0);
    dval_t *f  = dv_mul(pi, r2);
    dv_free(r);
    dv_free(pi);
    dv_free(r2);
    return f;
}

/* L04: π·[radius]²  (@pi → π is simple; radius is not — separator needed) */
static dval_t *make_expr_l04(void)
{
    dval_t *r  = dv_new_named_var_d(1.25, "radius");
    dval_t *pi = dv_new_named_const(QF_PI, "@pi");
    dval_t *r2 = dv_pow_d(r, 2.0);
    dval_t *f  = dv_mul(pi, r2);
    dv_free(r);
    dv_free(pi);
    dv_free(r2);
    return f;
}

/* L05: sin([theta])·cos([theta]) */
static dval_t *make_expr_l05(void)
{
    dval_t *t  = dv_new_named_var_d(1.25, "theta");
    dval_t *st = dv_sin(t);
    dval_t *ct = dv_cos(t);
    dval_t *f  = dv_mul(st, ct);
    dv_free(t);
    dv_free(st);
    dv_free(ct);
    return f;
}

/* L06: [pi]·[tau]·x  (two multi-char consts + one single-char var) */
static dval_t *make_expr_l06(void)
{
    dval_t *x   = dv_new_named_var_d(1.25, "x");
    dval_t *pi  = dv_new_named_const(QF_PI,  "pi");
    dval_t *tau = dv_new_named_const(QF_2PI, "tau");
    dval_t *t1  = dv_mul(pi, tau);
    dval_t *f   = dv_mul(t1, x);
    dv_free(x);
    dv_free(pi);
    dv_free(tau);
    dv_free(t1);
    return f;
}

/* L07: [my var]²  (space in name) */
static dval_t *make_expr_l07(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "my var");
    dval_t *f = dv_pow_d(x, 2.0);
    dv_free(x);
    return f;
}

/* L08: [2pi]·x  (name starting with a digit) */
static dval_t *make_expr_l08(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x");
    dval_t *c = dv_new_named_const(QF_PI, "2pi");
    dval_t *f = dv_mul(c, x);
    dv_free(x);
    dv_free(c);
    return f;
}

/* L09: [x']²  (non-alphanumeric character — apostrophe/prime) */
static dval_t *make_expr_l09(void)
{
    dval_t *x = dv_new_named_var_d(1.25, "x'");
    dval_t *f = dv_pow_d(x, 2.0);
    dv_free(x);
    return f;
}

/* ============================================================
 *  test_expressions_unnamed
 * ============================================================ */
static void test_expressions_unnamed(void)
{
    struct {
        const char *src;
        dval_t *(*make)(void);
        const char *expected_expr;
        const char *expected_func;
        int line;
    } tests[] = {
        /* U01 */
        {
            "x*x (unnamed)",
            make_expr_u01,
            "{ x₀² | x₀ = 1.25 }",
            "x₀ = 1.25\n"
            "expr(x₀) = x₀^2\n"
            "return expr(x₀)",
            __LINE__
        },

        /* U02 */
        {
            "x*x*x (unnamed)",
            make_expr_u02,
            "{ x₀³ | x₀ = 1.25 }",
            "x₀ = 1.25\n"
            "expr(x₀) = x₀^3\n"
            "return expr(x₀)",
            __LINE__
        },

        /* U03 */
        {
            "x^2*y^3*x (unnamed)",
            make_expr_u03,
            "{ x₀³x₁³ | x₀ = 1.25, x₁ = 1.25 }",
            "x₀ = 1.25\n"
            "x₁ = 1.25\n"
            "expr(x₀,x₁) = x₀^3*x₁^3\n"
            "return expr(x₀,x₁)",
            __LINE__
        },

        /* U04 */
        {
            "x*x + x*x (unnamed)",
            make_expr_u04,
            "{ 2x₀² | x₀ = 1.25 }",
            "x₀ = 1.25\n"
            "expr(x₀) = 2*x₀^2\n"
            "return expr(x₀)",
            __LINE__
        },

        /* U05 */
        {
            "sin(x)*cos(x) (unnamed)",
            make_expr_u05,
            "{ sin(x₀)·cos(x₀) | x₀ = 1.25 }",
            "x₀ = 1.25\n"
            "expr(x₀) = sin(x₀)*cos(x₀)\n"
            "return expr(x₀)",
            __LINE__
        },

        /* U06 */
        {
            "exp(sin(x))*exp(cos(x)) (unnamed)",
            make_expr_u06,
            "{ exp(sin(x₀) + cos(x₀)) | x₀ = 1.25 }",
            "x₀ = 1.25\n"
            "expr(x₀) = exp(sin(x₀) + cos(x₀))\n"
            "return expr(x₀)",
            __LINE__
        },

        /* C01 */
        {
            "c₀*x₀^2 (named const, unnamed var)",
            make_expr_c01,
            "{ c₀x₀² | x₀ = 1.25; c₀ = 3.141592653589793238462643383279505 }",
            "x₀ = 1.25\n"
            "c₀ = 3.141592653589793238462643383279505\n"
            "expr(x₀,c₀) = c₀*x₀^2\n"
            "return expr(x₀,c₀)",
            __LINE__
        },

        /* C02 */
        {
            "c₀*sin(x₀) (named const, unnamed var)",
            make_expr_c02,
            "{ c₀·sin(x₀) | x₀ = 1.25; c₀ = 2.718281828459045235360287471352664 }",
            "x₀ = 1.25\n"
            "c₀ = 2.718281828459045235360287471352664\n"
            "expr(x₀,c₀) = c₀*sin(x₀)\n"
            "return expr(x₀,c₀)",
            __LINE__
        },

        /* C03 */
        {
            "x₀ + x₁ + c₀",
            make_expr_c03,
            "{ x₀ + x₁ + c₀ | x₀ = 1.25, x₁ = 1.25; c₀ = 3.141592653589793238462643383279505 }",
            "x₀ = 1.25\n"
            "x₁ = 1.25\n"
            "c₀ = 3.141592653589793238462643383279505\n"
            "expr(x₀,x₁,c₀) = x₀ + x₁ + c₀\n"
            "return expr(x₀,x₁,c₀)",
            __LINE__
        },

        /* C04 */
        {
            "c₀*x₀ + c₁ (two named consts, unnamed var)",
            make_expr_c04,
            "{ c₀x₀ + c₁ | x₀ = 1.25; c₀ = 3.141592653589793238462643383279505, c₁ = 2.718281828459045235360287471352664 }",
            "x₀ = 1.25\n"
            "c₀ = 3.141592653589793238462643383279505\n"
            "c₁ = 2.718281828459045235360287471352664\n"
            "expr(x₀,c₀,c₁) = c₀*x₀ + c₁\n"
            "return expr(x₀,c₀,c₁)",
            __LINE__
        },
    };

    const int N = (int)(sizeof(tests) / sizeof(tests[0]));

    for (int i = 0; i < N; i++) {
        dval_t *f = tests[i].make();

        char *got_expr = dv_to_string(f, style_EXPRESSION);
        char *got_func = dv_to_string(f, style_FUNCTION);

        int ok_expr = strcmp(got_expr, tests[i].expected_expr) == 0;
        int ok_func = strcmp(got_func, tests[i].expected_func) == 0;

        if (ok_expr) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (EXPR)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (EXPR): " C_RED "%s:%d:1\n" C_RESET,
                   tests[i].src, __FILE__, tests[i].line);
            TEST_FAIL();
        }

        printf(C_BOLD "  got      " C_RESET "%s\n", got_expr);
        printf(C_BOLD "  expected " C_RESET "%s\n", tests[i].expected_expr);

        if (ok_func) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (FUNC)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (FUNC): " C_RED "%s:%d:1\n" C_RESET,
                   tests[i].src, __FILE__, tests[i].line);
            TEST_FAIL();
        }

        {
            const char *p = got_func;
            const char *nl;
            printf(C_BOLD "  got      " C_RESET);
            while ((nl = strchr(p, '\n'))) {
                fwrite(p, 1, nl - p, stdout);
                printf("\n           ");
                p = nl + 1;
            }
            printf("%s\n", p);
        }

        printf("  ───────────────────────────────\n");

        {
            const char *p = tests[i].expected_func;
            const char *nl;
            printf(C_BOLD "  expected " C_RESET);
            while ((nl = strchr(p, '\n'))) {
                fwrite(p, 1, nl - p, stdout);
                printf("\n           ");
                p = nl + 1;
            }
            printf("%s\n", p);
        }

        printf("\n");

        free(got_expr);
        free(got_func);
        dv_free(f);
    }
}

/* ============================================================
 *  test_expressions_longname
 * ============================================================ */
static void test_expressions_longname(void)
{
    struct {
        const char *src;
        dval_t *(*make)(void);
        const char *expected_expr;
        const char *expected_func;
        int line;
    } tests[] = {
        /* L01 */
        {
            "radius^2",
            make_expr_l01,
            "{ [radius]² | [radius] = 1.25 }",
            "radius = 1.25\n"
            "expr(radius) = radius^2\n"
            "return expr(radius)",
            __LINE__
        },

        /* L02 */
        {
            "base * height",
            make_expr_l02,
            "{ [base]·[height] | [base] = 1.25, [height] = 1.25 }",
            "base = 1.25\n"
            "height = 1.25\n"
            "expr(base,height) = base*height\n"
            "return expr(base,height)",
            __LINE__
        },

        /* L03 */
        {
            "pi * radius^2",
            make_expr_l03,
            "{ [pi]·[radius]² | [radius] = 1.25; [pi] = 3.141592653589793238462643383279505 }",
            "radius = 1.25\n"
            "pi = 3.141592653589793238462643383279505\n"
            "expr(radius,pi) = pi*radius^2\n"
            "return expr(radius,pi)",
            __LINE__
        },

        /* L04 */
        {
            "@pi * radius^2",
            make_expr_l04,
            "{ π·[radius]² | [radius] = 1.25; π = 3.141592653589793238462643383279505 }",
            "radius = 1.25\n"
            "π = 3.141592653589793238462643383279505\n"
            "expr(radius,π) = π*radius^2\n"
            "return expr(radius,π)",
            __LINE__
        },

        /* L05 */
        {
            "sin(theta)*cos(theta)",
            make_expr_l05,
            "{ sin([theta])·cos([theta]) | [theta] = 1.25 }",
            "theta = 1.25\n"
            "expr(theta) = sin(theta)*cos(theta)\n"
            "return expr(theta)",
            __LINE__
        },

        /* L06 */
        {
            "pi * tau * x",
            make_expr_l06,
            "{ [pi]·[tau]·x | x = 1.25; [pi] = 3.141592653589793238462643383279505, [tau] = 6.283185307179586476925286766559011 }",
            "x = 1.25\n"
            "pi = 3.141592653589793238462643383279505\n"
            "tau = 6.283185307179586476925286766559011\n"
            "expr(x,pi,tau) = pi*tau*x\n"
            "return expr(x,pi,tau)",
            __LINE__
        },

        /* L07: space in name */
        {
            "\"my var\"^2",
            make_expr_l07,
            "{ [my var]² | [my var] = 1.25 }",
            "[my var] = 1.25\n"
            "expr([my var]) = [my var]^2\n"
            "return expr([my var])",
            __LINE__
        },

        /* L08: name starting with a digit */
        {
            "\"2pi\" * x",
            make_expr_l08,
            "{ [2pi]·x | x = 1.25; [2pi] = 3.141592653589793238462643383279505 }",
            "x = 1.25\n"
            "[2pi] = 3.141592653589793238462643383279505\n"
            "expr(x,[2pi]) = [2pi]*x\n"
            "return expr(x,[2pi])",
            __LINE__
        },

        /* L09: non-alphanumeric character (apostrophe/prime) */
        {
            "\"x'\"^2",
            make_expr_l09,
            "{ [x']² | [x'] = 1.25 }",
            "[x'] = 1.25\n"
            "expr([x']) = [x']^2\n"
            "return expr([x'])",
            __LINE__
        },
    };

    const int N = (int)(sizeof(tests) / sizeof(tests[0]));

    for (int i = 0; i < N; i++) {
        dval_t *f = tests[i].make();

        char *got_expr = dv_to_string(f, style_EXPRESSION);
        char *got_func = dv_to_string(f, style_FUNCTION);

        int ok_expr = strcmp(got_expr, tests[i].expected_expr) == 0;
        int ok_func = strcmp(got_func, tests[i].expected_func) == 0;

        if (ok_expr) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (EXPR)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (EXPR): " C_RED "%s:%d:1\n" C_RESET,
                   tests[i].src, __FILE__, tests[i].line);
            TEST_FAIL();
        }

        printf(C_BOLD "  got      " C_RESET "%s\n", got_expr);
        printf(C_BOLD "  expected " C_RESET "%s\n", tests[i].expected_expr);

        if (ok_func) {
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s (FUNC)\n", tests[i].src);
        } else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s (FUNC): " C_RED "%s:%d:1\n" C_RESET,
                   tests[i].src, __FILE__, tests[i].line);
            TEST_FAIL();
        }

        {
            const char *p = got_func;
            const char *nl;
            printf(C_BOLD "  got      " C_RESET);
            while ((nl = strchr(p, '\n'))) {
                fwrite(p, 1, nl - p, stdout);
                printf("\n           ");
                p = nl + 1;
            }
            printf("%s\n", p);
        }

        printf("  ───────────────────────────────\n");

        {
            const char *p = tests[i].expected_func;
            const char *nl;
            printf(C_BOLD "  expected " C_RESET);
            while ((nl = strchr(p, '\n'))) {
                fwrite(p, 1, nl - p, stdout);
                printf("\n           ");
                p = nl + 1;
            }
            printf("%s\n", p);
        }

        printf("\n");

        free(got_expr);
        free(got_func);
        dv_free(f);
    }
}

/* Build expression: f(x) = exp(sin(x)) + 3*x^2 - 7 */
static dval_t *make_f(dval_t *x) {
    dval_t *sinx   = dv_sin(x);
    dval_t *exp_sx = dv_exp(sinx);
    dval_t *x2     = dv_pow_d(x, 2.0);
    dval_t *term2  = dv_mul_d(x2, 3.0);
    dval_t *f0     = dv_add(exp_sx, term2);
    dval_t *f      = dv_sub_d(f0, 7.0);   /* f = exp(sin(x)) + 3*x^2 - 7 */

    dv_free(sinx);
    dv_free(exp_sx);
    dv_free(x2);
    dv_free(term2);
    dv_free(f0);

    return f;
}

/* ============================================================
 *  test_dval_t_from_string — dedicated from_string test group
 * ============================================================ */

/* Round-trip helper: build a dval_t, convert to expr string, parse it back,
 * and verify the evaluated value matches the original. */
static void check_roundtrip(const char *label, dval_t *f, int line)
{
    char *s = dv_to_string(f, style_EXPRESSION);
    dval_t *g = dval_from_string(s);

    if (!g) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (from_string returned NULL) %s:%d:1\n",
               label, __FILE__, line);
        printf(C_BOLD "  string " C_RESET "%s\n\n", s);
        TEST_FAIL();
        free(s);
        dv_free(f);
        return;
    }

    qfloat_t expect = dv_eval(f);
    qfloat_t got    = dv_eval(g);
    qfloat_t diff   = qf_sub(got, expect);
    double abs_err = fabs(qf_to_double(diff));
    double exp_d   = fabs(qf_to_double(expect));
    double rel_err = (exp_d > 0) ? abs_err / exp_d : abs_err;

    const double TOL = 2e-14; /* round-trip through double in binding values */
    if (abs_err < TOL || rel_err < TOL) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n", label);
        printf(C_BOLD "  string  " C_RESET "%s\n\n", s);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s value mismatch %s:%d:1\n",
               label, __FILE__, line);
        printf(C_BOLD "  string  " C_RESET "%s\n", s);
        qf_printf(C_BOLD "  got     " C_RESET "%.34q\n", got);
        qf_printf(C_BOLD "  expect  " C_RESET "%.34q\n", expect);
        printf("\n");
        TEST_FAIL();
    }

    free(s);
    dv_free(g);
    dv_free(f);
}

/* Check that parsing an explicit string gives a specific evaluated value. */
static void check_parse_val(const char *label, const char *s,
                             double expect_d, int line)
{
    dval_t *g = dval_from_string(s);
    if (!g) {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (NULL) %s:%d:1\n",
               label, __FILE__, line);
        TEST_FAIL();
        return;
    }
    double got = dv_eval_d(g);
    double err = fabs(got - expect_d);
    double rel = (fabs(expect_d) > 0) ? err / fabs(expect_d) : err;
    const double TOL = 2e-14;
    if (err < TOL || rel < TOL) {
        char *parsed = dv_to_string(g, style_EXPRESSION);
        printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n", label);
        printf(C_BOLD "  input   " C_RESET "%s\n", s);
        printf(C_BOLD "  parsed  " C_RESET "%s\n\n", parsed ? parsed : "(null)");
        free(parsed);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s %s:%d:1\n",
               label, __FILE__, line);
        printf(C_BOLD "  string  " C_RESET "%s\n", s);
        printf(C_BOLD "  got     " C_RESET "%.17g\n", got);
        printf(C_BOLD "  expect  " C_RESET "%.17g\n", expect_d);
        printf("\n");
        TEST_FAIL();
    }
    dv_free(g);
}

/* Check that parsing a string returns NULL (expected error path).
 * Note: dval_from_string prints diagnostics to stderr for error cases. */
static void check_parse_null(const char *label, const char *s, int line)
{
    dval_t *g = dval_from_string(s);
    if (!g) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n\n", label);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s (expected NULL) %s:%d:1\n\n",
               label, __FILE__, line);
        TEST_FAIL();
        dv_free(g);
    }
}

/* ---- Pure-constant format: { name = val } ---- */

static void test_from_string_pure_const(void)
{
    /* Single Unicode letter name */
    check_parse_val("pure const π",
        "{ \xcf\x80 = 3.141592653589793 }",
        3.141592653589793, __LINE__);
    /* Bracketed multi-character name */
    check_parse_val("pure const [e]",
        "{ [e] = 2.718281828459045 }",
        2.718281828459045, __LINE__);
    /* Negative value */
    check_parse_val("pure const negative",
        "{ [neg] = -1.5 }",
        -1.5, __LINE__);
    /* Zero */
    check_parse_val("pure const zero",
        "{ z = 0 }",
        0.0, __LINE__);
    /* Name with spaces (bracketed) */
    check_parse_val("pure const [my const]",
        "{ [my const] = 42 }",
        42.0, __LINE__);
}

/* ---- Arithmetic operators ---- */

static void test_from_string_arithmetic(void)
{
    /* Addition */
    check_parse_val("x\xe2\x82\x80 + x\xe2\x82\x81 = 7",
        "{ x\xe2\x82\x80 + x\xe2\x82\x81 | x\xe2\x82\x80 = 3, x\xe2\x82\x81 = 4 }",
        7.0, __LINE__);
    /* Subtraction */
    check_parse_val("x\xe2\x82\x80 - x\xe2\x82\x81 = 7",
        "{ x\xe2\x82\x80 - x\xe2\x82\x81 | x\xe2\x82\x80 = 10, x\xe2\x82\x81 = 3 }",
        7.0, __LINE__);
    /* Unary negation */
    check_parse_val("-x = -3",
        "{ -x | x = 3 }",
        -3.0, __LINE__);
    /* Implicit multiplication (juxtaposition) */
    check_parse_val("x\xe2\x82\x80x\xe2\x82\x81 implicit = 12",
        "{ x\xe2\x82\x80x\xe2\x82\x81 | x\xe2\x82\x80 = 3, x\xe2\x82\x81 = 4 }",
        12.0, __LINE__);
    /* Explicit middle-dot multiplication */
    check_parse_val("x\xe2\x82\x80\xc2\xb7x\xe2\x82\x81 middle-dot = 12",
        "{ x\xe2\x82\x80\xc2\xb7x\xe2\x82\x81 | x\xe2\x82\x80 = 3, x\xe2\x82\x81 = 4 }",
        12.0, __LINE__);
    /* Superscript ² */
    check_parse_val("x\xc2\xb2 = 9",
        "{ x\xc2\xb2 | x = 3 }",
        9.0, __LINE__);
    /* Superscript ³ */
    check_parse_val("x\xc2\xb3 = 8",
        "{ x\xc2\xb3 | x = 2 }",
        8.0, __LINE__);
    /* Caret exponent */
    check_parse_val("x^2.5 at 4 = 32",
        "{ x^2.5 | x = 4 }",
        32.0, __LINE__);
    /* Parenthesised sub-expression with superscript */
    check_parse_val("(x\xe2\x82\x80 + x\xe2\x82\x81)\xc2\xb2 = 25",
        "{ (x\xe2\x82\x80 + x\xe2\x82\x81)\xc2\xb2 | x\xe2\x82\x80 = 2, x\xe2\x82\x81 = 3 }",
        25.0, __LINE__);
    /* Chained addition */
    check_parse_val("x\xe2\x82\x80 + x\xe2\x82\x81 + x\xe2\x82\x82 = 6",
        "{ x\xe2\x82\x80 + x\xe2\x82\x81 + x\xe2\x82\x82 | x\xe2\x82\x80 = 1, x\xe2\x82\x81 = 2, x\xe2\x82\x82 = 3 }",
        6.0, __LINE__);
    /* Mixed add and implicit mul: 2x + 3y */
    check_parse_val("c\xe2\x82\x80x\xe2\x82\x80 + c\xe2\x82\x81x\xe2\x82\x81 = 13",
        "{ c\xe2\x82\x80x\xe2\x82\x80 + c\xe2\x82\x81x\xe2\x82\x81 | x\xe2\x82\x80 = 2, x\xe2\x82\x81 = 3; c\xe2\x82\x80 = 2, c\xe2\x82\x81 = 3 }",
        13.0, __LINE__);
}

/* ---- Elementary functions ---- */

static void test_from_string_functions(void)
{
    /* All 16 unary functions at values where the result is exact or 0/1 */
    check_parse_val("sin(x) at 0",       "{ sin(x) | x = 0 }",           0.0,          __LINE__);
    check_parse_val("cos(x) at 0",       "{ cos(x) | x = 0 }",           1.0,          __LINE__);
    check_parse_val("tan(x) at 0",       "{ tan(x) | x = 0 }",           0.0,          __LINE__);
    check_parse_val("sinh(x) at 0",      "{ sinh(x) | x = 0 }",          0.0,          __LINE__);
    check_parse_val("cosh(x) at 0",      "{ cosh(x) | x = 0 }",          1.0,          __LINE__);
    check_parse_val("tanh(x) at 0",      "{ tanh(x) | x = 0 }",          0.0,          __LINE__);
    check_parse_val("asin(x) at 0",      "{ asin(x) | x = 0 }",          0.0,          __LINE__);
    check_parse_val("acos(x) at 1",      "{ acos(x) | x = 1 }",          0.0,          __LINE__);
    check_parse_val("atan(x) at 0",      "{ atan(x) | x = 0 }",          0.0,          __LINE__);
    check_parse_val("asinh(x) at 0",     "{ asinh(x) | x = 0 }",         0.0,          __LINE__);
    check_parse_val("acosh(x) at 1",     "{ acosh(x) | x = 1 }",         0.0,          __LINE__);
    check_parse_val("atanh(x) at 0",     "{ atanh(x) | x = 0 }",         0.0,          __LINE__);
    check_parse_val("exp(x) at 0",       "{ exp(x) | x = 0 }",           1.0,          __LINE__);
    check_parse_val("log(x) at 1",       "{ log(x) | x = 1 }",           0.0,          __LINE__);
    check_parse_val("sqrt(x) at 4",      "{ sqrt(x) | x = 4 }",          2.0,          __LINE__);
    /* Binary functions */
    check_parse_val("atan2(1, 1) = π/4",
        "{ atan2(x\xe2\x82\x80, x\xe2\x82\x81) | x\xe2\x82\x80 = 1, x\xe2\x82\x81 = 1 }",
        M_PI / 4.0, __LINE__);
    check_parse_val("pow(2, 3) = 8",
        "{ pow(x\xe2\x82\x80, x\xe2\x82\x81) | x\xe2\x82\x80 = 2, x\xe2\x82\x81 = 3 }",
        8.0, __LINE__);
    /* Superscript on function name: sin²(x) */
    check_parse_val("sin\xc2\xb2(x) at 0",
        "{ sin\xc2\xb2(x) | x = 0 }",
        0.0, __LINE__);
    /* Nested function calls */
    check_parse_val("exp(sin(x)) at 0 = 1",
        "{ exp(sin(x)) | x = 0 }",
        1.0, __LINE__);
    check_parse_val("sqrt(exp(x)) at 0 = 1",
        "{ sqrt(exp(x)) | x = 0 }",
        1.0, __LINE__);
    /* Function applied to parenthesised expression */
    check_parse_val("sin(x\xe2\x82\x80 + x\xe2\x82\x81) = sin(π/2) = 1",
        "{ sin(x\xe2\x82\x80 + x\xe2\x82\x81) | x\xe2\x82\x80 = 0, x\xe2\x82\x81 = 0 }",
        0.0, __LINE__);
}

/* ---- Special functions (the 18 new ops) ---- */

static void test_from_string_special_functions(void)
{
    /* Unary — clean exact values */
    check_parse_val("abs(-3) = 3",           "{ abs(x) | x = -3 }",          3.0,                     __LINE__);
    check_parse_val("erf(0) = 0",            "{ erf(x) | x = 0 }",           0.0,                     __LINE__);
    check_parse_val("erfc(0) = 1",           "{ erfc(x) | x = 0 }",          1.0,                     __LINE__);
    check_parse_val("erfinv(0) = 0",         "{ erfinv(x) | x = 0 }",        0.0,                     __LINE__);
    check_parse_val("erfcinv(1) = 0",        "{ erfcinv(x) | x = 1 }",       0.0,                     __LINE__);
    check_parse_val("gamma(3) = 2",          "{ gamma(x) | x = 3 }",         2.0,                     __LINE__);
    check_parse_val("lgamma(1) = 0",         "{ lgamma(x) | x = 1 }",        0.0,                     __LINE__);
    check_parse_val("digamma(1) = -gamma_E", "{ digamma(x) | x = 1 }",      -0.5772156649015329,       __LINE__);
    check_parse_val("lambert_w0(0) = 0",     "{ lambert_w0(x) | x = 0 }",    0.0,                     __LINE__);
    check_parse_val("lambert_wm1(-0.2)",     "{ lambert_wm1(x) | x = -0.2 }",  -2.5426413577735265,     __LINE__);
    check_parse_val("normal_pdf(0)",         "{ normal_pdf(x) | x = 0 }",    1.0/sqrt(2.0*M_PI),      __LINE__);
    check_parse_val("normal_cdf(0) = 0.5",   "{ normal_cdf(x) | x = 0 }",    0.5,                     __LINE__);
    check_parse_val("normal_logpdf(0)",      "{ normal_logpdf(x) | x = 0 }", -0.5*log(2.0*M_PI),      __LINE__);
    check_parse_val("Ei(1)",                 "{ Ei(x) | x = 1 }",            1.8951178163559367,       __LINE__);
    check_parse_val("E1(1)",                 "{ E1(x) | x = 1 }",            0.21938393439552029,      __LINE__);
    /* Binary functions */
    check_parse_val("beta(1,1) = 1",         "{ beta(x, y) | x = 1, y = 1 }", 1.0,                   __LINE__);
    check_parse_val("logbeta(1,1) = 0",      "{ logbeta(x, y) | x = 1, y = 1 }", 0.0,                __LINE__);
    check_parse_val("hypot(3,4) = 5",        "{ hypot(x, y) | x = 3, y = 4 }", 5.0,                  __LINE__);
}

/* ---- Named constants (binding section) ---- */

static void test_from_string_named_consts(void)
{
    /* Named constant after ';' combined with a variable */
    check_parse_val("c\xe2\x82\x80x\xe2\x82\x80\xc2\xb2 = 12",
        "{ c\xe2\x82\x80x\xe2\x82\x80\xc2\xb2 | x\xe2\x82\x80 = 2; c\xe2\x82\x80 = 3 }",
        12.0, __LINE__);
    /* Two named constants, no variables */
    check_parse_val("c\xe2\x82\x80 + c\xe2\x82\x81 (no vars) = 3",
        "{ c\xe2\x82\x80 + c\xe2\x82\x81 | ; c\xe2\x82\x80 = 1, c\xe2\x82\x81 = 2 }",
        3.0, __LINE__);
    /* Named constant and variable */
    check_parse_val("c\xe2\x82\x80 + x (const + var) = 15",
        "{ c\xe2\x82\x80 + x | x = 5; c\xe2\x82\x80 = 10 }",
        15.0, __LINE__);
    /* Two named constants in the const section */
    check_parse_val("c\xe2\x82\x80x\xe2\x82\x80 + c\xe2\x82\x81 = 2π+e",
        "{ c\xe2\x82\x80x\xe2\x82\x80 + c\xe2\x82\x81 | x\xe2\x82\x80 = 2; c\xe2\x82\x80 = 3.141592653589793, c\xe2\x82\x81 = 2.718281828459045 }",
        2 * 3.141592653589793 + 2.718281828459045, __LINE__);
    /* Bracketed name in const section */
    check_parse_val("[scale]·x = 6",
        "{ [scale]x | x = 3; [scale] = 2 }",
        6.0, __LINE__);
}

/* ---- Bracketed (multi-character) names ---- */

static void test_from_string_bracketed_names(void)
{
    check_parse_val("[radius]\xc2\xb2 = 25",
        "{ [radius]\xc2\xb2 | [radius] = 5 }",
        25.0, __LINE__);
    check_parse_val("[base]\xc2\xb7[height] = 12",
        "{ [base]\xc2\xb7[height] | [base] = 3, [height] = 4 }",
        12.0, __LINE__);
    check_parse_val("[my var] alone = 7",
        "{ [my var] | [my var] = 7 }",
        7.0, __LINE__);
    check_parse_val("[x']\xc2\xb2 = 36",
        "{ [x']\xc2\xb2 | [x'] = 6 }",
        36.0, __LINE__);
    check_parse_val("[2pi]\xc2\xb7x = 2π",
        "{ [2pi]x | x = 1; [2pi] = 6.283185307179586 }",
        6.283185307179586, __LINE__);
    /* Bracketed name as named const in pure-const format */
    check_parse_val("[my const] pure const = 99",
        "{ [my const] = 99 }",
        99.0, __LINE__);
}

/* ---- ASCII alternative syntax — subscripts (_N) ---- */

static void test_from_string_subscripts(void)
{
    /* Basic _N in both binding and expression */
    check_parse_val("x_0^2 = 9",
        "{ x_0^2 | x_0 = 3 }",
        9.0, __LINE__);
    check_parse_val("x_0 + x_1 + x_2 = 6",
        "{ x_0 + x_1 + x_2 | x_0 = 1, x_1 = 2, x_2 = 3 }",
        6.0, __LINE__);
    check_parse_val("c_0*x_0^2 + c_1 = 7",
        "{ c_0*x_0^2 + c_1 | x_0 = 2; c_0 = 1.5, c_1 = 1 }",
        7.0, __LINE__);
    check_parse_val("sin(x_0)*cos(x_0) at pi/4 = 0.5",
        "{ sin(x_0)*cos(x_0) | x_0 = 0.7853981633974483 }",
        0.5, __LINE__);
    /* _N and Unicode ₀-₉ must be interchangeable within one string */
    check_parse_val("Unicode expr, ASCII binding",
        "{ x\xe2\x82\x80^2 | x_0 = 3 }",
        9.0, __LINE__);
    check_parse_val("ASCII expr, Unicode binding",
        "{ x_0^2 | x\xe2\x82\x80 = 3 }",
        9.0, __LINE__);
    check_parse_val("Unicode expr + const, ASCII binding",
        "{ c\xe2\x82\x80*x\xe2\x82\x80^2 | x_0 = 2; c_0 = 3 }",
        12.0, __LINE__);
    check_parse_val("c_0*sin(x_0) + c_1*cos(x_1): Unicode consts, ASCII vars",
        "{ c\xe2\x82\x80*sin(x_0) + c\xe2\x82\x81*cos(x_1)"
        " | x_0 = 1.5707963267948966, x_1 = 0; c_0 = 2, c_1 = 5 }",
        7.0, __LINE__);   /* 2*sin(pi/2) + 5*cos(0) = 2*1 + 5*1 = 7 */
}

/* ---- ASCII alternative syntax — star (*) multiplication ---- */

static void test_from_string_star_mul(void)
{
    /* No spaces */
    check_parse_val("a*b = 12 (no spaces)",
        "{ x_0*x_1 | x_0 = 3, x_1 = 4 }",
        12.0, __LINE__);
    /* Spaces around '*' */
    check_parse_val("a * b = 12 (spaces)",
        "{ x_0 * x_1 | x_0 = 3, x_1 = 4 }",
        12.0, __LINE__);
    /* Scalar constant times function */
    check_parse_val("c * sin(pi/2) = 5",
        "{ c * sin(x) | x = 1.5707963267948966; c = 5 }",
        5.0, __LINE__);
    check_parse_val("c * exp(0) = 3",
        "{ c * exp(x) | x = 0; c = 3 }",
        3.0, __LINE__);
    check_parse_val("c * log(e) = 7",
        "{ c * log(x) | x = 2.718281828459045; c = 7 }",
        7.0, __LINE__);
    /* Chained '*' */
    check_parse_val("a * b * c = 24",
        "{ x_0 * x_1 * x_2 | x_0 = 2, x_1 = 3, x_2 = 4 }",
        24.0, __LINE__);
    /* '*' combined with negation */
    check_parse_val("a * -b = -12",
        "{ x_0 * -x_1 | x_0 = 3, x_1 = 4 }",
        -12.0, __LINE__);
    /* '*' with parenthesised sub-expressions: (a+b)(a-b) = a²-b² */
    check_parse_val("(a+b)*(a-b) = a^2-b^2 = 16",
        "{ (x_0 + x_1) * (x_0 - x_1) | x_0 = 5, x_1 = 3 }",
        16.0, __LINE__);
    /* Sine addition formula: sin(a)cos(b) + sin(b)cos(a) = sin(a+b) */
    check_parse_val("sin(a)*cos(b) + sin(b)*cos(a) = sin(pi/2) = 1",
        "{ sin(x_0)*cos(x_1) + sin(x_1)*cos(x_0)"
        " | x_0 = 1.0471975511965976, x_1 = 0.5235987755982988 }",
        1.0, __LINE__);
    /* Gaussian envelope: c*exp(-x^2) */
    check_parse_val("c * exp(-x^2) at x=0 = c",
        "{ c * exp(-x_0^2) | x_0 = 0; c = 7 }",
        7.0, __LINE__);
    /* exp product identity: exp(f)*exp(-f) = 1 */
    check_parse_val("exp(sin(x)) * exp(-sin(x)) = 1",
        "{ exp(sin(x)) * exp(-sin(x)) | x = 0.7 }",
        1.0, __LINE__);
}

/* ---- ASCII alternative syntax — ^N exponent on function names ---- */

static void test_from_string_func_power(void)
{
    /* All 15 unary functions with ^2: exact-value cases */
    check_parse_val("sin^2(0) = 0",       "{ sin^2(x) | x = 0 }",   0.0, __LINE__);
    check_parse_val("cos^2(0) = 1",       "{ cos^2(x) | x = 0 }",   1.0, __LINE__);
    check_parse_val("tan^2(0) = 0",       "{ tan^2(x) | x = 0 }",   0.0, __LINE__);
    check_parse_val("sinh^2(0) = 0",      "{ sinh^2(x) | x = 0 }",  0.0, __LINE__);
    check_parse_val("cosh^2(0) = 1",      "{ cosh^2(x) | x = 0 }",  1.0, __LINE__);
    check_parse_val("tanh^2(0) = 0",      "{ tanh^2(x) | x = 0 }",  0.0, __LINE__);
    check_parse_val("asin^2(0) = 0",      "{ asin^2(x) | x = 0 }",  0.0, __LINE__);
    check_parse_val("acos^2(1) = 0",      "{ acos^2(x) | x = 1 }",  0.0, __LINE__);
    check_parse_val("atan^2(0) = 0",      "{ atan^2(x) | x = 0 }",  0.0, __LINE__);
    check_parse_val("asinh^2(0) = 0",     "{ asinh^2(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("acosh^2(1) = 0",     "{ acosh^2(x) | x = 1 }", 0.0, __LINE__);
    check_parse_val("atanh^2(0) = 0",     "{ atanh^2(x) | x = 0 }", 0.0, __LINE__);
    check_parse_val("exp^2(0) = 1",       "{ exp^2(x) | x = 0 }",   1.0, __LINE__);
    check_parse_val("log^2(e) = 1",
        "{ log^2(x) | x = 2.718281828459045 }",                      1.0, __LINE__);
    check_parse_val("sqrt^2(9) = 9",      "{ sqrt^2(x) | x = 9 }",  9.0, __LINE__);
    /* Non-trivial exponent values */
    check_parse_val("tan^2(pi/4) = 1",
        "{ tan^2(x) | x = 0.7853981633974483 }",                     1.0, __LINE__);
    check_parse_val("sqrt^3(4) = 8",      "{ sqrt^3(x) | x = 4 }",  8.0, __LINE__);
    check_parse_val("exp^3(0) = 1",       "{ exp^3(x) | x = 0 }",   1.0, __LINE__);
    /* Multi-digit exponent */
    check_parse_val("sin^10(0) = 0",      "{ sin^10(x) | x = 0 }",  0.0, __LINE__);
    check_parse_val("cos^10(0) = 1",      "{ cos^10(x) | x = 0 }",  1.0, __LINE__);
    /* Pythagorean identities expressed with ^N */
    check_parse_val("sin^2(x) + cos^2(x) = 1",
        "{ sin^2(x) + cos^2(x) | x = 1.234 }",                      1.0, __LINE__);
    check_parse_val("cosh^2(x) - sinh^2(x) = 1",
        "{ cosh^2(x) - sinh^2(x) | x = 2.5 }",                      1.0, __LINE__);
    check_parse_val("sqrt(sin^2(x) + cos^2(x)) = 1",
        "{ sqrt(sin^2(x) + cos^2(x)) | x = 0.9 }",                  1.0, __LINE__);
    /* ^N combined with subscripted variable names */
    check_parse_val("sin^2(x_0) + cos^2(x_0) = 1 (subscript + ^N)",
        "{ sin^2(x_0) + cos^2(x_0) | x_0 = 0.7 }",                  1.0, __LINE__);
    check_parse_val("exp^2(x_0) * exp(-2*x_0^2) at x_0=1",
        "{ exp^2(x_0) * exp(-2*x_0^2) | x_0 = 1 }",                 1.0, __LINE__);
}

/* ---- Complex composed expressions using all ASCII alternatives ---- */

static void test_from_string_composed(void)
{
    /* Euclidean distance in 2D: sqrt(x^2 + y^2) */
    check_parse_val("sqrt(x_0^2 + x_1^2) = 5 (3-4-5 triangle)",
        "{ sqrt(x_0^2 + x_1^2) | x_0 = 3, x_1 = 4 }",
        5.0, __LINE__);

    /* exp / log mutual inverses */
    check_parse_val("exp(log(x_0)) = x_0 = 3",
        "{ exp(log(x_0)) | x_0 = 3 }",
        3.0, __LINE__);
    check_parse_val("log(exp(x_0)) = x_0 = 2.5",
        "{ log(exp(x_0)) | x_0 = 2.5 }",
        2.5, __LINE__);

    /* Trig inverse pairs */
    check_parse_val("asin(sin(x_0)) = 0.5",
        "{ asin(sin(x_0)) | x_0 = 0.5 }",
        0.5, __LINE__);
    check_parse_val("acos(cos(x_0)) = 0.6",
        "{ acos(cos(x_0)) | x_0 = 0.6 }",
        0.6, __LINE__);
    check_parse_val("atan(tan(x_0)) = pi/6",
        "{ atan(tan(x_0)) | x_0 = 0.5235987755982988 }",
        0.5235987755982988, __LINE__);

    /* Hyperbolic inverse pairs */
    check_parse_val("asinh(sinh(x_0)) = 1.5",
        "{ asinh(sinh(x_0)) | x_0 = 1.5 }",
        1.5, __LINE__);
    check_parse_val("acosh(cosh(x_0)) = 1.2",
        "{ acosh(cosh(x_0)) | x_0 = 1.2 }",
        1.2, __LINE__);
    check_parse_val("atanh(tanh(x_0)) = 0.8",
        "{ atanh(tanh(x_0)) | x_0 = 0.8 }",
        0.8, __LINE__);

    /* atan2 recovers angle from unit-circle coordinates */
    check_parse_val("atan2(sin(x_0), cos(x_0)) = x_0",
        "{ atan2(sin(x_0), cos(x_0)) | x_0 = 0.5 }",
        0.5, __LINE__);

    /* tan(x)*cos(x) = sin(x): at x=pi/6, result = 0.5 */
    check_parse_val("tan(x_0) * cos(x_0) = sin(x_0) = 0.5",
        "{ tan(x_0) * cos(x_0) | x_0 = 0.5235987755982988 }",
        0.5, __LINE__);

    /* exp product identity: exp(f(x)) * exp(-f(x)) = 1 */
    check_parse_val("exp(sin(x)) * exp(-sin(x)) = 1",
        "{ exp(sin(x)) * exp(-sin(x)) | x = 0.7 }",
        1.0, __LINE__);
    check_parse_val("exp(cos(x)) * exp(-cos(x)) = 1",
        "{ exp(cos(x)) * exp(-cos(x)) | x = 1.2 }",
        1.0, __LINE__);
    check_parse_val("exp(log(x_0)) * exp(-log(x_0)) = 1",
        "{ exp(log(x_0)) * exp(-log(x_0)) | x_0 = 4 }",
        1.0, __LINE__);

    /* c_0*sin^2(x_0) + c_1*cos^2(x_1): three ASCII features together */
    check_parse_val("c_0*sin^2(pi/2) + c_1*cos^2(0) = 3+5 = 8",
        "{ c_0*sin^2(x_0) + c_1*cos^2(x_1)"
        " | x_0 = 1.5707963267948966, x_1 = 0; c_0 = 3, c_1 = 5 }",
        8.0, __LINE__);

    /* Gaussian bell: c*exp(-x^2) at peak */
    check_parse_val("c_0 * exp(-x_0^2) at x_0=0 = c_0",
        "{ c_0 * exp(-x_0^2) | x_0 = 0; c_0 = 4 }",
        4.0, __LINE__);

    /* Chain: exp(c_0*x_0^2) * sin^2(x_1) + log(x_2) = 1 */
    check_parse_val("exp(c*x^2)*sin^2(y) + log(z) = 1",
        "{ exp(c_0*x_0^2)*sin^2(x_1) + log(x_2)"
        " | x_0 = 0, x_1 = 1.5707963267948966, x_2 = 1; c_0 = 1 }",
        1.0, __LINE__);

    /* sqrt(exp(2*ln(3))) = sqrt(9) = 3 */
    check_parse_val("sqrt(exp(c_0 * x_0)) = 3",
        "{ sqrt(exp(c_0 * x_0)) | x_0 = 1.0986122886681098; c_0 = 2 }",
        3.0, __LINE__);

    /* log(x_0^3) = 3*log(x_0): at x_0=e this is 3 */
    check_parse_val("log(x_0^3) = 3*log(e) = 3",
        "{ log(x_0^3) | x_0 = 2.718281828459045 }",
        3.0, __LINE__);

    /* cosh^2(x_0) - sinh^2(x_0) = 1 with subscripted name */
    check_parse_val("cosh^2(x_0) - sinh^2(x_0) = 1",
        "{ cosh^2(x_0) - sinh^2(x_0) | x_0 = 3.1 }",
        1.0, __LINE__);

    /* Four-variable expression: a*sin(x) + b*cos(y) + c*exp(-z) + d */
    check_parse_val("a*sin(x)+b*cos(y)+c*exp(-z)+d at zeros",
        "{ c_0*sin(x_0) + c_1*cos(x_1) + c_2*exp(-x_2) + c_3"
        " | x_0 = 0, x_1 = 0, x_2 = 0; c_0 = 1, c_1 = 2, c_2 = 3, c_3 = 4 }",
        0.0 + 2.0 + 3.0 + 4.0, __LINE__);
}

/* ---- Group runner for all ASCII-alternative tests ---- */

static void test_from_string_ascii_alternatives(void)
{
    RUN_TEST(test_from_string_subscripts, __func__);
    RUN_TEST(test_from_string_star_mul,   __func__);
    RUN_TEST(test_from_string_func_power, __func__);
    RUN_TEST(test_from_string_composed,   __func__);
}

/* ---- f, f', f'' of exp(sin(x)) + 3*x^2 - 7: parse, evaluate, differentiate ----
 *
 * The three explicit strings are the value, first derivative, and second
 * derivative of f(x) = exp(sin(x)) + 3*x² - 7 at x = 1.25.
 *
 * f'  = cos(x)·exp(sin(x)) + 6x
 * f'' = exp(sin(x))·(cos²(x) − sin(x)) + 6
 *
 * Tests cover: explicit star (*), implicit mul (6x), function power (cos^2),
 * parenthesised grouping ((cos^2(x) − sin(x))·exp(sin(x)) + 6),
 * and programmatic differentiation of a parsed dval_t.
 */

/* Inline comparison helper for the derivative checks below. */
static void check_dval_d(const char *label, const dval_t *node,
                          double expect, int line)
{
    qfloat_t qval = dv_eval(node);
    double got   = qf_to_double(qval);
    double err   = fabs(got - expect);
    double rel   = (fabs(expect) > 0.0) ? err / fabs(expect) : err;
    const double TOL = 2e-14;
    char *expr = dv_to_string(node, style_EXPRESSION);
    if (err < TOL || rel < TOL) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n", label);
        printf(C_BOLD "  expr   " C_RESET "%s\n", expr ? expr : "(null)");
        qf_printf(C_BOLD "  value  " C_RESET "%.34q\n\n", qval);
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " %s %s:%d:1\n", label, __FILE__, line);
        printf(C_BOLD "  expr   " C_RESET "%s\n", expr ? expr : "(null)");
        qf_printf(C_BOLD "  got    " C_RESET "%.34q\n", qval);
        printf(C_BOLD "  expect " C_RESET "%.17g\n\n", expect);
        TEST_FAIL();
    }
    free(expr);
}

static void test_from_string_deriv(void)
{
    const double xv  = 1.25;
    const double sx  = sin(xv);
    const double cx  = cos(xv);
    const double esx = exp(sx);

    /* ---- Explicit parse-and-evaluate: f, f', f'' as strings ---- */

    /* f(x) = exp(sin(x)) + 3*x^2 - 7 */
    check_parse_val("f = exp(sin(x)) + 3*x^2 - 7 at x=1.25",
        "{ exp(sin(x)) + 3*x^2 - 7 | x = 1.25 }",
        esx + 3*xv*xv - 7, __LINE__);

    /* f'(x) = cos(x)*exp(sin(x)) + 6*x  (explicit star) */
    check_parse_val("f' = cos(x)*exp(sin(x)) + 6*x at x=1.25",
        "{ cos(x)*exp(sin(x)) + 6*x | x = 1.25 }",
        cx*esx + 6*xv, __LINE__);

    /* f'(x) same expression with implicit mul for 6x */
    check_parse_val("f' = cos(x)*exp(sin(x)) + 6x (implicit 6x) at x=1.25",
        "{ cos(x)*exp(sin(x)) + 6x | x = 1.25 }",
        cx*esx + 6*xv, __LINE__);

    /* f''(x) = cos^2(x)*exp(sin(x)) - sin(x)*exp(sin(x)) + 6  (expanded) */
    check_parse_val("f'' expanded: cos^2(x)*exp(sin(x)) - sin(x)*exp(sin(x)) + 6",
        "{ cos^2(x)*exp(sin(x)) - sin(x)*exp(sin(x)) + 6 | x = 1.25 }",
        cx*cx*esx - sx*esx + 6, __LINE__);

    /* f''(x) same value, factored with brackets: (cos^2(x) - sin(x))*exp(sin(x)) + 6 */
    check_parse_val("f'' factored: (cos^2(x) - sin(x))*exp(sin(x)) + 6",
        "{ (cos^2(x) - sin(x))*exp(sin(x)) + 6 | x = 1.25 }",
        (cx*cx - sx)*esx + 6, __LINE__);

    /* Additional bracket-heavy forms */
    check_parse_val("f'' double-bracket: (cos^2(x) - sin(x)) * (exp(sin(x))) + 6",
        "{ (cos^2(x) - sin(x)) * (exp(sin(x))) + 6 | x = 1.25 }",
        (cx*cx - sx)*esx + 6, __LINE__);
    check_parse_val("(sin(x) + cos(x))^2 at x=0 = 1",
        "{ (sin(x) + cos(x))^2 | x = 0 }",
        1.0, __LINE__);
    check_parse_val("exp((x_0 + x_1)^2) at (0,0) = 1",
        "{ exp((x_0 + x_1)^2) | x_0 = 0, x_1 = 0 }",
        1.0, __LINE__);
    check_parse_val("(x^2 + 1)^2 at x=2 = 25",
        "{ (x^2 + 1)^2 | x = 2 }",
        25.0, __LINE__);

    /* ---- Programmatic differentiation ---- */
    /* Build f(x) = exp(sin(x)) + 3*x^2 - 7 explicitly so we hold the wrt pointer. */
    {
        dval_t *xvar  = dv_new_named_var_d(xv, "x");
        dval_t *sinx  = dv_sin(xvar);
        dval_t *esinx = dv_exp(sinx);
        dval_t *x2    = dv_pow_d(xvar, 2.0);
        dval_t *t     = dv_mul_d(x2, 3.0);
        dval_t *t2    = dv_sub_d(t, 7.0);
        dval_t *f     = dv_add(esinx, t2);

        check_dval_d("f(1.25) via dv_eval_d",   f,   esx + 3*xv*xv - 7, __LINE__);

        dval_t *df  = dv_create_deriv(f,  xvar);
        check_dval_d("f'(1.25) via dv_create_deriv",  df,  cx*esx + 6*xv, __LINE__);

        dval_t *d2f = dv_create_deriv(df, xvar);
        check_dval_d("f''(1.25) via dv_create_deriv", d2f, cx*cx*esx - sx*esx + 6, __LINE__);

        dv_free(d2f); dv_free(df); dv_free(f);
        dv_free(t2); dv_free(t); dv_free(x2); dv_free(esinx); dv_free(sinx);
        dv_free(xvar);
    }
}

/* ---- Error paths — all must return NULL ----
 * Note: dval_from_string writes diagnostics to stderr for these cases. */

static void test_from_string_errors(void)
{
    /* NULL input (silent — no stderr output) */
    check_parse_null("NULL input",
        NULL, __LINE__);
    /* Missing opening '{' */
    check_parse_null("no opening brace",
        "x + 1", __LINE__);
    /* Missing closing '}' */
    check_parse_null("no closing brace",
        "{ x | x = 1", __LINE__);
    /* Unknown symbol in expression */
    check_parse_null("unknown symbol",
        "{ z | x = 1 }", __LINE__);
    /* Duplicate variable name */
    check_parse_null("duplicate var name",
        "{ x | x = 1, x = 2 }", __LINE__);
    /* Same name used as both variable and named constant */
    check_parse_null("var-const name clash",
        "{ x | x = 1; x = 2 }", __LINE__);
    /* Missing '=' in binding */
    check_parse_null("missing '=' in binding",
        "{ x | x 1 }", __LINE__);
    /* Missing numeric value after '=' in binding */
    check_parse_null("missing value in binding",
        "{ x | x = }", __LINE__);
}

/* ---- Round-trips: build → string → parse → compare value ---- */

static void test_from_string_round_trips(void)
{
    /* Unnamed-variable expressions (auto-subscripted names x₀, x₁, …) */
    check_roundtrip("u01: x0^2",                    make_expr_u01(), __LINE__);
    check_roundtrip("u02: x0^3",                    make_expr_u02(), __LINE__);
    check_roundtrip("u03: x0^2 * y0^3 * x0",       make_expr_u03(), __LINE__);
    check_roundtrip("u04: x0^2 + x0^2",             make_expr_u04(), __LINE__);
    check_roundtrip("u05: sin(x0) * cos(x0)",       make_expr_u05(), __LINE__);
    check_roundtrip("u06: exp(sin(x0)) * exp(cos(x0))", make_expr_u06(), __LINE__);
    /* Named-constant expressions */
    check_roundtrip("c01: c0 * x0^2",               make_expr_c01(), __LINE__);
    check_roundtrip("c02: c0 * sin(x0)",             make_expr_c02(), __LINE__);
    check_roundtrip("c03: x0 + x1 + c0",            make_expr_c03(), __LINE__);
    check_roundtrip("c04: c0*x0 + c1",              make_expr_c04(), __LINE__);
    /* Bracketed (multi-character) name expressions */
    check_roundtrip("l01: [radius]^2",               make_expr_l01(), __LINE__);
    check_roundtrip("l02: [base] * [height]",        make_expr_l02(), __LINE__);
    check_roundtrip("l03: [pi] * [radius]^2",        make_expr_l03(), __LINE__);
    check_roundtrip("l04: pi * [radius]^2",          make_expr_l04(), __LINE__);
    check_roundtrip("l05: sin([theta]) * cos([theta])", make_expr_l05(), __LINE__);
    check_roundtrip("l06: [pi] * [tau] * x",         make_expr_l06(), __LINE__);
    check_roundtrip("l07: [my var]^2",               make_expr_l07(), __LINE__);
    check_roundtrip("l08: [2pi] * x",                make_expr_l08(), __LINE__);
    check_roundtrip("l09: [x']^2",                   make_expr_l09(), __LINE__);
}

void test_dval_t_from_string(void)
{
    RUN_TEST(test_from_string_pure_const,           __func__);
    RUN_TEST(test_from_string_arithmetic,           __func__);
    RUN_TEST(test_from_string_functions,            __func__);
    RUN_TEST(test_from_string_special_functions,    __func__);
    RUN_TEST(test_from_string_named_consts,         __func__);
    RUN_TEST(test_from_string_bracketed_names,   __func__);
    RUN_TEST(test_from_string_ascii_alternatives,__func__);
    RUN_TEST(test_from_string_errors,            __func__);
    RUN_TEST(test_from_string_round_trips,       __func__);
    RUN_TEST(test_from_string_deriv,             __func__);
}

static void test_readme_example(void) {
    /* Create a named variable x with initial value 1.25 */
    dval_t *x = dv_new_named_var_d(1.25, "x");

    /* Build expression:
         f(x) = exp(sin(x)) + 3*x^2 - 7
    */
    dval_t *f = make_f(x);

    /* First derivative (owning) */
    dval_t *df_dx = dv_create_deriv(f, x);   /* df/dx */

    /* Second derivative (borrowed) */
    const dval_t *d2f_dx = dv_get_deriv(df_dx, x);  /* d²f/dx² */

    /* Evaluate */
    qfloat_t f_val    = dv_eval(f);
    qfloat_t d1_val   = dv_eval(df_dx);
    qfloat_t d2_val   = dv_eval(d2f_dx);

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

void test_arithmetic(void) {
    RUN_TEST(test_add, __func__);
    RUN_TEST(test_sub, __func__);
    RUN_TEST(test_mul, __func__);
    RUN_TEST(test_div, __func__);
    RUN_TEST(test_mixed, __func__);
}

void test_d_variants(void) {
    RUN_TEST(test_add_d, __func__);
    RUN_TEST(test_mul_d, __func__);
    RUN_TEST(test_div_d, __func__);
}

void test_maths_functions(void) {
    RUN_TEST(test_sin, __func__);
    RUN_TEST(test_cos, __func__);
    RUN_TEST(test_tan, __func__);
    RUN_TEST(test_sinh, __func__);
    RUN_TEST(test_cosh, __func__);
    RUN_TEST(test_tanh, __func__);
    RUN_TEST(test_asin, __func__);
    RUN_TEST(test_acos, __func__);
    RUN_TEST(test_atan, __func__);
    RUN_TEST(test_atan2, __func__);
    RUN_TEST(test_asinh, __func__);
    RUN_TEST(test_acosh, __func__);
    RUN_TEST(test_atanh, __func__);
    RUN_TEST(test_exp, __func__);
    RUN_TEST(test_log, __func__);
    RUN_TEST(test_sqrt, __func__);
    RUN_TEST(test_pow_d, __func__);
    RUN_TEST(test_pow, __func__);
    RUN_TEST(test_abs, __func__);
    RUN_TEST(test_hypot, __func__);
    RUN_TEST(test_erf, __func__);
    RUN_TEST(test_erfc, __func__);
    RUN_TEST(test_erfinv, __func__);
    RUN_TEST(test_erfcinv, __func__);
    RUN_TEST(test_gamma, __func__);
    RUN_TEST(test_lgamma, __func__);
    RUN_TEST(test_digamma, __func__);
    RUN_TEST(test_trigamma, __func__);
    RUN_TEST(test_lambert_w0, __func__);
    RUN_TEST(test_lambert_wm1, __func__);
    RUN_TEST(test_normal_pdf, __func__);
    RUN_TEST(test_normal_cdf, __func__);
    RUN_TEST(test_normal_logpdf, __func__);
    RUN_TEST(test_ei, __func__);
    RUN_TEST(test_e1, __func__);
    RUN_TEST(test_beta, __func__);
    RUN_TEST(test_logbeta, __func__);
}

void test_first_derivatives(void) {
    RUN_TEST(test_deriv_const, __func__);
    RUN_TEST(test_deriv_var, __func__);
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

void test_second_derivatives(void) {
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

void test_dval_t_to_string(void) {
    RUN_TEST(test_to_string_all, __func__);
    RUN_TEST(test_expressions, __func__);
    RUN_TEST(test_expressions_unnamed, __func__);
    RUN_TEST(test_expressions_longname, __func__);
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
    qfloat_t f_val  = dv_eval(f);
    qfloat_t d1_val = dv_eval(df_dx);
    qfloat_t d2_val = dv_eval(d2f_dx);

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

    check_q_at(__FILE__, __LINE__, 1, "f(1,2)        = 7", dv_eval(f),        qf_from_double(7.0));
    check_q_at(__FILE__, __LINE__, 1, "∂f/∂x(1,2)   = 4", dv_eval(df_dx),    qf_from_double(4.0));
    check_q_at(__FILE__, __LINE__, 1, "∂f/∂y(1,2)   = 5", dv_eval(df_dy),    qf_from_double(5.0));
    check_q_at(__FILE__, __LINE__, 1, "∂²f/∂x∂y     = 1", dv_eval(d2f_dxdy), qf_from_double(1.0));

    /* borrowed pointer — same result, no extra free */
    const dval_t *p = dv_get_deriv(f, x);
    check_q_at(__FILE__, __LINE__, 1, "dv_get_deriv == dv_create_deriv", dv_eval(p), qf_from_double(4.0));

    /* update x=3; cached partial graphs recompute automatically */
    dv_set_val_d(x, 3.0);
    check_q_at(__FILE__, __LINE__, 1, "∂f/∂x(3,2)   = 8", dv_eval(df_dx), qf_from_double(8.0));
    check_q_at(__FILE__, __LINE__, 1, "∂f/∂y(3,2)   = 7", dv_eval(df_dy), qf_from_double(7.0));

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
/* ------------------------------------------------------------------------- */

/* f(x,y) = x*y  →  ∂f/∂x = y,  ∂f/∂y = x,  ∂²f/∂x∂y = 1 */
static void test_partial_xy_product(void)
{
    dval_t *x = dv_new_var_d(2.0);
    dval_t *y = dv_new_var_d(3.0);
    dval_t *f = dv_mul(x, y);

    dval_t *df_dx = dv_create_deriv(f, x);
    dval_t *df_dy = dv_create_deriv(f, y);
    dval_t *d2f   = dv_create_2nd_deriv(f, x, y);

    check_q_at(__FILE__, __LINE__, 1, "∂(x*y)/∂x at x=2,y=3", dv_eval(df_dx), qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "∂(x*y)/∂y at x=2,y=3", dv_eval(df_dy), qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "∂²(x*y)/∂x∂y",         dv_eval(d2f),   qf_from_double(1.0));

    dv_free(d2f);
    dv_free(df_dy);
    dv_free(df_dx);
    dv_free(f);
    dv_free(y);
    dv_free(x);
}

/* f(x,y) = x² + y³  →  ∂f/∂x = 2x,  ∂f/∂y = 3y²,  ∂²f/∂x² = 2,  ∂²f/∂y² = 6y */
static void test_partial_poly(void)
{
    dval_t *x  = dv_new_var_d(2.0);
    dval_t *y  = dv_new_var_d(3.0);
    dval_t *x2 = dv_pow_d(x, 2.0);
    dval_t *y3 = dv_pow_d(y, 3.0);
    dval_t *f  = dv_add(x2, y3);

    dval_t *df_dx   = dv_create_deriv(f, x);
    dval_t *df_dy   = dv_create_deriv(f, y);
    dval_t *d2f_dx2 = dv_create_2nd_deriv(f, x, x);
    dval_t *d2f_dy2 = dv_create_2nd_deriv(f, y, y);

    /* at x=2: ∂f/∂x = 2*2 = 4 */
    check_q_at(__FILE__, __LINE__, 1, "∂(x²+y³)/∂x at x=2",   dv_eval(df_dx),   qf_from_double(4.0));
    /* at y=3: ∂f/∂y = 3*9 = 27 */
    check_q_at(__FILE__, __LINE__, 1, "∂(x²+y³)/∂y at y=3",   dv_eval(df_dy),   qf_from_double(27.0));
    /* ∂²f/∂x² = 2 */
    check_q_at(__FILE__, __LINE__, 1, "∂²(x²+y³)/∂x² = 2",    dv_eval(d2f_dx2), qf_from_double(2.0));
    /* at y=3: ∂²f/∂y² = 6y = 18 */
    check_q_at(__FILE__, __LINE__, 1, "∂²(x²+y³)/∂y² at y=3", dv_eval(d2f_dy2), qf_from_double(18.0));

    dv_free(d2f_dy2);
    dv_free(d2f_dx2);
    dv_free(df_dy);
    dv_free(df_dx);
    dv_free(f);
    dv_free(y3);
    dv_free(x2);
    dv_free(y);
    dv_free(x);
}

/* f(x,y) = sin(x) * exp(y)  →  ∂f/∂x = cos(x)*exp(y),  ∂f/∂y = sin(x)*exp(y),
   ∂²f/∂x∂y = cos(x)*exp(y) */
static void test_partial_sin_exp(void)
{
    dval_t *x    = dv_new_var_d(1.0);
    dval_t *y    = dv_new_var_d(2.0);
    dval_t *sinx = dv_sin(x);
    dval_t *expy = dv_exp(y);
    dval_t *f    = dv_mul(sinx, expy);

    dval_t *df_dx = dv_create_deriv(f, x);
    dval_t *df_dy = dv_create_deriv(f, y);
    dval_t *d2f   = dv_create_2nd_deriv(f, x, y);

    qfloat_t qcos1   = qf_cos(qf_from_double(1.0));
    qfloat_t qsin1   = qf_sin(qf_from_double(1.0));
    qfloat_t qexp2   = qf_exp(qf_from_double(2.0));
    qfloat_t cos_exp = qf_mul(qcos1, qexp2);
    qfloat_t sin_exp = qf_mul(qsin1, qexp2);

    check_q_at(__FILE__, __LINE__, 1, "∂(sin(x)exp(y))/∂x",    dv_eval(df_dx), cos_exp);
    check_q_at(__FILE__, __LINE__, 1, "∂(sin(x)exp(y))/∂y",    dv_eval(df_dy), sin_exp);
    check_q_at(__FILE__, __LINE__, 1, "∂²(sin(x)exp(y))/∂x∂y", dv_eval(d2f),   cos_exp);

    dv_free(d2f);
    dv_free(df_dy);
    dv_free(df_dx);
    dv_free(f);
    dv_free(expy);
    dv_free(sinx);
    dv_free(y);
    dv_free(x);
}

/* Cross-partial symmetry: ∂²f/∂x∂y == ∂²f/∂y∂x for f = x*y + x²*y */
static void test_partial_symmetry(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *y   = dv_new_var_d(3.0);
    dval_t *xy  = dv_mul(x, y);
    dval_t *x2  = dv_pow_d(x, 2.0);
    dval_t *x2y = dv_mul(x2, y);
    dval_t *f   = dv_add(xy, x2y);  /* f = xy + x²y */

    dval_t *dxy  = dv_create_2nd_deriv(f, x, y);
    dval_t *dyx  = dv_create_2nd_deriv(f, y, x);

    /* ∂²f/∂x∂y = x + 2x*1 ... actually ∂f/∂x = y + 2xy, ∂²f/∂x∂y = 1 + 2x = 5 at x=2 */
    check_q_at(__FILE__, __LINE__, 1, "∂²f/∂x∂y at x=2,y=3", dv_eval(dxy), qf_from_double(5.0));
    check_q_at(__FILE__, __LINE__, 1, "∂²f/∂y∂x at x=2,y=3", dv_eval(dyx), qf_from_double(5.0));

    dv_free(dyx);
    dv_free(dxy);
    dv_free(f);
    dv_free(x2y);
    dv_free(x2);
    dv_free(xy);
    dv_free(y);
    dv_free(x);
}

/* dv_get_deriv returns a borrowed pointer; verify it evaluates correctly
   and that repeated calls return the cached result (same pointer) */
static void test_partial_get_borrowed(void)
{
    dval_t *x = dv_new_var_d(4.0);
    dval_t *y = dv_new_var_d(5.0);
    dval_t *f = dv_mul(x, y);  /* f = x*y */

    const dval_t *p1 = dv_get_deriv(f, x);
    const dval_t *p2 = dv_get_deriv(f, x);  /* should be cached */

    if (p1 != p2) {
        printf(C_BOLD C_RED "FAIL" C_RESET
               " dv_get_deriv not cached %s:%d:1\n", __FILE__, __LINE__);
        TEST_FAIL();
    } else {
        printf(C_BOLD C_GREEN "PASS" C_RESET " dv_get_deriv returns cached pointer\n");
    }

    check_q_at(__FILE__, __LINE__, 1, "dv_get_deriv(x*y, x) = y = 5", dv_eval(p1), qf_from_double(5.0));

    dv_free(f);
    dv_free(y);
    dv_free(x);
}

/* Symbolic expression-style string output for partial derivative nodes */
static void test_partial_to_string(void)
{
    fprintf(stderr, "\n  [%s]\n", __func__);
    dval_t *x = dv_new_named_var_d(2.0, "x");
    dval_t *y = dv_new_named_var_d(3.0, "y");

    /* f = xy */
    dval_t *f        = dv_mul(x, y);
    dval_t *df_dx    = dv_create_deriv(f, x);   /* simplifies to y */
    dval_t *df_dy    = dv_create_deriv(f, y);   /* simplifies to x */
    dval_t *d2f_dxdy = dv_create_2nd_deriv(f, x, y); /* simplifies to 1 */

    char *s;

    s = dv_to_string(f, style_EXPRESSION);
    to_string_pass("f = xy (EXPR)", s, "{ xy | x = 2, y = 3 }");
    if (!str_eq(s, "{ xy | x = 2, y = 3 }"))
        to_string_fail(__FILE__, __LINE__, 1, "f = xy (EXPR)", s, "{ xy | x = 2, y = 3 }");
    free(s);

    s = dv_to_string(df_dx, style_EXPRESSION);
    if (str_eq(s, "{ y | y = 3 }"))
        to_string_pass("∂(xy)/∂x (EXPR)", s, "{ y | y = 3 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(xy)/∂x (EXPR)", s, "{ y | y = 3 }");
    free(s);

    s = dv_to_string(df_dy, style_EXPRESSION);
    if (str_eq(s, "{ x | x = 2 }"))
        to_string_pass("∂(xy)/∂y (EXPR)", s, "{ x | x = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(xy)/∂y (EXPR)", s, "{ x | x = 2 }");
    free(s);

    s = dv_to_string(d2f_dxdy, style_EXPRESSION);
    if (str_eq(s, "{ c = 1 }"))
        to_string_pass("∂²(xy)/∂x∂y (EXPR)", s, "{ c = 1 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²(xy)/∂x∂y (EXPR)", s, "{ c = 1 }");
    free(s);

    /* g = x² + xy + y² */
    dval_t *x2 = dv_pow_d(x, 2.0);
    dval_t *xy = dv_mul(x, y);
    dval_t *y2 = dv_pow_d(y, 2.0);
    dval_t *t0 = dv_add(x2, xy);
    dval_t *g  = dv_add(t0, y2);

    dval_t *dg_dx = dv_create_deriv(g, x);  /* 2x + y */
    dval_t *dg_dy = dv_create_deriv(g, y);  /* x + 2y */

    s = dv_to_string(dg_dx, style_EXPRESSION);
    if (str_eq(s, "{ 2x + y | x = 2, y = 3 }"))
        to_string_pass("∂(x²+xy+y²)/∂x (EXPR)", s, "{ 2x + y | x = 2, y = 3 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(x²+xy+y²)/∂x (EXPR)", s, "{ 2x + y | x = 2, y = 3 }");
    free(s);

    s = dv_to_string(dg_dy, style_EXPRESSION);
    if (str_eq(s, "{ x + 2y | x = 2, y = 3 }"))
        to_string_pass("∂(x²+xy+y²)/∂y (EXPR)", s, "{ x + 2y | x = 2, y = 3 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(x²+xy+y²)/∂y (EXPR)", s, "{ x + 2y | x = 2, y = 3 }");
    free(s);

    dv_free(dg_dy); dv_free(dg_dx);
    dv_free(g); dv_free(t0); dv_free(y2); dv_free(xy); dv_free(x2);
    dv_free(d2f_dxdy); dv_free(df_dy); dv_free(df_dx); dv_free(f);
    dv_free(y); dv_free(x);
}

/* f(x,y) = sin(x)·exp(y) — symbolic output checks with elementary functions */
static void test_partial_to_string_functions(void)
{
    fprintf(stderr, "\n  [%s]\n", __func__);
    dval_t *x    = dv_new_named_var_d(1.0, "x");
    dval_t *y    = dv_new_named_var_d(2.0, "y");
    dval_t *sinx = dv_sin(x);
    dval_t *expy = dv_exp(y);
    dval_t *f    = dv_mul(sinx, expy);

    dval_t *df_dx    = dv_create_deriv(f, x);   /* cos(x)·exp(y)  */
    dval_t *df_dy    = dv_create_deriv(f, y);   /* sin(x)·exp(y) = f */
    dval_t *d2f_dx2  = dv_create_2nd_deriv(f, x, x); /* -sin(x)·exp(y) */
    dval_t *d2f_dxdy = dv_create_2nd_deriv(f, x, y); /* cos(x)·exp(y)  */

    char *s;

    s = dv_to_string(f, style_EXPRESSION);
    if (str_eq(s, "{ sin(x)·exp(y) | x = 1, y = 2 }"))
        to_string_pass("sin(x)·exp(y) (EXPR)", s, "{ sin(x)·exp(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "sin(x)·exp(y) (EXPR)", s, "{ sin(x)·exp(y) | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(df_dx, style_EXPRESSION);
    if (str_eq(s, "{ cos(x)·exp(y) | x = 1, y = 2 }"))
        to_string_pass("∂(sin(x)·exp(y))/∂x (EXPR)", s, "{ cos(x)·exp(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(sin(x)·exp(y))/∂x (EXPR)", s, "{ cos(x)·exp(y) | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(df_dy, style_EXPRESSION);
    if (str_eq(s, "{ sin(x)·exp(y) | x = 1, y = 2 }"))
        to_string_pass("∂(sin(x)·exp(y))/∂y (EXPR)", s, "{ sin(x)·exp(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(sin(x)·exp(y))/∂y (EXPR)", s, "{ sin(x)·exp(y) | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(d2f_dx2, style_EXPRESSION);
    if (str_eq(s, "{ -sin(x)·exp(y) | x = 1, y = 2 }"))
        to_string_pass("∂²(sin(x)·exp(y))/∂x² (EXPR)", s, "{ -sin(x)·exp(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²(sin(x)·exp(y))/∂x² (EXPR)", s, "{ -sin(x)·exp(y) | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(d2f_dxdy, style_EXPRESSION);
    if (str_eq(s, "{ cos(x)·exp(y) | x = 1, y = 2 }"))
        to_string_pass("∂²(sin(x)·exp(y))/∂x∂y (EXPR)", s, "{ cos(x)·exp(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²(sin(x)·exp(y))/∂x∂y (EXPR)", s, "{ cos(x)·exp(y) | x = 1, y = 2 }");
    free(s);

    dv_free(d2f_dxdy); dv_free(d2f_dx2); dv_free(df_dy); dv_free(df_dx);
    dv_free(f); dv_free(expy); dv_free(sinx); dv_free(y); dv_free(x);
}

/* f(x,y) = log(x² + y²)  — harmonic function; all partials involve the
   denominator (x² + y²) and its powers, exercising quotient-rule simplification */
static void test_partial_to_string_log_r2(void)
{
    fprintf(stderr, "\n  [%s]\n", __func__);
    dval_t *x   = dv_new_named_var_d(1.0, "x");
    dval_t *y   = dv_new_named_var_d(2.0, "y");
    dval_t *x2  = dv_pow_d(x, 2.0);
    dval_t *y2  = dv_pow_d(y, 2.0);
    dval_t *sum = dv_add(x2, y2);
    dval_t *f   = dv_log(sum);

    dval_t *df_dx    = dv_create_deriv(f, x);
    dval_t *df_dy    = dv_create_deriv(f, y);
    dval_t *d2f_dx2  = dv_create_2nd_deriv(f, x, x);
    dval_t *d2f_dxdy = dv_create_2nd_deriv(f, x, y);

    char *s;

    s = dv_to_string(f, style_EXPRESSION);
    if (str_eq(s, "{ log(x² + y²) | x = 1, y = 2 }"))
        to_string_pass("log(x²+y²) (EXPR)", s, "{ log(x² + y²) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "log(x²+y²) (EXPR)", s, "{ log(x² + y²) | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(df_dx, style_EXPRESSION);
    if (str_eq(s, "{ 2x/(x² + y²) | x = 1, y = 2 }"))
        to_string_pass("∂log(x²+y²)/∂x (EXPR)", s, "{ 2x/(x² + y²) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂log(x²+y²)/∂x (EXPR)", s, "{ 2x/(x² + y²) | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(df_dy, style_EXPRESSION);
    if (str_eq(s, "{ 2y/(x² + y²) | y = 2, x = 1 }"))
        to_string_pass("∂log(x²+y²)/∂y (EXPR)", s, "{ 2y/(x² + y²) | y = 2, x = 1 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂log(x²+y²)/∂y (EXPR)", s, "{ 2y/(x² + y²) | y = 2, x = 1 }");
    free(s);

    s = dv_to_string(d2f_dx2, style_EXPRESSION);
    if (str_eq(s, "{ (-2x² + 2y²)/(x² + y²)² | x = 1, y = 2 }"))
        to_string_pass("∂²log(x²+y²)/∂x² (EXPR)", s, "{ (-2x² + 2y²)/(x² + y²)² | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²log(x²+y²)/∂x² (EXPR)", s, "{ (-2x² + 2y²)/(x² + y²)² | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(d2f_dxdy, style_EXPRESSION);
    if (str_eq(s, "{ -4xy/(x² + y²)² | x = 1, y = 2 }"))
        to_string_pass("∂²log(x²+y²)/∂x∂y (EXPR)", s, "{ -4xy/(x² + y²)² | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²log(x²+y²)/∂x∂y (EXPR)", s, "{ -4xy/(x² + y²)² | x = 1, y = 2 }");
    free(s);

    dv_free(d2f_dxdy); dv_free(d2f_dx2); dv_free(df_dy); dv_free(df_dx);
    dv_free(f); dv_free(sum); dv_free(y2); dv_free(x2); dv_free(y); dv_free(x);
}

/* f(x,y) = sin(xy) + x·log(y)  — chain rule through a product argument plus
   a cross term; the mixed second partial -xy·sin(xy) + cos(xy) + 1/y exercises
   several simplification paths simultaneously */
static void test_partial_to_string_sin_xy(void)
{
    fprintf(stderr, "\n  [%s]\n", __func__);
    dval_t *x     = dv_new_named_var_d(1.0, "x");
    dval_t *y     = dv_new_named_var_d(2.0, "y");
    dval_t *xy    = dv_mul(x, y);
    dval_t *sinxy = dv_sin(xy);
    dval_t *logy  = dv_log(y);
    dval_t *xlogy = dv_mul(x, logy);
    dval_t *f     = dv_add(sinxy, xlogy);

    dval_t *df_dx    = dv_create_deriv(f, x);
    dval_t *df_dy    = dv_create_deriv(f, y);
    dval_t *d2f_dx2  = dv_create_2nd_deriv(f, x, x);
    dval_t *d2f_dxdy = dv_create_2nd_deriv(f, x, y);

    char *s;

    s = dv_to_string(f, style_EXPRESSION);
    if (str_eq(s, "{ sin(xy) + x·log(y) | x = 1, y = 2 }"))
        to_string_pass("sin(xy)+x·log(y) (EXPR)", s, "{ sin(xy) + x·log(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "sin(xy)+x·log(y) (EXPR)", s, "{ sin(xy) + x·log(y) | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(df_dx, style_EXPRESSION);
    if (str_eq(s, "{ y·cos(xy) + log(y) | x = 1, y = 2 }"))
        to_string_pass("∂(sin(xy)+x·log(y))/∂x (EXPR)", s, "{ y·cos(xy) + log(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(sin(xy)+x·log(y))/∂x (EXPR)", s, "{ y·cos(xy) + log(y) | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(df_dy, style_EXPRESSION);
    if (str_eq(s, "{ x·cos(xy) + x/y | x = 1, y = 2 }"))
        to_string_pass("∂(sin(xy)+x·log(y))/∂y (EXPR)", s, "{ x·cos(xy) + x/y | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(sin(xy)+x·log(y))/∂y (EXPR)", s, "{ x·cos(xy) + x/y | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(d2f_dx2, style_EXPRESSION);
    if (str_eq(s, "{ -y²·sin(xy) | x = 1, y = 2 }"))
        to_string_pass("∂²(sin(xy)+x·log(y))/∂x² (EXPR)", s, "{ -y²·sin(xy) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²(sin(xy)+x·log(y))/∂x² (EXPR)", s, "{ -y²·sin(xy) | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(d2f_dxdy, style_EXPRESSION);
    if (str_eq(s, "{ -xy·sin(xy) + cos(xy) + 1/y | x = 1, y = 2 }"))
        to_string_pass("∂²(sin(xy)+x·log(y))/∂x∂y (EXPR)", s, "{ -xy·sin(xy) + cos(xy) + 1/y | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²(sin(xy)+x·log(y))/∂x∂y (EXPR)", s, "{ -xy·sin(xy) + cos(xy) + 1/y | x = 1, y = 2 }");
    free(s);

    dv_free(d2f_dxdy); dv_free(d2f_dx2); dv_free(df_dy); dv_free(df_dx);
    dv_free(f); dv_free(xlogy); dv_free(logy); dv_free(sinxy); dv_free(xy);
    dv_free(y); dv_free(x);
}

void test_partial_derivatives(void)
{
    RUN_TEST(test_partial_xy_product,   __func__);
    RUN_TEST(test_partial_poly,         __func__);
    RUN_TEST(test_partial_sin_exp,      __func__);
    RUN_TEST(test_partial_symmetry,     __func__);
    RUN_TEST(test_partial_get_borrowed, __func__);
    RUN_TEST(test_partial_to_string,            __func__);
    RUN_TEST(test_partial_to_string_functions,  __func__);
    RUN_TEST(test_partial_to_string_log_r2,     __func__);
    RUN_TEST(test_partial_to_string_sin_xy,     __func__);
}

/* ------------------------------------------------------------------------- */
/* Precision / cache regressions                                             */
/* ------------------------------------------------------------------------- */

static void test_cmp_qfloat_precision(void)
{
    dval_t *a = dv_new_const(qf_from_string("1.00000000000000000001"));
    dval_t *b = dv_new_const(qf_from_string("1.00000000000000000002"));
    int cmp = dv_cmp(a, b);

    if (cmp < 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " dv_cmp respects qfloat precision\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET
               " dv_cmp lost qfloat precision %s:%d:1 (got %d, expected < 0)\n",
               __FILE__, __LINE__, cmp);
        TEST_FAIL();
    }

    dv_free(b);
    dv_free(a);
}

static void test_get_val_updates_after_set(void)
{
    dval_t *x = dv_new_var_d(1.0);
    dval_t *f = dv_add_d(x, 2.0);

    check_q_at(__FILE__, __LINE__, 1, "dv_get_val initial", dv_get_val(f), qf_from_double(3.0));
    dv_set_val_d(x, 5.0);
    check_q_at(__FILE__, __LINE__, 1, "dv_get_val after set", dv_get_val(f), qf_from_double(7.0));

    dv_free(f);
    dv_free(x);
}

static void test_simplify_inverse_unary_pairs(void)
{
    dval_t *x = dv_new_named_var_d(3.0, "x");
    dval_t *log_x = dv_log(x);
    dval_t *exp_log_x = dv_exp(log_x);
    dval_t *exp_x = dv_exp(x);
    dval_t *log_exp_x = dv_log(exp_x);
    char *exp_log_s = dv_to_string(exp_log_x, style_EXPRESSION);
    char *log_exp_s = dv_to_string(log_exp_x, style_EXPRESSION);
    const char *expect = "{ x | x = 3 }";

    check_q_at(__FILE__, __LINE__, 1, "exp(log(x)) eval", dv_eval(exp_log_x), qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "log(exp(x)) eval", dv_eval(log_exp_x), qf_from_double(3.0));

    if (str_eq(exp_log_s, expect))
        to_string_pass("exp(log(x)) simplification (EXPR)", exp_log_s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "exp(log(x)) simplification (EXPR)", exp_log_s, expect);

    if (str_eq(log_exp_s, expect))
        to_string_pass("log(exp(x)) simplification (EXPR)", log_exp_s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "log(exp(x)) simplification (EXPR)", log_exp_s, expect);

    free(log_exp_s);
    free(exp_log_s);
    dv_free(log_exp_x);
    dv_free(exp_x);
    dv_free(exp_log_x);
    dv_free(log_x);
    dv_free(x);
}

void test_runtime_regressions(void)
{
    RUN_TEST(test_cmp_qfloat_precision, __func__);
    RUN_TEST(test_get_val_updates_after_set, __func__);
    RUN_TEST(test_simplify_inverse_unary_pairs, __func__);
}

/* ------------------------------------------------------------------------- */
/* Reverse mode                                                              */
/* ------------------------------------------------------------------------- */

static void test_reverse_gradient_polynomial(void)
{
    dval_t *x  = dv_new_named_var_d(1.0, "x");
    dval_t *y  = dv_new_named_var_d(2.0, "y");
    dval_t *x2 = dv_pow_d(x, 2.0);
    dval_t *xy = dv_mul(x, y);
    dval_t *y2 = dv_pow_d(y, 2.0);
    dval_t *t0 = dv_add(x2, xy);
    dval_t *f  = dv_add(t0, y2);
    dval_t *vars[2] = { x, y };
    qfloat_t value;
    qfloat_t grads[2];

    if (dv_eval_derivatives(f, 2, vars, &value, grads) != 0) {
        printf(C_BOLD C_RED "FAIL" C_RESET " reverse polynomial gradient returned error\n");
        TEST_FAIL();
    }

    check_q_at(__FILE__, __LINE__, 1, "reverse value polynomial", value, qf_from_double(7.0));
    check_q_at(__FILE__, __LINE__, 1, "reverse df/dx polynomial", grads[0], qf_from_double(4.0));
    check_q_at(__FILE__, __LINE__, 1, "reverse df/dy polynomial", grads[1], qf_from_double(5.0));

    dv_free(f); dv_free(t0); dv_free(y2); dv_free(xy); dv_free(x2); dv_free(y); dv_free(x);
}

static void test_reverse_gradient_shared_subexpression(void)
{
    dval_t *x = dv_new_named_var_d(1.5, "x");
    dval_t *y = dv_new_named_var_d(-0.5, "y");
    dval_t *s = dv_add(x, y);
    dval_t *f = dv_mul(s, s);
    dval_t *vars[2] = { x, y };
    qfloat_t value;
    qfloat_t grads[2];

    if (dv_eval_derivatives(f, 2, vars, &value, grads) != 0) {
        printf(C_BOLD C_RED "FAIL" C_RESET " reverse shared-subexpression gradient returned error\n");
        TEST_FAIL();
    }

    check_q_at(__FILE__, __LINE__, 1, "reverse value shared", value, qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "reverse df/dx shared", grads[0], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "reverse df/dy shared", grads[1], qf_from_double(2.0));

    dv_free(f); dv_free(s); dv_free(y); dv_free(x);
}

static void test_reverse_matches_forward_composite(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *xy = dv_mul(x, y);
    dval_t *sin_xy = dv_sin(xy);
    dval_t *exp_term = dv_exp(sin_xy);
    dval_t *log_y = dv_log(y);
    dval_t *x_log_y = dv_mul(x, log_y);
    dval_t *f = dv_add(exp_term, x_log_y);
    dval_t *vars[2] = { x, y };
    qfloat_t grads[2];
    qfloat_t value;
    dval_t *df_dx = dv_create_deriv(f, x);
    dval_t *df_dy = dv_create_deriv(f, y);

    if (dv_eval_derivatives(f, 2, vars, &value, grads) != 0) {
        printf(C_BOLD C_RED "FAIL" C_RESET " reverse composite gradient returned error\n");
        TEST_FAIL();
    }

    check_q_at(__FILE__, __LINE__, 1, "reverse value composite", value, dv_eval(f));
    check_q_at(__FILE__, __LINE__, 1, "reverse matches forward df/dx", grads[0], dv_eval(df_dx));
    check_q_at(__FILE__, __LINE__, 1, "reverse matches forward df/dy", grads[1], dv_eval(df_dy));

    dv_free(df_dy); dv_free(df_dx);
    dv_free(f); dv_free(x_log_y); dv_free(log_y); dv_free(exp_term); dv_free(sin_xy); dv_free(xy);
    dv_free(y); dv_free(x);
}

static void test_reverse_gradient_missing_variable(void)
{
    dval_t *x = dv_new_named_var_d(3.0, "x");
    dval_t *z = dv_new_named_var_d(9.0, "z");
    dval_t *f = dv_mul_d(x, 4.0);
    dval_t *vars[2] = { x, z };
    qfloat_t grads[2];

    if (dv_eval_derivatives(f, 2, vars, NULL, grads) != 0) {
        printf(C_BOLD C_RED "FAIL" C_RESET " reverse missing-variable gradient returned error\n");
        TEST_FAIL();
    }

    check_q_at(__FILE__, __LINE__, 1, "reverse df/dx present", grads[0], qf_from_double(4.0));
    check_q_at(__FILE__, __LINE__, 1, "reverse df/dz missing", grads[1], QF_ZERO);

    dv_free(f); dv_free(z); dv_free(x);
}

void test_reverse_mode(void)
{
    RUN_TEST(test_reverse_gradient_polynomial, __func__);
    RUN_TEST(test_reverse_gradient_shared_subexpression, __func__);
    RUN_TEST(test_reverse_matches_forward_composite, __func__);
    RUN_TEST(test_reverse_gradient_missing_variable, __func__);
}

/* ------------------------------------------------------------------------- */
/* Test driver                                                                */
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

    printf(C_BOLD C_CYAN "=== Runtime regressions ===\n" C_RESET);
    RUN_TEST(test_runtime_regressions, NULL);

    printf(C_BOLD C_CYAN "=== Reverse mode ===\n" C_RESET);
    RUN_TEST(test_reverse_mode, NULL);

    printf(C_BOLD C_CYAN "=== README.md example ===\n" C_RESET);
    RUN_TEST(test_README_md_example, NULL);

    return tests_failed;
}
