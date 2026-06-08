#ifndef EXPR_INTERNAL_H
#define EXPR_INTERNAL_H

#include <stdint.h>
#include "qfloat.h"
#include "expression.h"
#include "internal/expr_internal.h"

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

/* ------------------------------------------------------------------------- */
/* Operator vtable                                                           */
/* ------------------------------------------------------------------------- */

/**
 * @brief Virtual function table for a differentiable value operator.
 *
 * Each node carries a pointer to one of these tables. The vtable defines:
 *   • how to evaluate the node (eval)
 *   • how to construct its derivative node (deriv)
 *
 * Both functions must return *new* nodes with refcount = 1.
 */
/**
 * @brief Arity classification for operator vtable entries.
 *
 * EXPR_OP_ATOM   — leaf node; no child pointers (constants, variables)
 * EXPR_OP_UNARY  — single child stored in expr_t::a
 * EXPR_OP_BINARY — two children stored in expr_t::a and expr_t::b
 */
typedef enum {
    EXPR_OP_ATOM,
    EXPR_OP_UNARY,
    EXPR_OP_BINARY
} expr_arity_t;

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
    EXPR_KIND_LAMBERT_W0,
    EXPR_KIND_LAMBERT_WM1,
    EXPR_KIND_NORMAL_PDF,
    EXPR_KIND_NORMAL_CDF,
    EXPR_KIND_NORMAL_LOGPDF,
    EXPR_KIND_EI,
    EXPR_KIND_E1,
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
    EXPR_KIND_COUNT
} expr_op_kind_t;

typedef enum expr_diff_kind {
    EXPR_DIFF_SMOOTH = 0,
    EXPR_DIFF_NONE
} expr_diff_kind_t;

typedef expr_t *(*expr_apply_unary_fn)(const expr_t *arg);
typedef expr_t *(*expr_apply_binary_fn)(const expr_t *left, const expr_t *right);
typedef expr_t *(*expr_simplify_fn)(const expr_t *tmpl, expr_t *a, expr_t *b);
typedef int (*expr_fold_const_unary_fn)(const number_t *in, number_t *out);
typedef expr_t *(*expr_inverse_unary_fn)(const expr_t *arg);
typedef expr_t *(*expr_integrate_fn)(const expr_t *expr, const expr_t *wrt);
typedef void (*expr_reverse_fn)(const expr_t *dv, const number_t *out_bar,
                                number_t *a_bar, number_t *b_bar);

typedef struct expr_ops {
    /** Compute the primal value of the node. Returns an owning `number_t` by value. */
    number_t  (*eval)(expr_t *dv);

    /** Build a new DAG node for the symbolic derivative. Returns owning (refcount=1). */
    expr_t *(*deriv)(expr_t *dv);

    /**
     * Reverse-mode local adjoint propagation hook.
     *
     * Given the adjoint of the operator output in @p out_bar, write the
     * contributions for child @p a to @p a_bar and child @p b to @p b_bar.
     * Unused child outputs must be written as zero.
     */
    expr_reverse_fn reverse;

    /** Stable operator kind tag for structural matching/introspection. */
    expr_op_kind_t kind;

    /** Arity of the operator; determines which child pointers are used. */
    expr_arity_t arity;

    /** Differentiability class used by UI and solver front-ends. */
    expr_diff_kind_t diff_kind;

    /** Human-readable operator name used in debug output and expr_to_text(). */
    const char  *name;

    /** TeX presentation name for renderers that emit native TeX. */
    const char  *tex_name;

    /**
     * Optional direct inverse for safe structural simplification:
     * op(op->direct_inverse(x)) -> x.
     */
    const struct expr_ops *direct_inverse;

    /**
     * Optional internal inverse constructor. This is metadata for symbolic
     * rewrites and solver machinery, not a public API promise. Branch-sensitive
     * pairs still need guards before simplifying inverse(op(x)) -> x.
     */
    expr_inverse_unary_fn inverse_unary;

    /**
     * Convenience constructor for unary ops: builds a new node wrapping @p arg.
     * NULL for non-unary operators. Returns owning (refcount=1).
     */
    expr_apply_unary_fn apply_unary;

    /**
     * Convenience constructor for binary ops: builds a new node from
     * @p left and @p right. NULL for non-binary operators. Returns owning
     * (refcount=1).
     */
    expr_apply_binary_fn apply_binary;

    /**
     * Optional local symbolic antiderivative hook.
     *
     * This is intended for operators whose primitives depend only on the node
     * and a directly matched affine child. Structural multi-node rules still
     * belong in the central integration dispatcher.
     */
    expr_integrate_fn integrate;

    /**
     * Simplification hook for this operator.
     *
     * @p tmpl is the original node being simplified. @p a and @p b are already
     * simplified owning children (or NULL if unused by the operator arity).
     * The hook must return a new owning result and take responsibility for
     * releasing any child handles it does not return.
     */
    expr_simplify_fn simplify;

    /**
     * Optional fast path for unary operators with notable constant inputs such
     * as sin(0), cos(0), exp(0), log(1), or sqrt(1). Returns non-zero when the
     * input was recognised and @p out was filled.
     */
    expr_fold_const_unary_fn fold_const_unary;
} expr_ops_t;

/* ------------------------------------------------------------------------- */
/* Internal representation of expr_t                                         */
/* ------------------------------------------------------------------------- */

/**
 * @brief Per-variable derivative cache entry.
 *
 * The derivative of a node can vary depending on which variable it is
 * differentiated with respect to. Each computed node holds a singly-linked
 * list of (wrt, dx) pairs so that partial derivatives w.r.t. different
 * variables can all be cached simultaneously.
 *
 * wrt == NULL is the sentinel for the single-variable / "differentiate
 * w.r.t. the unique variable" case used by expr_get_deriv() and
 * expr_create_deriv().
 */
typedef struct expr_deriv_cache {
    uint64_t              wrt_id; /* 0 = single-var sentinel; otherwise stable variable id */
    expr_t              *dx;  /* the derivative expression (owned) */
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

/**
 * @brief Full internal definition of a differentiable value node.
 *
 * Fields:
 *   ops      — operator vtable
 *   a, b     — child pointers (retained)
 *   c        — constant field (used by const and pow_d)
 *   x        — cached primal value
 *   x_valid  — whether x is valid
 *   simplified — whether this node has been certified by expr_simplify()
 *   simplify_epoch — maximum subtree epoch when simplified was certified
 *   dx_cache — singly-linked list of (wrt, dx) cache entries (owned)
 *   name     — optional symbolic name (owned)
 *   binding_expr — optional preserved binding RHS constant expression for display (owned)
 *   refcount — reference count for DAG lifetime management
 */
struct _expr_t {
    const expr_ops_t *ops;

    expr_t *a;
    expr_t *b;

    number_t  c;

    number_t  x;
    int       x_valid;

    /* epoch tracks the maximum variable generation seen at last evaluation.
     * For variable nodes, incremented by `expr_set_val()`. For computed nodes,
     * set to max(child epochs) after each recomputation. expr_eval() uses
     * this to detect stale caches automatically. */
    uint64_t  epoch;
    bool      simplified;
    uint64_t  simplify_epoch;

    expr_deriv_cache_t *dx_cache;

    char              *name;
    expr_binding_expr_t *binding_expr;

    int     refcount;
    uint64_t var_id;
};

/* ------------------------------------------------------------------------- */
/* Operator vtable instances                                                 */
/* ------------------------------------------------------------------------- */

/* Leaf nodes */
extern const expr_ops_t ops_const;
extern const expr_ops_t ops_var;

/* Arithmetic */
extern const expr_ops_t ops_add;
extern const expr_ops_t ops_sub;
extern const expr_ops_t ops_mul;
extern const expr_ops_t ops_div;
extern const expr_ops_t ops_neg;

/* Trigonometric */
extern const expr_ops_t ops_sin;
extern const expr_ops_t ops_cos;
extern const expr_ops_t ops_tan;
extern const expr_ops_t ops_sec;
extern const expr_ops_t ops_cosec;
extern const expr_ops_t ops_cot;

/* Hyperbolic */
extern const expr_ops_t ops_sinh;
extern const expr_ops_t ops_cosh;
extern const expr_ops_t ops_tanh;
extern const expr_ops_t ops_sech;
extern const expr_ops_t ops_cosech;
extern const expr_ops_t ops_coth;

/* Inverse trigonometric */
extern const expr_ops_t ops_asin;
extern const expr_ops_t ops_acos;
extern const expr_ops_t ops_atan;
extern const expr_ops_t ops_asec;
extern const expr_ops_t ops_acosec;
extern const expr_ops_t ops_acot;
extern const expr_ops_t ops_atan2;

/* Inverse hyperbolic */
extern const expr_ops_t ops_asinh;
extern const expr_ops_t ops_acosh;
extern const expr_ops_t ops_atanh;
extern const expr_ops_t ops_asech;
extern const expr_ops_t ops_acosech;
extern const expr_ops_t ops_acoth;

/* Exponential / logarithm / power */
extern const expr_ops_t ops_exp;
extern const expr_ops_t ops_log;
extern const expr_ops_t ops_log10;
extern const expr_ops_t ops_sqrt;
extern const expr_ops_t ops_floor;
extern const expr_ops_t ops_ceil;
extern const expr_ops_t ops_pow_d;  /* dv^(constant numeric exponent) */
extern const expr_ops_t ops_pow;    /* dv^dv */

/* Miscellaneous / special functions */
extern const expr_ops_t ops_abs;
extern const expr_ops_t ops_hypot;

/* Error functions */
extern const expr_ops_t ops_erf;
extern const expr_ops_t ops_erfc;
extern const expr_ops_t ops_erfinv;
extern const expr_ops_t ops_erfcinv;

/* Gamma family */
extern const expr_ops_t ops_gamma;
extern const expr_ops_t ops_lgamma;
extern const expr_ops_t ops_digamma;
extern const expr_ops_t ops_trigamma;
extern const expr_ops_t ops_polygamma;
extern const expr_ops_t ops_gammainv;

/* Lambert W (auto, principal and k=-1 branches) */
extern const expr_ops_t ops_lambert_w;
extern const expr_ops_t ops_lambert_w0;
extern const expr_ops_t ops_lambert_wm1;

/* Beta */
extern const expr_ops_t ops_beta;
extern const expr_ops_t ops_logbeta;
extern const expr_ops_t ops_gammainc_lower;
extern const expr_ops_t ops_gammainc_upper;
extern const expr_ops_t ops_gammainc_P;
extern const expr_ops_t ops_gammainc_Q;
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

/* Normal distribution */
extern const expr_ops_t ops_normal_pdf;
extern const expr_ops_t ops_normal_cdf;
extern const expr_ops_t ops_normal_logpdf;
extern const expr_ops_t ops_pdf;
extern const expr_ops_t ops_cdf;
extern const expr_ops_t ops_logpdf;

/* Exponential integrals */
extern const expr_ops_t ops_ei;
extern const expr_ops_t ops_e1;

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

expr_t *expr_alloc(const expr_ops_t *ops);
expr_t *expr_make_const_num(number_t x);
expr_t *expr_make_var_num(number_t x);
int expr_get_default_constant_num(const char *name, number_t *value_out);
int expr_get_default_constant_num_text(const string_t *name,
                                       number_t *value_out);
void expr_store_const_num(expr_t *dv, number_t value);
void expr_store_value_num(expr_t *dv, number_t value);
number_t expr_eval_num_internal(const expr_t *dv);
expr_t *expr_get_dx_internal(const expr_t *dv);
const expr_t *expr_current_wrt_internal(void);
expr_t *expr_new_unary_internal(const expr_ops_t *ops, const expr_t *a);
expr_t *expr_new_binary_internal(const expr_ops_t *ops, const expr_t *a, const expr_t *b);
expr_t *expr_new_pow_const_internal(const expr_t *a, number_t exponent);
expr_t *expr_polygamma_xp(const expr_t *order, const expr_t *arg);
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

static inline int expr_is_const(const expr_t *dv) { return expr_is_op(dv, &ops_const); }
static inline int expr_is_var(const expr_t *dv) { return expr_is_op(dv, &ops_var); }
static inline int expr_is_neg(const expr_t *dv) { return expr_is_op(dv, &ops_neg); }
static inline int expr_is_mul(const expr_t *dv) { return expr_is_op(dv, &ops_mul); }
static inline int expr_is_div(const expr_t *dv) { return expr_is_op(dv, &ops_div); }
static inline int expr_is_addsub(const expr_t *dv)
{
    return expr_is_op(dv, &ops_add) || expr_is_op(dv, &ops_sub);
}
static inline int expr_is_exp_expr(const expr_t *dv) { return expr_is_op(dv, &ops_exp); }
static inline int expr_is_sqrt_expr(const expr_t *dv) { return expr_is_op(dv, &ops_sqrt); }
static inline int expr_is_pow_d_expr(const expr_t *dv) { return expr_is_op(dv, &ops_pow_d); }
static inline int expr_is_unnamed_const(const expr_t *dv)
{
    return expr_is_const(dv) && (!dv->name || !*dv->name);
}

expr_t *expr_simplify_passthrough(const expr_t *dv, expr_t *a, expr_t *b);
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
expr_t *expr_simplify_direct_inverse_pair_from_raw(const expr_t *outer,
                                                 const expr_t *raw_inner,
                                                 expr_t *simplified_inner);
bool expr_ops_is_lambert(const expr_ops_t *ops);
bool expr_ops_is_floor_or_ceil(const expr_ops_t *ops);
bool expr_ops_are_direct_inverse_pair(const expr_ops_t *outer,
                                      const expr_ops_t *inner);
const expr_ops_t *expr_ops_reciprocal_unary(const expr_ops_t *ops);

expr_t *expr_integrate_dispatch_primitive(const expr_t *expr, const expr_t *wrt);
bool expr_ops_has_inverse_unary_simplify_rule(const expr_ops_t *ops);
bool expr_inverse_unary_candidate_value_ok(const expr_ops_t *ops,
                                         number_t value);
expr_t *expr_simplify_try_vtable_inverse_argument(const expr_t *outer,
                                                const expr_t *arg);
expr_t *expr_simplify_try_basic_sum(expr_t *a, expr_t *b);
expr_t *expr_simplify_try_basic_product(expr_t *a, expr_t *b);
expr_t *expr_simplify_try_trig_product(expr_t *a, expr_t *b);
expr_t *expr_simplify_try_lambert_product(expr_t *a, expr_t *b);
expr_t *expr_simplify_try_i_unit_product(expr_t *a, expr_t *b);
expr_t *expr_simplify_try_imag_trig_bridge(const expr_t *op, expr_t *arg);
expr_t *expr_try_trig_pythagorean_identity(const addend_t *terms, size_t n,
                                         number_t c_const, number_t common_coeff);

int expr_struct_eq(const expr_t *u, const expr_t *v);
expr_t *expr_make_scaled(number_t coeff, expr_t *base);
expr_t *expr_make_pow_like(expr_t *base, number_t exponent);
void expr_collect_addends(expr_t *dv, number_t scale, number_t *c_const,
                        addend_t **terms, size_t *n, size_t *cap);
void expr_combine_common_denominator_addends(addend_t *terms, size_t n);
void expr_sort_addends(addend_t *terms, size_t n);
int expr_extract_common_addend_coeff(const addend_t *terms, size_t n,
                                   number_t c_const, number_t *common_out);
void expr_free_node_array(expr_t **nodes, size_t count);
void expr_append_node(expr_t ***nodes, size_t *count, size_t *cap, expr_t *node);
void expr_split_division_terms(number_t *c_acc, int *is_zero,
                             expr_t **terms, size_t nterms,
                             expr_t ***den_terms, size_t *nden_terms,
                             size_t *den_cap);
void expr_combine_like_powers(expr_t **terms, size_t nterms);
void expr_cancel_common_powers(expr_t **terms, size_t nterms,
                             expr_t **den_terms, size_t nden_terms);
void expr_combine_exp_terms(expr_t **terms, size_t nterms);
void expr_merge_sqrt_terms(expr_t **terms, size_t nterms);
void expr_merge_sqrt_quotient_terms(expr_t **terms, size_t nterms,
                                  expr_t **den_terms, size_t nden_terms);
expr_t *expr_try_expand_shallow_product(number_t c_acc,
                                      expr_t **terms, size_t nterms,
                                      expr_t **den_terms, size_t nden_terms);
expr_t *expr_rebuild_product_chain(number_t c_acc, expr_t **terms, size_t nterms);
expr_t *expr_rebuild_division_chain(expr_t **den_terms, size_t nden_terms);

int expr_fold_zero_to_zero(const number_t *in, number_t *out);
int expr_fold_cos_const(const number_t *in, number_t *out);
int expr_fold_exp_const(const number_t *in, number_t *out);
int expr_fold_log_const(const number_t *in, number_t *out);
int expr_fold_sqrt_const(const number_t *in, number_t *out);
int expr_fold_floor_const(const number_t *in, number_t *out);
int expr_fold_erf_const(const number_t *in, number_t *out);
int expr_fold_erfc_const(const number_t *in, number_t *out);

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
void expr_reverse_gammainv(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
void expr_reverse_lambert_w(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar);
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

typedef struct binding_exact_complex {
    number_t real;
    number_t imag;
} binding_exact_complex_t;

expr_binding_expr_t *expr_binding_expr_new_number_text(const char *text);
expr_binding_expr_t *expr_binding_expr_new_const(expr_binding_const_id_t const_id);
expr_binding_expr_t *expr_binding_expr_new_neg(expr_binding_expr_t *child);
expr_binding_expr_t *expr_binding_expr_new_add(expr_binding_expr_t *left, expr_binding_expr_t *right);
expr_binding_expr_t *expr_binding_expr_new_sub(expr_binding_expr_t *left, expr_binding_expr_t *right);
expr_binding_expr_t *expr_binding_expr_new_mul(expr_binding_expr_t *left, expr_binding_expr_t *right);
expr_binding_expr_t *expr_binding_expr_new_div(expr_binding_expr_t *left, expr_binding_expr_t *right);
expr_binding_expr_t *expr_binding_expr_new_powi(expr_binding_expr_t *base, long exponent);
expr_binding_expr_t *expr_binding_expr_new_unary_op(const expr_ops_t *ops, expr_binding_expr_t *child);
expr_binding_expr_t *expr_binding_expr_new_binary_op(const expr_ops_t *ops, expr_binding_expr_t *left, expr_binding_expr_t *right);
expr_binding_expr_t *expr_binding_expr_clone(const expr_binding_expr_t *expr);
void expr_binding_expr_free(expr_binding_expr_t *expr);
number_t expr_binding_expr_eval(const expr_binding_expr_t *expr);
bool expr_binding_expr_is_numeric_literal(const expr_binding_expr_t *expr);
bool expr_binding_expr_exact_complex(const expr_binding_expr_t *expr,
                                   binding_exact_complex_t *out);
void expr_binding_exact_complex_clear(binding_exact_complex_t *value);
bool expr_binding_expr_eval_if_precision_increased(expr_binding_expr_t *expr,
                                                 number_t *value_out);

/**
 * @brief Simplify a differentiable value node using algebraic identities.
 *
 * Returned node is owning (refcount = 1).
 * Input node is borrowed.
 */
expr_t *expr_simplify(const expr_t *dv);

#endif /* EXPR_INTERNAL_H */
