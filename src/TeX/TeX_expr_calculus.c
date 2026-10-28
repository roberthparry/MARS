/**
 * @file TeX_expr_calculus.c
 * @brief Ordered derivatives, integral bounds and arbitrary functions.
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

static void emit_formal_partial_denominator(const expr_t *f, sbuf_t *b)
{
    size_t i = f->formal_wrt_count;
    bool first = true;

    while (i > 0u) {
        const expr_t *wrt = f->formal_wrts[i - 1u];
        size_t multiplicity = 1u;

        while (i > multiplicity && expr_struct_eq(f->formal_wrts[i - multiplicity - 1u], wrt)) {
            multiplicity++;
        }
        if (!first)
            sbuf_puts(b, "\\,");
        sbuf_puts(b, "\\partial ");
        TeX_emit_name(b, (wrt && wrt->name) ? wrt->name : "x");
        if (multiplicity > 1u) {
            char exponent[32];

            snprintf(exponent, sizeof(exponent), "^{%zu}", multiplicity);
            sbuf_puts(b, exponent);
        }
        first = false;
        i -= multiplicity;
    }
}

void TeX_emit_formal_derivative(const expr_t *f, sbuf_t *b)
{
    /* Keep a transformed expression full-sized beside its differential operator. */
    if (expr_notation_is_transform_derivative(f)) {
        char order[32] = "";
        if (f->formal_wrt_count > 1u)
            snprintf(order, sizeof(order), "^{%zu}", f->formal_wrt_count);
        sbuf_puts(b, "\\frac{d");
        sbuf_puts(b, order);
        sbuf_puts(b, "}{d ");
        TeX_emit_name(b, f->formal_wrts[0]->name ? f->formal_wrts[0]->name : "x");
        sbuf_puts(b, order);
        sbuf_puts(b, "}\\left[");
        TeX_emit_expr(f->a, b, PREC_LOWEST);
        sbuf_puts(b, "\\right]");
        return;
    }
    /* The default mathematical display uses a differential fraction, not the parser's D-name.
     * Keep the operand beside the operator so a composite expression remains full-sized. */
    sbuf_puts(b, "\\frac{\\partial");
    if (f->formal_wrt_count > 1u) {
        char order[32];
        snprintf(order, sizeof(order), "^{%zu}", f->formal_wrt_count);
        sbuf_puts(b, order);
    }
    sbuf_puts(b, "}{");
    emit_formal_partial_denominator(f, b);
    sbuf_puts(b, "}\\left[");
    TeX_emit_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, "\\right]");
}

void TeX_emit_arbitrary_function(const expr_t *f, sbuf_t *b)
{
    const char *name = f->name ? f->name : "F";
    size_t length = strlen(name);
    size_t prime_count = 0u;

    if (strcmp(name, "BesselJ") == 0 && f->a && f->a->ops == &ops_argument_list) {
        sbuf_puts(b, "J_{");
        TeX_emit_expr(f->a->a, b, PREC_LOWEST);
        sbuf_puts(b, "}\\left(");
        TeX_emit_expr(f->a->b, b, PREC_LOWEST);
        sbuf_puts(b, "\\right)");
        return;
    }

    if (strcmp(name, "LommelS") == 0 && f->a && f->a->ops == &ops_argument_list && f->a->a &&
        f->a->a->ops == &ops_argument_list) {
        sbuf_puts(b, "s_{");
        TeX_emit_expr(f->a->a->a, b, PREC_LOWEST);
        sbuf_puts(b, ",");
        TeX_emit_expr(f->a->a->b, b, PREC_LOWEST);
        sbuf_puts(b, "}\\left(");
        TeX_emit_expr(f->a->b, b, PREC_LOWEST);
        sbuf_puts(b, "\\right)");
        return;
    }

    if (strcmp(name, "Si") == 0 || strcmp(name, "Ci") == 0) {
        sbuf_puts(b, "\\operatorname{");
        sbuf_puts(b, name);
        sbuf_puts(b, "}\\left(");
        TeX_emit_expr(f->a, b, PREC_LOWEST);
        sbuf_puts(b, "\\right)");
        return;
    }

    while (prime_count < length && name[length - prime_count - 1u] == '\'')
        ++prime_count;
    if (prime_count > 0u && prime_count < length) {
        char *base_name = strndup(name, length - prime_count);

        if (base_name) {
            TeX_emit_name(b, base_name);
            free(base_name);
            for (size_t i = 0u; i < prime_count; ++i)
                sbuf_putc(b, '\'');
        } else {
            TeX_emit_name(b, name);
        }
    } else {
        TeX_emit_name(b, name);
    }
    sbuf_puts(b, "\\left(");
    TeX_emit_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, "\\right)");
}

void TeX_emit_argument_list(const expr_t *f, sbuf_t *b)
{
    TeX_emit_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    TeX_emit_expr(f->b, b, PREC_LOWEST);
}

void TeX_emit_integral(const expr_t *f, sbuf_t *b, int parent_prec)
{
    int need = PREC_UNARY < parent_prec;
    const expr_t *lower = expr_integral_lower_bound_expr(f);
    const expr_t *upper = expr_integral_upper_bound_expr(f);
    const expr_t *display_integrand = f ? f->a : NULL;
    const expr_t *display_dummy = expr_integral_dummy_expr(f);

    if (need)
        sbuf_puts(b, "\\left(");
    sbuf_puts(b, "\\int");
    if (lower) {
        sbuf_puts(b, "_{");
        TeX_emit_expr(lower, b, PREC_LOWEST);
        sbuf_putc(b, '}');
    }
    /* Match Function notation: an unbounded integral in its own dummy is indefinite. */
    if (lower || !expr_struct_eq(upper, display_dummy)) {
        sbuf_puts(b, "^{");
        TeX_emit_expr(upper, b, PREC_LOWEST);
        sbuf_putc(b, '}');
    }
    sbuf_putc(b, ' ');
    TeX_emit_expr(display_integrand, b, PREC_LOWEST);
    sbuf_puts(b, "\\, d");
    TeX_emit_expr(display_dummy, b, PREC_LOWEST);
    if (need)
        sbuf_puts(b, "\\right)");
}
