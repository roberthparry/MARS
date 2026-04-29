#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dval_tostring_internal.h"

void *dv_tostring_xmalloc(size_t n)
{
    void *p = malloc(n);

    if (!p) {
        fprintf(stderr, "dv_to_string: out of memory\n");
        abort();
    }
    return p;
}

char *dv_tostring_xstrdup(const char *s)
{
    size_t n;
    char *p;

    if (!s)
        return NULL;

    n = strlen(s) + 1;
    p = (char *)dv_tostring_xmalloc(n);
    memcpy(p, s, n);
    return p;
}

void sbuf_init(sbuf_t *b)
{
    b->cap = 128;
    b->len = 0;
    b->data = (char *)dv_tostring_xmalloc(b->cap);
    b->data[0] = '\0';
}

void sbuf_free(sbuf_t *b)
{
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

void sbuf_reserve(sbuf_t *b, size_t extra)
{
    size_t ncap;
    char *ndata;

    if (b->len + extra + 1 <= b->cap)
        return;

    ncap = b->cap * 2;
    while (ncap < b->len + extra + 1)
        ncap *= 2;

    ndata = (char *)dv_tostring_xmalloc(ncap);
    memcpy(ndata, b->data, b->len + 1);
    free(b->data);
    b->data = ndata;
    b->cap = ncap;
}

void sbuf_putc(sbuf_t *b, char c)
{
    sbuf_reserve(b, 1);
    b->data[b->len++] = c;
    b->data[b->len] = '\0';
}

void sbuf_puts(sbuf_t *b, const char *s)
{
    size_t n;

    if (!s)
        return;

    n = strlen(s);
    sbuf_reserve(b, n);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

int dv_tostring_utf8_decode(const char *s, unsigned int *out)
{
    const unsigned char *p = (const unsigned char *)s;

    if (p[0] < 0x80) {
        *out = p[0];
        return 1;
    }
    if ((p[0] & 0xE0) == 0xC0) {
        *out = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        return 2;
    }
    if ((p[0] & 0xF0) == 0xE0) {
        *out = ((p[0] & 0x0F) << 12) |
               ((p[1] & 0x3F) << 6) |
               (p[2] & 0x3F);
        return 3;
    }
    return -1;
}

void qf_to_string_simple(qcomplex_t v, char *buf, size_t n)
{
    if (n == 0)
        return;

    if (qc_eq(v, QC_ZERO)) {
        snprintf(buf, n, "0");
        return;
    }

    if (qf_eq(v.im, QF_ZERO)) {
        qf_sprintf(buf, n, "%q", v.re);
        return;
    }

    if (qf_eq(v.re, QF_ZERO)) {
        char imag[128];
        size_t len;

        qf_sprintf(imag, sizeof(imag), "%q", v.im);
        len = strlen(imag);
        if (len + 2 > n) {
            if (n > 0)
                buf[0] = '\0';
            return;
        }
        memcpy(buf, imag, len);
        buf[len] = 'i';
        buf[len + 1] = '\0';
        return;
    }

    qc_sprintf(buf, n, "%z", v);
}

int dv_tostring_is_negative_const(const dval_t *f)
{
    return dv_is_const(f) &&
           qf_eq(f->c.im, QF_ZERO) &&
           qf_to_double(f->c.re) < 0.0;
}

int dv_tostring_is_var_pow_d(const dval_t *f)
{
    return dv_is_pow_d_expr(f) && dv_is_var(f->a);
}

int dv_tostring_is_unicode_letter(unsigned int c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
        return 1;
    if (c >= 0x0391 && c <= 0x03A9)
        return 1;
    if (c >= 0x03B1 && c <= 0x03C9)
        return 1;
    return 0;
}

int dv_tostring_is_simple_name(const char *name)
{
    const char *p;
    unsigned int c;
    int len;

    if (!name || !*name)
        return 0;

    len = dv_tostring_utf8_decode(name, &c);
    if (len <= 0 || !dv_tostring_is_unicode_letter(c))
        return 0;

    if (name[len] == '\0')
        return 1;

    p = name + len;
    while (*p) {
        unsigned int sc;
        int sl = dv_tostring_utf8_decode(p, &sc);
        if (sl <= 0 || sc < 0x2080 || sc > 0x2089)
            return 0;
        p += sl;
    }
    return 1;
}

void emit_name(sbuf_t *b, const char *name)
{
    if (!name || !*name) {
        sbuf_puts(b, "x");
        return;
    }

    if (dv_tostring_is_simple_name(name)) {
        sbuf_puts(b, name);
    } else {
        sbuf_putc(b, '[');
        sbuf_puts(b, name);
        sbuf_putc(b, ']');
    }
}

int dv_tostring_is_safe_func_name(const char *name)
{
    const char *p;
    unsigned int c;
    int len;

    if (!name || !*name)
        return 0;

    len = dv_tostring_utf8_decode(name, &c);
    if (len <= 0 || !dv_tostring_is_unicode_letter(c))
        return 0;

    p = name + len;
    while (*p) {
        unsigned int sc;
        int sl = dv_tostring_utf8_decode(p, &sc);
        if (sl <= 0)
            return 0;
        if (!dv_tostring_is_unicode_letter(sc) &&
            !(sc >= '0' && sc <= '9') &&
            !(sc >= 0x2080 && sc <= 0x2089))
            return 0;
        p += sl;
    }
    return 1;
}

void emit_name_func(sbuf_t *b, const char *name)
{
    if (!name || !*name) {
        sbuf_puts(b, "x");
        return;
    }

    if (dv_tostring_is_safe_func_name(name)) {
        sbuf_puts(b, name);
    } else {
        sbuf_putc(b, '[');
        sbuf_puts(b, name);
        sbuf_putc(b, ']');
    }
}
