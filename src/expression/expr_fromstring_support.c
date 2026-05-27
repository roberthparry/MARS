#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qfloat.h"
#include "expr_fromstring.h"

void *fs_xmalloc(size_t n)
{
    void *p = malloc(n);

    if (!p) {
        fprintf(stderr, "out of memory\n");
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
    int allow_alias = 0;

    {
        static const char *builtin_names[] = { "pi", "phi", "gamma" };
        size_t i;

        for (i = 0; i < sizeof(builtin_names) / sizeof(builtin_names[0]); ++i) {
            size_t n = strlen(builtin_names[i]);

            if (strncmp(p, builtin_names[i], n) == 0 &&
                !isalnum((unsigned char)p[n]) && p[n] != '_') {
                char *result = (char *)fs_xmalloc(n + 1u);
                memcpy(result, builtin_names[i], n + 1u);
                *pp = p + n;
                return result;
            }
        }
    }

    if (*p == '@') {
        const char *alias = p + 1;
        const char *accepted[] = { "pi", "phi", "gamma", "tau" };
        size_t i;

        for (i = 0; i < sizeof(accepted) / sizeof(accepted[0]); ++i) {
            size_t n = strlen(accepted[i]);

            unsigned int suffix_cp = 0;
            int suffix_len = fs_utf8_decode(alias + n, &suffix_cp);
            int suffix_is_subscript = suffix_len > 0 &&
                                      suffix_cp >= 0x2080 &&
                                      suffix_cp <= 0x2089;

            if (strncmp(alias, accepted[i], n) == 0 &&
                (!alias[n] ||
                 alias[n] == '_' ||
                 isdigit((unsigned char)alias[n]) ||
                 suffix_is_subscript ||
                 (!isalnum((unsigned char)alias[n]) && alias[n] != '_'))) {
                allow_alias = 1;
                break;
            }
        }

        if (!allow_alias)
            return NULL;

        buf[blen++] = *p++;
        len = fs_utf8_decode(p, &c);
    }

    if (len <= 0 || !fs_is_letter(c))
        return NULL;

    if (allow_alias) {
        while (*p && isalpha((unsigned char)*p)) {
            if (blen + 1 >= (int)sizeof(buf) - 1)
                break;
            buf[blen++] = *p++;
        }
    } else {
        memcpy(buf + blen, p, (size_t)len);
        blen += len;
        p += len;
    }

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
        if (allow_alias && isdigit((unsigned char)*p)) {
            if (blen + 1 >= (int)sizeof(buf) - 1)
                break;
            buf[blen++] = *p++;
            continue;
        }
        if (!allow_alias && isdigit((unsigned char)*p)) {
            int d;

            if (blen + 3 >= (int)sizeof(buf) - 1)
                break;
            d = *p - '0';
            buf[blen++] = (char)0xE2;
            buf[blen++] = (char)0x82;
            buf[blen++] = (char)(0x80 + d);
            p++;
            continue;
        }
        if (allow_alias && *p == '_' && isdigit((unsigned char)p[1])) {
            if (blen + 1 >= (int)sizeof(buf) - 1)
                break;
            buf[blen++] = *p++;
            while (isdigit((unsigned char)*p)) {
                if (blen + 1 >= (int)sizeof(buf) - 1)
                    break;
                buf[blen++] = *p++;
            }
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
    static const char *special_names[] = {
        "@pi", "@phi", "@gamma", "@tau", "pi"
    };
    size_t i;

    if (**pp == '[')
        return read_bracketed_name(pp);

    for (i = 0; i < sizeof(special_names) / sizeof(special_names[0]); ++i) {
        size_t n = strlen(special_names[i]);

        if (strncmp(*pp, special_names[i], n) == 0 &&
            !isalnum((unsigned char)(*pp)[n]) && (*pp)[n] != '_') {
            char *result = (char *)fs_xmalloc(n + 1);
            memcpy(result, special_names[i], n + 1);
            *pp += n;
            return result;
        }
    }

    return read_simple_name(pp);
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

void symtab_add(symtab_t *t, const char *name, expr_t *node)
{
    size_t nl;

    if (t->count == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 8;
        t->entries = (sym_t *)realloc(t->entries, (size_t)t->cap * sizeof(sym_t));
        if (!t->entries) {
            fprintf(stderr, "out of memory\n");
            abort();
        }
    }

    nl = strlen(name) + 1;
    t->entries[t->count].name = (char *)fs_xmalloc(nl);
    memcpy(t->entries[t->count].name, name, nl);
    t->entries[t->count].node = node;
    t->count++;
}

expr_t *symtab_lookup(const symtab_t *t, const char *name)
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
        expr_free(t->entries[i].node);
    }
    free(t->entries);
    symtab_init(t);
}

int symtab_add_borrowed(symtab_t *t, const char *name, expr_t *node)
{
    if (!t || !name || !node)
        return -1;

    expr_retain(node);
    symtab_add(t, name, node);
    return 0;
}

static size_t binding_name_hash(const void *key)
{
    const unsigned char *s = (const unsigned char *)*(const char * const *)key;
    size_t hash = 1469598103934665603ull;

    while (*s) {
        hash ^= (size_t)*s++;
        hash *= 1099511628211ull;
    }
    return hash;
}

static int binding_name_cmp(const void *a, const void *b)
{
    const char *ka = *(const char * const *)a;
    const char *kb = *(const char * const *)b;

    return strcmp(ka, kb);
}

static dictionary_t *binding_index_create(void)
{
    return dictionary_create(sizeof(char *),
                             sizeof(expr_binding_entry_t *),
                             binding_name_hash,
                             binding_name_cmp,
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             NULL);
}

static void bindings_destroy_partial(expr_bindings_t *bindings)
{
    if (!bindings)
        return;
    if (bindings->entries) {
        for (size_t i = 0; i < bindings->count; ++i)
            expr_free(bindings->entries[i].expr);
    }
    dictionary_destroy(bindings->index);
    free(bindings->storage);
    free(bindings);
}

static expr_bindings_t *bindings_create(size_t count, size_t total_name_bytes)
{
    expr_bindings_t *bindings = calloc(1, sizeof(*bindings));

    if (!bindings)
        return NULL;

    bindings->storage = calloc(1, sizeof(bindings->entries[0]) * count +
                                  total_name_bytes);
    bindings->index = binding_index_create();
    if (!bindings->storage || !bindings->index) {
        bindings_destroy_partial(bindings);
        return NULL;
    }

    bindings->count = count;
    bindings->entries = (expr_binding_entry_t *)bindings->storage;
    return bindings;
}

static int bindings_index_entry(expr_bindings_t *bindings,
                                expr_binding_entry_t *entry)
{
    return dictionary_set(bindings->index, &entry->name, &entry) ? 0 : -1;
}

expr_bindings_t *symtab_build_bindings(const symtab_t *t)
{
    expr_bindings_t *bindings;
    char *name_store;
    size_t total_name_bytes = 0;

    if (!t || t->count <= 0)
        return NULL;

    for (int i = 0; i < t->count; ++i)
        total_name_bytes += strlen(t->entries[i].name) + 1;

    bindings = bindings_create((size_t)t->count, total_name_bytes);
    if (!bindings)
        return NULL;
    name_store = (char *)(bindings->entries + t->count);
    for (int i = 0; i < t->count; ++i) {
        expr_binding_entry_t *entry;
        size_t n = strlen(t->entries[i].name) + 1;

        memcpy(name_store, t->entries[i].name, n);
        entry = &bindings->entries[i];
        entry->name = name_store;
        entry->expr = t->entries[i].node;
        expr_retain(entry->expr);
        entry->is_constant = (t->entries[i].node &&
                              t->entries[i].node->ops == &ops_const);
        if (bindings_index_entry(bindings, entry) != 0) {
            bindings_destroy_partial(bindings);
            return NULL;
        }
        name_store += n;
    }

    return bindings;
}

expr_bindings_t *single_binding_from_node(expr_t *node)
{
    expr_bindings_t *bindings;
    size_t n;

    if (!node || !node->name || !*node->name)
        return NULL;

    n = strlen(node->name) + 1;
    bindings = bindings_create(1u, n);
    if (!bindings)
        return NULL;
    bindings->entries[0].name = (char *)(bindings->entries + 1);
    memcpy((char *)bindings->entries[0].name, node->name, n);
    bindings->entries[0].expr = node;
    expr_retain(bindings->entries[0].expr);
    bindings->entries[0].is_constant = (node->ops == &ops_const);
    {
        expr_binding_entry_t *entry = &bindings->entries[0];

        if (bindings_index_entry(bindings, entry) != 0) {
            bindings_destroy_partial(bindings);
            return NULL;
        }
    }
    return bindings;
}
