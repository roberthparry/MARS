#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "matrix_internal.h"

/* ============================================================
   Internal helpers
   ============================================================ */

typedef struct mat_fun_cache_entry {
    const matrix_t *key;
    matrix_t *spectral_Vq;
    qcomplex_t *spectral_evals;
    matrix_t *exp_preimage;
    struct mat_fun_cache_entry *next;
} mat_fun_cache_entry_t;

static mat_fun_cache_entry_t *mat_fun_cache_head = NULL;

static mat_fun_cache_entry_t *mat_fun_cache_find(const matrix_t *A)
{
    for (mat_fun_cache_entry_t *it = mat_fun_cache_head; it; it = it->next)
        if (it->key == A)
            return it;
    return NULL;
}

void mat_fun_cache_forget(const matrix_t *A)
{
    mat_fun_cache_entry_t **link = &mat_fun_cache_head;

    while (*link) {
        mat_fun_cache_entry_t *entry = *link;
        if (entry->key != A) {
            link = &entry->next;
            continue;
        }

        *link = entry->next;
        mat_free(entry->spectral_Vq);
        free(entry->spectral_evals);
        mat_free(entry->exp_preimage);
        free(entry);
        return;
    }
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

    mat_fun_cache_entry_t *cache = mat_fun_cache_find(A);
    if (!cache)
        return;

    if (cache->spectral_evals) {
        for (size_t i = 0; i < A->rows; ++i)
            cache->spectral_evals[i] = qc_make(qf_mul(cache->spectral_evals[i].re, r),
                                               qf_mul(cache->spectral_evals[i].im, r));
    }

    if (cache->exp_preimage) {
        mat_free(cache->exp_preimage);
        cache->exp_preimage = NULL;
    }
}

static matrix_t *mat_to_qcomplex_local(const matrix_t *A)
{
    return mat_convert_with_store(A, &qcomplex_elem, A ? A->store : NULL);
}

static void mat_attach_spectral_cache(matrix_t *A,
                                      const matrix_t *Vq,
                                      const qcomplex_t *evals)
{
    if (!A || !Vq || !evals || A->rows != A->cols)
        return;

    matrix_t *Vcopy = mat_to_qcomplex_local(Vq);
    qcomplex_t *ecopy = calloc(A->rows, sizeof(*ecopy));
    if (!Vcopy || !ecopy) {
        mat_free(Vcopy);
        free(ecopy);
        return;
    }

    memcpy(ecopy, evals, A->rows * sizeof(*ecopy));

    mat_fun_cache_entry_t *entry = mat_fun_cache_find(A);
    if (!entry) {
        entry = calloc(1, sizeof(*entry));
        if (!entry) {
            mat_free(Vcopy);
            free(ecopy);
            return;
        }
        entry->key = A;
        entry->next = mat_fun_cache_head;
        mat_fun_cache_head = entry;
    }

    mat_free(entry->spectral_Vq);
    free(entry->spectral_evals);

    entry->spectral_Vq = Vcopy;
    entry->spectral_evals = ecopy;
}

static matrix_t *mat_fun_from_spectral_cache(const matrix_t *A,
                                             void (*scalar_f)(void *out, const void *in))
{
    size_t n = A->rows;
    const struct elem_vtable *orig_elem = A->elem;
    mat_fun_cache_entry_t *cache = mat_fun_cache_find(A);
    matrix_t *FD = mat_create_diagonal_with_elem(n, &qcomplex_elem);
    qcomplex_t *mapped = calloc(n, sizeof(*mapped));
    if (!cache || !cache->spectral_Vq || !cache->spectral_evals || !FD || !mapped) {
        mat_free(FD);
        free(mapped);
        return NULL;
    }

    for (size_t i = 0; i < n; ++i) {
        scalar_f(&mapped[i], &cache->spectral_evals[i]);
        mat_set(FD, i, i, &mapped[i]);
    }

    matrix_t *VF = mat_mul(cache->spectral_Vq, FD);
    matrix_t *Vinv = mat_inverse(cache->spectral_Vq);
    matrix_t *R = (VF && Vinv) ? mat_mul(VF, Vinv) : NULL;

    mat_free(FD);
    mat_free(VF);
    mat_free(Vinv);
    if (!R) {
        free(mapped);
        return NULL;
    }

    mat_attach_spectral_cache(R, cache->spectral_Vq, mapped);

    if (orig_elem == R->elem) {
        free(mapped);
        return R;
    }

    matrix_t *out = mat_convert_with_store(R, orig_elem, R->store);
    if (!out) {
        free(mapped);
        mat_free(R);
        return NULL;
    }

    mat_attach_spectral_cache(out, cache->spectral_Vq, mapped);
    free(mapped);
    mat_free(R);
    return out;
}

static void mat_set_exp_preimage_cache(matrix_t *A, const matrix_t *preimage)
{
    if (!A || !preimage)
        return;

    matrix_t *copy = mat_copy_preserving_store(preimage);
    if (!copy)
        return;

    mat_fun_cache_entry_t *entry = mat_fun_cache_find(A);
    if (!entry) {
        entry = calloc(1, sizeof(*entry));
        if (!entry) {
            mat_free(copy);
            return;
        }
        entry->key = A;
        entry->next = mat_fun_cache_head;
        mat_fun_cache_head = entry;
    }

    mat_free(entry->exp_preimage);
    entry->exp_preimage = copy;
}

static matrix_t *mat_fun_hermitian(const matrix_t *A,
                                   void (*scalar_f)(void *out, const void *in))
{
    size_t n = A->rows;
    const struct elem_vtable *orig_elem = A->elem;
    void *eval_buf = calloc(n, orig_elem->size);
    qcomplex_t *evals = calloc(n, sizeof(*evals));
    qcomplex_t *mapped = calloc(n, sizeof(*mapped));
    if (!eval_buf || !evals || !mapped) {
        free(eval_buf);
        free(evals);
        free(mapped);
        return NULL;
    }

    matrix_t *V = NULL;
    matrix_t *Vq = NULL;
    if (mat_eigendecompose(A, eval_buf, &V) != 0) {
        free(eval_buf);
        free(evals);
        free(mapped);
        return NULL;
    }

    for (size_t i = 0; i < n; ++i)
        orig_elem->to_qc(&evals[i], (const char *)eval_buf + i * orig_elem->size);

    Vq = mat_to_qcomplex_local(V);
    if (!Vq) {
        mat_free(V);
        free(eval_buf);
        free(evals);
        free(mapped);
        return NULL;
    }

    matrix_t *FD = mat_create_diagonal_with_elem(n, &qcomplex_elem);
    if (!FD) {
        mat_free(Vq);
        mat_free(V);
        free(eval_buf);
        free(evals);
        free(mapped);
        return NULL;
    }

    for (size_t i = 0; i < n; ++i) {
        scalar_f(&mapped[i], &evals[i]);
        mat_set(FD, i, i, &mapped[i]);
    }

    matrix_t *VF = mat_mul(Vq, FD);
    matrix_t *Vinv = mat_inverse(Vq);
    matrix_t *R = (VF && Vinv) ? mat_mul(VF, Vinv) : NULL;

    mat_free(FD);
    mat_free(VF);
    mat_free(Vinv);

    if (!R) {
        mat_free(Vq);
        mat_free(V);
        free(eval_buf);
        free(evals);
        free(mapped);
        return NULL;
    }

    mat_attach_spectral_cache(R, Vq, mapped);

    free(eval_buf);
    free(evals);

    matrix_t *out = mat_convert_with_store(R, orig_elem, R->store);
    if (!out) {
        free(mapped);
        mat_free(Vq);
        mat_free(V);
        mat_free(R);
        return NULL;
    }

    mat_attach_spectral_cache(out, Vq, mapped);
    mat_free(Vq);
    mat_free(V);
    free(mapped);
    mat_free(R);
    return out;
}

static int mat_is_upper_triangular_local(const matrix_t *A)
{
    qfloat_t tol = qf_from_double(1e-30);
    qcomplex_t z;

    if (!A || A->rows != A->cols)
        return 0;

    for (size_t i = 1; i < A->rows; ++i) {
        for (size_t j = 0; j < i; ++j) {
            unsigned char raw[64];
            mat_get(A, i, j, raw);
            A->elem->to_qc(&z, raw);
            if (qf_lt(tol, qc_abs(z)))
                return 0;
        }
    }

    return 1;
}

static matrix_t *mat_fun_upper_triangular(const matrix_t *A,
                                          void (*scalar_f)(void *out, const void *in))
{
    const struct elem_vtable *orig_elem = A->elem;
    matrix_t *T = mat_to_qcomplex_local(A);
    matrix_t *FT = NULL;
    matrix_t *out = NULL;

    if (!T)
        return NULL;

    FT = mat_fun_triangular(T, scalar_f);
    if (!FT) {
        mat_free(T);
        return NULL;
    }

    out = mat_convert_with_store(FT, orig_elem, FT->store);
    if (!out) {
        mat_free(T);
        mat_free(FT);
        return NULL;
    }

    mat_free(T);
    mat_free(FT);
    return out;
}

/* ============================================================
   Schur-based matrix function engine
   ============================================================ */

matrix_t *mat_fun_schur(const matrix_t *A,
                        void (*scalar_f)(void *out, const void *in))
{
    if (!A || !scalar_f)
        return NULL;

    if (A->rows != A->cols)
        return NULL;

    if (A->rows == 1) {
        const struct elem_vtable *orig_elem = A->elem;
        matrix_t *out = mat_create_diagonal_with_elem(1, orig_elem);
        qcomplex_t in_qc, out_qc;
        unsigned char raw_in[64], raw_out[64];

        if (!out)
            return NULL;

        mat_get(A, 0, 0, raw_in);
        orig_elem->to_qc(&in_qc, raw_in);
        scalar_f(&out_qc, &in_qc);
        orig_elem->from_qc(raw_out, &out_qc);
        mat_set(out, 0, 0, raw_out);
        return out;
    }

    {
        mat_fun_cache_entry_t *cache = mat_fun_cache_find(A);
        if (cache && cache->spectral_Vq && cache->spectral_evals)
            return mat_fun_from_spectral_cache(A, scalar_f);
    }

    if (mat_is_hermitian(A))
        return mat_fun_hermitian(A, scalar_f);

    if (mat_is_upper_triangular_local(A))
        return mat_fun_upper_triangular(A, scalar_f);

    const struct elem_vtable *orig_elem = A->elem;
    mat_schur_factor_t S;
    int schur_rc = mat_schur_factor(A, &S);
    if (schur_rc != 0) {
        fprintf(stderr, "[mat_fun_schur] mat_schur_factor returned %d for %zu×%zu matrix\n",
                schur_rc, A->rows, A->cols);
        return NULL;
    }

    /* S.T is always qcomplex; scalar_f must already be the qcomplex
     * version of the desired function (callers are responsible for this). */
    matrix_t *FT = mat_fun_triangular(S.T, scalar_f);
    if (!FT) {
        mat_schur_factor_free(&S);
        return NULL;
    }

    /* Q f(T) Q* */
    matrix_t *QFT = mat_mul(S.Q, FT);
    if (!QFT) {
        mat_free(FT);
        mat_schur_factor_free(&S);
        return NULL;
    }

    matrix_t *QH = mat_hermitian(S.Q);
    if (!QH) {
        mat_free(FT);
        mat_free(QFT);
        mat_schur_factor_free(&S);
        return NULL;
    }

    matrix_t *R = mat_mul(QFT, QH);   /* qcomplex result */

    mat_free(FT);
    mat_free(QFT);
    mat_free(QH);
    mat_schur_factor_free(&S);

    if (!R) return NULL;

    matrix_t *out = mat_convert_with_store(R, orig_elem, R->store);
    if (!out) { mat_free(R); return NULL; }

    mat_free(R);
    return out;
}

matrix_t *mat_exp(const matrix_t *A)
{
    if (A) {
        mat_fun_cache_entry_t *cache = mat_fun_cache_find(A);
        if (cache && cache->exp_preimage)
            return mat_copy_preserving_store(cache->exp_preimage);
    }
    return mat_fun_schur(A, qcomplex_elem.fun->exp);
}

matrix_t *mat_log(const matrix_t *A)
{
    matrix_t *R = mat_fun_schur(A, qcomplex_elem.fun->log);
    if (R)
        mat_set_exp_preimage_cache(R, A);
    return R;
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

matrix_t *mat_erfinv(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->erfinv);
}

matrix_t *mat_erfcinv(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->erfcinv);
}

matrix_t *mat_gamma(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->gamma);
}

matrix_t *mat_lgamma(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->lgamma);
}

matrix_t *mat_digamma(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->digamma);
}

matrix_t *mat_trigamma(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->trigamma);
}

matrix_t *mat_tetragamma(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->tetragamma);
}

matrix_t *mat_gammainv(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->gammainv);
}

matrix_t *mat_normal_pdf(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->normal_pdf);
}

matrix_t *mat_normal_cdf(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->normal_cdf);
}

matrix_t *mat_normal_logpdf(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->normal_logpdf);
}

matrix_t *mat_lambert_w0(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->lambert_w0);
}

matrix_t *mat_lambert_wm1(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->lambert_wm1);
}

matrix_t *mat_productlog(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->productlog);
}

matrix_t *mat_ei(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->ei);
}

matrix_t *mat_e1(const matrix_t *A)
{
    return mat_fun_schur(A, qcomplex_elem.fun->e1);
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
        base = mat_copy_preserving_store(A);
        if (!base) return NULL;
        p = (unsigned int)n;
    }

    matrix_t *result = mat_create_identity_with_elem(sz, e);
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
