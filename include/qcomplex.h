#ifndef QCOMPLEX_H
#define QCOMPLEX_H

#include <stddef.h>
#include <stdarg.h>

#include "qfloat.h"

typedef struct _string_t string_t;

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
 * @brief -1 + 0i constant
 */
extern const qcomplex_t QC_NEG_ONE;

/**
 * @brief 1/2 + 0i constant
 */
extern const qcomplex_t QC_HALF;

/**
 * @brief 1/4 + 0i constant
 */
extern const qcomplex_t QC_QUARTER;

/**
 * @brief 1/8 + 0i constant
 */
extern const qcomplex_t QC_ONE_EIGHTH;

/**
 * @brief 2 + 0i constant
 */
extern const qcomplex_t QC_TWO;

/**
 * @brief 0 + 1i constant
 */
extern const qcomplex_t QC_I;

/**
 * @brief NaN + NaNi constant
 */
extern const qcomplex_t QC_NAN;

/**
 * @brief +inf + 0i constant
 */
extern const qcomplex_t QC_INF;

/**
 * @brief -inf + 0i constant
 */
extern const qcomplex_t QC_NINF;

/**
 * @brief Maximum finite qfloat value on the real axis
 */
extern const qcomplex_t QC_MAX;

/**
 * @brief π + 0i constant
 */
extern const qcomplex_t QC_PI;

/**
 * @brief 2π + 0i constant
 */
extern const qcomplex_t QC_2PI;

/**
 * @brief π/2 + 0i constant
 */
extern const qcomplex_t QC_PI_2;

/**
 * @brief -π/2 + 0i constant
 */
extern const qcomplex_t QC_NEG_PI_2;

/**
 * @brief π/4 + 0i constant
 */
extern const qcomplex_t QC_PI_4;

/**
 * @brief 3π/4 + 0i constant
 */
extern const qcomplex_t QC_3PI_4;

/**
 * @brief π/6 + 0i constant
 */
extern const qcomplex_t QC_PI_6;

/**
 * @brief π/3 + 0i constant
 */
extern const qcomplex_t QC_PI_3;

/**
 * @brief 2/π + 0i constant
 */
extern const qcomplex_t QC_2_PI;

/**
 * @brief e + 0i constant
 */
extern const qcomplex_t QC_E;

/**
 * @brief 1/e + 0i constant
 */
extern const qcomplex_t QC_INV_E;

/**
 * @brief -1/e + 0i constant
 */
extern const qcomplex_t QC_NEG_INV_E;

/**
 * @brief ln(2) + 0i constant
 */
extern const qcomplex_t QC_LN2;

/**
 * @brief ln(10) + 0i constant
 */
extern const qcomplex_t QC_LN10;

/**
 * @brief 1/ln(2) + 0i constant
 */
extern const qcomplex_t QC_INVLN2;

/**
 * @brief sqrt(1/2) + 0i constant
 */
extern const qcomplex_t QC_SQRT_HALF;

/**
 * @brief sqrt(2) + 0i constant
 */
extern const qcomplex_t QC_SQRT2;

/**
 * @brief sqrt(3) + 0i constant
 */
extern const qcomplex_t QC_SQRT3;

/**
 * @brief sqrt(2)/2 + 0i constant
 */
extern const qcomplex_t QC_SQRT2_OVER_TWO;

/**
 * @brief sqrt(3)/2 + 0i constant
 */
extern const qcomplex_t QC_SQRT3_OVER_TWO;

/**
 * @brief sqrt(π) + 0i constant
 */
extern const qcomplex_t QC_SQRT_PI;

/**
 * @brief sqrt(1/π) + 0i constant
 */
extern const qcomplex_t QC_SQRT1ONPI;

/**
 * @brief 2/sqrt(π) + 0i constant
 */
extern const qcomplex_t QC_2_SQRTPI;

/**
 * @brief 1/sqrt(2π) + 0i constant
 */
extern const qcomplex_t QC_INV_SQRT_2PI;

/**
 * @brief log(sqrt(2π)) + 0i constant
 */
extern const qcomplex_t QC_LOG_SQRT_2PI;

/**
 * @brief ln(2π) + 0i constant
 */
extern const qcomplex_t QC_LN_2PI;

/**
 * @brief Euler-Mascheroni constant + 0i
 */
extern const qcomplex_t QC_EULER_MASCHERONI;

/**
 * @brief Construct a qcomplex_t from real and imaginary parts.
 */
static inline qcomplex_t qc_make(qfloat_t re, qfloat_t im) {
    qcomplex_t z = { re, im };
    return z;
}

/**
 * @brief Return the real component of a complex value.
 */
static inline qfloat_t qc_real(qcomplex_t z) {
    return z.re;
}

/**
 * @brief Return the imaginary component of a complex value.
 */
static inline qfloat_t qc_imag(qcomplex_t z) {
    return z.im;
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
qcomplex_t qc_log10(qcomplex_t z);               /**< log10(z) */
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
qcomplex_t qc_sec(qcomplex_t z);                 /**< sec(z) */
qcomplex_t qc_cosec(qcomplex_t z);               /**< cosec(z) */
qcomplex_t qc_cot(qcomplex_t z);                 /**< cot(z) */
qcomplex_t qc_versin(qcomplex_t z);              /**< versin(z) = 1 - cos(z) */
qcomplex_t qc_vercos(qcomplex_t z);              /**< vercos(z) = 1 + cos(z) */
qcomplex_t qc_coversin(qcomplex_t z);            /**< coversin(z) = 1 - sin(z) */
qcomplex_t qc_covercos(qcomplex_t z);            /**< covercos(z) = 1 + sin(z) */
qcomplex_t qc_haversin(qcomplex_t z);            /**< haversin(z) = (1 - cos(z)) / 2 */
qcomplex_t qc_havercos(qcomplex_t z);            /**< havercos(z) = (1 + cos(z)) / 2 */
qcomplex_t qc_hacoversin(qcomplex_t z);          /**< hacoversin(z) = (1 - sin(z)) / 2 */
qcomplex_t qc_hacovercos(qcomplex_t z);          /**< hacovercos(z) = (1 + sin(z)) / 2 */
qcomplex_t qc_asin(qcomplex_t z);                /**< asin(z) */
qcomplex_t qc_acos(qcomplex_t z);                /**< acos(z) */
qcomplex_t qc_atan(qcomplex_t z);                /**< atan(z) */
qcomplex_t qc_asec(qcomplex_t z);                /**< asec(z) */
qcomplex_t qc_acosec(qcomplex_t z);              /**< acosec(z) */
qcomplex_t qc_acot(qcomplex_t z);                /**< acot(z) */
qcomplex_t qc_arcversin(qcomplex_t z);           /**< arcversin(z) = acos(1 - z) */
qcomplex_t qc_arcvercos(qcomplex_t z);           /**< arcvercos(z) = acos(z - 1) */
qcomplex_t qc_arccoversin(qcomplex_t z);         /**< arccoversin(z) = asin(1 - z) */
qcomplex_t qc_arccovercos(qcomplex_t z);         /**< arccovercos(z) = asin(z - 1) */
qcomplex_t qc_archaversin(qcomplex_t z);         /**< archaversin(z) = acos(1 - 2z) */
qcomplex_t qc_archavercos(qcomplex_t z);         /**< archavercos(z) = acos(2z - 1) */
qcomplex_t qc_archacoversin(qcomplex_t z);       /**< archacoversin(z) = asin(1 - 2z) */
qcomplex_t qc_archacovercos(qcomplex_t z);       /**< archacovercos(z) = asin(2z - 1) */
qcomplex_t qc_atan2(qcomplex_t y, qcomplex_t x); /**< atan2(y, x) */
/** @} */

/**
 * @name Hyperbolic functions
 * @{
 */
qcomplex_t qc_sinh(qcomplex_t z);                /**< sinh(z) */
qcomplex_t qc_cosh(qcomplex_t z);                /**< cosh(z) */
qcomplex_t qc_tanh(qcomplex_t z);                /**< tanh(z) */
qcomplex_t qc_sech(qcomplex_t z);                /**< sech(z) */
qcomplex_t qc_cosech(qcomplex_t z);              /**< cosech(z) */
qcomplex_t qc_coth(qcomplex_t z);                /**< coth(z) */
qcomplex_t qc_asinh(qcomplex_t z);               /**< asinh(z) */
qcomplex_t qc_acosh(qcomplex_t z);               /**< acosh(z) */
qcomplex_t qc_atanh(qcomplex_t z);               /**< atanh(z) */
qcomplex_t qc_asech(qcomplex_t z);               /**< asech(z) */
qcomplex_t qc_acosech(qcomplex_t z);             /**< acosech(z) */
qcomplex_t qc_acoth(qcomplex_t z);               /**< acoth(z) */
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
qcomplex_t qc_polygamma(unsigned int order, qcomplex_t z); /**< polygamma ψ⁽ⁿ⁾ */
qcomplex_t qc_dilog(qcomplex_t z);                 /**< dilogarithm Li₂(z) */
qcomplex_t qc_polylog(qcomplex_t s, qcomplex_t z); /**< polylogarithm Li_s(z), integer s */
qcomplex_t qc_appell_f1(qcomplex_t a, qcomplex_t b1, qcomplex_t b2,
                        qcomplex_t c, qcomplex_t x, qcomplex_t y); /**< Appell F1(a;b1,b2;c;x,y) */
qcomplex_t qc_legendre_chi(qcomplex_t s, qcomplex_t z); /**< Legendre chi chi_s(z), integer s */
qcomplex_t qc_gammainv(qcomplex_t z);            /**< inverse gamma */
qcomplex_t qc_beta(qcomplex_t a, qcomplex_t b);  /**< beta function */
qcomplex_t qc_logbeta(qcomplex_t a, qcomplex_t b); /**< log beta */
qcomplex_t qc_binomial(qcomplex_t a, qcomplex_t b); /**< binomial coefficient */
qcomplex_t qc_beta_pdf(qcomplex_t x, qcomplex_t a, qcomplex_t b); /**< beta PDF */
qcomplex_t qc_logbeta_pdf(qcomplex_t x, qcomplex_t a, qcomplex_t b); /**< log beta PDF */
qcomplex_t qc_normal_pdf(qcomplex_t z);          /**< normal PDF */
qcomplex_t qc_normal_cdf(qcomplex_t z);          /**< normal CDF */
qcomplex_t qc_normal_logpdf(qcomplex_t z);       /**< normal log PDF */
qcomplex_t qc_lambert_wn(int branch, qcomplex_t z); /**< Lambert W integer branch */
qcomplex_t qc_lambert_wm1(qcomplex_t z);         /**< Lambert W branch -1 */
qcomplex_t qc_productlog(qcomplex_t z);          /**< product log (Lambert W principal branch) */
qcomplex_t qc_gammainc_lower(qcomplex_t s, qcomplex_t x); /**< lower incomplete gamma */
qcomplex_t qc_gammainc_upper(qcomplex_t s, qcomplex_t x); /**< upper incomplete gamma */
qcomplex_t qc_gammainc_P(qcomplex_t s, qcomplex_t x);     /**< regularised lower gamma */
qcomplex_t qc_gammainc_Q(qcomplex_t s, qcomplex_t x);     /**< regularised upper gamma */
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
 * @brief Convert qcomplex_t to a newly allocated string.
 * @param z Complex number.
 * @return  Newly allocated string, or NULL on allocation failure.
 */
string_t *qc_to_string(qcomplex_t z);

/**
 * @brief Convert qcomplex_t to a newly allocated string.
 *
 * Preferred spelling for string_t-returning APIs. qc_to_string() is retained
 * as a convenience alias.
 *
 * @param z Complex number.
 * @return  Newly allocated string, or NULL on allocation failure.
 */
string_t *qc_to_text(qcomplex_t z);

/**
 * @brief Parse qcomplex_t from string.
 * @param s Input string (e.g. "3 + 4i", "2e-5 - 1.2e3i", "5i", "7", etc.)
 * @return Parsed qcomplex_t (NaN if parsing fails)
 */
qcomplex_t qc_from_string(const char *s);

/**
 * @brief Parse qcomplex_t from text.
 *
 * This is the string_t-based parser. qc_from_string() is the convenience
 * wrapper for callers with ordinary C string literals.
 *
 * @param text Input text.
 * @return Parsed qcomplex_t (NaN if parsing fails).
 */
qcomplex_t qc_from_text(const string_t *text);

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
 *     qc_printf("%z\n", z);          yields "3 + 4i"
 *     qc_printf("%.4z\n", z);        yields "3.0000 + 4.0000i"
 *     qc_printf("%Z\n", z);          yields "3e+0 + 4e+0i"
 *
 * @param out      Output buffer (NULL for dry-run count).
 * @param out_size Size of output buffer.
 * @param fmt      Format string.
 * @param ap       Variadic argument list.
 * @return Number of characters written (excluding null terminator).
 */
int qc_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap);

/**
 * @brief Format text into a new string_t with full qcomplex_t support.
 *
 * This is the string-owning counterpart to qc_vsprintf(). The caller owns the
 * returned string and must release it with string_free().
 *
 * @param fmt  Format string.
 * @param ap   Variadic argument list.
 * @return     Newly allocated formatted string, or NULL on error.
 */
string_t *qc_vsprintf_text(const char *fmt, va_list ap);

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
 * @brief Format text into a new string_t with full qcomplex_t support.
 *
 * This is the string-owning counterpart to qc_sprintf(). The caller owns the
 * returned string and must release it with string_free().
 *
 * @param fmt  Format string.
 * @param ...  Additional arguments.
 * @return     Newly allocated formatted string, or NULL on error.
 */
string_t *qc_sprintf_text(const char *fmt, ...);

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

#endif /* QCOMPLEX_H */
