#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "dval.h"
#include "qfloat.h"

/* ANSI colours */
#define GRN "\x1b[32m"
#define RED "\x1b[31m"
#define CYN "\x1b[36m"
#define RST "\x1b[0m"
#define BLD "\x1b[1m"

/* ------------------------------------------------------------------------- */
/* Compact qfloat comparison                                                 */
/* ------------------------------------------------------------------------- */

static void check_q_at(const char *file, int line, int col,
                       const char *label, qfloat got, qfloat expect)
{
    qfloat diff = qf_sub(got, expect);
    double abs_err = fabs(qf_to_double(diff));
    double exp_d   = fabs(qf_to_double(expect));

    /* Relative error when expect != 0 */
    double rel_err = (exp_d > 0)
        ? abs_err / exp_d
        : abs_err;

    /* Tolerances for quad-double */
    const double ABS_TOL = 2e-30;
    const double REL_TOL = 2e-30;

    if (abs_err < ABS_TOL || rel_err < REL_TOL) {
        printf("%sPASS %s %-32s  got=", GRN, RST, label);
        qf_printf("%.34q", got);
        printf("\n");
        return;
    }

    printf("%sFAIL%s   %s: %s:%d:%d: got=", RED, RST, label, file, line, col);

    qf_printf("%.34q", got);

    printf(" expect=");
    qf_printf("%.34q", expect);

    printf(" diff=");
    qf_printf("%.34q", diff);

    printf("\n");
}

/* ------------------------------------------------------------------------- */
/* Arithmetic tests                                                          */
/* ------------------------------------------------------------------------- */

static void test_add(void)
{
    dval_t *c2 = dv_new_const_d(2.0);
    dval_t *c3 = dv_new_const_d(3.0);
    dval_t *f = dv_add(c2, c3);
    check_q_at(__FILE__, __LINE__, 1, "2+3", dv_eval(f), qf_from_double(5));
    dv_free(c2);
    dv_free(c3);
    dv_free(f);
}

static void test_sub(void)
{
    dval_t *c10 = dv_new_const_d(10);
    dval_t *c4 = dv_new_const_d(4);
    dval_t *f = dv_sub(c10, c4);
    check_q_at(__FILE__, __LINE__, 1, "10-4", dv_eval(f), qf_from_double(6));
    dv_free(f);
    dv_free(c4);
    dv_free(c10);
}

static void test_mul(void)
{
    dval_t *c7 = dv_new_const_d(7);
    dval_t *c6 = dv_new_const_d(6);
    dval_t *f = dv_mul(c7, c6);
    check_q_at(__FILE__, __LINE__, 1, "7*6", dv_eval(f), qf_from_double(42));
    dv_free(f);
    dv_free(c6);
    dv_free(c7);
}

static void test_div(void)
{
    dval_t *c7 = dv_new_const_d(7);
    dval_t *c22 = dv_new_const_d(22);
    dval_t *f = dv_div(c22, c7);
    check_q_at(__FILE__, __LINE__, 1, "22/7", dv_eval(f), qf_div(qf_from_double(22), qf_from_double(7)));
    dv_free(f);
    dv_free(c22);
    dv_free(c7);
}

static void test_mixed(void)
{
    dval_t *two = dv_new_const_d(2);
    dval_t *three = dv_new_const_d(3);
    dval_t *ten = dv_new_const_d(10);
    dval_t *four = dv_new_const_d(4);
    dval_t *add_2_3 = dv_add(two, three);
    dval_t *sub_10_4 = dv_sub(ten, four);
    dval_t *mul_add_2_3_sub_10_4 = dv_mul(add_2_3, sub_10_4);
    dval_t *f = dv_div(mul_add_2_3_sub_10_4, two);

    check_q_at(__FILE__, __LINE__, 1, "mixed", dv_eval(f), qf_from_double(15));

    dv_free(f);
    dv_free(mul_add_2_3_sub_10_4);
    dv_free(sub_10_4);
    dv_free(add_2_3);
    dv_free(two);
    dv_free(three);
    dv_free(ten);
    dv_free(four);
}

/* ------------------------------------------------------------------------- */
/* _d variants                                                               */
/* ------------------------------------------------------------------------- */

static void test_add_d(void)
{
    dval_t *ten = dv_new_const_d(10);
    dval_t *f = dv_add_d(ten, 2.5);
    check_q_at(__FILE__, __LINE__, 1, "10+2.5", dv_eval(f), qf_from_double(12.5));
    dv_free(f);
    dv_free(ten);
}

static void test_mul_d(void)
{
    dval_t *three = dv_new_const_d(3);
    dval_t *f = dv_mul_d(three, 4);
    check_q_at(__FILE__, __LINE__, 1, "3*4", dv_eval(f), qf_from_double(12));
    dv_free(three);
    dv_free(f);
}

static void test_div_d(void)
{
    dval_t *nine = dv_new_const_d(9);
    dval_t *f = dv_div_d(nine, 3);
    check_q_at(__FILE__, __LINE__, 1, "9/3", dv_eval(f), qf_from_double(3));
    dv_free(f);
    dv_free(nine);
}

/* ------------------------------------------------------------------------- */
/* Mathematical function tests                                               */
/* ------------------------------------------------------------------------- */

static void test_sin(void)
{
    dval_t *point5 = dv_new_const_d(0.5);
    dval_t *f = dv_sin(point5);
    check_q_at(__FILE__, __LINE__, 1, "sin(0.5)", dv_eval(f), qf_sin(qf_from_double(0.5)));
    dv_free(f);
    dv_free(point5);
}
static void test_cos(void)
{
    dval_t *c = dv_new_const_d(0.5);
    dval_t *f = dv_cos(c);
    check_q_at(__FILE__, __LINE__, 1, "cos(0.5)", dv_eval(f), qf_cos(qf_from_double(0.5)));
    dv_free(f);
    dv_free(c);
}

static void test_tan(void)
{
    dval_t *c = dv_new_const_d(0.5);
    dval_t *f = dv_tan(c);
    check_q_at(__FILE__, __LINE__, 1, "tan(0.5)", dv_eval(f), qf_tan(qf_from_double(0.5)));
    dv_free(f);
    dv_free(c);
}

static void test_sinh(void)
{
    dval_t *c = dv_new_const_d(0.5);
    dval_t *f = dv_sinh(c);
    check_q_at(__FILE__, __LINE__, 1, "sinh(0.5)", dv_eval(f), qf_sinh(qf_from_double(0.5)));
    dv_free(f);
    dv_free(c);
}

static void test_cosh(void)
{
    dval_t *c = dv_new_const_d(0.5);
    dval_t *f = dv_cosh(c);
    check_q_at(__FILE__, __LINE__, 1, "cosh(0.5)", dv_eval(f), qf_cosh(qf_from_double(0.5)));
    dv_free(f);
    dv_free(c);
}

static void test_tanh(void)
{
    dval_t *c = dv_new_const_d(0.5);
    dval_t *f = dv_tanh(c);
    check_q_at(__FILE__, __LINE__, 1, "tanh(0.5)", dv_eval(f), qf_tanh(qf_from_double(0.5)));
    dv_free(f);
    dv_free(c);
}

static void test_asin(void)
{
    dval_t *c = dv_new_const_d(0.25);
    dval_t *f = dv_asin(c);
    check_q_at(__FILE__, __LINE__, 1, "asin(0.25)", dv_eval(f), qf_asin(qf_from_double(0.25)));
    dv_free(f);
    dv_free(c);
}

static void test_acos(void)
{
    dval_t *c = dv_new_const_d(0.25);
    dval_t *f = dv_acos(c);
    check_q_at(__FILE__, __LINE__, 1, "acos(0.25)", dv_eval(f), qf_acos(qf_from_double(0.25)));
    dv_free(f);
    dv_free(c);
}

static void test_atan(void)
{
    dval_t *c = dv_new_const_d(0.25);
    dval_t *f = dv_atan(c);
    check_q_at(__FILE__, __LINE__, 1, "atan(0.25)", dv_eval(f), qf_atan(qf_from_double(0.25)));
    dv_free(f);
    dv_free(c);
}

static void test_atan2(void)
{
    dval_t *base = dv_new_const_d(2.0);
    dval_t *expo = dv_new_const_d(3.0);
    dval_t *f    = dv_atan2(base, expo);
    check_q_at(__FILE__, __LINE__, 1, "atan2(2,3)", dv_eval(f),
            qf_atan2(qf_from_double(2.0), qf_from_double(3.0)));
    dv_free(f);
    dv_free(expo);
    dv_free(base);
}

static void test_asinh(void)
{
    dval_t *c = dv_new_const_d(0.25);
    dval_t *f = dv_asinh(c);
    check_q_at(__FILE__, __LINE__, 1, "asinh(0.25)", dv_eval(f), qf_asinh(qf_from_double(0.25)));
    dv_free(f);
    dv_free(c);
}

static void test_acosh(void)
{
    dval_t *c = dv_new_const_d(1.25);
    dval_t *f = dv_acosh(c);
    check_q_at(__FILE__, __LINE__, 1, "acosh(1.25)", dv_eval(f), qf_acosh(qf_from_double(1.25)));
    dv_free(f);
    dv_free(c);
}

static void test_atanh(void)
{
    dval_t *c = dv_new_const_d(0.25);
    dval_t *f = dv_atanh(c);
    check_q_at(__FILE__, __LINE__, 1, "atanh(0.25)", dv_eval(f), qf_atanh(qf_from_double(0.25)));
    dv_free(f);
    dv_free(c);
}

static void test_exp(void)
{
    dval_t *c = dv_new_const_d(1.5);
    dval_t *f = dv_exp(c);
    check_q_at(__FILE__, __LINE__, 1, "exp(1.5)", dv_eval(f), qf_exp(qf_from_double(1.5)));
    dv_free(f);
    dv_free(c);
}

static void test_log(void)
{
    dval_t *c = dv_new_const_d(1.5);
    dval_t *f = dv_log(c);
    check_q_at(__FILE__, __LINE__, 1, "log(1.5)", dv_eval(f), qf_log(qf_from_double(1.5)));
    dv_free(f);
    dv_free(c);
}

static void test_sqrt(void)
{
    dval_t *c = dv_new_const_d(2.0);
    dval_t *f = dv_sqrt(c);
    check_q_at(__FILE__, __LINE__, 1, "sqrt(2)", dv_eval(f), qf_sqrt(qf_from_double(2.0)));
    dv_free(f);
    dv_free(c);
}

static void test_pow_d(void)
{
    dval_t *base = dv_new_const_d(2.0);
    dval_t *f    = dv_pow_d(base, 3.0);
    check_q_at(__FILE__, __LINE__, 1, "2^3(d)", dv_eval(f),
            qf_pow(qf_from_double(2.0), qf_from_double(3.0)));
    dv_free(f);
    dv_free(base);
}

static void test_pow(void)
{
    dval_t *base = dv_new_const_d(2.0);
    dval_t *expo = dv_new_const_d(3.0);
    dval_t *f    = dv_pow(base, expo);
    check_q_at(__FILE__, __LINE__, 1, "2^3", dv_eval(f),
            qf_pow(qf_from_double(2.0), qf_from_double(3.0)));
    dv_free(f);
    dv_free(expo);
    dv_free(base);
}

/* ------------------------------------------------------------------------- */
/* First derivative tests                                                    */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* First derivative tests (compact, colourised, qfloat-accurate)             */
/* ------------------------------------------------------------------------- */

static void test_deriv_const(void)
{
    dval_t *c  = dv_new_const_d(5.0);
    dval_t *f  = c;                 /* f is just the same node */
    dval_t *df = dv_create_deriv(f);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{5}", dv_eval(dv_get_deriv(df)), qf_from_double(0.0));

    dv_free(df);
    dv_free(c);
}

static void test_deriv_var(void)
{
    dval_t *x = dv_new_var_d(2.0);
    const dval_t *dx = dv_get_deriv(x);
    check_q_at(__FILE__, __LINE__, 1, "d/dx{x} | x=2", dv_eval(dx), qf_from_double(1.0));

    dv_free(x);
}

static void test_deriv_neg(void)
{
    dval_t *x = dv_new_var_d(3.0);
    dval_t *f = dv_neg(x);
    const dval_t *df = dv_get_deriv(f);
    check_q_at(__FILE__, __LINE__, 1, "d/dx{-x} | x=3", dv_eval(df), qf_from_double(-1.0));

    dv_free(f);
    dv_free(x);
}

static void test_deriv_add_d(void)
{
    dval_t *x = dv_new_var_d(2.0);
    dval_t *f = dv_add_d(x, 5.0);
    const dval_t *df = dv_get_deriv(f);
    check_q_at(__FILE__, __LINE__, 1, "d/dx{x+5} | x=2", dv_eval(df), qf_from_double(1.0));

    dv_free(x);
    dv_free(f);
}

static void test_deriv_mul_d(void)
{
    dval_t *x = dv_new_var_d(4.0);
    dval_t *f = dv_mul_d(x, 7.0);
    const dval_t *df = dv_get_deriv(f);
    check_q_at(__FILE__, __LINE__, 1, "d/dx{7x} | x=4", dv_eval(df), qf_from_double(7.0));

    dv_free(f);
    dv_free(x);
}

static void test_deriv_div_d(void)
{
    dval_t *x = dv_new_var_d(9.0);
    dval_t *f = dv_div_d(x, 3.0);
    const dval_t *df = dv_get_deriv(f);
    check_q_at(__FILE__, __LINE__, 1, "d/dx{x/3} | x=9", dv_eval(df),
            qf_div(qf_from_double(1.0), qf_from_double(3.0)));

    dv_free(f);
    dv_free(x);
}

static void test_deriv_x2(void)
{
    dval_t *x = dv_new_var_d(3.0);
    dval_t *f = dv_mul(x, x);
    const dval_t *df = dv_get_deriv(f);
    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^2} | x=3", dv_eval(df), qf_from_double(6.0));

    dv_free(f);   // f owns x twice
    dv_free(x);   // f owns x twice
}

static void test_deriv_pow3(void)
{
    dval_t *x = dv_new_var_d(2.0);
    dval_t *f = dv_pow_d(x, 3.0);
    const dval_t *df = dv_get_deriv(f);
    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^3} | x=2", dv_eval(df), qf_from_double(12.0));

    dv_free(f);
    dv_free(x);
}

static void test_deriv_pow_xy(void)
{
    /* x = 2 */
    dval_t *x = dv_new_var_d(2.0);

    /* y = x^2 + 1 */
    dval_t *x2 = dv_mul(x, x);              /* x^2 */
    dval_t *one = dv_new_const_d(1.0);
    dval_t *y = dv_add(x2, one);            /* x^2 + 1 */

    /* f(x) = x^(x^2 + 1) */
    dval_t *f = dv_pow(x, y);
    const dval_t *df = dv_get_deriv(f);

    /* Expected derivative at x = 2:
       f'(2) = 32 * (4*ln(2) + 5/2)
    */
    qfloat X = qf_from_double(2.0);
    qfloat yval = qf_add(qf_mul(X, X), qf_from_double(1.0));   /* 5 */
    qfloat fval = qf_pow(X, yval);                             /* 32 */

    qfloat term1 = qf_mul(qf_from_double(4.0), qf_log(X));     /* 4 ln 2 */
    qfloat term2 = qf_div(yval, qf_from_double(2.0));          /* 5/2 */
    qfloat expect = qf_mul(fval, qf_add(term1, term2));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^(x^2+1)} | x=2", dv_eval(df), expect);

    dv_free(f);
    dv_free(y);
    dv_free(one);
    dv_free(x2);
    dv_free(x);
}

static void test_deriv_sin(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_sin(x);
    const dval_t *df = dv_get_deriv(f);
    check_q_at(__FILE__, __LINE__, 1, "d/dx{sin(x)} | x=0.5", dv_eval(df), qf_cos(qf_from_double(0.5)));

    dv_free(f);
    dv_free(x);
}

static void test_deriv_cos(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_cos(x);
    const dval_t *df = dv_get_deriv(f);
    check_q_at(__FILE__, __LINE__, 1, "d/dx{cos(x)} | x=0.5", dv_eval(df),
            qf_neg(qf_sin(qf_from_double(0.5))));

    dv_free(f);
    dv_free(x);
}

static void test_deriv_tan(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_tan(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat c = qf_cos(qf_from_double(0.5));
    qfloat expect = qf_div(qf_from_double(1.0), qf_mul(c, c));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{tan(x)} | x=0.5", dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

static void test_deriv_sinh(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_sinh(x);
    const dval_t *df = dv_get_deriv(f);
    check_q_at(__FILE__, __LINE__, 1, "d/dx{sinh(x)} | x=0.5", dv_eval(df), qf_cosh(qf_from_double(0.5)));

    dv_free(f);
    dv_free(x);
}

static void test_deriv_cosh(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_cosh(x);
    const dval_t *df = dv_get_deriv(f);
    check_q_at(__FILE__, __LINE__, 1, "d/dx{cosh(x)} | x=0.5", dv_eval(df), qf_sinh(qf_from_double(0.5)));

    dv_free(f);
    dv_free(x);
}

static void test_deriv_tanh(void)
{
    dval_t *x = dv_new_var_d(0.5);
    dval_t *f = dv_tanh(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat t = qf_tanh(qf_from_double(0.5));
    qfloat expect = qf_sub(qf_from_double(1.0), qf_mul(t, t));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{tanh(x)} | x=0.5", dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

static void test_deriv_asin(void)
{
    dval_t *x = dv_new_var_d(0.25);
    dval_t *f = dv_asin(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(0.25);
    qfloat expect = qf_div(qf_from_double(1.0),
                           qf_sqrt(qf_sub(qf_from_double(1.0), qf_mul(X, X))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{asin(x)} | x=0.25", dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

static void test_deriv_acos(void)
{
    dval_t *x = dv_new_var_d(0.25);
    dval_t *f = dv_acos(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(0.25);
    qfloat expect = qf_neg(qf_div(qf_from_double(1.0),
                                  qf_sqrt(qf_sub(qf_from_double(1.0), qf_mul(X, X)))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{acos(x)} | x=0.25", dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

static void test_deriv_atan(void)
{
    dval_t *x = dv_new_var_d(0.25);
    dval_t *f = dv_atan(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(0.25);
    qfloat expect = qf_div(qf_from_double(1.0),
                           qf_add(qf_from_double(1.0), qf_mul(X, X)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{atan(x)} | x=0.25", dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

static void test_deriv_atan2(void)
{
    /* One variable: x = 0.25 */
    dval_t *x = dv_new_var_d(0.25);

    /* f(x) = atan2(x, 1) */
    dval_t *one = dv_new_const_d(1.0);
    dval_t *f   = dv_atan2(x, one);

    const dval_t *df = dv_get_deriv(f);

    /* Expected derivative: 1 / (1 + x^2) */
    qfloat X = qf_from_double(0.25);
    qfloat expect = qf_div(qf_from_double(1.0),
                           qf_add(qf_from_double(1.0),
                                  qf_mul(X, X)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{atan2(x,1)} | x=0.25", dv_eval(df), expect);

    dv_free(f);
    dv_free(one);
    dv_free(x);
}

static void test_deriv_asinh(void)
{
    dval_t *x = dv_new_var_d(0.25);
    dval_t *f = dv_asinh(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(0.25);
    qfloat expect = qf_div(qf_from_double(1.0),
                           qf_sqrt(qf_add(qf_from_double(1.0), qf_mul(X, X))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{asinh(x)} | x=0.25", dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

static void test_deriv_acosh(void)
{
    dval_t *x = dv_new_var_d(1.25);
    dval_t *f = dv_acosh(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(1.25);
    qfloat expect = qf_div(qf_from_double(1.0),
                           qf_mul(qf_sqrt(qf_sub(X, qf_from_double(1.0))),
                                  qf_sqrt(qf_add(X, qf_from_double(1.0)))) );

    check_q_at(__FILE__, __LINE__, 1, "d/dx{acosh(x)} | x=1.25", dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

static void test_deriv_atanh(void)
{
    dval_t *x = dv_new_var_d(0.25);
    dval_t *f = dv_atanh(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(0.25);
    qfloat expect = qf_div(qf_from_double(1.0),
                           qf_sub(qf_from_double(1.0), qf_mul(X, X)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{atanh(x)} | x=0.25", dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

static void test_deriv_exp(void)
{
    dval_t *x = dv_new_var_d(1.5);
    dval_t *f = dv_exp(x);
    const dval_t *df = dv_get_deriv(f);
    check_q_at(__FILE__, __LINE__, 1, "d/dx{exp(x)} | x=1.5", dv_eval(df), qf_exp(qf_from_double(1.5)));

    dv_free(f);
    dv_free(x);
}

static void test_deriv_log(void)
{
    dval_t *x = dv_new_var_d(2.0);
    dval_t *f = dv_log(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat expect = qf_div(qf_from_double(1.0), qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "d/dx{log(x)} | x=2", dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

static void test_deriv_sqrt(void)
{
    dval_t *x = dv_new_var_d(4.0);
    dval_t *f = dv_sqrt(x);
    const dval_t *df = dv_get_deriv(f);

    qfloat expect = qf_div(qf_from_double(1.0),
                           qf_mul(qf_from_double(2.0), qf_sqrt(qf_from_double(4.0))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sqrt(x)} | x=4", dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

static void test_deriv_composite(void)
{
    dval_t *x = dv_new_var_d(1.0);
    dval_t *f = dv_mul(dv_sin(x), dv_exp(x));
    const dval_t *df = dv_get_deriv(f);

    qfloat X = qf_from_double(1.0);
    qfloat expect = qf_add(qf_mul(qf_cos(X), qf_exp(X)),
                           qf_mul(qf_sin(X), qf_exp(X)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sin(x)*exp(x)} | x=1", dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

static void test_deriv_sin_log(void)
{
    dval_t *x  = dv_new_var_d(1.3);
    dval_t *f  = dv_mul(dv_sin(x), dv_log(x));
    const dval_t *df = dv_get_deriv(f);

    qfloat X   = qf_from_double(1.3);
    qfloat expect = qf_add(qf_mul(qf_cos(X), qf_log(X)),
                           qf_mul(qf_sin(X),
                                  qf_div(qf_from_double(1.0), X)));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sin(x)*log(x)} | x=1.3", dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

static void test_deriv_exp_tanh(void)
{
    dval_t *x  = dv_new_var_d(0.7);
    dval_t *f  = dv_mul(dv_exp(x), dv_tanh(x));
    const dval_t *df = dv_get_deriv(f);

    qfloat X   = qf_from_double(0.7);
    qfloat t   = qf_tanh(X);
    qfloat expect = qf_add(qf_mul(qf_exp(X), t),
                           qf_mul(qf_exp(X),
                                  qf_sub(qf_from_double(1.0), qf_mul(t, t))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{exp(x)*tanh(x)} | x=0.7", dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

static void test_deriv_sqrt_sin_x2(void)
{
    dval_t *x  = dv_new_var_d(1.1);
    dval_t *x2 = dv_mul(x, x);
    dval_t *f  = dv_mul(dv_sqrt(x), dv_sin(x2));
    const dval_t *df = dv_get_deriv(f);

    qfloat X   = qf_from_double(1.1);
    qfloat X2  = qf_mul(X, X);
    qfloat term1 = qf_mul(qf_div(qf_from_double(1.0),
                                 qf_mul(qf_from_double(2.0), qf_sqrt(X))),
                          qf_sin(X2));
    qfloat term2 = qf_mul(qf_sqrt(X),
                          qf_mul(qf_cos(X2),
                                 qf_mul(qf_from_double(2.0), X)));
    qfloat expect = qf_add(term1, term2);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{sqrt(x)*sin(x^2)} | x=1.1", dv_eval(df), expect);

    dv_free(f);
    dv_free(x2);   // x2 was created here
    dv_free(x);    // x was not owned by f
}

static void test_deriv_log_cosh(void)
{
    dval_t *x  = dv_new_var_d(0.9);
    dval_t *f  = dv_log(dv_cosh(x));
    const dval_t *df = dv_get_deriv(f);

    qfloat X   = qf_from_double(0.9);
    qfloat expect = qf_tanh(X);

    check_q_at(__FILE__, __LINE__, 1, "d/dx{log(cosh(x))} | x=0.9", dv_eval(df), expect);

    dv_free(f);
    dv_free(x);
}

static void test_deriv_x2_exp_negx(void)
{
    dval_t *x   = dv_new_var_d(1.7);
    dval_t *xm  = dv_neg(x);
    dval_t *ex  = dv_exp(xm);
    dval_t *x2  = dv_mul(x, x);
    dval_t *f   = dv_mul(x2, ex);
    const dval_t *df  = dv_get_deriv(f);

    qfloat X    = qf_from_double(1.7);
    qfloat e_mx = qf_exp(qf_neg(X));
    qfloat expect = qf_mul(e_mx,
                        qf_add(qf_mul(qf_from_double(2.0), X),
                                qf_mul(qf_from_double(-1.0),
                                        qf_mul(X, X))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{x^2*exp(-x)} | x=1.7", dv_eval(df), expect);

    dv_free(f);
    dv_free(x2);
    dv_free(ex);
    dv_free(xm);
    dv_free(x);
}

static void test_deriv_atan_x_over_sqrt(void)
{
    dval_t *x   = dv_new_var_d(0.8);

    /* g(x) = 1/sqrt(1+x^2) */
    dval_t *x2  = dv_mul(x, x);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *den = dv_sqrt(dv_add(one, x2));
    dval_t *g   = dv_div(one, den);

    /* u(x) = x * g(x) */
    dval_t *u   = dv_mul(x, g);

    /* f(x) = atan(u(x)) */
    dval_t *f   = dv_atan(u);
    const dval_t *df  = dv_get_deriv(f);

    /* expected derivative */
    qfloat X = qf_from_double(0.8);
    qfloat expect = qf_div(qf_from_double(1.0),
                           qf_mul(qf_sqrt(qf_add(qf_from_double(1.0),
                                                 qf_mul(X, X))),
                                  qf_add(qf_from_double(1.0),
                                         qf_mul(qf_from_double(2.0),
                                                qf_mul(X, X)))));

    check_q_at(__FILE__, __LINE__, 1, "d/dx{atan(x/sqrt(1+x^2))} | x=0.8", dv_eval(df), expect);

    dv_free(f);
    dv_free(u);
    dv_free(g);
    dv_free(den);
    dv_free(one);
    dv_free(x2);
    dv_free(x);
}

/* ------------------------------------------------------------------------- */
/* Second derivative tests                                                   */
/* ------------------------------------------------------------------------- */

static void test_second_deriv_var(void)
{
    dval_t *x   = dv_new_var_d(2.0);

    dval_t *df  = dv_create_deriv(x);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x} | x=2", dv_eval(ddf), expect);

    dv_free(x);
    dv_free(df);
}

static void test_second_deriv_neg(void)
{
    dval_t *x   = dv_new_var_d(3.0);
    dval_t *f   = dv_neg(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{-x} | x=3", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_add_d(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *f   = dv_add_d(x, 5.0);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x+5} | x=2", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_mul_d(void)
{
    dval_t *x   = dv_new_var_d(4.0);
    dval_t *f   = dv_mul_d(x, 7.0);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{7x} | x=4", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_div_d(void)
{
    dval_t *x   = dv_new_var_d(9.0);
    dval_t *f   = dv_div_d(x, 3.0);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_from_double(0.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x/3} | x=9", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_x2(void)
{
    dval_t *x   = dv_new_var_d(3.0);
    dval_t *f   = dv_mul(x, x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_from_double(2.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x^2} | x=3", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_x3(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *f   = dv_pow_d(x, 3.0);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_from_double(12.0);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{x^3} | x=2", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_pow_xy(void)
{
    /* x = 2 */
    dval_t *x = dv_new_var_d(2.0);

    /* y = x^2 + 1 */
    dval_t *x2  = dv_mul(x, x);              /* x^2 */
    dval_t *one = dv_new_const_d(1.0);
    dval_t *y   = dv_add(x2, one);           /* x^2 + 1 */

    /* f(x) = x^(x^2 + 1) */
    dval_t *f   = dv_pow(x, y);

    /* First derivative */
    dval_t *df  = dv_create_deriv(f);

    /* Second derivative */
    const dval_t *ddf = dv_get_deriv(df);

    /* ------------------------------------------------------------
       Expected value from the exact closed-form:

       f''(x) = x^(x^2 + 1) *
                ( x^2
                + 4 x^2 ln^2(x)
                + (4 x^2 + 6) ln(x)
                + 5 )

       Evaluate at x = 2 using qfloat.
       ------------------------------------------------------------ */

    qfloat X  = qf_from_double(2.0);
    qfloat X2 = qf_mul(X, X);                 /* x^2 */

    qfloat lnX   = qf_log(X);
    qfloat lnX2  = qf_mul(lnX, lnX);          /* ln^2(x) */

    /* x^(x^2+1) */
    qfloat g     = qf_add(X2, qf_from_double(1.0));
    qfloat fx    = qf_pow(X, g);

    /* Build the polynomial factor */
    qfloat term1 = X2;                                        /* x^2 */
    qfloat term2 = qf_mul(qf_from_double(4.0),
                          qf_mul(X2, lnX2));                  /* 4 x^2 ln^2(x) */
    qfloat term3 = qf_mul(qf_add(qf_mul(qf_from_double(4.0), X2),
                                 qf_from_double(6.0)),
                          lnX);                               /* (4 x^2 + 6) ln(x) */
    qfloat term4 = qf_from_double(5.0);                       /* +5 */

    qfloat poly  = qf_add(qf_add(term1, term2),
                          qf_add(term3, term4));

    /* expect = x^(x^2+1) * poly */
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

static void test_second_deriv_sin(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_sin(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_neg(qf_sin(qf_from_double(0.5)));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sin(x)} | x=0.5", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_cos(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_cos(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_neg(qf_cos(qf_from_double(0.5)));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{cos(x)} | x=0.5", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_tan(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_tan(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.5);
    qfloat sec2 = qf_div(qf_from_double(1.0),
                         qf_mul(qf_cos(X), qf_cos(X)));
    qfloat expect = qf_mul(qf_from_double(2.0),
                           qf_mul(sec2, qf_tan(X)));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{tan(x)} | x=0.5", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_sinh(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_sinh(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_sinh(qf_from_double(0.5));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sinh(x)} | x=0.5", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_cosh(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_cosh(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_cosh(qf_from_double(0.5));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{cosh(x)} | x=0.5", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_tanh(void)
{
    dval_t *x   = dv_new_var_d(0.5);
    dval_t *f   = dv_tanh(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.5);
    qfloat sech2 = qf_div(qf_from_double(1.0),
                          qf_mul(qf_cosh(X), qf_cosh(X)));
    qfloat expect = qf_mul(qf_from_double(-2.0),
                           qf_mul(qf_tanh(X), sech2));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{tanh(x)} | x=0.5", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_asin(void)
{
    dval_t *x   = dv_new_var_d(0.25);
    dval_t *f   = dv_asin(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.25);
    qfloat denom = qf_pow(qf_sub(qf_from_double(1.0),
                                 qf_mul(X, X)),
                          qf_from_double(1.5));
    qfloat expect = qf_div(X, denom);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{asin(x)} | x=0.25", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_acos(void)
{
    dval_t *x   = dv_new_var_d(0.25);
    dval_t *f   = dv_acos(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.25);
    qfloat denom = qf_pow(qf_sub(qf_from_double(1.0),
                                 qf_mul(X, X)),
                          qf_from_double(1.5));
    qfloat expect = qf_neg(qf_div(X, denom));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{acos(x)} | x=0.25", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_atan(void)
{
    dval_t *x   = dv_new_var_d(0.25);
    dval_t *f   = dv_atan(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.25);
    qfloat denom = qf_mul(qf_add(qf_from_double(1.0),
                                 qf_mul(X, X)),
                          qf_add(qf_from_double(1.0),
                                 qf_mul(X, X)));
    qfloat expect = qf_div(qf_mul(qf_from_double(-2.0), X), denom);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{atan(x)} | x=0.25", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_atan2(void)
{
    /* One variable: x = 0.25 */
    dval_t *x   = dv_new_var_d(0.25);

    /* f(x) = atan2(x, 1) */
    dval_t *one = dv_new_const_d(1.0);
    dval_t *f   = dv_atan2(x, one);

    /* First derivative */
    dval_t *df  = dv_create_deriv(f);

    /* Second derivative */
    const dval_t *ddf = dv_get_deriv(df);

    /* Expected: -2x / (1 + x^2)^2 */
    qfloat X = qf_from_double(0.25);

    qfloat denom = qf_mul(
        qf_add(qf_from_double(1.0), qf_mul(X, X)),
        qf_add(qf_from_double(1.0), qf_mul(X, X))
    );

    qfloat expect = qf_div(
        qf_mul(qf_from_double(-2.0), X),
        denom
    );

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{atan2(x,1)} | x=0.25", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(one);
    dv_free(x);
}

static void test_second_deriv_asinh(void)
{
    dval_t *x   = dv_new_var_d(0.25);
    dval_t *f   = dv_asinh(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.25);
    qfloat denom = qf_pow(qf_add(qf_from_double(1.0),
                                 qf_mul(X, X)),
                          qf_from_double(1.5));
    qfloat expect = qf_neg(qf_div(X, denom));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{asinh(x)} | x=0.25", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_acosh(void)
{
    dval_t *x   = dv_new_var_d(1.25);
    dval_t *f   = dv_acosh(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(1.25);
    qfloat denom = qf_mul(qf_pow(qf_sub(X, qf_from_double(1.0)),
                                 qf_from_double(1.5)),
                          qf_pow(qf_add(X, qf_from_double(1.0)),
                                 qf_from_double(1.5)));
    qfloat expect = qf_neg(qf_div(X, denom));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{acosh(x)} | x=1.25", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_atanh(void)
{
    dval_t *x   = dv_new_var_d(0.25);
    dval_t *f   = dv_atanh(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.25);
    qfloat denom = qf_pow(qf_sub(qf_from_double(1.0),
                                 qf_mul(X, X)),
                          qf_from_double(2.0));
    qfloat expect = qf_mul(qf_from_double(2.0),
                           qf_div(X, denom));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{atanh(x)} | x=0.25", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_exp(void)
{
    dval_t *x   = dv_new_var_d(1.5);
    dval_t *f   = dv_exp(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat expect = qf_exp(qf_from_double(1.5));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{exp(x)} | x=1.5", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_log(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *f   = dv_log(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(2.0);
    qfloat denom = qf_mul(X, X);
    qfloat expect = qf_neg(qf_div(qf_from_double(1.0), denom));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{log(x)} | x=2", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_sqrt(void)
{
    dval_t *x   = dv_new_var_d(4.0);
    dval_t *f   = dv_sqrt(x);
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(4.0);
    qfloat denom = qf_mul(qf_from_double(4.0),
                          qf_pow(X, qf_from_double(1.5)));
    qfloat expect = qf_mul(qf_from_double(-1.0),
                           qf_div(qf_from_double(1.0), denom));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sqrt(x)} | x=4", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static dval_t *make_f_sin_exp(dval_t *x)
{
    dval_t *sinx = dv_sin(x);
    dval_t *ex   = dv_exp(x);
    dval_t *f = dv_mul(sinx, ex);

    dv_free(sinx);
    dv_free(ex);

    return f;
}

static void test_second_deriv_sin_exp(void)
{
    dval_t *x  = dv_new_var_d(1.0);
    dval_t *f  = make_f_sin_exp(x);
    dval_t *df = dv_create_deriv(f);   /* constructed f' */
    const dval_t *ddf = dv_get_deriv(df);    /* got f'' */

    qfloat X = qf_from_double(1.0);
    qfloat expect = qf_mul(qf_from_double(2.0),
                           qf_mul(qf_exp(X), qf_cos(X)));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sin(x)*exp(x)} | x=1", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_sin_log(void)
{
    dval_t *x   = dv_new_var_d(1.3);
    dval_t *f   = dv_mul(dv_sin(x), dv_log(x));
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(1.3);
    qfloat s = qf_sin(X);
    qfloat c = qf_cos(X);
    qfloat inv = qf_div(qf_from_double(1.0), X);
    qfloat term1 = qf_mul(qf_neg(s), qf_log(X));
    qfloat term2 = qf_mul(qf_from_double(2.0),
                          qf_mul(c, inv));
    qfloat term3 = qf_mul(s,
                          qf_mul(qf_from_double(-1.0),
                                 qf_div(qf_from_double(1.0),
                                        qf_mul(X, X))));
    qfloat expect = qf_add(term1, qf_add(term2, term3));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sin(x)*log(x)} | x=1.3", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static dval_t *make_f_exp_tanh(dval_t *x)
{
    dval_t *ex = dv_exp(x);
    dval_t *t  = dv_tanh(x);
    dval_t *f = dv_mul(ex, t);

    dv_free(ex);
    dv_free(t);

    return f;
}

static void test_second_deriv_exp_tanh(void)
{
    dval_t *x  = dv_new_var_d(0.7);
    dval_t *f  = make_f_exp_tanh(x);
    dval_t *df = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X  = qf_from_double(0.7);
    qfloat eX = qf_exp(X);
    qfloat tX = qf_tanh(X);
    qfloat s2 = qf_div(qf_from_double(1.0),
                       qf_mul(qf_cosh(X), qf_cosh(X)));

    qfloat expect = qf_mul(eX,
                           qf_add(tX,
                                  qf_add(qf_mul(qf_from_double(2.0), s2),
                                         qf_mul(qf_from_double(-2.0),
                                                qf_mul(s2, tX)))));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{exp(x)*tanh(x)} | x=0.7", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static dval_t *make_f_sqrt_sin_x2(dval_t *x)
{
    dval_t *x2    = dv_mul(x, x);
    dval_t *sqrtx = dv_sqrt(x);
    dval_t *sinx2 = dv_sin(x2);
    dval_t *f = dv_mul(sqrtx, sinx2);

    dv_free(x2);
    dv_free(sqrtx);
    dv_free(sinx2);

    return f;
}

static void test_second_deriv_sqrt_sin_x2(void)
{
    dval_t *x  = dv_new_var_d(1.1);
    dval_t *f  = make_f_sqrt_sin_x2(x);
    dval_t *df = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X     = qf_from_double(1.1);
    qfloat X2    = qf_mul(X, X);
    qfloat sqrtX = qf_sqrt(X);
    qfloat s     = qf_sin(X2);
    qfloat c     = qf_cos(X2);

    qfloat termA = qf_mul(qf_from_double(-1.0),
                          qf_div(s,
                                 qf_mul(qf_from_double(4.0),
                                        qf_pow(X, qf_from_double(1.5)))));
    qfloat termB = qf_mul(qf_from_double(-4.0),
                          qf_mul(qf_pow(X, qf_from_double(2.5)), s));
    qfloat termC = qf_mul(qf_from_double(4.0),
                          qf_mul(sqrtX, c));

    qfloat expect = qf_add(termA, qf_add(termB, termC));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{sqrt(x)*sin(x^2)} | x=1.1", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

static void test_second_deriv_log_cosh(void)
{
    dval_t *x   = dv_new_var_d(0.9);
    dval_t *f   = dv_log(dv_cosh(x));
    dval_t *df  = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X = qf_from_double(0.9);
    qfloat t = qf_tanh(X);
    qfloat expect = qf_mul(qf_from_double(1.0),
                           qf_sub(qf_from_double(1.0),
                                  qf_mul(t, t)));

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{log(cosh(x))} | x=0.9", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
    dv_free(x);
}

/* You said we can ignore x^2*exp(-x) for now, so no second-deriv test here. */

static dval_t *make_f_atan_x_over_sqrt(dval_t *x)
{
    dval_t *x2  = dv_mul(x, x);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *den = dv_sqrt(dv_add(one, x2));
    dval_t *g   = dv_div(one, den);
    dval_t *u   = dv_mul(x, g);
    dval_t *f   = dv_atan(u);

    dv_free(x2);
    dv_free(one);
    dv_free(den);
    dv_free(g);
    dv_free(u);

    return f;
}

static void test_second_deriv_atan_x_over_sqrt(void)
{
    dval_t *x  = dv_new_var_d(0.8);
    dval_t *f  = make_f_atan_x_over_sqrt(x);
    dval_t *df = dv_create_deriv(f);
    const dval_t *ddf = dv_get_deriv(df);

    qfloat X    = qf_from_double(0.8);
    qfloat oneq = qf_from_double(1.0);
    qfloat twoq = qf_from_double(2.0);

    qfloat a = qf_sqrt(qf_add(oneq, qf_mul(X, X)));
    qfloat b = qf_add(oneq, qf_mul(twoq, qf_mul(X, X)));

    qfloat term1 = qf_div(qf_mul(qf_from_double(-1.0), X),
                          qf_mul(qf_pow(a, qf_from_double(3.0)), b));
    qfloat term2 = qf_div(qf_mul(qf_from_double(-4.0), X),
                          qf_mul(a, qf_mul(b, b)));

    qfloat expect = qf_add(term1, term2);

    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{atan(x/sqrt(1+x^2))} | x=0.8", dv_eval(ddf), expect);

    dv_free(df);
    dv_free(f);
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
static void to_string_pass(const char *msg,
                           const char *got,
                           const char *expected)
{
    fprintf(stderr, "\033[1;32mPASS\033[0m %s\n", msg);

    int multi = is_multiline(got) || is_multiline(expected);

    print_multiline("got", got);

    if (multi)
        fprintf(stderr, "  ───────────────────────────────\n");

    print_multiline("expected", expected);
}

static void to_string_fail(const char *file, int line, int col,
                           const char *msg,
                           const char *got,
                           const char *expected)
{
    fprintf(stderr,
        "\033[1;31mFAIL\033[0m %s: %s:%d:%d\n",
        msg, file, line, col);

    int multi = is_multiline(got) || is_multiline(expected);

    print_multiline("got", got);

    if (multi)
        fprintf(stderr, "  ───────────────────────────────\n");

    print_multiline("expected", expected);
}

/* ------------------------------------------------------------------------- */
/* dv_to_string Tests                                                        */
/* ------------------------------------------------------------------------- */

/* ============================================================
 * BASIC CONST
 * ============================================================ */

static void test_to_string_basic_const_expr(void)
{
    dval_t *c = dv_new_const_d(3.5);
    char *s = dv_to_string(c, style_EXPRESSION);

    const char *expect = "{ c = 3.5 }";

    if (strcmp(s, expect) == 0)
        to_string_pass("basic const (EXPR)", s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic const (EXPR)", s, expect);

    free(s);
    dv_free(c);
}

static void test_to_string_basic_const_func(void)
{
    dval_t *c = dv_new_const_d(3.5);
    char *got = dv_to_string(c, style_FUNCTION);

    const char *expect =
        "c = 3.5\n"
        "return c\n";

    if (strcmp(got, expect) == 0)
        to_string_pass("basic const (FUNC)", got, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic const (FUNC)", got, expect);

    free(got);
    dv_free(c);
}

void test_to_string_basic_const(void)
{
    test_to_string_basic_const_expr();
    test_to_string_basic_const_func();
}


/* ============================================================
 * BASIC VAR
 * ============================================================ */

static void test_to_string_basic_var_expr(void)
{
    dval_t *x = dv_new_named_var_d(42.0, "x");
    char *s = dv_to_string(x, style_EXPRESSION);

    const char *expect = "{ x | x = 42 }";

    if (strcmp(s, expect) == 0)
        to_string_pass("basic var (EXPR)", s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic var (EXPR)", s, expect);

    free(s);
    dv_free(x);
}

static void test_to_string_basic_var_func(void)
{
    dval_t *x = dv_new_named_var_d(42.0, "x");
    char *s = dv_to_string(x, style_FUNCTION);

    const char *expect =
        "x = 42\n"
        "return x\n";

    if (strcmp(s, expect) == 0)
        to_string_pass("basic var (FUNC)", s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "basic var (FUNC)", s, expect);

    free(s);
    dv_free(x);
}

void test_to_string_basic_var(void)
{
    test_to_string_basic_var_expr();
    test_to_string_basic_var_func();
}


/* ============================================================
 * ADDITION
 * ============================================================ */

static void test_to_string_addition_expr(void)
{
    dval_t *x = dv_new_named_var_d(1, "x");
    dval_t *y = dv_new_named_var_d(2, "y");
    dval_t *f = dv_add(x, y);

    char *s = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ x + y | x = 1, y = 2 }";

    if (strcmp(s, expect) == 0)
        to_string_pass("addition (EXPR)", s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "addition (EXPR)", s, expect);

    free(s);
    dv_free(f);
}

static void test_to_string_addition_func(void)
{
    dval_t *x = dv_new_named_var_d(1, "x");
    dval_t *y = dv_new_named_var_d(2, "y");
    dval_t *f = dv_add(x, y);

    char *s = dv_to_string(f, style_FUNCTION);
    const char *expect =
        "x = 1\n"
        "y = 2\n"
        "expr(x,y) = x + y\n"
        "return expr(x,y)\n";

    if (strcmp(s, expect) == 0)
        to_string_pass("addition (FUNC)", s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "addition (FUNC)", s, expect);

    free(s);
    dv_free(f);
}

void test_to_string_addition(void)
{
    test_to_string_addition_expr();
    test_to_string_addition_func();
}


/* ============================================================
 * NESTED MUL + ADD
 * ============================================================ */

static void test_to_string_nested_mul_add_expr(void)
{
    dval_t *x = dv_new_named_var_d(2, "x");
    dval_t *y = dv_new_named_var_d(3, "y");
    dval_t *z = dv_new_named_var_d(4, "z");

    dval_t *f = dv_add(dv_mul(x, y), z);

    char *s = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ xy + z | x = 2, y = 3, z = 4 }";

    if (strcmp(s, expect) == 0)
        to_string_pass("nested mul+add (EXPR)", s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "nested mul+add (EXPR)", s, expect);

    free(s);
    dv_free(f);
}

static void test_to_string_nested_mul_add_func(void)
{
    dval_t *x = dv_new_named_var_d(2, "x");
    dval_t *y = dv_new_named_var_d(3, "y");
    dval_t *z = dv_new_named_var_d(4, "z");

    dval_t *f = dv_add(dv_mul(x, y), z);

    char *s = dv_to_string(f, style_FUNCTION);
    const char *expect =
        "x = 2\n"
        "y = 3\n"
        "z = 4\n"
        "expr(x,y,z) = x*y + z\n"
        "return expr(x,y,z)\n";

    if (strcmp(s, expect) == 0)
        to_string_pass("nested mul+add (FUNC)", s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "nested mul+add (FUNC)", s, expect);

    free(s);
    dv_free(f);
}

void test_to_string_nested_mul_add(void)
{
    test_to_string_nested_mul_add_expr();
    test_to_string_nested_mul_add_func();
}

static void test_to_string_atan2_expr(void)
{
    dval_t *x = dv_new_named_var_d(2, "x");
    dval_t *y = dv_new_named_var_d(3, "y");

    dval_t *f = dv_atan2(x, y);

    char *s = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ atan2(x, y) | x = 2, y = 3 }";

    if (strcmp(s, expect) == 0)
        to_string_pass("atan2 (EXPR)", s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "atan2 (EXPR)", s, expect);

    free(s);
    dv_free(f);
}

static void test_to_string_atan2_func(void)
{
    dval_t *x = dv_new_named_var_d(2, "x");
    dval_t *y = dv_new_named_var_d(3, "y");

    dval_t *f = dv_atan2(x, y);

    char *s = dv_to_string(f, style_FUNCTION);
    const char *expect =
        "x = 2\n"
        "y = 3\n"
        "expr(x,y) = atan2(x, y)\n"
        "return expr(x,y)\n";

    if (strcmp(s, expect) == 0)
        to_string_pass("atan2 (FUNC)", s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "atan2 (FUNC)", s, expect);

    free(s);
    dv_free(f);
}

void test_to_string_atan2(void)
{
    test_to_string_atan2_expr();
    test_to_string_atan2_func();
}

/* ============================================================
 * POW SUPERSCRIPT
 * ============================================================ */

static void test_to_string_pow_superscript_expr(void)
{
    dval_t *x = dv_new_named_var_d(2, "x");
    dval_t *f = dv_pow_d(x, 3);

    char *s = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ x³ | x = 2 }";

    if (strcmp(s, expect) == 0)
        to_string_pass("pow superscript (EXPR)", s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "pow superscript (EXPR)", s, expect);

    free(s);
    dv_free(f);
}

static void test_to_string_pow_superscript_func(void)
{
    dval_t *x = dv_new_named_var_d(2, "x");
    dval_t *f = dv_pow_d(x, 3);

    char *s = dv_to_string(f, style_FUNCTION);
    const char *expect =
        "x = 2\n"
        "expr(x) = x^3\n"
        "return expr(x)\n";

    if (strcmp(s, expect) == 0)
        to_string_pass("pow superscript (FUNC)", s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "pow superscript (FUNC)", s, expect);

    free(s);
    dv_free(f);
}

void test_to_string_pow_superscript(void)
{
    test_to_string_pow_superscript_expr();
    test_to_string_pow_superscript_func();
}


/* ============================================================
 * UNARY SIN
 * ============================================================ */

static void test_to_string_unary_sin_expr(void)
{
    dval_t *x = dv_new_named_var_d(0.5, "x");
    dval_t *f = dv_sin(x);

    char *s = dv_to_string(f, style_EXPRESSION);
    const char *expect = "{ sin x | x = 0.5 }";

    if (strcmp(s, expect) == 0)
        to_string_pass("unary sin (EXPR)", s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "unary sin (EXPR)", s, expect);

    free(s);
    dv_free(f);
}

static void test_to_string_unary_sin_func(void)
{
    dval_t *x = dv_new_named_var_d(0.5, "x");
    dval_t *f = dv_sin(x);

    char *s = dv_to_string(f, style_FUNCTION);
    const char *expect =
        "x = 0.5\n"
        "expr(x) = sin(x)\n"
        "return expr(x)\n";

    if (strcmp(s, expect) == 0)
        to_string_pass("unary sin (FUNC)", s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "unary sin (FUNC)", s, expect);

    free(s);
    dv_free(f);
}

void test_to_string_unary_sin(void)
{
    test_to_string_unary_sin_expr();
    test_to_string_unary_sin_func();
}


/* ============================================================
 * FUNCTION STYLE (identity)
 * ============================================================ */

static void test_to_string_function_style_expr(void)
{
    dval_t *x = dv_new_named_var_d(10, "x");
    char *s = dv_to_string(x, style_EXPRESSION);

    const char *expect = "{ x | x = 10 }";

    if (strcmp(s, expect) == 0)
        to_string_pass("function style identity (EXPR)", s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "function style identity (EXPR)", s, expect);

    free(s);
    dv_free(x);
}

static void test_to_string_function_style_func(void)
{
    dval_t *x = dv_new_named_var_d(10, "x");
    char *s = dv_to_string(x, style_FUNCTION);

    const char *expect =
        "x = 10\n"
        "return x\n";

    if (strcmp(s, expect) == 0)
        to_string_pass("function style identity (FUNC)", s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "function style identity (FUNC)", s, expect);

    free(s);
    dv_free(x);
}

void test_to_string_function_style(void)
{
    test_to_string_function_style_expr();
    test_to_string_function_style_func();
}


/* ============================================================
 * TEST SUITE RUNNER
 * ============================================================ */

void test_to_string_all(void)
{
    test_to_string_basic_const();
    test_to_string_basic_var();
    test_to_string_addition();
    test_to_string_nested_mul_add();
    test_to_string_atan2();
    test_to_string_pow_superscript();
    test_to_string_unary_sin();
    test_to_string_function_style();
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
            "{ πx² | x = 1.25 }",
            "x = 1.25\n"
            "π = 3.141592653589793\n"
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
            "{ x³y³ | x = 1.25 }",
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
            "{ 6x³y | x = 1.25 }",
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
            "{ x³y | x = 1.25 }",
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
            "{ 2xy | x = 1.25 }",
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
            "{ cos(x)·eˣ | x = 1.25 }",
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
            "{ exp(sin(x))·exp(cos(x)) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = exp(sin(x))*exp(cos(x))\n"
            "return expr(x)",
            __LINE__ 
        },

        /* 28 */
        {
            "exp(x^2)*exp(3*x^2)",
            make_expr_28,
            "{ exp(x²)·exp(3x²) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = exp(x^2)*exp(3*x^2)\n"
            "return expr(x)",
            __LINE__ 
        },

        /* 29 */
        {
            "exp(x)*exp(2*x)",
            make_expr_29,
            "{ exp(x)·exp(2x) | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = exp(x)*exp(2*x)\n"
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
            "{ π·sin(x) | x = 1.25 }",
            "x = 1.25\n"
            "π = 3.141592653589793\n"
            "expr(x,π) = π*sin(x)\n"
            "return expr(x,π)",
            __LINE__ 
        },

        /* 32 */
        {
            "τ*cos(x)",
            make_expr_32,
            "{ τ·cos(x) | x = 1.25 }",
            "x = 1.25\n"
            "τ = 6.283185307179586\n"
            "expr(x,τ) = τ*cos(x)\n"
            "return expr(x,τ)",
            __LINE__ 
        },

        /* 33 */
        {
            "e*x^2",
            make_expr_33,
            "{ e·x² | x = 1.25 }",
            "x = 1.25\n"
            "e = 2.718281828459045\n"
            "expr(x,e) = e*x^2\n"
            "return expr(x,e)",
            __LINE__ 
        },

        /* 34 */
        {
            "π*τ*e",
            make_expr_34,
            "{ π·τ·e | x = 1.25 }",
            "π = 3.141592653589793\n"
            "τ = 6.283185307179586\n"
            "e = 2.718281828459045\n"
            "expr(π,τ,e) = π*τ*e\n"
            "return expr(π,τ,e)",
            __LINE__ 
        },

        /* 35 */
        {
            "π*x*τ*y",
            make_expr_35,
            "{ π·τ·x·y | x = 1.25 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "π = 3.141592653589793\n"
            "τ = 6.283185307179586\n"
            "expr(x,y,π,τ) = π*x*τ*y\n"
            "return expr(x,y,π,τ)",
            __LINE__ 
        },

        /* 36 */
        {
            "exp(x)*π",
            make_expr_36,
            "{ π·eˣ | x = 1.25 }",
            "x = 1.25\n"
            "π = 3.141592653589793\n"
            "expr(x,π) = exp(x)*π\n"
            "return expr(x,π)",
            __LINE__ 
        },

        /* 37 */
        {
            "τ*exp(x^2)",
            make_expr_37,
            "{ τ·e^{x²} | x = 1.25 }",
            "x = 1.25\n"
            "τ = 6.283185307179586\n"
            "expr(x,τ) = τ*exp(x^2)\n"
            "return expr(x,τ)",
            __LINE__ 
        },

        /* 38 */
        {
            "e*sin(x)*cos(y)",
            make_expr_38,
            "{ e·sin(x)·cos(y) | x = 1.25 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "e = 2.718281828459045\n"
            "expr(x,y,e) = e*sin(x)*cos(y)\n"
            "return expr(x,y,e)",
            __LINE__ 
        },

        /* 39 */
        {
            "π*exp(τ*x)",
            make_expr_39,
            "{ π·e^{τx} | x = 1.25 }",
            "x = 1.25\n"
            "π = 3.141592653589793\n"
            "τ = 6.283185307179586\n"
            "expr(x,π,τ) = π*exp(τ*x)\n"
            "return expr(x,π,τ)",
            __LINE__ 
        },

        /* 40 */
        {
            "exp(π*x)*τ",
            make_expr_40,
            "{ τ·e^{πx} | x = 1.25 }",
            "x = 1.25\n"
            "π = 3.141592653589793\n"
            "τ = 6.283185307179586\n"
            "expr(x,π,τ) = exp(π*x)*τ\n"
            "return expr(x,π,τ)",
            __LINE__ 
        },

        /* 41 */
        {
            "sin(π*x)",
            make_expr_41,
            "{ sin(πx) | x = 1.25 }",
            "x = 1.25\n"
            "π = 3.141592653589793\n"
            "expr(x,π) = sin(π*x)\n"
            "return expr(x,π)",
            __LINE__
        },

        /* 42 */
        {
            "cos(τ*x)",
            make_expr_42,
            "{ cos(τx) | x = 1.25 }",
            "x = 1.25\n"
            "τ = 6.283185307179586\n"
            "expr(x,τ) = cos(τ*x)\n"
            "return expr(x,τ)",
            __LINE__
        },

        /* 43 */
        {
            "exp(π*τ*x)",
            make_expr_43,
            "{ e^{πτx} | x = 1.25 }",
            "x = 1.25\n"
            "π = 3.141592653589793\n"
            "τ = 6.283185307179586\n"
            "expr(x,π,τ) = exp(π*τ*x)\n"
            "return expr(x,π,τ)",
            __LINE__
        },

        /* 44 */
        {
            "sin(x)+cos(x)+exp(x)",
            make_expr_44,
            "{ sin(x)+cos(x)+eˣ | x = 1.25 }",
            "x = 1.25\n"
            "expr(x) = sin(x)+cos(x)+exp(x)\n"
            "return expr(x)",
            __LINE__
        },

        /* 45 */
        {
            "x + y + π + τ + e",
            make_expr_45,
            "{ x + y + π + τ + e | x = 1.25 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "π = 3.141592653589793\n"
            "τ = 6.283185307179586\n"
            "e = 2.718281828459045\n"
            "expr(x,y,π,τ,e) = x + y + π + τ + e\n"
            "return expr(x,y,π,τ,e)",
            __LINE__
        },

        /* 46 */
        {
            "x*y + π*x + τ*y + e",
            make_expr_46,
            "{ xy + πx + τy + e | x = 1.25 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "π = 3.141592653589793\n"
            "τ = 6.283185307179586\n"
            "e = 2.718281828459045\n"
            "expr(x,y,π,τ,e) = x*y + π*x + τ*y + e\n"
            "return expr(x,y,π,τ,e)",
            __LINE__
        },

        /* 47 */
        {
            "(x+π)*(y+τ)",
            make_expr_47,
            "{ (x+π)(y+τ) | x = 1.25 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "π = 3.141592653589793\n"
            "τ = 6.283185307179586\n"
            "expr(x,y,π,τ) = (x+π)*(y+τ)\n"
            "return expr(x,y,π,τ)",
            __LINE__
        },

        /* 48 */
        {
            "exp(x+π)*exp(y+τ)",
            make_expr_48,
            "{ e^{x+π}·e^{y+τ} | x = 1.25 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "π = 3.141592653589793\n"
            "τ = 6.283185307179586\n"
            "expr(x,y,π,τ) = exp(x+π)*exp(y+τ)\n"
            "return expr(x,y,π,τ)",
            __LINE__
        },

        /* 49 */
        {
            "sin(x+π)*cos(y+τ)",
            make_expr_49,
            "{ sin(x+π)·cos(y+τ) | x = 1.25 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "π = 3.141592653589793\n"
            "τ = 6.283185307179586\n"
            "expr(x,y,π,τ) = sin(x+π)*cos(y+τ)\n"
            "return expr(x,y,π,τ)",
            __LINE__
        },

        /* 50 */
        {
            "exp(sin(x+π) + cos(y+τ))",
            make_expr_50,
            "{ e^{sin(x+π)+cos(y+τ)} | x = 1.25 }",
            "x = 1.25\n"
            "y = 1.25\n"
            "π = 3.141592653589793\n"
            "τ = 6.283185307179586\n"
            "expr(x,y,π,τ) = exp(sin(x+π) + cos(y+τ))\n"
            "return expr(x,y,π,τ)",
            __LINE__
        },
    };

    /* ============================================================
     *  Test loop — formatted with bold PASS/FAIL and file:line
     * ============================================================ */
    const int N = (int)(sizeof(tests) / sizeof(tests[0]));

    printf(CYN "=== dval_t to_string - 50 Tests ===\n" RST);

    for (int i = 0; i < N; i++) {
        dval_t *f = tests[i].make();

        char *got_expr = dv_to_string(f, style_EXPRESSION);
        char *got_func = dv_to_string(f, style_FUNCTION);

        int ok_expr = strcmp(got_expr, tests[i].expected_expr) == 0;
        int ok_func = strcmp(got_func, tests[i].expected_func) == 0;

        /* ---------------- EXPR block ---------------- */
        if (ok_expr) {
            printf(BLD GRN "PASS" RST " %s (EXPR)\n", tests[i].src);
        } else {
            printf(BLD RED "FAIL" RST " %s (EXPR): tests/test_dval.c:%d:1\n",
                   tests[i].src, tests[i].line);
        }

        printf("  got      %s\n", got_expr);
        printf("  expected %s\n", tests[i].expected_expr);

        /* ---------------- FUNC block ---------------- */
        if (ok_func) {
            printf(BLD GRN "PASS" RST " %s (FUNC)\n", tests[i].src);
        } else {
            printf(BLD RED "FAIL" RST " %s (FUNC): tests/test_dval.c:%d:1\n",
                   tests[i].src, tests[i].line);
        }

        /* got block */
        {
            const char *p = got_func;
            const char *nl;
            printf("  got      ");
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
            printf("  expected ");
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
dval_t *make_f(dval_t *x) {
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

int test_readme_example(void) {
    /* Create a named variable x with initial value 1.25 */
    dval_t *x = dv_new_named_var_d(1.25, "x");

    /* Build expression:
         f(x) = exp(sin(x)) + 3*x^2 - 7
    */
    dval_t *f = make_f(x);

    /* First derivative (owning) */
    dval_t *df_dx = dv_create_deriv(f);   /* df/dx */

    /* Second derivative (borrowed) */
    const dval_t *d2f_dx = dv_get_deriv(df_dx);  /* d²f/dx² */

    /* Evaluate */
    qfloat f_val    = dv_eval(f);
    qfloat d1_val   = dv_eval(df_dx);
    qfloat d2_val   = dv_eval(d2f_dx);

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

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Entry point                                                               */
/* ------------------------------------------------------------------------- */

int main(void)
{
    printf(CYN "=== dval_t Arithmetic Tests (%%.34q precision) ===\n" RST);

    test_add();
    test_sub();
    test_mul();
    test_div();
    test_mixed();

    test_add_d();
    test_mul_d();
    test_div_d();

    printf(CYN "=== dval_t Maths functions Tests (%%.34q precision) ===\n" RST);

    test_sin();
    test_cos();
    test_tan();

    test_sinh();
    test_cosh();
    test_tanh();

    test_asin();
    test_acos();
    test_atan();
    test_atan2();

    test_asinh();
    test_acosh();
    test_atanh();

    test_exp();
    test_log();
    test_sqrt();
    test_pow_d();
    test_pow();

    printf(CYN "=== dval_t Derivatives Tests (%%.34q precision) ===\n" RST);

    test_deriv_const();
    test_deriv_var();
    test_deriv_x2();
    test_deriv_sin();
    test_deriv_exp();
    test_deriv_log();
    test_deriv_sqrt();
    test_deriv_pow3();

    test_deriv_neg();
    test_deriv_add_d();
    test_deriv_mul_d();
    test_deriv_div_d();

    test_deriv_tan();
    test_deriv_tanh();
    test_deriv_sinh();
    test_deriv_atanh();
    test_deriv_asin();
    test_deriv_asinh();
    test_deriv_acos();
    test_deriv_cosh();
    test_deriv_cos();
    test_deriv_acosh();
    test_deriv_atan();
    test_deriv_atan2();

    test_deriv_pow_xy();

    printf(CYN "=== dval_t Derivatives Tests on composite functions (%%.34q precision) ===\n" RST);

    test_deriv_composite();
    test_deriv_sin_log();
    test_deriv_exp_tanh();
    test_deriv_sqrt_sin_x2();
    test_deriv_log_cosh();
    test_deriv_x2_exp_negx();
    test_deriv_atan_x_over_sqrt();

    printf(CYN "=== dval_t Second Derivative Tests (%%.34q precision) ===\n" RST);

    /* Basic algebraic */
    test_second_deriv_var();
    test_second_deriv_neg();
    test_second_deriv_add_d();
    test_second_deriv_mul_d();
    test_second_deriv_div_d();
    test_second_deriv_x2();
    test_second_deriv_x3();
    test_second_deriv_pow_xy();

    /* Trigonometric */
    test_second_deriv_sin();
    test_second_deriv_cos();
    test_second_deriv_tan();

    /* Hyperbolic */
    test_second_deriv_sinh();
    test_second_deriv_cosh();
    test_second_deriv_tanh();

    /* Inverse trig */
    test_second_deriv_asin();
    test_second_deriv_acos();
    test_second_deriv_atan();
    test_second_deriv_atan2();

    /* Inverse hyperbolic */
    test_second_deriv_asinh();
    test_second_deriv_acosh();
    test_second_deriv_atanh();

    /* Exponential / log / sqrt */
    test_second_deriv_exp();
    test_second_deriv_log();
    test_second_deriv_sqrt();

    /* Composite functions */
    test_second_deriv_sin_exp();
    test_second_deriv_sin_log();
    test_second_deriv_exp_tanh();
    test_second_deriv_sqrt_sin_x2();
    test_second_deriv_log_cosh();

    /* Deep composite */
    test_second_deriv_atan_x_over_sqrt();

    printf(CYN "=== dval_t to_string Tests ===\n" RST);

    /* tests to_string() */
    test_to_string_all();
    test_expressions();

    printf(CYN "=== example in README.md ===\n" RST);

    test_readme_example();

    printf(CYN "=== Done ===\n" RST);

    return 0;
}
