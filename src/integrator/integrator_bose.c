/**
 * @file integrator_bose.c
 * @brief Exact positive-integer logarithmic Bose moments on the positive half-line.
 *
 * Recognises scaled x^m log(exp(a*x)-1)/(exp(a*x)-1), with a positive real
 * scale a and 1 <= m <= 32. Expansion into exponentially damped terms reduces
 * the moment to finite Euler sums of zeta values. No endpoint or overflowing
 * exponential is evaluated. Other structures remain with numerical integration.
 */
#include <stdlib.h>

#include "expression-t.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_INTEGRATOR_INTERNAL_ACCESS
#include "integrator_internal.h"

typedef struct {
    const expr_t *coordinate;
    const expr_t *denominator;
    const expr_t *logarithm;
    unsigned power;
    number_t scale;
} bose_moment_t;

static bool constant_value(const expr_t *expression, const expr_t *coordinate, number_t *value)
{
    expr_t *variables[] = {(expr_t *)coordinate};
    bool used[] = {false};
    if (!expr_collect_var_usage(expression, 1u, variables, used) || used[0])
        return false;
    *value = expr_eval(expression);
    return num_is_real(*value) && num_is_finite(*value);
}

/* Traverse only a bounded product/quotient tree; each factor is consumed once. */
static bool collect_factor(bose_moment_t *moment, const expr_t *expression, bool inverse, unsigned depth)
{
    if (!expression || depth > 32u)
        return false;
    const expr_t *left = NULL, *right = NULL;
    if (expr_match_mul_expr(expression, &left, &right))
        return collect_factor(moment, left, inverse, depth + 1u) &&
               collect_factor(moment, right, inverse, depth + 1u);
    if (expr_match_div_expr(expression, &left, &right))
        return collect_factor(moment, left, inverse, depth + 1u) &&
               collect_factor(moment, right, !inverse, depth + 1u);
    if (expr_match_neg_expr(expression, &left)) {
        number_t next = num_neg(moment->scale);
        num_destroy(&moment->scale);
        moment->scale = next;
        return collect_factor(moment, left, inverse, depth + 1u);
    }
    number_t constant = NUM_NAN;
    if (constant_value(expression, moment->coordinate, &constant) &&
        (!inverse || !num_is_zero(constant))) {
        number_t next = inverse ? num_div(moment->scale, constant) : num_mul(moment->scale, constant);
        num_destroy(&moment->scale);
        moment->scale = next;
        num_destroy(&constant);
        return true;
    }
    num_destroy(&constant);
    number_t exponent = NUM_NAN;
    long numerator = 0, denominator = 0;
    bool power = expr_match_pow_const(expression, &left, &exponent) &&
                 num_get_small_rational(exponent, &numerator, &denominator) && denominator == 1;
    num_destroy(&exponent);
    if (power && numerator == -1)
        return collect_factor(moment, left, !inverse, depth + 1u);
    if (inverse) {
        if (moment->denominator)
            return false;
        moment->denominator = expression;
        return true;
    }
    if (expr_struct_eq(expression, moment->coordinate))
        return ++moment->power <= 32u;
    if (power && numerator >= 0 && numerator <= 32 && expr_struct_eq(left, moment->coordinate)) {
        moment->power += (unsigned)numerator;
        return moment->power <= 32u;
    }
    if (!moment->logarithm && expr_match_log_expr(expression, &left)) {
        moment->logarithm = left;
        return true;
    }
    return false;
}

static bool exponential_rate(const expr_t *denominator, const expr_t *coordinate, number_t *rate)
{
    const expr_t *left = NULL, *right = NULL, *argument = NULL;
    bool subtract = false;
    number_t constant = NUM_NAN;
    bool matched = expr_match_add_sub_expr(denominator, &left, &right, &subtract) &&
                   constant_value(right, coordinate, &constant) &&
                   (subtract ? num_eq(constant, NUM_ONE) : num_eq(constant, NUM_NEG_ONE));
    num_destroy(&constant);
    if (!matched)
        return false;
    bool exponential = expr_match_exp_expr(left, &argument);
    if (!exponential && expr_match_pow_expr(left, &right, &argument)) {
        exponential = constant_value(right, coordinate, &constant) && num_eq(constant, NUM_E);
        num_destroy(&constant);
    }
    if (!exponential)
        return false;
    number_t coefficients[5] = {NUM_ZERO, NUM_ZERO, NUM_ZERO, NUM_ZERO, NUM_ZERO};
    matched = expr_collect_poly_deg4(argument, coordinate, coefficients) && num_is_zero(coefficients[0]) &&
              num_is_real(coefficients[1]) && num_is_finite(coefficients[1]) && num_gt(coefficients[1], NUM_ZERO) &&
              num_is_zero(coefficients[2]) && num_is_zero(coefficients[3]) && num_is_zero(coefficients[4]);
    if (matched)
        *rate = num_clone(coefficients[1]);
    for (size_t i = 0u; i < 5u; ++i)
        num_destroy(&coefficients[i]);
    return matched;
}

static expr_t *zeta_order(unsigned order)
{
    number_t value = num_create_from_long((long)order);
    expr_t *argument = expr_new_const(value);
    expr_t *result = argument ? expr_zeta(argument) : NULL;
    expr_free(argument);
    num_destroy(&value);
    return result;
}

/* The finite Euler-sum identity leaves exact zeta functions in the result graph. */
static expr_t *moment_formula(unsigned power, number_t scale, number_t rate)
{
    number_t factorial = num_factorial(power), order = num_create_from_long((long)power + 1L);
    number_t dilation = num_pow(rate, order), half_factorial = num_mul(factorial, NUM_HALF);
    number_t scaled = num_mul(half_factorial, scale), factor = num_div(scaled, dilation);
    number_t leading = num_mul(factor, order);
    expr_t *zeta = zeta_order(power + 2u);
    expr_t *sum = zeta ? (num_eq(leading, NUM_ONE) ? expr_retain_expr(zeta) : expr_mul_num(zeta, &leading)) : NULL;
    expr_free(zeta);
    for (unsigned k = 1u; sum && k < power; ++k) {
        expr_t *a = zeta_order(power + 1u - k), *b = zeta_order(k + 1u);
        expr_t *product = a && b ? expr_mul(a, b) : NULL;
        expr_t *term = product ? expr_mul_num(product, &factor) : NULL;
        expr_t *next = term ? expr_add(sum, term) : NULL;
        expr_free(term); expr_free(product); expr_free(b); expr_free(a); expr_free(sum);
        sum = next;
    }
    num_destroy(&leading); num_destroy(&factor); num_destroy(&scaled); num_destroy(&half_factorial);
    num_destroy(&dilation); num_destroy(&order); num_destroy(&factorial);
    return sum;
}

/* Match the complete domain and moment before publishing an exact result. */
int intg_try_logarithmic_bose(integrator_t *ig, const expr_t *expression, const expr_t *coordinate,
                               number_t lower, number_t upper, number_t *result, number_t *error)
{
    bool forward = num_eq(lower, NUM_ZERO) && num_eq(upper, NUM_INF);
    bool reverse = num_eq(lower, NUM_INF) && num_eq(upper, NUM_ZERO);
    if (!forward && !reverse)
        return 0;
    bose_moment_t moment = {.coordinate = coordinate, .scale = num_clone(reverse ? NUM_NEG_ONE : NUM_ONE)};
    number_t rate = NUM_NAN;
    bool matched = collect_factor(&moment, expression, false, 0u) && moment.power && moment.denominator &&
                   moment.logarithm && expr_struct_eq(moment.logarithm, moment.denominator) &&
                   exponential_rate(moment.denominator, coordinate, &rate);
    expr_t *exact = matched ? moment_formula(moment.power, moment.scale, rate) : NULL;
    num_destroy(&rate);
    num_destroy(&moment.scale);
    if (!matched)
        return 0;
    if (!exact)
        return -1;
    number_t value = expr_eval(exact);
    if (!num_is_real(value) || !num_is_finite(value)) {
        num_destroy(&value); expr_free(exact);
        return -1;
    }
    *result = value;
    if (error)
        *error = num_clone(NUM_ZERO);
    intg_set_exact_result_owned(ig, exact);
    ig->last_intervals = 1u;
    return 1;
}
