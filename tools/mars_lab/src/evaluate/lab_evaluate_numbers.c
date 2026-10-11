/**
 * @file lab_evaluate_numbers.c
 * @brief Lossless decimal-exponent typography for native Lab card markup.
 *
 * Formats a borrowed numeric token without evaluating it or rounding its digits.
 * Ordinary decimal notation is used through four places before the first digit;
 * smaller magnitudes and impractically large integers use Unicode powers of ten.
 * Executable source, exact worker fields and transport numbers remain unchanged.
 */
#include "string-t.h"
#include "lab_syntax_internal.h"

/* Format a complete lexical number, preserving malformed or non-exponential tokens. */
string_t *lab_pres_number_literal(const string_t *source)
{
    string_view_t view = string_view_all(source);
    size_t length = string_view_length(view), marker = 0u, point = 0u, digits = 0u;
    unsigned char byte = 0u;
    bool dotted = false;
    while (string_view_peek_ascii(view, marker, &byte)) {
        if (byte >= '0' && byte <= '9') {
            ++digits;
            if (!dotted) ++point;
        } else if (byte == '.' && !dotted) {
            unsigned char next = 0u;
            if (string_view_peek_ascii(view, marker + 1u, &next) && next == '.') break;
            dotted = true;
        } else {
            break;
        }
        ++marker;
    }
    bool abbreviated = string_view_equals_literal(string_view_slice(view, marker, 3u), "...");
    if (abbreviated) {
        marker += 3u;
        string_view_peek_ascii(view, marker, &byte);
    }
    if (!digits || marker == length || (byte != 'e' && byte != 'E'))
        return string_clone(source);
    size_t position = marker + 1u;
    bool negative = string_view_peek_ascii(view, position, &byte) && byte == '-';
    if (string_view_peek_ascii(view, position, &byte) && (byte == '+' || byte == '-')) ++position;
    size_t start = position, exponent = 0u;
    while (string_view_peek_ascii(view, position, &byte) && byte >= '0' && byte <= '9') {
        exponent = exponent <= 4096u ? exponent * 10u + (size_t)(byte - '0') : 4097u;
        ++position;
    }
    if (position == start || position != length)
        return string_clone(source);
    string_t *out = string_new();
    bool ok = out != NULL;
    long decimal = (long)point + (negative ? -(long)exponent : (long)exponent);
    if (!abbreviated && exponent <= 4096u && decimal >= -3L && decimal <= 4096L) {
        if (decimal <= 0L) {
            ok = ok && !string_append_cstr(out, "0.");
            for (long i = 0; ok && i < -decimal; ++i) ok = !string_append_char(out, '0');
        }
        size_t written = 0u;
        for (size_t i = 0u; ok && i < marker; ++i) {
            string_view_peek_ascii(view, i, &byte);
            if (byte == '.') continue;
            if (decimal > 0 && written == (size_t)decimal) ok = !string_append_char(out, '.');
            ok = ok && !string_append_char(out, (char)byte);
            ++written;
        }
        while (ok && decimal > 0 && written++ < (size_t)decimal) ok = !string_append_char(out, '0');
    } else {
        static const char *const superscripts[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
        ok = ok && string_append_format(out, "%W×10%s", string_view_slice(view, 0u, marker),
                                         negative ? "⁻" : "") >= 0;
        while (start + 1u < length && string_view_peek_ascii(view, start, &byte) && byte == '0') ++start;
        for (size_t i = start; ok && i < length; ++i) {
            string_view_peek_ascii(view, i, &byte);
            ok = !string_append_cstr(out, superscripts[byte - '0']);
        }
    }
    if (!ok) {
        string_free(out);
        out = NULL;
    }
    return out;
}
