#ifndef EXPR_STRINGOUT_INTERNAL_H
#define EXPR_STRINGOUT_INTERNAL_H

#if !defined(MARS_EXPR_STRINGOUT_INTERNAL_ACCESS) &&                                                                   \
    (!defined(__INTELLISENSE__) || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "expr_stringout_internal.h is private to expression formatting; include expression.h instead."
#endif

#include <stdbool.h>
#include <stddef.h>

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

typedef enum { PREC_LOWEST = 0, PREC_ADD = 1, PREC_MUL = 2, PREC_POW = 3, PREC_UNARY = 4, PREC_ATOM = 5 } prec_t;

typedef struct {
    expr_t *node;
    char *buf;
} autoname_entry_t;

typedef struct {
    autoname_entry_t *entries;
    size_t count;
    size_t cap;
} autoname_table_t;

typedef struct {
    expr_t **vars;
    size_t count;
    size_t cap;
} varlist_t;

/* Local value formatting. */
char *expr_number_to_string_local(number_t value);
char *expr_const_to_string_local(const expr_t *dv);
char *expr_eval_to_string_local(const expr_t *dv);
char *expr_text_to_TeX_local(const char *text);
bool expr_is_immortal_default_const_local(const expr_t *dv);
bool expr_set_number_scientific_local(bool scientific);
int expr_set_number_precision_local(int precision);

/* Variable and constant discovery. */
void autoname_init(autoname_table_t *t);
void autoname_restore(autoname_table_t *t);
void assign_unnamed_vars_dfs(expr_t *f, autoname_table_t *t);
void varlist_init(varlist_t *vl);
void find_vars_dfs(const expr_t *f, varlist_t *vl);
void find_named_consts_dfs(const expr_t *f, varlist_t *cl);
void find_explicit_named_consts_dfs(const expr_t *f, varlist_t *cl);
const char *expr_name_or_default(const expr_t *dv, const char *fallback);

/* Binding RHS formatting. */
char *binding_rhs_expr_string_local(const expr_t *dv);
char *binding_rhs_TeX_string_local(const expr_t *dv);
char *binding_rhs_c_string_local(const expr_t *dv);

/* Expression emitters. */
void emit_expr(const expr_t *f, sbuf_t *b, int parent_prec);
void emit_TeX_expr(const expr_t *f, sbuf_t *b, int parent_prec);
void emit_func_fragment(sbuf_t *b, const char *text);
void emit_func(const expr_t *f, sbuf_t *b, int parent_prec);
void emit_func_display(const expr_t *f, sbuf_t *b, int parent_prec);
void emit_func_with_temporaries(const expr_t *f, sbuf_t *b, int parent_prec, const expr_t *const *nodes,
                                const char *const *names, size_t count, const expr_t *expanded_node);
void emit_TeX_name(sbuf_t *b, const char *name);
void expr_TeX_partial_derivatives_push(void);
void expr_TeX_partial_derivatives_pop(void);
bool expr_TeX_partial_derivatives_enabled(void);
void expr_TeX_total_derivatives_push(void);
void expr_TeX_total_derivatives_pop(void);
bool expr_TeX_total_derivatives_enabled(void);

/* Top-level text builders. */
string_t *expr_to_text_expr(const expr_t *f);
string_t *expr_to_text_unbound(const expr_t *f);
string_t *expr_to_text_function(const expr_t *f);

#endif /* EXPR_STRINGOUT_INTERNAL_H */
