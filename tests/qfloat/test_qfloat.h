#ifndef TESTS_QFLOAT_QFLOAT_TEST_H
#define TESTS_QFLOAT_QFLOAT_TEST_H

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "qfloat.h"
#include "test_harness.h"

void print_q(const char *label, qfloat_t x);
int approx_equal(qfloat_t a, double b, double tol);
int qf_close(qfloat_t a, qfloat_t b, double rel);
int qf_close_rel(qfloat_t a, qfloat_t b, double rel);

void test_arithmetic(void);
void test_arithmetic_extensions(void);
void test_strings(void);
void test_printf(void);
void test_vsprintf(void);
void test_power(void);
void test_trigonometric(void);
void test_hyperbolic(void);
void test_hypotenus(void);
void test_gamma_erf_erfc_erfinv_erfcinv_digamma(void);
void test_lambert_w(void);
void test_beta_logbeta_binomial_beta_pdf_logbeta_pdf_normal_pdf_cdf_logpdf(void);
void test_gammainc_ei_e1(void);
void test_readme_examples(void);
void test_qf_productlog_all(void);
void test_qf_trigamma(void);
void test_qf_tetragamma(void);
void test_difficult_qfloat_cases(void);

#endif
