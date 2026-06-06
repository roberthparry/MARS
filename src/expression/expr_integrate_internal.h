#ifndef EXPR_INTEGRATE_INTERNAL_H
#define EXPR_INTEGRATE_INTERNAL_H

#include "expr_internal.h"

expr_t *simplify_owned(expr_t *expr);
bool depends_on_wrt(const expr_t *expr, const expr_t *wrt);
bool expr_equal_exact_local(const expr_t *a, const expr_t *b);
bool is_wrt(const expr_t *expr, const expr_t *wrt);
bool match_nonconstant_affine_linear_expr(const expr_t *expr,
                                          const expr_t *wrt,
                                          number_t *constant_out,
                                          number_t *coeff_out);
void number_array_zero_local(number_t *values, size_t count);
void number_array_clear_local(number_t *values, size_t count);
expr_t *div_number_owned(expr_t *expr, number_t denom);
expr_t *div_number_owned_consuming(expr_t *expr, number_t *denom);
expr_t *mul_number_owned(expr_t *expr, number_t factor);
expr_t *mul_number_owned_consuming(expr_t *expr, number_t *factor);
expr_t *build_affine_from_match(const expr_t *wrt,
                                number_t constant,
                                number_t coeff);
expr_t *build_polynomial_expr(const expr_t *var,
                              const number_t *coeffs,
                              size_t count);
bool affine_linear_match_eq(number_t constant_a,
                            number_t coeff_a,
                            number_t constant_b,
                            number_t coeff_b);
bool match_one_plus_minus_affine_square(const expr_t *expr,
                                        const expr_t *wrt,
                                        bool *is_plus_out,
                                        number_t *constant_out,
                                        number_t *coeff_out);
bool match_affine_unary_data(const expr_t *expr,
                             const expr_t *wrt,
                             expr_pattern_unary_affine_kind_t kind,
                             number_t *constant_out,
                             number_t *coeff_out);
bool match_affine_unary(const expr_t *expr,
                        const expr_t *wrt,
                        expr_pattern_unary_affine_kind_t kind,
                        number_t *constant_out,
                        number_t *coeff_out);
expr_t *integrate_affine_unary_kind(const expr_t *expr,
                                    const expr_t *wrt,
                                    expr_pattern_unary_affine_kind_t kind,
                                    expr_apply_unary_fn antiderivative_fn,
                                    number_t sign);
expr_t *integrate_linear_poly_times_inverse_affine(
    const expr_t *expr,
    const expr_t *wrt,
    expr_pattern_unary_affine_kind_t kind);
expr_t *integrate_linear_poly_times_normal_logpdf_affine(const expr_t *expr,
                                                         const expr_t *wrt);
expr_t *integrate_rational_partial_fractions(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_linear_over_symbolic_quadratic(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_log_of_symbolic_quadratic(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_exp_of_negative_quadratic(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_squared_unary_affine(const expr_t *expr,
                                       const expr_t *wrt,
                                       expr_pattern_unary_affine_kind_t kind);
expr_t *integrate_cubed_unary_affine(const expr_t *expr,
                                     const expr_t *wrt,
                                     expr_pattern_unary_affine_kind_t kind);
expr_t *integrate_same_affine_special_product(const expr_t *expr, const expr_t *wrt);
void exp_antiderivative_once_local(const number_t *src, size_t count, number_t *dst);
void trig_antiderivative_once_local(const number_t *a_src,
                                    const number_t *b_src,
                                    size_t count,
                                    number_t *a_dst,
                                    number_t *b_dst);
void hyperbolic_antiderivative_once_local(const number_t *a_src,
                                          const number_t *b_src,
                                          size_t count,
                                          number_t *a_dst,
                                          number_t *b_dst);

expr_t *integrate_exp_rule(const expr_t *expr, const expr_t *wrt);
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
expr_t *integrate_ei_rule(const expr_t *expr, const expr_t *wrt);
expr_t *integrate_e1_rule(const expr_t *expr, const expr_t *wrt);

#endif
