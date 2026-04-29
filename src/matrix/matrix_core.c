#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "matrix_internal.h"
#include "qfloat.h"
#include "qcomplex.h"
#include "matrix.h"
#include "dval_pattern.h"

/* ============================================================
   Internal matrix construction helpers (forward declarations)
   ============================================================ */

static struct matrix_t *store_create_dense(size_t rows, size_t cols,
                                           const struct elem_vtable *elem);
static struct matrix_t *store_create_sparse(size_t rows, size_t cols,
                                            const struct elem_vtable *elem);
static struct matrix_t *store_create_identity(size_t rows, size_t cols,
                                              const struct elem_vtable *elem);
static struct matrix_t *store_create_diagonal(size_t rows, size_t cols,
                                              const struct elem_vtable *elem);
static struct matrix_t *store_create_upper_triangular(size_t rows, size_t cols,
                                                      const struct elem_vtable *elem);
static struct matrix_t *store_create_lower_triangular(size_t rows, size_t cols,
                                                      const struct elem_vtable *elem);
static bool dense_is_sparse_storage(const struct matrix_t *A);
static bool sparse_is_sparse_storage(const struct matrix_t *A);
static bool identity_is_sparse_storage(const struct matrix_t *A);
static bool diagonal_is_sparse_storage(const struct matrix_t *A);
static bool upper_triangular_is_sparse_storage(const struct matrix_t *A);
static bool lower_triangular_is_sparse_storage(const struct matrix_t *A);
static bool dense_is_sparse_like(const struct matrix_t *A);
static bool sparse_is_sparse_like(const struct matrix_t *A);
static bool identity_is_sparse_like(const struct matrix_t *A);
static bool diagonal_is_sparse_like(const struct matrix_t *A);
static bool upper_triangular_is_sparse_like(const struct matrix_t *A);
static bool lower_triangular_is_sparse_like(const struct matrix_t *A);
static bool store_true(const struct matrix_t *A);
static bool store_false(const struct matrix_t *A);
static bool dense_is_diagonal(const struct matrix_t *A);
static bool sparse_is_diagonal(const struct matrix_t *A);
static bool upper_triangular_is_diagonal(const struct matrix_t *A);
static bool lower_triangular_is_diagonal(const struct matrix_t *A);
static bool generic_is_upper_triangular(const struct matrix_t *A);
static bool generic_is_lower_triangular(const struct matrix_t *A);
static size_t dense_nonzero_count(const struct matrix_t *A);
static size_t sparse_nonzero_count(const struct matrix_t *A);
static size_t identity_nonzero_count(const struct matrix_t *A);
static size_t diagonal_nonzero_count(const struct matrix_t *A);
static size_t upper_triangular_nonzero_count(const struct matrix_t *A);
static size_t lower_triangular_nonzero_count(const struct matrix_t *A);
static const struct store_vtable *store_self_unary(const struct matrix_t *A);
static const struct store_vtable *identity_unary_store(const struct matrix_t *A);
static const struct store_vtable *store_self_transpose(const struct matrix_t *A);
static const struct store_vtable *upper_triangular_transpose_store(const struct matrix_t *A);
static const struct store_vtable *lower_triangular_transpose_store(const struct matrix_t *A);
static struct matrix_t *mat_create_with_store(size_t rows, size_t cols,
                                              const struct elem_vtable *elem,
                                              const struct store_vtable *store);
static struct matrix_t *mat_create_elementwise_unary_result(size_t rows, size_t cols,
                                                            const struct elem_vtable *elem,
                                                            const struct matrix_t *layout_src);
static struct matrix_t *mat_create_transpose_result(size_t rows, size_t cols,
                                                    const struct elem_vtable *elem,
                                                    const struct matrix_t *layout_src);
static struct matrix_t *mat_create_binary_result(size_t rows, size_t cols,
                                                 const struct elem_vtable *elem,
                                                 const struct matrix_t *A,
                                                 const struct matrix_t *B);
static bool mat_uses_sparse_storage(const struct matrix_t *A);
static bool mat_is_sparse_like(const struct matrix_t *A);
static bool mat_has_diagonal_structure(const struct matrix_t *A);
static bool mat_has_upper_triangular_structure(const struct matrix_t *A);
static bool mat_has_lower_triangular_structure(const struct matrix_t *A);
static matrix_t *mat_convert_dense(const matrix_t *A, const struct elem_vtable *target);
static matrix_t *mat_convert_preserving_store(const matrix_t *A,
                                              const struct elem_vtable *target);

/* ============================================================
   Storage vtables (dense, identity)
   ============================================================ */

typedef struct sparse_entry_t {
    size_t col;
    struct sparse_entry_t *next;
    unsigned char value[];
} sparse_entry_t;

static const struct store_vtable sparse_store;
static const struct store_vtable diagonal_store;
static const struct store_vtable upper_triangular_store;
static const struct store_vtable lower_triangular_store;

static bool elem_is_dval(const struct elem_vtable *elem)
{
    return elem && elem->kind == ELEM_DVAL;
}

static bool dval_is_exact_zero(const dval_t *dv)
{
    return !dv || dv_is_exact_zero(dv);
}

static dval_t *dval_clone_for_storage(dval_t *dv)
{
    if (!dv)
        return NULL;
    if (dv == DV_ZERO || dv == DV_ONE)
        return dv_new_const_qc(dv_get_val(dv));
    dv_retain(dv);
    return dv;
}

static matrix_t *mat_finalize_symbolic_result(matrix_t *A)
{
    matrix_t *simplified;

    if (!A || A->elem != &dval_elem)
        return A;

    simplified = mat_simplify_symbolic(A);
    mat_free(A);
    return simplified;
}

static void elem_copy_value(const struct elem_vtable *elem, void *dst, const void *src)
{
    if (!elem_is_dval(elem)) {
        memcpy(dst, src, elem->size);
        return;
    }

    dval_t *dv = src ? *(dval_t *const *)src : NULL;
    *(dval_t **)dst = dval_clone_for_storage(dv);
}

static void elem_destroy_value(const struct elem_vtable *elem, void *slot)
{
    dval_t *dv;

    if (!elem_is_dval(elem) || !slot)
        return;

    dv = *(dval_t **)slot;
    if (dv)
        dv_free(dv);
    *(dval_t **)slot = NULL;
}

static void elem_simplify_value(const struct elem_vtable *elem, void *slot)
{
    dval_t *dv;
    dval_t *simp;

    if (!elem_is_dval(elem) || !slot)
        return;

    dv = *(dval_t **)slot;
    if (!dv)
        return;

    simp = dv_simplify(dv);
    dv_free(dv);
    *(dval_t **)slot = simp;
}

static bool elem_is_structural_zero(const struct elem_vtable *elem, const void *val)
{
    if (!elem)
        return true;

    if (!elem_is_dval(elem))
        return elem->cmp(val, elem->zero) == 0;

    return dval_is_exact_zero(*(dval_t *const *)val);
}

static bool elem_supports_numeric_algorithms(const struct elem_vtable *elem)
{
    return elem && elem->kind != ELEM_DVAL;
}

static void dense_swap_rows(struct matrix_t *A, size_t r1, size_t r2);
static matrix_t *mat_nullspace_dval_exact(const matrix_t *A);

static dval_t *dval_simplify_owned(dval_t *dv)
{
    dval_t *simp;

    if (!dv)
        return NULL;

    simp = dv_simplify(dv);
    dv_free(dv);
    return simp;
}

static dval_t *dval_div_simplify(dval_t *num, dval_t *den)
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

static int mat_simplify_symbolic_inplace(matrix_t *A)
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

static dval_t *dval_mul_simplify(dval_t *a, dval_t *b)
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

    return dval_simplify_owned(raw);
}

static dval_t *dval_add_simplify(dval_t *a, dval_t *b)
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

    return dval_simplify_owned(raw);
}

static dval_t *dval_sub_simplify(dval_t *a, dval_t *b)
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

static int mat_det_dval_exact(const matrix_t *A, dval_t **determinant);
static matrix_t *mat_create_direct_solve_result(const matrix_t *A,
                                                const matrix_t *B,
                                                const struct elem_vtable *elem);

static dval_t *mat_get_dval_or_zero(const matrix_t *A, size_t i, size_t j)
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
        dval_t *a = mat_get_dval_or_zero(A, 0, 0);
        dval_t *b = mat_get_dval_or_zero(A, 0, 1);
        dval_t *c = mat_get_dval_or_zero(A, 1, 0);
        dval_t *d = mat_get_dval_or_zero(A, 1, 1);
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

        scaled_bc = dval_mul_simplify(dv_new_const_d(4.0), bc);
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

        half = dv_new_const_d(0.5);
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
        dv_free(a);
        dv_free(b);
        dv_free(c);
        dv_free(d);
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
            dval_t *lambda = mat_get_dval_or_zero(A, k, k);

            for (size_t ii = k; ii-- > 0;) {
                dval_t *sum = dv_new_const(QF_ZERO);
                dval_t *denom;
                dval_t *x;

                if (!sum)
                    goto fail;

                for (size_t j = ii + 1; j <= k; ++j) {
                    dval_t *aij = mat_get_dval_or_zero(A, ii, j);
                    dval_t *xjk = mat_get_dval_or_zero(V, j, k);
                    dval_t *term;

                    dv_retain(aij);
                    dv_retain(xjk);
                    term = dval_mul_simplify(aij, xjk);
                    sum = dval_add_simplify(sum, term);
                    if (!sum)
                        goto fail;
                }

                {
                    dval_t *diag = mat_get_dval_or_zero(A, ii, ii);

                    dv_retain(diag);
                    dv_retain(lambda);
                    denom = dval_sub_simplify(diag, lambda);
                }
                if (!denom) {
                    dv_free(sum);
                    goto fail;
                }

                if (dv_is_exact_zero(denom)) {
                    dv_free(denom);
                    if (dv_is_exact_zero(sum)) {
                        dv_free(sum);
                        continue;
                    }
                    dv_free(sum);
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
            dval_t *lambda = mat_get_dval_or_zero(A, k, k);

            for (size_t i = k + 1; i < A->rows; ++i) {
                dval_t *sum = dv_new_const(QF_ZERO);
                dval_t *denom;
                dval_t *x;

                if (!sum)
                    goto fail;

                for (size_t j = k; j < i; ++j) {
                    dval_t *aij = mat_get_dval_or_zero(A, i, j);
                    dval_t *xjk = mat_get_dval_or_zero(V, j, k);
                    dval_t *term;

                    dv_retain(aij);
                    dv_retain(xjk);
                    term = dval_mul_simplify(aij, xjk);
                    sum = dval_add_simplify(sum, term);
                    if (!sum)
                        goto fail;
                }

                {
                    dval_t *diag = mat_get_dval_or_zero(A, i, i);

                    dv_retain(diag);
                    dv_retain(lambda);
                    denom = dval_sub_simplify(diag, lambda);
                }
                if (!denom) {
                    dv_free(sum);
                    goto fail;
                }

                if (dv_is_exact_zero(denom)) {
                    dv_free(denom);
                    if (dv_is_exact_zero(sum)) {
                        dv_free(sum);
                        continue;
                    }
                    dv_free(sum);
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
    dval_t *a;
    dval_t *b;
    dval_t *c;
    dval_t *d;

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
        q = dval_simplify_owned(b);
        dv_retain(c);
        r = dval_simplify_owned(c);
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

static int mat_eigendecompose_dval(const matrix_t *A, dval_t **eigenvalues, matrix_t **eigenvectors)
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
        dval_t *diag = mat_get_dval_or_zero(A, i, i);

        if (!diag || dv_is_exact_zero(diag))
            goto fail;

        for (size_t j = 0; j < B->cols; ++j) {
            dval_t *rhs = mat_get_dval_or_zero(B, i, j);
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
        dval_t *diag = mat_get_dval_or_zero(L, i, i);

        if (!diag || dv_is_exact_zero(diag))
            goto fail;

        for (size_t j = 0; j < B->cols; ++j) {
            dval_t *sum = mat_get_dval_or_zero(B, i, j);
            dval_t *out = NULL;

            dv_retain(sum);

            for (size_t k = 0; k < i; ++k) {
                dval_t *a = mat_get_dval_or_zero(L, i, k);
                dval_t *x = mat_get_dval_or_zero(X, k, j);
                dval_t *prod = NULL;
                dval_t *new_sum = NULL;

                dv_retain(a);
                dv_retain(x);
                prod = dval_mul_simplify(a, x);
                if (!prod) {
                    dv_free(sum);
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
        dval_t *diag = mat_get_dval_or_zero(U, ii, ii);

        if (!diag || dv_is_exact_zero(diag))
            goto fail;

        for (size_t j = 0; j < B->cols; ++j) {
            dval_t *sum = mat_get_dval_or_zero(B, ii, j);
            dval_t *out = NULL;

            dv_retain(sum);

            for (size_t k = ii + 1; k < U->cols; ++k) {
                dval_t *a = mat_get_dval_or_zero(U, ii, k);
                dval_t *x = mat_get_dval_or_zero(X, k, j);
                dval_t *prod = NULL;
                dval_t *new_sum = NULL;

                dv_retain(a);
                dv_retain(x);
                prod = dval_mul_simplify(a, x);
                if (!prod) {
                    dv_free(sum);
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
            dval_t *v = mat_get_dval_or_zero(A, i, j);
            mat_set(M, i, j, &v);
        }
        for (size_t j = 0; j < B->cols; ++j) {
            dval_t *v = mat_get_dval_or_zero(B, i, j);
            mat_set(X, i, j, &v);
        }
    }

    for (size_t k = 0; k < n; ++k) {
        size_t pivot_row = n;
        dval_t *pivot = NULL;

        for (size_t i = k; i < n; ++i) {
            dval_t *candidate = mat_get_dval_or_zero(M, i, k);
            if (!dval_is_exact_zero(candidate)) {
                pivot_row = i;
                break;
            }
        }

        if (pivot_row == n)
            goto fail;

        if (pivot_row != k) {
            dense_swap_rows(M, k, pivot_row);
            dense_swap_rows(X, k, pivot_row);
        }

        pivot = mat_get_dval_or_zero(M, k, k);
        if (!pivot || dval_is_exact_zero(pivot))
            goto fail;
        dv_retain(pivot);

        for (size_t j = 0; j < n; ++j) {
            dval_t *mkj = mat_get_dval_or_zero(M, k, j);
            dval_t *new_mkj = NULL;

            dv_retain(mkj);
            dv_retain(pivot);
            new_mkj = dval_div_simplify(mkj, pivot);
            if (!new_mkj)
                goto fail;
            mat_set(M, k, j, &new_mkj);
            dv_free(new_mkj);
        }

        for (size_t j = 0; j < X->cols; ++j) {
            dval_t *xkj = mat_get_dval_or_zero(X, k, j);
            dval_t *new_xkj = NULL;

            dv_retain(xkj);
            dv_retain(pivot);
            new_xkj = dval_div_simplify(xkj, pivot);
            if (!new_xkj)
                goto fail;
            mat_set(X, k, j, &new_xkj);
            dv_free(new_xkj);
        }
        dv_free(pivot);

        for (size_t i = 0; i < n; ++i) {
            dval_t *factor = NULL;

            if (i == k)
                continue;

            factor = mat_get_dval_or_zero(M, i, k);
            if (!factor || dval_is_exact_zero(factor))
                continue;
            dv_retain(factor);

            for (size_t j = 0; j < n; ++j) {
                dval_t *mij = mat_get_dval_or_zero(M, i, j);
                dval_t *mkj = mat_get_dval_or_zero(M, k, j);
                dval_t *term = NULL;
                dval_t *new_mij = NULL;

                dv_retain(factor);
                dv_retain(mkj);
                term = dval_mul_simplify(factor, mkj);
                if (!term)
                    goto fail;

                dv_retain(mij);
                new_mij = dval_sub_simplify(mij, term);
                if (!new_mij)
                    goto fail;
                mat_set(M, i, j, &new_mij);
                dv_free(new_mij);
            }

            for (size_t j = 0; j < X->cols; ++j) {
                dval_t *xij = mat_get_dval_or_zero(X, i, j);
                dval_t *xkj = mat_get_dval_or_zero(X, k, j);
                dval_t *term = NULL;
                dval_t *new_xij = NULL;

                dv_retain(factor);
                dv_retain(xkj);
                term = dval_mul_simplify(factor, xkj);
                if (!term)
                    goto fail;

                dv_retain(xij);
                new_xij = dval_sub_simplify(xij, term);
                if (!new_xij)
                    goto fail;
                mat_set(X, i, j, &new_xij);
                dv_free(new_xij);
            }

            dv_free(factor);
        }
    }

    mat_free(M);
    return X;

fail:
    mat_free(M);
    mat_free(X);
    return NULL;
}

static matrix_t *mat_solve_dval_exact(const matrix_t *A, const matrix_t *B)
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
        xii = dval_div_simplify(dv_new_const_d(1.0), uii);
        if (!xii)
            goto fail;
        mat_set(I, ii, ii, &xii);
        dv_free(xii);

        for (size_t j = ii + 1; j < n; ++j) {
            dval_t *sum = dv_new_const_d(0.0);
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
    matrix_t *M = NULL;
    matrix_t *I = NULL;

    if (!A || A->rows != A->cols)
        return NULL;

    n = A->rows;
    M = mat_create_dense_with_elem(n, n, &dval_elem);
    I = mat_create_dense_with_elem(n, n, &dval_elem);
    if (!M || !I)
        goto fail;

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            dval_t *v = NULL;
            dval_t *id = (i == j) ? DV_ONE : DV_ZERO;

            mat_get(A, i, j, &v);
            mat_set(M, i, j, &v);
            mat_set(I, i, j, &id);
        }
    }

    for (size_t k = 0; k < n; ++k) {
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

        if (pivot_row == n)
            goto fail;

        if (pivot_row != k) {
            dense_swap_rows(M, k, pivot_row);
            dense_swap_rows(I, k, pivot_row);
        }

        mat_get(M, k, k, &pivot);
        if (!pivot || dval_is_exact_zero(pivot))
            goto fail;
        dv_retain(pivot);

        for (size_t j = 0; j < n; ++j) {
            dval_t *mkj = NULL;
            dval_t *ikj = NULL;
            dval_t *new_mkj = NULL;
            dval_t *new_ikj = NULL;

            mat_get(M, k, j, &mkj);
            mat_get(I, k, j, &ikj);

            if (mkj)
                dv_retain(mkj);
            dv_retain(pivot);
            new_mkj = dval_div_simplify(mkj, pivot);
            if (!new_mkj)
                goto fail;

            if (ikj)
                dv_retain(ikj);
            dv_retain(pivot);
            new_ikj = dval_div_simplify(ikj, pivot);
            if (!new_ikj) {
                dv_free(new_mkj);
                goto fail;
            }

            mat_set(M, k, j, &new_mkj);
            mat_set(I, k, j, &new_ikj);
            dv_free(new_mkj);
            dv_free(new_ikj);
        }
        dv_free(pivot);

        for (size_t i = 0; i < n; ++i) {
            dval_t *factor = NULL;

            if (i == k)
                continue;

            mat_get(M, i, k, &factor);
            if (!factor || dval_is_exact_zero(factor))
                continue;
            dv_retain(factor);

            for (size_t j = 0; j < n; ++j) {
                dval_t *mij = NULL, *mkj = NULL, *iij = NULL, *ikj = NULL;
                dval_t *term_m = NULL, *term_i = NULL;
                dval_t *new_mij = NULL, *new_iij = NULL;

                mat_get(M, i, j, &mij);
                mat_get(M, k, j, &mkj);
                mat_get(I, i, j, &iij);
                mat_get(I, k, j, &ikj);

                dv_retain(factor);
                if (mkj)
                    dv_retain(mkj);
                term_m = dval_mul_simplify(factor, mkj);
                if (!term_m)
                    goto fail;

                if (mij)
                    dv_retain(mij);
                new_mij = dval_sub_simplify(mij, term_m);
                if (!new_mij)
                    goto fail;

                mat_set(M, i, j, &new_mij);
                dv_free(new_mij);

                dv_retain(factor);
                if (ikj)
                    dv_retain(ikj);
                term_i = dval_mul_simplify(factor, ikj);
                if (!term_i)
                    goto fail;

                if (iij)
                    dv_retain(iij);
                new_iij = dval_sub_simplify(iij, term_i);
                if (!new_iij)
                    goto fail;

                mat_set(I, i, j, &new_iij);
                dv_free(new_iij);
            }
            dv_free(factor);
        }
    }

    mat_free(M);
    return mat_finalize_symbolic_result(I);

fail:
    mat_free(M);
    mat_free(I);
    return NULL;
}

static int mat_det_dval_exact(const matrix_t *A, dval_t **determinant)
{
    matrix_t *M = NULL;
    dval_t *det = NULL;
    dval_t *prev_pivot = DV_ONE;
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
            det = DV_ZERO;
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
            *determinant = dv_new_const(QF_ZERO);
            mat_free(M);
            return *determinant ? 0 : -3;
        }

        if (pivot_row != k) {
            dense_swap_rows(M, k, pivot_row);
            negate = !negate;
        }

        mat_get(M, k, k, &pivot);
        if (dval_is_exact_zero(pivot)) {
            *determinant = dv_new_const(QF_ZERO);
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
                dval_t *zero = DV_ZERO;
                mat_set(M, i, k, &zero);
            }
        }

        prev_pivot = pivot;
    }

    mat_get(M, n - 1, n - 1, &det);
    if (!det)
        det = DV_ZERO;

    if (negate) {
        dval_t *raw = dv_neg(det);
        *determinant = dval_simplify_owned(raw);
    } else {
        *determinant = dv_simplify(det);
    }

    mat_free(M);
    return *determinant ? 0 : -3;
}

static matrix_t *mat_inverse_dval_exact(const matrix_t *A)
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

static matrix_t *mat_create_zero_with_elem(size_t rows, size_t cols,
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
            if (elem->kind == ELEM_DVAL) {
                dval_t *zero = DV_ZERO;
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

static matrix_t *mat_extract_block(const matrix_t *A,
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

static bool mat_insert_block(matrix_t *A, size_t row0, size_t col0, const matrix_t *B)
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

static matrix_t *mat_build_block_2x2(const matrix_t *B11, const matrix_t *B12,
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

static matrix_t *mat_charpoly_numeric(const matrix_t *A)
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

static matrix_t *mat_charpoly_dval(const matrix_t *A)
{
    matrix_t *coeffs = NULL;
    matrix_t *B = NULL;
    dval_t *coeff_prev = NULL;

    if (!A || A->rows != A->cols || A->elem != &dval_elem)
        return NULL;

    coeffs = mat_create_zero_with_elem(A->rows + 1, 1, &dval_elem);
    B = mat_create_zero_with_elem(A->rows, A->cols, &dval_elem);
    coeff_prev = dv_new_const_d(1.0);
    if (!coeffs || !B || !coeff_prev)
        goto fail;

    {
        dval_t *one = DV_ONE;
        mat_set(coeffs, 0, 0, &one);
    }

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
            dval_t *diag = mat_get_dval_or_zero(T, i, i);
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

        den = dv_new_const_d((double)k);
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

    {
        dval_t *tail = NULL;

        dv_retain(lambda);
        dv_retain(coeffs[degree]);
        tail = dval_mul_simplify(lambda, coeffs[degree]);
        if (!tail)
            goto fail;
        next[degree + 1] = dval_neg_simplify(tail);
        if (!next[degree + 1])
            goto fail;
    }

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

static matrix_t *mat_const_identity_with_elem(size_t n,
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

static matrix_t *mat_shift_dval_exact(const matrix_t *A, dval_t *lambda)
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
            diag = DV_ZERO;

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
                                                dval_t *lambda,
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

static matrix_t *mat_minpoly_dval(const matrix_t *A)
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
            dval_t *diag = mat_get_dval_or_zero(A, i, i);
            size_t idx = 0;

            while (idx < root_count && !dval_exprs_equal_exact(roots[idx], diag))
                ++idx;

            if (idx == root_count) {
                size_t multiplicity = 0;
                size_t exponent = 0;

                for (size_t j = 0; j < A->rows; ++j) {
                    dval_t *other = mat_get_dval_or_zero(A, j, j);
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
        dval_t *a = mat_get_dval_or_zero(A, 0, 0);
        dval_t *b = mat_get_dval_or_zero(A, 0, 1);
        dval_t *c = mat_get_dval_or_zero(A, 1, 0);
        dval_t *d = mat_get_dval_or_zero(A, 1, 1);
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

static matrix_t *mat_adjugate_exact(const matrix_t *A)
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
        if (e->kind == ELEM_DVAL) {
            dval_t *one = DV_ONE;
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

            if (e->kind == ELEM_DVAL) {
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

static matrix_t *mat_nullspace_dval_exact(const matrix_t *A)
{
    matrix_t *R = NULL;
    matrix_t *N = NULL;
    size_t *pivot_cols = NULL;
    bool *is_pivot = NULL;
    size_t rank = 0;
    size_t row = 0;
    size_t nullity = 0;
    size_t basis_col = 0;

    if (!A || A->elem != &dval_elem)
        return NULL;

    R = mat_copy_as_dense(A);
    if (!R)
        goto fail;

    pivot_cols = calloc(A->cols ? A->cols : 1, sizeof(size_t));
    is_pivot = calloc(A->cols ? A->cols : 1, sizeof(bool));
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

    nullity = A->cols - rank;
    N = mat_create_zero_with_elem(A->cols, nullity, &dval_elem);
    if (!N)
        goto fail;

    for (size_t free_col = 0; free_col < A->cols; ++free_col) {
        if (is_pivot[free_col])
            continue;

        {
            dval_t *one = DV_ONE;
            mat_set(N, free_col, basis_col, &one);
        }

        for (size_t r = 0; r < rank; ++r) {
            size_t pivot_col = pivot_cols[r];
            dval_t *entry = NULL;
            dval_t *coeff;

            mat_get(R, r, free_col, &entry);
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

    free(pivot_cols);
    free(is_pivot);
    mat_free(R);
    return N;

fail:
    free(pivot_cols);
    free(is_pivot);
    mat_free(R);
    mat_free(N);
    return NULL;
}

static bool dense_alloc(struct matrix_t *A) {
    size_t n = A->rows, m = A->cols, es = A->elem->size;

    A->data = malloc(n * sizeof(void*));
    if (!A->data)
        return false;
    for (size_t i = 0; i < n; i++) {
        A->data[i] = calloc(m, es);
        if (!A->data[i]) {
            for (size_t k = 0; k < i; k++) free(A->data[k]);
            free(A->data);
            A->data = NULL;
            return false;
        }
    }
    A->nnz = 0;
    return true;
}

static void dense_free(struct matrix_t *A) {
    if (!A->data) return;
    for (size_t i = 0; i < A->rows; i++) {
        if (elem_is_dval(A->elem)) {
            for (size_t j = 0; j < A->cols; j++) {
                void *slot = (char *)A->data[i] + j * A->elem->size;
                elem_destroy_value(A->elem, slot);
            }
        }
        free(A->data[i]);
    }
    free(A->data);
    A->data = NULL;
}

static void dense_get(const struct matrix_t *A, size_t i, size_t j, void *out) {
    memcpy(out,
           (char*)A->data[i] + j * A->elem->size,
           A->elem->size);
}

static void dense_set(struct matrix_t *A, size_t i, size_t j, const void *val) {
    void *slot = (char *)A->data[i] + j * A->elem->size;

    elem_destroy_value(A->elem, slot);
    elem_copy_value(A->elem, slot, val);
}

static void dense_swap_rows(struct matrix_t *A, size_t r1, size_t r2)
{
    void *tmp;

    if (!A || r1 == r2)
        return;

    tmp = A->data[r1];
    A->data[r1] = A->data[r2];
    A->data[r2] = tmp;
}

static void dense_row_eliminate_from(struct matrix_t *A, size_t dst_row, size_t src_row,
                                     size_t col_start, const void *factor)
{
    unsigned char src[64], dst[64], prod[64], out[64];

    if (!A || !factor || dst_row == src_row)
        return;

    for (size_t j = col_start; j < A->cols; j++) {
        dense_get(A, dst_row, j, dst);
        dense_get(A, src_row, j, src);
        A->elem->mul(prod, factor, src);
        A->elem->sub(out, dst, prod);
        dense_set(A, dst_row, j, out);
    }
}

static const struct store_vtable dense_store = {
    .create                   = store_create_dense,
    .alloc                    = dense_alloc,
    .free                     = dense_free,
    .get                      = dense_get,
    .set                      = dense_set,
    .swap_rows                = dense_swap_rows,
    .row_eliminate_from       = dense_row_eliminate_from,
    .materialise              = NULL,
    .is_sparse_storage        = dense_is_sparse_storage,
    .is_sparse_like           = dense_is_sparse_like,
    .is_diagonal              = dense_is_diagonal,
    .is_upper_triangular      = generic_is_upper_triangular,
    .is_lower_triangular      = generic_is_lower_triangular,
    .nonzero_count            = dense_nonzero_count,
    .elementwise_unary_store  = store_self_unary,
    .transpose_store          = store_self_transpose
};

/* ---------- sparse storage ---------- */

static sparse_entry_t *sparse_find_prev(const struct matrix_t *A, size_t row, size_t col)
{
    sparse_entry_t *prev = NULL;
    sparse_entry_t *cur = A->data ? (sparse_entry_t *)A->data[row] : NULL;

    while (cur && cur->col < col) {
        prev = cur;
        cur = cur->next;
    }

    return prev;
}

static bool sparse_alloc(struct matrix_t *A)
{
    A->data = calloc(A->rows, sizeof(void *));
    if (!A->data)
        return false;
    A->nnz = 0;
    return true;
}

static void sparse_free(struct matrix_t *A)
{
    if (!A->data)
        return;

    for (size_t i = 0; i < A->rows; i++) {
        sparse_entry_t *cur = (sparse_entry_t *)A->data[i];
        while (cur) {
            sparse_entry_t *next = cur->next;
            elem_destroy_value(A->elem, cur->value);
            free(cur);
            cur = next;
        }
    }

    free(A->data);
    A->data = NULL;
    A->nnz = 0;
}

static void sparse_get(const struct matrix_t *A, size_t i, size_t j, void *out)
{
    sparse_entry_t *prev = sparse_find_prev(A, i, j);
    sparse_entry_t *cur = prev ? prev->next : (A->data ? (sparse_entry_t *)A->data[i] : NULL);

    if (cur && cur->col == j) {
        memcpy(out, cur->value, A->elem->size);
        return;
    }

    memcpy(out, A->elem->zero, A->elem->size);
}

static void sparse_set(struct matrix_t *A, size_t i, size_t j, const void *val)
{
    sparse_entry_t *prev;
    sparse_entry_t *cur;
    int is_zero;

    prev = sparse_find_prev(A, i, j);
    cur = prev ? prev->next : (A->data ? (sparse_entry_t *)A->data[i] : NULL);
    is_zero = elem_is_structural_zero(A->elem, val);

    if (cur && cur->col == j) {
        if (is_zero) {
            if (prev)
                prev->next = cur->next;
            else
                A->data[i] = cur->next;
            elem_destroy_value(A->elem, cur->value);
            free(cur);
            if (A->nnz > 0)
                A->nnz--;
        } else {
            elem_destroy_value(A->elem, cur->value);
            elem_copy_value(A->elem, cur->value, val);
        }
        return;
    }

    if (is_zero)
        return;

    cur = malloc(sizeof(*cur) + A->elem->size);
    if (!cur)
        return;

    cur->col = j;
    memset(cur->value, 0, A->elem->size);
    elem_copy_value(A->elem, cur->value, val);
    if (prev) {
        cur->next = prev->next;
        prev->next = cur;
    } else {
        cur->next = (sparse_entry_t *)A->data[i];
        A->data[i] = cur;
    }
    A->nnz++;
}

static void sparse_swap_rows(struct matrix_t *A, size_t r1, size_t r2)
{
    void *tmp;

    if (!A || r1 == r2)
        return;

    tmp = A->data[r1];
    A->data[r1] = A->data[r2];
    A->data[r2] = tmp;
}

static void sparse_row_eliminate_from(struct matrix_t *A, size_t dst_row, size_t src_row,
                                      size_t col_start, const void *factor)
{
    sparse_entry_t *cur;
    unsigned char dst[64], prod[64], out[64];

    if (!A || !factor || dst_row == src_row || !A->data)
        return;

    cur = (sparse_entry_t *)A->data[src_row];
    while (cur && cur->col < col_start)
        cur = cur->next;

    while (cur) {
        sparse_get(A, dst_row, cur->col, dst);
        A->elem->mul(prod, factor, cur->value);
        A->elem->sub(out, dst, prod);
        sparse_set(A, dst_row, cur->col, out);
        cur = cur->next;
    }
}

static void sparse_materialise(struct matrix_t *A)
{
    void **old_rows;
    size_t old_nnz;

    old_rows = A->data;
    old_nnz = A->nnz;
    A->store = &dense_store;
    A->data = NULL;
    A->nnz = 0;
    if (!dense_alloc(A)) {
        A->store = &sparse_store;
        A->data = old_rows;
        A->nnz = old_nnz;
        return;
    }

    for (size_t i = 0; i < A->rows; i++) {
        sparse_entry_t *cur = (sparse_entry_t *)old_rows[i];
        while (cur) {
            dense_set(A, i, cur->col, cur->value);
            A->nnz++;
            elem_destroy_value(A->elem, cur->value);
            cur = cur->next;
        }
    }

    for (size_t i = 0; i < A->rows; i++) {
        sparse_entry_t *cur = (sparse_entry_t *)old_rows[i];
        while (cur) {
            sparse_entry_t *next = cur->next;
            free(cur);
            cur = next;
        }
    }
    free(old_rows);
}

static const struct store_vtable sparse_store = {
    .create                   = store_create_sparse,
    .alloc                    = sparse_alloc,
    .free                     = sparse_free,
    .get                      = sparse_get,
    .set                      = sparse_set,
    .swap_rows                = sparse_swap_rows,
    .row_eliminate_from       = sparse_row_eliminate_from,
    .materialise              = sparse_materialise,
    .is_sparse_storage        = sparse_is_sparse_storage,
    .is_sparse_like           = sparse_is_sparse_like,
    .is_diagonal              = sparse_is_diagonal,
    .is_upper_triangular      = generic_is_upper_triangular,
    .is_lower_triangular      = generic_is_lower_triangular,
    .nonzero_count            = sparse_nonzero_count,
    .elementwise_unary_store  = store_self_unary,
    .transpose_store          = store_self_transpose
};

/* ---------- identity storage ---------- */

static void ident_materialise(struct matrix_t *A);

static bool ident_alloc(struct matrix_t *A) {
    A->data = NULL;
    A->nnz = A->rows;
    return true;
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

static void ident_swap_rows(struct matrix_t *A, size_t r1, size_t r2)
{
    ident_materialise(A);
    dense_swap_rows(A, r1, r2);
}

static void ident_row_eliminate_from(struct matrix_t *A, size_t dst_row, size_t src_row,
                                     size_t col_start, const void *factor)
{
    ident_materialise(A);
    dense_row_eliminate_from(A, dst_row, src_row, col_start, factor);
}

static const struct store_vtable identity_store = {
    .create                   = store_create_identity,
    .alloc                    = ident_alloc,
    .free                     = ident_free,
    .get                      = ident_get,
    .set                      = ident_set,
    .swap_rows                = ident_swap_rows,
    .row_eliminate_from       = ident_row_eliminate_from,
    .materialise              = ident_materialise,
    .is_sparse_storage        = identity_is_sparse_storage,
    .is_sparse_like           = identity_is_sparse_like,
    .is_diagonal              = store_true,
    .is_upper_triangular      = store_true,
    .is_lower_triangular      = store_true,
    .nonzero_count            = identity_nonzero_count,
    .elementwise_unary_store  = identity_unary_store,
    .transpose_store          = store_self_transpose
};

static void ident_materialise(struct matrix_t *A) {
    A->store = &dense_store;
    if (!dense_alloc(A))
        return;

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++)
            dense_set(A, i, j, (i == j) ? A->elem->one : A->elem->zero);
}

/* ---------- diagonal storage ---------- */

static bool diagonal_alloc(struct matrix_t *A)
{
    size_t i;

    A->data = malloc(A->rows * sizeof(void *));
    if (!A->data)
        return false;

    for (i = 0; i < A->rows; i++) {
        A->data[i] = calloc(1, A->elem->size);
        if (!A->data[i]) {
            while (i > 0) {
                i--;
                free(A->data[i]);
            }
            free(A->data);
            A->data = NULL;
            return false;
        }
    }

    A->nnz = 0;
    return true;
}

static void diagonal_free(struct matrix_t *A)
{
    if (!A->data)
        return;

    for (size_t i = 0; i < A->rows; i++) {
        elem_destroy_value(A->elem, A->data[i]);
        free(A->data[i]);
    }
    free(A->data);
    A->data = NULL;
    A->nnz = 0;
}

static void diagonal_get(const struct matrix_t *A, size_t i, size_t j, void *out)
{
    memcpy(out, (i == j) ? A->data[i] : A->elem->zero, A->elem->size);
}

static void diagonal_materialise(struct matrix_t *A)
{
    void **old_diagonal = A->data;
    size_t old_nnz = A->nnz;

    A->store = &dense_store;
    A->data = NULL;
    A->nnz = 0;
    if (!dense_alloc(A)) {
        A->store = &diagonal_store;
        A->data = old_diagonal;
        A->nnz = old_nnz;
        return;
    }

    for (size_t i = 0; i < A->rows; i++) {
        dense_set(A, i, i, old_diagonal[i]);
        if (!elem_is_structural_zero(A->elem, old_diagonal[i]))
            A->nnz++;
        elem_destroy_value(A->elem, old_diagonal[i]);
        free(old_diagonal[i]);
    }
    free(old_diagonal);
}

static void diagonal_set(struct matrix_t *A, size_t i, size_t j, const void *val)
{
    if (i == j) {
        int was_zero = elem_is_structural_zero(A->elem, A->data[i]);
        int is_zero = elem_is_structural_zero(A->elem, val);

        elem_destroy_value(A->elem, A->data[i]);
        elem_copy_value(A->elem, A->data[i], val);
        if (was_zero && !is_zero)
            A->nnz++;
        else if (!was_zero && is_zero)
            A->nnz--;
        return;
    }

    if (elem_is_structural_zero(A->elem, val))
        return;

    diagonal_materialise(A);
    dense_set(A, i, j, val);
    A->nnz++;
}

static void diagonal_swap_rows(struct matrix_t *A, size_t r1, size_t r2)
{
    diagonal_materialise(A);
    dense_swap_rows(A, r1, r2);
}

static void diagonal_row_eliminate_from(struct matrix_t *A, size_t dst_row, size_t src_row,
                                        size_t col_start, const void *factor)
{
    diagonal_materialise(A);
    dense_row_eliminate_from(A, dst_row, src_row, col_start, factor);
}

static const struct store_vtable diagonal_store = {
    .create                   = store_create_diagonal,
    .alloc                    = diagonal_alloc,
    .free                     = diagonal_free,
    .get                      = diagonal_get,
    .set                      = diagonal_set,
    .swap_rows                = diagonal_swap_rows,
    .row_eliminate_from       = diagonal_row_eliminate_from,
    .materialise              = diagonal_materialise,
    .is_sparse_storage        = diagonal_is_sparse_storage,
    .is_sparse_like           = diagonal_is_sparse_like,
    .is_diagonal              = store_true,
    .is_upper_triangular      = store_true,
    .is_lower_triangular      = store_true,
    .nonzero_count            = diagonal_nonzero_count,
    .elementwise_unary_store  = store_self_unary,
    .transpose_store          = store_self_transpose
};

/* ---------- triangular storage ---------- */

static size_t upper_triangular_row_width(const struct matrix_t *A, size_t row)
{
    return (row < A->cols) ? (A->cols - row) : 0;
}

static size_t lower_triangular_row_width(const struct matrix_t *A, size_t row)
{
    return (row < A->cols) ? (row + 1) : A->cols;
}

static bool triangular_alloc(struct matrix_t *A, size_t (*row_width)(const struct matrix_t *, size_t))
{
    size_t i;

    A->data = malloc(A->rows * sizeof(void *));
    if (!A->data)
        return false;

    for (i = 0; i < A->rows; i++) {
        size_t width = row_width(A, i);
        A->data[i] = width ? calloc(width, A->elem->size) : NULL;
        if (width && !A->data[i]) {
            while (i > 0) {
                i--;
                if (elem_is_dval(A->elem) && A->data[i]) {
                    size_t old_width = row_width(A, i);
                    for (size_t j = 0; j < old_width; j++) {
                        void *slot = (char *)A->data[i] + j * A->elem->size;
                        elem_destroy_value(A->elem, slot);
                    }
                }
                free(A->data[i]);
            }
            free(A->data);
            A->data = NULL;
            return false;
        }
    }

    A->nnz = 0;
    return true;
}

static void triangular_free(struct matrix_t *A)
{
    if (!A->data)
        return;

    for (size_t i = 0; i < A->rows; i++) {
        if (elem_is_dval(A->elem)) {
            size_t width = (A->store == &upper_triangular_store)
                         ? upper_triangular_row_width(A, i)
                         : lower_triangular_row_width(A, i);
            for (size_t j = 0; j < width; j++) {
                void *slot = (char *)A->data[i] + j * A->elem->size;
                elem_destroy_value(A->elem, slot);
            }
        }
        free(A->data[i]);
    }
    free(A->data);
    A->data = NULL;
    A->nnz = 0;
}

static bool upper_triangular_alloc(struct matrix_t *A)
{
    return triangular_alloc(A, upper_triangular_row_width);
}

static bool lower_triangular_alloc(struct matrix_t *A)
{
    return triangular_alloc(A, lower_triangular_row_width);
}

static void upper_triangular_get(const struct matrix_t *A, size_t i, size_t j, void *out)
{
    if (i <= j && i < A->cols) {
        memcpy(out,
               (char *)A->data[i] + (j - i) * A->elem->size,
               A->elem->size);
        return;
    }

    memcpy(out, A->elem->zero, A->elem->size);
}

static void lower_triangular_get(const struct matrix_t *A, size_t i, size_t j, void *out)
{
    if (j <= i && j < A->cols) {
        memcpy(out,
               (char *)A->data[i] + j * A->elem->size,
               A->elem->size);
        return;
    }

    memcpy(out, A->elem->zero, A->elem->size);
}

static void upper_triangular_materialise(struct matrix_t *A)
{
    void **old_rows = A->data;
    size_t old_nnz = A->nnz;

    A->store = &dense_store;
    A->data = NULL;
    A->nnz = 0;
    if (!dense_alloc(A)) {
        A->store = &upper_triangular_store;
        A->data = old_rows;
        A->nnz = old_nnz;
        return;
    }

    for (size_t i = 0; i < A->rows; i++) {
        size_t width = upper_triangular_row_width(A, i);
        for (size_t offset = 0; offset < width; offset++) {
            void *src = (char *)old_rows[i] + offset * A->elem->size;
            dense_set(A, i, i + offset, src);
            if (!elem_is_structural_zero(A->elem, src))
                A->nnz++;
            elem_destroy_value(A->elem, src);
        }
        free(old_rows[i]);
    }
    free(old_rows);
}

static void lower_triangular_materialise(struct matrix_t *A)
{
    void **old_rows = A->data;
    size_t old_nnz = A->nnz;

    A->store = &dense_store;
    A->data = NULL;
    A->nnz = 0;
    if (!dense_alloc(A)) {
        A->store = &lower_triangular_store;
        A->data = old_rows;
        A->nnz = old_nnz;
        return;
    }

    for (size_t i = 0; i < A->rows; i++) {
        size_t width = lower_triangular_row_width(A, i);
        for (size_t j = 0; j < width; j++) {
            void *src = (char *)old_rows[i] + j * A->elem->size;
            dense_set(A, i, j, src);
            if (!elem_is_structural_zero(A->elem, src))
                A->nnz++;
            elem_destroy_value(A->elem, src);
        }
        free(old_rows[i]);
    }
    free(old_rows);
}

static void upper_triangular_set(struct matrix_t *A, size_t i, size_t j, const void *val)
{
    if (i <= j && i < A->cols) {
        void *slot = (char *)A->data[i] + (j - i) * A->elem->size;
        int was_zero = elem_is_structural_zero(A->elem, slot);
        int is_zero = elem_is_structural_zero(A->elem, val);

        elem_destroy_value(A->elem, slot);
        elem_copy_value(A->elem, slot, val);
        if (was_zero && !is_zero)
            A->nnz++;
        else if (!was_zero && is_zero)
            A->nnz--;
        return;
    }

    if (elem_is_structural_zero(A->elem, val))
        return;

    upper_triangular_materialise(A);
    dense_set(A, i, j, val);
    A->nnz++;
}

static void lower_triangular_set(struct matrix_t *A, size_t i, size_t j, const void *val)
{
    if (j <= i && j < A->cols) {
        void *slot = (char *)A->data[i] + j * A->elem->size;
        int was_zero = elem_is_structural_zero(A->elem, slot);
        int is_zero = elem_is_structural_zero(A->elem, val);

        elem_destroy_value(A->elem, slot);
        elem_copy_value(A->elem, slot, val);
        if (was_zero && !is_zero)
            A->nnz++;
        else if (!was_zero && is_zero)
            A->nnz--;
        return;
    }

    if (elem_is_structural_zero(A->elem, val))
        return;

    lower_triangular_materialise(A);
    dense_set(A, i, j, val);
    A->nnz++;
}

static void upper_triangular_swap_rows(struct matrix_t *A, size_t r1, size_t r2)
{
    upper_triangular_materialise(A);
    dense_swap_rows(A, r1, r2);
}

static void lower_triangular_swap_rows(struct matrix_t *A, size_t r1, size_t r2)
{
    lower_triangular_materialise(A);
    dense_swap_rows(A, r1, r2);
}

static void upper_triangular_row_eliminate_from(struct matrix_t *A, size_t dst_row, size_t src_row,
                                                size_t col_start, const void *factor)
{
    unsigned char src[64], dst[64], prod[64], out[64];

    if (!A || !factor || dst_row == src_row)
        return;

    for (size_t j = col_start; j < A->cols; j++) {
        upper_triangular_get(A, dst_row, j, dst);
        upper_triangular_get(A, src_row, j, src);
        A->elem->mul(prod, factor, src);
        A->elem->sub(out, dst, prod);
        upper_triangular_set(A, dst_row, j, out);
    }
}

static void lower_triangular_row_eliminate_from(struct matrix_t *A, size_t dst_row, size_t src_row,
                                                size_t col_start, const void *factor)
{
    lower_triangular_materialise(A);
    dense_row_eliminate_from(A, dst_row, src_row, col_start, factor);
}

static const struct store_vtable upper_triangular_store = {
    .create                   = store_create_upper_triangular,
    .alloc                    = upper_triangular_alloc,
    .free                     = triangular_free,
    .get                      = upper_triangular_get,
    .set                      = upper_triangular_set,
    .swap_rows                = upper_triangular_swap_rows,
    .row_eliminate_from       = upper_triangular_row_eliminate_from,
    .materialise              = upper_triangular_materialise,
    .is_sparse_storage        = upper_triangular_is_sparse_storage,
    .is_sparse_like           = upper_triangular_is_sparse_like,
    .is_diagonal              = upper_triangular_is_diagonal,
    .is_upper_triangular      = store_true,
    .is_lower_triangular      = generic_is_lower_triangular,
    .nonzero_count            = upper_triangular_nonzero_count,
    .elementwise_unary_store  = store_self_unary,
    .transpose_store          = upper_triangular_transpose_store
};

static const struct store_vtable lower_triangular_store = {
    .create                   = store_create_lower_triangular,
    .alloc                    = lower_triangular_alloc,
    .free                     = triangular_free,
    .get                      = lower_triangular_get,
    .set                      = lower_triangular_set,
    .swap_rows                = lower_triangular_swap_rows,
    .row_eliminate_from       = lower_triangular_row_eliminate_from,
    .materialise              = lower_triangular_materialise,
    .is_sparse_storage        = lower_triangular_is_sparse_storage,
    .is_sparse_like           = lower_triangular_is_sparse_like,
    .is_diagonal              = lower_triangular_is_diagonal,
    .is_upper_triangular      = generic_is_upper_triangular,
    .is_lower_triangular      = store_true,
    .nonzero_count            = lower_triangular_nonzero_count,
    .elementwise_unary_store  = store_self_unary,
    .transpose_store          = lower_triangular_transpose_store
};

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
    A->nnz   = 0;

    if (!A->store->alloc(A)) {
        free(A);
        return NULL;
    }
    return A;
}

/* ============================================================
   Matrix construction policy helpers
   ============================================================ */

static struct matrix_t *store_create_dense(size_t rows, size_t cols,
                                           const struct elem_vtable *elem)
{
    return mat_create_internal(rows, cols, elem, &dense_store);
}

static struct matrix_t *store_create_sparse(size_t rows, size_t cols,
                                            const struct elem_vtable *elem)
{
    return mat_create_internal(rows, cols, elem, &sparse_store);
}

static struct matrix_t *store_create_identity(size_t rows, size_t cols,
                                              const struct elem_vtable *elem)
{
    if (rows != cols)
        return NULL;
    return mat_create_internal(rows, cols, elem, &identity_store);
}

static struct matrix_t *store_create_diagonal(size_t rows, size_t cols,
                                              const struct elem_vtable *elem)
{
    if (rows != cols)
        return NULL;
    return mat_create_internal(rows, cols, elem, &diagonal_store);
}

static struct matrix_t *store_create_upper_triangular(size_t rows, size_t cols,
                                                      const struct elem_vtable *elem)
{
    return mat_create_internal(rows, cols, elem, &upper_triangular_store);
}

static struct matrix_t *store_create_lower_triangular(size_t rows, size_t cols,
                                                      const struct elem_vtable *elem)
{
    return mat_create_internal(rows, cols, elem, &lower_triangular_store);
}

static bool store_false(const struct matrix_t *A)
{
    (void)A;
    return false;
}

static bool store_true(const struct matrix_t *A)
{
    return A != NULL;
}

static bool dense_is_sparse_storage(const struct matrix_t *A)
{
    return store_false(A);
}

static bool sparse_is_sparse_storage(const struct matrix_t *A)
{
    return store_true(A);
}

static bool identity_is_sparse_storage(const struct matrix_t *A)
{
    return store_false(A);
}

static bool diagonal_is_sparse_storage(const struct matrix_t *A)
{
    return store_false(A);
}

static bool upper_triangular_is_sparse_storage(const struct matrix_t *A)
{
    return store_false(A);
}

static bool lower_triangular_is_sparse_storage(const struct matrix_t *A)
{
    return store_false(A);
}

static bool dense_is_sparse_like(const struct matrix_t *A)
{
    return store_false(A);
}

static bool sparse_is_sparse_like(const struct matrix_t *A)
{
    return store_true(A);
}

static bool identity_is_sparse_like(const struct matrix_t *A)
{
    return store_true(A);
}

static bool diagonal_is_sparse_like(const struct matrix_t *A)
{
    return store_true(A);
}

static bool upper_triangular_is_sparse_like(const struct matrix_t *A)
{
    return store_false(A);
}

static bool lower_triangular_is_sparse_like(const struct matrix_t *A)
{
    return store_false(A);
}

static bool dense_is_diagonal(const struct matrix_t *A)
{
    unsigned char raw[64];

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            if (i == j)
                continue;
            dense_get(A, i, j, raw);
            if (!elem_is_structural_zero(A->elem, raw))
                return false;
        }

    return true;
}

static bool sparse_is_diagonal(const struct matrix_t *A)
{
    for (size_t i = 0; i < A->rows; i++) {
        sparse_entry_t *cur = A->data ? (sparse_entry_t *)A->data[i] : NULL;
        while (cur) {
            if (cur->col != i && !elem_is_structural_zero(A->elem, cur->value))
                return false;
            cur = cur->next;
        }
    }
    return true;
}

static bool upper_triangular_is_diagonal(const struct matrix_t *A)
{
    for (size_t i = 0; i < A->rows; i++) {
        size_t width = upper_triangular_row_width(A, i);
        for (size_t offset = 1; offset < width; offset++) {
            void *slot = (char *)A->data[i] + offset * A->elem->size;
            if (!elem_is_structural_zero(A->elem, slot))
                return false;
        }
    }
    return true;
}

static bool lower_triangular_is_diagonal(const struct matrix_t *A)
{
    for (size_t i = 0; i < A->rows; i++) {
        size_t width = lower_triangular_row_width(A, i);
        for (size_t j = 0; j + 1 < width; j++) {
            void *slot = (char *)A->data[i] + j * A->elem->size;
            if (!elem_is_structural_zero(A->elem, slot))
                return false;
        }
    }
    return true;
}

static bool generic_is_upper_triangular(const struct matrix_t *A)
{
    unsigned char raw[64];

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols && j < i; j++) {
            mat_get(A, i, j, raw);
            if (!elem_is_structural_zero(A->elem, raw))
                return false;
        }

    return true;
}

static bool generic_is_lower_triangular(const struct matrix_t *A)
{
    unsigned char raw[64];

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = i + 1; j < A->cols; j++) {
            mat_get(A, i, j, raw);
            if (!elem_is_structural_zero(A->elem, raw))
                return false;
        }

    return true;
}

static size_t dense_nonzero_count(const struct matrix_t *A)
{
    size_t count = 0;
    unsigned char raw[64];

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            dense_get(A, i, j, raw);
            if (!elem_is_structural_zero(A->elem, raw))
                count++;
        }

    return count;
}

static size_t sparse_nonzero_count(const struct matrix_t *A)
{
    return A->nnz;
}

static size_t identity_nonzero_count(const struct matrix_t *A)
{
    return A->rows;
}

static size_t diagonal_nonzero_count(const struct matrix_t *A)
{
    return A->nnz;
}

static size_t upper_triangular_nonzero_count(const struct matrix_t *A)
{
    return A->nnz;
}

static size_t lower_triangular_nonzero_count(const struct matrix_t *A)
{
    return A->nnz;
}

static const struct store_vtable *store_self_unary(const struct matrix_t *A)
{
    return A ? A->store : NULL;
}

static const struct store_vtable *identity_unary_store(const struct matrix_t *A)
{
    (void)A;
    return &diagonal_store;
}

static const struct store_vtable *store_self_transpose(const struct matrix_t *A)
{
    return A ? A->store : NULL;
}

static const struct store_vtable *upper_triangular_transpose_store(const struct matrix_t *A)
{
    (void)A;
    return &lower_triangular_store;
}

static const struct store_vtable *lower_triangular_transpose_store(const struct matrix_t *A)
{
    (void)A;
    return &upper_triangular_store;
}

static struct matrix_t *mat_create_with_store(size_t rows, size_t cols,
                                              const struct elem_vtable *elem,
                                              const struct store_vtable *store)
{
    if (!elem || !store || !store->create)
        return NULL;
    return store->create(rows, cols, elem);
}

struct matrix_t *mat_create_dense_with_elem(size_t rows, size_t cols,
                                            const struct elem_vtable *elem)
{
    return mat_create_with_store(rows, cols, elem, &dense_store);
}

struct matrix_t *mat_create_sparse_with_elem(size_t rows, size_t cols,
                                             const struct elem_vtable *elem)
{
    return mat_create_with_store(rows, cols, elem, &sparse_store);
}

struct matrix_t *mat_create_identity_with_elem(size_t n,
                                               const struct elem_vtable *elem)
{
    return mat_create_with_store(n, n, elem, &identity_store);
}

struct matrix_t *mat_create_diagonal_with_elem(size_t n,
                                               const struct elem_vtable *elem)
{
    return mat_create_with_store(n, n, elem, &diagonal_store);
}

struct matrix_t *mat_create_upper_triangular_with_elem(size_t rows, size_t cols,
                                                       const struct elem_vtable *elem)
{
    return mat_create_with_store(rows, cols, elem, &upper_triangular_store);
}

struct matrix_t *mat_create_lower_triangular_with_elem(size_t rows, size_t cols,
                                                       const struct elem_vtable *elem)
{
    return mat_create_with_store(rows, cols, elem, &lower_triangular_store);
}

static struct matrix_t *mat_create_elementwise_unary_result(size_t rows, size_t cols,
                                                            const struct elem_vtable *elem,
                                                            const struct matrix_t *layout_src)
{
    const struct store_vtable *store;

    if (!layout_src || !layout_src->store || !layout_src->store->elementwise_unary_store)
        return mat_create_dense_with_elem(rows, cols, elem);

    store = layout_src->store->elementwise_unary_store(layout_src);
    return mat_create_with_store(rows, cols, elem, store);
}

static struct matrix_t *mat_create_transpose_result(size_t rows, size_t cols,
                                                    const struct elem_vtable *elem,
                                                    const struct matrix_t *layout_src)
{
    const struct store_vtable *store;

    if (!layout_src || !layout_src->store || !layout_src->store->transpose_store)
        return mat_create_dense_with_elem(rows, cols, elem);

    store = layout_src->store->transpose_store(layout_src);
    return mat_create_with_store(rows, cols, elem, store);
}

static struct matrix_t *mat_create_binary_result(size_t rows, size_t cols,
                                                 const struct elem_vtable *elem,
                                                 const struct matrix_t *A,
                                                 const struct matrix_t *B)
{
    const struct store_vtable *store = &dense_store;

    if (rows == cols &&
        mat_has_diagonal_structure(A) && mat_has_diagonal_structure(B))
        store = &diagonal_store;
    else if ((mat_has_upper_triangular_structure(A) && mat_has_upper_triangular_structure(B)) ||
             (mat_has_diagonal_structure(A) && mat_has_upper_triangular_structure(B)) ||
             (mat_has_upper_triangular_structure(A) && mat_has_diagonal_structure(B)))
        store = &upper_triangular_store;
    else if ((mat_has_lower_triangular_structure(A) && mat_has_lower_triangular_structure(B)) ||
             (mat_has_diagonal_structure(A) && mat_has_lower_triangular_structure(B)) ||
             (mat_has_lower_triangular_structure(A) && mat_has_diagonal_structure(B)))
        store = &lower_triangular_store;
    else if (mat_is_sparse_like(A) && mat_is_sparse_like(B))
        store = &sparse_store;

    return mat_create_with_store(rows, cols, elem, store);
}

static bool mat_uses_sparse_storage(const struct matrix_t *A)
{
    return A && A->store && A->store->is_sparse_storage &&
           A->store->is_sparse_storage(A);
}

static bool mat_is_sparse_like(const struct matrix_t *A)
{
    return A && A->store && A->store->is_sparse_like &&
           A->store->is_sparse_like(A);
}

static bool mat_has_diagonal_structure(const struct matrix_t *A)
{
    return A && A->store && A->store->is_diagonal &&
           A->store->is_diagonal(A);
}

static bool mat_has_upper_triangular_structure(const struct matrix_t *A)
{
    return A && A->store && A->store->is_upper_triangular &&
           A->store->is_upper_triangular(A);
}

static bool mat_has_lower_triangular_structure(const struct matrix_t *A)
{
    return A && A->store && A->store->is_lower_triangular &&
           A->store->is_lower_triangular(A);
}

/* ============================================================
   Element vtables (double, qfloat, qcomplex)
   ============================================================ */

/* ---------- double ---------- */

static const double D_ZERO = 0.0;
static const double D_ONE  = 1.0;
static const qfloat_t D_REL_EPS = { 2.2204460492503131e-16, 0.0 };
static const qfloat_t Q_REL_EPS = { 1.0e-30, 0.0 };

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
static void d_scalar_erfinv(void *out, const void *a)
{
    *(double *)out = qf_to_double(qf_erfinv(qf_from_double(*(const double *)a)));
}
static void d_scalar_erfcinv(void *out, const void *a)
{
    *(double *)out = qf_to_double(qf_erfcinv(qf_from_double(*(const double *)a)));
}
static void d_scalar_gamma(void *out, const void *a) { *(double*)out = tgamma(*(const double*)a); }
static void d_scalar_lgamma(void *out, const void *a) { *(double*)out = lgamma(*(const double*)a); }
static void d_scalar_digamma(void *out, const void *a)
{
    *(double *)out = qf_to_double(qf_digamma(qf_from_double(*(const double *)a)));
}
static void d_scalar_trigamma(void *out, const void *a)
{
    *(double *)out = qf_to_double(qf_trigamma(qf_from_double(*(const double *)a)));
}
static void d_scalar_tetragamma(void *out, const void *a)
{
    *(double *)out = qf_to_double(qf_tetragamma(qf_from_double(*(const double *)a)));
}
static void d_scalar_gammainv(void *out, const void *a)
{
    *(double *)out = qf_to_double(qf_gammainv(qf_from_double(*(const double *)a)));
}
static void d_scalar_normal_pdf(void *out, const void *a)
{
    *(double *)out = qf_to_double(qf_normal_pdf(qf_from_double(*(const double *)a)));
}
static void d_scalar_normal_cdf(void *out, const void *a)
{
    *(double *)out = qf_to_double(qf_normal_cdf(qf_from_double(*(const double *)a)));
}
static void d_scalar_normal_logpdf(void *out, const void *a)
{
    *(double *)out = qf_to_double(qf_normal_logpdf(qf_from_double(*(const double *)a)));
}
static void d_scalar_lambert_w0(void *out, const void *a)
{
    *(double *)out = qf_to_double(qf_lambert_w0(qf_from_double(*(const double *)a)));
}
static void d_scalar_lambert_wm1(void *out, const void *a)
{
    *(double *)out = qf_to_double(qf_lambert_wm1(qf_from_double(*(const double *)a)));
}
static void d_scalar_productlog(void *out, const void *a)
{
    *(double *)out = qf_to_double(qf_productlog(qf_from_double(*(const double *)a)));
}
static void d_scalar_ei(void *out, const void *a)
{
    *(double *)out = qf_to_double(qf_ei(qf_from_double(*(const double *)a)));
}
static void d_scalar_e1(void *out, const void *a)
{
    *(double *)out = qf_to_double(qf_e1(qf_from_double(*(const double *)a)));
}

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
    .erfc = d_scalar_erfc,
    .erfinv = d_scalar_erfinv,
    .erfcinv = d_scalar_erfcinv,
    .gamma = d_scalar_gamma,
    .lgamma = d_scalar_lgamma,
    .digamma = d_scalar_digamma,
    .trigamma = d_scalar_trigamma,
    .tetragamma = d_scalar_tetragamma,
    .gammainv = d_scalar_gammainv,
    .normal_pdf = d_scalar_normal_pdf,
    .normal_cdf = d_scalar_normal_cdf,
    .normal_logpdf = d_scalar_normal_logpdf,
    .lambert_w0 = d_scalar_lambert_w0,
    .lambert_wm1 = d_scalar_lambert_wm1,
    .productlog = d_scalar_productlog,
    .ei = d_scalar_ei,
    .e1 = d_scalar_e1
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
    .relative_epsilon = D_REL_EPS,
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
static void qf_scalar_erfinv(void *out, const void *a) { *(qfloat_t*)out = qf_erfinv(*(const qfloat_t*)a); }
static void qf_scalar_erfcinv(void *out, const void *a) { *(qfloat_t*)out = qf_erfcinv(*(const qfloat_t*)a); }
static void qf_scalar_gamma(void *out, const void *a) { *(qfloat_t*)out = qf_gamma(*(const qfloat_t*)a); }
static void qf_scalar_lgamma(void *out, const void *a) { *(qfloat_t*)out = qf_lgamma(*(const qfloat_t*)a); }
static void qf_scalar_digamma(void *out, const void *a) { *(qfloat_t*)out = qf_digamma(*(const qfloat_t*)a); }
static void qf_scalar_trigamma(void *out, const void *a) { *(qfloat_t*)out = qf_trigamma(*(const qfloat_t*)a); }
static void qf_scalar_tetragamma(void *out, const void *a) { *(qfloat_t*)out = qf_tetragamma(*(const qfloat_t*)a); }
static void qf_scalar_gammainv(void *out, const void *a) { *(qfloat_t*)out = qf_gammainv(*(const qfloat_t*)a); }
static void qf_scalar_normal_pdf(void *out, const void *a) { *(qfloat_t*)out = qf_normal_pdf(*(const qfloat_t*)a); }
static void qf_scalar_normal_cdf(void *out, const void *a) { *(qfloat_t*)out = qf_normal_cdf(*(const qfloat_t*)a); }
static void qf_scalar_normal_logpdf(void *out, const void *a) { *(qfloat_t*)out = qf_normal_logpdf(*(const qfloat_t*)a); }
static void qf_scalar_lambert_w0(void *out, const void *a) { *(qfloat_t*)out = qf_lambert_w0(*(const qfloat_t*)a); }
static void qf_scalar_lambert_wm1(void *out, const void *a) { *(qfloat_t*)out = qf_lambert_wm1(*(const qfloat_t*)a); }
static void qf_scalar_productlog(void *out, const void *a) { *(qfloat_t*)out = qf_productlog(*(const qfloat_t*)a); }
static void qf_scalar_ei(void *out, const void *a) { *(qfloat_t*)out = qf_ei(*(const qfloat_t*)a); }
static void qf_scalar_e1(void *out, const void *a) { *(qfloat_t*)out = qf_e1(*(const qfloat_t*)a); }

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
    .erfc = qf_scalar_erfc,
    .erfinv = qf_scalar_erfinv,
    .erfcinv = qf_scalar_erfcinv,
    .gamma = qf_scalar_gamma,
    .lgamma = qf_scalar_lgamma,
    .digamma = qf_scalar_digamma,
    .trigamma = qf_scalar_trigamma,
    .tetragamma = qf_scalar_tetragamma,
    .gammainv = qf_scalar_gammainv,
    .normal_pdf = qf_scalar_normal_pdf,
    .normal_cdf = qf_scalar_normal_cdf,
    .normal_logpdf = qf_scalar_normal_logpdf,
    .lambert_w0 = qf_scalar_lambert_w0,
    .lambert_wm1 = qf_scalar_lambert_wm1,
    .productlog = qf_scalar_productlog,
    .ei = qf_scalar_ei,
    .e1 = qf_scalar_e1
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
    .relative_epsilon = Q_REL_EPS,
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
static void qc_scalar_erfinv(void *out, const void *a) { *(qcomplex_t*)out = qc_erfinv(*(const qcomplex_t*)a); }
static void qc_scalar_erfcinv(void *out, const void *a) { *(qcomplex_t*)out = qc_erfcinv(*(const qcomplex_t*)a); }
static void qc_scalar_gamma(void *out, const void *a) { *(qcomplex_t*)out = qc_gamma(*(const qcomplex_t*)a); }
static void qc_scalar_lgamma(void *out, const void *a) { *(qcomplex_t*)out = qc_lgamma(*(const qcomplex_t*)a); }
static void qc_scalar_digamma(void *out, const void *a) { *(qcomplex_t*)out = qc_digamma(*(const qcomplex_t*)a); }
static void qc_scalar_trigamma(void *out, const void *a) { *(qcomplex_t*)out = qc_trigamma(*(const qcomplex_t*)a); }
static void qc_scalar_tetragamma(void *out, const void *a) { *(qcomplex_t*)out = qc_tetragamma(*(const qcomplex_t*)a); }
static void qc_scalar_gammainv(void *out, const void *a) { *(qcomplex_t*)out = qc_gammainv(*(const qcomplex_t*)a); }
static void qc_scalar_normal_pdf(void *out, const void *a) { *(qcomplex_t*)out = qc_normal_pdf(*(const qcomplex_t*)a); }
static void qc_scalar_normal_cdf(void *out, const void *a) { *(qcomplex_t*)out = qc_normal_cdf(*(const qcomplex_t*)a); }
static void qc_scalar_normal_logpdf(void *out, const void *a) { *(qcomplex_t*)out = qc_normal_logpdf(*(const qcomplex_t*)a); }
static void qc_scalar_lambert_w0(void *out, const void *a) { *(qcomplex_t*)out = qc_productlog(*(const qcomplex_t*)a); }
static void qc_scalar_lambert_wm1(void *out, const void *a) { *(qcomplex_t*)out = qc_lambert_wm1(*(const qcomplex_t*)a); }
static void qc_scalar_productlog(void *out, const void *a) { *(qcomplex_t*)out = qc_productlog(*(const qcomplex_t*)a); }
static void qc_scalar_ei(void *out, const void *a) { *(qcomplex_t*)out = qc_ei(*(const qcomplex_t*)a); }
static void qc_scalar_e1(void *out, const void *a) { *(qcomplex_t*)out = qc_e1(*(const qcomplex_t*)a); }

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
    .erfc = qc_scalar_erfc,
    .erfinv = qc_scalar_erfinv,
    .erfcinv = qc_scalar_erfcinv,
    .gamma = qc_scalar_gamma,
    .lgamma = qc_scalar_lgamma,
    .digamma = qc_scalar_digamma,
    .trigamma = qc_scalar_trigamma,
    .tetragamma = qc_scalar_tetragamma,
    .gammainv = qc_scalar_gammainv,
    .normal_pdf = qc_scalar_normal_pdf,
    .normal_cdf = qc_scalar_normal_cdf,
    .normal_logpdf = qc_scalar_normal_logpdf,
    .lambert_w0 = qc_scalar_lambert_w0,
    .lambert_wm1 = qc_scalar_lambert_wm1,
    .productlog = qc_scalar_productlog,
    .ei = qc_scalar_ei,
    .e1 = qc_scalar_e1
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
    .relative_epsilon = Q_REL_EPS,
    .fun             = &qcomplex_fun
};

/* ---------- dval_t* ---------- */

static void dv_add_wrap(void *o, const void *a, const void *b)
{
    dval_t *lhs = *(dval_t *const *)a;
    dval_t *rhs = *(dval_t *const *)b;
    dval_t *prev = *(dval_t **)o;
    dval_t *res = dv_add(lhs, rhs);

    if (prev)
        dv_free(prev);
    *(dval_t **)o = res;
}

static void dv_sub_wrap(void *o, const void *a, const void *b)
{
    dval_t *lhs = *(dval_t *const *)a;
    dval_t *rhs = *(dval_t *const *)b;
    dval_t *prev = *(dval_t **)o;
    dval_t *res = dv_sub(lhs, rhs);

    if (prev)
        dv_free(prev);
    *(dval_t **)o = res;
}

static void dv_mul_wrap(void *o, const void *a, const void *b)
{
    dval_t *lhs = *(dval_t *const *)a;
    dval_t *rhs = *(dval_t *const *)b;
    dval_t *prev = *(dval_t **)o;
    dval_t *res = dv_mul(lhs, rhs);

    if (prev)
        dv_free(prev);
    *(dval_t **)o = res;
}

static void dv_inv_wrap(void *o, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)o;
    dval_t *res = dv_d_div(1.0, arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)o = res;
}

static int dval_cmp_wrap(const void *a, const void *b)
{
    dval_t *lhs = *(dval_t *const *)a;
    dval_t *rhs = *(dval_t *const *)b;

    if (!lhs && !rhs)
        return 0;
    if (!lhs)
        return -1;
    if (!rhs)
        return 1;
    return dv_cmp(lhs, rhs);
}

static void dv_print_wrap(const void *v, char *buf, size_t n)
{
    dval_t *dv = *(dval_t *const *)v;
    char *tmp;
    char *inner;
    char *sep;
    size_t len;

    if (!dv) {
        snprintf(buf, n, "NULL");
        return;
    }

    tmp = dv_to_string(dv, style_EXPRESSION);
    if (!tmp) {
        snprintf(buf, n, "<dval>");
        return;
    }

    /* Matrix entries read better without repeating the full binding block for
     * every cell, so prefer the compact expression body when available. */
    inner = tmp;
    len = strlen(tmp);
    if (len >= 4 && tmp[0] == '{' && tmp[1] == ' ' && tmp[len - 2] == ' ' && tmp[len - 1] == '}') {
        inner = tmp + 2;
        tmp[len - 2] = '\0';
        sep = strstr(inner, " | ");
        if (sep)
            *sep = '\0';
    }

    snprintf(buf, n, "%s", inner);
    free(tmp);
}

static double dv_abs2_wrap(const void *a)
{
    dval_t *dv = *(dval_t *const *)a;
    double x = dv ? dv_eval_d(dv) : 0.0;
    return x * x;
}

static double dv_to_real_wrap(const void *a)
{
    dval_t *dv = *(dval_t *const *)a;
    return dv ? dv_eval_d(dv) : 0.0;
}

static void dv_from_real_wrap(void *o, double x)
{
    dval_t *prev = *(dval_t **)o;
    dval_t *res = dv_new_const_d(x);

    if (prev)
        dv_free(prev);
    *(dval_t **)o = res;
}

static void dv_conj_elem(void *o, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)o;

    if (prev)
        dv_free(prev);
    if (arg)
        dv_retain(arg);
    *(dval_t **)o = arg;
}

static void dv_to_qf(qfloat_t *o, const void *a)
{
    dval_t *dv = *(dval_t *const *)a;
    *o = dv ? dv_get_val_qf(dv) : QF_ZERO;
}

static void dv_abs_qf(qfloat_t *o, const void *a)
{
    dval_t *dv = *(dval_t *const *)a;
    *o = dv ? qc_abs(dv_get_val(dv)) : QF_ZERO;
}

static void dv_from_qf(void *o, const qfloat_t *x)
{
    dval_t *prev = *(dval_t **)o;
    dval_t *res = dv_new_const(*x);

    if (prev)
        dv_free(prev);
    *(dval_t **)o = res;
}

static void dv_to_qc_fn(qcomplex_t *o, const void *a)
{
    dval_t *dv = *(dval_t *const *)a;
    *o = dv ? dv_get_val(dv) : QC_ZERO;
}

static void dv_from_qc_fn(void *o, const qcomplex_t *z)
{
    dval_t *prev = *(dval_t **)o;
    dval_t *res = dv_new_const(z->re);

    if (prev)
        dv_free(prev);
    *(dval_t **)o = res;
}

static void dv_scalar_exp(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_exp(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_log(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_log(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_sin(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_sin(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_cos(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_cos(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_tan(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_tan(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_sinh(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_sinh(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_cosh(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_cosh(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_tanh(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_tanh(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_sqrt(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_sqrt(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_asin(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_asin(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_acos(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_acos(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_atan(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_atan(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_asinh(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_asinh(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_acosh(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_acosh(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_atanh(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_atanh(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_erf(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_erf(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_erfc(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_erfc(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_erfinv(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_erfinv(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_erfcinv(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_erfcinv(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_gamma(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_gamma(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_lgamma(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_lgamma(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_digamma(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_digamma(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_trigamma(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_trigamma(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_normal_pdf(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_normal_pdf(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_normal_cdf(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_normal_cdf(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_normal_logpdf(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_normal_logpdf(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_lambert_w0(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_lambert_w0(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_lambert_wm1(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_lambert_wm1(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_ei(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_ei(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static void dv_scalar_e1(void *out, const void *a)
{
    dval_t *arg = *(dval_t *const *)a;
    dval_t *prev = *(dval_t **)out;
    dval_t *res = dv_e1(arg);

    if (prev)
        dv_free(prev);
    *(dval_t **)out = res;
}

static const struct elem_fun_vtable dval_fun = {
    .exp = dv_scalar_exp,
    .sin = dv_scalar_sin,
    .cos = dv_scalar_cos,
    .tan = dv_scalar_tan,
    .sinh = dv_scalar_sinh,
    .cosh = dv_scalar_cosh,
    .tanh = dv_scalar_tanh,
    .sqrt = dv_scalar_sqrt,
    .log = dv_scalar_log,
    .asin = dv_scalar_asin,
    .acos = dv_scalar_acos,
    .atan = dv_scalar_atan,
    .asinh = dv_scalar_asinh,
    .acosh = dv_scalar_acosh,
    .atanh = dv_scalar_atanh,
    .erf = dv_scalar_erf,
    .erfc = dv_scalar_erfc,
    .erfinv = dv_scalar_erfinv,
    .erfcinv = dv_scalar_erfcinv,
    .gamma = dv_scalar_gamma,
    .lgamma = dv_scalar_lgamma,
    .digamma = dv_scalar_digamma,
    .trigamma = dv_scalar_trigamma,
    .tetragamma = NULL,
    .gammainv = NULL,
    .normal_pdf = dv_scalar_normal_pdf,
    .normal_cdf = dv_scalar_normal_cdf,
    .normal_logpdf = dv_scalar_normal_logpdf,
    .lambert_w0 = dv_scalar_lambert_w0,
    .lambert_wm1 = dv_scalar_lambert_wm1,
    .productlog = NULL,
    .ei = dv_scalar_ei,
    .e1 = dv_scalar_e1
};

const struct elem_vtable dval_elem = {
    .size            = sizeof(dval_t *),
    .kind            = ELEM_DVAL,
    .public_type     = MAT_TYPE_DVAL,
    .add             = dv_add_wrap,
    .sub             = dv_sub_wrap,
    .mul             = dv_mul_wrap,
    .inv             = dv_inv_wrap,
    .abs2            = dv_abs2_wrap,
    .to_real         = dv_to_real_wrap,
    .from_real       = dv_from_real_wrap,
    .conj_elem       = dv_conj_elem,
    .to_qf           = dv_to_qf,
    .abs_qf          = dv_abs_qf,
    .from_qf         = dv_from_qf,
    .to_qc           = dv_to_qc_fn,
    .from_qc         = dv_from_qc_fn,
    .zero            = &DV_ZERO,
    .one             = &DV_ONE,
    .cmp             = dval_cmp_wrap,
    .print           = dv_print_wrap,
    .relative_epsilon = Q_REL_EPS,
    .fun             = &dval_fun
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

static inline void d_as_dv(dval_t **out, const double *a) {
    *out = dv_new_const_d(*a);
}

static inline void qf_as_dv(dval_t **out, const qfloat_t *a) {
    *out = dv_new_const(*a);
}

static inline void id_dv(dval_t **out, dval_t *const *a) {
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

/* ---- double <-> dval ---- */

static void add_d_dv(void *out, const void *a, const void *b)
{
    dval_t *A, *B;
    d_as_dv(&A, (const double *)a);
    id_dv(&B, (dval_t *const *)b);
    *(dval_t **)out = dv_add(A, B);
    dv_free(A);
}

static void add_dv_d(void *out, const void *a, const void *b)
{
    dval_t *A, *B;
    id_dv(&A, (dval_t *const *)a);
    d_as_dv(&B, (const double *)b);
    *(dval_t **)out = dv_add(A, B);
    dv_free(B);
}

static void sub_d_dv(void *out, const void *a, const void *b)
{
    dval_t *A, *B;
    d_as_dv(&A, (const double *)a);
    id_dv(&B, (dval_t *const *)b);
    *(dval_t **)out = dv_sub(A, B);
    dv_free(A);
}

static void sub_dv_d(void *out, const void *a, const void *b)
{
    dval_t *A, *B;
    id_dv(&A, (dval_t *const *)a);
    d_as_dv(&B, (const double *)b);
    *(dval_t **)out = dv_sub(A, B);
    dv_free(B);
}

static void mul_d_dv(void *out, const void *a, const void *b)
{
    dval_t *A, *B;
    d_as_dv(&A, (const double *)a);
    id_dv(&B, (dval_t *const *)b);
    *(dval_t **)out = dv_mul(A, B);
    dv_free(A);
}

static void mul_dv_d(void *out, const void *a, const void *b)
{
    dval_t *A, *B;
    id_dv(&A, (dval_t *const *)a);
    d_as_dv(&B, (const double *)b);
    *(dval_t **)out = dv_mul(A, B);
    dv_free(B);
}

/* ---- qfloat <-> dval ---- */

static void add_qf_dv(void *out, const void *a, const void *b)
{
    dval_t *A, *B;
    qf_as_dv(&A, (const qfloat_t *)a);
    id_dv(&B, (dval_t *const *)b);
    *(dval_t **)out = dv_add(A, B);
    dv_free(A);
}

static void add_dv_qf(void *out, const void *a, const void *b)
{
    dval_t *A, *B;
    id_dv(&A, (dval_t *const *)a);
    qf_as_dv(&B, (const qfloat_t *)b);
    *(dval_t **)out = dv_add(A, B);
    dv_free(B);
}

static void sub_qf_dv(void *out, const void *a, const void *b)
{
    dval_t *A, *B;
    qf_as_dv(&A, (const qfloat_t *)a);
    id_dv(&B, (dval_t *const *)b);
    *(dval_t **)out = dv_sub(A, B);
    dv_free(A);
}

static void sub_dv_qf(void *out, const void *a, const void *b)
{
    dval_t *A, *B;
    id_dv(&A, (dval_t *const *)a);
    qf_as_dv(&B, (const qfloat_t *)b);
    *(dval_t **)out = dv_sub(A, B);
    dv_free(B);
}

static void mul_qf_dv(void *out, const void *a, const void *b)
{
    dval_t *A, *B;
    qf_as_dv(&A, (const qfloat_t *)a);
    id_dv(&B, (dval_t *const *)b);
    *(dval_t **)out = dv_mul(A, B);
    dv_free(A);
}

static void mul_dv_qf(void *out, const void *a, const void *b)
{
    dval_t *A, *B;
    id_dv(&A, (dval_t *const *)a);
    qf_as_dv(&B, (const qfloat_t *)b);
    *(dval_t **)out = dv_mul(A, B);
    dv_free(B);
}

/* ---- dval <-> dval ---- */

static void add_dv_dv(void *out, const void *a, const void *b)
{
    *(dval_t **)out = dv_add(*(dval_t *const *)a, *(dval_t *const *)b);
}

static void sub_dv_dv(void *out, const void *a, const void *b)
{
    *(dval_t **)out = dv_sub(*(dval_t *const *)a, *(dval_t *const *)b);
}

static void mul_dv_dv(void *out, const void *a, const void *b)
{
    *(dval_t **)out = dv_mul(*(dval_t *const *)a, *(dval_t *const *)b);
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
        },
        /* double op dval -> dval */
        [ELEM_DVAL] = {
            .result_elem = &dval_elem,
            .add = add_d_dv,
            .sub = sub_d_dv,
            .mul = mul_d_dv
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
        },
        /* qfloat op dval -> dval */
        [ELEM_DVAL] = {
            .result_elem = &dval_elem,
            .add = add_qf_dv,
            .sub = sub_qf_dv,
            .mul = mul_qf_dv
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
    },

    [ELEM_DVAL] = {
        /* dval op double -> dval */
        [ELEM_DOUBLE] = {
            .result_elem = &dval_elem,
            .add = add_dv_d,
            .sub = sub_dv_d,
            .mul = mul_dv_d
        },
        /* dval op qfloat -> dval */
        [ELEM_QFLOAT] = {
            .result_elem = &dval_elem,
            .add = add_dv_qf,
            .sub = sub_dv_qf,
            .mul = mul_dv_qf
        },
        /* dval op dval -> dval */
        [ELEM_DVAL] = {
            .result_elem = &dval_elem,
            .add = add_dv_dv,
            .sub = sub_dv_dv,
            .mul = mul_dv_dv
        }
    }
};

/* ============================================================
   Public API constructors (preserve old names)
   ============================================================ */

struct matrix_t *mat_new_d(size_t rows, size_t cols) {
    return mat_create_dense_with_elem(rows, cols, &double_elem);
}

struct matrix_t *mat_new_sparse_d(size_t rows, size_t cols) {
    return mat_create_sparse_with_elem(rows, cols, &double_elem);
}

struct matrix_t *mat_new_qf(size_t rows, size_t cols) {
    return mat_create_dense_with_elem(rows, cols, &qfloat_elem);
}

struct matrix_t *mat_new_sparse_qf(size_t rows, size_t cols) {
    return mat_create_sparse_with_elem(rows, cols, &qfloat_elem);
}

struct matrix_t *mat_new_qc(size_t rows, size_t cols) {
    return mat_create_dense_with_elem(rows, cols, &qcomplex_elem);
}

struct matrix_t *mat_new_sparse_qc(size_t rows, size_t cols) {
    return mat_create_sparse_with_elem(rows, cols, &qcomplex_elem);
}

struct matrix_t *mat_new_dv(size_t rows, size_t cols) {
    return mat_create_dense_with_elem(rows, cols, &dval_elem);
}

struct matrix_t *mat_new_sparse_dv(size_t rows, size_t cols) {
    return mat_create_sparse_with_elem(rows, cols, &dval_elem);
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

struct matrix_t *matsq_new_dv(size_t n) {
    return mat_new_dv(n, n);
}

struct matrix_t *mat_create_identity_d(size_t n) {
    return mat_create_identity_with_elem(n, &double_elem);
}

struct matrix_t *mat_create_identity_qf(size_t n) {
    return mat_create_identity_with_elem(n, &qfloat_elem);
}

struct matrix_t *mat_create_identity_qc(size_t n) {
    return mat_create_identity_with_elem(n, &qcomplex_elem);
}

struct matrix_t *mat_create_identity_dv(size_t n) {
    return mat_create_identity_with_elem(n, &dval_elem);
}

static matrix_t *mat_create_diagonal_from_array(size_t n,
                                                const void *diagonal,
                                                const struct elem_vtable *elem)
{
    matrix_t *A;
    const unsigned char *cursor = diagonal;

    if (!diagonal)
        return NULL;

    A = mat_create_diagonal_with_elem(n, elem);
    if (!A)
        return NULL;

    for (size_t i = 0; i < n; i++) {
        mat_set(A, i, i, cursor);
        cursor += elem->size;
    }

    return A;
}

matrix_t *mat_create_diagonal_d(size_t n, const double *diagonal)
{
    return mat_create_diagonal_from_array(n, diagonal, &double_elem);
}

matrix_t *mat_create_diagonal_qf(size_t n, const qfloat_t *diagonal)
{
    return mat_create_diagonal_from_array(n, diagonal, &qfloat_elem);
}

matrix_t *mat_create_diagonal_qc(size_t n, const qcomplex_t *diagonal)
{
    return mat_create_diagonal_from_array(n, diagonal, &qcomplex_elem);
}

matrix_t *mat_create_diagonal_dv(size_t n, dval_t *const *diagonal)
{
    return mat_create_diagonal_from_array(n, diagonal, &dval_elem);
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

matrix_t *mat_create_dv(size_t rows, size_t cols, dval_t *const *data)
{
    matrix_t *A = mat_new_dv(rows, cols);
    mat_set_data(A, data);
    return A;
}

/* ============================================================
    Destruction and basic access
    ============================================================ */

void mat_free(struct matrix_t *A) {
    if (!A) return;
    mat_fun_cache_forget(A);
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

bool mat_is_sparse(const matrix_t *A)
{
    return mat_uses_sparse_storage(A);
}

bool mat_is_diagonal(const matrix_t *A)
{
    return mat_has_diagonal_structure(A);
}

bool mat_is_upper_triangular(const matrix_t *A)
{
    return mat_has_upper_triangular_structure(A);
}

bool mat_is_lower_triangular(const matrix_t *A)
{
    return mat_has_lower_triangular_structure(A);
}

size_t mat_nonzero_count(const matrix_t *A)
{
    if (!A)
        return 0;
    return A->store->nonzero_count ? A->store->nonzero_count(A) : 0;
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

matrix_t *mat_to_dense(const matrix_t *A)
{
    return mat_convert_dense(A, A ? A->elem : NULL);
}

matrix_t *mat_to_sparse(const matrix_t *A)
{
    matrix_t *S;
    unsigned char raw[64] = {0};

    if (!A)
        return NULL;

    S = mat_create_sparse_with_elem(A->rows, A->cols, A->elem);

    if (!S)
        return NULL;

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, raw);
            if (!elem_is_structural_zero(A->elem, raw))
                mat_set(S, i, j, raw);
        }

    return S;
}

matrix_t *mat_evaluate_qf(const matrix_t *A)
{
    return mat_convert_preserving_store(A, A ? &qfloat_elem : NULL);
}

matrix_t *mat_evaluate_qc(const matrix_t *A)
{
    return mat_convert_preserving_store(A, A ? &qcomplex_elem : NULL);
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
    matrix_t *R = mat_create_elementwise_unary_result(A->rows, A->cols, re, A);
    if (!R) return NULL;

    unsigned char scalar_raw[64] = {0};
    unsigned char a_raw[64] = {0};
    unsigned char out_raw[64] = {0};

    /* encode scalar as a double (left operand type) */
    memcpy(scalar_raw, &s, sizeof(double));

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a_raw);
            op->mul(out_raw, scalar_raw, a_raw);
            mat_set(R, i, j, out_raw);
            elem_destroy_value(re, out_raw);
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
    matrix_t *R = mat_create_elementwise_unary_result(A->rows, A->cols, re, A);
    if (!R) return NULL;

    unsigned char scalar_raw[64] = {0};
    unsigned char a_raw[64] = {0};
    unsigned char out_raw[64] = {0};

    /* encode scalar as a qfloat_t (left operand type) */
    memcpy(scalar_raw, &s, sizeof(qfloat_t));

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a_raw);
            op->mul(out_raw, scalar_raw, a_raw);
            mat_set(R, i, j, out_raw);
            elem_destroy_value(re, out_raw);
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
    matrix_t *R = mat_create_elementwise_unary_result(A->rows, A->cols, re, A);
    if (!R) return NULL;

    unsigned char scalar_raw[64] = {0};
    unsigned char a_raw[64] = {0};
    unsigned char out_raw[64] = {0};

    /* encode scalar as a qcomplex_t (left operand type) */
    memcpy(scalar_raw, &s, sizeof(qcomplex_t));

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a_raw);
            op->mul(out_raw, scalar_raw, a_raw);
            mat_set(R, i, j, out_raw);
            elem_destroy_value(re, out_raw);
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

static struct matrix_t *mat_add_or_sub_sparse(const struct matrix_t *A,
                                              const struct matrix_t *B,
                                              const binop_vtable *op,
                                              int is_sub)
{
    const struct elem_vtable *re = op->result_elem;
    struct matrix_t *C = mat_create_sparse_with_elem(A->rows, A->cols, re);
    unsigned char a_raw[64] = {0}, b_raw[64] = {0}, out[64] = {0};

    if (!C)
        return NULL;

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a_raw);
            mat_get(B, i, j, b_raw);
            if (is_sub)
                op->sub(out, a_raw, b_raw);
            else
                op->add(out, a_raw, b_raw);
            elem_simplify_value(re, out);
            mat_set(C, i, j, out);
            elem_destroy_value(re, out);
        }

    return C;
}

static struct matrix_t *mat_mul_sparse(const struct matrix_t *A,
                                       const struct matrix_t *B,
                                       const binop_vtable *op)
{
    const struct elem_vtable *re = op->result_elem;
    struct matrix_t *As = NULL, *Bs = NULL, *C = NULL;
    unsigned char x_raw[64] = {0}, y_raw[64] = {0}, prod[64] = {0};
    unsigned char sum[64] = {0}, sum_acc[64] = {0};

    if (!re)
        return NULL;

    As = mat_uses_sparse_storage(A) ? (struct matrix_t *)A : mat_to_sparse(A);
    Bs = mat_uses_sparse_storage(B) ? (struct matrix_t *)B : mat_to_sparse(B);
    if (!As || !Bs) {
        if (As != A)
            mat_free(As);
        if (Bs != B)
            mat_free(Bs);
        return NULL;
    }

    C = mat_create_sparse_with_elem(A->rows, B->cols, re);
    if (!C) {
        if (As != A)
            mat_free(As);
        if (Bs != B)
            mat_free(Bs);
        return NULL;
    }

    for (size_t i = 0; i < As->rows; i++) {
        sparse_entry_t *a_cur = As->data ? (sparse_entry_t *)As->data[i] : NULL;

        while (a_cur) {
            size_t k = a_cur->col;
            sparse_entry_t *b_cur = Bs->data ? (sparse_entry_t *)Bs->data[k] : NULL;

            memcpy(x_raw, a_cur->value, As->elem->size);
            while (b_cur) {
                mat_get(C, i, b_cur->col, sum);
                elem_copy_value(re, sum_acc, sum);
                memcpy(y_raw, b_cur->value, Bs->elem->size);
                op->mul(prod, x_raw, y_raw);
                re->add(sum_acc, sum_acc, prod);
                elem_simplify_value(re, sum_acc);
                mat_set(C, i, b_cur->col, sum_acc);
                elem_destroy_value(re, sum_acc);
                elem_destroy_value(re, prod);
                b_cur = b_cur->next;
            }

            a_cur = a_cur->next;
        }
    }

    if (As != A)
        mat_free(As);
    if (Bs != B)
        mat_free(Bs);
    return C;
}

struct matrix_t *mat_add(const struct matrix_t *A, const struct matrix_t *B) {
    if (!A || !B || A->rows != B->rows || A->cols != B->cols)
        return NULL;

    elem_kind ak = elem_of(A)->kind;
    elem_kind bk = elem_of(B)->kind;

    const binop_vtable *op = &binops[ak][bk];
    if (!op->add || !op->result_elem)
        return NULL;

    if (mat_is_sparse_like(A) && mat_is_sparse_like(B))
        return mat_add_or_sub_sparse(A, B, op, 0);

    const struct elem_vtable *re = op->result_elem;
    struct matrix_t *C = mat_create_binary_result(A->rows, A->cols, re, A, B);
    if (!C) return NULL;

    unsigned char a_raw[64] = {0}, b_raw[64] = {0}, out[64] = {0};

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a_raw);
            mat_get(B, i, j, b_raw);
            op->add(out, a_raw, b_raw);
            elem_simplify_value(re, out);
            mat_set(C, i, j, out);
            elem_destroy_value(re, out);
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

    if (mat_is_sparse_like(A) && mat_is_sparse_like(B))
        return mat_add_or_sub_sparse(A, B, op, 1);

    const struct elem_vtable *re = op->result_elem;
    struct matrix_t *C = mat_create_binary_result(A->rows, A->cols, re, A, B);
    if (!C) return NULL;

    unsigned char a_raw[64] = {0}, b_raw[64] = {0}, out[64] = {0};

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a_raw);
            mat_get(B, i, j, b_raw);
            op->sub(out, a_raw, b_raw);
            elem_simplify_value(re, out);
            mat_set(C, i, j, out);
            elem_destroy_value(re, out);
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

    if (mat_is_sparse_like(A) && mat_is_sparse_like(B))
        return mat_mul_sparse(A, B, op);

    const struct elem_vtable *re = op->result_elem;
    struct matrix_t *C = mat_create_binary_result(A->rows, B->cols, re, A, B);
    if (!C) return NULL;

    unsigned char x_raw[64] = {0}, y_raw[64] = {0}, prod[64] = {0}, sum[64] = {0};

    for (size_t i = 0; i < A->rows; i++) {
        for (size_t j = 0; j < B->cols; j++) {

            elem_copy_value(re, sum, re->zero);

            for (size_t k = 0; k < A->cols; k++) {
                mat_get(A, i, k, x_raw);
                mat_get(B, k, j, y_raw);
                op->mul(prod, x_raw, y_raw);
                re->add(sum, sum, prod);
                elem_simplify_value(re, sum);
                elem_destroy_value(re, prod);
            }

            mat_set(C, i, j, sum);
            elem_destroy_value(re, sum);
        }
    }

    if (re == &dval_elem && mat_simplify_symbolic_inplace(C) != 0) {
        mat_free(C);
        return NULL;
    }

    return C;
}

matrix_t *mat_neg(const matrix_t *A)
{
    const struct elem_vtable *e;
    matrix_t *R;
    unsigned char a_raw[64] = {0}, out_raw[64] = {0};

    if (!A)
        return NULL;

    e = A->elem;
    if (!e || !e->sub)
        return NULL;

    R = mat_create_elementwise_unary_result(A->rows, A->cols, e, A);
    if (!R)
        return NULL;

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, a_raw);
            e->sub(out_raw, e->zero, a_raw);
            mat_set(R, i, j, out_raw);
            elem_destroy_value(e, out_raw);
        }

    return R;
}

matrix_t *mat_copy_with_store(const matrix_t *A,
                              const struct store_vtable *store)
{
    matrix_t *C;
    unsigned char raw[64] = {0};

    if (!A || !A->elem || !store)
        return NULL;

    C = mat_create_with_store(A->rows, A->cols, A->elem, store);
    if (!C)
        return NULL;

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, raw);
            mat_set(C, i, j, raw);
        }

    return C;
}

matrix_t *mat_copy_preserving_store(const matrix_t *A)
{
    if (!A)
        return NULL;

    return mat_copy_with_store(A, A->store);
}

matrix_t *mat_copy_as_dense(const matrix_t *A)
{
    return mat_copy_with_store(A, &dense_store);
}

matrix_t *mat_simplify_symbolic(const matrix_t *A)
{
    matrix_t *C;

    if (!A)
        return NULL;

    C = mat_copy_preserving_store(A);
    if (!C)
        return NULL;

    if (mat_simplify_symbolic_inplace(C) != 0) {
        mat_free(C);
        return NULL;
    }

    return C;
}

matrix_t *mat_convert_with_store(const matrix_t *A,
                                 const struct elem_vtable *target,
                                 const struct store_vtable *store)
{
    matrix_t *C;
    unsigned char src[64] = {0}, dst[64] = {0};

    if (!A || !target || !store)
        return NULL;

    C = mat_create_with_store(A->rows, A->cols, target, store);
    if (!C)
        return NULL;

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            qcomplex_t z;
            mat_get(A, i, j, src);
            A->elem->to_qc(&z, src);
            target->from_qc(dst, &z);
            mat_set(C, i, j, dst);
            elem_destroy_value(target, dst);
        }

    return C;
}

static matrix_t *mat_convert_dense(const matrix_t *A, const struct elem_vtable *target)
{
    return mat_convert_with_store(A, target, &dense_store);
}

static void mat_swap_rows(matrix_t *A, size_t r1, size_t r2)
{
    if (!A || !A->store || !A->store->swap_rows)
        return;

    A->store->swap_rows(A, r1, r2);
}

static void mat_row_eliminate_from(matrix_t *A, size_t dst_row, size_t src_row,
                                   size_t col_start, const void *factor)
{
    if (!A || !A->store || !A->store->row_eliminate_from)
        return;

    A->store->row_eliminate_from(A, dst_row, src_row, col_start, factor);
}

static size_t mat_find_pivot_row(const matrix_t *A, size_t col, size_t start)
{
    const struct elem_vtable *e = A->elem;
    unsigned char v[64];
    size_t best = start;
    double best_abs2 = -1.0;

    for (size_t i = start; i < A->rows; i++) {
        mat_get(A, i, col, v);
        double abs2 = e->abs2(v);
        if (abs2 > best_abs2) {
            best_abs2 = abs2;
            best = i;
        }
    }

    return best;
}

static const struct elem_vtable *mat_binary_result_elem(const matrix_t *A,
                                                        const matrix_t *B)
{
    elem_kind ak, bk;
    const binop_vtable *op;

    if (!A || !B)
        return NULL;

    ak = elem_of(A)->kind;
    bk = elem_of(B)->kind;
    op = &binops[ak][bk];
    return op->result_elem;
}

static matrix_t *mat_convert_preserving_store(const matrix_t *A,
                                              const struct elem_vtable *target)
{
    if (!A || !target)
        return NULL;

    return mat_convert_with_store(A, target, A->store);
}

static const struct store_vtable *mat_sparse_factor_store(const matrix_t *A,
                                                          const struct store_vtable *structured_store)
{
    if (!structured_store)
        return NULL;

    return mat_is_sparse_like(A) ? &sparse_store : structured_store;
}

static matrix_t *mat_apply_row_permutation(const matrix_t *P,
                                           const matrix_t *B,
                                           const struct elem_vtable *elem)
{
    matrix_t *PB;
    unsigned char pivot[64], value[64];

    if (!P || !B || !elem || P->rows != B->rows)
        return NULL;

    PB = mat_create_elementwise_unary_result(B->rows, B->cols, elem, B);
    if (!PB)
        return NULL;

    for (size_t i = 0; i < P->rows; i++) {
        size_t src_row = P->cols;

        for (size_t j = 0; j < P->cols; j++) {
            mat_get(P, i, j, pivot);
            if (elem->cmp(pivot, elem->zero) != 0) {
                src_row = j;
                break;
            }
        }

        if (src_row >= P->cols) {
            mat_free(PB);
            return NULL;
        }

        for (size_t j = 0; j < B->cols; j++) {
            mat_get(B, src_row, j, value);
            mat_set(PB, i, j, value);
        }
    }

    return PB;
}

static matrix_t *mat_create_direct_solve_result(const matrix_t *A,
                                                const matrix_t *B,
                                                const struct elem_vtable *elem)
{
    const struct store_vtable *store = &dense_store;

    if (!A || !B || !elem)
        return NULL;

    if (mat_has_diagonal_structure(A) &&
        B->store && B->store->elementwise_unary_store)
        store = B->store->elementwise_unary_store(B);

    return mat_create_with_store(A->cols, B->cols, elem, store);
}

static matrix_t *mat_solve_diagonal(const matrix_t *A,
                                    const matrix_t *B,
                                    const struct elem_vtable *elem)
{
    matrix_t *X;
    unsigned char diag[64], inv_diag[64], rhs[64], out[64];

    X = mat_create_direct_solve_result(A, B, elem);
    if (!X)
        return NULL;

    for (size_t i = 0; i < A->rows; i++) {
        mat_get(A, i, i, diag);
        if (elem->abs2(diag) < 1e-300) {
            mat_free(X);
            return NULL;
        }

        elem->inv(inv_diag, diag);
        for (size_t j = 0; j < B->cols; j++) {
            mat_get(B, i, j, rhs);
            elem->mul(out, inv_diag, rhs);
            mat_set(X, i, j, out);
        }
    }

    return X;
}

static matrix_t *mat_forward_substitute(const matrix_t *L,
                                        const matrix_t *B,
                                        const struct elem_vtable *elem)
{
    matrix_t *X;
    unsigned char diag[64], inv_diag[64], sum[64], a[64], b[64], prod[64], out[64];

    X = mat_create_dense_with_elem(L->cols, B->cols, elem);
    if (!X)
        return NULL;

    for (size_t i = 0; i < L->rows; i++) {
        mat_get(L, i, i, diag);
        if (elem->abs2(diag) < 1e-300) {
            mat_free(X);
            return NULL;
        }

        elem->inv(inv_diag, diag);
        for (size_t j = 0; j < B->cols; j++) {
            mat_get(B, i, j, sum);
            for (size_t k = 0; k < i; k++) {
                mat_get(L, i, k, a);
                mat_get(X, k, j, b);
                elem->mul(prod, a, b);
                elem->sub(sum, sum, prod);
            }
            elem->mul(out, inv_diag, sum);
            mat_set(X, i, j, out);
        }
    }

    return X;
}

static matrix_t *mat_backward_substitute(const matrix_t *U,
                                         const matrix_t *B,
                                         const struct elem_vtable *elem)
{
    matrix_t *X;
    unsigned char diag[64], inv_diag[64], sum[64], a[64], b[64], prod[64], out[64];

    X = mat_create_dense_with_elem(U->cols, B->cols, elem);
    if (!X)
        return NULL;

    for (size_t ii = U->rows; ii-- > 0;) {
        mat_get(U, ii, ii, diag);
        if (elem->abs2(diag) < 1e-300) {
            mat_free(X);
            return NULL;
        }

        elem->inv(inv_diag, diag);
        for (size_t j = 0; j < B->cols; j++) {
            mat_get(B, ii, j, sum);
            for (size_t k = ii + 1; k < U->cols; k++) {
                mat_get(U, ii, k, a);
                mat_get(X, k, j, b);
                elem->mul(prod, a, b);
                elem->sub(sum, sum, prod);
            }
            elem->mul(out, inv_diag, sum);
            mat_set(X, ii, j, out);
        }
    }

    return X;
}

/* ============================================================
   Transpose and conjugate (same-type result)
   ============================================================ */

struct matrix_t *mat_transpose(const struct matrix_t *A) {
    if (!A) return NULL;

    const struct elem_vtable *e = elem_of(A);
    struct matrix_t *T = mat_create_transpose_result(A->cols, A->rows, e, A);
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
    struct matrix_t *C = mat_create_elementwise_unary_result(A->rows, A->cols, e, A);
    if (!C) return NULL;

    unsigned char v[64] = {0}, cv[64] = {0};

    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, v);
            e->conj_elem(cv, v);
            mat_set(C, i, j, cv);
            elem_destroy_value(e, cv);
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

matrix_t *mat_deriv(const matrix_t *A, dval_t *wrt)
{
    matrix_t *D = NULL;

    if (!A || !wrt)
        return NULL;
    if (!A->elem)
        return NULL;

    if (A->elem->kind != ELEM_DVAL)
        return mat_create_zero_with_elem(A->rows, A->cols, A->elem);

    D = mat_create_zero_with_elem(A->rows, A->cols, &dval_elem);
    if (!D)
        return NULL;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            dval_t *entry = NULL;
            dval_t *deriv = NULL;

            mat_get(A, i, j, &entry);
            if (!entry)
                entry = DV_ZERO;

            deriv = dv_create_deriv(entry, wrt);
            if (!deriv) {
                mat_free(D);
                return NULL;
            }

            mat_set(D, i, j, &deriv);
            dv_free(deriv);
        }
    }

    return D;
}

dval_t *mat_deriv_trace_by_name(const matrix_t *A, binding_t *bindings, size_t number,
                                const char *name)
{
    binding_t *binding;

    if (!A || !bindings || !name)
        return NULL;

    binding = mat_binding_find(bindings, number, name);
    if (!binding)
        return NULL;

    return mat_deriv_trace(A, binding->symbol);
}

matrix_t *mat_deriv_by_name(const matrix_t *A, binding_t *bindings, size_t number,
                            const char *name)
{
    binding_t *binding;

    if (!A || !bindings || !name)
        return NULL;

    binding = mat_binding_find(bindings, number, name);
    if (!binding)
        return NULL;

    return mat_deriv(A, binding->symbol);
}

dval_t *mat_deriv_trace(const matrix_t *A, dval_t *wrt)
{
    dval_t *trace = NULL;
    dval_t *deriv = NULL;

    if (!A || !wrt)
        return NULL;
    if (!A->elem)
        return NULL;
    if (A->elem->kind != ELEM_DVAL)
        return dv_new_const_d(0.0);

    if (mat_trace(A, &trace) != 0 || !trace)
        return NULL;

    deriv = dv_create_deriv(trace, wrt);
    dv_free(trace);
    return deriv;
}

dval_t *mat_deriv_det(const matrix_t *A, dval_t *wrt)
{
    dval_t *det = NULL;
    dval_t *deriv = NULL;

    if (!A || !wrt)
        return NULL;
    if (!A->elem)
        return NULL;
    if (A->elem->kind != ELEM_DVAL)
        return dv_new_const_d(0.0);

    if (mat_det(A, &det) != 0 || !det)
        return NULL;

    deriv = dv_create_deriv(det, wrt);
    dv_free(det);
    return deriv;
}

dval_t *mat_deriv_det_by_name(const matrix_t *A, binding_t *bindings, size_t number,
                              const char *name)
{
    binding_t *binding;

    if (!A || !bindings || !name)
        return NULL;

    binding = mat_binding_find(bindings, number, name);
    if (!binding)
        return NULL;

    return mat_deriv_det(A, binding->symbol);
}

matrix_t *mat_deriv_inverse(const matrix_t *A, dval_t *wrt)
{
    matrix_t *Ai = NULL;
    matrix_t *dA = NULL;
    matrix_t *left = NULL;
    matrix_t *right = NULL;
    matrix_t *out = NULL;

    if (!A || !wrt)
        return NULL;
    if (!A->elem)
        return NULL;
    if (A->elem->kind != ELEM_DVAL) {
        Ai = mat_inverse(A);
        if (!Ai)
            return NULL;
        out = mat_create_zero_with_elem(A->rows, A->cols, A->elem);
        mat_free(Ai);
        return out;
    }

    Ai = mat_inverse(A);
    dA = mat_deriv(A, wrt);
    if (!Ai || !dA)
        goto cleanup;

    left = mat_mul(Ai, dA);
    if (!left)
        goto cleanup;

    right = mat_mul(left, Ai);
    if (!right)
        goto cleanup;

    out = mat_neg(right);

cleanup:
    mat_free(right);
    mat_free(left);
    mat_free(dA);
    mat_free(Ai);
    return out;
}

matrix_t *mat_deriv_inverse_by_name(const matrix_t *A, binding_t *bindings, size_t number,
                                    const char *name)
{
    binding_t *binding;

    if (!A || !bindings || !name)
        return NULL;

    binding = mat_binding_find(bindings, number, name);
    if (!binding)
        return NULL;

    return mat_deriv_inverse(A, binding->symbol);
}

matrix_t *mat_deriv_block_inverse(const matrix_t *A, size_t split, dval_t *wrt)
{
    matrix_t *Ai = NULL;
    matrix_t *dA = NULL;
    matrix_t *left = NULL;
    matrix_t *right = NULL;
    matrix_t *out = NULL;

    if (!A || !wrt)
        return NULL;
    if (A->rows != A->cols || split == 0 || split >= A->rows)
        return NULL;
    if (!A->elem)
        return NULL;
    if (A->elem->kind != ELEM_DVAL) {
        Ai = mat_block_inverse(A, split);
        if (!Ai)
            return NULL;
        out = mat_create_zero_with_elem(A->rows, A->cols, A->elem);
        mat_free(Ai);
        return out;
    }

    Ai = mat_block_inverse(A, split);
    dA = mat_deriv(A, wrt);
    if (!Ai || !dA)
        goto cleanup;

    left = mat_mul(Ai, dA);
    if (!left)
        goto cleanup;

    right = mat_mul(left, Ai);
    if (!right)
        goto cleanup;

    out = mat_neg(right);

cleanup:
    mat_free(right);
    mat_free(left);
    mat_free(dA);
    mat_free(Ai);
    return out;
}

matrix_t *mat_deriv_block_inverse_by_name(const matrix_t *A, size_t split,
                                          binding_t *bindings, size_t number,
                                          const char *name)
{
    binding_t *binding;

    if (!A || !bindings || !name)
        return NULL;

    binding = mat_binding_find(bindings, number, name);
    if (!binding)
        return NULL;

    return mat_deriv_block_inverse(A, split, binding->symbol);
}

matrix_t *mat_jacobian(const matrix_t *A, dval_t *const *vars, size_t nvars)
{
    matrix_t *J = NULL;

    if (!A || !vars || nvars == 0)
        return NULL;
    if (!A->elem)
        return NULL;
    if (A->elem->kind != ELEM_DVAL)
        return mat_create_zero_with_elem(A->rows * A->cols, nvars, &dval_elem);

    J = mat_create_zero_with_elem(A->rows * A->cols, nvars, &dval_elem);
    if (!J)
        return NULL;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            dval_t *entry = NULL;
            size_t row = i * A->cols + j;

            mat_get(A, i, j, &entry);
            if (!entry)
                entry = DV_ZERO;

            for (size_t k = 0; k < nvars; ++k) {
                dval_t *deriv = NULL;

                if (!vars[k]) {
                    mat_free(J);
                    return NULL;
                }

                deriv = dv_create_deriv(entry, vars[k]);
                if (!deriv) {
                    mat_free(J);
                    return NULL;
                }

                mat_set(J, row, k, &deriv);
                dv_free(deriv);
            }
        }
    }

    return J;
}

matrix_t *mat_jacobian_by_names(const matrix_t *A, binding_t *bindings, size_t number,
                                const char *const *names, size_t nnames)
{
    dval_t **vars = NULL;
    matrix_t *J = NULL;

    if (!A || !bindings || !names || nnames == 0)
        return NULL;

    vars = malloc(nnames * sizeof(*vars));
    if (!vars)
        return NULL;

    for (size_t i = 0; i < nnames; ++i) {
        binding_t *binding;

        if (!names[i]) {
            free(vars);
            return NULL;
        }

        binding = mat_binding_find(bindings, number, names[i]);
        if (!binding) {
            free(vars);
            return NULL;
        }

        vars[i] = binding->symbol;
    }

    J = mat_jacobian(A, vars, nnames);
    free(vars);
    return J;
}

int mat_trace(const matrix_t *A, void *trace)
{
    const struct elem_vtable *e;

    if (!A || !trace)
        return -1;
    if (A->rows != A->cols)
        return -2;

    e = A->elem;
    if (!e)
        return -3;

    if (e->kind == ELEM_DVAL) {
        dval_t *sum = dv_new_const(QF_ZERO);

        if (!sum)
            return -3;

        for (size_t i = 0; i < A->rows; ++i) {
            dval_t *term = NULL;
            dval_t *tmp = NULL;

            mat_get(A, i, i, &term);
            if (!term)
                term = DV_ZERO;

            tmp = dv_add(sum, term);
            dv_free(sum);
            sum = tmp;
            if (!sum)
                return -3;
        }

        *(dval_t **)trace = dval_simplify_owned(sum);
        return *(dval_t **)trace ? 0 : -3;
    }

    {
        unsigned char sum[64];
        unsigned char term[64];

        memcpy(sum, e->zero, e->size);
        for (size_t i = 0; i < A->rows; ++i) {
            mat_get(A, i, i, term);
            e->add(sum, sum, term);
        }
        memcpy(trace, sum, e->size);
    }

    return 0;
}

matrix_t *mat_charpoly(const matrix_t *A)
{
    if (!A || A->rows != A->cols)
        return NULL;

    if (A->elem == &dval_elem)
        return mat_charpoly_dval(A);

    if (!elem_supports_numeric_algorithms(A->elem))
        return NULL;

    return mat_charpoly_numeric(A);
}

matrix_t *mat_minpoly(const matrix_t *A)
{
    if (!A || A->rows != A->cols)
        return NULL;

    if (A->elem == &dval_elem)
        return mat_minpoly_dval(A);

    return NULL;
}

matrix_t *mat_apply_poly(const matrix_t *A, const matrix_t *coeffs)
{
    matrix_t *R = NULL;

    if (!A || !coeffs || A->rows != A->cols || coeffs->cols != 1 || coeffs->rows == 0)
        return NULL;
    if (A->elem != coeffs->elem)
        return NULL;

    {
        unsigned char c0[64] = {0};
        mat_get(coeffs, 0, 0, c0);
        R = mat_const_identity_with_elem(A->rows, A->elem, c0);
        if (!R)
            return NULL;
    }

    for (size_t k = 1; k < coeffs->rows; ++k) {
        unsigned char ck[64] = {0};
        matrix_t *AR = mat_mul(A, R);
        matrix_t *CI = NULL;
        matrix_t *Next = NULL;

        if (!AR) {
            mat_free(R);
            return NULL;
        }

        mat_get(coeffs, k, 0, ck);
        CI = mat_const_identity_with_elem(A->rows, A->elem, ck);
        if (!CI) {
            mat_free(AR);
            mat_free(R);
            return NULL;
        }

        Next = mat_add(AR, CI);
        mat_free(AR);
        mat_free(CI);
        mat_free(R);
        if (!Next)
            return NULL;
        R = Next;
    }

    return R;
}

matrix_t *mat_schur_complement(const matrix_t *A, size_t split)
{
    matrix_t *A11 = NULL;
    matrix_t *A12 = NULL;
    matrix_t *A21 = NULL;
    matrix_t *A22 = NULL;
    matrix_t *A11_inv = NULL;
    matrix_t *T = NULL;
    matrix_t *P = NULL;
    matrix_t *S = NULL;

    if (!A || A->rows != A->cols || split == 0 || split >= A->rows)
        return NULL;

    A11 = mat_extract_block(A, 0, split, 0, split);
    A12 = mat_extract_block(A, 0, split, split, A->cols - split);
    A21 = mat_extract_block(A, split, A->rows - split, 0, split);
    A22 = mat_extract_block(A, split, A->rows - split, split, A->cols - split);
    if (!A11 || !A12 || !A21 || !A22)
        goto fail;

    A11_inv = mat_inverse(A11);
    if (!A11_inv)
        goto fail;

    T = mat_mul(A21, A11_inv);
    if (!T)
        goto fail;

    P = mat_mul(T, A12);
    if (!P)
        goto fail;

    S = mat_sub(A22, P);

fail:
    mat_free(A11);
    mat_free(A12);
    mat_free(A21);
    mat_free(A22);
    mat_free(A11_inv);
    mat_free(T);
    mat_free(P);
    return S;
}

matrix_t *mat_block_inverse(const matrix_t *A, size_t split)
{
    matrix_t *A11 = NULL, *A12 = NULL, *A21 = NULL, *A22 = NULL;
    matrix_t *A11_inv = NULL, *S = NULL, *S_inv = NULL;
    matrix_t *A11i_A12 = NULL, *A21_A11i = NULL;
    matrix_t *Tmp = NULL, *TL = NULL, *TR = NULL, *BL = NULL, *BR = NULL;
    matrix_t *Out = NULL;

    if (!A || A->rows != A->cols || split == 0 || split >= A->rows)
        return NULL;

    A11 = mat_extract_block(A, 0, split, 0, split);
    A12 = mat_extract_block(A, 0, split, split, A->cols - split);
    A21 = mat_extract_block(A, split, A->rows - split, 0, split);
    A22 = mat_extract_block(A, split, A->rows - split, split, A->cols - split);
    if (!A11 || !A12 || !A21 || !A22)
        goto fail;

    A11_inv = mat_inverse(A11);
    S = mat_schur_complement(A, split);
    if (!A11_inv || !S)
        goto fail;

    S_inv = mat_inverse(S);
    if (!S_inv)
        goto fail;

    A11i_A12 = mat_mul(A11_inv, A12);
    A21_A11i = mat_mul(A21, A11_inv);
    if (!A11i_A12 || !A21_A11i)
        goto fail;

    Tmp = mat_mul(A11i_A12, S_inv);
    if (!Tmp)
        goto fail;
    TR = mat_neg(Tmp);
    mat_free(Tmp);
    Tmp = NULL;
    if (!TR)
        goto fail;

    Tmp = mat_mul(S_inv, A21_A11i);
    if (!Tmp)
        goto fail;
    BL = mat_neg(Tmp);
    mat_free(Tmp);
    Tmp = NULL;
    if (!BL)
        goto fail;

    BR = mat_extract_block(S_inv, 0, S_inv->rows, 0, S_inv->cols);
    if (!BR)
        goto fail;

    Tmp = mat_mul(A11i_A12, BL);
    if (!Tmp)
        goto fail;
    TL = mat_sub(A11_inv, Tmp);
    mat_free(Tmp);
    Tmp = NULL;
    if (!TL)
        goto fail;

    Out = mat_build_block_2x2(TL, TR, BL, BR);

fail:
    mat_free(A11);
    mat_free(A12);
    mat_free(A21);
    mat_free(A22);
    mat_free(A11_inv);
    mat_free(S);
    mat_free(S_inv);
    mat_free(A11i_A12);
    mat_free(A21_A11i);
    mat_free(Tmp);
    mat_free(TL);
    mat_free(TR);
    mat_free(BL);
    mat_free(BR);
    return mat_finalize_symbolic_result(Out);
}

matrix_t *mat_block_solve(const matrix_t *A, const matrix_t *B, size_t split)
{
    matrix_t *A11 = NULL, *A12 = NULL, *A21 = NULL;
    matrix_t *B1 = NULL, *B2 = NULL;
    matrix_t *A11_inv = NULL, *S = NULL, *S_inv = NULL;
    matrix_t *Tmp1 = NULL, *Tmp2 = NULL, *X1 = NULL, *X2 = NULL, *X = NULL;

    if (!A || !B || A->rows != A->cols || A->rows != B->rows)
        return NULL;
    if (split == 0 || split >= A->rows)
        return NULL;

    A11 = mat_extract_block(A, 0, split, 0, split);
    A12 = mat_extract_block(A, 0, split, split, A->cols - split);
    A21 = mat_extract_block(A, split, A->rows - split, 0, split);
    B1 = mat_extract_block(B, 0, split, 0, B->cols);
    B2 = mat_extract_block(B, split, B->rows - split, 0, B->cols);
    if (!A11 || !A12 || !A21 || !B1 || !B2)
        goto fail;

    A11_inv = mat_inverse(A11);
    S = mat_schur_complement(A, split);
    if (!A11_inv || !S)
        goto fail;

    S_inv = mat_inverse(S);
    if (!S_inv)
        goto fail;

    Tmp1 = mat_mul(A11_inv, B1);
    if (!Tmp1)
        goto fail;
    Tmp2 = mat_mul(A21, Tmp1);
    if (!Tmp2)
        goto fail;
    mat_free(Tmp1);
    Tmp1 = mat_sub(B2, Tmp2);
    mat_free(Tmp2);
    Tmp2 = NULL;
    if (!Tmp1)
        goto fail;
    X2 = mat_mul(S_inv, Tmp1);
    mat_free(Tmp1);
    Tmp1 = NULL;
    if (!X2)
        goto fail;

    Tmp1 = mat_mul(A12, X2);
    if (!Tmp1)
        goto fail;
    Tmp2 = mat_sub(B1, Tmp1);
    mat_free(Tmp1);
    Tmp1 = NULL;
    if (!Tmp2)
        goto fail;
    X1 = mat_mul(A11_inv, Tmp2);
    mat_free(Tmp2);
    Tmp2 = NULL;
    if (!X1)
        goto fail;

    X = mat_create_zero_with_elem(A->rows, B->cols, X1->elem);
    if (!X)
        goto fail;
    if (!mat_insert_block(X, 0, 0, X1) || !mat_insert_block(X, split, 0, X2)) {
        mat_free(X);
        X = NULL;
        goto fail;
    }

fail:
    mat_free(A11);
    mat_free(A12);
    mat_free(A21);
    mat_free(B1);
    mat_free(B2);
    mat_free(A11_inv);
    mat_free(S);
    mat_free(S_inv);
    mat_free(Tmp1);
    mat_free(Tmp2);
    mat_free(X1);
    mat_free(X2);
    return mat_finalize_symbolic_result(X);
}

matrix_t *mat_deriv_block_solve(const matrix_t *A, const matrix_t *B, size_t split, dval_t *wrt)
{
    matrix_t *X = NULL;
    matrix_t *dA = NULL;
    matrix_t *dB = NULL;
    matrix_t *dAX = NULL;
    matrix_t *RHS = NULL;
    matrix_t *dX = NULL;

    if (!A || !B || !wrt)
        return NULL;
    if (A->rows != A->cols || A->rows != B->rows)
        return NULL;
    if (split == 0 || split >= A->rows)
        return NULL;
    if (!A->elem || !B->elem)
        return NULL;
    if (A->elem->kind != ELEM_DVAL || B->elem->kind != ELEM_DVAL) {
        X = mat_block_solve(A, B, split);
        if (!X)
            return NULL;
        dX = mat_create_zero_with_elem(X->rows, X->cols, X->elem);
        mat_free(X);
        return dX;
    }

    X = mat_block_solve(A, B, split);
    dA = mat_deriv(A, wrt);
    dB = mat_deriv(B, wrt);
    if (!X || !dA || !dB)
        goto cleanup;

    dAX = mat_mul(dA, X);
    if (!dAX)
        goto cleanup;

    RHS = mat_sub(dB, dAX);
    if (!RHS)
        goto cleanup;

    dX = mat_block_solve(A, RHS, split);

cleanup:
    mat_free(RHS);
    mat_free(dAX);
    mat_free(dB);
    mat_free(dA);
    mat_free(X);
    return dX;
}

matrix_t *mat_deriv_block_solve_by_name(const matrix_t *A, const matrix_t *B, size_t split,
                                        binding_t *bindings, size_t number,
                                        const char *name)
{
    binding_t *binding;

    if (!A || !B || !bindings || !name)
        return NULL;

    binding = mat_binding_find(bindings, number, name);
    if (!binding)
        return NULL;

    return mat_deriv_block_solve(A, B, split, binding->symbol);
}

matrix_t *mat_deriv_solve(const matrix_t *A, const matrix_t *B, dval_t *wrt)
{
    matrix_t *X = NULL;
    matrix_t *dA = NULL;
    matrix_t *dB = NULL;
    matrix_t *dAX = NULL;
    matrix_t *RHS = NULL;
    matrix_t *dX = NULL;

    if (!A || !B || !wrt)
        return NULL;
    if (A->rows != A->cols || A->rows != B->rows)
        return NULL;
    if (!A->elem || !B->elem)
        return NULL;
    if (A->elem->kind != ELEM_DVAL || B->elem->kind != ELEM_DVAL) {
        X = mat_solve(A, B);
        if (!X)
            return NULL;
        dX = mat_create_zero_with_elem(X->rows, X->cols, X->elem);
        mat_free(X);
        return dX;
    }

    X = mat_solve(A, B);
    dA = mat_deriv(A, wrt);
    dB = mat_deriv(B, wrt);
    if (!X || !dA || !dB)
        goto cleanup;

    dAX = mat_mul(dA, X);
    if (!dAX)
        goto cleanup;

    RHS = mat_sub(dB, dAX);
    if (!RHS)
        goto cleanup;

    dX = mat_solve(A, RHS);

cleanup:
    mat_free(RHS);
    mat_free(dAX);
    mat_free(dB);
    mat_free(dA);
    mat_free(X);
    return dX;
}

matrix_t *mat_deriv_solve_by_name(const matrix_t *A, const matrix_t *B,
                                  binding_t *bindings, size_t number,
                                  const char *name)
{
    binding_t *binding;

    if (!A || !B || !bindings || !name)
        return NULL;

    binding = mat_binding_find(bindings, number, name);
    if (!binding)
        return NULL;

    return mat_deriv_solve(A, B, binding->symbol);
}

int mat_det(const matrix_t *A, void *determinant)
{
    if (A && A->elem && A->elem->kind == ELEM_DVAL)
        return mat_det_dval_exact(A, (dval_t **)determinant);
    if (!A || !determinant)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -3;

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
    matrix_t *M = mat_create_dense_with_elem(A->rows, A->cols, e);
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

matrix_t *mat_adjugate(const matrix_t *A)
{
    return mat_adjugate_exact(A);
}

matrix_t *mat_inverse(const matrix_t *A)
{
    if (!A)
        return NULL;

    if (A->rows != A->cols)
        return NULL;

    if (A->elem && A->elem->kind == ELEM_DVAL)
        return mat_inverse_dval_exact(A);

    if (!elem_supports_numeric_algorithms(A->elem))
        return NULL;

    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;

    /* Make working copies: M = A, I = identity */
    matrix_t *M = mat_create_dense_with_elem(n, n, e);
    matrix_t *I = mat_create_identity_with_elem(n, e);
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

matrix_t *mat_solve(const matrix_t *A, const matrix_t *B)
{
    mat_lu_factor_t lu = {0};
    const struct elem_vtable *e;
    matrix_t *Ac = NULL, *Bc = NULL, *PB = NULL, *Y = NULL, *X = NULL;

    if (!A || !B || A->rows != A->cols || A->rows != B->rows)
        return NULL;
    if (A->elem && B->elem &&
        A->elem->kind == ELEM_DVAL &&
        B->elem->kind == ELEM_DVAL)
        return mat_solve_dval_exact(A, B);
    if (!elem_supports_numeric_algorithms(A->elem) ||
        !elem_supports_numeric_algorithms(B->elem))
        return NULL;

    e = mat_binary_result_elem(A, B);
    if (!e)
        return NULL;

    Ac = mat_convert_preserving_store(A, e);
    Bc = mat_convert_preserving_store(B, e);
    if (!Ac || !Bc)
        goto fail;

    if (mat_has_diagonal_structure(Ac)) {
        X = mat_solve_diagonal(Ac, Bc, e);
        mat_free(Ac);
        mat_free(Bc);
        return X;
    }

    if (mat_has_lower_triangular_structure(Ac)) {
        X = mat_forward_substitute(Ac, Bc, e);
        mat_free(Ac);
        mat_free(Bc);
        return X;
    }

    if (mat_has_upper_triangular_structure(Ac)) {
        X = mat_backward_substitute(Ac, Bc, e);
        mat_free(Ac);
        mat_free(Bc);
        return X;
    }

    if (mat_lu_factor(Ac, &lu) != 0)
        goto fail;

    PB = mat_apply_row_permutation(lu.P, Bc, e);
    if (!PB)
        goto fail;

    Y = mat_forward_substitute(lu.L, PB, e);
    if (!Y)
        goto fail;

    X = mat_backward_substitute(lu.U, Y, e);
    if (!X)
        goto fail;

    mat_free(Ac);
    mat_free(Bc);
    mat_free(PB);
    mat_free(Y);
    mat_lu_factor_free(&lu);
    return X;

fail:
    mat_free(Ac);
    mat_free(Bc);
    mat_free(PB);
    mat_free(Y);
    mat_free(X);
    mat_lu_factor_free(&lu);
    return NULL;
}

matrix_t *mat_least_squares(const matrix_t *A, const matrix_t *B)
{
    mat_qr_factor_t qr = {0};
    matrix_t *QH = NULL, *QHB = NULL, *A_pinv = NULL, *X = NULL;
    int rank;

    if (!A || !B || A->rows != B->rows)
        return NULL;
    if (!elem_supports_numeric_algorithms(A->elem) ||
        !elem_supports_numeric_algorithms(B->elem))
        return NULL;

    if (A->rows == A->cols)
        return mat_solve(A, B);

    if (A->rows >= A->cols) {
        rank = mat_rank(A);
        if (rank < 0)
            return NULL;

        if ((size_t)rank == A->cols) {
            if (mat_qr_factor(A, &qr) != 0)
                goto fail;

            QH = mat_hermitian(qr.Q);
            QHB = QH ? mat_mul(QH, B) : NULL;
            if (!QH || !QHB)
                goto fail;

            X = mat_solve(qr.R, QHB);
            goto fail;
        }
    }

    A_pinv = mat_pseudoinverse(A);
    X = A_pinv ? mat_mul(A_pinv, B) : NULL;

fail:
    mat_qr_factor_free(&qr);
    mat_free(QH);
    mat_free(QHB);
    mat_free(A_pinv);
    return X;
}

void mat_lu_factor_free(mat_lu_factor_t *out)
{
    if (!out)
        return;
    mat_free(out->P);
    mat_free(out->L);
    mat_free(out->U);
    out->P = out->L = out->U = NULL;
}

int mat_lu_factor(const matrix_t *A, mat_lu_factor_t *out)
{
    const struct elem_vtable *e;
    const struct store_vtable *permutation_store;
    const struct store_vtable *lower_store;
    const struct store_vtable *upper_store;
    const struct store_vtable *working_lower_store;
    matrix_t *P_seed = NULL, *P = NULL, *L = NULL, *U = NULL;
    matrix_t *L_out = NULL, *U_out = NULL;
    unsigned char pivot[64], inv_pivot[64], factor[64];
    unsigned char a[64], b[64];

    if (!A || !out)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -3;
    if (A->rows != A->cols)
        return -2;

    e = A->elem;
    permutation_store = mat_sparse_factor_store(A, &identity_store);
    lower_store = mat_sparse_factor_store(A, &lower_triangular_store);
    upper_store = mat_sparse_factor_store(A, &upper_triangular_store);
    working_lower_store = mat_sparse_factor_store(A, &lower_triangular_store);
    P_seed = mat_create_identity_with_elem(A->rows, e);
    P = mat_convert_with_store(P_seed, e, permutation_store);
    L = mat_create_with_store(A->rows, A->rows, e, working_lower_store);
    U = mat_convert_preserving_store(A, e);
    mat_free(P_seed);
    if (!P || !L || !U) {
        mat_free(P);
        mat_free(L);
        mat_free(U);
        return -3;
    }

    for (size_t i = 0; i < A->rows; i++)
        mat_set(L, i, i, e->one);

    for (size_t k = 0; k < A->rows; k++) {
        size_t pivot_row = mat_find_pivot_row(U, k, k);
        mat_get(U, pivot_row, k, pivot);
        if (e->abs2(pivot) < 1e-300) {
            mat_free(P);
            mat_free(L);
            mat_free(U);
            return -4;
        }

        if (pivot_row != k) {
            mat_swap_rows(U, k, pivot_row);
            mat_swap_rows(P, k, pivot_row);
            for (size_t j = 0; j < k; j++) {
                mat_get(L, k, j, a);
                mat_get(L, pivot_row, j, b);
                mat_set(L, k, j, b);
                mat_set(L, pivot_row, j, a);
            }
        }

        mat_get(U, k, k, pivot);
        e->inv(inv_pivot, pivot);

        for (size_t i = k + 1; i < A->rows; i++) {
            mat_get(U, i, k, factor);
            if (e->abs2(factor) < 1e-300) {
                mat_set(L, i, k, e->zero);
                continue;
            }

            e->mul(factor, factor, inv_pivot);
            mat_set(L, i, k, factor);
            mat_set(U, i, k, e->zero);
            mat_row_eliminate_from(U, i, k, k + 1, factor);
        }
    }

    L_out = mat_convert_with_store(L, e, lower_store);
    U_out = mat_convert_with_store(U, e, upper_store);
    mat_free(U);
    mat_free(L);
    if (!L_out || !U_out) {
        mat_free(P);
        mat_free(L_out);
        mat_free(U_out);
        return -3;
    }

    out->P = P;
    out->L = L_out;
    out->U = U_out;
    return 0;
}

void mat_qr_factor_free(mat_qr_factor_t *out)
{
    if (!out)
        return;
    mat_free(out->Q);
    mat_free(out->R);
    out->Q = out->R = NULL;
}

int mat_qr_factor(const matrix_t *A, mat_qr_factor_t *out)
{
    size_t m, n, kdim;
    matrix_t *Z = NULL, *Qq = NULL, *Rq = NULL;
    const struct elem_vtable *target;

    if (!A || !out)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -3;

    m = A->rows;
    n = A->cols;
    kdim = (m < n) ? m : n;
    target = A->elem;

    Z = mat_convert_dense(A, &qcomplex_elem);
    Qq = mat_create_dense_with_elem(m, kdim, &qcomplex_elem);
    Rq = mat_create_upper_triangular_with_elem(kdim, n, &qcomplex_elem);
    if (!Z || !Qq || !Rq) {
        mat_free(Z);
        mat_free(Qq);
        mat_free(Rq);
        return -3;
    }

    for (size_t j = 0; j < n; j++) {
        qcomplex_t *v = malloc(m * sizeof(qcomplex_t));
        size_t imax = (j < kdim) ? j : kdim;
        if (!v) {
            mat_free(Z); mat_free(Qq); mat_free(Rq);
            return -3;
        }

        for (size_t r = 0; r < m; r++)
            mat_get(Z, r, j, &v[r]);

        for (size_t i = 0; i < imax; i++) {
            qcomplex_t rij = QC_ZERO;
            for (size_t r = 0; r < m; r++) {
                qcomplex_t qri;
                mat_get(Qq, r, i, &qri);
                rij = qc_add(rij, qc_mul(qc_conj(qri), v[r]));
            }
            mat_set(Rq, i, j, &rij);
            for (size_t r = 0; r < m; r++) {
                qcomplex_t qri;
                mat_get(Qq, r, i, &qri);
                v[r] = qc_sub(v[r], qc_mul(qri, rij));
            }
        }

        if (j < kdim) {
            qfloat_t norm2 = QF_ZERO;
            qcomplex_t rjj;
            for (size_t r = 0; r < m; r++)
                norm2 = qf_add(norm2, qf_add(qf_mul(v[r].re, v[r].re),
                                             qf_mul(v[r].im, v[r].im)));

            if (qf_to_double(norm2) < 1e-300) {
                rjj = QC_ZERO;
                mat_set(Rq, j, j, &rjj);
                for (size_t r = 0; r < m; r++) {
                    qcomplex_t zero = QC_ZERO;
                    mat_set(Qq, r, j, &zero);
                }
            } else {
                qfloat_t norm = qf_sqrt(norm2);
                qfloat_t inv = qf_div(QF_ONE, norm);
                rjj = qc_make(norm, QF_ZERO);
                mat_set(Rq, j, j, &rjj);
                for (size_t r = 0; r < m; r++) {
                    qcomplex_t qcol = qc_make(qf_mul(inv, v[r].re),
                                              qf_mul(inv, v[r].im));
                    mat_set(Qq, r, j, &qcol);
                }
            }
        }

        free(v);
    }

    out->Q = mat_convert_dense(Qq, target);
    out->R = mat_convert_with_store(Rq, target, &upper_triangular_store);
    mat_free(Z);
    mat_free(Qq);
    mat_free(Rq);

    if (!out->Q || !out->R) {
        mat_qr_factor_free(out);
        return -3;
    }

    return 0;
}

void mat_cholesky_free(mat_cholesky_t *out)
{
    if (!out)
        return;
    mat_free(out->L);
    out->L = NULL;
}

int mat_cholesky(const matrix_t *A, mat_cholesky_t *out)
{
    const struct store_vtable *lower_store;
    matrix_t *Z = NULL, *Lq = NULL;

    if (!A || !out)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -3;
    if (A->rows != A->cols)
        return -2;

    lower_store = mat_sparse_factor_store(A, &lower_triangular_store);

    Z = mat_convert_dense(A, &qcomplex_elem);
    Lq = mat_create_lower_triangular_with_elem(A->rows, A->cols, &qcomplex_elem);
    if (!Z || !Lq) {
        mat_free(Z);
        mat_free(Lq);
        return -3;
    }

    for (size_t i = 0; i < A->rows; i++) {
        for (size_t j = 0; j <= i; j++) {
            qcomplex_t sum, aij;
            mat_get(Z, i, j, &aij);
            sum = aij;

            for (size_t k = 0; k < j; k++) {
                qcomplex_t lik, ljk;
                mat_get(Lq, i, k, &lik);
                mat_get(Lq, j, k, &ljk);
                sum = qc_sub(sum, qc_mul(lik, qc_conj(ljk)));
            }

            if (i == j) {
                double imag_abs = fabs(qf_to_double(sum.im));
                double real_val = qf_to_double(sum.re);
                if (imag_abs > 1e-12 || real_val <= 0.0) {
                    mat_free(Z);
                    mat_free(Lq);
                    return -4;
                }
                qcomplex_t diag = qc_make(qf_sqrt(sum.re), QF_ZERO);
                mat_set(Lq, i, j, &diag);
            } else {
                qcomplex_t ljj;
                mat_get(Lq, j, j, &ljj);
                if (qf_to_double(qc_abs(ljj)) < 1e-300) {
                    mat_free(Z);
                    mat_free(Lq);
                    return -4;
                }
                qcomplex_t lij = qc_div(sum, qc_conj(ljj));
                mat_set(Lq, i, j, &lij);
            }
        }

    }

    out->L = mat_convert_with_store(Lq, A->elem, lower_store);
    mat_free(Z);
    mat_free(Lq);

    if (!out->L)
        return -3;

    return 0;
}

void mat_svd_factor_free(mat_svd_factor_t *out)
{
    if (!out)
        return;
    mat_free(out->U);
    mat_free(out->S);
    mat_free(out->V);
    out->U = out->S = out->V = NULL;
}

static double mat_singular_tolerance(const matrix_t *A, double sigma_max)
{
    qfloat_t sigma_qf = qf_from_double(sigma_max);
    qfloat_t dim_qf = qf_from_double((double)((A->rows > A->cols) ? A->rows : A->cols));
    qfloat_t tol_qf = qf_mul(qf_mul(sigma_qf, dim_qf), A->elem->relative_epsilon);

    if (qf_lt(tol_qf, A->elem->relative_epsilon))
        tol_qf = A->elem->relative_epsilon;

    return qf_to_double(tol_qf);
}

static int mat_norm_via_svd(const matrix_t *A, qfloat_t *out)
{
    mat_svd_factor_t svd = {0};
    size_t kdim;
    qfloat_t best = QF_ZERO;

    if (!A || !out)
        return -1;

    if (mat_svd_factor(A, &svd) != 0)
        return -2;

    kdim = (A->rows < A->cols) ? A->rows : A->cols;
    for (size_t i = 0; i < kdim; i++) {
        unsigned char raw[64];
        qfloat_t sig;
        mat_get(svd.S, i, i, raw);
        svd.S->elem->to_qf(&sig, raw);
        sig = qf_abs(sig);
        if (qf_gt(sig, best))
            best = sig;
    }

    *out = best;
    mat_svd_factor_free(&svd);
    return 0;
}

int mat_svd_factor(const matrix_t *A, mat_svd_factor_t *out)
{
    size_t m, n, kdim;
    matrix_t *Z = NULL, *ZH = NULL, *Gram = NULL;
    matrix_t *EigVecs = NULL, *LeftQ = NULL, *RightQ = NULL, *Sq = NULL;
    qcomplex_t *evals = NULL;
    qfloat_t *sigma = NULL;
    size_t *order = NULL;
    int rc;

    if (!A || !out)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -3;

    m = A->rows;
    n = A->cols;
    kdim = (m < n) ? m : n;

    Z = mat_convert_dense(A, &qcomplex_elem);
    if (!Z)
        return -3;

    evals = calloc(kdim ? kdim : 1, sizeof(qcomplex_t));
    sigma = calloc(kdim ? kdim : 1, sizeof(qfloat_t));
    order = calloc(kdim ? kdim : 1, sizeof(size_t));
    if (!evals || !sigma || !order) {
        rc = -3;
        goto fail;
    }

    if (m >= n) {
        ZH = mat_hermitian(Z);
        Gram = mat_mul(ZH, Z);
        if (!ZH || !Gram) {
            rc = -3;
            goto fail;
        }
        rc = mat_eigendecompose(Gram, evals, &EigVecs);
        if (rc != 0)
            goto fail;

        for (size_t i = 0; i < n; i++) {
            double re = qf_to_double(evals[i].re);
            order[i] = i;
            sigma[i] = (re > 0.0) ? qf_sqrt(evals[i].re) : QF_ZERO;
        }

        for (size_t i = 0; i < n; i++)
            for (size_t j = i + 1; j < n; j++)
                if (qf_to_double(sigma[order[j]]) > qf_to_double(sigma[order[i]])) {
                    size_t tmp = order[i];
                    order[i] = order[j];
                    order[j] = tmp;
                }

        RightQ = mat_create_dense_with_elem(n, kdim, &qcomplex_elem);
        LeftQ = mat_create_dense_with_elem(m, kdim, &qcomplex_elem);
        Sq = mat_create_diagonal_with_elem(kdim, &qcomplex_elem);
        if (!RightQ || !LeftQ || !Sq) {
            rc = -3;
            goto fail;
        }

        for (size_t j = 0; j < kdim; j++) {
            size_t idx = order[j];
            qcomplex_t sigqc = qc_make(sigma[idx], QF_ZERO);
            qcomplex_t invsig = qf_to_double(sigma[idx]) > 1e-300
                              ? qc_make(qf_div(QF_ONE, sigma[idx]), QF_ZERO)
                              : QC_ZERO;

            for (size_t r = 0; r < n; r++) {
                qcomplex_t v;
                mat_get(EigVecs, r, idx, &v);
                mat_set(RightQ, r, j, &v);
            }

            mat_set(Sq, j, j, &sigqc);

            for (size_t r = 0; r < m; r++) {
                qcomplex_t sum = QC_ZERO;
                for (size_t c = 0; c < n; c++) {
                    qcomplex_t a_rc, v_c;
                    mat_get(Z, r, c, &a_rc);
                    mat_get(RightQ, c, j, &v_c);
                    sum = qc_add(sum, qc_mul(a_rc, v_c));
                }
                if (qf_to_double(sigma[idx]) > 1e-300)
                    sum = qc_mul(sum, invsig);
                else
                    sum = QC_ZERO;
                mat_set(LeftQ, r, j, &sum);
            }
        }
    } else {
        ZH = mat_hermitian(Z);
        Gram = mat_mul(Z, ZH);
        if (!ZH || !Gram) {
            rc = -3;
            goto fail;
        }
        rc = mat_eigendecompose(Gram, evals, &EigVecs);
        if (rc != 0)
            goto fail;

        for (size_t i = 0; i < m; i++) {
            double re = qf_to_double(evals[i].re);
            order[i] = i;
            sigma[i] = (re > 0.0) ? qf_sqrt(evals[i].re) : QF_ZERO;
        }

        for (size_t i = 0; i < m; i++)
            for (size_t j = i + 1; j < m; j++)
                if (qf_to_double(sigma[order[j]]) > qf_to_double(sigma[order[i]])) {
                    size_t tmp = order[i];
                    order[i] = order[j];
                    order[j] = tmp;
                }

        LeftQ = mat_create_dense_with_elem(m, kdim, &qcomplex_elem);
        RightQ = mat_create_dense_with_elem(n, kdim, &qcomplex_elem);
        Sq = mat_create_diagonal_with_elem(kdim, &qcomplex_elem);
        if (!RightQ || !LeftQ || !Sq) {
            rc = -3;
            goto fail;
        }

        for (size_t j = 0; j < kdim; j++) {
            size_t idx = order[j];
            qcomplex_t sigqc = qc_make(sigma[idx], QF_ZERO);
            qcomplex_t invsig = qf_to_double(sigma[idx]) > 1e-300
                              ? qc_make(qf_div(QF_ONE, sigma[idx]), QF_ZERO)
                              : QC_ZERO;

            for (size_t r = 0; r < m; r++) {
                qcomplex_t u;
                mat_get(EigVecs, r, idx, &u);
                mat_set(LeftQ, r, j, &u);
            }

            mat_set(Sq, j, j, &sigqc);

            for (size_t r = 0; r < n; r++) {
                qcomplex_t sum = QC_ZERO;
                for (size_t c = 0; c < m; c++) {
                    qcomplex_t ah_rc, u_c;
                    mat_get(ZH, r, c, &ah_rc);
                    mat_get(LeftQ, c, j, &u_c);
                    sum = qc_add(sum, qc_mul(ah_rc, u_c));
                }
                if (qf_to_double(sigma[idx]) > 1e-300)
                    sum = qc_mul(sum, invsig);
                else
                    sum = QC_ZERO;
                mat_set(RightQ, r, j, &sum);
            }
        }
    }

    out->U = mat_convert_dense(LeftQ, A->elem);
    out->S = mat_convert_with_store(Sq, A->elem, &diagonal_store);
    out->V = mat_convert_dense(RightQ, A->elem);
    if (!out->U || !out->S || !out->V) {
        rc = -3;
        goto fail;
    }

    rc = 0;

fail:
    mat_free(Z);
    mat_free(ZH);
    mat_free(Gram);
    mat_free(EigVecs);
    mat_free(LeftQ);
    mat_free(RightQ);
    mat_free(Sq);
    free(evals);
    free(sigma);
    free(order);
    if (rc != 0)
        mat_svd_factor_free(out);
    return rc;
}

typedef int (*mat_norm_function_t)(const matrix_t *A, qfloat_t *out);

static int mat_norm_one(const matrix_t *A, qfloat_t *out)
{
    const struct elem_vtable *e;

    e = A->elem;

    qfloat_t best = QF_ZERO;
    for (size_t j = 0; j < A->cols; j++) {
        qfloat_t sum = QF_ZERO;
        for (size_t i = 0; i < A->rows; i++) {
            unsigned char raw[64];
            qfloat_t mag;
            mat_get(A, i, j, raw);
            e->abs_qf(&mag, raw);
            sum = qf_add(sum, mag);
        }
        if (qf_gt(sum, best))
            best = sum;
    }
    *out = best;
    return 0;
}

static int mat_norm_infinity(const matrix_t *A, qfloat_t *out)
{
    const struct elem_vtable *e = A->elem;
    qfloat_t best = QF_ZERO;

    for (size_t i = 0; i < A->rows; i++) {
        qfloat_t sum = QF_ZERO;
        for (size_t j = 0; j < A->cols; j++) {
            unsigned char raw[64];
            qfloat_t mag;
            mat_get(A, i, j, raw);
            e->abs_qf(&mag, raw);
            sum = qf_add(sum, mag);
        }
        if (qf_gt(sum, best))
            best = sum;
    }
    *out = best;
    return 0;
}

static int mat_norm_frobenius(const matrix_t *A, qfloat_t *out)
{
    const struct elem_vtable *e = A->elem;
    qfloat_t sumsq = QF_ZERO;

    for (size_t i = 0; i < A->rows; i++) {
        for (size_t j = 0; j < A->cols; j++) {
            unsigned char raw[64];
            qfloat_t mag;
            mat_get(A, i, j, raw);
            e->abs_qf(&mag, raw);
            sumsq = qf_add(sumsq, qf_mul(mag, mag));
        }
    }
    *out = qf_sqrt(sumsq);
    return 0;
}

static const mat_norm_function_t mat_norm_functions[] = {
    [MAT_NORM_1] = mat_norm_one,
    [MAT_NORM_INF] = mat_norm_infinity,
    [MAT_NORM_FRO] = mat_norm_frobenius,
    [MAT_NORM_2] = mat_norm_via_svd
};

static mat_norm_function_t mat_norm_function_for(mat_norm_type_t type)
{
    size_t index = (size_t)type;

    if (index >= sizeof(mat_norm_functions) / sizeof(mat_norm_functions[0]))
        return NULL;

    return mat_norm_functions[index];
}

int mat_norm(const matrix_t *A, mat_norm_type_t type, qfloat_t *out)
{
    mat_norm_function_t fun;

    if (!A || !out)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -2;

    fun = mat_norm_function_for(type);
    if (!fun)
        return -2;

    return fun(A, out);
}

int mat_condition_number(const matrix_t *A, mat_norm_type_t type, qfloat_t *out)
{
    int rank;
    size_t kdim;

    if (!A || !out)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -2;

    rank = mat_rank(A);
    if (rank < 0)
        return -2;

    kdim = (A->rows < A->cols) ? A->rows : A->cols;
    if ((size_t)rank < kdim) {
        *out = QF_INF;
        return 0;
    }

    if (mat_norm_function_for(type) == mat_norm_via_svd) {
        mat_svd_factor_t svd = {0};
        qfloat_t sigma_max = QF_ZERO;
        qfloat_t sigma_min = QF_INF;
        if (mat_svd_factor(A, &svd) != 0)
            return -2;
        for (size_t i = 0; i < kdim; i++) {
            unsigned char raw[64];
            qfloat_t sig;
            mat_get(svd.S, i, i, raw);
            svd.S->elem->to_qf(&sig, raw);
            sig = qf_abs(sig);
            if (qf_gt(sig, sigma_max))
                sigma_max = sig;
            if (qf_lt(sig, sigma_min))
                sigma_min = sig;
        }
        *out = qf_div(sigma_max, sigma_min);
        mat_svd_factor_free(&svd);
        return 0;
    }

    {
        qfloat_t na, ni;
        matrix_t *Aw = NULL;
        matrix_t *Ai = NULL;
        int rc_a, rc_i;

        Aw = mat_convert_dense(A, &qcomplex_elem);
        if (!Aw)
            return -2;

        if (Aw->rows == Aw->cols) {
            matrix_t *I = mat_create_identity_with_elem(Aw->rows, Aw->elem);
            if (!I)
                goto fail_non2;
            Ai = mat_solve(Aw, I);
            mat_free(I);
        } else {
            Ai = mat_pseudoinverse(Aw);
        }

        if (!Ai)
            goto fail_non2;

        rc_a = mat_norm(Aw, type, &na);
        rc_i = mat_norm(Ai, type, &ni);
        mat_free(Ai);
        mat_free(Aw);
        if (rc_a != 0 || rc_i != 0)
            return -2;
        *out = qf_mul(na, ni);
        return 0;

fail_non2:
        mat_free(Aw);
        mat_free(Ai);
        return -2;
    }
}

int mat_rank(const matrix_t *A)
{
    mat_svd_factor_t svd = {0};
    size_t kdim;
    double sigma_max = 0.0, tol;
    int rank = 0;

    if (!A)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -2;

    if (mat_svd_factor(A, &svd) != 0)
        return -2;

    kdim = (A->rows < A->cols) ? A->rows : A->cols;
    for (size_t i = 0; i < kdim; i++) {
        unsigned char raw[64];
        qfloat_t sig;
        double d;
        mat_get(svd.S, i, i, raw);
        svd.S->elem->to_qf(&sig, raw);
        d = qf_to_double(qf_abs(sig));
        if (d > sigma_max)
            sigma_max = d;
    }

    tol = mat_singular_tolerance(A, sigma_max);
    for (size_t i = 0; i < kdim; i++) {
        unsigned char raw[64];
        qfloat_t sig;
        double d;
        mat_get(svd.S, i, i, raw);
        svd.S->elem->to_qf(&sig, raw);
        d = qf_to_double(qf_abs(sig));
        if (d > tol)
            rank++;
    }

    mat_svd_factor_free(&svd);
    return rank;
}

matrix_t *mat_pseudoinverse(const matrix_t *A)
{
    mat_svd_factor_t svd = {0};
    matrix_t *Uq = NULL, *Vq = NULL, *UH = NULL, *Sp = NULL;
    matrix_t *VSp = NULL, *Pinvq = NULL, *Pinv = NULL;
    size_t kdim;
    double sigma_max = 0.0, tol;

    if (!A)
        return NULL;
    if (!elem_supports_numeric_algorithms(A->elem))
        return NULL;

    if (mat_svd_factor(A, &svd) != 0)
        return NULL;

    kdim = (A->rows < A->cols) ? A->rows : A->cols;
    for (size_t i = 0; i < kdim; i++) {
        unsigned char raw[64];
        qfloat_t sig;
        double d;
        mat_get(svd.S, i, i, raw);
        svd.S->elem->to_qf(&sig, raw);
        d = qf_to_double(qf_abs(sig));
        if (d > sigma_max)
            sigma_max = d;
    }
    tol = mat_singular_tolerance(A, sigma_max);

    Uq = mat_convert_dense(svd.U, &qcomplex_elem);
    Vq = mat_convert_dense(svd.V, &qcomplex_elem);
    Sp = mat_create_diagonal_with_elem(kdim, &qcomplex_elem);
    if (!Uq || !Vq || !Sp)
        goto fail;

    for (size_t i = 0; i < kdim; i++) {
        unsigned char raw[64];
        qfloat_t sig;
        double d;
        qcomplex_t val;
        mat_get(svd.S, i, i, raw);
        svd.S->elem->to_qf(&sig, raw);
        d = qf_to_double(qf_abs(sig));
        if (d > tol)
            val = qc_make(qf_div(QF_ONE, sig), QF_ZERO);
        else
            val = QC_ZERO;
        mat_set(Sp, i, i, &val);
    }

    UH = mat_hermitian(Uq);
    VSp = (Vq && Sp) ? mat_mul(Vq, Sp) : NULL;
    Pinvq = (VSp && UH) ? mat_mul(VSp, UH) : NULL;
    if (!UH || !VSp || !Pinvq)
        goto fail;

    Pinv = mat_convert_dense(Pinvq, A->elem);

fail:
    mat_svd_factor_free(&svd);
    mat_free(Uq);
    mat_free(Vq);
    mat_free(UH);
    mat_free(Sp);
    mat_free(VSp);
    mat_free(Pinvq);
    return Pinv;
}

matrix_t *mat_nullspace(const matrix_t *A)
{
    matrix_t *AH = NULL, *Gram = NULL, *Gramq = NULL, *V = NULL, *Nq = NULL, *N = NULL;
    qcomplex_t *evals = NULL;
    size_t nullity = 0, col = 0;
    double sigma_max = 0.0, tol;
    int rc;

    if (!A)
        return NULL;
    if (A->elem == &dval_elem)
        return mat_nullspace_dval_exact(A);
    if (!elem_supports_numeric_algorithms(A->elem))
        return NULL;

    AH = mat_hermitian(A);
    Gram = AH ? mat_mul(AH, A) : NULL;
    Gramq = Gram ? mat_convert_dense(Gram, &qcomplex_elem) : NULL;
    if (!AH || !Gram || !Gramq)
        goto fail;

    evals = calloc(A->cols ? A->cols : 1, sizeof(qcomplex_t));
    if (!evals)
        goto fail;

    rc = mat_eigendecompose(Gramq, evals, &V);
    if (rc != 0 || !V)
        goto fail;

    for (size_t i = 0; i < A->cols; i++) {
        double d = qf_to_double(qf_abs(evals[i].re));
        if (d > sigma_max)
            sigma_max = d;
    }
    tol = mat_singular_tolerance(A, sqrt(sigma_max));
    tol *= tol;

    for (size_t i = 0; i < A->cols; i++) {
        double d = qf_to_double(qf_abs(evals[i].re));
        if (d <= tol)
            nullity++;
    }

    Nq = mat_create_dense_with_elem(A->cols, nullity, &qcomplex_elem);
    if (!Nq)
        goto fail;

    for (size_t i = 0; i < A->cols; i++) {
        double d = qf_to_double(qf_abs(evals[i].re));
        if (d > tol)
            continue;
        for (size_t r = 0; r < A->cols; r++) {
            unsigned char raw[64];
            qcomplex_t z;
            mat_get(V, r, i, raw);
            V->elem->to_qc(&z, raw);
            mat_set(Nq, r, col, &z);
        }
        col++;
    }

    N = mat_convert_dense(Nq, A->elem);

fail:
    mat_free(AH);
    mat_free(Gram);
    mat_free(Gramq);
    mat_free(V);
    mat_free(Nq);
    free(evals);
    return N;
}

static matrix_t *mat_shift_subtract_eigenvalue(const matrix_t *A, const void *eigenvalue)
{
    matrix_t *Shifted = NULL;

    if (!A || !eigenvalue || A->rows != A->cols)
        return NULL;

    Shifted = mat_copy_as_dense(A);
    if (!Shifted)
        return NULL;

    if (A->elem == &dval_elem) {
        dval_t *lambda = *(dval_t *const *)eigenvalue;

        if (!lambda) {
            mat_free(Shifted);
            return NULL;
        }

        for (size_t i = 0; i < A->rows; ++i) {
            dval_t *diag = NULL;
            dval_t *new_diag = NULL;

            mat_get(Shifted, i, i, &diag);
            if (!diag)
                diag = DV_ZERO;

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
    } else {
        unsigned char diag[64];
        unsigned char shifted[64];

        if (!elem_supports_numeric_algorithms(A->elem)) {
            mat_free(Shifted);
            return NULL;
        }

        for (size_t i = 0; i < A->rows; ++i) {
            mat_get(Shifted, i, i, diag);
            A->elem->sub(shifted, diag, eigenvalue);
            mat_set(Shifted, i, i, shifted);
        }
    }

    return Shifted;
}

static bool mat_column_is_structural_zero(const matrix_t *A, size_t col)
{
    unsigned char raw[64];

    if (!A || col >= A->cols)
        return true;

    for (size_t i = 0; i < A->rows; ++i) {
        mat_get(A, i, col, raw);
        if (!elem_is_structural_zero(A->elem, raw))
            return false;
    }

    return true;
}

static matrix_t *mat_extract_column_copy(const matrix_t *A, size_t col)
{
    matrix_t *C;
    unsigned char raw[64];

    if (!A || col >= A->cols)
        return NULL;

    C = mat_create_dense_with_elem(A->rows, 1, A->elem);
    if (!C)
        return NULL;

    for (size_t i = 0; i < A->rows; ++i) {
        mat_get(A, i, col, raw);
        mat_set(C, i, 0, raw);
    }

    return C;
}

matrix_t *mat_eigenspace(const matrix_t *A, const void *eigenvalue)
{
    matrix_t *Shifted = NULL;
    matrix_t *E = NULL;

    Shifted = mat_shift_subtract_eigenvalue(A, eigenvalue);
    if (!Shifted)
        return NULL;

    E = mat_nullspace(Shifted);
    mat_free(Shifted);
    return E;
}

matrix_t *mat_generalized_eigenspace(const matrix_t *A, const void *eigenvalue,
                                     size_t order)
{
    matrix_t *Shifted = NULL;
    matrix_t *Power = NULL;
    matrix_t *G = NULL;

    if (!A || !eigenvalue || A->rows != A->cols || order == 0)
        return NULL;

    if (order == 1)
        return mat_eigenspace(A, eigenvalue);

    Shifted = mat_shift_subtract_eigenvalue(A, eigenvalue);
    if (!Shifted)
        return NULL;

    Power = mat_pow_int(Shifted, (int)order);
    if (!Power) {
        mat_free(Shifted);
        return NULL;
    }

    G = mat_nullspace(Power);
    mat_free(Shifted);
    mat_free(Power);
    return G;
}

matrix_t *mat_jordan_chain(const matrix_t *A, const void *eigenvalue,
                           size_t order)
{
    matrix_t *Shifted = NULL;
    matrix_t *G = NULL;
    matrix_t *Chain = NULL;
    matrix_t *Tail = NULL;
    matrix_t *Probe = NULL;
    matrix_t *Current = NULL;
    bool found = false;

    if (!A || !eigenvalue || A->rows != A->cols || order == 0)
        return NULL;

    Shifted = mat_shift_subtract_eigenvalue(A, eigenvalue);
    if (!Shifted)
        return NULL;

    G = mat_generalized_eigenspace(A, eigenvalue, order);
    if (!G)
        goto fail;

    for (size_t col = 0; col < G->cols && !found; ++col) {
        Tail = mat_extract_column_copy(G, col);
        if (!Tail)
            goto fail;

        Probe = mat_copy_preserving_store(Tail);
        if (!Probe)
            goto fail;

        for (size_t step = 1; step < order; ++step) {
            matrix_t *Next = mat_mul(Shifted, Probe);
            mat_free(Probe);
            Probe = Next;
            if (!Probe)
                goto fail;
        }

        if (!mat_column_is_structural_zero(Probe, 0))
            found = true;
        else {
            mat_free(Tail);
            Tail = NULL;
        }

        mat_free(Probe);
        Probe = NULL;
    }

    if (!found || !Tail)
        goto fail;

    Chain = mat_create_dense_with_elem(A->rows, order, A->elem);
    if (!Chain)
        goto fail;

    Current = Tail;
    Tail = NULL;
    for (size_t j = order; j-- > 0;) {
        unsigned char raw[64];

        for (size_t i = 0; i < A->rows; ++i) {
            mat_get(Current, i, 0, raw);
            mat_set(Chain, i, j, raw);
        }

        if (j > 0) {
            matrix_t *Prev = mat_mul(Shifted, Current);
            if (!Prev)
                goto fail;
            mat_free(Current);
            Current = Prev;
        }
    }

    mat_free(Current);
    mat_free(Shifted);
    mat_free(G);
    return Chain;

fail:
    mat_free(Current);
    mat_free(Tail);
    mat_free(Probe);
    mat_free(Chain);
    mat_free(G);
    mat_free(Shifted);
    return NULL;
}

matrix_t *mat_jordan_profile(const matrix_t *A, const void *eigenvalue)
{
    size_t *dims = NULL;
    matrix_t *G = NULL;
    matrix_t *P = NULL;
    size_t n;
    size_t blocks = 0;
    size_t out = 0;

    if (!A || !eigenvalue || A->rows != A->cols)
        return NULL;
    if (A->elem != &dval_elem)
        return NULL;

    n = A->rows;
    dims = calloc(n + 1, sizeof(*dims));
    if (!dims)
        return NULL;

    for (size_t k = 1; k <= n; ++k) {
        G = mat_generalized_eigenspace(A, eigenvalue, k);
        if (!G)
            goto fail;
        dims[k] = G->cols;
        mat_free(G);
        G = NULL;
        if (dims[k] < dims[k - 1])
            goto fail;
    }

    blocks = dims[1];
    P = mat_new_d(blocks, 1);
    if (!P)
        goto fail;

    for (size_t k = n; k >= 1; --k) {
        size_t at_least_k = dims[k] - dims[k - 1];
        size_t at_least_next = (k < n) ? (dims[k + 1] - dims[k]) : 0;
        size_t exact_k = at_least_k - at_least_next;
        double block_size = (double)k;

        for (size_t c = 0; c < exact_k; ++c) {
            if (out >= blocks)
                goto fail;
            mat_set(P, out, 0, &block_size);
            out++;
        }

        if (k == 1)
            break;
    }

    if (out != blocks)
        goto fail;

    free(dims);
    return P;

fail:
    free(dims);
    mat_free(G);
    mat_free(P);
    return NULL;
}

/* ============================================================
   Eigenvalues / eigenvectors (Hermitian Jacobi implementation)
   ============================================================ */

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

    matrix_t *W = mat_copy_as_dense(A);
    if (!W) return -3;

    matrix_t *V = mat_create_identity_with_elem(n, e);
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
int mat_is_hermitian(const matrix_t *A)
{
    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;
    unsigned char aij[64], aji[64], cji[64], diff[64], diag[64];
    double tol2 = 0.0;
    /* build tolerance from Frobenius norm */
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < n; j++) {
            mat_get(A, i, j, aij);
            tol2 += e->abs2(aij);
        }
    tol2 *= 1e-28;
    for (size_t i = 0; i < n; i++) {
        mat_get(A, i, i, diag);
        e->conj_elem(cji, diag);
        e->sub(diff, diag, cji);
        if (e->abs2(diff) > tol2) return 0;
    }
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

static void qr_schur_2x2(qcomplex_t *H, qcomplex_t *Q, size_t n)
{
    qfloat_t eps = qf_from_double(1e-30);
    qcomplex_t a = QCM(H, 0, 0, n);
    qcomplex_t b = QCM(H, 0, 1, n);
    qcomplex_t c = QCM(H, 1, 0, n);
    qcomplex_t d = QCM(H, 1, 1, n);

    if (qf_lt(qc_abs(c), eps)) {
        QCM(H, 1, 0, n) = QC_ZERO;
        return;
    }

    qcomplex_t half_tr = qcs(qf_div(QF_ONE, qf_from_double(2.0)),
                             qc_add(a, d));
    qcomplex_t det = qc_sub(qc_mul(a, d), qc_mul(b, c));
    qcomplex_t disc = qc_sub(qc_mul(half_tr, half_tr), det);
    qcomplex_t root = qc_sqrt(disc);
    qcomplex_t lam1 = qc_add(half_tr, root);
    qcomplex_t lam2 = qc_sub(half_tr, root);

    qcomplex_t v0 = qc_sub(lam1, d);
    qcomplex_t v1 = c;
    qfloat_t vnrm = qf_sqrt(qf_add(qc_abs2_qf(v0), qc_abs2_qf(v1)));
    if (qf_eq(vnrm, QF_ZERO))
        return;

    qfloat_t inv = qf_div(QF_ONE, vnrm);
    qcomplex_t q0 = qcs(inv, v0);
    qcomplex_t q1 = qcs(inv, v1);
    qcomplex_t cq0 = qc_conj(q0);
    qcomplex_t cq1 = qc_conj(q1);
    qcomplex_t neg_cq1 = qc_sub(QC_ZERO, cq1);

    qcomplex_t hq01 = qc_add(qc_mul(a, neg_cq1), qc_mul(b, cq0));
    qcomplex_t hq11 = qc_add(qc_mul(c, neg_cq1), qc_mul(d, cq0));
    qcomplex_t t01 = qc_add(qc_mul(cq0, hq01), qc_mul(cq1, hq11));

    QCM(H, 0, 0, n) = lam1;
    QCM(H, 0, 1, n) = t01;
    QCM(H, 1, 0, n) = QC_ZERO;
    QCM(H, 1, 1, n) = lam2;

    for (size_t i = 0; i < n; ++i) {
        qcomplex_t qi0 = QCM(Q, i, 0, n);
        qcomplex_t qi1 = QCM(Q, i, 1, n);
        QCM(Q, i, 0, n) = qc_add(qc_mul(qi0, q0), qc_mul(qi1, q1));
        QCM(Q, i, 1, n) = qc_add(qc_mul(qi0, neg_cq1), qc_mul(qi1, cq0));
    }
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
        if (sz == 2) {
            qr_schur_2x2(H, Q, n);
            break;
        }
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
        matrix_t *V = mat_create_dense_with_elem(n, n, e);
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

    if (A->elem && A->elem->kind == ELEM_DVAL)
        return mat_eigendecompose_dval(A, (dval_t **)eigenvalues, eigenvectors);

    if (!elem_supports_numeric_algorithms(A->elem)) return -3;

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
    char *s = mat_to_string(A, MAT_STRING_LAYOUT_PRETTY);
    if (!s) {
        printf("(null)\n");
        return;
    }
    printf("%s\n", s);
    free(s);
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

    matrix_t *Z = mat_create_dense_with_elem(A->rows, A->cols, &qcomplex_elem);
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
        Q = mat_create_identity_with_elem(n, &qcomplex_elem);
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

        /* normalise v */
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

            /* Single-shift implicit QR step (bulge-chase Householder) */
            for (size_t k = 0; k < m; ++k) {

                /* k=0: initial shifted vector [H[0,0]-mu, H[1,0]]
                 * k>0: chase the bulge using [H[k,k-1], H[k+1,k-1]] (no shift) */
                qcomplex_t a, b;
                if (k == 0) {
                    qcomplex_t h00, h10;
                    mat_get(H, 0, 0, &h00);
                    mat_get(H, 1, 0, &h10);
                    a = qc_sub(h00, mu);
                    b = h10;
                } else {
                    mat_get(H, k,   k-1, &a);
                    mat_get(H, k+1, k-1, &b);
                }

                qfloat_t a_abs = qc_abs(a);
                qfloat_t b_abs = qc_abs(b);
                qfloat_t nrm2  = qf_add(qf_mul(a_abs, a_abs),
                                        qf_mul(b_abs, b_abs));
                if (qf_lt(nrm2, eps)) continue;

                qfloat_t nrm = qf_sqrt(nrm2);

                /* Householder vector: v = [a + (a/|a|)*nrm, b], then normalise */
                qcomplex_t v0;
                if (qf_lt(a_abs, eps)) {
                    v0 = qc_make(nrm, QF_ZERO);
                } else {
                    qcomplex_t a_phase = qc_scale(a, qf_div(QF_ONE, a_abs));
                    v0 = qc_add(a, qc_scale(a_phase, nrm));
                }
                qcomplex_t v1 = b;

                qfloat_t v0_abs = qc_abs(v0);
                qfloat_t vnrm2  = qf_add(qf_mul(v0_abs, v0_abs),
                                         qf_mul(b_abs,  b_abs));
                if (qf_lt(vnrm2, eps)) continue;
                qfloat_t inv = qf_div(QF_ONE, qf_sqrt(vnrm2));
                qcomplex_t u0 = qc_scale(v0, inv);
                qcomplex_t u1 = qc_scale(v1, inv);

                /* apply from left: H := (I - 2 u u*) H, rows k, k+1
                 * include column k-1 for k>0 to zero the bulge H[k+1,k-1] */
                size_t jstart = (k > 0) ? k - 1 : (size_t)0;
                for (size_t j = jstart; j < n; ++j) {
                    qcomplex_t hk0, hk1;
                    mat_get(H, k,   j, &hk0);
                    mat_get(H, k+1, j, &hk1);

                    qcomplex_t dot = qc_add(qc_mul(qc_conj(u0), hk0),
                                            qc_mul(qc_conj(u1), hk1));
                    dot = qc_add(dot, dot);

                    qcomplex_t t0 = qc_sub(hk0, qc_mul(u0, dot));
                    qcomplex_t t1 = qc_sub(hk1, qc_mul(u1, dot));
                    mat_set(H, k,   j, &t0);
                    mat_set(H, k+1, j, &t1);
                }

                /* apply from right: H := H (I - 2 u u*), cols k, k+1 */
                for (size_t i = 0; i < n; ++i) {
                    qcomplex_t hik0, hik1;
                    mat_get(H, i, k,   &hik0);
                    mat_get(H, i, k+1, &hik1);

                    qcomplex_t dot = qc_add(qc_mul(hik0, u0),
                                            qc_mul(hik1, u1));
                    dot = qc_add(dot, dot);

                    qcomplex_t t0 = qc_sub(hik0, qc_mul(dot, qc_conj(u0)));
                    qcomplex_t t1 = qc_sub(hik1, qc_mul(dot, qc_conj(u1)));
                    mat_set(H, i, k,   &t0);
                    mat_set(H, i, k+1, &t1);
                }

                /* accumulate into Q: Q := Q (I - 2 u u*), cols k, k+1 */
                for (size_t i = 0; i < n; ++i) {
                    qcomplex_t qik0, qik1;
                    mat_get(Q, i, k,   &qik0);
                    mat_get(Q, i, k+1, &qik1);

                    qcomplex_t dot = qc_add(qc_mul(qik0, u0),
                                            qc_mul(qik1, u1));
                    dot = qc_add(dot, dot);

                    qcomplex_t t0 = qc_sub(qik0, qc_mul(dot, qc_conj(u0)));
                    qcomplex_t t1 = qc_sub(qik1, qc_mul(dot, qc_conj(u1)));
                    mat_set(Q, i, k,   &t0);
                    mat_set(Q, i, k+1, &t1);
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
            return -1;
        }

        m--;
    }

    return 0;
}

/* ============================================================
   Public Schur API
   ============================================================ */

int mat_schur_factor(const matrix_t *A, mat_schur_factor_t *out)
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
    out->T = mat_convert_with_store(Z, &qcomplex_elem, &upper_triangular_store);
    mat_free(Z);
    if (!out->T) {
        mat_free(Q0);
        return -3;
    }
    return 0;
}

void mat_schur_factor_free(mat_schur_factor_t *S)
{
    if (!S) return;
    if (S->Q) mat_free(S->Q);
    if (S->T) mat_free(S->T);
    S->Q = S->T = NULL;
}

/* ============================================================
   Parlett recurrence on upper triangular T
   ============================================================ */

static void qc_fun_coeffs_up_to_second(qcomplex_t *c0,
                                       qcomplex_t *c1,
                                       qcomplex_t *c2,
                                       void (*scalar_f)(void *out, const void *in),
                                       qcomplex_t lambda)
{
    qcomplex_t f0;

    scalar_f(&f0, &lambda);
    if (c0)
        *c0 = f0;

    if (scalar_f == qcomplex_elem.fun->gamma) {
        qcomplex_t psi = qc_digamma(lambda);
        qcomplex_t tri = qc_trigamma(lambda);
        if (c1)
            *c1 = qc_mul(f0, psi);
        if (c2) {
            qcomplex_t second = qc_mul(f0, qc_add(tri, qc_mul(psi, psi)));
            *c2 = qc_mul(qc_make(qf_from_double(0.5), QF_ZERO), second);
        }
        return;
    }

    if (scalar_f == qcomplex_elem.fun->digamma) {
        if (c1)
            *c1 = qc_trigamma(lambda);
        if (c2)
            *c2 = qc_mul(qc_make(qf_from_double(0.5), QF_ZERO),
                         qc_tetragamma(lambda));
        return;
    }

    if (scalar_f == qcomplex_elem.fun->lambert_w0 ||
        scalar_f == qcomplex_elem.fun->lambert_wm1) {
        qcomplex_t one = qc_make(QF_ONE, QF_ZERO);
        qcomplex_t two = qc_make(qf_from_double(2.0), QF_ZERO);
        qcomplex_t wp1 = qc_add(one, f0);
        qcomplex_t lam2 = qc_mul(lambda, lambda);
        qcomplex_t denom1 = qc_mul(lambda, wp1);
        if (c1)
            *c1 = qc_div(f0, denom1);
        if (c2) {
            qcomplex_t numer = qc_neg(qc_mul(qc_mul(f0, f0), qc_add(f0, two)));
            qcomplex_t denom = qc_mul(lam2, qc_mul(wp1, qc_mul(wp1, wp1)));
            *c2 = qc_mul(qc_make(qf_from_double(0.5), QF_ZERO), qc_div(numer, denom));
        }
        return;
    }

    if (scalar_f == qcomplex_elem.fun->ei) {
        qcomplex_t exp_lambda = qc_exp(lambda);
        qcomplex_t one = qc_make(QF_ONE, QF_ZERO);
        qcomplex_t lam2 = qc_mul(lambda, lambda);
        if (c1)
            *c1 = qc_div(exp_lambda, lambda);
        if (c2) {
            qcomplex_t second = qc_div(qc_mul(exp_lambda, qc_sub(lambda, one)), lam2);
            *c2 = qc_mul(qc_make(qf_from_double(0.5), QF_ZERO), second);
        }
        return;
    }

    if (scalar_f == qcomplex_elem.fun->e1) {
        qcomplex_t one = qc_make(QF_ONE, QF_ZERO);
        qcomplex_t emlambda = qc_exp(qc_neg(lambda));
        qcomplex_t lam2 = qc_mul(lambda, lambda);
        if (c1)
            *c1 = qc_div(qc_neg(emlambda), lambda);
        if (c2) {
            qcomplex_t second = qc_div(qc_mul(emlambda, qc_add(lambda, one)), lam2);
            *c2 = qc_mul(qc_make(qf_from_double(0.5), QF_ZERO), second);
        }
        return;
    }

    if (scalar_f == qcomplex_elem.fun->erf) {
        qcomplex_t scale = qc_make(qf_div(qf_from_double(2.0), QF_SQRT_PI), QF_ZERO);
        qcomplex_t fp = qc_mul(scale, qc_exp(qc_neg(qc_mul(lambda, lambda))));
        if (c1)
            *c1 = fp;
        if (c2)
            *c2 = qc_neg(qc_mul(lambda, fp));
        return;
    }

    if (scalar_f == qcomplex_elem.fun->erfc) {
        qcomplex_t scale = qc_make(qf_div(qf_neg(qf_from_double(2.0)), QF_SQRT_PI), QF_ZERO);
        qcomplex_t fp = qc_mul(scale, qc_exp(qc_neg(qc_mul(lambda, lambda))));
        if (c1)
            *c1 = fp;
        if (c2)
            *c2 = qc_neg(qc_mul(lambda, fp));
        return;
    }

    if (scalar_f == qcomplex_elem.fun->normal_pdf) {
        qcomplex_t fp = qc_neg(qc_mul(lambda, f0));
        if (c1)
            *c1 = fp;
        if (c2) {
            qcomplex_t lambda2 = qc_mul(lambda, lambda);
            *c2 = qc_mul(qc_make(qf_from_double(0.5), QF_ZERO),
                         qc_mul(qc_sub(lambda2, qc_make(QF_ONE, QF_ZERO)), f0));
        }
        return;
    }

    if (scalar_f == qcomplex_elem.fun->normal_cdf) {
        qcomplex_t pdf = qc_normal_pdf(lambda);
        if (c1)
            *c1 = pdf;
        if (c2)
            *c2 = qc_mul(qc_make(qf_from_double(-0.5), QF_ZERO), qc_mul(lambda, pdf));
        return;
    }

    if (scalar_f == qcomplex_elem.fun->normal_logpdf) {
        if (c1)
            *c1 = qc_neg(lambda);
        if (c2)
            *c2 = qc_make(qf_from_double(-0.5), QF_ZERO);
        return;
    }

    {
        qfloat_t h = qf_from_double(1e-6);
        qcomplex_t ih = qc_make(QF_ZERO, h);
        qcomplex_t fp, fm, lambda_p, lambda_m;
        qcomplex_t denom1 = qc_make(QF_ZERO, qf_mul_double(h, 2.0));
        qcomplex_t denom2 = qc_make(qf_mul_double(qf_mul(h, h), -2.0), QF_ZERO);
        qcomplex_t two_f0 = qc_mul(qc_make(qf_from_double(2.0), QF_ZERO), f0);

        lambda_p = qc_add(lambda, ih);
        lambda_m = qc_sub(lambda, ih);
        scalar_f(&fp, &lambda_p);
        scalar_f(&fm, &lambda_m);

        if (c1)
            *c1 = qc_div(qc_sub(fp, fm), denom1);
        if (c2)
            *c2 = qc_div(qc_add(qc_sub(fp, two_f0), fm), denom2);
    }
}

static matrix_t *mat_fun_triangular_equal_diag(const matrix_t *T,
                                               void (*scalar_f)(void *out, const void *in))
{
    size_t n = T->rows;
    matrix_t *F = mat_create_upper_triangular_with_elem(n, n, &qcomplex_elem);
    matrix_t *N = mat_create_upper_triangular_with_elem(n, n, &qcomplex_elem);
    if (!F || !N) {
        mat_free(F);
        mat_free(N);
        return NULL;
    }

    qcomplex_t lambda, c0, c1 = QC_ZERO, c2 = QC_ZERO;
    mat_get(T, 0, 0, &lambda);
    qc_fun_coeffs_up_to_second(&c0, &c1, &c2, scalar_f, lambda);

    for (size_t i = 0; i < n; ++i) {
        mat_set(F, i, i, &c0);
        for (size_t j = i; j < n; ++j) {
            qcomplex_t tij;
            mat_get(T, i, j, &tij);
            if (i == j)
                tij = QC_ZERO;
            mat_set(N, i, j, &tij);
        }
    }

    if (n >= 2) {
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                qcomplex_t nij, term;
                mat_get(N, i, j, &nij);
                term = qc_mul(c1, nij);
                mat_set(F, i, j, &term);
            }
        }
    }

    if (n >= 3) {
        matrix_t *N2 = mat_mul(N, N);
        if (!N2) {
            mat_free(F);
            mat_free(N);
            return NULL;
        }

        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 2; j < n; ++j) {
                qcomplex_t fij, n2ij;
                mat_get(F, i, j, &fij);
                mat_get(N2, i, j, &n2ij);
                fij = qc_add(fij, qc_mul(c2, n2ij));
                mat_set(F, i, j, &fij);
            }
        }

        mat_free(N2);
    }

    mat_free(N);
    return F;
}

matrix_t *mat_fun_triangular(const matrix_t *T,
                             void (*scalar_f)(void *out, const void *in))
{
    if (!T || !scalar_f)
        return NULL;

    if (T->rows != T->cols)
        return NULL;

    size_t n = T->rows;
    const struct elem_vtable *e = T->elem;

    unsigned char t_ii[64] = {0}, t_jj[64] = {0}, t_ij[64] = {0};
    unsigned char f_ii[64] = {0}, f_jj[64] = {0}, f_ij[64] = {0};
    unsigned char num[64] = {0}, tmp[64] = {0}, sum[64] = {0};
    unsigned char t_ik[64] = {0}, t_kj[64] = {0}, f_ik[64] = {0}, f_kj[64] = {0};
    unsigned char denom[64] = {0}, inv_denom[64] = {0};

    {
        int all_diag_equal = 1;
        qcomplex_t lam0, lami;
        qfloat_t tol = qf_from_double(1e-24);
        unsigned char lam0_raw[64] = {0}, lami_raw[64] = {0};
        mat_get(T, 0, 0, lam0_raw);
        e->to_qc(&lam0, lam0_raw);
        for (size_t i = 1; i < n; ++i) {
            mat_get(T, i, i, lami_raw);
            e->to_qc(&lami, lami_raw);
            if (qf_lt(tol, qc_abs(qc_sub(lami, lam0)))) {
                all_diag_equal = 0;
                break;
            }
        }
        if (all_diag_equal)
            return mat_fun_triangular_equal_diag(T, scalar_f);
    }

    matrix_t *F = mat_create_upper_triangular_with_elem(n, n, e);
    if (!F)
        return NULL;

    /* 1. Diagonal: f(T_ii) */
    for (size_t i = 0; i < n; ++i) {
        mat_get(T, i, i, t_ii);
        scalar_f(f_ii, t_ii);
        mat_set(F, i, i, f_ii);
        elem_destroy_value(e, f_ii);
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

            /* num = T_ij * (F_jj - F_ii) + sum */
            mat_get(T, i, j, t_ij);
            mat_get(F, i, i, f_ii);
            mat_get(F, j, j, f_jj);

            e->sub(tmp, f_jj, f_ii);      /* tmp = F_jj - F_ii */
            e->mul(num, t_ij, tmp);       /* num = T_ij * (F_jj - F_ii) */
            e->add(num, num, sum);        /* num += sum */

            /* Handle T_ii == T_jj: avoid 0/0 */
            if (e->cmp(denom, e->zero) == 0) {
                if (e->cmp(num, e->zero) == 0) {
                    /* F[i,j] = f'(lambda) * T[i,j] via central difference */
                    unsigned char lam_p[64] = {0}, lam_m[64] = {0};
                    unsigned char fp[64] = {0}, fm[64] = {0};
                    unsigned char h_raw[64] = {0}, two_h[64] = {0};
                    unsigned char inv_2h[64] = {0}, deriv[64] = {0};
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
                    elem_destroy_value(e, lam_p);
                    elem_destroy_value(e, lam_m);
                    elem_destroy_value(e, fp);
                    elem_destroy_value(e, fm);
                    elem_destroy_value(e, h_raw);
                    elem_destroy_value(e, two_h);
                    elem_destroy_value(e, inv_2h);
                    elem_destroy_value(e, deriv);
                    elem_destroy_value(e, f_ij);
                    elem_destroy_value(e, denom);
                    elem_destroy_value(e, num);
                    elem_destroy_value(e, tmp);
                    elem_destroy_value(e, sum);
                    continue;
                } else {
                    /* TODO: full confluent Parlett (higher derivatives needed) */
                    memcpy(f_ij, e->zero, e->size);
                    mat_set(F, i, j, f_ij);
                    elem_destroy_value(e, f_ij);
                    elem_destroy_value(e, denom);
                    elem_destroy_value(e, num);
                    elem_destroy_value(e, tmp);
                    elem_destroy_value(e, sum);
                    continue;
                }
            }

            /* F_ij = num / (T_ii - T_jj) = num * inv(denom) */
            e->inv(inv_denom, denom);
            e->mul(f_ij, num, inv_denom);
            mat_set(F, i, j, f_ij);
            elem_destroy_value(e, inv_denom);
            elem_destroy_value(e, f_ij);
            elem_destroy_value(e, denom);
            elem_destroy_value(e, num);
            elem_destroy_value(e, tmp);
            elem_destroy_value(e, sum);
        }
    }

    return F;
}
