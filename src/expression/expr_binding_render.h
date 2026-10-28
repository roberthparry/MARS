/**
 * @file expr_binding_render.h
 * @brief Expression-owned preparation helpers for exact binding-value output.
 *
 * Shares precedence and literal-number classification between native binding
 * output and semantic notation preparation. It contains no typesetting commands.
 * Public callers request complete expression output through expression-t.h.
 */
#ifndef MARS_EXPR_BINDING_RENDER_H
#define MARS_EXPR_BINDING_RENDER_H
#if !defined(MARS_EXPR_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_EXPR_INTERNAL_ACCESS."
#endif

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_bindings.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_stringout.h"

/** @brief Binding grammar precedence used by native text output. */
typedef enum {
    BIND_PREC_LOWEST = 0,
    BIND_PREC_ADD = 1,
    BIND_PREC_MUL = 2,
    BIND_PREC_POW = 3,
    BIND_PREC_UNARY = 4,
    BIND_PREC_ATOM = 5
} binding_prec_t;

/** @brief Owned numerator and denominator spellings of an exact literal fraction. */
typedef struct {
    bool negative;
    string_t *numer;
    string_t *denom;
} binding_simple_rational_t;

/** @brief Whether native power notation must visibly group the base. */
bool binding_expr_needs_pow_base_parens(const expr_binding_expr_t *expr);

/** @brief Release both literal strings and reset a parsed fraction. */
void binding_simple_rational_clear(binding_simple_rational_t *rational);

/** @brief Parse an exact literal fraction into caller-owned component strings. */
bool binding_text_is_simple_rational(const char *text, binding_simple_rational_t *out);

/** @brief Append the decimal digits of a borrowed string to a native output buffer. */
void emit_binding_digits(sbuf_t *buffer, const string_t *digits);

/** @brief Return an owned decimal spelling with redundant trailing precision removed. */
string_t *binding_decimal_display_text(const char *text);

/** @brief Whether native factor ordering should put a power before a radical. */
bool binding_mul_should_swap_power_before_radical(const expr_binding_expr_t *left, const expr_binding_expr_t *right);

#endif
