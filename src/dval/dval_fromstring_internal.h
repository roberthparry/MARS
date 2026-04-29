#ifndef DVAL_FROMSTRING_INTERNAL_H
#define DVAL_FROMSTRING_INTERNAL_H

#include <stddef.h>

#include "dval.h"
#include "dval_internal.h"

typedef struct {
    char   *name;
    dval_t *node;
} sym_t;

typedef struct {
    sym_t *entries;
    int    count;
    int    cap;
} symtab_t;

void *fs_xmalloc(size_t n);
int fs_utf8_decode(const char *s, unsigned int *out);
int fs_is_letter(unsigned int c);
void skip_spaces(const char **pp, const char *end);
size_t scan_decimal_len(const char *s, const char *end);
int read_superscript(const char **pp);
char *read_simple_name(const char **pp);
char *read_bracketed_name(const char **pp);
char *read_any_name(const char **pp);
int fs_is_default_constant_name(const char *name);
char *fs_normalise_binding_name(const char *name);

void symtab_init(symtab_t *t);
int symtab_has(const symtab_t *t, const char *name);
void symtab_add(symtab_t *t, const char *name, dval_t *node);
dval_t *symtab_lookup(const symtab_t *t, const char *name);
void symtab_free(symtab_t *t);
int symtab_add_borrowed(symtab_t *t, const char *name, dval_t *node);
dval_binding_t *symtab_build_bindings(const symtab_t *t, size_t *number_out);
dval_binding_t *single_binding_from_node(dval_t *node, size_t *number_out);

#endif
