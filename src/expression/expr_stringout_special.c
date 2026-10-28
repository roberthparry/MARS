/**
 * @file expr_stringout_special.c
 * @brief Special-function names and argument notation.
 *
 * Supplies shared naming and style-specific forms for hypergeometric, Lommel, Lambert and related operations. It
 * prevents individual renderers from inventing incompatible spellings.
 *
 * This is part of the expr_t implementation. Preserve expression ownership, symbol identity and mathematical
 * domain restrictions when extending these operations.
 */

/* Special-function names, arguments and TeX atoms. */

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

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_bindings.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"


const char *expr_unary_name(const expr_t *f)
{
    if (!f || !f->ops)
        return "?";
    return expr_ops_expression_name(f->ops);
}

/* Share indexed mathematical notation across the cylindrical function families. */
const char *expr_cylindrical_symbol(const expr_t *f)
{
    static const char *const symbols[] = {
        [EXPR_KIND_BESSEL_J] = "J",
        [EXPR_KIND_BESSEL_Y] = "Y",
        [EXPR_KIND_BESSEL_K] = "K",
        [EXPR_KIND_BESSEL_I] = "I",
        [EXPR_KIND_STRUVE_L] = "𝐋",
        [EXPR_KIND_STRUVE_H] = "𝐇",
    };
    size_t kind = (size_t)f->ops->kind;
    return kind < sizeof(symbols) / sizeof(symbols[0]) ? symbols[kind] : NULL;
}

int expr_polygamma_order(const expr_t *f, long *order)
{
    return f && expr_is_op(f, &ops_polygamma) && f->a && expr_is_const(f->a) &&
           expr_try_get_small_integer_exponent(f->a->c, order) && *order >= 0;
}

int expr_has_polygamma_order(const expr_t *f)
{
    long order;

    return expr_polygamma_order(f, &order);
}

int expr_polylog_order(const expr_t *f, long *order)
{
    return f && expr_is_op(f, &ops_polylog) && f->a && expr_is_const(f->a) &&
           expr_try_get_small_integer_exponent(f->a->c, order) && *order >= 0;
}

int expr_has_polylog_order(const expr_t *f)
{
    long order;

    return expr_polylog_order(f, &order);
}

int expr_legendre_chi_order(const expr_t *f, long *order)
{
    return f && expr_is_op(f, &ops_legendre_chi) && f->a && expr_is_const(f->a) &&
           expr_try_get_small_integer_exponent(f->a->c, order) && *order >= 0;
}

int expr_has_legendre_chi_order(const expr_t *f)
{
    long order;

    return expr_legendre_chi_order(f, &order);
}

void emit_expr_lambert_wn(const expr_t *f, sbuf_t *b)
{
    sbuf_puts(b, expr_ops_expression_name(f->ops));
    sbuf_putc(b, '(');
    emit_expr(f->a, b, 0);
    sbuf_puts(b, ", ");
    emit_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

void emit_expr_polygamma(const expr_t *f, sbuf_t *b)
{
    long order;

    if (!expr_polygamma_order(f, &order))
        return;
    sbuf_puts(b, expr_ops_expression_name(f->ops));
    sbuf_puts(b, "⁽");
    emit_superscript_int(b, order);
    sbuf_puts(b, "⁾(");
    emit_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

void emit_expr_polylog(const expr_t *f, sbuf_t *b)
{
    long order;

    if (!expr_polylog_order(f, &order))
        return;
    if (order == 1L) {
        sbuf_puts(b, "polylog(1, ");
        emit_expr(f->b, b, 0);
        sbuf_putc(b, ')');
        return;
    }
    sbuf_puts(b, "Li");
    emit_subscript_int(b, order);
    sbuf_putc(b, '(');
    emit_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

void emit_expr_legendre_chi(const expr_t *f, sbuf_t *b)
{
    long order;

    if (!expr_legendre_chi_order(f, &order))
        return;
    sbuf_puts(b, "χ");
    emit_subscript_int(b, order);
    sbuf_putc(b, '(');
    emit_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

void emit_expr_appell_f1(const expr_t *f, sbuf_t *b)
{
    const expr_t *a = NULL;
    const expr_t *b1 = NULL;
    const expr_t *b2 = NULL;
    const expr_t *c = NULL;
    const expr_t *x = NULL;
    const expr_t *y = NULL;

    if (!expr_appell_f1_unpack(f, &a, &b1, &b2, &c, &x, &y))
        return;
    sbuf_puts(b, expr_ops_expression_name(f->ops));
    sbuf_putc(b, '(');
    emit_expr(a, b, 0);
    sbuf_puts(b, "; ");
    emit_expr(b1, b, 0);
    sbuf_puts(b, ", ");
    emit_expr(b2, b, 0);
    sbuf_puts(b, "; ");
    emit_expr(c, b, 0);
    sbuf_puts(b, "; ");
    emit_expr(x, b, 0);
    sbuf_puts(b, ", ");
    emit_expr(y, b, 0);
    sbuf_putc(b, ')');
}

void emit_expr_lauricella_f(const expr_t *f, sbuf_t *out)
{
    const expr_t *a = NULL;
    const expr_t **parameters = NULL;
    const expr_t *c = NULL;
    const expr_t **variables = NULL;
    size_t count = 0u;
    char prefix[64];

    if (!expr_lauricella_f_unpack(f, &a, &parameters, &c, &variables, &count))
        return;
    snprintf(prefix, sizeof(prefix), "(%zu, ", count);
    sbuf_puts(out, expr_ops_expression_name(f->ops));
    sbuf_puts(out, prefix);
    emit_expr(a, out, 0);
    for (size_t i = 0u; i < count; ++i) {
        sbuf_puts(out, ", ");
        emit_expr(parameters[i], out, 0);
    }
    sbuf_puts(out, ", ");
    emit_expr(c, out, 0);
    for (size_t i = 0u; i < count; ++i) {
        sbuf_puts(out, ", ");
        emit_expr(variables[i], out, 0);
    }
    sbuf_putc(out, ')');
    free(variables);
    free(parameters);
}

void emit_expr_hypergeometric_pFq(const expr_t *f, sbuf_t *b)
{
    const expr_t **upper = NULL;
    const expr_t **lower = NULL;
    const expr_t *argument = NULL;
    size_t p = 0u;
    size_t q = 0u;
    char counts[64];

    if (!expr_hypergeometric_pFq_unpack(f, &upper, &p, &lower, &q, &argument))
        return;
    snprintf(counts, sizeof(counts), "(%zu, %zu", p, q);
    sbuf_puts(b, expr_ops_expression_name(f->ops));
    sbuf_puts(b, counts);
    for (size_t i = 0u; i < p; ++i) {
        sbuf_puts(b, ", ");
        emit_expr(upper[i], b, 0);
    }
    for (size_t i = 0u; i < q; ++i) {
        sbuf_puts(b, ", ");
        emit_expr(lower[i], b, 0);
    }
    sbuf_puts(b, ", ");
    emit_expr(argument, b, 0);
    sbuf_putc(b, ')');
    free(lower);
    free(upper);
}

void emit_expr_lommel_s(const expr_t *f, sbuf_t *b)
{
    const expr_t *mu = NULL;
    const expr_t *nu = NULL;
    const expr_t *argument = NULL;

    if (!expr_lommel_s_unpack(f, &mu, &nu, &argument))
        return;
    sbuf_puts(b, expr_ops_expression_name(f->ops));
    sbuf_putc(b, '(');
    emit_expr(mu, b, 0);
    sbuf_puts(b, ", ");
    emit_expr(nu, b, 0);
    sbuf_puts(b, ", ");
    emit_expr(argument, b, 0);
    sbuf_putc(b, ')');
}

void emit_func_polygamma(const expr_t *f, sbuf_t *b)
{
    long order;
    char buf[32];

    if (!expr_polygamma_order(f, &order))
        return;
    snprintf(buf, sizeof(buf), "%ld", order);
    emit_function_builtin_name(b, f->ops);
    sbuf_putc(b, '(');
    sbuf_puts(b, buf);
    sbuf_puts(b, ", ");
    emit_func(f->b, b, 0);
    sbuf_putc(b, ')');
}

void emit_func_appell_f1(const expr_t *f, sbuf_t *b)
{
    const expr_t *a = NULL;
    const expr_t *b1 = NULL;
    const expr_t *b2 = NULL;
    const expr_t *c = NULL;
    const expr_t *x = NULL;
    const expr_t *y = NULL;

    if (!expr_appell_f1_unpack(f, &a, &b1, &b2, &c, &x, &y))
        return;
    emit_function_builtin_name(b, f->ops);
    sbuf_putc(b, '(');
    emit_func(a, b, 0);
    sbuf_puts(b, ", ");
    emit_func(b1, b, 0);
    sbuf_puts(b, ", ");
    emit_func(b2, b, 0);
    sbuf_puts(b, ", ");
    emit_func(c, b, 0);
    sbuf_puts(b, ", ");
    emit_func(x, b, 0);
    sbuf_puts(b, ", ");
    emit_func(y, b, 0);
    sbuf_putc(b, ')');
}

void emit_func_lauricella_f(const expr_t *f, sbuf_t *out)
{
    const expr_t *a = NULL;
    const expr_t **parameters = NULL;
    const expr_t *c = NULL;
    const expr_t **variables = NULL;
    size_t count = 0u;
    char prefix[64];

    if (!expr_lauricella_f_unpack(f, &a, &parameters, &c, &variables, &count))
        return;
    snprintf(prefix, sizeof(prefix), "(%zu, ", count);
    emit_function_builtin_name(out, f->ops);
    sbuf_puts(out, prefix);
    emit_func(a, out, 0);
    for (size_t i = 0u; i < count; ++i) {
        sbuf_puts(out, ", ");
        emit_func(parameters[i], out, 0);
    }
    sbuf_puts(out, ", ");
    emit_func(c, out, 0);
    for (size_t i = 0u; i < count; ++i) {
        sbuf_puts(out, ", ");
        emit_func(variables[i], out, 0);
    }
    sbuf_putc(out, ')');
    free(variables);
    free(parameters);
}

void emit_func_hypergeometric_pFq(const expr_t *f, sbuf_t *b)
{
    const expr_t **upper = NULL;
    const expr_t **lower = NULL;
    const expr_t *argument = NULL;
    size_t p = 0u;
    size_t q = 0u;
    char counts[64];

    if (!expr_hypergeometric_pFq_unpack(f, &upper, &p, &lower, &q, &argument))
        return;
    snprintf(counts, sizeof(counts), "(%zu, %zu", p, q);
    emit_function_builtin_name(b, f->ops);
    sbuf_puts(b, counts);
    for (size_t i = 0u; i < p; ++i) {
        sbuf_puts(b, ", ");
        emit_func(upper[i], b, 0);
    }
    for (size_t i = 0u; i < q; ++i) {
        sbuf_puts(b, ", ");
        emit_func(lower[i], b, 0);
    }
    sbuf_puts(b, ", ");
    emit_func(argument, b, 0);
    sbuf_putc(b, ')');
    free(lower);
    free(upper);
}

void emit_func_lommel_s(const expr_t *f, sbuf_t *b)
{
    const expr_t *mu = NULL;
    const expr_t *nu = NULL;
    const expr_t *argument = NULL;

    if (!expr_lommel_s_unpack(f, &mu, &nu, &argument))
        return;
    emit_function_builtin_name(b, f->ops);
    sbuf_putc(b, '(');
    emit_func(mu, b, 0);
    sbuf_puts(b, ", ");
    emit_func(nu, b, 0);
    sbuf_puts(b, ", ");
    emit_func(argument, b, 0);
    sbuf_putc(b, ')');
}

void emit_func_lambert_wn(const expr_t *f, sbuf_t *b)
{
    emit_function_builtin_name(b, f->ops);
    sbuf_putc(b, '(');
    emit_func(f->a, b, 0);
    sbuf_puts(b, ", ");
    emit_func(f->b, b, 0);
    sbuf_putc(b, ')');
}

/* Emit the canonical built-in name stored for FUNCTION source. */
void emit_function_builtin_name(sbuf_t *b, const expr_ops_t *ops)
{
    sbuf_puts(b, expr_ops_function_name(ops));
}
