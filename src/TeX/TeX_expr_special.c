/**
 * @file TeX_expr_special.c
 * @brief Special-function names, arguments and constant notation.
 *
 * Renders native expression structure through the controlled expression rendering interface.
 * Mathematical evaluation remains in expression; this backend owns mathematical typesetting syntax.
 */

#include <ctype.h>
#include <gmp.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression-t.h"
#include "string-t.h"

#define MARS_SHARED_EXPR_NOTATION_INTERNAL_ACCESS
#include "internal/expr_notation_internal.h"
#define MARS_SHARED_TeX_SEMANTIC_INTERNAL_ACCESS
#include "internal/TeX_semantic_internal.h"
#define MARS_SHARED_EXPR_RENDER_INTERNAL_ACCESS
#include "internal/expr_render_internal.h"
#define MARS_TeX_SEMANTIC_SYMBOLS_INTERNAL_ACCESS
#include "TeX_semantic_symbols.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "internal/TeX_internal.h"
#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_expr_private.h"

void TeX_emit_name(sbuf_t *b, const char *name)
{
    char *tex;

    if (!name || !*name) {
        sbuf_puts(b, "x");
        return;
    }
    if (strcmp(name, "±") == 0) {
        sbuf_puts(b, "\\pm");
        return;
    }

    bool compound_name = !expr_tostring_is_simple_name(name);

    if (compound_name)
        sbuf_puts(b, "\\mathit{");
    tex = TeX_from_unicode_math(name);
    if (tex) {
        sbuf_puts(b, tex);
        free(tex);
    } else {
        sbuf_puts(b, name);
    }
    if (compound_name)
        sbuf_putc(b, '}');
}

static const char *expr_known_constant_TeX_local(number_t value)
{
    if (num_eq(value, NUM_SQRT1ONPI))
        return "\\frac{1}{\\sqrt{\\pi}}";
    if (num_eq(value, NUM_2_SQRTPI))
        return "\\frac{2}{\\sqrt{\\pi}}";
    if (num_eq(value, NUM_NEG_TWO_OVER_SQRT_PI))
        return "-\\frac{2}{\\sqrt{\\pi}}";
    if (num_eq(value, NUM_SQRT_PI))
        return "\\sqrt{\\pi}";
    if (num_eq(value, NUM_SQRT_2PI))
        return "\\sqrt{2\\pi}";
    if (num_eq(value, NUM_INV_SQRT_2PI))
        return "\\frac{1}{\\sqrt{2\\pi}}";
    if (num_eq(value, NUM_SQRT_PI_OVER_TWO))
        return "\\sqrt{\\pi/2}";
    if (num_eq(value, NUM_SQRT2))
        return "\\sqrt{2}";
    if (num_eq(value, NUM_SQRT3))
        return "\\sqrt{3}";
    if (num_eq(value, NUM_SQRT_HALF))
        return "\\sqrt{1/2}";
    if (num_eq(value, NUM_SQRT2_OVER_TWO))
        return "\\frac{\\sqrt{2}}{2}";
    if (num_eq(value, NUM_SQRT3_OVER_TWO))
        return "\\frac{\\sqrt{3}}{2}";
    return NULL;
}

void emit_TeX_number_value(sbuf_t *b, number_t value)
{
    if (expr_emit_exact_rational_TeX(b, value))
        return;
    const char *constant_TeX = NULL;
    char *text = expr_number_to_string_local(num_clone(value));
    char *tex;

    if (!text)
        return;

    if (!num_is_exact(value))
        constant_TeX = expr_known_constant_TeX_local(value);
    if (constant_TeX) {
        sbuf_puts(b, constant_TeX);
        free(text);
        return;
    }

    tex = expr_text_to_TeX_local(text);
    if (tex) {
        sbuf_puts(b, tex);
        free(tex);
    } else {
        sbuf_puts(b, text);
    }
    free(text);
}

void emit_TeX_const_value(sbuf_t *b, const expr_t *expr)
{
    if (expr && expr_emit_exact_rational_TeX(b, expr->c))
        return;
    const char *constant_TeX = NULL;
    char *text = expr_const_to_string_local(expr);
    char *tex;

    if (!text)
        return;

    if (expr && !num_is_exact(expr->c))
        constant_TeX = expr_known_constant_TeX_local(expr->c);
    if (constant_TeX) {
        sbuf_puts(b, constant_TeX);
        free(text);
        return;
    }

    tex = expr_text_to_TeX_local(text);
    if (tex) {
        sbuf_puts(b, tex);
        free(tex);
    } else {
        sbuf_puts(b, text);
    }
    free(text);
}

bool emit_TeX_exp_unit_fraction_root(const expr_t *arg, sbuf_t *b)
{
    long numerator;
    long denominator;

    if (!expr_is_const(arg) || !num_get_small_rational(arg->c, &numerator, &denominator) || numerator != 1L ||
        denominator <= 1L)
        return false;

    if (denominator == 2L) {
        sbuf_puts(b, "\\sqrt{e}");
    } else {
        char index_text[32];

        snprintf(index_text, sizeof(index_text), "%ld", denominator);
        sbuf_puts(b, "\\sqrt[");
        sbuf_puts(b, index_text);
        sbuf_puts(b, "]{e}");
    }
    return true;
}

void emit_TeX_atom(const expr_t *f, sbuf_t *b)
{
    if (expr_is_const(f)) {
        if (expr_tostring_should_emit_binding_expr(f)) {
            char *text = expr_binding_expr_to_TeX(f->binding_expr);

            if (text) {
                sbuf_puts(b, text);
                free(text);
            }
        } else if (f->name && *f->name)
            TeX_emit_name(b, f->name);
        else
            emit_TeX_const_value(b, f);
        return;
    }

    if (expr_is_var(f)) {
        TeX_emit_name(b, f->name ? f->name : "x");
        return;
    }

    emit_TeX_number_value(b, expr_eval(f));
}

const char *TeX_unary_name(const expr_t *f)
{
    expr_notation_unary_t operation;
    return expr_notation_unary(f, &operation) && operation.name ? TeX_operation_name(operation.name) : NULL;
}

bool TeX_unary_has_bare_greek_argument(const expr_t *function)
{
    expr_notation_unary_t operation;
    return expr_notation_unary(function, &operation) && !operation.signal_function &&
           expr_notation_is_greek_symbol(operation.atomic_argument_name);
}

void emit_TeX_polygamma(const expr_t *f, sbuf_t *b)
{
    long order;
    char buf[32];

    if (!f || !expr_is_op(f, &ops_polygamma) || !f->a || !f->b)
        return;
    sbuf_puts(b, "\\psi^{(");
    if (expr_polygamma_order(f, &order)) {
        snprintf(buf, sizeof(buf), "%ld", order);
        sbuf_puts(b, buf);
    } else {
        TeX_emit_expr(f->a, b, PREC_LOWEST);
    }
    sbuf_puts(b, ")}(");
    TeX_emit_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

void emit_TeX_polylog(const expr_t *f, sbuf_t *b)
{
    long order;
    char buf[32];

    if (!expr_polylog_order(f, &order))
        return;
    snprintf(buf, sizeof(buf), "%ld", order);
    sbuf_puts(b, "\\operatorname{Li}_{");
    sbuf_puts(b, buf);
    sbuf_puts(b, "}(");
    TeX_emit_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

void emit_TeX_harmonic_poly(const expr_t *f, sbuf_t *b)
{
    if (!f || !expr_is_op(f, &ops_harmonic_poly) || !f->a || !f->b)
        return;
    sbuf_puts(b, "H_{");
    TeX_emit_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, "}(");
    TeX_emit_expr(f->b, b, PREC_LOWEST);
    sbuf_putc(b, ')');
}

void emit_TeX_legendre_chi(const expr_t *f, sbuf_t *b)
{
    long order;
    char buf[32];

    if (!expr_legendre_chi_order(f, &order))
        return;
    snprintf(buf, sizeof(buf), "%ld", order);
    sbuf_puts(b, "\\chi_{");
    sbuf_puts(b, buf);
    sbuf_puts(b, "}(");
    TeX_emit_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

void emit_TeX_appell_f1(const expr_t *f, sbuf_t *b)
{
    const expr_t *a = NULL;
    const expr_t *b1 = NULL;
    const expr_t *b2 = NULL;
    const expr_t *c = NULL;
    const expr_t *x = NULL;
    const expr_t *y = NULL;

    if (!expr_appell_f1_unpack(f, &a, &b1, &b2, &c, &x, &y))
        return;
    sbuf_puts(b, "F_{1}\\left(");
    TeX_emit_expr(a, b, 0);
    sbuf_puts(b, "; ");
    TeX_emit_expr(b1, b, 0);
    sbuf_puts(b, ", ");
    TeX_emit_expr(b2, b, 0);
    sbuf_puts(b, "; ");
    TeX_emit_expr(c, b, 0);
    sbuf_puts(b, "; ");
    TeX_emit_expr(x, b, 0);
    sbuf_puts(b, ", ");
    TeX_emit_expr(y, b, 0);
    sbuf_puts(b, "\\right)");
}

void emit_TeX_lauricella_f(const expr_t *f, sbuf_t *out)
{
    const expr_t *a = NULL;
    const expr_t **parameters = NULL;
    const expr_t *c = NULL;
    const expr_t **variables = NULL;
    size_t count = 0u;
    char prefix[64];

    if (!expr_lauricella_f_unpack(f, &a, &parameters, &c, &variables, &count))
        return;
    snprintf(prefix, sizeof(prefix), "F_D^{(%zu)}\\left(", count);
    sbuf_puts(out, prefix);
    TeX_emit_expr(a, out, 0);
    sbuf_puts(out, "; ");
    for (size_t i = 0u; i < count; ++i) {
        if (i > 0u)
            sbuf_puts(out, ", ");
        TeX_emit_expr(parameters[i], out, 0);
    }
    sbuf_puts(out, "; ");
    TeX_emit_expr(c, out, 0);
    sbuf_puts(out, "; ");
    for (size_t i = 0u; i < count; ++i) {
        if (i > 0u)
            sbuf_puts(out, ", ");
        TeX_emit_expr(variables[i], out, 0);
    }
    sbuf_puts(out, "\\right)");
    free(variables);
    free(parameters);
}

void emit_TeX_hypergeometric_pFq(const expr_t *f, sbuf_t *b)
{
    const expr_t **upper = NULL;
    const expr_t **lower = NULL;
    const expr_t *argument = NULL;
    size_t p = 0u;
    size_t q = 0u;
    char order[64];

    if (!expr_hypergeometric_pFq_unpack(f, &upper, &p, &lower, &q, &argument))
        return;
    snprintf(order, sizeof(order), "{}_{%zu}F_{%zu}\\left(", p, q);
    sbuf_puts(b, order);
    if (p == 0u)
        sbuf_puts(b, "-");
    for (size_t i = 0u; i < p; ++i) {
        if (i > 0u)
            sbuf_puts(b, ", ");
        TeX_emit_expr(upper[i], b, 0);
    }
    sbuf_puts(b, "; ");
    if (q == 0u)
        sbuf_puts(b, "-");
    for (size_t i = 0u; i < q; ++i) {
        if (i > 0u)
            sbuf_puts(b, ", ");
        TeX_emit_expr(lower[i], b, 0);
    }
    sbuf_puts(b, "; ");
    TeX_emit_expr(argument, b, 0);
    sbuf_puts(b, "\\right)");
    free(lower);
    free(upper);
}

void emit_TeX_lommel_s(const expr_t *f, sbuf_t *b)
{
    const expr_t *mu = NULL;
    const expr_t *nu = NULL;
    const expr_t *argument = NULL;

    if (!expr_lommel_s_unpack(f, &mu, &nu, &argument))
        return;
    sbuf_puts(b, "s_{");
    TeX_emit_expr(mu, b, 0);
    sbuf_puts(b, ",");
    TeX_emit_expr(nu, b, 0);
    sbuf_puts(b, "}\\left(");
    TeX_emit_expr(argument, b, 0);
    sbuf_puts(b, "\\right)");
}

void emit_TeX_lambert_wn(const expr_t *f, sbuf_t *b)
{
    sbuf_puts(b, "W_{");
    TeX_emit_expr(f->a, b, 0);
    sbuf_puts(b, "}(");
    TeX_emit_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}
