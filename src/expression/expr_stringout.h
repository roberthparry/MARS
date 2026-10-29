/**
 * @file expr_stringout.h
 * @brief Shared private expression formatting services.
 *
 * Defines text-buffer and Cartesian composition helpers and declarations used across output wrappers and emitters.
 * It connects native, function and TeX presentation while keeping formatting machinery outside the public
 * expression API.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef EXPR_STRINGOUT_H
#define EXPR_STRINGOUT_H

#if !defined(MARS_EXPR_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_EXPR_INTERNAL_ACCESS."
#endif

#include <stddef.h>

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

/* One Cartesian presentation shared by all output styles for a composed Li or Ei expression. */
typedef struct {
    const expr_t *source;
    expr_t *real_name;
    expr_t *imaginary_name;
    expr_t *real_definition;
    expr_t *imaginary_definition;
    expr_t *compact;
    expr_t *expanded;
} expr_cartesian_composition_t;

bool expr_cartesian_composition_init(const expr_t *expr, expr_cartesian_composition_t *view);
void expr_cartesian_composition_clear(expr_cartesian_composition_t *view);

/* Allocation helpers. */
void *expr_tostring_xmalloc(size_t n);
char *expr_tostring_xstrdup(const char *s);

/* Buffer lifecycle and mutation. */
void sbuf_init(sbuf_t *b);
void sbuf_free(sbuf_t *b);
void sbuf_reserve(sbuf_t *b, size_t extra);
void sbuf_putc(sbuf_t *b, char c);
void sbuf_puts(sbuf_t *b, const char *s);
void sbuf_put_string(sbuf_t *b, const string_t *s);

/* Buffer export. */
/**
 * @brief Borrow the buffer's NUL-terminated text until the buffer changes or is released.
 * @param buffer Caller-owned formatting buffer, initialised before use and released with sbuf_free.
 * @return Borrowed text as described above, or NULL when unavailable; do not modify or free it.
 */
const char *sbuf_c_str(const sbuf_t *buffer);
size_t sbuf_len(const sbuf_t *b);
string_t *sbuf_to_string(const sbuf_t *b);
char *sbuf_to_c_string(const sbuf_t *b);
char *sbuf_take_c_string(sbuf_t *b);

/* Expression-name and formatting helpers. */
int expr_tostring_is_negative_const(const expr_t *f);
int expr_tostring_is_var_pow_d(const expr_t *f);
int expr_tostring_is_unicode_letter(unsigned int c);
int expr_tostring_is_simple_name(const char *name);

/**
 * @brief Check whether a displayed name is exactly one letter, including Greek symbols.
 * @param[in] name Borrowed UTF-8 name; NULL and empty names are rejected.
 * @return True for one unadorned letter, false for compound or bracketed names.
 */
bool expr_tostring_is_single_symbol(const char *name);

int expr_tostring_is_safe_func_name(const char *name);

/* Emission helpers. */
void emit_name(sbuf_t *b, const char *name);
void emit_name_func(sbuf_t *b, const char *name);
void emit_function_builtin_name(sbuf_t *b, const expr_ops_t *ops);

/**
 * @brief Render independently allocated TeX body and binding strings; free both outputs; zero denotes success.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param expr_out Required caller-owned output receiving an owned C string; release it with free.
 * @param bindings_out Required caller-owned output receiving an owned C string; release it with free.
 * @return Zero on success; a non-zero error status on failure.
 */
int expr_to_TeX_parts(const expr_t *expr, char **expr_out, char **bindings_out);

/**
 * @brief Render owning TeX body and bindings separately, retaining a bound root variable symbolically.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param expr_out Required caller-owned output receiving an owned C string; release it with free.
 * @param bindings_out Required caller-owned output receiving an owned C string; release it with free.
 * @return Zero on success; a non-zero error status on failure.
 */
int expr_to_TeX_symbolic_parts(const expr_t *expr, char **expr_out, char **bindings_out);

#endif /* EXPR_STRINGOUT_H */
