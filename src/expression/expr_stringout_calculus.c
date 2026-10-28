/**
 * @file expr_stringout_calculus.c
 * @brief Integral, derivative and arbitrary-function notation.
 *
 * Renders calculus nodes, argument lists and executable integral forms for the output styles. It keeps calculus
 * operators and their bounds structurally distinct from surrounding coefficients.
 *
 * This is part of the expr_t implementation. Preserve expression ownership, symbol identity and mathematical
 * domain restrictions when extending these operations.
 */

/* Integral, derivative and arbitrary-function notation. */

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

void emit_expr_integral(const expr_t *f, sbuf_t *b, int parent_prec)
{
    int need = PREC_UNARY < parent_prec;
    const expr_t *lower = expr_integral_lower_bound_expr(f);
    const expr_t *upper = expr_integral_upper_bound_expr(f);
    const expr_t *display_integrand = f ? f->a : NULL;
    const expr_t *display_dummy = expr_integral_dummy_expr(f);
    bool group_upper = upper && expr_is_addsub(upper);
    bool group_lower = lower && expr_is_addsub(lower);
    bool group_integrand = expr_is_addsub(display_integrand);

    if (need)
        sbuf_putc(b, '(');
    sbuf_puts(b, "∫^");
    if (group_upper)
        sbuf_putc(b, '(');
    emit_expr(upper, b, PREC_LOWEST);
    if (group_upper)
        sbuf_putc(b, ')');
    if (lower) {
        sbuf_putc(b, '_');
        if (group_lower)
            sbuf_putc(b, '(');
        emit_expr(lower, b, PREC_LOWEST);
        if (group_lower)
            sbuf_putc(b, ')');
    }
    sbuf_putc(b, ' ');
    if (group_integrand)
        sbuf_putc(b, '(');
    emit_expr(display_integrand, b, PREC_LOWEST);
    if (group_integrand)
        sbuf_putc(b, ')');
    sbuf_puts(b, "·d");
    emit_expr(display_dummy, b, PREC_LOWEST);
    if (need)
        sbuf_putc(b, ')');
}

void emit_formal_derivative_expr(const expr_t *f, sbuf_t *b)
{
    sbuf_putc(b, 'D');
    for (size_t i = 0u; i < f->formal_wrt_count; ++i) {
        const expr_t *wrt = f->formal_wrts[i];

        emit_name(b, (wrt && wrt->name) ? wrt->name : "x");
    }
    sbuf_putc(b, '(');
    emit_expr(f->a, b, PREC_LOWEST);
    sbuf_putc(b, ')');
}

/* Ordered function derivatives keep their order separate from ordinary powers. */
void emit_ordered_derivative(const expr_t *f, sbuf_t *b, int style)
{
    void (*emit)(const expr_t *, sbuf_t *, int) = style == 1 ? emit_func : emit_expr;
    const char *name = f->a->name;
    bool builtin = !expr_is_arbitrary_function(f->a);
    if (builtin && style != 2) {
        sbuf_puts(b, style == 1 ? "derivative(" : "Derivative(");
        const expr_t *expanded = style == 1 ? expr_stringout_set_expanded_node(f->a) : NULL;
        emit(f->a, b, PREC_LOWEST);
        if (style == 1)
            expr_stringout_set_expanded_node(expanded);
        sbuf_puts(b, ", ");
        emit(f->b, b, PREC_LOWEST);
        sbuf_putc(b, ')');
        return;
    }
    if (style == 1)
        emit_name_func(b, name);
    else
        emit_name(b, name);
    number_t order = NUM_NAN;
    long p = 0, q = 0;
    bool primes = expr_match_const_value(f->b, &order) && num_get_small_rational(order, &p, &q) &&
                  q == 1 && p > 0 && p <= 3;
    num_destroy(&order);
    if (primes) {
        for (long i = 0; i < p; ++i)
            sbuf_putc(b, '\'');
    } else {
        sbuf_puts(b,  "^(");
        emit(f->b, b, PREC_LOWEST);
        sbuf_puts(b,  ")");
    }
    sbuf_puts(b,  "(");
    emit(f->a->a, b, PREC_LOWEST);
    sbuf_puts(b,  ")");
}


void emit_arbitrary_function_expr(const expr_t *f, sbuf_t *b)
{
    const char *name = f->name ? f->name : "F";
    size_t length = strlen(name);
    size_t prime_count = 0u;

    if (strcmp(name, "BesselJ") == 0 && f->a && f->a->ops == &ops_argument_list) {
        sbuf_puts(b, "BesselJ(");
        emit_expr(f->a, b, PREC_LOWEST);
        sbuf_putc(b, ')');
        return;
    }

    if (strcmp(name, "LommelS") == 0 && f->a && f->a->ops == &ops_argument_list) {
        sbuf_puts(b, "LommelS(");
        emit_expr(f->a, b, PREC_LOWEST);
        sbuf_putc(b, ')');
        return;
    }

    if (strcmp(name, "Si") == 0 || strcmp(name, "Ci") == 0) {
        sbuf_puts(b, name);
        sbuf_putc(b, '(');
        emit_expr(f->a, b, PREC_LOWEST);
        sbuf_putc(b, ')');
        return;
    }

    while (prime_count < length && name[length - prime_count - 1u] == '\'')
        ++prime_count;
    if (prime_count > 0u && prime_count < length) {
        char *base_name = strndup(name, length - prime_count);

        if (base_name) {
            emit_name(b, base_name);
            free(base_name);
            for (size_t i = 0u; i < prime_count; ++i)
                sbuf_putc(b, '\'');
        } else {
            emit_name(b, name);
        }
    } else {
        emit_name(b, name);
    }
    sbuf_putc(b, '(');
    emit_expr(f->a, b, PREC_LOWEST);
    sbuf_putc(b, ')');
}

void emit_argument_list_expr(const expr_t *f, sbuf_t *b)
{
    emit_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_expr(f->b, b, PREC_LOWEST);
}

void emit_formal_derivative_func(const expr_t *f, sbuf_t *b)
{
    /* Nest coordinate groups in differentiation order; repeated coordinates share one order. */
    for (size_t i = 0u; i < f->formal_wrt_count; ++i) {
        if (i == 0u || !expr_struct_eq(f->formal_wrts[i - 1u], f->formal_wrts[i]))
            sbuf_puts(b, "derivative(");
    }
    emit_func(f->a, b, PREC_LOWEST);
    for (size_t i = 0u; i < f->formal_wrt_count;) {
        size_t end = i + 1u;
        while (end < f->formal_wrt_count && expr_struct_eq(f->formal_wrts[i], f->formal_wrts[end]))
            ++end;
        sbuf_puts(b, ", ");
        emit_name_func(b, f->formal_wrts[i]->name ? f->formal_wrts[i]->name : "x");
        char order[32];
        snprintf(order, sizeof(order), ", %zu)", end - i);
        sbuf_puts(b, order);
        i = end;
    }
}

void emit_arbitrary_function_func(const expr_t *f, sbuf_t *b)
{
    const char *name = f->name ? f->name : "F";
    const char *function_name = name;
    size_t length;
    size_t prime_count = 0u;

    if (strcmp(name, "BesselJ") == 0)
        function_name = "besselj";
    else if (strcmp(name, "BesselY") == 0)
        function_name = "bessely";
    else if (strcmp(name, "LommelS") == 0)
        function_name = "lommels";

    length = strlen(function_name);
    while (prime_count < length && function_name[length - prime_count - 1u] == '\'')
        ++prime_count;
    if (prime_count > 0u && prime_count < length) {
        char *base_name = strndup(function_name, length - prime_count);

        if (base_name) {
            if (expr_tostring_is_safe_func_name(base_name))
                sbuf_puts(b, base_name);
            else
                emit_name_func(b, base_name);
            free(base_name);
            for (size_t i = 0u; i < prime_count; ++i)
                sbuf_putc(b, '\'');
        } else {
            emit_name_func(b, function_name);
        }
    } else if (expr_tostring_is_safe_func_name(function_name)) {
        sbuf_puts(b, function_name);
    } else {
        emit_name_func(b, function_name);
    }
    sbuf_putc(b, '(');
    emit_func(f->a, b, PREC_LOWEST);
    sbuf_putc(b, ')');
}

void emit_argument_list_func(const expr_t *f, sbuf_t *b)
{
    emit_func(f->a, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_func(f->b, b, PREC_LOWEST);
}

void emit_func_integral(const expr_t *f, sbuf_t *b)
{
    const expr_t *lower = expr_integral_lower_bound_expr(f);
    const expr_t *upper = expr_integral_upper_bound_expr(f);
    const expr_t *display_integrand = f ? f->a : NULL;
    const expr_t *display_dummy = expr_integral_dummy_expr(f);
    sbuf_puts(b, "integral(");
    emit_func(display_integrand, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_func(display_dummy, b, PREC_LOWEST);
    if (lower || !expr_struct_eq(upper, display_dummy)) {
        sbuf_puts(b, ", ");
        if (lower) {
            emit_func(lower, b, PREC_LOWEST);
            sbuf_puts(b, ", ");
        }
        emit_func(upper, b, PREC_LOWEST);
    }
    sbuf_putc(b, ')');
}
