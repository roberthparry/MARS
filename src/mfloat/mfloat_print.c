#include "mfloat_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MF_PRINT_SIG_DIGITS 32

static void pad_string(char *out, size_t out_size,
                       const char *s,
                       int width,
                       int flag_minus,
                       int flag_zero,
                       int sign_aware_zero)
{
    size_t len = strlen(s);
    int pad = (width > (int)len) ? (width - (int)len) : 0;
    size_t pos = 0;

    if (pad < 0)
        pad = 0;

    if (flag_minus) {
        for (size_t i = 0; s[i] && pos + 1 < out_size; i++)
            out[pos++] = s[i];
        for (int i = 0; i < pad && pos + 1 < out_size; i++)
            out[pos++] = ' ';
    } else if (flag_zero) {
        size_t i = 0;

        if (sign_aware_zero &&
            (s[0] == '+' || s[0] == '-' || s[0] == ' ')) {
            if (pos + 1 < out_size)
                out[pos++] = s[i++];
            for (int j = 0; j < pad && pos + 1 < out_size; j++)
                out[pos++] = '0';
        } else {
            for (int j = 0; j < pad && pos + 1 < out_size; j++)
                out[pos++] = '0';
        }

        for (; s[i] && pos + 1 < out_size; i++)
            out[pos++] = s[i];
    } else {
        for (int i = 0; i < pad && pos + 1 < out_size; i++)
            out[pos++] = ' ';
        for (size_t i = 0; s[i] && pos + 1 < out_size; i++)
            out[pos++] = s[i];
    }

    out[pos] = '\0';
}

static void mf_put_char(char c, char **dst, size_t *remaining, size_t *count)
{
    (*count)++;

    if (*dst == NULL || *remaining == 0)
        return;

    if (*remaining > 1) {
        **dst = c;
        (*dst)++;
        (*remaining)--;
    }
}

static void mf_put_str(const char *s, char **dst, size_t *remaining, size_t *count)
{
    while (*s) {
        mf_put_char(*s, dst, remaining, count);
        s++;
    }
}

static void mf_apply_sign_prefix(char **text_io, int flag_plus, int flag_space)
{
    char *text = *text_io;
    size_t len;
    char *out;

    if (!text || text[0] == '-')
        return;
    if (!flag_plus && !flag_space)
        return;

    len = strlen(text);
    out = malloc(len + 2u);
    if (!out)
        return;

    out[0] = flag_plus ? '+' : ' ';
    memcpy(out + 1, text, len + 1u);
    free(text);
    *text_io = out;
}

static void mf_round_digits_carry(char *digits,
                                  int *nd,
                                  size_t digits_cap,
                                  int *fixed_dp,
                                  int round_index)
{
    int i = round_index - 1;

    while (i >= 0 && digits[i] == '9') {
        digits[i] = '0';
        i--;
    }
    if (i >= 0) {
        digits[i]++;
    } else if (*nd + 1 < (int)digits_cap) {
        memmove(digits + 1, digits, (size_t)*nd + 1u);
        digits[0] = '1';
        (*nd)++;
        (*fixed_dp)++;
    }
}

static void mf_limit_significant_digits(char *digits, int *nd, int *fixed_dp, int limit)
{
    if (!digits || !nd || !fixed_dp || limit <= 0)
        return;

    if (*nd > limit) {
        if (digits[limit] >= '5')
            mf_round_digits_carry(digits, nd, (size_t)*nd + 2u, fixed_dp, limit);
        if (*nd > limit) {
            *nd = limit;
            digits[*nd] = '\0';
        }
    }
}

static int mf_parse_fixed_components(const char *text,
                                     int *negative_out,
                                     char **digits_out,
                                     int *fixed_dp_out)
{
    const char *s = text;
    const char *dot = NULL;
    size_t len;
    size_t digits_len = 0;
    char *digits;
    size_t int_len;
    size_t leading = 0;

    if (!text || !negative_out || !digits_out || !fixed_dp_out)
        return -1;

    *negative_out = 0;
    *digits_out = NULL;
    *fixed_dp_out = 0;

    if (*s == '-') {
        *negative_out = 1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    dot = strchr(s, '.');
    len = strlen(s);
    int_len = dot ? (size_t)(dot - s) : len;

    digits = malloc(len + 1u);
    if (!digits)
        return -1;

    for (size_t i = 0; i < len; i++) {
        if (s[i] != '.')
            digits[digits_len++] = s[i];
    }
    digits[digits_len] = '\0';

    while (leading + 1u < digits_len && digits[leading] == '0')
        leading++;

    if (digits_len == 0u || (digits_len == 1u && digits[0] == '0')) {
        digits[0] = '0';
        digits[1] = '\0';
        *digits_out = digits;
        *fixed_dp_out = 1;
        return 0;
    }

    if (leading > 0u) {
        memmove(digits, digits + leading, digits_len - leading + 1u);
        digits_len -= leading;
    }

    *digits_out = digits;
    *fixed_dp_out = (int)int_len - (int)leading;
    return 0;
}

static char *mf_build_scientific(const char *fixed, int uppercase, int precision)
{
    int negative;
    char *digits = NULL;
    int fixed_dp = 0;
    size_t digits_len;
    size_t sig_digits_target = 0u;
    size_t frac_len = 0;
    char expbuf[64];
    size_t exp_len;
    size_t out_len;
    char *out;
    char *p;
    int exp10;

    if (mf_parse_fixed_components(fixed, &negative, &digits, &fixed_dp) != 0)
        return NULL;

    digits_len = strlen(digits);
    {
        int nd = (int)digits_len;
        int sig_digits = precision >= 0 ? (precision + 1) : MF_PRINT_SIG_DIGITS;

        if (sig_digits < 1)
            sig_digits = 1;
        sig_digits_target = (size_t)sig_digits;
        mf_limit_significant_digits(digits, &nd, &fixed_dp, sig_digits);
        digits_len = strlen(digits);
    }
    if (precision >= 0) {
        if (digits_len < sig_digits_target) {
            char *grown = realloc(digits, sig_digits_target + 1u);

            if (!grown) {
                free(digits);
                return NULL;
            }
            digits = grown;
            memset(digits + digits_len, '0', sig_digits_target - digits_len);
            digits[sig_digits_target] = '\0';
            digits_len = sig_digits_target;
        }
    } else {
        while (digits_len > 1u && digits[digits_len - 1u] == '0') {
            digits[digits_len - 1u] = '\0';
            digits_len--;
        }
    }

    exp10 = fixed_dp - 1;
    if (digits_len > 1u)
        frac_len = digits_len - 1u;

    snprintf(expbuf, sizeof(expbuf), "%c%+d", uppercase ? 'E' : 'e', exp10);
    exp_len = strlen(expbuf);

    out_len = (negative ? 1u : 0u) + 1u + (frac_len ? 1u + frac_len : 0u) + exp_len + 1u;
    out = malloc(out_len);
    if (!out) {
        free(digits);
        return NULL;
    }

    p = out;
    if (negative)
        *p++ = '-';
    *p++ = digits[0];
    if (frac_len) {
        *p++ = '.';
        memcpy(p, digits + 1, frac_len);
        p += frac_len;
    }
    memcpy(p, expbuf, exp_len + 1u);

    free(digits);
    return out;
}

static char *mf_build_fixed_form(const char *fixed,
                                 int flag_plus,
                                 int flag_space,
                                 int flag_hash,
                                 int precision)
{
    int negative;
    char *digits = NULL;
    int fixed_dp = 0;
    int nd;
    int frac_digits;
    size_t buf_cap;
    char *buf;
    char *bp;

    if (mf_parse_fixed_components(fixed, &negative, &digits, &fixed_dp) != 0)
        return NULL;

    nd = (int)strlen(digits);

    if (precision < 0) {
        mf_limit_significant_digits(digits, &nd, &fixed_dp, MF_PRINT_SIG_DIGITS);
        nd = (int)strlen(digits);
    }

    if (precision >= 0) {
        int k = fixed_dp + precision;

        if (k + 1 > nd) {
            int pad = k + 1 - nd;
            char *grown = realloc(digits, (size_t)nd + (size_t)pad + 1u);

            if (!grown) {
                free(digits);
                return NULL;
            }
            digits = grown;
            for (int i = 0; i < pad; i++)
                digits[nd + i] = '0';
            nd += pad;
            digits[nd] = '\0';
        }

        if (k >= 0 && k < nd && digits[k] >= '5')
            mf_round_digits_carry(digits, &nd, (size_t)nd + 2u, &fixed_dp, k);

        if (k < nd)
            nd = k;
        if (nd < 0)
            nd = 0;
        digits[nd] = '\0';
    }

    frac_digits = precision >= 0 ? precision : (nd - fixed_dp);
    if (frac_digits < 0)
        frac_digits = 0;

    buf_cap = (negative || flag_plus || flag_space ? 1u : 0u)
            + (fixed_dp > 0 ? (size_t)fixed_dp : 1u)
            + ((frac_digits > 0 || flag_hash) ? 1u + (size_t)frac_digits : 0u)
            + 8u;
    if (buf_cap < (size_t)nd + 8u)
        buf_cap = (size_t)nd + 8u;

    buf = malloc(buf_cap);
    if (!buf) {
        free(digits);
        return NULL;
    }

    bp = buf;
    if (negative) {
        *bp++ = '-';
    } else if (flag_plus) {
        *bp++ = '+';
    } else if (flag_space) {
        *bp++ = ' ';
    }

    if (fixed_dp <= 0) {
        *bp++ = '0';
    } else {
        for (int i = 0; i < fixed_dp; i++) {
            int idx = i;

            if (idx >= 0 && idx < nd)
                *bp++ = digits[idx];
            else
                *bp++ = '0';
        }
    }

    if (frac_digits > 0 || flag_hash) {
        *bp++ = '.';
        for (int i = 0; i < frac_digits; i++) {
            int idx = fixed_dp + i;

            if (idx >= 0 && idx < nd)
                *bp++ = digits[idx];
            else
                *bp++ = '0';
        }

        if (precision < 0) {
            char *q = bp - 1;

            while (q > buf && *q == '0') {
                *q-- = '\0';
                bp--;
            }
            if (!flag_hash && q > buf && *q == '.') {
                *q = '\0';
                bp--;
            }
        } else if (precision == 0 && !flag_hash) {
            bp--;
        }
    }

    *bp = '\0';

    if (buf[0] == '0' && buf[1] >= '0' && buf[1] <= '9')
        memmove(buf, buf + 1, strlen(buf));

    if (buf[0] == '\0')
        strcpy(buf, "0");

    free(digits);
    return buf;
}

int mf_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap)
{
    const char *p = fmt;
    va_list ap_local;
    char *dst = out;
    size_t remaining = out_size;
    size_t count = 0;

    va_copy(ap_local, ap);

    if (!out || out_size == 0u) {
        dst = NULL;
        remaining = 0u;
    }

    while (*p) {
        if (*p != '%') {
            mf_put_char(*p++, &dst, &remaining, &count);
            continue;
        }

        p++;

        if (*p == '%') {
            mf_put_char('%', &dst, &remaining, &count);
            p++;
            continue;
        }

        {
            int flag_plus = 0;
            int flag_space = 0;
            int flag_minus = 0;
            int flag_zero = 0;
            int flag_hash = 0;
            int width = 0;
            int precision = -1;

            while (1) {
                if (*p == '+') {
                    flag_plus = 1;
                    p++;
                } else if (*p == ' ') {
                    flag_space = 1;
                    p++;
                } else if (*p == '-') {
                    flag_minus = 1;
                    p++;
                } else if (*p == '0') {
                    flag_zero = 1;
                    p++;
                } else if (*p == '#') {
                    flag_hash = 1;
                    p++;
                } else {
                    break;
                }
            }

            while (*p >= '0' && *p <= '9')
                width = width * 10 + (*p++ - '0');

            if (*p == '.') {
                p++;
                precision = 0;
                while (*p >= '0' && *p <= '9')
                    precision = precision * 10 + (*p++ - '0');
            }

            if ((*p == 'm' && p[1] == 'f') || (*p == 'M' && p[1] == 'F')) {
                int upper = (*p == 'M');
                const mfloat_t *x = va_arg(ap_local, const mfloat_t *);
                char *core = NULL;
                char *formatted = NULL;
                char *padded = NULL;

                p += 2;

                if (!x) {
                    core = strdup("<?>");
                } else if (!mfloat_is_finite(x)) {
                    core = mf_to_string(x);
                } else if (upper) {
                    char *fixed = mf_to_string(x);

                    if (fixed) {
                        core = mf_build_scientific(fixed, 1, precision);
                        free(fixed);
                    }
                } else {
                    char *fixed = mf_to_string(x);

                    if (fixed) {
                        core = mf_build_fixed_form(fixed, flag_plus, flag_space, flag_hash, precision);
                        free(fixed);
                    }
                }

                if (!core)
                    core = strdup("<?>");

                if (upper) {
                    mf_apply_sign_prefix(&core, flag_plus, flag_space);
                    formatted = core;
                } else {
                    formatted = core;
                }

                if (!formatted) {
                    free(core);
                    core = strdup("<?>");
                    formatted = core;
                }

                padded = malloc(strlen(formatted) + (size_t)(width > 0 ? width : 0) + 2u);
                if (!padded) {
                    free(formatted);
                    va_end(ap_local);
                    return -1;
                }

                pad_string(padded, strlen(formatted) + (size_t)(width > 0 ? width : 0) + 2u,
                           formatted, width, flag_minus, flag_zero, 0);
                mf_put_str(padded, &dst, &remaining, &count);

                free(padded);
                free(formatted);
                continue;
            }

            {
                char fmtbuf[32];
                char tmp[256];
                char *f = fmtbuf;

                *f++ = '%';
                if (flag_plus)  *f++ = '+';
                if (flag_space) *f++ = ' ';
                if (flag_minus) *f++ = '-';
                if (flag_zero)  *f++ = '0';
                if (flag_hash)  *f++ = '#';
                if (width > 0)
                    f += sprintf(f, "%d", width);
                if (precision >= 0) {
                    *f++ = '.';
                    f += sprintf(f, "%d", precision);
                }
                *f++ = *p;
                *f = '\0';

                switch (*p) {
                    case 'd': {
                        int v = va_arg(ap_local, int);
                        snprintf(tmp, sizeof(tmp), fmtbuf, v);
                    } break;
                    case 's': {
                        const char *v = va_arg(ap_local, const char *);
                        snprintf(tmp, sizeof(tmp), fmtbuf, v);
                    } break;
                    case 'c': {
                        int v = va_arg(ap_local, int);
                        snprintf(tmp, sizeof(tmp), fmtbuf, v);
                    } break;
                    default:
                        snprintf(tmp, sizeof(tmp), "<?>");
                        break;
                }

                p++;
                mf_put_str(tmp, &dst, &remaining, &count);
            }
        }
    }

    if (dst && remaining > 0u)
        *dst = '\0';

    va_end(ap_local);
    return (int)count;
}

int mf_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = mf_vsprintf(out, out_size, fmt, ap);
    va_end(ap);
    return n;
}

int mf_printf(const char *fmt, ...)
{
    va_list ap;
    int needed;
    char *buf;

    va_start(ap, fmt);
    needed = mf_vsprintf(NULL, 0u, fmt, ap);
    va_end(ap);

    if (needed < 0)
        return needed;

    buf = malloc((size_t)needed + 1u);
    if (!buf)
        return -1;

    va_start(ap, fmt);
    mf_vsprintf(buf, (size_t)needed + 1u, fmt, ap);
    va_end(ap);

    fwrite(buf, 1u, (size_t)needed, stdout);
    free(buf);
    return needed;
}
