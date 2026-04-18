#ifndef DVAL_H
#define DVAL_H

#include "qfloat.h"

/**
 * @file dval.h
 * @brief Lazy, vtable-driven, reference-counted differentiable value type.
 *
 * Ownership rules:
 *   • Any function whose name begins with dv_new_* or dv_create_* returns
 *     an owning handle. The caller must call dv_free() exactly once.
 *
 *   • All arithmetic and math functions (dv_add, dv_mul, dv_sin, …) also
 *     return owning handles. The caller must dv_free() them.
 *
 *   • dv_get_deriv() returns a *borrowed* view. The caller must NOT free it.
 *
 * Internally, each dval_t is a node in a reference-counted DAG.
 */

typedef struct _dval_t dval_t;

/* ------------------------------------------------------------------------- */
/* Constructors — constants                                                  */
/* ------------------------------------------------------------------------- */

/**
 * @brief Create a constant node from a double or qfloat_t value.
 *
 * Constants have no variable binding; their derivative is always zero.
 * Returns an owning handle; caller must call dv_free() exactly once.
 */
dval_t *dv_new_const_d(double x);
dval_t *dv_new_const(qfloat_t x);

/**
 * @brief Create a named constant node.
 *
 * Behaves like dv_new_const() but attaches a symbolic name used in
 * dv_to_string() output and debug printing. @p name is copied.
 * Returns an owning handle; caller must call dv_free() exactly once.
 */
dval_t *dv_new_named_const(qfloat_t x, const char *name);
dval_t *dv_new_named_const_d(double x, const char *name);

/* ------------------------------------------------------------------------- */
/* Constructors — variables                                                  */
/* ------------------------------------------------------------------------- */

/**
 * @brief Create a variable node from a double or qfloat_t value.
 *
 * Variables are leaf nodes whose value can be updated via dv_set_val().
 * Derivative of a variable with respect to itself is 1.
 * Returns an owning handle; caller must call dv_free() exactly once.
 */
dval_t *dv_new_var_d(double x);
dval_t *dv_new_var(qfloat_t x);

/**
 * @brief Create a named variable node.
 *
 * Behaves like dv_new_var() but attaches a symbolic name used in
 * dv_to_string() output and debug printing. @p name is copied.
 * Returns an owning handle; caller must call dv_free() exactly once.
 */
dval_t *dv_new_named_var(qfloat_t x, const char *name);
dval_t *dv_new_named_var_d(double x, const char *name);

/* ------------------------------------------------------------------------- */
/* Mutators                                                                  */
/* ------------------------------------------------------------------------- */

/**
 * @brief Update the value of a variable node.
 *
 * Invalidates the cached primal value and cached derivative for @p dv
 * and all ancestor nodes that depend on it.
 * @p dv must be a variable node (created with dv_new_var or dv_new_named_var).
 */
void dv_set_val(dval_t *dv, qfloat_t value);
void dv_set_val_d(dval_t *dv, double value);

/**
 * @brief Attach or replace the symbolic name of a node.
 *
 * @p name is copied. Passing NULL removes any existing name.
 */
void dv_set_name(dval_t *dv, const char *name);

/* ------------------------------------------------------------------------- */
/* Accessors                                                                 */
/* ------------------------------------------------------------------------- */

/**
 * @brief Return the cached primal value without forcing evaluation.
 *
 * Returns the last computed value. If the node has never been evaluated
 * the result is undefined. Prefer dv_eval() unless you are certain the
 * cache is valid.
 */
double dv_get_val_d(const dval_t *dv);
qfloat_t dv_get_val(const dval_t *dv);

/**
 * @brief Get the cached first derivative node (borrowed).
 *
 * The returned pointer is owned by @p dv and must NOT be freed by the caller.
 * If the derivative has not yet been built, it is created lazily.
 */
const dval_t *dv_get_deriv(const dval_t *dv);

/* ------------------------------------------------------------------------- */
/* Evaluation                                                                */
/* ------------------------------------------------------------------------- */

/**
 * @brief Evaluate the node, updating the cached primal value.
 *
 * Traverses the DAG recursively, recomputing any nodes whose cache is
 * stale. The result is stored in the node's cache and returned.
 */
qfloat_t dv_eval(const dval_t *dv);
double dv_eval_d(const dval_t *dv);

/* ------------------------------------------------------------------------- */
/* Derivative creation (owning)                                              */
/* ------------------------------------------------------------------------- */

/**
 * @brief Build a new DAG node representing the n-th derivative of @p dv.
 *
 * Returns an owning handle; caller must call dv_free() exactly once.
 * Passing n = 0 returns a new reference to @p dv itself.
 * dv_create_deriv() is equivalent to dv_create_nth_deriv(1, dv).
 */
dval_t *dv_create_deriv(dval_t *dv);
dval_t *dv_create_2nd_deriv(dval_t *dv);
dval_t *dv_create_3rd_deriv(dval_t *dv);
dval_t *dv_create_nth_deriv(unsigned int n, dval_t *dv);

/* ------------------------------------------------------------------------- */
/* Arithmetic (graph-building, owning)                                       */
/* ------------------------------------------------------------------------- */

/**
 * All arithmetic functions build a new DAG node representing the operation
 * and return an owning handle. Arguments are retained (not consumed); the
 * caller remains responsible for freeing its own handles.
 *
 * _d variants accept a plain double for the scalar operand; d_sub and d_div
 * treat the double as the left-hand operand (d - dv and d / dv).
 */
dval_t *dv_neg(dval_t *dv);
dval_t *dv_add(dval_t *dv1, dval_t *dv2);
dval_t *dv_sub(dval_t *dv1, dval_t *dv2);
dval_t *dv_mul(dval_t *dv1, dval_t *dv2);
dval_t *dv_div(dval_t *dv1, dval_t *dv2);

dval_t *dv_add_d(dval_t *dv, double d);
dval_t *dv_sub_d(dval_t *dv, double d);
dval_t *dv_d_sub(double d, dval_t *dv);
dval_t *dv_mul_d(dval_t *dv, double d);
dval_t *dv_div_d(dval_t *dv, double d);
dval_t *dv_d_div(double d, dval_t *dv);

/* ------------------------------------------------------------------------- */
/* Comparison                                                                */
/* ------------------------------------------------------------------------- */

/**
 * @brief Compare the primal values of two nodes.
 *
 * Forces evaluation of both nodes, then compares their values.
 * Returns -1 if dv1 < dv2, 0 if equal, +1 if dv1 > dv2.
 */
int dv_cmp(const dval_t *dv1, const dval_t *dv2);

/* ------------------------------------------------------------------------- */
/* Elementary functions (owning)                                             */
/* ------------------------------------------------------------------------- */

/**
 * All elementary functions build a new DAG node and return an owning handle.
 * Arguments are retained (not consumed).
 *
 * dv_pow_d(dv, d) computes dv^d for a constant double exponent.
 * dv_pow(dv1, dv2) computes dv1^dv2 where both operands are differentiable.
 */
dval_t *dv_sin(dval_t *dv);
dval_t *dv_cos(dval_t *dv);
dval_t *dv_tan(dval_t *dv);
dval_t *dv_sinh(dval_t *dv);
dval_t *dv_cosh(dval_t *dv);
dval_t *dv_tanh(dval_t *dv);
dval_t *dv_asin(dval_t *dv);
dval_t *dv_acos(dval_t *dv);
dval_t *dv_atan(dval_t *dv);
dval_t *dv_atan2(dval_t *dv1, dval_t *dv2);
dval_t *dv_asinh(dval_t *dv);
dval_t *dv_acosh(dval_t *dv);
dval_t *dv_atanh(dval_t *dv);
dval_t *dv_exp(dval_t *dv);
dval_t *dv_log(dval_t *dv);
dval_t *dv_sqrt(dval_t *dv);
dval_t *dv_pow_d(dval_t *dv, double d);
dval_t *dv_pow(dval_t *dv1, dval_t *dv2);

/* ------------------------------------------------------------------------- */
/* Special functions (owning)                                                */
/* ------------------------------------------------------------------------- */

/**
 * All special functions build a new DAG node and return an owning handle.
 * Arguments are retained (not consumed).
 *
 * Error functions:   dv_erf, dv_erfc, dv_erfinv, dv_erfcinv
 * Gamma family:      dv_gamma (Γ), dv_lgamma (log Γ), dv_digamma (ψ),
 *                    dv_trigamma (ψ₁)
 * Lambert W:         dv_lambert_w0 (principal branch), dv_lambert_wm1 (k=-1)
 * Beta:              dv_beta (B), dv_logbeta (log B)
 * Normal dist.:      dv_normal_pdf, dv_normal_cdf, dv_normal_logpdf
 * Exponential int.:  dv_ei (Ei), dv_e1 (E₁)
 */
dval_t *dv_abs(dval_t *dv);
dval_t *dv_hypot(dval_t *dv1, dval_t *dv2);
dval_t *dv_erf(dval_t *dv);
dval_t *dv_erfc(dval_t *dv);
dval_t *dv_erfinv(dval_t *dv);
dval_t *dv_erfcinv(dval_t *dv);
dval_t *dv_gamma(dval_t *dv);
dval_t *dv_lgamma(dval_t *dv);
dval_t *dv_digamma(dval_t *dv);
dval_t *dv_trigamma(dval_t *dv);
dval_t *dv_lambert_w0(dval_t *dv);
dval_t *dv_lambert_wm1(dval_t *dv);
dval_t *dv_beta(dval_t *dv1, dval_t *dv2);
dval_t *dv_logbeta(dval_t *dv1, dval_t *dv2);
dval_t *dv_normal_pdf(dval_t *dv);
dval_t *dv_normal_cdf(dval_t *dv);
dval_t *dv_normal_logpdf(dval_t *dv);
dval_t *dv_ei(dval_t *dv);
dval_t *dv_e1(dval_t *dv);

/* ------------------------------------------------------------------------- */
/* Debug / lifetime                                                          */
/* ------------------------------------------------------------------------- */

/**
 * @brief Decrement the reference count and free if it reaches zero.
 *
 * Must be called exactly once for every owning handle returned by:
 *   - dv_new_*
 *   - dv_create_*
 *   - dv_add, dv_mul, dv_sin, etc.
 */
void dv_free(dval_t *dv);

/* ------------------------------------------------------------------------- */
/* String conversion                                                         */
/* ------------------------------------------------------------------------- */

/**
 * @brief Output style for dv_to_string().
 *
 * style_EXPRESSION  — infix notation, e.g. "{ sin(x₀) | x₀ = 1.0 }"
 * style_FUNCTION    — prefix/function notation, e.g. "sin(var(x₀=1.0))"
 */
typedef enum {
    style_FUNCTION,
    style_EXPRESSION
} style_t;

/**
 * @brief Serialize @p dv to a newly allocated string.
 *
 * The format is controlled by @p style (see style_t).
 * The expression style produces output that can be round-tripped through
 * dval_from_string(). The returned string is heap-allocated; the caller
 * must free() it.
 */
char *dv_to_string(const dval_t *dv, style_t style);

/**
 * @brief Print the expression-style string representation of @p dv to stdout.
 *
 * Equivalent to calling dv_to_string(dv, style_EXPRESSION) and printing
 * the result, followed by a newline.
 */
void dv_print(const dval_t *dv);

/* ------------------------------------------------------------------------- */
/* Parsing                                                                   */
/* ------------------------------------------------------------------------- */

/**
 * @brief Construct a dval_t from an expression-style string.
 *
 * Accepts strings in the format produced by dv_to_string(f, style_EXPRESSION):
 *
 *   { expr | x₀ = val, ...; [name] = val, ... }
 *
 * or for a pure named constant:
 *
 *   { name = val }
 *
 * Variables appear before the ';' in the binding section; named constants
 * appear after it.
 *
 * The following ASCII alternatives are accepted in addition to the canonical
 * Unicode forms:
 *
 *   @p _N       subscript digit (x_0 ≡ x₀); interchangeable with U+2080–U+2089
 *               within the same string
 *   @p *        explicit multiplication (c * sin(x) ≡ c·sin(x)); spaces around
 *               @p * are permitted
 *   @p ^N       integer exponent on a function name (sin^2 ≡ sin²) or on a
 *               sub-expression ((x+1)^2 ≡ (x+1)²)
 *
 * Bracketed names (@p [my var], @p [2pi], …) are supported for identifiers
 * that do not fit the single-letter-plus-subscript rule.
 *
 * Returns an owning handle on success, or NULL on error (details written to
 * stderr).  The caller must call dv_free() on the returned pointer exactly
 * once.
 */
dval_t *dval_from_string(const char *s);

#endif /* DVAL_H */
