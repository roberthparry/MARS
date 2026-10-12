/**
 * @file quantity_text.c
 * @brief Quantity text parsing and native scalar/unit presentation.
 *
 * Bracketed units delimit numerical text unambiguously, including complex
 * scalars. TeX escapes unit spellings rather than accepting markup from input.
 */
#include <ctype.h>
#include <errno.h>
#include <string.h>

#define MARS_QUANTITY_INTERNAL_ACCESS
#include "quantity_internal.h"

/* Ordinary temperatures are readings; difference(...) explicitly requests a temperature interval. */
quantity_t *quantity_from_string(const string_t *text)
{
    if (!text || !string_is_valid_utf8(text))
        return quantity_fail(EINVAL);
    size_t length = string_byte_length(text);
    if (length > 65536)
        return quantity_fail(EOVERFLOW);
    const char *start = string_c_str(text), *end = start + length;
    if (memchr(start, '\0', length))
        return quantity_fail(EINVAL);
    while (start < end && isspace((unsigned char)*start))
        ++start;
    while (end > start && isspace((unsigned char)end[-1]))
        --end;
    bool difference = end - start >= 10 && !memcmp(start, "difference", 10);
    if (difference) {
        start += 10;
        while (start < end && isspace((unsigned char)*start))
            ++start;
        if (start == end || *start++ != '(' || start == end || end[-1] != ')')
            return quantity_fail(EINVAL);
        --end;
        while (start < end && isspace((unsigned char)*start))
            ++start;
        while (end > start && isspace((unsigned char)end[-1]))
            --end;
    }
    const char *open = memchr(start, '[', (size_t)(end - start));
    if (!open || end[-1] != ']' || open == start)
        return quantity_fail(EINVAL);
    string_t *scalar = string_new(), *unit = string_new();
    if (!scalar || !unit || string_append_chars(scalar, start, (size_t)(open - start)) < 0 ||
        string_append_chars(unit, open + 1, (size_t)(end - open - 2)) < 0) {
        string_free(scalar);
        string_free(unit);
        return quantity_fail(ENOMEM);
    }
    number_t value = num_create_from_text(scalar);
    quantity_t *result = NULL;
    if (num_is_finite(value))
        result = difference ? quantity_new_difference(value, unit) : quantity_new(value, unit);
    else
        errno = EINVAL;
    num_destroy(&value);
    string_free(scalar);
    string_free(unit);
    return result;
}

static bool quantity_append_unit_TeX(string_t *out, const string_t *unit)
{
    static const char *const operators[128] = {
        ['*'] = "\\,",
        ['/'] = "/",
        ['('] = "\\left(",
        [')'] = "\\right)",
    };
    const char *text = string_c_str(unit);
    for (size_t i = 0, length = string_byte_length(unit); i < length;) {
        unsigned char byte = (unsigned char)text[i];
        if (byte < 128 && operators[byte]) {
            if (string_append_cstr(out, operators[byte]) < 0)
                return false;
            ++i;
        } else if (byte == '^') {
            if (string_append_cstr(out, "^{") < 0)
                return false;
            ++i;
            bool fraction = text[i] == '(';
            if (fraction)
                ++i;
            while (i < length && (isdigit((unsigned char)text[i]) || text[i] == '-' || text[i] == '+' ||
                                   (fraction && text[i] == '/'))) {
                if (string_append_chars(out, text + i++, 1) < 0)
                    return false;
            }
            if (fraction)
                ++i;
            if (string_append_cstr(out, "}") < 0)
                return false;
        } else {
            if (string_append_cstr(out, "\\mathrm{") < 0)
                return false;
            do {
                if (strchr("_%#$&{}\\", text[i]) && string_append_cstr(out, "\\") < 0)
                    return false;
                if (string_append_chars(out, text + i++, 1) < 0)
                    return false;
            } while (i < length && !strchr("*/^()", text[i]));
            if (string_append_cstr(out, "}") < 0)
                return false;
        }
    }
    return true;
}

/* Formatting is native; FUNCTION text is accepted by quantity_from_string(). */
string_t *quantity_to_string(const quantity_t *quantity, style_t style)
{
    if (!quantity || (style != style_FUNCTION && style != style_EXPRESSION &&
                      style != style_LATEX && style != style_UNBOUND))
        return quantity_fail(EINVAL);
    string_t *scalar = NULL;
    number_t value = quantity_value(quantity);
    if (style == style_FUNCTION)
        scalar = num_to_string(value);
    else {
        expr_t *expression = expr_new_const(value);
        scalar = expression ? expr_to_text(expression, style == style_LATEX ? style_LATEX : style_EXPRESSION) : NULL;
        expr_free(expression);
    }
    num_destroy(&value);
    if (!scalar)
        return NULL;
    string_t *out = NULL;
    bool difference = !quantity->point && quantity_unit_is_temperature(&quantity->unit);
    if (style == style_LATEX) {
        out = string_sprintf("%s%S\\,", difference ? "\\operatorname{difference}\\left(" : "", scalar);
        if (out && (!quantity_append_unit_TeX(out, quantity->unit.name) ||
                    (difference && string_append_cstr(out, "\\right)") < 0))) {
            string_free(out);
            out = NULL;
        }
    } else
        out = string_sprintf("%s%S [%S]%s", difference ? "difference(" : "", scalar,
                            quantity->unit.name, difference ? ")" : "");
    string_free(scalar);
    return out;
}
