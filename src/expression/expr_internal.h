#ifndef EXPR_INTERNAL_H
#define EXPR_INTERNAL_H

#if !defined(MARS_EXPR_INTERNAL_ACCESS) &&                                                                             \
    (!defined(__INTELLISENSE__) || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "expr_internal.h is private to the expression module; include expression.h instead."
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dictionary.h"
#include "expression.h"
#include "qfloat.h"

/**
 * @file expr_internal.h
 * @brief Internal structures and operator vtables for the differentiable
 *        value DAG.
 *
 * This header defines the full internal representation of ::expr_t and the
 * operator vtables used by the lazy evaluation and automatic differentiation
 * engine. It is not intended for public use; external code should include
 * only expression.h.
 *
 * Ownership model (internal summary):
 *   • Every expr_t is a reference-counted node.
 *   • Arithmetic builders retain their children; they never steal ownership.
 *   • expr_get_deriv() returns a borrowed pointer to f->dx.
 *   • expr_create_* functions produce owning handles.
 */

typedef enum { EXPR_OP_ATOM, EXPR_OP_UNARY, EXPR_OP_BINARY } expr_arity_t;

typedef enum {
    EXPR_KIND_CONST,
    EXPR_KIND_VAR,
    EXPR_KIND_ADD,
    EXPR_KIND_SUB,
    EXPR_KIND_MUL,
    EXPR_KIND_DIV,
    EXPR_KIND_POW,
    EXPR_KIND_POW_D,
    EXPR_KIND_ATAN2,
    EXPR_KIND_NEG,
    EXPR_KIND_SIN,
    EXPR_KIND_COS,
    EXPR_KIND_TAN,
    EXPR_KIND_SEC,
    EXPR_KIND_COSEC,
    EXPR_KIND_COT,
    EXPR_KIND_SINH,
    EXPR_KIND_COSH,
    EXPR_KIND_TANH,
    EXPR_KIND_SECH,
    EXPR_KIND_COSECH,
    EXPR_KIND_COTH,
    EXPR_KIND_ASIN,
    EXPR_KIND_ACOS,
    EXPR_KIND_ATAN,
    EXPR_KIND_ASEC,
    EXPR_KIND_ACOSEC,
    EXPR_KIND_ACOT,
    EXPR_KIND_ASINH,
    EXPR_KIND_ACOSH,
    EXPR_KIND_ATANH,
    EXPR_KIND_ASECH,
    EXPR_KIND_ACOSECH,
    EXPR_KIND_ACOTH,
    EXPR_KIND_EXP,
    EXPR_KIND_LOG,
    EXPR_KIND_LOG10,
    EXPR_KIND_SQRT,
    EXPR_KIND_FLOOR,
    EXPR_KIND_CEIL,
    EXPR_KIND_ABS,
    EXPR_KIND_CONJ,
    EXPR_KIND_HYPOT,
    EXPR_KIND_ERF,
    EXPR_KIND_ERFC,
    EXPR_KIND_LGAMMA,
    EXPR_KIND_ERFINV,
    EXPR_KIND_ERFCINV,
    EXPR_KIND_GAMMA,
    EXPR_KIND_DIGAMMA,
    EXPR_KIND_TRIGAMMA,
    EXPR_KIND_GAMMAINV,
    EXPR_KIND_LAMBERT_W,
    EXPR_KIND_LAMBERT_WN,
    EXPR_KIND_LAMBERT_W0,
    EXPR_KIND_LAMBERT_WM1,
    EXPR_KIND_NORMAL_PDF,
    EXPR_KIND_NORMAL_CDF,
    EXPR_KIND_NORMAL_LOGPDF,
    EXPR_KIND_EI,
    EXPR_KIND_E1,
    EXPR_KIND_DILOG,
    EXPR_KIND_POLYLOG,
    EXPR_KIND_LEGENDRE_CHI,
    EXPR_KIND_BESSEL_J,
    EXPR_KIND_BESSEL_Y,
    EXPR_KIND_LOMMEL_S,
    EXPR_KIND_LOMMEL_S_PACK,
    EXPR_KIND_APPELL_F1,
    EXPR_KIND_APPELL_F1_PACK,
    EXPR_KIND_LAURICELLA_F,
    EXPR_KIND_HYPERGEOMETRIC_PFQ,
    EXPR_KIND_HYPERGEOMETRIC_PFQ_PACK,
    EXPR_KIND_VERSIN,
    EXPR_KIND_VERCOS,
    EXPR_KIND_COVERSIN,
    EXPR_KIND_COVERCOS,
    EXPR_KIND_HAVERSIN,
    EXPR_KIND_HAVERCOS,
    EXPR_KIND_HACOVERSIN,
    EXPR_KIND_HACOVERCOS,
    EXPR_KIND_ARCVERSIN,
    EXPR_KIND_ARCVERCOS,
    EXPR_KIND_ARCCOVERSIN,
    EXPR_KIND_ARCCOVERCOS,
    EXPR_KIND_ARCHAVERSIN,
    EXPR_KIND_ARCHAVERCOS,
    EXPR_KIND_ARCHACOVERSIN,
    EXPR_KIND_ARCHACOVERCOS,
    EXPR_KIND_BETA,
    EXPR_KIND_LOGBETA,
    EXPR_KIND_GAMMAINC_LOWER,
    EXPR_KIND_GAMMAINC_UPPER,
    EXPR_KIND_GAMMAINC_P,
    EXPR_KIND_GAMMAINC_Q,
    EXPR_KIND_POLYGAMMA,
    EXPR_KIND_FACTORIAL,
    EXPR_KIND_FIBONACCI,
    EXPR_KIND_PARTITION,
    EXPR_KIND_ISQRT,
    EXPR_KIND_GCD,
    EXPR_KIND_LCM,
    EXPR_KIND_MOD,
    EXPR_KIND_MODINV,
    EXPR_KIND_IS_PRIME,
    EXPR_KIND_NEXT_PRIME,
    EXPR_KIND_PREV_PRIME,
    EXPR_KIND_BIT_AND,
    EXPR_KIND_BIT_OR,
    EXPR_KIND_BIT_XOR,
    EXPR_KIND_BIT_NOT,
    EXPR_KIND_SHL,
    EXPR_KIND_SHR,
    EXPR_KIND_FACTORS,
    EXPR_KIND_INTEGRAL,
    EXPR_KIND_INTEGRAL_META,
    EXPR_KIND_INTEGRAL_BOUNDS,
    EXPR_KIND_INDEXED_SYMBOL,
    EXPR_KIND_SUMMATION,
    EXPR_KIND_FORMAL_DERIVATIVE,
    EXPR_KIND_ARBITRARY_FUNCTION,
    EXPR_KIND_ARGUMENT_LIST,
    EXPR_KIND_COUNT
} expr_op_kind_t;

typedef enum expr_diff_kind { EXPR_DIFF_SMOOTH = 0, EXPR_DIFF_NONE } expr_diff_kind_t;

typedef void (*expr_reverse_fn)(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
typedef int (*expr_reverse_accumulate_fn)(void *context, const expr_t *child, const number_t *child_bar);
typedef int (*expr_reverse_many_fn)(const expr_t *dv, const number_t *out_bar, expr_reverse_accumulate_fn accumulate,
                                    void *context);
typedef expr_t *(*expr_apply_unary_fn)(const expr_t *arg);
typedef expr_t *(*expr_apply_binary_fn)(const expr_t *left, const expr_t *right);
typedef expr_t *(*expr_simplify_fn)(const expr_t *tmpl, expr_t *a, expr_t *b);
typedef int (*expr_fold_const_unary_fn)(const number_t *in, number_t *out);
typedef expr_t *(*expr_inverse_unary_fn)(const expr_t *arg);
typedef expr_t *(*expr_integrate_fn)(const expr_t *expr, const expr_t *wrt);

static inline number_t expr_reverse_num_sq(const number_t value)
{
    return num_mul(value, value);
}

static inline number_t expr_reverse_num_inverse(const number_t value)
{
    return num_div(NUM_ONE, value);
}

static inline number_t expr_reverse_num_clone(const number_t value)
{
    return num_clone(value);
}

static inline number_t expr_reverse_num_neg(const number_t value)
{
    return num_neg(value);
}

static inline number_t expr_reverse_num_mul(const number_t a, const number_t b)
{
    return num_mul(a, b);
}

static inline number_t expr_reverse_num_div(const number_t a, const number_t b)
{
    return num_div(a, b);
}

typedef struct expr_ops {
    number_t (*eval)(expr_t *dv);
    expr_t *(*deriv)(expr_t *dv);
    expr_reverse_fn reverse;
    expr_reverse_many_fn reverse_many;
    expr_op_kind_t kind;
    expr_arity_t arity;
    expr_diff_kind_t diff_kind;
    const char *name;
    const char *TeX_name;
    const struct expr_ops *direct_inverse;
    expr_inverse_unary_fn inverse_unary;
    expr_apply_unary_fn apply_unary;
    expr_apply_binary_fn apply_binary;
    expr_integrate_fn integrate;
    expr_simplify_fn simplify;
    expr_fold_const_unary_fn fold_const_unary;
} expr_ops_t;

typedef struct expr_deriv_cache {
    uint64_t wrt_id;
    expr_t *dx;
    struct expr_deriv_cache *next;
} expr_deriv_cache_t;

typedef struct {
    expr_t *base;
    number_t coeff;
} addend_t;

typedef enum {
    EXPR_BINDING_EXPR_NUMBER,
    EXPR_BINDING_EXPR_CONST,
    EXPR_BINDING_EXPR_NEG,
    EXPR_BINDING_EXPR_ADD,
    EXPR_BINDING_EXPR_SUB,
    EXPR_BINDING_EXPR_MUL,
    EXPR_BINDING_EXPR_DIV,
    EXPR_BINDING_EXPR_POWI,
    EXPR_BINDING_EXPR_UNARY_OP,
    EXPR_BINDING_EXPR_BINARY_OP
} expr_binding_expr_kind_t;

typedef enum {
    EXPR_BINDING_CONST_E,
    EXPR_BINDING_CONST_I,
    EXPR_BINDING_CONST_PI,
    EXPR_BINDING_CONST_PHI,
    EXPR_BINDING_CONST_GAMMA
} expr_binding_const_id_t;

typedef struct expr_binding_expr {
    expr_binding_expr_kind_t kind;
    number_t cached_value;
    size_t cached_precision_bits;
    bool cached_value_valid;
    union {
        char *text;
        expr_binding_const_id_t const_id;
        struct {
            struct expr_binding_expr *child;
        } unary;
        struct {
            struct expr_binding_expr *left;
            struct expr_binding_expr *right;
        } binary;
        struct {
            struct expr_binding_expr *base;
            long exponent;
        } powi;
        struct {
            const expr_ops_t *ops;
            struct expr_binding_expr *child;
        } unary_op;
        struct {
            const expr_ops_t *ops;
            struct expr_binding_expr *left;
            struct expr_binding_expr *right;
        } binary_op;
    } u;
} expr_binding_expr_t;

struct _expr_t {
    const expr_ops_t *ops;
    expr_t *a;
    expr_t *b;
    number_t c;
    number_t x;
    int x_valid;
    uint64_t epoch;
    bool simplified;
    uint64_t simplify_epoch;
    expr_deriv_cache_t *dx_cache;
    char *name;
    expr_binding_expr_t *binding_expr;
    expr_t **formal_wrts;
    size_t formal_wrt_count;
    int refcount;
    uint64_t var_id;
};

extern const expr_ops_t ops_const;
extern const expr_ops_t ops_var;
extern const expr_ops_t ops_add;
extern const expr_ops_t ops_sub;
extern const expr_ops_t ops_mul;
extern const expr_ops_t ops_div;
extern const expr_ops_t ops_pow;
extern const expr_ops_t ops_pow_d;
extern const expr_ops_t ops_atan2;
extern const expr_ops_t ops_neg;
extern const expr_ops_t ops_integral;
extern const expr_ops_t ops_integral_meta;
extern const expr_ops_t ops_integral_bounds;
extern const expr_ops_t ops_indexed_symbol;
extern const expr_ops_t ops_summation;
extern const expr_ops_t ops_formal_derivative;
extern const expr_ops_t ops_arbitrary_function;
extern const expr_ops_t ops_argument_list;

expr_t *expr_new_indexed_symbol(const char *name, const expr_t *index);
expr_t *expr_new_summation(const expr_t *term, const expr_t *index);
expr_t *expr_new_finite_summation(const expr_t *term, const expr_t *index, const expr_t *upper);

expr_t *expr_new_formal_derivative(const expr_t *dependent, size_t wrt_count, expr_t *const *wrts);
bool expr_is_formal_derivative(const expr_t *expr);
const expr_t *expr_formal_derivative_dependent(const expr_t *expr);
size_t expr_formal_derivative_order(const expr_t *expr);
const expr_t *expr_formal_derivative_wrt_at(const expr_t *expr, size_t index);
expr_t *expr_new_arbitrary_function(const char *name, const expr_t *argument);
expr_t *expr_new_arbitrary_function_n(const char *name, size_t argument_count, expr_t *const *arguments);
bool expr_is_arbitrary_function(const expr_t *expr);
extern const expr_ops_t ops_sin;
extern const expr_ops_t ops_cos;
extern const expr_ops_t ops_tan;
extern const expr_ops_t ops_sec;
extern const expr_ops_t ops_cosec;
extern const expr_ops_t ops_cot;
extern const expr_ops_t ops_versin;
extern const expr_ops_t ops_vercos;
extern const expr_ops_t ops_coversin;
extern const expr_ops_t ops_covercos;
extern const expr_ops_t ops_haversin;
extern const expr_ops_t ops_havercos;
extern const expr_ops_t ops_hacoversin;
extern const expr_ops_t ops_hacovercos;
extern const expr_ops_t ops_sinh;
extern const expr_ops_t ops_cosh;
extern const expr_ops_t ops_tanh;
extern const expr_ops_t ops_sech;
extern const expr_ops_t ops_cosech;
extern const expr_ops_t ops_coth;
extern const expr_ops_t ops_asin;
extern const expr_ops_t ops_acos;
extern const expr_ops_t ops_atan;
extern const expr_ops_t ops_asec;
extern const expr_ops_t ops_acosec;
extern const expr_ops_t ops_acot;
extern const expr_ops_t ops_arcversin;
extern const expr_ops_t ops_arcvercos;
extern const expr_ops_t ops_arccoversin;
extern const expr_ops_t ops_arccovercos;
extern const expr_ops_t ops_archaversin;
extern const expr_ops_t ops_archavercos;
extern const expr_ops_t ops_archacoversin;
extern const expr_ops_t ops_archacovercos;
extern const expr_ops_t ops_asinh;
extern const expr_ops_t ops_acosh;
extern const expr_ops_t ops_atanh;
extern const expr_ops_t ops_asech;
extern const expr_ops_t ops_acosech;
extern const expr_ops_t ops_acoth;
extern const expr_ops_t ops_exp;
extern const expr_ops_t ops_log;
extern const expr_ops_t ops_log10;
extern const expr_ops_t ops_sqrt;
extern const expr_ops_t ops_floor;
extern const expr_ops_t ops_ceil;
extern const expr_ops_t ops_abs;
extern const expr_ops_t ops_conj;
extern const expr_ops_t ops_hypot;
extern const expr_ops_t ops_erf;
extern const expr_ops_t ops_erfc;
extern const expr_ops_t ops_lgamma;
extern const expr_ops_t ops_erfinv;
extern const expr_ops_t ops_erfcinv;
extern const expr_ops_t ops_gamma;
extern const expr_ops_t ops_digamma;
extern const expr_ops_t ops_trigamma;
extern const expr_ops_t ops_dilog;
extern const expr_ops_t ops_polylog;
extern const expr_ops_t ops_legendre_chi;
extern const expr_ops_t ops_bessel_j;
extern const expr_ops_t ops_bessel_y;
extern const expr_ops_t ops_lommel_s;
extern const expr_ops_t ops_lommel_s_pack;
extern const expr_ops_t ops_appell_f1;
extern const expr_ops_t ops_appell_f1_pack;
extern const expr_ops_t ops_lauricella_f;
extern const expr_ops_t ops_hypergeometric_pFq;
extern const expr_ops_t ops_hypergeometric_pFq_pack;
extern const expr_ops_t ops_gammainv;
extern const expr_ops_t ops_lambert_w;
extern const expr_ops_t ops_lambert_wn;
extern const expr_ops_t ops_lambert_w0;
extern const expr_ops_t ops_lambert_wm1;
extern const expr_ops_t ops_normal_pdf;
extern const expr_ops_t ops_normal_cdf;
extern const expr_ops_t ops_normal_logpdf;
extern const expr_ops_t ops_pdf;
extern const expr_ops_t ops_cdf;
extern const expr_ops_t ops_logpdf;
extern const expr_ops_t ops_ei;
extern const expr_ops_t ops_e1;
extern const expr_ops_t ops_beta;
extern const expr_ops_t ops_logbeta;
extern const expr_ops_t ops_gammainc_lower;
extern const expr_ops_t ops_gammainc_upper;
extern const expr_ops_t ops_gammainc_P;
extern const expr_ops_t ops_gammainc_Q;
extern const expr_ops_t ops_polygamma;
extern const expr_ops_t ops_factorial;
extern const expr_ops_t ops_fibonacci;
extern const expr_ops_t ops_partition;
extern const expr_ops_t ops_isqrt;
extern const expr_ops_t ops_gcd;
extern const expr_ops_t ops_lcm;
extern const expr_ops_t ops_mod;
extern const expr_ops_t ops_modinv;
extern const expr_ops_t ops_is_prime;
extern const expr_ops_t ops_next_prime;
extern const expr_ops_t ops_prev_prime;
extern const expr_ops_t ops_bit_and;
extern const expr_ops_t ops_bit_or;
extern const expr_ops_t ops_bit_xor;
extern const expr_ops_t ops_bit_not;
extern const expr_ops_t ops_shl;
extern const expr_ops_t ops_shr;
extern const expr_ops_t ops_factors;

static inline int expr_const_is_zero(const expr_t *dv)
{
    return dv && dv->ops == &ops_const && num_eq(dv->c, NUM_ZERO);
}

static inline int expr_const_is_one(const expr_t *dv)
{
    return dv && dv->ops == &ops_const && num_eq(dv->c, NUM_ONE);
}

static inline int expr_const_is_minus_one(const expr_t *dv)
{
    return dv && dv->ops == &ops_const && num_eq(dv->c, NUM_NEG_ONE);
}

static inline int expr_is_op(const expr_t *dv, const expr_ops_t *ops)
{
    return dv && dv->ops == ops;
}

static inline int expr_is_const(const expr_t *dv)
{
    return expr_is_op(dv, &ops_const);
}

static inline int expr_is_var(const expr_t *dv)
{
    return expr_is_op(dv, &ops_var);
}

static inline int expr_is_neg(const expr_t *dv)
{
    return expr_is_op(dv, &ops_neg);
}

static inline int expr_is_mul(const expr_t *dv)
{
    return expr_is_op(dv, &ops_mul);
}

static inline int expr_is_div(const expr_t *dv)
{
    return expr_is_op(dv, &ops_div);
}

static inline int expr_is_integral_bounds(const expr_t *dv)
{
    return expr_is_op(dv, &ops_integral_bounds);
}

static inline int expr_is_integral_meta(const expr_t *dv)
{
    return expr_is_op(dv, &ops_integral_meta);
}

static inline int expr_is_addsub(const expr_t *dv)
{
    return expr_is_op(dv, &ops_add) || expr_is_op(dv, &ops_sub);
}

static inline int expr_is_exp_expr(const expr_t *dv)
{
    return expr_is_op(dv, &ops_exp);
}

static inline int expr_is_sqrt_expr(const expr_t *dv)
{
    return dv && dv->ops && dv->ops->kind == EXPR_KIND_SQRT;
}

static inline int expr_is_pow_d_expr(const expr_t *dv)
{
    return dv && dv->ops && dv->ops->kind == EXPR_KIND_POW_D;
}

static inline int expr_is_unnamed_const(const expr_t *dv)
{
    return expr_is_const(dv) && (!dv->name || !*dv->name);
}

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
    bool has_symbolic_derivative;
    bool has_symbolic_integral;
};

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

/* Core construction and evaluation helpers. */
expr_t *expr_alloc(const expr_ops_t *ops);
expr_t *expr_make_const_num(number_t x);
expr_t *expr_make_var_num(number_t x);
int expr_get_default_constant_num(const char *name, number_t *value_out);
int expr_get_default_constant_num_text(const string_t *name, number_t *value_out);
void expr_store_const_num(expr_t *dv, number_t value);
void expr_store_value_num(expr_t *dv, number_t value);
number_t expr_eval_num_internal(const expr_t *dv);
expr_t *expr_get_dx_internal(const expr_t *dv);
const expr_t *expr_current_wrt_internal(void);
expr_t *expr_deriv_rational_over_polynomial_power(const expr_t *expr, const expr_t *wrt);
expr_t *expr_new_unary_internal(const expr_ops_t *ops, const expr_t *a);
expr_t *expr_new_binary_internal(const expr_ops_t *ops, const expr_t *a, const expr_t *b);
expr_t *expr_new_pow_const_internal(const expr_t *a, number_t exponent);
expr_t *expr_integral_with_dummy_internal(const expr_t *integrand, const expr_t *upper, const expr_t *dummy);
expr_t *expr_integral_with_bounds_internal(const expr_t *integrand, const expr_t *lower, const expr_t *upper,
                                           const expr_t *dummy);
expr_t *expr_polygamma_xp(const expr_t *order, const expr_t *arg);
expr_t *expr_polylog_xp(const expr_t *order, const expr_t *arg);
expr_t *expr_legendre_chi_xp(const expr_t *order, const expr_t *arg);
expr_t *expr_lambert_wn_xp(const expr_t *branch, const expr_t *arg);
bool expr_lommel_s_unpack(const expr_t *expr, const expr_t **mu, const expr_t **nu, const expr_t **argument);
bool expr_appell_f1_unpack(const expr_t *expr, const expr_t **a, const expr_t **b1, const expr_t **b2, const expr_t **c,
                           const expr_t **x, const expr_t **y);
bool expr_lauricella_f_unpack(const expr_t *expr, const expr_t **a, const expr_t ***b, const expr_t **c,
                              const expr_t ***x, size_t *variable_count);
bool expr_hypergeometric_pFq_unpack(const expr_t *expr, const expr_t ***upper, size_t *upper_count,
                                    const expr_t ***lower, size_t *lower_count, const expr_t **argument);

/* Simplification helpers. */
expr_t *expr_simplify_passthrough(const expr_t *dv, expr_t *a, expr_t *b);
expr_t *expr_simplify_rebuild_binary_operator(const expr_t *dv, expr_t *a, expr_t *b);
expr_t *expr_simplify_unary_operator(const expr_t *dv, expr_t *a, expr_t *b);
expr_t *expr_simplify_binary_operator(const expr_t *dv, expr_t *a, expr_t *b);
expr_t *expr_simplify_neg_operator(const expr_t *dv, expr_t *a, expr_t *b);
expr_t *expr_simplify_add_sub_operator(const expr_t *dv, expr_t *a, expr_t *b);
expr_t *expr_simplify_mul_operator(const expr_t *dv, expr_t *a, expr_t *b);
expr_t *expr_simplify_div_operator(const expr_t *dv, expr_t *a, expr_t *b);
expr_t *expr_simplify_pow_d_operator(const expr_t *dv, expr_t *a, expr_t *b);
expr_t *expr_simplify_pow_operator(const expr_t *dv, expr_t *a, expr_t *b);
expr_t *expr_simplify_hypot_operator(const expr_t *dv, expr_t *a, expr_t *b);
bool expr_simplify_is_plain_real_const(const expr_t *dv);
bool expr_simplify_try_get_plain_real_const(const expr_t *dv, number_t *out);
bool expr_simplify_is_simplifiable_const(const expr_t *dv);
bool expr_simplify_allows_const_identity_fold(const expr_t *dv);
number_t expr_simplify_normalise_simple_rational_coeff(number_t coeff);
expr_t *expr_simplify_positive_part_if_negative(expr_t *dv);
expr_t *expr_simplify_try_log10_power_of_ten(expr_t *arg);
expr_t *expr_simplify_try_floor_ceil_const(const expr_t *op, expr_t *arg);
expr_t *expr_simplify_try_unary_const_fold(const expr_t *op, expr_t *arg);
expr_t *expr_simplify_try_unary_const_value_fold(const expr_t *op, expr_t *arg);
expr_t *expr_simplify_try_sqrt_scaled_square_const(expr_t *arg);
expr_t *expr_simplify_try_sqrt_quotient(expr_t *num, expr_t *den);
expr_t *expr_simplify_direct_inverse_pair(const expr_t *outer, expr_t *inner);
expr_t *expr_simplify_direct_inverse_pair_from_raw(const expr_t *outer, const expr_t *raw_inner,
                                                   expr_t *simplified_inner);
bool expr_ops_is_lambert(const expr_ops_t *ops);
const expr_t *expr_lambert_arg(const expr_t *expr);
bool expr_ops_is_floor_or_ceil(const expr_ops_t *ops);
bool expr_ops_are_direct_inverse_pair(const expr_ops_t *outer, const expr_ops_t *inner);
const expr_ops_t *expr_ops_reciprocal_unary(const expr_ops_t *ops);

/* Integration and inverse-function simplification helpers. */
expr_t *expr_integrate_dispatch_primitive(const expr_t *expr, const expr_t *wrt);
expr_bindings_t *expr_bindings_from_expr_internal(const expr_t *expr);
expr_bindings_t *expr_bindings_merge_internal(const expr_bindings_t *bindings,
                                              const expr_bindings_t *additional_bindings);
bool expr_ops_has_inverse_unary_simplify_rule(const expr_ops_t *ops);
bool expr_inverse_unary_candidate_value_ok(const expr_ops_t *ops, number_t value);
expr_t *expr_simplify_try_vtable_inverse_argument(const expr_t *outer, const expr_t *arg);
expr_t *expr_simplify_try_basic_sum(expr_t *a, expr_t *b);
expr_t *expr_simplify_try_basic_product(expr_t *a, expr_t *b);
expr_t *expr_simplify_try_trig_product(expr_t *a, expr_t *b);
expr_t *expr_simplify_try_trig_weighted_sum(const expr_t *a, const expr_t *b);
expr_t *expr_simplify_try_lambert_exp(expr_t *arg);
expr_t *expr_simplify_try_lambert_argument(expr_t *arg);
expr_t *expr_simplify_try_lambert_product(expr_t *a, expr_t *b);
expr_t *expr_simplify_try_i_unit_product(expr_t *a, expr_t *b);
expr_t *expr_simplify_try_imag_trig_bridge(const expr_t *op, expr_t *arg);
bool expr_match_double_argument(const expr_t *expr, const expr_t *arg);
expr_t *expr_try_trig_pythagorean_identity(const addend_t *terms, size_t n, number_t c_const, number_t common_coeff);
bool expr_combine_trig_pythagorean_addends(addend_t *terms, size_t n);

/* Term collection and product rebuilding helpers. */
int expr_struct_eq(const expr_t *u, const expr_t *v);
bool expr_simplify_same_factor(const expr_t *left, const expr_t *right);
bool expr_simplify_additive_terms_equal(const expr_t *left, const expr_t *right);
expr_t *expr_simplify_extract_exact_factor_quotient(const expr_t *expr, const expr_t *factor);
expr_t *expr_simplify_extract_common_factor_quotient(const expr_t *expr, const expr_t *factor);
expr_t *expr_simplify_normalize_negated_mul_factor(const expr_t *expr);
expr_t *expr_make_scaled(number_t coeff, expr_t *base);
expr_t *expr_make_pow_like(expr_t *base, number_t exponent);
void expr_collect_addends(expr_t *dv, number_t scale, number_t *c_const, addend_t **terms, size_t *n, size_t *cap);
void expr_combine_common_denominator_addends(addend_t *terms, size_t n);
void expr_sort_addends(addend_t *terms, size_t n);
int expr_extract_common_addend_coeff(const addend_t *terms, size_t n, number_t c_const, number_t *common_out);
void expr_free_node_array(expr_t **nodes, size_t count);
void expr_append_node(expr_t ***nodes, size_t *count, size_t *cap, expr_t *node);
void expr_split_division_terms(number_t *c_acc, int *is_zero, expr_t **terms, size_t nterms, expr_t ***den_terms,
                               size_t *nden_terms, size_t *den_cap);
void expr_combine_like_powers(expr_t **terms, size_t nterms);
void expr_cancel_common_powers(expr_t **terms, size_t nterms, expr_t **den_terms, size_t nden_terms);
void expr_combine_exp_terms(expr_t **terms, size_t nterms);
void expr_merge_sqrt_terms(expr_t **terms, size_t nterms);
void expr_merge_coefficient_sqrt_terms(number_t *coefficient, expr_t **terms, size_t nterms);
void expr_merge_sqrt_quotient_terms(expr_t **terms, size_t nterms, expr_t **den_terms, size_t nden_terms);
expr_t *expr_try_expand_shallow_product(number_t c_acc, expr_t **terms, size_t nterms, expr_t **den_terms,
                                        size_t nden_terms);
expr_t *expr_rebuild_product_chain(number_t c_acc, expr_t **terms, size_t nterms);
expr_t *expr_rebuild_division_chain(expr_t **den_terms, size_t nden_terms);

/* Constant-fold hooks. */
int expr_fold_zero_to_zero(const number_t *in, number_t *out);
int expr_fold_cos_const(const number_t *in, number_t *out);
int expr_fold_asin_const(const number_t *in, number_t *out);
int expr_fold_acos_const(const number_t *in, number_t *out);
int expr_fold_atan_const(const number_t *in, number_t *out);
int expr_fold_asec_const(const number_t *in, number_t *out);
int expr_fold_acosec_const(const number_t *in, number_t *out);
int expr_fold_acot_const(const number_t *in, number_t *out);
int expr_inverse_trig_exact_pi_ratio(const expr_ops_t *ops, const number_t *in, long *numer_out,
                                     unsigned long *denom_out);
int expr_fold_exp_const(const number_t *in, number_t *out);
int expr_fold_log_const(const number_t *in, number_t *out);
int expr_fold_sqrt_const(const number_t *in, number_t *out);
int expr_fold_floor_const(const number_t *in, number_t *out);
int expr_fold_erf_const(const number_t *in, number_t *out);
int expr_fold_erfc_const(const number_t *in, number_t *out);

/* Reverse-mode local adjoint hooks. */
void expr_reverse_atom(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_add(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_sub(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_mul(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_div(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_pow(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_pow_d(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_atan2(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_neg(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_sin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_cos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_tan(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_sec(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_cosec(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_cot(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_versin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_vercos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_coversin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_covercos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_haversin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_havercos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_hacoversin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_hacovercos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_sinh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_cosh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_tanh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_sech(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_cosech(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_coth(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_asin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_acos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_atan(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_asec(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_acosec(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_acot(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_arcversin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_arcvercos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_arccoversin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_arccovercos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_archaversin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_archavercos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_archacoversin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_archacovercos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_asinh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_acosh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_atanh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_asech(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_acosech(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_acoth(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_exp(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_log(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_log10(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_sqrt(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_floor(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_ceil(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_abs(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_conj(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_hypot(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_erf(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_erfc(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_erfinv(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_erfcinv(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_gamma(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_lgamma(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_digamma(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_trigamma(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_polygamma(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_dilog(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_polylog(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_legendre_chi(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_bessel_j(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_bessel_y(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_lommel_s(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_parameter_pack(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_hypergeometric_pFq(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
int expr_reverse_appell_f1_many(const expr_t *dv, const number_t *out_bar, expr_reverse_accumulate_fn accumulate,
                                void *context);
int expr_reverse_lauricella_f_many(const expr_t *dv, const number_t *out_bar, expr_reverse_accumulate_fn accumulate,
                                   void *context);
void expr_reverse_gammainv(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_lambert_w(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_lambert_wn(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_lambert_w0(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_lambert_wm1(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_normal_pdf(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_normal_cdf(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_normal_logpdf(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_ei(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_e1(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_beta(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_logbeta(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_gammainc_lower(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_gammainc_upper(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_gammainc_P(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_gammainc_Q(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_not_differentiable(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);

bool expr_is_exact_zero(const expr_t *dv);
bool expr_is_named_const(const expr_t *dv);
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
bool expr_match_unary_op(const expr_t *expr, expr_op_kind_t kind, const expr_t **arg_out);
bool expr_match_unary_expr(const expr_t *expr, const expr_t **arg_out);
bool expr_match_cot_expr(const expr_t *expr, const expr_t **arg_out);
bool expr_match_binary_op(const expr_t *expr, expr_op_kind_t kind, const expr_t **left_out, const expr_t **right_out);
bool expr_match_pow_const(const expr_t *expr, const expr_t **base_out, number_t *exponent_out);
bool expr_match_pow_expr(const expr_t *expr, const expr_t **base_out, const expr_t **exponent_out);
bool expr_match_integral_expr(const expr_t *expr, const expr_t **integrand_out, const expr_t **domain_out);
bool expr_child_exprs(const expr_t *expr, const expr_t **left_out, const expr_t **right_out);
const expr_t *expr_integral_dummy_expr(const expr_t *integral);
const expr_t *expr_integral_upper_bound_expr(const expr_t *integral);
const expr_t *expr_integral_lower_bound_expr(const expr_t *integral);
const char *expr_symbol_name(const expr_t *expr);
void expr_set_binding_pi_linear_family(expr_t *expr, long denominator, long n_coeff, long offset);
expr_t *expr_apply_unary_kind(expr_op_kind_t kind, const expr_t *arg);
bool expr_match_const_value(const expr_t *expr, number_t *value_out);
bool expr_match_var_expr(const expr_t *expr, size_t nvars, expr_t *const *vars, size_t *index_out);
bool expr_match_scaled_expr(const expr_t *expr, number_t *scale_out, const expr_t **base_out);
bool expr_match_add_sub_expr(const expr_t *expr, const expr_t **left_out, const expr_t **right_out, bool *is_sub_out);
bool expr_match_mul_expr(const expr_t *expr, const expr_t **left_out, const expr_t **right_out);
bool expr_collect_var_usage(const expr_t *expr, size_t nvars, expr_t *const *vars, bool *used_out);
bool expr_has_unbound_parameters(const expr_t *expr, size_t nvars, expr_t *const *vars);
expr_t *expr_integrate_iterated(const expr_t *integrand, size_t ndim, expr_t *const *vars,
                                const expr_integration_bound_kind_t *kinds, expr_t *const *lo, expr_t *const *hi,
                                size_t max_steps, size_t *completed_steps_out, expr_t **first_antiderivative_out);
expr_t *expr_integrate_iterated_best_effort(const expr_t *integrand, size_t ndim, expr_t *const *vars,
                                            const expr_integration_bound_kind_t *kinds, expr_t *const *lo,
                                            expr_t *const *hi, size_t *completed_steps_out, size_t *remaining_ndim_out,
                                            expr_t **remaining_vars_out, number_t *remaining_lo_num_out,
                                            number_t *remaining_hi_num_out, const number_t *lo_num,
                                            const number_t *hi_num);
bool expr_match_affine_poly_deg0(const expr_t *expr, size_t nvars, expr_t *const *vars, number_t *constant_out);
bool expr_match_affine_poly_deg1(const expr_t *expr, size_t nvars, expr_t *const *vars, number_t *constant_out,
                                 number_t *coeffs_out);
bool expr_match_affine_poly_deg2(const expr_t *expr, size_t nvars, expr_t *const *vars, number_t *poly_coeffs_out,
                                 number_t *constant_out, number_t *coeffs_out);
bool expr_match_affine_poly_deg3(const expr_t *expr, size_t nvars, expr_t *const *vars, number_t *poly_coeffs_out,
                                 number_t *constant_out, number_t *coeffs_out);
bool expr_match_affine_poly_deg4(const expr_t *expr, size_t nvars, expr_t *const *vars, number_t *poly_coeffs_out,
                                 number_t *constant_out, number_t *coeffs_out);
bool expr_collect_single_var(const expr_t *expr, const expr_t **var_out);
bool expr_collect_poly_deg4(const expr_t *expr, const expr_t *var, number_t *coeffs_out);
bool expr_polynomials_equal_deg4(const expr_t *left, const expr_t *right);
bool expr_polynomial_is_zero_deg4(const expr_t *expr);
bool expr_match_unary_affine_kind(const expr_t *expr, expr_pattern_unary_affine_kind_t kind, size_t nvars,
                                  expr_t *const *vars, number_t *constant_out, number_t *coeffs_out);
bool expr_match_affine_poly_deg1_times_unary_affine_kind(const expr_t *expr,
                                                         expr_pattern_unary_affine_kind_t unary_kind, size_t nvars,
                                                         expr_t *const *vars, number_t *constant_out,
                                                         number_t *coeffs_out);
bool expr_match_affine_poly_deg2_times_unary_affine_kind(const expr_t *expr,
                                                         expr_pattern_unary_affine_kind_t unary_kind, size_t nvars,
                                                         expr_t *const *vars, number_t *poly_coeffs_out,
                                                         number_t *constant_out, number_t *coeffs_out);
bool expr_match_affine_poly_deg3_times_unary_affine_kind(const expr_t *expr,
                                                         expr_pattern_unary_affine_kind_t unary_kind, size_t nvars,
                                                         expr_t *const *vars, number_t *poly_coeffs_out,
                                                         number_t *constant_out, number_t *coeffs_out);
bool expr_match_affine_poly_deg4_times_unary_affine_kind(const expr_t *expr,
                                                         expr_pattern_unary_affine_kind_t unary_kind, size_t nvars,
                                                         expr_t *const *vars, number_t *poly_coeffs_out,
                                                         number_t *constant_out, number_t *coeffs_out);
char *expr_normalise_name(const char *name);
string_t *expr_normalise_name_text(const string_t *name);
string_t *expr_normalise_greek_alias_text(const string_t *alias);
char *expr_take_string_as_c_string(string_t *text);
char *expr_normalise_binding_name(const char *name);
string_t *expr_normalise_binding_name_text(const string_t *name);
size_t expr_match_leading_greek_alias_len(const string_cursor_t *cursor, string_pos_t pos);
int expr_is_default_constant_name(const char *name);
int expr_is_default_constant_name_text(const string_t *name);
const char *expr_default_constant_canonical_name(const char *name);
string_t *expr_default_constant_canonical_name_text(const string_t *name);
char *expr_tostring_texify(const char *text);
int expr_to_TeX_parts(const expr_t *dv, char **expr_out, char **bindings_out);
char *expr_to_TeX_body_wrapped(const expr_t *expr, size_t line_limit);
void *fs_xmalloc(size_t n);
int fs_is_letter(unsigned int c);
void symtab_init(symtab_t *t);
int symtab_has_text(const symtab_t *t, const string_t *name);
void symtab_add_text(symtab_t *t, const string_t *name, expr_t *node);
expr_t *symtab_lookup_text(const symtab_t *t, const string_t *name);
void symtab_free(symtab_t *t);
int symtab_add_borrowed_text(symtab_t *t, const string_t *name, expr_t *node);
expr_bindings_t *symtab_build_bindings(const symtab_t *t);
expr_bindings_t *symtab_build_bindings_for_expr(const symtab_t *t, const expr_t *expr);
expr_bindings_t *single_binding_from_node(expr_t *node);

typedef struct binding_exact_complex {
    number_t real;
    number_t imag;
} binding_exact_complex_t;

/* Binding-expression constructors. */
expr_binding_expr_t *expr_binding_expr_new_number_text(const char *text);
expr_binding_expr_t *expr_binding_expr_new_const(expr_binding_const_id_t const_id);
expr_binding_expr_t *expr_binding_expr_new_neg(expr_binding_expr_t *child);
expr_binding_expr_t *expr_binding_expr_new_add(expr_binding_expr_t *left, expr_binding_expr_t *right);
expr_binding_expr_t *expr_binding_expr_new_sub(expr_binding_expr_t *left, expr_binding_expr_t *right);
expr_binding_expr_t *expr_binding_expr_new_mul(expr_binding_expr_t *left, expr_binding_expr_t *right);
expr_binding_expr_t *expr_binding_expr_new_div(expr_binding_expr_t *left, expr_binding_expr_t *right);
expr_binding_expr_t *expr_binding_expr_new_powi(expr_binding_expr_t *base, long exponent);
expr_binding_expr_t *expr_binding_expr_new_unary_op(const expr_ops_t *ops, expr_binding_expr_t *child);
expr_binding_expr_t *expr_binding_expr_new_binary_op(const expr_ops_t *ops, expr_binding_expr_t *left,
                                                     expr_binding_expr_t *right);

/* Binding-expression lifecycle and evaluation. */
expr_binding_expr_t *expr_binding_expr_clone(const expr_binding_expr_t *expr);
void expr_binding_expr_free(expr_binding_expr_t *expr);
number_t expr_binding_expr_eval(const expr_binding_expr_t *expr);
bool expr_binding_expr_is_numeric_literal(const expr_binding_expr_t *expr);
bool expr_binding_expr_exact_complex(const expr_binding_expr_t *expr, binding_exact_complex_t *out);
void expr_binding_exact_complex_clear(binding_exact_complex_t *value);

/* Precision-sensitive binding evaluation. */
bool expr_binding_expr_eval_if_precision_increased(expr_binding_expr_t *expr, number_t *value_out);

/**
 * @brief Simplify a differentiable value node using algebraic identities.
 *
 * Returned node is owning (refcount = 1).
 * Input node is borrowed.
 */
expr_t *expr_simplify(const expr_t *dv);
expr_t *expr_expand_products_internal(const expr_t *expr);
expr_t *expr_canonicalize_known_radicals_internal(const expr_t *expr);

#endif /* EXPR_INTERNAL_H */
