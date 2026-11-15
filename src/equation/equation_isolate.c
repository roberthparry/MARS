/**
 * @file equation_isolate.c
 * @brief Guarded isolation through affine arithmetic and principal unary functions.
 *
 * Walks one dependency chain without changing the source equation or its bindings. Independent coefficients
 * are moved algebraically, while principal square-root, logarithm and inverse-hyperbolic ranges remain native
 * expression domains on the resulting formula. Remaining dependency chains delegate to the ordinary solver
 * pipeline, retaining all polynomial and constant-base exponential branches it supplies.
 */
#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"

static bool isolate_uses(const expr_t *expression, const expr_t *variable)
{
    expr_t *variables[] = {(expr_t *)variable};
    bool used = false;
    return expression && expr_collect_var_usage(expression, 1, variables, &used) && used;
}

/* Domain wrappers retain live symbols, so later coordinate and parameter values also update their restrictions. */
static bool isolate_guard(expr_t **domain, const expr_t *predicate)
{
    expr_t *zero = expr_const_long(0);
    expr_t *args[] = {*domain, (expr_t *)predicate, zero};
    expr_t *next = predicate && zero ? expr_real_domain_from_args(3, args) : NULL;
    expr_free(zero);
    if (!next)
        return false;
    expr_free(*domain);
    *domain = next;
    return true;
}

static bool isolate_nonzero(expr_t **domain, const expr_t *value)
{
    expr_t *magnitude = expr_simplify_owned(expr_abs(value));
    bool ok = magnitude && isolate_guard(domain, magnitude);
    expr_free(magnitude);
    return ok;
}

/* Certify an authored constant half-period from its exact preserved algebra, rather than rounded pi values. */
static int isolate_constant_half_period(const expr_t *value, const expr_t *width)
{
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(value);
    bool constant = expr_bindings_count(bindings) == 0u;
    expr_bindings_free(bindings);
    if (!constant)
        return 0;
    expr_t *expanded = expr_expand_preserved_for_display(value);
    expr_t *unit = expr_new_named_const(NUM_I, "i");
    expr_t *pole = unit ? expr_mul(unit, width) : NULL;
    int result = 0;
    for (unsigned sign = 0; expanded && pole && !result && sign < 2u; ++sign) {
        expr_t *difference = expr_simplify_owned(sign ? expr_add(expanded, pole) : expr_sub(expanded, pole));
        number_t remainder = difference ? expr_eval(difference) : num_clone(NUM_NAN);
        if (num_is_exact(remainder) && num_is_finite(remainder) && num_is_real(remainder))
            result = sign ? -1 : 1;
        num_destroy(&remainder);
        expr_free(difference);
    }
    expr_free(pole);
    expr_free(unit);
    expr_free(expanded);
    return result;
}

/* Principal Log uses -pi < Im(z) <= pi; atanh uses the corresponding half-width strip. */
static bool isolate_strip(expr_t **domain, const expr_t *value, bool half)
{
    expr_t *imaginary = expr_apply_unary_function("Im", value, NULL);
    expr_t *pi = expr_new_named_const(NUM_PI, "@pi");
    expr_t *width = pi ? (half ? expr_div_long(pi, 2) : expr_retain_expr(pi)) : NULL;
    expr_t *lower = width ? expr_neg(width) : NULL;
    expr_t *above = imaginary && lower ? expr_real_comparison(imaginary, lower, false) : NULL;
    expr_t *below = imaginary && width ? expr_real_comparison(width, imaginary, true) : NULL;
    int boundary = half && width ? isolate_constant_half_period(value, width) : 0;
    expr_t *zero = boundary < 0 ? expr_const_long(0) : NULL;
    bool ok = boundary > 0 || (boundary < 0 ? zero && isolate_guard(domain, zero) :
                              above && below && isolate_guard(domain, above) && isolate_guard(domain, below));
    if (ok && half) {
        expr_t *unit = expr_new_named_const(NUM_I, "i");
        expr_t *pole = unit ? expr_mul(unit, width) : NULL;
        expr_t *difference = pole ? expr_sub(value, pole) : NULL;
        ok = difference && isolate_nonzero(domain, difference);
        expr_free(difference);
        expr_free(pole);
        expr_free(unit);
    }
    expr_free(below);
    expr_free(above);
    expr_free(lower);
    expr_free(width);
    expr_free(pi);
    expr_free(imaginary);
    expr_free(zero);
    return ok;
}

/* These real-coordinate inequalities describe the right half-plane and its non-negative imaginary boundary. */
static bool isolate_root_range(expr_t **domain, const expr_t *target)
{
    expr_t *real = expr_apply_unary_function("Re", target, NULL);
    expr_t *imaginary = expr_apply_unary_function("Im", target, NULL);
    expr_t *zero = expr_const_long(0);
    expr_t *real_sign = real ? expr_sgn(real) : NULL, *imaginary_sign = imaginary ? expr_sgn(imaginary) : NULL;
    expr_t *signs = real_sign && imaginary_sign ? expr_add(real_sign, imaginary_sign) : NULL;
    expr_t *right = real && zero ? expr_real_comparison(real, zero, true) : NULL;
    expr_t *upper = signs && zero ? expr_real_comparison(signs, zero, true) : NULL;
    bool ok = right && upper && isolate_guard(domain, right) && isolate_guard(domain, upper);
    expr_free(upper);
    expr_free(right);
    expr_free(signs);
    expr_free(imaginary_sign);
    expr_free(real_sign);
    expr_free(zero);
    expr_free(imaginary);
    expr_free(real);
    return ok;
}

/* Cancel an authored half-period exactly, so boundary values do not acquire a rounding-induced imaginary part. */
static expr_t *isolate_tanh(const expr_t *target)
{
    expr_t *preserved = expr_expand_preserved_for_display(target);
    expr_t *expanded = preserved ? expr_clone_linked_symbols(preserved, target) : NULL;
    expr_free(preserved);
    const expr_t *argument = expanded ? expanded : target;
    const expr_t *left = NULL, *right = NULL;
    bool subtract = false;
    if (!expr_match_add_sub_expr(argument, &left, &right, &subtract)) {
        expr_free(expanded);
        return expr_tanh(target);
    }
    expr_t *unit = expr_new_named_const(NUM_I, "i"), *pi = expr_new_named_const(NUM_PI, "@pi");
    expr_t *product = unit && pi ? expr_mul(unit, pi) : NULL;
    expr_t *pole = product ? expr_div_long(product, 2) : NULL;
    expr_t *opposite = subtract ? expr_neg(right) : NULL, *result = NULL;
    /* Both signs of the half-period give coth; a cancelled summand certifies the structural identity. */
    for (unsigned sign = 0; pole && !result && sign < 2u; ++sign) {
        expr_t *shifted = expr_simplify_owned(sign ? expr_add(argument, pole) : expr_sub(argument, pole));
        bool cancelled = shifted && (expr_struct_eq(shifted, left) || expr_struct_eq(shifted, right) ||
                                      (opposite && expr_struct_eq(shifted, opposite)));
        if (cancelled)
            result = expr_coth(shifted);
        expr_free(shifted);
    }
    expr_free(opposite);
    expr_free(pole);
    expr_free(product);
    expr_free(pi);
    expr_free(unit);
    expr_free(expanded);
    return result ? result : expr_tanh(target);
}

/* Each step removes one operation. No name, coefficient or authored equation is recognised specially. */
static expr_t *isolate_step(const expr_t **dependent, const expr_t *target, const expr_t *variable,
                             expr_t **domain, bool *nonlinear, bool *supported)
{
    const expr_t *left = NULL, *right = NULL;
    bool subtract = false;
    if (expr_match_neg_expr(*dependent, &left)) {
        *supported = true;
        *dependent = left;
        return expr_neg(target);
    }
    if (expr_match_add_sub_expr(*dependent, &left, &right, &subtract)) {
        bool a = isolate_uses(left, variable), b = isolate_uses(right, variable);
        if (a == b)
            return NULL;
        *supported = true;
        *dependent = a ? left : right;
        return a ? (subtract ? expr_add(target, right) : expr_sub(target, right)) :
                   (subtract ? expr_sub(left, target) : expr_sub(target, left));
    }
    bool division = expr_match_div_expr(*dependent, &left, &right);
    if (division || expr_match_mul_expr(*dependent, &left, &right)) {
        bool a = isolate_uses(left, variable), b = isolate_uses(right, variable);
        if (a == b)
            return NULL;
        const expr_t *coefficient = a ? right : left;
        number_t value = expr_eval(coefficient);
        bool zero = num_is_zero(value);
        num_destroy(&value);
        if (zero)
            return NULL;
        *supported = true;
        *dependent = a ? left : right;
        if (!isolate_nonzero(domain, division && !a ? target : coefficient))
            return NULL;
        return division ? (a ? expr_mul(target, coefficient) : expr_div(coefficient, target)) :
                          expr_div(target, coefficient);
    }
    if (expr_match_sqrt_expr(*dependent, &left)) {
        *supported = true;
        if (!isolate_root_range(domain, target))
            return NULL;
        *dependent = left;
        *nonlinear = true;
        return expr_pow_long(target, 2);
    }
    typedef struct {
        expr_pattern_unary_affine_kind_t kind;
        expr_t *(*inverse)(const expr_t *);
    } inverse_rule_t;
    static const inverse_rule_t rules[] = {
        {EXPR_PATTERN_UNARY_ATANH, isolate_tanh},
        {EXPR_PATTERN_UNARY_EXP, expr_log},
        {EXPR_PATTERN_UNARY_LOG, expr_exp},
    };
    /* Three fixed operation alternatives; structural matching remains owned by expression. */
    for (size_t i = 0; i < sizeof rules / sizeof *rules; ++i) {
        if (!expr_is_unary_pattern_kind(*dependent, rules[i].kind))
            continue;
        *supported = true;
        bool ok = true;
        if (rules[i].kind == EXPR_PATTERN_UNARY_ATANH)
            ok = isolate_strip(domain, target, true);
        else if (rules[i].kind == EXPR_PATTERN_UNARY_LOG)
            ok = isolate_strip(domain, target, false);
        else
            ok = isolate_nonzero(domain, target);
        if (!ok)
            return NULL;
        *dependent = expr_first_child(*dependent);
        *nonlinear = true;
        return rules[i].inverse(target);
    }
    return NULL;
}

/* Keep domain restrictions with the algebra rather than asking clients to repair an unsafe inverse. */
int equ_try_solve_guarded_inverse(const equation_t *equation, const expr_t *variable, equation_solutions_t *solutions)
{
    const expr_t *dependent = equ_lhs(equation), *other = equ_rhs(equation);
    if (!isolate_uses(dependent, variable)) {
        dependent = equ_rhs(equation);
        other = equ_lhs(equation);
    }
    if (!isolate_uses(dependent, variable) || isolate_uses(other, variable))
        return 1;
    expr_t *target = expr_clone(other), *domain = expr_const_long(1);
    if (!target || !domain) {
        expr_free(domain);
        expr_free(target);
        return -1;
    }
    bool nonlinear = false;
    for (unsigned depth = 0; target && domain && !expr_struct_eq(dependent, variable) && depth < 128u; ++depth) {
        if (!equ_computation_step("equation.isolate")) {
            expr_free(target);
            expr_free(domain);
            return -1;
        }
        bool supported = false;
        expr_t *next = isolate_step(&dependent, target, variable, &domain, &nonlinear, &supported);
        if (!next && !supported)
            break;
        expr_free(target);
        if (!next && supported) {
            expr_free(domain);
            return -1;
        }
        target = next ? expr_simplify_owned(next) : NULL;
        if (!target) {
            expr_free(domain);
            return -1;
        }
    }
    int status = 1;
    if (target && nonlinear && !expr_struct_eq(dependent, variable)) {
        /* Polynomial and constant-base power inverses retain every branch from the ordinary native pipeline. */
        equation_t *reduced = equ_reduction_new(equation, dependent, target);
        status = reduced ? equ_solve_for_into(reduced, variable, solutions) : -1;
        equ_free(reduced);
        for (size_t i = 0; status == 0 && i < solutions->count; ++i) {
            equation_t *solution = solutions->solutions[i];
            expr_t *guarded = expr_with_domain_of(equ_rhs(solution), domain);
            equation_t *replacement = guarded ? equ_new(equ_lhs(solution), guarded) : NULL;
            expr_free(guarded);
            if (!replacement)
                status = -1;
            else {
                solutions->solutions[i] = replacement;
                equ_free(solution);
            }
        }
        if (status == 0 && !solutions->count && solutions->status == EQUATION_SOLVE_UNSOLVED)
            status = 1;
    }
    if (target && domain && nonlinear && expr_struct_eq(dependent, variable)) {
        expr_t *guarded = expr_with_domain_of(target, domain);
        guarded = guarded ? expr_simplify_owned(guarded) : NULL;
        number_t constant = NUM_NAN;
        bool empty = guarded && expr_match_const_value(guarded, &constant) && num_is_nan(constant);
        num_destroy(&constant);
        if (empty) {
            solutions->status = EQUATION_SOLVE_NO_SOLUTIONS;
            status = 0;
        } else
            status = guarded ? equ_append_solution_expr(variable, guarded, solutions) : -1;
        expr_free(guarded);
    }
    expr_free(domain);
    expr_free(target);
    return status;
}
