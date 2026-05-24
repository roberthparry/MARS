#ifndef DVAL_H
#define DVAL_H

#include <stdbool.h>
#include <stddef.h>
#include "number.h"

/**
 * @file dval.h
 * @brief Lazy, vtable-driven, reference-counted differentiable value type.
 *
 * Ownership rules:
 *   • Any function whose name begins with dv_new_* or dv_create_* returns
 *     an owning handle. The caller must call dv_free() exactly once.
 *
 *   • All arithmetic and mathematical functions (dv_add, dv_mul, dv_sin, …) also
 *     return owning handles. The caller must dv_free() them.
 *
 *   • dv_get_deriv() returns a *borrowed* view. The caller must NOT free it.
 *
 * Internally, each dval_t is a node in a reference-counted DAG.
 * Primal numeric values, cached results, and derivative outputs are all
 * represented with `number_t`.
 *
 * Threading:
 *   • dval_t is currently a single-threaded type.
 *   • Do not read, evaluate, differentiate, or mutate the same DAG from
 *     multiple threads concurrently without external synchronisation.
 */

typedef struct _dval_t dval_t;
typedef struct dval_bindings_t dval_bindings_t;

/**
 * @brief Canonical differentiable singleton constants.
 *
 * These are process-lifetime constant nodes. `DV_LN10` evaluates to the
 * current `number_t` ln(10) constant at the library default precision.
 */
extern const dval_t * const DV_ZERO;
extern const dval_t * const DV_ONE;
extern const dval_t * const DV_LN10;

/* ------------------------------------------------------------------------- */
/* Constructors — constants                                                  */
/* ------------------------------------------------------------------------- */

/**
 * @brief Create a constant node from a `number_t` value.
 *
 * Constants have no variable binding; their derivative is always zero.
 * Returns an owning handle; caller must call dv_free() exactly once.
 */
dval_t *dv_new_const(number_t x);

/**
 * @brief Create a named constant node from a `number_t` value.
 *
 * Behaves like `dv_new_const()` but attaches a symbolic name used in
 * dv_to_string() output and debug printing. @p name is copied.
 * Returns an owning handle; caller must call dv_free() exactly once.
 */
dval_t *dv_new_named_const(number_t x, const char *name);

/* ------------------------------------------------------------------------- */
/* Constructors — variables                                                  */
/* ------------------------------------------------------------------------- */

/**
 * @brief Create a variable node from a `number_t` value.
 *
 * Variables are leaf nodes whose value can be updated via `dv_set_val()`.
 * Derivative of a variable with respect to itself is 1.
 * Returns an owning handle; caller must call dv_free() exactly once.
 */
dval_t *dv_new_var(number_t x);

/**
 * @brief Create a named variable node from a `number_t` value.
 *
 * Behaves like `dv_new_var()` but attaches a symbolic name used in
 * dv_to_string() output and debug printing. @p name is copied.
 * Returns an owning handle; caller must call dv_free() exactly once.
 */
dval_t *dv_new_named_var(number_t x, const char *name);

/* ------------------------------------------------------------------------- */
/* Mutators                                                                  */
/* ------------------------------------------------------------------------- */

/**
 * @brief Update the value of a variable or named-constant node.
 *
 * Sets the node's value and advances the node's internal epoch counter.
 * The next call to dv_eval() on any expression that depends on @p dv will
 * automatically detect the change and recompute. Calling dv_invalidate()
 * before dv_eval() is no longer required.
 *
 * @p dv must be a variable node (created with `dv_new_var()` or
 * `dv_new_named_var()`) or a named constant node created with
 * `dv_new_named_const()`.
 */
void dv_set_val(dval_t *dv, number_t value);

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
 * @brief Return the current primal value of a node as an owning `number_t`.
 *
 * This function evaluates the node if required and returns the current
 * primal value. Use `dv_eval()` when you want that intent to be explicit;
 * `dv_get_val()` is a convenient accessor that still returns an owning
 * `number_t`. The returned value does not alias internal node storage; the
 * caller owns it and must later release it with num_destroy().
 */
number_t dv_get_val(const dval_t *dv);

/**
 * @brief Get the derivative ∂expr/∂wrt (borrowed).
 *
 * The returned pointer is owned by @p expr and must NOT be freed by the caller.
 * The result is cached keyed by @p wrt, so repeated calls are cheap.
 * @p wrt must be a variable node that appears in the DAG rooted at @p expr.
 */
const dval_t *dv_get_deriv(const dval_t *expr, const dval_t *wrt);

/* ------------------------------------------------------------------------- */
/* Evaluation                                                                */
/* ------------------------------------------------------------------------- */

/**
 * @brief Evaluate the node, updating the cached primal value.
 *
 * Traverses the DAG recursively, recomputing any nodes whose cache is
 * stale. The result is stored in the node's cache and also returned as an
 * owning `number_t` value for the caller.
 */
number_t dv_eval(const dval_t *dv);

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
 * @param value_out  Optional destination for the owning primal value of @p expr.
 * @param derivs_out Optional output array of length @p nvars for owning
 *                   derivative values.
 * @return           0 on success, non-zero on invalid input or allocation
 *                   failure while building the result array.
 *
 * On success, any non-NULL outputs receive owning `number_t` values that the
 * caller must later release with num_destroy().
 */
int dv_eval_derivatives(const dval_t *expr,
                        size_t nvars,
                        const dval_t *const *vars,
                        number_t *value_out,
                        number_t *derivs_out);

/* ------------------------------------------------------------------------- */
/* Goal seek                                                                 */
/* ------------------------------------------------------------------------- */

/**
 * @brief Options for dv_goal_seek().
 *
 * Zero-valued fields select library defaults. @p tolerance may be left as
 * zero/invalid to derive a significant-digits tolerance of roughly
 * max(1, abs(target)) * 10^-precision_digits.
 */
typedef struct dv_goal_seek_options {
    size_t precision_digits;
    size_t max_iterations;
    bool allow_complex;
    bool simplify_result;
    number_t tolerance;
} dv_goal_seek_options_t;

/**
 * @brief Result returned by dv_goal_seek().
 *
 * dv_goal_seek() updates the supplied binding nodes in place. @p bindings is
 * therefore a borrowed echo of the input bindings pointer, not an owning copy.
 *
 * @p expr, @p value, and @p residual are owning outputs and must be released
 * with dv_goal_seek_result_clear().
 */
typedef struct dv_goal_seek_result {
    dval_t *expr;
    dval_bindings_t *bindings;
    number_t value;
    number_t residual;
    size_t iterations;
    bool used_complex;
    bool converged;
} dv_goal_seek_result_t;

/**
 * @brief Adjust variable bindings so @p expr evaluates to @p target.
 *
 * For one variable, the solver first attempts a real-valued bracketed solve.
 * If that cannot produce a real solution and @p options allows complex solving,
 * it falls back to Newton iteration in the complex plane using automatic
 * derivatives. For several variables, it takes least-norm real Newton steps
 * using the expression gradient.
 *
 * Variables are the non-constant entries in @p bindings. Fixed named constants
 * remain unchanged. Returns 0 on convergence and non-zero on invalid input or
 * failure to converge.
 */
int dv_goal_seek(dval_t *expr,
                 dval_bindings_t *bindings,
                 number_t target,
                 const dv_goal_seek_options_t *options,
                 dv_goal_seek_result_t *result);

/**
 * @brief Release owning fields in a goal-seek result.
 */
void dv_goal_seek_result_clear(dv_goal_seek_result_t *result);

/* ------------------------------------------------------------------------- */
/* Derivative creation (owning)                                              */
/* ------------------------------------------------------------------------- */

/**
 * @brief Build a new DAG node representing a derivative of @p expr w.r.t. @p wrt.
 *
 * @p wrt must be a variable node (created with `dv_new_var()` or
 * `dv_new_named_var()`)
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
dval_t *dv_create_deriv(const dval_t *expr, const dval_t *wrt);
dval_t *dv_create_2nd_deriv(const dval_t *expr, const dval_t *wrt1, const dval_t *wrt2);
dval_t *dv_create_3rd_deriv(const dval_t *expr, const dval_t *wrt1, const dval_t *wrt2, const dval_t *wrt3);
dval_t *dv_create_nth_deriv(unsigned int n, const dval_t *expr, const dval_t *wrt);

/* ------------------------------------------------------------------------- */
/* Arithmetic (graph-building, owning)                                       */
/* ------------------------------------------------------------------------- */

/**
 * All arithmetic functions build a new DAG node representing the operation
 * and return an owning handle. Arguments are retained (not consumed); the
 * caller remains responsible for freeing its own handles.
 *
 * Scalar helpers whose names end in `_num` borrow a constant `number_t`
 * pointer and do not consume or modify the pointed-to value; `dv_num_sub()`
 * and `dv_num_div()` treat the scalar as the left-hand operand
 * (`value - dv` and `value / dv`).
 */
dval_t *dv_neg(const dval_t *dv);
dval_t *dv_add(const dval_t *dv1, const dval_t *dv2);
dval_t *dv_sub(const dval_t *dv1, const dval_t *dv2);
dval_t *dv_mul(const dval_t *dv1, const dval_t *dv2);
dval_t *dv_div(const dval_t *dv1, const dval_t *dv2);

dval_t *dv_add_num(const dval_t *dv, const number_t *value);
dval_t *dv_sub_num(const dval_t *dv, const number_t *value);
dval_t *dv_num_sub(const number_t *value, const dval_t *dv);
dval_t *dv_mul_num(const dval_t *dv, const number_t *value);
dval_t *dv_div_num(const dval_t *dv, const number_t *value);
dval_t *dv_num_div(const number_t *value, const dval_t *dv);

/* ------------------------------------------------------------------------- */
/* Comparison                                                                */
/* ------------------------------------------------------------------------- */

/**
 * @brief Compare the primal values of two nodes.
 *
 * Forces evaluation of both nodes, then compares their primal values
 * lexicographically: real part first, then imaginary part.
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
 * `dv_pow(dv, exponent)` computes dv^exponent for a constant numeric
 * exponent supplied as a borrowed `number_t`.
 * `dv_pow_dv(base, exponent)` computes base^exponent where both operands
 * are differentiable expressions.
 */
dval_t *dv_sin(const dval_t *dv);
dval_t *dv_cos(const dval_t *dv);
dval_t *dv_tan(const dval_t *dv);
dval_t *dv_sinh(const dval_t *dv);
dval_t *dv_cosh(const dval_t *dv);
dval_t *dv_tanh(const dval_t *dv);
dval_t *dv_asin(const dval_t *dv);
dval_t *dv_acos(const dval_t *dv);
dval_t *dv_atan(const dval_t *dv);
dval_t *dv_atan2(const dval_t *dv1, const dval_t *dv2);
dval_t *dv_asinh(const dval_t *dv);
dval_t *dv_acosh(const dval_t *dv);
dval_t *dv_atanh(const dval_t *dv);
dval_t *dv_exp(const dval_t *dv);
dval_t *dv_log(const dval_t *dv);
dval_t *dv_log10(const dval_t *dv);
dval_t *dv_sqrt(const dval_t *dv);
dval_t *dv_floor(const dval_t *dv);
dval_t *dv_ceil(const dval_t *dv);
dval_t *dv_pow(const dval_t *dv, const number_t *exponent);
dval_t *dv_pow_dv(const dval_t *dv1, const dval_t *dv2);

/* ------------------------------------------------------------------------- */
/* Special functions (owning)                                                */
/* ------------------------------------------------------------------------- */

/**
 * All special functions build a new DAG node and return an owning handle.
 * Arguments are retained (not consumed).
 *
 * Error functions:   dv_erf, dv_erfc, dv_erfinv, dv_erfcinv
 * Gamma family:      dv_gamma (Γ), dv_lgamma (log Γ), dv_digamma (ψ),
 *                    dv_trigamma (ψ₁), dv_gammainv (Γ⁻¹)
 * Lambert W:         dv_lambert_w0 (principal branch), dv_lambert_wm1 (k=-1)
 * Beta:              dv_beta (B), dv_logbeta (log B)
 * Normal dist.:      dv_normal_pdf, dv_normal_cdf, dv_normal_logpdf
 * Exponential int.:  dv_ei (Ei), dv_e1 (E₁)
 */
dval_t *dv_abs(const dval_t *dv);
dval_t *dv_hypot(const dval_t *dv1, const dval_t *dv2);
dval_t *dv_erf(const dval_t *dv);
dval_t *dv_erfc(const dval_t *dv);
dval_t *dv_erfinv(const dval_t *dv);
dval_t *dv_erfcinv(const dval_t *dv);
dval_t *dv_gamma(const dval_t *dv);
dval_t *dv_lgamma(const dval_t *dv);
dval_t *dv_digamma(const dval_t *dv);
dval_t *dv_trigamma(const dval_t *dv);
dval_t *dv_gammainv(const dval_t *dv);
dval_t *dv_lambert_w0(const dval_t *dv);
dval_t *dv_lambert_wm1(const dval_t *dv);
dval_t *dv_beta(const dval_t *dv1, const dval_t *dv2);
dval_t *dv_logbeta(const dval_t *dv1, const dval_t *dv2);
dval_t *dv_normal_pdf(const dval_t *dv);
dval_t *dv_normal_cdf(const dval_t *dv);
dval_t *dv_normal_logpdf(const dval_t *dv);
dval_t *dv_ei(const dval_t *dv);
dval_t *dv_e1(const dval_t *dv);

/* ------------------------------------------------------------------------- */
/* Debug / lifetime                                                          */
/* ------------------------------------------------------------------------- */

/**
 * @brief Increment the reference count for a borrowed handle.
 *
 * Use this when an API returns a borrowed `dval_t *` and the caller needs to
 * keep an owning handle beyond the borrowed lifetime. Pair the retained handle
 * with one later call to dv_free().
 */
void dv_retain(const dval_t *dv);

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
dval_t *dv_simplify(const dval_t *dv);


/* ------------------------------------------------------------------------- */
/* String conversion                                                         */
/* ------------------------------------------------------------------------- */

/**
 * @brief Output style for dv_to_string().
 *
 * style_EXPRESSION  — infix notation, e.g. "{ sin(x₀) | x₀ = 1.0 }"
 *                     or "{ 1 }" when no bindings are needed
 * style_FUNCTION    — C-like function notation, e.g.
 *                     "number expr(number x) { return sin(x); }"
 * style_TEX         — TeX mathematical notation, e.g. "\left\{ x_{0} \;\middle|\; x_{0} = 1.0 \right\}"
 */
typedef enum {
    style_FUNCTION,
    style_EXPRESSION,
    style_TEX
} style_t;

/**
 * @brief Serialise @p dv to a newly allocated string.
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
 * If there is no binding section and the expression still contains symbolic
 * names, bare inference uses these defaults:
 *   @p e, @p pi, @p π, @p @pi, @p @phi, and @p @gamma become named constants
 *   with built-in values
 *   @p a, @p b, @p c, @p d and their indexed forms become named constant
 *   placeholders initialised to NaN
 *   everything else, including @p τ and @p @tau, becomes a named variable
 *   initialised to NaN
 *
 * If @p bnd_out is non-NULL and the parsed expression is symbolic, the
 * function also returns an opaque bindings object that can be queried with
 * dval_bindings_get() and later released with dval_bindings_free(). If bindings are not needed,
 * pass NULL.
 *
 * Returns an owning handle on success, or NULL on error (details written to
 * stderr). The caller must call dv_free() on the returned pointer exactly
 * once.
 */
dval_t *dval_from_string(const char *s, dval_bindings_t **bnd_out);

/**
 * @brief Look up a parsed binding by name.
 *
 * The lookup accepts normalised binding names. Bracketed names may be queried
 * either as @p [radius] or @p radius. Greek-style aliases are normalised too,
 * so a parsed binding may be queried as either @p @pi or @p π, @p @phi or
 * @p φ, @p @gamma or @p γ, and @p @tau or @p τ.
 *
 * Returns the borrowed `dval_t *` leaf for that binding, or NULL if no such
 * binding exists.
 */
dval_t *dval_bindings_get(dval_bindings_t *bnd, const char *name);

/**
 * @brief Destroy an opaque bindings object returned by dval_from_string().
 */
void dval_bindings_free(dval_bindings_t *bnd);

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
