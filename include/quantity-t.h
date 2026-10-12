/**
 * @file quantity-t.h
 * @brief Immutable, dimension-checked physical quantities backed by number_t.
 *
 * A quantity owns a scalar, a unit and rational powers of the seven SI base
 * dimensions plus a separate angle dimension. Defined conversion factors remain
 * exact rationals; powers of pi are retained until numerical conversion needs
 * them. Temperature points are distinct from temperature differences.
 * Physical scale factors share number_t's immortal SI constants.
 *
 * Construction, numerical results, scalar access and formatting use the current
 * number_t default precision and rational-size budget. Exact integers and small
 * rationals remain exact. Changing the setting does not mutate existing inputs
 * or recover digits already rounded in ordinary stored values.
 *
 * Constructors copy their arguments. Results are independently owned and must
 * be released with quantity_free(); quantity_value() returns an owned number.
 * Concurrent readers follow number_t's precision and threading rules. No mutable
 * unit registry or process-wide conversion state is introduced.
 *
 * Failures return NULL or false and set errno: EINVAL for invalid input or unit
 * syntax, EDOM for incompatible dimensions or operations, EOVERFLOW for bounded
 * parser/exponent limits, and ENOMEM for allocation failure. Scalars must be
 * finite. This module does not attach units to expression graphs or propagate
 * measurement uncertainty. See docs/quantity.md for unit conventions and syntax.
 */
#ifndef MARS_QUANTITY_H
#define MARS_QUANTITY_H

#include <stdbool.h>

#include "number-t.h"
#include "expression-t.h"

/** @brief Opaque, independently owned physical quantity. */
typedef struct _quantity_t quantity_t;

/** @brief Copy a finite scalar and unit; ordinary K, degC and degF denote temperature readings. */
quantity_t *quantity_new(number_t value, const string_t *unit);

/** @brief Copy a real scalar as an affine temperature point in K, degC or degF. */
quantity_t *quantity_new_point(number_t value, const string_t *unit);

/** @brief Construct a temperature difference; reject units with any other dimensions. */
quantity_t *quantity_new_difference(number_t value, const string_t *unit);

/** @brief Parse `number [unit]` or `difference(number [unit])` (temperature difference); return an owned quantity. */
quantity_t *quantity_from_string(const string_t *text);

/** @brief Return an independent copy; NULL input fails with EINVAL. */
quantity_t *quantity_clone(const quantity_t *quantity);

/** @brief Release a quantity and its owned components; NULL is harmless. */
void quantity_free(quantity_t *quantity);

/** @brief Return an owned scalar at the selected precision; NULL returns an invalid number and sets EINVAL. */
number_t quantity_value(const quantity_t *quantity);

/** @brief Borrow the current unit spelling until the quantity is freed; NULL input sets EINVAL. */
const string_t *quantity_unit(const quantity_t *quantity);

/** @brief Test whether this is an affine temperature point; NULL returns false. */
bool quantity_is_point(const quantity_t *quantity);

/** @brief Compare dimensions only, not the temperature point/difference distinction; NULL inputs set EINVAL. */
bool quantity_compatible(const quantity_t *left, const quantity_t *right);

/** @brief Convert to compatible units, preserving temperature point/difference semantics. */
quantity_t *quantity_convert(const quantity_t *quantity, const string_t *unit);

/** @brief Return a copy expressed in coherent SI base units; angles retain rad. */
quantity_t *quantity_to_si(const quantity_t *quantity);

/** @brief Add compatible differences or translate a temperature point; point plus point is invalid. */
quantity_t *quantity_add(const quantity_t *left, const quantity_t *right);

/** @brief Subtract compatible quantities; point minus point yields a K difference, difference minus point fails. */
quantity_t *quantity_sub(const quantity_t *left, const quantity_t *right);

/** @brief Multiply linear quantities, returning coherent SI units; temperature points are invalid. */
quantity_t *quantity_mul(const quantity_t *left, const quantity_t *right);

/** @brief Divide linear quantities, returning coherent SI units; zero divisors and temperature points are invalid. */
quantity_t *quantity_div(const quantity_t *left, const quantity_t *right);

/** @brief Negate a linear quantity, preserving its unit; temperature points are invalid. */
quantity_t *quantity_neg(const quantity_t *quantity);

/** @brief Multiply a linear quantity by a finite scalar, preserving its unit. */
quantity_t *quantity_scale(const quantity_t *quantity, number_t factor);

/** @brief Raise a linear quantity to an integer power, returning coherent SI units. */
quantity_t *quantity_pow_int(const quantity_t *quantity, int exponent);

/** @brief Take a principal root of positive degree, retaining rational dimension powers in coherent SI units. */
quantity_t *quantity_root(const quantity_t *quantity, unsigned degree);

/** @brief Order compatible real quantities of the same point/difference kind; write -1, 0 or 1 on success. */
bool quantity_compare(const quantity_t *left, const quantity_t *right, int *ordering);

/** @brief Sine of an angle (converted to radians) or a dimensionless scalar; return dimensionless units. */
quantity_t *quantity_sin(const quantity_t *quantity);

/** @brief Cosine of an angle (converted to radians) or a dimensionless scalar; return dimensionless units. */
quantity_t *quantity_cos(const quantity_t *quantity);

/** @brief Tangent of an angle (converted to radians) or a dimensionless scalar; return dimensionless units. */
quantity_t *quantity_tan(const quantity_t *quantity);

/** @brief Exponential of a dimensionless linear quantity; dimensioned arguments fail. */
quantity_t *quantity_exp(const quantity_t *quantity);

/** @brief Principal natural logarithm of a dimensionless linear quantity; zero fails. */
quantity_t *quantity_log(const quantity_t *quantity);

/**
 * @brief Return owned text, released with string_free().
 *
 * FUNCTION emits the quantity parser's `number [unit]` notation, not an
 * expression-language function. EXPRESSION and UNBOUND use mathematical scalar
 * formatting; LATEX uses mathematical scalars and upright units. Temperature
 * readings have no special marker; differences use an explicit difference(...) wrapper.
 */
string_t *quantity_to_string(const quantity_t *quantity, style_t style);

#endif
