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
    qf_sprintf(buf, n, "%q", &v);
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

typedef enum {
    FACT_CONST = 0,
    FACT_SINGLE_VAR = 1,
    FACT_MULTI_VAR = 2,
    FACT_VAR_POWER = 3,
    FACT_UNARY_FUNC = 4,
    FACT_OTHER = 5
} factor_kind_t;

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

static int expr_depth(const dval_t *f)
{
    if (!f) return 0;

    if (f->ops == &ops_const || f->ops == &ops_var)
        return 1;

    if (f->ops == &ops_neg)
        return expr_depth(f->a);

    if (f->ops->arity == DV_OP_UNARY)
        return 1 + expr_depth(f->a);

    if (f->ops->arity == DV_OP_BINARY) {
        int da = expr_depth(f->a);
        int db = expr_depth(f->b);
        return 1 + (da > db ? da : db);
    }

    return 1;
}

static int expr_class(const dval_t *f)
{
    if (f->ops == &ops_const || f->ops == &ops_var)
        return 0;

    if (f->ops == &ops_pow_d && f->a->ops == &ops_var)
        return 1;

    if (f->ops->arity == DV_OP_UNARY)
        return 2 + expr_class(f->a);

    if (f->ops->arity == DV_OP_BINARY)
        return 4;

    return 5;
}

static int eff_depth(const dval_t *f)
{
    if (f->ops == &ops_neg)
        return expr_depth(f->a);
    return expr_depth(f);
}

static const char *eff_name(const dval_t *f)
{
    if (f->ops == &ops_neg)
        return f->a->ops->name;
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

    int ca = expr_class(a);
    int cb = expr_class(b);
    if (ca != cb)
        return (ca < cb) ? -1 : 1;

    return strcmp(eff_name(a), eff_name(b));
}

/* ------------------------------------------------------------------------- */
/* EXPRESSION MODE (pretty math)                                             */
/* ------------------------------------------------------------------------- */

static void emit_expr(const dval_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) { sbuf_puts(b, "0"); return; }

    const int prec_add = 1;
    const int prec_mul = 2;
    const int prec_pow = 3;
    const int prec_un  = 4;

    if (f->ops == &ops_const || f->ops == &ops_var) {
        emit_atom((dval_t *)f, b);
        return;
    }

    if (f->ops->arity == DV_OP_UNARY) {
        int need = prec_un < parent_prec;
        if (need) sbuf_putc(b, '(');

        sbuf_puts(b, f->ops->name);
        sbuf_putc(b, '(');
        emit_expr(f->a, b, 0);
        sbuf_putc(b, ')');

        if (need) sbuf_putc(b, ')');
        return;
    }

    if (f->ops == &ops_pow_d) {
        int need = prec_pow < parent_prec;
        if (need) sbuf_putc(b, '(');

        emit_expr(f->a, b, prec_pow);

        double ed = qf_to_double(f->c);
        long   ei = (long)ed;

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

    if (f->ops == &ops_mul) {
        int need = prec_mul < parent_prec;
        if (need) sbuf_putc(b, '(');

        dval_t *fac[64];
        int n = 0;
        flatten_mul((dval_t *)f, fac, &n, 64);
        qsort(fac, n, sizeof(dval_t *), factor_cmp);

        for (int i = 0; i < n; i++) {
            if (i > 0) {
                int left_atomic  = is_atomic_for_mul(fac[i-1]);
                int right_atomic = is_atomic_for_mul(fac[i]);

                if (left_atomic && right_atomic) {
                    /* implicit */
                } else if (!left_atomic && !right_atomic) {
                    sbuf_puts(b, "·");
                } else {
                    sbuf_putc(b, '*');
                }
            }
            emit_expr(fac[i], b, prec_mul);
        }

        if (need) sbuf_putc(b, ')');
        return;
    }

    if (f->ops == &ops_add || f->ops == &ops_sub) {
        int need = prec_add < parent_prec;
        if (need) sbuf_putc(b, '(');

        emit_expr(f->a, b, prec_add);

        if (f->ops == &ops_add)
            sbuf_puts(b, " + ");
        else
            sbuf_puts(b, " - ");

        emit_expr(f->b, b, prec_add);

        if (need) sbuf_putc(b, ')');
        return;
    }

    emit_atom((dval_t *)f, b);
}

/* ------------------------------------------------------------------------- */
/* FUNCTION MODE (calculator-style)                                          */
/* ------------------------------------------------------------------------- */

static void emit_func(const dval_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) { sbuf_puts(b, "0"); return; }

    const int prec_add = 1;
    const int prec_mul = 2;
    const int prec_pow = 3;
    const int prec_un  = 4;

    if (f->ops == &ops_const || f->ops == &ops_var) {
        emit_atom((dval_t *)f, b);
        return;
    }

    if (f->ops->arity == DV_OP_UNARY) {
        int need = prec_un < parent_prec;
        if (need) sbuf_putc(b, '(');

        sbuf_puts(b, f->ops->name);
        sbuf_putc(b, '(');
        emit_func(f->a, b, 0);
        sbuf_putc(b, ')');

        if (need) sbuf_putc(b, ')');
        return;
    }

    if (f->ops == &ops_pow_d) {
        int need = prec_pow < parent_prec;
        if (need) sbuf_putc(b, '(');

        emit_func(f->a, b, prec_pow);

        sbuf_putc(b, '^');
        char buf[64];
        qf_to_string_simple(f->c, buf, sizeof(buf));
        sbuf_puts(b, buf);

        if (need) sbuf_putc(b, ')');
        return;
    }

    if (f->ops == &ops_mul) {
        int need = prec_mul < parent_prec;
        if (need) sbuf_putc(b, '(');

        dval_t *fac[64];
        int n = 0;
        flatten_mul((dval_t *)f, fac, &n, 64);
        qsort(fac, n, sizeof(dval_t *), factor_cmp);

        for (int i = 0; i < n; i++) {
            if (i > 0)
                sbuf_putc(b, '*');
            emit_func(fac[i], b, prec_mul);
        }

        if (need) sbuf_putc(b, ')');
        return;
    }

    if (f->ops == &ops_add || f->ops == &ops_sub) {
        int need = prec_add < parent_prec;
        if (need) sbuf_putc(b, '(');

        emit_func(f->a, b, prec_add);

        if (f->ops == &ops_add)
            sbuf_puts(b, " + ");
        else
            sbuf_puts(b, " - ");

        emit_func(f->b, b, prec_add);

        if (need) sbuf_putc(b, ')');
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

    /* Simplify first */
    dval_t *g = dv_simplify((dval_t *)f);

    /* Discover variables */
    varlist_t vl;
    varlist_init(&vl);
    find_vars_dfs(g, &vl);

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

    /* Pure variable */
    if (g->ops == &ops_var) {
        const char *vname = (g->name && *g->name) ? g->name : "x";

        sbuf_puts(&b, "return ");
        emit_name(&b, vname);

        /* Strip trailing whitespace */
        while (b.len > 0 &&
               (b.data[b.len - 1] == ' ' ||
                b.data[b.len - 1] == '\n' ||
                b.data[b.len - 1] == '\t'))
        {
            b.data[--b.len] = '\0';
        }

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

        /* Strip trailing whitespace */
        while (b.len > 0 &&
               (b.data[b.len - 1] == ' ' ||
                b.data[b.len - 1] == '\n' ||
                b.data[b.len - 1] == '\t'))
        {
            b.data[--b.len] = '\0';
        }

        char *out = xstrdup(b.data);
        sbuf_free(&b);
        free(vl.vars);
        dv_free(g);
        return out;
    }

    /* General expression */
    const char *fname = (g->name && *g->name) ? g->name : "expr";

    /* expr(x,y,z) = ... */
    sbuf_puts(&b, fname);
    sbuf_putc(&b, '(');
    for (size_t i = 0; i < vl.count; ++i) {
        if (i > 0) sbuf_putc(&b, ',');
        const char *vname = (vl.vars[i]->name && *vl.vars[i]->name)
                            ? vl.vars[i]->name : "x";
        emit_name(&b, vname);
    }
    sbuf_puts(&b, ") = ");
    emit_func(g, &b, PREC_LOWEST);
    sbuf_putc(&b, '\n');

    /* return expr(x,y,z) */
    sbuf_puts(&b, "return ");
    sbuf_puts(&b, fname);
    sbuf_putc(&b, '(');
    for (size_t i = 0; i < vl.count; ++i) {
        if (i > 0) sbuf_putc(&b, ',');
        const char *vname = (vl.vars[i]->name && *vl.vars[i]->name)
                            ? vl.vars[i]->name : "x";
        emit_name(&b, vname);
    }
    sbuf_puts(&b, ")");   /* ← NO NEWLINE */

    /* Strip trailing whitespace (Option B) */
    while (b.len > 0 &&
           (b.data[b.len - 1] == ' ' ||
            b.data[b.len - 1] == '\n' ||
            b.data[b.len - 1] == '\t'))
    {
        b.data[--b.len] = '\0';
    }

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
