#ifndef TESTS_MATRIX_MATRIX_TEST_H
#define TESTS_MATRIX_MATRIX_TEST_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matrix.h"
#include "../expression/test_expr.h"
#include "test_harness.h"

const test_validity_contract_t *matrix_validity_contract_double_default(void);
const test_validity_contract_t *matrix_validity_contract_mp_real_default(void);
const test_validity_contract_t *matrix_validity_contract_complex_default(void);
bool test_assert_matrix_d_close(matrix_t *got,
                                matrix_t *expected,
                                double tol,
                                const char *file,
                                int line);
bool test_assert_matrix_mp_real_close(matrix_t *got,
                                      matrix_t *expected,
                                      double tol,
                                      const char *file,
                                      int line);
bool test_assert_matrix_complex_close(matrix_t *got,
                                      matrix_t *expected,
                                      double tol,
                                      const char *file,
                                      int line);
bool test_assert_matrix_d_identity(matrix_t *got,
                                   size_t n,
                                   double tol,
                                   const char *file,
                                   int line);
bool test_assert_matrix_mp_real_identity(matrix_t *got,
                                         size_t n,
                                         double tol,
                                         const char *file,
                                         int line);
bool test_assert_matrix_complex_identity(matrix_t *got,
                                         size_t n,
                                         double tol,
                                         const char *file,
                                         int line);

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

#define mat_create_num(rows, cols, values) \
    mat_create((rows), (cols), (values))

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

static inline matrix_t *test_mat_dense_d(size_t rows, size_t cols)
{
    size_t count = rows * cols;
    number_t *values = calloc(count ? count : 1u, sizeof(*values));
    matrix_t *out;

    if (!values)
        return NULL;

    for (size_t i = 0; i < count; ++i)
        values[i] = test_num_from_d(0.0);

    out = mat_create(rows, cols, values);
    for (size_t i = 0; i < count; ++i)
        num_destroy(&values[i]);
    free(values);
    return out;
}

static inline matrix_t *test_mat_evaluate_complex(const matrix_t *A)
{
    size_t rows, cols, count, idx;
    number_t *values;
    matrix_t *out;

    if (!A)
        return NULL;

    rows = mat_get_row_count(A);
    cols = mat_get_col_count(A);
    count = rows * cols;
    values = calloc(count ? count : 1u, sizeof(*values));
    if (!values)
        return NULL;

    idx = 0u;
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j)
            values[idx++] = mat_get_num(A, i, j);
    }

    out = mat_create(rows, cols, values);
    for (idx = 0u; idx < count; ++idx)
        num_destroy(&values[idx]);
    free(values);
    return out;
}

static inline matrix_t *test_mat_evaluate_mp_real(const matrix_t *A)
{
    return mat_evaluate(A);
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

static inline void test_mat_set_expr_slot(matrix_t *A, size_t i, size_t j, expr_t *const *value)
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

static inline void test_mat_get_expr_slot(const matrix_t *A, size_t i, size_t j, expr_t **out)
{
    expr_t *borrowed = NULL;

    if (!out)
        return;
    mat_get(A, i, j, &borrowed);
    *out = borrowed;
    if (*out)
        expr_retain(*out);
}

static inline void test_mat_set_data_num_slot(matrix_t *A, const number_t *data)
{
    mat_set_data(A, data);
}

static inline void test_mat_set_data_expr_slot(matrix_t *A, expr_t *const *data)
{
    mat_set_data_expr(A, data);
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

static inline void test_mat_get_data_expr_slot(const matrix_t *A, expr_t **data)
{
    mat_get_data_expr(A, data);
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

static inline matrix_t *test_mat_identity_d(size_t n)
{
    return mat_create_identity(n);
}

static inline matrix_t *test_mat_sparse_d(size_t rows, size_t cols)
{
    return mat_new_sparse(rows, cols);
}

static inline matrix_t *test_mat_diagonal_d(size_t n, const double *diagonal)
{
    size_t i;
    number_t *tmp;
    matrix_t *out;

    if (!diagonal)
        return NULL;

    tmp = calloc(n ? n : 1u, sizeof(*tmp));
    if (!tmp)
        return NULL;

    for (i = 0; i < n; ++i)
        tmp[i] = test_num_from_d(diagonal[i]);

    out = mat_create_diagonal(n, tmp);

    for (i = 0; i < n; ++i)
        num_destroy(&tmp[i]);
    free(tmp);
    return out;
}

static inline int test_mat_trace_num_slot(const matrix_t *A, number_t *trace)
{
    return mat_trace(A, trace);
}

static inline int test_mat_trace_d(const matrix_t *A, double *trace)
{
    number_t n, re;

    if (!trace)
        return -1;
    if (mat_trace(A, &n) != 0)
        return -1;
    re = num_real_part(n);
    *trace = num_to_double(re);
    num_destroy(&re);
    num_destroy(&n);
    return 0;
}

static inline int test_mat_det_num_slot(const matrix_t *A, number_t *determinant)
{
    return mat_det(A, determinant);
}

static inline int test_mat_det_d(const matrix_t *A, double *determinant)
{
    number_t n, re;

    if (!determinant)
        return -1;
    if (mat_det(A, &n) != 0)
        return -1;
    re = num_real_part(n);
    *determinant = num_to_double(re);
    num_destroy(&re);
    num_destroy(&n);
    return 0;
}

static inline int test_mat_det_mp_real(const matrix_t *A, qfloat_t *determinant)
{
    number_t n, re;

    if (!determinant)
        return -1;
    if (mat_det(A, &n) != 0)
        return -1;
    re = num_real_part(n);
    *determinant = num_to_qfloat(re);
    num_destroy(&re);
    num_destroy(&n);
    return 0;
}

static inline int test_mat_det_complex(const matrix_t *A, qcomplex_t *determinant)
{
    number_t n;

    if (!determinant)
        return -1;
    if (mat_det(A, &n) != 0)
        return -1;
    *determinant = test_num_to_qcomplex(n);
    num_destroy(&n);
    return 0;
}

static inline int test_mat_det_expr_slot(const matrix_t *A, expr_t **determinant)
{
    return mat_det_expr(A, determinant);
}

static inline matrix_t *test_mat_eigenspace_expr_slot(const matrix_t *A, expr_t **eigenvalue)
{
    return (A && eigenvalue && *eigenvalue) ? mat_eigenspace_expr(A, *eigenvalue) : NULL;
}

static inline matrix_t *test_mat_generalized_eigenspace_expr_slot(const matrix_t *A,
                                                                 expr_t **eigenvalue,
                                                                 size_t order)
{
    return (A && eigenvalue && *eigenvalue)
             ? mat_generalized_eigenspace_expr(A, *eigenvalue, order)
             : NULL;
}

static inline matrix_t *test_mat_jordan_chain_expr_slot(const matrix_t *A,
                                                       expr_t **eigenvalue,
                                                       size_t order)
{
    return (A && eigenvalue && *eigenvalue)
             ? mat_jordan_chain_expr(A, *eigenvalue, order)
             : NULL;
}

static inline matrix_t *test_mat_jordan_profile_expr_slot(const matrix_t *A, expr_t **eigenvalue)
{
    return (A && eigenvalue && *eigenvalue) ? mat_jordan_profile_expr(A, *eigenvalue) : NULL;
}

static inline int test_mat_trace_mp_real(const matrix_t *A, qfloat_t *trace)
{
    number_t n, re;

    if (!trace)
        return -1;
    if (mat_trace(A, &n) != 0)
        return -1;
    re = num_real_part(n);
    *trace = num_to_qfloat(re);
    num_destroy(&re);
    num_destroy(&n);
    return 0;
}

static inline int test_mat_trace_complex(const matrix_t *A, qcomplex_t *trace)
{
    number_t n;

    if (!trace)
        return -1;
    if (mat_trace(A, &n) != 0)
        return -1;
    *trace = test_num_to_qcomplex(n);
    num_destroy(&n);
    return 0;
}

void d_to_coloured_string(double x, char *out, size_t out_size);
void d_to_coloured_err_string(double x, double tol, char *out, size_t out_size);
void qf_to_coloured_string(qfloat_t x, char *out, size_t out_size);
void qc_to_coloured_string(qcomplex_t z, char *out, size_t out_size);
void print_md(const char *label, matrix_t *A);
void print_mnum(const char *label, matrix_t *A);
void print_mdv(const char *label, matrix_t *A);
void print_mqc(const char *label, matrix_t *A);
void print_matrix_working_precision_line(const char *label, const matrix_t *A);
void print_current_input_matrix(void);
void check_d(const char *label, double got, double expected, double tol);
void check_qf_val(const char *label, qfloat_t got, qfloat_t expected, double tol);
void check_qc_val(const char *label, qcomplex_t got, qcomplex_t expected, double tol);
void check_bool(const char *label, int cond);
void clear_matrix_input_context(void);
extern char current_matrix_input_label[128];

matrix_t *test_mat_create_d(size_t rows, size_t cols, const double *values);
matrix_t *test_mat_create_qf(size_t rows, size_t cols, const qfloat_t *values);
matrix_t *test_mat_create_qc(size_t rows, size_t cols, const qcomplex_t *values);

static inline matrix_t *test_mat_create_complex(size_t rows, size_t cols, const qcomplex_t *values)
{
    return test_mat_create_qc(rows, cols, values);
}

static inline matrix_t *matsq_new_num(size_t n)
{
    return mat_new(n, n);
}

static inline matrix_t *test_mat_square_expr(size_t n)
{
    return mat_new_expr(n, n);
}

static inline int test_mat_bindings_set_d(mat_bindings_t *bindings,
                                          const char *name,
                                          double value)
{
    expr_t *binding;
    number_t n;

    if (!bindings || !name)
        return -1;

    binding = mat_bindings_get(bindings, name);
    if (!binding)
        return -1;

    n = test_num_from_d(value);
    expr_set_val(binding, n);
    num_destroy(&n);
    return 0;
}

void run_matrix_core_tests(void);
void run_matrix_function_tests(void);
void run_matrix_function_regression_tests(void);
void run_matrix_fromstring_tests(void);
void run_matrix_tostring_tests(void);
void run_matrix_output_tests(void);

#define mat_new_num(rows, cols) \
    mat_new((rows), (cols))

#define mat_new_sparse_num(rows, cols) \
    mat_new_sparse((rows), (cols))

#define mat_create_identity_num(n) \
    mat_create_identity((n))

#define mat_create_diagonal_num(n, diagonal) \
    mat_create_diagonal((n), (diagonal))

#define mat_evaluate_num(A) \
    mat_evaluate((A))

#define mat_get_data_num(A, data) \
    test_mat_get_data_num_slot((A), (data))

#define mat_get(A, i, j, out) \
    _Generic((out), \
        number_t *: test_mat_get_num_slot, \
        expr_t **: test_mat_get_expr_slot, \
        double *: test_mat_get_d, \
        qfloat_t *: test_mat_get_mp_real, \
        qcomplex_t *: test_mat_get_complex \
    )((A), (i), (j), (out))

#define mat_set(A, i, j, val) \
    _Generic((val), \
        const number_t *: test_mat_set_num_slot, \
        number_t *: test_mat_set_num_slot, \
        expr_t *const *: test_mat_set_expr_slot, \
        expr_t **: test_mat_set_expr_slot, \
        const double *: test_mat_set_d, \
        double *: test_mat_set_d, \
        const qfloat_t *: test_mat_set_mp_real, \
        qfloat_t *: test_mat_set_mp_real, \
        const qcomplex_t *: test_mat_set_complex, \
        qcomplex_t *: test_mat_set_complex \
    )((A), (i), (j), (val))

#define mat_get_data(A, data) \
    _Generic((data), \
        number_t *: test_mat_get_data_num_slot, \
        expr_t **: test_mat_get_data_expr_slot, \
        double *: test_mat_get_data_d, \
        qfloat_t *: test_mat_get_data_mp_real, \
        qcomplex_t *: test_mat_get_data_complex \
    )((A), (data))

#define mat_trace(A, trace) \
    _Generic((trace), \
        number_t *: test_mat_trace_num_slot, \
        double *: test_mat_trace_d, \
        qfloat_t *: test_mat_trace_mp_real, \
        qcomplex_t *: test_mat_trace_complex, \
        expr_t **: mat_trace_expr \
    )((A), (trace))

#define mat_det(A, determinant) \
    _Generic((determinant), \
        number_t *: test_mat_det_num_slot, \
        double *: test_mat_det_d, \
        qfloat_t *: test_mat_det_mp_real, \
        qcomplex_t *: test_mat_det_complex, \
        expr_t **: test_mat_det_expr_slot \
    )((A), (determinant))

#define mat_eigenspace(A, eigenvalue) \
    _Generic((eigenvalue), \
        const number_t *: mat_eigenspace, \
        number_t *: mat_eigenspace, \
        const expr_t *: mat_eigenspace_expr, \
        expr_t **: test_mat_eigenspace_expr_slot, \
        expr_t *: mat_eigenspace_expr \
    )((A), (eigenvalue))

#define mat_generalized_eigenspace(A, eigenvalue, order) \
    _Generic((eigenvalue), \
        const number_t *: mat_generalized_eigenspace, \
        number_t *: mat_generalized_eigenspace, \
        const expr_t *: mat_generalized_eigenspace_expr, \
        expr_t **: test_mat_generalized_eigenspace_expr_slot, \
        expr_t *: mat_generalized_eigenspace_expr \
    )((A), (eigenvalue), (order))

#define mat_jordan_chain(A, eigenvalue, order) \
    _Generic((eigenvalue), \
        const number_t *: mat_jordan_chain, \
        number_t *: mat_jordan_chain, \
        const expr_t *: mat_jordan_chain_expr, \
        expr_t **: test_mat_jordan_chain_expr_slot, \
        expr_t *: mat_jordan_chain_expr \
    )((A), (eigenvalue), (order))

#define mat_jordan_profile(A, eigenvalue) \
    _Generic((eigenvalue), \
        const number_t *: mat_jordan_profile, \
        number_t *: mat_jordan_profile, \
        const expr_t *: mat_jordan_profile_expr, \
        expr_t **: test_mat_jordan_profile_expr_slot, \
        expr_t *: mat_jordan_profile_expr \
    )((A), (eigenvalue))

#endif
