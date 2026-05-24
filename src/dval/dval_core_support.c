#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "number.h"
#include "dictionary.h"
#include "dval_internal.h"
#include "dval.h"

static size_t dv_alias_hash(const void *key)
{
    const unsigned char *s = (const unsigned char *)*(const char * const *)key;
    size_t hash = 1469598103934665603ull;

    while (*s) {
        hash ^= (size_t)*s++;
        hash *= 1099511628211ull;
    }
    return hash;
}

static int dv_alias_cmp(const void *a, const void *b)
{
    const char *ka = *(const char * const *)a;
    const char *kb = *(const char * const *)b;

    return strcmp(ka, kb);
}

static dictionary_t *dv_default_constant_alias_table_storage = NULL;

static void dv_destroy_default_constant_alias_table(void)
{
    dictionary_destroy(dv_default_constant_alias_table_storage);
    dv_default_constant_alias_table_storage = NULL;
}

static dictionary_t *dv_default_constant_alias_table(void)
{
    static int cleanup_registered = 0;
    static const struct {
        const char *key;
        const char *value;
    } aliases[] = {
        { "i",        "i"      },
        { "pi",       "@pi"    },
        { "@pi",      "@pi"    },
        { "\xcf\x80", "@pi"    },
        { "phi",      "@phi"   },
        { "@phi",     "@phi"   },
        { "\xcf\x86", "@phi"   },
        { "gamma",    "@gamma" },
        { "@gamma",   "@gamma" },
        { "\xce\xb3", "@gamma" },
        { "@tau",     "@tau"   },
        { "\xcf\x84", "@tau"   },
    };

    if (dv_default_constant_alias_table_storage)
        return dv_default_constant_alias_table_storage;

    dv_default_constant_alias_table_storage = dictionary_create(sizeof(const char *),
                                                                sizeof(const char *),
                                                                dv_alias_hash,
                                                                dv_alias_cmp,
                                                                NULL,
                                                                NULL,
                                                                NULL,
                                                                NULL,
                                                                NULL);
    if (!dv_default_constant_alias_table_storage)
        abort();
    if (!cleanup_registered) {
        if (atexit(dv_destroy_default_constant_alias_table) != 0)
            abort();
        cleanup_registered = 1;
    }

    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); ++i) {
        const char *key = aliases[i].key;
        const char *value = aliases[i].value;

        if (!dictionary_set(dv_default_constant_alias_table_storage, &key, &value))
            abort();
    }

    return dv_default_constant_alias_table_storage;
}

static const char *dv_lookup_default_constant_alias(const char *name)
{
    const char *mapped = NULL;
    dictionary_t *table;

    if (!name)
        return NULL;

    table = dv_default_constant_alias_table();
    if (dictionary_get(table, &name, &mapped))
        return mapped;
    return name;
}

void dv_store_const_num(dval_t *dv, number_t value)
{
    num_destroy(&dv->c);
    dv->c = num_scope_detach(value);
}

void dv_store_value_num(dval_t *dv, number_t value)
{
    num_destroy(&dv->x);
    dv->x = num_scope_detach(value);
}

/* ------------------------------------------------------------------------- */
/* Canonical singleton leaves                                                */
/* ------------------------------------------------------------------------- */

static struct _dval_t _DV_ZERO_NODE = {
    .ops = &ops_const,
    .a = NULL,
    .b = NULL,
    .c = { { 0, 0, 0, 0, 0 } },
    .x = { { 0, 0, 0, 0, 0 } },
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
    .c = { { 0, 0, 0, 0, 0 } },
    .x = { { 0, 0, 0, 0, 0 } },
    .x_valid = 1,
    .epoch = 0,
    .dx_cache = NULL,
    .name = NULL,
    .refcount = INT_MAX,
    .var_id = 0
};

static struct _dval_t _DV_LN10_NODE = {
    .ops = &ops_const,
    .a = NULL,
    .b = NULL,
    .c = { { 0, 0, 0, 0, 0 } },
    .x = { { 0, 0, 0, 0, 0 } },
    .x_valid = 1,
    .epoch = 0,
    .dx_cache = NULL,
    .name = "ln10",
    .refcount = INT_MAX,
    .var_id = 0
};

const dval_t * const DV_ZERO = &_DV_ZERO_NODE;
const dval_t * const DV_ONE = &_DV_ONE_NODE;
const dval_t * const DV_LN10 = &_DV_LN10_NODE;

static uint64_t next_var_id = 1;
static int dv_singletons_ready = 0;

static void dv_shutdown_singletons(void)
{
    if (!dv_singletons_ready)
        return;

    num_destroy(&_DV_LN10_NODE.c);
    num_destroy(&_DV_LN10_NODE.x);
}

static void dv_init_singletons(void)
{
    if (dv_singletons_ready)
        return;

    _DV_ZERO_NODE.c = NUM_ZERO;
    _DV_ZERO_NODE.x = NUM_ZERO;
    _DV_ONE_NODE.c = NUM_ONE;
    _DV_ONE_NODE.x = NUM_ONE;
    _DV_LN10_NODE.c = num_scope_detach(num_const(NUM_LN10));
    _DV_LN10_NODE.x = num_scope_detach(num_clone(_DV_LN10_NODE.c));
    if (atexit(dv_shutdown_singletons) != 0)
        abort();
    dv_singletons_ready = 1;
}

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

static void append_subscript_digit(char *out, size_t *out_len, char digit)
{
    int d = digit - '0';

    out[(*out_len)++] = (char)0xE2;
    out[(*out_len)++] = (char)0x82;
    out[(*out_len)++] = (char)(0x80 + d);
}

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

char *dv_normalise_name(const char *name)
{
    const char *s;
    const char *e;
    size_t len;
    char *t;
    char *canon;
    size_t out_len = 0;

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

    len = strlen(t);
    canon = malloc(len * 3 + 1);
    if (!canon) {
        free(t);
        abort();
    }

    for (size_t r = 0; r < len; ) {
        if (t[r] == '_' && r + 1 < len &&
            isdigit((unsigned char)t[r + 1])) {
            r++;
            while (r < len && isdigit((unsigned char)t[r])) {
                append_subscript_digit(canon, &out_len, t[r]);
                r++;
            }
            continue;
        }

        canon[out_len++] = t[r++];
    }
    canon[out_len] = '\0';

    size_t run_start = out_len;

    while (run_start > 0 &&
           isdigit((unsigned char)canon[run_start - 1])) {
        run_start--;
    }

    if (run_start < out_len && run_start > 0) {
        char *final = malloc(out_len * 3 + 1);
        size_t final_len = 0;

        if (!final) {
            free(canon);
            free(t);
            abort();
        }

        for (size_t i = 0; i < run_start; ++i)
            final[final_len++] = canon[i];
        for (size_t i = run_start; i < out_len; ++i)
            append_subscript_digit(final, &final_len, canon[i]);
        final[final_len] = '\0';

        free(canon);
        canon = final;
    }

    free(t);
    return canon;
}

char *dv_normalise_binding_name(const char *name)
{
    const char *s;
    const char *e;
    char *inner;
    char *out;

    if (!name)
        return NULL;

    s = name;
    while (*s && isspace((unsigned char)*s))
        s++;
    e = name + strlen(name);
    while (e > s && isspace((unsigned char)e[-1]))
        e--;
    if (e == s)
        return NULL;

    if ((size_t)(e - s) >= 2 && s[0] == '[' && e[-1] == ']') {
        inner = malloc((size_t)(e - s - 1));
        if (!inner)
            abort();
        memcpy(inner, s + 1, (size_t)(e - s - 2));
        inner[e - s - 2] = '\0';
        return inner;
    }

    size_t n = (size_t)(e - s);
    char *trimmed = malloc(n + 1);

    if (!trimmed)
        abort();
    memcpy(trimmed, s, n);
    trimmed[n] = '\0';
    out = dv_normalise_name(dv_default_constant_canonical_name(trimmed));
    free(trimmed);

    return out;
}

int dv_is_default_constant_name(const char *name)
{
    const char *p = name;
    size_t len;

    if (!name || !*name)
        return 0;
    if (*name != 'a' && *name != 'b' && *name != 'c' && *name != 'd')
        return 0;

    p++;
    len = strlen(p);
    if (*p == '\0')
        return 1;
    if ((unsigned char)*p == 0xE2 && len >= 3) {
        while (*p) {
            int d;

            d = (unsigned char)p[0] == 0xE2 &&
                (unsigned char)p[1] == 0x82 &&
                (unsigned char)p[2] >= 0x80 &&
                (unsigned char)p[2] <= 0x89;
            if (!d)
                return 0;
            p += 3;
        }
        return 1;
    }
    if (*p == '_' && p[1] >= '0' && p[1] <= '9') {
        p += 2;
        while (*p >= '0' && *p <= '9')
            p++;
        return *p == '\0';
    }
    return 0;
}

int dv_get_default_constant_num(const char *name, number_t *value_out)
{
    const char *canon = dv_lookup_default_constant_alias(name);

    if (!name || !value_out)
        return 0;

    if (strcmp(canon, "e") == 0) {
        *value_out = num_const(NUM_E);
        return 1;
    }

    if (strcmp(canon, "i") == 0) {
        *value_out = num_const(NUM_I);
        return 1;
    }

    if (strcmp(canon, "@pi") == 0) {
        *value_out = num_const(NUM_PI);
        return 1;
    }

    if (strcmp(canon, "@phi") == 0) {
        *value_out = num_const(NUM_PHI);
        return 1;
    }

    if (strcmp(canon, "@gamma") == 0) {
        *value_out = num_const(NUM_EULER_MASCHERONI);
        return 1;
    }

    return 0;
}

const char *dv_default_constant_canonical_name(const char *name)
{
    return dv_lookup_default_constant_alias(name);
}

/* ------------------------------------------------------------------------- */
/* Lifetime                                                                  */
/* ------------------------------------------------------------------------- */

void dv_retain(const dval_t *dv)
{
    dv_init_singletons();
    if (dv)
        refcount_inc(&((dval_t *)dv)->refcount);
}

static void dv_release(dval_t *dv)
{
    dval_t *a;
    dval_t *b;
    dv_deriv_cache_t *ce;

    if (!dv)
        return;
    dv_init_singletons();
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
    if (dv->binding_expr)
        dv_binding_expr_free(dv->binding_expr);
    num_destroy(&dv->c);
    num_destroy(&dv->x);
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

    dv_init_singletons();
    dv->ops = ops;
    dv->a = NULL;
    dv->b = NULL;
    dv->c = NUM_ZERO;
    dv->x = NUM_ZERO;
    dv->x_valid = 0;
    dv->epoch = 0;
    dv->simplified = false;
    dv->simplify_epoch = 0;
    dv->dx_cache = NULL;
    dv->name = NULL;
    dv->binding_expr = NULL;
    dv->refcount = 1;
    dv->var_id = 0;

    return dv;
}

/* ------------------------------------------------------------------------- */
/* Internal number_t-first leaf builders                                     */
/* ------------------------------------------------------------------------- */

dval_t *dv_make_const_num(number_t x)
{
    dval_t *dv = dv_alloc(&ops_const);

    dv_store_const_num(dv, x);
    dv_store_value_num(dv, num_clone(dv->c));
    dv->x_valid = 1;
    return dv;
}

dval_t *dv_make_var_num(number_t x)
{
    dval_t *dv = dv_alloc(&ops_var);

    dv_store_const_num(dv, x);
    dv_store_value_num(dv, num_clone(dv->c));
    dv->x_valid = 1;
    dv->var_id = alloc_var_id();
    return dv;
}

/* ------------------------------------------------------------------------- */
/* Public number_t constructors                                              */
/* ------------------------------------------------------------------------- */

dval_t *dv_new_const(number_t x)
{
    return dv_make_const_num(num_clone(x));
}

dval_t *dv_new_var(number_t x)
{
    return dv_make_var_num(num_clone(x));
}

static dval_t *dv_attach_name(dval_t *dv, const char *name)
{
    dv->name = dv_normalise_name(name);
    return dv;
}

dval_t *dv_new_named_const(number_t x, const char *name)
{
    return dv_attach_name(dv_new_const(x), name);
}

dval_t *dv_new_named_var(number_t x, const char *name)
{
    return dv_attach_name(dv_new_var(x), name);
}
