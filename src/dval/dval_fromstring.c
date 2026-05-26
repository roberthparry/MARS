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
 *   N or _N     trailing subscript digit N (0–9), normalised to U+2080+N
 *               internally so x0, x_0 and x₀ are interchangeable within the
 *               same string
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
#include "dval_bindings_internal.h"
#include "dval_internal.h"
#include "dval_fromstring_internal.h"
#include "dval.h"

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
    if (c == '[' || c == '(' || c == '@') return 1;
    if (at_middle_dot(p)) return 1;
    if (scan_unicode_fraction_len(p->p, p->end) > 0u) return 1;
    unsigned int uc;
    int len = fs_utf8_decode(p->p, &uc);
    if (len > 0 && (fs_is_letter(uc) || uc == 0x230A || uc == 0x2308))
        return 1;
    if (isdigit(c) || c == '.') return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Forward declarations                                                 */
/* ------------------------------------------------------------------ */

static dval_t *parse_addexpr(parser_t *p);
static dval_t *parse_signed_power(parser_t *p);
static dval_t *parse_expression_region(const char *start,
                                       const char *end,
                                       symtab_t *syms,
                                       const char *context_label,
                                       int report_errors);
static void symtab_discard_storage(symtab_t *t);

/* ------------------------------------------------------------------ */
/* Function dispatch                                                    */
/* ------------------------------------------------------------------ */

typedef dval_t *(*unary_fn)(const dval_t *);
typedef dval_t *(*binary_fn)(const dval_t *, const dval_t *);
typedef dval_t *(*ternary_fn)(const dval_t *, const dval_t *, const dval_t *);

/* ------------------------------------------------------------------ */
/* Function dispatch                                                   */
/* ------------------------------------------------------------------ */

/* Function keyword table.
 *
 * Fixed aliases live here too, so the parser has one source of truth for
 * supported spellings.  The only function-like spelling handled separately is
 * ψ⁽ⁿ⁾(...), whose order is encoded in the token itself. */
#define FUNC_TABLE_SIZE 78

typedef struct {
    const char *kw;
    size_t      klen;
    unsigned    arity;
    const dval_ops_t *ops;
    unary_fn    ufn;
    binary_fn   bfn;
    ternary_fn  tfn;
} func_entry_t;

#define FUNC_ENTRY(name, is_bin, op, unary, binary) \
    { (name), sizeof(name) - 1u, (is_bin) ? 2u : 1u, (op), (unary), (binary), NULL }

#define FUNC_TERNARY_ENTRY(name, ternary) \
    { (name), sizeof(name) - 1u, 3u, NULL, NULL, NULL, (ternary) }

static const unsigned char s_func_displacements[FUNC_TABLE_SIZE] = {
    0, 6, 0, 2, 4, 0, 2, 0, 3, 0,
    0, 0, 0, 0, 0, 1, 1, 0, 0, 0,
    0, 3, 0, 1, 1, 0, 3, 11, 7, 0,
    0, 0, 0, 2, 9, 14, 0, 10, 0, 10,
    14, 0, 0, 0, 9, 6, 5, 0, 38, 31,
    4, 47, 0
};

static const func_entry_t s_funcs[FUNC_TABLE_SIZE] = {
    FUNC_ENTRY("asinh",         false, &ops_asinh,         dv_asinh,         NULL),
    FUNC_ENTRY("sqrt",          false, &ops_sqrt,          dv_sqrt,          NULL),
    FUNC_ENTRY("floor",         false, &ops_floor,         dv_floor,         NULL),
    FUNC_ENTRY("trigamma",      false, &ops_trigamma,      dv_trigamma,      NULL),
    FUNC_ENTRY("abs",           false, &ops_abs,           dv_abs,           NULL),
    FUNC_ENTRY("W₋₁",           false, &ops_lambert_wm1,   dv_lambert_wm1,   NULL),
    FUNC_ENTRY("acosh",         false, &ops_acosh,         dv_acosh,         NULL),
    FUNC_ENTRY("productlog",    false, &ops_lambert_w,     dv_lambert_w,     NULL),
    FUNC_ENTRY("hypot",         true,  &ops_hypot,         NULL,             dv_hypot),
    FUNC_ENTRY("lgamma",        false, &ops_lgamma,        dv_lgamma,        NULL),
    FUNC_ENTRY("polygamma",     true,  &ops_polygamma,     NULL,             dv_polygamma_dv),
    FUNC_ENTRY("gammainv",      false, &ops_gammainv,      dv_gammainv,      NULL),
    FUNC_ENTRY("E1",            false, &ops_e1,            dv_e1,            NULL),
    FUNC_ENTRY("exp",           false, &ops_exp,           dv_exp,           NULL),
    FUNC_ENTRY("tanh",          false, &ops_tanh,          dv_tanh,          NULL),
    FUNC_ENTRY("logbeta",       true,  &ops_logbeta,       NULL,             dv_logbeta),
    FUNC_ENTRY("gammainc_lower", true, &ops_gammainc_lower, NULL,            dv_gammainc_lower),
    FUNC_ENTRY("gammainc_upper", true, &ops_gammainc_upper, NULL,            dv_gammainc_upper),
    FUNC_ENTRY("gammainc_P",    true,  &ops_gammainc_P,    NULL,             dv_gammainc_P),
    FUNC_ENTRY("gammainc_Q",    true,  &ops_gammainc_Q,    NULL,             dv_gammainc_Q),
    FUNC_ENTRY("asin",          false, &ops_asin,          dv_asin,          NULL),
    FUNC_ENTRY("beta",          true,  &ops_beta,          NULL,             dv_beta),
    FUNC_ENTRY("W_0",           false, &ops_lambert_w0,    dv_lambert_w0,    NULL),
    FUNC_ENTRY("sinh",          false, &ops_sinh,          dv_sinh,          NULL),
    FUNC_ENTRY("Ei",            false, &ops_ei,            dv_ei,            NULL),
    FUNC_ENTRY("erfinv",        false, &ops_erfinv,        dv_erfinv,        NULL),
    FUNC_ENTRY("Γ",             false, &ops_gamma,         dv_gamma,         NULL),
    FUNC_ENTRY("erf",           false, &ops_erf,           dv_erf,           NULL),
    FUNC_ENTRY("normal_pdf",    false, &ops_normal_pdf,    dv_normal_pdf,    NULL),
    FUNC_ENTRY("tan",           false, &ops_tan,           dv_tan,           NULL),
    FUNC_ENTRY("normal_cdf",    false, &ops_normal_cdf,    dv_normal_cdf,    NULL),
    FUNC_ENTRY("acos",          false, &ops_acos,          dv_acos,          NULL),
    FUNC_ENTRY("W-1",           false, &ops_lambert_wm1,   dv_lambert_wm1,   NULL),
    FUNC_ENTRY("ln",            false, &ops_log,           dv_log,           NULL),
    FUNC_ENTRY("log10",         false, &ops_log10,         dv_log10,         NULL),
    FUNC_ENTRY("atanh",         false, &ops_atanh,         dv_atanh,         NULL),
    FUNC_ENTRY("lambert_wm1",   false, &ops_lambert_wm1,   dv_lambert_wm1,   NULL),
    FUNC_ENTRY("cosh",          false, &ops_cosh,          dv_cosh,          NULL),
    FUNC_ENTRY("sin",           false, &ops_sin,           dv_sin,           NULL),
    FUNC_ENTRY("erfc",          false, &ops_erfc,          dv_erfc,          NULL),
    FUNC_ENTRY("digamma",       false, &ops_digamma,       dv_digamma,       NULL),
    FUNC_ENTRY("ψ⁽¹⁾",          false, &ops_trigamma,      dv_trigamma,      NULL),
    FUNC_ENTRY("gamma",         false, &ops_gamma,         dv_gamma,         NULL),
    FUNC_ENTRY("atan2",         true,  &ops_atan2,         NULL,             dv_atan2),
    FUNC_ENTRY("ψ⁽⁰⁾",          false, &ops_digamma,       dv_digamma,       NULL),
    FUNC_ENTRY("W",             false, &ops_lambert_w,     dv_lambert_w,     NULL),
    FUNC_ENTRY("pow",           true,  &ops_pow,           NULL,             dv_pow_dv),
    FUNC_ENTRY("normal_logpdf", false, &ops_normal_logpdf, dv_normal_logpdf, NULL),
    FUNC_ENTRY("cos",           false, &ops_cos,           dv_cos,           NULL),
    FUNC_ENTRY("log",           false, &ops_log10,         dv_log10,         NULL),
    FUNC_ENTRY("lambert_w0",    false, &ops_lambert_w0,    dv_lambert_w0,    NULL),
    FUNC_ENTRY("atan",          false, &ops_atan,          dv_atan,          NULL),
    FUNC_ENTRY("W0",            false, &ops_lambert_w0,    dv_lambert_w0,    NULL),
    FUNC_ENTRY("ceil",          false, &ops_ceil,          dv_ceil,          NULL),
    FUNC_ENTRY("W_-1",          false, &ops_lambert_wm1,   dv_lambert_wm1,   NULL),
    FUNC_ENTRY("erfcinv",       false, &ops_erfcinv,       dv_erfcinv,       NULL),
    FUNC_ENTRY("W₀",            false, &ops_lambert_w0,    dv_lambert_w0,    NULL),
    FUNC_ENTRY("factorial",     false, &ops_factorial,     dv_factorial,     NULL),
    FUNC_ENTRY("fibonacci",     false, &ops_fibonacci,     dv_fibonacci,     NULL),
    FUNC_ENTRY("partition",     false, &ops_partition,     dv_partition,     NULL),
    FUNC_ENTRY("isqrt",         false, &ops_isqrt,         dv_isqrt,         NULL),
    FUNC_ENTRY("gcd",           true,  &ops_gcd,           NULL,             dv_gcd),
    FUNC_ENTRY("lcm",           true,  &ops_lcm,           NULL,             dv_lcm),
    FUNC_ENTRY("mod",           true,  &ops_mod,           NULL,             dv_mod),
    FUNC_ENTRY("modinv",        true,  &ops_modinv,        NULL,             dv_modinv),
    FUNC_ENTRY("is_prime",      false, &ops_is_prime,      dv_is_prime,      NULL),
    FUNC_ENTRY("next_prime",    false, &ops_next_prime,    dv_next_prime,    NULL),
    FUNC_ENTRY("prev_prime",    false, &ops_prev_prime,    dv_prev_prime,    NULL),
    FUNC_ENTRY("AND",           true,  &ops_bit_and,       NULL,             dv_bit_and),
    FUNC_ENTRY("OR",            true,  &ops_bit_or,        NULL,             dv_bit_or),
    FUNC_ENTRY("XOR",           true,  &ops_bit_xor,       NULL,             dv_bit_xor),
    FUNC_ENTRY("NOT",           false, &ops_bit_not,       dv_bit_not,       NULL),
    FUNC_ENTRY("SHL",           true,  &ops_shl,           NULL,             dv_shl),
    FUNC_ENTRY("SHR",           true,  &ops_shr,           NULL,             dv_shr),
    FUNC_ENTRY("factors",       false, &ops_factors,       dv_factors,       NULL),
    FUNC_ENTRY("binomial",      true,  NULL,                NULL,             dv_binomial),
    FUNC_TERNARY_ENTRY("beta_pdf",    dv_beta_pdf),
    FUNC_TERNARY_ENTRY("logbeta_pdf", dv_logbeta_pdf),
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

static int superscript_digit_value(unsigned int c)
{
    if (c == 0x00B9)
        return 1;
    if (c == 0x00B2)
        return 2;
    if (c == 0x00B3)
        return 3;
    if (c == 0x2070)
        return 0;
    if (c >= 0x2074 && c <= 0x2079)
        return (int)(c - 0x2070);
    return -1;
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
static const char *func_call_start(const char *pos, const char *end,
                                   const char *kw, size_t klen)
{
    if (pos + klen > end)
        return NULL;
    if (strncmp(pos, kw, klen) != 0) return NULL;
    const char *after = pos + klen;
    /* Skip Unicode superscript digits (sin²(x) form) */
    while (after < end && is_superscript_byte(after)) {
        unsigned int c;
        int len = fs_utf8_decode(after, &c);
        after += len;
    }
    /* Skip ASCII ^N (sin^2(x) form) */
    if (after + 1 < end && *after == '^' &&
        isdigit((unsigned char)after[1])) {
        after++; /* skip '^' */
        while (after < end && isdigit((unsigned char)*after)) after++;
    }
    return (after < end && *after == '(') ? after : NULL;
}

static const func_entry_t *lookup_fixed_func_call(const char *pos,
                                                  const char *end,
                                                  const char **paren_out)
{
    const char *id = pos;
    const char *id_end = id;
    size_t id_len;

    if (paren_out)
        *paren_out = NULL;

    while (id_end < end &&
           (isalpha((unsigned char)*id_end) ||
            isdigit((unsigned char)*id_end) ||
            *id_end == '_'))
        id_end++;
    id_len = (size_t)(id_end - id);

    if (id_len > 0u) {
        const func_entry_t *entry = lookup_func(id, id_len);

        if (entry) {
            const char *paren = func_call_start(pos, end, entry->kw, entry->klen);

            if (paren) {
                if (paren_out)
                    *paren_out = paren;
                return entry;
            }
        }
    }

    for (size_t i = 0u; i < FUNC_TABLE_SIZE; ++i) {
        const func_entry_t *entry = &s_funcs[i];
        const char *paren;

        if (!entry->kw)
            continue;
        paren = func_call_start(pos, end, entry->kw, entry->klen);
        if (!paren)
            continue;

        if (paren_out)
            *paren_out = paren;
        return entry;
    }

    return NULL;
}

static int scan_polygamma_symbol_call(const char *pos, const char *end,
                                      unsigned int *order_out,
                                      const char **paren_out)
{
    const char *p = pos;
    unsigned int c;
    unsigned long order = 0ul;
    int len;
    int digits = 0;

    if (order_out)
        *order_out = 0u;
    if (paren_out)
        *paren_out = NULL;

    len = scan_utf8_codepoint(p, end, &c);
    if (len <= 0 || c != 0x03C8)
        return 0;
    p += len;

    len = scan_utf8_codepoint(p, end, &c);
    if (len <= 0 || c != 0x207D)
        return 0;
    p += len;

    while ((len = scan_utf8_codepoint(p, end, &c)) > 0 &&
           is_superscript_digit_codepoint(c)) {
        int digit = superscript_digit_value(c);

        if (digit < 0)
            return 0;
        if (order > (ULONG_MAX - (unsigned long)digit) / 10ul)
            return 0;
        order = order * 10ul + (unsigned long)digit;
        p += len;
        ++digits;
    }
    if (digits == 0 || order > UINT_MAX)
        return 0;

    len = scan_utf8_codepoint(p, end, &c);
    if (len <= 0 || c != 0x207E)
        return 0;
    p += len;

    if (p >= end || *p != '(')
        return 0;

    if (order_out)
        *order_out = (unsigned int)order;
    if (paren_out)
        *paren_out = p;
    return 1;
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

static int parse_three_args(parser_t *p,
                            dval_t **a_out,
                            dval_t **b_out,
                            dval_t **c_out)
{
    dval_t *a = parse_addexpr(p);
    dval_t *b;
    dval_t *c;

    if (!a)
        return 0;
    if (p->p >= p->end || *p->p != ',') {
        dv_free(a);
        set_error(p, "expected ',' in ternary function");
        return 0;
    }
    p->p++;
    skip_spaces(&p->p, p->end);

    b = parse_addexpr(p);
    if (!b) {
        dv_free(a);
        return 0;
    }
    if (p->p >= p->end || *p->p != ',') {
        dv_free(b);
        dv_free(a);
        set_error(p, "expected ',' in ternary function");
        return 0;
    }
    p->p++;
    skip_spaces(&p->p, p->end);

    c = parse_addexpr(p);
    if (!c) {
        dv_free(b);
        dv_free(a);
        return 0;
    }

    *a_out = a;
    *b_out = b;
    *c_out = c;
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
    size_t atom_len;
    char *roundtrip;

    while (start < end && isspace((unsigned char)*start))
        start++;
    while (end > start && isspace((unsigned char)end[-1]))
        end--;

    if (start >= end)
        return 0;

    len = (size_t)(end - start);
    atom_len = scan_number_atom_len(start, end);
    if (atom_len != len)
        return 0;

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

static int node_has_preserved_constexpr(const dval_t *node)
{
    number_t value;
    int is_builtin_const;
    int value_matches_builtin = 0;

    if (!dv_is_const(node) || !node->binding_expr)
        return 0;
    if (!node->name || !*node->name)
        return 1;

    is_builtin_const = dv_get_default_constant_num(node->name, &value);
    if (is_builtin_const) {
        value_matches_builtin = num_eq(node->c, value);
        num_destroy(&value);
    }

    return dv_is_const(node) &&
           node->binding_expr &&
           is_builtin_const &&
           value_matches_builtin;
}

static int binding_const_is_numeric_literal(dv_binding_const_id_t const_id)
{
    /*
     * These constants are lexical numeric atoms rather than symbolic display
     * constants.  Symbolic constants such as pi/gamma stay in the preserved
     * expression tree even though they also have numeric values.
     */
    return const_id == DV_BINDING_CONST_I;
}

static int binding_expr_is_numeric_literal(const dv_binding_expr_t *expr)
{
    if (!expr)
        return 0;

    switch (expr->kind) {
    case DV_BINDING_EXPR_NUMBER:
        return 1;
    case DV_BINDING_EXPR_CONST:
        return binding_const_is_numeric_literal(expr->u.const_id);
    case DV_BINDING_EXPR_NEG:
        return binding_expr_is_numeric_literal(expr->u.unary.child);
    case DV_BINDING_EXPR_ADD:
    case DV_BINDING_EXPR_SUB:
    case DV_BINDING_EXPR_MUL:
    case DV_BINDING_EXPR_DIV:
        return binding_expr_is_numeric_literal(expr->u.binary.left) &&
               binding_expr_is_numeric_literal(expr->u.binary.right);
    case DV_BINDING_EXPR_POWI:
        return binding_expr_is_numeric_literal(expr->u.powi.base);
    case DV_BINDING_EXPR_UNARY_OP:
    case DV_BINDING_EXPR_BINARY_OP:
        return 0;
    }

    return 0;
}

static dv_binding_expr_t *binding_expr_number_from_value_local(number_t value)
{
    char *text = num_to_string(value);
    dv_binding_expr_t *expr =
        dv_binding_expr_new_number_text(text ? text : "NAN");

    free(text);
    return expr;
}

static dval_t *const_node_from_binding_expr(dv_binding_expr_t *expr)
{
    dv_binding_expr_t *original_expr;
    number_t value;
    dval_t *node;

    if (!expr)
        return dv_new_const(NUM_NAN);

    original_expr = dv_binding_expr_clone(expr);
    expr = dv_binding_expr_simplify(expr);
    value = dv_binding_expr_eval(expr);
    if (!num_is_finite(value)) {
        dv_binding_expr_free(expr);
        node = dv_binding_expr_eval_dval(original_expr);
        if (node) {
            dv_binding_expr_free(node->binding_expr);
            node->binding_expr = original_expr;
        } else {
            dv_binding_expr_free(original_expr);
        }
        num_destroy(&value);
        return node ? node : dv_new_const(NUM_NAN);
    }
    dv_binding_expr_free(original_expr);
    node = dv_new_const(value);
    if (binding_expr_is_numeric_literal(expr) &&
        (num_is_exact(value) || num_is_real(value))) {
        dv_binding_expr_free(expr);
        expr = binding_expr_number_from_value_local(value);
    }
    num_destroy(&value);
    node->binding_expr = expr;
    return node;
}

static dval_t *apply_unary_preserving_constexpr(const dval_ops_t *ops,
                                                dval_t *arg,
                                                dval_t *(*fallback)(const dval_t *))
{
    if (node_has_preserved_constexpr(arg)) {
        dv_binding_expr_t *child = dv_binding_expr_clone(arg->binding_expr);
        dv_binding_expr_t *expr = (ops == &ops_neg)
            ? dv_binding_expr_new_neg(child)
            : dv_binding_expr_new_unary_op(ops, child);
        dval_t *node = const_node_from_binding_expr(expr);

        dv_free(arg);
        return node;
    }

    {
        dval_t *node = fallback(arg);
        dv_free(arg);
        return node;
    }
}

static dv_binding_expr_t *binding_expr_for_binary_constexpr(const dval_ops_t *ops,
                                                            const dval_t *left,
                                                            const dval_t *right)
{
    dv_binding_expr_t *l = dv_binding_expr_clone(left->binding_expr);
    dv_binding_expr_t *r = dv_binding_expr_clone(right->binding_expr);

    if (ops == &ops_add)
        return dv_binding_expr_new_add(l, r);
    if (ops == &ops_sub)
        return dv_binding_expr_new_sub(l, r);
    if (ops == &ops_mul)
        return dv_binding_expr_new_mul(l, r);
    if (ops == &ops_div)
        return dv_binding_expr_new_div(l, r);
    if (ops == &ops_pow)
        return dv_binding_expr_new_binary_op(&ops_pow, l, r);

    return dv_binding_expr_new_binary_op(ops, l, r);
}

static dval_t *apply_binary_preserving_constexpr(const dval_ops_t *ops,
                                                 dval_t *left,
                                                 dval_t *right,
                                                 dval_t *(*fallback)(const dval_t *,
                                                                     const dval_t *))
{
    if (node_has_preserved_constexpr(left) &&
        node_has_preserved_constexpr(right)) {
        dv_binding_expr_t *expr = binding_expr_for_binary_constexpr(ops, left, right);
        dval_t *node = const_node_from_binding_expr(expr);

        dv_free(left);
        dv_free(right);
        return node;
    }

    {
        dval_t *node = fallback(left, right);
        dv_free(left);
        dv_free(right);
        return node;
    }
}

static dval_t *apply_pow_const_preserving_constexpr(dval_t *base, const number_t *exponent)
{
    if (node_has_preserved_constexpr(base)) {
        char *text = num_to_string(*exponent);
        dv_binding_expr_t *rhs = dv_binding_expr_new_number_text(text ? text : "NAN");
        dv_binding_expr_t *expr =
            dv_binding_expr_new_binary_op(&ops_pow,
                                          dv_binding_expr_clone(base->binding_expr),
                                          rhs);
        dval_t *node;

        free(text);
        node = const_node_from_binding_expr(expr);
        dv_free(base);
        return node;
    }

    {
        dval_t *node = dv_pow(base, exponent);
        dv_free(base);
        return node;
    }
}

static dval_t *apply_integer_power_if_present(dval_t *value, int exponent)
{
    NUM_SCOPE(scope);
    if (exponent < 0)
        return value;

    number_t exponent_num = num_create_from_long(exponent);
    dval_t *powered = apply_pow_const_preserving_constexpr(value, &exponent_num);
    return powered;
}

static dval_t *apply_factorial_postfix(dval_t *value)
{
    dval_t *one = dv_new_const(NUM_ONE);
    dval_t *incremented;

    if (!one) {
        dv_free(value);
        return NULL;
    }

    one->binding_expr = dv_binding_expr_new_number_text("1");
    incremented = apply_binary_preserving_constexpr(&ops_add, value, one, dv_add);
    if (!incremented)
        return NULL;
    return apply_unary_preserving_constexpr(&ops_gamma, incremented, dv_gamma);
}

static dval_t *parse_enclosed_addexpr(parser_t *p, char closing, const char *errmsg)
{
    dval_t *inner;

    skip_spaces(&p->p, p->end);
    inner = parse_addexpr(p);

    if (!inner)
        return NULL;
    skip_spaces(&p->p, p->end);
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
        return apply_unary_preserving_constexpr(&ops_abs, inner, dv_abs);
    }

    /* Mathematical floor/ceiling brackets: ⌊expr⌋ and ⌈expr⌉ */
    if (cp_len > 0 && (cp == 0x230A || cp == 0x2308)) {
        const unsigned int closing = (cp == 0x230A) ? 0x230B : 0x2309;
        const char *errmsg = (cp == 0x230A) ? "expected '⌋'" : "expected '⌉'";
        dval_t *inner;
        dval_t *result;
        unsigned int close_cp = 0;
        int close_len;

        p->p += cp_len;
        inner = parse_addexpr(p);
        if (!inner)
            return NULL;
        skip_spaces(&p->p, p->end);

        close_len = fs_utf8_decode(p->p, &close_cp);
        if (close_len <= 0 || close_cp != closing) {
            dv_free(inner);
            set_error(p, errmsg);
            return NULL;
        }
        p->p += close_len;

        result = (cp == 0x230A)
            ? apply_unary_preserving_constexpr(&ops_floor, inner, dv_floor)
            : apply_unary_preserving_constexpr(&ops_ceil, inner, dv_ceil);
        return result;
    }

    /* Numeric atom (integer/decimal/rational, optionally with trailing i) */
    if (isdigit((unsigned char)*p->p) || *p->p == '.' ||
        scan_unicode_fraction_len(p->p, p->end) > 0u) {
        size_t len = scan_number_atom_len(p->p, p->end);
        const char *start = p->p;
        number_t value;
        dval_t *node;
        char *text;

        if (len == 0 || !parse_number_region(p->p, p->p + len, &value)) {
            set_error(p, "expected numeric literal");
            return NULL;
        }
        p->p += len;
        node = dv_new_const(value);
        if (num_is_exact(value)) {
            text = num_to_string(value);
            if (!text) {
                text = (char *)fs_xmalloc(4u);
                memcpy(text, "NAN", 4u);
            }
        } else {
            text = (char *)fs_xmalloc(len + 1u);
            memcpy(text, start, len);
            text[len] = '\0';
        }
        node->binding_expr = dv_binding_expr_new_number_text(text);
        free(text);
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
        result = apply_unary_preserving_constexpr(&ops_sqrt, arg, dv_sqrt);
        return apply_integer_power_if_present(result, sup);
    }

    {
        const char *paren = NULL;
        const func_entry_t *fe = lookup_fixed_func_call(p->p, p->end, &paren);

        if (fe) {
            const char *after_kw = p->p + fe->klen;
            int sup = read_optional_display_exponent(&after_kw);
            (void)after_kw;

            p->p = paren + 1; /* skip past '(' */
            if (fe->arity == 2u) {
                dval_t *a = NULL;
                dval_t *b = NULL;
                dval_t *result;

                if (!parse_two_args(p, &a, &b))
                    return NULL;
                if (!parse_required_char(p, ')', "expected ')' after binary function")) {
                    dv_free(a);
                    dv_free(b);
                    return NULL;
                }
                result = fe->ops
                    ? apply_binary_preserving_constexpr(fe->ops, a, b, fe->bfn)
                    : fe->bfn(a, b);
                if (!fe->ops) {
                    dv_free(a);
                    dv_free(b);
                }
                return apply_integer_power_if_present(result, sup);
            } else if (fe->arity == 3u) {
                dval_t *a = NULL;
                dval_t *b = NULL;
                dval_t *c = NULL;
                dval_t *result;

                if (!parse_three_args(p, &a, &b, &c))
                    return NULL;
                if (!parse_required_char(p, ')', "expected ')' after ternary function")) {
                    dv_free(a);
                    dv_free(b);
                    dv_free(c);
                    return NULL;
                }
                result = fe->tfn(a, b, c);
                dv_free(a);
                dv_free(b);
                dv_free(c);
                return apply_integer_power_if_present(result, sup);
            } else {
                dval_t *arg = parse_enclosed_addexpr(
                    p, ')', "expected ')' after function argument");
                dval_t *result;

                if (!arg)
                    return NULL;
                if (fe->ops == &ops_factors) {
                    result = fe->ufn(arg);
                    dv_free(arg);
                } else {
                    result = apply_unary_preserving_constexpr(fe->ops, arg, fe->ufn);
                }
                return apply_integer_power_if_present(result, sup);
            }
        }
    }

    {
        unsigned int order = 0u;
        const char *paren = NULL;

        if (scan_polygamma_symbol_call(p->p, p->end, &order, &paren)) {
            dval_t *arg;
            dval_t *result;

            p->p = paren + 1;
            arg = parse_enclosed_addexpr(
                p, ')', "expected ')' after polygamma argument");
            if (!arg)
                return NULL;
            result = dv_polygamma(order, arg);
            dv_free(arg);
            return result;
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
        char *normalised = dv_normalise_name(dv_default_constant_canonical_name(name));

        if (normalised) {
            sym = symtab_lookup(p->syms, normalised);
            free(normalised);
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

    while (p->p < p->end && *p->p == '!') {
        p->p++;
        base = apply_factorial_postfix(base);
        if (!base)
            return NULL;
    }

    /* Unicode superscript exponent: x² */
    int sup = read_superscript(&p->p);
    if (sup >= 0)
        return apply_integer_power_if_present(base, sup);

    /* Caret exponent: x^n, x^y, x^(a+b), right-associative */
    if (p->p < p->end && *p->p == '^') {
        dval_t *exponent = NULL;
        dval_t *result = NULL;

        p->p++;

        if (p->p < p->end && *p->p == '(') {
            p->p++;
            exponent = parse_enclosed_addexpr(p, ')', "expected ')' after exponent");
            if (!exponent) {
                dv_free(base);
                return NULL;
            }
        } else {
            exponent = parse_signed_power(p);
            if (!exponent) {
                dv_free(base);
                set_error(p, "expected exponent after '^'");
                return NULL;
            }
        }

        if (dv_is_unnamed_const(exponent) && num_is_real(exponent->c) &&
            (!exponent->binding_expr || !node_has_preserved_constexpr(base))) {
            result = apply_pow_const_preserving_constexpr(base, &exponent->c);
            dv_free(exponent);
        } else {
            result = apply_binary_preserving_constexpr(&ops_pow, base, exponent, dv_pow_dv);
        }
        if (!result)
            return result;
        return result;
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
        return apply_unary_preserving_constexpr(&ops_neg, inner, dv_neg);
    }
    return parse_power(p);
}

/* ------------------------------------------------------------------ */
/* Multiplication / division (implicit, '*', '·', '/')                 */
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
            lhs = apply_binary_preserving_constexpr(&ops_mul, lhs, rhs, dv_mul);
            continue;
        }

        /* Explicit '*' or '/' accepted with or without surrounding spaces,
         * e.g. "x*y", "x * y", "x/y", and "x / y". Peek past spaces before
         * committing — if neither operator is present we fall through without
         * advancing p->p. */
        {
            const char *peek = p->p;
            while (peek < p->end && *peek == ' ') peek++;
            if (peek < p->end && (*peek == '*' || *peek == '/')) {
                char op = *peek;

                p->p = peek + 1; /* consume optional leading spaces and operator */
                skip_spaces(&p->p, p->end); /* trailing spaces */
                dval_t *rhs = parse_signed_power(p);
                if (!rhs) { dv_free(lhs); return NULL; }
                lhs = (op == '*')
                    ? apply_binary_preserving_constexpr(&ops_mul, lhs, rhs, dv_mul)
                    : apply_binary_preserving_constexpr(&ops_div, lhs, rhs, dv_div);
                continue;
            }
        }

        /* Implicit multiplication: next position can start a factor */
        if (can_start_factor(p)) {
            dval_t *rhs = parse_signed_power(p);
            if (!rhs) { dv_free(lhs); return NULL; }
            lhs = apply_binary_preserving_constexpr(&ops_mul, lhs, rhs, dv_mul);
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
        const char *peek = p->p;

        while (peek < p->end && *peek == ' ')
            peek++;

        if (peek < p->end && (*peek == '+' || *peek == '-')) {
            char op = *peek;

            p->p = peek + 1;
            skip_spaces(&p->p, p->end);
            dval_t *rhs = parse_mulexpr(p);
            if (!rhs) { dv_free(lhs); return NULL; }
            lhs = (op == '+')
                ? apply_binary_preserving_constexpr(&ops_add, lhs, rhs, dv_add)
                : apply_binary_preserving_constexpr(&ops_sub, lhs, rhs, dv_sub);
            continue;
        }

        break;
    }
    return lhs;
}

/* ------------------------------------------------------------------ */
/* Binding section parser                                               */
/* ------------------------------------------------------------------ */

/* Parse comma/semicolon-separated "name = value" pairs from [s, end).
 * is_var: 1 → create dv_new_named_var(); 0 → create dv_new_named_const().
 * On success returns 0; on failure writes to errmsg and returns -1. */
static int parse_bindings(const char *s, const char *end,
                           int is_var, symtab_t *syms,
                           char *errmsg, size_t errmsg_n)
{
    NUM_SCOPE(scope);
    const char *p = s;
    while (p < end) {
        /* Skip whitespace and separators between entries. */
        while (p < end && (isspace((unsigned char)*p) || *p == ',' || *p == ';')) p++;
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
        dv_binding_expr_t *binding_expr;
        number_t val;
        dval_t *node;

        binding_expr = dv_binding_expr_parse_region(p, value_end, errmsg, errmsg_n);
        if (!binding_expr) {
            free(name);
            return -1;
        }
        binding_expr = dv_binding_expr_simplify(binding_expr);
        val = dv_binding_expr_eval(binding_expr);
        p = value_end;

        node = is_var
            ? dv_new_named_var(val, name)
            : dv_new_named_const(val, name);
        num_destroy(&val);
        node->binding_expr = binding_expr;

        /* dv_new_named_* calls dv_normalise_name, which may transform the name
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

    dv_binding_expr_t *binding_expr = dv_binding_expr_parse_region(p, end, errmsg, errmsg_n);
    if (!binding_expr) {
        free(name);
        return NULL;
    }
    binding_expr = dv_binding_expr_simplify(binding_expr);
    number_t val = dv_binding_expr_eval(binding_expr);

    if (!name) {
        dv_binding_expr_free(binding_expr);
        snprintf(errmsg, errmsg_n, "constant name is required in pure-constant format");
        return NULL;
    }
    dval_t *result = dv_new_named_const(val, name);
    num_destroy(&val);
    result->binding_expr = binding_expr;
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
        const char *id = p;
        const char *id_end = id;

        while (id_end < end &&
               (isalpha((unsigned char)*id_end) ||
                isdigit((unsigned char)*id_end) ||
                *id_end == '_'))
            id_end++;

        if (id_end > id || (unsigned char)*p >= 0x80) {
            const char *paren = NULL;
            const func_entry_t *fe = lookup_fixed_func_call(p, end, &paren);

            if (fe && paren) {
                p += fe->klen;
                continue;
            }
        }

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
            char bind_err[128];

            canonical_name = dv_default_constant_canonical_name(name);
            node = dv_new_named_const(value, canonical_name);
            node->binding_expr =
                dv_binding_expr_parse_region(name, name + strlen(name),
                                             bind_err, sizeof(bind_err));
        } else {
            node = is_const
                ? dv_new_named_const(NUM_NAN, name)
                : dv_new_named_var(NUM_NAN, name);
        }

        key = dv_normalise_name(canonical_name);
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

static void symtab_discard_storage(symtab_t *t)
{
    symtab_free(t);
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

static dval_t *simplify_parsed_result(dval_t *result)
{
    dval_t *simplified;

    if (!result)
        return NULL;

    simplified = dv_simplify(result);
    if (!simplified)
        return result;

    dv_free(result);
    return simplified;
}

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
            result = simplify_parsed_result(result);
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
            if (result && bindings_out)
                symtab_discard_storage(&syms);
            else
                symtab_free(&syms);
            if (result) {
                result = simplify_parsed_result(result);
                if (bindings_out)
                    *bindings_out = bindings;
                return result;
            }
        }

        errmsg[0] = '\0';
        result = parse_pure_const(s, content_end, errmsg, sizeof(errmsg));
        if (!result)
            fprintf(stderr, "dval_from_string: %s\n", errmsg);
        else {
            result = simplify_parsed_result(result);
            if (bindings_out)
                *bindings_out = single_binding_from_node(result);
        }
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

    if (collect_implicit_symbols(s, expr_end, &syms) < 0) {
        symtab_free(&syms);
        fprintf(stderr, "dval_from_string: out of memory\n");
        return NULL;
    }

    dval_t *result = parse_expression_region(s, expr_end, &syms,
                                             "dval_from_string", 1);
    if (result && bindings_out) {
        bindings = symtab_build_bindings(&syms);
    }

    if (result && bindings_out) {
        symtab_discard_storage(&syms);
    } else {
        symtab_free(&syms);
    }
    if (result)
        result = simplify_parsed_result(result);
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

    norm = dv_normalise_binding_name(name);
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
    for (size_t i = 0; i < bnd->count; ++i)
        dv_free(bnd->entries[i].dval);
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
    result = simplify_parsed_result(result);
    return result;
}
