/**
 * @file qcomplex_print.c
 * @brief Printf-style complex-double-double output.
 *
 * Formats complex values through the MARS string and output helpers. It preserves real and imaginary components
 * rather than narrowing the result to ordinary complex double.
 * Real components use resolved native formatting options without constructing and reparsing a format string.
 *
 * This implements fixed double-double complex arithmetic for qcomplex_t. It is distinct from configurable
 * multiprecision complex arithmetic in the number module.
 */

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string-t.h"
#include "qcomplex-t.h"
#define MARS_SHARED_QFLOAT_FORMAT_INTERNAL_ACCESS
#include "internal/qfloat_format_internal.h"
#define MARS_SHARED_QCOMPLEX_FORMAT_INTERNAL_ACCESS
#include "internal/qcomplex_format_internal.h"

static string_t *qc_format_qfloat_text(qfloat_t value, char conversion, const qfi_format_options_t *opts, int flag_plus,
                                       int flag_space, int lower_exponent)
{
    qfi_format_options_t real_options = *opts;
    string_t *text;

    real_options.conversion = conversion;
    real_options.flag_plus = flag_plus;
    real_options.flag_space = flag_space;
    text = qfi_format_value_text(value, &real_options);
    if (text && lower_exponent)
        string_replace(text, "E", "e");

    return text;
}

/* Share native complex formatting without reparsing a generated format string. */
string_t *qci_format_value_text(qcomplex_t value, const qfi_format_options_t *opts)
{
    if (!opts || !opts->conversion || !strchr("zZqQfFeEgG", opts->conversion))
        return NULL;
    char qfmt = opts->conversion == 'Z' ? 'Q' : opts->conversion == 'z' ? 'q' : opts->conversion;
    int lower_exponent = opts->conversion == 'Z';
    qfloat_t real = qc_real(value);
    qfloat_t imag = qc_imag(value);
    qfloat_t im_abs = qf_signbit(imag) ? qf_neg(imag) : imag;
    int imag_is_one = qf_cmp(im_abs, QF_ONE) == 0;
    string_t *real_text;
    string_t *imag_text;
    string_t *out;

    real_text = qc_format_qfloat_text(real, qfmt, opts, opts->flag_plus, opts->flag_space, lower_exponent);
    if (!real_text)
        return NULL;

    if (qf_cmp(imag, QF_ZERO) == 0)
        return real_text;

    if (qf_cmp(real, QF_ZERO) == 0 && imag_is_one) {
        string_free(real_text);
        return string_new_with(qf_signbit(imag) ? "-i" : "i");
    }

    imag_text = qc_format_qfloat_text(im_abs, qfmt, opts, 0, 0, lower_exponent);
    if (!imag_text) {
        string_free(real_text);
        return NULL;
    }

    out = string_new();
    if (!out) {
        string_free(real_text);
        string_free(imag_text);
        return NULL;
    }

    if (qf_cmp(real, QF_ZERO) == 0) {
        if (qf_signbit(imag) && string_append_char(out, '-') != 0)
            goto fail;
        if (string_append_string(out, imag_text) != 0 || string_append_char(out, 'i') != 0)
            goto fail;
    } else {
        const char *sep = qf_signbit(imag) ? " - " : " + ";

        if (string_append_string(out, real_text) != 0 || string_append_cstr(out, sep) != 0)
            goto fail;
        if (!imag_is_one && string_append_string(out, imag_text) != 0)
            goto fail;
        if (string_append_char(out, 'i') != 0)
            goto fail;
    }

    string_free(real_text);
    string_free(imag_text);
    return out;

fail:
    string_free(real_text);
    string_free(imag_text);
    string_free(out);
    return NULL;
}

static string_format_result_t qc_format_callback(string_t *out, const string_format_spec_t *spec, va_list ap,
                                                 void *user)
{
    string_t *text;
    qfi_format_options_t opts;

    (void)user;

    if (!out || !spec || !ap)
        return STRING_FORMAT_ERROR;

    if (spec->conversion != 'z' && spec->conversion != 'Z' && spec->conversion != 'q' && spec->conversion != 'Q')
        return STRING_FORMAT_UNHANDLED;

    if (qfi_format_options_from_spec(&opts, spec, ap) != 0)
        return STRING_FORMAT_ERROR;

    if (spec->conversion == 'z' || spec->conversion == 'Z') {
        qcomplex_t value = va_arg(ap, qcomplex_t);

        text = qci_format_value_text(value, &opts);
    } else {
        qfloat_t value = va_arg(ap, qfloat_t);

        text = qc_format_qfloat_text(value, spec->conversion, &opts, opts.flag_plus, opts.flag_space, 0);
    }
    if (!text)
        return STRING_FORMAT_ERROR;

    if (string_append_string(out, text) != 0) {
        string_free(text);
        return STRING_FORMAT_ERROR;
    }

    string_free(text);
    return STRING_FORMAT_HANDLED;
}

static int qc_copy_text_to_buffer(const string_t *text, char *out, size_t out_size)
{
    size_t len;

    if (!text)
        return -1;

    len = string_view_length(string_view_all(text));
    if (out && out_size > 0u) {
        size_t copy_len = len < out_size - 1u ? len : out_size - 1u;

        memcpy(out, string_c_str(text), copy_len);
        out[copy_len] = '\0';
    }

    return len <= (size_t)INT_MAX ? (int)len : -1;
}

string_t *qc_vsprintf_text(const char *fmt, va_list ap)
{
    if (!fmt)
        return NULL;

    return string_vsprintf_with_callback(fmt, ap, qc_format_callback, NULL);
}

int qc_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap)
{
    string_t *text = qc_vsprintf_text(fmt, ap);
    int written = qc_copy_text_to_buffer(text, out, out_size);

    string_free(text);
    return written;
}

int qc_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = qc_vsprintf(out, out_size, fmt, ap);
    va_end(ap);
    return n;
}

string_t *qc_sprintf_text(const char *fmt, ...)
{
    va_list ap;
    string_t *out;

    va_start(ap, fmt);
    out = qc_vsprintf_text(fmt, ap);
    va_end(ap);
    return out;
}

/* Retain the original printf-style complex formatting and stream semantics. */
int qc_printf(const char *fmt, ...)
{
    string_t *text;
    int written;
    va_list ap;

    va_start(ap, fmt);
    text = qc_vsprintf_text(fmt, ap);
    va_end(ap);

    if (!text)
        return -1;

    written = string_fprintf(stdout, "%S", text);
    string_free(text);
    return written;
}

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------ */

/* Faddeeva w(z) via Weideman's rational approximation (N=32 for ~quad precision) */
/* ------------------------------------------------------------ */
