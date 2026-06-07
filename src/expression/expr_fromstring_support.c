#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qfloat.h"
#include "expr_fromstring.h"

void *fs_xmalloc(size_t n)
{
    void *p = malloc(n);

    if (!p) {
        fprintf(stderr, "out of memory\n");
        abort();
    }
    return p;
}

int fs_is_letter(unsigned int c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
        return 1;
    if (c >= 0x0391 && c <= 0x03A9)
        return 1;
    if (c >= 0x03B1 && c <= 0x03C9)
        return 1;
    return 0;
}

void symtab_init(symtab_t *t)
{
    t->entries = NULL;
    t->count = 0;
    t->cap = 0;
}

int symtab_has(const symtab_t *t, const char *name)
{
    if (!t)
        return 0;
    for (int i = 0; i < t->count; i++)
        if (strcmp(t->entries[i].name, name) == 0)
            return 1;
    return 0;
}

void symtab_add(symtab_t *t, const char *name, expr_t *node)
{
    size_t nl;

    if (t->count == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 8;
        t->entries = (sym_t *)realloc(t->entries, (size_t)t->cap * sizeof(sym_t));
        if (!t->entries) {
            fprintf(stderr, "out of memory\n");
            abort();
        }
    }

    nl = strlen(name) + 1;
    t->entries[t->count].name = (char *)fs_xmalloc(nl);
    memcpy(t->entries[t->count].name, name, nl);
    t->entries[t->count].node = node;
    t->count++;
}

expr_t *symtab_lookup(const symtab_t *t, const char *name)
{
    if (!t)
        return NULL;
    for (int i = 0; i < t->count; i++)
        if (strcmp(t->entries[i].name, name) == 0)
            return t->entries[i].node;
    return NULL;
}

void symtab_free(symtab_t *t)
{
    for (int i = 0; i < t->count; i++) {
        free(t->entries[i].name);
        expr_free(t->entries[i].node);
    }
    free(t->entries);
    symtab_init(t);
}

int symtab_add_borrowed(symtab_t *t, const char *name, expr_t *node)
{
    if (!t || !name || !node)
        return -1;

    expr_retain(node);
    symtab_add(t, name, node);
    return 0;
}

static size_t binding_name_hash(const void *key)
{
    const unsigned char *s = (const unsigned char *)*(const char * const *)key;
    size_t hash = 1469598103934665603ull;

    while (*s) {
        hash ^= (size_t)*s++;
        hash *= 1099511628211ull;
    }
    return hash;
}

static int binding_name_cmp(const void *a, const void *b)
{
    const char *ka = *(const char * const *)a;
    const char *kb = *(const char * const *)b;

    return strcmp(ka, kb);
}

static dictionary_t *binding_index_create(void)
{
    return dictionary_create(sizeof(char *),
                             sizeof(expr_binding_entry_t *),
                             binding_name_hash,
                             binding_name_cmp,
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             NULL);
}

static void bindings_destroy_partial(expr_bindings_t *bindings)
{
    if (!bindings)
        return;
    if (bindings->entries) {
        for (size_t i = 0; i < bindings->count; ++i)
            expr_free(bindings->entries[i].expr);
    }
    dictionary_destroy(bindings->index);
    free(bindings->storage);
    free(bindings);
}

static expr_bindings_t *bindings_create(size_t count, size_t total_name_bytes)
{
    expr_bindings_t *bindings = calloc(1, sizeof(*bindings));

    if (!bindings)
        return NULL;

    bindings->storage = calloc(1, sizeof(bindings->entries[0]) * count +
                                  total_name_bytes);
    bindings->index = binding_index_create();
    if (!bindings->storage || !bindings->index) {
        bindings_destroy_partial(bindings);
        return NULL;
    }

    bindings->count = count;
    bindings->entries = (expr_binding_entry_t *)bindings->storage;
    return bindings;
}

static int bindings_index_entry(expr_bindings_t *bindings,
                                expr_binding_entry_t *entry)
{
    return dictionary_set(bindings->index, &entry->name, &entry) ? 0 : -1;
}

expr_bindings_t *symtab_build_bindings(const symtab_t *t)
{
    expr_bindings_t *bindings;
    char *name_store;
    size_t total_name_bytes = 0;

    if (!t || t->count <= 0)
        return NULL;

    for (int i = 0; i < t->count; ++i)
        total_name_bytes += strlen(t->entries[i].name) + 1;

    bindings = bindings_create((size_t)t->count, total_name_bytes);
    if (!bindings)
        return NULL;
    name_store = (char *)(bindings->entries + t->count);
    for (int i = 0; i < t->count; ++i) {
        expr_binding_entry_t *entry;
        size_t n = strlen(t->entries[i].name) + 1;

        memcpy(name_store, t->entries[i].name, n);
        entry = &bindings->entries[i];
        entry->name = name_store;
        entry->expr = t->entries[i].node;
        expr_retain(entry->expr);
        entry->is_constant = (t->entries[i].node &&
                              t->entries[i].node->ops == &ops_const);
        if (bindings_index_entry(bindings, entry) != 0) {
            bindings_destroy_partial(bindings);
            return NULL;
        }
        name_store += n;
    }

    return bindings;
}

expr_bindings_t *single_binding_from_node(expr_t *node)
{
    expr_bindings_t *bindings;
    size_t n;

    if (!node || !node->name || !*node->name)
        return NULL;

    n = strlen(node->name) + 1;
    bindings = bindings_create(1u, n);
    if (!bindings)
        return NULL;
    bindings->entries[0].name = (char *)(bindings->entries + 1);
    memcpy((char *)bindings->entries[0].name, node->name, n);
    bindings->entries[0].expr = node;
    expr_retain(bindings->entries[0].expr);
    bindings->entries[0].is_constant = (node->ops == &ops_const);
    {
        expr_binding_entry_t *entry = &bindings->entries[0];

        if (bindings_index_entry(bindings, entry) != 0) {
            bindings_destroy_partial(bindings);
            return NULL;
        }
    }
    return bindings;
}
