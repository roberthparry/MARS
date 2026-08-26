#ifndef EXPR_SHARED_INTERNAL_H
#define EXPR_SHARED_INTERNAL_H

#if !defined(MARS_SHARED_EXPR_INTERNAL_ACCESS) &&                                                                      \
    (!defined(__INTELLISENSE__) || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "internal/expr_internal.h is private to the MARS implementation; include expression.h instead."
#endif

#ifndef EXPR_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "expression.h"
#include "ustring.h"

typedef enum expr_integration_bound_kind {
    EXPR_INTEGRATION_BOUND_DEFINITE = 0,
    EXPR_INTEGRATION_BOUND_UPPER_ONLY,
    EXPR_INTEGRATION_BOUND_INDEFINITE
} expr_integration_bound_kind_t;

/**
 * @brief Return an owning Cartesian display form without factoring out a shared real scale.
 *
 * @param expr Expression to separate into real and imaginary terms.
 * @return Owning separated expression, or `NULL` when no Cartesian separation is available.
 */
expr_t *expr_separate_cartesian_for_display(const expr_t *expr);

/**
 * @brief Split an expression into owning real and imaginary Cartesian coefficients for display.
 *
 * @param expr Expression to inspect.
 * @param real_out Owning real coefficient on success.
 * @param imaginary_out Owning imaginary coefficient on success.
 * @param has_imaginary_out Whether the imaginary coefficient is structurally present.
 * @return `true` when the expression can be split.
 */
bool expr_cartesian_parts_for_display(const expr_t *expr, expr_t **real_out, expr_t **imaginary_out,
                                      bool *has_imaginary_out);

/**
 * @brief Expand supported unary functions of Cartesian arguments recursively for display.
 *
 * @param expr Expression whose supported unary nodes should be expanded.
 * @return Owning Cartesian expression, or `NULL` when no display expansion applies.
 */
expr_t *expr_complex_unary_cartesian_for_display(const expr_t *expr);

/**
 * @brief Rotate Cartesian expressions by exact imaginary factors recursively for display.
 *
 * @param expr Expression whose Cartesian product should be rotated.
 * @return Owning rotated expression, or `NULL` when the rewrite does not apply.
 */
expr_t *expr_beautify_imaginary_cartesian_product_for_display(const expr_t *expr);

/**
 * @brief Move a named additive constant last without resimplifying the displayed algebra.
 *
 * @param expr Expression whose named addend should be reordered.
 * @param name Name of the additive constant to move.
 * @return Owning reordered expression, or `NULL` when the named addend is absent.
 */
expr_t *expr_move_named_addend_last_for_display(const expr_t *expr, const char *name);

/**
 * @brief Place the imaginary unit last in Cartesian coefficients for display.
 *
 * @param expr Cartesian expression whose imaginary coefficients should be reordered.
 * @return Owning reordered expression, or `NULL` when no reorder is required.
 */
expr_t *expr_move_imaginary_unit_last_for_display(const expr_t *expr);

/**
 * @brief Prepend an explicit zero real component to a purely imaginary display expression.
 *
 * @param expr Expression to inspect.
 * @return Owning `0 + qi` expression, or `NULL` when no prefix is needed.
 */
expr_t *expr_prepend_zero_real_component_for_display(const expr_t *expr);

/**
 * @brief Write a symbolic reciprocal square root in Cartesian surd form for display.
 *
 * @param expr Expression to rewrite.
 * @return Owning Cartesian reciprocal-root expression, or `NULL` when the rewrite does not apply.
 */
expr_t *expr_beautify_symbolic_complex_square_root_reciprocal_for_display(const expr_t *expr);

/**
 * @brief Expand preserved binding expressions into their expression trees for display processing.
 *
 * @param expr Expression whose preserved nodes should be expanded.
 * @return Owning expanded expression, or `NULL` on failure.
 */
expr_t *expr_expand_preserved_for_display(const expr_t *expr);

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
/** Return whether an expression is a formal summation node. */
bool expr_is_summation(const expr_t *expr);

/**
 * @brief Return the closed form supplied by the summand function for a recognised finite progression.
 *
 * @param expr Expression to inspect.
 * @return Owning closed-form expression, or `NULL` when no identity applies.
 */
expr_t *expr_finite_progression_closed_form(const expr_t *expr);

/**
 * @brief Return whether a progression reduction depends on the supplied value of its step.
 *
 * @param expr Expression to inspect.
 * @return `true` when the native result cards must retain the bound algebraic specialisation.
 */
bool expr_finite_progression_requires_bound_step(const expr_t *expr);
expr_t *expr_finite_weighted_sinh_lerch_form(const expr_t *expr);
expr_t *expr_finite_weighted_sinh_from_lerch_form(const expr_t *expr);
bool expr_is_finite_weighted_sinh_lerch_form(const expr_t *expr);
bool expr_finite_weighted_sinh_lerch_value(const expr_t *expr, number_t *value_out);
expr_t *expr_finite_weighted_cosh_lerch_form(const expr_t *expr);
expr_t *expr_finite_weighted_cosh_from_lerch_form(const expr_t *expr);
bool expr_is_finite_weighted_cosh_lerch_form(const expr_t *expr);
bool expr_finite_weighted_cosh_lerch_value(const expr_t *expr, number_t *value_out);
expr_t *expr_finite_weighted_sin_lerch_form(const expr_t *expr);
bool expr_finite_weighted_sin_lerch_value(const expr_t *expr, number_t *value_out);
expr_t *expr_finite_weighted_cos_lerch_form(const expr_t *expr);
bool expr_finite_weighted_cos_lerch_value(const expr_t *expr, number_t *value_out);
expr_t *expr_finite_tangent_from_qdigamma_form(const expr_t *expr);
bool expr_is_finite_tangent_qdigamma_form(const expr_t *expr);
bool expr_finite_tangent_qdigamma_value(const expr_t *expr, number_t *value_out);
expr_t *expr_finite_tanh_from_qdigamma_form(const expr_t *expr);
expr_t *expr_finite_progression_from_qdigamma_form(const expr_t *expr);
bool expr_finite_qdigamma_progression_value(const expr_t *expr, number_t *value_out);
expr_t *expr_finite_atan_progression_derivative_form(const expr_t *expr, const expr_t *wrt);

/**
 * @brief Render a recognised finite progression as a summation followed by its closed form.
 *
 * @param expr Expression to inspect.
 * @return Newly allocated TeX text, or `NULL` when the expression is not a recognised progression.
 */
char *expr_finite_progression_identity_TeX(const expr_t *expr);
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

/**
 * @brief Test whether an expression is headed by a selected unary function.
 *
 * @param expr Expression to inspect.
 * @param kind Unary function kind to match.
 * @return `true` when the expression has the requested unary head.
 */
bool expr_is_unary_pattern_kind(const expr_t *expr, expr_pattern_unary_affine_kind_t kind);
bool expr_match_affine_poly_deg4_times_unary_affine_kind(const expr_t *expr,
                                                         expr_pattern_unary_affine_kind_t unary_kind, size_t nvars,
                                                         expr_t *const *vars, number_t *poly_coeffs_out,
                                                         number_t *constant_out, number_t *coeffs_out);

string_t *expr_normalise_name_text(const string_t *name);
string_t *expr_normalise_greek_alias_text(const string_t *alias);

/**
 * @brief Resolve a canonical Greek symbol to its at-prefixed ASCII alias.
 *
 * Lower-case and upper-case symbols retain their case in the returned alias.
 *
 * @param symbol Greek Unicode symbol to resolve.
 * @return Borrowed process-lifetime alias, or `NULL` when the symbol is not registered.
 */
const char *expr_greek_symbol_alias(rune_t symbol);

string_t *expr_normalise_binding_name_text(const string_t *name);
int expr_is_default_constant_name_text(const string_t *name);
char *expr_tostring_texify(const char *text);
int expr_to_TeX_parts(const expr_t *dv, char **expr_out, char **bindings_out);
char *expr_to_TeX_body_wrapped_with_partials(const expr_t *expr, size_t line_limit);
char *expr_to_TeX_body_wrapped_with_totals(const expr_t *expr, size_t line_limit);

#endif /* EXPR_INTERNAL_H */

typedef struct expr_function_temporaries expr_function_temporaries_t;

/* Build a shared function-temporary plan for several expression roots. */
expr_function_temporaries_t *expr_function_temporaries_new(const expr_t *const *roots, size_t count);

/* Render the declarations selected by a shared function-temporary plan. */
string_t *expr_function_temporaries_declarations_text(const expr_function_temporaries_t *plan);

/* Render one root using the names selected by a shared function-temporary plan. */
string_t *expr_function_temporaries_expression_text(const expr_function_temporaries_t *plan, const expr_t *expr);

/* Release a shared function-temporary plan. */
void expr_function_temporaries_free(expr_function_temporaries_t *plan);

typedef bool (*expr_series_binding_lookup_fn)(void *context, const char *name, number_t *value_out);

string_t *expr_expand_series_text(string_view_t source, string_t **display_TeX_out,
                                  expr_series_binding_lookup_fn lookup_binding, void *lookup_context,
                                  bool *domain_specialised_out);

expr_t *expr_from_string_with_derivation_TeX_internal(const char *s, expr_bindings_t **bnd_out,
                                                      string_t **derivation_TeX_out,
                                                      bool *domain_specialised_out);

#endif /* EXPR_SHARED_INTERNAL_H */
