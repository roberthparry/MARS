#include <stdlib.h>
#include <string.h>

#include "expr_bindings.h"
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
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

    if (f && f->binding_expr && !expr_is_const(f) && !expr_binding_expr_is_array(f->binding_expr))
        return expr_text_from_owned_c_string(expr_binding_expr_to_string(f->binding_expr));

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

static void emit_conditioned_bindings(sbuf_t *buffer, const varlist_t *bindings)
{
    for (size_t i = 0u; i < bindings->count; ++i) {
        const expr_t *binding = bindings->vars[i];
        char *value = binding_rhs_expr_string_local(binding);

        if (i)
            sbuf_puts(buffer, ", ");
        emit_name(buffer, expr_name_or_default(binding, "x"));
        sbuf_puts(buffer, " = ");
        sbuf_puts(buffer, value && strcmp(value, "NAN") != 0 ? value : "?");
        free(value);
    }
}

static void emit_conditioned_case(sbuf_t *buffer, const expr_t *body, const expr_t *order,
                                  const char *comparison, const varlist_t *variables, const varlist_t *constants)
{
    sbuf_puts(buffer, "{ ");
    emit_expr(body, buffer, PREC_LOWEST);
    sbuf_puts(buffer, " | ");
    emit_conditioned_bindings(buffer, variables);
    sbuf_puts(buffer, "; ");
    emit_conditioned_bindings(buffer, constants);
    sbuf_puts(buffer, "; ");
    emit_expr(order, buffer, PREC_LOWEST);
    sbuf_puts(buffer, comparison);
    sbuf_puts(buffer, " }");
}

/* Render finite-series cases for display without changing the parseable source expression. */
char *expr_conditioned_cases_to_string(const expr_t *expr)
{
    const expr_t *order = NULL;
    const expr_t *endpoint = NULL;
    expr_t *digamma;
    expr_t *gamma;
    expr_t *harmonic;
    autoname_table_t names;
    varlist_t variables;
    varlist_t constants;
    sbuf_t buffer;
    char *out;

    if (!expr_series_zeta_difference_parts(expr, &order, &endpoint))
        return NULL;
    digamma = expr_digamma(endpoint);
    gamma = expr_from_string("gamma", NULL);
    harmonic = digamma && gamma ? expr_add(digamma, gamma) : NULL;
    expr_free(digamma);
    expr_free(gamma);
    if (!harmonic)
        return NULL;

    autoname_init(&names);
    assign_unnamed_vars_dfs((expr_t *)expr, &names);
    varlist_init(&variables);
    varlist_init(&constants);
    find_vars_dfs(expr, &variables);
    find_explicit_named_consts_dfs(expr, &constants);
    find_named_consts_dfs(expr, &constants);
    sbuf_init(&buffer);
    emit_conditioned_case(&buffer, harmonic, order, " = 1", &variables, &constants);
    sbuf_putc(&buffer, '\n');
    emit_conditioned_case(&buffer, expr, order, " ≠ 1", &variables, &constants);
    out = sbuf_to_c_string(&buffer);
    sbuf_free(&buffer);
    free(variables.vars);
    free(constants.vars);
    autoname_restore(&names);
    expr_free(harmonic);
    return out;
}

string_t *expr_to_text_unbound(const expr_t *f)
{
    sbuf_t b;
    autoname_table_t vnames;
    const expr_t *g = f;
    string_t *out;

    if (f && f->binding_expr && !expr_is_const(f) && !expr_binding_expr_is_array(f->binding_expr))
        return expr_text_from_owned_c_string(expr_binding_expr_to_string(f->binding_expr));

    autoname_init(&vnames);
    assign_unnamed_vars_dfs((expr_t *)f, &vnames);

    sbuf_init(&b);
    emit_expr(g, &b, PREC_LOWEST);

    out = sbuf_to_string(&b);
    sbuf_free(&b);
    autoname_restore(&vnames);
    return out;
}
