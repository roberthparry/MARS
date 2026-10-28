/**
 * @file expr_notation_coefficients.c
 * @brief Prepare rational-power display through semantic notation.
 *
 * Expression decides when a principal root represents an exact power; TeX
 * supplies root, fraction and grouping presentation without native graph access.
 */
#include <ctype.h>
#include <stdlib.h>

#include "expression-t.h"

#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_SHARED_TeX_SEMANTIC_INTERNAL_ACCESS
#include "internal/TeX_semantic_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_bindings.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_notation_internal.h"

/* Use a root only for unit rational powers, preserving the complex principal branch. */
bool emit_TeX_unit_fraction_power(const expr_t *base, number_t exponent, sbuf_t *buffer, int parent_prec)
{
    long numerator, denominator;
    if (!num_get_small_rational(exponent, &numerator, &denominator) ||
        (numerator != 1L && numerator != -1L) || denominator <= 1L)
        return false;
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t *radicand = build ? notation_build_expr(build, base) : NULL;
    number_t degree = num_create_from_long(denominator);
    const TeX_model_node_t *parts[] = {radicand, build ? notation_build_number(build, degree) : NULL};
    num_destroy(&degree);
    const TeX_model_node_t *root = radicand && parts[1] ? notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_ROOT, .child_count = denominator == 2L ? 1u : 2u}, parts) : NULL;
    if (root && numerator < 0L) {
        const TeX_model_node_t *fraction[] = {notation_build_literal(build, TeX_NODE_NUMBER, "1"), root};
        root = notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_FRACTION, .child_count = 2u}, fraction);
    }
    string_t *text = root ? notation_build_render(build, root) : NULL;
    notation_build_free(build);
    if (!text)
        return false;
    if ((numerator < 0L ? PREC_MUL : PREC_UNARY) < parent_prec) {
        string_t *grouped = TeX_parenthesise_compact(text);
        string_free(text);
        text = grouped;
    }
    if (!text)
        return false;
    sbuf_puts(buffer, string_c_str(text));
    string_free(text);
    return true;
}

/* Preserve exact negative binding values while each presentation owns its syntax. */
bool emit_negative_const_binding_expr_abs(const expr_t *expression, sbuf_t *buffer, bool TeX)
{
    if (!expression || !expression->binding_expr)
        return false;
    char *raw = TeX ? expr_binding_expr_to_TeX(expression->binding_expr)
                    : expr_binding_expr_to_string(expression->binding_expr);
    string_t *text = raw ? string_new_with(raw) : NULL;
    free(raw);
    if (!text)
        return false;
    if (TeX) {
        string_t *absolute = TeX_negative_coefficient_abs(text);
        if (absolute)
            sbuf_puts(buffer, string_c_str(absolute));
        bool success = absolute != NULL;
        string_free(absolute);
        string_free(text);
        return success;
    }
    string_cursor_t *cursor = string_cursor_new(text);
    if (!cursor) {
        string_free(text);
        return false;
    }
    string_cursor_skip_spaces(cursor);
    bool success = false;
    if (rune_is_equal(string_cursor_peek(cursor), '-')) {
        string_cursor_next(cursor);
        string_cursor_skip_spaces(cursor);
        string_pos_t start = string_cursor_position(cursor);
        unsigned char digit;
        while (string_cursor_peek_ascii(cursor, &digit) && isdigit(digit))
            string_cursor_next(cursor);
        string_pos_t end = string_cursor_position(cursor);
        string_t *part = NULL;
        if (end > start && string_cursor_consume(cursor, "·π")) {
            part = string_cursor_slice_between(start, end, cursor);
            if (part) {
                sbuf_puts(buffer, string_c_str(part));
                sbuf_puts(buffer, "π");
                string_free(part);
                part = string_cursor_slice_between(string_cursor_position(cursor),
                                                   string_cursor_end_position(cursor), cursor);
            }
        } else {
            part = string_cursor_slice_between(start, string_cursor_end_position(cursor), cursor);
        }
        if (part) {
            sbuf_puts(buffer, string_c_str(part));
            string_free(part);
            success = true;
        }
    }
    string_cursor_free(cursor);
    string_free(text);
    return success;
}
