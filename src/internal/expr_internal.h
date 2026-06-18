#ifndef EXPR_SHARED_INTERNAL_H
#define EXPR_SHARED_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "dictionary.h"
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

typedef struct {
    string_t *name;
    expr_t *node;
} sym_t;

typedef struct {
    sym_t *entries;
    int count;
    int cap;
} symtab_t;

typedef struct {
    string_t *name;
    expr_t *expr;
    bool is_constant;
} expr_binding_entry_t;

struct expr_bindings_t {
    size_t count;
    expr_binding_entry_t *entries;
    dictionary_t *index;
};

typedef struct binding_exact_complex {
    number_t real;
    number_t imag;
} binding_exact_complex_t;

bool expr_is_exact_zero(const expr_t *dv);
bool expr_is_named_const(const expr_t *dv);
int expr_get_default_constant_num(const char *name, number_t *value_out);
int expr_get_default_constant_num_text(const string_t *name, number_t *value_out);
expr_t *expr_substitute(const expr_t *expr, const expr_t *needle, const expr_t *replacement);
expr_t *expr_clone(const expr_t *expr);
expr_t *expr_const_zero(void);
expr_t *expr_const_one(void);
expr_t *expr_const_long(long value);
expr_t *expr_retain_expr(const expr_t *expr);
expr_t *expr_simplify_owned(expr_t *expr);
expr_t *expr_negate_owned(expr_t *expr);
expr_t *expr_add_owned(expr_t *left, expr_t *right);
expr_t *expr_add_long(const expr_t *expr, long value);
expr_t *expr_mul_long(const expr_t *expr, long value);
expr_t *expr_div_long(const expr_t *expr, long value);
expr_t *expr_pow_long(const expr_t *expr, long exponent);
expr_t *expr_add_simplify_owned(const expr_t *left, const expr_t *right);
expr_t *expr_sub_simplify_owned(const expr_t *left, const expr_t *right);
expr_t *expr_mul_simplify_owned(const expr_t *left, const expr_t *right);
expr_t *expr_div_simplify_owned(const expr_t *left, const expr_t *right);
int expr_struct_eq(const expr_t *u, const expr_t *v);

bool expr_match_neg_expr(const expr_t *expr, const expr_t **arg_out);
bool expr_match_exp_expr(const expr_t *expr, const expr_t **arg_out);
bool expr_match_log_expr(const expr_t *expr, const expr_t **arg_out);
bool expr_match_sin_expr(const expr_t *expr, const expr_t **arg_out);
bool expr_match_cos_expr(const expr_t *expr, const expr_t **arg_out);
bool expr_match_tan_expr(const expr_t *expr, const expr_t **arg_out);
bool expr_match_add_expr(const expr_t *expr,
                         const expr_t **left_out,
                         const expr_t **right_out);
bool expr_match_sub_expr(const expr_t *expr,
                         const expr_t **left_out,
                         const expr_t **right_out);
bool expr_match_pow_const(const expr_t *expr,
                          const expr_t **base_out,
                          number_t *exponent_out);
bool expr_match_pow_expr(const expr_t *expr,
                         const expr_t **base_out,
                         const expr_t **exponent_out);
bool expr_match_integral_expr(const expr_t *expr,
                              const expr_t **integrand_out,
                              const expr_t **domain_out);
bool expr_child_exprs(const expr_t *expr,
                      const expr_t **left_out,
                      const expr_t **right_out);
const expr_t *expr_integral_dummy_expr(const expr_t *integral);
const expr_t *expr_integral_upper_bound_expr(const expr_t *integral);
const expr_t *expr_integral_lower_bound_expr(const expr_t *integral);
const char *expr_symbol_name(const expr_t *expr);
void expr_set_binding_pi_linear_family(expr_t *expr,
                                       long denominator,
                                       long n_coeff,
                                       long offset);

bool expr_match_const_value(const expr_t *expr, number_t *value_out);
bool expr_match_var_expr(const expr_t *expr,
                         size_t nvars,
                         expr_t *const *vars,
                         size_t *index_out);
bool expr_match_scaled_expr(const expr_t *expr,
                            number_t *scale_out,
                            const expr_t **base_out);
bool expr_match_add_sub_expr(const expr_t *expr,
                             const expr_t **left_out,
                             const expr_t **right_out,
                             bool *is_sub_out);
bool expr_match_mul_expr(const expr_t *expr,
                         const expr_t **left_out,
                         const expr_t **right_out);
bool expr_collect_var_usage(const expr_t *expr,
                            size_t nvars,
                            expr_t *const *vars,
                            bool *used_out);
bool expr_has_unbound_parameters(const expr_t *expr,
                                 size_t nvars,
                                 expr_t *const *vars);
expr_t *expr_integrate_iterated(const expr_t *integrand,
                                size_t ndim,
                                expr_t *const *vars,
                                const expr_integration_bound_kind_t *kinds,
                                expr_t *const *lo,
                                expr_t *const *hi,
                                size_t max_steps,
                                size_t *completed_steps_out,
                                expr_t **first_antiderivative_out);
expr_t *expr_integrate_iterated_best_effort(
    const expr_t *integrand,
    size_t ndim,
    expr_t *const *vars,
    const expr_integration_bound_kind_t *kinds,
    expr_t *const *lo,
    expr_t *const *hi,
    size_t *completed_steps_out,
    size_t *remaining_ndim_out,
    expr_t **remaining_vars_out,
    number_t *remaining_lo_num_out,
    number_t *remaining_hi_num_out,
    const number_t *lo_num,
    const number_t *hi_num);

bool expr_match_affine_poly_deg0(const expr_t *expr,
                                 size_t nvars,
                                 expr_t *const *vars,
                                 number_t *constant_out);
bool expr_match_affine_poly_deg1(const expr_t *expr,
                                 size_t nvars,
                                 expr_t *const *vars,
                                 number_t *constant_out,
                                 number_t *coeffs_out);
bool expr_match_affine_poly_deg2(const expr_t *expr,
                                 size_t nvars,
                                 expr_t *const *vars,
                                 number_t *poly_coeffs_out,
                                 number_t *constant_out,
                                 number_t *coeffs_out);
bool expr_match_affine_poly_deg3(const expr_t *expr,
                                 size_t nvars,
                                 expr_t *const *vars,
                                 number_t *poly_coeffs_out,
                                 number_t *constant_out,
                                 number_t *coeffs_out);
bool expr_match_affine_poly_deg4(const expr_t *expr,
                                 size_t nvars,
                                 expr_t *const *vars,
                                 number_t *poly_coeffs_out,
                                 number_t *constant_out,
                                 number_t *coeffs_out);
bool expr_match_unary_affine_kind(const expr_t *expr,
                                  expr_pattern_unary_affine_kind_t kind,
                                  size_t nvars,
                                  expr_t *const *vars,
                                  number_t *constant_out,
                                  number_t *coeffs_out);
bool expr_match_affine_poly_deg1_times_unary_affine_kind(
    const expr_t *expr,
    expr_pattern_unary_affine_kind_t unary_kind,
    size_t nvars,
    expr_t *const *vars,
    number_t *constant_out,
    number_t *coeffs_out);
bool expr_match_affine_poly_deg2_times_unary_affine_kind(
    const expr_t *expr,
    expr_pattern_unary_affine_kind_t unary_kind,
    size_t nvars,
    expr_t *const *vars,
    number_t *poly_coeffs_out,
    number_t *constant_out,
    number_t *coeffs_out);
bool expr_match_affine_poly_deg3_times_unary_affine_kind(
    const expr_t *expr,
    expr_pattern_unary_affine_kind_t unary_kind,
    size_t nvars,
    expr_t *const *vars,
    number_t *poly_coeffs_out,
    number_t *constant_out,
    number_t *coeffs_out);
bool expr_match_affine_poly_deg4_times_unary_affine_kind(
    const expr_t *expr,
    expr_pattern_unary_affine_kind_t unary_kind,
    size_t nvars,
    expr_t *const *vars,
    number_t *poly_coeffs_out,
    number_t *constant_out,
    number_t *coeffs_out);

char *expr_normalise_name(const char *name);
string_t *expr_normalise_name_text(const string_t *name);
char *expr_take_string_as_c_string(string_t *text);
char *expr_normalise_binding_name(const char *name);
string_t *expr_normalise_binding_name_text(const string_t *name);
size_t expr_match_leading_greek_alias_len(const string_cursor_t *cursor,
                                          string_pos_t pos);
int expr_is_default_constant_name(const char *name);
int expr_is_default_constant_name_text(const string_t *name);
const char *expr_default_constant_canonical_name(const char *name);
string_t *expr_default_constant_canonical_name_text(const string_t *name);
char *expr_tostring_texify(const char *text);
int expr_to_tex_parts(const expr_t *dv, char **expr_out, char **bindings_out);
void *fs_xmalloc(size_t n);
int fs_is_letter(unsigned int c);
void symtab_init(symtab_t *t);
int symtab_has_text(const symtab_t *t, const string_t *name);
void symtab_add_text(symtab_t *t, const string_t *name, expr_t *node);
expr_t *symtab_lookup_text(const symtab_t *t, const string_t *name);
void symtab_free(symtab_t *t);
int symtab_add_borrowed_text(symtab_t *t, const string_t *name, expr_t *node);
expr_bindings_t *symtab_build_bindings(const symtab_t *t);
expr_bindings_t *symtab_build_bindings_for_expr(const symtab_t *t,
                                                const expr_t *expr);
expr_bindings_t *single_binding_from_node(expr_t *node);

#endif /* EXPR_SHARED_INTERNAL_H */
