/**
 * @file expr_function_aliases.c
 * @brief Lower numerical convenience spellings into existing native expression operations.
 *
 * These parser constructors preserve symbolic operands and reuse ordinary expression calculus and rendering.
 * Paired-output numerical helpers accept a constant component selector because expressions are scalar valued.
 */
#include "expression-t.h"

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

expr_t *expr_alias_inverse(const expr_t *argument)
{
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *result = expr_div(one, argument);
    expr_free(one);
    return result;
}

expr_t *expr_alias_square(const expr_t *argument)
{
    number_t two = num_create_from_long(2);
    expr_t *result = expr_pow(argument, &two);
    num_destroy(&two);
    return result;
}

expr_t *expr_alias_pow10(const expr_t *argument)
{
    number_t ten = num_create_from_long(10);
    expr_t *base = expr_new_const(ten);
    expr_t *result = expr_pow_xp(base, argument);
    expr_free(base);
    num_destroy(&ten);
    return result;
}

static expr_t *scaled_power(const expr_t *argument, const expr_t *exponent, long radix)
{
    number_t value = num_create_from_long(radix);
    expr_t *base = expr_new_const(value);
    expr_t *power = expr_pow_xp(base, exponent);
    expr_t *result = expr_mul(argument, power);
    expr_free(power);
    expr_free(base);
    num_destroy(&value);
    return result;
}

expr_t *expr_alias_mul_pow10(const expr_t *argument, const expr_t *exponent)
{
    return scaled_power(argument, exponent, 10);
}

expr_t *expr_alias_ldexp(const expr_t *argument, const expr_t *exponent)
{
    return scaled_power(argument, exponent, 2);
}

expr_t *expr_alias_tetragamma(const expr_t *argument)
{
    number_t two = num_create_from_long(2);
    expr_t *order = expr_new_const(two);
    expr_t *result = expr_polygamma_xp(order, argument);
    expr_free(order);
    num_destroy(&two);
    return result;
}

expr_t *expr_alias_arg(const expr_t *argument)
{
    expr_t *real = expr_real_coordinate(argument);
    expr_t *imaginary = expr_imag_coordinate(argument);
    expr_t *result = expr_atan2(imaginary, real);
    expr_free(imaginary);
    expr_free(real);
    return result;
}

static int pair_component(const expr_t *component)
{
    if (!expr_is_const(component))
        return -1;
    number_t value = expr_eval(component);
    int result = num_is_zero(value) ? 0 : num_is_one(value) ? 1 : -1;
    num_destroy(&value);
    return result;
}

expr_t *expr_alias_sincos(const expr_t *argument, const expr_t *component)
{
    int index = pair_component(component);
    return index == 0 ? expr_sin(argument) : index == 1 ? expr_cos(argument) : NULL;
}

expr_t *expr_alias_sinhcosh(const expr_t *argument, const expr_t *component)
{
    int index = pair_component(component);
    return index == 0 ? expr_sinh(argument) : index == 1 ? expr_cosh(argument) : NULL;
}

expr_t *expr_clausen_args(size_t count, expr_t *const *arguments)
{
    if (count == 1)
        return expr_clausen2(arguments[0]);
    return count == 2 ? expr_clausen_xp(arguments[0], arguments[1]) : NULL;
}
