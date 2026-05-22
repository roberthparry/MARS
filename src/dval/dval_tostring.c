/* dval_tostring.c - symbolic/string conversion for dval_t
 *
 * Produces human-readable string representations of a dval_t DAG via
 * dv_to_string(dv, style).  Three styles are supported:
 *
 *   style_EXPRESSION  — infix notation, e.g.
 *                         { sin(x)·cos(y) | x = 1, y = ½π }
 *                       or, when no bindings are needed:
 *                         sin(x)·cos(y)
 *                       This is the preferred round-trip format accepted by
 *                       dval_from_string().
 *
 *   style_FUNCTION    — function-like notation, e.g.
 *                         x = 1
 *                         y = π/2
 *                         expr(x,y) = sin(x)*cos(y)
 *                         return expr(x,y)
 *                       Useful for debugging graph structure and generated
 *                       callable forms.
 *
 *   style_TEX         — native TeX notation generated directly from the DAG,
 *                       e.g.
 *                         \left\{ \sin(x)\cos(y) \;\middle|\;
 *                         x = 1, y = \frac{\pi}{2} \right\}
 *
 * Responsibilities of this file:
 *   • Operator precedence and parenthesisation (infix only)
 *   • Unicode superscripts and fraction glyphs for compact powers/coefficients
 *   • The { expr | bindings } wrapper when bindings are present
 *   • Native TeX emission for expression and binding DAGs
 *
 * Algebraic simplification is deliberately not part of ordinary rendering:
 * callers see the expression shape they built or parsed.  Owning derivative
 * creation simplifies derivatives in the DAG before they are rendered.
 */

#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "dval_bindings_internal.h"
#include "dval_internal.h"
#include "dval_tostring_internal.h"
#include "dval.h"
#include "internal/number_internal.h"

/* ------------------------------------------------------------------------- */
/* Small helpers                                                             */
/* ------------------------------------------------------------------------- */

static void dv_trim_decimal_display_artifacts_local(char *text);

static char *dv_number_to_string_local(number_t value)
{
    char *text;
    size_t bits;
    size_t digits;
    char fmt[32];
    int needed;

    if ((!num_is_mfloat_backend(value) && !num_is_mcomplex_backend(value)) ||
        num_is_exact(value) || !num_is_finite(value)) {
        text = num_to_string(value);
        dv_trim_decimal_display_artifacts_local(text);
        num_destroy(&value);
        return text;
    }

    bits = num_get_prec_bits(value);
    if (bits == 0u)
        bits = num_get_effective_prec_bits(value);
    digits = bits == 0u ? 0u : (size_t)((double)bits * 0.3010299956639812);
    if (digits == 0u)
        digits = num_get_default_prec_digits();
    if (digits == 0u || digits > (size_t)INT_MAX) {
        text = num_to_string(value);
        dv_trim_decimal_display_artifacts_local(text);
        num_destroy(&value);
        return text;
    }

    snprintf(fmt, sizeof(fmt), "%%.%dn", (int)digits);
    needed = num_sprintf(NULL, 0u, fmt, value);
    if (needed < 0) {
        text = num_to_string(value);
    } else {
        text = malloc((size_t)needed + 1u);
        if (text)
            num_sprintf(text, (size_t)needed + 1u, fmt, value);
    }

    dv_trim_decimal_display_artifacts_local(text);
    num_destroy(&value);
    return text;
}

static void dv_trim_decimal_display_artifacts_local(char *text)
{
    char *p;

    if (!text)
        return;

    p = text;
    while ((p = strchr(p, '.')) != NULL) {
        char *frac = p + 1;
        char *end = frac;
        char *q;
        char *zero_start = NULL;
        size_t zero_run = 0u;
        bool seen_nonzero = false;

        while (isdigit((unsigned char)*end))
            ++end;
        if (end == frac) {
            ++p;
            continue;
        }

        for (q = frac; q < end; ++q) {
            if (*q == '0') {
                if (seen_nonzero) {
                    if (!zero_start)
                        zero_start = q;
                    ++zero_run;
                }
                if (zero_start && zero_run >= 24u) {
                    memmove(zero_start, end, strlen(end) + 1u);
                    p = zero_start;
                    break;
                }
            } else {
                seen_nonzero = true;
                zero_start = NULL;
                zero_run = 0u;
            }
        }
        if (q != end)
            continue;

        while (end > frac && end[-1] == '0')
            --end;
        if (end == frac) {
            memmove(p, q, strlen(q) + 1u);
            continue;
        }
        if (*end == '\0') {
            *end = '\0';
            p = end;
        } else {
            memmove(end, q, strlen(q) + 1u);
            p = end;
        }
    }
}

static char *dv_const_to_string_local(const dval_t *dv)
{
    return dv ? dv_number_to_string_local(num_clone(dv->c)) : NULL;
}

static char *dv_eval_to_string_local(const dval_t *dv)
{
    return dv_number_to_string_local(dv_eval(dv));
}

static bool dv_is_immortal_default_const_local(const dval_t *dv)
{
    const char *canon;
    number_t builtin;
    bool match;

    if (!dv || !dv_is_const(dv) || !dv->name || !*dv->name)
        return false;

    canon = dv_default_constant_canonical_name(dv->name);
    if (!canon)
        return false;
    if (strcmp(canon, "@tau") == 0)
        return false;
    if (!dv_get_default_constant_num(canon, &builtin))
        return false;

    match = num_eq(dv->c, builtin);
    num_destroy(&builtin);
    return match;
}

/* ------------------------------------------------------------------------- */
/* Growable string buffer                                                    */
/* ------------------------------------------------------------------------- */

#define xmalloc dv_tostring_xmalloc
#define xstrdup dv_tostring_xstrdup
#define utf8_decode dv_tostring_utf8_decode

/* ------------------------------------------------------------------------- */
/* Auto-naming for unnamed nodes                                             */
/* ------------------------------------------------------------------------- */

/* The 10 subscript digit strings (U+2080–U+2089), each 3 UTF-8 bytes.
 * Multi-digit subscripts like ₁₀ are assembled from these at call time. */
static const char subscript_digits[10][4] = {
    "\xE2\x82\x80", "\xE2\x82\x81", "\xE2\x82\x82", "\xE2\x82\x83",
    "\xE2\x82\x84", "\xE2\x82\x85", "\xE2\x82\x86", "\xE2\x82\x87",
    "\xE2\x82\x88", "\xE2\x82\x89",
};

/* Build a name of the form  prefix₀  prefix₁  prefix₁₀  …
 * The returned buffer is heap-allocated and owned by the caller. */
static char *make_subscript_name(char prefix, int idx)
{
    char digits[16];
    int nd = 0, n = idx;
    do { digits[nd++] = (char)(n % 10); n /= 10; } while (n > 0);

    char *buf = (char *)xmalloc(1 + (size_t)nd * 3 + 1);
    buf[0] = prefix;
    int pos = 1;
    for (int i = nd - 1; i >= 0; i--) {
        memcpy(buf + pos, subscript_digits[(unsigned char)digits[i]], 3);
        pos += 3;
    }
    buf[pos] = '\0';
    return buf;
}

typedef struct {
    dval_t *node;
    char   *buf;   /* allocated name, also stored in node->name during use */
} autoname_entry_t;

typedef struct {
    autoname_entry_t *entries;
    size_t            count;
    size_t            cap;
} autoname_table_t;

static void autoname_init(autoname_table_t *t)
{
    t->entries = NULL;
    t->count   = 0;
    t->cap     = 0;
}

/* Restore node->name fields to NULL and free all allocated name buffers. */
static void autoname_restore(autoname_table_t *t)
{
    for (size_t i = 0; i < t->count; i++) {
        t->entries[i].node->name = NULL;
        free(t->entries[i].buf);
    }
    free(t->entries);
    t->entries = NULL;
    t->count   = 0;
    t->cap     = 0;
}

/* DFS: find unnamed var nodes and assign x₀, x₁, … in first-visit order. */
static void assign_unnamed_vars_dfs(dval_t *f, autoname_table_t *t)
{
    if (!f) return;

    if (dv_is_var(f)) {
        if (f->name && *f->name) return;  /* already named */
        /* Check if this node was already assigned */
        for (size_t i = 0; i < t->count; i++)
            if (t->entries[i].node == f) return;
        /* Grow table if needed */
        if (t->count == t->cap) {
            t->cap = t->cap ? t->cap * 2 : 4;
            t->entries = (autoname_entry_t *)realloc(
                t->entries, t->cap * sizeof(autoname_entry_t));
            if (!t->entries) { fprintf(stderr, "auto-name: OOM\n"); abort(); }
        }
        char *buf = make_subscript_name('x', (int)t->count);
        t->entries[t->count].node = f;
        t->entries[t->count].buf  = buf;
        t->count++;
        f->name = buf;   /* temporary assignment */
        return;
    }

    if (dv_is_const(f)) return;   /* constants have no children to recurse */

    assign_unnamed_vars_dfs(f->a, t);
    assign_unnamed_vars_dfs(f->b, t);
}


/* ------------------------------------------------------------------------- */
/* Precedence and superscripts                                               */
/* ------------------------------------------------------------------------- */

typedef enum {
    PREC_LOWEST = 0,
    PREC_ADD    = 1,
    PREC_MUL    = 2,
    PREC_POW    = 3,
    PREC_UNARY  = 4,
    PREC_ATOM   = 5
} prec_t;

static const char *sup_digits[10] = {
    "⁰","¹","²","³","⁴","⁵","⁶","⁷","⁸","⁹"
};

static void emit_superscript_int(sbuf_t *b, long n)
{
    if (n < 0) {
        sbuf_puts(b, "⁻");
        n = -n;
    }
    if (n == 0) {
        sbuf_puts(b, "⁰");
        return;
    }
    char tmp[32];
    int  len = 0;
    while (n > 0 && len < (int)sizeof(tmp)) {
        tmp[len++] = (char)('0' + (n % 10));
        n /= 10;
    }
    for (int i = len - 1; i >= 0; --i) {
        int d = tmp[i] - '0';
        sbuf_puts(b, sup_digits[d]);
    }
}

static bool dv_try_get_small_integer_exponent(number_t value, long *out)
{
    char *text;
    char *end = NULL;
    long parsed;

    if (!out || !num_is_real(value) || !num_is_integer(value))
        return false;

    text = num_to_string(value);
    if (!text)
        return false;
    if (*text == '\0') {
        free(text);
        return false;
    }

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || !end || *end != '\0') {
        free(text);
        return false;
    }

    free(text);
    *out = parsed;
    return true;
}

/* ------------------------------------------------------------------------- */
/* Atom helpers                                                              */
/* ------------------------------------------------------------------------- */

static int dv_tostring_should_emit_binding_expr(const dval_t *f)
{
    number_t value;
    int is_builtin_const;
    int value_matches_builtin = 0;

    if (!f || !f->binding_expr)
        return 0;
    if (!f->name || !*f->name)
        return 1;

    is_builtin_const = dv_get_default_constant_num(f->name, &value);
    if (is_builtin_const) {
        value_matches_builtin = num_eq(f->c, value);
        num_destroy(&value);
    }
    return is_builtin_const && value_matches_builtin;
}

static void emit_atom(dval_t *f, sbuf_t *b)
{
    if (dv_is_const(f)) {
        if (dv_tostring_should_emit_binding_expr(f)) {
            char *text = dv_binding_expr_to_string(f->binding_expr);

            if (text) {
                sbuf_puts(b, text);
                free(text);
            }
        } else if (f->name && *f->name) {
            emit_name(b, f->name);
        } else {
            char *text = dv_const_to_string_local(f);
            if (text) {
                sbuf_puts(b, text);
                free(text);
            }
        }
    } else if (dv_is_var(f)) {
        emit_name(b, f->name ? f->name : "x");
    } else {
        char *text = dv_eval_to_string_local(f);
        if (text) {
            sbuf_puts(b, text);
            free(text);
        }
    }
}

static bool emit_negative_const_binding_expr_abs(const dval_t *f,
                                                 sbuf_t *b,
                                                 bool tex)
{
    char *text;
    const char *p;
    const char *q;
    size_t i;

    if (!f || !f->binding_expr)
        return false;

    text = tex
        ? dv_binding_expr_to_tex(f->binding_expr)
        : dv_binding_expr_to_string(f->binding_expr);
    if (!text)
        return false;

    p = text;
    while (*p && isspace((unsigned char)*p))
        ++p;
    if (*p != '-') {
        free(text);
        return false;
    }

    ++p;
    while (*p && isspace((unsigned char)*p))
        ++p;
    q = p;
    while (isdigit((unsigned char)*q))
        ++q;
    if (q > p) {
        if (tex && strncmp(q, " \\cdot \\pi", 10u) == 0) {
            for (i = 0u; p + i < q; ++i)
                sbuf_putc(b, p[i]);
            sbuf_puts(b, "\\pi");
            sbuf_puts(b, q + 10u);
            free(text);
            return true;
        }
        if (!tex && strncmp(q, "·π", sizeof("·π") - 1u) == 0) {
            for (i = 0u; p + i < q; ++i)
                sbuf_putc(b, p[i]);
            sbuf_puts(b, "π");
            sbuf_puts(b, q + sizeof("·π") - 1u);
            free(text);
            return true;
        }
    }
    sbuf_puts(b, p);
    free(text);
    return true;
}

/* -------------------------------------------------------------
   Helper: does a pow exponent need wrapping parens?
   Atoms (var/const) and function calls (unary/binary — they have their own
   parentheses) are self-delimiting; infix operators and neg are not.
   ------------------------------------------------------------- */
static int pow_exp_needs_parens(const dval_t *e)
{
    if (!e) return 0;
    if (e->ops->arity == DV_OP_ATOM)  return 0;  /* var, const */
    if (dv_is_neg(e))                  return 1;
    if (dv_is_pow_d_expr(e))           return 1;  /* e.g. y² is ambiguous as exponent */
    if (e->ops->arity == DV_OP_UNARY)  return 0;  /* sin(…), exp(…), etc. */
    /* DV_OP_BINARY: arithmetic/pow need parens; named functions (atan2 …) don't */
    if (dv_is_addsub(e) || dv_is_mul(e) ||
        dv_is_op(e, &ops_div) || dv_is_op(e, &ops_pow)) return 1;
    return 0;
}

static int pow_base_needs_visible_parens(const dval_t *base)
{
    return base && dv_is_const(base) && !num_is_real(base->c);
}

/* -------------------------------------------------------------
   Helper: atomic factors for implicit multiplication (EXPR mode)
   ------------------------------------------------------------- */
static int is_atomic_for_mul(const dval_t *f)
{
    if (!f) return 0;

    if (dv_is_const(f)) {
        /* Unnamed numeric constants are always atomic (e.g. the leading "6" in 6x²).
         * Named constants are atomic only when their name is "simple" (single letter
         * or letter + subscript digits).  Multi-char names like "pi" or "radius"
         * are non-atomic so that a middle-dot separator is inserted between adjacent
         * bracketed terms: [pi]·[radius]² instead of [pi][radius]². */
        if (!f->name || !*f->name) return 1;
        return dv_tostring_is_simple_name(f->name);
    }

    if (dv_is_var(f))
        return dv_tostring_is_simple_name(f->name);

    if (dv_tostring_is_var_pow_d(f))
        return dv_tostring_is_simple_name(f->a->name);

    return 0;
}

/* -------------------------------------------------------------
   Factor classification / flattening / ordering
   ------------------------------------------------------------- */

static void flatten_mul(dval_t *f, dval_t **buf, int *count, int max)
{
    if (!f || *count >= max) return;

    if (dv_is_mul(f)) {
        flatten_mul(f->a, buf, count, max);
        flatten_mul(f->b, buf, count, max);
    } else {
        buf[(*count)++] = f;
    }
}

/* Sort group for multiplication factors:
 *   0 = unnamed numeric constant       (e.g. 6)
 *   1 = Greek immortal named constant  (e.g. π)
 *   2 = Latin/other immortal constant  (e.g. e)
 *   3 = variable or var^n              (e.g. x, x³)
 *   4 = explicit bound named constant  (e.g. H or x from the const bindings)
 *   5 = everything else (unary/binary fns) — sort by primary arg var name,
 *       stable so same-arg functions keep their original tree order
 */
static int factor_group(const dval_t *f)
{
    if (dv_is_neg(f)) f = f->a;

    if (dv_is_const(f)) {
        if (!f->name || !*f->name) return 0;
        if (f->binding_expr && !dv_is_immortal_default_const_local(f))
            return 4;
        /* Greek letters are UTF-8 multi-byte; first byte >= 0x80 */
        return ((unsigned char)f->name[0] >= 0x80) ? 1 : 2;
    }

    if (dv_is_var(f))
        return 3;

    if (dv_tostring_is_var_pow_d(f))
        return 3;

    return 5;
}

/* DFS to find the name of the first variable in an expression. */
static const char *first_var_name(const dval_t *f)
{
    if (!f) return "";
    if (dv_is_var(f)) return f->name ? f->name : "";
    const char *a = first_var_name(f->a);
    if (*a) return a;
    return first_var_name(f->b);
}

/* Counts levels of function *nesting* (not tree depth).
 * pow_d and neg are transparent — cos²(x) has the same nesting depth as cos(x).
 * This makes cos²(x) (depth 1) sort before exp(sin(x)) (depth 2). */
static int factor_depth(const dval_t *f)
{
    if (!f || dv_is_const(f) || dv_is_var(f)) return 0;
    if (dv_is_neg(f) || dv_is_pow_d_expr(f)) return factor_depth(f->a);
    if (f->ops->arity == DV_OP_UNARY) return 1 + factor_depth(f->a);
    if (f->ops->arity == DV_OP_BINARY) {
        int da = factor_depth(f->a), db = factor_depth(f->b);
        return 1 + (da > db ? da : db);
    }
    return 0;
}

static const char *factor_sort_name(const dval_t *f)
{
    if (dv_is_neg(f)) f = f->a;

    if (dv_is_const(f))
        return (f->name && *f->name) ? f->name : "";

    if (dv_is_var(f))
        return f->name ? f->name : "";

    if (dv_tostring_is_var_pow_d(f))
        return f->a->name ? f->a->name : "";

    /* Unary/binary functions: sort by the primary variable in the argument
     * so e.g. sin(x) and cos(y) sort by x vs y, not by function name.
     * Functions with the same primary variable keep their original order
     * (handled by the stable sort below). */
    return first_var_name(f->a);
}

/* Stable insertion sort for factor arrays.
 * Within group 4 (functions), sort shallower expressions first so that
 * e.g. cos(x) (depth 2) appears before exp(sin(x)) (depth 3). */
static void sort_factors(dval_t **fac, int n)
{
    for (int s = 1; s < n; s++) {
        dval_t *key = fac[s];
        int kg = factor_group(key);
        const char *kn = factor_sort_name(key);
        int kd = (kg == 5) ? factor_depth(key) : 0;
        int t = s - 1;
        while (t >= 0) {
            int tg = factor_group(fac[t]);
            int cmp;
            if (tg != kg) {
                cmp = tg - kg;
            } else if (kg == 5) {
                int td = factor_depth(fac[t]);
                cmp = (td != kd) ? (td - kd) : strcmp(factor_sort_name(fac[t]), kn);
            } else {
                cmp = strcmp(factor_sort_name(fac[t]), kn);
            }
            if (cmp <= 0) break;
            fac[t + 1] = fac[t];
            t--;
        }
        fac[t + 1] = key;
    }
}

/* ------------------------------------------------------------------------- */
/* EXPRESSION MODE (pretty math)                                             */
/* ------------------------------------------------------------------------- */

static void emit_expr(const dval_t *f, sbuf_t *b, int parent_prec);
static void emit_expr_abs(const dval_t *f, sbuf_t *b, int parent_prec);
static void emit_expr_abs_bars(const dval_t *f, sbuf_t *b);
static void emit_tex_expr(const dval_t *f, sbuf_t *b, int parent_prec);
static void emit_tex_expr_abs(const dval_t *f, sbuf_t *b, int parent_prec);
static void emit_func(const dval_t *f, sbuf_t *b, int parent_prec);
static void emit_func_abs(const dval_t *f, sbuf_t *b, int parent_prec);

static int expr_is_negative(const dval_t *f)
{
    dval_t *fac[64];
    int n = 0;
    int sign = 0;

    if (!f)
        return 0;
    if (dv_tostring_is_negative_const(f) || dv_is_neg(f))
        return 1;
    if (dv_is_mul(f)) {
        flatten_mul((dval_t *)f, fac, &n, 64);
        for (int i = 0; i < n; ++i)
            sign ^= expr_is_negative(fac[i]) ? 1 : 0;
        return sign;
    }
    if (dv_is_op(f, &ops_div))
        return expr_is_negative(f->a) ^ expr_is_negative(f->b);
    return 0;
}

static void emit_factor_abs(const dval_t *f, sbuf_t *b)
{
    if (expr_is_negative(f))
        emit_expr_abs(f, b, PREC_MUL);
    else
        emit_expr(f, b, PREC_MUL);
}

static int expr_renders_negative(const dval_t *f)
{
    sbuf_t b;
    int neg;

    sbuf_init(&b);
    emit_expr(f, &b, 0);
    neg = (b.len > 0 && b.data[0] == '-');
    sbuf_free(&b);
    return neg;
}

static void emit_expr_abs(const dval_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    if (dv_tostring_is_negative_const(f)) {
        char *text;
        number_t pos_value = num_neg(f->c);

        if (emit_negative_const_binding_expr_abs(f, b, false)) {
            num_destroy(&pos_value);
            return;
        }

        text = dv_number_to_string_local(pos_value);
        if (text) {
            sbuf_puts(b, text);
            free(text);
        }
        return;
    }

    if (dv_is_neg(f)) {
        emit_expr(f->a, b, parent_prec);
        return;
    }

    if (dv_is_mul(f)) {
        dval_t *fac[64];
        int n = 0;

        flatten_mul((dval_t *)f, fac, &n, 64);
        sort_factors(fac, n);

        for (int i = 0; i < n; ++i) {
            if (dv_tostring_is_negative_const(fac[i])) {
                if (num_eq(fac[i]->c, NUM_NEG_ONE)) {
                    for (int j = i; j < n - 1; ++j)
                        fac[j] = fac[j + 1];
                    --n;
                }
            }
        }

        for (int i = 0; i < n; ++i) {
            if (i > 0) {
                int left_atomic = is_atomic_for_mul(fac[i - 1]);
                int right_atomic = is_atomic_for_mul(fac[i]);

                if (!(left_atomic && right_atomic))
                    sbuf_puts(b, "·");
            }
            emit_factor_abs(fac[i], b);
        }
        return;
    }

    if (dv_is_op(f, &ops_div) && expr_is_negative(f->a)) {
        int need = PREC_MUL < parent_prec;
        if (need) sbuf_putc(b, '(');
        emit_expr_abs(f->a, b, PREC_MUL);
        sbuf_putc(b, '/');
        emit_expr(f->b, b, PREC_POW);
        if (need) sbuf_putc(b, ')');
        return;
    }

    emit_expr(f, b, parent_prec);
}

static void emit_expr_abs_bars(const dval_t *f, sbuf_t *b)
{
    sbuf_putc(b, '|');
    emit_expr_abs(f, b, 0);
    sbuf_putc(b, '|');
}

static void emit_tex_name(sbuf_t *b, const char *name)
{
    char *tex;

    if (!name || !*name) {
        sbuf_puts(b, "x");
        return;
    }

    if (dv_tostring_is_simple_name(name)) {
        tex = dv_tostring_texify(name);
        if (tex) {
            sbuf_puts(b, tex);
            free(tex);
        } else {
            sbuf_puts(b, name);
        }
        return;
    }

    sbuf_putc(b, '[');
    tex = dv_tostring_texify(name);
    if (tex) {
        sbuf_puts(b, tex);
        free(tex);
    } else {
        sbuf_puts(b, name);
    }
    sbuf_putc(b, ']');
}

static void emit_tex_number_value(sbuf_t *b, number_t value)
{
    char *text = dv_number_to_string_local(value);
    char *tex;

    if (!text)
        return;

    tex = dv_tostring_texify(text);
    if (tex) {
        sbuf_puts(b, tex);
        free(tex);
    } else {
        sbuf_puts(b, text);
    }
    free(text);
}

static void emit_tex_const_value(sbuf_t *b, const dval_t *dv)
{
    char *text = dv_const_to_string_local(dv);
    char *tex;

    if (!text)
        return;

    tex = dv_tostring_texify(text);
    if (tex) {
        sbuf_puts(b, tex);
        free(tex);
    } else {
        sbuf_puts(b, text);
    }
    free(text);
}

static void emit_tex_atom(const dval_t *f, sbuf_t *b)
{
    if (dv_is_const(f)) {
        if (dv_tostring_should_emit_binding_expr(f)) {
            char *text = dv_binding_expr_to_tex(f->binding_expr);

            if (text) {
                sbuf_puts(b, text);
                free(text);
            }
        } else if (f->name && *f->name)
            emit_tex_name(b, f->name);
        else
            emit_tex_const_value(b, f);
        return;
    }

    if (dv_is_var(f)) {
        emit_tex_name(b, f->name ? f->name : "x");
        return;
    }

    emit_tex_number_value(b, dv_eval(f));
}

static const char *tex_unary_name(const dval_t *f)
{
    if (!f || !f->ops)
        return NULL;

    if (dv_is_op(f, &ops_abs))
        return NULL;
    if (dv_is_sqrt_expr(f))
        return "\\sqrt";
    return f->ops->tex_name;
}

static int tex_exp_needs_parens(const dval_t *e)
{
    if (!e)
        return 0;
    if (e->ops->arity == DV_OP_ATOM)
        return 0;
    if (dv_is_neg(e) || dv_is_pow_d_expr(e))
        return 1;
    if (e->ops->arity == DV_OP_UNARY)
        return 0;
    if (dv_is_addsub(e) || dv_is_mul(e) || dv_is_op(e, &ops_div) || dv_is_op(e, &ops_pow))
        return 1;
    return 0;
}

static void emit_tex_factor_abs(const dval_t *f, sbuf_t *b)
{
    if (expr_is_negative(f))
        emit_tex_expr_abs(f, b, PREC_MUL);
    else
        emit_tex_expr(f, b, PREC_MUL);
}

static void emit_tex_expr_abs(const dval_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    if (dv_tostring_is_negative_const(f)) {
        if (emit_negative_const_binding_expr_abs(f, b, true))
            return;
        emit_tex_number_value(b, num_neg(f->c));
        return;
    }

    if (dv_is_neg(f)) {
        emit_tex_expr(f->a, b, parent_prec);
        return;
    }

    if (dv_is_mul(f)) {
        dval_t *fac[64];
        int n = 0;

        flatten_mul((dval_t *)f, fac, &n, 64);
        sort_factors(fac, n);

        for (int i = 0; i < n; ++i) {
            if (dv_tostring_is_negative_const(fac[i]) && num_eq(fac[i]->c, NUM_NEG_ONE)) {
                for (int j = i; j < n - 1; ++j)
                    fac[j] = fac[j + 1];
                --n;
                break;
            }
        }

        for (int i = 0; i < n; ++i) {
            if (i > 0) {
                int left_atomic = is_atomic_for_mul(fac[i - 1]);
                int right_atomic = is_atomic_for_mul(fac[i]);

                if (left_atomic && right_atomic)
                    sbuf_putc(b, ' ');
                else
                    sbuf_puts(b, " \\cdot ");
            }
            emit_tex_factor_abs(fac[i], b);
        }
        return;
    }

    if (dv_is_op(f, &ops_div) && expr_is_negative(f->a)) {
        int need = PREC_MUL < parent_prec;
        if (need)
            sbuf_puts(b, "\\left(");
        sbuf_puts(b, "\\frac{");
        emit_tex_expr_abs(f->a, b, PREC_LOWEST);
        sbuf_puts(b, "}{");
        emit_tex_expr(f->b, b, PREC_LOWEST);
        sbuf_putc(b, '}');
        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    emit_tex_expr(f, b, parent_prec);
}

static void emit_func_abs(const dval_t *f, sbuf_t *b, int parent_prec)
{
    sbuf_t tmp;

    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    if (dv_is_neg(f)) {
        emit_func(f->a, b, parent_prec);
        return;
    }

    if (!expr_renders_negative(f)) {
        emit_func(f, b, parent_prec);
        return;
    }

    sbuf_init(&tmp);
    emit_func(f, &tmp, parent_prec);
    if (tmp.len > 0 && tmp.data[0] == '-')
        sbuf_puts(b, tmp.data + 1u);
    else
        sbuf_puts(b, tmp.data);
    sbuf_free(&tmp);
}

static void emit_tex_expr(const dval_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    if (dv_is_const(f) || dv_is_var(f)) {
        emit_tex_atom(f, b);
        return;
    }

    if (dv_is_neg(f)) {
        int need = PREC_UNARY < parent_prec;
        const dval_t *a = f->a;

        if (need)
            sbuf_puts(b, "\\left(");
        if (dv_is_neg(a)) {
            emit_tex_expr(a->a, b, 0);
        } else if (expr_is_negative(a)) {
            emit_tex_expr_abs(a, b, 0);
        } else {
            int child_needs_paren = dv_is_addsub(a);
            sbuf_putc(b, '-');
            if (child_needs_paren)
                sbuf_puts(b, "\\left(");
            emit_tex_expr(a, b, 0);
            if (child_needs_paren)
                sbuf_puts(b, "\\right)");
        }
        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (f->ops->arity == DV_OP_UNARY) {
        int need = PREC_UNARY < parent_prec;
        const char *name = tex_unary_name(f);

        if (need)
            sbuf_puts(b, "\\left(");

        if (dv_is_op(f, &ops_abs)) {
            sbuf_puts(b, "\\left|");
            emit_tex_expr_abs(f->a, b, 0);
            sbuf_puts(b, "\\right|");
        } else if (dv_is_op(f, &ops_floor)) {
            sbuf_puts(b, "\\left\\lfloor ");
            emit_tex_expr(f->a, b, 0);
            sbuf_puts(b, " \\right\\rfloor");
        } else if (dv_is_op(f, &ops_ceil)) {
            sbuf_puts(b, "\\left\\lceil ");
            emit_tex_expr(f->a, b, 0);
            sbuf_puts(b, " \\right\\rceil");
        } else if (dv_is_sqrt_expr(f)) {
            sbuf_puts(b, "\\sqrt{");
            emit_tex_expr(f->a, b, 0);
            sbuf_putc(b, '}');
        } else if (dv_is_op(f, &ops_exp)) {
            sbuf_puts(b, "e^{");
            emit_tex_expr(f->a, b, 0);
            sbuf_putc(b, '}');
        } else {
            sbuf_puts(b, name ? name : "\\operatorname{f}");
            sbuf_putc(b, '(');
            emit_tex_expr(f->a, b, 0);
            sbuf_putc(b, ')');
        }

        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (dv_is_pow_d_expr(f)) {
        int need = PREC_POW < parent_prec;
        long ei = 0;
        int exponent_has_small_int = dv_try_get_small_integer_exponent(f->c, &ei);

        if (need)
            sbuf_puts(b, "\\left(");

        if (f->a->ops->arity == DV_OP_UNARY && !dv_is_sqrt_expr(f->a) && !dv_is_op(f->a, &ops_abs)) {
            const char *name = tex_unary_name(f->a);
            sbuf_puts(b, name ? name : "\\operatorname{f}");
            sbuf_puts(b, "^{");
            if (exponent_has_small_int) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%ld", ei);
                sbuf_puts(b, buf);
            } else {
                emit_tex_const_value(b, f);
            }
            sbuf_puts(b, "}(");
            emit_tex_expr(f->a->a, b, 0);
            sbuf_putc(b, ')');
        } else {
            int base_needs_parens = pow_base_needs_visible_parens(f->a);

            if (base_needs_parens)
                sbuf_puts(b, "\\left(");
            emit_tex_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
            if (base_needs_parens)
                sbuf_puts(b, "\\right)");
            sbuf_puts(b, "^{");
            if (exponent_has_small_int) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%ld", ei);
                sbuf_puts(b, buf);
            } else {
                emit_tex_const_value(b, f);
            }
            sbuf_putc(b, '}');
        }

        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (dv_is_mul(f)) {
        int need = PREC_MUL < parent_prec;
        dval_t *fac[64];
        int n = 0;
        int sign = 1;

        if (need)
            sbuf_puts(b, "\\left(");

        flatten_mul((dval_t *)f, fac, &n, 64);
        sort_factors(fac, n);

        for (int i = 0; i < n; i++) {
            if (!expr_is_negative(fac[i]))
                continue;

            sign = -sign;

            if (dv_tostring_is_negative_const(fac[i])) {
                if (num_eq(fac[i]->c, NUM_NEG_ONE)) {
                    for (int j = i; j < n - 1; j++)
                        fac[j] = fac[j + 1];
                    n--;
                    i--;
                    continue;
                }
                continue;
            }

            if (dv_is_neg(fac[i])) {
                fac[i] = fac[i]->a;
                continue;
            }

            break;
        }

        if (sign < 0)
            sbuf_putc(b, '-');

        for (int i = 0; i < n; i++) {
            if (i > 0) {
                int left_atomic = is_atomic_for_mul(fac[i - 1]);
                int right_atomic = is_atomic_for_mul(fac[i]);

                if (left_atomic && right_atomic)
                    sbuf_putc(b, ' ');
                else
                    sbuf_puts(b, " \\cdot ");
            }
            emit_tex_factor_abs(fac[i], b);
        }

        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (dv_is_addsub(f)) {
        int need = PREC_ADD < parent_prec;
        bool neg = expr_renders_negative(f->b);

        if (need)
            sbuf_puts(b, "\\left(");
        emit_tex_expr(f->a, b, PREC_ADD);

        if (dv_is_op(f, &ops_add))
            sbuf_puts(b, neg ? " - " : " + ");
        else
            sbuf_puts(b, neg ? " + " : " - ");

        if (neg)
            emit_tex_expr_abs(f->b, b, PREC_ADD);
        else
            emit_tex_expr(f->b, b, PREC_ADD);

        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (dv_is_op(f, &ops_div)) {
        int need = PREC_MUL < parent_prec;
        bool neg_num = expr_is_negative(f->a);
        bool neg_den = expr_is_negative(f->b);

        if (need)
            sbuf_puts(b, "\\left(");
        if (neg_num ^ neg_den)
            sbuf_putc(b, '-');

        sbuf_puts(b, "\\frac{");
        if (neg_num)
            emit_tex_expr_abs(f->a, b, PREC_LOWEST);
        else
            emit_tex_expr(f->a, b, PREC_LOWEST);
        sbuf_puts(b, "}{");
        if (neg_den)
            emit_tex_expr_abs(f->b, b, PREC_LOWEST);
        else
            emit_tex_expr(f->b, b, PREC_LOWEST);
        sbuf_putc(b, '}');

        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (dv_is_op(f, &ops_pow)) {
        int need = PREC_POW < parent_prec;
        int base_needs_parens = pow_base_needs_visible_parens(f->a);

        if (need)
            sbuf_puts(b, "\\left(");
        if (base_needs_parens)
            sbuf_puts(b, "\\left(");
        emit_tex_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
        if (base_needs_parens)
            sbuf_puts(b, "\\right)");
        sbuf_puts(b, "^{");
        if (tex_exp_needs_parens(f->b))
            emit_tex_expr(f->b, b, 0);
        else
            emit_tex_expr(f->b, b, 0);
        sbuf_putc(b, '}');
        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (f->ops->arity == DV_OP_BINARY) {
        sbuf_puts(b, "\\operatorname{");
        sbuf_puts(b, f->ops->name);
        sbuf_puts(b, "}(");
        emit_tex_expr(f->a, b, 0);
        sbuf_puts(b, ", ");
        emit_tex_expr(f->b, b, 0);
        sbuf_putc(b, ')');
        return;
    }

    emit_tex_atom(f, b);
}

static void emit_expr(const dval_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) { sbuf_puts(b, "0"); return; }

    /* Atoms */
    if (dv_is_const(f) || dv_is_var(f)) {
        emit_atom((dval_t *)f, b);
        return;
    }

    /* Negation: -a  — only parenthesise when the child is an add/sub */
    if (dv_is_neg(f)) {
        int need = PREC_UNARY < parent_prec;
        if (need) sbuf_putc(b, '(');

        const dval_t *a = f->a;
        if (dv_is_neg(a)) {
            emit_expr(a->a, b, 0);
            if (need) sbuf_putc(b, ')');
            return;
        }
        if (expr_is_negative(a)) {
            emit_expr_abs(a, b, 0);
            if (need) sbuf_putc(b, ')');
            return;
        }
        int child_needs_paren = dv_is_addsub(a);
        sbuf_putc(b, '-');
        if (child_needs_paren) sbuf_putc(b, '(');
        emit_expr(a, b, 0);
        if (child_needs_paren) sbuf_putc(b, ')');

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Unary ops */
    if (f->ops->arity == DV_OP_UNARY) {
        int need = PREC_UNARY < parent_prec;
        if (need) sbuf_putc(b, '(');

        if (dv_is_op(f, &ops_abs)) {
            emit_expr_abs_bars(f->a, b);
            if (need) sbuf_putc(b, ')');
            return;
        }
        if (dv_is_op(f, &ops_floor)) {
            sbuf_puts(b, "⌊");
            emit_expr(f->a, b, 0);
            sbuf_puts(b, "⌋");
            if (need) sbuf_putc(b, ')');
            return;
        }
        if (dv_is_op(f, &ops_ceil)) {
            sbuf_puts(b, "⌈");
            emit_expr(f->a, b, 0);
            sbuf_puts(b, "⌉");
            if (need) sbuf_putc(b, ')');
            return;
        }
        if (dv_is_sqrt_expr(f))
            sbuf_puts(b, "√");
        else
            sbuf_puts(b, f->ops->name);
        sbuf_putc(b, '(');
        emit_expr(f->a, b, 0);
        sbuf_putc(b, ')');

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Power */
    if (dv_is_pow_d_expr(f)) {
        int need = PREC_POW < parent_prec;
        long ei = 0;
        int exponent_has_small_int = dv_try_get_small_integer_exponent(f->c, &ei);

        if (need) sbuf_putc(b, '(');

        /* For unary functions raised to a power, write func²(arg)
         * rather than func(arg)² so the exponent binds to the function name.
         * Floor/ceiling keep their mathematical brackets: ⌊x⌋². */
        if (f->a->ops->arity == DV_OP_UNARY) {
            dval_t *inner = f->a;
            if (dv_is_op(inner, &ops_floor) || dv_is_op(inner, &ops_ceil)) {
                emit_expr(inner, b, PREC_POW);
            } else {
                if (dv_is_sqrt_expr(inner))
                    sbuf_puts(b, "√");
                else
                    sbuf_puts(b, inner->ops->name);
            }

            if (exponent_has_small_int)
                emit_superscript_int(b, ei);
            else {
                sbuf_putc(b, '^');
                char *text = dv_const_to_string_local(f);
                if (text) {
                    sbuf_puts(b, text);
                    free(text);
                }
            }

            if (!dv_is_op(inner, &ops_floor) && !dv_is_op(inner, &ops_ceil)) {
                sbuf_putc(b, '(');
                emit_expr(inner->a, b, 0);
                sbuf_putc(b, ')');
            }

            if (need) sbuf_putc(b, ')');
            return;
        }

        {
            int base_needs_parens = pow_base_needs_visible_parens(f->a);

            if (base_needs_parens) sbuf_putc(b, '(');
            emit_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
            if (base_needs_parens) sbuf_putc(b, ')');
        }

        if (exponent_has_small_int)
            emit_superscript_int(b, ei);
        else {
            sbuf_putc(b, '^');
            char *text = dv_const_to_string_local(f);
            if (text) {
                sbuf_puts(b, text);
                free(text);
            }
        }

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Multiplication with sign folding */
    if (dv_is_mul(f)) {
        int need = PREC_MUL < parent_prec;
        dval_t *fac[64];
        int n = 0;

        if (need) sbuf_putc(b, '(');

        flatten_mul((dval_t *)f, fac, &n, 64);
        sort_factors(fac, n);

        int sign = 1;
        for (int i = 0; i < n; i++) {
            if (!expr_is_negative(fac[i]))
                continue;

            sign = -sign;

            if (dv_tostring_is_negative_const(fac[i])) {
                if (num_eq(fac[i]->c, NUM_NEG_ONE)) {
                    for (int j = i; j < n - 1; j++)
                        fac[j] = fac[j + 1];
                    n--;
                    i--;
                    continue;
                }
                continue;
            }

            if (dv_is_neg(fac[i])) {
                fac[i] = fac[i]->a;
                continue;
            }

            break;
        }

        if (sign < 0)
            sbuf_putc(b, '-');

        for (int i = 0; i < n; i++) {
            if (i > 0) {
                int left_atomic  = is_atomic_for_mul(fac[i - 1]);
                int right_atomic = is_atomic_for_mul(fac[i]);

                if (!(left_atomic && right_atomic))
                    sbuf_puts(b, "·");
            }
            emit_factor_abs(fac[i], b);
        }

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Addition/subtraction with a + -b → a - b and a - -b → a + b */
    if (dv_is_addsub(f)) {
        int need = PREC_ADD < parent_prec;
        if (need) sbuf_putc(b, '(');

        emit_expr(f->a, b, PREC_ADD);

        bool neg = expr_renders_negative(f->b);

        /* Emit flipped operator if needed */
        if (dv_is_op(f, &ops_add)) {
            sbuf_puts(b, neg ? " - " : " + ");
        } else { /* subtraction */
            sbuf_puts(b, neg ? " + " : " - ");
        }

        if (neg) {
            emit_expr_abs(f->b, b, PREC_ADD);
        } else {
            emit_expr(f->b, b, PREC_ADD);
        }

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Division: normalise sign onto the outside when possible */
    if (dv_is_op(f, &ops_div)) {
        int need = PREC_MUL < parent_prec;
        bool neg_num = expr_is_negative(f->a);
        bool neg_den = expr_is_negative(f->b);
        bool neg = neg_num ^ neg_den;

        if (need) sbuf_putc(b, '(');
        if (neg) sbuf_putc(b, '-');

        if (neg_num) emit_expr_abs(f->a, b, PREC_MUL);
        else         emit_expr(f->a, b, PREC_MUL);

        sbuf_putc(b, '/');

        if (neg_den) emit_expr_abs(f->b, b, PREC_POW);
        else         emit_expr(f->b, b, PREC_POW);

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Binary power: base^exp  or  base^(exp) when exponent needs grouping */
    if (dv_is_op(f, &ops_pow)) {
        int need = PREC_POW < parent_prec;
        int base_needs_parens = pow_base_needs_visible_parens(f->a);
        if (need) sbuf_putc(b, '(');

        if (base_needs_parens) sbuf_putc(b, '(');
        emit_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
        if (base_needs_parens) sbuf_putc(b, ')');
        sbuf_putc(b, '^');
        int ep = pow_exp_needs_parens(f->b);
        if (ep) sbuf_putc(b, '(');
        emit_expr(f->b, b, 0);
        if (ep) sbuf_putc(b, ')');

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Named binary functions (e.g. atan2) */
    if (f->ops->arity == DV_OP_BINARY) {
        sbuf_puts(b, f->ops->name);
        sbuf_putc(b, '(');
        emit_expr(f->a, b, 0);
        sbuf_puts(b, ", ");
        emit_expr(f->b, b, 0);
        sbuf_putc(b, ')');
        return;
    }

    /* Fallback */
    emit_atom((dval_t *)f, b);
}

/* ------------------------------------------------------------------------- */
/* FUNCTION MODE (calculator-style)                                          */
/* ------------------------------------------------------------------------- */

static void emit_func(const dval_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) { sbuf_puts(b, "0"); return; }

    if (dv_is_const(f)) {
        if (dv_tostring_should_emit_binding_expr(f)) {
            char *text = dv_binding_expr_to_string(f->binding_expr);

            if (text) {
                sbuf_puts(b, text);
                free(text);
            }
        } else if (f->name && *f->name)
            emit_name_func(b, f->name);
        else {
            char *text = dv_const_to_string_local(f);
            if (text) {
                sbuf_puts(b, text);
                free(text);
            }
        }
        return;
    }

    if (dv_is_var(f)) {
        emit_name_func(b, f->name ? f->name : "x");
        return;
    }

    if (f->ops->arity == DV_OP_UNARY) {
        int need = PREC_UNARY < parent_prec;
        if (need) sbuf_putc(b, '(');

        sbuf_puts(b, f->ops->name);
        sbuf_putc(b, '(');
        emit_func(f->a, b, 0);
        sbuf_putc(b, ')');

        if (need) sbuf_putc(b, ')');
        return;
    }

    if (dv_is_pow_d_expr(f)) {
        int need = PREC_POW < parent_prec;
        int base_needs_parens = pow_base_needs_visible_parens(f->a);
        if (need) sbuf_putc(b, '(');

        if (base_needs_parens) sbuf_putc(b, '(');
        emit_func(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
        if (base_needs_parens) sbuf_putc(b, ')');

        sbuf_putc(b, '^');
        char *text = dv_const_to_string_local(f);
        if (text) {
            sbuf_puts(b, text);
            free(text);
        }

        if (need) sbuf_putc(b, ')');
        return;
    }

    if (dv_is_mul(f)) {
        int need = PREC_MUL < parent_prec;
        if (need) sbuf_putc(b, '(');

        dval_t *fac[64];
        int n = 0;
        flatten_mul((dval_t *)f, fac, &n, 64);
        sort_factors(fac, n);

        for (int i = 0; i < n; i++) {
            if (i > 0)
                sbuf_putc(b, '*');
            emit_func(fac[i], b, PREC_MUL);
        }

        if (need) sbuf_putc(b, ')');
        return;
    }

    if (dv_is_addsub(f)) {
        int need = PREC_ADD < parent_prec;
        int neg;

        if (need) sbuf_putc(b, '(');

        emit_func(f->a, b, PREC_ADD);
        neg = expr_renders_negative(f->b);

        if (dv_is_op(f, &ops_add))
            sbuf_puts(b, neg ? " - " : " + ");
        else
            sbuf_puts(b, neg ? " + " : " - ");

        if (neg)
            emit_func_abs(f->b, b, PREC_ADD);
        else
            emit_func(f->b, b, PREC_ADD);

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Division: a/b */
    if (dv_is_op(f, &ops_div)) {
        int need = PREC_MUL < parent_prec;
        if (need) sbuf_putc(b, '(');

        emit_func(f->a, b, PREC_MUL);
        sbuf_putc(b, '/');
        emit_func(f->b, b, PREC_POW);

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Binary power: base^exp  or  base^(exp) when exponent needs grouping */
    if (dv_is_op(f, &ops_pow)) {
        int need = PREC_POW < parent_prec;
        int base_needs_parens = pow_base_needs_visible_parens(f->a);
        if (need) sbuf_putc(b, '(');

        if (base_needs_parens) sbuf_putc(b, '(');
        emit_func(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
        if (base_needs_parens) sbuf_putc(b, ')');
        sbuf_putc(b, '^');
        int ep = pow_exp_needs_parens(f->b);
        if (ep) sbuf_putc(b, '(');
        emit_func(f->b, b, 0);
        if (ep) sbuf_putc(b, ')');

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Named binary functions (e.g. atan2) */
    if (f->ops->arity == DV_OP_BINARY) {
        sbuf_puts(b, f->ops->name);
        sbuf_putc(b, '(');
        emit_func(f->a, b, 0);
        sbuf_puts(b, ", ");
        emit_func(f->b, b, 0);
        sbuf_putc(b, ')');
        return;
    }

    emit_name_func(b, f->name ? f->name : "?");
}

/* ------------------------------------------------------------------------- */
/* Variable discovery (DFS order)                                            */
/* ------------------------------------------------------------------------- */

typedef struct {
    dval_t **vars;
    size_t   count;
    size_t   cap;
} varlist_t;

static void varlist_init(varlist_t *vl)
{
    vl->vars  = NULL;
    vl->count = 0;
    vl->cap   = 0;
}

static void varlist_add(varlist_t *vl, dval_t *v)
{
    for (size_t i = 0; i < vl->count; ++i)
        if (vl->vars[i] == v)
            return;

    if (vl->count == vl->cap) {
        vl->cap = vl->cap ? vl->cap * 2 : 4;
        vl->vars = (dval_t **)realloc(vl->vars, vl->cap * sizeof(dval_t *));
        if (!vl->vars) {
            fprintf(stderr, "varlist_add: out of memory\n");
            abort();
        }
    }
    vl->vars[vl->count++] = v;
}

static void find_vars_dfs(const dval_t *f, varlist_t *vl)
{
    if (!f) return;

    if (dv_is_var(f)) {
        varlist_add(vl, (dval_t *)f);
        return;
    }

    if (dv_is_const(f)) return;

    find_vars_dfs(f->a, vl);
    find_vars_dfs(f->b, vl);
}

static void find_named_consts_dfs(const dval_t *f, varlist_t *cl)
{
    if (!f) return;

    if (dv_is_const(f)) {
        if (f->name && *f->name && !dv_is_immortal_default_const_local(f))
            varlist_add(cl, (dval_t *)f);
        return;
    }

    if (dv_is_var(f)) return;

    find_named_consts_dfs(f->a, cl);
    find_named_consts_dfs(f->b, cl);
}

static void find_explicit_named_consts_dfs(const dval_t *f, varlist_t *cl)
{
    if (!f) return;

    if (dv_is_const(f)) {
        if (f->name && *f->name && f->binding_expr &&
            !dv_is_immortal_default_const_local(f))
            varlist_add(cl, (dval_t *)f);
        return;
    }

    if (dv_is_var(f)) return;

    find_explicit_named_consts_dfs(f->a, cl);
    find_explicit_named_consts_dfs(f->b, cl);
}

static const char *dv_name_or_default(const dval_t *dv, const char *fallback)
{
    return (dv->name && *dv->name) ? dv->name : fallback;
}

static char *binding_rhs_expr_string_local(const dval_t *dv)
{
    if (dv && dv->binding_expr)
        return dv_binding_expr_to_string(dv->binding_expr);
    return dv_const_to_string_local(dv);
}

static char *binding_rhs_tex_string_local(const dval_t *dv)
{
    if (dv && dv->binding_expr)
        return dv_binding_expr_to_tex(dv->binding_expr);
    return dv_const_to_string_local(dv);
}

static void emit_binding_line(sbuf_t *b,
                              const dval_t *dv,
                              const char *name,
                              void (*emit_name_style)(sbuf_t *, const char *))
{
    char *valbuf;

    emit_name_style(b, name);
    sbuf_puts(b, " = ");
    valbuf = binding_rhs_expr_string_local(dv);
    if (valbuf) {
        sbuf_puts(b, valbuf);
        free(valbuf);
    }
    sbuf_putc(b, '\n');
}

static void emit_function_arg_list(sbuf_t *b, const varlist_t *vl, const varlist_t *cl)
{
    for (size_t i = 0; i < vl->count; ++i) {
        if (i > 0)
            sbuf_putc(b, ',');
        emit_name_func(b, dv_name_or_default(vl->vars[i], "x"));
    }
    for (size_t i = 0; i < cl->count; ++i) {
        if (vl->count > 0 || i > 0)
            sbuf_putc(b, ',');
        emit_name_func(b, cl->vars[i]->name);
    }
}

/* ------------------------------------------------------------------------- */
/* FUNCTION-style printing                                                   */
/* ------------------------------------------------------------------------- */

static char *dv_to_string_function(const dval_t *f)
{
    sbuf_t b;
    sbuf_init(&b);

    /* Assign auto-names (x₀, x₁, …) to unnamed vars before rendering.
     * Unnamed numeric constants (coefficients created by dv_mul_num /
     * dv_add_num etc.) are not auto-named; they appear as plain numbers.
     * Callers that want symbolic unnamed constants should use dv_new_named_const(). */
    autoname_table_t vnames;
    autoname_init(&vnames);
    assign_unnamed_vars_dfs((dval_t *)f, &vnames);

    const dval_t *g = f;

    /* Discover variables and named constants */
    varlist_t vl;
    varlist_init(&vl);
    find_vars_dfs(g, &vl);

    varlist_t cl;
    varlist_init(&cl);
    find_explicit_named_consts_dfs(f, &cl);
    find_named_consts_dfs(g, &cl);

    /* Emit variable bindings */
    for (size_t i = 0; i < vl.count; ++i) {
        dval_t *v = vl.vars[i];
        emit_binding_line(&b, v, dv_name_or_default(v, "x"), emit_name_func);
    }

    /* Emit named constant bindings */
    for (size_t i = 0; i < cl.count; ++i) {
        dval_t *c = cl.vars[i];
        emit_binding_line(&b, c, c->name, emit_name_func);
    }

    /* Pure variable */
    if (dv_is_var(g)) {
        sbuf_puts(&b, "return ");
        emit_name_func(&b, dv_name_or_default(g, "x"));

        char *out = xstrdup(b.data);
        sbuf_free(&b);
        free(vl.vars);
        free(cl.vars);
        autoname_restore(&vnames);
        return out;
    }

    /* Pure constant */
    if (dv_is_const(g)) {
        const char *cname = dv_name_or_default(g, "c");

        sbuf_puts(&b, "return ");
        emit_name_func(&b, cname);

        if (!dv_is_immortal_default_const_local(g)) {
            sbuf_t prefix;
            sbuf_init(&prefix);
            emit_binding_line(&prefix, g, cname, emit_name_func);
            sbuf_puts(&prefix, b.data);
            free(b.data);
            b = prefix;
        }

        char *out = xstrdup(b.data);
        sbuf_free(&b);
        free(vl.vars);
        free(cl.vars);
        autoname_restore(&vnames);
        return out;
    }

    /* General expression */
    const char *fname = (g->name && *g->name) ? g->name : "expr";

    /* expr(x,y,z,π,...) = ... */
    sbuf_puts(&b, fname);
    sbuf_putc(&b, '(');
    emit_function_arg_list(&b, &vl, &cl);
    sbuf_puts(&b, ") = ");
    emit_func(g, &b, PREC_LOWEST);
    sbuf_putc(&b, '\n');

    /* return expr(x,y,z,π,...) */
    sbuf_puts(&b, "return ");
    sbuf_puts(&b, fname);
    sbuf_putc(&b, '(');
    emit_function_arg_list(&b, &vl, &cl);
    sbuf_puts(&b, ")");

    char *out = xstrdup(b.data);
    sbuf_free(&b);
    free(vl.vars);
    free(cl.vars);
    autoname_restore(&vnames);
    return out;
}

/* ------------------------------------------------------------------------- */
/* EXPRESSION-style printing                                                 */
/* ------------------------------------------------------------------------- */

static char *dv_to_string_expr(const dval_t *f)
{
    sbuf_t b;
    autoname_table_t vnames;
    autoname_init(&vnames);
    assign_unnamed_vars_dfs((dval_t *)f, &vnames);

    const dval_t *g = f;

    varlist_t vl;
    varlist_init(&vl);
    find_vars_dfs(g, &vl);

    varlist_t cl;
    varlist_init(&cl);
    find_explicit_named_consts_dfs(f, &cl);
    find_named_consts_dfs(g, &cl);

    sbuf_init(&b);
    if (vl.count == 0 && cl.count == 0) {
        emit_expr(g, &b, PREC_LOWEST);
    } else {
        sbuf_putc(&b, '{');
        sbuf_putc(&b, ' ');
        emit_expr(g, &b, PREC_LOWEST);
        sbuf_putc(&b, ' ');
        sbuf_putc(&b, '|');
        sbuf_putc(&b, ' ');

        for (size_t i = 0; i < vl.count; ++i) {
            dval_t *v = vl.vars[i];
            char *valbuf = binding_rhs_expr_string_local(v);
            emit_name(&b, dv_name_or_default(v, "x"));
            sbuf_puts(&b, " = ");
            if (valbuf) {
                sbuf_puts(&b, valbuf);
                free(valbuf);
            }

            if (i + 1 < vl.count)
                sbuf_puts(&b, ", ");
        }

        /* Named constants always follow ';' so round-trips preserve constness. */
        if (cl.count > 0) {
            sbuf_puts(&b, "; ");
            for (size_t i = 0; i < cl.count; ++i) {
                dval_t *c = cl.vars[i];
                char *valbuf = binding_rhs_expr_string_local(c);
                emit_name(&b, c->name);
                sbuf_puts(&b, " = ");
                if (valbuf) {
                    sbuf_puts(&b, valbuf);
                    free(valbuf);
                }
                if (i + 1 < cl.count)
                    sbuf_puts(&b, ", ");
            }
        }

        sbuf_putc(&b, ' ');
        sbuf_putc(&b, '}');
    }

    char *out = xstrdup(b.data);
    sbuf_free(&b);
    free(vl.vars);
    free(cl.vars);
    autoname_restore(&vnames);
    return out;
}

int dv_to_tex_parts(const dval_t *dv, char **expr_out, char **bindings_out)
{
    autoname_table_t vnames;
    const dval_t *g;
    varlist_t vl;
    varlist_t cl;
    sbuf_t expr;
    sbuf_t bindings;

    if (!expr_out || !bindings_out)
        return -1;

    *expr_out = NULL;
    *bindings_out = NULL;

    if (!dv) {
        *expr_out = xstrdup("NULL");
        *bindings_out = xstrdup("");
        return (*expr_out && *bindings_out) ? 0 : -1;
    }

    autoname_init(&vnames);
    assign_unnamed_vars_dfs((dval_t *)dv, &vnames);
    g = dv;

    varlist_init(&vl);
    varlist_init(&cl);
    find_vars_dfs(g, &vl);
    find_explicit_named_consts_dfs(dv, &cl);
    find_named_consts_dfs(g, &cl);

    sbuf_init(&expr);
    emit_tex_expr(g, &expr, PREC_LOWEST);

    sbuf_init(&bindings);
    if (vl.count > 0 || cl.count > 0) {
        for (size_t i = 0; i < vl.count; ++i) {
            dval_t *v = vl.vars[i];
            char *binding_text;

            if (i > 0)
                sbuf_puts(&bindings, ", ");
            emit_tex_name(&bindings, dv_name_or_default(v, "x"));
            sbuf_puts(&bindings, " = ");
            binding_text = binding_rhs_tex_string_local(v);
            if (binding_text) {
                sbuf_puts(&bindings, binding_text);
                free(binding_text);
            }
        }

        if (cl.count > 0) {
            sbuf_puts(&bindings, "; ");
            for (size_t i = 0; i < cl.count; ++i) {
                dval_t *c = cl.vars[i];
                char *binding_text;

                if (i > 0)
                    sbuf_puts(&bindings, ", ");
                emit_tex_name(&bindings, c->name);
                sbuf_puts(&bindings, " = ");
                binding_text = binding_rhs_tex_string_local(c);
                if (binding_text) {
                    sbuf_puts(&bindings, binding_text);
                    free(binding_text);
                }
            }
        }
    }

    *expr_out = dv_tostring_texify(expr.data);
    *bindings_out = dv_tostring_texify(bindings.data);

    sbuf_free(&expr);
    sbuf_free(&bindings);
    free(vl.vars);
    free(cl.vars);
    autoname_restore(&vnames);

    if (!*expr_out || !*bindings_out) {
        free(*expr_out);
        free(*bindings_out);
        *expr_out = NULL;
        *bindings_out = NULL;
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Public entry points                                                       */
/* ------------------------------------------------------------------------- */

static void strip_trailing_newline(char *s)
{
    size_t len = strlen(s);
    while (len > 0 &&
           (s[len - 1] == '\n' || s[len - 1] == '\r' ||
            s[len - 1] == ' '  || s[len - 1] == '\t'))
        s[--len] = '\0';
}

char *dv_to_string(const dval_t *dv, style_t style)
{
    char *out;
    char *expr = NULL;
    char *bindings = NULL;

    if (!dv) {
        char *s = (char *)xmalloc(5);
        strcpy(s, "NULL");
        return s;
    }

    if (style == style_TEX) {
        sbuf_t b;

        if (dv_to_tex_parts(dv, &expr, &bindings) != 0)
            return dv_to_string_expr(dv);

        sbuf_init(&b);
        if (bindings && *bindings) {
            sbuf_puts(&b, "\\left\\{ ");
            sbuf_puts(&b, expr);
            sbuf_puts(&b, " \\;\\middle|\\; ");
            sbuf_puts(&b, bindings);
            sbuf_puts(&b, " \\right\\}");
        } else {
            sbuf_puts(&b, expr);
        }

        free(expr);
        free(bindings);
        out = b.data;
    } else if (style == style_FUNCTION) {
        out = dv_to_string_function(dv);
    } else {
        out = dv_to_string_expr(dv);
    }

    strip_trailing_newline(out);
    return out;
}

void dv_print(const dval_t *dv)
{
    char *s = dv_to_string(dv, style_EXPRESSION);
    fputs(s, stdout);
    fputc('\n', stdout);
    free(s);
}
