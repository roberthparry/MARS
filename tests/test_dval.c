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
/* Compact qfloat comparison (kept exactly as-is, but using harness colours) */
/* ------------------------------------------------------------------------- */

static void check_q_at(const char *file, int line, int col,
                       const char *label, qfloat got, qfloat expect)
{
    qfloat diff = qf_sub(got, expect);
    double abs_err = fabs(qf_to_double(diff));
    double exp_d   = fabs(qf_to_double(expect));

    double rel_err = (exp_d > 0)
        ? abs_err / exp_d
        : abs_err;

    const double ABS_TOL = 2e-30;
    const double REL_TOL = 2e-30;

    if (abs_err < ABS_TOL || rel_err < REL_TOL) {
        printf("%sPASS%s %-32s  got=", C_GREEN, C_RESET, label);
        qf_printf("%.34q", got);
        printf("\n");
        return;
    }

    printf("%sFAIL%s %s: %s:%d:%d: got=",
           C_RED, C_RESET, label, file, line, col);

    qf_printf("%.34q", got);

    printf(" expect=");
    qf_printf("%.34q", expect);

    printf(" diff=");
    qf_printf("%.34q", diff);

    printf("\n");

    /* integrate with harness */
    tests_failed++;
}

/* ------------------------------------------------------------------------- */
/* Arithmetic tests                                                           */
/* ------------------------------------------------------------------------- */

void test_add(void)
{
    dval_t *c2 = dv_new_const_d(2.0);
    dval_t *c3 = dv_new_const_d(3.0);
    dval_t *f = dv_add(c2, c3);

    check_q_at(__FILE__, __LINE__, 1, "2+3",
               dv_eval(f), qf_from_double(5));

    dv_free(c2);
    dv_free(c3);
    dv_free(f);
}

void test_sub(void)
{
    dval_t *c10 = dv_new_const_d(10);
    dval_t *c4  = dv_new_const_d(4);
    dval_t *f   = dv_sub(c10, c4);

    check_q_at(__FILE__, __LINE__, 1, "10-4",
               dv_eval(f), qf_from_double(6));

    dv_free(f);
    dv_free(c4);
    dv_free(c10);
}

void test_mul(void)
{
    dval_t *c7 = dv_new_const_d(7);
    dval_t *c6 = dv_new_const_d(6);
    dval_t *f  = dv_mul(c7, c6);

    check_q_at(__FILE__, __LINE__, 1, "7*6",
               dv_eval(f), qf_from_double(42));

    dv_free(f);
    dv_free(c6);
    dv_free(c7);
}

void test_div(void)
{
    dval_t *c7  = dv_new_const_d(7);
    dval_t *c22 = dv_new_const_d(22);
    dval_t *f   = dv_div(c22, c7);

    check_q_at(__FILE__, __LINE__, 1, "22/7",
               dv_eval(f),
               qf_div(qf_from_double(22), qf_from_double(7)));

    dv_free(f);
    dv_free(c22);
    dv_free(c7);
}

void test_mixed(void)
{
    dval_t *two   = dv_new_const_d(2);
    dval_t *three = dv_new_const_d(3);
    dval_t *ten   = dv_new_const_d(10);
    dval_t *four  = dv_new_const_d(4);

    dval_t *add_2_3 = dv_add(two, three);
    dval_t *sub_10_4 = dv_sub(ten, four);
    dval_t *mul_add_sub = dv_mul(add_2_3, sub_10_4);
    dval_t *f = dv_div(mul_add_sub, two);

    check_q_at(__FILE__, __LINE__, 1, "mixed",
               dv_eval(f), qf_from_double(15));

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

    check_q_at(__FILE__, __LINE__, 1, "10+2.5",
               dv_eval(f), qf_from_double(12.5));

    dv_free(f);
    dv_free(ten);
}

void test_mul_d(void)
{
    dval_t *three = dv_new_const_d(3);
    dval_t *f     = dv_mul_d(three, 4);

    check_q_at(__FILE__, __LINE__, 1, "3*4",
               dv_eval(f), qf_from_double(12));

    dv_free(three);
    dv_free(f);
}

void test_div_d(void)
{
    dval_t *nine = dv_new_const_d(9);
    dval_t *f    = dv_div_d(nine, 3);

    check_q_at(__FILE__, __LINE__, 1, "9/3",
               dv_eval(f), qf_from_double(3));

    dv_free(f);
    dv_free(nine);
}

/* ------------------------------------------------------------------------- */
/* Mathematical function tests                                                */
/* ------------------------------------------------------------------------- */

void test_sin(void)
{
    dval_t *p = dv_new_const_d(0.5);
    dval_t *f = dv_sin(p);

    check_q_at(__FILE__, __LINE__, 1, "sin(0.5)",
               dv_eval(f), qf_sin(qf_from_double(0.5)));

    dv_free(f);
    dv_free(p);
}

void test_cos(void)
{
    dval_t *c = dv_new_const_d(0.5);
    dval_t *f = dv_cos(c);

    check_q_at(__FILE__, __LINE__, 1, "cos(0.5)",
               dv_eval(f), qf_cos(qf_from_double(0.5)));

    dv_free(f);
    dv_free(c);
}

void test_tan(void)
{
    dval_t *c = dv_new_const_d(0.5);
    dval_t *f = dv_tan(c);

    check_q_at(__FILE__, __LINE__, 1, "tan(0.5)",
               dv_eval(f), qf_tan(qf_from_double(0.5)));

    dv_free(f);
    dv_free(c);
}

void test_sinh(void)
{
    dval_t *c = dv_new_const_d(0.5);
    dval_t *f = dv_sinh(c);

    check_q_at(__FILE__, __LINE__, 1, "sinh(0.5)",
               dv_eval(f), qf_sinh(qf_from_double(0.5)));

    dv_free(f);
    dv_free(c);
}

void test_cosh(void)
{
    dval_t *c = dv_new_const_d(0.5);
    dval_t *f = dv_cosh(c);

    check_q_at(__FILE__, __LINE__, 1, "cosh(0.5)",
               dv_eval(f), qf_cosh(qf_from_double(0.5)));

    dv_free(f);
    dv_free(c);
}

void test_tanh(void)
{
    dval_t *c = dv_new_const_d(0.5);
    dval_t *f = dv_tanh(c);

    check_q_at(__FILE__, __LINE__, 1, "tanh(0.5)",
               dv_eval(f), qf_tanh(qf_from_double(0.5)));

    dv_free(f);
    dv_free(c);
}

void test_asin(void)
{
    dval_t *c = dv_new_const_d(0.25);
    dval_t *f = dv_asin(c);

    check_q_at(__FILE__, __LINE__, 1, "asin(0.25)",
               dv_eval(f), qf_asin(qf_from_double(0.25)));

    dv_free(f);
    dv_free(c);
}

void test_acos(void)
{
    dval_t *c = dv_new_const_d(0.25);
    dval_t *f = dv_acos(c);

    check_q_at(__FILE__, __LINE__, 1, "acos(0.25)",
               dv_eval(f), qf_acos(qf_from_double(0.25)));

    dv_free(f);
    dv_free(c);
}

void test_atan(void)
{
    dval_t *c = dv_new_const_d(0.25);
    dval_t *f = dv_atan(c);

    check_q_at(__FILE__, __LINE__, 1, "atan(0.25)",
               dv_eval(f), qf_atan(qf_from_double(0.25)));

    dv_free(f);
    dv_free(c);
}

void test_atan2(void)
{
    dval_t *base = dv_new_const_d(2.0);
    dval_t *expo = dv_new_const_d(3.0);
    dval_t *f    = dv_atan2(base, expo);

    check_q_at(__FILE__, __LINE__, 1, "atan2(2,3)",
               dv_eval(f),
               qf_atan2(qf_from_double(2.0), qf_from_double(3.0)));

    dv_free(f);
    dv_free(expo);
    dv_free(base);
}

void test_asinh(void)
{
    dval_t *c = dv_new_const_d(0.25);
    dval_t *f = dv_asinh(c);

    check_q_at(__FILE__, __LINE__, 1, "asinh(0.25)",
               dv_eval(f), qf_asinh(qf_from_double(0.25)));

    dv_free(f);
    dv_free(c);
}

void test_acosh(void)
{
    dval_t *c = dv_new_const_d(1.25);
    dval_t *f = dv_acosh(c);

    check_q_at(__FILE__, __LINE__, 1, "acosh(1.25)",
               dv_eval(f), qf_acosh(qf_from_double(1.25)));

    dv_free(f);
    dv_free(c);
}

void test_atanh(void)
{
    dval_t *c = dv_new_const_d(0.25);
    dval_t *f = dv_atanh(c);

    check_q_at(__FILE__, __LINE__, 1, "atanh(0.25)",
               dv_eval(f), qf_atanh(qf_from_double(0.25)));

    dv_free(f);
    dv_free(c);
}

void test_exp(void)
{
    dval_t *c = dv_new_const_d(1.5);
    dval_t *f = dv_exp(c);

    check_q_at(__FILE__, __LINE__, 1, "exp(1.5)",
               dv_eval(f), qf_exp(qf_from_double(1.5)));

    dv_free(f);
    dv_free(c);
}

void test_log(void)
{
    dval_t *c = dv_new_const_d(1.5);
    dval_t *f = dv_log(c);

    check_q_at(__FILE__, __LINE__, 1, "log(1.5)",
               dv_eval(f), qf_log(qf_from_double(1.5)));

    dv_free(f);
    dv_free(c);
}

void test_sqrt(void)
{
    dval_t *c = dv_new_const_d(2.0);
    dval_t *f = dv_sqrt(c);

    check_q_at(__FILE__, __LINE__, 1, "sqrt(2)",
               dv_eval(f), qf_sqrt(qf_from_double(2.0)));

    dv_free(f);
    dv_free(c);
}

void test_pow_d(void)
{
    dval_t *base = dv_new_const_d(2.0);
    dval_t *f    = dv_pow_d(base, 3.0);

    check_q_at(__FILE__, __LINE__, 1, "2^3(d)",
               dv_eval(f),
               qf_pow(qf_from_double(2.0), qf_from_double(3.0)));

    dv_free(f);
    dv_free(base);
}

void test_pow(void)
{
    dval_t *base = dv_new_const_d(2.0);
    dval_t *expo = dv_new_const_d(3.0);
    dval_t *f    = dv_pow(base, expo);

    check_q_at(__FILE__, __LINE__, 1, "2^3",
               dv_eval(f),
               qf_pow(qf_from_double(2.0), qf_from_double(3.0)));

    dv_free(f);
    dv_free(expo);
    dv_free(base);
}

/* ------------------------------------------------------------------------- */
/* First derivative tests                                                     */
/* ------------------------------------------------------------------------- */

void test_deriv_const(void)
{
    dval_t *c  = dv_new_const_d(5.0);
    dval_t *f  = c;
    dval_t *df = dv_create_deriv(f);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{5}",
               dv_eval(dv_get_deriv(df)), qf_from_double(0.0));

    dv_free(df);
    dv_free(c);
}

void test_deriv_var(void)
{
    dval_t *x = dv_new_var_d(2.0);
    const dval_t *dx = dv_get_deriv(x);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x} | x=2",
               dv_eval(dx), qf_from_double(1.0));

    dv_free(x);
}

void test_deriv_neg(void)
{
    dval_t *x = dv_new_var_d(3.0);
    dval_t *f = dv_neg(x);
    const dval_t *df = dv_get_deriv(f);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{-x} | x=3",
               dv_eval(df), qf_from_double(-1.0));

    dv_free(f);
    dv_free(x);
}

void test_deriv_add_d(void)
{
    dval_t *x = dv_new_var_d(2.0);
    dval_t *f = dv_add_d(x, 5.0);
    const dval_t *df = dv_get_deriv(f);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x+5} | x=2",
               dv_eval(df), qf_from_double(1.0));

    dv_free(x);
    dv_free(f);
}

void test_deriv_mul_d(void)
{
    dval_t *x = dv_new_var_d(4.0);
    dval_t *f = dv_mul_d(x, 7.0);
    const dval_t *df = dv_get_deriv(f);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{7x} | x=4",
               dv_eval(df), qf_from_double(7.0));

    dv_free(f);
    dv_free(x);
}

void test_deriv_div_d(void)
{
    dval_t *x = dv_new_var_d(9.0);
    dval_t *f = dv_div_d(x, 3.0);
    const dval_t *df = dv_get_deriv(f);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x/3} | x=9",
               dv_eval(df),
               qf_div(qf_from_double(1.0), qf_from_double(3.0)));

    dv_free(f);
    dv_free(x);
}

void test_deriv_x2(void)
{
    dval_t *x = dv_new_var_d(3.0);
    dval_t *f = dv_mul(x, x);
    const dval_t *df = dv_get_deriv(f);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^2} | x=3",
               dv_eval(df), qf_from_double(6.0));

    dv_free(f);
    dv_free(x);
}

void test_deriv_pow3(void)
{
    dval_t *x = dv_new_var_d(2.0);
    dval_t *f = dv_pow_d(x, 3.0);
    const dval_t *df = dv_get_deriv(f);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^3} | x=2",
               dv_eval(df), qf_from_double(12.0));

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
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(2.0);
    qfloat yval = qf_add(qf_mul(X, X), qf_from_double(1.0));
    qfloat fval = qf_pow(X, yval);

    qfloat term1 = qf_mul(qf_from_double(4.0), qf_log(X));
    qfloat term2 = qf_div(yval, qf_from_double(2.0));
    qfloat expect = qf_mul(fval, qf_add(term1, term2));

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{x^(x^2+1)} | x=2",
               dv_eval(df), expect);

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
    const dval_t *df = dv_get_deriv(f);

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{sin(x)} | x=0.5",
               dv_eval(df),
               qf_cos(qf_from_double(0.5)));

    dv_free(f);
    dv_free(x);
}

void test_deriv_cos(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_cos(x);
    const dval_t *df = dv_get_deriv(f);

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{cos(x)} | x=0.5",
               dv_eval(df),
               qf_neg(qf_sin(qf_from_double(0.5))));

    dv_free(f);
    dv_free(x);
}

void test_deriv_tan(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_tan(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(0.5);
    qfloat c = qf_cos(X);
    qfloat expect = qf_div(qf_from_double(1.0), qf_mul(c, c));

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{tan(x)} | x=0.5",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

void test_deriv_sinh(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_sinh(x);
    const dval_t *df = dv_get_deriv(f);

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{sinh(x)} | x=0.5",
               dv_eval(df),
               qf_cosh(qf_from_double(0.5)));

    dv_free(f);
    dv_free(x);
}

void test_deriv_cosh(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_cosh(x);
    const dval_t *df = dv_get_deriv(f);

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{cosh(x)} | x=0.5",
               dv_eval(df),
               qf_sinh(qf_from_double(0.5)));

    dv_free(f);
    dv_free(x);
}

void test_deriv_tanh(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_tanh(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(0.5);
    qfloat t = qf_tanh(X);
    qfloat expect = qf_sub(qf_from_double(1.0), qf_mul(t, t));

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{tanh(x)} | x=0.5",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

void test_deriv_asin(void)
{
    dval_t *x = dv_new_var_d(0.25);
    dval_t *f = dv_asin(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(0.25);
    qfloat expect =
        qf_div(qf_from_double(1.0),
               qf_sqrt(qf_sub(qf_from_double(1.0),
                              qf_mul(X, X))));

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{asin(x)} | x=0.25",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

void test_deriv_acos(void)
{
    dval_t *x = dv_new_var_d(0.25);
    dval_t *f = dv_acos(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(0.25);
    qfloat expect =
        qf_neg(qf_div(qf_from_double(1.0),
                      qf_sqrt(qf_sub(qf_from_double(1.0),
                                     qf_mul(X, X)))));

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{acos(x)} | x=0.25",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

void test_deriv_atan(void)
{
    dval_t *x = dv_new_var_d(0.25);
    dval_t *f = dv_atan(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(0.25);
    qfloat expect =
        qf_div(qf_from_double(1.0),
               qf_add(qf_from_double(1.0),
                      qf_mul(X, X)));

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{atan(x)} | x=0.25",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

void test_deriv_atan2(void)
{
    dval_t *x = dv_new_var_d(0.25);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *f = dv_atan2(x, one);
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(0.25);
    qfloat expect =
        qf_div(qf_from_double(1.0),
               qf_add(qf_from_double(1.0),
                      qf_mul(X, X)));

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{atan2(x,1)} | x=0.25",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(one);
    dv_free(x);
}

void test_deriv_asinh(void)
{
    dval_t *x = dv_new_var_d(0.25);
    dval_t *f = dv_asinh(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(0.25);
    qfloat expect =
        qf_div(qf_from_double(1.0),
               qf_sqrt(qf_add(qf_from_double(1.0),
                              qf_mul(X, X))));

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{asinh(x)} | x=0.25",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

void test_deriv_acosh(void)
{
    dval_t *x = dv_new_var_d(1.25);
    dval_t *f = dv_acosh(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(1.25);
    qfloat expect =
        qf_div(qf_from_double(1.0),
               qf_mul(qf_sqrt(qf_sub(X, qf_from_double(1.0))),
                      qf_sqrt(qf_add(X, qf_from_double(1.0)))));

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{acosh(x)} | x=1.25",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

void test_deriv_atanh(void)
{
    dval_t *x = dv_new_var_d(0.25);
    dval_t *f = dv_atanh(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(0.25);
    qfloat expect =
        qf_div(qf_from_double(1.0),
               qf_sub(qf_from_double(1.0),
                      qf_mul(X, X)));

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{atanh(x)} | x=0.25",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

void test_deriv_exp(void)
{
    dval_t *x = dv_new_var_d(1.5);
    dval_t *f = dv_exp(x);
    const dval_t *df = dv_get_deriv(f);

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{exp(x)} | x=1.5",
               dv_eval(df),
               qf_exp(qf_from_double(1.5)));

    dv_free(f);
    dv_free(x);
}

void test_deriv_log(void)
{
    dval_t *x = dv_new_var_d(2.0);
    dval_t *f = dv_log(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat expect =
        qf_div(qf_from_double(1.0),
               qf_from_double(2.0));

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{log(x)} | x=2",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

void test_deriv_sqrt(void)
{
    dval_t *x = dv_new_var_d(4.0);
    dval_t *f = dv_sqrt(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat expect =
        qf_div(qf_from_double(1.0),
               qf_mul(qf_from_double(2.0),
                      qf_sqrt(qf_from_double(4.0))));

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{sqrt(x)} | x=4",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

void test_deriv_composite(void)
{
    dval_t *x  = dv_new_var_d(1.0);
    dval_t *f  = dv_mul(dv_sin(x), dv_exp(x));
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(1.0);
    qfloat expect =
        qf_add(qf_mul(qf_cos(X), qf_exp(X)),
               qf_mul(qf_sin(X), qf_exp(X)));

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{sin(x)*exp(x)} | x=1",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

void test_deriv_sin_log(void)
{
    dval_t *x  = dv_new_var_d(1.3);
    dval_t *f  = dv_mul(dv_sin(x), dv_log(x));
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(1.3);
    qfloat expect =
        qf_add(qf_mul(qf_cos(X), qf_log(X)),
               qf_mul(qf_sin(X),
                      qf_div(qf_from_double(1.0), X)));

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{sin(x)*log(x)} | x=1.3",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

void test_deriv_exp_tanh(void)
{
    dval_t *x  = dv_new_var_d(0.7);
    dval_t *f  = dv_mul(dv_exp(x), dv_tanh(x));
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(0.7);
    qfloat t = qf_tanh(X);

    qfloat expect =
        qf_add(qf_mul(qf_exp(X), t),
               qf_mul(qf_exp(X),
                      qf_sub(qf_from_double(1.0),
                             qf_mul(t, t))));

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{exp(x)*tanh(x)} | x=0.7",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

void test_deriv_sqrt_sin_x2(void)
{
    dval_t *x  = dv_new_var_d(1.1);
    dval_t *x2 = dv_mul(x, x);
    dval_t *f  = dv_mul(dv_sqrt(x), dv_sin(x2));
    const dval_t *df = dv_get_deriv(f);

    qfloat X  = qf_from_double(1.1);
    qfloat X2 = qf_mul(X, X);

    qfloat term1 =
        qf_mul(qf_div(qf_from_double(1.0),
                      qf_mul(qf_from_double(2.0),
                             qf_sqrt(X))),
               qf_sin(X2));

    qfloat term2 =
        qf_mul(qf_sqrt(X),
               qf_mul(qf_cos(X2),
                      qf_mul(qf_from_double(2.0), X)));

    qfloat expect = qf_add(term1, term2);

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{sqrt(x)*sin(x^2)} | x=1.1",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(x2);
    dv_free(x);
}

void test_deriv_log_cosh(void)
{
    dval_t *x  = dv_new_var_d(0.9);
    dval_t *f  = dv_log(dv_cosh(x));
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(0.9);
    qfloat expect = qf_tanh(X);

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{log(cosh(x))} | x=0.9",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

void test_deriv_x2_exp_negx(void)
{
    dval_t *x   = dv_new_var_d(1.7);
    dval_t *xm  = dv_neg(x);
    dval_t *ex  = dv_exp(xm);
    dval_t *x2  = dv_mul(x, x);
    dval_t *f   = dv_mul(x2, ex);
    const dval_t *df = dv_get_deriv(f);

    qfloat X    = qf_from_double(1.7);
    qfloat e_mx = qf_exp(qf_neg(X));

    qfloat expect =
        qf_mul(e_mx,
               qf_add(qf_mul(qf_from_double(2.0), X),
                      qf_mul(qf_from_double(-1.0),
                             qf_mul(X, X))));

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{x^2*exp(-x)} | x=1.7",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(x2);
    dv_free(ex);
    dv_free(xm);
    dv_free(x);
}

void test_deriv_atan_x_over_sqrt(void)
{
    dval_t *x   = dv_new_var_d(0.8);

    dval_t *x2  = dv_mul(x, x);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *den = dv_sqrt(dv_add(one, x2));
    dval_t *g   = dv_div(one, den);

    dval_t *u   = dv_mul(x, g);
    dval_t *f   = dv_atan(u);
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(0.8);

    qfloat expect =
        qf_div(qf_from_double(1.0),
               qf_mul(qf_sqrt(qf_add(qf_from_double(1.0),
                                     qf_mul(X, X))),
                      qf_add(qf_from_double(1.0),
                             qf_mul(qf_from_double(2.0),
                                    qf_mul(X, X)))));

    check_q_at(__FILE__, __LINE__, 1,
               "d/dx{atan(x/sqrt(1+x^2))} | x=0.8",
               dv_eval(df), expect);

    dv_free(f);
    dv_free(u);
    dv_free(g);
    dv_free(den);
    dv_free(one);
    dv_free(x2);
    dv_free(x);
}

/* ------------------------------------------------------------------------- */
/* Second derivative tests                                                    */
/* ------------------------------------------------------------------------- */

void test_second_deriv_var(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *df  = dv_create_deriv(x);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{x} | x=2",
               dv_eval(ddf), expect);

    dv_free(x);
    dv_free(df);
}

void test_second_deriv_neg(void)
{
    dval_t *x   = dv_new_var_d(3.0);
    dval_t *f   = dv_neg(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{-x} | x=3",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_add_d(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *f   = dv_add_d(x, 5.0);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{x+5} | x=2",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_mul_d(void)
{
    dval_t *x   = dv_new_var_d(4.0);
    dval_t *f   = dv_mul_d(x, 7.0);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{7x} | x=4",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_div_d(void)
{
    dval_t *x   = dv_new_var_d(9.0);
    dval_t *f   = dv_div_d(x, 3.0);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{x/3} | x=9",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_x2(void)
{
    dval_t *x   = dv_new_var_d(3.0);
    dval_t *f   = dv_mul(x, x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_from_double(2.0);

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{x^2} | x=3",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_x3(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *f   = dv_pow_d(x, 3.0);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_from_double(12.0);

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{x^3} | x=2",
               dv_eval(ddf), expect);

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
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X  = qf_from_double(2.0);
    qfloat X2 = qf_mul(X, X);

    qfloat lnX  = qf_log(X);
    qfloat lnX2 = qf_mul(lnX, lnX);

    qfloat g    = qf_add(X2, qf_from_double(1.0));
    qfloat fx   = qf_pow(X, g);

    qfloat term1 = X2;
    qfloat term2 = qf_mul(qf_from_double(4.0),
                          qf_mul(X2, lnX2));
    qfloat term3 = qf_mul(qf_add(qf_mul(qf_from_double(4.0), X2),
                                 qf_from_double(6.0)),
                          lnX);
    qfloat term4 = qf_from_double(5.0);

    qfloat poly = qf_add(qf_add(term1, term2),
                         qf_add(term3, term4));

    qfloat expect = qf_mul(fx, poly);

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{x^(x^2+1)} | x=2",
               dv_eval(ddf), expect);

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
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_neg(qf_sin(qf_from_double(0.5)));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{sin(x)} | x=0.5",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_cos(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_cos(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_neg(qf_cos(qf_from_double(0.5)));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{cos(x)} | x=0.5",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_tan(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_tan(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.5);
    qfloat sec2 = qf_div(qf_from_double(1.0),
                         qf_mul(qf_cos(X), qf_cos(X)));
    qfloat expect =
        qf_mul(qf_from_double(2.0),
               qf_mul(sec2, qf_tan(X)));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{tan(x)} | x=0.5",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_sinh(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_sinh(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_sinh(qf_from_double(0.5));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{sinh(x)} | x=0.5",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_cosh(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_cosh(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_cosh(qf_from_double(0.5));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{cosh(x)} | x=0.5",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_tanh(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_tanh(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.5);
    qfloat t = qf_tanh(X);

    qfloat expect =
        qf_mul(qf_from_double(-2.0),
               qf_mul(t,
                      qf_sub(qf_from_double(1.0),
                             qf_mul(t, t))));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{tanh(x)} | x=0.5",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_asin(void)
{
    dval_t *x   = dv_new_var_d(0.25);
    dval_t *f   = dv_asin(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.25);
    qfloat denom =
        qf_sqrt(qf_sub(qf_from_double(1.0),
                       qf_mul(X, X)));

    qfloat expect =
        qf_mul(qf_from_double(-1.0),
               qf_div(X,
                      qf_mul(denom, denom)));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{asin(x)} | x=0.25",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_acos(void)
{
    dval_t *x   = dv_new_var_d(0.25);
    dval_t *f   = dv_acos(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.25);
    qfloat denom =
        qf_sqrt(qf_sub(qf_from_double(1.0),
                       qf_mul(X, X)));

    qfloat expect =
        qf_div(X,
               qf_mul(denom, denom));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{acos(x)} | x=0.25",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_atan(void)
{
    dval_t *x   = dv_new_var_d(0.25);
    dval_t *f   = dv_atan(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.25);
    qfloat denom =
        qf_add(qf_from_double(1.0),
               qf_mul(X, X));

    qfloat expect =
        qf_mul(qf_from_double(-2.0),
               qf_mul(X,
                      qf_div(qf_from_double(1.0),
                             qf_mul(denom, denom))));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{atan(x)} | x=0.25",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_atan2(void)
{
    dval_t *x   = dv_new_var_d(0.25);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *f   = dv_atan2(x, one);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.25);

    qfloat denom =
        qf_add(qf_from_double(1.0),
               qf_mul(X, X));

    qfloat expect =
        qf_mul(qf_from_double(-2.0),
               qf_mul(X,
                      qf_div(qf_from_double(1.0),
                             qf_mul(denom, denom))));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{atan2(x,1)} | x=0.25",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(one);
    dv_free(x);
}

void test_second_deriv_asinh(void)
{
    dval_t *x   = dv_new_var_d(0.25);
    dval_t *f   = dv_asinh(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.25);

    qfloat denom =
        qf_sqrt(qf_add(qf_from_double(1.0),
                       qf_mul(X, X)));

    qfloat expect =
        qf_mul(qf_from_double(-1.0),
               qf_mul(X,
                      qf_div(qf_from_double(1.0),
                             qf_mul(denom, denom))));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{asinh(x)} | x=0.25",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_acosh(void)
{
    dval_t *x   = dv_new_var_d(1.25);
    dval_t *f   = dv_acosh(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(1.25);

    qfloat denom1 =
        qf_sqrt(qf_sub(X, qf_from_double(1.0)));
    qfloat denom2 =
        qf_sqrt(qf_add(X, qf_from_double(1.0)));

    qfloat denom = qf_mul(denom1, denom2);

    qfloat expect =
        qf_mul(qf_from_double(-1.0),
               qf_mul(X,
                      qf_div(qf_from_double(1.0),
                             qf_mul(denom, denom))));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{acosh(x)} | x=1.25",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_atanh(void)
{
    dval_t *x   = dv_new_var_d(0.25);
    dval_t *f   = dv_atanh(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.25);

    qfloat denom =
        qf_sub(qf_from_double(1.0),
               qf_mul(X, X));

    qfloat expect =
        qf_mul(qf_from_double(2.0),
               qf_mul(X,
                      qf_div(qf_from_double(1.0),
                             qf_mul(denom, denom))));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{atanh(x)} | x=0.25",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_exp(void)
{
    dval_t *x   = dv_new_var_d(1.5);
    dval_t *f   = dv_exp(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(1.5);
    qfloat expect = qf_exp(X);

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{exp(x)} | x=1.5",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_log(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *f   = dv_log(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect =
        qf_mul(qf_from_double(-1.0),
               qf_div(qf_from_double(1.0),
                      qf_mul(qf_from_double(2.0),
                             qf_from_double(2.0))));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{log(x)} | x=2",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_sqrt(void)
{
    dval_t *x   = dv_new_var_d(4.0);
    dval_t *f   = dv_sqrt(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(4.0);

    qfloat expect =
        qf_mul(qf_from_double(-1.0),
               qf_div(qf_from_double(1.0),
                      qf_mul(qf_from_double(4.0),
                             qf_mul(qf_sqrt(X), qf_sqrt(X)))));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{sqrt(x)} | x=4",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_composite(void)
{
    dval_t *x   = dv_new_var_d(1.0);
    dval_t *f   = dv_mul(dv_sin(x), dv_exp(x));
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(1.0);

    qfloat expect =
        qf_add(qf_mul(qf_neg(qf_sin(X)), qf_exp(X)),
               qf_add(qf_mul(qf_cos(X), qf_exp(X)),
                      qf_mul(qf_sin(X), qf_exp(X))));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{sin(x)*exp(x)} | x=1",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_sin_log(void)
{
    dval_t *x   = dv_new_var_d(1.3);
    dval_t *f   = dv_mul(dv_sin(x), dv_log(x));
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(1.3);

    qfloat expect =
        qf_add(
            qf_mul(qf_neg(qf_sin(X)), qf_log(X)),
            qf_add(
                qf_mul(qf_cos(X),
                       qf_div(qf_from_double(1.0), X)),
                qf_mul(qf_sin(X),
                       qf_mul(qf_from_double(-1.0),
                              qf_div(qf_from_double(1.0),
                                     qf_mul(X, X)))))
        );

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{sin(x)*log(x)} | x=1.3",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_exp_tanh(void)
{
    dval_t *x   = dv_new_var_d(0.7);
    dval_t *f   = dv_mul(dv_exp(x), dv_tanh(x));
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.7);
    qfloat t = qf_tanh(X);

    qfloat expect =
        qf_add(
            qf_mul(qf_exp(X),
                   qf_sub(qf_from_double(1.0),
                          qf_mul(t, t))),
            qf_add(
                qf_mul(qf_exp(X), t),
                qf_mul(qf_exp(X),
                       qf_mul(qf_from_double(-2.0),
                              qf_mul(t,
                                     qf_sub(qf_from_double(1.0),
                                            qf_mul(t, t)))))));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{exp(x)*tanh(x)} | x=0.7",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_sqrt_sin_x2(void)
{
    dval_t *x   = dv_new_var_d(1.1);
    dval_t *x2  = dv_mul(x, x);
    dval_t *f   = dv_mul(dv_sqrt(x), dv_sin(x2));
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X  = qf_from_double(1.1);
    qfloat X2 = qf_mul(X, X);

    qfloat term1 =
        qf_mul(qf_mul(qf_from_double(-1.0),
                      qf_div(qf_from_double(1.0),
                             qf_mul(qf_from_double(4.0),
                                    qf_mul(qf_sqrt(X), qf_sqrt(X))))),
               qf_sin(X2));

    qfloat term2 =
        qf_mul(qf_div(qf_from_double(1.0),
                      qf_mul(qf_from_double(2.0), qf_sqrt(X))),
               qf_mul(qf_cos(X2),
                      qf_mul(qf_from_double(2.0), X)));

    qfloat term3 =
        qf_mul(qf_sqrt(X),
               qf_mul(qf_neg(qf_sin(X2)),
                      qf_mul(qf_from_double(2.0),
                             qf_mul(X, X))));

    qfloat expect = qf_add(qf_add(term1, term2), term3);

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{sqrt(x)*sin(x^2)} | x=1.1",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x2);
    dv_free(x);
}

void test_second_deriv_log_cosh(void)
{
    dval_t *x   = dv_new_var_d(0.9);
    dval_t *f   = dv_log(dv_cosh(x));
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.9);

    qfloat expect =
        qf_mul(qf_tanh(X),
               qf_tanh(X));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{log(cosh(x))} | x=0.9",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

void test_second_deriv_x2_exp_negx(void)
{
    dval_t *x   = dv_new_var_d(1.7);
    dval_t *xm  = dv_neg(x);
    dval_t *ex  = dv_exp(xm);
    dval_t *x2  = dv_mul(x, x);
    dval_t *f   = dv_mul(x2, ex);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X    = qf_from_double(1.7);
    qfloat e_mx = qf_exp(qf_neg(X));

    qfloat expect =
        qf_mul(e_mx,
               qf_add(qf_mul(qf_from_double(2.0), X),
                      qf_mul(qf_from_double(-1.0),
                             qf_mul(X, X))));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{x^2*exp(-x)} | x=1.7",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x2);
    dv_free(ex);
    dv_free(xm);
    dv_free(x);
}

void test_second_deriv_atan_x_over_sqrt(void)
{
    dval_t *x   = dv_new_var_d(0.8);

    dval_t *x2  = dv_mul(x, x);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *den = dv_sqrt(dv_add(one, x2));
    dval_t *g   = dv_div(one, den);

    dval_t *u   = dv_mul(x, g);
    dval_t *f   = dv_atan(u);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.8);

    qfloat expect =
        qf_div(qf_from_double(1.0),
               qf_mul(qf_sqrt(qf_add(qf_from_double(1.0),
                                     qf_mul(X, X))),
                      qf_add(qf_from_double(1.0),
                             qf_mul(qf_from_double(2.0),
                                    qf_mul(X, X)))));

    check_q_at(__FILE__, __LINE__, 1,
               "d²/dx²{atan(x/sqrt(1+x^2))} | x=0.8",
               dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(u);
    dv_free(g);
    dv_free(den);
    dv_free(one);
    dv_free(x2);
    dv_free(x);
}

/* ------------------------------------------------------------------------- */
/* Test driver                                                                */
/* ------------------------------------------------------------------------- */

int tests_main(void)
{
    /* ---------------- Arithmetic ---------------- */
    TEST_GROUP("Arithmetic");
    RUN_TEST(test_add,        NULL);
    RUN_TEST(test_sub,        NULL);
    RUN_TEST(test_mul,        NULL);
    RUN_TEST(test_div,        NULL);
    RUN_TEST(test_mixed,      NULL);

    /* ---------------- _d variants ---------------- */
    TEST_GROUP("_d variants");
    RUN_TEST(test_add_d,      NULL);
    RUN_TEST(test_mul_d,      NULL);
    RUN_TEST(test_div_d,      NULL);

    /* ---------------- Math functions ------------- */
    TEST_GROUP("Math functions");
    RUN_TEST(test_sin,        NULL);
    RUN_TEST(test_cos,        NULL);
    RUN_TEST(test_tan,        NULL);
    RUN_TEST(test_sinh,       NULL);
    RUN_TEST(test_cosh,       NULL);
    RUN_TEST(test_tanh,       NULL);
    RUN_TEST(test_asin,       NULL);
    RUN_TEST(test_acos,       NULL);
    RUN_TEST(test_atan,       NULL);
    RUN_TEST(test_atan2,      NULL);
    RUN_TEST(test_asinh,      NULL);
    RUN_TEST(test_acosh,      NULL);
    RUN_TEST(test_atanh,      NULL);
    RUN_TEST(test_exp,        NULL);
    RUN_TEST(test_log,        NULL);
    RUN_TEST(test_sqrt,       NULL);
    RUN_TEST(test_pow_d,      NULL);
    RUN_TEST(test_pow,        NULL);

    /* ---------------- First derivatives ---------- */
    TEST_GROUP("First derivatives");
    RUN_TEST(test_deriv_const,        NULL);
    RUN_TEST(test_deriv_var,          NULL);
    RUN_TEST(test_deriv_neg,          NULL);
    RUN_TEST(test_deriv_add_d,        NULL);
    RUN_TEST(test_deriv_mul_d,        NULL);
    RUN_TEST(test_deriv_div_d,        NULL);
    RUN_TEST(test_deriv_x2,           NULL);
    RUN_TEST(test_deriv_pow3,         NULL);
    RUN_TEST(test_deriv_pow_xy,       NULL);
    RUN_TEST(test_deriv_sin,          NULL);
    RUN_TEST(test_deriv_cos,          NULL);
    RUN_TEST(test_deriv_tan,          NULL);
    RUN_TEST(test_deriv_sinh,         NULL);
    RUN_TEST(test_deriv_cosh,         NULL);
    RUN_TEST(test_deriv_tanh,         NULL);
    RUN_TEST(test_deriv_asin,         NULL);
    RUN_TEST(test_deriv_acos,         NULL);
    RUN_TEST(test_deriv_atan,         NULL);
    RUN_TEST(test_deriv_atan2,        NULL);
    RUN_TEST(test_deriv_asinh,        NULL);
    RUN_TEST(test_deriv_acosh,        NULL);
    RUN_TEST(test_deriv_atanh,        NULL);
    RUN_TEST(test_deriv_exp,          NULL);
    RUN_TEST(test_deriv_log,          NULL);
    RUN_TEST(test_deriv_sqrt,         NULL);
    RUN_TEST(test_deriv_composite,    NULL);
    RUN_TEST(test_deriv_sin_log,      NULL);
    RUN_TEST(test_deriv_exp_tanh,     NULL);
    RUN_TEST(test_deriv_sqrt_sin_x2,  NULL);
    RUN_TEST(test_deriv_log_cosh,     NULL);
    RUN_TEST(test_deriv_x2_exp_negx,  NULL);
    RUN_TEST(test_deriv_atan_x_over_sqrt, NULL);

    /* ---------------- Second derivatives --------- */
    TEST_GROUP("Second derivatives");
    RUN_TEST(test_second_deriv_var,            NULL);
    RUN_TEST(test_second_deriv_neg,            NULL);
    RUN_TEST(test_second_deriv_add_d,          NULL);
    RUN_TEST(test_second_deriv_mul_d,          NULL);
    RUN_TEST(test_second_deriv_div_d,          NULL);
    RUN_TEST(test_second_deriv_x2,             NULL);
    RUN_TEST(test_second_deriv_x3,             NULL);
    RUN_TEST(test_second_deriv_pow_xy,         NULL);
    RUN_TEST(test_second_deriv_sin,            NULL);
    RUN_TEST(test_second_deriv_cos,            NULL);
    RUN_TEST(test_second_deriv_tan,            NULL);
    RUN_TEST(test_second_deriv_sinh,           NULL);
    RUN_TEST(test_second_deriv_cosh,           NULL);
    RUN_TEST(test_second_deriv_tanh,           NULL);
    RUN_TEST(test_second_deriv_asin,           NULL);
    RUN_TEST(test_second_deriv_acos,           NULL);
    RUN_TEST(test_second_deriv_atan,           NULL);
    RUN_TEST(test_second_deriv_atan2,          NULL);
    RUN_TEST(test_second_deriv_asinh,          NULL);
    RUN_TEST(test_second_deriv_acosh,          NULL);
    RUN_TEST(test_second_deriv_atanh,          NULL);
    RUN_TEST(test_second_deriv_exp,            NULL);
    RUN_TEST(test_second_deriv_log,            NULL);
    RUN_TEST(test_second_deriv_sqrt,           NULL);
    RUN_TEST(test_second_deriv_composite,      NULL);
    RUN_TEST(test_second_deriv_sin_log,        NULL);
    RUN_TEST(test_second_deriv_exp_tanh,       NULL);
    RUN_TEST(test_second_deriv_sqrt_sin_x2,    NULL);
    RUN_TEST(test_second_deriv_log_cosh,       NULL);
    RUN_TEST(test_second_deriv_x2_exp_negx,    NULL);
    RUN_TEST(test_second_deriv_atan_x_over_sqrt, NULL);

    return 0;
}
