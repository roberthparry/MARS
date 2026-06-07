#ifndef EXPR_FROMSTRING_H
#define EXPR_FROMSTRING_H

#include <stddef.h>

#include "dictionary.h"
#include "expression.h"
#include "expr_internal.h"

typedef struct {
    char   *name;
    expr_t *node;
} sym_t;

typedef struct {
    sym_t *entries;
    int    count;
    int    cap;
} symtab_t;

void *fs_xmalloc(size_t n);
int fs_is_letter(unsigned int c);

void symtab_init(symtab_t *t);
int symtab_has(const symtab_t *t, const char *name);
void symtab_add(symtab_t *t, const char *name, expr_t *node);
expr_t *symtab_lookup(const symtab_t *t, const char *name);
void symtab_free(symtab_t *t);
int symtab_add_borrowed(symtab_t *t, const char *name, expr_t *node);

typedef struct {
    const char *name;
    expr_t *expr;
    bool is_constant;
} expr_binding_entry_t;

struct expr_bindings_t {
    size_t count;
    expr_binding_entry_t *entries;
    dictionary_t *index;
    void *storage;
};

expr_bindings_t *symtab_build_bindings(const symtab_t *t);
expr_bindings_t *single_binding_from_node(expr_t *node);

#endif
