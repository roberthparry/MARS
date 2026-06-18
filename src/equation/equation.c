#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "equation.h"
#include "equation_internal.h"
#include "expression.h"
#include "internal/expr_internal.h"
#include "ustring.h"

struct equation_t {
    expr_t *lhs;
    expr_t *rhs;
    expr_bindings_t *bindings;
};

static void equ_solutions_reset(equation_solutions_t *solutions)
{
    if (!solutions)
        return;
    solutions->solutions = NULL;
    solutions->count = 0u;
    solutions->status = EQUATION_SOLVE_INVALID;
}

static equation_solutions_t *equ_solutions_new(void)
{
    equation_solutions_t *solutions = calloc(1u, sizeof(*solutions));

    if (!solutions)
        return NULL;
    equ_solutions_reset(solutions);
    return solutions;
}

void equ_solutions_free(equation_solutions_t *solutions)
{
    if (!solutions)
        return;
    equ_solutions_clear(solutions);
    free(solutions);
}

equation_t *equ_new_with_owned_bindings(const expr_t *lhs,
                                             const expr_t *rhs,
                                             expr_bindings_t *bindings)
{
    equation_t *equation;

    if (!lhs || !rhs)
        return NULL;

    equation = calloc(1u, sizeof(*equation));
    if (!equation)
        return NULL;

    equation->lhs = (expr_t *)lhs;
    equation->rhs = (expr_t *)rhs;
    equation->bindings = bindings;
    expr_retain(equation->lhs);
    expr_retain(equation->rhs);
    return equation;
}

equation_t *equ_new(const expr_t *lhs, const expr_t *rhs)
{
    return equ_new_with_owned_bindings(lhs, rhs, NULL);
}

void equ_free(equation_t *equation)
{
    if (!equation)
        return;
    expr_bindings_free(equation->bindings);
    expr_free(equation->lhs);
    expr_free(equation->rhs);
    free(equation);
}

const expr_t *equ_lhs(const equation_t *equation)
{
    return equation ? equation->lhs : NULL;
}

const expr_t *equ_rhs(const equation_t *equation)
{
    return equation ? equation->rhs : NULL;
}

expr_bindings_t *equ_bindings(const equation_t *equation)
{
    return equation ? equation->bindings : NULL;
}

expr_bindings_t *equ_bindings_borrow(const equation_t *equation)
{
    return equ_bindings(equation);
}

expr_t *equ_binding(const equation_t *equation, const char *name)
{
    expr_bindings_t *bindings = equ_bindings(equation);

    if (!bindings || !name)
        return NULL;
    return expr_bindings_get(bindings, name);
}

expr_t *equ_residual(const equation_t *equation)
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

static bool equ_expr_uses_wrt(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    bool used = false;

    if (!expr || !wrt)
        return false;

    vars[0] = (expr_t *)wrt;
    return expr_collect_var_usage(expr, 1u, vars, &used) && used;
}

static bool equ_lhs_is_wrt(const equation_t *equation, const expr_t *wrt)
{
    expr_t *vars[1];
    size_t index = 0u;

    if (!equation || !wrt)
        return false;

    vars[0] = (expr_t *)wrt;
    return expr_match_var_expr(equation->lhs, 1u, vars, &index) && index == 0u;
}

bool equ_is_solved_for(const equation_t *equation, const expr_t *wrt)
{
    return equ_lhs_is_wrt(equation, wrt) &&
           !equ_expr_uses_wrt(equation->rhs, wrt);
}

static int equ_solutions_append(equation_solutions_t *solutions,
                                        equation_t *solution)
{
    equation_t **items;

    if (!solutions || !solution)
        return -1;

    for (size_t i = 0u; i < solutions->count; ++i) {
        equation_t *existing = solutions->solutions[i];

        if (!existing)
            continue;
        if (!expr_struct_eq(equ_lhs(existing), equ_lhs(solution)))
            continue;
        if (!expr_struct_eq(equ_rhs(existing), equ_rhs(solution)))
            continue;

        equ_free(solution);
        return 0;
    }

    items = realloc(solutions->solutions,
                    (solutions->count + 1u) * sizeof(*solutions->solutions));
    if (!items)
        return -1;

    solutions->solutions = items;
    solutions->solutions[solutions->count++] = solution;
    solutions->status = EQUATION_SOLVE_SOLVED;
    return 0;
}

static int equ_append_existing_solution(const equation_t *equation,
                                             equation_solutions_t *solutions)
{
    equation_t *solution = equ_new_with_owned_bindings(equation->lhs,
                                                            equation->rhs,
                                                            NULL);

    if (!solution)
        return -1;

    if (equ_solutions_append(solutions, solution) != 0) {
        equ_free(solution);
        return -1;
    }

    return 0;
}

static number_t *equ_snapshot_binding_values(expr_bindings_t *bindings);
static void equ_restore_binding_values(expr_bindings_t *bindings,
                                            number_t *values);
static void equ_free_binding_value_snapshot(expr_bindings_t *bindings,
                                                 number_t *values);

int equ_append_solution_value(const expr_t *wrt,
                                   number_t value,
                                   equation_solutions_t *solutions)
{
    expr_t *rhs = expr_new_const(value);
    equation_t *solution = rhs ? equ_new(wrt, rhs) : NULL;
    int rc = -1;

    if (!solution)
        goto cleanup;

    if (equ_solutions_append(solutions, solution) != 0)
        goto cleanup;

    solution = NULL;
    rc = 0;

cleanup:
    equ_free(solution);
    expr_free(rhs);
    return rc;
}

int equ_append_solution_expr(const expr_t *wrt,
                                  const expr_t *rhs,
                                  equation_solutions_t *solutions)
{
    equation_t *solution = equ_new(wrt, rhs);

    if (!solution)
        return -1;

    if (equ_solutions_append(solutions, solution) != 0) {
        equ_free(solution);
        return -1;
    }

    return 0;
}

static size_t equ_variable_binding_count(expr_bindings_t *bindings)
{
    size_t count = 0u;

    if (!bindings)
        return 0u;

    for (size_t i = 0u; i < bindings->count; ++i) {
        if (!bindings->entries[i].is_constant)
            ++count;
    }

    return count;
}

static int equ_merge_solutions(equation_solutions_t *dst,
                                    const equation_solutions_t *src)
{
    if (!dst || !src)
        return -1;

    for (size_t i = 0u; i < src->count; ++i) {
        const equation_t *solution = src->solutions[i];

        if (!solution)
            continue;
        if (equ_append_solution_expr(equ_lhs(solution),
                                          equ_rhs(solution),
                                          dst) != 0)
            return -1;
    }

    return 0;
}

static int equ_default_goal_seek_options(expr_goal_seek_options_t *options)
{
    size_t digits = num_get_default_prec_digits();

    if (!options)
        return -1;
    if (digits == 0u)
        digits = 64u;

    *options = (expr_goal_seek_options_t){
        .precision_digits = digits,
        .max_iterations = 0u,
        .allow_complex = true,
        .simplify_result = false,
        .tolerance = num_new()
    };
    return 0;
}

static int equ_derive_symbolic_solutions(const equation_t *equation,
                                              expr_bindings_t *bindings,
                                              equation_solutions_t *solutions)
{
    number_t *original_values = NULL;
    int rc = 0;

    if (!bindings)
        return 0;
    if (bindings->count > 0u) {
        original_values = equ_snapshot_binding_values(bindings);
        if (!original_values)
            return -1;
    }

    for (size_t i = 0u; i < bindings->count; ++i) {
        expr_binding_entry_t *entry = &bindings->entries[i];

        if (!entry->is_constant)
            expr_set_val(entry->expr, NUM_NAN);
    }

    for (size_t i = 0u; i < bindings->count; ++i) {
        expr_binding_entry_t *entry = &bindings->entries[i];
        equation_solutions_t partial;
        int solve_rc;

        if (entry->is_constant)
            continue;

        equ_solutions_reset(&partial);
        solve_rc = equ_solve_for_into(equation, entry->expr, &partial);
        if (solve_rc < 0) {
            equ_solutions_clear(&partial);
            rc = -1;
            goto cleanup;
        }
        if (partial.count > 0u && equ_merge_solutions(solutions, &partial) != 0)
            rc = -1;
        equ_solutions_clear(&partial);
        if (rc != 0)
            goto cleanup;
    }

cleanup:
    equ_restore_binding_values(bindings, original_values);
    equ_free_binding_value_snapshot(bindings, original_values);
    return rc;
}

static int equ_derive_without_bindings(const equation_t *equation,
                                            equation_solutions_t *solutions)
{
    const expr_t *lhs;

    if (!equation || !solutions)
        return -1;

    lhs = equ_lhs(equation);
    if (lhs && equ_is_solved_for(equation, lhs))
        return equ_append_existing_solution(equation, solutions);

    return 0;
}

static bool equ_expr_is_zero(const expr_t *expr);
static bool equ_expr_is_one(const expr_t *expr);
static expr_t *equ_symbolic_pi_expr(void);
static expr_t *equ_symbolic_two_pi_expr(void);
static expr_t *equ_exact_tan_sqrt_three_family_expr(void);

static int equ_try_solve_symbolic_affine(const expr_t *residual,
                                              const expr_t *wrt,
                                              equation_solutions_t *solutions);
static int equ_try_solve_unary_periodic(const equation_t *equation,
                                             const expr_t *wrt,
                                             equation_solutions_t *solutions);
static int equ_try_solve_unary_inverse(const equation_t *equation,
                                            const expr_t *wrt,
                                            equation_solutions_t *solutions);

static int equ_try_solve_affine(const equation_t *equation,
                                     const expr_t *wrt,
                                     equation_solutions_t *solutions)
{
    expr_t *residual = equ_residual(equation);
    number_t constant = num_new();
    number_t coeff = num_new();
    number_t neg_constant;
    number_t solution_value;
    bool ok;
    int rc = -1;

    if (!residual)
        goto cleanup;

    ok = equ_match_affine_linear_expr(residual, wrt, true, &constant, &coeff);
    if (!ok) {
        rc = equ_try_solve_symbolic_affine(residual, wrt, solutions);
        goto cleanup;
    }

    neg_constant = num_neg(constant);
    solution_value = num_div(neg_constant, coeff);
    if (equ_append_solution_value(wrt, solution_value, solutions) != 0) {
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

static number_t equ_quadratic_discriminant(number_t constant,
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

static number_t equ_quadratic_root(number_t neg_linear,
                                        number_t signed_sqrt_discriminant,
                                        number_t denominator)
{
    number_t numerator = num_add(neg_linear, signed_sqrt_discriminant);
    number_t root = num_div(numerator, denominator);

    num_destroy(&numerator);
    return root;
}

static expr_t *equ_symbolic_pi_expr(void)
{
    number_t pi_value = num_clone(NUM_PI);
    expr_t *pi = expr_new_named_const(pi_value, "@pi");

    num_destroy(&pi_value);
    return pi;
}

static expr_t *equ_symbolic_two_pi_expr(void)
{
    number_t two_value = num_create_from_long(2L);
    expr_t *two = expr_new_const(two_value);
    expr_t *pi = equ_symbolic_pi_expr();
    expr_t *out = (two && pi) ? expr_mul(two, pi) : NULL;

    expr_free(pi);
    expr_free(two);
    num_destroy(&two_value);
    return out;
}

static expr_t *equ_exact_sin_one_family_expr(void)
{
    number_t two_value = num_create_from_long(2L);
    number_t four_value = num_create_from_long(4L);
    number_t one_value = num_create_from_long(1L);
    expr_t *pi = equ_symbolic_pi_expr();
    expr_t *two = expr_new_const(two_value);
    expr_t *four = expr_new_const(four_value);
    expr_t *one = expr_new_const(one_value);
    expr_t *n = expr_new_named_var(NUM_NAN, "n");
    expr_t *pi_over_two = (pi && two) ? expr_div(pi, two) : NULL;
    expr_t *four_n = (four && n) ? expr_mul(four, n) : NULL;
    expr_t *affine = (four_n && one) ? expr_add(four_n, one) : NULL;
    expr_t *out = (pi_over_two && affine) ? expr_mul(pi_over_two, affine)
                                          : NULL;

    expr_free(affine);
    expr_free(four_n);
    expr_free(pi_over_two);
    expr_free(n);
    expr_free(one);
    expr_free(four);
    expr_free(two);
    expr_free(pi);
    num_destroy(&one_value);
    num_destroy(&four_value);
    num_destroy(&two_value);
    return out;
}

static expr_t *equ_exact_tan_sqrt_three_family_expr(void)
{
    number_t one_value = num_create_from_long(1L);
    number_t three_value = num_create_from_long(3L);
    number_t one_third_value = num_div(one_value, three_value);
    expr_t *one = expr_new_const(one_value);
    expr_t *three = expr_new_const(three_value);
    expr_t *pi = equ_symbolic_pi_expr();
    expr_t *n = expr_new_named_var(NUM_NAN, "n");
    expr_t *one_third = expr_new_const(one_third_value);
    expr_t *three_n = (three && n) ? expr_mul(three, n) : NULL;
    expr_t *affine = (three_n && one) ? expr_add(three_n, one) : NULL;
    expr_t *pi_affine = (pi && affine) ? expr_mul(pi, affine) : NULL;
    expr_t *out = (one_third && pi_affine) ? expr_mul(one_third, pi_affine)
                                           : NULL;

    if (out)
        expr_set_binding_pi_linear_family(out, 3L, 3L, 1L);

    expr_free(pi_affine);
    expr_free(affine);
    expr_free(three_n);
    expr_free(one_third);
    expr_free(n);
    expr_free(pi);
    expr_free(three);
    expr_free(one);
    num_destroy(&one_third_value);
    num_destroy(&three_value);
    num_destroy(&one_value);
    return out;
}

static expr_t *equ_symbolic_linear_root(const expr_t *constant,
                                             const expr_t *linear)
{
    expr_t *neg_constant = expr_neg(constant);
    expr_t *quotient = neg_constant ? expr_div(neg_constant, linear) : NULL;
    expr_t *root = expr_simplify_owned(quotient);

    expr_free(neg_constant);
    return root;
}

static expr_t *equ_symbolic_linear_phase_root(const expr_t *constant,
                                                   const expr_t *linear,
                                                   const expr_t *phase)
{
    if (equ_expr_is_zero(constant) && equ_expr_is_one(linear)) {
        expr_t *copy = (expr_t *)phase;

        expr_retain(copy);
        return expr_simplify_owned(copy);
    }

    expr_t *shifted_constant = (constant && phase)
        ? expr_sub(constant, phase)
        : NULL;
    expr_t *root = shifted_constant
        ? equ_symbolic_linear_root(shifted_constant, linear)
        : NULL;

    expr_free(shifted_constant);
    return root;
}

static expr_t *equ_periodic_family_expr(const expr_t *base,
                                             const expr_t *period,
                                             const expr_t *n)
{
    expr_t *period_term = (period && n) ? expr_mul(period, n) : NULL;
    expr_t *sum = (base && period_term) ? expr_add(base, period_term) : NULL;
    expr_t *out = expr_simplify_owned(sum);

    expr_free(period_term);
    return out;
}

static expr_t *equ_periodic_sub_family_expr(const expr_t *offset,
                                                 const expr_t *period,
                                                 const expr_t *n,
                                                 const expr_t *subtrahend)
{
    expr_t *period_term = (period && n) ? expr_mul(period, n) : NULL;
    expr_t *offset_sum = (offset && period_term) ? expr_add(offset, period_term)
                                                 : NULL;
    expr_t *difference = (offset_sum && subtrahend)
        ? expr_sub(offset_sum, subtrahend)
        : NULL;
    expr_t *out = expr_simplify_owned(difference);

    expr_free(offset_sum);
    expr_free(period_term);
    return out;
}

static int equ_append_trig_family_root(const expr_t *wrt,
                                            const expr_t *constant,
                                            const expr_t *linear,
                                            const expr_t *base,
                                            const expr_t *period,
                                            const expr_t *n,
                                            equation_solutions_t *solutions)
{
    expr_t *family = equ_periodic_family_expr(base, period, n);
    expr_t *root = family
        ? equ_symbolic_linear_phase_root(constant, linear, family)
        : NULL;
    int rc = -1;

    if (!root)
        goto cleanup;
    if (equ_append_solution_expr(wrt, root, solutions) != 0)
        goto cleanup;

    rc = 0;

cleanup:
    expr_free(root);
    expr_free(family);
    return rc;
}

typedef enum {
    EQU_PERIODIC_TRIG_SIN,
    EQU_PERIODIC_TRIG_COS,
    EQU_PERIODIC_TRIG_TAN
} equ_periodic_trig_kind_t;

static int equ_try_solve_periodic_trig_kind(equ_periodic_trig_kind_t kind,
                                                 const expr_t *inner,
                                                 const expr_t *target,
                                                 const expr_t *wrt,
                                                 equation_solutions_t *solutions)
{
    expr_t *constant = NULL;
    expr_t *linear = NULL;
    expr_t *n = NULL;
    expr_t *base = NULL;
    expr_t *alt_base = NULL;
    expr_t *period = NULL;
    expr_t *pi = NULL;
    expr_t *neg_base = NULL;
    expr_t *exact_family = NULL;
    expr_t *exact_root = NULL;
    int rc = -1;

    if (!inner || !target || !wrt || !solutions)
        return -1;
    if (!equ_match_symbolic_linear_expr(inner, wrt, &constant, &linear)) {
        rc = 1;
        goto cleanup;
    }

    n = expr_new_named_var(NUM_NAN, "n");
    if (!n)
        goto cleanup;

    switch (kind) {
        case EQU_PERIODIC_TRIG_SIN:
            if (equ_expr_is_one(target)) {
                exact_family = equ_exact_sin_one_family_expr();
                if (!exact_family)
                    goto cleanup;
                if (equ_expr_is_zero(constant) && equ_expr_is_one(linear)) {
                    rc = equ_append_solution_expr(wrt, exact_family, solutions);
                    break;
                }
                exact_root = equ_symbolic_linear_phase_root(constant, linear,
                                                            exact_family);
                if (!exact_root)
                    goto cleanup;
                rc = equ_append_solution_expr(wrt, exact_root, solutions);
                break;
            }
            base = expr_simplify_owned(expr_asin(target));
            period = equ_symbolic_two_pi_expr();
            if (!base || !period)
                goto cleanup;
            rc = equ_append_trig_family_root(wrt, constant, linear, base, period,
                                            n, solutions);
            if (rc != 0)
                goto cleanup;
            pi = equ_symbolic_pi_expr();
            alt_base = equ_periodic_sub_family_expr(pi, period, n, base);
            if (!alt_base)
                goto cleanup;
            exact_root = equ_symbolic_linear_phase_root(constant, linear, alt_base);
            if (!exact_root) {
                rc = -1;
                goto cleanup;
            }
            if (equ_append_solution_expr(wrt, exact_root, solutions) != 0) {
                equ_solutions_clear(solutions);
                rc = -1;
                goto cleanup;
            }
            rc = 0;
            break;

        case EQU_PERIODIC_TRIG_COS:
            base = expr_simplify_owned(expr_acos(target));
            period = equ_symbolic_two_pi_expr();
            if (!base || !period)
                goto cleanup;
            rc = equ_append_trig_family_root(wrt, constant, linear, base, period,
                                            n, solutions);
            if (rc != 0)
                goto cleanup;
            neg_base = base ? expr_neg(base) : NULL;
            alt_base = expr_simplify_owned(neg_base);
            neg_base = NULL;
            if (!alt_base)
                goto cleanup;
            if (equ_append_trig_family_root(wrt, constant, linear, alt_base, period,
                                            n, solutions) != 0) {
                equ_solutions_clear(solutions);
                rc = -1;
                goto cleanup;
            }
            rc = 0;
            break;

        case EQU_PERIODIC_TRIG_TAN:
        {
            number_t target_value = num_new();
            bool is_sqrt_three = expr_match_const_value(target, &target_value) &&
                                num_eq(target_value, NUM_SQRT3);

            num_destroy(&target_value);
            if (is_sqrt_three) {
                exact_family = equ_exact_tan_sqrt_three_family_expr();
                if (!exact_family)
                    goto cleanup;
                if (equ_expr_is_zero(constant) && equ_expr_is_one(linear)) {
                    rc = equ_append_solution_expr(wrt, exact_family, solutions);
                    break;
                }
                exact_root = equ_symbolic_linear_phase_root(constant, linear,
                                                            exact_family);
                if (!exact_root)
                    goto cleanup;
                rc = equ_append_solution_expr(wrt, exact_root, solutions);
                break;
            }
            base = expr_simplify_owned(expr_atan(target));
            period = equ_symbolic_pi_expr();
            if (!base || !period)
                goto cleanup;
            rc = equ_append_trig_family_root(wrt, constant, linear, base, period,
                                            n, solutions);
            break;
        }

        default:
            rc = 1;
            break;
    }

cleanup:
    expr_free(exact_root);
    expr_free(exact_family);
    expr_free(neg_base);
    expr_free(pi);
    expr_free(period);
    expr_free(alt_base);
    expr_free(base);
    expr_free(n);
    expr_free(linear);
    expr_free(constant);
    return rc;
}

static int equ_try_solve_unary_periodic_side(const expr_t *lhs,
                                                  const expr_t *rhs,
                                                  const expr_t *wrt,
                                                  equation_solutions_t *solutions)
{
    const expr_t *inner = NULL;

    if (!lhs || !rhs || !wrt || !solutions)
        return -1;
    if (!equ_expr_uses_wrt(lhs, wrt) || equ_expr_uses_wrt(rhs, wrt))
        return 1;

    if (expr_match_sin_expr(lhs, &inner))
        return equ_try_solve_periodic_trig_kind(EQU_PERIODIC_TRIG_SIN, inner, rhs,
                                                wrt, solutions);
    if (expr_match_cos_expr(lhs, &inner))
        return equ_try_solve_periodic_trig_kind(EQU_PERIODIC_TRIG_COS, inner, rhs,
                                                wrt, solutions);
    if (expr_match_tan_expr(lhs, &inner))
        return equ_try_solve_periodic_trig_kind(EQU_PERIODIC_TRIG_TAN, inner, rhs,
                                                wrt, solutions);

    return 1;
}

static int equ_try_solve_unary_periodic(const equation_t *equation,
                                             const expr_t *wrt,
                                             equation_solutions_t *solutions)
{
    int rc;

    if (!equation || !wrt || !solutions)
        return -1;

    rc = equ_try_solve_unary_periodic_side(equation->lhs, equation->rhs,
                                           wrt, solutions);
    if (rc != 1)
        return rc;
    return equ_try_solve_unary_periodic_side(equation->rhs, equation->lhs,
                                             wrt, solutions);
}

typedef enum {
    EQU_UNARY_INVERSE_EXP,
    EQU_UNARY_INVERSE_LOG
} equ_unary_inverse_kind_t;

static expr_t *equ_unary_inverse_rhs(equ_unary_inverse_kind_t kind, const expr_t *rhs)
{
    switch (kind) {
        case EQU_UNARY_INVERSE_EXP:
            return rhs ? expr_simplify_owned(expr_log(rhs)) : NULL;
        case EQU_UNARY_INVERSE_LOG:
            return rhs ? expr_simplify_owned(expr_exp(rhs)) : NULL;
        default:
            return NULL;
    }
}

static int equ_try_solve_unary_inverse_side(const expr_t *lhs,
                                            const expr_t *rhs,
                                            const expr_t *wrt,
                                            equation_solutions_t *solutions)
{
    const expr_t *inner = NULL;
    expr_t *inverse_rhs = NULL;
    equation_t *reduced = NULL;
    int rc = 1;

    if (!lhs || !rhs || !wrt || !solutions)
        return -1;
    if (!equ_expr_uses_wrt(lhs, wrt) || equ_expr_uses_wrt(rhs, wrt))
        return 1;

    if (expr_match_exp_expr(lhs, &inner))
        inverse_rhs = equ_unary_inverse_rhs(EQU_UNARY_INVERSE_EXP, rhs);
    else if (expr_match_log_expr(lhs, &inner))
        inverse_rhs = equ_unary_inverse_rhs(EQU_UNARY_INVERSE_LOG, rhs);
    else
        return 1;

    if (!inverse_rhs)
        return -1;

    reduced = equ_new(inner, inverse_rhs);
    if (!reduced) {
        rc = -1;
        goto cleanup;
    }

    rc = equ_solve_for_into(reduced, wrt, solutions);

cleanup:
    equ_free(reduced);
    expr_free(inverse_rhs);
    return rc;
}

static int equ_try_solve_unary_inverse(const equation_t *equation,
                                            const expr_t *wrt,
                                            equation_solutions_t *solutions)
{
    int rc;

    if (!equation || !wrt || !solutions)
        return -1;

    rc = equ_try_solve_unary_inverse_side(equation->lhs, equation->rhs,
                                          wrt, solutions);
    if (rc != 1)
        return rc;
    return equ_try_solve_unary_inverse_side(equation->rhs, equation->lhs,
                                            wrt, solutions);
}

static int equ_try_solve_symbolic_affine(const expr_t *residual,
                                              const expr_t *wrt,
                                              equation_solutions_t *solutions)
{
    expr_t *constant = NULL;
    expr_t *linear = NULL;
    expr_t *root = NULL;
    int rc = -1;

    if (!equ_match_symbolic_linear_expr(residual, wrt, &constant,
                                             &linear)) {
        rc = 1;
        goto cleanup;
    }

    root = equ_symbolic_linear_root(constant, linear);
    if (!root)
        goto cleanup;

    if (equ_append_solution_expr(wrt, root, solutions) != 0)
        goto cleanup;

    rc = 0;

cleanup:
    expr_free(root);
    expr_free(linear);
    expr_free(constant);
    return rc;
}

static expr_t *equ_symbolic_quadratic_discriminant(const expr_t *constant,
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
    expr_t *out = expr_simplify_owned(discriminant);

    expr_free(four_quad_constant);
    expr_free(quad_constant);
    expr_free(linear_sq);
    num_destroy(&four);
    return out;
}

static expr_t *equ_symbolic_quadratic_root(const expr_t *linear,
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
    expr_t *root = expr_simplify_owned(quotient);

    expr_free(denominator);
    expr_free(numerator);
    expr_free(neg_linear);
    num_destroy(&two);
    return root;
}

static bool equ_expr_simplifies_equal(const expr_t *left,
                                           const expr_t *right)
{
    expr_t *diff = (left && right) ? expr_sub(left, right) : NULL;
    expr_t *simplified = diff ? expr_simplify(diff) : NULL;
    bool equal = simplified && expr_is_exact_zero(simplified);

    expr_free(simplified);
    expr_free(diff);
    return equal;
}

static bool equ_expr_text_equal(const expr_t *left, const expr_t *right)
{
    string_t *left_text = left ? expr_to_text(left, style_EXPRESSION) : NULL;
    string_t *right_text = right ? expr_to_text(right, style_EXPRESSION) : NULL;
    bool equal = left_text && right_text &&
                 strcmp(string_c_str(left_text),
                        string_c_str(right_text)) == 0;

    string_free(right_text);
    string_free(left_text);
    return equal;
}

static bool equ_expr_matches_expected(const expr_t *expr,
                                           const expr_t *expected)
{
    return equ_expr_text_equal(expr, expected) ||
           equ_expr_simplifies_equal(expr, expected);
}

static bool equ_expr_is_zero(const expr_t *expr)
{
    number_t value = num_new();
    bool ok = expr_match_const_value(expr, &value) && num_eq(value, NUM_ZERO);

    num_destroy(&value);
    return ok;
}

static bool equ_expr_is_one(const expr_t *expr)
{
    number_t value = num_new();
    bool ok = expr_match_const_value(expr, &value) && num_eq(value, NUM_ONE);

    num_destroy(&value);
    return ok;
}

static expr_t *equ_symbolic_negated_sum(const expr_t *left,
                                             const expr_t *right)
{
    expr_t *neg_left = left ? expr_neg(left) : NULL;
    expr_t *neg_right = right ? expr_neg(right) : NULL;
    expr_t *sum = (neg_left && neg_right) ? expr_add(neg_left, neg_right) : NULL;
    expr_t *out = expr_simplify_owned(sum);

    expr_free(neg_right);
    expr_free(neg_left);
    return out;
}

static int equ_try_solve_symbolic_quadratic_product_roots(
    const expr_t *constant,
    const expr_t *linear,
    const expr_t *quadratic,
    const expr_t *wrt,
    equation_solutions_t *solutions)
{
    const expr_t *first = NULL;
    const expr_t *second = NULL;
    expr_t *expected_linear = NULL;
    bool reversed = false;
    int rc = 1;

    if (!equ_expr_is_one(quadratic))
        return 1;
    if (!expr_match_mul_expr(constant, &first, &second))
        return 1;
    if (equ_expr_uses_wrt(first, wrt) ||
        equ_expr_uses_wrt(second, wrt))
        return 1;

    expected_linear = equ_symbolic_negated_sum(first, second);
    if (!expected_linear)
        return -1;

    if (!equ_expr_matches_expected(linear, expected_linear)) {
        expr_free(expected_linear);
        expected_linear = equ_symbolic_negated_sum(second, first);
        if (!expected_linear)
            return -1;
        if (!equ_expr_matches_expected(linear, expected_linear))
            goto cleanup;
        reversed = true;
    }

    if (equ_append_solution_expr(wrt, reversed ? second : first, solutions) != 0)
        goto error;
    if (equ_append_solution_expr(wrt, reversed ? first : second, solutions) != 0)
        goto error_clear;

    rc = 0;
    goto cleanup;

error_clear:
    equ_solutions_clear(solutions);
error:
    rc = -1;

cleanup:
    expr_free(expected_linear);
    return rc;
}

static int equ_try_solve_symbolic_quadratic(const expr_t *residual,
                                                 const expr_t *wrt,
                                                 equation_solutions_t *solutions)
{
    expr_t *constant = NULL;
    expr_t *linear = NULL;
    expr_t *quadratic = NULL;
    expr_t *discriminant = NULL;
    expr_t *sqrt_discriminant = NULL;
    expr_t *root_plus = NULL;
    expr_t *root_minus = NULL;
    int rc = -1;

    if (!equ_match_symbolic_quadratic_expr(residual, wrt, &constant,
                                                &linear, &quadratic)) {
        rc = 1;
        goto cleanup;
    }

    rc = equ_try_solve_symbolic_quadratic_product_roots(
        constant, linear, quadratic, wrt, solutions);
    if (rc <= 0)
        goto cleanup;

    discriminant = equ_symbolic_quadratic_discriminant(constant, linear,
                                                            quadratic);
    sqrt_discriminant = discriminant ? expr_sqrt(discriminant) : NULL;
    sqrt_discriminant = expr_simplify_owned(sqrt_discriminant);
    root_plus = sqrt_discriminant
        ? equ_symbolic_quadratic_root(linear, quadratic,
                                           sqrt_discriminant, true)
        : NULL;
    root_minus = sqrt_discriminant
        ? equ_symbolic_quadratic_root(linear, quadratic,
                                           sqrt_discriminant, false)
        : NULL;

    if (!root_plus || !root_minus)
        goto cleanup;

    if (equ_append_solution_expr(wrt, root_plus, solutions) != 0)
        goto cleanup;
    if (equ_append_solution_expr(wrt, root_minus, solutions) != 0) {
        equ_solutions_clear(solutions);
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

static int equ_try_solve_quadratic(const equation_t *equation,
                                        const expr_t *wrt,
                                        equation_solutions_t *solutions)
{
    expr_t *residual = equ_residual(equation);
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

    ok = equ_match_quadratic_expr(residual, wrt, &constant, &linear,
                                       &quadratic);
    if (!ok) {
        rc = equ_try_solve_symbolic_quadratic(residual, wrt, solutions);
        goto cleanup;
    }

    discriminant = equ_quadratic_discriminant(constant, linear, quadratic);
    sqrt_discriminant = num_sqrt(discriminant);
    neg_linear = num_neg(linear);
    denominator = num_mul_long(quadratic, 2L);

    root = equ_quadratic_root(neg_linear, sqrt_discriminant, denominator);
    if (equ_append_solution_value(wrt, root, solutions) != 0) {
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
        root = equ_quadratic_root(neg_linear, neg_sqrt_discriminant,
                                       denominator);
        num_destroy(&neg_sqrt_discriminant);
        if (equ_append_solution_value(wrt, root, solutions) != 0) {
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

static int equ_try_solve_zero_product_factor(const expr_t *factor,
                                                  const expr_t *wrt,
                                                  const expr_t *zero,
                                                  equation_solutions_t *solutions,
                                                  bool *saw_solved_factor)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    equation_t *factor_equation = NULL;
    equation_solutions_t factor_result;
    int rc = -1;

    equ_solutions_reset(&factor_result);

    if (expr_match_mul_expr(factor, &left, &right)) {
        rc = equ_try_solve_zero_product_factor(left, wrt, zero, solutions,
                                                    saw_solved_factor);
        if (rc != 0)
            goto cleanup;
        rc = equ_try_solve_zero_product_factor(right, wrt, zero, solutions,
                                                    saw_solved_factor);
        goto cleanup;
    }

    if (!equ_expr_uses_wrt(factor, wrt)) {
        rc = expr_is_exact_zero(factor) ? 1 : 0;
        goto cleanup;
    }

    factor_equation = equ_new(factor, zero);
    if (!factor_equation)
        goto cleanup;

    if (equ_solve_for_into(factor_equation, wrt, &factor_result) != 0)
        goto cleanup;
    if (factor_result.status != EQUATION_SOLVE_SOLVED ||
        factor_result.count == 0u) {
        rc = 1;
        goto cleanup;
    }

    for (size_t i = 0u; i < factor_result.count; ++i) {
        if (equ_append_solution_expr(
                wrt, equ_rhs(factor_result.solutions[i]), solutions) != 0)
            goto cleanup;
    }

    *saw_solved_factor = true;
    rc = 0;

cleanup:
    equ_solutions_clear(&factor_result);
    equ_free(factor_equation);
    return rc;
}

static int equ_try_solve_zero_product(const equation_t *equation,
                                           const expr_t *wrt,
                                           equation_solutions_t *solutions)
{
    const expr_t *product = NULL;
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *zero = NULL;
    bool saw_solved_factor = false;
    int rc;

    if (!equation || !wrt || !solutions)
        return -1;

    if (expr_is_exact_zero(equation->rhs))
        product = equation->lhs;
    else if (expr_is_exact_zero(equation->lhs))
        product = equation->rhs;
    else
        return 1;

    if (!expr_match_mul_expr(product, &left, &right))
        return 1;

    zero = expr_new_const(NUM_ZERO);
    if (!zero)
        return -1;

    rc = equ_try_solve_zero_product_factor(product, wrt, zero, solutions,
                                                &saw_solved_factor);
    expr_free(zero);

    if (rc != 0 || !saw_solved_factor) {
        equ_solutions_clear(solutions);
        return rc < 0 ? -1 : 1;
    }

    return 0;
}

static int equ_append_numeric_binding_solutions(expr_bindings_t *bindings,
                                                     equation_solutions_t *solutions)
{
    if (!bindings || !solutions)
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

        solution = equ_new(entry->expr, rhs);
        expr_free(rhs);
        if (!solution)
            return -1;

        if (equ_solutions_append(solutions, solution) != 0) {
            equ_free(solution);
            return -1;
        }
    }

    return solutions->count > 0u ? 0 : 1;
}

static size_t equ_numeric_precision_digits(const expr_goal_seek_options_t *options)
{
    if (options && options->precision_digits > 0u)
        return options->precision_digits;
    return num_get_default_prec_digits();
}

static number_t equ_numeric_tolerance(const expr_goal_seek_options_t *options)
{
    if (options && num_is_finite(options->tolerance) &&
        !num_is_zero(options->tolerance))
        return num_abs(options->tolerance);

    return num_pow10(-(int)equ_numeric_precision_digits(options));
}

static bool equ_numeric_residual_is_solved(const expr_t *residual,
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
    tolerance = equ_numeric_tolerance(options);
    ok = num_is_finite(mag) && num_le(mag, tolerance);

    num_destroy(&tolerance);
    num_destroy(&mag);
    num_destroy(&value);
    return ok;
}

static number_t *equ_snapshot_binding_values(expr_bindings_t *bindings)
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

static void equ_restore_binding_values(expr_bindings_t *bindings,
                                            number_t *values)
{
    if (!bindings || !values)
        return;

    for (size_t i = 0u; i < bindings->count; ++i)
        expr_set_val(bindings->entries[i].expr, values[i]);
}

static void equ_free_binding_value_snapshot(expr_bindings_t *bindings,
                                                 number_t *values)
{
    if (!bindings || !values)
        return;

    for (size_t i = 0u; i < bindings->count; ++i)
        num_destroy(&values[i]);
    free(values);
}

int equ_solve_numeric_into(const equation_t *equation,
                                expr_bindings_t *bindings,
                                const expr_goal_seek_options_t *options,
                                equation_solutions_t *solutions)
{
    expr_t *residual = NULL;
    number_t target = num_create_from_long(0L);
    number_t *original_values = NULL;
    expr_goal_seek_result_t goal_result;
    int rc = -1;

    if (!solutions)
        return -1;

    equ_solutions_reset(solutions);
    if (!equation || !bindings)
        goto cleanup_no_goal;

    solutions->status = EQUATION_SOLVE_UNSOLVED;
    residual = equ_residual(equation);
    if (!residual)
        goto cleanup_no_goal;
    original_values = equ_snapshot_binding_values(bindings);
    if (bindings->count > 0u && !original_values)
        goto cleanup_no_goal;

    if (expr_goal_seek(residual, bindings, target, options, &goal_result) != 0) {
        equ_restore_binding_values(bindings, original_values);
        rc = 0;
        goto cleanup_no_goal;
    }

    if (!equ_numeric_residual_is_solved(residual, options)) {
        equ_restore_binding_values(bindings, original_values);
        rc = 0;
        expr_goal_seek_result_clear(&goal_result);
        goto cleanup_no_goal;
    }

    rc = equ_append_numeric_binding_solutions(bindings, solutions);
    if (rc < 0)
        equ_solutions_clear(solutions);
    else if (rc > 0)
        solutions->status = EQUATION_SOLVE_UNSOLVED;
    expr_goal_seek_result_clear(&goal_result);

cleanup_no_goal:
    equ_free_binding_value_snapshot(bindings, original_values);
    num_destroy(&target);
    expr_free(residual);
    return rc < 0 ? -1 : 0;
}

int equ_solve_for_into(const equation_t *equation,
                            const expr_t *wrt,
                            equation_solutions_t *solutions)
{
    int unary_inverse_rc;
    int unary_periodic_rc;
    int affine_rc;
    int zero_product_rc;
    int quadratic_rc;
    int cubic_rc;

    if (!solutions)
        return -1;

    equ_solutions_reset(solutions);

    if (!equation || !wrt)
        return -1;

    solutions->status = EQUATION_SOLVE_UNSOLVED;

    if (equ_is_solved_for(equation, wrt))
        return equ_append_existing_solution(equation, solutions);

    unary_inverse_rc = equ_try_solve_unary_inverse(equation, wrt, solutions);
    if (unary_inverse_rc == 0)
        return 0;
    if (unary_inverse_rc < 0)
        return -1;

    unary_periodic_rc = equ_try_solve_unary_periodic(equation, wrt, solutions);
    if (unary_periodic_rc == 0)
        return 0;
    if (unary_periodic_rc < 0)
        return -1;

    affine_rc = equ_try_solve_affine(equation, wrt, solutions);
    if (affine_rc == 0)
        return 0;
    if (affine_rc < 0)
        return -1;

    zero_product_rc = equ_try_solve_zero_product(equation, wrt, solutions);
    if (zero_product_rc == 0)
        return 0;
    if (zero_product_rc < 0)
        return -1;

    quadratic_rc = equ_try_solve_quadratic(equation, wrt, solutions);
    if (quadratic_rc == 0)
        return 0;
    if (quadratic_rc < 0)
        return -1;

    cubic_rc = equ_try_solve_cubic(equation, wrt, solutions);
    if (cubic_rc == 0)
        return 0;
    if (cubic_rc < 0)
        return -1;

    return 0;
}

equation_solutions_t *equ_derive_solutions(const equation_t *equation)
{
    equation_solutions_t *solutions = equ_solutions_new();
    expr_bindings_t *bindings = equ_bindings(equation);
    expr_goal_seek_options_t options;
    bool used_numeric = false;

    if (!solutions)
        return NULL;

    if (!equation)
        return solutions;

    if (!bindings) {
        if (equ_derive_without_bindings(equation, solutions) != 0)
            goto error;
        return solutions;
    }

    if (equ_derive_symbolic_solutions(equation, bindings, solutions) != 0)
        goto error;

    if (equ_solutions_count(solutions) == 0u &&
        equ_variable_binding_count(bindings) > 0u) {
        if (equ_default_goal_seek_options(&options) != 0)
            goto error;
        used_numeric = true;
        if (equ_solve_numeric_into(equation, bindings, &options, solutions) != 0) {
            num_destroy(&options.tolerance);
            goto error;
        }
        num_destroy(&options.tolerance);
    }

    return solutions;

error:
    if (used_numeric)
        num_destroy(&options.tolerance);
    if (solutions) {
        equ_solutions_free(solutions);
    }
    return NULL;
}

size_t equ_solutions_count(const equation_solutions_t *solutions)
{
    return solutions ? solutions->count : 0u;
}

const equation_t *equ_solutions_at(
    const equation_solutions_t *solutions,
    size_t index)
{
    if (!solutions || index >= solutions->count)
        return NULL;
    return solutions->solutions[index];
}

void equ_solutions_clear(equation_solutions_t *solutions)
{
    if (!solutions)
        return;

    for (size_t i = 0u; i < solutions->count; ++i)
        equ_free(solutions->solutions[i]);
    free(solutions->solutions);
    equ_solutions_reset(solutions);
}
