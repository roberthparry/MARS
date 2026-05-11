#include <stdlib.h>

#include "matrix_internal.h"

/* ============================================================
   Public API constructors
   ============================================================ */

struct matrix_t *mat_new_num(size_t rows, size_t cols) {
    return mat_create_dense_with_elem(rows, cols, &number_elem);
}

struct matrix_t *mat_new_sparse_num(size_t rows, size_t cols) {
    return mat_create_sparse_with_elem(rows, cols, &number_elem);
}

struct matrix_t *mat_new_dv(size_t rows, size_t cols) {
    return mat_create_dense_with_elem(rows, cols, &dval_elem);
}

struct matrix_t *mat_new_sparse_dv(size_t rows, size_t cols) {
    return mat_create_sparse_with_elem(rows, cols, &dval_elem);
}

struct matrix_t *matsq_new_num(size_t n) {
    return mat_new_num(n, n);
}

struct matrix_t *mat_create_identity_num(size_t n) {
    return mat_create_identity_with_elem(n, &number_elem);
}

struct matrix_t *mat_create_identity_dv(size_t n) {
    return mat_create_identity_with_elem(n, &dval_elem);
}

static matrix_t *mat_create_diagonal_from_array(size_t n,
                                                const void *diagonal,
                                                const struct elem_vtable *elem)
{
    matrix_t *A;
    const unsigned char *cursor = diagonal;

    if (!diagonal)
        return NULL;

    A = mat_create_diagonal_with_elem(n, elem);
    if (!A)
        return NULL;

    for (size_t i = 0; i < n; i++) {
        mat_set(A, i, i, cursor);
        cursor += elem->size;
    }

    return A;
}

matrix_t *mat_create_diagonal_num(size_t n, const number_t *diagonal)
{
    return mat_create_diagonal_from_array(n, diagonal, &number_elem);
}

matrix_t *mat_create_diagonal_dv(size_t n, dval_t *const *diagonal)
{
    return mat_create_diagonal_from_array(n, diagonal, &dval_elem);
}

matrix_t *mat_create_num(size_t rows, size_t cols, const number_t *data)
{
    matrix_t *A = mat_new_num(rows, cols);
    mat_set_data(A, data);
    return A;
}

matrix_t *mat_create_dv(size_t rows, size_t cols, dval_t *const *data)
{
    matrix_t *A = mat_new_dv(rows, cols);
    mat_set_data(A, data);
    return A;
}

/* ============================================================
   Destruction and basic access
   ============================================================ */

void mat_free(struct matrix_t *A) {
    if (!A) return;
    mat_fun_cache_forget(A);
    A->store->free(A);
    free(A);
}

void mat_get(const struct matrix_t *A, size_t i, size_t j, void *out) {
    A->store->get(A, i, j, out);
}

number_t mat_get_num(const struct matrix_t *A, size_t i, size_t j)
{
    unsigned char raw[64] = {0};
    number_t out = num_new();
    qcomplex_t z = QC_ZERO;

    if (!A)
        return out;

    if (A->elem == &number_elem) {
        mat_get_owned(A, i, j, &out);
        return out;
    }

    if (A->elem == &dval_elem) {
        dval_t *dv = NULL;

        mat_get(A, i, j, &dv);
        if (dv)
            out = dv_eval_num(dv);
        return out;
    }

    mat_get(A, i, j, raw);
    if (A->elem->to_qc)
        A->elem->to_qc(&z, raw);
    out = num_create_from_qcomplex(z);
    return out;
}

void mat_set(struct matrix_t *A, size_t i, size_t j, const void *val) {
    A->store->set(A, i, j, val);
}

size_t mat_get_row_count(const struct matrix_t *A) {
    return A->rows;
}

size_t mat_get_col_count(const struct matrix_t *A) {
    return A->cols;
}

bool mat_is_sparse(const matrix_t *A)
{
    return A && A->store && A->store->is_sparse_storage &&
           A->store->is_sparse_storage(A);
}

bool mat_is_diagonal(const matrix_t *A)
{
    return mat_has_diagonal_structure(A);
}

bool mat_is_upper_triangular(const matrix_t *A)
{
    return mat_has_upper_triangular_structure(A);
}

bool mat_is_lower_triangular(const matrix_t *A)
{
    return mat_has_lower_triangular_structure(A);
}

size_t mat_nonzero_count(const matrix_t *A)
{
    if (!A)
        return 0;
    return A->store->nonzero_count ? A->store->nonzero_count(A) : 0;
}

mat_type_t mat_typeof(const matrix_t *A)
{
    return A->elem->public_type;
}

static inline void mat_copy_flat(matrix_t *A, void *data, void (*op)(matrix_t *A, size_t, size_t, void *))
{
    size_t elem_size = A->elem->size;
    char *cursor = (char *)data;

    for (size_t i = 0; i < A->rows; i++) {
        for (size_t j = 0; j < A->cols; j++) {
            op(A, i, j, cursor);
            cursor += elem_size;
        }
    }
}

void mat_set_data(matrix_t *A, const void *data)
{
    if (!A || !data)
        return;

    mat_copy_flat(A, (void *)data, (void (*)(matrix_t*,size_t,size_t,void*))mat_set);
}

void mat_get_data(const matrix_t *A, void *data)
{
    if (!A || !data)
        return;

    mat_copy_flat((matrix_t *)A, data, (void (*)(matrix_t*,size_t,size_t,void*))mat_get);
}

void mat_get_data_num(const matrix_t *A, number_t *data)
{
    size_t cursor = 0;

    if (!A || !data)
        return;

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++)
            data[cursor++] = mat_get_num(A, i, j);
}

matrix_t *mat_to_dense(const matrix_t *A)
{
    return mat_convert_dense(A, A ? A->elem : NULL);
}

matrix_t *mat_to_sparse(const matrix_t *A)
{
    matrix_t *S;
    unsigned char raw[64] = {0};

    if (!A)
        return NULL;

    S = mat_create_sparse_with_elem(A->rows, A->cols, A->elem);

    if (!S)
        return NULL;

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, raw);
            if (!elem_is_structural_zero(A->elem, raw))
                mat_set(S, i, j, raw);
        }

    return S;
}
