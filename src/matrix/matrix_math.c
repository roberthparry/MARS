#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "matrix_internal.h"
#include "dval_helpers.h"
#include "internal/dval_refcount.h"

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

static matrix_t *mat_fun_apply(const matrix_t *A,
                               void (*qcomplex_f)(void *out, const void *a),
                               void (*dval_f)(void *out, const void *a));
static matrix_t *mat_fun_dval_structured(const matrix_t *A,
                                         void (*scalar_f)(void *out, const void *in));
static matrix_t *mat_apply_unary_via_qc_eval(const matrix_t *A,
                                             matrix_t *(*fun)(const matrix_t *));
static matrix_t *mat_fun_dval_uniform_diag_offdiag(const matrix_t *A,
                                                   void (*scalar_f)(void *out, const void *in));
static matrix_t *mat_fun_dval_scalar_plus_rank_one(const matrix_t *A,
                                                   void (*scalar_f)(void *out, const void *in));
static matrix_t *mat_fun_dval_quartic_biquadratic_exact(const matrix_t *A,
                                                        void (*scalar_f)(void *out, const void *in));
static int dval_fun_coeffs_up_to_second(dval_t **c0,
                                        dval_t **c1,
                                        dval_t **c2,
                                        void (*scalar_f)(void *out, const void *in),
                                        dval_t *lambda);

static matrix_t *mat_apply_unary(const matrix_t *A,
                                 void (*qcomplex_f)(void *out, const void *a),
                                 void (*dval_f)(void *out, const void *a))
{
    return mat_fun_apply(A, qcomplex_f, dval_elem.fun ? dval_f : NULL);
}

static matrix_t *mat_apply_unary_via_qc_eval(const matrix_t *A,
                                             matrix_t *(*fun)(const matrix_t *))
{
    matrix_t *evaluated = NULL;
    matrix_t *result = NULL;

    if (!A || !fun)
        return NULL;

    evaluated = mat_evaluate_qc(A);
    if (!evaluated)
        return NULL;

    result = fun(evaluated);
    mat_free(evaluated);
    return result;
}

static int mat_elem_supports_numeric_algorithms(const matrix_t *A)
{
    return A && A->elem && A->elem->kind != ELEM_DVAL;
}

static dval_t *dval_simplify_owned_local(dval_t *dv)
{
    dval_t *simp;

    if (!dv)
        return NULL;

    simp = dv_simplify(dv);
    dv_free(dv);
    return simp;
}

static dval_t *dval_div_simplify_local(dval_t *num, dval_t *den)
{
    dval_t *raw;

    if (!num || !den) {
        dv_free(num);
        dv_free(den);
        return NULL;
    }

    raw = dv_div(num, den);
    dv_free(num);
    dv_free(den);
    if (!raw)
        return NULL;

    return dval_simplify_owned_local(raw);
}

static dval_t *dval_mul_simplify_local(dval_t *a, dval_t *b)
{
    dval_t *raw;

    if (!a || !b) {
        dv_free(a);
        dv_free(b);
        return NULL;
    }

    raw = dv_mul(a, b);
    dv_free(a);
    dv_free(b);
    if (!raw)
        return NULL;

    return dval_simplify_owned_local(raw);
}

static dval_t *dval_add_simplify_local(dval_t *a, dval_t *b)
{
    dval_t *raw;

    if (!a || !b) {
        dv_free(a);
        dv_free(b);
        return NULL;
    }

    raw = dv_add(a, b);
    dv_free(a);
    dv_free(b);
    if (!raw)
        return NULL;

    return dval_simplify_owned_local(raw);
}

static dval_t *dval_sub_simplify_local(dval_t *a, dval_t *b)
{
    dval_t *raw;

    if (!a || !b) {
        dv_free(a);
        dv_free(b);
        return NULL;
    }

    raw = dv_sub(a, b);
    dv_free(a);
    dv_free(b);
    if (!raw)
        return NULL;

    return dval_simplify_owned_local(raw);
}

static bool dval_is_zero_local(const dval_t *dv)
{
    return !dv || dv_is_exact_zero(dv);
}

static dval_t *dval_fun_first_derivative_at_zero_local(
    void (*scalar_f)(void *out, const void *in))
{
    dval_t *zero = NULL;
    dval_t *c0 = NULL;
    dval_t *c1 = NULL;

    if (!scalar_f)
        return NULL;

    zero = dv_new_const_d(0.0);
    if (!zero)
        return NULL;

    if (dval_fun_coeffs_up_to_second(&c0, &c1, NULL, scalar_f, zero) != 0) {
        dv_free(zero);
        return NULL;
    }

    dv_free(zero);
    dv_free(c0);
    return c1;
}

static bool dval_equal_exact_local(const dval_t *a, const dval_t *b)
{
    dval_t *diff;
    bool equal;

    if (a == b)
        return true;
    if (!a)
        return dval_is_zero_local(b);
    if (!b)
        return dval_is_zero_local(a);

    dv_retain((dval_t *)a);
    dv_retain((dval_t *)b);
    diff = dval_sub_simplify_local((dval_t *)a, (dval_t *)b);
    if (!diff)
        return false;

    equal = dv_is_exact_zero(diff);
    dv_free(diff);
    return equal;
}

static dval_t *dval_mul_or_zero_owned_local(const dval_t *a, const dval_t *b)
{
    if (dval_is_zero_local(a) || dval_is_zero_local(b))
        return dv_new_const_d(0.0);

    dv_retain((dval_t *)a);
    dv_retain((dval_t *)b);
    return dval_mul_simplify_local((dval_t *)a, (dval_t *)b);
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

static matrix_t *mat_fun_dval_diagonalizable_2x2(const matrix_t *A,
                                                 void (*scalar_f)(void *out, const void *in))
{
    dval_t *eigenvalues[2] = {NULL, NULL};
    dval_t *mapped[2] = {NULL, NULL};
    matrix_t *V = NULL;
    matrix_t *FD = NULL;
    matrix_t *VF = NULL;
    matrix_t *Vinv = NULL;
    matrix_t *R = NULL;

    if (!A || !scalar_f || A->rows != 2 || A->cols != 2 ||
        !A->elem || A->elem->kind != ELEM_DVAL)
        return NULL;

    if (mat_eigendecompose(A, eigenvalues, &V) != 0 || !V)
        goto fail;

    FD = mat_create_diagonal_with_elem(2, &dval_elem);
    if (!FD)
        goto fail;

    for (size_t i = 0; i < 2; ++i) {
        scalar_f(&mapped[i], &eigenvalues[i]);
        if (!mapped[i])
            goto fail;
        mat_set(FD, i, i, &mapped[i]);
    }

    VF = mat_mul(V, FD);
    if (!VF)
        goto fail;

    Vinv = mat_inverse(V);
    if (!Vinv)
        goto fail;

    R = mat_mul(VF, Vinv);

fail:
    for (size_t i = 0; i < 2; ++i) {
        dv_free(eigenvalues[i]);
        dv_free(mapped[i]);
    }
    mat_free(V);
    mat_free(FD);
    mat_free(VF);
    mat_free(Vinv);
    return R;
}

static bool mat_dval_block_is_exact_zero(const matrix_t *A,
                                         size_t row0, size_t rows,
                                         size_t col0, size_t cols)
{
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            dval_t *entry = NULL;

            mat_get(A, row0 + i, col0 + j, &entry);
            if (!dval_is_zero_local(entry))
                return false;
        }
    }

    return true;
}

static matrix_t *mat_dval_permute_principal_local(const matrix_t *A,
                                                  const size_t *perm)
{
    matrix_t *P;

    if (!A || !perm || A->rows != A->cols)
        return NULL;

    P = mat_create_dense_with_elem(A->rows, A->cols, &dval_elem);
    if (!P)
        return NULL;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            dval_t *entry = NULL;

            mat_get(A, perm[i], perm[j], &entry);
            mat_set(P, i, j, &entry);
        }
    }

    return P;
}

static matrix_t *mat_dval_extract_block_local(const matrix_t *A,
                                              size_t row0, size_t rows,
                                              size_t col0, size_t cols)
{
    matrix_t *B;

    if (!A || row0 + rows > A->rows || col0 + cols > A->cols)
        return NULL;

    B = mat_create_dense_with_elem(rows, cols, &dval_elem);
    if (!B)
        return NULL;

    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            dval_t *entry = NULL;

            mat_get(A, row0 + i, col0 + j, &entry);
            mat_set(B, i, j, &entry);
        }
    }

    return B;
}

static bool mat_dval_insert_block_local(matrix_t *A, size_t row0, size_t col0,
                                        const matrix_t *B)
{
    if (!A || !B || row0 + B->rows > A->rows || col0 + B->cols > A->cols)
        return false;

    for (size_t i = 0; i < B->rows; ++i) {
        for (size_t j = 0; j < B->cols; ++j) {
            dval_t *entry = NULL;

            mat_get(B, i, j, &entry);
            mat_set(A, row0 + i, col0 + j, &entry);
        }
    }

    return true;
}

static int mat_dval_component_partition(const matrix_t *A,
                                        size_t **perm_out,
                                        size_t *count_out)
{
    size_t n;
    size_t *perm = NULL;
    bool *seen = NULL;
    size_t perm_len = 0;
    size_t comp_count = 0;

    if (!A || !perm_out || !count_out || A->rows != A->cols)
        return -1;

    *perm_out = NULL;
    *count_out = 0;

    n = A->rows;
    if (n == 0)
        return 0;

    perm = malloc(n * sizeof(*perm));
    seen = calloc(n, sizeof(*seen));
    if (!perm || !seen)
        goto fail;

    for (size_t root = 0; root < n; ++root) {
        size_t *queue = NULL;
        size_t qh = 0;
        size_t qt = 0;

        if (seen[root])
            continue;

        queue = malloc(n * sizeof(*queue));
        if (!queue)
            goto fail;

        queue[qt++] = root;
        seen[root] = true;

        while (qh < qt) {
            size_t u = queue[qh++];

            perm[perm_len++] = u;
            for (size_t v = 0; v < n; ++v) {
                dval_t *uv = NULL;
                dval_t *vu = NULL;
                bool connected;

                if (seen[v])
                    continue;

                mat_get(A, u, v, &uv);
                mat_get(A, v, u, &vu);
                connected = !dval_is_zero_local(uv) || !dval_is_zero_local(vu);
                if (!connected)
                    continue;

                seen[v] = true;
                queue[qt++] = v;
            }
        }

        free(queue);
        comp_count++;
    }

    *perm_out = perm;
    *count_out = comp_count;
    free(seen);
    return 0;

fail:
    free(perm);
    free(seen);
    return -1;
}

static matrix_t *mat_fun_dval_block_diagonal(const matrix_t *A,
                                             void (*scalar_f)(void *out, const void *in))
{
    size_t n;

    if (!A || !scalar_f || !A->elem || A->elem->kind != ELEM_DVAL)
        return NULL;
    if (A->rows != A->cols || A->rows < 2)
        return NULL;

    n = A->rows;
    for (size_t split = 1; split < n; ++split) {
        matrix_t *A11 = NULL;
        matrix_t *A22 = NULL;
        matrix_t *F11 = NULL;
        matrix_t *F22 = NULL;
        matrix_t *F = NULL;

        if (!mat_dval_block_is_exact_zero(A, 0, split, split, n - split) ||
            !mat_dval_block_is_exact_zero(A, split, n - split, 0, split))
            continue;

        A11 = mat_dval_extract_block_local(A, 0, split, 0, split);
        A22 = mat_dval_extract_block_local(A, split, n - split, split, n - split);
        if (!A11 || !A22)
            goto next_split;

        F11 = mat_fun_dval_structured(A11, scalar_f);
        F22 = mat_fun_dval_structured(A22, scalar_f);
        if (!F11 || !F22)
            goto next_split;

        F = mat_create_dense_with_elem(n, n, &dval_elem);
        if (!F)
            goto next_split;

        if (!mat_dval_insert_block_local(F, 0, 0, F11) ||
            !mat_dval_insert_block_local(F, split, split, F22)) {
            mat_free(F);
            F = NULL;
            goto next_split;
        }

        mat_free(A11);
        mat_free(A22);
        mat_free(F11);
        mat_free(F22);
        return F;

next_split:
        mat_free(A11);
        mat_free(A22);
        mat_free(F11);
        mat_free(F22);
    }

    return NULL;
}

static matrix_t *mat_fun_dval_permuted_block_diagonal(const matrix_t *A,
                                                      void (*scalar_f)(void *out, const void *in))
{
    size_t *perm = NULL;
    size_t comp_count = 0;
    matrix_t *P = NULL;
    matrix_t *FP = NULL;
    matrix_t *F = NULL;

    if (!A || !scalar_f || !A->elem || A->elem->kind != ELEM_DVAL)
        return NULL;
    if (A->rows != A->cols || A->rows < 2)
        return NULL;

    if (mat_dval_component_partition(A, &perm, &comp_count) != 0)
        goto fail;
    if (comp_count <= 1)
        goto fail;

    P = mat_dval_permute_principal_local(A, perm);
    if (!P)
        goto fail;

    FP = mat_fun_dval_block_diagonal(P, scalar_f);
    if (!FP)
        goto fail;

    F = mat_create_dense_with_elem(A->rows, A->cols, &dval_elem);
    if (!F)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            dval_t *entry = NULL;

            mat_get(FP, i, j, &entry);
            mat_set(F, perm[i], perm[j], &entry);
        }
    }

fail:
    free(perm);
    mat_free(P);
    mat_free(FP);
    return F;
}

static matrix_t *mat_fun_dval_uniform_diag_offdiag(const matrix_t *A,
                                                   void (*scalar_f)(void *out, const void *in))
{
    matrix_t *out = NULL;
    dval_t *diag = NULL;
    dval_t *offdiag = NULL;
    dval_t *alpha = NULL;
    dval_t *beta = NULL;
    dval_t *fa = NULL;
    dval_t *fb = NULL;
    dval_t *delta = NULL;
    size_t n;

    if (!A || !scalar_f || !A->elem || A->elem->kind != ELEM_DVAL)
        return NULL;
    if (A->rows != A->cols || A->rows < 2)
        return NULL;

    n = A->rows;
    mat_get(A, 0, 0, &diag);
    mat_get(A, 0, 1, &offdiag);

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            dval_t *entry = NULL;
            bool same;

            mat_get(A, i, j, &entry);
            same = (i == j)
                ? dval_equal_exact_local(diag, entry)
                : dval_equal_exact_local(offdiag, entry);
            if (!same)
                goto fail;
        }
    }

    dv_retain(diag ? diag : DV_ZERO);
    dv_retain(offdiag ? offdiag : DV_ZERO);
    alpha = dval_sub_simplify_local(diag ? diag : DV_ZERO, offdiag ? offdiag : DV_ZERO);
    if (!alpha)
        goto fail;

    dv_retain(diag ? diag : DV_ZERO);
    dv_retain(offdiag ? offdiag : DV_ZERO);
    dval_t *scaled_offdiag = dval_mul_simplify_local(
        dv_new_const_d((double)(n - 1)),
        offdiag ? offdiag : DV_ZERO);
    if (!scaled_offdiag)
        goto fail;
    beta = dval_add_simplify_local(diag ? diag : DV_ZERO, scaled_offdiag);
    scaled_offdiag = NULL;
    if (!beta)
        goto fail;

    scalar_f(&fa, &alpha);
    scalar_f(&fb, &beta);
    if (!fa || !fb)
        goto fail;

    dv_retain(fb);
    dv_retain(fa);
    delta = dval_sub_simplify_local(fb, fa);
    if (!delta)
        goto fail;
    {
        dval_t *scaled_delta = dv_div_d(delta, (double)n);
        dv_free(delta);
        delta = dval_simplify_owned_local(scaled_delta);
    }
    if (!delta)
        goto fail;

    out = mat_new_dv(n, n);
    if (!out)
        goto fail;

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            dval_t *entry = NULL;

            if (i == j) {
                dv_retain(fa);
                dv_retain(delta);
                entry = dval_add_simplify_local(fa, delta);
            } else {
                dv_retain(delta);
                entry = dval_simplify_owned_local(delta);
            }

            if (!entry)
                goto fail;

            mat_set(out, i, j, &entry);
            dv_free(entry);
        }
    }

    dv_free(alpha);
    dv_free(beta);
    dv_free(fa);
    dv_free(fb);
    dv_free(delta);
    return out;

fail:
    mat_free(out);
    dv_free(alpha);
    dv_free(beta);
    dv_free(fa);
    dv_free(fb);
    dv_free(delta);
    return NULL;
}

static matrix_t *mat_fun_dval_scalar_plus_rank_one(const matrix_t *A,
                                                   void (*scalar_f)(void *out, const void *in))
{
    matrix_t *out = NULL;
    dval_t **u = NULL;
    dval_t **v = NULL;
    dval_t *alpha = NULL;
    dval_t *lambda = NULL;
    dval_t *f_alpha = NULL;
    dval_t *f_beta = NULL;
    dval_t *coeff = NULL;
    size_t n;
    size_t p = 0;
    size_t q = 0;
    bool found_pivot = false;

    if (!A || !scalar_f || !A->elem || A->elem->kind != ELEM_DVAL)
        return NULL;
    if (A->rows != A->cols || A->rows < 3)
        return NULL;
    if (mat_is_upper_triangular(A) || mat_is_lower_triangular(A))
        return NULL;

    n = A->rows;
    u = calloc(n, sizeof(*u));
    v = calloc(n, sizeof(*v));
    if (!u || !v)
        goto fail;

    for (size_t i = 0; i < n && !found_pivot; ++i) {
        for (size_t j = 0; j < n; ++j) {
            dval_t *entry = NULL;

            if (i == j)
                continue;
            mat_get(A, i, j, &entry);
            if (!dval_is_zero_local(entry)) {
                p = i;
                q = j;
                found_pivot = true;
                break;
            }
        }
    }
    if (!found_pivot)
        goto fail;

    for (size_t i = 0; i < n; ++i) {
        dval_t *entry = NULL;

        if (i == q)
            continue;
        mat_get(A, i, q, &entry);
        if (!dval_is_zero_local(entry)) {
            dv_retain(entry);
            u[i] = entry;
        }
    }

    v[q] = dv_new_const_d(1.0);
    if (!v[q])
        goto fail;

    for (size_t j = 0; j < n; ++j) {
        dval_t *apj = NULL;
        dval_t *apq = NULL;

        if (j == p || j == q)
            continue;
        mat_get(A, p, j, &apj);
        if (dval_is_zero_local(apj))
            continue;
        mat_get(A, p, q, &apq);
        if (dval_is_zero_local(apq))
            goto fail;

        dv_retain(apj);
        dv_retain(apq);
        v[j] = dval_div_simplify_local(apj, apq);
        if (!v[j])
            goto fail;
    }

    for (size_t i = 0; i < n; ++i) {
        dval_t *aii = NULL;
        dval_t *diag_corr = NULL;
        dval_t *cand = NULL;
        bool usable = (i != p && i != q);

        if (!usable)
            continue;

        diag_corr = dval_mul_or_zero_owned_local(u[i], v[i]);
        if (!diag_corr)
            goto fail;

        mat_get(A, i, i, &aii);
        dv_retain(aii ? aii : DV_ZERO);
        cand = dval_sub_simplify_local(aii ? aii : DV_ZERO, diag_corr);
        if (!cand)
            goto fail;

        if (!alpha) {
            alpha = cand;
        } else {
            bool same = dval_equal_exact_local(alpha, cand);
            dv_free(cand);
            if (!same)
                goto fail;
        }
    }
    if (!alpha)
        goto fail;

    {
        dval_t *app = NULL;
        dval_t *diag_p = NULL;
        dval_t *aqq = NULL;
        dval_t *diag_q = NULL;
        dval_t *apq = NULL;

        mat_get(A, p, p, &app);
        dv_retain(app ? app : DV_ZERO);
        dv_retain(alpha);
        diag_p = dval_sub_simplify_local(app ? app : DV_ZERO, alpha);
        if (!diag_p)
            goto fail;

        mat_get(A, p, q, &apq);
        if (dval_is_zero_local(apq))
            goto fail;
        dv_retain(diag_p);
        dv_retain(apq);
        v[p] = dval_div_simplify_local(diag_p, apq);
        dv_free(diag_p);
        if (!v[p])
            goto fail;

        mat_get(A, q, q, &aqq);
        dv_retain(aqq ? aqq : DV_ZERO);
        dv_retain(alpha);
        diag_q = dval_sub_simplify_local(aqq ? aqq : DV_ZERO, alpha);
        if (!diag_q)
            goto fail;
        u[q] = diag_q;
    }

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            dval_t *aij = NULL;
            dval_t *expected = NULL;

            mat_get(A, i, j, &aij);
            if (i == j) {
                dval_t *diag_corr = dval_mul_or_zero_owned_local(u[i], v[i]);
                if (!diag_corr)
                    goto fail;
                dv_retain(alpha);
                expected = dval_add_simplify_local(alpha, diag_corr);
            } else {
                expected = dval_mul_or_zero_owned_local(u[i], v[j]);
            }

            if (!expected)
                goto fail;
            if (!dval_equal_exact_local(aij, expected)) {
                dv_free(expected);
                goto fail;
            }
            dv_free(expected);
        }
    }

    lambda = dv_new_const_d(0.0);
    if (!lambda)
        goto fail;
    for (size_t i = 0; i < n; ++i) {
        dval_t *term = dval_mul_or_zero_owned_local(u[i], v[i]);
        dval_t *next;

        if (!term)
            goto fail;
        next = dval_add_simplify_local(lambda, term);
        if (!next)
            goto fail;
        lambda = next;
    }

    if (dv_is_exact_zero(lambda)) {
        if (dval_fun_coeffs_up_to_second(&f_alpha, &coeff, NULL, scalar_f, alpha) != 0)
            goto fail;
    } else {
        dval_t *beta = NULL;
        dval_t *num = NULL;

        dv_retain(alpha);
        dv_retain(lambda);
        beta = dval_add_simplify_local(alpha, lambda);
        if (!beta)
            goto fail;

        scalar_f(&f_alpha, &alpha);
        scalar_f(&f_beta, &beta);
        dv_free(beta);
        if (!f_alpha || !f_beta)
            goto fail;

        dv_retain(f_beta);
        dv_retain(f_alpha);
        num = dval_sub_simplify_local(f_beta, f_alpha);
        if (!num)
            goto fail;
        dv_retain(lambda);
        coeff = dval_div_simplify_local(num, lambda);
        if (!coeff)
            goto fail;
    }

    out = mat_new_dv(n, n);
    if (!out)
        goto fail;

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            dval_t *aij = NULL;
            dval_t *entry = NULL;

            if (i == j) {
                dval_t *diag_delta = NULL;

                mat_get(A, i, i, &aij);
                dv_retain(aij ? aij : DV_ZERO);
                dv_retain(alpha);
                diag_delta = dval_sub_simplify_local(aij ? aij : DV_ZERO, alpha);
                if (!diag_delta)
                    goto fail;

                if (dval_is_zero_local(diag_delta)) {
                    dv_free(diag_delta);
                    dv_retain(f_alpha);
                    entry = dval_simplify_owned_local(f_alpha);
                } else {
                    dval_t *scaled = NULL;

                    dv_retain(coeff);
                    dv_retain(diag_delta);
                    scaled = dval_mul_simplify_local(coeff, diag_delta);
                    dv_free(diag_delta);
                    if (!scaled)
                        goto fail;
                    dv_retain(f_alpha);
                    entry = dval_add_simplify_local(f_alpha, scaled);
                }
            } else {
                mat_get(A, i, j, &aij);
                if (dval_is_zero_local(aij)) {
                    entry = dv_new_const_d(0.0);
                } else {
                    dv_retain(coeff);
                    dv_retain(aij);
                    entry = dval_mul_simplify_local(coeff, aij);
                }
            }

            if (!entry)
                goto fail;
            mat_set(out, i, j, &entry);
            dv_free(entry);
        }
    }

    for (size_t i = 0; i < n; ++i) {
        dv_free(u[i]);
        dv_free(v[i]);
    }
    free(u);
    free(v);
    dv_free(alpha);
    dv_free(lambda);
    dv_free(f_alpha);
    dv_free(f_beta);
    dv_free(coeff);
    return out;

fail:
    mat_free(out);
    if (u) {
        for (size_t i = 0; i < n; ++i)
            dv_free(u[i]);
    }
    if (v) {
        for (size_t i = 0; i < n; ++i)
            dv_free(v[i]);
    }
    free(u);
    free(v);
    dv_free(alpha);
    dv_free(lambda);
    dv_free(f_alpha);
    dv_free(f_beta);
    dv_free(coeff);
    return NULL;
}

static matrix_t *mat_fun_dval_cubic_linear_exact(const matrix_t *A,
                                                 void (*scalar_f)(void *out, const void *in))
{
    matrix_t *A2 = NULL;
    matrix_t *A3 = NULL;
    matrix_t *out = NULL;
    dval_t *s = NULL;
    dval_t *root = NULL;
    dval_t *f0 = NULL;
    dval_t *fp = NULL;
    dval_t *fm = NULL;
    dval_t *c0 = NULL;
    dval_t *c1 = NULL;
    dval_t *c2 = NULL;
    bool saw_nonzero = false;

    if (!A || !scalar_f || !A->elem || A->elem->kind != ELEM_DVAL)
        return NULL;
    if (A->rows != A->cols || A->rows == 0)
        return NULL;
    if (mat_is_upper_triangular(A) || mat_is_lower_triangular(A))
        return NULL;

    A2 = mat_mul(A, A);
    A3 = A2 ? mat_mul(A2, A) : NULL;
    if (!A2 || !A3)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            dval_t *aij = NULL;
            dval_t *a3ij = NULL;

            mat_get(A, i, j, &aij);
            mat_get(A3, i, j, &a3ij);
            if (dval_is_zero_local(aij)) {
                if (!dval_is_zero_local(a3ij))
                    goto fail;
                continue;
            }

            saw_nonzero = true;
            dv_retain(a3ij);
            dv_retain(aij);
            dval_t *cand = dval_div_simplify_local(a3ij, aij);
            if (!cand)
                goto fail;

            if (!s) {
                s = cand;
            } else {
                bool same = dval_equal_exact_local(s, cand);
                dv_free(cand);
                if (!same)
                    goto fail;
            }
        }
    }

    if (!saw_nonzero || !s)
        goto fail;

    if (dv_is_exact_zero(s)) {
        dval_t *zero = dv_new_const_d(0.0);

        if (!zero)
            goto fail;
        if (dval_fun_coeffs_up_to_second(&c0, &c1, &c2, scalar_f, zero) != 0) {
            dv_free(zero);
            goto fail;
        }
        dv_free(zero);
    } else {
        dval_t *zero = dv_new_const_d(0.0);
        dval_t *neg_root = NULL;
        dval_t *two_root = NULL;
        dval_t *two_s = NULL;
        dval_t *sum = NULL;
        dval_t *diff = NULL;
        dval_t *tmp = NULL;

        if (!zero)
            goto fail;

        root = dv_sqrt(s);
        root = dval_simplify_owned_local(root);
        if (!root) {
            dv_free(zero);
            goto fail;
        }

        dv_retain(root);
        neg_root = dval_sub_simplify_local(dv_new_const_d(0.0), root);
        if (!neg_root) {
            dv_free(zero);
            goto fail;
        }

        scalar_f(&f0, &zero);
        scalar_f(&fp, &root);
        scalar_f(&fm, &neg_root);
        dv_free(zero);
        if (!f0 || !fp || !fm)
            goto fail;

        c0 = dval_simplify_owned_local(f0);
        f0 = NULL;
        if (!c0)
            goto fail;

        dv_retain(fp);
        dv_retain(fm);
        diff = dval_sub_simplify_local(fp, fm);
        if (!diff)
            goto fail;

        dv_retain(root);
        two_root = dval_mul_simplify_local(dv_new_const_d(2.0), root);
        if (!two_root)
            goto fail;
        c1 = dval_div_simplify_local(diff, two_root);
        diff = NULL;
        two_root = NULL;
        if (!c1)
            goto fail;

        dv_retain(fp);
        dv_retain(fm);
        sum = dval_add_simplify_local(fp, fm);
        if (!sum)
            goto fail;

        dv_retain(c0);
        tmp = dval_mul_simplify_local(dv_new_const_d(2.0), c0);
        if (!tmp)
            goto fail;
        sum = dval_sub_simplify_local(sum, tmp);
        tmp = NULL;
        if (!sum)
            goto fail;

        dv_retain(s);
        two_s = dval_mul_simplify_local(dv_new_const_d(2.0), s);
        if (!two_s)
            goto fail;
        c2 = dval_div_simplify_local(sum, two_s);
        sum = NULL;
        two_s = NULL;
        if (!c2)
            goto fail;

        dv_free(neg_root);
    }

    out = mat_new_dv(A->rows, A->cols);
    if (!out)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            dval_t *aij = NULL;
            dval_t *a2ij = NULL;
            dval_t *term1 = NULL;
            dval_t *term2 = NULL;
            dval_t *entry = NULL;

            mat_get(A, i, j, &aij);
            mat_get(A2, i, j, &a2ij);

            dv_retain(c1);
            dv_retain(aij ? aij : DV_ZERO);
            term1 = dval_mul_simplify_local(c1, aij ? aij : DV_ZERO);
            if (!term1)
                goto fail;

            dv_retain(c2);
            dv_retain(a2ij ? a2ij : DV_ZERO);
            term2 = dval_mul_simplify_local(c2, a2ij ? a2ij : DV_ZERO);
            if (!term2)
                goto fail;

            entry = dval_add_simplify_local(term1, term2);
            term1 = NULL;
            term2 = NULL;
            if (!entry)
                goto fail;

            if (i == j) {
                dv_retain(c0);
                entry = dval_add_simplify_local(c0, entry);
                if (!entry)
                    goto fail;
            }

            mat_set(out, i, j, &entry);
            dv_free(entry);
        }
    }

    mat_free(A2);
    mat_free(A3);
    dv_free(s);
    dv_free(root);
    dv_free(f0);
    dv_free(fp);
    dv_free(fm);
    dv_free(c0);
    dv_free(c1);
    dv_free(c2);
    return out;

fail:
    mat_free(A2);
    mat_free(A3);
    mat_free(out);
    dv_free(s);
    dv_free(root);
    dv_free(f0);
    dv_free(fp);
    dv_free(fm);
    dv_free(c0);
    dv_free(c1);
    dv_free(c2);
    return NULL;
}

static matrix_t *mat_fun_dval_quartic_biquadratic_exact(const matrix_t *A,
                                                        void (*scalar_f)(void *out, const void *in))
{
    matrix_t *A2 = NULL;
    matrix_t *A3 = NULL;
    matrix_t *A4 = NULL;
    matrix_t *out = NULL;
    dval_t *s = NULL;
    dval_t *t = NULL;
    dval_t *disc = NULL;
    dval_t *root = NULL;
    dval_t *mu1 = NULL;
    dval_t *mu2 = NULL;
    dval_t *r1 = NULL;
    dval_t *r2 = NULL;
    dval_t *nr1 = NULL;
    dval_t *nr2 = NULL;
    dval_t *fp1 = NULL;
    dval_t *fm1 = NULL;
    dval_t *fp2 = NULL;
    dval_t *fm2 = NULL;
    dval_t *g1 = NULL;
    dval_t *g2 = NULL;
    dval_t *h1 = NULL;
    dval_t *h2 = NULL;
    dval_t *e0 = NULL;
    dval_t *e1 = NULL;
    dval_t *o0 = NULL;
    dval_t *o1 = NULL;
    dval_t *step = NULL;
    dval_t *step_sq = NULL;

    if (!A || !scalar_f || !A->elem || A->elem->kind != ELEM_DVAL)
        return NULL;
    if (A->rows != A->cols || A->rows != 4)
        return NULL;
    if (mat_is_upper_triangular(A) || mat_is_lower_triangular(A))
        return NULL;

    A2 = mat_mul(A, A);
    A3 = A2 ? mat_mul(A2, A) : NULL;
    A4 = A2 ? mat_mul(A2, A2) : NULL;
    if (!A2 || !A3 || !A4)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            dval_t *aij = NULL;
            bool should_match_step = (i + 1 == j) || (j + 1 == i);

            mat_get(A, i, j, &aij);
            if (i == j) {
                if (!dval_is_zero_local(aij))
                    goto fail;
                continue;
            }

            if (should_match_step) {
                if (dval_is_zero_local(aij))
                    goto fail;
                if (!step) {
                    dv_retain(aij);
                    step = aij;
                } else if (!dval_equal_exact_local(step, aij)) {
                    goto fail;
                }
            } else if (!dval_is_zero_local(aij)) {
                goto fail;
            }
        }
    }

    if (!step)
        goto fail;

    dv_retain(step);
    dv_retain(step);
    step_sq = dval_mul_simplify_local(step, step);
    if (!step_sq)
        goto fail;
    dv_retain(step_sq);
    s = dval_mul_simplify_local(dv_new_const_d(3.0), step_sq);
    if (!s)
        goto fail;

    dv_retain(step_sq);
    dv_retain(step_sq);
    dval_t *step_four = dval_mul_simplify_local(step_sq, step_sq);
    if (!step_four)
        goto fail;
    t = dval_sub_simplify_local(dv_new_const_d(0.0), step_four);
    if (!t)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            dval_t *a2ij = NULL;
            dval_t *a4ij = NULL;
            dval_t *expected = NULL;
            dval_t *scaled = NULL;

            mat_get(A2, i, j, &a2ij);
            mat_get(A4, i, j, &a4ij);
            dv_retain(s);
            dv_retain(a2ij ? a2ij : DV_ZERO);
            scaled = dval_mul_simplify_local(s, a2ij ? a2ij : DV_ZERO);
            if (!scaled)
                goto fail;

            if (i == j) {
                dv_retain(t);
                expected = dval_add_simplify_local(scaled, t);
            } else {
                expected = scaled;
            }

            if (!expected)
                goto fail;
            if (!dval_equal_exact_local(a4ij, expected)) {
                dv_free(expected);
                goto fail;
            }
            dv_free(expected);
        }
    }

    dv_retain(s);
    dv_retain(s);
    disc = dval_mul_simplify_local(s, s);
    if (!disc)
        goto fail;

    dv_retain(t);
    dval_t *four_t = dval_mul_simplify_local(dv_new_const_d(4.0), t);
    if (!four_t)
        goto fail;
    disc = dval_add_simplify_local(disc, four_t);
    if (!disc || dv_is_exact_zero(disc))
        goto fail;

    root = dv_sqrt(disc);
    root = dval_simplify_owned_local(root);
    if (!root)
        goto fail;

    dv_retain(s);
    dv_retain(root);
    dval_t *sum = dval_add_simplify_local(s, root);
    if (!sum)
        goto fail;
    mu1 = dv_div_d(sum, 2.0);
    dv_free(sum);
    mu1 = dval_simplify_owned_local(mu1);
    if (!mu1)
        goto fail;

    dv_retain(s);
    dv_retain(root);
    dval_t *diff = dval_sub_simplify_local(s, root);
    if (!diff)
        goto fail;
    mu2 = dv_div_d(diff, 2.0);
    dv_free(diff);
    mu2 = dval_simplify_owned_local(mu2);
    if (!mu2 || dval_equal_exact_local(mu1, mu2))
        goto fail;

    r1 = dv_sqrt(mu1);
    r1 = dval_simplify_owned_local(r1);
    r2 = dv_sqrt(mu2);
    r2 = dval_simplify_owned_local(r2);
    if (!r1 || !r2)
        goto fail;

    dv_retain(r1);
    nr1 = dval_sub_simplify_local(dv_new_const_d(0.0), r1);
    dv_retain(r2);
    nr2 = dval_sub_simplify_local(dv_new_const_d(0.0), r2);
    if (!nr1 || !nr2)
        goto fail;

    scalar_f(&fp1, &r1);
    scalar_f(&fm1, &nr1);
    scalar_f(&fp2, &r2);
    scalar_f(&fm2, &nr2);
    if (!fp1 || !fm1 || !fp2 || !fm2)
        goto fail;

    dv_retain(fp1);
    dv_retain(fm1);
    g1 = dval_add_simplify_local(fp1, fm1);
    if (g1) {
        dval_t *half = dv_div_d(g1, 2.0);
        dv_free(g1);
        g1 = half ? dval_simplify_owned_local(half) : NULL;
    }
    if (!g1)
        goto fail;

    dv_retain(fp2);
    dv_retain(fm2);
    g2 = dval_add_simplify_local(fp2, fm2);
    if (g2) {
        dval_t *half = dv_div_d(g2, 2.0);
        dv_free(g2);
        g2 = half ? dval_simplify_owned_local(half) : NULL;
    }
    if (!g2)
        goto fail;

    if (dv_is_exact_zero(mu1)) {
        h1 = dval_fun_first_derivative_at_zero_local(scalar_f);
    } else {
        dv_retain(fp1);
        dv_retain(fm1);
        dval_t *num = dval_sub_simplify_local(fp1, fm1);
        dval_t *den = NULL;

        if (!num)
            goto fail;
        dv_retain(r1);
        den = dval_mul_simplify_local(dv_new_const_d(2.0), r1);
        if (!den)
            goto fail;
        h1 = dval_div_simplify_local(num, den);
    }
    if (!h1)
        goto fail;

    if (dv_is_exact_zero(mu2)) {
        h2 = dval_fun_first_derivative_at_zero_local(scalar_f);
    } else {
        dv_retain(fp2);
        dv_retain(fm2);
        dval_t *num = dval_sub_simplify_local(fp2, fm2);
        dval_t *den = NULL;

        if (!num)
            goto fail;
        dv_retain(r2);
        den = dval_mul_simplify_local(dv_new_const_d(2.0), r2);
        if (!den)
            goto fail;
        h2 = dval_div_simplify_local(num, den);
    }
    if (!h2)
        goto fail;

    {
        dval_t *num = NULL;
        dval_t *den = NULL;

        dv_retain(g1);
        dv_retain(g2);
        num = dval_sub_simplify_local(g1, g2);
        dv_retain(mu1);
        dv_retain(mu2);
        den = dval_sub_simplify_local(mu1, mu2);
        e1 = dval_div_simplify_local(num, den);
        if (!e1)
            goto fail;

        dv_retain(e1);
        dv_retain(mu1);
        dval_t *e1mu = dval_mul_simplify_local(e1, mu1);
        if (!e1mu)
            goto fail;
        dv_retain(g1);
        e0 = dval_sub_simplify_local(g1, e1mu);
        if (!e0)
            goto fail;
    }

    {
        dval_t *num = NULL;
        dval_t *den = NULL;

        dv_retain(h1);
        dv_retain(h2);
        num = dval_sub_simplify_local(h1, h2);
        dv_retain(mu1);
        dv_retain(mu2);
        den = dval_sub_simplify_local(mu1, mu2);
        o1 = dval_div_simplify_local(num, den);
        if (!o1)
            goto fail;

        dv_retain(o1);
        dv_retain(mu1);
        dval_t *o1mu = dval_mul_simplify_local(o1, mu1);
        if (!o1mu)
            goto fail;
        dv_retain(h1);
        o0 = dval_sub_simplify_local(h1, o1mu);
        if (!o0)
            goto fail;
    }

    out = mat_new_dv(A->rows, A->cols);
    if (!out)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            dval_t *aij = NULL;
            dval_t *a2ij = NULL;
            dval_t *a3ij = NULL;
            dval_t *entry = NULL;
            dval_t *term = NULL;

            if (i == j) {
                dv_retain(e0);
                entry = dval_simplify_owned_local(e0);
                if (!entry)
                    goto fail;
            } else {
                entry = dv_new_const_d(0.0);
                if (!entry)
                    goto fail;
            }

            mat_get(A, i, j, &aij);
            dv_retain(o0);
            dv_retain(aij ? aij : DV_ZERO);
            term = dval_mul_simplify_local(o0, aij ? aij : DV_ZERO);
            if (!term) {
                dv_free(entry);
                goto fail;
            }
            entry = dval_add_simplify_local(entry, term);
            if (!entry)
                goto fail;

            mat_get(A2, i, j, &a2ij);
            dv_retain(e1);
            dv_retain(a2ij ? a2ij : DV_ZERO);
            term = dval_mul_simplify_local(e1, a2ij ? a2ij : DV_ZERO);
            if (!term) {
                dv_free(entry);
                goto fail;
            }
            entry = dval_add_simplify_local(entry, term);
            if (!entry)
                goto fail;

            mat_get(A3, i, j, &a3ij);
            dv_retain(o1);
            dv_retain(a3ij ? a3ij : DV_ZERO);
            term = dval_mul_simplify_local(o1, a3ij ? a3ij : DV_ZERO);
            if (!term) {
                dv_free(entry);
                goto fail;
            }
            entry = dval_add_simplify_local(entry, term);
            if (!entry)
                goto fail;

            mat_set(out, i, j, &entry);
            dv_free(entry);
        }
    }

    mat_free(A2);
    mat_free(A3);
    mat_free(A4);
    dv_free(s);
    dv_free(t);
    dv_free(step);
    dv_free(step_sq);
    dv_free(disc);
    dv_free(root);
    dv_free(mu1);
    dv_free(mu2);
    dv_free(r1);
    dv_free(r2);
    dv_free(nr1);
    dv_free(nr2);
    dv_free(fp1);
    dv_free(fm1);
    dv_free(fp2);
    dv_free(fm2);
    dv_free(g1);
    dv_free(g2);
    dv_free(h1);
    dv_free(h2);
    dv_free(e0);
    dv_free(e1);
    dv_free(o0);
    dv_free(o1);
    return out;

fail:
    mat_free(A2);
    mat_free(A3);
    mat_free(A4);
    mat_free(out);
    dv_free(s);
    dv_free(t);
    dv_free(step);
    dv_free(step_sq);
    dv_free(disc);
    dv_free(root);
    dv_free(mu1);
    dv_free(mu2);
    dv_free(r1);
    dv_free(r2);
    dv_free(nr1);
    dv_free(nr2);
    dv_free(fp1);
    dv_free(fm1);
    dv_free(fp2);
    dv_free(fm2);
    dv_free(g1);
    dv_free(g2);
    dv_free(h1);
    dv_free(h2);
    dv_free(e0);
    dv_free(e1);
    dv_free(o0);
    dv_free(o1);
    return NULL;
}

static matrix_t *mat_fun_dval_quadratic_exact(const matrix_t *A,
                                              void (*scalar_f)(void *out, const void *in))
{
    matrix_t *A2 = NULL;
    matrix_t *out = NULL;
    dval_t *p = NULL;
    dval_t *q = NULL;
    dval_t *disc = NULL;
    dval_t *root = NULL;
    dval_t *lambda1 = NULL;
    dval_t *lambda2 = NULL;
    dval_t *f1 = NULL;
    dval_t *f2 = NULL;
    dval_t *c0 = NULL;
    dval_t *c1 = NULL;
    bool saw_offdiag = false;

    if (!A || !scalar_f || !A->elem || A->elem->kind != ELEM_DVAL)
        return NULL;
    if (A->rows != A->cols || A->rows == 0)
        return NULL;
    if (mat_is_upper_triangular(A) || mat_is_lower_triangular(A))
        return NULL;

    A2 = mat_mul(A, A);
    if (!A2)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            dval_t *aij = NULL;
            dval_t *a2ij = NULL;

            if (i == j)
                continue;

            mat_get(A, i, j, &aij);
            mat_get(A2, i, j, &a2ij);
            if (dval_is_zero_local(aij)) {
                if (!dval_is_zero_local(a2ij))
                    goto fail;
                continue;
            }

            saw_offdiag = true;
            dv_retain(a2ij);
            dv_retain(aij);
            dval_t *cand = dval_div_simplify_local(a2ij, aij);
            if (!cand)
                goto fail;

            if (!p) {
                p = cand;
            } else {
                bool same = dval_equal_exact_local(p, cand);
                dv_free(cand);
                if (!same)
                    goto fail;
            }
        }
    }

    if (!saw_offdiag || !p)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        dval_t *aii = NULL;
        dval_t *a2ii = NULL;
        dval_t *p_aii = NULL;
        dval_t *cand = NULL;

        mat_get(A, i, i, &aii);
        mat_get(A2, i, i, &a2ii);
        dv_retain(p);
        dv_retain(aii ? aii : DV_ZERO);
        p_aii = dval_mul_simplify_local(p, aii ? aii : DV_ZERO);
        if (!p_aii)
            goto fail;

        dv_retain(a2ii ? a2ii : DV_ZERO);
        cand = dval_sub_simplify_local(a2ii ? a2ii : DV_ZERO, p_aii);
        if (!cand)
            goto fail;

        if (!q) {
            q = cand;
        } else {
            bool same = dval_equal_exact_local(q, cand);
            dv_free(cand);
            if (!same)
                goto fail;
        }
    }

    if (!q)
        goto fail;

    dv_retain(p);
    dv_retain(p);
    disc = dval_mul_simplify_local(p, p);
    if (!disc)
        goto fail;

    dv_retain(q);
    dval_t *four_q = dval_mul_simplify_local(dv_new_const_d(4.0), q);
    if (!four_q)
        goto fail;
    disc = dval_add_simplify_local(disc, four_q);
    if (!disc)
        goto fail;

    if (dv_is_exact_zero(disc)) {
        dval_t *lambda = NULL;

        dv_retain(p);
        lambda = dv_div_d(p, 2.0);
        lambda = dval_simplify_owned_local(lambda);
        if (!lambda)
            goto fail;

        if (dval_fun_coeffs_up_to_second(&f1, &c1, NULL, scalar_f, lambda) != 0)
            goto fail;

        dv_retain(c1);
        dv_retain(lambda);
        dval_t *lambda_df = dval_mul_simplify_local(c1, lambda);
        if (!lambda_df)
            goto fail;

        dv_retain(f1);
        c0 = dval_sub_simplify_local(f1, lambda_df);
        dv_free(lambda);
        if (!c0 || !c1)
            goto fail;
    } else {
        dval_t *sum = NULL;
        dval_t *diff = NULL;
        dval_t *num = NULL;
        dval_t *den = NULL;

        root = dv_sqrt(disc);
        root = dval_simplify_owned_local(root);
        if (!root)
            goto fail;

        dv_retain(p);
        dv_retain(root);
        sum = dval_add_simplify_local(p, root);
        if (!sum)
            goto fail;

        dv_retain(p);
        dv_retain(root);
        diff = dval_sub_simplify_local(p, root);
        if (!diff)
            goto fail;

        lambda1 = dv_div_d(sum, 2.0);
        lambda1 = dval_simplify_owned_local(lambda1);
        lambda2 = dv_div_d(diff, 2.0);
        lambda2 = dval_simplify_owned_local(lambda2);
        dv_free(sum);
        dv_free(diff);
        if (!lambda1 || !lambda2)
            goto fail;

        scalar_f(&f1, &lambda1);
        scalar_f(&f2, &lambda2);
        if (!f1 || !f2)
            goto fail;

        dv_retain(f1);
        dv_retain(f2);
        num = dval_sub_simplify_local(f1, f2);
        dv_retain(lambda1);
        dv_retain(lambda2);
        den = dval_sub_simplify_local(lambda1, lambda2);
        c1 = dval_div_simplify_local(num, den);
        if (!c1)
            goto fail;

        dv_retain(c1);
        dv_retain(lambda1);
        dval_t *c1_l1 = dval_mul_simplify_local(c1, lambda1);
        if (!c1_l1)
            goto fail;

        dv_retain(f1);
        c0 = dval_sub_simplify_local(f1, c1_l1);
        if (!c0)
            goto fail;
    }

    out = mat_new_dv(A->rows, A->cols);
    if (!out)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            dval_t *aij = NULL;
            dval_t *term = NULL;
            dval_t *entry = NULL;

            mat_get(A, i, j, &aij);
            dv_retain(c1);
            dv_retain(aij ? aij : DV_ZERO);
            term = dval_mul_simplify_local(c1, aij ? aij : DV_ZERO);
            if (!term)
                goto fail;

            if (i == j) {
                dv_retain(c0);
                entry = dval_add_simplify_local(c0, term);
            } else {
                entry = term;
            }

            if (!entry)
                goto fail;

            mat_set(out, i, j, &entry);
            dv_free(entry);
        }
    }

    mat_free(A2);
    dv_free(p);
    dv_free(q);
    dv_free(disc);
    dv_free(root);
    dv_free(lambda1);
    dv_free(lambda2);
    dv_free(f1);
    dv_free(f2);
    dv_free(c0);
    dv_free(c1);
    return out;

fail:
    mat_free(A2);
    if (out)
        mat_free(out);
    dv_free(p);
    dv_free(q);
    dv_free(disc);
    dv_free(root);
    dv_free(lambda1);
    dv_free(lambda2);
    dv_free(f1);
    dv_free(f2);
    dv_free(c0);
    dv_free(c1);
    return NULL;
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

    if (!mat_is_lower_triangular(A)) {
        matrix_t *block_diag = mat_fun_dval_block_diagonal(A, scalar_f);
        if (block_diag)
            return block_diag;
        matrix_t *perm_block_diag = mat_fun_dval_permuted_block_diagonal(A, scalar_f);
        if (perm_block_diag)
            return perm_block_diag;
        matrix_t *uniform = mat_fun_dval_uniform_diag_offdiag(A, scalar_f);
        if (uniform)
            return uniform;
        matrix_t *rank_one = mat_fun_dval_scalar_plus_rank_one(A, scalar_f);
        if (rank_one)
            return rank_one;
        matrix_t *cubic_linear = mat_fun_dval_cubic_linear_exact(A, scalar_f);
        if (cubic_linear)
            return cubic_linear;
        matrix_t *quartic_biquadratic = mat_fun_dval_quartic_biquadratic_exact(A, scalar_f);
        if (quartic_biquadratic)
            return quartic_biquadratic;
        matrix_t *quadratic = mat_fun_dval_quadratic_exact(A, scalar_f);
        if (quadratic)
            return quadratic;
        if (A->rows == 2 && A->cols == 2)
            return mat_fun_dval_diagonalizable_2x2(A, scalar_f);
        return NULL;
    }

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
    return mat_apply_unary(A, qcomplex_elem.fun->exp, dval_elem.fun->exp);
}

matrix_t *mat_log(const matrix_t *A)
{
    matrix_t *R = mat_apply_unary(A, qcomplex_elem.fun->log, dval_elem.fun->log);
    if (R)
        mat_set_exp_preimage_cache(R, A);
    return R;
}

matrix_t *mat_sin(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->sin, dval_elem.fun->sin);
}

matrix_t *mat_cos(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->cos, dval_elem.fun->cos);
}

matrix_t *mat_tan(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->tan, dval_elem.fun->tan);
}

matrix_t *mat_sinh(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->sinh, dval_elem.fun->sinh);
}

matrix_t *mat_cosh(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->cosh, dval_elem.fun->cosh);
}

matrix_t *mat_tanh(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->tanh, dval_elem.fun->tanh);
}

matrix_t *mat_sqrt(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->sqrt, dval_elem.fun->sqrt);
}

matrix_t *mat_asin(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->asin, dval_elem.fun->asin);
}

matrix_t *mat_acos(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->acos, dval_elem.fun->acos);
}

matrix_t *mat_atan(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->atan, dval_elem.fun->atan);
}

matrix_t *mat_asinh(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->asinh, dval_elem.fun->asinh);
}

matrix_t *mat_acosh(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->acosh, dval_elem.fun->acosh);
}

matrix_t *mat_atanh(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->atanh, dval_elem.fun->atanh);
}

matrix_t *mat_erf(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->erf, dval_elem.fun->erf);
}

matrix_t *mat_erfc(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->erfc, dval_elem.fun->erfc);
}

matrix_t *mat_erfinv(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->erfinv, dval_elem.fun->erfinv);
}

matrix_t *mat_erfcinv(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->erfcinv, dval_elem.fun->erfcinv);
}

matrix_t *mat_gamma(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->gamma, dval_elem.fun->gamma);
}

matrix_t *mat_lgamma(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->lgamma, dval_elem.fun->lgamma);
}

matrix_t *mat_digamma(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->digamma, dval_elem.fun->digamma);
}

matrix_t *mat_trigamma(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->trigamma, dval_elem.fun->trigamma);
}

matrix_t *mat_tetragamma(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->tetragamma, dval_elem.fun->tetragamma);
}

matrix_t *mat_gammainv(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->gammainv, dval_elem.fun->gammainv);
}

matrix_t *mat_normal_pdf(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->normal_pdf, dval_elem.fun->normal_pdf);
}

matrix_t *mat_normal_cdf(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->normal_cdf, dval_elem.fun->normal_cdf);
}

matrix_t *mat_normal_logpdf(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->normal_logpdf, dval_elem.fun->normal_logpdf);
}

matrix_t *mat_lambert_w0(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->lambert_w0, dval_elem.fun->lambert_w0);
}

matrix_t *mat_lambert_wm1(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->lambert_wm1, dval_elem.fun->lambert_wm1);
}

matrix_t *mat_productlog(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->productlog, dval_elem.fun->productlog);
}

matrix_t *mat_ei(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->ei, dval_elem.fun->ei);
}

matrix_t *mat_e1(const matrix_t *A)
{
    return mat_apply_unary(A, qcomplex_elem.fun->e1, dval_elem.fun->e1);
}

matrix_t *mat_exp_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_exp);
}

matrix_t *mat_sin_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_sin);
}

matrix_t *mat_cos_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_cos);
}

matrix_t *mat_tan_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_tan);
}

matrix_t *mat_sinh_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_sinh);
}

matrix_t *mat_cosh_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_cosh);
}

matrix_t *mat_tanh_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_tanh);
}

matrix_t *mat_sqrt_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_sqrt);
}

matrix_t *mat_log_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_log);
}

matrix_t *mat_asin_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_asin);
}

matrix_t *mat_acos_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_acos);
}

matrix_t *mat_atan_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_atan);
}

matrix_t *mat_asinh_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_asinh);
}

matrix_t *mat_acosh_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_acosh);
}

matrix_t *mat_atanh_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_atanh);
}

matrix_t *mat_erf_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_erf);
}

matrix_t *mat_erfc_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_erfc);
}

matrix_t *mat_erfinv_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_erfinv);
}

matrix_t *mat_erfcinv_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_erfcinv);
}

matrix_t *mat_gamma_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_gamma);
}

matrix_t *mat_lgamma_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_lgamma);
}

matrix_t *mat_digamma_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_digamma);
}

matrix_t *mat_trigamma_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_trigamma);
}

matrix_t *mat_tetragamma_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_tetragamma);
}

matrix_t *mat_gammainv_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_gammainv);
}

matrix_t *mat_normal_pdf_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_normal_pdf);
}

matrix_t *mat_normal_cdf_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_normal_cdf);
}

matrix_t *mat_normal_logpdf_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_normal_logpdf);
}

matrix_t *mat_lambert_w0_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_lambert_w0);
}

matrix_t *mat_lambert_wm1_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_lambert_wm1);
}

matrix_t *mat_productlog_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_productlog);
}

matrix_t *mat_ei_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_ei);
}

matrix_t *mat_e1_eval_qc(const matrix_t *A)
{
    return mat_apply_unary_via_qc_eval(A, mat_e1);
}

/* ============================================================
   mat_pow_int  —  binary exponentiation
   Negative exponents invert A first.
   ============================================================ */

matrix_t *mat_pow_int(const matrix_t *A, int n)
{
    matrix_t *simplified = NULL;

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

    if (result->elem != &dval_elem)
        return result;

    simplified = mat_simplify_symbolic(result);
    mat_free(result);
    return simplified;
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
