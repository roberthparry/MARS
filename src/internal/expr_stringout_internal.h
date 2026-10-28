/**
 * @file expr_stringout_internal.h
 * @brief Controlled expression-formatting services shared with equation.
 *
 * Exposes the buffer, temporary naming and symbol discovery needed to render
 * equation function text. Expression retains its private graph and rendering
 * implementation. Ordinary callers use the public expression and equation APIs.
 */
#ifndef MARS_SHARED_EXPR_STRINGOUT_INTERNAL_H
#define MARS_SHARED_EXPR_STRINGOUT_INTERNAL_H

#if !defined(MARS_SHARED_EXPR_STRINGOUT_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_EXPR_STRINGOUT_INTERNAL_ACCESS."
#endif

#include <stddef.h>

#include "expression-t.h"
#include "string-t.h"

/** @brief Mutable expression-formatting buffer, initialised and released by sbuf_init and sbuf_free. */
typedef struct {
    string_t *text; /**< Buffer-owned text. */
} sbuf_t;

/** @brief Private entry owned by an automatic-name table. */
typedef struct _expr_autoname_entry_t autoname_entry_t;

/** @brief Temporary expression names restored by autoname_restore. */
typedef struct {
    autoname_entry_t *entries; /**< Expression-owned records. */
    size_t count;              /**< Number of records. */
    size_t cap;                /**< Allocated record capacity. */
} autoname_table_t;

/** @brief Borrowed expression symbols in an owned array. */
typedef struct {
    expr_t **vars; /**< Owned array of borrowed expression pointers; release the array with free. */
    size_t count;  /**< Number of symbols. */
    size_t cap;    /**< Allocated array capacity. */
} varlist_t;

/** @brief Initialise an empty output buffer. */
void sbuf_init(sbuf_t *buffer);

/** @brief Release a buffer and its text. */
void sbuf_free(sbuf_t *buffer);

/** @brief Append one character to a buffer. */
void sbuf_putc(sbuf_t *buffer, char value);

/** @brief Append a NUL-terminated string to a buffer. */
void sbuf_puts(sbuf_t *buffer, const char *text);

/** @brief Borrow the buffer's NUL-terminated text until the buffer changes or is released. */
const char *sbuf_c_str(const sbuf_t *buffer);

/** @brief Copy a buffer's text into an independently owned string. */
string_t *sbuf_to_string(const sbuf_t *buffer);

/** @brief Write a symbol name using native function syntax. */
void emit_name_func(sbuf_t *buffer, const char *name);

/** @brief Render a function-style expression body into an independently allocated C string. */
char *expr_function_display_body(const expr_t *expression);

/** @brief Set thread-local scientific formatting and return its previous state. */
bool expr_set_number_scientific_local(bool scientific);

/** @brief Set thread-local numeric precision and return its previous value. */
int expr_set_number_precision_local(int precision);

/** @brief Initialise a temporary naming table. */
void autoname_init(autoname_table_t *table);

/** @brief Restore temporarily assigned names and release table storage. */
void autoname_restore(autoname_table_t *table);

/** @brief Assign temporary names to unnamed variables in a borrowed expression. */
void assign_unnamed_vars_dfs(expr_t *expression, autoname_table_t *table);

/** @brief Initialise an empty borrowed-symbol list. */
void varlist_init(varlist_t *list);

/** @brief Add variables found in the borrowed expression to the list. */
void find_vars_dfs(const expr_t *expression, varlist_t *list);

/** @brief Add named constants found in the borrowed expression to the list. */
void find_named_consts_dfs(const expr_t *expression, varlist_t *list);

/** @brief Add explicitly named constants found in the borrowed expression to the list. */
void find_explicit_named_consts_dfs(const expr_t *expression, varlist_t *list);

/** @brief Return the borrowed expression name, or the borrowed fallback if unnamed. */
const char *expr_name_or_default(const expr_t *expression, const char *fallback);

#endif
