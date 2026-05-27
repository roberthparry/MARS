#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <stdbool.h>
#include <stddef.h>
#include "number.h"

/**
 * @file expression.h
 * @brief Lazy, vtable-driven, reference-counted differentiable value type.
 *
 * Ownership rules:
 *   • Any function whose name begins with expr_new_* or expr_create_* returns
 *     an owning handle. The caller must call expr_free() exactly once.
 *
 *   • All arithmetic and mathematical functions (expr_add, expr_mul, expr_sin, …) also
 *     return owning handles. The caller must expr_free() them.
 *
 *   • expr_get_deriv() returns a *borrowed* view. The caller must NOT free it.
 *
 * Internally, each expr_t is a node in a reference-counted DAG.
 * Primal numeric values, cached results, and derivative outputs are all
 * represented with `number_t`.
 *
 * Threading:
 *   • expr_t is currently a single-threaded type.
 *   • Do not read, evaluate, differentiate, or mutate the same DAG from
 *     multiple threads concurrently without external synchronisation.
 */

typedef struct _expr_t expr_t;
typedef struct expr_bindings_t expr_bindings_t;

/**
 * @brief Canonical differentiable singleton constants.
 *
 * These are process-lifetime constant nodes. `EXPR_LN10` evaluates to the
 * current `number_t` ln(10) constant at the library default precision.
 */
extern const expr_t * const EXPR_ZERO;
extern const expr_t * const EXPR_ONE;
extern const expr_t * const EXPR_LN10;

/* ------------------------------------------------------------------------- */
/* Constructors — constants                                                  */
/* ------------------------------------------------------------------------- */

/**
 * @brief Create a constant node from a `number_t` value.
 *
 * Constants have no variable binding; their derivative is always zero.
 * Returns an owning handle; caller must call expr_free() exactly once.
 */
expr_t *expr_new_const(number_t x);

/**
 * @brief Create a named constant node from a `number_t` value.
 *
 * Behaves like `expr_new_const()` but attaches a symbolic name used in
 * expr_to_string() output and debug printing. @p name is copied.
 * Returns an owning handle; caller must call expr_free() exactly once.
 */
expr_t *expr_new_named_const(number_t x, const char *name);

/* ------------------------------------------------------------------------- */
/* Constructors — variables                                                  */
/* ------------------------------------------------------------------------- */

/**
 * @brief Create a variable node from a `number_t` value.
 *
 * Variables are leaf nodes whose value can be updated via `expr_set_val()`.
 * Derivative of a variable with respect to itself is 1.
 * Returns an owning handle; caller must call expr_free() exactly once.
 */
expr_t *expr_new_var(number_t x);

/**
 * @brief Create a named variable node from a `number_t` value.
 *
 * Behaves like `expr_new_var()` but attaches a symbolic name used in
 * expr_to_string() output and debug printing. @p name is copied.
 * Returns an owning handle; caller must call expr_free() exactly once.
 */
expr_t *expr_new_named_var(number_t x, const char *name);

/* ------------------------------------------------------------------------- */
/* Mutators                                                                  */
/* ------------------------------------------------------------------------- */

/**
 * @brief Update the value of a variable or named-constant node.
 *
 * Sets the node's value and advances the node's internal epoch counter.
 * The next call to expr_eval() on any expression that depends on @p dv will
 * automatically detect the change and recompute. Calling expr_invalidate()
 * before expr_eval() is no longer required.
 *
 * @p dv must be a variable node (created with `expr_new_var()` or
 * `expr_new_named_var()`) or a named constant node created with
 * `expr_new_named_const()`.
 */
void expr_set_val(expr_t *dv, number_t value);

/**
 * @brief Attach or replace the symbolic name of a node.
 *
 * @p name is copied. Passing NULL removes any existing name.
 */
void expr_set_name(expr_t *dv, const char *name);

/* ------------------------------------------------------------------------- */
/* Accessors                                                                 */
/* ------------------------------------------------------------------------- */

/**
 * @brief Return the current primal value of a node as an owning `number_t`.
 *
 * This function evaluates the node if required and returns the current
 * primal value. Use `expr_eval()` when you want that intent to be explicit;
 * `expr_get_val()` is a convenient accessor that still returns an owning
 * `number_t`. The returned value does not alias internal node storage; the
 * caller owns it and must later release it with num_destroy().
 */
number_t expr_get_val(const expr_t *dv);

/**
 * @brief Get the derivative ∂expr/∂wrt (borrowed).
 *
 * The returned pointer is owned by @p expr and must NOT be freed by the caller.
 * The result is cached keyed by @p wrt, so repeated calls are cheap.
 * @p wrt must be a variable node that appears in the DAG rooted at @p expr.
 */
const expr_t *expr_get_deriv(const expr_t *expr, const expr_t *wrt);

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
number_t expr_eval(const expr_t *dv);

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
int expr_eval_derivatives(const expr_t *expr,
                        size_t nvars,
                        const expr_t *const *vars,
                        number_t *value_out,
                        number_t *derivs_out);

/* ------------------------------------------------------------------------- */
/* Goal seek                                                                 */
/* ------------------------------------------------------------------------- */

/**
 * @brief Options for expr_goal_seek().
 *
 * Zero-valued fields select library defaults. @p tolerance may be left as
 * zero/invalid to derive a significant-digits tolerance of roughly
 * max(1, abs(target)) * 10^-precision_digits.
 */
typedef struct expr_goal_seek_options {
    size_t precision_digits;
    size_t max_iterations;
    bool allow_complex;
    bool simplify_result;
    number_t tolerance;
} expr_goal_seek_options_t;

/**
 * @brief Result returned by expr_goal_seek().
 *
 * expr_goal_seek() updates the supplied binding nodes in place. @p bindings is
 * therefore a borrowed echo of the input bindings pointer, not an owning copy.
 *
 * @p expr, @p value, and @p residual are owning outputs and must be released
 * with expr_goal_seek_result_clear().
 */
typedef struct expr_goal_seek_result {
    expr_t *expr;
    expr_bindings_t *bindings;
    number_t value;
    number_t residual;
    size_t iterations;
    bool used_complex;
    bool converged;
} expr_goal_seek_result_t;

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
int expr_goal_seek(expr_t *expr,
                 expr_bindings_t *bindings,
                 number_t target,
                 const expr_goal_seek_options_t *options,
                 expr_goal_seek_result_t *result);

/**
 * @brief Release owning fields in a goal-seek result.
 */
void expr_goal_seek_result_clear(expr_goal_seek_result_t *result);

/* ------------------------------------------------------------------------- */
/* Derivative creation (owning)                                              */
/* ------------------------------------------------------------------------- */

/**
 * @brief Build a new DAG node representing a derivative of @p expr w.r.t. @p wrt.
 *
 * @p wrt must be a variable node (created with `expr_new_var()` or
 * `expr_new_named_var()`)
 * that appears in the expression DAG rooted at @p expr.  Only the nominated
 * variable contributes a derivative of 1; all other variable nodes contribute 0.
 *
 * All functions return owning handles; caller must call expr_free() exactly once.
 * expr_get_deriv() returns a *borrowed* pointer (do NOT free it); the result is
 * cached inside @p expr keyed by @p wrt, so repeated calls are cheap.
 *
 * expr_create_2nd_deriv(expr, wrt1, wrt2) computes ∂²expr/(∂wrt1 ∂wrt2).
 * expr_create_3rd_deriv(expr, wrt1, wrt2, wrt3) computes the mixed third derivative.
 * expr_create_nth_deriv(n, expr, wrt) applies d/d(wrt) n times in succession.
 */
expr_t *expr_create_deriv(const expr_t *expr, const expr_t *wrt);
expr_t *expr_create_2nd_deriv(const expr_t *expr, const expr_t *wrt1, const expr_t *wrt2);
expr_t *expr_create_3rd_deriv(const expr_t *expr, const expr_t *wrt1, const expr_t *wrt2, const expr_t *wrt3);
expr_t *expr_create_nth_deriv(unsigned int n, const expr_t *expr, const expr_t *wrt);

/* ------------------------------------------------------------------------- */
/* Arithmetic (graph-building, owning)                                       */
/* ------------------------------------------------------------------------- */

/**
 * All arithmetic functions build a new DAG node representing the operation
 * and return an owning handle. Arguments are retained (not consumed); the
 * caller remains responsible for freeing its own handles.
 *
 * Scalar helpers whose names end in `_num` borrow a constant `number_t`
 * pointer and do not consume or modify the pointed-to value; `expr_num_sub()`
 * and `expr_num_div()` treat the scalar as the left-hand operand
 * (`value - dv` and `value / dv`).
 */
expr_t *expr_neg(const expr_t *dv);
expr_t *expr_add(const expr_t *dv1, const expr_t *dv2);
expr_t *expr_sub(const expr_t *dv1, const expr_t *dv2);
expr_t *expr_mul(const expr_t *dv1, const expr_t *dv2);
expr_t *expr_div(const expr_t *dv1, const expr_t *dv2);

expr_t *expr_add_num(const expr_t *dv, const number_t *value);
expr_t *expr_sub_num(const expr_t *dv, const number_t *value);
expr_t *expr_num_sub(const number_t *value, const expr_t *dv);
expr_t *expr_mul_num(const expr_t *dv, const number_t *value);
expr_t *expr_div_num(const expr_t *dv, const number_t *value);
expr_t *expr_num_div(const number_t *value, const expr_t *dv);

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
int expr_cmp(const expr_t *dv1, const expr_t *dv2);

/* ------------------------------------------------------------------------- */
/* Elementary functions (owning)                                             */
/* ------------------------------------------------------------------------- */

/**
 * All elementary functions build a new DAG node and return an owning handle.
 * Arguments are retained (not consumed).
 *
 * `expr_pow(dv, exponent)` computes dv^exponent for a constant numeric
 * exponent supplied as a borrowed `number_t`.
 * `expr_pow_xp(base, exponent)` computes base^exponent where both operands
 * are differentiable expressions.
 */
expr_t *expr_sin(const expr_t *dv);
expr_t *expr_cos(const expr_t *dv);
expr_t *expr_tan(const expr_t *dv);
expr_t *expr_sec(const expr_t *dv);
expr_t *expr_cosec(const expr_t *dv);
expr_t *expr_cot(const expr_t *dv);
expr_t *expr_sinh(const expr_t *dv);
expr_t *expr_cosh(const expr_t *dv);
expr_t *expr_tanh(const expr_t *dv);
expr_t *expr_sech(const expr_t *dv);
expr_t *expr_cosech(const expr_t *dv);
expr_t *expr_coth(const expr_t *dv);
expr_t *expr_asin(const expr_t *dv);
expr_t *expr_acos(const expr_t *dv);
expr_t *expr_atan(const expr_t *dv);
expr_t *expr_asec(const expr_t *dv);
expr_t *expr_acosec(const expr_t *dv);
expr_t *expr_acot(const expr_t *dv);
expr_t *expr_atan2(const expr_t *dv1, const expr_t *dv2);
expr_t *expr_asinh(const expr_t *dv);
expr_t *expr_acosh(const expr_t *dv);
expr_t *expr_atanh(const expr_t *dv);
expr_t *expr_asech(const expr_t *dv);
expr_t *expr_acosech(const expr_t *dv);
expr_t *expr_acoth(const expr_t *dv);
expr_t *expr_exp(const expr_t *dv);
expr_t *expr_log(const expr_t *dv);
expr_t *expr_log10(const expr_t *dv);
expr_t *expr_sqrt(const expr_t *dv);
expr_t *expr_floor(const expr_t *dv);
expr_t *expr_ceil(const expr_t *dv);
expr_t *expr_pow(const expr_t *dv, const number_t *exponent);
expr_t *expr_pow_xp(const expr_t *dv1, const expr_t *dv2);

/* ------------------------------------------------------------------------- */
/* Special functions (owning)                                                */
/* ------------------------------------------------------------------------- */

/**
 * All special functions build a new DAG node and return an owning handle.
 * Arguments are retained (not consumed).
 *
 * Error functions:   expr_erf, expr_erfc, expr_erfinv, expr_erfcinv
 * Gamma family:      expr_gamma (Γ), expr_lgamma (log Γ), expr_digamma (ψ⁽⁰⁾),
 *                    expr_trigamma (ψ⁽¹⁾), expr_polygamma (ψ⁽ⁿ⁾),
 *                    expr_gammainv (Γ⁻¹), expr_gammainc_lower,
 *                    expr_gammainc_upper, expr_gammainc_P, expr_gammainc_Q
 * Lambert W:         expr_lambert_w0 (principal branch), expr_lambert_wm1 (k=-1)
 * Beta/binomial:     expr_beta (B), expr_logbeta (log B), expr_beta_pdf,
 *                    expr_logbeta_pdf, expr_binomial
 * Normal dist.:      expr_normal_pdf, expr_normal_cdf, expr_normal_logpdf
 * Exponential int.:  expr_ei (Ei), expr_e1 (E₁)
 *
 * Exact/discrete helpers such as expr_partition and expr_gcd are value functions:
 * they evaluate normally, but they are not differentiable.
 */
expr_t *expr_abs(const expr_t *dv);
expr_t *expr_hypot(const expr_t *dv1, const expr_t *dv2);
expr_t *expr_erf(const expr_t *dv);
expr_t *expr_erfc(const expr_t *dv);
expr_t *expr_erfinv(const expr_t *dv);
expr_t *expr_erfcinv(const expr_t *dv);
expr_t *expr_gamma(const expr_t *dv);
expr_t *expr_lgamma(const expr_t *dv);
expr_t *expr_digamma(const expr_t *dv);
expr_t *expr_trigamma(const expr_t *dv);
expr_t *expr_polygamma(unsigned int order, const expr_t *dv);
expr_t *expr_gammainv(const expr_t *dv);
expr_t *expr_gammainc_lower(const expr_t *s, const expr_t *x);
expr_t *expr_gammainc_upper(const expr_t *s, const expr_t *x);
expr_t *expr_gammainc_P(const expr_t *s, const expr_t *x);
expr_t *expr_gammainc_Q(const expr_t *s, const expr_t *x);
expr_t *expr_lambert_w(const expr_t *dv);
expr_t *expr_lambert_w0(const expr_t *dv);
expr_t *expr_lambert_wm1(const expr_t *dv);
expr_t *expr_beta(const expr_t *dv1, const expr_t *dv2);
expr_t *expr_logbeta(const expr_t *dv1, const expr_t *dv2);
expr_t *expr_beta_pdf(const expr_t *x, const expr_t *a, const expr_t *b);
expr_t *expr_logbeta_pdf(const expr_t *x, const expr_t *a, const expr_t *b);
expr_t *expr_binomial(const expr_t *n, const expr_t *k);
expr_t *expr_factorial(const expr_t *n);
expr_t *expr_fibonacci(const expr_t *n);
expr_t *expr_partition(const expr_t *n);
expr_t *expr_isqrt(const expr_t *n);
expr_t *expr_gcd(const expr_t *a, const expr_t *b);
expr_t *expr_lcm(const expr_t *a, const expr_t *b);
expr_t *expr_mod(const expr_t *a, const expr_t *b);
expr_t *expr_modinv(const expr_t *a, const expr_t *b);
expr_t *expr_is_prime(const expr_t *n);
expr_t *expr_next_prime(const expr_t *n);
expr_t *expr_prev_prime(const expr_t *n);
expr_t *expr_bit_and(const expr_t *a, const expr_t *b);
expr_t *expr_bit_or(const expr_t *a, const expr_t *b);
expr_t *expr_bit_xor(const expr_t *a, const expr_t *b);
expr_t *expr_bit_not(const expr_t *a);
expr_t *expr_shl(const expr_t *a, const expr_t *bits);
expr_t *expr_shr(const expr_t *a, const expr_t *bits);
expr_t *expr_factors(const expr_t *n);
expr_t *expr_normal_pdf(const expr_t *dv);
expr_t *expr_normal_cdf(const expr_t *dv);
expr_t *expr_normal_logpdf(const expr_t *dv);
expr_t *expr_ei(const expr_t *dv);
expr_t *expr_e1(const expr_t *dv);

/**
 * @brief Return true when every operation in @p dv is differentiable.
 *
 * This is intended for front-ends such as MARS Lab, so they can hide
 * derivative controls for value-only functions such as gcd(), partition(),
 * factorial(), and primality helpers.
 */
bool expr_is_differentiable(const expr_t *dv);

/* ------------------------------------------------------------------------- */
/* Debug / lifetime                                                          */
/* ------------------------------------------------------------------------- */

/**
 * @brief Increment the reference count for a borrowed handle.
 *
 * Use this when an API returns a borrowed `expr_t *` and the caller needs to
 * keep an owning handle beyond the borrowed lifetime. Pair the retained handle
 * with one later call to expr_free().
 */
void expr_retain(const expr_t *dv);

/**
 * @brief Decrement the reference count and free if it reaches zero.
 *
 * Must be called exactly once for every owning handle returned by:
 *   - expr_new_*
 *   - expr_create_*
 *   - expr_add, expr_mul, expr_sin, etc.
 */
void expr_free(expr_t *dv);

/**
 * @brief Return a simplified owning handle for @p dv.
 *
 * The input handle is not consumed. The caller owns the returned handle and
 * must call expr_free() on it.
 */
expr_t *expr_simplify(const expr_t *dv);


/* ------------------------------------------------------------------------- */
/* String conversion                                                         */
/* ------------------------------------------------------------------------- */

/**
 * @brief Output style for expr_to_string().
 *
 * style_FUNCTION    — C-like function notation, e.g.
 *                     "number expr(number x) { return sin(x); }"
 * style_EXPRESSION  — round-trip infix notation, e.g.
 *                     "{ sin(x₀) | x₀ = 1.0 }"
 * style_TEX         — TeX mathematical notation, e.g. "\left\{ x_{0} \;\middle|\; x_{0} = 1.0 \right\}"
 * style_UNBOUND     — infix expression body without the { ... | bindings }
 *                     wrapper, e.g. "sin(x₀)"
 */
typedef enum {
    style_FUNCTION,
    style_EXPRESSION,
    style_TEX,
    style_UNBOUND
} style_t;

/**
 * @brief Serialise @p dv to a newly allocated string.
 *
 * The format is controlled by @p style (see style_t).
 * The expression style produces output that can be round-tripped through
 * expr_from_string(). The returned string is heap-allocated; the caller
 * must free() it.
 */
char *expr_to_string(const expr_t *dv, style_t style);

/**
 * @brief Print the expression-style string representation of @p dv to stdout.
 *
 * Equivalent to calling expr_to_string(dv, style_EXPRESSION) and printing
 * the result, followed by a newline.
 */
void expr_print(const expr_t *dv);

/* ------------------------------------------------------------------------- */
/* Parsing                                                                   */
/* ------------------------------------------------------------------------- */

/**
 * @brief Construct a expr_t from an expression-style string.
 *
 * Accepts strings in the format produced by expr_to_string(f, style_EXPRESSION):
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
 * expr_bindings_get() and later released with expr_bindings_free(). If bindings are not needed,
 * pass NULL.
 *
 * Returns an owning handle on success, or NULL on error (details written to
 * stderr). The caller must call expr_free() on the returned pointer exactly
 * once.
 */
expr_t *expr_from_string(const char *s, expr_bindings_t **bnd_out);

/**
 * @brief Look up a parsed binding by name.
 *
 * The lookup accepts normalised binding names. Bracketed names may be queried
 * either as @p [radius] or @p radius. Greek-style aliases are normalised too,
 * so a parsed binding may be queried as either @p @pi or @p π, @p @phi or
 * @p φ, @p @gamma or @p γ, and @p @tau or @p τ.
 *
 * Returns the borrowed `expr_t *` leaf for that binding, or NULL if no such
 * binding exists.
 */
expr_t *expr_bindings_get(expr_bindings_t *bnd, const char *name);

/**
 * @brief Destroy an opaque bindings object returned by expr_from_string().
 */
void expr_bindings_free(expr_bindings_t *bnd);

/**
 * @brief Construct a expr_t from a bare expression string using supplied symbols.
 *
 * This parser accepts the same expression grammar as expr_from_string(), but
 * without the outer braces or binding section. Symbol resolution is performed
 * against the supplied @p names / @p symbols table.
 *
 * Each entry in @p names is matched to the corresponding entry in @p symbols.
 * The parser borrows those symbols during parsing and returns a new owning
 * handle for the parsed expression.
 *
 * Returns an owning handle on success, or NULL on error (details written to
 * stderr). The caller must call expr_free() on the returned pointer exactly once.
 */
expr_t *expr_from_expression_string(const char *expr,
                                    const char *const *names,
                                    expr_t *const *symbols,
                                    size_t nsymbols);

#endif /* EXPRESSION_H */
