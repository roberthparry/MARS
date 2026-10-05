/* Cartesian forms and analytic continuations. */

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

static bool expr_stringout_tree_has_symbol_name(const expr_t *expr, const char *name)
{
    if (!expr || !name)
        return false;
    if (expr->name && strcmp(expr->name, name) == 0)
        return true;
    return expr_stringout_tree_has_symbol_name(expr->a, name) || expr_stringout_tree_has_symbol_name(expr->b, name);
}

static void emit_TeX_log_cartesian_real(const expr_t *real, const expr_t *imaginary, sbuf_t *b)
{
    sbuf_puts(b, "\\frac{1}{2}\\ln\\left(");
    emit_TeX_expr(real, b, PREC_POW);
    sbuf_puts(b, "^{2}+");
    emit_TeX_expr(imaginary, b, PREC_POW);
    sbuf_puts(b, "^{2}\\right)");
}

static void emit_TeX_cartesian_norm_squared(const expr_t *real, const expr_t *imaginary, sbuf_t *b)
{
    emit_TeX_expr(real, b, PREC_POW);
    sbuf_puts(b, "^{2}+");
    emit_TeX_expr(imaginary, b, PREC_POW);
    sbuf_puts(b, "^{2}");
}

static void emit_TeX_log_cartesian_imaginary(const expr_t *real, const expr_t *imaginary, sbuf_t *b)
{
    sbuf_puts(b, "\\operatorname{atan2}(");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_putc(b, ',');
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_TeX_log_cartesian_norm_squared(const expr_t *real, const expr_t *imaginary, sbuf_t *b)
{
    sbuf_puts(b, "\\left(");
    emit_TeX_log_cartesian_real(real, imaginary, b);
    sbuf_puts(b, "\\right)^{2}+\\operatorname{atan2}^{2}(");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_putc(b, ',');
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_TeX_log_cartesian_argument(const expr_t *real, const expr_t *imaginary, sbuf_t *b)
{
    sbuf_puts(b, "\\operatorname{atan2}\\left(");
    emit_TeX_log_cartesian_imaginary(real, imaginary, b);
    sbuf_putc(b, ',');
    emit_TeX_log_cartesian_real(real, imaginary, b);
    sbuf_puts(b, "\\right)");
}

bool emit_TeX_logarithmic_integral_cartesian(const expr_t *f, sbuf_t *b)
{
    static const char *index_names[] = {"n", "m", "k", "l", "j"};
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    bool has_imaginary = false;
    const char *index = "j";

    if (!f || !expr_is_op(f, &ops_Li) ||
        !expr_cartesian_parts_for_display(f->a, &real, &imaginary, &has_imaginary) || !has_imaginary) {
        expr_free(imaginary);
        expr_free(real);
        return false;
    }
    for (size_t i = 0u; i < sizeof(index_names) / sizeof(index_names[0]); ++i) {
        if (!expr_stringout_tree_has_symbol_name(f, index_names[i])) {
            index = index_names[i];
            break;
        }
    }

    sbuf_puts(b, "\\begin{aligned}\n\\operatorname{Li}(");
    emit_TeX_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, ")&=p+q\\mkern-2mu i,\\\\\np&=\\gamma+\\frac{1}{2}\\ln\\left(");
    emit_TeX_log_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, "\\right)+\\sum_{");
    sbuf_puts(b, index);
    sbuf_puts(b, "=1}^{\\infty}\\frac{\\left(");
    emit_TeX_log_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, "\\right)^{");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2}\\cos\\left(");
    sbuf_puts(b, index);
    emit_TeX_log_cartesian_argument(real, imaginary, b);
    sbuf_puts(b, "\\right)}{");
    sbuf_puts(b, index);
    sbuf_puts(b, "\\,");
    sbuf_puts(b, index);
    sbuf_puts(b, "!},\\\\\nq&=");
    emit_TeX_log_cartesian_argument(real, imaginary, b);
    sbuf_puts(b, "+\\sum_{");
    sbuf_puts(b, index);
    sbuf_puts(b, "=1}^{\\infty}\\frac{\\left(");
    emit_TeX_log_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, "\\right)^{");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2}\\sin\\left(");
    sbuf_puts(b, index);
    emit_TeX_log_cartesian_argument(real, imaginary, b);
    sbuf_puts(b, "\\right)}{");
    sbuf_puts(b, index);
    sbuf_puts(b, "\\,");
    sbuf_puts(b, index);
    sbuf_puts(b, "!}\\end{aligned}");

    expr_free(imaginary);
    expr_free(real);
    return true;
}

bool emit_TeX_exponential_integral_cartesian(const expr_t *f, sbuf_t *b)
{
    static const char *index_names[] = {"n", "m", "k", "l", "j"};
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    bool has_imaginary = false;
    const char *index = "j";

    if (!f || !expr_is_op(f, &ops_Ei) ||
        !expr_cartesian_parts_for_display(f->a, &real, &imaginary, &has_imaginary) || !has_imaginary) {
        expr_free(imaginary);
        expr_free(real);
        return false;
    }
    for (size_t i = 0u; i < sizeof(index_names) / sizeof(index_names[0]); ++i) {
        if (!expr_stringout_tree_has_symbol_name(f, index_names[i])) {
            index = index_names[i];
            break;
        }
    }

    sbuf_puts(b, "\\begin{aligned}\n\\operatorname{Ei}(");
    emit_TeX_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, ")&=p+q\\mkern-2mu i,\\\\\np&=\\gamma+\\frac{1}{2}\\ln\\left(");
    emit_TeX_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, "\\right)+\\sum_{");
    sbuf_puts(b, index);
    sbuf_puts(b, "=1}^{\\infty}\\frac{\\left(");
    emit_TeX_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, "\\right)^{");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2}\\cos\\left(");
    sbuf_puts(b, index);
    sbuf_puts(b, "\\operatorname{atan2}(");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ",");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ")\\right)}{");
    sbuf_puts(b, index);
    sbuf_puts(b, "\\,");
    sbuf_puts(b, index);
    sbuf_puts(b, "!},\\\\\nq&=\\operatorname{atan2}(");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ",");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ")+\\sum_{");
    sbuf_puts(b, index);
    sbuf_puts(b, "=1}^{\\infty}\\frac{\\left(");
    emit_TeX_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, "\\right)^{");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2}\\sin\\left(");
    sbuf_puts(b, index);
    sbuf_puts(b, "\\operatorname{atan2}(");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ",");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ")\\right)}{");
    sbuf_puts(b, index);
    sbuf_puts(b, "\\,");
    sbuf_puts(b, index);
    sbuf_puts(b, "!}\\end{aligned}");

    expr_free(imaginary);
    expr_free(real);
    return true;
}

static bool expr_log_cartesian_parts_for_output(const expr_t *argument, expr_t **real_out, expr_t **imaginary_out);

static void emit_expr_cartesian_norm_squared(const expr_t *real, const expr_t *imaginary, sbuf_t *b)
{
    emit_expr(real, b, PREC_POW);
    sbuf_puts(b, "² + ");
    emit_expr(imaginary, b, PREC_POW);
    sbuf_puts(b, "²");
}

static bool emit_expr_exponential_integral_cartesian(const expr_t *f, sbuf_t *b, int parent_prec)
{
    static const char *index_names[] = {"n", "m", "k", "l", "j"};
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    bool has_imaginary = false;
    const char *index = "j";
    bool parenthesise = PREC_ADD < parent_prec;

    if (!f || (!expr_is_op(f, &ops_Ei) && !expr_is_op(f, &ops_Li)) ||
        !(expr_is_op(f, &ops_Li) ? expr_log_cartesian_parts_for_output(f->a, &real, &imaginary)
                                 : expr_cartesian_parts_for_display(f->a, &real, &imaginary, &has_imaginary)) ||
        (expr_is_op(f, &ops_Ei) && !has_imaginary)) {
        expr_free(imaginary);
        expr_free(real);
        return false;
    }
    for (size_t i = 0u; i < sizeof(index_names) / sizeof(index_names[0]); ++i) {
        if (!expr_stringout_tree_has_symbol_name(f, index_names[i])) {
            index = index_names[i];
            break;
        }
    }
    if (parenthesise)
        sbuf_putc(b, '(');
    sbuf_puts(b, "γ + ½ln(");
    emit_expr_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, ") + Σ_(");
    sbuf_puts(b, index);
    sbuf_puts(b, "=1)^∞ (");
    emit_expr_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, ")^(");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2)cos(");
    sbuf_puts(b, index);
    sbuf_puts(b, "·atan2(");
    emit_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, "))/(");
    sbuf_puts(b, index);
    sbuf_puts(b, "·");
    sbuf_puts(b, index);
    sbuf_puts(b, "!) + (atan2(");
    emit_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ") + Σ_(");
    sbuf_puts(b, index);
    sbuf_puts(b, "=1)^∞ (");
    emit_expr_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, ")^(");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2)sin(");
    sbuf_puts(b, index);
    sbuf_puts(b, "·atan2(");
    emit_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, "))/(");
    sbuf_puts(b, index);
    sbuf_puts(b, "·");
    sbuf_puts(b, index);
    sbuf_puts(b, "!))i");
    if (parenthesise)
        sbuf_putc(b, ')');
    expr_free(imaginary);
    expr_free(real);
    return true;
}

static bool emit_func_exponential_integral_cartesian(const expr_t *f, sbuf_t *b, int parent_prec)
{
    static const char *index_names[] = {"n", "m", "k", "l", "j"};
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    bool has_imaginary = false;
    const char *index = "j";
    bool parenthesise = PREC_ADD < parent_prec;

    if (!f || (!expr_is_op(f, &ops_Ei) && !expr_is_op(f, &ops_Li)) ||
        !(expr_is_op(f, &ops_Li) ? expr_log_cartesian_parts_for_output(f->a, &real, &imaginary)
                                 : expr_cartesian_parts_for_display(f->a, &real, &imaginary, &has_imaginary)) ||
        (expr_is_op(f, &ops_Ei) && !has_imaginary)) {
        expr_free(imaginary);
        expr_free(real);
        return false;
    }
    for (size_t i = 0u; i < sizeof(index_names) / sizeof(index_names[0]); ++i) {
        if (!expr_stringout_tree_has_symbol_name(f, index_names[i])) {
            index = index_names[i];
            break;
        }
    }
    if (parenthesise)
        sbuf_putc(b, '(');
    sbuf_puts(b, "@eulermascheroni + ln(");
    emit_func(real, b, PREC_LOWEST);
    sbuf_puts(b, "^2 + ");
    emit_func(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, "^2)/2 + sum(");
    sbuf_puts(b, index);
    sbuf_puts(b, ", 1, @inf, (");
    emit_func(real, b, PREC_LOWEST);
    sbuf_puts(b, "^2 + ");
    emit_func(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, "^2)^(");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2).cos(");
    sbuf_puts(b, index);
    sbuf_puts(b, ".atan2(");
    emit_func(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_func(real, b, PREC_LOWEST);
    sbuf_puts(b, "))/(");
    sbuf_puts(b, index);
    sbuf_puts(b, ".factorial(");
    sbuf_puts(b, index);
    sbuf_puts(b, "))) + (atan2(");
    emit_func(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_func(real, b, PREC_LOWEST);
    sbuf_puts(b, ") + sum(");
    sbuf_puts(b, index);
    sbuf_puts(b, ", 1, @inf, (");
    emit_func(real, b, PREC_LOWEST);
    sbuf_puts(b, "^2 + ");
    emit_func(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, "^2)^(");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2).sin(");
    sbuf_puts(b, index);
    sbuf_puts(b, ".atan2(");
    emit_func(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_func(real, b, PREC_LOWEST);
    sbuf_puts(b, "))/(");
    sbuf_puts(b, index);
    sbuf_puts(b, ".factorial(");
    sbuf_puts(b, index);
    sbuf_puts(b, ")))).i");
    if (parenthesise)
        sbuf_putc(b, ')');
    expr_free(imaginary);
    expr_free(real);
    return true;
}

static expr_t *expr_log_cartesian_for_output(const expr_t *argument)
{
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    expr_t *real_squared = NULL;
    expr_t *imaginary_squared = NULL;
    expr_t *norm_squared = NULL;
    expr_t *norm_log = NULL;
    expr_t *half = NULL;
    expr_t *real_part = NULL;
    expr_t *imaginary_part = NULL;
    expr_t *imaginary_unit = NULL;
    expr_t *imaginary_term = NULL;
    expr_t *result = NULL;
    bool has_imaginary = false;

    if (!expr_cartesian_parts_for_display(argument, &real, &imaginary, &has_imaginary) || !has_imaginary)
        goto cleanup;
    real_squared = expr_pow(real, &NUM_TWO);
    imaginary_squared = expr_pow(imaginary, &NUM_TWO);
    norm_squared = real_squared && imaginary_squared ? expr_add(real_squared, imaginary_squared) : NULL;
    norm_log = norm_squared ? expr_log(norm_squared) : NULL;
    half = expr_new_const(NUM_HALF);
    real_part = half && norm_log ? expr_mul(half, norm_log) : NULL;
    imaginary_part = expr_atan2(imaginary, real);
    imaginary_unit = expr_new_named_const(NUM_I, "i");
    imaginary_term = imaginary_part && imaginary_unit ? expr_mul(imaginary_part, imaginary_unit) : NULL;
    result = real_part && imaginary_term ? expr_add(real_part, imaginary_term) : NULL;

cleanup:
    expr_free(imaginary_term);
    expr_free(imaginary_unit);
    expr_free(imaginary_part);
    expr_free(real_part);
    expr_free(half);
    expr_free(norm_log);
    expr_free(norm_squared);
    expr_free(imaginary_squared);
    expr_free(real_squared);
    expr_free(imaginary);
    expr_free(real);
    return result;
}

static bool expr_log_cartesian_parts_for_output(const expr_t *argument, expr_t **real_out, expr_t **imaginary_out)
{
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    expr_t *real_squared = NULL;
    expr_t *imaginary_squared = NULL;
    expr_t *norm_squared = NULL;
    expr_t *norm_log = NULL;
    expr_t *half = NULL;
    bool has_imaginary = false;
    bool ok = false;

    if (!real_out || !imaginary_out ||
        !expr_cartesian_parts_for_display(argument, &real, &imaginary, &has_imaginary) || !has_imaginary)
        goto cleanup;
    real_squared = expr_pow(real, &NUM_TWO);
    imaginary_squared = expr_pow(imaginary, &NUM_TWO);
    norm_squared = real_squared && imaginary_squared ? expr_add(real_squared, imaginary_squared) : NULL;
    norm_log = norm_squared ? expr_log(norm_squared) : NULL;
    half = expr_new_const(NUM_HALF);
    *real_out = half && norm_log ? expr_mul(half, norm_log) : NULL;
    *imaginary_out = expr_atan2(imaginary, real);
    ok = *real_out && *imaginary_out;

cleanup:
    if (!ok) {
        expr_free(*imaginary_out);
        expr_free(*real_out);
        *imaginary_out = NULL;
        *real_out = NULL;
    }
    expr_free(half);
    expr_free(norm_log);
    expr_free(norm_squared);
    expr_free(imaginary_squared);
    expr_free(real_squared);
    expr_free(imaginary);
    expr_free(real);
    return ok;
}

bool emit_expr_integral_cartesian(const expr_t *f, sbuf_t *b, int parent_prec)
{
    expr_t *cartesian_logarithm;
    expr_t *exponential_integral;
    bool emitted;

    if (emit_expr_exponential_integral_cartesian(f, b, parent_prec))
        return true;
    if (!f || !expr_is_op(f, &ops_Li) || !f->a)
        return false;
    cartesian_logarithm = expr_log_cartesian_for_output(f->a);
    exponential_integral = cartesian_logarithm ? expr_Ei(cartesian_logarithm) : NULL;
    emitted = exponential_integral &&
              emit_expr_exponential_integral_cartesian(exponential_integral, b, parent_prec);
    expr_free(exponential_integral);
    expr_free(cartesian_logarithm);
    return emitted;
}

bool emit_func_integral_cartesian(const expr_t *f, sbuf_t *b, int parent_prec)
{
    expr_t *cartesian_logarithm;
    expr_t *exponential_integral;
    bool emitted;

    if (emit_func_exponential_integral_cartesian(f, b, parent_prec))
        return true;
    if (!f || !expr_is_op(f, &ops_Li) || !f->a)
        return false;
    cartesian_logarithm = expr_log_cartesian_for_output(f->a);
    exponential_integral = cartesian_logarithm ? expr_Ei(cartesian_logarithm) : NULL;
    emitted = exponential_integral &&
              emit_func_exponential_integral_cartesian(exponential_integral, b, parent_prec);
    expr_free(exponential_integral);
    expr_free(cartesian_logarithm);
    return emitted;
}

bool emit_func_integral_cartesian_body(const expr_t *f, sbuf_t *b)
{
    static const char *index_names[] = {"n", "m", "k", "l", "j"};
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    bool has_imaginary = false;
    bool logarithmic_integral;
    bool cache_real_part;
    const char *index = "j";
    const char *radius;
    const char *angle;
    const char *log_real;
    const char *log_radius;
    const char *log_angle;
    const char *series_term;
    const char *series_angle;

    if (!f || (!expr_is_op(f, &ops_Ei) && !expr_is_op(f, &ops_Li)) ||
        !expr_cartesian_parts_for_display(f->a, &real, &imaginary, &has_imaginary) || !has_imaginary) {
        expr_free(imaginary);
        expr_free(real);
        return false;
    }
    for (size_t i = 0u; i < sizeof(index_names) / sizeof(index_names[0]); ++i) {
        if (!expr_stringout_tree_has_symbol_name(f, index_names[i])) {
            index = index_names[i];
            break;
        }
    }
    logarithmic_integral = expr_is_op(f, &ops_Li);
    cache_real_part = !expr_is_var(real);
    if (cache_real_part) {
        sbuf_puts(b, "    v1 = ");
        emit_func(real, b, PREC_LOWEST);
        sbuf_puts(b, ".\n");
    }
    radius = cache_real_part ? "v2" : "v1";
    angle = cache_real_part ? "v3" : "v2";
    log_real = cache_real_part ? "v4" : "v3";
    log_radius = cache_real_part ? "v5" : "v4";
    log_angle = cache_real_part ? "v6" : "v5";
    series_term = cache_real_part ? (logarithmic_integral ? "v7" : "v4")
                                         : (logarithmic_integral ? "v6" : "v3");
    series_angle = cache_real_part ? (logarithmic_integral ? "v8" : "v5")
                                          : (logarithmic_integral ? "v7" : "v4");
    sbuf_puts(b, "    ");
    sbuf_puts(b, radius);
    sbuf_puts(b, " = ");
    if (cache_real_part)
        sbuf_puts(b, "v1");
    else
        emit_func(real, b, PREC_POW);
    sbuf_puts(b, "^2 + ");
    emit_func(imaginary, b, PREC_POW);
    sbuf_puts(b, "^2.\n    ");
    sbuf_puts(b, angle);
    sbuf_puts(b, " = atan2(");
    emit_func(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    if (cache_real_part)
        sbuf_puts(b, "v1");
    else
        emit_func(real, b, PREC_LOWEST);
    sbuf_puts(b, ").\n");
    if (logarithmic_integral) {
        sbuf_puts(b, "    ");
        sbuf_puts(b, log_real);
        sbuf_puts(b, " = ln(");
        sbuf_puts(b, radius);
        sbuf_puts(b, ")/2.\n    ");
        sbuf_puts(b, log_radius);
        sbuf_puts(b, " = ");
        sbuf_puts(b, log_real);
        sbuf_puts(b, "^2 + ");
        sbuf_puts(b, angle);
        sbuf_puts(b, "^2.\n    ");
        sbuf_puts(b, log_angle);
        sbuf_puts(b, " = atan2(");
        sbuf_puts(b, angle);
        sbuf_puts(b, ", ");
        sbuf_puts(b, log_real);
        sbuf_puts(b, ").\n");
    }
    sbuf_puts(b, "    ");
    sbuf_puts(b, series_term);
    sbuf_puts(b, " = ");
    sbuf_puts(b, logarithmic_integral ? log_radius : radius);
    sbuf_puts(b, "^(");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2)/(");
    sbuf_puts(b, index);
    sbuf_puts(b, ".factorial(");
    sbuf_puts(b, index);
    sbuf_puts(b, ")).\n");
    sbuf_puts(b, "    ");
    sbuf_puts(b, series_angle);
    sbuf_puts(b, " = ");
    sbuf_puts(b, index);
    sbuf_putc(b, '.');
    sbuf_puts(b, logarithmic_integral ? log_angle : angle);
    sbuf_puts(b, ".\n");
    sbuf_puts(b, "    p = @eulermascheroni + ln(");
    sbuf_puts(b, logarithmic_integral ? log_radius : radius);
    sbuf_puts(b, ")/2 + sum(");
    sbuf_puts(b, index);
    sbuf_puts(b, ", 1, @inf, ");
    sbuf_puts(b, series_term);
    sbuf_puts(b, ".cos(");
    sbuf_puts(b, series_angle);
    sbuf_puts(b, ")).\n    q = ");
    sbuf_puts(b, logarithmic_integral ? log_angle : angle);
    sbuf_puts(b, " + sum(");
    sbuf_puts(b, index);
    sbuf_puts(b, ", 1, @inf, ");
    sbuf_puts(b, series_term);
    sbuf_puts(b, ".sin(");
    sbuf_puts(b, series_angle);
    sbuf_puts(b, ")).\n\n    return p + q.i.\n");
    expr_free(imaginary);
    expr_free(real);
    return true;
}

static bool expr_kind_has_analytic_cartesian_series(expr_op_kind_t kind)
{
    static const bool supported[EXPR_KIND_COUNT] = {
        [EXPR_KIND_SQRT]              = true,
        [EXPR_KIND_ERF]               = true,
        [EXPR_KIND_ERFC]              = true,
        [EXPR_KIND_LGAMMA]            = true,
        [EXPR_KIND_ERFINV]            = true,
        [EXPR_KIND_ERFCINV]           = true,
        [EXPR_KIND_GAMMA]             = true,
        [EXPR_KIND_DIGAMMA]           = true,
        [EXPR_KIND_TRIGAMMA]          = true,
        [EXPR_KIND_ZETA]              = true,
        [EXPR_KIND_GAMMAINV]          = true,
        [EXPR_KIND_LAMBERT_W0]        = true,
        [EXPR_KIND_LAMBERT_WM1]       = true,
        [EXPR_KIND_NORMAL_PDF]        = true,
        [EXPR_KIND_NORMAL_CDF]        = true,
        [EXPR_KIND_NORMAL_LOGPDF]     = true,
        [EXPR_KIND_E1]                = true,
        [EXPR_KIND_DILOG]             = true,
        [EXPR_KIND_POLYLOG1]          = true,
        [EXPR_KIND_VERSIN]            = true,
        [EXPR_KIND_VERCOS]            = true,
        [EXPR_KIND_COVERSIN]          = true,
        [EXPR_KIND_COVERCOS]          = true,
        [EXPR_KIND_HAVERSIN]          = true,
        [EXPR_KIND_HAVERCOS]          = true,
        [EXPR_KIND_HACOVERSIN]        = true,
        [EXPR_KIND_HACOVERCOS]        = true,
        [EXPR_KIND_ARCVERSIN]         = true,
        [EXPR_KIND_ARCVERCOS]         = true,
        [EXPR_KIND_ARCCOVERSIN]       = true,
        [EXPR_KIND_ARCCOVERCOS]       = true,
        [EXPR_KIND_ARCHAVERSIN]       = true,
        [EXPR_KIND_ARCHAVERCOS]       = true,
        [EXPR_KIND_ARCHACOVERSIN]     = true,
        [EXPR_KIND_ARCHACOVERCOS]     = true,
    };

    return kind >= 0 && kind < EXPR_KIND_COUNT && supported[kind];
}

bool emit_TeX_cube_root_cartesian(const expr_t *f, sbuf_t *b)
{
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    bool has_imaginary = false;

    if (!f || !expr_is_op(f, &ops_cubrt) ||
        !expr_cartesian_parts_for_display(f->a, &real, &imaginary, &has_imaginary) || !has_imaginary) {
        expr_free(imaginary);
        expr_free(real);
        return false;
    }

    sbuf_puts(b, "\\begin{aligned}\n\\sqrt[3]{");
    emit_TeX_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, "}&=p+q\\mkern-2mu i,\\\\\np&=\\left(");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, "^{2}+");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, "^{2}\\right)^{1/6}\\cos\\left(\\frac{1}{3}\\operatorname{atan2}(");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ",");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ")\\right),\\\\\nq&=\\left(");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, "^{2}+");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, "^{2}\\right)^{1/6}\\sin\\left(\\frac{1}{3}\\operatorname{atan2}(");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ",");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ")\\right)\\end{aligned}");

    expr_free(imaginary);
    expr_free(real);
    return true;
}

bool emit_TeX_analytic_unary_cartesian(const expr_t *f, sbuf_t *b)
{
    static const char *index_names[] = {"n", "m", "k", "l", "j"};
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    bool has_imaginary = false;
    const char *index = "j";
    const char *name;

    if (!f || !f->ops || f->ops->arity != EXPR_OP_UNARY ||
        !expr_kind_has_analytic_cartesian_series(f->ops->kind) ||
        !expr_cartesian_parts_for_display(f->a, &real, &imaginary, &has_imaginary) || !has_imaginary) {
        expr_free(imaginary);
        expr_free(real);
        return false;
    }
    for (size_t i = 0u; i < sizeof(index_names) / sizeof(index_names[0]); ++i) {
        if (!expr_stringout_tree_has_symbol_name(f, index_names[i])) {
            index = index_names[i];
            break;
        }
    }
    name = f->ops->TeX_name ? f->ops->TeX_name : "\\operatorname{f}";

    sbuf_puts(b, "\\begin{aligned}\n");
    sbuf_puts(b, name);
    sbuf_putc(b, '(');
    emit_TeX_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, ")&=p+q\\mkern-2mu i,\\\\\np&=\\sum_{");
    sbuf_puts(b, index);
    sbuf_puts(b, "=0}^{\\infty}\\frac{(-1)^{");
    sbuf_puts(b, index);
    sbuf_puts(b, "}");
    emit_TeX_expr(imaginary, b, PREC_POW);
    sbuf_puts(b, "^{2");
    sbuf_puts(b, index);
    sbuf_puts(b, "}}{(2");
    sbuf_puts(b, index);
    sbuf_puts(b, ")!}");
    sbuf_puts(b, "\\left(");
    sbuf_puts(b, name);
    sbuf_puts(b, "\\right)^{(2");
    sbuf_puts(b, index);
    sbuf_puts(b, ")}(");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, "),\\\\\nq&=\\sum_{");
    sbuf_puts(b, index);
    sbuf_puts(b, "=0}^{\\infty}\\frac{(-1)^{");
    sbuf_puts(b, index);
    sbuf_puts(b, "}");
    emit_TeX_expr(imaginary, b, PREC_POW);
    sbuf_puts(b, "^{2");
    sbuf_puts(b, index);
    sbuf_puts(b, "+1}}{(2");
    sbuf_puts(b, index);
    sbuf_puts(b, "+1)!}");
    sbuf_puts(b, "\\left(");
    sbuf_puts(b, name);
    sbuf_puts(b, "\\right)^{(2");
    sbuf_puts(b, index);
    sbuf_puts(b, "+1)}(");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ")\\end{aligned}");

    expr_free(imaginary);
    expr_free(real);
    return true;
}
