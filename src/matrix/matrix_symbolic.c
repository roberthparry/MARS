#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "matrix_internal.h"
#include "internal/expr_internal.h"

static bool expr_node_is_exact_zero(const expr_t *dv)
{
    return !dv || expr_is_exact_zero(dv);
}

typedef struct {
    matrix_t *R;
    size_t *pivot_cols;
    bool *is_pivot;
    size_t rank;
} expr_rref_info_t;

int mat_simplify_symbolic_inplace(matrix_t *A)
{
    expr_t *dv = NULL;
    expr_t *simp = NULL;

    if (!A)
        return -1;
    if (A->elem != &expr_elem)
        return 0;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            dv = NULL;
            simp = NULL;
            mat_get(A, i, j, &dv);
            if (!dv)
                continue;
            simp = expr_simplify(dv);
            if (!simp)
                return -1;
            mat_set(A, i, j, &simp);
            expr_free(simp);
        }
    }

    return 0;
}

static expr_t *expr_det2_simplify(expr_t *a, expr_t *b, expr_t *c, expr_t *d)
{
    expr_t *left = NULL, *right = NULL;

    if (a)
        expr_retain(a);
    if (d)
        expr_retain(d);
    left = expr_mul_simplify_owned(a, d);

    if (b)
        expr_retain(b);
    if (c)
        expr_retain(c);
    right = expr_mul_simplify_owned(b, c);

    return expr_sub_simplify_owned(left, right);
}

int mat_det_expr_exact(const matrix_t *A, expr_t **determinant);

static const expr_t *mat_get_expr_or_zero(const matrix_t *A, size_t i, size_t j)
{
    expr_t *v = NULL;

    mat_get(A, i, j, &v);
    return v ? v : EXPR_ZERO;
}

static bool expr_exprs_equal_exact(const expr_t *a, const expr_t *b)
{
    expr_t *diff;
    bool equal;

    if (a == b)
        return true;
    if (!a || !b)
        return false;

    expr_retain((expr_t *)a);
    expr_retain((expr_t *)b);
    diff = expr_sub_simplify_owned((expr_t *)a, (expr_t *)b);
    if (!diff)
        return false;

    equal = expr_is_exact_zero(diff);
    expr_free(diff);
    return equal;
}

static int mat_eigenvalues_expr_symbolic(const matrix_t *A, expr_t **eigenvalues)
{
    if (!A || !eigenvalues)
        return -1;
    if (A->rows != A->cols)
        return -2;

    if (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) {
        for (size_t i = 0; i < A->rows; ++i) {
            expr_t *diag = NULL;

            mat_get(A, i, i, &diag);
            eigenvalues[i] = expr_clone_for_storage(diag ? diag : EXPR_ZERO);
            if (!eigenvalues[i])
                goto fail;
        }
        return 0;
    }

    if (A->rows == 2) {
        const expr_t *a = mat_get_expr_or_zero(A, 0, 0);
        const expr_t *b = mat_get_expr_or_zero(A, 0, 1);
        const expr_t *c = mat_get_expr_or_zero(A, 1, 0);
        const expr_t *d = mat_get_expr_or_zero(A, 1, 1);
        expr_t *sum = NULL, *diff = NULL, *diff2 = NULL;
        expr_t *bc = NULL, *scaled_bc = NULL, *disc = NULL, *root = NULL;
        expr_t *plus = NULL, *minus = NULL, *half = NULL;

        if (a)
            expr_retain(a);
        if (d)
            expr_retain(d);
        sum = expr_add_simplify_owned(a, d);
        a = d = NULL;
        if (!sum)
            goto fail_2x2;

        a = mat_get_expr_or_zero(A, 0, 0);
        d = mat_get_expr_or_zero(A, 1, 1);
        if (a)
            expr_retain(a);
        if (d)
            expr_retain(d);
        diff = expr_sub_simplify_owned(a, d);
        a = d = NULL;
        if (!diff)
            goto fail_2x2;

        expr_retain(diff);
        diff2 = expr_mul_simplify_owned(diff, diff);
        diff = NULL;
        if (!diff2)
            goto fail_2x2;

        if (b)
            expr_retain(b);
        if (c)
            expr_retain(c);
        bc = expr_mul_simplify_owned(b, c);
        if (!bc)
            goto fail_2x2;

        {
            number_t four = num_create_from_long(4);
            scaled_bc = expr_mul_simplify_owned(expr_new_const(four), bc);
            num_destroy(&four);
        }
        bc = NULL;
        if (!scaled_bc)
            goto fail_2x2;

        disc = expr_add_simplify_owned(diff2, scaled_bc);
        diff2 = NULL;
        scaled_bc = NULL;
        if (!disc)
            goto fail_2x2;

        {
            expr_t *raw_root = expr_sqrt(disc);

            expr_free(disc);
            disc = NULL;
            root = expr_simplify_owned(raw_root);
        }
        if (!root)
            goto fail_2x2;

        expr_retain(sum);
        expr_retain(root);
        plus = expr_add_simplify_owned(sum, root);
        if (!plus)
            goto fail_2x2;

        minus = expr_sub_simplify_owned(sum, root);
        sum = NULL;
        root = NULL;
        if (!minus)
            goto fail_2x2;

        {
            number_t two = num_create_from_long(2);
            number_t half_num = num_div(NUM_ONE, two);
            half = expr_new_const(half_num);
            num_destroy(&two);
            num_destroy(&half_num);
        }
        if (!half)
            goto fail_2x2;

        expr_retain(half);
        eigenvalues[0] = expr_mul_simplify_owned(half, plus);
        plus = NULL;
        if (!eigenvalues[0])
            goto fail_2x2;

        expr_retain(half);
        eigenvalues[1] = expr_mul_simplify_owned(half, minus);
        minus = NULL;
        expr_free(half);
        half = NULL;
        if (!eigenvalues[1])
            goto fail_2x2;

        return 0;

fail_2x2:
        expr_free((expr_t *)a);
        expr_free((expr_t *)b);
        expr_free((expr_t *)c);
        expr_free((expr_t *)d);
        expr_free(sum);
        expr_free(diff);
        expr_free(diff2);
        expr_free(bc);
        expr_free(scaled_bc);
        expr_free(disc);
        expr_free(root);
        expr_free(plus);
        expr_free(minus);
        expr_free(half);
        for (size_t i = 0; i < A->rows; ++i) {
            expr_free(eigenvalues[i]);
            eigenvalues[i] = NULL;
        }
        return -3;
    }

    return -3;

fail:
    for (size_t i = 0; i < A->rows; ++i) {
        expr_free(eigenvalues[i]);
        eigenvalues[i] = NULL;
    }
    return -3;
}

static matrix_t *mat_eigenvectors_expr_triangular(const matrix_t *A)
{
    matrix_t *V;
    bool upper;

    if (!A || A->rows != A->cols)
        return NULL;

    if (mat_is_diagonal(A))
        return mat_create_identity_expr(A->rows);

    upper = mat_is_upper_triangular(A);
    if (!upper && !mat_is_lower_triangular(A))
        return NULL;

    V = mat_create_identity_expr(A->rows);
    if (!V)
        return NULL;

    if (upper) {
        for (size_t k = 0; k < A->rows; ++k) {
            const expr_t *lambda = mat_get_expr_or_zero(A, k, k);

            for (size_t ii = k; ii-- > 0;) {
                expr_t *sum = expr_new_const(NUM_ZERO);
                expr_t *denom;
                expr_t *x;

                if (!sum)
                    goto fail;

                for (size_t j = ii + 1; j <= k; ++j) {
                    const expr_t *aij = mat_get_expr_or_zero(A, ii, j);
                    const expr_t *xjk = mat_get_expr_or_zero(V, j, k);
                    expr_t *term;

                    expr_retain(aij);
                    expr_retain(xjk);
                    term = expr_mul_simplify_owned(aij, xjk);
                    sum = expr_add_simplify_owned(sum, term);
                    if (!sum)
                        goto fail;
                }

                {
                    const expr_t *diag = mat_get_expr_or_zero(A, ii, ii);

                    expr_retain(diag);
                    expr_retain(lambda);
                    denom = expr_sub_simplify_owned(diag, lambda);
                }
                if (!denom) {
                    expr_free((expr_t *)sum);
                    goto fail;
                }

                if (expr_is_exact_zero(denom)) {
                    expr_free(denom);
                    if (expr_is_exact_zero(sum)) {
                        expr_free(sum);
                        continue;
                    }
                    expr_free((expr_t *)sum);
                    goto fail;
                }

                x = expr_negate_owned(sum);
                denom = expr_simplify_owned(denom);
                if (!x) {
                    expr_free(denom);
                    goto fail;
                }

                x = expr_div_simplify_owned(x, denom);
                if (!x)
                    goto fail;

                mat_set(V, ii, k, &x);
                expr_free(x);
            }
        }
    } else {
        for (size_t k = 0; k < A->rows; ++k) {
            const expr_t *lambda = mat_get_expr_or_zero(A, k, k);

            for (size_t i = k + 1; i < A->rows; ++i) {
                expr_t *sum = expr_new_const(NUM_ZERO);
                expr_t *denom;
                expr_t *x;

                if (!sum)
                    goto fail;

                for (size_t j = k; j < i; ++j) {
                    const expr_t *aij = mat_get_expr_or_zero(A, i, j);
                    const expr_t *xjk = mat_get_expr_or_zero(V, j, k);
                    expr_t *term;

                    expr_retain(aij);
                    expr_retain(xjk);
                    term = expr_mul_simplify_owned(aij, xjk);
                    sum = expr_add_simplify_owned(sum, term);
                    if (!sum)
                        goto fail;
                }

                {
                    const expr_t *diag = mat_get_expr_or_zero(A, i, i);

                    expr_retain(diag);
                    expr_retain(lambda);
                    denom = expr_sub_simplify_owned(diag, lambda);
                }
                if (!denom) {
                    expr_free((expr_t *)sum);
                    goto fail;
                }

                if (expr_is_exact_zero(denom)) {
                    expr_free(denom);
                    if (expr_is_exact_zero(sum)) {
                        expr_free(sum);
                        continue;
                    }
                    expr_free((expr_t *)sum);
                    goto fail;
                }

                x = expr_negate_owned(sum);
                denom = expr_simplify_owned(denom);
                if (!x) {
                    expr_free(denom);
                    goto fail;
                }

                x = expr_div_simplify_owned(x, denom);
                if (!x)
                    goto fail;

                mat_set(V, i, k, &x);
                expr_free(x);
            }
        }
    }

    return V;

fail:
    mat_free(V);
    return NULL;
}

static matrix_t *mat_eigenvectors_expr_2x2(const matrix_t *A, expr_t **eigenvalues)
{
    matrix_t *V;
    expr_t *local_ev[2] = {NULL, NULL};
    expr_t **ev = eigenvalues ? eigenvalues : local_ev;
    const expr_t *a;
    const expr_t *b;
    const expr_t *c;
    const expr_t *d;

    if (!A || A->rows != 2 || A->cols != 2)
        return NULL;

    a = mat_get_expr_or_zero(A, 0, 0);
    b = mat_get_expr_or_zero(A, 0, 1);
    c = mat_get_expr_or_zero(A, 1, 0);
    d = mat_get_expr_or_zero(A, 1, 1);

    if (expr_node_is_exact_zero(b) && expr_node_is_exact_zero(c) && expr_exprs_equal_exact(a, d)) {
        if (!eigenvalues && mat_eigenvalues_expr_symbolic(A, ev) != 0)
            return NULL;
        if (!eigenvalues) {
            expr_free(ev[0]);
            expr_free(ev[1]);
        }
        return mat_create_identity_expr(2);
    }

    if (!eigenvalues && mat_eigenvalues_expr_symbolic(A, ev) != 0)
        return NULL;

    V = mat_new_expr(2, 2);
    if (!V)
        goto fail;

    for (size_t k = 0; k < 2; ++k) {
        expr_t *lambda = ev[k];
        expr_t *p;
        expr_t *q;
        expr_t *r;
        expr_t *s;
        expr_t *v0 = NULL;
        expr_t *v1 = NULL;

        expr_retain(a);
        expr_retain(lambda);
        p = expr_sub_simplify_owned(a, lambda);
        q = expr_clone_for_storage(b);
        r = expr_clone_for_storage(c);
        expr_retain(d);
        expr_retain(lambda);
        s = expr_sub_simplify_owned(d, lambda);
        if (!p || !q || !r || !s) {
            expr_free(p);
            expr_free(q);
            expr_free(r);
            expr_free(s);
            goto fail;
        }

        if (!expr_is_exact_zero(p) || !expr_is_exact_zero(q)) {
            v0 = q;
            q = NULL;
            expr_retain(lambda);
            expr_retain(a);
            v1 = expr_sub_simplify_owned(lambda, a);
        } else if (!expr_is_exact_zero(r) || !expr_is_exact_zero(s)) {
            expr_retain(lambda);
            expr_retain(d);
            v0 = expr_sub_simplify_owned(lambda, d);
            v1 = r;
            r = NULL;
        } else if (k == 0) {
            v0 = expr_clone_for_storage(EXPR_ONE);
            v1 = expr_clone_for_storage(EXPR_ZERO);
        } else {
            v0 = expr_clone_for_storage(EXPR_ZERO);
            v1 = expr_clone_for_storage(EXPR_ONE);
        }

        expr_free(p);
        expr_free(q);
        expr_free(r);
        expr_free(s);

        if (!v0 || !v1) {
            expr_free(v0);
            expr_free(v1);
            goto fail;
        }

        mat_set(V, 0, k, &v0);
        mat_set(V, 1, k, &v1);
        expr_free(v0);
        expr_free(v1);
    }

    if (!eigenvalues) {
        expr_free(ev[0]);
        expr_free(ev[1]);
    }
    return V;

fail:
    if (!eigenvalues) {
        expr_free(ev[0]);
        expr_free(ev[1]);
    }
    mat_free(V);
    return NULL;
}

static int mat_eigendecompose_expr_symbolic(const matrix_t *A, expr_t **eigenvalues, matrix_t **eigenvectors)
{
    matrix_t *V = NULL;
    expr_t **ev = eigenvalues;
    expr_t *local_ev_stack[2] = {NULL, NULL};
    expr_t **local_ev_heap = NULL;
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

    rc = mat_eigenvalues_expr_symbolic(A, ev);
    if (rc != 0)
        goto cleanup;

    if (!eigenvectors)
        goto success;

    if (mat_is_diagonal(A) || mat_is_upper_triangular(A) || mat_is_lower_triangular(A))
        V = mat_eigenvectors_expr_triangular(A);
    else if (A->rows == 2)
        V = mat_eigenvectors_expr_2x2(A, ev);

    if (!V) {
        for (size_t i = 0; i < A->rows; ++i) {
            expr_free(ev[i]);
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
            expr_free(ev[i]);
    }
    free(local_ev_heap);
    return rc;
}

int mat_eigenvalues_expr(const matrix_t *A, expr_t **eigenvalues)
{
    return mat_eigenvalues_expr_symbolic(A, eigenvalues);
}

int mat_eigendecompose_expr(const matrix_t *A, expr_t **eigenvalues, matrix_t **eigenvectors)
{
    return mat_eigendecompose_expr_symbolic(A, eigenvalues, eigenvectors);
}

static matrix_t *mat_solve_expr_diagonal_exact(const matrix_t *A, const matrix_t *B)
{
    matrix_t *X;

    if (!A || !B || A->rows != A->cols || A->rows != B->rows)
        return NULL;

    X = mat_create_direct_solve_result(A, B, &expr_elem);
    if (!X)
        return NULL;

    for (size_t i = 0; i < A->rows; ++i) {
        const expr_t *diag = mat_get_expr_or_zero(A, i, i);

        if (!diag || expr_is_exact_zero(diag))
            goto fail;

        for (size_t j = 0; j < B->cols; ++j) {
            const expr_t *rhs = mat_get_expr_or_zero(B, i, j);
            expr_t *out = NULL;

            expr_retain(rhs);
            expr_retain(diag);
            out = expr_div_simplify_owned(rhs, diag);
            if (!out)
                goto fail;
            mat_set(X, i, j, &out);
            expr_free(out);
        }
    }

    return X;

fail:
    mat_free(X);
    return NULL;
}

static expr_t *expr_bareiss_update_simplify(const expr_t *left_a,
                                            const expr_t *left_b,
                                            const expr_t *right_a,
                                            const expr_t *right_b,
                                            const expr_t *divisor,
                                            bool divide)
{
    expr_t *lhs = NULL;
    expr_t *rhs = NULL;
    expr_t *num = NULL;
    expr_t *out = NULL;

    if (left_a)
        expr_retain((expr_t *)left_a);
    if (left_b)
        expr_retain((expr_t *)left_b);
    lhs = expr_mul_simplify_owned((expr_t *)left_a, (expr_t *)left_b);
    if (!lhs)
        return NULL;

    if (right_a)
        expr_retain((expr_t *)right_a);
    if (right_b)
        expr_retain((expr_t *)right_b);
    rhs = expr_mul_simplify_owned((expr_t *)right_a, (expr_t *)right_b);
    if (!rhs) {
        expr_free(lhs);
        return NULL;
    }

    num = expr_sub_simplify_owned(lhs, rhs);
    if (!num)
        return NULL;

    if (!divide)
        return num;

    if (divisor)
        expr_retain((expr_t *)divisor);
    out = expr_div_simplify_owned(num, (expr_t *)divisor);
    return out;
}

static int mat_fraction_free_eliminate_expr(matrix_t *M,
                                            matrix_t *RHS,
                                            bool *negate_out)
{
    size_t n;
    const expr_t *prev_pivot = EXPR_ONE;
    bool negate = false;

    if (!M || M->elem != &expr_elem || M->rows != M->cols)
        return -1;
    if (RHS && (RHS->elem != &expr_elem || RHS->rows != M->rows))
        return -1;

    n = M->rows;
    if (negate_out)
        *negate_out = false;

    for (size_t k = 0; k + 1 < n; ++k) {
        size_t pivot_row = n;
        const expr_t *pivot = NULL;

        for (size_t i = k; i < n; ++i) {
            const expr_t *candidate = mat_get_expr_or_zero(M, i, k);
            if (!expr_node_is_exact_zero(candidate)) {
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

        pivot = mat_get_expr_or_zero(M, k, k);
        if (!pivot || expr_node_is_exact_zero(pivot))
            return 1;

        for (size_t i = k + 1; i < n; ++i) {
            const expr_t *aik = mat_get_expr_or_zero(M, i, k);

            if (!aik || expr_node_is_exact_zero(aik)) {
                const expr_t *zero = EXPR_ZERO;
                mat_set(M, i, k, &zero);
                continue;
            }

            for (size_t j = k + 1; j < n; ++j) {
                const expr_t *aij = mat_get_expr_or_zero(M, i, j);
                const expr_t *akj = mat_get_expr_or_zero(M, k, j);
                expr_t *next = expr_bareiss_update_simplify(aij, pivot,
                                                            aik, akj,
                                                            prev_pivot,
                                                            k > 0);

                if (!next)
                    return -1;
                mat_set(M, i, j, &next);
                expr_free(next);
            }

            if (RHS) {
                for (size_t j = 0; j < RHS->cols; ++j) {
                    const expr_t *bij = mat_get_expr_or_zero(RHS, i, j);
                    const expr_t *bkj = mat_get_expr_or_zero(RHS, k, j);
                    expr_t *next = expr_bareiss_update_simplify(bij, pivot,
                                                                aik, bkj,
                                                                prev_pivot,
                                                                k > 0);

                    if (!next)
                        return -1;
                    mat_set(RHS, i, j, &next);
                    expr_free(next);
                }
            }

            {
                const expr_t *zero = EXPR_ZERO;
                mat_set(M, i, k, &zero);
            }
        }

        prev_pivot = pivot;
    }

    if (negate_out)
        *negate_out = negate;
    return 0;
}

static matrix_t *mat_forward_substitute_expr_exact(const matrix_t *L,
                                                   const matrix_t *B)
{
    matrix_t *X;

    if (!L || !B || L->rows != L->cols || L->rows != B->rows)
        return NULL;

    X = mat_create_dense_with_elem(L->cols, B->cols, &expr_elem);
    if (!X)
        return NULL;

    for (size_t i = 0; i < L->rows; ++i) {
        const expr_t *diag = mat_get_expr_or_zero(L, i, i);

        if (!diag || expr_is_exact_zero(diag))
            goto fail;

        for (size_t j = 0; j < B->cols; ++j) {
            const expr_t *sum = mat_get_expr_or_zero(B, i, j);
            expr_t *out = NULL;

            expr_retain(sum);

            for (size_t k = 0; k < i; ++k) {
                const expr_t *a = mat_get_expr_or_zero(L, i, k);
                const expr_t *x = mat_get_expr_or_zero(X, k, j);
                expr_t *prod = NULL;
                expr_t *new_sum = NULL;

                expr_retain(a);
                expr_retain(x);
                prod = expr_mul_simplify_owned(a, x);
                if (!prod) {
                    expr_free((expr_t *)sum);
                    goto fail;
                }

                new_sum = expr_sub_simplify_owned(sum, prod);
                if (!new_sum)
                    goto fail;
                sum = new_sum;
            }

            expr_retain(diag);
            out = expr_div_simplify_owned(sum, diag);
            if (!out)
                goto fail;
            mat_set(X, i, j, &out);
            expr_free(out);
        }
    }

    return X;

fail:
    mat_free(X);
    return NULL;
}

static matrix_t *mat_backward_substitute_expr_exact(const matrix_t *U,
                                                    const matrix_t *B)
{
    matrix_t *X;

    if (!U || !B || U->rows != U->cols || U->rows != B->rows)
        return NULL;

    X = mat_create_dense_with_elem(U->cols, B->cols, &expr_elem);
    if (!X)
        return NULL;

    for (size_t ii = U->rows; ii-- > 0;) {
        const expr_t *diag = mat_get_expr_or_zero(U, ii, ii);

        if (!diag || expr_is_exact_zero(diag))
            goto fail;

        for (size_t j = 0; j < B->cols; ++j) {
            const expr_t *sum = mat_get_expr_or_zero(B, ii, j);
            expr_t *out = NULL;

            expr_retain(sum);

            for (size_t k = ii + 1; k < U->cols; ++k) {
                const expr_t *a = mat_get_expr_or_zero(U, ii, k);
                const expr_t *x = mat_get_expr_or_zero(X, k, j);
                expr_t *prod = NULL;
                expr_t *new_sum = NULL;

                expr_retain(a);
                expr_retain(x);
                prod = expr_mul_simplify_owned(a, x);
                if (!prod) {
                    expr_free((expr_t *)sum);
                    goto fail;
                }

                new_sum = expr_sub_simplify_owned(sum, prod);
                if (!new_sum)
                    goto fail;
                sum = new_sum;
            }

            expr_retain(diag);
            out = expr_div_simplify_owned(sum, diag);
            if (!out)
                goto fail;
            mat_set(X, ii, j, &out);
            expr_free(out);
        }
    }

    return X;

fail:
    mat_free(X);
    return NULL;
}

static matrix_t *mat_solve_expr_dense_exact(const matrix_t *A, const matrix_t *B)
{
    size_t n;
    matrix_t *M = NULL;
    matrix_t *X = NULL;

    if (!A || !B || A->rows != A->cols || A->rows != B->rows)
        return NULL;

    n = A->rows;
    M = mat_create_dense_with_elem(n, n, &expr_elem);
    X = mat_create_dense_with_elem(B->rows, B->cols, &expr_elem);
    if (!M || !X)
        goto fail;

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            const expr_t *v = mat_get_expr_or_zero(A, i, j);
            mat_set(M, i, j, &v);
        }
        for (size_t j = 0; j < B->cols; ++j) {
            const expr_t *v = mat_get_expr_or_zero(B, i, j);
            mat_set(X, i, j, &v);
        }
    }

    if (mat_fraction_free_eliminate_expr(M, X, NULL) != 0)
        goto fail;

    for (size_t ii = n; ii-- > 0;) {
        const expr_t *diag = mat_get_expr_or_zero(M, ii, ii);

        if (!diag || expr_node_is_exact_zero(diag))
            goto fail;

        for (size_t j = 0; j < X->cols; ++j) {
            const expr_t *sum = mat_get_expr_or_zero(X, ii, j);
            expr_t *out = NULL;

            expr_retain(sum);

            for (size_t k = ii + 1; k < n; ++k) {
                const expr_t *uik = mat_get_expr_or_zero(M, ii, k);
                const expr_t *xkj = mat_get_expr_or_zero(X, k, j);
                expr_t *prod = NULL;
                expr_t *new_sum = NULL;

                expr_retain(uik);
                expr_retain(xkj);
                prod = expr_mul_simplify_owned(uik, xkj);
                if (!prod) {
                    expr_free((expr_t *)sum);
                    goto fail;
                }

                new_sum = expr_sub_simplify_owned(sum, prod);
                if (!new_sum)
                    goto fail;
                sum = new_sum;
            }

            expr_retain(diag);
            out = expr_div_simplify_owned(sum, diag);
            if (!out)
                goto fail;
            mat_set(X, ii, j, &out);
            expr_free(out);
        }
    }

    mat_free(M);
    return X;

fail:
    mat_free(M);
    mat_free(X);
    return NULL;
}

static matrix_t *mat_solve_expr_2x2_exact(const matrix_t *A, const matrix_t *B)
{
    const expr_t *a;
    const expr_t *b;
    const expr_t *c;
    const expr_t *d;
    expr_t *det = NULL;
    matrix_t *X = NULL;

    if (!A || !B || A->rows != 2 || A->cols != 2 || B->rows != 2)
        return NULL;

    a = mat_get_expr_or_zero(A, 0, 0);
    b = mat_get_expr_or_zero(A, 0, 1);
    c = mat_get_expr_or_zero(A, 1, 0);
    d = mat_get_expr_or_zero(A, 1, 1);

    det = expr_det2_simplify((expr_t *)a, (expr_t *)b,
                             (expr_t *)c, (expr_t *)d);
    if (!det || expr_node_is_exact_zero(det))
        goto fail;

    X = mat_create_dense_with_elem(2, B->cols, &expr_elem);
    if (!X)
        goto fail;

    for (size_t j = 0; j < B->cols; ++j) {
        const expr_t *r0 = mat_get_expr_or_zero(B, 0, j);
        const expr_t *r1 = mat_get_expr_or_zero(B, 1, j);
        expr_t *x0_left = NULL;
        expr_t *x0_right = NULL;
        expr_t *x0_num = NULL;
        expr_t *x0 = NULL;
        expr_t *x1_left = NULL;
        expr_t *x1_right = NULL;
        expr_t *x1_num = NULL;
        expr_t *x1 = NULL;

        expr_retain((expr_t *)d);
        expr_retain((expr_t *)r0);
        x0_left = expr_mul_simplify_owned(d, r0);
        expr_retain((expr_t *)b);
        expr_retain((expr_t *)r1);
        x0_right = expr_mul_simplify_owned(b, r1);
        x0_num = expr_sub_simplify_owned(x0_left, x0_right);
        expr_retain(det);
        x0 = expr_div_simplify_owned(x0_num, det);
        if (!x0)
            goto fail;
        mat_set(X, 0, j, &x0);
        expr_free(x0);

        expr_retain((expr_t *)a);
        expr_retain((expr_t *)r1);
        x1_left = expr_mul_simplify_owned(a, r1);
        expr_retain((expr_t *)c);
        expr_retain((expr_t *)r0);
        x1_right = expr_mul_simplify_owned(c, r0);
        x1_num = expr_sub_simplify_owned(x1_left, x1_right);
        expr_retain(det);
        x1 = expr_div_simplify_owned(x1_num, det);
        if (!x1)
            goto fail;
        mat_set(X, 1, j, &x1);
        expr_free(x1);
    }

    expr_free(det);
    return X;

fail:
    expr_free(det);
    mat_free(X);
    return NULL;
}

matrix_t *mat_solve_expr_exact(const matrix_t *A, const matrix_t *B)
{
    matrix_t *X = NULL;

    if (!A || !B || A->rows != A->cols || A->rows != B->rows)
        return NULL;

    if (A->rows == 2 && A->cols == 2)
        X = mat_solve_expr_2x2_exact(A, B);
    else if (mat_has_diagonal_structure(A))
        X = mat_solve_expr_diagonal_exact(A, B);
    else if (mat_has_lower_triangular_structure(A))
        X = mat_forward_substitute_expr_exact(A, B);
    else if (mat_has_upper_triangular_structure(A))
        X = mat_backward_substitute_expr_exact(A, B);
    else
        X = mat_solve_expr_dense_exact(A, B);

    return mat_finalize_symbolic_result(X);
}

static matrix_t *mat_inverse_expr_upper_exact(const matrix_t *A)
{
    size_t n;
    matrix_t *I = NULL;

    if (!A || A->rows != A->cols)
        return NULL;

    n = A->rows;
    I = mat_create_upper_triangular_with_elem(n, n, &expr_elem);
    if (!I)
        return NULL;

    for (size_t ii = n; ii-- > 0; ) {
        expr_t *uii = NULL;
        expr_t *xii = NULL;

        mat_get(A, ii, ii, &uii);
        if (!uii || expr_is_exact_zero(uii))
            goto fail;

        expr_retain(uii);
        xii = expr_div_simplify_owned(expr_new_const(NUM_ONE), uii);
        if (!xii)
            goto fail;
        mat_set(I, ii, ii, &xii);
        expr_free(xii);

        for (size_t j = ii + 1; j < n; ++j) {
            expr_t *sum = expr_new_const(NUM_ZERO);
            expr_t *xij = NULL;

            if (!sum)
                goto fail;

            for (size_t k = ii + 1; k <= j; ++k) {
                expr_t *uik = NULL;
                expr_t *xkj = NULL;
                expr_t *prod = NULL;
                expr_t *new_sum = NULL;

                mat_get(A, ii, k, &uik);
                mat_get(I, k, j, &xkj);
                if (!uik || !xkj) {
                    expr_free(sum);
                    goto fail;
                }

                prod = expr_mul(uik, xkj);
                if (!prod) {
                    expr_free(sum);
                    goto fail;
                }

                new_sum = expr_add(sum, prod);
                expr_free(sum);
                expr_free(prod);
                sum = new_sum ? expr_simplify_owned(new_sum) : NULL;
                if (!sum)
                    goto fail;
            }

            expr_retain(uii);
            xij = expr_div_simplify_owned(expr_negate_owned(sum), uii);
            if (!xij)
                goto fail;
            mat_set(I, ii, j, &xij);
            expr_free(xij);
        }
    }

    return mat_finalize_symbolic_result(I);

fail:
    mat_free(I);
    return NULL;
}

static matrix_t *mat_inverse_expr_lower_exact(const matrix_t *A)
{
    matrix_t *AT = NULL;
    matrix_t *ATi = NULL;
    matrix_t *I = NULL;

    AT = mat_transpose(A);
    if (!AT)
        return NULL;

    ATi = mat_inverse_expr_upper_exact(AT);
    if (!ATi) {
        mat_free(AT);
        return NULL;
    }

    I = mat_transpose(ATi);
    mat_free(AT);
    mat_free(ATi);
    return mat_finalize_symbolic_result(I);
}

static matrix_t *mat_inverse_expr_dense3_exact(const matrix_t *A)
{
    expr_t *m[3][3] = {{0}};
    expr_t *cof[3][3] = {{0}};
    expr_t *det = NULL;
    matrix_t *I = NULL;

    if (!A || A->rows != 3 || A->cols != 3)
        return NULL;

    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j)
            mat_get(A, i, j, &m[i][j]);

    if (mat_det_expr_exact(A, &det) != 0)
        det = NULL;
    if (!det || expr_is_exact_zero(det))
        goto fail;

    cof[0][0] = expr_det2_simplify(m[1][1], m[1][2], m[2][1], m[2][2]);
    cof[0][1] = expr_negate_owned(expr_det2_simplify(m[1][0], m[1][2], m[2][0], m[2][2]));
    cof[0][2] = expr_det2_simplify(m[1][0], m[1][1], m[2][0], m[2][1]);
    cof[1][0] = expr_negate_owned(expr_det2_simplify(m[0][1], m[0][2], m[2][1], m[2][2]));
    cof[1][1] = expr_det2_simplify(m[0][0], m[0][2], m[2][0], m[2][2]);
    cof[1][2] = expr_negate_owned(expr_det2_simplify(m[0][0], m[0][1], m[2][0], m[2][1]));
    cof[2][0] = expr_det2_simplify(m[0][1], m[0][2], m[1][1], m[1][2]);
    cof[2][1] = expr_negate_owned(expr_det2_simplify(m[0][0], m[0][2], m[1][0], m[1][2]));
    cof[2][2] = expr_det2_simplify(m[0][0], m[0][1], m[1][0], m[1][1]);

    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j)
            if (!cof[i][j])
                goto fail;

    I = mat_new_expr(3, 3);
    if (!I)
        goto fail;

    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            expr_t *entry = NULL;

            expr_retain(cof[j][i]);
            expr_retain(det);
            entry = expr_div_simplify_owned(cof[j][i], det);
            if (!entry)
                goto fail;
            mat_set(I, i, j, &entry);
            expr_free(entry);
        }
    }

    expr_free(det);
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j)
            expr_free(cof[i][j]);
    return mat_finalize_symbolic_result(I);

fail:
    expr_free(det);
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j)
            expr_free(cof[i][j]);
    mat_free(I);
    return NULL;
}

static matrix_t *mat_inverse_expr_dense_exact(const matrix_t *A)
{
    size_t n;
    matrix_t *I = NULL;
    matrix_t *Ai = NULL;

    if (!A || A->rows != A->cols)
        return NULL;

    n = A->rows;

    I = mat_create_identity_with_elem(n, &expr_elem);
    if (!I)
        return NULL;

    Ai = mat_solve_expr_exact(A, I);
    mat_free(I);
    return Ai;
}

int mat_det_expr_exact(const matrix_t *A, expr_t **determinant)
{
    matrix_t *M = NULL;
    expr_t *det = NULL;
    const expr_t *prev_pivot = EXPR_ONE;
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
            det = (expr_t *)EXPR_ZERO;
        expr_retain(det);
        *determinant = expr_simplify_owned(det);
        return *determinant ? 0 : -3;
    }

    M = mat_create_dense_with_elem(A->rows, A->cols, &expr_elem);
    if (!M)
        return -3;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            expr_t *v = NULL;
            mat_get(A, i, j, &v);
            mat_set(M, i, j, &v);
        }
    }

    for (size_t k = 0; k + 1 < n; ++k) {
        size_t pivot_row = n;
        expr_t *pivot = NULL;

        for (size_t i = k; i < n; ++i) {
            expr_t *candidate = NULL;
            mat_get(M, i, k, &candidate);
            if (!expr_node_is_exact_zero(candidate)) {
                pivot_row = i;
                break;
            }
        }

        if (pivot_row == n) {
            *determinant = expr_new_const(NUM_ZERO);
            mat_free(M);
            return *determinant ? 0 : -3;
        }

        if (pivot_row != k) {
            dense_swap_rows(M, k, pivot_row);
            negate = !negate;
        }

        mat_get(M, k, k, &pivot);
        if (expr_node_is_exact_zero(pivot)) {
            *determinant = expr_new_const(NUM_ZERO);
            mat_free(M);
            return *determinant ? 0 : -3;
        }

        for (size_t i = k + 1; i < n; ++i) {
            expr_t *aik = NULL;
            mat_get(M, i, k, &aik);

            for (size_t j = k + 1; j < n; ++j) {
                expr_t *aij = NULL;
                expr_t *akj = NULL;
                expr_t *lhs = NULL;
                expr_t *rhs = NULL;
                expr_t *num = NULL;
                expr_t *raw = NULL;
                expr_t *simp = NULL;

                mat_get(M, i, j, &aij);
                mat_get(M, k, j, &akj);

                lhs = expr_mul(aij, pivot);
                rhs = expr_mul(aik, akj);
                num = expr_sub(lhs, rhs);
                expr_free(lhs);
                expr_free(rhs);

                if (k == 0) {
                    raw = num;
                } else {
                    raw = expr_div(num, prev_pivot);
                    expr_free(num);
                }

                simp = expr_simplify_owned(raw);
                if (!simp) {
                    mat_free(M);
                    return -3;
                }

                mat_set(M, i, j, &simp);
                expr_free(simp);
            }

            {
                const expr_t *zero = EXPR_ZERO;
                mat_set(M, i, k, &zero);
            }
        }

        prev_pivot = pivot;
    }

    mat_get(M, n - 1, n - 1, &det);
    if (!det)
        det = (expr_t *)EXPR_ZERO;

    if (negate) {
        expr_t *raw = expr_neg(det);
        *determinant = expr_simplify_owned(raw);
    } else {
        *determinant = expr_simplify(det);
    }

    mat_free(M);
    return *determinant ? 0 : -3;
}

matrix_t *mat_inverse_expr_exact(const matrix_t *A)
{
    matrix_t *I = NULL;
    expr_t *a = NULL, *b = NULL, *c = NULL, *d = NULL;
    expr_t *det_left = NULL, *det_right = NULL, *det_raw = NULL, *det = NULL;
    expr_t *neg_b = NULL, *neg_c = NULL;
    expr_t *e00_raw = NULL, *e01_raw = NULL, *e10_raw = NULL, *e11_raw = NULL;
    expr_t *e00 = NULL, *e01 = NULL, *e10 = NULL, *e11 = NULL;

    if (!A || A->rows != A->cols)
        return NULL;


    if (A->rows == 1) {
        expr_t *v = NULL;
        expr_t *inv_raw = NULL;
        expr_t *inv = NULL;

        mat_get(A, 0, 0, &v);
        if (!v)
            return NULL;
        if (expr_is_exact_zero(v))
            return NULL;

        inv_raw = expr_div(EXPR_ONE, v);
        inv = expr_simplify_owned(inv_raw);
        if (!inv)
            return NULL;

        I = mat_new_expr(1, 1);
        if (!I) {
            expr_free(inv);
            return NULL;
        }

        mat_set(I, 0, 0, &inv);
        expr_free(inv);
        return mat_finalize_symbolic_result(I);
    }

    if (mat_is_upper_triangular(A))
        return mat_inverse_expr_upper_exact(A);

    if (mat_is_lower_triangular(A))
        return mat_inverse_expr_lower_exact(A);

    if (A->rows == 3)
        return mat_inverse_expr_dense3_exact(A);

    if (A->rows > 3)
        return mat_inverse_expr_dense_exact(A);

    mat_get(A, 0, 0, &a);
    mat_get(A, 0, 1, &b);
    mat_get(A, 1, 0, &c);
    mat_get(A, 1, 1, &d);

    det_left = expr_mul(a, d);
    det_right = expr_mul(b, c);
    det_raw = expr_sub(det_left, det_right);
    expr_free(det_left);
    det_left = NULL;
    expr_free(det_right);
    det_right = NULL;
    det = expr_simplify_owned(det_raw);
    det_raw = NULL;
    if (!det)
        return NULL;

    if (expr_is_exact_zero(det)) {
        expr_free(det);
        return NULL;
    }

    neg_b = expr_neg(b);
    neg_c = expr_neg(c);

    e00_raw = expr_div(d, det);
    e01_raw = expr_div(neg_b, det);
    e10_raw = expr_div(neg_c, det);
    e11_raw = expr_div(a, det);

    e00 = expr_simplify_owned(e00_raw);
    e00_raw = NULL;
    e01 = expr_simplify_owned(e01_raw);
    e01_raw = NULL;
    e10 = expr_simplify_owned(e10_raw);
    e10_raw = NULL;
    e11 = expr_simplify_owned(e11_raw);
    e11_raw = NULL;

    expr_free(neg_b);
    neg_b = NULL;
    expr_free(neg_c);
    neg_c = NULL;
    expr_free(det);
    det = NULL;

    if (!e00 || !e01 || !e10 || !e11)
        goto fail;

    I = mat_new_expr(2, 2);
    if (!I)
        goto fail;

    mat_set(I, 0, 0, &e00);
    mat_set(I, 0, 1, &e01);
    mat_set(I, 1, 0, &e10);
    mat_set(I, 1, 1, &e11);

    expr_free(e00);
    expr_free(e01);
    expr_free(e10);
    expr_free(e11);
    return mat_finalize_symbolic_result(I);

fail:
    expr_free(det_left);
    expr_free(det_right);
    expr_free(det);
    expr_free(neg_b);
    expr_free(neg_c);
    expr_free(e00_raw);
    expr_free(e01_raw);
    expr_free(e10_raw);
    expr_free(e11_raw);
    expr_free(e00);
    expr_free(e01);
    expr_free(e10);
    expr_free(e11);
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
                const expr_t *zero = EXPR_ZERO;
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
        unsigned char raw[MATRIX_SCALAR_STORAGE_BYTES];

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
            unsigned char raw[MATRIX_SCALAR_STORAGE_BYTES];
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
            unsigned char raw[MATRIX_SCALAR_STORAGE_BYTES];
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
    unsigned char coeff_prev[MATRIX_SCALAR_STORAGE_BYTES];
    unsigned char trace_val[MATRIX_SCALAR_STORAGE_BYTES];
    unsigned char k_val[MATRIX_SCALAR_STORAGE_BYTES];
    unsigned char inv_k[MATRIX_SCALAR_STORAGE_BYTES];
    unsigned char coeff[MATRIX_SCALAR_STORAGE_BYTES];
    unsigned char diag[MATRIX_SCALAR_STORAGE_BYTES];

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

        {
            number_t trace_num = num_new();

            if (mat_trace(Bnew, &trace_num) != 0) {
                num_destroy(&trace_num);
                mat_free(Bnew);
                goto fail;
            }
            mat_raw_value_from_number(e, trace_val, &trace_num);
            num_destroy(&trace_num);
        }

        {
            number_t k_num = num_create_from_long((long)k);
            mat_raw_value_from_number(e, k_val, &k_num);
            num_destroy(&k_num);
        }
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

matrix_t *mat_charpoly_expr(const matrix_t *A)
{
    matrix_t *coeffs = NULL;
    matrix_t *B = NULL;
    expr_t *coeff_prev = NULL;

    if (!A || A->rows != A->cols || A->elem != &expr_elem)
        return NULL;

    coeffs = mat_create_zero_with_elem(A->rows + 1, 1, &expr_elem);
    B = mat_create_zero_with_elem(A->rows, A->cols, &expr_elem);
    coeff_prev = expr_new_const(NUM_ONE);
    if (!coeffs || !B || !coeff_prev)
        goto fail;

    const expr_t *one = EXPR_ONE;
    mat_set(coeffs, 0, 0, &one);

    for (size_t k = 1; k <= A->rows; ++k) {
        matrix_t *T = mat_copy_as_dense(B);
        matrix_t *Bnew = NULL;
        expr_t *trace_val = NULL;
        expr_t *den = NULL;
        expr_t *quot = NULL;
        expr_t *coeff = NULL;

        if (!T)
            goto fail;

        for (size_t i = 0; i < A->rows; ++i) {
            const expr_t *diag = mat_get_expr_or_zero(T, i, i);
            expr_t *new_diag;

            expr_retain(diag);
            expr_retain(coeff_prev);
            new_diag = expr_add_simplify_owned(diag, coeff_prev);
            if (!new_diag) {
                mat_free(T);
                goto fail;
            }
            mat_set(T, i, i, &new_diag);
            expr_free(new_diag);
        }

        Bnew = mat_mul(A, T);
        mat_free(T);
        if (!Bnew)
            goto fail;

        if (mat_trace_expr(Bnew, &trace_val) != 0 || !trace_val) {
            mat_free(Bnew);
            goto fail;
        }

        {
            number_t k_num = num_create_from_long((long)k);
            den = expr_new_const(k_num);
            num_destroy(&k_num);
        }
        quot = expr_div_simplify_owned(trace_val, den);
        if (!quot) {
            mat_free(Bnew);
            goto fail;
        }

        coeff = expr_negate_owned(quot);
        if (!coeff) {
            mat_free(Bnew);
            goto fail;
        }

        mat_set(coeffs, k, 0, &coeff);
        expr_free(coeff_prev);
        coeff_prev = coeff;

        mat_free(B);
        B = Bnew;
    }

    expr_free(coeff_prev);
    mat_free(B);
    return coeffs;

fail:
    expr_free(coeff_prev);
    mat_free(coeffs);
    mat_free(B);
    return NULL;
}

static void expr_poly_coeffs_free(expr_t **coeffs, size_t count)
{
    if (!coeffs)
        return;
    for (size_t i = 0; i < count; ++i)
        expr_free(coeffs[i]);
    free(coeffs);
}

static expr_t **expr_poly_multiply_linear(expr_t **coeffs, size_t degree, expr_t *lambda)
{
    expr_t **next = NULL;

    if (!coeffs || !lambda)
        return NULL;

    next = calloc(degree + 2, sizeof(*next));
    if (!next)
        return NULL;

    next[0] = expr_clone_for_storage(coeffs[0]);
    if (!next[0])
        goto fail;

    for (size_t k = 1; k <= degree; ++k) {
        expr_t *term = NULL;

        expr_retain(lambda);
        expr_retain(coeffs[k - 1]);
        term = expr_mul_simplify_owned(lambda, coeffs[k - 1]);
        if (!term)
            goto fail;

        expr_retain(coeffs[k]);
        next[k] = expr_sub_simplify_owned(coeffs[k], term);
        if (!next[k])
            goto fail;
    }

    expr_t *tail = NULL;

    expr_retain(lambda);
    expr_retain(coeffs[degree]);
    tail = expr_mul_simplify_owned(lambda, coeffs[degree]);
    if (!tail)
        goto fail;
    next[degree + 1] = expr_negate_owned(tail);
    if (!next[degree + 1])
        goto fail;

    return next;

fail:
    expr_poly_coeffs_free(next, degree + 2);
    return NULL;
}

static matrix_t *expr_poly_matrix_from_coeffs(expr_t **coeffs, size_t degree)
{
    matrix_t *P = NULL;

    if (!coeffs)
        return NULL;

    P = mat_create_zero_with_elem(degree + 1, 1, &expr_elem);
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

static matrix_t *mat_shift_expr_exact(const matrix_t *A, const expr_t *lambda)
{
    matrix_t *Shifted = NULL;

    if (!A || A->elem != &expr_elem || !lambda || A->rows != A->cols)
        return NULL;

    Shifted = mat_copy_as_dense(A);
    if (!Shifted)
        return NULL;

    for (size_t i = 0; i < A->rows; ++i) {
        expr_t *diag = NULL;
        expr_t *new_diag = NULL;

        mat_get(Shifted, i, i, &diag);
        if (!diag)
            diag = (expr_t *)EXPR_ZERO;

        expr_retain(diag);
        expr_retain(lambda);
        new_diag = expr_sub_simplify_owned(diag, lambda);
        if (!new_diag) {
            mat_free(Shifted);
            return NULL;
        }
        mat_set(Shifted, i, i, &new_diag);
        expr_free(new_diag);
    }

    return Shifted;
}

static int mat_expr_nullity_exact(const matrix_t *A)
{
    matrix_t *N = NULL;
    int nullity = -1;

    if (!A || A->elem != &expr_elem)
        return -1;

    N = mat_nullspace_expr_exact(A);
    if (!N)
        return -1;

    nullity = (int)mat_get_col_count(N);
    mat_free(N);
    return nullity;
}

static size_t mat_expr_triangular_root_exponent(const matrix_t *A,
                                                const expr_t *lambda,
                                                size_t multiplicity)
{
    matrix_t *Shifted = NULL;
    matrix_t *Power = NULL;
    size_t exponent = 0;

    if (!A || !lambda || multiplicity == 0)
        return 0;

    Shifted = mat_shift_expr_exact(A, lambda);
    if (!Shifted)
        return 0;

    Power = mat_copy_as_dense(Shifted);
    if (!Power) {
        mat_free(Shifted);
        return 0;
    }

    for (size_t k = 1; k <= multiplicity; ++k) {
        int nullity = mat_expr_nullity_exact(Power);

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

static void expr_rref_info_reset(expr_rref_info_t *info)
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

static int mat_expr_rref_exact(const matrix_t *A, expr_rref_info_t *out)
{
    matrix_t *R = NULL;
    size_t *pivot_cols = NULL;
    bool *is_pivot = NULL;
    size_t rank = 0;
    size_t row = 0;

    if (!A || A->elem != &expr_elem || !out)
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
            expr_t *candidate = NULL;
            mat_get(R, r, col, &candidate);
            if (!expr_node_is_exact_zero(candidate)) {
                pivot_row = r;
                break;
            }
        }

        if (pivot_row == A->rows)
            continue;

        if (pivot_row != row)
            dense_swap_rows(R, row, pivot_row);

        {
            expr_t *pivot = NULL;
            mat_get_owned(R, row, col, &pivot);

            for (size_t j = col; j < A->cols; ++j) {
                expr_t *entry = NULL;
                expr_t *new_entry;

                mat_get(R, row, j, &entry);
                expr_retain(entry);
                expr_retain(pivot);
                new_entry = expr_div_simplify_owned(entry, pivot);
                if (!new_entry) {
                    expr_free(pivot);
                    goto fail;
                }
                mat_set(R, row, j, &new_entry);
                expr_free(new_entry);
            }
            expr_free(pivot);
        }

        for (size_t i = 0; i < A->rows; ++i) {
            expr_t *factor = NULL;

            if (i == row)
                continue;

            mat_get(R, i, col, &factor);
            if (expr_node_is_exact_zero(factor))
                continue;
            expr_retain(factor);

            for (size_t j = col; j < A->cols; ++j) {
                expr_t *rij = NULL;
                expr_t *rrj = NULL;
                expr_t *term = NULL;
                expr_t *new_rij = NULL;

                mat_get(R, i, j, &rij);
                mat_get(R, row, j, &rrj);
                expr_retain(rij);
                expr_retain(factor);
                expr_retain(rrj);
                term = expr_mul_simplify_owned(factor, rrj);
                if (!term) {
                    expr_free(factor);
                    goto fail;
                }
                new_rij = expr_sub_simplify_owned(rij, term);
                if (!new_rij) {
                    expr_free(factor);
                    goto fail;
                }
                mat_set(R, i, j, &new_rij);
                expr_free(new_rij);
            }

            expr_free(factor);
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

static matrix_t *mat_expr_extract_columns(const matrix_t *A,
                                          const size_t *cols,
                                          size_t ncols)
{
    matrix_t *C = NULL;

    if (!A || A->elem != &expr_elem || (!cols && ncols != 0))
        return NULL;

    C = mat_create_zero_with_elem(A->rows, ncols, &expr_elem);
    if (!C)
        return NULL;

    for (size_t j = 0; j < ncols; ++j) {
        for (size_t i = 0; i < A->rows; ++i) {
            expr_t *entry = NULL;

            mat_get(A, i, cols[j], &entry);
            mat_set(C, i, j, &entry);
        }
    }

    return C;
}

static matrix_t *mat_expr_build_pivot_factor(const matrix_t *A,
                                             const expr_rref_info_t *info)
{
    matrix_t *F = NULL;

    if (!A || !info || !info->R || A->elem != &expr_elem)
        return NULL;

    F = mat_create_zero_with_elem(info->rank, A->cols, &expr_elem);
    if (!F)
        return NULL;

    for (size_t k = 0; k < info->rank; ++k) {
        const expr_t *one = EXPR_ONE;
        mat_set(F, k, info->pivot_cols[k], &one);
    }

    for (size_t col = 0; col < A->cols; ++col) {
        if (info->is_pivot[col])
            continue;

        for (size_t k = 0; k < info->rank; ++k) {
            expr_t *entry = NULL;

            mat_get(info->R, k, col, &entry);
            if (!expr_node_is_exact_zero(entry))
                mat_set(F, k, col, &entry);
        }
    }

    return F;
}

matrix_t *mat_minpoly_expr(const matrix_t *A)
{
    expr_t **roots = NULL;
    size_t *exponents = NULL;
    size_t root_count = 0;
    expr_t **coeffs = NULL;
    size_t degree = 0;
    matrix_t *P = NULL;

    if (!A || A->rows != A->cols || A->elem != &expr_elem)
        return NULL;

    if (A->rows == 0) {
        coeffs = calloc(1, sizeof(*coeffs));
        if (!coeffs)
            return NULL;
        coeffs[0] = expr_clone_for_storage(EXPR_ONE);
        if (!coeffs[0]) {
            free(coeffs);
            return NULL;
        }
        P = expr_poly_matrix_from_coeffs(coeffs, 0);
        expr_poly_coeffs_free(coeffs, 1);
        return P;
    }

    if (mat_is_diagonal(A) || mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) {
        roots = calloc(A->rows, sizeof(*roots));
        exponents = calloc(A->rows, sizeof(*exponents));
        if (!roots || !exponents)
            goto fail;

        for (size_t i = 0; i < A->rows; ++i) {
            const expr_t *diag = mat_get_expr_or_zero(A, i, i);
            size_t idx = 0;

            while (idx < root_count && !expr_exprs_equal_exact(roots[idx], diag))
                ++idx;

            if (idx == root_count) {
                size_t multiplicity = 0;
                size_t exponent = 0;

                for (size_t j = 0; j < A->rows; ++j) {
                    const expr_t *other = mat_get_expr_or_zero(A, j, j);
                    if (expr_exprs_equal_exact(diag, other))
                        ++multiplicity;
                }

                exponent = mat_is_diagonal(A)
                    ? 1
                    : mat_expr_triangular_root_exponent(A, diag, multiplicity);
                if (exponent == 0)
                    goto fail;

                roots[root_count] = expr_clone_for_storage(diag);
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
        roots[0] = expr_clone_for_storage(mat_get_expr_or_zero(A, 0, 0));
        if (!roots[0])
            goto fail;
        exponents[0] = 1;
        root_count = 1;
    } else if (A->rows == 2) {
        const expr_t *a = mat_get_expr_or_zero(A, 0, 0);
        const expr_t *b = mat_get_expr_or_zero(A, 0, 1);
        const expr_t *c = mat_get_expr_or_zero(A, 1, 0);
        const expr_t *d = mat_get_expr_or_zero(A, 1, 1);
        expr_t *ev[2] = {NULL, NULL};

        roots = calloc(2, sizeof(*roots));
        exponents = calloc(2, sizeof(*exponents));
        if (!roots || !exponents)
            goto fail;

        if (expr_node_is_exact_zero(b) && expr_node_is_exact_zero(c) && expr_exprs_equal_exact(a, d)) {
            roots[0] = expr_clone_for_storage(a);
            if (!roots[0])
                goto fail;
            exponents[0] = 1;
            root_count = 1;
        } else {
            if (mat_eigenvalues_expr_symbolic(A, ev) != 0 || !ev[0] || !ev[1]) {
                expr_free(ev[0]);
                expr_free(ev[1]);
                goto fail;
            }

            if (expr_exprs_equal_exact(ev[0], ev[1])) {
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

            expr_free(ev[0]);
            expr_free(ev[1]);
        }
    } else {
        return NULL;
    }

    coeffs = calloc(1, sizeof(*coeffs));
    if (!coeffs)
        goto fail;
    coeffs[0] = expr_clone_for_storage(EXPR_ONE);
    if (!coeffs[0])
        goto fail;

    for (size_t i = 0; i < root_count; ++i) {
        for (size_t p = 0; p < exponents[i]; ++p) {
            expr_t **next = expr_poly_multiply_linear(coeffs, degree, roots[i]);
            if (!next)
                goto fail;
            expr_poly_coeffs_free(coeffs, degree + 1);
            coeffs = next;
            ++degree;
        }
    }

    P = expr_poly_matrix_from_coeffs(coeffs, degree);

fail:
    if (roots) {
        for (size_t i = 0; i < root_count; ++i)
            expr_free(roots[i]);
    }
    free(roots);
    free(exponents);
    expr_poly_coeffs_free(coeffs, degree + 1);
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
            const expr_t *one = EXPR_ONE;
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
                expr_t *det = NULL;
                expr_t *entry;

                if (mat_det_expr(Minor, &det) != 0 || !det) {
                    mat_free(Minor);
                    goto fail;
                }

                entry = det;
                if (((i + j) & 1u) != 0u) {
                    entry = expr_negate_owned(det);
                    det = NULL;
                    if (!entry) {
                        mat_free(Minor);
                        goto fail;
                    }
                }

                mat_set(Adj, i, j, &entry);
                expr_free(entry);
            } else {
                number_t det_num = num_new();

                if (mat_det(Minor, &det_num) != 0) {
                    num_destroy(&det_num);
                    mat_free(Minor);
                    goto fail;
                }
                if (((i + j) & 1u) != 0u) {
                    number_t neg_det = num_neg(det_num);
                    num_destroy(&det_num);
                    det_num = neg_det;
                }
                mat_set(Adj, i, j, &det_num);
                num_destroy(&det_num);
            }

            mat_free(Minor);
        }
    }

    return Adj;

fail:
    mat_free(Adj);
    return NULL;
}

matrix_t *mat_nullspace_expr_exact(const matrix_t *A)
{
    expr_rref_info_t info = {0};
    matrix_t *N = NULL;
    size_t nullity = 0;
    size_t basis_col = 0;

    if (!A || A->elem != &expr_elem)
        return NULL;

    if (mat_expr_rref_exact(A, &info) != 0)
        goto fail;

    nullity = A->cols - info.rank;
    N = mat_create_zero_with_elem(A->cols, nullity, &expr_elem);
    if (!N)
        goto fail;

    for (size_t free_col = 0; free_col < A->cols; ++free_col) {
        if (info.is_pivot[free_col])
            continue;

        {
            const expr_t *one = EXPR_ONE;
            mat_set(N, free_col, basis_col, &one);
        }

        for (size_t r = 0; r < info.rank; ++r) {
            size_t pivot_col = info.pivot_cols[r];
            expr_t *entry = NULL;
            expr_t *coeff;

            mat_get(info.R, r, free_col, &entry);
            if (expr_node_is_exact_zero(entry))
                continue;

            expr_retain(entry);
            coeff = expr_negate_owned(entry);
            if (!coeff)
                goto fail;
            mat_set(N, pivot_col, basis_col, &coeff);
            expr_free(coeff);
        }

        basis_col++;
    }

    expr_rref_info_reset(&info);
    return N;

fail:
    expr_rref_info_reset(&info);
    mat_free(N);
    return NULL;
}

int mat_rank_expr_exact(const matrix_t *A)
{
    expr_rref_info_t info = {0};
    int rank;

    if (!A || A->elem != &expr_elem)
        return -1;

    if (mat_expr_rref_exact(A, &info) != 0)
        return -2;

    rank = (int)info.rank;
    expr_rref_info_reset(&info);
    return rank;
}

matrix_t *mat_pseudoinverse_expr_exact(const matrix_t *A)
{
    expr_rref_info_t info = {0};
    matrix_t *AT = NULL;
    matrix_t *Gram = NULL;
    matrix_t *Gram_inv = NULL;
    matrix_t *C = NULL;
    matrix_t *F = NULL;
    matrix_t *C_pinv = NULL;
    matrix_t *F_pinv = NULL;
    matrix_t *Pinv = NULL;

    if (!A || A->elem != &expr_elem)
        return NULL;

    if (mat_expr_rref_exact(A, &info) != 0)
        return NULL;

    if (info.rank == 0) {
        Pinv = mat_create_zero_with_elem(A->cols, A->rows, &expr_elem);
        expr_rref_info_reset(&info);
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

    C = mat_expr_extract_columns(A, info.pivot_cols, info.rank);
    F = C ? mat_expr_build_pivot_factor(A, &info) : NULL;
    C_pinv = C ? mat_pseudoinverse_expr_exact(C) : NULL;
    F_pinv = F ? mat_pseudoinverse_expr_exact(F) : NULL;
    Pinv = (F_pinv && C_pinv) ? mat_mul(F_pinv, C_pinv) : NULL;

done:
    expr_rref_info_reset(&info);
    mat_free(AT);
    mat_free(Gram);
    mat_free(Gram_inv);
    mat_free(C);
    mat_free(F);
    mat_free(C_pinv);
    mat_free(F_pinv);
    return mat_finalize_symbolic_result(Pinv);
}
