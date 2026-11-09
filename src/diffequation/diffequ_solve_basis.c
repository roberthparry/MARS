/**
 * @file diffequ_solve_basis.c
 * @brief Impose linear point conditions on a differential equation's solution basis.
 *
 * Shared by the constant-coefficient and Sturm--Liouville solvers. Complete data use a square matrix solve;
 * incomplete data use the matrix module's exact nullspace to retain every undetermined integration constant.
 * No differential-equation-specific elimination algorithm or presentation syntax is implemented here.
 */
#include <stdio.h>
#include <stdlib.h>

#include "matrix-t.h"
#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"

static expr_t *de_basis_derivative_at(const expr_t *expression, const expr_t *coordinate, size_t order,
                                     const expr_t *point)
{
    expr_t *derivative = order ? expr_create_nth_deriv((unsigned)order, expression, coordinate)
                              : expr_clone(expression);
    expr_t *at = derivative ? expr_substitute(derivative, coordinate, point) : NULL;
    expr_free(derivative);
    return at ? expr_simplify_owned(at) : NULL;
}

static bool de_basis_condition(const diffequ_t *de, size_t index, const expr_t *dependent, const expr_t *coordinate,
                               size_t *order, const expr_t **point, const expr_t **value)
{
    const equation_t *condition = de->conditions[index];
    const expr_t *left = condition ? equ_lhs(condition) : NULL;
    if (!left || de->condition_point_counts[index] != 1u)
        return false;
    *order = 0u;
    if (!expr_struct_eq(left, dependent)) {
        if (!expr_is_formal_derivative(left) || !expr_struct_eq(expr_formal_derivative_dependent(left), dependent))
            return false;
        *order = expr_formal_derivative_order(left);
        for (size_t i = 0u; i < *order; ++i)
            if (!expr_struct_eq(expr_formal_derivative_wrt_at(left, i), coordinate))
                return false;
    }
    *point = de->condition_points[index][0];
    *value = equ_rhs(condition);
    return true;
}

static matrix_t *de_basis_partial_constants(const matrix_t *matrix, const matrix_t *right)
{
    size_t rows = mat_get_row_count(matrix), count = mat_get_col_count(matrix);
    matrix_t *augmented = mat_new_expr(rows, count + 1u), *nullspace = NULL, *constants = NULL;
    expr_t *normaliser = NULL;
    expr_t **parameters = NULL;
    size_t dimension = 0u;
    bool ok = false;
    if (!augmented)
        goto fail;
    for (size_t row = 0u; row < rows; ++row) {
        for (size_t col = 0u; col < count; ++col) {
            expr_t *entry = NULL;
            mat_get(matrix, row, col, &entry);
            mat_set(augmented, row, col, &entry);
        }
        expr_t *entry = NULL;
        mat_get(right, row, 0u, &entry);
        expr_t *negative = entry ? expr_neg(entry) : NULL;
        if (!negative)
            goto fail;
        mat_set(augmented, row, count, &negative);
        expr_free(negative);
    }
    nullspace = mat_nullspace(augmented);
    if (!nullspace)
        goto fail;
    dimension = mat_get_col_count(nullspace);
    size_t anchor = dimension;
    /* Every basis column may contribute: this algebraic pass is bounded by the ODE's basis dimension. */
    for (size_t col = 0u; col < dimension; ++col) {
        expr_t *entry = NULL;
        mat_get(nullspace, count, col, &entry);
        if (entry && !expr_is_exact_zero(entry)) {
            normaliser = entry;
            anchor = col;
            break;
        }
    }
    if (!normaliser)
        goto fail; /* No vector with final coordinate one means the authored data are inconsistent. */
    constants = mat_new_expr(count, 1u);
    parameters = calloc(dimension, sizeof *parameters);
    if (!constants || !parameters)
        goto fail;
    size_t next_constant = 1u;
    for (size_t col = 0u; col < dimension; ++col) {
        if (col == anchor)
            continue;
        char name[32];
        snprintf(name, sizeof name, "C%zu", next_constant++);
        parameters[col] = expr_new_named_const(NUM_NAN, name);
        if (!parameters[col])
            goto fail;
    }
    for (size_t row = 0u; row < count; ++row) {
        expr_t *entry = NULL;
        mat_get(nullspace, row, anchor, &entry);
        expr_t *particular = entry ? expr_div_simplify_owned(expr_clone(entry), expr_clone(normaliser)) : NULL;
        expr_t *value = particular ? expr_clone(particular) : NULL;
        for (size_t col = 0u; value && col < dimension; ++col) {
            if (col == anchor)
                continue;
            expr_t *component = NULL, *last = NULL;
            mat_get(nullspace, row, col, &component);
            mat_get(nullspace, count, col, &last);
            expr_t *direction = component && last ?
                expr_sub_simplify_owned(expr_clone(component), expr_mul(particular, last)) : NULL;
            expr_t *term = direction ? expr_mul(parameters[col], direction) : NULL;
            expr_free(direction);
            if (!term) {
                expr_free(value);
                value = NULL;
                break;
            }
            value = expr_add_simplify_owned(value, term);
        }
        expr_free(particular);
        if (!value)
            goto fail;
        mat_set(constants, row, 0u, &value);
        expr_free(value);
    }
    ok = true;
fail:
    if (parameters)
        for (size_t col = 0u; col < dimension; ++col)
            expr_free(parameters[col]);
    free(parameters);
    mat_free(nullspace);
    mat_free(augmented);
    if (!ok) {
        mat_free(constants);
        constants = NULL;
    }
    return constants;
}

/* Impose the authored linear conditions without inventing any missing initial data. */
expr_t *de_apply_basis_conditions(const diffequ_t *de, const expr_t *coordinate, const expr_t *dependent,
                                  expr_t *const *basis, size_t count, const expr_t *particular)
{
    if (!de || !coordinate || !dependent || !basis || !particular || !de->condition_count ||
        de->condition_count > count)
        return NULL;
    size_t rows = de->condition_count;
    matrix_t *matrix = mat_new_expr(rows, count), *right = mat_new_expr(rows, 1u), *constants = NULL;
    expr_t *solution = NULL;
    if (!matrix || !right)
        goto cleanup;
    for (size_t row = 0u; row < rows; ++row) {
        size_t order = 0u;
        const expr_t *point = NULL, *value = NULL;
        if (!de_basis_condition(de, row, dependent, coordinate, &order, &point, &value))
            goto cleanup;
        expr_t *at = de_basis_derivative_at(particular, coordinate, order, point);
        expr_t *rhs = at ? expr_sub_simplify_owned(expr_clone(value), at) : NULL;
        if (!rhs)
            goto cleanup;
        mat_set(right, row, 0u, &rhs);
        expr_free(rhs);
        for (size_t col = 0u; col < count; ++col) {
            expr_t *entry = de_basis_derivative_at(basis[col], coordinate, order, point);
            if (!entry)
                goto cleanup;
            mat_set(matrix, row, col, &entry);
            expr_free(entry);
        }
    }
    constants = rows == count ? mat_solve(matrix, right) : NULL;
    if (!constants)
        constants = de_basis_partial_constants(matrix, right);
    if (!constants)
        goto cleanup;
    solution = expr_clone(particular);
    for (size_t i = 0u; solution && i < count; ++i) {
        expr_t *constant = NULL;
        mat_get(constants, i, 0u, &constant);
        expr_t *term = constant ? expr_mul_simplify_owned(expr_clone(constant), expr_clone(basis[i])) : NULL;
        if (!term) {
            expr_free(solution);
            solution = NULL;
            break;
        }
        solution = expr_add_simplify_owned(solution, term);
    }
    if (solution) {
        expr_t *shown = expr_display_simplified(solution);
        if (shown) {
            expr_free(solution);
            solution = shown;
        }
    }
cleanup:
    mat_free(constants);
    mat_free(right);
    mat_free(matrix);
    return solution;
}
