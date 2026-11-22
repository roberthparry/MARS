/**
 * @file expr_maths.h
 * @brief Private mathematical expression callbacks.
 *
 * Declares evaluation, derivative and supporting callbacks bound to mathematical node operations. Operator-table
 * construction and function implementations share this contract; application code uses constructors from
 * expr_t.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef EXPR_MATHS_H
#define EXPR_MATHS_H

#if !defined(MARS_EXPR_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_EXPR_INTERNAL_ACCESS."
#endif

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

/**
 * @brief Evaluate a packed Lambert application at the active numerical precision.
 * @param expression Borrowed application; ordinary numerical caches may be updated.
 * @return Owned value, released with num_destroy(), or NaN for invalid inputs or non-convergence.
 */
number_t lambert_eval(expr_t *expression);

/**
 * @brief Apply Lambert-sum continuous chain rules through contiguous relations.
 * @param expression Borrowed application, with fixed discrete parameters.
 * @return Owned derivative, released with expr_free(), or NaN for varying discrete parameters.
 */
expr_t *lambert_derivative(expr_t *expression);

/**
 * @brief Rebuild a Lambert application from its packed first five arguments and term count.
 * @param left Borrowed packed arguments.
 * @param right Borrowed count.
 * @return Owned retained-argument expression, released with expr_free(), or NULL on failure.
 */
expr_t *lambert_rebuild(const expr_t *left, const expr_t *right);

/**
 * @brief Accumulate analytic Lambert-sum reverse sensitivities.
 * @param expression Borrowed application.
 * @param out_bar Borrowed output sensitivity.
 * @param accumulate Callback borrowing each argument and sensitivity during its call.
 * @param context Borrowed callback context.
 * @return Zero on success or -1 on invalid input or callback failure; no ownership transfers.
 */
int lambert_reverse(const expr_t *expression, const number_t *out_bar,
                    expr_reverse_accumulate_fn accumulate, void *context);

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
number_t eval_legendre_chi(expr_t *expr);
number_t eval_bessel_J(expr_t *expr);
number_t eval_bessel_Y(expr_t *expr);
number_t eval_lommel_S(expr_t *expr);

/**
 * @brief Evaluate an Appell F1 node using its six packed arguments.
 * @param expr Borrowed F1 expression; its arguments are evaluated without consuming their graph.
 * @return Numerical result released with num_destroy(), or NAN for malformed arguments or numerical failure.
 */
number_t eval_appell_F1(expr_t *expr);

/**
 * @brief Evaluate a packed Appell F2, F3 or F4 node through the number layer.
 * @param expression Borrowed family node; its arguments and bindings remain owned by the caller.
 * @return Numerical result released with num_destroy(), or NAN for malformed arguments or numerical failure.
 */
number_t appell_eval(expr_t *expression);

number_t eval_lauricella_F(expr_t *expr);
number_t eval_hypergeometric_pFq(expr_t *expr);
number_t eval_gammainv(expr_t *expr);
number_t eval_lambert_w(expr_t *expr);
number_t eval_lambert_Wn(expr_t *expr);
number_t eval_lambert_W0(expr_t *expr);
number_t eval_lambert_Wm1(expr_t *expr);

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
expr_t *deriv_harmonic_poly(expr_t *expr);
expr_t *deriv_legendre_chi(expr_t *expr);
expr_t *deriv_bessel_J(expr_t *expr);
expr_t *deriv_bessel_Y(expr_t *expr);
expr_t *deriv_lommel_S(expr_t *expr);

/**
 * @brief Differentiate Appell F1 with respect to the active expression coordinate.
 * @param expr Borrowed F1 expression; its graph is not consumed or changed.
 * @return Owned derivative released with expr_free(), a NAN expression for invalid input, or NULL on allocation
 * failure. Unsupported parameter derivatives remain formal through the Lauricella implementation.
 */
expr_t *deriv_appell_F1(expr_t *expr);

/**
 * @brief Differentiate F2--F4 using contiguous relations and the active coordinate's chain rule.
 * @param expression Borrowed packed Appell node; its graph and binding roles remain unchanged.
 * @return Owned derivative released with expr_free(), a formal derivative for parameter dependence, a NAN
 * expression for invalid input, or NULL on allocation failure.
 */
expr_t *appell_derivative(expr_t *expression);

expr_t *deriv_lauricella_F(expr_t *expr);
expr_t *deriv_hypergeometric_pFq(expr_t *expr);
expr_t *deriv_gammainv(expr_t *expr);
expr_t *deriv_lambert_w(expr_t *expr);
expr_t *deriv_lambert_Wn(expr_t *expr);
expr_t *deriv_lambert_W0(expr_t *expr);
expr_t *deriv_lambert_Wm1(expr_t *expr);

bool expr_lommel_S_unpack(const expr_t *expr, const expr_t **mu, const expr_t **nu, const expr_t **argument);
bool expr_hypergeometric_pFq_unpack(const expr_t *expr, const expr_t ***upper, size_t *upper_count,
                                    const expr_t ***lower, size_t *lower_count, const expr_t **argument);
expr_t *expr_hypergeometric_pFq_from_args(size_t argument_count, expr_t *const *arguments);
expr_t *expr_lauricella_F_from_args(size_t argument_count, expr_t *const *arguments);

/**
 * @brief Accumulate Appell argument sensitivities using the native contiguous partial derivatives.
 * @param expression Borrowed packed Appell node, retained by the caller throughout the operation.
 * @param out_bar Borrowed incoming adjoint; never consumed or modified.
 * @param accumulate Borrowed callback receiving each argument and a temporary contribution; neither is transferred.
 * @param context Caller-owned state passed unchanged to accumulate.
 * @return Zero on success, or -1 for malformed input, unavailable partials or callback failure. Parameter
 * sensitivities are explicitly NAN; contribution pointers remain valid only during their callback.
 */
int appell_reverse(const expr_t *expression, const number_t *out_bar,
                    expr_reverse_accumulate_fn accumulate, void *context);

/**
 * @brief Rebuild an Appell F1 node from its existing parameter and variable packs.
 * @param params Borrowed parameter pack, retained by the returned expression.
 * @param vars Borrowed two-variable pack, retained by the returned expression.
 * @return Owned node released with expr_free(), or NULL for an absent pack or allocation failure.
 */
expr_t *expr_appell_F1_from_packs(const expr_t *params, const expr_t *vars);

/**
 * @brief Rebuild an Appell F2 node from its existing parameter and variable packs.
 * @param parameters Borrowed parameter pack, retained by the returned expression.
 * @param variables Borrowed two-variable pack, retained by the returned expression.
 * @return Owned node released with expr_free(), or NULL for an absent pack or allocation failure.
 */
expr_t *appell_F2_rebuild(const expr_t *parameters, const expr_t *variables);

/**
 * @brief Rebuild an Appell F3 node from its existing parameter and variable packs.
 * @param parameters Borrowed parameter pack, retained by the returned expression.
 * @param variables Borrowed two-variable pack, retained by the returned expression.
 * @return Owned node released with expr_free(), or NULL for an absent pack or allocation failure.
 */
expr_t *appell_F3_rebuild(const expr_t *parameters, const expr_t *variables);

/**
 * @brief Rebuild an Appell F4 node from its existing parameter and variable packs.
 * @param parameters Borrowed parameter pack, retained by the returned expression.
 * @param variables Borrowed two-variable pack, retained by the returned expression.
 * @return Owned node released with expr_free(), or NULL for an absent pack or allocation failure.
 */
expr_t *appell_F4_rebuild(const expr_t *parameters, const expr_t *variables);

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

expr_t *expr_lommel_S_argument_derivative_expansion(const expr_t *mu, const expr_t *nu, const expr_t *argument);

#endif /* EXPR_MATHS_H */
