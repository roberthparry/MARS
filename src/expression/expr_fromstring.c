/* expr_fromstring.c - construct a expr_t from an expression-style string
 *
 * Accepts strings in the format produced by expr_to_string(f, style_EXPRESSION):
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
 * Returns an owning expr_t* on success, NULL on parse error (details written
 * to stderr).  The caller must call expr_free() on the returned pointer exactly
 * once.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "qfloat.h"
#include "expr_bindings.h"
#include "expr_internal.h"
#include "expr_fromstring.h"
#include "expression.h"

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
static size_t scan_special_number_literal_len(const char *s, const char *end);

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
    if (scan_special_number_literal_len(p->p, p->end) > 0u) return 1;
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

static expr_t *parse_addexpr(parser_t *p);
static expr_t *parse_signed_power(parser_t *p);
static expr_t *parse_signed_power_operand(parser_t *p);
static expr_t *parse_expression_region(const char *start,
                                       const char *end,
                                       symtab_t *syms,
                                       const char *context_label,
                                       int report_errors);
static void symtab_discard_storage(symtab_t *t);

/* ------------------------------------------------------------------ */
/* Function dispatch                                                    */
/* ------------------------------------------------------------------ */

typedef expr_t *(*unary_fn)(const expr_t *);
typedef expr_t *(*binary_fn)(const expr_t *, const expr_t *);
typedef expr_t *(*ternary_fn)(const expr_t *, const expr_t *, const expr_t *);

/* ------------------------------------------------------------------ */
/* Function dispatch                                                   */
/* ------------------------------------------------------------------ */

/* Function keyword table.
 *
 * Fixed aliases live here too, so the parser has one source of truth for
 * supported spellings.  The only function-like spelling handled separately is
 * ψ⁽ⁿ⁾(...), whose order is encoded in the token itself. */
#define FUNC_TABLE_SIZE 107

typedef struct {
    const char *kw;
    size_t      klen;
    unsigned    arity;
    const expr_ops_t *ops;
    unary_fn    ufn;
    binary_fn   bfn;
    ternary_fn  tfn;
} func_entry_t;

#define FUNC_ENTRY(name, is_bin, op, unary, binary) \
    { (name), sizeof(name) - 1u, (is_bin) ? 2u : 1u, (op), (unary), (binary), NULL }

#define FUNC_TERNARY_ENTRY(name, ternary) \
    { (name), sizeof(name) - 1u, 3u, NULL, NULL, NULL, (ternary) }

static const unsigned char s_func_displacements[FUNC_TABLE_SIZE] = {
      0,   0,   0,   3,   1,   0,   1,   0,   0,   1,
      0,   5,   0,   2,   0,   0,   1,   0,   0,   3,
      0,   0,   0,   2,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   1,   2,   0,   0,   1,
      3,  21,   0,   0,   0,   0,   0,   2,   1,   1,
      1,  16,   0,   0,   0,   4,   3,   0,   0,   0,
      0,   0,   3,   0,   5,  22,   0,   0,   0,   4,
      2,   3,   0,   3,   1,   0,   0,   0,   0,   0,
      6,   0,   1,   0,  24,  13,   0,   4,  16,   0,
      5,   0,   0,   0,   5,   5,   0,  13,   0,  43,
      0,   0,  53,  25,  33,  74,   0
};

static const func_entry_t s_funcs[FUNC_TABLE_SIZE] = {
    [  0] = FUNC_ENTRY("SHR",           true,  &ops_shr,           NULL,             expr_shr),
    [  1] = FUNC_ENTRY("hypot",         true,  &ops_hypot,         NULL,             expr_hypot),
    [  2] = FUNC_ENTRY("tan",           false, &ops_tan,           expr_tan,           NULL),
    [  3] = FUNC_ENTRY("beta",          true,  &ops_beta,          NULL,             expr_beta),
    [  4] = FUNC_ENTRY("asin",          false, &ops_asin,          expr_asin,          NULL),
    [  5] = FUNC_ENTRY("arccot",        false, &ops_acot,          expr_acot,          NULL),
    [  6] = FUNC_ENTRY("log",           false, &ops_log10,         expr_log10,         NULL),
    [  7] = FUNC_ENTRY("digamma",       false, &ops_digamma,       expr_digamma,       NULL),
    [  8] = FUNC_ENTRY("SHL",           true,  &ops_shl,           NULL,             expr_shl),
    [  9] = FUNC_ENTRY("lambert_w0",    false, &ops_lambert_w0,    expr_lambert_w0,    NULL),
    [ 10] = FUNC_ENTRY("binomial",      true,  NULL,                NULL,             expr_binomial),
    [ 11] = FUNC_ENTRY("acos",          false, &ops_acos,          expr_acos,          NULL),
    [ 12] = FUNC_ENTRY("gammainv",      false, &ops_gammainv,      expr_gammainv,      NULL),
    [ 13] = FUNC_ENTRY("arsech",        false, &ops_asech,         expr_asech,         NULL),
    [ 14] = FUNC_ENTRY("trigamma",      false, &ops_trigamma,      expr_trigamma,      NULL),
    [ 15] = FUNC_ENTRY("AND",           true,  &ops_bit_and,       NULL,             expr_bit_and),
    [ 16] = FUNC_ENTRY("atan2",         true,  &ops_atan2,         NULL,             expr_atan2),
    [ 17] = FUNC_ENTRY("arcoth",        false, &ops_acoth,         expr_acoth,         NULL),
    [ 18] = FUNC_ENTRY("normal_pdf",    false, &ops_normal_pdf,    expr_normal_pdf,    NULL),
    [ 19] = FUNC_ENTRY("arcosech",      false, &ops_acosech,       expr_acosech,       NULL),
    [ 20] = FUNC_ENTRY("sqrt",          false, &ops_sqrt,          expr_sqrt,          NULL),
    [ 21] = FUNC_ENTRY("prev_prime",    false, &ops_prev_prime,    expr_prev_prime,    NULL),
    [ 22] = FUNC_ENTRY("log10",         false, &ops_log10,         expr_log10,         NULL),
    [ 23] = FUNC_ENTRY("asinh",         false, &ops_asinh,         expr_asinh,         NULL),
    [ 24] = FUNC_ENTRY("sec",           false, &ops_sec,           expr_sec,           NULL),
    [ 25] = FUNC_ENTRY("next_prime",    false, &ops_next_prime,    expr_next_prime,    NULL),
    [ 26] = FUNC_ENTRY("lg",            false, &ops_log10,         expr_log10,         NULL),
    [ 28] = FUNC_ENTRY("isqrt",         false, &ops_isqrt,         expr_isqrt,         NULL),
    [ 29] = FUNC_ENTRY("Γ",             false, &ops_gamma,         expr_gamma,         NULL),
    [ 30] = FUNC_ENTRY("gammainc_Q",    true,  &ops_gammainc_Q,    NULL,             expr_gammainc_Q),
    [ 31] = FUNC_ENTRY("erfinv",        false, &ops_erfinv,        expr_erfinv,        NULL),
    [ 32] = FUNC_ENTRY("arcsch",        false, &ops_acosech,       expr_acosech,       NULL),
    [ 33] = FUNC_ENTRY("cosec",         false, &ops_cosec,         expr_cosec,         NULL),
    [ 34] = FUNC_ENTRY("is_prime",      false, &ops_is_prime,      expr_is_prime,      NULL),
    [ 35] = FUNC_ENTRY("atanh",         false, &ops_atanh,         expr_atanh,         NULL),
    [ 36] = FUNC_ENTRY("acsch",         false, &ops_acosech,       expr_acosech,       NULL),
    [ 37] = FUNC_ENTRY("coth",          false, &ops_coth,          expr_coth,          NULL),
    [ 38] = FUNC_ENTRY("factorial",     false, &ops_factorial,     expr_factorial,     NULL),
    [ 39] = FUNC_ENTRY("acosh",         false, &ops_acosh,         expr_acosh,         NULL),
    [ 41] = FUNC_ENTRY("asec",          false, &ops_asec,          expr_asec,          NULL),
    [ 43] = FUNC_ENTRY("ψ⁽¹⁾",          false, &ops_trigamma,      expr_trigamma,      NULL),
    [ 44] = FUNC_ENTRY("W_0",           false, &ops_lambert_w0,    expr_lambert_w0,    NULL),
    [ 46] = FUNC_ENTRY("gamma",         false, &ops_gamma,         expr_gamma,         NULL),
    [ 47] = FUNC_ENTRY("gammainc_P",    true,  &ops_gammainc_P,    NULL,             expr_gammainc_P),
    [ 48] = FUNC_ENTRY("sech",          false, &ops_sech,          expr_sech,          NULL),
    [ 49] = FUNC_ENTRY("gammainc_lower", true, &ops_gammainc_lower, NULL,            expr_gammainc_lower),
    [ 50] = FUNC_ENTRY("NOT",           false, &ops_bit_not,       expr_bit_not,       NULL),
    [ 51] = FUNC_ENTRY("tanh",          false, &ops_tanh,          expr_tanh,          NULL),
    [ 52] = FUNC_ENTRY("acosec",        false, &ops_acosec,        expr_acosec,        NULL),
    [ 53] = FUNC_ENTRY("acsc",          false, &ops_acosec,        expr_acosec,        NULL),
    [ 54] = FUNC_ENTRY("factors",       false, &ops_factors,       expr_factors,       NULL),
    [ 55] = FUNC_ENTRY("ln",            false, &ops_log,           expr_log,           NULL),
    [ 56] = FUNC_ENTRY("lambert_wm1",   false, &ops_lambert_wm1,   expr_lambert_wm1,   NULL),
    [ 57] = FUNC_ENTRY("erfcinv",       false, &ops_erfcinv,       expr_erfcinv,       NULL),
    [ 58] = FUNC_ENTRY("ψ⁽⁰⁾",          false, &ops_digamma,       expr_digamma,       NULL),
    [ 59] = FUNC_ENTRY("XOR",           true,  &ops_bit_xor,       NULL,             expr_bit_xor),
    [ 60] = FUNC_ENTRY("OR",            true,  &ops_bit_or,        NULL,             expr_bit_or),
    [ 61] = FUNC_ENTRY("normal_logpdf", false, &ops_normal_logpdf, expr_normal_logpdf, NULL),
    [ 62] = FUNC_ENTRY("csch",          false, &ops_cosech,        expr_cosech,        NULL),
    [ 63] = FUNC_ENTRY("arccsc",        false, &ops_acosec,        expr_acosec,        NULL),
    [ 64] = FUNC_ENTRY("acot",          false, &ops_acot,          expr_acot,          NULL),
    [ 65] = FUNC_ENTRY("normal_cdf",    false, &ops_normal_cdf,    expr_normal_cdf,    NULL),
    [ 66] = FUNC_ENTRY("pow",           true,  &ops_pow,           NULL,             expr_pow_xp),
    [ 67] = FUNC_ENTRY("cosech",        false, &ops_cosech,        expr_cosech,        NULL),
    [ 68] = FUNC_ENTRY("Ei",            false, &ops_ei,            expr_ei,            NULL),
    [ 69] = FUNC_ENTRY("sin",           false, &ops_sin,           expr_sin,           NULL),
    [ 70] = FUNC_ENTRY("arccosec",      false, &ops_acosec,        expr_acosec,        NULL),
    [ 71] = FUNC_ENTRY("partition",     false, &ops_partition,     expr_partition,     NULL),
    [ 72] = FUNC_ENTRY("W_-1",          false, &ops_lambert_wm1,   expr_lambert_wm1,   NULL),
    [ 73] = FUNC_ENTRY("erf",           false, &ops_erf,           expr_erf,           NULL),
    [ 74] = FUNC_ENTRY("E1",            false, &ops_e1,            expr_e1,            NULL),
    [ 75] = FUNC_ENTRY("W0",            false, &ops_lambert_w0,    expr_lambert_w0,    NULL),
    [ 76] = FUNC_ENTRY("asech",         false, &ops_asech,         expr_asech,         NULL),
    [ 77] = FUNC_ENTRY("polygamma",     true,  &ops_polygamma,     NULL,             expr_polygamma_xp),
    [ 78] = FUNC_ENTRY("acosech",       false, &ops_acosech,       expr_acosech,       NULL),
    [ 79] = FUNC_ENTRY("gammainc_upper", true, &ops_gammainc_upper, NULL,            expr_gammainc_upper),
    [ 80] = FUNC_ENTRY("erfc",          false, &ops_erfc,          expr_erfc,          NULL),
    [ 81] = FUNC_ENTRY("productlog",    false, &ops_lambert_w,     expr_lambert_w,     NULL),
    [ 82] = FUNC_ENTRY("gcd",           true,  &ops_gcd,           NULL,             expr_gcd),
    [ 83] = FUNC_ENTRY("sinh",          false, &ops_sinh,          expr_sinh,          NULL),
    [ 84] = FUNC_ENTRY("modinv",        true,  &ops_modinv,        NULL,             expr_modinv),
    [ 85] = FUNC_ENTRY("lcm",           true,  &ops_lcm,           NULL,             expr_lcm),
    [ 86] = FUNC_ENTRY("W₋₁",           false, &ops_lambert_wm1,   expr_lambert_wm1,   NULL),
    [ 87] = FUNC_ENTRY("cosh",          false, &ops_cosh,          expr_cosh,          NULL),
    [ 88] = FUNC_ENTRY("abs",           false, &ops_abs,           expr_abs,           NULL),
    [ 89] = FUNC_ENTRY("logbeta",       true,  &ops_logbeta,       NULL,             expr_logbeta),
    [ 90] = FUNC_ENTRY("lgamma",        false, &ops_lgamma,        expr_lgamma,        NULL),
    [ 91] = FUNC_ENTRY("cot",           false, &ops_cot,           expr_cot,           NULL),
    [ 92] = FUNC_ENTRY("mod",           true,  &ops_mod,           NULL,             expr_mod),
    [ 93] = FUNC_TERNARY_ENTRY("logbeta_pdf", expr_logbeta_pdf),
    [ 94] = FUNC_ENTRY("atan",          false, &ops_atan,          expr_atan,          NULL),
    [ 95] = FUNC_ENTRY("W-1",           false, &ops_lambert_wm1,   expr_lambert_wm1,   NULL),
    [ 96] = FUNC_ENTRY("ceil",          false, &ops_ceil,          expr_ceil,          NULL),
    [ 97] = FUNC_ENTRY("floor",         false, &ops_floor,         expr_floor,         NULL),
    [ 98] = FUNC_ENTRY("W₀",            false, &ops_lambert_w0,    expr_lambert_w0,    NULL),
    [ 99] = FUNC_ENTRY("arcsec",        false, &ops_asec,          expr_asec,          NULL),
    [100] = FUNC_ENTRY("W",             false, &ops_lambert_w,     expr_lambert_w,     NULL),
    [101] = FUNC_TERNARY_ENTRY("beta_pdf",    expr_beta_pdf),
    [102] = FUNC_ENTRY("cos",           false, &ops_cos,           expr_cos,           NULL),
    [103] = FUNC_ENTRY("exp",           false, &ops_exp,           expr_exp,           NULL),
    [104] = FUNC_ENTRY("csc",           false, &ops_cosec,         expr_cosec,         NULL),
    [105] = FUNC_ENTRY("acoth",         false, &ops_acoth,         expr_acoth,         NULL),
    [106] = FUNC_ENTRY("fibonacci",     false, &ops_fibonacci,     expr_fibonacci,     NULL),
};

static unsigned func_bucket_hash(const char *kw, size_t klen)
{
    const unsigned char *s = (const unsigned char *)kw;

    return ((unsigned)klen + 5u * s[0] + 3u * s[klen - 1u]) % FUNC_TABLE_SIZE;
}

static unsigned func_slot_hash(const char *kw, size_t klen)
{
    const unsigned char *s = (const unsigned char *)kw;
    unsigned h = 2166136261u;

    for (size_t i = 0; i < klen; i++)
        h = (h ^ s[i]) * 16777619u;

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

static int parse_two_args(parser_t *p, expr_t **a_out, expr_t **b_out)
{
    expr_t *a = parse_addexpr(p);
    if (!a) return 0;

    if (p->p >= p->end || *p->p != ',') {
        expr_free(a);
        set_error(p, "expected ',' in binary function");
        return 0;
    }
    p->p++;
    skip_spaces(&p->p, p->end);

    expr_t *b = parse_addexpr(p);
    if (!b) { expr_free(a); return 0; }

    *a_out = a;
    *b_out = b;
    return 1;
}

static int parse_three_args(parser_t *p,
                            expr_t **a_out,
                            expr_t **b_out,
                            expr_t **c_out)
{
    expr_t *a = parse_addexpr(p);
    expr_t *b;
    expr_t *c;

    if (!a)
        return 0;
    if (p->p >= p->end || *p->p != ',') {
        expr_free(a);
        set_error(p, "expected ',' in ternary function");
        return 0;
    }
    p->p++;
    skip_spaces(&p->p, p->end);

    b = parse_addexpr(p);
    if (!b) {
        expr_free(a);
        return 0;
    }
    if (p->p >= p->end || *p->p != ',') {
        expr_free(b);
        expr_free(a);
        set_error(p, "expected ',' in ternary function");
        return 0;
    }
    p->p++;
    skip_spaces(&p->p, p->end);

    c = parse_addexpr(p);
    if (!c) {
        expr_free(b);
        expr_free(a);
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

static int ascii_region_eq_ci(const char *s, const char *end, const char *kw)
{
    size_t len = strlen(kw);

    if ((size_t)(end - s) < len)
        return 0;
    for (size_t i = 0u; i < len; ++i) {
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)kw[i]))
            return 0;
    }
    return 1;
}

static int special_number_literal_boundary(const char *p, const char *end)
{
    if (p >= end)
        return 1;
    return !isalnum((unsigned char)*p) && *p != '_';
}

static size_t scan_special_number_literal_len(const char *s, const char *end)
{
    static const char *const specials[] = { "infinity", "nan", "inf" };

    for (size_t i = 0u; i < sizeof(specials) / sizeof(specials[0]); ++i) {
        size_t len = strlen(specials[i]);

        if (ascii_region_eq_ci(s, end, specials[i]) &&
            special_number_literal_boundary(s + len, end))
            return len;
    }
    return 0u;
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
    if (atom_len == 0u)
        atom_len = scan_special_number_literal_len(start, end);
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

static int node_has_preserved_constexpr(const expr_t *node)
{
    number_t value;
    int is_builtin_const;
    int value_matches_builtin = 0;

    if (!expr_is_const(node) || !node->binding_expr)
        return 0;
    if (!node->name || !*node->name)
        return 1;

    is_builtin_const = expr_get_default_constant_num(node->name, &value);
    if (is_builtin_const) {
        value_matches_builtin = num_eq(node->c, value);
        num_destroy(&value);
    }

    return expr_is_const(node) &&
           node->binding_expr &&
           is_builtin_const &&
           value_matches_builtin;
}

static expr_binding_expr_t *binding_expr_number_from_value_local(number_t value)
{
    char *text = num_to_string(value);
    expr_binding_expr_t *expr =
        expr_binding_expr_new_number_text(text ? text : "NAN");

    free(text);
    return expr;
}

static expr_t *const_node_from_binding_expr(expr_binding_expr_t *expr)
{
    expr_binding_expr_t *original_expr;
    number_t value;
    expr_t *node;

    if (!expr)
        return expr_new_const(NUM_NAN);

    original_expr = expr_binding_expr_clone(expr);
    expr = expr_binding_expr_simplify(expr);
    value = expr_binding_expr_eval(expr);
    if (!num_is_finite(value)) {
        expr_binding_expr_free(expr);
        node = expr_binding_expr_eval_expr(original_expr);
        if (node) {
            expr_binding_expr_free(node->binding_expr);
            node->binding_expr = original_expr;
        } else {
            expr_binding_expr_free(original_expr);
        }
        num_destroy(&value);
        return node ? node : expr_new_const(NUM_NAN);
    }
    expr_binding_expr_free(original_expr);
    node = expr_new_const(value);
    if (expr_binding_expr_is_numeric_literal(expr) &&
        (num_is_exact(value) || num_is_real(value))) {
        expr_binding_expr_free(expr);
        expr = binding_expr_number_from_value_local(value);
    }
    num_destroy(&value);
    node->binding_expr = expr;
    return node;
}

static expr_t *apply_unary_preserving_constexpr(const expr_ops_t *ops,
                                                expr_t *arg,
                                                expr_t *(*fallback)(const expr_t *))
{
    if (node_has_preserved_constexpr(arg)) {
        expr_binding_expr_t *child = expr_binding_expr_clone(arg->binding_expr);
        expr_binding_expr_t *expr = (ops == &ops_neg)
            ? expr_binding_expr_new_neg(child)
            : expr_binding_expr_new_unary_op(ops, child);
        expr_t *node = const_node_from_binding_expr(expr);

        expr_free(arg);
        return node;
    }

    {
        expr_t *node = fallback(arg);
        expr_free(arg);
        return node;
    }
}

static expr_binding_expr_t *binding_expr_for_binary_constexpr(const expr_ops_t *ops,
                                                            const expr_t *left,
                                                            const expr_t *right)
{
    expr_binding_expr_t *l = expr_binding_expr_clone(left->binding_expr);
    expr_binding_expr_t *r = expr_binding_expr_clone(right->binding_expr);

    if (ops == &ops_add)
        return expr_binding_expr_new_add(l, r);
    if (ops == &ops_sub)
        return expr_binding_expr_new_sub(l, r);
    if (ops == &ops_mul)
        return expr_binding_expr_new_mul(l, r);
    if (ops == &ops_div)
        return expr_binding_expr_new_div(l, r);
    if (ops == &ops_pow)
        return expr_binding_expr_new_binary_op(&ops_pow, l, r);

    return expr_binding_expr_new_binary_op(ops, l, r);
}

static expr_t *apply_binary_preserving_constexpr(const expr_ops_t *ops,
                                                 expr_t *left,
                                                 expr_t *right,
                                                 expr_t *(*fallback)(const expr_t *,
                                                                     const expr_t *))
{
    if (node_has_preserved_constexpr(left) &&
        node_has_preserved_constexpr(right)) {
        expr_binding_expr_t *expr = binding_expr_for_binary_constexpr(ops, left, right);
        expr_t *node = const_node_from_binding_expr(expr);

        expr_free(left);
        expr_free(right);
        return node;
    }

    {
        expr_t *node = fallback(left, right);
        expr_free(left);
        expr_free(right);
        return node;
    }
}

static expr_t *apply_pow_const_preserving_constexpr(expr_t *base, const number_t *exponent)
{
    if (node_has_preserved_constexpr(base)) {
        char *text = num_to_string(*exponent);
        expr_binding_expr_t *rhs = expr_binding_expr_new_number_text(text ? text : "NAN");
        expr_binding_expr_t *expr =
            expr_binding_expr_new_binary_op(&ops_pow,
                                          expr_binding_expr_clone(base->binding_expr),
                                          rhs);
        expr_t *node;

        free(text);
        node = const_node_from_binding_expr(expr);
        expr_free(base);
        return node;
    }

    {
        expr_t *node = expr_pow(base, exponent);
        expr_free(base);
        return node;
    }
}

static expr_t *apply_integer_power_if_present(expr_t *value, int exponent)
{
    NUM_SCOPE(scope);
    if (exponent < 0)
        return value;

    number_t exponent_num = num_create_from_long(exponent);
    expr_t *powered = apply_pow_const_preserving_constexpr(value, &exponent_num);
    return powered;
}

static expr_t *apply_factorial_postfix(expr_t *value)
{
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *incremented;

    if (!one) {
        expr_free(value);
        return NULL;
    }

    one->binding_expr = expr_binding_expr_new_number_text("1");
    incremented = apply_binary_preserving_constexpr(&ops_add, value, one, expr_add);
    if (!incremented)
        return NULL;
    return apply_unary_preserving_constexpr(&ops_gamma, incremented, expr_gamma);
}

static expr_t *parse_enclosed_addexpr(parser_t *p, char closing, const char *errmsg)
{
    expr_t *inner;

    skip_spaces(&p->p, p->end);
    inner = parse_addexpr(p);

    if (!inner)
        return NULL;
    skip_spaces(&p->p, p->end);
    if (!parse_required_char(p, closing, errmsg)) {
        expr_free(inner);
        return NULL;
    }
    return inner;
}

/* ------------------------------------------------------------------ */
/* Atom parser                                                          */
/* ------------------------------------------------------------------ */

static expr_t *parse_atom(parser_t *p)
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
        expr_t *inner = parse_enclosed_addexpr(p, '|', "expected '|'");
        if (!inner)
            return NULL;
        return apply_unary_preserving_constexpr(&ops_abs, inner, expr_abs);
    }

    /* Mathematical floor/ceiling brackets: ⌊expr⌋ and ⌈expr⌉ */
    if (cp_len > 0 && (cp == 0x230A || cp == 0x2308)) {
        const unsigned int closing = (cp == 0x230A) ? 0x230B : 0x2309;
        const char *errmsg = (cp == 0x230A) ? "expected '⌋'" : "expected '⌉'";
        expr_t *inner;
        expr_t *result;
        unsigned int close_cp = 0;
        int close_len;

        p->p += cp_len;
        inner = parse_addexpr(p);
        if (!inner)
            return NULL;
        skip_spaces(&p->p, p->end);

        close_len = fs_utf8_decode(p->p, &close_cp);
        if (close_len <= 0 || close_cp != closing) {
            expr_free(inner);
            set_error(p, errmsg);
            return NULL;
        }
        p->p += close_len;

        result = (cp == 0x230A)
            ? apply_unary_preserving_constexpr(&ops_floor, inner, expr_floor)
            : apply_unary_preserving_constexpr(&ops_ceil, inner, expr_ceil);
        return result;
    }

    /* Numeric atom (integer/decimal/rational, optionally with trailing i) */
    if (isdigit((unsigned char)*p->p) || *p->p == '.' ||
        scan_unicode_fraction_len(p->p, p->end) > 0u ||
        scan_special_number_literal_len(p->p, p->end) > 0u) {
        size_t len = scan_number_atom_len(p->p, p->end);
        size_t special_len = scan_special_number_literal_len(p->p, p->end);
        const char *start = p->p;
        number_t value;
        expr_t *node;
        char *text;

        if (len == 0u)
            len = special_len;
        if (len == 0 || !parse_number_region(p->p, p->p + len, &value)) {
            set_error(p, "expected numeric literal");
            return NULL;
        }
        p->p += len;
        node = expr_new_const(value);
        if (special_len > 0u && ascii_region_eq_ci(start, start + special_len, "nan")) {
            text = (char *)fs_xmalloc(4u);
            memcpy(text, "NAN", 4u);
        } else if (num_is_exact(value)) {
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
        node->binding_expr = expr_binding_expr_new_number_text(text);
        free(text);
        return node;
    }

    if (cp_len > 0 && cp == 0x221A) {
        p->p += cp_len;
        int sup = read_optional_display_exponent(&p->p);

        if (!parse_required_char(p, '(', "expected '(' after √"))
            return NULL;

        expr_t *arg = parse_enclosed_addexpr(p, ')', "expected ')' after √ argument");
        expr_t *result;
        if (!arg)
            return NULL;
        result = apply_unary_preserving_constexpr(&ops_sqrt, arg, expr_sqrt);
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
                expr_t *a = NULL;
                expr_t *b = NULL;
                expr_t *result;

                if (!parse_two_args(p, &a, &b))
                    return NULL;
                if (!parse_required_char(p, ')', "expected ')' after binary function")) {
                    expr_free(a);
                    expr_free(b);
                    return NULL;
                }
                result = fe->ops
                    ? apply_binary_preserving_constexpr(fe->ops, a, b, fe->bfn)
                    : fe->bfn(a, b);
                if (!fe->ops) {
                    expr_free(a);
                    expr_free(b);
                }
                return apply_integer_power_if_present(result, sup);
            } else if (fe->arity == 3u) {
                expr_t *a = NULL;
                expr_t *b = NULL;
                expr_t *c = NULL;
                expr_t *result;

                if (!parse_three_args(p, &a, &b, &c))
                    return NULL;
                if (!parse_required_char(p, ')', "expected ')' after ternary function")) {
                    expr_free(a);
                    expr_free(b);
                    expr_free(c);
                    return NULL;
                }
                result = fe->tfn(a, b, c);
                expr_free(a);
                expr_free(b);
                expr_free(c);
                return apply_integer_power_if_present(result, sup);
            } else {
                expr_t *arg = parse_enclosed_addexpr(
                    p, ')', "expected ')' after function argument");
                expr_t *result;

                if (!arg)
                    return NULL;
                if (fe->ops == &ops_factors) {
                    result = fe->ufn(arg);
                    expr_free(arg);
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
            expr_t *arg;
            expr_t *result;

            p->p = paren + 1;
            arg = parse_enclosed_addexpr(
                p, ')', "expected ')' after polygamma argument");
            if (!arg)
                return NULL;
            result = expr_polygamma(order, arg);
            expr_free(arg);
            return result;
        }
    }

    /* Simple name (single Unicode letter + subscript digits) or [bracketed name] */
    char *name = read_any_name(&p->p);
    if (!name) {
        set_error(p, "expected expression");
        return NULL;
    }

    expr_t *sym = symtab_lookup(p->syms, name);
    if (!sym) {
        char *normalised = expr_normalise_name(expr_default_constant_canonical_name(name));

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
    expr_retain(sym); /* give caller an owning reference */
    free(name);
    return sym;
}

/* ------------------------------------------------------------------ */
/* Power parser                                                         */
/* ------------------------------------------------------------------ */

static expr_t *parse_power_operand(parser_t *p)
{
    expr_t *base = parse_atom(p);
    if (!base) return NULL;

    while (p->p < p->end && *p->p == '!') {
        p->p++;
        base = apply_factorial_postfix(base);
        if (!base)
            return NULL;
    }

    return base;
}

static expr_t *parse_power(parser_t *p)
{
    NUM_SCOPE(scope);
    if (p->error) return NULL;

    expr_t *base = parse_power_operand(p);
    if (!base) return NULL;

    for (;;) {
        expr_t *result = NULL;

        /* Unicode superscript exponent: x² */
        int sup = read_superscript(&p->p);
        if (sup >= 0) {
            base = apply_integer_power_if_present(base, sup);
            if (!base)
                return NULL;
            continue;
        }

        /* Caret exponent: chained powers are left-associative in expr syntax:
         * a^x^2 means (a^x)^2. Use explicit parentheses for powers inside
         * the exponent: a^(x^2). */
        if (p->p >= p->end || *p->p != '^')
            break;

        expr_t *exponent = NULL;

        p->p++;
        skip_spaces(&p->p, p->end);

        if (p->p < p->end && *p->p == '(') {
            p->p++;
            exponent = parse_enclosed_addexpr(p, ')', "expected ')' after exponent");
        } else {
            exponent = parse_signed_power_operand(p);
        }

        if (!exponent) {
            expr_free(base);
            set_error(p, "expected exponent after '^'");
            return NULL;
        }

        if (expr_is_unnamed_const(exponent) && num_is_real(exponent->c) &&
            (!exponent->binding_expr || !node_has_preserved_constexpr(base))) {
            result = apply_pow_const_preserving_constexpr(base, &exponent->c);
            expr_free(exponent);
        } else {
            result = apply_binary_preserving_constexpr(&ops_pow, base, exponent, expr_pow_xp);
        }
        if (!result)
            return result;
        base = result;
    }

    return base;
}

/* ------------------------------------------------------------------ */
/* Signed factor (unary minus)                                         */
/* ------------------------------------------------------------------ */

static expr_t *parse_signed_power(parser_t *p)
{
    int negate = 0;
    expr_t *inner;

    if (p->error) return NULL;

    while (p->p < p->end && (*p->p == '-' || *p->p == '+')) {
        if (*p->p == '-')
            negate = !negate;
        p->p++;
        skip_spaces(&p->p, p->end);
    }

    inner = parse_power(p);
    if (!inner) return NULL;
    return negate ? apply_unary_preserving_constexpr(&ops_neg, inner, expr_neg) : inner;
}

static expr_t *parse_signed_power_operand(parser_t *p)
{
    int negate = 0;
    expr_t *inner;

    if (p->error) return NULL;

    while (p->p < p->end && (*p->p == '-' || *p->p == '+')) {
        if (*p->p == '-')
            negate = !negate;
        p->p++;
        skip_spaces(&p->p, p->end);
    }

    inner = parse_power_operand(p);
    if (!inner) return NULL;
    return negate ? apply_unary_preserving_constexpr(&ops_neg, inner, expr_neg) : inner;
}

/* ------------------------------------------------------------------ */
/* Multiplication / division (implicit, '*', '·', '/')                 */
/* ------------------------------------------------------------------ */

static expr_t *parse_mulexpr(parser_t *p)
{
    if (p->error) return NULL;
    expr_t *lhs = parse_signed_power(p);
    if (!lhs) return NULL;

    for (;;) {
        if (p->p >= p->end) break;

        /* Explicit middle dot '·' */
        if (at_middle_dot(p)) {
            p->p += 2;
            expr_t *rhs = parse_signed_power(p);
            if (!rhs) { expr_free(lhs); return NULL; }
            lhs = apply_binary_preserving_constexpr(&ops_mul, lhs, rhs, expr_mul);
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
                expr_t *rhs = parse_signed_power(p);
                if (!rhs) { expr_free(lhs); return NULL; }
                lhs = (op == '*')
                    ? apply_binary_preserving_constexpr(&ops_mul, lhs, rhs, expr_mul)
                    : apply_binary_preserving_constexpr(&ops_div, lhs, rhs, expr_div);
                continue;
            }
        }

        /* Implicit multiplication: next position can start a factor */
        if (can_start_factor(p)) {
            expr_t *rhs = parse_signed_power(p);
            if (!rhs) { expr_free(lhs); return NULL; }
            lhs = apply_binary_preserving_constexpr(&ops_mul, lhs, rhs, expr_mul);
            continue;
        }

        break;
    }
    return lhs;
}

/* ------------------------------------------------------------------ */
/* Addition / subtraction                                              */
/* ------------------------------------------------------------------ */

static expr_t *parse_addexpr(parser_t *p)
{
    if (p->error) return NULL;
    expr_t *lhs = parse_mulexpr(p);
    if (!lhs) return NULL;

    for (;;) {
        const char *peek = p->p;

        while (peek < p->end && *peek == ' ')
            peek++;

        if (peek < p->end && (*peek == '+' || *peek == '-')) {
            char op = *peek;

            p->p = peek + 1;
            skip_spaces(&p->p, p->end);
            expr_t *rhs = parse_mulexpr(p);
            if (!rhs) { expr_free(lhs); return NULL; }
            lhs = (op == '+')
                ? apply_binary_preserving_constexpr(&ops_add, lhs, rhs, expr_add)
                : apply_binary_preserving_constexpr(&ops_sub, lhs, rhs, expr_sub);
            continue;
        }

        break;
    }
    return lhs;
}

/* ------------------------------------------------------------------ */
/* Binding section parser                                               */
/* ------------------------------------------------------------------ */

static void binding_parse_error(char *errmsg,
                                size_t errmsg_n,
                                const char *name,
                                const char *value_start,
                                const char *value_end)
{
    const size_t max_value = 160u;
    size_t value_len;
    int value_width;
    const char *suffix;

    if (!errmsg || errmsg_n == 0u)
        return;

    while (value_start < value_end && isspace((unsigned char)*value_start))
        value_start++;
    while (value_end > value_start && isspace((unsigned char)value_end[-1]))
        value_end--;

    value_len = (size_t)(value_end - value_start);
    suffix = value_len > max_value ? "..." : "";
    if (value_len > max_value)
        value_len = max_value;
    value_width = value_len > (size_t)INT_MAX ? INT_MAX : (int)value_len;

    snprintf(errmsg, errmsg_n,
             "incorrect syntax for %.80s: %.*s%s",
             name ? name : "",
             value_width,
             value_start,
             suffix);
}

/* Parse comma/semicolon-separated "name = value" pairs from [s, end).
 * is_var: 1 → create expr_new_named_var(); 0 → create expr_new_named_const().
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
        const char *value_start = p;
        expr_binding_expr_t *binding_expr;
        number_t val;
        expr_t *node;

        binding_expr = expr_binding_expr_parse_region(p, value_end, errmsg, errmsg_n);
        if (!binding_expr) {
            binding_parse_error(errmsg, errmsg_n, name, value_start, value_end);
            free(name);
            return -1;
        }
        binding_expr = expr_binding_expr_simplify(binding_expr);
        val = expr_binding_expr_eval(binding_expr);
        p = value_end;

        node = is_var
            ? expr_new_named_var(val, name)
            : expr_new_named_const(val, name);
        num_destroy(&val);
        node->binding_expr = binding_expr;

        /* expr_new_named_* calls expr_normalise_name, which may transform the name
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
            expr_free(node);
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

static expr_t *parse_pure_const(const char *s, const char *end,
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

    expr_binding_expr_t *binding_expr = expr_binding_expr_parse_region(p, end, errmsg, errmsg_n);
    if (!binding_expr) {
        free(name);
        return NULL;
    }
    binding_expr = expr_binding_expr_simplify(binding_expr);
    number_t val = expr_binding_expr_eval(binding_expr);

    if (!name) {
        expr_binding_expr_free(binding_expr);
        snprintf(errmsg, errmsg_n, "constant name is required in pure-constant format");
        return NULL;
    }
    expr_t *result = expr_new_named_const(val, name);
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
        size_t special_len = scan_special_number_literal_len(p, end);

        if (special_len > 0u) {
            p += special_len;
            continue;
        }

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
        expr_t *node;
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

        is_const = expr_is_default_constant_name(name);
        if (expr_get_default_constant_num(name, &value)) {
            char bind_err[128];

            canonical_name = expr_default_constant_canonical_name(name);
            node = expr_new_named_const(value, canonical_name);
            node->binding_expr =
                expr_binding_expr_parse_region(name, name + strlen(name),
                                             bind_err, sizeof(bind_err));
        } else {
            node = is_const
                ? expr_new_named_const(NUM_NAN, name)
                : expr_new_named_var(NUM_NAN, name);
        }

        key = expr_normalise_name(canonical_name);
        if (!key)
            key = strdup(canonical_name);
        if (!key) {
            expr_free(node);
            free(name);
            return -1;
        }

        if (symtab_has(syms, key)) {
            free(key);
            expr_free(node);
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

static expr_t *parse_expression_region(const char *start,
                                       const char *end,
                                       symtab_t *syms,
                                       const char *context_label,
                                       int report_errors)
{
    parser_t ps;
    expr_t *result;

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
        expr_free(result);
        result = NULL;
        set_error(&ps, "trailing input");
    } else if (result) {
        expr_free(result);
        result = NULL;
    }

    (void)context_label;
    if (ps.error && report_errors)
        fprintf(stderr, "parse error: %s\n", ps.errmsg);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Public entry point                                                   */
/* ------------------------------------------------------------------ */

static expr_t *simplify_parsed_result(expr_t *result)
{
    expr_t *simplified;

    if (!result)
        return NULL;

    simplified = expr_simplify(result);
    if (!simplified)
        return result;

    expr_free(result);
    return simplified;
}

static expr_t *expr_from_string_impl(const char *s,
                                     expr_bindings_t **bindings_out)
{
    expr_bindings_t *bindings = NULL;

    if (bindings_out)
        *bindings_out = NULL;
    if (!s) return NULL;

    while (isspace((unsigned char)*s)) s++;
    if (*s != '{') {
        fprintf(stderr, "expected '{'\n");
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
        if (pipe_pos && *scan == '}') {
            close_pos = scan;
            break;
        }
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
        if (depth != 0) {
            const char *body_start = s;
            const char *body_end = scan;

            while (body_start < body_end && isspace((unsigned char)*body_start))
                body_start++;
            while (body_end > body_start && isspace((unsigned char)body_end[-1]))
                body_end--;
            if (body_end > body_start && body_end[-1] == '}') {
                body_end--;
                while (body_end > body_start && isspace((unsigned char)body_end[-1]))
                    body_end--;
            }
            fprintf(stderr, "syntax error: %.*s\n",
                    (int)(body_end - body_start),
                    body_start);
            return NULL;
        }
        fprintf(stderr, "expected '}'\n");
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

        expr_t *result = parse_expression_region(s, content_end, NULL,
                                                 "expr_from_string", 0);

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
                                                 "expr_from_string", 1);
            }
            if (result && bindings_out)
                bindings = symtab_build_bindings(&syms);
            if (result && bindings_out)
                symtab_discard_storage(&syms);
            else
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
            fprintf(stderr, "%s\n", errmsg);
        else {
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
        fprintf(stderr, "%s\n", errmsg);
        return NULL;
    }
    if (semi_pos) {
        if (parse_bindings(semi_pos + 1, bind_end, 0, &syms,
                           errmsg, sizeof(errmsg)) < 0) {
            symtab_free(&syms);
            fprintf(stderr, "%s\n", errmsg);
            return NULL;
        }
    }

    if (collect_implicit_symbols(s, expr_end, &syms) < 0) {
        symtab_free(&syms);
        fprintf(stderr, "out of memory\n");
        return NULL;
    }

    expr_t *result = parse_expression_region(s, expr_end, &syms,
                                             "expr_from_string", 1);
    if (result && bindings_out) {
        bindings = symtab_build_bindings(&syms);
    }

    if (result && bindings_out) {
        symtab_discard_storage(&syms);
    } else {
        symtab_free(&syms);
    }
    if (result && bindings_out)
        *bindings_out = bindings;
    return result;
}

expr_t *expr_from_string(const char *s, expr_bindings_t **bnd_out)
{
    return expr_from_string_impl(s, bnd_out);
}

static expr_binding_entry_t *bnd_find_entry(expr_bindings_t *bnd,
                                            const char *name)
{
    char *norm;
    expr_binding_entry_t *entry = NULL;

    if (!bnd || !bnd->index || !name)
        return NULL;

    norm = expr_normalise_binding_name(name);
    if (!norm)
        return NULL;

    dictionary_get(bnd->index, &norm, &entry);
    free(norm);
    return entry;
}

expr_t *expr_bindings_get(expr_bindings_t *bnd, const char *name)
{
    expr_binding_entry_t *entry = bnd_find_entry(bnd, name);

    return entry ? entry->expr : NULL;
}

void expr_bindings_free(expr_bindings_t *bnd)
{
    if (!bnd)
        return;
    for (size_t i = 0; i < bnd->count; ++i)
        expr_free(bnd->entries[i].expr);
    dictionary_destroy(bnd->index);
    free(bnd->storage);
    free(bnd);
}

expr_t *expr_from_expression_string(const char *expr,
                                    const char *const *names,
                                    expr_t *const *symbols,
                                    size_t nsymbols)
{
    symtab_t syms;
    expr_t *result;

    if (!expr)
        return NULL;
    if (nsymbols > 0 && (!names || !symbols)) {
        fprintf(stderr,
                "expr_from_expression_string: symbol table is incomplete\n");
        return NULL;
    }
    if (nsymbols > (size_t)INT_MAX) {
        fprintf(stderr,
                "expr_from_expression_string: too many symbols\n");
        return NULL;
    }

    symtab_init(&syms);
    for (size_t i = 0; i < nsymbols; ++i) {
        if (!names[i] || !symbols[i]) {
            fprintf(stderr,
                    "expr_from_expression_string: null symbol entry\n");
            symtab_free(&syms);
            return NULL;
        }
        if (symtab_has(&syms, names[i])) {
            fprintf(stderr,
                    "expr_from_expression_string: duplicate symbol '%s'\n",
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
                                     "expr_from_expression_string", 1);
    symtab_free(&syms);
    result = simplify_parsed_result(result);
    return result;
}
