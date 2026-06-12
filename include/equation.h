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
 */
equation_t *equ_new(const expr_t *lhs, const expr_t *rhs);

/**
 * @brief Release an owning equation handle.
 */
void equ_free(equation_t *equation);

/**
 * @brief Borrow the left-hand side expression from an equation.
 */
const expr_t *equ_lhs(const equation_t *equation);

/**
 * @brief Borrow the right-hand side expression from an equation.
 */
const expr_t *equ_rhs(const equation_t *equation);

/**
 * @brief Borrow the binding set owned by @p equation.
 *
 * The returned bindings remain owned by @p equation and must not be freed.
 */
expr_bindings_t *equ_bindings(const equation_t *equation);

/**
 * @brief Borrow the named binding node owned by @p equation.
 */
expr_t *equ_binding(const equation_t *equation, const char *name);

/**
 * @brief Build the owning residual expression @c lhs - rhs, simplified.
 */
expr_t *equ_residual(const equation_t *equation);

/**
 * @brief Return true when the equation is already isolated as @c wrt = f(...)
 *        and the right-hand side does not contain @p wrt.
 */
bool equ_is_solved_for(const equation_t *equation, const expr_t *wrt);

/**
 * @brief Release an owning solution set returned by equ_derive_solutions().
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
 *
 * The caller owns the returned solution set and must release it with
 * equ_solutions_free().
 */
equation_solutions_t *equ_derive_solutions(const equation_t *equation);

/**
 * @brief Borrow the number of solutions currently stored in @p result.
 */
size_t equ_solutions_count(const equation_solutions_t *solutions);

/**
 * @brief Borrow the solution at @p index, or NULL when out of range.
 */
const equation_t *equ_solutions_at(
    const equation_solutions_t *solutions,
    size_t index);

/**
 * @brief Serialise @p equation to newly allocated text.
 *
 * The expression style produces a parseable equation wrapper:
 *
 *   { lhs = rhs }
 *
 * Use equ_from_text() or equ_from_string() to parse it back.
 */
string_t *equ_to_text(const equation_t *equation, style_t style);

/**
 * @brief Format equation-aware text into a new string_t from a va_list.
 *
 * Supports ordinary string formatting plus equation conversions:
 * `%n` expression style, `%nu` unbound style, and `%nt` TeX style.
 *
 * `%N` selects scientific numeric formatting for embedded number_t values
 * while keeping the same equation style selection rules.
 */
string_t *equ_vsprintf_text(const char *fmt, va_list ap);

/**
 * @brief Format equation-aware text into a new string_t.
 *
 * The caller owns the returned string and must release it with string_free().
 */
string_t *equ_sprintf_text(const char *fmt, ...);

/**
 * @brief Format equation-aware text into a caller-provided buffer.
 */
int equ_sprintf(char *out, size_t out_size, const char *fmt, ...);

/**
 * @brief Print equation-aware formatted text to stdout.
 *
 * Supports `%n` expression style, `%nu` unbound style, and `%nt` TeX style.
 *
 * `%N` selects scientific numeric formatting for embedded number_t values
 * while keeping the same equation style selection rules.
 */
int equ_printf(const char *fmt, ...);

/**
 * @brief Print the expression-style string representation of @p equation.
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
 * The left and right sides share one symbol table, so a variable named on both
 * sides resolves to the same expr_t node. Parsed bindings are owned by the
 * returned equation and can be borrowed with equ_bindings() or
 * equ_binding().
 */
equation_t *equ_from_string(const char *s);

/**
 * @brief Construct an equation from text stored in a string.
 */
equation_t *equ_from_text(const string_t *text);

#endif
