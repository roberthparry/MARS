/**
 * @file TeX_arithmetic.c
 * @brief Structured symbol and arithmetic presentation for reverse Polish construction.
 *
 * Supplies operator syntax and grouping from stored presentation kinds, never
 * by parsing generated TeX. Symbols accept plain UTF-8 names rather than command
 * fragments. These helpers construct notation without evaluating or rearranging algebra.
 */
#include <errno.h>
#include <stdlib.h>

#include "string-t.h"
#include "TeX-t.h"

#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_internal.h"

static const TeX_t *TeX_grouped(TeX_node_kind_t parent, const TeX_t *body, size_t index, TeX_t **owned)
{
    *owned = NULL;
    if (!body)
        return NULL;
    TeX_node_kind_t kind = TeX_kind(body);
    bool negative = kind == TeX_NODE_NEGATIVE &&
        (parent == TeX_NODE_PRODUCT || parent == TeX_NODE_NEGATIVE || (index && parent == TeX_NODE_SUM));
    if (kind != TeX_NODE_COUNT && !negative && !TeX_needs_fence(parent, kind, index))
        return body;
    *owned = TeX_classify(TeX_compose_fenced(body, TeX_FENCE_PARENTHESES), TeX_NODE_FENCED);
    return *owned;
}

/* Validate an identifier once, selecting recognised singleton glyphs by direct indexing. */
TeX_t *TeX_symbol(const char *name)
{
    if (!name || !*name) { errno = EINVAL; return NULL; }
    string_t *input = string_new_with(name);
    string_cursor_t *cursor = input ? string_cursor_new(input) : NULL;
    bool valid = cursor != NULL, first = true;
    unsigned singleton = 0;
    while (valid && !string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);
        unsigned cp = rune_value(rune);
        bool alphanumeric = rune_is_alpha_numeric(rune);
        bool glyph = !alphanumeric && TeX_symbol_codepoint(cp) != NULL;
        valid = (alphanumeric || cp == '_' || (first && glyph)) &&
                (!first || !rune_is_digit(rune)) && (!singleton || first);
        if (first && glyph) singleton = cp;
        if (string_cursor_next(cursor)) valid = false;
        if (first && string_cursor_done(cursor)) singleton = cp;
        first = false;
    }
    TeX_t *result = NULL;
    if (valid) {
        const char *alias = TeX_symbol_name(input);
        if (!alias && singleton)
            alias = TeX_symbol_codepoint(singleton);
        result = alias ? TeX_raw(alias) : TeX_identifier(input);
        TeX_classify(result, TeX_NODE_IDENTIFIER);
    } else errno = cursor ? EINVAL : ENOMEM;
    string_cursor_free(cursor);
    string_free(input);
    return result;
}

/* Join two operands with an operator owned by TeX and group the right difference when necessary. */
TeX_t *TeX_arithmetic_binary(const TeX_t *left, const TeX_t *right, bool subtract)
{
    TeX_node_kind_t kind = subtract ? TeX_NODE_DIFFERENCE : TeX_NODE_SUM;
    TeX_t *owned_left = NULL, *owned_right = NULL;
    const TeX_t *lhs = TeX_grouped(kind, left, 0, &owned_left);
    const TeX_t *rhs = TeX_grouped(kind, right, 1, &owned_right);
    TeX_t *separator = TeX_raw(subtract ? " - " : " + ");
    const TeX_t *parts[] = {lhs, separator, rhs};
    TeX_t *result = lhs && rhs && separator ? TeX_compose_sequence(parts, 3) : NULL;
    TeX_free(owned_left); TeX_free(owned_right); TeX_free(separator);
    return TeX_classify(result, kind);
}

/* Prefix negation only after preserving the complete operand's visual scope. */
TeX_t *TeX_arithmetic_negative(const TeX_t *body)
{
    TeX_t *owned = NULL;
    const TeX_t *operand = TeX_grouped(TeX_NODE_NEGATIVE, body, 0, &owned);
    string_t *source = operand ? string_sprintf("-%s", TeX_source(operand)) : NULL;
    TeX_t *result = source ? TeX_raw(string_c_str(source)) : NULL;
    string_free(source); TeX_free(owned);
    return TeX_classify(result, TeX_NODE_NEGATIVE);
}

/* Multiply exactly two factors, retaining scope and keeping numeric products unambiguous. */
TeX_t *TeX_arithmetic_multiply(const TeX_t *left, const TeX_t *right)
{
    if (!left || !right) {
        errno = EINVAL;
        return NULL;
    }
    TeX_t *owned_left = NULL, *owned_right = NULL;
    const TeX_t *lhs = TeX_grouped(TeX_NODE_PRODUCT, left, 0, &owned_left);
    const TeX_t *rhs = TeX_grouped(TeX_NODE_PRODUCT, right, 1, &owned_right);
    TeX_node_kind_t lkind = TeX_kind(left), rkind = TeX_kind(right);
    bool numeric = (lkind == TeX_NODE_NUMBER && (rkind == TeX_NODE_NUMBER || rkind == TeX_NODE_PRODUCT)) ||
                   (rkind == TeX_NODE_NUMBER && lkind == TeX_NODE_PRODUCT);
    string_t *source = lhs && rhs ? string_sprintf("%s%s%s", TeX_source(lhs), numeric ? " \\times " : "\\,",
                                                 TeX_source(rhs)) : NULL;
    TeX_t *result = source ? TeX_raw(string_c_str(source)) : NULL;
    string_free(source);
    TeX_free(owned_left); TeX_free(owned_right);
    return TeX_classify(result, TeX_NODE_PRODUCT);
}

/* Reuse script syntax while protecting compound bases using their stored structure. */
TeX_t *TeX_arithmetic_scripts(const TeX_t *base, const TeX_t *lower, const TeX_t *upper)
{
    TeX_node_kind_t kind = upper ? TeX_NODE_POWER : TeX_NODE_SUBSCRIPT;
    TeX_t *owned = NULL;
    const TeX_t *body = TeX_grouped(kind, base, 0, &owned);
    TeX_t *result = body ? TeX_compose_scripts(body, lower, upper) : NULL;
    TeX_free(owned);
    return TeX_classify(result, kind);
}

/* Preserve the scope of a complete operand before attaching derivative primes. */
TeX_t *TeX_arithmetic_prime(const TeX_t *body, unsigned order)
{
    if (!order) {
        errno = EINVAL;
        return NULL;
    }
    TeX_t *owned = NULL;
    const TeX_t *operand = TeX_grouped(TeX_NODE_PRIME, body, 0, &owned);
    TeX_t *result = operand ? TeX_compose_prime(operand, order) : NULL;
    TeX_free(owned);
    return TeX_classify(result, TeX_NODE_PRIME);
}
