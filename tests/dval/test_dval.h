#ifndef TESTS_DVAL_DVAL_TEST_H
#define TESTS_DVAL_DVAL_TEST_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dval.h"
#include "qfloat.h"
#include "test_harness.h"

void check_q_at(const char *file, int line, int col,
                const char *label, qfloat_t got, qfloat_t expect);
void print_expr_of(const dval_t *f);

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
void test_README_md_example(void);

void test_to_string_all(void);
void test_expressions(void);
void test_expressions_unnamed(void);
void test_expressions_longname(void);
void test_erf(void);
void test_erfc(void);
void test_erfinv(void);
void test_erfcinv(void);
void test_gamma(void);
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
void check_parse_val(const char *label, const char *s, double expect_d, int line);
void check_parse_null(const char *label, const char *s, int line);
void to_string_pass(const char *msg, const char *got, const char *expected);
void to_string_fail(const char *file, int line, int col, const char *msg,
                    const char *got, const char *expected);
int str_eq(const char *a, const char *b);

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
