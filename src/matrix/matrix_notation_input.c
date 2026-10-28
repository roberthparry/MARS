/**
 * @file matrix_notation_input.c
 * @brief Parse matrix notation for the mathematical text convenience interface.
 *
 * Owns matrix delimiters and symbolic powers, delegates scalar operands through
 * the native input interface, and composes notation without TeX command strings.
 */
#include <stdint.h>
#include <string.h>

#include "matrix-t.h"
#include "TeX-t.h"

#define MARS_SHARED_EXPR_NOTATION_INTERNAL_ACCESS
#include "internal/expr_notation_internal.h"
#define MARS_SHARED_MATRIX_NOTATION_INTERNAL_ACCESS
#include "internal/matrix_notation_internal.h"


/* A compound exponent must be grouped; never reinterpret a trailing matrix sum as part of the power. */
static bool mat_notation_power_is_grouped(const string_t *text)
{
    string_cursor_t *cursor = string_cursor_new(text);
    if (!cursor)
        return false;
    string_cursor_skip_spaces(cursor);
    size_t depth = 0u, brackets = 0u;
    bool first = true, ended = false, valid = true;
    while (!string_cursor_done(cursor)) {
        unsigned char c;
        if (!depth && !brackets && ended) {
            string_cursor_skip_spaces(cursor);
            valid = string_cursor_done(cursor);
            break;
        }
        if (string_cursor_peek_ascii(cursor, &c)) {
            if (c == '[')
                ++brackets;
            else if (c == ']' && brackets)
                --brackets;
            else if (!brackets) {
                if (c == '(')
                    ++depth;
                else if (c == ')') {
                    if (!depth) {
                        valid = false;
                        break;
                    }
                    ended = --depth == 0u;
                } else if (!depth && strchr("+-*/=,;|^", c) && !(first && (c == '+' || c == '-'))) {
                    valid = false;
                    break;
                } else if (!depth && (c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
                    ended = !first;
                }
            }
        }
        first = false;
        if (string_cursor_next(cursor)) {
            valid = false;
            break;
        }
    }
    string_cursor_free(cursor);
    return valid && !first && !depth && !brackets;
}

/* One delimiter traversal locates explicit products; recursive operands retain native grammar. */
static TeX_t *mat_notation_product(const string_t *text, const string_t *bindings, bool *recognised)
{
    string_view_t view = string_view_all(text);
    size_t split = SIZE_MAX, depth = 0u, brackets = 0u;
    bool operand = true;
    for (size_t i = 0u; i < string_view_length(view); ++i) {
        unsigned char c;
        if (!string_view_peek_ascii(view, i, &c))
            continue;
        if (c == '[')
            ++brackets;
        else if (c == ']' && brackets)
            --brackets;
        else if (!brackets) {
            if (c == '(') {
                ++depth;
                operand = false;
            } else if (c == ')' && depth) {
                --depth;
                operand = false;
            } else if (!depth && c == '*') {
                split = i;
                operand = true;
            } else if (!depth && ((c == '+' || c == '-') && !operand))
                return NULL;
            else if (!depth && c == '=')
                return NULL;
            else if (!depth && !strchr(" \t\n\r+-", c))
                operand = c == '/' || c == '^';
        }
    }
    if (split == SIZE_MAX)
        return NULL;
    string_view_t lv = string_view_slice(view, 0u, split);
    string_view_t rv = string_view_slice(view, split + 1u, string_view_length(view) - split - 1u);
    string_t *left = string_from_view(&lv), *right = string_from_view(&rv);
    bool lm = false, rm = false;
    TeX_t *lhs = left ? mat_notation_input(left, bindings, &lm) : NULL;
    TeX_t *rhs = right ? mat_notation_input(right, bindings, &rm) : NULL;
    *recognised = lm || rm;
    TeX_t *result = NULL;
    if (*recognised && lm != rm) {
        const string_t *scalar_text = lm ? right : left;
        string_t *bound = scalar_text && bindings ? string_sprintf("{ %S | %S }", scalar_text, bindings) : NULL;
        TeX_t *factor = scalar_text && (!bindings || bound)
                            ? expr_notation_scalar(bound ? bound : scalar_text, false) : NULL;
        TeX_t *matrix = lm ? lhs : rhs;
        if (factor && matrix)
            result = TeX_scaled(factor, matrix);
        TeX_free(factor);
        string_free(bound);
    }
    TeX_free(lhs);
    TeX_free(rhs);
    string_free(left);
    string_free(right);
    return result;
}

/* Inspect delimiters once; entry syntax belongs exclusively to the native matrix parser. */
TeX_t *mat_notation_input(const string_t *text, const string_t *bindings, bool *recognised)
{
    TeX_t *product = mat_notation_product(text, bindings, recognised);
    if (*recognised)
        return product;
    string_cursor_t *cursor = string_cursor_new(text);
    if (!cursor) {
        *recognised = true;
        return NULL;
    }
    string_cursor_skip_spaces(cursor);
    unsigned char character;
    if (!string_cursor_peek_ascii(cursor, &character) || character != '(') {
        string_cursor_free(cursor);
        return NULL;
    }
    string_pos_t start = string_cursor_position(cursor), end = start;
    size_t parentheses = 0u, brackets = 0u, braces = 0u;
    while (!string_cursor_done(cursor)) {
        if (string_cursor_peek_ascii(cursor, &character)) {
            if (character == '[')
                ++brackets;
            else if (character == ']' && brackets)
                --brackets;
            else if (!brackets) {
                if (character == '{')
                    ++braces;
                else if (character == '}' && braces)
                    --braces;
                else if (!braces) {
                    if (character == '(')
                        ++parentheses;
                    else if (character == ')') {
                        if (--parentheses == 0u) {
                            string_cursor_next(cursor);
                            end = string_cursor_position(cursor);
                            break;
                        }
                    } else if (parentheses == 1u && (character == ',' || character == ';')) {
                        *recognised = true;
                    }
                }
            }
        }
        if (string_cursor_next(cursor))
            break;
    }
    if (!*recognised || end == start) {
        string_cursor_free(cursor);
        return NULL;
    }
    string_view_t literal_view = string_view_slice(string_view_all(text), start, end - start);
    string_t *literal = string_from_view(&literal_view);
    if (literal && bindings) {
        string_t *bound = string_sprintf("{ %S | %S }", literal, bindings);
        string_free(literal);
        literal = bound;
    }
    matrix_t *matrix = literal ? mat_from_text_expr(literal, NULL) : NULL;
    TeX_context_t *context = matrix ? mat_TeX_context(matrix) : NULL;
    TeX_t *base = context ? TeX_from_context(context) : NULL;
    TeX_context_free(context);
    TeX_t *result = NULL;
    string_cursor_skip_spaces(cursor);
    if (string_cursor_done(cursor)) {
        result = base;
        base = NULL;
    } else if (base && string_cursor_peek_ascii(cursor, &character) && character == '^') {
        string_cursor_next(cursor);
        string_pos_t position = string_cursor_position(cursor);
        string_view_t exponent_view = string_view_slice(string_view_all(text), position,
                                                        string_byte_length(text) - position);
        string_t *exponent = string_from_view(&exponent_view);
        bool valid = exponent && mat_notation_power_is_grouped(exponent);
        if (valid && bindings) {
            string_t *bound = string_sprintf("{ %S | %S }", exponent, bindings);
            string_free(exponent);
            exponent = bound;
        }
        TeX_t *power = valid && exponent ? expr_notation_scalar(exponent, false) : NULL;
        if (power)
            result = TeX_scripts(base, NULL, power);
        TeX_free(power);
        string_free(exponent);
    }
    TeX_free(base);
    mat_free(matrix);
    string_free(literal);
    string_cursor_free(cursor);
    return result;
}
