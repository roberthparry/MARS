#ifndef MRATIONAL_H
#define MRATIONAL_H

#include <stdbool.h>
#include <stddef.h>

#include "mint.h"

/**
 * @file mrational.h
 * @brief Opaque exact rational values backed by GMP rationals.
 */

/**
 * @brief Opaque multiprecision rational type.
 */
typedef struct _mrational_t mrational_t;

/** @name Constants
 * Process-lifetime convenience values owned by the mrational subsystem. They
 * must not be modified or freed by callers.
 * @{
 */
extern const mrational_t * const MR_HALF;
extern const mrational_t * const MR_ONE_AND_HALF;
extern const mrational_t * const MR_ONE_THIRD;
extern const mrational_t * const MR_QUARTER;
extern const mrational_t * const MR_ONE_SIXTH;
extern const mrational_t * const MR_ONE_EIGHTH;
extern const mrational_t * const MR_ONE_TENTH;
/** @} */

/** @name Lifecycle
 * Constructors return newly allocated values or `NULL` on error.
 * @{
 */
mrational_t *mr_new(void);
mrational_t *mr_const(const mrational_t *constant);
mrational_t *mr_create_long(long value);
mrational_t *mr_create_frac_long(long numerator, long denominator);
mrational_t *mr_create_mints(const mint_t *numerator, const mint_t *denominator);
mrational_t *mr_create_string(const char *text);
mrational_t *mr_clone(const mrational_t *rational);
void mr_free(mrational_t *rational);
void mr_clear(mrational_t *rational);
/** @} */

/** @name Setters and formatting
 * @{
 */
int mr_set_long(mrational_t *rational, long value);
int mr_set_frac_long(mrational_t *rational, long numerator, long denominator);
int mr_set_mints(mrational_t *rational, const mint_t *numerator, const mint_t *denominator);
int mr_set_string(mrational_t *rational, const char *text);
char *mr_to_string(const mrational_t *rational);
/** @} */

/** @name Queries and accessors
 * Accessor helpers return borrowed read-only views that remain valid while
 * the owning `mrational_t` is alive.
 * @{
 */
bool mr_is_zero(const mrational_t *rational);
bool mr_is_integer(const mrational_t *rational);
const mint_t *mr_numerator(const mrational_t *rational);
const mint_t *mr_denominator(const mrational_t *rational);
/** @} */

/** @name Comparisons
 * @{
 */
int mr_cmp(const mrational_t *a, const mrational_t *b);
bool mr_eq(const mrational_t *a, const mrational_t *b);
bool mr_lt(const mrational_t *a, const mrational_t *b);
bool mr_le(const mrational_t *a, const mrational_t *b);
bool mr_gt(const mrational_t *a, const mrational_t *b);
bool mr_ge(const mrational_t *a, const mrational_t *b);
/** @} */

/** @name Arithmetic
 * In-place exact rational arithmetic on the first argument.
 * @{
 */
int mr_neg(mrational_t *rational);
int mr_abs(mrational_t *rational);
int mr_inv(mrational_t *rational);
int mr_add(mrational_t *rational, const mrational_t *other);
int mr_sub(mrational_t *rational, const mrational_t *other);
int mr_mul(mrational_t *rational, const mrational_t *other);
int mr_div(mrational_t *rational, const mrational_t *other);
int mr_pow_int(mrational_t *rational, int exponent);
/** @} */

#endif
