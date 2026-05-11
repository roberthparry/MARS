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
    number_t re = num_real_part(x);
    number_t im = num_imag_part(x);
    qcomplex_t z = qc_make(num_to_qfloat(re), num_to_qfloat(im));

    num_destroy(&re);
    num_destroy(&im);
    return z;
}

#define num_to_qcomplex test_num_to_qcomplex

static inline number_t test_num_from_d(double x)
{
    return num_create_from_double(x);
}

static inline number_t test_num_from_qf(qfloat_t x)
{
    return num_create_from_qfloat(x);
}

static inline number_t test_num_from_qc(qcomplex_t z)
{
    return num_create_from_qcomplex(z);
}

static inline void test_mat_set_num_slot(matrix_t *A, size_t i, size_t j, const number_t *value)
{
    mat_set(A, i, j, value);
}

static inline void test_mat_set_d(matrix_t *A, size_t i, size_t j, const double *value)
{
    number_t n = test_num_from_d(*value);
    mat_set(A, i, j, &n);
    num_destroy(&n);
}

static inline void test_mat_set_qf(matrix_t *A, size_t i, size_t j, const qfloat_t *value)
{
    number_t n = test_num_from_qf(*value);
    mat_set(A, i, j, &n);
    num_destroy(&n);
}

static inline void test_mat_set_qc(matrix_t *A, size_t i, size_t j, const qcomplex_t *value)
{
    number_t n = test_num_from_qc(*value);
    mat_set(A, i, j, &n);
    num_destroy(&n);
}

static inline void test_mat_set_dv_slot(matrix_t *A, size_t i, size_t j, dval_t *const *value)
{
    mat_set(A, i, j, value);
}

static inline void test_mat_get_num_slot(const matrix_t *A, size_t i, size_t j, number_t *out)
{
    if (!out)
        return;
    *out = mat_get_num(A, i, j);
}

static inline void test_mat_get_d(const matrix_t *A, size_t i, size_t j, double *out)
{
    number_t n, re;

    if (!out)
        return;
    n = mat_get_num(A, i, j);
    re = num_real_part(n);
    *out = num_to_double(re);
    num_destroy(&re);
    num_destroy(&n);
}

static inline void test_mat_get_qf(const matrix_t *A, size_t i, size_t j, qfloat_t *out)
{
    number_t n, re;

    if (!out)
        return;
    n = mat_get_num(A, i, j);
    re = num_real_part(n);
    *out = num_to_qfloat(re);
    num_destroy(&re);
    num_destroy(&n);
}

static inline void test_mat_get_qc(const matrix_t *A, size_t i, size_t j, qcomplex_t *out)
{
    number_t n;

    if (!out)
        return;
    n = mat_get_num(A, i, j);
    *out = test_num_to_qcomplex(n);
    num_destroy(&n);
}

static inline void test_mat_get_dv_slot(const matrix_t *A, size_t i, size_t j, dval_t **out)
{
    mat_get_owned(A, i, j, out);
}

static inline void test_mat_set_data_num_slot(matrix_t *A, const number_t *data)
{
    mat_set_data(A, data);
}

static inline void test_mat_set_data_dv_slot(matrix_t *A, dval_t *const *data)
{
    mat_set_data(A, data);
}

static inline void test_mat_set_data_d(matrix_t *A, const double *data)
{
    size_t count, i;
    number_t *tmp;

    if (!A || !data)
        return;

    count = mat_get_row_count(A) * mat_get_col_count(A);
    tmp = calloc(count, sizeof(*tmp));
    if (!tmp)
        return;

    for (i = 0; i < count; ++i)
        tmp[i] = test_num_from_d(data[i]);

    mat_set_data(A, tmp);

    for (i = 0; i < count; ++i)
        num_destroy(&tmp[i]);
    free(tmp);
}

static inline void test_mat_set_data_qf(matrix_t *A, const qfloat_t *data)
{
    size_t count, i;
    number_t *tmp;

    if (!A || !data)
        return;

    count = mat_get_row_count(A) * mat_get_col_count(A);
    tmp = calloc(count, sizeof(*tmp));
    if (!tmp)
        return;

    for (i = 0; i < count; ++i)
        tmp[i] = test_num_from_qf(data[i]);

    mat_set_data(A, tmp);

    for (i = 0; i < count; ++i)
        num_destroy(&tmp[i]);
    free(tmp);
}

static inline void test_mat_set_data_qc(matrix_t *A, const qcomplex_t *data)
{
    size_t count, i;
    number_t *tmp;

    if (!A || !data)
        return;

    count = mat_get_row_count(A) * mat_get_col_count(A);
    tmp = calloc(count, sizeof(*tmp));
    if (!tmp)
        return;

    for (i = 0; i < count; ++i)
        tmp[i] = test_num_from_qc(data[i]);

    mat_set_data(A, tmp);

    for (i = 0; i < count; ++i)
        num_destroy(&tmp[i]);
    free(tmp);
}

static inline void test_mat_get_data_num_slot(const matrix_t *A, number_t *data)
{
    mat_get_data_num(A, data);
}

static inline void test_mat_get_data_dv_slot(const matrix_t *A, dval_t **data)
{
    size_t rows, cols, idx = 0;

    if (!A || !data)
        return;

    rows = mat_get_row_count(A);
    cols = mat_get_col_count(A);
    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < cols; ++j)
            mat_get_owned(A, i, j, &data[idx++]);
}

static inline void test_mat_get_data_d(const matrix_t *A, double *data)
{
    size_t rows, cols, idx = 0;

    if (!A || !data)
        return;

    rows = mat_get_row_count(A);
    cols = mat_get_col_count(A);
    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < cols; ++j)
            test_mat_get_d(A, i, j, &data[idx++]);
}

static inline void test_mat_get_data_qf(const matrix_t *A, qfloat_t *data)
{
    size_t rows, cols, idx = 0;

    if (!A || !data)
        return;

    rows = mat_get_row_count(A);
    cols = mat_get_col_count(A);
    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < cols; ++j)
            test_mat_get_qf(A, i, j, &data[idx++]);
}

static inline void test_mat_get_data_qc(const matrix_t *A, qcomplex_t *data)
{
    size_t rows, cols, idx = 0;

    if (!A || !data)
        return;

    rows = mat_get_row_count(A);
    cols = mat_get_col_count(A);
    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < cols; ++j)
            test_mat_get_qc(A, i, j, &data[idx++]);
}

static inline int test_mat_trace_num_slot(const matrix_t *A, number_t *trace)
{
    return mat_trace(A, trace);
}

static inline int test_mat_trace_d(const matrix_t *A, double *trace)
{
    number_t n, re;
    int rc;

    if (!trace)
        return -1;
    *trace = 0.0;

    rc = mat_trace(A, &n);
    if (rc != 0)
        return rc;

    re = num_real_part(n);
    *trace = num_to_double(re);
    num_destroy(&re);
    num_destroy(&n);
    return 0;
}

static inline int test_mat_trace_qf(const matrix_t *A, qfloat_t *trace)
{
    number_t n, re;
    int rc;

    if (!trace)
        return -1;
    *trace = QF_ZERO;

    rc = mat_trace(A, &n);
    if (rc != 0)
        return rc;

    re = num_real_part(n);
    *trace = num_to_qfloat(re);
    num_destroy(&re);
    num_destroy(&n);
    return 0;
}

static inline int test_mat_trace_qc(const matrix_t *A, qcomplex_t *trace)
{
    number_t n;
    int rc;

    if (!trace)
        return -1;
    *trace = QC_ZERO;

    rc = mat_trace(A, &n);
    if (rc != 0)
        return rc;

    *trace = test_num_to_qcomplex(n);
    num_destroy(&n);
    return 0;
}

static inline int test_mat_trace_dv_slot(const matrix_t *A, dval_t **trace)
{
    return mat_trace(A, trace);
}

static inline int test_mat_det_num_slot(const matrix_t *A, number_t *det)
{
    return mat_det(A, det);
}

static inline int test_mat_det_d(const matrix_t *A, double *det)
{
    number_t n, re;
    int rc;

    if (!det)
        return -1;
    *det = 0.0;

    rc = mat_det(A, &n);
    if (rc != 0)
        return rc;

    re = num_real_part(n);
    *det = num_to_double(re);
    num_destroy(&re);
    num_destroy(&n);
    return 0;
}

static inline int test_mat_det_qf(const matrix_t *A, qfloat_t *det)
{
    number_t n, re;
    int rc;

    if (!det)
        return -1;
    *det = QF_ZERO;

    rc = mat_det(A, &n);
    if (rc != 0)
        return rc;

    re = num_real_part(n);
    *det = num_to_qfloat(re);
    num_destroy(&re);
    num_destroy(&n);
    return 0;
}

static inline int test_mat_det_qc(const matrix_t *A, qcomplex_t *det)
{
    number_t n;
    int rc;

    if (!det)
        return -1;
    *det = QC_ZERO;

    rc = mat_det(A, &n);
    if (rc != 0)
        return rc;

    *det = test_num_to_qcomplex(n);
    num_destroy(&n);
    return 0;
}

static inline int test_mat_det_dv_slot(const matrix_t *A, dval_t **det)
{
    return mat_det(A, det);
}

#define mat_set(A, i, j, value) \
    _Generic(*(value), \
        double: test_mat_set_d, \
        const double: test_mat_set_d, \
        qfloat_t: test_mat_set_qf, \
        const qfloat_t: test_mat_set_qf, \
        qcomplex_t: test_mat_set_qc, \
        const qcomplex_t: test_mat_set_qc, \
        number_t: test_mat_set_num_slot, \
        const number_t: test_mat_set_num_slot, \
        dval_t *: test_mat_set_dv_slot, \
        const dval_t *: test_mat_set_dv_slot \
    )((A), (i), (j), (value))

#define mat_get(A, i, j, out) \
    _Generic(*(out), \
        double: test_mat_get_d, \
        qfloat_t: test_mat_get_qf, \
        qcomplex_t: test_mat_get_qc, \
        number_t: test_mat_get_num_slot, \
        dval_t *: test_mat_get_dv_slot \
    )((A), (i), (j), (out))

#define mat_set_data(A, data) \
    _Generic(*(data), \
        double: test_mat_set_data_d, \
        const double: test_mat_set_data_d, \
        qfloat_t: test_mat_set_data_qf, \
        const qfloat_t: test_mat_set_data_qf, \
        qcomplex_t: test_mat_set_data_qc, \
        const qcomplex_t: test_mat_set_data_qc, \
        number_t: test_mat_set_data_num_slot, \
        const number_t: test_mat_set_data_num_slot, \
        dval_t *: test_mat_set_data_dv_slot, \
        const dval_t *: test_mat_set_data_dv_slot \
    )((A), (data))

#define mat_get_data(A, data) \
    _Generic(*(data), \
        double: test_mat_get_data_d, \
        qfloat_t: test_mat_get_data_qf, \
        qcomplex_t: test_mat_get_data_qc, \
        number_t: test_mat_get_data_num_slot, \
        dval_t *: test_mat_get_data_dv_slot \
    )((A), (data))

#define mat_trace(A, trace) \
    _Generic(*(trace), \
        double: test_mat_trace_d, \
        qfloat_t: test_mat_trace_qf, \
        qcomplex_t: test_mat_trace_qc, \
        number_t: test_mat_trace_num_slot, \
        dval_t *: test_mat_trace_dv_slot \
    )((A), (trace))

#define mat_det(A, det) \
    _Generic(*(det), \
        double: test_mat_det_d, \
        qfloat_t: test_mat_det_qf, \
        qcomplex_t: test_mat_det_qc, \
        number_t: test_mat_det_num_slot, \
        dval_t *: test_mat_det_dv_slot \
    )((A), (det))

static inline matrix_t *test_mat_evaluate_qf(const matrix_t *A)
{
    return mat_evaluate_num(A);
}

static inline matrix_t *test_mat_evaluate_qc(const matrix_t *A)
{
    return mat_evaluate_num(A);
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
    size_t count = rows * cols;
    number_t *vals = calloc(count, sizeof(*vals));
    matrix_t *A;

    if (!vals)
        return NULL;
    for (size_t i = 0; i < count; ++i)
        vals[i] = num_clone(NUM_ZERO);
    A = mat_create_num(rows, cols, vals);
    for (size_t i = 0; i < count; ++i)
        num_destroy(&vals[i]);
    free(vals);
    return A;
}

static inline matrix_t *test_mat_dense_qf(size_t rows, size_t cols)
{
    return test_mat_dense_d(rows, cols);
}

static inline matrix_t *test_mat_dense_qc(size_t rows, size_t cols)
{
    return test_mat_dense_d(rows, cols);
}

static inline matrix_t *test_mat_sparse_d(size_t rows, size_t cols)
{
    return mat_new_sparse_num(rows, cols);
}

static inline matrix_t *test_mat_sparse_qf(size_t rows, size_t cols)
{
    return mat_new_sparse_num(rows, cols);
}

static inline matrix_t *test_mat_sparse_qc(size_t rows, size_t cols)
{
    return mat_new_sparse_num(rows, cols);
}

static inline matrix_t *test_mat_square_d(size_t n)
{
    return test_mat_dense_d(n, n);
}

static inline matrix_t *test_mat_square_qf(size_t n)
{
    return test_mat_dense_d(n, n);
}

static inline matrix_t *test_mat_square_qc(size_t n)
{
    return test_mat_dense_d(n, n);
}

static inline matrix_t *test_mat_square_dv(size_t n)
{
    return mat_create_dense_with_elem(n, n, &dval_elem);
}

static inline matrix_t *test_mat_identity_d(size_t n)
{
    return mat_create_identity_num(n);
}

static inline matrix_t *test_mat_identity_qf(size_t n)
{
    return mat_create_identity_num(n);
}

static inline matrix_t *test_mat_identity_qc(size_t n)
{
    return mat_create_identity_num(n);
}

static inline matrix_t *test_mat_diagonal_d(size_t n, const double *diagonal)
{
    matrix_t *A;
    number_t *vals;

    if (!diagonal)
        return NULL;
    vals = calloc(n, sizeof(*vals));
    if (!vals)
        return NULL;
    for (size_t i = 0; i < n; ++i)
        vals[i] = test_num_from_d(diagonal[i]);
    A = mat_create_diagonal_num(n, vals);
    for (size_t i = 0; i < n; ++i)
        num_destroy(&vals[i]);
    free(vals);
    return A;
}

static inline matrix_t *test_mat_diagonal_qf(size_t n, const qfloat_t *diagonal)
{
    matrix_t *A;
    number_t *vals;

    if (!diagonal)
        return NULL;
    vals = calloc(n, sizeof(*vals));
    if (!vals)
        return NULL;
    for (size_t i = 0; i < n; ++i)
        vals[i] = test_num_from_qf(diagonal[i]);
    A = mat_create_diagonal_num(n, vals);
    for (size_t i = 0; i < n; ++i)
        num_destroy(&vals[i]);
    free(vals);
    return A;
}

static inline matrix_t *test_mat_diagonal_qc(size_t n, const qcomplex_t *diagonal)
{
    matrix_t *A;
    number_t *vals;

    if (!diagonal)
        return NULL;
    vals = calloc(n, sizeof(*vals));
    if (!vals)
        return NULL;
    for (size_t i = 0; i < n; ++i)
        vals[i] = test_num_from_qc(diagonal[i]);
    A = mat_create_diagonal_num(n, vals);
    for (size_t i = 0; i < n; ++i)
        num_destroy(&vals[i]);
    free(vals);
    return A;
}

static inline matrix_t *test_mat_create_d(size_t rows, size_t cols, const double *data)
{
    matrix_t *A;
    number_t *vals;
    size_t count = rows * cols;

    if (!data)
        return NULL;
    vals = calloc(count, sizeof(*vals));
    if (!vals)
        return NULL;
    for (size_t i = 0; i < count; ++i)
        vals[i] = test_num_from_d(data[i]);
    A = mat_create_num(rows, cols, vals);
    for (size_t i = 0; i < count; ++i)
        num_destroy(&vals[i]);
    free(vals);
    return A;
}

static inline matrix_t *test_mat_create_qf(size_t rows, size_t cols, const qfloat_t *data)
{
    matrix_t *A;
    number_t *vals;
    size_t count = rows * cols;

    if (!data)
        return NULL;
    vals = calloc(count, sizeof(*vals));
    if (!vals)
        return NULL;
    for (size_t i = 0; i < count; ++i)
        vals[i] = test_num_from_qf(data[i]);
    A = mat_create_num(rows, cols, vals);
    for (size_t i = 0; i < count; ++i)
        num_destroy(&vals[i]);
    free(vals);
    return A;
}

static inline matrix_t *test_mat_create_qc(size_t rows, size_t cols, const qcomplex_t *data)
{
    matrix_t *A;
    number_t *vals;
    size_t count = rows * cols;

    if (!data)
        return NULL;
    vals = calloc(count, sizeof(*vals));
    if (!vals)
        return NULL;
    for (size_t i = 0; i < count; ++i)
        vals[i] = test_num_from_qc(data[i]);
    A = mat_create_num(rows, cols, vals);
    for (size_t i = 0; i < count; ++i)
        num_destroy(&vals[i]);
    free(vals);
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
