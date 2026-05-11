#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "matrix_internal.h"
#include "matrix_vtable_defs.h"
#include "number.h"
#include "qfloat.h"
#include "qcomplex.h"
#include "matrix.h"
#include "internal/dval_internal.h"
#include "internal/number_internal.h"
#include "number/number_internal.h"

/* ============================================================
   Internal matrix construction helpers (forward declarations)
   ============================================================ */

 struct matrix_t *store_create_dense(size_t rows, size_t cols,
                                           const struct elem_vtable *elem);
 struct matrix_t *store_create_sparse(size_t rows, size_t cols,
                                            const struct elem_vtable *elem);
 struct matrix_t *store_create_identity(size_t rows, size_t cols,
                                              const struct elem_vtable *elem);
 struct matrix_t *store_create_diagonal(size_t rows, size_t cols,
                                              const struct elem_vtable *elem);
 struct matrix_t *store_create_upper_triangular(size_t rows, size_t cols,
                                                      const struct elem_vtable *elem);
 struct matrix_t *store_create_lower_triangular(size_t rows, size_t cols,
                                                      const struct elem_vtable *elem);
 bool dense_is_sparse_storage(const struct matrix_t *A);
 bool sparse_is_sparse_storage(const struct matrix_t *A);
 bool identity_is_sparse_storage(const struct matrix_t *A);
 bool diagonal_is_sparse_storage(const struct matrix_t *A);
 bool upper_triangular_is_sparse_storage(const struct matrix_t *A);
 bool lower_triangular_is_sparse_storage(const struct matrix_t *A);
 bool dense_is_sparse_like(const struct matrix_t *A);
 bool sparse_is_sparse_like(const struct matrix_t *A);
 bool identity_is_sparse_like(const struct matrix_t *A);
 bool diagonal_is_sparse_like(const struct matrix_t *A);
 bool upper_triangular_is_sparse_like(const struct matrix_t *A);
 bool lower_triangular_is_sparse_like(const struct matrix_t *A);
 bool store_true(const struct matrix_t *A);
static bool store_false(const struct matrix_t *A);
 bool dense_is_diagonal(const struct matrix_t *A);
 bool sparse_is_diagonal(const struct matrix_t *A);
 bool upper_triangular_is_diagonal(const struct matrix_t *A);
 bool lower_triangular_is_diagonal(const struct matrix_t *A);
 bool generic_is_upper_triangular(const struct matrix_t *A);
 bool generic_is_lower_triangular(const struct matrix_t *A);
 size_t dense_nonzero_count(const struct matrix_t *A);
 size_t sparse_nonzero_count(const struct matrix_t *A);
 size_t identity_nonzero_count(const struct matrix_t *A);
 size_t diagonal_nonzero_count(const struct matrix_t *A);
 size_t upper_triangular_nonzero_count(const struct matrix_t *A);
 size_t lower_triangular_nonzero_count(const struct matrix_t *A);
 const struct store_vtable *store_self_unary(const struct matrix_t *A);
 const struct store_vtable *identity_unary_store(const struct matrix_t *A);
 const struct store_vtable *store_self_transpose(const struct matrix_t *A);
 const struct store_vtable *upper_triangular_transpose_store(const struct matrix_t *A);
 const struct store_vtable *lower_triangular_transpose_store(const struct matrix_t *A);
struct matrix_t *mat_create_with_store(size_t rows, size_t cols,
                                       const struct elem_vtable *elem,
                                       const struct store_vtable *store);
struct matrix_t *mat_create_transpose_result(size_t rows, size_t cols,
                                             const struct elem_vtable *elem,
                                             const struct matrix_t *layout_src);
static struct matrix_t *mat_create_binary_result(size_t rows, size_t cols,
                                                 const struct elem_vtable *elem,
                                                 const struct matrix_t *A,
                                                 const struct matrix_t *B);
static bool mat_uses_sparse_storage(const struct matrix_t *A);
static bool mat_is_sparse_like(const struct matrix_t *A);
bool mat_has_diagonal_structure(const struct matrix_t *A);
bool mat_has_upper_triangular_structure(const struct matrix_t *A);
bool mat_has_lower_triangular_structure(const struct matrix_t *A);
matrix_t *mat_convert_dense(const matrix_t *A, const struct elem_vtable *target);

/* ============================================================
   Storage vtables (dense, identity)
   ============================================================ */

typedef struct sparse_entry_t {
    size_t col;
    struct sparse_entry_t *next;
    unsigned char value[];
} sparse_entry_t;

static bool dval_is_exact_zero(const dval_t *dv)
{
    return !dv || dv_is_exact_zero(dv);
}

dval_t *dval_clone_for_storage(const dval_t *dv)
{
    if (!dv)
        return NULL;
    if (dv == DV_ZERO || dv == DV_ONE) {
        number_t value = dv_get_val_num(dv);
        dval_t *clone = dv_new_const_num(value);

        num_destroy(&value);
        return clone;
    }
    dv_retain(dv);
    return (dval_t *)dv;
}

matrix_t *mat_finalize_symbolic_result(matrix_t *A)
{
    matrix_t *simplified;

    if (!A || A->elem != &dval_elem)
        return A;

    simplified = mat_simplify_symbolic(A);
    mat_free(A);
    return simplified;
}

void elem_init_zero_value(const struct elem_vtable *elem, void *slot)
{
    if (!elem || !slot)
        return;
    if (elem->init_zero_slot) {
        elem->init_zero_slot(slot);
        return;
    }
    memcpy(slot, elem->zero, elem->size);
}

void elem_copy_value(const struct elem_vtable *elem, void *dst, const void *src)
{
    if (!elem || !dst)
        return;
    if (elem->copy_value) {
        elem->copy_value(dst, src);
        return;
    }
    if (src)
        memcpy(dst, src, elem->size);
}

void elem_destroy_value(const struct elem_vtable *elem, void *slot)
{
    if (!elem || !slot)
        return;
    if (elem->destroy_value)
        elem->destroy_value(slot);
}

void elem_simplify_value(const struct elem_vtable *elem, void *slot)
{
    if (!elem || !slot)
        return;
    if (elem->simplify_value)
        elem->simplify_value(slot);
}

bool elem_is_structural_zero(const struct elem_vtable *elem, const void *val)
{
    if (!elem)
        return true;
    if (elem->is_structural_zero)
        return elem->is_structural_zero(val);
    return elem->cmp(val, elem->zero) == 0;
}

bool elem_supports_numeric_algorithms(const struct elem_vtable *elem)
{
    return elem && !elem_is_symbolic(elem);
}

void mat_value_init_zero(const struct matrix_t *A, void *slot)
{
    if (!A || !slot)
        return;
    elem_init_zero_value(A->elem, slot);
}

void mat_value_destroy(const struct matrix_t *A, void *slot)
{
    if (!A || !slot)
        return;
    elem_destroy_value(A->elem, slot);
}

void mat_get_owned(const struct matrix_t *A, size_t i, size_t j, void *out)
{
    unsigned char raw[64] = {0};

    if (!A || !out)
        return;

    if (A->elem->copy_value) {
        A->store->get(A, i, j, raw);
        A->elem->copy_value(out, raw);
        return;
    }

    A->store->get(A, i, j, out);
}

qcomplex_t mat_number_to_qcomplex_value(const number_t *value)
{
    number_t real;
    number_t imag;
    qcomplex_t out;

    if (!value)
        return QC_ZERO;

    real = num_real_part(*value);
    imag = num_imag_part(*value);
    out = qc_make(num_to_qfloat(real), num_to_qfloat(imag));

    num_destroy(&imag);
    num_destroy(&real);
    return out;
}

qfloat_t mat_number_to_real_qfloat_value(const number_t *value)
{
    number_t real;
    qfloat_t out;

    if (!value)
        return QF_ZERO;

    real = num_real_part(*value);
    out = num_to_qfloat(real);
    num_destroy(&real);
    return out;
}

qfloat_t mat_number_abs_qfloat_value(const number_t *value)
{
    number_t abs_value;
    qfloat_t out;

    if (!value)
        return QF_ZERO;

    abs_value = num_abs(*value);
    out = num_to_qfloat(abs_value);
    num_destroy(&abs_value);
    return out;
}

number_t mat_raw_value_to_number(const struct elem_vtable *elem, const void *value)
{
    if (elem == &number_elem) {
        number_t number;

        memcpy(&number, value, sizeof(number));
        return num_clone(number);
    }

    if (elem == &dval_elem) {
        dval_t *dv = NULL;

        memcpy(&dv, value, sizeof(dv));
        return dv ? dv_eval_num(dv) : NUM_ZERO;
    }

    return NUM_ZERO;
}

void mat_raw_value_from_number(const struct elem_vtable *elem, void *out,
                               const number_t *value)
{
    const number_t *source = value ? value : &NUM_ZERO;

    if (elem == &number_elem) {
        num_copy_value(out, source);
        return;
    }

    if (elem == &dval_elem)
        *(dval_t **)out = dv_new_const_num(*source);
}

qcomplex_t mat_raw_value_to_qcomplex(const struct elem_vtable *elem,
                                     const void *value)
{
    number_t number = mat_raw_value_to_number(elem, value);
    qcomplex_t out = mat_number_to_qcomplex_value(&number);

    num_destroy(&number);
    return out;
}

qfloat_t mat_raw_value_to_qfloat(const struct elem_vtable *elem,
                                 const void *value)
{
    number_t number = mat_raw_value_to_number(elem, value);
    qfloat_t out = mat_number_to_real_qfloat_value(&number);

    num_destroy(&number);
    return out;
}

qfloat_t mat_raw_value_abs_qfloat(const struct elem_vtable *elem,
                                  const void *value)
{
    number_t number = mat_raw_value_to_number(elem, value);
    qfloat_t out = mat_number_abs_qfloat_value(&number);

    num_destroy(&number);
    return out;
}

void mat_raw_value_from_qcomplex(const struct elem_vtable *elem, void *out,
                                 qcomplex_t value)
{
    number_t number = num_create_from_qcomplex(value);

    mat_raw_value_from_number(elem, out, &number);
    num_destroy(&number);
}

void mat_raw_value_from_qfloat(const struct elem_vtable *elem, void *out,
                               qfloat_t value)
{
    number_t number = num_create_from_qfloat(value);

    mat_raw_value_from_number(elem, out, &number);
    num_destroy(&number);
}

 void num_init_zero_slot(void *slot)
{
    *(number_t *)slot = NUM_ZERO;
}

 void num_copy_value(void *dst, const void *src)
{
    const number_t *value = src ? (const number_t *)src : &NUM_ZERO;

    *(number_t *)dst = num_is_immortal(*value) ? *value : num_clone(*value);
}

 void num_destroy_value(void *slot)
{
    number_t *value = (number_t *)slot;

    num_destroy(value);
    *value = num_new();
}

 bool num_is_structural_zero(const void *val)
{
    return !val || num_is_zero(*(const number_t *)val);
}

 void dv_init_zero_slot(void *slot)
{
    *(dval_t **)slot = NULL;
}

 void dv_copy_value(void *dst, const void *src)
{
    dval_t *dv = src ? *(dval_t *const *)src : NULL;

    *(dval_t **)dst = dval_clone_for_storage(dv);
}

 void dv_destroy_value(void *slot)
{
    dval_t *dv = *(dval_t **)slot;

    if (dv)
        dv_free(dv);
    *(dval_t **)slot = NULL;
}

 void dv_simplify_value(void *slot)
{
    dval_t *dv = *(dval_t **)slot;
    dval_t *simp;

    if (!dv)
        return;

    simp = dv_simplify(dv);
    dv_free(dv);
    *(dval_t **)slot = simp;
}

 bool dv_is_structural_zero(const void *val)
{
    return dval_is_exact_zero(*(dval_t *const *)val);
}

 bool dense_alloc(struct matrix_t *A) {
    size_t n = A->rows, m = A->cols, es = A->elem->size;

    A->data = malloc(n * sizeof(void*));
    if (!A->data)
        return false;
    for (size_t i = 0; i < n; i++) {
        A->data[i] = malloc(m * es);
        if (!A->data[i]) {
            for (size_t k = 0; k < i; k++) {
                for (size_t j = 0; j < m; j++) {
                    void *slot = (char *)A->data[k] + j * es;
                    elem_destroy_value(A->elem, slot);
                }
                free(A->data[k]);
            }
            free(A->data);
            A->data = NULL;
            return false;
        }
        for (size_t j = 0; j < m; j++) {
            void *slot = (char *)A->data[i] + j * es;
            elem_init_zero_value(A->elem, slot);
        }
    }
    A->nnz = 0;
    return true;
}

 void dense_free(struct matrix_t *A) {
    if (!A->data) return;
    for (size_t i = 0; i < A->rows; i++) {
        for (size_t j = 0; j < A->cols; j++) {
            void *slot = (char *)A->data[i] + j * A->elem->size;
            elem_destroy_value(A->elem, slot);
        }
        free(A->data[i]);
    }
    free(A->data);
    A->data = NULL;
}

 void dense_get(const struct matrix_t *A, size_t i, size_t j, void *out) {
    memcpy(out,
           (char*)A->data[i] + j * A->elem->size,
           A->elem->size);
}

 void dense_set(struct matrix_t *A, size_t i, size_t j, const void *val) {
    void *slot = (char *)A->data[i] + j * A->elem->size;

    elem_destroy_value(A->elem, slot);
    elem_copy_value(A->elem, slot, val);
}

void dense_swap_rows(struct matrix_t *A, size_t r1, size_t r2)
{
    void *tmp;

    if (!A || r1 == r2)
        return;

    tmp = A->data[r1];
    A->data[r1] = A->data[r2];
    A->data[r2] = tmp;
}

 void dense_row_eliminate_from(struct matrix_t *A, size_t dst_row, size_t src_row,
                                     size_t col_start, const void *factor)
{
    unsigned char src[64], dst[64], prod[64], out[64];

    if (!A || !factor || dst_row == src_row)
        return;

    for (size_t j = col_start; j < A->cols; j++) {
        dense_get(A, dst_row, j, dst);
        dense_get(A, src_row, j, src);
        A->elem->mul(prod, factor, src);
        A->elem->sub(out, dst, prod);
        dense_set(A, dst_row, j, out);
    }
}


/* ---------- sparse storage ---------- */

static sparse_entry_t *sparse_find_prev(const struct matrix_t *A, size_t row, size_t col)
{
    sparse_entry_t *prev = NULL;
    sparse_entry_t *cur = A->data ? (sparse_entry_t *)A->data[row] : NULL;

    while (cur && cur->col < col) {
        prev = cur;
        cur = cur->next;
    }

    return prev;
}

 bool sparse_alloc(struct matrix_t *A)
{
    A->data = calloc(A->rows, sizeof(void *));
    if (!A->data)
        return false;
    A->nnz = 0;
    return true;
}

 void sparse_free(struct matrix_t *A)
{
    if (!A->data)
        return;

    for (size_t i = 0; i < A->rows; i++) {
        sparse_entry_t *cur = (sparse_entry_t *)A->data[i];
        while (cur) {
            sparse_entry_t *next = cur->next;
            elem_destroy_value(A->elem, cur->value);
            free(cur);
            cur = next;
        }
    }

    free(A->data);
    A->data = NULL;
    A->nnz = 0;
}

 void sparse_get(const struct matrix_t *A, size_t i, size_t j, void *out)
{
    sparse_entry_t *prev = sparse_find_prev(A, i, j);
    sparse_entry_t *cur = prev ? prev->next : (A->data ? (sparse_entry_t *)A->data[i] : NULL);

    if (cur && cur->col == j) {
        memcpy(out, cur->value, A->elem->size);
        return;
    }

    memcpy(out, A->elem->zero, A->elem->size);
}

 void sparse_set(struct matrix_t *A, size_t i, size_t j, const void *val)
{
    sparse_entry_t *prev;
    sparse_entry_t *cur;
    int is_zero;

    prev = sparse_find_prev(A, i, j);
    cur = prev ? prev->next : (A->data ? (sparse_entry_t *)A->data[i] : NULL);
    is_zero = elem_is_structural_zero(A->elem, val);

    if (cur && cur->col == j) {
        if (is_zero) {
            if (prev)
                prev->next = cur->next;
            else
                A->data[i] = cur->next;
            elem_destroy_value(A->elem, cur->value);
            free(cur);
            if (A->nnz > 0)
                A->nnz--;
        } else {
            elem_destroy_value(A->elem, cur->value);
            elem_copy_value(A->elem, cur->value, val);
        }
        return;
    }

    if (is_zero)
        return;

    cur = malloc(sizeof(*cur) + A->elem->size);
    if (!cur)
        return;

    cur->col = j;
    memset(cur->value, 0, A->elem->size);
    elem_copy_value(A->elem, cur->value, val);
    if (prev) {
        cur->next = prev->next;
        prev->next = cur;
    } else {
        cur->next = (sparse_entry_t *)A->data[i];
        A->data[i] = cur;
    }
    A->nnz++;
}

 void sparse_swap_rows(struct matrix_t *A, size_t r1, size_t r2)
{
    void *tmp;

    if (!A || r1 == r2)
        return;

    tmp = A->data[r1];
    A->data[r1] = A->data[r2];
    A->data[r2] = tmp;
}

 void sparse_row_eliminate_from(struct matrix_t *A, size_t dst_row, size_t src_row,
                                      size_t col_start, const void *factor)
{
    sparse_entry_t *cur;
    unsigned char dst[64], prod[64], out[64];

    if (!A || !factor || dst_row == src_row || !A->data)
        return;

    cur = (sparse_entry_t *)A->data[src_row];
    while (cur && cur->col < col_start)
        cur = cur->next;

    while (cur) {
        sparse_get(A, dst_row, cur->col, dst);
        A->elem->mul(prod, factor, cur->value);
        A->elem->sub(out, dst, prod);
        sparse_set(A, dst_row, cur->col, out);
        cur = cur->next;
    }
}

 void sparse_materialise(struct matrix_t *A)
{
    void **old_rows;
    size_t old_nnz;

    old_rows = A->data;
    old_nnz = A->nnz;
    A->store = &dense_store;
    A->data = NULL;
    A->nnz = 0;
    if (!dense_alloc(A)) {
        A->store = &sparse_store;
        A->data = old_rows;
        A->nnz = old_nnz;
        return;
    }

    for (size_t i = 0; i < A->rows; i++) {
        sparse_entry_t *cur = (sparse_entry_t *)old_rows[i];
        while (cur) {
            dense_set(A, i, cur->col, cur->value);
            A->nnz++;
            elem_destroy_value(A->elem, cur->value);
            cur = cur->next;
        }
    }

    for (size_t i = 0; i < A->rows; i++) {
        sparse_entry_t *cur = (sparse_entry_t *)old_rows[i];
        while (cur) {
            sparse_entry_t *next = cur->next;
            free(cur);
            cur = next;
        }
    }
    free(old_rows);
}


/* ---------- identity storage ---------- */

 void ident_materialise(struct matrix_t *A);

 bool ident_alloc(struct matrix_t *A) {
    A->data = NULL;
    A->nnz = A->rows;
    return true;
}

 void ident_free(struct matrix_t *A) {
    (void)A;
}

 void ident_get(const struct matrix_t *A, size_t i, size_t j, void *out) {
    memcpy(out,
           (i == j) ? A->elem->one : A->elem->zero,
           A->elem->size);
}

 void ident_set(struct matrix_t *A, size_t i, size_t j, const void *val) {
    ident_materialise(A);
    dense_set(A, i, j, val);
}

 void ident_swap_rows(struct matrix_t *A, size_t r1, size_t r2)
{
    ident_materialise(A);
    dense_swap_rows(A, r1, r2);
}

 void ident_row_eliminate_from(struct matrix_t *A, size_t dst_row, size_t src_row,
                                     size_t col_start, const void *factor)
{
    ident_materialise(A);
    dense_row_eliminate_from(A, dst_row, src_row, col_start, factor);
}


 void ident_materialise(struct matrix_t *A) {
    A->store = &dense_store;
    if (!dense_alloc(A))
        return;

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++)
            dense_set(A, i, j, (i == j) ? A->elem->one : A->elem->zero);
}

/* ---------- diagonal storage ---------- */

 bool diagonal_alloc(struct matrix_t *A)
{
    size_t i;

    A->data = malloc(A->rows * sizeof(void *));
    if (!A->data)
        return false;

    for (i = 0; i < A->rows; i++) {
        A->data[i] = malloc(A->elem->size);
        if (!A->data[i]) {
            while (i > 0) {
                i--;
                elem_destroy_value(A->elem, A->data[i]);
                free(A->data[i]);
            }
            free(A->data);
            A->data = NULL;
            return false;
        }
        elem_init_zero_value(A->elem, A->data[i]);
    }

    A->nnz = 0;
    return true;
}

 void diagonal_free(struct matrix_t *A)
{
    if (!A->data)
        return;

    for (size_t i = 0; i < A->rows; i++) {
        elem_destroy_value(A->elem, A->data[i]);
        free(A->data[i]);
    }
    free(A->data);
    A->data = NULL;
    A->nnz = 0;
}

 void diagonal_get(const struct matrix_t *A, size_t i, size_t j, void *out)
{
    memcpy(out, (i == j) ? A->data[i] : A->elem->zero, A->elem->size);
}

 void diagonal_materialise(struct matrix_t *A)
{
    void **old_diagonal = A->data;
    size_t old_nnz = A->nnz;

    A->store = &dense_store;
    A->data = NULL;
    A->nnz = 0;
    if (!dense_alloc(A)) {
        A->store = &diagonal_store;
        A->data = old_diagonal;
        A->nnz = old_nnz;
        return;
    }

    for (size_t i = 0; i < A->rows; i++) {
        dense_set(A, i, i, old_diagonal[i]);
        if (!elem_is_structural_zero(A->elem, old_diagonal[i]))
            A->nnz++;
        elem_destroy_value(A->elem, old_diagonal[i]);
        free(old_diagonal[i]);
    }
    free(old_diagonal);
}

 void diagonal_set(struct matrix_t *A, size_t i, size_t j, const void *val)
{
    if (i == j) {
        int was_zero = elem_is_structural_zero(A->elem, A->data[i]);
        int is_zero = elem_is_structural_zero(A->elem, val);

        elem_destroy_value(A->elem, A->data[i]);
        elem_copy_value(A->elem, A->data[i], val);
        if (was_zero && !is_zero)
            A->nnz++;
        else if (!was_zero && is_zero)
            A->nnz--;
        return;
    }

    if (elem_is_structural_zero(A->elem, val))
        return;

    diagonal_materialise(A);
    dense_set(A, i, j, val);
    A->nnz++;
}

 void diagonal_swap_rows(struct matrix_t *A, size_t r1, size_t r2)
{
    diagonal_materialise(A);
    dense_swap_rows(A, r1, r2);
}

 void diagonal_row_eliminate_from(struct matrix_t *A, size_t dst_row, size_t src_row,
                                        size_t col_start, const void *factor)
{
    diagonal_materialise(A);
    dense_row_eliminate_from(A, dst_row, src_row, col_start, factor);
}


/* ---------- triangular storage ---------- */

static size_t upper_triangular_row_width(const struct matrix_t *A, size_t row)
{
    return (row < A->cols) ? (A->cols - row) : 0;
}

static size_t lower_triangular_row_width(const struct matrix_t *A, size_t row)
{
    return (row < A->cols) ? (row + 1) : A->cols;
}

static bool triangular_alloc(struct matrix_t *A, size_t (*row_width)(const struct matrix_t *, size_t))
{
    size_t i;

    A->data = malloc(A->rows * sizeof(void *));
    if (!A->data)
        return false;

    for (i = 0; i < A->rows; i++) {
        size_t width = row_width(A, i);
        A->data[i] = width ? malloc(width * A->elem->size) : NULL;
        if (width && !A->data[i]) {
            while (i > 0) {
                i--;
                size_t old_width = row_width(A, i);
                for (size_t j = 0; j < old_width; j++) {
                    void *slot = (char *)A->data[i] + j * A->elem->size;
                    elem_destroy_value(A->elem, slot);
                }
                free(A->data[i]);
            }
            free(A->data);
            A->data = NULL;
            return false;
        }
        for (size_t j = 0; j < width; j++) {
            void *slot = (char *)A->data[i] + j * A->elem->size;
            elem_init_zero_value(A->elem, slot);
        }
    }

    A->nnz = 0;
    return true;
}

 void triangular_free(struct matrix_t *A)
{
    if (!A->data)
        return;

    for (size_t i = 0; i < A->rows; i++) {
        size_t width = (A->store == &upper_triangular_store)
                     ? upper_triangular_row_width(A, i)
                     : lower_triangular_row_width(A, i);
        for (size_t j = 0; j < width; j++) {
            void *slot = (char *)A->data[i] + j * A->elem->size;
            elem_destroy_value(A->elem, slot);
        }
        free(A->data[i]);
    }
    free(A->data);
    A->data = NULL;
    A->nnz = 0;
}

 bool upper_triangular_alloc(struct matrix_t *A)
{
    return triangular_alloc(A, upper_triangular_row_width);
}

 bool lower_triangular_alloc(struct matrix_t *A)
{
    return triangular_alloc(A, lower_triangular_row_width);
}

 void upper_triangular_get(const struct matrix_t *A, size_t i, size_t j, void *out)
{
    if (i <= j && i < A->cols) {
        memcpy(out,
               (char *)A->data[i] + (j - i) * A->elem->size,
               A->elem->size);
        return;
    }

    memcpy(out, A->elem->zero, A->elem->size);
}

 void lower_triangular_get(const struct matrix_t *A, size_t i, size_t j, void *out)
{
    if (j <= i && j < A->cols) {
        memcpy(out,
               (char *)A->data[i] + j * A->elem->size,
               A->elem->size);
        return;
    }

    memcpy(out, A->elem->zero, A->elem->size);
}

 void upper_triangular_materialise(struct matrix_t *A)
{
    void **old_rows = A->data;
    size_t old_nnz = A->nnz;

    A->store = &dense_store;
    A->data = NULL;
    A->nnz = 0;
    if (!dense_alloc(A)) {
        A->store = &upper_triangular_store;
        A->data = old_rows;
        A->nnz = old_nnz;
        return;
    }

    for (size_t i = 0; i < A->rows; i++) {
        size_t width = upper_triangular_row_width(A, i);
        for (size_t offset = 0; offset < width; offset++) {
            void *src = (char *)old_rows[i] + offset * A->elem->size;
            dense_set(A, i, i + offset, src);
            if (!elem_is_structural_zero(A->elem, src))
                A->nnz++;
            elem_destroy_value(A->elem, src);
        }
        free(old_rows[i]);
    }
    free(old_rows);
}

 void lower_triangular_materialise(struct matrix_t *A)
{
    void **old_rows = A->data;
    size_t old_nnz = A->nnz;

    A->store = &dense_store;
    A->data = NULL;
    A->nnz = 0;
    if (!dense_alloc(A)) {
        A->store = &lower_triangular_store;
        A->data = old_rows;
        A->nnz = old_nnz;
        return;
    }

    for (size_t i = 0; i < A->rows; i++) {
        size_t width = lower_triangular_row_width(A, i);
        for (size_t j = 0; j < width; j++) {
            void *src = (char *)old_rows[i] + j * A->elem->size;
            dense_set(A, i, j, src);
            if (!elem_is_structural_zero(A->elem, src))
                A->nnz++;
            elem_destroy_value(A->elem, src);
        }
        free(old_rows[i]);
    }
    free(old_rows);
}

 void upper_triangular_set(struct matrix_t *A, size_t i, size_t j, const void *val)
{
    if (i <= j && i < A->cols) {
        void *slot = (char *)A->data[i] + (j - i) * A->elem->size;
        int was_zero = elem_is_structural_zero(A->elem, slot);
        int is_zero = elem_is_structural_zero(A->elem, val);

        elem_destroy_value(A->elem, slot);
        elem_copy_value(A->elem, slot, val);
        if (was_zero && !is_zero)
            A->nnz++;
        else if (!was_zero && is_zero)
            A->nnz--;
        return;
    }

    if (elem_is_structural_zero(A->elem, val))
        return;

    upper_triangular_materialise(A);
    dense_set(A, i, j, val);
    A->nnz++;
}

 void lower_triangular_set(struct matrix_t *A, size_t i, size_t j, const void *val)
{
    if (j <= i && j < A->cols) {
        void *slot = (char *)A->data[i] + j * A->elem->size;
        int was_zero = elem_is_structural_zero(A->elem, slot);
        int is_zero = elem_is_structural_zero(A->elem, val);

        elem_destroy_value(A->elem, slot);
        elem_copy_value(A->elem, slot, val);
        if (was_zero && !is_zero)
            A->nnz++;
        else if (!was_zero && is_zero)
            A->nnz--;
        return;
    }

    if (elem_is_structural_zero(A->elem, val))
        return;

    lower_triangular_materialise(A);
    dense_set(A, i, j, val);
    A->nnz++;
}

 void upper_triangular_swap_rows(struct matrix_t *A, size_t r1, size_t r2)
{
    upper_triangular_materialise(A);
    dense_swap_rows(A, r1, r2);
}

 void lower_triangular_swap_rows(struct matrix_t *A, size_t r1, size_t r2)
{
    lower_triangular_materialise(A);
    dense_swap_rows(A, r1, r2);
}

 void upper_triangular_row_eliminate_from(struct matrix_t *A, size_t dst_row, size_t src_row,
                                                size_t col_start, const void *factor)
{
    unsigned char src[64], dst[64], prod[64], out[64];

    if (!A || !factor || dst_row == src_row)
        return;

    for (size_t j = col_start; j < A->cols; j++) {
        upper_triangular_get(A, dst_row, j, dst);
        upper_triangular_get(A, src_row, j, src);
        A->elem->mul(prod, factor, src);
        A->elem->sub(out, dst, prod);
        upper_triangular_set(A, dst_row, j, out);
    }
}

 void lower_triangular_row_eliminate_from(struct matrix_t *A, size_t dst_row, size_t src_row,
                                                size_t col_start, const void *factor)
{
    lower_triangular_materialise(A);
    dense_row_eliminate_from(A, dst_row, src_row, col_start, factor);
}



/* ============================================================
   Internal constructor helper
   ============================================================ */

static struct matrix_t *mat_create_internal(size_t rows, size_t cols,
                                            const struct elem_vtable *elem,
                                            const struct store_vtable *store)
{
    struct matrix_t *A = malloc(sizeof(*A));
    if (!A) return NULL;

    A->rows  = rows;
    A->cols  = cols;
    A->elem  = elem;
    A->store = store;
    A->data  = NULL;
    A->nnz   = 0;

    if (!A->store->alloc(A)) {
        free(A);
        return NULL;
    }
    return A;
}

/* ============================================================
   Matrix construction policy helpers
   ============================================================ */

 struct matrix_t *store_create_dense(size_t rows, size_t cols,
                                           const struct elem_vtable *elem)
{
    return mat_create_internal(rows, cols, elem, &dense_store);
}

 struct matrix_t *store_create_sparse(size_t rows, size_t cols,
                                            const struct elem_vtable *elem)
{
    return mat_create_internal(rows, cols, elem, &sparse_store);
}

 struct matrix_t *store_create_identity(size_t rows, size_t cols,
                                              const struct elem_vtable *elem)
{
    if (rows != cols)
        return NULL;
    return mat_create_internal(rows, cols, elem, &identity_store);
}

 struct matrix_t *store_create_diagonal(size_t rows, size_t cols,
                                              const struct elem_vtable *elem)
{
    if (rows != cols)
        return NULL;
    return mat_create_internal(rows, cols, elem, &diagonal_store);
}

 struct matrix_t *store_create_upper_triangular(size_t rows, size_t cols,
                                                      const struct elem_vtable *elem)
{
    return mat_create_internal(rows, cols, elem, &upper_triangular_store);
}

 struct matrix_t *store_create_lower_triangular(size_t rows, size_t cols,
                                                      const struct elem_vtable *elem)
{
    return mat_create_internal(rows, cols, elem, &lower_triangular_store);
}

static bool store_false(const struct matrix_t *A)
{
    (void)A;
    return false;
}

 bool store_true(const struct matrix_t *A)
{
    return A != NULL;
}

 bool dense_is_sparse_storage(const struct matrix_t *A)
{
    return store_false(A);
}

 bool sparse_is_sparse_storage(const struct matrix_t *A)
{
    return store_true(A);
}

 bool identity_is_sparse_storage(const struct matrix_t *A)
{
    return store_false(A);
}

 bool diagonal_is_sparse_storage(const struct matrix_t *A)
{
    return store_false(A);
}

 bool upper_triangular_is_sparse_storage(const struct matrix_t *A)
{
    return store_false(A);
}

 bool lower_triangular_is_sparse_storage(const struct matrix_t *A)
{
    return store_false(A);
}

 bool dense_is_sparse_like(const struct matrix_t *A)
{
    return store_false(A);
}

 bool sparse_is_sparse_like(const struct matrix_t *A)
{
    return store_true(A);
}

 bool identity_is_sparse_like(const struct matrix_t *A)
{
    return store_true(A);
}

 bool diagonal_is_sparse_like(const struct matrix_t *A)
{
    return store_true(A);
}

 bool upper_triangular_is_sparse_like(const struct matrix_t *A)
{
    return store_false(A);
}

 bool lower_triangular_is_sparse_like(const struct matrix_t *A)
{
    return store_false(A);
}

 bool dense_is_diagonal(const struct matrix_t *A)
{
    unsigned char raw[64];

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            if (i == j)
                continue;
            dense_get(A, i, j, raw);
            if (!elem_is_structural_zero(A->elem, raw))
                return false;
        }

    return true;
}

 bool sparse_is_diagonal(const struct matrix_t *A)
{
    for (size_t i = 0; i < A->rows; i++) {
        sparse_entry_t *cur = A->data ? (sparse_entry_t *)A->data[i] : NULL;
        while (cur) {
            if (cur->col != i && !elem_is_structural_zero(A->elem, cur->value))
                return false;
            cur = cur->next;
        }
    }
    return true;
}

 bool upper_triangular_is_diagonal(const struct matrix_t *A)
{
    for (size_t i = 0; i < A->rows; i++) {
        size_t width = upper_triangular_row_width(A, i);
        for (size_t offset = 1; offset < width; offset++) {
            void *slot = (char *)A->data[i] + offset * A->elem->size;
            if (!elem_is_structural_zero(A->elem, slot))
                return false;
        }
    }
    return true;
}

 bool lower_triangular_is_diagonal(const struct matrix_t *A)
{
    for (size_t i = 0; i < A->rows; i++) {
        size_t width = lower_triangular_row_width(A, i);
        for (size_t j = 0; j + 1 < width; j++) {
            void *slot = (char *)A->data[i] + j * A->elem->size;
            if (!elem_is_structural_zero(A->elem, slot))
                return false;
        }
    }
    return true;
}

 bool generic_is_upper_triangular(const struct matrix_t *A)
{
    unsigned char raw[64];

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols && j < i; j++) {
            mat_get(A, i, j, raw);
            if (!elem_is_structural_zero(A->elem, raw))
                return false;
        }

    return true;
}

 bool generic_is_lower_triangular(const struct matrix_t *A)
{
    unsigned char raw[64];

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = i + 1; j < A->cols; j++) {
            mat_get(A, i, j, raw);
            if (!elem_is_structural_zero(A->elem, raw))
                return false;
        }

    return true;
}

 size_t dense_nonzero_count(const struct matrix_t *A)
{
    size_t count = 0;
    unsigned char raw[64];

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            dense_get(A, i, j, raw);
            if (!elem_is_structural_zero(A->elem, raw))
                count++;
        }

    return count;
}

 size_t sparse_nonzero_count(const struct matrix_t *A)
{
    return A->nnz;
}

 size_t identity_nonzero_count(const struct matrix_t *A)
{
    return A->rows;
}

 size_t diagonal_nonzero_count(const struct matrix_t *A)
{
    return A->nnz;
}

 size_t upper_triangular_nonzero_count(const struct matrix_t *A)
{
    return A->nnz;
}

 size_t lower_triangular_nonzero_count(const struct matrix_t *A)
{
    return A->nnz;
}

 const struct store_vtable *store_self_unary(const struct matrix_t *A)
{
    return A ? A->store : NULL;
}

 const struct store_vtable *identity_unary_store(const struct matrix_t *A)
{
    (void)A;
    return &diagonal_store;
}

 const struct store_vtable *store_self_transpose(const struct matrix_t *A)
{
    return A ? A->store : NULL;
}

 const struct store_vtable *upper_triangular_transpose_store(const struct matrix_t *A)
{
    (void)A;
    return &lower_triangular_store;
}

 const struct store_vtable *lower_triangular_transpose_store(const struct matrix_t *A)
{
    (void)A;
    return &upper_triangular_store;
}

struct matrix_t *mat_create_with_store(size_t rows, size_t cols,
                                       const struct elem_vtable *elem,
                                       const struct store_vtable *store)
{
    if (!elem || !store || !store->create)
        return NULL;
    return store->create(rows, cols, elem);
}

struct matrix_t *mat_create_dense_with_elem(size_t rows, size_t cols,
                                            const struct elem_vtable *elem)
{
    return mat_create_with_store(rows, cols, elem, &dense_store);
}

struct matrix_t *mat_create_sparse_with_elem(size_t rows, size_t cols,
                                             const struct elem_vtable *elem)
{
    return mat_create_with_store(rows, cols, elem, &sparse_store);
}

struct matrix_t *mat_create_identity_with_elem(size_t n,
                                               const struct elem_vtable *elem)
{
    return mat_create_with_store(n, n, elem, &identity_store);
}

struct matrix_t *mat_create_diagonal_with_elem(size_t n,
                                               const struct elem_vtable *elem)
{
    return mat_create_with_store(n, n, elem, &diagonal_store);
}

struct matrix_t *mat_create_upper_triangular_with_elem(size_t rows, size_t cols,
                                                       const struct elem_vtable *elem)
{
    return mat_create_with_store(rows, cols, elem, &upper_triangular_store);
}

struct matrix_t *mat_create_lower_triangular_with_elem(size_t rows, size_t cols,
                                                       const struct elem_vtable *elem)
{
    return mat_create_with_store(rows, cols, elem, &lower_triangular_store);
}

struct matrix_t *mat_create_elementwise_unary_result(size_t rows, size_t cols,
                                                     const struct elem_vtable *elem,
                                                     const struct matrix_t *layout_src)
{
    const struct store_vtable *store;

    if (!layout_src || !layout_src->store || !layout_src->store->elementwise_unary_store)
        return mat_create_dense_with_elem(rows, cols, elem);

    store = layout_src->store->elementwise_unary_store(layout_src);
    return mat_create_with_store(rows, cols, elem, store);
}

struct matrix_t *mat_create_transpose_result(size_t rows, size_t cols,
                                             const struct elem_vtable *elem,
                                             const struct matrix_t *layout_src)
{
    const struct store_vtable *store;

    if (!layout_src || !layout_src->store || !layout_src->store->transpose_store)
        return mat_create_dense_with_elem(rows, cols, elem);

    store = layout_src->store->transpose_store(layout_src);
    return mat_create_with_store(rows, cols, elem, store);
}

static struct matrix_t *mat_create_binary_result(size_t rows, size_t cols,
                                                 const struct elem_vtable *elem,
                                                 const struct matrix_t *A,
                                                 const struct matrix_t *B)
{
    const struct store_vtable *store = &dense_store;

    if (rows == cols &&
        mat_has_diagonal_structure(A) && mat_has_diagonal_structure(B))
        store = &diagonal_store;
    else if ((mat_has_upper_triangular_structure(A) && mat_has_upper_triangular_structure(B)) ||
             (mat_has_diagonal_structure(A) && mat_has_upper_triangular_structure(B)) ||
             (mat_has_upper_triangular_structure(A) && mat_has_diagonal_structure(B)))
        store = &upper_triangular_store;
    else if ((mat_has_lower_triangular_structure(A) && mat_has_lower_triangular_structure(B)) ||
             (mat_has_diagonal_structure(A) && mat_has_lower_triangular_structure(B)) ||
             (mat_has_lower_triangular_structure(A) && mat_has_diagonal_structure(B)))
        store = &lower_triangular_store;
    else if (mat_is_sparse_like(A) && mat_is_sparse_like(B))
        store = &sparse_store;

    return mat_create_with_store(rows, cols, elem, store);
}

static bool mat_uses_sparse_storage(const struct matrix_t *A)
{
    return A && A->store && A->store->is_sparse_storage &&
           A->store->is_sparse_storage(A);
}

static bool mat_is_sparse_like(const struct matrix_t *A)
{
    return A && A->store && A->store->is_sparse_like &&
           A->store->is_sparse_like(A);
}

bool mat_has_diagonal_structure(const struct matrix_t *A)
{
    return A && A->store && A->store->is_diagonal &&
           A->store->is_diagonal(A);
}

bool mat_has_upper_triangular_structure(const struct matrix_t *A)
{
    return A && A->store && A->store->is_upper_triangular &&
           A->store->is_upper_triangular(A);
}

bool mat_has_lower_triangular_structure(const struct matrix_t *A)
{
    return A && A->store && A->store->is_lower_triangular &&
           A->store->is_lower_triangular(A);
}

/* ---------- number ---------- */

 void num_add_wrap(void *o, const void *a, const void *b)
{
    *(number_t *)o = num_add(*(const number_t *)a, *(const number_t *)b);
}

 void num_sub_wrap(void *o, const void *a, const void *b)
{
    *(number_t *)o = num_sub(*(const number_t *)a, *(const number_t *)b);
}

 void num_mul_wrap(void *o, const void *a, const void *b)
{
    *(number_t *)o = num_mul(*(const number_t *)a, *(const number_t *)b);
}

 void num_inv_wrap(void *o, const void *a)
{
    *(number_t *)o = num_inv(*(const number_t *)a);
}

 int number_cmp_wrap(const void *a, const void *b)
{
    return num_cmp(*(const number_t *)a, *(const number_t *)b);
}

 void num_print_wrap(const void *v, char *buf, size_t n)
{
    char *text = num_to_string(*(const number_t *)v);

    if (!text) {
        snprintf(buf, n, "<num>");
        return;
    }

    snprintf(buf, n, "%s", text);
    free(text);
}

 int num_format_scalar(const void *v, int scientific, char *buf, size_t n)
{
    number_t value = *(const number_t *)v;
    char fmt[32];
    size_t significant_digits = num_get_prec_digits(value);
    size_t precision;
    int written;

    if (num_is_exact(value) || significant_digits == 0u) {
        written = num_sprintf(buf, n, scientific ? "%N" : "%n", value);
        return written < 0 ? -1 : 0;
    }

    precision = significant_digits > 0u ? significant_digits - 1u : 0u;
    snprintf(fmt, sizeof(fmt), "%%.%zu%c", precision, scientific ? 'N' : 'n');
    written = num_sprintf(buf, n, fmt, value);
    return written < 0 ? -1 : 0;
}

 double num_abs2_wrap(const void *a)
{
    number_t abs_value = num_abs(*(const number_t *)a);
    number_t squared = num_mul(abs_value, abs_value);
    double out = num_to_double(squared);

    num_destroy(&abs_value);
    num_destroy(&squared);
    return out;
}

 double num_to_real_wrap(const void *a)
{
    return num_to_double(*(const number_t *)a);
}

 void num_from_real_wrap(void *o, double x)
{
    *(number_t *)o = num_create_from_double(x);
}

 void num_conj_elem(void *o, const void *a)
{
    *(number_t *)o = num_conj(*(const number_t *)a);
}

 void num_scalar_exp(void *out, const void *a) { *(number_t *)out = num_exp(*(const number_t *)a); }
 void num_scalar_log(void *out, const void *a) { *(number_t *)out = num_log(*(const number_t *)a); }
 void num_scalar_sin(void *out, const void *a) { *(number_t *)out = num_sin(*(const number_t *)a); }
 void num_scalar_cos(void *out, const void *a) { *(number_t *)out = num_cos(*(const number_t *)a); }
 void num_scalar_tan(void *out, const void *a) { *(number_t *)out = num_tan(*(const number_t *)a); }
 void num_scalar_sinh(void *out, const void *a) { *(number_t *)out = num_sinh(*(const number_t *)a); }
 void num_scalar_cosh(void *out, const void *a) { *(number_t *)out = num_cosh(*(const number_t *)a); }
 void num_scalar_tanh(void *out, const void *a) { *(number_t *)out = num_tanh(*(const number_t *)a); }
 void num_scalar_sqrt(void *out, const void *a) { *(number_t *)out = num_sqrt(*(const number_t *)a); }
 void num_scalar_asin(void *out, const void *a) { *(number_t *)out = num_asin(*(const number_t *)a); }
 void num_scalar_acos(void *out, const void *a) { *(number_t *)out = num_acos(*(const number_t *)a); }
 void num_scalar_atan(void *out, const void *a) { *(number_t *)out = num_atan(*(const number_t *)a); }
 void num_scalar_asinh(void *out, const void *a) { *(number_t *)out = num_asinh(*(const number_t *)a); }
 void num_scalar_acosh(void *out, const void *a) { *(number_t *)out = num_acosh(*(const number_t *)a); }
 void num_scalar_atanh(void *out, const void *a) { *(number_t *)out = num_atanh(*(const number_t *)a); }
 void num_scalar_erf(void *out, const void *a) { *(number_t *)out = num_erf(*(const number_t *)a); }
 void num_scalar_erfc(void *out, const void *a) { *(number_t *)out = num_erfc(*(const number_t *)a); }
 void num_scalar_erfinv(void *out, const void *a) { *(number_t *)out = num_erfinv(*(const number_t *)a); }
 void num_scalar_erfcinv(void *out, const void *a) { *(number_t *)out = num_erfcinv(*(const number_t *)a); }
 void num_scalar_gamma(void *out, const void *a) { *(number_t *)out = num_gamma(*(const number_t *)a); }
 void num_scalar_lgamma(void *out, const void *a) { *(number_t *)out = num_lgamma(*(const number_t *)a); }
 void num_scalar_digamma(void *out, const void *a) { *(number_t *)out = num_digamma(*(const number_t *)a); }
 void num_scalar_trigamma(void *out, const void *a) { *(number_t *)out = num_trigamma(*(const number_t *)a); }
 void num_scalar_tetragamma(void *out, const void *a) { *(number_t *)out = num_tetragamma(*(const number_t *)a); }
 void num_scalar_gammainv(void *out, const void *a) { *(number_t *)out = num_gammainv(*(const number_t *)a); }
 void num_scalar_normal_pdf(void *out, const void *a) { *(number_t *)out = num_normal_pdf(*(const number_t *)a); }
 void num_scalar_normal_cdf(void *out, const void *a) { *(number_t *)out = num_normal_cdf(*(const number_t *)a); }
 void num_scalar_normal_logpdf(void *out, const void *a) { *(number_t *)out = num_normal_logpdf(*(const number_t *)a); }
 void num_scalar_lambert_w0(void *out, const void *a) { *(number_t *)out = num_lambert_w0(*(const number_t *)a); }
 void num_scalar_lambert_wm1(void *out, const void *a) { *(number_t *)out = num_lambert_wm1(*(const number_t *)a); }
 void num_scalar_productlog(void *out, const void *a) { *(number_t *)out = num_productlog(*(const number_t *)a); }
 void num_scalar_ei(void *out, const void *a) { *(number_t *)out = num_ei(*(const number_t *)a); }
 void num_scalar_e1(void *out, const void *a) { *(number_t *)out = num_e1(*(const number_t *)a); }



/* ---------- dval_t* ---------- */

 void dv_add_wrap(void *o, const void *a, const void *b)
{
    const dval_t *lhs = (*(dval_t *const *)a) ? *(dval_t *const *)a : DV_ZERO;
    const dval_t *rhs = (*(dval_t *const *)b) ? *(dval_t *const *)b : DV_ZERO;
    dval_t *prev = *(dval_t **)o;
    dval_t *res = dv_add(lhs, rhs);

    if (prev)
        dv_free(prev);
    *(dval_t **)o = res;
}

 void dv_sub_wrap(void *o, const void *a, const void *b)
{
    const dval_t *lhs = (*(dval_t *const *)a) ? *(dval_t *const *)a : DV_ZERO;
    const dval_t *rhs = (*(dval_t *const *)b) ? *(dval_t *const *)b : DV_ZERO;
    dval_t *prev = *(dval_t **)o;
    dval_t *res = dv_sub(lhs, rhs);

    if (prev)
        dv_free(prev);
    *(dval_t **)o = res;
}

 void dv_mul_wrap(void *o, const void *a, const void *b)
{
    const dval_t *lhs = (*(dval_t *const *)a) ? *(dval_t *const *)a : DV_ZERO;
    const dval_t *rhs = (*(dval_t *const *)b) ? *(dval_t *const *)b : DV_ZERO;
    dval_t *prev = *(dval_t **)o;
    dval_t *res = dv_mul(lhs, rhs);

    if (prev)
        dv_free(prev);
    *(dval_t **)o = res;
}

 void dv_inv_wrap(void *o, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)o;
    dval_t *res = dv_d_div(1.0, arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)o = res;
}

 int dval_cmp_wrap(const void *a, const void *b)
{
    dval_t *lhs = *(dval_t *const *)a;
    dval_t *rhs = *(dval_t *const *)b;

    if (!lhs && !rhs)
        return 0;
    if (!lhs)
        return -1;
    if (!rhs)
        return 1;
    return dv_cmp(lhs, rhs);
}

 void dv_print_wrap(const void *v, char *buf, size_t n)
{
    dval_t *dv = *(dval_t *const *)v;
    char *tmp;
    char *inner;
    char *sep;
    size_t len;

    if (!dv) {
        snprintf(buf, n, "NULL");
        return;
    }

    tmp = dv_to_string(dv, style_EXPRESSION);
    if (!tmp) {
        snprintf(buf, n, "<dval>");
        return;
    }

    /* Matrix entries read better without repeating the full binding block for
     * every cell, so prefer the compact expression body when available. */
    inner = tmp;
    len = strlen(tmp);
    if (len >= 4 && tmp[0] == '{' && tmp[1] == ' ' && tmp[len - 2] == ' ' && tmp[len - 1] == '}') {
        inner = tmp + 2;
        tmp[len - 2] = '\0';
        sep = strstr(inner, " | ");
        if (sep)
            *sep = '\0';
    }

    snprintf(buf, n, "%s", inner);
    free(tmp);
}

 double dv_abs2_wrap(const void *a)
{
    dval_t *dv = *(dval_t *const *)a;
    double x = dv ? dv_num_eval_d(dv) : 0.0;
    return x * x;
}

 double dv_to_real_wrap(const void *a)
{
    dval_t *dv = *(dval_t *const *)a;
    return dv ? dv_num_eval_d(dv) : 0.0;
}

 void dv_from_real_wrap(void *o, double x)
{
    dval_t *prev = *(dval_t **)o;
    dval_t *res = dv_num_const_d(x);

    if (prev)
        dv_free(prev);
    *(dval_t **)o = res;
}

 void dv_conj_elem(void *o, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)o;

    if (prev)
        dv_free(prev);
    if (arg)
        dv_retain(arg);
    *(dval_t **)o = arg;
}

 void dv_scalar_exp(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_exp(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_log(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_log(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_sin(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_sin(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_cos(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_cos(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_tan(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_tan(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_sinh(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_sinh(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_cosh(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_cosh(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_tanh(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_tanh(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_sqrt(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_sqrt(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_asin(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_asin(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_acos(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_acos(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_atan(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_atan(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_asinh(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_asinh(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_acosh(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_acosh(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_atanh(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_atanh(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_erf(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_erf(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_erfc(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_erfc(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_erfinv(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_erfinv(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_erfcinv(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_erfcinv(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_gamma(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_gamma(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_lgamma(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_lgamma(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_digamma(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_digamma(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_trigamma(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_trigamma(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_normal_pdf(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_normal_pdf(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_normal_cdf(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_normal_cdf(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_normal_logpdf(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_normal_logpdf(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_lambert_w0(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_lambert_w0(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_lambert_wm1(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_lambert_wm1(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_ei(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_ei(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

 void dv_scalar_e1(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_e1(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}



/* ============================================================
   Conversion helpers for mixed-type arithmetic
   ============================================================ */

static inline void num_as_dv(dval_t **out, const number_t *a) {
    *out = dv_new_const_num(*a);
}

static inline void id_dv(dval_t **out, dval_t *const *a) {
    *out = (dval_t *)((a && *a) ? *a : DV_ZERO);
}

/* ============================================================
   Cross-type arithmetic: add / sub / mul
   ============================================================ */

/* ---- number <-> dval ---- */

static void add_num_dv(void *out, const void *a, const void *b)
{
    dval_t *lhs = dv_new_const_num(*(const number_t *)a);
    dval_t *rhs;

    id_dv(&rhs, (dval_t *const *)b);
    *(dval_t **)out = dv_add(lhs, rhs);
    dv_free(lhs);
}

static void add_dv_num(void *out, const void *a, const void *b)
{
    dval_t *lhs;
    dval_t *rhs = dv_new_const_num(*(const number_t *)b);

    id_dv(&lhs, (dval_t *const *)a);
    *(dval_t **)out = dv_add(lhs, rhs);
    dv_free(rhs);
}

static void sub_num_dv(void *out, const void *a, const void *b)
{
    dval_t *lhs = dv_new_const_num(*(const number_t *)a);
    dval_t *rhs;

    id_dv(&rhs, (dval_t *const *)b);
    *(dval_t **)out = dv_sub(lhs, rhs);
    dv_free(lhs);
}

static void sub_dv_num(void *out, const void *a, const void *b)
{
    dval_t *lhs;
    dval_t *rhs = dv_new_const_num(*(const number_t *)b);

    id_dv(&lhs, (dval_t *const *)a);
    *(dval_t **)out = dv_sub(lhs, rhs);
    dv_free(rhs);
}

static void mul_num_dv(void *out, const void *a, const void *b)
{
    dval_t *lhs = dv_new_const_num(*(const number_t *)a);
    dval_t *rhs;

    id_dv(&rhs, (dval_t *const *)b);
    *(dval_t **)out = dv_mul(lhs, rhs);
    dv_free(lhs);
}

static void mul_dv_num(void *out, const void *a, const void *b)
{
    dval_t *lhs;
    dval_t *rhs = dv_new_const_num(*(const number_t *)b);

    id_dv(&lhs, (dval_t *const *)a);
    *(dval_t **)out = dv_mul(lhs, rhs);
    dv_free(rhs);
}

/* ---- number <-> number ---- */

static void add_num_num(void *out, const void *a, const void *b)
{
    *(number_t *)out = num_add(*(const number_t *)a, *(const number_t *)b);
}

static void sub_num_num(void *out, const void *a, const void *b)
{
    *(number_t *)out = num_sub(*(const number_t *)a, *(const number_t *)b);
}

static void mul_num_num(void *out, const void *a, const void *b)
{
    *(number_t *)out = num_mul(*(const number_t *)a, *(const number_t *)b);
}

/* ---- dval <-> dval ---- */

static void add_dv_dv(void *out, const void *a, const void *b)
{
    const dval_t *lhs = (*(dval_t *const *)a) ? *(dval_t *const *)a : DV_ZERO;
    const dval_t *rhs = (*(dval_t *const *)b) ? *(dval_t *const *)b : DV_ZERO;
    *(dval_t **)out = dv_add(lhs, rhs);
}

static void sub_dv_dv(void *out, const void *a, const void *b)
{
    const dval_t *lhs = (*(dval_t *const *)a) ? *(dval_t *const *)a : DV_ZERO;
    const dval_t *rhs = (*(dval_t *const *)b) ? *(dval_t *const *)b : DV_ZERO;
    *(dval_t **)out = dv_sub(lhs, rhs);
}

static void mul_dv_dv(void *out, const void *a, const void *b)
{
    const dval_t *lhs = (*(dval_t *const *)a) ? *(dval_t *const *)a : DV_ZERO;
    const dval_t *rhs = (*(dval_t *const *)b) ? *(dval_t *const *)b : DV_ZERO;
    *(dval_t **)out = dv_mul(lhs, rhs);
}

/* ============================================================
   Binary-operation 2D vtable (static)
   ============================================================ */

typedef struct {
    const struct elem_vtable *result_elem;

    void (*add)(void *out, const void *a, const void *b);
    void (*sub)(void *out, const void *a, const void *b);
    void (*mul)(void *out, const void *a, const void *b);
} binop_vtable;

typedef enum {
    BIN_ELEM_NUMBER = 0,
    BIN_ELEM_DVAL = 1,
    BIN_ELEM_MAX
} binop_elem_kind;

static binop_elem_kind elem_binop_kind(const struct elem_vtable *elem)
{
    if (elem == &number_elem)
        return BIN_ELEM_NUMBER;
    if (elem == &dval_elem)
        return BIN_ELEM_DVAL;
    return BIN_ELEM_MAX;
}

static const binop_vtable binops[BIN_ELEM_MAX][BIN_ELEM_MAX] = {
    [BIN_ELEM_NUMBER] = {
        [BIN_ELEM_NUMBER] = {
            .result_elem = &number_elem,
            .add = add_num_num,
            .sub = sub_num_num,
            .mul = mul_num_num
        },
        [BIN_ELEM_DVAL] = {
            .result_elem = &dval_elem,
            .add = add_num_dv,
            .sub = sub_num_dv,
            .mul = mul_num_dv
        }
    },

    [BIN_ELEM_DVAL] = {
        /* dval op number -> dval */
        [BIN_ELEM_NUMBER] = {
            .result_elem = &dval_elem,
            .add = add_dv_num,
            .sub = sub_dv_num,
            .mul = mul_dv_num
        },
        /* dval op dval -> dval */
        [BIN_ELEM_DVAL] = {
            .result_elem = &dval_elem,
            .add = add_dv_dv,
            .sub = sub_dv_dv,
            .mul = mul_dv_dv
        }
    }
};

const struct elem_vtable *mat_binary_result_elem(const matrix_t *A,
                                                 const matrix_t *B)
{
    binop_elem_kind ak, bk;

    if (!A || !B)
        return NULL;

    ak = elem_binop_kind(elem_of(A));
    bk = elem_binop_kind(elem_of(B));
    if (ak >= BIN_ELEM_MAX || bk >= BIN_ELEM_MAX)
        return NULL;

    return binops[ak][bk].result_elem;
}

matrix_t *mat_evaluate_num(const matrix_t *A)
{
    matrix_t *C = NULL;
    number_t value;

    if (!A)
        return NULL;

    C = mat_create_with_store(A->rows, A->cols, &number_elem, A->store);
    if (!C)
        return NULL;

    for (size_t i = 0; i < A->rows; ++i)
        for (size_t j = 0; j < A->cols; ++j) {
            value = mat_get_num(A, i, j);
            mat_set(C, i, j, &value);
            num_destroy(&value);
        }

    return C;
}

matrix_t *mat_scalar_mul_num(matrix_t *A, const number_t *s)
{
    if (!A || !s) return NULL;

    binop_elem_kind ak = elem_binop_kind(elem_of(A));
    const binop_vtable *op;
    const struct elem_vtable *re;
    matrix_t *R;
    unsigned char a_raw[64] = {0};
    unsigned char out_raw[64] = {0};

    if (ak >= BIN_ELEM_MAX)
        return NULL;

    op = &binops[BIN_ELEM_NUMBER][ak];
    if (!op->mul || !op->result_elem)
        return NULL;

    re = op->result_elem;
    R = mat_create_elementwise_unary_result(A->rows, A->cols, re, A);
    if (!R)
        return NULL;

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a_raw);
            op->mul(out_raw, s, a_raw);
            mat_set(R, i, j, out_raw);
            elem_destroy_value(re, out_raw);
        }

    return R;
}

matrix_t *mat_scalar_div_num(matrix_t *A, const number_t *s)
{
    number_t inv;
    matrix_t *out;

    if (!s)
        return NULL;

    inv = num_inv(*s);
    out = mat_scalar_mul_num(A, &inv);
    num_destroy(&inv);
    return out;
}

/* ============================================================
   Mixed-type matrix operations (2D vtable driven)
   ============================================================ */

static struct matrix_t *mat_add_or_sub_sparse(const struct matrix_t *A,
                                              const struct matrix_t *B,
                                              const binop_vtable *op,
                                              int is_sub)
{
    const struct elem_vtable *re = op->result_elem;
    struct matrix_t *C = mat_create_sparse_with_elem(A->rows, A->cols, re);
    unsigned char a_raw[64] = {0}, b_raw[64] = {0}, out[64] = {0};

    if (!C)
        return NULL;

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a_raw);
            mat_get(B, i, j, b_raw);
            if (is_sub)
                op->sub(out, a_raw, b_raw);
            else
                op->add(out, a_raw, b_raw);
            elem_simplify_value(re, out);
            mat_set(C, i, j, out);
            elem_destroy_value(re, out);
        }

    return C;
}

static struct matrix_t *mat_mul_sparse(const struct matrix_t *A,
                                       const struct matrix_t *B,
                                       const binop_vtable *op)
{
    const struct elem_vtable *re = op->result_elem;
    struct matrix_t *As = NULL, *Bs = NULL, *C = NULL;
    unsigned char x_raw[64] = {0}, y_raw[64] = {0}, prod[64] = {0};
    unsigned char sum[64] = {0}, sum_acc[64] = {0};

    if (!re)
        return NULL;

    As = mat_uses_sparse_storage(A) ? (struct matrix_t *)A : mat_to_sparse(A);
    Bs = mat_uses_sparse_storage(B) ? (struct matrix_t *)B : mat_to_sparse(B);
    if (!As || !Bs) {
        if (As != A)
            mat_free(As);
        if (Bs != B)
            mat_free(Bs);
        return NULL;
    }

    C = mat_create_sparse_with_elem(A->rows, B->cols, re);
    if (!C) {
        if (As != A)
            mat_free(As);
        if (Bs != B)
            mat_free(Bs);
        return NULL;
    }

    for (size_t i = 0; i < As->rows; i++) {
        sparse_entry_t *a_cur = As->data ? (sparse_entry_t *)As->data[i] : NULL;

        while (a_cur) {
            size_t k = a_cur->col;
            sparse_entry_t *b_cur = Bs->data ? (sparse_entry_t *)Bs->data[k] : NULL;

            memcpy(x_raw, a_cur->value, As->elem->size);
            while (b_cur) {
                mat_get(C, i, b_cur->col, sum);
                if (elem_is_structural_zero(re, sum))
                    elem_copy_value(re, sum_acc, re->zero);
                else
                    elem_copy_value(re, sum_acc, sum);
                memcpy(y_raw, b_cur->value, Bs->elem->size);
                op->mul(prod, x_raw, y_raw);
                re->add(sum_acc, sum_acc, prod);
                elem_simplify_value(re, sum_acc);
                mat_set(C, i, b_cur->col, sum_acc);
                elem_destroy_value(re, sum_acc);
                elem_destroy_value(re, prod);
                b_cur = b_cur->next;
            }

            a_cur = a_cur->next;
        }
    }

    if (As != A)
        mat_free(As);
    if (Bs != B)
        mat_free(Bs);
    return C;
}

struct matrix_t *mat_add(const struct matrix_t *A, const struct matrix_t *B) {
    binop_elem_kind ak, bk;
    const binop_vtable *op;

    if (!A || !B || A->rows != B->rows || A->cols != B->cols)
        return NULL;

    ak = elem_binop_kind(elem_of(A));
    bk = elem_binop_kind(elem_of(B));
    if (ak >= BIN_ELEM_MAX || bk >= BIN_ELEM_MAX)
        return NULL;
    op = &binops[ak][bk];
    if (!op->add || !op->result_elem)
        return NULL;

    if (mat_is_sparse_like(A) && mat_is_sparse_like(B))
        return mat_add_or_sub_sparse(A, B, op, 0);

    const struct elem_vtable *re = op->result_elem;
    struct matrix_t *C = mat_create_binary_result(A->rows, A->cols, re, A, B);
    if (!C) return NULL;

    unsigned char a_raw[64] = {0}, b_raw[64] = {0}, out[64] = {0};

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a_raw);
            mat_get(B, i, j, b_raw);
            op->add(out, a_raw, b_raw);
            elem_simplify_value(re, out);
            mat_set(C, i, j, out);
            elem_destroy_value(re, out);
        }

    return C;
}

struct matrix_t *mat_sub(const struct matrix_t *A, const struct matrix_t *B) {
    binop_elem_kind ak, bk;
    const binop_vtable *op;

    if (!A || !B || A->rows != B->rows || A->cols != B->cols)
        return NULL;

    ak = elem_binop_kind(elem_of(A));
    bk = elem_binop_kind(elem_of(B));
    if (ak >= BIN_ELEM_MAX || bk >= BIN_ELEM_MAX)
        return NULL;
    op = &binops[ak][bk];
    if (!op->sub || !op->result_elem)
        return NULL;

    if (mat_is_sparse_like(A) && mat_is_sparse_like(B))
        return mat_add_or_sub_sparse(A, B, op, 1);

    const struct elem_vtable *re = op->result_elem;
    struct matrix_t *C = mat_create_binary_result(A->rows, A->cols, re, A, B);
    if (!C) return NULL;

    unsigned char a_raw[64] = {0}, b_raw[64] = {0}, out[64] = {0};

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a_raw);
            mat_get(B, i, j, b_raw);
            op->sub(out, a_raw, b_raw);
            elem_simplify_value(re, out);
            mat_set(C, i, j, out);
            elem_destroy_value(re, out);
        }

    return C;
}

struct matrix_t *mat_mul(const struct matrix_t *A, const struct matrix_t *B) {
    binop_elem_kind ak, bk;
    const binop_vtable *op;

    if (!A || !B || A->cols != B->rows)
        return NULL;

    ak = elem_binop_kind(elem_of(A));
    bk = elem_binop_kind(elem_of(B));
    if (ak >= BIN_ELEM_MAX || bk >= BIN_ELEM_MAX)
        return NULL;
    op = &binops[ak][bk];
    if (!op->mul || !op->result_elem)
        return NULL;

    if (mat_is_sparse_like(A) && mat_is_sparse_like(B) && op->result_elem != &dval_elem)
        return mat_mul_sparse(A, B, op);

    const struct elem_vtable *re = op->result_elem;
    struct matrix_t *C = mat_create_binary_result(A->rows, B->cols, re, A, B);
    if (!C) return NULL;

    unsigned char x_raw[64] = {0}, y_raw[64] = {0}, prod[64] = {0}, sum[64] = {0};

    for (size_t i = 0; i < A->rows; i++) {
        for (size_t j = 0; j < B->cols; j++) {

            elem_copy_value(re, sum, re->zero);

            for (size_t k = 0; k < A->cols; k++) {
                mat_get(A, i, k, x_raw);
                mat_get(B, k, j, y_raw);
                op->mul(prod, x_raw, y_raw);
                re->add(sum, sum, prod);
                elem_simplify_value(re, sum);
                elem_destroy_value(re, prod);
            }

            mat_set(C, i, j, sum);
            elem_destroy_value(re, sum);
        }
    }

    if (re == &dval_elem && mat_simplify_symbolic_inplace(C) != 0) {
        mat_free(C);
        return NULL;
    }

    return C;
}

matrix_t *mat_neg(const matrix_t *A)
{
    const struct elem_vtable *e;
    matrix_t *R;
    unsigned char a_raw[64] = {0}, out_raw[64] = {0};

    if (!A)
        return NULL;

    e = A->elem;
    if (!e || !e->sub)
        return NULL;

    R = mat_create_elementwise_unary_result(A->rows, A->cols, e, A);
    if (!R)
        return NULL;

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a_raw);
            e->sub(out_raw, e->zero, a_raw);
            mat_set(R, i, j, out_raw);
            elem_destroy_value(e, out_raw);
        }

    return R;
}


/* ============================================================
   Schur decomposition: A = Q T Q*
   Full, Hessenberg + implicit double-shift QR, qcomplex backend
   ============================================================ */

static qcomplex_t mat_eval_number_scalar_qcomplex(void (*scalar_f)(void *out, const void *in),
                                                  qcomplex_t input_qc)
{
    number_t input = num_create_from_qcomplex(input_qc);
    number_t output = number_invalid();
    qcomplex_t out_qc;

    scalar_f(&output, &input);
    out_qc = mat_number_to_qcomplex_value(&output);
    num_destroy(&output);
    num_destroy(&input);
    return out_qc;
}

static void mat_eval_number_scalar_elem(const struct elem_vtable *elem,
                                        void *out,
                                        void (*scalar_f)(void *out, const void *in),
                                        const void *in)
{
    if (elem == &number_elem) {
        number_t result = number_invalid();

        scalar_f(&result, in);
        *(number_t *)out = result;
        return;
    }

    qcomplex_t in_qc;
    qcomplex_t out_qc;

    in_qc = mat_raw_value_to_qcomplex(elem, in);
    out_qc = mat_eval_number_scalar_qcomplex(scalar_f, in_qc);
    mat_raw_value_from_qcomplex(elem, out, out_qc);
}

static void qc_fun_coeffs_up_to_second(qcomplex_t *c0,
                                       qcomplex_t *c1,
                                       qcomplex_t *c2,
                                       void (*scalar_f)(void *out, const void *in),
                                       qcomplex_t lambda)
{
    qcomplex_t f0 = mat_eval_number_scalar_qcomplex(scalar_f, lambda);
    if (c0)
        *c0 = f0;

    if (scalar_f == number_elem.fun->gamma) {
        qcomplex_t psi = qc_digamma(lambda);
        qcomplex_t tri = qc_trigamma(lambda);
        if (c1)
            *c1 = qc_mul(f0, psi);
        if (c2) {
            qcomplex_t second = qc_mul(f0, qc_add(tri, qc_mul(psi, psi)));
            *c2 = qc_mul(qc_make(qf_from_double(0.5), QF_ZERO), second);
        }
        return;
    }

    if (scalar_f == number_elem.fun->digamma) {
        if (c1)
            *c1 = qc_trigamma(lambda);
        if (c2)
            *c2 = qc_mul(qc_make(qf_from_double(0.5), QF_ZERO),
                         qc_tetragamma(lambda));
        return;
    }

    if (scalar_f == number_elem.fun->lambert_w0 ||
        scalar_f == number_elem.fun->lambert_wm1) {
        qcomplex_t one = qc_make(QF_ONE, QF_ZERO);
        qcomplex_t two = qc_make(qf_from_double(2.0), QF_ZERO);
        qcomplex_t wp1 = qc_add(one, f0);
        qcomplex_t lam2 = qc_mul(lambda, lambda);
        qcomplex_t denom1 = qc_mul(lambda, wp1);
        if (c1)
            *c1 = qc_div(f0, denom1);
        if (c2) {
            qcomplex_t numer = qc_neg(qc_mul(qc_mul(f0, f0), qc_add(f0, two)));
            qcomplex_t denom = qc_mul(lam2, qc_mul(wp1, qc_mul(wp1, wp1)));
            *c2 = qc_mul(qc_make(qf_from_double(0.5), QF_ZERO), qc_div(numer, denom));
        }
        return;
    }

    if (scalar_f == number_elem.fun->ei) {
        qcomplex_t exp_lambda = qc_exp(lambda);
        qcomplex_t one = qc_make(QF_ONE, QF_ZERO);
        qcomplex_t lam2 = qc_mul(lambda, lambda);
        if (c1)
            *c1 = qc_div(exp_lambda, lambda);
        if (c2) {
            qcomplex_t second = qc_div(qc_mul(exp_lambda, qc_sub(lambda, one)), lam2);
            *c2 = qc_mul(qc_make(qf_from_double(0.5), QF_ZERO), second);
        }
        return;
    }

    if (scalar_f == number_elem.fun->e1) {
        qcomplex_t one = qc_make(QF_ONE, QF_ZERO);
        qcomplex_t emlambda = qc_exp(qc_neg(lambda));
        qcomplex_t lam2 = qc_mul(lambda, lambda);
        if (c1)
            *c1 = qc_div(qc_neg(emlambda), lambda);
        if (c2) {
            qcomplex_t second = qc_div(qc_mul(emlambda, qc_add(lambda, one)), lam2);
            *c2 = qc_mul(qc_make(qf_from_double(0.5), QF_ZERO), second);
        }
        return;
    }

    if (scalar_f == number_elem.fun->erf) {
        qcomplex_t scale = qc_make(qf_div(qf_from_double(2.0), QF_SQRT_PI), QF_ZERO);
        qcomplex_t fp = qc_mul(scale, qc_exp(qc_neg(qc_mul(lambda, lambda))));
        if (c1)
            *c1 = fp;
        if (c2)
            *c2 = qc_neg(qc_mul(lambda, fp));
        return;
    }

    if (scalar_f == number_elem.fun->erfc) {
        qcomplex_t scale = qc_make(qf_div(qf_neg(qf_from_double(2.0)), QF_SQRT_PI), QF_ZERO);
        qcomplex_t fp = qc_mul(scale, qc_exp(qc_neg(qc_mul(lambda, lambda))));
        if (c1)
            *c1 = fp;
        if (c2)
            *c2 = qc_neg(qc_mul(lambda, fp));
        return;
    }

    if (scalar_f == number_elem.fun->normal_pdf) {
        qcomplex_t fp = qc_neg(qc_mul(lambda, f0));
        if (c1)
            *c1 = fp;
        if (c2) {
            qcomplex_t lambda2 = qc_mul(lambda, lambda);
            *c2 = qc_mul(qc_make(qf_from_double(0.5), QF_ZERO),
                         qc_mul(qc_sub(lambda2, qc_make(QF_ONE, QF_ZERO)), f0));
        }
        return;
    }

    if (scalar_f == number_elem.fun->normal_cdf) {
        qcomplex_t pdf = qc_normal_pdf(lambda);
        if (c1)
            *c1 = pdf;
        if (c2)
            *c2 = qc_mul(qc_make(qf_from_double(-0.5), QF_ZERO), qc_mul(lambda, pdf));
        return;
    }

    if (scalar_f == number_elem.fun->normal_logpdf) {
        if (c1)
            *c1 = qc_neg(lambda);
        if (c2)
            *c2 = qc_make(qf_from_double(-0.5), QF_ZERO);
        return;
    }

    qfloat_t h = qf_from_double(1e-6);
    qcomplex_t ih = qc_make(QF_ZERO, h);
    qcomplex_t fp, fm, lambda_p, lambda_m;
    qcomplex_t denom1 = qc_make(QF_ZERO, qf_mul_double(h, 2.0));
    qcomplex_t denom2 = qc_make(qf_mul_double(qf_mul(h, h), -2.0), QF_ZERO);
    qcomplex_t two_f0 = qc_mul(qc_make(qf_from_double(2.0), QF_ZERO), f0);

    lambda_p = qc_add(lambda, ih);
    lambda_m = qc_sub(lambda, ih);
    fp = mat_eval_number_scalar_qcomplex(scalar_f, lambda_p);
    fm = mat_eval_number_scalar_qcomplex(scalar_f, lambda_m);

    if (c1)
        *c1 = qc_div(qc_sub(fp, fm), denom1);
    if (c2)
        *c2 = qc_div(qc_add(qc_sub(fp, two_f0), fm), denom2);
}

static matrix_t *mat_fun_triangular_equal_diag(const matrix_t *T,
                                               void (*scalar_f)(void *out, const void *in))
{
    size_t n = T->rows;
    const struct elem_vtable *e = T->elem;
    matrix_t *F = mat_create_upper_triangular_with_elem(n, n, e);
    matrix_t *N = mat_create_upper_triangular_with_elem(n, n, e);
    if (!F || !N) {
        mat_free(F);
        mat_free(N);
        return NULL;
    }

    unsigned char raw[64] = {0};
    unsigned char slot[64] = {0};
    qcomplex_t lambda, c0, c1 = QC_ZERO, c2 = QC_ZERO;
    mat_get(T, 0, 0, raw);
    lambda = mat_raw_value_to_qcomplex(e, raw);
    qc_fun_coeffs_up_to_second(&c0, &c1, &c2, scalar_f, lambda);

    for (size_t i = 0; i < n; ++i) {
        mat_raw_value_from_qcomplex(e, slot, c0);
        mat_set(F, i, i, slot);
        elem_destroy_value(e, slot);
        for (size_t j = i; j < n; ++j) {
            qcomplex_t tij;
            mat_get(T, i, j, raw);
            tij = mat_raw_value_to_qcomplex(e, raw);
            if (i == j)
                tij = QC_ZERO;
            mat_raw_value_from_qcomplex(e, slot, tij);
            mat_set(N, i, j, slot);
            elem_destroy_value(e, slot);
        }
    }

    if (n >= 2) {
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                qcomplex_t nij, term;
                mat_get(N, i, j, raw);
                nij = mat_raw_value_to_qcomplex(e, raw);
                term = qc_mul(c1, nij);
                mat_raw_value_from_qcomplex(e, slot, term);
                mat_set(F, i, j, slot);
                elem_destroy_value(e, slot);
            }
        }
    }

    if (n >= 3) {
        matrix_t *N2 = mat_mul(N, N);
        if (!N2) {
            mat_free(F);
            mat_free(N);
            return NULL;
        }

        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 2; j < n; ++j) {
                qcomplex_t fij, n2ij;
                mat_get(F, i, j, raw);
                fij = mat_raw_value_to_qcomplex(e, raw);
                mat_get(N2, i, j, raw);
                n2ij = mat_raw_value_to_qcomplex(e, raw);
                fij = qc_add(fij, qc_mul(c2, n2ij));
                mat_raw_value_from_qcomplex(e, slot, fij);
                mat_set(F, i, j, slot);
                elem_destroy_value(e, slot);
            }
        }

        mat_free(N2);
    }

    mat_free(N);
    return F;
}

static int qc_divided_difference_perturbed(qcomplex_t *out,
                                           const qcomplex_t *nodes,
                                           size_t count,
                                           void (*scalar_f)(void *out, const void *in))
{
    qcomplex_t *pts = NULL;
    qcomplex_t *table = NULL;
    qfloat_t base_step;

    if (!out || !nodes || count == 0 || !scalar_f)
        return -1;

    pts = calloc(count, sizeof(*pts));
    table = calloc(count * count, sizeof(*table));
    if (!pts || !table) {
        free(pts);
        free(table);
        return -1;
    }

    base_step = qf_from_double(1e-12);
    for (size_t d = 0; d < count; ++d) {
        size_t dup_index = 0;
        qfloat_t scale;
        qfloat_t step;
        qcomplex_t shift;

        pts[d] = nodes[d];
        for (size_t p = 0; p < d; ++p)
            if (qf_lt(qc_abs(qc_sub(nodes[d], nodes[p])), qf_from_double(1e-24)))
                dup_index++;

        if (dup_index > 0) {
            scale = qc_abs(nodes[d]);
            if (qf_lt(scale, QF_ONE))
                scale = QF_ONE;
            step = qf_mul(base_step, scale);
            shift = qc_make(qf_mul(step, qf_from_double((double)dup_index)), QF_ZERO);
            pts[d] = qc_add(pts[d], shift);
        }
    }

    for (size_t i = 0; i < count; ++i)
        table[i * count] = mat_eval_number_scalar_qcomplex(scalar_f, pts[i]);

    for (size_t order = 1; order < count; ++order) {
        for (size_t i = 0; i + order < count; ++i) {
            qcomplex_t numer;
            qcomplex_t denom;

            numer = qc_sub(table[(i + 1) * count + (order - 1)],
                           table[i * count + (order - 1)]);
            denom = qc_sub(pts[i + order], pts[i]);
            table[i * count + order] = qc_div(numer, denom);
        }
    }

    *out = table[count - 1];
    free(table);
    free(pts);
    return 0;
}

static int mat_fun_triangular_confluent_sum_paths(qcomplex_t *out,
                                                  const matrix_t *T,
                                                  size_t start,
                                                  size_t current,
                                                  size_t end,
                                                  qcomplex_t edge_prod,
                                                  qcomplex_t *nodes,
                                                  size_t depth,
                                                  void (*scalar_f)(void *out, const void *in))
{
    const struct elem_vtable *e = T->elem;
    qcomplex_t contrib;
    unsigned char raw[64] = {0};

    if (!out || !T || !nodes || !scalar_f || current > end)
        return -1;

    (void)start;

    if (current == end) {
        if (qc_divided_difference_perturbed(&contrib, nodes, depth, scalar_f) != 0)
            return -1;
        *out = qc_add(*out, qc_mul(edge_prod, contrib));
        return 0;
    }

    for (size_t next = current + 1; next <= end; ++next) {
        qcomplex_t tij;
        qcomplex_t lam_next;
        qcomplex_t next_prod;

        mat_get(T, current, next, raw);
        tij = mat_raw_value_to_qcomplex(e, raw);
        if (qf_lt(qc_abs(tij), qf_from_double(1e-30)))
            continue;

        mat_get(T, next, next, raw);
        lam_next = mat_raw_value_to_qcomplex(e, raw);
        nodes[depth] = lam_next;
        next_prod = qc_mul(edge_prod, tij);
        if (mat_fun_triangular_confluent_sum_paths(out, T, start, next, end,
                                                   next_prod, nodes, depth + 1,
                                                   scalar_f) != 0)
            return -1;
    }

    return 0;
}

static int mat_fun_triangular_confluent_fallback(qcomplex_t *out,
                                                 const matrix_t *T,
                                                 size_t i,
                                                 size_t j,
                                                 void (*scalar_f)(void *out, const void *in))
{
    const struct elem_vtable *e = T->elem;
    qcomplex_t *nodes = NULL;
    unsigned char raw[64] = {0};

    if (!out || !T || !scalar_f || i >= j || j >= T->rows || T->rows != T->cols)
        return -1;

    nodes = calloc(j - i + 1, sizeof(*nodes));
    if (!nodes)
        return -1;

    *out = QC_ZERO;
    mat_get(T, i, i, raw);
    nodes[0] = mat_raw_value_to_qcomplex(e, raw);
    if (mat_fun_triangular_confluent_sum_paths(out, T, i, i, j, qc_make(QF_ONE, QF_ZERO),
                                               nodes, 1, scalar_f) != 0) {
        free(nodes);
        return -1;
    }

    free(nodes);
    return 0;
}

matrix_t *mat_fun_triangular(const matrix_t *T,
                             void (*scalar_f)(void *out, const void *in))
{
    if (!T || !scalar_f)
        return NULL;

    if (T->rows != T->cols)
        return NULL;

    size_t n = T->rows;
    const struct elem_vtable *e = T->elem;

    unsigned char t_ii[64] = {0}, t_jj[64] = {0}, t_ij[64] = {0};
    unsigned char f_ii[64] = {0}, f_jj[64] = {0}, f_ij[64] = {0};
    unsigned char num[64] = {0}, tmp[64] = {0}, sum[64] = {0};
    unsigned char t_ik[64] = {0}, t_kj[64] = {0}, f_ik[64] = {0}, f_kj[64] = {0};
    unsigned char denom[64] = {0}, inv_denom[64] = {0};

    int all_diag_equal = 1;
    qcomplex_t lam0, lami;
    qfloat_t tol = qf_from_double(1e-24);
    unsigned char lam0_raw[64] = {0}, lami_raw[64] = {0};
    mat_get(T, 0, 0, lam0_raw);
    lam0 = mat_raw_value_to_qcomplex(e, lam0_raw);
    for (size_t i = 1; i < n; ++i) {
        mat_get(T, i, i, lami_raw);
        lami = mat_raw_value_to_qcomplex(e, lami_raw);
        if (qf_lt(tol, qc_abs(qc_sub(lami, lam0)))) {
            all_diag_equal = 0;
            break;
        }
    }
    if (all_diag_equal)
        return mat_fun_triangular_equal_diag(T, scalar_f);

    matrix_t *F = mat_create_upper_triangular_with_elem(n, n, e);
    if (!F)
        return NULL;

    /* 1. Diagonal: f(T_ii) */
    for (size_t i = 0; i < n; ++i) {
        mat_get(T, i, i, t_ii);
        mat_eval_number_scalar_elem(e, f_ii, scalar_f, t_ii);
        mat_set(F, i, i, f_ii);
        elem_destroy_value(e, f_ii);
    }

    /* 2. Off-diagonal: Parlett recurrence */
    for (size_t j = 1; j < n; ++j) {
        for (size_t i = j; i-- > 0; ) {
            if (i == j)
                continue;

            /* denom = T_ii - T_jj */
            mat_get(T, i, i, t_ii);
            mat_get(T, j, j, t_jj);
            e->sub(denom, t_ii, t_jj);

            /* sum = Σ_{k=i+1}^{j-1} (F_ik T_kj - T_ik F_kj) */
            memcpy(sum, e->zero, e->size);
            for (size_t k = i + 1; k < j; ++k) {
                mat_get(T, i, k, t_ik);
                mat_get(T, k, j, t_kj);
                mat_get(F, k, j, f_kj);
                mat_get(F, i, k, f_ik);

                /* tmp = F_ik * T_kj */
                e->mul(tmp, f_ik, t_kj);
                e->add(sum, sum, tmp);

                /* tmp = T_ik * F_kj */
                e->mul(tmp, t_ik, f_kj);
                e->sub(sum, sum, tmp);
            }

            /* num = T_ij * (F_ii - F_jj) + sum */
            mat_get(T, i, j, t_ij);
            mat_get(F, i, i, f_ii);
            mat_get(F, j, j, f_jj);

            e->sub(tmp, f_ii, f_jj);      /* tmp = F_ii - F_jj */
            e->mul(num, t_ij, tmp);       /* num = T_ij * (F_ii - F_jj) */
            e->add(num, num, sum);        /* num += sum */

            /* Handle T_ii == T_jj: avoid 0/0 */
            if (e->cmp(denom, e->zero) == 0) {
                qcomplex_t fallback = QC_ZERO;

                if (mat_fun_triangular_confluent_fallback(&fallback, T, i, j, scalar_f) == 0) {
                    mat_raw_value_from_qcomplex(e, f_ij, fallback);
                    mat_set(F, i, j, f_ij);
                    elem_destroy_value(e, f_ij);
                    elem_destroy_value(e, denom);
                    elem_destroy_value(e, num);
                    elem_destroy_value(e, tmp);
                    elem_destroy_value(e, sum);
                    continue;
                }

                if (e->cmp(num, e->zero) == 0) {
                    /* F[i,j] = f'(lambda) * T[i,j] via central difference */
                    unsigned char lam_p[64] = {0}, lam_m[64] = {0};
                    unsigned char fp[64] = {0}, fm[64] = {0};
                    unsigned char h_raw[64] = {0}, two_h[64] = {0};
                    unsigned char inv_2h[64] = {0}, deriv[64] = {0};
                    e->from_real(h_raw, 1e-10);
                    e->add(lam_p, t_ii, h_raw);
                    e->sub(lam_m, t_ii, h_raw);
                    mat_eval_number_scalar_elem(e, fp, scalar_f, lam_p);
                    mat_eval_number_scalar_elem(e, fm, scalar_f, lam_m);
                    e->sub(deriv, fp, fm);
                    e->from_real(two_h, 2e-10);
                    e->inv(inv_2h, two_h);
                    e->mul(deriv, deriv, inv_2h);
                    e->mul(f_ij, deriv, t_ij);
                    mat_set(F, i, j, f_ij);
                    elem_destroy_value(e, lam_p);
                    elem_destroy_value(e, lam_m);
                    elem_destroy_value(e, fp);
                    elem_destroy_value(e, fm);
                    elem_destroy_value(e, h_raw);
                    elem_destroy_value(e, two_h);
                    elem_destroy_value(e, inv_2h);
                    elem_destroy_value(e, deriv);
                    elem_destroy_value(e, f_ij);
                    elem_destroy_value(e, denom);
                    elem_destroy_value(e, num);
                    elem_destroy_value(e, tmp);
                    elem_destroy_value(e, sum);
                    continue;
                } else {
                    memcpy(f_ij, e->zero, e->size);
                    mat_set(F, i, j, f_ij);
                    elem_destroy_value(e, f_ij);
                    elem_destroy_value(e, denom);
                    elem_destroy_value(e, num);
                    elem_destroy_value(e, tmp);
                    elem_destroy_value(e, sum);
                    continue;
                }
            }

            /* F_ij = num / (T_ii - T_jj) = num * inv(denom) */
            e->inv(inv_denom, denom);
            e->mul(f_ij, num, inv_denom);
            mat_set(F, i, j, f_ij);
            elem_destroy_value(e, inv_denom);
            elem_destroy_value(e, f_ij);
            elem_destroy_value(e, denom);
            elem_destroy_value(e, num);
            elem_destroy_value(e, tmp);
            elem_destroy_value(e, sum);
        }
    }

    return F;
}
