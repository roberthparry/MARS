#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "matrix_internal.h"
#include "qfloat.h"
#include "qcomplex.h"

/* ============================================================
   Element-type-specific matrix constructors (forward declarations)
   ============================================================ */

static struct matrix_t *create_matrix_double(size_t rows, size_t cols);
static struct matrix_t *create_identity_double(size_t n);
static struct matrix_t *create_matrix_qfloat(size_t rows, size_t cols);
static struct matrix_t *create_identity_qfloat(size_t n);
static struct matrix_t *create_matrix_qcomplex(size_t rows, size_t cols);
static struct matrix_t *create_identity_qcomplex(size_t n);

/* ============================================================
   Storage vtables (dense, identity)
   ============================================================ */

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

static struct matrix_t *create_matrix_double(size_t r, size_t c) {
    return mat_create_internal(r, c, &double_elem, &dense_store);
}

static struct matrix_t *create_identity_double(size_t n) {
    return mat_create_internal(n, n, &double_elem, &identity_store);
}

static struct matrix_t *create_matrix_qfloat(size_t r, size_t c) {
    return mat_create_internal(r, c, &qfloat_elem, &dense_store);
}

static struct matrix_t *create_identity_qfloat(size_t n) {
    return mat_create_internal(n, n, &qfloat_elem, &identity_store);
}

static struct matrix_t *create_matrix_qcomplex(size_t r, size_t c) {
    return mat_create_internal(r, c, &qcomplex_elem, &dense_store);
}

static struct matrix_t *create_identity_qcomplex(size_t n) {
    return mat_create_internal(n, n, &qcomplex_elem, &identity_store);
}

/* ============================================================
   Element vtables (double, qfloat, qcomplex)
   ============================================================ */

/* ---------- double ---------- */

static const double D_ZERO = 0.0;
static const double D_ONE  = 1.0;

static void d_add(void *o, const void *a, const void *b) {
    *(double*)o = *(const double*)a + *(const double*)b;
}

static void d_sub(void *o, const void *a, const void *b) {
    *(double*)o = *(const double*)a - *(const double*)b;
}

static void d_mul(void *o, const void *a, const void *b) {
    *(double*)o = *(const double*)a * *(const double*)b;
}

static void d_inv(void *o, const void *a) {
    *(double*)o = 1.0 / *(const double*)a;
}

static void d_print(const void *v, char *buf, size_t n) {
    snprintf(buf, n, "%.16g", *(const double*)v);
}

static double d_abs2(const void *a)              { double x = *(const double*)a; return x * x; }
static double d_to_real(const void *a)            { return *(const double*)a; }
static void   d_from_real(void *o, double x)      { *(double*)o = x; }
static void   d_conj_elem(void *o, const void *a) { *(double*)o = *(const double*)a; }

static void d_to_qf(qfloat_t *o, const void *a)  { *o = qf_from_double(*(const double*)a); }
static void d_abs_qf(qfloat_t *o, const void *a) { *o = qf_from_double(fabs(*(const double*)a)); }
static void d_from_qf(void *o, const qfloat_t *x){ *(double*)o = qf_to_double(*x); }

const struct elem_vtable double_elem = {
    .size      = sizeof(double),
    .kind      = ELEM_DOUBLE,
    .add       = d_add,
    .sub       = d_sub,
    .mul       = d_mul,
    .inv       = d_inv,
    .abs2      = d_abs2,
    .to_real   = d_to_real,
    .from_real = d_from_real,
    .conj_elem = d_conj_elem,
    .to_qf     = d_to_qf,
    .abs_qf    = d_abs_qf,
    .from_qf   = d_from_qf,
    .zero      = &D_ZERO,
    .one       = &D_ONE,
    .print     = d_print,
    .create_matrix   = create_matrix_double,
    .create_identity = create_identity_double
};

/* ---------- qfloat ---------- */

static void qf_add_wrap(void *o, const void *a, const void *b) {
    *(qfloat_t*)o = qf_add(*(const qfloat_t*)a, *(const qfloat_t*)b);
}

static void qf_sub_wrap(void *o, const void *a, const void *b) {
    *(qfloat_t*)o = qf_sub(*(const qfloat_t*)a, *(const qfloat_t*)b);
}

static void qf_mul_wrap(void *o, const void *a, const void *b) {
    *(qfloat_t*)o = qf_mul(*(const qfloat_t*)a, *(const qfloat_t*)b);
}

static void qf_inv_wrap(void *o, const void *a) {
    *(qfloat_t*)o = qf_div(QF_ONE, *(const qfloat_t*)a);
}

static void qf_print_wrap(const void *v, char *buf, size_t n) {
    qf_to_string(*(const qfloat_t*)v, buf, n);
}

static double qf_abs2_wrap(const void *a) {
    qfloat_t x = *(const qfloat_t*)a;
    return qf_to_double(qf_mul(x, x));
}
static double qf_to_real_wrap(const void *a)            { return qf_to_double(*(const qfloat_t*)a); }
static void   qf_from_real_wrap(void *o, double x)      { *(qfloat_t*)o = qf_from_double(x); }
static void   qf_conj_elem(void *o, const void *a)      { *(qfloat_t*)o = *(const qfloat_t*)a; }

static void qf_to_qf(qfloat_t *o, const void *a)  { *o = *(const qfloat_t*)a; }
static void qf_abs_qf(qfloat_t *o, const void *a) { *o = qf_abs(*(const qfloat_t*)a); }
static void qf_from_qf(void *o, const qfloat_t *x){ *(qfloat_t*)o = *x; }

const struct elem_vtable qfloat_elem = {
    .size      = sizeof(qfloat_t),
    .kind      = ELEM_QFLOAT,
    .add       = qf_add_wrap,
    .sub       = qf_sub_wrap,
    .mul       = qf_mul_wrap,
    .inv       = qf_inv_wrap,
    .abs2      = qf_abs2_wrap,
    .to_real   = qf_to_real_wrap,
    .from_real = qf_from_real_wrap,
    .conj_elem = qf_conj_elem,
    .to_qf     = qf_to_qf,
    .abs_qf    = qf_abs_qf,
    .from_qf   = qf_from_qf,
    .zero      = &QF_ZERO,
    .one       = &QF_ONE,
    .print     = qf_print_wrap,
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

static void qc_add_wrap(void *o, const void *a, const void *b) {
    *(qcomplex_t*)o = qc_add(*(const qcomplex_t*)a, *(const qcomplex_t*)b);
}

static void qc_sub_wrap(void *o, const void *a, const void *b) {
    *(qcomplex_t*)o = qc_sub(*(const qcomplex_t*)a, *(const qcomplex_t*)b);
}

static void qc_mul_wrap(void *o, const void *a, const void *b) {
    *(qcomplex_t*)o = qc_mul(*(const qcomplex_t*)a, *(const qcomplex_t*)b);
}

static void qc_inv_wrap(void *o, const void *a) {
    *(qcomplex_t*)o = qc_inv(*(const qcomplex_t*)a);
}

static void qc_print_wrap(const void *v, char *buf, size_t n) {
    qc_to_string(*(const qcomplex_t*)v, buf, n);
}

static double qc_abs2_wrap(const void *a) {
    qcomplex_t z = *(const qcomplex_t*)a;
    return qf_to_double(qf_add(qf_mul(z.re, z.re), qf_mul(z.im, z.im)));
}
static double qc_to_real_wrap(const void *a) {
    return qf_to_double(((const qcomplex_t*)a)->re);
}
static void qc_from_real_wrap(void *o, double x) {
    *(qcomplex_t*)o = qc_make(qf_from_double(x), QF_ZERO);
}
static void qc_conj_elem(void *o, const void *a) {
    *(qcomplex_t*)o = qc_conj(*(const qcomplex_t*)a);
}

static void qc_to_qf(qfloat_t *o, const void *a)  { *o = ((const qcomplex_t*)a)->re; }
static void qc_abs_qf(qfloat_t *o, const void *a) { *o = qc_abs(*(const qcomplex_t*)a); }
static void qc_from_qf(void *o, const qfloat_t *x){ *(qcomplex_t*)o = qc_make(*x, QF_ZERO); }

const struct elem_vtable qcomplex_elem = {
    .size      = sizeof(qcomplex_t),
    .kind      = ELEM_QCOMPLEX,
    .add       = qc_add_wrap,
    .sub       = qc_sub_wrap,
    .mul       = qc_mul_wrap,
    .inv       = qc_inv_wrap,
    .abs2      = qc_abs2_wrap,
    .to_real   = qc_to_real_wrap,
    .from_real = qc_from_real_wrap,
    .conj_elem = qc_conj_elem,
    .to_qf     = qc_to_qf,
    .abs_qf    = qc_abs_qf,
    .from_qf   = qc_from_qf,
    .zero      = &QC_ZERO,
    .one       = &QC_ONE,
    .print     = qc_print_wrap,
    .create_matrix   = create_matrix_qcomplex,
    .create_identity = create_identity_qcomplex
};

/* ============================================================
   Conversion helpers for mixed-type arithmetic
   ============================================================ */

static inline void d_as_qf(qfloat_t *out, const double *a) {
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
    d_as_qf(&A, (const double*)a);
    id_qf(&B, (const qfloat_t*)b);
    *(qfloat_t*)out = qf_add(A, B);
}

static void add_qf_d(void *out, const void *a, const void *b)
{
    qfloat_t A, B;
    id_qf(&A, (const qfloat_t*)a);
    d_as_qf(&B, (const double*)b);
    *(qfloat_t*)out = qf_add(A, B);
}

static void sub_d_qf(void *out, const void *a, const void *b)
{
    qfloat_t A, B;
    d_as_qf(&A, (const double*)a);
    id_qf(&B, (const qfloat_t*)b);
    *(qfloat_t*)out = qf_sub(A, B);
}

static void sub_qf_d(void *out, const void *a, const void *b)
{
    qfloat_t A, B;
    id_qf(&A, (const qfloat_t*)a);
    d_as_qf(&B, (const double*)b);
    *(qfloat_t*)out = qf_sub(A, B);
}

static void mul_d_qf(void *out, const void *a, const void *b)
{
    qfloat_t A, B;
    d_as_qf(&A, (const double*)a);
    id_qf(&B, (const qfloat_t*)b);
    *(qfloat_t*)out = qf_mul(A, B);
}

static void mul_qf_d(void *out, const void *a, const void *b)
{
    qfloat_t A, B;
    id_qf(&A, (const qfloat_t*)a);
    d_as_qf(&B, (const double*)b);
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

struct matrix_t *matsq_create_d(size_t n)  {
    return mat_create_d(n, n);
}

struct matrix_t *matsq_create_qf(size_t n) {
    return mat_create_qf(n, n);
}

struct matrix_t *matsq_create_qc(size_t n) {
    return mat_create_qc(n, n);
}

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

matrix_t *mat_scalar_mul_d(double s, matrix_t *A)
{
    if (!A) return NULL;

    elem_kind ak = elem_of(A)->kind;

    /* scalar is double => left kind is ELEM_DOUBLE */
    const binop_vtable *op = &binops[ELEM_DOUBLE][ak];
    if (!op->mul || !op->result_elem)
        return NULL;

    const struct elem_vtable *re = op->result_elem;
    matrix_t *R = re->create_matrix(A->rows, A->cols);
    if (!R) return NULL;

    unsigned char scalar_raw[64];
    unsigned char a_raw[64];
    unsigned char out_raw[64];

    /* encode scalar as a double (left operand type) */
    memcpy(scalar_raw, &s, sizeof(double));

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a_raw);
            op->mul(out_raw, scalar_raw, a_raw);
            mat_set(R, i, j, out_raw);
        }

    return R;
}

matrix_t *mat_scalar_mul_qf(qfloat_t s, matrix_t *A)
{
    if (!A) return NULL;

    elem_kind ak = elem_of(A)->kind;

    /* scalar is qfloat => left kind is ELEM_QFLOAT */
    const binop_vtable *op = &binops[ELEM_QFLOAT][ak];
    if (!op->mul || !op->result_elem)
        return NULL;

    const struct elem_vtable *re = op->result_elem;
    matrix_t *R = re->create_matrix(A->rows, A->cols);
    if (!R) return NULL;

    unsigned char scalar_raw[64];
    unsigned char a_raw[64];
    unsigned char out_raw[64];

    /* encode scalar as a qfloat_t (left operand type) */
    memcpy(scalar_raw, &s, sizeof(qfloat_t));

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a_raw);
            op->mul(out_raw, scalar_raw, a_raw);
            mat_set(R, i, j, out_raw);
        }

    return R;
}

matrix_t *mat_scalar_mul_qc(qcomplex_t s, matrix_t *A)
{
    if (!A) return NULL;

    elem_kind ak = elem_of(A)->kind;

    /* scalar is qcomplex => left kind is ELEM_QCOMPLEX */
    const binop_vtable *op = &binops[ELEM_QCOMPLEX][ak];
    if (!op->mul || !op->result_elem)
        return NULL;

    const struct elem_vtable *re = op->result_elem;
    matrix_t *R = re->create_matrix(A->rows, A->cols);
    if (!R) return NULL;

    unsigned char scalar_raw[64];
    unsigned char a_raw[64];
    unsigned char out_raw[64];

    /* encode scalar as a qcomplex_t (left operand type) */
    memcpy(scalar_raw, &s, sizeof(qcomplex_t));

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a_raw);
            op->mul(out_raw, scalar_raw, a_raw);
            mat_set(R, i, j, out_raw);
        }

    return R;
}

matrix_t *mat_scalar_div_d(double s, matrix_t *A)
{
    double inv = 1.0 / s;
    return mat_scalar_mul_d(inv, A);
}

matrix_t *mat_scalar_div_qf(qfloat_t s, matrix_t *A)
{
    qfloat_t inv = qf_div(QF_ONE, s);
    return mat_scalar_mul_qf(inv, A);
}

matrix_t *mat_scalar_div_qc(qcomplex_t s, matrix_t *A)
{
    qcomplex_t inv = qc_inv(s);
    return mat_scalar_mul_qc(inv, A);
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

    unsigned char v[64], cv[64];

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, v);
            e->conj_elem(cv, v);
            mat_set(C, i, j, cv);
        }

    return C;
}

matrix_t *mat_hermitian(const matrix_t *A)
{
    if (!A)
        return NULL;

    /* Hermitian = conjugate transpose */
    matrix_t *T = mat_transpose(A);
    if (!T)
        return NULL;

    matrix_t *H = mat_conj(T);
    mat_free(T);

    return H;
}

int mat_det(const matrix_t *A, void *determinant)
{
    if (!A || !determinant)
        return -1;

    if (A->rows != A->cols)
        return -2;

    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;

    /* 1×1 case */
    if (n == 1) {
        mat_get(A, 0, 0, determinant);
        return 0;
    }

    /* Make a working dense copy of A in the same element type */
    matrix_t *M = e->create_matrix(A->rows, A->cols);
    if (!M)
        return -3;

    unsigned char val[64];
    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, val);
            mat_set(M, i, j, val);
        }

    /* Buffers for element arithmetic */
    unsigned char pivot[64];
    unsigned char inv_pivot[64];
    unsigned char factor[64];
    unsigned char a[64];
    unsigned char b[64];
    unsigned char prod[64];
    unsigned char tmp[64];

    /* Gaussian elimination without pivoting */
    for (size_t k = 0; k < n; k++) {

        /* pivot = M[k][k] */
        mat_get(M, k, k, pivot);

        /* If pivot == 0 → determinant is 0 */
        if (memcmp(pivot, e->zero, e->size) == 0) {
            memcpy(determinant, e->zero, e->size);
            mat_free(M);
            return 0;
        }

        /* inv_pivot = 1 / pivot */
        e->inv(inv_pivot, pivot);

        for (size_t i = k + 1; i < n; i++) {

            /* factor = M[i][k] / pivot = M[i][k] * inv_pivot */
            mat_get(M, i, k, factor);
            e->mul(factor, inv_pivot, factor);

            for (size_t j = k; j < n; j++) {

                /* a = M[i][j], b = M[k][j] */
                mat_get(M, i, j, a);
                mat_get(M, k, j, b);

                /* prod = factor * b */
                e->mul(prod, factor, b);

                /* tmp = a - prod */
                e->sub(tmp, a, prod);

                mat_set(M, i, j, tmp);
            }
        }
    }

    /* determinant = product of diagonal entries */
    unsigned char det[64];
    memcpy(det, e->one, e->size);

    for (size_t i = 0; i < n; i++) {
        mat_get(M, i, i, a);
        e->mul(det, det, a);
    }

    memcpy(determinant, det, e->size);
    mat_free(M);
    return 0;
}

matrix_t *mat_inverse(const matrix_t *A)
{
    if (!A)
        return NULL;

    if (A->rows != A->cols)
        return NULL;

    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;

    /* Make working copies: M = A, I = identity */
    matrix_t *M = e->create_matrix(n, n);
    matrix_t *I = e->create_identity(n);
    if (!M || !I) {
        if (M) mat_free(M);
        if (I) mat_free(I);
        return NULL;
    }

    unsigned char v[64];

    /* Copy A into M */
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < n; j++) {
            mat_get(A, i, j, v);
            mat_set(M, i, j, v);
        }

    /* Temporary buffers */
    unsigned char pivot[64];
    unsigned char inv_pivot[64];
    unsigned char factor[64];
    unsigned char a[64];
    unsigned char b[64];
    unsigned char prod[64];
    unsigned char tmp[64];

    /* Gauss–Jordan elimination */
    for (size_t k = 0; k < n; k++) {

        /* pivot = M[k][k] */
        mat_get(M, k, k, pivot);

        /* If pivot == 0 → singular */
        if (memcmp(pivot, e->zero, e->size) == 0) {
            mat_free(M);
            mat_free(I);
            return NULL;
        }

        /* inv_pivot = 1 / pivot */
        e->inv(inv_pivot, pivot);

        /* Normalize row k: divide entire row by pivot */
        for (size_t j = 0; j < n; j++) {
            mat_get(M, k, j, a);
            e->mul(a, inv_pivot, a);
            mat_set(M, k, j, a);

            mat_get(I, k, j, b);
            e->mul(b, inv_pivot, b);
            mat_set(I, k, j, b);
        }

        /* Eliminate all other rows */
        for (size_t i = 0; i < n; i++) {
            if (i == k) continue;

            /* factor = M[i][k] */
            mat_get(M, i, k, factor);

            /* If factor == 0, skip */
            if (memcmp(factor, e->zero, e->size) == 0)
                continue;

            for (size_t j = 0; j < n; j++) {

                /* M[i][j] -= factor * M[k][j] */
                mat_get(M, i, j, a);
                mat_get(M, k, j, b);
                e->mul(prod, factor, b);
                e->sub(tmp, a, prod);
                mat_set(M, i, j, tmp);

                /* I[i][j] -= factor * I[k][j] */
                mat_get(I, i, j, a);
                mat_get(I, k, j, b);
                e->mul(prod, factor, b);
                e->sub(tmp, a, prod);
                mat_set(I, i, j, tmp);
            }
        }
    }

    /* M is now identity; I is A^{-1} */
    mat_free(M);
    return I;
}

/* ============================================================
   Eigenvalues / eigenvectors (Hermitian Jacobi implementation)
   ============================================================ */

static matrix_t *mat_copy_dense(const matrix_t *A)
{
    const struct elem_vtable *e = A->elem;
    matrix_t *C = e->create_matrix(A->rows, A->cols);
    if (!C) return NULL;
    unsigned char v[64];
    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, v);
            mat_set(C, i, j, v);
        }
    return C;
}

/* Squared Frobenius norm of all off-diagonal elements (convergence probe). */
static double offdiag_norm2(const matrix_t *A)
{
    const struct elem_vtable *e = A->elem;
    unsigned char v[64];
    double s = 0.0;
    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            if (i == j) continue;
            mat_get(A, i, j, v);
            s += e->abs2(v);
        }
    return s;
}

/* Zero A[p][q] with a complex Givens rotation; accumulate into V.
 *
 * Handles the Hermitian case: the rotation has a real magnitude (c, s)
 * and a complex phase derived from A[p][q] itself, so the algorithm
 * never inspects the element kind directly.
 *
 * After this call: (J† A J)[p][q] == 0,  V_new = V J.
 */
static void jacobi_apply(matrix_t *A, matrix_t *V, size_t p, size_t q)
{
    const struct elem_vtable *e = A->elem;
    size_t n = A->rows;

    unsigned char a_pq[64];
    mat_get(A, p, q, a_pq);

    /* |A[p,q]| in qfloat precision */
    qfloat_t B_qf;
    e->abs_qf(&B_qf, a_pq);
    if (qf_to_double(B_qf) < 1e-150) return;

    unsigned char a_pp[64], a_qq[64];
    mat_get(A, p, p, a_pp);
    mat_get(A, q, q, a_qq);

    /* Real Jacobi parameters computed entirely in qfloat precision */
    qfloat_t app_qf, aqq_qf;
    e->to_qf(&app_qf, a_pp);
    e->to_qf(&aqq_qf, a_qq);
    qfloat_t two   = qf_from_double(2.0);
    qfloat_t tau   = qf_div(qf_sub(aqq_qf, app_qf), qf_mul(two, B_qf));
    qfloat_t sign  = (qf_to_double(tau) >= 0.0) ? QF_ONE : qf_neg(QF_ONE);
    qfloat_t t_qf  = qf_div(sign, qf_add(qf_abs(tau), qf_sqrt(qf_add(QF_ONE, qf_mul(tau, tau)))));
    qfloat_t c_qf  = qf_div(QF_ONE, qf_sqrt(qf_add(QF_ONE, qf_mul(t_qf, t_qf))));
    qfloat_t s_qf  = qf_mul(t_qf, c_qf);
    qfloat_t ns_qf = qf_neg(s_qf);
    qfloat_t ib_qf = qf_div(QF_ONE, B_qf);

    /* phase = A[p,q] / |A[p,q]|  —  a unit element in the matrix's type */
    unsigned char inv_B[64], ph[64], ph_c[64];
    e->from_qf(inv_B, &ib_qf);
    e->mul(ph,  a_pq, inv_B);
    e->conj_elem(ph_c, ph);

    /* Pre-compute the four rotation scalars as typed elements */
    unsigned char ce[64], nse[64], cph[64], cph_c[64], sph[64], sph_c[64];
    unsigned char se[64];
    e->from_qf(ce,  &c_qf);
    e->from_qf(se,  &s_qf);
    e->from_qf(nse, &ns_qf);
    e->mul(cph,   ce, ph);      /* c · phase   */
    e->mul(cph_c, ce, ph_c);    /* c · phase*  */
    e->mul(sph,   se, ph);      /* s · phase   */
    e->mul(sph_c, se, ph_c);    /* s · phase*  */

    unsigned char ai_p[64], ai_q[64], t1[64], t2[64], np[64], nq[64];

    /* Column update: A ← A J
     *   A[:,p] = (c·ph)·A[:,p]  +  (-s)·A[:,q]
     *   A[:,q] = (s·ph)·A[:,p]  +    c ·A[:,q]
     */
    for (size_t i = 0; i < n; i++) {
        mat_get(A, i, p, ai_p);
        mat_get(A, i, q, ai_q);
        e->mul(t1, cph,  ai_p);  e->mul(t2, nse, ai_q);  e->add(np, t1, t2);
        e->mul(t1, sph,  ai_p);  e->mul(t2, ce,  ai_q);  e->add(nq, t1, t2);
        mat_set(A, i, p, np);
        mat_set(A, i, q, nq);
    }

    /* Row update: A ← J† A
     *   A[p,:] = (c·ph*)·A[p,:]  +  (-s)·A[q,:]
     *   A[q,:] = (s·ph*)·A[p,:]  +    c ·A[q,:]
     */
    unsigned char ap_k[64], aq_k[64];
    for (size_t k = 0; k < n; k++) {
        mat_get(A, p, k, ap_k);
        mat_get(A, q, k, aq_k);
        e->mul(t1, cph_c, ap_k);  e->mul(t2, nse, aq_k);  e->add(np, t1, t2);
        e->mul(t1, sph_c, ap_k);  e->mul(t2, ce,  aq_k);  e->add(nq, t1, t2);
        mat_set(A, p, k, np);
        mat_set(A, q, k, nq);
    }

    /* Eigenvector accumulation: V ← V J  (same column transform as above) */
    unsigned char vi_p[64], vi_q[64];
    for (size_t i = 0; i < n; i++) {
        mat_get(V, i, p, vi_p);
        mat_get(V, i, q, vi_q);
        e->mul(t1, cph,  vi_p);  e->mul(t2, nse, vi_q);  e->add(np, t1, t2);
        e->mul(t1, sph,  vi_p);  e->mul(t2, ce,  vi_q);  e->add(nq, t1, t2);
        mat_set(V, i, p, np);
        mat_set(V, i, q, nq);
    }
}

int mat_eigendecompose(const matrix_t *A, void *eigenvalues, matrix_t **eigenvectors)
{
    if (!A) return -1;
    if (A->rows != A->cols) return -2;

    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;

    matrix_t *W = mat_copy_dense(A);
    if (!W) return -3;

    matrix_t *V = e->create_identity(n);
    if (!V) { mat_free(W); return -3; }

    /* Frobenius norm² of W (invariant under orthogonal similarity) */
    double fro2 = 0.0;
    {
        unsigned char fv[64];
        for (size_t i = 0; i < n; i++)
            for (size_t j = 0; j < n; j++) {
                mat_get(W, i, j, fv);
                fro2 += e->abs2(fv);
            }
    }
    /* Converge when off-diagonal norm < 1e-29 * ||A||_F (safely below qfloat eps) */
    double tol = fro2 * 1e-29;

    for (int sweep = 0; sweep < 50; sweep++) {
        for (size_t p = 0; p < n - 1; p++)
            for (size_t q = p + 1; q < n; q++)
                jacobi_apply(W, V, p, q);
        if (offdiag_norm2(W) < tol) break;
    }

    if (eigenvalues) {
        /* Diagonal of W holds eigenvalues; extract real part via qfloat to
           preserve full precision and strip any imaginary rounding noise. */
        char *ev = (char *)eigenvalues;
        unsigned char dv[64];
        for (size_t i = 0; i < n; i++) {
            mat_get(W, i, i, dv);
            qfloat_t re_qf;
            e->to_qf(&re_qf, dv);
            e->from_qf(ev + i * e->size, &re_qf);
        }
    }

    if (eigenvectors)
        *eigenvectors = V;
    else
        mat_free(V);

    mat_free(W);
    return 0;
}

int mat_eigenvalues(const matrix_t *A, void *eigenvalues)
{
    return mat_eigendecompose(A, eigenvalues, NULL);
}

matrix_t *mat_eigenvectors(const matrix_t *A)
{
    matrix_t *V = NULL;
    if (mat_eigendecompose(A, NULL, &V) != 0)
        return NULL;
    return V;
}

/* ============================================================
   Debug printing
   ============================================================ */

void mat_print(const struct matrix_t *A)
{
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
