#define MARS_QFLOAT_INTERNAL_ACCESS
#include "qfloat_internal.h"
#include "ustring.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int qf_append_repeated_char(string_t *out, char ch, int count)
{
    if (!out || count < 0)
        return -1;

    for (int i = 0; i < count; i++) {
        if (string_append_char(out, ch) != 0)
            return -1;
    }

    return 0;
}

static string_t *qf_pad_text(const string_t *text, int width, int flag_minus, int flag_zero, int sign_aware_zero)
{
    string_t *out;
    size_t len;
    int pad;

    if (!text)
        return NULL;

    len = string_length(text);
    pad = (width > (int)len) ? (width - (int)len) : 0;
    if (pad < 0)
        pad = 0;

    out = string_new();
    if (!out)
        return NULL;

    if (flag_minus) {
        if (string_append_string(out, text) != 0 || qf_append_repeated_char(out, ' ', pad) != 0) {
            string_free(out);
            return NULL;
        }
        return out;
    }

    if (flag_zero) {
        rune_t first = string_at(text, 0);

        if (sign_aware_zero && (rune_is_equal(first, '+') || rune_is_equal(first, '-') || rune_is_equal(first, ' '))) {
            string_t *rest = string_substring(text, 1, len > 0u ? len - 1u : 0u);

            if (!rest || string_append_rune(out, first) != 0 || qf_append_repeated_char(out, '0', pad) != 0 ||
                string_append_string(out, rest) != 0) {
                string_free(rest);
                string_free(out);
                return NULL;
            }

            string_free(rest);
            return out;
        }

        if (qf_append_repeated_char(out, '0', pad) != 0 || string_append_string(out, text) != 0) {
            string_free(out);
            return NULL;
        }
        return out;
    }

    if (qf_append_repeated_char(out, ' ', pad) != 0 || string_append_string(out, text) != 0) {
        string_free(out);
        return NULL;
    }

    return out;
}

static string_t *qf_text_with_sign_prefix(const string_t *text, int flag_plus, int flag_space)
{
    string_t *out;

    if (!text)
        return NULL;

    if (rune_is_equal(string_at(text, 0), '-'))
        return string_clone(text);

    out = string_new();
    if (!out)
        return NULL;

    if (flag_plus) {
        if (string_append_char(out, '+') != 0)
            goto fail;
    } else if (flag_space) {
        if (string_append_char(out, ' ') != 0)
            goto fail;
    }

    if (string_append_string(out, text) != 0)
        goto fail;

    return out;

fail:
    string_free(out);
    return NULL;
}

static string_t *qf_signed_padded_text(const string_t *text, int flag_plus, int flag_space, int width, int flag_minus,
                                       int flag_zero, int sign_aware_zero)
{
    string_t *signed_text;
    string_t *padded;

    signed_text = qf_text_with_sign_prefix(text, flag_plus, flag_space);
    if (!signed_text)
        return NULL;

    padded = qf_pad_text(signed_text, width, flag_minus, flag_zero, sign_aware_zero);
    string_free(signed_text);
    return padded;
}

static string_offset_t qf_exponent_marker_pos(const string_t *text)
{
    string_offset_t pos;

    if (!text)
        return -1;

    pos = string_find(text, "e");
    return pos >= 0 ? pos : string_find(text, "E");
}

static int qf_parse_exp10_text(const string_t *text, string_offset_t marker_pos, int *exp10_out)
{
    size_t len;
    string_t *exp_text;

    if (!text || marker_pos < 0 || !exp10_out)
        return -1;

    len = string_view_length(string_view_all(text));
    if ((size_t)marker_pos + 1u > len)
        return -1;

    exp_text = string_substr(text, (size_t)marker_pos + 1u, len - (size_t)marker_pos - 1u);
    if (!exp_text)
        return -1;

    *exp10_out = atoi(string_c_str(exp_text));
    string_free(exp_text);
    return 0;
}

static string_t *qf_trim_scientific_mantissa_text(const string_t *mantissa)
{
    size_t len;

    if (!mantissa)
        return NULL;

    len = string_length(mantissa);
    while (len > 0u && rune_is_equal(string_at(mantissa, len - 1u), '0'))
        len--;
    if (len > 0u && rune_is_equal(string_at(mantissa, len - 1u), '.'))
        len--;
    if (len == 0u)
        return string_new_with("0");
    return string_substring(mantissa, 0u, len);
}

static string_t *qf_normalised_scientific_text(const string_t *text, string_offset_t marker_pos)
{
    string_t *mantissa = NULL;
    string_t *trimmed = NULL;
    string_t *exponent = NULL;
    string_t *out = NULL;
    size_t len;

    if (!text)
        return NULL;
    if (marker_pos < 0)
        return string_clone(text);

    len = string_view_length(string_view_all(text));
    mantissa = string_substr(text, 0u, (size_t)marker_pos);
    exponent = string_substr(text, (size_t)marker_pos, len - (size_t)marker_pos);
    if (!mantissa || !exponent)
        goto done;

    if (string_replace(exponent, "E", "e") != 0)
        goto done;

    trimmed = qf_trim_scientific_mantissa_text(mantissa);
    if (trimmed)
        out = string_sprintf("%S%S", trimmed, exponent);

done:
    string_free(mantissa);
    string_free(trimmed);
    string_free(exponent);
    return out;
}

static string_t *qf_unsigned_mantissa_text(const string_t *mantissa, int *negative_out)
{
    size_t len;

    if (negative_out)
        *negative_out = 0;
    if (!mantissa)
        return NULL;

    len = string_length(mantissa);
    if (len > 0u && rune_is_equal(string_at(mantissa, 0u), '-')) {
        if (negative_out)
            *negative_out = 1;
        return string_substring(mantissa, 1u, len - 1u);
    }

    return string_clone(mantissa);
}

static int qf_digit_text_length(const string_t *digits)
{
    size_t len;

    if (!digits)
        return 0;
    len = string_length(digits);
    return len <= (size_t)INT_MAX ? (int)len : INT_MAX;
}

static char qf_digit_text_at(const string_t *digits, int index)
{
    size_t len;
    char ch = '0';

    if (!digits || index < 0)
        return '0';
    len = string_length(digits);
    if ((size_t)index >= len)
        return '0';
    return rune_to_ascii(string_at(digits, (size_t)index), &ch) && ch >= '0' && ch <= '9' ? ch : '0';
}

static bool qf_digit_text_replace_at(string_t **digits_ptr, int index, char digit)
{
    string_t *digits;
    string_t *prefix;
    string_t *suffix;
    string_t *next;
    size_t len;
    bool ok;

    if (!digits_ptr || !*digits_ptr || index < 0 || digit < '0' || digit > '9')
        return false;

    digits = *digits_ptr;
    len = string_length(digits);
    if ((size_t)index >= len)
        return false;

    prefix = string_substring(digits, 0u, (size_t)index);
    suffix = string_substring(digits, (size_t)index + 1u, len - (size_t)index - 1u);
    next = string_new();
    ok = prefix && suffix && next && string_append_string(next, prefix) == 0 && string_append_char(next, digit) == 0 &&
         string_append_string(next, suffix) == 0;

    string_free(prefix);
    string_free(suffix);
    if (!ok) {
        string_free(next);
        return false;
    }

    string_free(*digits_ptr);
    *digits_ptr = next;
    return true;
}

static bool qf_digit_text_prepend(string_t **digits_ptr, char digit)
{
    string_t *next;

    if (!digits_ptr || !*digits_ptr || digit < '0' || digit > '9')
        return false;

    next = string_new();
    if (!next)
        return false;
    if (string_append_char(next, digit) != 0 || string_append_string(next, *digits_ptr) != 0) {
        string_free(next);
        return false;
    }

    string_free(*digits_ptr);
    *digits_ptr = next;
    return true;
}

static bool qf_digit_text_truncate(string_t **digits_ptr, int keep_digits)
{
    string_t *next;
    size_t len;

    if (!digits_ptr || !*digits_ptr)
        return false;
    if (keep_digits < 0)
        keep_digits = 0;

    len = string_length(*digits_ptr);
    if ((size_t)keep_digits >= len)
        return true;

    next = string_substring(*digits_ptr, 0u, (size_t)keep_digits);
    if (!next)
        return false;

    string_free(*digits_ptr);
    *digits_ptr = next;
    return true;
}

static bool qf_digit_text_pad_zeros(string_t *digits, int target_len)
{
    if (!digits || target_len < 0)
        return false;

    while (qf_digit_text_length(digits) < target_len) {
        if (string_append_char(digits, '0') != 0)
            return false;
    }
    return true;
}

static bool qf_digit_text_round_carry(string_t **digits_ptr, int *fixed_dp, int round_index)
{
    int i;

    if (!digits_ptr || !*digits_ptr || !fixed_dp)
        return false;

    i = round_index - 1;

    while (i >= 0 && qf_digit_text_at(*digits_ptr, i) == '9') {
        if (!qf_digit_text_replace_at(digits_ptr, i, '0'))
            return false;
        i--;
    }
    if (i >= 0) {
        char digit = qf_digit_text_at(*digits_ptr, i);

        return qf_digit_text_replace_at(digits_ptr, i, (char)(digit + 1));
    } else {
        if (!qf_digit_text_prepend(digits_ptr, '1'))
            return false;
        (*fixed_dp)++;
    }

    return true;
}

static string_t *qf_finalise_fixed_q_text(string_t *text)
{
    size_t len;

    if (!text)
        return NULL;

    len = string_length(text);
    if (len >= 2u && rune_is_equal(string_at(text, 0u), '0') && rune_is_digit(string_at(text, 1u))) {
        string_t *trimmed = string_substring(text, 1u, len - 1u);

        string_free(text);
        return trimmed;
    }

    if (len == 0u && string_append_char(text, '0') != 0) {
        string_free(text);
        return NULL;
    }

    return text;
}

typedef struct {
    char conversion;
    int flag_plus;
    int flag_space;
    int flag_minus;
    int flag_zero;
    int flag_hash;
    int width;
    int precision;
} qf_format_options_t;

static int qf_format_options_from_spec(qf_format_options_t *opts, const string_format_spec_t *spec, va_list ap)
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
    opts->precision = spec->precision_from_argument ? va_arg(ap, int) : spec->precision;

    if (opts->width < 0) {
        opts->flag_minus = 1;
        opts->width = -opts->width;
    }
    return 0;
}

static string_t *qf_format_q_value_text(qfloat_t x, const qf_format_options_t *opts)
{
    string_t *sci_text = NULL;
    string_t *mantissa = NULL;
    string_t *unsigned_mantissa = NULL;
    string_t *intpart = NULL;
    string_t *fracpart = NULL;
    string_t *digits_text = NULL;
    string_t *out = NULL;
    string_t *padded = NULL;
    string_offset_t marker_pos;
    int exp10 = 0;
    int negative = 0;
    int nd;
    int fixed_dp;
    string_offset_t dot_pos;

    sci_text = qf_to_string(x);
    if (!sci_text)
        return NULL;

    marker_pos = qf_exponent_marker_pos(sci_text);
    if (marker_pos >= 0 && qf_parse_exp10_text(sci_text, marker_pos, &exp10) != 0)
        goto done;

    if (marker_pos < 0 || exp10 < -6 || exp10 > 32) {
        string_t *core = qf_normalised_scientific_text(sci_text, marker_pos);

        if (core)
            padded = qf_signed_padded_text(core, opts->flag_plus, opts->flag_space, opts->width, opts->flag_minus,
                                           opts->flag_zero, 0);
        string_free(core);
        goto done;
    }

    mantissa = string_substr(sci_text, 0u, (size_t)marker_pos);
    unsigned_mantissa = qf_unsigned_mantissa_text(mantissa, &negative);
    if (!mantissa || !unsigned_mantissa)
        goto done;

    dot_pos = string_find(unsigned_mantissa, ".");
    if (dot_pos >= 0) {
        size_t mantissa_len = string_view_length(string_view_all(unsigned_mantissa));

        intpart = string_substr(unsigned_mantissa, 0u, (size_t)dot_pos);
        fracpart = string_substr(unsigned_mantissa, (size_t)dot_pos + 1u, mantissa_len - (size_t)dot_pos - 1u);
    } else {
        intpart = string_clone(unsigned_mantissa);
        fracpart = string_new();
    }
    if (!intpart || !fracpart)
        goto done;

    digits_text = string_sprintf("%S%S", intpart, fracpart);
    if (!digits_text)
        goto done;
    fixed_dp = (int)string_length(intpart) + exp10;
    nd = qf_digit_text_length(digits_text);

    if (nd > 32) {
        int trail = 0;
        int tail_nonzero = 0;

        for (int i = 31; i >= 0 && qf_digit_text_at(digits_text, i) == '0'; i--)
            trail++;
        for (int i = 32; i < nd; i++) {
            if (qf_digit_text_at(digits_text, i) != '0') {
                tail_nonzero = 1;
                break;
            }
        }

        if (trail >= 5 && tail_nonzero) {
            if (qf_digit_text_at(digits_text, 32) >= '5' && !qf_digit_text_round_carry(&digits_text, &fixed_dp, 32))
                goto done;
            if (!qf_digit_text_truncate(&digits_text, 32))
                goto done;
            nd = qf_digit_text_length(digits_text);
        }
    }

    if (opts->precision >= 0) {
        int K = fixed_dp + opts->precision;

        if (K + 1 > nd) {
            int pad = K + 1 - nd;

            if (nd + pad > 255)
                pad = 255 - nd;
            if (pad > 0 && !qf_digit_text_pad_zeros(digits_text, nd + pad))
                goto done;
            nd = qf_digit_text_length(digits_text);
        }

        if (K >= 0 && K < nd && qf_digit_text_at(digits_text, K) >= '5' &&
            !qf_digit_text_round_carry(&digits_text, &fixed_dp, K))
            goto done;

        if (K < 0)
            K = 0;
        if (K < nd && !qf_digit_text_truncate(&digits_text, K))
            goto done;
        nd = qf_digit_text_length(digits_text);
    }

    out = string_new();
    if (!out)
        goto done;

    if ((negative && string_append_char(out, '-') != 0) ||
        (!negative && opts->flag_plus && string_append_char(out, '+') != 0) ||
        (!negative && !opts->flag_plus && opts->flag_space && string_append_char(out, ' ') != 0))
        goto done;

    if (fixed_dp <= 0) {
        if (string_append_char(out, '0') != 0)
            goto done;
    } else {
        for (int i = 0; i < fixed_dp; i++) {
            if (string_append_char(out, qf_digit_text_at(digits_text, i)) != 0)
                goto done;
        }
    }

    {
        int frac_digits = opts->precision < 0 ? nd - fixed_dp : opts->precision;

        if (frac_digits < 0)
            frac_digits = 0;

        if (opts->precision < 0) {
            int end = fixed_dp + frac_digits;

            while (end > fixed_dp && qf_digit_text_at(digits_text, end - 1) == '0')
                end--;
            frac_digits = end - fixed_dp;
            if (frac_digits < 0)
                frac_digits = 0;
        }

        if (frac_digits > 0 || opts->flag_hash) {
            if (string_append_char(out, '.') != 0)
                goto done;
            for (int i = 0; i < frac_digits; i++) {
                if (string_append_char(out, qf_digit_text_at(digits_text, fixed_dp + i)) != 0)
                    goto done;
            }
        }
    }

    out = qf_finalise_fixed_q_text(out);
    if (!out)
        goto done;

    padded = qf_pad_text(out, opts->width, opts->flag_minus, opts->flag_zero, 0);

done:
    string_free(sci_text);
    string_free(mantissa);
    string_free(unsigned_mantissa);
    string_free(intpart);
    string_free(fracpart);
    string_free(digits_text);
    string_free(out);
    return padded;
}
static string_t *qf_format_Q_value_text(qfloat_t x, const qf_format_options_t *opts)
{
    string_t *digits_text = NULL;
    string_t *out = NULL;
    int negative;
    int exp10 = 0;
    int precision;
    int keep_digits;
    int extracted_digits;

    if (opts->precision < 0 || isnan(x.hi) || isnan(x.lo) || isinf(x.hi)) {
        string_t *core_text;
        string_t *upper_text;

        core_text = qf_to_string(x);
        if (!core_text)
            return NULL;
        if (string_replace(core_text, "e", "E") != 0) {
            string_free(core_text);
            return NULL;
        }
        upper_text = qf_signed_padded_text(core_text, opts->flag_plus, opts->flag_space, opts->width, opts->flag_minus,
                                           opts->flag_zero, 0);
        string_free(core_text);
        return upper_text;
    }

    negative = (x.hi < 0.0 || (x.hi == 0.0 && x.lo < 0.0));
    if (negative) {
        x.hi = -x.hi;
        x.lo = -x.lo;
    }

    precision = opts->precision;
    keep_digits = precision + 1;
    if (keep_digits < 1)
        keep_digits = 1;

    extracted_digits = keep_digits + 1;
    if (extracted_digits < 34)
        extracted_digits = 34;
    if (extracted_digits > 127)
        extracted_digits = 127;

    digits_text = qf_decimal_digits_text(x, extracted_digits, &exp10);
    if (!digits_text)
        return NULL;

    if (keep_digits < extracted_digits && qf_digit_text_at(digits_text, keep_digits) >= '5') {
        int i = keep_digits - 1;

        while (i >= 0 && qf_digit_text_at(digits_text, i) == '9') {
            if (!qf_digit_text_replace_at(&digits_text, i, '0'))
                goto fail;
            i--;
        }
        if (i >= 0) {
            char digit = qf_digit_text_at(digits_text, i);

            if (!qf_digit_text_replace_at(&digits_text, i, (char)(digit + 1)))
                goto fail;
        } else {
            if (!qf_digit_text_replace_at(&digits_text, 0, '1'))
                goto fail;
            for (i = 1; i < keep_digits; i++) {
                if (!qf_digit_text_replace_at(&digits_text, i, '0'))
                    goto fail;
            }
            exp10++;
        }
    }

    if (negative)
        out = string_new_with("-");
    else if (opts->flag_plus)
        out = string_new_with("+");
    else if (opts->flag_space)
        out = string_new_with(" ");
    else
        out = string_new();
    if (!out)
        goto fail;

    if (string_append_char(out, qf_digit_text_at(digits_text, 0)) != 0)
        goto fail;
    if (precision > 0 || opts->flag_hash) {
        if (string_append_char(out, '.') != 0)
            goto fail;
        for (int i = 0; i < precision; i++) {
            int digit_index = i + 1;
            if (string_append_char(out, digit_index < extracted_digits ? qf_digit_text_at(digits_text, digit_index)
                                                                       : '0') != 0)
                goto fail;
        }
    }
    if (string_append_format(out, "E%+d", exp10) < 0)
        goto fail;

    {
        string_t *padded = qf_pad_text(out, opts->width, opts->flag_minus, opts->flag_zero, 0);
        string_free(digits_text);
        string_free(out);
        return padded;
    }

fail:
    string_free(digits_text);
    string_free(out);
    return NULL;
}

static string_t *qf_format_value_text(qfloat_t x, const qf_format_options_t *opts)
{
    if (!opts)
        return NULL;
    return opts->conversion == 'Q' ? qf_format_Q_value_text(x, opts) : qf_format_q_value_text(x, opts);
}

static string_format_result_t qf_format_callback(string_t *out, const string_format_spec_t *spec, va_list ap,
                                                 void *user)
{
    qf_format_options_t opts;
    string_t *text;
    qfloat_t value;

    (void)user;

    if (!out || !spec || !ap)
        return STRING_FORMAT_ERROR;
    if (spec->conversion != 'q' && spec->conversion != 'Q')
        return STRING_FORMAT_UNHANDLED;

    if (qf_format_options_from_spec(&opts, spec, ap) != 0)
        return STRING_FORMAT_ERROR;

    value = va_arg(ap, qfloat_t);
    text = qf_format_value_text(value, &opts);
    if (!text)
        return STRING_FORMAT_ERROR;

    if (string_append_string(out, text) != 0) {
        string_free(text);
        return STRING_FORMAT_ERROR;
    }

    string_free(text);
    return STRING_FORMAT_HANDLED;
}

static int qf_copy_text_to_buffer(const string_t *text, char *out, size_t out_size)
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

string_t *qf_vsprintf_text(const char *fmt, va_list ap)
{
    if (!fmt)
        return NULL;

    return string_vsprintf_with_callback(fmt, ap, qf_format_callback, NULL);
}

int qf_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap)
{
    string_t *text = qf_vsprintf_text(fmt, ap);
    int written = qf_copy_text_to_buffer(text, out, out_size);

    string_free(text);
    return written;
}

int qf_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = qf_vsprintf(out, out_size, fmt, ap);
    va_end(ap);
    return n;
}

string_t *qf_sprintf_text(const char *fmt, ...)
{
    va_list ap;
    string_t *out;

    va_start(ap, fmt);
    out = qf_vsprintf_text(fmt, ap);
    va_end(ap);
    return out;
}

int qf_printf(const char *fmt, ...)
{
    string_t *text;
    int written;
    va_list ap;

    va_start(ap, fmt);
    text = qf_vsprintf_text(fmt, ap);
    va_end(ap);

    if (!text)
        return -1;

    written = string_printf("%S", text);
    string_free(text);
    return written;
}
