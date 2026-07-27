#include <stdlib.h>

#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

matrix_t *mat_deriv(const matrix_t *A, expr_t *wrt)
{
    matrix_t *D = NULL;

    if (!A || !wrt)
        return NULL;
    if (!A->elem)
        return NULL;

    if (!matrix_is_symbolic(A))
        return mat_create_zero_with_elem(A->rows, A->cols, A->elem);

    D = mat_create_zero_with_elem(A->rows, A->cols, &expr_elem);
    if (!D)
        return NULL;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            expr_t *entry = NULL;
            expr_t *deriv = NULL;

            mat_get(A, i, j, &entry);
            if (!entry)
                entry = (expr_t *)EXPR_ZERO;

            deriv = expr_create_deriv(entry, wrt);
            if (!deriv) {
                mat_free(D);
                return NULL;
            }

            mat_set(D, i, j, &deriv);
            expr_free(deriv);
        }
    }

    return D;
}

expr_t *mat_deriv_trace_by_name(const matrix_t *A, mat_bindings_t *bindings,
                                const char *name)
{
    expr_t *binding;

    if (!A || !bindings || !name)
        return NULL;

    binding = mat_bindings_get(bindings, name);
    if (!binding)
        return NULL;

    return mat_deriv_trace(A, binding);
}

matrix_t *mat_deriv_by_name(const matrix_t *A, mat_bindings_t *bindings,
                            const char *name)
{
    expr_t *binding;

    if (!A || !bindings || !name)
        return NULL;

    binding = mat_bindings_get(bindings, name);
    if (!binding)
        return NULL;

    return mat_deriv(A, binding);
}

expr_t *mat_deriv_trace(const matrix_t *A, expr_t *wrt)
{
    expr_t *trace = NULL;
    expr_t *deriv = NULL;

    if (!A || !wrt)
        return NULL;
    if (!A->elem)
        return NULL;
    if (!matrix_is_symbolic(A))
        return expr_new_const(NUM_ZERO);

    if (mat_trace_expr(A, &trace) != 0 || !trace)
        return NULL;

    deriv = expr_create_deriv(trace, wrt);
    expr_free(trace);
    return deriv;
}

expr_t *mat_deriv_det(const matrix_t *A, expr_t *wrt)
{
    expr_t *det = NULL;
    expr_t *deriv = NULL;

    if (!A || !wrt)
        return NULL;
    if (!A->elem)
        return NULL;
    if (!matrix_is_symbolic(A))
        return expr_new_const(NUM_ZERO);

    if (mat_det_expr(A, &det) != 0 || !det)
        return NULL;

    deriv = expr_create_deriv(det, wrt);
    expr_free(det);
    return deriv;
}

expr_t *mat_deriv_det_by_name(const matrix_t *A, mat_bindings_t *bindings,
                              const char *name)
{
    expr_t *binding;

    if (!A || !bindings || !name)
        return NULL;

    binding = mat_bindings_get(bindings, name);
    if (!binding)
        return NULL;

    return mat_deriv_det(A, binding);
}

matrix_t *mat_deriv_inverse(const matrix_t *A, expr_t *wrt)
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
    if (!matrix_is_symbolic(A)) {
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

matrix_t *mat_deriv_inverse_by_name(const matrix_t *A, mat_bindings_t *bindings,
                                    const char *name)
{
    expr_t *binding;

    if (!A || !bindings || !name)
        return NULL;

    binding = mat_bindings_get(bindings, name);
    if (!binding)
        return NULL;

    return mat_deriv_inverse(A, binding);
}

matrix_t *mat_deriv_block_inverse(const matrix_t *A, size_t split, expr_t *wrt)
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
    if (!matrix_is_symbolic(A)) {
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
                                          mat_bindings_t *bindings,
                                          const char *name)
{
    expr_t *binding;

    if (!A || !bindings || !name)
        return NULL;

    binding = mat_bindings_get(bindings, name);
    if (!binding)
        return NULL;

    return mat_deriv_block_inverse(A, split, binding);
}

matrix_t *mat_jacobian(const matrix_t *A, expr_t *const *vars, size_t nvars)
{
    matrix_t *J = NULL;

    if (!A || !vars || nvars == 0)
        return NULL;
    if (!A->elem)
        return NULL;
    if (!matrix_is_symbolic(A))
        return mat_create_zero_with_elem(A->rows * A->cols, nvars, &expr_elem);

    J = mat_create_zero_with_elem(A->rows * A->cols, nvars, &expr_elem);
    if (!J)
        return NULL;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            expr_t *entry = NULL;
            size_t row = i * A->cols + j;

            mat_get(A, i, j, &entry);
            if (!entry)
                entry = (expr_t *)EXPR_ZERO;

            for (size_t k = 0; k < nvars; ++k) {
                expr_t *deriv = NULL;

                if (!vars[k]) {
                    mat_free(J);
                    return NULL;
                }

                deriv = expr_create_deriv(entry, vars[k]);
                if (!deriv) {
                    mat_free(J);
                    return NULL;
                }

                mat_set(J, row, k, &deriv);
                expr_free(deriv);
            }
        }
    }

    return J;
}

matrix_t *mat_jacobian_by_names(const matrix_t *A, mat_bindings_t *bindings,
                                const char *const *names, size_t nnames)
{
    expr_t **vars = NULL;
    matrix_t *J = NULL;

    if (!A || !bindings || !names || nnames == 0)
        return NULL;

    vars = malloc(nnames * sizeof(*vars));
    if (!vars)
        return NULL;

    for (size_t i = 0; i < nnames; ++i) {
        expr_t *binding;

        if (!names[i]) {
            free(vars);
            return NULL;
        }

        binding = mat_bindings_get(bindings, names[i]);
        if (!binding) {
            free(vars);
            return NULL;
        }

        vars[i] = binding;
    }

    J = mat_jacobian(A, vars, nnames);
    free(vars);
    return J;
}

matrix_t *mat_deriv_block_solve(const matrix_t *A, const matrix_t *B, size_t split, expr_t *wrt)
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
    if (!matrix_is_symbolic(A) || !matrix_is_symbolic(B)) {
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
                                        mat_bindings_t *bindings,
                                        const char *name)
{
    expr_t *binding;

    if (!A || !B || !bindings || !name)
        return NULL;

    binding = mat_bindings_get(bindings, name);
    if (!binding)
        return NULL;

    return mat_deriv_block_solve(A, B, split, binding);
}

matrix_t *mat_deriv_solve(const matrix_t *A, const matrix_t *B, expr_t *wrt)
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
    if (!matrix_is_symbolic(A) || !matrix_is_symbolic(B)) {
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
                                  mat_bindings_t *bindings,
                                  const char *name)
{
    expr_t *binding;

    if (!A || !B || !bindings || !name)
        return NULL;

    binding = mat_bindings_get(bindings, name);
    if (!binding)
        return NULL;

    return mat_deriv_solve(A, B, binding);
}
