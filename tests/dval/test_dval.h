#ifndef TESTS_DVAL_DVAL_TEST_H
#define TESTS_DVAL_DVAL_TEST_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dval.h"
#include "qfloat.h"
#include "test_harness.h"

static inline dval_t *test_dv_new_const_d(double x)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));
    dval_t *dv = dv_new_const(n);

    num_destroy(&n);
    return dv;
}

static inline dval_t *test_dv_new_const_qf(qfloat_t x)
{
    number_t n = num_create_from_qfloat(x);
    dval_t *dv = dv_new_const(n);

    num_destroy(&n);
    return dv;
}

static inline dval_t *test_dv_new_const_qc(qcomplex_t x)
{
    number_t n = num_create_from_qcomplex(x);
    dval_t *dv = dv_new_const(n);

    num_destroy(&n);
    return dv;
}

static inline dval_t *test_dv_new_named_const_d(double x, const char *name)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));
    dval_t *dv = dv_new_named_const(n, name);

    num_destroy(&n);
    return dv;
}

static inline dval_t *test_dv_new_named_const_qf(qfloat_t x, const char *name)
{
    number_t n = num_create_from_qfloat(x);
    dval_t *dv = dv_new_named_const(n, name);

    num_destroy(&n);
    return dv;
}

static inline dval_t *test_dv_new_named_const_qc(qcomplex_t x, const char *name)
{
    number_t n = num_create_from_qcomplex(x);
    dval_t *dv = dv_new_named_const(n, name);

    num_destroy(&n);
    return dv;
}

static inline dval_t *test_dv_new_var_d(double x)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));
    dval_t *dv = dv_new_var(n);

    num_destroy(&n);
    return dv;
}

static inline dval_t *test_dv_new_var_qf(qfloat_t x)
{
    number_t n = num_create_from_qfloat(x);
    dval_t *dv = dv_new_var(n);

    num_destroy(&n);
    return dv;
}

static inline dval_t *test_dv_new_var_qc(qcomplex_t x)
{
    number_t n = num_create_from_qcomplex(x);
    dval_t *dv = dv_new_var(n);

    num_destroy(&n);
    return dv;
}

static inline dval_t *test_dv_new_named_var_d(double x, const char *name)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));
    dval_t *dv = dv_new_named_var(n, name);

    num_destroy(&n);
    return dv;
}

static inline dval_t *test_dv_new_named_var_qf(qfloat_t x, const char *name)
{
    number_t n = num_create_from_qfloat(x);
    dval_t *dv = dv_new_named_var(n, name);

    num_destroy(&n);
    return dv;
}

static inline dval_t *test_dv_new_named_var_qc(qcomplex_t x, const char *name)
{
    number_t n = num_create_from_qcomplex(x);
    dval_t *dv = dv_new_named_var(n, name);

    num_destroy(&n);
    return dv;
}

static inline void test_dv_set_val_d(dval_t *dv, double x)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));

    dv_set_val(dv, n);
    num_destroy(&n);
}

static inline void test_dv_set_val_qf(dval_t *dv, qfloat_t x)
{
    number_t n = num_create_from_qfloat(x);

    dv_set_val(dv, n);
    num_destroy(&n);
}

static inline void test_dv_set_val_qc(dval_t *dv, qcomplex_t x)
{
    number_t n = num_create_from_qcomplex(x);

    dv_set_val(dv, n);
    num_destroy(&n);
}

static inline dval_t *test_dv_add_d(const dval_t *dv, double x)
{
    number_t n = num_create_from_double(x);
    dval_t *out = dv_add_num(dv, &n);

    num_destroy(&n);
    return out;
}

static inline dval_t *test_dv_sub_d(const dval_t *dv, double x)
{
    number_t n = num_create_from_double(x);
    dval_t *out = dv_sub_num(dv, &n);

    num_destroy(&n);
    return out;
}

static inline dval_t *test_dv_d_sub(double x, const dval_t *dv)
{
    number_t n = num_create_from_double(x);
    dval_t *out = dv_num_sub(&n, dv);

    num_destroy(&n);
    return out;
}

static inline dval_t *test_dv_mul_d(const dval_t *dv, double x)
{
    number_t n = num_create_from_double(x);
    dval_t *out = dv_mul_num(dv, &n);

    num_destroy(&n);
    return out;
}

static inline dval_t *test_dv_div_d(const dval_t *dv, double x)
{
    number_t n = num_create_from_double(x);
    dval_t *out = dv_div_num(dv, &n);

    num_destroy(&n);
    return out;
}

static inline dval_t *test_dv_d_div(double x, const dval_t *dv)
{
    number_t n = num_create_from_double(x);
    dval_t *out = dv_num_div(&n, dv);

    num_destroy(&n);
    return out;
}

static inline dval_t *test_dv_pow_d(const dval_t *dv, double x)
{
    number_t n = num_create_from_double(x);
    dval_t *out = dv_pow(dv, &n);

    num_destroy(&n);
    return out;
}

static inline dval_t *test_dv_pow_qc(const dval_t *dv, qcomplex_t x)
{
    number_t n = num_create_from_qcomplex(x);
    dval_t *out = dv_pow(dv, &n);

    num_destroy(&n);
    return out;
}

static inline double test_dv_eval_d(const dval_t *dv)
{
    number_t n = dv_eval(dv);
    double out = num_to_double(n);

    num_destroy(&n);
    return out;
}

static inline qfloat_t test_dv_eval_qf(const dval_t *dv)
{
    number_t n = dv_eval(dv);
    qfloat_t out = num_to_qfloat(n);

    num_destroy(&n);
    return out;
}

static inline qcomplex_t test_dv_eval_qc(const dval_t *dv)
{
    number_t n = dv_eval(dv);
    number_t re_n = num_real_part(n);
    number_t im_n = num_imag_part(n);
    qcomplex_t out = qc_make(num_to_qfloat(re_n), num_to_qfloat(im_n));

    num_destroy(&im_n);
    num_destroy(&re_n);
    num_destroy(&n);
    return out;
}

static inline qfloat_t test_dv_get_val_qf(const dval_t *dv)
{
    number_t n = dv_get_val(dv);
    qfloat_t out = num_to_qfloat(n);

    num_destroy(&n);
    return out;
}

static inline qcomplex_t test_dv_get_val_qc(const dval_t *dv)
{
    number_t n = dv_get_val(dv);
    number_t re_n = num_real_part(n);
    number_t im_n = num_imag_part(n);
    qcomplex_t out = qc_make(num_to_qfloat(re_n), num_to_qfloat(im_n));

    num_destroy(&im_n);
    num_destroy(&re_n);
    num_destroy(&n);
    return out;
}

int str_eq(const char *a, const char *b);
void to_string_pass(const char *msg, const char *got, const char *expected);
void to_string_fail(const char *file, int line, int col, const char *msg,
                    const char *got, const char *expected);

#define dv_eval_d      test_dv_eval_d
#define dv_eval_qf     test_dv_eval_qf
#define dv_eval_qc     test_dv_eval_qc
#define dv_get_val_qf  test_dv_get_val_qf
#define dv_get_val_qc  test_dv_get_val_qc
#define dv_add_d       test_dv_add_d
#define dv_sub_d       test_dv_sub_d
#define dv_d_sub       test_dv_d_sub
#define dv_mul_d       test_dv_mul_d
#define dv_div_d       test_dv_div_d
#define dv_d_div       test_dv_d_div
#define dv_pow_d       test_dv_pow_d
#define dv_pow_qc      test_dv_pow_qc

void check_q_at(const char *file, int line, int col,
                const char *label, qfloat_t got, qfloat_t expect);
void print_expr_of(const dval_t *f);
const test_validity_contract_t *dval_validity_contract_number_exact(void);
const test_validity_contract_t *dval_validity_contract_number_close(void);

#define TEST_ASSERT_DVAL_NUMBER_EQ(actual, expected) \
    do { \
        number_t test_dval_actual__ = (actual); \
        number_t test_dval_expected__ = (expected); \
        TEST_ASSERT_VALID_NAMED("dval-number-exact", \
                                &test_dval_actual__, \
                                &test_dval_expected__); \
    } while (0)

#define TEST_ASSERT_DVAL_NUMBER_CLOSE(actual, expected) \
    do { \
        number_t test_dval_actual__ = (actual); \
        number_t test_dval_expected__ = (expected); \
        TEST_ASSERT_VALID_NAMED("dval-number-close", \
                                &test_dval_actual__, \
                                &test_dval_expected__); \
    } while (0)

#define ASSERT_DVAL_NUMBER_EQ(actual, expected) \
    TEST_ASSERT_DVAL_NUMBER_EQ((actual), (expected))

#define ASSERT_DVAL_NUMBER_CLOSE(actual, expected) \
    TEST_ASSERT_DVAL_NUMBER_CLOSE((actual), (expected))

void test_arithmetic(void);
void test_d_variants(void);
void test_maths_functions(void);
void test_first_derivatives(void);
void test_second_derivatives(void);
void test_dval_t_to_string(void);
void test_dval_t_from_string(void);
void test_partial_derivatives(void);
void test_dval_pattern_helpers(void);
void test_runtime_regressions(void);
void test_reverse_mode(void);

void test_to_string_all(void);
void test_expressions(void);
void test_expressions_unnamed(void);
void test_expressions_longname(void);
void test_erf(void);
void test_erfc(void);
void test_erfinv(void);
void test_erfcinv(void);
void test_gamma(void);
void test_gammainv(void);
void test_lgamma(void);
void test_digamma(void);
void test_trigamma(void);
void test_lambert_w0(void);
void test_lambert_wm1(void);
void test_normal_pdf(void);
void test_normal_cdf(void);
void test_normal_logpdf(void);
void test_ei(void);
void test_e1(void);
void test_beta(void);
void test_logbeta(void);
void test_deriv_trigamma(void);
void test_second_deriv_digamma(void);

void check_roundtrip(const char *label, dval_t *f, int line);
void check_parse_val(const char *label, const char *s,
                     double expect_d, int line);
void check_parse_null(const char *label, const char *s, int line);
void check_parse_null_stderr_contains(const char *label,
                                      const char *s,
                                      const char *expected_substring,
                                      int line);

dval_t *make_expr_u01(void);
dval_t *make_expr_u02(void);
dval_t *make_expr_u03(void);
dval_t *make_expr_u04(void);
dval_t *make_expr_u05(void);
dval_t *make_expr_u06(void);
dval_t *make_expr_c01(void);
dval_t *make_expr_c02(void);
dval_t *make_expr_c03(void);
dval_t *make_expr_c04(void);
dval_t *make_expr_l01(void);
dval_t *make_expr_l02(void);
dval_t *make_expr_l03(void);
dval_t *make_expr_l04(void);
dval_t *make_expr_l05(void);
dval_t *make_expr_l06(void);
dval_t *make_expr_l07(void);
dval_t *make_expr_l08(void);
dval_t *make_expr_l09(void);

#endif
