#ifndef TESTS_MATRIX_MATRIX_TEST_H
#define TESTS_MATRIX_MATRIX_TEST_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matrix.h"
#include "matrix/matrix_internal.h"
#include "number/number_internal.h"
#include "test_harness.h"

static inline qcomplex_t test_num_to_qcomplex(const number_t x)
{
    return number_value_to_qcomplex(&x);
}

#define num_to_qcomplex test_num_to_qcomplex

static inline matrix_t *test_mat_evaluate_qf(const matrix_t *A)
{
    matrix_t *N = mat_evaluate_num(A);
    matrix_t *Q = NULL;

    if (!N)
        return NULL;
    Q = mat_convert_with_store(N, &qfloat_elem, N->store);
    mat_free(N);
    return Q;
}

static inline matrix_t *test_mat_evaluate_qc(const matrix_t *A)
{
    matrix_t *N = mat_evaluate_num(A);
    matrix_t *Z = NULL;

    if (!N)
        return NULL;
    Z = mat_convert_with_store(N, &qcomplex_elem, N->store);
    mat_free(N);
    return Z;
}

static inline dval_t *test_dv_new_const_d(double x)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));
    dval_t *dv = dv_new_const_num(n);

    num_destroy(&n);
    return dv;
}

static inline dval_t *test_dv_new_named_const_d(double x, const char *name)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));
    dval_t *dv = dv_new_named_const_num(n, name);

    num_destroy(&n);
    return dv;
}

static inline dval_t *test_dv_new_named_const_qf(qfloat_t x, const char *name)
{
    number_t n = num_create_from_qfloat(x);
    dval_t *dv = dv_new_named_const_num(n, name);

    num_destroy(&n);
    return dv;
}

static inline dval_t *test_dv_new_named_var_d(double x, const char *name)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));
    dval_t *dv = dv_new_named_var_num(n, name);

    num_destroy(&n);
    return dv;
}

static inline void test_dv_set_val_d(dval_t *dv, double x)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));

    dv_set_val_num(dv, n);
    num_destroy(&n);
}

static inline void test_dv_set_val_qf(dval_t *dv, qfloat_t x)
{
    number_t n = num_create_from_qfloat(x);

    dv_set_val_num(dv, n);
    num_destroy(&n);
}

static inline int test_mat_bindings_set_d(mat_bindings_t *bnd, const char *name, double x)
{
    dval_t *dv = mat_bindings_get(bnd, name);

    if (!dv)
        return -1;
    test_dv_set_val_d(dv, x);
    return 0;
}

static inline int test_mat_bindings_set_qf(mat_bindings_t *bnd, const char *name, qfloat_t x)
{
    dval_t *dv = mat_bindings_get(bnd, name);

    if (!dv)
        return -1;
    test_dv_set_val_qf(dv, x);
    return 0;
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
    dval_t *out = dv_pow_num(dv, &n);

    num_destroy(&n);
    return out;
}

static inline double test_dv_eval_d(const dval_t *dv)
{
    number_t n = dv_eval_num(dv);
    double out = num_to_double(n);

    num_destroy(&n);
    return out;
}

static inline qfloat_t test_dv_eval_qf(const dval_t *dv)
{
    number_t n = dv_eval_num(dv);
    qfloat_t out = num_to_qfloat(n);

    num_destroy(&n);
    return out;
}

#define dv_eval_d  test_dv_eval_d
#define dv_eval_qf test_dv_eval_qf
#define dv_add_d   test_dv_add_d
#define dv_sub_d   test_dv_sub_d
#define dv_d_sub   test_dv_d_sub
#define dv_mul_d   test_dv_mul_d
#define dv_div_d   test_dv_div_d
#define dv_d_div   test_dv_d_div
#define dv_pow_d   test_dv_pow_d

static inline matrix_t *test_mat_dense_d(size_t rows, size_t cols)
{
    return mat_create_dense_with_elem(rows, cols, &double_elem);
}

static inline matrix_t *test_mat_dense_qf(size_t rows, size_t cols)
{
    return mat_create_dense_with_elem(rows, cols, &qfloat_elem);
}

static inline matrix_t *test_mat_dense_qc(size_t rows, size_t cols)
{
    return mat_create_dense_with_elem(rows, cols, &qcomplex_elem);
}

static inline matrix_t *test_mat_sparse_d(size_t rows, size_t cols)
{
    return mat_create_sparse_with_elem(rows, cols, &double_elem);
}

static inline matrix_t *test_mat_sparse_qf(size_t rows, size_t cols)
{
    return mat_create_sparse_with_elem(rows, cols, &qfloat_elem);
}

static inline matrix_t *test_mat_sparse_qc(size_t rows, size_t cols)
{
    return mat_create_sparse_with_elem(rows, cols, &qcomplex_elem);
}

static inline matrix_t *test_mat_square_d(size_t n)
{
    return mat_create_dense_with_elem(n, n, &double_elem);
}

static inline matrix_t *test_mat_square_qf(size_t n)
{
    return mat_create_dense_with_elem(n, n, &qfloat_elem);
}

static inline matrix_t *test_mat_square_qc(size_t n)
{
    return mat_create_dense_with_elem(n, n, &qcomplex_elem);
}

static inline matrix_t *test_mat_square_dv(size_t n)
{
    return mat_create_dense_with_elem(n, n, &dval_elem);
}

static inline matrix_t *test_mat_identity_d(size_t n)
{
    return mat_create_identity_with_elem(n, &double_elem);
}

static inline matrix_t *test_mat_identity_qf(size_t n)
{
    return mat_create_identity_with_elem(n, &qfloat_elem);
}

static inline matrix_t *test_mat_identity_qc(size_t n)
{
    return mat_create_identity_with_elem(n, &qcomplex_elem);
}

static inline matrix_t *test_mat_diagonal_d(size_t n, const double *diagonal)
{
    matrix_t *A = mat_create_diagonal_with_elem(n, &double_elem);

    if (!A || !diagonal)
        return A;
    for (size_t i = 0; i < n; ++i)
        mat_set(A, i, i, &diagonal[i]);
    return A;
}

static inline matrix_t *test_mat_diagonal_qf(size_t n, const qfloat_t *diagonal)
{
    matrix_t *A = mat_create_diagonal_with_elem(n, &qfloat_elem);

    if (!A || !diagonal)
        return A;
    for (size_t i = 0; i < n; ++i)
        mat_set(A, i, i, &diagonal[i]);
    return A;
}

static inline matrix_t *test_mat_diagonal_qc(size_t n, const qcomplex_t *diagonal)
{
    matrix_t *A = mat_create_diagonal_with_elem(n, &qcomplex_elem);

    if (!A || !diagonal)
        return A;
    for (size_t i = 0; i < n; ++i)
        mat_set(A, i, i, &diagonal[i]);
    return A;
}

static inline matrix_t *test_mat_create_d(size_t rows, size_t cols, const double *data)
{
    matrix_t *A = mat_create_dense_with_elem(rows, cols, &double_elem);

    if (A && data)
        mat_set_data(A, data);
    return A;
}

static inline matrix_t *test_mat_create_qf(size_t rows, size_t cols, const qfloat_t *data)
{
    matrix_t *A = mat_create_dense_with_elem(rows, cols, &qfloat_elem);

    if (A && data)
        mat_set_data(A, data);
    return A;
}

static inline matrix_t *test_mat_create_qc(size_t rows, size_t cols, const qcomplex_t *data)
{
    matrix_t *A = mat_create_dense_with_elem(rows, cols, &qcomplex_elem);

    if (A && data)
        mat_set_data(A, data);
    return A;
}

#define mat_binding_find(bnd, number, name) mat_bindings_get((bnd), (name))
#define mat_binding_get(bnd, number, name)  mat_bindings_get((bnd), (name))
#define mat_binding_set_d(bnd, number, name, value) test_mat_bindings_set_d((bnd), (name), (value))
#define mat_binding_set_qf(bnd, number, name, value) test_mat_bindings_set_qf((bnd), (name), (value))

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
void print_mnum(const char *label, matrix_t *A);
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
void run_matrix_fromstring_tests(void);
void run_matrix_tostring_tests(void);
void run_matrix_output_tests(void);

#endif
