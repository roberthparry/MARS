#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dval.h"
#include "dval_bindings_internal.h"
#include "dval_fromstring_internal.h"
#include "dval_tostring_internal.h"

typedef struct {
    const char *p;
    const char *end;
    int         error;
    char        errmsg[256];
} binding_parser_t;

typedef enum {
    BIND_PREC_LOWEST = 0,
    BIND_PREC_ADD    = 1,
    BIND_PREC_MUL    = 2,
    BIND_PREC_POW    = 3,
    BIND_PREC_UNARY  = 4,
    BIND_PREC_ATOM   = 5
} binding_prec_t;

#define BINDING_CONST_COUNT 5u
#define BINDING_EXPR_KIND_COUNT 8u

typedef struct {
    dv_binding_const_id_t id;
    const char           *canonical_name;
    const char           *expr_name;
    const char           *tex_name;
    const number_t       *value;
} binding_const_meta_t;

typedef struct {
    int     precedence;
    bool    atomic;
    void  (*free_payload)(dv_binding_expr_t *expr);
    dval_t *(*eval_dval)(const dv_binding_expr_t *expr);
    void  (*emit_expr)(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
    void  (*emit_tex)(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
} binding_expr_ops_t;

static const char *s_binding_sup_digits[10] = {
    "⁰","¹","²","³","⁴","⁵","⁶","⁷","⁸","⁹"
};

static const char *s_binding_sub_digits[10] = {
    "₀","₁","₂","₃","₄","₅","₆","₇","₈","₉"
};

static const binding_const_meta_t s_binding_consts[BINDING_CONST_COUNT] = {
    [DV_BINDING_CONST_E]     = { DV_BINDING_CONST_E,     "e",      "e", "e",       &NUM_E },
    [DV_BINDING_CONST_I]     = { DV_BINDING_CONST_I,     "i",      "i", "i",       &NUM_I },
    [DV_BINDING_CONST_PI]    = { DV_BINDING_CONST_PI,    "@pi",    "π", "\\pi",    &NUM_PI },
    [DV_BINDING_CONST_PHI]   = { DV_BINDING_CONST_PHI,   "@phi",   "φ", "\\phi",   &NUM_PHI },
    [DV_BINDING_CONST_GAMMA] = { DV_BINDING_CONST_GAMMA, "@gamma", "γ", "\\gamma", &NUM_EULER_MASCHERONI }
};

static const binding_const_meta_t *binding_const_meta(dv_binding_const_id_t const_id)
{
    if ((unsigned)const_id >= BINDING_CONST_COUNT ||
        s_binding_consts[const_id].value == NULL)
        return NULL;
    return &s_binding_consts[const_id];
}

static const char *binding_const_expr_name(dv_binding_const_id_t const_id)
{
    const binding_const_meta_t *meta = binding_const_meta(const_id);

    return meta ? meta->expr_name : "?";
}

static const char *binding_const_tex_name(dv_binding_const_id_t const_id)
{
    const binding_const_meta_t *meta = binding_const_meta(const_id);

    return meta ? meta->tex_name : "?";
}

static void binding_free_none(dv_binding_expr_t *expr);
static void binding_free_number(dv_binding_expr_t *expr);
static void binding_free_unary(dv_binding_expr_t *expr);
static void binding_free_binary(dv_binding_expr_t *expr);
static void binding_free_powi(dv_binding_expr_t *expr);

static dval_t *binding_eval_number(const dv_binding_expr_t *expr);
static dval_t *binding_eval_const(const dv_binding_expr_t *expr);
static dval_t *binding_eval_neg(const dv_binding_expr_t *expr);
static dval_t *binding_eval_add(const dv_binding_expr_t *expr);
static dval_t *binding_eval_sub(const dv_binding_expr_t *expr);
static dval_t *binding_eval_mul(const dv_binding_expr_t *expr);
static dval_t *binding_eval_div(const dv_binding_expr_t *expr);
static dval_t *binding_eval_powi(const dv_binding_expr_t *expr);

static void emit_binding_expr_number(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_const(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_neg(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_add(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_sub(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_mul_node(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_div(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_powi(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);

static void emit_binding_tex_number(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_const(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_neg(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_add(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_sub(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_mul_node(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_div(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_powi(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);

static const binding_expr_ops_t s_binding_expr_ops[BINDING_EXPR_KIND_COUNT] = {
    [DV_BINDING_EXPR_NUMBER] = { BIND_PREC_ATOM,  true,  binding_free_number, binding_eval_number, emit_binding_expr_number,   emit_binding_tex_number   },
    [DV_BINDING_EXPR_CONST]  = { BIND_PREC_ATOM,  true,  binding_free_none,   binding_eval_const,  emit_binding_expr_const,    emit_binding_tex_const    },
    [DV_BINDING_EXPR_NEG]    = { BIND_PREC_UNARY, false, binding_free_unary,  binding_eval_neg,    emit_binding_expr_neg,      emit_binding_tex_neg      },
    [DV_BINDING_EXPR_ADD]    = { BIND_PREC_ADD,   false, binding_free_binary, binding_eval_add,    emit_binding_expr_add,      emit_binding_tex_add      },
    [DV_BINDING_EXPR_SUB]    = { BIND_PREC_ADD,   false, binding_free_binary, binding_eval_sub,    emit_binding_expr_sub,      emit_binding_tex_sub      },
    [DV_BINDING_EXPR_MUL]    = { BIND_PREC_MUL,   false, binding_free_binary, binding_eval_mul,    emit_binding_expr_mul_node, emit_binding_tex_mul_node },
    [DV_BINDING_EXPR_DIV]    = { BIND_PREC_MUL,   false, binding_free_binary, binding_eval_div,    emit_binding_expr_div,      emit_binding_tex_div      },
    [DV_BINDING_EXPR_POWI]   = { BIND_PREC_POW,   true,  binding_free_powi,   binding_eval_powi,   emit_binding_expr_powi,     emit_binding_tex_powi     }
};

static const binding_expr_ops_t *binding_expr_ops_for_kind(dv_binding_expr_kind_t kind)
{
    if ((unsigned)kind >= BINDING_EXPR_KIND_COUNT ||
        s_binding_expr_ops[kind].eval_dval == NULL)
        return NULL;
    return &s_binding_expr_ops[kind];
}

static dv_binding_expr_t *binding_expr_alloc(dv_binding_expr_kind_t kind)
{
    dv_binding_expr_t *expr = calloc(1u, sizeof(*expr));

    if (!expr)
        abort();
    expr->kind = kind;
    return expr;
}

dv_binding_expr_t *dv_binding_expr_new_number_text(const char *text)
{
    dv_binding_expr_t *expr = binding_expr_alloc(DV_BINDING_EXPR_NUMBER);

    expr->u.text = text ? dv_tostring_xstrdup(text) : dv_tostring_xstrdup("0");
    return expr;
}

dv_binding_expr_t *dv_binding_expr_new_const(dv_binding_const_id_t const_id)
{
    dv_binding_expr_t *expr = binding_expr_alloc(DV_BINDING_EXPR_CONST);

    expr->u.const_id = const_id;
    return expr;
}

dv_binding_expr_t *dv_binding_expr_new_neg(dv_binding_expr_t *child)
{
    dv_binding_expr_t *expr = binding_expr_alloc(DV_BINDING_EXPR_NEG);

    expr->u.unary.child = child;
    return expr;
}

dv_binding_expr_t *dv_binding_expr_new_add(dv_binding_expr_t *left, dv_binding_expr_t *right)
{
    dv_binding_expr_t *expr = binding_expr_alloc(DV_BINDING_EXPR_ADD);

    expr->u.binary.left = left;
    expr->u.binary.right = right;
    return expr;
}

dv_binding_expr_t *dv_binding_expr_new_sub(dv_binding_expr_t *left, dv_binding_expr_t *right)
{
    dv_binding_expr_t *expr = binding_expr_alloc(DV_BINDING_EXPR_SUB);

    expr->u.binary.left = left;
    expr->u.binary.right = right;
    return expr;
}

dv_binding_expr_t *dv_binding_expr_new_mul(dv_binding_expr_t *left, dv_binding_expr_t *right)
{
    dv_binding_expr_t *expr = binding_expr_alloc(DV_BINDING_EXPR_MUL);

    expr->u.binary.left = left;
    expr->u.binary.right = right;
    return expr;
}

dv_binding_expr_t *dv_binding_expr_new_div(dv_binding_expr_t *left, dv_binding_expr_t *right)
{
    dv_binding_expr_t *expr = binding_expr_alloc(DV_BINDING_EXPR_DIV);

    expr->u.binary.left = left;
    expr->u.binary.right = right;
    return expr;
}

dv_binding_expr_t *dv_binding_expr_new_powi(dv_binding_expr_t *base, long exponent)
{
    dv_binding_expr_t *expr = binding_expr_alloc(DV_BINDING_EXPR_POWI);

    expr->u.powi.base = base;
    expr->u.powi.exponent = exponent;
    return expr;
}

void dv_binding_expr_free(dv_binding_expr_t *expr)
{
    const binding_expr_ops_t *ops;

    if (!expr)
        return;

    ops = binding_expr_ops_for_kind(expr->kind);
    if (ops && ops->free_payload)
        ops->free_payload(expr);
    free(expr);
}

static number_t binding_const_number(dv_binding_const_id_t const_id)
{
    const binding_const_meta_t *meta = binding_const_meta(const_id);

    return meta ? num_const(*meta->value) : num_clone(NUM_NAN);
}

static void binding_free_none(dv_binding_expr_t *expr)
{
    (void)expr;
}

static void binding_free_number(dv_binding_expr_t *expr)
{
    free(expr->u.text);
}

static void binding_free_unary(dv_binding_expr_t *expr)
{
    dv_binding_expr_free(expr->u.unary.child);
}

static void binding_free_binary(dv_binding_expr_t *expr)
{
    dv_binding_expr_free(expr->u.binary.left);
    dv_binding_expr_free(expr->u.binary.right);
}

static void binding_free_powi(dv_binding_expr_t *expr)
{
    dv_binding_expr_free(expr->u.powi.base);
}

static int binding_number_text_is_exact_decimal(const char *text)
{
    const char *p = text;
    int have_decimal_marker = 0;
    int have_digit = 0;
    int exp_digits = 0;

    if (!p)
        return 0;

    if (*p == '+' || *p == '-')
        p++;

    while (isdigit((unsigned char)*p)) {
        have_digit = 1;
        p++;
    }

    if (*p == '.') {
        have_decimal_marker = 1;
        p++;
        while (isdigit((unsigned char)*p)) {
            have_digit = 1;
            p++;
        }
    }

    if (*p == 'e' || *p == 'E') {
        have_decimal_marker = 1;
        p++;
        if (*p == '+' || *p == '-')
            p++;
        while (isdigit((unsigned char)*p)) {
            exp_digits++;
            p++;
        }
        if (exp_digits == 0)
            return 0;
    }

    return have_digit && have_decimal_marker && *p == '\0';
}

static number_t binding_number_from_exact_decimal(const char *text)
{
    const char *p = text;
    char *digits;
    char *literal;
    number_t value;
    size_t digit_count = 0u;
    size_t digit_cap = strlen(text) + 1u;
    long frac_digits = 0;
    long exponent = 0;
    int negative = 0;
    int seen_nonzero = 0;

    digits = (char *)fs_xmalloc(digit_cap);

    if (*p == '+' || *p == '-') {
        negative = (*p == '-');
        p++;
    }

    while (isdigit((unsigned char)*p)) {
        if (*p != '0' || seen_nonzero) {
            seen_nonzero = 1;
            digits[digit_count++] = *p;
        }
        p++;
    }

    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p)) {
            frac_digits++;
            if (*p != '0' || seen_nonzero) {
                seen_nonzero = 1;
                digits[digit_count++] = *p;
            }
            p++;
        }
    }

    if (*p == 'e' || *p == 'E') {
        int exp_negative = 0;

        p++;
        if (*p == '+' || *p == '-') {
            exp_negative = (*p == '-');
            p++;
        }
        while (isdigit((unsigned char)*p)) {
            exponent = exponent * 10 + (*p - '0');
            p++;
        }
        if (exp_negative)
            exponent = -exponent;
    }

    if (digit_count == 0u) {
        free(digits);
        return num_create_from_string("0");
    }
    digits[digit_count] = '\0';

    frac_digits -= exponent;
    if (frac_digits <= 0) {
        size_t zeros = (size_t)-frac_digits;
        size_t len = (negative ? 1u : 0u) + digit_count + zeros;
        literal = (char *)fs_xmalloc(len + 1u);
        p = literal;
        if (negative)
            *literal++ = '-';
        memcpy(literal, digits, digit_count);
        literal += digit_count;
        memset(literal, '0', zeros);
        literal += zeros;
        *literal = '\0';
        literal = (char *)p;
    } else {
        size_t denom_len = (size_t)frac_digits + 1u;
        size_t len = (negative ? 1u : 0u) + digit_count + 1u + denom_len;
        literal = (char *)fs_xmalloc(len + 1u);
        p = literal;
        if (negative)
            *literal++ = '-';
        memcpy(literal, digits, digit_count);
        literal += digit_count;
        *literal++ = '/';
        *literal++ = '1';
        memset(literal, '0', (size_t)frac_digits);
        literal += frac_digits;
        *literal = '\0';
        literal = (char *)p;
    }

    value = num_create_from_string(literal);
    free(literal);
    free(digits);
    return value;
}

static number_t binding_number_from_text(const char *text)
{
    if (binding_number_text_is_exact_decimal(text))
        return binding_number_from_exact_decimal(text);

    return num_create_from_string(text);
}

dval_t *dv_binding_expr_eval_dval(const dv_binding_expr_t *expr)
{
    const binding_expr_ops_t *ops;

    if (!expr)
        return dv_new_const(NUM_NAN);

    ops = binding_expr_ops_for_kind(expr->kind);
    if (ops && ops->eval_dval)
        return ops->eval_dval(expr);

    return dv_new_const(NUM_NAN);
}

static dval_t *binding_eval_number(const dv_binding_expr_t *expr)
{
    number_t value = binding_number_from_text(expr->u.text);
    dval_t *node = dv_new_const(value);

    num_destroy(&value);
    return node;
}

static dval_t *binding_eval_const(const dv_binding_expr_t *expr)
{
    return dv_new_named_const(binding_const_number(expr->u.const_id),
                              binding_const_expr_name(expr->u.const_id));
}

static dval_t *binding_eval_neg(const dv_binding_expr_t *expr)
{
    dval_t *child = dv_binding_expr_eval_dval(expr->u.unary.child);
    dval_t *node = dv_neg(child);

    dv_free(child);
    return node;
}

static dval_t *binding_eval_binary(const dv_binding_expr_t *expr,
                                   dval_t *(*op)(const dval_t *, const dval_t *))
{
    dval_t *left = dv_binding_expr_eval_dval(expr->u.binary.left);
    dval_t *right = dv_binding_expr_eval_dval(expr->u.binary.right);
    dval_t *node = op(left, right);

    dv_free(left);
    dv_free(right);
    return node;
}

static dval_t *binding_eval_add(const dv_binding_expr_t *expr)
{
    return binding_eval_binary(expr, dv_add);
}

static dval_t *binding_eval_sub(const dv_binding_expr_t *expr)
{
    return binding_eval_binary(expr, dv_sub);
}

static dval_t *binding_eval_mul(const dv_binding_expr_t *expr)
{
    return binding_eval_binary(expr, dv_mul);
}

static bool binding_expr_is_const_id(const dv_binding_expr_t *expr,
                                     dv_binding_const_id_t const_id)
{
    return expr &&
           expr->kind == DV_BINDING_EXPR_CONST &&
           expr->u.const_id == const_id;
}

static bool binding_number_text_eq_long(const dv_binding_expr_t *expr,
                                        long expected_long)
{
    number_t value;
    number_t expected;
    bool equal;

    if (!expr || expr->kind != DV_BINDING_EXPR_NUMBER)
        return false;

    value = binding_number_from_text(expr->u.text);
    expected = num_create_from_long(expected_long);
    equal = num_eq(value, expected);
    num_destroy(&expected);
    num_destroy(&value);
    return equal;
}

static dval_t *binding_eval_known_pi_ratio(const dv_binding_expr_t *numer,
                                           const dv_binding_expr_t *denom)
{
    if (!binding_expr_is_const_id(numer, DV_BINDING_CONST_PI))
        return NULL;

    if (binding_number_text_eq_long(denom, 2))
        return dv_new_const(NUM_PI_2);
    if (binding_number_text_eq_long(denom, 3))
        return dv_new_const(NUM_PI_3);
    if (binding_number_text_eq_long(denom, 4))
        return dv_new_const(NUM_PI_4);
    if (binding_number_text_eq_long(denom, 6))
        return dv_new_const(NUM_PI_6);

    return NULL;
}

static dval_t *binding_eval_div(const dv_binding_expr_t *expr)
{
    dval_t *known = binding_eval_known_pi_ratio(expr->u.binary.left,
                                                expr->u.binary.right);

    if (known)
        return known;

    return binding_eval_binary(expr, dv_div);
}

static dval_t *binding_eval_powi(const dv_binding_expr_t *expr)
{
    dval_t *base = dv_binding_expr_eval_dval(expr->u.powi.base);
    number_t exponent = num_create_from_long(expr->u.powi.exponent);
    dval_t *node = dv_pow(base, &exponent);

    dv_free(base);
    num_destroy(&exponent);
    return node;
}

number_t dv_binding_expr_eval(const dv_binding_expr_t *expr)
{
    dval_t *node;
    number_t value;

    if (!expr)
        return num_clone(NUM_NAN);

    node = dv_binding_expr_eval_dval(expr);
    value = dv_eval(node);
    dv_free(node);
    return value;
}

static void binding_set_error(binding_parser_t *p, const char *msg)
{
    if (!p->error) {
        p->error = 1;
        snprintf(p->errmsg, sizeof(p->errmsg), "%s", msg);
    }
}

static void binding_skip_spaces(binding_parser_t *p)
{
    while (p->p < p->end && isspace((unsigned char)*p->p))
        p->p++;
}

static int scan_utf8_codepoint(const char *p, const char *end, unsigned int *out)
{
    int len;

    if (p >= end)
        return 0;
    len = fs_utf8_decode(p, out);
    if (len <= 0 || p + len > end)
        return 0;
    return len;
}

static int is_superscript_digit_codepoint(unsigned int c)
{
    return c == 0x00B2 || c == 0x00B3 || c == 0x00B9 || c == 0x2070 ||
           (c >= 0x2074 && c <= 0x2079);
}

static int is_subscript_digit_codepoint(unsigned int c)
{
    return c >= 0x2080 && c <= 0x2089;
}

static int is_fraction_glyph_codepoint(unsigned int c)
{
    return c == 0x00BC || c == 0x00BD || c == 0x00BE ||
           (c >= 0x2150 && c <= 0x215E);
}

static size_t scan_unicode_fraction_len(const char *s, const char *end)
{
    const char *p = s;
    unsigned int c;
    int len;
    int digits = 0;

    len = scan_utf8_codepoint(p, end, &c);
    if (len <= 0)
        return 0u;

    if (is_fraction_glyph_codepoint(c))
        return (size_t)len;

    while ((len = scan_utf8_codepoint(p, end, &c)) > 0 &&
           is_superscript_digit_codepoint(c)) {
        p += len;
        ++digits;
    }
    if (digits == 0)
        return 0u;

    len = scan_utf8_codepoint(p, end, &c);
    if (len <= 0 || c != 0x2044)
        return 0u;
    p += len;

    digits = 0;
    while ((len = scan_utf8_codepoint(p, end, &c)) > 0 &&
           is_subscript_digit_codepoint(c)) {
        p += len;
        ++digits;
    }
    if (digits == 0)
        return 0u;

    return (size_t)(p - s);
}

static int binding_region_eq_ci(const char *s, const char *end, const char *lit)
{
    const char *p = s;

    while (*lit) {
        if (p >= end ||
            tolower((unsigned char)*p) != tolower((unsigned char)*lit))
            return 0;
        p++;
        lit++;
    }

    return 1;
}

static size_t scan_special_number_len(const char *s, const char *end)
{
    if (binding_region_eq_ci(s, end, "infinity"))
        return 8u;
    if (binding_region_eq_ci(s, end, "nan") ||
        binding_region_eq_ci(s, end, "inf"))
        return 3u;
    return 0u;
}

static size_t scan_number_atom_len(const char *s, const char *end)
{
    size_t len = scan_decimal_len(s, end);
    const char *p;
    size_t tail;

    if (len == 0u) {
        len = scan_special_number_len(s, end);
        if (len > 0u)
            return len;

        len = scan_unicode_fraction_len(s, end);
        if (len == 0u)
            return 0u;
        p = s + len;
        if (p < end && (*p == 'i' || *p == 'I'))
            p++;
        return (size_t)(p - s);
    }

    p = s + len;
    if (p < end && *p == '/') {
        tail = scan_decimal_len(p + 1, end);
        if (tail == 0u)
            return len;
        p += 1 + tail;
    }

    if (p < end && (*p == 'i' || *p == 'I'))
        p++;

    return (size_t)(p - s);
}

static int parse_number_region(const char *start, const char *end, number_t *out)
{
    char *buf;
    size_t len;
    size_t atom_len;
    char *roundtrip;

    while (start < end && isspace((unsigned char)*start))
        start++;
    while (end > start && isspace((unsigned char)end[-1]))
        end--;

    if (start >= end)
        return 0;

    len = (size_t)(end - start);
    atom_len = scan_number_atom_len(start, end);
    if (atom_len != len)
        return 0;

    buf = (char *)fs_xmalloc(len + 1u);
    memcpy(buf, start, len);
    buf[len] = '\0';

    *out = num_create_from_string(buf);
    free(buf);

    roundtrip = num_to_string(*out);
    if (!roundtrip) {
        num_destroy(out);
        return 0;
    }

    free(roundtrip);
    return 1;
}

static int binding_can_start_atom(const binding_parser_t *p)
{
    if (p->p >= p->end)
        return 0;

    if (*p->p == '(' || *p->p == '+' || *p->p == '-')
        return 1;
    if (isdigit((unsigned char)*p->p) || *p->p == '.')
        return 1;
    if (scan_unicode_fraction_len(p->p, p->end) > 0u)
        return 1;

    {
        const char *q = p->p;
        char *name = read_any_name(&q);

        if (name) {
            free(name);
            return 1;
        }
    }

    return 0;
}

static int parse_binding_integer_exponent(binding_parser_t *p, long *exp_out)
{
    const char *q;
    int sign = 1;
    long value = 0;
    int have_digit = 0;
    int had_parens = 0;

    binding_skip_spaces(p);
    q = p->p;

    if (q < p->end && *q == '(') {
        had_parens = 1;
        q++;
        binding_skip_spaces(p);
        q = p->p;
    }

    if (q < p->end && (*q == '+' || *q == '-')) {
        if (*q == '-')
            sign = -1;
        q++;
    }

    while (q < p->end && isdigit((unsigned char)*q)) {
        have_digit = 1;
        value = value * 10 + (*q - '0');
        q++;
    }

    if (!have_digit)
        return 0;

    p->p = q;
    binding_skip_spaces(p);
    if (had_parens && p->p < p->end && *p->p == ')')
        p->p++;

    *exp_out = sign * value;
    return 1;
}

static int binding_const_id_from_name(const char *name,
                                      dv_binding_const_id_t *const_id_out)
{
    const char *canon = dv_default_constant_canonical_name(name);

    if (!canon)
        return 0;

    for (size_t i = 0; i < BINDING_CONST_COUNT; ++i) {
        if (strcmp(canon, s_binding_consts[i].canonical_name) == 0) {
            *const_id_out = s_binding_consts[i].id;
            return 1;
        }
    }

    return 0;
}

static dv_binding_expr_t *parse_binding_addexpr(binding_parser_t *p);

static dv_binding_expr_t *parse_binding_atom(binding_parser_t *p)
{
    NUM_SCOPE(scope);
    binding_skip_spaces(p);
    if (p->error || p->p >= p->end) {
        binding_set_error(p, "expected binding expression");
        return NULL;
    }

    if (*p->p == '(') {
        dv_binding_expr_t *inner;

        p->p++;
        inner = parse_binding_addexpr(p);
        binding_skip_spaces(p);
        if (!inner)
            return NULL;
        if (p->p >= p->end || *p->p != ')') {
            dv_binding_expr_free(inner);
            binding_set_error(p, "expected ')'");
            return NULL;
        }
        p->p++;
        return inner;
    }

    if (isdigit((unsigned char)*p->p) || *p->p == '.' ||
        scan_special_number_len(p->p, p->end) > 0u ||
        scan_unicode_fraction_len(p->p, p->end) > 0u) {
        size_t len = scan_number_atom_len(p->p, p->end);
        char *text;
        number_t value;

        if (len == 0u || !parse_number_region(p->p, p->p + len, &value)) {
            binding_set_error(p, "expected numeric literal");
            return NULL;
        }
        num_destroy(&value);
        text = (char *)fs_xmalloc(len + 1u);
        memcpy(text, p->p, len);
        text[len] = '\0';
        p->p += len;
        {
            dv_binding_expr_t *expr = dv_binding_expr_new_number_text(text);
            free(text);
            return expr;
        }
    }

    {
        char *name = read_any_name(&p->p);
        dv_binding_const_id_t const_id;

        if (!name) {
            binding_set_error(p, "expected arithmetic constant");
            return NULL;
        }
        if (!binding_const_id_from_name(name, &const_id)) {
            free(name);
            binding_set_error(p, "binding expressions only allow numeric constants");
            return NULL;
        }
        free(name);
        return dv_binding_expr_new_const(const_id);
    }
}

static dv_binding_expr_t *parse_binding_power(binding_parser_t *p)
{
    dv_binding_expr_t *base = parse_binding_atom(p);
    long exponent_long;

    if (!base)
        return NULL;

    binding_skip_spaces(p);
    if (p->p >= p->end || *p->p != '^')
        return base;

    p->p++;
    if (!parse_binding_integer_exponent(p, &exponent_long)) {
        dv_binding_expr_free(base);
        binding_set_error(p, "expected integer exponent after '^'");
        return NULL;
    }

    return dv_binding_expr_new_powi(base, exponent_long);
}

static dv_binding_expr_t *parse_binding_signed(binding_parser_t *p)
{
    binding_skip_spaces(p);
    if (p->p < p->end && *p->p == '+') {
        p->p++;
        return parse_binding_signed(p);
    }
    if (p->p < p->end && *p->p == '-') {
        dv_binding_expr_t *inner;

        p->p++;
        inner = parse_binding_signed(p);
        if (!inner)
            return NULL;
        return dv_binding_expr_new_neg(inner);
    }

    return parse_binding_power(p);
}

static dv_binding_expr_t *parse_binding_mulexpr(binding_parser_t *p)
{
    dv_binding_expr_t *numer = parse_binding_signed(p);
    dv_binding_expr_t *denom = NULL;

    if (!numer)
        return NULL;

    for (;;) {
        char op = '\0';
        const char *peek;
        dv_binding_expr_t *rhs;

        binding_skip_spaces(p);
        if (p->p >= p->end)
            break;

        peek = p->p;
        if (*peek == '+' || *peek == '-')
            break;
        if (*peek == '*' || *peek == '/') {
            op = *peek;
            p->p++;
        } else if (binding_can_start_atom(p)) {
            op = '*';
        } else {
            break;
        }

        rhs = parse_binding_signed(p);
        if (!rhs) {
            dv_binding_expr_free(numer);
            dv_binding_expr_free(denom);
            return NULL;
        }

        if (op == '*') {
            numer = dv_binding_expr_new_mul(numer, rhs);
        } else if (!denom) {
            denom = rhs;
        } else {
            denom = dv_binding_expr_new_mul(denom, rhs);
        }
    }

    if (denom)
        return dv_binding_expr_new_div(numer, denom);

    return numer;
}

static dv_binding_expr_t *parse_binding_addexpr(binding_parser_t *p)
{
    dv_binding_expr_t *lhs = parse_binding_mulexpr(p);

    if (!lhs)
        return NULL;

    for (;;) {
        char op;
        dv_binding_expr_t *rhs;

        binding_skip_spaces(p);
        if (p->p >= p->end || (*p->p != '+' && *p->p != '-'))
            break;

        op = *p->p++;
        rhs = parse_binding_mulexpr(p);
        if (!rhs) {
            dv_binding_expr_free(lhs);
            return NULL;
        }

        lhs = (op == '+')
            ? dv_binding_expr_new_add(lhs, rhs)
            : dv_binding_expr_new_sub(lhs, rhs);
    }

    return lhs;
}

dv_binding_expr_t *dv_binding_expr_parse_region(const char *start,
                                                const char *end,
                                                char *errmsg,
                                                size_t errmsg_n)
{
    binding_parser_t ps;
    dv_binding_expr_t *result;

    if (errmsg && errmsg_n > 0u)
        errmsg[0] = '\0';

    if (!start || !end || end < start)
        return NULL;

    while (start < end && isspace((unsigned char)*start))
        start++;
    while (end > start && isspace((unsigned char)end[-1]))
        end--;

    ps.p = start;
    ps.end = end;
    ps.error = 0;
    ps.errmsg[0] = '\0';

    result = parse_binding_addexpr(&ps);
    binding_skip_spaces(&ps);
    if (result && !ps.error && ps.p == end)
        return result;

    if (result)
        dv_binding_expr_free(result);
    if (!ps.error)
        binding_set_error(&ps, "trailing input");
    if (errmsg && errmsg_n > 0u)
        snprintf(errmsg, errmsg_n, "%s", ps.errmsg);
    return NULL;
}

static int binding_expr_prec(const dv_binding_expr_t *expr)
{
    const binding_expr_ops_t *ops;

    if (!expr)
        return BIND_PREC_LOWEST;

    ops = binding_expr_ops_for_kind(expr->kind);
    if (ops)
        return ops->precedence;

    return BIND_PREC_LOWEST;
}

static bool binding_expr_is_atomic(const dv_binding_expr_t *expr)
{
    const binding_expr_ops_t *ops;

    if (!expr)
        return false;

    ops = binding_expr_ops_for_kind(expr->kind);
    return ops ? ops->atomic : false;
}

static void emit_binding_superscript_int(sbuf_t *b, long n)
{
    char tmp[32];
    int len = 0;

    if (n < 0) {
        sbuf_puts(b, "⁻");
        n = -n;
    }
    if (n == 0) {
        sbuf_puts(b, "⁰");
        return;
    }
    while (n > 0 && len < (int)sizeof(tmp)) {
        tmp[len++] = (char)('0' + (n % 10));
        n /= 10;
    }
    while (--len >= 0)
        sbuf_puts(b, s_binding_sup_digits[tmp[len] - '0']);
}

static bool binding_text_is_simple_rational(const char *text,
                                            bool *negative_out,
                                            const char **numer_start_out,
                                            size_t *numer_len_out,
                                            const char **denom_start_out,
                                            size_t *denom_len_out)
{
    const char *s;
    const char *slash;

    if (!text || !*text)
        return false;

    s = text;
    *negative_out = false;
    if (*s == '+' || *s == '-') {
        *negative_out = (*s == '-');
        s++;
    }
    if (!isdigit((unsigned char)*s))
        return false;

    slash = strchr(s, '/');
    if (!slash || strchr(slash + 1, '/'))
        return false;
    if (slash == s || slash[1] == '\0')
        return false;

    for (const char *p = s; p < slash; ++p)
        if (!isdigit((unsigned char)*p))
            return false;
    for (const char *p = slash + 1; *p; ++p)
        if (!isdigit((unsigned char)*p))
            return false;

    *numer_start_out = s;
    *numer_len_out = (size_t)(slash - s);
    *denom_start_out = slash + 1;
    *denom_len_out = strlen(slash + 1);
    return true;
}

static void emit_binding_unicode_digits(sbuf_t *b,
                                        const char *digits,
                                        size_t len,
                                        const char *const table[10])
{
    for (size_t i = 0; i < len; ++i)
        sbuf_puts(b, table[digits[i] - '0']);
}

static void emit_binding_number_text(const char *text, sbuf_t *b)
{
    bool negative;
    const char *numer;
    const char *denom;
    size_t numer_len;
    size_t denom_len;

    if (binding_text_is_simple_rational(text, &negative,
                                        &numer, &numer_len,
                                        &denom, &denom_len)) {
        if (negative)
            sbuf_putc(b, '-');
        emit_binding_unicode_digits(b, numer, numer_len, s_binding_sup_digits);
        sbuf_puts(b, "⁄");
        emit_binding_unicode_digits(b, denom, denom_len, s_binding_sub_digits);
        return;
    }

    sbuf_puts(b, text);
}

static void emit_binding_expr(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_expr(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);

static void emit_binding_expr_mul(const dv_binding_expr_t *left,
                                  const dv_binding_expr_t *right,
                                  sbuf_t *b,
                                  int parent_prec)
{
    bool need = BIND_PREC_MUL < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    emit_binding_expr(left, b, BIND_PREC_MUL);
    if (!(binding_expr_is_atomic(left) && binding_expr_is_atomic(right)))
        sbuf_puts(b, "·");
    emit_binding_expr(right, b, BIND_PREC_MUL);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_tex_mul(const dv_binding_expr_t *left,
                                 const dv_binding_expr_t *right,
                                 sbuf_t *b,
                                 int parent_prec)
{
    bool need = BIND_PREC_MUL < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    emit_binding_tex_expr(left, b, BIND_PREC_MUL);
    if (binding_expr_is_atomic(left) && binding_expr_is_atomic(right))
        sbuf_putc(b, ' ');
    else
        sbuf_puts(b, " \\cdot ");
    emit_binding_tex_expr(right, b, BIND_PREC_MUL);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_expr_number(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    (void)parent_prec;
    emit_binding_number_text(expr->u.text, b);
}

static void emit_binding_expr_const(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    (void)parent_prec;
    sbuf_puts(b, binding_const_expr_name(expr->u.const_id));
}

static void emit_binding_expr_neg(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = BIND_PREC_UNARY < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    sbuf_putc(b, '-');
    emit_binding_expr(expr->u.unary.child, b, BIND_PREC_UNARY);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_expr_addsub(const dv_binding_expr_t *expr,
                                     sbuf_t *b,
                                     int parent_prec,
                                     const char *op,
                                     int right_prec)
{
    bool need = BIND_PREC_ADD < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    emit_binding_expr(expr->u.binary.left, b, BIND_PREC_ADD);
    sbuf_puts(b, op);
    emit_binding_expr(expr->u.binary.right, b, right_prec);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_expr_add(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_expr_addsub(expr, b, parent_prec, " + ", BIND_PREC_ADD);
}

static void emit_binding_expr_sub(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_expr_addsub(expr, b, parent_prec, " - ", BIND_PREC_ADD + 1);
}

static void emit_binding_expr_mul_node(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_expr_mul(expr->u.binary.left, expr->u.binary.right, b, parent_prec);
}

static void emit_binding_expr_div(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = BIND_PREC_MUL < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    emit_binding_expr(expr->u.binary.left, b, BIND_PREC_MUL);
    sbuf_putc(b, '/');
    emit_binding_expr(expr->u.binary.right, b, BIND_PREC_POW);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_expr_powi(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = binding_expr_prec(expr->u.powi.base) < BIND_PREC_POW;

    (void)parent_prec;
    if (need)
        sbuf_putc(b, '(');
    emit_binding_expr(expr->u.powi.base, b, BIND_PREC_POW);
    if (need)
        sbuf_putc(b, ')');
    emit_binding_superscript_int(b, expr->u.powi.exponent);
}

static void emit_binding_tex_number(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    char *tex;

    (void)parent_prec;
    tex = dv_tostring_texify(expr->u.text);
    if (tex) {
        sbuf_puts(b, tex);
        free(tex);
    }
}

static void emit_binding_tex_const(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    (void)parent_prec;
    sbuf_puts(b, binding_const_tex_name(expr->u.const_id));
}

static void emit_binding_tex_neg(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = BIND_PREC_UNARY < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    sbuf_putc(b, '-');
    emit_binding_tex_expr(expr->u.unary.child, b, BIND_PREC_UNARY);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_tex_addsub(const dv_binding_expr_t *expr,
                                    sbuf_t *b,
                                    int parent_prec,
                                    const char *op,
                                    int right_prec)
{
    bool need = BIND_PREC_ADD < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    emit_binding_tex_expr(expr->u.binary.left, b, BIND_PREC_ADD);
    sbuf_puts(b, op);
    emit_binding_tex_expr(expr->u.binary.right, b, right_prec);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_tex_add(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_tex_addsub(expr, b, parent_prec, " + ", BIND_PREC_ADD);
}

static void emit_binding_tex_sub(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_tex_addsub(expr, b, parent_prec, " - ", BIND_PREC_ADD + 1);
}

static void emit_binding_tex_mul_node(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_tex_mul(expr->u.binary.left, expr->u.binary.right, b, parent_prec);
}

static void emit_binding_tex_div(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    (void)parent_prec;
    sbuf_puts(b, "\\frac{");
    emit_binding_tex_expr(expr->u.binary.left, b, BIND_PREC_LOWEST);
    sbuf_puts(b, "}{");
    emit_binding_tex_expr(expr->u.binary.right, b, BIND_PREC_LOWEST);
    sbuf_putc(b, '}');
}

static void emit_binding_tex_powi(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = binding_expr_prec(expr->u.powi.base) < BIND_PREC_POW;
    char expbuf[64];

    (void)parent_prec;
    if (need)
        sbuf_putc(b, '(');
    emit_binding_tex_expr(expr->u.powi.base, b, BIND_PREC_POW);
    if (need)
        sbuf_putc(b, ')');
    snprintf(expbuf, sizeof(expbuf), "%ld", expr->u.powi.exponent);
    sbuf_puts(b, "^{");
    sbuf_puts(b, expbuf);
    sbuf_putc(b, '}');
}

static void emit_binding_expr(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    const binding_expr_ops_t *ops;

    if (!expr) {
        sbuf_puts(b, "0");
        return;
    }

    ops = binding_expr_ops_for_kind(expr->kind);
    if (!ops || !ops->emit_expr) {
        sbuf_puts(b, "?");
        return;
    }

    ops->emit_expr(expr, b, parent_prec);
}

static void emit_binding_tex_expr(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    const binding_expr_ops_t *ops;

    if (!expr) {
        sbuf_puts(b, "0");
        return;
    }

    ops = binding_expr_ops_for_kind(expr->kind);
    if (!ops || !ops->emit_tex) {
        sbuf_puts(b, "?");
        return;
    }

    ops->emit_tex(expr, b, parent_prec);
}

char *dv_binding_expr_to_string(const dv_binding_expr_t *expr)
{
    sbuf_t b;

    sbuf_init(&b);
    emit_binding_expr(expr, &b, BIND_PREC_LOWEST);
    {
        char *out = dv_tostring_xstrdup(b.data);
        sbuf_free(&b);
        return out;
    }
}

char *dv_binding_expr_to_tex(const dv_binding_expr_t *expr)
{
    sbuf_t b;

    sbuf_init(&b);
    emit_binding_tex_expr(expr, &b, BIND_PREC_LOWEST);
    {
        char *out = dv_tostring_xstrdup(b.data);
        sbuf_free(&b);
        return out;
    }
}
