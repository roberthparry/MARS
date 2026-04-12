/**
 * @file qfloat.h
 * @brief Double-double precision floating‑point type (≈106 bits, ~32 decimal digits).
 *
 * This module implements a "double‑double" floating‑point number, represented as
 * the unevaluated sum of two IEEE‑754 doubles:
 *
 *      x = hi + lo
 *
 * where:
 *   - `hi` stores the leading 53 bits of precision
 *   - `lo` stores the trailing error term
 *
 * Together they provide ~106 bits of precision (about 31–32 decimal digits).
 *
 * The implementation follows classic algorithms from:
 *   - Dekker (1971)
 *   - Knuth, TAOCP Vol. 2
 *   - Bailey et al., QD Library
 *
 * All operations maintain the invariant:
 *
 *      |lo| <= 0.5 * ulp(hi)
 *
 * ensuring a unique, normalized representation.
 */

#ifndef QFLOAT_H
#define QFLOAT_H

#include <stdbool.h>
#include <stdarg.h>

/**
 * @struct qfloat
 * @brief Double‑double precision floating‑point number.
 *
 * A qfloat stores a number as:
 *
 *      value = hi + lo
 *
 * where `hi` contains the main value and `lo` contains the residual error.
 * This representation yields approximately 106 bits of precision.
 */
typedef struct {
    double hi; /**< Leading part (most significant 53 bits). */
    double lo; /**< Trailing part (error term, ~53 bits).    */
} qfloat;


/* -------------------------------------------------------------------------
   Constants
   ------------------------------------------------------------------------- */

/// @brief NaN
extern const qfloat QF_NAN;

/// @brief +∞
extern const qfloat QF_INF;

/// @brief -∞
extern const qfloat QF_NINF;

/// @brief Maximum possible qfloat value
extern const qfloat QF_MAX;

/// @brief PI
extern const qfloat QF_PI;

/// @brief 2*PI
extern const qfloat QF_2PI;

/// @brief PI/2
extern const qfloat QF_PI_2;

/// @brief PI/4
extern const qfloat QF_PI_4;

/// @brief 3*PI/4
extern const qfloat QF_3PI_4;

/// @brief PI/6
extern const qfloat QF_PI_6;

/// @brief PI/3
extern const qfloat QF_PI_3;

/// @brief 2/PI
extern const qfloat QF_2_PI;

/// @brief e
extern const qfloat QF_E;

/// @brief 1/e
extern const qfloat QF_INV_E;

/// @brief ln(2)
extern const qfloat QF_LN2;

/// @brief 1/ln(2)
extern const qfloat QF_INVLN2;

/// @brief sqrt(0.5)
extern const qfloat QF_SQRT_HALF;

/// @brief sqrt(1/π)
extern const qfloat QF_SQRT1ONPI;

/// @brief 2*sqrt(1/π)
extern const qfloat QF_2_SQRTPI;

/// @brief ln(2*π)
extern const qfloat QF_LOG_SQRT_2PI;

/// @brief Euler–Mascheroni constant γ
extern const qfloat QF_EULER_MASCHERONI;

/// @brief 1 / sqrt(2π)
extern const qfloat QF_INV_SQRT_2PI;

/// @brief ln(2π)
extern const qfloat QF_LN_2PI;

/* -------------------------------------------------------------------------
   Constructors and conversions
   ------------------------------------------------------------------------- */

/**
 * @brief Construct a qfloat from a double.
 *
 * The result is exact because `hi = x` and `lo = 0`.
 *
 * @param x Input double.
 * @return qfloat representing x exactly.
 */
qfloat qf_from_double(double x);

/**
 * @brief Convert a qfloat to a double.
 *
 * Returns `hi + lo`, rounded to nearest double.
 *
 * @param x Input qfloat.
 * @return Nearest double approximation.
 */
double qf_to_double(qfloat x);

/**
 * @brief Parse a decimal string into a qfloat.
 *
 * Supported formats:
 *   - integer and fractional parts
 *   - optional leading sign
 *   - scientific notation (`e` or `E`)
 *
 * Parsing is performed using exact decimal accumulation and exponentiation
 * by squaring, ensuring full double‑double precision.
 *
 * @param s Null‑terminated decimal string.
 * @return Parsed qfloat value.
 */
qfloat qf_from_string(const char *s);

/**
 * @brief Convert a qfloat to normalized scientific notation.
 *
 * Produces exactly 32 significant digits in the form:
 *
 *      ±d.dddddddddddddddddddddddddddddd e±NN
 *
 * where the mantissa is in [1,10).
 *
 * @param x   Input qfloat.
 * @param buf Output buffer.
 * @param n   Size of output buffer.
 */
void qf_to_string(qfloat x, char *out, size_t out_size);

/**
 * @brief Returns the absolute value of a qfloat (Windel's algorithm).
 *
 * @param x Input qfloat.
 * @return Absolute value of `x`.
 */
qfloat qf_abs(qfloat x);

/**
 * @param x Input qfloat.
 * @return -x (double-double precision).
 */
qfloat qf_neg(qfloat x);

/**
 * @brief Returns true if `a == b`.
 *
 * @param a Input qfloat.
 * @param b Input qfloat.
 * @return True if `a == b`.
 */
bool qf_eq(qfloat a, qfloat b);

/**
 * @brief Returns true if `a < b`.
 *
 * @param a Input qfloat.
 * @param b Input qfloat.
 * @return True if `a < b`.
 */
bool qf_lt(qfloat a, qfloat b);

/**
 * @brief Returns true if `a <= b`.
 *
 * @param a Input qfloat.
 * @param b Input qfloat.
 * @return True if `a <= b`.
 */
bool qf_le(qfloat a, qfloat b);

/**
 * @brief Returns true if `a > b`.
 *
 * @param a Input qfloat.
 * @param b Input qfloat.
 * @return True if `a > b`.
 */
bool qf_gt(qfloat a, qfloat b);

/**
 * @brief  Returns true if `a >= b`.
 *
 * @param a Input qfloat.
 * @param b Input qfloat.
 * @return True if `a >= b`.
 */
bool qf_ge(qfloat a, qfloat b);

/**
 * @brief Compares two qfloats.
 *
 * @param a Input qfloat.
 * @param b Input qfloat.
 * @return -1 if `a < b`, 0 if `a == b`, 1 if `a > b`.
 */
int qf_cmp(qfloat a, qfloat b);

/**
 * @brief Returns the sign bit of a qfloat.
 *
 * @param x Input qfloat.
 * @return Sign bit of `x`.
 */
int qf_signbit(qfloat x);

/**
 * @brief Square of a qfloat.
 *
 * @param x Input qfloat.
 * @return Square of `x`, exactly.
 */
qfloat qf_sqr(qfloat x);

/**
 * @brief Returns the floor of a qfloat.
 *
 * @param x Input qfloat.
 * @return Floor of `x`.
 */
qfloat qf_floor(qfloat x);

/**
 * @brief Multiply a qfloat by a power of 10.
 *
 * @param x   Input qfloat.
 * @param k   Power of 10 to multiply by.
 * @return `x * 10^k` exactly.
 */
qfloat qf_mul_pow10(qfloat x, int k);

/**
 * @brief Internal printf‑style formatter with full qfloat support.
 *
 * qf_vsprintf implements the %q and %Q format specifiers for qfloat values.
 *
 * IMPORTANT:
 *   qfloat arguments to %q and %Q are passed BY VALUE through the variadic
 *   argument list. Callers of qf_sprintf() and qf_printf() should therefore
 *   pass qfloat directly, not qfloat*.
 *
 *       qfloat x = qf_from_double(3.14);
 *       qf_printf("x = %q\n", x);   // correct
 *
 * Supported qfloat‑specific format specifiers:
 *   - %Q : scientific notation (uppercase exponent)
 *   - %q : fixed‑format decimal with fallback to scientific
 *
 * All non‑qfloat specifiers are delegated to snprintf().
 *
 * @param out       Output buffer.
 * @param out_size  Size of output buffer.
 * @param fmt       Format string.
 * @param ap        Variadic argument list.
 * @return Number of characters written (excluding null terminator).
 */
int qf_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap);

/**
 * @brief Print formatted text with full qfloat support.
 *
 * qf_sprintf extends snprintf() with two qfloat‑specific format specifiers:
 *
 *   - %Q : scientific notation (uppercase exponent)
 *   - %q : fixed‑format decimal with fallback to scientific
 *
 * IMPORTANT:
 *   qfloat arguments to %q and %Q MUST be passed by value.
 *
 *       qfloat x = qf_from_double(3.14);
 *       qf_sprintf(buf, n, "x = %q", x);   // correct
 *
 * All non‑qfloat specifiers behave exactly like snprintf().
 *
 * @param out       Output buffer.
 * @param out_size  Size of output buffer.
 * @param fmt       Format string.
 * @param ...       Additional arguments.
 * @return Number of characters written (excluding null terminator).
 */
int qf_sprintf(char *out, size_t out_size, const char *fmt, ...);

/**
 * @brief Print formatted text with full qfloat support.
 *
 * qf_printf behaves like printf(), but adds support for:
 *
 *   - %Q : scientific notation (uppercase exponent)
 *   - %q : fixed‑format decimal with fallback to scientific
 *
 * IMPORTANT:
 *   qfloat arguments to %q and %Q MUST be passed by value.
 *
 *       qfloat x = qf_from_double(3.14159);
 *       qf_printf("x = %Q\n", x);   // correct
 *
 * All other format specifiers behave exactly like printf().
 *
 * @param fmt Format string.
 * @param ... Additional arguments.
 * @return Number of characters written.
 */
int qf_printf(const char *fmt, ...);

/* -------------------------------------------------------------------------
   Basic arithmetic
   ------------------------------------------------------------------------- */

/**
 * @brief Add two qfloats.
 *
 * Uses error‑free transformations (TwoSum + renormalization).
 *
 * @param a First operand.
 * @param b Second operand.
 * @return a + b (double‑double precision).
 */
qfloat qf_add(qfloat a, qfloat b);

/**
 * @brief Add a double to a qfloat.
 *
 * @param x qfloat value.
 * @param y double value.
 * @return x + y (double‑double precision).
 */
qfloat qf_add_double(qfloat x, double y);

/**
 * @brief Subtract two qfloats.
 *
 * @param a Minuend.
 * @param b Subtrahend.
 * @return a - b (double‑double precision).
 */
qfloat qf_sub(qfloat a, qfloat b);

/**
 * @brief Multiply two qfloats.
 *
 * Uses Dekker's TwoProd algorithm with correction terms.
 *
 * @param a First operand.
 * @param b Second operand.
 * @return a * b (double‑double precision).
 */
qfloat qf_mul(qfloat a, qfloat b);

/**
 * @brief Multiply a qfloat by a double.
 *
 * @param x   Input qfloat.
 * @param a   Input double.
 * @return `x * a` exactly.
 */
qfloat qf_mul_double(qfloat x, double a);

/**
 * @brief Divide two qfloats.
 *
 * Uses a two‑step Newton‑style quotient:
 *
 *      q1 = a.hi / b.hi
 *      r  = a - q1*b
 *      q2 = r.hi / b.hi
 *      return q1 + q2
 *
 * @param a Numerator.
 * @param b Denominator.
 * @return a / b (double‑double precision).
 */
qfloat qf_div(qfloat a, qfloat b);

/**
 * @brief Raise a qfloat to an integer power.
 *
 * Computes @f$ x^n @f$ using exponentiation‑by‑squaring, which provides
 * logarithmic complexity and full double‑double precision. Negative
 * exponents are supported and return:
 *
 *      x^n = 1 / x^{-n}
 *
 * Special cases:
 *   - @f$x^0 = 1@f$ for all @f$x \neq 0@f$
 *   - @f$0^n = 0@f$ for all @f$n > 0@f$
 *
 * No overflow, underflow, or domain errors are detected beyond those
 * inherent in the qfloat representation.
 *
 * @param x Base value.
 * @param n Integer exponent (may be negative).
 * @return @f$x^n@f$ computed in double‑double precision.
 */
qfloat qf_pow_int(qfloat x, int n);

/**
 * @brief Raise a qfloat to a qfloat power.
 *
 * Computes @f$ x^y @f$ using the identity:
 *
 *      x^y = exp( y * log(x) )
 *
 * Full double‑double precision is maintained through the use of
 * qf_log(), qf_mul(), and qf_exp().
 *
 * Domain rules (matching standard pow()):
 *   - If @f$x < 0@f$ and @f$y@f$ is not an integer, the result is NaN.
 *   - If @f$x = 0@f$ and @f$y \le 0@f$, the result is NaN.
 *   - If @f$x = 0@f$ and @f$y > 0@f$, the result is @f$0@f$.
 *
 * The NaN value is represented as a qfloat with both components set to
 * IEEE‑754 NaN.
 *
 * @param x Base value.
 * @param y Exponent value.
 * @return @f$x^y@f$ in double‑double precision, or qfloat‑NaN on domain error.
 */
qfloat qf_pow(qfloat x, qfloat y);

/* -------------------------------------------------------------------------
   Elementary functions
   ------------------------------------------------------------------------- */

qfloat qf_ldexp(qfloat x, int k);

/**
 * @brief Square root of a qfloat.
 *
 * Uses one Newton iteration:
 *
 *      y = sqrt(hi)
 *      y = 0.5 * (y + x/y)
 *
 * @param x Input value.
 * @return sqrt(x) (double‑double precision).
 */
qfloat qf_sqrt(qfloat x);

/**
 * @brief Natural exponential function.
 *
 * Uses a 40‑term Taylor series, sufficient for full double‑double precision.
 *
 * @param x Input value.
 * @return exp(x) (double‑double precision).
 */
qfloat qf_exp(qfloat x);

/**
 * @brief Natural logarithm.
 *
 * Uses Newton iteration:
 *
 *      y_{n+1} = y_n + (x - exp(y_n)) / exp(y_n)
 *
 * @param x Input value (must be > 0).
 * @return log(x) (double‑double precision).
 */
qfloat qf_log(qfloat x);

/**
 * @brief Test whether a qfloat is a NaN value.
 *
 * A qfloat NaN is represented by setting both components (`hi` and `lo`)
 * to IEEE‑754 NaN. This function returns true if either component is NaN.
 *
 * The test is performed using the property that NaN is the only value
 * that does not compare equal to itself.
 *
 * @param x Input qfloat.
 * @return Non‑zero if x is NaN, zero otherwise.
 */
bool qf_isnan(qfloat x);

/**
 * @brief Test whether a qfloat is a positive infinity value.
 *
 * A qfloat positive infinity is represented by setting the high component
 * (`hi`) to IEEE-754 positive infinity, and the low component (`lo`) to zero.
 *
 * @param x Input qfloat.
 * @return Non-zero if x is positive infinity, zero otherwise.
 */
bool qf_isposinf(qfloat x);

/**
 * @brief Test whether a qfloat is a negative infinity value.
 *
 * A qfloat negative infinity is represented by setting the high component
 * (`hi`) to IEEE-754 negative infinity, and the low component (`lo`) to zero.
 *
 * @param x Input qfloat.
 * @return Non-zero if x is negative infinity, zero otherwise.
 */
bool qf_isneginf(qfloat x);

bool qf_isinf(qfloat x);

/**
 * @brief Compute 10 raised to an integer power.
 *
 * Returns @f$10^n@f$ using qf_pow_int() for full double‑double precision.
 * This is a convenience wrapper used internally by formatting routines
 * and decimal conversion code.
 *
 * Special cases:
 *   - @f$10^0 = 1@f$
 *   - Negative exponents return @f$1 / 10^{-n}@f$
 *
 * @param n Integer exponent (may be negative).
 * @return @f$10^n@f$ in double‑double precision.
 */
qfloat qf_pow10(int n);

/**
 * @brief High-precision hypotenuse.
 *
 * Computes sqrt(x^2 + y^2) to full quad-double precision.
 *
 * @param x First operand.
 * @param y Second operand.
 * @return sqrt(x^2 + y^2) as a quad-double.
 */
qfloat qf_hypot(qfloat x, qfloat y);

/**
 * @brief High‑precision sine.
 *
 * Computes sin(x) to full quad‑double precision.
 *
 * @param x  Angle in radians.
 * @return   sin(x) as a quad‑double.
 */
qfloat qf_sin(qfloat x);

/**
 * @brief High‑precision cosine.
 *
 * Computes cos(x) to full quad‑double precision.
 *
 * @param x  Angle in radians.
 * @return   cos(x) as a quad‑double.
 */
qfloat qf_cos(qfloat x);

/**
 * @brief High‑precision tangent.
 *
 * Computes tan(x) = sin(x) / cos(x) to full quad‑double precision.
 * Returns NaN at points where the tangent function is undefined.
 *
 * @param x  Angle in radians.
 * @return   tan(x) as a quad‑double, or NaN at a pole.
 */
qfloat qf_tan(qfloat x);

/**
 * @brief Inverse tangent function.
 *
 * Computes atan(x) to full quad‑double precision using a 40‑term Taylor
 * series and the identity atan(x) = atan(x / sqrt(1 + x^2)) + atan(x
 * / sqrt(1 + x^2)) / (1 + x) .
 *
 * @param x Input value.
 * @return atan(x) (double‑double precision).
 */
qfloat qf_atan(qfloat x);

/**
 * @brief Inverse tangent function of two arguments.
 *
 * Computes atan2(y, x) to full quad‑double precision.
 *
 * @param y First argument.
 * @param x Second argument.
 * @return atan2(y, x) (double‑double precision).
 */
qfloat qf_atan2(qfloat y, qfloat x);

/**
 * @brief High-precision inverse sine.
 *
 * Computes asin(x) to full quad-double precision using a 40-term Taylor
 * series and the identity asin(x) = atan(sqrt(1 - x^2) + x) / sqrt(1 - x^2) .
 *
 * @param x Input value.
 * @return asin(x) (double-double precision).
 */
qfloat qf_asin(qfloat x);

/**
 * @brief High-precision inverse cosine.
 *
 * computes acos(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return acos(x) (double-double precision).
 */
qfloat qf_acos(qfloat x);

/**
 * @brief High-precision hyperbolic sine.
 *
 * computes sinh(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return sinh(x) (double-double precision).
 */
qfloat qf_sinh(qfloat x);

/**
 * @brief Hyperbolic cosine function.
 *
 * Computes cosh(x) to full quad‑double precision.
 *
 * @param x Input value.
 * @return cosh(x) (double‑double precision).
 */
qfloat qf_cosh(qfloat x);

/**
 * @brief High-precision hyperbolic tangent function.
 *
 * computes tanh(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return tanh(x) (double-double precision).
 */
qfloat qf_tanh(qfloat x);

/**
 * @brief High-precision inverse hyperbolic sine.
 *
 * computes asinh(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return asinh(x) (double-double precision).
 */
qfloat qf_asinh(qfloat x);

/**
 * @brief High-precision inverse hyperbolic cosine.
 *
 * computes acosh(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return acosh(x) (double-double precision).
 */
qfloat qf_acosh(qfloat x);

/**
 * @brief Inverse hyperbolic tangent function.
 *
 * computes atanh(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return atanh(x) (double-double precision).
 */
qfloat qf_atanh(qfloat x);

/**
 * @brief Compute the gamma function for a qfloat argument.
 *
 * @param x Input value.
 * @return gamma(x) (double-double precision).
 */
qfloat qf_gamma(qfloat x);

/**
 * @brief High-precision error function.
 *
 * computes erf(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return erf(x) (double-double precision).
 */
qfloat qf_erf(qfloat x);

/**
 * @brief Compute the complementary error function for a qfloat argument.
 *
 * @param x Input value.
 * @return erfc(x) (double-double precision).
 */
qfloat qf_erfc(qfloat x);

/**
 * @brief High-precision inverse error function.
 *
 * computes erfinv(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return erfinv(x) (double-double precision).
 */
qfloat qf_erfinv(qfloat x);

/**
 * @brief Compute the inverse of the complementary error function.
 *
 * computes erfcinv(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return erfcinv(x) (double-double precision).
 */
qfloat qf_erfcinv(qfloat x);

/**
 * @brief Compute the natural logarithm of the absolute value of the gamma function
 * for a qfloat argument.
 *
 * @param x Input value.
 * @return log(|gamma(x)|) (double-double precision).
 */
qfloat qf_lgamma(qfloat x);

/**
 * @brief Compute the digamma function for a qfloat argument.
 *
 * @param x Input value.
 * @return digamma(x) (double-double precision).
 */
qfloat qf_digamma(qfloat x);

/**
 * @brief Compute the trigamma function (psi'(x), the first derivative of
 *        digamma) for a qfloat argument.
 *
 * @param x Input value. Must not be a non-positive integer (pole).
 * @return trigamma(x) (double-double precision).
 */
qfloat qf_trigamma(qfloat x);

/**
 * @brief Compute the tetragamma function ψ''(x) (second derivative of digamma).
 *
 * Uses the asymptotic Bernoulli series for x > 20, with recurrence and
 * reflection to extend to all non-integer x.  Full qfloat precision.
 *
 * @param x Input value. Must not be a non-positive integer (pole).
 * @return tetragamma(x) (double-double precision).
 */
qfloat qf_tetragamma(qfloat x);

/**
 * @brief Compute the main branch of the inverse of the gamma function.
 *
 * @param y Input value.
 * @return gammainv(y) (double-double precision).
 */
qfloat qf_gammainv(qfloat y);

/**
 * @brief Compute the principal branch of the Lambert W function.
 *
 * @param x Input value.
 * @return W0(x) (double-double precision).
 */
qfloat qf_lambert_w0(qfloat x);

/**
 * @brief Compute the -1 branch of the Lambert W function.
 *
 * @param x Input value.
 * @return W_{-1}(x) (double-double precision).
 */
qfloat qf_lambert_wm1(qfloat x);

/**
 * @brief Compute the beta function for two qfloat arguments.
 *
 * @param a First argument.
 * @param b Second argument.
 * @return beta(a, b) (double-double precision).
 */
qfloat qf_beta(qfloat a, qfloat b);

/**
 * @brief Compute the logarithm of the beta function.
 *
 * @param a First argument.
 * @param b Second argument.
 * @return log(beta(a, b)) (double-double precision).
 */
qfloat qf_logbeta(qfloat a, qfloat b);

/**
 * @brief Compute the binomial coefficient for two qfloat arguments.
 * @param a First argument.
 * @param b Second argument.
 * @return binomial coefficient (double-double precision).
 */
qfloat qf_binomial(qfloat a, qfloat b);

/**
 * @brief Probability density function of the beta distribution.
 *
 * @param x Input value.
 * @param a First shape parameter.
 * @param b Second shape parameter.
 * @return f(x; a, b) (double-double precision).
 */
qfloat qf_beta_pdf(qfloat x, qfloat a, qfloat b);

/**
 * @brief Logarithm of the probability density function of the beta distribution.
 *
 * @param x Input value.
 * @param a First shape parameter.
 * @param b Second shape parameter.
 * @return log(f(x; a, b)) (double-double precision).
 */
qfloat qf_logbeta_pdf(qfloat x, qfloat a, qfloat b);

/**
 * @brief Probability density function of the standard normal distribution.
 *
 * @param x Input value.
 * @return φ(x) (double-double precision).
 */
qfloat qf_normal_pdf(qfloat x);

/**
 * @brief Cumulative distribution function of the standard normal distribution.
 * @param x Input value.
 * @return Φ(x) (double-double precision).
 */
qfloat qf_normal_cdf(qfloat x);

/**
 * @brief Logarithm of the probability density function of the standard normal distribution.
 *
 * @param x Input value.
 * @return log(φ(x)) (double-double precision).
 */
qfloat qf_normal_logpdf(qfloat x);

/**
 * @brief Compute the product log of a qfloat argument.
 *
 * @param x Input value.
 * @return Lambert W function of x (double-double precision).
 */
qfloat qf_productlog(qfloat x);

/**
 * @brief Lower incomplete gamma γ(s, x) = ∫_0^x t^{s-1} e^{-t} dt
 */
qfloat qf_gammainc_lower(qfloat s, qfloat x);

/**
 * @brief Upper incomplete gamma Γ(s, x) = ∫_x^∞ t^{s-1} e^{-t} dt
 */
qfloat qf_gammainc_upper(qfloat s, qfloat x);

/**
 * @brief Regularized lower incomplete gamma P(s, x) = γ(s, x) / Γ(s)
 */
qfloat qf_gammainc_P(qfloat s, qfloat x);

/**
 * @brief Regularized upper incomplete gamma Q(s, x) = Γ(s, x) / Γ(s)
 */
qfloat qf_gammainc_Q(qfloat s, qfloat x);

/**
 * @brief Exponential integral Ei(x) = -PV ∫_{-x}^{∞} (e^{-t} / t) dt
 *
 * Principal value on the real axis, with the usual branch cut on (-∞, 0].
 */
qfloat qf_ei(qfloat x);

/**
 * @brief Exponential integral E1(x) = ∫_{x}^{∞} (e^{-t} / t) dt,  x > 0
 */
qfloat qf_e1(qfloat x);


#endif /* QFLOAT_H */

