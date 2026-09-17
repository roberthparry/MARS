#include <ctype.h>

#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

/* Decimal derivative orders may use Unicode superscripts or a caret, optionally with braces.
   Return 0 for no order, 1 for a positive bounded order, and -1 for malformed or excessive input. */
int de_parse_derivative_order(const char *text, size_t *position, size_t *order)
{
    size_t cursor = *position, value = 0u;
    bool found = false, braced = false;
    while (isspace((unsigned char)text[cursor]))
        ++cursor;
    bool caret = text[cursor] == '^';
    if (caret) {
        ++cursor;
        while (isspace((unsigned char)text[cursor]))
            ++cursor;
        braced = text[cursor] == '{';
        if (braced)
            ++cursor;
        while (isspace((unsigned char)text[cursor]))
            ++cursor;
    }
    while (text[cursor]) {
        unsigned int digit;
        size_t length;
        const unsigned char *p = (const unsigned char *)text + cursor;
        if (caret && p[0] >= '0' && p[0] <= '9') {
            digit = p[0] - '0';
            length = 1u;
        } else if (!caret && p[0] == 0xc2u && (p[1] == 0xb9u || p[1] == 0xb2u || p[1] == 0xb3u)) {
            digit = p[1] == 0xb9u ? 1u : p[1] - 0xb0u;
            length = 2u;
        } else if (!caret && p[0] == 0xe2u && p[1] == 0x81u &&
                   (p[2] == 0xb0u || (p[2] >= 0xb4u && p[2] <= 0xb9u))) {
            digit = p[2] - 0xb0u;
            length = 3u;
        } else {
            break;
        }
        /* Both total and partial derivative normalisers expand at most 128 coordinates. */
        if (value > (128u - digit) / 10u)
            return -1;
        value = value * 10u + digit;
        cursor += length;
        found = true;
    }
    if (braced) {
        while (isspace((unsigned char)text[cursor]))
            ++cursor;
        if (text[cursor++] != '}')
            return -1;
    }
    if (!found)
        return caret ? -1 : 0;
    if (!value)
        return -1;
    *position = cursor;
    *order = value;
    return 1;
}
