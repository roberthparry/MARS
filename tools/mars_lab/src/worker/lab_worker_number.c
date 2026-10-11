/**
 * @file lab_worker_number.c
 * @brief Human-readable numerical values for native Lab result cards.
 *
 * Keeps significant digits supplied by the number module, uses ordinary decimal
 * notation where practical, and writes small magnitudes with Unicode powers of
 * ten. Real and imaginary components are formatted independently; no nonzero
 * component is discarded. This presentation helper does not alter stored values
 * or the machine-readable number and Function output formats.
 */

#include "number.h"
#include "ustring.h"
#include "lab_worker_internal.h"

static string_t *lab_worker_number_real(number_t value, int precision)
{
    string_t *raw = num_sprintf_text("%.*n", precision, value);
    string_view_t view = string_view_all(raw);
    size_t length = string_view_length(view), marker = 0u;
    unsigned char byte = 0u;
    while (marker < length && string_view_peek_ascii(view, marker, &byte) && byte != 'e' && byte != 'E')
        ++marker;
    if (!raw || marker == length)
        return raw;

    size_t position = marker + 1u;
    bool negative = string_view_peek_ascii(view, position, &byte) && byte == '-';
    if (string_view_peek_ascii(view, position, &byte) && (byte == '+' || byte == '-'))
        ++position;
    size_t exponent_start = position, exponent = 0u;
    while (string_view_peek_ascii(view, position, &byte) && byte >= '0' && byte <= '9') {
        /* Saturate: never expand unbounded decimal output for enormous exponents. */
        exponent = exponent <= 4096u ? exponent * 10u + (size_t)(byte - '0') : 4097u;
        ++position;
    }
    if (position != length || position == exponent_start)
        return raw;

    string_t *out = string_new();
    bool ok = out != NULL;
    if (!negative && exponent <= 4096u) {
        size_t digits = 0u;
        for (size_t i = 0u; ok && i < marker; ++i) {
            string_view_peek_ascii(view, i, &byte);
            if (byte == '.')
                continue;
            ok = string_append_char(out, (char)byte) == 0;
            digits += byte >= '0' && byte <= '9';
        }
        while (ok && digits++ < exponent + 1u)
            ok = string_append_char(out, '0') == 0;
    } else {
        static const char *const superscripts[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
        string_view_t mantissa = string_view_slice(view, 0u, marker);
        ok = ok && string_append_format(out, "%W×10%s", mantissa, negative ? "⁻" : "") >= 0;
        while (exponent_start + 1u < length &&
               string_view_peek_ascii(view, exponent_start, &byte) && byte == '0')
            ++exponent_start;
        for (size_t i = exponent_start; ok && i < length; ++i) {
            string_view_peek_ascii(view, i, &byte);
            ok = string_append_cstr(out, superscripts[byte - '0']) == 0;
        }
    }
    string_free(raw);
    if (!ok) {
        string_free(out);
        out = NULL;
    }
    return out;
}

/* Format borrowed values without changing their precision, exactness or components. */
string_t *lab_worker_number_text(number_t value, int precision)
{
    if (precision <= 0)
        precision = (int)num_get_default_prec_digits();
    if (num_is_real(value) || !num_is_finite(value))
        return lab_worker_number_real(value, precision);

    number_t real = num_real_part(value), imaginary = num_imag_part(value);
    number_t magnitude = num_abs(imaginary);
    /* Match number's complex formatting: render components at the requested working precision. */
    number_t zero = num_new(), real_decimal = num_add(real, zero), imag_decimal = num_add(magnitude, zero);
    string_t *a = num_is_zero(real) ? string_new() : lab_worker_number_real(real_decimal, precision);
    string_t *b = num_is_one(magnitude) ? string_new() : lab_worker_number_real(imag_decimal, precision);
    bool negative = num_sign(imaginary) < 0;
    const char *separator = num_is_zero(real) ? (negative ? "-" : "") : (negative ? " - " : " + ");
    string_t *out = a && b ? string_sprintf("%S%s%Si", a, separator, b) : NULL;
    string_free(b);
    string_free(a);
    num_destroy(&imag_decimal);
    num_destroy(&real_decimal);
    num_destroy(&zero);
    num_destroy(&magnitude);
    num_destroy(&imaginary);
    num_destroy(&real);
    return out;
}
