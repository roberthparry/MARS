#include <stdbool.h>
#include <stdlib.h>

#include "equation.h"
#include "equation_internal.h"
#include "expression/expr_stringin_internal.h"
#include "expression.h"
#include "internal/expr_internal.h"

struct equation_t {
    expr_t *lhs;
    expr_t *rhs;
};

static void equation_solve_result_reset(equation_solve_result_t *result)
{
    if (!result)
        return;
    result->solutions = NULL;
    result->count = 0u;
    result->status = EQUATION_SOLVE_INVALID;
}

equation_t *equation_new(const expr_t *lhs, const expr_t *rhs)
{
    equation_t *equation;

    if (!lhs || !rhs)
        return NULL;

    equation = calloc(1u, sizeof(*equation));
    if (!equation)
        return NULL;

    equation->lhs = (expr_t *)lhs;
    equation->rhs = (expr_t *)rhs;
    expr_retain(equation->lhs);
    expr_retain(equation->rhs);
    return equation;
}

void equation_free(equation_t *equation)
{
    if (!equation)
        return;
    expr_free(equation->lhs);
    expr_free(equation->rhs);
    free(equation);
}

const expr_t *equation_lhs(const equation_t *equation)
{
    return equation ? equation->lhs : NULL;
}

const expr_t *equation_rhs(const equation_t *equation)
{
    return equation ? equation->rhs : NULL;
}

expr_t *equation_residual(const equation_t *equation)
{
    expr_t *raw;
    expr_t *residual;

    if (!equation)
        return NULL;

    raw = expr_sub(equation->lhs, equation->rhs);
    residual = raw ? expr_simplify(raw) : NULL;
    expr_free(raw);
    return residual;
}

static bool equation_expr_uses_wrt(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    bool used = false;

    if (!expr || !wrt)
        return false;

    vars[0] = (expr_t *)wrt;
    return expr_collect_var_usage(expr, 1u, vars, &used) && used;
}

static bool equation_lhs_is_wrt(const equation_t *equation, const expr_t *wrt)
{
    expr_t *vars[1];
    size_t index = 0u;

    if (!equation || !wrt)
        return false;

    vars[0] = (expr_t *)wrt;
    return expr_match_var_expr(equation->lhs, 1u, vars, &index) && index == 0u;
}

bool equation_is_solved_for(const equation_t *equation, const expr_t *wrt)
{
    return equation_lhs_is_wrt(equation, wrt) &&
           !equation_expr_uses_wrt(equation->rhs, wrt);
}

static int equation_solve_result_append(equation_solve_result_t *result,
                                        equation_t *solution)
{
    equation_t **items;

    if (!result || !solution)
        return -1;

    items = realloc(result->solutions,
                    (result->count + 1u) * sizeof(*result->solutions));
    if (!items)
        return -1;

    result->solutions = items;
    result->solutions[result->count++] = solution;
    result->status = EQUATION_SOLVE_SOLVED;
    return 0;
}

static int equation_append_existing_solution(const equation_t *equation,
                                             equation_solve_result_t *result)
{
    equation_t *solution = equation_new(equation->lhs, equation->rhs);

    if (!solution)
        return -1;

    if (equation_solve_result_append(result, solution) != 0) {
        equation_free(solution);
        return -1;
    }

    return 0;
}

int equation_append_solution_value(const expr_t *wrt,
                                   number_t value,
                                   equation_solve_result_t *result)
{
    expr_t *rhs = expr_new_const(value);
    equation_t *solution = rhs ? equation_new(wrt, rhs) : NULL;
    int rc = -1;

    if (!solution)
        goto cleanup;

    if (equation_solve_result_append(result, solution) != 0)
        goto cleanup;

    solution = NULL;
    rc = 0;

cleanup:
    equation_free(solution);
    expr_free(rhs);
    return rc;
}

int equation_append_solution_expr(const expr_t *wrt,
                                  const expr_t *rhs,
                                  equation_solve_result_t *result)
{
    equation_t *solution = equation_new(wrt, rhs);

    if (!solution)
        return -1;

    if (equation_solve_result_append(result, solution) != 0) {
        equation_free(solution);
        return -1;
    }

    return 0;
}

static expr_t *equation_simplify_owned(expr_t *expr);

static int equation_try_solve_symbolic_affine(const expr_t *residual,
                                              const expr_t *wrt,
                                              equation_solve_result_t *result);

static int equation_try_solve_affine(const equation_t *equation,
                                     const expr_t *wrt,
                                     equation_solve_result_t *result)
{
    expr_t *residual = equation_residual(equation);
    number_t constant = num_new();
    number_t coeff = num_new();
    number_t neg_constant;
    number_t solution_value;
    bool ok;
    int rc = -1;

    if (!residual)
        goto cleanup;

    ok = equation_match_affine_linear_expr(residual, wrt, true, &constant, &coeff);
    if (!ok) {
        rc = equation_try_solve_symbolic_affine(residual, wrt, result);
        goto cleanup;
    }

    neg_constant = num_neg(constant);
    solution_value = num_div(neg_constant, coeff);
    if (equation_append_solution_value(wrt, solution_value, result) != 0) {
        num_destroy(&solution_value);
        num_destroy(&neg_constant);
        goto cleanup;
    }

    num_destroy(&solution_value);
    num_destroy(&neg_constant);
    rc = 0;

cleanup:
    num_destroy(&coeff);
    num_destroy(&constant);
    expr_free(residual);
    return rc;
}

static number_t equation_quadratic_discriminant(number_t constant,
                                                number_t linear,
                                                number_t quadratic)
{
    number_t linear_sq = num_mul(linear, linear);
    number_t quad_constant = num_mul(quadratic, constant);
    number_t four_quad_constant = num_mul_long(quad_constant, 4L);
    number_t discriminant = num_sub(linear_sq, four_quad_constant);

    num_destroy(&four_quad_constant);
    num_destroy(&quad_constant);
    num_destroy(&linear_sq);
    return discriminant;
}

static number_t equation_quadratic_root(number_t neg_linear,
                                        number_t signed_sqrt_discriminant,
                                        number_t denominator)
{
    number_t numerator = num_add(neg_linear, signed_sqrt_discriminant);
    number_t root = num_div(numerator, denominator);

    num_destroy(&numerator);
    return root;
}

static expr_t *equation_simplify_owned(expr_t *expr)
{
    expr_t *simplified;

    if (!expr)
        return NULL;

    simplified = expr_simplify(expr);
    expr_free(expr);
    return simplified;
}

static expr_t *equation_symbolic_linear_root(const expr_t *constant,
                                             const expr_t *linear)
{
    expr_t *neg_constant = expr_neg(constant);
    expr_t *quotient = neg_constant ? expr_div(neg_constant, linear) : NULL;
    expr_t *root = equation_simplify_owned(quotient);

    expr_free(neg_constant);
    return root;
}

static int equation_try_solve_symbolic_affine(const expr_t *residual,
                                              const expr_t *wrt,
                                              equation_solve_result_t *result)
{
    expr_t *constant = NULL;
    expr_t *linear = NULL;
    expr_t *root = NULL;
    int rc = -1;

    if (!equation_match_symbolic_linear_expr(residual, wrt, &constant,
                                             &linear)) {
        rc = 1;
        goto cleanup;
    }

    root = equation_symbolic_linear_root(constant, linear);
    if (!root)
        goto cleanup;

    if (equation_append_solution_expr(wrt, root, result) != 0)
        goto cleanup;

    rc = 0;

cleanup:
    expr_free(root);
    expr_free(linear);
    expr_free(constant);
    return rc;
}

static expr_t *equation_symbolic_quadratic_discriminant(const expr_t *constant,
                                                        const expr_t *linear,
                                                        const expr_t *quadratic)
{
    number_t four = num_create_from_long(4L);
    expr_t *linear_sq = expr_pow(linear, &NUM_TWO);
    expr_t *quad_constant = expr_mul(quadratic, constant);
    expr_t *four_quad_constant = quad_constant
        ? expr_mul_num(quad_constant, &four)
        : NULL;
    expr_t *discriminant = (linear_sq && four_quad_constant)
        ? expr_sub(linear_sq, four_quad_constant)
        : NULL;
    expr_t *out = equation_simplify_owned(discriminant);

    expr_free(four_quad_constant);
    expr_free(quad_constant);
    expr_free(linear_sq);
    num_destroy(&four);
    return out;
}

static expr_t *equation_symbolic_quadratic_root(const expr_t *linear,
                                                const expr_t *quadratic,
                                                const expr_t *sqrt_discriminant,
                                                bool add_root)
{
    number_t two = num_create_from_long(2L);
    expr_t *neg_linear = expr_neg(linear);
    expr_t *numerator = add_root
        ? ((neg_linear && sqrt_discriminant)
            ? expr_add(neg_linear, sqrt_discriminant)
            : NULL)
        : ((neg_linear && sqrt_discriminant)
            ? expr_sub(neg_linear, sqrt_discriminant)
            : NULL);
    expr_t *denominator = expr_mul_num(quadratic, &two);
    expr_t *quotient = (numerator && denominator)
        ? expr_div(numerator, denominator)
        : NULL;
    expr_t *root = equation_simplify_owned(quotient);

    expr_free(denominator);
    expr_free(numerator);
    expr_free(neg_linear);
    num_destroy(&two);
    return root;
}

static int equation_try_solve_symbolic_quadratic(const expr_t *residual,
                                                 const expr_t *wrt,
                                                 equation_solve_result_t *result)
{
    expr_t *constant = NULL;
    expr_t *linear = NULL;
    expr_t *quadratic = NULL;
    expr_t *discriminant = NULL;
    expr_t *sqrt_discriminant = NULL;
    expr_t *root_plus = NULL;
    expr_t *root_minus = NULL;
    int rc = -1;

    if (!equation_match_symbolic_quadratic_expr(residual, wrt, &constant,
                                                &linear, &quadratic)) {
        rc = 1;
        goto cleanup;
    }

    discriminant = equation_symbolic_quadratic_discriminant(constant, linear,
                                                            quadratic);
    sqrt_discriminant = discriminant ? expr_sqrt(discriminant) : NULL;
    sqrt_discriminant = equation_simplify_owned(sqrt_discriminant);
    root_plus = sqrt_discriminant
        ? equation_symbolic_quadratic_root(linear, quadratic,
                                           sqrt_discriminant, true)
        : NULL;
    root_minus = sqrt_discriminant
        ? equation_symbolic_quadratic_root(linear, quadratic,
                                           sqrt_discriminant, false)
        : NULL;

    if (!root_plus || !root_minus)
        goto cleanup;

    if (equation_append_solution_expr(wrt, root_plus, result) != 0)
        goto cleanup;
    if (equation_append_solution_expr(wrt, root_minus, result) != 0) {
        equation_solve_result_clear(result);
        goto cleanup;
    }

    rc = 0;

cleanup:
    expr_free(root_minus);
    expr_free(root_plus);
    expr_free(sqrt_discriminant);
    expr_free(discriminant);
    expr_free(quadratic);
    expr_free(linear);
    expr_free(constant);
    return rc;
}

static int equation_try_solve_quadratic(const equation_t *equation,
                                        const expr_t *wrt,
                                        equation_solve_result_t *result)
{
    expr_t *residual = equation_residual(equation);
    number_t constant = num_new();
    number_t linear = num_new();
    number_t quadratic = num_new();
    number_t discriminant;
    number_t sqrt_discriminant;
    number_t neg_sqrt_discriminant;
    number_t neg_linear;
    number_t denominator;
    number_t root;
    bool ok;
    int rc = -1;

    if (!residual)
        goto cleanup;

    ok = equation_match_quadratic_expr(residual, wrt, &constant, &linear,
                                       &quadratic);
    if (!ok) {
        rc = equation_try_solve_symbolic_quadratic(residual, wrt, result);
        goto cleanup;
    }

    discriminant = equation_quadratic_discriminant(constant, linear, quadratic);
    sqrt_discriminant = num_sqrt(discriminant);
    neg_linear = num_neg(linear);
    denominator = num_mul_long(quadratic, 2L);

    root = equation_quadratic_root(neg_linear, sqrt_discriminant, denominator);
    if (equation_append_solution_value(wrt, root, result) != 0) {
        num_destroy(&root);
        num_destroy(&denominator);
        num_destroy(&neg_linear);
        num_destroy(&sqrt_discriminant);
        num_destroy(&discriminant);
        goto cleanup;
    }
    num_destroy(&root);

    if (!num_is_zero(sqrt_discriminant)) {
        neg_sqrt_discriminant = num_neg(sqrt_discriminant);
        root = equation_quadratic_root(neg_linear, neg_sqrt_discriminant,
                                       denominator);
        num_destroy(&neg_sqrt_discriminant);
        if (equation_append_solution_value(wrt, root, result) != 0) {
            num_destroy(&root);
            num_destroy(&denominator);
            num_destroy(&neg_linear);
            num_destroy(&sqrt_discriminant);
            num_destroy(&discriminant);
            goto cleanup;
        }
        num_destroy(&root);
    }

    num_destroy(&denominator);
    num_destroy(&neg_linear);
    num_destroy(&sqrt_discriminant);
    num_destroy(&discriminant);
    rc = 0;

cleanup:
    num_destroy(&quadratic);
    num_destroy(&linear);
    num_destroy(&constant);
    expr_free(residual);
    return rc;
}

static int equation_append_numeric_binding_solutions(expr_bindings_t *bindings,
                                                     equation_solve_result_t *result)
{
    if (!bindings || !result)
        return -1;

    for (size_t i = 0u; i < bindings->count; ++i) {
        expr_binding_entry_t *entry = &bindings->entries[i];
        number_t value;
        expr_t *rhs;
        equation_t *solution;

        if (entry->is_constant)
            continue;

        value = expr_eval(entry->expr);
        rhs = expr_new_const(value);
        num_destroy(&value);
        if (!rhs)
            return -1;

        solution = equation_new(entry->expr, rhs);
        expr_free(rhs);
        if (!solution)
            return -1;

        if (equation_solve_result_append(result, solution) != 0) {
            equation_free(solution);
            return -1;
        }
    }

    return result->count > 0u ? 0 : 1;
}

static size_t equation_numeric_precision_digits(const expr_goal_seek_options_t *options)
{
    if (options && options->precision_digits > 0u)
        return options->precision_digits;
    return 64u;
}

static number_t equation_numeric_tolerance(const expr_goal_seek_options_t *options)
{
    if (options && num_is_finite(options->tolerance) &&
        !num_is_zero(options->tolerance))
        return num_abs(options->tolerance);

    return num_pow10(-(int)equation_numeric_precision_digits(options));
}

static bool equation_numeric_residual_is_solved(const expr_t *residual,
                                                const expr_goal_seek_options_t *options)
{
    number_t value;
    number_t mag;
    number_t tolerance;
    bool ok;

    if (!residual)
        return false;

    value = expr_eval(residual);
    if (!num_is_finite(value)) {
        num_destroy(&value);
        return false;
    }

    mag = num_abs(value);
    tolerance = equation_numeric_tolerance(options);
    ok = num_is_finite(mag) && num_le(mag, tolerance);

    num_destroy(&tolerance);
    num_destroy(&mag);
    num_destroy(&value);
    return ok;
}

static number_t *equation_snapshot_binding_values(expr_bindings_t *bindings)
{
    number_t *values;

    if (!bindings || bindings->count == 0u)
        return NULL;

    values = calloc(bindings->count, sizeof(*values));
    if (!values)
        return NULL;

    for (size_t i = 0u; i < bindings->count; ++i)
        values[i] = expr_eval(bindings->entries[i].expr);
    return values;
}

static void equation_restore_binding_values(expr_bindings_t *bindings,
                                            number_t *values)
{
    if (!bindings || !values)
        return;

    for (size_t i = 0u; i < bindings->count; ++i)
        expr_set_val(bindings->entries[i].expr, values[i]);
}

static void equation_free_binding_value_snapshot(expr_bindings_t *bindings,
                                                 number_t *values)
{
    if (!bindings || !values)
        return;

    for (size_t i = 0u; i < bindings->count; ++i)
        num_destroy(&values[i]);
    free(values);
}

int equation_solve_numeric(const equation_t *equation,
                           expr_bindings_t *bindings,
                           const expr_goal_seek_options_t *options,
                           equation_solve_result_t *result)
{
    expr_t *residual = NULL;
    number_t target = num_create_from_long(0L);
    number_t *original_values = NULL;
    expr_goal_seek_result_t goal_result;
    int rc = -1;

    if (!result)
        return -1;

    equation_solve_result_reset(result);
    if (!equation || !bindings)
        goto cleanup_no_goal;

    result->status = EQUATION_SOLVE_UNSOLVED;
    residual = equation_residual(equation);
    if (!residual)
        goto cleanup_no_goal;
    original_values = equation_snapshot_binding_values(bindings);
    if (bindings->count > 0u && !original_values)
        goto cleanup_no_goal;

    if (expr_goal_seek(residual, bindings, target, options, &goal_result) != 0) {
        equation_restore_binding_values(bindings, original_values);
        rc = 0;
        goto cleanup_no_goal;
    }

    if (!equation_numeric_residual_is_solved(residual, options)) {
        equation_restore_binding_values(bindings, original_values);
        rc = 0;
        expr_goal_seek_result_clear(&goal_result);
        goto cleanup_no_goal;
    }

    rc = equation_append_numeric_binding_solutions(bindings, result);
    if (rc < 0)
        equation_solve_result_clear(result);
    else if (rc > 0)
        result->status = EQUATION_SOLVE_UNSOLVED;
    expr_goal_seek_result_clear(&goal_result);

cleanup_no_goal:
    equation_free_binding_value_snapshot(bindings, original_values);
    num_destroy(&target);
    expr_free(residual);
    return rc < 0 ? -1 : 0;
}

int equation_solve_for(const equation_t *equation,
                       const expr_t *wrt,
                       equation_solve_result_t *result)
{
    int affine_rc;
    int quadratic_rc;
    int cubic_rc;

    if (!result)
        return -1;

    equation_solve_result_reset(result);

    if (!equation || !wrt)
        return -1;

    result->status = EQUATION_SOLVE_UNSOLVED;

    if (equation_is_solved_for(equation, wrt))
        return equation_append_existing_solution(equation, result);

    affine_rc = equation_try_solve_affine(equation, wrt, result);
    if (affine_rc == 0)
        return 0;
    if (affine_rc < 0)
        return -1;

    quadratic_rc = equation_try_solve_quadratic(equation, wrt, result);
    if (quadratic_rc == 0)
        return 0;
    if (quadratic_rc < 0)
        return -1;

    cubic_rc = equation_try_solve_cubic(equation, wrt, result);
    if (cubic_rc == 0)
        return 0;
    if (cubic_rc < 0)
        return -1;

    return 0;
}

void equation_solve_result_clear(equation_solve_result_t *result)
{
    if (!result)
        return;

    for (size_t i = 0u; i < result->count; ++i)
        equation_free(result->solutions[i]);
    free(result->solutions);
    equation_solve_result_reset(result);
}
