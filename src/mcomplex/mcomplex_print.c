#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mcomplex_internal.h"

static void mc_put_char(char c, char **dst, size_t *remaining, size_t *count)
{
    (*count)++;

    if (*dst == NULL || *remaining == 0u)
        return;

    if (*remaining > 1u) {
        **dst = c;
        (*dst)++;
        (*remaining)--;
    }
}

static void mc_put_str(const char *s, char **dst, size_t *remaining, size_t *count)
{
    while (*s) {
        mc_put_char(*s, dst, remaining, count);
        s++;
    }
}

static int mc_format_complex(char *out,
                             size_t out_size,
                             const mcomplex_t *value,
                             int scientific,
                             int width,
                             int precision,
                             int flag_minus,
                             int flag_zero)
{
    char real_buf[256];
    char imag_buf[256];
    char fmt[32];
    char assembled[640];
    const char *imag_digits;

    if (!value)
        return -1;

    if (precision >= 0) {
        snprintf(fmt, sizeof(fmt), "%%%s%s%d.%d%s",
                 flag_minus ? "-" : "",
                 flag_zero ? "0" : "",
                 width,
                 precision,
                 scientific ? "MF" : "mf");
    } else if (width > 0) {
        snprintf(fmt, sizeof(fmt), "%%%s%s%d%s",
                 flag_minus ? "-" : "",
                 flag_zero ? "0" : "",
                 width,
                 scientific ? "MF" : "mf");
    } else {
        snprintf(fmt, sizeof(fmt), "%%%s%s",
                 scientific ? "MF" : "mf",
                 "");
    }

    if (mf_sprintf(real_buf, sizeof(real_buf), fmt, mc_real(value)) < 0 ||
        mf_sprintf(imag_buf, sizeof(imag_buf), fmt, mc_imag(value)) < 0)
        return -1;

    imag_digits = imag_buf;
    if (imag_buf[0] == '-')
        imag_digits++;

    if (imag_buf[0] == '-')
        snprintf(assembled, sizeof(assembled), "%s - %si", real_buf, imag_digits);
    else
        snprintf(assembled, sizeof(assembled), "%s + %si", real_buf, imag_digits);

    if (out && out_size > 0u) {
        size_t assembled_len = strlen(assembled);
        size_t copy_len = assembled_len;

        if (copy_len >= out_size)
            copy_len = out_size - 1u;

        memcpy(out, assembled, copy_len);
        out[copy_len] = '\0';
    }

    return (int)strlen(assembled);
}

int mc_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap)
{
    const char *p = fmt;
    va_list ap_local;
    char *dst = out;
    size_t remaining = out_size;
    size_t count = 0u;

    va_copy(ap_local, ap);

    if (!out || out_size == 0u) {
        dst = NULL;
        remaining = 0u;
    }

    while (*p) {
        if (*p != '%') {
            mc_put_char(*p++, &dst, &remaining, &count);
            continue;
        }

        p++;

        if (*p == '%') {
            mc_put_char(*p++, &dst, &remaining, &count);
            continue;
        }

        {
            int flag_minus = 0;
            int flag_zero = 0;
            int width = 0;
            int precision = -1;

            while (*p == '-' || *p == '0') {
                if (*p == '-')
                    flag_minus = 1;
                else if (*p == '0')
                    flag_zero = 1;
                p++;
            }

            while (*p >= '0' && *p <= '9')
                width = width * 10 + (*p++ - '0');

            if (*p == '.') {
                precision = 0;
                p++;
                while (*p >= '0' && *p <= '9')
                    precision = precision * 10 + (*p++ - '0');
            }

            if ((*p == 'm' && p[1] == 'z') || (*p == 'M' && p[1] == 'Z')) {
                const mcomplex_t *value = va_arg(ap_local, const mcomplex_t *);
                char tmp[640];
                int n = mc_format_complex(tmp, sizeof(tmp), value, *p == 'M',
                                          width, precision, flag_minus, flag_zero);

                if (n < 0) {
                    va_end(ap_local);
                    return -1;
                }
                mc_put_str(tmp, &dst, &remaining, &count);
                p += 2;
                continue;
            }

            {
                char specbuf[64];
                char tmp[512];
                char *f = specbuf;

                *f++ = '%';
                if (flag_minus)
                    *f++ = '-';
                if (flag_zero)
                    *f++ = '0';
                if (width > 0)
                    f += sprintf(f, "%d", width);
                if (precision >= 0) {
                    *f++ = '.';
                    f += sprintf(f, "%d", precision);
                }
                *f++ = *p;
                *f = '\0';

                switch (*p) {
                    case 'd':
                    case 'i':
                        snprintf(tmp, sizeof(tmp), specbuf, va_arg(ap_local, int));
                        break;
                    case 'u':
                    case 'o':
                    case 'x':
                    case 'X':
                        snprintf(tmp, sizeof(tmp), specbuf, va_arg(ap_local, unsigned int));
                        break;
                    case 'f':
                    case 'e':
                    case 'E':
                    case 'g':
                    case 'G':
                        snprintf(tmp, sizeof(tmp), specbuf, va_arg(ap_local, double));
                        break;
                    case 'c':
                        snprintf(tmp, sizeof(tmp), specbuf, va_arg(ap_local, int));
                        break;
                    case 's':
                        snprintf(tmp, sizeof(tmp), specbuf, va_arg(ap_local, const char *));
                        break;
                    case 'p':
                        snprintf(tmp, sizeof(tmp), specbuf, va_arg(ap_local, void *));
                        break;
                    default:
                        snprintf(tmp, sizeof(tmp), "<?>");
                        break;
                }

                mc_put_str(tmp, &dst, &remaining, &count);
                if (*p)
                    p++;
            }
        }
    }

    if (dst && remaining > 0u)
        *dst = '\0';

    va_end(ap_local);
    return (int)count;
}

int mc_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    int n;
    va_list ap;

    va_start(ap, fmt);
    n = mc_vsprintf(out, out_size, fmt, ap);
    va_end(ap);
    return n;
}

int mc_printf(const char *fmt, ...)
{
    int needed;
    char *buf;
    va_list ap;

    va_start(ap, fmt);
    needed = mc_vsprintf(NULL, 0u, fmt, ap);
    va_end(ap);
    if (needed < 0)
        return needed;

    buf = malloc((size_t)needed + 1u);
    if (!buf)
        return -1;

    va_start(ap, fmt);
    mc_vsprintf(buf, (size_t)needed + 1u, fmt, ap);
    va_end(ap);

    fwrite(buf, 1u, (size_t)needed, stdout);
    free(buf);
    return needed;
}
