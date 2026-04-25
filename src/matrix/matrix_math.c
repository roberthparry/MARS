#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "matrix_internal.h"

/* ============================================================
   Internal helpers
   ============================================================ */

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
   Schur-based matrix function engine
   ============================================================ */

matrix_t *mat_fun_schur(const matrix_t *A,
                        void (*scalar_f)(void *out, const void *in))
{
    if (!A || !scalar_f)
        return NULL;

    const struct elem_vtable *orig_elem = A->elem;
    size_t n = A->rows;

    mat_schur_t S;
    if (mat_schur(A, &S) != 0)
        return NULL;

    /* S.T is always qcomplex; scalar_f must already be the qcomplex
     * version of the desired function (callers are responsible for this). */
    matrix_t *FT = mat_fun_triangular(S.T, scalar_f);
    if (!FT) {
        mat_schur_free(&S);
        return NULL;
    }

    /* Q f(T) Q* */
    matrix_t *QFT = mat_mul(S.Q, FT);
    if (!QFT) {
        mat_free(FT);
        mat_schur_free(&S);
        return NULL;
    }

    matrix_t *QH = mat_hermitian(S.Q);
    if (!QH) {
        mat_free(FT);
        mat_free(QFT);
        mat_schur_free(&S);
        return NULL;
    }

    matrix_t *R = mat_mul(QFT, QH);   /* qcomplex result */

    mat_free(FT);
    mat_free(QFT);
    mat_free(QH);
    mat_schur_free(&S);

    if (!R) return NULL;

    /* Convert R back to the original element type. */
    if (orig_elem == R->elem)
        return R;   /* already correct type (qcomplex input) */

    matrix_t *out = orig_elem->create_matrix(n, n);
    if (!out) { mat_free(R); return NULL; }

    qcomplex_t qc;
    unsigned char raw[64];
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            mat_get(R, i, j, &qc);
            orig_elem->from_qc(raw, &qc);
            mat_set(out, i, j, raw);
        }
    }

    mat_free(R);
    return out;
}

matrix_t *mat_exp(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->exp);
}

matrix_t *mat_log(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->log);
}

matrix_t *mat_sin(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->sin);
}

matrix_t *mat_cos(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->cos);
}

matrix_t *mat_tan(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->tan);
}

matrix_t *mat_sinh(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->sinh);
}

matrix_t *mat_cosh(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->cosh);
}

matrix_t *mat_tanh(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->tanh);
}

matrix_t *mat_sqrt(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->sqrt);
}

matrix_t *mat_asin(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->asin);
}

matrix_t *mat_acos(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->acos);
}

matrix_t *mat_atan(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->atan);
}

matrix_t *mat_asinh(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->asinh);
}

matrix_t *mat_acosh(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->acosh);
}

matrix_t *mat_atanh(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->atanh);
}

matrix_t *mat_erf(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->erf);
}

matrix_t *mat_erfc(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->erfc);
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
