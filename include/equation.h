/**
 * @file equation.h
 * @brief Algebraic equations, symbolic isolation and numerical root solving.
 *
 * The opaque equation_t type represents two expressions joined by equality, with
 * optional symbol bindings. The API supports constructing and parsing equations,
 * inspecting their sides, formatting them and obtaining solution collections.
 *
 * Use this module to solve algebraic constraints rather than merely evaluate an
 * expression. Supported symbolic and polynomial paths retain exact values and
 * surds where possible; numerical solving is available where an exact solution is
 * not obtained. General equations are not guaranteed to have closed-form solutions.
 *
 * Use expression.h for the underlying symbolic expressions and diffequation.h for
 * differential equations. Respect the documented ownership of equation handles,
 * solution collections and borrowed entries within those collections.
 */

#ifndef EQUATION_H
#define EQUATION_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include "expression.h"

typedef struct equation_t equation_t;
typedef struct equation_solutions equation_solutions_t;

/**
 * @brief Create an equation from a left- and right-hand expression.
 *
 * The expressions are retained, not consumed. The caller owns the returned
 * equation and must release it with equ_free().
 *
 * @param[in] lhs Borrowed left-hand expression; must not be NULL. The equation retains its own reference.
 * @param[in] rhs Borrowed right-hand expression; must not be NULL. The equation retains its own reference.
 * @return Caller-owned equation, or NULL on invalid input or failure; release with equ_free.
 * @note This constructor does not create a binding table; use equ_new_with_inferred_bindings when one is needed.
 */
equation_t *equ_new(const expr_t *lhs, const expr_t *rhs);

/**
 * @brief Create a solvable equation, inferring named bindings from both sides.
 *
 * Retains the expressions and their existing variable and constant nodes without
 * reparsing or changing their values. Occurrences of the same name must already
 * refer to the same node. The caller owns the result and releases it with equ_free().
 * Unlike equ_new(), this constructor supplies the binding table used by
 * equ_derive_solutions(). Returns NULL on invalid input or allocation failure.
 *
 * @param[in] lhs Borrowed left-hand expression; must not be NULL. The equation retains its own reference.
 * @param[in] rhs Borrowed right-hand expression; must not be NULL. The equation retains its own reference.
 * @return Caller-owned equation, or NULL on invalid input or failure; release with equ_free.
 */
equation_t *equ_new_with_inferred_bindings(const expr_t *lhs, const expr_t *rhs);

/**
 * @brief Release an owning equation handle.
 *
 * @param[in] equation Owned equation handle to destroy; NULL is safe. Borrowed results become invalid.
 */
void equ_free(equation_t *equation);

/**
 * @brief Borrow the left-hand side expression from an equation.
 *
 * @param[in] equation Borrowed equation to inspect or operate on; ownership is unchanged.
 * @return Borrowed expression, valid while retained by the equation; NULL for a NULL equation. Do not free it.
 */
const expr_t *equ_lhs(const equation_t *equation);

/**
 * @brief Borrow the right-hand side expression from an equation.
 *
 * @param[in] equation Borrowed equation to inspect or operate on; ownership is unchanged.
 * @return Borrowed expression, valid while retained by the equation; NULL for a NULL equation. Do not free it.
 */
const expr_t *equ_rhs(const equation_t *equation);

/**
 * @brief Borrow the binding set owned by @p equation.
 *
 * The returned bindings remain owned by @p equation and must not be freed.
 *
 * @param[in] equation Borrowed equation to inspect or operate on; ownership is unchanged.
 * @return Borrowed binding set, or NULL for no bindings or a NULL equation; do not free it.
 * @note The binding set remains valid only while owned by the equation.
 */
expr_bindings_t *equ_bindings(const equation_t *equation);

/**
 * @brief Borrow the named binding node owned by @p equation.
 *
 * @param[in] equation Borrowed equation to inspect or operate on; ownership is unchanged.
 * @param[in] name Required NUL-terminated binding name to look up; borrowed and not modified.
 * @return Borrowed node, or NULL for a missing name, absent bindings or invalid input; do not free it.
 * @note The returned node remains owned by the equation's bindings.
 */
expr_t *equ_binding(const equation_t *equation, const char *name);

/**
 * @brief Build the owning residual expression @c lhs - rhs, simplified.
 *
 * @param[in] equation Borrowed equation to inspect or operate on; ownership is unchanged.
 * @return Caller-owned simplified residual, or NULL on invalid input or failure; release with expr_free.
 */
expr_t *equ_residual(const equation_t *equation);

/**
 * @brief Build an owning display equation with polynomial sides expanded in
 *        descending powers of @p wrt.
 *
 * Non-polynomial sides fall back to the general display expansion. Inputs are
 * borrowed; release the returned equation with equ_free().
 *
 * @param[in] equation Borrowed equation to inspect or operate on; ownership is unchanged.
 * @param[in] wrt Borrowed polynomial variable; NULL selects general display expansion.
 * @return Caller-owned equation, or NULL on invalid input or failure; release with equ_free.
 */
equation_t *equ_display_expanded(const equation_t *equation, const expr_t *wrt);

/**
 * @brief Return true when the equation is already isolated as @c wrt = f(...)
 *        and the right-hand side does not contain @p wrt.
 *
 * @param[in] equation Borrowed equation to inspect or operate on; ownership is unchanged.
 * @param[in] wrt Borrowed expression node identifying the variable of interest; not consumed.
 * @return True if the left side is wrt and the right side is independent of it; false otherwise or for invalid input.
 */
bool equ_is_solved_for(const equation_t *equation, const expr_t *wrt);

/**
 * @brief Release an owning solution set returned by equ_derive_solutions().
 *
 * @param[in] solutions Owned solution set to destroy, including its equations; NULL is safe.
 */
void equ_solutions_free(equation_solutions_t *solutions);

/**
 * @brief Derive the best solutions available for @p equation.
 *
 * The equation's owned bindings determine which symbols are solved as
 * variables and which are treated as constants. Symbolic isolation is tried
 * first for each variable binding; when that produces no solutions, the solver
 * falls back to numeric goal-seeking across the equation's variable bindings.
 * Any current values already stored on those variable bindings are used as the
 * numeric starting point.
 * An unseeded real affine zeta equation uses a bounded multi-start search;
 * consult equ_solutions_search_note() for its bounds and limitations, and
 * equ_solutions_family_note() for any separate exact family.
 *
 * The caller owns the returned solution set and must release it with
 * equ_solutions_free().
 *
 * @param[in] equation Borrowed equation to inspect or operate on; ownership is unchanged.
 * @return Caller-owned solution set, possibly empty, or NULL on failure; release with equ_solutions_free.
 * @note NULL input produces an empty set if allocation succeeds. An empty set alone does not prove
 * that no roots exist; check equ_solutions_proven_empty.
 */
equation_solutions_t *equ_derive_solutions(const equation_t *equation);

/**
 * @brief Return the number of solutions currently stored in @p solutions.
 *
 * @param[in] solutions Borrowed solution set to inspect; ownership is unchanged.
 * @return Number of stored solutions, or zero for a NULL set.
 */
size_t equ_solutions_count(const equation_solutions_t *solutions);

/**
 * @brief Borrow the solution at @p index, or NULL when out of range.
 *
 * @param[in] solutions Borrowed solution set to inspect; ownership is unchanged.
 * @param[in] index Zero-based solution index; must be less than equ_solutions_count for a non-NULL result.
 * @return Borrowed equation valid until its solution set is freed; NULL for a NULL set or out-of-range index.
 */
const equation_t *equ_solutions_at(const equation_solutions_t *solutions, size_t index);

/**
 * @brief Borrow a description of the numerical search bounds, or NULL when not applicable.
 *
 * A bounded search does not certify that all roots within its bounds have been found.
 *
 * @param[in] solutions Borrowed solution set to inspect; ownership is unchanged.
 * @return Borrowed explanatory text, or NULL for no applicable note or a NULL set; do not free it.
 */
const char *equ_solutions_search_note(const equation_solutions_t *solutions);

/**
 * @brief Borrow a description of a separate exact solution family, or NULL when absent.
 *
 * @param[in] solutions Borrowed solution set to inspect; ownership is unchanged.
 * @return Borrowed family description, or NULL for no applicable family or a NULL set; do not free it.
 */
const char *equ_solutions_family_note(const equation_solutions_t *solutions);

/**
 * @brief Borrow a parsed equation's series-domain note, or NULL when not applicable.
 *
 * @param[in] equation Borrowed equation to inspect or operate on; ownership is unchanged.
 * @return Borrowed domain explanation, or NULL when absent or given a NULL equation; do not free it.
 */
const char *equ_interpretation_note(const equation_t *equation);

/**
 * @brief Return true only when the equation has been proved to have no solutions in its domain.
 *
 * @param[in] solutions Borrowed solution set to inspect; ownership is unchanged.
 * @return True only for a proved empty set; false for unknown, non-empty or NULL results.
 */
bool equ_solutions_proven_empty(const equation_solutions_t *solutions);

/**
 * @brief Serialise @p equation to newly allocated text.
 *
 * The expression style produces a parseable equation wrapper:
 *
 *   { lhs = rhs }
 *
 * Function style produces an equation-valued callable that preserves both
 * sides of the relation, followed by a compact solve-and-output call:
 *
 *   equation equ(x) { return equation(lhs = rhs). }
 *   output(solve(equ(x))).
 *
 * Unbound and LaTeX styles may use faithful compact solver notation with
 * definitions. Expression style retains the underlying algebra; use that style
 * with equ_from_text() or equ_from_string() to parse an equation back.
 *
 * @param[in] equation Borrowed equation to inspect or operate on; ownership is unchanged.
 * @param[in] style Output style: style_EXPRESSION, style_UNBOUND, style_LATEX or style_FUNCTION.
 * @return Caller-owned string, or NULL on formatting/allocation failure; release with string_free.
 * @note NULL input produces an owned string containing "NULL", unless allocation fails.
 */
string_t *equ_to_text(const equation_t *equation, style_t style);

/**
 * @brief Render an equation as an owning aligned TeX body.
 *
 * Native display metadata, including formal finite sums preserved while the
 * solver expands their values, is honoured. The caller must release the
 * returned C string with free().
 *
 * @param[in] equation Borrowed equation to inspect or operate on; ownership is unchanged.
 * @param[in] line_limit Desired TeX source-line length in bytes; zero selects the default wrapping limit. Not a hard
 * output bound.
 * @return Caller-owned NUL-terminated TeX body, or NULL for invalid input or failure; release with free.
 */
char *equ_to_TeX_body_wrapped(const equation_t *equation, size_t line_limit);

/**
 * @brief Format equation-aware text into a new string_t from a va_list.
 *
 * Supports ordinary string formatting plus equation conversions:
 * `%n` expression style, `%nu` unbound style, `%nt` TeX style, and `%nf`
 * function style.
 *
 * `%N` selects scientific numeric formatting for embedded number_t values
 * while keeping the same equation style selection rules.
 *
 * @param[in] fmt Borrowed NUL-terminated format string; equation conversions consume equation_t pointers.
 * @param[in] ap Initialised argument list matching fmt; copied internally. The caller remains responsible for va_end.
 * @return Caller-owned string, or NULL on formatting/allocation failure; release with string_free.
 */
string_t *equ_vsprintf_text(const char *fmt, va_list ap);

/**
 * @brief Format equation-aware text into a new string_t.
 *
 * The caller owns the returned string and must release it with string_free().
 *
 * @param[in] fmt Borrowed NUL-terminated format string; equation conversions consume equation_t pointers.
 * @param[in] ... Additional arguments matching fmt; equation arguments are borrowed, not consumed.
 * @return Caller-owned string, or NULL on formatting/allocation failure; release with string_free.
 */
string_t *equ_sprintf_text(const char *fmt, ...);

/**
 * @brief Format equation-aware text into a caller-provided buffer.
 *
 * @param[out] out Optional caller-owned output buffer; NULL requests formatting without copying.
 * @param[out] out_size Buffer capacity in bytes, including the terminating NUL; zero disables copying.
 * @param[in] fmt Borrowed NUL-terminated format string; equation conversions consume equation_t pointers.
 * @param[in] ... Additional arguments matching fmt; equation arguments are borrowed, not consumed.
 * @return Full formatted string_length value before truncation, or -1 on failure or an unrepresentable int result.
 * @note When out is non-NULL and out_size is positive, copying is limited to out_size - 1 and NUL-terminated.
 * The current implementation also limits copied bytes using string_length (a character count).
 * For multibyte UTF-8 output use equ_sprintf_text instead; this buffer helper may truncate a code point.
 */
int equ_sprintf(char *out, size_t out_size, const char *fmt, ...);

/**
 * @brief Print equation-aware formatted text to stdout.
 *
 * Supports `%n` expression style, `%nu` unbound style, and `%nt` TeX style.
 *
 * `%N` selects scientific numeric formatting for embedded number_t values
 * while keeping the same equation style selection rules.
 *
 * @param[in] fmt Borrowed NUL-terminated format string; equation conversions consume equation_t pointers.
 * @param[in] ... Additional arguments matching fmt; equation arguments are borrowed, not consumed.
 * @return Result of string_printf when writing the formatted text, or -1 if formatting fails.
 */
int equ_printf(const char *fmt, ...);

/**
 * @brief Print the expression-style string representation of @p equation.
 *
 * @param[in] equation Borrowed equation to inspect or operate on; ownership is unchanged.
 * @note Prints a trailing newline. A formatting failure prints "NULL" instead.
 */
void equ_print(const equation_t *equation);

/**
 * @brief Construct an equation from equation-style text.
 *
 * Accepted forms are:
 *
 *   lhs = rhs
 *   { lhs = rhs }
 *   { lhs = rhs | x = val, ...; [name] = val, ... }
 *
 * A top-level additive arithmetic or quadratic sequence may abbreviate its
 * omitted terms with @c ... or @c … when three preceding like terms establish
 * its finite differences and one following like term supplies the endpoint.
 *
 * The left and right sides share one symbol table, so a variable named on both
 * sides resolves to the same expr_t node. Parsed bindings are owned by the
 * returned equation and can be borrowed with equ_bindings() or
 * equ_binding().
 *
 * @param[in] s Borrowed NUL-terminated equation text in an accepted input form.
 * @return Caller-owned equation, or NULL on invalid input or failure; release with equ_free.
 */
equation_t *equ_from_string(const char *s);

/**
 * @brief Construct an equation from text stored in a string.
 *
 * @param[in] text Borrowed string containing equation text; not consumed or modified.
 * @return Caller-owned equation, or NULL on invalid input or failure; release with equ_free.
 */
equation_t *equ_from_text(const string_t *text);

/**
 * @brief Serialise an equation into a SQLite-ready payload.
 *
 * The payload uses the round-trippable expression-style equation text format.
 * On success, the caller owns @p out_type, @p out_encoding, and @p out_data
 * and must release them with @c string_free() and @c free().
 *
 *
 * @param[in] equation Borrowed equation to inspect or operate on; ownership is unchanged.
 * @param[out] out_type Required output pointer; receives an owned type label on success. Release with string_free.
 * @param[out] out_encoding Required output pointer; receives an owned encoding label on success. Release with
 * string_free.
 * @param[out] out_data Required output pointer; receives an owned payload buffer on success. Release with free.
 * @param[out] out_len Required output pointer; receives the payload byte count, excluding any terminating NUL.
 * @return True on success; false on invalid arguments or failure. Output variables are assigned only on success.
 * @note The payload is counted bytes, not a NUL-terminated C string.
 */
bool equ_serialize(const equation_t *equation, string_t **out_type, string_t **out_encoding, void **out_data,
                   size_t *out_len);

/**
 * @brief Reconstruct an equation from a serialised payload.
 *
 *
 * @param[in] data Required borrowed payload buffer containing at least len bytes; need not be NUL-terminated.
 * @param[in] len Payload length in bytes.
 * @param[in] type Required borrowed type label; must contain "equation_t".
 * @param[in] encoding Required borrowed encoding label; must contain "mars/equation".
 * @return Caller-owned equation, or NULL on invalid input or failure; release with equ_free.
 */
equation_t *equ_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding);

#endif
