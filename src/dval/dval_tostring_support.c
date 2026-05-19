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

int dv_tostring_is_negative_const(const dval_t *f)
{
    if (!dv_is_unnamed_const(f))
        return 0;
    return num_is_real(f->c) && num_get_sign(f->c) < 0;
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

    if (dv_tostring_is_simple_name(name)) {
        sbuf_puts(b, name);
    } else {
        sbuf_putc(b, '[');
        sbuf_puts(b, name);
        sbuf_putc(b, ']');
    }
}

static int dv_tostring_is_superscript_cp(unsigned int c)
{
    return (c >= 0x2070 && c <= 0x2079) || c == 0x00B9 || c == 0x00B2 || c == 0x00B3 ||
           c == 0x207A || c == 0x207B || c == 0x207C || c == 0x207D || c == 0x207E;
}

static int dv_tostring_is_subscript_cp(unsigned int c)
{
    return (c >= 0x2080 && c <= 0x2089) ||
           c == 0x208A || c == 0x208B || c == 0x208C || c == 0x208D || c == 0x208E;
}

typedef struct {
    unsigned int codepoint;
    const char *text;
} dv_tostring_tex_map_t;

#define DV_TEX_HASH_GREEK_SIZE       19u
#define DV_TEX_HASH_FRACTION_SIZE    15u
#define DV_TEX_HASH_SYMBOL_SIZE      9u

static const dv_tostring_tex_map_t dv_tostring_greek_tex_table[DV_TEX_HASH_GREEK_SIZE] = {
    [0]  = {0x03A6, "\\Phi"},
    [3]  = {0x03A0, "\\Pi"},
    [4]  = {0x03A3, "\\Sigma"},
    [5]  = {0x03C4, "\\tau"},
    [6]  = {0x03BB, "\\lambda"},
    [7]  = {0x03C6, "\\phi"},
    [9]  = {0x03B8, "\\theta"},
    [10] = {0x03C0, "\\pi"},
    [11] = {0x03C3, "\\sigma"},
    [12] = {0x03BC, "\\mu"},
    [13] = {0x03D5, "\\phi"},
    [14] = {0x03B1, "\\alpha"},
    [15] = {0x03A9, "\\Omega"},
    [16] = {0x03B3, "\\gamma"},
    [17] = {0x03B2, "\\beta"},
    [18] = {0x03B4, "\\delta"}
};

static const dv_tostring_tex_map_t dv_tostring_vulgar_fraction_tex_table[DV_TEX_HASH_FRACTION_SIZE] = {
    [0]  = {0x2157, "\\frac{3}{5}"},
    [1]  = {0x2158, "\\frac{4}{5}"},
    [2]  = {0x2159, "\\frac{1}{6}"},
    [3]  = {0x215A, "\\frac{5}{6}"},
    [4]  = {0x215B, "\\frac{1}{8}"},
    [5]  = {0x215C, "\\frac{3}{8}"},
    [6]  = {0x215D, "\\frac{5}{8}"},
    [7]  = {0x215E, "\\frac{7}{8}"},
    [8]  = {0x00BC, "\\frac{1}{4}"},
    [9]  = {0x00BD, "\\frac{1}{2}"},
    [10] = {0x00BE, "\\frac{3}{4}"},
    [11] = {0x2153, "\\frac{1}{3}"},
    [12] = {0x2154, "\\frac{2}{3}"},
    [13] = {0x2155, "\\frac{1}{5}"},
    [14] = {0x2156, "\\frac{2}{5}"}
};

static const dv_tostring_tex_map_t dv_tostring_symbol_tex_table[DV_TEX_HASH_SYMBOL_SIZE] = {
    [0]  = {0x2260, "\\neq"},
    [1]  = {0x2202, "\\partial"},
    [2]  = {0x221E, "\\infty"},
    [3]  = {0x00B7, " \\cdot "},
    [4]  = {0x2264, "\\leq"},
    [5]  = {0x2265, "\\geq"},
    [7]  = {0x221A, "\\sqrt{}"},
    [8]  = {0x00D7, " \\times "}
};

static size_t dv_tostring_greek_hash(unsigned int cp)
{
    return (cp ^ (cp >> 2) ^ 37u) % DV_TEX_HASH_GREEK_SIZE;
}

static size_t dv_tostring_fraction_hash(unsigned int cp)
{
    return cp % DV_TEX_HASH_FRACTION_SIZE;
}

static size_t dv_tostring_symbol_hash(unsigned int cp)
{
    return (cp ^ (cp >> 12)) % DV_TEX_HASH_SYMBOL_SIZE;
}

static const char *dv_tostring_tex_lookup(const dv_tostring_tex_map_t *table,
                                          size_t idx,
                                          unsigned int cp)
{
    return table[idx].codepoint == cp ? table[idx].text : NULL;
}

static char dv_tostring_superscript_ascii(unsigned int c)
{
    if (c >= 0x2074 && c <= 0x2079)
        return (char)('4' + (c - 0x2074));

    switch (c) {
        case 0x2070: return '0';
        case 0x00B9: return '1';
        case 0x00B2: return '2';
        case 0x00B3: return '3';
        case 0x207A: return '+';
        case 0x207B: return '-';
        case 0x207C: return '=';
        case 0x207D: return '(';
        case 0x207E: return ')';
        default:     return '\0';
    }
}

static char dv_tostring_subscript_ascii(unsigned int c)
{
    if (c >= 0x2080 && c <= 0x2089)
        return (char)('0' + (c - 0x2080));

    switch (c) {
        case 0x208A: return '+';
        case 0x208B: return '-';
        case 0x208C: return '=';
        case 0x208D: return '(';
        case 0x208E: return ')';
        default:     return '\0';
    }
}

static int dv_tostring_utf8_encode(unsigned int cp, char out[5])
{
    if (cp < 0x80) {
        out[0] = (char)cp;
        out[1] = '\0';
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        out[2] = '\0';
        return 2;
    }

    out[0] = (char)(0xE0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (char)(0x80 | (cp & 0x3F));
    out[3] = '\0';
    return 3;
}

static const char *dv_tostring_greek_tex(unsigned int cp)
{
    return dv_tostring_tex_lookup(dv_tostring_greek_tex_table,
                                  dv_tostring_greek_hash(cp),
                                  cp);
}

static const char *dv_tostring_vulgar_fraction_tex(unsigned int cp)
{
    return dv_tostring_tex_lookup(dv_tostring_vulgar_fraction_tex_table,
                                  dv_tostring_fraction_hash(cp),
                                  cp);
}

static const char *dv_tostring_symbol_tex(unsigned int cp)
{
    return dv_tostring_tex_lookup(dv_tostring_symbol_tex_table,
                                  dv_tostring_symbol_hash(cp),
                                  cp);
}

static int dv_tostring_is_ascii_letter(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static const char *dv_tostring_collect_superscript(const char *p, sbuf_t *tmp)
{
    while (*p) {
        unsigned int cp;
        int len = dv_tostring_utf8_decode(p, &cp);
        char mapped;

        if (len <= 0 || !dv_tostring_is_superscript_cp(cp))
            break;

        mapped = dv_tostring_superscript_ascii(cp);
        if (mapped == '\0')
            break;

        sbuf_putc(tmp, mapped);
        p += len;
    }

    return p;
}

static const char *dv_tostring_collect_subscript(const char *p, sbuf_t *tmp)
{
    while (*p) {
        unsigned int cp;
        int len = dv_tostring_utf8_decode(p, &cp);
        char mapped;

        if (len <= 0 || !dv_tostring_is_subscript_cp(cp))
            break;

        mapped = dv_tostring_subscript_ascii(cp);
        if (mapped == '\0')
            break;

        sbuf_putc(tmp, mapped);
        p += len;
    }

    return p;
}

char *dv_tostring_texify(const char *text)
{
    sbuf_t out;
    const char *p = text ? text : "";

    sbuf_init(&out);

    while (*p) {
        unsigned int cp;
        int len = dv_tostring_utf8_decode(p, &cp);
        const char *mapped;
        const char *next;
        char utf8_buf[5];
        sbuf_t seq;

        if (len <= 0) {
            sbuf_putc(&out, *p++);
            continue;
        }

        mapped = dv_tostring_vulgar_fraction_tex(cp);
        if (mapped) {
            sbuf_puts(&out, mapped);
            p += len;
            continue;
        }

        if (dv_tostring_is_superscript_cp(cp)) {
            sbuf_t num;
            sbuf_t den;
            unsigned int slash_cp;
            int slash_len;

            sbuf_init(&num);
            next = dv_tostring_collect_superscript(p, &num);
            slash_len = dv_tostring_utf8_decode(next, &slash_cp);
            if (slash_len > 0 && slash_cp == 0x2044) {
                sbuf_init(&den);
                next = dv_tostring_collect_subscript(next + slash_len, &den);
                if (den.len > 0) {
                    sbuf_puts(&out, "\\frac{");
                    sbuf_puts(&out, num.data);
                    sbuf_puts(&out, "}{");
                    sbuf_puts(&out, den.data);
                    sbuf_putc(&out, '}');
                    sbuf_free(&num);
                    sbuf_free(&den);
                    p = next;
                    continue;
                }
                sbuf_free(&den);
            }

            sbuf_puts(&out, "^{");
            sbuf_puts(&out, num.data);
            sbuf_putc(&out, '}');
            sbuf_free(&num);
            p = next;
            continue;
        }

        if (dv_tostring_is_subscript_cp(cp)) {
            sbuf_init(&seq);
            next = dv_tostring_collect_subscript(p, &seq);
            sbuf_puts(&out, "_{");
            sbuf_puts(&out, seq.data);
            sbuf_putc(&out, '}');
            sbuf_free(&seq);
            p = next;
            continue;
        }

        mapped = dv_tostring_greek_tex(cp);
        if (mapped) {
            sbuf_puts(&out, mapped);
            if (dv_tostring_is_ascii_letter(p[len]))
                sbuf_puts(&out, "{}");
            p += len;
            continue;
        }

        mapped = dv_tostring_symbol_tex(cp);
        if (mapped) {
            sbuf_puts(&out, mapped);
            p += len;
            continue;
        }

        dv_tostring_utf8_encode(cp, utf8_buf);
        sbuf_puts(&out, utf8_buf);
        p += len;
    }

    return out.data;
}
