#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr_bindings.h"
#include "expr_tostring.h"
#include "expr_tostring_internal.h"

static const char subscript_digits[10][4] = {
    "\xE2\x82\x80", "\xE2\x82\x81", "\xE2\x82\x82", "\xE2\x82\x83",
    "\xE2\x82\x84", "\xE2\x82\x85", "\xE2\x82\x86", "\xE2\x82\x87",
    "\xE2\x82\x88", "\xE2\x82\x89",
};

static char *make_subscript_name(char prefix, int idx)
{
    char digits[16];
    int nd = 0;
    int n = idx;
    char *buf;
    int pos = 1;

    do {
        digits[nd++] = (char)(n % 10);
        n /= 10;
    } while (n > 0);

    buf = expr_tostring_xmalloc(1u + (size_t)nd * 3u + 1u);
    buf[0] = prefix;
    for (int i = nd - 1; i >= 0; --i) {
        memcpy(buf + pos, subscript_digits[(unsigned char)digits[i]], 3u);
        pos += 3;
    }
    buf[pos] = '\0';
    return buf;
}

void autoname_init(autoname_table_t *t)
{
    t->entries = NULL;
    t->count = 0u;
    t->cap = 0u;
}

void autoname_restore(autoname_table_t *t)
{
    for (size_t i = 0u; i < t->count; ++i) {
        t->entries[i].node->name = NULL;
        free(t->entries[i].buf);
    }
    free(t->entries);
    t->entries = NULL;
    t->count = 0u;
    t->cap = 0u;
}

void assign_unnamed_vars_dfs(expr_t *f, autoname_table_t *t)
{
    if (!f)
        return;

    if (expr_is_var(f)) {
        if (f->name && *f->name)
            return;
        for (size_t i = 0u; i < t->count; ++i) {
            if (t->entries[i].node == f)
                return;
        }
        if (t->count == t->cap) {
            t->cap = t->cap ? t->cap * 2u : 4u;
            t->entries = realloc(t->entries, t->cap * sizeof(*t->entries));
            if (!t->entries) {
                fprintf(stderr, "auto-name: out of memory\n");
                abort();
            }
        }
        t->entries[t->count].node = f;
        t->entries[t->count].buf = make_subscript_name('x', (int)t->count);
        f->name = t->entries[t->count].buf;
        ++t->count;
        return;
    }

    if (expr_is_const(f))
        return;

    assign_unnamed_vars_dfs(f->a, t);
    assign_unnamed_vars_dfs(f->b, t);
}

void varlist_init(varlist_t *vl)
{
    vl->vars = NULL;
    vl->count = 0u;
    vl->cap = 0u;
}

static void varlist_add(varlist_t *vl, expr_t *v)
{
    for (size_t i = 0u; i < vl->count; ++i) {
        if (vl->vars[i] == v)
            return;
    }

    if (vl->count == vl->cap) {
        vl->cap = vl->cap ? vl->cap * 2u : 4u;
        vl->vars = realloc(vl->vars, vl->cap * sizeof(*vl->vars));
        if (!vl->vars) {
            fprintf(stderr, "varlist_add: out of memory\n");
            abort();
        }
    }
    vl->vars[vl->count++] = v;
}

void find_vars_dfs(const expr_t *f, varlist_t *vl)
{
    if (!f)
        return;

    if (expr_is_var(f)) {
        varlist_add(vl, (expr_t *)f);
        return;
    }
    if (expr_is_const(f))
        return;

    find_vars_dfs(f->a, vl);
    find_vars_dfs(f->b, vl);
}

void find_named_consts_dfs(const expr_t *f, varlist_t *cl)
{
    if (!f)
        return;

    if (expr_is_const(f)) {
        if (f->name && *f->name && !expr_is_immortal_default_const_local(f))
            varlist_add(cl, (expr_t *)f);
        return;
    }
    if (expr_is_var(f))
        return;

    find_named_consts_dfs(f->a, cl);
    find_named_consts_dfs(f->b, cl);
}

void find_explicit_named_consts_dfs(const expr_t *f, varlist_t *cl)
{
    if (!f)
        return;

    if (expr_is_const(f)) {
        if (f->name && *f->name && f->binding_expr &&
            !expr_is_immortal_default_const_local(f))
            varlist_add(cl, (expr_t *)f);
        return;
    }
    if (expr_is_var(f))
        return;

    find_explicit_named_consts_dfs(f->a, cl);
    find_explicit_named_consts_dfs(f->b, cl);
}

const char *expr_name_or_default(const expr_t *dv, const char *fallback)
{
    return (dv->name && *dv->name) ? dv->name : fallback;
}

char *binding_rhs_expr_string_local(const expr_t *dv)
{
    if (dv && dv->binding_expr)
        return expr_binding_expr_to_string(dv->binding_expr);
    return expr_const_to_string_local(dv);
}

char *binding_rhs_tex_string_local(const expr_t *dv)
{
    if (dv && dv->binding_expr)
        return expr_binding_expr_to_tex(dv->binding_expr);
    return expr_const_to_string_local(dv);
}

char *binding_rhs_c_string_local(const expr_t *dv)
{
    if (dv && dv->binding_expr)
        return expr_binding_expr_to_function_string(dv->binding_expr);
    return expr_const_to_string_local(dv);
}
