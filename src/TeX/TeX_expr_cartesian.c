/**
 * @file TeX_expr_cartesian.c
 * @brief Cartesian displays of complex elementary and special functions.
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

#define MARS_SHARED_EXPR_RENDER_INTERNAL_ACCESS
#include "internal/expr_render_internal.h"
#define MARS_TeX_SEMANTIC_SYMBOLS_INTERNAL_ACCESS
#include "TeX_semantic_symbols.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "internal/TeX_internal.h"

static void emit_TeX_log_cartesian_real(const expr_t *real, const expr_t *imaginary, sbuf_t *b)
{
    sbuf_puts(b, "\\frac{1}{2}\\ln\\left(");
    TeX_emit_expr(real, b, PREC_POW);
    sbuf_puts(b, "^{2}+");
    TeX_emit_expr(imaginary, b, PREC_POW);
    sbuf_puts(b, "^{2}\\right)");
}

static void emit_TeX_cartesian_norm_squared(const expr_t *real, const expr_t *imaginary, sbuf_t *b)
{
    TeX_emit_expr(real, b, PREC_POW);
    sbuf_puts(b, "^{2}+");
    TeX_emit_expr(imaginary, b, PREC_POW);
    sbuf_puts(b, "^{2}");
}

static void emit_TeX_log_cartesian_imaginary(const expr_t *real, const expr_t *imaginary, sbuf_t *b)
{
    sbuf_puts(b, "\\operatorname{atan2}(");
    TeX_emit_expr(imaginary, b, PREC_LOWEST);
    sbuf_putc(b, ',');
    TeX_emit_expr(real, b, PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_TeX_log_cartesian_norm_squared(const expr_t *real, const expr_t *imaginary, sbuf_t *b)
{
    sbuf_puts(b, "\\left(");
    emit_TeX_log_cartesian_real(real, imaginary, b);
    sbuf_puts(b, "\\right)^{2}+\\operatorname{atan2}^{2}(");
    TeX_emit_expr(imaginary, b, PREC_LOWEST);
    sbuf_putc(b, ',');
    TeX_emit_expr(real, b, PREC_LOWEST);
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
    TeX_emit_expr(f->a, b, PREC_LOWEST);
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
    TeX_emit_expr(f->a, b, PREC_LOWEST);
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
    TeX_emit_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ",");
    TeX_emit_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ")\\right)}{");
    sbuf_puts(b, index);
    sbuf_puts(b, "\\,");
    sbuf_puts(b, index);
    sbuf_puts(b, "!},\\\\\nq&=\\operatorname{atan2}(");
    TeX_emit_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ",");
    TeX_emit_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ")+\\sum_{");
    sbuf_puts(b, index);
    sbuf_puts(b, "=1}^{\\infty}\\frac{\\left(");
    emit_TeX_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, "\\right)^{");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2}\\sin\\left(");
    sbuf_puts(b, index);
    sbuf_puts(b, "\\operatorname{atan2}(");
    TeX_emit_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ",");
    TeX_emit_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ")\\right)}{");
    sbuf_puts(b, index);
    sbuf_puts(b, "\\,");
    sbuf_puts(b, index);
    sbuf_puts(b, "!}\\end{aligned}");

    expr_free(imaginary);
    expr_free(real);
    return true;
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
    TeX_emit_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, "}&=p+q\\mkern-2mu i,\\\\\np&=\\left(");
    TeX_emit_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, "^{2}+");
    TeX_emit_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, "^{2}\\right)^{1/6}\\cos\\left(\\frac{1}{3}\\operatorname{atan2}(");
    TeX_emit_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ",");
    TeX_emit_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ")\\right),\\\\\nq&=\\left(");
    TeX_emit_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, "^{2}+");
    TeX_emit_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, "^{2}\\right)^{1/6}\\sin\\left(\\frac{1}{3}\\operatorname{atan2}(");
    TeX_emit_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ",");
    TeX_emit_expr(real, b, PREC_LOWEST);
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
    name = TeX_operation_name(f->ops->function_name);
    if (!name)
        name = "\\operatorname{f}";

    sbuf_puts(b, "\\begin{aligned}\n");
    sbuf_puts(b, name);
    sbuf_putc(b, '(');
    TeX_emit_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, ")&=p+q\\mkern-2mu i,\\\\\np&=\\sum_{");
    sbuf_puts(b, index);
    sbuf_puts(b, "=0}^{\\infty}\\frac{(-1)^{");
    sbuf_puts(b, index);
    sbuf_puts(b, "}");
    TeX_emit_expr(imaginary, b, PREC_POW);
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
    TeX_emit_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, "),\\\\\nq&=\\sum_{");
    sbuf_puts(b, index);
    sbuf_puts(b, "=0}^{\\infty}\\frac{(-1)^{");
    sbuf_puts(b, index);
    sbuf_puts(b, "}");
    TeX_emit_expr(imaginary, b, PREC_POW);
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
    TeX_emit_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ")\\end{aligned}");

    expr_free(imaginary);
    expr_free(real);
    return true;
}
