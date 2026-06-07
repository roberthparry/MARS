/* expr_fromstring.c - construct a expr_t from an expression-style string
 *
 * Accepts strings in the format produced by expr_to_text(f, style_EXPRESSION):
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
#include "expr_parse_text.h"
#include "expression.h"
#include "ustring.h"

/* ------------------------------------------------------------------ */
/* Parser state                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    string_cursor_t    *cursor;
    symtab_t            *syms;
    int                  error;
    string_t            *errmsg;
} expr_parse_state_t;

static void set_error(expr_parse_state_t *p, const char *msg)
{
    if (!p->error) {
        p->error = 1;
        if (p->errmsg)
            string_append_cstr(p->errmsg, msg ? msg : "");
    }
}

static int expr_parse_state_init(expr_parse_state_t *p,
                                 string_view_t text,
                                 symtab_t *syms)
{
    if (!p)
        return -1;

    p->cursor = string_cursor_new_view(text);
    if (!p->cursor)
        return -1;
    p->syms = syms;
    p->error = 0;
    p->errmsg = string_new();

    if (!p->errmsg) {
        string_cursor_free(p->cursor);
        p->cursor = NULL;
        return -1;
    }
    return 0;
}

static void expr_parse_state_dispose(expr_parse_state_t *p)
{
    if (!p)
        return;

    string_cursor_free(p->cursor);
    p->cursor = NULL;
    string_free(p->errmsg);
    p->errmsg = NULL;
}

static size_t expr_parse_pos(const expr_parse_state_t *p)
{
    return string_cursor_position(p->cursor);
}

static int expr_parse_at_end(const expr_parse_state_t *p)
{
    return string_cursor_done(p->cursor);
}

static int expr_parse_peek_ascii(const expr_parse_state_t *p, unsigned char *out)
{
    return string_cursor_peek_ascii(p->cursor, out);
}

static int expr_parse_peek_value(const expr_parse_state_t *p,
                                 uint32_t *out,
                                 size_t *width_out)
{
    return expr_parse_cursor_peek_value(p ? p->cursor : NULL,
                                        out,
                                        width_out);
}

static int expr_parse_skip(expr_parse_state_t *p, size_t count)
{
    return string_cursor_skip(p->cursor, count);
}

static int expr_parse_set_pos(expr_parse_state_t *p, size_t pos)
{
    return string_cursor_seek(p->cursor, pos) == 0;
}

static int expr_parse_consume_char(expr_parse_state_t *p, unsigned char ch)
{
    return expr_parse_cursor_consume_char(p->cursor, (char)ch);
}

static void expr_parse_skip_spaces(expr_parse_state_t *p)
{
    string_cursor_skip_spaces(p->cursor);
}

static string_view_t expr_parse_text(const expr_parse_state_t *p)
{
    return string_cursor_view_between(0u,
                                      string_cursor_end_position(p->cursor),
                                      p->cursor);
}

/* ------------------------------------------------------------------ */
/* Forward declarations                                                 */
/* ------------------------------------------------------------------ */

static expr_t *parse_addexpr(expr_parse_state_t *p);
static expr_t *parse_signed_power(expr_parse_state_t *p);
static expr_t *parse_signed_power_operand(expr_parse_state_t *p);
static size_t scan_unicode_fraction_len_view(string_view_t view, size_t pos);
static size_t scan_special_number_literal_len_view(string_view_t view, size_t pos);
static size_t scan_number_atom_len_view(string_view_t view, size_t pos);
static expr_t *parse_expression_view(string_view_t text,
                                     symtab_t *syms,
                                     const char *context_label,
                                     int report_errors);
static void symtab_discard_storage(symtab_t *t);

/* True if we're at the middle dot · (U+00B7, UTF-8: 0xC2 0xB7). */
static int at_middle_dot(const expr_parse_state_t *p)
{
    uint32_t cp = 0u;

    return expr_parse_cursor_peek_value_at(p->cursor, expr_parse_pos(p),
                                    &cp, NULL) &&
           cp == 0x00B7u;
}

/* True if the current position can start a new multiplication factor.
 * Spaces are NOT skipped — they only appear before binary '+'/'-'. */
static int can_start_factor(const expr_parse_state_t *p)
{
    size_t pos = expr_parse_pos(p);
    string_view_t text = expr_parse_text(p);
    unsigned char c = 0u;
    uint32_t uc = 0u;
    size_t len = 0u;

    if (at_middle_dot(p)) return 1;
    if (scan_unicode_fraction_len_view(text, pos) > 0u) return 1;
    if (scan_special_number_literal_len_view(text, pos) > 0u) return 1;
    if (expr_parse_peek_value(p, &uc, &len) &&
        (fs_is_letter(uc) || uc == 0x230A || uc == 0x2308))
        return 1;
    if (!expr_parse_peek_ascii(p, &c))
        return 0;
    if (c == ')' || c == '}' || c == ',' || c == ';' || c == '|') return 0;
    if (c == ' ') return 0;
    if (c == '[' || c == '(' || c == '@') return 1;
    if (isdigit(c) || c == '.') return 1;
    return 0;
}

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
      0,   1,   0,   1,   1,   0,  11,   0,   0,   1,
      0,  15,   0,   1,  10,   2,   1,   0,   0,   3,
      2,   2,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   2,   3,   1,   0,   1,
     10,  22,   0,   1,   0,   0,  10,   2,   1,   0,
     51,  17,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   3,   0,   5,  27,  16,   0,   0,  14,
      1,   2,   0,   4,   6,   0,   0,   0,   0,   0,
     26,   0,   1,   0,  58,   7,   0,   4,   8,  38,
     10,   0,   0,   0,   7,  50,   0,  55,   0,  85,
      0,   0,   6,   0,   0,   1,   0
};

static const func_entry_t s_funcs[FUNC_TABLE_SIZE] = {
    [  0] = FUNC_ENTRY("SHR",           true,  &ops_shr,           NULL,             expr_shr),
    [  1] = FUNC_ENTRY("hypot",         true,  &ops_hypot,         NULL,             expr_hypot),
    [  2] = FUNC_ENTRY("tan",           false, &ops_tan,           expr_tan,           NULL),
    [  3] = FUNC_ENTRY("asin",          false, &ops_asin,          expr_asin,          NULL),
    [  4] = FUNC_ENTRY("beta",          true,  &ops_beta,          NULL,             expr_beta),
    [  5] = FUNC_ENTRY("acos",          false, &ops_acos,          expr_acos,          NULL),
    [  6] = FUNC_ENTRY("digamma",       false, &ops_digamma,       expr_digamma,       NULL),
    [  7] = FUNC_ENTRY("csc",           false, &ops_cosec,         expr_cosec,         NULL),
    [  8] = FUNC_ENTRY("SHL",           true,  &ops_shl,           NULL,             expr_shl),
    [  9] = FUNC_ENTRY("lambert_w0",    false, &ops_lambert_w0,    expr_lambert_w0,    NULL),
    [ 10] = FUNC_ENTRY("arccot",        false, &ops_acot,          expr_acot,          NULL),
    [ 11] = FUNC_ENTRY("binomial",      true,  NULL,                NULL,             expr_binomial),
    [ 12] = FUNC_ENTRY("trigamma",      false, &ops_trigamma,      expr_trigamma,      NULL),
    [ 13] = FUNC_ENTRY("arsech",        false, &ops_asech,         expr_asech,         NULL),
    [ 14] = FUNC_ENTRY("gammainv",      false, &ops_gammainv,      expr_gammainv,      NULL),
    [ 15] = FUNC_ENTRY("abs",           false, &ops_abs,           expr_abs,           NULL),
    [ 16] = FUNC_ENTRY("arcosech",      false, &ops_acosech,       expr_acosech,       NULL),
    [ 17] = FUNC_ENTRY("arcoth",        false, &ops_acoth,         expr_acoth,         NULL),
    [ 18] = FUNC_ENTRY("normal_pdf",    false, &ops_normal_pdf,    expr_normal_pdf,    NULL),
    [ 19] = FUNC_ENTRY("sqrt",          false, &ops_sqrt,          expr_sqrt,          NULL),
    [ 20] = FUNC_ENTRY("logbeta",       true,  &ops_logbeta,       NULL,             expr_logbeta),
    [ 21] = FUNC_ENTRY("prev_prime",    false, &ops_prev_prime,    expr_prev_prime,    NULL),
    [ 22] = FUNC_ENTRY("log10",         false, &ops_log10,         expr_log10,         NULL),
    [ 23] = FUNC_ENTRY("asinh",         false, &ops_asinh,         expr_asinh,         NULL),
    [ 24] = FUNC_ENTRY("sec",           false, &ops_sec,           expr_sec,           NULL),
    [ 25] = FUNC_ENTRY("next_prime",    false, &ops_next_prime,    expr_next_prime,    NULL),
    [ 26] = FUNC_ENTRY("cot",           false, &ops_cot,           expr_cot,           NULL),
    [ 27] = FUNC_ENTRY("mod",           true,  &ops_mod,           NULL,             expr_mod),
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
    [ 38] = FUNC_ENTRY("W-1",           false, &ops_lambert_wm1,   expr_lambert_wm1,   NULL),
    [ 39] = FUNC_ENTRY("acosh",         false, &ops_acosh,         expr_acosh,         NULL),
    [ 40] = FUNC_ENTRY("factorial",     false, &ops_factorial,     expr_factorial,     NULL),
    [ 41] = FUNC_ENTRY("exp",           false, &ops_exp,           expr_exp,           NULL),
    [ 42] = FUNC_ENTRY("asec",          false, &ops_asec,          expr_asec,          NULL),
    [ 43] = FUNC_ENTRY("ψ⁽¹⁾",          false, &ops_trigamma,      expr_trigamma,      NULL),
    [ 44] = FUNC_ENTRY("W_0",           false, &ops_lambert_w0,    expr_lambert_w0,    NULL),
    [ 46] = FUNC_ENTRY("gamma",         false, &ops_gamma,         expr_gamma,         NULL),
    [ 47] = FUNC_ENTRY("gammainc_P",    true,  &ops_gammainc_P,    NULL,             expr_gammainc_P),
    [ 48] = FUNC_ENTRY("NOT",           false, &ops_bit_not,       expr_bit_not,       NULL),
    [ 49] = FUNC_ENTRY("sech",          false, &ops_sech,          expr_sech,          NULL),
    [ 50] = FUNC_ENTRY("atan2",         true,  &ops_atan2,         NULL,             expr_atan2),
    [ 51] = FUNC_ENTRY("gammainc_lower", true, &ops_gammainc_lower, NULL,            expr_gammainc_lower),
    [ 52] = FUNC_ENTRY("acosec",        false, &ops_acosec,        expr_acosec,        NULL),
    [ 53] = FUNC_ENTRY("lambert_wm1",   false, &ops_lambert_wm1,   expr_lambert_wm1,   NULL),
    [ 54] = FUNC_ENTRY("acsc",          false, &ops_acosec,        expr_acosec,        NULL),
    [ 55] = FUNC_ENTRY("ln",            false, &ops_log,           expr_log,           NULL),
    [ 56] = FUNC_ENTRY("acot",          false, &ops_acot,          expr_acot,          NULL),
    [ 57] = FUNC_ENTRY("erfcinv",       false, &ops_erfcinv,       expr_erfcinv,       NULL),
    [ 58] = FUNC_ENTRY("tanh",          false, &ops_tanh,          expr_tanh,          NULL),
    [ 59] = FUNC_ENTRY("XOR",           true,  &ops_bit_xor,       NULL,             expr_bit_xor),
    [ 60] = FUNC_ENTRY("normal_logpdf", false, &ops_normal_logpdf, expr_normal_logpdf, NULL),
    [ 61] = FUNC_ENTRY("OR",            true,  &ops_bit_or,        NULL,             expr_bit_or),
    [ 62] = FUNC_ENTRY("csch",          false, &ops_cosech,        expr_cosech,        NULL),
    [ 63] = FUNC_ENTRY("arccsc",        false, &ops_acosec,        expr_acosec,        NULL),
    [ 64] = FUNC_ENTRY("factors",       false, &ops_factors,       expr_factors,       NULL),
    [ 65] = FUNC_ENTRY("normal_cdf",    false, &ops_normal_cdf,    expr_normal_cdf,    NULL),
    [ 66] = FUNC_ENTRY("pow",           true,  &ops_pow,           NULL,             expr_pow_xp),
    [ 67] = FUNC_ENTRY("cosech",        false, &ops_cosech,        expr_cosech,        NULL),
    [ 68] = FUNC_ENTRY("ψ⁽⁰⁾",          false, &ops_digamma,       expr_digamma,       NULL),
    [ 69] = FUNC_ENTRY("sin",           false, &ops_sin,           expr_sin,           NULL),
    [ 70] = FUNC_ENTRY("Ei",            false, &ops_ei,            expr_ei,            NULL),
    [ 71] = FUNC_ENTRY("arccosec",      false, &ops_acosec,        expr_acosec,        NULL),
    [ 72] = FUNC_ENTRY("partition",     false, &ops_partition,     expr_partition,     NULL),
    [ 73] = FUNC_ENTRY("W_-1",          false, &ops_lambert_wm1,   expr_lambert_wm1,   NULL),
    [ 74] = FUNC_ENTRY("acosech",       false, &ops_acosech,       expr_acosech,       NULL),
    [ 75] = FUNC_ENTRY("pdf",           false, &ops_pdf,           expr_pdf,           NULL),
    [ 76] = FUNC_ENTRY("asech",         false, &ops_asech,         expr_asech,         NULL),
    [ 77] = FUNC_ENTRY("polygamma",     true,  &ops_polygamma,     NULL,             expr_polygamma_xp),
    [ 78] = FUNC_ENTRY("erf",           false, &ops_erf,           expr_erf,           NULL),
    [ 79] = FUNC_ENTRY("productlog",    false, &ops_lambert_w,     expr_lambert_w,     NULL),
    [ 80] = FUNC_ENTRY("erfc",          false, &ops_erfc,          expr_erfc,          NULL),
    [ 81] = FUNC_ENTRY("gammainc_upper", true, &ops_gammainc_upper, NULL,            expr_gammainc_upper),
    [ 82] = FUNC_ENTRY("lgamma",        false, &ops_lgamma,        expr_lgamma,        NULL),
    [ 83] = FUNC_ENTRY("modinv",        true,  &ops_modinv,        NULL,             expr_modinv),
    [ 84] = FUNC_ENTRY("sinh",          false, &ops_sinh,          expr_sinh,          NULL),
    [ 85] = FUNC_ENTRY("W0",            false, &ops_lambert_w0,    expr_lambert_w0,    NULL),
    [ 86] = FUNC_ENTRY("lg",            false, &ops_log10,         expr_log10,         NULL),
    [ 87] = FUNC_ENTRY("cosh",          false, &ops_cosh,          expr_cosh,          NULL),
    [ 88] = FUNC_ENTRY("log",           false, &ops_log10,         expr_log10,         NULL),
    [ 89] = FUNC_ENTRY("AND",           true,  &ops_bit_and,       NULL,             expr_bit_and),
    [ 90] = FUNC_ENTRY("E1",            false, &ops_e1,            expr_e1,            NULL),
    [ 91] = FUNC_ENTRY("logpdf",        false, &ops_logpdf,        expr_logpdf,        NULL),
    [ 92] = FUNC_ENTRY("gcd",           true,  &ops_gcd,           NULL,             expr_gcd),
    [ 93] = FUNC_ENTRY("atan",          false, &ops_atan,          expr_atan,          NULL),
    [ 94] = FUNC_TERNARY_ENTRY("logbeta_pdf", expr_logbeta_pdf),
    [ 95] = FUNC_ENTRY("lcm",           true,  &ops_lcm,           NULL,             expr_lcm),
    [ 96] = FUNC_ENTRY("W",             false, &ops_lambert_w,     expr_lambert_w,     NULL),
    [ 97] = FUNC_TERNARY_ENTRY("beta_pdf",    expr_beta_pdf),
    [ 98] = FUNC_ENTRY("floor",         false, &ops_floor,         expr_floor,         NULL),
    [ 99] = FUNC_ENTRY("arcsec",        false, &ops_asec,          expr_asec,          NULL),
    [100] = FUNC_ENTRY("fibonacci",     false, &ops_fibonacci,     expr_fibonacci,     NULL),
    [101] = FUNC_ENTRY("ceil",          false, &ops_ceil,          expr_ceil,          NULL),
    [102] = FUNC_ENTRY("cdf",           false, &ops_cdf,           expr_cdf,           NULL),
    [103] = FUNC_ENTRY("W₀",            false, &ops_lambert_w0,    expr_lambert_w0,    NULL),
    [104] = FUNC_ENTRY("cos",           false, &ops_cos,           expr_cos,           NULL),
    [105] = FUNC_ENTRY("acoth",         false, &ops_acoth,         expr_acoth,         NULL),
    [106] = FUNC_ENTRY("W₋₁",           false, &ops_lambert_wm1,   expr_lambert_wm1,   NULL),
};

static unsigned func_bucket_hash(string_view_t kw)
{
    size_t len = string_view_length(kw);
    unsigned char first = 0u;
    unsigned char last = 0u;

    if (!expr_parse_view_peek_ascii(kw, 0u, &first) ||
        !expr_parse_view_peek_ascii(kw, len - 1u, &last))
        return 0u;

    return ((unsigned)len + 5u * first + 3u * last) % FUNC_TABLE_SIZE;
}

static unsigned func_slot_hash(string_view_t kw)
{
    size_t len = string_view_length(kw);
    unsigned h = 2166136261u;

    for (size_t i = 0; i < len; i++) {
        unsigned char b = 0u;

        if (expr_parse_view_peek_ascii(kw, i, &b))
            h = (h ^ b) * 16777619u;
    }

    return h % FUNC_TABLE_SIZE;
}

static bool func_entry_matches(const func_entry_t *entry, string_view_t kw)
{
    return string_view_equals_literal(kw, entry->kw);
}

static const func_entry_t *lookup_func(string_view_t kw)
{
    const func_entry_t *entry;
    unsigned bucket;
    unsigned slot;

    if (string_view_is_empty(kw))
        return NULL;

    bucket = func_bucket_hash(kw);
    slot = (func_slot_hash(kw) + s_func_displacements[bucket]) % FUNC_TABLE_SIZE;
    entry = &s_funcs[slot];

    if (func_entry_matches(entry, kw))
        return entry;

    return NULL;
}

/* ------------------------------------------------------------------ */
/* Binary function argument parser helper                               */
/* ------------------------------------------------------------------ */

static int parse_two_args(expr_parse_state_t *p, expr_t **a_out, expr_t **b_out)
{
    expr_t *a = parse_addexpr(p);
    if (!a) return 0;

    if (!expr_parse_consume_char(p, ',')) {
        expr_free(a);
        set_error(p, "expected ',' in binary function");
        return 0;
    }
    expr_parse_skip_spaces(p);

    expr_t *b = parse_addexpr(p);
    if (!b) { expr_free(a); return 0; }

    *a_out = a;
    *b_out = b;
    return 1;
}

static int parse_three_args(expr_parse_state_t *p,
                            expr_t **a_out,
                            expr_t **b_out,
                            expr_t **c_out)
{
    expr_t *a = parse_addexpr(p);
    expr_t *b;
    expr_t *c;

    if (!a)
        return 0;
    if (!expr_parse_consume_char(p, ',')) {
        expr_free(a);
        set_error(p, "expected ',' in ternary function");
        return 0;
    }
    expr_parse_skip_spaces(p);

    b = parse_addexpr(p);
    if (!b) {
        expr_free(a);
        return 0;
    }
    if (!expr_parse_consume_char(p, ',')) {
        expr_free(b);
        expr_free(a);
        set_error(p, "expected ',' in ternary function");
        return 0;
    }
    expr_parse_skip_spaces(p);

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

static size_t scan_unicode_fraction_len_view(string_view_t view, size_t pos)
{
    return expr_parse_scan_unicode_fraction_len(view, pos);
}

static size_t scan_special_number_literal_len_view(string_view_t view, size_t pos)
{
    return expr_parse_scan_special_number_len(view, pos, false, true);
}

static size_t scan_number_atom_len_view(string_view_t view, size_t pos)
{
    return expr_parse_scan_number_atom_len(view, pos, false);
}

static int parse_number_view(string_view_t text, number_t *out)
{
    size_t len;
    size_t atom_len;
    char *roundtrip;
    string_t *literal;

    text = string_view_trim(text);

    if (string_view_is_empty(text))
        return 0;

    len = string_view_length(text);
    atom_len = scan_number_atom_len_view(text, 0u);
    if (atom_len == 0u)
        atom_len = scan_special_number_literal_len_view(text, 0u);
    if (atom_len != len)
        return 0;

    literal = string_from_view(&text);
    if (!literal)
        return 0;

    *out = num_create_from_string(string_c_str(literal));
    string_free(literal);

    roundtrip = num_to_string(*out);
    if (!roundtrip) {
        num_destroy(out);
        return 0;
    }

    free(roundtrip);
    return 1;
}

static string_t *read_any_name_cursor(string_cursor_t *cursor)
{
    return expr_parse_read_name(cursor, false);
}

static string_view_t scan_binding_value(string_cursor_t *cursor)
{
    size_t start_pos = string_cursor_position(cursor);
    int depth = 0;
    unsigned char c;

    while (!string_cursor_done(cursor)) {
        if (string_cursor_peek_ascii(cursor, &c)) {
            if (c == '(' || c == '[') {
                depth++;
                string_cursor_skip(cursor, 1u);
                continue;
            }
            if ((c == ')' || c == ']') && depth > 0) {
                depth--;
                string_cursor_skip(cursor, 1u);
                continue;
            }
            if (depth == 0 && (c == ',' || c == ';'))
                break;
            string_cursor_skip(cursor, 1u);
            continue;
        }

        if (string_cursor_next(cursor) != 0)
            break;
    }

    return string_cursor_view_extract(start_pos, cursor);
}

static int read_superscript_cursor(string_cursor_t *cursor)
{
    return expr_parse_read_superscript_int(cursor);
}

static int read_optional_display_exponent(expr_parse_state_t *p)
{
    string_cursor_t *scan = string_cursor_clone(p->cursor);
    int exponent;
    unsigned char c;

    if (!scan)
        return -1;

    exponent = read_superscript_cursor(scan);
    if (exponent >= 0) {
        string_cursor_seek(p->cursor, string_cursor_position(scan));
        string_cursor_free(scan);
        return exponent;
    }

    if (expr_parse_cursor_consume_char(scan, '^') &&
        string_cursor_peek_ascii(scan, &c) && isdigit(c)) {
        exponent = 0;
        while (string_cursor_peek_ascii(scan, &c) && isdigit(c)) {
            exponent = exponent * 10 + (int)(c - '0');
            string_cursor_skip(scan, 1u);
        }
        string_cursor_seek(p->cursor, string_cursor_position(scan));
    }

    string_cursor_free(scan);
    return exponent;
}

static size_t scan_function_power_marker_pos_view(string_view_t text,
                                                  size_t pos)
{
    size_t after = pos;
    uint32_t c = 0;
    size_t len = 0u;
    unsigned char b0;
    unsigned char b1;
    unsigned char b2;

    while (expr_parse_view_peek_value(text, after, &c, &len) &&
           expr_parse_is_superscript_digit(c))
        after += len;

    if (expr_parse_view_peek_value(text, after, &c, &len) && c == 0x207B) {
        uint32_t next = 0;
        size_t next_len = 0u;

        if (expr_parse_view_peek_value(text, after + len, &next, &next_len) &&
            next == 0x00B9)
            return after + len + next_len;
    }

    if (expr_parse_view_peek_ascii(text, after, &b0) &&
        expr_parse_view_peek_ascii(text, after + 1u, &b1) &&
        b0 == '^' && isdigit(b1)) {
        after++;
        while (expr_parse_view_peek_ascii(text, after, &b0) && isdigit(b0))
            after++;
        return after;
    }

    if (expr_parse_view_peek_ascii(text, after, &b0) &&
        expr_parse_view_peek_ascii(text, after + 1u, &b1) &&
        expr_parse_view_peek_ascii(text, after + 2u, &b2) &&
        b0 == '^' && b1 == '-' && b2 == '1')
        return after + 3u;

    if (expr_parse_view_peek_ascii(text, after, &b0) && b0 == '^') {
        string_cursor_t *cursor = string_cursor_new_view(text);
        string_t *name;
        size_t end_pos;

        if (!cursor)
            return SIZE_MAX;
        if (string_cursor_seek(cursor, after + 1u) != 0) {
            string_cursor_free(cursor);
            return SIZE_MAX;
        }
        name = read_any_name_cursor(cursor);
        if (!name) {
            string_cursor_free(cursor);
            return SIZE_MAX;
        }
        string_free(name);
        end_pos = string_cursor_position(cursor);
        string_cursor_free(cursor);
        return end_pos;
    }

    return after;
}

static int func_call_start_view(string_view_t text,
                                size_t pos,
                                const char *kw,
                                size_t klen,
                                size_t *paren_pos_out)
{
    size_t after;
    unsigned char c;

    if (!string_view_equals_literal(string_view_slice(text, pos, klen), kw))
        return 0;

    after = scan_function_power_marker_pos_view(text, pos + klen);
    if (after == SIZE_MAX)
        return 0;
    if (!expr_parse_view_peek_ascii(text, after, &c) || c != '(')
        return 0;

    if (paren_pos_out)
        *paren_pos_out = after;
    return 1;
}

static size_t scan_ascii_identifier_len_view(string_view_t text, size_t pos)
{
    size_t end = pos;
    unsigned char c;

    while (expr_parse_view_peek_ascii(text, end, &c) &&
           (isalpha(c) || isdigit(c) || c == '_'))
        end++;

    return end - pos;
}

static const func_entry_t *lookup_fixed_func_call_view(string_view_t text,
                                                       size_t pos,
                                                       size_t *paren_pos_out)
{
    size_t id_len = scan_ascii_identifier_len_view(text, pos);

    if (paren_pos_out)
        *paren_pos_out = SIZE_MAX;

    if (id_len > 0u) {
        const func_entry_t *entry =
            lookup_func(string_view_slice(text, pos, id_len));

        if (entry) {
            size_t paren_pos = SIZE_MAX;

            if (func_call_start_view(text, pos, entry->kw, entry->klen, &paren_pos)) {
                if (paren_pos_out)
                    *paren_pos_out = paren_pos;
                return entry;
            }
        }
    }

    for (size_t i = 0u; i < FUNC_TABLE_SIZE; ++i) {
        const func_entry_t *entry = &s_funcs[i];
        size_t paren_pos = SIZE_MAX;

        if (!entry->kw)
            continue;
        if (!func_call_start_view(text, pos, entry->kw, entry->klen, &paren_pos))
            continue;

        if (paren_pos_out)
            *paren_pos_out = paren_pos;
        return entry;
    }

    return NULL;
}

static int scan_polygamma_symbol_call_view(string_view_t text,
                                           size_t pos,
                                           unsigned int *order_out,
                                           size_t *paren_pos_out)
{
    size_t p = pos;
    uint32_t c = 0;
    size_t len = 0u;
    unsigned long order = 0ul;
    int digits = 0;
    unsigned char b = 0u;

    if (order_out)
        *order_out = 0u;
    if (paren_pos_out)
        *paren_pos_out = SIZE_MAX;

    if (!expr_parse_view_peek_value(text, p, &c, &len) || c != 0x03C8)
        return 0;
    p += len;

    if (!expr_parse_view_peek_value(text, p, &c, &len) || c != 0x207D)
        return 0;
    p += len;

    while (expr_parse_view_peek_value(text, p, &c, &len) &&
           expr_parse_is_superscript_digit(c)) {
        int digit = expr_parse_superscript_digit_value(c);

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

    if (!expr_parse_view_peek_value(text, p, &c, &len) || c != 0x207E)
        return 0;
    p += len;

    if (!expr_parse_view_peek_ascii(text, p, &b) || b != '(')
        return 0;

    if (order_out)
        *order_out = (unsigned int)order;
    if (paren_pos_out)
        *paren_pos_out = p;
    return 1;
}

static bool function_power_marker_is_inverse(string_view_t marker)
{
    uint32_t c = 0;
    size_t len = 0u;
    uint32_t one = 0;
    size_t one_len = 0u;
    unsigned char b0;
    unsigned char b1;
    unsigned char b2;

    if (string_view_is_empty(marker))
        return false;
    if (string_view_length(marker) == 3u &&
        expr_parse_view_peek_ascii(marker, 0u, &b0) &&
        expr_parse_view_peek_ascii(marker, 1u, &b1) &&
        expr_parse_view_peek_ascii(marker, 2u, &b2) &&
        b0 == '^' && b1 == '-' && b2 == '1')
        return true;

    if (!expr_parse_view_peek_value(marker, 0u, &c, &len))
        return false;
    if (len > 0 && c == 0x207B) {
        if (expr_parse_view_peek_value(marker, len, &one, &one_len) &&
            one == 0x00B9 && len + one_len == string_view_length(marker))
            return true;
    }

    return false;
}

static bool function_supports_inverse_power_notation(const func_entry_t *fe)
{
    static const bool inverse_power_supported[EXPR_KIND_COUNT] = {
        [EXPR_KIND_SIN] = true,
        [EXPR_KIND_COS] = true,
        [EXPR_KIND_TAN] = true,
        [EXPR_KIND_SEC] = true,
        [EXPR_KIND_COSEC] = true,
        [EXPR_KIND_COT] = true,
        [EXPR_KIND_SINH] = true,
        [EXPR_KIND_COSH] = true,
        [EXPR_KIND_TANH] = true,
        [EXPR_KIND_SECH] = true,
        [EXPR_KIND_COSECH] = true,
        [EXPR_KIND_COTH] = true
    };

    if (!fe || !fe->ops)
        return false;
    if ((unsigned)fe->ops->kind >= (unsigned)EXPR_KIND_COUNT)
        return false;
    return inverse_power_supported[fe->ops->kind];
}

static int parse_required_char(expr_parse_state_t *p, char expected, const char *errmsg)
{
    if (!expr_parse_consume_char(p, (unsigned char)expected)) {
        set_error(p, errmsg);
        return 0;
    }
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
    if (expr->kind != EXPR_BINDING_EXPR_NUMBER &&
        expr_binding_expr_is_numeric_literal(expr) &&
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

static expr_t *parse_enclosed_addexpr(expr_parse_state_t *p,
                                      char closing,
                                      const char *errmsg)
{
    expr_t *inner;

    expr_parse_skip_spaces(p);
    inner = parse_addexpr(p);

    if (!inner)
        return NULL;
    expr_parse_skip_spaces(p);
    if (!parse_required_char(p, closing, errmsg)) {
        expr_free(inner);
        return NULL;
    }
    return inner;
}

/* ------------------------------------------------------------------ */
/* Atom parser                                                          */
/* ------------------------------------------------------------------ */

static expr_t *parse_atom(expr_parse_state_t *p)
{
    NUM_SCOPE(scope);
    size_t pos = expr_parse_pos(p);
    string_view_t text = expr_parse_text(p);
    uint32_t cp = 0;
    size_t cp_len = 0u;
    unsigned char b = 0u;

    if (p->error || expr_parse_at_end(p)) {
        set_error(p, "unexpected end of expression");
        return NULL;
    }
    expr_parse_peek_ascii(p, &b);
    expr_parse_peek_value(p, &cp, &cp_len);

    /* Parenthesised sub-expression */
    if (expr_parse_consume_char(p, '(')) {
        return parse_enclosed_addexpr(p, ')', "expected ')'");
    }

    /* Absolute-value bars: |expr| */
    if (expr_parse_consume_char(p, '|')) {
        expr_t *inner = parse_enclosed_addexpr(p, '|', "expected '|'");
        if (!inner)
            return NULL;
        return apply_unary_preserving_constexpr(&ops_abs, inner, expr_abs);
    }

    /* Mathematical floor/ceiling brackets: ⌊expr⌋ and ⌈expr⌉ */
    if (cp_len > 0 && (cp == 0x230A || cp == 0x2308)) {
        const uint32_t closing = (cp == 0x230A) ? 0x230B : 0x2309;
        const char *errmsg = (cp == 0x230A) ? "expected '⌋'" : "expected '⌉'";
        expr_t *inner;
        expr_t *result;
        uint32_t close_cp = 0;
        size_t close_len = 0u;

        expr_parse_skip(p, cp_len);
        inner = parse_addexpr(p);
        if (!inner)
            return NULL;
        expr_parse_skip_spaces(p);

        if (!expr_parse_peek_value(p, &close_cp, &close_len) ||
            close_cp != closing) {
            expr_free(inner);
            set_error(p, errmsg);
            return NULL;
        }
        expr_parse_skip(p, close_len);

        result = (cp == 0x230A)
            ? apply_unary_preserving_constexpr(&ops_floor, inner, expr_floor)
            : apply_unary_preserving_constexpr(&ops_ceil, inner, expr_ceil);
        return result;
    }

    /* Numeric atom (integer/decimal/rational, optionally with trailing i) */
    if (isdigit(b) || b == '.' ||
        scan_unicode_fraction_len_view(text, pos) > 0u ||
        scan_special_number_literal_len_view(text, pos) > 0u) {
        size_t len = scan_number_atom_len_view(text, pos);
        size_t special_len = scan_special_number_literal_len_view(text, pos);
        string_view_t literal_view;
        number_t value;
        expr_t *node;
        char *literal_text;

        if (len == 0u)
            len = special_len;
        literal_view = string_view_slice(text, pos, len);
        if (len == 0 ||
            !parse_number_view(literal_view, &value)) {
            set_error(p, "expected numeric literal");
            return NULL;
        }
        expr_parse_skip(p, len);
        node = expr_new_const(value);
        if (special_len > 0u &&
            expr_parse_view_starts_with_text(string_view_slice(text, pos, special_len),
                                       "nan",
                                       true)) {
            literal_text = (char *)fs_xmalloc(4u);
            memcpy(literal_text, "NAN", 4u);
        } else if (num_is_exact(value)) {
            literal_text = num_to_string(value);
            if (!literal_text) {
                literal_text = (char *)fs_xmalloc(4u);
                memcpy(literal_text, "NAN", 4u);
            }
        } else {
            string_t *literal = string_from_view(&literal_view);

            if (!literal) {
                expr_free(node);
                num_destroy(&value);
                set_error(p, "could not preserve numeric literal");
                return NULL;
            }
            literal_text = strdup(string_c_str(literal));
            string_free(literal);
            if (!literal_text) {
                expr_free(node);
                num_destroy(&value);
                set_error(p, "out of memory");
                return NULL;
            }
        }
        node->binding_expr = expr_binding_expr_new_number_text(literal_text);
        free(literal_text);
        return node;
    }

    if (cp_len > 0 && cp == 0x221A) {
        int sup;

        expr_parse_skip(p, cp_len);
        sup = read_optional_display_exponent(p);

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
        size_t paren_pos = SIZE_MAX;
        const func_entry_t *fe =
            lookup_fixed_func_call_view(text, pos, &paren_pos);

        if (fe && paren_pos != SIZE_MAX) {
            size_t after_kw_pos = pos + fe->klen;
            expr_parse_state_t marker = *p;
            string_view_t symbolic_exp_text = string_view_empty();
            bool inverse_power = false;
            int sup;
            expr_t *symbolic_exponent = NULL;
            unsigned char marker_byte = 0u;

            expr_parse_set_pos(&marker, after_kw_pos);
            sup = read_optional_display_exponent(&marker);
            after_kw_pos = expr_parse_pos(&marker);

            if (sup < 0 && after_kw_pos < paren_pos) {
                if (function_power_marker_is_inverse(
                        string_view_slice(text,
                                          after_kw_pos,
                                          paren_pos - after_kw_pos))) {
                    inverse_power = true;
                } else if (expr_parse_view_peek_ascii(text,
                                                      after_kw_pos,
                                                      &marker_byte) &&
                           marker_byte == '^') {
                    symbolic_exp_text = string_view_slice(
                        text,
                        after_kw_pos + 1u,
                        paren_pos - after_kw_pos - 1u);
                }
            }

            expr_parse_set_pos(p, paren_pos + 1u);
            if (fe->arity == 2u) {
                expr_t *a = NULL;
                expr_t *b = NULL;
                expr_t *result;
                number_t minus_one;

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
                if (!string_view_is_empty(symbolic_exp_text)) {
                    symbolic_exponent = parse_expression_view(symbolic_exp_text,
                                                              p->syms,
                                                              "expr_from_string", 1);
                    if (!symbolic_exponent) {
                        expr_free(result);
                        return NULL;
                    }
                    return apply_binary_preserving_constexpr(
                        &ops_pow, result, symbolic_exponent, expr_pow_xp);
                }
                if (inverse_power) {
                    minus_one = num_create_from_long(-1);
                    return apply_pow_const_preserving_constexpr(result, &minus_one);
                }
                return apply_integer_power_if_present(result, sup);
            } else if (fe->arity == 3u) {
                expr_t *a = NULL;
                expr_t *b = NULL;
                expr_t *c = NULL;
                expr_t *result;
                number_t minus_one;

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
                if (!string_view_is_empty(symbolic_exp_text)) {
                    symbolic_exponent = parse_expression_view(symbolic_exp_text,
                                                              p->syms,
                                                              "expr_from_string", 1);
                    if (!symbolic_exponent) {
                        expr_free(result);
                        return NULL;
                    }
                    return apply_binary_preserving_constexpr(
                        &ops_pow, result, symbolic_exponent, expr_pow_xp);
                }
                if (inverse_power) {
                    minus_one = num_create_from_long(-1);
                    return apply_pow_const_preserving_constexpr(result, &minus_one);
                }
                return apply_integer_power_if_present(result, sup);
            } else {
                expr_t *arg = parse_enclosed_addexpr(
                    p, ')', "expected ')' after function argument");
                expr_t *result;
                bool inverse_applied = false;
                number_t minus_one;

                if (!arg)
                    return NULL;
                if (inverse_power && !function_supports_inverse_power_notation(fe)) {
                    expr_free(arg);
                    set_error(p, "unsupported inverse-function notation");
                    return NULL;
                }
                if (inverse_power && function_supports_inverse_power_notation(fe) &&
                    fe->ops && fe->ops->inverse_unary) {
                    result = fe->ops->inverse_unary(arg);
                    expr_free(arg);
                    inverse_applied = true;
                } else if (fe->ops == &ops_factors) {
                    result = fe->ufn(arg);
                    expr_free(arg);
                } else {
                    result = apply_unary_preserving_constexpr(fe->ops, arg, fe->ufn);
                }
                if (!string_view_is_empty(symbolic_exp_text)) {
                    symbolic_exponent = parse_expression_view(symbolic_exp_text,
                                                              p->syms,
                                                              "expr_from_string", 1);
                    if (!symbolic_exponent) {
                        expr_free(result);
                        return NULL;
                    }
                    return apply_binary_preserving_constexpr(
                        &ops_pow, result, symbolic_exponent, expr_pow_xp);
                }
                if (inverse_power && !inverse_applied) {
                    minus_one = num_create_from_long(-1);
                    return apply_pow_const_preserving_constexpr(result, &minus_one);
                }
                return apply_integer_power_if_present(result, sup);
            }
        }
    }

    {
        unsigned int order = 0u;
        size_t paren_pos = SIZE_MAX;

        if (scan_polygamma_symbol_call_view(text, pos, &order, &paren_pos)) {
            expr_t *arg;
            expr_t *result;

            expr_parse_set_pos(p, paren_pos + 1u);
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
    string_t *name = read_any_name_cursor(p->cursor);
    if (!name) {
        set_error(p, "expected expression");
        return NULL;
    }

    expr_t *sym = symtab_lookup(p->syms, string_c_str(name));
    if (!sym) {
        char *normalised =
            expr_normalise_name(expr_default_constant_canonical_name(string_c_str(name)));

        if (normalised) {
            sym = symtab_lookup(p->syms, normalised);
            free(normalised);
        }
    }
    if (!sym) {
        if (!p->error) {
            p->error = 1;
            if (p->errmsg)
                string_append_format(p->errmsg,
                                     "unknown symbol '%.200s'",
                                     string_c_str(name));
        }
        string_free(name);
        return NULL;
    }
    expr_retain(sym); /* give caller an owning reference */
    string_free(name);
    return sym;
}

/* ------------------------------------------------------------------ */
/* Power parser                                                         */
/* ------------------------------------------------------------------ */

static expr_t *parse_power_operand(expr_parse_state_t *p)
{
    expr_t *base = parse_atom(p);
    if (!base) return NULL;

    while (expr_parse_consume_char(p, '!')) {
        base = apply_factorial_postfix(base);
        if (!base)
            return NULL;
    }

    return base;
}

static expr_t *parse_power(expr_parse_state_t *p)
{
    NUM_SCOPE(scope);
    if (p->error) return NULL;

    expr_t *base = parse_power_operand(p);
    if (!base) return NULL;

    for (;;) {
        expr_t *result = NULL;

        /* Unicode superscript exponent: x² */
        int sup = read_superscript_cursor(p->cursor);
        if (sup >= 0) {
            base = apply_integer_power_if_present(base, sup);
            if (!base)
                return NULL;
            continue;
        }

        /* Caret exponent: chained powers are left-associative in expr syntax:
         * a^x^2 means (a^x)^2. Use explicit parentheses for powers inside
         * the exponent: a^(x^2). */
        if (!expr_parse_consume_char(p, '^'))
            break;

        expr_t *exponent = NULL;

        expr_parse_skip_spaces(p);

        if (expr_parse_consume_char(p, '(')) {
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

static expr_t *parse_signed_power(expr_parse_state_t *p)
{
    int negate = 0;
    expr_t *inner;

    if (p->error) return NULL;

    for (;;) {
        unsigned char c = 0u;

        if (!expr_parse_peek_ascii(p, &c) || (c != '-' && c != '+'))
            break;
        if (c == '-')
            negate = !negate;
        expr_parse_skip(p, 1u);
        expr_parse_skip_spaces(p);
    }

    inner = parse_power(p);
    if (!inner) return NULL;
    return negate ? apply_unary_preserving_constexpr(&ops_neg, inner, expr_neg) : inner;
}

static expr_t *parse_signed_power_operand(expr_parse_state_t *p)
{
    int negate = 0;
    expr_t *inner;

    if (p->error) return NULL;

    for (;;) {
        unsigned char c = 0u;

        if (!expr_parse_peek_ascii(p, &c) || (c != '-' && c != '+'))
            break;
        if (c == '-')
            negate = !negate;
        expr_parse_skip(p, 1u);
        expr_parse_skip_spaces(p);
    }

    inner = parse_power_operand(p);
    if (!inner) return NULL;
    return negate ? apply_unary_preserving_constexpr(&ops_neg, inner, expr_neg) : inner;
}

/* ------------------------------------------------------------------ */
/* Multiplication / division (implicit, '*', '·', '/')                 */
/* ------------------------------------------------------------------ */

static expr_t *parse_mulexpr(expr_parse_state_t *p)
{
    if (p->error) return NULL;
    expr_t *lhs = parse_signed_power(p);
    if (!lhs) return NULL;

    for (;;) {
        if (expr_parse_at_end(p)) break;

        /* Explicit middle dot '·' */
        if (at_middle_dot(p)) {
            expr_parse_skip(p, 2u);
            expr_t *rhs = parse_signed_power(p);
            if (!rhs) { expr_free(lhs); return NULL; }
            lhs = apply_binary_preserving_constexpr(&ops_mul, lhs, rhs, expr_mul);
            continue;
        }

        /* Explicit '*' or '/' accepted with or without surrounding spaces,
         * e.g. "x*y", "x * y", "x/y", and "x / y". Peek past spaces before
         * committing so a failed probe leaves the cursor untouched. */
        {
            string_cursor_t *scan = string_cursor_clone(p->cursor);
            unsigned char op = 0u;

            if (!scan) {
                expr_free(lhs);
                return NULL;
            }
            string_cursor_skip_spaces(scan);
            if (string_cursor_peek_ascii(scan, &op) &&
                (op == '*' || op == '/')) {
                string_cursor_seek(p->cursor, string_cursor_position(scan));
                string_cursor_free(scan);
                expr_parse_skip(p, 1u);
                expr_parse_skip_spaces(p);
                expr_t *rhs = parse_signed_power(p);
                if (!rhs) { expr_free(lhs); return NULL; }
                lhs = (op == '*')
                    ? apply_binary_preserving_constexpr(&ops_mul, lhs, rhs, expr_mul)
                    : apply_binary_preserving_constexpr(&ops_div, lhs, rhs, expr_div);
                continue;
            }
            string_cursor_free(scan);
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

static expr_t *parse_addexpr(expr_parse_state_t *p)
{
    if (p->error) return NULL;
    expr_t *lhs = parse_mulexpr(p);
    if (!lhs) return NULL;

    for (;;) {
        string_cursor_t *scan = string_cursor_clone(p->cursor);
        unsigned char op = 0u;

        if (!scan) {
            expr_free(lhs);
            return NULL;
        }
        string_cursor_skip_spaces(scan);

        if (string_cursor_peek_ascii(scan, &op) &&
            (op == '+' || op == '-')) {
            string_cursor_seek(p->cursor, string_cursor_position(scan));
            string_cursor_free(scan);
            expr_parse_skip(p, 1u);
            expr_parse_skip_spaces(p);
            expr_t *rhs = parse_mulexpr(p);
            if (!rhs) { expr_free(lhs); return NULL; }
            lhs = (op == '+')
                ? apply_binary_preserving_constexpr(&ops_add, lhs, rhs, expr_add)
                : apply_binary_preserving_constexpr(&ops_sub, lhs, rhs, expr_sub);
            continue;
        }

        string_cursor_free(scan);
        break;
    }
    return lhs;
}

/* ------------------------------------------------------------------ */
/* Binding section parser                                               */
/* ------------------------------------------------------------------ */

static void binding_parse_error(string_t *errmsg,
                                const char *name,
                                string_view_t value)
{
    const size_t max_value = 160u;
    size_t value_len;
    const char *suffix;
    string_t *display;

    if (!errmsg)
        return;

    string_clear(errmsg);
    value = string_view_trim(value);

    value_len = string_view_length(value);
    suffix = value_len > max_value ? "..." : "";
    if (value_len > max_value)
        value = string_view_slice(value, 0u, max_value);

    display = string_from_view(&value);

    string_append_format(errmsg,
                         "incorrect syntax for %.80s: %s%s",
                         name ? name : "",
                         display ? string_c_str(display) : "",
                         suffix);
    string_free(display);
}

/* Parse comma/semicolon-separated "name = value" pairs from [s, end).
 * is_var: 1 → create expr_new_named_var(); 0 → create expr_new_named_const().
 * On success returns 0; on failure writes to errmsg and returns -1. */
static int parse_bindings(string_view_t text,
                          int is_var, symtab_t *syms,
                          string_t *errmsg)
{
    NUM_SCOPE(scope);
    string_cursor_t *cursor;

    if (string_view_is_empty(text))
        return 0;

    cursor = string_cursor_new_view(text);
    if (!cursor) {
        if (errmsg) {
            string_clear(errmsg);
            string_append_cstr(errmsg, "out of memory");
        }
        return -1;
    }

    while (!string_cursor_done(cursor)) {
        /* Skip whitespace and separators between entries. */
        for (;;) {
            string_cursor_skip_spaces(cursor);
            if (expr_parse_cursor_consume_char(cursor, ',') ||
                expr_parse_cursor_consume_char(cursor, ';'))
                continue;
            break;
        }
        if (string_cursor_done(cursor))
            break;

        string_t *name = read_any_name_cursor(cursor);
        if (!name) {
            if (errmsg) {
                string_clear(errmsg);
                string_append_cstr(errmsg, "expected name in binding section");
            }
            string_cursor_free(cursor);
            return -1;
        }

        string_cursor_skip_spaces(cursor);
        if (!expr_parse_cursor_consume_char(cursor, '=')) {
            string_free(name);
            if (errmsg) {
                string_clear(errmsg);
                string_append_cstr(errmsg, "expected '=' after name in binding");
            }
            string_cursor_free(cursor);
            return -1;
        }
        string_cursor_skip_spaces(cursor);

        string_view_t value = scan_binding_value(cursor);
        expr_binding_expr_t *binding_expr;
        number_t val;
        expr_t *node;

        binding_expr = expr_binding_expr_parse_view(value, errmsg);
        if (!binding_expr) {
            binding_parse_error(errmsg, string_c_str(name), value);
            string_free(name);
            string_cursor_free(cursor);
            return -1;
        }
        binding_expr = expr_binding_expr_simplify(binding_expr);
        val = expr_binding_expr_eval(binding_expr);

        node = is_var
            ? expr_new_named_var(val, string_c_str(name))
            : expr_new_named_const(val, string_c_str(name));
        num_destroy(&val);
        node->binding_expr = binding_expr;

        /* expr_new_named_* calls expr_normalise_name, which may transform the name
         * (e.g. "@pi" → "π").  Use the normalised form as the lookup key so it
         * matches what the expression text will contain after its own read_any_name. */
        const char *key = (node->name && *node->name)
            ? node->name
            : string_c_str(name);

        /* Detect name clashes — same name used twice, or once as a variable
         * and once as a named constant. */
        if (symtab_has(syms, key)) {
            if (errmsg) {
                string_clear(errmsg);
                string_append_format(errmsg,
                                     "duplicate name '%.200s' in binding section",
                                     key);
            }
            expr_free(node);
            string_free(name);
            string_cursor_free(cursor);
            return -1;
        }

        symtab_add(syms, key, node);
        string_free(name);
    }
    string_cursor_free(cursor);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Pure-constant format: { name = val }                                 */
/* ------------------------------------------------------------------ */

static expr_t *parse_pure_const(string_view_t text,
                                string_t *errmsg)
{
    NUM_SCOPE(scope);
    string_cursor_t *cursor;

    if (string_view_is_empty(text))
        return NULL;

    cursor = string_cursor_new_view(text);
    if (!cursor)
        return NULL;

    string_cursor_skip_spaces(cursor);

    string_t *name = read_any_name_cursor(cursor);

    string_cursor_skip_spaces(cursor);
    if (!expr_parse_cursor_consume_char(cursor, '=')) {
        string_free(name);
        string_cursor_free(cursor);
        if (errmsg) {
            string_clear(errmsg);
            string_append_cstr(errmsg, "expected '=' in constant format");
        }
        return NULL;
    }
    string_cursor_skip_spaces(cursor);

    expr_binding_expr_t *binding_expr =
        expr_binding_expr_parse_view(
            string_cursor_view_between(string_cursor_position(cursor),
                                       string_cursor_end_position(cursor),
                                       cursor),
            errmsg);
    if (!binding_expr) {
        string_free(name);
        string_cursor_free(cursor);
        return NULL;
    }
    binding_expr = expr_binding_expr_simplify(binding_expr);
    number_t val = expr_binding_expr_eval(binding_expr);

    if (!name) {
        expr_binding_expr_free(binding_expr);
        string_cursor_free(cursor);
        if (errmsg) {
            string_clear(errmsg);
            string_append_cstr(errmsg, "constant name is required in pure-constant format");
        }
        return NULL;
    }
    expr_t *result = expr_new_named_const(val, string_c_str(name));
    num_destroy(&val);
    result->binding_expr = binding_expr;
    string_free(name);
    string_cursor_free(cursor);
    return result;
}

static int has_top_level_equals(string_view_t text)
{
    int depth = 0;
    string_cursor_t *cursor;
    unsigned char c;

    if (string_view_is_empty(text))
        return 0;

    cursor = string_cursor_new_view(text);
    if (!cursor)
        return 0;

    while (!string_cursor_done(cursor)) {
        if (string_cursor_peek_ascii(cursor, &c)) {
            if (c == '(' || c == '[') {
                depth++;
                string_cursor_skip(cursor, 1u);
                continue;
            }
            if (c == ')' || c == ']') {
                depth--;
                string_cursor_skip(cursor, 1u);
                continue;
            }
            if (depth == 0 && c == '=') {
                string_cursor_free(cursor);
                return 1;
            }
            string_cursor_skip(cursor, 1u);
            continue;
        }

        if (string_cursor_next(cursor) != 0)
            break;
    }

    string_cursor_free(cursor);
    return 0;
}

static int collect_implicit_symbols(string_view_t text,
                                    symtab_t *syms)
{
    NUM_SCOPE(scope);
    string_cursor_t *cursor;

    if (string_view_is_empty(text))
        return 0;

    cursor = string_cursor_new_view(text);
    if (!cursor)
        return -1;

    while (!string_cursor_done(cursor)) {
        size_t pos = string_cursor_position(cursor);
        size_t special_len = scan_special_number_literal_len_view(text, pos);

        if (special_len > 0u) {
            string_cursor_skip(cursor, special_len);
            continue;
        }

        {
            size_t paren_pos = SIZE_MAX;
            const func_entry_t *fe =
                lookup_fixed_func_call_view(text, pos, &paren_pos);

            if (fe && paren_pos != SIZE_MAX) {
                string_cursor_skip(cursor, fe->klen);
                continue;
            }
        }

        string_t *name = read_any_name_cursor(cursor);
        expr_t *node;
        int is_const;
        number_t value;
        const char *canonical_name;
        char *key = NULL;

        if (!name) {
            if (string_cursor_next(cursor) != 0)
                string_cursor_skip(cursor, 1u);
            continue;
        }

        canonical_name = string_c_str(name);
        is_const = expr_is_default_constant_name(canonical_name);
        if (expr_get_default_constant_num(canonical_name, &value)) {
            canonical_name = expr_default_constant_canonical_name(canonical_name);
            node = expr_new_named_const(value, canonical_name);
            node->binding_expr =
                expr_binding_expr_parse_view(
                    string_view_all(name),
                    NULL);
        } else {
            node = is_const
                ? expr_new_named_const(NUM_NAN, string_c_str(name))
                : expr_new_named_var(NUM_NAN, string_c_str(name));
        }

        key = expr_normalise_name(canonical_name);
        if (!key)
            key = strdup(canonical_name);
        if (!key) {
            expr_free(node);
            string_free(name);
            string_cursor_free(cursor);
            return -1;
        }

        if (symtab_has(syms, key)) {
            free(key);
            expr_free(node);
            string_free(name);
            continue;
        }

        symtab_add(syms, key, node);
        free(key);
        string_free(name);
    }

    string_cursor_free(cursor);
    return 0;
}

static void symtab_discard_storage(symtab_t *t)
{
    symtab_free(t);
}

static expr_t *parse_expression_view(string_view_t text,
                                     symtab_t *syms,
                                     const char *context_label,
                                     int report_errors)
{
    expr_parse_state_t ps;
    expr_t *result;
    expr_t *out = NULL;

    text = string_view_trim(text);
    if (string_view_is_empty(text))
        return NULL;

    if (expr_parse_state_init(&ps, text, syms) != 0)
        return NULL;

    result = parse_addexpr(&ps);
    if (result && !ps.error) {
        expr_parse_skip_spaces(&ps);
        if (expr_parse_at_end(&ps)) {
            out = result;
            result = NULL;
            goto done;
        }
        expr_free(result);
        result = NULL;
        set_error(&ps, "trailing input");
    } else if (result) {
        expr_free(result);
        result = NULL;
    }

    (void)context_label;
    if (ps.error && report_errors)
        fprintf(stderr, "parse error: %s\n", string_c_str(ps.errmsg));

done:
    expr_parse_state_dispose(&ps);
    return out;
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

static string_view_t view_rtrim_ascii(string_view_t text)
{
    size_t len = string_view_length(text);
    unsigned char c;

    while (len > 0u &&
           expr_parse_view_peek_ascii(text, len - 1u, &c) &&
           isspace(c))
        len--;

    return string_view_slice(text, 0u, len);
}

static void print_syntax_error_view(string_view_t text)
{
    string_view_t body = string_view_trim(text);
    size_t len = string_view_length(body);
    string_t *display;

    if (len > 0u) {
        unsigned char c;
        if (expr_parse_view_peek_ascii(body, len - 1u, &c) && c == '}')
            body = view_rtrim_ascii(string_view_slice(body, 0u, len - 1u));
    }

    display = string_from_view(&body);
    fprintf(stderr, "syntax error: %s\n",
            display ? string_c_str(display) : "");
    string_free(display);
}

static size_t find_binding_semicolon(string_view_t text)
{
    string_cursor_t *cursor = string_cursor_new_view(text);
    unsigned char c;

    if (!cursor)
        return SIZE_MAX;

    while (!string_cursor_done(cursor)) {
        if (string_cursor_peek_ascii(cursor, &c)) {
            if (c == ';') {
                size_t pos = string_cursor_position(cursor);
                string_cursor_free(cursor);
                return pos;
            }
            string_cursor_skip(cursor, 1u);
            continue;
        }

        if (string_cursor_next(cursor) != 0)
            break;
    }

    string_cursor_free(cursor);
    return SIZE_MAX;
}

static expr_t *expr_from_string_view_impl(string_view_t source,
                                          expr_bindings_t **bindings_out)
{
    expr_bindings_t *bindings = NULL;
    string_view_t text;
    string_cursor_t *cursor;
    string_cursor_t *scan_cursor;
    size_t body_start;
    size_t pipe_pos = SIZE_MAX;
    size_t close_pos = SIZE_MAX;
    int depth = 0;
    unsigned char c;

    if (bindings_out)
        *bindings_out = NULL;
    if (string_view_is_empty(source))
        return NULL;

    text = string_view_trim(source);
    cursor = string_cursor_new_view(text);
    if (!cursor)
        return NULL;
    string_cursor_skip_spaces(cursor);

    if (!expr_parse_cursor_consume_char(cursor, '{')) {
        fprintf(stderr, "expected '{'\n");
        string_cursor_free(cursor);
        return NULL;
    }

    string_cursor_skip_spaces(cursor);
    body_start = string_cursor_position(cursor);

    /* Scan for '|' and '}', tracking bracket/paren depth so we don't mistake
     * a '|' or '}' inside a bracketed name or parenthesised expression. */
    scan_cursor = string_cursor_clone(cursor);
    if (!scan_cursor) {
        string_cursor_free(cursor);
        return NULL;
    }
    while (!string_cursor_done(scan_cursor)) {
        if (string_cursor_peek_ascii(scan_cursor, &c)) {
            if (pipe_pos != SIZE_MAX && c == '}') {
                close_pos = string_cursor_position(scan_cursor);
                break;
            }
            if (c == '(' || c == '[') {
                depth++;
                string_cursor_skip(scan_cursor, 1u);
                continue;
            }
            if (c == ')' || c == ']') {
                depth--;
                string_cursor_skip(scan_cursor, 1u);
                continue;
            }
            if (depth == 0) {
                if (c == '|') {
                    pipe_pos = string_cursor_position(scan_cursor);
                    string_cursor_skip(scan_cursor, 1u);
                    continue;
                }
                if (c == '}') {
                    close_pos = string_cursor_position(scan_cursor);
                    break;
                }
            }
            string_cursor_skip(scan_cursor, 1u);
            continue;
        }

        if (string_cursor_next(scan_cursor) != 0)
            break;
    }

    if (close_pos == SIZE_MAX) {
        if (depth != 0) {
            print_syntax_error_view(
                string_cursor_view_between(body_start,
                                           string_cursor_position(scan_cursor),
                                           scan_cursor));
            string_cursor_free(scan_cursor);
            string_cursor_free(cursor);
            return NULL;
        }
        fprintf(stderr, "expected '}'\n");
        string_cursor_free(scan_cursor);
        string_cursor_free(cursor);
        return NULL;
    }
    string_cursor_free(scan_cursor);
    string_cursor_free(cursor);

    string_t *errmsg = string_new();
    if (!errmsg)
        return NULL;

    if (pipe_pos != SIZE_MAX &&
        !has_top_level_equals(
            string_view_slice(text, pipe_pos + 1u, close_pos - pipe_pos - 1u)))
        pipe_pos = SIZE_MAX;

    /* ---- No bindings: either { expr } or legacy { name = val } ---- */
    if (pipe_pos == SIZE_MAX) {
        string_view_t content = view_rtrim_ascii(
            string_view_slice(text, body_start, close_pos - body_start));
        int content_has_top_level_equals;
        symtab_t syms;

        content_has_top_level_equals = has_top_level_equals(content);

        expr_t *result = parse_expression_view(content, NULL, "expr_from_string", 0);

        if (result) {
            if (bindings_out)
                *bindings_out = single_binding_from_node(result);
            string_free(errmsg);
            return result;
        }

        if (!content_has_top_level_equals) {
            symtab_init(&syms);
            if (collect_implicit_symbols(content, &syms) == 0 && syms.count > 0) {
                result = parse_expression_view(content,
                                               &syms, "expr_from_string", 1);
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
                string_free(errmsg);
                return result;
            }

            string_free(errmsg);
            return NULL;
        }

        string_clear(errmsg);
        result = parse_pure_const(content, errmsg);
        if (!result)
            fprintf(stderr, "%s\n", string_c_str(errmsg));
        else {
            if (bindings_out)
                *bindings_out = single_binding_from_node(result);
        }
        string_free(errmsg);
        return result;
    }

    /* ---- Expression with bindings: { expr | vars; consts } ---- */
    string_view_t expr_view = view_rtrim_ascii(
        string_view_slice(text, body_start, pipe_pos - body_start));
    string_view_t bind_view =
        string_view_slice(text, pipe_pos + 1u, close_pos - pipe_pos - 1u);
    size_t semi_pos = find_binding_semicolon(bind_view);

    symtab_t syms;
    symtab_init(&syms);

    string_view_t var_bindings = semi_pos != SIZE_MAX
        ? string_view_slice(bind_view, 0u, semi_pos)
        : bind_view;

    if (parse_bindings(var_bindings, 1, &syms, errmsg) < 0) {
        symtab_free(&syms);
        fprintf(stderr, "%s\n", string_c_str(errmsg));
        string_free(errmsg);
        return NULL;
    }
    if (semi_pos != SIZE_MAX) {
        if (parse_bindings(
                string_view_slice(bind_view,
                                  semi_pos + 1u,
                                  string_view_length(bind_view) - semi_pos - 1u),
                0, &syms, errmsg) < 0) {
            symtab_free(&syms);
            fprintf(stderr, "%s\n", string_c_str(errmsg));
            string_free(errmsg);
            return NULL;
        }
    }

    if (collect_implicit_symbols(expr_view, &syms) < 0) {
        symtab_free(&syms);
        fprintf(stderr, "out of memory\n");
        string_free(errmsg);
        return NULL;
    }

    expr_t *result = parse_expression_view(expr_view, &syms, "expr_from_string", 1);
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
    string_free(errmsg);
    return result;
}

expr_t *expr_from_text(const string_t *text, expr_bindings_t **bnd_out)
{
    if (!text) {
        if (bnd_out)
            *bnd_out = NULL;
        return NULL;
    }

    return expr_from_string_view_impl(string_view_all(text), bnd_out);
}

expr_t *expr_from_string(const char *s, expr_bindings_t **bnd_out)
{
    string_t *text;
    expr_t *result;

    if (bnd_out)
        *bnd_out = NULL;
    if (!s)
        return NULL;

    text = string_new_with(s);
    if (!text)
        return NULL;

    result = expr_from_text(text, bnd_out);
    string_free(text);
    return result;
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
    string_t *text;
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

    text = string_new_with(expr);
    if (!text)
        return NULL;

    symtab_init(&syms);
    for (size_t i = 0; i < nsymbols; ++i) {
        if (!names[i] || !symbols[i]) {
            fprintf(stderr,
                    "expr_from_expression_string: null symbol entry\n");
            symtab_free(&syms);
            string_free(text);
            return NULL;
        }
        if (symtab_has(&syms, names[i])) {
            fprintf(stderr,
                    "expr_from_expression_string: duplicate symbol '%s'\n",
                    names[i]);
            symtab_free(&syms);
            string_free(text);
            return NULL;
        }
        if (symtab_add_borrowed(&syms, names[i], symbols[i]) != 0) {
            symtab_free(&syms);
            string_free(text);
            return NULL;
        }
    }

    result = parse_expression_view(string_view_all(text),
                                   nsymbols ? &syms : NULL,
                                   "expr_from_expression_string", 1);
    symtab_free(&syms);
    string_free(text);
    result = simplify_parsed_result(result);
    return result;
}
