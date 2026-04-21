#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "matrix.h"
#include "qfloat.h"
#include "qcomplex.h"

/* ============================================================
   Forward declarations of internal vtables
   ============================================================ */

struct elem_vtable;
struct store_vtable;

/* ============================================================
   Matrix object (opaque in matrix.h)
   ============================================================ */

struct matrix_t {
    size_t rows;
    size_t cols;

    const struct elem_vtable  *elem;
    const struct store_vtable *store;

    void **data;   /* row pointers for dense; NULL for identity */
};

/* ============================================================
   Element vtable (double, qfloat, qcomplex)
   ============================================================ */

struct elem_vtable {
    size_t size;

    void (*add)(void *out, const void *a, const void *b);
    void (*sub)(void *out, const void *a, const void *b);
    void (*mul)(void *out, const void *a, const void *b);
    void (*inv)(void *out, const void *a);

    const void *zero;
    const void *one;

    void (*print)(const void *val, char *buf, size_t buflen);
};

/* ---------- double ---------- */

static const double D_ZERO = 0.0;
static const double D_ONE  = 1.0;

static void d_add(void *o, const void *a, const void *b) { *(double*)o = *(const double*)a + *(const double*)b; }
static void d_sub(void *o, const void *a, const void *b) { *(double*)o = *(const double*)a - *(const double*)b; }
static void d_mul(void *o, const void *a, const void *b) { *(double*)o = *(const double*)a * *(const double*)b; }
static void d_inv(void *o, const void *a) { *(double*)o = 1.0 / *(const double*)a; }
static void d_print(const void *v, char *buf, size_t n) { snprintf(buf, n, "%.16g", *(const double*)v); }

static const struct elem_vtable double_elem = {
    .size  = sizeof(double),
    .add   = d_add,
    .sub   = d_sub,
    .mul   = d_mul,
    .inv   = d_inv,
    .zero  = &D_ZERO,
    .one   = &D_ONE,
    .print = d_print
};

/* ---------- qfloat ---------- */

static const qfloat_t QF_ZERO = {0};
static const qfloat_t QF_ONE  = { .hi = 1.0, .lo = 0.0 };

static void qf_add_wrap(void *o, const void *a, const void *b) { *(qfloat_t*)o = qf_add(*(const qfloat_t*)a, *(const qfloat_t*)b); }
static void qf_sub_wrap(void *o, const void *a, const void *b) { *(qfloat_t*)o = qf_sub(*(const qfloat_t*)a, *(const qfloat_t*)b); }
static void qf_mul_wrap(void *o, const void *a, const void *b) { *(qfloat_t*)o = qf_mul(*(const qfloat_t*)a, *(const qfloat_t*)b); }
static void qf_inv_wrap(void *o, const void *a) { *(qfloat_t*)o = qf_div(QF_ONE, *(const qfloat_t*)a); }
static void qf_print_wrap(const void *v, char *buf, size_t n) { qf_to_string(*(const qfloat_t*)v, buf, n); }

static const struct elem_vtable qfloat_elem = {
    .size  = sizeof(qfloat_t),
    .add   = qf_add_wrap,
    .sub   = qf_sub_wrap,
    .mul   = qf_mul_wrap,
    .inv   = qf_inv_wrap,
    .zero  = &QF_ZERO,
    .one   = &QF_ONE,
    .print = qf_print_wrap
};

/* ---------- qcomplex ---------- */

static const qcomplex_t QC_ZERO = { .re = QF_ZERO, .im = QF_ZERO };
static const qcomplex_t QC_ONE  = { .re = QF_ONE,  .im = QF_ZERO };

static qcomplex_t qc_inv(qcomplex_t z)
{
    /* 1/z = conj(z) / (re^2 + im^2) */
    qfloat_t denom = qf_add(qf_mul(z.re, z.re), qf_mul(z.im, z.im));

    qfloat_t re = qf_div(z.re, denom);
    qfloat_t im = qf_neg(qf_div(z.im, denom));

    return qc_make(re, im);
}

static void qc_add_wrap(void *o, const void *a, const void *b) { *(qcomplex_t*)o = qc_add(*(const qcomplex_t*)a, *(const qcomplex_t*)b); }
static void qc_sub_wrap(void *o, const void *a, const void *b) { *(qcomplex_t*)o = qc_sub(*(const qcomplex_t*)a, *(const qcomplex_t*)b); }
static void qc_mul_wrap(void *o, const void *a, const void *b) { *(qcomplex_t*)o = qc_mul(*(const qcomplex_t*)a, *(const qcomplex_t*)b); }
static void qc_inv_wrap(void *o, const void *a) { *(qcomplex_t*)o = qc_inv(*(const qcomplex_t*)a); }
static void qc_print_wrap(const void *v, char *buf, size_t n) { qc_to_string(*(const qcomplex_t*)v, buf, n); }

static const struct elem_vtable qcomplex_elem = {
    .size  = sizeof(qcomplex_t),
    .add   = qc_add_wrap,
    .sub   = qc_sub_wrap,
    .mul   = qc_mul_wrap,
    .inv   = qc_inv_wrap,
    .zero  = &QC_ZERO,
    .one   = &QC_ONE,
    .print = qc_print_wrap
};

/* ============================================================
   Storage vtable (dense, identity)
   ============================================================ */

struct store_vtable {
    void (*alloc)(matrix_t *A);
    void (*free)(matrix_t *A);

    void (*get)(const matrix_t *A, size_t i, size_t j, void *out);
    void (*set)(matrix_t *A, size_t i, size_t j, const void *val);

    void (*materialise)(matrix_t *A);
};

/* ---------- dense storage (row-pointer) ---------- */

static void dense_alloc(matrix_t *A) {
    size_t n = A->rows, m = A->cols, es = A->elem->size;

    A->data = malloc(n * sizeof(void*));
    if (!A->data) return;
    for (size_t i = 0; i < n; i++) {
        A->data[i] = calloc(m, es);
        if (!A->data[i]) {
            for (size_t k = 0; k < i; k++) free(A->data[k]);
            free(A->data);
            A->data = NULL;
            return;
        }
    }
}

static void dense_free(matrix_t *A) {
    if (!A->data) return;
    for (size_t i = 0; i < A->rows; i++)
        free(A->data[i]);
    free(A->data);
    A->data = NULL;
}

static void dense_get(const matrix_t *A, size_t i, size_t j, void *out) {
    memcpy(out,
           (char*)A->data[i] + j * A->elem->size,
           A->elem->size);
}

static void dense_set(matrix_t *A, size_t i, size_t j, const void *val) {
    memcpy((char*)A->data[i] + j * A->elem->size,
           val,
           A->elem->size);
}

static const struct store_vtable dense_store = {
    .alloc        = dense_alloc,
    .free         = dense_free,
    .get          = dense_get,
    .set          = dense_set,
    .materialise  = NULL
};

/* ---------- identity storage ---------- */

static void ident_alloc(matrix_t *A) {
    A->data = NULL;
}

static void ident_free(matrix_t *A) {
    (void)A;
}

static void ident_get(const matrix_t *A, size_t i, size_t j, void *out) {
    memcpy(out,
           (i == j) ? A->elem->one : A->elem->zero,
           A->elem->size);
}

static void ident_materialise(matrix_t *A);

static void ident_set(matrix_t *A, size_t i, size_t j, const void *val) {
    ident_materialise(A);
    dense_set(A, i, j, val);
}

static const struct store_vtable identity_store = {
    .alloc        = ident_alloc,
    .free         = ident_free,
    .get          = ident_get,
    .set          = ident_set,
    .materialise  = ident_materialise
};

static void ident_materialise(matrix_t *A) {
    if (A->store == &dense_store)
        return;

    A->store = &dense_store;
    dense_alloc(A);
    if (!A->data) return;

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++)
            dense_set(A, i, j,
                      (i == j) ? A->elem->one : A->elem->zero);
}

/* ============================================================
   Internal constructor helper
   ============================================================ */

static matrix_t *mat_create_internal(size_t rows, size_t cols,
                                     const struct elem_vtable *elem,
                                     const struct store_vtable *store)
{
    matrix_t *A = malloc(sizeof(*A));
    if (!A) return NULL;

    A->rows  = rows;
    A->cols  = cols;
    A->elem  = elem;
    A->store = store;
    A->data  = NULL;

    A->store->alloc(A);
    if (store == &dense_store && !A->data) {
        free(A);
        return NULL;
    }
    return A;
}

/* ============================================================
   Public constructors
   ============================================================ */

matrix_t *mat_create_d(size_t rows, size_t cols) {
    return mat_create_internal(rows, cols, &double_elem, &dense_store);
}

matrix_t *mat_create_qf(size_t rows, size_t cols) {
    return mat_create_internal(rows, cols, &qfloat_elem, &dense_store);
}

matrix_t *mat_create_qc(size_t rows, size_t cols) {
    return mat_create_internal(rows, cols, &qcomplex_elem, &dense_store);
}

matrix_t *matsq_create_d(size_t n)  { return mat_create_d(n, n); }
matrix_t *matsq_create_qf(size_t n) { return mat_create_qf(n, n); }
matrix_t *matsq_create_qc(size_t n) { return mat_create_qc(n, n); }

matrix_t *matsq_ident_d(size_t n) {
    return mat_create_internal(n, n, &double_elem, &identity_store);
}

matrix_t *matsq_ident_qf(size_t n) {
    return mat_create_internal(n, n, &qfloat_elem, &identity_store);
}

matrix_t *matsq_ident_qc(size_t n) {
    return mat_create_internal(n, n, &qcomplex_elem, &identity_store);
}

/* ============================================================
   Destruction
   ============================================================ */

void mat_free(matrix_t *A) {
    if (!A) return;
    A->store->free(A);
    free(A);
}

/* ============================================================
   Element access
   ============================================================ */

void mat_get(const matrix_t *A, size_t i, size_t j, void *out) {
    A->store->get(A, i, j, out);
}

void mat_set(matrix_t *A, size_t i, size_t j, const void *val) {
    A->store->set(A, i, j, val);
}

size_t mat_get_row_count(const matrix_t *A)
{
    return A->rows;
}

size_t mat_get_col_count(const matrix_t *A)
{
    return A->cols;
}

/* ============================================================
   Basic operations
   ============================================================ */

static const struct elem_vtable *elem_of(const matrix_t *A) {
    return A->elem;
}

matrix_t *mat_add(const matrix_t *A, const matrix_t *B) {
    if (!A || !B || A->rows != B->rows || A->cols != B->cols || A->elem != B->elem)
        return NULL;

    const struct elem_vtable *e = elem_of(A);
    matrix_t *C;

    if (e == &double_elem)      C = mat_create_d(A->rows, A->cols);
    else if (e == &qfloat_elem) C = mat_create_qf(A->rows, A->cols);
    else                        C = mat_create_qc(A->rows, A->cols);

    if (!C) return NULL;

    unsigned char a[64], b[64], out[64];

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a);
            mat_get(B, i, j, b);
            e->add(out, a, b);
            mat_set(C, i, j, out);
        }

    return C;
}

matrix_t *mat_sub(const matrix_t *A, const matrix_t *B) {
    if (!A || !B || A->rows != B->rows || A->cols != B->cols || A->elem != B->elem)
        return NULL;

    const struct elem_vtable *e = elem_of(A);
    matrix_t *C;

    if (e == &double_elem)      C = mat_create_d(A->rows, A->cols);
    else if (e == &qfloat_elem) C = mat_create_qf(A->rows, A->cols);
    else                        C = mat_create_qc(A->rows, A->cols);

    if (!C) return NULL;

    unsigned char a[64], b[64], out[64];

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a);
            mat_get(B, i, j, b);
            e->sub(out, a, b);
            mat_set(C, i, j, out);
        }

    return C;
}

matrix_t *mat_mul(const matrix_t *A, const matrix_t *B) {
    if (!A || !B || A->cols != B->rows || A->elem != B->elem)
        return NULL;

    const struct elem_vtable *e = elem_of(A);
    matrix_t *C;

    if (e == &double_elem)      C = mat_create_d(A->rows, B->cols);
    else if (e == &qfloat_elem) C = mat_create_qf(A->rows, B->cols);
    else                        C = mat_create_qc(A->rows, B->cols);

    if (!C) return NULL;

    unsigned char x[64], y[64], tmp[64], sum[64];

    for (size_t i = 0; i < A->rows; i++) {
        for (size_t j = 0; j < B->cols; j++) {

            memcpy(sum, e->zero, e->size);

            for (size_t k = 0; k < A->cols; k++) {
                mat_get(A, i, k, x);
                mat_get(B, k, j, y);
                e->mul(tmp, x, y);
                e->add(sum, sum, tmp);
            }

            mat_set(C, i, j, sum);
        }
    }

    return C;
}

matrix_t *mat_transpose(const matrix_t *A) {
    if (!A) return NULL;

    const struct elem_vtable *e = elem_of(A);
    matrix_t *T;

    if (e == &double_elem)      T = mat_create_d(A->cols, A->rows);
    else if (e == &qfloat_elem) T = mat_create_qf(A->cols, A->rows);
    else                        T = mat_create_qc(A->cols, A->rows);

    if (!T) return NULL;

    unsigned char v[64];

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, v);
            mat_set(T, j, i, v);
        }

    return T;
}

matrix_t *mat_conj(const matrix_t *A) {
    if (!A) return NULL;

    const struct elem_vtable *e = elem_of(A);
    matrix_t *C;

    if (e == &double_elem)      C = mat_create_d(A->rows, A->cols);
    else if (e == &qfloat_elem) C = mat_create_qf(A->rows, A->cols);
    else                        C = mat_create_qc(A->rows, A->cols);

    if (!C) return NULL;

    unsigned char v[64];

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, v);
            if (e == &qcomplex_elem) {
                qcomplex_t z = *(qcomplex_t*)v;
                z = qc_conj(z);
                mat_set(C, i, j, &z);
            } else {
                mat_set(C, i, j, v);
            }
        }

    return C;
}

/* ============================================================
   Debug printing
   ============================================================ */

void mat_print(const matrix_t *A) {
    if (!A) {
        printf("(null)\n");
        return;
    }

    char buf[128];
    unsigned char v[64];

    for (size_t i = 0; i < A->rows; i++) {
        printf("[");
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, v);
            A->elem->print(v, buf, sizeof(buf));
            printf(" %s", buf);
        }
        printf(" ]\n");
    }
}
