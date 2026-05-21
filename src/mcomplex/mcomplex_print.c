#include <ctype.h>
#include <limits.h>
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

static int mc_formatted_zero(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    int saw_digit = 0;

    if (!s)
        return 0;

    while (isspace(*p))
        ++p;
    if (*p == '+' || *p == '-')
        ++p;

    while (*p) {
        if (*p >= '0' && *p <= '9') {
            saw_digit = 1;
            if (*p != '0')
                return 0;
            ++p;
            continue;
        }
        if (*p == '.') {
            ++p;
            continue;
        }
        if (*p == 'e' || *p == 'E')
            return saw_digit;
        if (isspace(*p)) {
            ++p;
            continue;
        }
        return 0;
    }

    return saw_digit;
}

static int mc_formatted_one(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    int saw_one = 0;
    int before_dot = 1;

    if (!s)
        return 0;

    while (isspace(*p))
        ++p;
    if (*p == '+')
        ++p;

    while (*p) {
        if (*p >= '0' && *p <= '9') {
            if (before_dot) {
                if (!saw_one) {
                    if (*p == '1')
                        saw_one = 1;
                    else if (*p != '0')
                        return 0;
                } else {
                    return 0;
                }
            } else if (*p != '0') {
                return 0;
            }
            ++p;
            continue;
        }
        if (*p == '.') {
            before_dot = 0;
            ++p;
            continue;
        }
        if (*p == 'e' || *p == 'E') {
            char *end = NULL;
            long exponent;

            ++p;
            exponent = strtol((const char *)p, &end, 10);
            if ((const unsigned char *)end == p || exponent != 0)
                return 0;
            p = (const unsigned char *)end;
            continue;
        }
        if (isspace(*p)) {
            ++p;
            continue;
        }
        return 0;
    }

    return saw_one;
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
    char *real_buf = NULL;
    char *imag_buf = NULL;
    char fmt[32];
    char *assembled = NULL;
    const char *imag_digits;
    size_t assembled_len;
    int real_needed;
    int imag_needed;
    int rc = -1;

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

    if (precision < 0 && width == 0 && !flag_minus && !flag_zero && !scientific) {
        real_buf = mf_to_string(mc_real(value));
        imag_buf = mf_to_string(mc_imag(value));
        if (!real_buf || !imag_buf)
            goto cleanup;
    } else {
        real_needed = mf_sprintf(NULL, 0u, fmt, mc_real(value));
        imag_needed = mf_sprintf(NULL, 0u, fmt, mc_imag(value));
        if (real_needed < 0 || imag_needed < 0)
            goto cleanup;

        real_buf = malloc((size_t)real_needed + 1u);
        imag_buf = malloc((size_t)imag_needed + 1u);
        if (!real_buf || !imag_buf)
            goto cleanup;

        if (mf_sprintf(real_buf, (size_t)real_needed + 1u, fmt, mc_real(value)) < 0 ||
            mf_sprintf(imag_buf, (size_t)imag_needed + 1u, fmt, mc_imag(value)) < 0)
            goto cleanup;
    }

    imag_digits = imag_buf;
    if (imag_buf[0] == '-')
        imag_digits++;

    if (mc_formatted_zero(imag_buf)) {
        assembled_len = strlen(real_buf);
        assembled = malloc(assembled_len + 1u);
        if (assembled)
            memcpy(assembled, real_buf, assembled_len + 1u);
    } else if (mc_formatted_zero(real_buf)) {
        assembled_len = mc_formatted_one(imag_digits)
                            ? (imag_buf[0] == '-' ? 2u : 1u)
                            : strlen(imag_digits) + (imag_buf[0] == '-' ? 2u : 1u);
        assembled = malloc(assembled_len + 1u);
        if (assembled) {
            if (mc_formatted_one(imag_digits))
                snprintf(assembled, assembled_len + 1u,
                         imag_buf[0] == '-' ? "-i" : "i");
            else
                snprintf(assembled, assembled_len + 1u,
                         imag_buf[0] == '-' ? "-%si" : "%si", imag_digits);
        }
    } else if (imag_buf[0] == '-') {
        assembled_len = strlen(real_buf) + (mc_formatted_one(imag_digits)
                                                ? 4u
                                                : strlen(imag_digits) + 5u);
        assembled = malloc(assembled_len + 1u);
        if (assembled) {
            if (mc_formatted_one(imag_digits))
                snprintf(assembled, assembled_len + 1u, "%s - i", real_buf);
            else
                snprintf(assembled, assembled_len + 1u, "%s - %si", real_buf, imag_digits);
        }
    } else {
        assembled_len = strlen(real_buf) + (mc_formatted_one(imag_digits)
                                                ? 4u
                                                : strlen(imag_digits) + 5u);
        assembled = malloc(assembled_len + 1u);
        if (assembled) {
            if (mc_formatted_one(imag_digits))
                snprintf(assembled, assembled_len + 1u, "%s + i", real_buf);
            else
                snprintf(assembled, assembled_len + 1u, "%s + %si", real_buf, imag_digits);
        }
    }
    if (!assembled)
        goto cleanup;

    if (out && out_size > 0u) {
        size_t copy_len = assembled_len;

        if (copy_len >= out_size)
            copy_len = out_size - 1u;

        memcpy(out, assembled, copy_len);
        out[copy_len] = '\0';
    }

    rc = assembled_len > (size_t)INT_MAX ? -1 : (int)assembled_len;

cleanup:
    free(assembled);
    free(imag_buf);
    free(real_buf);
    return rc;
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
                if ((size_t)n < sizeof(tmp)) {
                    mc_put_str(tmp, &dst, &remaining, &count);
                } else {
                    char *big = malloc((size_t)n + 1u);
                    if (!big ||
                        mc_format_complex(big, (size_t)n + 1u, value, *p == 'M',
                                          width, precision, flag_minus, flag_zero) < 0) {
                        free(big);
                        va_end(ap_local);
                        return -1;
                    }
                    mc_put_str(big, &dst, &remaining, &count);
                    free(big);
                }
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
