/**
 * @file TeX_expr_display.c
 * @brief TeX expression wrappers and derivative contexts.
 *
 * Wraps rendered TeX bodies with the required presentation context, including partial and total derivative
 * notation. Recursive node emission is handled by TeX_expr_emit.c.
 *
 * This is the native expression backend of the TeX module. Preserve expression ownership, symbol identity and mathematical
 * domain restrictions when extending these operations.
 */

#include <stdlib.h>
#include <string.h>

#define MARS_SHARED_TeX_SEMANTIC_INTERNAL_ACCESS
#include "internal/TeX_semantic_internal.h"
#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "internal/TeX_internal.h"
#define MARS_SHARED_EXPR_NOTATION_INTERNAL_ACCESS
#include "internal/expr_notation_internal.h"
#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_expr_private.h"

static int TeX_term_consume_leading_minus(char **term)
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

static char *TeX_body_for_node(const expr_t *expr, int parent_prec)
{
    sbuf_t b;
    char *out;

    if (parent_prec == TeX_PRECEDENCE_LOWEST) {
        TeX_context_t *context = expr_TeX_result_context(expr);
        TeX_t *notation = context ? TeX_from_context(context) : NULL;
        out = notation ? strdup(TeX_source(notation)) : NULL;
        TeX_free(notation);
        TeX_context_free(context);
        if (out)
            return out;
    }
    sbuf_init(&b);
    TeX_emit_expr(expr, &b, parent_prec);
    out = TeX_from_unicode_math(sbuf_c_str(&b));
    sbuf_free(&b);
    return out;
}

static char *TeX_aligned_add_terms(const expr_t *expr, size_t line_limit, int wrap_in_parens)
{
    expr_notation_terms_t *terms = expr_notation_terms_new(expr);
    sbuf_t b;
    char *one_line = NULL;
    char *out = NULL;

    if (expr_notation_terms_count(terms) < 2u)
        goto cleanup;

    one_line = TeX_body_for_node(expr, TeX_PRECEDENCE_LOWEST);
    if (!one_line || (line_limit > 0u && strlen(one_line) <= line_limit))
        goto cleanup;

    sbuf_init(&b);
    if (wrap_in_parens)
        sbuf_puts(&b, "\\left(\\begin{aligned}[t]\n");
    else
        sbuf_puts(&b, "\\begin{aligned}[t]\n");

    for (size_t i = 0u; i < expr_notation_terms_count(terms); ++i) {
        int sign;
        const expr_t *term_expr = expr_notation_terms_get(terms, i, &sign);
        char *term_alloc = TeX_body_for_node(term_expr, TeX_PRECEDENCE_ADDITION);
        char *term = term_alloc;

        if (!term_alloc) {
            sbuf_free(&b);
            goto cleanup;
        }
        if (TeX_term_consume_leading_minus(&term))
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
    out = strdup(sbuf_c_str(&b));
    sbuf_free(&b);

cleanup:
    free(one_line);
    expr_notation_terms_free(terms);
    return out;
}

static char *TeX_aligned_scaled_add_terms(const expr_t *sum, const expr_t *factor, size_t line_limit)
{
    expr_notation_terms_t *terms = expr_notation_terms_new(sum);
    sbuf_t b;
    char *one_line = NULL;
    char *factor_alloc = NULL;
    char *factor_TeX = NULL;
    char *out = NULL;
    int factor_sign = 1;

    if (!sum || !factor || expr_notation_terms_count(terms) < 2u)
        goto cleanup;

    one_line = TeX_body_for_node(sum, TeX_PRECEDENCE_LOWEST);
    if (!one_line || (line_limit > 0u && strlen(one_line) <= line_limit))
        goto cleanup;

    factor_alloc = TeX_body_for_node(factor, TeX_PRECEDENCE_MULTIPLICATION);
    factor_TeX = factor_alloc;
    if (!factor_alloc)
        goto cleanup;
    if (TeX_term_consume_leading_minus(&factor_TeX))
        factor_sign = -1;

    sbuf_init(&b);
    sbuf_puts(&b, "\\begin{aligned}[t]\n");
    for (size_t i = 0u; i < expr_notation_terms_count(terms); ++i) {
        int sign;
        const expr_t *term_expr = expr_notation_terms_get(terms, i, &sign);
        char *term_alloc = TeX_body_for_node(term_expr, TeX_PRECEDENCE_MULTIPLICATION);
        char *term = term_alloc;
        sign *= factor_sign;

        if (!term_alloc) {
            sbuf_free(&b);
            goto cleanup;
        }
        if (TeX_term_consume_leading_minus(&term))
            sign = -sign;
        if (i > 0u)
            sbuf_puts(&b, " \\\\\n");
        sbuf_putc(&b, '&');
        if (sign < 0)
            sbuf_puts(&b, i == 0u ? "-" : "{} - ");
        else if (i > 0u)
            sbuf_puts(&b, "{} + ");
        sbuf_puts(&b, factor_TeX);
        TeX_emit_mul_separator(factor, term_expr, &b);
        sbuf_puts(&b, term);
        free(term_alloc);
    }
    sbuf_puts(&b, "\n\\end{aligned}");
    out = strdup(sbuf_c_str(&b));
    sbuf_free(&b);

cleanup:
    free(one_line);
    free(factor_alloc);
    expr_notation_terms_free(terms);
    return out;
}

static char *TeX_wrapped_mul_with_additive_factor(const expr_t *expr, size_t line_limit)
{
    const expr_t *factor = NULL;
    const expr_t *sum = NULL;

    if (!expr_notation_scaled_sum(expr, &factor, &sum))
        return NULL;

    return TeX_aligned_scaled_add_terms(sum, factor, line_limit);
}

static char *TeX_wrapped_body_inner(const expr_t *expr, size_t line_limit)
{
    char *one_line = NULL;
    char *wrapped = NULL;

    if (!expr)
        return NULL;
    if (line_limit == 0u)
        line_limit = 110u;

    one_line = TeX_body_for_node(expr, TeX_PRECEDENCE_LOWEST);
    if (!one_line || strlen(one_line) <= line_limit || expr_distribution_has_qualification(expr))
        return one_line;

    wrapped = TeX_wrapped_mul_with_additive_factor(expr, line_limit);
    if (!wrapped)
        wrapped = TeX_aligned_add_terms(expr, line_limit, 0);
    if (wrapped) {
        free(one_line);
        return wrapped;
    }
    return one_line;
}

static bool TeX_primitive_definition(const expr_t *initial, sbuf_t *out)
{
    if (!initial)
        return true;
    TeX_context_t *context = expr_notation_primitive_context(initial);
    TeX_t *definition = context ? TeX_from_context(context) : NULL;
    if (definition)
        sbuf_puts(out, TeX_source(definition));
    bool success = definition != NULL;
    TeX_free(definition);
    TeX_context_free(context);
    return success;
}

static int expr_to_TeX_parts_mode(const expr_t *expr, char **expr_out, char **bindings_out, bool preserve_root_symbol)
{
    autoname_table_t vnames;
    const expr_t *g;
    sbuf_t expression_text;
    sbuf_t bindings;

    expr_init_singletons();
    if (!expr_out || !bindings_out)
        return -1;

    *expr_out = NULL;
    *bindings_out = NULL;
    expr_t *initial = NULL;
    expr_t *display = expr ? expr_notation_primitive_display(expr, &initial) : NULL;

    /*
     * A binding expression records the user's surface syntax, but its compact
     * D^n(y) notation cannot distinguish the nth derivative from a power of a
     * derivative.  Render the expression tree whenever formal derivatives are
     * present so (Dx(y))^2 remains visibly distinct from Dxx(y).
     */
    TeX_context_t *surface_context = !display ? expr_notation_surface_context(expr, preserve_root_symbol) : NULL;
    if (surface_context) {
        TeX_t *surface = TeX_from_context(surface_context);
        *expr_out = surface ? strdup(TeX_source(surface)) : NULL;
        *bindings_out = strdup("");
        TeX_free(surface);
        TeX_context_free(surface_context);
        if (*expr_out && *bindings_out)
            return 0;
        free(*expr_out);
        free(*bindings_out);
        *expr_out = NULL;
        *bindings_out = NULL;
        return -1;
    }

    if (!expr) {
        *expr_out = strdup("NULL");
        *bindings_out = strdup("");
        return (*expr_out && *bindings_out) ? 0 : -1;
    }

    autoname_init(&vnames);
    assign_unnamed_vars_dfs((expr_t *)expr, &vnames);
    g = expr;

    sbuf_init(&expression_text);
    TeX_context_t *semantic_context = expr_TeX_result_context(display ? display : g);
    TeX_t *semantic_body = semantic_context ? TeX_from_context(semantic_context) : NULL;
    if (semantic_body)
        sbuf_puts(&expression_text, TeX_source(semantic_body));
    else
        TeX_emit_expr(display ? display : g, &expression_text, TeX_PRECEDENCE_LOWEST);
    TeX_free(semantic_body);
    TeX_context_free(semantic_context);
    bool primitive_ready = TeX_primitive_definition(initial, &expression_text);
    expr_free(initial);
    expr_free(display);

    sbuf_init(&bindings);
    TeX_context_t *binding_context = NULL;
    bool bindings_ready = primitive_ready && expr_notation_bindings_context(expr, &binding_context);
    TeX_t *semantic_bindings = binding_context ? TeX_from_context(binding_context) : NULL;
    if (semantic_bindings)
        sbuf_puts(&bindings, TeX_source(semantic_bindings));
    else if (binding_context)
        bindings_ready = false;
    TeX_free(semantic_bindings);
    TeX_context_free(binding_context);

    if (bindings_ready) {
        *expr_out = TeX_from_unicode_math(sbuf_c_str(&expression_text));
        *bindings_out = TeX_from_unicode_math(sbuf_c_str(&bindings));
    }

    sbuf_free(&expression_text);
    sbuf_free(&bindings);
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

/* Render standalone TeX and bindings using the normal expression semantics. */
int expr_to_TeX_parts(const expr_t *expr, char **expr_out, char **bindings_out)
{
    return expr_to_TeX_parts_mode(expr, expr_out, bindings_out, false);
}

/* Retain a bound root variable in TeX entries of larger mathematical objects. */
int expr_to_TeX_symbolic_parts(const expr_t *expr, char **expr_out, char **bindings_out)
{
    return expr_to_TeX_parts_mode(expr, expr_out, bindings_out, true);
}

char *expr_to_TeX_body_wrapped(const expr_t *expr, size_t line_limit)
{
    autoname_table_t vnames;
    char *body = NULL;

    if (!expr)
        return NULL;
    if (expr_notation_requires_unwrapped_body(expr))
        return expr_to_TeX_body(expr);

    autoname_init(&vnames);
    assign_unnamed_vars_dfs((expr_t *)expr, &vnames);
    expr_t *initial = NULL;
    expr_t *display = expr_notation_primitive_display(expr, &initial);
    body = TeX_wrapped_body_inner(display ? display : expr, line_limit);
    if (body && initial) {
        sbuf_t defined;
        sbuf_init(&defined);
        sbuf_puts(&defined, body);
        bool primitive_ready = TeX_primitive_definition(initial, &defined);
        free(body);
        body = primitive_ready ? strdup(sbuf_c_str(&defined)) : NULL;
        sbuf_free(&defined);
    }
    expr_free(display);
    expr_free(initial);
    autoname_restore(&vnames);
    return body;
}
