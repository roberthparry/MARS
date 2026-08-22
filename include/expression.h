#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "number.h"
#include "ustring.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

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
extern const expr_t *const EXPR_ZERO;
extern const expr_t *const EXPR_ONE;
extern const expr_t *const EXPR_LN10;

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
 * expr_to_text() output and debug printing. @p name is copied.
 * Returns an owning handle; caller must call expr_free() exactly once.
 */
expr_t *expr_new_named_const(number_t x, const char *name);

/**
 * @brief Create a named constant node from string text.
 *
 * This is the string_t-based counterpart to expr_new_named_const(). The name
 * is borrowed, normalised, and copied into the expression node.
 */
expr_t *expr_new_named_const_text(number_t x, const string_t *name);

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
 * expr_to_text() output and debug printing. @p name is copied.
 * Returns an owning handle; caller must call expr_free() exactly once.
 */
expr_t *expr_new_named_var(number_t x, const char *name);

/**
 * @brief Create a named variable node from string text.
 *
 * This is the string_t-based counterpart to expr_new_named_var(). The name is
 * borrowed, normalised, and copied into the expression node.
 */
expr_t *expr_new_named_var_text(number_t x, const string_t *name);

/* ------------------------------------------------------------------------- */
/* Mutators                                                                  */
/* ------------------------------------------------------------------------- */

/**
 * @brief Update the value of a variable or named-constant node.
 *
 * Sets the node's value and advances the node's internal epoch counter.
 * The next call to expr_eval() on any expression that depends on @p expr will
 * automatically detect the change and recompute. Calling expr_invalidate()
 * before expr_eval() is no longer required.
 *
 * @p expr must be a variable node (created with `expr_new_var()` or
 * `expr_new_named_var()`) or a named constant node created with
 * `expr_new_named_const()`.
 */
void expr_set_val(expr_t *expr, number_t value);

/**
 * @brief Attach or replace the symbolic name of a node.
 *
 * @p name is copied. Passing NULL removes any existing name.
 */
void expr_set_name(expr_t *expr, const char *name);

/**
 * @brief Set the display name from string text.
 *
 * This is the string_t-based counterpart to expr_set_name(). The name is
 * borrowed, normalised, and copied into the expression node.
 */
void expr_set_name_text(expr_t *expr, const string_t *name);

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
number_t expr_get_val(const expr_t *expr);

/**
 * @brief Get the derivative ∂expr/∂wrt (borrowed).
 *
 * The returned pointer is owned by @p expr and must NOT be freed by the caller.
 * The result is cached keyed by @p wrt, so repeated calls are cheap.
 * @p wrt must be a variable node that appears in the DAG rooted at @p expr.
 */
const expr_t *expr_get_deriv(const expr_t *expr, const expr_t *wrt);

/**
 * @brief Return true when @p expr is a variable node.
 */
bool expr_is_variable(const expr_t *expr);

/**
 * @brief Borrow the symbolic display name attached to @p expr, if any.
 *
 * The returned pointer is owned by @p expr and remains valid only while that
 * expression node remains alive and unchanged.
 */
const char *expr_symbol_name(const expr_t *expr);

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
number_t expr_eval(const expr_t *expr);

/**
 * @brief Evaluate a scalar expression and compute derivatives with respect to
 *        several variables.
 *
 * This reverse-mode routine evaluates @p expr once and computes derivatives with respect to the variable nodes listed
 * in @p vars. It is useful when one scalar output depends on many input variables, because all requested derivatives
 * can be obtained in a single reverse pass over the expression DAG. It is separate from symbolic differentiation and
 * from any forward-mode directional-derivative evaluation.
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
int expr_eval_derivatives(const expr_t *expr, size_t nvars, const expr_t *const *vars, number_t *value_out,
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
int expr_goal_seek(expr_t *expr, expr_bindings_t *bindings, number_t target, const expr_goal_seek_options_t *options,
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
/* Symbolic integration (owning)                                             */
/* ------------------------------------------------------------------------- */

/**
 * @brief Try to build a symbolic antiderivative of @p expr with respect to @p wrt.
 *
 * Returns an owning expression @c F such that dF/d(@p wrt) equals @p expr for
 * the currently supported rule set. Additive expressions may carry unsupported
 * pieces as unevaluated integral nodes; a fully unsupported non-additive input
 * still returns NULL so callers can fall back to numerical integration.
 *
 * The first rule set intentionally covers simple, reliable cases: constants,
 * sums/differences, constant multiples, powers of the integration variable,
 * reciprocal 1/x, log(x), and affine exp/sin/cos/tan/sinh/cosh/tanh terms.
 */
expr_t *expr_integrate(const expr_t *expr, const expr_t *wrt);

/**
 * @brief Build a user-facing indefinite-integral family.
 *
 * Returns the symbolic antiderivative from expr_integrate() plus an arbitrary
 * named constant. The first constant is C_0, with later C_n names chosen if
 * earlier ones already appear in the expression.
 */
expr_t *expr_integrate_family(const expr_t *expr, const expr_t *wrt);

/**
 * @brief Create an arbitrary integration constant whose name does not collide with supplied expressions.
 *
 * The returned named constant is selected from C, C_0, C_1, and subsequent indexed names after inspecting
 * @p expr, @p wrt, and @p antiderivative for existing symbols.
 *
 * @return A newly allocated named constant, or NULL on allocation failure.
 */
expr_t *expr_new_integration_constant(const expr_t *expr, const expr_t *wrt, const expr_t *antiderivative);

/**
 * @brief Build an unevaluated integral node representing ∫^upper f(t) dt.
 *
 * The node stores the displayed integrand with its dummy variable substituted
 * by @p upper. Numeric evaluation computes the definite integral from 0 to
 * @p upper, and differentiation applies the fundamental theorem of calculus
 * with the chain rule.
 */
expr_t *expr_integral(const expr_t *integrand, const expr_t *wrt);

/**
 * @brief Return an owning copy of @p expr.
 */
expr_t *expr_clone(const expr_t *expr);

/**
 * @brief Substitute all references to @p needle in @p expr with @p replacement.
 *
 * Returns an owning expression. Inputs are borrowed and are not consumed.
 */
expr_t *expr_substitute(const expr_t *expr, const expr_t *needle, const expr_t *replacement);

/**
 * @brief Return an owning display-oriented simplification of @p expr.
 *
 * This helper expands simple products over sums before simplification so UI
 * clients can present friendlier algebra without walking the expression tree.
 */
expr_t *expr_display_simplified(const expr_t *expr);

/**
 * @brief Return an owning fully expanded display form of @p expr.
 *
 * Unlike expr_display_simplified(), this explicitly distributes products when
 * both factors are sums. It is intended for callers that request expanded
 * polynomial or algebraic output.
 */
expr_t *expr_display_expanded(const expr_t *expr);

/**
 * @brief Find a user-facing note about undefined integral bounds, if any.
 *
 * Writes a short explanation into @p out and returns true when a note was
 * found. This is intended for front-ends that display unevaluated integral
 * expressions and want to explain domain failures near their bounds.
 */
bool expr_integral_value_note(const expr_t *expr, char *out, size_t out_size);

/**
 * @brief Report whether @p expr contains an integral operation.
 *
 * Front-ends can use this to distinguish an integral operation that should be
 * evaluated symbolically from an ordinary expression awaiting binding values.
 */
bool expr_contains_integral_operation(const expr_t *expr);

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
 * (`value - expr` and `value / expr`).
 */
expr_t *expr_neg(const expr_t *expr);
expr_t *expr_add(const expr_t *expr1, const expr_t *expr2);
expr_t *expr_sub(const expr_t *expr1, const expr_t *expr2);
expr_t *expr_mul(const expr_t *expr1, const expr_t *expr2);
expr_t *expr_div(const expr_t *expr1, const expr_t *expr2);

expr_t *expr_add_num(const expr_t *expr, const number_t *value);
expr_t *expr_sub_num(const expr_t *expr, const number_t *value);
expr_t *expr_num_sub(const number_t *value, const expr_t *expr);
expr_t *expr_mul_num(const expr_t *expr, const number_t *value);
expr_t *expr_div_num(const expr_t *expr, const number_t *value);
expr_t *expr_num_div(const number_t *value, const expr_t *expr);

/* ------------------------------------------------------------------------- */
/* Comparison                                                                */
/* ------------------------------------------------------------------------- */

/**
 * @brief Compare the primal values of two nodes.
 *
 * Forces evaluation of both nodes, then compares their primal values
 * lexicographically: real part first, then imaginary part.
 * Returns -1 if expr1 < expr2, 0 if equal, +1 if expr1 > expr2.
 */
int expr_cmp(const expr_t *expr1, const expr_t *expr2);

/* ------------------------------------------------------------------------- */
/* Elementary functions (owning)                                             */
/* ------------------------------------------------------------------------- */

/**
 * @brief Apply a registered unary expression function by name.
 *
 * The name is resolved by the expression parser's perfect hash, including all
 * accepted aliases. @p argument is borrowed. The returned expression is an
 * owning handle and must be released with expr_free(). When requested,
 * @p canonical_name_out receives a borrowed process-lifetime canonical name.
 *
 * @param name Function name or accepted alias.
 * @param argument Expression supplied as the function argument.
 * @param canonical_name_out Optional destination for the canonical name.
 * @return A newly allocated expression, or NULL if the name is not a registered unary function.
 */
expr_t *expr_apply_unary_function(const char *name, const expr_t *argument, const char **canonical_name_out);

/**
 * All elementary functions build a new DAG node and return an owning handle.
 * Arguments are retained (not consumed).
 *
 * `expr_pow(expr, exponent)` computes expr^exponent for a constant numeric
 * exponent supplied as a borrowed `number_t`.
 * `expr_pow_xp(base, exponent)` computes base^exponent where both operands
 * are differentiable expressions.
 */
expr_t *expr_sin(const expr_t *expr);
expr_t *expr_cos(const expr_t *expr);
expr_t *expr_tan(const expr_t *expr);
expr_t *expr_sec(const expr_t *expr);
expr_t *expr_cosec(const expr_t *expr);
expr_t *expr_cot(const expr_t *expr);
expr_t *expr_versin(const expr_t *expr);
expr_t *expr_vercos(const expr_t *expr);
expr_t *expr_coversin(const expr_t *expr);
expr_t *expr_covercos(const expr_t *expr);
expr_t *expr_haversin(const expr_t *expr);
expr_t *expr_havercos(const expr_t *expr);
expr_t *expr_hacoversin(const expr_t *expr);
expr_t *expr_hacovercos(const expr_t *expr);
expr_t *expr_sinh(const expr_t *expr);
expr_t *expr_cosh(const expr_t *expr);
expr_t *expr_tanh(const expr_t *expr);
expr_t *expr_sech(const expr_t *expr);
expr_t *expr_cosech(const expr_t *expr);
expr_t *expr_coth(const expr_t *expr);
expr_t *expr_asin(const expr_t *expr);
expr_t *expr_acos(const expr_t *expr);
expr_t *expr_atan(const expr_t *expr);
expr_t *expr_asec(const expr_t *expr);
expr_t *expr_acosec(const expr_t *expr);
expr_t *expr_acot(const expr_t *expr);
expr_t *expr_arcversin(const expr_t *expr);
expr_t *expr_arcvercos(const expr_t *expr);
expr_t *expr_arccoversin(const expr_t *expr);
expr_t *expr_arccovercos(const expr_t *expr);
expr_t *expr_archaversin(const expr_t *expr);
expr_t *expr_archavercos(const expr_t *expr);
expr_t *expr_archacoversin(const expr_t *expr);
expr_t *expr_archacovercos(const expr_t *expr);
expr_t *expr_atan2(const expr_t *expr1, const expr_t *expr2);
expr_t *expr_asinh(const expr_t *expr);
expr_t *expr_acosh(const expr_t *expr);
expr_t *expr_atanh(const expr_t *expr);
expr_t *expr_asech(const expr_t *expr);
expr_t *expr_acosech(const expr_t *expr);
expr_t *expr_acoth(const expr_t *expr);
expr_t *expr_exp(const expr_t *expr);
expr_t *expr_log(const expr_t *expr);
/**
 * @brief Construct a natural-logarithm expression.
 *
 * This is a shorthand for expr_log() and constructs the same symbolic operation.
 *
 * @param expr Operand expression; it is retained.
 * @return A newly allocated natural-logarithm expression, or NULL on error.
 */
expr_t *expr_ln(const expr_t *expr);
expr_t *expr_log10(const expr_t *expr);
/**
 * @brief Construct a common-logarithm expression.
 *
 * This is a shorthand for expr_log10() and constructs the same symbolic operation.
 *
 * @param expr Operand expression; it is retained.
 * @return A newly allocated common-logarithm expression, or NULL on error.
 */
expr_t *expr_lg(const expr_t *expr);
/**
 * @brief Construct the single-valued principal square root of an expression.
 *
 * For a complex value z, this is exp(Log(z) / 2), where the principal argument
 * lies in (-pi, pi]. Equivalently, the result has non-negative real part; on
 * the negative real axis it has positive imaginary part.
 *
 * @param expr Expression whose principal square root is required; it is retained.
 * @return A newly allocated principal-square-root expression, or NULL on error.
 */
expr_t *expr_sqrt(const expr_t *expr);
/**
 * @brief Construct the single-valued principal cube root of an expression.
 *
 * @param expr Radicand expression; it is retained.
 * @return A newly allocated principal-cube-root expression, or NULL on error.
 */
expr_t *expr_cubrt(const expr_t *expr);
/**
 * @brief Construct the single-valued principal root of an expression.
 *
 * The order must evaluate to a real integer greater than one.
 *
 * @param expr Radicand expression; it is retained.
 * @param order Root-order expression; it is retained.
 * @return A newly allocated principal-root expression, or NULL on error.
 */
expr_t *expr_root(const expr_t *expr, const expr_t *order);
expr_t *expr_floor(const expr_t *expr);
expr_t *expr_ceil(const expr_t *expr);
/**
 * @brief Construct a power with a constant numeric exponent.
 *
 * An explicit reciprocal-integer exponent is retained so a result consumer can
 * enumerate the complete root family. Numeric evaluation still selects the
 * principal member; use expr_sqrt() when a single-valued square root is meant.
 *
 * @param expr Base expression; it is retained.
 * @param exponent Constant exponent; it is borrowed.
 * @return A newly allocated power expression, or NULL on error.
 */
expr_t *expr_pow(const expr_t *expr, const number_t *exponent);
expr_t *expr_pow_xp(const expr_t *expr1, const expr_t *expr2);

/**
 * @brief Build a formal finite summation with explicit inclusive bounds.
 *
 * The returned expression represents @c sum(term,index,lower,upper) without
 * evaluating it. All inputs are borrowed; the caller owns the returned node.
 */
expr_t *expr_new_finite_summation_range(const expr_t *term, const expr_t *index, const expr_t *lower,
                                        const expr_t *upper);

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
 * Polylogarithms:    expr_dilog (Li₂), expr_polylog (Liₙ),
 *                    expr_legendre_chi (χₙ), expr_appell_f1 (F₁)
 * Bessel functions:  expr_bessel_j (Jᵥ), expr_bessel_y (Yᵥ)
 * Lommel function:    expr_lommel_s (s_(μ,ν))
 * Lambert W:         expr_lambert_w0 (principal branch), expr_lambert_wm1 (k=-1),
 *                    expr_lambert_wn (integer branch n)
 * Beta/binomial:     expr_beta (B), expr_logbeta (log B), expr_beta_pdf,
 *                    expr_logbeta_pdf, expr_binomial
 * Normal dist.:      expr_normal_pdf, expr_normal_cdf, expr_normal_logpdf
 * Exponential int.:  expr_Ei (Ei), expr_E1 (E₁)
 *
 * Exact/discrete helpers such as expr_partition and expr_gcd are value functions:
 * they evaluate normally, but they are not differentiable.
 */
expr_t *expr_abs(const expr_t *expr);
/**
 * @brief Construct the complex conjugate of an expression.
 *
 * @param expr Expression to conjugate; it is retained rather than consumed.
 * @return     Newly allocated conjugate expression, or NULL on error.
 */
expr_t *expr_conj(const expr_t *expr);
expr_t *expr_hypot(const expr_t *expr1, const expr_t *expr2);
expr_t *expr_erf(const expr_t *expr);
expr_t *expr_erfc(const expr_t *expr);
expr_t *expr_erfinv(const expr_t *expr);
expr_t *expr_erfcinv(const expr_t *expr);
expr_t *expr_gamma(const expr_t *expr);
expr_t *expr_lgamma(const expr_t *expr);
expr_t *expr_digamma(const expr_t *expr);
expr_t *expr_trigamma(const expr_t *expr);
expr_t *expr_polygamma(unsigned int order, const expr_t *expr);
/**
 * @brief Construct a Riemann zeta function expression.
 *
 * @param expr Argument expression; it is retained.
 * @return A newly allocated ζ(expr) expression, or NULL on error.
 */
expr_t *expr_zeta(const expr_t *expr);
/**
 * @brief Construct a Hurwitz zeta function expression.
 *
 * @param s Exponent expression; it is retained.
 * @param a Shift expression; it is retained.
 * @return A newly allocated ζ(s, a) expression, or NULL on error.
 */
expr_t *expr_zetah(const expr_t *s, const expr_t *a);
/**
 * @brief Construct a first Hurwitz zeta derivative expression.
 *
 * @param s Exponent expression; it is retained.
 * @param a Shift expression; it is retained.
 * @return A newly allocated ∂ζ(s, a)/∂s expression, or NULL on error.
 */
expr_t *expr_zatahp(const expr_t *s, const expr_t *a);
/**
 * @brief Construct a first Riemann zeta derivative expression.
 *
 * @param expr Argument expression; it is retained.
 * @return A newly allocated ζ′(expr) expression, or NULL on error.
 */
expr_t *expr_zetap(const expr_t *expr);
expr_t *expr_dilog(const expr_t *expr);
expr_t *expr_polylog(unsigned int order, const expr_t *expr);
expr_t *expr_legendre_chi(unsigned int order, const expr_t *expr);
expr_t *expr_bessel_j(const expr_t *order, const expr_t *argument);
expr_t *expr_bessel_y(const expr_t *order, const expr_t *argument);
expr_t *expr_lommel_s(const expr_t *mu, const expr_t *nu, const expr_t *argument);
expr_t *expr_appell_f1(const expr_t *a, const expr_t *b1, const expr_t *b2, const expr_t *c, const expr_t *x,
                       const expr_t *y);
expr_t *expr_lauricella_f(const expr_t *a, size_t variable_count, const expr_t *const *b, const expr_t *c,
                          const expr_t *const *x);
expr_t *expr_hypergeometric_pFq(size_t upper_count, const expr_t *const *upper, size_t lower_count,
                                const expr_t *const *lower, const expr_t *argument);
expr_t *expr_gammainv(const expr_t *expr);
expr_t *expr_gammainc_lower(const expr_t *s, const expr_t *x);
expr_t *expr_gammainc_upper(const expr_t *s, const expr_t *x);
expr_t *expr_gammainc_P(const expr_t *s, const expr_t *x);
expr_t *expr_gammainc_Q(const expr_t *s, const expr_t *x);
expr_t *expr_lambert_w(const expr_t *expr);
expr_t *expr_lambert_wn(const expr_t *branch, const expr_t *expr);
expr_t *expr_lambert_w0(const expr_t *expr);
expr_t *expr_lambert_wm1(const expr_t *expr);
expr_t *expr_beta(const expr_t *expr1, const expr_t *expr2);
expr_t *expr_logbeta(const expr_t *expr1, const expr_t *expr2);
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
expr_t *expr_normal_pdf(const expr_t *expr);
expr_t *expr_normal_cdf(const expr_t *expr);
expr_t *expr_normal_logpdf(const expr_t *expr);
expr_t *expr_pdf(const expr_t *expr);
expr_t *expr_cdf(const expr_t *expr);
expr_t *expr_logpdf(const expr_t *expr);
expr_t *expr_Ei(const expr_t *expr);
expr_t *expr_E1(const expr_t *expr);

/**
 * @brief Return true when every operation in @p expr is differentiable.
 *
 * This is intended for front-ends such as MARS Lab, so they can hide
 * derivative controls for value-only functions such as gcd(), partition(),
 * factorial(), and primality helpers.
 */
bool expr_is_differentiable(const expr_t *expr);

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
void expr_retain(const expr_t *expr);

/**
 * @brief Decrement the reference count and free if it reaches zero.
 *
 * Must be called exactly once for every owning handle returned by:
 *   - expr_new_*
 *   - expr_create_*
 *   - expr_add, expr_mul, expr_sin, etc.
 */
void expr_free(expr_t *expr);

/**
 * @brief Return a simplified owning handle for @p expr.
 *
 * The input handle is not consumed. The caller owns the returned handle and
 * must call expr_free() on it.
 */
expr_t *expr_simplify(const expr_t *expr);

/**
 * @brief Return an algebraically equivalent expression arranged for readable presentation.
 *
 * Ordinary simplification is completed first. The subsequent pass preserves the expression's value while preferring
 * symmetric radicals, common factors and balanced forms. The input is not consumed; the caller owns the result.
 */
expr_t *expr_beautify(const expr_t *expr);

/* ------------------------------------------------------------------------- */
/* String conversion                                                         */
/* ------------------------------------------------------------------------- */

/**
 * @brief Output style for expr_to_text().
 *
 * style_FUNCTION    — MARS function notation, including @c array parameters and @c array @c const parameters for
 *                     array-valued bindings, e.g. "expression expr(array x) { return sin(x). }"; a full stop followed
 *                     by whitespace or end of input terminates a statement, while an internal full stop multiplies;
 *                     paired backticks delimit comments and two opening backticks introduce a line comment
 * style_EXPRESSION  — round-trip infix notation, including array bindings such as
 *                     "{ sin(x₀) | x₀ = [1, 2, 3] }"; @c [] and @c [?] both denote an unspecified array
 * style_LATEX         — TeX mathematical notation, e.g. "\left\{ x_{0} \;\middle|\; x_{0} = 1.0 \right\}"
 * style_UNBOUND     — infix expression body without the { ... | bindings }
 *                     wrapper, e.g. "sin(x₀)"
 */
typedef enum { style_FUNCTION, style_EXPRESSION, style_LATEX, style_UNBOUND } style_t;

/**
 * @brief Serialise @p expr to newly allocated text.
 *
 * The format is controlled by @p style (see style_t).
 * The expression style produces output that can be round-tripped through
 * expr_from_string(). The returned string is owned by the caller and must be
 * released with string_free().
 */
string_t *expr_to_text(const expr_t *expr, style_t style);

/**
 * @brief Serialise @p expr to a newly allocated C string.
 *
 * This is the plain C-string counterpart to expr_to_text(). The format is
 * controlled by @p style (see style_t). The returned string is allocated with
 * malloc() and must be released with free().
 */
char *expr_to_string(const expr_t *expr, style_t style);

/**
 * @brief Serialise an expression body using MARS function notation.
 *
 * Unlike @c expr_to_text(..., style_FUNCTION), this returns only the expression
 * used after a function's @c return keyword. It is intended for native
 * containers, such as matrices, that provide their own function wrapper.
 * The caller owns the returned string and must release it with
 * @c string_free().
 */
string_t *expr_to_function_body_text(const expr_t *expr);

/**
 * @brief Serialise an expression body to a C string using MARS function notation.
 *
 * This is the plain C-string counterpart to expr_to_function_body_text(). The
 * caller owns the returned buffer and must release it with free().
 */
char *expr_to_function_body(const expr_t *expr);

/**
 * @brief Return the TeX expression body without binding wrappers.
 *
 * The returned C string is allocated with malloc() and must be released with
 * free(). Returns NULL on invalid input or allocation failure.
 */
char *expr_to_TeX_body(const expr_t *expr);

/**
 * @brief Return a display-oriented TeX expression body with line breaks.
 *
 * This keeps the ordinary expr_to_TeX_body() spelling available for exact
 * serialisation, but may wrap long additive expressions in an amsmath
 * aligned environment for display. @p line_limit is a soft character budget;
 * pass 0 for the default.
 *
 * The returned C string is allocated with malloc() and must be released with
 * free(). Returns NULL on invalid input or allocation failure.
 */
char *expr_to_TeX_body_wrapped(const expr_t *expr, size_t line_limit);

/**
 * @brief Format expression-aware text into a new string_t from a va_list.
 *
 * Supports ordinary string formatting plus expression conversions:
 * `%n` expression style, `%nu` unbound style, `%nt` TeX style, and
 * `%nf` function style.
 *
 * `%N` selects scientific numeric formatting for embedded number_t values
 * while keeping the same expression style selection rules.
 */
string_t *expr_vsprintf_text(const char *fmt, va_list ap);

/**
 * @brief Format expression-aware text into a new string_t.
 *
 * The caller owns the returned string and must release it with string_free().
 */
string_t *expr_sprintf_text(const char *fmt, ...);

/**
 * @brief Format expression-aware text into a caller-provided buffer.
 */
int expr_sprintf(char *out, size_t out_size, const char *fmt, ...);

/**
 * @brief Print expression-aware formatted text to stdout.
 *
 * Supports `%n` expression style, `%nu` unbound style, `%nt` TeX style, and
 * `%nf` function style.
 *
 * `%N` selects scientific numeric formatting for embedded number_t values
 * while keeping the same expression style selection rules.
 */
int expr_printf(const char *fmt, ...);

/**
 * @brief Print the expression-style string representation of @p expr to stdout.
 *
 * Equivalent to calling expr_to_text(expr, style_EXPRESSION) and printing
 * the result, followed by a newline.
 */
void expr_print(const expr_t *expr);

/* ------------------------------------------------------------------------- */
/* Parsing                                                                   */
/* ------------------------------------------------------------------------- */

/**
 * @brief Construct a expr_t from an expression-style string.
 *
 * Accepts strings in the format produced by expr_to_text(f, style_EXPRESSION):
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
 * that do not fit the single-letter-plus-subscript rule. The input-only
 * @p $[name] alias is accepted and canonicalised to @p [name].
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
 * The @p @S integral operator is completed during parsing. Without an upper
 * substitution, @c @S f(x) dx returns the indefinite family @c F(x)+C_0.
 * With an upper substitution, @c @S^u f(x) dx returns the single
 * antiderivative @c F(u). Literal @c ∫ syntax constructs an integral node
 * instead.
 *
 * If @p bnd_out is non-NULL and the parsed expression is symbolic, the
 * function also returns an opaque bindings object that can be queried with
 * expr_bindings_get() and later released with expr_bindings_free(). If bindings are not needed,
 * pass NULL.
 *
 * Apart from operators such as @p @S that explicitly request evaluation,
 * parsing is source-preserving: it canonicalises notation such as @p pi to
 * @p π and @p e to @p e, but it does not run a whole-expression algebraic
 * simplification pass. Call expr_simplify() explicitly when the simplified
 * equivalent is wanted.
 *
 * The outer expression braces are optional on input. Bare shorthand such as
 * @c x+y is equivalent to @c {x+y}; MARS infers and returns its bindings.
 *
 * Returns an owning handle on success, or NULL on error (details written to
 * stderr). The caller must call expr_free() on the returned pointer exactly
 * once.
 */
expr_t *expr_from_string(const char *s, expr_bindings_t **bnd_out);

/**
 * @brief Parse an expression and return its native series derivation in TeX when applicable.
 *
 * This accepts the same grammar and ownership rules as expr_from_string(). When the source contains a recognised
 * finite series ellipsis, @p derivation_TeX_out receives an owning aligned TeX rendering containing the inferred
 * sigma formula followed by the simplified result. Otherwise it receives @c NULL. Release it with string_free().
 */
expr_t *expr_from_string_with_derivation_TeX(const char *s, expr_bindings_t **bnd_out,
                                             string_t **derivation_TeX_out);

/**
 * @brief Construct a expr_t from text stored in a string.
 *
 * This accepts the same grammar as expr_from_string(), but takes a @c string_t
 * so callers can enter the expression parser without exposing the parser to
 * raw C-string ownership or lifetime details.
 *
 * Returns an owning handle on success, or NULL on error (details written to
 * stderr). The caller must call expr_free() on the returned pointer exactly
 * once.
 */
expr_t *expr_from_text(const string_t *text, expr_bindings_t **bnd_out);

/**
 * @brief Serialise an expression into a SQLite-ready payload.
 *
 * The payload uses the round-trippable @c style_EXPRESSION text format. On
 * success, the caller owns @p out_type, @p out_encoding, and @p out_data and
 * must release them with @c string_free() and @c free().
 *
 * @param expr Expression to serialise.
 * @param out_type Receives a newly allocated type label.
 * @param out_encoding Receives a newly allocated encoding label.
 * @param out_data Receives a newly allocated payload buffer.
 * @param out_len Receives the payload length in bytes.
 * @return @c true on success, otherwise @c false.
 */
bool expr_serialize(const expr_t *expr, string_t **out_type, string_t **out_encoding, void **out_data, size_t *out_len);

/**
 * @brief Reconstruct an expression from a serialised payload.
 *
 * Parsed bindings, if any, are created internally and owned by the returned
 * expression in the usual way.
 *
 * @param data Serialised payload bytes.
 * @param len Payload length in bytes.
 * @param type Stored type label.
 * @param encoding Stored encoding label.
 * @return Newly allocated expression on success, otherwise @c NULL.
 */
expr_t *expr_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding);

/**
 * @brief Look up a parsed binding by name.
 *
 * The lookup accepts normalised binding names. Bracketed names may be queried
 * either as @p [radius] or @p radius. Greek-style aliases are normalised too,
 * so a parsed binding may be queried using its plain ASCII, at-prefixed ASCII,
 * or Unicode spelling, for example @p theta, @p @theta, or @p θ.
 *
 * Returns the borrowed `expr_t *` leaf for that binding, or NULL if no such
 * binding exists.
 */
expr_t *expr_bindings_get(expr_bindings_t *bnd, const char *name);

/**
 * @brief Look up a parsed binding by string name.
 *
 * This is the string_t-based counterpart to expr_bindings_get(). The lookup
 * name is borrowed and may be queried with the same aliases accepted by the
 * raw-string convenience wrapper.
 */
expr_t *expr_bindings_get_text(expr_bindings_t *bnd, const string_t *name);

/**
 * @brief Borrow the number of entries in @p bnd.
 */
size_t expr_bindings_count(const expr_bindings_t *bnd);

/**
 * @brief Borrow binding metadata by index.
 */
const char *expr_bindings_name_at(const expr_bindings_t *bnd, size_t index);
const string_t *expr_bindings_name_text_at(const expr_bindings_t *bnd, size_t index);
expr_t *expr_bindings_expr_at(expr_bindings_t *bnd, size_t index);
bool expr_bindings_is_constant_at(const expr_bindings_t *bnd, size_t index);

/**
 * @brief Report whether parsing applied explicit derivative syntax.
 *
 * This distinguishes an input such as @c Dxx(f(x)) from the ordinary
 * expression tree produced after that derivative has been evaluated.
 */
bool expr_bindings_has_symbolic_derivative(const expr_bindings_t *bnd);

/**
 * @brief Report whether parsing applied symbolic integral syntax.
 *
 * This records an @c @S operation even when expr_from_string() completed it
 * into an antiderivative expression containing no integral node.
 */
bool expr_bindings_has_symbolic_integral(const expr_bindings_t *bnd);

/**
 * @brief Apply one binding value edit and return a canonical expression.
 *
 * Binding lookup, value parsing, constant removal, and expression
 * simplification are all performed by the expression library. An empty value
 * removes a constant binding from the expression and leaves a variable
 * unresolved. On success, @p bindings_out receives bindings for the returned
 * expression.
 */
expr_t *expr_edit_binding(const expr_t *expr, const expr_bindings_t *bindings, const char *name, const char *value_text,
                          expr_bindings_t **bindings_out);

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
expr_t *expr_from_expression_string(const char *expr, const char *const *names, expr_t *const *symbols,
                                    size_t nsymbols);

/**
 * @brief Construct an expression from bare expression text using supplied symbols.
 *
 * This is the string_t-based counterpart to expr_from_expression_string(). The
 * input text is borrowed and is not modified.
 */
expr_t *expr_from_expression_text(const string_t *expr, const string_t *const *names, expr_t *const *symbols,
                                  size_t nsymbols);

#endif /* EXPRESSION_H */
