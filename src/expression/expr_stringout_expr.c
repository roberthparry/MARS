#include <stdlib.h>

#include "expr_bindings.h"
#include "expr_stringout.h"
#include "expr_stringout_internal.h"

static string_t *expr_text_from_owned_c_string(char *raw)
{
    string_t *text = raw ? string_new_with(raw) : NULL;

    free(raw);
    return text;
}

string_t *expr_to_text_expr(const expr_t *f)
{
    sbuf_t b;
    autoname_table_t vnames;
    varlist_t vl;
    varlist_t cl;
    const expr_t *g = f;
    string_t *out;

    if (f && f->binding_expr && !expr_is_const(f))
        return expr_text_from_owned_c_string(
            expr_binding_expr_to_string(f->binding_expr));

    autoname_init(&vnames);
    assign_unnamed_vars_dfs((expr_t *)f, &vnames);

    varlist_init(&vl);
    find_vars_dfs(g, &vl);

    varlist_init(&cl);
    find_explicit_named_consts_dfs(f, &cl);
    find_named_consts_dfs(g, &cl);

    sbuf_init(&b);
    if (vl.count == 0u && cl.count == 0u) {
        emit_expr(g, &b, PREC_LOWEST);
    } else {
        sbuf_putc(&b, '{');
        sbuf_putc(&b, ' ');
        emit_expr(g, &b, PREC_LOWEST);
        sbuf_putc(&b, ' ');
        sbuf_putc(&b, '|');
        sbuf_putc(&b, ' ');

        for (size_t i = 0u; i < vl.count; ++i) {
            expr_t *v = vl.vars[i];
            char *valbuf = binding_rhs_expr_string_local(v);

            emit_name(&b, expr_name_or_default(v, "x"));
            sbuf_puts(&b, " = ");
            if (valbuf) {
                sbuf_puts(&b, valbuf);
                free(valbuf);
            }
            if (i + 1u < vl.count)
                sbuf_puts(&b, ", ");
        }

        if (cl.count > 0u) {
            sbuf_puts(&b, "; ");
            for (size_t i = 0u; i < cl.count; ++i) {
                expr_t *c = cl.vars[i];
                char *valbuf = binding_rhs_expr_string_local(c);

                emit_name(&b, c->name);
                sbuf_puts(&b, " = ");
                if (valbuf) {
                    sbuf_puts(&b, valbuf);
                    free(valbuf);
                }
                if (i + 1u < cl.count)
                    sbuf_puts(&b, ", ");
            }
        }

        sbuf_putc(&b, ' ');
        sbuf_putc(&b, '}');
    }

    out = sbuf_to_string(&b);
    sbuf_free(&b);
    free(vl.vars);
    free(cl.vars);
    autoname_restore(&vnames);
    return out;
}

string_t *expr_to_text_unbound(const expr_t *f)
{
    sbuf_t b;
    autoname_table_t vnames;
    const expr_t *g = f;
    string_t *out;

    if (f && f->binding_expr && !expr_is_const(f))
        return expr_text_from_owned_c_string(
            expr_binding_expr_to_string(f->binding_expr));

    autoname_init(&vnames);
    assign_unnamed_vars_dfs((expr_t *)f, &vnames);

    sbuf_init(&b);
    emit_expr(g, &b, PREC_LOWEST);

    out = sbuf_to_string(&b);
    sbuf_free(&b);
    autoname_restore(&vnames);
    return out;
}
