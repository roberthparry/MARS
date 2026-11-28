/**
 * @file format_excel_detect.c
 * @brief Classify bounded field spans without temporary strings or repeated whole-string searches.
 *
 * Native option syntax retains precedence over custom numeric/date masks. A byte-property table accumulates
 * custom-code features in one pass; ASCII comparison deliberately ignores the host's locale settings.
 */
#include <string.h>

#include "format-t.h"
#define MARS_FORMAT_INTERNAL_ACCESS
#include "format_excel_internal.h"

enum {
    FX_NUMBER = 1u, FX_FIXED = 2u, FX_DATE = 4u, FX_TRIGGER = 8u, FX_ALIGNMENT = 16u,
    FX_INTEGER_STYLE = 32u, FX_PLACEHOLDER = 64u, FX_HASH = 128u, FX_DATE_INITIAL = 256u,
    FX_LETTER = 512u, FX_UNICODE = 1024u
};

static const unsigned fx_byte_properties[128] = {
    [' ' ] = FX_NUMBER | FX_FIXED | FX_DATE,
    [0x22] = FX_TRIGGER,
    ['#' ] = FX_NUMBER | FX_FIXED | FX_PLACEHOLDER | FX_HASH,
    ['$' ] = FX_TRIGGER,
    ['%' ] = FX_FIXED | FX_TRIGGER,
    ['(' ] = FX_FIXED | FX_TRIGGER,
    [')' ] = FX_FIXED | FX_TRIGGER,
    ['+' ] = FX_NUMBER | FX_FIXED,
    [',' ] = FX_NUMBER | FX_FIXED | FX_TRIGGER,
    ['-' ] = FX_NUMBER | FX_FIXED | FX_DATE,
    ['.' ] = FX_NUMBER | FX_FIXED | FX_DATE,
    ['/' ] = FX_DATE | FX_TRIGGER,
    ['0' ] = FX_NUMBER | FX_FIXED | FX_DATE | FX_PLACEHOLDER,
    [':' ] = FX_DATE | FX_TRIGGER,
    [';' ] = FX_TRIGGER,
    ['<' ] = FX_ALIGNMENT,
    ['=' ] = FX_ALIGNMENT,
    ['>' ] = FX_ALIGNMENT,
    ['?' ] = FX_NUMBER | FX_FIXED | FX_TRIGGER | FX_PLACEHOLDER,
    ['@' ] = FX_TRIGGER,
    ['A' ] = FX_LETTER,
    ['B' ] = FX_LETTER,
    ['C' ] = FX_LETTER,
    ['D' ] = FX_DATE | FX_DATE_INITIAL | FX_LETTER,
    ['E' ] = FX_NUMBER | FX_INTEGER_STYLE | FX_LETTER,
    ['F' ] = FX_INTEGER_STYLE | FX_LETTER,
    ['G' ] = FX_INTEGER_STYLE | FX_LETTER,
    ['H' ] = FX_DATE | FX_DATE_INITIAL | FX_LETTER,
    ['I' ] = FX_LETTER,
    ['J' ] = FX_LETTER,
    ['K' ] = FX_LETTER,
    ['L' ] = FX_LETTER,
    ['M' ] = FX_DATE | FX_DATE_INITIAL | FX_LETTER,
    ['N' ] = FX_LETTER,
    ['O' ] = FX_DATE | FX_LETTER,
    ['P' ] = FX_LETTER,
    ['Q' ] = FX_INTEGER_STYLE | FX_LETTER,
    ['R' ] = FX_LETTER,
    ['S' ] = FX_DATE | FX_DATE_INITIAL | FX_LETTER,
    ['T' ] = FX_LETTER,
    ['U' ] = FX_LETTER,
    ['V' ] = FX_LETTER,
    ['W' ] = FX_LETTER,
    ['X' ] = FX_INTEGER_STYLE | FX_LETTER,
    ['Y' ] = FX_DATE | FX_DATE_INITIAL | FX_LETTER,
    ['Z' ] = FX_LETTER,
    ['[' ] = FX_TRIGGER,
    [0x5c] = FX_TRIGGER,
    ['^' ] = FX_DATE | FX_ALIGNMENT,
    ['a' ] = FX_LETTER,
    ['b' ] = FX_INTEGER_STYLE | FX_LETTER,
    ['c' ] = FX_INTEGER_STYLE | FX_LETTER,
    ['d' ] = FX_DATE | FX_INTEGER_STYLE | FX_DATE_INITIAL | FX_LETTER,
    ['e' ] = FX_NUMBER | FX_INTEGER_STYLE | FX_LETTER,
    ['f' ] = FX_INTEGER_STYLE | FX_LETTER,
    ['g' ] = FX_INTEGER_STYLE | FX_LETTER,
    ['h' ] = FX_DATE | FX_DATE_INITIAL | FX_LETTER,
    ['i' ] = FX_LETTER,
    ['j' ] = FX_LETTER,
    ['k' ] = FX_LETTER,
    ['l' ] = FX_LETTER,
    ['m' ] = FX_DATE | FX_DATE_INITIAL | FX_LETTER,
    ['n' ] = FX_INTEGER_STYLE | FX_LETTER,
    ['o' ] = FX_DATE | FX_INTEGER_STYLE | FX_LETTER,
    ['p' ] = FX_INTEGER_STYLE | FX_LETTER,
    ['q' ] = FX_INTEGER_STYLE | FX_LETTER,
    ['r' ] = FX_LETTER,
    ['s' ] = FX_DATE | FX_INTEGER_STYLE | FX_DATE_INITIAL | FX_LETTER,
    ['t' ] = FX_LETTER,
    ['u' ] = FX_LETTER,
    ['v' ] = FX_LETTER,
    ['w' ] = FX_LETTER,
    ['x' ] = FX_INTEGER_STYLE | FX_LETTER,
    ['y' ] = FX_DATE | FX_DATE_INITIAL | FX_LETTER,
    ['z' ] = FX_LETTER,
};

int fmt_excel_ascii_compare(const char *a, const char *b, size_t length)
{
    for (size_t i = 0u; i < length; ++i) {
        unsigned char left = fmt_excel_lower((unsigned char)a[i]), right = fmt_excel_lower((unsigned char)b[i]);
        if (left != right)
            return (int)left - (int)right;
        if (!left)
            return 0;
    }
    return 0;
}

static bool fx_native_options(const char *code, size_t length)
{
    const char *at = code, *end = code + length;
    bool aligned = false;
    if (*at == '[' || *at == '"' || *at == '\\')
        return false;
    if (length > 1u && strchr("<>=^", at[1])) {
        if ((unsigned char)at[0] < 32u || (unsigned char)at[0] > 126u)
            return false;
        at += 2u;
        aligned = true;
    } else if (strchr("<>=^", *at)) {
        ++at;
        aligned = true;
    }
    if (at < end && (*at == '+' || *at == '-' || *at == ' '))
        ++at;
    if (at < end && *at == '#')
        ++at;
    if (at < end && *at == '0')
        ++at;
    while (at < end && *at >= '0' && *at <= '9')
        ++at;
    if (at < end && *at == '.') {
        ++at;
        if (at == end || *at < '0' || *at > '9')
            return false;
        while (at < end && *at >= '0' && *at <= '9')
            ++at;
    }
    const char *word = at;
    while (at < end && ((*at >= 'a' && *at <= 'z') || (*at >= 'A' && *at <= 'Z') || *at == '_'))
        ++at;
    if (at != end)
        return false;
    return (word != code && at != word) || (aligned && at == word) || (code[0] == '.' && at == word);
}

bool fmt_excel_detect(const char *code, size_t length)
{
    if (!code || !length)
        return false;
    if (length == 7u && !fmt_excel_ascii_compare(code, "General", 7u))
        return true;
    if (length >= 6u && !fmt_excel_ascii_compare(code, "excel:", 6u))
        return true;
    if (fx_native_options(code, length))
        return false;

    unsigned allowed = FX_NUMBER | FX_FIXED | FX_DATE, seen = 0u;
    for (size_t i = 0u; i < length; ++i) {
        unsigned byte = (unsigned char)code[i];
        unsigned properties = byte < 128u ? fx_byte_properties[byte] : FX_UNICODE;
        allowed &= properties;
        seen |= properties;
    }
    unsigned first = (unsigned char)code[0];
    unsigned initial = first < 128u ? fx_byte_properties[first] : 0u;
    if ((seen & FX_UNICODE) && (!(seen & FX_ALIGNMENT) || (initial & FX_DATE_INITIAL)))
        return true;
    if (seen & FX_TRIGGER)
        return true;
    if ((seen & FX_HASH) && !(seen & FX_INTEGER_STYLE))
        return true;
    if (first == '*' && length > 1u && !strchr("<>=^", code[1]))
        return true;
    unsigned last = (unsigned char)code[length - 1u];
    bool native_precision = first == '.' && last < 128u && (fx_byte_properties[last] & FX_LETTER);
    bool number = (allowed & FX_NUMBER) && (seen & FX_PLACEHOLDER) && !native_precision;
    bool fixed = (allowed & FX_FIXED) && (seen & FX_PLACEHOLDER);
    bool date = (allowed & FX_DATE) && length > 1u;
    return number || fixed || date;
}
