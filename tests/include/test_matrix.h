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

static inline number_t test_num_from_mp_real(qfloat_t x)
{
    return num_create_from_qfloat(x);
}

static inline number_t test_num_from_complex(qcomplex_t z)
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

static inline void test_mat_set_mp_real(matrix_t *A, size_t i, size_t j, const qfloat_t *value)
{
    number_t n = test_num_from_mp_real(*value);
    mat_set(A, i, j, &n);
    num_destroy(&n);
}

static inline void test_mat_set_complex(matrix_t *A, size_t i, size_t j, const qcomplex_t *value)
{
    number_t n = test_num_from_complex(*value);
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

static inline void test_mat_get_mp_real(const matrix_t *A, size_t i, size_t j, qfloat_t *out)
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

static inline void test_mat_get_complex(const matrix_t *A, size_t i, size_t j, qcomplex_t *out)
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
    mat_set_data_dv(A, data);
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

static inline void test_mat_set_data_mp_real(matrix_t *A, const qfloat_t *data)
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
        tmp[i] = test_num_from_mp_real(data[i]);

    mat_set_data(A, tmp);

    for (i = 0; i < count; ++i)
        num_destroy(&tmp[i]);
    free(tmp);
}

static inline void test_mat_set_data_complex(matrix_t *A, const qcomplex_t *data)
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
        tmp[i] = test_num_from_complex(data[i]);

    mat_set_data(A, tmp);

    for (i = 0; i < count; ++i)
        num_destroy(&tmp[i]);
    free(tmp);
}

static inline void test_mat_get_data_num_slot(const matrix_t *A, number_t *data)
{
    mat_get_data(A, data);
}

static inline void test_mat_get_data_dv_slot(const matrix_t *A, dval_t **data)
{
    mat_get_data_dv(A, data);
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

static inline void test_mat_get_data_mp_real(const matrix_t *A, qfloat_t *data)
{
    size_t rows, cols, idx = 0;

    if (!A || !data)
        return;

    rows = mat_get_row_count(A);
    cols = mat_get_col_count(A);
    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < cols; ++j)
            test_mat_get_mp_real(A, i, j, &data[idx++]);
}

static inline void test_mat_get_data_complex(const matrix_t *A, qcomplex_t *data)
{
    size_t rows, cols, idx = 0;

    if (!A || !data)
        return;

    rows = mat_get_row_count(A);
    cols = mat_get_col_count(A);
    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < cols; ++j)
            test_mat_get_complex(A, i, j, &data[idx++]);
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

static inline int test_mat_trace_mp_real(const matrix_t *A, qfloat_t *trace)
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

static inline int test_mat_trace_complex(const matrix_t *A, qcomplex_t *trace)
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
    return mat_trace_dv(A, trace);
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

static inline int test_mat_det_mp_real(const matrix_t *A, qfloat_t *det)
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

static inline int test_mat_det_complex(const matrix_t *A, qcomplex_t *det)
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
    return mat_det_dv(A, det);
}

static inline int test_mat_eigenvalues_num_slot(const matrix_t *A, number_t *eigenvalues)
{
    return mat_eigenvalues(A, eigenvalues);
}

static inline int test_mat_eigenvalues_dv_slot(const matrix_t *A, dval_t **eigenvalues)
{
    return mat_eigenvalues_dv(A, eigenvalues);
}

static inline int test_mat_eigendecompose_num_slot(const matrix_t *A,
                                                   number_t *eigenvalues,
                                                   matrix_t **eigenvectors)
{
    return mat_eigendecompose(A, eigenvalues, eigenvectors);
}

static inline int test_mat_eigendecompose_dv_slot(const matrix_t *A,
                                                  dval_t **eigenvalues,
                                                  matrix_t **eigenvectors)
{
    return mat_eigendecompose_dv(A, eigenvalues, eigenvectors);
}

static inline matrix_t *test_mat_eigenspace_num_slot(const matrix_t *A, const number_t *eigenvalue)
{
    return mat_eigenspace(A, eigenvalue);
}

static inline matrix_t *test_mat_eigenspace_dv_slot(const matrix_t *A, dval_t *const *eigenvalue)
{
    return mat_eigenspace_dv(A, eigenvalue ? *eigenvalue : NULL);
}

static inline matrix_t *test_mat_generalized_eigenspace_num_slot(const matrix_t *A,
                                                                 const number_t *eigenvalue,
                                                                 size_t order)
{
    return mat_generalized_eigenspace(A, eigenvalue, order);
}

static inline matrix_t *test_mat_generalized_eigenspace_dv_slot(const matrix_t *A,
                                                                dval_t *const *eigenvalue,
                                                                size_t order)
{
    return mat_generalized_eigenspace_dv(A, eigenvalue ? *eigenvalue : NULL, order);
}

static inline matrix_t *test_mat_jordan_chain_num_slot(const matrix_t *A,
                                                       const number_t *eigenvalue,
                                                       size_t order)
{
    return mat_jordan_chain(A, eigenvalue, order);
}

static inline matrix_t *test_mat_jordan_chain_dv_slot(const matrix_t *A,
                                                      dval_t *const *eigenvalue,
                                                      size_t order)
{
    return mat_jordan_chain_dv(A, eigenvalue ? *eigenvalue : NULL, order);
}

static inline matrix_t *test_mat_jordan_profile_num_slot(const matrix_t *A,
                                                         const number_t *eigenvalue)
{
    return mat_jordan_profile(A, eigenvalue);
}

static inline matrix_t *test_mat_jordan_profile_dv_slot(const matrix_t *A,
                                                        dval_t *const *eigenvalue)
{
    return mat_jordan_profile_dv(A, eigenvalue ? *eigenvalue : NULL);
}

#define mat_set(A, i, j, value) \
    _Generic(*(value), \
        double: test_mat_set_d, \
        const double: test_mat_set_d, \
        qfloat_t: test_mat_set_mp_real, \
        const qfloat_t: test_mat_set_mp_real, \
        qcomplex_t: test_mat_set_complex, \
        const qcomplex_t: test_mat_set_complex, \
        number_t: test_mat_set_num_slot, \
        const number_t: test_mat_set_num_slot, \
        dval_t *: test_mat_set_dv_slot, \
        const dval_t *: test_mat_set_dv_slot \
    )((A), (i), (j), (value))

#define mat_get(A, i, j, out) \
    _Generic(*(out), \
        double: test_mat_get_d, \
        qfloat_t: test_mat_get_mp_real, \
        qcomplex_t: test_mat_get_complex, \
        number_t: test_mat_get_num_slot, \
        dval_t *: test_mat_get_dv_slot \
    )((A), (i), (j), (out))

#define mat_set_data(A, data) \
    _Generic(*(data), \
        double: test_mat_set_data_d, \
        const double: test_mat_set_data_d, \
        qfloat_t: test_mat_set_data_mp_real, \
        const qfloat_t: test_mat_set_data_mp_real, \
        qcomplex_t: test_mat_set_data_complex, \
        const qcomplex_t: test_mat_set_data_complex, \
        number_t: test_mat_set_data_num_slot, \
        const number_t: test_mat_set_data_num_slot, \
        dval_t *: test_mat_set_data_dv_slot, \
        const dval_t *: test_mat_set_data_dv_slot \
    )((A), (data))

#define mat_get_data(A, data) \
    _Generic(*(data), \
        double: test_mat_get_data_d, \
        qfloat_t: test_mat_get_data_mp_real, \
        qcomplex_t: test_mat_get_data_complex, \
        number_t: test_mat_get_data_num_slot, \
        dval_t *: test_mat_get_data_dv_slot \
    )((A), (data))

#define mat_trace(A, trace) \
    _Generic(*(trace), \
        double: test_mat_trace_d, \
        qfloat_t: test_mat_trace_mp_real, \
        qcomplex_t: test_mat_trace_complex, \
        number_t: test_mat_trace_num_slot, \
        dval_t *: test_mat_trace_dv_slot \
    )((A), (trace))

#define mat_det(A, det) \
    _Generic(*(det), \
        double: test_mat_det_d, \
        qfloat_t: test_mat_det_mp_real, \
        qcomplex_t: test_mat_det_complex, \
        number_t: test_mat_det_num_slot, \
        dval_t *: test_mat_det_dv_slot \
    )((A), (det))

#define mat_eigenvalues(A, eigenvalues) \
    _Generic(*(eigenvalues), \
        number_t: test_mat_eigenvalues_num_slot, \
        dval_t *: test_mat_eigenvalues_dv_slot \
    )((A), (eigenvalues))

#define mat_eigendecompose(A, eigenvalues, eigenvectors) \
    _Generic(*(eigenvalues), \
        number_t: test_mat_eigendecompose_num_slot, \
        dval_t *: test_mat_eigendecompose_dv_slot \
    )((A), (eigenvalues), (eigenvectors))

#define mat_eigenspace(A, eigenvalue) \
    _Generic(*(eigenvalue), \
        number_t: test_mat_eigenspace_num_slot, \
        dval_t *: test_mat_eigenspace_dv_slot \
    )((A), (eigenvalue))

#define mat_generalized_eigenspace(A, eigenvalue, order) \
    _Generic(*(eigenvalue), \
        number_t: test_mat_generalized_eigenspace_num_slot, \
        dval_t *: test_mat_generalized_eigenspace_dv_slot \
    )((A), (eigenvalue), (order))

#define mat_jordan_chain(A, eigenvalue, order) \
    _Generic(*(eigenvalue), \
        number_t: test_mat_jordan_chain_num_slot, \
        dval_t *: test_mat_jordan_chain_dv_slot \
    )((A), (eigenvalue), (order))

#define mat_jordan_profile(A, eigenvalue) \
    _Generic(*(eigenvalue), \
        number_t: test_mat_jordan_profile_num_slot, \
        dval_t *: test_mat_jordan_profile_dv_slot \
    )((A), (eigenvalue))

static inline matrix_t *test_mat_evaluate_mp_real(const matrix_t *A)
{
    return mat_evaluate(A);
}

static inline matrix_t *test_mat_evaluate_complex(const matrix_t *A)
{
    return mat_evaluate(A);
}

static inline dval_t *test_dv_new_const_d(double x)
{
    number_t n = num_create_from_double(x);
    dval_t *dv = dv_new_const(n);

    num_destroy(&n);
    return dv;
}

static inline dval_t *test_dv_new_named_const_d(double x, const char *name)
{
    number_t n = num_create_from_double(x);
    dval_t *dv = dv_new_named_const(n, name);

    num_destroy(&n);
    return dv;
}

static inline dval_t *test_dv_new_named_const_mp_real(qfloat_t x, const char *name)
{
    number_t n = num_create_from_qfloat(x);
    dval_t *dv = dv_new_named_const(n, name);

    num_destroy(&n);
    return dv;
}

static inline dval_t *test_dv_new_named_var_d(double x, const char *name)
{
    number_t n = num_create_from_double(x);
    dval_t *dv = dv_new_named_var(n, name);

    num_destroy(&n);
    return dv;
}

static inline void test_dv_set_val_d(dval_t *dv, double x)
{
    number_t n = num_create_from_double(x);

    dv_set_val(dv, n);
    num_destroy(&n);
}

static inline void test_dv_set_val_mp_real(dval_t *dv, qfloat_t x)
{
    number_t n = num_create_from_qfloat(x);

    dv_set_val(dv, n);
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

static inline int test_mat_bindings_set_mp_real(mat_bindings_t *bnd, const char *name, qfloat_t x)
{
    dval_t *dv = mat_bindings_get(bnd, name);

    if (!dv)
        return -1;
    test_dv_set_val_mp_real(dv, x);
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

static inline qfloat_t test_dv_eval_mp_real(const dval_t *dv)
{
    number_t n = dv_eval(dv);
    qfloat_t out = num_to_qfloat(n);

    num_destroy(&n);
    return out;
}

#define dv_eval_d  test_dv_eval_d
#define dv_eval_mp_real test_dv_eval_mp_real
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
    A = mat_create(rows, cols, vals);
    for (size_t i = 0; i < count; ++i)
        num_destroy(&vals[i]);
    free(vals);
    return A;
}

static inline matrix_t *test_mat_sparse_d(size_t rows, size_t cols)
{
    return mat_new_sparse(rows, cols);
}

static inline matrix_t *test_mat_square_d(size_t n)
{
    return test_mat_dense_d(n, n);
}

static inline matrix_t *test_mat_square_dv(size_t n)
{
    return mat_create_dense_with_elem(n, n, &dval_elem);
}

static inline matrix_t *test_mat_identity_d(size_t n)
{
    return mat_create_identity(n);
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
    A = mat_create_diagonal(n, vals);
    for (size_t i = 0; i < n; ++i)
        num_destroy(&vals[i]);
    free(vals);
    return A;
}

static inline matrix_t *test_mat_diagonal_mp_real(size_t n, const qfloat_t *diagonal)
{
    matrix_t *A;
    number_t *vals;

    if (!diagonal)
        return NULL;
    vals = calloc(n, sizeof(*vals));
    if (!vals)
        return NULL;
    for (size_t i = 0; i < n; ++i)
        vals[i] = test_num_from_mp_real(diagonal[i]);
    A = mat_create_diagonal(n, vals);
    for (size_t i = 0; i < n; ++i)
        num_destroy(&vals[i]);
    free(vals);
    return A;
}

static inline matrix_t *test_mat_diagonal_complex(size_t n, const qcomplex_t *diagonal)
{
    matrix_t *A;
    number_t *vals;

    if (!diagonal)
        return NULL;
    vals = calloc(n, sizeof(*vals));
    if (!vals)
        return NULL;
    for (size_t i = 0; i < n; ++i)
        vals[i] = test_num_from_complex(diagonal[i]);
    A = mat_create_diagonal(n, vals);
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
    A = mat_create(rows, cols, vals);
    for (size_t i = 0; i < count; ++i)
        num_destroy(&vals[i]);
    free(vals);
    return A;
}

static inline matrix_t *test_mat_create_mp_real(size_t rows, size_t cols, const qfloat_t *data)
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
        vals[i] = test_num_from_mp_real(data[i]);
    A = mat_create(rows, cols, vals);
    for (size_t i = 0; i < count; ++i)
        num_destroy(&vals[i]);
    free(vals);
    return A;
}

static inline matrix_t *test_mat_create_complex(size_t rows, size_t cols, const qcomplex_t *data)
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
        vals[i] = test_num_from_complex(data[i]);
    A = mat_create(rows, cols, vals);
    for (size_t i = 0; i < count; ++i)
        num_destroy(&vals[i]);
    free(vals);
    return A;
}

#define mat_binding_find(bnd, number, name) mat_bindings_get((bnd), (name))
#define mat_binding_get(bnd, number, name)  mat_bindings_get((bnd), (name))
#define mat_binding_set_d(bnd, number, name, value) test_mat_bindings_set_d((bnd), (name), (value))
#define mat_binding_set_mp_real(bnd, number, name, value) test_mat_bindings_set_mp_real((bnd), (name), (value))

/* Temporary compatibility aliases while the matrix tests are migrated. */
#define mat_new_num              mat_new
#define mat_new_sparse_num       mat_new_sparse
#define matsq_new_num            matsq_new
#define mat_create_identity_num  mat_create_identity
#define mat_create_diagonal_num  mat_create_diagonal
#define mat_create_num           mat_create
#define mat_evaluate_num         mat_evaluate
#define mat_get_data_num         mat_get_data

extern char current_matrix_input_label[128];

void clear_matrix_input_context(void);
void print_current_input_matrix(void);

void d_to_coloured_string(double x, char *out, size_t out_size);
void d_to_coloured_err_string(double x, double tol, char *out, size_t out_size);
void qf_to_coloured_string(qfloat_t x, char *out, size_t out_size);
void qc_to_coloured_string(qcomplex_t z, char *out, size_t out_size);

void print_complex(const char *label, qcomplex_t z);
void print_mp_real(const char *label, qfloat_t x);
void print_md(const char *label, matrix_t *A);
void print_mqf(const char *label, matrix_t *A);
void print_mqc(const char *label, matrix_t *A);
void print_mnum(const char *label, matrix_t *A);
void print_mdv(const char *label, matrix_t *A);
void print_matrix_working_precision_line(const char *label, const matrix_t *A);

void check_d(const char *label, double got, double expected, double tol);
void check_qf_val(const char *label, qfloat_t got, qfloat_t expected, double tol);
void check_qc_val(const char *label, qcomplex_t got, qcomplex_t expected, double tol);
void check_bool(const char *label, int cond);

void check_mat_d(const char *label, matrix_t *got, matrix_t *expected_mat, double tol);
void check_mat_identity_d(const char *label, matrix_t *R, size_t n, double tol);
void check_mat_mp_real(const char *label, matrix_t *got, matrix_t *expected_mat, double tol);
void check_mat_complex(const char *label, matrix_t *got, matrix_t *expected_mat, double tol);
void check_mat_identity_mp_real(const char *label, matrix_t *R, size_t n, double tol);
void check_mat_identity_complex(const char *label, matrix_t *R, size_t n, double tol);

void run_matrix_core_tests(void);
void run_matrix_function_tests(void);
void run_matrix_function_regression_tests(void);
void run_matrix_fromstring_tests(void);
void run_matrix_tostring_tests(void);
void run_matrix_output_tests(void);

#endif
