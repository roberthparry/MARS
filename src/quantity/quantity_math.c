/**
 * @file quantity_math.c
 * @brief Dimension-checked arithmetic and elementary quantity functions.
 *
 * Affine temperature points can be translated or subtracted, not multiplied.
 * Products and powers return coherent SI units with rational dimension powers.
 */
#include <errno.h>

#define MARS_QUANTITY_INTERNAL_ACCESS
#include "quantity_internal.h"

static quantity_t *quantity_sum(const quantity_t *left, const quantity_t *right, bool subtract)
{
    if (!left || !right)
        return quantity_fail(EINVAL);
    if (!quantity_compatible(left, right) || (!subtract && left->point && right->point) ||
        (subtract && !left->point && right->point))
        return quantity_fail(EDOM);
    if (!subtract && right->point)
        return quantity_sum(right, left, false);
    bool difference = subtract && left->point && right->point;
    number_t converted = quantity_convert_value(right->value, &right->unit, &left->unit, difference);
    number_t value = subtract ? num_sub(left->value, converted) : num_add(left->value, converted);
    quantity_t *result = quantity_make(value, &left->unit, left->point && !difference);
    num_destroy(&converted);
    num_destroy(&value);
    if (difference && result) {
        quantity_t *coherent = quantity_to_si(result);
        quantity_free(result);
        result = coherent;
    }
    return result;
}

/* Keep the left unit, except that difference plus point keeps the point's unit. */
quantity_t *quantity_add(const quantity_t *left, const quantity_t *right)
{
    return quantity_sum(left, right, false);
}

/* Differences between temperature points are returned in kelvins. */
quantity_t *quantity_sub(const quantity_t *left, const quantity_t *right)
{
    return quantity_sum(left, right, true);
}

static quantity_t *quantity_product(const quantity_t *left, const quantity_t *right, bool divide)
{
    if (!left || !right)
        return quantity_fail(EINVAL);
    if (left->point || right->point || (divide && num_is_zero(right->value)))
        return quantity_fail(EDOM);
    quantity_t *combined = quantity_clone(left), *result = NULL;
    if (!combined)
        return NULL;
    for (unsigned i = 0; i < QUANTITY_POWERS; ++i)
        if (!quantity_power_add(&combined->unit.powers[i], left->unit.powers[i], right->unit.powers[i],
                                divide ? -1 : 1))
            goto done;
    number_t value = divide ? num_div(left->value, right->value) : num_mul(left->value, right->value);
    num_destroy(&combined->value);
    combined->value = value;
    quantity_unit_scale_product(&combined->unit, &left->unit, &right->unit, divide);
    /* Cancel unit factors and pi before performing any inexact conversion. */
    result = quantity_to_si(combined);
done:
    quantity_free(combined);
    return result;
}

/* Multiplication combines physical dimensions, without guessing a named unit. */
quantity_t *quantity_mul(const quantity_t *left, const quantity_t *right)
{
    return quantity_product(left, right, false);
}

/* Division rejects zero before performing scalar arithmetic. */
quantity_t *quantity_div(const quantity_t *left, const quantity_t *right)
{
    return quantity_product(left, right, true);
}

/* A scalar multiplier cannot translate an affine point. */
quantity_t *quantity_scale(const quantity_t *quantity, number_t factor)
{
    if (!quantity)
        return quantity_fail(EINVAL);
    if (quantity->point || !num_is_finite(factor))
        return quantity_fail(EDOM);
    number_t value = num_mul(quantity->value, factor);
    quantity_t *result = quantity_make(value, &quantity->unit, false);
    num_destroy(&value);
    return result;
}

/* Negation preserves a linear quantity's chosen unit. */
quantity_t *quantity_neg(const quantity_t *quantity)
{
    number_t minus_one = num_create_from_frac(-1, 1);
    quantity_t *result = quantity_scale(quantity, minus_one);
    num_destroy(&minus_one);
    return result;
}

static quantity_t *quantity_power(const quantity_t *quantity, int numerator, unsigned denominator)
{
    if (!quantity || !denominator)
        return quantity_fail(EINVAL);
    if (quantity->point || (numerator <= 0 && num_is_zero(quantity->value)))
        return quantity_fail(EDOM);
    if (numerator < -QUANTITY_LIMIT || numerator > QUANTITY_LIMIT || denominator > QUANTITY_LIMIT)
        return quantity_fail(EOVERFLOW);
    quantity_t *si = quantity_to_si(quantity), *result = NULL;
    if (!si)
        return NULL;
    for (unsigned i = 0; i < QUANTITY_AXES; ++i) {
        quantity_power_t *power = &si->unit.powers[i];
        if (!quantity_power_set(power, (int64_t)power->numerator * numerator,
                                (int64_t)power->denominator * denominator))
            goto done;
    }
    if (!quantity_unit_coherent(&si->unit))
        goto done;
    number_t exponent = num_create_from_frac(numerator, denominator);
    number_t value = num_pow(si->value, exponent);
    result = quantity_make(value, &si->unit, false);
    num_destroy(&exponent);
    num_destroy(&value);
done:
    quantity_free(si);
    return result;
}

/* Integer powers preserve exact scalar arithmetic where number_t can do so. */
quantity_t *quantity_pow_int(const quantity_t *quantity, int exponent)
{
    return quantity_power(quantity, exponent, 1);
}

/* Principal roots retain fractional dimensions instead of discarding units. */
quantity_t *quantity_root(const quantity_t *quantity, unsigned degree)
{
    return quantity_power(quantity, 1, degree);
}

/* Complex values and mismatched coordinate kinds have no real ordering here. */
bool quantity_compare(const quantity_t *left, const quantity_t *right, int *ordering)
{
    if (!left || !right || !ordering)
        return quantity_error(EINVAL);
    if (!quantity_compatible(left, right) || left->point != right->point ||
        !num_is_real(left->value) || !num_is_real(right->value))
        return quantity_error(EDOM);
    number_t converted = quantity_convert_value(right->value, &right->unit, &left->unit, left->point);
    bool valid = num_is_finite(converted);
    if (valid)
        *ordering = num_cmp(left->value, converted);
    else
        errno = EDOM;
    num_destroy(&converted);
    return valid;
}

static quantity_t *quantity_function(const quantity_t *quantity, number_t (*function)(number_t), bool angle)
{
    if (!quantity)
        return quantity_fail(EINVAL);
    if (quantity->point)
        return quantity_fail(EDOM);
    for (unsigned i = 0; i < QUANTITY_AXES; ++i) {
        quantity_power_t power = quantity->unit.powers[i];
        if (power.numerator && !(angle && i == 7 && power.numerator == power.denominator))
            return quantity_fail(EDOM);
    }
    quantity_t *si = quantity_to_si(quantity);
    if (!si)
        return NULL;
    number_t value = function(si->value);
    quantity_unit_t unit;
    quantity_unit_init(&unit);
    quantity_t *result = quantity_make(value, &unit, false);
    quantity_unit_clear(&unit);
    num_destroy(&value);
    quantity_free(si);
    return result;
}

/* Trigonometry consumes angles in radians or dimensionless scalars. */
quantity_t *quantity_sin(const quantity_t *quantity)
{
    return quantity_function(quantity, num_sin, true);
}

/* Cosine returns a dimensionless scalar. */
quantity_t *quantity_cos(const quantity_t *quantity)
{
    return quantity_function(quantity, num_cos, true);
}

/* Tangent returns a dimensionless scalar, rejecting non-finite results. */
quantity_t *quantity_tan(const quantity_t *quantity)
{
    return quantity_function(quantity, num_tan, true);
}

/* Exponentials require a dimensionless argument. */
quantity_t *quantity_exp(const quantity_t *quantity)
{
    return quantity_function(quantity, num_exp, false);
}

/* The scalar layer supplies the principal logarithm branch. */
quantity_t *quantity_log(const quantity_t *quantity)
{
    return quantity_function(quantity, num_log, false);
}
