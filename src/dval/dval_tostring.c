/* dval_tostring.c - symbolic/string conversion for dval_t */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "qfloat.h"
#include "dval_internal.h"
#include "dval.h"

/* ------------------------------------------------------------------------- */
/* Memory helpers                                                            */
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

/* not yet used but could be useful later */
static inline void sbuf_printf(sbuf_t *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char tmp[256];
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);

    if (n <= 0) return;

    if ((size_t)n < sizeof(tmp)) {
        sbuf_puts(b, tmp);
        return;
    }

    char *buf = (char *)xmalloc((size_t)n + 1);
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);

    sbuf_puts(b, buf);
    free(buf);
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

    if (c >= 0x0391 && c <= 0x03A9) /* Greek uppercase Α–Ω */
        return 1;

    if (c >= 0x03B1 && c <= 0x03C9) /* Greek lowercase α–ω */
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

    /* single Unicode letter */
    if (name[len] == '\0' && is_unicode_letter(c))
        return 1;

    /* first char must be Unicode letter */
    if (!is_unicode_letter(c))
        return 0;

    /* remaining chars: ASCII alnum or '_' */
    for (const unsigned char *p = (const unsigned char *)name + len; *p; ++p) {
        if (!(isalnum(*p) || *p == '_'))
            return 0;
    }

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

static void emit_superscript_string(sbuf_t *b, const char *s)
{
    for (; *s; ++s) {
        unsigned char c = *s;
        if (c >= '0' && c <= '9') {
            sbuf_puts(b, sup_digits[c - '0']);
        } else if (c == '-') {
            sbuf_puts(b, "⁻");
        } else if (c == '+') {
            sbuf_puts(b, "⁺");
        } else if (c == '.') {
            sbuf_puts(b, "⋅");
        } else {
            sbuf_putc(b, c);
        }
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
            emit_name(b, f->name);   /* named constants: π, etc. */
        } else {
            char buf[64];
            qf_to_string_simple(f->c, buf, sizeof(buf));
            sbuf_puts(b, buf);
        }
        return;
    } else if (f->ops == &ops_var) {
        emit_name(b, f->name ? f->name : "x");
        return;
    } else {
        char buf[64];
        qf_to_string_simple(dv_get_val(f), buf, sizeof(buf));
        sbuf_puts(b, buf);
        return;
    }
}

static void emit_expr(dval_t *f, sbuf_t *b, prec_t parent_prec, style_t style)
{
    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    /* Atoms */
    if (f->ops == &ops_const || f->ops == &ops_var) {
        emit_atom(f, b);
        return;
    }

    /* Unary minus */
    if (f->ops == &ops_neg) {
        prec_t myp = PREC_UNARY;
        int need_paren = (myp < parent_prec);
        if (need_paren) sbuf_putc(b, '(');
        sbuf_putc(b, '-');
        emit_expr(f->a, b, PREC_UNARY, style);
        if (need_paren) sbuf_putc(b, ')');
        return;
    }

    /* Unary functions */
    if (f->ops == &ops_sin || f->ops == &ops_cos || f->ops == &ops_tan ||
        f->ops == &ops_sinh || f->ops == &ops_cosh || f->ops == &ops_tanh ||
        f->ops == &ops_asin || f->ops == &ops_acos || f->ops == &ops_atan ||
        f->ops == &ops_asinh || f->ops == &ops_acosh || f->ops == &ops_atanh ||
        f->ops == &ops_exp || f->ops == &ops_log || f->ops == &ops_sqrt) {

        const char *fname = NULL;

        if      (f->ops == &ops_sin)   fname = "sin";
        else if (f->ops == &ops_cos)   fname = "cos";
        else if (f->ops == &ops_tan)   fname = "tan";
        else if (f->ops == &ops_sinh)  fname = "sinh";
        else if (f->ops == &ops_cosh)  fname = "cosh";
        else if (f->ops == &ops_tanh)  fname = "tanh";
        else if (f->ops == &ops_asin)  fname = "asin";
        else if (f->ops == &ops_acos)  fname = "acos";
        else if (f->ops == &ops_atan)  fname = "atan";
        else if (f->ops == &ops_asinh) fname = "asinh";
        else if (f->ops == &ops_acosh) fname = "acosh";
        else if (f->ops == &ops_atanh) fname = "atanh";
        else if (f->ops == &ops_exp)   fname = "exp";
        else if (f->ops == &ops_log)   fname = "log";
        else if (f->ops == &ops_sqrt)  fname = "sqrt";

        prec_t myp = PREC_UNARY;
        int need_paren = (myp < parent_prec);
        if (need_paren) sbuf_putc(b, '(');

        sbuf_puts(b, fname);

        if (style == style_FUNCTION) {
            sbuf_putc(b, '(');
            emit_expr(f->a, b, PREC_LOWEST, style);
            sbuf_putc(b, ')');
        } else {
            if (f->a && f->a->ops == &ops_var) {
                sbuf_putc(b, ' ');
                emit_expr(f->a, b, PREC_UNARY, style);
            } else {
                sbuf_putc(b, '(');
                emit_expr(f->a, b, PREC_LOWEST, style);
                sbuf_putc(b, ')');
            }
        }

        if (need_paren) sbuf_putc(b, ')');
        return;
    }

    /* Power with constant exponent */
    if (f->ops == &ops_pow_d) {
        prec_t myp = PREC_POW;
        int need_paren = (myp < parent_prec);
        if (need_paren) sbuf_putc(b, '(');

        emit_expr(f->a, b, PREC_POW, style);

        double ed = qf_to_double(f->c);
        long   ei = (long)ed;

        if (style == style_EXPRESSION) {
            if (ed == (double)ei) {
                emit_superscript_int(b, ei);
            } else {
                char buf[64];
                qf_to_string_simple(f->c, buf, sizeof(buf));
                emit_superscript_string(b, buf);
            }
        } else {
            sbuf_putc(b, '^');
            if (ed == (double)ei) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%ld", ei);
                sbuf_puts(b, buf);
            } else {
                char buf[64];
                qf_to_string_simple(f->c, buf, sizeof(buf));
                sbuf_puts(b, buf);
            }
        }

        if (need_paren) sbuf_putc(b, ')');
        return;
    }

    /* Addition / subtraction */
    if (f->ops == &ops_add || f->ops == &ops_sub) {
        prec_t myp = PREC_ADD;
        int need_paren = (myp < parent_prec);
        if (need_paren) sbuf_putc(b, '(');

        emit_expr(f->a, b, PREC_ADD, style);
        sbuf_putc(b, ' ');
        sbuf_putc(b, (f->ops == &ops_add) ? '+' : '-');
        sbuf_putc(b, ' ');
        emit_expr(f->b, b, PREC_ADD, style);

        if (need_paren) sbuf_putc(b, ')');
        return;
    }

    /* Multiplication / division */
    if (f->ops == &ops_mul || f->ops == &ops_div) {
        prec_t myp = PREC_MUL;
        int need_paren = (myp < parent_prec);

        dval_t *lhs = f->a;
        dval_t *rhs = f->b;

        /* FUNCTION: constant-first */
        if (style == style_FUNCTION && f->ops == &ops_mul) {
            int lhs_const = (lhs && lhs->ops == &ops_const);
            int rhs_const = (rhs && rhs->ops == &ops_const);

            if (!lhs_const && rhs_const) {
                dval_t *tmp = lhs;
                lhs = rhs;
                rhs = tmp;
            }
        }

        if (need_paren) sbuf_putc(b, '(');

        /* EXPRESSION STYLE MULTIPLICATION */
        if (style == style_EXPRESSION && f->ops == &ops_mul) {

            int lhs_const = (lhs && lhs->ops == &ops_const);
            int rhs_const = (rhs && rhs->ops == &ops_const);

            int lhs_var   = (lhs && lhs->ops == &ops_var);
            int rhs_var   = (rhs && rhs->ops == &ops_var);

            /* Reorder: constant first */
            if (!lhs_const && rhs_const) {
                dval_t *tmp = lhs;
                lhs = rhs;
                rhs = tmp;

                int tmpc = lhs_const;
                lhs_const = rhs_const;
                rhs_const = tmpc;

                int tmpv = lhs_var;
                lhs_var = rhs_var;
                rhs_var = tmpv;
            }

            /* Case 1: constant * variable → 7α */
            if (lhs_const && rhs_var) {
                emit_expr(lhs, b, PREC_MUL, style);
                emit_expr(rhs, b, PREC_MUL, style);
                if (need_paren) sbuf_putc(b, ')');
                return;
            }

            /* Case 2: variable * variable → αβ */
            if (lhs_var && rhs_var) {
                emit_expr(lhs, b, PREC_MUL, style);
                emit_expr(rhs, b, PREC_MUL, style);
                if (need_paren) sbuf_putc(b, ')');
                return;
            }

            /* Case 3: constant * non-atom → 7(x+1) */
            if (lhs_const) {
                emit_expr(lhs, b, PREC_MUL, style);
                sbuf_putc(b, ' ');
                emit_expr(rhs, b, PREC_MUL, style);
                if (need_paren) sbuf_putc(b, ')');
                return;
            }

            /* Case 4: variable * non-atom → α(x+1) */
            if (lhs_var) {
                emit_expr(lhs, b, PREC_MUL, style);
                sbuf_putc(b, ' ');
                emit_expr(rhs, b, PREC_MUL, style);
                if (need_paren) sbuf_putc(b, ')');
                return;
            }

            /* Case 5: both composite → (x+1)(y+2) */
            emit_expr(lhs, b, PREC_MUL, style);
            sbuf_putc(b, ' ');
            emit_expr(rhs, b, PREC_MUL, style);

            if (need_paren) sbuf_putc(b, ')');
            return;
        }

        /* FUNCTION STYLE MULTIPLICATION */
        emit_expr(lhs, b, PREC_MUL, style);
        sbuf_putc(b, (f->ops == &ops_mul) ? '*' : '/');
        emit_expr(rhs, b, PREC_MUL, style);

        if (need_paren) sbuf_putc(b, ')');
        return;
    }

    /* Fallback */
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

    varlist_t vl;
    varlist_init(&vl);
    find_vars_dfs((dval_t *)f, &vl);

    for (size_t i = 0; i < vl.count; ++i) {
        dval_t *v = vl.vars[i];
        emit_name(&b, v->name ? v->name : "x");
        sbuf_puts(&b, " = ");
        char valbuf[64];
        qf_to_string_simple(v->c, valbuf, sizeof(valbuf));
        sbuf_puts(&b, valbuf);
        sbuf_putc(&b, '\n');
    }

    const char *fname = (f->name && *f->name) ? f->name : "expr";
    const char *argname = (vl.count > 0 && vl.vars[0]->name) ? vl.vars[0]->name : "x";

    emit_name(&b, fname);
    sbuf_putc(&b, '(');
    emit_name(&b, argname);
    sbuf_puts(&b, ") = ");
    emit_expr((dval_t *)f, &b, PREC_LOWEST, style_FUNCTION);
    sbuf_putc(&b, '\n');

    sbuf_puts(&b, "return ");
    emit_name(&b, fname);
    sbuf_putc(&b, '(');
    emit_name(&b, argname);
    sbuf_puts(&b, ")\n");

    char *out = xstrdup(b.data);
    sbuf_free(&b);
    free(vl.vars);
    return out;
}

/* ------------------------------------------------------------------------- */
/* Expression-style printing                                                 */
/* ------------------------------------------------------------------------- */

static char *dv_to_string_expr(const dval_t *f)
{
    sbuf_t b;
    sbuf_init(&b);

    /* Special case: the entire expression is a constant */
    if (f->ops == &ops_const) {

        sbuf_putc(&b, '{');
        sbuf_putc(&b, ' ');

        if (f->name && *f->name) {
            /* Named constant: print in brackets, preserving spaces */
            sbuf_putc(&b, '[');
            sbuf_puts(&b, f->name);
            sbuf_putc(&b, ']');
        } else {
            /* Unnamed constant: use generic c */
            sbuf_puts(&b, "c");
        }

        sbuf_puts(&b, " = ");

        char valbuf[64];
        qf_to_string_simple(f->c, valbuf, sizeof(valbuf));
        sbuf_puts(&b, valbuf);

        sbuf_putc(&b, ' ');
        sbuf_putc(&b, '}');

        char *out = xstrdup(b.data);
        sbuf_free(&b);
        return out;
    }

    /* Normal expression case */
    sbuf_putc(&b, '{');
    sbuf_putc(&b, ' ');
    emit_expr((dval_t *)f, &b, PREC_LOWEST, style_EXPRESSION);
    sbuf_putc(&b, ' ');

    varlist_t vl;
    varlist_init(&vl);
    find_vars_dfs((dval_t *)f, &vl);

    dval_t *var = (vl.count > 0) ? vl.vars[0] : NULL;
    const char *vname = (var && var->name && *var->name) ? var->name : "x";

    qfloat v = var ? var->c : dv_get_val(f);
    char valbuf[64];
    qf_to_string_simple(v, valbuf, sizeof(valbuf));

    sbuf_putc(&b, '|');
    sbuf_putc(&b, ' ');
    emit_name(&b, vname);
    sbuf_puts(&b, " = ");
    sbuf_puts(&b, valbuf);
    sbuf_putc(&b, ' ');
    sbuf_putc(&b, '}');

    char *out = xstrdup(b.data);
    sbuf_free(&b);
    free(vl.vars);
    return out;
}

/* ------------------------------------------------------------------------- */
/* Public entry point                                                        */
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
