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
 * ensuring a unique, normalised representation.
 */

#ifndef QFLOAT_H
#define QFLOAT_H

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct _string_t string_t;

/**
 * @struct qfloat_t
 * @brief Double-double precision floating-point number.
 *
 * A qfloat_t stores a number as:
 *
 *      value = hi + lo
 *
 * where `hi` contains the main value and `lo` contains the residual error.
 * This representation yields approximately 106 bits of precision.
 */
typedef struct {
    double hi; /**< Leading part (most significant 53 bits). */
    double lo; /**< Trailing part (error term, ~53 bits).    */
} qfloat_t;

#ifndef QF_SPLIT
#define QF_SPLIT 134217729.0
#endif

static inline void qf_inline_two_sum(double a, double b, double *s, double *e)
{
    double bb;

    *s = a + b;
    bb = *s - a;
    *e = (a - (*s - bb)) + (b - bb);
}

static inline void qf_inline_quick_two_sum(double a, double b, double *s, double *e)
{
    double t = a + b;

    *s = t;
    *e = b - (t - a);
}

static inline qfloat_t qf_inline_renorm(double hi, double lo)
{
    qfloat_t r;

    qf_inline_quick_two_sum(hi, lo, &r.hi, &r.lo);
    return r;
}

/* -------------------------------------------------------------------------
   Constants
   ------------------------------------------------------------------------- */

/** @brief Zero */
extern const qfloat_t QF_ZERO;

/** @brief One */
extern const qfloat_t QF_ONE;

/** @brief Negative one */
extern const qfloat_t QF_NEG_ONE;

/** @brief One half */
extern const qfloat_t QF_HALF;

/** @brief Two */
extern const qfloat_t QF_TWO;

/** @brief Ten */
extern const qfloat_t QF_TEN;

/** @brief NaN */
extern const qfloat_t QF_NAN;

/** @brief +∞ */
extern const qfloat_t QF_INF;

/** @brief -∞ */
extern const qfloat_t QF_NINF;

/** @brief Maximum possible qfloat_t value */
extern const qfloat_t QF_MAX;

/** @brief PI */
extern const qfloat_t QF_PI;

/** @brief 2*PI */
extern const qfloat_t QF_2PI;

/** @brief PI/2 */
extern const qfloat_t QF_PI_2;

/** @brief -PI/2 */
extern const qfloat_t QF_NEG_PI_2;

/** @brief PI/4 */
extern const qfloat_t QF_PI_4;

/** @brief 3*PI/4 */
extern const qfloat_t QF_3PI_4;

/** @brief PI/6 */
extern const qfloat_t QF_PI_6;

/** @brief PI/3 */
extern const qfloat_t QF_PI_3;

/** @brief 2/PI */
extern const qfloat_t QF_2_PI;

/** @brief e */
extern const qfloat_t QF_E;

/** @brief 1/e */
extern const qfloat_t QF_INV_E;

/** @brief -1/e */
extern const qfloat_t QF_NEG_INV_E;

/** @brief ln(2) */
extern const qfloat_t QF_LN2;

/** @brief ln(10) */
extern const qfloat_t QF_LN10;

/** @brief 1/ln(2) */
extern const qfloat_t QF_INVLN2;

/** @brief sqrt(0.5) */
extern const qfloat_t QF_SQRT_HALF;

/** @brief sqrt(2) */
extern const qfloat_t QF_SQRT2;

/** @brief sqrt(3) */
extern const qfloat_t QF_SQRT3;

/** @brief sqrt(2)/2 */
extern const qfloat_t QF_SQRT2_OVER_TWO;

/** @brief sqrt(3)/2 */
extern const qfloat_t QF_SQRT3_OVER_TWO;

/** @brief sqrt(2π) */
extern const qfloat_t QF_SQRT_2PI;

/** @brief sqrt(π) */
extern const qfloat_t QF_SQRT_PI;

/** @brief sqrt(π/2) */
extern const qfloat_t QF_SQRT_PI_OVER_TWO;

/** @brief sqrt(1/π) */
extern const qfloat_t QF_SQRT1ONPI;

/** @brief 2*sqrt(1/π) */
extern const qfloat_t QF_2_SQRTPI;

/** @brief -2*sqrt(1/π) */
extern const qfloat_t QF_NEG_TWO_OVER_SQRT_PI;

/** @brief 1/sqrt(2π) */
extern const qfloat_t QF_INV_SQRT_2PI;

/** @brief ln(sqrt(2π)) */
extern const qfloat_t QF_LOG_SQRT_2PI;

/** @brief ln(2π) */
extern const qfloat_t QF_LN_2PI;

/** @brief Euler–Mascheroni constant γ */
extern const qfloat_t QF_EULER_MASCHERONI;

/** @brief Golden ratio φ */
extern const qfloat_t QF_PHI;

/** @brief π² */
extern const qfloat_t QF_PI_SQUARED;

/** @brief (2π)³ */
extern const qfloat_t QF_2PI_CUBED;

/** @brief 3/2 */
extern const qfloat_t QF_ONE_AND_HALF;

/** @brief 1/3 */
extern const qfloat_t QF_ONE_THIRD;

/** @brief 1/4 */
extern const qfloat_t QF_QUARTER;

/** @brief 1/6 */
extern const qfloat_t QF_ONE_SIXTH;

/** @brief 1/8 */
extern const qfloat_t QF_ONE_EIGHTH;

/** @brief 1/10 */
extern const qfloat_t QF_ONE_TENTH;

/* -------------------------------------------------------------------------
   Constructors and conversions
   ------------------------------------------------------------------------- */

/**
 * @brief Construct a qfloat_t from a double.
 *
 * The result is exact because `hi = x` and `lo = 0`.
 *
 * @param x Input double.
 * @return qfloat_t representing x exactly.
 */
qfloat_t qf_from_double(double x);

/**
 * @brief Convert a qfloat_t to a double.
 *
 * Returns `hi + lo`, rounded to nearest double.
 *
 * @param x Input qfloat_t.
 * @return Nearest double approximation.
 */
double qf_to_double(qfloat_t x);

/**
 * @brief Parse a decimal string into a qfloat_t.
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
 * @return Parsed qfloat_t value.
 */
qfloat_t qf_from_string(const char *s);

/**
 * @brief Parse text into a qfloat_t.
 *
 * This is the string_t-based parser. qf_from_string() is the convenience
 * wrapper for callers with ordinary C string literals.
 *
 * @param text Input text.
 * @return Parsed qfloat_t value.
 */
qfloat_t qf_from_text(const string_t *text);

/**
 * @brief Convert a qfloat_t to normalised scientific notation.
 *
 * Produces exactly 32 significant digits in the form:
 *
 *      ±d.dddddddddddddddddddddddddddddd e±NN
 *
 * where the mantissa is in [1,10).
 *
 * @param x Input qfloat_t.
 * @return  Newly allocated string, or NULL on allocation failure.
 */
string_t *qf_to_text(qfloat_t x);

/**
 * @brief Convert a qfloat_t to normalised scientific notation.
 *
 * Convenience alias for qf_to_text(). The caller owns the returned string and
 * must release it with string_free().
 *
 * @param x Input qfloat_t.
 * @return  Newly allocated string, or NULL on allocation failure.
 */
string_t *qf_to_string(qfloat_t x);

/**
 * @brief Returns the absolute value of a qfloat_t (Windel's algorithm).
 *
 * @param x Input qfloat_t.
 * @return Absolute value of `x`.
 */
qfloat_t qf_abs(qfloat_t x);

/**
 * @param x Input qfloat_t.
 * @return -x (double-double precision).
 */
qfloat_t qf_neg(qfloat_t x);

/**
 * @brief Returns true if `a == b`.
 *
 * @param a Input qfloat_t.
 * @param b Input qfloat_t.
 * @return True if `a == b`.
 */
bool qf_eq(qfloat_t a, qfloat_t b);

/**
 * @brief Returns true if `a < b`.
 *
 * @param a Input qfloat_t.
 * @param b Input qfloat_t.
 * @return True if `a < b`.
 */
bool qf_lt(qfloat_t a, qfloat_t b);

/**
 * @brief Returns true if `a <= b`.
 *
 * @param a Input qfloat_t.
 * @param b Input qfloat_t.
 * @return True if `a <= b`.
 */
bool qf_le(qfloat_t a, qfloat_t b);

/**
 * @brief Returns true if `a > b`.
 *
 * @param a Input qfloat_t.
 * @param b Input qfloat_t.
 * @return True if `a > b`.
 */
bool qf_gt(qfloat_t a, qfloat_t b);

/**
 * @brief  Returns true if `a >= b`.
 *
 * @param a Input qfloat_t.
 * @param b Input qfloat_t.
 * @return True if `a >= b`.
 */
bool qf_ge(qfloat_t a, qfloat_t b);

/**
 * @brief Compares two qfloat_t values.
 *
 * @param a Input qfloat_t.
 * @param b Input qfloat_t.
 * @return -1 if `a < b`, 0 if `a == b`, 1 if `a > b`.
 */
int qf_cmp(qfloat_t a, qfloat_t b);

/**
 * @brief Returns the sign bit of a qfloat_t.
 *
 * @param x Input qfloat_t.
 * @return Sign bit of `x`.
 */
int qf_signbit(qfloat_t x);

/**
 * @brief Returns the base-2 exponent of a finite nonzero qfloat_t.
 *
 * This is analogous to `ilogb()` for qfloat_t values. For zero, NaN, and
 * infinities, this function returns 0.
 *
 * @param x Input qfloat_t.
 * @return Unbiased base-2 exponent, or 0 for zero/non-finite inputs.
 */
long qf_get_exponent2(qfloat_t x);

/**
 * @brief Square of a qfloat_t.
 *
 * @param x Input qfloat_t.
 * @return Square of `x`, exactly.
 */
qfloat_t qf_sqr(qfloat_t x);

/**
 * @brief Returns the floor of a qfloat_t.
 *
 * @param x Input qfloat_t.
 * @return Floor of `x`.
 */
qfloat_t qf_floor(qfloat_t x);

/**
 * @brief Multiply a qfloat_t by a power of 10.
 *
 * @param x   Input qfloat_t.
 * @param k   Power of 10 to multiply by.
 * @return `x * 10^k` exactly.
 */
qfloat_t qf_mul_pow10(qfloat_t x, int k);

/**
 * @brief Internal printf‑style formatter with full qfloat_t support.
 *
 * qf_vsprintf implements the %q and %Q format specifiers for qfloat_t values.
 *
 * IMPORTANT:
 *   qfloat_t arguments to %q and %Q are passed BY VALUE through the variadic
 *   argument list. Callers of qf_sprintf() and qf_printf() should therefore
 *   pass qfloat_t directly, not qfloat_t*.
 *
 *       qfloat_t x = qf_from_double(3.14);
 *       qf_printf("x = %q\n", x);   valid usage
 *
 * Supported qfloat_t‑specific format specifiers:
 *   - %Q : scientific notation (uppercase exponent)
 *   - %q : fixed‑format decimal with fallback to scientific
 *
 * All non‑qfloat_t specifiers are delegated to snprintf().
 *
 * @param out       Output buffer.
 * @param out_size  Size of output buffer.
 * @param fmt       Format string.
 * @param ap        Variadic argument list.
 * @return Number of characters written (excluding null terminator).
 */
int qf_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap);

/**
 * @brief Format text into a new string_t with full qfloat_t support.
 *
 * This is the string-owning counterpart to qf_vsprintf(). The caller owns the
 * returned string and must release it with string_free().
 *
 * @param fmt  Format string.
 * @param ap   Variadic argument list.
 * @return     Newly allocated formatted string, or NULL on error.
 */
string_t *qf_vsprintf_text(const char *fmt, va_list ap);

/**
 * @brief Print formatted text with full qfloat_t support.
 *
 * qf_sprintf extends snprintf() with two qfloat_t‑specific format specifiers:
 *
 *   - %Q : scientific notation (uppercase exponent)
 *   - %q : fixed‑format decimal with fallback to scientific
 *
 * IMPORTANT:
 *   qfloat_t arguments to %q and %Q MUST be passed by value.
 *
 *       qfloat_t x = qf_from_double(3.14);
 *       qf_sprintf(buf, n, "x = %q", x);   valid usage
 *
 * All non‑qfloat_t specifiers behave exactly like snprintf().
 *
 * @param out       Output buffer.
 * @param out_size  Size of output buffer.
 * @param fmt       Format string.
 * @param ...       Additional arguments.
 * @return Number of characters written (excluding null terminator).
 */
int qf_sprintf(char *out, size_t out_size, const char *fmt, ...);

/**
 * @brief Format text into a new string_t with full qfloat_t support.
 *
 * This is the string-owning counterpart to qf_sprintf(). The caller owns the
 * returned string and must release it with string_free().
 *
 * @param fmt  Format string.
 * @param ...  Additional arguments.
 * @return     Newly allocated formatted string, or NULL on error.
 */
string_t *qf_sprintf_text(const char *fmt, ...);

/**
 * @brief Print formatted text with full qfloat_t support.
 *
 * qf_printf behaves like printf(), but adds support for:
 *
 *   - %Q : scientific notation (uppercase exponent)
 *   - %q : fixed‑format decimal with fallback to scientific
 *
 * IMPORTANT:
 *   qfloat_t arguments to %q and %Q MUST be passed by value.
 *
 *       qfloat_t x = qf_from_double(3.14159);
 *       qf_printf("x = %Q\n", x);   valid usage
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
 * @brief Add two qfloat_t values.
 *
 * Uses error-free transformations (TwoSum + renormalisation).
 *
 * @param a First operand.
 * @param b Second operand.
 * @return a + b (double‑double precision).
 */
#ifndef MARS_QFLOAT_IMPLEMENTATION
static inline qfloat_t qf_add(qfloat_t a, qfloat_t b)
{
    double s;
    double e1;
    double e2;

    qf_inline_two_sum(a.hi, b.hi, &s, &e1);
    e2 = a.lo + b.lo + e1;
    return qf_inline_renorm(s, e2);
}
#else
qfloat_t qf_add(qfloat_t a, qfloat_t b);
#endif

/**
 * @brief Add a double to a qfloat_t.
 *
 * @param x qfloat_t value.
 * @param y double value.
 * @return x + y (double‑double precision).
 */
qfloat_t qf_add_double(qfloat_t x, double y);

/**
 * @brief Subtract two qfloat_t values.
 *
 * @param a Minuend.
 * @param b Subtrahend.
 * @return a - b (double‑double precision).
 */
#ifndef MARS_QFLOAT_IMPLEMENTATION
static inline qfloat_t qf_sub(qfloat_t a, qfloat_t b)
{
    double s;
    double e1;
    double e2;

    qf_inline_two_sum(a.hi, -b.hi, &s, &e1);
    e2 = a.lo - b.lo + e1;
    return qf_inline_renorm(s, e2);
}
#else
qfloat_t qf_sub(qfloat_t a, qfloat_t b);
#endif

/**
 * @brief Multiply two qfloat_t values.
 *
 * Uses Dekker's TwoProd algorithm with correction terms.
 *
 * @param a First operand.
 * @param b Second operand.
 * @return a * b (double‑double precision).
 */
#ifndef MARS_QFLOAT_IMPLEMENTATION
static inline qfloat_t qf_mul(qfloat_t a, qfloat_t b)
{
    double hx;
    double tx;
    double hy;
    double ty;
    double C;
    double c;
    double d;
    double hi;
    double lo;

    C = QF_SPLIT * a.hi;
    hx = C - a.hi;
    hx = C - hx;
    tx = a.hi - hx;

    d = QF_SPLIT * b.hi;
    hy = d - b.hi;
    hy = d - hy;
    ty = b.hi - hy;

    C = a.hi * b.hi;
    c = ((((hx * hy - C) + hx * ty) + tx * hy) + tx * ty) + (a.hi * b.lo + a.lo * b.hi) + (a.lo * b.lo);
    qf_inline_two_sum(C, c, &hi, &lo);
    return (qfloat_t){hi, lo};
}
#else
qfloat_t qf_mul(qfloat_t a, qfloat_t b);
#endif

/**
 * @brief Multiply a qfloat_t by a double.
 *
 * @param x   Input qfloat_t.
 * @param a   Input double.
 * @return `x * a` exactly.
 */
qfloat_t qf_mul_double(qfloat_t x, double a);

/**
 * @brief Divide two qfloat_t values.
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
#if !defined(MARS_QFLOAT_IMPLEMENTATION) && !defined(MARS_QFLOAT_NO_INLINE_DIV)
static inline qfloat_t qf_div(qfloat_t a, qfloat_t b)
{
    double b_hi;
    double q1;
    double q2;
    qfloat_t q1q;
    qfloat_t qb;
    qfloat_t r;
    qfloat_t q2q;

    if (b.hi == 0.0 && b.lo == 0.0)
        return QF_NAN;

    b_hi = b.hi;
    q1 = a.hi / b_hi;
    q1q = (qfloat_t){q1, 0.0};
    qb = qf_mul(q1q, b);
    r = qf_sub(a, qb);
    q2 = r.hi / b_hi;
    q2q = (qfloat_t){q2, 0.0};
    return qf_add(q1q, q2q);
}
#else
qfloat_t qf_div(qfloat_t a, qfloat_t b);
#endif

/**
 * @brief Raise a qfloat_t to an integer power.
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
 * inherent in the qfloat_t representation.
 *
 * @param x Base value.
 * @param n Integer exponent (may be negative).
 * @return @f$x^n@f$ computed in double‑double precision.
 */
qfloat_t qf_pow_int(qfloat_t x, int n);

/**
 * @brief Raise a qfloat_t to a qfloat_t power.
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
 * The NaN value is represented as a qfloat_t with both components set to
 * IEEE‑754 NaN.
 *
 * @param x Base value.
 * @param y Exponent value.
 * @return @f$x^y@f$ in double‑double precision, or qfloat_t‑NaN on domain error.
 */
qfloat_t qf_pow(qfloat_t x, qfloat_t y);

/* -------------------------------------------------------------------------
   Elementary functions
   ------------------------------------------------------------------------- */

qfloat_t qf_ldexp(qfloat_t x, int k);

/**
 * @brief Square root of a qfloat_t.
 *
 * Uses one Newton iteration:
 *
 *      y = sqrt(hi)
 *      y = 0.5 * (y + x/y)
 *
 * @param x Input value.
 * @return sqrt(x) (double‑double precision).
 */
qfloat_t qf_sqrt(qfloat_t x);

/**
 * @brief Principal cube root of a non-negative qfloat_t.
 *
 * @param x Input value.
 * @return The principal cube root, or qfloat_t-NaN when @p x is negative.
 */
qfloat_t qf_cubrt(qfloat_t x);

/**
 * @brief Principal @p n-th root of a non-negative qfloat_t.
 *
 * @param x Input value.
 * @param n Integer root order, which must be greater than one.
 * @return The principal root, or qfloat_t-NaN on a domain error.
 */
qfloat_t qf_root(qfloat_t x, unsigned int n);

/**
 * @brief Natural exponential function.
 *
 * Uses a 40‑term Taylor series, sufficient for full double‑double precision.
 *
 * @param x Input value.
 * @return exp(x) (double‑double precision).
 */
qfloat_t qf_exp(qfloat_t x);

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
qfloat_t qf_log(qfloat_t x);

/**
 * @brief Natural-logarithm shorthand.
 *
 * @param x Input value (must be > 0).
 * @return The same value as qf_log(x).
 */
qfloat_t qf_ln(qfloat_t x);

/**
 * @brief Common logarithm.
 *
 * @param x Input value (must be > 0).
 * @return log10(x) (double-double precision).
 */
qfloat_t qf_log10(qfloat_t x);

/**
 * @brief Common-logarithm shorthand.
 *
 * @param x Input value (must be > 0).
 * @return The same value as qf_log10(x).
 */
qfloat_t qf_lg(qfloat_t x);

/**
 * @brief Test whether a qfloat_t is a NaN value.
 *
 * A qfloat_t NaN is represented by setting both components (`hi` and `lo`)
 * to IEEE‑754 NaN. This function returns true if either component is NaN.
 *
 * The test is performed using the property that NaN is the only value
 * that does not compare equal to itself.
 *
 * @param x Input qfloat_t.
 * @return Non‑zero if x is NaN, zero otherwise.
 */
bool qf_isnan(qfloat_t x);

/**
 * @brief Test whether a qfloat_t is a positive infinity value.
 *
 * A qfloat_t positive infinity is represented by setting the high component
 * (`hi`) to IEEE-754 positive infinity, and the low component (`lo`) to zero.
 *
 * @param x Input qfloat_t.
 * @return Non-zero if x is positive infinity, zero otherwise.
 */
bool qf_isposinf(qfloat_t x);

/**
 * @brief Test whether a qfloat_t is a negative infinity value.
 *
 * A qfloat_t negative infinity is represented by setting the high component
 * (`hi`) to IEEE-754 negative infinity, and the low component (`lo`) to zero.
 *
 * @param x Input qfloat_t.
 * @return Non-zero if x is negative infinity, zero otherwise.
 */
bool qf_isneginf(qfloat_t x);

bool qf_isinf(qfloat_t x);

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
qfloat_t qf_pow10(int n);

/**
 * @brief High-precision hypotenuse.
 *
 * Computes sqrt(x^2 + y^2) to full quad-double precision.
 *
 * @param x First operand.
 * @param y Second operand.
 * @return sqrt(x^2 + y^2) as a quad-double.
 */
qfloat_t qf_hypot(qfloat_t x, qfloat_t y);

/**
 * @brief High‑precision sine.
 *
 * Computes sin(x) to full quad‑double precision.
 *
 * @param x  Angle in radians.
 * @return   sin(x) as a quad‑double.
 */
qfloat_t qf_sin(qfloat_t x);

/**
 * @brief High‑precision cosine.
 *
 * Computes cos(x) to full quad‑double precision.
 *
 * @param x  Angle in radians.
 * @return   cos(x) as a quad‑double.
 */
qfloat_t qf_cos(qfloat_t x);

/**
 * @brief High‑precision tangent.
 *
 * Computes tan(x) = sin(x) / cos(x) to full quad‑double precision.
 * Returns NaN at points where the tangent function is undefined.
 *
 * @param x  Angle in radians.
 * @return   tan(x) as a quad‑double, or NaN at a pole.
 */
qfloat_t qf_tan(qfloat_t x);

/**
 * @brief High-precision secant.
 *
 * Computes sec(x) = 1 / cos(x) to full quad-double precision.
 *
 * @param x Angle in radians.
 * @return sec(x) as a quad-double, or NaN at a pole.
 */
qfloat_t qf_sec(qfloat_t x);

/**
 * @brief High-precision cosecant.
 *
 * Computes cosec(x) = 1 / sin(x) to full quad-double precision.
 *
 * @param x Angle in radians.
 * @return cosec(x) as a quad-double, or NaN at a pole.
 */
qfloat_t qf_cosec(qfloat_t x);

/**
 * @brief High-precision cotangent.
 *
 * Computes cot(x) = 1 / tan(x) to full quad-double precision.
 *
 * @param x Angle in radians.
 * @return cot(x) as a quad-double, or NaN at a pole.
 */
qfloat_t qf_cot(qfloat_t x);
qfloat_t qf_versin(qfloat_t x);
qfloat_t qf_vercos(qfloat_t x);
qfloat_t qf_coversin(qfloat_t x);
qfloat_t qf_covercos(qfloat_t x);
qfloat_t qf_haversin(qfloat_t x);
qfloat_t qf_havercos(qfloat_t x);
qfloat_t qf_hacoversin(qfloat_t x);
qfloat_t qf_hacovercos(qfloat_t x);

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
qfloat_t qf_atan(qfloat_t x);

/**
 * @brief Inverse tangent function of two arguments.
 *
 * Computes atan2(y, x) to full quad‑double precision.
 *
 * @param y First argument.
 * @param x Second argument.
 * @return atan2(y, x) (double‑double precision).
 */
qfloat_t qf_atan2(qfloat_t y, qfloat_t x);

/**
 * @brief High-precision inverse sine.
 *
 * Computes asin(x) to full quad-double precision using a 40-term Taylor
 * series and the identity asin(x) = atan(sqrt(1 - x^2) + x) / sqrt(1 - x^2) .
 *
 * @param x Input value.
 * @return asin(x) (double-double precision).
 */
qfloat_t qf_asin(qfloat_t x);

/**
 * @brief High-precision inverse cosine.
 *
 * computes acos(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return acos(x) (double-double precision).
 */
qfloat_t qf_acos(qfloat_t x);

/**
 * @brief High-precision inverse secant.
 *
 * Computes asec(x) = acos(1 / x) to full quad-double precision.
 *
 * @param x Input value.
 * @return asec(x) (double-double precision).
 */
qfloat_t qf_asec(qfloat_t x);

/**
 * @brief High-precision inverse cosecant.
 *
 * Computes acosec(x) = asin(1 / x) to full quad-double precision.
 *
 * @param x Input value.
 * @return acosec(x) (double-double precision).
 */
qfloat_t qf_acosec(qfloat_t x);

/**
 * @brief High-precision inverse cotangent.
 *
 * Computes acot(x) with the principal real branch in [0, pi].
 *
 * @param x Input value.
 * @return acot(x) (double-double precision).
 */
qfloat_t qf_acot(qfloat_t x);
qfloat_t qf_arcversin(qfloat_t x);
qfloat_t qf_arcvercos(qfloat_t x);
qfloat_t qf_arccoversin(qfloat_t x);
qfloat_t qf_arccovercos(qfloat_t x);
qfloat_t qf_archaversin(qfloat_t x);
qfloat_t qf_archavercos(qfloat_t x);
qfloat_t qf_archacoversin(qfloat_t x);
qfloat_t qf_archacovercos(qfloat_t x);

/**
 * @brief High-precision hyperbolic sine.
 *
 * computes sinh(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return sinh(x) (double-double precision).
 */
qfloat_t qf_sinh(qfloat_t x);

/**
 * @brief Hyperbolic cosine function.
 *
 * Computes cosh(x) to full quad‑double precision.
 *
 * @param x Input value.
 * @return cosh(x) (double‑double precision).
 */
qfloat_t qf_cosh(qfloat_t x);

/**
 * @brief High-precision hyperbolic tangent function.
 *
 * computes tanh(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return tanh(x) (double-double precision).
 */
qfloat_t qf_tanh(qfloat_t x);

/**
 * @brief High-precision hyperbolic secant.
 *
 * Computes sech(x) = 1 / cosh(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return sech(x) (double-double precision).
 */
qfloat_t qf_sech(qfloat_t x);

/**
 * @brief High-precision hyperbolic cosecant.
 *
 * Computes cosech(x) = 1 / sinh(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return cosech(x) (double-double precision).
 */
qfloat_t qf_cosech(qfloat_t x);

/**
 * @brief High-precision hyperbolic cotangent.
 *
 * Computes coth(x) = 1 / tanh(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return coth(x) (double-double precision).
 */
qfloat_t qf_coth(qfloat_t x);

/**
 * @brief High-precision inverse hyperbolic sine.
 *
 * computes asinh(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return asinh(x) (double-double precision).
 */
qfloat_t qf_asinh(qfloat_t x);

/**
 * @brief High-precision inverse hyperbolic cosine.
 *
 * computes acosh(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return acosh(x) (double-double precision).
 */
qfloat_t qf_acosh(qfloat_t x);

/**
 * @brief Inverse hyperbolic tangent function.
 *
 * computes atanh(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return atanh(x) (double-double precision).
 */
qfloat_t qf_atanh(qfloat_t x);

/**
 * @brief High-precision inverse hyperbolic secant.
 *
 * Computes asech(x) = acosh(1 / x) to full quad-double precision.
 *
 * @param x Input value.
 * @return asech(x) (double-double precision).
 */
qfloat_t qf_asech(qfloat_t x);

/**
 * @brief High-precision inverse hyperbolic cosecant.
 *
 * Computes acosech(x) = asinh(1 / x) to full quad-double precision.
 *
 * @param x Input value.
 * @return acosech(x) (double-double precision).
 */
qfloat_t qf_acosech(qfloat_t x);

/**
 * @brief High-precision inverse hyperbolic cotangent.
 *
 * Computes acoth(x) = atanh(1 / x) to full quad-double precision.
 *
 * @param x Input value.
 * @return acoth(x) (double-double precision).
 */
qfloat_t qf_acoth(qfloat_t x);

/**
 * @brief Compute the gamma function for a qfloat_t argument.
 *
 * @param x Input value.
 * @return gamma(x) (double-double precision).
 */
qfloat_t qf_gamma(qfloat_t x);

/**
 * @brief High-precision error function.
 *
 * computes erf(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return erf(x) (double-double precision).
 */
qfloat_t qf_erf(qfloat_t x);

/**
 * @brief Compute the complementary error function for a qfloat_t argument.
 *
 * @param x Input value.
 * @return erfc(x) (double-double precision).
 */
qfloat_t qf_erfc(qfloat_t x);

/**
 * @brief High-precision inverse error function.
 *
 * computes erfinv(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return erfinv(x) (double-double precision).
 */
qfloat_t qf_erfinv(qfloat_t x);

/**
 * @brief Compute the inverse of the complementary error function.
 *
 * computes erfcinv(x) to full quad-double precision.
 *
 * @param x Input value.
 * @return erfcinv(x) (double-double precision).
 */
qfloat_t qf_erfcinv(qfloat_t x);

/**
 * @brief Compute the natural logarithm of the absolute value of the gamma function
 * for a qfloat_t argument.
 *
 * @param x Input value.
 * @return log(|gamma(x)|) (double-double precision).
 */
qfloat_t qf_lgamma(qfloat_t x);

/**
 * @brief Compute the digamma function for a qfloat_t argument.
 *
 * @param x Input value.
 * @return digamma(x) (double-double precision).
 */
qfloat_t qf_digamma(qfloat_t x);

/**
 * @brief Compute the trigamma function (psi'(x), the first derivative of
 *        digamma) for a qfloat_t argument.
 *
 * @param x Input value. Must not be a non-positive integer (pole).
 * @return trigamma(x) (double-double precision).
 */
qfloat_t qf_trigamma(qfloat_t x);

/**
 * @brief Compute the tetragamma function ψ''(x) (second derivative of digamma).
 *
 * Uses the asymptotic Bernoulli series for x > 20, with recurrence and
 * reflection to extend to all non-integer x.  Full qfloat_t precision.
 *
 * @param x Input value. Must not be a non-positive integer (pole).
 * @return tetragamma(x) (double-double precision).
 */
qfloat_t qf_tetragamma(qfloat_t x);

/**
 * @brief Compute the polygamma function ψ⁽ⁿ⁾(x).
 *
 * Orders 0, 1, and 2 dispatch to qf_digamma(), qf_trigamma(), and
 * qf_tetragamma(). Higher orders use recurrence and a Bernoulli asymptotic
 * expansion for non-pole positive arguments.
 *
 * @param order Derivative order n.
 * @param x Input value. Must not be a non-positive integer pole.
 * @return ψ⁽ⁿ⁾(x) (double-double precision).
 */
qfloat_t qf_polygamma(unsigned int order, qfloat_t x);

/**
 * @brief Compute the Riemann zeta function by analytic continuation.
 *
 * @param x Argument of the zeta function. The value 1 is its simple pole.
 * @return ζ(x) at double-double precision, infinity at the pole, or NaN when undefined.
 */
qfloat_t qf_zeta(qfloat_t x);

/**
 * @brief Compute the Hurwitz zeta function by analytic continuation.
 *
 * The real backend accepts a strictly positive second argument. Use qcomplex
 * when the second argument lies outside that real principal domain.
 *
 * @param s Exponent argument. The value 1 is the function's simple pole.
 * @param a Strictly positive shift argument.
 * @return ζ(s, a) at double-double precision, infinity at the pole, or NaN when undefined.
 */
qfloat_t qf_zetah(qfloat_t s, qfloat_t a);

/**
 * @brief Compute the first derivative of the Hurwitz zeta function with respect to its exponent.
 *
 * @param s Exponent argument. The value 1 is a double pole of the derivative.
 * @param a Strictly positive shift argument.
 * @return ∂ζ(s, a)/∂s at double-double precision, or NaN when undefined.
 */
qfloat_t qf_zatahp(qfloat_t s, qfloat_t a);

/**
 * @brief Compute the first derivative of the Riemann zeta function.
 *
 * @param x Argument of the zeta derivative. The value 1 is a double pole.
 * @return ζ′(x) at double-double precision, negative infinity at the pole, or NaN when undefined.
 */
qfloat_t qf_zetap(qfloat_t x);

/**
 * @brief Compute the principal dilogarithm Li₂(x).
 *
 * Real-valued for x <= 1 on the principal branch. Arguments beyond the real
 * branch cut return NaN in qfloat; use qcomplex for complex branch values.
 *
 * @param x Input value.
 * @return Li₂(x) (double-double precision where covered).
 */
qfloat_t qf_dilog(qfloat_t x);

/**
 * @brief Compute the polylogarithm Li_s(x) for integer real orders.
 *
 * The order argument must be an integer-valued qfloat. Orders 0, 1, and 2 have
 * direct formulae; higher positive orders use the defining series in its
 * convergence disk.
 *
 * @param s Integer order.
 * @param x Input value.
 * @return Li_s(x), or NaN outside implemented coverage.
 */
qfloat_t qf_polylog(qfloat_t s, qfloat_t x);

/**
 * @brief Evaluate the harmonic polynomial @f$H_n(x)=\sum_{k=1}^{n}x^k/k@f$.
 *
 * @param degree Non-negative integer degree.
 * @param argument Real argument.
 * @return The finite logarithmic sum.
 */
qfloat_t qf_harmonic_poly(unsigned long degree, qfloat_t argument);

/**
 * @brief Compute Appell's hypergeometric function F1.
 *
 * Uses the defining double series in the bidisc |x| < 1, |y| < 1. Arguments
 * outside the implemented convergence region return NaN.
 *
 * @param a First numerator parameter.
 * @param b1 First Appell numerator parameter.
 * @param b2 Second Appell numerator parameter.
 * @param c Denominator parameter.
 * @param x First variable.
 * @param y Second variable.
 * @return F1(a; b1, b2; c; x, y), or NaN outside implemented coverage.
 */
qfloat_t qf_appell_f1(qfloat_t a, qfloat_t b1, qfloat_t b2, qfloat_t c, qfloat_t x, qfloat_t y);

/**
 * @brief Compute the Lauricella hypergeometric function F_D in n variables.
 *
 * The b and x arrays must each contain variable_count values.  The defining
 * multivariate series is used in the polydisc max |x_i| < 1.  Appell F1 is
 * the variable_count == 2 member of this family.
 */
qfloat_t qf_lauricella_f(qfloat_t a, const qfloat_t *b, qfloat_t c, const qfloat_t *x, size_t variable_count);

/**
 * @brief Compute the generalised hypergeometric function pFq.
 *
 * The numerator and denominator parameter arrays may be empty when their
 * corresponding count is zero.  The defining series is used wherever it
 * converges, including terminating series.
 */
qfloat_t qf_hypergeometric_pFq(const qfloat_t *upper, size_t upper_count, const qfloat_t *lower, size_t lower_count,
                               qfloat_t argument);

/**
 * @brief Compute the Legendre chi function chi_s(x) for integer real orders.
 *
 * Uses chi_s(x) = (Li_s(x) - Li_s(-x)) / 2. The order argument must be an
 * integer-valued qfloat.
 *
 * @param s Integer order.
 * @param x Input value.
 * @return chi_s(x), or NaN outside implemented coverage.
 */
qfloat_t qf_legendre_chi(qfloat_t s, qfloat_t x);

/** @brief Bessel function of the first kind J_order(argument), for real order. */
qfloat_t qf_bessel_j(qfloat_t order, qfloat_t argument);

/** @brief Bessel function of the second kind Y_order(argument), for real order. */
qfloat_t qf_bessel_y(qfloat_t order, qfloat_t argument);

/**
 * @brief Lower-case Lommel function s_(mu,nu)(argument), for real parameters.
 *
 * This evaluates the convergent defining series. Singular parameter pairs and
 * real-domain violations return NaN.
 */
qfloat_t qf_lommel_s(qfloat_t mu, qfloat_t nu, qfloat_t argument);

/**
 * @brief Compute the main branch of the inverse of the gamma function.
 *
 * @param y Input value.
 * @return gammainv(y) (double-double precision).
 */
qfloat_t qf_gammainv(qfloat_t y);

/**
 * @brief Compute the principal branch of the Lambert W function.
 *
 * @param x Input value.
 * @return W0(x) (double-double precision).
 */
qfloat_t qf_lambert_w0(qfloat_t x);

/**
 * @brief Compute the -1 branch of the Lambert W function.
 *
 * @param x Input value.
 * @return W_{-1}(x) (double-double precision).
 */
qfloat_t qf_lambert_wm1(qfloat_t x);

/**
 * @brief Compute the beta function for two qfloat_t arguments.
 *
 * @param a First argument.
 * @param b Second argument.
 * @return beta(a, b) (double-double precision).
 */
qfloat_t qf_beta(qfloat_t a, qfloat_t b);

/**
 * @brief Compute the logarithm of the beta function.
 *
 * @param a First argument.
 * @param b Second argument.
 * @return log(beta(a, b)) (double-double precision).
 */
qfloat_t qf_logbeta(qfloat_t a, qfloat_t b);

/**
 * @brief Compute the binomial coefficient for two qfloat_t arguments.
 * @param a First argument.
 * @param b Second argument.
 * @return binomial coefficient (double-double precision).
 */
qfloat_t qf_binomial(qfloat_t a, qfloat_t b);

/**
 * @brief Probability density function of the beta distribution.
 *
 * @param x Input value.
 * @param a First shape parameter.
 * @param b Second shape parameter.
 * @return f(x; a, b) (double-double precision).
 */
qfloat_t qf_beta_pdf(qfloat_t x, qfloat_t a, qfloat_t b);

/**
 * @brief Logarithm of the probability density function of the beta distribution.
 *
 * @param x Input value.
 * @param a First shape parameter.
 * @param b Second shape parameter.
 * @return log(f(x; a, b)) (double-double precision).
 */
qfloat_t qf_logbeta_pdf(qfloat_t x, qfloat_t a, qfloat_t b);

/**
 * @brief Probability density function of the standard normal distribution.
 *
 * @param x Input value.
 * @return φ(x) (double-double precision).
 */
qfloat_t qf_normal_pdf(qfloat_t x);

/**
 * @brief Cumulative distribution function of the standard normal distribution.
 * @param x Input value.
 * @return Φ(x) (double-double precision).
 */
qfloat_t qf_normal_cdf(qfloat_t x);

/**
 * @brief Logarithm of the probability density function of the standard normal distribution.
 *
 * @param x Input value.
 * @return log(φ(x)) (double-double precision).
 */
qfloat_t qf_normal_logpdf(qfloat_t x);

/**
 * @brief Compute the product log of a qfloat_t argument.
 *
 * @param x Input value.
 * @return Lambert W function of x (double-double precision).
 */
qfloat_t qf_productlog(qfloat_t x);

/**
 * @brief Lower incomplete gamma γ(s, x) = ∫_0^x t^{s-1} e^{-t} dt
 */
qfloat_t qf_gammainc_lower(qfloat_t s, qfloat_t x);

/**
 * @brief Upper incomplete gamma Γ(s, x) = ∫_x^∞ t^{s-1} e^{-t} dt
 */
qfloat_t qf_gammainc_upper(qfloat_t s, qfloat_t x);

/**
 * @brief Regularized lower incomplete gamma P(s, x) = γ(s, x) / Γ(s)
 */
qfloat_t qf_gammainc_P(qfloat_t s, qfloat_t x);

/**
 * @brief Regularized upper incomplete gamma Q(s, x) = Γ(s, x) / Γ(s)
 */
qfloat_t qf_gammainc_Q(qfloat_t s, qfloat_t x);

/**
 * @brief Exponential integral Ei(x) = -PV ∫_{-x}^{∞} (e^{-t} / t) dt
 *
 * Principal value on the real axis, with the usual branch cut on (-∞, 0].
 */
qfloat_t qf_Ei(qfloat_t x);

/**
 * @brief Exponential integral E1(x) = ∫_{x}^{∞} (e^{-t} / t) dt,  x > 0
 */
qfloat_t qf_E1(qfloat_t x);

#endif /* QFLOAT_H */
