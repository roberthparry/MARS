#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#include "ustring.h"

typedef struct {
    size_t start;
    size_t end;
} expr_series_span_t;

typedef enum {
    EXPR_SERIES_MODEL_NONE,
    EXPR_SERIES_MODEL_POLYNOMIAL,
    EXPR_SERIES_MODEL_GEOMETRIC,
    EXPR_SERIES_MODEL_INVERSE_INDEX_POWER,
} expr_series_model_kind_t;

typedef struct {
    number_t sum;
    number_t *polynomial;
    size_t polynomial_count;
    number_t geometric_first;
    number_t geometric_ratio;
    expr_t *symbolic_geometric_first;
    expr_t *symbolic_geometric_ratio;
    number_t inverse_index_first;
    int inverse_index_exponent;
    expr_t *inverse_index_numerator;
    expr_t *symbolic_exponent;
    bool symbolic_direct_power;
    size_t endpoint;
    expr_t *symbolic_endpoint;
    expr_series_model_kind_t kind;
} expr_series_result_t;

static void expr_series_destroy_numbers(number_t *values, size_t count);

static void expr_series_result_init(expr_series_result_t *result)
{
    *result = (expr_series_result_t){
        .sum = NUM_NAN,
        .geometric_first = NUM_NAN,
        .geometric_ratio = NUM_NAN,
        .inverse_index_first = NUM_NAN,
    };
}

static void expr_series_result_clear(expr_series_result_t *result)
{
    if (!result)
        return;
    num_destroy(&result->sum);
    num_destroy(&result->geometric_first);
    num_destroy(&result->geometric_ratio);
    expr_free(result->symbolic_geometric_first);
    expr_free(result->symbolic_geometric_ratio);
    num_destroy(&result->inverse_index_first);
    expr_free(result->inverse_index_numerator);
    expr_free(result->symbolic_exponent);
    expr_free(result->symbolic_endpoint);
    expr_series_destroy_numbers(result->polynomial, result->polynomial_count);
    expr_series_result_init(result);
}

static expr_series_span_t expr_series_trim(const char *text, expr_series_span_t span)
{
    while (span.start < span.end && isspace((unsigned char)text[span.start]))
        ++span.start;
    while (span.end > span.start && isspace((unsigned char)text[span.end - 1u]))
        --span.end;
    return span;
}

static char *expr_series_copy_span(const char *text, expr_series_span_t span)
{
    const size_t length = span.end - span.start;
    char *copy = malloc(length + 1u);

    if (!copy)
        return NULL;
    memcpy(copy, text + span.start, length);
    copy[length] = '\0';
    return copy;
}

static char *expr_series_replace_literal_infinity(const char *text, bool *replaced_out)
{
    static const char infinity[] = "inf";
    static const char placeholder[] = "q";
    const size_t infinity_length = sizeof(infinity) - 1u;
    const size_t placeholder_length = sizeof(placeholder) - 1u;
    const size_t text_length = strlen(text);
    const char *match = text;
    char *replaced;
    size_t prefix_length;

    *replaced_out = false;
    while ((match = strstr(match, infinity)) != NULL) {
        const bool starts_token = match == text || (!isalnum((unsigned char)match[-1]) && match[-1] != '_');
        const char after = match[infinity_length];
        const bool ends_token = after == '\0' || (!isalnum((unsigned char)after) && after != '_');

        if (starts_token && ends_token)
            break;
        match += infinity_length;
    }
    if (!match)
        return strdup(text);
    prefix_length = (size_t)(match - text);
    replaced = malloc(text_length - infinity_length + placeholder_length + 1u);
    if (!replaced)
        return NULL;
    memcpy(replaced, text, prefix_length);
    memcpy(replaced + prefix_length, placeholder, placeholder_length);
    memcpy(replaced + prefix_length + placeholder_length, match + infinity_length,
           text_length - prefix_length - infinity_length + 1u);
    *replaced_out = true;
    return replaced;
}

static bool expr_series_is_ellipsis(const char *text, expr_series_span_t span)
{
    char *term;
    bool match;

    span = expr_series_trim(text, span);
    term = expr_series_copy_span(text, span);
    if (!term)
        return false;
    match = strcmp(term, "...") == 0 || strcmp(term, "…") == 0;
    free(term);
    return match;
}

static bool expr_series_exponent_plus(const char *text, size_t length, size_t pos)
{
    return pos > 1u && pos + 1u < length && (text[pos - 1u] == 'e' || text[pos - 1u] == 'E') &&
           (isdigit((unsigned char)text[pos - 2u]) || text[pos - 2u] == '.') &&
           isdigit((unsigned char)text[pos + 1u]);
}

static bool expr_series_split_terms(const char *text, expr_series_span_t **terms_out, size_t *count_out)
{
    const size_t length = strlen(text);
    expr_series_span_t *terms = calloc(length + 1u, sizeof(*terms));
    size_t count = 0u;
    size_t start = 0u;
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;

    if (!terms)
        return false;

    for (size_t i = 0u; i < length; ++i) {
        switch (text[i]) {
            case '(':
                ++paren_depth;
                break;
            case ')':
                if (paren_depth > 0)
                    --paren_depth;
                break;
            case '[':
                ++bracket_depth;
                break;
            case ']':
                if (bracket_depth > 0)
                    --bracket_depth;
                break;
            case '{':
                ++brace_depth;
                break;
            case '}':
                if (brace_depth > 0)
                    --brace_depth;
                break;
            case '+':
                if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0 &&
                    !expr_series_exponent_plus(text, length, i)) {
                    terms[count++] = (expr_series_span_t){start, i};
                    start = i + 1u;
                }
                break;
            default:
                break;
        }
    }

    terms[count++] = (expr_series_span_t){start, length};
    *terms_out = terms;
    *count_out = count;
    return true;
}

static bool expr_series_parse_term(const char *text, expr_series_span_t span, number_t *coefficient_out,
                                  char **factor_out)
{
    char *term;
    char *factor = NULL;
    expr_t *expression = NULL;
    expr_bindings_t *bindings = NULL;
    const expr_t *base = NULL;
    number_t coefficient = num_new();
    bool parsed = false;

    *coefficient_out = NUM_NAN;
    *factor_out = NULL;
    span = expr_series_trim(text, span);
    term = expr_series_copy_span(text, span);
    if (!term || !*term) {
        free(term);
        return false;
    }
    expression = expr_from_string(term, &bindings);
    free(term);
    if (!expression)
        goto cleanup;

    if (!bindings || expr_bindings_count(bindings) == 0u) {
        number_t value = expr_eval(expression);

        if (num_is_exact(value) && num_is_finite(value)) {
            num_destroy(&coefficient);
            coefficient = value;
            factor = strdup("");
        } else {
            num_destroy(&value);
        }
    }
    if (factor) {
        /* The complete term is an exact numerical coefficient. */
    } else if (expr_match_const_value(expression, &coefficient)) {
        factor = strdup("");
    } else if (expr_match_scaled_expr(expression, &coefficient, &base) && base && base != expression) {
        factor = expr_to_string(base, style_UNBOUND);
    } else {
        num_destroy(&coefficient);
        coefficient = num_clone(NUM_ONE);
        factor = expr_to_string(expression, style_UNBOUND);
    }
    parsed = factor && num_is_exact(coefficient) && num_is_finite(coefficient);
    if (!parsed)
        goto cleanup;

    *coefficient_out = coefficient;
    *factor_out = factor;
    coefficient = NUM_NAN;
    factor = NULL;

cleanup:
    free(factor);
    num_destroy(&coefficient);
    expr_bindings_free(bindings);
    expr_free(expression);
    return parsed;
}

static bool expr_series_parse_symbolic_power_endpoint(const char *text, expr_series_span_t span,
                                                      expr_t **numerator_out, expr_t **endpoint_out,
                                                      expr_t **symbolic_exponent_out, int *exponent_out,
                                                      bool *direct_power_out)
{
    char *term_text;
    expr_t *expression = NULL;
    const expr_t *numerator = NULL;
    const expr_t *denominator = NULL;
    const expr_t *endpoint = NULL;
    const expr_t *symbolic_exponent = NULL;
    number_t exponent = NUM_NAN;
    number_t endpoint_value = NUM_NAN;
    int exponent_value = 1;
    bool direct_power = false;
    bool endpoint_is_positive_infinity = false;
    bool endpoint_is_symbol = false;
    bool replaced_literal_infinity = false;
    bool literal_positive_infinity = false;
    bool matched = false;

    *numerator_out = NULL;
    *endpoint_out = NULL;
    *symbolic_exponent_out = NULL;
    *exponent_out = 0;
    *direct_power_out = false;
    span = expr_series_trim(text, span);
    term_text = expr_series_copy_span(text, span);
    if (!term_text)
        return false;
    {
        char *parse_text = expr_series_replace_literal_infinity(term_text, &replaced_literal_infinity);

        expression = parse_text ? expr_from_string(parse_text, NULL) : NULL;
        free(parse_text);
    }
    free(term_text);
    if (!expression)
        goto cleanup;

    if (expr_match_div_expr(expression, &numerator, &denominator) && numerator && denominator) {
        endpoint = denominator;
        if (expr_match_pow_const(denominator, &endpoint, &exponent)) {
            exponent_value = 0;
            for (int candidate = 1; candidate <= 32; ++candidate) {
                number_t candidate_value = num_create_from_long(candidate);
                const bool equal = num_eq(exponent, candidate_value);

                num_destroy(&candidate_value);
                if (equal) {
                    exponent_value = candidate;
                    break;
                }
            }
        } else if (expr_match_pow_expr(denominator, &endpoint, &symbolic_exponent)) {
            number_t symbolic_value = expr_eval(symbolic_exponent);

            exponent_value = 0;
            if ((!expr_is_variable(symbolic_exponent) && !expr_is_named_const(symbolic_exponent)) ||
                !num_is_nan(symbolic_value)) {
                num_destroy(&symbolic_value);
                goto cleanup;
            }
            num_destroy(&symbolic_value);
        }
    } else if (expr_match_pow_expr(expression, &endpoint, &symbolic_exponent)) {
        const expr_t *positive_exponent = NULL;
        number_t symbolic_value = expr_eval(symbolic_exponent);

        numerator = EXPR_ONE;
        exponent_value = 0;
        if (expr_match_neg_expr(symbolic_exponent, &positive_exponent) && positive_exponent) {
            symbolic_exponent = positive_exponent;
            direct_power = false;
            num_destroy(&symbolic_value);
            symbolic_value = expr_eval(symbolic_exponent);
        } else {
            direct_power = true;
        }
        if ((!expr_is_variable(symbolic_exponent) && !expr_is_named_const(symbolic_exponent)) ||
            !num_is_nan(symbolic_value)) {
            num_destroy(&symbolic_value);
            goto cleanup;
        }
        num_destroy(&symbolic_value);
    }
    if (endpoint && replaced_literal_infinity) {
        char *endpoint_text = expr_to_string(endpoint, style_UNBOUND);

        literal_positive_infinity = endpoint_text && strcmp(endpoint_text, "q") == 0;
        free(endpoint_text);
    }
    if (endpoint)
        endpoint_value = expr_eval(endpoint);
    endpoint_is_positive_infinity = literal_positive_infinity ||
                                    (num_is_real(endpoint_value) && num_is_inf(endpoint_value) &&
                                     num_get_sign(endpoint_value) > 0);
    endpoint_is_symbol = endpoint && (expr_is_variable(endpoint) || expr_is_named_const(endpoint)) &&
                         num_is_nan(endpoint_value);
    if ((!symbolic_exponent && exponent_value == 0) || !endpoint ||
        (!endpoint_is_symbol && !endpoint_is_positive_infinity))
        goto cleanup;

    *numerator_out = expr_clone(numerator);
    *endpoint_out = literal_positive_infinity ? expr_from_string("inf", NULL) : expr_clone(endpoint);
    *symbolic_exponent_out = expr_clone(symbolic_exponent);
    *exponent_out = exponent_value;
    *direct_power_out = direct_power;
    matched = *numerator_out && *endpoint_out && (!symbolic_exponent || *symbolic_exponent_out);
    if (!matched) {
        expr_free(*numerator_out);
        expr_free(*endpoint_out);
        expr_free(*symbolic_exponent_out);
        *numerator_out = NULL;
        *endpoint_out = NULL;
        *symbolic_exponent_out = NULL;
        *exponent_out = 0;
        *direct_power_out = false;
    }

cleanup:
    num_destroy(&endpoint_value);
    num_destroy(&exponent);
    expr_free(expression);
    return matched;
}

static bool expr_series_parse_symbolic_geometric_endpoint(const char *text, expr_series_span_t span,
                                                          expr_t **term_count_out, number_t *ratio_out)
{
    char *term_text;
    expr_t *expression = NULL;
    const expr_t *base = NULL;
    const expr_t *exponent = NULL;
    const expr_t *term_count = NULL;
    const expr_t *offset = NULL;
    number_t ratio = NUM_NAN;
    number_t offset_value = NUM_NAN;
    number_t term_count_value = NUM_NAN;
    bool subtract = false;
    bool matched = false;

    *term_count_out = NULL;
    *ratio_out = NUM_NAN;
    span = expr_series_trim(text, span);
    term_text = expr_series_copy_span(text, span);
    if (!term_text)
        return false;
    expression = expr_from_string(term_text, NULL);
    free(term_text);
    if (!expression || !expr_match_pow_expr(expression, &base, &exponent) || !base || !exponent ||
        !expr_match_add_sub_expr(exponent, &term_count, &offset, &subtract) || !subtract || !term_count || !offset)
        goto cleanup;

    ratio = expr_eval(base);
    offset_value = expr_eval(offset);
    term_count_value = expr_eval(term_count);
    if (!num_is_exact(ratio) || !num_is_finite(ratio) || num_is_zero(ratio) || num_is_one(ratio) ||
        !num_is_one(offset_value) || (!expr_is_variable(term_count) && !expr_is_named_const(term_count)) ||
        !num_is_nan(term_count_value))
        goto cleanup;

    *term_count_out = expr_clone(term_count);
    *ratio_out = num_clone(ratio);
    matched = *term_count_out != NULL;

cleanup:
    if (!matched) {
        expr_free(*term_count_out);
        *term_count_out = NULL;
        num_destroy(ratio_out);
        *ratio_out = NUM_NAN;
    }
    num_destroy(&term_count_value);
    num_destroy(&offset_value);
    num_destroy(&ratio);
    expr_free(expression);
    return matched;
}

static bool expr_series_geometric_prefix_matches(const number_t *coefficients, size_t coefficient_count,
                                                 number_t ratio)
{
    if (!coefficients || coefficient_count < 2u || !num_is_one(coefficients[0]))
        return false;
    for (size_t i = 1u; i < coefficient_count; ++i) {
        number_t expected = num_mul(coefficients[i - 1u], ratio);
        const bool matches = num_is_finite(expected) && num_eq(expected, coefficients[i]);

        num_destroy(&expected);
        if (!matches)
            return false;
    }
    return true;
}

static bool expr_series_expr_text_equal(const expr_t *left, const expr_t *right)
{
    char *left_text;
    char *right_text;
    bool equal;

    if (!left || !right)
        return false;
    left_text = expr_to_string(left, style_UNBOUND);
    right_text = expr_to_string(right, style_UNBOUND);
    equal = left_text && right_text && strcmp(left_text, right_text) == 0;
    free(right_text);
    free(left_text);
    return equal;
}

static expr_t *expr_series_simplify_owned(expr_t *expression)
{
    expr_t *simplified = expression ? expr_display_simplified(expression) : NULL;

    expr_free(expression);
    return simplified;
}

static bool expr_series_simplified_equal(const expr_t *left, const expr_t *right)
{
    expr_t *left_simplified = left ? expr_display_simplified(left) : NULL;
    expr_t *right_simplified = right ? expr_display_simplified(right) : NULL;
    bool equal = expr_series_expr_text_equal(left_simplified, right_simplified);

    expr_free(right_simplified);
    expr_free(left_simplified);
    return equal;
}

static expr_t *expr_series_parse_term_expression(const char *text, expr_series_span_t span)
{
    char *term_text;
    expr_t *term;

    span = expr_series_trim(text, span);
    term_text = expr_series_copy_span(text, span);
    if (!term_text)
        return NULL;
    term = expr_from_string(term_text, NULL);
    free(term_text);
    return term;
}

static expr_t *expr_series_parse_quotient_expression(const char *text, expr_series_span_t numerator_span,
                                                     expr_series_span_t denominator_span)
{
    char *numerator_text = expr_series_copy_span(text, expr_series_trim(text, numerator_span));
    char *denominator_text = expr_series_copy_span(text, expr_series_trim(text, denominator_span));
    string_t *quotient_text = string_new();
    expr_t *quotient = NULL;
    expr_t *simplified = NULL;

    if (numerator_text && denominator_text && quotient_text &&
        string_append_format(quotient_text, "(%s)/(%s)", numerator_text, denominator_text) >= 0)
        quotient = expr_from_string(string_c_str(quotient_text), NULL);
    simplified = quotient ? expr_display_simplified(quotient) : NULL;
    expr_free(quotient);
    string_free(quotient_text);
    free(denominator_text);
    free(numerator_text);
    return simplified;
}

static expr_t *expr_series_geometric_expected_term(const expr_t *first, const expr_t *ratio, size_t exponent)
{
    expr_t *power;
    expr_t *term;

    if (exponent == 0u)
        return expr_clone(first);
    power = exponent == 1u ? expr_clone(ratio) : expr_pow_long(ratio, (long)exponent);
    term = power ? expr_mul_simplify_owned(expr_clone(first), power) : NULL;
    return term;
}

static bool expr_series_parse_symbolic_geometric(const char *text, const expr_series_span_t *terms,
                                                 size_t ellipsis_index, expr_series_result_t *result,
                                                 size_t *series_start_out)
{
    expr_t *endpoint = NULL;
    bool matched = false;

    if (!text || !terms || !result || !series_start_out || ellipsis_index < 3u)
        return false;
    endpoint = expr_series_parse_term_expression(text, terms[ellipsis_index + 1u]);
    if (!endpoint)
        return false;

    for (size_t start = 0u; start + 2u < ellipsis_index; ++start) {
        expr_t *first = expr_series_parse_term_expression(text, terms[start]);
        expr_t *second = expr_series_parse_term_expression(text, terms[start + 1u]);
        expr_t *ratio = expr_series_parse_quotient_expression(text, terms[start + 1u], terms[start]);
        expr_t *endpoint_power =
            expr_series_parse_quotient_expression(text, terms[ellipsis_index + 1u], terms[start]);
        const expr_t *endpoint_base = NULL;
        const expr_t *endpoint_exponent = NULL;
        number_t endpoint_exponent_value = NUM_NAN;
        bool prefix_matches = first && second && ratio && endpoint_power &&
                              expr_match_pow_expr(endpoint_power, &endpoint_base, &endpoint_exponent) &&
                              endpoint_base && endpoint_exponent && expr_series_simplified_equal(endpoint_base, ratio);

        if (prefix_matches)
            endpoint_exponent_value = expr_eval(endpoint_exponent);
        prefix_matches = prefix_matches && num_is_nan(endpoint_exponent_value);
        for (size_t position = start; prefix_matches && position < ellipsis_index; ++position) {
            expr_t *actual = expr_series_parse_term_expression(text, terms[position]);
            expr_t *expected = expr_series_geometric_expected_term(first, ratio, position - start);

            prefix_matches = actual && expected && expr_series_simplified_equal(actual, expected);
            expr_free(expected);
            expr_free(actual);
        }
        if (prefix_matches) {
            expr_t *term_count = expr_add(endpoint_exponent, EXPR_ONE);

            result->symbolic_geometric_first = expr_clone(first);
            result->symbolic_geometric_ratio = expr_clone(ratio);
            result->symbolic_endpoint = expr_series_simplify_owned(term_count);
            result->kind = EXPR_SERIES_MODEL_GEOMETRIC;
            *series_start_out = start;
            matched = result->symbolic_geometric_first && result->symbolic_geometric_ratio &&
                      result->symbolic_endpoint;
        }
        num_destroy(&endpoint_exponent_value);
        expr_free(endpoint_power);
        expr_free(ratio);
        expr_free(second);
        expr_free(first);
        if (matched)
            break;
        expr_free(result->symbolic_geometric_first);
        expr_free(result->symbolic_geometric_ratio);
        expr_free(result->symbolic_endpoint);
        result->symbolic_geometric_first = NULL;
        result->symbolic_geometric_ratio = NULL;
        result->symbolic_endpoint = NULL;
        result->kind = EXPR_SERIES_MODEL_NONE;
    }

    expr_free(endpoint);
    return matched;
}

static bool expr_series_exact_positive_long(const expr_t *expression, long *value_out)
{
    number_t value;
    string_t *text;
    char *end = NULL;
    long value_long;

    if (!expression || !value_out)
        return false;
    value = expr_eval(expression);
    if (!num_is_exact(value) || !num_is_real(value) || !num_is_integer(value) || !num_is_finite(value) ||
        num_sign(value) <= 0) {
        num_destroy(&value);
        return false;
    }
    text = num_to_string(value);
    num_destroy(&value);
    if (!text)
        return false;
    value_long = strtol(string_c_str(text), &end, 10);
    if (!end || *end != '\0' || value_long <= 0L) {
        string_free(text);
        return false;
    }
    string_free(text);
    *value_out = value_long;
    return true;
}

static bool expr_series_exact_reciprocal_index(const expr_t *expression, long *index_out)
{
    number_t value;
    number_t reciprocal;
    expr_t *reciprocal_expr;
    bool matched;

    if (!expression || !index_out)
        return false;
    value = expr_eval(expression);
    reciprocal = num_div(NUM_ONE, value);
    reciprocal_expr = expr_new_const(reciprocal);
    matched = reciprocal_expr && expr_series_exact_positive_long(reciprocal_expr, index_out);
    expr_free(reciprocal_expr);
    num_destroy(&reciprocal);
    num_destroy(&value);
    return matched;
}

static bool expr_series_symbolic_power_term_matches(const char *text, expr_series_span_t span, long index,
                                                    const expr_t *numerator, const expr_t *exponent,
                                                    bool direct_power)
{
    char *term_text;
    expr_t *expression;
    const expr_t *term_numerator = NULL;
    const expr_t *denominator = NULL;
    const expr_t *term_index = NULL;
    const expr_t *term_exponent = NULL;
    long term_index_value = 0L;
    bool matched;

    span = expr_series_trim(text, span);
    term_text = expr_series_copy_span(text, span);
    if (!term_text)
        return false;
    expression = expr_from_string(term_text, NULL);
    free(term_text);
    if (!expression)
        return false;

    if (index == 1L) {
        matched = expr_series_expr_text_equal(expression, numerator);
    } else if (direct_power && expr_match_pow_expr(expression, &term_index, &term_exponent)) {
        number_t numerator_value = expr_eval(numerator);

        matched = num_is_one(numerator_value) && term_index && term_exponent &&
                  expr_series_exact_positive_long(term_index, &term_index_value) && term_index_value == index &&
                  expr_series_expr_text_equal(term_exponent, exponent);
        num_destroy(&numerator_value);
    } else if (!direct_power && expr_match_div_expr(expression, &term_numerator, &denominator) && term_numerator &&
               denominator &&
               expr_match_pow_expr(denominator, &term_index, &term_exponent)) {
        matched = term_index && term_exponent && expr_series_exact_positive_long(term_index, &term_index_value) &&
                  term_index_value == index && expr_series_expr_text_equal(term_numerator, numerator) &&
                  expr_series_expr_text_equal(term_exponent, exponent);
    } else if (!direct_power && expr_match_pow_expr(expression, &term_index, &term_exponent)) {
        const expr_t *positive_exponent = NULL;
        number_t numerator_value = expr_eval(numerator);

        matched = num_is_one(numerator_value) && term_index && term_exponent &&
                  ((expr_series_exact_reciprocal_index(term_index, &term_index_value) &&
                    expr_series_expr_text_equal(term_exponent, exponent)) ||
                   (expr_match_neg_expr(term_exponent, &positive_exponent) && positive_exponent &&
                    expr_series_exact_positive_long(term_index, &term_index_value) &&
                    expr_series_expr_text_equal(positive_exponent, exponent))) &&
                  term_index_value == index;
        num_destroy(&numerator_value);
    } else {
        matched = false;
    }
    expr_free(expression);
    return matched;
}

static bool expr_series_symbolic_power_prefix_matches(const char *text, const expr_series_span_t *terms,
                                                      size_t ellipsis_index, const expr_t *numerator,
                                                      const expr_t *exponent, bool direct_power,
                                                      size_t *series_start_out)
{
    char *term_text;
    expr_t *expression;
    const expr_t *term_numerator = NULL;
    const expr_t *denominator = NULL;
    const expr_t *term_index = NULL;
    const expr_t *term_exponent = NULL;
    long index = 0L;
    size_t position;
    bool matched = false;

    if (!text || !terms || ellipsis_index < 2u || !numerator || !exponent || !series_start_out)
        return false;
    term_text = expr_series_copy_span(text, expr_series_trim(text, terms[ellipsis_index - 1u]));
    if (!term_text)
        return false;
    expression = expr_from_string(term_text, NULL);
    free(term_text);
    if (!expression)
        return false;
    if (direct_power && expr_match_pow_expr(expression, &term_index, &term_exponent)) {
        if (!expr_series_exact_positive_long(term_index, &index))
            goto cleanup;
    } else if (!direct_power && expr_match_div_expr(expression, &term_numerator, &denominator) && denominator &&
        expr_match_pow_expr(denominator, &term_index, &term_exponent)) {
        if (!expr_series_exact_positive_long(term_index, &index))
            goto cleanup;
    } else if (!direct_power && expr_match_pow_expr(expression, &term_index, &term_exponent)) {
        const expr_t *positive_exponent = NULL;

        if (expr_match_neg_expr(term_exponent, &positive_exponent) && positive_exponent) {
            if (!expr_series_exact_positive_long(term_index, &index))
                goto cleanup;
        } else if (!expr_series_exact_reciprocal_index(term_index, &index)) {
            goto cleanup;
        }
    } else {
        goto cleanup;
    }
    if (index < 2L)
        goto cleanup;
    expr_free(expression);
    expression = NULL;

    position = ellipsis_index;
    while (index >= 1L && position > 0u) {
        --position;
        if (!expr_series_symbolic_power_term_matches(text, terms[position], index, numerator, exponent,
                                                     direct_power))
            return false;
        --index;
    }
    if (index != 0L)
        return false;
    *series_start_out = position;
    matched = true;

cleanup:
    expr_free(expression);
    return matched;
}

static bool expr_series_symbolic_numerator_matches(number_t first, const char *factor, const expr_t *numerator)
{
    expr_t *coefficient = NULL;
    expr_t *factor_expr = NULL;
    expr_t *expected = NULL;
    expr_t *expected_simplified = NULL;
    expr_t *numerator_simplified = NULL;
    char *expected_text = NULL;
    char *numerator_text = NULL;
    bool matched = false;

    if (!numerator)
        return false;
    if (!factor || !*factor) {
        number_t value = expr_eval(numerator);

        matched = num_is_finite(value) && num_eq(value, first);
        num_destroy(&value);
        return matched;
    }

    coefficient = expr_new_const(first);
    factor_expr = expr_from_string(factor, NULL);
    expected = coefficient && factor_expr ? expr_mul(coefficient, factor_expr) : NULL;
    expected_simplified = expected ? expr_display_simplified(expected) : NULL;
    numerator_simplified = expr_display_simplified(numerator);
    expected_text = expected_simplified ? expr_to_string(expected_simplified, style_UNBOUND) : NULL;
    numerator_text = numerator_simplified ? expr_to_string(numerator_simplified, style_UNBOUND) : NULL;
    matched = expected_text && numerator_text && strcmp(expected_text, numerator_text) == 0;

    free(numerator_text);
    free(expected_text);
    expr_free(numerator_simplified);
    expr_free(expected_simplified);
    expr_free(expected);
    expr_free(factor_expr);
    expr_free(coefficient);
    return matched;
}

static int expr_series_append_slice(string_t *output, const char *text, size_t start, size_t end)
{
    expr_series_span_t span = {start, end};
    char *copy = expr_series_copy_span(text, span);
    int status;

    if (!copy)
        return -1;
    status = string_append_cstr(output, copy);
    free(copy);
    return status;
}

static void expr_series_destroy_numbers(number_t *values, size_t count)
{
    if (!values)
        return;
    for (size_t i = 0u; i < count; ++i)
        num_destroy(&values[i]);
    free(values);
}

static bool expr_series_initial_sum(const number_t *coefficients, size_t coefficient_count, number_t *sum_out)
{
    number_t sum = num_clone(NUM_ZERO);

    *sum_out = NUM_NAN;
    for (size_t i = 0u; i < coefficient_count; ++i) {
        number_t next = num_add(sum, coefficients[i]);

        num_destroy(&sum);
        sum = next;
        if (!num_is_finite(sum)) {
            num_destroy(&sum);
            return false;
        }
    }
    *sum_out = sum;
    return true;
}

static bool expr_series_sum_geometric(const number_t *coefficients, size_t coefficient_count, number_t last,
                                     number_t *sum_out, bool *recognised_out, size_t *endpoint_out,
                                     number_t *ratio_out)
{
    number_t ratio = NUM_NAN;
    number_t current = NUM_NAN;
    number_t sum = NUM_NAN;
    bool found = false;

    *sum_out = NUM_NAN;
    *recognised_out = false;
    *endpoint_out = 0u;
    *ratio_out = NUM_NAN;
    if (coefficient_count < 3u || num_is_zero(coefficients[0]))
        return false;
    ratio = num_div(coefficients[1], coefficients[0]);
    if (!num_is_finite(ratio) || num_is_one(ratio))
        goto cleanup;
    for (size_t i = 2u; i < coefficient_count; ++i) {
        number_t expected = num_mul(coefficients[i - 1u], ratio);
        bool matches = num_is_finite(expected) && num_eq(expected, coefficients[i]);

        num_destroy(&expected);
        if (!matches)
            goto cleanup;
    }
    *recognised_out = true;
    if (!expr_series_initial_sum(coefficients, coefficient_count, &sum))
        goto cleanup;
    current = num_clone(coefficients[coefficient_count - 1u]);
    for (size_t iteration = 0u; iteration < 1000000u; ++iteration) {
        number_t next = num_mul(current, ratio);
        number_t next_sum = num_add(sum, next);

        num_destroy(&current);
        num_destroy(&sum);
        current = next;
        sum = next_sum;
        if (!num_is_finite(current) || !num_is_finite(sum))
            goto cleanup;
        if (num_eq(current, last)) {
            *sum_out = sum;
            *ratio_out = num_clone(ratio);
            *endpoint_out = coefficient_count + iteration + 1u;
            sum = NUM_NAN;
            found = true;
            break;
        }
        if (num_sign(current) > 0 && num_sign(last) > 0 && num_gt(ratio, NUM_ONE) && num_gt(current, last))
            break;
    }

cleanup:
    num_destroy(&sum);
    num_destroy(&current);
    num_destroy(&ratio);
    return found;
}

static bool expr_series_index_power_exponent(const number_t *coefficients, size_t coefficient_count,
                                            int *exponent_out)
{
    number_t ratio = NUM_NAN;
    number_t two = NUM_NAN;
    number_t ratio_power = NUM_NAN;
    size_t bit_length;
    size_t exponent;
    bool recognised = false;

    *exponent_out = 0;
    if (coefficient_count < 3u || num_is_zero(coefficients[0]))
        return false;
    ratio = num_div(coefficients[1], coefficients[0]);
    if (!num_is_finite(ratio) || !num_is_integer(ratio) || num_sign(ratio) <= 0)
        goto cleanup;
    bit_length = num_bit_length(ratio);
    if (bit_length < 2u || bit_length - 1u > (size_t)INT_MAX)
        goto cleanup;
    exponent = bit_length - 1u;
    two = num_create_from_long(2L);
    ratio_power = num_pow_int(two, (int)exponent);
    if (!num_is_finite(ratio_power) || !num_eq(ratio_power, ratio))
        goto cleanup;

    for (size_t i = 2u; i < coefficient_count; ++i) {
        number_t index = num_create_from_long((long)(i + 1u));
        number_t index_power = num_pow_int(index, (int)exponent);
        number_t expected = num_mul(coefficients[0], index_power);
        bool matches = num_is_finite(expected) && num_eq(expected, coefficients[i]);

        num_destroy(&expected);
        num_destroy(&index_power);
        num_destroy(&index);
        if (!matches)
            goto cleanup;
    }
    *exponent_out = (int)exponent;
    recognised = true;

cleanup:
    num_destroy(&ratio_power);
    num_destroy(&two);
    num_destroy(&ratio);
    return recognised;
}

static bool expr_series_sum_index_power(const number_t *coefficients, size_t coefficient_count, number_t last,
                                       number_t *sum_out, bool *recognised_out, size_t *endpoint_out,
                                       int *exponent_out)
{
    number_t sum = NUM_NAN;
    int exponent = 0;
    bool found = false;

    *sum_out = NUM_NAN;
    *endpoint_out = 0u;
    *exponent_out = 0;
    *recognised_out = expr_series_index_power_exponent(coefficients, coefficient_count, &exponent);
    if (!*recognised_out || !expr_series_initial_sum(coefficients, coefficient_count, &sum))
        return false;

    for (size_t index_value = coefficient_count + 1u; index_value < 1000000u; ++index_value) {
        number_t index = num_create_from_long((long)index_value);
        number_t index_power = num_pow_int(index, exponent);
        number_t term = num_mul(coefficients[0], index_power);
        number_t next_sum = num_add(sum, term);

        num_destroy(&sum);
        sum = next_sum;
        num_destroy(&index_power);
        num_destroy(&index);
        if (!num_is_finite(term) || !num_is_finite(sum)) {
            num_destroy(&term);
            goto cleanup;
        }
        if (num_eq(term, last)) {
            *sum_out = sum;
            *endpoint_out = index_value;
            *exponent_out = exponent;
            sum = NUM_NAN;
            found = true;
            num_destroy(&term);
            break;
        }
        if ((num_sign(coefficients[0]) > 0 && num_sign(last) > 0 && num_gt(term, last)) ||
            (num_sign(coefficients[0]) < 0 && num_sign(last) < 0 && num_lt(term, last))) {
            num_destroy(&term);
            break;
        }
        num_destroy(&term);
    }

cleanup:
    num_destroy(&sum);
    return found;
}

static bool expr_series_inverse_index_power_exponent(const number_t *coefficients, size_t coefficient_count,
                                                    int *exponent_out)
{
    number_t ratio = NUM_NAN;
    number_t two = num_create_from_long(2L);
    bool recognised = false;

    *exponent_out = 0;
    if (coefficient_count < 3u || num_is_zero(coefficients[0]))
        goto cleanup;
    ratio = num_div(coefficients[1], coefficients[0]);
    if (!num_is_exact(ratio) || !num_is_finite(ratio))
        goto cleanup;

    for (int exponent = 1; exponent <= 32; ++exponent) {
        number_t denominator = num_pow_int(two, exponent);
        number_t expected_ratio = num_div(NUM_ONE, denominator);
        bool ratio_matches = num_is_finite(expected_ratio) && num_eq(ratio, expected_ratio);

        num_destroy(&expected_ratio);
        num_destroy(&denominator);
        if (!ratio_matches)
            continue;

        recognised = true;
        for (size_t i = 2u; i < coefficient_count; ++i) {
            number_t index = num_create_from_long((long)(i + 1u));
            number_t index_power = num_pow_int(index, exponent);
            number_t expected = num_div(coefficients[0], index_power);
            bool matches = num_is_finite(expected) && num_eq(expected, coefficients[i]);

            num_destroy(&expected);
            num_destroy(&index_power);
            num_destroy(&index);
            if (!matches) {
                recognised = false;
                break;
            }
        }
        if (recognised) {
            *exponent_out = exponent;
            break;
        }
    }

cleanup:
    num_destroy(&two);
    num_destroy(&ratio);
    return recognised;
}

static bool expr_series_inverse_index_power_endpoint(const number_t first, const number_t last, int exponent,
                                                     size_t minimum, size_t *endpoint_out)
{
    number_t ratio = NUM_NAN;
    long low = 1L;
    long high;
    bool found = false;

    *endpoint_out = 0u;
    if (num_is_zero(last) || num_sign(first) != num_sign(last) || minimum > (size_t)LONG_MAX)
        return false;
    ratio = num_div(first, last);
    if (!num_is_exact(ratio) || !num_is_integer(ratio) || num_sign(ratio) <= 0)
        goto cleanup;

    high = minimum > 2u ? (long)minimum : 2L;
    for (;;) {
        number_t candidate = num_create_from_long(high);
        number_t power = num_pow_int(candidate, exponent);
        const int comparison = num_cmp(power, ratio);

        num_destroy(&power);
        num_destroy(&candidate);
        if (comparison >= 0)
            break;
        if (high > LONG_MAX / 2L)
            goto cleanup;
        high *= 2L;
    }

    while (low <= high) {
        const long middle = low + (high - low) / 2L;
        number_t candidate = num_create_from_long(middle);
        number_t power = num_pow_int(candidate, exponent);
        const int comparison = num_cmp(power, ratio);

        num_destroy(&power);
        num_destroy(&candidate);
        if (comparison == 0) {
            if ((size_t)middle >= minimum) {
                *endpoint_out = (size_t)middle;
                found = true;
            }
            break;
        }
        if (comparison < 0)
            low = middle + 1L;
        else
            high = middle - 1L;
    }

cleanup:
    num_destroy(&ratio);
    return found;
}

static number_t expr_series_inverse_index_power_sum_approx(const number_t first, size_t endpoint, int exponent)
{
    number_t endpoint_value = num_create_from_long((long)endpoint);
    number_t upper = num_add(endpoint_value, NUM_ONE);
    number_t harmonic = NUM_NAN;
    number_t sum = NUM_NAN;

    if (exponent == 1) {
        number_t digamma = num_digamma(upper);

        harmonic = num_add(digamma, NUM_EULER_MASCHERONI);
        num_destroy(&digamma);
    } else {
        const unsigned int order = (unsigned int)(exponent - 1);
        number_t at_one = num_polygamma(order, NUM_ONE);
        number_t at_upper = num_polygamma(order, upper);
        number_t difference = num_sub(at_one, at_upper);
        number_t factorial = num_factorial(order);

        harmonic = num_div(difference, factorial);
        if (exponent % 2 != 0) {
            number_t negated = num_neg(harmonic);

            num_destroy(&harmonic);
            harmonic = negated;
        }
        num_destroy(&factorial);
        num_destroy(&difference);
        num_destroy(&at_upper);
        num_destroy(&at_one);
    }
    if (num_is_finite(harmonic))
        sum = num_mul(first, harmonic);

    num_destroy(&harmonic);
    num_destroy(&upper);
    num_destroy(&endpoint_value);
    return sum;
}

static bool expr_series_sum_inverse_index_power(const number_t *coefficients, size_t coefficient_count, number_t last,
                                               number_t *sum_out, bool *recognised_out, size_t *endpoint_out,
                                               int *exponent_out)
{
    number_t sum = NUM_NAN;
    size_t endpoint = 0u;
    int exponent = 0;
    bool found = false;

    *sum_out = NUM_NAN;
    *endpoint_out = 0u;
    *exponent_out = 0;
    *recognised_out = expr_series_inverse_index_power_exponent(coefficients, coefficient_count, &exponent);
    if (!*recognised_out ||
        !expr_series_inverse_index_power_endpoint(coefficients[0], last, exponent, coefficient_count + 1u, &endpoint))
        return false;

    if (endpoint > 1000000u) {
        sum = expr_series_inverse_index_power_sum_approx(coefficients[0], endpoint, exponent);
        if (!num_is_finite(sum))
            goto cleanup;
        *sum_out = sum;
        *endpoint_out = endpoint;
        *exponent_out = exponent;
        sum = NUM_NAN;
        found = true;
        goto cleanup;
    }
    if (!expr_series_initial_sum(coefficients, coefficient_count, &sum))
        return false;

    for (size_t index_value = coefficient_count + 1u; index_value <= endpoint; ++index_value) {
        number_t index = num_create_from_long((long)index_value);
        number_t index_power = num_pow_int(index, exponent);
        number_t term = num_div(coefficients[0], index_power);
        number_t next_sum = num_add(sum, term);

        num_destroy(&sum);
        sum = next_sum;
        num_destroy(&index_power);
        num_destroy(&index);
        if (!num_is_finite(term) || !num_is_finite(sum)) {
            num_destroy(&term);
            goto cleanup;
        }
        if (index_value == endpoint && num_eq(term, last)) {
            *sum_out = sum;
            *endpoint_out = index_value;
            *exponent_out = exponent;
            sum = NUM_NAN;
            found = true;
            num_destroy(&term);
            break;
        }
        num_destroy(&term);
    }

cleanup:
    num_destroy(&sum);
    return found;
}

static number_t *expr_series_difference_tails(const number_t *coefficients, size_t coefficient_count,
                                             size_t *degree_out)
{
    number_t *row = calloc(coefficient_count, sizeof(*row));
    number_t *tails = calloc(coefficient_count, sizeof(*tails));
    size_t row_count = coefficient_count;
    size_t tails_count = 0u;

    *degree_out = 0u;
    if (!row || !tails)
        goto failure;
    for (size_t i = 0u; i < coefficient_count; ++i)
        row[i] = num_clone(coefficients[i]);
    tails[tails_count++] = num_clone(row[row_count - 1u]);

    while (row_count > 1u) {
        number_t *next = calloc(row_count - 1u, sizeof(*next));

        if (!next)
            goto failure;
        for (size_t i = 0u; i + 1u < row_count; ++i) {
            next[i] = num_sub(row[i + 1u], row[i]);
            if (!num_is_finite(next[i])) {
                expr_series_destroy_numbers(next, i + 1u);
                goto failure;
            }
        }
        expr_series_destroy_numbers(row, row_count);
        row = next;
        --row_count;
        tails[tails_count++] = num_clone(row[row_count - 1u]);
    }
    expr_series_destroy_numbers(row, row_count);

    *degree_out = coefficient_count - 1u;
    while (*degree_out > 0u && num_is_zero(tails[*degree_out]))
        --*degree_out;
    return tails;

failure:
    expr_series_destroy_numbers(row, row_count);
    expr_series_destroy_numbers(tails, tails_count);
    return NULL;
}

static number_t *expr_series_zero_numbers(size_t count)
{
    number_t *values = calloc(count, sizeof(*values));

    if (!values)
        return NULL;
    for (size_t i = 0u; i < count; ++i)
        values[i] = num_clone(NUM_ZERO);
    return values;
}

/* Interpolate the supplied term-index/value points exactly in the monomial basis. */
static number_t *expr_series_lagrange_polynomial(const number_t *values, size_t coefficient_count, number_t last,
                                                size_t endpoint_index)
{
    const bool has_endpoint = endpoint_index != 0u;
    const size_t point_count = coefficient_count + (has_endpoint ? 1u : 0u);
    number_t *polynomial = expr_series_zero_numbers(point_count);

    if (!polynomial)
        return NULL;
    for (size_t point = 0u; point < point_count; ++point) {
        number_t *basis = expr_series_zero_numbers(point_count);
        number_t denominator;
        number_t ordinate;
        size_t basis_count = 1u;

        if (!basis)
            goto failure;
        denominator = num_clone(NUM_ONE);
        ordinate = point < coefficient_count ? num_clone(values[point]) : num_clone(last);
        num_destroy(&basis[0]);
        basis[0] = num_clone(NUM_ONE);
        for (size_t other = 0u; other < point_count; ++other) {
            number_t *next;
            number_t other_index;
            number_t negative_index;
            number_t point_index;
            number_t difference;
            number_t next_denominator;

            if (other == point)
                continue;
            next = expr_series_zero_numbers(point_count);
            if (!next) {
                expr_series_destroy_numbers(basis, point_count);
                num_destroy(&ordinate);
                num_destroy(&denominator);
                goto failure;
            }
            other_index = num_create_from_long((long)(other < coefficient_count ? other + 1u : endpoint_index));
            negative_index = num_neg(other_index);
            for (size_t power = 0u; power < basis_count; ++power) {
                number_t constant_term = num_mul(basis[power], negative_index);
                number_t next_constant = num_add(next[power], constant_term);
                number_t next_linear = num_add(next[power + 1u], basis[power]);

                num_destroy(&constant_term);
                num_destroy(&next[power]);
                next[power] = next_constant;
                num_destroy(&next[power + 1u]);
                next[power + 1u] = next_linear;
            }
            point_index = num_create_from_long((long)(point < coefficient_count ? point + 1u : endpoint_index));
            difference = num_sub(point_index, other_index);
            next_denominator = num_mul(denominator, difference);
            num_destroy(&difference);
            num_destroy(&point_index);
            num_destroy(&negative_index);
            num_destroy(&other_index);
            num_destroy(&denominator);
            denominator = next_denominator;
            expr_series_destroy_numbers(basis, point_count);
            basis = next;
            ++basis_count;
        }
        {
            number_t scale = num_div(ordinate, denominator);

            for (size_t power = 0u; power < point_count; ++power) {
                number_t term = num_mul(scale, basis[power]);
                number_t updated = num_add(polynomial[power], term);

                num_destroy(&term);
                num_destroy(&polynomial[power]);
                polynomial[power] = updated;
            }
            num_destroy(&scale);
        }
        expr_series_destroy_numbers(basis, point_count);
        num_destroy(&ordinate);
        num_destroy(&denominator);
    }
    for (size_t power = 0u; power < point_count; ++power) {
        if (!num_is_exact(polynomial[power]) || !num_is_finite(polynomial[power]))
            goto failure;
    }
    return polynomial;

failure:
    expr_series_destroy_numbers(polynomial, point_count);
    return NULL;
}

static number_t expr_series_polynomial_value(const number_t *polynomial, size_t coefficient_count,
                                            size_t index_value)
{
    number_t index = num_create_from_long((long)index_value);
    number_t value = num_clone(polynomial[coefficient_count - 1u]);

    for (size_t power = coefficient_count - 1u; power > 0u; --power) {
        number_t product = num_mul(value, index);
        number_t updated = num_add(product, polynomial[power - 1u]);

        num_destroy(&product);
        num_destroy(&value);
        value = updated;
    }
    num_destroy(&index);
    return value;
}

static bool expr_series_polynomial_complexity(const number_t *polynomial, size_t coefficient_count,
                                             size_t *maximum_out, size_t *total_out)
{
    size_t maximum = 0u;
    size_t total = 0u;

    for (size_t i = 0u; i < coefficient_count; ++i) {
        string_t *text = num_to_string(polynomial[i]);
        size_t length;

        if (!text)
            return false;
        length = strlen(string_c_str(text));
        string_free(text);
        if (length > maximum)
            maximum = length;
        if (SIZE_MAX - total < length)
            return false;
        total += length;
    }
    *maximum_out = maximum;
    *total_out = total;
    return true;
}

static size_t expr_series_lagrange_endpoint_limit(number_t last, size_t coefficient_count)
{
    const size_t value_bits = num_bit_length(last);
    size_t index_bits = (value_bits + coefficient_count - 1u) / coefficient_count + 2u;
    size_t limit;

    if (index_bits >= sizeof(size_t) * CHAR_BIT || index_bits >= 20u)
        limit = 1000000u;
    else
        limit = (size_t)1u << index_bits;
    if (limit < coefficient_count + 32u)
        limit = coefficient_count + 32u;
    return limit > 1000000u ? 1000000u : limit;
}

static bool expr_series_sum_lagrange(const number_t *coefficients, size_t coefficient_count, number_t last,
                                    number_t *sum_out, number_t **polynomial_out, size_t *polynomial_count_out,
                                    size_t *endpoint_out)
{
    const size_t point_count = coefficient_count + 1u;
    const size_t endpoint_limit = expr_series_lagrange_endpoint_limit(last, coefficient_count);
    number_t *best = NULL;
    size_t best_endpoint = 0u;
    size_t best_maximum = SIZE_MAX;
    size_t best_total = SIZE_MAX;
    number_t sum = NUM_NAN;

    *sum_out = NUM_NAN;
    *polynomial_out = NULL;
    *polynomial_count_out = 0u;
    *endpoint_out = 0u;
    if (coefficient_count < 3u || !num_is_exact(last))
        return false;
    for (size_t endpoint = coefficient_count + 2u; endpoint <= endpoint_limit; ++endpoint) {
        number_t *candidate = expr_series_lagrange_polynomial(coefficients, coefficient_count, last, endpoint);
        size_t maximum;
        size_t total;

        if (!candidate)
            continue;
        if (!expr_series_polynomial_complexity(candidate, point_count, &maximum, &total)) {
            expr_series_destroy_numbers(candidate, point_count);
            continue;
        }
        if (maximum < best_maximum || (maximum == best_maximum && total < best_total)) {
            expr_series_destroy_numbers(best, point_count);
            best = candidate;
            best_endpoint = endpoint;
            best_maximum = maximum;
            best_total = total;
        } else {
            expr_series_destroy_numbers(candidate, point_count);
        }
    }
    if (!best || !expr_series_initial_sum(coefficients, coefficient_count, &sum))
        goto cleanup;
    for (size_t index = coefficient_count + 1u; index <= best_endpoint; ++index) {
        number_t term = expr_series_polynomial_value(best, point_count, index);
        number_t updated = num_add(sum, term);

        num_destroy(&sum);
        sum = updated;
        if (!num_is_exact(term) || !num_is_finite(term) || !num_is_finite(sum)) {
            num_destroy(&term);
            goto cleanup;
        }
        if (index == best_endpoint && !num_eq(term, last)) {
            num_destroy(&term);
            goto cleanup;
        }
        num_destroy(&term);
    }
    *sum_out = sum;
    *polynomial_out = best;
    *polynomial_count_out = point_count;
    *endpoint_out = best_endpoint;
    sum = NUM_NAN;
    best = NULL;

cleanup:
    num_destroy(&sum);
    expr_series_destroy_numbers(best, point_count);
    return num_is_finite(*sum_out);
}

static bool expr_series_differences_move_away(const number_t *tails, size_t degree, number_t last)
{
    bool nonnegative = true;
    bool nonpositive = true;

    for (size_t order = 1u; order <= degree; ++order) {
        const int sign = num_sign(tails[order]);

        if (sign < 0)
            nonnegative = false;
        if (sign > 0)
            nonpositive = false;
    }
    return (nonnegative && num_gt(tails[0], last)) || (nonpositive && num_lt(tails[0], last));
}

static bool expr_series_sum_polynomial(const number_t *coefficients, size_t coefficient_count, number_t last,
                                      number_t *sum_out, size_t *endpoint_out)
{
    number_t *tails;
    number_t sum = NUM_NAN;
    size_t degree = 0u;
    bool found = false;

    *sum_out = NUM_NAN;
    *endpoint_out = 0u;
    if (coefficient_count < 2u || !expr_series_initial_sum(coefficients, coefficient_count, &sum))
        return false;
    tails = expr_series_difference_tails(coefficients, coefficient_count, &degree);
    if (!tails || degree == 0u)
        goto cleanup;

    for (size_t iteration = 0u; iteration < 1000000u; ++iteration) {
        number_t next_sum;

        for (size_t order = degree; order > 0u; --order) {
            number_t updated = num_add(tails[order - 1u], tails[order]);

            num_destroy(&tails[order - 1u]);
            tails[order - 1u] = updated;
            if (!num_is_finite(updated))
                goto cleanup;
        }
        next_sum = num_add(sum, tails[0]);
        num_destroy(&sum);
        sum = next_sum;
        if (!num_is_finite(sum))
            goto cleanup;
        if (num_eq(tails[0], last)) {
            *sum_out = sum;
            *endpoint_out = coefficient_count + iteration + 1u;
            sum = NUM_NAN;
            found = true;
            break;
        }
        if (expr_series_differences_move_away(tails, degree, last))
            break;
    }

cleanup:
    num_destroy(&sum);
    expr_series_destroy_numbers(tails, coefficient_count);
    return found;
}

static bool expr_series_set_index_power_model(expr_series_result_t *result, const number_t *coefficients, int exponent)
{
    result->polynomial = expr_series_zero_numbers((size_t)exponent + 1u);
    if (!result->polynomial)
        return false;
    num_destroy(&result->polynomial[exponent]);
    result->polynomial[exponent] = num_clone(coefficients[0]);
    result->polynomial_count = (size_t)exponent + 1u;
    result->kind = EXPR_SERIES_MODEL_POLYNOMIAL;
    return true;
}

static bool expr_series_sum_coefficients(const number_t *coefficients, size_t coefficient_count, number_t last,
                                        expr_series_result_t *result)
{
    bool geometric = false;
    bool index_power = false;
    bool inverse_index_power = false;
    size_t endpoint = 0u;
    int exponent = 0;
    number_t ratio = NUM_NAN;

    if (expr_series_sum_inverse_index_power(coefficients, coefficient_count, last, &result->sum,
                                           &inverse_index_power, &endpoint, &exponent)) {
        result->inverse_index_first = num_clone(coefficients[0]);
        result->inverse_index_exponent = exponent;
        result->endpoint = endpoint;
        result->kind = EXPR_SERIES_MODEL_INVERSE_INDEX_POWER;
        return true;
    }
    if (inverse_index_power)
        return false;
    if (expr_series_sum_geometric(coefficients, coefficient_count, last, &result->sum, &geometric, &endpoint, &ratio)) {
        result->geometric_first = num_clone(coefficients[0]);
        result->geometric_ratio = ratio;
        result->endpoint = endpoint;
        result->kind = EXPR_SERIES_MODEL_GEOMETRIC;
        return true;
    }
    num_destroy(&ratio);
    if (geometric)
        return false;
    if (expr_series_sum_index_power(coefficients, coefficient_count, last, &result->sum, &index_power, &endpoint,
                                   &exponent)) {
        result->endpoint = endpoint;
        return expr_series_set_index_power_model(result, coefficients, exponent);
    }
    if (index_power)
        return false;
    if (expr_series_sum_polynomial(coefficients, coefficient_count, last, &result->sum, &endpoint)) {
        result->polynomial = expr_series_lagrange_polynomial(coefficients, coefficient_count, NUM_NAN, 0u);
        if (!result->polynomial)
            return false;
        result->polynomial_count = coefficient_count;
        result->endpoint = endpoint;
        result->kind = EXPR_SERIES_MODEL_POLYNOMIAL;
        return true;
    }
    if (!expr_series_sum_lagrange(coefficients, coefficient_count, last, &result->sum, &result->polynomial,
                                 &result->polynomial_count, &result->endpoint))
        return false;
    result->kind = EXPR_SERIES_MODEL_POLYNOMIAL;
    return true;
}

static expr_t *expr_series_polynomial_expr(const expr_series_result_t *result, const expr_t *index)
{
    expr_t *sum = NULL;

    for (size_t power = 0u; power < result->polynomial_count; ++power) {
        expr_t *coefficient;
        expr_t *index_power = NULL;
        expr_t *term;
        expr_t *next;

        if (num_is_zero(result->polynomial[power]))
            continue;
        coefficient = expr_new_const(result->polynomial[power]);
        if (power == 0u) {
            term = coefficient;
            coefficient = NULL;
        } else {
            number_t exponent = num_create_from_long((long)power);

            index_power = power == 1u ? expr_clone(index) : expr_pow(index, &exponent);
            num_destroy(&exponent);
            term = coefficient && index_power ? expr_mul(coefficient, index_power) : NULL;
        }
        expr_free(index_power);
        expr_free(coefficient);
        if (!term)
            goto failure;
        if (!sum) {
            sum = term;
            continue;
        }
        next = expr_add(sum, term);
        expr_free(term);
        expr_free(sum);
        if (!next)
            return NULL;
        sum = next;
    }
    return sum ? sum : expr_new_const(NUM_ZERO);

failure:
    expr_free(sum);
    return NULL;
}

static expr_t *expr_series_geometric_expr(const expr_series_result_t *result, const expr_t *index)
{
    expr_t *first = expr_new_const(result->geometric_first);
    expr_t *ratio = expr_new_const(result->geometric_ratio);
    expr_t *exponent = expr_sub(index, EXPR_ONE);
    expr_t *power = ratio && exponent ? expr_pow_xp(ratio, exponent) : NULL;
    expr_t *term = first && power ? expr_mul(first, power) : NULL;

    expr_free(power);
    expr_free(exponent);
    expr_free(ratio);
    expr_free(first);
    return term;
}

static expr_t *expr_series_geometric_expr_zero_based(const expr_series_result_t *result, const expr_t *index)
{
    expr_t *first = result->symbolic_geometric_first ? expr_clone(result->symbolic_geometric_first)
                                                     : expr_new_const(result->geometric_first);
    expr_t *ratio = result->symbolic_geometric_ratio ? expr_clone(result->symbolic_geometric_ratio)
                                                     : expr_new_const(result->geometric_ratio);
    expr_t *power = ratio ? expr_pow_xp(ratio, index) : NULL;
    expr_t *term = first && power ? expr_mul(first, power) : NULL;

    expr_free(power);
    expr_free(ratio);
    expr_free(first);
    return term;
}

static expr_t *expr_series_inverse_index_power_expr(const expr_series_result_t *result, const expr_t *index,
                                                   const expr_t *factor)
{
    expr_t *first = expr_new_const(result->inverse_index_first);
    expr_t *numerator = result->inverse_index_numerator
                            ? expr_clone(result->inverse_index_numerator)
                            : (factor ? expr_mul(first, factor) : expr_clone(first));
    expr_t *denominator = NULL;
    expr_t *power = NULL;
    expr_t *term = NULL;

    if (result->symbolic_direct_power) {
        power = result->symbolic_exponent ? expr_pow_xp(index, result->symbolic_exponent) : NULL;
        term = numerator && power ? expr_mul(numerator, power) : NULL;
    } else {
        denominator = result->symbolic_exponent
                          ? expr_pow_xp(index, result->symbolic_exponent)
                          : (result->inverse_index_exponent == 1
                                 ? expr_clone(index)
                                 : expr_pow_long(index, result->inverse_index_exponent));
        term = numerator && denominator ? expr_div(numerator, denominator) : NULL;
    }

    expr_free(power);
    expr_free(denominator);
    expr_free(numerator);
    expr_free(first);
    return term;
}

static expr_t *expr_series_parse_span_expr(const char *text, expr_series_span_t span)
{
    char *term_text;
    expr_t *term;

    span = expr_series_trim(text, span);
    term_text = expr_series_copy_span(text, span);
    if (!term_text)
        return NULL;
    term = expr_from_string(term_text, NULL);
    free(term_text);
    return term;
}

static expr_t *expr_series_add_term(expr_t *sum, expr_t *term)
{
    expr_t *next;

    if (!sum)
        return term;
    next = term ? expr_add(sum, term) : NULL;
    expr_free(term);
    expr_free(sum);
    return next;
}

static bool expr_series_text_uses_ascii_symbol(const char *text, char symbol)
{
    if (!text)
        return false;
    for (size_t i = 0u; text[i] != '\0'; ++i) {
        const unsigned char previous = i > 0u ? (unsigned char)text[i - 1u] : 0u;
        const unsigned char current = (unsigned char)text[i];
        const unsigned char next = (unsigned char)text[i + 1u];

        if (current == (unsigned char)symbol && !(isalnum(previous) || previous == '_') &&
            !(isalnum(next) || next == '_'))
            return true;
    }
    return false;
}

static const char *expr_series_display_index_name(const char *text)
{
    static const char candidates[] = {'n', 'k', 'j', 'm', 'l', 'r', 's', 't'};
    static const char names[][2] = {"n", "k", "j", "m", "l", "r", "s", "t"};

    for (size_t i = 0u; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (!expr_series_text_uses_ascii_symbol(text, candidates[i]))
            return names[i];
    }
    return "index";
}

static string_t *expr_series_display_TeX(const char *text, const expr_series_span_t *terms, size_t term_count,
                                        size_t series_start, size_t ellipsis_index, const char *factor,
                                        const expr_series_result_t *result, bool endpoint_positive_infinity)
{
    expr_t *index = expr_new_named_var(NUM_NAN, expr_series_display_index_name(text));
    expr_t *model = NULL;
    expr_t *factor_expr = NULL;
    expr_t *term = NULL;
    const bool zero_based_geometric = result->kind == EXPR_SERIES_MODEL_GEOMETRIC && result->symbolic_endpoint;
    expr_t *lower = expr_new_const(zero_based_geometric ? NUM_ZERO : NUM_ONE);
    number_t endpoint_value = NUM_NAN;
    expr_t *upper = NULL;
    expr_t *summation = NULL;
    expr_t *display = NULL;
    expr_t *simplified = NULL;
    string_t *TeX = NULL;
    char *TeX_body = NULL;

    if (result->symbolic_endpoint && endpoint_positive_infinity) {
        upper = expr_new_const(NUM_INF);
    } else if (result->symbolic_endpoint) {
        upper = zero_based_geometric ? expr_sub_simplify_owned(expr_clone(result->symbolic_endpoint),
                                                               expr_clone(EXPR_ONE))
                                     : expr_clone(result->symbolic_endpoint);
    } else {
        endpoint_value = num_create_from_long((long)result->endpoint);
        upper = expr_new_const(endpoint_value);
        num_destroy(&endpoint_value);
    }
    if (!index || !lower || !upper)
        goto cleanup;
    if (factor && *factor) {
        factor_expr = expr_from_string(factor, NULL);
        if (!factor_expr)
            goto cleanup;
    }
    if (result->kind == EXPR_SERIES_MODEL_GEOMETRIC)
        model = zero_based_geometric ? expr_series_geometric_expr_zero_based(result, index)
                                     : expr_series_geometric_expr(result, index);
    else if (result->kind == EXPR_SERIES_MODEL_INVERSE_INDEX_POWER)
        model = expr_series_inverse_index_power_expr(result, index, factor_expr);
    else
        model = expr_series_polynomial_expr(result, index);
    if (!model)
        goto cleanup;
    simplified = expr_display_simplified(model);
    if (!simplified)
        goto cleanup;
    term = factor_expr && result->kind != EXPR_SERIES_MODEL_INVERSE_INDEX_POWER ? expr_mul(simplified, factor_expr)
                                                                               : expr_clone(simplified);
    if (!term)
        goto cleanup;
    summation = expr_new_finite_summation_range(term, index, lower, upper);
    if (!summation)
        goto cleanup;

    for (size_t i = 0u; i < series_start; ++i) {
        display = expr_series_add_term(display, expr_series_parse_span_expr(text, terms[i]));
        if (!display)
            goto cleanup;
    }
    display = expr_series_add_term(display, summation);
    summation = NULL;
    if (!display)
        goto cleanup;
    for (size_t i = ellipsis_index + 2u; i < term_count; ++i) {
        display = expr_series_add_term(display, expr_series_parse_span_expr(text, terms[i]));
        if (!display)
            goto cleanup;
    }
    TeX_body = expr_to_TeX_body(display);
    TeX = TeX_body ? string_new_with(TeX_body) : NULL;

cleanup:
    free(TeX_body);
    expr_free(display);
    expr_free(summation);
    expr_free(simplified);
    expr_free(term);
    expr_free(factor_expr);
    expr_free(model);
    expr_free(upper);
    expr_free(lower);
    expr_free(index);
    return TeX;
}

static int expr_series_append_inverse_index_formula(string_t *output, const expr_series_result_t *result,
                                                    const char *factor, bool endpoint_positive_infinity,
                                                    bool *domain_specialised_out)
{
    const bool has_coefficient = !num_is_one(result->inverse_index_first);
    const bool has_factor = factor && *factor;
    string_t *coefficient_text = has_coefficient ? num_to_string(result->inverse_index_first) : NULL;
    char *endpoint_text = result->symbolic_endpoint ? expr_to_string(result->symbolic_endpoint, style_UNBOUND) : NULL;
    int status = 0;

    if (has_coefficient && !coefficient_text)
        return -1;
    if (result->symbolic_endpoint && !endpoint_text) {
        string_free(coefficient_text);
        return -1;
    }
    if (has_coefficient && string_append_format(output, "%S*", coefficient_text) < 0)
        status = -1;
    if (status == 0 && has_factor && string_append_format(output, "(%s)*", factor) < 0)
        status = -1;
    if (status == 0 && (has_coefficient || has_factor) && string_append_char(output, '(') != 0)
        status = -1;
    if (status == 0 && endpoint_positive_infinity && result->inverse_index_exponent > 1) {
        if (result->inverse_index_exponent == 2 && string_append_cstr(output, "pi^2/6") != 0)
            status = -1;
        else if (result->inverse_index_exponent != 2 &&
                 string_append_format(output, "zeta(%d)", result->inverse_index_exponent) < 0)
            status = -1;
        else if (domain_specialised_out)
            *domain_specialised_out = true;
    } else if (status == 0 && result->inverse_index_exponent == 1) {
        if (result->symbolic_endpoint) {
            if (string_append_format(output, "digamma((%s)+1)+gamma", endpoint_text) < 0)
                status = -1;
        } else if (string_append_format(output, "digamma(%zu)+gamma", result->endpoint + 1u) < 0) {
            status = -1;
        }
    } else if (status == 0 && result->inverse_index_exponent == 2) {
        if (result->symbolic_endpoint) {
            if (string_append_format(output, "pi^2/6-trigamma((%s)+1)", endpoint_text) < 0)
                status = -1;
        } else if (string_append_format(output, "pi^2/6-trigamma(%zu)", result->endpoint + 1u) < 0) {
            status = -1;
        }
    } else if (status == 0) {
        const unsigned int order = (unsigned int)(result->inverse_index_exponent - 1);
        number_t factorial = num_factorial(order);
        string_t *factorial_text = num_to_string(factorial);
        const bool add_tail = (result->inverse_index_exponent % 2) != 0;

        if (!factorial_text) {
            status = -1;
        } else if (result->symbolic_endpoint && add_tail) {
            if (string_append_format(output, "zeta(%d)+polygamma(%u,(%s)+1)/%S", result->inverse_index_exponent,
                                     order, endpoint_text, factorial_text) < 0)
                status = -1;
        } else if (result->symbolic_endpoint) {
            if (string_append_format(output, "zeta(%d)-polygamma(%u,(%s)+1)/%S", result->inverse_index_exponent,
                                     order, endpoint_text, factorial_text) < 0)
                status = -1;
        } else if (add_tail) {
            if (string_append_format(output, "zeta(%d)+polygamma(%u,%zu)/%S", result->inverse_index_exponent, order,
                                     result->endpoint + 1u, factorial_text) < 0)
                status = -1;
        } else if (string_append_format(output, "zeta(%d)-polygamma(%u,%zu)/%S", result->inverse_index_exponent,
                                        order, result->endpoint + 1u, factorial_text) < 0) {
            status = -1;
        }
        string_free(factorial_text);
        num_destroy(&factorial);
    }
    if (status == 0 && (has_coefficient || has_factor) && string_append_char(output, ')') != 0)
        status = -1;

    string_free(coefficient_text);
    free(endpoint_text);
    return status;
}

static bool expr_series_number_to_long(number_t value, long *value_out)
{
    string_t *text;
    char *end = NULL;
    long parsed;

    if (!value_out || !num_is_real(value) || !num_is_finite(value) || !num_is_integer(value))
        return false;
    text = num_to_string(value);
    if (!text)
        return false;
    parsed = strtol(string_c_str(text), &end, 10);
    if (!end || *end != '\0') {
        string_free(text);
        return false;
    }
    string_free(text);
    *value_out = parsed;
    return true;
}

static int expr_series_append_faulhaber_formula(string_t *output, const expr_series_result_t *result, long exponent)
{
    const unsigned long maximum_degree = 32ul;
    unsigned long degree;
    size_t point_count;
    expr_series_result_t polynomial_result;
    number_t *values = NULL;
    number_t running_sum = NUM_NAN;
    number_t numerator_value = NUM_NAN;
    expr_t *formula = NULL;
    expr_t *scaled_formula = NULL;
    expr_t *simplified = NULL;
    char *formula_text = NULL;
    int status = -1;

    if (exponent > 0L || exponent == LONG_MIN)
        return 1;
    degree = (unsigned long)(-exponent);
    if (degree > maximum_degree)
        return 1;
    point_count = (size_t)degree + 2u;
    expr_series_result_init(&polynomial_result);
    values = expr_series_zero_numbers(point_count);
    if (!values)
        goto cleanup;
    running_sum = num_clone(NUM_ZERO);
    for (size_t point = 0u; point < point_count; ++point) {
        number_t index = num_create_from_long((long)point + 1L);
        number_t term = num_pow_int(index, (int)degree);
        number_t next = num_add(running_sum, term);

        num_destroy(&values[point]);
        values[point] = num_clone(next);
        num_destroy(&running_sum);
        running_sum = next;
        num_destroy(&term);
        num_destroy(&index);
    }
    polynomial_result.polynomial = expr_series_lagrange_polynomial(values, point_count, NUM_NAN, 0u);
    polynomial_result.polynomial_count = point_count;
    formula = polynomial_result.polynomial
                  ? expr_series_polynomial_expr(&polynomial_result, result->symbolic_endpoint)
                  : NULL;
    if (!formula)
        goto cleanup;
    numerator_value = expr_eval(result->inverse_index_numerator);
    scaled_formula = num_is_one(numerator_value) ? expr_clone(formula)
                                                  : expr_mul(result->inverse_index_numerator, formula);
    simplified = scaled_formula ? expr_display_simplified(scaled_formula) : NULL;
    formula_text = simplified ? expr_to_string(simplified, style_UNBOUND) : NULL;
    if (formula_text && string_append_cstr(output, formula_text) == 0)
        status = 0;

cleanup:
    free(formula_text);
    expr_free(simplified);
    expr_free(scaled_formula);
    expr_free(formula);
    num_destroy(&numerator_value);
    num_destroy(&running_sum);
    expr_series_destroy_numbers(values, point_count);
    expr_series_result_clear(&polynomial_result);
    return status;
}

static int expr_series_append_symbolic_geometric_formula(string_t *output, const expr_series_result_t *result)
{
    char *symbolic_first_text = result->symbolic_geometric_first
                                    ? expr_to_string(result->symbolic_geometric_first, style_UNBOUND)
                                    : NULL;
    char *symbolic_ratio_text = result->symbolic_geometric_ratio
                                    ? expr_to_string(result->symbolic_geometric_ratio, style_UNBOUND)
                                    : NULL;
    string_t *first_text = num_to_string(result->geometric_first);
    string_t *ratio_text = num_to_string(result->geometric_ratio);
    char *term_count_text = expr_to_string(result->symbolic_endpoint, style_UNBOUND);
    number_t denominator = num_sub(result->geometric_ratio, NUM_ONE);
    string_t *denominator_text = num_to_string(denominator);
    int status = -1;

    if (symbolic_first_text && symbolic_ratio_text && term_count_text) {
        if (string_append_format(output, "(%s)*((%s)^(%s)-1)/((%s)-1)", symbolic_first_text,
                                 symbolic_ratio_text, term_count_text, symbolic_ratio_text) >= 0) {
            status = 0;
        }
        goto cleanup;
    }
    if (!first_text || !ratio_text || !term_count_text || !num_is_finite(denominator) || !denominator_text)
        goto cleanup;
    if (num_is_one(result->geometric_first) && num_is_one(denominator)) {
        if (string_append_format(output, "(%S)^(%s)-1", ratio_text, term_count_text) >= 0)
            status = 0;
    } else if (num_is_one(result->geometric_first)) {
        if (string_append_format(output, "((%S)^(%s)-1)/(%S)", ratio_text, term_count_text, denominator_text) >= 0)
            status = 0;
    } else if (string_append_format(output, "(%S)*((%S)^(%s)-1)/(%S)", first_text, ratio_text, term_count_text,
                                    denominator_text) >= 0) {
        status = 0;
    }

cleanup:
    free(symbolic_ratio_text);
    free(symbolic_first_text);
    string_free(denominator_text);
    num_destroy(&denominator);
    free(term_count_text);
    string_free(ratio_text);
    string_free(first_text);
    return status;
}

static int expr_series_append_symbolic_power_formula(string_t *output, const expr_series_result_t *result,
                                                     expr_series_binding_lookup_fn lookup_binding,
                                                     void *lookup_context, bool endpoint_positive_infinity,
                                                     bool *domain_specialised_out)
{
    char *numerator_text = expr_to_string(result->inverse_index_numerator, style_UNBOUND);
    char *exponent_text = expr_to_string(result->symbolic_exponent, style_UNBOUND);
    char *endpoint_text = expr_to_string(result->symbolic_endpoint, style_UNBOUND);
    number_t numerator_value = expr_eval(result->inverse_index_numerator);
    bool unit_numerator = num_is_one(numerator_value);
    int status = -1;

    if (lookup_binding && exponent_text) {
        number_t bound_exponent = NUM_NAN;
        long exponent;
        long identity_exponent;

        if (lookup_binding(lookup_context, exponent_text, &bound_exponent) &&
            num_is_real(bound_exponent)) {
            number_t identity_bound_exponent =
                result->symbolic_direct_power ? num_neg(bound_exponent) : num_clone(bound_exponent);
            int alternate_status = 1;

            if (endpoint_positive_infinity && num_gt(identity_bound_exponent, NUM_ONE)) {
                const char *sign = result->symbolic_direct_power ? "-" : "";

                if (num_eq(identity_bound_exponent, NUM_TWO)) {
                    alternate_status = unit_numerator
                                           ? string_append_cstr(output, "pi^2/6")
                                           : string_append_format(output, "(%s)*(pi^2/6)", numerator_text);
                } else {
                    alternate_status = unit_numerator
                                           ? string_append_format(output, "zeta(%s(%s))", sign, exponent_text)
                                           : string_append_format(output, "(%s)*zeta(%s(%s))", numerator_text, sign,
                                                                  exponent_text);
                }
                alternate_status = alternate_status < 0 ? -1 : 0;
                if (alternate_status == 0 && domain_specialised_out)
                    *domain_specialised_out = true;
            } else if (expr_series_number_to_long(bound_exponent, &exponent)) {
                if (result->symbolic_direct_power && exponent == LONG_MIN) {
                    identity_exponent = LONG_MAX;
                } else {
                    identity_exponent = result->symbolic_direct_power ? -exponent : exponent;
                }
                if (identity_exponent <= 0L)
                    alternate_status = expr_series_append_faulhaber_formula(output, result, identity_exponent);
                else if (identity_exponent == 1L && endpoint_text && numerator_text) {
                    const int append_status =
                        unit_numerator ? string_append_format(output, "digamma((%s)+1)+gamma", endpoint_text)
                                       : string_append_format(output, "(%s)*(digamma((%s)+1)+gamma)",
                                                              numerator_text, endpoint_text);

                    alternate_status = append_status < 0 ? -1 : 0;
                    if (alternate_status == 0 && domain_specialised_out)
                        *domain_specialised_out = true;
                }
            }
            num_destroy(&identity_bound_exponent);
            num_destroy(&bound_exponent);
            if (alternate_status <= 0) {
                num_destroy(&numerator_value);
                free(endpoint_text);
                free(exponent_text);
                free(numerator_text);
                return alternate_status;
            }
        } else {
            num_destroy(&bound_exponent);
        }
    }
    if (numerator_text && exponent_text && endpoint_text) {
        const char *sign = result->symbolic_direct_power ? "-" : "";

        if (unit_numerator &&
            string_append_format(output, "zeta(%s(%s))-zetah(%s(%s),(%s)+1)", sign, exponent_text, sign, exponent_text,
                                 endpoint_text) >= 0)
            status = 0;
        else if (!unit_numerator &&
                 string_append_format(output, "(%s)*(zeta(%s(%s))-zetah(%s(%s),(%s)+1))", numerator_text,
                                      sign, exponent_text, sign, exponent_text, endpoint_text) >= 0)
            status = 0;
    }
    num_destroy(&numerator_value);
    free(endpoint_text);
    free(exponent_text);
    free(numerator_text);
    return status;
}

static string_t *expr_series_expand_once(const string_t *side, bool *expanded_out, string_t **display_TeX_out,
                                         expr_series_binding_lookup_fn lookup_binding, void *lookup_context,
                                         bool *domain_specialised_out)
{
    const char *text = string_c_str(side);
    expr_series_span_t *terms = NULL;
    size_t term_count = 0u;
    size_t ellipsis_index = SIZE_MAX;
    size_t series_start = 0u;
    size_t coefficient_count = 0u;
    number_t *coefficients = NULL;
    number_t last = NUM_NAN;
    expr_series_result_t result;
    char *last_factor = NULL;
    char *terminal_factor = NULL;
    expr_t *symbolic_numerator = NULL;
    number_t symbolic_geometric_ratio = NUM_NAN;
    int symbolic_exponent = 0;
    bool symbolic_endpoint = false;
    bool symbolic_geometric = false;
    bool symbolic_geometric_expression = false;
    bool endpoint_positive_infinity = false;
    string_t *output = NULL;

    *expanded_out = false;
    *display_TeX_out = NULL;
    expr_series_result_init(&result);
    if (!expr_series_split_terms(text, &terms, &term_count))
        return NULL;

    for (size_t i = 0u; i < term_count; ++i) {
        if (expr_series_is_ellipsis(text, terms[i])) {
            ellipsis_index = i;
            break;
        }
    }
    if (ellipsis_index == SIZE_MAX) {
        expr_series_result_clear(&result);
        free(terms);
        return string_clone(side);
    }
    if (ellipsis_index < 2u || ellipsis_index + 1u >= term_count)
        goto unsupported;

    symbolic_geometric = expr_series_parse_symbolic_geometric_endpoint(
        text, terms[ellipsis_index + 1u], &result.symbolic_endpoint, &symbolic_geometric_ratio);
    if (!symbolic_geometric) {
        symbolic_geometric_expression =
            expr_series_parse_symbolic_geometric(text, terms, ellipsis_index, &result, &series_start);
        symbolic_geometric = symbolic_geometric_expression;
    }
    symbolic_endpoint = symbolic_geometric;
    if (!symbolic_geometric) {
        symbolic_endpoint = expr_series_parse_symbolic_power_endpoint(
            text, terms[ellipsis_index + 1u], &symbolic_numerator, &result.symbolic_endpoint,
            &result.symbolic_exponent, &symbolic_exponent, &result.symbolic_direct_power);
    }
    if (!symbolic_endpoint &&
        !expr_series_parse_term(text, terms[ellipsis_index + 1u], &last, &terminal_factor))
        goto unsupported;
    if (symbolic_endpoint && result.symbolic_exponent) {
        if (!expr_series_symbolic_power_prefix_matches(text, terms, ellipsis_index, symbolic_numerator,
                                                       result.symbolic_exponent, result.symbolic_direct_power,
                                                       &series_start))
            goto unsupported;
        result.inverse_index_numerator = expr_clone(symbolic_numerator);
        result.kind = EXPR_SERIES_MODEL_INVERSE_INDEX_POWER;
        last_factor = strdup("");
        if (!result.inverse_index_numerator || !last_factor)
            goto allocation_failure;
        goto model_ready;
    }
    if (symbolic_geometric_expression) {
        last_factor = strdup("");
        if (!last_factor)
            goto allocation_failure;
        goto model_ready;
    }
    coefficients = calloc(ellipsis_index, sizeof(*coefficients));
    if (!coefficients)
        goto allocation_failure;
    series_start = ellipsis_index;
    while (series_start > 0u) {
        number_t coefficient = NUM_NAN;
        char *factor = NULL;

        if (!expr_series_parse_term(text, terms[series_start - 1u], &coefficient, &factor))
            break;
        if (last_factor && strcmp(factor, last_factor) != 0) {
            num_destroy(&coefficient);
            free(factor);
            break;
        }
        if (!last_factor) {
            last_factor = factor;
            factor = NULL;
        }
        free(factor);
        coefficients[coefficient_count++] = coefficient;
        --series_start;
    }
    if (coefficient_count < 2u)
        goto unsupported;
    for (size_t i = 0u; i < coefficient_count / 2u; ++i) {
        number_t swap = coefficients[i];

        coefficients[i] = coefficients[coefficient_count - i - 1u];
        coefficients[coefficient_count - i - 1u] = swap;
    }

    if (symbolic_geometric) {
        if (!expr_series_geometric_prefix_matches(coefficients, coefficient_count, symbolic_geometric_ratio))
            goto unsupported;
        result.geometric_first = num_clone(coefficients[0]);
        result.geometric_ratio = num_clone(symbolic_geometric_ratio);
        result.kind = EXPR_SERIES_MODEL_GEOMETRIC;
    } else if (symbolic_endpoint) {
        int inferred_exponent = 0;

        if (!expr_series_inverse_index_power_exponent(coefficients, coefficient_count, &inferred_exponent) ||
            inferred_exponent != symbolic_exponent ||
            !expr_series_symbolic_numerator_matches(coefficients[0], last_factor, symbolic_numerator))
            goto unsupported;
        result.inverse_index_first = num_clone(coefficients[0]);
        result.inverse_index_exponent = inferred_exponent;
        result.kind = EXPR_SERIES_MODEL_INVERSE_INDEX_POWER;
    } else {
        if (strcmp(last_factor, terminal_factor) != 0 ||
            !expr_series_sum_coefficients(coefficients, coefficient_count, last, &result))
            goto unsupported;
    }

model_ready:
    if (result.kind == EXPR_SERIES_MODEL_INVERSE_INDEX_POWER && result.symbolic_endpoint) {
        number_t endpoint_value = expr_eval(result.symbolic_endpoint);

        endpoint_positive_infinity =
            num_is_real(endpoint_value) && num_is_inf(endpoint_value) && num_get_sign(endpoint_value) > 0;
        if (!endpoint_positive_infinity && lookup_binding) {
            char *endpoint_name = expr_to_string(result.symbolic_endpoint, style_UNBOUND);

            num_destroy(&endpoint_value);
            endpoint_value = NUM_NAN;
            if (endpoint_name && lookup_binding(lookup_context, endpoint_name, &endpoint_value))
                endpoint_positive_infinity =
                    num_is_real(endpoint_value) && num_is_inf(endpoint_value) && num_get_sign(endpoint_value) > 0;
            free(endpoint_name);
        }
        num_destroy(&endpoint_value);
    }
    *display_TeX_out = expr_series_display_TeX(text, terms, term_count, series_start, ellipsis_index, last_factor,
                                              &result, endpoint_positive_infinity);
    if (!*display_TeX_out)
        goto allocation_failure;

    output = string_new();
    if (!output ||
        expr_series_append_slice(output, text, 0u, expr_series_trim(text, terms[series_start]).start) != 0)
        goto allocation_failure;
    if (result.kind == EXPR_SERIES_MODEL_GEOMETRIC && result.symbolic_endpoint) {
        if (expr_series_append_symbolic_geometric_formula(output, &result) != 0)
            goto allocation_failure;
    } else if (result.kind == EXPR_SERIES_MODEL_INVERSE_INDEX_POWER && result.symbolic_exponent) {
        if (expr_series_append_symbolic_power_formula(output, &result, lookup_binding, lookup_context,
                                                      endpoint_positive_infinity,
                                                      domain_specialised_out) != 0)
            goto allocation_failure;
    } else if (result.kind == EXPR_SERIES_MODEL_INVERSE_INDEX_POWER &&
        (result.symbolic_endpoint || result.inverse_index_exponent == 2)) {
        if (expr_series_append_inverse_index_formula(output, &result, last_factor, endpoint_positive_infinity,
                                                     domain_specialised_out) != 0)
            goto allocation_failure;
    } else if (!*last_factor) {
        string_t *sum_text = num_to_string(result.sum);

        if (!sum_text || string_append_cstr(output, string_c_str(sum_text)) != 0) {
            string_free(sum_text);
            goto allocation_failure;
        }
        string_free(sum_text);
    } else if (num_is_one(result.sum)) {
        if (string_append_cstr(output, last_factor) != 0)
            goto allocation_failure;
    } else if (num_eq(result.sum, NUM_NEG_ONE)) {
        if (string_append_char(output, '-') != 0 || string_append_cstr(output, last_factor) != 0)
            goto allocation_failure;
    } else {
        string_t *sum_text = num_to_string(result.sum);

        if (!sum_text || string_append_format(output, "%S*(%s)", sum_text, last_factor) < 0) {
            string_free(sum_text);
            goto allocation_failure;
        }
        string_free(sum_text);
    }
    if (expr_series_append_slice(output, text, expr_series_trim(text, terms[ellipsis_index + 1u]).end,
                                strlen(text)) != 0)
        goto allocation_failure;

    *expanded_out = true;
    goto cleanup;

unsupported:
    fprintf(stderr, "expr_from_string: ellipsis must abbreviate an exact polynomial, index-power, inverse-index-power, "
                    "or geometric sequence of like additive terms\n");
    output = NULL;
    goto cleanup;

allocation_failure:
    string_free(*display_TeX_out);
    *display_TeX_out = NULL;
    string_free(output);
    output = NULL;

cleanup:
    expr_series_result_clear(&result);
    expr_free(symbolic_numerator);
    num_destroy(&symbolic_geometric_ratio);
    num_destroy(&last);
    expr_series_destroy_numbers(coefficients, coefficient_count);
    free(last_factor);
    free(terminal_factor);
    free(terms);
    return output;
}

string_t *expr_expand_series_text(string_view_t source, string_t **display_TeX_out,
                                  expr_series_binding_lookup_fn lookup_binding, void *lookup_context,
                                  bool *domain_specialised_out)
{
    string_t *current = string_from_view(&source);
    string_t *display_TeX = NULL;

    if (display_TeX_out)
        *display_TeX_out = NULL;
    if (domain_specialised_out)
        *domain_specialised_out = false;

    if (!current)
        return NULL;

    for (size_t pass = 0u; pass < 32u; ++pass) {
        string_t *next;
        string_t *next_display_TeX = NULL;
        bool expanded = false;

        bool pass_domain_specialised = false;

        next = expr_series_expand_once(current, &expanded, &next_display_TeX, lookup_binding, lookup_context,
                                       &pass_domain_specialised);
        if (pass_domain_specialised && domain_specialised_out)
            *domain_specialised_out = true;
        string_free(current);
        if (!next) {
            string_free(display_TeX);
            return NULL;
        }
        current = next;
        if (!expanded) {
            if (display_TeX_out)
                *display_TeX_out = display_TeX;
            else
                string_free(display_TeX);
            return current;
        }
        string_free(display_TeX);
        display_TeX = next_display_TeX;
    }

    string_free(display_TeX);
    string_free(current);
    fprintf(stderr, "expr_from_text: too many sequence ellipses\n");
    return NULL;
}
