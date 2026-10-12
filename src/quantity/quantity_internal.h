/**
 * @file quantity_internal.h
 * @brief Private unit algebra, conversion dispatch and owned quantity storage.
 *
 * Used only by the quantity implementation. Public callers use quantity-t.h.
 * Eight dimension axes and one symbolic pi exponent share bounded rational
 * arithmetic; registry lookup is a collision-free, checked hash lookup.
 */
#ifndef MARS_QUANTITY_INTERNAL_ACCESS
#error "Define MARS_QUANTITY_INTERNAL_ACCESS immediately before including this header"
#endif
#ifndef MARS_QUANTITY_INTERNAL_H
#define MARS_QUANTITY_INTERNAL_H

#include <stdint.h>

#include "quantity-t.h"

enum { QUANTITY_AXES = 8, QUANTITY_PI = 8, QUANTITY_POWERS = 9, QUANTITY_LIMIT = 32767 };

typedef struct {
    int32_t numerator;
    int32_t denominator;
} quantity_power_t;

typedef struct {
    number_t numerator;
    number_t denominator;
    number_t offset;
    quantity_power_t powers[QUANTITY_POWERS];
    string_t *name;
    bool point_allowed;
} quantity_unit_t;

struct _quantity_t {
    number_t value;
    quantity_unit_t unit;
    bool point;
};

/** @brief Set errno and return NULL for pointer-returning operations. */
void *quantity_fail(int error);

/** @brief Set errno and return false for predicate-returning operations. */
bool quantity_error(int error);

/** @brief Initialise a dimensionless unit with owned exact scale and offset. */
void quantity_unit_init(quantity_unit_t *unit);

/** @brief Release owned unit members; zero-initialised members are permitted. */
void quantity_unit_clear(quantity_unit_t *unit);

/** @brief Copy every unit member, including its independently owned name. */
bool quantity_unit_copy(quantity_unit_t *target, const quantity_unit_t *source);

/** @brief Parse registered aliases and explicit compound-unit expressions. */
bool quantity_unit_parse(quantity_unit_t *unit, const string_t *text);

/** @brief Look up one complete alias through the static perfect hash. */
bool quantity_unit_lookup(quantity_unit_t *unit, const char *name, size_t length);

/** @brief Compare eight physical axes, excluding scale and symbolic pi. */
bool quantity_dimensions_equal(const quantity_unit_t *left, const quantity_unit_t *right);

/** @brief Test for temperature to the first power with no other physical dimensions. */
bool quantity_unit_is_temperature(const quantity_unit_t *unit);

/** @brief Store a reduced bounded rational exponent, reporting EOVERFLOW on excess. */
bool quantity_power_set(quantity_power_t *power, int64_t numerator, int64_t denominator);

/** @brief Combine two rational exponents using addition or subtraction. */
bool quantity_power_add(quantity_power_t *out, quantity_power_t left, quantity_power_t right, int sign);

/** @brief Replace name/scale/offset with coherent SI, retaining physical dimensions. */
bool quantity_unit_coherent(quantity_unit_t *unit);

/** @brief Multiply or divide unit scale fractions without prematurely rounding their integer components. */
void quantity_unit_scale_product(quantity_unit_t *out, const quantity_unit_t *left,
                                 const quantity_unit_t *right, bool divide);

/** @brief Convert a scalar with linear or affine dispatch, preserving exact scale ratios and pi cancellation. */
number_t quantity_convert_value(number_t value, const quantity_unit_t *from, const quantity_unit_t *to, bool point);

/** @brief Copy a unit and finite scalar into a new independently owned quantity. */
quantity_t *quantity_make(number_t value, const quantity_unit_t *unit, bool point);

#endif
