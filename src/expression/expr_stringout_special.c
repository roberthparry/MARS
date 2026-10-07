/**
 * @file expr_stringout_special.c
 * @brief Special-function names and argument notation.
 *
 * Supplies shared naming and style-specific forms for hypergeometric, Lommel, Lambert and related operations. It
 * prevents individual renderers from inventing incompatible spellings.
 *
 * This is part of the expression.h implementation. Preserve expression ownership, symbol identity and mathematical
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

#include "expr_bindings.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"
#include "expression.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#include "ustring.h"

void emit_TeX_name(sbuf_t *b, const char *name)
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
    tex = expr_tostring_texify(name);
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
            emit_TeX_name(b, f->name);
        else
            emit_TeX_const_value(b, f);
        return;
    }

    if (expr_is_var(f)) {
        emit_TeX_name(b, f->name ? f->name : "x");
        return;
    }

    emit_TeX_number_value(b, expr_eval(f));
}

const char *TeX_unary_name(const expr_t *f)
{
    if (!f || !f->ops)
        return NULL;

    if (expr_is_op(f, &ops_abs))
        return NULL;
    if (expr_is_sqrt_expr(f))
        return "\\sqrt";
    return f->ops->TeX_name;
}

bool TeX_unary_has_bare_greek_argument(const expr_t *function)
{
    static const bool explicit_argument[EXPR_KIND_COUNT] = {
        [EXPR_KIND_SGN]             = true,
        [EXPR_KIND_STEP]            = true,
        [EXPR_KIND_RECT]            = true,
        [EXPR_KIND_TRI]             = true,
        [EXPR_KIND_CIRC]            = true,
        [EXPR_KIND_SINC]            = true,
        [EXPR_KIND_DELTA]           = true,
        [EXPR_KIND_ANALYTIC_DELTA]  = true,
        [EXPR_KIND_PRINCIPAL_VALUE] = true,
        [EXPR_KIND_FINITE_PART]     = true,
    };
    if (!function || explicit_argument[function->ops->kind])
        return false;
    const expr_t *arg = function->a;
    const char *name;
    string_t *name_text;
    string_t *normalized;
    string_cursor_t *cursor;
    uint32_t codepoint = 0u;
    bool greek = false;

    if (!arg || (!expr_is_var(arg) && !expr_is_const(arg)) || !arg->name || !*arg->name)
        return false;

    name = arg->name[0] == '@' ? arg->name + 1 : arg->name;
    name_text = string_new_with(name);
    if (!name_text)
        return false;
    normalized = expr_normalise_greek_alias_text(name_text);
    if (normalized) {
        greek = true;
        goto cleanup;
    }

    cursor = string_cursor_new(name_text);
    if (cursor) {
        codepoint = rune_value(string_cursor_peek(cursor));
        string_cursor_next(cursor);
        greek = string_cursor_done(cursor) &&
                ((codepoint >= 0x0370u && codepoint <= 0x03ffu) || (codepoint >= 0x1f00u && codepoint <= 0x1fffu));
        string_cursor_free(cursor);
    }

cleanup:
    string_free(normalized);
    string_free(name_text);
    return greek;
}

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

static int expr_polygamma_order(const expr_t *f, long *order)
{
    return f && expr_is_op(f, &ops_polygamma) && f->a && expr_is_const(f->a) &&
           expr_try_get_small_integer_exponent(f->a->c, order) && *order >= 0;
}

int expr_has_polygamma_order(const expr_t *f)
{
    long order;

    return expr_polygamma_order(f, &order);
}

static int expr_polylog_order(const expr_t *f, long *order)
{
    return f && expr_is_op(f, &ops_polylog) && f->a && expr_is_const(f->a) &&
           expr_try_get_small_integer_exponent(f->a->c, order) && *order >= 0;
}

int expr_has_polylog_order(const expr_t *f)
{
    long order;

    return expr_polylog_order(f, &order);
}

static int expr_legendre_chi_order(const expr_t *f, long *order)
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
        emit_TeX_expr(f->a, b, PREC_LOWEST);
    }
    sbuf_puts(b, ")}(");
    emit_TeX_expr(f->b, b, 0);
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
    emit_TeX_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

void emit_TeX_harmonic_poly(const expr_t *f, sbuf_t *b)
{
    if (!f || !expr_is_op(f, &ops_harmonic_poly) || !f->a || !f->b)
        return;
    sbuf_puts(b, "H_{");
    emit_TeX_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, "}(");
    emit_TeX_expr(f->b, b, PREC_LOWEST);
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
    emit_TeX_expr(f->b, b, 0);
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
    emit_TeX_expr(a, b, 0);
    sbuf_puts(b, "; ");
    emit_TeX_expr(b1, b, 0);
    sbuf_puts(b, ", ");
    emit_TeX_expr(b2, b, 0);
    sbuf_puts(b, "; ");
    emit_TeX_expr(c, b, 0);
    sbuf_puts(b, "; ");
    emit_TeX_expr(x, b, 0);
    sbuf_puts(b, ", ");
    emit_TeX_expr(y, b, 0);
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
    emit_TeX_expr(a, out, 0);
    sbuf_puts(out, "; ");
    for (size_t i = 0u; i < count; ++i) {
        if (i > 0u)
            sbuf_puts(out, ", ");
        emit_TeX_expr(parameters[i], out, 0);
    }
    sbuf_puts(out, "; ");
    emit_TeX_expr(c, out, 0);
    sbuf_puts(out, "; ");
    for (size_t i = 0u; i < count; ++i) {
        if (i > 0u)
            sbuf_puts(out, ", ");
        emit_TeX_expr(variables[i], out, 0);
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
        emit_TeX_expr(upper[i], b, 0);
    }
    sbuf_puts(b, "; ");
    if (q == 0u)
        sbuf_puts(b, "-");
    for (size_t i = 0u; i < q; ++i) {
        if (i > 0u)
            sbuf_puts(b, ", ");
        emit_TeX_expr(lower[i], b, 0);
    }
    sbuf_puts(b, "; ");
    emit_TeX_expr(argument, b, 0);
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
    emit_TeX_expr(mu, b, 0);
    sbuf_puts(b, ",");
    emit_TeX_expr(nu, b, 0);
    sbuf_puts(b, "}\\left(");
    emit_TeX_expr(argument, b, 0);
    sbuf_puts(b, "\\right)");
}

void emit_TeX_lambert_wn(const expr_t *f, sbuf_t *b)
{
    sbuf_puts(b, "W_{");
    emit_TeX_expr(f->a, b, 0);
    sbuf_puts(b, "}(");
    emit_TeX_expr(f->b, b, 0);
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
