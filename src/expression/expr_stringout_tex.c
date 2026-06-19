#include <stdlib.h>

#include "expr_bindings.h"
#include "expr_stringout.h"
#include "expr_stringout_internal.h"

int expr_to_tex_parts(const expr_t *dv, char **expr_out, char **bindings_out)
{
    autoname_table_t vnames;
    const expr_t *g;
    varlist_t vl;
    varlist_t cl;
    sbuf_t expr;
    sbuf_t bindings;

    if (!expr_out || !bindings_out)
        return -1;

    *expr_out = NULL;
    *bindings_out = NULL;

    if (dv && dv->binding_expr && !expr_is_const(dv)) {
        *expr_out = expr_binding_expr_to_tex(dv->binding_expr);
        *bindings_out = expr_tostring_xstrdup("");
        return (*expr_out && *bindings_out) ? 0 : -1;
    }

    if (!dv) {
        *expr_out = expr_tostring_xstrdup("NULL");
        *bindings_out = expr_tostring_xstrdup("");
        return (*expr_out && *bindings_out) ? 0 : -1;
    }

    autoname_init(&vnames);
    assign_unnamed_vars_dfs((expr_t *)dv, &vnames);
    g = dv;

    varlist_init(&vl);
    varlist_init(&cl);
    find_vars_dfs(g, &vl);
    find_explicit_named_consts_dfs(dv, &cl);
    find_named_consts_dfs(g, &cl);

    sbuf_init(&expr);
    emit_tex_expr(g, &expr, PREC_LOWEST);

    sbuf_init(&bindings);
    if (vl.count > 0u || cl.count > 0u) {
        for (size_t i = 0u; i < vl.count; ++i) {
            expr_t *v = vl.vars[i];
            char *binding_text;

            if (i > 0u)
                sbuf_puts(&bindings, ", ");
            emit_tex_name(&bindings, expr_name_or_default(v, "x"));
            sbuf_puts(&bindings, " = ");
            binding_text = binding_rhs_tex_string_local(v);
            if (binding_text) {
                sbuf_puts(&bindings, binding_text);
                free(binding_text);
            }
        }

        if (cl.count > 0u) {
            sbuf_puts(&bindings, "; ");
            for (size_t i = 0u; i < cl.count; ++i) {
                expr_t *c = cl.vars[i];
                char *binding_text;

                if (i > 0u)
                    sbuf_puts(&bindings, ", ");
                emit_tex_name(&bindings, c->name);
                sbuf_puts(&bindings, " = ");
                binding_text = binding_rhs_tex_string_local(c);
                if (binding_text) {
                    sbuf_puts(&bindings, binding_text);
                    free(binding_text);
                }
            }
        }
    }

    *expr_out = expr_tostring_texify(sbuf_c_str(&expr));
    *bindings_out = expr_tostring_texify(sbuf_c_str(&bindings));

    sbuf_free(&expr);
    sbuf_free(&bindings);
    free(vl.vars);
    free(cl.vars);
    autoname_restore(&vnames);

    if (!*expr_out || !*bindings_out) {
        free(*expr_out);
        free(*bindings_out);
        *expr_out = NULL;
        *bindings_out = NULL;
        return -1;
    }

    return 0;
}

char *expr_to_tex_body(const expr_t *expr)
{
    char *body = NULL;
    char *bindings = NULL;

    if (!expr)
        return NULL;
    if (expr_to_tex_parts(expr, &body, &bindings) == 0) {
        free(bindings);
        return body;
    }
    free(body);
    free(bindings);
    return NULL;
}
