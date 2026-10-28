/**
 * @file TeX_expr_private.h
 * @brief Helpers shared only by the legacy TeX expression renderer.
 *
 * These declarations are private to TeX's remaining expression renderer and
 * are not part of the controlled interface used by mathematical modules.
 */
#ifndef MARS_TeX_EXPR_PRIVATE_H
#define MARS_TeX_EXPR_PRIVATE_H
#if !defined(MARS_TeX_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_TeX_INTERNAL_ACCESS."
#endif

#define MARS_SHARED_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "internal/expr_stringout_internal.h"

/** @brief Emit a mathematical symbol name in TeX notation. */
void TeX_emit_name(sbuf_t *buffer, const char *name);

/** @brief Legacy expression precedence levels accepted by TeX_emit_expr. */
typedef enum {
    TeX_PRECEDENCE_LOWEST = 0,
    TeX_PRECEDENCE_ADDITION = 1,
    TeX_PRECEDENCE_MULTIPLICATION = 2
} TeX_precedence_t;

/** @brief Emit a borrowed expression using the legacy renderer at the given enclosing precedence. */
void TeX_emit_expr(const expr_t *expression, sbuf_t *buffer, int parent_precedence);

/** @brief Emit the legacy product separator for two borrowed expression factors. */
void TeX_emit_mul_separator(const expr_t *left, const expr_t *right, sbuf_t *buffer);

/** @brief Emit a formal derivative using the legacy expression renderer. */
void TeX_emit_formal_derivative(const expr_t *expression, sbuf_t *buffer);

/** @brief Emit an arbitrary function using the legacy expression renderer. */
void TeX_emit_arbitrary_function(const expr_t *expression, sbuf_t *buffer);

/** @brief Emit the argument list of a legacy expression. */
void TeX_emit_argument_list(const expr_t *expression, sbuf_t *buffer);

/** @brief Emit an integral using the legacy expression renderer. */
void TeX_emit_integral(const expr_t *expression, sbuf_t *buffer, int parent_precedence);

#endif
