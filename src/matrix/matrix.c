#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "matrix.h"
#include "qfloat.h"
#include "qcomplex.h"

/* ============================================================
   Element kinds
   ============================================================ */

typedef enum {
    ELEM_DOUBLE = 0,
    ELEM_QFLOAT = 1,
    ELEM_QCOMPLEX = 2,
    ELEM_MAX
} elem_kind;

/* Forward declarations */
struct elem_vtable;
struct store_vtable;
struct matrix_t;

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
   Element vtable (extended)
   ============================================================ */

struct elem_vtable {
    size_t size;
    elem_kind kind;

    /* arithmetic */
    void (*add)(void *out, const void *a, const void *b);
    void (*sub)(void *out, const void *a, const void *b);
    void (*mul)(void *out, const void *a, const void *b);
    void (*inv)(void *out, const void *a);

    /* constants */
    const void *zero;
    const void *one;

    /* printing */
    void (*print)(const void *val, char *buf, size_t buflen);

    /* matrix constructors */
    struct matrix_t *(*create_matrix)(size_t rows, size_t cols);
    struct matrix_t *(*create_identity)(size_t n);
};

/* ============================================================
   Storage vtable (dense, identity)
   ============================================================ */

struct store_vtable {
    void (*alloc)(struct matrix_t *A);
    void (*free)(struct matrix_t *A);

    void (*get)(const struct matrix_t *A, size_t i, size_t j, void *out);
    void (*set)(struct matrix_t *A, size_t i, size_t j, const void *val);

    void (*materialise)(struct matrix_t *A);
};

/* ---------- dense storage (row-pointer) ---------- */

static void dense_alloc(struct matrix_t *A) {
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

static void dense_free(struct matrix_t *A) {
    if (!A->data) return;
    for (size_t i = 0; i < A->rows; i++)
        free(A->data[i]);
    free(A->data);
    A->data = NULL;
}

static void dense_get(const struct matrix_t *A, size_t i, size_t j, void *out) {
    memcpy(out,
           (char*)A->data[i] + j * A->elem->size,
           A->elem->size);
}

static void dense_set(struct matrix_t *A, size_t i, size_t j, const void *val) {
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

static void ident_materialise(struct matrix_t *A);

static void ident_alloc(struct matrix_t *A) {
    A->data = NULL;
}

static void ident_free(struct matrix_t *A) {
    (void)A;
}

static void ident_get(const struct matrix_t *A, size_t i, size_t j, void *out) {
    memcpy(out,
           (i == j) ? A->elem->one : A->elem->zero,
           A->elem->size);
}

static void ident_set(struct matrix_t *A, size_t i, size_t j, const void *val) {
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

static void ident_materialise(struct matrix_t *A) {
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

    A->store->alloc(A);
    if (store == &dense_store && !A->data) {
        free(A);
        return NULL;
    }
    return A;
}

/* ============================================================
   Element-type-specific matrix constructors
   ============================================================ */

static struct matrix_t *create_matrix_double(size_t r, size_t c);
static struct matrix_t *create_identity_double(size_t n);
static struct matrix_t *create_matrix_qfloat(size_t r, size_t c);
static struct matrix_t *create_identity_qfloat(size_t n);
static struct matrix_t *create_matrix_qcomplex(size_t r, size_t c);
static struct matrix_t *create_identity_qcomplex(size_t n);

static struct matrix_t *create_matrix_double(size_t r, size_t c) {
    extern const struct elem_vtable double_elem;
    return mat_create_internal(r, c, &double_elem, &dense_store);
}

static struct matrix_t *create_identity_double(size_t n) {
    extern const struct elem_vtable double_elem;
    return mat_create_internal(n, n, &double_elem, &identity_store);
}

static struct matrix_t *create_matrix_qfloat(size_t r, size_t c) {
    extern const struct elem_vtable qfloat_elem;
    return mat_create_internal(r, c, &qfloat_elem, &dense_store);
}

static struct matrix_t *create_identity_qfloat(size_t n) {
    extern const struct elem_vtable qfloat_elem;
    return mat_create_internal(n, n, &qfloat_elem, &identity_store);
}

static struct matrix_t *create_matrix_qcomplex(size_t r, size_t c) {
    extern const struct elem_vtable qcomplex_elem;
    return mat_create_internal(r, c, &qcomplex_elem, &dense_store);
}

static struct matrix_t *create_identity_qcomplex(size_t n) {
    extern const struct elem_vtable qcomplex_elem;
    return mat_create_internal(n, n, &qcomplex_elem, &identity_store);
}

/* ============================================================
   Element vtables (double, qfloat, qcomplex)
   ============================================================ */

/* ---------- double ---------- */

static const double D_ZERO = 0.0;
static const double D_ONE  = 1.0;

static void d_add(void *o, const void *a, const void *b) { *(double*)o = *(const double*)a + *(const double*)b; }
static void d_sub(void *o, const void *a, const void *b) { *(double*)o = *(const double*)a - *(const double*)b; }
static void d_mul(void *o, const void *a, const void *b) { *(double*)o = *(const double*)a * *(const double*)b; }
static void d_inv(void *o, const void *a) { *(double*)o = 1.0 / *(const double*)a; }
static void d_print(const void *v, char *buf, size_t n) { snprintf(buf, n, "%.16g", *(const double*)v); }

const struct elem_vtable double_elem = {
    .size  = sizeof(double),
    .kind  = ELEM_DOUBLE,
    .add   = d_add,
    .sub   = d_sub,
    .mul   = d_mul,
    .inv   = d_inv,
    .zero  = &D_ZERO,
    .one   = &D_ONE,
    .print = d_print,
    .create_matrix   = create_matrix_double,
    .create_identity = create_identity_double
};

/* ---------- qfloat ---------- */

static void qf_add_wrap(void *o, const void *a, const void *b) { *(qfloat_t*)o = qf_add(*(const qfloat_t*)a, *(const qfloat_t*)b); }
static void qf_sub_wrap(void *o, const void *a, const void *b) { *(qfloat_t*)o = qf_sub(*(const qfloat_t*)a, *(const qfloat_t*)b); }
static void qf_mul_wrap(void *o, const void *a, const void *b) { *(qfloat_t*)o = qf_mul(*(const qfloat_t*)a, *(const qfloat_t*)b); }
static void qf_inv_wrap(void *o, const void *a) { *(qfloat_t*)o = qf_div(QF_ONE, *(const qfloat_t*)a); }
static void qf_print_wrap(const void *v, char *buf, size_t n) { qf_to_string(*(const qfloat_t*)v, buf, n); }

const struct elem_vtable qfloat_elem = {
    .size  = sizeof(qfloat_t),
    .kind  = ELEM_QFLOAT,
    .add   = qf_add_wrap,
    .sub   = qf_sub_wrap,
    .mul   = qf_mul_wrap,
    .inv   = qf_inv_wrap,
    .zero  = &QF_ZERO,
    .one   = &QF_ONE,
    .print = qf_print_wrap,
    .create_matrix   = create_matrix_qfloat,
    .create_identity = create_identity_qfloat
};

/* ---------- qcomplex ---------- */

static qcomplex_t qc_inv(qcomplex_t z)
{
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

const struct elem_vtable qcomplex_elem = {
    .size  = sizeof(qcomplex_t),
    .kind  = ELEM_QCOMPLEX,
    .add   = qc_add_wrap,
    .sub   = qc_sub_wrap,
    .mul   = qc_mul_wrap,
    .inv   = qc_inv_wrap,
    .zero  = &QC_ZERO,
    .one   = &QC_ONE,
    .print = qc_print_wrap,
    .create_matrix   = create_matrix_qcomplex,
    .create_identity = create_identity_qcomplex
};

/* ============================================================
   Conversion helpers for mixed-type arithmetic
   ============================================================ */

static inline void d_to_qf(qfloat_t *out, const double *a) {
    *out = qf_from_double(*a);
}

static inline void d_to_qc(qcomplex_t *out, const double *a) {
    qfloat_t r = qf_from_double(*a);
    *out = qc_make(r, QF_ZERO);
}

static inline void qf_to_qc(qcomplex_t *out, const qfloat_t *a) {
    *out = qc_make(*a, QF_ZERO);
}

static inline void id_qf(qfloat_t *out, const qfloat_t *a) {
    *out = *a;
}

static inline void id_qc(qcomplex_t *out, const qcomplex_t *a) {
    *out = *a;
}

/* ============================================================
   Cross-type arithmetic: add / sub / mul
   ============================================================ */

/* ---- double <-> qfloat ---- */

static void add_d_qf(void *out, const void *a, const void *b)
{
    qfloat_t A, B;
    d_to_qf(&A, (const double*)a);
    id_qf(&B, (const qfloat_t*)b);
    *(qfloat_t*)out = qf_add(A, B);
}

static void add_qf_d(void *out, const void *a, const void *b)
{
    qfloat_t A, B;
    id_qf(&A, (const qfloat_t*)a);
    d_to_qf(&B, (const double*)b);
    *(qfloat_t*)out = qf_add(A, B);
}

static void sub_d_qf(void *out, const void *a, const void *b)
{
    qfloat_t A, B;
    d_to_qf(&A, (const double*)a);
    id_qf(&B, (const qfloat_t*)b);
    *(qfloat_t*)out = qf_sub(A, B);
}

static void sub_qf_d(void *out, const void *a, const void *b)
{
    qfloat_t A, B;
    id_qf(&A, (const qfloat_t*)a);
    d_to_qf(&B, (const double*)b);
    *(qfloat_t*)out = qf_sub(A, B);
}

static void mul_d_qf(void *out, const void *a, const void *b)
{
    qfloat_t A, B;
    d_to_qf(&A, (const double*)a);
    id_qf(&B, (const qfloat_t*)b);
    *(qfloat_t*)out = qf_mul(A, B);
}

static void mul_qf_d(void *out, const void *a, const void *b)
{
    qfloat_t A, B;
    id_qf(&A, (const qfloat_t*)a);
    d_to_qf(&B, (const double*)b);
    *(qfloat_t*)out = qf_mul(A, B);
}

/* ---- double <-> qcomplex ---- */

static void add_d_qc(void *out, const void *a, const void *b)
{
    qcomplex_t A, B;
    d_to_qc(&A, (const double*)a);
    id_qc(&B, (const qcomplex_t*)b);
    *(qcomplex_t*)out = qc_add(A, B);
}

static void add_qc_d(void *out, const void *a, const void *b)
{
    qcomplex_t A, B;
    id_qc(&A, (const qcomplex_t*)a);
    d_to_qc(&B, (const double*)b);
    *(qcomplex_t*)out = qc_add(A, B);
}

static void sub_d_qc(void *out, const void *a, const void *b)
{
    qcomplex_t A, B;
    d_to_qc(&A, (const double*)a);
    id_qc(&B, (const qcomplex_t*)b);
    *(qcomplex_t*)out = qc_sub(A, B);
}

static void sub_qc_d(void *out, const void *a, const void *b)
{
    qcomplex_t A, B;
    id_qc(&A, (const qcomplex_t*)a);
    d_to_qc(&B, (const double*)b);
    *(qcomplex_t*)out = qc_sub(A, B);
}

static void mul_d_qc(void *out, const void *a, const void *b)
{
    qcomplex_t A, B;
    d_to_qc(&A, (const double*)a);
    id_qc(&B, (const qcomplex_t*)b);
    *(qcomplex_t*)out = qc_mul(A, B);
}

static void mul_qc_d(void *out, const void *a, const void *b)
{
    qcomplex_t A, B;
    id_qc(&A, (const qcomplex_t*)a);
    d_to_qc(&B, (const double*)b);
    *(qcomplex_t*)out = qc_mul(A, B);
}

/* ---- qfloat <-> qcomplex ---- */

static void add_qf_qc(void *out, const void *a, const void *b)
{
    qcomplex_t A, B;
    qf_to_qc(&A, (const qfloat_t*)a);
    id_qc(&B, (const qcomplex_t*)b);
    *(qcomplex_t*)out = qc_add(A, B);
}

static void add_qc_qf(void *out, const void *a, const void *b)
{
    qcomplex_t A, B;
    id_qc(&A, (const qcomplex_t*)a);
    qf_to_qc(&B, (const qfloat_t*)b);
    *(qcomplex_t*)out = qc_add(A, B);
}

static void sub_qf_qc(void *out, const void *a, const void *b)
{
    qcomplex_t A, B;
    qf_to_qc(&A, (const qfloat_t*)a);
    id_qc(&B, (const qcomplex_t*)b);
    *(qcomplex_t*)out = qc_sub(A, B);
}

static void sub_qc_qf(void *out, const void *a, const void *b)
{
    qcomplex_t A, B;
    id_qc(&A, (const qcomplex_t*)a);
    qf_to_qc(&B, (const qfloat_t*)b);
    *(qcomplex_t*)out = qc_sub(A, B);
}

static void mul_qf_qc(void *out, const void *a, const void *b)
{
    qcomplex_t A, B;
    qf_to_qc(&A, (const qfloat_t*)a);
    id_qc(&B, (const qcomplex_t*)b);
    *(qcomplex_t*)out = qc_mul(A, B);
}

static void mul_qc_qf(void *out, const void *a, const void *b)
{
    qcomplex_t A, B;
    id_qc(&A, (const qcomplex_t*)a);
    qf_to_qc(&B, (const qfloat_t*)b);
    *(qcomplex_t*)out = qc_mul(A, B);
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

static const binop_vtable binops[ELEM_MAX][ELEM_MAX] = {
    [ELEM_DOUBLE] = {
        /* double op double -> double */
        [ELEM_DOUBLE] = {
            .result_elem = &double_elem,
            .add = d_add,
            .sub = d_sub,
            .mul = d_mul
        },
        /* double op qfloat -> qfloat */
        [ELEM_QFLOAT] = {
            .result_elem = &qfloat_elem,
            .add = add_d_qf,
            .sub = sub_d_qf,
            .mul = mul_d_qf
        },
        /* double op qcomplex -> qcomplex */
        [ELEM_QCOMPLEX] = {
            .result_elem = &qcomplex_elem,
            .add = add_d_qc,
            .sub = sub_d_qc,
            .mul = mul_d_qc
        }
    },

    [ELEM_QFLOAT] = {
        /* qfloat op double -> qfloat */
        [ELEM_DOUBLE] = {
            .result_elem = &qfloat_elem,
            .add = add_qf_d,
            .sub = sub_qf_d,
            .mul = mul_qf_d
        },
        /* qfloat op qfloat -> qfloat */
        [ELEM_QFLOAT] = {
            .result_elem = &qfloat_elem,
            .add = qf_add_wrap,
            .sub = qf_sub_wrap,
            .mul = qf_mul_wrap
        },
        /* qfloat op qcomplex -> qcomplex */
        [ELEM_QCOMPLEX] = {
            .result_elem = &qcomplex_elem,
            .add = add_qf_qc,
            .sub = sub_qf_qc,
            .mul = mul_qf_qc
        }
    },

    [ELEM_QCOMPLEX] = {
        /* qcomplex op double -> qcomplex */
        [ELEM_DOUBLE] = {
            .result_elem = &qcomplex_elem,
            .add = add_qc_d,
            .sub = sub_qc_d,
            .mul = mul_qc_d
        },
        /* qcomplex op qfloat -> qcomplex */
        [ELEM_QFLOAT] = {
            .result_elem = &qcomplex_elem,
            .add = add_qc_qf,
            .sub = sub_qc_qf,
            .mul = mul_qc_qf
        },
        /* qcomplex op qcomplex -> qcomplex */
        [ELEM_QCOMPLEX] = {
            .result_elem = &qcomplex_elem,
            .add = qc_add_wrap,
            .sub = qc_sub_wrap,
            .mul = qc_mul_wrap
        }
    }
};

/* ============================================================
   Public API constructors (preserve old names)
   ============================================================ */

struct matrix_t *mat_create_d(size_t rows, size_t cols) {
    return double_elem.create_matrix(rows, cols);
}

struct matrix_t *mat_create_qf(size_t rows, size_t cols) {
    return qfloat_elem.create_matrix(rows, cols);
}

struct matrix_t *mat_create_qc(size_t rows, size_t cols) {
    return qcomplex_elem.create_matrix(rows, cols);
}

struct matrix_t *matsq_create_d(size_t n)  { return mat_create_d(n, n); }
struct matrix_t *matsq_create_qf(size_t n) { return mat_create_qf(n, n); }
struct matrix_t *matsq_create_qc(size_t n) { return mat_create_qc(n, n); }

struct matrix_t *matsq_ident_d(size_t n) {
    return double_elem.create_identity(n);
}

struct matrix_t *matsq_ident_qf(size_t n) {
    return qfloat_elem.create_identity(n);
}

struct matrix_t *matsq_ident_qc(size_t n) {
    return qcomplex_elem.create_identity(n);
}

/* ============================================================
   Destruction and basic access
   ============================================================ */

void mat_free(struct matrix_t *A) {
    if (!A) return;
    A->store->free(A);
    free(A);
}

void mat_get(const struct matrix_t *A, size_t i, size_t j, void *out) {
    A->store->get(A, i, j, out);
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

static inline const struct elem_vtable *elem_of(const struct matrix_t *A) {
    return A->elem;
}

/* ============================================================
   Mixed-type matrix operations (2D vtable driven)
   ============================================================ */

struct matrix_t *mat_add(const struct matrix_t *A, const struct matrix_t *B) {
    if (!A || !B || A->rows != B->rows || A->cols != B->cols)
        return NULL;

    elem_kind ak = elem_of(A)->kind;
    elem_kind bk = elem_of(B)->kind;

    const binop_vtable *op = &binops[ak][bk];
    if (!op->add || !op->result_elem)
        return NULL;

    const struct elem_vtable *re = op->result_elem;
    struct matrix_t *C = re->create_matrix(A->rows, A->cols);
    if (!C) return NULL;

    unsigned char a_raw[64], b_raw[64], out[64];

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a_raw);
            mat_get(B, i, j, b_raw);
            op->add(out, a_raw, b_raw);
            mat_set(C, i, j, out);
        }

    return C;
}

struct matrix_t *mat_sub(const struct matrix_t *A, const struct matrix_t *B) {
    if (!A || !B || A->rows != B->rows || A->cols != B->cols)
        return NULL;

    elem_kind ak = elem_of(A)->kind;
    elem_kind bk = elem_of(B)->kind;

    const binop_vtable *op = &binops[ak][bk];
    if (!op->sub || !op->result_elem)
        return NULL;

    const struct elem_vtable *re = op->result_elem;
    struct matrix_t *C = re->create_matrix(A->rows, A->cols);
    if (!C) return NULL;

    unsigned char a_raw[64], b_raw[64], out[64];

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a_raw);
            mat_get(B, i, j, b_raw);
            op->sub(out, a_raw, b_raw);
            mat_set(C, i, j, out);
        }

    return C;
}

struct matrix_t *mat_mul(const struct matrix_t *A, const struct matrix_t *B) {
    if (!A || !B || A->cols != B->rows)
        return NULL;

    elem_kind ak = elem_of(A)->kind;
    elem_kind bk = elem_of(B)->kind;

    const binop_vtable *op = &binops[ak][bk];
    if (!op->mul || !op->result_elem)
        return NULL;

    const struct elem_vtable *re = op->result_elem;
    struct matrix_t *C = re->create_matrix(A->rows, B->cols);
    if (!C) return NULL;

    unsigned char x_raw[64], y_raw[64], prod[64], sum[64];

    for (size_t i = 0; i < A->rows; i++) {
        for (size_t j = 0; j < B->cols; j++) {

            memcpy(sum, re->zero, re->size);

            for (size_t k = 0; k < A->cols; k++) {
                mat_get(A, i, k, x_raw);
                mat_get(B, k, j, y_raw);
                op->mul(prod, x_raw, y_raw);
                re->add(sum, sum, prod);
            }

            mat_set(C, i, j, sum);
        }
    }

    return C;
}

/* ============================================================
   Transpose and conjugate (same-type result)
   ============================================================ */

struct matrix_t *mat_transpose(const struct matrix_t *A) {
    if (!A) return NULL;

    const struct elem_vtable *e = elem_of(A);
    struct matrix_t *T = e->create_matrix(A->cols, A->rows);
    if (!T) return NULL;

    unsigned char v[64];

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, v);
            mat_set(T, j, i, v);
        }

    return T;
}

struct matrix_t *mat_conj(const struct matrix_t *A) {
    if (!A) return NULL;

    const struct elem_vtable *e = elem_of(A);
    struct matrix_t *C = e->create_matrix(A->rows, A->cols);
    if (!C) return NULL;

    unsigned char v[64];

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, v);

            if (e->kind == ELEM_QCOMPLEX) {
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

void mat_print(const struct matrix_t *A) {
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
