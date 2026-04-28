#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matrix.h"

static int mo_put_char(char **dst, size_t *remaining, size_t *count, char c)
{
    if (*dst && *remaining > 1) {
        **dst = c;
        (*dst)++;
        (*remaining)--;
        **dst = '\0';
    }
    (*count)++;
    return 0;
}

static int mo_put_str(char **dst, size_t *remaining, size_t *count, const char *s)
{
    while (*s)
        mo_put_char(dst, remaining, count, *s++);
    return 0;
}

static int mo_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap)
{
    const char *p = fmt;
    char *dst = out;
    size_t remaining = out && out_size ? out_size : 0;
    size_t count = 0;
    va_list ap_local;

    if (dst && remaining > 0)
        *dst = '\0';

    va_copy(ap_local, ap);
    while (*p) {
        if (*p != '%') {
            mo_put_char(&dst, &remaining, &count, *p++);
            continue;
        }

        p++;
        if (*p == '%') {
            mo_put_char(&dst, &remaining, &count, *p++);
            continue;
        }

        if (*p == 'M' || *p == 'm') {
            int scientific = (*p == 'M');
            int layout = 0;
            const matrix_t *A;
            char *s;

            p++;
            if (*p == 'L' || *p == 'l') {
                layout = 1;
                p++;
            }

            A = va_arg(ap_local, const matrix_t *);
            s = mat_to_string(A, scientific
                                 ? (layout ? MAT_STRING_LAYOUT_SCIENTIFIC
                                           : MAT_STRING_INLINE_SCIENTIFIC)
                                 : (layout ? MAT_STRING_LAYOUT_PRETTY
                                           : MAT_STRING_INLINE_PRETTY));
            if (!s) {
                va_end(ap_local);
                return -1;
            }
            mo_put_str(&dst, &remaining, &count, s);
            free(s);
            continue;
        }

        {
            char spec = *p++;
            char fmtbuf[8];
            char tmp[512];
            int wrote;

            snprintf(fmtbuf, sizeof(fmtbuf), "%%%c", spec);

            switch (spec) {
            case 'd': {
                int v = va_arg(ap_local, int);
                wrote = snprintf(tmp, sizeof(tmp), fmtbuf, v);
                break;
            }
            case 'u': {
                unsigned v = va_arg(ap_local, unsigned);
                wrote = snprintf(tmp, sizeof(tmp), fmtbuf, v);
                break;
            }
            case 'g':
            case 'f':
            case 'e':
            case 'E': {
                double v = va_arg(ap_local, double);
                wrote = snprintf(tmp, sizeof(tmp), fmtbuf, v);
                break;
            }
            case 'c': {
                int v = va_arg(ap_local, int);
                wrote = snprintf(tmp, sizeof(tmp), fmtbuf, v);
                break;
            }
            case 's': {
                const char *v = va_arg(ap_local, const char *);
                wrote = snprintf(tmp, sizeof(tmp), fmtbuf, v ? v : "(null)");
                break;
            }
            case 'p': {
                void *v = va_arg(ap_local, void *);
                wrote = snprintf(tmp, sizeof(tmp), fmtbuf, v);
                break;
            }
            default:
                tmp[0] = '%';
                tmp[1] = spec;
                tmp[2] = '\0';
                wrote = 2;
                break;
            }

            if (wrote < 0) {
                va_end(ap_local);
                return -1;
            }
            mo_put_str(&dst, &remaining, &count, tmp);
        }
    }

    va_end(ap_local);
    return (int)count;
}

int mat_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    int n;
    va_list ap;

    va_start(ap, fmt);
    n = mo_vsprintf(out, out_size, fmt, ap);
    va_end(ap);
    return n;
}

int mat_printf(const char *fmt, ...)
{
    va_list ap;
    int needed;
    char *buf;

    va_start(ap, fmt);
    needed = mo_vsprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0)
        return needed;

    buf = malloc((size_t)needed + 1);
    if (!buf)
        return -1;

    va_start(ap, fmt);
    mo_vsprintf(buf, (size_t)needed + 1, fmt, ap);
    va_end(ap);

    fputs(buf, stdout);
    free(buf);
    return needed;
}
