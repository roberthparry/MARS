#ifndef MFLOAT_H
#define MFLOAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include "qfloat.h"

/**
 * @file mfloat.h
 * @brief Opaque multiprecision floating-point type.
 *
 * A `mfloat_t` stores a sign, a binary exponent, and a normalised arbitrary-
 * precision mantissa. This first arithmetic layer mirrors the core arithmetic
 * surface of `qfloat_t`, but with heap-allocated opaque values and mutable
 * operations that update the first argument in place.
 */

/**
 * @brief Opaque multiprecision floating-point type.
 */
typedef struct _mfloat_t mfloat_t;

/** @name Constants
 * Process-lifetime convenience values owned by the mfloat subsystem. They must
 * not be modified or freed by callers.
 * @{
 */
extern const mfloat_t MF_ZERO_VALUE;
extern const mfloat_t MF_ONE_VALUE;
extern const mfloat_t MF_HALF_VALUE;
extern const mfloat_t MF_TENTH_VALUE;
extern const mfloat_t MF_TEN_VALUE;
extern const mfloat_t MF_PI_VALUE;
extern const mfloat_t MF_E_VALUE;
extern const mfloat_t MF_EULER_MASCHERONI_VALUE;
extern const mfloat_t MF_SQRT2_VALUE;
extern const mfloat_t MF_SQRT_PI_VALUE;
extern const mfloat_t MF_NAN_VALUE;
extern const mfloat_t MF_INF_VALUE;
extern const mfloat_t MF_NINF_VALUE;

#define MF_ZERO (&MF_ZERO_VALUE)
#define MF_ONE (&MF_ONE_VALUE)
#define MF_HALF (&MF_HALF_VALUE)
#define MF_TENTH (&MF_TENTH_VALUE)
#define MF_TEN (&MF_TEN_VALUE)
#define MF_PI (&MF_PI_VALUE)
#define MF_E (&MF_E_VALUE)
#define MF_EULER_MASCHERONI (&MF_EULER_MASCHERONI_VALUE)
#define MF_SQRT2 (&MF_SQRT2_VALUE)
#define MF_SQRT_PI (&MF_SQRT_PI_VALUE)
#define MF_NAN (&MF_NAN_VALUE)
#define MF_INF (&MF_INF_VALUE)
#define MF_NINF (&MF_NINF_VALUE)
/** @} */

/** @name Lifecycle
 * Constructors return newly allocated values or `NULL` on allocation failure.
 * `mf_clone()` returns a deep copy, `mf_clear()` resets an existing
 * value to canonical zero, and `mf_free()` releases owned storage.
 * @{
 */
mfloat_t *mf_new(void);
mfloat_t *mf_new_prec(size_t precision_bits);
mfloat_t *mf_create_long(long value);
mfloat_t *mf_create_double(double value);
mfloat_t *mf_create_qfloat(qfloat_t value);
mfloat_t *mf_create_string(const char *text);
mfloat_t *mf_pi(void);
mfloat_t *mf_e(void);
mfloat_t *mf_euler_mascheroni(void);
mfloat_t *mf_max(void);
mfloat_t *mf_clone(const mfloat_t *mfloat);
void mf_free(mfloat_t *mfloat);
void mf_clear(mfloat_t *mfloat);
/** @} */

/** @name Precision and setup
 * `mf_set_default_precision()` updates the constructor precision used by
 * `mf_new()`, `mf_create_string()`, and the named constant constructors.
 * `mf_set_precision()` updates the target working precision metadata for
 * future rounded operations on an existing object. `mf_set_long()` stores an
 * exact native integer value. `mf_set_string()` parses a decimal string into
 * the current precision.
 * @{
 */
int mf_set_default_precision(size_t precision_bits);
size_t mf_get_default_precision(void);
int mf_set_precision(mfloat_t *mfloat, size_t precision_bits);
size_t mf_get_precision(const mfloat_t *mfloat);
int mf_set_default_precision_digits(size_t significant_digits);
size_t mf_get_default_precision_digits(void);
int mf_set_precision_digits(mfloat_t *mfloat, size_t significant_digits);
size_t mf_get_precision_digits(const mfloat_t *mfloat);
int mf_set_long(mfloat_t *mfloat, long value);
int mf_set_double(mfloat_t *mfloat, double value);
int mf_set_qfloat(mfloat_t *mfloat, qfloat_t value);
int mf_set_string(mfloat_t *mfloat, const char *text);
char *mf_to_string(const mfloat_t *mfloat);
double mf_to_double(const mfloat_t *mfloat);
qfloat_t mf_to_qfloat(const mfloat_t *mfloat);
/** @} */

/** @name Formatted output
 * `mf_vsprintf()` implements `%mf` and `%MF` for `mfloat_t *` arguments.
 * `%mf` prints a human-friendly fixed-format decimal using the same
 * width/padding rules as `qfloat`. `%MF` prints scientific notation with an
 * uppercase exponent marker. Callers must pass `mfloat_t *` values, not
 * `mfloat_t` by value.
 *
 * Supported form:
 *
 *     %[flags][width][.precision]mf
 *     %[flags][width][.precision]MF
 *
 * Supported flags:
 *   - `-` : left-justify within the field width
 *   - `0` : zero-pad on the left when right-justified
 *   - `+` : always print a leading sign for positive values
 *   - space : print a leading space instead of `+` for positive values
 *   - `#` : keep the decimal point even when the fractional precision is zero
 *
 * Width:
 *   - a decimal field width works like `printf`
 *   - if the formatted value is shorter than `width`, it is padded
 *   - without `-`, padding is added on the left
 *   - with `-`, padding is added on the right
 *   - with `0`, left padding uses zeroes instead of spaces
 *
 * Precision:
 *   - for `%mf`, `.precision` sets the number of digits after the decimal point
 *   - for `%MF`, `.precision` is currently parsed but scientific output remains
 *     in the library's default pretty-scientific form
 *
 * Examples:
 *
 *     %8mf    -> right-justified in width 8
 *     %08mf   -> zero-padded in width 8
 *     %.4mf   -> exactly 4 digits after the decimal point
 *     %MF     -> scientific notation
 * @{
 */
int mf_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap);
int mf_sprintf(char *out, size_t out_size, const char *fmt, ...);
int mf_printf(const char *fmt, ...);
/** @} */

/** @name Queries
 * Introspection helpers for the current normalised representation.
 * @{
 */
bool mf_is_zero(const mfloat_t *mfloat);
short mf_get_sign(const mfloat_t *mfloat);
long mf_get_exponent2(const mfloat_t *mfloat);
size_t mf_get_mantissa_bits(const mfloat_t *mfloat);
bool mf_get_mantissa_u64(const mfloat_t *mfloat, uint64_t *out);
/** @} */

/** @name Comparisons
 * Comparison helpers over exact stored values.
 * @{
 */
bool mf_eq(const mfloat_t *a, const mfloat_t *b);
bool mf_lt(const mfloat_t *a, const mfloat_t *b);
bool mf_le(const mfloat_t *a, const mfloat_t *b);
bool mf_gt(const mfloat_t *a, const mfloat_t *b);
bool mf_ge(const mfloat_t *a, const mfloat_t *b);
int mf_cmp(const mfloat_t *a, const mfloat_t *b);
/** @} */

/** @name Basic arithmetic
 * In-place arithmetic on multiprecision floating-point values. Rounded
 * operations such as division and square root use the precision stored in the
 * destination object.
 * @{
 */
int mf_abs(mfloat_t *mfloat);
int mf_neg(mfloat_t *mfloat);
int mf_add(mfloat_t *mfloat, const mfloat_t *other);
int mf_add_long(mfloat_t *mfloat, long value);
int mf_sub(mfloat_t *mfloat, const mfloat_t *other);
int mf_mul(mfloat_t *mfloat, const mfloat_t *other);
int mf_mul_long(mfloat_t *mfloat, long value);
int mf_div(mfloat_t *mfloat, const mfloat_t *other);
int mf_inv(mfloat_t *mfloat);
int mf_pow_int(mfloat_t *mfloat, int exponent);
int mf_pow(mfloat_t *mfloat, const mfloat_t *exponent);
int mf_ldexp(mfloat_t *mfloat, int exponent2);
int mf_sqrt(mfloat_t *mfloat);
/** @} */

/** @name Extended math
 * The current implementation exposes the broader `qfloat`-style math surface
 * on `mfloat`, still using the library's in-place convention on the first
 * argument.
 * @{
 */
mfloat_t *mf_pow10(int exponent10);
int mf_sqr(mfloat_t *mfloat);
int mf_floor(mfloat_t *mfloat);
int mf_mul_pow10(mfloat_t *mfloat, int exponent10);
int mf_hypot(mfloat_t *mfloat, const mfloat_t *other);
int mf_exp(mfloat_t *mfloat);
int mf_log(mfloat_t *mfloat);
int mf_sin(mfloat_t *mfloat);
int mf_cos(mfloat_t *mfloat);
int mf_tan(mfloat_t *mfloat);
int mf_atan(mfloat_t *mfloat);
int mf_atan2(mfloat_t *mfloat, const mfloat_t *other);
int mf_asin(mfloat_t *mfloat);
int mf_acos(mfloat_t *mfloat);
int mf_sinh(mfloat_t *mfloat);
int mf_cosh(mfloat_t *mfloat);
int mf_tanh(mfloat_t *mfloat);
int mf_asinh(mfloat_t *mfloat);
int mf_acosh(mfloat_t *mfloat);
int mf_atanh(mfloat_t *mfloat);
int mf_gamma(mfloat_t *mfloat);
int mf_erf(mfloat_t *mfloat);
int mf_erfc(mfloat_t *mfloat);
int mf_erfinv(mfloat_t *mfloat);
int mf_erfcinv(mfloat_t *mfloat);
int mf_lgamma(mfloat_t *mfloat);
int mf_digamma(mfloat_t *mfloat);
int mf_trigamma(mfloat_t *mfloat);
int mf_tetragamma(mfloat_t *mfloat);
int mf_gammainv(mfloat_t *mfloat);
int mf_lambert_w0(mfloat_t *mfloat);
int mf_lambert_wm1(mfloat_t *mfloat);
int mf_beta(mfloat_t *mfloat, const mfloat_t *other);
int mf_logbeta(mfloat_t *mfloat, const mfloat_t *other);
int mf_binomial(mfloat_t *mfloat, const mfloat_t *other);
int mf_beta_pdf(mfloat_t *mfloat, const mfloat_t *a, const mfloat_t *b);
int mf_logbeta_pdf(mfloat_t *mfloat, const mfloat_t *a, const mfloat_t *b);
int mf_normal_pdf(mfloat_t *mfloat);
int mf_normal_cdf(mfloat_t *mfloat);
int mf_normal_logpdf(mfloat_t *mfloat);
int mf_productlog(mfloat_t *mfloat);
int mf_gammainc_lower(mfloat_t *mfloat, const mfloat_t *other);
int mf_gammainc_upper(mfloat_t *mfloat, const mfloat_t *other);
int mf_gammainc_P(mfloat_t *mfloat, const mfloat_t *other);
int mf_gammainc_Q(mfloat_t *mfloat, const mfloat_t *other);
int mf_ei(mfloat_t *mfloat);
int mf_e1(mfloat_t *mfloat);
/** @} */

#endif
