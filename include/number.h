#ifndef NUMBER_H
#define NUMBER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

#include "mcomplex.h"
#include "mfloat.h"
#include "mint.h"
#include "mrational.h"
#include "qcomplex.h"
#include "qfloat.h"

/**
 * @file number.h
 * @brief Generic numeric value cluster with by-value public handles.
 *
 * `number_t` is a fixed-size public value type that can represent several
 * numeric backends behind one uniform interface. The current implementation
 * may store exact, fixed-precision, or multiprecision numeric representations
 * internally while keeping one stable public API.
 *
 * Construction is explicit for typed inputs, while string parsing chooses the
 * most suitable representation by syntax:
 *
 * - integer text -> `mint_t`
 * - `a/b` fraction text -> `mrational_t`
 * - decimal / scientific real text -> `mfloat_t`
 * - complex text -> `mcomplex_t`
 *
 * Unless a precision is specified explicitly, multiprecision construction
 * uses `1024` bits.
 *
 * Ownership model:
 *
 * - functions returning `number_t` return a live value
 * - callers should call `num_clear(&value)` when finished with any such value
 * - passing `number_t` by value performs a shallow copy only
 * - shallow copies may alias the same heap-backed payload internally
 * - use `num_clone()` when an independent live copy is required
 * - `char *` strings returned by `num_to_string()` must be released with
 *   `free()`
 *
 * For immortal-backed values such as `NUM_PI`, `NUM_E`, and `NUM_HALF`,
 * `num_clear(&value)` is safe and may be a no-op.
 *
 * Pure arithmetic helpers never mutate their by-value inputs. Functions that
 * write to an existing `number_t` take a `number_t *` destination.
 */

/**
 * @brief Fixed-size public storage for a generic numeric value.
 *
 * Callers may copy, return, and pass `number_t` by value. The storage words
 * are public only so the type has a known size; callers must treat them as
 * opaque and must not inspect or modify them directly.
 *
 * Some `number_t` values may own heap-backed payload internally, so
 * `num_clear()` must be used to release resources when a live value is no
 * longer needed.
 */
typedef struct _number_t {
    uint64_t storage[5];
} number_t;

/** @name Lifecycle
 * Constructors and pure transforms returning `number_t` produce live values.
 * Call `num_clear(&value)` when a returned value is no longer needed.
 *
 * Constructors taking `const ... *` inputs clone or otherwise capture the
 * referenced value into the returned `number_t`. They do not make the caller
 * responsible for extending the source object's lifetime.
 *
 * Returned values are owning values. Even though `number_t` itself is passed
 * by value, the returned object may still manage heap-backed internal state.
 * @{
 */
number_t num_new(void);
number_t num_new_prec(size_t precision_bits);
number_t num_create_long(long value);
number_t num_create_double(double value);
number_t num_create_qfloat(qfloat_t value);
number_t num_create_qcomplex(qcomplex_t value);
number_t num_create_mint(const mint_t *value);
number_t num_create_mrational(const mrational_t *value);
/**
 * @brief Creates a generic number by cloning an `mfloat_t`.
 *
 * The clone is retargeted to the current `number_t` default multiprecision
 * working precision.
 */
number_t num_create_mfloat(const mfloat_t *value);
/** @brief Creates a generic number by cloning an `mfloat_t` at a specific bit precision. */
number_t num_create_mfloat_prec(const mfloat_t *value, size_t precision_bits);
/** @brief Creates a generic number by cloning an `mfloat_t` at a specific decimal-digit precision. */
number_t num_create_mfloat_digits(const mfloat_t *value, size_t significant_digits);
/**
 * @brief Creates a generic number by cloning an `mcomplex_t`.
 *
 * The clone is retargeted to the current `number_t` default multiprecision
 * working precision.
 */
number_t num_create_mcomplex(const mcomplex_t *value);
/** @brief Creates a generic number by cloning an `mcomplex_t` at a specific bit precision. */
number_t num_create_mcomplex_prec(const mcomplex_t *value, size_t precision_bits);
/** @brief Creates a generic number by cloning an `mcomplex_t` at a specific decimal-digit precision. */
number_t num_create_mcomplex_digits(const mcomplex_t *value, size_t significant_digits);
/**
 * @brief Parses text into the most suitable numeric representation.
 *
 * Parsing follows the syntax-to-backend mapping described in the file-level
 * documentation.
 */
number_t num_create_string(const char *text);
/**
 * @brief Process-lifetime `number_t` constant representing exact zero.
 */
extern const number_t NUM_ZERO;
/**
 * @brief Process-lifetime `number_t` constant representing exact one.
 */
extern const number_t NUM_ONE;
extern const number_t NUM_NEG_ONE;
/**
 * @brief Process-lifetime `number_t` constant representing the exact rational value `1/2`.
 *
 * `num_clear()` on a by-value copy is safe and is a no-op for the shared
 * immortal payload.
 */
extern const number_t NUM_HALF;
extern const number_t NUM_ONE_AND_HALF;
extern const number_t NUM_ONE_THIRD;
extern const number_t NUM_QUARTER;
extern const number_t NUM_ONE_SIXTH;
extern const number_t NUM_ONE_EIGHTH;
extern const number_t NUM_ONE_TENTH;
extern const number_t NUM_TWO;
/**
 * @brief Process-lifetime `number_t` constant representing exact ten.
 */
extern const number_t NUM_TEN;
extern const number_t NUM_NAN;
extern const number_t NUM_INF;
extern const number_t NUM_NINF;
/**
 * @brief Process-lifetime `number_t` constant representing pi.
 */
extern const number_t NUM_PI;
extern const number_t NUM_2PI;
extern const number_t NUM_PI_2;
extern const number_t NUM_PI_4;
extern const number_t NUM_3PI_4;
extern const number_t NUM_PI_6;
extern const number_t NUM_PI_3;
extern const number_t NUM_2_PI;
/**
 * @brief Process-lifetime `number_t` constant representing Euler's number.
 */
extern const number_t NUM_E;
extern const number_t NUM_INV_E;
extern const number_t NUM_LN2;
extern const number_t NUM_INVLN2;
/**
 * @brief Process-lifetime `number_t` constant representing the Euler-Mascheroni constant.
 */
extern const number_t NUM_EULER_MASCHERONI;
extern const number_t NUM_SQRT_HALF;
extern const number_t NUM_SQRT2;
extern const number_t NUM_SQRT3;
extern const number_t NUM_SQRT2_OVER_TWO;
extern const number_t NUM_SQRT3_OVER_TWO;
extern const number_t NUM_SQRT_2PI;
extern const number_t NUM_SQRT_PI;
extern const number_t NUM_SQRT_PI_OVER_TWO;
extern const number_t NUM_SQRT1ONPI;
extern const number_t NUM_2_SQRTPI;
extern const number_t NUM_NEG_TWO_OVER_SQRT_PI;
extern const number_t NUM_INV_SQRT_2PI;
extern const number_t NUM_LOG_SQRT_2PI;
extern const number_t NUM_LN_2PI;
extern const number_t NUM_PI_SQUARED;
extern const number_t NUM_2PI_CUBED;
/**
 * @brief Process-lifetime `number_t` constant representing the imaginary unit.
 */
extern const number_t NUM_I;
/**
 * @brief Returns an owning `number_t` representing `10^exponent10`.
 *
 * Call `num_clear(&value)` when finished with the returned value.
 */
number_t num_pow10(int exponent10);
/**
 * @brief Returns an independent live copy of a `number_t`.
 *
 * Use this when a duplicated value must later be cleared independently from
 * the original.
 */
number_t num_clone(const number_t number);
/**
 * @brief Releases any owned payload and resets a number to the invalid state.
 *
 * Call this exactly once for each live owning `number_t` value when it is no
 * longer needed. It is safe to call on an already-cleared value.
 *
 * Because `number_t` uses shallow by-value copies, callers must avoid clearing
 * multiple aliases of the same underlying payload unless they first made an
 * independent copy with `num_clone()`.
 */
void num_clear(number_t *number);
/** @} */

/** @name Precision, setup, and conversion
 * Setter functions mutate an existing destination in place.
 *
 * Precision setters affect only backends where precision is meaningful.
 * Exact integer and rational representations may treat such requests as
 * metadata no-ops.
 * @{
 */
int num_set_default_precision(size_t precision_bits);
size_t num_get_default_precision(void);
int num_set_default_precision_digits(size_t significant_digits);
size_t num_get_default_precision_digits(void);
int num_set_precision(number_t *number, size_t precision_bits);
size_t num_get_precision(const number_t number);
int num_set_precision_digits(number_t *number, size_t significant_digits);
size_t num_get_precision_digits(const number_t number);
int num_set_long(number_t *number, long value);
int num_set_double(number_t *number, double value);
int num_set_qfloat(number_t *number, qfloat_t value);
int num_set_mrational(number_t *number, const mrational_t *value);
int num_set_string(number_t *number, const char *text);
/**
 * @brief Formats a numeric value as a newly allocated string.
 *
 * The returned string must be released with `free()`.
 */
char *num_to_string(const number_t number);
double num_to_double(const number_t number);
qfloat_t num_to_qfloat(const number_t number);
/** @} */

/** @name Formatted output
 * `number_t` values participate in the library's formatted output helpers.
 *
 * The `%n` conversion prints a pretty human-readable representation.
 * The `%N` conversion prefers scientific notation for floating-point and
 * complex backends and otherwise falls back to the ordinary exact form.
 *
 * Width and precision follow the library's floating formatting rules when the
 * active backend is floating-point or complex. For exact integer and rational
 * backends, floating-style precision specifiers are ignored.
 * @{
 */
/**
 * @brief Formats into a caller-provided buffer using `%n` / `%N`.
 *
 * `number_t` arguments are passed by value through the variadic argument list.
 */
int num_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap);
/** @brief Formats into a caller-provided buffer using `%n` / `%N`. */
int num_sprintf(char *out, size_t out_size, const char *fmt, ...);
/** @brief Prints using `%n` / `%N` formatting rules for `number_t`. */
int num_printf(const char *fmt, ...);
/** @} */

/** @name Queries and comparisons
 * Query helpers inspect the current stored value.
 *
 * Equality is supported across mixed backends through the generic promotion
 * rules. Ordering (`num_lt`, `num_le`, `num_gt`, `num_ge`, `num_cmp`) is only
 * meaningful for real values; if either operand is non-real, the relational
 * predicates return `false` and `num_cmp` returns `0`.
 *
 * These functions do not take ownership of their inputs and never mutate the
 * passed values.
 * @{
 */
bool num_is_exact(const number_t number);
bool num_is_real(const number_t number);
bool num_is_integer(const number_t number);
bool num_is_finite(const number_t number);
bool num_is_nan(const number_t number);
bool num_is_inf(const number_t number);
bool num_is_zero(const number_t number);
bool num_is_one(const number_t number);
short num_get_sign(const number_t number);
long num_get_exponent2(const number_t number);
size_t num_get_mantissa_bits(const number_t number);
bool num_get_mantissa_u64(const number_t number, uint64_t *out);
int num_sign(const number_t number);
bool num_eq(const number_t a, const number_t b);
bool num_lt(const number_t a, const number_t b);
bool num_le(const number_t a, const number_t b);
bool num_gt(const number_t a, const number_t b);
bool num_ge(const number_t a, const number_t b);
int num_cmp(const number_t a, const number_t b);
/** @} */

/** @name Core arithmetic
 * Each function returning `number_t` returns an owning value. Call
 * `num_clear(&result)` when finished with it.
 *
 * Binary operations may promote operands internally to a common backend before
 * evaluating the operation.
 *
 * Input values are passed by value but are treated as borrowed read-only
 * handles. Implementations must not clear or otherwise destroy them.
 * @{
 */
number_t num_neg(const number_t number);
number_t num_abs(const number_t number);
number_t num_inv(const number_t number);
number_t num_conj(const number_t number);
number_t num_real_part(const number_t number);
number_t num_imag_part(const number_t number);
number_t num_arg(const number_t number);
number_t num_add(const number_t a, const number_t b);
number_t num_add_mrational(const number_t number, const mrational_t *value);
number_t num_add_long(const number_t number, long value);
number_t num_sub(const number_t a, const number_t b);
number_t num_mul(const number_t a, const number_t b);
number_t num_mul_long(const number_t number, long value);
number_t num_mul_mrational(const number_t number, const mrational_t *value);
number_t num_div(const number_t a, const number_t b);
number_t num_pow(const number_t base, const number_t exponent);
number_t num_pow_int(const number_t base, int exponent);
number_t num_ldexp(const number_t number, int exponent2);
/** @} */

/** @name Elementary functions
 * Each function returning `number_t` returns an owning value. Call
 * `num_clear(&result)` when finished with it.
 *
 * If a backend does not support an operation natively, the implementation may
 * promote to a richer floating or complex backend first.
 * @{
 */
number_t num_exp(const number_t number);
number_t num_log(const number_t number);
number_t num_sqrt(const number_t number);
number_t num_sqr(const number_t number);
number_t num_floor(const number_t number);
number_t num_mul_pow10(const number_t number, int exponent10);
number_t num_hypot(const number_t a, const number_t b);
/**
 * @brief Computes `sin(x)` and `cos(x)` into caller-provided outputs.
 *
 * `sin_out` and `cos_out` must point to fresh or already-cleared `number_t`
 * objects, such as values returned by `num_new()`. This function does not
 * release any existing live contents in those outputs; callers remain
 * responsible for clearing them first if needed.
 */
int num_sincos(const number_t x, number_t *sin_out, number_t *cos_out);
number_t num_sin(const number_t number);
number_t num_cos(const number_t number);
number_t num_tan(const number_t number);
number_t num_atan(const number_t number);
number_t num_atan2(const number_t y, const number_t x);
number_t num_asin(const number_t number);
number_t num_acos(const number_t number);
number_t num_sinh(const number_t number);
number_t num_cosh(const number_t number);
/**
 * @brief Computes `sinh(x)` and `cosh(x)` into caller-provided outputs.
 *
 * `sinh_out` and `cosh_out` must point to fresh or already-cleared `number_t`
 * objects, such as values returned by `num_new()`. This function does not
 * release any existing live contents in those outputs; callers remain
 * responsible for clearing them first if needed.
 */
int num_sinhcosh(const number_t x, number_t *sinh_out, number_t *cosh_out);
number_t num_tanh(const number_t number);
number_t num_asinh(const number_t number);
number_t num_acosh(const number_t number);
number_t num_atanh(const number_t number);
/** @} */

/** @name Special functions
 * Each function returning `number_t` returns an owning value. Call
 * `num_clear(&result)` when finished with it.
 *
 * Special functions typically promote to the richest available backend needed
 * to evaluate the requested function.
 * @{
 */
number_t num_gamma(const number_t number);
number_t num_lgamma(const number_t number);
number_t num_digamma(const number_t number);
number_t num_trigamma(const number_t number);
number_t num_tetragamma(const number_t number);
number_t num_gammainv(const number_t number);
number_t num_erf(const number_t number);
number_t num_erfc(const number_t number);
number_t num_erfinv(const number_t number);
number_t num_erfcinv(const number_t number);
number_t num_lambert_w0(const number_t number);
number_t num_lambert_wm1(const number_t number);
number_t num_beta(const number_t a, const number_t b);
number_t num_logbeta(const number_t a, const number_t b);
number_t num_binomial(const number_t n, const number_t k);
number_t num_beta_pdf(const number_t x, const number_t a, const number_t b);
number_t num_logbeta_pdf(const number_t x, const number_t a, const number_t b);
number_t num_normal_pdf(const number_t number);
number_t num_normal_cdf(const number_t number);
number_t num_normal_logpdf(const number_t number);
number_t num_productlog(const number_t number);
number_t num_gammainc_lower(const number_t s, const number_t x);
number_t num_gammainc_upper(const number_t s, const number_t x);
number_t num_gammainc_P(const number_t s, const number_t x);
number_t num_gammainc_Q(const number_t s, const number_t x);
number_t num_ei(const number_t number);
number_t num_e1(const number_t number);
/** @} */

#endif
