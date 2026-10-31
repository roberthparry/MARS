/**
 * @file equation_notation_input.c
 * @brief Native relation tokens and ordered mathematical relation presentation.
 *
 * Owns keyboard and Unicode relation spellings, recognises complete top-level
 * tokens and delegates each operand to its mathematical reader. Public TeX stack
 * operations supply symbol syntax. Relations are displayed without comparison,
 * tolerance selection or inequality solving; the ordinary equation parser shares
 * token recognition but accepts only one exact equality.
 */
#include <errno.h>
#include <stdint.h>

#include "string-t.h"
#include "TeX-t.h"

#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"

static bool equ_ascii_at(string_view_t view, size_t position, unsigned char expected)
{
    unsigned char value;
    return string_view_peek_ascii(view, position, &value) && value == expected;
}

static int equ_relation_token(string_view_t view, size_t position, uint32_t rune,
                              size_t *width, equ_relation_kind_t *kind)
{
    bool equal = equ_ascii_at(view, position + 1u, '=');
    switch (rune) {
        case '<':
            *kind = equal ? EQU_RELATION_LESS_EQUAL : EQU_RELATION_LESS;
            *width = equal ? 2u : 1u;
            return 1;
        case '>':
            *kind = equal ? EQU_RELATION_GREATER_EQUAL : EQU_RELATION_GREATER;
            *width = equal ? 2u : 1u;
            return 1;
        case '=':
            *kind = EQU_RELATION_EQUAL;
            *width = equal ? 2u : 1u;
            return 1;
        case '!':
            if (!equal)
                return 0; /* A lone exclamation mark belongs to factorial syntax. */
            *kind = EQU_RELATION_NOT_EQUAL;
            *width = 2u;
            return 1;
        case '~':
            if (!equ_ascii_at(view, position + 2u, '='))
                return -1;
            if (equ_ascii_at(view, position + 1u, '~'))
                *kind = EQU_RELATION_APPROXIMATE;
            else if (equal)
                *kind = EQU_RELATION_CONGRUENT;
            else
                return -1;
            *width = 3u;
            return 1;
        case 0x2260u:
            *kind = EQU_RELATION_NOT_EQUAL;
            return 1;
        case 0x2264u:
            *kind = EQU_RELATION_LESS_EQUAL;
            return 1;
        case 0x2265u:
            *kind = EQU_RELATION_GREATER_EQUAL;
            return 1;
        case 0x2248u:
            *kind = EQU_RELATION_APPROXIMATE;
            return 1;
        case 0x2245u:
            *kind = EQU_RELATION_CONGRUENT;
            return 1;
        default:
            return 0;
    }
}

/* Traverse each source span once, retaining nested operand syntax for its owning parser. */
int equ_input_next_relation(string_view_t body, size_t *start, equ_input_relation_t *relation)
{
    size_t length = string_view_length(body);
    if (!start || !relation || *start > length) {
        errno = EINVAL;
        return -1;
    }
    size_t parentheses = 0, brackets = 0, braces = 0;
    for (size_t position = *start; position < length;) {
        uint32_t rune;
        size_t next;
        if (!string_view_peek_rune_value(body, position, &rune, &next))
            goto malformed;
        if (!parentheses && !brackets && !braces) {
            size_t width = next - position;
            equ_relation_kind_t kind;
            int matched = equ_relation_token(body, position, rune, &width, &kind);
            if (matched < 0) {
                errno = EINVAL;
                return -1;
            }
            if (matched) {
                *relation = (equ_input_relation_t){position, width, kind};
                *start = position + width;
                return 1;
            }
        }
        switch (rune) {
            case '(':
                ++parentheses;
                break;
            case ')':
                if (!parentheses) goto malformed;
                --parentheses;
                break;
            case '[':
                ++brackets;
                break;
            case ']':
                if (!brackets) goto malformed;
                --brackets;
                break;
            case '{':
                ++braces;
                break;
            case '}':
                if (!braces) goto malformed;
                --braces;
                break;
            default:
                break;
        }
        position = next;
    }
    if (!parentheses && !brackets && !braces) {
        *start = length;
        return 0;
    }
malformed:
    errno = EINVAL;
    return -1;
}

static TeX_t *equ_read_operand(string_view_t operand, string_view_t bindings, bool bound,
                                notation_read_fn read, void *context)
{
    operand = string_view_trim(operand);
    if (string_view_is_empty(operand)) {
        errno = EINVAL;
        return NULL;
    }
    string_t *text = string_from_view(&operand);
    string_t *declarations = bound ? string_from_view(&bindings) : NULL;
    if (bound) {
        string_t *wrapped = text && declarations ? string_sprintf("{ %S | %S }", text, declarations) : NULL;
        string_free(text);
        text = wrapped;
    }
    TeX_t *result = text ? read(context, text) : NULL;
    string_free(declarations);
    string_free(text);
    return result;
}

/* Compose authored relation chains through mathematical operand callbacks and public TeX operations. */
TeX_t *equ_notation_input(const string_t *text, notation_read_fn read, void *context, bool *recognised)
{
    if (!text || !read || !recognised) {
        errno = EINVAL;
        return NULL;
    }
    *recognised = false;
    string_view_t body, bindings;
    bool bound;
    if (!equ_input_parts(text, &body, &bindings, &bound)) {
        *recognised = true;
        return NULL;
    }
    size_t next = 0;
    equ_input_relation_t relation;
    int matched = equ_input_next_relation(body, &next, &relation);
    if (!matched)
        return NULL;
    *recognised = true;
    if (matched < 0)
        return NULL;
    static bool (*const operations[EQU_RELATION_COUNT])(TeX_builder_t *) = {
        [EQU_RELATION_EQUAL]         = TeX_eq,
        [EQU_RELATION_NOT_EQUAL]     = TeX_ne,
        [EQU_RELATION_LESS]          = TeX_lt,
        [EQU_RELATION_LESS_EQUAL]    = TeX_le,
        [EQU_RELATION_GREATER]       = TeX_gt,
        [EQU_RELATION_GREATER_EQUAL] = TeX_ge,
        [EQU_RELATION_APPROXIMATE]   = TeX_approx,
        [EQU_RELATION_CONGRUENT]     = TeX_cong
    };
    TeX_builder_t *builder = TeX_builder_new();
    string_view_t first = string_view_slice(body, 0, relation.position);
    TeX_t *operand = equ_read_operand(first, bindings, bound, read, context);
    bool valid = operand && TeX_push_notation(builder, operand);
    TeX_free(operand);
    while (valid && matched > 0) {
        equ_relation_kind_t operation = relation.kind;
        size_t start = next;
        matched = equ_input_next_relation(body, &next, &relation);
        if (matched < 0) {
            valid = false;
            break;
        }
        size_t end = matched ? relation.position : string_view_length(body);
        string_view_t side = string_view_slice(body, start, end - start);
        operand = equ_read_operand(side, bindings, bound, read, context);
        valid = operand && TeX_push_notation(builder, operand) && operations[operation](builder);
        TeX_free(operand);
    }
    TeX_t *result = valid ? TeX_builder_finish(builder) : NULL;
    TeX_builder_free(builder);
    return result;
}
