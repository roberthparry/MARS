#ifndef DVAL_BINDING_SIMPLIFY_H
#define DVAL_BINDING_SIMPLIFY_H

#include "dval_bindings.h"

dv_binding_expr_t *dv_binding_simplify_atom(dv_binding_expr_t *expr);
dv_binding_expr_t *dv_binding_simplify_neg(dv_binding_expr_t *expr);
dv_binding_expr_t *dv_binding_simplify_addsub(dv_binding_expr_t *expr);
dv_binding_expr_t *dv_binding_simplify_mul(dv_binding_expr_t *expr);
dv_binding_expr_t *dv_binding_simplify_div(dv_binding_expr_t *expr);
dv_binding_expr_t *dv_binding_simplify_powi(dv_binding_expr_t *expr);
dv_binding_expr_t *dv_binding_simplify_unary_op(dv_binding_expr_t *expr);
dv_binding_expr_t *dv_binding_simplify_binary_op(dv_binding_expr_t *expr);

number_t binding_number_from_text(const char *text);
long binding_gcd_long(long a, long b);
bool binding_number_text_eq_long(const dv_binding_expr_t *expr,
                                 long expected_long);
bool binding_number_text_to_long(const dv_binding_expr_t *expr, long *out);
bool binding_number_text_to_small_rational(const dv_binding_expr_t *expr,
                                           long *numerator,
                                           long *denominator);
bool binding_number_text_log10_power_exponent(const dv_binding_expr_t *expr,
                                              long *exponent_out);
bool binding_expr_scaled_const_ratio(const dv_binding_expr_t *expr,
                                     long *numer_out,
                                     long *denom_out,
                                     dv_binding_const_id_t *const_id_out);
bool binding_const_ratio_parts(const dv_binding_expr_t *numer_expr,
                               const dv_binding_expr_t *denom_expr,
                               long *numer_out,
                               long *denom_out,
                               dv_binding_const_id_t *const_id_out);

bool binding_expr_is_const_id(const dv_binding_expr_t *expr,
                              dv_binding_const_id_t const_id);
dv_binding_expr_t *binding_expr_fold_to_number_owned(dv_binding_expr_t *expr,
                                                     number_t value);
dv_binding_expr_t *binding_expr_try_fold_exact_complex_owned(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_fold_number_owned(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_fold_neg_leading_number(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_fold_mul_leading_numbers(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_fold_div_leading_number(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_combine_mul_powers(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_simplify_basic_sum(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_simplify_basic_product(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_simplify_basic_quotient(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_simplify_i_unit_product(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_simplify_trig_product(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_simplify_trig_sum(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_simplify_principal_inverse(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_simplify_direct_inverse(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_simplify_imag_trig_bridge(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_simplify_lambert_product(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_simplify_lambert_inverse(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_simplify_complex_floor_ceil(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_simplify_log_e(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_simplify_log10_power(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_simplify_trig_exact(dv_binding_expr_t *expr);
dv_binding_expr_t *binding_expr_try_simplify_logbeta_integers(dv_binding_expr_t *expr);

#endif
