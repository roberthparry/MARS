#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "matrix_internal.h"
#include "matrix_vtable_defs.h"
#include "number.h"
#include "qfloat.h"
#include "qcomplex.h"
#include "matrix.h"
#include "internal/expr_internal.h"

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

static size_t upper_triangular_capacity(size_t rows, size_t cols)
{
    size_t cap = 0;

    for (size_t i = 0; i < rows; ++i) {
        if (i < cols)
            cap += (cols - i);
    }
    return cap;
}

static size_t lower_triangular_capacity(size_t rows, size_t cols)
{
    size_t cap = 0;

    for (size_t i = 0; i < rows; ++i) {
        cap += (i + 1 < cols) ? (i + 1) : cols;
    }
    return cap;
}

/* ============================================================
   Storage vtables (dense, identity)
   ============================================================ */

typedef struct sparse_entry_t {
    size_t col;
    struct sparse_entry_t *next;
    unsigned char value[];
} sparse_entry_t;

static sparse_entry_t *sparse_find_prev(const struct matrix_t *A, size_t row, size_t col);

struct mat_precision_bucket {
    size_t bits;
    size_t count;
    struct mat_precision_bucket *next;
};

static size_t matrix_raw_precision_bits(const struct elem_vtable *elem, const void *raw)
{
    size_t precision_bits;

    if (elem != &number_elem || !raw)
        return 106u;
    precision_bits = num_get_effective_prec_bits(*(const number_t *)raw);
    return precision_bits == 0u ? 53u : precision_bits;
}

static bool matrix_raw_is_exact(const struct elem_vtable *elem, const void *raw)
{
    if (elem != &number_elem || !raw)
        return false;
    return num_is_exact(*(const number_t *)raw);
}

static struct mat_precision_bucket *precision_bucket_find(struct mat_precision_bucket *head, size_t bits)
{
    while (head) {
        if (head->bits == bits)
            return head;
        head = head->next;
    }
    return NULL;
}

static void precision_bucket_inc(struct matrix_t *A, size_t bits, size_t amount)
{
    struct mat_precision_bucket *bucket;

    if (!A || A->elem != &number_elem)
        return;

    bucket = precision_bucket_find(A->meta.numeric_precision_hist, bits);
    if (bucket) {
        bucket->count += amount;
    } else {
        bucket = malloc(sizeof(*bucket));
        if (!bucket)
            return;
        bucket->bits = bits;
        bucket->count = amount;
        bucket->next = A->meta.numeric_precision_hist;
        A->meta.numeric_precision_hist = bucket;
    }

    if (bits < A->meta.numeric_min_precision_bits)
        A->meta.numeric_min_precision_bits = bits;
}

static void precision_bucket_dec(struct matrix_t *A, size_t bits, size_t amount)
{
    struct mat_precision_bucket *prev = NULL;
    struct mat_precision_bucket *cur;

    if (!A || A->elem != &number_elem)
        return;

    cur = A->meta.numeric_precision_hist;
    while (cur && cur->bits != bits) {
        prev = cur;
        cur = cur->next;
    }
    if (!cur)
        return;

    if (cur->count > amount) {
        cur->count -= amount;
    } else {
        if (prev)
            prev->next = cur->next;
        else
            A->meta.numeric_precision_hist = cur->next;
        free(cur);
    }

    if (bits == A->meta.numeric_min_precision_bits) {
        size_t new_min = (size_t)-1;

        cur = A->meta.numeric_precision_hist;
        while (cur) {
            if (cur->bits < new_min)
                new_min = cur->bits;
            cur = cur->next;
        }
        A->meta.numeric_min_precision_bits = (new_min == (size_t)-1) ? 106u : new_min;
    }
}

static void precision_bucket_free_all(struct matrix_t *A)
{
    struct mat_precision_bucket *cur;

    if (!A)
        return;
    cur = A->meta.numeric_precision_hist;
    while (cur) {
        struct mat_precision_bucket *next = cur->next;
        free(cur);
        cur = next;
    }
    A->meta.numeric_precision_hist = NULL;
}

void mat_numeric_precision_release(struct matrix_t *A)
{
    precision_bucket_free_all(A);
}

size_t mat_cached_numeric_precision_bits(const struct matrix_t *A)
{
    if (!A || A->elem != &number_elem)
        return 106u;
    return A->meta.numeric_min_precision_bits ? A->meta.numeric_min_precision_bits : 106u;
}

void mat_numeric_precision_note_set(struct matrix_t *A,
                                    const void *old_val,
                                    const void *new_val)
{
    size_t old_bits;
    size_t new_bits;
    bool old_exact;
    bool new_exact;

    if (!A || A->elem != &number_elem)
        return;

    old_bits = matrix_raw_precision_bits(A->elem, old_val);
    new_bits = matrix_raw_precision_bits(A->elem, new_val);
    old_exact = matrix_raw_is_exact(A->elem, old_val);
    new_exact = matrix_raw_is_exact(A->elem, new_val);
    if (old_bits == new_bits)
        goto exact_update;

    precision_bucket_dec(A, old_bits, 1u);
    precision_bucket_inc(A, new_bits, 1u);

exact_update:
    if (old_exact != new_exact) {
        if (old_exact) {
            A->meta.numeric_inexact_count++;
        } else if (A->meta.numeric_inexact_count > 0) {
            A->meta.numeric_inexact_count--;
        }
    }
}

static bool expr_node_is_exact_zero(const expr_t *dv)
{
    return !dv || expr_is_exact_zero(dv);
}

expr_t *expr_clone_for_storage(const expr_t *dv)
{
    if (!dv)
        return NULL;
    if (dv == EXPR_ZERO || dv == EXPR_ONE) {
        number_t value = expr_get_val(dv);
        expr_t *clone = expr_new_const(value);

        num_destroy(&value);
        return clone;
    }
    expr_retain(dv);
    return (expr_t *)dv;
}

matrix_t *mat_finalize_symbolic_result(matrix_t *A)
{
    matrix_t *simplified;

    if (!A || A->elem != &expr_elem)
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
    unsigned char raw[MATRIX_SCALAR_STORAGE_BYTES] = {0};

    if (!A || !out)
        return;

    if (A->elem->copy_value) {
        A->store->get(A, i, j, raw);
        A->elem->copy_value(out, raw);
        return;
    }

    A->store->get(A, i, j, out);
}

number_t mat_raw_value_to_number(const struct elem_vtable *elem, const void *value)
{
    if (elem == &number_elem) {
        number_t number;

        memcpy(&number, value, sizeof(number));
        return num_clone(number);
    }

    if (elem == &expr_elem) {
        expr_t *dv = NULL;

        memcpy(&dv, value, sizeof(dv));
        return dv ? expr_eval(dv) : NUM_ZERO;
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

    if (elem == &expr_elem)
        *(expr_t **)out = expr_new_const(*source);
}

static void mat_number_slot_take(struct matrix_t *A, void *slot, number_t *value)
{
    number_t moved;
    int was_zero;
    int is_zero;
    number_t old_num;

    if (!A || A->elem != &number_elem || !slot)
        return;

    moved = value ? num_scope_detach(*value) : NUM_ZERO;
    was_zero = elem_is_structural_zero(A->elem, slot);
    is_zero = num_is_zero(moved);
    old_num = *(number_t *)slot;
    mat_numeric_precision_note_set(A, &old_num, &moved);
    elem_destroy_value(A->elem, slot);
    *(number_t *)slot = moved;

    if (was_zero && !is_zero)
        A->nnz++;
    else if (!was_zero && is_zero && A->nnz > 0)
        A->nnz--;

    if (value)
        *value = NUM_ZERO;
}

void mat_set_num_owned(struct matrix_t *A, size_t i, size_t j, number_t *value)
{
    if (!A || A->elem != &number_elem)
        return;

    if (A->store == &dense_store) {
        void *slot = (char *)A->data[i] + j * A->elem->size;
        mat_number_slot_take(A, slot, value);
        return;
    }

    if (A->store == &diagonal_store) {
        if (i == j) {
            mat_number_slot_take(A, A->data[i], value);
            return;
        }
        if (!value || num_is_zero(*value))
            return;
        diagonal_materialise(A);
        mat_set_num_owned(A, i, j, value);
        return;
    }

    if (A->store == &upper_triangular_store) {
        if (i <= j && i < A->cols) {
            void *slot = (char *)A->data[i] + (j - i) * A->elem->size;
            mat_number_slot_take(A, slot, value);
            return;
        }
        if (!value || num_is_zero(*value))
            return;
        upper_triangular_materialise(A);
        mat_set_num_owned(A, i, j, value);
        return;
    }

    if (A->store == &lower_triangular_store) {
        if (j <= i && j < A->cols) {
            void *slot = (char *)A->data[i] + j * A->elem->size;
            mat_number_slot_take(A, slot, value);
            return;
        }
        if (!value || num_is_zero(*value))
            return;
        lower_triangular_materialise(A);
        mat_set_num_owned(A, i, j, value);
        return;
    }

    if (A->store == &sparse_store) {
        sparse_entry_t *prev;
        sparse_entry_t *cur;
        number_t moved = value ? num_scope_detach(*value) : NUM_ZERO;
        int is_zero = num_is_zero(moved);

        prev = sparse_find_prev(A, i, j);
        cur = prev ? prev->next : (A->data ? (sparse_entry_t *)A->data[i] : NULL);

        if (cur && cur->col == j) {
            if (is_zero) {
                mat_numeric_precision_note_set(A, cur->value, A->elem->zero);
                if (prev)
                    prev->next = cur->next;
                else
                    A->data[i] = cur->next;
                elem_destroy_value(A->elem, cur->value);
                free(cur);
                if (A->nnz > 0)
                    A->nnz--;
            } else {
                mat_numeric_precision_note_set(A, cur->value, &moved);
                elem_destroy_value(A->elem, cur->value);
                *(number_t *)cur->value = moved;
            }
            if (value)
                *value = NUM_ZERO;
            return;
        }

        if (is_zero) {
            if (value)
                *value = NUM_ZERO;
            return;
        }

        cur = malloc(sizeof(*cur) + A->elem->size);
        if (!cur) {
            num_destroy(&moved);
            if (value)
                *value = NUM_ZERO;
            return;
        }

        cur->col = j;
        *(number_t *)cur->value = moved;
        mat_numeric_precision_note_set(A, A->elem->zero, &moved);
        if (prev) {
            cur->next = prev->next;
            prev->next = cur;
        } else {
            cur->next = (sparse_entry_t *)A->data[i];
            A->data[i] = cur;
        }
        A->nnz++;
        if (value)
            *value = NUM_ZERO;
        return;
    }

    mat_set(A, i, j, value ? value : &NUM_ZERO);
    if (value) {
        num_destroy(value);
        *value = NUM_ZERO;
    }
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
    *value = NUM_ZERO;
}

 bool num_is_structural_zero(const void *val)
{
    return !val || num_is_zero(*(const number_t *)val);
}

 void expr_init_zero_slot(void *slot)
{
    *(expr_t **)slot = NULL;
}

 void expr_copy_value(void *dst, const void *src)
{
    expr_t *dv = src ? *(expr_t *const *)src : NULL;

    *(expr_t **)dst = expr_clone_for_storage(dv);
}

 void expr_destroy_value(void *slot)
{
    expr_t *dv = *(expr_t **)slot;

    if (dv)
        expr_free(dv);
    *(expr_t **)slot = NULL;
}

 void expr_simplify_value(void *slot)
{
    expr_t *dv = *(expr_t **)slot;
    expr_t *simp;

    if (!dv)
        return;

    simp = expr_simplify(dv);
    expr_free(dv);
    *(expr_t **)slot = simp;
}

 bool expr_is_structural_zero(const void *val)
{
    return expr_node_is_exact_zero(*(expr_t *const *)val);
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
    number_t old_num;
    int was_zero = elem_is_structural_zero(A->elem, slot);
    int is_zero = elem_is_structural_zero(A->elem, val);

    if (A->elem == &number_elem) {
        old_num = *(number_t *)slot;
        mat_numeric_precision_note_set(A, &old_num, val);
    }
    elem_destroy_value(A->elem, slot);
    elem_copy_value(A->elem, slot, val);

    if (was_zero && !is_zero)
        A->nnz++;
    else if (!was_zero && is_zero && A->nnz > 0)
        A->nnz--;
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
    unsigned char src[MATRIX_SCALAR_STORAGE_BYTES], dst[MATRIX_SCALAR_STORAGE_BYTES], prod[MATRIX_SCALAR_STORAGE_BYTES], out[MATRIX_SCALAR_STORAGE_BYTES];

    if (!A || !factor || dst_row == src_row)
        return;

    elem_init_zero_value(A->elem, prod);
    elem_init_zero_value(A->elem, out);

    for (size_t j = col_start; j < A->cols; j++) {
        dense_get(A, dst_row, j, dst);
        dense_get(A, src_row, j, src);
        A->elem->mul(prod, factor, src);
        A->elem->sub(out, dst, prod);
        dense_set(A, dst_row, j, out);
    }

    elem_destroy_value(A->elem, out);
    elem_destroy_value(A->elem, prod);
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
            if (A->elem == &number_elem)
                mat_numeric_precision_note_set(A, cur->value, A->elem->zero);
            if (prev)
                prev->next = cur->next;
            else
                A->data[i] = cur->next;
            elem_destroy_value(A->elem, cur->value);
            free(cur);
            if (A->nnz > 0)
                A->nnz--;
        } else {
            if (A->elem == &number_elem)
                mat_numeric_precision_note_set(A, cur->value, val);
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
    if (A->elem == &number_elem)
        mat_numeric_precision_note_set(A, A->elem->zero, val);
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
    unsigned char dst[MATRIX_SCALAR_STORAGE_BYTES], prod[MATRIX_SCALAR_STORAGE_BYTES], out[MATRIX_SCALAR_STORAGE_BYTES];

    if (!A || !factor || dst_row == src_row || !A->data)
        return;

    cur = (sparse_entry_t *)A->data[src_row];
    while (cur && cur->col < col_start)
        cur = cur->next;

    elem_init_zero_value(A->elem, prod);
    elem_init_zero_value(A->elem, out);
    while (cur) {
        sparse_get(A, dst_row, cur->col, dst);
        A->elem->mul(prod, factor, cur->value);
        A->elem->sub(out, dst, prod);
        sparse_set(A, dst_row, cur->col, out);
        elem_destroy_value(A->elem, out);
        elem_destroy_value(A->elem, prod);
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
        number_t old_num;

        if (A->elem == &number_elem) {
            old_num = *(number_t *)A->data[i];
            mat_numeric_precision_note_set(A, &old_num, val);
        }
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
        number_t old_num;

        if (A->elem == &number_elem) {
            old_num = *(number_t *)slot;
            mat_numeric_precision_note_set(A, &old_num, val);
        }
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
}

 void lower_triangular_set(struct matrix_t *A, size_t i, size_t j, const void *val)
{
    if (j <= i && j < A->cols) {
        void *slot = (char *)A->data[i] + j * A->elem->size;
        int was_zero = elem_is_structural_zero(A->elem, slot);
        int is_zero = elem_is_structural_zero(A->elem, val);
        number_t old_num;

        if (A->elem == &number_elem) {
            old_num = *(number_t *)slot;
            mat_numeric_precision_note_set(A, &old_num, val);
        }
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
    unsigned char src[MATRIX_SCALAR_STORAGE_BYTES], dst[MATRIX_SCALAR_STORAGE_BYTES], prod[MATRIX_SCALAR_STORAGE_BYTES], out[MATRIX_SCALAR_STORAGE_BYTES];

    if (!A || !factor || dst_row == src_row)
        return;

    elem_init_zero_value(A->elem, prod);
    elem_init_zero_value(A->elem, out);

    for (size_t j = col_start; j < A->cols; j++) {
        upper_triangular_get(A, dst_row, j, dst);
        upper_triangular_get(A, src_row, j, src);
        A->elem->mul(prod, factor, src);
        A->elem->sub(out, dst, prod);
        upper_triangular_set(A, dst_row, j, out);
    }

    elem_destroy_value(A->elem, out);
    elem_destroy_value(A->elem, prod);
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
    A->meta.numeric_min_precision_bits = 106u;
    A->meta.numeric_inexact_count = 0;
    A->meta.numeric_precision_hist = NULL;

    if (!A->store->alloc(A)) {
        free(A);
        return NULL;
    }
    if (A->elem == &number_elem) {
        size_t zero_bits = matrix_raw_precision_bits(A->elem, A->elem->zero);
        precision_bucket_inc(A, zero_bits, rows * cols);
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
    unsigned char raw[MATRIX_SCALAR_STORAGE_BYTES];
    size_t diag_cap;

    diag_cap = (A->rows < A->cols) ? A->rows : A->cols;
    if (A->nnz > diag_cap)
        return false;

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
    unsigned char raw[MATRIX_SCALAR_STORAGE_BYTES];
    size_t cap = upper_triangular_capacity(A->rows, A->cols);

    if (A->nnz > cap)
        return false;

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
    unsigned char raw[MATRIX_SCALAR_STORAGE_BYTES];
    size_t cap = lower_triangular_capacity(A->rows, A->cols);

    if (A->nnz > cap)
        return false;

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
    return A ? A->nnz : 0u;
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

static void num_replace_value(void *slot, number_t value)
{
    number_t *out = (number_t *)slot;

    num_destroy(out);
    *out = value;
}

 void num_add_wrap(void *o, const void *a, const void *b)
{
    num_replace_value(o, num_add(*(const number_t *)a, *(const number_t *)b));
}

 void num_sub_wrap(void *o, const void *a, const void *b)
{
    num_replace_value(o, num_sub(*(const number_t *)a, *(const number_t *)b));
}

 void num_mul_wrap(void *o, const void *a, const void *b)
{
    num_replace_value(o, num_mul(*(const number_t *)a, *(const number_t *)b));
}

 void num_inv_wrap(void *o, const void *a)
{
    num_replace_value(o, num_inv(*(const number_t *)a));
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

 void num_conj_elem(void *o, const void *a)
{
    num_replace_value(o, num_conj(*(const number_t *)a));
}

 void num_scalar_exp(void *out, const void *a) { num_replace_value(out, num_exp(*(const number_t *)a)); }
 void num_scalar_log(void *out, const void *a) { num_replace_value(out, num_log(*(const number_t *)a)); }
 void num_scalar_sin(void *out, const void *a) { num_replace_value(out, num_sin(*(const number_t *)a)); }
 void num_scalar_cos(void *out, const void *a) { num_replace_value(out, num_cos(*(const number_t *)a)); }
 void num_scalar_tan(void *out, const void *a) { num_replace_value(out, num_tan(*(const number_t *)a)); }
 void num_scalar_sinh(void *out, const void *a) { num_replace_value(out, num_sinh(*(const number_t *)a)); }
 void num_scalar_cosh(void *out, const void *a) { num_replace_value(out, num_cosh(*(const number_t *)a)); }
 void num_scalar_tanh(void *out, const void *a) { num_replace_value(out, num_tanh(*(const number_t *)a)); }
 void num_scalar_sqrt(void *out, const void *a) { num_replace_value(out, num_sqrt(*(const number_t *)a)); }
 void num_scalar_asin(void *out, const void *a) { num_replace_value(out, num_asin(*(const number_t *)a)); }
 void num_scalar_acos(void *out, const void *a) { num_replace_value(out, num_acos(*(const number_t *)a)); }
 void num_scalar_atan(void *out, const void *a) { num_replace_value(out, num_atan(*(const number_t *)a)); }
 void num_scalar_asinh(void *out, const void *a) { num_replace_value(out, num_asinh(*(const number_t *)a)); }
 void num_scalar_acosh(void *out, const void *a) { num_replace_value(out, num_acosh(*(const number_t *)a)); }
 void num_scalar_atanh(void *out, const void *a) { num_replace_value(out, num_atanh(*(const number_t *)a)); }
 void num_scalar_erf(void *out, const void *a) { num_replace_value(out, num_erf(*(const number_t *)a)); }
 void num_scalar_erfc(void *out, const void *a) { num_replace_value(out, num_erfc(*(const number_t *)a)); }
 void num_scalar_erfinv(void *out, const void *a) { num_replace_value(out, num_erfinv(*(const number_t *)a)); }
 void num_scalar_erfcinv(void *out, const void *a) { num_replace_value(out, num_erfcinv(*(const number_t *)a)); }
 void num_scalar_gamma(void *out, const void *a) { num_replace_value(out, num_gamma(*(const number_t *)a)); }
 void num_scalar_lgamma(void *out, const void *a) { num_replace_value(out, num_lgamma(*(const number_t *)a)); }
 void num_scalar_digamma(void *out, const void *a) { num_replace_value(out, num_digamma(*(const number_t *)a)); }
 void num_scalar_trigamma(void *out, const void *a) { num_replace_value(out, num_trigamma(*(const number_t *)a)); }
 void num_scalar_tetragamma(void *out, const void *a) { num_replace_value(out, num_tetragamma(*(const number_t *)a)); }
 void num_scalar_gammainv(void *out, const void *a) { num_replace_value(out, num_gammainv(*(const number_t *)a)); }
 void num_scalar_normal_pdf(void *out, const void *a) { num_replace_value(out, num_normal_pdf(*(const number_t *)a)); }
 void num_scalar_normal_cdf(void *out, const void *a) { num_replace_value(out, num_normal_cdf(*(const number_t *)a)); }
 void num_scalar_normal_logpdf(void *out, const void *a) { num_replace_value(out, num_normal_logpdf(*(const number_t *)a)); }
 void num_scalar_lambert_w0(void *out, const void *a) { num_replace_value(out, num_lambert_w0(*(const number_t *)a)); }
 void num_scalar_lambert_wm1(void *out, const void *a) { num_replace_value(out, num_lambert_wm1(*(const number_t *)a)); }
 void num_scalar_productlog(void *out, const void *a) { num_replace_value(out, num_productlog(*(const number_t *)a)); }
 void num_scalar_ei(void *out, const void *a) { num_replace_value(out, num_ei(*(const number_t *)a)); }
 void num_scalar_e1(void *out, const void *a) { num_replace_value(out, num_e1(*(const number_t *)a)); }



/* ---------- expr_t* ---------- */

 void expr_add_wrap(void *o, const void *a, const void *b)
{
    const expr_t *lhs = (*(expr_t *const *)a) ? *(expr_t *const *)a : EXPR_ZERO;
    const expr_t *rhs = (*(expr_t *const *)b) ? *(expr_t *const *)b : EXPR_ZERO;
    expr_t *prev = *(expr_t **)o;
    expr_t *res = expr_add(lhs, rhs);

    if (prev)
        expr_free(prev);
    *(expr_t **)o = res;
}

 void expr_sub_wrap(void *o, const void *a, const void *b)
{
    const expr_t *lhs = (*(expr_t *const *)a) ? *(expr_t *const *)a : EXPR_ZERO;
    const expr_t *rhs = (*(expr_t *const *)b) ? *(expr_t *const *)b : EXPR_ZERO;
    expr_t *prev = *(expr_t **)o;
    expr_t *res = expr_sub(lhs, rhs);

    if (prev)
        expr_free(prev);
    *(expr_t **)o = res;
}

 void expr_mul_wrap(void *o, const void *a, const void *b)
{
    const expr_t *lhs = (*(expr_t *const *)a) ? *(expr_t *const *)a : EXPR_ZERO;
    const expr_t *rhs = (*(expr_t *const *)b) ? *(expr_t *const *)b : EXPR_ZERO;
    expr_t *prev = *(expr_t **)o;
    expr_t *res = expr_mul(lhs, rhs);

    if (prev)
        expr_free(prev);
    *(expr_t **)o = res;
}

 void expr_inv_wrap(void *o, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)o;
    expr_t *res = expr_num_div(&NUM_ONE, arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)o = res;
}

 int expr_cmp_wrap(const void *a, const void *b)
{
    expr_t *lhs = *(expr_t *const *)a;
    expr_t *rhs = *(expr_t *const *)b;

    if (!lhs && !rhs)
        return 0;
    if (!lhs)
        return -1;
    if (!rhs)
        return 1;
    return expr_cmp(lhs, rhs);
}

 void expr_print_wrap(const void *v, char *buf, size_t n)
{
    expr_t *dv = *(expr_t *const *)v;
    string_t *tmp_text;
    char *inner;
    char *tmp;
    char *sep;
    size_t len;

    if (!dv) {
        snprintf(buf, n, "NULL");
        return;
    }

    tmp_text = expr_to_text(dv, style_EXPRESSION);
    if (!tmp_text) {
        snprintf(buf, n, "<expr>");
        return;
    }
    tmp = strdup(string_c_str(tmp_text));
    string_free(tmp_text);
    if (!tmp) {
        snprintf(buf, n, "<expr>");
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

 void expr_conj_elem(void *o, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)o;

    if (prev)
        expr_free(prev);
    if (arg)
        expr_retain(arg);
    *(expr_t **)o = arg;
}

 void expr_scalar_exp(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_exp(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_log(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_log(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_sin(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_sin(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_cos(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_cos(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_tan(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_tan(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_sinh(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_sinh(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_cosh(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_cosh(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_tanh(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_tanh(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_sqrt(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_sqrt(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_asin(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_asin(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_acos(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_acos(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_atan(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_atan(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_asinh(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_asinh(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_acosh(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_acosh(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_atanh(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_atanh(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_erf(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_erf(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_erfc(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_erfc(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_erfinv(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_erfinv(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_erfcinv(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_erfcinv(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_gamma(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_gamma(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_gammainv(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_gammainv(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_lgamma(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_lgamma(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_digamma(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_digamma(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_trigamma(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_trigamma(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_normal_pdf(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_normal_pdf(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_normal_cdf(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_normal_cdf(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_normal_logpdf(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_normal_logpdf(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_lambert_w0(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_lambert_w0(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_lambert_wm1(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_lambert_wm1(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_productlog(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    /* ProductLog is the principal Lambert W branch. */
    expr_t *res = expr_lambert_w0(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_ei(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_ei(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}

 void expr_scalar_e1(void *out, const void *a)
{
    expr_t *arg = *(expr_t *const *)a;
    expr_t *prev = *(expr_t **)out;
    expr_t *res = expr_e1(arg);

    if (prev)
        expr_free(prev);
    *(expr_t **)out = res;
}



/* ============================================================
   Conversion helpers for mixed-type arithmetic
   ============================================================ */

static inline void num_as_expr(expr_t **out, const number_t *a) {
    *out = expr_new_const(*a);
}

static inline void id_expr(expr_t **out, expr_t *const *a) {
    *out = (expr_t *)((a && *a) ? *a : EXPR_ZERO);
}

/* ============================================================
   Cross-type arithmetic: add / sub / mul
   ============================================================ */

/* ---- number <-> expr ---- */

static void add_num_expr(void *out, const void *a, const void *b)
{
    expr_t *lhs = expr_new_const(*(const number_t *)a);
    expr_t *rhs;

    id_expr(&rhs, (expr_t *const *)b);
    *(expr_t **)out = expr_add(lhs, rhs);
    expr_free(lhs);
}

static void add_expr_num(void *out, const void *a, const void *b)
{
    expr_t *lhs;
    expr_t *rhs = expr_new_const(*(const number_t *)b);

    id_expr(&lhs, (expr_t *const *)a);
    *(expr_t **)out = expr_add(lhs, rhs);
    expr_free(rhs);
}

static void sub_num_expr(void *out, const void *a, const void *b)
{
    expr_t *lhs = expr_new_const(*(const number_t *)a);
    expr_t *rhs;

    id_expr(&rhs, (expr_t *const *)b);
    *(expr_t **)out = expr_sub(lhs, rhs);
    expr_free(lhs);
}

static void sub_expr_num(void *out, const void *a, const void *b)
{
    expr_t *lhs;
    expr_t *rhs = expr_new_const(*(const number_t *)b);

    id_expr(&lhs, (expr_t *const *)a);
    *(expr_t **)out = expr_sub(lhs, rhs);
    expr_free(rhs);
}

static void mul_num_expr(void *out, const void *a, const void *b)
{
    expr_t *lhs = expr_new_const(*(const number_t *)a);
    expr_t *rhs;

    id_expr(&rhs, (expr_t *const *)b);
    *(expr_t **)out = expr_mul(lhs, rhs);
    expr_free(lhs);
}

static void mul_expr_num(void *out, const void *a, const void *b)
{
    expr_t *lhs;
    expr_t *rhs = expr_new_const(*(const number_t *)b);

    id_expr(&lhs, (expr_t *const *)a);
    *(expr_t **)out = expr_mul(lhs, rhs);
    expr_free(rhs);
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

/* ---- expr <-> expr ---- */

static void add_expr_expr(void *out, const void *a, const void *b)
{
    const expr_t *lhs = (*(expr_t *const *)a) ? *(expr_t *const *)a : EXPR_ZERO;
    const expr_t *rhs = (*(expr_t *const *)b) ? *(expr_t *const *)b : EXPR_ZERO;
    *(expr_t **)out = expr_add(lhs, rhs);
}

static void sub_expr_expr(void *out, const void *a, const void *b)
{
    const expr_t *lhs = (*(expr_t *const *)a) ? *(expr_t *const *)a : EXPR_ZERO;
    const expr_t *rhs = (*(expr_t *const *)b) ? *(expr_t *const *)b : EXPR_ZERO;
    *(expr_t **)out = expr_sub(lhs, rhs);
}

static void mul_expr_expr(void *out, const void *a, const void *b)
{
    const expr_t *lhs = (*(expr_t *const *)a) ? *(expr_t *const *)a : EXPR_ZERO;
    const expr_t *rhs = (*(expr_t *const *)b) ? *(expr_t *const *)b : EXPR_ZERO;
    *(expr_t **)out = expr_mul(lhs, rhs);
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
    BIN_ELEM_EXPR = 1,
    BIN_ELEM_MAX
} binop_elem_kind;

static binop_elem_kind elem_binop_kind(const struct elem_vtable *elem)
{
    if (elem == &number_elem)
        return BIN_ELEM_NUMBER;
    if (elem == &expr_elem)
        return BIN_ELEM_EXPR;
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
        [BIN_ELEM_EXPR] = {
            .result_elem = &expr_elem,
            .add = add_num_expr,
            .sub = sub_num_expr,
            .mul = mul_num_expr
        }
    },

    [BIN_ELEM_EXPR] = {
        /* expr op number -> expr */
        [BIN_ELEM_NUMBER] = {
            .result_elem = &expr_elem,
            .add = add_expr_num,
            .sub = sub_expr_num,
            .mul = mul_expr_num
        },
        /* expr op expr -> expr */
        [BIN_ELEM_EXPR] = {
            .result_elem = &expr_elem,
            .add = add_expr_expr,
            .sub = sub_expr_expr,
            .mul = mul_expr_expr
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

matrix_t *mat_evaluate(const matrix_t *A)
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
            mat_set_num_owned(C, i, j, &value);
        }

    return C;
}

matrix_t *mat_scalar_mul(matrix_t *A, const number_t *s)
{
    if (!A || !s) return NULL;

    binop_elem_kind ak = elem_binop_kind(elem_of(A));
    const binop_vtable *op;
    const struct elem_vtable *re;
    matrix_t *R;
    unsigned char a_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0};
    unsigned char out_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0};

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

matrix_t *mat_scalar_div(matrix_t *A, const number_t *s)
{
    number_t inv;
    matrix_t *out;

    if (!s)
        return NULL;

    inv = num_inv(*s);
    out = mat_scalar_mul(A, &inv);
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
    unsigned char a_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0}, b_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0}, out[MATRIX_SCALAR_STORAGE_BYTES] = {0};

    if (!C)
        return NULL;

    elem_init_zero_value(re, out);

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
    unsigned char x_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0}, y_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0}, prod[MATRIX_SCALAR_STORAGE_BYTES] = {0};
    unsigned char sum[MATRIX_SCALAR_STORAGE_BYTES] = {0}, sum_acc[MATRIX_SCALAR_STORAGE_BYTES] = {0}, next_sum[MATRIX_SCALAR_STORAGE_BYTES] = {0};

    if (!re)
        return NULL;

    elem_init_zero_value(re, prod);
    elem_init_zero_value(re, sum_acc);
    elem_init_zero_value(re, next_sum);

    As = mat_uses_sparse_storage(A) ? (struct matrix_t *)A : mat_to_sparse(A);
    Bs = mat_uses_sparse_storage(B) ? (struct matrix_t *)B : mat_to_sparse(B);
    if (!As || !Bs) {
        elem_destroy_value(re, prod);
        elem_destroy_value(re, sum_acc);
        elem_destroy_value(re, next_sum);
        if (As != A)
            mat_free(As);
        if (Bs != B)
            mat_free(Bs);
        return NULL;
    }

    C = mat_create_sparse_with_elem(A->rows, B->cols, re);
    if (!C) {
        elem_destroy_value(re, prod);
        elem_destroy_value(re, sum_acc);
        elem_destroy_value(re, next_sum);
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
                re->add(next_sum, sum_acc, prod);
                elem_simplify_value(re, next_sum);
                elem_destroy_value(re, sum_acc);
                memcpy(sum_acc, next_sum, re->size);
                elem_init_zero_value(re, next_sum);
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
    elem_destroy_value(re, prod);
    elem_destroy_value(re, sum_acc);
    elem_destroy_value(re, next_sum);
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

    unsigned char a_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0}, b_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0}, out[MATRIX_SCALAR_STORAGE_BYTES] = {0};
    elem_init_zero_value(re, out);

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

    unsigned char a_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0}, b_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0}, out[MATRIX_SCALAR_STORAGE_BYTES] = {0};
    elem_init_zero_value(re, out);

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

    if (mat_is_sparse_like(A) && mat_is_sparse_like(B) && op->result_elem != &expr_elem)
        return mat_mul_sparse(A, B, op);

    const struct elem_vtable *re = op->result_elem;
    struct matrix_t *C = mat_create_binary_result(A->rows, B->cols, re, A, B);
    if (!C) return NULL;

    unsigned char x_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0}, y_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0}, prod[MATRIX_SCALAR_STORAGE_BYTES] = {0};
    unsigned char sum[MATRIX_SCALAR_STORAGE_BYTES] = {0}, next_sum[MATRIX_SCALAR_STORAGE_BYTES] = {0};
    elem_init_zero_value(re, prod);
    elem_init_zero_value(re, sum);
    elem_init_zero_value(re, next_sum);

    for (size_t i = 0; i < A->rows; i++) {
        for (size_t j = 0; j < B->cols; j++) {

            elem_copy_value(re, sum, re->zero);

            for (size_t k = 0; k < A->cols; k++) {
                mat_get(A, i, k, x_raw);
                mat_get(B, k, j, y_raw);
                op->mul(prod, x_raw, y_raw);
                re->add(next_sum, sum, prod);
                elem_simplify_value(re, next_sum);
                elem_destroy_value(re, sum);
                memcpy(sum, next_sum, re->size);
                elem_init_zero_value(re, next_sum);
                elem_destroy_value(re, prod);
            }

            mat_set(C, i, j, sum);
            elem_destroy_value(re, sum);
        }
    }

    elem_destroy_value(re, prod);
    elem_destroy_value(re, sum);
    elem_destroy_value(re, next_sum);

    if (re == &expr_elem && mat_simplify_symbolic_inplace(C) != 0) {
        mat_free(C);
        return NULL;
    }

    return C;
}

matrix_t *mat_neg(const matrix_t *A)
{
    const struct elem_vtable *e;
    matrix_t *R;
    unsigned char a_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0}, out_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0};

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
   Full, Hessenberg + implicit double-shift QR, number_t backend
   ============================================================ */

static number_t mat_eval_number_scalar_number(void (*scalar_f)(void *out, const void *in),
                                              const number_t *input)
{
    NUM_SCOPE(scope);
    number_t safe_input = input ? num_clone(*input) : num_clone(NUM_ZERO);
    number_t output = number_invalid();

    scalar_f(&output, &safe_input);
    return num_scope_detach(output);
}

static void mat_eval_number_scalar_elem(const struct elem_vtable *elem,
                                        void *out,
                                        void (*scalar_f)(void *out, const void *in),
                                        const void *in)
{
    number_t input = mat_raw_value_to_number(elem, in);
    number_t result = mat_eval_number_scalar_number(scalar_f, &input);
    mat_raw_value_from_number(elem, out, &result);
    num_destroy(&result);
    num_destroy(&input);
}

static void num_fun_coeffs_up_to_second(number_t *c0,
                                        number_t *c1,
                                        number_t *c2,
                                        void (*scalar_f)(void *out, const void *in),
                                        const number_t *lambda)
{
    NUM_SCOPE(scope);
    number_t f0 = mat_eval_number_scalar_number(scalar_f, lambda);
    if (c0)
        *c0 = num_scope_detach(num_clone(f0));

    if (scalar_f == number_elem.fun->gamma) {
        number_t psi = num_digamma(*lambda);
        number_t tri = num_trigamma(*lambda);
        if (c1)
            *c1 = num_scope_detach(num_mul(f0, psi));
        if (c2) {
            number_t psi2 = num_mul(psi, psi);
            number_t tri_plus = num_add(tri, psi2);
            number_t second = num_mul(f0, tri_plus);
            *c2 = num_scope_detach(num_mul(NUM_HALF, second));
        }
        return;
    }

    if (scalar_f == number_elem.fun->digamma) {
        if (c1)
            *c1 = num_scope_detach(num_trigamma(*lambda));
        if (c2)
        {
            number_t tetra = num_tetragamma(*lambda);
            *c2 = num_scope_detach(num_mul(NUM_HALF, tetra));
        }
        return;
    }

    if (scalar_f == number_elem.fun->lambert_w0 ||
        scalar_f == number_elem.fun->lambert_wm1) {
        number_t one = num_clone(NUM_ONE);
        number_t two = num_create_from_double(2.0);
        number_t wp1 = num_add(one, f0);
        number_t lam2 = num_mul(*lambda, *lambda);
        number_t denom1 = num_mul(*lambda, wp1);
        if (c1)
            *c1 = num_scope_detach(num_div(f0, denom1));
        if (c2) {
            number_t f02 = num_mul(f0, f0);
            number_t f0p2 = num_add(f0, two);
            number_t numer_core = num_mul(f02, f0p2);
            number_t numer = num_neg(numer_core);
            number_t wp12 = num_mul(wp1, wp1);
            number_t wp13 = num_mul(wp1, wp12);
            number_t denom = num_mul(lam2, wp13);
            number_t frac = num_div(numer, denom);
            *c2 = num_scope_detach(num_mul(NUM_HALF, frac));
        }
        return;
    }

    if (scalar_f == number_elem.fun->ei) {
        number_t exp_lambda = num_exp(*lambda);
        number_t one = num_clone(NUM_ONE);
        number_t lam2 = num_mul(*lambda, *lambda);
        if (c1)
            *c1 = num_scope_detach(num_div(exp_lambda, *lambda));
        if (c2) {
            number_t lam_m_one = num_sub(*lambda, one);
            number_t numer = num_mul(exp_lambda, lam_m_one);
            number_t second = num_div(numer, lam2);
            *c2 = num_scope_detach(num_mul(NUM_HALF, second));
        }
        return;
    }

    if (scalar_f == number_elem.fun->e1) {
        number_t one = num_clone(NUM_ONE);
        number_t neg_lambda = num_neg(*lambda);
        number_t emlambda = num_exp(neg_lambda);
        number_t lam2 = num_mul(*lambda, *lambda);
        if (c1)
        {
            number_t neg_emlambda = num_neg(emlambda);
            *c1 = num_scope_detach(num_div(neg_emlambda, *lambda));
        }
        if (c2) {
            number_t lam_p_one = num_add(*lambda, one);
            number_t numer = num_mul(emlambda, lam_p_one);
            number_t second = num_div(numer, lam2);
            *c2 = num_scope_detach(num_mul(NUM_HALF, second));
        }
        return;
    }

    if (scalar_f == number_elem.fun->erf) {
        number_t scale = num_clone(NUM_2_SQRTPI);
        number_t lambda2 = num_mul(*lambda, *lambda);
        number_t neg_lambda2 = num_neg(lambda2);
        number_t exp_term = num_exp(neg_lambda2);
        number_t fp = num_mul(scale, exp_term);
        if (c1)
            *c1 = num_scope_detach(num_clone(fp));
        if (c2)
        {
            number_t prod = num_mul(*lambda, fp);
            *c2 = num_scope_detach(num_neg(prod));
        }
        return;
    }

    if (scalar_f == number_elem.fun->erfc) {
        number_t scale = num_clone(NUM_NEG_TWO_OVER_SQRT_PI);
        number_t lambda2 = num_mul(*lambda, *lambda);
        number_t neg_lambda2 = num_neg(lambda2);
        number_t exp_term = num_exp(neg_lambda2);
        number_t fp = num_mul(scale, exp_term);
        if (c1)
            *c1 = num_scope_detach(num_clone(fp));
        if (c2)
        {
            number_t prod = num_mul(*lambda, fp);
            *c2 = num_scope_detach(num_neg(prod));
        }
        return;
    }

    if (scalar_f == number_elem.fun->normal_pdf) {
        number_t lambda_f0 = num_mul(*lambda, f0);
        number_t fp = num_neg(lambda_f0);
        if (c1)
            *c1 = num_scope_detach(num_clone(fp));
        if (c2) {
            number_t lambda2 = num_mul(*lambda, *lambda);
            number_t lambda2m1 = num_sub(lambda2, NUM_ONE);
            number_t core = num_mul(lambda2m1, f0);
            *c2 = num_scope_detach(num_mul(NUM_HALF, core));
        }
        return;
    }

    if (scalar_f == number_elem.fun->normal_cdf) {
        number_t pdf = num_normal_pdf(*lambda);
        if (c1)
            *c1 = num_scope_detach(num_clone(pdf));
        if (c2)
        {
            number_t lambda_pdf = num_mul(*lambda, pdf);
            number_t neg_half = num_create_from_double(-0.5);
            *c2 = num_scope_detach(num_mul(neg_half, lambda_pdf));
        }
        return;
    }

    if (scalar_f == number_elem.fun->normal_logpdf) {
        if (c1)
            *c1 = num_scope_detach(num_neg(*lambda));
        if (c2)
            *c2 = num_scope_detach(num_create_from_double(-0.5));
        return;
    }

    {
        number_t h = num_create_from_double(1e-6);
        number_t ih = num_mul(NUM_I, h);
        number_t fp, fm, lambda_p, lambda_m;
        number_t two = num_create_from_double(2.0);
        number_t denom1 = num_mul(ih, two);
        number_t h2 = num_mul(h, h);
        number_t neg_two = num_create_from_double(-2.0);
        number_t denom2 = num_mul(h2, neg_two);
        number_t two_f0 = num_mul(two, f0);

        lambda_p = num_add(*lambda, ih);
        lambda_m = num_sub(*lambda, ih);
        fp = mat_eval_number_scalar_number(scalar_f, &lambda_p);
        fm = mat_eval_number_scalar_number(scalar_f, &lambda_m);

        if (c1) {
            number_t numer1 = num_sub(fp, fm);
            *c1 = num_scope_detach(num_div(numer1, denom1));
        }
        if (c2) {
            number_t fp_minus = num_sub(fp, two_f0);
            number_t numer2 = num_add(fp_minus, fm);
            *c2 = num_scope_detach(num_div(numer2, denom2));
        }
    }
}

static void num_array_destroy(number_t *values, size_t count)
{
    if (!values)
        return;
    for (size_t i = 0; i < count; ++i)
        num_destroy(&values[i]);
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

    unsigned char raw[MATRIX_SCALAR_STORAGE_BYTES] = {0};
    number_t lambda = number_invalid();
    number_t c0 = number_invalid();
    number_t c1 = number_invalid();
    number_t c2 = number_invalid();
    mat_get(T, 0, 0, raw);
    lambda = mat_raw_value_to_number(e, raw);
    num_fun_coeffs_up_to_second(&c0, &c1, &c2, scalar_f, &lambda);

    for (size_t i = 0; i < n; ++i) {
        mat_set_num_clone(F, i, i, &c0);
        for (size_t j = i; j < n; ++j) {
            number_t tij = number_invalid();
            mat_get(T, i, j, raw);
            tij = mat_raw_value_to_number(e, raw);
            if (i == j) {
                num_destroy(&tij);
                tij = num_clone(NUM_ZERO);
            }
            mat_set_num_owned(N, i, j, &tij);
        }
    }

    if (n >= 2) {
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                number_t nij = number_invalid();
                number_t term = number_invalid();
                mat_get(N, i, j, raw);
                nij = mat_raw_value_to_number(e, raw);
                term = num_mul(c1, nij);
                mat_set_num_owned(F, i, j, &term);
                num_destroy(&nij);
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
                number_t fij = number_invalid();
                number_t n2ij = number_invalid();
                number_t term = number_invalid();
                number_t sum = number_invalid();
                mat_get(F, i, j, raw);
                fij = mat_raw_value_to_number(e, raw);
                mat_get(N2, i, j, raw);
                n2ij = mat_raw_value_to_number(e, raw);
                term = num_mul(c2, n2ij);
                sum = num_add(fij, term);
                mat_set_num_owned(F, i, j, &sum);
                num_destroy(&term);
                num_destroy(&n2ij);
                num_destroy(&fij);
            }
        }

        mat_free(N2);
    }
    num_destroy(&c2);
    num_destroy(&c1);
    num_destroy(&c0);
    num_destroy(&lambda);
    mat_free(N);
    return F;
}

static int num_divided_difference_perturbed(number_t *out,
                                            const number_t *nodes,
                                            size_t count,
                                            void (*scalar_f)(void *out, const void *in))
{
    number_t *pts = NULL;
    number_t *table = NULL;
    number_t base_step = number_invalid();
    number_t dup_tol = number_invalid();
    number_t one = number_invalid();

    if (!out || !nodes || count == 0 || !scalar_f)
        return -1;

    pts = calloc(count, sizeof(*pts));
    table = calloc(count * count, sizeof(*table));
    if (!pts || !table) {
        free(pts);
        free(table);
        return -1;
    }

    for (size_t i = 0; i < count; ++i)
        pts[i] = number_invalid();
    for (size_t i = 0; i < count * count; ++i)
        table[i] = number_invalid();

    base_step = num_create_from_double(1e-12);
    dup_tol = num_create_from_double(1e-24);
    one = num_clone(NUM_ONE);
    for (size_t d = 0; d < count; ++d) {
        size_t dup_index = 0;
        number_t scale = number_invalid();
        number_t step = number_invalid();
        number_t dup_num = number_invalid();
        number_t shift = number_invalid();
        number_t shifted = number_invalid();

        pts[d] = num_clone(nodes[d]);
        for (size_t p = 0; p < d; ++p) {
            number_t diff = num_sub(nodes[d], nodes[p]);
            number_t absdiff = num_abs(diff);
            if (num_lt(absdiff, dup_tol))
                dup_index++;
            num_destroy(&absdiff);
            num_destroy(&diff);
        }

        if (dup_index > 0) {
            scale = num_abs(nodes[d]);
            if (num_lt(scale, one)) {
                num_destroy(&scale);
                scale = num_clone(one);
            }
            step = num_mul(base_step, scale);
            dup_num = num_create_from_long((long)dup_index);
            shift = num_mul(step, dup_num);
            shifted = num_add(pts[d], shift);
            num_destroy(&pts[d]);
            pts[d] = shifted;
            num_destroy(&shift);
            num_destroy(&dup_num);
            num_destroy(&step);
            num_destroy(&scale);
        }
    }

    for (size_t i = 0; i < count; ++i)
        table[i * count] = mat_eval_number_scalar_number(scalar_f, &pts[i]);

    for (size_t order = 1; order < count; ++order) {
        for (size_t i = 0; i + order < count; ++i) {
            number_t numer = number_invalid();
            number_t denom = number_invalid();
            number_t quot = number_invalid();

            numer = num_sub(table[(i + 1) * count + (order - 1)],
                            table[i * count + (order - 1)]);
            denom = num_sub(pts[i + order], pts[i]);
            quot = num_div(numer, denom);
            table[i * count + order] = quot;
            num_destroy(&denom);
            num_destroy(&numer);
        }
    }

    *out = num_clone(table[count - 1]);
    num_destroy(&one);
    num_destroy(&dup_tol);
    num_destroy(&base_step);
    num_array_destroy(table, count * count);
    num_array_destroy(pts, count);
    free(table);
    free(pts);
    return 0;
}

static int num_divided_difference_ordinary(number_t *out,
                                           const number_t *nodes,
                                           size_t count,
                                           void (*scalar_f)(void *out, const void *in))
{
    number_t *table = NULL;

    if (!out || !nodes || count == 0 || !scalar_f)
        return -1;

    table = calloc(count * count, sizeof(*table));
    if (!table)
        return -1;

    for (size_t i = 0; i < count * count; ++i)
        table[i] = number_invalid();

    for (size_t i = 0; i < count; ++i)
        table[i * count] = mat_eval_number_scalar_number(scalar_f, &nodes[i]);

    for (size_t order = 1; order < count; ++order) {
        for (size_t i = 0; i + order < count; ++i) {
            number_t numer = num_sub(table[(i + 1) * count + (order - 1)],
                                     table[i * count + (order - 1)]);
            number_t denom = num_sub(nodes[i + order], nodes[i]);
            table[i * count + order] = num_div(numer, denom);
            num_destroy(&denom);
            num_destroy(&numer);
        }
    }

    *out = num_clone(table[count - 1]);
    num_array_destroy(table, count * count);
    free(table);
    return 0;
}

static int num_divided_difference_confluent(number_t *out,
                                            const number_t *nodes,
                                            size_t count,
                                            void (*scalar_f)(void *out, const void *in))
{
    NUM_SCOPE(scope);

    if (!out || !nodes || count == 0 || !scalar_f)
        return -1;

    if (count == 1) {
        *out = num_scope_detach(mat_eval_number_scalar_number(scalar_f, &nodes[0]));
        return 0;
    }

    if (count == 2 && num_eq(nodes[0], nodes[1])) {
        number_t c1 = number_invalid();
        num_fun_coeffs_up_to_second(NULL, &c1, NULL, scalar_f, &nodes[0]);
        *out = num_scope_detach(c1);
        return 0;
    }

    if (count == 3) {
        bool eq01 = num_eq(nodes[0], nodes[1]);
        bool eq12 = num_eq(nodes[1], nodes[2]);
        bool eq02 = num_eq(nodes[0], nodes[2]);

        if (eq01 && eq12) {
            number_t c2 = number_invalid();
            num_fun_coeffs_up_to_second(NULL, NULL, &c2, scalar_f, &nodes[0]);
            *out = num_scope_detach(c2);
            return 0;
        }

        if (eq02) {
            number_t fa = mat_eval_number_scalar_number(scalar_f, &nodes[0]);
            number_t fb = mat_eval_number_scalar_number(scalar_f, &nodes[1]);
            number_t fp = number_invalid();
            number_t a_minus_b = num_sub(nodes[0], nodes[1]);
            number_t fa_minus_fb = num_sub(fa, fb);
            number_t fp_scaled = number_invalid();
            number_t numer = number_invalid();
            number_t denom = num_mul(a_minus_b, a_minus_b);

            num_fun_coeffs_up_to_second(NULL, &fp, NULL, scalar_f, &nodes[0]);
            fp_scaled = num_mul(fp, a_minus_b);
            numer = num_sub(fp_scaled, fa_minus_fb);
            *out = num_scope_detach(num_div(numer, denom));
            return 0;
        }

        if (eq01) {
            number_t fa = mat_eval_number_scalar_number(scalar_f, &nodes[0]);
            number_t fb = mat_eval_number_scalar_number(scalar_f, &nodes[2]);
            number_t fp = number_invalid();
            number_t b_minus_a = num_sub(nodes[2], nodes[0]);
            number_t fb_minus_fa = num_sub(fb, fa);
            number_t first_dd = num_div(fb_minus_fa, b_minus_a);
            number_t numer = number_invalid();

            num_fun_coeffs_up_to_second(NULL, &fp, NULL, scalar_f, &nodes[0]);
            numer = num_sub(first_dd, fp);
            *out = num_scope_detach(num_div(numer, b_minus_a));
            return 0;
        }

        if (eq12) {
            number_t fa = mat_eval_number_scalar_number(scalar_f, &nodes[0]);
            number_t fb = mat_eval_number_scalar_number(scalar_f, &nodes[1]);
            number_t fp = number_invalid();
            number_t b_minus_a = num_sub(nodes[1], nodes[0]);
            number_t fb_minus_fa = num_sub(fb, fa);
            number_t first_dd = num_div(fb_minus_fa, b_minus_a);
            number_t numer = number_invalid();

            num_fun_coeffs_up_to_second(NULL, &fp, NULL, scalar_f, &nodes[1]);
            numer = num_sub(fp, first_dd);
            *out = num_scope_detach(num_div(numer, b_minus_a));
            return 0;
        }
    }

    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i + 1; j < count; ++j) {
            if (num_eq(nodes[i], nodes[j]))
                return num_divided_difference_perturbed(out, nodes, count, scalar_f);
        }
    }

    return num_divided_difference_ordinary(out, nodes, count, scalar_f);
}

static int mat_fun_triangular_confluent_sum_paths(number_t *out,
                                                  const matrix_t *T,
                                                  size_t start,
                                                  size_t current,
                                                  size_t end,
                                                  number_t edge_prod,
                                                  number_t *nodes,
                                                  size_t depth,
                                                  void (*scalar_f)(void *out, const void *in))
{
    const struct elem_vtable *e = T->elem;
    number_t contrib = number_invalid();
    unsigned char raw[MATRIX_SCALAR_STORAGE_BYTES] = {0};

    if (!out || !T || !nodes || !scalar_f || current > end)
        return -1;

    (void)start;

    if (current == end) {
        if (num_divided_difference_confluent(&contrib, nodes, depth, scalar_f) != 0)
            return -1;
        {
            number_t prod = num_mul(edge_prod, contrib);
            number_t next_out = num_add(*out, prod);
            num_destroy(&prod);
            num_destroy(&contrib);
            num_destroy(out);
            *out = next_out;
        }
        return 0;
    }

    for (size_t next = current + 1; next <= end; ++next) {
        number_t tij = number_invalid();
        number_t lam_next = number_invalid();
        number_t next_prod = number_invalid();
        number_t abs_tij = number_invalid();
        number_t zero_tol = number_invalid();

        mat_get(T, current, next, raw);
        tij = mat_raw_value_to_number(e, raw);
        abs_tij = num_abs(tij);
        zero_tol = num_create_from_double(1e-30);
        if (num_lt(abs_tij, zero_tol)) {
            num_destroy(&zero_tol);
            num_destroy(&abs_tij);
            num_destroy(&tij);
            continue;
        }
        num_destroy(&zero_tol);
        num_destroy(&abs_tij);

        mat_get(T, next, next, raw);
        lam_next = mat_raw_value_to_number(e, raw);
        num_destroy(&nodes[depth]);
        nodes[depth] = num_clone(lam_next);
        next_prod = num_mul(edge_prod, tij);
        if (mat_fun_triangular_confluent_sum_paths(out, T, start, next, end,
                                                   next_prod, nodes, depth + 1,
                                                   scalar_f) != 0) {
            num_destroy(&next_prod);
            num_destroy(&lam_next);
            num_destroy(&tij);
            return -1;
        }
        num_destroy(&next_prod);
        num_destroy(&lam_next);
        num_destroy(&tij);
    }

    return 0;
}

static int mat_fun_triangular_confluent_fallback(number_t *out,
                                                 const matrix_t *T,
                                                 size_t i,
                                                 size_t j,
                                                 void (*scalar_f)(void *out, const void *in))
{
    const struct elem_vtable *e = T->elem;
    number_t *nodes = NULL;
    unsigned char raw[MATRIX_SCALAR_STORAGE_BYTES] = {0};

    if (!out || !T || !scalar_f || i >= j || j >= T->rows || T->rows != T->cols)
        return -1;

    nodes = calloc(j - i + 1, sizeof(*nodes));
    if (!nodes)
        return -1;
    for (size_t idx = 0; idx < (j - i + 1); ++idx)
        nodes[idx] = number_invalid();

    num_destroy(out);
    *out = num_clone(NUM_ZERO);
    mat_get(T, i, i, raw);
    nodes[0] = mat_raw_value_to_number(e, raw);
    {
        number_t edge_prod = num_clone(NUM_ONE);
        if (mat_fun_triangular_confluent_sum_paths(out, T, i, i, j, edge_prod,
                                                   nodes, 1, scalar_f) != 0) {
            num_destroy(&edge_prod);
            num_array_destroy(nodes, j - i + 1);
            free(nodes);
            return -1;
        }
        num_destroy(&edge_prod);
    }

    num_array_destroy(nodes, j - i + 1);
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

    unsigned char t_ii[MATRIX_SCALAR_STORAGE_BYTES] = {0}, t_jj[MATRIX_SCALAR_STORAGE_BYTES] = {0}, t_ij[MATRIX_SCALAR_STORAGE_BYTES] = {0};
    unsigned char f_ii[MATRIX_SCALAR_STORAGE_BYTES] = {0}, f_jj[MATRIX_SCALAR_STORAGE_BYTES] = {0}, f_ij[MATRIX_SCALAR_STORAGE_BYTES] = {0};
    unsigned char num[MATRIX_SCALAR_STORAGE_BYTES] = {0}, tmp[MATRIX_SCALAR_STORAGE_BYTES] = {0}, sum[MATRIX_SCALAR_STORAGE_BYTES] = {0};
    unsigned char t_ik[MATRIX_SCALAR_STORAGE_BYTES] = {0}, t_kj[MATRIX_SCALAR_STORAGE_BYTES] = {0}, f_ik[MATRIX_SCALAR_STORAGE_BYTES] = {0}, f_kj[MATRIX_SCALAR_STORAGE_BYTES] = {0};
    unsigned char denom[MATRIX_SCALAR_STORAGE_BYTES] = {0}, inv_denom[MATRIX_SCALAR_STORAGE_BYTES] = {0};

    bool all_diag_equal = true;
    number_t lam0 = number_invalid();
    number_t tol = num_create_from_double(1e-24);
    unsigned char lam0_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0}, lami_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0};
    mat_get(T, 0, 0, lam0_raw);
    lam0 = mat_raw_value_to_number(e, lam0_raw);
    for (size_t i = 1; i < n; ++i) {
        number_t lami = number_invalid();
        number_t diff = number_invalid();
        number_t absdiff = number_invalid();

        mat_get(T, i, i, lami_raw);
        lami = mat_raw_value_to_number(e, lami_raw);
        diff = num_sub(lami, lam0);
        absdiff = num_abs(diff);
        if (num_lt(tol, absdiff)) {
            all_diag_equal = false;
            num_destroy(&absdiff);
            num_destroy(&diff);
            num_destroy(&lami);
            break;
        }
        num_destroy(&absdiff);
        num_destroy(&diff);
        num_destroy(&lami);
    }
    num_destroy(&lam0);
    num_destroy(&tol);
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
                number_t fallback = number_invalid();

                if (mat_fun_triangular_confluent_fallback(&fallback, T, i, j, scalar_f) == 0) {
                    mat_raw_value_from_number(e, f_ij, &fallback);
                    mat_set(F, i, j, f_ij);
                    num_destroy(&fallback);
                    elem_destroy_value(e, f_ij);
                    elem_destroy_value(e, denom);
                    elem_destroy_value(e, num);
                    elem_destroy_value(e, tmp);
                    elem_destroy_value(e, sum);
                    continue;
                }

                if (e->cmp(num, e->zero) == 0) {
                    /* F[i,j] = f'(lambda) * T[i,j] via central difference */
                    unsigned char lam_p[MATRIX_SCALAR_STORAGE_BYTES] = {0}, lam_m[MATRIX_SCALAR_STORAGE_BYTES] = {0};
                    unsigned char fp[MATRIX_SCALAR_STORAGE_BYTES] = {0}, fm[MATRIX_SCALAR_STORAGE_BYTES] = {0};
                    unsigned char h_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0}, two_h[MATRIX_SCALAR_STORAGE_BYTES] = {0};
                    unsigned char inv_2h[MATRIX_SCALAR_STORAGE_BYTES] = {0}, deriv[MATRIX_SCALAR_STORAGE_BYTES] = {0};
                    {
                        number_t h_num = num_create_from_double(1e-10);
                        mat_raw_value_from_number(e, h_raw, &h_num);
                        num_destroy(&h_num);
                    }
                    e->add(lam_p, t_ii, h_raw);
                    e->sub(lam_m, t_ii, h_raw);
                    mat_eval_number_scalar_elem(e, fp, scalar_f, lam_p);
                    mat_eval_number_scalar_elem(e, fm, scalar_f, lam_m);
                    e->sub(deriv, fp, fm);
                    {
                        number_t two_h_num = num_create_from_double(2e-10);
                        mat_raw_value_from_number(e, two_h, &two_h_num);
                        num_destroy(&two_h_num);
                    }
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
