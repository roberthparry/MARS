#include "number.h"
#include "number_internal.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *number_strdup(const char *text)
{
    size_t len;
    char *copy;

    if (!text)
        return NULL;
    len = strlen(text);
    copy = malloc(len + 1u);
    if (!copy)
        return NULL;
    memcpy(copy, text, len + 1u);
    return copy;
}

char *number_format_double(const number_t *number, bool scientific, int precision)
{
    int needed;
    char *out;
    char fmt[32];
    double value;

    if (!number)
        return NULL;
    value = number_impl_const(number)->value.d;
    if (precision >= 0)
        snprintf(fmt, sizeof(fmt), scientific ? "%%.%dE" : "%%.%dg", precision);
    else
        snprintf(fmt, sizeof(fmt), scientific ? "%%.16E" : "%%.17g");
    needed = snprintf(NULL, 0, fmt, value);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    snprintf(out, (size_t)needed + 1u, fmt, value);
    return out;
}

char *number_format_qfloat(const number_t *number, bool scientific, int precision)
{
    int needed;
    char *out;
    char fmt[32];

    if (!number)
        return NULL;
    if (precision >= 0)
        snprintf(fmt, sizeof(fmt), scientific ? "%%.%dQ" : "%%.%dq", precision);
    else
        snprintf(fmt, sizeof(fmt), scientific ? "%%Q" : "%%q");
    needed = qf_sprintf(NULL, 0u, fmt, number_impl_const(number)->value.qf);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    qf_sprintf(out, (size_t)needed + 1u, fmt, number_impl_const(number)->value.qf);
    return out;
}

char *number_format_qcomplex(const number_t *number, bool scientific, int precision)
{
    int needed;
    char *out;
    char fmt[32];

    if (!number)
        return NULL;
    if (precision >= 0)
        snprintf(fmt, sizeof(fmt), scientific ? "%%.%dZ" : "%%.%dz", precision);
    else
        snprintf(fmt, sizeof(fmt), scientific ? "%%Z" : "%%z");
    needed = qc_sprintf(NULL, 0u, fmt, number_impl_const(number)->value.qc);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    qc_sprintf(out, (size_t)needed + 1u, fmt, number_impl_const(number)->value.qc);
    return out;
}

char *number_format_mfloat(const number_t *number, bool scientific, int precision)
{
    int needed;
    char *out;
    char fmt[32];

    if (!number)
        return NULL;
    if (precision >= 0)
        snprintf(fmt, sizeof(fmt), scientific ? "%%.%dMF" : "%%.%dmf", precision);
    else
        snprintf(fmt, sizeof(fmt), scientific ? "%%MF" : "%%mf");
    needed = mf_sprintf(NULL, 0u, fmt, number_impl_const(number)->value.mf);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    mf_sprintf(out, (size_t)needed + 1u, fmt, number_impl_const(number)->value.mf);
    return out;
}

char *number_format_mcomplex(const number_t *number, bool scientific, int precision)
{
    int needed;
    char *out;
    char fmt[32];

    if (!number)
        return NULL;
    if (precision >= 0)
        snprintf(fmt, sizeof(fmt), scientific ? "%%.%dMZ" : "%%.%dmz", precision);
    else
        snprintf(fmt, sizeof(fmt), scientific ? "%%MZ" : "%%mz");
    needed = mc_sprintf(NULL, 0u, fmt, number_impl_const(number)->value.mc);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    mc_sprintf(out, (size_t)needed + 1u, fmt, number_impl_const(number)->value.mc);
    return out;
}

static char *number_format_inexact(const number_t *number, bool scientific, int precision)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    if (!number)
        return NULL;
    if (vt && vt->format_inexact)
        return vt->format_inexact(number, scientific, precision);
    return NULL;
}

int num_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap)
{
    va_list ap_local;
    const char *p = fmt;
    size_t pos = 0u;
    int total = 0;

    if (!fmt)
        return -1;
    va_copy(ap_local, ap);
    while (*p) {
        char tmp[512];
        int width = 0;
        int precision = -1;
        int left = 0, zero = 0, plus = 0, space = 0, alt = 0;
        char spec;
        char *core = NULL;
        int core_len;
        int pad;

        if (*p != '%') {
            if (out && pos + 1u < out_size)
                out[pos] = *p;
            pos += 1u;
            total += 1;
            ++p;
            continue;
        }
        ++p;
        if (*p == '%') {
            if (out && pos + 1u < out_size)
                out[pos] = '%';
            pos += 1u;
            total += 1;
            ++p;
            continue;
        }
        while (*p == '-' || *p == '0' || *p == '+' || *p == ' ' || *p == '#') {
            left |= (*p == '-');
            zero |= (*p == '0');
            plus |= (*p == '+');
            space |= (*p == ' ');
            alt |= (*p == '#');
            ++p;
        }
        while (isdigit((unsigned char)*p)) {
            width = width * 10 + (*p - '0');
            ++p;
        }
        if (*p == '.') {
            precision = 0;
            ++p;
            while (isdigit((unsigned char)*p)) {
                precision = precision * 10 + (*p - '0');
                ++p;
            }
        }
        while (*p == 'l' || *p == 'h' || *p == 'z' || *p == 't' || *p == 'j' || *p == 'L')
            ++p;
        spec = *p ? *p++ : '\0';

        if (spec == 'n' || spec == 'N') {
            number_t value = va_arg(ap_local, number_t);
            core = (num_is_exact(value) && !number_vt(&value)->complex)
                ? num_to_string(value)
                : number_format_inexact(&value, spec == 'N', precision);
        } else if (spec == 'd' || spec == 'i') {
            snprintf(tmp, sizeof(tmp), "%d", va_arg(ap_local, int));
            core = number_strdup(tmp);
        } else if (spec == 'u') {
            snprintf(tmp, sizeof(tmp), "%u", va_arg(ap_local, unsigned int));
            core = number_strdup(tmp);
        } else if (spec == 'f' || spec == 'g' || spec == 'e' || spec == 'E') {
            double value = va_arg(ap_local, double);
            if (precision >= 0)
                snprintf(tmp, sizeof(tmp), (spec == 'f') ? "%.*f" : (spec == 'g') ? "%.*g" : (spec == 'e') ? "%.*e" : "%.*E",
                         precision, value);
            else
                snprintf(tmp, sizeof(tmp), (spec == 'f') ? "%f" : (spec == 'g') ? "%g" : (spec == 'e') ? "%e" : "%E",
                         value);
            core = number_strdup(tmp);
        } else if (spec == 'c') {
            tmp[0] = (char)va_arg(ap_local, int);
            tmp[1] = '\0';
            core = number_strdup(tmp);
        } else if (spec == 's') {
            const char *value = va_arg(ap_local, const char *);
            core = number_strdup(value ? value : "(null)");
        } else if (spec == 'p') {
            snprintf(tmp, sizeof(tmp), "%p", va_arg(ap_local, void *));
            core = number_strdup(tmp);
        } else {
            tmp[0] = '%';
            tmp[1] = spec ? spec : '\0';
            tmp[2] = '\0';
            core = number_strdup(tmp);
        }

        if (!core) {
            va_end(ap_local);
            return -1;
        }

        if ((plus || space) && core[0] != '-' &&
            (spec == 'n' || spec == 'N' || spec == 'f' || spec == 'g' || spec == 'e' || spec == 'E' || spec == 'd' || spec == 'i')) {
            char *prefixed = malloc(strlen(core) + 2u);
            if (!prefixed) {
                free(core);
                va_end(ap_local);
                return -1;
            }
            prefixed[0] = plus ? '+' : ' ';
            strcpy(prefixed + 1, core);
            free(core);
            core = prefixed;
        }
        core_len = (int)strlen(core);
        pad = width > core_len ? width - core_len : 0;
        if (!left) {
            char fill = zero ? '0' : ' ';
            while (pad-- > 0) {
                if (out && pos + 1u < out_size)
                    out[pos] = fill;
                ++pos;
                ++total;
            }
        }
        for (int i = 0; i < core_len; ++i) {
            if (out && pos + 1u < out_size)
                out[pos] = core[i];
            ++pos;
            ++total;
        }
        if (left) {
            pad = width > core_len ? width - core_len : 0;
            while (pad-- > 0) {
                if (out && pos + 1u < out_size)
                    out[pos] = ' ';
                ++pos;
                ++total;
            }
        }
        free(core);
        (void)alt;
    }
    if (out_size > 0u) {
        if (out) {
            size_t term = pos < out_size ? pos : out_size - 1u;
            out[term] = '\0';
        }
    }
    va_end(ap_local);
    return total;
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

int num_printf(const char *fmt, ...)
{
    int needed;
    int written;
    char *buf;
    va_list ap;

    va_start(ap, fmt);
    needed = num_vsprintf(NULL, 0u, fmt, ap);
    va_end(ap);
    if (needed < 0)
        return needed;
    buf = malloc((size_t)needed + 1u);
    if (!buf)
        return -1;
    va_start(ap, fmt);
    written = num_vsprintf(buf, (size_t)needed + 1u, fmt, ap);
    va_end(ap);
    if (written >= 0)
        fputs(buf, stdout);
    free(buf);
    return written;
}
