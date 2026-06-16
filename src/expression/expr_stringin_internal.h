#ifndef EXPR_STRINGIN_INTERNAL_H
#define EXPR_STRINGIN_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "dictionary.h"
#include "expression.h"
#include "expr_internal.h"

typedef struct {
    string_t *name;
    expr_t   *node;
} sym_t;

typedef struct {
    sym_t *entries;
    int    count;
    int    cap;
} symtab_t;

void * fs_xmalloc  (size_t n);
int    fs_is_letter(unsigned int c);

/* Symbol table lifecycle and lookup. */
void     symtab_init             (symtab_t *t);
int      symtab_has_text         (const symtab_t *t, const string_t *name);
void     symtab_add_text         (symtab_t *t, const string_t *name, expr_t *node);
expr_t * symtab_lookup_text      (const symtab_t *t, const string_t *name);
void     symtab_free             (symtab_t *t);
int      symtab_add_borrowed_text(symtab_t *t, const string_t *name, expr_t *node);

typedef struct {
    string_t *name;
    expr_t   *expr;
    bool      is_constant;
} expr_binding_entry_t;

struct expr_bindings_t {
    size_t                count;
    expr_binding_entry_t *entries;
    dictionary_t         *index;
};

/* Binding construction from parsed symbols. */
expr_bindings_t * symtab_build_bindings   (const symtab_t *t);
expr_bindings_t * symtab_build_bindings_for_expr(const symtab_t *t,
                                                 const expr_t *expr);
expr_bindings_t * single_binding_from_node(expr_t *node);

#endif /* EXPR_STRINGIN_INTERNAL_H */
