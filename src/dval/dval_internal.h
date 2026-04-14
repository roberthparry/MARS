#ifndef DVAL_INTERNAL_H
#define DVAL_INTERNAL_H

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
typedef enum {
    DV_OP_ATOM,
    DV_OP_UNARY,
    DV_OP_BINARY
} dval_arity_t;

typedef struct dval_ops {
    qfloat  (*eval)(dval_t *dv);
    dval_t *(*deriv)(dval_t *dv);

    dval_arity_t arity;
    const char  *name;

    /* constructor for unary ops (NULL for non-unary) */
    dval_t *(*apply_unary)(dval_t *arg);
} dval_ops_t;

/* ------------------------------------------------------------------------- */
/* Internal representation of dval_t                                         */
/* ------------------------------------------------------------------------- */

/**
 * @brief Full internal definition of a differentiable value node.
 *
 * Fields:
 *   ops      — operator vtable
 *   a, b     — child pointers (retained)
 *   c        — constant field (used by const and pow_d)
 *   x        — cached primal value
 *   x_valid  — whether x is valid
 *   dx       — cached derivative node (owned)
 *   dx_valid — whether dx is valid
 *   name     — optional symbolic name (owned)
 *   refcount — reference count for DAG lifetime management
 */
struct _dval_t {
    const dval_ops_t *ops;

    dval_t *a;
    dval_t *b;

    qfloat  c;

    qfloat  x;
    int     x_valid;

    dval_t *dx;
    int     dx_valid;

    char   *name;

    int     refcount;
};

/* ------------------------------------------------------------------------- */
/* Operator vtable instances                                                 */
/* ------------------------------------------------------------------------- */

extern const dval_ops_t ops_const;
extern const dval_ops_t ops_var;

extern const dval_ops_t ops_add;
extern const dval_ops_t ops_sub;
extern const dval_ops_t ops_mul;
extern const dval_ops_t ops_div;
extern const dval_ops_t ops_neg;

extern const dval_ops_t ops_sin;
extern const dval_ops_t ops_cos;
extern const dval_ops_t ops_tan;

extern const dval_ops_t ops_sinh;
extern const dval_ops_t ops_cosh;
extern const dval_ops_t ops_tanh;

extern const dval_ops_t ops_asin;
extern const dval_ops_t ops_acos;
extern const dval_ops_t ops_atan;
extern const dval_ops_t ops_atan2;

extern const dval_ops_t ops_asinh;
extern const dval_ops_t ops_acosh;
extern const dval_ops_t ops_atanh;

extern const dval_ops_t ops_exp;
extern const dval_ops_t ops_log;
extern const dval_ops_t ops_sqrt;

extern const dval_ops_t ops_pow_d;
extern const dval_ops_t ops_pow;

extern const dval_ops_t ops_abs;
extern const dval_ops_t ops_erf;
extern const dval_ops_t ops_erfc;
extern const dval_ops_t ops_erfinv;
extern const dval_ops_t ops_erfcinv;
extern const dval_ops_t ops_gamma;
extern const dval_ops_t ops_lgamma;
extern const dval_ops_t ops_digamma;
extern const dval_ops_t ops_trigamma;
extern const dval_ops_t ops_lambert_w0;
extern const dval_ops_t ops_lambert_wm1;
extern const dval_ops_t ops_beta;
extern const dval_ops_t ops_logbeta;
extern const dval_ops_t ops_normal_pdf;
extern const dval_ops_t ops_normal_cdf;
extern const dval_ops_t ops_normal_logpdf;
extern const dval_ops_t ops_ei;
extern const dval_ops_t ops_e1;
extern const dval_ops_t ops_hypot;

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

/**
 * @brief Increment the reference count of a node.
 *
 * Safe to call with NULL.
 */
void dv_retain(dval_t *dv);

/**
 * @brief Simplify a differentiable value node using algebraic identities.
 *
 * Returned node is owning (refcount = 1).
 * Input node is borrowed.
 */
dval_t *dv_simplify(dval_t *dv);

#endif /* DVAL_INTERNAL_H */
