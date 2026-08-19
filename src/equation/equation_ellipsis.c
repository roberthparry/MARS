#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "equation.h"
#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"
#include "ustring.h"

typedef struct {
    size_t start;
    size_t end;
} equ_series_span_t;

static equ_series_span_t equ_series_trim(const char *text, equ_series_span_t span)
{
    while (span.start < span.end && isspace((unsigned char)text[span.start]))
        ++span.start;
    while (span.end > span.start && isspace((unsigned char)text[span.end - 1u]))
        --span.end;
    return span;
}

static char *equ_series_copy_span(const char *text, equ_series_span_t span)
{
    const size_t length = span.end - span.start;
    char *copy = malloc(length + 1u);

    if (!copy)
        return NULL;
    memcpy(copy, text + span.start, length);
    copy[length] = '\0';
    return copy;
}

static bool equ_series_is_ellipsis(const char *text, equ_series_span_t span)
{
    char *term;
    bool match;

    span = equ_series_trim(text, span);
    term = equ_series_copy_span(text, span);
    if (!term)
        return false;
    match = strcmp(term, "...") == 0 || strcmp(term, "…") == 0;
    free(term);
    return match;
}

static bool equ_series_exponent_plus(const char *text, size_t length, size_t pos)
{
    return pos > 1u && pos + 1u < length && (text[pos - 1u] == 'e' || text[pos - 1u] == 'E') &&
           (isdigit((unsigned char)text[pos - 2u]) || text[pos - 2u] == '.') &&
           isdigit((unsigned char)text[pos + 1u]);
}

static bool equ_series_split_terms(const char *text, equ_series_span_t **terms_out, size_t *count_out)
{
    const size_t length = strlen(text);
    equ_series_span_t *terms = calloc(length + 1u, sizeof(*terms));
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
                    !equ_series_exponent_plus(text, length, i)) {
                    terms[count++] = (equ_series_span_t){start, i};
                    start = i + 1u;
                }
                break;
            default:
                break;
        }
    }

    terms[count++] = (equ_series_span_t){start, length};
    *terms_out = terms;
    *count_out = count;
    return true;
}

static bool equ_series_parse_term(const char *text, equ_series_span_t span, long long *coefficient_out,
                                  char **factor_out)
{
    char *term;
    char *end = NULL;
    char *factor;
    long long coefficient = 1;

    *factor_out = NULL;
    span = equ_series_trim(text, span);
    term = equ_series_copy_span(text, span);
    if (!term || !*term) {
        free(term);
        return false;
    }

    if (isdigit((unsigned char)term[0])) {
        errno = 0;
        coefficient = strtoll(term, &end, 10);
        if (errno == ERANGE || end == term) {
            free(term);
            return false;
        }
        while (*end && isspace((unsigned char)*end))
            ++end;
        if (*end == '*') {
            ++end;
            while (*end && isspace((unsigned char)*end))
                ++end;
        }
    } else {
        end = term;
    }

    factor = strdup(end);
    free(term);
    if (!factor)
        return false;

    *coefficient_out = coefficient;
    *factor_out = factor;
    return true;
}

static int equ_series_append_slice(string_t *output, const char *text, size_t start, size_t end)
{
    equ_series_span_t span = {start, end};
    char *copy = equ_series_copy_span(text, span);
    int status;

    if (!copy)
        return -1;
    status = string_append_cstr(output, copy);
    free(copy);
    return status;
}

static bool equ_series_sum_coefficients(long long first, long long second, long long third, long long last,
                                        long long *sum_out)
{
    const __int128 first_difference = (__int128)second - (__int128)first;
    const __int128 second_difference = (__int128)third - 2 * (__int128)second + (__int128)first;
    __int128 endpoint_index = -1;
    __int128 count;
    __int128 pair_count;
    __int128 triple_count;
    __int128 sum;

    if (second_difference == 0) {
        const __int128 endpoint_delta = (__int128)last - (__int128)first;

        if (first_difference == 0 || endpoint_delta % first_difference != 0)
            return false;
        endpoint_index = endpoint_delta / first_difference;
    } else {
        for (__int128 index = 3; index < 1000000; ++index) {
            const __int128 term = (__int128)first + index * first_difference +
                                  index * (index - 1) / 2 * second_difference;

            if (term == (__int128)last) {
                endpoint_index = index;
                break;
            }
        }
    }
    if (endpoint_index < 3 || endpoint_index >= 1000000)
        return false;

    count = endpoint_index + 1;
    pair_count = count * (count - 1) / 2;
    triple_count = count * (count - 1) * (count - 2) / 6;
    sum = count * (__int128)first + pair_count * first_difference + triple_count * second_difference;
    if (sum < LLONG_MIN || sum > LLONG_MAX)
        return false;

    *sum_out = (long long)sum;
    return true;
}

static string_t *equ_series_expand_once(const string_t *side, bool *expanded_out)
{
    const char *text = string_c_str(side);
    equ_series_span_t *terms = NULL;
    size_t term_count = 0u;
    size_t ellipsis_index = SIZE_MAX;
    long long first = 0;
    long long second = 0;
    long long third = 0;
    long long last = 0;
    long long sum;
    char *first_factor = NULL;
    char *second_factor = NULL;
    char *third_factor = NULL;
    char *last_factor = NULL;
    string_t *output = NULL;

    *expanded_out = false;
    if (!equ_series_split_terms(text, &terms, &term_count))
        return NULL;

    for (size_t i = 0u; i < term_count; ++i) {
        if (equ_series_is_ellipsis(text, terms[i])) {
            ellipsis_index = i;
            break;
        }
    }
    if (ellipsis_index == SIZE_MAX) {
        free(terms);
        return string_clone(side);
    }
    if (ellipsis_index < 3u || ellipsis_index + 1u >= term_count)
        goto unsupported;

    if (!equ_series_parse_term(text, terms[ellipsis_index - 3u], &first, &first_factor) ||
        !equ_series_parse_term(text, terms[ellipsis_index - 2u], &second, &second_factor) ||
        !equ_series_parse_term(text, terms[ellipsis_index - 1u], &third, &third_factor) ||
        !equ_series_parse_term(text, terms[ellipsis_index + 1u], &last, &last_factor))
        goto unsupported;
    if (strcmp(first_factor, second_factor) != 0 || strcmp(first_factor, third_factor) != 0 ||
        strcmp(first_factor, last_factor) != 0)
        goto unsupported;

    if (!equ_series_sum_coefficients(first, second, third, last, &sum))
        goto unsupported;

    output = string_new();
    if (!output ||
        equ_series_append_slice(output, text, 0u, equ_series_trim(text, terms[ellipsis_index - 3u]).start) != 0)
        goto allocation_failure;
    if (!*first_factor) {
        if (string_append_format(output, "%lld", sum) < 0)
            goto allocation_failure;
    } else if (sum == 1) {
        if (string_append_cstr(output, first_factor) != 0)
            goto allocation_failure;
    } else if (sum == -1) {
        if (string_append_char(output, '-') != 0 || string_append_cstr(output, first_factor) != 0)
            goto allocation_failure;
    } else if (string_append_format(output, "%lld*(%s)", sum, first_factor) < 0) {
        goto allocation_failure;
    }
    if (equ_series_append_slice(output, text, equ_series_trim(text, terms[ellipsis_index + 1u]).end,
                                strlen(text)) != 0)
        goto allocation_failure;

    *expanded_out = true;
    free(last_factor);
    free(third_factor);
    free(second_factor);
    free(first_factor);
    free(terms);
    return output;

unsupported:
    fprintf(stderr,
            "equ_from_text: ellipsis must abbreviate an arithmetic or quadratic sequence of like additive terms\n");
    output = NULL;
    goto cleanup;

allocation_failure:
    string_free(output);
    output = NULL;

cleanup:
    free(last_factor);
    free(third_factor);
    free(second_factor);
    free(first_factor);
    free(terms);
    return output;
}

string_t *equ_expand_polynomial_series_side(string_view_t side)
{
    string_t *current = string_from_view(&side);

    if (!current)
        return NULL;

    for (size_t pass = 0u; pass < 32u; ++pass) {
        string_t *next;
        bool expanded = false;

        next = equ_series_expand_once(current, &expanded);
        string_free(current);
        if (!next)
            return NULL;
        current = next;
        if (!expanded)
            return current;
    }

    string_free(current);
    fprintf(stderr, "equ_from_text: too many polynomial sequence ellipses\n");
    return NULL;
}
