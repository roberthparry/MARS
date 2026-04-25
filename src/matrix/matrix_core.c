#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "matrix_internal.h"
#include "qfloat.h"
#include "qcomplex.h"
#include "matrix.h"

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

static int d_cmp(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
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

static void d_to_qc_fn(qcomplex_t *o, const void *a)  { *o = qc_make(qf_from_double(*(const double*)a), QF_ZERO); }
static void d_from_qc_fn(void *o, const qcomplex_t *z) { *(double*)o = qf_to_double(z->re); }

/* ============================================================
   Scalar function vtable for double
   ============================================================ */

static void d_scalar_exp (void *out, const void *a) { *(double*)out = exp (*(const double*)a); }
static void d_scalar_log (void *out, const void *a) { *(double*)out = log (*(const double*)a); }
static void d_scalar_sin (void *out, const void *a) { *(double*)out = sin (*(const double*)a); }
static void d_scalar_cos (void *out, const void *a) { *(double*)out = cos (*(const double*)a); }
static void d_scalar_tan (void *out, const void *a) { *(double*)out = tan (*(const double*)a); }

static void d_scalar_sinh(void *out, const void *a) { *(double*)out = sinh(*(const double*)a); }
static void d_scalar_cosh(void *out, const void *a) { *(double*)out = cosh(*(const double*)a); }
static void d_scalar_tanh(void *out, const void *a) { *(double*)out = tanh(*(const double*)a); }

static void d_scalar_sqrt(void *out, const void *a) { *(double*)out = sqrt(*(const double*)a); }

static void d_scalar_asin(void *out, const void *a) { *(double*)out = asin(*(const double*)a); }
static void d_scalar_acos(void *out, const void *a) { *(double*)out = acos(*(const double*)a); }
static void d_scalar_atan(void *out, const void *a) { *(double*)out = atan(*(const double*)a); }

static void d_scalar_asinh(void *out, const void *a) { *(double*)out = asinh(*(const double*)a); }
static void d_scalar_acosh(void *out, const void *a) { *(double*)out = acosh(*(const double*)a); }
static void d_scalar_atanh(void *out, const void *a) { *(double*)out = atanh(*(const double*)a); }

static void d_scalar_erf (void *out, const void *a) { *(double*)out = erf (*(const double*)a); }
static void d_scalar_erfc(void *out, const void *a) { *(double*)out = erfc(*(const double*)a); }

static const struct elem_fun_vtable double_fun = {
    .exp  = d_scalar_exp,
    .sin  = d_scalar_sin,
    .cos  = d_scalar_cos,
    .tan  = d_scalar_tan,

    .sinh = d_scalar_sinh,
    .cosh = d_scalar_cosh,
    .tanh = d_scalar_tanh,

    .sqrt = d_scalar_sqrt,
    .log  = d_scalar_log,

    .asin = d_scalar_asin,
    .acos = d_scalar_acos,
    .atan = d_scalar_atan,

    .asinh = d_scalar_asinh,
    .acosh = d_scalar_acosh,
    .atanh = d_scalar_atanh,

    .erf  = d_scalar_erf,
    .erfc = d_scalar_erfc
};

const struct elem_vtable double_elem = {
    .size            = sizeof(double),
    .kind            = ELEM_DOUBLE,
    .public_type     = MAT_TYPE_DOUBLE,
    .add             = d_add,
    .sub             = d_sub,
    .mul             = d_mul,
    .inv             = d_inv,
    .abs2            = d_abs2,
    .to_real         = d_to_real,
    .from_real       = d_from_real,
    .conj_elem       = d_conj_elem,
    .to_qf           = d_to_qf,
    .abs_qf          = d_abs_qf,
    .from_qf         = d_from_qf,
    .to_qc           = d_to_qc_fn,
    .from_qc         = d_from_qc_fn,
    .zero            = &D_ZERO,
    .one             = &D_ONE,
    .cmp             = d_cmp,
    .print           = d_print,
    .create_matrix   = create_matrix_double,
    .create_identity = create_identity_double,

    .fun             = &double_fun
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

static int qfloat_cmp(const void *a, const void *b)
{
    return qf_cmp(*(const qfloat_t *)a, *(const qfloat_t *)b);
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

static void qf_to_qc_fn(qcomplex_t *o, const void *a)  { *o = qc_make(*(const qfloat_t*)a, QF_ZERO); }
static void qf_from_qc_fn(void *o, const qcomplex_t *z) { *(qfloat_t*)o = z->re; }

/* ============================================================
   Scalar function vtable for qfloat
   ============================================================ */

static void qf_scalar_exp (void *out, const void *a) { *(qfloat_t*)out = qf_exp (*(const qfloat_t*)a); }
static void qf_scalar_log (void *out, const void *a) { *(qfloat_t*)out = qf_log (*(const qfloat_t*)a); }
static void qf_scalar_sin (void *out, const void *a) { *(qfloat_t*)out = qf_sin (*(const qfloat_t*)a); }
static void qf_scalar_cos (void *out, const void *a) { *(qfloat_t*)out = qf_cos (*(const qfloat_t*)a); }
static void qf_scalar_tan (void *out, const void *a) { *(qfloat_t*)out = qf_tan (*(const qfloat_t*)a); }

static void qf_scalar_sinh(void *out, const void *a) { *(qfloat_t*)out = qf_sinh(*(const qfloat_t*)a); }
static void qf_scalar_cosh(void *out, const void *a) { *(qfloat_t*)out = qf_cosh(*(const qfloat_t*)a); }
static void qf_scalar_tanh(void *out, const void *a) { *(qfloat_t*)out = qf_tanh(*(const qfloat_t*)a); }

static void qf_scalar_sqrt(void *out, const void *a) { *(qfloat_t*)out = qf_sqrt(*(const qfloat_t*)a); }

static void qf_scalar_asin(void *out, const void *a) { *(qfloat_t*)out = qf_asin(*(const qfloat_t*)a); }
static void qf_scalar_acos(void *out, const void *a) { *(qfloat_t*)out = qf_acos(*(const qfloat_t*)a); }
static void qf_scalar_atan(void *out, const void *a) { *(qfloat_t*)out = qf_atan(*(const qfloat_t*)a); }

static void qf_scalar_asinh(void *out, const void *a) { *(qfloat_t*)out = qf_asinh(*(const qfloat_t*)a); }
static void qf_scalar_acosh(void *out, const void *a) { *(qfloat_t*)out = qf_acosh(*(const qfloat_t*)a); }
static void qf_scalar_atanh(void *out, const void *a) { *(qfloat_t*)out = qf_atanh(*(const qfloat_t*)a); }

static void qf_scalar_erf (void *out, const void *a) { *(qfloat_t*)out = qf_erf (*(const qfloat_t*)a); }
static void qf_scalar_erfc(void *out, const void *a) { *(qfloat_t*)out = qf_erfc(*(const qfloat_t*)a); }

static const struct elem_fun_vtable qfloat_fun = {
    .exp  = qf_scalar_exp,
    .sin  = qf_scalar_sin,
    .cos  = qf_scalar_cos,
    .tan  = qf_scalar_tan,

    .sinh = qf_scalar_sinh,
    .cosh = qf_scalar_cosh,
    .tanh = qf_scalar_tanh,

    .sqrt = qf_scalar_sqrt,
    .log  = qf_scalar_log,

    .asin = qf_scalar_asin,
    .acos = qf_scalar_acos,
    .atan = qf_scalar_atan,

    .asinh = qf_scalar_asinh,
    .acosh = qf_scalar_acosh,
    .atanh = qf_scalar_atanh,

    .erf  = qf_scalar_erf,
    .erfc = qf_scalar_erfc
};

const struct elem_vtable qfloat_elem = {
    .size            = sizeof(qfloat_t),
    .kind            = ELEM_QFLOAT,
    .public_type     = MAT_TYPE_QFLOAT,
    .add             = qf_add_wrap,
    .sub             = qf_sub_wrap,
    .mul             = qf_mul_wrap,
    .inv             = qf_inv_wrap,
    .abs2            = qf_abs2_wrap,
    .to_real         = qf_to_real_wrap,
    .from_real       = qf_from_real_wrap,
    .conj_elem       = qf_conj_elem,
    .to_qf           = qf_to_qf,
    .abs_qf          = qf_abs_qf,
    .from_qf         = qf_from_qf,
    .to_qc           = qf_to_qc_fn,
    .from_qc         = qf_from_qc_fn,
    .zero            = &QF_ZERO,
    .one             = &QF_ONE,
    .cmp             = qfloat_cmp,
    .print           = qf_print_wrap,
    .create_matrix   = create_matrix_qfloat,
    .create_identity = create_identity_qfloat,

    .fun             = &qfloat_fun
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

static int qcomplex_cmp(const void *a, const void *b)
{
    qcomplex_t A = *(const qcomplex_t *)a;
    qcomplex_t B = *(const qcomplex_t *)b;

    /* equality test */
    if (qc_eq(A, B)) return 0;

    /* arbitrary but consistent ordering */
    if (qf_lt(A.re, B.re)) return -1;
    if (qf_gt(A.re, B.re)) return 1;
    if (qf_lt(A.im, B.im)) return -1;
    return 1;
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

static void qc_to_qc_fn(qcomplex_t *o, const void *a)  { *o = *(const qcomplex_t*)a; }
static void qc_from_qc_fn(void *o, const qcomplex_t *z) { *(qcomplex_t*)o = *z; }

/* ============================================================
   Scalar function vtable for qcomplex
   ============================================================ */

static void qc_scalar_exp (void *out, const void *a) { *(qcomplex_t*)out = qc_exp (*(const qcomplex_t*)a); }
static void qc_scalar_log (void *out, const void *a) { *(qcomplex_t*)out = qc_log (*(const qcomplex_t*)a); }
static void qc_scalar_sin (void *out, const void *a) { *(qcomplex_t*)out = qc_sin (*(const qcomplex_t*)a); }
static void qc_scalar_cos (void *out, const void *a) { *(qcomplex_t*)out = qc_cos (*(const qcomplex_t*)a); }
static void qc_scalar_tan (void *out, const void *a) { *(qcomplex_t*)out = qc_tan (*(const qcomplex_t*)a); }

static void qc_scalar_sinh(void *out, const void *a) { *(qcomplex_t*)out = qc_sinh(*(const qcomplex_t*)a); }
static void qc_scalar_cosh(void *out, const void *a) { *(qcomplex_t*)out = qc_cosh(*(const qcomplex_t*)a); }
static void qc_scalar_tanh(void *out, const void *a) { *(qcomplex_t*)out = qc_tanh(*(const qcomplex_t*)a); }

static void qc_scalar_sqrt(void *out, const void *a) { *(qcomplex_t*)out = qc_sqrt(*(const qcomplex_t*)a); }

static void qc_scalar_asin(void *out, const void *a) { *(qcomplex_t*)out = qc_asin(*(const qcomplex_t*)a); }
static void qc_scalar_acos(void *out, const void *a) { *(qcomplex_t*)out = qc_acos(*(const qcomplex_t*)a); }
static void qc_scalar_atan(void *out, const void *a) { *(qcomplex_t*)out = qc_atan(*(const qcomplex_t*)a); }

static void qc_scalar_asinh(void *out, const void *a) { *(qcomplex_t*)out = qc_asinh(*(const qcomplex_t*)a); }
static void qc_scalar_acosh(void *out, const void *a) { *(qcomplex_t*)out = qc_acosh(*(const qcomplex_t*)a); }
static void qc_scalar_atanh(void *out, const void *a) { *(qcomplex_t*)out = qc_atanh(*(const qcomplex_t*)a); }

static void qc_scalar_erf (void *out, const void *a) { *(qcomplex_t*)out = qc_erf (*(const qcomplex_t*)a); }
static void qc_scalar_erfc(void *out, const void *a) { *(qcomplex_t*)out = qc_erfc(*(const qcomplex_t*)a); }

static const struct elem_fun_vtable qcomplex_fun = {
    .exp  = qc_scalar_exp,
    .sin  = qc_scalar_sin,
    .cos  = qc_scalar_cos,
    .tan  = qc_scalar_tan,

    .sinh = qc_scalar_sinh,
    .cosh = qc_scalar_cosh,
    .tanh = qc_scalar_tanh,

    .sqrt = qc_scalar_sqrt,
    .log  = qc_scalar_log,

    .asin = qc_scalar_asin,
    .acos = qc_scalar_acos,
    .atan = qc_scalar_atan,

    .asinh = qc_scalar_asinh,
    .acosh = qc_scalar_acosh,
    .atanh = qc_scalar_atanh,

    .erf  = qc_scalar_erf,
    .erfc = qc_scalar_erfc
};

const struct elem_vtable qcomplex_elem = {
    .size            = sizeof(qcomplex_t),
    .kind            = ELEM_QCOMPLEX,
    .public_type     = MAT_TYPE_QCOMPLEX,
    .add             = qc_add_wrap,
    .sub             = qc_sub_wrap,
    .mul             = qc_mul_wrap,
    .inv             = qc_inv_wrap,
    .abs2            = qc_abs2_wrap,
    .to_real         = qc_to_real_wrap,
    .from_real       = qc_from_real_wrap,
    .conj_elem       = qc_conj_elem,
    .to_qf           = qc_to_qf,
    .abs_qf          = qc_abs_qf,
    .from_qf         = qc_from_qf,
    .to_qc           = qc_to_qc_fn,
    .from_qc         = qc_from_qc_fn,
    .zero            = &QC_ZERO,
    .one             = &QC_ONE,
    .cmp             = qcomplex_cmp,
    .print           = qc_print_wrap,
    .create_matrix   = create_matrix_qcomplex,
    .create_identity = create_identity_qcomplex,

    .fun             = &qcomplex_fun
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

struct matrix_t *mat_new_d(size_t rows, size_t cols) {
    return double_elem.create_matrix(rows, cols);
}

struct matrix_t *mat_new_qf(size_t rows, size_t cols) {
    return qfloat_elem.create_matrix(rows, cols);
}

struct matrix_t *mat_new_qc(size_t rows, size_t cols) {
    return qcomplex_elem.create_matrix(rows, cols);
}

struct matrix_t *matsq_new_d(size_t n)  {
    return mat_new_d(n, n);
}

struct matrix_t *matsq_new_qf(size_t n) {
    return mat_new_qf(n, n);
}

struct matrix_t *matsq_new_qc(size_t n) {
    return mat_new_qc(n, n);
}

struct matrix_t *mat_create_identity_d(size_t n) {
    return double_elem.create_identity(n);
}

struct matrix_t *mat_create_identity_qf(size_t n) {
    return qfloat_elem.create_identity(n);
}

struct matrix_t *mat_create_identity_qc(size_t n) {
    return qcomplex_elem.create_identity(n);
}

matrix_t *mat_create_d(size_t rows, size_t cols, const double *data)
{
    matrix_t *A = mat_new_d(rows, cols);
    mat_set_data(A, data);
    return A;
}

matrix_t *mat_create_qf(size_t rows, size_t cols, const qfloat_t *data)
{
    matrix_t *A = mat_new_qf(rows, cols);
    mat_set_data(A, data);
    return A;
}

matrix_t *mat_create_qc(size_t rows, size_t cols, const qcomplex_t *data)
{
    matrix_t *A = mat_new_qc(rows, cols);
    mat_set_data(A, data);
    return A;
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

matrix_t *mat_scalar_mul_d(matrix_t *A, double s)
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

matrix_t *mat_scalar_mul_qf(matrix_t *A, qfloat_t s)
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

matrix_t *mat_scalar_mul_qc(matrix_t *A, qcomplex_t s)
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

matrix_t *mat_scalar_div_d(matrix_t *A, double s)
{
    double inv = 1.0 / s;
    return mat_scalar_mul_d(A, inv);
}

matrix_t *mat_scalar_div_qf(matrix_t *A, qfloat_t s)
{
    qfloat_t inv = qf_div(QF_ONE, s);
    return mat_scalar_mul_qf(A, inv);
}

matrix_t *mat_scalar_div_qc(matrix_t *A, qcomplex_t s)
{
    qcomplex_t inv = qc_inv(s);
    return mat_scalar_mul_qc(A, inv);
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

/* ============================================================
   Hermitian fast path (Jacobi)
   ============================================================ */

static int mat_eigendecompose_hermitian(const matrix_t *A, void *eigenvalues, matrix_t **eigenvectors)
{
    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;

    matrix_t *W = mat_copy_dense(A);
    if (!W) return -3;

    matrix_t *V = e->create_identity(n);
    if (!V) { mat_free(W); return -3; }

    double fro2 = 0.0;
    {
        unsigned char fv[64];
        for (size_t i = 0; i < n; i++)
            for (size_t j = 0; j < n; j++) {
                mat_get(W, i, j, fv);
                fro2 += e->abs2(fv);
            }
    }
    double tol = fro2 * 1e-29;

    for (int sweep = 0; sweep < 50; sweep++) {
        for (size_t p = 0; p < n - 1; p++)
            for (size_t q = p + 1; q < n; q++)
                jacobi_apply(W, V, p, q);
        if (offdiag_norm2(W) < tol) break;
    }

    if (eigenvalues) {
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

/* ============================================================
   General QR eigensolver — Francis implicit single-shift
   All internal arithmetic is in qcomplex_t (quad precision).
   ============================================================ */

/* Index into flat n×n qcomplex array */
#define QCM(a,i,j,n) ((a)[(size_t)(i)*(n)+(size_t)(j)])

static inline qfloat_t qc_abs2_qf(qcomplex_t z)
{
    return qf_add(qf_mul(z.re, z.re), qf_mul(z.im, z.im));
}

static inline qcomplex_t qcs(qfloat_t s, qcomplex_t z)
{
    return qc_make(qf_mul(s, z.re), qf_mul(s, z.im));
}

/* Detect whether A is Hermitian: A[i,j] == conj(A[j,i]) within tolerance */
static int mat_is_hermitian(const matrix_t *A)
{
    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;
    unsigned char aij[64], aji[64], cji[64], diff[64];
    double tol2 = 0.0;
    /* build tolerance from Frobenius norm */
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < n; j++) {
            mat_get(A, i, j, aij);
            tol2 += e->abs2(aij);
        }
    tol2 *= 1e-28;
    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 1; j < n; j++) {
            mat_get(A, i, j, aij);
            mat_get(A, j, i, aji);
            e->conj_elem(cji, aji);
            e->sub(diff, aij, cji);
            if (e->abs2(diff) > tol2) return 0;
        }
    return 1;
}

/* Householder reduction to upper Hessenberg form in-place.
 * H is n×n qcomplex array. Q accumulates transformations (Q·H·Q†). */
static void eigen_hess_reduce(qcomplex_t *H, qcomplex_t *Q, size_t n)
{
    qfloat_t two = qf_from_double(2.0);

    for (size_t k = 0; k + 2 <= n; k++) {
        /* Build Householder vector v from H[k+1..n-1, k] */
        size_t len = n - k - 1;
        qcomplex_t *col = (qcomplex_t *)malloc(len * sizeof(qcomplex_t));
        if (!col) return;
        for (size_t i = 0; i < len; i++)
            col[i] = QCM(H, k+1+i, k, n);

        /* norm of column */
        qfloat_t norm2 = QF_ZERO;
        for (size_t i = 0; i < len; i++)
            norm2 = qf_add(norm2, qc_abs2_qf(col[i]));
        qfloat_t norm = qf_sqrt(norm2);

        if (qf_to_double(norm) < 1e-150) { free(col); continue; }

        /* phase of first element: alpha = -norm * (col[0] / |col[0]|) */
        qfloat_t abs0 = qc_abs(col[0]);
        qcomplex_t alpha;
        if (qf_to_double(abs0) < 1e-150) {
            alpha = qc_make(qf_neg(norm), QF_ZERO);
        } else {
            /* alpha = -norm * col[0]/|col[0]| */
            qfloat_t inv0 = qf_div(QF_ONE, abs0);
            qcomplex_t phase = qcs(inv0, col[0]);
            alpha = qcs(qf_neg(norm), phase);
        }

        /* v = col; v[0] -= alpha */
        col[0] = qc_sub(col[0], alpha);

        /* beta = 2 / (v† v) */
        qfloat_t vdotv = QF_ZERO;
        for (size_t i = 0; i < len; i++)
            vdotv = qf_add(vdotv, qc_abs2_qf(col[i]));
        qfloat_t beta = qf_div(two, vdotv);

        /* Apply P = I - beta v v† from left: H ← P H */
        /* w = beta * v† H[k+1:, :] */
        for (size_t j = 0; j < n; j++) {
            qcomplex_t s = QC_ZERO;
            for (size_t i = 0; i < len; i++)
                s = qc_add(s, qc_mul(qc_conj(col[i]), QCM(H, k+1+i, j, n)));
            s = qcs(beta, s);
            for (size_t i = 0; i < len; i++)
                QCM(H, k+1+i, j, n) = qc_sub(QCM(H, k+1+i, j, n), qc_mul(col[i], s));
        }

        /* Apply P from right: H ← H P */
        for (size_t i = 0; i < n; i++) {
            qcomplex_t s = QC_ZERO;
            for (size_t j = 0; j < len; j++)
                s = qc_add(s, qc_mul(QCM(H, i, k+1+j, n), col[j]));
            s = qcs(beta, s);
            for (size_t j = 0; j < len; j++)
                QCM(H, i, k+1+j, n) = qc_sub(QCM(H, i, k+1+j, n), qc_mul(qc_conj(col[j]), s));
        }

        /* Accumulate into Q: Q ← Q P */
        for (size_t i = 0; i < n; i++) {
            qcomplex_t s = QC_ZERO;
            for (size_t j = 0; j < len; j++)
                s = qc_add(s, qc_mul(QCM(Q, i, k+1+j, n), col[j]));
            s = qcs(beta, s);
            for (size_t j = 0; j < len; j++)
                QCM(Q, i, k+1+j, n) = qc_sub(QCM(Q, i, k+1+j, n), qc_mul(qc_conj(col[j]), s));
        }

        free(col);
    }
}

/* Givens rotation: find c (real), s (complex) such that
 *   [ c    conj(s) ] [a]   [r]
 *   [-s      c    ] [b] = [0]
 * where r = hypot(|a|,|b|). */
static void givens_cs(qcomplex_t a, qcomplex_t b,
                      qfloat_t *c_out, qcomplex_t *s_out)
{
    qfloat_t abs_a = qc_abs(a);
    qfloat_t abs_b = qc_abs(b);
    qfloat_t r = qf_sqrt(qf_add(qf_mul(abs_a, abs_a), qf_mul(abs_b, abs_b)));
    if (qf_to_double(r) < 1e-150) {
        *c_out = QF_ONE;
        *s_out = QC_ZERO;
        return;
    }
    qfloat_t c = qf_div(abs_a, r);
    /* s = c * b / a  (complex division; if a≈0 fall back) */
    qcomplex_t s;
    if (qf_to_double(abs_a) < 1e-150) {
        s = qc_make(QF_ZERO, QF_ZERO);
    } else {
        qfloat_t inv_a_abs = qf_div(QF_ONE, abs_a);
        qcomplex_t a_unit = qcs(inv_a_abs, a); /* a/|a| */
        /* s = c * conj(a_unit) * b */
        s = qcs(c, qc_mul(qc_conj(a_unit), b));
    }
    *c_out = c;
    *s_out = s;
}

/* Apply one Givens rotation to columns k and k+1 of H (rows j0..n-1)
 * and rows k and k+1 of H (cols 0..n-1), and Q cols k and k+1. */
static void givens_apply(qcomplex_t *H, qcomplex_t *Q, size_t n,
                         size_t k, size_t row_start,
                         qfloat_t c, qcomplex_t s)
{
    qcomplex_t cs = qc_conj(s);

    /* left multiply rows k, k+1 of H */
    for (size_t j = row_start; j < n; j++) {
        qcomplex_t x = QCM(H, k,   j, n);
        qcomplex_t y = QCM(H, k+1, j, n);
        QCM(H, k,   j, n) = qc_add(qcs(c, x), qc_mul(cs, y));
        QCM(H, k+1, j, n) = qc_add(qc_neg(qc_mul(s, x)), qcs(c, y));
    }

    /* right multiply cols k, k+1 of H */
    for (size_t i = 0; i < n; i++) {
        qcomplex_t x = QCM(H, i, k,   n);
        qcomplex_t y = QCM(H, i, k+1, n);
        QCM(H, i, k,   n) = qc_add(qcs(c, x), qc_mul(s, y));
        QCM(H, i, k+1, n) = qc_add(qc_mul(qc_neg(cs), x), qcs(c, y));
    }

    /* accumulate into Q (eigenvector columns) */
    for (size_t i = 0; i < n; i++) {
        qcomplex_t x = QCM(Q, i, k,   n);
        qcomplex_t y = QCM(Q, i, k+1, n);
        QCM(Q, i, k,   n) = qc_add(qcs(c, x), qc_mul(s, y));
        QCM(Q, i, k+1, n) = qc_add(qc_mul(qc_neg(cs), x), qcs(c, y));
    }
}

/* Wilkinson shift: eigenvalue of bottom 2×2 submatrix of T[0..sz-1, 0..sz-1]
 * that is closest to T[sz-1, sz-1]. */
static qcomplex_t wilkinson_shift(const qcomplex_t *T, size_t sz, size_t n)
{
    if (sz < 2) return QCM(T, sz-1, sz-1, n);
    qcomplex_t a = QCM(T, sz-2, sz-2, n);
    qcomplex_t b = QCM(T, sz-2, sz-1, n);
    qcomplex_t c = QCM(T, sz-1, sz-2, n);
    qcomplex_t d = QCM(T, sz-1, sz-1, n);
    /* eigenvalues of [[a,b],[c,d]]:
     * mu = (a+d)/2 ± sqrt(((a-d)/2)^2 + bc) */
    qfloat_t half = qf_from_double(0.5);
    qcomplex_t tr_half = qcs(half, qc_add(a, d));
    qcomplex_t disc = qc_add(
        qc_mul(qcs(half, qc_sub(a, d)), qcs(half, qc_sub(a, d))),
        qc_mul(b, c));
    qcomplex_t sq = qc_sqrt(disc);
    qcomplex_t e1 = qc_add(tr_half, sq);
    qcomplex_t e2 = qc_sub(tr_half, sq);
    /* pick eigenvalue closer to d */
    qfloat_t d1 = qc_abs(qc_sub(e1, d));
    qfloat_t d2 = qc_abs(qc_sub(e2, d));
    return (qf_to_double(d1) <= qf_to_double(d2)) ? e1 : e2;
}

/* Single QR step with shift mu on active submatrix [0..sz-1].
 * H and Q are n×n flat arrays. */
static void qr_step(qcomplex_t *H, qcomplex_t *Q, size_t sz, size_t n,
                    qcomplex_t mu)
{
    /* Shift: H ← H - mu I */
    for (size_t i = 0; i < sz; i++)
        QCM(H, i, i, n) = qc_sub(QCM(H, i, i, n), mu);

    /* QR via Givens on Hessenberg structure */
    qfloat_t c[256];
    qcomplex_t s[256];
    for (size_t k = 0; k + 1 < sz; k++) {
        givens_cs(QCM(H, k, k, n), QCM(H, k+1, k, n), &c[k], &s[k]);
        givens_apply(H, Q, n, k, k, c[k], s[k]);
    }

    /* Unshift */
    for (size_t i = 0; i < sz; i++)
        QCM(H, i, i, n) = qc_add(QCM(H, i, i, n), mu);
}

/* Implicit single-shift QR: drive H to quasi-upper-triangular (Schur form). */
static void qr_schur(qcomplex_t *H, qcomplex_t *Q, size_t n)
{
    size_t sz = n;
    for (int iter = 0; iter < (int)(30 * n) && sz > 1; iter++) {
        /* deflate trailing near-zero subdiagonals */
        while (sz > 1) {
            qfloat_t sub = qc_abs(QCM(H, sz-1, sz-2, n));
            qfloat_t d1  = qc_abs(QCM(H, sz-2, sz-2, n));
            qfloat_t d2  = qc_abs(QCM(H, sz-1, sz-1, n));
            qfloat_t tol = qf_mul(qf_from_double(1e-29),
                                   qf_add(d1, d2));
            if (qf_to_double(sub) <= qf_to_double(tol)) {
                QCM(H, sz-1, sz-2, n) = QC_ZERO;
                sz--;
            } else {
                break;
            }
        }
        if (sz <= 1) break;
        qcomplex_t mu = wilkinson_shift(H, sz, n);
        qr_step(H, Q, sz, n, mu);
    }
}

/* Back-substitution: given upper-triangular T and eigenvalue T[k,k],
 * compute the k-th eigenvector by back-solving (T - lambda I) x = -e_k
 * for x[0..k-1] (x[k]=1 by convention). */
static void backsub_eigenvec(const qcomplex_t *T, qcomplex_t *Y, size_t n, size_t k)
{
    qcomplex_t lambda = QCM(T, k, k, n);
    /* Y[:,k] will be the eigenvector; zero it first */
    for (size_t i = 0; i < n; i++)
        QCM(Y, i, k, n) = QC_ZERO;
    QCM(Y, k, k, n) = QC_ONE;

    /* Back-substitute rows k-1 down to 0 */
    for (size_t i = k; i-- > 0; ) {
        /* (T[i,i] - lambda) y[i] = -sum_{j=i+1}^{k} T[i,j] y[j] */
        qcomplex_t rhs = QC_ZERO;
        for (size_t j = i + 1; j <= k; j++)
            rhs = qc_sub(rhs, qc_mul(QCM(T, i, j, n), QCM(Y, j, k, n)));
        qcomplex_t diag = qc_sub(QCM(T, i, i, n), lambda);
        double dabs = qf_to_double(qc_abs(diag));
        if (dabs < 1e-150)
            QCM(Y, i, k, n) = QC_ZERO;
        else
            QCM(Y, i, k, n) = qc_div(rhs, diag);
    }

    /* Normalize */
    qfloat_t nrm = QF_ZERO;
    for (size_t i = 0; i <= k; i++)
        nrm = qf_add(nrm, qc_abs2_qf(QCM(Y, i, k, n)));
    nrm = qf_sqrt(nrm);
    if (qf_to_double(nrm) > 1e-150) {
        qfloat_t inv = qf_div(QF_ONE, nrm);
        for (size_t i = 0; i <= k; i++)
            QCM(Y, i, k, n) = qcs(inv, QCM(Y, i, k, n));
    }
}

static int mat_eigendecompose_general(const matrix_t *A, void *eigenvalues,
                                      matrix_t **eigenvectors)
{
    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;

    /* Load A into flat qcomplex array H */
    qcomplex_t *H = (qcomplex_t *)malloc(n * n * sizeof(qcomplex_t));
    if (!H) return -3;
    {
        unsigned char raw[64];
        for (size_t i = 0; i < n; i++)
            for (size_t j = 0; j < n; j++) {
                mat_get(A, i, j, raw);
                e->to_qc(&QCM(H, i, j, n), raw);
            }
    }

    /* Q = identity (accumulates similarity transforms) */
    qcomplex_t *Qm = (qcomplex_t *)calloc(n * n, sizeof(qcomplex_t));
    if (!Qm) { free(H); return -3; }
    for (size_t i = 0; i < n; i++) QCM(Qm, i, i, n) = QC_ONE;

    /* Hessenberg reduction then Schur form */
    eigen_hess_reduce(H, Qm, n);
    qr_schur(H, Qm, n);

    /* Extract eigenvalues from diagonal of Schur matrix */
    if (eigenvalues) {
        char *ev = (char *)eigenvalues;
        for (size_t i = 0; i < n; i++)
            e->from_qc(ev + i * e->size, &QCM(H, i, i, n));
    }

    /* Compute eigenvectors if requested */
    if (eigenvectors) {
        /* Back-substitution: eigenvectors of T in Schur basis */
        qcomplex_t *Y = (qcomplex_t *)calloc(n * n, sizeof(qcomplex_t));
        if (!Y) { free(H); free(Qm); return -3; }

        for (size_t k = 0; k < n; k++)
            backsub_eigenvec(H, Y, n, k);

        /* Transform back: eigvec_A = Q * Y */
        matrix_t *V = e->create_matrix(n, n);
        if (!V) { free(H); free(Qm); free(Y); return -3; }

        unsigned char raw[64];
        for (size_t i = 0; i < n; i++) {
            for (size_t j = 0; j < n; j++) {
                qcomplex_t sum = QC_ZERO;
                for (size_t k = 0; k < n; k++)
                    sum = qc_add(sum, qc_mul(QCM(Qm, i, k, n), QCM(Y, k, j, n)));
                e->from_qc(raw, &sum);
                mat_set(V, i, j, raw);
            }
        }
        free(Y);
        *eigenvectors = V;
    }

    free(H);
    free(Qm);
    return 0;
}

int mat_eigendecompose(const matrix_t *A, void *eigenvalues, matrix_t **eigenvectors)
{
    if (!A) return -1;
    if (A->rows != A->cols) return -2;

    if (mat_is_hermitian(A))
        return mat_eigendecompose_hermitian(A, eigenvalues, eigenvectors);
    return mat_eigendecompose_general(A, eigenvalues, eigenvectors);
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

/* ============================================================
   Schur decomposition: A = Q T Q*
   Full, Hessenberg + implicit double-shift QR, qcomplex backend
   ============================================================ */


   /* ---------- small helpers on qcomplex ---------- */

static inline void qc_add_inplace(qcomplex_t *x, qcomplex_t y)
{
    *x = qc_add(*x, y);
}

static inline void qc_sub_inplace(qcomplex_t *x, qcomplex_t y)
{
    *x = qc_sub(*x, y);
}

static inline void qc_mul_inplace(qcomplex_t *x, qcomplex_t y)
{
    *x = qc_mul(*x, y);
}

static inline qcomplex_t qc_scale(qcomplex_t z, qfloat_t s)
{
    return qc_make(qf_mul(z.re, s), qf_mul(z.im, s));
}

/* norm2 of a complex vector (as qfloat) */
static qfloat_t qc_vec_norm2(const qcomplex_t *x, size_t n)
{
    qfloat_t s = QF_ZERO;
    for (size_t i = 0; i < n; ++i) {
        qfloat_t a = qc_abs(x[i]);
        s = qf_add(s, qf_mul(a, a));
    }
    return s;
}

/* ============================================================
   Convert arbitrary A to dense qcomplex matrix
   ============================================================ */

static matrix_t *mat_to_qcomplex(const matrix_t *A)
{
    if (!A) return NULL;

    matrix_t *Z = qcomplex_elem.create_matrix(A->rows, A->cols);
    if (!Z) return NULL;

    unsigned char v_raw[64];
    qcomplex_t z;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            mat_get(A, i, j, v_raw);
            A->elem->to_qc(&z, v_raw);
            mat_set(Z, i, j, &z);
        }
    }

    return Z;
}

/* ============================================================
   Safe complex Hessenberg reduction: Q* H Q = Hessenberg(H)
   H is overwritten with its Hessenberg form.
   *Qptr is accumulated with the unitary similarity (created if NULL).
   ============================================================ */
static int hessenberg_reduce_qc(matrix_t *H, matrix_t **Qptr)
{
    size_t n = H->rows;
    if (n != H->cols) return -1;

    /* Ensure Q exists and is n×n qcomplex */
    matrix_t *Q = *Qptr;
    if (!Q) {
        Q = qcomplex_elem.create_identity(n);
        if (!Q) return -1;
        *Qptr = Q;
    } else {
        if (Q->rows != n || Q->cols != n)
            return -1;
    }

    if (n <= 2)
        return 0; /* already Hessenberg */

    for (size_t k = 0; k + 2 < n; ++k) {

        size_t m = n - (k + 1);          /* length of Householder vector */
        qcomplex_t *v = malloc(m * sizeof(qcomplex_t));
        if (!v) return -1;

        /* x = H[k+1..n-1, k] */
        qcomplex_t x0;
        mat_get(H, k+1, k, &x0);

        qfloat_t sigma = QF_ZERO;
        for (size_t i = 1; i < m; ++i) {
            qcomplex_t xi;
            mat_get(H, k+1+i, k, &xi);
            qfloat_t a = qc_abs(xi);
            sigma = qf_add(sigma, qf_mul(a, a));
        }

        if (qf_eq(sigma, QF_ZERO)) {
            free(v);
            continue; /* nothing to do */
        }

        qfloat_t x0_abs = qc_abs(x0);
        qfloat_t mu = qf_sqrt(qf_add(qf_mul(x0_abs, x0_abs), sigma));

        qcomplex_t v0;
        if (qf_eq(x0_abs, QF_ZERO)) {
            v0 = qc_make(mu, QF_ZERO);
        } else {
            qcomplex_t x0_scaled = qc_scale(x0, qf_div(QF_ONE, x0_abs));
            v0 = qc_add(x0_scaled, qc_scale(x0_scaled, qf_div(mu, x0_abs)));
        }

        v[0] = v0;
        for (size_t i = 1; i < m; ++i) {
            mat_get(H, k+1+i, k, &v[i]);
        }

        /* normalize v */
        qfloat_t vnorm2 = qc_vec_norm2(v, m);
        qfloat_t vnorm  = qf_sqrt(vnorm2);
        if (!qf_eq(vnorm, QF_ZERO)) {
            qfloat_t inv = qf_div(QF_ONE, vnorm);
            for (size_t i = 0; i < m; ++i)
                v[i] = qc_scale(v[i], inv);
        }

        qfloat_t two = qf_add(QF_ONE, QF_ONE);

        /* apply from left: H := (I - 2 v v*) H, rows k+1..n-1, cols k..n-1 */
        for (size_t j = k; j < n; ++j) {
            qcomplex_t w = QC_ZERO;
            for (size_t i = 0; i < m; ++i) {
                qcomplex_t hij;
                mat_get(H, k+1+i, j, &hij);
                w = qc_add(w, qc_mul(qc_conj(v[i]), hij));
            }
            w = qc_scale(w, two);

            for (size_t i = 0; i < m; ++i) {
                qcomplex_t hij;
                mat_get(H, k+1+i, j, &hij);
                qcomplex_t corr = qc_mul(v[i], w);
                hij = qc_sub(hij, corr);
                mat_set(H, k+1+i, j, &hij);
            }
        }

        /* apply from right: H := H (I - 2 v v*), rows 0..n-1, cols k+1..n-1 */
        for (size_t i = 0; i < n; ++i) {
            qcomplex_t w = QC_ZERO;
            for (size_t j = 0; j < m; ++j) {
                qcomplex_t hij;
                mat_get(H, i, k+1+j, &hij);
                w = qc_add(w, qc_mul(hij, v[j]));
            }
            w = qc_scale(w, two);

            for (size_t j = 0; j < m; ++j) {
                qcomplex_t hij;
                mat_get(H, i, k+1+j, &hij);
                qcomplex_t corr = qc_mul(w, qc_conj(v[j]));
                hij = qc_sub(hij, corr);
                mat_set(H, i, k+1+j, &hij);
            }
        }

        /* accumulate into Q: Q := Q (I - 2 v v*), rows 0..n-1, cols k+1..n-1 */
        for (size_t i = 0; i < n; ++i) {
            qcomplex_t w = QC_ZERO;
            for (size_t j = 0; j < m; ++j) {
                qcomplex_t qij;
                mat_get(Q, i, k+1+j, &qij);
                w = qc_add(w, qc_mul(qij, v[j]));
            }
            w = qc_scale(w, two);

            for (size_t j = 0; j < m; ++j) {
                qcomplex_t qij;
                mat_get(Q, i, k+1+j, &qij);
                qcomplex_t corr = qc_mul(w, qc_conj(v[j]));
                qij = qc_sub(qij, corr);
                mat_set(Q, i, k+1+j, &qij);
            }
        }

        /* enforce exact zeros below subdiagonal in column k */
        for (size_t i = k+2; i < n; ++i) {
            qcomplex_t zero = QC_ZERO;
            mat_set(H, i, k, &zero);
        }

        free(v);
    }

    return 0;
}

/* ============================================================
   Wilkinson shift for trailing 2x2 block of H
   ============================================================ */

static qcomplex_t wilkinson_shift_qc(const matrix_t *H, size_t m)
{
    /* m is last index (inclusive), use 2x2 block H[m-1:m, m-1:m] */
    qcomplex_t a, b, c, d;
    mat_get(H, m - 1, m - 1, &a);
    mat_get(H, m - 1, m,     &b);
    mat_get(H, m,     m - 1, &c);
    mat_get(H, m,     m,     &d);

    qcomplex_t tr = qc_add(a, d);
    qcomplex_t det = qc_sub(qc_mul(a, d), qc_mul(b, c));

    qcomplex_t half_tr = qc_scale(tr, qf_div(QF_ONE, qf_from_double(2.0)));
    qcomplex_t disc = qc_sub(qc_mul(half_tr, half_tr), det);
    qcomplex_t root = qc_sqrt(disc);

    qcomplex_t mu1 = qc_add(half_tr, root);
    qcomplex_t mu2 = qc_sub(half_tr, root);

    /* choose eigenvalue closer to d */
    qcomplex_t diff1 = qc_sub(d, mu1);
    qcomplex_t diff2 = qc_sub(d, mu2);
    qfloat_t n1 = qc_abs(diff1);
    qfloat_t n2 = qc_abs(diff2);

    return (qf_lt(n1, n2) ? mu1 : mu2);
}

/* ============================================================
   Implicit double-shift QR on Hessenberg H, accumulate Q
   ============================================================ */
static int schur_qr_qc(matrix_t *H, matrix_t *Q)
{
    size_t n = H->rows;
    if (n != H->cols) return -1;

    /* nothing to do for 1x1: already in Schur form */
    if (n <= 1)
        return 0;

    const int max_iter = 1000 * (int)n;
    qfloat_t eps = qf_from_double(1e-30);

    /* 2×2: compute Schur form analytically via eigenvector of one eigenvalue. */
    if (n == 2) {
        qcomplex_t a, b, c, d;
        mat_get(H, 0, 0, &a);
        mat_get(H, 0, 1, &b);
        mat_get(H, 1, 0, &c);
        mat_get(H, 1, 1, &d);

        if (qf_lt(qc_abs(c), eps))
            return 0; /* already upper triangular */

        /* eigenvalues from quadratic */
        qcomplex_t half_tr = qc_scale(qc_add(a, d),
                                      qf_div(QF_ONE, qf_from_double(2.0)));
        qcomplex_t det     = qc_sub(qc_mul(a, d), qc_mul(b, c));
        qcomplex_t disc    = qc_sub(qc_mul(half_tr, half_tr), det);
        qcomplex_t root    = qc_sqrt(disc);
        qcomplex_t lam1    = qc_add(half_tr, root);
        qcomplex_t lam2    = qc_sub(half_tr, root);

        /* eigenvector for lam1: [lam1-d, c]^T (always valid since c ≠ 0) */
        qcomplex_t v0 = qc_sub(lam1, d);
        qcomplex_t v1 = c;
        qfloat_t vnrm = qf_sqrt(qf_add(qf_mul(qc_abs(v0), qc_abs(v0)),
                                        qf_mul(qc_abs(v1), qc_abs(v1))));
        if (qf_eq(vnrm, QF_ZERO))
            return -1;
        qfloat_t inv = qf_div(QF_ONE, vnrm);
        qcomplex_t q0 = qc_scale(v0, inv);   /* first column of Q_step */
        qcomplex_t q1 = qc_scale(v1, inv);

        /* T = Q* H Q:  T[0,0]=lam1, T[1,0]=0, T[1,1]=lam2,
         * T[0,1] = conj(q0)*(a*(-conj(q1))+b*conj(q0))
         *        + conj(q1)*(c*(-conj(q1))+d*conj(q0))  */
        qcomplex_t cq0 = qc_conj(q0), cq1 = qc_conj(q1);
        qcomplex_t neg_cq1 = qc_sub(QC_ZERO, cq1);
        qcomplex_t hq01 = qc_add(qc_mul(a, neg_cq1), qc_mul(b, cq0));
        qcomplex_t hq11 = qc_add(qc_mul(c, neg_cq1), qc_mul(d, cq0));
        qcomplex_t t01  = qc_add(qc_mul(cq0, hq01), qc_mul(cq1, hq11));
        qcomplex_t zero2 = QC_ZERO;

        mat_set(H, 0, 0, &lam1);
        mat_set(H, 0, 1, &t01);
        mat_set(H, 1, 0, &zero2);
        mat_set(H, 1, 1, &lam2);

        /* accumulate Q_total = Q_old * Q_step
         * Q_step = [[q0, -conj(q1)], [q1, conj(q0)]] */
        for (size_t i = 0; i < 2; ++i) {
            qcomplex_t qi0, qi1;
            mat_get(Q, i, 0, &qi0);
            mat_get(Q, i, 1, &qi1);
            qcomplex_t new0 = qc_add(qc_mul(qi0, q0),      qc_mul(qi1, q1));
            qcomplex_t new1 = qc_add(qc_mul(qi0, neg_cq1), qc_mul(qi1, cq0));
            mat_set(Q, i, 0, &new0);
            mat_set(Q, i, 1, &new1);
        }
        return 0;
    }

    qcomplex_t zero = QC_ZERO;

    for (size_t m = n - 1; m > 0; ) {

        /* check for deflation at H[m, m-1] */
        qcomplex_t hml;
        mat_get(H, m, m - 1, &hml);
        if (qf_lt(qc_abs(hml), eps)) {
            /* decouple 1x1 block */
            m--;
            continue;
        }

        int iter = 0;
        while (iter++ < max_iter) {

            /* Wilkinson shift */
            qcomplex_t mu = wilkinson_shift_qc(H, m);

            /* perform one implicit QR step with shift mu */
            qcomplex_t h00, h10;
            mat_get(H, 0, 0, &h00);
            mat_get(H, 1, 0, &h10);

            qcomplex_t x = qc_sub(h00, mu);
            qcomplex_t y = h10;

            /* Householder on [x; y] */
            for (size_t k = 0; k < m; ++k) {
                /* form vector [x; y] from H[k,k]-mu, H[k+1,k] */
                mat_get(H, k,   k, &h00);
                mat_get(H, k+1, k, &h10);
                x = qc_sub(h00, mu);
                y = h10;

                qfloat_t nrm2 = qf_add(qf_mul(qc_abs(x), qc_abs(x)),
                                       qf_mul(qc_abs(y), qc_abs(y)));
                if (qf_lt(nrm2, eps)) continue;

                qfloat_t nrm = qf_sqrt(nrm2);
                qcomplex_t u0 = qc_scale(x, qf_div(QF_ONE, nrm));
                qcomplex_t u1 = qc_scale(y, qf_div(QF_ONE, nrm));

                /* apply from left to H rows k,k+1 */
                for (size_t j = k; j < n; ++j) {
                    qcomplex_t hk0, hk1;
                    mat_get(H, k,   j, &hk0);
                    mat_get(H, k+1, j, &hk1);

                    qcomplex_t dot = qc_add(qc_mul(qc_conj(u0), hk0),
                                            qc_mul(qc_conj(u1), hk1));

                    qcomplex_t t0 = qc_sub(hk0, qc_mul(u0, dot));
                    qcomplex_t t1 = qc_sub(hk1, qc_mul(u1, dot));

                    mat_set(H, k,   j, &t0);
                    mat_set(H, k+1, j, &t1);
                }

                /* apply from right to H columns k,k+1 */
                for (size_t i = 0; i < n; ++i) {
                    qcomplex_t hik0, hik1;
                    mat_get(H, i, k,   &hik0);
                    mat_get(H, i, k+1, &hik1);

                    qcomplex_t dot = qc_add(qc_mul(hik0, u0),
                                            qc_mul(hik1, u1));

                    qcomplex_t t0 = qc_sub(hik0, qc_mul(dot, qc_conj(u0)));
                    qcomplex_t t1 = qc_sub(hik1, qc_mul(dot, qc_conj(u1)));

                    mat_set(H, i, k,   &t0);
                    mat_set(H, i, k+1, &t1);
                }

                /* accumulate into Q */
                for (size_t i = 0; i < n; ++i) {
                    qcomplex_t qik0, qik1;
                    mat_get(Q, i, k,   &qik0);
                    mat_get(Q, i, k+1, &qik1);

                    qcomplex_t dot = qc_add(qc_mul(qik0, u0),
                                            qc_mul(qik1, u1));

                    qcomplex_t t0 = qc_sub(qik0, qc_mul(dot, qc_conj(u0)));
                    qcomplex_t t1 = qc_sub(qik1, qc_mul(dot, qc_conj(u1)));

                    mat_set(Q, i, k,   &t0);
                    mat_set(Q, i, k+1, &t1);
                }

                /* prepare x,y for next bulge if it exists */
                if (k + 2 < n) {
                    mat_get(H, k+2, k,   &x);
                    mat_get(H, k+2, k+1, &y);
                }
            }

            /* check for deflation again */
            mat_get(H, m, m - 1, &hml);
            if (qf_lt(qc_abs(hml), eps)) {
                mat_set(H, m, m - 1, &zero);
                break;
            }
        }

        if (iter >= max_iter) {
            /* failed to converge */
            return -1;
        }

        m--;
    }

    return 0;
}

/* ============================================================
   Public Schur API
   ============================================================ */

int mat_schur(const matrix_t *A, mat_schur_t *out)
{
    if (!A || !out) return -1;
    if (A->rows != A->cols) return -2;

    /* Step 1: convert to qcomplex */
    matrix_t *Z = mat_to_qcomplex(A);
    if (!Z) return -3;

    /* Step 2: Hessenberg reduction Z -> H, Q0 */
    matrix_t *Q0 = NULL;
    if (hessenberg_reduce_qc(Z, &Q0) != 0) {
        mat_free(Z);
        return -4;
    }

    /* Step 3: QR iteration on H (in Z), accumulate into Q0 */
    if (schur_qr_qc(Z, Q0) != 0) {
        mat_free(Z);
        mat_free(Q0);
        return -5;
    }

    /* Z is now T (Schur form), Q0 is Q */
    out->Q = Q0;
    out->T = Z;
    return 0;
}

void mat_schur_free(mat_schur_t *S)
{
    if (!S) return;
    if (S->Q) mat_free(S->Q);
    if (S->T) mat_free(S->T);
    S->Q = S->T = NULL;
}

/* ============================================================
   Parlett recurrence on upper triangular T
   ============================================================ */

matrix_t *mat_fun_triangular(const matrix_t *T,
                             void (*scalar_f)(void *out, const void *in))
{
    if (!T || !scalar_f)
        return NULL;

    if (T->rows != T->cols)
        return NULL;

    size_t n = T->rows;
    const struct elem_vtable *e = T->elem;

    matrix_t *F = e->create_matrix(n, n);
    if (!F)
        return NULL;

    unsigned char t_ii[64], t_jj[64], t_ij[64];
    unsigned char f_ii[64], f_jj[64], f_ij[64];
    unsigned char num[64], tmp[64], sum[64];
    unsigned char t_ik[64], t_kj[64], f_ik[64], f_kj[64];
    unsigned char denom[64], inv_denom[64];

    /* 1. Diagonal: f(T_ii) */
    for (size_t i = 0; i < n; ++i) {
        mat_get(T, i, i, t_ii);
        scalar_f(f_ii, t_ii);
        mat_set(F, i, i, f_ii);
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

            /* sum = Σ_{k=i+1}^{j-1} (T_ik F_kj - F_ik T_kj) */
            memcpy(sum, e->zero, e->size);
            for (size_t k = i + 1; k < j; ++k) {
                mat_get(T, i, k, t_ik);
                mat_get(T, k, j, t_kj);
                mat_get(F, k, j, f_kj);
                mat_get(F, i, k, f_ik);

                /* tmp = T_ik * F_kj */
                e->mul(tmp, t_ik, f_kj);
                e->add(sum, sum, tmp);

                /* tmp = F_ik * T_kj */
                e->mul(tmp, f_ik, t_kj);
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
                if (e->cmp(num, e->zero) == 0) {
                    /* F[i,j] = f'(lambda) * T[i,j] via central difference */
                    unsigned char lam_p[64], lam_m[64], fp[64], fm[64];
                    unsigned char h_raw[64], two_h[64], inv_2h[64], deriv[64];
                    e->from_real(h_raw, 1e-10);
                    e->add(lam_p, t_ii, h_raw);
                    e->sub(lam_m, t_ii, h_raw);
                    scalar_f(fp, lam_p);
                    scalar_f(fm, lam_m);
                    e->sub(deriv, fp, fm);
                    e->from_real(two_h, 2e-10);
                    e->inv(inv_2h, two_h);
                    e->mul(deriv, deriv, inv_2h);
                    e->mul(f_ij, deriv, t_ij);
                    mat_set(F, i, j, f_ij);
                    continue;
                } else {
                    /* TODO: full confluent Parlett (higher derivatives needed) */
                    memcpy(f_ij, e->zero, e->size);
                    mat_set(F, i, j, f_ij);
                    continue;
                }
            }

            /* F_ij = num / (T_ii - T_jj) = num * inv(denom) */
            e->inv(inv_denom, denom);
            e->mul(f_ij, num, inv_denom);
            mat_set(F, i, j, f_ij);
        }
    }

    return F;
}
