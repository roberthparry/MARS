/**
 * @file expr_context.c
 * @brief Owned, hash-indexed symbol policy and binding snapshots.
 *
 * Implements the opaque expression context without depending on expression node
 * layout. The controlled expression facade supplies symbol identity and native
 * rewriting. Every update validates the complete replacement before publication;
 * no context caches algebra derived from an older revision.
 */
#include <errno.h>
#include <stdlib.h>

#include "dictionary-t.h"
#include "expr_context-t.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

typedef struct {
    uint64_t identity;
    string_t *name;
} context_key_t;

typedef struct {
    expr_context_role_t role;
    unsigned assumptions;
    number_t value;
} context_entry_t;

struct _expr_context_t {
    dictionary_t *symbols;
    uint64_t revision;
};

static size_t context_key_hash(const void *key)
{
    const context_key_t *symbol = key;
    uint64_t value = symbol->identity;
    if (!value)
        return (size_t)string_hash(symbol->name);
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    return (size_t)(value ^ (value >> 31));
}

static int context_key_compare(const void *left, const void *right)
{
    const context_key_t *a = left, *b = right;
    if (a->identity != b->identity)
        return a->identity < b->identity ? -1 : 1;
    return a->identity ? 0 : string_compare(a->name, b->name);
}

static void context_key_clone(void *target, const void *source)
{
    context_key_t *out = target;
    *out = *(const context_key_t *)source;
    out->name = out->name ? string_clone(out->name) : NULL;
}

static void context_key_destroy(void *key)
{
    string_free(((context_key_t *)key)->name);
}

static void context_entry_clone(void *target, const void *source)
{
    context_entry_t *out = target;
    *out = *(const context_entry_t *)source;
    out->value = num_clone(out->value);
}

static void context_entry_destroy(void *entry)
{
    num_destroy(&((context_entry_t *)entry)->value);
}

static bool context_key_make(const expr_t *symbol, context_key_t *key)
{
    const char *name = NULL;
    if (!expr_context_symbol_identity(symbol, &key->identity, &name)) {
        errno = EINVAL;
        return false;
    }
    key->name = name ? string_new_with(name) : NULL;
    return true;
}

static bool context_can_mutate(const expr_context_t *context)
{
    if (!context || context->revision == UINT64_MAX) {
        errno = context ? EOVERFLOW : EINVAL;
        return false;
    }
    return true;
}

/* Create an independent symbol-policy owner. */
expr_context_t *expr_context_new(void)
{
    expr_context_t *context = calloc(1, sizeof(*context));
    if (!context)
        return NULL;
    context->symbols = dictionary_create(sizeof(context_key_t), sizeof(context_entry_t), context_key_hash,
                                        context_key_compare, context_key_clone, context_key_destroy,
                                        NULL, context_entry_clone, context_entry_destroy);
    if (!context->symbols) {
        free(context);
        return NULL;
    }
    return context;
}

/* Snapshot source bindings without retaining the source graph. */
expr_context_t *expr_context_from_expression(const expr_t *source, const expr_t *wrt)
{
    if (!source || (wrt && !expr_is_variable(wrt))) {
        errno = EINVAL;
        return NULL;
    }
    expr_context_t *context = expr_context_new();
    if (context && !expr_context_capture_symbols(context, source, wrt)) {
        expr_context_free(context);
        return NULL;
    }
    return context;
}

/* Deep-copy policy, binding values and the revision stamp. */
expr_context_t *expr_context_clone(const expr_context_t *context)
{
    if (!context) {
        errno = EINVAL;
        return NULL;
    }
    expr_context_t *copy = expr_context_new();
    if (!copy)
        return NULL;
    size_t count = dictionary_size(context->symbols);
    for (size_t i = 0; i < count; ++i) {
        if (!dictionary_set(copy->symbols, dictionary_get_key(context->symbols, i),
                            dictionary_get_value(context->symbols, i))) {
            expr_context_free(copy);
            return NULL;
        }
    }
    copy->revision = context->revision;
    return copy;
}

/* Release all owned symbols and numbers. */
void expr_context_free(expr_context_t *context)
{
    if (context) {
        dictionary_destroy(context->symbols);
        free(context);
    }
}

/* Validate and publish a complete symbol entry. */
bool expr_context_set(expr_context_t *context, const expr_t *symbol, expr_context_role_t role,
                      unsigned assumptions, number_t value)
{
    if (!context_can_mutate(context))
        return false;
    const unsigned known = EXPR_CONTEXT_REAL | EXPR_CONTEXT_NONZERO | EXPR_CONTEXT_POSITIVE | EXPR_CONTEXT_NEGATIVE;
    if ((role != EXPR_CONTEXT_FREE && role != EXPR_CONTEXT_PARAMETER) || (assumptions & ~known) ||
        (!num_is_nan(value) && !num_is_finite(value))) {
        errno = EINVAL;
        return false;
    }
    if (assumptions & (EXPR_CONTEXT_POSITIVE | EXPR_CONTEXT_NEGATIVE))
        assumptions |= EXPR_CONTEXT_REAL | EXPR_CONTEXT_NONZERO;
    bool conflict = (assumptions & EXPR_CONTEXT_POSITIVE) && (assumptions & EXPR_CONTEXT_NEGATIVE);
    if (!num_is_nan(value)) {
        conflict = conflict || ((assumptions & EXPR_CONTEXT_REAL) && !num_is_real(value)) ||
                   ((assumptions & EXPR_CONTEXT_NONZERO) && num_eq(value, NUM_ZERO)) ||
                   ((assumptions & EXPR_CONTEXT_POSITIVE) && !num_gt(value, NUM_ZERO)) ||
                   ((assumptions & EXPR_CONTEXT_NEGATIVE) && !num_lt(value, NUM_ZERO));
    }
    if (conflict) {
        errno = EDOM;
        return false;
    }
    context_key_t key = {0};
    if (!context_key_make(symbol, &key))
        return false;
    context_entry_t entry = {.role = role, .assumptions = assumptions, .value = value};
    bool ok = dictionary_set(context->symbols, &key, &entry);
    context_key_destroy(&key);
    if (ok)
        ++context->revision;
    return ok;
}

/* Query by stable symbol identity, not by scanning entries. */
bool expr_context_get(const expr_context_t *context, const expr_t *symbol, expr_context_role_t *role,
                      unsigned *assumptions, number_t *value)
{
    context_key_t key = {0};
    if (!context || !context_key_make(symbol, &key))
        return false;
    dictionary_entry_t *found = NULL;
    bool ok = dictionary_get_entry(context->symbols, &key, &found);
    context_key_destroy(&key);
    if (!ok)
        return false;
    const context_entry_t *entry = dictionary_entry_value(found);
    if (role)
        *role = entry->role;
    if (assumptions)
        *assumptions = entry->assumptions;
    if (value)
        *value = num_clone(entry->value);
    return true;
}

/* Forget a symbol without affecting existing result snapshots. */
bool expr_context_remove(expr_context_t *context, const expr_t *symbol)
{
    context_key_t key = {0};
    if (!context_can_mutate(context) || !context_key_make(symbol, &key))
        return false;
    bool removed = dictionary_remove(context->symbols, &key);
    context_key_destroy(&key);
    if (removed)
        ++context->revision;
    return removed;
}

/* Expose the revision for caller-owned result caches. */
uint64_t expr_context_revision(const expr_context_t *context)
{
    return context ? context->revision : 0;
}

/* Delegate mathematical rewriting to the expression module. */
expr_t *expr_context_simplify(const expr_context_t *context, const expr_t *source)
{
    return context && source ? expr_context_snapshot(context, source, true) : NULL;
}

/* Preserve existing solver arrangements unless context evidence justifies a rewrite. */
expr_t *expr_context_refine(const expr_context_t *context, const expr_t *source)
{
    return context && source ? expr_context_snapshot(context, source, false) : NULL;
}

/* Query conservative structural evidence without evaluating a sampled formula. */
bool expr_context_proves(const expr_context_t *context, const expr_t *source, unsigned assumptions)
{
    const unsigned supported = EXPR_CONTEXT_REAL | EXPR_CONTEXT_NONZERO | EXPR_CONTEXT_POSITIVE | EXPR_CONTEXT_NEGATIVE;
    if (!context || !source || !assumptions || (assumptions & ~supported))
        return false;
    return (expr_context_proven_facts(context, source) & assumptions) == assumptions;
}

/* Evaluate an independent graph so earlier evaluation caches cannot leak across updates. */
number_t expr_context_evaluate(const expr_context_t *context, const expr_t *source)
{
    expr_t *snapshot = expr_context_simplify(context, source);
    number_t value = snapshot ? expr_eval(snapshot) : num_clone(NUM_NAN);
    expr_free(snapshot);
    return value;
}
