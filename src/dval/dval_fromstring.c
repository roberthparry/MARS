/* dval_fromstring.c - construct a dval_t from an expression-style string
 *
 * Accepts strings in the format produced by dv_to_string(f, style_EXPRESSION):
 *
 *   { expr }
 *   { expr | x₀ = val, ...; [name] = val, ... }
 *
 * The parser also accepts a legacy pure named constant form:
 *
 *   { name = val }
 *
 * Variables appear before the ';' in the binding section; named constants
 * appear after it. If there is no ';', all bindings are treated as variables.
 * If the binding section begins with ';', all bindings are treated as named
 * constants. If there is no binding section and the expression still contains
 * symbolic names, the parser infers variables and named constants from common
 * mathematical conventions and initialises every discovered symbol to NaN.
 * The parser also accepts the following ASCII alternatives for convenience:
 *
 *   _N          subscript digit N (0–9), normalised to U+2080+N internally
 *               so x_0 and x₀ are interchangeable within the same string
 *
 *   *           explicit multiplication in place of middle-dot (·) or
 *               implicit juxtaposition; spaces around '*' are permitted
 *
 *   ^N          integer exponent on a function name (sin^2, cos^3, …) or
 *               on a sub-expression, in place of Unicode superscripts
 *               (², ³, …)
 *
 * Bracketed names ([my var], [2pi], …) are supported for identifiers that
 * do not fit the single-letter-plus-subscript rule.
 *
 * Returns an owning dval_t* on success, NULL on parse error (details written
 * to stderr).  The caller must call dv_free() on the returned pointer exactly
 * once.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "qfloat.h"
#include "qcomplex.h"
#include "dval_internal.h"
#include "dval.h"

/* ------------------------------------------------------------------ */
/* Utilities                                                            */
/* ------------------------------------------------------------------ */

static void *fs_xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "dval_from_string: out of memory\n"); abort(); }
    return p;
}

static int fs_utf8_decode(const char *s, unsigned int *out)
{
    const unsigned char *p = (const unsigned char *)s;
    if (p[0] < 0x80) { *out = p[0]; return 1; }
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

static int fs_is_letter(unsigned int c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) return 1;
    if (c >= 0x0391 && c <= 0x03A9) return 1;  /* Greek uppercase */
    if (c >= 0x03B1 && c <= 0x03C9) return 1;  /* Greek lowercase */
    return 0;
}

static void skip_spaces(const char **pp, const char *end)
{
    while (*pp < end && isspace((unsigned char)**pp))
        (*pp)++;
}

static size_t scan_decimal_len(const char *s, const char *end)
{
    const char *p = s;
    int ndigits = 0;
    int frac_digits = 0;

    if (p < end && (*p == '-' || *p == '+')) p++;
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
    if (ndigits == 0 && frac_digits == 0) {
        return 0;
    }
    if (p < end && (*p == 'e' || *p == 'E')) {
        const char *exp_start = p;
        int exp_digits = 0;

        p++;
        if (p < end && (*p == '+' || *p == '-')) p++;
        while (p < end && isdigit((unsigned char)*p)) {
            p++;
            exp_digits++;
        }
        if (exp_digits == 0) {
            return (size_t)(exp_start - s);
        }
    }
    return (size_t)(p - s);
}

/* ------------------------------------------------------------------ */
/* Superscript digit reading                                            */
/* ------------------------------------------------------------------ */

/* Reads one or more consecutive superscript digits from *pp and advances *pp.
 * Supported: ¹ ² ³ ⁰ ⁴–⁹  (U+00B9, U+00B2, U+00B3, U+2070, U+2074–U+2079).
 * Returns the accumulated integer value, or -1 if no superscript is found. */
static int read_superscript(const char **pp)
{
    const char *p = *pp;
    unsigned int c;
    int len = fs_utf8_decode(p, &c);
    if (len <= 0) return -1;

    int digit;
    if      (c == 0x00B2) digit = 2;
    else if (c == 0x00B3) digit = 3;
    else if (c == 0x00B9) digit = 1;
    else if (c == 0x2070) digit = 0;
    else if (c >= 0x2074 && c <= 0x2079) digit = (int)(c - 0x2074 + 4);
    else return -1;

    p += len;
    int val = digit;
    for (;;) {
        len = fs_utf8_decode(p, &c);
        if (len <= 0) break;
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

/* ------------------------------------------------------------------ */
/* Name reading                                                         */
/* ------------------------------------------------------------------ */

/* Read a simple name (Unicode letter + optional subscript digits).
 * Subscripts may be Unicode U+2080–U+2089 or ASCII _0–_9 (normalised to
 * U+2080–U+2089 in the returned string so both forms produce the same key).
 * Returns a newly allocated string, or NULL if not a valid name start. */
static char *read_simple_name(const char **pp)
{
    const char *p = *pp;
    unsigned int c;
    int len = fs_utf8_decode(p, &c);
    if (len <= 0 || !fs_is_letter(c)) return NULL;

    /* Build the normalised name into a local buffer.
     * Initial letter: ≤4 bytes.  Each subscript: ≤3 bytes.  256 is ample. */
    char buf[256];
    int blen = 0;

    memcpy(buf, p, (size_t)len);
    blen = len;
    p += len;

    for (;;) {
        /* Unicode subscript digit U+2080–U+2089: copy verbatim */
        unsigned int sc;
        int sl = fs_utf8_decode(p, &sc);
        if (sl > 0 && sc >= 0x2080 && sc <= 0x2089) {
            if (blen + sl >= (int)sizeof(buf) - 1) break;
            memcpy(buf + blen, p, (size_t)sl);
            blen += sl;
            p += sl;
            continue;
        }
        /* ASCII _N (N = 0–9): normalise to Unicode subscript U+2080+N */
        if (*p == '_' && (unsigned char)p[1] >= '0' && (unsigned char)p[1] <= '9') {
            if (blen + 3 >= (int)sizeof(buf) - 1) break;
            int d = p[1] - '0';
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

/* Read a bracketed name [content].
 * Returns the content (without brackets) as a newly allocated string,
 * or NULL if the current position isn't '['. */
static char *read_bracketed_name(const char **pp)
{
    const char *p = *pp;
    if (*p != '[') return NULL;
    p++;
    const char *start = p;
    while (*p && *p != ']') p++;
    if (*p != ']') return NULL;
    size_t n = (size_t)(p - start);
    char *buf = (char *)fs_xmalloc(n + 1);
    memcpy(buf, start, n);
    buf[n] = '\0';
    *pp = p + 1;
    return buf;
}

/* Read a simple or bracketed name from *pp. */
static char *read_any_name(const char **pp)
{
    if (**pp == '[') return read_bracketed_name(pp);
    return read_simple_name(pp);
}

static int fs_is_default_constant_name(const char *name)
{
    const char *p = name;
    unsigned int c;
    int len;

    if (!name || !*name)
        return 0;

    if (*name != 'a' &&
        *name != 'b' &&
        *name != 'c' &&
        *name != 'd') {
        return 0;
    }
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

static char *fs_normalise_binding_name(const char *name)
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

/* ------------------------------------------------------------------ */
/* Symbol table                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    char   *name;   /* owned copy of the (possibly normalised) name */
    dval_t *node;   /* owned dval_t* */
} sym_t;

typedef struct {
    sym_t *entries;
    int    count;
    int    cap;
} symtab_t;

static void symtab_init(symtab_t *t)
{
    t->entries = NULL;
    t->count   = 0;
    t->cap     = 0;
}

/* Returns 1 if name is already in the table, 0 otherwise. */
static int symtab_has(const symtab_t *t, const char *name)
{
    if (!t) return 0;
    for (int i = 0; i < t->count; i++)
        if (strcmp(t->entries[i].name, name) == 0)
            return 1;
    return 0;
}

/* Takes ownership of node; copies name. */
static void symtab_add(symtab_t *t, const char *name, dval_t *node)
{
    if (t->count == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 8;
        t->entries = (sym_t *)realloc(t->entries, (size_t)t->cap * sizeof(sym_t));
        if (!t->entries) { fprintf(stderr, "dval_from_string: OOM\n"); abort(); }
    }
    size_t nl = strlen(name) + 1;
    t->entries[t->count].name = (char *)fs_xmalloc(nl);
    memcpy(t->entries[t->count].name, name, nl);
    t->entries[t->count].node = node;
    t->count++;
}

/* Returns a borrowed pointer (do not free). */
static dval_t *symtab_lookup(const symtab_t *t, const char *name)
{
    if (!t) return NULL;
    for (int i = 0; i < t->count; i++)
        if (strcmp(t->entries[i].name, name) == 0)
            return t->entries[i].node;
    return NULL;
}

static void symtab_free(symtab_t *t)
{
    for (int i = 0; i < t->count; i++) {
        free(t->entries[i].name);
        dv_free(t->entries[i].node);
    }
    free(t->entries);
    symtab_init(t);
}

static int symtab_add_borrowed(symtab_t *t, const char *name, dval_t *node)
{
    if (!t || !name || !node)
        return -1;

    dv_retain(node);
    symtab_add(t, name, node);
    return 0;
}

static dval_binding_t *symtab_build_bindings(const symtab_t *t, size_t *number_out)
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

static dval_binding_t *single_binding_from_node(dval_t *node, size_t *number_out)
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

/* ------------------------------------------------------------------ */
/* Parser state                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *p;        /* current scan position */
    const char *end;      /* one past last character of the expression region */
    symtab_t   *syms;
    int         error;
    char        errmsg[256];
} parser_t;

static void set_error(parser_t *p, const char *msg)
{
    if (!p->error) {
        p->error = 1;
        snprintf(p->errmsg, sizeof(p->errmsg), "%s", msg);
    }
}

/* True if we're at the middle dot · (U+00B7, UTF-8: 0xC2 0xB7). */
static int at_middle_dot(const parser_t *p)
{
    return p->p + 1 < p->end &&
           (unsigned char)p->p[0] == 0xC2 &&
           (unsigned char)p->p[1] == 0xB7;
}

/* True if the current position can start a new multiplication factor.
 * Spaces are NOT skipped — they only appear before binary '+'/'-'. */
static int can_start_factor(const parser_t *p)
{
    if (p->p >= p->end) return 0;
    unsigned char c = (unsigned char)*p->p;
    if (c == ')' || c == '}' || c == ',' || c == ';' || c == '|') return 0;
    if (c == ' ') return 0;
    if (c == '[' || c == '(' || c == '-') return 1;
    if (at_middle_dot(p)) return 1;
    unsigned int uc;
    int len = fs_utf8_decode(p->p, &uc);
    if (len > 0 && fs_is_letter(uc)) return 1;
    if (isdigit(c) || c == '.') return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Forward declarations                                                 */
/* ------------------------------------------------------------------ */

static dval_t *parse_addexpr(parser_t *p);

/* ------------------------------------------------------------------ */
/* Function dispatch                                                    */
/* ------------------------------------------------------------------ */

typedef dval_t *(*unary_fn)(dval_t *);
typedef dval_t *(*binary_fn)(dval_t *, dval_t *);

/* ------------------------------------------------------------------ */
/* Function dispatch                                                   */
/* ------------------------------------------------------------------ */

/* Collision-free direct hash table for the current 36 function keywords.
 * The salted hash uses modulus 54, and the highest occupied slot is 51, so
 * the backing table itself only needs 52 entries. */
#define FUNC_HT_MODULUS  54
#define FUNC_HT_SIZE     52

typedef struct {
    const char *kw;
    size_t      klen;
    bool        is_binary;
    unary_fn    ufn;
    binary_fn   bfn;
} func_entry_t;

static const func_entry_t s_funcs[FUNC_HT_SIZE] = {
    [0]  = { "atanh",          5, false, dv_atanh,         NULL        },
    [1]  = { "E1",             2, false, dv_e1,            NULL        },
    [2]  = { "acosh",          5, false, dv_acosh,         NULL        },
    [6]  = { "erf",            3, false, dv_erf,           NULL        },
    [8]  = { "atan2",          5, true,  NULL,             dv_atan2    },
    [9]  = { "lambert_w0",    10, false, dv_lambert_w0,    NULL        },
    [10] = { "log",            3, false, dv_log,           NULL        },
    [12] = { "erfc",           4, false, dv_erfc,          NULL        },
    [13] = { "erfinv",         6, false, dv_erfinv,        NULL        },
    [14] = { "cosh",           4, false, dv_cosh,          NULL        },
    [15] = { "Ei",             2, false, dv_ei,            NULL        },
    [16] = { "trigamma",       8, false, dv_trigamma,      NULL        },
    [17] = { "cos",            3, false, dv_cos,           NULL        },
    [18] = { "logbeta",        7, true,  NULL,             dv_logbeta  },
    [21] = { "tan",            3, false, dv_tan,           NULL        },
    [22] = { "exp",            3, false, dv_exp,           NULL        },
    [23] = { "sinh",           4, false, dv_sinh,          NULL        },
    [24] = { "digamma",        7, false, dv_digamma,       NULL        },
    [25] = { "normal_logpdf", 13, false, dv_normal_logpdf, NULL        },
    [26] = { "pow",            3, true,  NULL,             dv_pow      },
    [27] = { "lambert_wm1",   11, false, dv_lambert_wm1,   NULL        },
    [29] = { "lgamma",         6, false, dv_lgamma,        NULL        },
    [30] = { "sin",            3, false, dv_sin,           NULL        },
    [31] = { "abs",            3, false, dv_abs,           NULL        },
    [32] = { "hypot",          5, true,  NULL,             dv_hypot    },
    [34] = { "asinh",          5, false, dv_asinh,         NULL        },
    [35] = { "tanh",           4, false, dv_tanh,          NULL        },
    [37] = { "erfcinv",        7, false, dv_erfcinv,       NULL        },
    [41] = { "normal_cdf",    10, false, dv_normal_cdf,    NULL        },
    [43] = { "sqrt",           4, false, dv_sqrt,          NULL        },
    [44] = { "asin",           4, false, dv_asin,          NULL        },
    [45] = { "beta",           4, true,  NULL,             dv_beta     },
    [46] = { "atan",           4, false, dv_atan,          NULL        },
    [48] = { "gamma",          5, false, dv_gamma,         NULL        },
    [49] = { "acos",           4, false, dv_acos,          NULL        },
    [51] = { "normal_pdf",    10, false, dv_normal_pdf,    NULL        },
};

static unsigned func_ht_hash(const char *s, size_t n)
{
    unsigned h = 230u;

    h += (unsigned)n;
    for (size_t i = 0; i < n; i++) {
        h *= 521u;
        h ^= (unsigned char)s[i];
    }

    h ^= (h >> 13);
    h *= 2654435761u;

    return h % FUNC_HT_MODULUS;
}

static const func_entry_t *lookup_func(const char *kw, size_t klen)
{
    unsigned slot = func_ht_hash(kw, klen);
    if (slot >= FUNC_HT_SIZE) return NULL;
    if (!s_funcs[slot].kw) return NULL;
    if (s_funcs[slot].klen == klen &&
            memcmp(s_funcs[slot].kw, kw, klen) == 0)
        return &s_funcs[slot];
    return NULL;
}

/* Return 1 if the byte sequence at p looks like a superscript codepoint. */
static int is_superscript_byte(const char *p)
{
    unsigned int c;
    int len = fs_utf8_decode(p, &c);
    if (len <= 0) return 0;
    return c == 0x00B2 || c == 0x00B3 || c == 0x00B9 || c == 0x2070 ||
           (c >= 0x2074 && c <= 0x2079);
}

/* Check whether the text at pos starts with keyword kw (length klen) and is
 * immediately followed by '(' optionally preceded by a power marker.
 * Accepted power markers: Unicode superscripts (sin²) or ASCII ^N (sin^2).
 * Returns the position of the opening '(', or NULL if the pattern doesn't match. */
static const char *func_call_start(const char *pos, const char *kw, size_t klen)
{
    if (strncmp(pos, kw, klen) != 0) return NULL;
    const char *after = pos + klen;
    /* Skip Unicode superscript digits (sin²(x) form) */
    while (is_superscript_byte(after)) {
        unsigned int c;
        int len = fs_utf8_decode(after, &c);
        after += len;
    }
    /* Skip ASCII ^N (sin^2(x) form) */
    if (*after == '^' && isdigit((unsigned char)after[1])) {
        after++; /* skip '^' */
        while (isdigit((unsigned char)*after)) after++;
    }
    return (*after == '(') ? after : NULL;
}

/* ------------------------------------------------------------------ */
/* Binary function argument parser helper                               */
/* ------------------------------------------------------------------ */

static int parse_two_args(parser_t *p, dval_t **a_out, dval_t **b_out)
{
    dval_t *a = parse_addexpr(p);
    if (!a) return 0;

    if (p->p >= p->end || *p->p != ',') {
        dv_free(a);
        set_error(p, "expected ',' in binary function");
        return 0;
    }
    p->p++;
    skip_spaces(&p->p, p->end);

    dval_t *b = parse_addexpr(p);
    if (!b) { dv_free(a); return 0; }

    *a_out = a;
    *b_out = b;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Atom parser                                                          */
/* ------------------------------------------------------------------ */

static dval_t *parse_atom(parser_t *p)
{
    if (p->error || p->p >= p->end) {
        set_error(p, "unexpected end of expression");
        return NULL;
    }

    /* Parenthesised sub-expression */
    if (*p->p == '(') {
        p->p++;
        dval_t *inner = parse_addexpr(p);
        if (!inner) return NULL;
        if (p->p >= p->end || *p->p != ')') {
            dv_free(inner);
            set_error(p, "expected ')'");
            return NULL;
        }
        p->p++;
        return inner;
    }

    /* Decimal number (starts with digit or '.') */
    if (isdigit((unsigned char)*p->p) || *p->p == '.') {
        const char *start = p->p;
        size_t n = scan_decimal_len(start, p->end);
        p->p = start + n;
        char nbuf[64];
        if (n >= sizeof(nbuf)) n = sizeof(nbuf) - 1;
        memcpy(nbuf, start, n);
        nbuf[n] = '\0';
        return dv_new_const(qf_from_string(nbuf));
    }

    /* Function keywords — O(1) hash lookup.  We read the ASCII identifier at
     * the current position (letters, digits, underscores; stops before UTF-8
     * superscripts and '^'), look it up in the hash table, then confirm that
     * '(' (optionally preceded by a superscript) follows. */
    {
        const char *id = p->p;
        const char *id_end = id;
        while (id_end < p->end &&
               (isalpha((unsigned char)*id_end) ||
                isdigit((unsigned char)*id_end) ||
                *id_end == '_'))
            id_end++;
        size_t id_len = (size_t)(id_end - id);

        if (id_len > 0) {
            const func_entry_t *fe = lookup_func(id, id_len);
            if (fe) {
                const char *paren = func_call_start(p->p, fe->kw, fe->klen);
                if (paren) {
                    /* Read optional exponent between keyword and '('. */
                    const char *after_kw = p->p + fe->klen;
                    int sup = read_superscript(&after_kw);
                    if (sup < 0 && after_kw[0] == '^' &&
                            isdigit((unsigned char)after_kw[1])) {
                        after_kw++;
                        sup = 0;
                        while (isdigit((unsigned char)*after_kw))
                            sup = sup * 10 + (*after_kw++ - '0');
                    }
                    (void)after_kw;

                    p->p = paren + 1; /* skip past '(' */

                    if (fe->is_binary) {
                        dval_t *a = NULL, *b = NULL;
                        if (!parse_two_args(p, &a, &b)) return NULL;
                        if (p->p >= p->end || *p->p != ')') {
                            dv_free(a); dv_free(b);
                            set_error(p, "expected ')' after binary function");
                            return NULL;
                        }
                        p->p++;
                        dval_t *result = fe->bfn(a, b);
                        dv_free(a); dv_free(b);
                        if (sup >= 0) {
                            dval_t *tmp = dv_pow_d(result, (double)sup);
                            dv_free(result);
                            result = tmp;
                        }
                        return result;
                    } else {
                        dval_t *arg = parse_addexpr(p);
                        if (!arg) return NULL;
                        if (p->p >= p->end || *p->p != ')') {
                            dv_free(arg);
                            set_error(p, "expected ')' after function argument");
                            return NULL;
                        }
                        p->p++;
                        dval_t *result = fe->ufn(arg);
                        dv_free(arg);
                        if (sup >= 0) {
                            dval_t *tmp = dv_pow_d(result, (double)sup);
                            dv_free(result);
                            result = tmp;
                        }
                        return result;
                    }
                }
            }
        }
    }

    /* Simple name (single Unicode letter + subscript digits) or [bracketed name] */
    char *name = read_any_name(&p->p);
    if (!name) {
        set_error(p, "expected expression");
        return NULL;
    }

    dval_t *sym = symtab_lookup(p->syms, name);
    if (!sym) {
        char msg[256];
        snprintf(msg, sizeof(msg), "unknown symbol '%.200s'", name);
        set_error(p, msg);
        free(name);
        return NULL;
    }
    dv_retain(sym); /* give caller an owning reference */
    free(name);
    return sym;
}

/* ------------------------------------------------------------------ */
/* Power parser                                                         */
/* ------------------------------------------------------------------ */

static dval_t *parse_power(parser_t *p)
{
    if (p->error) return NULL;

    dval_t *base = parse_atom(p);
    if (!base) return NULL;

    /* Unicode superscript exponent: x² */
    int sup = read_superscript(&p->p);
    if (sup >= 0) {
        dval_t *tmp = dv_pow_d(base, (double)sup);
        dv_free(base);
        return tmp;
    }

    /* Caret exponent: x^n or x^(a,b) */
    if (p->p < p->end && *p->p == '^') {
        p->p++;

        /* General pow: ^(a, b) */
        if (p->p < p->end && *p->p == '(') {
            p->p++;
            dval_t *a = NULL, *b = NULL;
            if (!parse_two_args(p, &a, &b)) { dv_free(base); return NULL; }
            if (p->p >= p->end || *p->p != ')') {
                dv_free(base); dv_free(a); dv_free(b);
                set_error(p, "expected ')' after '^' arguments");
                return NULL;
            }
            p->p++;
            /* base is unused here — ^(a, b) is its own expression */
            dv_free(base);
            dval_t *result = dv_pow(a, b);
            dv_free(a); dv_free(b);
            return result;
        }

        /* Numeric exponent: ^3.5 */
        const char *num_start = p->p;
        if (p->p < p->end && (*p->p == '-' || *p->p == '+')) p->p++;
        size_t n = scan_decimal_len(num_start, p->end);
        p->p = num_start + n;
        if (n == 0) {
            dv_free(base);
            set_error(p, "expected exponent after '^'");
            return NULL;
        }
        char nbuf[64];
        if (n >= sizeof(nbuf)) n = sizeof(nbuf) - 1;
        memcpy(nbuf, num_start, n);
        nbuf[n] = '\0';
        double exp_val = atof(nbuf);
        dval_t *tmp = dv_pow_d(base, exp_val);
        dv_free(base);
        return tmp;
    }

    return base;
}

/* ------------------------------------------------------------------ */
/* Signed factor (unary minus)                                         */
/* ------------------------------------------------------------------ */

static dval_t *parse_signed_power(parser_t *p)
{
    if (p->error) return NULL;
    if (p->p < p->end && *p->p == '-') {
        p->p++;
        dval_t *inner = parse_power(p);
        if (!inner) return NULL;
        dval_t *result = dv_neg(inner);
        dv_free(inner);
        return result;
    }
    return parse_power(p);
}

/* ------------------------------------------------------------------ */
/* Multiplication (implicit and '·')                                   */
/* ------------------------------------------------------------------ */

static dval_t *parse_mulexpr(parser_t *p)
{
    if (p->error) return NULL;
    dval_t *lhs = parse_signed_power(p);
    if (!lhs) return NULL;

    for (;;) {
        if (p->p >= p->end) break;

        /* Explicit middle dot '·' */
        if (at_middle_dot(p)) {
            p->p += 2;
            dval_t *rhs = parse_signed_power(p);
            if (!rhs) { dv_free(lhs); return NULL; }
            dval_t *tmp = dv_mul(lhs, rhs);
            dv_free(lhs); dv_free(rhs);
            lhs = tmp;
            continue;
        }

        /* Explicit '*' (ASCII alternative to middle dot): accepted with or
         * without surrounding spaces, e.g. "x*y" and "x * y" both work.
         * Peek past spaces before committing — if '*' is absent we fall
         * through without advancing p->p. */
        {
            const char *peek = p->p;
            while (peek < p->end && *peek == ' ') peek++;
            if (peek < p->end && *peek == '*') {
                p->p = peek + 1; /* consume optional leading spaces and '*' */
                skip_spaces(&p->p, p->end); /* trailing spaces */
                dval_t *rhs = parse_signed_power(p);
                if (!rhs) { dv_free(lhs); return NULL; }
                dval_t *tmp = dv_mul(lhs, rhs);
                dv_free(lhs); dv_free(rhs);
                lhs = tmp;
                continue;
            }
        }

        /* Implicit multiplication: next position can start a factor */
        if (can_start_factor(p)) {
            dval_t *rhs = parse_signed_power(p);
            if (!rhs) { dv_free(lhs); return NULL; }
            dval_t *tmp = dv_mul(lhs, rhs);
            dv_free(lhs); dv_free(rhs);
            lhs = tmp;
            continue;
        }

        break;
    }
    return lhs;
}

/* ------------------------------------------------------------------ */
/* Addition / subtraction                                              */
/* ------------------------------------------------------------------ */

static dval_t *parse_addexpr(parser_t *p)
{
    if (p->error) return NULL;
    dval_t *lhs = parse_mulexpr(p);
    if (!lhs) return NULL;

    for (;;) {
        if (p->p + 2 >= p->end) break;

        if (p->p[0] == ' ' && p->p[1] == '+' && p->p[2] == ' ') {
            p->p += 3;
            dval_t *rhs = parse_mulexpr(p);
            if (!rhs) { dv_free(lhs); return NULL; }
            dval_t *tmp = dv_add(lhs, rhs);
            dv_free(lhs); dv_free(rhs);
            lhs = tmp;
            continue;
        }

        if (p->p[0] == ' ' && p->p[1] == '-' && p->p[2] == ' ') {
            p->p += 3;
            dval_t *rhs = parse_mulexpr(p);
            if (!rhs) { dv_free(lhs); return NULL; }
            dval_t *tmp = dv_sub(lhs, rhs);
            dv_free(lhs); dv_free(rhs);
            lhs = tmp;
            continue;
        }

        break;
    }
    return lhs;
}

/* ------------------------------------------------------------------ */
/* Binding section parser                                               */
/* ------------------------------------------------------------------ */

/* Parse comma-separated "name = value" pairs from [s, end).
 * is_var: 1 → create dv_new_named_var; 0 → create dv_new_named_const.
 * On success returns 0; on failure writes to errmsg and returns -1. */
static int parse_bindings(const char *s, const char *end,
                           int is_var, symtab_t *syms,
                           char *errmsg, size_t errmsg_n)
{
    const char *p = s;
    while (p < end) {
        /* Skip whitespace and commas between entries */
        while (p < end && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (p >= end) break;

        char *name = read_any_name(&p);
        if (!name) {
            snprintf(errmsg, errmsg_n, "expected name in binding section");
            return -1;
        }

        skip_spaces(&p, end);
        if (p >= end || *p != '=') {
            free(name);
            snprintf(errmsg, errmsg_n, "expected '=' after name in binding");
            return -1;
        }
        p++; /* skip '=' */
        skip_spaces(&p, end);

        /* Parse decimal value (may be negative) */
        const char *val_start = p;
        size_t vlen = scan_decimal_len(val_start, end);
        p = val_start + vlen;
        if (vlen == 0) {
            free(name);
            snprintf(errmsg, errmsg_n, "expected numeric value in binding");
            return -1;
        }

        char *vbuf = (char *)fs_xmalloc(vlen + 1);
        memcpy(vbuf, val_start, vlen);
        vbuf[vlen] = '\0';
        qfloat_t val = qf_from_string(vbuf);
        free(vbuf);

        dval_t *node = is_var
            ? dv_new_named_var(val, name)
            : dv_new_named_const(val, name);

        /* dv_new_named_* calls dv_normalize_name, which may transform the name
         * (e.g. "@pi" → "π").  Use the normalised form as the lookup key so it
         * matches what the expression text will contain after its own read_any_name. */
        const char *key = (node->name && *node->name) ? node->name : name;

        /* Detect name clashes — same name used twice, or once as a variable
         * and once as a named constant. */
        if (symtab_has(syms, key)) {
            /* Copy key before freeing node/name — key may alias node->name or name */
            char key_copy[210];
            strncpy(key_copy, key, sizeof(key_copy) - 1);
            key_copy[sizeof(key_copy) - 1] = '\0';
            dv_free(node);
            free(name);
            snprintf(errmsg, errmsg_n, "duplicate name '%.200s' in binding section", key_copy);
            return -1;
        }

        symtab_add(syms, key, node); /* symtab takes ownership of node */
        free(name);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Pure-constant format: { name = val }                                 */
/* ------------------------------------------------------------------ */

static dval_t *parse_pure_const(const char *s, const char *end,
                                 char *errmsg, size_t errmsg_n)
{
    const char *p = s;
    skip_spaces(&p, end);

    char *name = read_any_name(&p);

    skip_spaces(&p, end);
    if (p >= end || *p != '=') {
        free(name);
        snprintf(errmsg, errmsg_n, "expected '=' in constant format");
        return NULL;
    }
    p++;
    skip_spaces(&p, end);

    const char *val_start = p;
    size_t vlen = scan_decimal_len(val_start, end);
    p = val_start + vlen;
    if (vlen == 0) {
        free(name);
        snprintf(errmsg, errmsg_n, "expected value in constant format");
        return NULL;
    }

    char *vbuf = (char *)fs_xmalloc(vlen + 1);
    memcpy(vbuf, val_start, vlen);
    vbuf[vlen] = '\0';
    qfloat_t val = qf_from_string(vbuf);
    free(vbuf);

    if (!name) {
        snprintf(errmsg, errmsg_n, "constant name is required in pure-constant format");
        return NULL;
    }
    dval_t *result = dv_new_named_const(val, name);
    free(name);
    return result;
}

static int has_top_level_equals(const char *start, const char *end)
{
    int depth = 0;
    const char *p = start;

    while (p < end) {
        if (*p == '(' || *p == '[') {
            depth++;
            p++;
            continue;
        }
        if (*p == ')' || *p == ']') {
            depth--;
            p++;
            continue;
        }
        if (depth == 0 && *p == '=')
            return 1;

        {
            unsigned int c;
            int len = fs_utf8_decode(p, &c);
            p += (len > 0) ? len : 1;
        }
    }

    return 0;
}

static int collect_implicit_symbols(const char *start, const char *end,
                                    symtab_t *syms)
{
    const char *p = start;

    while (p < end) {
        char *name = read_any_name(&p);
        dval_t *node;
        int is_const;

        if (!name) {
            unsigned int c;
            int len = fs_utf8_decode(p, &c);
            p += (len > 0) ? len : 1;
            continue;
        }

        if (symtab_has(syms, name)) {
            free(name);
            continue;
        }

        is_const = fs_is_default_constant_name(name);
        node = is_const
            ? dv_new_named_const(QF_NAN, name)
            : dv_new_named_var(QF_NAN, name);
        symtab_add(syms, node->name ? node->name : name, node);
        free(name);
    }

    return 0;
}

static dval_t *parse_expression_region(const char *start,
                                       const char *end,
                                       symtab_t *syms,
                                       const char *context_label,
                                       int report_errors)
{
    parser_t ps;
    dval_t *result;

    if (!start || !end || end < start)
        return NULL;

    while (start < end && isspace((unsigned char)*start))
        start++;
    while (end > start && isspace((unsigned char)end[-1]))
        end--;

    ps.p = start;
    ps.end = end;
    ps.syms = syms;
    ps.error = 0;
    ps.errmsg[0] = '\0';

    result = parse_addexpr(&ps);
    if (result && !ps.error) {
        while (ps.p < end && isspace((unsigned char)*ps.p))
            ps.p++;
        if (ps.p == end)
            return result;
        dv_free(result);
        result = NULL;
        set_error(&ps, "trailing input");
    } else if (result) {
        dv_free(result);
        result = NULL;
    }

    if (ps.error && report_errors)
        fprintf(stderr, "%s: parse error: %s\n", context_label, ps.errmsg);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Public entry point                                                   */
/* ------------------------------------------------------------------ */

static dval_t *dval_from_string_impl(const char *s,
                                     dval_binding_t **bindings_out,
                                     size_t *number_out)
{
    dval_binding_t *bindings = NULL;
    size_t nbindings = 0;

    if (bindings_out)
        *bindings_out = NULL;
    if (number_out)
        *number_out = 0;
    if (!s) return NULL;

    while (isspace((unsigned char)*s)) s++;
    if (*s != '{') {
        fprintf(stderr, "dval_from_string: expected '{'\n");
        return NULL;
    }
    s++;
    while (isspace((unsigned char)*s)) s++;

    /* Scan for '|' and '}', tracking bracket/paren depth so we don't mistake
     * a '|' or '}' inside a bracketed name or parenthesised expression. */
    const char *pipe_pos  = NULL;
    const char *close_pos = NULL;
    int depth = 0;
    const char *scan = s;
    while (*scan) {
        if (*scan == '(' || *scan == '[') { depth++; scan++; continue; }
        if (*scan == ')' || *scan == ']') { depth--; scan++; continue; }
        if (depth == 0) {
            if (*scan == '|' && !pipe_pos) { pipe_pos = scan; scan++; continue; }
            if (*scan == '}') { close_pos = scan; break; }
        }
        unsigned int uc;
        int len = fs_utf8_decode(scan, &uc);
        scan += (len > 0) ? len : 1;
    }

    if (!close_pos) {
        fprintf(stderr, "dval_from_string: expected '}'\n");
        return NULL;
    }

    char errmsg[256] = { 0 };

    /* ---- No bindings: either { expr } or legacy { name = val } ---- */
    if (!pipe_pos) {
        const char *content_end = close_pos;
        symtab_t syms;
        while (content_end > s && isspace((unsigned char)content_end[-1]))
            content_end--;

        dval_t *result = parse_expression_region(s, content_end, NULL,
                                                 "dval_from_string", 0);

        if (result) {
            if (bindings_out) {
                bindings = single_binding_from_node(result, &nbindings);
                *bindings_out = bindings;
            }
            if (number_out)
                *number_out = nbindings;
            return result;
        }

        if (!has_top_level_equals(s, content_end)) {
            symtab_init(&syms);
            if (collect_implicit_symbols(s, content_end, &syms) == 0 &&
                syms.count > 0) {
                result = parse_expression_region(s, content_end, &syms,
                                                 "dval_from_string", 1);
            }
            if (result && bindings_out)
                bindings = symtab_build_bindings(&syms, &nbindings);
            symtab_free(&syms);
            if (result) {
                if (bindings_out)
                    *bindings_out = bindings;
                if (number_out)
                    *number_out = nbindings;
                return result;
            }
        }

        errmsg[0] = '\0';
        result = parse_pure_const(s, content_end, errmsg, sizeof(errmsg));
        if (!result)
            fprintf(stderr, "dval_from_string: %s\n", errmsg);
        else if (bindings_out) {
            bindings = single_binding_from_node(result, &nbindings);
            *bindings_out = bindings;
        }
        if (number_out)
            *number_out = nbindings;
        return result;
    }

    /* ---- Expression with bindings: { expr | vars; consts } ---- */
    const char *expr_end   = pipe_pos;
    const char *bind_start = pipe_pos + 1;
    const char *bind_end   = close_pos;

    /* Trim trailing whitespace from expression region */
    while (expr_end > s && isspace((unsigned char)expr_end[-1]))
        expr_end--;

    /* Split binding section at ';' to separate vars from named consts */
    const char *semi_pos = NULL;
    for (const char *bp = bind_start; bp < bind_end; bp++) {
        if (*bp == ';') { semi_pos = bp; break; }
    }

    symtab_t syms;
    symtab_init(&syms);

    const char *vars_end = semi_pos ? semi_pos : bind_end;
    if (parse_bindings(bind_start, vars_end, 1, &syms, errmsg, sizeof(errmsg)) < 0) {
        symtab_free(&syms);
        fprintf(stderr, "dval_from_string: %s\n", errmsg);
        return NULL;
    }

    if (semi_pos) {
        if (parse_bindings(semi_pos + 1, bind_end, 0, &syms,
                           errmsg, sizeof(errmsg)) < 0) {
            symtab_free(&syms);
            fprintf(stderr, "dval_from_string: %s\n", errmsg);
            return NULL;
        }
    }

    dval_t *result = parse_expression_region(s, expr_end, &syms,
                                             "dval_from_string", 1);
    if (result && bindings_out)
        bindings = symtab_build_bindings(&syms, &nbindings);

    symtab_free(&syms);
    if (result && bindings_out)
        *bindings_out = bindings;
    if (result && number_out)
        *number_out = nbindings;
    return result;
}

dval_t *dval_from_string(const char *s)
{
    return dval_from_string_impl(s, NULL, NULL);
}

dval_t *dval_from_string_with_bindings(const char *s,
                                       dval_binding_t **bindings_out,
                                       size_t *number_out)
{
    return dval_from_string_impl(s, bindings_out, number_out);
}

dval_t *dval_from_expression_string(const char *expr,
                                    const char *const *names,
                                    dval_t *const *symbols,
                                    size_t nsymbols)
{
    symtab_t syms;
    dval_t *result;

    if (!expr)
        return NULL;
    if (nsymbols > 0 && (!names || !symbols)) {
        fprintf(stderr,
                "dval_from_expression_string: symbol table is incomplete\n");
        return NULL;
    }
    if (nsymbols > (size_t)INT_MAX) {
        fprintf(stderr,
                "dval_from_expression_string: too many symbols\n");
        return NULL;
    }

    symtab_init(&syms);
    for (size_t i = 0; i < nsymbols; ++i) {
        if (!names[i] || !symbols[i]) {
            fprintf(stderr,
                    "dval_from_expression_string: null symbol entry\n");
            symtab_free(&syms);
            return NULL;
        }
        if (symtab_has(&syms, names[i])) {
            fprintf(stderr,
                    "dval_from_expression_string: duplicate symbol '%s'\n",
                    names[i]);
            symtab_free(&syms);
            return NULL;
        }
        if (symtab_add_borrowed(&syms, names[i], symbols[i]) != 0) {
            symtab_free(&syms);
            return NULL;
        }
    }

    result = parse_expression_region(expr, expr + strlen(expr),
                                     nsymbols ? &syms : NULL,
                                     "dval_from_expression_string", 1);
    symtab_free(&syms);
    return result;
}

dval_binding_t *dval_binding_get(dval_binding_t *bindings,
                                 size_t number,
                                 const char *name)
{
    char *norm;

    if (!bindings || !name)
        return NULL;

    norm = fs_normalise_binding_name(name);
    if (!norm)
        return NULL;

    for (size_t i = 0; i < number; ++i) {
        if (strcmp(bindings[i].name, norm) == 0) {
            free(norm);
            return &bindings[i];
        }
    }

    free(norm);
    return NULL;
}

dval_binding_t *dval_binding_find(dval_binding_t *bindings,
                                  size_t number,
                                  const char *name)
{
    return dval_binding_get(bindings, number, name);
}

int dval_binding_set_qf(dval_binding_t *bindings,
                        size_t number,
                        const char *name,
                        qfloat_t value)
{
    dval_binding_t *binding = dval_binding_get(bindings, number, name);

    if (!binding)
        return -1;
    dv_set_val_qf(binding->symbol, value);
    return 0;
}

int dval_binding_set_qc(dval_binding_t *bindings,
                        size_t number,
                        const char *name,
                        qcomplex_t value)
{
    dval_binding_t *binding = dval_binding_get(bindings, number, name);

    if (!binding)
        return -1;
    dv_set_val(binding->symbol, value);
    return 0;
}

int dval_binding_set_d(dval_binding_t *bindings,
                       size_t number,
                       const char *name,
                       double value)
{
    dval_binding_t *binding = dval_binding_get(bindings, number, name);

    if (!binding)
        return -1;
    dv_set_val_d(binding->symbol, value);
    return 0;
}
