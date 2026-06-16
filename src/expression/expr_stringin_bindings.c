#include <stdio.h>
#include <stdlib.h>

#include "qfloat.h"
#include "expr_stringin_internal.h"

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

int symtab_has_text(const symtab_t *t, const string_t *name)
{
    if (!t)
        return 0;
    for (int i = 0; i < t->count; i++)
        if (string_compare(t->entries[i].name, name) == 0)
            return 1;
    return 0;
}

void symtab_add_text(symtab_t *t, const string_t *name, expr_t *node)
{
    if (t->count == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 8;
        t->entries = (sym_t *)realloc(t->entries, (size_t)t->cap * sizeof(sym_t));
        if (!t->entries) {
            fprintf(stderr, "out of memory\n");
            abort();
        }
    }

    t->entries[t->count].name = string_clone(name);
    if (!t->entries[t->count].name) {
        fprintf(stderr, "out of memory\n");
        abort();
    }
    t->entries[t->count].node = node;
    t->count++;
}

expr_t *symtab_lookup_text(const symtab_t *t, const string_t *name)
{
    if (!t)
        return NULL;
    for (int i = 0; i < t->count; i++)
        if (string_compare(t->entries[i].name, name) == 0)
            return t->entries[i].node;
    return NULL;
}

void symtab_free(symtab_t *t)
{
    for (int i = 0; i < t->count; i++) {
        string_free(t->entries[i].name);
        expr_free(t->entries[i].node);
    }
    free(t->entries);
    symtab_init(t);
}

int symtab_add_borrowed_text(symtab_t *t, const string_t *name, expr_t *node)
{
    if (!t || !name || !node)
        return -1;

    expr_retain(node);
    symtab_add_text(t, name, node);
    return 0;
}

static size_t binding_name_hash(const void *key)
{
    return (size_t)string_hash(*(string_t * const *)key);
}

static int binding_name_cmp(const void *a, const void *b)
{
    const string_t *ka = *(const string_t * const *)a;
    const string_t *kb = *(const string_t * const *)b;

    return string_compare(ka, kb);
}

static dictionary_t *binding_index_create(void)
{
    return dictionary_create(sizeof(string_t *),
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
        for (size_t i = 0; i < bindings->count; ++i) {
            string_free(bindings->entries[i].name);
            expr_free(bindings->entries[i].expr);
        }
    }
    dictionary_destroy(bindings->index);
    free(bindings->entries);
    free(bindings);
}

static expr_bindings_t *bindings_create(size_t count)
{
    expr_bindings_t *bindings = calloc(1, sizeof(*bindings));

    if (!bindings)
        return NULL;

    bindings->entries = calloc(count ? count : 1u, sizeof(bindings->entries[0]));
    bindings->index = binding_index_create();
    if (!bindings->entries || !bindings->index) {
        bindings_destroy_partial(bindings);
        return NULL;
    }

    bindings->count = count;
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

    if (!t || t->count <= 0)
        return NULL;

    bindings = bindings_create((size_t)t->count);
    if (!bindings)
        return NULL;
    for (int i = 0; i < t->count; ++i) {
        expr_binding_entry_t *entry;

        entry = &bindings->entries[i];
        entry->name = string_clone(t->entries[i].name);
        if (!entry->name) {
            bindings_destroy_partial(bindings);
            return NULL;
        }
        entry->expr = t->entries[i].node;
        expr_retain(entry->expr);
        entry->is_constant = (t->entries[i].node &&
                              t->entries[i].node->ops == &ops_const);
        if (bindings_index_entry(bindings, entry) != 0) {
            bindings_destroy_partial(bindings);
            return NULL;
        }
    }

    return bindings;
}

static int symtab_binding_is_needed_for_expr(const expr_t *expr,
                                             const expr_t *node)
{
    expr_t *vars[1];
    bool used[1];

    if (!node)
        return 0;
    if (!expr || !expr_is_var(node))
        return 1;
    if (node->binding_expr)
        return 1;

    vars[0] = (expr_t *)node;
    return expr_collect_var_usage(expr, 1u, vars, used) && used[0];
}

expr_bindings_t *symtab_build_bindings_for_expr(const symtab_t *t,
                                                const expr_t *expr)
{
    expr_bindings_t *bindings;
    size_t count = 0u;
    size_t out = 0u;

    if (!t || t->count <= 0)
        return NULL;

    for (int i = 0; i < t->count; ++i)
        if (symtab_binding_is_needed_for_expr(expr, t->entries[i].node))
            count++;

    if (count == 0u)
        return NULL;

    bindings = bindings_create(count);
    if (!bindings)
        return NULL;

    for (int i = 0; i < t->count; ++i) {
        expr_binding_entry_t *entry;

        if (!symtab_binding_is_needed_for_expr(expr, t->entries[i].node))
            continue;

        entry = &bindings->entries[out++];
        entry->name = string_clone(t->entries[i].name);
        if (!entry->name) {
            bindings_destroy_partial(bindings);
            return NULL;
        }
        entry->expr = t->entries[i].node;
        expr_retain(entry->expr);
        entry->is_constant = (t->entries[i].node &&
                              t->entries[i].node->ops == &ops_const);
        if (bindings_index_entry(bindings, entry) != 0) {
            bindings_destroy_partial(bindings);
            return NULL;
        }
    }

    return bindings;
}

expr_bindings_t *single_binding_from_node(expr_t *node)
{
    expr_bindings_t *bindings;

    if (!node || !node->name || !*node->name)
        return NULL;

    bindings = bindings_create(1u);
    if (!bindings)
        return NULL;
    bindings->entries[0].name = string_new_with(node->name);
    if (!bindings->entries[0].name) {
        bindings_destroy_partial(bindings);
        return NULL;
    }
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
