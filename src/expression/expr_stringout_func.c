#include <stdlib.h>
#include <string.h>

#include "expr_bindings.h"
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"

static void emit_name_c(sbuf_t *b, const char *name)
{
    emit_name_func(b, name);
}

static void emit_assignment_value(sbuf_t *b, char *value)
{
    static const struct {
        const char *symbol;
        const char *alias;
    } aliases[] = {
        {"π", "@pi"},
        {"φ", "@phi"},
        {"γ", "@gamma"},
        {"τ", "@tau"},
    };
    char *cursor = value;

    if (!cursor) {
        sbuf_puts(b, "NAN");
        return;
    }

    while (*cursor) {
        char *match = NULL;
        size_t alias_index = 0u;

        for (size_t i = 0u; i < sizeof(aliases) / sizeof(aliases[0]); ++i) {
            char *candidate = strstr(cursor, aliases[i].symbol);

            if (candidate && (!match || candidate < match)) {
                match = candidate;
                alias_index = i;
            }
        }
        if (!match) {
            sbuf_puts(b, cursor);
            break;
        }

        *match = '\0';
        sbuf_puts(b, cursor);
        *match = aliases[alias_index].symbol[0];
        sbuf_puts(b, aliases[alias_index].alias);
        cursor = match + strlen(aliases[alias_index].symbol);
    }
}

static void emit_function_output_arg_list(sbuf_t *b, const varlist_t *vl, const varlist_t *cl)
{
    for (size_t i = 0u; i < vl->count; ++i) {
        if (i > 0u)
            sbuf_puts(b, ", ");
        emit_name_c(b, expr_name_or_default(vl->vars[i], "x"));
    }
    for (size_t i = 0u; i < cl->count; ++i) {
        char *value;

        if (vl->count > 0u || i > 0u)
            sbuf_puts(b, ", ");
        value = binding_rhs_c_string_local(cl->vars[i]);
        if (value && strcmp(value, "NAN") != 0)
            sbuf_puts(b, value);
        else
            emit_name_c(b, cl->vars[i]->name);
        free(value);
    }
}

static void emit_variable_bindings(sbuf_t *b, const varlist_t *vl)
{
    for (size_t i = 0u; i < vl->count; ++i) {
        char *value = binding_rhs_c_string_local(vl->vars[i]);
        bool unknown = !value || strcmp(value, "NAN") == 0;

        if (unknown) {
            sbuf_puts(b, "// ");
            emit_name_c(b, expr_name_or_default(vl->vars[i], "x"));
            sbuf_puts(b, " = ?");
        } else {
            emit_name_c(b, expr_name_or_default(vl->vars[i], "x"));
            sbuf_puts(b, " = ");
            emit_assignment_value(b, value);
        }
        sbuf_putc(b, '\n');
        free(value);
    }
}

static void emit_function_param_list(sbuf_t *b, const varlist_t *vl, const varlist_t *cl)
{
    if (vl->count == 0u && cl->count == 0u) {
        sbuf_puts(b, "void");
        return;
    }

    for (size_t i = 0u; i < vl->count; ++i) {
        if (i > 0u)
            sbuf_puts(b, ", ");
        emit_name_c(b, expr_name_or_default(vl->vars[i], "x"));
    }
    for (size_t i = 0u; i < cl->count; ++i) {
        if (vl->count > 0u || i > 0u)
            sbuf_puts(b, ", ");
        sbuf_puts(b, "const ");
        emit_name_c(b, cl->vars[i]->name);
    }
}

string_t *expr_to_text_function(const expr_t *f)
{
    sbuf_t b;
    autoname_table_t vnames;
    varlist_t vl;
    varlist_t cl;
    const expr_t *g = f;
    const char *fname = "expr";
    string_t *out;

    sbuf_init(&b);

    if (f && f->binding_expr && !expr_is_const(f)) {
        char *rhs = expr_binding_expr_to_function_string(f->binding_expr);

        if (rhs) {
            sbuf_puts(&b, "expression expr(void) {\n");
            sbuf_puts(&b, "    return ");
            sbuf_puts(&b, rhs);
            sbuf_puts(&b, ";\n");
            sbuf_puts(&b, "}\n\n");
            sbuf_puts(&b, "output(expr());");
            free(rhs);

            out = sbuf_to_string(&b);
            sbuf_free(&b);
            return out;
        }
    }

    autoname_init(&vnames);
    assign_unnamed_vars_dfs((expr_t *)f, &vnames);

    varlist_init(&vl);
    find_vars_dfs(g, &vl);

    varlist_init(&cl);
    find_explicit_named_consts_dfs(f, &cl);
    find_named_consts_dfs(g, &cl);

    sbuf_puts(&b, "expression ");
    sbuf_puts(&b, fname);
    sbuf_putc(&b, '(');
    emit_function_param_list(&b, &vl, &cl);
    sbuf_puts(&b, ") {\n");
    sbuf_puts(&b, "    return ");
    emit_func(g, &b, PREC_LOWEST);
    sbuf_puts(&b, ";\n");
    sbuf_puts(&b, "}\n\n");

    emit_variable_bindings(&b, &vl);
    sbuf_puts(&b, "output(");
    sbuf_puts(&b, fname);
    sbuf_putc(&b, '(');
    emit_function_output_arg_list(&b, &vl, &cl);
    sbuf_puts(&b, "));");

    out = sbuf_to_string(&b);
    sbuf_free(&b);
    free(vl.vars);
    free(cl.vars);
    autoname_restore(&vnames);
    return out;
}
