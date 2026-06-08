#include <stdarg.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcomplex.h"
#include "ustring.h"

typedef struct {
    char conversion;
    int flag_plus;
    int flag_space;
    int flag_minus;
    int flag_zero;
    int flag_hash;
    int width;
    int precision;
} qc_format_options_t;

static string_t *qc_build_qfloat_format(char spec,
                                        int width,
                                        int precision,
                                        int flag_plus,
                                        int flag_space,
                                        int flag_minus,
                                        int flag_zero,
                                        int flag_hash)
{
    string_t *fmt = string_new_with("%");

    if (!fmt)
        return NULL;

    if ((flag_plus && string_append_char(fmt, '+') != 0) ||
        (flag_space && string_append_char(fmt, ' ') != 0) ||
        (flag_minus && string_append_char(fmt, '-') != 0) ||
        (flag_zero && string_append_char(fmt, '0') != 0) ||
        (flag_hash && string_append_char(fmt, '#') != 0))
        goto fail;

    if (width > 0 && string_append_format(fmt, "%d", width) < 0)
        goto fail;
    if (precision >= 0) {
        if (string_append_char(fmt, '.') != 0 ||
            string_append_format(fmt, "%d", precision) < 0)
            goto fail;
    }
    if (string_append_char(fmt, spec) != 0)
        goto fail;

    return fmt;

fail:
    string_free(fmt);
    return NULL;
}

static int qc_format_options_from_spec(qc_format_options_t *opts,
                                       const string_format_spec_t *spec,
                                       va_list ap)
{
    if (!opts || !spec || spec->length[0] != '\0')
        return -1;

    opts->conversion = spec->conversion;
    opts->flag_plus = spec->flag_sign;
    opts->flag_space = spec->flag_space;
    opts->flag_minus = spec->flag_left;
    opts->flag_zero = spec->flag_zero;
    opts->flag_hash = spec->flag_alternate;
    opts->width = spec->width_from_argument ? va_arg(ap, int) : spec->width;
    opts->precision = spec->precision_from_argument
        ? va_arg(ap, int)
        : spec->precision;

    if (opts->width < 0) {
        opts->flag_minus = 1;
        opts->width = -opts->width;
    }

    return 0;
}

static string_t *qc_format_qfloat_text(qfloat_t value,
                                       char conversion,
                                       const qc_format_options_t *opts,
                                       int flag_plus,
                                       int flag_space,
                                       int lower_exponent)
{
    string_t *fmt;
    string_t *text;

    fmt = qc_build_qfloat_format(conversion,
                                 opts->width,
                                 opts->precision,
                                 flag_plus,
                                 flag_space,
                                 opts->flag_minus,
                                 opts->flag_zero,
                                 opts->flag_hash);
    if (!fmt)
        return NULL;

    text = qf_sprintf_text(string_c_str(fmt), value);
    string_free(fmt);
    if (text && lower_exponent)
        string_replace(text, "E", "e");

    return text;
}

static string_t *qc_format_qcomplex_text(qcomplex_t value,
                                         const qc_format_options_t *opts)
{
    char qfmt = opts->conversion == 'Z' ? 'Q' : 'q';
    int lower_exponent = opts->conversion == 'Z';
    qfloat_t real = qc_real(value);
    qfloat_t imag = qc_imag(value);
    qfloat_t im_abs = qf_signbit(imag) ? qf_neg(imag) : imag;
    int imag_is_one = qf_cmp(im_abs, QF_ONE) == 0;
    string_t *real_text;
    string_t *imag_text;
    string_t *out;

    real_text = qc_format_qfloat_text(real, qfmt, opts,
                                      opts->flag_plus,
                                      opts->flag_space,
                                      lower_exponent);
    if (!real_text)
        return NULL;

    if (qf_cmp(imag, QF_ZERO) == 0)
        return real_text;

    if (qf_cmp(real, QF_ZERO) == 0 && imag_is_one) {
        string_free(real_text);
        return string_new_with(qf_signbit(imag) ? "-i" : "i");
    }

    imag_text = qc_format_qfloat_text(im_abs, qfmt, opts,
                                      0, 0, lower_exponent);
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
        if (string_append_string(out, imag_text) != 0 ||
            string_append_char(out, 'i') != 0)
            goto fail;
    } else {
        const char *sep = qf_signbit(imag) ? " - " : " + ";

        if (string_append_string(out, real_text) != 0 ||
            string_append_cstr(out, sep) != 0)
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

static string_format_result_t qc_format_callback(string_t *out,
                                                 const string_format_spec_t *spec,
                                                 va_list ap,
                                                 void *user)
{
    string_t *text;
    qc_format_options_t opts;

    (void)user;

    if (!out || !spec || !ap)
        return STRING_FORMAT_ERROR;

    if (spec->conversion != 'z' && spec->conversion != 'Z' &&
        spec->conversion != 'q' && spec->conversion != 'Q')
        return STRING_FORMAT_UNHANDLED;

    if (qc_format_options_from_spec(&opts, spec, ap) != 0)
        return STRING_FORMAT_ERROR;

    if (spec->conversion == 'z' || spec->conversion == 'Z') {
        qcomplex_t value = va_arg(ap, qcomplex_t);

        text = qc_format_qcomplex_text(value, &opts);
    } else {
        qfloat_t value = va_arg(ap, qfloat_t);

        text = qc_format_qfloat_text(value, spec->conversion, &opts,
                                     opts.flag_plus, opts.flag_space, 0);
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

    written = string_printf("%S", text);
    string_free(text);
    return written;
}

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------ */

/* Faddeeva w(z) via Weideman's rational approximation (N=32 for ~quad precision) */
/* ------------------------------------------------------------ */
