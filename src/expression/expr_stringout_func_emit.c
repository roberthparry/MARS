/**
 * @file expr_stringout_func_emit.c
 * @brief Recursive executable function-body rendering.
 *
 * Emits operation calls, numeric atoms and additive chains in the function syntax. This unit formats expression
 * bodies; declaration and binding assembly belongs to the wrapper file.
 *
 * This is part of the expression.h implementation. Preserve expression ownership, symbol identity and mathematical
 * domain restrictions when extending these operations.
 */

/* Recursive executable Function body rendering. */

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

static _Thread_local bool preserve_function_operations;

void emit_func_abs(const expr_t *f, sbuf_t *b, int parent_prec)
{
    sbuf_t tmp;

    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    if (expr_is_neg(f)) {
        emit_func(f->a, b, parent_prec);
        return;
    }

    if (!expr_renders_negative(f)) {
        emit_func(f, b, parent_prec);
        return;
    }

    sbuf_init(&tmp);
    emit_func(f, &tmp, parent_prec);
    if (sbuf_len(&tmp) > 0u && rune_is_equal(string_at(tmp.text, 0u), '-')) {
        string_t *tail = string_substr(tmp.text, 1u, sbuf_len(&tmp) - 1u);

        if (tail) {
            sbuf_put_string(b, tail);
            string_free(tail);
        }
    } else {
        sbuf_put_string(b, tmp.text);
    }
    sbuf_free(&tmp);
}


void emit_func_fragment(sbuf_t *b, const char *text)
{
    static const struct {
        const char *symbol;
        const char *alias;
    } named_constants[] = {
        {"π", "@pi"},
        {"φ", "@phi"},
        {"γ", "@eulermascheroni"},
        {"τ", "@tau"},
        {"∞", "@inf"},
    };
    char *normalised;
    size_t input_length;
    size_t input_index = 0u;
    size_t output_index = 0u;

    if (!text)
        return;

    input_length = strlen(text);
    if (input_length > ((size_t)-1 - 1u) / 9u) {
        sbuf_puts(b, text);
        return;
    }
    normalised = malloc(input_length * 9u + 1u);
    if (!normalised) {
        sbuf_puts(b, text);
        return;
    }

    while (text[input_index] != '\0') {
        if (strncmp(text + input_index, "@gamma", 6u) == 0 &&
            !isalnum((unsigned char)text[input_index + 6u]) && text[input_index + 6u] != '_') {
            strcpy(normalised + output_index, "@eulermascheroni");
            output_index += strlen("@eulermascheroni");
            input_index += 6u;
            continue;
        }
        size_t fraction_width = function_ascii_stacked_fraction(normalised + output_index, text + input_index);
        bool emitted_named_constant = false;

        if (fraction_width == 0u)
            fraction_width = function_ascii_vulgar_fraction(normalised + output_index, text + input_index);
        if (fraction_width != 0u) {
            output_index += strlen(normalised + output_index);
            input_index += fraction_width;
            continue;
        }
        for (size_t constant_index = 0u;
             constant_index < sizeof(named_constants) / sizeof(named_constants[0]); ++constant_index) {
            size_t symbol_width = strlen(named_constants[constant_index].symbol);

            if (strncmp(text + input_index, named_constants[constant_index].symbol, symbol_width) != 0)
                continue;
            strcpy(normalised + output_index, named_constants[constant_index].alias);
            output_index += strlen(named_constants[constant_index].alias);
            input_index += symbol_width;
            emitted_named_constant = true;
            break;
        }
        if (emitted_named_constant)
            continue;
        if (strncmp(text + input_index, "inf", 3u) == 0 &&
            (input_index == 0u || (!isalnum((unsigned char)text[input_index - 1u]) &&
                                   text[input_index - 1u] != '_' && text[input_index - 1u] != '@')) &&
            !isalnum((unsigned char)text[input_index + 3u]) && text[input_index + 3u] != '_') {
            memcpy(normalised + output_index, "@inf", 4u);
            output_index += 4u;
            input_index += 3u;
            continue;
        }
        if (text[input_index] == ' ') {
            size_t operator_index = input_index;

            while (text[operator_index] == ' ')
                ++operator_index;
            if (text[operator_index] == '*') {
                input_index = operator_index + 1u;
                while (text[input_index] == ' ')
                    ++input_index;
                normalised[output_index++] = '.';
                continue;
            }
        }
        if (text[input_index] == '*') {
            normalised[output_index++] = '.';
            ++input_index;
            while (text[input_index] == ' ')
                ++input_index;
            continue;
        }
        normalised[output_index++] = text[input_index++];
    }
    normalised[output_index] = '\0';
    sbuf_puts(b, normalised);
    free(normalised);
}

/* Normalise fraction glyphs before testing precedence; a following multiplication dot must not join a denominator. */
static void emit_func_numeric_atom(const expr_t *expr, sbuf_t *b, const char *text, int parent_prec)
{
    sbuf_t atom;
    sbuf_init(&atom);
    emit_func_fragment(&atom, text);
    const char *normalised = string_c_str(atom.text);
    bool grouped = numeric_atom_needs_grouping(expr, normalised, parent_prec) ||
                   (parent_prec >= PREC_MUL && normalised && strchr(normalised, '/'));
    /* Additive chains extract a leading sign; keep it visible outside grouped product coefficients. */
    bool leading_minus = grouped && parent_prec == PREC_MUL && normalised[0] == '-';
    if (leading_minus)
        sbuf_putc(b, '-');
    if (grouped)
        sbuf_putc(b, '(');
    sbuf_puts(b, normalised ? normalised + leading_minus : "");
    if (grouped)
        sbuf_putc(b, ')');
    sbuf_free(&atom);
}

static void emit_func_additive_chain(const expr_t *expr, sbuf_t *b, bool subtract, bool *emitted)
{
    const char *temporary_name = function_temporary_name(expr);
    bool negative;
    bool effective_subtract;

    if (!temporary_name && expr_is_op(expr, &ops_add)) {
        emit_func_additive_chain(expr->a, b, subtract, emitted);
        emit_func_additive_chain(expr->b, b, subtract, emitted);
        return;
    }
    if (!temporary_name && expr_is_op(expr, &ops_sub)) {
        emit_func_additive_chain(expr->a, b, subtract, emitted);
        emit_func_additive_chain(expr->b, b, !subtract, emitted);
        return;
    }
    if (!temporary_name && expr_is_op(expr, &ops_neg)) {
        emit_func_additive_chain(expr->a, b, !subtract, emitted);
        return;
    }

    negative = !temporary_name && expr_renders_negative(expr);
    effective_subtract = subtract != negative;
    if (*emitted)
        sbuf_puts(b, effective_subtract ? " - " : " + ");
    else if (effective_subtract)
        sbuf_putc(b, '-');

    if (negative)
        emit_func_abs(expr, b, PREC_ADD);
    else
        emit_func(expr, b, PREC_ADD);
    *emitted = true;
}

/* Emit an authored operation tree without replacing nested transforms by their results. */
void emit_func_operations(const expr_t *f, sbuf_t *b, int parent_prec)
{
    bool previous = preserve_function_operations;

    preserve_function_operations = true;
    emit_func(f, b, parent_prec);
    preserve_function_operations = previous;
}

void emit_func(const expr_t *f, sbuf_t *b, int parent_prec)
{
    const char *temporary_name = function_temporary_name(f);

    if (temporary_name) {
        sbuf_puts(b, temporary_name);
        return;
    }

    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    if (expr_is_half_line_finite_part(f)) {
        emit_func(f->a, b, parent_prec);
        return;
    }
    const char *qualification = expr_distribution_qualification(f);
    if (qualification) {
        sbuf_putc(b, '(');
        emit_func(f->a, b, PREC_LOWEST);
        sbuf_puts(b, " : ");
        sbuf_puts(b, qualification);
        sbuf_putc(b, ')');
        return;
    }

    if (!preserve_function_operations && expr_is_addsub(f) &&
        (display_series_remainder(f) || display_sum_has_transform(f)) &&
        emit_func_display_polynomial_sum(f, b, parent_prec))
        return;

    if (!preserve_function_operations && emit_func_integral_cartesian(f, b, parent_prec))
        return;

    if (expr_is_integral_transform(f)) {
        expr_t *result = preserve_function_operations ? NULL : expr_transform_result(f);
        if (result) {
            emit_func(result, b, parent_prec);
            expr_free(result);
            return;
        }
        sbuf_puts(b, f->ops->function_name);
        sbuf_putc(b, '(');
        emit_func(f->a, b, PREC_LOWEST);
        sbuf_puts(b, ", ");
        emit_func(f->b->a, b, PREC_LOWEST);
        sbuf_puts(b, ", ");
        emit_func(f->b->b->a, b, PREC_LOWEST);
        sbuf_putc(b, ')');
        return;
    }
    if (expr_is_op(f, &ops_real_domain)) {
        if (parent_prec > PREC_LOWEST)
            sbuf_putc(b, '(');
        emit_func(f->a, b, PREC_LOWEST);
        sbuf_puts(b, " where (");
        for (const expr_t *pair = f->b; pair; pair = pair->b->b) {
            if (pair != f->b)
                sbuf_puts(b, "; ");
            const expr_t *nonzero = expr_domain_nonzero_operand(pair);
            if (nonzero) {
                emit_func(nonzero, b, PREC_LOWEST);
                sbuf_puts(b, " != 0");
                continue;
            }
            if (expr_is_op(pair->a, &ops_nonnegative_integer) || expr_is_op(pair->a, &ops_real_parameter)) {
                emit_func(pair->a->a, b, PREC_LOWEST);
                sbuf_puts(b, expr_is_op(pair->a, &ops_real_parameter) ? " ∈ ℝ" : " ∈ ℤ≥0");
                continue;
            }
            sbuf_puts(b, "Re(");
            emit_func(pair->a, b, PREC_LOWEST);
            sbuf_puts(b, ") > ");
            emit_func(pair->b->a, b, PREC_LOWEST);
        }
        sbuf_putc(b, ')');
        if (parent_prec > PREC_LOWEST)
            sbuf_putc(b, ')');
        return;
    }
    if (expr_is_formal_derivative(f)) {
        emit_formal_derivative_func(f, b);
        return;
    }
    if (expr_is_op(f, &ops_ordered_derivative)) {
        emit_ordered_derivative(f, b, 1);
        return;
    }
    if (expr_is_arbitrary_function(f)) {
        emit_arbitrary_function_func(f, b);
        return;
    }
    if (expr_is_op(f, &ops_argument_list)) {
        emit_argument_list_func(f, b);
        return;
    }

    if (expr_is_const(f)) {
        if (expr_tostring_should_emit_binding_expr(f)) {
            char *text = expr_binding_expr_to_function_string(f->binding_expr);

            if (text) {
                emit_func_numeric_atom(f, b, text, parent_prec);
                free(text);
            }
        } else if (f->name && *f->name) {
            const char *canonical = expr_default_constant_canonical_name(f->name);

            if (canonical && strcmp(canonical, "@gamma") == 0)
                sbuf_puts(b, "@eulermascheroni");
            else
                emit_name_func(b, f->name);
        }
        else {
            char *text = expr_const_to_string_local(f);
            if (text) {
                emit_func_numeric_atom(f, b, text, parent_prec);
                free(text);
            }
        }
        return;
    }

    if (expr_is_var(f)) {
        emit_name_func(b, f->name ? f->name : "x");
        return;
    }

    if (expr_is_op(f, &ops_integral)) {
        emit_func_integral(f, b);
        return;
    }

    if (expr_is_op(f, &ops_neg)) {
        int need = PREC_UNARY < parent_prec;
        int nested_negation = f->a && expr_is_op(f->a, &ops_neg);
        int child_precedence = f->a && (expr_is_mul(f->a) || expr_is_op(f->a, &ops_div)) ? PREC_MUL : PREC_UNARY;

        if (need)
            sbuf_putc(b, '(');
        sbuf_putc(b, '-');
        if (nested_negation)
            sbuf_putc(b, '(');
        emit_func(f->a, b, nested_negation ? PREC_LOWEST : child_precedence);
        if (nested_negation)
            sbuf_putc(b, ')');
        if (need)
            sbuf_putc(b, ')');
        return;
    }

    if (f->ops->arity == EXPR_OP_UNARY) {
        int need = PREC_UNARY < parent_prec;
        if (need)
            sbuf_putc(b, '(');

        emit_function_builtin_name(b, f->ops);
        sbuf_putc(b, '(');
        emit_func(f->a, b, 0);
        sbuf_putc(b, ')');

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    if (expr_is_pow_d_expr(f)) {
        int need = PREC_POW < parent_prec;
        int base_needs_parens = pow_base_needs_visible_parens(f->a);
        long ei = 0;
        int exponent_has_small_int = expr_try_get_small_integer_exponent(f->c, &ei);

        if (exponent_has_small_int && ei < 0) {
            int recip_need = PREC_MUL < parent_prec;
            long positive_exponent = -ei;

            if (recip_need)
                sbuf_putc(b, '(');
            sbuf_puts(b, "1/");
            if (positive_exponent == 1L) {
                emit_func(f->a, b, PREC_POW);
            } else {
                if (base_needs_parens)
                    sbuf_putc(b, '(');
                emit_func(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
                if (base_needs_parens)
                    sbuf_putc(b, ')');
                sbuf_putc(b, '^');
                char buf[64];
                snprintf(buf, sizeof(buf), "%ld", positive_exponent);
                sbuf_puts(b, buf);
            }
            if (recip_need)
                sbuf_putc(b, ')');
            return;
        }

        if (need)
            sbuf_putc(b, '(');

        if (base_needs_parens)
            sbuf_putc(b, '(');
        emit_func(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
        if (base_needs_parens)
            sbuf_putc(b, ')');

        sbuf_putc(b, '^');
        char *text = expr_const_to_string_local(f);
        if (text) {
            emit_func_fragment(b, text);
            free(text);
        }

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    if (expr_is_mul(f)) {
        int need = PREC_MUL < parent_prec;
        bool leading_half_as_divisor = false;
        bool emitted = false;
        bool consumed[64] = {false};
        const char *factored_temporary;

        if (need)
            sbuf_putc(b, '(');

        expr_t *fac[64];
        int n = 0;
        flatten_func_mul((expr_t *)f, fac, &n, 64);
        sort_factors(fac, n);
        factored_temporary = function_factored_temporary_name(f, NULL, fac, n, consumed);
        leading_half_as_divisor = n > 1 && !consumed[0] && expr_is_const_half_local(fac[0]);

        for (int i = 0; i < n; i++) {
            if (consumed[i] || (leading_half_as_divisor && i == 0))
                continue;

            if (emitted)
                sbuf_putc(b, '.');
            if ((n > 1 || factored_temporary) && !function_temporary_name(fac[i]) &&
                mul_factor_needs_visible_parens(fac[i]))
                sbuf_putc(b, '(');
            emit_func(fac[i], b, PREC_MUL);
            if ((n > 1 || factored_temporary) && !function_temporary_name(fac[i]) &&
                mul_factor_needs_visible_parens(fac[i]))
                sbuf_putc(b, ')');
            emitted = true;
        }

        if (factored_temporary) {
            if (emitted)
                sbuf_putc(b, '.');
            sbuf_puts(b, factored_temporary);
        }

        if (leading_half_as_divisor)
            sbuf_puts(b, "/2");

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    if (expr_is_addsub(f)) {
        int need = PREC_ADD < parent_prec;
        bool emitted = false;

        if (need)
            sbuf_putc(b, '(');

        emit_func_additive_chain(f, b, false, &emitted);

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Division: a/b */
    if (expr_is_op(f, &ops_div)) {
        int need = PREC_MUL < parent_prec;
        expr_t *numerator_factors[64];
        bool consumed[64] = {false};
        const char *factored_temporary;
        int numerator_count = 0;

        flatten_func_mul(f->a, numerator_factors, &numerator_count, 64);
        sort_factors(numerator_factors, numerator_count);
        factored_temporary =
            function_factored_temporary_name(f, f->b, numerator_factors, numerator_count, consumed);
        if (factored_temporary) {
            bool emitted = false;

            if (need)
                sbuf_putc(b, '(');
            for (int index = 0; index < numerator_count; ++index) {
                if (consumed[index])
                    continue;
                if (emitted)
                    sbuf_putc(b, '.');
                if (mul_factor_needs_visible_parens(numerator_factors[index]))
                    sbuf_putc(b, '(');
                emit_func(numerator_factors[index], b, PREC_MUL);
                if (mul_factor_needs_visible_parens(numerator_factors[index]))
                    sbuf_putc(b, ')');
                emitted = true;
            }
            if (emitted)
                sbuf_putc(b, '.');
            sbuf_puts(b, factored_temporary);
            if (need)
                sbuf_putc(b, ')');
            return;
        }

        if (need)
            sbuf_putc(b, '(');

        emit_func(f->a, b, PREC_MUL);
        sbuf_putc(b, '/');
        emit_func(f->b, b, PREC_POW);

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Binary power: base^exp  or  base^(exp) when exponent needs grouping */
    if (expr_is_op(f, &ops_pow)) {
        int need = PREC_POW < parent_prec;
        int base_needs_parens = function_temporary_name(f->a) ? 0 : pow_base_needs_visible_parens(f->a);
        if (need)
            sbuf_putc(b, '(');

        if (base_needs_parens)
            sbuf_putc(b, '(');
        emit_func(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
        if (base_needs_parens)
            sbuf_putc(b, ')');
        sbuf_putc(b, '^');
        int ep = function_temporary_name(f->b) ? 0 : pow_exp_needs_parens(f->b);
        if (ep)
            sbuf_putc(b, '(');
        emit_func(f->b, b, 0);
        if (ep)
            sbuf_putc(b, ')');

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Named binary functions (e.g. atan2) */
    if (f->ops->arity == EXPR_OP_BINARY) {
        if (expr_is_op(f, &ops_indexed_symbol)) {
            sbuf_puts(b, "indexed(");
            emit_func(f->a, b, 0);
            sbuf_puts(b, ", ");
            emit_func(f->b, b, 0);
            sbuf_putc(b, ')');
            return;
        }
        if (expr_is_op(f, &ops_summation) || expr_is_op(f, &ops_product)) {
            const expr_t *index = f->b;
            const expr_t *lower = NULL;
            const expr_t *upper = NULL;
            number_t upper_value = NUM_NAN;
            bool infinite = false;

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
            sbuf_puts(b, expr_is_op(f, &ops_summation) ? "sum(" : "product(");
            emit_func(index, b, 0);
            sbuf_puts(b, ", ");
            if (lower)
                emit_func(lower, b, 0);
            else
                sbuf_putc(b, '0');
            sbuf_puts(b, ", ");
            if (upper && !infinite)
                emit_func(upper, b, 0);
            else
                sbuf_puts(b, "@inf");
            sbuf_puts(b, ", ");
            emit_func(f->a, b, 0);
            sbuf_putc(b, ')');
            num_destroy(&upper_value);
            return;
        }
        if (expr_has_polygamma_order(f)) {
            emit_func_polygamma(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lommel_s)) {
            emit_func_lommel_s(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lerch_phi) && f->a && expr_is_op(f->a, &ops_lerch_phi_pack)) {
            emit_function_builtin_name(b, f->ops);
            sbuf_putc(b, '(');
            emit_func(f->a->a, b, 0);
            sbuf_puts(b, ", ");
            emit_func(f->a->b, b, 0);
            sbuf_puts(b, ", ");
            emit_func(f->b, b, 0);
            sbuf_putc(b, ')');
            return;
        }
        if (expr_is_op(f, &ops_appell_f1)) {
            emit_func_appell_f1(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lauricella_f)) {
            emit_func_lauricella_f(f, b);
            return;
        }
        if (expr_is_op(f, &ops_hypergeometric_pFq)) {
            emit_func_hypergeometric_pFq(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lambert_wn)) {
            emit_func_lambert_wn(f, b);
            return;
        }
        emit_function_builtin_name(b, f->ops);
        sbuf_putc(b, '(');
        emit_func(f->a, b, 0);
        sbuf_puts(b, ", ");
        emit_func(f->b, b, 0);
        sbuf_putc(b, ')');
        return;
    }

    emit_name_func(b, f->name ? f->name : "?");
}
