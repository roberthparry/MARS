#include <ctype.h>
#include <limits.h>
#include <math.h>
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
    EXPR_SERIES_MODEL_ALTERNATING_ARITHMETIC_RECIPROCAL,
} expr_series_model_kind_t;

typedef enum {
    EXPR_SERIES_TRIGONOMETRIC_NONE,
    EXPR_SERIES_TRIGONOMETRIC_SIN,
    EXPR_SERIES_TRIGONOMETRIC_COS,
    EXPR_SERIES_TRIGONOMETRIC_SINH,
    EXPR_SERIES_TRIGONOMETRIC_COSH,
} expr_series_trigonometric_kind_t;

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
    long alternating_reciprocal_step;
    expr_t *inverse_index_numerator;
    expr_t *symbolic_exponent;
    bool symbolic_direct_power;
    size_t endpoint;
    number_t exact_endpoint;
    expr_t *symbolic_endpoint;
    expr_series_model_kind_t kind;
} expr_series_result_t;

static void expr_series_destroy_numbers(number_t *values, size_t count);
static bool expr_series_number_to_long(number_t value, long *value_out);
static int expr_series_append_slice(string_t *output, const char *text, size_t start, size_t end);
static const char *expr_series_display_index_name(const char *text);

static void expr_series_result_init(expr_series_result_t *result)
{
    *result = (expr_series_result_t){
        .sum = NUM_NAN,
        .geometric_first = NUM_NAN,
        .geometric_ratio = NUM_NAN,
        .inverse_index_first = NUM_NAN,
        .exact_endpoint = NUM_NAN,
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
    num_destroy(&result->exact_endpoint);
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
    const char *cursor;
    size_t dot_count = 0u;
    bool match;

    span = expr_series_trim(text, span);
    term = expr_series_copy_span(text, span);
    if (!term)
        return false;
    cursor = term;
    if (*cursor == '+' || *cursor == '-')
        ++cursor;
    while (isspace((unsigned char)*cursor))
        ++cursor;
    if (strcmp(cursor, "…") == 0) {
        free(term);
        return true;
    }
    while (*cursor == '.') {
        ++dot_count;
        ++cursor;
    }
    while (isspace((unsigned char)*cursor))
        ++cursor;
    match = dot_count >= 3u && *cursor == '\0';
    free(term);
    return match;
}

static const char *expr_series_skip_spaces(const char *cursor)
{
    while (cursor && isspace((unsigned char)*cursor))
        ++cursor;
    return cursor;
}

static bool expr_series_parse_unit_reciprocal_factor(const char **cursor_inout, int *reciprocal_sign_out,
                                                     char **denominator_out)
{
    const char *cursor = expr_series_skip_spaces(*cursor_inout);
    const char *start;
    int reciprocal_sign;

    *reciprocal_sign_out = 0;
    *denominator_out = NULL;
    if (*cursor++ != '(')
        return false;
    cursor = expr_series_skip_spaces(cursor);
    if (*cursor++ != '1')
        return false;
    cursor = expr_series_skip_spaces(cursor);
    if (*cursor == '+')
        reciprocal_sign = 1;
    else if (*cursor == '-')
        reciprocal_sign = -1;
    else
        return false;
    ++cursor;
    cursor = expr_series_skip_spaces(cursor);
    if (*cursor++ != '1')
        return false;
    cursor = expr_series_skip_spaces(cursor);
    if (*cursor++ != '/')
        return false;
    cursor = expr_series_skip_spaces(cursor);
    start = cursor;
    while (isalnum((unsigned char)*cursor) || *cursor == '_' || (unsigned char)*cursor >= 0x80u)
        ++cursor;
    if (cursor == start)
        return false;
    *denominator_out = strndup(start, (size_t)(cursor - start));
    if (!*denominator_out)
        return false;
    cursor = expr_series_skip_spaces(cursor);
    if (*cursor++ != ')') {
        free(*denominator_out);
        *denominator_out = NULL;
        return false;
    }
    *reciprocal_sign_out = reciprocal_sign;
    *cursor_inout = cursor;
    return true;
}

static string_t *expr_series_expand_telescoping_product(const string_t *source, bool *expanded_out,
                                                        string_t **display_TeX_out)
{
    const char *cursor = string_c_str(source);
    const char *ellipsis;
    char *denominators[4] = {NULL, NULL, NULL, NULL};
    string_t *expanded = NULL;
    string_t *TeX = NULL;
    bool matched = false;

    *expanded_out = false;
    *display_TeX_out = NULL;
    for (size_t i = 0u; i < 3u; ++i) {
        int reciprocal_sign = 0;

        if (!expr_series_parse_unit_reciprocal_factor(&cursor, &reciprocal_sign, &denominators[i]) ||
            reciprocal_sign != 1)
            goto cleanup;
    }
    if (strcmp(denominators[0], "2") != 0 || strcmp(denominators[1], "3") != 0 ||
        strcmp(denominators[2], "4") != 0)
        goto cleanup;
    cursor = expr_series_skip_spaces(cursor);
    ellipsis = cursor;
    while (*cursor == '.')
        ++cursor;
    if ((size_t)(cursor - ellipsis) < 3u)
        goto cleanup;
    {
        int reciprocal_sign = 0;

        if (!expr_series_parse_unit_reciprocal_factor(&cursor, &reciprocal_sign, &denominators[3]) ||
            reciprocal_sign != 1 || *expr_series_skip_spaces(cursor) != '\0')
            goto cleanup;
    }

    expanded = string_new();
    TeX = string_new();
    if (!expanded || !TeX || string_append_format(expanded, "((%s)+1)/2", denominators[3]) < 0 ||
        string_append_format(TeX,
                             "\\prod_{k=2}^{%s}\\left(1+\\frac{1}{k}\\right)="
                             "\\frac{%s+1}{2}",
                             denominators[3], denominators[3]) < 0)
        goto cleanup;
    matched = true;

cleanup:
    for (size_t i = 0u; i < 4u; ++i)
        free(denominators[i]);
    if (!matched) {
        string_free(expanded);
        string_free(TeX);
        return string_clone(source);
    }
    *expanded_out = true;
    *display_TeX_out = TeX;
    return expanded;
}

static bool expr_series_parse_ellipsis_at_end(const char *cursor)
{
    const char *ellipsis = expr_series_skip_spaces(cursor);
    size_t dot_count = 0u;

    if (strncmp(ellipsis, "\xE2\x80\xA6", 3u) == 0)
        return *expr_series_skip_spaces(ellipsis + 3u) == '\0';
    while (*ellipsis == '.') {
        ++dot_count;
        ++ellipsis;
    }
    return dot_count >= 3u && *expr_series_skip_spaces(ellipsis) == '\0';
}

static string_t *expr_series_expand_infinite_prime_product(const string_t *source, bool *expanded_out,
                                                           string_t **display_TeX_out)
{
    const char *cursor = string_c_str(source);
    number_t expected_prime = num_create_from_long(2L);
    size_t factor_count = 0u;
    string_t *expanded = NULL;
    string_t *TeX = NULL;
    int product_sign = 0;

    *expanded_out = false;
    *display_TeX_out = NULL;
    while (!expr_series_parse_ellipsis_at_end(cursor)) {
        char *denominator = NULL;
        number_t actual_prime;
        number_t next_prime;
        int reciprocal_sign = 0;

        if (!expr_series_parse_unit_reciprocal_factor(&cursor, &reciprocal_sign, &denominator))
            goto cleanup;
        if (product_sign == 0)
            product_sign = reciprocal_sign;
        else if (reciprocal_sign != product_sign) {
            free(denominator);
            goto cleanup;
        }
        actual_prime = num_create_from_string(denominator);
        free(denominator);
        if (!num_is_exact(actual_prime) || !num_is_integer(actual_prime) || !num_eq(actual_prime, expected_prime)) {
            num_destroy(&actual_prime);
            goto cleanup;
        }
        num_destroy(&actual_prime);
        ++factor_count;
        next_prime = num_next_prime(expected_prime);
        num_destroy(&expected_prime);
        expected_prime = next_prime;
    }
    if (factor_count < 4u)
        goto cleanup;

    expanded = string_new();
    TeX = string_new();
    if (!expanded || !TeX ||
        string_append_cstr(expanded, product_sign > 0 ? "inf" : "0") != 0 ||
        string_append_cstr(TeX, product_sign > 0
                                    ? "\\prod_{p\\in\\mathbb{P}}\\left(1+\\frac{1}{p}\\right)=+\\infty"
                                    : "\\prod_{p\\in\\mathbb{P}}\\left(1-\\frac{1}{p}\\right)=0") != 0)
        goto cleanup;
    *expanded_out = true;
    *display_TeX_out = TeX;
    num_destroy(&expected_prime);
    return expanded;

cleanup:
    num_destroy(&expected_prime);
    string_free(expanded);
    string_free(TeX);
    return string_clone(source);
}

static bool expr_series_exponent_sign(const char *text, size_t length, size_t pos)
{
    return pos > 1u && pos + 1u < length && (text[pos - 1u] == 'e' || text[pos - 1u] == 'E') &&
           (isdigit((unsigned char)text[pos - 2u]) || text[pos - 2u] == '.') &&
           isdigit((unsigned char)text[pos + 1u]);
}

static bool expr_series_sign_is_unary(const char *text, size_t start, size_t pos)
{
    size_t first = start;
    size_t previous = pos;

    while (first < pos && isspace((unsigned char)text[first]))
        ++first;
    if (first == pos)
        return true;
    while (previous > start && isspace((unsigned char)text[previous - 1u]))
        --previous;
    if (previous == start)
        return true;
    return strchr("+-*/^(,[{|=;", text[previous - 1u]) != NULL;
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
            case '-':
                if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0 &&
                    !expr_series_exponent_sign(text, length, i) && !expr_series_sign_is_unary(text, start, i)) {
                    terms[count++] = (expr_series_span_t){start, i};
                    start = text[i] == '-' ? i : i + 1u;
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
    expr_t *reciprocal_endpoint = NULL;
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
    bool endpoint_is_positive_integer = false;
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
    /* Parsing a literal reciprocal power can produce (1/N)^s instead of 1/(N^s). */
    if (symbolic_exponent && numerator == EXPR_ONE && num_is_exact(endpoint_value) &&
        num_is_real(endpoint_value) && num_gt(endpoint_value, NUM_ZERO) && num_lt(endpoint_value, NUM_ONE)) {
        number_t reciprocal = num_inv(endpoint_value);

        if (num_is_exact(reciprocal) && num_is_integer(reciprocal)) {
            reciprocal_endpoint = expr_new_const(reciprocal);
            endpoint = reciprocal_endpoint;
            num_destroy(&endpoint_value);
            endpoint_value = num_clone(reciprocal);
            direct_power = !direct_power;
        }
        num_destroy(&reciprocal);
    }
    endpoint_is_positive_infinity = literal_positive_infinity ||
                                    (num_is_real(endpoint_value) && num_is_inf(endpoint_value) &&
                                     num_get_sign(endpoint_value) > 0);
    endpoint_is_symbol = endpoint && (expr_is_variable(endpoint) || expr_is_named_const(endpoint)) &&
                         num_is_nan(endpoint_value);
    endpoint_is_positive_integer = symbolic_exponent && endpoint && num_is_exact(endpoint_value) &&
                                   num_is_real(endpoint_value) && num_is_finite(endpoint_value) &&
                                   num_is_integer(endpoint_value) && num_gt(endpoint_value, NUM_ZERO);
    if ((!symbolic_exponent && exponent_value == 0) || !endpoint ||
        (!endpoint_is_symbol && !endpoint_is_positive_infinity && !endpoint_is_positive_integer))
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
    expr_free(reciprocal_endpoint);
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

static bool expr_series_match_positive_long_scale(const expr_t *expression, const expr_t *base, long *scale_out)
{
    number_t scale;
    const expr_t *scaled_base = NULL;
    bool matched = false;

    if (expr_series_simplified_equal(expression, base)) {
        *scale_out = 1L;
        return true;
    }
    scale = num_new();
    if (expr_match_scaled_expr(expression, &scale, &scaled_base) && scaled_base &&
        expr_series_simplified_equal(scaled_base, base) && expr_series_number_to_long(scale, scale_out) &&
        *scale_out > 0L)
        matched = true;
    num_destroy(&scale);
    return matched;
}

static bool expr_series_parse_symbolic_alternating_reciprocal_endpoint(const char *text, expr_series_span_t span,
                                                                       number_t *first_out, long *step_out,
                                                                       expr_t **endpoint_out)
{
    char *term_text = NULL;
    expr_t *expression = NULL;
    const expr_t *numerator = NULL;
    const expr_t *denominator = NULL;
    const expr_t *alternating_power = NULL;
    const expr_t *minus_one = NULL;
    const expr_t *endpoint = NULL;
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *linear = NULL;
    const expr_t *offset = NULL;
    number_t first = num_new();
    number_t base_value = num_new();
    number_t offset_value = num_new();
    number_t endpoint_value = num_new();
    bool subtract = false;
    long step = 0L;
    bool matched = false;

    *first_out = NUM_NAN;
    *step_out = 0L;
    *endpoint_out = NULL;
    span = expr_series_trim(text, span);
    term_text = expr_series_copy_span(text, span);
    expression = term_text ? expr_from_string(term_text, NULL) : NULL;
    free(term_text);
    if (!expression || !expr_match_div_expr(expression, &numerator, &denominator) || !numerator || !denominator)
        goto cleanup;

    if (expr_match_scaled_expr(numerator, &first, &alternating_power) && alternating_power) {
        /* The leading coefficient was separated from (-1)^n. */
    } else {
        num_destroy(&first);
        first = num_clone(NUM_ONE);
        alternating_power = numerator;
    }
    if (!num_is_exact(first) || !num_is_finite(first) || num_is_zero(first) ||
        !expr_match_pow_expr(alternating_power, &minus_one, &endpoint) || !minus_one || !endpoint ||
        !expr_match_const_value(minus_one, &base_value) || !num_eq(base_value, NUM_NEG_ONE))
        goto cleanup;
    num_destroy(&endpoint_value);
    endpoint_value = expr_eval(endpoint);
    if ((!expr_is_variable(endpoint) && !expr_is_named_const(endpoint)) || !num_is_nan(endpoint_value))
        goto cleanup;

    if (!expr_match_add_sub_expr(denominator, &left, &right, &subtract) || subtract || !left || !right)
        goto cleanup;
    if (expr_match_const_value(right, &offset_value) && num_is_one(offset_value)) {
        linear = left;
        offset = right;
    } else {
        num_destroy(&offset_value);
        offset_value = num_new();
        if (!expr_match_const_value(left, &offset_value) || !num_is_one(offset_value))
            goto cleanup;
        linear = right;
        offset = left;
    }
    if (!offset || !expr_series_match_positive_long_scale(linear, endpoint, &step))
        goto cleanup;

    *first_out = first;
    *step_out = step;
    *endpoint_out = expr_clone(endpoint);
    first = NUM_NAN;
    matched = *endpoint_out != NULL;

cleanup:
    if (!matched) {
        num_destroy(first_out);
        *first_out = NUM_NAN;
        *step_out = 0L;
        expr_free(*endpoint_out);
        *endpoint_out = NULL;
    }
    num_destroy(&endpoint_value);
    num_destroy(&offset_value);
    num_destroy(&base_value);
    num_destroy(&first);
    expr_free(expression);
    return matched;
}

static bool expr_series_parse_numeric_fraction(const char *text, expr_series_span_t span, number_t *numerator_out,
                                               number_t *denominator_out)
{
    char *term_text = NULL;
    char *slash = NULL;
    char *numerator_text = NULL;
    char *denominator_text = NULL;
    char *end = NULL;
    number_t numerator = NUM_NAN;
    number_t denominator = NUM_NAN;
    bool matched = false;

    *numerator_out = NUM_NAN;
    *denominator_out = NUM_NAN;
    span = expr_series_trim(text, span);
    term_text = expr_series_copy_span(text, span);
    slash = term_text ? strchr(term_text, '/') : NULL;
    if (!slash || strchr(slash + 1, '/'))
        goto cleanup;
    *slash = '\0';
    numerator_text = term_text;
    denominator_text = slash + 1;
    while (isspace((unsigned char)*numerator_text))
        ++numerator_text;
    while (isspace((unsigned char)*denominator_text))
        ++denominator_text;
    end = numerator_text + strlen(numerator_text);
    while (end > numerator_text && isspace((unsigned char)end[-1]))
        *--end = '\0';
    end = denominator_text + strlen(denominator_text);
    while (end > denominator_text && isspace((unsigned char)end[-1]))
        *--end = '\0';
    if (!*numerator_text || !*denominator_text)
        goto cleanup;
    numerator = num_create_from_string(numerator_text);
    denominator = num_create_from_string(denominator_text);
    if (!num_is_exact(numerator) || !num_is_integer(numerator) || !num_is_exact(denominator) ||
        !num_is_integer(denominator) || !num_gt(denominator, NUM_ZERO))
        goto cleanup;

    *numerator_out = numerator;
    *denominator_out = denominator;
    numerator = NUM_NAN;
    denominator = NUM_NAN;
    matched = true;

cleanup:
    num_destroy(&denominator);
    num_destroy(&numerator);
    free(term_text);
    return matched;
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

static expr_series_trigonometric_kind_t expr_series_match_trigonometric_term(const expr_t *term,
                                                                             const expr_t **argument_out)
{
    *argument_out = NULL;
    if (!term)
        return EXPR_SERIES_TRIGONOMETRIC_NONE;
    if (expr_match_sin_expr(term, argument_out))
        return EXPR_SERIES_TRIGONOMETRIC_SIN;
    if (expr_match_cos_expr(term, argument_out))
        return EXPR_SERIES_TRIGONOMETRIC_COS;
    if (expr_is_unary_pattern_kind(term, EXPR_PATTERN_UNARY_SINH) && expr_match_unary_expr(term, argument_out))
        return EXPR_SERIES_TRIGONOMETRIC_SINH;
    if (expr_is_unary_pattern_kind(term, EXPR_PATTERN_UNARY_COSH) && expr_match_unary_expr(term, argument_out))
        return EXPR_SERIES_TRIGONOMETRIC_COSH;
    return EXPR_SERIES_TRIGONOMETRIC_NONE;
}

static string_t *expr_series_expand_trigonometric_progression(const string_t *source, bool *expanded_out,
                                                              string_t **display_TeX_out)
{
    const char *text = string_c_str(source);
    expr_series_span_t *terms = NULL;
    size_t term_count = 0u;
    size_t ellipsis_index = SIZE_MAX;
    expr_series_trigonometric_kind_t trigonometric_kind = EXPR_SERIES_TRIGONOMETRIC_NONE;
    bool unit_angle_step = false;
    expr_t *angle_step = NULL;
    expr_t *endpoint_term = NULL;
    expr_t *endpoint = NULL;
    char *endpoint_text = NULL;
    char *endpoint_TeX = NULL;
    char *angle_step_text = NULL;
    char *angle_step_TeX = NULL;
    string_t *expanded = NULL;
    string_t *TeX = NULL;
    bool matched = false;

    *expanded_out = false;
    *display_TeX_out = NULL;
    if (!expr_series_split_terms(text, &terms, &term_count))
        return NULL;
    for (size_t i = 0u; i < term_count; ++i) {
        if (expr_series_is_ellipsis(text, terms[i])) {
            ellipsis_index = i;
            break;
        }
    }
    if (ellipsis_index < 3u || ellipsis_index == SIZE_MAX || ellipsis_index + 2u != term_count)
        goto cleanup;

    for (size_t i = 0u; i < ellipsis_index; ++i) {
        expr_t *term = expr_series_parse_term_expression(text, terms[i]);
        expr_t *expanded_term = term ? expr_expand_preserved_for_display(term) : NULL;
        const expr_t *matched_term = expanded_term ? expanded_term : term;
        const expr_t *argument = NULL;
        number_t argument_value = NUM_NAN;
        number_t expected = num_create_from_long((long)i + 1L);
        const expr_series_trigonometric_kind_t term_kind =
            expr_series_match_trigonometric_term(matched_term, &argument);

        if (argument)
            argument_value = expr_eval(argument);
        if (term_kind == EXPR_SERIES_TRIGONOMETRIC_NONE ||
            (trigonometric_kind != EXPR_SERIES_TRIGONOMETRIC_NONE && term_kind != trigonometric_kind)) {
            num_destroy(&expected);
            num_destroy(&argument_value);
            expr_free(expanded_term);
            expr_free(term);
            goto cleanup;
        }
        if (i == 0u) {
            unit_angle_step = num_is_exact(argument_value) && num_eq(argument_value, NUM_ONE);
            if (!unit_angle_step &&
                ((!expr_is_variable(argument) && !expr_is_named_const(argument)) || !num_is_nan(argument_value))) {
                num_destroy(&expected);
                num_destroy(&argument_value);
                expr_free(expanded_term);
                expr_free(term);
                goto cleanup;
            }
            if (!unit_angle_step)
                angle_step = expr_clone(argument);
        } else if (unit_angle_step) {
            if (!num_is_exact(argument_value) || !num_eq(argument_value, expected)) {
                num_destroy(&expected);
                num_destroy(&argument_value);
                expr_free(expanded_term);
                expr_free(term);
                goto cleanup;
            }
        } else {
            long scale = 0L;

            if (!angle_step || !expr_series_match_positive_long_scale(argument, angle_step, &scale) ||
                scale != (long)i + 1L) {
                num_destroy(&expected);
                num_destroy(&argument_value);
                expr_free(expanded_term);
                expr_free(term);
                goto cleanup;
            }
        }
        trigonometric_kind = term_kind;
        num_destroy(&expected);
        num_destroy(&argument_value);
        expr_free(expanded_term);
        expr_free(term);
    }

    endpoint_term = expr_series_parse_term_expression(text, terms[ellipsis_index + 1u]);
    {
        expr_t *expanded_endpoint_term = endpoint_term ? expr_expand_preserved_for_display(endpoint_term) : NULL;
        const expr_t *matched_endpoint_term = expanded_endpoint_term ? expanded_endpoint_term : endpoint_term;
        const expr_t *argument = NULL;
        number_t endpoint_value = NUM_NAN;

        const bool endpoint_matches =
            expr_series_match_trigonometric_term(matched_endpoint_term, &argument) == trigonometric_kind;
        const expr_t *endpoint_argument = argument;

        if (!endpoint_matches || !argument) {
            expr_free(expanded_endpoint_term);
            goto cleanup;
        }
        if (!unit_angle_step) {
            const expr_t *left = NULL;
            const expr_t *right = NULL;

            if (!expr_match_mul_expr(argument, &left, &right) || !left || !right) {
                expr_free(expanded_endpoint_term);
                goto cleanup;
            }
            if (expr_series_simplified_equal(left, angle_step))
                endpoint_argument = right;
            else if (expr_series_simplified_equal(right, angle_step))
                endpoint_argument = left;
            else {
                expr_free(expanded_endpoint_term);
                goto cleanup;
            }
        }
        endpoint_value = expr_eval(endpoint_argument);
        if ((!expr_is_variable(endpoint_argument) && !expr_is_named_const(endpoint_argument)) ||
            !num_is_nan(endpoint_value)) {
            num_destroy(&endpoint_value);
            expr_free(expanded_endpoint_term);
            goto cleanup;
        }
        num_destroy(&endpoint_value);
        endpoint = expr_clone(endpoint_argument);
        expr_free(expanded_endpoint_term);
    }
    endpoint_text = endpoint ? expr_to_string(endpoint, style_UNBOUND) : NULL;
    endpoint_TeX = endpoint ? expr_to_TeX_body(endpoint) : NULL;
    angle_step_text = angle_step ? expr_to_string(angle_step, style_UNBOUND) : NULL;
    angle_step_TeX = angle_step ? expr_to_TeX_body(angle_step) : NULL;
    expanded = string_new();
    TeX = string_new();
    if (!endpoint_text || !endpoint_TeX || (!unit_angle_step && (!angle_step_text || !angle_step_TeX)) ||
        !expanded || !TeX)
        goto cleanup;
    {
        const bool hyperbolic = trigonometric_kind == EXPR_SERIES_TRIGONOMETRIC_SINH ||
                                trigonometric_kind == EXPR_SERIES_TRIGONOMETRIC_COSH;
        const bool sine_family = trigonometric_kind == EXPR_SERIES_TRIGONOMETRIC_SIN ||
                                 trigonometric_kind == EXPR_SERIES_TRIGONOMETRIC_SINH;
        const char *first_function = hyperbolic ? "sinh" : "sin";
        const char *second_function = sine_family ? first_function : (hyperbolic ? "cosh" : "cos");
        const char *TeX_function = trigonometric_kind == EXPR_SERIES_TRIGONOMETRIC_SIN    ? "\\sin"
                                   : trigonometric_kind == EXPR_SERIES_TRIGONOMETRIC_COS  ? "\\cos"
                                   : trigonometric_kind == EXPR_SERIES_TRIGONOMETRIC_SINH ? "\\sinh"
                                                                                           : "\\cosh";

        if (unit_angle_step) {
            if (string_append_format(expanded, "%s((%s)/2)*%s(((%s)+1)/2)/%s(1/2)", first_function,
                                     endpoint_text, second_function, endpoint_text, first_function) < 0 ||
                string_append_format(TeX, "\\sum_{k=1}^{%s}%s(k)", endpoint_TeX, TeX_function) < 0)
                goto cleanup;
        } else if (string_append_format(expanded, "%s((%s)*(%s)/2)*%s(((%s)+1)*(%s)/2)/%s((%s)/2)",
                                        first_function, endpoint_text, angle_step_text, second_function,
                                        endpoint_text, angle_step_text, first_function, angle_step_text) < 0 ||
                   string_append_format(TeX, "\\sum_{k=1}^{%s}%s(k\\mkern-2mu %s)", endpoint_TeX,
                                        TeX_function, angle_step_TeX) < 0) {
            goto cleanup;
        }
    }
    matched = true;

cleanup:
    free(angle_step_TeX);
    free(angle_step_text);
    free(endpoint_TeX);
    free(endpoint_text);
    expr_free(endpoint);
    expr_free(endpoint_term);
    expr_free(angle_step);
    free(terms);
    if (!matched) {
        string_free(TeX);
        string_free(expanded);
        return string_clone(source);
    }
    *expanded_out = true;
    *display_TeX_out = TeX;
    return expanded;
}

static bool expr_series_match_weighted_trigonometric_term(const expr_t *term, long index,
                                                          expr_series_trigonometric_kind_t expected_kind,
                                                          const expr_t *angle_step)
{
    const expr_t *numerator = term;
    const expr_t *denominator = NULL;
    const expr_t *argument = NULL;
    expr_t *expanded = term ? expr_expand_preserved_for_display(term) : NULL;
    const expr_t *matched = expanded ? expanded : term;
    number_t denominator_value = NUM_NAN;
    number_t expected_denominator = num_create_from_long(index);
    bool matched_term = false;

    if (index > 1L) {
        if (!expr_match_div_expr(matched, &numerator, &denominator) || !numerator || !denominator)
            goto cleanup;
        denominator_value = expr_eval(denominator);
        if (!num_is_exact(denominator_value) || !num_eq(denominator_value, expected_denominator))
            goto cleanup;
    }
    if (expr_series_match_trigonometric_term(numerator, &argument) != expected_kind || !argument)
        goto cleanup;
    if (index == 1L)
        matched_term = expr_series_simplified_equal(argument, angle_step);
    else {
        long scale = 0L;

        matched_term = expr_series_match_positive_long_scale(argument, angle_step, &scale) && scale == index;
    }

cleanup:
    num_destroy(&expected_denominator);
    num_destroy(&denominator_value);
    expr_free(expanded);
    return matched_term;
}

static string_t *expr_series_expand_weighted_trigonometric_progression(const string_t *source, bool *expanded_out,
                                                                       string_t **display_TeX_out)
{
    const char *text = string_c_str(source);
    expr_series_span_t *terms = NULL;
    size_t term_count = 0u;
    size_t ellipsis_index = SIZE_MAX;
    expr_series_trigonometric_kind_t kind = EXPR_SERIES_TRIGONOMETRIC_NONE;
    expr_t *first = NULL;
    expr_t *expanded_first = NULL;
    const expr_t *first_argument = NULL;
    expr_t *angle_step = NULL;
    expr_t *endpoint_term = NULL;
    expr_t *expanded_endpoint = NULL;
    const expr_t *endpoint_numerator = NULL;
    const expr_t *endpoint_denominator = NULL;
    const expr_t *endpoint_argument = NULL;
    const expr_t *endpoint_factor = NULL;
    expr_t *endpoint = NULL;
    char *angle_text = NULL;
    char *angle_TeX = NULL;
    char *endpoint_text = NULL;
    char *endpoint_TeX = NULL;
    string_t *output = NULL;
    string_t *TeX = NULL;
    bool matched = false;

    *expanded_out = false;
    *display_TeX_out = NULL;
    if (!expr_series_split_terms(text, &terms, &term_count))
        return NULL;
    for (size_t i = 0u; i < term_count; ++i) {
        if (expr_series_is_ellipsis(text, terms[i])) {
            ellipsis_index = i;
            break;
        }
    }
    if (ellipsis_index == SIZE_MAX || ellipsis_index < 3u || ellipsis_index + 1u >= term_count)
        goto cleanup;

    first = expr_series_parse_term_expression(text, terms[0]);
    expanded_first = first ? expr_expand_preserved_for_display(first) : NULL;
    kind = expr_series_match_trigonometric_term(expanded_first ? expanded_first : first, &first_argument);
    if (kind == EXPR_SERIES_TRIGONOMETRIC_NONE || !first_argument)
        goto cleanup;
    angle_step = expr_clone(first_argument);
    if (!angle_step)
        goto cleanup;
    for (size_t i = 0u; i < ellipsis_index; ++i) {
        expr_t *term = expr_series_parse_term_expression(text, terms[i]);
        const bool term_matches =
            term && expr_series_match_weighted_trigonometric_term(term, (long)i + 1L, kind, angle_step);

        expr_free(term);
        if (!term_matches)
            goto cleanup;
    }

    endpoint_term = expr_series_parse_term_expression(text, terms[ellipsis_index + 1u]);
    expanded_endpoint = endpoint_term ? expr_expand_preserved_for_display(endpoint_term) : NULL;
    if (!expr_match_div_expr(expanded_endpoint ? expanded_endpoint : endpoint_term, &endpoint_numerator,
                             &endpoint_denominator) ||
        !endpoint_numerator || !endpoint_denominator ||
        expr_series_match_trigonometric_term(endpoint_numerator, &endpoint_argument) != kind || !endpoint_argument)
        goto cleanup;
    if (expr_match_mul_expr(endpoint_argument, &endpoint_factor, &first_argument) && endpoint_factor &&
        first_argument && expr_series_simplified_equal(first_argument, angle_step) &&
        expr_series_simplified_equal(endpoint_factor, endpoint_denominator)) {
        endpoint = expr_clone(endpoint_denominator);
    } else if (expr_match_mul_expr(endpoint_argument, &first_argument, &endpoint_factor) && endpoint_factor &&
               first_argument && expr_series_simplified_equal(first_argument, angle_step) &&
               expr_series_simplified_equal(endpoint_factor, endpoint_denominator)) {
        endpoint = expr_clone(endpoint_denominator);
    }
    if (!endpoint || (!expr_is_variable(endpoint) && !expr_is_named_const(endpoint)))
        goto cleanup;

    angle_text = expr_to_string(angle_step, style_UNBOUND);
    angle_TeX = expr_to_TeX_body(angle_step);
    endpoint_text = expr_to_string(endpoint, style_UNBOUND);
    endpoint_TeX = expr_to_TeX_body(endpoint);
    output = string_new();
    TeX = string_new();
    if (!angle_text || !angle_TeX || !endpoint_text || !endpoint_TeX || !output || !TeX)
        goto cleanup;
    {
        const char *function_name = kind == EXPR_SERIES_TRIGONOMETRIC_SIN    ? "sin"
                                    : kind == EXPR_SERIES_TRIGONOMETRIC_COS  ? "cos"
                                    : kind == EXPR_SERIES_TRIGONOMETRIC_SINH ? "sinh"
                                                                            : "cosh";
        const char *TeX_name = kind == EXPR_SERIES_TRIGONOMETRIC_SIN    ? "\\sin"
                               : kind == EXPR_SERIES_TRIGONOMETRIC_COS  ? "\\cos"
                               : kind == EXPR_SERIES_TRIGONOMETRIC_SINH ? "\\sinh"
                                                                       : "\\cosh";

        if (expr_series_append_slice(output, text, 0u, expr_series_trim(text, terms[0]).start) != 0 ||
            string_append_format(output, "sum(k,1,%s,%s(k*(%s))/k)", endpoint_text, function_name, angle_text) < 0 ||
            expr_series_append_slice(output, text, expr_series_trim(text, terms[ellipsis_index + 1u]).end,
                                     strlen(text)) != 0 ||
            string_append_format(TeX, "\\sum_{k=1}^{%s}\\frac{%s(k\\mkern-2mu %s)}{k}", endpoint_TeX,
                                 TeX_name, angle_TeX) < 0)
            goto cleanup;
    }
    matched = true;

cleanup:
    free(endpoint_TeX);
    free(endpoint_text);
    free(angle_TeX);
    free(angle_text);
    expr_free(endpoint);
    expr_free(expanded_endpoint);
    expr_free(endpoint_term);
    expr_free(angle_step);
    expr_free(expanded_first);
    expr_free(first);
    free(terms);
    if (!matched) {
        string_free(TeX);
        string_free(output);
        return string_clone(source);
    }
    *expanded_out = true;
    *display_TeX_out = TeX;
    return output;
}

static expr_t *expr_series_unary_progression_endpoint(const expr_t *argument, const expr_t *base)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *candidate = NULL;
    number_t base_value = NUM_NAN;
    number_t endpoint_value = NUM_NAN;
    expr_t *endpoint = NULL;

    if (!argument || !base)
        return NULL;
    base_value = expr_eval(base);
    if (num_is_exact(base_value) && num_is_one(base_value)) {
        candidate = argument;
    } else if (expr_match_mul_expr(argument, &left, &right) && left && right) {
        if (expr_series_simplified_equal(left, base))
            candidate = right;
        else if (expr_series_simplified_equal(right, base))
            candidate = left;
    }
    if (!candidate || (!expr_is_variable(candidate) && !expr_is_named_const(candidate)))
        goto cleanup;
    endpoint_value = expr_eval(candidate);
    if (num_is_nan(endpoint_value))
        endpoint = expr_clone(candidate);

cleanup:
    num_destroy(&endpoint_value);
    num_destroy(&base_value);
    return endpoint;
}

static const char *expr_series_unary_progression_index_name(const expr_t *first, const expr_t *endpoint)
{
    static const char *const candidates[] = {"k", "j", "m", "l", "r", "s", "t"};
    expr_bindings_t *first_bindings = expr_bindings_from_expr_internal(first);
    expr_bindings_t *endpoint_bindings = expr_bindings_from_expr_internal(endpoint);
    const char *name = "index";

    if (!first_bindings || !endpoint_bindings)
        goto cleanup;
    for (size_t i = 0u; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (!expr_bindings_get(first_bindings, candidates[i]) && !expr_bindings_get(endpoint_bindings, candidates[i])) {
            name = candidates[i];
            break;
        }
    }

cleanup:
    expr_bindings_free(endpoint_bindings);
    expr_bindings_free(first_bindings);
    return name;
}

static string_t *expr_series_expand_unary_function_progression(const string_t *source, bool *expanded_out,
                                                                string_t **display_TeX_out)
{
    const char *text = string_c_str(source);
    expr_series_span_t *terms = NULL;
    size_t term_count = 0u;
    size_t ellipsis_index = SIZE_MAX;
    expr_t *first = NULL;
    const expr_t *first_argument = NULL;
    expr_t *endpoint_term = NULL;
    expr_t *endpoint = NULL;
    expr_t *index = NULL;
    expr_t *indexed_argument = NULL;
    expr_t *indexed_term = NULL;
    expr_t *lower = NULL;
    expr_t *summation = NULL;
    char *summation_text = NULL;
    char *summation_TeX = NULL;
    string_t *output = NULL;
    string_t *TeX = NULL;
    bool matched = false;

    *expanded_out = false;
    *display_TeX_out = NULL;
    if (!expr_series_split_terms(text, &terms, &term_count))
        return NULL;
    for (size_t i = 0u; i < term_count; ++i) {
        if (expr_series_is_ellipsis(text, terms[i])) {
            ellipsis_index = i;
            break;
        }
    }
    if (ellipsis_index == SIZE_MAX || ellipsis_index < 3u || ellipsis_index + 2u != term_count)
        goto cleanup;

    first = expr_series_parse_term_expression(text, terms[0]);
    if (!expr_match_reapplicable_unary_function(first, &first_argument) || !first_argument)
        goto cleanup;
    for (size_t i = 0u; i < ellipsis_index; ++i) {
        expr_t *term = expr_series_parse_term_expression(text, terms[i]);
        const expr_t *term_argument = NULL;
        long scale = 0L;
        const bool term_matches = expr_match_same_reapplicable_unary_function(term, first, &term_argument) &&
                                  term_argument &&
                                  expr_series_match_positive_long_scale(term_argument, first_argument, &scale) &&
                                  scale == (long)i + 1L;

        expr_free(term);
        if (!term_matches)
            goto cleanup;
    }

    endpoint_term = expr_series_parse_term_expression(text, terms[ellipsis_index + 1u]);
    {
        const expr_t *endpoint_argument = NULL;

        if (!expr_match_same_reapplicable_unary_function(endpoint_term, first, &endpoint_argument) ||
            !endpoint_argument)
            goto cleanup;
        endpoint = expr_series_unary_progression_endpoint(endpoint_argument, first_argument);
    }
    if (!endpoint)
        goto cleanup;

    index = expr_new_named_var(NUM_NAN, expr_series_unary_progression_index_name(first, endpoint));
    indexed_argument = index ? expr_mul(index, first_argument) : NULL;
    indexed_term = indexed_argument ? expr_apply_reapplicable_unary_function(first, indexed_argument) : NULL;
    lower = expr_new_const(NUM_ONE);
    summation = indexed_term && index && lower ? expr_new_finite_summation_range(indexed_term, index, lower, endpoint)
                                               : NULL;
    summation_text = summation ? expr_to_string(summation, style_UNBOUND) : NULL;
    summation_TeX = summation ? expr_to_TeX_body(summation) : NULL;
    output = string_new();
    TeX = string_new();
    if (!summation_text || !summation_TeX || !output || !TeX ||
        expr_series_append_slice(output, text, 0u, expr_series_trim(text, terms[0]).start) != 0 ||
        string_append_cstr(output, summation_text) != 0 ||
        expr_series_append_slice(output, text, expr_series_trim(text, terms[ellipsis_index + 1u]).end,
                                 strlen(text)) != 0 ||
        string_append_cstr(TeX, summation_TeX) != 0)
        goto cleanup;
    matched = true;

cleanup:
    free(summation_TeX);
    free(summation_text);
    expr_free(summation);
    expr_free(lower);
    expr_free(indexed_term);
    expr_free(indexed_argument);
    expr_free(index);
    expr_free(endpoint);
    expr_free(endpoint_term);
    expr_free(first);
    free(terms);
    if (!matched) {
        string_free(TeX);
        string_free(output);
        return string_clone(source);
    }
    *expanded_out = true;
    *display_TeX_out = TeX;
    return output;
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

    if (index == 1L && expr_series_expr_text_equal(expression, numerator)) {
        matched = true;
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

/* Retain an open power series as a summation, so its convergence domain cannot be discarded. */
static string_t *expr_series_expand_open_symbolic_power(const string_t *source, bool *expanded_out,
                                                       string_t **display_TeX_out)
{
    const char *text = string_c_str(source);
    expr_series_span_t *terms = NULL;
    size_t count = 0u;
    expr_t *numerator = NULL;
    expr_t *endpoint = NULL;
    expr_t *exponent = NULL;
    expr_t *power = NULL;
    expr_t *lower = NULL;
    expr_t *term = NULL;
    expr_t *result = NULL;
    expr_t *index = NULL;
    expr_t *upper = NULL;
    string_t *output = NULL;
    char *TeX = NULL;
    long last = 0L;
    long first = 0L;
    int literal_exponent = 0;
    bool direct = false;

    *expanded_out = false;
    *display_TeX_out = NULL;
    if (!expr_series_split_terms(text, &terms, &count) || count < 4u || count - 2u > LONG_MAX ||
        !expr_series_is_ellipsis(text, terms[count - 1u]) ||
        !expr_series_parse_symbolic_power_endpoint(text, terms[count - 2u], &numerator, &endpoint,
                                                   &exponent, &literal_exponent, &direct) || !exponent ||
        !expr_series_exact_positive_long(endpoint, &last) || last <= (long)(count - 2u))
        goto cleanup;
    first = last - (long)(count - 2u);
    /* Check every supplied term; irregular and prime prefixes must not become a zeta series. */
    for (size_t i = 0u; i + 1u < count; ++i)
        if (!expr_series_symbolic_power_term_matches(text, terms[i], first + (long)i,
                                                     numerator, exponent, direct))
            goto cleanup;

    index = expr_new_named_var(NUM_NAN, expr_series_display_index_name(text));
    power = index ? expr_pow_xp(index, exponent) : NULL;
    term = power ? (direct ? expr_mul_simplify_owned(expr_clone(numerator), expr_clone(power))
                           : expr_div(numerator, power)) : NULL;
    {
        number_t value = num_create_from_long(first);

        lower = expr_new_const(value);
        upper = expr_new_const(NUM_INF);
        num_destroy(&value);
    }
    result = term && lower && upper ? expr_new_finite_summation_range(term, index, lower, upper) : NULL;
    output = result ? expr_to_text(result, style_UNBOUND) : NULL;
    TeX = result ? expr_to_TeX_body(result) : NULL;
    if (output && TeX) {
        *display_TeX_out = string_new_with(TeX);
        *expanded_out = *display_TeX_out != NULL;
    }

cleanup:
    free(TeX);
    expr_free(result);
    expr_free(term);
    expr_free(lower);
    expr_free(upper);
    expr_free(index);
    expr_free(power);
    expr_free(exponent);
    expr_free(endpoint);
    expr_free(numerator);
    free(terms);
    if (*expanded_out)
        return output;
    string_free(output);
    return string_clone(source);
}

/* Recognise the prime prefix before lowering it to a native, primality-filtered finite sum. */
static string_t *expr_series_expand_prime_power_sum(const string_t *source, bool *expanded_out,
                                                    string_t **display_TeX_out)
{
    const char *text = string_c_str(source);
    expr_series_span_t *terms = NULL;
    size_t term_count = 0u;
    size_t ellipsis_index = SIZE_MAX;
    expr_t *numerator = NULL;
    expr_t *endpoint = NULL;
    expr_t *exponent = NULL;
    expr_t *index = NULL;
    expr_t *predicate = NULL;
    expr_t *power = NULL;
    expr_t *weighted = NULL;
    expr_t *term = NULL;
    expr_t *lower = NULL;
    expr_t *summation = NULL;
    number_t prime = num_create_from_long(2L);
    number_t endpoint_value = NUM_NAN;
    char *summation_text = NULL;
    char *summation_TeX = NULL;
    string_t *output = NULL;
    string_t *TeX = NULL;
    int literal_exponent = 0;
    bool direct_power = false;
    bool symbolic_endpoint = false;
    bool matched = false;

    *expanded_out = false;
    *display_TeX_out = NULL;
    if (!expr_series_split_terms(text, &terms, &term_count))
        goto cleanup;
    for (size_t i = 0u; i < term_count; ++i) {
        if (expr_series_is_ellipsis(text, terms[i])) {
            ellipsis_index = i;
            break;
        }
    }
    /* At least 2, 3, 5 distinguishes primes from consecutive integers. */
    if (ellipsis_index == SIZE_MAX || ellipsis_index < 3u || ellipsis_index + 2u != term_count ||
        !expr_series_parse_symbolic_power_endpoint(text, terms[ellipsis_index + 1u], &numerator, &endpoint,
                                                   &exponent, &literal_exponent, &direct_power) || !exponent)
        goto cleanup;
    endpoint_value = expr_eval(endpoint);
    symbolic_endpoint = (expr_is_variable(endpoint) || expr_is_named_const(endpoint)) && num_is_nan(endpoint_value);
    if (!symbolic_endpoint &&
        (!num_is_exact(endpoint_value) || !num_is_real(endpoint_value) || !num_is_finite(endpoint_value) ||
         !num_is_integer(endpoint_value) || !num_is_prime(endpoint_value)))
        goto cleanup;
    /* This scan is bounded by the number of explicitly supplied terms, not by the endpoint. */
    for (size_t i = 0u; i < ellipsis_index; ++i) {
        long prime_index = 0L;
        number_t next;

        if (!expr_series_number_to_long(prime, &prime_index) ||
            !expr_series_symbolic_power_term_matches(text, terms[i], prime_index, numerator, exponent, direct_power))
            goto cleanup;
        next = num_next_prime(prime);
        num_destroy(&prime);
        prime = next;
    }
    if (!symbolic_endpoint && num_lt(endpoint_value, prime))
        goto cleanup;

    index = expr_new_named_var(NUM_NAN, expr_series_display_index_name(text));
    predicate = index ? expr_is_prime(index) : NULL;
    power = index ? expr_pow_xp(index, exponent) : NULL;
    weighted = predicate ? expr_mul_simplify_owned(expr_clone(numerator), expr_clone(predicate)) : NULL;
    term = weighted && power ? (direct_power ? expr_mul(weighted, power) : expr_div(weighted, power)) : NULL;
    lower = expr_from_string("2", NULL);
    summation = term && index && lower ? expr_new_finite_summation_range(term, index, lower, endpoint) : NULL;
    summation_text = summation ? expr_to_string(summation, style_UNBOUND) : NULL;
    summation_TeX = summation ? expr_to_TeX_body(summation) : NULL;
    output = string_new();
    TeX = string_new();
    if (!summation_text || !summation_TeX || !output || !TeX ||
        string_append_cstr(output, summation_text) != 0 || string_append_cstr(TeX, summation_TeX) != 0)
        goto cleanup;
    matched = true;

cleanup:
    free(summation_text);
    free(summation_TeX);
    expr_free(summation);
    expr_free(lower);
    expr_free(term);
    expr_free(weighted);
    expr_free(power);
    expr_free(predicate);
    expr_free(index);
    expr_free(exponent);
    expr_free(endpoint);
    expr_free(numerator);
    num_destroy(&endpoint_value);
    num_destroy(&prime);
    free(terms);
    if (!matched) {
        string_free(output);
        string_free(TeX);
        return string_clone(source);
    }
    *expanded_out = true;
    *display_TeX_out = TeX;
    return output;
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

static bool expr_series_reciprocal_square_root(number_t argument_squared, long *root_out)
{
    number_t reciprocal_squared = num_div(NUM_ONE, argument_squared);
    long reciprocal_squared_long = 0L;
    long root = 0L;
    bool matched = false;

    *root_out = 0L;
    if (!expr_series_number_to_long(reciprocal_squared, &reciprocal_squared_long) || reciprocal_squared_long <= 0L)
        goto cleanup;
    root = (long)sqrtl((long double)reciprocal_squared_long);
    while (root > 0L && root > reciprocal_squared_long / root)
        --root;
    while (root < LONG_MAX && root + 1L <= reciprocal_squared_long / (root + 1L))
        ++root;
    if (root <= 0L || root != reciprocal_squared_long / root || reciprocal_squared_long % root != 0L)
        goto cleanup;
    *root_out = root;
    matched = true;

cleanup:
    num_destroy(&reciprocal_squared);
    return matched;
}

static bool expr_series_arctangent_endpoint_linear(const expr_t *linear, const expr_t *endpoint)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    number_t constant = NUM_NAN;
    bool subtract = false;
    long scale = 0L;
    bool matched = false;

    if (!expr_match_add_sub_expr(linear, &left, &right, &subtract) || subtract || !left || !right)
        return false;
    if (expr_match_const_value(right, &constant) && num_is_one(constant))
        matched = expr_series_match_positive_long_scale(left, endpoint, &scale) && scale == 2L;
    num_destroy(&constant);
    constant = NUM_NAN;
    if (!matched && expr_match_const_value(left, &constant) && num_is_one(constant))
        matched = expr_series_match_positive_long_scale(right, endpoint, &scale) && scale == 2L;
    num_destroy(&constant);
    return matched;
}

static bool expr_series_arctangent_endpoint_power(const expr_t *power, const expr_t *endpoint, long root)
{
    const expr_t *base = NULL;
    const expr_t *exponent = NULL;
    number_t base_value = NUM_NAN;
    number_t root_value = num_create_from_long(root);
    long scale = 0L;
    bool matched;

    if (!expr_match_pow_expr(power, &base, &exponent) || !base || !exponent ||
        !expr_match_const_value(base, &base_value)) {
        num_destroy(&root_value);
        num_destroy(&base_value);
        return false;
    }
    matched = num_eq(base_value, root_value) &&
              expr_series_match_positive_long_scale(exponent, endpoint, &scale) && scale == 2L;
    num_destroy(&root_value);
    num_destroy(&base_value);
    return matched;
}

static bool expr_series_arctangent_symbolic_endpoint(const char *text, expr_series_span_t span,
                                                      number_t argument_squared, expr_t **endpoint_out)
{
    char *term_text = NULL;
    expr_t *expression = NULL;
    const expr_t *numerator = NULL;
    const expr_t *denominator = NULL;
    const expr_t *minus_one = NULL;
    const expr_t *endpoint = NULL;
    const expr_t *left_factor = NULL;
    const expr_t *right_factor = NULL;
    number_t minus_one_value = NUM_NAN;
    number_t endpoint_value = NUM_NAN;
    long root = 0L;
    bool matched = false;

    *endpoint_out = NULL;
    if (!expr_series_reciprocal_square_root(argument_squared, &root))
        return false;
    term_text = expr_series_copy_span(text, expr_series_trim(text, span));
    expression = term_text ? expr_from_string(term_text, NULL) : NULL;
    free(term_text);
    if (!expression || !expr_match_div_expr(expression, &numerator, &denominator) || !numerator || !denominator ||
        !expr_match_pow_expr(numerator, &minus_one, &endpoint) || !minus_one || !endpoint ||
        !expr_match_const_value(minus_one, &minus_one_value) || !num_eq(minus_one_value, NUM_NEG_ONE) ||
        !expr_match_mul_expr(denominator, &left_factor, &right_factor) || !left_factor || !right_factor)
        goto cleanup;
    endpoint_value = expr_eval(endpoint);
    if ((!expr_is_variable(endpoint) && !expr_is_named_const(endpoint)) || !num_is_nan(endpoint_value))
        goto cleanup;
    if (!((expr_series_arctangent_endpoint_linear(left_factor, endpoint) &&
           expr_series_arctangent_endpoint_power(right_factor, endpoint, root)) ||
          (expr_series_arctangent_endpoint_linear(right_factor, endpoint) &&
           expr_series_arctangent_endpoint_power(left_factor, endpoint, root))))
        goto cleanup;
    *endpoint_out = expr_clone(endpoint);
    matched = *endpoint_out != NULL;

cleanup:
    num_destroy(&endpoint_value);
    num_destroy(&minus_one_value);
    expr_free(expression);
    return matched;
}

static bool expr_series_arctangent_argument_squared(const char *text, number_t *argument_squared_out,
                                                     expr_t **endpoint_out)
{
    expr_series_span_t *terms = NULL;
    size_t term_count = 0u;
    number_t values[3] = {NUM_NAN, NUM_NAN, NUM_NAN};
    char *factors[3] = {NULL, NULL, NULL};
    number_t negative_second = NUM_NAN;
    number_t argument_squared = NUM_NAN;
    number_t argument_fourth = NUM_NAN;
    number_t five = NUM_NAN;
    number_t expected_third = NUM_NAN;
    bool matched = false;

    *argument_squared_out = NUM_NAN;
    *endpoint_out = NULL;
    if (!expr_series_split_terms(text, &terms, &term_count) ||
        !((term_count == 4u && expr_series_is_ellipsis(text, terms[3])) ||
          (term_count == 5u && expr_series_is_ellipsis(text, terms[3]))))
        goto cleanup;
    for (size_t i = 0u; i < 3u; ++i) {
        if (!expr_series_parse_term(text, terms[i], &values[i], &factors[i]) || !factors[i] || *factors[i])
            goto cleanup;
    }
    if (!num_is_one(values[0]) || num_get_sign(values[1]) >= 0 || num_get_sign(values[2]) <= 0)
        goto cleanup;

    negative_second = num_neg(values[1]);
    argument_squared = num_mul_long(negative_second, 3L);
    argument_fourth = num_mul(argument_squared, argument_squared);
    five = num_create_from_long(5L);
    expected_third = num_div(argument_fourth, five);
    if (!num_is_exact(argument_squared) || !num_is_finite(argument_squared) ||
        num_get_sign(argument_squared) <= 0 ||
        !num_eq(values[2], expected_third))
        goto cleanup;

    if (term_count == 5u &&
        !expr_series_arctangent_symbolic_endpoint(text, terms[4], argument_squared, endpoint_out))
        goto cleanup;
    *argument_squared_out = num_clone(argument_squared);
    matched = true;

cleanup:
    num_destroy(&expected_third);
    num_destroy(&five);
    num_destroy(&argument_fourth);
    num_destroy(&argument_squared);
    num_destroy(&negative_second);
    for (size_t i = 0u; i < 3u; ++i) {
        free(factors[i]);
        num_destroy(&values[i]);
    }
    free(terms);
    return matched;
}

static bool expr_series_endpoint_is_positive_infinity(const expr_t *endpoint,
                                                      expr_series_binding_lookup_fn lookup_binding,
                                                      void *lookup_context)
{
    number_t endpoint_value = endpoint ? expr_eval(endpoint) : NUM_NAN;
    bool positive_infinity =
        num_is_real(endpoint_value) && num_is_inf(endpoint_value) && num_get_sign(endpoint_value) > 0;

    if (!positive_infinity && endpoint && lookup_binding) {
        char *endpoint_name = expr_to_string(endpoint, style_UNBOUND);

        num_destroy(&endpoint_value);
        endpoint_value = NUM_NAN;
        if (endpoint_name && lookup_binding(lookup_context, endpoint_name, &endpoint_value))
            positive_infinity =
                num_is_real(endpoint_value) && num_is_inf(endpoint_value) && num_get_sign(endpoint_value) > 0;
        free(endpoint_name);
    }
    num_destroy(&endpoint_value);
    return positive_infinity;
}

static string_t *expr_series_arctangent_formula(number_t argument_squared, const expr_t *endpoint,
                                                bool endpoint_positive_infinity)
{
    char *endpoint_text = endpoint ? expr_to_string(endpoint, style_UNBOUND) : NULL;
    long root = 0L;
    string_t *formula = NULL;

    if (expr_series_reciprocal_square_root(argument_squared, &root)) {
        formula = string_new();
        if (!formula ||
            (endpoint_text && !endpoint_positive_infinity &&
             string_append_format(formula,
                                  "%ld*atan(1/%ld)+(-1)^(%s)/(%ld^(2*(%s)+2)*(2*(%s)+3))*"
                                  "pFq(2,1,1,(%s)+3/2,(%s)+5/2,-1/%ld^2)",
                                  root, root, endpoint_text, root, endpoint_text, endpoint_text,
                                  endpoint_text, endpoint_text, root) < 0) ||
            ((!endpoint_text || endpoint_positive_infinity) &&
             string_append_format(formula, "%ld*atan(1/%ld)", root, root) < 0)) {
            string_free(formula);
            formula = NULL;
        }
    }
    if (!formula && !endpoint) {
        string_t *argument_squared_text = num_to_string(argument_squared);

        formula = string_new();
        if (!argument_squared_text || !formula ||
            string_append_format(formula, "atan(sqrt(%S))/sqrt(%S)", argument_squared_text,
                                 argument_squared_text) < 0) {
            string_free(formula);
            formula = NULL;
        }
        string_free(argument_squared_text);
    }
    free(endpoint_text);
    return formula;
}

static string_t *expr_series_expand_nested_arctangent_once(const string_t *source, bool *expanded_out,
                                                           expr_series_binding_lookup_fn lookup_binding,
                                                           void *lookup_context)
{
    const char *text = string_c_str(source);
    const size_t length = strlen(text);
    size_t *open_parentheses = calloc(length + 1u, sizeof(*open_parentheses));
    size_t depth = 0u;
    string_t *output = NULL;

    *expanded_out = false;
    if (!open_parentheses)
        return NULL;
    for (size_t i = 0u; i < length; ++i) {
        number_t argument_squared = NUM_NAN;
        expr_t *endpoint = NULL;
        char *inner = NULL;

        if (text[i] == '(') {
            open_parentheses[depth++] = i;
            continue;
        }
        if (text[i] != ')' || depth == 0u)
            continue;
        const size_t open = open_parentheses[--depth];
        expr_series_span_t inner_span = {open + 1u, i};

        inner = expr_series_copy_span(text, inner_span);
        if (!inner)
            goto cleanup;
        if (!expr_series_arctangent_argument_squared(inner, &argument_squared, &endpoint)) {
            free(inner);
            continue;
        }
        free(inner);
        const bool endpoint_positive_infinity =
            expr_series_endpoint_is_positive_infinity(endpoint, lookup_binding, lookup_context);
        string_t *formula =
            expr_series_arctangent_formula(argument_squared, endpoint, endpoint_positive_infinity);

        output = string_new();
        if (!formula || !output || expr_series_append_slice(output, text, 0u, open + 1u) != 0 ||
            string_append_format(output, "%S", formula) < 0 ||
            expr_series_append_slice(output, text, i, length) != 0) {
            string_free(formula);
            expr_free(endpoint);
            num_destroy(&argument_squared);
            string_free(output);
            output = NULL;
            goto cleanup;
        }
        string_free(formula);
        expr_free(endpoint);
        num_destroy(&argument_squared);
        *expanded_out = true;
        goto cleanup;
    }
    output = string_clone(source);

cleanup:
    free(open_parentheses);
    return output;
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

static string_t *expr_series_complete_open_inverse_index_power(const string_t *side, bool *expanded_out)
{
    const char *text = string_c_str(side);
    expr_series_span_t *terms = NULL;
    size_t term_count = 0u;
    size_t ellipsis_index = SIZE_MAX;
    size_t coefficient_count = 0u;
    number_t *coefficients = NULL;
    char *factor = NULL;
    string_t *first_text = NULL;
    string_t *output = NULL;
    int exponent = 0;

    *expanded_out = false;
    if (!expr_series_split_terms(text, &terms, &term_count))
        return NULL;
    for (size_t i = 0u; i < term_count; ++i) {
        if (expr_series_is_ellipsis(text, terms[i])) {
            ellipsis_index = i;
            break;
        }
    }
    if (ellipsis_index == SIZE_MAX || ellipsis_index + 1u != term_count || ellipsis_index < 3u)
        goto unchanged;

    coefficients = calloc(ellipsis_index, sizeof(*coefficients));
    if (!coefficients)
        goto allocation_failure;
    for (size_t i = 0u; i < ellipsis_index; ++i) {
        char *term_factor = NULL;

        if (!expr_series_parse_term(text, terms[i], &coefficients[coefficient_count], &term_factor)) {
            free(term_factor);
            goto unchanged;
        }
        ++coefficient_count;
        if (factor && strcmp(factor, term_factor) != 0) {
            free(term_factor);
            goto unchanged;
        }
        if (!factor) {
            factor = term_factor;
            term_factor = NULL;
        }
        free(term_factor);
    }
    if (!expr_series_inverse_index_power_exponent(coefficients, coefficient_count, &exponent))
        goto unchanged;

    first_text = num_to_string(coefficients[0]);
    output = string_clone(side);
    if (!first_text || !output)
        goto allocation_failure;
    if (factor && *factor) {
        if (string_append_format(output, " + (%S)*(%s)/inf^%d", first_text, factor, exponent) < 0)
            goto allocation_failure;
    } else if (string_append_format(output, " + (%S)/inf^%d", first_text, exponent) < 0) {
        goto allocation_failure;
    }
    *expanded_out = true;
    goto cleanup;

unchanged:
    output = string_clone(side);
    goto cleanup;

allocation_failure:
    string_free(output);
    output = NULL;

cleanup:
    string_free(first_text);
    free(factor);
    expr_series_destroy_numbers(coefficients, coefficient_count);
    free(terms);
    return output;
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

static number_t expr_series_alternating_arithmetic_reciprocal_term(number_t first, long step, size_t index)
{
    number_t denominator = num_create_from_long(1L + step * (long)index);
    number_t term = num_div(first, denominator);

    num_destroy(&denominator);
    if ((index & 1u) != 0u) {
        number_t negative = num_neg(term);

        num_destroy(&term);
        term = negative;
    }
    return term;
}

static bool expr_series_alternating_arithmetic_reciprocal_prefix(const number_t *coefficients,
                                                                 size_t coefficient_count, long *step_out)
{
    number_t first_magnitude = NUM_NAN;
    number_t second_magnitude = NUM_NAN;
    number_t denominator = NUM_NAN;
    long second_denominator = 0L;
    long step = 0L;
    bool matched = false;

    *step_out = 0L;
    if (coefficient_count < 3u || num_is_zero(coefficients[0]) ||
        num_sign(coefficients[1]) == num_sign(coefficients[0]))
        return false;
    first_magnitude = num_abs(coefficients[0]);
    second_magnitude = num_abs(coefficients[1]);
    denominator = num_div(first_magnitude, second_magnitude);
    if (!num_is_exact(denominator) || !num_is_integer(denominator) ||
        !expr_series_number_to_long(denominator, &second_denominator) || second_denominator <= 1L)
        goto cleanup;
    step = second_denominator - 1L;
    if (step > (LONG_MAX - 1L) / (long)(coefficient_count - 1u))
        goto cleanup;

    for (size_t index = 0u; index < coefficient_count; ++index) {
        number_t expected = expr_series_alternating_arithmetic_reciprocal_term(coefficients[0], step, index);
        bool matches = num_is_finite(expected) && num_eq(expected, coefficients[index]);

        num_destroy(&expected);
        if (!matches)
            goto cleanup;
    }
    *step_out = step;
    matched = true;

cleanup:
    num_destroy(&denominator);
    num_destroy(&second_magnitude);
    num_destroy(&first_magnitude);
    return matched;
}

static bool expr_series_alternating_arithmetic_reciprocal_endpoint(number_t first, number_t last, long step,
                                                                   size_t minimum, number_t *endpoint_out)
{
    number_t first_magnitude = num_abs(first);
    number_t last_magnitude = num_abs(last);
    number_t denominator = num_div(first_magnitude, last_magnitude);
    number_t denominator_minus_one = NUM_NAN;
    number_t step_value = num_create_from_long(step);
    number_t endpoint = NUM_NAN;
    number_t division_remainder = NUM_NAN;
    number_t minimum_value = num_create_from_long((long)minimum);
    number_t remainder = NUM_NAN;
    number_t scaled_endpoint = NUM_NAN;
    number_t expected_denominator = NUM_NAN;
    number_t expected = NUM_NAN;
    bool matched = false;

    *endpoint_out = NUM_NAN;
    if (num_is_zero(last) || !num_is_exact(denominator) || !num_is_integer(denominator) ||
        !num_gt(denominator, NUM_ZERO))
        goto cleanup;
    denominator_minus_one = num_sub(denominator, NUM_ONE);
    if (num_divmod(denominator_minus_one, step_value, &endpoint, &division_remainder) != 0 ||
        !num_is_zero(division_remainder) || !num_is_exact(endpoint) || !num_is_integer(endpoint) ||
        !num_ge(endpoint, minimum_value))
        goto cleanup;
    scaled_endpoint = num_mul(endpoint, step_value);
    expected_denominator = num_add(scaled_endpoint, NUM_ONE);
    expected = num_div(first, expected_denominator);
    remainder = num_mod(endpoint, NUM_TWO);
    if (num_is_one(remainder)) {
        number_t negative = num_neg(expected);

        num_destroy(&expected);
        expected = negative;
    }
    if (!num_is_finite(expected) || !num_eq(expected, last))
        goto cleanup;
    *endpoint_out = endpoint;
    endpoint = NUM_NAN;
    matched = true;

cleanup:
    num_destroy(&expected_denominator);
    num_destroy(&scaled_endpoint);
    num_destroy(&remainder);
    num_destroy(&minimum_value);
    num_destroy(&division_remainder);
    num_destroy(&endpoint);
    num_destroy(&step_value);
    num_destroy(&denominator_minus_one);
    num_destroy(&expected);
    num_destroy(&denominator);
    num_destroy(&last_magnitude);
    num_destroy(&first_magnitude);
    return matched;
}

static bool expr_series_alternating_arithmetic_reciprocal_fraction_endpoint(
    number_t first, number_t numerator, number_t denominator, long step, size_t minimum, number_t *endpoint_out)
{
    number_t first_magnitude = num_abs(first);
    number_t numerator_magnitude = num_abs(numerator);
    number_t denominator_minus_one = NUM_NAN;
    number_t step_value = num_create_from_long(step);
    number_t endpoint = NUM_NAN;
    number_t division_remainder = NUM_NAN;
    number_t minimum_value = num_create_from_long((long)minimum);
    number_t remainder = NUM_NAN;
    bool signs_match;
    bool matched = false;

    *endpoint_out = NUM_NAN;
    if (!num_is_exact(numerator) || !num_is_integer(numerator) || !num_is_exact(denominator) ||
        !num_is_integer(denominator) || !num_gt(denominator, NUM_ZERO) ||
        !num_eq(first_magnitude, numerator_magnitude))
        goto cleanup;
    denominator_minus_one = num_sub(denominator, NUM_ONE);
    if (num_divmod(denominator_minus_one, step_value, &endpoint, &division_remainder) != 0 ||
        !num_is_zero(division_remainder) || !num_is_exact(endpoint) || !num_is_integer(endpoint) ||
        !num_ge(endpoint, minimum_value))
        goto cleanup;
    remainder = num_mod(endpoint, NUM_TWO);
    signs_match = num_sign(first) == num_sign(numerator);
    if ((num_is_zero(remainder) && !signs_match) || (num_is_one(remainder) && signs_match))
        goto cleanup;

    *endpoint_out = endpoint;
    endpoint = NUM_NAN;
    matched = true;

cleanup:
    num_destroy(&remainder);
    num_destroy(&minimum_value);
    num_destroy(&division_remainder);
    num_destroy(&endpoint);
    num_destroy(&step_value);
    num_destroy(&denominator_minus_one);
    num_destroy(&numerator_magnitude);
    num_destroy(&first_magnitude);
    return matched;
}

static number_t expr_series_alternating_arithmetic_reciprocal_sum_approx(number_t first, long step,
                                                                         number_t endpoint)
{
    number_t step_value = num_create_from_long(step);
    number_t reciprocal_step = num_div(NUM_ONE, step_value);
    number_t first_digamma_arg_numerator = num_add(reciprocal_step, NUM_ONE);
    number_t first_digamma_arg = num_div(first_digamma_arg_numerator, NUM_TWO);
    number_t second_digamma_arg = num_div(reciprocal_step, NUM_TWO);
    number_t first_digamma = num_digamma(first_digamma_arg);
    number_t second_digamma = num_digamma(second_digamma_arg);
    number_t infinite_difference = num_sub(first_digamma, second_digamma);
    number_t endpoint_plus_reciprocal = num_add(endpoint, reciprocal_step);
    number_t tail_first_numerator = num_add(endpoint_plus_reciprocal, NUM_TWO);
    number_t tail_second_numerator = num_add(endpoint_plus_reciprocal, NUM_ONE);
    number_t tail_first_arg = num_div(tail_first_numerator, NUM_TWO);
    number_t tail_second_arg = num_div(tail_second_numerator, NUM_TWO);
    number_t tail_first = num_digamma(tail_first_arg);
    number_t tail_second = num_digamma(tail_second_arg);
    number_t tail_difference = num_sub(tail_first, tail_second);
    number_t endpoint_remainder = num_mod(endpoint, NUM_TWO);
    number_t signed_tail = num_is_one(endpoint_remainder) ? num_neg(tail_difference) : num_clone(tail_difference);
    number_t bracket = num_add(infinite_difference, signed_tail);
    number_t twice_step = num_mul(NUM_TWO, step_value);
    number_t scale = num_div(first, twice_step);
    number_t sum = num_mul(scale, bracket);

    num_destroy(&scale);
    num_destroy(&twice_step);
    num_destroy(&bracket);
    num_destroy(&signed_tail);
    num_destroy(&endpoint_remainder);
    num_destroy(&tail_difference);
    num_destroy(&tail_second);
    num_destroy(&tail_first);
    num_destroy(&tail_second_arg);
    num_destroy(&tail_first_arg);
    num_destroy(&tail_second_numerator);
    num_destroy(&tail_first_numerator);
    num_destroy(&endpoint_plus_reciprocal);
    num_destroy(&infinite_difference);
    num_destroy(&second_digamma);
    num_destroy(&first_digamma);
    num_destroy(&second_digamma_arg);
    num_destroy(&first_digamma_arg);
    num_destroy(&first_digamma_arg_numerator);
    num_destroy(&reciprocal_step);
    num_destroy(&step_value);
    return sum;
}

static bool expr_series_sum_alternating_arithmetic_reciprocal(const number_t *coefficients,
                                                              size_t coefficient_count, number_t last,
                                                              number_t *sum_out, bool *recognised_out,
                                                              size_t *endpoint_out, number_t *exact_endpoint_out,
                                                              long *step_out)
{
    number_t sum = NUM_NAN;
    number_t exact_endpoint = NUM_NAN;
    number_t exact_sum_limit = NUM_NAN;
    long step = 0L;
    size_t endpoint = 0u;
    long endpoint_long = 0L;
    bool found = false;

    *sum_out = NUM_NAN;
    *recognised_out = false;
    *endpoint_out = 0u;
    *exact_endpoint_out = NUM_NAN;
    *step_out = 0L;
    if (!expr_series_alternating_arithmetic_reciprocal_prefix(coefficients, coefficient_count, &step))
        return false;
    *recognised_out = true;
    if (!expr_series_alternating_arithmetic_reciprocal_endpoint(coefficients[0], last, step, coefficient_count,
                                                                &exact_endpoint))
        return false;
    exact_sum_limit = num_create_from_long(1000000L);
    if (!num_is_finite(exact_sum_limit))
        goto cleanup;
    if (num_gt(exact_endpoint, exact_sum_limit)) {
        sum = expr_series_alternating_arithmetic_reciprocal_sum_approx(coefficients[0], step, exact_endpoint);
        if (!num_is_finite(sum))
            goto cleanup;
        *sum_out = sum;
        *exact_endpoint_out = exact_endpoint;
        *step_out = step;
        sum = NUM_NAN;
        exact_endpoint = NUM_NAN;
        num_destroy(&exact_sum_limit);
        return true;
    }
    if (!expr_series_number_to_long(exact_endpoint, &endpoint_long) || endpoint_long < 0L)
        goto cleanup;
    endpoint = (size_t)endpoint_long;
    if (!expr_series_initial_sum(coefficients, coefficient_count, &sum))
        goto cleanup;

    for (size_t index = coefficient_count; index <= endpoint; ++index) {
        number_t term;
        number_t next_sum;

        if (step > (LONG_MAX - 1L) / (long)index)
            break;
        term = expr_series_alternating_arithmetic_reciprocal_term(coefficients[0], step, index);
        next_sum = num_add(sum, term);
        num_destroy(&sum);
        sum = next_sum;
        if (!num_is_finite(term) || !num_is_finite(sum)) {
            num_destroy(&term);
            goto cleanup;
        }
        if (index == endpoint && num_eq(term, last)) {
            *sum_out = sum;
            *endpoint_out = index;
            *exact_endpoint_out = exact_endpoint;
            *step_out = step;
            sum = NUM_NAN;
            exact_endpoint = NUM_NAN;
            found = true;
            num_destroy(&term);
            break;
        }
        num_destroy(&term);
    }

cleanup:
    num_destroy(&exact_sum_limit);
    num_destroy(&exact_endpoint);
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
    bool alternating_arithmetic_reciprocal = false;
    bool geometric = false;
    bool index_power = false;
    bool inverse_index_power = false;
    size_t endpoint = 0u;
    int exponent = 0;
    long alternating_reciprocal_step = 0L;
    number_t ratio = NUM_NAN;

    if (expr_series_sum_alternating_arithmetic_reciprocal(
            coefficients, coefficient_count, last, &result->sum, &alternating_arithmetic_reciprocal,
            &endpoint, &result->exact_endpoint, &alternating_reciprocal_step)) {
        result->inverse_index_first = num_clone(coefficients[0]);
        result->alternating_reciprocal_step = alternating_reciprocal_step;
        result->endpoint = endpoint;
        result->kind = EXPR_SERIES_MODEL_ALTERNATING_ARITHMETIC_RECIPROCAL;
        return true;
    }
    if (alternating_arithmetic_reciprocal)
        return false;
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

static expr_t *expr_series_alternating_arithmetic_reciprocal_expr(const expr_series_result_t *result,
                                                                  const expr_t *index)
{
    number_t step_value = num_create_from_long(result->alternating_reciprocal_step);
    expr_t *minus_one = expr_new_const(NUM_NEG_ONE);
    expr_t *alternating_sign = minus_one ? expr_pow_xp(minus_one, index) : NULL;
    expr_t *step = expr_new_const(step_value);
    expr_t *scaled_index = step ? expr_mul(step, index) : NULL;
    expr_t *denominator = scaled_index ? expr_add(scaled_index, EXPR_ONE) : NULL;
    expr_t *first = expr_new_const(result->inverse_index_first);
    expr_t *numerator = first && alternating_sign ? expr_mul(first, alternating_sign) : NULL;
    expr_t *term = numerator && denominator ? expr_div(numerator, denominator) : NULL;

    expr_free(numerator);
    expr_free(first);
    expr_free(denominator);
    expr_free(scaled_index);
    expr_free(step);
    expr_free(alternating_sign);
    expr_free(minus_one);
    num_destroy(&step_value);
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
    const bool zero_based = zero_based_geometric ||
                            result->kind == EXPR_SERIES_MODEL_ALTERNATING_ARITHMETIC_RECIPROCAL;
    expr_t *lower = expr_new_const(zero_based ? NUM_ZERO : NUM_ONE);
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
    } else if (num_is_exact(result->exact_endpoint)) {
        upper = expr_new_const(result->exact_endpoint);
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
    else if (result->kind == EXPR_SERIES_MODEL_ALTERNATING_ARITHMETIC_RECIPROCAL)
        model = expr_series_alternating_arithmetic_reciprocal_expr(result, index);
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

static int expr_series_append_symbolic_alternating_reciprocal_formula(string_t *output,
                                                                      const expr_series_result_t *result,
                                                                      bool endpoint_positive_infinity,
                                                                      bool *domain_specialised_out)
{
    string_t *first_text = num_to_string(result->inverse_index_first);
    char *endpoint_text = expr_to_string(result->symbolic_endpoint, style_UNBOUND);
    number_t four = num_create_from_long(4L);
    const bool is_leibniz_pi_series = result->alternating_reciprocal_step == 2L &&
                                      num_eq(result->inverse_index_first, four);
    int status = -1;

    if (!first_text || !endpoint_text)
        goto cleanup;
    if (endpoint_positive_infinity && is_leibniz_pi_series) {
        if (string_append_cstr(output, "pi") == 0) {
            if (domain_specialised_out)
                *domain_specialised_out = true;
            status = 0;
        }
    } else if (is_leibniz_pi_series &&
               string_append_format(output,
                                    "pi+(-1)^(%s)*(digamma((%s)/2+5/4)-digamma((%s)/2+3/4))",
                                    endpoint_text, endpoint_text, endpoint_text) >= 0) {
        status = 0;
    } else if (endpoint_positive_infinity &&
               string_append_format(output,
                                    "(%S)/(2*(%ld))*(digamma(((1/(%ld))+1)/2)-digamma(1/(2*(%ld))))",
                                    first_text, result->alternating_reciprocal_step,
                                    result->alternating_reciprocal_step,
                                    result->alternating_reciprocal_step) >= 0) {
        if (domain_specialised_out)
            *domain_specialised_out = true;
        status = 0;
    } else if (string_append_format(
                   output,
                   "(%S)/(2*(%ld))*(digamma(((1/(%ld))+1)/2)-digamma(1/(2*(%ld)))+"
                   "(-1)^(%s)*(digamma(((%s)+(1/(%ld))+2)/2)-digamma(((%s)+(1/(%ld))+1)/2)))",
                   first_text, result->alternating_reciprocal_step, result->alternating_reciprocal_step,
                   result->alternating_reciprocal_step, endpoint_text, endpoint_text,
                   result->alternating_reciprocal_step, endpoint_text,
                   result->alternating_reciprocal_step) >= 0) {
        status = 0;
    }

cleanup:
    num_destroy(&four);
    free(endpoint_text);
    string_free(first_text);
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
    number_t symbolic_alternating_first = NUM_NAN;
    number_t numeric_terminal_numerator = NUM_NAN;
    number_t numeric_terminal_denominator = NUM_NAN;
    int symbolic_exponent = 0;
    long symbolic_alternating_step = 0L;
    bool symbolic_endpoint = false;
    bool symbolic_alternating = false;
    bool numeric_fraction_endpoint = false;
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
        symbolic_alternating = expr_series_parse_symbolic_alternating_reciprocal_endpoint(
            text, terms[ellipsis_index + 1u], &symbolic_alternating_first, &symbolic_alternating_step,
            &result.symbolic_endpoint);
        symbolic_endpoint = symbolic_alternating;
    }
    if (!symbolic_geometric && !symbolic_alternating) {
        symbolic_endpoint = expr_series_parse_symbolic_power_endpoint(
            text, terms[ellipsis_index + 1u], &symbolic_numerator, &result.symbolic_endpoint,
            &result.symbolic_exponent, &symbolic_exponent, &result.symbolic_direct_power);
    }
    if (!symbolic_endpoint) {
        numeric_fraction_endpoint = expr_series_parse_numeric_fraction(
            text, terms[ellipsis_index + 1u], &numeric_terminal_numerator, &numeric_terminal_denominator);
        if (expr_series_parse_term(text, terms[ellipsis_index + 1u], &last, &terminal_factor))
            numeric_fraction_endpoint = false;
        else if (!numeric_fraction_endpoint)
            goto unsupported;
    }
    if (symbolic_endpoint && result.symbolic_exponent) {
        if (!expr_series_symbolic_power_prefix_matches(text, terms, ellipsis_index, symbolic_numerator,
                                                       result.symbolic_exponent, result.symbolic_direct_power,
                                                       &series_start))
            goto unsupported;
        /* A literal endpoint must lie beyond the explicitly supplied consecutive prefix. */
        number_t endpoint_value = expr_eval(result.symbolic_endpoint);
        number_t prefix_length = num_create_from_long((long)(ellipsis_index - series_start));
        bool endpoint_overlaps_prefix = num_is_finite(endpoint_value) && num_le(endpoint_value, prefix_length);

        num_destroy(&prefix_length);
        num_destroy(&endpoint_value);
        if (endpoint_overlaps_prefix)
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
    } else if (symbolic_alternating) {
        long inferred_step = 0L;

        if (!expr_series_alternating_arithmetic_reciprocal_prefix(coefficients, coefficient_count, &inferred_step) ||
            inferred_step != symbolic_alternating_step || !num_eq(coefficients[0], symbolic_alternating_first))
            goto unsupported;
        result.inverse_index_first = num_clone(coefficients[0]);
        result.alternating_reciprocal_step = inferred_step;
        result.kind = EXPR_SERIES_MODEL_ALTERNATING_ARITHMETIC_RECIPROCAL;
    } else if (symbolic_endpoint) {
        int inferred_exponent = 0;

        if (!expr_series_inverse_index_power_exponent(coefficients, coefficient_count, &inferred_exponent) ||
            inferred_exponent != symbolic_exponent ||
            !expr_series_symbolic_numerator_matches(coefficients[0], last_factor, symbolic_numerator))
            goto unsupported;
        result.inverse_index_first = num_clone(coefficients[0]);
        result.inverse_index_exponent = inferred_exponent;
        result.kind = EXPR_SERIES_MODEL_INVERSE_INDEX_POWER;
    } else if (numeric_fraction_endpoint) {
        number_t exact_endpoint = NUM_NAN;
        number_t exact_sum_limit = num_create_from_long(1000000L);
        long inferred_step = 0L;
        bool prefix_matches =
            expr_series_alternating_arithmetic_reciprocal_prefix(coefficients, coefficient_count, &inferred_step);
        bool endpoint_matches =
            prefix_matches && expr_series_alternating_arithmetic_reciprocal_fraction_endpoint(
                                  coefficients[0], numeric_terminal_numerator, numeric_terminal_denominator,
                                  inferred_step, coefficient_count, &exact_endpoint);

        if (*last_factor || !prefix_matches || !endpoint_matches) {
            num_destroy(&exact_sum_limit);
            num_destroy(&exact_endpoint);
            goto unsupported;
        }
        if (!num_gt(exact_endpoint, exact_sum_limit)) {
            last = num_div(numeric_terminal_numerator, numeric_terminal_denominator);
            num_destroy(&exact_endpoint);
            num_destroy(&exact_sum_limit);
            if (!num_is_exact(last) || !expr_series_sum_coefficients(coefficients, coefficient_count, last, &result))
                goto unsupported;
        } else {
            result.sum = expr_series_alternating_arithmetic_reciprocal_sum_approx(coefficients[0], inferred_step,
                                                                                  exact_endpoint);
            if (!num_is_finite(result.sum)) {
                num_destroy(&exact_sum_limit);
                num_destroy(&exact_endpoint);
                goto unsupported;
            }
            result.inverse_index_first = num_clone(coefficients[0]);
            result.alternating_reciprocal_step = inferred_step;
            result.exact_endpoint = exact_endpoint;
            result.kind = EXPR_SERIES_MODEL_ALTERNATING_ARITHMETIC_RECIPROCAL;
            num_destroy(&exact_sum_limit);
        }
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
    } else if (result.kind == EXPR_SERIES_MODEL_ALTERNATING_ARITHMETIC_RECIPROCAL &&
               result.symbolic_endpoint) {
        if (expr_series_append_symbolic_alternating_reciprocal_formula(
                output, &result, endpoint_positive_infinity, domain_specialised_out) != 0)
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
    fprintf(stderr,
            "expr_from_string: ellipsis must abbreviate an exact polynomial, index-power, inverse-index-power, "
            "alternating reciprocal, or geometric sequence of like additive terms\n");
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
    num_destroy(&numeric_terminal_denominator);
    num_destroy(&numeric_terminal_numerator);
    num_destroy(&symbolic_alternating_first);
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
                                  bool *domain_specialised_out, bool *power_series_out)
{
    string_t *current = string_from_view(&source);
    string_t *display_TeX = NULL;

    if (display_TeX_out)
        *display_TeX_out = NULL;
    if (domain_specialised_out)
        *domain_specialised_out = false;
    if (power_series_out)
        *power_series_out = false;

    if (!current)
        return NULL;

    for (size_t pass = 0u; pass < 32u; ++pass) {
        string_t *next;
        string_t *next_display_TeX = NULL;
        bool expanded = false;

        bool pass_domain_specialised = false;

        next = expr_series_expand_infinite_prime_product(current, &expanded, &next_display_TeX);
        if (!next) {
            string_free(display_TeX);
            string_free(current);
            return NULL;
        }
        if (expanded) {
            string_free(current);
            current = next;
            string_free(display_TeX);
            display_TeX = next_display_TeX;
            continue;
        }
        string_free(next);

        next = expr_series_expand_telescoping_product(current, &expanded, &next_display_TeX);
        if (!next) {
            string_free(display_TeX);
            string_free(current);
            return NULL;
        }
        if (expanded) {
            string_free(current);
            current = next;
            string_free(display_TeX);
            display_TeX = next_display_TeX;
            continue;
        }
        string_free(next);

        next = expr_series_expand_nested_arctangent_once(current, &expanded, lookup_binding, lookup_context);
        if (!next) {
            string_free(display_TeX);
            string_free(current);
            return NULL;
        }
        if (expanded) {
            string_free(current);
            current = next;
            continue;
        }
        string_free(next);

        next = expr_series_expand_weighted_trigonometric_progression(current, &expanded, &next_display_TeX);
        if (!next) {
            string_free(display_TeX);
            string_free(current);
            return NULL;
        }
        if (expanded) {
            string_free(current);
            current = next;
            string_free(display_TeX);
            display_TeX = next_display_TeX;
            continue;
        }
        string_free(next);

        next = expr_series_expand_trigonometric_progression(current, &expanded, &next_display_TeX);
        if (!next) {
            string_free(display_TeX);
            string_free(current);
            return NULL;
        }
        if (expanded) {
            string_free(current);
            current = next;
            string_free(display_TeX);
            display_TeX = next_display_TeX;
            continue;
        }
        string_free(next);

        next = expr_series_expand_unary_function_progression(current, &expanded, &next_display_TeX);
        if (!next) {
            string_free(display_TeX);
            string_free(current);
            return NULL;
        }
        if (expanded) {
            string_free(current);
            current = next;
            string_free(display_TeX);
            display_TeX = next_display_TeX;
            continue;
        }
        string_free(next);

        next = expr_series_expand_prime_power_sum(current, &expanded, &next_display_TeX);
        if (!next) {
            string_free(display_TeX);
            string_free(current);
            return NULL;
        }
        if (expanded) {
            string_free(current);
            current = next;
            string_free(display_TeX);
            display_TeX = next_display_TeX;
            continue;
        }
        string_free(next);

        next = expr_series_expand_open_symbolic_power(current, &expanded, &next_display_TeX);
        if (expanded && power_series_out)
            *power_series_out = true;
        if (!next) {
            string_free(display_TeX);
            string_free(current);
            return NULL;
        }
        if (expanded) {
            string_free(current);
            current = next;
            string_free(display_TeX);
            display_TeX = next_display_TeX;
            continue;
        }
        string_free(next);

        next = expr_series_complete_open_inverse_index_power(current, &expanded);
        if (!next) {
            string_free(display_TeX);
            string_free(current);
            return NULL;
        }
        if (expanded) {
            string_free(current);
            current = next;
            continue;
        }
        string_free(next);

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
