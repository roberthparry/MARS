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
    double d = qf_to_double(v);
    snprintf(buf, n, "%.17g", d);
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
/* Expression emitter                                                        */
/* ------------------------------------------------------------------------- */

static void emit_expr(dval_t *f, sbuf_t *b, prec_t parent_prec, style_t style);

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
   Helper: atomic factors for implicit multiplication
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
   Factor classification for Option B ordering
   ------------------------------------------------------------- */
typedef enum {
    FACT_CONST = 0,
    FACT_SINGLE_VAR = 1,
    FACT_MULTI_VAR = 2,
    FACT_VAR_POWER = 3,
    FACT_UNARY_FUNC = 4,
    FACT_OTHER = 5
} factor_kind_t;

/* -------------------------------------------------------------
   Flatten multiplication chain (shape-independent)
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

/* -------------------------------------------------------------
   MAIN PRINTER
   ------------------------------------------------------------- */

/* -------------------------------------------------------------
   Factor comparator (Option B + unary-depth ordering)
   ------------------------------------------------------------- */

static int expr_depth(const dval_t *f)
{
    if (!f) return 0;

    /* Atoms: const, var */
    if (f->ops == &ops_const || f->ops == &ops_var)
        return 1;

    /* Negation: do NOT increase depth, just look through it */
    if (f->ops == &ops_neg)
        return expr_depth(f->a);

    /* Unary ops (sin, cos, exp, etc.) */
    if (f->ops->arity == DV_OP_UNARY)
        return 1 + expr_depth(f->a);

    /* Binary ops (+, -, *, /, pow, etc.) */
    if (f->ops->arity == DV_OP_BINARY) {
        int da = expr_depth(f->a);
        int db = expr_depth(f->b);
        return 1 + (da > db ? da : db);
    }

    /* Fallback */
    return 1;
}

static int expr_class(const dval_t *f)
{
    /* Atoms */
    if (f->ops == &ops_const || f->ops == &ops_var)
        return 0;

    /* Simple power: x^n where base is var and exponent is const */
    if (f->ops == &ops_pow_d && f->a->ops == &ops_var)
        return 1;

    /* Unary functions */
    if (f->ops->arity == DV_OP_UNARY)
        return 2 + expr_class(f->a);  /* nested unary increases class */

    /* Binary expressions */
    if (f->ops->arity == DV_OP_BINARY)
        return 4;

    return 5; /* fallback */
}

static int eff_depth(const dval_t *f)
{
    if (f->ops == &ops_neg)
        return expr_depth(f->a);   /* ignore negation */
    return expr_depth(f);
}

static const char *eff_name(const dval_t *f)
{
    if (f->ops == &ops_neg)
        return f->a->ops->name;    /* ignore negation */
    return f->ops->name;
}

static int factor_cmp(const void *pa, const void *pb)
{
    const dval_t *a = *(const dval_t * const *)pa;
    const dval_t *b = *(const dval_t * const *)pb;

    int da = eff_depth(a);
    int db = eff_depth(b);

    if (da != db)
        return (da < db) ? -1 : 1;

    /* Secondary key: expression class */
    int ca = expr_class(a);
    int cb = expr_class(b);
    if (ca != cb)
        return (ca < cb) ? -1 : 1;

    /* Tertiary key: alphabetical */
    return strcmp(eff_name(a), eff_name(b));
}

static void emit_expr(dval_t *f, sbuf_t *b, prec_t parent_prec, style_t style)
{
    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    /* ============================================================
       1.  MULTIPLICATION / DIVISION  (must come BEFORE neg/unary)
       ============================================================ */
    if (f->ops == &ops_mul || f->ops == &ops_div) {
        prec_t myp = PREC_MUL;
        int need_paren = (myp < parent_prec);
        if (need_paren) sbuf_putc(b, '(');

        /* Division is simple */
        if (f->ops == &ops_div) {
            emit_expr(f->a, b, PREC_MUL, style);
            sbuf_putc(b, '/');
            emit_expr(f->b, b, PREC_MUL, style);
            if (need_paren) sbuf_putc(b, ')');
            return;
        }

        /* Multiplication: flatten */
        dval_t *factors[64];
        int n = 0;
        flatten_mul(f, factors, &n, 64);

        /* Always sort by structural simplicity */
        qsort(factors, n, sizeof(dval_t *), factor_cmp);

        /* Leading -1 constant → print '-' */
        int start = 0;
        if (n > 0 &&
            factors[0]->ops == &ops_const &&
            qf_to_double(factors[0]->c) == -1.0)
        {
            sbuf_putc(b, '-');
            start = 1;
        }

        /* Emit factors */
        for (int i = start; i < n; i++) {
            if (i > start) {
                int left_atomic  = is_atomic_for_mul(factors[i-1]);
                int right_atomic = is_atomic_for_mul(factors[i]);

                /* Decide how to print multiplication between factors[i-1] and factors[i] */
                dval_t *L = factors[i-1];
                dval_t *R = factors[i];

                /* Case 1: implicit multiplication: const · var or const · var^k */
                if (L->ops == &ops_const &&
                    (R->ops == &ops_var || R->ops == &ops_pow_d))
                {
                    /* no symbol */
                }
                /* Case 2: both atomic → implicit multiplication */
                else if (left_atomic && right_atomic) {
                    /* no symbol */
                }
                /* Case 3: both non‑atomic → use centered dot */
                else if (!left_atomic && !right_atomic) {
                    sbuf_puts(b, "·");
                }
                /* Case 4: mixed atomic/non‑atomic → use '*' */
                else {
                    sbuf_putc(b, '*');
                }
            }
            emit_expr(factors[i], b, PREC_MUL, style);
        }

        if (need_paren) sbuf_putc(b, ')');
        return;
    }

    /* ============================================================
       2.  NEGATION
       ============================================================ */
    if (f->ops == &ops_neg) {
        prec_t myp = PREC_UNARY;
        int need_paren = (myp < parent_prec);

        if (need_paren) sbuf_putc(b, '(');
        sbuf_putc(b, '-');
        emit_expr(f->a, b, PREC_MUL, style);
        if (need_paren) sbuf_putc(b, ')');
        return;
    }

    /* ============================================================
       3.  ATOMS
       ============================================================ */
    if (f->ops == &ops_const || f->ops == &ops_var) {
        emit_atom(f, b);
        return;
    }

    /* ============================================================
       4.  UNARY FUNCTIONS
       ============================================================ */
    if (f->ops->arity == DV_OP_UNARY) {
        prec_t myp = PREC_UNARY;
        int need_paren = (myp < parent_prec);

        if (need_paren) sbuf_putc(b, '(');
        sbuf_puts(b, f->ops->name);
        sbuf_putc(b, '(');
        emit_expr(f->a, b, PREC_LOWEST, style);
        sbuf_putc(b, ')');
        if (need_paren) sbuf_putc(b, ')');
        return;
    }

    /* ============================================================
       5.  POWER
       ============================================================ */
    if (f->ops == &ops_pow_d) {
        prec_t myp = PREC_POW;
        int need_paren = (myp < parent_prec);
        dval_t *base = f->a;

        if (need_paren) sbuf_putc(b, '(');

        if (base->ops == &ops_var)
            emit_atom(base, b);
        else if (base->ops->arity == DV_OP_UNARY)
            sbuf_puts(b, base->ops->name);
        else {
            sbuf_putc(b, '(');
            emit_expr(base, b, PREC_LOWEST, style);
            sbuf_putc(b, ')');
        }

        double ed = qf_to_double(f->c);
        long ei = (long)ed;

        if (style == style_EXPRESSION && ed == (double)ei)
            emit_superscript_int(b, ei);
        else {
            sbuf_putc(b, '^');
            char buf[64];
            qf_to_string_simple(f->c, buf, sizeof(buf));
            sbuf_puts(b, buf);
        }

        if (base->ops->arity == DV_OP_UNARY) {
            sbuf_putc(b, '(');
            emit_expr(base->a, b, PREC_LOWEST, style);
            sbuf_putc(b, ')');
        }

        if (need_paren) sbuf_putc(b, ')');
        return;
    }

    /* ============================================================
       6.  ADD / SUB
       ============================================================ */
    if (f->ops == &ops_add || f->ops == &ops_sub) {
        prec_t myp = PREC_ADD;
        int need_paren = (myp < parent_prec);

        if (need_paren) sbuf_putc(b, '(');

        /* print left side */
        emit_expr(f->a, b, PREC_ADD, style);

        dval_t *rhs = f->b;

        /* Case 1: RHS is a negation node */
        if (rhs->ops == &ops_neg) {
            sbuf_puts(b, " - ");
            emit_expr(rhs->a, b, PREC_ADD, style);
        }

        /* Case 2: RHS is a MUL whose first factor is -1 */
        else if (rhs->ops == &ops_mul) {

            dval_t *factors[64];
            int n = 0;
            flatten_mul(rhs, factors, &n, 64);

            /* Check for leading -1 */
            if (n > 0 &&
                factors[0]->ops == &ops_const &&
                qf_to_double(factors[0]->c) == -1.0)
            {
                sbuf_puts(b, " - ");

                /* Sort factors for consistent ordering */
                qsort(factors, n, sizeof(dval_t *), factor_cmp);

                /* Skip the -1 and print the rest */
                for (int i = 1; i < n; i++) {
                    if (i > 1) sbuf_puts(b, "·");
                    emit_expr(factors[i], b, PREC_MUL, style);
                }
            }
            else {
                sbuf_puts(b, " + ");
                emit_expr(rhs, b, PREC_ADD, style);
            }
        }

        /* Case 3: normal addition */
        else {
            sbuf_puts(b, " + ");
            emit_expr(rhs, b, PREC_ADD, style);
        }

        if (need_paren) sbuf_putc(b, ')');
        return;
    }

    /* ============================================================
       7.  FALLBACK
       ============================================================ */
    emit_atom(f, b);
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

    find_vars_dfs(f->a, vl);
    find_vars_dfs(f->b, vl);
}

/* ------------------------------------------------------------------------- */
/* FUNCTION-style printing                                                   */
/* ------------------------------------------------------------------------- */

static char *dv_to_string_function(const dval_t *f)
{
    sbuf_t b;
    sbuf_init(&b);

    /* CRITICAL: simplify first */
    dval_t *g = dv_simplify((dval_t *)f);

    varlist_t vl;
    varlist_init(&vl);
    find_vars_dfs(g, &vl);

    /* Emit variable bindings (x = value) lines */
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

    /* Pure variable */
    if (g->ops == &ops_var) {
        const char *vname = (g->name && *g->name) ? g->name : "x";

        sbuf_puts(&b, "return ");
        emit_name(&b, vname);
        sbuf_putc(&b, '\n');

        char *out = xstrdup(b.data);
        sbuf_free(&b);
        free(vl.vars);
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
        sbuf_putc(&b, '\n');

        char *out = xstrdup(b.data);
        sbuf_free(&b);
        free(vl.vars);
        dv_free(g);
        return out;
    }

    /* General expression */
    const char *fname = (g->name && *g->name) ? g->name : "expr";

    sbuf_puts(&b, fname);
    sbuf_putc(&b, '(');
    for (size_t i = 0; i < vl.count; ++i) {
        if (i > 0) sbuf_putc(&b, ',');
        const char *vname = (vl.vars[i]->name && *vl.vars[i]->name)
                            ? vl.vars[i]->name : "x";
        emit_name(&b, vname);
    }
    sbuf_puts(&b, ") = ");
    emit_expr(g, &b, PREC_LOWEST, style_FUNCTION);
    sbuf_putc(&b, '\n');

    sbuf_puts(&b, "return ");
    sbuf_puts(&b, fname);
    sbuf_putc(&b, '(');
    for (size_t i = 0; i < vl.count; ++i) {
        if (i > 0) sbuf_putc(&b, ',');
        const char *vname = (vl.vars[i]->name && *vl.vars[i]->name)
                            ? vl.vars[i]->name : "x";
        emit_name(&b, vname);
    }
    sbuf_puts(&b, ")\n");

    char *out = xstrdup(b.data);
    sbuf_free(&b);
    free(vl.vars);
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

    /* CRITICAL: simplify first */
    dval_t *g = dv_simplify((dval_t *)f);

    /* Pure constant */
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

    /* General expression */
    sbuf_putc(&b, '{');
    sbuf_putc(&b, ' ');
    emit_expr(g, &b, PREC_LOWEST, style_EXPRESSION);
    sbuf_putc(&b, ' ');

    varlist_t vl;
    varlist_init(&vl);
    find_vars_dfs(g, &vl);

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

    sbuf_putc(&b, ' ');
    sbuf_putc(&b, '}');

    char *out = xstrdup(b.data);
    sbuf_free(&b);
    free(vl.vars);
    dv_free(g);
    return out;
}

/* ------------------------------------------------------------------------- */
/* Public entry points                                                       */
/* ------------------------------------------------------------------------- */

char *dv_to_string(const dval_t *f, style_t style)
{
    if (!f) {
        char *s = (char *)xmalloc(5);
        strcpy(s, "NULL");
        return s;
    }

    if (style == style_FUNCTION)
        return dv_to_string_function(f);
    else
        return dv_to_string_expr(f);
}

void dv_print(const dval_t *f)
{
    char *s = dv_to_string(f, style_EXPRESSION);
    fputs(s, stdout);
    fputc('\n', stdout);
    free(s);
}
