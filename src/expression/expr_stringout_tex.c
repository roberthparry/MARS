#include <stdlib.h>
#include <string.h>

#include "expr_bindings.h"
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"

typedef struct {
    const expr_t **exprs;
    int          *signs;
    size_t        count;
    size_t        cap;
} tex_add_terms_t;

static void tex_add_terms_free(tex_add_terms_t *terms)
{
    if (!terms)
        return;
    free(terms->exprs);
    free(terms->signs);
    terms->exprs = NULL;
    terms->signs = NULL;
    terms->count = 0u;
    terms->cap = 0u;
}

static int tex_add_terms_push(tex_add_terms_t *terms, const expr_t *expr, int sign)
{
    const expr_t **new_exprs;
    int *new_signs;
    size_t new_cap;

    if (!terms || !expr)
        return -1;
    if (terms->count == terms->cap) {
        new_cap = terms->cap ? terms->cap * 2u : 8u;
        new_exprs = realloc(terms->exprs, new_cap * sizeof(*new_exprs));
        if (!new_exprs)
            return -1;
        terms->exprs = new_exprs;
        new_signs = realloc(terms->signs, new_cap * sizeof(*new_signs));
        if (!new_signs)
            return -1;
        terms->signs = new_signs;
        terms->cap = new_cap;
    }
    terms->exprs[terms->count] = expr;
    terms->signs[terms->count] = sign < 0 ? -1 : 1;
    terms->count++;
    return 0;
}

static int tex_collect_add_terms(const expr_t *expr, int sign, tex_add_terms_t *terms)
{
    if (!expr)
        return -1;
    if (expr_is_neg(expr))
        return tex_collect_add_terms(expr->a, -sign, terms);
    if (expr_is_op(expr, &ops_add))
        return tex_collect_add_terms(expr->a, sign, terms) == 0 &&
               tex_collect_add_terms(expr->b, sign, terms) == 0
            ? 0
            : -1;
    if (expr_is_op(expr, &ops_sub))
        return tex_collect_add_terms(expr->a, sign, terms) == 0 &&
               tex_collect_add_terms(expr->b, -sign, terms) == 0
            ? 0
            : -1;
    return tex_add_terms_push(terms, expr, sign);
}

static int tex_term_consume_leading_minus(char **term)
{
    char *text;

    if (!term || !*term)
        return 0;
    text = *term;
    while (*text == ' ')
        text++;
    if (*text != '-')
        return 0;
    *term = text + 1;
    return 1;
}

static char *tex_body_for_node(const expr_t *expr, int parent_prec)
{
    sbuf_t b;
    char *out;

    sbuf_init(&b);
    emit_tex_expr(expr, &b, parent_prec);
    out = expr_tostring_texify(sbuf_c_str(&b));
    sbuf_free(&b);
    return out;
}

static char *tex_aligned_add_terms(const expr_t *expr, size_t line_limit,
                                   int wrap_in_parens)
{
    tex_add_terms_t terms = {0};
    sbuf_t b;
    char *one_line = NULL;
    char *out = NULL;

    if (!expr_is_addsub(expr) ||
        tex_collect_add_terms(expr, 1, &terms) != 0 ||
        terms.count < 2u)
        goto cleanup;

    one_line = tex_body_for_node(expr, PREC_LOWEST);
    if (!one_line || (line_limit > 0u && strlen(one_line) <= line_limit))
        goto cleanup;

    sbuf_init(&b);
    if (wrap_in_parens)
        sbuf_puts(&b, "\\left(\\begin{aligned}[t]\n");
    else
        sbuf_puts(&b, "\\begin{aligned}[t]\n");

    for (size_t i = 0u; i < terms.count; ++i) {
        char *term_alloc = tex_body_for_node(terms.exprs[i], PREC_ADD);
        char *term = term_alloc;
        int sign = terms.signs[i];

        if (!term_alloc) {
            sbuf_free(&b);
            goto cleanup;
        }
        if (tex_term_consume_leading_minus(&term))
            sign = -sign;
        if (i > 0u)
            sbuf_puts(&b, " \\\\\n");
        sbuf_putc(&b, '&');
        if (sign < 0)
            sbuf_puts(&b, i == 0u ? "-" : "{} - ");
        else if (i > 0u)
            sbuf_puts(&b, "{} + ");
        sbuf_puts(&b, term);
        free(term_alloc);
    }

    if (wrap_in_parens)
        sbuf_puts(&b, "\n\\end{aligned}\\right)");
    else
        sbuf_puts(&b, "\n\\end{aligned}");
    out = expr_tostring_xstrdup(sbuf_c_str(&b));
    sbuf_free(&b);

cleanup:
    free(one_line);
    tex_add_terms_free(&terms);
    return out;
}

static char *tex_aligned_scaled_add_terms(const expr_t *sum,
                                          const expr_t *factor,
                                          size_t line_limit)
{
    tex_add_terms_t terms = {0};
    sbuf_t b;
    char *one_line = NULL;
    char *factor_alloc = NULL;
    char *factor_tex = NULL;
    char *out = NULL;
    int factor_sign = 1;

    if (!sum || !factor || !expr_is_addsub(sum) ||
        tex_collect_add_terms(sum, 1, &terms) != 0 ||
        terms.count < 2u)
        goto cleanup;

    one_line = tex_body_for_node(sum, PREC_LOWEST);
    if (!one_line || (line_limit > 0u && strlen(one_line) <= line_limit))
        goto cleanup;

    factor_alloc = tex_body_for_node(factor, PREC_MUL);
    factor_tex = factor_alloc;
    if (!factor_alloc)
        goto cleanup;
    if (tex_term_consume_leading_minus(&factor_tex))
        factor_sign = -1;

    sbuf_init(&b);
    sbuf_puts(&b, "\\begin{aligned}[t]\n");
    for (size_t i = 0u; i < terms.count; ++i) {
        char *term_alloc = tex_body_for_node(terms.exprs[i], PREC_MUL);
        char *term = term_alloc;
        int sign = terms.signs[i] * factor_sign;

        if (!term_alloc) {
            sbuf_free(&b);
            goto cleanup;
        }
        if (tex_term_consume_leading_minus(&term))
            sign = -sign;
        if (i > 0u)
            sbuf_puts(&b, " \\\\\n");
        sbuf_putc(&b, '&');
        if (sign < 0)
            sbuf_puts(&b, i == 0u ? "-" : "{} - ");
        else if (i > 0u)
            sbuf_puts(&b, "{} + ");
        sbuf_puts(&b, factor_tex);
        sbuf_puts(&b, "\\cdot ");
        sbuf_puts(&b, term);
        free(term_alloc);
    }
    sbuf_puts(&b, "\n\\end{aligned}");
    out = expr_tostring_xstrdup(sbuf_c_str(&b));
    sbuf_free(&b);

cleanup:
    free(one_line);
    free(factor_alloc);
    tex_add_terms_free(&terms);
    return out;
}

static char *tex_wrapped_mul_with_additive_factor(const expr_t *expr,
                                                  size_t line_limit)
{
    const expr_t *factor = NULL;
    const expr_t *sum = NULL;

    if (!expr_is_mul(expr))
        return NULL;

    if (expr_is_addsub(expr->a) && !expr_is_addsub(expr->b)) {
        sum = expr->a;
        factor = expr->b;
    } else if (!expr_is_addsub(expr->a) && expr_is_addsub(expr->b)) {
        factor = expr->a;
        sum = expr->b;
    } else {
        return NULL;
    }

    return tex_aligned_scaled_add_terms(sum, factor, line_limit);
}

static char *tex_wrapped_body_inner(const expr_t *expr, size_t line_limit)
{
    char *one_line = NULL;
    char *wrapped = NULL;

    if (!expr)
        return NULL;
    if (line_limit == 0u)
        line_limit = 110u;

    one_line = tex_body_for_node(expr, PREC_LOWEST);
    if (!one_line || strlen(one_line) <= line_limit)
        return one_line;

    wrapped = tex_wrapped_mul_with_additive_factor(expr, line_limit);
    if (!wrapped && expr_is_addsub(expr))
        wrapped = tex_aligned_add_terms(expr, line_limit, 0);
    if (wrapped) {
        free(one_line);
        return wrapped;
    }
    return one_line;
}

static int tex_tree_contains_formal_derivative(const expr_t *expr)
{
    if (!expr)
        return 0;
    if (expr_is_formal_derivative(expr))
        return 1;
    return tex_tree_contains_formal_derivative(expr->a) ||
           tex_tree_contains_formal_derivative(expr->b);
}

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

    /*
     * A binding expression records the user's surface syntax, but its compact
     * D^n(y) notation cannot distinguish the nth derivative from a power of a
     * derivative.  Render the expression tree whenever formal derivatives are
     * present so (Dx(y))^2 remains visibly distinct from Dxx(y).
     */
    if (dv && dv->binding_expr && !expr_is_const(dv) &&
        !tex_tree_contains_formal_derivative(dv)) {
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

char *expr_to_tex_body_wrapped(const expr_t *expr, size_t line_limit)
{
    autoname_table_t vnames;
    char *body = NULL;

    if (!expr)
        return NULL;
    if (expr->binding_expr && !expr_is_const(expr))
        return expr_to_tex_body(expr);

    autoname_init(&vnames);
    assign_unnamed_vars_dfs((expr_t *)expr, &vnames);
    body = tex_wrapped_body_inner(expr, line_limit);
    autoname_restore(&vnames);
    return body;
}
