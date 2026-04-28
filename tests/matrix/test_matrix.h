#ifndef TESTS_MATRIX_MATRIX_TEST_H
#define TESTS_MATRIX_MATRIX_TEST_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matrix.h"
#include "test_harness.h"

extern char current_matrix_input_label[128];

void clear_matrix_input_context(void);
void print_current_input_matrix(void);

void d_to_coloured_string(double x, char *out, size_t out_size);
void d_to_coloured_err_string(double x, double tol, char *out, size_t out_size);
void qf_to_coloured_string(qfloat_t x, char *out, size_t out_size);
void qc_to_coloured_string(qcomplex_t z, char *out, size_t out_size);

void print_qc(const char *label, qcomplex_t z);
void print_qf(const char *label, qfloat_t x);
void print_md(const char *label, matrix_t *A);
void print_mqf(const char *label, matrix_t *A);
void print_mqc(const char *label, matrix_t *A);
void print_mdv(const char *label, matrix_t *A);

void check_d(const char *label, double got, double expected, double tol);
void check_qf_val(const char *label, qfloat_t got, qfloat_t expected, double tol);
void check_qc_val(const char *label, qcomplex_t got, qcomplex_t expected, double tol);
void check_bool(const char *label, int cond);

void check_mat_d(const char *label, matrix_t *got, matrix_t *expected_mat, double tol);
void check_mat_identity_d(const char *label, matrix_t *R, size_t n, double tol);
void check_mat_qf(const char *label, matrix_t *got, matrix_t *expected_mat, double tol);
void check_mat_qc(const char *label, matrix_t *got, matrix_t *expected_mat, double tol);
void check_mat_identity_qf(const char *label, matrix_t *R, size_t n, double tol);
void check_mat_identity_qc(const char *label, matrix_t *R, size_t n, double tol);

void run_matrix_core_tests(void);
void run_matrix_function_tests(void);
void run_matrix_function_regression_tests(void);

#endif
