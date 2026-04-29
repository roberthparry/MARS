#ifndef DVAL_H
#define DVAL_H

#include <stdbool.h>
#include <stddef.h>
#include "qfloat.h"
#include "qcomplex.h"

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
 *
 * Threading:
 *   • dval_t is currently a single-threaded type.
 *   • Do not read, evaluate, differentiate, or mutate the same DAG from
 *     multiple threads concurrently without external synchronisation.
 */

typedef struct _dval_t dval_t;

/**
 * @brief Borrowed symbolic binding returned by dval_from_string_with_bindings().
 *
 * The @p name pointer and @p symbol handle remain valid for as long as the
 * dval returned by dval_from_string_with_bindings() remains alive. Releasing
 * the bindings array itself only requires a plain free(bindings).
 */
typedef struct {
    const char *name;
    dval_t *symbol;
    bool is_constant;
} dval_binding_t;

/**
 * @brief Canonical differentiable constants for zero and one.
 *
 * These are process-lifetime singleton constant nodes.
 */
extern dval_t *DV_ZERO;
extern dval_t *DV_ONE;

/* ------------------------------------------------------------------------- */
/* Constructors — constants                                                  */
/* ------------------------------------------------------------------------- */

/**
 * @brief Create a constant node from a double, qfloat_t, or qcomplex_t value.
 *
 * Constants have no variable binding; their derivative is always zero.
 * Returns an owning handle; caller must call dv_free() exactly once.
 */
dval_t *dv_new_const_d(double x);
dval_t *dv_new_const(qfloat_t x);
dval_t *dv_new_const_qc(qcomplex_t x);

/**
 * @brief Create a named constant node.
 *
 * Behaves like dv_new_const() but attaches a symbolic name used in
 * dv_to_string() output and debug printing. @p name is copied.
 * Returns an owning handle; caller must call dv_free() exactly once.
 */
dval_t *dv_new_named_const(qfloat_t x, const char *name);
dval_t *dv_new_named_const_qc(qcomplex_t x, const char *name);
dval_t *dv_new_named_const_d(double x, const char *name);

/* ------------------------------------------------------------------------- */
/* Constructors — variables                                                  */
/* ------------------------------------------------------------------------- */

/**
 * @brief Create a variable node from a double, qfloat_t, or qcomplex_t value.
 *
 * Variables are leaf nodes whose value can be updated via dv_set_val().
 * Derivative of a variable with respect to itself is 1.
 * Returns an owning handle; caller must call dv_free() exactly once.
 */
dval_t *dv_new_var_d(double x);
dval_t *dv_new_var(qfloat_t x);
dval_t *dv_new_var_qc(qcomplex_t x);

/**
 * @brief Create a named variable node.
 *
 * Behaves like dv_new_var() but attaches a symbolic name used in
 * dv_to_string() output and debug printing. @p name is copied.
 * Returns an owning handle; caller must call dv_free() exactly once.
 */
dval_t *dv_new_named_var(qfloat_t x, const char *name);
dval_t *dv_new_named_var_qc(qcomplex_t x, const char *name);
dval_t *dv_new_named_var_d(double x, const char *name);

/* ------------------------------------------------------------------------- */
/* Mutators                                                                  */
/* ------------------------------------------------------------------------- */

/**
 * @brief Update the value of a variable node.
 *
 * Sets the node's value and advances the node's internal epoch counter.
 * The next call to dv_eval() on any expression that depends on @p dv will
 * automatically detect the change and recompute. Calling dv_invalidate()
 * before dv_eval() is no longer required.
 *
 * @p dv must be a variable node (created with dv_new_var or dv_new_named_var).
 */
void dv_set_val(dval_t *dv, qcomplex_t value);
void dv_set_val_qf(dval_t *dv, qfloat_t value);
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
 * @brief Return the current primal value of a node.
 *
 * This function evaluates the node if required and returns the current
 * primal value. Use dv_eval() when you want that intent to be explicit;
 * these accessors are convenient value-returning wrappers.
 */
double dv_get_val_d(const dval_t *dv);
qfloat_t dv_get_val_qf(const dval_t *dv);
qcomplex_t dv_get_val(const dval_t *dv);

/**
 * @brief Get the derivative ∂expr/∂wrt (borrowed).
 *
 * The returned pointer is owned by @p expr and must NOT be freed by the caller.
 * The result is cached keyed by @p wrt, so repeated calls are cheap.
 * @p wrt must be a variable node that appears in the DAG rooted at @p expr.
 */
const dval_t *dv_get_deriv(const dval_t *expr, dval_t *wrt);

/* ------------------------------------------------------------------------- */
/* Evaluation                                                                */
/* ------------------------------------------------------------------------- */

/**
 * @brief Evaluate the node, updating the cached primal value.
 *
 * Traverses the DAG recursively, recomputing any nodes whose cache is
 * stale. The result is stored in the node's cache and returned.
 */
qcomplex_t dv_eval(const dval_t *dv);
qfloat_t dv_eval_qf(const dval_t *dv);
double dv_eval_d(const dval_t *dv);

/**
 * @brief Evaluate a scalar expression and compute derivatives with respect to
 *        several variables.
 *
 * This routine evaluates @p expr once and computes derivatives with respect to
 * the variable nodes listed in @p vars. It is useful when one scalar output
 * depends on many input variables, because all requested derivatives can be obtained
 * in a single pass over the expression DAG.
 *
 * @p expr must be a scalar expression DAG. @p vars points to an array of
 * variable nodes whose derivatives are desired; entries not present in the DAG
 * receive a derivative of zero. The order of @p derivs_out matches the order of
 * @p vars.
 *
 * @param expr       Expression whose value and derivatives are required.
 * @param nvars      Number of entries in @p vars and @p derivs_out.
 * @param vars       Variable nodes with respect to which derivatives are taken.
 * @param value_out  Optional destination for the primal value of @p expr.
 * @param derivs_out Output array of length @p nvars for derivative values.
 * @return           0 on success, non-zero on invalid input.
 */
int dv_eval_derivatives(const dval_t *expr,
                        size_t nvars,
                        dval_t *const *vars,
                        qfloat_t *value_out,
                        qfloat_t *derivs_out);

/* ------------------------------------------------------------------------- */
/* Derivative creation (owning)                                              */
/* ------------------------------------------------------------------------- */

/**
 * @brief Build a new DAG node representing a derivative of @p expr w.r.t. @p wrt.
 *
 * @p wrt must be a variable node (created with dv_new_var or dv_new_named_var)
 * that appears in the expression DAG rooted at @p expr.  Only the nominated
 * variable contributes a derivative of 1; all other variable nodes contribute 0.
 *
 * All functions return owning handles; caller must call dv_free() exactly once.
 * dv_get_deriv() returns a *borrowed* pointer (do NOT free it); the result is
 * cached inside @p expr keyed by @p wrt, so repeated calls are cheap.
 *
 * dv_create_2nd_deriv(expr, wrt1, wrt2) computes ∂²expr/(∂wrt1 ∂wrt2).
 * dv_create_3rd_deriv(expr, wrt1, wrt2, wrt3) computes the mixed third derivative.
 * dv_create_nth_deriv(n, expr, wrt) applies d/d(wrt) n times in succession.
 */
dval_t *dv_create_deriv(dval_t *expr, dval_t *wrt);
dval_t *dv_create_2nd_deriv(dval_t *expr, dval_t *wrt1, dval_t *wrt2);
dval_t *dv_create_3rd_deriv(dval_t *expr, dval_t *wrt1, dval_t *wrt2, dval_t *wrt3);
dval_t *dv_create_nth_deriv(unsigned int n, dval_t *expr, dval_t *wrt);

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
 * dv_pow_qc(dv, z) computes dv^z for a constant complex exponent.
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
dval_t *dv_pow_qc(dval_t *dv, qcomplex_t z);
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
 * @brief Increment the reference count of an existing handle.
 *
 * This is mainly useful for container or cache layers that need to keep an
 * additional owning reference to a node returned elsewhere.
 */
void dv_retain(dval_t *dv);

/**
 * @brief Decrement the reference count and free if it reaches zero.
 *
 * Must be called exactly once for every owning handle returned by:
 *   - dv_new_*
 *   - dv_create_*
 *   - dv_add, dv_mul, dv_sin, etc.
 */
void dv_free(dval_t *dv);

/**
 * @brief Return a simplified owning handle for @p dv.
 *
 * The input handle is not consumed. The caller owns the returned handle and
 * must call dv_free() on it.
 */
dval_t *dv_simplify(dval_t *dv);


/* ------------------------------------------------------------------------- */
/* String conversion                                                         */
/* ------------------------------------------------------------------------- */

/**
 * @brief Output style for dv_to_string().
 *
 * style_EXPRESSION  — infix notation, e.g. "{ sin(x₀) | x₀ = 1.0 }"
 *                     or "{ 1 }" when no bindings are needed
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
 *   { expr }
 *   { expr | x₀ = val, ...; [name] = val, ... }
 *
 * The parser also accepts a legacy pure named constant form:
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

/**
 * @brief Construct a dval_t from an expression-style string and return bindings.
 *
 * Behaves like dval_from_string(), but when the parsed expression is symbolic
 * it can also return a heap-allocated array of borrowed symbol bindings.
 * The array itself should be released with free(*bindings_out).
 *
 * If the parsed expression is purely numeric, @p *bindings_out is set to NULL
 * and @p *number_out is set to 0.
 */
dval_t *dval_from_string_with_bindings(const char *s,
                                       dval_binding_t **bindings_out,
                                       size_t *number_out);

/**
 * @brief Find a returned symbolic binding by name.
 *
 * The lookup accepts the normalised binding names returned by
 * dval_from_string_with_bindings(). Bracketed names may be queried either as
 * @p [radius] or @p radius.
 */
dval_binding_t *dval_binding_get(dval_binding_t *bindings,
                                 size_t number,
                                 const char *name);

/**
 * @brief Synonym for dval_binding_get().
 */
dval_binding_t *dval_binding_find(dval_binding_t *bindings,
                                  size_t number,
                                  const char *name);

/**
 * @brief Set a returned symbolic binding from a qfloat_t value.
 */
int dval_binding_set_qf(dval_binding_t *bindings,
                        size_t number,
                        const char *name,
                        qfloat_t value);

/**
 * @brief Set a returned symbolic binding from a qcomplex_t value.
 */
int dval_binding_set_qc(dval_binding_t *bindings,
                        size_t number,
                        const char *name,
                        qcomplex_t value);

/**
 * @brief Set a returned symbolic binding from a double value.
 */
int dval_binding_set_d(dval_binding_t *bindings,
                       size_t number,
                       const char *name,
                       double value);

/**
 * @brief Construct a dval_t from a bare expression string using supplied symbols.
 *
 * This parser accepts the same expression grammar as dval_from_string(), but
 * without the outer braces or binding section. Symbol resolution is performed
 * against the supplied @p names / @p symbols table.
 *
 * Each entry in @p names is matched to the corresponding entry in @p symbols.
 * The parser borrows those symbols during parsing and returns a new owning
 * handle for the parsed expression.
 *
 * Returns an owning handle on success, or NULL on error (details written to
 * stderr). The caller must call dv_free() on the returned pointer exactly once.
 */
dval_t *dval_from_expression_string(const char *expr,
                                    const char *const *names,
                                    dval_t *const *symbols,
                                    size_t nsymbols);

#endif /* DVAL_H */
