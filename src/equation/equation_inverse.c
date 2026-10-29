/**
 * @file equation_inverse.c
 * @brief Branch-aware isolation of nested roots, exponentials and logarithms.
 *
 * Removes one outer operation at a time and delegates the smaller equation to the normal solver pipeline.
 * Principal square roots are squared only after checking the target's principal-root range. Positive constant
 * exponential bases use a logarithmic inverse; exact integer exponents are certified by exact exponentiation.
 * Uncertified exponents retain their symbolic logarithmic quotient. No input binding is changed here.
 */
#include <math.h>
#include <stdlib.h>

#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"

static bool inverse_uses_variable(const expr_t *expression, const expr_t *variable)
{
    expr_t *variables[] = {(expr_t *)variable};
    bool used = false;
    return expression && expr_collect_var_usage(expression, 1, variables, &used) && used;
}

/* Approximation proposes one integer; exact arithmetic must certify it before it is published. */
static expr_t *inverse_exact_exponent(number_t base, number_t target)
{
    if (!num_is_exact(base) || !num_is_exact(target) || !num_is_real(target) || num_sign(target) <= 0)
        return NULL;
    number_t numerator = num_log(target), denominator = num_log(base);
    number_t quotient = num_div(numerator, denominator);
    double candidate = nearbyint(num_to_double(quotient));
    string_t *text = num_to_string(base);
    size_t length = text ? string_byte_length(text) : 0;
    expr_t *result = NULL;
    /* Bound the optional certificate's exact numerator/denominator growth to about one million decimal digits. */
    if (length && isfinite(candidate) && fabs(candidate) <= 1048576.0 / (double)length &&
        equ_computation_step("equation.inverse.certificate")) {
        number_t exponent = num_create_from_long((long)candidate);
        number_t power = num_pow(base, exponent);
        if (num_is_exact(power) && num_eq(power, target))
            result = expr_new_const(exponent);
        num_destroy(&power);
        num_destroy(&exponent);
    }
    string_free(text);
    num_destroy(&quotient);
    num_destroy(&denominator);
    num_destroy(&numerator);
    return result;
}

/* The principal square root has positive real part, or non-negative imaginary part on its boundary. */
static int inverse_square_root_target(const expr_t *target)
{
    number_t value = expr_eval(target);
    if (!num_is_finite(value)) { num_destroy(&value); return 1; }
    number_t real = num_real_part(value), imaginary = num_imag_part(value);
    bool allowed = num_sign(real) > 0 || (num_is_zero(real) && num_sign(imaginary) >= 0);
    num_destroy(&imaginary);
    num_destroy(&real);
    num_destroy(&value);
    return allowed ? 0 : -1;
}

static expr_t *inverse_power_target(const expr_t *base, const expr_t *target, bool *applicable, bool *empty)
{
    number_t base_value = expr_eval(base), target_value = expr_eval(target);
    *applicable = num_is_finite(base_value) && num_is_real(base_value) && num_sign(base_value) > 0 &&
                  !num_eq(base_value, NUM_ONE);
    *empty = *applicable && num_is_zero(target_value);
    expr_t *inverse = *applicable && !*empty ? inverse_exact_exponent(base_value, target_value) : NULL;
    if (*applicable && !*empty && !inverse) {
        expr_t *numerator = expr_log(target), *denominator = expr_log(base);
        inverse = numerator && denominator ? expr_simplify_owned(expr_div(numerator, denominator)) : NULL;
        expr_free(denominator);
        expr_free(numerator);
    }
    num_destroy(&target_value);
    num_destroy(&base_value);
    return inverse;
}

static int inverse_side(const expr_t *left, const expr_t *right, const expr_t *variable,
                         equation_solutions_t *solutions)
{
    if (!inverse_uses_variable(left, variable) || inverse_uses_variable(right, variable))
        return 1;
    const expr_t *inner = NULL, *base = NULL;
    expr_t *target = NULL;
    if (expr_match_exp_expr(left, &inner))
        target = expr_simplify_owned(expr_log(right));
    else if (expr_match_log_expr(left, &inner))
        target = expr_simplify_owned(expr_exp(right));
    else if (expr_match_sqrt_expr(left, &inner)) {
        int range = inverse_square_root_target(right);
        if (range > 0) return 1;
        if (range < 0) { solutions->status = EQUATION_SOLVE_NO_SOLUTIONS; return 0; }
        target = expr_simplify_owned(expr_pow_long(right, 2));
    } else if (expr_match_pow_expr(left, &base, &inner) && !inverse_uses_variable(base, variable)) {
        bool applicable = false, empty = false;
        target = inverse_power_target(base, right, &applicable, &empty);
        if (!applicable) return 1;
        if (empty) { solutions->status = EQUATION_SOLVE_NO_SOLUTIONS; return 0; }
    } else
        return 1;
    equation_t *reduced = target ? equ_new(inner, target) : NULL;
    int status = reduced ? equ_solve_for_into(reduced, variable, solutions) : -1;
    equ_free(reduced);
    expr_free(target);
    return status;
}

/* Invert an isolated outer operation on either side, preserving the original equation and its bindings. */
int equ_try_solve_unary_inverse(const equation_t *equation, const expr_t *variable, equation_solutions_t *solutions)
{
    if (!equation || !variable || !solutions) return -1;
    int status = inverse_side(equ_lhs(equation), equ_rhs(equation), variable, solutions);
    return status == 1 ? inverse_side(equ_rhs(equation), equ_lhs(equation), variable, solutions) : status;
}
