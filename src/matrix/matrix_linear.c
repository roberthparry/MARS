#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"
#include "matrix_vtable_defs.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

static bool mat_uses_sparse_like_storage(const matrix_t *A)
{
    return A && A->store && A->store->is_sparse_like && A->store->is_sparse_like(A);
}

matrix_t *mat_copy_with_store(const matrix_t *A, const struct store_vtable *store)
{
    matrix_t *C;
    unsigned char raw[MATRIX_SCALAR_STORAGE_BYTES] = {0};

    if (!A || !A->elem || !store)
        return NULL;

    C = mat_create_with_store(A->rows, A->cols, A->elem, store);
    if (!C)
        return NULL;

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, raw);
            mat_set(C, i, j, raw);
        }

    return C;
}

matrix_t *mat_copy_preserving_store(const matrix_t *A)
{
    if (!A)
        return NULL;

    return mat_copy_with_store(A, A->store);
}

matrix_t *mat_copy_as_dense(const matrix_t *A)
{
    return mat_copy_with_store(A, &dense_store);
}

matrix_t *mat_convert_with_store(const matrix_t *A, const struct elem_vtable *target, const struct store_vtable *store)
{
    matrix_t *C;
    unsigned char src[MATRIX_SCALAR_STORAGE_BYTES], dst[MATRIX_SCALAR_STORAGE_BYTES];

    if (!A || !target || !store)
        return NULL;
    if (A->elem == target)
        return mat_copy_with_store(A, store);

    C = mat_create_with_store(A->rows, A->cols, target, store);
    if (!C)
        return NULL;

    mat_value_init_zero(A, src);
    elem_init_zero_value(target, dst);

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            number_t value;
            mat_value_destroy(A, src);
            elem_destroy_value(target, dst);
            mat_value_init_zero(A, src);
            elem_init_zero_value(target, dst);
            mat_get_owned(A, i, j, src);
            value = mat_raw_value_to_number(A->elem, src);
            mat_raw_value_from_number(target, dst, &value);
            num_destroy(&value);
            mat_set(C, i, j, dst);
            elem_destroy_value(target, dst);
        }

    mat_value_destroy(A, src);
    elem_destroy_value(target, dst);
    return C;
}

matrix_t *mat_convert_dense(const matrix_t *A, const struct elem_vtable *target)
{
    return mat_convert_with_store(A, target, &dense_store);
}

matrix_t *mat_convert_preserving_store(const matrix_t *A, const struct elem_vtable *target)
{
    if (!A || !target)
        return NULL;

    return mat_convert_with_store(A, target, A->store);
}

const struct store_vtable *mat_sparse_factor_store(const matrix_t *A, const struct store_vtable *structured_store)
{
    if (!structured_store)
        return NULL;

    return mat_uses_sparse_like_storage(A) ? &sparse_store : structured_store;
}

static void mat_swap_rows(matrix_t *A, size_t r1, size_t r2)
{
    if (!A || !A->store || !A->store->swap_rows)
        return;

    A->store->swap_rows(A, r1, r2);
}

static void mat_row_eliminate_from(matrix_t *A, size_t dst_row, size_t src_row, size_t col_start, const void *factor)
{
    if (!A || !A->store || !A->store->row_eliminate_from)
        return;

    A->store->row_eliminate_from(A, dst_row, src_row, col_start, factor);
}

static number_t mat_elem_abs2_num(const struct elem_vtable *elem, const void *raw)
{
    NUM_SCOPE(scope);
    number_t value = mat_raw_value_to_number(elem, raw);
    number_t mag = num_abs(value);
    number_t abs2 = num_mul(mag, mag);

    return num_scope_detach(abs2);
}

static bool mat_elem_abs2_below(const struct elem_vtable *elem, const void *raw, const number_t *threshold)
{
    number_t abs2 = mat_elem_abs2_num(elem, raw);
    bool below = num_cmp(abs2, *threshold) < 0;

    num_destroy(&abs2);
    return below;
}

static size_t mat_find_pivot_row(const matrix_t *A, size_t col, size_t start)
{
    const struct elem_vtable *e = A->elem;
    unsigned char v[MATRIX_SCALAR_STORAGE_BYTES];
    size_t best = start;
    number_t best_abs2 = NUM_ZERO;
    bool have_best = false;

    for (size_t i = start; i < A->rows; i++) {
        number_t abs2;

        mat_get(A, i, col, v);
        abs2 = mat_elem_abs2_num(e, v);
        if (!have_best || num_cmp(abs2, best_abs2) > 0) {
            if (have_best)
                num_destroy(&best_abs2);
            best_abs2 = abs2;
            have_best = true;
            best = i;
        } else {
            num_destroy(&abs2);
        }
    }

    if (have_best)
        num_destroy(&best_abs2);
    return best;
}

static matrix_t *mat_apply_row_permutation(const matrix_t *P, const matrix_t *B, const struct elem_vtable *elem)
{
    matrix_t *PB;
    unsigned char pivot[MATRIX_SCALAR_STORAGE_BYTES], value[MATRIX_SCALAR_STORAGE_BYTES];

    if (!P || !B || !elem || P->rows != B->rows)
        return NULL;

    PB = mat_create_elementwise_unary_result(B->rows, B->cols, elem, B);
    if (!PB)
        return NULL;

    for (size_t i = 0; i < P->rows; i++) {
        size_t src_row = P->cols;

        for (size_t j = 0; j < P->cols; j++) {
            mat_get(P, i, j, pivot);
            if (elem->cmp(pivot, elem->zero) != 0) {
                src_row = j;
                break;
            }
        }

        if (src_row >= P->cols) {
            mat_free(PB);
            return NULL;
        }

        for (size_t j = 0; j < B->cols; j++) {
            mat_get(B, src_row, j, value);
            mat_set(PB, i, j, value);
        }
    }

    return PB;
}

matrix_t *mat_create_direct_solve_result(const matrix_t *A, const matrix_t *B, const struct elem_vtable *elem)
{
    const struct store_vtable *store = &dense_store;

    if (!A || !B || !elem)
        return NULL;

    if (mat_has_diagonal_structure(A) && B->store && B->store->elementwise_unary_store)
        store = B->store->elementwise_unary_store(B);

    return mat_create_with_store(A->cols, B->cols, elem, store);
}

static matrix_t *mat_solve_diagonal(const matrix_t *A, const matrix_t *B, const struct elem_vtable *elem)
{
    matrix_t *X;
    unsigned char diag[MATRIX_SCALAR_STORAGE_BYTES], inv_diag[MATRIX_SCALAR_STORAGE_BYTES],
        rhs[MATRIX_SCALAR_STORAGE_BYTES], out[MATRIX_SCALAR_STORAGE_BYTES];
    number_t near_zero_tol = num_create_from_double(1e-300);

    X = mat_create_direct_solve_result(A, B, elem);
    if (!X)
        return NULL;

    elem_init_zero_value(elem, diag);
    elem_init_zero_value(elem, inv_diag);
    elem_init_zero_value(elem, rhs);
    elem_init_zero_value(elem, out);

    for (size_t i = 0; i < A->rows; i++) {
        elem_destroy_value(elem, diag);
        elem_init_zero_value(elem, diag);
        mat_get_owned(A, i, i, diag);
        if (mat_elem_abs2_below(elem, diag, &near_zero_tol)) {
            elem_destroy_value(elem, diag);
            elem_destroy_value(elem, inv_diag);
            elem_destroy_value(elem, rhs);
            elem_destroy_value(elem, out);
            num_destroy(&near_zero_tol);
            mat_free(X);
            return NULL;
        }

        elem_destroy_value(elem, inv_diag);
        elem_init_zero_value(elem, inv_diag);
        elem->inv(inv_diag, diag);
        for (size_t j = 0; j < B->cols; j++) {
            elem_destroy_value(elem, rhs);
            elem_destroy_value(elem, out);
            elem_init_zero_value(elem, rhs);
            elem_init_zero_value(elem, out);
            mat_get_owned(B, i, j, rhs);
            elem->mul(out, inv_diag, rhs);
            mat_set(X, i, j, out);
        }
    }

    elem_destroy_value(elem, diag);
    elem_destroy_value(elem, inv_diag);
    elem_destroy_value(elem, rhs);
    elem_destroy_value(elem, out);
    num_destroy(&near_zero_tol);
    return X;
}

static matrix_t *mat_forward_substitute(const matrix_t *L, const matrix_t *B, const struct elem_vtable *elem)
{
    matrix_t *X;
    unsigned char diag[MATRIX_SCALAR_STORAGE_BYTES], inv_diag[MATRIX_SCALAR_STORAGE_BYTES],
        sum[MATRIX_SCALAR_STORAGE_BYTES], a[MATRIX_SCALAR_STORAGE_BYTES], b[MATRIX_SCALAR_STORAGE_BYTES],
        prod[MATRIX_SCALAR_STORAGE_BYTES], out[MATRIX_SCALAR_STORAGE_BYTES];
    number_t near_zero_tol = num_create_from_double(1e-300);

    X = mat_create_dense_with_elem(L->cols, B->cols, elem);
    if (!X)
        return NULL;

    elem_init_zero_value(elem, diag);
    elem_init_zero_value(elem, inv_diag);
    elem_init_zero_value(elem, sum);
    elem_init_zero_value(elem, a);
    elem_init_zero_value(elem, b);
    elem_init_zero_value(elem, prod);
    elem_init_zero_value(elem, out);

    for (size_t i = 0; i < L->rows; i++) {
        elem_destroy_value(elem, diag);
        elem_init_zero_value(elem, diag);
        mat_get_owned(L, i, i, diag);
        if (mat_elem_abs2_below(elem, diag, &near_zero_tol)) {
            elem_destroy_value(elem, diag);
            elem_destroy_value(elem, inv_diag);
            elem_destroy_value(elem, sum);
            elem_destroy_value(elem, a);
            elem_destroy_value(elem, b);
            elem_destroy_value(elem, prod);
            elem_destroy_value(elem, out);
            num_destroy(&near_zero_tol);
            mat_free(X);
            return NULL;
        }

        elem_destroy_value(elem, inv_diag);
        elem_init_zero_value(elem, inv_diag);
        elem->inv(inv_diag, diag);
        for (size_t j = 0; j < B->cols; j++) {
            elem_destroy_value(elem, sum);
            elem_destroy_value(elem, out);
            elem_init_zero_value(elem, sum);
            elem_init_zero_value(elem, out);
            mat_get_owned(B, i, j, sum);
            for (size_t k = 0; k < i; k++) {
                elem_destroy_value(elem, a);
                elem_destroy_value(elem, b);
                elem_destroy_value(elem, prod);
                elem_init_zero_value(elem, a);
                elem_init_zero_value(elem, b);
                elem_init_zero_value(elem, prod);
                mat_get_owned(L, i, k, a);
                mat_get_owned(X, k, j, b);
                elem->mul(prod, a, b);
                elem->sub(sum, sum, prod);
            }
            elem->mul(out, inv_diag, sum);
            mat_set(X, i, j, out);
        }
    }

    elem_destroy_value(elem, diag);
    elem_destroy_value(elem, inv_diag);
    elem_destroy_value(elem, sum);
    elem_destroy_value(elem, a);
    elem_destroy_value(elem, b);
    elem_destroy_value(elem, prod);
    elem_destroy_value(elem, out);
    num_destroy(&near_zero_tol);
    return X;
}

static matrix_t *mat_backward_substitute(const matrix_t *U, const matrix_t *B, const struct elem_vtable *elem)
{
    matrix_t *X;
    unsigned char diag[MATRIX_SCALAR_STORAGE_BYTES], inv_diag[MATRIX_SCALAR_STORAGE_BYTES],
        sum[MATRIX_SCALAR_STORAGE_BYTES], a[MATRIX_SCALAR_STORAGE_BYTES], b[MATRIX_SCALAR_STORAGE_BYTES],
        prod[MATRIX_SCALAR_STORAGE_BYTES], out[MATRIX_SCALAR_STORAGE_BYTES];
    number_t near_zero_tol = num_create_from_double(1e-300);

    X = mat_create_dense_with_elem(U->cols, B->cols, elem);
    if (!X)
        return NULL;

    elem_init_zero_value(elem, diag);
    elem_init_zero_value(elem, inv_diag);
    elem_init_zero_value(elem, sum);
    elem_init_zero_value(elem, a);
    elem_init_zero_value(elem, b);
    elem_init_zero_value(elem, prod);
    elem_init_zero_value(elem, out);

    for (size_t ii = U->rows; ii-- > 0;) {
        elem_destroy_value(elem, diag);
        elem_init_zero_value(elem, diag);
        mat_get_owned(U, ii, ii, diag);
        if (mat_elem_abs2_below(elem, diag, &near_zero_tol)) {
            elem_destroy_value(elem, diag);
            elem_destroy_value(elem, inv_diag);
            elem_destroy_value(elem, sum);
            elem_destroy_value(elem, a);
            elem_destroy_value(elem, b);
            elem_destroy_value(elem, prod);
            elem_destroy_value(elem, out);
            num_destroy(&near_zero_tol);
            mat_free(X);
            return NULL;
        }

        elem_destroy_value(elem, inv_diag);
        elem_init_zero_value(elem, inv_diag);
        elem->inv(inv_diag, diag);
        for (size_t j = 0; j < B->cols; j++) {
            elem_destroy_value(elem, sum);
            elem_destroy_value(elem, out);
            elem_init_zero_value(elem, sum);
            elem_init_zero_value(elem, out);
            mat_get_owned(B, ii, j, sum);
            for (size_t k = ii + 1; k < U->cols; k++) {
                elem_destroy_value(elem, a);
                elem_destroy_value(elem, b);
                elem_destroy_value(elem, prod);
                elem_init_zero_value(elem, a);
                elem_init_zero_value(elem, b);
                elem_init_zero_value(elem, prod);
                mat_get_owned(U, ii, k, a);
                mat_get_owned(X, k, j, b);
                elem->mul(prod, a, b);
                elem->sub(sum, sum, prod);
            }
            elem->mul(out, inv_diag, sum);
            mat_set(X, ii, j, out);
        }
    }

    elem_destroy_value(elem, diag);
    elem_destroy_value(elem, inv_diag);
    elem_destroy_value(elem, sum);
    elem_destroy_value(elem, a);
    elem_destroy_value(elem, b);
    elem_destroy_value(elem, prod);
    elem_destroy_value(elem, out);
    num_destroy(&near_zero_tol);
    return X;
}

struct matrix_t *mat_transpose(const struct matrix_t *A)
{
    const struct elem_vtable *e;
    struct matrix_t *T;
    unsigned char v[MATRIX_SCALAR_STORAGE_BYTES];

    if (!A)
        return NULL;

    e = elem_of(A);
    T = mat_create_transpose_result(A->cols, A->rows, e, A);
    if (!T)
        return NULL;

    mat_value_init_zero(A, v);

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_value_destroy(A, v);
            mat_value_init_zero(A, v);
            mat_get_owned(A, i, j, v);
            mat_set(T, j, i, v);
        }

    mat_value_destroy(A, v);
    return T;
}

struct matrix_t *mat_conj(const struct matrix_t *A)
{
    const struct elem_vtable *e;
    struct matrix_t *C;
    unsigned char v[MATRIX_SCALAR_STORAGE_BYTES], cv[MATRIX_SCALAR_STORAGE_BYTES];

    if (!A)
        return NULL;

    e = elem_of(A);
    C = mat_create_elementwise_unary_result(A->rows, A->cols, e, A);
    if (!C)
        return NULL;

    mat_value_init_zero(A, v);
    mat_value_init_zero(C, cv);

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_value_destroy(A, v);
            mat_value_destroy(C, cv);
            mat_value_init_zero(A, v);
            mat_value_init_zero(C, cv);
            mat_get_owned(A, i, j, v);
            e->conj_elem(cv, v);
            mat_set(C, i, j, cv);
            elem_destroy_value(e, cv);
        }

    mat_value_destroy(A, v);
    mat_value_destroy(C, cv);
    return C;
}

matrix_t *mat_hermitian(const matrix_t *A)
{
    matrix_t *H;
    matrix_t *T;

    if (!A)
        return NULL;

    T = mat_transpose(A);
    if (!T)
        return NULL;

    H = mat_conj(T);
    mat_free(T);
    return H;
}

int mat_trace_expr(const matrix_t *A, expr_t **trace)
{
    if (!A || !trace)
        return -1;
    if (A->rows != A->cols)
        return -2;
    if (A->elem != &expr_elem)
        return -3;
    expr_t *sum = expr_new_const(NUM_ZERO);

    if (!sum)
        return -3;

    for (size_t i = 0; i < A->rows; ++i) {
        expr_t *term = NULL;
        expr_t *tmp = NULL;

        mat_get(A, i, i, &term);
        if (!term)
            term = (expr_t *)EXPR_ZERO;

        tmp = expr_add(sum, term);
        expr_free(sum);
        sum = tmp;
        if (!sum)
            return -3;
    }

    *trace = expr_simplify_owned(sum);
    return *trace ? 0 : -3;
}

int mat_trace(const matrix_t *A, number_t *trace)
{
    if (!A || !trace)
        return -1;
    if (A->rows != A->cols)
        return -2;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -3;

    *trace = num_clone(NUM_ZERO);
    for (size_t i = 0; i < A->rows; ++i) {
        NUM_SCOPE(iter_scope);
        number_t term = mat_get_num(A, i, i);
        number_t next = num_add(*trace, term);
        num_destroy(trace);
        *trace = num_scope_detach(next);
    }

    return 0;
}

int mat_det_expr(const matrix_t *A, expr_t **determinant)
{
    if (!A || !determinant)
        return -1;
    if (A->rows != A->cols)
        return -2;
    if (A->elem != &expr_elem)
        return -3;

    return mat_det_expr_exact(A, determinant);
}

int mat_det(const matrix_t *A, number_t *determinant)
{
    size_t n;
    const struct elem_vtable *e;
    matrix_t *M;
    unsigned char *val;
    unsigned char *pivot;
    unsigned char *inv_pivot;
    unsigned char *factor;
    unsigned char *a;
    unsigned char *b;
    unsigned char *prod;
    unsigned char *tmp;
    unsigned char *det;

    if (!A || !determinant)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -3;
    if (A->rows != A->cols)
        return -2;

    n = A->rows;
    e = A->elem;
    if (n == 1) {
        *determinant = mat_get_num(A, 0, 0);
        return 0;
    }

    M = mat_create_dense_with_elem(A->rows, A->cols, e);
    if (!M)
        return -3;

    val = calloc(1u, e->size ? e->size : 1u);
    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get_owned(A, i, j, val);
            mat_set(M, i, j, val);
            elem_destroy_value(e, val);
            elem_init_zero_value(e, val);
        }

    pivot = calloc(1u, e->size ? e->size : 1u);
    inv_pivot = calloc(1u, e->size ? e->size : 1u);
    factor = calloc(1u, e->size ? e->size : 1u);
    a = calloc(1u, e->size ? e->size : 1u);
    b = calloc(1u, e->size ? e->size : 1u);
    prod = calloc(1u, e->size ? e->size : 1u);
    tmp = calloc(1u, e->size ? e->size : 1u);
    det = calloc(1u, e->size ? e->size : 1u);

    if (!val || !pivot || !inv_pivot || !factor || !a || !b || !prod || !tmp || !det) {
        free(val);
        free(pivot);
        free(inv_pivot);
        free(factor);
        free(a);
        free(b);
        free(prod);
        free(tmp);
        free(det);
        mat_free(M);
        return -3;
    }

    elem_copy_value(e, det, e->one);

    for (size_t k = 0; k < n; k++) {
        mat_get_owned(M, k, k, pivot);
        if (elem_is_structural_zero(e, pivot)) {
            *determinant = num_clone(NUM_ZERO);
            elem_destroy_value(e, pivot);
            elem_destroy_value(e, inv_pivot);
            elem_destroy_value(e, factor);
            elem_destroy_value(e, a);
            elem_destroy_value(e, b);
            elem_destroy_value(e, prod);
            elem_destroy_value(e, tmp);
            elem_destroy_value(e, det);
            free(val);
            free(pivot);
            free(inv_pivot);
            free(factor);
            free(a);
            free(b);
            free(prod);
            free(tmp);
            free(det);
            mat_free(M);
            return 0;
        }

        e->inv(inv_pivot, pivot);
        elem_destroy_value(e, pivot);
        elem_init_zero_value(e, pivot);

        for (size_t i = k + 1; i < n; i++) {
            mat_get_owned(M, i, k, factor);
            e->mul(factor, inv_pivot, factor);

            for (size_t j = k; j < n; j++) {
                mat_get_owned(M, i, j, a);
                mat_get_owned(M, k, j, b);
                e->mul(prod, factor, b);
                e->sub(tmp, a, prod);
                mat_set(M, i, j, tmp);
                elem_destroy_value(e, a);
                elem_destroy_value(e, b);
                elem_destroy_value(e, prod);
                elem_destroy_value(e, tmp);
                elem_init_zero_value(e, a);
                elem_init_zero_value(e, b);
                elem_init_zero_value(e, prod);
                elem_init_zero_value(e, tmp);
            }
            elem_destroy_value(e, factor);
            elem_init_zero_value(e, factor);
        }
        elem_destroy_value(e, inv_pivot);
        elem_init_zero_value(e, inv_pivot);
    }

    for (size_t i = 0; i < n; i++) {
        mat_get_owned(M, i, i, a);
        e->mul(det, det, a);
        elem_destroy_value(e, a);
        elem_init_zero_value(e, a);
    }

    *determinant = num_clone(*(number_t *)det);
    elem_destroy_value(e, val);
    elem_destroy_value(e, pivot);
    elem_destroy_value(e, inv_pivot);
    elem_destroy_value(e, factor);
    elem_destroy_value(e, a);
    elem_destroy_value(e, b);
    elem_destroy_value(e, prod);
    elem_destroy_value(e, tmp);
    elem_destroy_value(e, det);
    free(val);
    free(pivot);
    free(inv_pivot);
    free(factor);
    free(a);
    free(b);
    free(prod);
    free(tmp);
    free(det);
    mat_free(M);
    return 0;
}

matrix_t *mat_adjugate(const matrix_t *A)
{
    return mat_adjugate_exact(A);
}

matrix_t *mat_inverse(const matrix_t *A)
{
    size_t n;
    const struct elem_vtable *e;
    matrix_t *M;
    matrix_t *I;
    unsigned char *v;
    unsigned char *pivot;
    unsigned char *inv_pivot;
    unsigned char *factor;
    unsigned char *a;
    unsigned char *b;
    unsigned char *prod;
    unsigned char *tmp;

    if (!A)
        return NULL;
    if (A->rows != A->cols)
        return NULL;
    if (matrix_is_symbolic(A))
        return mat_inverse_expr_exact(A);
    if (!elem_supports_numeric_algorithms(A->elem))
        return NULL;

    n = A->rows;
    e = A->elem;
    M = mat_create_dense_with_elem(n, n, e);
    I = mat_create_identity_with_elem(n, e);
    if (!M || !I) {
        if (M)
            mat_free(M);
        if (I)
            mat_free(I);
        return NULL;
    }

    v = calloc(1u, e->size ? e->size : 1u);
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < n; j++) {
            mat_get_owned(A, i, j, v);
            mat_set(M, i, j, v);
            elem_destroy_value(e, v);
            elem_init_zero_value(e, v);
        }

    pivot = calloc(1u, e->size ? e->size : 1u);
    inv_pivot = calloc(1u, e->size ? e->size : 1u);
    factor = calloc(1u, e->size ? e->size : 1u);
    a = calloc(1u, e->size ? e->size : 1u);
    b = calloc(1u, e->size ? e->size : 1u);
    prod = calloc(1u, e->size ? e->size : 1u);
    tmp = calloc(1u, e->size ? e->size : 1u);

    if (!v || !pivot || !inv_pivot || !factor || !a || !b || !prod || !tmp) {
        free(v);
        free(pivot);
        free(inv_pivot);
        free(factor);
        free(a);
        free(b);
        free(prod);
        free(tmp);
        mat_free(M);
        mat_free(I);
        return NULL;
    }

    for (size_t k = 0; k < n; k++) {
        mat_get_owned(M, k, k, pivot);
        if (elem_is_structural_zero(e, pivot)) {
            elem_destroy_value(e, v);
            elem_destroy_value(e, pivot);
            elem_destroy_value(e, inv_pivot);
            elem_destroy_value(e, factor);
            elem_destroy_value(e, a);
            elem_destroy_value(e, b);
            elem_destroy_value(e, prod);
            elem_destroy_value(e, tmp);
            free(v);
            free(pivot);
            free(inv_pivot);
            free(factor);
            free(a);
            free(b);
            free(prod);
            free(tmp);
            mat_free(M);
            mat_free(I);
            return NULL;
        }

        e->inv(inv_pivot, pivot);
        elem_destroy_value(e, pivot);
        elem_init_zero_value(e, pivot);

        for (size_t j = 0; j < n; j++) {
            mat_get_owned(M, k, j, a);
            e->mul(a, inv_pivot, a);
            mat_set(M, k, j, a);
            elem_destroy_value(e, a);
            elem_init_zero_value(e, a);

            mat_get_owned(I, k, j, b);
            e->mul(b, inv_pivot, b);
            mat_set(I, k, j, b);
            elem_destroy_value(e, b);
            elem_init_zero_value(e, b);
        }

        for (size_t i = 0; i < n; i++) {
            if (i == k)
                continue;

            mat_get_owned(M, i, k, factor);
            if (elem_is_structural_zero(e, factor)) {
                elem_destroy_value(e, factor);
                elem_init_zero_value(e, factor);
                continue;
            }

            for (size_t j = 0; j < n; j++) {
                mat_get_owned(M, i, j, a);
                mat_get_owned(M, k, j, b);
                e->mul(prod, factor, b);
                e->sub(tmp, a, prod);
                mat_set(M, i, j, tmp);
                elem_destroy_value(e, a);
                elem_destroy_value(e, b);
                elem_destroy_value(e, prod);
                elem_destroy_value(e, tmp);
                elem_init_zero_value(e, a);
                elem_init_zero_value(e, b);
                elem_init_zero_value(e, prod);
                elem_init_zero_value(e, tmp);

                mat_get_owned(I, i, j, a);
                mat_get_owned(I, k, j, b);
                e->mul(prod, factor, b);
                e->sub(tmp, a, prod);
                mat_set(I, i, j, tmp);
                elem_destroy_value(e, a);
                elem_destroy_value(e, b);
                elem_destroy_value(e, prod);
                elem_destroy_value(e, tmp);
                elem_init_zero_value(e, a);
                elem_init_zero_value(e, b);
                elem_init_zero_value(e, prod);
                elem_init_zero_value(e, tmp);
            }
            elem_destroy_value(e, factor);
            elem_init_zero_value(e, factor);
        }
        elem_destroy_value(e, inv_pivot);
        elem_init_zero_value(e, inv_pivot);
    }

    elem_destroy_value(e, v);
    elem_destroy_value(e, pivot);
    elem_destroy_value(e, inv_pivot);
    elem_destroy_value(e, factor);
    elem_destroy_value(e, a);
    elem_destroy_value(e, b);
    elem_destroy_value(e, prod);
    elem_destroy_value(e, tmp);
    free(v);
    free(pivot);
    free(inv_pivot);
    free(factor);
    free(a);
    free(b);
    free(prod);
    free(tmp);
    mat_free(M);
    return I;
}

matrix_t *mat_solve(const matrix_t *A, const matrix_t *B)
{
    mat_lu_factor_t lu = {0};
    const struct elem_vtable *e;
    matrix_t *Ac = NULL, *Bc = NULL, *PB = NULL, *Y = NULL, *X = NULL;

    if (!A || !B || A->rows != A->cols || A->rows != B->rows)
        return NULL;
    if (matrix_is_symbolic(A) || matrix_is_symbolic(B)) {
        Ac = mat_convert_preserving_store(A, &expr_elem);
        Bc = mat_convert_preserving_store(B, &expr_elem);
        if (!Ac || !Bc)
            goto fail;
        X = mat_solve_expr_exact(Ac, Bc);
        mat_free(Ac);
        mat_free(Bc);
        return X;
    }
    if (!elem_supports_numeric_algorithms(A->elem) || !elem_supports_numeric_algorithms(B->elem))
        return NULL;

    e = mat_binary_result_elem(A, B);
    if (!e)
        return NULL;

    Ac = mat_convert_preserving_store(A, e);
    Bc = mat_convert_preserving_store(B, e);
    if (!Ac || !Bc)
        goto fail;

    if (mat_has_diagonal_structure(Ac)) {
        X = mat_solve_diagonal(Ac, Bc, e);
        mat_free(Ac);
        mat_free(Bc);
        return X;
    }

    if (mat_has_lower_triangular_structure(Ac)) {
        X = mat_forward_substitute(Ac, Bc, e);
        mat_free(Ac);
        mat_free(Bc);
        return X;
    }

    if (mat_has_upper_triangular_structure(Ac)) {
        X = mat_backward_substitute(Ac, Bc, e);
        mat_free(Ac);
        mat_free(Bc);
        return X;
    }

    if (mat_lu_factor(Ac, &lu) != 0)
        goto fail;

    PB = mat_apply_row_permutation(lu.P, Bc, e);
    if (!PB)
        goto fail;

    Y = mat_forward_substitute(lu.L, PB, e);
    if (!Y)
        goto fail;

    X = mat_backward_substitute(lu.U, Y, e);
    if (!X)
        goto fail;

    mat_free(Ac);
    mat_free(Bc);
    mat_free(PB);
    mat_free(Y);
    mat_lu_factor_free(&lu);
    return X;

fail:
    mat_free(Ac);
    mat_free(Bc);
    mat_free(PB);
    mat_free(Y);
    mat_free(X);
    mat_lu_factor_free(&lu);
    return NULL;
}

matrix_t *mat_least_squares(const matrix_t *A, const matrix_t *B)
{
    matrix_t *A_pinv = NULL, *Bn = NULL, *X = NULL;

    if (!A || !B || A->rows != B->rows)
        return NULL;
    if (matrix_is_symbolic(A) && matrix_is_symbolic(B)) {
        A_pinv = mat_pseudoinverse_expr_exact(A);
        X = A_pinv ? mat_mul(A_pinv, B) : NULL;
        mat_free(A_pinv);
        return mat_finalize_symbolic_result(X);
    }
    if (!elem_supports_numeric_algorithms(A->elem) || !elem_supports_numeric_algorithms(B->elem))
        return NULL;

    if (A->rows == A->cols)
        return mat_solve(A, B);

    A_pinv = mat_pseudoinverse(A);
    Bn = mat_evaluate(B);
    X = (A_pinv && Bn) ? mat_mul(A_pinv, Bn) : NULL;

    mat_free(A_pinv);
    mat_free(Bn);
    return X;
}

void mat_lu_factor_free(mat_lu_factor_t *out)
{
    if (!out)
        return;
    mat_free(out->P);
    mat_free(out->L);
    mat_free(out->U);
    out->P = out->L = out->U = NULL;
}

int mat_lu_factor(const matrix_t *A, mat_lu_factor_t *out)
{
    const struct elem_vtable *e;
    const struct store_vtable *permutation_store;
    const struct store_vtable *lower_store;
    const struct store_vtable *upper_store;
    const struct store_vtable *working_lower_store;
    matrix_t *P_seed = NULL, *P = NULL, *L = NULL, *U = NULL;
    matrix_t *L_out = NULL, *U_out = NULL;
    unsigned char pivot[MATRIX_SCALAR_STORAGE_BYTES], inv_pivot[MATRIX_SCALAR_STORAGE_BYTES],
        factor[MATRIX_SCALAR_STORAGE_BYTES];
    unsigned char a[MATRIX_SCALAR_STORAGE_BYTES], b[MATRIX_SCALAR_STORAGE_BYTES];
    number_t near_zero_tol = num_create_from_double(1e-300);

    if (!A || !out)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -3;
    if (A->rows != A->cols)
        return -2;

    e = A->elem;
    permutation_store = mat_sparse_factor_store(A, &identity_store);
    lower_store = mat_sparse_factor_store(A, &lower_triangular_store);
    upper_store = mat_sparse_factor_store(A, &upper_triangular_store);
    working_lower_store = mat_sparse_factor_store(A, &lower_triangular_store);
    P_seed = mat_create_identity_with_elem(A->rows, e);
    P = mat_convert_with_store(P_seed, e, permutation_store);
    L = mat_create_with_store(A->rows, A->rows, e, working_lower_store);
    U = mat_convert_preserving_store(A, e);
    mat_free(P_seed);
    if (!P || !L || !U) {
        mat_free(P);
        mat_free(L);
        mat_free(U);
        return -3;
    }

    elem_init_zero_value(e, pivot);
    elem_init_zero_value(e, inv_pivot);
    elem_init_zero_value(e, factor);
    elem_init_zero_value(e, a);
    elem_init_zero_value(e, b);

    for (size_t i = 0; i < A->rows; i++)
        mat_set(L, i, i, e->one);

    for (size_t k = 0; k < A->rows; k++) {
        size_t pivot_row = mat_find_pivot_row(U, k, k);
        elem_destroy_value(e, pivot);
        elem_init_zero_value(e, pivot);
        mat_get_owned(U, pivot_row, k, pivot);
        if (mat_elem_abs2_below(e, pivot, &near_zero_tol)) {
            elem_destroy_value(e, pivot);
            elem_destroy_value(e, inv_pivot);
            elem_destroy_value(e, factor);
            elem_destroy_value(e, a);
            elem_destroy_value(e, b);
            num_destroy(&near_zero_tol);
            mat_free(P);
            mat_free(L);
            mat_free(U);
            return -4;
        }

        if (pivot_row != k) {
            mat_swap_rows(U, k, pivot_row);
            mat_swap_rows(P, k, pivot_row);
            for (size_t j = 0; j < k; j++) {
                elem_destroy_value(e, a);
                elem_destroy_value(e, b);
                elem_init_zero_value(e, a);
                elem_init_zero_value(e, b);
                mat_get_owned(L, k, j, a);
                mat_get_owned(L, pivot_row, j, b);
                mat_set(L, k, j, b);
                mat_set(L, pivot_row, j, a);
            }
        }

        elem_destroy_value(e, pivot);
        elem_destroy_value(e, inv_pivot);
        elem_init_zero_value(e, pivot);
        elem_init_zero_value(e, inv_pivot);
        mat_get_owned(U, k, k, pivot);
        e->inv(inv_pivot, pivot);

        for (size_t i = k + 1; i < A->rows; i++) {
            elem_destroy_value(e, factor);
            elem_destroy_value(e, a);
            elem_init_zero_value(e, factor);
            elem_init_zero_value(e, a);
            mat_get_owned(U, i, k, factor);
            if (mat_elem_abs2_below(e, factor, &near_zero_tol)) {
                mat_set(L, i, k, e->zero);
                continue;
            }

            e->mul(a, factor, inv_pivot);
            mat_set(L, i, k, a);
            mat_set(U, i, k, e->zero);
            mat_row_eliminate_from(U, i, k, k + 1, a);
        }
    }

    L_out = mat_convert_with_store(L, e, lower_store);
    U_out = mat_convert_with_store(U, e, upper_store);
    mat_free(U);
    mat_free(L);
    if (!L_out || !U_out) {
        mat_free(P);
        mat_free(L_out);
        mat_free(U_out);
        elem_destroy_value(e, pivot);
        elem_destroy_value(e, inv_pivot);
        elem_destroy_value(e, factor);
        elem_destroy_value(e, a);
        elem_destroy_value(e, b);
        return -3;
    }

    elem_destroy_value(e, pivot);
    elem_destroy_value(e, inv_pivot);
    elem_destroy_value(e, factor);
    elem_destroy_value(e, a);
    elem_destroy_value(e, b);
    num_destroy(&near_zero_tol);
    out->P = P;
    out->L = L_out;
    out->U = U_out;
    return 0;
}
