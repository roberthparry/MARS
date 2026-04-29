#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "qcomplex.h"
#include "dval_internal.h"
#include "dval.h"

qcomplex_t dv_qc_real_qf(qfloat_t x)
{
    return qc_make(x, QF_ZERO);
}

qcomplex_t dv_qc_real_d(double x)
{
    return qc_make(qf_from_double(x), QF_ZERO);
}

static struct _dval_t _DV_ZERO_NODE = {
    .ops = &ops_const,
    .a = NULL,
    .b = NULL,
    .c = { { 0.0, 0.0 }, { 0.0, 0.0 } },
    .x = { { 0.0, 0.0 }, { 0.0, 0.0 } },
    .x_valid = 1,
    .epoch = 0,
    .dx_cache = NULL,
    .name = NULL,
    .refcount = INT_MAX,
    .var_id = 0
};

static struct _dval_t _DV_ONE_NODE = {
    .ops = &ops_const,
    .a = NULL,
    .b = NULL,
    .c = { { 1.0, 0.0 }, { 0.0, 0.0 } },
    .x = { { 1.0, 0.0 }, { 0.0, 0.0 } },
    .x_valid = 1,
    .epoch = 0,
    .dx_cache = NULL,
    .name = NULL,
    .refcount = INT_MAX,
    .var_id = 0
};

dval_t *DV_ZERO = &_DV_ZERO_NODE;
dval_t *DV_ONE = &_DV_ONE_NODE;

static uint64_t next_var_id = 1;

static inline void refcount_inc(int *rc)
{
    if (*rc < INT_MAX)
        (*rc)++;
}

static inline int refcount_dec(int *rc)
{
    int prev = *rc;

    if (*rc < INT_MAX)
        (*rc)--;
    return prev;
}

static uint64_t alloc_var_id(void)
{
    return next_var_id++;
}

typedef struct {
    const char *ascii;
    size_t klen;
    const char *lower;
    const char *upper;
} greek_entry_t;

enum { GREEK_HT_SIZE = 30 };

static const greek_entry_t s_greek_names[GREEK_HT_SIZE] = {
    [0]  = { "theta",   5, "θ", "Θ" },
    [1]  = { "psi",     3, "ψ", "Ψ" },
    [2]  = { "chi",     3, "χ", "Χ" },
    [4]  = { "lambda",  6, "λ", "Λ" },
    [5]  = { "delta",   5, "δ", "Δ" },
    [6]  = { "omicron", 8, "ο", "Ο" },
    [8]  = { "iota",    4, "ι", "Ι" },
    [10] = { "mu",      2, "μ", "Μ" },
    [11] = { "pi",      2, "π", "Π" },
    [12] = { "phi",     3, "φ", "Φ" },
    [13] = { "alpha",   5, "α", "Α" },
    [14] = { "zeta",    4, "ζ", "Ζ" },
    [15] = { "tau",     3, "τ", "Τ" },
    [16] = { "rho",     3, "ρ", "Ρ" },
    [17] = { "beta",    4, "β", "Β" },
    [19] = { "nu",      2, "ν", "Ν" },
    [20] = { "kappa",   5, "κ", "Κ" },
    [22] = { "sigma",   5, "σ", "Σ" },
    [23] = { "xi",      2, "ξ", "Ξ" },
    [24] = { "eta",     3, "η", "Η" },
    [25] = { "epsilon", 7, "ε", "Ε" },
    [26] = { "gamma",   5, "γ", "Γ" },
    [27] = { "upsilon", 7, "υ", "Υ" },
    [29] = { "omega",   5, "ω", "Ω" }
};

static unsigned greek_ht_hash(const char *s, size_t n)
{
    unsigned x = 113u;

    for (size_t i = 0; i < n; ++i) {
        x *= 65599u;
        x ^= (unsigned char)(s[i] | 32);
    }

    x ^= (x >> 15);
    x *= 2654435761u;

    return x % GREEK_HT_SIZE;
}

static const greek_entry_t *lookup_greek_name(const char *kw, size_t klen)
{
    unsigned slot = greek_ht_hash(kw, klen);
    const greek_entry_t *entry = &s_greek_names[slot];

    if (!entry->ascii)
        return NULL;
    if (entry->klen == klen && strncasecmp(entry->ascii, kw, klen) == 0)
        return entry;
    return NULL;
}

char *dv_normalize_name(const char *name)
{
    const char *s;
    const char *e;
    size_t len;
    char *t;

    if (!name)
        return NULL;

    s = name;
    while (*s && isspace((unsigned char)*s))
        s++;
    e = name + strlen(name);
    while (e > s && isspace((unsigned char)e[-1]))
        e--;

    len = (size_t)(e - s);
    if (len == 0)
        return NULL;

    t = malloc(len + 1);
    memcpy(t, s, len);
    t[len] = '\0';

    if (t[0] == '@') {
        const char *p = t + 1;
        size_t alias_len = 0;
        const greek_entry_t *entry;

        while (p[alias_len] && isalpha((unsigned char)p[alias_len]))
            alias_len++;

        entry = alias_len ? lookup_greek_name(p, alias_len) : NULL;
        if (entry) {
            int upper = 1;
            const char *g;
            const char *rest = p + alias_len;
            size_t gl;
            size_t rl;
            char *out;

            for (size_t k = 0; k < alias_len; ++k) {
                if (!isupper((unsigned char)p[k]))
                    upper = 0;
            }

            g = upper ? entry->upper : entry->lower;
            gl = strlen(g);
            rl = strlen(rest);
            out = malloc(gl + rl + 1);
            memcpy(out, g, gl);
            memcpy(out + gl, rest, rl);
            out[gl + rl] = '\0';

            free(t);
            t = out;
        }

        size_t n = strlen(t);
        char *clean = malloc(n + 1);
        size_t w = 0;
        for (size_t r = 0; r < n; ++r)
            if (t[r] != '@')
                clean[w++] = t[r];
        clean[w] = '\0';

        free(t);
        t = clean;
    }

    return t;
}

void dv_retain(dval_t *dv)
{
    if (dv)
        refcount_inc(&dv->refcount);
}

static void dv_release(dval_t *dv)
{
    dval_t *a;
    dval_t *b;
    dv_deriv_cache_t *ce;

    if (!dv)
        return;
    if (refcount_dec(&dv->refcount) > 1)
        return;

    a = dv->a;
    b = dv->b;

    ce = dv->dx_cache;
    while (ce) {
        dv_deriv_cache_t *next = ce->next;
        dv_release(ce->dx);
        free(ce);
        ce = next;
    }

    if (dv->name)
        free(dv->name);
    free(dv);

    dv_release(a);
    dv_release(b);
}

void dv_free(dval_t *dv)
{
    dv_release(dv);
}

dval_t *dv_alloc(const dval_ops_t *ops)
{
    dval_t *dv = malloc(sizeof *dv);

    if (!dv)
        abort();

    dv->ops = ops;
    dv->a = NULL;
    dv->b = NULL;
    dv->c = QC_ZERO;
    dv->x = QC_ZERO;
    dv->x_valid = 0;
    dv->epoch = 0;
    dv->dx_cache = NULL;
    dv->name = NULL;
    dv->refcount = 1;
    dv->var_id = 0;

    return dv;
}

dval_t *dv_make_const_qc(qcomplex_t x)
{
    dval_t *dv = dv_alloc(&ops_const);
    dv->c = x;
    dv->x = x;
    dv->x_valid = 1;
    return dv;
}

dval_t *dv_make_var_qc(qcomplex_t x)
{
    dval_t *dv = dv_alloc(&ops_var);
    dv->c = x;
    dv->x = x;
    dv->x_valid = 1;
    dv->var_id = alloc_var_id();
    return dv;
}

dval_t *dv_new_const_d(double x) { return dv_make_const_qc(dv_qc_real_d(x)); }
dval_t *dv_new_const(qfloat_t x) { return dv_make_const_qc(dv_qc_real_qf(x)); }
dval_t *dv_new_const_qc(qcomplex_t x) { return dv_make_const_qc(x); }

dval_t *dv_new_var_d(double x)
{
    return dv_make_var_qc(dv_qc_real_d(x));
}

dval_t *dv_new_var(qfloat_t x)
{
    return dv_make_var_qc(dv_qc_real_qf(x));
}

dval_t *dv_new_var_qc(qcomplex_t x)
{
    return dv_make_var_qc(x);
}

dval_t *dv_new_named_const(qfloat_t x, const char *name)
{
    dval_t *dv = dv_new_const(x);
    dv->name = dv_normalize_name(name);
    return dv;
}

dval_t *dv_new_named_const_qc(qcomplex_t x, const char *name)
{
    dval_t *dv = dv_new_const_qc(x);
    dv->name = dv_normalize_name(name);
    return dv;
}

dval_t *dv_new_named_const_d(double x, const char *name)
{
    return dv_new_named_const(qf_from_double(x), name);
}

dval_t *dv_new_named_var(qfloat_t x, const char *name)
{
    dval_t *dv = dv_new_var(x);
    dv->name = dv_normalize_name(name);
    return dv;
}

dval_t *dv_new_named_var_qc(qcomplex_t x, const char *name)
{
    dval_t *dv = dv_new_var_qc(x);
    dv->name = dv_normalize_name(name);
    return dv;
}

dval_t *dv_new_named_var_d(double x, const char *name)
{
    return dv_new_named_var(qf_from_double(x), name);
}
