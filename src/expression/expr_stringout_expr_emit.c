/**
 * @file expr_stringout_expr_emit.c
 * @brief Recursive native mathematical expression rendering.
 *
 * Emits operators, absolute values and grouped subexpressions in native notation. The wrapper layer adds bindings
 * and result-level conditions after body rendering.
 *
 * This is part of the expression.h implementation. Preserve expression ownership, symbol identity and mathematical
 * domain restrictions when extending these operations.
 */

/* Recursive native expression body rendering. */

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

void emit_expr_abs(const expr_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    if (expr_tostring_is_negative_const(f)) {
        char *text;
        number_t pos_value = num_neg(f->c);

        if (emit_negative_const_binding_expr_abs(f, b, false)) {
            num_destroy(&pos_value);
            return;
        }

        text = expr_number_to_string_local(pos_value);
        if (text) {
            sbuf_puts(b, text);
            free(text);
        }
        return;
    }

    if (expr_is_const(f) && expr_renders_negative(f)) {
        char *text;
        number_t pos_value = num_neg(f->c);

        if (emit_negative_const_binding_expr_abs(f, b, false)) {
            num_destroy(&pos_value);
            return;
        }

        text = expr_number_to_string_local(pos_value);
        if (text) {
            sbuf_puts(b, text);
            free(text);
        }
        return;
    }

    if (expr_is_neg(f)) {
        emit_expr(f->a, b, parent_prec);
        return;
    }

    if (expr_is_mul(f)) {
        expr_t *fac[64];
        int n = 0;

        flatten_mul((expr_t *)f, fac, &n, 64);
        sort_factors(fac, n);

        for (int i = 0; i < n; ++i) {
            if (expr_tostring_is_negative_const(fac[i])) {
                if (num_eq(fac[i]->c, NUM_NEG_ONE)) {
                    for (int j = i; j < n - 1; ++j)
                        fac[j] = fac[j + 1];
                    --n;
                }
            }
        }

        for (int i = 0; i < n; ++i) {
            if (i > 0)
                emit_expr_mul_separator_local(fac[i - 1], fac[i], b);
            if (n > 1 && mul_factor_needs_visible_parens(fac[i]))
                sbuf_putc(b, '(');
            emit_factor_abs(fac[i], b);
            if (n > 1 && mul_factor_needs_visible_parens(fac[i]))
                sbuf_putc(b, ')');
        }
        return;
    }

    if (expr_is_op(f, &ops_div) && expr_is_negative(f)) {
        int need = PREC_MUL < parent_prec;
        if (need)
            sbuf_putc(b, '(');
        emit_factor_abs(f->a, b);
        sbuf_putc(b, '/');
        if (expr_is_negative(f->b))
            emit_expr_abs(f->b, b, PREC_POW);
        else
            emit_expr(f->b, b, PREC_POW);
        if (need)
            sbuf_putc(b, ')');
        return;
    }

    emit_expr(f, b, parent_prec);
}

void emit_expr_abs_bars(const expr_t *f, sbuf_t *b)
{
    sbuf_putc(b, '|');
    emit_expr_abs(f, b, 0);
    sbuf_putc(b, '|');
}


static _Thread_local unsigned expression_output_depth;
static void emit_expr_inner(const expr_t *f, sbuf_t *b, int parent_prec);

void emit_expr(const expr_t *f, sbuf_t *b, int parent_prec)
{
    const char *temporary = expr_math_temporary_name(f);
    if (temporary) {
        emit_name(b, temporary);
        return;
    }
    expr_cartesian_composition_t view;

    if (expression_output_depth++ == 0u && expr_cartesian_composition_init(f, &view)) {
        emit_expr_inner(view.expanded, b, parent_prec);
        expr_cartesian_composition_clear(&view);
    } else {
        emit_expr_inner(f, b, parent_prec);
    }
    --expression_output_depth;
}

static void emit_expr_inner(const expr_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    if (expr_distribution_expr_emit(f, b))
        return;

    if (emit_expr_integral_cartesian(f, b, parent_prec))
        return;

    if (expr_is_op(f, &ops_real_domain)) {
        if (parent_prec > PREC_LOWEST)
            sbuf_putc(b, '(');
        emit_expr(f->a, b, PREC_LOWEST);
        sbuf_puts(b, " where (");
        for (const expr_t *pair = f->b; pair; pair = pair->b->b) {
            if (pair != f->b)
                sbuf_puts(b, "; ");
            const expr_t *nonzero = expr_domain_nonzero_operand(pair);
            if (nonzero) {
                emit_expr(nonzero, b, PREC_LOWEST);
                sbuf_puts(b, " ≠ 0");
                continue;
            }
            if (expr_is_op(pair->a, &ops_nonnegative_integer) || expr_is_op(pair->a, &ops_real_parameter)) {
                emit_expr(pair->a->a, b, PREC_LOWEST);
                sbuf_puts(b, expr_is_op(pair->a, &ops_real_parameter) ? " ∈ ℝ" : " ∈ ℤ≥0");
                continue;
            }
            sbuf_puts(b, "Re(");
            emit_expr(pair->a, b, PREC_LOWEST);
            sbuf_puts(b, ") > ");
            emit_expr(pair->b->a, b, PREC_LOWEST);
        }
        sbuf_putc(b, ')');
        if (parent_prec > PREC_LOWEST)
            sbuf_putc(b, ')');
        return;
    }
    if (expr_is_integral_transform(f)) {
        expr_t *result = expr_transform_result(f);
        if (result) {
            emit_expr(result, b, parent_prec);
            expr_free(result);
            return;
        }
        sbuf_puts(b, f->ops->expression_name);
        sbuf_putc(b, '(');
        emit_expr(f->a, b, PREC_LOWEST);
        if (num_to_double(f->b->b->b->c) > 1) {
            sbuf_puts(b, ", ");
            emit_expr(f->b->a, b, PREC_LOWEST);
        }
        if (num_to_double(f->b->b->b->c) > 2) {
            sbuf_puts(b, ", ");
            emit_expr(f->b->b->a, b, PREC_LOWEST);
        }
        sbuf_putc(b, ')');
        return;
    }
    if (expr_is_formal_derivative(f)) {
        emit_formal_derivative_expr(f, b);
        return;
    }
    if (expr_is_op(f, &ops_ordered_derivative)) {
        emit_ordered_derivative(f, b, 0);
        return;
    }
    if (expr_is_arbitrary_function(f)) {
        emit_arbitrary_function_expr(f, b);
        return;
    }
    if (expr_is_op(f, &ops_argument_list)) {
        emit_argument_list_expr(f, b);
        return;
    }

    /* Atoms */
    if (expr_is_const(f) || expr_is_var(f)) {
        emit_atom((expr_t *)f, b, parent_prec);
        return;
    }

    if (expr_is_op(f, &ops_integral)) {
        emit_expr_integral(f, b, parent_prec);
        return;
    }

    /* Negation: -a  — only parenthesise when the child is an add/sub */
    if (expr_is_neg(f)) {
        int need = PREC_UNARY < parent_prec;
        if (need)
            sbuf_putc(b, '(');

        const expr_t *a = f->a;
        if (expr_is_neg(a)) {
            emit_expr(a->a, b, 0);
            if (need)
                sbuf_putc(b, ')');
            return;
        }
        if (expr_is_negative(a)) {
            emit_expr_abs(a, b, 0);
            if (need)
                sbuf_putc(b, ')');
            return;
        }
        int child_needs_paren = expr_is_addsub(a);
        sbuf_putc(b, '-');
        if (child_needs_paren)
            sbuf_putc(b, '(');
        emit_expr(a, b, 0);
        if (child_needs_paren)
            sbuf_putc(b, ')');

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Unary ops */
    if (f->ops->arity == EXPR_OP_UNARY) {
        int need = PREC_UNARY < parent_prec;
        if (need)
            sbuf_putc(b, '(');

        if (expr_is_op(f, &ops_abs)) {
            emit_expr_abs_bars(f->a, b);
            if (need)
                sbuf_putc(b, ')');
            return;
        }
        if (expr_is_op(f, &ops_floor)) {
            sbuf_puts(b, "⌊");
            emit_expr(f->a, b, 0);
            sbuf_puts(b, "⌋");
            if (need)
                sbuf_putc(b, ')');
            return;
        }
        if (expr_is_op(f, &ops_ceil)) {
            sbuf_puts(b, "⌈");
            emit_expr(f->a, b, 0);
            sbuf_puts(b, "⌉");
            if (need)
                sbuf_putc(b, ')');
            return;
        }
        if (expr_is_sqrt_expr(f))
            sbuf_puts(b, "√");
        else
            sbuf_puts(b, expr_unary_name(f));
        sbuf_putc(b, '(');
        emit_expr(f->a, b, 0);
        sbuf_putc(b, ')');

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Power */
    if (expr_is_pow_d_expr(f)) {
        int need = PREC_POW < parent_prec;
        long ei = 0;
        int exponent_has_small_int = expr_try_get_small_integer_exponent(f->c, &ei);

        if (num_eq(f->c, NUM_HALF)) {
            emit_expr_sqrt_power(f->a, b, parent_prec, false);
            return;
        }

        if (number_is_neg_half_local(f->c)) {
            emit_expr_sqrt_power(f->a, b, parent_prec, true);
            return;
        }

        if (exponent_has_small_int && ei < 0) {
            int recip_need = PREC_MUL < parent_prec;
            int base_needs_parens = pow_base_needs_visible_parens(f->a);
            long positive_exponent = -ei;

            if (recip_need)
                sbuf_putc(b, '(');
            sbuf_puts(b, "1/");
            if (positive_exponent == 1L) {
                emit_expr(f->a, b, PREC_POW);
            } else {
                if (base_needs_parens)
                    sbuf_putc(b, '(');
                emit_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
                if (base_needs_parens)
                    sbuf_putc(b, ')');
                emit_superscript_int(b, positive_exponent);
            }
            if (recip_need)
                sbuf_putc(b, ')');
            return;
        }

        if (need)
            sbuf_putc(b, '(');

        /* For unary functions raised to a power, write func²(arg)
         * rather than func(arg)² so the exponent binds to the function name.
         * Floor/ceiling keep their mathematical brackets: ⌊x⌋². */
        if (f->a->ops->arity == EXPR_OP_UNARY && !expr_is_formal_derivative(f->a) && !expr_is_neg(f->a) &&
            !expr_distribution_qualification(f->a)) {
            expr_t *inner = f->a;
            if (expr_is_op(inner, &ops_floor) || expr_is_op(inner, &ops_ceil)) {
                emit_expr(inner, b, PREC_POW);
            } else {
                if (expr_is_sqrt_expr(inner))
                    sbuf_puts(b, "√");
                else
                    sbuf_puts(b, expr_unary_name(inner));
            }

            if (exponent_has_small_int)
                emit_superscript_int(b, ei);
            else {
                sbuf_putc(b, '^');
                char *text = expr_const_to_string_local(f);
                if (text) {
                    sbuf_puts(b, text);
                    free(text);
                }
            }

            if (!expr_is_op(inner, &ops_floor) && !expr_is_op(inner, &ops_ceil)) {
                sbuf_putc(b, '(');
                emit_expr(inner->a, b, 0);
                sbuf_putc(b, ')');
            }

            if (need)
                sbuf_putc(b, ')');
            return;
        }

        {
            int base_needs_parens = pow_base_needs_visible_parens(f->a);

            if (base_needs_parens)
                sbuf_putc(b, '(');
            emit_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
            if (base_needs_parens)
                sbuf_putc(b, ')');
        }

        if (exponent_has_small_int)
            emit_superscript_int(b, ei);
        else {
            sbuf_putc(b, '^');
            char *text = expr_const_to_string_local(f);
            if (text) {
                sbuf_puts(b, text);
                free(text);
            }
        }

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Multiplication with sign folding */
    if (expr_is_mul(f)) {
        int need = PREC_MUL < parent_prec;
        expr_t *fac[64];
        int n = 0;

        if (need)
            sbuf_putc(b, '(');

        flatten_mul((expr_t *)f, fac, &n, 64);
        sort_factors(fac, n);

        int sign = 1;
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
                emit_expr_mul_separator_local(fac[i - 1], fac[i], b);
            bool grouped = n > 1 && mul_coefficient_needs_parens(fac[i], i == 0 && sign > 0);
            if (grouped)
                sbuf_putc(b, '(');
            emit_factor_abs(fac[i], b);
            if (grouped)
                sbuf_putc(b, ')');
        }

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Addition/subtraction with a + -b → a - b and a - -b → a + b */
    if (expr_is_addsub(f)) {
        int need = PREC_ADD < parent_prec;
        const expr_t *negative_complex_base = NULL;
        const expr_t *negative_complex_rhs = NULL;
        const expr_t *complex_shift_base = NULL;
        const expr_t *complex_shift_real = NULL;
        const expr_t *complex_shift_imag = NULL;

        if (match_add_negative_complex_rhs(f, &negative_complex_base, &negative_complex_rhs)) {
            if (need)
                sbuf_putc(b, '(');
            emit_expr(negative_complex_base, b, PREC_ADD);
            sbuf_puts(b, " - (");
            emit_expr_abs(negative_complex_rhs, b, PREC_ADD);
            sbuf_putc(b, ')');
            if (need)
                sbuf_putc(b, ')');
            return;
        }

        if (match_additive_complex_shift(f, &complex_shift_base, &complex_shift_real, &complex_shift_imag)) {
            bool imag_neg = expr_renders_negative(complex_shift_imag);

            if (need)
                sbuf_putc(b, '(');
            emit_expr(complex_shift_base, b, PREC_ADD);
            sbuf_puts(b, " - (");
            emit_expr(complex_shift_real, b, PREC_ADD);
            sbuf_puts(b, imag_neg ? " - " : " + ");
            if (imag_neg)
                emit_expr_abs(complex_shift_imag, b, PREC_ADD);
            else
                emit_expr(complex_shift_imag, b, PREC_ADD);
            sbuf_putc(b, ')');
            if (need)
                sbuf_putc(b, ')');
            return;
        }

        if (emit_expr_display_polynomial_sum(f, b, parent_prec))
            return;

        if (need)
            sbuf_putc(b, '(');

        emit_expr(f->a, b, PREC_ADD);

        bool neg = expr_renders_negative(f->b);

        /* Emit flipped operator if needed */
        if (expr_is_op(f, &ops_add)) {
            sbuf_puts(b, neg ? " - " : " + ");
        } else { /* subtraction */
            sbuf_puts(b, neg ? " + " : " - ");
        }

        int rhs_parens = add_rhs_needs_visible_parens(f->b);
        if (neg && !rhs_parens)
            rhs_parens = emit_expr_abs_needs_visible_add_parens(f->b);
        if (rhs_parens)
            sbuf_putc(b, '(');
        if (neg) {
            emit_expr_abs(f->b, b, PREC_ADD);
        } else {
            emit_expr(f->b, b, PREC_ADD);
        }
        if (rhs_parens)
            sbuf_putc(b, ')');

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Division: normalise sign onto the outside when possible */
    if (expr_is_op(f, &ops_div)) {
        int need = PREC_MUL < parent_prec;
        bool neg_num = expr_is_negative(f->a);
        bool neg_den = expr_is_negative(f->b);
        bool neg = neg_num ^ neg_den;
        const expr_t *atan_expr = NULL;
        const expr_t *denominator = NULL;
        const expr_t *common_numerator_factor = NULL;
        const expr_t *common_sum = NULL;

        if (expr_is_div(f->a) && f->a->a && f->a->b) {
            if (need)
                sbuf_putc(b, '(');
            emit_expr(f->a->a, b, PREC_MUL);
            sbuf_puts(b, "/(");
            emit_expr(f->a->b, b, PREC_POW);
            sbuf_puts(b, "·");
            emit_expr(f->b, b, PREC_POW);
            sbuf_putc(b, ')');
            if (need)
                sbuf_putc(b, ')');
            return;
        }

        /* Keep ordinary sums in the numerator; do not manufacture a reciprocal coefficient. */
        if (!expr_is_rendered_log_local(f->b) && match_sum_quotient(f, &common_numerator_factor, &common_sum) &&
            (common_numerator_factor || expr_contains_calculus_request(common_sum))) {
            if (need)
                sbuf_putc(b, '(');
            if (common_numerator_factor)
                emit_expr(common_numerator_factor, b, PREC_MUL);
            else
                sbuf_putc(b, '1');
            sbuf_putc(b, '/');
            emit_expr(f->b, b, PREC_POW);
            sbuf_puts(b, "·(");
            emit_expr(common_sum, b, PREC_LOWEST);
            sbuf_putc(b, ')');
            if (need)
                sbuf_putc(b, ')');
            return;
        }

        if (match_atan_over_argument_denominator(f, &atan_expr, &denominator) && !expr_is_negative(denominator)) {
            if (need)
                sbuf_putc(b, '(');
            sbuf_puts(b, "1/");
            emit_expr(denominator, b, PREC_POW);
            sbuf_puts(b, "·");
            emit_expr(atan_expr, b, PREC_MUL);
            if (need)
                sbuf_putc(b, ')');
            return;
        }

        if (need)
            sbuf_putc(b, '(');
        if (neg)
            sbuf_putc(b, '-');

        if (neg_num)
            emit_expr_abs(f->a, b, PREC_MUL);
        else
            emit_expr(f->a, b, PREC_MUL);

        sbuf_putc(b, '/');

        if (neg_den)
            emit_expr_abs(f->b, b, PREC_POW);
        else
            emit_expr(f->b, b, PREC_POW);

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Binary power: base^exp  or  base^(exp) when exponent needs grouping */
    if (expr_is_op(f, &ops_pow)) {
        int need = PREC_POW < parent_prec;
        int base_needs_parens = pow_base_needs_visible_parens(f->a);

        if (expr_const_half_can_render_as_sqrt_local(f->b)) {
            emit_expr_sqrt_power(f->a, b, parent_prec, false);
            return;
        }

        if (expr_const_neg_half_can_render_as_sqrt_local(f->b)) {
            emit_expr_sqrt_power(f->a, b, parent_prec, true);
            return;
        }

        if (need)
            sbuf_putc(b, '(');

        if (base_needs_parens)
            sbuf_putc(b, '(');
        emit_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
        if (base_needs_parens)
            sbuf_putc(b, ')');
        sbuf_putc(b, '^');
        int ep = pow_exp_needs_parens(f->b);
        if (ep)
            sbuf_putc(b, '(');
        emit_expr(f->b, b, 0);
        if (ep)
            sbuf_putc(b, ')');

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Named binary functions (e.g. atan2) */
    if (f->ops->arity == EXPR_OP_BINARY) {
        if (expr_is_op(f, &ops_indexed_symbol)) {
            emit_expr(f->a, b, 0);
            sbuf_puts(b, "_(");
            emit_expr(f->b, b, 0);
            sbuf_putc(b, ')');
            return;
        }
        if (expr_is_op(f, &ops_summation) || expr_is_op(f, &ops_product)) {
            const expr_t *index = f->b;
            const expr_t *lower = NULL;
            const expr_t *upper = NULL;
            number_t upper_value = NUM_NAN;
            bool infinite = false;

            /* Otherwise a following product or quotient is reparsed inside the sum. */
            bool grouped = parent_prec >= PREC_MUL;
            if (grouped)
                sbuf_putc(b, '(');
            if (expr_is_op(f->b, &ops_argument_list)) {
                index = f->b->a;
                upper = f->b->b;
                if (expr_is_op(upper, &ops_argument_list)) {
                    lower = upper->a;
                    upper = upper->b;
                }
            }
            if (upper) {
                upper_value = expr_eval(upper);
                infinite = num_is_inf(upper_value) && num_get_sign(upper_value) > 0;
            }
            sbuf_puts(b, expr_is_op(f, &ops_summation) ? "Σ_(" : "Π_(");
            emit_expr(index, b, 0);
            sbuf_putc(b, '=');
            if (lower && expr_is_addsub(lower))
                sbuf_putc(b, '(');
            if (lower)
                emit_expr(lower, b, 0);
            else
                sbuf_putc(b, '0');
            if (lower && expr_is_addsub(lower))
                sbuf_putc(b, ')');
            sbuf_putc(b, ')');
            sbuf_putc(b, '^');
            if (!upper || infinite)
                sbuf_puts(b, "∞");
            else if (expr_is_addsub(upper))
                sbuf_putc(b, '(');
            if (upper && !infinite)
                emit_expr(upper, b, 0);
            if (upper && !infinite && expr_is_addsub(upper))
                sbuf_putc(b, ')');
            sbuf_putc(b, ' ');
            emit_expr(f->a, b, PREC_MUL);
            if (grouped)
                sbuf_putc(b, ')');
            num_destroy(&upper_value);
            return;
        }
        const char *cylindrical_symbol = expr_cylindrical_symbol(f);
        if (cylindrical_symbol) {
            long order;
            sbuf_puts(b, cylindrical_symbol);
            if (f->a && expr_is_const(f->a) && !f->a->name &&
                expr_try_get_small_integer_exponent(f->a->c, &order) && order != LONG_MIN) {
                emit_subscript_int(b, order);
            } else {
                sbuf_puts(b, "_{");
                emit_expr(f->a, b, PREC_LOWEST);
                sbuf_putc(b, '}');
            }
            sbuf_putc(b, '(');
            emit_expr(f->b, b, PREC_LOWEST);
            sbuf_putc(b, ')');
            return;
        }
        if (expr_has_polygamma_order(f)) {
            emit_expr_polygamma(f, b);
            return;
        }
        if (expr_has_polylog_order(f)) {
            emit_expr_polylog(f, b);
            return;
        }
        if (expr_has_legendre_chi_order(f)) {
            emit_expr_legendre_chi(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lambert_wn)) {
            emit_expr_lambert_wn(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lommel_s)) {
            emit_expr_lommel_s(f, b);
            return;
        }
        if (expr_is_op(f, &ops_appell_f1)) {
            emit_expr_appell_f1(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lauricella_f)) {
            emit_expr_lauricella_f(f, b);
            return;
        }
        if (expr_is_op(f, &ops_hypergeometric_pFq)) {
            emit_expr_hypergeometric_pFq(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lerch_phi) && f->a && expr_is_op(f->a, &ops_lerch_phi_pack)) {
            sbuf_puts(b, expr_ops_expression_name(f->ops));
            sbuf_putc(b, '(');
            emit_expr(f->a->a, b, 0);
            sbuf_puts(b, ", ");
            emit_expr(f->a->b, b, 0);
            sbuf_puts(b, ", ");
            emit_expr(f->b, b, 0);
            sbuf_putc(b, ')');
            return;
        }
        else
            sbuf_puts(b, expr_ops_expression_name(f->ops));
        sbuf_putc(b, '(');
        emit_expr(f->a, b, 0);
        sbuf_puts(b, ", ");
        emit_expr(f->b, b, 0);
        sbuf_putc(b, ')');
        return;
    }

    /* Fallback */
    emit_atom((expr_t *)f, b, parent_prec);
}

/* ------------------------------------------------------------------------- */
/* FUNCTION MODE (calculator-style)                                          */
/* ------------------------------------------------------------------------- */
