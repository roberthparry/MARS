#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr_bindings.h"
#include "expr_stringout.h"
#include "expr_stringout_internal.h"

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

static void varlist_add(varlist_t *vl, expr_t *v);

static int varlist_contains_equiv(const varlist_t *vl, const expr_t *v)
{
    size_t i;

    if (!vl || !v)
        return 0;

    for (i = 0u; i < vl->count; ++i) {
        expr_t *existing = vl->vars[i];

        if (existing == v)
            return 1;
        if (expr_is_var(existing) && expr_is_var(v) &&
            existing->var_id != 0 && existing->var_id == v->var_id)
            return 1;
        if (existing->name && *existing->name &&
            v->name && *v->name &&
            strcmp(existing->name, v->name) == 0)
            return 1;
    }

    return 0;
}

static void find_vars_dfs_impl(const expr_t *expr,
                               varlist_t *vars,
                               varlist_t *bound_vars)
{
    const expr_t *dummy;
    const expr_t *lower;
    const expr_t *upper;

    if (!expr)
        return;

    if (expr_is_var(expr)) {
        if (!varlist_contains_equiv(bound_vars, expr))
            varlist_add(vars, (expr_t *)expr);
        return;
    }
    if (expr_is_const(expr))
        return;

    if (expr_is_op(expr, &ops_integral)) {
        lower = expr_integral_lower_bound_expr(expr);
        upper = expr_integral_upper_bound_expr(expr);
        dummy = expr_integral_dummy_expr(expr);

        find_vars_dfs_impl(lower, vars, bound_vars);
        find_vars_dfs_impl(upper, vars, bound_vars);

        if (dummy)
            varlist_add(bound_vars, (expr_t *)dummy);
        find_vars_dfs_impl(expr->a, vars, bound_vars);
        if (dummy && bound_vars->count > 0u)
            --bound_vars->count;
        return;
    }

    find_vars_dfs_impl(expr->a, vars, bound_vars);
    find_vars_dfs_impl(expr->b, vars, bound_vars);
}

static void varlist_add(varlist_t *vl, expr_t *v)
{
    for (size_t i = 0u; i < vl->count; ++i) {
        expr_t *existing = vl->vars[i];

        if (existing == v)
            return;
        if (expr_is_var(existing) && expr_is_var(v) &&
            existing->var_id != 0 && existing->var_id == v->var_id)
            return;
        if (existing->name && *existing->name &&
            v->name && *v->name &&
            strcmp(existing->name, v->name) == 0)
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
    varlist_t bound;

    if (!f || !vl)
        return;

    varlist_init(&bound);
    find_vars_dfs_impl(f, vl, &bound);
    free(bound.vars);
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
    string_t *text;
    char *out;

    if (dv && dv->binding_expr)
        return expr_binding_expr_to_string(dv->binding_expr);
    text = dv ? num_to_string(dv->c) : NULL;
    out = text ? expr_tostring_xstrdup(string_c_str(text)) : NULL;
    string_free(text);
    return out;
}

char *binding_rhs_tex_string_local(const expr_t *dv)
{
    string_t *number_text;
    char *text;
    char *tex;

    if (dv && dv->binding_expr)
        return expr_binding_expr_to_tex(dv->binding_expr);
    number_text = dv ? num_to_string(dv->c) : NULL;
    text = number_text ? expr_tostring_xstrdup(string_c_str(number_text)) : NULL;
    string_free(number_text);
    tex = expr_text_to_tex_local(text);
    free(text);
    return tex;
}

char *binding_rhs_c_string_local(const expr_t *dv)
{
    string_t *text;
    char *out;

    if (dv && dv->binding_expr)
        return expr_binding_expr_to_function_string(dv->binding_expr);
    text = dv ? num_to_string(dv->c) : NULL;
    out = text ? expr_tostring_xstrdup(string_c_str(text)) : NULL;
    string_free(text);
    return out;
}
