#ifndef QCOMPLEX_H
#define QCOMPLEX_H

#include <stddef.h>

#include "qfloat.h"

/**
 * @brief Double-double complex number (qfloat_t real and imaginary parts)
 */
typedef struct {
    qfloat_t re; /**< Real part */
    qfloat_t im; /**< Imaginary part */
} qcomplex_t;

/**
 * @brief 0 + 0i constant
 */
extern const qcomplex_t QC_ZERO;

/**
 * @brief 1 + 0i constant
 */
extern const qcomplex_t QC_ONE;

/**
 * @brief Construct a qcomplex_t from real and imaginary parts.
 */
static inline qcomplex_t qc_make(qfloat_t re, qfloat_t im) {
    qcomplex_t z = { re, im };
    return z;
}

/**
 * @name Basic arithmetic
 * @{
 */
qcomplex_t qc_add(qcomplex_t a, qcomplex_t b);   /**< a + b */
qcomplex_t qc_sub(qcomplex_t a, qcomplex_t b);   /**< a - b */
qcomplex_t qc_mul(qcomplex_t a, qcomplex_t b);   /**< a * b */
qcomplex_t qc_div(qcomplex_t a, qcomplex_t b);   /**< a / b */
qcomplex_t qc_neg(qcomplex_t a);                 /**< -a */
qcomplex_t qc_conj(qcomplex_t a);                /**< conjugate(a) */
/** @} */

/**
 * @name Magnitude and argument
 * @{
 */
qfloat_t   qc_abs(qcomplex_t z);                 /**< |z| */
qfloat_t   qc_arg(qcomplex_t z);                 /**< arg(z) */
/** @} */

/**
 * @name Polar form
 * @{
 */
/** Construct z = r * exp(i*theta) from polar coordinates. */
qcomplex_t qc_from_polar(qfloat_t r, qfloat_t theta);
/** Decompose z into (r, theta) where r = |z| and theta = arg(z) in (-pi, pi]. */
void       qc_to_polar(qcomplex_t z, qfloat_t *r, qfloat_t *theta);
/** @} */

/**
 * @name Elementary functions
 * @{
 */
qcomplex_t qc_exp(qcomplex_t z);                 /**< exp(z) */
qcomplex_t qc_log(qcomplex_t z);                 /**< log(z) */
qcomplex_t qc_pow(qcomplex_t a, qcomplex_t b);   /**< a^b */
qcomplex_t qc_sqrt(qcomplex_t z);                /**< sqrt(z) */
/** @} */

/**
 * @name Trigonometric functions
 * @{
 */
qcomplex_t qc_sin(qcomplex_t z);                 /**< sin(z) */
qcomplex_t qc_cos(qcomplex_t z);                 /**< cos(z) */
qcomplex_t qc_tan(qcomplex_t z);                 /**< tan(z) */
qcomplex_t qc_asin(qcomplex_t z);                /**< asin(z) */
qcomplex_t qc_acos(qcomplex_t z);                /**< acos(z) */
qcomplex_t qc_atan(qcomplex_t z);                /**< atan(z) */
qcomplex_t qc_atan2(qcomplex_t y, qcomplex_t x); /**< atan2(y, x) */
/** @} */

/**
 * @name Hyperbolic functions
 * @{
 */
qcomplex_t qc_sinh(qcomplex_t z);                /**< sinh(z) */
qcomplex_t qc_cosh(qcomplex_t z);                /**< cosh(z) */
qcomplex_t qc_tanh(qcomplex_t z);                /**< tanh(z) */
qcomplex_t qc_asinh(qcomplex_t z);               /**< asinh(z) */
qcomplex_t qc_acosh(qcomplex_t z);               /**< acosh(z) */
qcomplex_t qc_atanh(qcomplex_t z);               /**< atanh(z) */
/** @} */

/**
 * @name Special functions
 * @{
 */
qcomplex_t qc_erf(qcomplex_t z);                 /**< error function */
qcomplex_t qc_erfc(qcomplex_t z);                /**< complementary error function */
qcomplex_t qc_erfinv(qcomplex_t z);              /**< inverse error function */
qcomplex_t qc_erfcinv(qcomplex_t z);             /**< inverse complementary error function */
qcomplex_t qc_gamma(qcomplex_t z);               /**< gamma function */
qcomplex_t qc_lgamma(qcomplex_t z);              /**< log gamma */
qcomplex_t qc_digamma(qcomplex_t z);             /**< digamma */
qcomplex_t qc_trigamma(qcomplex_t z);            /**< trigamma */
qcomplex_t qc_tetragamma(qcomplex_t z);          /**< tetragamma */
qcomplex_t qc_gammainv(qcomplex_t z);            /**< inverse gamma */
qcomplex_t qc_beta(qcomplex_t a, qcomplex_t b);  /**< beta function */
qcomplex_t qc_logbeta(qcomplex_t a, qcomplex_t b); /**< log beta */
qcomplex_t qc_binomial(qcomplex_t a, qcomplex_t b); /**< binomial coefficient */
qcomplex_t qc_beta_pdf(qcomplex_t x, qcomplex_t a, qcomplex_t b); /**< beta PDF */
qcomplex_t qc_logbeta_pdf(qcomplex_t x, qcomplex_t a, qcomplex_t b); /**< log beta PDF */
qcomplex_t qc_normal_pdf(qcomplex_t z);          /**< normal PDF */
qcomplex_t qc_normal_cdf(qcomplex_t z);          /**< normal CDF */
qcomplex_t qc_normal_logpdf(qcomplex_t z);       /**< normal log PDF */
qcomplex_t qc_productlog(qcomplex_t z);          /**< product log (Lambert W) */
qcomplex_t qc_gammainc_lower(qcomplex_t s, qcomplex_t x); /**< lower incomplete gamma */
qcomplex_t qc_gammainc_upper(qcomplex_t s, qcomplex_t x); /**< upper incomplete gamma */
qcomplex_t qc_gammainc_P(qcomplex_t s, qcomplex_t x);     /**< regularized lower gamma */
qcomplex_t qc_gammainc_Q(qcomplex_t s, qcomplex_t x);     /**< regularized upper gamma */
qcomplex_t qc_ei(qcomplex_t z);                  /**< exponential integral Ei */
qcomplex_t qc_e1(qcomplex_t z);                  /**< exponential integral E1 */
/** @} */

/**
 * @name Utility
 * @{
 */
qcomplex_t qc_ldexp(qcomplex_t z, int k);        /**< z * 2^k */
qcomplex_t qc_floor(qcomplex_t z);               /**< floor(z) */
qcomplex_t qc_hypot(qcomplex_t x, qcomplex_t y); /**< sqrt(|x|^2 + |y|^2) */
/** @} */

/**
 * @name Comparison
 * @{
 */
bool qc_eq(qcomplex_t a, qcomplex_t b);          /**< a == b */
bool qc_isnan(qcomplex_t z);                     /**< isnan(z) */
bool qc_isinf(qcomplex_t z);                     /**< isinf(z) */
bool qc_isposinf(qcomplex_t z);                  /**< isposinf(z) */
bool qc_isneginf(qcomplex_t z);                  /**< isneginf(z) */
/** @} */

/**
 * @brief Print qcomplex_t to string buffer.
 * @param z Complex number
 * @param out Output buffer
 * @param out_size Size of output buffer
 */
void qc_to_string(qcomplex_t z, char *out, size_t out_size);

/**
 * @brief Parse qcomplex_t from string.
 * @param s Input string (e.g. "3 + 4i", "2e-5 - 1.2e3i", "5i", "7", etc.)
 * @return Parsed qcomplex_t (NaN if parsing fails)
 */
qcomplex_t qc_from_string(const char *s);

/**
 * @brief Internal printf-style formatter with full qcomplex_t and qfloat_t support.
 *
 * qc_vsprintf handles all standard printf specifiers plus:
 *
 *   - %z : fixed-decimal complex  — formats as "a + bi" or "a - bi" using %q per part
 *   - %Z : scientific complex     — formats as "a + bi" or "a - bi" using %Q per part
 *   - %q : fixed-decimal qfloat_t  (same as qf_vsprintf)
 *   - %Q : scientific qfloat_t    (same as qf_vsprintf)
 *
 * Flags (+, space, -, 0, #), width, and precision are fully supported.
 * For %z/%Z, precision controls decimal places on each component; width
 * and alignment flags apply to the assembled "a ± bi" string.
 *
 * IMPORTANT: qcomplex_t and qfloat_t arguments are passed BY VALUE.
 *
 *     qcomplex_t z = qc_make(qf_from_double(3.0), qf_from_double(4.0));
 *     qc_printf("%z\n", z);          // "3 + 4i"
 *     qc_printf("%.4z\n", z);        // "3.0000 + 4.0000i"
 *     qc_printf("%Z\n", z);          // "3e+0 + 4e+0i"
 *
 * @param out      Output buffer (NULL for dry-run count).
 * @param out_size Size of output buffer.
 * @param fmt      Format string.
 * @param ap       Variadic argument list.
 * @return Number of characters written (excluding null terminator).
 */
int qc_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap);

/**
 * @brief printf-style formatter with full qcomplex_t and qfloat_t support.
 *
 * Extends snprintf() with %z, %Z (qcomplex_t) and %q, %Q (qfloat_t).
 * All other specifiers behave exactly like snprintf().
 *
 * @param out      Output buffer.
 * @param out_size Size of output buffer.
 * @param fmt      Format string.
 * @param ...      Additional arguments.
 * @return Number of characters written (excluding null terminator).
 */
int qc_sprintf(char *out, size_t out_size, const char *fmt, ...);

/**
 * @brief printf to stdout with full qcomplex_t and qfloat_t support.
 *
 * Extends printf() with %z, %Z (qcomplex_t) and %q, %Q (qfloat_t).
 * All other specifiers behave exactly like printf().
 *
 * @param fmt Format string.
 * @param ... Additional arguments.
 * @return Number of characters written.
 */
int qc_printf(const char *fmt, ...);

#endif // QCOMPLEX_H