#include "number.h"
#include "number_internal.h"
#include "ustring.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *number_strdup(const char *text)
{
    string_t *wrapped;
    size_t len;
    char *copy;

    if (!text)
        return NULL;
    wrapped = string_new_with(text);
    if (!wrapped)
        return NULL;
    len = string_view_length(string_view_all(wrapped));
    copy = malloc(len + 1u);
    if (!copy) {
        string_free(wrapped);
        return NULL;
    }
    memcpy(copy, string_c_str(wrapped), len + 1u);
    string_free(wrapped);
    return copy;
}

char *number_cstring_from_text(string_t *text)
{
    char *out = text ? number_strdup(string_c_str(text)) : NULL;

    string_free(text);
    return out;
}

string_t *number_format_double_text(const number_t *number, bool scientific, int precision)
{
    char fmt[32];
    double value;

    if (!number)
        return NULL;
    value = number_impl_const(number)->value.d;
    if (precision >= 0)
        snprintf(fmt, sizeof(fmt), scientific ? "%%.%dE" : "%%.%dg", precision);
    else
        snprintf(fmt, sizeof(fmt), scientific ? "%%.16E" : "%%.17g");
    return string_sprintf(fmt, value);
}

string_t *number_format_qfloat_text(const number_t *number, bool scientific, int precision)
{
    char fmt[32];

    if (!number)
        return NULL;
    if (precision >= 0)
        snprintf(fmt, sizeof(fmt), scientific ? "%%.%dQ" : "%%.%dq", precision);
    else
        snprintf(fmt, sizeof(fmt), scientific ? "%%Q" : "%%q");
    return qf_sprintf_text(fmt, number_impl_const(number)->value.qf);
}

string_t *number_format_qcomplex_text(const number_t *number, bool scientific, int precision)
{
    char fmt[32];

    if (!number)
        return NULL;
    if (precision >= 0)
        snprintf(fmt, sizeof(fmt), scientific ? "%%.%dZ" : "%%.%dz", precision);
    else
        snprintf(fmt, sizeof(fmt), scientific ? "%%Z" : "%%z");
    return qc_sprintf_text(fmt, number_impl_const(number)->value.qc);
}

string_t *number_format_mpfr_text(const number_t *number, bool scientific, int precision)
{
    mpfr_srcptr value = number ? number_mpfr_value(number_impl_const(number)->value.mpfr) : NULL;
    int needed;
    char *out;
    char fmt[32];
    string_t *text;

    if (!value)
        return NULL;
    if (precision >= 0)
        snprintf(fmt, sizeof(fmt), scientific ? "%%.%dRE" : "%%.%dRg", precision);
    else
        snprintf(fmt, sizeof(fmt), scientific ? "%%.*RE" : "%%.*Rg");
    needed = precision >= 0
        ? mpfr_snprintf(NULL, 0u, fmt, value)
        : mpfr_snprintf(NULL, 0u, fmt, (int)num_get_prec_digits(*number), value);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    if (precision >= 0)
        mpfr_snprintf(out, (size_t)needed + 1u, fmt, value);
    else
        mpfr_snprintf(out, (size_t)needed + 1u, fmt, (int)num_get_prec_digits(*number), value);
    text = string_new_with(out);
    free(out);
    return text;
}

string_t *number_format_complex_text(const number_t *number, bool scientific, int precision)
{
    const complex_t *value = number ? number_impl_const(number)->value.cx : NULL;
    size_t precision_bits = number ? num_get_prec_bits(*number) : 0u;
    mpc_t tmp;
    mpfr_t imag_abs;
    char *real_buffer = NULL;
    char *imag_buffer = NULL;
    string_t *real = NULL;
    string_t *imag = NULL;
    string_t *out = NULL;
    int real_needed;
    int imag_needed;
    int digits;
    char fmt[32];

    if (!value)
        return NULL;
    if (precision_bits == 0u)
        precision_bits = number_default_precision_bits;
    digits = precision >= 0 ? precision : (int)num_get_prec_digits(*number);
    if (digits <= 0)
        digits = (int)num_get_default_prec_digits();
    snprintf(fmt, sizeof(fmt), scientific ? "%%.%dRE" : "%%.%dRg", digits);
    mpc_init2(tmp, (mpfr_prec_t)precision_bits);
    mpfr_init2(imag_abs, (mpfr_prec_t)precision_bits);
    if (number_complex_get_mpc(tmp, value, precision_bits) != 0) {
        mpfr_clear(imag_abs);
        mpc_clear(tmp);
        return NULL;
    }
    mpfr_abs(imag_abs, mpc_imagref(tmp), MPFR_RNDN);
    real_needed = mpfr_snprintf(NULL, 0u, fmt, mpc_realref(tmp));
    imag_needed = mpfr_snprintf(NULL, 0u, fmt, imag_abs);
    if (real_needed < 0 || imag_needed < 0)
        goto done;
    real_buffer = malloc((size_t)real_needed + 1u);
    imag_buffer = malloc((size_t)imag_needed + 1u);
    if (!real_buffer || !imag_buffer)
        goto done;
    mpfr_snprintf(real_buffer, (size_t)real_needed + 1u, fmt, mpc_realref(tmp));
    mpfr_snprintf(imag_buffer, (size_t)imag_needed + 1u, fmt, imag_abs);
    real = string_new_with(real_buffer);
    imag = string_new_with(imag_buffer);
    if (!real || !imag)
        goto done;
    {
        int imag_sign = mpfr_sgn(mpc_imagref(tmp));
        bool real_zero = mpfr_zero_p(mpc_realref(tmp)) != 0;
        bool imag_is_unit = mpfr_cmpabs_ui(mpc_imagref(tmp), 1u) == 0;
        const char *sep = imag_sign < 0 ? " - " : " + ";

        if (real_zero)
            out = imag_is_unit
                ? string_sprintf("%si", imag_sign < 0 ? "-" : "")
                : string_sprintf("%s%Si", imag_sign < 0 ? "-" : "", imag);
        else
            out = imag_is_unit
                ? string_sprintf("%S%si", real, sep)
                : string_sprintf("%S%s%Si", real, sep, imag);
    }

done:
    string_free(real);
    string_free(imag);
    free(real_buffer);
    free(imag_buffer);
    mpfr_clear(imag_abs);
    mpc_clear(tmp);
    return out;
}

static string_t *number_format_inexact_text(const number_t *number,
                                            bool scientific,
                                            int precision)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    if (vt && vt->format_inexact_text)
        return vt->format_inexact_text(number, scientific, precision);
    return NULL;
}

static bool number_format_text_starts_with_ascii(const string_t *text, char ch)
{
    string_cursor_t *cursor;
    bool found = false;
    unsigned char ascii = 0u;

    if (!text)
        return false;

    cursor = string_cursor_new(text);
    if (!cursor)
        return false;

    found = string_cursor_peek_ascii(cursor, &ascii) &&
            ascii == (unsigned char)ch;
    string_cursor_free(cursor);
    return found;
}

static string_t *number_format_value_text(number_t value,
                                          char spec,
                                          int precision)
{
    if (num_is_exact(value) && !number_vt(&value)->is_complex)
        return num_to_string(value);
    return number_format_inexact_text(&value, spec == 'N', precision);
}

static int number_format_append_padding(string_t *out, int count, char fill)
{
    while (count-- > 0) {
        if (string_append_char(out, fill) != 0)
            return -1;
    }
    return 0;
}

static string_format_result_t number_format_callback(string_t *out,
                                                     const string_format_spec_t *spec,
                                                     va_list ap,
                                                     void *user)
{
    int width;
    int precision;
    bool left;
    bool zero;
    string_t *core = NULL;
    size_t core_len;
    int pad;
    number_t value;

    (void)user;

    if (!out || !spec || !ap)
        return STRING_FORMAT_ERROR;
    if (spec->conversion != 'n' && spec->conversion != 'N')
        return STRING_FORMAT_UNHANDLED;
    if (spec->length[0] != '\0')
        return STRING_FORMAT_ERROR;

    width = spec->width_from_argument ? va_arg(ap, int) : spec->width;
    precision = spec->precision_from_argument ? va_arg(ap, int) : spec->precision;
    left = spec->flag_left;
    zero = spec->flag_zero;
    if (width < 0) {
        left = true;
        width = -width;
    }
    if (precision < 0)
        precision = -1;

    value = va_arg(ap, number_t);
    core = number_format_value_text(value, spec->conversion, precision);
    if (!core)
        return STRING_FORMAT_ERROR;

    if ((spec->flag_sign || spec->flag_space) &&
        !number_format_text_starts_with_ascii(core, '-')) {
        string_t *prefixed = string_sprintf("%c%S",
                                            spec->flag_sign ? '+' : ' ',
                                            core);

        string_free(core);
        core = prefixed;
        if (!core)
            return STRING_FORMAT_ERROR;
    }

    core_len = string_view_length(string_view_all(core));
    pad = width > (int)core_len ? width - (int)core_len : 0;
    if (!left && number_format_append_padding(out, pad, zero ? '0' : ' ') != 0)
        goto fail;
    if (string_append_string(out, core) != 0)
        goto fail;
    if (left && number_format_append_padding(out, pad, ' ') != 0)
        goto fail;

    string_free(core);
    return STRING_FORMAT_HANDLED;

fail:
    string_free(core);
    return STRING_FORMAT_ERROR;
}

string_t *num_vsprintf_text(const char *fmt, va_list ap)
{
    return string_vsprintf_with_callback(fmt,
                                         ap,
                                         number_format_callback,
                                         NULL);
}

int num_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap)
{
    string_t *text;
    size_t len;

    text = num_vsprintf_text(fmt, ap);
    if (!text)
        return -1;

    len = string_view_length(string_view_all(text));
    if (out_size > 0u && out) {
        size_t copy_len = len < out_size - 1u ? len : out_size - 1u;

        memcpy(out, string_c_str(text), copy_len);
        out[copy_len] = '\0';
    }

    string_free(text);
    return len <= (size_t)INT_MAX ? (int)len : -1;
}

int num_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    int n;
    va_list ap;

    va_start(ap, fmt);
    n = num_vsprintf(out, out_size, fmt, ap);
    va_end(ap);
    return n;
}

string_t *num_sprintf_text(const char *fmt, ...)
{
    string_t *text;
    va_list ap;

    va_start(ap, fmt);
    text = num_vsprintf_text(fmt, ap);
    va_end(ap);
    return text;
}

int num_printf(const char *fmt, ...)
{
    int written;
    string_t *text;
    va_list ap;

    va_start(ap, fmt);
    text = num_vsprintf_text(fmt, ap);
    va_end(ap);
    if (!text)
        return -1;

    written = string_printf("%S", text);
    string_free(text);
    return written;
}
