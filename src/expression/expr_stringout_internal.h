#ifndef EXPR_STRINGOUT_INTERNAL_H
#define EXPR_STRINGOUT_INTERNAL_H

#if !defined(MARS_EXPR_STRINGOUT_INTERNAL_ACCESS) &&                                                                   \
    (!defined(__INTELLISENSE__) || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "expr_stringout_internal.h is private to expression formatting; include expression.h instead."
#endif

#include <stdbool.h>
#include <stddef.h>

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

typedef enum { PREC_LOWEST = 0, PREC_ADD = 1, PREC_MUL = 2, PREC_POW = 3, PREC_UNARY = 4, PREC_ATOM = 5 } prec_t;

typedef struct {
    expr_t *node;
    char *buf;
} autoname_entry_t;

typedef struct {
    autoname_entry_t *entries;
    size_t count;
    size_t cap;
} autoname_table_t;

typedef struct {
    expr_t **vars;
    size_t count;
    size_t cap;
} varlist_t;

/* Local value formatting. */
char *expr_number_to_string_local(number_t value);
char *expr_const_to_string_local(const expr_t *expr);
char *expr_eval_to_string_local(const expr_t *expr);
char *expr_text_to_TeX_local(const char *text);
bool expr_is_immortal_default_const_local(const expr_t *expr);
bool expr_set_number_scientific_local(bool scientific);
int expr_set_number_precision_local(int precision);

/* Variable and constant discovery. */
/* Recognise opposite shifts in paired function arguments or definite-integral bounds, without rewriting the DAG. */
const expr_t *expr_display_symmetric_shift_centre(const expr_t *expr);
bool expr_display_centred_shift_parts(const expr_t *expr, const expr_t *centre,
                                      const expr_t **shift, bool *subtract);

void autoname_init(autoname_table_t *t);
void autoname_restore(autoname_table_t *t);
void assign_unnamed_vars_dfs(expr_t *f, autoname_table_t *t);
void varlist_init(varlist_t *vl);
void find_vars_dfs(const expr_t *f, varlist_t *vl);
void find_named_consts_dfs(const expr_t *f, varlist_t *cl);
void find_explicit_named_consts_dfs(const expr_t *f, varlist_t *cl);
const char *expr_name_or_default(const expr_t *expr, const char *fallback);

/* Binding RHS formatting. */
char *binding_rhs_expr_string_local(const expr_t *expr);
char *binding_rhs_TeX_string_local(const expr_t *expr);
char *binding_rhs_c_string_local(const expr_t *expr);

/* Expression emitters. */
typedef struct expr_distribution_TeX_scope {
    const expr_t *root;
    const expr_t *detached;
    bool appended;
    struct expr_distribution_TeX_scope *outer;
} expr_distribution_TeX_scope_t;

const char *expr_distribution_qualification(const expr_t *expr);
bool expr_distribution_has_qualification(const expr_t *expr);
const expr_t *expr_distribution_expr_body(const expr_t *root, sbuf_t *buffer);
bool expr_distribution_expr_emit(const expr_t *expr, sbuf_t *buffer);
void expr_distribution_expr_caption(const expr_t *expr, sbuf_t *buffer);
void expr_distribution_TeX_begin(const expr_t *root, expr_distribution_TeX_scope_t *scope);
void expr_distribution_TeX_end(expr_distribution_TeX_scope_t *scope, sbuf_t *buffer);
bool expr_distribution_TeX_emit(const expr_t *expr, sbuf_t *buffer, int parent_prec);
void expr_distribution_TeX_conditions(const expr_t *root, sbuf_t *buffer);

void emit_expr(const expr_t *f, sbuf_t *b, int parent_prec);
void emit_TeX_expr(const expr_t *f, sbuf_t *b, int parent_prec);
/** Emit native multiplication spacing or a separator between neighbouring factors. */
void emit_TeX_mul_separator(const expr_t *left, const expr_t *right, sbuf_t *b);
/** Collect borrowed additive terms in display order, with their signs, within the supplied capacity. */
bool expr_display_ordered_sum(const expr_t *expr, const expr_t **nodes, int *signs, size_t *count, size_t capacity);
void emit_func_fragment(sbuf_t *b, const char *text);
void emit_func(const expr_t *f, sbuf_t *b, int parent_prec);
void emit_func_operations(const expr_t *f, sbuf_t *b, int parent_prec);
bool emit_func_integral_cartesian_body(const expr_t *f, sbuf_t *b);
void emit_func_display(const expr_t *f, sbuf_t *b, int parent_prec);
void emit_func_with_temporaries(const expr_t *f, sbuf_t *b, int parent_prec, const expr_t *const *nodes,
                                const char *const *names, size_t count, const expr_t *expanded_node);
void emit_math_with_temporaries(const expr_t *f, sbuf_t *b, style_t style, const expr_t *const *nodes,
                                const char *const *names, size_t count);
void emit_TeX_name(sbuf_t *b, const char *name);
void expr_TeX_partial_derivatives_push(void);
void expr_TeX_partial_derivatives_pop(void);
bool expr_TeX_partial_derivatives_enabled(void);
void expr_TeX_total_derivatives_push(void);
void expr_TeX_total_derivatives_pop(void);
bool expr_TeX_total_derivatives_enabled(void);

/* Top-level text builders. */
string_t *expr_to_text_expr(const expr_t *f);
string_t *expr_to_text_unbound(const expr_t *f);
string_t *expr_to_text_function(const expr_t *f);
string_t *expr_to_text_function_cartesian(const expr_t *f);


/* Shared implementation helpers, grouped by their owning formatter. */

/* common formatting. */
void emit_superscript_int(sbuf_t *b, long n);
void emit_subscript_int(sbuf_t *b, long n);
bool expr_try_get_small_integer_exponent(number_t value, long *out);
int expr_tostring_should_emit_binding_expr(const expr_t *f);
bool numeric_atom_needs_grouping(const expr_t *expr, const char *text, int parent_prec);
void emit_atom(expr_t *f, sbuf_t *b, int parent_prec);
bool emit_negative_const_binding_expr_abs(const expr_t *f, sbuf_t *b, bool tex);
int pow_exp_needs_parens(const expr_t *e);
int pow_base_needs_visible_parens(const expr_t *base);
int mul_factor_needs_visible_parens(const expr_t *factor);
bool mul_coefficient_needs_parens(const expr_t *factor, bool leading);
bool additive_const_needs_visible_parens(const expr_t *constant);
int add_rhs_needs_visible_parens(const expr_t *rhs);
bool expr_is_const_half_local(const expr_t *f);
bool number_is_neg_half_local(number_t value);
bool expr_const_half_can_render_as_sqrt_local(const expr_t *f);
bool expr_const_neg_half_can_render_as_sqrt_local(const expr_t *f);
void emit_expr_mul_separator_local(const expr_t *left, const expr_t *right, sbuf_t *b);
void flatten_mul(expr_t *f, expr_t **buf, int *count, int max);
const char *function_temporary_name(const expr_t *expr);
void flatten_func_mul(expr_t *f, expr_t **buf, int *count, int max);
const char *function_factored_temporary_name(const expr_t *product, const expr_t *denominator,
                                                    expr_t **factors, int factor_count, bool *consumed);
int expr_tostring_is_primary_variable_name(const char *name);
void sort_factors(expr_t **fac, int n);
bool emit_TeX_unit_fraction_power(const expr_t *base, number_t exponent, sbuf_t *b, int parent_prec);
void emit_expr_sqrt_power(const expr_t *base, sbuf_t *b, int parent_prec, bool reciprocal);
bool match_atan_over_argument_denominator(const expr_t *expr, const expr_t **atan_expr_out,
                                                 const expr_t **denominator_out);
bool match_sum_quotient(const expr_t *expr, const expr_t **factor_out, const expr_t **sum_out);
int expr_is_negative(const expr_t *f);
void emit_factor_abs(const expr_t *f, sbuf_t *b);
int expr_renders_negative(const expr_t *f);
size_t function_ascii_stacked_fraction(char *out, const char *text);
size_t function_ascii_vulgar_fraction(char *out, const char *text);
const expr_t *expr_stringout_set_expanded_node(const expr_t *node);
const char *expr_math_temporary_name(const expr_t *expr);

/* cartesian formatting. */
bool emit_TeX_logarithmic_integral_cartesian(const expr_t *f, sbuf_t *b);
bool emit_TeX_exponential_integral_cartesian(const expr_t *f, sbuf_t *b);
bool emit_expr_integral_cartesian(const expr_t *f, sbuf_t *b, int parent_prec);
bool emit_func_integral_cartesian(const expr_t *f, sbuf_t *b, int parent_prec);
bool emit_TeX_cube_root_cartesian(const expr_t *f, sbuf_t *b);
bool emit_TeX_analytic_unary_cartesian(const expr_t *f, sbuf_t *b);

/* calculus formatting. */
void emit_expr_integral(const expr_t *f, sbuf_t *b, int parent_prec);
void emit_formal_derivative_expr(const expr_t *f, sbuf_t *b);
void emit_ordered_derivative(const expr_t *f, sbuf_t *b, int style);
void emit_formal_derivative_TeX(const expr_t *f, sbuf_t *b);
void emit_arbitrary_function_expr(const expr_t *f, sbuf_t *b);
void emit_argument_list_expr(const expr_t *f, sbuf_t *b);
void emit_formal_derivative_func(const expr_t *f, sbuf_t *b);
void emit_arbitrary_function_func(const expr_t *f, sbuf_t *b);
void emit_argument_list_func(const expr_t *f, sbuf_t *b);
void emit_arbitrary_function_TeX(const expr_t *f, sbuf_t *b);
void emit_argument_list_TeX(const expr_t *f, sbuf_t *b);
void emit_TeX_integral(const expr_t *f, sbuf_t *b, int parent_prec);
void emit_func_integral(const expr_t *f, sbuf_t *b);

/* polynomial formatting. */
bool match_add_negative_complex_rhs(const expr_t *expr, const expr_t **base_out, const expr_t **complex_out);
bool match_additive_complex_shift(const expr_t *expr, const expr_t **base_out, const expr_t **real_out,
                                         const expr_t **imag_out);
bool display_poly_is_indexed_arbitrary_constant(const expr_t *expr);
bool display_poly_contains_indexed_arbitrary_constant(const expr_t *expr);
const expr_t *display_series_remainder(const expr_t *expr);
bool display_sum_has_transform(const expr_t *expr);
bool emit_expr_display_polynomial_sum(const expr_t *expr, sbuf_t *b, int parent_prec);
bool emit_TeX_display_polynomial_sum(const expr_t *expr, sbuf_t *b, int parent_prec);
bool emit_func_display_polynomial_sum(const expr_t *expr, sbuf_t *b, int parent_prec);

/* special formatting. */
void emit_TeX_number_value(sbuf_t *b, number_t value);
void emit_TeX_const_value(sbuf_t *b, const expr_t *expr);
bool emit_TeX_exp_unit_fraction_root(const expr_t *arg, sbuf_t *b);
void emit_TeX_atom(const expr_t *f, sbuf_t *b);
const char *TeX_unary_name(const expr_t *f);
bool TeX_unary_has_bare_greek_argument(const expr_t *function);
const char *expr_unary_name(const expr_t *f);
const char *expr_cylindrical_symbol(const expr_t *f);
int expr_has_polygamma_order(const expr_t *f);
int expr_has_polylog_order(const expr_t *f);
int expr_has_legendre_chi_order(const expr_t *f);
void emit_expr_lambert_wn(const expr_t *f, sbuf_t *b);
void emit_expr_polygamma(const expr_t *f, sbuf_t *b);
void emit_expr_polylog(const expr_t *f, sbuf_t *b);
void emit_expr_legendre_chi(const expr_t *f, sbuf_t *b);
void emit_expr_appell_f1(const expr_t *f, sbuf_t *b);
void emit_expr_lauricella_f(const expr_t *f, sbuf_t *out);
void emit_expr_hypergeometric_pFq(const expr_t *f, sbuf_t *b);
void emit_expr_lommel_s(const expr_t *f, sbuf_t *b);
void emit_TeX_polygamma(const expr_t *f, sbuf_t *b);
void emit_TeX_polylog(const expr_t *f, sbuf_t *b);
void emit_TeX_harmonic_poly(const expr_t *f, sbuf_t *b);
void emit_TeX_legendre_chi(const expr_t *f, sbuf_t *b);
void emit_TeX_appell_f1(const expr_t *f, sbuf_t *b);
void emit_TeX_lauricella_f(const expr_t *f, sbuf_t *out);
void emit_TeX_hypergeometric_pFq(const expr_t *f, sbuf_t *b);
void emit_TeX_lommel_s(const expr_t *f, sbuf_t *b);
void emit_TeX_lambert_wn(const expr_t *f, sbuf_t *b);
void emit_func_polygamma(const expr_t *f, sbuf_t *b);
void emit_func_appell_f1(const expr_t *f, sbuf_t *b);
void emit_func_lauricella_f(const expr_t *f, sbuf_t *out);
void emit_func_hypergeometric_pFq(const expr_t *f, sbuf_t *b);
void emit_func_lommel_s(const expr_t *f, sbuf_t *b);
void emit_func_lambert_wn(const expr_t *f, sbuf_t *b);
void emit_function_builtin_name(sbuf_t *b, const expr_ops_t *ops);

/* TeX emit formatting. */
bool expr_TeX_source_order_preserved(void);
bool expr_is_rendered_log_local(const expr_t *expr);
void emit_TeX_expr_abs(const expr_t *f, sbuf_t *b, int parent_prec);
bool emit_expr_abs_needs_visible_add_parens(const expr_t *expr);
char *expr_to_TeX_operation_body(const expr_t *expr);
char *expr_to_TeX_body_ordered(const expr_t *expr, bool partial);

/* expr emit formatting. */
void emit_expr_abs(const expr_t *f, sbuf_t *b, int parent_prec);
void emit_expr_abs_bars(const expr_t *f, sbuf_t *b);

/* func emit formatting. */
void emit_func_abs(const expr_t *f, sbuf_t *b, int parent_prec);

#endif /* EXPR_STRINGOUT_INTERNAL_H */
