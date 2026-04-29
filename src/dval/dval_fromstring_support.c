#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qfloat.h"
#include "qcomplex.h"
#include "dval_fromstring_internal.h"

void *fs_xmalloc(size_t n)
{
    void *p = malloc(n);

    if (!p) {
        fprintf(stderr, "dval_from_string: out of memory\n");
        abort();
    }
    return p;
}

int fs_utf8_decode(const char *s, unsigned int *out)
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
        *out = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        return 3;
    }
    return -1;
}

int fs_is_letter(unsigned int c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
        return 1;
    if (c >= 0x0391 && c <= 0x03A9)
        return 1;
    if (c >= 0x03B1 && c <= 0x03C9)
        return 1;
    return 0;
}

void skip_spaces(const char **pp, const char *end)
{
    while (*pp < end && isspace((unsigned char)**pp))
        (*pp)++;
}

size_t scan_decimal_len(const char *s, const char *end)
{
    const char *p = s;
    int ndigits = 0;
    int frac_digits = 0;

    if (p < end && (*p == '-' || *p == '+'))
        p++;
    while (p < end && isdigit((unsigned char)*p)) {
        p++;
        ndigits++;
    }
    if (p < end && *p == '.') {
        p++;
        while (p < end && isdigit((unsigned char)*p)) {
            p++;
            frac_digits++;
        }
    }
    if (ndigits == 0 && frac_digits == 0)
        return 0;

    if (p < end && (*p == 'e' || *p == 'E')) {
        const char *exp_start = p;
        int exp_digits = 0;

        p++;
        if (p < end && (*p == '+' || *p == '-'))
            p++;
        while (p < end && isdigit((unsigned char)*p)) {
            p++;
            exp_digits++;
        }
        if (exp_digits == 0)
            return (size_t)(exp_start - s);
    }

    return (size_t)(p - s);
}

int read_superscript(const char **pp)
{
    const char *p = *pp;
    unsigned int c;
    int len = fs_utf8_decode(p, &c);
    int digit;
    int val;

    if (len <= 0)
        return -1;

    if      (c == 0x00B2) digit = 2;
    else if (c == 0x00B3) digit = 3;
    else if (c == 0x00B9) digit = 1;
    else if (c == 0x2070) digit = 0;
    else if (c >= 0x2074 && c <= 0x2079) digit = (int)(c - 0x2074 + 4);
    else return -1;

    p += len;
    val = digit;
    for (;;) {
        len = fs_utf8_decode(p, &c);
        if (len <= 0)
            break;
        if      (c == 0x00B2) digit = 2;
        else if (c == 0x00B3) digit = 3;
        else if (c == 0x00B9) digit = 1;
        else if (c == 0x2070) digit = 0;
        else if (c >= 0x2074 && c <= 0x2079) digit = (int)(c - 0x2074 + 4);
        else break;
        val = val * 10 + digit;
        p += len;
    }
    *pp = p;
    return val;
}

char *read_simple_name(const char **pp)
{
    const char *p = *pp;
    unsigned int c;
    int len = fs_utf8_decode(p, &c);
    char buf[256];
    int blen = 0;

    if (len <= 0 || !fs_is_letter(c))
        return NULL;

    memcpy(buf, p, (size_t)len);
    blen = len;
    p += len;

    for (;;) {
        unsigned int sc;
        int sl = fs_utf8_decode(p, &sc);

        if (sl > 0 && sc >= 0x2080 && sc <= 0x2089) {
            if (blen + sl >= (int)sizeof(buf) - 1)
                break;
            memcpy(buf + blen, p, (size_t)sl);
            blen += sl;
            p += sl;
            continue;
        }
        if (*p == '_' && (unsigned char)p[1] >= '0' && (unsigned char)p[1] <= '9') {
            int d;

            if (blen + 3 >= (int)sizeof(buf) - 1)
                break;
            d = p[1] - '0';
            buf[blen++] = (char)0xE2;
            buf[blen++] = (char)0x82;
            buf[blen++] = (char)(0x80 + d);
            p += 2;
            continue;
        }
        break;
    }
    buf[blen] = '\0';

    char *result = (char *)fs_xmalloc((size_t)blen + 1);
    memcpy(result, buf, (size_t)blen + 1);
    *pp = p;
    return result;
}

char *read_bracketed_name(const char **pp)
{
    const char *p = *pp;
    const char *start;
    size_t n;
    char *buf;

    if (*p != '[')
        return NULL;

    p++;
    start = p;
    while (*p && *p != ']')
        p++;
    if (*p != ']')
        return NULL;

    n = (size_t)(p - start);
    buf = (char *)fs_xmalloc(n + 1);
    memcpy(buf, start, n);
    buf[n] = '\0';
    *pp = p + 1;
    return buf;
}

char *read_any_name(const char **pp)
{
    if (**pp == '[')
        return read_bracketed_name(pp);
    return read_simple_name(pp);
}

int fs_is_default_constant_name(const char *name)
{
    const char *p = name;
    unsigned int c;
    int len;

    if (!name || !*name)
        return 0;
    if (*name != 'a' && *name != 'b' && *name != 'c' && *name != 'd')
        return 0;

    p++;
    len = fs_utf8_decode(p, &c);
    if (*p == '\0')
        return 1;
    if (len > 0 && c >= 0x2080 && c <= 0x2089)
        return 1;
    if (*p == '_' && p[1] >= '0' && p[1] <= '9')
        return 1;
    return 0;
}

char *fs_normalise_binding_name(const char *name)
{
    const char *s;
    const char *e;
    char *out;
    size_t out_len = 0;

    if (!name)
        return NULL;

    s = name;
    while (*s && isspace((unsigned char)*s))
        s++;
    e = name + strlen(name);
    while (e > s && isspace((unsigned char)e[-1]))
        e--;
    if (e == s)
        return NULL;

    if ((size_t)(e - s) >= 2 && s[0] == '[' && e[-1] == ']') {
        out = (char *)fs_xmalloc((size_t)(e - s - 1));
        memcpy(out, s + 1, (size_t)(e - s - 2));
        out[e - s - 2] = '\0';
        return out;
    }

    out = (char *)fs_xmalloc((size_t)(e - s) * 3 + 1);
    while (s < e) {
        if (*s == '_' && s + 1 < e && s[1] >= '0' && s[1] <= '9') {
            int d = s[1] - '0';
            out[out_len++] = (char)0xE2;
            out[out_len++] = (char)0x82;
            out[out_len++] = (char)(0x80 + d);
            s += 2;
            continue;
        }
        out[out_len++] = *s++;
    }
    out[out_len] = '\0';
    return out;
}

void symtab_init(symtab_t *t)
{
    t->entries = NULL;
    t->count = 0;
    t->cap = 0;
}

int symtab_has(const symtab_t *t, const char *name)
{
    if (!t)
        return 0;
    for (int i = 0; i < t->count; i++)
        if (strcmp(t->entries[i].name, name) == 0)
            return 1;
    return 0;
}

void symtab_add(symtab_t *t, const char *name, dval_t *node)
{
    size_t nl;

    if (t->count == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 8;
        t->entries = (sym_t *)realloc(t->entries, (size_t)t->cap * sizeof(sym_t));
        if (!t->entries) {
            fprintf(stderr, "dval_from_string: OOM\n");
            abort();
        }
    }

    nl = strlen(name) + 1;
    t->entries[t->count].name = (char *)fs_xmalloc(nl);
    memcpy(t->entries[t->count].name, name, nl);
    t->entries[t->count].node = node;
    t->count++;
}

dval_t *symtab_lookup(const symtab_t *t, const char *name)
{
    if (!t)
        return NULL;
    for (int i = 0; i < t->count; i++)
        if (strcmp(t->entries[i].name, name) == 0)
            return t->entries[i].node;
    return NULL;
}

void symtab_free(symtab_t *t)
{
    for (int i = 0; i < t->count; i++) {
        free(t->entries[i].name);
        dv_free(t->entries[i].node);
    }
    free(t->entries);
    symtab_init(t);
}

int symtab_add_borrowed(symtab_t *t, const char *name, dval_t *node)
{
    if (!t || !name || !node)
        return -1;

    dv_retain(node);
    symtab_add(t, name, node);
    return 0;
}

dval_binding_t *symtab_build_bindings(const symtab_t *t, size_t *number_out)
{
    dval_binding_t *bindings;
    char *name_store;
    size_t total_name_bytes = 0;

    if (number_out)
        *number_out = 0;
    if (!t || t->count <= 0)
        return NULL;

    for (int i = 0; i < t->count; ++i)
        total_name_bytes += strlen(t->entries[i].name) + 1;

    bindings = calloc(1, sizeof(*bindings) * (size_t)t->count + total_name_bytes);
    if (!bindings)
        return NULL;

    name_store = (char *)(bindings + t->count);
    for (int i = 0; i < t->count; ++i) {
        size_t n = strlen(t->entries[i].name) + 1;
        memcpy(name_store, t->entries[i].name, n);
        bindings[i].name = name_store;
        bindings[i].symbol = t->entries[i].node;
        bindings[i].is_constant = (t->entries[i].node &&
                                   t->entries[i].node->ops == &ops_const);
        name_store += n;
    }

    if (number_out)
        *number_out = (size_t)t->count;
    return bindings;
}

dval_binding_t *single_binding_from_node(dval_t *node, size_t *number_out)
{
    dval_binding_t *bindings;
    size_t n;

    if (number_out)
        *number_out = 0;
    if (!node || !node->name || !*node->name)
        return NULL;

    n = strlen(node->name) + 1;
    bindings = calloc(1, sizeof(*bindings) + n);
    if (!bindings)
        return NULL;

    bindings[0].name = (char *)(bindings + 1);
    memcpy((char *)bindings[0].name, node->name, n);
    bindings[0].symbol = node;
    bindings[0].is_constant = (node->ops == &ops_const);
    if (number_out)
        *number_out = 1;
    return bindings;
}
