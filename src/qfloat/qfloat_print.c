#include "qfloat_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper: apply width / padding to a preformatted string.
 * - s already contains any sign characters.
 * - sign_aware_zero controls whether '0' padding should come after the sign.
 */
static void pad_string(char *out, size_t out_size,
                       const char *s,
                       int width,
                       int flag_minus,
                       int flag_zero,
                       int sign_aware_zero)
{
    size_t len = strlen(s);
    int pad = (width > (int)len) ? (width - (int)len) : 0;
    if (pad < 0) pad = 0;

    size_t pos = 0;

    if (flag_minus) {
        /* left-justify: value, then spaces */
        for (size_t i = 0; s[i] && pos + 1 < out_size; i++)
            out[pos++] = s[i];
        for (int i = 0; i < pad && pos + 1 < out_size; i++)
            out[pos++] = ' ';
    } else if (flag_zero) {
        size_t i = 0;
        if (sign_aware_zero &&
            (s[0] == '+' || s[0] == '-' || s[0] == ' ')) {
            /* keep sign, then zeros, then rest */
            if (pos + 1 < out_size)
                out[pos++] = s[i++];
            for (int j = 0; j < pad && pos + 1 < out_size; j++)
                out[pos++] = '0';
        } else {
            /* zeros first, then full string */
            for (int j = 0; j < pad && pos + 1 < out_size; j++)
                out[pos++] = '0';
        }
        for (; s[i] && pos + 1 < out_size; i++)
            out[pos++] = s[i];
    } else {
        /* right-justify: spaces, then value */
        for (int i = 0; i < pad && pos + 1 < out_size; i++)
            out[pos++] = ' ';
        for (size_t i = 0; s[i] && pos + 1 < out_size; i++)
            out[pos++] = s[i];
    }

    out[pos] = '\0';
}

/* Inline helpers for safe output */

static void qf_put_char(char c, char **dst, size_t *remaining, size_t *count)
{
    /* Count always increases */
    (*count)++;

    /* If no output buffer, only count */
    if (*dst == NULL || *remaining == 0)
        return;

    /* Write only if space for at least 1 char + NUL */
    if (*remaining > 1) {
        **dst = c;
        (*dst)++;
        (*remaining)--;
    }
}

static void qf_put_str(const char *s, char **dst, size_t *remaining, size_t *count)
{
    while (*s) {
        qf_put_char(*s, dst, remaining, count);
        s++;
    }
}

/* Helper: uppercase exponent for %Q */
static inline void qf_uppercase_E(char *s) {
    char *e = strchr(s, 'e');
    if (e) *e = 'E';
}

static void qf_apply_sign_prefix(char *text, int flag_plus, int flag_space)
{
    if (text[0] == '-')
        return;

    if (flag_plus) {
        memmove(text + 1, text, strlen(text) + 1);
        text[0] = '+';
    } else if (flag_space) {
        memmove(text + 1, text, strlen(text) + 1);
        text[0] = ' ';
    }
}

static void qf_round_digits_carry(char *digits,
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
        memmove(digits + 1, digits, (size_t)*nd + 1);
        digits[0] = '1';
        (*nd)++;
        (*fixed_dp)++;
    }
}
    
int qf_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap)
{
    const char *p = fmt;

    va_list ap_local;
    va_copy(ap_local, ap);

    char *dst = out;
    size_t remaining = out_size;
    size_t count = 0;

    if (!out || out_size == 0) {
        dst = NULL;
        remaining = 0;
    }

    while (*p) {

        if (*p != '%') {
            qf_put_char(*p++, &dst, &remaining, &count);
            continue;
        }

        p++; /* skip '%' */

        int flag_plus  = 0;
        int flag_space = 0;
        int flag_minus = 0;
        int flag_zero  = 0;
        int flag_hash  = 0;

        while (1) {
            if      (*p == '+') { flag_plus = 1; p++; }
            else if (*p == ' ') { flag_space = 1; p++; }
            else if (*p == '-') { flag_minus = 1; p++; }
            else if (*p == '0') { flag_zero  = 1; p++; }
            else if (*p == '#') { flag_hash  = 1; p++; }
            else break;
        }

        int width = 0;
        while (*p >= '0' && *p <= '9')
            width = width * 10 + (*p++ - '0');

        int precision = -1;
        if (*p == '.') {
            p++;
            precision = 0;
            while (*p >= '0' && *p <= '9')
                precision = precision * 10 + (*p++ - '0');
        }

        /* ----------------------------------------------------
           %Q — scientific (uppercase exponent)
           ---------------------------------------------------- */
        if (*p == 'Q') {
            p++;

            qfloat_t x   = va_arg(ap_local, qfloat_t);

            char core[256];
            qf_to_string(x, core, sizeof(core));
            qf_uppercase_E(core);

            char tmp[256];
            strcpy(tmp, core);

            qf_apply_sign_prefix(tmp, flag_plus, flag_space);

            char padded[256];
            pad_string(padded, sizeof(padded),
                       tmp, width, flag_minus, flag_zero, 0);

            qf_put_str(padded, &dst, &remaining, &count);
            continue;
        }

        /* ----------------------------------------------------
           %q — fixed-format with explicit precision and
                 concise default, fallback to scientific
           ---------------------------------------------------- */
        else if (*p == 'q') {
            p++;

            qfloat_t x = va_arg(ap_local, qfloat_t);

            /* Step 1: canonical scientific form */
            char sci[128];
            qf_to_string(x, sci, sizeof(sci));   /* mantissa + 'e' + exponent */

            char *e = strchr(sci, 'e');
            if (!e) e = strchr(sci, 'E');

            int exp10 = 0;
            if (e) {
                exp10 = atoi(e + 1);
                *e = '\0'; /* terminate mantissa */
            }

            /* Step 2: scientific fallback if exponent outside [-6, 32] */
            if (!e || exp10 < -6 || exp10 > 32) {

                char tmp[256];
                qf_to_string(x, tmp, sizeof(tmp));

                /* normalize to lowercase 'e' and trim mantissa only */
                char *ep = strchr(tmp, 'e');
                if (!ep) ep = strchr(tmp, 'E');

                if (ep) {
                    char mant[256];
                    char expbuf[32];

                    size_t mlen = (size_t)(ep - tmp);
                    memcpy(mant, tmp, mlen);
                    mant[mlen] = '\0';

                    strcpy(expbuf, ep); /* includes 'e' and exponent */

                    /* force lowercase 'e' */
                    if (expbuf[0] == 'E')
                        expbuf[0] = 'e';

                    /* trim trailing zeros in mantissa */
                    char *q = mant + strlen(mant) - 1;
                    while (q > mant && *q == '0')
                        *q-- = '\0';
                    if (q > mant && *q == '.')
                        *q = '\0';

                    {
                        size_t mlen = strlen(mant);
                        size_t elen = strlen(expbuf);

                        /* Clamp lengths to avoid overflow */
                        if (mlen > sizeof(tmp) - 1)
                            mlen = sizeof(tmp) - 1;

                        if (elen > sizeof(tmp) - 1 - mlen)
                            elen = sizeof(tmp) - 1 - mlen;

                        memcpy(tmp, mant, mlen);
                        memcpy(tmp + mlen, expbuf, elen);
                        tmp[mlen + elen] = '\0';
                    }
                }

                qf_apply_sign_prefix(tmp, flag_plus, flag_space);

                char padded[256];
                pad_string(padded, sizeof(padded),
                           tmp, width, flag_minus, flag_zero, 0);

                qf_put_str(padded, &dst, &remaining, &count);
                continue;
            }

            /* Step 3: fixed-format reconstruction */

            char mantissa[128];
            strcpy(mantissa, sci);

            int negative = 0;
            char *mant = mantissa;

            if (*mant == '-') {
                negative = 1;
                mant++;
            }

            char intpart[128], fracpart[128];
            char *dot = strchr(mant, '.');

            if (dot) {
                size_t ilen = (size_t)(dot - mant);
                memcpy(intpart, mant, ilen);
                intpart[ilen] = '\0';
                strcpy(fracpart, dot + 1);
            } else {
                strcpy(intpart, mant);
                fracpart[0] = '\0';
            }

            char digits[256];
            snprintf(digits, sizeof(digits), "%s%s", intpart, fracpart);
            int nd = (int)strlen(digits);

            int fixed_dp = (int)strlen(intpart) + exp10; /* decimal point position in digits[] */

            /* Step 3a: suppress noise in digits 33-34.
             * qfloat_t is reliable to ~32 significant digits.  Digits 33-34
             * are real information only when the preceding digits are varied;
             * when they follow a long run of zeros they are rounding noise.
             * Heuristic: if the first 32 digits end in >=5 zeros AND the
             * remaining digits are non-zero, treat them as noise and round
             * to 32 sig figs.  Otherwise keep all 34. */
            if (nd > 32) {
                int trail = 0;
                for (int i = 31; i >= 0 && digits[i] == '0'; i--)
                    trail++;
                int tail_nonzero = 0;
                for (int i = 32; i < nd; i++)
                    if (digits[i] != '0') { tail_nonzero = 1; break; }

                if (trail >= 5 && tail_nonzero) {
                    if (digits[32] >= '5')
                        qf_round_digits_carry(digits, &nd, sizeof(digits), &fixed_dp, 32);
                    nd = 32;
                    digits[nd] = '\0';
                }
            }

            /* Step 3b: rounding for explicit precision */
            if (precision >= 0) {
                int K = fixed_dp + precision; /* index of rounding digit */

                if (K + 1 > nd) {
                    int pad = K + 1 - nd;
                    if (nd + pad >= (int)sizeof(digits))
                        pad = (int)sizeof(digits) - 1 - nd;
                    for (int i = 0; i < pad; i++)
                        digits[nd + i] = '0';
                    nd += pad;
                    digits[nd] = '\0';
                }

                if (K >= 0 && K < nd && digits[K] >= '5') {
                    qf_round_digits_carry(digits, &nd, sizeof(digits), &fixed_dp, K);
                }

                if (K < nd)
                    nd = K;
                digits[nd] = '\0';
            }

            char buf2[512];
            char *bp = buf2;

            /* sign */
            if (negative) {
                *bp++ = '-';
            } else if (flag_plus) {
                *bp++ = '+';
            } else if (flag_space) {
                *bp++ = ' ';
            }

            /* integer part */
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

            /* fractional part */
            int frac_digits;
            if (precision < 0) {
                frac_digits = nd - fixed_dp;
                if (frac_digits < 0) frac_digits = 0;
            } else {
                frac_digits = precision;
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
                    /* trim trailing zeros, but keep '.' if flag_hash */
                    char *q = bp - 1;
                    while (q > buf2 && *q == '0') {
                        *q-- = '\0';
                        bp--;
                    }
                    if (!flag_hash && q > buf2 && *q == '.') {
                        *q = '\0';
                        bp--;
                    }
                } else {
                    if (precision == 0 && !flag_hash) {
                        /* remove '.' if no digits requested and no # */
                        bp--;
                    }
                }
            }

            *bp = '\0';

            /* Fix leading zero in integer part (e.g. "099" → "99") */
            if (buf2[0] == '0' && buf2[1] >= '0' && buf2[1] <= '9') {
                memmove(buf2, buf2 + 1, strlen(buf2));
                bp--;  /* adjust write pointer */
            }

            if (buf2[0] == '\0')
                strcpy(buf2, "0");

            char padded[512];
            pad_string(padded, sizeof(padded),
                       buf2, width, flag_minus, flag_zero, 0);

            qf_put_str(padded, &dst, &remaining, &count);
            continue;
        }

        /* ----------------------------------------------------
           Delegate to snprintf for everything else
           ---------------------------------------------------- */
        else {
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
            *f   = '\0';

            switch (*p) {
                case 'd': {
                    int v = va_arg(ap_local, int);
                    snprintf(tmp, sizeof(tmp), fmtbuf, v);
                } break;

                case 's': {
                    const char *v = va_arg(ap_local, const char *);
                    snprintf(tmp, sizeof(tmp), fmtbuf, v);
                } break;

                default:
                    snprintf(tmp, sizeof(tmp), "<?>");
                    break;
            }

            p++;

            qf_put_str(tmp, &dst, &remaining, &count);
            continue;
        }
    }

    if (dst && remaining > 0)
        *dst = '\0';

    va_end(ap_local);
    return (int)count;
}

int qf_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    int n = qf_vsprintf(out, out_size, fmt, ap);

    va_end(ap);
    return n;
}

int qf_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    int needed = qf_vsprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (needed < 0)
        return needed;

    char *buf = malloc((size_t)needed + 1);
    if (!buf)
        return -1;

    va_start(ap, fmt);
    qf_vsprintf(buf, (size_t)needed + 1, fmt, ap);
    va_end(ap);

    fwrite(buf, 1, needed, stdout);
    free(buf);

    return needed;
}
