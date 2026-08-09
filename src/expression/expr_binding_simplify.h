#ifndef EXPR_BINDING_SIMPLIFY_H
#define EXPR_BINDING_SIMPLIFY_H

#include "expr_bindings.h"

/* Top-level simplification stages. */
expr_binding_expr_t *expr_binding_simplify_atom(expr_binding_expr_t *expr);
expr_binding_expr_t *expr_binding_simplify_neg(expr_binding_expr_t *expr);
expr_binding_expr_t *expr_binding_simplify_addsub(expr_binding_expr_t *expr);
expr_binding_expr_t *expr_binding_simplify_mul(expr_binding_expr_t *expr);
expr_binding_expr_t *expr_binding_simplify_div(expr_binding_expr_t *expr);
expr_binding_expr_t *expr_binding_simplify_powi(expr_binding_expr_t *expr);
expr_binding_expr_t *expr_binding_simplify_unary_op(expr_binding_expr_t *expr);
expr_binding_expr_t *expr_binding_simplify_binary_op(expr_binding_expr_t *expr);

/* Numeric and constant-shape helpers. */
number_t binding_number_from_text(const char *text);
long binding_gcd_long(long a, long b);
bool binding_number_text_eq_long(const expr_binding_expr_t *expr, long expected_long);
bool binding_number_text_to_long(const expr_binding_expr_t *expr, long *out);
bool binding_number_text_to_small_rational(const expr_binding_expr_t *expr, long *numerator, long *denominator);
bool binding_number_text_log10_power_exponent(const expr_binding_expr_t *expr, long *exponent_out);
bool binding_expr_scaled_const_ratio(const expr_binding_expr_t *expr, long *numer_out, long *denom_out,
                                     expr_binding_const_id_t *const_id_out);
bool binding_const_ratio_parts(const expr_binding_expr_t *numer_expr, const expr_binding_expr_t *denom_expr,
                               long *numer_out, long *denom_out, expr_binding_const_id_t *const_id_out);

bool binding_expr_is_const_id(const expr_binding_expr_t *expr, expr_binding_const_id_t const_id);
expr_binding_expr_t *binding_expr_fold_to_number_owned(expr_binding_expr_t *expr, number_t value);

/* Local fold rules. */
expr_binding_expr_t *binding_expr_try_fold_exact_complex_owned(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_preserve_negated_decimal_owned(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_fold_number_owned(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_fold_neg_leading_number(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_fold_mul_leading_numbers(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_fold_div_leading_number(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_scaled_const_addsub(expr_binding_expr_t *expr);

/* Algebraic rewrite rules. */
expr_binding_expr_t *binding_expr_try_combine_mul_powers(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_nested_power(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_sqrt_square(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_integer_exp_power(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_exp_product(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_euler_square(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_log_difference(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_basic_sum(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_basic_product(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_basic_quotient(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_reciprocal_unary(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_i_unit_product(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_trig_product(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_trig_sum(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_principal_inverse(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_direct_inverse(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_imag_trig_bridge(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_lambert_exp(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_lambert_product(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_lambert_inverse(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_complex_floor_ceil(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_e_power(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_log_e(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_log10_power(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_asin_exact(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_trig_exact(expr_binding_expr_t *expr);
expr_binding_expr_t *binding_expr_try_simplify_logbeta_integers(expr_binding_expr_t *expr);

#endif /* EXPR_BINDING_SIMPLIFY_H */
