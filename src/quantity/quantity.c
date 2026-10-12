/**
 * @file quantity.c
 * @brief Quantity ownership, affine points and public conversion operations.
 *
 * All public results own independent number and unit storage. The shared unit
 * layer supplies dimension checks and linear/affine conversion dispatch.
 */
#include <errno.h>
#include <stdlib.h>

#define MARS_QUANTITY_INTERNAL_ACCESS
#include "quantity_internal.h"

quantity_t *quantity_make(number_t value, const quantity_unit_t *unit, bool point)
{
    if (!num_is_finite(value) || (point && !num_is_real(value)))
        return quantity_fail(EDOM);
    quantity_t *quantity = calloc(1, sizeof(*quantity));
    if (!quantity)
        return quantity_fail(ENOMEM);
    quantity->value = num_const(value);
    quantity->point = point;
    if (!num_is_finite(quantity->value) || !quantity_unit_copy(&quantity->unit, unit)) {
        quantity_free(quantity);
        return quantity_fail(ENOMEM);
    }
    return quantity;
}

typedef enum {
    QUANTITY_DEFAULT,
    QUANTITY_POINT,
    QUANTITY_DIFFERENCE
} quantity_construction_t;

static quantity_t *quantity_construct(number_t value, const string_t *name, quantity_construction_t kind)
{
    quantity_unit_t unit = {0};
    if (!quantity_unit_parse(&unit, name))
        return NULL;
    bool valid = (kind != QUANTITY_POINT || unit.point_allowed) &&
                 (kind != QUANTITY_DIFFERENCE || quantity_unit_is_temperature(&unit));
    bool point = kind != QUANTITY_DIFFERENCE && unit.point_allowed;
    quantity_t *result = valid ? quantity_make(value, &unit, point) : quantity_fail(EDOM);
    quantity_unit_clear(&unit);
    return result;
}

/* Ordinary temperature units denote readings; other units denote linear quantities. */
quantity_t *quantity_new(number_t value, const string_t *unit)
{
    return quantity_construct(value, unit, QUANTITY_DEFAULT);
}

/* Explicitly require an ordinary temperature unit. */
quantity_t *quantity_new_point(number_t value, const string_t *unit)
{
    return quantity_construct(value, unit, QUANTITY_POINT);
}

/* Temperature differences use scale alone, without an origin offset. */
quantity_t *quantity_new_difference(number_t value, const string_t *unit)
{
    return quantity_construct(value, unit, QUANTITY_DIFFERENCE);
}

/* Copies retain their own number and string ownership. */
quantity_t *quantity_clone(const quantity_t *quantity)
{
    return quantity ? quantity_make(quantity->value, &quantity->unit, quantity->point) : quantity_fail(EINVAL);
}

/* Dispose of independent storage; freeing NULL is permitted. */
void quantity_free(quantity_t *quantity)
{
    if (!quantity)
        return;
    num_destroy(&quantity->value);
    quantity_unit_clear(&quantity->unit);
    free(quantity);
}

/* The returned number belongs to the caller, not to the quantity. */
number_t quantity_value(const quantity_t *quantity)
{
    if (!quantity) {
        errno = EINVAL;
        return (number_t){0};
    }
    return num_const(quantity->value);
}

/* Borrow the canonical alias or validated compound-unit spelling. */
const string_t *quantity_unit(const quantity_t *quantity)
{
    return quantity ? quantity->unit.name : quantity_fail(EINVAL);
}

/* A NULL handle is not a temperature point. */
bool quantity_is_point(const quantity_t *quantity)
{
    return quantity && quantity->point;
}

/* Compatibility refers to dimensions, independently of affine semantics. */
bool quantity_compatible(const quantity_t *left, const quantity_t *right)
{
    if (!left || !right)
        return quantity_error(EINVAL);
    return quantity_dimensions_equal(&left->unit, &right->unit);
}

/* Preserve the coordinate kind and dispatch conversion without double intermediates. */
quantity_t *quantity_convert(const quantity_t *quantity, const string_t *name)
{
    if (!quantity)
        return quantity_fail(EINVAL);
    quantity_unit_t unit = {0};
    if (!quantity_unit_parse(&unit, name))
        return NULL;
    quantity_t *result = NULL;
    if (!quantity_dimensions_equal(&quantity->unit, &unit) || (quantity->point && !unit.point_allowed))
        errno = EDOM;
    else {
        number_t value = quantity_convert_value(quantity->value, &quantity->unit, &unit, quantity->point);
        result = quantity_make(value, &unit, quantity->point);
        num_destroy(&value);
    }
    quantity_unit_clear(&unit);
    return result;
}

/* Coherent output preserves the independent angle dimension. */
quantity_t *quantity_to_si(const quantity_t *quantity)
{
    if (!quantity)
        return quantity_fail(EINVAL);
    quantity_unit_t unit = {0};
    if (!quantity_unit_copy(&unit, &quantity->unit) || !quantity_unit_coherent(&unit)) {
        quantity_unit_clear(&unit);
        return quantity_fail(ENOMEM);
    }
    number_t value = quantity_convert_value(quantity->value, &quantity->unit, &unit, quantity->point);
    quantity_t *result = quantity_make(value, &unit, quantity->point);
    num_destroy(&value);
    quantity_unit_clear(&unit);
    return result;
}
