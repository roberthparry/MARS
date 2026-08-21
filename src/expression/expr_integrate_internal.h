#ifndef EXPR_INTEGRATE_INTERNAL_H
#define EXPR_INTEGRATE_INTERNAL_H

#if !defined(MARS_EXPR_INTEGRATE_INTERNAL_ACCESS) &&                                                                   \
    (!defined(__INTELLISENSE__) || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error                                                                                                                 \
    "expr_integrate_internal.h is private to the expression integration implementation; include expression.h instead."
#endif

#include <stdbool.h>
#include <stddef.h>

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

static inline bool expr_integrate_contains_imaginary_unit(const expr_t *expr)
{
    if (!expr)
        return false;
    if (expr_is_const(expr) && (num_eq(expr->c, NUM_I) || num_eq(expr->c, NUM_NEG_I)))
        return true;
    return expr_integrate_contains_imaginary_unit(expr->a) || expr_integrate_contains_imaginary_unit(expr->b);
}

/* Shared ownership, simplification and dispatch helpers. */
expr_t *simplify_owned(expr_t *expr);
bool depends_on_wrt(const expr_t *expr, const expr_t *wrt);
bool expr_equal_exact_local(const expr_t *a, const expr_t *b);
bool is_wrt(const expr_t *expr, const expr_t *wrt);
expr_t *expr_integrate_dispatch(const expr_t *expr, const expr_t *wrt);
expr_t *expr_integrate_as_constant(const expr_t *expr, const expr_t *wrt);
expr_t *expr_integrate_normalize_radical_products(const expr_t *expr);

/* Polynomial and affine matching helpers. */
bool match_nonconstant_affine_linear_expr(const expr_t *expr, const expr_t *wrt, number_t *constant_out,
                                          number_t *coeff_out);
void number_array_zero_local(number_t *values, size_t count);
void number_array_clear_local(number_t *values, size_t count);
void number_array_reset_zero_local(number_t *values, size_t count);
bool expr_integrate_rewrite_poly_deg4_to_affine_basis(number_t *poly, number_t from_constant, number_t from_coeff,
                                                      number_t to_constant, number_t to_coeff);
expr_t *integrate_poly_times_affine_power(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_poly_times_unary_affine_kind(const expr_t *expr, const expr_t *wrt,
                                               expr_pattern_unary_affine_kind_t kind);
expr_t *integrate_poly_times_log_affine(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_poly_over_matching_affine(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_poly_over_centered_quadratic(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_power_of_wrt(const expr_t *base, number_t exponent, const expr_t *wrt);
expr_t *integrate_constant_over_power_denominator(const expr_t *numerator, const expr_t *denominator,
                                                  const expr_t *wrt);
expr_t *div_number_owned(expr_t *expr, number_t denom);
expr_t *div_number_owned_consuming(expr_t *expr, number_t *denom);
expr_t *div_number_owned_by_product(expr_t *expr, number_t left, number_t right);
expr_t *div_number_owned_by_long_product(expr_t *expr, long left, number_t right);
expr_t *mul_number_owned(expr_t *expr, number_t factor);
expr_t *mul_number_owned_consuming(expr_t *expr, number_t *factor);

/* Expression construction helpers. */
expr_t *build_affine_from_match(const expr_t *wrt, number_t constant, number_t coeff);
expr_t *build_polynomial_expr(const expr_t *var, const number_t *coeffs, size_t count);
bool affine_linear_match_eq(number_t constant_a, number_t coeff_a, number_t constant_b, number_t coeff_b);

/* Affine and symbolic shape matchers. */
bool match_one_plus_minus_affine_square(const expr_t *expr, const expr_t *wrt, bool *is_plus_out,
                                        number_t *constant_out, number_t *coeff_out);
bool match_affine_unary_data(const expr_t *expr, const expr_t *wrt, expr_pattern_unary_affine_kind_t kind,
                             number_t *constant_out, number_t *coeff_out);
bool match_affine_unary(const expr_t *expr, const expr_t *wrt, expr_pattern_unary_affine_kind_t kind,
                        number_t *constant_out, number_t *coeff_out);
bool is_wrt_symbolic_affine_leaf(const expr_t *expr, const expr_t *wrt);
bool is_negated_wrt_symbolic_affine_leaf(const expr_t *expr, const expr_t *wrt);
expr_t *match_symbolic_wrt_factor_coeff(const expr_t *expr, const expr_t *wrt);
bool match_symbolic_affine_constant_and_coeff(const expr_t *expr, const expr_t *wrt, expr_t **constant_term_out,
                                              expr_t **coeff_out);
bool match_symbolic_quadratic_coeffs(const expr_t *expr, const expr_t *wrt, expr_t **quad_out, expr_t **linear_out,
                                     expr_t **constant_out);

/* Affine, inverse and radical integration helpers. */
expr_t *integrate_affine_unary_kind(const expr_t *expr, const expr_t *wrt, expr_pattern_unary_affine_kind_t kind,
                                    expr_apply_unary_fn antiderivative_fn, number_t sign);
expr_t *integrate_linear_poly_times_inverse_affine(const expr_t *expr, const expr_t *wrt,
                                                   expr_pattern_unary_affine_kind_t kind);
expr_t *integrate_sqrt_one_plus_minus_affine_square(const expr_t *quadratic, const expr_t *wrt);
expr_t *integrate_centered_quadratic_root(const expr_t *quadratic, const expr_t *wrt);
expr_t *integrate_linear_poly_over_centered_quadratic_root(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_linear_poly_times_centered_quadratic_root(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_sqrt_wrt_over_symbolic_unit_affine(const expr_t *base, const expr_t *wrt);
expr_t *integrate_wrt_over_symbolic_affine_root(const expr_t *expr, const expr_t *wrt);

/* Symbolic square and general-quadratic families. */
expr_t *integrate_symbolic_monomial_times_affine_power(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_symbolic_square_family_root(const expr_t *quadratic, const expr_t *wrt);
expr_t *integrate_symbolic_square_family_inverse_root(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_symbolic_square_family_wrt_over_root(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_symbolic_square_family_times_root(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_symbolic_general_quadratic_root(const expr_t *quadratic, const expr_t *wrt);
expr_t *integrate_symbolic_general_quadratic_linear_over_root(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_symbolic_general_quadratic_times_root(const expr_t *expr, const expr_t *wrt);

/* Logarithmic, exponential and trigonometric symbolic families. */
expr_t *integrate_log_of_symbolic_affine(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_log_over_symbolic_proportional_affine(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_wrt_times_log_symbolic_affine(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_wrt_times_log_symbolic_quadratic(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_log_over_proportional_affine(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_polynomial_times_polynomial_exp(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_symbolic_integer_power_times_exp(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_symbolic_power_times_exp_gamma(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_symbolic_integer_power_times_trig(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_sine_cosine_times_secant_power(const expr_t *expr, const expr_t *wrt);
expr_t *expr_integrate_build_unsigned_expr_power(const expr_t *base, unsigned int exponent);
bool expr_integrate_number_matches_uint_at_most(number_t value, unsigned int max_value, unsigned int *out);
bool expr_integrate_match_wrt_power_factor_exponent(const expr_t *expr, const expr_t *wrt, expr_t **exponent_out);
bool match_exp_proportional_wrt_coeff(const expr_t *expr, const expr_t *wrt, expr_t **coeff_out);
bool match_trig_proportional_wrt_coeff(const expr_t *expr, const expr_t *wrt, bool *is_sin_out, expr_t **coeff_out);
expr_t *integrate_symbolic_exp_times_trig(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_symbolic_exp_times_hyperbolic(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_symbolic_hyperbolic_product(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_symbolic_trig_times_hyperbolic(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_symbolic_squared_hyperbolic(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_exact_substitution_product(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_poly_times_rational_unary_by_parts(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_log_times_trig_by_parts(const expr_t *expr, const expr_t *wrt);

/* Exact products and distribution-specific helpers. */
expr_t *integrate_wrt_exp_times_trig_exact(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_exp_tanh_exact(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_linear_poly_times_normal_logpdf_affine(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_polynomial_over_monomial_power(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_rational_partial_fractions(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_linear_over_symbolic_quadratic(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_log_of_symbolic_quadratic(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_exp_of_negative_quadratic(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_squared_unary_affine(const expr_t *expr, const expr_t *wrt, expr_pattern_unary_affine_kind_t kind);
expr_t *integrate_cubed_unary_affine(const expr_t *expr, const expr_t *wrt, expr_pattern_unary_affine_kind_t kind);
expr_t *integrate_sec_double_angle_log_tan_cot(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_sec_squared_log_tan_cot(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_inverse_sqrt_sin_cos_sin3_cos(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_inverse_quartic_appell_f1(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_matching_squared_unary_affine(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_matching_cubed_unary_affine(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_same_affine_special_product(const expr_t *expr, const expr_t *wrt);
void exp_antiderivative_once_local(const number_t *src, size_t count, number_t *dst);
void trig_antiderivative_once_local(const number_t *a_src, const number_t *b_src, size_t count, number_t *a_dst,
                                    number_t *b_dst);
void hyperbolic_antiderivative_once_local(const number_t *a_src, const number_t *b_src, size_t count, number_t *a_dst,
                                          number_t *b_dst);

/* Dispatcher rule entry points. */
expr_t *integrate_exp_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_constant_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_var_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_add_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_sub_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_neg_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_mul_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_div_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_pow_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_pow_d_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_sqrt_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_cubrt_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_root_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_log_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_log10_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_sin_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_cos_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_tan_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_sec_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_cosec_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_cot_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_sinh_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_cosh_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_cosech_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_tanh_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_sech_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_coth_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_asin_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_acos_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_atan_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_asec_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_acosec_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_acot_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_asinh_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_acosh_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_atanh_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_asech_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_acosech_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_acoth_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_erf_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_erfc_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_normal_pdf_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_normal_cdf_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_normal_logpdf_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_Ei_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_E1_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_bessel_j_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_bessel_y_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_lommel_s_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_hypergeometric_pFq_rule(const expr_t *expr, const expr_t *wrt);

#endif /* EXPR_INTEGRATE_INTERNAL_H */
