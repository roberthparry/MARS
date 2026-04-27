#ifndef DVAL_INTERNAL_H
#define DVAL_INTERNAL_H

#include <stdint.h>
#include "qfloat.h"
#include "dval.h"

/**
 * @file dval_internal.h
 * @brief Internal structures and operator vtables for the differentiable
 *        value DAG.
 *
 * This header defines the full internal representation of ::dval_t and the
 * operator vtables used by the lazy evaluation and automatic differentiation
 * engine. It is not intended for public use; external code should include
 * only dval.h.
 *
 * Ownership model (internal summary):
 *   • Every dval_t is a reference-counted node.
 *   • Arithmetic builders retain their children; they never steal ownership.
 *   • dv_get_deriv() returns a borrowed pointer to f->dx.
 *   • dv_create_* functions produce owning handles.
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
 * DV_OP_ATOM   — leaf node; no child pointers (constants, variables)
 * DV_OP_UNARY  — single child stored in dval_t::a
 * DV_OP_BINARY — two children stored in dval_t::a and dval_t::b
 */
typedef enum {
    DV_OP_ATOM,
    DV_OP_UNARY,
    DV_OP_BINARY
} dval_arity_t;

typedef dval_t *(*dval_apply_unary_fn)(dval_t *arg);
typedef dval_t *(*dval_apply_binary_fn)(dval_t *left, dval_t *right);
typedef dval_t *(*dval_simplify_fn)(const dval_t *tmpl, dval_t *a, dval_t *b);
typedef int (*dval_fold_const_unary_fn)(qfloat_t in, qfloat_t *out);
typedef void (*dval_reverse_fn)(const dval_t *dv, qfloat_t out_bar,
                                qfloat_t *a_bar, qfloat_t *b_bar);

typedef struct dval_ops {
    /** Compute the primal value of the node. Returns a qfloat_t by value. */
    qfloat_t  (*eval)(dval_t *dv);

    /** Build a new DAG node for the symbolic derivative. Returns owning (refcount=1). */
    dval_t *(*deriv)(dval_t *dv);

    /**
     * Reverse-mode local adjoint propagation hook.
     *
     * Given the adjoint of the operator output in @p out_bar, write the
     * contributions for child @p a to @p a_bar and child @p b to @p b_bar.
     * Unused child outputs must be written as zero.
     */
    dval_reverse_fn reverse;

    /** Arity of the operator; determines which child pointers are used. */
    dval_arity_t arity;

    /** Human-readable operator name used in debug output and dv_to_string(). */
    const char  *name;

    /**
     * Convenience constructor for unary ops: builds a new node wrapping @p arg.
     * NULL for non-unary operators. Returns owning (refcount=1).
     */
    dval_apply_unary_fn apply_unary;

    /**
     * Convenience constructor for binary ops: builds a new node from
     * @p left and @p right. NULL for non-binary operators. Returns owning
     * (refcount=1).
     */
    dval_apply_binary_fn apply_binary;

    /**
     * Simplification hook for this operator.
     *
     * @p tmpl is the original node being simplified. @p a and @p b are already
     * simplified owning children (or NULL if unused by the operator arity).
     * The hook must return a new owning result and take responsibility for
     * releasing any child handles it does not return.
     */
    dval_simplify_fn simplify;

    /**
     * Optional fast path for unary operators with notable constant inputs such
     * as sin(0), cos(0), exp(0), log(1), or sqrt(1). Returns non-zero when the
     * input was recognised and @p out was filled.
     */
    dval_fold_const_unary_fn fold_const_unary;
} dval_ops_t;

/* ------------------------------------------------------------------------- */
/* Internal representation of dval_t                                         */
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
 * w.r.t. the unique variable" case used by dv_get_deriv() and
 * dv_create_deriv().
 */
typedef struct dv_deriv_cache {
    uint64_t              wrt_id; /* 0 = single-var sentinel; otherwise stable variable id */
    dval_t              *dx;  /* the derivative expression (owned) */
    struct dv_deriv_cache *next;
} dv_deriv_cache_t;

/**
 * @brief Full internal definition of a differentiable value node.
 *
 * Fields:
 *   ops      — operator vtable
 *   a, b     — child pointers (retained)
 *   c        — constant field (used by const and pow_d)
 *   x        — cached primal value
 *   x_valid  — whether x is valid
 *   dx_cache — singly-linked list of (wrt, dx) cache entries (owned)
 *   name     — optional symbolic name (owned)
 *   refcount — reference count for DAG lifetime management
 */
struct _dval_t {
    const dval_ops_t *ops;

    dval_t *a;
    dval_t *b;

    qfloat_t  c;

    qfloat_t  x;
    int       x_valid;

    /* epoch tracks the maximum variable generation seen at last evaluation.
     * For variable nodes, incremented by dv_set_val(). For computed nodes,
     * set to max(child epochs) after each recomputation. dv_eval() uses
     * this to detect stale caches automatically. */
    uint64_t  epoch;

    dv_deriv_cache_t *dx_cache;

    char   *name;

    int     refcount;
    uint64_t var_id;
};

/* ------------------------------------------------------------------------- */
/* Operator vtable instances                                                 */
/* ------------------------------------------------------------------------- */

/* Leaf nodes */
extern const dval_ops_t ops_const;
extern const dval_ops_t ops_var;

/* Arithmetic */
extern const dval_ops_t ops_add;
extern const dval_ops_t ops_sub;
extern const dval_ops_t ops_mul;
extern const dval_ops_t ops_div;
extern const dval_ops_t ops_neg;

/* Trigonometric */
extern const dval_ops_t ops_sin;
extern const dval_ops_t ops_cos;
extern const dval_ops_t ops_tan;

/* Hyperbolic */
extern const dval_ops_t ops_sinh;
extern const dval_ops_t ops_cosh;
extern const dval_ops_t ops_tanh;

/* Inverse trigonometric */
extern const dval_ops_t ops_asin;
extern const dval_ops_t ops_acos;
extern const dval_ops_t ops_atan;
extern const dval_ops_t ops_atan2;

/* Inverse hyperbolic */
extern const dval_ops_t ops_asinh;
extern const dval_ops_t ops_acosh;
extern const dval_ops_t ops_atanh;

/* Exponential / logarithm / power */
extern const dval_ops_t ops_exp;
extern const dval_ops_t ops_log;
extern const dval_ops_t ops_sqrt;
extern const dval_ops_t ops_pow_d;  /* dv^(constant double) */
extern const dval_ops_t ops_pow;    /* dv^dv */

/* Miscellaneous / special functions */
extern const dval_ops_t ops_abs;
extern const dval_ops_t ops_hypot;

/* Error functions */
extern const dval_ops_t ops_erf;
extern const dval_ops_t ops_erfc;
extern const dval_ops_t ops_erfinv;
extern const dval_ops_t ops_erfcinv;

/* Gamma family */
extern const dval_ops_t ops_gamma;
extern const dval_ops_t ops_lgamma;
extern const dval_ops_t ops_digamma;
extern const dval_ops_t ops_trigamma;

/* Lambert W (principal and k=-1 branches) */
extern const dval_ops_t ops_lambert_w0;
extern const dval_ops_t ops_lambert_wm1;

/* Beta */
extern const dval_ops_t ops_beta;
extern const dval_ops_t ops_logbeta;

/* Normal distribution */
extern const dval_ops_t ops_normal_pdf;
extern const dval_ops_t ops_normal_cdf;
extern const dval_ops_t ops_normal_logpdf;

/* Exponential integrals */
extern const dval_ops_t ops_ei;
extern const dval_ops_t ops_e1;

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

/**
 * @brief Increment the reference count of a node.
 *
 * Safe to call with NULL.
 */
void dv_retain(dval_t *dv);
dval_t *dv_pow_qf(dval_t *a, qfloat_t exponent);

dval_t *dv_simplify_passthrough(const dval_t *dv, dval_t *a, dval_t *b);
dval_t *dv_simplify_unary_operator(const dval_t *dv, dval_t *a, dval_t *b);
dval_t *dv_simplify_binary_operator(const dval_t *dv, dval_t *a, dval_t *b);
dval_t *dv_simplify_neg_operator(const dval_t *dv, dval_t *a, dval_t *b);
dval_t *dv_simplify_add_sub_operator(const dval_t *dv, dval_t *a, dval_t *b);
dval_t *dv_simplify_mul_operator(const dval_t *dv, dval_t *a, dval_t *b);
dval_t *dv_simplify_div_operator(const dval_t *dv, dval_t *a, dval_t *b);
dval_t *dv_simplify_pow_d_operator(const dval_t *dv, dval_t *a, dval_t *b);
dval_t *dv_simplify_pow_operator(const dval_t *dv, dval_t *a, dval_t *b);
dval_t *dv_simplify_hypot_operator(const dval_t *dv, dval_t *a, dval_t *b);

int dv_fold_zero_to_zero(qfloat_t in, qfloat_t *out);
int dv_fold_cos_const(qfloat_t in, qfloat_t *out);
int dv_fold_exp_const(qfloat_t in, qfloat_t *out);
int dv_fold_log_const(qfloat_t in, qfloat_t *out);
int dv_fold_sqrt_const(qfloat_t in, qfloat_t *out);

void dv_reverse_atom(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_add(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_sub(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_mul(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_div(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_pow(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_pow_d(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_atan2(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_neg(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_sin(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_cos(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_tan(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_sinh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_cosh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_tanh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_asin(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_acos(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_atan(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_asinh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_acosh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_atanh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_exp(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_log(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_sqrt(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_abs(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_hypot(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_erf(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_erfc(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_erfinv(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_erfcinv(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_gamma(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_lgamma(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_digamma(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_trigamma(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_lambert_w0(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_lambert_wm1(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_normal_pdf(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_normal_cdf(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_normal_logpdf(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_ei(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_e1(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_beta(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);
void dv_reverse_logbeta(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar);

/**
 * @brief Simplify a differentiable value node using algebraic identities.
 *
 * Returned node is owning (refcount = 1).
 * Input node is borrowed.
 */
dval_t *dv_simplify(dval_t *dv);

#endif /* DVAL_INTERNAL_H */
