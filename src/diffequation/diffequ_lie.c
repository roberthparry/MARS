#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"

struct de_lie_t {
    expr_t *coordinate[3];
    expr_t *rhs;
};

static bool de_lie_zero_polynomial(const de_lie_t *lie, const expr_t *expr, size_t coordinate)
{
    if (!expr)
        return false;
    if (expr_is_exact_zero(expr))
        return true;
    if (coordinate == 3u)
        return false;
    expr_t **coefficients = NULL;
    size_t degree = 0u;
    bool zero = equ_collect_symbolic_polynomial_alloc(expr, lie->coordinate[coordinate], &coefficients, &degree);
    for (size_t i = 0u; zero && i <= degree; ++i)
        zero = de_lie_zero_polynomial(lie, coefficients[i], coordinate + 1u);
    if (coefficients) {
        for (size_t i = 0u; i <= degree; ++i)
            expr_free(coefficients[i]);
    }
    free(coefficients);
    return zero;
}

static bool de_lie_name_used(const expr_t *expr, const char *name)
{
    const char *symbol = expr_symbol_name(expr);
    const expr_t *left = NULL, *right = NULL;
    if (symbol && strcmp(symbol, name) == 0)
        return true;
    return expr_child_exprs(expr, &left, &right) &&
           (de_lie_name_used(left, name) || de_lie_name_used(right, name));
}

static expr_t *de_lie_simplify(expr_t *owned)
{
    return de_simplify_unary_owned(owned, expr_expand_products_internal);
}

/* Normalise the equation without imposing its initial or boundary conditions. */
de_lie_t *de_lie_new(const diffequ_t *de)
{
    de_lie_t *lie = NULL;
    expr_t *residual = NULL, *leading = NULL, *remainder = NULL, *rhs = NULL;
    const expr_t *dependent = NULL, *first = NULL, *second = NULL;
    size_t order = 0u;
    bool valid = false;

    if (!de || de->independent_count != 1u)
        return NULL;
    residual = equ_residual(de->equation);
    if (!residual || !de_find_derivatives(residual, de->independent_vars[0], &dependent, &first, &second, &order) ||
        order != 2u || !dependent || !second ||
        !de_linear_decompose(residual, second, &leading, &remainder) || expr_is_exact_zero(leading))
        goto cleanup;
    lie = calloc(1u, sizeof(*lie));
    if (!lie)
        goto cleanup;
    lie->coordinate[0] = expr_clone(de->independent_vars[0]);
    lie->coordinate[1] = expr_clone(dependent);
    for (size_t suffix = 0u; !lie->coordinate[2]; ++suffix) {
        char name[64];
        if (suffix)
            snprintf(name, sizeof(name), "p_%zu", suffix);
        else
            snprintf(name, sizeof(name), "p");
        expr_t *candidate = expr_new_named_var(NUM_NAN, name);
        if (!candidate)
            goto cleanup;
        const char *canonical = expr_symbol_name(candidate);
        if (!de_lie_name_used(residual, canonical) && !de_lie_name_used(lie->coordinate[0], canonical) &&
            !de_lie_name_used(lie->coordinate[1], canonical))
            lie->coordinate[2] = candidate;
        else
            expr_free(candidate);
    }
    rhs = expr_div_simplify_owned(expr_neg(remainder), expr_clone(leading));
    lie->rhs = rhs ? (first ? expr_substitute(rhs, first, lie->coordinate[2]) : expr_clone(rhs)) : NULL;
    lie->rhs = de_lie_simplify(lie->rhs);
    const expr_t *remaining_dependent = NULL, *remaining_first = NULL, *remaining_second = NULL;
    size_t remaining_order = 0u;
    valid = lie->coordinate[0] && lie->coordinate[1] && lie->rhs &&
            de_find_derivatives(lie->rhs, lie->coordinate[0], &remaining_dependent, &remaining_first,
                                  &remaining_second, &remaining_order) && remaining_order == 0u;
cleanup:
    expr_free(rhs);
    expr_free(remainder);
    expr_free(leading);
    expr_free(residual);
    if (!valid) {
        de_lie_free(lie);
        lie = NULL;
    }
    return lie;
}

/* Release the owned normal form and jet coordinates. */
void de_lie_free(de_lie_t *lie)
{
    if (!lie)
        return;
    for (size_t i = 0u; i < 3u; ++i)
        expr_free(lie->coordinate[i]);
    expr_free(lie->rhs);
    free(lie);
}

/* Borrow a coordinate from the analysis. */
const expr_t *de_lie_coordinate(const de_lie_t *lie, size_t index)
{
    return lie && index < 3u ? lie->coordinate[index] : NULL;
}

/* Borrow the normalised right-hand side. */
const expr_t *de_lie_rhs(const de_lie_t *lie)
{
    return lie ? lie->rhs : NULL;
}

static expr_t *de_lie_total_derivative(const de_lie_t *lie, const expr_t *expr)
{
    if (!lie || !expr)
        return NULL;
    expr_t *dx = expr_create_deriv(expr, lie->coordinate[0]);
    expr_t *dy = expr_mul_simplify_owned(expr_clone(lie->coordinate[2]),
                                         expr_create_deriv(expr, lie->coordinate[1]));
    expr_t *dp = expr_mul_simplify_owned(expr_clone(lie->rhs), expr_create_deriv(expr, lie->coordinate[2]));
    return de_lie_simplify(expr_add_simplify_owned(expr_add_simplify_owned(dx, dy), dp));
}

static bool de_lie_point_components(const de_lie_t *lie, const expr_t *xi, const expr_t *eta)
{
    return lie && xi && eta && !de_expr_uses(xi, lie->coordinate[2]) && !de_expr_uses(eta, lie->coordinate[2]);
}

/* Apply the general prolongation recurrence on the equation manifold. */
expr_t *de_lie_prolongation(const de_lie_t *lie, const expr_t *xi, const expr_t *eta, size_t order)
{
    if (!de_lie_point_components(lie, xi, eta) || order < 1u || order > 2u)
        return NULL;
    expr_t *dxi = de_lie_total_derivative(lie, xi);
    expr_t *coefficient = expr_clone(eta);
    for (size_t k = 1u; k <= order; ++k) {
        expr_t *next = de_lie_total_derivative(lie, coefficient);
        expr_t *correction = dxi ? expr_mul(k == 1u ? lie->coordinate[2] : lie->rhs, dxi) : NULL;
        expr_free(coefficient);
        coefficient = de_lie_simplify(expr_sub_simplify_owned(next, correction));
    }
    expr_free(dxi);
    return coefficient;
}

/* Test infinitesimal invariance without assuming a particular equation family. */
expr_t *de_lie_residual(const de_lie_t *lie, const expr_t *xi, const expr_t *eta)
{
    if (!de_lie_point_components(lie, xi, eta))
        return NULL;
    expr_t *first = de_lie_prolongation(lie, xi, eta, 1u);
    expr_t *residual = de_lie_prolongation(lie, xi, eta, 2u);
    const expr_t *components[3] = {xi, eta, first};
    for (size_t i = 0u; i < 3u; ++i) {
        expr_t *term = expr_mul_simplify_owned(expr_clone(components[i]),
                                               expr_create_deriv(lie->rhs, lie->coordinate[i]));
        residual = expr_sub_simplify_owned(residual, term);
    }
    expr_free(first);
    residual = de_lie_simplify(residual);
    if (residual && !expr_is_exact_zero(residual) && de_lie_zero_polynomial(lie, residual, 0u)) {
        expr_free(residual);
        residual = expr_const_zero();
    }
    return residual;
}

/* Construct the determining PDE with native arbitrary functions and derivatives. */
equation_t *de_lie_determining_equation(const de_lie_t *lie)
{
    if (!lie)
        return NULL;
    expr_t *arguments[2] = {lie->coordinate[0], lie->coordinate[1]};
    expr_t *xi = expr_new_arbitrary_function_n("ξ", 2u, arguments);
    expr_t *eta = expr_new_arbitrary_function_n("η", 2u, arguments);
    expr_t *residual = de_lie_residual(lie, xi, eta);
    expr_t *zero = expr_const_zero();
    equation_t *equation = residual && zero ? equ_new(residual, zero) : NULL;
    expr_free(zero);
    expr_free(residual);
    expr_free(eta);
    expr_free(xi);
    return equation;
}

/* Compute the two relative invariants, not a pre-tabulated diagnostic. */
expr_t *de_lie_invariant(const de_lie_t *lie, size_t index)
{
    if (!lie || index > 1u)
        return NULL;
    expr_t *fp = expr_create_deriv(lie->rhs, lie->coordinate[2]);
    expr_t *fpp = fp ? expr_create_deriv(fp, lie->coordinate[2]) : NULL;
    expr_t *result = NULL;
    if (index == 0u) {
        expr_t *third = fpp ? expr_create_deriv(fpp, lie->coordinate[2]) : NULL;
        result = third ? expr_create_deriv(third, lie->coordinate[2]) : NULL;
        expr_free(third);
    } else {
        expr_t *fy = expr_create_deriv(lie->rhs, lie->coordinate[1]);
        expr_t *fyp = fy ? expr_create_deriv(fy, lie->coordinate[2]) : NULL;
        expr_t *dfpp = de_lie_total_derivative(lie, fpp);
        expr_t *dfyp = de_lie_total_derivative(lie, fyp);
        expr_t *fyy = fy ? expr_create_deriv(fy, lie->coordinate[1]) : NULL;
        result = de_lie_total_derivative(lie, dfpp);
        result = expr_sub_simplify_owned(result, expr_mul_long(dfyp, 4L));
        result = expr_sub_simplify_owned(result, expr_mul(fp, dfpp));
        result = expr_add_simplify_owned(result, expr_mul_long(fyy, 6L));
        result = expr_sub_simplify_owned(result, expr_mul_simplify_owned(expr_mul_long(fy, 3L), expr_clone(fpp)));
        result = expr_add_simplify_owned(result, expr_mul_simplify_owned(expr_mul_long(fp, 4L), expr_clone(fyp)));
        expr_free(fyy);
        expr_free(dfyp);
        expr_free(dfpp);
        expr_free(fyp);
        expr_free(fy);
    }
    expr_free(fpp);
    expr_free(fp);
    return de_lie_simplify(result);
}

/* A bounded, directly indexed coefficient space avoids symbolic term searches. */
enum { DE_LIE_RADIX = 16, DE_LIE_COEFFICIENTS = 16 * 16 * 16 };

static bool de_lie_collect(const de_lie_t *lie, const expr_t *expr, size_t coordinate, size_t key,
                           expr_t **slots, size_t stride, size_t column)
{
    if (!expr)
        return false;
    if (expr_is_exact_zero(expr))
        return true;
    if (coordinate == 3u) {
        number_t value = num_new();
        bool valid = expr_match_const_value(expr, &value) && num_is_real(value) && num_is_finite(value);
        num_destroy(&value);
        if (valid)
            slots[key * stride + column] = expr_clone(expr);
        return valid && slots[key * stride + column];
    }
    expr_t **coefficients = NULL;
    size_t degree = 0u;
    bool valid = equ_collect_symbolic_polynomial_alloc(expr, lie->coordinate[coordinate], &coefficients, &degree);
    valid = valid && degree < DE_LIE_RADIX;
    for (size_t i = 0u; valid && i <= degree; ++i)
        valid = de_lie_collect(lie, coefficients[i], coordinate + 1u, key * DE_LIE_RADIX + i,
                               slots, stride, column);
    if (coefficients) {
        for (size_t i = 0u; i <= degree; ++i)
            expr_free(coefficients[i]);
    }
    free(coefficients);
    return valid;
}

static const expr_t *de_lie_entry(const matrix_t *matrix, size_t row, size_t column)
{
    expr_t *entry = NULL;
    if (!matrix || mat_typeof(matrix) != MAT_TYPE_EXPR || row >= mat_get_row_count(matrix) ||
        column >= mat_get_col_count(matrix))
        return NULL;
    mat_get(matrix, row, column, &entry);
    /* Dense and sparse expression matrices both permit structural zero entries. */
    return entry ? entry : EXPR_ZERO;
}

/* Flatten one or two polynomial components into an exact coefficient matrix. */
static matrix_t *de_lie_coefficient_matrix(const de_lie_t *lie, const matrix_t *expressions)
{
    if (!lie || !expressions)
        return NULL;
    size_t components = mat_get_row_count(expressions), columns = mat_get_col_count(expressions);
    if (!components || components > 2u || !columns || columns > 930u)
        return NULL;
    size_t keys = components * DE_LIE_COEFFICIENTS;
    expr_t **slots = calloc(keys * columns, sizeof(*slots));
    matrix_t *matrix = NULL;
    bool valid = slots != NULL;
    for (size_t i = 0u; valid && i < components; ++i) {
        for (size_t j = 0u; valid && j < columns; ++j)
            valid = de_lie_collect(lie, de_lie_entry(expressions, i, j), 0u, i, slots, columns, j);
    }
    size_t rows = 0u;
    /* The finite 4096-key grid is traversed once to compact occupied rows. */
    for (size_t key = 0u; valid && key < keys; ++key) {
        bool occupied = false;
        for (size_t j = 0u; j < columns; ++j)
            occupied = occupied || slots[key * columns + j] != NULL;
        rows += occupied;
    }
    if (valid) {
        matrix = mat_new_expr(rows ? rows : 1u, columns);
        expr_t *zero = expr_const_zero();
        for (size_t i = 0u; matrix && zero && i < (rows ? rows : 1u); ++i) {
            for (size_t j = 0u; j < columns; ++j)
                mat_set(matrix, i, j, &zero);
        }
        expr_free(zero);
    }
    size_t row = 0u;
    for (size_t key = 0u; matrix && key < keys; ++key) {
        bool occupied = false;
        for (size_t j = 0u; j < columns; ++j) {
            expr_t *entry = slots[key * columns + j];
            if (entry) {
                mat_set(matrix, row, j, &entry);
                occupied = true;
            }
        }
        row += occupied;
    }
    if (slots) {
        for (size_t i = 0u; i < keys * columns; ++i)
            expr_free(slots[i]);
    }
    free(slots);
    return matrix;
}

/* Solve the determining equation in a declared finite polynomial space. */
matrix_t *de_lie_polynomial_generators(const de_lie_t *lie, size_t degree)
{
    if (!lie || degree > 4u)
        return NULL;
    size_t count = (degree + 1u) * (degree + 2u) / 2u;
    expr_t *monomials[15] = {0};
    expr_t *powers[2][5] = {{0}};
    matrix_t *residuals = mat_new_expr(1u, 2u * count), *coefficients = NULL, *kernel = NULL, *generators = NULL;
    expr_t *zero = expr_const_zero();
    bool valid = residuals && zero;
    for (size_t c = 0u; c < 2u; ++c) {
        powers[c][0] = expr_const_one();
        for (size_t k = 1u; k <= degree; ++k)
            powers[c][k] = expr_mul_simplify_owned(expr_clone(powers[c][k - 1u]), expr_clone(lie->coordinate[c]));
    }
    size_t index = 0u;
    for (size_t total = 0u; valid && total <= degree; ++total) {
        for (size_t j = 0u; valid && j <= total; ++j, ++index) {
            monomials[index] = expr_mul_simplify_owned(expr_clone(powers[0][total - j]), expr_clone(powers[1][j]));
            for (size_t c = 0u; valid && c < 2u; ++c) {
                expr_t *residual = de_lie_residual(lie, c == 0u ? monomials[index] : zero,
                                                  c == 1u ? monomials[index] : zero);
                valid = residual != NULL;
                if (valid)
                    mat_set(residuals, 0u, c * count + index, &residual);
                expr_free(residual);
            }
        }
    }
    coefficients = valid ? de_lie_coefficient_matrix(lie, residuals) : NULL;
    kernel = coefficients ? mat_nullspace(coefficients) : NULL;
    generators = kernel ? mat_new_expr(2u, mat_get_col_count(kernel)) : NULL;
    for (size_t j = 0u; generators && j < mat_get_col_count(generators); ++j) {
        for (size_t c = 0u; valid && c < 2u; ++c) {
            expr_t *component = expr_const_zero();
            for (size_t i = 0u; i < count; ++i)
                component = expr_add_simplify_owned(component,
                                                     expr_mul(monomials[i], de_lie_entry(kernel, c * count + i, j)));
            component = de_lie_simplify(component);
            valid = component != NULL;
            if (valid)
                mat_set(generators, c, j, &component);
            expr_free(component);
        }
        expr_t *check = valid ? de_lie_residual(lie, de_lie_entry(generators, 0u, j),
                                               de_lie_entry(generators, 1u, j)) : NULL;
        valid = check && expr_is_exact_zero(check);
        expr_free(check);
        if (!valid)
            break;
    }
    if (!valid) {
        mat_free(generators);
        generators = NULL;
    }
    mat_free(kernel);
    mat_free(coefficients);
    mat_free(residuals);
    for (size_t i = 0u; i < count; ++i)
        expr_free(monomials[i]);
    for (size_t c = 0u; c < 2u; ++c) {
        for (size_t k = 0u; k <= degree; ++k)
            expr_free(powers[c][k]);
    }
    expr_free(zero);
    return generators;
}

/* Compute the commutator by differentiating the supplied vector fields. */
matrix_t *de_lie_bracket(const de_lie_t *lie, const matrix_t *generators, size_t i, size_t j)
{
    if (!lie || !generators || mat_typeof(generators) != MAT_TYPE_EXPR || mat_get_row_count(generators) != 2u ||
        i >= mat_get_col_count(generators) || j >= mat_get_col_count(generators))
        return NULL;
    const expr_t *left[2] = {de_lie_entry(generators, 0u, i), de_lie_entry(generators, 1u, i)};
    const expr_t *right[2] = {de_lie_entry(generators, 0u, j), de_lie_entry(generators, 1u, j)};
    if (!de_lie_point_components(lie, left[0], left[1]) || !de_lie_point_components(lie, right[0], right[1]))
        return NULL;
    matrix_t *bracket = mat_new_expr(2u, 1u);
    for (size_t c = 0u; bracket && c < 2u; ++c) {
        expr_t *component = expr_const_zero();
        for (size_t k = 0u; k < 2u; ++k) {
            component = expr_add_simplify_owned(component, expr_mul_simplify_owned(expr_clone(left[k]),
                                                 expr_create_deriv(right[c], lie->coordinate[k])));
            component = expr_sub_simplify_owned(component, expr_mul_simplify_owned(expr_clone(right[k]),
                                                 expr_create_deriv(left[c], lie->coordinate[k])));
        }
        component = de_lie_simplify(component);
        if (component)
            mat_set(bracket, c, 0u, &component);
        else {
            mat_free(bracket);
            bracket = NULL;
        }
        expr_free(component);
    }
    return bracket;
}

/* Solve for constant bracket coefficients and verify closure exactly. */
matrix_t *de_lie_structure_constants(const de_lie_t *lie, const matrix_t *generators)
{
    if (!lie || !generators)
        return NULL;
    size_t count = mat_get_col_count(generators);
    if (mat_typeof(generators) != MAT_TYPE_EXPR ||
        mat_get_row_count(generators) != 2u || count > 30u)
        return NULL;
    if (!count)
        return mat_new_expr(0u, 0u);
    matrix_t *expressions = mat_new_expr(2u, count + count * count);
    matrix_t *coefficients = NULL, *basis = NULL, *brackets = NULL, *transpose = NULL;
    matrix_t *gram = NULL, *right = NULL, *solution = NULL, *product = NULL, *result = NULL;
    bool valid = expressions != NULL;
    for (size_t i = 0u; valid && i < count; ++i) {
        for (size_t c = 0u; c < 2u; ++c) {
            const expr_t *entry = de_lie_entry(generators, c, i);
            mat_set(expressions, c, i, &entry);
        }
        for (size_t j = 0u; valid && j < count; ++j) {
            matrix_t *bracket = de_lie_bracket(lie, generators, i, j);
            valid = bracket != NULL;
            for (size_t c = 0u; valid && c < 2u; ++c) {
                const expr_t *entry = de_lie_entry(bracket, c, 0u);
                mat_set(expressions, c, count + i * count + j, &entry);
            }
            mat_free(bracket);
        }
    }
    coefficients = valid ? de_lie_coefficient_matrix(lie, expressions) : NULL;
    size_t rows = coefficients ? mat_get_row_count(coefficients) : 0u;
    basis = coefficients ? mat_new_expr(rows, count) : NULL;
    brackets = coefficients ? mat_new_expr(rows, count * count) : NULL;
    for (size_t i = 0u; basis && brackets && i < rows; ++i) {
        for (size_t j = 0u; j < count + count * count; ++j) {
            const expr_t *entry = de_lie_entry(coefficients, i, j);
            mat_set(j < count ? basis : brackets, i, j < count ? j : j - count, &entry);
        }
    }
    /* For real constant coefficients, A^T A is invertible precisely when the basis is independent. */
    transpose = basis ? mat_transpose(basis) : NULL;
    gram = transpose ? mat_mul(transpose, basis) : NULL;
    right = transpose && brackets ? mat_mul(transpose, brackets) : NULL;
    solution = gram && right ? mat_solve(gram, right) : NULL;
    product = solution ? mat_mul(basis, solution) : NULL;
    valid = product != NULL;
    for (size_t i = 0u; valid && i < rows; ++i) {
        for (size_t j = 0u; valid && j < count * count; ++j) {
            expr_t *difference = de_lie_simplify(expr_sub(de_lie_entry(product, i, j), de_lie_entry(brackets, i, j)));
            valid = difference && expr_is_exact_zero(difference);
            expr_free(difference);
        }
    }
    for (size_t i = 0u; valid && i < count; ++i) {
        for (size_t j = 0u; valid && j < count * count; ++j) {
            number_t value = num_new();
            valid = expr_match_const_value(de_lie_entry(solution, i, j), &value) &&
                    num_is_real(value) && num_is_finite(value);
            num_destroy(&value);
        }
    }
    result = valid ? mat_transpose(solution) : NULL;
    mat_free(product);
    mat_free(solution);
    mat_free(right);
    mat_free(gram);
    mat_free(transpose);
    mat_free(brackets);
    mat_free(basis);
    mat_free(coefficients);
    mat_free(expressions);
    return result;
}

/* Use the verified translation symmetry to lower the order locally. */
equation_t *de_lie_autonomous_reduction(const de_lie_t *lie)
{
    if (!lie || de_expr_uses(lie->rhs, lie->coordinate[0]))
        return NULL;
    expr_t *wrt = lie->coordinate[1];
    expr_t *derivative = expr_new_formal_derivative(lie->coordinate[2], 1u, &wrt);
    expr_t *left = derivative ? expr_mul(lie->coordinate[2], derivative) : NULL;
    equation_t *reduced = left ? equ_new(left, lie->rhs) : NULL;
    expr_free(left);
    expr_free(derivative);
    return reduced;
}

/* Keep symmetry analysis separate from claiming a closed-form solution. */
diffequ_solve_result_t *de_lie_unsolved_analysis(const diffequ_t *de, bool include_steps)
{
    de_lie_t *lie = de_lie_new(de);
    if (!lie)
        return NULL;
    expr_t *invariants[2] = {de_lie_invariant(lie, 0u), de_lie_invariant(lie, 1u)};
    char *plain[2] = {expr_to_string(invariants[0], style_UNBOUND), expr_to_string(invariants[1], style_UNBOUND)};
    string_t *diagnostic = invariants[0] && invariants[1] && plain[0] && plain[1]
                              ? string_sprintf("no supported closed-form solution; computed Lie–Tressé invariants: "
                                               "I1 = %s; I2 = %s", plain[0], plain[1]) : NULL;
    diffequ_solve_result_t *result = diagnostic ? de_solve_result_new(DE_SOLVE_STATUS_UNSUPPORTED, DE_SOLVER_NONE,
                                                                     string_c_str(diagnostic)) : NULL;
    if (result && include_steps) {
        matrix_t *generators = de_lie_polynomial_generators(lie, 2u);
        equation_t *reduced = de_lie_autonomous_reduction(lie);
        string_t *steps = string_sprintf("Lie point-symmetry analysis of the normalised second-order equation.\n"
                                         "Computed Lie–Tressé invariants: I1 = %s; I2 = %s.\n"
                                         "Both must vanish identically for local point-linearisation.\n", plain[0], plain[1]);
        char *tex[2] = {expr_to_TeX_body(invariants[0]), expr_to_TeX_body(invariants[1])};
        string_t *steps_TeX = tex[0] && tex[1] ? string_sprintf("\\begin{aligned}&I_1=%s\\\\&I_2=%s",
                                                               tex[0], tex[1]) : NULL;
        if (steps && generators) {
            size_t count = mat_get_col_count(generators);
            string_append_format(steps, "Polynomial generator search (total degree <= 2): %zu verified.\n", count);
            if (steps_TeX)
                string_append_format(steps_TeX, "\\\\&\\text{Polynomial search: total degree }\\le2;\\quad"
                                                "%zu\\text{ verified generators}", count);
            for (size_t j = 0u; j < count; ++j) {
                for (size_t c = 0u; c < 2u; ++c) {
                    const expr_t *entry = de_lie_entry(generators, c, j);
                    char *component = expr_to_string(entry, style_UNBOUND);
                    char *component_TeX = expr_to_TeX_body(entry);
                    if (component)
                        string_append_format(steps, "G%zu: %s = %s\n", j + 1u, c ? "eta" : "xi", component);
                    if (steps_TeX && component_TeX)
                        string_append_format(steps_TeX, "\\\\&%s_{%zu}=%s", c ? "\\eta" : "\\xi", j + 1u,
                                              component_TeX);
                    free(component_TeX);
                    free(component);
                }
            }
            string_append_cstr(steps, "This finite search does not exhaust non-polynomial symmetries.\n");
            if (steps_TeX)
                string_append_cstr(steps_TeX, "\\\\&\\text{Non-polynomial symmetries are not exhausted by this search.}");
        } else if (steps) {
            string_append_cstr(steps, "Polynomial coefficient search is unsupported for this normal form.\n");
            if (steps_TeX)
                string_append_cstr(steps_TeX, "\\\\&\\text{Polynomial coefficient search is unsupported.}");
        }
        if (steps && reduced) {
            string_t *reduction = equ_to_text(reduced, style_UNBOUND);
            if (reduction)
                string_append_format(steps, "Translation symmetry gives the local order reduction: %s.\n"
                                            "Here the velocity is a function of the dependent coordinate; "
                                            "check equilibria separately in the original ODE.\n", string_c_str(reduction));
            string_free(reduction);
            char *lhs = expr_to_TeX_body_wrapped_with_totals(equ_lhs(reduced), 0u);
            char *rhs = expr_to_TeX_body(equ_rhs(reduced));
            if (steps_TeX && lhs && rhs)
                string_append_format(steps_TeX, "\\\\&\\text{Local order reduction:}\\quad %s=%s"
                                                "\\\\&\\text{Check equilibria separately in the original ODE.}", lhs, rhs);
            free(rhs);
            free(lhs);
        }
        if (steps_TeX)
            string_append_cstr(steps_TeX, "\\end{aligned}");
        if (!steps || !steps_TeX || de_solve_result_set_steps(result, string_c_str(steps)) != 0 ||
            de_solve_result_set_steps_TeX(result, string_c_str(steps_TeX)) != 0) {
            de_solve_result_free(result);
            result = NULL;
        }
        free(tex[1]);
        free(tex[0]);
        string_free(steps_TeX);
        string_free(steps);
        equ_free(reduced);
        mat_free(generators);
    }
    string_free(diagnostic);
    for (size_t i = 0u; i < 2u; ++i) {
        free(plain[i]);
        expr_free(invariants[i]);
    }
    de_lie_free(lie);
    return result;
}
