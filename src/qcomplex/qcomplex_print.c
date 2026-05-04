#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcomplex_internal.h"
static inline void qc_put_char(char **dst, size_t *remaining, size_t *count, char c) {
    if (*remaining > 1 && *dst) {
        **dst = c;
        (*dst)++;
        (*remaining)--;
    }
    (*count)++;
}

static inline void qc_put_str(char **dst, size_t *remaining, size_t *count, const char *s) {
    while (*s) {
        qc_put_char(dst, remaining, count, *s++);
    }
}

static void qc_pad_string(char *out, size_t out_size,
                           const char *s, int width,
                           int flag_minus, int flag_zero)
{
    size_t len = strlen(s);
    int pad = (width > (int)len) ? (width - (int)len) : 0;
    size_t pos = 0;
    if (flag_minus) {
        for (size_t i = 0; s[i] && pos + 1 < out_size; i++) out[pos++] = s[i];
        for (int i = 0; i < pad && pos + 1 < out_size; i++) out[pos++] = ' ';
    } else if (flag_zero) {
        for (int j = 0; j < pad && pos + 1 < out_size; j++) out[pos++] = '0';
        for (size_t i = 0; s[i] && pos + 1 < out_size; i++) out[pos++] = s[i];
    } else {
        for (int i = 0; i < pad && pos + 1 < out_size; i++) out[pos++] = ' ';
        for (size_t i = 0; s[i] && pos + 1 < out_size; i++) out[pos++] = s[i];
    }
    out[pos] = '\0';
}

/* Ensure a qfloat string has exactly <precision> digits after the decimal.
   Works for both fixed (%q) and scientific (%Q) formats. */
static void qc_fix_precision(char *s, int precision)
{
    if (precision < 0) return;

    char *exp = strchr(s, 'e');
    if (!exp) exp = strchr(s, 'E');

    char saved_exp[64] = {0};
    if (exp) {
        strcpy(saved_exp, exp);
        *exp = '\0';
    }

    char *dot = strchr(s, '.');
    if (!dot) {
        /* No decimal point → add one */
        size_t len = strlen(s);
        s[len] = '.';
        s[len+1] = '\0';
        dot = s + len;
    }

    char *p = dot + 1;
    int count = 0;

    while (*p && count < precision) {
        p++;
        count++;
    }

    /* Truncate or pad */
    *p = '\0';

    while (count < precision) {
        strcat(s, "0");
        count++;
    }

    /* Restore exponent */
    if (saved_exp[0]) {
        strcat(s, saved_exp);
    }
}

int qc_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap)
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
            qc_put_char(&dst, &remaining, &count, *p++);
            continue;
        }

        p++; /* skip '%' */

        int flag_plus  = 0;
        int flag_space = 0;
        int flag_minus = 0;
        int flag_zero  = 0;
        int flag_hash  = 0;

        while (1) {
            if      (*p == '+') { flag_plus  = 1; p++; }
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

        /* ============================================================
           %z / %Z  (complex number formatting)
           ============================================================ */
        if (*p == 'z' || *p == 'Z') {
            char spec = *p++;
            qcomplex_t z = va_arg(ap_local, qcomplex_t);

            char re_buf[256], im_buf[256];
            qfloat_t im_abs = qf_signbit(z.im) ? qf_neg(z.im) : z.im;

            /* Build qfloat format: %q or %Q */
            char cfmt[16];
            snprintf(cfmt, sizeof(cfmt), "%%%c", (spec == 'Z') ? 'Q' : 'q');

            /* Format real and imaginary parts */
            qf_sprintf(re_buf, sizeof(re_buf), cfmt, z.re);
            qf_sprintf(im_buf, sizeof(im_buf), cfmt, im_abs);

            /* Enforce precision manually */
            if (precision >= 0) {
                qc_fix_precision(re_buf, precision);
                qc_fix_precision(im_buf, precision);
            }

            /* Lowercase exponent for %Z */
            if (spec == 'Z') {
                for (char *s = re_buf; *s; s++) if (*s == 'E') *s = 'e';
                for (char *s = im_buf; *s; s++) if (*s == 'E') *s = 'e';
            }

            const char *sep = qf_signbit(z.im) ? " - " : " + ";

            char assembled[600];
            snprintf(assembled, sizeof(assembled), "%s%s%si", re_buf, sep, im_buf);

            char padded[600];
            qc_pad_string(padded, sizeof(padded),
                          assembled, width, flag_minus, flag_zero);

            qc_put_str(&dst, &remaining, &count, padded);
            continue;
        }

        /* ============================================================
           %q / %Q (qfloat formatting)
           ============================================================ */
        else if (*p == 'q' || *p == 'Q') {
            char spec = *p++;
            qfloat_t x = va_arg(ap_local, qfloat_t);

            char cfmt[32];
            char *f = cfmt;
            *f++ = '%';
            if (flag_plus)  *f++ = '+';
            if (flag_space) *f++ = ' ';
            if (flag_minus) *f++ = '-';
            if (flag_zero)  *f++ = '0';
            if (flag_hash)  *f++ = '#';
            if (width > 0)  f += sprintf(f, "%d", width);
            if (precision >= 0) { *f++ = '.'; f += sprintf(f, "%d", precision); }
            *f++ = spec;
            *f = '\0';

            char tmp[512];
            qf_sprintf(tmp, sizeof(tmp), cfmt, x);
            qc_put_str(&dst, &remaining, &count, tmp);
            continue;
        }

        /* ============================================================
           Standard C specifiers
           ============================================================ */
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
            if (width > 0)  f += sprintf(f, "%d", width);
            if (precision >= 0) { *f++ = '.'; f += sprintf(f, "%d", precision); }
            *f++ = *p;
            *f = '\0';

            switch (*p) {
                case 'd': case 'i': {
                    int v = va_arg(ap_local, int);
                    snprintf(tmp, sizeof(tmp), fmtbuf, v);
                } break;
                case 'u': case 'o': case 'x': case 'X': {
                    unsigned int v = va_arg(ap_local, unsigned int);
                    snprintf(tmp, sizeof(tmp), fmtbuf, v);
                } break;
                case 'f': case 'e': case 'E': case 'g': case 'G': {
                    double v = va_arg(ap_local, double);
                    snprintf(tmp, sizeof(tmp), fmtbuf, v);
                } break;
                case 'c': {
                    int v = va_arg(ap_local, int);
                    snprintf(tmp, sizeof(tmp), fmtbuf, (char)v);
                } break;
                case 's': {
                    const char *v = va_arg(ap_local, const char *);
                    snprintf(tmp, sizeof(tmp), fmtbuf, v);
                } break;
                case 'p': {
                    void *v = va_arg(ap_local, void *);
                    snprintf(tmp, sizeof(tmp), fmtbuf, v);
                } break;
                case '%': {
                    tmp[0] = '%'; tmp[1] = '\0';
                } break;
                default:
                    snprintf(tmp, sizeof(tmp), "<?>");
                    break;
            }
            p++;
            qc_put_str(&dst, &remaining, &count, tmp);
            continue;
        }
    }

    if (dst && remaining > 0)
        *dst = '\0';

    va_end(ap_local);
    return (int)count;
}

int qc_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = qc_vsprintf(out, out_size, fmt, ap);
    va_end(ap);
    return n;
}

int qc_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int needed = qc_vsprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (needed < 0) return needed;

    char *buf = malloc((size_t)needed + 1);
    if (!buf) return -1;

    va_start(ap, fmt);
    qc_vsprintf(buf, (size_t)needed + 1, fmt, ap);
    va_end(ap);

    fwrite(buf, 1, needed, stdout);
    free(buf);
    return needed;
}

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------ */

/* Faddeeva w(z) via Weideman's rational approximation (N=32 for ~quad precision) */
/* ------------------------------------------------------------ */

