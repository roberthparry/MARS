#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qfloat.h"
#include "expr_stringin_internal.h"
#include "expr_stringout.h"
#include "expr_stringout_internal.h"

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
                                expr_binding_entry_t *entry);

static bool binding_is_noneditable_builtin(const expr_t *node)
{
    return expr_is_immortal_default_const_local(node) &&
           (!node->name || strcmp(node->name, "i") != 0);
}

expr_bindings_t *expr_bindings_from_expr_internal(const expr_t *expr)
{
    varlist_t vars;
    varlist_t constants;
    expr_bindings_t *bindings;
    size_t count = 0u;
    size_t out = 0u;

    if (!expr)
        return NULL;

    varlist_init(&vars);
    varlist_init(&constants);
    find_vars_dfs(expr, &vars);
    find_named_consts_dfs(expr, &constants);

    for (size_t i = 0u; i < vars.count; ++i)
        if (vars.vars[i]->name && *vars.vars[i]->name)
            count++;
    for (size_t i = 0u; i < constants.count; ++i)
        if (constants.vars[i]->name && *constants.vars[i]->name &&
            !binding_is_noneditable_builtin(constants.vars[i]))
            count++;

    if (count == 0u) {
        free(constants.vars);
        free(vars.vars);
        return NULL;
    }

    bindings = bindings_create(count);
    if (!bindings)
        goto fail;

    for (size_t group = 0u; group < 2u; ++group) {
        varlist_t *list = group == 0u ? &vars : &constants;

        for (size_t i = 0u; i < list->count; ++i) {
            expr_t *node = list->vars[i];
            expr_binding_entry_t *entry;

            if (!node->name || !*node->name)
                continue;
            if (group == 1u && binding_is_noneditable_builtin(node))
                continue;
            entry = &bindings->entries[out++];
            entry->name = string_new_with(node->name);
            if (!entry->name) {
                bindings_destroy_partial(bindings);
                bindings = NULL;
                goto fail;
            }
            entry->expr = node;
            expr_retain(entry->expr);
            entry->is_constant = expr_is_const(node);
            if (bindings_index_entry(bindings, entry) != 0) {
                bindings_destroy_partial(bindings);
                bindings = NULL;
                goto fail;
            }
        }
    }

    free(constants.vars);
    free(vars.vars);
    return bindings;

fail:
    free(constants.vars);
    free(vars.vars);
    return NULL;
}

static int bindings_index_entry(expr_bindings_t *bindings,
                                expr_binding_entry_t *entry)
{
    return dictionary_set(bindings->index, &entry->name, &entry) ? 0 : -1;
}

static bool bindings_has_name(const expr_bindings_t *bindings,
                              const string_t *name)
{
    if (!bindings || !name)
        return false;

    for (size_t i = 0u; i < bindings->count; ++i)
        if (string_compare(bindings->entries[i].name, name) == 0)
            return true;
    return false;
}

expr_bindings_t *expr_bindings_merge_internal(
    const expr_bindings_t *bindings,
    const expr_bindings_t *additional_bindings)
{
    expr_bindings_t *merged;
    size_t count = bindings ? bindings->count : 0u;
    size_t out = 0u;

    if (!bindings && !additional_bindings)
        return NULL;

    if (additional_bindings) {
        for (size_t i = 0u; i < additional_bindings->count; ++i) {
            const expr_binding_entry_t *entry = &additional_bindings->entries[i];

            if (!bindings_has_name(bindings, entry->name))
                count++;
        }
    }

    merged = bindings_create(count);
    if (!merged)
        return NULL;

    for (size_t group = 0u; group < 2u; ++group) {
        const expr_bindings_t *source =
            group == 0u ? bindings : additional_bindings;

        if (!source)
            continue;
        for (size_t i = 0u; i < source->count; ++i) {
            const expr_binding_entry_t *source_entry = &source->entries[i];
            expr_binding_entry_t *entry;

            if (group == 1u &&
                bindings_has_name(bindings, source_entry->name))
                continue;

            entry = &merged->entries[out++];
            entry->name = string_clone(source_entry->name);
            if (!entry->name) {
                bindings_destroy_partial(merged);
                return NULL;
            }
            entry->expr = source_entry->expr;
            expr_retain(entry->expr);
            entry->is_constant = source_entry->is_constant;
            if (bindings_index_entry(merged, entry) != 0) {
                bindings_destroy_partial(merged);
                return NULL;
            }
        }
    }

    merged->has_symbolic_derivative =
        (bindings && bindings->has_symbolic_derivative) ||
        (additional_bindings &&
         additional_bindings->has_symbolic_derivative);
    merged->has_symbolic_integral =
        (bindings && bindings->has_symbolic_integral) ||
        (additional_bindings &&
         additional_bindings->has_symbolic_integral);
    return merged;
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

static int expr_contains_node(const expr_t *expr, const expr_t *node)
{
    if (!expr || !node)
        return 0;
    if (expr == node)
        return 1;
    return expr_contains_node(expr->a, node) ||
           expr_contains_node(expr->b, node);
}

static int expr_integral_binds_name(const expr_t *expr, const char *name)
{
    const expr_t *dummy;

    if (!expr || !name)
        return 0;
    if (expr_is_op(expr, &ops_integral)) {
        dummy = expr_integral_dummy_expr(expr);
        if (dummy && dummy->name && strcmp(dummy->name, name) == 0)
            return 1;
    }
    return expr_integral_binds_name(expr->a, name) ||
           expr_integral_binds_name(expr->b, name);
}

static int expr_contains_integral(const expr_t *expr)
{
    if (!expr)
        return 0;
    if (expr_is_op(expr, &ops_integral))
        return 1;
    return expr_contains_integral(expr->a) ||
           expr_contains_integral(expr->b);
}

static int symtab_binding_is_needed_for_expr(const expr_t *expr,
                                             const expr_t *node)
{
    expr_t *vars[1];
    bool used[1] = { false };

    if (!node)
        return 0;
    if (!expr)
        return 0;
    if (expr_is_const(node)) {
        if (binding_is_noneditable_builtin(node))
            return 0;
        if (expr_contains_node(expr, node))
            return 1;
        if (node->name && expr_integral_binds_name(expr, node->name))
            return 0;
        if (node->name && strcmp(node->name, "d") == 0 &&
            expr_contains_integral(expr))
            return 0;
        return 1;
    }
    if (!expr_is_var(node))
        return 0;
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

    if (!node || !node->name || !*node->name ||
        binding_is_noneditable_builtin(node))
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
