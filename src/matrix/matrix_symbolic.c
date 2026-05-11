#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "matrix_internal.h"
#include "internal/dval_internal.h"

static bool dval_is_exact_zero(const dval_t *dv)
{
    return !dv || dv_is_exact_zero(dv);
}

typedef struct {
    matrix_t *R;
    size_t *pivot_cols;
    bool *is_pivot;
    size_t rank;
} dval_rref_info_t;

dval_t *dval_simplify_owned(dval_t *dv)
{
    dval_t *simp;

    if (!dv)
        return NULL;

    simp = dv_simplify(dv);
    dv_free(dv);
    return simp;
}

static dval_t *dval_div_simplify(const dval_t *num, const dval_t *den)
{
    dval_t *raw;

    if (!num || !den) {
        dv_free((dval_t *)num);
        dv_free((dval_t *)den);
        return NULL;
    }

    raw = dv_div(num, den);
    dv_free((dval_t *)num);
    dv_free((dval_t *)den);
    if (!raw)
        return NULL;

    return dval_simplify_owned(raw);
}

static dval_t *dval_neg_simplify(dval_t *dv)
{
    dval_t *raw;

    if (!dv)
        return NULL;

    raw = dv_neg(dv);
    dv_free(dv);
    if (!raw)
        return NULL;

    return dval_simplify_owned(raw);
}

int mat_simplify_symbolic_inplace(matrix_t *A)
{
    dval_t *dv = NULL;
    dval_t *simp = NULL;

    if (!A)
        return -1;
    if (A->elem != &dval_elem)
        return 0;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            dv = NULL;
            simp = NULL;
            mat_get(A, i, j, &dv);
            if (!dv)
                continue;
            simp = dv_simplify(dv);
            if (!simp)
                return -1;
            mat_set(A, i, j, &simp);
            dv_free(simp);
        }
    }

    return 0;
}

static dval_t *dval_mul_simplify(const dval_t *a, const dval_t *b)
{
    dval_t *raw;

    if (!a || !b) {
        dv_free((dval_t *)a);
        dv_free((dval_t *)b);
        return NULL;
    }

    raw = dv_mul(a, b);
    dv_free((dval_t *)a);
    dv_free((dval_t *)b);
    if (!raw)
        return NULL;

    return dval_simplify_owned(raw);
}

static dval_t *dval_add_simplify(const dval_t *a, const dval_t *b)
{
    dval_t *raw;

    if (!a || !b) {
        dv_free((dval_t *)a);
        dv_free((dval_t *)b);
        return NULL;
    }

    raw = dv_add(a, b);
    dv_free((dval_t *)a);
    dv_free((dval_t *)b);
    if (!raw)
        return NULL;

    return dval_simplify_owned(raw);
}

dval_t *dval_sub_simplify(const dval_t *a, const dval_t *b)
{
    dval_t *raw;

    if (!a || !b) {
        dv_free((dval_t *)a);
        dv_free((dval_t *)b);
        return NULL;
    }

    raw = dv_sub(a, b);
    dv_free((dval_t *)a);
    dv_free((dval_t *)b);
    if (!raw)
        return NULL;

    return dval_simplify_owned(raw);
}

static dval_t *dval_det2_simplify(dval_t *a, dval_t *b, dval_t *c, dval_t *d)
{
    dval_t *left = NULL, *right = NULL;

    if (a)
        dv_retain(a);
    if (d)
        dv_retain(d);
    left = dval_mul_simplify(a, d);

    if (b)
        dv_retain(b);
    if (c)
        dv_retain(c);
    right = dval_mul_simplify(b, c);

    return dval_sub_simplify(left, right);
}

int mat_det_dval_exact(const matrix_t *A, dval_t **determinant);

static const dval_t *mat_get_dval_or_zero(const matrix_t *A, size_t i, size_t j)
{
    dval_t *v = NULL;

    mat_get(A, i, j, &v);
    return v ? v : DV_ZERO;
}

static bool dval_exprs_equal_exact(const dval_t *a, const dval_t *b)
{
    dval_t *diff;
    bool equal;

    if (a == b)
        return true;
    if (!a || !b)
        return false;

    dv_retain((dval_t *)a);
    dv_retain((dval_t *)b);
    diff = dval_sub_simplify((dval_t *)a, (dval_t *)b);
    if (!diff)
        return false;

    equal = dv_is_exact_zero(diff);
    dv_free(diff);
    return equal;
}

static int mat_eigenvalues_dval(const matrix_t *A, dval_t **eigenvalues)
{
    if (!A || !eigenvalues)
        return -1;
    if (A->rows != A->cols)
        return -2;

    if (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) {
        for (size_t i = 0; i < A->rows; ++i) {
            dval_t *diag = NULL;

            mat_get(A, i, i, &diag);
            eigenvalues[i] = dval_clone_for_storage(diag ? diag : DV_ZERO);
            if (!eigenvalues[i])
                goto fail;
        }
        return 0;
    }

    if (A->rows == 2) {
        const dval_t *a = mat_get_dval_or_zero(A, 0, 0);
        const dval_t *b = mat_get_dval_or_zero(A, 0, 1);
        const dval_t *c = mat_get_dval_or_zero(A, 1, 0);
        const dval_t *d = mat_get_dval_or_zero(A, 1, 1);
        dval_t *sum = NULL, *diff = NULL, *diff2 = NULL;
        dval_t *bc = NULL, *scaled_bc = NULL, *disc = NULL, *root = NULL;
        dval_t *plus = NULL, *minus = NULL, *half = NULL;

        if (a)
            dv_retain(a);
        if (d)
            dv_retain(d);
        sum = dval_add_simplify(a, d);
        a = d = NULL;
        if (!sum)
            goto fail_2x2;

        a = mat_get_dval_or_zero(A, 0, 0);
        d = mat_get_dval_or_zero(A, 1, 1);
        if (a)
            dv_retain(a);
        if (d)
            dv_retain(d);
        diff = dval_sub_simplify(a, d);
        a = d = NULL;
        if (!diff)
            goto fail_2x2;

        dv_retain(diff);
        diff2 = dval_mul_simplify(diff, diff);
        diff = NULL;
        if (!diff2)
            goto fail_2x2;

        if (b)
            dv_retain(b);
        if (c)
            dv_retain(c);
        bc = dval_mul_simplify(b, c);
        if (!bc)
            goto fail_2x2;

        scaled_bc = dval_mul_simplify(dv_num_const_d(4.0), bc);
        bc = NULL;
        if (!scaled_bc)
            goto fail_2x2;

        disc = dval_add_simplify(diff2, scaled_bc);
        diff2 = NULL;
        scaled_bc = NULL;
        if (!disc)
            goto fail_2x2;

        {
            dval_t *raw_root = dv_sqrt(disc);

            dv_free(disc);
            disc = NULL;
            root = dval_simplify_owned(raw_root);
        }
        if (!root)
            goto fail_2x2;

        dv_retain(sum);
        dv_retain(root);
        plus = dval_add_simplify(sum, root);
        if (!plus)
            goto fail_2x2;

        minus = dval_sub_simplify(sum, root);
        sum = NULL;
        root = NULL;
        if (!minus)
            goto fail_2x2;

        half = dv_num_const_d(0.5);
        if (!half)
            goto fail_2x2;

        dv_retain(half);
        eigenvalues[0] = dval_mul_simplify(half, plus);
        plus = NULL;
        if (!eigenvalues[0])
            goto fail_2x2;

        dv_retain(half);
        eigenvalues[1] = dval_mul_simplify(half, minus);
        minus = NULL;
        dv_free(half);
        half = NULL;
        if (!eigenvalues[1])
            goto fail_2x2;

        return 0;

fail_2x2:
        dv_free((dval_t *)a);
        dv_free((dval_t *)b);
        dv_free((dval_t *)c);
        dv_free((dval_t *)d);
        dv_free(sum);
        dv_free(diff);
        dv_free(diff2);
        dv_free(bc);
        dv_free(scaled_bc);
        dv_free(disc);
        dv_free(root);
        dv_free(plus);
        dv_free(minus);
        dv_free(half);
        for (size_t i = 0; i < A->rows; ++i) {
            dv_free(eigenvalues[i]);
            eigenvalues[i] = NULL;
        }
        return -3;
    }

    return -3;

fail:
    for (size_t i = 0; i < A->rows; ++i) {
        dv_free(eigenvalues[i]);
        eigenvalues[i] = NULL;
    }
    return -3;
}

static matrix_t *mat_eigenvectors_dval_triangular(const matrix_t *A)
{
    matrix_t *V;
    bool upper;

    if (!A || A->rows != A->cols)
        return NULL;

    if (mat_is_diagonal(A))
        return mat_create_identity_dv(A->rows);

    upper = mat_is_upper_triangular(A);
    if (!upper && !mat_is_lower_triangular(A))
        return NULL;

    V = mat_create_identity_dv(A->rows);
    if (!V)
        return NULL;

    if (upper) {
        for (size_t k = 0; k < A->rows; ++k) {
            const dval_t *lambda = mat_get_dval_or_zero(A, k, k);

            for (size_t ii = k; ii-- > 0;) {
                dval_t *sum = dv_num_const_qf(QF_ZERO);
                dval_t *denom;
                dval_t *x;

                if (!sum)
                    goto fail;

                for (size_t j = ii + 1; j <= k; ++j) {
                    const dval_t *aij = mat_get_dval_or_zero(A, ii, j);
                    const dval_t *xjk = mat_get_dval_or_zero(V, j, k);
                    dval_t *term;

                    dv_retain(aij);
                    dv_retain(xjk);
                    term = dval_mul_simplify(aij, xjk);
                    sum = dval_add_simplify(sum, term);
                    if (!sum)
                        goto fail;
                }

                {
                    const dval_t *diag = mat_get_dval_or_zero(A, ii, ii);

                    dv_retain(diag);
                    dv_retain(lambda);
                    denom = dval_sub_simplify(diag, lambda);
                }
                if (!denom) {
                    dv_free((dval_t *)sum);
                    goto fail;
                }

                if (dv_is_exact_zero(denom)) {
                    dv_free(denom);
                    if (dv_is_exact_zero(sum)) {
                        dv_free(sum);
                        continue;
                    }
                    dv_free((dval_t *)sum);
                    goto fail;
                }

                x = dval_neg_simplify(sum);
                denom = dval_simplify_owned(denom);
                if (!x) {
                    dv_free(denom);
                    goto fail;
                }

                x = dval_div_simplify(x, denom);
                if (!x)
                    goto fail;

                mat_set(V, ii, k, &x);
                dv_free(x);
            }
        }
    } else {
        for (size_t k = 0; k < A->rows; ++k) {
            const dval_t *lambda = mat_get_dval_or_zero(A, k, k);

            for (size_t i = k + 1; i < A->rows; ++i) {
                dval_t *sum = dv_num_const_qf(QF_ZERO);
                dval_t *denom;
                dval_t *x;

                if (!sum)
                    goto fail;

                for (size_t j = k; j < i; ++j) {
                    const dval_t *aij = mat_get_dval_or_zero(A, i, j);
                    const dval_t *xjk = mat_get_dval_or_zero(V, j, k);
                    dval_t *term;

                    dv_retain(aij);
                    dv_retain(xjk);
                    term = dval_mul_simplify(aij, xjk);
                    sum = dval_add_simplify(sum, term);
                    if (!sum)
                        goto fail;
                }

                {
                    const dval_t *diag = mat_get_dval_or_zero(A, i, i);

                    dv_retain(diag);
                    dv_retain(lambda);
                    denom = dval_sub_simplify(diag, lambda);
                }
                if (!denom) {
                    dv_free((dval_t *)sum);
                    goto fail;
                }

                if (dv_is_exact_zero(denom)) {
                    dv_free(denom);
                    if (dv_is_exact_zero(sum)) {
                        dv_free(sum);
                        continue;
                    }
                    dv_free((dval_t *)sum);
                    goto fail;
                }

                x = dval_neg_simplify(sum);
                denom = dval_simplify_owned(denom);
                if (!x) {
                    dv_free(denom);
                    goto fail;
                }

                x = dval_div_simplify(x, denom);
                if (!x)
                    goto fail;

                mat_set(V, i, k, &x);
                dv_free(x);
            }
        }
    }

    return V;

fail:
    mat_free(V);
    return NULL;
}

static matrix_t *mat_eigenvectors_dval_2x2(const matrix_t *A, dval_t **eigenvalues)
{
    matrix_t *V;
    dval_t *local_ev[2] = {NULL, NULL};
    dval_t **ev = eigenvalues ? eigenvalues : local_ev;
    const dval_t *a;
    const dval_t *b;
    const dval_t *c;
    const dval_t *d;

    if (!A || A->rows != 2 || A->cols != 2)
        return NULL;

    a = mat_get_dval_or_zero(A, 0, 0);
    b = mat_get_dval_or_zero(A, 0, 1);
    c = mat_get_dval_or_zero(A, 1, 0);
    d = mat_get_dval_or_zero(A, 1, 1);

    if (dval_is_exact_zero(b) && dval_is_exact_zero(c) && dval_exprs_equal_exact(a, d)) {
        if (!eigenvalues && mat_eigenvalues_dval(A, ev) != 0)
            return NULL;
        if (!eigenvalues) {
            dv_free(ev[0]);
            dv_free(ev[1]);
        }
        return mat_create_identity_dv(2);
    }

    if (!eigenvalues && mat_eigenvalues_dval(A, ev) != 0)
        return NULL;

    V = mat_new_dv(2, 2);
    if (!V)
        goto fail;

    for (size_t k = 0; k < 2; ++k) {
        dval_t *lambda = ev[k];
        dval_t *p;
        dval_t *q;
        dval_t *r;
        dval_t *s;
        dval_t *v0 = NULL;
        dval_t *v1 = NULL;

        dv_retain(a);
        dv_retain(lambda);
        p = dval_sub_simplify(a, lambda);
        dv_retain(b);
        q = dval_clone_for_storage(b);
        dv_retain(c);
        r = dval_clone_for_storage(c);
        dv_retain(d);
        dv_retain(lambda);
        s = dval_sub_simplify(d, lambda);
        if (!p || !q || !r || !s) {
            dv_free(p);
            dv_free(q);
            dv_free(r);
            dv_free(s);
            goto fail;
        }

        if (!dv_is_exact_zero(p) || !dv_is_exact_zero(q)) {
            v0 = dval_neg_simplify(q);
            q = NULL;
            v1 = p;
            p = NULL;
        } else if (!dv_is_exact_zero(r) || !dv_is_exact_zero(s)) {
            v0 = dval_neg_simplify(s);
            s = NULL;
            v1 = r;
            r = NULL;
        } else if (k == 0) {
            v0 = dval_clone_for_storage(DV_ONE);
            v1 = dval_clone_for_storage(DV_ZERO);
        } else {
            v0 = dval_clone_for_storage(DV_ZERO);
            v1 = dval_clone_for_storage(DV_ONE);
        }

        dv_free(p);
        dv_free(q);
        dv_free(r);
        dv_free(s);

        if (!v0 || !v1) {
            dv_free(v0);
            dv_free(v1);
            goto fail;
        }

        mat_set(V, 0, k, &v0);
        mat_set(V, 1, k, &v1);
        dv_free(v0);
        dv_free(v1);
    }

    if (!eigenvalues) {
        dv_free(ev[0]);
        dv_free(ev[1]);
    }
    return V;

fail:
    if (!eigenvalues) {
        dv_free(ev[0]);
        dv_free(ev[1]);
    }
    mat_free(V);
    return NULL;
}

int mat_eigendecompose_dval(const matrix_t *A, dval_t **eigenvalues, matrix_t **eigenvectors)
{
    matrix_t *V = NULL;
    dval_t **ev = eigenvalues;
    dval_t *local_ev_stack[2] = {NULL, NULL};
    dval_t **local_ev_heap = NULL;
    int rc;

    if (!A)
        return -1;
    if (A->rows != A->cols)
        return -2;

    if (!ev) {
        if (A->rows <= 2) {
            ev = local_ev_stack;
        } else {
            local_ev_heap = calloc(A->rows, sizeof(*local_ev_heap));
            if (!local_ev_heap)
                return -3;
            ev = local_ev_heap;
        }
    }

    rc = mat_eigenvalues_dval(A, ev);
    if (rc != 0)
        goto cleanup;

    if (!eigenvectors)
        goto success;

    if (mat_is_diagonal(A) || mat_is_upper_triangular(A) || mat_is_lower_triangular(A))
        V = mat_eigenvectors_dval_triangular(A);
    else if (A->rows == 2)
        V = mat_eigenvectors_dval_2x2(A, ev);

    if (!V) {
        for (size_t i = 0; i < A->rows; ++i) {
            dv_free(ev[i]);
            ev[i] = NULL;
        }
        rc = -3;
        goto cleanup;
    }

    *eigenvectors = V;
success:
    rc = 0;

cleanup:
    if (!eigenvalues && ev) {
        for (size_t i = 0; i < A->rows; ++i)
            dv_free(ev[i]);
    }
    free(local_ev_heap);
    return rc;
}

static matrix_t *mat_solve_dval_diagonal_exact(const matrix_t *A, const matrix_t *B)
{
    matrix_t *X;

    if (!A || !B || A->rows != A->cols || A->rows != B->rows)
        return NULL;

    X = mat_create_direct_solve_result(A, B, &dval_elem);
    if (!X)
        return NULL;

    for (size_t i = 0; i < A->rows; ++i) {
        const dval_t *diag = mat_get_dval_or_zero(A, i, i);

        if (!diag || dv_is_exact_zero(diag))
            goto fail;

        for (size_t j = 0; j < B->cols; ++j) {
            const dval_t *rhs = mat_get_dval_or_zero(B, i, j);
            dval_t *out = NULL;

            dv_retain(rhs);
            dv_retain(diag);
            out = dval_div_simplify(rhs, diag);
            if (!out)
                goto fail;
            mat_set(X, i, j, &out);
            dv_free(out);
        }
    }

    return X;

fail:
    mat_free(X);
    return NULL;
}

static dval_t *dval_bareiss_update_simplify(const dval_t *left_a,
                                            const dval_t *left_b,
                                            const dval_t *right_a,
                                            const dval_t *right_b,
                                            const dval_t *divisor,
                                            bool divide)
{
    dval_t *lhs = NULL;
    dval_t *rhs = NULL;
    dval_t *num = NULL;
    dval_t *out = NULL;

    if (left_a)
        dv_retain((dval_t *)left_a);
    if (left_b)
        dv_retain((dval_t *)left_b);
    lhs = dval_mul_simplify((dval_t *)left_a, (dval_t *)left_b);
    if (!lhs)
        return NULL;

    if (right_a)
        dv_retain((dval_t *)right_a);
    if (right_b)
        dv_retain((dval_t *)right_b);
    rhs = dval_mul_simplify((dval_t *)right_a, (dval_t *)right_b);
    if (!rhs) {
        dv_free(lhs);
        return NULL;
    }

    num = dval_sub_simplify(lhs, rhs);
    if (!num)
        return NULL;

    if (!divide)
        return num;

    if (divisor)
        dv_retain((dval_t *)divisor);
    out = dval_div_simplify(num, (dval_t *)divisor);
    return out;
}

static int mat_fraction_free_eliminate_dval(matrix_t *M,
                                            matrix_t *RHS,
                                            bool *negate_out)
{
    size_t n;
    const dval_t *prev_pivot = DV_ONE;
    bool negate = false;

    if (!M || M->elem != &dval_elem || M->rows != M->cols)
        return -1;
    if (RHS && (RHS->elem != &dval_elem || RHS->rows != M->rows))
        return -1;

    n = M->rows;
    if (negate_out)
        *negate_out = false;

    for (size_t k = 0; k + 1 < n; ++k) {
        size_t pivot_row = n;
        const dval_t *pivot = NULL;

        for (size_t i = k; i < n; ++i) {
            const dval_t *candidate = mat_get_dval_or_zero(M, i, k);
            if (!dval_is_exact_zero(candidate)) {
                pivot_row = i;
                break;
            }
        }

        if (pivot_row == n)
            return 1;

        if (pivot_row != k) {
            dense_swap_rows(M, k, pivot_row);
            if (RHS)
                dense_swap_rows(RHS, k, pivot_row);
            negate = !negate;
        }

        pivot = mat_get_dval_or_zero(M, k, k);
        if (!pivot || dval_is_exact_zero(pivot))
            return 1;

        for (size_t i = k + 1; i < n; ++i) {
            const dval_t *aik = mat_get_dval_or_zero(M, i, k);

            if (!aik || dval_is_exact_zero(aik)) {
                const dval_t *zero = DV_ZERO;
                mat_set(M, i, k, &zero);
                continue;
            }

            for (size_t j = k + 1; j < n; ++j) {
                const dval_t *aij = mat_get_dval_or_zero(M, i, j);
                const dval_t *akj = mat_get_dval_or_zero(M, k, j);
                dval_t *next = dval_bareiss_update_simplify(aij, pivot,
                                                            aik, akj,
                                                            prev_pivot,
                                                            k > 0);

                if (!next)
                    return -1;
                mat_set(M, i, j, &next);
                dv_free(next);
            }

            if (RHS) {
                for (size_t j = 0; j < RHS->cols; ++j) {
                    const dval_t *bij = mat_get_dval_or_zero(RHS, i, j);
                    const dval_t *bkj = mat_get_dval_or_zero(RHS, k, j);
                    dval_t *next = dval_bareiss_update_simplify(bij, pivot,
                                                                aik, bkj,
                                                                prev_pivot,
                                                                k > 0);

                    if (!next)
                        return -1;
                    mat_set(RHS, i, j, &next);
                    dv_free(next);
                }
            }

            {
                const dval_t *zero = DV_ZERO;
                mat_set(M, i, k, &zero);
            }
        }

        prev_pivot = pivot;
    }

    if (negate_out)
        *negate_out = negate;
    return 0;
}

static matrix_t *mat_forward_substitute_dval_exact(const matrix_t *L,
                                                   const matrix_t *B)
{
    matrix_t *X;

    if (!L || !B || L->rows != L->cols || L->rows != B->rows)
        return NULL;

    X = mat_create_dense_with_elem(L->cols, B->cols, &dval_elem);
    if (!X)
        return NULL;

    for (size_t i = 0; i < L->rows; ++i) {
        const dval_t *diag = mat_get_dval_or_zero(L, i, i);

        if (!diag || dv_is_exact_zero(diag))
            goto fail;

        for (size_t j = 0; j < B->cols; ++j) {
            const dval_t *sum = mat_get_dval_or_zero(B, i, j);
            dval_t *out = NULL;

            dv_retain(sum);

            for (size_t k = 0; k < i; ++k) {
                const dval_t *a = mat_get_dval_or_zero(L, i, k);
                const dval_t *x = mat_get_dval_or_zero(X, k, j);
                dval_t *prod = NULL;
                dval_t *new_sum = NULL;

                dv_retain(a);
                dv_retain(x);
                prod = dval_mul_simplify(a, x);
                if (!prod) {
                    dv_free((dval_t *)sum);
                    goto fail;
                }

                new_sum = dval_sub_simplify(sum, prod);
                if (!new_sum)
                    goto fail;
                sum = new_sum;
            }

            dv_retain(diag);
            out = dval_div_simplify(sum, diag);
            if (!out)
                goto fail;
            mat_set(X, i, j, &out);
            dv_free(out);
        }
    }

    return X;

fail:
    mat_free(X);
    return NULL;
}

static matrix_t *mat_backward_substitute_dval_exact(const matrix_t *U,
                                                    const matrix_t *B)
{
    matrix_t *X;

    if (!U || !B || U->rows != U->cols || U->rows != B->rows)
        return NULL;

    X = mat_create_dense_with_elem(U->cols, B->cols, &dval_elem);
    if (!X)
        return NULL;

    for (size_t ii = U->rows; ii-- > 0;) {
        const dval_t *diag = mat_get_dval_or_zero(U, ii, ii);

        if (!diag || dv_is_exact_zero(diag))
            goto fail;

        for (size_t j = 0; j < B->cols; ++j) {
            const dval_t *sum = mat_get_dval_or_zero(B, ii, j);
            dval_t *out = NULL;

            dv_retain(sum);

            for (size_t k = ii + 1; k < U->cols; ++k) {
                const dval_t *a = mat_get_dval_or_zero(U, ii, k);
                const dval_t *x = mat_get_dval_or_zero(X, k, j);
                dval_t *prod = NULL;
                dval_t *new_sum = NULL;

                dv_retain(a);
                dv_retain(x);
                prod = dval_mul_simplify(a, x);
                if (!prod) {
                    dv_free((dval_t *)sum);
                    goto fail;
                }

                new_sum = dval_sub_simplify(sum, prod);
                if (!new_sum)
                    goto fail;
                sum = new_sum;
            }

            dv_retain(diag);
            out = dval_div_simplify(sum, diag);
            if (!out)
                goto fail;
            mat_set(X, ii, j, &out);
            dv_free(out);
        }
    }

    return X;

fail:
    mat_free(X);
    return NULL;
}

static matrix_t *mat_solve_dval_dense_exact(const matrix_t *A, const matrix_t *B)
{
    size_t n;
    matrix_t *M = NULL;
    matrix_t *X = NULL;

    if (!A || !B || A->rows != A->cols || A->rows != B->rows)
        return NULL;

    n = A->rows;
    M = mat_create_dense_with_elem(n, n, &dval_elem);
    X = mat_create_dense_with_elem(B->rows, B->cols, &dval_elem);
    if (!M || !X)
        goto fail;

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            const dval_t *v = mat_get_dval_or_zero(A, i, j);
            mat_set(M, i, j, &v);
        }
        for (size_t j = 0; j < B->cols; ++j) {
            const dval_t *v = mat_get_dval_or_zero(B, i, j);
            mat_set(X, i, j, &v);
        }
    }

    if (mat_fraction_free_eliminate_dval(M, X, NULL) != 0)
        goto fail;

    for (size_t ii = n; ii-- > 0;) {
        const dval_t *diag = mat_get_dval_or_zero(M, ii, ii);

        if (!diag || dval_is_exact_zero(diag))
            goto fail;

        for (size_t j = 0; j < X->cols; ++j) {
            const dval_t *sum = mat_get_dval_or_zero(X, ii, j);
            dval_t *out = NULL;

            dv_retain(sum);

            for (size_t k = ii + 1; k < n; ++k) {
                const dval_t *uik = mat_get_dval_or_zero(M, ii, k);
                const dval_t *xkj = mat_get_dval_or_zero(X, k, j);
                dval_t *prod = NULL;
                dval_t *new_sum = NULL;

                dv_retain(uik);
                dv_retain(xkj);
                prod = dval_mul_simplify(uik, xkj);
                if (!prod) {
                    dv_free((dval_t *)sum);
                    goto fail;
                }

                new_sum = dval_sub_simplify(sum, prod);
                if (!new_sum)
                    goto fail;
                sum = new_sum;
            }

            dv_retain(diag);
            out = dval_div_simplify(sum, diag);
            if (!out)
                goto fail;
            mat_set(X, ii, j, &out);
            dv_free(out);
        }
    }

    mat_free(M);
    return X;

fail:
    mat_free(M);
    mat_free(X);
    return NULL;
}

matrix_t *mat_solve_dval_exact(const matrix_t *A, const matrix_t *B)
{
    matrix_t *X = NULL;

    if (!A || !B || A->rows != A->cols || A->rows != B->rows)
        return NULL;

    if (mat_has_diagonal_structure(A))
        X = mat_solve_dval_diagonal_exact(A, B);
    else if (mat_has_lower_triangular_structure(A))
        X = mat_forward_substitute_dval_exact(A, B);
    else if (mat_has_upper_triangular_structure(A))
        X = mat_backward_substitute_dval_exact(A, B);
    else
        X = mat_solve_dval_dense_exact(A, B);

    return mat_finalize_symbolic_result(X);
}

static matrix_t *mat_inverse_dval_upper_exact(const matrix_t *A)
{
    size_t n;
    matrix_t *I = NULL;

    if (!A || A->rows != A->cols)
        return NULL;

    n = A->rows;
    I = mat_create_upper_triangular_with_elem(n, n, &dval_elem);
    if (!I)
        return NULL;

    for (size_t ii = n; ii-- > 0; ) {
        dval_t *uii = NULL;
        dval_t *xii = NULL;

        mat_get(A, ii, ii, &uii);
        if (!uii || dv_is_exact_zero(uii))
            goto fail;

        dv_retain(uii);
        xii = dval_div_simplify(dv_num_const_d(1.0), uii);
        if (!xii)
            goto fail;
        mat_set(I, ii, ii, &xii);
        dv_free(xii);

        for (size_t j = ii + 1; j < n; ++j) {
            dval_t *sum = dv_num_const_d(0.0);
            dval_t *xij = NULL;

            if (!sum)
                goto fail;

            for (size_t k = ii + 1; k <= j; ++k) {
                dval_t *uik = NULL;
                dval_t *xkj = NULL;
                dval_t *prod = NULL;
                dval_t *new_sum = NULL;

                mat_get(A, ii, k, &uik);
                mat_get(I, k, j, &xkj);
                if (!uik || !xkj) {
                    dv_free(sum);
                    goto fail;
                }

                prod = dv_mul(uik, xkj);
                if (!prod) {
                    dv_free(sum);
                    goto fail;
                }

                new_sum = dv_add(sum, prod);
                dv_free(sum);
                dv_free(prod);
                sum = new_sum ? dval_simplify_owned(new_sum) : NULL;
                if (!sum)
                    goto fail;
            }

            dv_retain(uii);
            xij = dval_div_simplify(dval_neg_simplify(sum), uii);
            if (!xij)
                goto fail;
            mat_set(I, ii, j, &xij);
            dv_free(xij);
        }
    }

    return mat_finalize_symbolic_result(I);

fail:
    mat_free(I);
    return NULL;
}

static matrix_t *mat_inverse_dval_lower_exact(const matrix_t *A)
{
    matrix_t *AT = NULL;
    matrix_t *ATi = NULL;
    matrix_t *I = NULL;

    AT = mat_transpose(A);
    if (!AT)
        return NULL;

    ATi = mat_inverse_dval_upper_exact(AT);
    if (!ATi) {
        mat_free(AT);
        return NULL;
    }

    I = mat_transpose(ATi);
    mat_free(AT);
    mat_free(ATi);
    return mat_finalize_symbolic_result(I);
}

static matrix_t *mat_inverse_dval_dense3_exact(const matrix_t *A)
{
    dval_t *m[3][3] = {{0}};
    dval_t *cof[3][3] = {{0}};
    dval_t *det = NULL;
    matrix_t *I = NULL;

    if (!A || A->rows != 3 || A->cols != 3)
        return NULL;

    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j)
            mat_get(A, i, j, &m[i][j]);

    if (mat_det_dval_exact(A, &det) != 0)
        det = NULL;
    if (!det || dv_is_exact_zero(det))
        goto fail;

    cof[0][0] = dval_det2_simplify(m[1][1], m[1][2], m[2][1], m[2][2]);
    cof[0][1] = dval_neg_simplify(dval_det2_simplify(m[1][0], m[1][2], m[2][0], m[2][2]));
    cof[0][2] = dval_det2_simplify(m[1][0], m[1][1], m[2][0], m[2][1]);
    cof[1][0] = dval_neg_simplify(dval_det2_simplify(m[0][1], m[0][2], m[2][1], m[2][2]));
    cof[1][1] = dval_det2_simplify(m[0][0], m[0][2], m[2][0], m[2][2]);
    cof[1][2] = dval_neg_simplify(dval_det2_simplify(m[0][0], m[0][1], m[2][0], m[2][1]));
    cof[2][0] = dval_det2_simplify(m[0][1], m[0][2], m[1][1], m[1][2]);
    cof[2][1] = dval_neg_simplify(dval_det2_simplify(m[0][0], m[0][2], m[1][0], m[1][2]));
    cof[2][2] = dval_det2_simplify(m[0][0], m[0][1], m[1][0], m[1][1]);

    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j)
            if (!cof[i][j])
                goto fail;

    I = mat_new_dv(3, 3);
    if (!I)
        goto fail;

    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            dval_t *entry = NULL;

            dv_retain(cof[j][i]);
            dv_retain(det);
            entry = dval_div_simplify(cof[j][i], det);
            if (!entry)
                goto fail;
            mat_set(I, i, j, &entry);
            dv_free(entry);
        }
    }

    dv_free(det);
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j)
            dv_free(cof[i][j]);
    return mat_finalize_symbolic_result(I);

fail:
    dv_free(det);
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j)
            dv_free(cof[i][j]);
    mat_free(I);
    return NULL;
}

static matrix_t *mat_inverse_dval_dense_exact(const matrix_t *A)
{
    size_t n;
    matrix_t *I = NULL;
    matrix_t *Ai = NULL;

    if (!A || A->rows != A->cols)
        return NULL;

    n = A->rows;
    I = mat_create_identity_with_elem(n, &dval_elem);
    if (!I)
        return NULL;

    Ai = mat_solve_dval_exact(A, I);
    mat_free(I);
    return Ai;
}

int mat_det_dval_exact(const matrix_t *A, dval_t **determinant)
{
    matrix_t *M = NULL;
    dval_t *det = NULL;
    const dval_t *prev_pivot = DV_ONE;
    bool negate = false;
    size_t n;

    if (!A || !determinant)
        return -1;
    if (A->rows != A->cols)
        return -2;

    *determinant = NULL;
    n = A->rows;

    if (n == 0)
        return -2;

    if (n == 1) {
        mat_get(A, 0, 0, &det);
        if (!det)
            det = (dval_t *)DV_ZERO;
        dv_retain(det);
        *determinant = dval_simplify_owned(det);
        return *determinant ? 0 : -3;
    }

    M = mat_create_dense_with_elem(A->rows, A->cols, &dval_elem);
    if (!M)
        return -3;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            dval_t *v = NULL;
            mat_get(A, i, j, &v);
            mat_set(M, i, j, &v);
        }
    }

    for (size_t k = 0; k + 1 < n; ++k) {
        size_t pivot_row = n;
        dval_t *pivot = NULL;

        for (size_t i = k; i < n; ++i) {
            dval_t *candidate = NULL;
            mat_get(M, i, k, &candidate);
            if (!dval_is_exact_zero(candidate)) {
                pivot_row = i;
                break;
            }
        }

        if (pivot_row == n) {
            *determinant = dv_num_const_qf(QF_ZERO);
            mat_free(M);
            return *determinant ? 0 : -3;
        }

        if (pivot_row != k) {
            dense_swap_rows(M, k, pivot_row);
            negate = !negate;
        }

        mat_get(M, k, k, &pivot);
        if (dval_is_exact_zero(pivot)) {
            *determinant = dv_num_const_qf(QF_ZERO);
            mat_free(M);
            return *determinant ? 0 : -3;
        }

        for (size_t i = k + 1; i < n; ++i) {
            dval_t *aik = NULL;
            mat_get(M, i, k, &aik);

            for (size_t j = k + 1; j < n; ++j) {
                dval_t *aij = NULL;
                dval_t *akj = NULL;
                dval_t *lhs = NULL;
                dval_t *rhs = NULL;
                dval_t *num = NULL;
                dval_t *raw = NULL;
                dval_t *simp = NULL;

                mat_get(M, i, j, &aij);
                mat_get(M, k, j, &akj);

                lhs = dv_mul(aij, pivot);
                rhs = dv_mul(aik, akj);
                num = dv_sub(lhs, rhs);
                dv_free(lhs);
                dv_free(rhs);

                if (k == 0) {
                    raw = num;
                } else {
                    raw = dv_div(num, prev_pivot);
                    dv_free(num);
                }

                simp = dval_simplify_owned(raw);
                if (!simp) {
                    mat_free(M);
                    return -3;
                }

                mat_set(M, i, j, &simp);
                dv_free(simp);
            }

            {
                const dval_t *zero = DV_ZERO;
                mat_set(M, i, k, &zero);
            }
        }

        prev_pivot = pivot;
    }

    mat_get(M, n - 1, n - 1, &det);
    if (!det)
        det = (dval_t *)DV_ZERO;

    if (negate) {
        dval_t *raw = dv_neg(det);
        *determinant = dval_simplify_owned(raw);
    } else {
        *determinant = dv_simplify(det);
    }

    mat_free(M);
    return *determinant ? 0 : -3;
}

matrix_t *mat_inverse_dval_exact(const matrix_t *A)
{
    matrix_t *I = NULL;
    dval_t *a = NULL, *b = NULL, *c = NULL, *d = NULL;
    dval_t *det_left = NULL, *det_right = NULL, *det_raw = NULL, *det = NULL;
    dval_t *neg_b = NULL, *neg_c = NULL;
    dval_t *e00_raw = NULL, *e01_raw = NULL, *e10_raw = NULL, *e11_raw = NULL;
    dval_t *e00 = NULL, *e01 = NULL, *e10 = NULL, *e11 = NULL;

    if (!A || A->rows != A->cols)
        return NULL;

    if (A->rows == 1) {
        dval_t *v = NULL;
        dval_t *inv_raw = NULL;
        dval_t *inv = NULL;

        mat_get(A, 0, 0, &v);
        if (!v)
            return NULL;
        if (dv_is_exact_zero(v))
            return NULL;

        inv_raw = dv_div(DV_ONE, v);
        inv = dval_simplify_owned(inv_raw);
        if (!inv)
            return NULL;

        I = mat_new_dv(1, 1);
        if (!I) {
            dv_free(inv);
            return NULL;
        }

        mat_set(I, 0, 0, &inv);
        dv_free(inv);
        return mat_finalize_symbolic_result(I);
    }

    if (mat_is_upper_triangular(A))
        return mat_inverse_dval_upper_exact(A);

    if (mat_is_lower_triangular(A))
        return mat_inverse_dval_lower_exact(A);

    if (A->rows == 3)
        return mat_inverse_dval_dense3_exact(A);

    if (A->rows > 3)
        return mat_inverse_dval_dense_exact(A);

    mat_get(A, 0, 0, &a);
    mat_get(A, 0, 1, &b);
    mat_get(A, 1, 0, &c);
    mat_get(A, 1, 1, &d);

    det_left = dv_mul(a, d);
    det_right = dv_mul(b, c);
    det_raw = dv_sub(det_left, det_right);
    dv_free(det_left);
    det_left = NULL;
    dv_free(det_right);
    det_right = NULL;
    det = dval_simplify_owned(det_raw);
    det_raw = NULL;
    if (!det)
        return NULL;

    if (dv_is_exact_zero(det)) {
        dv_free(det);
        return NULL;
    }

    neg_b = dv_neg(b);
    neg_c = dv_neg(c);

    e00_raw = dv_div(d, det);
    e01_raw = dv_div(neg_b, det);
    e10_raw = dv_div(neg_c, det);
    e11_raw = dv_div(a, det);

    e00 = dval_simplify_owned(e00_raw);
    e00_raw = NULL;
    e01 = dval_simplify_owned(e01_raw);
    e01_raw = NULL;
    e10 = dval_simplify_owned(e10_raw);
    e10_raw = NULL;
    e11 = dval_simplify_owned(e11_raw);
    e11_raw = NULL;

    dv_free(neg_b);
    neg_b = NULL;
    dv_free(neg_c);
    neg_c = NULL;
    dv_free(det);
    det = NULL;

    if (!e00 || !e01 || !e10 || !e11)
        goto fail;

    I = mat_new_dv(2, 2);
    if (!I)
        goto fail;

    mat_set(I, 0, 0, &e00);
    mat_set(I, 0, 1, &e01);
    mat_set(I, 1, 0, &e10);
    mat_set(I, 1, 1, &e11);

    dv_free(e00);
    dv_free(e01);
    dv_free(e10);
    dv_free(e11);
    return mat_finalize_symbolic_result(I);

fail:
    dv_free(det_left);
    dv_free(det_right);
    dv_free(det);
    dv_free(neg_b);
    dv_free(neg_c);
    dv_free(e00_raw);
    dv_free(e01_raw);
    dv_free(e10_raw);
    dv_free(e11_raw);
    dv_free(e00);
    dv_free(e01);
    dv_free(e10);
    dv_free(e11);
    mat_free(I);
    return NULL;
}

matrix_t *mat_create_zero_with_elem(size_t rows, size_t cols,
                                    const struct elem_vtable *elem)
{
    matrix_t *M;

    if (!elem)
        return NULL;

    M = mat_create_dense_with_elem(rows, cols, elem);
    if (!M)
        return NULL;

    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            if (elem_is_symbolic(elem)) {
                const dval_t *zero = DV_ZERO;
                mat_set(M, i, j, &zero);
            } else {
                mat_set(M, i, j, elem->zero);
            }
        }
    }

    return M;
}

static matrix_t *mat_minor_matrix(const matrix_t *A, size_t skip_row, size_t skip_col)
{
    matrix_t *M;
    size_t mi = 0;

    if (!A || A->rows == 0 || A->cols == 0 || skip_row >= A->rows || skip_col >= A->cols)
        return NULL;

    M = mat_create_zero_with_elem(A->rows - 1, A->cols - 1, A->elem);
    if (!M)
        return NULL;

    for (size_t i = 0; i < A->rows; ++i) {
        size_t mj = 0;
        unsigned char raw[64];

        if (i == skip_row)
            continue;

        for (size_t j = 0; j < A->cols; ++j) {
            if (j == skip_col)
                continue;
            mat_get(A, i, j, raw);
            mat_set(M, mi, mj, raw);
            mj++;
        }
        mi++;
    }

    return M;
}

matrix_t *mat_extract_block(const matrix_t *A,
                            size_t row0, size_t rows,
                            size_t col0, size_t cols)
{
    matrix_t *M;

    if (!A || row0 + rows > A->rows || col0 + cols > A->cols)
        return NULL;

    M = mat_create_zero_with_elem(rows, cols, A->elem);
    if (!M)
        return NULL;

    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            unsigned char raw[64];
            mat_get(A, row0 + i, col0 + j, raw);
            mat_set(M, i, j, raw);
        }
    }

    return M;
}

bool mat_insert_block(matrix_t *A, size_t row0, size_t col0, const matrix_t *B)
{
    if (!A || !B || A->elem != B->elem)
        return false;
    if (row0 + B->rows > A->rows || col0 + B->cols > A->cols)
        return false;

    for (size_t i = 0; i < B->rows; ++i) {
        for (size_t j = 0; j < B->cols; ++j) {
            unsigned char raw[64];
            mat_get(B, i, j, raw);
            mat_set(A, row0 + i, col0 + j, raw);
        }
    }

    return true;
}

matrix_t *mat_build_block_2x2(const matrix_t *B11, const matrix_t *B12,
                              const matrix_t *B21, const matrix_t *B22)
{
    matrix_t *M = NULL;

    if (!B11 || !B12 || !B21 || !B22)
        return NULL;
    if (B11->elem != B12->elem || B11->elem != B21->elem || B11->elem != B22->elem)
        return NULL;
    if (B11->rows != B12->rows || B21->rows != B22->rows)
        return NULL;
    if (B11->cols != B21->cols || B12->cols != B22->cols)
        return NULL;

    M = mat_create_zero_with_elem(B11->rows + B21->rows, B11->cols + B12->cols, B11->elem);
    if (!M)
        return NULL;

    if (!mat_insert_block(M, 0, 0, B11) ||
        !mat_insert_block(M, 0, B11->cols, B12) ||
        !mat_insert_block(M, B11->rows, 0, B21) ||
        !mat_insert_block(M, B11->rows, B11->cols, B22)) {
        mat_free(M);
        return NULL;
    }

    return M;
}

matrix_t *mat_charpoly_numeric(const matrix_t *A)
{
    const struct elem_vtable *e;
    matrix_t *coeffs = NULL;
    matrix_t *B = NULL;
    unsigned char coeff_prev[64];
    unsigned char trace_val[64];
    unsigned char k_val[64];
    unsigned char inv_k[64];
    unsigned char coeff[64];
    unsigned char diag[64];

    if (!A || A->rows != A->cols || !elem_supports_numeric_algorithms(A->elem))
        return NULL;

    e = A->elem;
    coeffs = mat_create_zero_with_elem(A->rows + 1, 1, e);
    B = mat_create_zero_with_elem(A->rows, A->cols, e);
    if (!coeffs || !B)
        goto fail;

    memcpy(coeff_prev, e->one, e->size);
    mat_set(coeffs, 0, 0, coeff_prev);

    for (size_t k = 1; k <= A->rows; ++k) {
        matrix_t *T = mat_copy_as_dense(B);
        matrix_t *Bnew = NULL;

        if (!T)
            goto fail;

        for (size_t i = 0; i < A->rows; ++i) {
            mat_get(T, i, i, diag);
            e->add(diag, diag, coeff_prev);
            mat_set(T, i, i, diag);
        }

        Bnew = mat_mul(A, T);
        mat_free(T);
        if (!Bnew)
            goto fail;

        if (mat_trace(Bnew, trace_val) != 0) {
            mat_free(Bnew);
            goto fail;
        }

        e->from_real(k_val, (double)k);
        e->inv(inv_k, k_val);
        e->mul(coeff, trace_val, inv_k);
        e->sub(coeff, e->zero, coeff);

        mat_set(coeffs, k, 0, coeff);
        memcpy(coeff_prev, coeff, e->size);

        mat_free(B);
        B = Bnew;
    }

    mat_free(B);
    return coeffs;

fail:
    mat_free(coeffs);
    mat_free(B);
    return NULL;
}

matrix_t *mat_charpoly_dval(const matrix_t *A)
{
    matrix_t *coeffs = NULL;
    matrix_t *B = NULL;
    dval_t *coeff_prev = NULL;

    if (!A || A->rows != A->cols || A->elem != &dval_elem)
        return NULL;

    coeffs = mat_create_zero_with_elem(A->rows + 1, 1, &dval_elem);
    B = mat_create_zero_with_elem(A->rows, A->cols, &dval_elem);
    coeff_prev = dv_num_const_d(1.0);
    if (!coeffs || !B || !coeff_prev)
        goto fail;

    const dval_t *one = DV_ONE;
    mat_set(coeffs, 0, 0, &one);

    for (size_t k = 1; k <= A->rows; ++k) {
        matrix_t *T = mat_copy_as_dense(B);
        matrix_t *Bnew = NULL;
        dval_t *trace_val = NULL;
        dval_t *den = NULL;
        dval_t *quot = NULL;
        dval_t *coeff = NULL;

        if (!T)
            goto fail;

        for (size_t i = 0; i < A->rows; ++i) {
            const dval_t *diag = mat_get_dval_or_zero(T, i, i);
            dval_t *new_diag;

            dv_retain(diag);
            dv_retain(coeff_prev);
            new_diag = dval_add_simplify(diag, coeff_prev);
            if (!new_diag) {
                mat_free(T);
                goto fail;
            }
            mat_set(T, i, i, &new_diag);
            dv_free(new_diag);
        }

        Bnew = mat_mul(A, T);
        mat_free(T);
        if (!Bnew)
            goto fail;

        if (mat_trace(Bnew, &trace_val) != 0 || !trace_val) {
            mat_free(Bnew);
            goto fail;
        }

        den = dv_num_const_d((double)k);
        quot = dval_div_simplify(trace_val, den);
        if (!quot) {
            mat_free(Bnew);
            goto fail;
        }

        coeff = dval_neg_simplify(quot);
        if (!coeff) {
            mat_free(Bnew);
            goto fail;
        }

        mat_set(coeffs, k, 0, &coeff);
        dv_free(coeff_prev);
        coeff_prev = coeff;

        mat_free(B);
        B = Bnew;
    }

    dv_free(coeff_prev);
    mat_free(B);
    return coeffs;

fail:
    dv_free(coeff_prev);
    mat_free(coeffs);
    mat_free(B);
    return NULL;
}

static void dval_poly_coeffs_free(dval_t **coeffs, size_t count)
{
    if (!coeffs)
        return;
    for (size_t i = 0; i < count; ++i)
        dv_free(coeffs[i]);
    free(coeffs);
}

static dval_t **dval_poly_multiply_linear(dval_t **coeffs, size_t degree, dval_t *lambda)
{
    dval_t **next = NULL;

    if (!coeffs || !lambda)
        return NULL;

    next = calloc(degree + 2, sizeof(*next));
    if (!next)
        return NULL;

    next[0] = dval_clone_for_storage(coeffs[0]);
    if (!next[0])
        goto fail;

    for (size_t k = 1; k <= degree; ++k) {
        dval_t *term = NULL;

        dv_retain(lambda);
        dv_retain(coeffs[k - 1]);
        term = dval_mul_simplify(lambda, coeffs[k - 1]);
        if (!term)
            goto fail;

        dv_retain(coeffs[k]);
        next[k] = dval_sub_simplify(coeffs[k], term);
        if (!next[k])
            goto fail;
    }

    dval_t *tail = NULL;

    dv_retain(lambda);
    dv_retain(coeffs[degree]);
    tail = dval_mul_simplify(lambda, coeffs[degree]);
    if (!tail)
        goto fail;
    next[degree + 1] = dval_neg_simplify(tail);
    if (!next[degree + 1])
        goto fail;

    return next;

fail:
    dval_poly_coeffs_free(next, degree + 2);
    return NULL;
}

static matrix_t *dval_poly_matrix_from_coeffs(dval_t **coeffs, size_t degree)
{
    matrix_t *P = NULL;

    if (!coeffs)
        return NULL;

    P = mat_create_zero_with_elem(degree + 1, 1, &dval_elem);
    if (!P)
        return NULL;

    for (size_t i = 0; i <= degree; ++i)
        mat_set(P, i, 0, &coeffs[i]);

    return P;
}

matrix_t *mat_const_identity_with_elem(size_t n,
                                       const struct elem_vtable *elem,
                                       const void *scalar)
{
    matrix_t *I = NULL;

    if (!elem || !scalar)
        return NULL;

    I = mat_create_zero_with_elem(n, n, elem);
    if (!I)
        return NULL;

    for (size_t i = 0; i < n; ++i)
        mat_set(I, i, i, scalar);

    return I;
}

static matrix_t *mat_shift_dval_exact(const matrix_t *A, const dval_t *lambda)
{
    matrix_t *Shifted = NULL;

    if (!A || A->elem != &dval_elem || !lambda || A->rows != A->cols)
        return NULL;

    Shifted = mat_copy_as_dense(A);
    if (!Shifted)
        return NULL;

    for (size_t i = 0; i < A->rows; ++i) {
        dval_t *diag = NULL;
        dval_t *new_diag = NULL;

        mat_get(Shifted, i, i, &diag);
        if (!diag)
            diag = (dval_t *)DV_ZERO;

        dv_retain(diag);
        dv_retain(lambda);
        new_diag = dval_sub_simplify(diag, lambda);
        if (!new_diag) {
            mat_free(Shifted);
            return NULL;
        }
        mat_set(Shifted, i, i, &new_diag);
        dv_free(new_diag);
    }

    return Shifted;
}

static int mat_dval_nullity_exact(const matrix_t *A)
{
    matrix_t *N = NULL;
    int nullity = -1;

    if (!A || A->elem != &dval_elem)
        return -1;

    N = mat_nullspace_dval_exact(A);
    if (!N)
        return -1;

    nullity = (int)mat_get_col_count(N);
    mat_free(N);
    return nullity;
}

static size_t mat_dval_triangular_root_exponent(const matrix_t *A,
                                                const dval_t *lambda,
                                                size_t multiplicity)
{
    matrix_t *Shifted = NULL;
    matrix_t *Power = NULL;
    size_t exponent = 0;

    if (!A || !lambda || multiplicity == 0)
        return 0;

    Shifted = mat_shift_dval_exact(A, lambda);
    if (!Shifted)
        return 0;

    Power = mat_copy_as_dense(Shifted);
    if (!Power) {
        mat_free(Shifted);
        return 0;
    }

    for (size_t k = 1; k <= multiplicity; ++k) {
        int nullity = mat_dval_nullity_exact(Power);

        if (nullity < 0)
            break;
        if ((size_t)nullity >= multiplicity) {
            exponent = k;
            break;
        }

        if (k < multiplicity) {
            matrix_t *Next = mat_mul(Power, Shifted);
            mat_free(Power);
            Power = Next;
            if (!Power)
                break;
        }
    }

    mat_free(Power);
    mat_free(Shifted);
    return exponent;
}

static void dval_rref_info_reset(dval_rref_info_t *info)
{
    if (!info)
        return;

    mat_free(info->R);
    free(info->pivot_cols);
    free(info->is_pivot);
    info->R = NULL;
    info->pivot_cols = NULL;
    info->is_pivot = NULL;
    info->rank = 0;
}

static int mat_dval_rref_exact(const matrix_t *A, dval_rref_info_t *out)
{
    matrix_t *R = NULL;
    size_t *pivot_cols = NULL;
    bool *is_pivot = NULL;
    size_t rank = 0;
    size_t row = 0;

    if (!A || A->elem != &dval_elem || !out)
        return -1;

    memset(out, 0, sizeof(*out));

    R = mat_copy_as_dense(A);
    if (!R)
        return -2;

    pivot_cols = calloc(A->cols ? A->cols : 1, sizeof(*pivot_cols));
    is_pivot = calloc(A->cols ? A->cols : 1, sizeof(*is_pivot));
    if (!pivot_cols || !is_pivot)
        goto fail;

    for (size_t col = 0; col < A->cols && row < A->rows; ++col) {
        size_t pivot_row = A->rows;

        for (size_t r = row; r < A->rows; ++r) {
            dval_t *candidate = NULL;
            mat_get(R, r, col, &candidate);
            if (!dval_is_exact_zero(candidate)) {
                pivot_row = r;
                break;
            }
        }

        if (pivot_row == A->rows)
            continue;

        if (pivot_row != row)
            dense_swap_rows(R, row, pivot_row);

        {
            dval_t *pivot = NULL;
            mat_get(R, row, col, &pivot);

            for (size_t j = col; j < A->cols; ++j) {
                dval_t *entry = NULL;
                dval_t *new_entry;

                mat_get(R, row, j, &entry);
                dv_retain(entry);
                dv_retain(pivot);
                new_entry = dval_div_simplify(entry, pivot);
                if (!new_entry)
                    goto fail;
                mat_set(R, row, j, &new_entry);
                dv_free(new_entry);
            }
        }

        for (size_t i = 0; i < A->rows; ++i) {
            dval_t *factor = NULL;

            if (i == row)
                continue;

            mat_get(R, i, col, &factor);
            if (dval_is_exact_zero(factor))
                continue;
            dv_retain(factor);

            for (size_t j = col; j < A->cols; ++j) {
                dval_t *rij = NULL;
                dval_t *rrj = NULL;
                dval_t *term = NULL;
                dval_t *new_rij = NULL;

                mat_get(R, i, j, &rij);
                mat_get(R, row, j, &rrj);
                dv_retain(rij);
                dv_retain(factor);
                dv_retain(rrj);
                term = dval_mul_simplify(factor, rrj);
                if (!term) {
                    dv_free(factor);
                    goto fail;
                }
                new_rij = dval_sub_simplify(rij, term);
                if (!new_rij) {
                    dv_free(factor);
                    goto fail;
                }
                mat_set(R, i, j, &new_rij);
                dv_free(new_rij);
            }

            dv_free(factor);
        }

        pivot_cols[rank] = col;
        is_pivot[col] = true;
        rank++;
        row++;
    }

    out->R = R;
    out->pivot_cols = pivot_cols;
    out->is_pivot = is_pivot;
    out->rank = rank;
    return 0;

fail:
    mat_free(R);
    free(pivot_cols);
    free(is_pivot);
    return -2;
}

static matrix_t *mat_dval_extract_columns(const matrix_t *A,
                                          const size_t *cols,
                                          size_t ncols)
{
    matrix_t *C = NULL;

    if (!A || A->elem != &dval_elem || (!cols && ncols != 0))
        return NULL;

    C = mat_create_zero_with_elem(A->rows, ncols, &dval_elem);
    if (!C)
        return NULL;

    for (size_t j = 0; j < ncols; ++j) {
        for (size_t i = 0; i < A->rows; ++i) {
            dval_t *entry = NULL;

            mat_get(A, i, cols[j], &entry);
            mat_set(C, i, j, &entry);
        }
    }

    return C;
}

static matrix_t *mat_dval_build_pivot_factor(const matrix_t *A,
                                             const dval_rref_info_t *info)
{
    matrix_t *F = NULL;

    if (!A || !info || !info->R || A->elem != &dval_elem)
        return NULL;

    F = mat_create_zero_with_elem(info->rank, A->cols, &dval_elem);
    if (!F)
        return NULL;

    for (size_t k = 0; k < info->rank; ++k) {
        const dval_t *one = DV_ONE;
        mat_set(F, k, info->pivot_cols[k], &one);
    }

    for (size_t col = 0; col < A->cols; ++col) {
        if (info->is_pivot[col])
            continue;

        for (size_t k = 0; k < info->rank; ++k) {
            dval_t *entry = NULL;

            mat_get(info->R, k, col, &entry);
            if (!dval_is_exact_zero(entry))
                mat_set(F, k, col, &entry);
        }
    }

    return F;
}

matrix_t *mat_minpoly_dval(const matrix_t *A)
{
    dval_t **roots = NULL;
    size_t *exponents = NULL;
    size_t root_count = 0;
    dval_t **coeffs = NULL;
    size_t degree = 0;
    matrix_t *P = NULL;

    if (!A || A->rows != A->cols || A->elem != &dval_elem)
        return NULL;

    if (A->rows == 0) {
        coeffs = calloc(1, sizeof(*coeffs));
        if (!coeffs)
            return NULL;
        coeffs[0] = dval_clone_for_storage(DV_ONE);
        if (!coeffs[0]) {
            free(coeffs);
            return NULL;
        }
        P = dval_poly_matrix_from_coeffs(coeffs, 0);
        dval_poly_coeffs_free(coeffs, 1);
        return P;
    }

    if (mat_is_diagonal(A) || mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) {
        roots = calloc(A->rows, sizeof(*roots));
        exponents = calloc(A->rows, sizeof(*exponents));
        if (!roots || !exponents)
            goto fail;

        for (size_t i = 0; i < A->rows; ++i) {
            const dval_t *diag = mat_get_dval_or_zero(A, i, i);
            size_t idx = 0;

            while (idx < root_count && !dval_exprs_equal_exact(roots[idx], diag))
                ++idx;

            if (idx == root_count) {
                size_t multiplicity = 0;
                size_t exponent = 0;

                for (size_t j = 0; j < A->rows; ++j) {
                    const dval_t *other = mat_get_dval_or_zero(A, j, j);
                    if (dval_exprs_equal_exact(diag, other))
                        ++multiplicity;
                }

                exponent = mat_is_diagonal(A)
                    ? 1
                    : mat_dval_triangular_root_exponent(A, diag, multiplicity);
                if (exponent == 0)
                    goto fail;

                roots[root_count] = dval_clone_for_storage(diag);
                if (!roots[root_count])
                    goto fail;
                exponents[root_count] = exponent;
                ++root_count;
            }
        }
    } else if (A->rows == 1) {
        roots = calloc(1, sizeof(*roots));
        exponents = calloc(1, sizeof(*exponents));
        if (!roots || !exponents)
            goto fail;
        roots[0] = dval_clone_for_storage(mat_get_dval_or_zero(A, 0, 0));
        if (!roots[0])
            goto fail;
        exponents[0] = 1;
        root_count = 1;
    } else if (A->rows == 2) {
        const dval_t *a = mat_get_dval_or_zero(A, 0, 0);
        const dval_t *b = mat_get_dval_or_zero(A, 0, 1);
        const dval_t *c = mat_get_dval_or_zero(A, 1, 0);
        const dval_t *d = mat_get_dval_or_zero(A, 1, 1);
        dval_t *ev[2] = {NULL, NULL};

        roots = calloc(2, sizeof(*roots));
        exponents = calloc(2, sizeof(*exponents));
        if (!roots || !exponents)
            goto fail;

        if (dval_is_exact_zero(b) && dval_is_exact_zero(c) && dval_exprs_equal_exact(a, d)) {
            roots[0] = dval_clone_for_storage(a);
            if (!roots[0])
                goto fail;
            exponents[0] = 1;
            root_count = 1;
        } else {
            if (mat_eigenvalues_dval(A, ev) != 0 || !ev[0] || !ev[1]) {
                dv_free(ev[0]);
                dv_free(ev[1]);
                goto fail;
            }

            if (dval_exprs_equal_exact(ev[0], ev[1])) {
                roots[0] = ev[0];
                ev[0] = NULL;
                exponents[0] = 2;
                root_count = 1;
            } else {
                roots[0] = ev[0];
                roots[1] = ev[1];
                ev[0] = NULL;
                ev[1] = NULL;
                exponents[0] = 1;
                exponents[1] = 1;
                root_count = 2;
            }

            dv_free(ev[0]);
            dv_free(ev[1]);
        }
    } else {
        return NULL;
    }

    coeffs = calloc(1, sizeof(*coeffs));
    if (!coeffs)
        goto fail;
    coeffs[0] = dval_clone_for_storage(DV_ONE);
    if (!coeffs[0])
        goto fail;

    for (size_t i = 0; i < root_count; ++i) {
        for (size_t p = 0; p < exponents[i]; ++p) {
            dval_t **next = dval_poly_multiply_linear(coeffs, degree, roots[i]);
            if (!next)
                goto fail;
            dval_poly_coeffs_free(coeffs, degree + 1);
            coeffs = next;
            ++degree;
        }
    }

    P = dval_poly_matrix_from_coeffs(coeffs, degree);

fail:
    if (roots) {
        for (size_t i = 0; i < root_count; ++i)
            dv_free(roots[i]);
    }
    free(roots);
    free(exponents);
    dval_poly_coeffs_free(coeffs, degree + 1);
    return P;
}

matrix_t *mat_adjugate_exact(const matrix_t *A)
{
    matrix_t *Adj = NULL;
    const struct elem_vtable *e;

    if (!A || A->rows != A->cols)
        return NULL;

    e = A->elem;
    Adj = mat_create_zero_with_elem(A->rows, A->cols, e);
    if (!Adj)
        return NULL;

    if (A->rows == 1) {
        if (elem_is_symbolic(e)) {
            const dval_t *one = DV_ONE;
            mat_set(Adj, 0, 0, &one);
        } else {
            mat_set(Adj, 0, 0, e->one);
        }
        return Adj;
    }

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            matrix_t *Minor = mat_minor_matrix(A, j, i);

            if (!Minor)
                goto fail;

            if (elem_is_symbolic(e)) {
                dval_t *det = NULL;
                dval_t *entry;

                if (mat_det(Minor, &det) != 0 || !det) {
                    mat_free(Minor);
                    goto fail;
                }

                entry = det;
                if (((i + j) & 1u) != 0u) {
                    entry = dval_neg_simplify(det);
                    det = NULL;
                    if (!entry) {
                        mat_free(Minor);
                        goto fail;
                    }
                }

                mat_set(Adj, i, j, &entry);
                dv_free(entry);
            } else {
                unsigned char det[64];

                if (mat_det(Minor, det) != 0) {
                    mat_free(Minor);
                    goto fail;
                }
                if (((i + j) & 1u) != 0u)
                    e->sub(det, e->zero, det);
                mat_set(Adj, i, j, det);
            }

            mat_free(Minor);
        }
    }

    return Adj;

fail:
    mat_free(Adj);
    return NULL;
}

matrix_t *mat_nullspace_dval_exact(const matrix_t *A)
{
    dval_rref_info_t info = {0};
    matrix_t *N = NULL;
    size_t nullity = 0;
    size_t basis_col = 0;

    if (!A || A->elem != &dval_elem)
        return NULL;

    if (mat_dval_rref_exact(A, &info) != 0)
        goto fail;

    nullity = A->cols - info.rank;
    N = mat_create_zero_with_elem(A->cols, nullity, &dval_elem);
    if (!N)
        goto fail;

    for (size_t free_col = 0; free_col < A->cols; ++free_col) {
        if (info.is_pivot[free_col])
            continue;

        {
            const dval_t *one = DV_ONE;
            mat_set(N, free_col, basis_col, &one);
        }

        for (size_t r = 0; r < info.rank; ++r) {
            size_t pivot_col = info.pivot_cols[r];
            dval_t *entry = NULL;
            dval_t *coeff;

            mat_get(info.R, r, free_col, &entry);
            if (dval_is_exact_zero(entry))
                continue;

            dv_retain(entry);
            coeff = dval_neg_simplify(entry);
            if (!coeff)
                goto fail;
            mat_set(N, pivot_col, basis_col, &coeff);
            dv_free(coeff);
        }

        basis_col++;
    }

    dval_rref_info_reset(&info);
    return N;

fail:
    dval_rref_info_reset(&info);
    mat_free(N);
    return NULL;
}

int mat_rank_dval_exact(const matrix_t *A)
{
    dval_rref_info_t info = {0};
    int rank;

    if (!A || A->elem != &dval_elem)
        return -1;

    if (mat_dval_rref_exact(A, &info) != 0)
        return -2;

    rank = (int)info.rank;
    dval_rref_info_reset(&info);
    return rank;
}

matrix_t *mat_pseudoinverse_dval_exact(const matrix_t *A)
{
    dval_rref_info_t info = {0};
    matrix_t *AT = NULL;
    matrix_t *Gram = NULL;
    matrix_t *Gram_inv = NULL;
    matrix_t *C = NULL;
    matrix_t *F = NULL;
    matrix_t *C_pinv = NULL;
    matrix_t *F_pinv = NULL;
    matrix_t *Pinv = NULL;

    if (!A || A->elem != &dval_elem)
        return NULL;

    if (mat_dval_rref_exact(A, &info) != 0)
        return NULL;

    if (info.rank == 0) {
        Pinv = mat_create_zero_with_elem(A->cols, A->rows, &dval_elem);
        dval_rref_info_reset(&info);
        return mat_finalize_symbolic_result(Pinv);
    }

    if (A->rows >= A->cols && info.rank == A->cols) {
        AT = mat_transpose(A);
        Gram = AT ? mat_mul(AT, A) : NULL;
        Gram_inv = Gram ? mat_inverse(Gram) : NULL;
        Pinv = (Gram_inv && AT) ? mat_mul(Gram_inv, AT) : NULL;
        goto done;
    }

    if (A->rows < A->cols && info.rank == A->rows) {
        AT = mat_transpose(A);
        Gram = AT ? mat_mul(A, AT) : NULL;
        Gram_inv = Gram ? mat_inverse(Gram) : NULL;
        Pinv = (AT && Gram_inv) ? mat_mul(AT, Gram_inv) : NULL;
        goto done;
    }

    C = mat_dval_extract_columns(A, info.pivot_cols, info.rank);
    F = C ? mat_dval_build_pivot_factor(A, &info) : NULL;
    C_pinv = C ? mat_pseudoinverse_dval_exact(C) : NULL;
    F_pinv = F ? mat_pseudoinverse_dval_exact(F) : NULL;
    Pinv = (F_pinv && C_pinv) ? mat_mul(F_pinv, C_pinv) : NULL;

done:
    dval_rref_info_reset(&info);
    mat_free(AT);
    mat_free(Gram);
    mat_free(Gram_inv);
    mat_free(C);
    mat_free(F);
    mat_free(C_pinv);
    mat_free(F_pinv);
    return mat_finalize_symbolic_result(Pinv);
}
