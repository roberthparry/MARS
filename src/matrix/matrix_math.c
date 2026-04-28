#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "matrix_internal.h"
#include "dval_pattern.h"

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

static int mat_elem_supports_numeric_algorithms(const matrix_t *A)
{
    return A && A->elem && A->elem->kind != ELEM_DVAL;
}

static int dval_fun_coeffs_up_to_second(dval_t **c0,
                                        dval_t **c1,
                                        dval_t **c2,
                                        void (*scalar_f)(void *out, const void *in),
                                        dval_t *lambda)
{
    dval_t *u;
    dval_t *f_u = NULL;
    dval_t *df_u = NULL;
    dval_t *d2f_u = NULL;
    dval_t *tmp = NULL;

    if (!c0 || !scalar_f || !lambda)
        return -1;

    *c0 = NULL;
    if (c1)
        *c1 = NULL;
    if (c2)
        *c2 = NULL;

    u = dv_new_named_var_d(dv_eval_d(lambda), "u");
    if (!u)
        return -1;

    scalar_f(&f_u, &u);
    if (!f_u) {
        dv_free(u);
        return -1;
    }

    *c0 = dv_substitute(f_u, u, lambda);
    if (!*c0) {
        dv_free(f_u);
        dv_free(u);
        return -1;
    }

    if (c1) {
        df_u = dv_create_deriv(f_u, u);
        *c1 = dv_substitute(df_u, u, lambda);
        if (!*c1) {
            dv_free(df_u);
            dv_free(f_u);
            dv_free(u);
            dv_free(*c0);
            *c0 = NULL;
            return -1;
        }
    }

    if (c2) {
        d2f_u = dv_create_2nd_deriv(f_u, u, u);
        tmp = dv_substitute(d2f_u, u, lambda);
        if (!tmp) {
            dv_free(d2f_u);
            dv_free(df_u);
            dv_free(f_u);
            dv_free(u);
            dv_free(*c0);
            if (c1) {
                dv_free(*c1);
                *c1 = NULL;
            }
            *c0 = NULL;
            return -1;
        }
        *c2 = dv_mul_d(tmp, 0.5);
        dv_free(tmp);
        if (!*c2) {
            dv_free(d2f_u);
            dv_free(df_u);
            dv_free(f_u);
            dv_free(u);
            dv_free(*c0);
            if (c1) {
                dv_free(*c1);
                *c1 = NULL;
            }
            *c0 = NULL;
            return -1;
        }
    }

    dv_free(d2f_u);
    dv_free(df_u);
    dv_free(f_u);
    dv_free(u);
    return 0;
}

static matrix_t *mat_fun_triangular_equal_diag_dval(const matrix_t *T,
                                                    void (*scalar_f)(void *out, const void *in))
{
    size_t n = T->rows;
    matrix_t *F = mat_create_upper_triangular_with_elem(n, n, &dval_elem);
    matrix_t *N = mat_create_upper_triangular_with_elem(n, n, &dval_elem);
    dval_t *lambda = NULL;
    dval_t *c0 = NULL;
    dval_t *c1 = NULL;
    dval_t *c2 = NULL;

    if (!F || !N) {
        mat_free(F);
        mat_free(N);
        return NULL;
    }

    mat_get(T, 0, 0, &lambda);
    if (dval_fun_coeffs_up_to_second(&c0, &c1, &c2, scalar_f, lambda) != 0) {
        mat_free(F);
        mat_free(N);
        return NULL;
    }

    for (size_t i = 0; i < n; ++i) {
        mat_set(F, i, i, &c0);
        for (size_t j = i; j < n; ++j) {
            dval_t *tij = NULL;
            dval_t *zero = DV_ZERO;
            mat_get(T, i, j, &tij);
            if (i == j)
                mat_set(N, i, j, &zero);
            else
                mat_set(N, i, j, &tij);
        }
    }

    if (n >= 2) {
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                dval_t *nij = NULL;
                dval_t *term = NULL;
                mat_get(N, i, j, &nij);
                term = dv_mul(c1, nij);
                if (!term) {
                    dv_free(c0);
                    dv_free(c1);
                    dv_free(c2);
                    mat_free(F);
                    mat_free(N);
                    return NULL;
                }
                mat_set(F, i, j, &term);
                dv_free(term);
            }
        }
    }

    if (n >= 3) {
        matrix_t *N2 = mat_mul(N, N);
        if (!N2) {
            dv_free(c0);
            dv_free(c1);
            dv_free(c2);
            mat_free(F);
            mat_free(N);
            return NULL;
        }

        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 2; j < n; ++j) {
                dval_t *fij = NULL;
                dval_t *n2ij = NULL;
                dval_t *extra = NULL;
                dval_t *sum = NULL;
                mat_get(F, i, j, &fij);
                mat_get(N2, i, j, &n2ij);
                extra = dv_mul(c2, n2ij);
                sum = extra ? dv_add(fij, extra) : NULL;
                dv_free(extra);
                if (!sum) {
                    mat_free(N2);
                    dv_free(c0);
                    dv_free(c1);
                    dv_free(c2);
                    mat_free(F);
                    mat_free(N);
                    return NULL;
                }
                mat_set(F, i, j, &sum);
                dv_free(sum);
            }
        }

        mat_free(N2);
    }

    dv_free(c0);
    dv_free(c1);
    dv_free(c2);
    mat_free(N);
    return F;
}

static matrix_t *mat_fun_dval_structured(const matrix_t *A,
                                         void (*scalar_f)(void *out, const void *in))
{
    size_t n;
    matrix_t *T;
    matrix_t *FT;
    matrix_t *out;
    dval_t *diag0 = NULL;
    qfloat_t tol = qf_from_double(1e-24);

    if (!A || !scalar_f || !A->elem || A->elem->kind != ELEM_DVAL)
        return NULL;
    if (A->rows != A->cols)
        return NULL;
    if (mat_is_upper_triangular(A)) {
        if (A->rows == 0)
            return mat_copy_preserving_store(A);

        n = A->rows;
        mat_get(A, 0, 0, &diag0);
        for (size_t i = 1; i < n; ++i) {
            dval_t *diag_i = NULL;
            qfloat_t diff;
            mat_get(A, i, i, &diag_i);
            diff = qc_abs(qc_sub(dv_get_val(diag_i), dv_get_val(diag0)));
            if (qf_lt(tol, diff))
                return mat_fun_triangular(A, scalar_f);
        }
        return mat_fun_triangular_equal_diag_dval(A, scalar_f);
    }

    if (!mat_is_lower_triangular(A))
        return NULL;

    T = mat_transpose(A);
    if (!T)
        return NULL;
    FT = mat_fun_dval_structured(T, scalar_f);
    out = FT ? mat_transpose(FT) : NULL;
    mat_free(T);
    mat_free(FT);
    return out;
}

static matrix_t *mat_fun_apply(const matrix_t *A,
                               void (*qcomplex_scalar_f)(void *out, const void *in),
                               void (*dval_scalar_f)(void *out, const void *in))
{
    if (A && A->elem && A->elem->kind == ELEM_DVAL)
        return dval_scalar_f ? mat_fun_dval_structured(A, dval_scalar_f) : NULL;
    return mat_fun_schur(A, qcomplex_scalar_f);
}

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
    unsigned char r_raw[64] = {0}, v[64] = {0};
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
    if (!mat_elem_supports_numeric_algorithms(A))
        return NULL;

    if (A->rows != A->cols)
        return NULL;

    if (A->rows == 1) {
        const struct elem_vtable *orig_elem = A->elem;
        matrix_t *out = mat_create_diagonal_with_elem(1, orig_elem);
        qcomplex_t in_qc, out_qc;
        unsigned char raw_in[64] = {0}, raw_out[64] = {0};

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
    return mat_fun_apply(A, qcomplex_elem.fun->exp, dval_elem.fun ? dval_elem.fun->exp : NULL);
}

matrix_t *mat_log(const matrix_t *A)
{
    matrix_t *R = mat_fun_apply(A, qcomplex_elem.fun->log, dval_elem.fun ? dval_elem.fun->log : NULL);
    if (R)
        mat_set_exp_preimage_cache(R, A);
    return R;
}

matrix_t *mat_sin(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->sin, dval_elem.fun ? dval_elem.fun->sin : NULL);
}

matrix_t *mat_cos(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->cos, dval_elem.fun ? dval_elem.fun->cos : NULL);
}

matrix_t *mat_tan(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->tan, dval_elem.fun ? dval_elem.fun->tan : NULL);
}

matrix_t *mat_sinh(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->sinh, dval_elem.fun ? dval_elem.fun->sinh : NULL);
}

matrix_t *mat_cosh(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->cosh, dval_elem.fun ? dval_elem.fun->cosh : NULL);
}

matrix_t *mat_tanh(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->tanh, dval_elem.fun ? dval_elem.fun->tanh : NULL);
}

matrix_t *mat_sqrt(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->sqrt, dval_elem.fun ? dval_elem.fun->sqrt : NULL);
}

matrix_t *mat_asin(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->asin, dval_elem.fun ? dval_elem.fun->asin : NULL);
}

matrix_t *mat_acos(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->acos, dval_elem.fun ? dval_elem.fun->acos : NULL);
}

matrix_t *mat_atan(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->atan, dval_elem.fun ? dval_elem.fun->atan : NULL);
}

matrix_t *mat_asinh(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->asinh, dval_elem.fun ? dval_elem.fun->asinh : NULL);
}

matrix_t *mat_acosh(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->acosh, dval_elem.fun ? dval_elem.fun->acosh : NULL);
}

matrix_t *mat_atanh(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->atanh, dval_elem.fun ? dval_elem.fun->atanh : NULL);
}

matrix_t *mat_erf(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->erf, dval_elem.fun ? dval_elem.fun->erf : NULL);
}

matrix_t *mat_erfc(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->erfc, dval_elem.fun ? dval_elem.fun->erfc : NULL);
}

matrix_t *mat_erfinv(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->erfinv, dval_elem.fun ? dval_elem.fun->erfinv : NULL);
}

matrix_t *mat_erfcinv(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->erfcinv, dval_elem.fun ? dval_elem.fun->erfcinv : NULL);
}

matrix_t *mat_gamma(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->gamma, dval_elem.fun ? dval_elem.fun->gamma : NULL);
}

matrix_t *mat_lgamma(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->lgamma, dval_elem.fun ? dval_elem.fun->lgamma : NULL);
}

matrix_t *mat_digamma(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->digamma, dval_elem.fun ? dval_elem.fun->digamma : NULL);
}

matrix_t *mat_trigamma(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->trigamma, dval_elem.fun ? dval_elem.fun->trigamma : NULL);
}

matrix_t *mat_tetragamma(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->tetragamma, dval_elem.fun ? dval_elem.fun->tetragamma : NULL);
}

matrix_t *mat_gammainv(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->gammainv, dval_elem.fun ? dval_elem.fun->gammainv : NULL);
}

matrix_t *mat_normal_pdf(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->normal_pdf, dval_elem.fun ? dval_elem.fun->normal_pdf : NULL);
}

matrix_t *mat_normal_cdf(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->normal_cdf, dval_elem.fun ? dval_elem.fun->normal_cdf : NULL);
}

matrix_t *mat_normal_logpdf(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->normal_logpdf, dval_elem.fun ? dval_elem.fun->normal_logpdf : NULL);
}

matrix_t *mat_lambert_w0(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->lambert_w0, dval_elem.fun ? dval_elem.fun->lambert_w0 : NULL);
}

matrix_t *mat_lambert_wm1(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->lambert_wm1, dval_elem.fun ? dval_elem.fun->lambert_wm1 : NULL);
}

matrix_t *mat_productlog(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->productlog, dval_elem.fun ? dval_elem.fun->productlog : NULL);
}

matrix_t *mat_ei(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->ei, dval_elem.fun ? dval_elem.fun->ei : NULL);
}

matrix_t *mat_e1(const matrix_t *A)
{
    return mat_fun_apply(A, qcomplex_elem.fun->e1, dval_elem.fun ? dval_elem.fun->e1 : NULL);
}

/* ============================================================
   mat_pow_int  —  binary exponentiation
   Negative exponents invert A first.
   ============================================================ */

matrix_t *mat_pow_int(const matrix_t *A, int n)
{
    if (!A || A->rows != A->cols) return NULL;
    if (!mat_elem_supports_numeric_algorithms(A) && n < 0) return NULL;
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
    if (A->elem && A->elem->kind == ELEM_DVAL)
        return NULL;
    if (!mat_elem_supports_numeric_algorithms(A)) return NULL;

    matrix_t *L = mat_log(A);
    if (!L) return NULL;

    mat_scale_qf(L, qf_from_double(s));

    matrix_t *result = mat_exp(L);
    mat_free(L);
    return result;
}
