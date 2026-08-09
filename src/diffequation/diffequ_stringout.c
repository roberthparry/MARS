#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

/* TeX commands are considerably wider in source than when rendered. */
static const size_t de_TeX_line_limit = 180u;

static int de_append_superscript(string_t *out, size_t value)
{
    static const char *const digits[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    char text[3u * sizeof(size_t) + 1u];

    if (snprintf(text, sizeof(text), "%zu", value) < 0)
        return -1;
    for (const char *cursor = text; *cursor; ++cursor) {
        if (string_append_cstr(out, digits[(size_t)(*cursor - '0')]) != 0)
            return -1;
    }
    return 0;
}

static int de_is_derivative_letter(char value)
{
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

static size_t de_derivative_letter_length(const char *text)
{
    unsigned char lead;
    size_t length;

    if (!text || !*text)
        return 0u;
    if (de_is_derivative_letter(*text))
        return 1u;
    lead = (unsigned char)*text;
    if ((lead & 0xe0u) == 0xc0u)
        length = 2u;
    else if ((lead & 0xf0u) == 0xe0u)
        length = 3u;
    else if ((lead & 0xf8u) == 0xf0u)
        length = 4u;
    else
        return 0u;
    for (size_t i = 1u; i < length; ++i) {
        if (!text[i] || ((unsigned char)text[i] & 0xc0u) != 0x80u)
            return 0u;
    }
    return length;
}

static int de_append_next_character(string_t *out, const char *text, size_t *position)
{
    size_t length = de_derivative_letter_length(text + *position);

    if (length == 0u)
        length = 1u;
    if (string_append_chars(out, text + *position, length) != 0)
        return -1;
    *position += length;
    return 0;
}

static string_t *de_normalise_greek_slice(const char *text, size_t start, size_t end)
{
    size_t length = end - start;
    char *alias;
    string_t *alias_text;
    string_t *normalized;

    alias = malloc(length + 1u);
    if (!alias)
        return NULL;
    memcpy(alias, text + start, length);
    alias[length] = '\0';
    alias_text = string_new_with(alias);
    free(alias);
    if (!alias_text)
        return NULL;
    normalized = expr_normalise_greek_alias_text(alias_text);
    string_free(alias_text);
    return normalized;
}

static string_t *de_greek_aliases_to_symbols(const string_t *source)
{
    const char *text = source ? string_c_str(source) : "";
    string_t *out = string_new();
    size_t index = 0u;

    if (!out)
        return NULL;
    while (text[index]) {
        size_t start = index;
        size_t end;
        string_t *normalized = NULL;

        if (text[index] == '@' && isalpha((unsigned char)text[index + 1u])) {
            start = ++index;
            while (isalpha((unsigned char)text[index]))
                ++index;
            normalized = de_normalise_greek_slice(text, start, index);
            if (normalized) {
                if (string_append_string(out, normalized) != 0)
                    goto fail_normalized;
                string_free(normalized);
                continue;
            }
            index = start - 1u;
        } else if (isalpha((unsigned char)text[index])) {
            while (isalpha((unsigned char)text[index]))
                ++index;
            end = index;
            normalized = de_normalise_greek_slice(text, start, end);
            if (!normalized && end - start > 1u && text[start] == 'd') {
                normalized = de_normalise_greek_slice(text, start + 1u, end);
                if (normalized && string_append_char(out, 'd') != 0)
                    goto fail_normalized;
            }
            if (normalized) {
                if (string_append_string(out, normalized) != 0)
                    goto fail_normalized;
                string_free(normalized);
            } else if (string_append_chars(out, text + start, end - start) != 0) {
                goto fail;
            }
            continue;
        }

        if (de_append_next_character(out, text, &index) != 0)
            goto fail;
        continue;

    fail_normalized:
        string_free(normalized);
        goto fail;
    }
    return out;

fail:
    string_free(out);
    return NULL;
}

static int de_append_standard_derivative(string_t *out, const char *suffix, size_t suffix_length, size_t order,
                                         const char *dependent, size_t dependent_length, bool partial)
{
    if (string_append_cstr(out, partial ? "∂" : "d") != 0)
        return -1;
    if (order > 1u && de_append_superscript(out, order) != 0)
        return -1;
    if (string_append_chars(out, dependent, dependent_length) != 0 || string_append_char(out, '/') != 0)
        return -1;

    if (!partial) {
        size_t variable_length = de_derivative_letter_length(suffix);

        if (string_append_char(out, 'd') != 0 || variable_length == 0u ||
            string_append_chars(out, suffix, variable_length) != 0)
            return -1;
        return order > 1u ? de_append_superscript(out, order) : 0;
    }

    {
        size_t *offsets = calloc(order ? order : 1u, sizeof(*offsets));
        size_t *lengths = calloc(order ? order : 1u, sizeof(*lengths));
        size_t cursor = 0u;
        int status = 0;

        if (!offsets || !lengths) {
            free(lengths);
            free(offsets);
            return -1;
        }
        for (size_t i = 0u; i < order; ++i) {
            offsets[i] = cursor;
            lengths[i] = de_derivative_letter_length(suffix + cursor);
            if (lengths[i] == 0u || cursor + lengths[i] > suffix_length) {
                status = -1;
                goto partial_cleanup;
            }
            cursor += lengths[i];
        }
        if (cursor != suffix_length) {
            status = -1;
            goto partial_cleanup;
        }
        for (size_t index = order; index > 0u;) {
            size_t variable = index - 1u;
            size_t multiplicity = 1u;

            while (index > multiplicity) {
                size_t previous = index - multiplicity - 1u;

                if (lengths[previous] != lengths[variable] ||
                    memcmp(suffix + offsets[previous], suffix + offsets[variable], lengths[variable]) != 0)
                    break;
                ++multiplicity;
            }
            if (string_append_cstr(out, "∂") != 0 ||
                string_append_chars(out, suffix + offsets[variable], lengths[variable]) != 0 ||
                (multiplicity > 1u && de_append_superscript(out, multiplicity) != 0)) {
                status = -1;
                goto partial_cleanup;
            }
            index -= multiplicity;
        }

    partial_cleanup:
        free(lengths);
        free(offsets);
        return status;
    }
}

static string_t *de_derivatives_to_standard_text(const string_t *source, bool partial)
{
    const char *text = source ? string_c_str(source) : "";
    string_t *out = string_new();
    size_t index = 0u;

    if (!out)
        return NULL;
    while (text[index]) {
        size_t suffix_end;
        size_t order = 0u;
        size_t dependent_start;
        size_t dependent_end;

        if (text[index] != 'D' || de_derivative_letter_length(text + index + 1u) == 0u) {
            if (de_append_next_character(out, text, &index) != 0)
                goto fail;
            continue;
        }

        suffix_end = index + 1u;
        {
            size_t length;

            while ((length = de_derivative_letter_length(text + suffix_end)) > 0u) {
                suffix_end += length;
                order++;
            }
        }
        if (text[suffix_end] != '(') {
            if (de_append_next_character(out, text, &index) != 0)
                goto fail;
            continue;
        }

        dependent_start = suffix_end + 1u;
        dependent_end = dependent_start;
        while (text[dependent_end] && text[dependent_end] != ')' && text[dependent_end] != '(' &&
               text[dependent_end] != ' ' && text[dependent_end] != '\t' && text[dependent_end] != '\r' &&
               text[dependent_end] != '\n')
            ++dependent_end;
        if (dependent_end == dependent_start || text[dependent_end] != ')') {
            if (de_append_next_character(out, text, &index) != 0)
                goto fail;
            continue;
        }

        if (de_append_standard_derivative(out, text + index + 1u, suffix_end - index - 1u, order,
                                          text + dependent_start, dependent_end - dependent_start, partial) != 0)
            goto fail;
        index = dependent_end + 1u;
    }
    return out;

fail:
    string_free(out);
    return NULL;
}

static string_t *de_conditions_to_text(const diffequ_t *de, bool partial_derivatives)
{
    string_t *out = string_new();

    if (!out)
        return NULL;

    for (size_t i = 0u; i < de->condition_count; ++i) {
        string_t *condition = de_derivatives_to_standard_text(de->condition_texts[i], partial_derivatives);

        if (!condition)
            goto fail;
        if (i > 0u && string_append_cstr(out, ", ") != 0)
            goto condition_fail;
        if (string_append_string(out, condition) != 0)
            goto condition_fail;
        string_free(condition);
        continue;

    condition_fail:
        string_free(condition);
        goto fail;
    }
    return out;

fail:
    string_free(out);
    return NULL;
}

static string_t *de_to_expression_text(const diffequ_t *de)
{
    string_t *canonical_equation;
    string_t *display_equation;
    string_t *equation;
    string_t *conditions;
    string_t *out;
    bool partial_derivatives;

    if (!de || !de->equation)
        return string_new_with("NULL");

    partial_derivatives = de->independent_count > 1u || de->partial_derivative_input;
    canonical_equation = de->differential_form_input && string_length(de->differential_form_text) > 0u
                             ? string_new_with(string_c_str(de->differential_form_text))
                         : string_length(de->equation_text) > 0u ? string_new_with(string_c_str(de->equation_text))
                                                                 : equ_to_text(de->equation, style_UNBOUND);
    display_equation = de->differential_form_input ? de_greek_aliases_to_symbols(canonical_equation)
                                                   : string_clone(canonical_equation);
    string_free(canonical_equation);
    equation = de_derivatives_to_standard_text(display_equation, partial_derivatives);
    string_free(display_equation);
    conditions = de_conditions_to_text(de, partial_derivatives);
    if (!equation || !conditions) {
        string_free(conditions);
        string_free(equation);
        return NULL;
    }

    out = string_sprintf("{ %S | %S; %S; %S }", equation, de->independent_text, de->constant_text, conditions);

    string_free(conditions);
    string_free(equation);
    return out;
}

static char *de_expr_to_unbound_TeX(const expr_t *expr, bool partial_derivatives)
{
    const char *symbol_name;
    char *tex;

    symbol_name = expr_symbol_name(expr);
    if (symbol_name) {
        expr_t *symbol = expr_new_named_var(NUM_NAN, symbol_name);

        tex = symbol ? expr_to_TeX_body_wrapped(symbol, de_TeX_line_limit) : NULL;
        expr_free(symbol);
        return tex;
    }
    return partial_derivatives ? expr_to_TeX_body_wrapped_with_partials(expr, de_TeX_line_limit)
                               : expr_to_TeX_body_wrapped_with_totals(expr, de_TeX_line_limit);
}

static expr_t *de_TeX_binomial_coefficient(size_t n, size_t k)
{
    number_t n_value = num_create_from_long((long)n);
    number_t k_value = num_create_from_long((long)k);
    number_t value = num_binomial(n_value, k_value);
    expr_t *coefficient = num_is_finite(value) ? expr_new_const(value) : NULL;

    num_destroy(&value);
    num_destroy(&k_value);
    num_destroy(&n_value);
    return coefficient;
}

static string_t *de_repeated_quadratic_to_TeX(const diffequ_t *de)
{
    const expr_t *square_base = de ? de->repeated_quadratic_square : NULL;
    expr_t *dependent = NULL;
    char *dependent_TeX = NULL;
    char *independent_TeX = NULL;
    string_t *out = NULL;
    bool negative_square;
    bool first = true;

    if (!de || de->repeated_quadratic_power < 2u || !square_base || !de->repeated_quadratic_dependent ||
        de->independent_count != 1u)
        return NULL;
    negative_square = expr_match_neg_expr(de->repeated_quadratic_square, &square_base);
    dependent = expr_new_named_var(NUM_NAN, de->repeated_quadratic_dependent);
    dependent_TeX = dependent ? de_expr_to_unbound_TeX(dependent, false) : NULL;
    independent_TeX = de_expr_to_unbound_TeX(de->independent_vars[0], false);
    out = dependent_TeX && independent_TeX ? string_new() : NULL;
    if (!out)
        goto fail;

    for (size_t j = de->repeated_quadratic_power + 1u; j > 0u; --j) {
        size_t derivative_power = j - 1u;
        size_t square_power = de->repeated_quadratic_power - derivative_power;
        expr_t *binomial = de_TeX_binomial_coefficient(de->repeated_quadratic_power, derivative_power);
        expr_t *power = expr_pow_long(square_base, (long)square_power);
        expr_t *coefficient = binomial && power ? expr_mul(binomial, power) : NULL;
        expr_t *display = NULL;
        const expr_t *magnitude;
        bool subtract;
        bool unit;
        number_t value = num_new();
        char *coefficient_TeX = NULL;
        size_t order = 2u * derivative_power;

        if (coefficient && negative_square && (square_power & 1u) != 0u) {
            expr_t *negative = expr_neg(coefficient);

            expr_free(coefficient);
            coefficient = negative;
        }
        display = coefficient ? expr_display_simplified(coefficient) : NULL;
        expr_free(coefficient);
        expr_free(power);
        expr_free(binomial);
        if (!display)
            goto fail;

        magnitude = display;
        subtract = expr_match_neg_expr(display, &magnitude);
        unit = expr_match_const_value(magnitude, &value) && num_eq(value, NUM_ONE);
        num_destroy(&value);
        coefficient_TeX = unit ? NULL : de_expr_to_unbound_TeX(magnitude, false);
        if (!unit && !coefficient_TeX) {
            expr_free(display);
            goto fail;
        }

        if (first) {
            if (subtract && string_append_char(out, '-') != 0)
                goto term_fail;
        } else if (string_append_cstr(out, subtract ? " - " : " + ") != 0) {
            goto term_fail;
        }
        if (!unit && (string_append_cstr(out, coefficient_TeX) != 0 || string_append_cstr(out, " \\cdot ") != 0))
            goto term_fail;
        if (order == 0u) {
            if (string_append_cstr(out, dependent_TeX) != 0)
                goto term_fail;
        } else if (string_append_format(out, "\\frac{d^{%zu} %s}{d %s^{%zu}}", order, dependent_TeX, independent_TeX,
                                        order) < 0) {
            goto term_fail;
        }
        free(coefficient_TeX);
        expr_free(display);
        first = false;
        continue;

    term_fail:
        free(coefficient_TeX);
        expr_free(display);
        goto fail;
    }
    if (string_append_cstr(out, " = 0") != 0)
        goto fail;
    free(independent_TeX);
    free(dependent_TeX);
    expr_free(dependent);
    return out;

fail:
    string_free(out);
    free(independent_TeX);
    free(dependent_TeX);
    expr_free(dependent);
    return NULL;
}

static const expr_t *de_first_formal_derivative(const expr_t *expr)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *found;

    if (!expr)
        return NULL;
    if (expr_is_formal_derivative(expr))
        return expr;
    if (!expr_child_exprs(expr, &left, &right))
        return NULL;
    found = de_first_formal_derivative(left);
    return found ? found : de_first_formal_derivative(right);
}

static int de_append_differential_term(string_t *out, const expr_t *coefficient, const expr_t *variable, bool first)
{
    const expr_t *display = coefficient;
    const expr_t *negative = NULL;
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool is_sub = false;
    number_t value = num_new();
    bool subtract = expr_match_neg_expr(coefficient, &negative);
    bool unit;
    bool additive;
    char *coefficient_TeX = NULL;
    char *variable_TeX = NULL;
    int status = -1;

    if (subtract)
        display = negative;
    unit = expr_match_const_value(display, &value) && num_eq(value, NUM_ONE);
    num_destroy(&value);
    additive = expr_match_add_sub_expr(display, &left, &right, &is_sub);
    if (!unit)
        coefficient_TeX = de_expr_to_unbound_TeX(display, false);
    variable_TeX = de_expr_to_unbound_TeX(variable, false);
    if ((!unit && !coefficient_TeX) || !variable_TeX)
        goto cleanup;

    if (first) {
        if (subtract && string_append_char(out, '-') != 0)
            goto cleanup;
    } else if (string_append_cstr(out, subtract ? " - " : " + ") != 0) {
        goto cleanup;
    }
    if (!unit) {
        if (additive && string_append_cstr(out, "\\left(") != 0)
            goto cleanup;
        if (string_append_cstr(out, coefficient_TeX) != 0)
            goto cleanup;
        if (additive && string_append_cstr(out, "\\right)") != 0)
            goto cleanup;
        if (string_append_cstr(out, "\\,") != 0)
            goto cleanup;
    }
    if (string_append_format(out, "d%s", variable_TeX) < 0)
        goto cleanup;
    status = 0;

cleanup:
    free(variable_TeX);
    free(coefficient_TeX);
    return status;
}

static string_t *de_differential_form_to_TeX(const diffequ_t *de)
{
    const expr_t *derivative;
    const expr_t *dependent;
    const expr_t *independent;
    expr_t *residual = NULL;
    expr_t *dependent_coefficient = NULL;
    expr_t *independent_coefficient = NULL;
    string_t *out = NULL;

    if (!de || !de->differential_form_input || de->independent_count != 1u)
        return NULL;
    independent = de->independent_vars[0];
    residual = equ_residual(de->equation);
    derivative = de_first_formal_derivative(residual);
    dependent = derivative ? expr_formal_derivative_dependent(derivative) : NULL;
    if (!derivative || !dependent ||
        !de_linear_decompose(residual, derivative, &dependent_coefficient, &independent_coefficient))
        goto cleanup;

    out = string_new();
    if (!out || de_append_differential_term(out, dependent_coefficient, dependent, true) != 0 ||
        de_append_differential_term(out, independent_coefficient, independent, false) != 0 ||
        string_append_cstr(out, " = 0") != 0) {
        string_free(out);
        out = NULL;
    }

cleanup:
    expr_free(independent_coefficient);
    expr_free(dependent_coefficient);
    expr_free(residual);
    return out;
}

static string_t *de_to_TeX(const diffequ_t *de)
{
    char *lhs;
    char *rhs;
    string_t *out;

    if (!de || !de->equation)
        return string_new_with("NULL");

    if (de->repeated_quadratic_power >= 2u) {
        out = de_repeated_quadratic_to_TeX(de);
        if (out)
            return out;
    }

    if (de->differential_form_input) {
        out = de_differential_form_to_TeX(de);
        if (out)
            return out;
    }

    lhs = de_expr_to_unbound_TeX(equ_lhs(de->equation), de->independent_count > 1u || de->partial_derivative_input);
    rhs = de_expr_to_unbound_TeX(equ_rhs(de->equation), de->independent_count > 1u || de->partial_derivative_input);
    out = lhs && rhs ? string_sprintf("%s = %s", lhs, rhs) : NULL;
    free(rhs);
    free(lhs);
    return out;
}

string_t *de_to_text(const diffequ_t *de, style_t style)
{
    if (!de)
        return string_new_with("NULL");
    if (style == style_UNBOUND)
        return equ_to_text(de->equation, style_UNBOUND);
    if (style == style_LATEX)
        return de_to_TeX(de);
    return de_to_expression_text(de);
}

char *de_to_string(const diffequ_t *de, style_t style)
{
    string_t *text = de_to_text(de, style);
    char *out = text ? strdup(string_c_str(text)) : NULL;

    string_free(text);
    return out;
}
