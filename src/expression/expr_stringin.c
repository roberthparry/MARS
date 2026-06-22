/* expr_stringin.c - construct a expr_t from an expression-style string
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
 *   @S ... dt or @S^x ... dt
 *               integral sign stand-in for an unevaluated integral
 *
 *   integral(x, f_expr, t)
 *               ASCII function-style form for an unevaluated integral with
 *               upper variable x, displayed integrand f_expr, and dummy
 *               integration variable t
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
#include "expr_stringin_internal.h"
#include "expr_stringin_scan.h"
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

static string_t *expr_node_name_as_text(const expr_t *node,
                                        const string_t *fallback)
{
    if (node && node->name && *node->name)
        return string_new_with(node->name);
    return fallback ? string_clone(fallback) : NULL;
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

static expr_t *build_ascii_integral_expr(const expr_t *upper,
                                         const expr_t *display_integrand,
                                         const expr_t *dummy);
static expr_t *build_ascii_bounded_integral_expr(const expr_t *lower,
                                                 const expr_t *upper,
                                                 const expr_t *display_integrand,
                                                 const expr_t *dummy);
static expr_t *parse_signed_power_operand(expr_parse_state_t *p);
static size_t scan_unicode_fraction_len_view(string_view_t view, size_t pos);
static size_t scan_special_number_literal_len_view(string_view_t view, size_t pos);
static size_t scan_number_atom_len_view(string_view_t view, size_t pos);
static expr_t *parse_expression_view(string_view_t text,
                                     symtab_t *syms,
                                     const char *context_label,
                                     int report_errors);
static void symtab_discard_storage(symtab_t *t);

static int symtab_clone_borrowed_except_text(symtab_t *dst,
                                             const symtab_t *src,
                                             const string_t *except_name);

#define INTEGRAL_SIGN_CODEPOINT 0x222Bu

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
        (fs_is_letter(uc) || uc == 0x230A || uc == 0x2308 ||
         uc == INTEGRAL_SIGN_CODEPOINT))
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
typedef expr_t *(*senary_fn)(const expr_t *, const expr_t *, const expr_t *,
                             const expr_t *, const expr_t *, const expr_t *);

/* ------------------------------------------------------------------ */
/* Function dispatch                                                   */
/* ------------------------------------------------------------------ */

/* Function keyword table.
 *
 * Fixed aliases live here too, so the parser has one source of truth for
 * supported spellings.  The only function-like spelling handled separately is
 * ψ⁽ⁿ⁾(...), whose order is encoded in the token itself. */
#define FUNC_TABLE_SIZE 137

typedef struct {
    const char *kw;
    unsigned    arity;
    const expr_ops_t *ops;
    unary_fn    ufn;
    binary_fn   bfn;
    ternary_fn  tfn;
    senary_fn   sfn;
} func_entry_t;

static const unsigned char s_func_displacements[FUNC_TABLE_SIZE] = {
     23,   0,   1,   5,   1,   6,  52,   0,   0,   2,
      0,   0,   0,   0,   0,  89,   0,   0,   0,   7,
      6,   0,   0,   0,  10,   0,   1,   0,   9,   0,
      0,   0,  10,   0,   0,   0,   0,   0,   0,   7,
      0,   5,   0,   0,   0,   0,   0,   0,   0,   0,
     13,   0,   0,   0,   8,   0,   3,   0,   0,   0,
      0,   0,   0,   0,   8,   0,   2,   0,   4,  18,
     37,  13,   0,   0,  31,   0,   0,   0,   0,   1,
      0,   8,   0,   0,   0,   1,   0,  87,   0,   1,
      0,   1,   0,   0,   0,   3,   0,   0,   0,  15,
      0,   5, 124,   1,   4,   0,   0,   0,   0,   0,
      0,   1,   0,   0,   9,   0,   8,   0,  26,   0,
      0,   3,   0,   0,   0,   0,  12,   2,   0,   0,
      0,   7,   5,   0,   6,   9,   6
};

static const func_entry_t s_funcs[FUNC_TABLE_SIZE] = {
    [  0] = { .kw = "acoth",                .arity = 1u, .ops = &ops_acoth,           .ufn = expr_acoth },
    [  1] = { .kw = "appell_f1",            .arity = 6u,                              .sfn = expr_appell_f1 },
    [  2] = { .kw = "acosech",              .arity = 1u, .ops = &ops_acosech,         .ufn = expr_acosech },
    [  3] = { .kw = "cos",                  .arity = 1u, .ops = &ops_cos,             .ufn = expr_cos },
    [  4] = { .kw = "vercos",               .arity = 1u, .ops = &ops_vercos,          .ufn = expr_vercos },
    [  5] = { .kw = "legendre_chi",         .arity = 2u, .ops = &ops_legendre_chi,    .bfn = expr_legendre_chi_xp },
    [  6] = { .kw = "arccsc",               .arity = 1u, .ops = &ops_acosec,          .ufn = expr_acosec },
    [  7] = { .kw = "arccoversin",          .arity = 1u, .ops = &ops_arccoversin,     .ufn = expr_arccoversin },
    [  8] = { .kw = "productlog",           .arity = 1u, .ops = &ops_lambert_w,       .ufn = expr_lambert_w },
    [  9] = { .kw = "gamma",                .arity = 1u, .ops = &ops_gamma,           .ufn = expr_gamma },
    [ 10] = { .kw = "pdf",                  .arity = 1u, .ops = &ops_pdf,             .ufn = expr_pdf },
    [ 11] = { .kw = "normal_cdf",           .arity = 1u, .ops = &ops_normal_cdf,      .ufn = expr_normal_cdf },
    [ 12] = { .kw = "binomial",             .arity = 2u, .ops = NULL,                 .bfn = expr_binomial },
    [ 13] = { .kw = "SHR",                  .arity = 2u, .ops = &ops_shr,             .bfn = expr_shr },
    [ 14] = { .kw = "logbeta",              .arity = 2u, .ops = &ops_logbeta,         .bfn = expr_logbeta },
    [ 15] = { .kw = "acos",                 .arity = 1u, .ops = &ops_acos,            .ufn = expr_acos },
    [ 16] = { .kw = "polygamma",            .arity = 2u, .ops = &ops_polygamma,       .bfn = expr_polygamma_xp },
    [ 17] = { .kw = "csch",                 .arity = 1u, .ops = &ops_cosech,          .ufn = expr_cosech },
    [ 18] = { .kw = "Li2",                  .arity = 1u, .ops = &ops_dilog,           .ufn = expr_dilog },
    [ 19] = { .kw = "gcd",                  .arity = 2u, .ops = &ops_gcd,             .bfn = expr_gcd },
    [ 20] = { .kw = "floor",                .arity = 1u, .ops = &ops_floor,           .ufn = expr_floor },
    [ 21] = { .kw = "pow",                  .arity = 2u, .ops = &ops_pow,             .bfn = expr_pow_xp },
    [ 22] = { .kw = "lambert_wm1",          .arity = 1u, .ops = &ops_lambert_wm1,     .ufn = expr_lambert_wm1 },
    [ 23] = { .kw = "erfinv",               .arity = 1u, .ops = &ops_erfinv,          .ufn = expr_erfinv },
    [ 24] = { .kw = "arcosech",             .arity = 1u, .ops = &ops_acosech,         .ufn = expr_acosech },
    [ 25] = { .kw = "Ei",                   .arity = 1u, .ops = &ops_ei,              .ufn = expr_ei },
    [ 26] = { .kw = "prev_prime",           .arity = 1u, .ops = &ops_prev_prime,      .ufn = expr_prev_prime },
    [ 27] = { .kw = "covercos",             .arity = 1u, .ops = &ops_covercos,        .ufn = expr_covercos },
    [ 28] = { .kw = "sin",                  .arity = 1u, .ops = &ops_sin,             .ufn = expr_sin },
    [ 29] = { .kw = "atan",                 .arity = 1u, .ops = &ops_atan,            .ufn = expr_atan },
    [ 30] = { .kw = "sech",                 .arity = 1u, .ops = &ops_sech,            .ufn = expr_sech },
    [ 31] = { .kw = "erfc",                 .arity = 1u, .ops = &ops_erfc,            .ufn = expr_erfc },
    [ 32] = { .kw = "asech",                .arity = 1u, .ops = &ops_asech,           .ufn = expr_asech },
    [ 33] = { .kw = "beta_pdf",             .arity = 3u,                              .tfn = expr_beta_pdf },
    [ 34] = { .kw = "normal_pdf",           .arity = 1u, .ops = &ops_normal_pdf,      .ufn = expr_normal_pdf },
    [ 35] = { .kw = "exp",                  .arity = 1u, .ops = &ops_exp,             .ufn = expr_exp },
    [ 36] = { .kw = "acsch",                .arity = 1u, .ops = &ops_acosech,         .ufn = expr_acosech },
    [ 37] = { .kw = "gammainv",             .arity = 1u, .ops = &ops_gammainv,        .ufn = expr_gammainv },
    [ 38] = { .kw = "F₁",                   .arity = 6u,                              .sfn = expr_appell_f1 },
    [ 39] = { .kw = "arccovercos",          .arity = 1u, .ops = &ops_arccovercos,     .ufn = expr_arccovercos },
    [ 40] = { .kw = "arccot",               .arity = 1u, .ops = &ops_acot,            .ufn = expr_acot },
    [ 41] = { .kw = "logpdf",               .arity = 1u, .ops = &ops_logpdf,          .ufn = expr_logpdf },
    [ 42] = { .kw = "SHL",                  .arity = 2u, .ops = &ops_shl,             .bfn = expr_shl },
    [ 43] = { .kw = "archaversin",          .arity = 1u, .ops = &ops_archaversin,     .ufn = expr_archaversin },
    [ 44] = { .kw = "erf",                  .arity = 1u, .ops = &ops_erf,             .ufn = expr_erf },
    [ 45] = { .kw = "OR",                   .arity = 2u, .ops = &ops_bit_or,          .bfn = expr_bit_or },
    [ 46] = { .kw = "E1",                   .arity = 1u, .ops = &ops_e1,              .ufn = expr_e1 },
    [ 47] = { .kw = "factorial",            .arity = 1u, .ops = &ops_factorial,       .ufn = expr_factorial },
    [ 48] = { .kw = "W₋₁",                  .arity = 1u, .ops = &ops_lambert_wm1,     .ufn = expr_lambert_wm1 },
    [ 49] = { .kw = "cdf",                  .arity = 1u, .ops = &ops_cdf,             .ufn = expr_cdf },
    [ 50] = { .kw = "hacovercos",           .arity = 1u, .ops = &ops_hacovercos,      .ufn = expr_hacovercos },
    [ 51] = { .kw = "W_n",                  .arity = 2u, .ops = &ops_lambert_wn,      .bfn = expr_lambert_wn_xp },
    [ 52] = { .kw = "F1",                   .arity = 6u,                              .sfn = expr_appell_f1 },
    [ 53] = { .kw = "acsc",                 .arity = 1u, .ops = &ops_acosec,          .ufn = expr_acosec },
    [ 54] = { .kw = "hacoversin",           .arity = 1u, .ops = &ops_hacoversin,      .ufn = expr_hacoversin },
    [ 55] = { .kw = "sinh",                 .arity = 1u, .ops = &ops_sinh,            .ufn = expr_sinh },
    [ 56] = { .kw = "havercos",             .arity = 1u, .ops = &ops_havercos,        .ufn = expr_havercos },
    [ 57] = { .kw = "digamma",              .arity = 1u, .ops = &ops_digamma,         .ufn = expr_digamma },
    [ 58] = { .kw = "isqrt",                .arity = 1u, .ops = &ops_isqrt,           .ufn = expr_isqrt },
    [ 59] = { .kw = "ψ⁽⁰⁾",                 .arity = 1u, .ops = &ops_digamma,         .ufn = expr_digamma },
    [ 60] = { .kw = "lambert_w0",           .arity = 1u, .ops = &ops_lambert_w0,      .ufn = expr_lambert_w0 },
    [ 61] = { .kw = "arccosec",             .arity = 1u, .ops = &ops_acosec,          .ufn = expr_acosec },
    [ 62] = { .kw = "NOT",                  .arity = 1u, .ops = &ops_bit_not,         .ufn = expr_bit_not },
    [ 63] = { .kw = "sqrt",                 .arity = 1u, .ops = &ops_sqrt,            .ufn = expr_sqrt },
    [ 64] = { .kw = "lambert_wn",           .arity = 2u, .ops = &ops_lambert_wn,      .bfn = expr_lambert_wn_xp },
    [ 65] = { .kw = "csc",                  .arity = 1u, .ops = &ops_cosec,           .ufn = expr_cosec },
    [ 66] = { .kw = "atan2",                .arity = 2u, .ops = &ops_atan2,           .bfn = expr_atan2 },
    [ 67] = { .kw = "Wn",                   .arity = 2u, .ops = &ops_lambert_wn,      .bfn = expr_lambert_wn_xp },
    [ 68] = { .kw = "ln",                   .arity = 1u, .ops = &ops_log,             .ufn = expr_log },
    [ 69] = { .kw = "sec",                  .arity = 1u, .ops = &ops_sec,             .ufn = expr_sec },
    [ 70] = { .kw = "Γ",                    .arity = 1u, .ops = &ops_gamma,           .ufn = expr_gamma },
    [ 71] = { .kw = "asinh",                .arity = 1u, .ops = &ops_asinh,           .ufn = expr_asinh },
    [ 72] = { .kw = "asin",                 .arity = 1u, .ops = &ops_asin,            .ufn = expr_asin },
    [ 73] = { .kw = "gammainc_Q",           .arity = 2u, .ops = &ops_gammainc_Q,      .bfn = expr_gammainc_Q },
    [ 74] = { .kw = "abs",                  .arity = 1u, .ops = &ops_abs,             .ufn = expr_abs },
    [ 75] = { .kw = "modinv",               .arity = 2u, .ops = &ops_modinv,          .bfn = expr_modinv },
    [ 76] = { .kw = "atanh",                .arity = 1u, .ops = &ops_atanh,           .ufn = expr_atanh },
    [ 77] = { .kw = "W0",                   .arity = 1u, .ops = &ops_lambert_w0,      .ufn = expr_lambert_w0 },
    [ 78] = { .kw = "Wₙ",                   .arity = 2u, .ops = &ops_lambert_wn,      .bfn = expr_lambert_wn_xp },
    [ 79] = { .kw = "cosec",                .arity = 1u, .ops = &ops_cosec,           .ufn = expr_cosec },
    [ 80] = { .kw = "acosec",               .arity = 1u, .ops = &ops_acosec,          .ufn = expr_acosec },
    [ 81] = { .kw = "factors",              .arity = 1u, .ops = &ops_factors,         .ufn = expr_factors },
    [ 82] = { .kw = "logbeta_pdf",          .arity = 3u,                              .tfn = expr_logbeta_pdf },
    [ 83] = { .kw = "normal_logpdf",        .arity = 1u, .ops = &ops_normal_logpdf,   .ufn = expr_normal_logpdf },
    [ 84] = { .kw = "W_-1",                 .arity = 1u, .ops = &ops_lambert_wm1,     .ufn = expr_lambert_wm1 },
    [ 85] = { .kw = "lcm",                  .arity = 2u, .ops = &ops_lcm,             .bfn = expr_lcm },
    [ 86] = { .kw = "tan",                  .arity = 1u, .ops = &ops_tan,             .ufn = expr_tan },
    [ 87] = { .kw = "cosh",                 .arity = 1u, .ops = &ops_cosh,            .ufn = expr_cosh },
    [ 88] = { .kw = "arcversin",            .arity = 1u, .ops = &ops_arcversin,       .ufn = expr_arcversin },
    [ 89] = { .kw = "log",                  .arity = 1u, .ops = &ops_log10,           .ufn = expr_log10 },
    [ 90] = { .kw = "mod",                  .arity = 2u, .ops = &ops_mod,             .bfn = expr_mod },
    [ 91] = { .kw = "next_prime",           .arity = 1u, .ops = &ops_next_prime,      .ufn = expr_next_prime },
    [ 92] = { .kw = "trigamma",             .arity = 1u, .ops = &ops_trigamma,        .ufn = expr_trigamma },
    [ 93] = { .kw = "W_0",                  .arity = 1u, .ops = &ops_lambert_w0,      .ufn = expr_lambert_w0 },
    [ 94] = { .kw = "hypot",                .arity = 2u, .ops = &ops_hypot,           .bfn = expr_hypot },
    [ 95] = { .kw = "acosh",                .arity = 1u, .ops = &ops_acosh,           .ufn = expr_acosh },
    [ 96] = { .kw = "partition",            .arity = 1u, .ops = &ops_partition,       .ufn = expr_partition },
    [ 97] = { .kw = "arcsec",               .arity = 1u, .ops = &ops_asec,            .ufn = expr_asec },
    [ 98] = { .kw = "arcoth",               .arity = 1u, .ops = &ops_acoth,           .ufn = expr_acoth },
    [ 99] = { .kw = "haversin",             .arity = 1u, .ops = &ops_haversin,        .ufn = expr_haversin },
    [100] = { .kw = "archacovercos",        .arity = 1u, .ops = &ops_archacovercos,   .ufn = expr_archacovercos },
    [101] = { .kw = "coversin",             .arity = 1u, .ops = &ops_coversin,        .ufn = expr_coversin },
    [102] = { .kw = "cot",                  .arity = 1u, .ops = &ops_cot,             .ufn = expr_cot },
    [103] = { .kw = "arsech",               .arity = 1u, .ops = &ops_asech,           .ufn = expr_asech },
    [104] = { .kw = "chi",                  .arity = 2u, .ops = &ops_legendre_chi,    .bfn = expr_legendre_chi_xp },
    [105] = { .kw = "lg",                   .arity = 1u, .ops = &ops_log10,           .ufn = expr_log10 },
    [107] = { .kw = "polylog",              .arity = 2u, .ops = &ops_polylog,         .bfn = expr_polylog_xp },
    [108] = { .kw = "archacoversin",        .arity = 1u, .ops = &ops_archacoversin,   .ufn = expr_archacoversin },
    [109] = { .kw = "versin",               .arity = 1u, .ops = &ops_versin,          .ufn = expr_versin },
    [110] = { .kw = "AND",                  .arity = 2u, .ops = &ops_bit_and,         .bfn = expr_bit_and },
    [111] = { .kw = "W",                    .arity = 1u, .ops = &ops_lambert_w,       .ufn = expr_lambert_w },
    [112] = { .kw = "XOR",                  .arity = 2u, .ops = &ops_bit_xor,         .bfn = expr_bit_xor },
    [113] = { .kw = "ceil",                 .arity = 1u, .ops = &ops_ceil,            .ufn = expr_ceil },
    [115] = { .kw = "gammainc_lower",       .arity = 2u, .ops = &ops_gammainc_lower,  .bfn = expr_gammainc_lower },
    [116] = { .kw = "log10",                .arity = 1u, .ops = &ops_log10,           .ufn = expr_log10 },
    [117] = { .kw = "W₀",                   .arity = 1u, .ops = &ops_lambert_w0,      .ufn = expr_lambert_w0 },
    [118] = { .kw = "coth",                 .arity = 1u, .ops = &ops_coth,            .ufn = expr_coth },
    [119] = { .kw = "tanh",                 .arity = 1u, .ops = &ops_tanh,            .ufn = expr_tanh },
    [120] = { .kw = "lgamma",               .arity = 1u, .ops = &ops_lgamma,          .ufn = expr_lgamma },
    [121] = { .kw = "ψ⁽¹⁾",                 .arity = 1u, .ops = &ops_trigamma,        .ufn = expr_trigamma },
    [122] = { .kw = "archavercos",          .arity = 1u, .ops = &ops_archavercos,     .ufn = expr_archavercos },
    [123] = { .kw = "asec",                 .arity = 1u, .ops = &ops_asec,            .ufn = expr_asec },
    [124] = { .kw = "dilog",                .arity = 1u, .ops = &ops_dilog,           .ufn = expr_dilog },
    [125] = { .kw = "fibonacci",            .arity = 1u, .ops = &ops_fibonacci,       .ufn = expr_fibonacci },
    [126] = { .kw = "beta",                 .arity = 2u, .ops = &ops_beta,            .bfn = expr_beta },
    [127] = { .kw = "arcsch",               .arity = 1u, .ops = &ops_acosech,         .ufn = expr_acosech },
    [128] = { .kw = "gammainc_upper",       .arity = 2u, .ops = &ops_gammainc_upper,  .bfn = expr_gammainc_upper },
    [129] = { .kw = "gammainc_P",           .arity = 2u, .ops = &ops_gammainc_P,      .bfn = expr_gammainc_P },
    [130] = { .kw = "F_1",                  .arity = 6u,                              .sfn = expr_appell_f1 },
    [131] = { .kw = "W-1",                  .arity = 1u, .ops = &ops_lambert_wm1,     .ufn = expr_lambert_wm1 },
    [132] = { .kw = "is_prime",             .arity = 1u, .ops = &ops_is_prime,        .ufn = expr_is_prime },
    [133] = { .kw = "erfcinv",              .arity = 1u, .ops = &ops_erfcinv,         .ufn = expr_erfcinv },
    [134] = { .kw = "arcvercos",            .arity = 1u, .ops = &ops_arcvercos,       .ufn = expr_arcvercos },
    [135] = { .kw = "acot",                 .arity = 1u, .ops = &ops_acot,            .ufn = expr_acot },
    [136] = { .kw = "cosech",               .arity = 1u, .ops = &ops_cosech,          .ufn = expr_cosech },
};

static unsigned func_bucket_hash(string_view_t kw)
{
    const size_t byte_len = string_view_length(kw);
    size_t pos = 0u;
    size_t width = 0u;
    size_t rune_count = 0u;
    uint32_t first = 0u;
    uint32_t last = 0u;
    uint32_t value = 0u;

    if (byte_len == 0u ||
        !expr_parse_view_peek_value(kw, 0u, &first, &width) ||
        width == 0u)
        return 0u;

    while (pos < byte_len) {
        if (!expr_parse_view_peek_value(kw, pos, &value, &width) ||
            width == 0u)
            break;
        last = value;
        rune_count++;
        pos += width;
    }

    return (unsigned)((8u * byte_len +
                       8u * first +
                       15u * last +
                       3u * rune_count) % FUNC_TABLE_SIZE);
}

static unsigned func_slot_hash(string_view_t kw)
{
    size_t len = string_view_length(kw);
    size_t pos = 0u;
    unsigned h = 2166136261u;

    while (pos < len) {
        uint32_t value = 0u;
        size_t width = 0u;

        if (!expr_parse_view_peek_value(kw, pos, &value, &width) || width == 0u)
            break;
        h = (h ^ value) * 16777619u;
        pos += width;
    }

    h ^= (unsigned)len;
    return h % FUNC_TABLE_SIZE;
}

static bool func_entry_matches(const func_entry_t *entry, string_view_t kw)
{
    return string_view_equals_literal(kw, entry->kw);
}

static size_t func_entry_kw_len(const func_entry_t *entry)
{
    return strlen(entry->kw);
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

static int parse_six_args(expr_parse_state_t *p,
                          expr_t **a_out,
                          expr_t **b_out,
                          expr_t **c_out,
                          expr_t **d_out,
                          expr_t **e_out,
                          expr_t **f_out)
{
    expr_t *args[6] = { NULL, NULL, NULL, NULL, NULL, NULL };

    for (size_t i = 0u; i < 6u; ++i) {
        args[i] = parse_addexpr(p);
        if (!args[i])
            goto fail;
        if (i == 5u)
            break;
        if (!expr_parse_consume_char(p, ',')) {
            set_error(p, "expected ',' in six-argument function");
            goto fail;
        }
        expr_parse_skip_spaces(p);
    }

    *a_out = args[0];
    *b_out = args[1];
    *c_out = args[2];
    *d_out = args[3];
    *e_out = args[4];
    *f_out = args[5];
    return 1;

fail:
    for (size_t i = 0u; i < 6u; ++i)
        expr_free(args[i]);
    return 0;
}

static int parse_integral_call_args(expr_parse_state_t *p,
                                    expr_t **lower_out,
                                    expr_t **upper_out,
                                    expr_t **integrand_out,
                                    expr_t **dummy_out)
{
    expr_t *a = parse_addexpr(p);
    expr_t *b = NULL;
    expr_t *c = NULL;
    expr_t *d = NULL;
    unsigned char next = 0u;

    if (!a)
        return 0;
    if (!expr_parse_consume_char(p, ',')) {
        expr_free(a);
        set_error(p, "expected ',' in integral(...)");
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
        set_error(p, "expected ',' in integral(...)");
        return 0;
    }
    expr_parse_skip_spaces(p);

    c = parse_addexpr(p);
    if (!c) {
        expr_free(b);
        expr_free(a);
        return 0;
    }

    if (!expr_parse_peek_ascii(p, &next) || next == ')') {
        *lower_out = NULL;
        *upper_out = a;
        *integrand_out = b;
        *dummy_out = c;
        return 1;
    }

    if (!expr_parse_consume_char(p, ',')) {
        expr_free(c);
        expr_free(b);
        expr_free(a);
        set_error(p, "expected ',' in integral(...)");
        return 0;
    }
    expr_parse_skip_spaces(p);

    d = parse_addexpr(p);
    if (!d) {
        expr_free(c);
        expr_free(b);
        expr_free(a);
        return 0;
    }

    *lower_out = a;
    *upper_out = b;
    *integrand_out = c;
    *dummy_out = d;
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
    string_t *roundtrip;
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

    *out = num_create_from_text(literal);
    string_free(literal);

    roundtrip = num_to_string(*out);
    if (!roundtrip) {
        num_destroy(out);
        return 0;
    }

    string_free(roundtrip);
    return 1;
}

static string_t *read_any_name_cursor(string_cursor_t *cursor)
{
    return expr_parse_read_name(cursor, false);
}

static expr_t *lookup_symbol_text_normalised(const symtab_t *syms,
                                             const string_t *name)
{
    expr_t *sym;

    if (!syms || !name)
        return NULL;

    sym = symtab_lookup_text(syms, name);
    if (!sym) {
        string_t *canonical = expr_default_constant_canonical_name_text(name);
        string_t *normalised = expr_normalise_name_text(canonical);

        if (normalised)
            sym = symtab_lookup_text(syms, normalised);
        string_free(normalised);
        string_free(canonical);
    }

    return sym;
}

static int symtab_clone_borrowed_except_text(symtab_t *dst,
                                             const symtab_t *src,
                                             const string_t *except_name)
{
    if (!dst)
        return -1;

    symtab_init(dst);
    if (!src)
        return 0;

    for (int i = 0; i < src->count; ++i) {
        if (except_name &&
            string_compare(src->entries[i].name, except_name) == 0)
            continue;
        if (symtab_add_borrowed_text(dst,
                                     src->entries[i].name,
                                     src->entries[i].node) != 0) {
            symtab_free(dst);
            return -1;
        }
    }

    return 0;
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
                                const func_entry_t *entry,
                                size_t *paren_pos_out)
{
    size_t klen;
    size_t after;
    unsigned char c;

    if (!entry || !entry->kw)
        return 0;

    klen = func_entry_kw_len(entry);

    if (!string_view_equals_literal(string_view_slice(text, pos, klen), entry->kw))
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

static bool integral_ascii_standin_starts_view(string_view_t text, size_t pos)
{
    unsigned char first = 0u;
    unsigned char second = 0u;
    unsigned char next = 0u;

    if (!expr_parse_view_peek_ascii(text, pos, &first) || first != '@' ||
        !expr_parse_view_peek_ascii(text, pos + 1u, &second) ||
        (second != 'S' && second != 's') ||
        !expr_parse_view_peek_ascii(text, pos + 2u, &next))
        return false;

    return next == '^' || next == '_' || isspace(next);
}

static const func_entry_t *lookup_special_func_call_view(string_view_t text,
                                                         size_t pos,
                                                         size_t *paren_pos_out)
{
    static const func_entry_t s_integral_entry = {
        .kw = "integral",
        .arity = 3u,
        .tfn = build_ascii_integral_expr
    };
    static const func_entry_t *const entries[] = {
        &s_integral_entry
    };
    size_t id_len = scan_ascii_identifier_len_view(text, pos);
    string_view_t ident;

    if (paren_pos_out)
        *paren_pos_out = SIZE_MAX;
    if (id_len == 0u)
        return NULL;

    ident = string_view_slice(text, pos, id_len);
    for (size_t i = 0u; i < sizeof(entries) / sizeof(entries[0]); ++i) {
        const func_entry_t *entry = entries[i];
        size_t paren_pos = SIZE_MAX;

        if (!string_view_equals_literal(ident, entry->kw))
            continue;
        if (!func_call_start_view(text, pos, entry, &paren_pos))
            return NULL;
        if (paren_pos_out)
            *paren_pos_out = paren_pos;
        return entry;
    }

    return NULL;
}

static const func_entry_t *lookup_fixed_func_call_view(string_view_t text,
                                                       size_t pos,
                                                       size_t *paren_pos_out)
{
    size_t id_len = scan_ascii_identifier_len_view(text, pos);

    {
        const func_entry_t *special =
            lookup_special_func_call_view(text, pos, paren_pos_out);

        if (special)
            return special;
    }

    if (paren_pos_out)
        *paren_pos_out = SIZE_MAX;

    if (id_len > 0u) {
        const func_entry_t *entry =
            lookup_func(string_view_slice(text, pos, id_len));

        if (entry) {
            size_t paren_pos = SIZE_MAX;

            if (func_call_start_view(text, pos, entry, &paren_pos)) {
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
        if (!func_call_start_view(text, pos, entry, &paren_pos))
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

static int scan_legendre_chi_symbol_call_view(string_view_t text,
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

    if (!expr_parse_view_peek_value(text, p, &c, &len) || c != 0x03C7)
        return 0;
    p += len;

    while (expr_parse_view_peek_value(text, p, &c, &len) &&
           expr_parse_is_subscript_digit(c)) {
        int digit = expr_parse_subscript_digit_value(c);

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
    string_t *text = num_to_string(value);
    expr_binding_expr_t *expr =
        expr_binding_expr_new_number_text(text ? string_c_str(text) : "NAN");

    string_free(text);
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
        string_t *text = num_to_string(*exponent);
        expr_binding_expr_t *rhs =
            expr_binding_expr_new_number_text(text ? string_c_str(text) : "NAN");
        expr_binding_expr_t *expr =
            expr_binding_expr_new_binary_op(&ops_pow,
                                          expr_binding_expr_clone(base->binding_expr),
                                          rhs);
        expr_t *node;

        string_free(text);
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

static int parse_integral_is_space(uint32_t cp)
{
    return (cp >= 0x09u && cp <= 0x0du) ||
           cp == 0x20u ||
           cp == 0x85u ||
           cp == 0xa0u ||
           cp == 0x1680u ||
           (cp >= 0x2000u && cp <= 0x200au) ||
           cp == 0x2028u ||
           cp == 0x2029u ||
           cp == 0x202fu ||
           cp == 0x205fu ||
           cp == 0x3000u;
}

static int parse_integral_after_differential_boundary(string_view_t text,
                                                      size_t pos)
{
    string_cursor_t *cursor = string_cursor_new_view(text);
    unsigned char c = 0u;
    uint32_t cp = 0u;
    size_t cp_len = 0u;
    int ok = 0;

    if (!cursor)
        return 0;
    if (string_cursor_seek(cursor, pos) != 0) {
        string_cursor_free(cursor);
        return 0;
    }

    string_cursor_skip_spaces(cursor);
    pos = string_cursor_position(cursor);
    if (string_cursor_done(cursor)) {
        string_cursor_free(cursor);
        return 1;
    }

    if (expr_parse_view_peek_ascii(text, pos, &c)) {
        ok = (c == ')' || c == '}' || c == ']' ||
              c == ',' || c == ';' || c == '|' ||
              c == '+' || c == '-' || c == '*' || c == '/');
    } else if (expr_parse_view_peek_value(text, pos, &cp, &cp_len)) {
        (void)cp_len;
        ok = (cp == 0x00B7u);
    }

    string_cursor_free(cursor);
    return ok;
}

static int parse_integral_try_differential_at(string_view_t text,
                                              size_t start,
                                              size_t diff_pos,
                                              size_t integrand_end,
                                              size_t *integrand_end_out,
                                              size_t *after_diff_out,
                                              string_t **dummy_name_out)
{
    string_cursor_t *marker = string_cursor_new_view(text);
    size_t marker_pos;

    if (!marker)
        return 0;
    if (string_cursor_seek(marker, diff_pos) != 0) {
        string_cursor_free(marker);
        return 0;
    }

    string_cursor_skip_spaces(marker);
    marker_pos = string_cursor_position(marker);
    if (marker_pos <= start ||
        !expr_parse_cursor_consume_char(marker, 'd')) {
        string_cursor_free(marker);
        return 0;
    }

    {
        string_t *name = read_any_name_cursor(marker);

        if (name) {
            size_t after = string_cursor_position(marker);

            if (parse_integral_after_differential_boundary(text, after)) {
                if (integrand_end_out)
                    *integrand_end_out = integrand_end;
                if (after_diff_out)
                    *after_diff_out = after;
                if (dummy_name_out)
                    *dummy_name_out = name;
                else
                    string_free(name);
                string_cursor_free(marker);
                return 1;
            }
            string_free(name);
        }
    }

    string_cursor_free(marker);
    return 0;
}

static int parse_integral_find_differential(expr_parse_state_t *p,
                                            size_t start,
                                            size_t *integrand_end_out,
                                            size_t *after_diff_out,
                                            string_t **dummy_name_out)
{
    string_view_t text = expr_parse_text(p);
    size_t len = string_view_length(text);
    size_t pos = start;
    int depth = 0;

    if (integrand_end_out)
        *integrand_end_out = SIZE_MAX;
    if (after_diff_out)
        *after_diff_out = SIZE_MAX;
    if (dummy_name_out)
        *dummy_name_out = NULL;

    while (pos < len) {
        uint32_t cp = 0u;
        size_t cp_len = 0u;
        unsigned char c = 0u;

        if (!expr_parse_view_peek_value(text, pos, &cp, &cp_len) || cp_len == 0u)
            break;

        if (expr_parse_view_peek_ascii(text, pos, &c)) {
            if (c == '(' || c == '[') {
                depth++;
                pos += cp_len;
                continue;
            }
            if (c == ')' || c == ']') {
                if (depth == 0)
                    break;
                depth--;
                pos += cp_len;
                continue;
            }
            if (depth == 0 &&
                ((parse_integral_is_space(cp) &&
                  parse_integral_try_differential_at(text,
                                                     start,
                                                     pos,
                                                     pos,
                                                     integrand_end_out,
                                                     after_diff_out,
                                                     dummy_name_out)) ||
                 ((c == '*' || c == '.') &&
                  parse_integral_try_differential_at(text,
                                                     start,
                                                     pos + cp_len,
                                                     pos,
                                                     integrand_end_out,
                                                     after_diff_out,
                                                     dummy_name_out)) ||
                 (c == 'd' &&
                  parse_integral_try_differential_at(text,
                                                     start,
                                                     pos,
                                                     pos,
                                                     integrand_end_out,
                                                     after_diff_out,
                                                     dummy_name_out)))) {
                return 1;
            }
            pos += cp_len;
            continue;
        }

        if (depth == 0 &&
            cp == 0x00B7u &&
            parse_integral_try_differential_at(text,
                                               start,
                                               pos + cp_len,
                                               pos,
                                               integrand_end_out,
                                               after_diff_out,
                                               dummy_name_out)) {
            return 1;
        }

        if (cp == 0x230Au || cp == 0x2308u)
            depth++;
        else if (cp == 0x230Bu || cp == 0x2309u) {
            if (depth == 0)
                break;
            depth--;
        }
        pos += cp_len;
    }

    return 0;
}

static expr_t *build_ascii_integral_expr(const expr_t *upper,
                                         const expr_t *display_integrand,
                                         const expr_t *dummy)
{
    return build_ascii_bounded_integral_expr(NULL, upper, display_integrand, dummy);
}

static expr_t *build_ascii_bounded_integral_expr(const expr_t *lower,
                                                 const expr_t *upper,
                                                 const expr_t *display_integrand,
                                                 const expr_t *dummy)
{
    if (!upper || !display_integrand || !expr_is_var(dummy))
        return NULL;

    return lower
        ? expr_integral_with_bounds_internal(display_integrand, lower, upper, dummy)
        : expr_integral_with_dummy_internal(display_integrand, upper, dummy);
}

static expr_t *parse_integral_bound_var_slice(symtab_t *syms, string_view_t text)
{
    string_cursor_t *cursor = string_cursor_new_view(text);
    string_t *name;
    expr_t *existing;
    expr_t *var;
    string_t *key;

    if (!cursor)
        return NULL;
    name = expr_parse_read_name(cursor, true);
    string_cursor_free(cursor);
    if (!name)
        return NULL;

    existing = lookup_symbol_text_normalised(syms, name);
    if (existing) {
        expr_retain(existing);
        string_free(name);
        return existing;
    }

    var = expr_new_named_var_text(NUM_NAN, name);
    if (!var) {
        string_free(name);
        return NULL;
    }

    key = expr_node_name_as_text(var, name);
    if (!key || symtab_add_borrowed_text(syms, key, var) != 0) {
        string_free(key);
        expr_free(var);
        string_free(name);
        return NULL;
    }

    string_free(key);
    string_free(name);
    return var;
}

static expr_t *parse_integral_bound_until_separator(expr_parse_state_t *p,
                                                    unsigned char separator)
{
    string_view_t text = expr_parse_text(p);
    size_t start = expr_parse_pos(p);
    size_t len = string_view_length(text);
    size_t pos = start;
    int depth = 0;

    while (pos < len) {
        uint32_t cp = 0u;
        size_t cp_len = 0u;
        unsigned char c = 0u;

        if (!expr_parse_view_peek_value(text, pos, &cp, &cp_len) || cp_len == 0u)
            break;

        if (expr_parse_view_peek_ascii(text, pos, &c)) {
            if (c == '(' || c == '[') {
                depth++;
                pos += cp_len;
                continue;
            }
            if (c == ')' || c == ']') {
                if (depth == 0)
                    break;
                depth--;
                pos += cp_len;
                continue;
            }
            if (depth == 0 && c == separator)
                break;
            pos += cp_len;
            continue;
        }

        if (cp == 0x230Au || cp == 0x2308u)
            depth++;
        else if (cp == 0x230Bu || cp == 0x2309u) {
            if (depth == 0)
                break;
            depth--;
        }
        pos += cp_len;
    }

    if (pos == start) {
        set_error(p, "expected expression");
        return NULL;
    }

    {
        expr_t *bound = parse_expression_view(
            string_view_slice(text, start, pos - start),
            p->syms,
            "expr_from_string",
            1);

        if (!bound)
            return NULL;
        expr_parse_set_pos(p, pos);
        return bound;
    }
}

static expr_t *parse_integral_upper_bound_then_optional_lower(expr_parse_state_t *p)
{
    string_view_t text = expr_parse_text(p);
    size_t start = expr_parse_pos(p);
    unsigned char marker = 0u;
    uint32_t cp = 0u;
    size_t cp_len = 0u;
    size_t end = start;

    if (expr_parse_peek_ascii(p, &marker) && marker == '@') {
        size_t ident_len = scan_ascii_identifier_len_view(text, start + 1u);

        if (ident_len > 0u &&
            expr_parse_view_peek_ascii(text, start + 1u + ident_len, &marker) &&
            marker == '_') {
            expr_t *upper = parse_integral_bound_var_slice(
                p->syms,
                string_view_slice(text, start, ident_len + 1u));

            if (!upper)
                return NULL;
            expr_parse_set_pos(p, start + ident_len + 1u);
            return upper;
        }
    }

    if (expr_parse_peek_value(p, &cp, &cp_len) && fs_is_letter(cp)) {
        end = start + cp_len;
        if (expr_parse_view_peek_ascii(text, end, &marker) && marker == '_') {
            expr_t *upper = parse_integral_bound_var_slice(
                p->syms,
                string_view_slice(text, start, cp_len));

            if (!upper)
                return NULL;
            expr_parse_set_pos(p, end);
            return upper;
        }
    }

    return parse_signed_power_operand(p);
}

static expr_t *parse_integral_atom(expr_parse_state_t *p)
{
    string_view_t text = expr_parse_text(p);
    uint32_t cp = 0u;
    size_t cp_len = 0u;
    size_t integrand_start;
    size_t integrand_end = SIZE_MAX;
    size_t after_diff = SIZE_MAX;
    string_t *dummy_name = NULL;
    expr_t *lower = NULL;
    expr_t *upper = NULL;
    expr_t *display_integrand = NULL;
    expr_t *dummy = NULL;
    string_t *dummy_key = NULL;
    expr_t *result = NULL;
    unsigned char marker = 0u;
    symtab_t local_syms;
    symtab_t *integrand_syms = p->syms;
    bool local_syms_ready = false;

    if (expr_parse_peek_value(p, &cp, &cp_len) &&
        cp == INTEGRAL_SIGN_CODEPOINT) {
        expr_parse_skip(p, cp_len);
    } else if (integral_ascii_standin_starts_view(text, expr_parse_pos(p))) {
        expr_parse_skip(p, 2u);
    } else {
        set_error(p, "expected integral");
        return NULL;
    }

    expr_parse_skip_spaces(p);
    if (expr_parse_peek_ascii(p, &marker) && marker == '^') {
        expr_parse_skip(p, 1u);
        expr_parse_skip_spaces(p);
        upper = parse_integral_upper_bound_then_optional_lower(p);
        if (!upper)
            return NULL;
        expr_parse_skip_spaces(p);
        if (expr_parse_peek_ascii(p, &marker) && marker == '_') {
            expr_parse_skip(p, 1u);
            expr_parse_skip_spaces(p);
            lower = parse_signed_power_operand(p);
            if (!lower) {
                expr_free(upper);
                return NULL;
            }
        }
    } else if (expr_parse_peek_ascii(p, &marker) && marker == '_') {
        expr_parse_skip(p, 1u);
        expr_parse_skip_spaces(p);
        lower = parse_integral_bound_until_separator(p, '^');
        if (!lower)
            return NULL;
        expr_parse_skip_spaces(p);
        if (!parse_required_char(p, '^', "expected '^' after integral lower bound")) {
            expr_free(lower);
            return NULL;
        }
        expr_parse_skip_spaces(p);
        upper = parse_signed_power_operand(p);
        if (!upper) {
            expr_free(lower);
            return NULL;
        }
    }

    expr_parse_skip_spaces(p);
    integrand_start = expr_parse_pos(p);
    if (!parse_integral_find_differential(p, integrand_start,
                                          &integrand_end,
                                          &after_diff,
                                          &dummy_name)) {
        expr_free(lower);
        expr_free(upper);
        set_error(p, "expected differential after integral integrand");
        return NULL;
    }

    dummy = expr_new_named_var_text(NUM_ZERO, dummy_name);
    if (!dummy) {
        expr_free(lower);
        expr_free(upper);
        string_free(dummy_name);
        set_error(p, "could not create integral dummy variable");
        return NULL;
    }
    if (!upper) {
        expr_retain(dummy);
        upper = dummy;
    }

    dummy_key = expr_node_name_as_text(dummy, dummy_name);
    if (!dummy_key ||
        symtab_clone_borrowed_except_text(&local_syms, p->syms, dummy_key) != 0 ||
        symtab_add_borrowed_text(&local_syms, dummy_key, dummy) != 0) {
        expr_free(dummy);
        string_free(dummy_key);
        expr_free(lower);
        expr_free(upper);
        string_free(dummy_name);
        set_error(p, "could not create integral parser scope");
        return NULL;
    }
    local_syms_ready = true;
    integrand_syms = &local_syms;

    display_integrand = parse_expression_view(
        string_view_slice(text,
                          integrand_start,
                          integrand_end - integrand_start),
        integrand_syms,
        "expr_from_string",
        1);
    if (!display_integrand) {
        symtab_free(&local_syms);
        expr_free(dummy);
        string_free(dummy_key);
        expr_free(lower);
        expr_free(upper);
        string_free(dummy_name);
        return NULL;
    }

    result = build_ascii_bounded_integral_expr(lower,
                                               upper,
                                               display_integrand,
                                               dummy);
    expr_free(dummy);
    expr_free(display_integrand);
    expr_free(lower);
    expr_free(upper);
    if (local_syms_ready)
        symtab_free(&local_syms);
    string_free(dummy_key);
    string_free(dummy_name);

    if (!result) {
        set_error(p, "could not construct integral expression");
        return NULL;
    }

    expr_parse_set_pos(p, after_diff);
    return result;
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

    if ((cp_len > 0 && cp == INTEGRAL_SIGN_CODEPOINT) ||
        integral_ascii_standin_starts_view(text, pos)) {
        return parse_integral_atom(p);
    }

    /* Numeric atom (integer/decimal/rational, optionally with trailing i) */
    if (isdigit(b) || b == '.' ||
        scan_unicode_fraction_len_view(text, pos) > 0u ||
        scan_special_number_literal_len_view(text, pos) > 0u) {
        size_t len = scan_number_atom_len_view(text, pos);
        size_t special_len = scan_special_number_literal_len_view(text, pos);
        size_t decimal_len = expr_parse_scan_decimal_len(text, pos);
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
        } else if (decimal_len > 0u &&
                   (len == decimal_len ||
                    (len == decimal_len + 1u &&
                     expr_parse_view_peek_ascii(text, pos + decimal_len, &b) &&
                     (b == 'i' || b == 'I')))) {
            string_t *literal = string_from_view(&literal_view);

            literal_text = literal ? strdup(string_c_str(literal)) : NULL;
            string_free(literal);
            if (!literal_text) {
                expr_free(node);
                num_destroy(&value);
                set_error(p, "could not preserve numeric literal");
                return NULL;
            }
        } else if (num_is_exact(value)) {
            string_t *exact_text = num_to_string(value);

            literal_text = exact_text ? strdup(string_c_str(exact_text)) : NULL;
            string_free(exact_text);
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
            size_t after_kw_pos = pos + func_entry_kw_len(fe);
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
            if (fe->kw && strcmp(fe->kw, "integral") == 0 &&
                fe->tfn == build_ascii_integral_expr) {
                expr_t *lower = NULL;
                expr_t *upper = NULL;
                expr_t *display_integrand = NULL;
                expr_t *dummy = NULL;
                expr_t *result;
                number_t minus_one;

                if (!parse_integral_call_args(p,
                                              &lower,
                                              &upper,
                                              &display_integrand,
                                              &dummy))
                    return NULL;
                if (!parse_required_char(p, ')', "expected ')' after integral(...)")) {
                    expr_free(dummy);
                    expr_free(display_integrand);
                    expr_free(upper);
                    expr_free(lower);
                    return NULL;
                }
                result = build_ascii_bounded_integral_expr(lower,
                                                           upper,
                                                           display_integrand,
                                                           dummy);
                expr_free(dummy);
                expr_free(display_integrand);
                expr_free(upper);
                expr_free(lower);
                if (!result) {
                    set_error(p, "could not construct integral(...)");
                    return NULL;
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
            } else if (fe->arity == 2u) {
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
                if (!result) {
                    set_error(p, "could not construct ternary function");
                    return NULL;
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
            } else if (fe->arity == 6u) {
                expr_t *a = NULL;
                expr_t *b = NULL;
                expr_t *c = NULL;
                expr_t *d = NULL;
                expr_t *e = NULL;
                expr_t *f = NULL;
                expr_t *result;
                number_t minus_one;

                if (!parse_six_args(p, &a, &b, &c, &d, &e, &f))
                    return NULL;
                if (!parse_required_char(p, ')', "expected ')' after six-argument function")) {
                    expr_free(f);
                    expr_free(e);
                    expr_free(d);
                    expr_free(c);
                    expr_free(b);
                    expr_free(a);
                    return NULL;
                }
                result = fe->sfn(a, b, c, d, e, f);
                expr_free(f);
                expr_free(e);
                expr_free(d);
                expr_free(c);
                expr_free(b);
                expr_free(a);
                if (!result) {
                    set_error(p, "could not construct six-argument function");
                    return NULL;
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

        if (scan_legendre_chi_symbol_call_view(text, pos, &order, &paren_pos)) {
            expr_t *arg;
            expr_t *result;

            expr_parse_set_pos(p, paren_pos + 1u);
            arg = parse_enclosed_addexpr(
                p, ')', "expected ')' after Legendre chi argument");
            if (!arg)
                return NULL;
            result = expr_legendre_chi(order, arg);
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

    expr_t *sym = lookup_symbol_text_normalised(p->syms, name);
    if (!sym) {
        if (!p->error) {
            p->error = 1;
            if (p->errmsg)
                string_append_format(p->errmsg,
                                     "unknown symbol '%S'",
                                     name);
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
/* Multiplication / division (implicit, '*', '.', '·', '/')            */
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

        /* Explicit '*', '.', or '/' accepted with or without surrounding
         * spaces, e.g. "x*y", "x.y", "x / y", and "x/y". Peek past spaces
         * before committing so a failed probe leaves the cursor untouched. */
        {
            string_cursor_t *scan = string_cursor_clone(p->cursor);
            unsigned char op = 0u;

            if (!scan) {
                expr_free(lhs);
                return NULL;
            }
            string_cursor_skip_spaces(scan);
            if (string_cursor_peek_ascii(scan, &op) &&
                (op == '*' || op == '.' || op == '/')) {
                string_cursor_seek(p->cursor, string_cursor_position(scan));
                string_cursor_free(scan);
                expr_parse_skip(p, 1u);
                expr_parse_skip_spaces(p);
                expr_t *rhs = parse_signed_power(p);
                if (!rhs) { expr_free(lhs); return NULL; }
                lhs = (op == '/')
                    ? apply_binary_preserving_constexpr(&ops_div, lhs, rhs, expr_div)
                    : apply_binary_preserving_constexpr(&ops_mul, lhs, rhs, expr_mul);
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
                                const string_t *name,
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
                         "incorrect syntax for %S: %S%s",
                         name,
                         display,
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
            binding_parse_error(errmsg, name, value);
            string_free(name);
            string_cursor_free(cursor);
            return -1;
        }
        binding_expr = expr_binding_expr_simplify(binding_expr);
        val = expr_binding_expr_eval(binding_expr);

        node = is_var
            ? expr_new_named_var_text(val, name)
            : expr_new_named_const_text(val, name);
        num_destroy(&val);
        if (!node) {
            expr_binding_expr_free(binding_expr);
            string_free(name);
            string_cursor_free(cursor);
            return -1;
        }
        node->binding_expr = binding_expr;

        /* expr_new_named_* calls expr_normalise_name, which may transform the name
         * (e.g. "@pi" → "π").  Use the normalised form as the lookup key so it
         * matches what the expression text will contain after its own read_any_name. */
        string_t *key = expr_node_name_as_text(node, name);

        /* Detect name clashes — same name used twice, or once as a variable
         * and once as a named constant. */
        if (!key || symtab_has_text(syms, key)) {
            if (errmsg) {
                string_clear(errmsg);
                string_append_format(errmsg,
                                     "duplicate name '%S' in binding section",
                                     key);
            }
            expr_free(node);
            string_free(key);
            string_free(name);
            string_cursor_free(cursor);
            return -1;
        }

        symtab_add_text(syms, key, node);
        string_free(key);
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
    expr_t *result = expr_new_named_const_text(val, name);
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
            unsigned int order = 0u;

            if (fe && paren_pos != SIZE_MAX) {
                string_cursor_skip(cursor, func_entry_kw_len(fe));
                continue;
            }
            if (scan_legendre_chi_symbol_call_view(text, pos, &order, &paren_pos) &&
                paren_pos != SIZE_MAX) {
                string_cursor_skip(cursor, paren_pos - pos + 1u);
                continue;
            }
        }

        string_t *name = read_any_name_cursor(cursor);
        expr_t *node;
        int is_const;
        number_t value;
        string_t *canonical_text = NULL;
        string_t *key = NULL;

        if (!name) {
            if (string_cursor_next(cursor) != 0)
                string_cursor_skip(cursor, 1u);
            continue;
        }

        is_const = expr_is_default_constant_name_text(name);
        if (expr_get_default_constant_num_text(name, &value)) {
            canonical_text = expr_default_constant_canonical_name_text(name);
            node = expr_new_named_const_text(value, canonical_text);
            node->binding_expr =
                expr_binding_expr_parse_view(
                    string_view_all(name),
                    NULL);
        } else {
            node = is_const
                ? expr_new_named_const_text(NUM_NAN, name)
                : expr_new_named_var_text(NUM_NAN, name);
            canonical_text = string_clone(name);
        }

        key = expr_normalise_name_text(canonical_text);
        if (!key)
            key = string_clone(canonical_text);
        if (!key) {
            string_free(canonical_text);
            expr_free(node);
            string_free(name);
            string_cursor_free(cursor);
            return -1;
        }

        if (symtab_has_text(syms, key)) {
            string_free(key);
            string_free(canonical_text);
            expr_free(node);
            string_free(name);
            continue;
        }

        symtab_add_text(syms, key, node);
        string_free(key);
        string_free(canonical_text);
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
                bindings = symtab_build_bindings_for_expr(&syms, result);
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
        bindings = symtab_build_bindings_for_expr(&syms, result);
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

static expr_binding_entry_t *bnd_find_entry_text(expr_bindings_t *bnd,
                                                 const string_t *name)
{
    string_t *norm;
    expr_binding_entry_t *entry = NULL;

    if (!bnd || !bnd->index || !name)
        return NULL;

    norm = expr_normalise_binding_name_text(name);
    if (!norm)
        return NULL;

    dictionary_get(bnd->index, &norm, &entry);
    string_free(norm);
    return entry;
}

expr_t *expr_bindings_get_text(expr_bindings_t *bnd, const string_t *name)
{
    expr_binding_entry_t *entry = bnd_find_entry_text(bnd, name);

    return entry ? entry->expr : NULL;
}

expr_t *expr_bindings_get(expr_bindings_t *bnd, const char *name)
{
    string_t *text;
    expr_t *expr;

    if (!name)
        return NULL;

    text = string_new_with(name);
    if (!text)
        return NULL;

    expr = expr_bindings_get_text(bnd, text);
    string_free(text);
    return expr;
}

size_t expr_bindings_count(const expr_bindings_t *bnd)
{
    return bnd ? bnd->count : 0u;
}

const char *expr_bindings_name_at(const expr_bindings_t *bnd, size_t index)
{
    if (!bnd || index >= bnd->count || !bnd->entries[index].name)
        return NULL;
    return string_c_str(bnd->entries[index].name);
}

const string_t *expr_bindings_name_text_at(const expr_bindings_t *bnd, size_t index)
{
    if (!bnd || index >= bnd->count)
        return NULL;
    return bnd->entries[index].name;
}

expr_t *expr_bindings_expr_at(expr_bindings_t *bnd, size_t index)
{
    if (!bnd || index >= bnd->count)
        return NULL;
    return bnd->entries[index].expr;
}

bool expr_bindings_is_constant_at(const expr_bindings_t *bnd, size_t index)
{
    return bnd && index < bnd->count && bnd->entries[index].is_constant;
}

void expr_bindings_free(expr_bindings_t *bnd)
{
    if (!bnd)
        return;
    for (size_t i = 0; i < bnd->count; ++i)
        string_free(bnd->entries[i].name);
    for (size_t i = 0; i < bnd->count; ++i)
        expr_free(bnd->entries[i].expr);
    dictionary_destroy(bnd->index);
    free(bnd->entries);
    free(bnd);
}

expr_t *expr_from_expression_string(const char *expr,
                                    const char *const *names,
                                    expr_t *const *symbols,
                                    size_t nsymbols)
{
    string_t *text;
    string_t **name_texts = NULL;
    expr_t *result;

    if (!expr)
        return NULL;

    text = string_new_with(expr);
    if (!text)
        return NULL;

    if (nsymbols > 0u) {
        name_texts = calloc(nsymbols, sizeof(*name_texts));
        if (!name_texts) {
            string_free(text);
            return NULL;
        }
        for (size_t i = 0; i < nsymbols; ++i) {
            if (!names || !names[i])
                continue;
            name_texts[i] = string_new_with(names[i]);
            if (!name_texts[i]) {
                for (size_t j = 0; j < i; ++j)
                    string_free(name_texts[j]);
                free(name_texts);
                string_free(text);
                return NULL;
            }
        }
    }

    result = expr_from_expression_text(text,
                                       (const string_t *const *)name_texts,
                                       symbols,
                                       nsymbols);
    if (name_texts) {
        for (size_t i = 0; i < nsymbols; ++i)
            string_free(name_texts[i]);
        free(name_texts);
    }
    string_free(text);
    return result;
}

expr_t *expr_from_expression_text(const string_t *expr,
                                  const string_t *const *names,
                                  expr_t *const *symbols,
                                  size_t nsymbols)
{
    symtab_t syms;
    expr_t *result;

    if (!expr)
        return NULL;
    if (nsymbols > 0 && (!names || !symbols)) {
        fprintf(stderr,
                "expr_from_expression_text: symbol table is incomplete\n");
        return NULL;
    }
    if (nsymbols > (size_t)INT_MAX) {
        fprintf(stderr,
                "expr_from_expression_text: too many symbols\n");
        return NULL;
    }

    symtab_init(&syms);
    for (size_t i = 0; i < nsymbols; ++i) {
        if (!names[i] || !symbols[i]) {
            fprintf(stderr,
                    "expr_from_expression_text: null symbol entry\n");
            symtab_free(&syms);
            return NULL;
        }
        if (symtab_has_text(&syms, names[i])) {
            string_fprintf(stderr,
                           "expr_from_expression_text: duplicate symbol '%S'\n",
                           names[i]);
            symtab_free(&syms);
            return NULL;
        }
        if (symtab_add_borrowed_text(&syms, names[i], symbols[i]) != 0) {
            symtab_free(&syms);
            return NULL;
        }
    }

    result = parse_expression_view(string_view_all(expr),
                                   nsymbols ? &syms : NULL,
                                   "expr_from_expression_text", 1);
    symtab_free(&syms);
    result = simplify_parsed_result(result);
    return result;
}
