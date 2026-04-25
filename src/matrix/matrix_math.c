#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "matrix_internal.h"
#include "qfloat.h"

/* ============================================================
   Internal helpers
   ============================================================ */

static double mat_frob2(const matrix_t *A)
{
    const struct elem_vtable *e = A->elem;
    unsigned char v[64];
    double s = 0.0;
    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, v);
            s += e->abs2(v);
        }
    return s;
}

/* Dense copy of A in the same element type. */
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

/* Scale every element of A in-place by the qfloat scalar r.
 * Using from_qf preserves full precision for qfloat/qcomplex elements. */
static void mat_scale_qf(matrix_t *A, qfloat_t r)
{
    const struct elem_vtable *e = A->elem;
    unsigned char r_raw[64], v[64];
    e->from_qf(r_raw, &r);
    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, v);
            e->mul(v, r_raw, v);
            mat_set(A, i, j, v);
        }
}

/* ============================================================
   Taylor-series engine

   Evaluates  f(A) = term0 + Σ_{k=1}^{∞} T_k
   where      T_k  = ratio(k) · T_{k-1} · Astep,   T_0 = term0.

   ratio() returns qfloat_t so that qfloat/qcomplex matrices get
   full precision; e->from_qf narrows to the element type correctly.

   Neither term0 nor Astep are consumed; caller owns them.
   ============================================================ */

#define SERIES_MAX_TERMS 100

/* SERIES_TOL is applied to |T_k|_F² / |result|_F² (both computed as
 * double via e->abs2). 1e-65 pushes the series well past qfloat
 * precision (~31 decimal digits) before declaring convergence. */
#define SERIES_TOL 1e-65

static qfloat_t ratio_exp (int k) { return qf_div(QF_ONE, qf_from_double(k)); }
static qfloat_t ratio_sin (int k) { return qf_neg(qf_div(QF_ONE, qf_mul(qf_from_double(2*k),   qf_from_double(2*k+1)))); }
static qfloat_t ratio_cos (int k) { return qf_neg(qf_div(QF_ONE, qf_mul(qf_from_double(2*k-1), qf_from_double(2*k))));   }
static qfloat_t ratio_sinh(int k) { return        qf_div(QF_ONE, qf_mul(qf_from_double(2*k),   qf_from_double(2*k+1)));  }
static qfloat_t ratio_cosh(int k) { return        qf_div(QF_ONE, qf_mul(qf_from_double(2*k-1), qf_from_double(2*k)));    }

static matrix_t *mat_series_sum(
    const matrix_t *Astep,
    const matrix_t *term0,
    qfloat_t (*ratio)(int k))
{
    matrix_t *result = mat_copy_dense(term0);
    if (!result) return NULL;

    matrix_t *term = NULL;

    for (int k = 1; k <= SERIES_MAX_TERMS; k++) {
        const matrix_t *prev = (k == 1) ? term0 : term;
        matrix_t *prod = mat_mul(prev, Astep);
        if (k > 1) { mat_free(term); term = NULL; }
        if (!prod) { mat_free(result); return NULL; }

        mat_scale_qf(prod, ratio(k));
        term = prod;

        matrix_t *new_result = mat_add(result, term);
        if (!new_result) { mat_free(term); mat_free(result); return NULL; }
        mat_free(result);
        result = new_result;

        double term_n2   = mat_frob2(term);
        double result_n2 = mat_frob2(result);
        if (term_n2 < SERIES_TOL * result_n2 + SERIES_TOL)
            break;
    }

    if (term) mat_free(term);
    return result;
}

/* ============================================================
   mat_exp  —  scaling and squaring
   ============================================================ */

matrix_t *mat_exp(const matrix_t *A)
{
    if (!A || A->rows != A->cols) return NULL;

    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;

    /* choose m so ||A / 2^m||_F ≤ 1 */
    int m = 0;
    double norm = sqrt(mat_frob2(A));
    while (norm > 1.0) { norm /= 2.0; m++; }

    matrix_t *B = mat_copy_dense(A);
    if (!B) return NULL;
    if (m > 0) mat_scale_qf(B, qf_from_double(ldexp(1.0, -m)));

    matrix_t *I = e->create_identity(n);
    if (!I) { mat_free(B); return NULL; }

    matrix_t *E = mat_series_sum(B, I, ratio_exp);
    mat_free(B);
    mat_free(I);
    if (!E) return NULL;

    /* squaring phase: E ← E² repeated m times */
    for (int i = 0; i < m; i++) {
        matrix_t *E2 = mat_mul(E, E);
        mat_free(E);
        if (!E2) return NULL;
        E = E2;
    }

    return E;
}

/* ============================================================
   mat_sincos  —  private helper

   Computes sin(A) and cos(A) simultaneously via halving/doubling:
     sin(2X) = 2 sin(X) cos(X)
     cos(2X) = 2 cos²(X) − I       [using sin²+cos²=I]
   ============================================================ */

static int mat_sincos(const matrix_t *A, matrix_t **psin, matrix_t **pcos)
{
    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;

    /* halve until ||Ah||_F ≤ 0.5 */
    int m = 0;
    double norm = sqrt(mat_frob2(A));
    while (norm > 0.5) { norm /= 2.0; m++; }

    matrix_t *Ah = mat_copy_dense(A);
    if (!Ah) return -1;
    if (m > 0) mat_scale_qf(Ah, qf_from_double(ldexp(1.0, -m)));

    matrix_t *Ah2 = mat_mul(Ah, Ah);
    if (!Ah2) { mat_free(Ah); return -1; }

    matrix_t *I = e->create_identity(n);
    if (!I) { mat_free(Ah); mat_free(Ah2); return -1; }

    matrix_t *s = mat_series_sum(Ah2, Ah, ratio_sin);
    if (!s) { mat_free(Ah); mat_free(Ah2); mat_free(I); return -1; }

    matrix_t *c = mat_series_sum(Ah2, I, ratio_cos);
    mat_free(Ah); mat_free(Ah2); mat_free(I);
    if (!c) { mat_free(s); return -1; }

    /* doubling phase */
    qfloat_t two = qf_from_double(2.0);

    for (int i = 0; i < m; i++) {
        /* new_s = 2 s c */
        matrix_t *sc = mat_mul(s, c);
        mat_free(s);
        if (!sc) { mat_free(c); return -1; }
        mat_scale_qf(sc, two);
        s = sc;

        /* new_c = 2 c² − I */
        matrix_t *c2 = mat_mul(c, c);
        mat_free(c);
        if (!c2) { mat_free(s); return -1; }
        mat_scale_qf(c2, two);

        matrix_t *Id = e->create_identity(n);
        if (!Id) { mat_free(s); mat_free(c2); return -1; }
        c = mat_sub(c2, Id);
        mat_free(c2); mat_free(Id);
        if (!c) { mat_free(s); return -1; }
    }

    *psin = s;
    *pcos = c;
    return 0;
}

/* ============================================================
   mat_sinhcosh  —  private helper

   Computes sinh(A) and cosh(A) simultaneously via halving/doubling:
     sinh(2X) = 2 sinh(X) cosh(X)
     cosh(2X) = 2 cosh²(X) − I     [using cosh²−sinh²=I]
   ============================================================ */

static int mat_sinhcosh(const matrix_t *A, matrix_t **psinh, matrix_t **pcosh)
{
    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;

    /* halve until ||Ah||_F ≤ 1 */
    int m = 0;
    double norm = sqrt(mat_frob2(A));
    while (norm > 1.0) { norm /= 2.0; m++; }

    matrix_t *Ah = mat_copy_dense(A);
    if (!Ah) return -1;
    if (m > 0) mat_scale_qf(Ah, qf_from_double(ldexp(1.0, -m)));

    matrix_t *Ah2 = mat_mul(Ah, Ah);
    if (!Ah2) { mat_free(Ah); return -1; }

    matrix_t *I = e->create_identity(n);
    if (!I) { mat_free(Ah); mat_free(Ah2); return -1; }

    matrix_t *sh = mat_series_sum(Ah2, Ah, ratio_sinh);
    if (!sh) { mat_free(Ah); mat_free(Ah2); mat_free(I); return -1; }

    matrix_t *ch = mat_series_sum(Ah2, I, ratio_cosh);
    mat_free(Ah); mat_free(Ah2); mat_free(I);
    if (!ch) { mat_free(sh); return -1; }

    /* doubling phase */
    qfloat_t two = qf_from_double(2.0);

    for (int i = 0; i < m; i++) {
        /* new_sh = 2 sh ch */
        matrix_t *shch = mat_mul(sh, ch);
        mat_free(sh);
        if (!shch) { mat_free(ch); return -1; }
        mat_scale_qf(shch, two);
        sh = shch;

        /* new_ch = 2 ch² − I */
        matrix_t *ch2 = mat_mul(ch, ch);
        mat_free(ch);
        if (!ch2) { mat_free(sh); return -1; }
        mat_scale_qf(ch2, two);

        matrix_t *Id = e->create_identity(n);
        if (!Id) { mat_free(sh); mat_free(ch2); return -1; }
        ch = mat_sub(ch2, Id);
        mat_free(ch2); mat_free(Id);
        if (!ch) { mat_free(sh); return -1; }
    }

    *psinh = sh;
    *pcosh = ch;
    return 0;
}

/* ============================================================
   Public API
   ============================================================ */

matrix_t *mat_sin(const matrix_t *A)
{
    if (!A || A->rows != A->cols) return NULL;
    matrix_t *s = NULL, *c = NULL;
    if (mat_sincos(A, &s, &c) != 0) return NULL;
    mat_free(c);
    return s;
}

matrix_t *mat_cos(const matrix_t *A)
{
    if (!A || A->rows != A->cols) return NULL;
    matrix_t *s = NULL, *c = NULL;
    if (mat_sincos(A, &s, &c) != 0) return NULL;
    mat_free(s);
    return c;
}

matrix_t *mat_tan(const matrix_t *A)
{
    if (!A || A->rows != A->cols) return NULL;
    matrix_t *s = NULL, *c = NULL;
    if (mat_sincos(A, &s, &c) != 0) return NULL;
    matrix_t *inv_c = mat_inverse(c);
    mat_free(c);
    if (!inv_c) { mat_free(s); return NULL; }
    matrix_t *result = mat_mul(s, inv_c);
    mat_free(s);
    mat_free(inv_c);
    return result;
}

matrix_t *mat_sinh(const matrix_t *A)
{
    if (!A || A->rows != A->cols) return NULL;
    matrix_t *sh = NULL, *ch = NULL;
    if (mat_sinhcosh(A, &sh, &ch) != 0) return NULL;
    mat_free(ch);
    return sh;
}

matrix_t *mat_cosh(const matrix_t *A)
{
    if (!A || A->rows != A->cols) return NULL;
    matrix_t *sh = NULL, *ch = NULL;
    if (mat_sinhcosh(A, &sh, &ch) != 0) return NULL;
    mat_free(sh);
    return ch;
}

matrix_t *mat_tanh(const matrix_t *A)
{
    if (!A || A->rows != A->cols) return NULL;
    matrix_t *sh = NULL, *ch = NULL;
    if (mat_sinhcosh(A, &sh, &ch) != 0) return NULL;
    matrix_t *inv_ch = mat_inverse(ch);
    mat_free(ch);
    if (!inv_ch) { mat_free(sh); return NULL; }
    matrix_t *result = mat_mul(sh, inv_ch);
    mat_free(sh);
    mat_free(inv_ch);
    return result;
}

/* ============================================================
   mat_sqrt  —  Denman-Beavers iteration
   X_{k+1} = (X_k + Y_k^{-1}) / 2
   Y_{k+1} = (Y_k + X_k^{-1}) / 2
   Converges quadratically; X → sqrt(A), Y → sqrt(A)^{-1}.
   ============================================================ */

matrix_t *mat_sqrt(const matrix_t *A)
{
    if (!A || A->rows != A->cols) return NULL;
    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;

    /* Zero matrix: sqrt(0) = 0 */
    if (mat_frob2(A) == 0.0) return mat_copy_dense(A);

    matrix_t *X = mat_copy_dense(A);
    if (!X) return NULL;

    matrix_t *Y = e->create_identity(n);
    if (!Y) { mat_free(X); return NULL; }

    qfloat_t half = qf_from_double(0.5);

    for (int iter = 0; iter < 50; iter++) {
        matrix_t *Xinv = mat_inverse(X);
        if (!Xinv) { mat_free(X); mat_free(Y); return NULL; }
        matrix_t *Yinv = mat_inverse(Y);
        if (!Yinv) { mat_free(Xinv); mat_free(X); mat_free(Y); return NULL; }

        /* newX = (X + Yinv) / 2 */
        matrix_t *tmp = mat_add(X, Yinv);
        mat_free(Yinv);
        if (!tmp) { mat_free(Xinv); mat_free(X); mat_free(Y); return NULL; }
        mat_scale_qf(tmp, half);

        /* newY = (Y + Xinv) / 2 */
        matrix_t *tmp2 = mat_add(Y, Xinv);
        mat_free(Xinv);
        if (!tmp2) { mat_free(tmp); mat_free(X); mat_free(Y); return NULL; }
        mat_scale_qf(tmp2, half);

        /* convergence: ||X_{k+1} - X_k||_F² / ||X_{k+1}||_F² */
        matrix_t *diff = mat_sub(tmp, X);
        mat_free(X); mat_free(Y);
        if (!diff) { mat_free(tmp); mat_free(tmp2); return NULL; }

        double diff2   = mat_frob2(diff);
        double result2 = mat_frob2(tmp);
        mat_free(diff);

        X = tmp;
        Y = tmp2;

        if (diff2 < SERIES_TOL * result2 + SERIES_TOL)
            break;
    }

    mat_free(Y);
    return X;
}

/* ============================================================
   mat_log  —  repeated square root + Taylor series
   B = A^{1/2^m} scaled until ||B - I||_F ≤ 0.5,
   log(B) via  L = (B-I) - (B-I)²/2 + (B-I)³/3 - ...
   result = 2^m * log(B).
   ============================================================ */

static qfloat_t ratio_log(int k) {
    /* term_k = -(k/(k+1)) * term_{k-1} * B,  term_0 = B-I
     * gives B - B²/2 + B³/3 - ...  */
    return qf_neg(qf_div(qf_from_double(k), qf_from_double(k + 1)));
}

matrix_t *mat_log(const matrix_t *A)
{
    if (!A || A->rows != A->cols) return NULL;
    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;

    /* Repeated square roots until ||B - I||_F ≤ 0.5 */
    int m = 0;
    matrix_t *B = mat_copy_dense(A);
    if (!B) return NULL;

    matrix_t *I = e->create_identity(n);
    if (!I) { mat_free(B); return NULL; }

    for (;;) {
        matrix_t *D = mat_sub(B, I);
        if (!D) { mat_free(B); mat_free(I); return NULL; }
        double nrm = sqrt(mat_frob2(D));
        mat_free(D);
        if (nrm <= 0.5 || m >= 64) break;
        matrix_t *Bsq = mat_sqrt(B);
        mat_free(B);
        if (!Bsq) { mat_free(I); return NULL; }
        B = Bsq;
        m++;
    }

    /* C = B - I */
    matrix_t *C = mat_sub(B, I);
    mat_free(B); mat_free(I);
    if (!C) return NULL;

    /* log(I + C) = C - C²/2 + C³/3 - ... via series with Astep = C */
    matrix_t *L = mat_series_sum(C, C, ratio_log);
    mat_free(C);
    if (!L) return NULL;

    /* result = 2^m * L */
    if (m > 0)
        mat_scale_qf(L, qf_from_double(ldexp(1.0, m)));

    return L;
}

/* ============================================================
   Inverse trig / hyperbolic via Taylor series
   All use Astep = A², term0 = A.

   asin(A)  = A + (1/6)A³ + (3/40)A⁵ + ...
     ratio_asin(k) = (2k-1)² / (2k(2k+1))

   atan(A)  = A - A³/3 + A⁵/5 - ...
     ratio_atan(k) = -(2k-1)/(2k+1)

   atanh(A) = A + A³/3 + A⁵/5 + ...
     ratio_atanh(k) = (2k-1)/(2k+1)

   asinh(A) = A - (1/6)A³ + (3/40)A⁵ - ...
     ratio_asinh(k) = -(2k-1)² / (2k(2k+1))

   acos(A)  = π/2·I - asin(A)
   acosh(A) = log(A + sqrt(A²-I))
   ============================================================ */

static qfloat_t ratio_asin(int k) {
    qfloat_t num = qf_from_double((2.0*k - 1) * (2.0*k - 1));
    qfloat_t den = qf_from_double(2.0*k * (2.0*k + 1));
    return qf_div(num, den);
}

static qfloat_t ratio_atan(int k) {
    return qf_neg(qf_div(qf_from_double(2.0*k - 1), qf_from_double(2.0*k + 1)));
}

static qfloat_t ratio_atanh(int k) {
    return qf_div(qf_from_double(2.0*k - 1), qf_from_double(2.0*k + 1));
}

static qfloat_t ratio_asinh(int k) {
    qfloat_t num = qf_neg(qf_from_double((2.0*k - 1) * (2.0*k - 1)));
    qfloat_t den = qf_from_double(2.0*k * (2.0*k + 1));
    return qf_div(num, den);
}

matrix_t *mat_asin(const matrix_t *A)
{
    if (!A || A->rows != A->cols) return NULL;

    matrix_t *A2 = mat_mul(A, A);
    if (!A2) return NULL;

    matrix_t *Ac = mat_copy_dense(A);
    if (!Ac) { mat_free(A2); return NULL; }

    matrix_t *result = mat_series_sum(A2, Ac, ratio_asin);
    mat_free(A2); mat_free(Ac);
    return result;
}

matrix_t *mat_acos(const matrix_t *A)
{
    if (!A || A->rows != A->cols) return NULL;
    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;

    matrix_t *S = mat_asin(A);
    if (!S) return NULL;

    /* π/2 · I */
    matrix_t *I = e->create_identity(n);
    if (!I) { mat_free(S); return NULL; }
    mat_scale_qf(I, QF_PI_2);

    matrix_t *result = mat_sub(I, S);
    mat_free(I); mat_free(S);
    return result;
}

matrix_t *mat_atan(const matrix_t *A)
{
    if (!A || A->rows != A->cols) return NULL;

    matrix_t *A2 = mat_mul(A, A);
    if (!A2) return NULL;

    matrix_t *Ac = mat_copy_dense(A);
    if (!Ac) { mat_free(A2); return NULL; }

    matrix_t *result = mat_series_sum(A2, Ac, ratio_atan);
    mat_free(A2); mat_free(Ac);
    return result;
}

matrix_t *mat_asinh(const matrix_t *A)
{
    if (!A || A->rows != A->cols) return NULL;

    matrix_t *A2 = mat_mul(A, A);
    if (!A2) return NULL;

    matrix_t *Ac = mat_copy_dense(A);
    if (!Ac) { mat_free(A2); return NULL; }

    matrix_t *result = mat_series_sum(A2, Ac, ratio_asinh);
    mat_free(A2); mat_free(Ac);
    return result;
}

matrix_t *mat_acosh(const matrix_t *A)
{
    if (!A || A->rows != A->cols) return NULL;
    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;

    /* A² - I */
    matrix_t *A2 = mat_mul(A, A);
    if (!A2) return NULL;

    matrix_t *I = e->create_identity(n);
    if (!I) { mat_free(A2); return NULL; }
    mat_scale_qf(I, qf_neg(QF_ONE));

    matrix_t *A2mI = mat_add(A2, I);
    mat_free(A2); mat_free(I);
    if (!A2mI) return NULL;

    matrix_t *sq = mat_sqrt(A2mI);
    mat_free(A2mI);
    if (!sq) return NULL;

    matrix_t *Ac = mat_copy_dense(A);
    if (!Ac) { mat_free(sq); return NULL; }

    matrix_t *arg = mat_add(Ac, sq);
    mat_free(Ac); mat_free(sq);
    if (!arg) return NULL;

    matrix_t *result = mat_log(arg);
    mat_free(arg);
    return result;
}

matrix_t *mat_atanh(const matrix_t *A)
{
    if (!A || A->rows != A->cols) return NULL;

    matrix_t *A2 = mat_mul(A, A);
    if (!A2) return NULL;

    matrix_t *Ac = mat_copy_dense(A);
    if (!Ac) { mat_free(A2); return NULL; }

    matrix_t *result = mat_series_sum(A2, Ac, ratio_atanh);
    mat_free(A2); mat_free(Ac);
    return result;
}

/* ============================================================
   mat_erf  —  Taylor series scaled by 2/√π

   erf(A) = (2/√π) Σ_{k=0}^∞ (-1)^k A^{2k+1} / (k! (2k+1))
   ratio_erf(k) = -(2k-1) / (k (2k+1))
   ============================================================ */

static qfloat_t ratio_erf(int k) {
    return qf_neg(qf_div(qf_from_double(2.0*k - 1),
                         qf_mul(qf_from_double(k), qf_from_double(2.0*k + 1))));
}

matrix_t *mat_erf(const matrix_t *A)
{
    if (!A || A->rows != A->cols) return NULL;

    matrix_t *A2 = mat_mul(A, A);
    if (!A2) return NULL;

    matrix_t *Ac = mat_copy_dense(A);
    if (!Ac) { mat_free(A2); return NULL; }

    matrix_t *S = mat_series_sum(A2, Ac, ratio_erf);
    mat_free(A2); mat_free(Ac);
    if (!S) return NULL;

    qfloat_t two_over_sqrt_pi = qf_div(qf_from_double(2.0), qf_sqrt(QF_PI));
    mat_scale_qf(S, two_over_sqrt_pi);
    return S;
}

/* ============================================================
   mat_erfc  —  erfc(A) = I - erf(A)
   ============================================================ */

matrix_t *mat_erfc(const matrix_t *A)
{
    if (!A || A->rows != A->cols) return NULL;
    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;

    matrix_t *E = mat_erf(A);
    if (!E) return NULL;

    matrix_t *I = e->create_identity(n);
    if (!I) { mat_free(E); return NULL; }

    matrix_t *result = mat_sub(I, E);
    mat_free(I); mat_free(E);
    return result;
}

/* ============================================================
   mat_pow_int  —  binary exponentiation
   Negative exponents invert A first.
   ============================================================ */

matrix_t *mat_pow_int(const matrix_t *A, int n)
{
    if (!A || A->rows != A->cols) return NULL;
    size_t sz = A->rows;
    const struct elem_vtable *e = A->elem;

    matrix_t *base;
    unsigned int p;
    if (n < 0) {
        base = mat_inverse(A);
        if (!base) return NULL;
        p = (unsigned int)(-(long long)n);
    } else {
        base = mat_copy_dense(A);
        if (!base) return NULL;
        p = (unsigned int)n;
    }

    matrix_t *result = e->create_identity(sz);
    if (!result) { mat_free(base); return NULL; }

    while (p > 0u) {
        if (p & 1u) {
            matrix_t *tmp = mat_mul(result, base);
            mat_free(result);
            if (!tmp) { mat_free(base); return NULL; }
            result = tmp;
        }
        p >>= 1u;
        if (p > 0u) {
            matrix_t *tmp = mat_mul(base, base);
            mat_free(base);
            if (!tmp) { mat_free(result); return NULL; }
            base = tmp;
        }
    }

    mat_free(base);
    return result;
}

/* ============================================================
   mat_pow  —  A^s = exp(s · log(A))
   Requires A to admit a principal logarithm (positive definite).
   ============================================================ */

matrix_t *mat_pow(const matrix_t *A, double s)
{
    if (!A || A->rows != A->cols) return NULL;

    matrix_t *L = mat_log(A);
    if (!L) return NULL;

    mat_scale_qf(L, qf_from_double(s));

    matrix_t *result = mat_exp(L);
    mat_free(L);
    return result;
}
