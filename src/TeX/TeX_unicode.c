/**
 * @file TeX_unicode.c
 * @brief Unicode symbols and perfect-hash TeX translations.
 *
 * Converts mathematical Unicode symbols into notation without accessing native
 * mathematical object layouts. Uses string cursors and direct indexed lookups.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string-t.h"

#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "internal/TeX_internal.h"
#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_internal.h"

#define TeX_FRACTION_HASH_SIZE 21u
#define TeX_SYMBOL_HASH_SIZE 9u

typedef struct {
    unsigned int codepoint;
    const char *text;
} TeX_character_map_t;

static const TeX_character_map_t TeX_vulgar_fractions[TeX_FRACTION_HASH_SIZE] = {
    [0] = {0x00BD, "\\frac{1}{2}"},
    [1] = {0x00BE, "\\frac{3}{4}"},
    [2] = {0x2150, "\\frac{1}{7}"},
    [3] = {0x2151, "\\frac{1}{9}"},
    [4] = {0x2152, "\\frac{1}{10}"},
    [5] = {0x2153, "\\frac{1}{3}"},
    [6] = {0x2154, "\\frac{2}{3}"},
    [7] = {0x2155, "\\frac{1}{5}"},
    [8] = {0x2156, "\\frac{2}{5}"},
    [9] = {0x2157, "\\frac{3}{5}"},
    [10] = {0x2158, "\\frac{4}{5}"},
    [11] = {0x2159, "\\frac{1}{6}"},
    [12] = {0x215A, "\\frac{5}{6}"},
    [13] = {0x215B, "\\frac{1}{8}"},
    [14] = {0x215C, "\\frac{3}{8}"},
    [15] = {0x215D, "\\frac{5}{8}"},
    [16] = {0x215E, "\\frac{7}{8}"},
    [20] = {0x00BC, "\\frac{1}{4}"}
};

static const TeX_character_map_t TeX_symbols[TeX_SYMBOL_HASH_SIZE] = {
    [0] = {0x2260, "\\neq"},
    [1] = {0x2202, "\\partial"},
    [2] = {0x221E, "\\infty"},
    [3] = {0x00B7, "\\mkern-2mu "},
    [4] = {0x2264, "\\leq"},
    [5] = {0x2265, "\\geq"},
    [7] = {0x221A, "\\sqrt{}"},
    [8] = {0x00D7, " \\times "}
};

typedef struct {
    unsigned int codepoint;
    char ascii;
} TeX_script_character_t;

static const TeX_script_character_t TeX_superscript[28] = {
    [0]  = {0x207C, '='},
    [1]  = {0x207D, '('},
    [2]  = {0x207E, ')'},
    [10] = {0x00B2, '2'},
    [11] = {0x00B3, '3'},
    [16] = {0x2070, '0'},
    [17] = {0x00B9, '1'},
    [20] = {0x2074, '4'},
    [21] = {0x2075, '5'},
    [22] = {0x2076, '6'},
    [23] = {0x2077, '7'},
    [24] = {0x2078, '8'},
    [25] = {0x2079, '9'},
    [26] = {0x207A, '+'},
    [27] = {0x207B, '-'}
};

static char TeX_script_character(unsigned int cp, bool superscript)
{
    if (superscript) {
        const TeX_script_character_t *entry = &TeX_superscript[cp % 28u];
        return entry->codepoint == cp ? entry->ascii : '\0';
    }
    static const char subscript[] = "0123456789+-=()";
    return cp >= 0x2080u && cp - 0x2080u < sizeof subscript - 1u ? subscript[cp - 0x2080u] : '\0';
}

static int TeX_collect_script(string_cursor_t *cursor, string_t *output, bool superscript)
{
    while (!string_cursor_done(cursor)) {
        char mapped = TeX_script_character(rune_value(string_cursor_peek(cursor)), superscript);
        if (!mapped)
            break;
        if (string_append_char(output, mapped) || string_cursor_next(cursor))
            return -1;
    }
    return 0;
}

static const char *TeX_character_lookup(const TeX_character_map_t *table, size_t index, unsigned int cp)
{
    return table[index].codepoint == cp ? table[index].text : NULL;
}

/* Convert Unicode mathematical symbols without accessing native expression structures. */
char *TeX_from_unicode_math(const char *text)
{
    string_t *source = string_new_with(text ? text : "");
    string_t *output = string_new();
    string_cursor_t *cursor = source ? string_cursor_new(source) : NULL;
    if (!output || !cursor)
        goto fail;
    while (!string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);
        unsigned int cp = rune_value(rune);
        const char *mapped = TeX_character_lookup(TeX_vulgar_fractions,
                                                  cp % TeX_FRACTION_HASH_SIZE, cp);
        if (mapped) {
            if (string_append_cstr(output, mapped) || string_cursor_next(cursor))
                goto fail;
            continue;
        }
        bool superscript = TeX_script_character(cp, true) != '\0';
        if (superscript || TeX_script_character(cp, false)) {
            string_t *first = string_new(), *second = NULL;
            if (!first || TeX_collect_script(cursor, first, superscript)) {
                string_free(first);
                goto fail;
            }
            if (superscript && rune_value(string_cursor_peek(cursor)) == 0x2044u) {
                string_cursor_next(cursor);
                second = string_new();
                if (!second || TeX_collect_script(cursor, second, false)) {
                    string_free(first);
                    string_free(second);
                    goto fail;
                }
            }
            int status = second && string_byte_length(second)
                ? string_append_format(output, "\\frac{%S}{%S}", first, second)
                : string_append_format(output, superscript ? "^{%S}" : "_{%S}", first);
            string_free(first);
            string_free(second);
            if (status < 0)
                goto fail;
            continue;
        }
        mapped = TeX_semantic_greek(cp);
        if (mapped) {
            if (string_append_cstr(output, mapped) || string_cursor_next(cursor))
                goto fail;
            unsigned char next;
            if (string_cursor_peek_ascii(cursor, &next) &&
                ((next >= 'A' && next <= 'Z') || (next >= 'a' && next <= 'z')) && string_append_cstr(output, "{}"))
                goto fail;
            continue;
        }
        mapped = TeX_character_lookup(TeX_symbols,
                                      (cp ^ (cp >> 12u)) % TeX_SYMBOL_HASH_SIZE, cp);
        if ((mapped ? string_append_cstr(output, mapped) : string_append_rune(output, rune)) ||
            string_cursor_next(cursor))
            goto fail;
    }
    char *result = strdup(string_c_str(output));
    string_cursor_free(cursor);
    string_free(output);
    string_free(source);
    return result;
fail:
    string_cursor_free(cursor);
    string_free(output);
    string_free(source);
    return NULL;
}
