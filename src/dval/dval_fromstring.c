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
#include "dval_internal.h"
#include "dval_fromstring_internal.h"
#include "dval.h"
#include "internal/number_internal.h"

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

static size_t scan_unicode_fraction_len(const char *s, const char *end);

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
    if (c == '[' || c == '(' || c == '-' || c == '@') return 1;
    if (at_middle_dot(p)) return 1;
    if (scan_unicode_fraction_len(p->p, p->end) > 0u) return 1;
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

typedef dval_t *(*unary_fn)(const dval_t *);
typedef dval_t *(*binary_fn)(const dval_t *, const dval_t *);

/* ------------------------------------------------------------------ */
/* Function dispatch                                                   */
/* ------------------------------------------------------------------ */

/* Sorted function keyword table.
 *
 * With only 40 supported keywords, a compact sorted table plus binary search
 * is smaller and easier to maintain than a sparse direct-hash array.  A small
 * number of Unicode round-trip aliases are handled outside this table. */
#define FUNC_TABLE_SIZE 40

typedef struct {
    const char *kw;
    size_t      klen;
    bool        is_binary;
    unary_fn    ufn;
    binary_fn   bfn;
} func_entry_t;

static const unsigned char s_func_displacements[FUNC_TABLE_SIZE] = {
    1, 0, 1, 0, 0, 0, 1, 6, 0, 6,
    7, 0, 2, 0, 3, 0, 0, 3, 3, 0,
    5, 6, 2, 0, 0, 5, 0, 2, 0, 23,
    0, 0, 3, 6, 0, 0, 0, 0, 14, 23
};

static const func_entry_t s_funcs[FUNC_TABLE_SIZE] = {
    { "cos",            3, false, dv_cos,           NULL        },
    { "atan2",          5, true,  NULL,             dv_atan2    },
    { "log10",          5, false, dv_log10,         NULL        },
    { "acos",           4, false, dv_acos,          NULL        },
    { "productlog",    10, false, dv_lambert_w0,    NULL        },
    { "logbeta",        7, true,  NULL,             dv_logbeta  },
    { "atanh",          5, false, dv_atanh,         NULL        },
    { "lambert_wm1",   11, false, dv_lambert_wm1,   NULL        },
    { "tan",            3, false, dv_tan,           NULL        },
    { "atan",           4, false, dv_atan,          NULL        },
    { "sin",            3, false, dv_sin,           NULL        },
    { "erf",            3, false, dv_erf,           NULL        },
    { "normal_cdf",    10, false, dv_normal_cdf,    NULL        },
    { "gammainv",       8, false, dv_gammainv,      NULL        },
    { "normal_logpdf", 13, false, dv_normal_logpdf, NULL        },
    { "tanh",           4, false, dv_tanh,          NULL        },
    { "digamma",        7, false, dv_digamma,       NULL        },
    { "exp",            3, false, dv_exp,           NULL        },
    { "trigamma",       8, false, dv_trigamma,      NULL        },
    { "lambert_w0",    10, false, dv_lambert_w0,    NULL        },
    { "sinh",           4, false, dv_sinh,          NULL        },
    { "lgamma",         6, false, dv_lgamma,        NULL        },
    { "E1",             2, false, dv_e1,            NULL        },
    { "log",            3, false, dv_log10,         NULL        },
    { "erfinv",         6, false, dv_erfinv,        NULL        },
    { "beta",           4, true,  NULL,             dv_beta     },
    { "gamma",          5, false, dv_gamma,         NULL        },
    { "Ei",             2, false, dv_ei,            NULL        },
    { "asinh",          5, false, dv_asinh,         NULL        },
    { "erfcinv",        7, false, dv_erfcinv,       NULL        },
    { "sqrt",           4, false, dv_sqrt,          NULL        },
    { "asin",           4, false, dv_asin,          NULL        },
    { "abs",            3, false, dv_abs,           NULL        },
    { "hypot",          5, true,  NULL,             dv_hypot    },
    { "acosh",          5, false, dv_acosh,         NULL        },
    { "ln",             2, false, dv_log,           NULL        },
    { "normal_pdf",    10, false, dv_normal_pdf,    NULL        },
    { "pow",            3, true,  NULL,             dv_pow_dv   },
    { "erfc",           4, false, dv_erfc,          NULL        },
    { "cosh",           4, false, dv_cosh,          NULL        },
};

static const func_entry_t s_func_alias_w0 = {
    "W₀", sizeof("W₀") - 1u, false, dv_lambert_w0, NULL
};

static const func_entry_t s_func_alias_wm1 = {
    "W₋₁", sizeof("W₋₁") - 1u, false, dv_lambert_wm1, NULL
};

static unsigned func_bucket_hash(const char *kw, size_t klen)
{
    const unsigned char *s = (const unsigned char *)kw;

    return (unsigned)(klen + s[0] + 3u * s[klen - 1u]) % FUNC_TABLE_SIZE;
}

static unsigned func_slot_hash(const char *kw, size_t klen)
{
    const unsigned char *s = (const unsigned char *)kw;
    unsigned h = 7u * s[0] + s[klen - 1u];

    for (size_t i = 0; i < klen; i++)
        h += (unsigned)(i + 1u) * s[i];

    return h % FUNC_TABLE_SIZE;
}

static const func_entry_t *lookup_func(const char *kw, size_t klen)
{
    const func_entry_t *entry;
    unsigned bucket;
    unsigned slot;

    if (klen == 0u)
        return NULL;

    bucket = func_bucket_hash(kw, klen);
    slot = (func_slot_hash(kw, klen) + s_func_displacements[bucket]) % FUNC_TABLE_SIZE;
    entry = &s_funcs[slot];

    if (entry->klen == klen && memcmp(kw, entry->kw, klen) == 0)
        return entry;

    if (klen == s_func_alias_w0.klen &&
        memcmp(kw, s_func_alias_w0.kw, klen) == 0)
        return &s_func_alias_w0;

    if (klen == s_func_alias_wm1.klen &&
        memcmp(kw, s_func_alias_wm1.kw, klen) == 0)
        return &s_func_alias_wm1;

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

static int scan_utf8_codepoint(const char *p, const char *end, unsigned int *out)
{
    int len;

    if (p >= end)
        return 0;
    len = fs_utf8_decode(p, out);
    if (len <= 0 || p + len > end)
        return 0;
    return len;
}

static int is_superscript_digit_codepoint(unsigned int c)
{
    return c == 0x00B2 || c == 0x00B3 || c == 0x00B9 || c == 0x2070 ||
           (c >= 0x2074 && c <= 0x2079);
}

static int is_subscript_digit_codepoint(unsigned int c)
{
    return c >= 0x2080 && c <= 0x2089;
}

static int is_fraction_glyph_codepoint(unsigned int c)
{
    return c == 0x00BC || c == 0x00BD || c == 0x00BE ||
           (c >= 0x2150 && c <= 0x215E);
}

static size_t scan_unicode_fraction_len(const char *s, const char *end)
{
    const char *p = s;
    unsigned int c;
    int len;
    int digits = 0;

    len = scan_utf8_codepoint(p, end, &c);
    if (len <= 0)
        return 0u;

    if (is_fraction_glyph_codepoint(c))
        return (size_t)len;

    while ((len = scan_utf8_codepoint(p, end, &c)) > 0 &&
           is_superscript_digit_codepoint(c)) {
        p += len;
        ++digits;
    }
    if (digits == 0)
        return 0u;

    len = scan_utf8_codepoint(p, end, &c);
    if (len <= 0 || c != 0x2044)
        return 0u;
    p += len;

    digits = 0;
    while ((len = scan_utf8_codepoint(p, end, &c)) > 0 &&
           is_subscript_digit_codepoint(c)) {
        p += len;
        ++digits;
    }
    if (digits == 0)
        return 0u;

    return (size_t)(p - s);
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

static const func_entry_t *lookup_unicode_func_alias(const char *pos, const char **paren_out)
{
    static const func_entry_t *const aliases[] = {
        &s_func_alias_w0,
        &s_func_alias_wm1,
    };
    size_t i;

    if (paren_out)
        *paren_out = NULL;

    for (i = 0u; i < sizeof(aliases) / sizeof(aliases[0]); ++i) {
        const char *paren = func_call_start(pos, aliases[i]->kw, aliases[i]->klen);

        if (!paren)
            continue;

        if (paren_out)
            *paren_out = paren;
        return aliases[i];
    }

    return NULL;
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

static int parse_number_literal(const char **p_in, const char *end, number_t *out)
{
    const char *start = *p_in;
    size_t len = scan_decimal_len(start, end);

    if (len == 0)
        return 0;

    char *buf = (char *)fs_xmalloc(len + 1);
    memcpy(buf, start, len);
    buf[len] = '\0';
    *out = num_create_from_string(buf);
    free(buf);
    *p_in = start + len;
    return 1;
}

static size_t scan_number_atom_len(const char *s, const char *end)
{
    size_t len = scan_decimal_len(s, end);
    const char *p;
    size_t tail;

    if (len == 0) {
        len = scan_unicode_fraction_len(s, end);
        if (len == 0)
            return 0;
        p = s + len;
        if (p < end && (*p == 'i' || *p == 'I'))
            p++;
        return (size_t)(p - s);
    }

    p = s + len;
    if (p < end && *p == '/') {
        tail = scan_decimal_len(p + 1, end);
        if (tail == 0)
            return len;
        p += 1 + tail;
    }

    if (p < end && (*p == 'i' || *p == 'I'))
        p++;

    return (size_t)(p - s);
}

static int parse_number_region(const char *start, const char *end, number_t *out)
{
    char *buf;
    size_t len;
    char *roundtrip;

    while (start < end && isspace((unsigned char)*start))
        start++;
    while (end > start && isspace((unsigned char)end[-1]))
        end--;

    if (start >= end)
        return 0;

    len = (size_t)(end - start);
    buf = (char *)fs_xmalloc(len + 1);
    memcpy(buf, start, len);
    buf[len] = '\0';

    *out = num_create_from_string(buf);
    free(buf);

    roundtrip = num_to_string(*out);
    if (!roundtrip) {
        num_destroy(out);
        return 0;
    }

    free(roundtrip);
    return 1;
}

static const char *scan_binding_value_end(const char *p, const char *end)
{
    int depth = 0;

    while (p < end) {
        if (*p == '(' || *p == '[') {
            depth++;
            p++;
            continue;
        }
        if ((*p == ')' || *p == ']') && depth > 0) {
            depth--;
            p++;
            continue;
        }
        if (depth == 0 && (*p == ',' || *p == ';'))
            break;

        {
            unsigned int c;
            int len = fs_utf8_decode(p, &c);
            p += (len > 0) ? len : 1;
        }
    }

    return p;
}

static int read_optional_display_exponent(const char **p_in)
{
    const char *p = *p_in;
    int exponent = read_superscript(&p);

    if (exponent < 0 && p[0] == '^' && isdigit((unsigned char)p[1])) {
        p++;
        exponent = 0;
        while (isdigit((unsigned char)*p))
            exponent = exponent * 10 + (*p++ - '0');
    }

    *p_in = p;
    return exponent;
}

static int parse_required_char(parser_t *p, char expected, const char *errmsg)
{
    if (p->p >= p->end || *p->p != expected) {
        set_error(p, errmsg);
        return 0;
    }
    p->p++;
    return 1;
}

static dval_t *apply_integer_power_if_present(dval_t *value, int exponent)
{
    NUM_SCOPE(scope);
    if (exponent < 0)
        return value;

    number_t exponent_num = num_create_from_long(exponent);
    dval_t *powered = dv_pow(value, &exponent_num);
    dv_free(value);
    return powered;
}

static dval_t *parse_enclosed_addexpr(parser_t *p, char closing, const char *errmsg)
{
    dval_t *inner = parse_addexpr(p);

    if (!inner)
        return NULL;
    if (!parse_required_char(p, closing, errmsg)) {
        dv_free(inner);
        return NULL;
    }
    return inner;
}

/* ------------------------------------------------------------------ */
/* Atom parser                                                          */
/* ------------------------------------------------------------------ */

static dval_t *parse_atom(parser_t *p)
{
    NUM_SCOPE(scope);
    unsigned int cp = 0;
    int cp_len = fs_utf8_decode(p->p, &cp);

    if (p->error || p->p >= p->end) {
        set_error(p, "unexpected end of expression");
        return NULL;
    }

    /* Parenthesised sub-expression */
    if (*p->p == '(') {
        p->p++;
        return parse_enclosed_addexpr(p, ')', "expected ')'");
    }

    /* Absolute-value bars: |expr| */
    if (*p->p == '|') {
        p->p++;
        dval_t *inner = parse_enclosed_addexpr(p, '|', "expected '|'");
        if (!inner)
            return NULL;
        dval_t *result = dv_abs(inner);
        dv_free(inner);
        return result;
    }

    /* Numeric atom (integer/decimal/rational, optionally with trailing i) */
    if (isdigit((unsigned char)*p->p) || *p->p == '.' ||
        scan_unicode_fraction_len(p->p, p->end) > 0u) {
        size_t len = scan_number_atom_len(p->p, p->end);
        number_t value;
        dval_t *node;

        if (len == 0 || !parse_number_region(p->p, p->p + len, &value)) {
            set_error(p, "expected numeric literal");
            return NULL;
        }
        p->p += len;
        node = dv_new_const(value);
        return node;
    }

    if (cp_len > 0 && cp == 0x221A) {
        p->p += cp_len;
        int sup = read_optional_display_exponent(&p->p);

        if (!parse_required_char(p, '(', "expected '(' after √"))
            return NULL;

        dval_t *arg = parse_enclosed_addexpr(p, ')', "expected ')' after √ argument");
        dval_t *result;
        if (!arg)
            return NULL;
        result = dv_sqrt(arg);
        dv_free(arg);
        return apply_integer_power_if_present(result, sup);
    }

    {
        const char *paren = NULL;
        const func_entry_t *fe = lookup_unicode_func_alias(p->p, &paren);

        if (fe && paren) {
            const char *after_kw = p->p + fe->klen;
            int sup = read_optional_display_exponent(&after_kw);
            (void)after_kw;

            p->p = paren + 1;

            dval_t *arg = parse_enclosed_addexpr(
                p, ')', "expected ')' after function argument");
            dval_t *result;

            if (!arg)
                return NULL;
            result = fe->ufn(arg);
            dv_free(arg);
            return apply_integer_power_if_present(result, sup);
        }
    }

    /* Function keywords — O(1) hash lookup.  We read the ASCII identifier at
     * the current position (letters, digits, underscores; stops before UTF-8
     * superscripts and '^'), look it up in the hash table, then confirm that
     * '(' (optionally preceded by a superscript) follows. */
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
                const char *after_kw = p->p + fe->klen;
                int sup = read_optional_display_exponent(&after_kw);
                (void)after_kw;

                p->p = paren + 1; /* skip past '(' */

                if (fe->is_binary) {
                    dval_t *a = NULL, *b = NULL;
                    if (!parse_two_args(p, &a, &b)) return NULL;
                    if (!parse_required_char(p, ')', "expected ')' after binary function")) {
                        dv_free(a);
                        dv_free(b);
                        return NULL;
                    }
                    dval_t *result = fe->bfn(a, b);
                    dv_free(a);
                    dv_free(b);
                    return apply_integer_power_if_present(result, sup);
                } else {
                    dval_t *arg = parse_enclosed_addexpr(
                        p, ')', "expected ')' after function argument");
                    dval_t *result;
                    if (!arg)
                        return NULL;
                    result = fe->ufn(arg);
                    dv_free(arg);
                    return apply_integer_power_if_present(result, sup);
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
        char *normalized = dv_normalize_name(dv_default_constant_canonical_name(name));

        if (normalized) {
            sym = symtab_lookup(p->syms, normalized);
            free(normalized);
        }
    }
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
    NUM_SCOPE(scope);
    if (p->error) return NULL;

    dval_t *base = parse_atom(p);
    if (!base) return NULL;

    /* Unicode superscript exponent: x² */
    int sup = read_superscript(&p->p);
    if (sup >= 0)
        return apply_integer_power_if_present(base, sup);

    /* Caret exponent: x^n or x^(a,b) */
    if (p->p < p->end && *p->p == '^') {
        p->p++;

        /* General pow: ^(a, b) */
        if (p->p < p->end && *p->p == '(') {
            p->p++;
            dval_t *a = NULL, *b = NULL;
            if (!parse_two_args(p, &a, &b)) { dv_free(base); return NULL; }
            if (!parse_required_char(p, ')', "expected ')' after '^' arguments")) {
                dv_free(base);
                dv_free(a);
                dv_free(b);
                return NULL;
            }
            /* base is unused here — ^(a, b) is its own expression */
            dv_free(base);
            dval_t *result = dv_pow_dv(a, b);
            dv_free(a); dv_free(b);
            return result;
        }

        /* Numeric exponent: ^3.5 */
        number_t exponent_num;
        if (!parse_number_literal(&p->p, p->end, &exponent_num)) {
            dv_free(base);
            set_error(p, "expected exponent after '^'");
            return NULL;
        }
        dval_t *tmp = dv_pow(base, &exponent_num);
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
 * is_var: 1 → create dv_new_named_var(); 0 → create dv_new_named_const().
 * On success returns 0; on failure writes to errmsg and returns -1. */
static int parse_bindings(const char *s, const char *end,
                           int is_var, symtab_t *syms,
                           char *errmsg, size_t errmsg_n)
{
    NUM_SCOPE(scope);
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

        const char *value_end = scan_binding_value_end(p, end);
        number_t val;
        dval_t *node;

        if (!parse_number_region(p, value_end, &val)) {
            free(name);
            snprintf(errmsg, errmsg_n, "expected numeric value in binding");
            return -1;
        }
        p = value_end;

        node = is_var
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
    NUM_SCOPE(scope);
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

    number_t val;
    if (!parse_number_region(p, end, &val)) {
        free(name);
        snprintf(errmsg, errmsg_n, "expected value in constant format");
        return NULL;
    }

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
    NUM_SCOPE(scope);
    const char *p = start;

    while (p < end) {
        char *name = read_any_name(&p);
        dval_t *node;
        int is_const;
        number_t value;
        const char *canonical_name = name;
        char *key = NULL;

        if (!name) {
            unsigned int c;
            int len = fs_utf8_decode(p, &c);
            p += (len > 0) ? len : 1;
            continue;
        }

        is_const = dv_is_default_constant_name(name);
        if (dv_get_default_constant_num(name, &value)) {
            canonical_name = dv_default_constant_canonical_name(name);
            node = dv_new_named_const(value, canonical_name);
        } else {
            node = is_const
                ? dv_new_named_const(NUM_NAN, name)
                : dv_new_named_var(NUM_NAN, name);
        }

        key = dv_normalize_name(canonical_name);
        if (!key)
            key = strdup(canonical_name);
        if (!key) {
            dv_free(node);
            free(name);
            return -1;
        }

        if (symtab_has(syms, key)) {
            free(key);
            dv_free(node);
            free(name);
            continue;
        }

        symtab_add(syms, key, node);
        free(key);
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
                                     dval_bindings_t **bindings_out)
{
    dval_bindings_t *bindings = NULL;

    if (bindings_out)
        *bindings_out = NULL;
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
            if (*scan == '|') { pipe_pos = scan; scan++; continue; }
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

    if (pipe_pos && !has_top_level_equals(pipe_pos + 1, close_pos))
        pipe_pos = NULL;

    /* ---- No bindings: either { expr } or legacy { name = val } ---- */
    if (!pipe_pos) {
        const char *content_end = close_pos;
        symtab_t syms;
        while (content_end > s && isspace((unsigned char)content_end[-1]))
            content_end--;

        dval_t *result = parse_expression_region(s, content_end, NULL,
                                                 "dval_from_string", 0);

        if (result) {
            if (bindings_out)
                *bindings_out = single_binding_from_node(result);
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
                bindings = symtab_build_bindings(&syms);
            symtab_free(&syms);
            if (result) {
                if (bindings_out)
                    *bindings_out = bindings;
                return result;
            }
        }

        errmsg[0] = '\0';
        result = parse_pure_const(s, content_end, errmsg, sizeof(errmsg));
        if (!result)
            fprintf(stderr, "dval_from_string: %s\n", errmsg);
        else if (bindings_out)
            *bindings_out = single_binding_from_node(result);
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
        bindings = symtab_build_bindings(&syms);

    symtab_free(&syms);
    if (result && bindings_out)
        *bindings_out = bindings;
    return result;
}

dval_t *dval_from_string(const char *s, dval_bindings_t **bnd_out)
{
    return dval_from_string_impl(s, bnd_out);
}

static dval_binding_entry_t *bnd_find_entry(dval_bindings_t *bnd,
                                            const char *name)
{
    char *norm;
    dval_binding_entry_t *entry = NULL;

    if (!bnd || !bnd->index || !name)
        return NULL;

    norm = dv_normalize_binding_name(name);
    if (!norm)
        return NULL;

    dictionary_get(bnd->index, &norm, &entry);
    free(norm);
    return entry;
}

dval_t *dval_bindings_get(dval_bindings_t *bnd, const char *name)
{
    dval_binding_entry_t *entry = bnd_find_entry(bnd, name);

    return entry ? entry->dval : NULL;
}

void dval_bindings_free(dval_bindings_t *bnd)
{
    if (!bnd)
        return;
    dictionary_destroy(bnd->index);
    free(bnd->storage);
    free(bnd);
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
