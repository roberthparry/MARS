/* dval_tostring.c - symbolic/string conversion for dval_t
 *
 * Responsibilities:
 *   - precedence and parentheses
 *   - superscripts for powers (expression style)
 *   - function vs expression style
 *   - { expr | x = value } wrapper
 *
 * All algebraic simplification (flattening, factoring, ordering, etc.)
 * is done in dv_simplify.c.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "qfloat.h"
#include "dval_internal.h"
#include "dval.h"

/* ------------------------------------------------------------------------- */
/* Small helpers                                                             */
/* ------------------------------------------------------------------------- */

static void *xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p) {
        fprintf(stderr, "dv_to_string: out of memory\n");
        abort();
    }
    return p;
}

static char *xstrdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)xmalloc(n);
    memcpy(p, s, n);
    return p;
}

/* ------------------------------------------------------------------------- */
/* Growable string buffer                                                    */
/* ------------------------------------------------------------------------- */

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} sbuf_t;

static void sbuf_init(sbuf_t *b)
{
    b->cap  = 128;
    b->len  = 0;
    b->data = (char *)xmalloc(b->cap);
    b->data[0] = '\0';
}

static void sbuf_free(sbuf_t *b)
{
    free(b->data);
    b->data = NULL;
    b->len  = 0;
    b->cap  = 0;
}

static void sbuf_reserve(sbuf_t *b, size_t extra)
{
    if (b->len + extra + 1 <= b->cap)
        return;
    size_t ncap = b->cap * 2;
    while (ncap < b->len + extra + 1)
        ncap *= 2;
    char *ndata = (char *)xmalloc(ncap);
    memcpy(ndata, b->data, b->len + 1);
    free(b->data);
    b->data = ndata;
    b->cap  = ncap;
}

static void sbuf_putc(sbuf_t *b, char c)
{
    sbuf_reserve(b, 1);
    b->data[b->len++] = c;
    b->data[b->len]   = '\0';
}

static void sbuf_puts(sbuf_t *b, const char *s)
{
    if (!s) return;
    size_t n = strlen(s);
    sbuf_reserve(b, n);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

/* ------------------------------------------------------------------------- */
/* UTF‑8 decoding (minimal)                                                  */
/* ------------------------------------------------------------------------- */

static int utf8_decode(const char *s, unsigned int *out)
{
    const unsigned char *p = (const unsigned char *)s;

    if (p[0] < 0x80) {
        *out = p[0];
        return 1;
    }
    if ((p[0] & 0xE0) == 0xC0) {
        *out = ((p[0] & 0x1F) << 6) |
               (p[1] & 0x3F);
        return 2;
    }
    if ((p[0] & 0xF0) == 0xE0) {
        *out = ((p[0] & 0x0F) << 12) |
               ((p[1] & 0x3F) << 6) |
               (p[2] & 0x3F);
        return 3;
    }
    return -1;
}

/* ------------------------------------------------------------------------- */
/* qfloat formatting                                                         */
/* ------------------------------------------------------------------------- */

static void qf_to_string_simple(qfloat v, char *buf, size_t n)
{
    qf_sprintf(buf, n, "%q", v);
}

/* ------------------------------------------------------------------------- */
/* Name classification                                                       */
/* ------------------------------------------------------------------------- */

static int is_unicode_letter(unsigned int c)
{
    if ((c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z'))
        return 1;

    if (c >= 0x0391 && c <= 0x03A9)
        return 1;

    if (c >= 0x03B1 && c <= 0x03C9)
        return 1;

    return 0;
}

static int is_simple_name(const char *name)
{
    if (!name || !*name)
        return 0;

    unsigned int c;
    int len = utf8_decode(name, &c);
    if (len <= 0)
        return 0;

    if (name[len] == '\0' && is_unicode_letter(c))
        return 1;

    if (!is_unicode_letter(c))
        return 0;

    for (const unsigned char *p = (const unsigned char *)name + len; *p; ++p)
        if (!(isalnum(*p) || *p == '_'))
            return 0;

    return 1;
}

static void emit_name(sbuf_t *b, const char *name)
{
    if (!name || !*name) {
        sbuf_puts(b, "x");
        return;
    }
    if (is_simple_name(name)) {
        sbuf_puts(b, name);
    } else {
        sbuf_putc(b, '[');
        sbuf_puts(b, name);
        sbuf_putc(b, ']');
    }
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
    if (f->ops == &ops_const) {
        if (f->name && *f->name) {
            emit_name(b, f->name);
        } else {
            char buf[64];
            qf_to_string_simple(f->c, buf, sizeof(buf));
            sbuf_puts(b, buf);
        }
    } else if (f->ops == &ops_var) {
        emit_name(b, f->name ? f->name : "x");
    } else {
        char buf[64];
        qf_to_string_simple(dv_get_val(f), buf, sizeof(buf));
        sbuf_puts(b, buf);
    }
}

static int is_single_char_name(const dval_t *f)
{
    if (!f || !f->name) return 0;
    unsigned int cp;
    int len = utf8_decode(f->name, &cp);
    return (len > 0 && f->name[len] == '\0');
}

/* -------------------------------------------------------------
   Helper: atomic factors for implicit multiplication (EXPR mode)
   ------------------------------------------------------------- */
static int is_atomic_for_mul(const dval_t *f)
{
    if (!f) return 0;

    if (f->ops == &ops_const)
        return 1;

    if (f->ops == &ops_var && is_single_char_name(f))
        return 1;

    if (f->ops == &ops_pow_d &&
        f->a && f->a->ops == &ops_var &&
        is_single_char_name(f->a))
        return 1;

    return 0;
}

/* -------------------------------------------------------------
   Factor classification / flattening / ordering
   ------------------------------------------------------------- */

static void flatten_mul(dval_t *f, dval_t **buf, int *count, int max)
{
    if (!f || *count >= max) return;

    if (f->ops == &ops_mul) {
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
    if (f->ops == &ops_neg) f = f->a;

    if (f->ops == &ops_const) {
        if (!f->name || !*f->name) return 0;
        /* Greek letters are UTF-8 multi-byte; first byte >= 0x80 */
        return ((unsigned char)f->name[0] >= 0x80) ? 1 : 2;
    }

    if (f->ops == &ops_var)
        return 3;

    if (f->ops == &ops_pow_d && f->a->ops == &ops_var)
        return 3;

    return 4;
}

/* DFS to find the name of the first variable in an expression. */
static const char *first_var_name(const dval_t *f)
{
    if (!f) return "";
    if (f->ops == &ops_var) return f->name ? f->name : "";
    const char *a = first_var_name(f->a);
    if (*a) return a;
    return first_var_name(f->b);
}

/* Counts levels of function *nesting* (not tree depth).
 * pow_d and neg are transparent — cos²(x) has the same nesting depth as cos(x).
 * This makes cos²(x) (depth 1) sort before exp(sin(x)) (depth 2). */
static int factor_depth(const dval_t *f)
{
    if (!f || f->ops == &ops_const || f->ops == &ops_var) return 0;
    if (f->ops == &ops_neg || f->ops == &ops_pow_d) return factor_depth(f->a);
    if (f->ops->arity == DV_OP_UNARY) return 1 + factor_depth(f->a);
    if (f->ops->arity == DV_OP_BINARY) {
        int da = factor_depth(f->a), db = factor_depth(f->b);
        return 1 + (da > db ? da : db);
    }
    return 0;
}

static const char *factor_sort_name(const dval_t *f)
{
    if (f->ops == &ops_neg) f = f->a;

    if (f->ops == &ops_const)
        return (f->name && *f->name) ? f->name : "";

    if (f->ops == &ops_var)
        return f->name ? f->name : "";

    if (f->ops == &ops_pow_d && f->a->ops == &ops_var)
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

static void emit_expr(const dval_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) { sbuf_puts(b, "0"); return; }

    /* Atoms */
    if (f->ops == &ops_const || f->ops == &ops_var) {
        emit_atom((dval_t *)f, b);
        return;
    }

    /* Unary ops */
    if (f->ops->arity == DV_OP_UNARY) {
        int need = PREC_UNARY < parent_prec;
        if (need) sbuf_putc(b, '(');

        sbuf_puts(b, f->ops->name);
        sbuf_putc(b, '(');
        emit_expr(f->a, b, 0);
        sbuf_putc(b, ')');

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Power */
    if (f->ops == &ops_pow_d) {
        int need = PREC_POW < parent_prec;
        if (need) sbuf_putc(b, '(');

        double ed = qf_to_double(f->c);
        long   ei = (long)ed;

        /* For unary functions raised to a power, write func²(arg)
         * rather than func(arg)² so the exponent binds to the function name. */
        if (f->a->ops->arity == DV_OP_UNARY) {
            dval_t *inner = f->a;
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
    if (f->ops == &ops_mul) {
        int need = PREC_MUL < parent_prec;
        if (need) sbuf_putc(b, '(');

        dval_t *fac[64];
        int n = 0;
        flatten_mul((dval_t *)f, fac, &n, 64);
        sort_factors(fac, n);

        int sign = 1;
        for (int i = 0; i < n; i++) {
            if (fac[i]->ops == &ops_const &&
                qf_to_double(fac[i]->c) == -1.0)
            {
                sign = -sign;
                for (int j = i; j < n - 1; j++)
                    fac[j] = fac[j + 1];
                n--;
                i--;
            }
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
            emit_expr(fac[i], b, PREC_MUL);
        }

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Addition/subtraction with a + -b → a - b and a - -b → a + b */
    if (f->ops == &ops_add || f->ops == &ops_sub) {
        int need = PREC_ADD < parent_prec;
        if (need) sbuf_putc(b, '(');

        emit_expr(f->a, b, PREC_ADD);

        /* Detect if right child is syntactically negative */
        bool neg = false;

        if (f->b->ops == &ops_const) {
            if (qf_to_double(f->b->c) < 0)
                neg = true;
        }
        else if (f->b->ops == &ops_mul) {
            dval_t *fac[64];
            int n = 0;
            flatten_mul((dval_t *)f->b, fac, &n, 64);
            for (int i = 0; i < n; i++) {
                if (fac[i]->ops == &ops_const &&
                    qf_to_double(fac[i]->c) == -1.0)
                {
                    neg = true;
                    break;
                }
            }
        }

        /* Emit flipped operator if needed */
        if (f->ops == &ops_add) {
            sbuf_puts(b, neg ? " - " : " + ");
        } else { /* subtraction */
            sbuf_puts(b, neg ? " + " : " - ");
        }

        /* Emit |b| (absolute value of right operand) */
        if (neg && f->b->ops == &ops_const) {
            dval_t tmp = *f->b;
            tmp.c = qf_neg(tmp.c);
            emit_expr(&tmp, b, PREC_ADD);
        }
        else if (neg && f->b->ops == &ops_mul) {
            /* Re-emit product without the -1 factor and without a leading '-' */
            dval_t *fac[64];
            int n = 0;
            flatten_mul((dval_t *)f->b, fac, &n, 64);
            sort_factors(fac, n);

            /* strip -1 factors but ignore resulting sign (we already used it) */
            for (int i = 0; i < n; i++) {
                if (fac[i]->ops == &ops_const &&
                    qf_to_double(fac[i]->c) == -1.0)
                {
                    for (int j = i; j < n - 1; j++)
                        fac[j] = fac[j + 1];
                    n--;
                    i--;
                }
            }

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
                emit_expr(fac[i], b, PREC_MUL);
            }
        }
        else {
            emit_expr(f->b, b, PREC_ADD);
        }

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

    if (f->ops == &ops_const || f->ops == &ops_var) {
        emit_atom((dval_t *)f, b);
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

    if (f->ops == &ops_pow_d) {
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

    if (f->ops == &ops_mul) {
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

    if (f->ops == &ops_add || f->ops == &ops_sub) {
        int need = PREC_ADD < parent_prec;
        if (need) sbuf_putc(b, '(');

        emit_func(f->a, b, PREC_ADD);

        if (f->ops == &ops_add)
            sbuf_puts(b, " + ");
        else
            sbuf_puts(b, " - ");

        emit_func(f->b, b, PREC_ADD);

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

    emit_atom((dval_t *)f, b);
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

    if (f->ops == &ops_var) {
        varlist_add(vl, f);
        return;
    }

    if (f->ops == &ops_const) return;

    find_vars_dfs(f->a, vl);
    find_vars_dfs(f->b, vl);
}

static void find_named_consts_dfs(dval_t *f, varlist_t *cl)
{
    if (!f) return;

    if (f->ops == &ops_const) {
        if (f->name && *f->name)
            varlist_add(cl, f);
        return;
    }

    if (f->ops == &ops_var) return;

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

        emit_name(&b, vname);
        sbuf_puts(&b, " = ");

        char valbuf[64];
        qf_to_string_simple(v->c, valbuf, sizeof(valbuf));
        sbuf_puts(&b, valbuf);
        sbuf_putc(&b, '\n');
    }

    /* Emit named constant bindings */
    for (size_t i = 0; i < cl.count; ++i) {
        dval_t *c = cl.vars[i];
        emit_name(&b, c->name);
        sbuf_puts(&b, " = ");

        char valbuf[64];
        qf_to_string_simple(c->c, valbuf, sizeof(valbuf));
        sbuf_puts(&b, valbuf);
        sbuf_putc(&b, '\n');
    }

    /* Pure variable */
    if (g->ops == &ops_var) {
        const char *vname = (g->name && *g->name) ? g->name : "x";

        sbuf_puts(&b, "return ");
        emit_name(&b, vname);

        char *out = xstrdup(b.data);
        sbuf_free(&b);
        free(vl.vars);
        free(cl.vars);
        dv_free(g);
        return out;
    }

    /* Pure constant */
    if (g->ops == &ops_const) {
        const char *cname = (g->name && *g->name) ? g->name : "c";

        emit_name(&b, cname);
        sbuf_puts(&b, " = ");

        char valbuf[64];
        qf_to_string_simple(g->c, valbuf, sizeof(valbuf));
        sbuf_puts(&b, valbuf);
        sbuf_putc(&b, '\n');

        sbuf_puts(&b, "return ");
        emit_name(&b, cname);

        char *out = xstrdup(b.data);
        sbuf_free(&b);
        free(vl.vars);
        free(cl.vars);
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
        emit_name(&b, vname);
    }
    for (size_t i = 0; i < cl.count; ++i) {
        if (vl.count > 0 || i > 0) sbuf_putc(&b, ',');
        emit_name(&b, cl.vars[i]->name);
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
        emit_name(&b, vname);
    }
    for (size_t i = 0; i < cl.count; ++i) {
        if (vl.count > 0 || i > 0) sbuf_putc(&b, ',');
        emit_name(&b, cl.vars[i]->name);
    }
    sbuf_puts(&b, ")");

    char *out = xstrdup(b.data);
    sbuf_free(&b);
    free(vl.vars);
    free(cl.vars);
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

    dval_t *g = dv_simplify((dval_t *)f);

    if (g->ops == &ops_const) {
        sbuf_putc(&b, '{');
        sbuf_putc(&b, ' ');

        if (g->name && *g->name) {
            sbuf_putc(&b, '[');
            sbuf_puts(&b, g->name);
            sbuf_putc(&b, ']');
        } else {
            sbuf_puts(&b, "c");
        }

        sbuf_puts(&b, " = ");

        char valbuf[64];
        qf_to_string_simple(g->c, valbuf, sizeof(valbuf));
        sbuf_puts(&b, valbuf);

        sbuf_putc(&b, ' ');
        sbuf_putc(&b, '}');

        char *out = xstrdup(b.data);
        sbuf_free(&b);
        dv_free(g);
        return out;
    }

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
    sbuf_putc(&b, '}');

    char *out = xstrdup(b.data);
    sbuf_free(&b);
    free(vl.vars);
    free(cl.vars);
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

char *dv_to_string(const dval_t *f, style_t style)
{
    if (!f) {
        char *s = (char *)xmalloc(5);
        strcpy(s, "NULL");
        return s;
    }

    char *out = (style == style_FUNCTION)
        ? dv_to_string_function(f)
        : dv_to_string_expr(f);

    strip_trailing_newline(out);
    return out;
}

void dv_print(const dval_t *f)
{
    char *s = dv_to_string(f, style_EXPRESSION);
    fputs(s, stdout);
    fputc('\n', stdout);
    free(s);
}
