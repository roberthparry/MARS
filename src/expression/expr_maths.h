/**
 * @file expr_maths.h
 * @brief Private mathematical expression callbacks.
 *
 * Declares evaluation, derivative and supporting callbacks bound to mathematical node operations. Operator-table
 * construction and function implementations share this contract; application code uses constructors from
 * expression.h.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef EXPR_MATHS_H
#define EXPR_MATHS_H

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

int expr_number_to_polygamma_order(number_t value, unsigned int *order);

/* Evaluation: trigonometric. */
number_t eval_sin(expr_t *expr);
number_t eval_cos(expr_t *expr);
number_t eval_tan(expr_t *expr);
number_t eval_sec(expr_t *expr);
number_t eval_cosec(expr_t *expr);
number_t eval_cot(expr_t *expr);
number_t eval_versin(expr_t *expr);
number_t eval_vercos(expr_t *expr);
number_t eval_coversin(expr_t *expr);
number_t eval_covercos(expr_t *expr);
number_t eval_haversin(expr_t *expr);
number_t eval_havercos(expr_t *expr);
number_t eval_hacoversin(expr_t *expr);
number_t eval_hacovercos(expr_t *expr);

/* Evaluation: hyperbolic. */
number_t eval_sinh(expr_t *expr);
number_t eval_cosh(expr_t *expr);
number_t eval_tanh(expr_t *expr);
number_t eval_sech(expr_t *expr);
number_t eval_cosech(expr_t *expr);
number_t eval_coth(expr_t *expr);

/* Evaluation: inverse trigonometric and inverse hyperbolic. */
number_t eval_asin(expr_t *expr);
number_t eval_acos(expr_t *expr);
number_t eval_atan(expr_t *expr);
number_t eval_asec(expr_t *expr);
number_t eval_acosec(expr_t *expr);
number_t eval_acot(expr_t *expr);
number_t eval_arcversin(expr_t *expr);
number_t eval_arcvercos(expr_t *expr);
number_t eval_arccoversin(expr_t *expr);
number_t eval_arccovercos(expr_t *expr);
number_t eval_archaversin(expr_t *expr);
number_t eval_archavercos(expr_t *expr);
number_t eval_archacoversin(expr_t *expr);
number_t eval_archacovercos(expr_t *expr);
number_t eval_asinh(expr_t *expr);
number_t eval_acosh(expr_t *expr);
number_t eval_atanh(expr_t *expr);
number_t eval_asech(expr_t *expr);
number_t eval_acosech(expr_t *expr);
number_t eval_acoth(expr_t *expr);

/* Evaluation: elementary unary functions. */
number_t eval_exp(expr_t *expr);
number_t eval_log(expr_t *expr);
number_t eval_log10(expr_t *expr);
number_t eval_sqrt(expr_t *expr);
number_t eval_cubrt(expr_t *expr);
number_t eval_root(expr_t *expr);
number_t eval_floor(expr_t *expr);
number_t eval_ceil(expr_t *expr);
number_t eval_abs(expr_t *expr);
number_t eval_conj(expr_t *expr);

/* Evaluation: special functions. */
number_t eval_erf(expr_t *expr);
number_t eval_erfc(expr_t *expr);
number_t eval_lgamma(expr_t *expr);
number_t eval_erfinv(expr_t *expr);
number_t eval_erfcinv(expr_t *expr);
number_t eval_gamma(expr_t *expr);
number_t eval_digamma(expr_t *expr);
number_t eval_qdigamma(expr_t *expr);
number_t eval_trigamma(expr_t *expr);
number_t eval_polygamma(expr_t *expr);
number_t eval_zeta(expr_t *expr);
number_t eval_zetap(expr_t *expr);
number_t eval_zetah(expr_t *expr);
number_t eval_zatahp(expr_t *expr);
number_t eval_dilog(expr_t *expr);
number_t eval_polylog1(expr_t *expr);
number_t eval_polylog(expr_t *expr);
number_t eval_harmonic_poly(expr_t *expr);
number_t eval_lerch_phi(expr_t *expr);
number_t eval_lerch_phi_pack(expr_t *expr);
number_t eval_legendre_chi(expr_t *expr);
number_t eval_bessel_j(expr_t *expr);
number_t eval_bessel_y(expr_t *expr);
number_t eval_lommel_s(expr_t *expr);
number_t eval_lommel_s_pack(expr_t *expr);
number_t eval_appell_f1(expr_t *expr);
number_t eval_appell_f1_pack(expr_t *expr);
number_t eval_lauricella_f(expr_t *expr);
number_t eval_hypergeometric_pFq(expr_t *expr);
number_t eval_hypergeometric_pFq_pack(expr_t *expr);
number_t eval_gammainv(expr_t *expr);
number_t eval_lambert_w(expr_t *expr);
number_t eval_lambert_wn(expr_t *expr);
number_t eval_lambert_w0(expr_t *expr);
number_t eval_lambert_wm1(expr_t *expr);

/* Evaluation: distributions and multi-argument special functions. */
number_t eval_normal_pdf(expr_t *expr);
number_t eval_normal_cdf(expr_t *expr);
number_t eval_normal_logpdf(expr_t *expr);
number_t eval_Ei(expr_t *expr);
number_t eval_Li(expr_t *expr);
number_t eval_E1(expr_t *expr);
number_t eval_hypot(expr_t *expr);
number_t eval_beta(expr_t *expr);
number_t eval_logbeta(expr_t *expr);
number_t eval_gammainc_lower(expr_t *expr);
number_t eval_gammainc_upper(expr_t *expr);
number_t eval_gammainc_P(expr_t *expr);
number_t eval_gammainc_Q(expr_t *expr);
number_t eval_factorial(expr_t *expr);

/* Evaluation: integer and bitwise helpers. */
number_t eval_fibonacci(expr_t *expr);
number_t eval_partition(expr_t *expr);
number_t eval_isqrt(expr_t *expr);
number_t eval_gcd(expr_t *expr);
number_t eval_lcm(expr_t *expr);
number_t eval_mod(expr_t *expr);
number_t eval_modinv(expr_t *expr);
number_t eval_is_prime(expr_t *expr);
number_t eval_next_prime(expr_t *expr);
number_t eval_prev_prime(expr_t *expr);
number_t eval_bit_and(expr_t *expr);
number_t eval_bit_or(expr_t *expr);
number_t eval_bit_xor(expr_t *expr);
number_t eval_bit_not(expr_t *expr);
number_t eval_shl(expr_t *expr);
number_t eval_shr(expr_t *expr);
number_t eval_factors(expr_t *expr);
number_t eval_atan2(expr_t *expr);

/* Derivatives: trigonometric. */
expr_t *deriv_sin(expr_t *expr);
expr_t *deriv_cos(expr_t *expr);
expr_t *deriv_tan(expr_t *expr);
expr_t *deriv_sec(expr_t *expr);
expr_t *deriv_cosec(expr_t *expr);
expr_t *deriv_cot(expr_t *expr);
expr_t *deriv_versin(expr_t *expr);
expr_t *deriv_vercos(expr_t *expr);
expr_t *deriv_coversin(expr_t *expr);
expr_t *deriv_covercos(expr_t *expr);
expr_t *deriv_haversin(expr_t *expr);
expr_t *deriv_havercos(expr_t *expr);
expr_t *deriv_hacoversin(expr_t *expr);
expr_t *deriv_hacovercos(expr_t *expr);

/* Derivatives: hyperbolic. */
expr_t *deriv_sinh(expr_t *expr);
expr_t *deriv_cosh(expr_t *expr);
expr_t *deriv_tanh(expr_t *expr);
expr_t *deriv_sech(expr_t *expr);
expr_t *deriv_cosech(expr_t *expr);
expr_t *deriv_coth(expr_t *expr);

/* Derivatives: inverse trigonometric and inverse hyperbolic. */
expr_t *deriv_asin(expr_t *expr);
expr_t *deriv_acos(expr_t *expr);
expr_t *deriv_atan(expr_t *expr);
expr_t *deriv_asec(expr_t *expr);
expr_t *deriv_acosec(expr_t *expr);
expr_t *deriv_acot(expr_t *expr);
expr_t *deriv_arcversin(expr_t *expr);
expr_t *deriv_arcvercos(expr_t *expr);
expr_t *deriv_arccoversin(expr_t *expr);
expr_t *deriv_arccovercos(expr_t *expr);
expr_t *deriv_archaversin(expr_t *expr);
expr_t *deriv_archavercos(expr_t *expr);
expr_t *deriv_archacoversin(expr_t *expr);
expr_t *deriv_archacovercos(expr_t *expr);
expr_t *deriv_asinh(expr_t *expr);
expr_t *deriv_acosh(expr_t *expr);
expr_t *deriv_atanh(expr_t *expr);
expr_t *deriv_asech(expr_t *expr);
expr_t *deriv_acosech(expr_t *expr);
expr_t *deriv_acoth(expr_t *expr);

/* Derivatives: elementary unary functions. */
expr_t *deriv_exp(expr_t *expr);
expr_t *deriv_log(expr_t *expr);
expr_t *deriv_log10(expr_t *expr);
expr_t *deriv_sqrt(expr_t *expr);
expr_t *deriv_cubrt(expr_t *expr);
expr_t *deriv_root(expr_t *expr);
expr_t *deriv_floor(expr_t *expr);
expr_t *deriv_ceil(expr_t *expr);
expr_t *deriv_abs(expr_t *expr);
expr_t *deriv_conj(expr_t *expr);

/* Derivatives: special functions. */
expr_t *deriv_erf(expr_t *expr);
expr_t *deriv_erfc(expr_t *expr);
expr_t *deriv_lgamma(expr_t *expr);
expr_t *deriv_erfinv(expr_t *expr);
expr_t *deriv_erfcinv(expr_t *expr);
expr_t *deriv_gamma(expr_t *expr);
expr_t *deriv_digamma(expr_t *expr);
expr_t *deriv_qdigamma(expr_t *expr);
expr_t *deriv_trigamma(expr_t *expr);
expr_t *deriv_polygamma(expr_t *expr);
expr_t *deriv_zeta(expr_t *expr);
expr_t *deriv_zetap(expr_t *expr);
expr_t *deriv_zetah(expr_t *expr);
expr_t *deriv_zatahp(expr_t *expr);
expr_t *deriv_dilog(expr_t *expr);
expr_t *deriv_polylog1(expr_t *expr);
expr_t *deriv_polylog(expr_t *expr);
expr_t *integrate_polylog1(const expr_t *expr, const expr_t *wrt);
expr_t *deriv_lerch_phi(expr_t *expr);
expr_t *deriv_lerch_phi_pack(expr_t *expr);
expr_t *deriv_harmonic_poly(expr_t *expr);
expr_t *deriv_legendre_chi(expr_t *expr);
expr_t *deriv_bessel_j(expr_t *expr);
expr_t *deriv_bessel_y(expr_t *expr);
expr_t *deriv_lommel_s(expr_t *expr);
expr_t *deriv_lommel_s_pack(expr_t *expr);
expr_t *deriv_appell_f1(expr_t *expr);
expr_t *deriv_appell_f1_pack(expr_t *expr);
expr_t *deriv_lauricella_f(expr_t *expr);
expr_t *deriv_hypergeometric_pFq(expr_t *expr);
expr_t *deriv_hypergeometric_pFq_pack(expr_t *expr);
expr_t *deriv_gammainv(expr_t *expr);
expr_t *deriv_lambert_w(expr_t *expr);
expr_t *deriv_lambert_wn(expr_t *expr);
expr_t *deriv_lambert_w0(expr_t *expr);
expr_t *deriv_lambert_wm1(expr_t *expr);

bool expr_lommel_s_unpack(const expr_t *expr, const expr_t **mu, const expr_t **nu, const expr_t **argument);
bool expr_hypergeometric_pFq_unpack(const expr_t *expr, const expr_t ***upper, size_t *upper_count,
                                    const expr_t ***lower, size_t *lower_count, const expr_t **argument);
expr_t *expr_hypergeometric_pFq_from_args(size_t argument_count, expr_t *const *arguments);
expr_t *expr_lauricella_f_from_args(size_t argument_count, expr_t *const *arguments);

/* Derivatives: distributions and multi-argument special functions. */
expr_t *deriv_normal_pdf(expr_t *expr);
expr_t *deriv_normal_cdf(expr_t *expr);
expr_t *deriv_normal_logpdf(expr_t *expr);
expr_t *deriv_pdf(expr_t *expr);
expr_t *deriv_cdf(expr_t *expr);
expr_t *deriv_logpdf(expr_t *expr);
expr_t *deriv_Ei(expr_t *expr);
expr_t *deriv_Li(expr_t *expr);
expr_t *deriv_E1(expr_t *expr);
expr_t *deriv_hypot(expr_t *expr);
expr_t *deriv_beta(expr_t *expr);
expr_t *deriv_logbeta(expr_t *expr);
expr_t *deriv_gammainc_lower(expr_t *expr);
expr_t *deriv_gammainc_upper(expr_t *expr);
expr_t *deriv_gammainc_P(expr_t *expr);
expr_t *deriv_gammainc_Q(expr_t *expr);

/* Derivatives: not-differentiable and special binary cases. */
expr_t *deriv_not_differentiable(expr_t *expr);
expr_t *deriv_atan2(expr_t *expr);

expr_t *expr_lommel_s_argument_derivative_expansion(const expr_t *mu, const expr_t *nu, const expr_t *argument);

#endif /* EXPR_MATHS_H */
