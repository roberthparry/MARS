#include <stdlib.h>

#include "expr_bindings.h"
#include "expr_tostring.h"
#include "expr_tostring_internal.h"

static void emit_name_c(sbuf_t *b, const char *name)
{
    emit_name_func(b, name);
}

static void emit_function_arg_list(sbuf_t *b, const varlist_t *vl, const varlist_t *cl)
{
    for (size_t i = 0u; i < vl->count; ++i) {
        if (i > 0u)
            sbuf_puts(b, ", ");
        emit_name_c(b, expr_name_or_default(vl->vars[i], "x"));
    }
    for (size_t i = 0u; i < cl->count; ++i) {
        if (vl->count > 0u || i > 0u)
            sbuf_puts(b, ", ");
        emit_name_c(b, cl->vars[i]->name);
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

static void emit_c_binding_assignment_line(sbuf_t *b,
                                           const expr_t *dv,
                                           const char *name,
                                           int is_const)
{
    char *valbuf;

    sbuf_puts(b, "    ");
    if (is_const)
        sbuf_puts(b, "const ");
    emit_name_c(b, name);
    sbuf_puts(b, " = ");
    valbuf = binding_rhs_c_string_local(dv);
    if (valbuf) {
        sbuf_puts(b, valbuf);
        free(valbuf);
    }
    sbuf_puts(b, ";\n");
}

char *expr_to_string_function(const expr_t *f)
{
    sbuf_t b;
    autoname_table_t vnames;
    varlist_t vl;
    varlist_t cl;
    const expr_t *g = f;
    const char *fname = "expr";
    char *out;

    sbuf_init(&b);

    if (f && f->binding_expr && !expr_is_const(f)) {
        char *rhs = expr_binding_expr_to_function_string(f->binding_expr);

        if (rhs) {
            sbuf_puts(&b, "expression expr(void) {\n");
            sbuf_puts(&b, "    return ");
            sbuf_puts(&b, rhs);
            sbuf_puts(&b, ";\n");
            sbuf_puts(&b, "}\n\n");
            sbuf_puts(&b, "expression expr_eval() {\n");
            sbuf_puts(&b, "    return expr();\n");
            sbuf_puts(&b, "}");
            free(rhs);

            out = expr_tostring_xstrdup(b.data);
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

    sbuf_puts(&b, "expression expr_eval() {\n");
    for (size_t i = 0u; i < vl.count; ++i) {
        expr_t *v = vl.vars[i];

        emit_c_binding_assignment_line(&b, v, expr_name_or_default(v, "x"), 0);
    }
    for (size_t i = 0u; i < cl.count; ++i) {
        expr_t *c = cl.vars[i];

        emit_c_binding_assignment_line(&b, c, c->name, 1);
    }

    sbuf_puts(&b, "    return ");
    sbuf_puts(&b, fname);
    sbuf_putc(&b, '(');
    emit_function_arg_list(&b, &vl, &cl);
    sbuf_puts(&b, ");\n");
    sbuf_puts(&b, "}");

    out = expr_tostring_xstrdup(b.data);
    sbuf_free(&b);
    free(vl.vars);
    free(cl.vars);
    autoname_restore(&vnames);
    return out;
}
