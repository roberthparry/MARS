/* Recursive TeX body rendering and source-order scopes. */

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

static _Thread_local unsigned TeX_source_order_depth;

bool expr_TeX_source_order_preserved(void)
{
    return TeX_source_order_depth != 0u;
}

static int TeX_exp_needs_parens(const expr_t *e)
{
    if (!e)
        return 0;
    if (e->ops->arity == EXPR_OP_ATOM)
        return 0;
    if (expr_is_neg(e) || expr_is_pow_d_expr(e))
        return 1;
    if (e->ops->arity == EXPR_OP_UNARY)
        return 0;
    if (expr_is_addsub(e) || expr_is_mul(e) || expr_is_op(e, &ops_div) || expr_is_op(e, &ops_pow))
        return 1;
    return 0;
}

static void emit_TeX_factor_abs(const expr_t *f, sbuf_t *b)
{
    if (expr_is_negative(f))
        emit_TeX_expr_abs(f, b, PREC_MUL);
    else
        emit_TeX_expr(f, b, PREC_MUL);
}

static bool TeX_contains_calculus(const expr_t *f);

/* Keep a descriptive identifier visually separate from neighbouring factors. */
static bool TeX_factor_has_compound_name(const expr_t *factor)
{
    while (factor && (expr_is_neg(factor) || expr_is_op(factor, &ops_pow) || expr_is_pow_d_expr(factor)))
        factor = factor->a;
    return factor && (expr_is_var(factor) || expr_is_const(factor)) && factor->name && *factor->name &&
           !expr_tostring_should_emit_binding_expr(factor) && !expr_tostring_is_simple_name(factor->name);
}

/* Use the same factor separation in ordinary products, fractions and multiline output. */
void emit_TeX_mul_separator(const expr_t *left, const expr_t *right, sbuf_t *b)
{
    const expr_t *right_power_base = NULL;

    if (expr_is_op(right, &ops_hypergeometric_pFq)) {
        sbuf_puts(b, " \\cdot ");
        return;
    }

    if (TeX_contains_calculus(left) && TeX_contains_calculus(right)) {
        sbuf_puts(b, " \\cdot ");
        return;
    }

    if (right && (expr_is_op(right, &ops_pow) || expr_is_pow_d_expr(right)))
        right_power_base = right->a;

    if (left && expr_is_const(left) && (!left->name || !*left->name) &&
        ((right && expr_is_const(right) && (!right->name || !*right->name) &&
          num_is_real(left->c) && num_is_integer(right->c) &&
          (!right->binding_expr || expr_binding_expr_is_numeric_literal(right->binding_expr))) ||
         (right_power_base && expr_is_const(right_power_base) &&
          (!right_power_base->name || !*right_power_base->name)) ||
         (right && expr_is_const(right) && right->binding_expr &&
          expr_binding_expr_needs_explicit_mul_separator(right->binding_expr)))) {
        sbuf_puts(b, " \\times ");
        return;
    }
    if (TeX_factor_has_compound_name(left) || TeX_factor_has_compound_name(right)) {
        sbuf_puts(b, " \\cdot ");
        return;
    }
    sbuf_puts(b, "\\mkern-2mu ");
}

bool expr_is_rendered_log_local(const expr_t *expr)
{
    return expr_is_op(expr, &ops_log) ||
           (expr && expr_is_const(expr) && expr->binding_expr &&
            expr->binding_expr->kind == EXPR_BINDING_EXPR_UNARY_OP &&
            expr->binding_expr->u.unary_op.ops == &ops_log);
}

/* Inspect only the expression tree being rendered; binding tables and derivative-coordinate metadata are not scanned. */
static bool TeX_contains_calculus(const expr_t *f)
{
    if (!f)
        return false;
    if (expr_is_op(f, &ops_integral) ||
        (expr_is_formal_derivative(f) &&
         (expr_TeX_partial_derivatives_enabled() || expr_TeX_total_derivatives_enabled())))
        return true;
    return TeX_contains_calculus(f->a) || TeX_contains_calculus(f->b);
}

/* Stable partition of the renderer's bounded factor array: coefficients precede derivatives and integrals. */
static void sort_TeX_factors(expr_t **factors, int count)
{
    expr_t *ordered[64];
    bool calculus[64];
    int next = 0;
    sort_factors(factors, count);
    for (int i = 0; i < count; ++i) {
        calculus[i] = TeX_contains_calculus(factors[i]);
        if (!calculus[i])
            ordered[next++] = factors[i];
    }
    for (int i = 0; i < count; ++i)
        if (calculus[i])
            ordered[next++] = factors[i];
    memcpy(factors, ordered, (size_t)count * sizeof(*factors));
}

/* Only multiplicative factors are traversed. Refuse oversized products rather than dropping any factors. */
static bool TeX_collect_signed_factors(const expr_t *f, const expr_t **factors, size_t *count, bool *negative)
{
    if (expr_is_neg(f)) {
        *negative = !*negative;
        return TeX_collect_signed_factors(f->a, factors, count, negative);
    }
    if (expr_is_mul(f))
        return TeX_collect_signed_factors(f->a, factors, count, negative) &&
               TeX_collect_signed_factors(f->b, factors, count, negative);
    if (*count == 64u)
        return false;
    *negative = *negative != expr_is_negative(f);
    factors[(*count)++] = f;
    return true;
}

/* Keep integrals and derivative fractions outside algebraic fractions; reciprocal exponentials use negative exponents. */
static bool emit_TeX_calculus_quotient(const expr_t *f, sbuf_t *b, int parent_prec, bool absolute)
{
    if (!expr_is_div(f))
        return false;
    const expr_t *factors[64];
    bool calculus[64];
    size_t count = 0u, calculus_count = 0u;
    bool negative = expr_is_negative(f->b);
    if (!TeX_collect_signed_factors(f->a, factors, &count, &negative))
        return false;
    for (size_t i = 0u; i < count; ++i) {
        calculus[i] = TeX_contains_calculus(factors[i]);
        calculus_count += calculus[i];
    }
    bool inverse_power = TeX_contains_calculus(f->b);
    if (calculus_count == 0u && !inverse_power)
        return false;
    const expr_t *denominator = expr_is_neg(f->b) ? f->b->a : f->b;
    bool inverse_exp = expr_is_op(denominator, &ops_exp);
    expr_t *negative_exponent = inverse_exp ? expr_negate_owned(expr_clone(denominator->a)) : NULL;
    if (inverse_exp && !negative_exponent)
        return false;
    bool grouped = PREC_MUL < parent_prec, emitted = false;
    bool fraction = !inverse_exp && !inverse_power;
    if (grouped)
        sbuf_puts(b, "\\left(");
    if (negative && !absolute)
        sbuf_putc(b, '-');
    if (fraction)
        sbuf_puts(b, "\\frac{");
    for (size_t i = 0u; i < count; ++i) {
        if (calculus[i])
            continue;
        if (!fraction && expr_is_const(factors[i]) && !factors[i]->name &&
            (num_eq(factors[i]->c, NUM_ONE) || num_eq(factors[i]->c, NUM_NEG_ONE)))
            continue;
        if (emitted)
            sbuf_puts(b, "\\,");
        emit_TeX_expr_abs(factors[i], b, PREC_MUL);
        emitted = true;
    }
    if (fraction) {
        if (!emitted)
            sbuf_putc(b, '1');
        sbuf_puts(b, "}{");
        emit_TeX_expr_abs(f->b, b, PREC_LOWEST);
        sbuf_putc(b, '}');
    } else {
        if (emitted)
            sbuf_puts(b, "\\,");
        if (inverse_exp) {
            sbuf_puts(b, "e^{");
            emit_TeX_expr(negative_exponent, b, PREC_LOWEST);
            sbuf_putc(b, '}');
        } else {
            sbuf_puts(b, "\\left(");
            emit_TeX_expr_abs(f->b, b, PREC_LOWEST);
            sbuf_puts(b, "\\right)^{-1}");
        }
    }
    expr_free(negative_exponent);
    for (size_t i = 0u; i < count; ++i) {
        if (!calculus[i])
            continue;
        sbuf_puts(b, "\\,");
        emit_TeX_expr_abs(factors[i], b, PREC_MUL);
    }
    if (grouped)
        sbuf_puts(b, "\\right)");
    return true;
}

/* A small prefactor keeps a long sum at the surrounding font size. */
static bool emit_TeX_large_sum_quotient(const expr_t *sum, const expr_t *denominator,
                                      long p, long q, sbuf_t *b, int parent_prec)
{
    if (!sum || !(expr_is_op(sum, &ops_add) || expr_is_op(sum, &ops_sub)))
        return false;
    sbuf_t numerator, divisor;
    sbuf_init(&numerator);
    sbuf_init(&divisor);
    emit_TeX_expr(sum, &numerator, PREC_LOWEST);
    if (q != 1L) {
        char text[32];
        snprintf(text, sizeof(text), "%ld", q);
        sbuf_puts(&divisor, text);
        emit_TeX_mul_separator(NULL, denominator, &divisor);
    }
    emit_TeX_expr(denominator, &divisor, q == 1L ? PREC_LOWEST : PREC_MUL);
    bool factored = sbuf_len(&numerator) >= 200u && sbuf_len(&divisor) <= 32u;
    if (factored) {
        if (PREC_MUL < parent_prec)
            sbuf_puts(b, "\\left(");
        char text[48];
        snprintf(text, sizeof(text), "\\frac{%ld}{", p);
        sbuf_puts(b, text);
        sbuf_put_string(b, divisor.text);
        sbuf_puts(b, "}\\,\\left[");
        sbuf_put_string(b, numerator.text);
        sbuf_puts(b, "\\right]");
        if (PREC_MUL < parent_prec)
            sbuf_puts(b, "\\right)");
    }
    sbuf_free(&divisor);
    sbuf_free(&numerator);
    return factored;
}

/* Put an exact rational coefficient's denominator into the surrounding fraction. */
static bool emit_TeX_rational_quotient(const expr_t *numerator, const expr_t *denominator,
                                     sbuf_t *b, bool absolute, int parent_prec)
{
    long p, q;
    const expr_t *factors[64];
    size_t count = 0u, coefficient_index = 0u;
    bool negative = false, found = false;
    if (!numerator || !TeX_collect_signed_factors(numerator, factors, &count, &negative))
        return false;
    /* The native product normaliser collects numeric factors into one coefficient. */
    for (size_t i = 0u; i < count; ++i) {
        if (expr_is_const(factors[i]) && !factors[i]->name &&
            num_get_small_rational(factors[i]->c, &p, &q) && q > 1L) {
            coefficient_index = i;
            found = true;
            break;
        }
    }
    if (!found)
        return false;
    if (count == 2u && p > 0 && !negative &&
        emit_TeX_large_sum_quotient(factors[1u - coefficient_index], denominator, p, q, b, parent_prec))
        return true;
    number_t coefficient = num_create_from_frac(p, 1L);
    if (p < 0L) {
        number_t positive = num_neg(coefficient);
        num_destroy(&coefficient);
        coefficient = positive;
    }
    if (PREC_MUL < parent_prec)
        sbuf_puts(b, "\\left(");
    if (negative && !absolute)
        sbuf_putc(b, '-');
    sbuf_puts(b, "\\frac{");
    bool emitted = !num_eq(coefficient, NUM_ONE) || count == 1u;
    if (emitted)
        emit_TeX_number_value(b, coefficient);
    num_destroy(&coefficient);
    const expr_t *previous_factor = NULL;
    for (size_t i = 0u; i < count; ++i) {
        if (i == coefficient_index)
            continue;
        if (emitted)
            emit_TeX_mul_separator(previous_factor, factors[i], b);
        emit_TeX_expr_abs(factors[i], b, PREC_MUL);
        previous_factor = factors[i];
        emitted = true;
    }
    sbuf_puts(b, "}{");
    char text[32];
    snprintf(text, sizeof(text), "%ld", q);
    sbuf_puts(b, text);
    emit_TeX_mul_separator(NULL, denominator, b);
    emit_TeX_expr(denominator, b, PREC_MUL);
    sbuf_putc(b, '}');
    if (PREC_MUL < parent_prec)
        sbuf_puts(b, "\\right)");
    return true;
}

void emit_TeX_expr_abs(const expr_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    if (expr_tostring_is_negative_const(f)) {
        if (emit_negative_const_binding_expr_abs(f, b, true))
            return;
        {
            number_t positive = num_neg(f->c);

            emit_TeX_number_value(b, positive);
            num_destroy(&positive);
        }
        return;
    }

    if (expr_is_const(f) && expr_renders_negative(f)) {
        if (emit_negative_const_binding_expr_abs(f, b, true))
            return;
        {
            number_t positive = num_neg(f->c);

            emit_TeX_number_value(b, positive);
            num_destroy(&positive);
        }
        return;
    }

    if (expr_is_neg(f)) {
        emit_TeX_expr(f->a, b, parent_prec);
        return;
    }

    if (expr_is_mul(f)) {
        expr_t *fac[64];
        int n = 0;

        flatten_mul((expr_t *)f, fac, &n, 64);
        sort_TeX_factors(fac, n);

        for (int i = 0; i < n; ++i) {
            if (expr_tostring_is_negative_const(fac[i]) && num_eq(fac[i]->c, NUM_NEG_ONE)) {
                for (int j = i; j < n - 1; ++j)
                    fac[j] = fac[j + 1];
                --n;
                break;
            }
        }

        for (int i = 0; i < n; ++i) {
            if (i > 0)
                emit_TeX_mul_separator(fac[i - 1], fac[i], b);
            if (n > 1 && mul_factor_needs_visible_parens(fac[i]))
                sbuf_puts(b, "\\left(");
            emit_TeX_factor_abs(fac[i], b);
            if (n > 1 && mul_factor_needs_visible_parens(fac[i]))
                sbuf_puts(b, "\\right)");
        }
        return;
    }

    if (expr_is_op(f, &ops_div) && expr_is_negative(f)) {
        if (emit_TeX_calculus_quotient(f, b, parent_prec, true))
            return;
        int need = PREC_MUL < parent_prec;
        if (need)
            sbuf_puts(b, "\\left(");
        if (expr_is_negative(f->a) && emit_TeX_rational_quotient(f->a, f->b, b, true, PREC_LOWEST)) {
            if (need)
                sbuf_puts(b, "\\right)");
            return;
        }
        sbuf_puts(b, "\\frac{");
        if (expr_is_negative(f->a))
            emit_TeX_expr_abs(f->a, b, PREC_LOWEST);
        else
            emit_TeX_expr(f->a, b, PREC_LOWEST);
        sbuf_puts(b, "}{");
        if (expr_is_negative(f->b))
            emit_TeX_expr_abs(f->b, b, PREC_LOWEST);
        else
            emit_TeX_expr(f->b, b, PREC_LOWEST);
        sbuf_putc(b, '}');
        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    emit_TeX_expr(f, b, parent_prec);
}

bool emit_expr_abs_needs_visible_add_parens(const expr_t *expr)
{
    const expr_t *constant = expr_is_neg(expr) ? expr->a : expr;

    return additive_const_needs_visible_parens(constant);
}

static bool emit_TeX_expr_abs_needs_visible_add_parens(const expr_t *expr)
{
    return emit_expr_abs_needs_visible_add_parens(expr);
}


static _Thread_local unsigned TeX_expression_depth;
static _Thread_local bool preserve_TeX_operations;
static _Thread_local const expr_t *TeX_shift_centre;
static void emit_TeX_expr_inner(const expr_t *f, sbuf_t *b, int parent_prec);

typedef struct TeX_index_domain {
    const expr_t *index;
    bool nonnegative_integer;
    const struct TeX_index_domain *outer;
} TeX_index_domain_t;

static _Thread_local const TeX_index_domain_t *TeX_index_domain;

static bool TeX_known_nonnegative_integer(const expr_t *expr)
{
    if (!expr)
        return false;
    /* The bounded scope stack also records unknown domains, so inner binders shadow outer ones. */
    for (const TeX_index_domain_t *scope = TeX_index_domain; scope; scope = scope->outer) {
        if (expr == scope->index || (expr->name && scope->index && scope->index->name &&
                                    strcmp(expr->name, scope->index->name) == 0))
            return scope->nonnegative_integer;
    }
    if (expr_is_const(expr) && !expr->name &&
        (!expr->binding_expr || expr_binding_expr_is_numeric_literal(expr->binding_expr)))
        return num_is_real(expr->c) && num_is_integer(expr->c) && num_ge(expr->c, NUM_ZERO);
    if (expr_is_op(expr, &ops_add) || expr_is_op(expr, &ops_mul))
        return TeX_known_nonnegative_integer(expr->a) && TeX_known_nonnegative_integer(expr->b);
    if (expr_is_op(expr, &ops_pow_d))
        return TeX_known_nonnegative_integer(expr->a) && num_is_real(expr->c) &&
               num_is_integer(expr->c) && num_ge(expr->c, NUM_ZERO);
    if (expr_is_op(expr, &ops_factorial))
        return TeX_known_nonnegative_integer(expr->a);
    return false;
}

static const expr_t *TeX_postfix_factorial_argument(const expr_t *expr)
{
    if (expr_is_op(expr, &ops_factorial))
        return TeX_known_nonnegative_integer(expr->a) ? expr->a : NULL;
    if (expr_is_op(expr, &ops_gamma) && expr_is_op(expr->a, &ops_add)) {
        const expr_t *left = expr->a->a;
        const expr_t *right = expr->a->b;

        if (expr_is_const(right) && !right->name && num_eq(right->c, NUM_ONE) &&
            TeX_known_nonnegative_integer(left))
            return left;
        if (expr_is_const(left) && !left->name && num_eq(left->c, NUM_ONE) &&
            TeX_known_nonnegative_integer(right))
            return right;
    }
    return NULL;
}

void emit_TeX_expr(const expr_t *f, sbuf_t *b, int parent_prec)
{
    const char *temporary = expr_math_temporary_name(f);
    if (temporary) {
        emit_TeX_name(b, temporary);
        return;
    }
    expr_cartesian_composition_t view;
    const bool outermost = TeX_expression_depth == 0u;
    expr_t *resolved = outermost && !preserve_TeX_operations && expr_is_integral_transform(f)
                           ? expr_transform_result(f) : NULL;
    if (resolved)
        f = resolved;
    expr_distribution_TeX_scope_t distribution;
    if (outermost)
        expr_distribution_TeX_begin(f, &distribution);
    const expr_t *saved_centre = TeX_shift_centre;
    const expr_t *centre = expr_display_symmetric_shift_centre(f);
    if (centre)
        TeX_shift_centre = centre;

    if (TeX_expression_depth++ == 0u && !preserve_TeX_operations && expr_cartesian_composition_init(f, &view)) {
        sbuf_puts(b, "\\begin{aligned}\n&");
        emit_TeX_expr(view.compact, b, PREC_LOWEST);
        sbuf_puts(b, ",\\\\\n");
        emit_TeX_expr(view.real_name, b, PREC_LOWEST);
        sbuf_puts(b, "&=");
        emit_TeX_expr(view.real_definition, b, PREC_LOWEST);
        sbuf_puts(b, ",\\\\\n");
        emit_TeX_expr(view.imaginary_name, b, PREC_LOWEST);
        sbuf_puts(b, "&=");
        emit_TeX_expr(view.imaginary_definition, b, PREC_LOWEST);
        sbuf_puts(b, "\\end{aligned}");
        expr_cartesian_composition_clear(&view);
    } else {
        emit_TeX_expr_inner(f, b, parent_prec);
    }
    if (outermost)
        expr_distribution_TeX_end(&distribution, b);
    --TeX_expression_depth;
    TeX_shift_centre = saved_centre;
    expr_free(resolved);
}

static void emit_TeX_expr_inner(const expr_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    if (expr_distribution_TeX_emit(f, b, parent_prec))
        return;

    const expr_t *shift = NULL;
    bool subtract = false;
    if (TeX_shift_centre && expr_display_centred_shift_parts(f, TeX_shift_centre, &shift, &subtract)) {
        if (parent_prec > PREC_ADD)
            sbuf_puts(b, "\\left(");
        emit_TeX_expr(TeX_shift_centre, b, PREC_ADD);
        sbuf_puts(b, subtract ? " - " : " + ");
        emit_TeX_expr(shift, b, PREC_MUL);
        if (parent_prec > PREC_ADD)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (expr_is_op(f, &ops_real_domain)) {
        emit_TeX_expr(f->a, b, parent_prec);
        sbuf_puts(b, "\\quad (");
        for (const expr_t *pair = f->b; pair; pair = pair->b->b) {
            if (pair != f->b)
                sbuf_puts(b, ",\\;");
            const expr_t *nonzero = expr_domain_nonzero_operand(pair);
            if (nonzero) {
                emit_TeX_expr(nonzero, b, PREC_LOWEST);
                sbuf_puts(b, "\\ne 0");
                continue;
            }
            if (expr_is_op(pair->a, &ops_nonnegative_integer) || expr_is_op(pair->a, &ops_real_parameter)) {
                emit_TeX_expr(pair->a->a, b, PREC_LOWEST);
                sbuf_puts(b, expr_is_op(pair->a, &ops_real_parameter) ? "\\in\\mathbb{R}" : "\\in\\mathbb{Z}_{\\ge0}");
                continue;
            }
            sbuf_puts(b, "\\operatorname{Re}(");
            emit_TeX_expr(pair->a, b, PREC_LOWEST);
            sbuf_puts(b, ")>");
            emit_TeX_expr(pair->b->a, b, PREC_LOWEST);
        }
        expr_distribution_TeX_conditions(f, b);
        sbuf_putc(b, ')');
        return;
    }
    if (expr_is_integral_transform(f)) {
        expr_t *result = preserve_TeX_operations ? NULL : expr_transform_result(f);
        if (result) {
            emit_TeX_expr(result, b, parent_prec);
        } else {
            sbuf_puts(b, f->ops->TeX_name);
            sbuf_puts(b, "_{");
            if (f->b->a->name)
                emit_TeX_name(b, f->b->a->name);
            else
                emit_TeX_expr(f->b->a, b, PREC_LOWEST);
            sbuf_puts(b, "\\to ");
            if (f->b->b->a->name)
                emit_TeX_name(b, f->b->b->a->name);
            else
                emit_TeX_expr(f->b->b->a, b, PREC_LOWEST);
            sbuf_puts(b, "}\\{");
            emit_TeX_expr(f->a, b, PREC_LOWEST);
            sbuf_puts(b, "\\}");
        }
        expr_free(result);
        return;
    }
    if (expr_is_formal_derivative(f)) {
        emit_formal_derivative_TeX(f, b);
        return;
    }
    if (expr_is_op(f, &ops_ordered_derivative)) {
        emit_ordered_derivative(f, b, 2);
        return;
    }
    if (expr_is_arbitrary_function(f)) {
        emit_arbitrary_function_TeX(f, b);
        return;
    }
    if (expr_is_op(f, &ops_argument_list)) {
        emit_argument_list_TeX(f, b);
        return;
    }

    if (expr_is_const(f) || expr_is_var(f)) {
        emit_TeX_atom(f, b);
        return;
    }

    if (expr_is_op(f, &ops_integral)) {
        emit_TeX_integral(f, b, parent_prec);
        return;
    }

    if (expr_is_neg(f)) {
        int need = PREC_UNARY < parent_prec;
        const expr_t *a = f->a;

        if (need)
            sbuf_puts(b, "\\left(");
        if (expr_is_neg(a)) {
            emit_TeX_expr(a->a, b, 0);
        } else if (expr_is_negative(a)) {
            emit_TeX_expr_abs(a, b, 0);
        } else {
            int child_needs_paren = expr_is_addsub(a);
            sbuf_putc(b, '-');
            if (child_needs_paren)
                sbuf_puts(b, "\\left(");
            emit_TeX_expr(a, b, 0);
            if (child_needs_paren)
                sbuf_puts(b, "\\right)");
        }
        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (f->ops->arity == EXPR_OP_UNARY) {
        int need = PREC_UNARY < parent_prec;
        const char *name = TeX_unary_name(f);
        const expr_t *factorial_argument = TeX_postfix_factorial_argument(f);

        if (need)
            sbuf_puts(b, "\\left(");

        if (TeX_expression_depth == 1u &&
            (emit_TeX_logarithmic_integral_cartesian(f, b) || emit_TeX_exponential_integral_cartesian(f, b) ||
             emit_TeX_cube_root_cartesian(f, b) || emit_TeX_analytic_unary_cartesian(f, b))) {
            /* The complete Cartesian identity has already been emitted. */
        } else if (expr_is_op(f, &ops_factorial) || factorial_argument) {
            if (!factorial_argument) {
                expr_t *argument = expr_add_long(f->a, 1L);

                sbuf_puts(b, "\\Gamma(");
                emit_TeX_expr(argument, b, PREC_LOWEST);
                sbuf_putc(b, ')');
                expr_free(argument);
                if (need)
                    sbuf_puts(b, "\\right)");
                return;
            }
            bool group_argument = factorial_argument->ops->arity != EXPR_OP_ATOM;

            if (group_argument)
                sbuf_puts(b, "\\left(");
            emit_TeX_expr(factorial_argument, b, PREC_LOWEST);
            if (group_argument)
                sbuf_puts(b, "\\right)");
            sbuf_putc(b, '!');
        } else if (expr_is_op(f, &ops_abs)) {
            sbuf_puts(b, "\\left|");
            emit_TeX_expr_abs(f->a, b, 0);
            sbuf_puts(b, "\\right|");
        } else if (expr_is_op(f, &ops_floor)) {
            sbuf_puts(b, "\\left\\lfloor ");
            emit_TeX_expr(f->a, b, 0);
            sbuf_puts(b, " \\right\\rfloor");
        } else if (expr_is_op(f, &ops_ceil)) {
            sbuf_puts(b, "\\left\\lceil ");
            emit_TeX_expr(f->a, b, 0);
            sbuf_puts(b, " \\right\\rceil");
        } else if (expr_is_sqrt_expr(f)) {
            sbuf_puts(b, "\\sqrt{");
            emit_TeX_expr(f->a, b, 0);
            sbuf_putc(b, '}');
        } else if (expr_is_op(f, &ops_cubrt)) {
            sbuf_puts(b, "\\sqrt[3]{");
            emit_TeX_expr(f->a, b, 0);
            sbuf_putc(b, '}');
        } else if (expr_is_op(f, &ops_exp)) {
            if (!emit_TeX_exp_unit_fraction_root(f->a, b)) {
                sbuf_puts(b, "e^{");
                emit_TeX_expr(f->a, b, 0);
                sbuf_putc(b, '}');
            }
        } else {
            sbuf_puts(b, name ? name : "\\operatorname{f}");
            if (TeX_unary_has_bare_greek_argument(f)) {
                sbuf_putc(b, ' ');
                emit_TeX_expr(f->a, b, PREC_UNARY);
            } else {
                sbuf_putc(b, '(');
                emit_TeX_expr(f->a, b, 0);
                sbuf_putc(b, ')');
            }
        }

        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (expr_is_pow_d_expr(f)) {
        int need = PREC_POW < parent_prec;
        long ei = 0;
        int exponent_has_small_int = expr_try_get_small_integer_exponent(f->c, &ei);

        if (num_is_real(f->c) && num_sign(f->c) < 0 && TeX_contains_calculus(f->a)) {
            sbuf_puts(b, "\\left(");
            emit_TeX_expr(f->a, b, PREC_LOWEST);
            sbuf_puts(b, "\\right)^{");
            emit_TeX_const_value(b, f);
            sbuf_putc(b, '}');
            return;
        }

        if (emit_TeX_unit_fraction_power(f->a, f->c, b, parent_prec))
            return;

        if (exponent_has_small_int && ei < 0) {
            int recip_need = PREC_MUL < parent_prec;
            long positive_exponent = -ei;

            if (recip_need)
                sbuf_puts(b, "\\left(");
            sbuf_puts(b, "\\frac{1}{");
            bool group_factorial = positive_exponent != 1L && TeX_postfix_factorial_argument(f->a);

            if (group_factorial)
                sbuf_puts(b, "\\left(");
            emit_TeX_expr(f->a, b, positive_exponent == 1L ? PREC_LOWEST : PREC_POW);
            if (group_factorial)
                sbuf_puts(b, "\\right)");
            if (positive_exponent != 1L) {
                char buf[64];

                snprintf(buf, sizeof(buf), "%ld", positive_exponent);
                sbuf_puts(b, "^{");
                sbuf_puts(b, buf);
                sbuf_putc(b, '}');
            }
            sbuf_putc(b, '}');
            if (recip_need)
                sbuf_puts(b, "\\right)");
            return;
        }

        if (need)
            sbuf_puts(b, "\\left(");

        const char *unary_name = f->a->ops->arity == EXPR_OP_UNARY ? TeX_unary_name(f->a) : NULL;
        if (f->a->ops->arity == EXPR_OP_UNARY && !expr_is_formal_derivative(f->a) && !expr_is_neg(f->a) &&
            !expr_distribution_qualification(f->a) &&
            !expr_is_sqrt_expr(f->a) && !expr_is_op(f->a, &ops_abs) && !expr_is_op(f->a, &ops_factorial) &&
            !TeX_postfix_factorial_argument(f->a) &&
            !strchr(unary_name ? unary_name : "", '^')) {
            const char *name = unary_name;
            sbuf_puts(b, name ? name : "\\operatorname{f}");
            sbuf_puts(b, "^{");
            if (exponent_has_small_int) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%ld", ei);
                sbuf_puts(b, buf);
            } else {
                emit_TeX_const_value(b, f);
            }
            if (TeX_unary_has_bare_greek_argument(f->a)) {
                sbuf_puts(b, "} ");
                emit_TeX_expr(f->a->a, b, PREC_UNARY);
            } else {
                sbuf_puts(b, "}(");
                emit_TeX_expr(f->a->a, b, 0);
                sbuf_putc(b, ')');
            }
        } else {
            int base_needs_parens = pow_base_needs_visible_parens(f->a) || TeX_postfix_factorial_argument(f->a);

            if (base_needs_parens)
                sbuf_puts(b, "\\left(");
            emit_TeX_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
            if (base_needs_parens)
                sbuf_puts(b, "\\right)");
            sbuf_puts(b, "^{");
            if (exponent_has_small_int) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%ld", ei);
                sbuf_puts(b, buf);
            } else {
                emit_TeX_const_value(b, f);
            }
            sbuf_putc(b, '}');
        }

        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (expr_is_mul(f)) {
        int need = PREC_MUL < parent_prec;
        expr_t *fac[64];
        int n = 0;
        int sign = 1;

        if (need)
            sbuf_puts(b, "\\left(");

        flatten_mul((expr_t *)f, fac, &n, 64);
        sort_TeX_factors(fac, n);

        for (int i = 0; i < n; i++) {
            if (!expr_is_negative(fac[i]))
                continue;

            sign = -sign;

            if (expr_tostring_is_negative_const(fac[i])) {
                if (num_eq(fac[i]->c, NUM_NEG_ONE)) {
                    for (int j = i; j < n - 1; j++)
                        fac[j] = fac[j + 1];
                    n--;
                    i--;
                    continue;
                }
                continue;
            }

            if (expr_is_neg(fac[i])) {
                fac[i] = fac[i]->a;
                continue;
            }

            break;
        }

        if (sign < 0)
            sbuf_putc(b, '-');

        for (int i = 0; i < n; i++) {
            if (i > 0)
                emit_TeX_mul_separator(fac[i - 1], fac[i], b);
            bool grouped = n > 1 && mul_coefficient_needs_parens(fac[i], i == 0 && sign > 0);
            if (grouped)
                sbuf_puts(b, "\\left(");
            emit_TeX_factor_abs(fac[i], b);
            if (grouped)
                sbuf_puts(b, "\\right)");
        }

        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (expr_is_addsub(f)) {
        int need = PREC_ADD < parent_prec;
        bool neg = expr_renders_negative(f->b);
        const expr_t *negative_complex_base = NULL;
        const expr_t *negative_complex_rhs = NULL;
        const expr_t *complex_shift_base = NULL;
        const expr_t *complex_shift_real = NULL;
        const expr_t *complex_shift_imag = NULL;

        if (match_add_negative_complex_rhs(f, &negative_complex_base, &negative_complex_rhs)) {
            if (need)
                sbuf_puts(b, "\\left(");
            emit_TeX_expr(negative_complex_base, b, PREC_ADD);
            sbuf_puts(b, " - \\left(");
            emit_TeX_expr_abs(negative_complex_rhs, b, PREC_ADD);
            sbuf_puts(b, "\\right)");
            if (need)
                sbuf_puts(b, "\\right)");
            return;
        }

        if (match_additive_complex_shift(f, &complex_shift_base, &complex_shift_real, &complex_shift_imag)) {
            bool imag_neg = expr_renders_negative(complex_shift_imag);

            if (need)
                sbuf_puts(b, "\\left(");
            emit_TeX_expr(complex_shift_base, b, PREC_ADD);
            sbuf_puts(b, " - \\left(");
            emit_TeX_expr(complex_shift_real, b, PREC_ADD);
            sbuf_puts(b, imag_neg ? " - " : " + ");
            if (imag_neg)
                emit_TeX_expr_abs(complex_shift_imag, b, PREC_ADD);
            else
                emit_TeX_expr(complex_shift_imag, b, PREC_ADD);
            sbuf_puts(b, "\\right)");
            if (need)
                sbuf_puts(b, "\\right)");
            return;
        }

        if (emit_TeX_display_polynomial_sum(f, b, parent_prec))
            return;

        if (need)
            sbuf_puts(b, "\\left(");
        emit_TeX_expr(f->a, b, PREC_ADD);

        if (expr_is_op(f, &ops_add))
            sbuf_puts(b, neg ? " - " : " + ");
        else
            sbuf_puts(b, neg ? " + " : " - ");

        int rhs_parens = add_rhs_needs_visible_parens(f->b);
        if (neg && !rhs_parens)
            rhs_parens = emit_TeX_expr_abs_needs_visible_add_parens(f->b);
        if (rhs_parens)
            sbuf_puts(b, "\\left(");
        if (neg)
            emit_TeX_expr_abs(f->b, b, PREC_ADD);
        else
            emit_TeX_expr(f->b, b, PREC_ADD);
        if (rhs_parens)
            sbuf_puts(b, "\\right)");

        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (expr_is_op(f, &ops_div)) {
        if (emit_TeX_calculus_quotient(f, b, parent_prec, false))
            return;
        if (emit_TeX_large_sum_quotient(f->a, f->b, 1L, 1L, b, parent_prec))
            return;
        int need = PREC_MUL < parent_prec;
        bool neg_num = expr_is_negative(f->a);
        bool neg_den = expr_is_negative(f->b);
        const expr_t *atan_expr = NULL;
        const expr_t *denominator = NULL;
        const expr_t *common_numerator_factor = NULL;
        const expr_t *common_sum = NULL;

        if (expr_is_div(f->a) && f->a->a && f->a->b) {
            if (need)
                sbuf_puts(b, "\\left(");
            sbuf_puts(b, "\\frac{");
            emit_TeX_expr(f->a->a, b, PREC_LOWEST);
            sbuf_puts(b, "}{");
            emit_TeX_expr(f->a->b, b, PREC_MUL);
            emit_TeX_mul_separator(f->a->b, f->b, b);
            emit_TeX_expr(f->b, b, PREC_MUL);
            sbuf_putc(b, '}');
            if (need)
                sbuf_puts(b, "\\right)");
            return;
        }

        if (!expr_is_rendered_log_local(f->b) && match_sum_quotient(f, &common_numerator_factor, &common_sum)) {
            if (need)
                sbuf_puts(b, "\\left(");
            if (!emit_TeX_rational_quotient(common_numerator_factor, f->b, b, false, PREC_LOWEST)) {
                sbuf_puts(b, "\\frac{");
                if (common_numerator_factor)
                    emit_TeX_expr(common_numerator_factor, b, PREC_LOWEST);
                else
                    emit_TeX_expr(common_sum, b, PREC_LOWEST);
                sbuf_puts(b, "}{");
                emit_TeX_expr(f->b, b, PREC_LOWEST);
                sbuf_putc(b, '}');
            }
            if (common_numerator_factor) {
                sbuf_puts(b, "\\mkern-2mu \\left(");
                emit_TeX_expr(common_sum, b, PREC_LOWEST);
                sbuf_puts(b, "\\right)");
            }
            if (need)
                sbuf_puts(b, "\\right)");
            return;
        }

        if (emit_TeX_rational_quotient(f->a, f->b, b, false, parent_prec))
            return;

        if (match_atan_over_argument_denominator(f, &atan_expr, &denominator) && !expr_is_negative(denominator)) {
            if (need)
                sbuf_puts(b, "\\left(");
            sbuf_puts(b, "\\frac{1}{");
            emit_TeX_expr(denominator, b, PREC_LOWEST);
            sbuf_puts(b, "} ");
            emit_TeX_expr(atan_expr, b, PREC_MUL);
            if (need)
                sbuf_puts(b, "\\right)");
            return;
        }

        if (need)
            sbuf_puts(b, "\\left(");
        if (neg_num ^ neg_den)
            sbuf_putc(b, '-');

        sbuf_puts(b, "\\frac{");
        if (neg_num)
            emit_TeX_expr_abs(f->a, b, PREC_LOWEST);
        else
            emit_TeX_expr(f->a, b, PREC_LOWEST);
        sbuf_puts(b, "}{");
        if (neg_den)
            emit_TeX_expr_abs(f->b, b, PREC_LOWEST);
        else
            emit_TeX_expr(f->b, b, PREC_LOWEST);
        sbuf_putc(b, '}');

        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (expr_is_op(f, &ops_pow)) {
        int need = PREC_POW < parent_prec;
        int base_is_composite_binding =
            expr_is_const(f->a) && expr_tostring_should_emit_binding_expr(f->a) && f->a->binding_expr &&
            f->a->binding_expr->kind != EXPR_BINDING_EXPR_NUMBER &&
            f->a->binding_expr->kind != EXPR_BINDING_EXPR_CONST;
        int base_needs_parens =
            pow_base_needs_visible_parens(f->a) || expr_is_op(f->a, &ops_exp) || base_is_composite_binding ||
            TeX_postfix_factorial_argument(f->a);

        if (expr_is_const(f->b) && (!f->b->name || !*f->b->name) &&
            emit_TeX_unit_fraction_power(f->a, f->b->c, b, parent_prec))
            return;

        if (need)
            sbuf_puts(b, "\\left(");
        if (base_needs_parens)
            sbuf_puts(b, "\\left(");
        emit_TeX_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
        if (base_needs_parens)
            sbuf_puts(b, "\\right)");
        sbuf_puts(b, "^{");
        if (TeX_exp_needs_parens(f->b))
            emit_TeX_expr(f->b, b, 0);
        else
            emit_TeX_expr(f->b, b, 0);
        sbuf_putc(b, '}');
        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (f->ops->arity == EXPR_OP_BINARY) {
        if (expr_is_op(f, &ops_root)) {
            sbuf_puts(b, "\\sqrt[");
            emit_TeX_expr(f->b, b, PREC_LOWEST);
            sbuf_puts(b, "]{");
            emit_TeX_expr(f->a, b, PREC_LOWEST);
            sbuf_putc(b, '}');
            return;
        }
        if (expr_is_op(f, &ops_indexed_symbol)) {
            emit_TeX_expr(f->a, b, 0);
            sbuf_puts(b, "_{");
            emit_TeX_expr(f->b, b, 0);
            sbuf_putc(b, '}');
            return;
        }
        if (expr_is_op(f, &ops_summation) || expr_is_op(f, &ops_product)) {
            const expr_t *index = f->b;
            const expr_t *lower = NULL;
            const expr_t *upper = NULL;

            /* Delimit the whole aggregate when another factor can follow it. */
            bool grouped = parent_prec >= PREC_MUL;
            if (grouped)
                sbuf_puts(b, "\\left(");
            if (expr_is_op(f->b, &ops_argument_list)) {
                index = f->b->a;
                upper = f->b->b;
                if (expr_is_op(upper, &ops_argument_list)) {
                    lower = upper->a;
                    upper = upper->b;
                }
            }
            sbuf_puts(b, expr_is_op(f, &ops_summation) ? "\\sum_{" : "\\prod_{");
            emit_TeX_expr(index, b, 0);
            sbuf_putc(b, '=');
            if (lower)
                emit_TeX_expr(lower, b, 0);
            else
                sbuf_putc(b, '0');
            sbuf_puts(b, "}^{");
            if (upper)
                emit_TeX_expr(upper, b, 0);
            else
                sbuf_puts(b, "\\infty");
            sbuf_putc(b, '}');
            TeX_index_domain_t scope = {index, !lower || TeX_known_nonnegative_integer(lower), TeX_index_domain};

            TeX_index_domain = &scope;
            emit_TeX_expr(f->a, b, PREC_MUL);
            TeX_index_domain = scope.outer;
            if (grouped)
                sbuf_puts(b, "\\right)");
            return;
        }
        if (expr_is_op(f, &ops_qdigamma)) {
            sbuf_puts(b, "\\psi_{");
            emit_TeX_expr(f->a, b, PREC_LOWEST);
            sbuf_puts(b, "}\\left(");
            emit_TeX_expr(f->b, b, PREC_LOWEST);
            sbuf_puts(b, "\\right)");
            return;
        }
        if (expr_is_op(f, &ops_polygamma)) {
            emit_TeX_polygamma(f, b);
            return;
        }
        if (expr_is_op(f, &ops_zetah) || expr_is_op(f, &ops_zatahp)) {
            sbuf_puts(b, f->ops->TeX_name);
            sbuf_putc(b, '(');
            emit_TeX_expr(f->a, b, 0);
            sbuf_puts(b, ", ");
            emit_TeX_expr(f->b, b, 0);
            sbuf_putc(b, ')');
            return;
        }
        if (expr_is_op(f, &ops_lerch_phi) && f->a && expr_is_op(f->a, &ops_lerch_phi_pack)) {
            sbuf_puts(b, "\\Phi\\left(");
            emit_TeX_expr(f->a->a, b, 0);
            sbuf_puts(b, ", ");
            emit_TeX_expr(f->a->b, b, 0);
            sbuf_puts(b, ", ");
            emit_TeX_expr(f->b, b, 0);
            sbuf_puts(b, "\\right)");
            return;
        }
        if (expr_has_polylog_order(f)) {
            emit_TeX_polylog(f, b);
            return;
        }
        if (f->ops == &ops_chebyshev_t || f->ops == &ops_chebyshev_u || f->ops == &ops_hermite_h) {
            sbuf_puts(b, f->ops->TeX_name);
            sbuf_puts(b, "_{");
            emit_TeX_expr(f->a, b, PREC_LOWEST);
            sbuf_puts(b, "}");
            sbuf_puts(b, "\\left(");
            emit_TeX_expr(f->b, b, PREC_LOWEST);
            sbuf_puts(b, "\\right)");
            return;
        }
        if (expr_is_op(f, &ops_harmonic_poly)) {
            emit_TeX_harmonic_poly(f, b);
            return;
        }
        if (expr_has_legendre_chi_order(f)) {
            emit_TeX_legendre_chi(f, b);
            return;
        }
        if (expr_is_op(f, &ops_clausen)) {
            sbuf_puts(b, "\\operatorname{Cl}_{");
            emit_TeX_expr(f->a, b, PREC_LOWEST);
            sbuf_puts(b, "}(");
            emit_TeX_expr(f->b, b, PREC_LOWEST);
            sbuf_putc(b, ')');
            return;
        }
        if (expr_cylindrical_symbol(f)) {
            sbuf_puts(b, f->ops->TeX_name);
            sbuf_puts(b, "_{");
            emit_TeX_expr(f->a, b, PREC_LOWEST);
            sbuf_puts(b, "}\\left(");
            emit_TeX_expr(f->b, b, PREC_LOWEST);
            sbuf_puts(b, "\\right)");
            return;
        }
        if (expr_is_op(f, &ops_lommel_s)) {
            emit_TeX_lommel_s(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lambert_wn)) {
            emit_TeX_lambert_wn(f, b);
            return;
        }
        if (expr_is_op(f, &ops_appell_f1)) {
            emit_TeX_appell_f1(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lauricella_f)) {
            emit_TeX_lauricella_f(f, b);
            return;
        }
        if (expr_is_op(f, &ops_hypergeometric_pFq)) {
            emit_TeX_hypergeometric_pFq(f, b);
            return;
        }
        if (f->ops->TeX_name)
            sbuf_puts(b, f->ops->TeX_name);
        else {
            sbuf_puts(b, "\\operatorname{");
            sbuf_puts(b, expr_ops_expression_name(f->ops));
            sbuf_putc(b, '}');
        }
        sbuf_putc(b, '(');
        emit_TeX_expr(f->a, b, 0);
        sbuf_puts(b, ", ");
        emit_TeX_expr(f->b, b, 0);
        sbuf_putc(b, ')');
        return;
    }

    emit_TeX_atom(f, b);
}


char *expr_to_TeX_operation_body(const expr_t *expr)
{
    sbuf_t buffer;
    bool previous = preserve_TeX_operations;

    if (!expr)
        return NULL;
    sbuf_init(&buffer);
    preserve_TeX_operations = true;
    ++TeX_source_order_depth;
    emit_TeX_expr(expr, &buffer, PREC_LOWEST);
    --TeX_source_order_depth;
    preserve_TeX_operations = previous;
    char *text = expr_tostring_texify(sbuf_c_str(&buffer));
    sbuf_free(&buffer);
    return text;
}

/* Render an authored equation term without polynomial reordering, retaining the normal calculus factor layout. */
char *expr_to_TeX_body_ordered(const expr_t *expr, bool partial)
{
    if (!expr)
        return NULL;
    sbuf_t buffer;
    sbuf_init(&buffer);
    ++TeX_source_order_depth;
    if (partial)
        expr_TeX_partial_derivatives_push();
    else
        expr_TeX_total_derivatives_push();
    emit_TeX_expr(expr, &buffer, PREC_LOWEST);
    if (partial)
        expr_TeX_partial_derivatives_pop();
    else
        expr_TeX_total_derivatives_pop();
    --TeX_source_order_depth;
    char *text = expr_tostring_texify(sbuf_c_str(&buffer));
    sbuf_free(&buffer);
    return text;
}
