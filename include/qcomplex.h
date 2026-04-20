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

#endif // QCOMPLEX_H