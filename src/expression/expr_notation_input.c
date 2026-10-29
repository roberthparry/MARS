/**
 * @file expr_notation_input.c
 * @brief Convert native scalar, matrix and differential-equation source into TeX.
 *
 * Keeps scalar parsing and algebra in the expression module and matrix entries in the matrix module.
 * Outer bindings are passed to the native parsers; initial conditions belong to the differential-equation module.
 * Equation sides are interpreted through an operand-reader callback. Matrix products and powers are delegated
 * to matrix. This adapter uses public TeX composition without embedding typesetting syntax; applications can
 * use basic TeX constructors without linking these parsers until they request mathematical text input.
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "expression-t.h"
#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_SHARED_MATRIX_INTERNAL_ACCESS
#include "internal/matrix_internal.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
#define MARS_SHARED_DIFFEQUATION_INTERNAL_ACCESS
#include "internal/diffequation_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_notation_internal.h"

/* Interpret scalar mathematics here and request notation from the existing TeX backend. */
TeX_t *expr_notation_scalar(const string_t *text, bool formal)
{
    expr_t *parsed = formal ? expr_from_presentation_text_internal(text)
                            : expr_from_text_preserving_calculus(text, NULL);
    if (!parsed)
        return NULL;
    bool calculus = expr_contains_calculus_request(parsed);
    expr_t *arranged = calculus ? NULL : expr_beautify(parsed);
    TeX_context_t *context = expr_TeX_context(calculus ? parsed : arranged);
    TeX_t *result = context ? TeX_from_context(context) : NULL;
    TeX_context_free(context);
    expr_free(arranged);
    expr_free(parsed);
    return result;
}

/* Split only the outer envelope. The owning parsers still interpret and validate every binding. */
static bool expr_notation_parts(const string_t *text, string_t **body, string_t **bindings, bool *differential)
{
    string_cursor_t *cursor = string_cursor_new(text);
    if (!cursor)
        return false;
    string_cursor_skip_spaces(cursor);
    unsigned char c;
    bool wrapped = string_cursor_peek_ascii(cursor, &c) && c == '{';
    string_pos_t start = string_cursor_position(cursor) + (wrapped ? 1u : 0u);
    string_pos_t end = string_byte_length(text), pipe = SIZE_MAX;
    size_t parentheses = 0u, brackets = 0u, braces = 0u, semicolons = 0u;
    bool lhs = true, condition_call = false, derivative = false, closed = !wrapped, valid = true;
    while (!string_cursor_done(cursor)) {
        if (string_cursor_peek_ascii(cursor, &c)) {
            if (c == '[')
                ++brackets;
            else if (c == ']' && brackets)
                --brackets;
            else if (!brackets) {
                if (c == '\'')
                    *differential = true;
                if (pipe == SIZE_MAX) {
                    unsigned char next = 0;
                    string_pos_t position = string_cursor_position(cursor);
                    string_cursor_peek_ascii_at(cursor, position + 1u, &next);
                    derivative = derivative || (c == '/' && next == 'd') ||
                                 (c == 'D' && (next == '(' || (next >= 'a' && next <= 'z')));
                }
                if (c == '{')
                    ++braces;
                else if (c == '}' && braces) {
                    if (--braces == 0u && wrapped) {
                        end = string_cursor_position(cursor);
                        closed = true;
                        string_cursor_next(cursor);
                        string_cursor_skip_spaces(cursor);
                        valid = string_cursor_done(cursor);
                        break;
                    }
                } else if (c == '(') {
                    if (!parentheses && pipe != SIZE_MAX && lhs)
                        condition_call = true;
                    ++parentheses;
                } else if (c == ')' && parentheses) {
                    --parentheses;
                } else if (!parentheses && braces == (wrapped ? 1u : 0u)) {
                    if (c == '|' && pipe == SIZE_MAX) {
                        pipe = string_cursor_position(cursor);
                        lhs = true;
                        condition_call = false;
                    } else if (c == '=' && pipe != SIZE_MAX) {
                        unsigned char previous = 0;
                        string_pos_t position = string_cursor_position(cursor);
                        if (position)
                            string_cursor_peek_ascii_at(cursor, position - 1u, &previous);
                        if (condition_call && previous != '<' && previous != '>' && previous != '!')
                            *differential = true;
                        lhs = false;
                    } else if (c == ',' || c == ';') {
                        lhs = true;
                        condition_call = false;
                        if (pipe != SIZE_MAX && c == ';')
                            ++semicolons;
                    }
                }
            }
        } else if (!brackets && pipe == SIZE_MAX &&
                   string_cursor_match_at(cursor, string_cursor_position(cursor), "∂")) {
            derivative = true;
        }
        if (string_cursor_next(cursor)) {
            valid = false;
            break;
        }
    }
    string_cursor_free(cursor);
    if (!valid || !closed || brackets || parentheses)
        return false;
    *differential = *differential || (semicolons >= 2u && derivative);
    string_view_t body_view = string_view_slice(string_view_all(text), start,
                                               (pipe == SIZE_MAX ? end : pipe) - start);
    *body = string_from_view(&body_view);
    if (pipe != SIZE_MAX) {
        string_view_t binding_view = string_view_slice(string_view_all(text), pipe + 1u, end - pipe - 1u);
        *bindings = string_from_view(&binding_view);
    }
    return *body && (pipe == SIZE_MAX || *bindings);
}

/* Equation owns the separator; this callback owns interpretation of each operand. */
static TeX_t *expr_notation_read_value(void *context, const string_t *text)
{
    (void)context;
    string_t *body = NULL, *bindings = NULL;
    bool differential = false, recognised = false;
    if (!expr_notation_parts(text, &body, &bindings, &differential)) {
        string_free(body);
        string_free(bindings);
        return NULL;
    }
    TeX_t *result = mat_notation_input(body, bindings, &recognised);
    if (!recognised)
        result = expr_notation_scalar(text, true);
    string_free(body);
    string_free(bindings);
    return result;
}

/* Delegate mathematical interpretation and presentation to the expression module. */
TeX_t *expr_notation_from_text(const string_t *text)
{
    if (!text) {
        errno = EINVAL;
        return NULL;
    }
    string_cursor_t *cursor = string_cursor_new(text);
    if (!cursor)
        return NULL;
    /* Validate the entire input, so a NUL cannot hide trailing mathematical text. */
    while (!string_cursor_done(cursor)) {
        unsigned char ascii;
        if ((string_cursor_peek_ascii(cursor, &ascii) && !ascii) || string_cursor_next(cursor)) {
            string_cursor_free(cursor);
            errno = EINVAL;
            return NULL;
        }
    }
    string_cursor_free(cursor);
    string_t *body = NULL, *bindings = NULL;
    bool differential = false;
    if (!expr_notation_parts(text, &body, &bindings, &differential)) {
        string_free(bindings);
        string_free(body);
        return NULL;
    }
    TeX_t *notation = NULL;
    if (differential) {
        diffequ_t *problem = de_from_text(text);
        TeX_context_t *context = problem ? de_TeX_context(problem) : NULL;
        notation = context ? TeX_from_context(context) : NULL;
        TeX_context_free(context);
        de_free(problem);
    } else {
        bool equation = false;
        notation = equ_notation_input(text, expr_notation_read_value, NULL, &equation);
        if (!equation) {
            bool matrix = false;
            notation = mat_notation_input(body, bindings, &matrix);
            if (!matrix)
                notation = expr_notation_scalar(text, false);
        }
    }
    string_free(bindings);
    string_free(body);
    return notation;
}
