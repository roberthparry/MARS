/* dval_tostring.c - symbolic/string conversion for dval_t
 *
 * Produces human-readable and round-trip-safe string representations of a
 * dval_t DAG via dv_to_string(dv, style).  Two styles are supported:
 *
 *   style_EXPRESSION  — infix notation, e.g.
 *                         { sin(x₀) * cos(x₁) | x₀ = 1.0, x₁ = 0.5 }
 *                       or, when no bindings are needed:
 *                         { 1 }
 *                       This format is accepted by dval_from_string().
 *
 *   style_FUNCTION    — nested prefix notation, e.g.
 *                         mul(sin(var(x₀=1.0)), cos(var(x₁=0.5)))
 *                       Useful for debugging graph structure.
 *
 * Responsibilities of this file:
 *   • Operator precedence and parenthesisation (infix only)
 *   • Unicode superscript encoding for integer powers (², ³, …)
 *   • The { expr } / { expr | bindings } wrapper for expression style
 *
 * All algebraic simplification (flattening, factoring, ordering, etc.)
 * is done in dv_simplify.c before this file is reached.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "qcomplex.h"
#include "dval_internal.h"
#include "dval_tostring_internal.h"
#include "dval.h"

/* ------------------------------------------------------------------------- */
/* Small helpers                                                             */
/* ------------------------------------------------------------------------- */

#define is_op dv_is_op
#define is_const dv_is_const
#define is_var dv_is_var
#define is_neg dv_is_neg
#define is_mul dv_is_mul
#define is_addsub dv_is_addsub
#define is_pow_d_expr dv_is_pow_d_expr
#define is_sqrt_expr dv_is_sqrt_expr

#define is_negative_const dv_tostring_is_negative_const
#define is_var_pow_d dv_tostring_is_var_pow_d

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

    if (is_var(f)) {
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

    if (is_const(f)) return;   /* constants have no children to recurse */

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

/* ------------------------------------------------------------------------- */
/* Atom helpers                                                              */
/* ------------------------------------------------------------------------- */

static void emit_atom(dval_t *f, sbuf_t *b)
{
    if (is_const(f)) {
        if (f->name && *f->name) {
            emit_name(b, f->name);
        } else {
            char buf[64];
            qf_to_string_simple(f->c, buf, sizeof(buf));
            sbuf_puts(b, buf);
        }
    } else if (is_var(f)) {
        emit_name(b, f->name ? f->name : "x");
    } else {
        char buf[64];
        qf_to_string_simple(dv_get_val(f), buf, sizeof(buf));
        sbuf_puts(b, buf);
    }
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
    if (is_neg(e))                     return 1;
    if (is_pow_d_expr(e))              return 1;  /* e.g. y² is ambiguous as exponent */
    if (e->ops->arity == DV_OP_UNARY)  return 0;  /* sin(…), exp(…), etc. */
    /* DV_OP_BINARY: arithmetic/pow need parens; named functions (atan2 …) don't */
    if (is_addsub(e) || is_mul(e) ||
        is_op(e, &ops_div) || is_op(e, &ops_pow)) return 1;
    return 0;
}

/* -------------------------------------------------------------
   Helper: atomic factors for implicit multiplication (EXPR mode)
   ------------------------------------------------------------- */
static int is_atomic_for_mul(const dval_t *f)
{
    if (!f) return 0;

    if (is_const(f)) {
        /* Unnamed numeric constants are always atomic (e.g. the leading "6" in 6x²).
         * Named constants are atomic only when their name is "simple" (single letter
         * or letter + subscript digits).  Multi-char names like "pi" or "radius"
         * are non-atomic so that a middle-dot separator is inserted between adjacent
         * bracketed terms: [pi]·[radius]² instead of [pi][radius]². */
        if (!f->name || !*f->name) return 1;
        return dv_tostring_is_simple_name(f->name);
    }

    if (is_var(f))
        return dv_tostring_is_simple_name(f->name);

    if (is_var_pow_d(f))
        return dv_tostring_is_simple_name(f->a->name);

    return 0;
}

/* -------------------------------------------------------------
   Factor classification / flattening / ordering
   ------------------------------------------------------------- */

static void flatten_mul(dval_t *f, dval_t **buf, int *count, int max)
{
    if (!f || *count >= max) return;

    if (is_mul(f)) {
        flatten_mul(f->a, buf, count, max);
        flatten_mul(f->b, buf, count, max);
    } else {
        buf[(*count)++] = f;
    }
}

/* Sort group for multiplication factors:
 *   0 = unnamed numeric constant      (e.g. 6)
 *   1 = Greek named constant          (e.g. π, τ) — alphabetical within group
 *   2 = Latin/other named constant    (e.g. e)    — alphabetical within group
 *   3 = variable or var^n             (e.g. x, x³) — alphabetical by var name
 *   4 = everything else (unary/binary fns) — sort by primary arg var name,
 *       stable so same-arg functions keep their original tree order
 */
static int factor_group(const dval_t *f)
{
    if (is_neg(f)) f = f->a;

    if (is_const(f)) {
        if (!f->name || !*f->name) return 0;
        /* Greek letters are UTF-8 multi-byte; first byte >= 0x80 */
        return ((unsigned char)f->name[0] >= 0x80) ? 1 : 2;
    }

    if (is_var(f))
        return 3;

    if (is_var_pow_d(f))
        return 3;

    return 4;
}

/* DFS to find the name of the first variable in an expression. */
static const char *first_var_name(const dval_t *f)
{
    if (!f) return "";
    if (is_var(f)) return f->name ? f->name : "";
    const char *a = first_var_name(f->a);
    if (*a) return a;
    return first_var_name(f->b);
}

/* Counts levels of function *nesting* (not tree depth).
 * pow_d and neg are transparent — cos²(x) has the same nesting depth as cos(x).
 * This makes cos²(x) (depth 1) sort before exp(sin(x)) (depth 2). */
static int factor_depth(const dval_t *f)
{
    if (!f || is_const(f) || is_var(f)) return 0;
    if (is_neg(f) || is_pow_d_expr(f)) return factor_depth(f->a);
    if (f->ops->arity == DV_OP_UNARY) return 1 + factor_depth(f->a);
    if (f->ops->arity == DV_OP_BINARY) {
        int da = factor_depth(f->a), db = factor_depth(f->b);
        return 1 + (da > db ? da : db);
    }
    return 0;
}

static const char *factor_sort_name(const dval_t *f)
{
    if (is_neg(f)) f = f->a;

    if (is_const(f))
        return (f->name && *f->name) ? f->name : "";

    if (is_var(f))
        return f->name ? f->name : "";

    if (is_var_pow_d(f))
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
        int kd = (kg == 4) ? factor_depth(key) : 0;
        int t = s - 1;
        while (t >= 0) {
            int tg = factor_group(fac[t]);
            int cmp;
            if (tg != kg) {
                cmp = tg - kg;
            } else if (kg == 4) {
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

static int expr_is_negative(const dval_t *f)
{
    dval_t *fac[64];
    int n = 0;
    int sign = 0;

    if (!f)
        return 0;
    if (is_negative_const(f) || is_neg(f))
        return 1;
    if (is_mul(f)) {
        flatten_mul((dval_t *)f, fac, &n, 64);
        for (int i = 0; i < n; ++i)
            sign ^= expr_is_negative(fac[i]) ? 1 : 0;
        return sign;
    }
    if (is_op(f, &ops_div))
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

    if (is_negative_const(f)) {
        dval_t tmp = *f;
        tmp.c = qc_neg(tmp.c);
        tmp.x_valid = 0;
        emit_expr(&tmp, b, parent_prec);
        return;
    }

    if (is_neg(f)) {
        emit_expr(f->a, b, parent_prec);
        return;
    }

    if (is_mul(f)) {
        dval_t *fac[64];
        dval_t pos_const;
        int n = 0;

        flatten_mul((dval_t *)f, fac, &n, 64);
        sort_factors(fac, n);

        for (int i = 0; i < n; ++i) {
            if (is_negative_const(fac[i])) {
                if (qf_to_double(fac[i]->c.re) == -1.0) {
                    for (int j = i; j < n - 1; ++j)
                        fac[j] = fac[j + 1];
                    --n;
                } else {
                    pos_const = *fac[i];
                    pos_const.c = qc_neg(fac[i]->c);
                    pos_const.x_valid = 0;
                    fac[i] = &pos_const;
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

    if (is_op(f, &ops_div) && expr_is_negative(f->a)) {
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

static void emit_expr(const dval_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) { sbuf_puts(b, "0"); return; }

    /* Atoms */
    if (is_const(f) || is_var(f)) {
        emit_atom((dval_t *)f, b);
        return;
    }

    /* Negation: -a  — only parenthesise when the child is an add/sub */
    if (is_neg(f)) {
        int need = PREC_UNARY < parent_prec;
        if (need) sbuf_putc(b, '(');

        const dval_t *a = f->a;
        if (is_neg(a)) {
            emit_expr(a->a, b, 0);
            if (need) sbuf_putc(b, ')');
            return;
        }
        if (expr_is_negative(a)) {
            emit_expr_abs(a, b, 0);
            if (need) sbuf_putc(b, ')');
            return;
        }
        int child_needs_paren = is_addsub(a);
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

        if (is_op(f, &ops_abs)) {
            emit_expr_abs_bars(f->a, b);
            if (need) sbuf_putc(b, ')');
            return;
        }
        if (is_sqrt_expr(f))
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
    if (is_pow_d_expr(f)) {
        int need = PREC_POW < parent_prec;
        if (need) sbuf_putc(b, '(');

        double ed = qf_to_double(f->c.re);
        long   ei = (long)ed;

        /* For unary functions raised to a power, write func²(arg)
         * rather than func(arg)² so the exponent binds to the function name. */
        if (f->a->ops->arity == DV_OP_UNARY) {
            dval_t *inner = f->a;
            if (is_sqrt_expr(inner))
                sbuf_puts(b, "√");
            else
                sbuf_puts(b, inner->ops->name);

            if (ed == (double)ei)
                emit_superscript_int(b, ei);
            else {
                sbuf_putc(b, '^');
                char buf[64];
                qf_to_string_simple(f->c, buf, sizeof(buf));
                sbuf_puts(b, buf);
            }

            sbuf_putc(b, '(');
            emit_expr(inner->a, b, 0);
            sbuf_putc(b, ')');

            if (need) sbuf_putc(b, ')');
            return;
        }

        emit_expr(f->a, b, PREC_POW);

        if (ed == (double)ei)
            emit_superscript_int(b, ei);
        else {
            sbuf_putc(b, '^');
            char buf[64];
            qf_to_string_simple(f->c, buf, sizeof(buf));
            sbuf_puts(b, buf);
        }

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Multiplication with sign folding */
    if (is_mul(f)) {
        int need = PREC_MUL < parent_prec;
        if (need) sbuf_putc(b, '(');

        dval_t *fac[64];
        int n = 0;
        flatten_mul((dval_t *)f, fac, &n, 64);
        sort_factors(fac, n);

        int sign = 1;
        dval_t pos_const;
        for (int i = 0; i < n; i++) {
            if (!expr_is_negative(fac[i]))
                continue;

            sign = -sign;

            if (is_negative_const(fac[i])) {
                if (qf_to_double(fac[i]->c.re) == -1.0) {
                    for (int j = i; j < n - 1; j++)
                        fac[j] = fac[j + 1];
                    n--;
                    i--;
                    continue;
                }
                pos_const = *fac[i];
                pos_const.c = qc_neg(fac[i]->c);
                pos_const.x_valid = 0;
                fac[i] = &pos_const;
                continue;
            }

            if (is_neg(fac[i])) {
                fac[i] = fac[i]->a;
                continue;
            }

            break;
        }

        if (sign < 0)
            sbuf_putc(b, '-');

        for (int i = 0; i < n; i++) {
            if (i > 0) {
                int left_atomic  = is_atomic_for_mul(fac[i-1]);
                int right_atomic = is_atomic_for_mul(fac[i]);

                if (left_atomic && right_atomic) {
                    /* implicit */
                } else {
                    sbuf_puts(b, "·");
                }
            }
            emit_factor_abs(fac[i], b);
        }

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Addition/subtraction with a + -b → a - b and a - -b → a + b */
    if (is_addsub(f)) {
        int need = PREC_ADD < parent_prec;
        if (need) sbuf_putc(b, '(');

        emit_expr(f->a, b, PREC_ADD);

        bool neg = expr_renders_negative(f->b);

        /* Emit flipped operator if needed */
        if (is_op(f, &ops_add)) {
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

    /* Division: normalize sign onto the outside when possible */
    if (is_op(f, &ops_div)) {
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
    if (is_op(f, &ops_pow)) {
        int need = PREC_POW < parent_prec;
        if (need) sbuf_putc(b, '(');

        emit_expr(f->a, b, PREC_POW);
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

    if (is_const(f)) {
        if (f->name && *f->name)
            emit_name_func(b, f->name);
        else {
            char buf[64];
            qf_to_string_simple(f->c, buf, sizeof(buf));
            sbuf_puts(b, buf);
        }
        return;
    }

    if (is_var(f)) {
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

    if (is_pow_d_expr(f)) {
        int need = PREC_POW < parent_prec;
        if (need) sbuf_putc(b, '(');

        emit_func(f->a, b, PREC_POW);

        sbuf_putc(b, '^');
        char buf[64];
        qf_to_string_simple(f->c, buf, sizeof(buf));
        sbuf_puts(b, buf);

        if (need) sbuf_putc(b, ')');
        return;
    }

    if (is_mul(f)) {
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

    if (is_addsub(f)) {
        int need = PREC_ADD < parent_prec;
        if (need) sbuf_putc(b, '(');

        emit_func(f->a, b, PREC_ADD);

        if (is_op(f, &ops_add))
            sbuf_puts(b, " + ");
        else
            sbuf_puts(b, " - ");

        emit_func(f->b, b, PREC_ADD);

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Division: a/b */
    if (is_op(f, &ops_div)) {
        int need = PREC_MUL < parent_prec;
        if (need) sbuf_putc(b, '(');

        emit_func(f->a, b, PREC_MUL);
        sbuf_putc(b, '/');
        emit_func(f->b, b, PREC_POW);

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Binary power: base^exp  or  base^(exp) when exponent needs grouping */
    if (is_op(f, &ops_pow)) {
        int need = PREC_POW < parent_prec;
        if (need) sbuf_putc(b, '(');

        emit_func(f->a, b, PREC_POW);
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

static void find_vars_dfs(dval_t *f, varlist_t *vl)
{
    if (!f) return;

    if (is_var(f)) {
        varlist_add(vl, f);
        return;
    }

    if (is_const(f)) return;

    find_vars_dfs(f->a, vl);
    find_vars_dfs(f->b, vl);
}

static void find_named_consts_dfs(dval_t *f, varlist_t *cl)
{
    if (!f) return;

    if (is_const(f)) {
        if (f->name && *f->name)
            varlist_add(cl, f);
        return;
    }

    if (is_var(f)) return;

    find_named_consts_dfs(f->a, cl);
    find_named_consts_dfs(f->b, cl);
}

/* ------------------------------------------------------------------------- */
/* FUNCTION-style printing                                                   */
/* ------------------------------------------------------------------------- */

static char *dv_to_string_function(const dval_t *f)
{
    sbuf_t b;
    sbuf_init(&b);

    /* Assign auto-names (x₀, x₁, …) to unnamed vars BEFORE simplification so
     * the names survive into the simplified tree — var leaf nodes are shared by
     * reference, so the name set here is visible throughout the tree after
     * dv_simplify returns.  Unnamed numeric constants (coefficients created by
     * dv_mul_d / dv_add_d etc.) are not auto-named; they appear as plain numbers.
     * Callers that want symbolic unnamed constants should use dv_new_named_const. */
    autoname_table_t vnames;
    autoname_init(&vnames);
    assign_unnamed_vars_dfs((dval_t *)f, &vnames);

    /* Simplify first */
    dval_t *g = dv_simplify((dval_t *)f);

    /* Discover variables and named constants */
    varlist_t vl;
    varlist_init(&vl);
    find_vars_dfs(g, &vl);

    varlist_t cl;
    varlist_init(&cl);
    find_named_consts_dfs(g, &cl);

    /* Emit variable bindings */
    for (size_t i = 0; i < vl.count; ++i) {
        dval_t *v = vl.vars[i];
        const char *vname = (v->name && *v->name) ? v->name : "x";

        emit_name_func(&b, vname);
        sbuf_puts(&b, " = ");

        char valbuf[64];
        qf_to_string_simple(v->c, valbuf, sizeof(valbuf));
        sbuf_puts(&b, valbuf);
        sbuf_putc(&b, '\n');
    }

    /* Emit named constant bindings */
    for (size_t i = 0; i < cl.count; ++i) {
        dval_t *c = cl.vars[i];
        emit_name_func(&b, c->name);
        sbuf_puts(&b, " = ");

        char valbuf[64];
        qf_to_string_simple(c->c, valbuf, sizeof(valbuf));
        sbuf_puts(&b, valbuf);
        sbuf_putc(&b, '\n');
    }

    /* Pure variable */
    if (is_var(g)) {
        const char *vname = (g->name && *g->name) ? g->name : "x";

        sbuf_puts(&b, "return ");
        emit_name_func(&b, vname);

        char *out = xstrdup(b.data);
        sbuf_free(&b);
        free(vl.vars);
        free(cl.vars);
        autoname_restore(&vnames);
        dv_free(g);
        return out;
    }

    /* Pure constant */
    if (is_const(g)) {
        const char *cname = (g->name && *g->name) ? g->name : "c";

        emit_name_func(&b, cname);
        sbuf_puts(&b, " = ");

        char valbuf[64];
        qf_to_string_simple(g->c, valbuf, sizeof(valbuf));
        sbuf_puts(&b, valbuf);
        sbuf_putc(&b, '\n');

        sbuf_puts(&b, "return ");
        emit_name_func(&b, cname);

        char *out = xstrdup(b.data);
        sbuf_free(&b);
        free(vl.vars);
        free(cl.vars);
        autoname_restore(&vnames);
        dv_free(g);
        return out;
    }

    /* General expression */
    const char *fname = (g->name && *g->name) ? g->name : "expr";

    /* expr(x,y,z,π,...) = ... */
    sbuf_puts(&b, fname);
    sbuf_putc(&b, '(');
    for (size_t i = 0; i < vl.count; ++i) {
        if (i > 0) sbuf_putc(&b, ',');
        const char *vname = (vl.vars[i]->name && *vl.vars[i]->name)
                            ? vl.vars[i]->name : "x";
        emit_name_func(&b, vname);
    }
    for (size_t i = 0; i < cl.count; ++i) {
        if (vl.count > 0 || i > 0) sbuf_putc(&b, ',');
        emit_name_func(&b, cl.vars[i]->name);
    }
    sbuf_puts(&b, ") = ");
    emit_func(g, &b, PREC_LOWEST);
    sbuf_putc(&b, '\n');

    /* return expr(x,y,z,π,...) */
    sbuf_puts(&b, "return ");
    sbuf_puts(&b, fname);
    sbuf_putc(&b, '(');
    for (size_t i = 0; i < vl.count; ++i) {
        if (i > 0) sbuf_putc(&b, ',');
        const char *vname = (vl.vars[i]->name && *vl.vars[i]->name)
                            ? vl.vars[i]->name : "x";
        emit_name_func(&b, vname);
    }
    for (size_t i = 0; i < cl.count; ++i) {
        if (vl.count > 0 || i > 0) sbuf_putc(&b, ',');
        emit_name_func(&b, cl.vars[i]->name);
    }
    sbuf_puts(&b, ")");

    char *out = xstrdup(b.data);
    sbuf_free(&b);
    free(vl.vars);
    free(cl.vars);
    autoname_restore(&vnames);
    dv_free(g);
    return out;
}

/* ------------------------------------------------------------------------- */
/* EXPRESSION-style printing                                                 */
/* ------------------------------------------------------------------------- */

static char *dv_to_string_expr(const dval_t *f)
{
    sbuf_t b;
    sbuf_init(&b);

    autoname_table_t vnames;
    autoname_init(&vnames);
    assign_unnamed_vars_dfs((dval_t *)f, &vnames);

    dval_t *g = dv_simplify((dval_t *)f);

    sbuf_putc(&b, '{');
    sbuf_putc(&b, ' ');
    emit_expr(g, &b, PREC_LOWEST);
    sbuf_putc(&b, ' ');

    varlist_t vl;
    varlist_init(&vl);
    find_vars_dfs(g, &vl);

    varlist_t cl;
    varlist_init(&cl);
    find_named_consts_dfs(g, &cl);

    if (vl.count > 0 || cl.count > 0) {
        sbuf_putc(&b, '|');
        sbuf_putc(&b, ' ');

        for (size_t i = 0; i < vl.count; ++i) {
            dval_t *v = vl.vars[i];
            const char *vname = (v->name && *v->name) ? v->name : "x";

            char valbuf[64];
            qf_to_string_simple(v->c, valbuf, sizeof(valbuf));

            emit_name(&b, vname);
            sbuf_puts(&b, " = ");
            sbuf_puts(&b, valbuf);

            if (i + 1 < vl.count)
                sbuf_puts(&b, ", ");
        }

        /* named constants after ';' (or directly if no variables) */
        if (cl.count > 0) {
            if (vl.count > 0)
                sbuf_puts(&b, "; ");
            for (size_t i = 0; i < cl.count; ++i) {
                dval_t *c = cl.vars[i];
                char valbuf[64];
                qf_to_string_simple(c->c, valbuf, sizeof(valbuf));
                emit_name(&b, c->name);
                sbuf_puts(&b, " = ");
                sbuf_puts(&b, valbuf);
                if (i + 1 < cl.count)
                    sbuf_puts(&b, ", ");
            }
        }

        sbuf_putc(&b, ' ');
    }
    sbuf_putc(&b, '}');

    char *out = xstrdup(b.data);
    sbuf_free(&b);
    free(vl.vars);
    free(cl.vars);
    autoname_restore(&vnames);
    dv_free(g);
    return out;
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
    if (!dv) {
        char *s = (char *)xmalloc(5);
        strcpy(s, "NULL");
        return s;
    }

    char *out = (style == style_FUNCTION)
        ? dv_to_string_function(dv)
        : dv_to_string_expr(dv);

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
