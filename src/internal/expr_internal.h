#ifndef EXPR_SHARED_INTERNAL_H
#define EXPR_SHARED_INTERNAL_H

#if !defined(MARS_SHARED_EXPR_INTERNAL_ACCESS) &&                                                                      \
    (!defined(__INTELLISENSE__) || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "internal/expr_internal.h is private to the MARS implementation; include expression.h instead."
#endif

#include <stdbool.h>
#include <stddef.h>

#include "expression.h"
#include "ustring.h"

typedef enum expr_integration_bound_kind {
    EXPR_INTEGRATION_BOUND_DEFINITE = 0,
    EXPR_INTEGRATION_BOUND_UPPER_ONLY,
    EXPR_INTEGRATION_BOUND_INDEFINITE
} expr_integration_bound_kind_t;

typedef enum {
    EXPR_PATTERN_UNARY_EXP,
    EXPR_PATTERN_UNARY_LOG,
    EXPR_PATTERN_UNARY_LOG10,
    EXPR_PATTERN_UNARY_SIN,
    EXPR_PATTERN_UNARY_COS,
    EXPR_PATTERN_UNARY_TAN,
    EXPR_PATTERN_UNARY_SEC,
    EXPR_PATTERN_UNARY_COSEC,
    EXPR_PATTERN_UNARY_COT,
    EXPR_PATTERN_UNARY_SINH,
    EXPR_PATTERN_UNARY_COSH,
    EXPR_PATTERN_UNARY_COSECH,
    EXPR_PATTERN_UNARY_TANH,
    EXPR_PATTERN_UNARY_SECH,
    EXPR_PATTERN_UNARY_COTH,
    EXPR_PATTERN_UNARY_ASIN,
    EXPR_PATTERN_UNARY_ACOS,
    EXPR_PATTERN_UNARY_ATAN,
    EXPR_PATTERN_UNARY_ASEC,
    EXPR_PATTERN_UNARY_ACOSEC,
    EXPR_PATTERN_UNARY_ACOT,
    EXPR_PATTERN_UNARY_ASINH,
    EXPR_PATTERN_UNARY_ACOSH,
    EXPR_PATTERN_UNARY_ATANH,
    EXPR_PATTERN_UNARY_ASECH,
    EXPR_PATTERN_UNARY_ACOSECH,
    EXPR_PATTERN_UNARY_ACOTH,
    EXPR_PATTERN_UNARY_ERF,
    EXPR_PATTERN_UNARY_ERFC,
    EXPR_PATTERN_UNARY_NORMAL_PDF,
    EXPR_PATTERN_UNARY_NORMAL_CDF,
    EXPR_PATTERN_UNARY_NORMAL_LOGPDF,
    EXPR_PATTERN_UNARY_EI,
    EXPR_PATTERN_UNARY_E1,
    EXPR_PATTERN_UNARY_COUNT
} expr_pattern_unary_affine_kind_t;

bool expr_is_exact_zero(const expr_t *dv);
bool expr_is_named_const(const expr_t *dv);
bool expr_stringin_function_hash_is_valid(void);
int expr_get_default_constant_num_text(const string_t *name, number_t *value_out);
expr_t *expr_const_zero(void);
expr_t *expr_const_one(void);
expr_t *expr_const_long(long value);
expr_t *expr_retain_expr(const expr_t *expr);
expr_t *expr_from_expression_text_formal(const string_t *expr, const string_t *const *names, expr_t *const *symbols,
                                         size_t nsymbols);
expr_bindings_t *expr_bindings_clone_internal(const expr_bindings_t *bindings, bool constants_only);
expr_bindings_t *expr_bindings_from_expr_internal(const expr_t *expr);
expr_bindings_t *expr_bindings_merge_internal(const expr_bindings_t *bindings,
                                              const expr_bindings_t *additional_bindings);
expr_t *expr_simplify_owned(expr_t *expr);
expr_t *expr_beautify(const expr_t *expr);
expr_t *expr_beautify_presimplified(const expr_t *expr);
expr_t *expr_factor_common_post_calculus(const expr_t *expr);
bool expr_contains_half_scaled_symbolic_power(const expr_t *expr);
expr_t *expr_negate_owned(expr_t *expr);
expr_t *expr_mul_long(const expr_t *expr, long value);
expr_t *expr_div_long(const expr_t *expr, long value);
expr_t *expr_pow_long(const expr_t *expr, long exponent);
expr_t *expr_add_long(const expr_t *expr, long value);
expr_t *expr_new_indexed_symbol(const char *name, const expr_t *index);
expr_t *expr_new_summation(const expr_t *term, const expr_t *index);
expr_t *expr_new_finite_summation(const expr_t *term, const expr_t *index, const expr_t *upper);
expr_t *expr_add_simplify_owned(const expr_t *left, const expr_t *right);
expr_t *expr_sub_simplify_owned(const expr_t *left, const expr_t *right);
expr_t *expr_mul_simplify_owned(const expr_t *left, const expr_t *right);
expr_t *expr_div_simplify_owned(const expr_t *left, const expr_t *right);
int expr_struct_eq(const expr_t *u, const expr_t *v);
bool expr_child_exprs(const expr_t *expr, const expr_t **left_out, const expr_t **right_out);
bool expr_is_formal_derivative(const expr_t *expr);
expr_t *expr_new_formal_derivative(const expr_t *dependent, size_t wrt_count, expr_t *const *wrts);
expr_t *expr_new_arbitrary_function(const char *name, const expr_t *argument);
expr_t *expr_new_arbitrary_function_n(const char *name, size_t argument_count, expr_t *const *arguments);
bool expr_is_arbitrary_function(const expr_t *expr);
const expr_t *expr_formal_derivative_dependent(const expr_t *expr);
size_t expr_formal_derivative_order(const expr_t *expr);
const expr_t *expr_formal_derivative_wrt_at(const expr_t *expr, size_t index);

bool expr_match_neg_expr(const expr_t *expr, const expr_t **arg_out);
bool expr_match_unary_expr(const expr_t *expr, const expr_t **arg_out);
bool expr_match_exp_expr(const expr_t *expr, const expr_t **arg_out);
bool expr_match_log_expr(const expr_t *expr, const expr_t **arg_out);
bool expr_match_sin_expr(const expr_t *expr, const expr_t **arg_out);
bool expr_match_cos_expr(const expr_t *expr, const expr_t **arg_out);
bool expr_match_tan_expr(const expr_t *expr, const expr_t **arg_out);
bool expr_match_cot_expr(const expr_t *expr, const expr_t **arg_out);
bool expr_match_pow_const(const expr_t *expr, const expr_t **base_out, number_t *exponent_out);
bool expr_match_pow_expr(const expr_t *expr, const expr_t **base_out, const expr_t **exponent_out);
bool expr_match_integral_expr(const expr_t *expr, const expr_t **integrand_out, const expr_t **domain_out);
void expr_set_binding_pi_linear_family(expr_t *expr, long denominator, long n_coeff, long offset);
bool expr_exact_complex_root_seed(const expr_t *expr, number_t *seed_out, long *order_out);
expr_t *expr_explicit_root_base(const expr_t *expr, long *order_out);
bool expr_explicit_root_order(const expr_t *expr, long *order_out);

bool expr_match_const_value(const expr_t *expr, number_t *value_out);
bool expr_match_var_expr(const expr_t *expr, size_t nvars, expr_t *const *vars, size_t *index_out);
bool expr_match_scaled_expr(const expr_t *expr, number_t *scale_out, const expr_t **base_out);
bool expr_match_add_sub_expr(const expr_t *expr, const expr_t **left_out, const expr_t **right_out, bool *is_sub_out);
const expr_t *expr_first_child(const expr_t *expr);
const expr_t *expr_second_child(const expr_t *expr);
bool expr_simplify_same_factor(const expr_t *left, const expr_t *right);
bool expr_match_mul_expr(const expr_t *expr, const expr_t **left_out, const expr_t **right_out);

expr_t *expr_simplify_extract_common_factor_quotient(const expr_t *expr, const expr_t *factor);
expr_t *expr_simplify_find_common_factor(expr_t *const *expressions, size_t count);
bool expr_match_div_expr(const expr_t *expr, const expr_t **left_out, const expr_t **right_out);
bool expr_collect_var_usage(const expr_t *expr, size_t nvars, expr_t *const *vars, bool *used_out);
bool expr_has_unbound_parameters(const expr_t *expr, size_t nvars, expr_t *const *vars);
expr_t *expr_expand_products_internal(const expr_t *expr);
expr_t *expr_canonicalize_known_radicals_internal(const expr_t *expr);
expr_t *expr_distribute_negative_for_display(const expr_t *expr);
string_t *expr_default_constant_canonical_name_text(const string_t *name);
expr_t *expr_integrate_iterated(const expr_t *integrand, size_t ndim, expr_t *const *vars,
                                const expr_integration_bound_kind_t *kinds, expr_t *const *lo, expr_t *const *hi,
                                size_t max_steps, size_t *completed_steps_out, expr_t **first_antiderivative_out);
expr_t *expr_integrate_iterated_best_effort(const expr_t *integrand, size_t ndim, expr_t *const *vars,
                                            const expr_integration_bound_kind_t *kinds, expr_t *const *lo,
                                            expr_t *const *hi, size_t *completed_steps_out, size_t *remaining_ndim_out,
                                            expr_t **remaining_vars_out, number_t *remaining_lo_num_out,
                                            number_t *remaining_hi_num_out, const number_t *lo_num,
                                            const number_t *hi_num);

bool expr_match_affine_poly_deg4(const expr_t *expr, size_t nvars, expr_t *const *vars, number_t *poly_coeffs_out,
                                 number_t *constant_out, number_t *coeffs_out);
bool expr_collect_poly_deg4(const expr_t *expr, const expr_t *var, number_t *coeffs_out);
bool expr_match_unary_affine_kind(const expr_t *expr, expr_pattern_unary_affine_kind_t kind, size_t nvars,
                                  expr_t *const *vars, number_t *constant_out, number_t *coeffs_out);
bool expr_match_affine_poly_deg4_times_unary_affine_kind(const expr_t *expr,
                                                         expr_pattern_unary_affine_kind_t unary_kind, size_t nvars,
                                                         expr_t *const *vars, number_t *poly_coeffs_out,
                                                         number_t *constant_out, number_t *coeffs_out);

string_t *expr_normalise_name_text(const string_t *name);
string_t *expr_normalise_greek_alias_text(const string_t *alias);
string_t *expr_normalise_binding_name_text(const string_t *name);
int expr_is_default_constant_name_text(const string_t *name);
char *expr_tostring_texify(const char *text);
int expr_to_TeX_parts(const expr_t *dv, char **expr_out, char **bindings_out);
char *expr_to_TeX_body_wrapped_with_partials(const expr_t *expr, size_t line_limit);
char *expr_to_TeX_body_wrapped_with_totals(const expr_t *expr, size_t line_limit);

#endif /* EXPR_SHARED_INTERNAL_H */
