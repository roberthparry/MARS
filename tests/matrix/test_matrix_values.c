/**
 * @file test_matrix_values.c
 * @brief Shared numeric adapters for matrix tests.
 *
 * Supplies value conversion and binding helpers that let matrix tests compare results across numeric
 * representations. These adapters belong to the test harness, not the public matrix API.
 *
 * Linked into the relevant test executables as shared support. Production code should not depend on this test-only
 * implementation.
 */

/* Shared matrix value adapters for the matrix suite. */
#include "test_matrix.h"

/* Call native accessors beneath the test header's compatibility dispatch. */
#undef mat_get
#undef mat_set
#undef mat_trace
#undef mat_det

/* Convert a number to a complex value and release temporary parts. */
qcomplex_t test_num_to_qcomplex(const number_t x)
{
    number_t re = num_real_part(x);
    number_t im = num_imag_part(x);
    qcomplex_t z = qc_make(num_to_qfloat(re), num_to_qfloat(im));

    num_destroy(&re);
    num_destroy(&im);
    return z;
}

/* Create a dense zero matrix using double-valued number slots. */
matrix_t *test_mat_dense_d(size_t rows, size_t cols)
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

/* Evaluate matrix entries into an owning numeric matrix. */
matrix_t *test_mat_evaluate_complex(const matrix_t *A)
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

/* Set a matrix entry from a double and release the temporary number. */
void test_mat_set_d(matrix_t *A, size_t i, size_t j, const double *value)
{
    number_t n = test_num_from_d(*value);
    mat_set(A, i, j, &n);
    num_destroy(&n);
}

/* Set a matrix entry from a qfloat and release the temporary number. */
void test_mat_set_mp_real(matrix_t *A, size_t i, size_t j, const qfloat_t *value)
{
    number_t n = test_num_from_mp_real(*value);
    mat_set(A, i, j, &n);
    num_destroy(&n);
}

/* Set a matrix entry from a complex value and release the temporary number. */
void test_mat_set_complex(matrix_t *A, size_t i, size_t j, const qcomplex_t *value)
{
    number_t n = test_num_from_complex(*value);
    mat_set(A, i, j, &n);
    num_destroy(&n);
}

/* Read an owning number when the output slot is available. */
void test_mat_get_num_slot(const matrix_t *A, size_t i, size_t j, number_t *out)
{
    if (!out)
        return;
    *out = mat_get_num(A, i, j);
}

/* Read the real part of a matrix entry as a double. */
void test_mat_get_d(const matrix_t *A, size_t i, size_t j, double *out)
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

/* Read the real part of a matrix entry as a qfloat. */
void test_mat_get_mp_real(const matrix_t *A, size_t i, size_t j, qfloat_t *out)
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

/* Read a matrix entry as a complex value. */
void test_mat_get_complex(const matrix_t *A, size_t i, size_t j, qcomplex_t *out)
{
    number_t n;

    if (!out)
        return;
    n = mat_get_num(A, i, j);
    *out = test_num_to_qcomplex(n);
    num_destroy(&n);
}

/* Read an expression when the output slot is available. */
void test_mat_get_expr_slot(const matrix_t *A, size_t i, size_t j, expr_t **out)
{
    if (!out)
        return;
    mat_get(A, i, j, out);
}

/* Set matrix data from doubles and release temporary numbers. */
void test_mat_set_data_d(matrix_t *A, const double *data)
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

/* Set matrix data from qfloats and release temporary numbers. */
void test_mat_set_data_mp_real(matrix_t *A, const qfloat_t *data)
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

/* Set matrix data from complex values and release temporary numbers. */
void test_mat_set_data_complex(matrix_t *A, const qcomplex_t *data)
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

/* Read matrix entries as doubles in row-major order. */
void test_mat_get_data_d(const matrix_t *A, double *data)
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

/* Read matrix entries as qfloats in row-major order. */
void test_mat_get_data_mp_real(const matrix_t *A, qfloat_t *data)
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

/* Read matrix entries as complex values in row-major order. */
void test_mat_get_data_complex(const matrix_t *A, qcomplex_t *data)
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

/* Create a diagonal matrix from doubles and release temporary numbers. */
matrix_t *test_mat_diagonal_d(size_t n, const double *diagonal)
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

/* Read a matrix trace as a double and release temporary numbers. */
int test_mat_trace_d(const matrix_t *A, double *trace)
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

/* Read a determinant as a double and release temporary numbers. */
int test_mat_det_d(const matrix_t *A, double *determinant)
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

/* Read a determinant as a qfloat and release temporary numbers. */
int test_mat_det_mp_real(const matrix_t *A, qfloat_t *determinant)
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

/* Read a determinant as a complex value and release the temporary number. */
int test_mat_det_complex(const matrix_t *A, qcomplex_t *determinant)
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

/* Resolve a supplied expression eigenvalue before finding its eigenspace. */
matrix_t *test_mat_eigenspace_expr_slot(const matrix_t *A, expr_t **eigenvalue)
{
    return (A && eigenvalue && *eigenvalue) ? mat_eigenspace_expr(A, *eigenvalue) : NULL;
}

/* Resolve a supplied expression eigenvalue before finding its generalised eigenspace. */
matrix_t *test_mat_generalized_eigenspace_expr_slot(const matrix_t *A, expr_t **eigenvalue, size_t order)
{
    return (A && eigenvalue && *eigenvalue) ? mat_generalized_eigenspace_expr(A, *eigenvalue, order) : NULL;
}

/* Resolve a supplied expression eigenvalue before finding its Jordan chain. */
matrix_t *test_mat_jordan_chain_expr_slot(const matrix_t *A, expr_t **eigenvalue, size_t order)
{
    return (A && eigenvalue && *eigenvalue) ? mat_jordan_chain_expr(A, *eigenvalue, order) : NULL;
}

/* Resolve a supplied expression eigenvalue before finding its Jordan profile. */
matrix_t *test_mat_jordan_profile_expr_slot(const matrix_t *A, expr_t **eigenvalue)
{
    return (A && eigenvalue && *eigenvalue) ? mat_jordan_profile_expr(A, *eigenvalue) : NULL;
}

/* Read a matrix trace as a qfloat and release temporary numbers. */
int test_mat_trace_mp_real(const matrix_t *A, qfloat_t *trace)
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

/* Read a matrix trace as a complex value and release the temporary number. */
int test_mat_trace_complex(const matrix_t *A, qcomplex_t *trace)
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

/* Update a named matrix binding using a temporary number. */
int test_mat_bindings_set_d(mat_bindings_t *bindings, const char *name, double value)
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
