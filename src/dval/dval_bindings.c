#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dval.h"
#include "dval_binding_simplify.h"
#include "dval_bindings.h"
#include "dval_fromstring.h"
#include "dval_tostring.h"
#include "internal/number_internal.h"

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
#define BINDING_EXPR_KIND_COUNT 10u

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
    dv_binding_expr_t *(*clone)(const dv_binding_expr_t *expr);
    dval_t *(*eval_dval)(const dv_binding_expr_t *expr);
    bool  (*number_value)(const dv_binding_expr_t *expr, number_t *out);
    dv_binding_expr_t *(*simplify)(dv_binding_expr_t *expr);
    bool  (*struct_eq)(const dv_binding_expr_t *left,
                       const dv_binding_expr_t *right);
    bool  (*numeric_literal)(const dv_binding_expr_t *expr);
    bool  (*exact_complex)(const dv_binding_expr_t *expr,
                           binding_exact_complex_t *out);
    void  (*emit_expr)(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
    void  (*emit_func)(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
    void  (*emit_tex)(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
} binding_expr_ops_t;

typedef number_t (*binding_number_unary_fn)(number_t);
typedef number_t (*binding_number_binary_fn)(number_t, number_t);

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
static void binding_free_unary_op(dv_binding_expr_t *expr);
static void binding_free_binary_op(dv_binding_expr_t *expr);

static dv_binding_expr_t *binding_clone_number(const dv_binding_expr_t *expr);
static dv_binding_expr_t *binding_clone_const(const dv_binding_expr_t *expr);
static dv_binding_expr_t *binding_clone_neg(const dv_binding_expr_t *expr);
static dv_binding_expr_t *binding_clone_add(const dv_binding_expr_t *expr);
static dv_binding_expr_t *binding_clone_sub(const dv_binding_expr_t *expr);
static dv_binding_expr_t *binding_clone_mul(const dv_binding_expr_t *expr);
static dv_binding_expr_t *binding_clone_div(const dv_binding_expr_t *expr);
static dv_binding_expr_t *binding_clone_powi(const dv_binding_expr_t *expr);
static dv_binding_expr_t *binding_clone_unary_op(const dv_binding_expr_t *expr);
static dv_binding_expr_t *binding_clone_binary_op(const dv_binding_expr_t *expr);

static dval_t *binding_eval_number(const dv_binding_expr_t *expr);
static dval_t *binding_eval_const(const dv_binding_expr_t *expr);
static dval_t *binding_eval_neg(const dv_binding_expr_t *expr);
static dval_t *binding_eval_add(const dv_binding_expr_t *expr);
static dval_t *binding_eval_sub(const dv_binding_expr_t *expr);
static dval_t *binding_eval_mul(const dv_binding_expr_t *expr);
static dval_t *binding_eval_div(const dv_binding_expr_t *expr);
static dval_t *binding_eval_powi(const dv_binding_expr_t *expr);
static dval_t *binding_eval_unary_op(const dv_binding_expr_t *expr);
static dval_t *binding_eval_binary_op(const dv_binding_expr_t *expr);

static bool binding_number_value_number(const dv_binding_expr_t *expr,
                                        number_t *out);
static bool binding_number_value_false(const dv_binding_expr_t *expr,
                                       number_t *out);
static bool binding_number_value_neg(const dv_binding_expr_t *expr,
                                     number_t *out);
static bool binding_number_value_add(const dv_binding_expr_t *expr,
                                     number_t *out);
static bool binding_number_value_sub(const dv_binding_expr_t *expr,
                                     number_t *out);
static bool binding_number_value_mul(const dv_binding_expr_t *expr,
                                     number_t *out);
static bool binding_number_value_div(const dv_binding_expr_t *expr,
                                     number_t *out);
static bool binding_number_value_powi(const dv_binding_expr_t *expr,
                                      number_t *out);

static bool binding_struct_eq_number(const dv_binding_expr_t *left,
                                     const dv_binding_expr_t *right);
static bool binding_struct_eq_const(const dv_binding_expr_t *left,
                                    const dv_binding_expr_t *right);
static bool binding_struct_eq_unary(const dv_binding_expr_t *left,
                                    const dv_binding_expr_t *right);
static bool binding_struct_eq_binary(const dv_binding_expr_t *left,
                                     const dv_binding_expr_t *right);
static bool binding_struct_eq_powi(const dv_binding_expr_t *left,
                                   const dv_binding_expr_t *right);
static bool binding_struct_eq_unary_op(const dv_binding_expr_t *left,
                                       const dv_binding_expr_t *right);
static bool binding_struct_eq_binary_op(const dv_binding_expr_t *left,
                                        const dv_binding_expr_t *right);

static bool binding_numeric_literal_true(const dv_binding_expr_t *expr);
static bool binding_numeric_literal_const(const dv_binding_expr_t *expr);
static bool binding_numeric_literal_unary(const dv_binding_expr_t *expr);
static bool binding_numeric_literal_binary(const dv_binding_expr_t *expr);
static bool binding_numeric_literal_powi(const dv_binding_expr_t *expr);
static bool binding_numeric_literal_false(const dv_binding_expr_t *expr);

static bool binding_exact_complex_number(const dv_binding_expr_t *expr,
                                         binding_exact_complex_t *out);
static bool binding_exact_complex_const(const dv_binding_expr_t *expr,
                                        binding_exact_complex_t *out);
static bool binding_exact_complex_unary(const dv_binding_expr_t *expr,
                                        binding_exact_complex_t *out);
static bool binding_exact_complex_addsub(const dv_binding_expr_t *expr,
                                         binding_exact_complex_t *out);
static bool binding_exact_complex_mul(const dv_binding_expr_t *expr,
                                      binding_exact_complex_t *out);
static bool binding_exact_complex_div(const dv_binding_expr_t *expr,
                                      binding_exact_complex_t *out);
static bool binding_exact_complex_powi(const dv_binding_expr_t *expr,
                                       binding_exact_complex_t *out);

static void emit_binding_expr_number(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_const(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_neg(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_add(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_sub(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_mul_node(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_div(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_powi(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_unary_op(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_binary_op(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);

static void emit_binding_func_number(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_const(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_neg(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_add(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_sub(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_mul_node(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_div(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_powi(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_unary_op(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_binary_op(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);

static void emit_binding_tex_number(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_const(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_neg(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_add(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_sub(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_mul_node(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_div(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_powi(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_unary_op(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_binary_op(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static const binding_expr_ops_t s_binding_expr_ops[BINDING_EXPR_KIND_COUNT] = {
    [DV_BINDING_EXPR_NUMBER]    = { BIND_PREC_ATOM,  true,  binding_free_number,    binding_clone_number,    binding_eval_number,    binding_number_value_number, dv_binding_simplify_atom,      binding_struct_eq_number,    binding_numeric_literal_true,   binding_exact_complex_number, emit_binding_expr_number,    emit_binding_func_number,    emit_binding_tex_number    },
    [DV_BINDING_EXPR_CONST]     = { BIND_PREC_ATOM,  true,  binding_free_none,      binding_clone_const,     binding_eval_const,     binding_number_value_false,  dv_binding_simplify_atom,      binding_struct_eq_const,     binding_numeric_literal_const,  binding_exact_complex_const,  emit_binding_expr_const,     emit_binding_func_const,     emit_binding_tex_const     },
    [DV_BINDING_EXPR_NEG]       = { BIND_PREC_UNARY, false, binding_free_unary,     binding_clone_neg,       binding_eval_neg,       binding_number_value_neg,    dv_binding_simplify_neg,       binding_struct_eq_unary,     binding_numeric_literal_unary,  binding_exact_complex_unary,  emit_binding_expr_neg,       emit_binding_func_neg,       emit_binding_tex_neg       },
    [DV_BINDING_EXPR_ADD]       = { BIND_PREC_ADD,   false, binding_free_binary,    binding_clone_add,       binding_eval_add,       binding_number_value_add,    dv_binding_simplify_addsub,    binding_struct_eq_binary,    binding_numeric_literal_binary, binding_exact_complex_addsub, emit_binding_expr_add,       emit_binding_func_add,       emit_binding_tex_add       },
    [DV_BINDING_EXPR_SUB]       = { BIND_PREC_ADD,   false, binding_free_binary,    binding_clone_sub,       binding_eval_sub,       binding_number_value_sub,    dv_binding_simplify_addsub,    binding_struct_eq_binary,    binding_numeric_literal_binary, binding_exact_complex_addsub, emit_binding_expr_sub,       emit_binding_func_sub,       emit_binding_tex_sub       },
    [DV_BINDING_EXPR_MUL]       = { BIND_PREC_MUL,   false, binding_free_binary,    binding_clone_mul,       binding_eval_mul,       binding_number_value_mul,    dv_binding_simplify_mul,       binding_struct_eq_binary,    binding_numeric_literal_binary, binding_exact_complex_mul,    emit_binding_expr_mul_node,  emit_binding_func_mul_node,  emit_binding_tex_mul_node  },
    [DV_BINDING_EXPR_DIV]       = { BIND_PREC_MUL,   false, binding_free_binary,    binding_clone_div,       binding_eval_div,       binding_number_value_div,    dv_binding_simplify_div,       binding_struct_eq_binary,    binding_numeric_literal_binary, binding_exact_complex_div,    emit_binding_expr_div,       emit_binding_func_div,       emit_binding_tex_div       },
    [DV_BINDING_EXPR_POWI]      = { BIND_PREC_POW,   true,  binding_free_powi,      binding_clone_powi,      binding_eval_powi,      binding_number_value_powi,   dv_binding_simplify_powi,      binding_struct_eq_powi,      binding_numeric_literal_powi,   binding_exact_complex_powi,   emit_binding_expr_powi,      emit_binding_func_powi,      emit_binding_tex_powi      },
    [DV_BINDING_EXPR_UNARY_OP]  = { BIND_PREC_UNARY, false, binding_free_unary_op,  binding_clone_unary_op,  binding_eval_unary_op,  binding_number_value_false,  dv_binding_simplify_unary_op,  binding_struct_eq_unary_op,  binding_numeric_literal_false,  NULL,                         emit_binding_expr_unary_op,  emit_binding_func_unary_op,  emit_binding_tex_unary_op  },
    [DV_BINDING_EXPR_BINARY_OP] = { BIND_PREC_POW,   false, binding_free_binary_op, binding_clone_binary_op, binding_eval_binary_op, binding_number_value_false,  dv_binding_simplify_binary_op, binding_struct_eq_binary_op, binding_numeric_literal_false,  NULL,                         emit_binding_expr_binary_op, emit_binding_func_binary_op, emit_binding_tex_binary_op }
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
    expr->cached_value = NUM_ZERO;
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

dv_binding_expr_t *dv_binding_expr_new_unary_op(const dval_ops_t *ops, dv_binding_expr_t *child)
{
    dv_binding_expr_t *expr = binding_expr_alloc(DV_BINDING_EXPR_UNARY_OP);

    expr->u.unary_op.ops = ops;
    expr->u.unary_op.child = child;
    return expr;
}

dv_binding_expr_t *dv_binding_expr_new_binary_op(const dval_ops_t *ops,
                                                 dv_binding_expr_t *left,
                                                 dv_binding_expr_t *right)
{
    dv_binding_expr_t *expr = binding_expr_alloc(DV_BINDING_EXPR_BINARY_OP);

    expr->u.binary_op.ops = ops;
    expr->u.binary_op.left = left;
    expr->u.binary_op.right = right;
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
    if (expr->cached_value_valid)
        num_destroy(&expr->cached_value);
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

static void binding_free_unary_op(dv_binding_expr_t *expr)
{
    dv_binding_expr_free(expr->u.unary_op.child);
}

static void binding_free_binary_op(dv_binding_expr_t *expr)
{
    dv_binding_expr_free(expr->u.binary_op.left);
    dv_binding_expr_free(expr->u.binary_op.right);
}

static dv_binding_expr_t *binding_clone_binary_plain(const dv_binding_expr_t *expr,
                                                     dv_binding_expr_t *(*ctor)(dv_binding_expr_t *,
                                                                               dv_binding_expr_t *))
{
    return ctor(dv_binding_expr_clone(expr->u.binary.left),
                dv_binding_expr_clone(expr->u.binary.right));
}

static dv_binding_expr_t *binding_clone_number(const dv_binding_expr_t *expr)
{
    return dv_binding_expr_new_number_text(expr->u.text);
}

static dv_binding_expr_t *binding_clone_const(const dv_binding_expr_t *expr)
{
    return dv_binding_expr_new_const(expr->u.const_id);
}

static dv_binding_expr_t *binding_clone_neg(const dv_binding_expr_t *expr)
{
    return dv_binding_expr_new_neg(dv_binding_expr_clone(expr->u.unary.child));
}

static dv_binding_expr_t *binding_clone_add(const dv_binding_expr_t *expr)
{
    return binding_clone_binary_plain(expr, dv_binding_expr_new_add);
}

static dv_binding_expr_t *binding_clone_sub(const dv_binding_expr_t *expr)
{
    return binding_clone_binary_plain(expr, dv_binding_expr_new_sub);
}

static dv_binding_expr_t *binding_clone_mul(const dv_binding_expr_t *expr)
{
    return binding_clone_binary_plain(expr, dv_binding_expr_new_mul);
}

static dv_binding_expr_t *binding_clone_div(const dv_binding_expr_t *expr)
{
    return binding_clone_binary_plain(expr, dv_binding_expr_new_div);
}

static dv_binding_expr_t *binding_clone_powi(const dv_binding_expr_t *expr)
{
    return dv_binding_expr_new_powi(dv_binding_expr_clone(expr->u.powi.base),
                                    expr->u.powi.exponent);
}

static dv_binding_expr_t *binding_clone_unary_op(const dv_binding_expr_t *expr)
{
    return dv_binding_expr_new_unary_op(expr->u.unary_op.ops,
                                        dv_binding_expr_clone(expr->u.unary_op.child));
}

static dv_binding_expr_t *binding_clone_binary_op(const dv_binding_expr_t *expr)
{
    return dv_binding_expr_new_binary_op(expr->u.binary_op.ops,
                                         dv_binding_expr_clone(expr->u.binary_op.left),
                                         dv_binding_expr_clone(expr->u.binary_op.right));
}

dv_binding_expr_t *dv_binding_expr_clone(const dv_binding_expr_t *expr)
{
    const binding_expr_ops_t *ops;

    if (!expr)
        return NULL;

    ops = binding_expr_ops_for_kind(expr->kind);
    return (ops && ops->clone) ? ops->clone(expr) : NULL;
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

number_t binding_number_from_text(const char *text)
{
    if (text && strcmp(text, "∞") == 0)
        return num_clone(NUM_INF);
    if (text && strcmp(text, "-∞") == 0)
        return num_clone(NUM_NINF);
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
    number_t value = binding_const_number(expr->u.const_id);
    dval_t *node = dv_new_named_const(value,
                                      binding_const_expr_name(expr->u.const_id));

    num_destroy(&value);
    return node;
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

bool binding_expr_is_const_id(const dv_binding_expr_t *expr,
                              dv_binding_const_id_t const_id)
{
    return expr &&
           expr->kind == DV_BINDING_EXPR_CONST &&
           expr->u.const_id == const_id;
}


bool dv_binding_expr_struct_eq(const dv_binding_expr_t *left,
                               const dv_binding_expr_t *right)
{
    const binding_expr_ops_t *ops;

    if (left == right)
        return true;
    if (!left || !right || left->kind != right->kind)
        return false;

    ops = binding_expr_ops_for_kind(left->kind);
    return ops && ops->struct_eq ? ops->struct_eq(left, right) : false;
}

static bool binding_struct_eq_number(const dv_binding_expr_t *left,
                                     const dv_binding_expr_t *right)
{
    if (!left->u.text || !right->u.text)
        return left->u.text == right->u.text;
    return strcmp(left->u.text, right->u.text) == 0;
}

static bool binding_struct_eq_const(const dv_binding_expr_t *left,
                                    const dv_binding_expr_t *right)
{
    return left->u.const_id == right->u.const_id;
}

static bool binding_struct_eq_unary(const dv_binding_expr_t *left,
                                    const dv_binding_expr_t *right)
{
    return dv_binding_expr_struct_eq(left->u.unary.child,
                                     right->u.unary.child);
}

static bool binding_struct_eq_binary(const dv_binding_expr_t *left,
                                     const dv_binding_expr_t *right)
{
    return dv_binding_expr_struct_eq(left->u.binary.left,
                                     right->u.binary.left) &&
           dv_binding_expr_struct_eq(left->u.binary.right,
                                     right->u.binary.right);
}

static bool binding_struct_eq_powi(const dv_binding_expr_t *left,
                                   const dv_binding_expr_t *right)
{
    return left->u.powi.exponent == right->u.powi.exponent &&
           dv_binding_expr_struct_eq(left->u.powi.base,
                                     right->u.powi.base);
}

static bool binding_struct_eq_unary_op(const dv_binding_expr_t *left,
                                       const dv_binding_expr_t *right)
{
    return left->u.unary_op.ops == right->u.unary_op.ops &&
           dv_binding_expr_struct_eq(left->u.unary_op.child,
                                     right->u.unary_op.child);
}

static bool binding_struct_eq_binary_op(const dv_binding_expr_t *left,
                                        const dv_binding_expr_t *right)
{
    return left->u.binary_op.ops == right->u.binary_op.ops &&
           dv_binding_expr_struct_eq(left->u.binary_op.left,
                                     right->u.binary_op.left) &&
           dv_binding_expr_struct_eq(left->u.binary_op.right,
                                     right->u.binary_op.right);
}

static bool binding_numeric_literal_true(const dv_binding_expr_t *expr)
{
    (void)expr;
    return true;
}

static bool binding_numeric_literal_const(const dv_binding_expr_t *expr)
{
    /*
     * These constants are lexical numeric atoms rather than symbolic display
     * constants.  Symbolic constants such as pi/gamma stay in the preserved
     * expression tree even though they also have numeric values.
     */
    return expr->u.const_id == DV_BINDING_CONST_I;
}

static bool binding_numeric_literal_unary(const dv_binding_expr_t *expr)
{
    return dv_binding_expr_is_numeric_literal(expr->u.unary.child);
}

static bool binding_numeric_literal_binary(const dv_binding_expr_t *expr)
{
    return dv_binding_expr_is_numeric_literal(expr->u.binary.left) &&
           dv_binding_expr_is_numeric_literal(expr->u.binary.right);
}

static bool binding_numeric_literal_powi(const dv_binding_expr_t *expr)
{
    return dv_binding_expr_is_numeric_literal(expr->u.powi.base);
}

static bool binding_numeric_literal_false(const dv_binding_expr_t *expr)
{
    (void)expr;
    return false;
}

bool dv_binding_expr_is_numeric_literal(const dv_binding_expr_t *expr)
{
    const binding_expr_ops_t *ops;

    if (!expr)
        return false;

    ops = binding_expr_ops_for_kind(expr->kind);
    return ops && ops->numeric_literal ? ops->numeric_literal(expr) : false;
}

void dv_binding_exact_complex_clear(binding_exact_complex_t *value)
{
    if (!value)
        return;
    num_destroy(&value->imag);
    num_destroy(&value->real);
}

static void binding_exact_complex_set(binding_exact_complex_t *out,
                                      number_t real,
                                      number_t imag)
{
    out->real = num_scope_detach(real);
    out->imag = num_scope_detach(imag);
}

bool dv_binding_expr_exact_complex(const dv_binding_expr_t *expr,
                                   binding_exact_complex_t *out)
{
    const binding_expr_ops_t *ops;

    if (!expr || !out)
        return false;

    ops = binding_expr_ops_for_kind(expr->kind);
    return ops && ops->exact_complex ? ops->exact_complex(expr, out) : false;
}

static bool binding_number_text_exact_complex(const char *text,
                                              binding_exact_complex_t *out)
{
    size_t len;
    char *coeff_text = NULL;
    number_t value;

    if (!text || !out)
        return false;

    len = strlen(text);
    if (len > 0u && (text[len - 1u] == 'i' || text[len - 1u] == 'I')) {
        if (len == 1u) {
            value = num_clone(NUM_ONE);
        } else {
            coeff_text = dv_tostring_xstrdup(text);
            coeff_text[len - 1u] = '\0';
            if (strcmp(coeff_text, "+") == 0)
                value = num_clone(NUM_ONE);
            else if (strcmp(coeff_text, "-") == 0)
                value = num_clone(NUM_NEG_ONE);
            else
                value = binding_number_from_text(coeff_text);
            free(coeff_text);
        }
        if (!num_is_exact(value) || !num_is_real(value)) {
            num_destroy(&value);
            return false;
        }
        binding_exact_complex_set(out, num_clone(NUM_ZERO), value);
        return true;
    }

    value = binding_number_from_text(text);
    if (!num_is_exact(value) || !num_is_real(value)) {
        num_destroy(&value);
        return false;
    }
    binding_exact_complex_set(out, value, num_clone(NUM_ZERO));
    return true;
}

static bool binding_exact_complex_number(const dv_binding_expr_t *expr,
                                         binding_exact_complex_t *out)
{
    return binding_number_text_exact_complex(expr->u.text, out);
}

static bool binding_exact_complex_const(const dv_binding_expr_t *expr,
                                        binding_exact_complex_t *out)
{
    if (expr->u.const_id != DV_BINDING_CONST_I)
        return false;
    binding_exact_complex_set(out, num_clone(NUM_ZERO), num_clone(NUM_ONE));
    return true;
}

static bool binding_exact_complex_unary(const dv_binding_expr_t *expr,
                                        binding_exact_complex_t *out)
{
    binding_exact_complex_t child;

    if (!dv_binding_expr_exact_complex(expr->u.unary.child, &child))
        return false;

    binding_exact_complex_set(out, num_neg(child.real), num_neg(child.imag));
    dv_binding_exact_complex_clear(&child);
    return true;
}

static bool binding_exact_complex_addsub(const dv_binding_expr_t *expr,
                                         binding_exact_complex_t *out)
{
    binding_exact_complex_t left;
    binding_exact_complex_t right;
    bool subtract = expr->kind == DV_BINDING_EXPR_SUB;

    if (!dv_binding_expr_exact_complex(expr->u.binary.left, &left))
        return false;
    if (!dv_binding_expr_exact_complex(expr->u.binary.right, &right)) {
        dv_binding_exact_complex_clear(&left);
        return false;
    }

    binding_exact_complex_set(out,
                              subtract ? num_sub(left.real, right.real)
                                       : num_add(left.real, right.real),
                              subtract ? num_sub(left.imag, right.imag)
                                       : num_add(left.imag, right.imag));
    dv_binding_exact_complex_clear(&right);
    dv_binding_exact_complex_clear(&left);
    return true;
}

static bool binding_exact_complex_mul(const dv_binding_expr_t *expr,
                                      binding_exact_complex_t *out)
{
    NUM_SCOPE(scope);
    binding_exact_complex_t left;
    binding_exact_complex_t right;

    if (!dv_binding_expr_exact_complex(expr->u.binary.left, &left))
        return false;
    if (!dv_binding_expr_exact_complex(expr->u.binary.right, &right)) {
        dv_binding_exact_complex_clear(&left);
        return false;
    }

    binding_exact_complex_set(out,
        num_sub(num_mul(left.real, right.real), num_mul(left.imag, right.imag)),
        num_add(num_mul(left.real, right.imag), num_mul(left.imag, right.real)));
    dv_binding_exact_complex_clear(&right);
    dv_binding_exact_complex_clear(&left);
    return true;
}

static bool binding_exact_complex_div(const dv_binding_expr_t *expr,
                                      binding_exact_complex_t *out)
{
    NUM_SCOPE(scope);
    binding_exact_complex_t left;
    binding_exact_complex_t right;
    number_t denom;

    if (!dv_binding_expr_exact_complex(expr->u.binary.left, &left))
        return false;
    if (!dv_binding_expr_exact_complex(expr->u.binary.right, &right)) {
        dv_binding_exact_complex_clear(&left);
        return false;
    }

    denom = num_add(num_mul(right.real, right.real),
                    num_mul(right.imag, right.imag));
    if (num_is_zero(denom)) {
        dv_binding_exact_complex_clear(&right);
        dv_binding_exact_complex_clear(&left);
        return false;
    }

    binding_exact_complex_set(out,
        num_div(num_add(num_mul(left.real, right.real),
                        num_mul(left.imag, right.imag)), denom),
        num_div(num_sub(num_mul(left.imag, right.real),
                        num_mul(left.real, right.imag)), denom));
    dv_binding_exact_complex_clear(&right);
    dv_binding_exact_complex_clear(&left);
    return true;
}

static bool binding_exact_complex_powi(const dv_binding_expr_t *expr,
                                       binding_exact_complex_t *out)
{
    binding_exact_complex_t result;
    binding_exact_complex_t base;

    if (expr->u.powi.exponent < 0 ||
        !dv_binding_expr_exact_complex(expr->u.powi.base, &base))
        return false;

    binding_exact_complex_set(&result, num_clone(NUM_ONE), num_clone(NUM_ZERO));
    for (long i = 0; i < expr->u.powi.exponent; ++i) {
        NUM_SCOPE(scope);
        binding_exact_complex_t next;

        binding_exact_complex_set(&next,
            num_sub(num_mul(result.real, base.real),
                    num_mul(result.imag, base.imag)),
            num_add(num_mul(result.real, base.imag),
                    num_mul(result.imag, base.real)));
        dv_binding_exact_complex_clear(&result);
        result = next;
    }

    dv_binding_exact_complex_clear(&base);
    *out = result;
    return true;
}

long binding_gcd_long(long a, long b)
{
    if (a < 0L)
        a = -a;
    if (b < 0L)
        b = -b;
    while (b != 0L) {
        long t = a % b;

        a = b;
        b = t;
    }
    return a;
}

bool binding_expr_scaled_const_ratio(const dv_binding_expr_t *expr,
                                     long *numer_out,
                                     long *denom_out,
                                     dv_binding_const_id_t *const_id_out);

static bool binding_expr_scaled_const(const dv_binding_expr_t *expr,
                                      long *factor_out,
                                      dv_binding_const_id_t *const_id_out)
{
    long numer;
    long denom;
    dv_binding_const_id_t const_id;

    if (!binding_expr_scaled_const_ratio(expr, &numer, &denom, &const_id) ||
        denom != 1L)
        return false;

    *factor_out = numer;
    *const_id_out = const_id;
    return true;
}

bool binding_expr_scaled_const_ratio(const dv_binding_expr_t *expr,
                                     long *numer_out,
                                     long *denom_out,
                                     dv_binding_const_id_t *const_id_out)
{
    long factor_numer;
    long factor_denom;
    long nested_numer;
    long nested_denom;
    long gcd;
    dv_binding_const_id_t const_id;

    if (!expr)
        return false;

    if (expr->kind == DV_BINDING_EXPR_CONST) {
        *numer_out = 1L;
        *denom_out = 1L;
        *const_id_out = expr->u.const_id;
        return true;
    }

    if (expr->kind == DV_BINDING_EXPR_NEG &&
        binding_expr_scaled_const_ratio(expr->u.unary.child,
                                        &nested_numer,
                                        &nested_denom,
                                        &const_id)) {
        *numer_out = -nested_numer;
        *denom_out = nested_denom;
        *const_id_out = const_id;
        return true;
    }

    if (expr->kind != DV_BINDING_EXPR_MUL)
        return false;

    if (!binding_number_text_to_small_rational(expr->u.binary.left,
                                               &factor_numer,
                                               &factor_denom) ||
        !binding_expr_scaled_const_ratio(expr->u.binary.right,
                                         &nested_numer,
                                         &nested_denom,
                                         &const_id)) {
        if (!binding_number_text_to_small_rational(expr->u.binary.right,
                                                   &factor_numer,
                                                   &factor_denom) ||
            !binding_expr_scaled_const_ratio(expr->u.binary.left,
                                             &nested_numer,
                                             &nested_denom,
                                             &const_id))
            return false;
    }

    factor_numer *= nested_numer;
    factor_denom *= nested_denom;
    if (factor_denom < 0L) {
        factor_numer = -factor_numer;
        factor_denom = -factor_denom;
    }
    gcd = binding_gcd_long(factor_numer, factor_denom);
    if (gcd > 1L) {
        factor_numer /= gcd;
        factor_denom /= gcd;
    }
    *numer_out = factor_numer;
    *denom_out = factor_denom;
    *const_id_out = const_id;
    return true;
}

bool binding_const_ratio_parts(const dv_binding_expr_t *numer_expr,
                               const dv_binding_expr_t *denom_expr,
                               long *numer_out,
                               long *denom_out,
                               dv_binding_const_id_t *const_id_out)
{
    long numer;
    long denom;
    long gcd;
    dv_binding_const_id_t const_id;

    if (!binding_expr_scaled_const(numer_expr, &numer, &const_id) ||
        !binding_number_text_to_long(denom_expr, &denom) ||
        denom == 0L || numer == 0L)
        return false;

    if (denom < 0L) {
        numer = -numer;
        denom = -denom;
    }
    gcd = binding_gcd_long(numer, denom);
    if (gcd > 1L) {
        numer /= gcd;
        denom /= gcd;
    }
    *numer_out = numer;
    *denom_out = denom;
    *const_id_out = const_id;
    return true;
}

static dval_t *binding_eval_known_pi_ratio(const dv_binding_expr_t *numer,
                                           const dv_binding_expr_t *denom)
{
    dval_t *node = NULL;

    if (!binding_expr_is_const_id(numer, DV_BINDING_CONST_PI))
        return NULL;

    if (binding_number_text_eq_long(denom, 2))
        node = dv_new_const(NUM_PI_2);
    else if (binding_number_text_eq_long(denom, 3))
        node = dv_new_const(NUM_PI_3);
    else if (binding_number_text_eq_long(denom, 4))
        node = dv_new_const(NUM_PI_4);
    else if (binding_number_text_eq_long(denom, 6))
        node = dv_new_const(NUM_PI_6);

    return node;
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

static dval_t *binding_eval_unary_op(const dv_binding_expr_t *expr)
{
    dval_t *child = dv_binding_expr_eval_dval(expr->u.unary_op.child);
    dval_t *node;

    if (!expr->u.unary_op.ops || !expr->u.unary_op.ops->apply_unary) {
        dv_free(child);
        return dv_new_const(NUM_NAN);
    }

    node = expr->u.unary_op.ops->apply_unary(child);
    dv_free(child);
    return node ? node : dv_new_const(NUM_NAN);
}

static dval_t *binding_eval_binary_op(const dv_binding_expr_t *expr)
{
    dval_t *left = dv_binding_expr_eval_dval(expr->u.binary_op.left);
    dval_t *right = dv_binding_expr_eval_dval(expr->u.binary_op.right);
    dval_t *node;

    if (!expr->u.binary_op.ops || !expr->u.binary_op.ops->apply_binary) {
        dv_free(left);
        dv_free(right);
        return dv_new_const(NUM_NAN);
    }

    node = expr->u.binary_op.ops->apply_binary(left, right);
    dv_free(left);
    dv_free(right);
    return node ? node : dv_new_const(NUM_NAN);
}

static void binding_expr_store_cached_value(dv_binding_expr_t *expr,
                                            number_t value,
                                            size_t precision_bits)
{
    if (expr->cached_value_valid)
        num_destroy(&expr->cached_value);
    expr->cached_value = num_scope_detach(num_clone(value));
    expr->cached_precision_bits = precision_bits;
    expr->cached_value_valid = true;
}

static number_t binding_expr_compute_value(const dv_binding_expr_t *expr)
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

number_t dv_binding_expr_eval(const dv_binding_expr_t *expr)
{
    dv_binding_expr_t *mutable_expr = (dv_binding_expr_t *)expr;
    number_t value;

    if (!mutable_expr)
        return num_clone(NUM_NAN);

    if (mutable_expr->cached_value_valid &&
        mutable_expr->cached_precision_bits >= num_get_default_prec_bits()) {
        return num_clone(mutable_expr->cached_value);
    }

    value = binding_expr_compute_value(mutable_expr);
    binding_expr_store_cached_value(mutable_expr, value,
                                    num_get_default_prec_bits());
    return value;
}

bool dv_binding_expr_eval_if_precision_increased(dv_binding_expr_t *expr,
                                                 number_t *value_out)
{
    number_t value;
    size_t precision_bits;

    if (!expr || !value_out)
        return false;

    precision_bits = num_get_default_prec_bits();
    if (expr->cached_value_valid &&
        expr->cached_precision_bits >= precision_bits) {
        return false;
    }

    value = binding_expr_compute_value(expr);
    binding_expr_store_cached_value(expr, value, precision_bits);
    *value_out = value;
    return true;
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

typedef struct {
    const char       *kw;
    size_t            klen;
    bool              is_binary;
    const dval_ops_t *ops;
} binding_func_entry_t;

#define BINDING_FUNC_TABLE_SIZE 71u
#define BINDING_FUNC_ENTRY(name, binary, op) \
    { (name), sizeof(name) - 1u, (binary), (op) }

static const unsigned char s_binding_func_displacements[BINDING_FUNC_TABLE_SIZE] = {
    0, 0, 0, 0, 0, 0, 1, 0, 2, 0,
    0, 7, 1, 6, 0, 2, 0, 0, 12, 3,
    0, 1, 1, 2, 6, 1, 0, 0, 2, 1,
    0, 20, 17, 3, 32, 0, 0, 2, 1, 6,
    37, 0, 0, 0, 2, 0, 0, 0, 0
};

static const binding_func_entry_t s_binding_funcs[BINDING_FUNC_TABLE_SIZE] = {
    BINDING_FUNC_ENTRY("atan",          false, &ops_atan),
    BINDING_FUNC_ENTRY("lambert_wm1",   false, &ops_lambert_wm1),
    BINDING_FUNC_ENTRY("erf",           false, &ops_erf),
    BINDING_FUNC_ENTRY("abs",           false, &ops_abs),
    BINDING_FUNC_ENTRY("sinh",          false, &ops_sinh),
    BINDING_FUNC_ENTRY("erfc",          false, &ops_erfc),
    BINDING_FUNC_ENTRY("W0",            false, &ops_lambert_w0),
    BINDING_FUNC_ENTRY("erfcinv",       false, &ops_erfcinv),
    BINDING_FUNC_ENTRY("ln",            false, &ops_log),
    BINDING_FUNC_ENTRY("lambert_w0",    false, &ops_lambert_w0),
    BINDING_FUNC_ENTRY("erfinv",        false, &ops_erfinv),
    BINDING_FUNC_ENTRY("cos",           false, &ops_cos),
    BINDING_FUNC_ENTRY("log10",         false, &ops_log10),
    BINDING_FUNC_ENTRY("tan",           false, &ops_tan),
    BINDING_FUNC_ENTRY("digamma",       false, &ops_digamma),
    BINDING_FUNC_ENTRY("trigamma",      false, &ops_trigamma),
    BINDING_FUNC_ENTRY("normal_pdf",    false, &ops_normal_pdf),
    BINDING_FUNC_ENTRY("hypot",         true,  &ops_hypot),
    BINDING_FUNC_ENTRY("Ei",            false, &ops_ei),
    BINDING_FUNC_ENTRY("W_0",           false, &ops_lambert_w0),
    BINDING_FUNC_ENTRY("acosh",         false, &ops_acosh),
    BINDING_FUNC_ENTRY("sin",           false, &ops_sin),
    BINDING_FUNC_ENTRY("W₀",            false, &ops_lambert_w0),
    BINDING_FUNC_ENTRY("ceil",          false, &ops_ceil),
    BINDING_FUNC_ENTRY("productlog",    false, &ops_lambert_w),
    BINDING_FUNC_ENTRY("asinh",         false, &ops_asinh),
    BINDING_FUNC_ENTRY("log",           false, &ops_log10),
    BINDING_FUNC_ENTRY("gammainv",      false, &ops_gammainv),
    BINDING_FUNC_ENTRY("gammainc_lower", true, &ops_gammainc_lower),
    BINDING_FUNC_ENTRY("gammainc_upper", true, &ops_gammainc_upper),
    BINDING_FUNC_ENTRY("gammainc_P",    true,  &ops_gammainc_P),
    BINDING_FUNC_ENTRY("gammainc_Q",    true,  &ops_gammainc_Q),
    BINDING_FUNC_ENTRY("W_-1",          false, &ops_lambert_wm1),
    BINDING_FUNC_ENTRY("tanh",          false, &ops_tanh),
    BINDING_FUNC_ENTRY("logbeta",       true,  &ops_logbeta),
    BINDING_FUNC_ENTRY("gamma",         false, &ops_gamma),
    BINDING_FUNC_ENTRY("E1",            false, &ops_e1),
    BINDING_FUNC_ENTRY("W₋₁",           false, &ops_lambert_wm1),
    BINDING_FUNC_ENTRY("floor",         false, &ops_floor),
    BINDING_FUNC_ENTRY("normal_cdf",    false, &ops_normal_cdf),
    BINDING_FUNC_ENTRY("atanh",         false, &ops_atanh),
    BINDING_FUNC_ENTRY("asin",          false, &ops_asin),
    BINDING_FUNC_ENTRY("acos",          false, &ops_acos),
    BINDING_FUNC_ENTRY("exp",           false, &ops_exp),
    BINDING_FUNC_ENTRY("pow",           true,  &ops_pow),
    BINDING_FUNC_ENTRY("W-1",           false, &ops_lambert_wm1),
    BINDING_FUNC_ENTRY("cosh",          false, &ops_cosh),
    BINDING_FUNC_ENTRY("sqrt",          false, &ops_sqrt),
    BINDING_FUNC_ENTRY("lgamma",        false, &ops_lgamma),
    BINDING_FUNC_ENTRY("W",             false, &ops_lambert_w),
    BINDING_FUNC_ENTRY("beta",          true,  &ops_beta),
    BINDING_FUNC_ENTRY("atan2",         true,  &ops_atan2),
    BINDING_FUNC_ENTRY("normal_logpdf", false, &ops_normal_logpdf),
    BINDING_FUNC_ENTRY("factorial",     false, &ops_factorial),
    BINDING_FUNC_ENTRY("fibonacci",     false, &ops_fibonacci),
    BINDING_FUNC_ENTRY("partition",     false, &ops_partition),
    BINDING_FUNC_ENTRY("isqrt",         false, &ops_isqrt),
    BINDING_FUNC_ENTRY("gcd",           true,  &ops_gcd),
    BINDING_FUNC_ENTRY("lcm",           true,  &ops_lcm),
    BINDING_FUNC_ENTRY("mod",           true,  &ops_mod),
    BINDING_FUNC_ENTRY("modinv",        true,  &ops_modinv),
    BINDING_FUNC_ENTRY("is_prime",      false, &ops_is_prime),
    BINDING_FUNC_ENTRY("next_prime",    false, &ops_next_prime),
    BINDING_FUNC_ENTRY("prev_prime",    false, &ops_prev_prime),
    BINDING_FUNC_ENTRY("AND",           true,  &ops_bit_and),
    BINDING_FUNC_ENTRY("OR",            true,  &ops_bit_or),
    BINDING_FUNC_ENTRY("XOR",           true,  &ops_bit_xor),
    BINDING_FUNC_ENTRY("NOT",           false, &ops_bit_not),
    BINDING_FUNC_ENTRY("SHL",           true,  &ops_shl),
    BINDING_FUNC_ENTRY("SHR",           true,  &ops_shr),
    BINDING_FUNC_ENTRY("factors",       false, &ops_factors)
};

static unsigned binding_func_bucket_hash(const char *kw, size_t klen)
{
    const unsigned char *s = (const unsigned char *)kw;
    unsigned h = (unsigned)klen + s[0] + s[klen - 1u];

    for (size_t i = 1u; i + 1u < klen; ++i)
        h += (unsigned)(i + 1u) * s[i];

    return h % BINDING_FUNC_TABLE_SIZE;
}

static unsigned binding_func_slot_hash(const char *kw, size_t klen)
{
    const unsigned char *s = (const unsigned char *)kw;
    unsigned h = s[0] + s[klen - 1u];

    for (size_t i = 0u; i < klen; ++i)
        h += (unsigned)(i + 3u) * s[i];

    return h % BINDING_FUNC_TABLE_SIZE;
}

static const binding_func_entry_t *binding_func_lookup(const char *kw, size_t klen)
{
    const binding_func_entry_t *entry;
    unsigned bucket;
    unsigned slot;

    if (klen == 0u)
        return NULL;

    bucket = binding_func_bucket_hash(kw, klen);
    slot = (binding_func_slot_hash(kw, klen) +
            s_binding_func_displacements[bucket]) % BINDING_FUNC_TABLE_SIZE;
    entry = &s_binding_funcs[slot];

    if (entry->klen == klen && memcmp(entry->kw, kw, klen) == 0)
        return entry;

    for (size_t i = 0u; i < BINDING_FUNC_TABLE_SIZE; ++i) {
        entry = &s_binding_funcs[i];
        if (entry->klen == klen && memcmp(entry->kw, kw, klen) == 0)
            return entry;
    }

    return NULL;
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
    if (s < end && (unsigned char)s[0] == 0xE2 &&
        (size_t)(end - s) >= 3u &&
        (unsigned char)s[1] == 0x88 &&
        (unsigned char)s[2] == 0x9E)
        return 3u;
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

    if (len == 3u &&
        (unsigned char)start[0] == 0xE2 &&
        (unsigned char)start[1] == 0x88 &&
        (unsigned char)start[2] == 0x9E) {
        *out = num_clone(NUM_INF);
        return 1;
    }

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

    if (*p->p == '(' || *p->p == '+' || *p->p == '-' || *p->p == '|')
        return 1;
    if (isdigit((unsigned char)*p->p) || *p->p == '.')
        return 1;
    if (scan_unicode_fraction_len(p->p, p->end) > 0u)
        return 1;

    {
        unsigned int cp;
        int len = scan_utf8_codepoint(p->p, p->end, &cp);

        if (len > 0 && (cp == 0x221A || cp == 0x230A || cp == 0x2308))
            return 1;
    }

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

/* True if we're at the middle dot · (U+00B7, UTF-8: 0xC2 0xB7). */
static int binding_at_middle_dot(const binding_parser_t *p)
{
    return p->p + 1 < p->end &&
           (unsigned char)p->p[0] == 0xC2 &&
           (unsigned char)p->p[1] == 0xB7;
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
static dv_binding_expr_t *parse_binding_signed(binding_parser_t *p);

static int parse_binding_required_char(binding_parser_t *p, char expected, const char *errmsg)
{
    binding_skip_spaces(p);
    if (p->p >= p->end || *p->p != expected) {
        binding_set_error(p, errmsg);
        return 0;
    }
    p->p++;
    return 1;
}

static dv_binding_expr_t *parse_binding_enclosed_expr(binding_parser_t *p,
                                                      char closing,
                                                      const char *errmsg)
{
    dv_binding_expr_t *inner = parse_binding_addexpr(p);

    if (!inner)
        return NULL;
    if (!parse_binding_required_char(p, closing, errmsg)) {
        dv_binding_expr_free(inner);
        return NULL;
    }

    return inner;
}

static int parse_binding_two_args(binding_parser_t *p,
                                  dv_binding_expr_t **a_out,
                                  dv_binding_expr_t **b_out)
{
    dv_binding_expr_t *a = parse_binding_addexpr(p);
    dv_binding_expr_t *b;

    if (!a)
        return 0;
    if (!parse_binding_required_char(p, ',', "expected ',' in binary function")) {
        dv_binding_expr_free(a);
        return 0;
    }
    b = parse_binding_addexpr(p);
    if (!b) {
        dv_binding_expr_free(a);
        return 0;
    }

    *a_out = a;
    *b_out = b;
    return 1;
}

static const binding_func_entry_t *parse_binding_function_head(binding_parser_t *p,
                                                               const char **paren_out)
{
    const char *id = p->p;
    const char *id_end = id;

    while (id_end < p->end &&
           (isalpha((unsigned char)*id_end) ||
            isdigit((unsigned char)*id_end) ||
            *id_end == '_' ||
            *id_end == '-'))
        id_end++;

    if (id_end > id) {
        const binding_func_entry_t *entry =
            binding_func_lookup(id, (size_t)(id_end - id));

        if (entry) {
            const char *q = id_end;

            binding_parser_t tmp = *p;
            tmp.p = q;
            binding_skip_spaces(&tmp);
            if (tmp.p < tmp.end && *tmp.p == '(') {
                *paren_out = tmp.p;
                return entry;
            }
        }
    }

    {
        static const char *const unicode_aliases[] = { "W₀", "W₋₁" };

        for (size_t i = 0u; i < sizeof(unicode_aliases) / sizeof(unicode_aliases[0]); ++i) {
            size_t len = strlen(unicode_aliases[i]);

            if ((size_t)(p->end - p->p) < len ||
                memcmp(p->p, unicode_aliases[i], len) != 0)
                continue;

            const binding_func_entry_t *entry = binding_func_lookup(unicode_aliases[i], len);
            binding_parser_t tmp = *p;

            tmp.p += len;
            binding_skip_spaces(&tmp);
            if (entry && tmp.p < tmp.end && *tmp.p == '(') {
                *paren_out = tmp.p;
                return entry;
            }
        }
    }

    *paren_out = NULL;
    return NULL;
}

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

    if (*p->p == '|') {
        dv_binding_expr_t *inner;

        p->p++;
        inner = parse_binding_enclosed_expr(p, '|', "expected '|'");
        return inner ? dv_binding_expr_new_unary_op(&ops_abs, inner) : NULL;
    }

    if (*p->p == '?') {
        p->p++;
        return dv_binding_expr_new_number_text("NAN");
    }

    {
        unsigned int cp = 0;
        int cp_len = scan_utf8_codepoint(p->p, p->end, &cp);

        if (cp_len > 0 && cp == 0x221A) {
            dv_binding_expr_t *arg;

            p->p += cp_len;
            if (!parse_binding_required_char(p, '(', "expected '(' after √"))
                return NULL;
            arg = parse_binding_enclosed_expr(p, ')', "expected ')' after √ argument");
            return arg ? dv_binding_expr_new_unary_op(&ops_sqrt, arg) : NULL;
        }

        if (cp_len > 0 && (cp == 0x230A || cp == 0x2308)) {
            const dval_ops_t *ops = (cp == 0x230A) ? &ops_floor : &ops_ceil;
            unsigned int close_cp = 0;
            const unsigned int closing = (cp == 0x230A) ? 0x230B : 0x2309;
            const char *errmsg = (cp == 0x230A) ? "expected '⌋'" : "expected '⌉'";
            dv_binding_expr_t *inner;
            int close_len;

            p->p += cp_len;
            inner = parse_binding_addexpr(p);
            if (!inner)
                return NULL;
            binding_skip_spaces(p);
            close_len = scan_utf8_codepoint(p->p, p->end, &close_cp);
            if (close_len <= 0 || close_cp != closing) {
                dv_binding_expr_free(inner);
                binding_set_error(p, errmsg);
                return NULL;
            }
            p->p += close_len;
            return dv_binding_expr_new_unary_op(ops, inner);
        }
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
        const char *paren = NULL;
        const binding_func_entry_t *func = parse_binding_function_head(p, &paren);

        if (func && paren) {
            p->p = paren + 1;
            if (func->is_binary) {
                dv_binding_expr_t *a = NULL;
                dv_binding_expr_t *b = NULL;

                if (!parse_binding_two_args(p, &a, &b))
                    return NULL;
                if (!parse_binding_required_char(p, ')', "expected ')' after binary function")) {
                    dv_binding_expr_free(a);
                    dv_binding_expr_free(b);
                    return NULL;
                }
                return dv_binding_expr_new_binary_op(func->ops, a, b);
            } else {
                dv_binding_expr_t *arg =
                    parse_binding_enclosed_expr(p, ')', "expected ')' after function argument");

                return arg ? dv_binding_expr_new_unary_op(func->ops, arg) : NULL;
            }
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

    if (!base)
        return NULL;

    binding_skip_spaces(p);
    if (p->p >= p->end || *p->p != '^')
        return base;

    p->p++;
    {
        dv_binding_expr_t *exponent;
        long exponent_long;

        binding_skip_spaces(p);
        if (p->p < p->end && *p->p == '(') {
            p->p++;
            exponent = parse_binding_enclosed_expr(p, ')', "expected ')' after exponent");
        } else {
            exponent = parse_binding_signed(p);
        }

        if (!exponent) {
            dv_binding_expr_free(base);
            binding_set_error(p, "expected exponent after '^'");
            return NULL;
        }

        if (binding_number_text_to_long(exponent, &exponent_long)) {
            dv_binding_expr_free(exponent);
            return dv_binding_expr_new_powi(base, exponent_long);
        }

        return dv_binding_expr_new_binary_op(&ops_pow, base, exponent);
    }
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
        if (binding_at_middle_dot(p)) {
            op = '*';
            p->p += 2;
        } else if (*peek == '*' || *peek == '/') {
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

    if (expr->kind == DV_BINDING_EXPR_NUMBER) {
        number_t value = num_create_from_string(expr->u.text);
        bool atomic = !num_is_nan(value) &&
                      (num_is_real(value) || num_eq(value, NUM_I));

        num_destroy(&value);
        return atomic;
    }

    ops = binding_expr_ops_for_kind(expr->kind);
    return ops ? ops->atomic : false;
}

static bool binding_expr_needs_pow_base_parens(const dv_binding_expr_t *expr)
{
    number_t value;
    bool need;

    if (!expr)
        return false;
    if (binding_expr_prec(expr) < BIND_PREC_POW)
        return true;
    if (expr->kind != DV_BINDING_EXPR_NUMBER)
        return false;

    value = num_create_from_string(expr->u.text);
    need = !num_is_real(value) || num_lt(value, NUM_ZERO);
    num_destroy(&value);
    return need;
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

static void binding_trim_decimal_display_artifacts(char *text)
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

static void emit_binding_number_text(const char *text, sbuf_t *b)
{
    char *clean;
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
        if (denom_len == 1u && denom[0] == '1') {
            for (size_t i = 0u; i < numer_len; ++i)
                sbuf_putc(b, numer[i]);
            return;
        }
        emit_binding_unicode_digits(b, numer, numer_len, s_binding_sup_digits);
        sbuf_puts(b, "⁄");
        emit_binding_unicode_digits(b, denom, denom_len, s_binding_sub_digits);
        return;
    }

    clean = dv_tostring_xstrdup(text ? text : "");
    binding_trim_decimal_display_artifacts(clean);
    sbuf_puts(b, clean);
    free(clean);
}

static void emit_binding_expr(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_expr(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);

static bool binding_number_value_unary(const dv_binding_expr_t *expr,
                                       binding_number_unary_fn op,
                                       number_t *out)
{
    number_t child;

    if (!dv_binding_expr_number_value(expr->u.unary.child, &child))
        return false;
    *out = num_scope_detach(op(child));
    num_destroy(&child);
    return true;
}

static bool binding_number_value_binary(const dv_binding_expr_t *expr,
                                        binding_number_binary_fn op,
                                        number_t *out)
{
    number_t left;
    number_t right;

    if (!dv_binding_expr_number_value(expr->u.binary.left, &left))
        return false;
    if (!dv_binding_expr_number_value(expr->u.binary.right, &right)) {
        num_destroy(&left);
        return false;
    }
    *out = num_scope_detach(op(left, right));
    num_destroy(&right);
    num_destroy(&left);
    return true;
}

static bool binding_number_value_number(const dv_binding_expr_t *expr,
                                        number_t *out)
{
    *out = num_scope_detach(binding_number_from_text(expr->u.text));
    return true;
}

static bool binding_number_value_false(const dv_binding_expr_t *expr,
                                       number_t *out)
{
    (void)expr;
    (void)out;
    return false;
}

static bool binding_number_value_neg(const dv_binding_expr_t *expr,
                                     number_t *out)
{
    return binding_number_value_unary(expr, num_neg, out);
}

static bool binding_number_value_add(const dv_binding_expr_t *expr,
                                     number_t *out)
{
    return binding_number_value_binary(expr, num_add, out);
}

static bool binding_number_value_sub(const dv_binding_expr_t *expr,
                                     number_t *out)
{
    return binding_number_value_binary(expr, num_sub, out);
}

static bool binding_number_value_mul(const dv_binding_expr_t *expr,
                                     number_t *out)
{
    return binding_number_value_binary(expr, num_mul, out);
}

static bool binding_number_value_div(const dv_binding_expr_t *expr,
                                     number_t *out)
{
    return binding_number_value_binary(expr, num_div, out);
}

static bool binding_number_value_powi(const dv_binding_expr_t *expr,
                                      number_t *out)
{
    number_t base;

    if (!dv_binding_expr_number_value(expr->u.powi.base, &base))
        return false;
    *out = num_scope_detach(num_pow_int(base, (int)expr->u.powi.exponent));
    num_destroy(&base);
    return true;
}

bool dv_binding_expr_number_value(const dv_binding_expr_t *expr, number_t *out)
{
    const binding_expr_ops_t *ops;

    if (!expr || !out)
        return false;

    ops = binding_expr_ops_for_kind(expr->kind);
    return ops && ops->number_value ? ops->number_value(expr, out) : false;
}

static bool binding_expr_leading_number(const dv_binding_expr_t *expr,
                                        number_t *coeff_out,
                                        const dv_binding_expr_t **rest_out)
{
    if (dv_binding_expr_number_value(expr, coeff_out)) {
        *rest_out = NULL;
        return true;
    }

    if (expr && expr->kind == DV_BINDING_EXPR_MUL &&
        dv_binding_expr_number_value(expr->u.binary.left, coeff_out)) {
        *rest_out = expr->u.binary.right;
        return true;
    }

    return false;
}

bool dv_binding_expr_split_leading_number(const dv_binding_expr_t *expr,
                                          number_t *coeff_out,
                                          dv_binding_expr_t **rest_out)
{
    number_t coeff;
    number_t right_coeff;
    number_t folded;
    const dv_binding_expr_t *rest = NULL;
    long numer;
    long denom;
    dv_binding_const_id_t const_id;

    if (!expr || !coeff_out || !rest_out)
        return false;

    if (dv_binding_expr_number_value(expr, &coeff)) {
        *coeff_out = coeff;
        *rest_out = NULL;
        return true;
    }

    if (expr->kind == DV_BINDING_EXPR_MUL) {
        if (dv_binding_expr_number_value(expr->u.binary.left, &coeff)) {
            *coeff_out = coeff;
            *rest_out = dv_binding_expr_clone(expr->u.binary.right);
            return true;
        }
        if (dv_binding_expr_number_value(expr->u.binary.right, &coeff)) {
            *coeff_out = coeff;
            *rest_out = dv_binding_expr_clone(expr->u.binary.left);
            return true;
        }
    }

    if (expr->kind == DV_BINDING_EXPR_DIV &&
        dv_binding_expr_number_value(expr->u.binary.right, &right_coeff)) {
        if (dv_binding_expr_split_leading_number(expr->u.binary.left, &coeff, rest_out)) {
            folded = num_scope_detach(num_div(coeff, right_coeff));
            num_destroy(&coeff);
            num_destroy(&right_coeff);
            *coeff_out = folded;
            return true;
        }
        num_destroy(&right_coeff);
    }

    if (expr->kind == DV_BINDING_EXPR_DIV &&
        binding_const_ratio_parts(expr->u.binary.left,
                                  expr->u.binary.right,
                                  &numer,
                                  &denom,
                                  &const_id)) {
        number_t numer_value = num_create_from_long(numer);
        number_t denom_value = num_create_from_long(denom);

        *coeff_out = num_scope_detach(num_div(numer_value, denom_value));
        num_destroy(&denom_value);
        num_destroy(&numer_value);
        *rest_out = dv_binding_expr_new_const(const_id);
        return true;
    }

    if (binding_expr_leading_number(expr, &coeff, &rest)) {
        *coeff_out = coeff;
        *rest_out = rest ? dv_binding_expr_clone(rest) : NULL;
        return true;
    }

    return false;
}


dv_binding_expr_t *dv_binding_expr_simplify(dv_binding_expr_t *expr)
{
    const binding_expr_ops_t *ops;

    if (!expr)
        return NULL;

    ops = binding_expr_ops_for_kind(expr->kind);
    return ops && ops->simplify ? ops->simplify(expr) : expr;
}

static void emit_binding_expr_mul_separator(const dv_binding_expr_t *left,
                                            const dv_binding_expr_t *right,
                                            sbuf_t *b)
{
    if (!(binding_expr_is_atomic(left) && binding_expr_is_atomic(right)))
        sbuf_puts(b, "·");
}

static void emit_binding_tex_mul_separator(const dv_binding_expr_t *left,
                                           const dv_binding_expr_t *right,
                                           sbuf_t *b)
{
    if (left && left->kind == DV_BINDING_EXPR_NUMBER &&
        binding_expr_is_const_id(right, DV_BINDING_CONST_I))
        return;
    if (binding_expr_is_atomic(left) && binding_expr_is_atomic(right))
        sbuf_putc(b, ' ');
    else
        sbuf_puts(b, " \\cdot ");
}

static void emit_binding_expr_mul(const dv_binding_expr_t *left,
                                  const dv_binding_expr_t *right,
                                  sbuf_t *b,
                                  int parent_prec)
{
    bool need = BIND_PREC_MUL < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    emit_binding_expr(left, b, BIND_PREC_MUL);
    emit_binding_expr_mul_separator(left, right, b);
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
    emit_binding_tex_mul_separator(left, right, b);
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

static void emit_binding_expr_const_ratio_value(dv_binding_const_id_t const_id,
                                                long numer,
                                                long denom,
                                                sbuf_t *b)
{
    long abs_numer = numer < 0L ? -numer : numer;

    if (numer < 0L)
        sbuf_putc(b, '-');
    if (abs_numer != 1L) {
        char nbuf[32];

        snprintf(nbuf, sizeof(nbuf), "%ld", abs_numer);
        sbuf_puts(b, nbuf);
    }
    sbuf_puts(b, binding_const_expr_name(const_id));
    if (denom != 1L) {
        char dbuf[32];

        snprintf(dbuf, sizeof(dbuf), "%ld", denom);
        sbuf_putc(b, '/');
        sbuf_puts(b, dbuf);
    }
}

static void emit_binding_expr_div(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = BIND_PREC_MUL < parent_prec;
    long numer;
    long denom;
    dv_binding_const_id_t const_id;

    if (need)
        sbuf_putc(b, '(');
    if (binding_const_ratio_parts(expr->u.binary.left, expr->u.binary.right,
                                  &numer, &denom, &const_id)) {
        emit_binding_expr_const_ratio_value(const_id, numer, denom, b);
        if (need)
            sbuf_putc(b, ')');
        return;
    }
    emit_binding_expr(expr->u.binary.left, b, BIND_PREC_MUL);
    sbuf_putc(b, '/');
    emit_binding_expr(expr->u.binary.right, b, BIND_PREC_POW);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_expr_powi(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = binding_expr_needs_pow_base_parens(expr->u.powi.base);

    (void)parent_prec;
    if (need)
        sbuf_putc(b, '(');
    emit_binding_expr(expr->u.powi.base, b, need ? BIND_PREC_LOWEST : BIND_PREC_POW);
    if (need)
        sbuf_putc(b, ')');
    emit_binding_superscript_int(b, expr->u.powi.exponent);
}

static void emit_binding_tex_number(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    char *clean;
    char *tex;
    bool negative;
    const char *numer;
    const char *denom;
    size_t numer_len;
    size_t denom_len;

    (void)parent_prec;
    if (expr->u.text && strcmp(expr->u.text, "∞") == 0) {
        sbuf_puts(b, "\\infty");
        return;
    }
    if (expr->u.text && strcmp(expr->u.text, "-∞") == 0) {
        sbuf_puts(b, "-\\infty");
        return;
    }
    if (binding_text_is_simple_rational(expr->u.text, &negative,
                                        &numer, &numer_len,
                                        &denom, &denom_len)) {
        if (negative)
            sbuf_putc(b, '-');
        sbuf_puts(b, "\\frac{");
        for (size_t i = 0u; i < numer_len; ++i)
            sbuf_putc(b, numer[i]);
        sbuf_puts(b, "}{");
        for (size_t i = 0u; i < denom_len; ++i)
            sbuf_putc(b, denom[i]);
        sbuf_putc(b, '}');
        return;
    }

    clean = dv_tostring_xstrdup(expr->u.text ? expr->u.text : "");
    binding_trim_decimal_display_artifacts(clean);
    tex = dv_tostring_texify(clean);
    free(clean);
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

static void emit_binding_tex_const_ratio_numer(dv_binding_const_id_t const_id,
                                               long numer,
                                               sbuf_t *b)
{
    long abs_numer = numer < 0L ? -numer : numer;

    if (numer < 0L)
        sbuf_putc(b, '-');
    if (abs_numer != 1L) {
        char nbuf[32];

        snprintf(nbuf, sizeof(nbuf), "%ld", abs_numer);
        sbuf_puts(b, nbuf);
    }
    sbuf_puts(b, binding_const_tex_name(const_id));
}

static void emit_binding_tex_const_ratio_value(dv_binding_const_id_t const_id,
                                               long numer,
                                               long denom,
                                               sbuf_t *b)
{
    if (denom == 1L) {
        emit_binding_tex_const_ratio_numer(const_id, numer, b);
        return;
    }
    {
        char dbuf[32];

        snprintf(dbuf, sizeof(dbuf), "%ld", denom);
        sbuf_puts(b, "\\frac{");
        emit_binding_tex_const_ratio_numer(const_id, numer, b);
        sbuf_puts(b, "}{");
        sbuf_puts(b, dbuf);
        sbuf_putc(b, '}');
    }
}

static void emit_binding_tex_div(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    long numer;
    long denom;
    dv_binding_const_id_t const_id;

    (void)parent_prec;
    if (binding_const_ratio_parts(expr->u.binary.left, expr->u.binary.right,
                                  &numer, &denom, &const_id)) {
        emit_binding_tex_const_ratio_value(const_id, numer, denom, b);
        return;
    }
    sbuf_puts(b, "\\frac{");
    emit_binding_tex_expr(expr->u.binary.left, b, BIND_PREC_LOWEST);
    sbuf_puts(b, "}{");
    emit_binding_tex_expr(expr->u.binary.right, b, BIND_PREC_LOWEST);
    sbuf_putc(b, '}');
}

static void emit_binding_tex_powi(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = binding_expr_needs_pow_base_parens(expr->u.powi.base);
    char expbuf[64];

    (void)parent_prec;
    if (need)
        sbuf_puts(b, "\\left(");
    emit_binding_tex_expr(expr->u.powi.base, b, need ? BIND_PREC_LOWEST : BIND_PREC_POW);
    if (need)
        sbuf_puts(b, "\\right)");
    snprintf(expbuf, sizeof(expbuf), "%ld", expr->u.powi.exponent);
    sbuf_puts(b, "^{");
    sbuf_puts(b, expbuf);
    sbuf_putc(b, '}');
}

static void emit_binding_expr_unary_call(const dval_ops_t *ops,
                                         const dv_binding_expr_t *child,
                                         sbuf_t *b)
{
    if (ops == &ops_gamma)
        sbuf_puts(b, "Γ");
    else if (ops == &ops_digamma)
        sbuf_puts(b, "ψ⁽⁰⁾");
    else if (ops == &ops_trigamma)
        sbuf_puts(b, "ψ⁽¹⁾");
    else
        sbuf_puts(b, (ops && ops->name) ? ops->name : "?");
    sbuf_putc(b, '(');
    emit_binding_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_binding_tex_unary_call(const dval_ops_t *ops,
                                        const dv_binding_expr_t *child,
                                        sbuf_t *b)
{
    const char *name = (ops && ops->tex_name) ? ops->tex_name : NULL;

    if (!name) {
        sbuf_puts(b, "\\operatorname{");
        sbuf_puts(b, (ops && ops->name) ? ops->name : "?");
        sbuf_putc(b, '}');
    } else {
        sbuf_puts(b, name);
    }
    sbuf_putc(b, '(');
    emit_binding_tex_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_binding_expr_unary_sqrt(const dval_ops_t *ops,
                                         const dv_binding_expr_t *child,
                                         sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "√(");
    emit_binding_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_binding_tex_unary_sqrt(const dval_ops_t *ops,
                                        const dv_binding_expr_t *child,
                                        sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "\\sqrt{");
    emit_binding_tex_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, '}');
}

static void emit_binding_expr_unary_abs(const dval_ops_t *ops,
                                        const dv_binding_expr_t *child,
                                        sbuf_t *b)
{
    (void)ops;
    sbuf_putc(b, '|');
    emit_binding_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, '|');
}

static void emit_binding_tex_unary_abs(const dval_ops_t *ops,
                                       const dv_binding_expr_t *child,
                                       sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "\\left|");
    emit_binding_tex_expr(child, b, BIND_PREC_LOWEST);
    sbuf_puts(b, "\\right|");
}

static void emit_binding_expr_unary_floor(const dval_ops_t *ops,
                                          const dv_binding_expr_t *child,
                                          sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "⌊");
    emit_binding_expr(child, b, BIND_PREC_LOWEST);
    sbuf_puts(b, "⌋");
}

static void emit_binding_tex_unary_floor(const dval_ops_t *ops,
                                         const dv_binding_expr_t *child,
                                         sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "\\left\\lfloor ");
    emit_binding_tex_expr(child, b, BIND_PREC_LOWEST);
    sbuf_puts(b, " \\right\\rfloor");
}

static void emit_binding_expr_unary_ceil(const dval_ops_t *ops,
                                         const dv_binding_expr_t *child,
                                         sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "⌈");
    emit_binding_expr(child, b, BIND_PREC_LOWEST);
    sbuf_puts(b, "⌉");
}

static void emit_binding_tex_unary_ceil(const dval_ops_t *ops,
                                        const dv_binding_expr_t *child,
                                        sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "\\left\\lceil ");
    emit_binding_tex_expr(child, b, BIND_PREC_LOWEST);
    sbuf_puts(b, " \\right\\rceil");
}

static void emit_binding_tex_unary_exp(const dval_ops_t *ops,
                                       const dv_binding_expr_t *child,
                                       sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "e^{");
    emit_binding_tex_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, '}');
}

static void emit_binding_expr_unary_neg_op(const dval_ops_t *ops,
                                           const dv_binding_expr_t *child,
                                           sbuf_t *b)
{
    (void)ops;
    sbuf_putc(b, '-');
    emit_binding_expr(child, b, BIND_PREC_UNARY);
}

static void emit_binding_tex_unary_neg_op(const dval_ops_t *ops,
                                          const dv_binding_expr_t *child,
                                          sbuf_t *b)
{
    (void)ops;
    sbuf_putc(b, '-');
    emit_binding_tex_expr(child, b, BIND_PREC_UNARY);
}

typedef void (*binding_unary_emit_fn)(const dval_ops_t *ops,
                                      const dv_binding_expr_t *child,
                                      sbuf_t *b);

typedef struct {
    const dval_ops_t        *ops;
    binding_unary_emit_fn   emit_expr;
    binding_unary_emit_fn   emit_tex;
} binding_unary_render_t;

static const binding_unary_render_t s_binding_unary_renderers[] = {
    { &ops_neg,   emit_binding_expr_unary_neg_op, emit_binding_tex_unary_neg_op },
    { &ops_sqrt,  emit_binding_expr_unary_sqrt,  emit_binding_tex_unary_sqrt  },
    { &ops_abs,   emit_binding_expr_unary_abs,   emit_binding_tex_unary_abs   },
    { &ops_floor, emit_binding_expr_unary_floor, emit_binding_tex_unary_floor },
    { &ops_ceil,  emit_binding_expr_unary_ceil,  emit_binding_tex_unary_ceil  },
    { &ops_exp,   emit_binding_expr_unary_call,  emit_binding_tex_unary_exp   }
};

static const binding_unary_render_t *binding_unary_renderer_for_ops(const dval_ops_t *ops)
{
    for (size_t i = 0u; i < sizeof(s_binding_unary_renderers) / sizeof(s_binding_unary_renderers[0]); ++i)
        if (s_binding_unary_renderers[i].ops == ops)
            return &s_binding_unary_renderers[i];

    return NULL;
}

static void emit_binding_expr_unary_op(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    const binding_unary_render_t *renderer = binding_unary_renderer_for_ops(expr->u.unary_op.ops);
    bool need = BIND_PREC_UNARY < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    if (renderer && renderer->emit_expr)
        renderer->emit_expr(expr->u.unary_op.ops, expr->u.unary_op.child, b);
    else
        emit_binding_expr_unary_call(expr->u.unary_op.ops, expr->u.unary_op.child, b);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_tex_unary_op(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    const binding_unary_render_t *renderer = binding_unary_renderer_for_ops(expr->u.unary_op.ops);
    bool need = BIND_PREC_UNARY < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    if (renderer && renderer->emit_tex)
        renderer->emit_tex(expr->u.unary_op.ops, expr->u.unary_op.child, b);
    else
        emit_binding_tex_unary_call(expr->u.unary_op.ops, expr->u.unary_op.child, b);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_expr_binary_op(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    const dval_ops_t *ops = expr->u.binary_op.ops;

    if (ops == &ops_pow) {
        bool need = BIND_PREC_POW < parent_prec;
        bool base_need = binding_expr_needs_pow_base_parens(expr->u.binary_op.left);
        long exponent_long;

        if (need)
            sbuf_putc(b, '(');
        if (base_need)
            sbuf_putc(b, '(');
        emit_binding_expr(expr->u.binary_op.left, b, base_need ? BIND_PREC_LOWEST : BIND_PREC_POW);
        if (base_need)
            sbuf_putc(b, ')');
        if (binding_number_text_to_long(expr->u.binary_op.right, &exponent_long)) {
            emit_binding_superscript_int(b, exponent_long);
        } else if (binding_expr_is_atomic(expr->u.binary_op.right)) {
            sbuf_putc(b, '^');
            emit_binding_expr(expr->u.binary_op.right, b, BIND_PREC_POW);
        } else {
            sbuf_putc(b, '^');
            sbuf_putc(b, '(');
            emit_binding_expr(expr->u.binary_op.right, b, BIND_PREC_LOWEST);
            sbuf_putc(b, ')');
        }
        if (need)
            sbuf_putc(b, ')');
        return;
    }

    sbuf_puts(b, (ops && ops->name) ? ops->name : "?");
    sbuf_putc(b, '(');
    emit_binding_expr(expr->u.binary_op.left, b, BIND_PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_binding_expr(expr->u.binary_op.right, b, BIND_PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_binding_func_expr(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec);

static void emit_binding_func_number(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_expr_number(expr, b, parent_prec);
}

static void emit_binding_func_const(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_expr_const(expr, b, parent_prec);
}

static void emit_binding_func_neg(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = BIND_PREC_UNARY < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    sbuf_putc(b, '-');
    emit_binding_func_expr(expr->u.unary.child, b, BIND_PREC_UNARY);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_func_addsub(const dv_binding_expr_t *expr,
                                     sbuf_t *b,
                                     int parent_prec,
                                     const char *op,
                                     int right_prec)
{
    bool need = BIND_PREC_ADD < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    emit_binding_func_expr(expr->u.binary.left, b, BIND_PREC_ADD);
    sbuf_puts(b, op);
    emit_binding_func_expr(expr->u.binary.right, b, right_prec);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_func_add(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_func_addsub(expr, b, parent_prec, " + ", BIND_PREC_ADD);
}

static void emit_binding_func_sub(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_func_addsub(expr, b, parent_prec, " - ", BIND_PREC_ADD + 1);
}

static void emit_binding_func_mul_node(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = BIND_PREC_MUL < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    emit_binding_func_expr(expr->u.binary.left, b, BIND_PREC_MUL);
    sbuf_puts(b, " * ");
    emit_binding_func_expr(expr->u.binary.right, b, BIND_PREC_MUL);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_func_div(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = BIND_PREC_MUL < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    emit_binding_func_expr(expr->u.binary.left, b, BIND_PREC_MUL);
    sbuf_puts(b, " / ");
    emit_binding_func_expr(expr->u.binary.right, b, BIND_PREC_POW);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_func_powi(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = BIND_PREC_POW < parent_prec;
    bool base_need = binding_expr_needs_pow_base_parens(expr->u.powi.base);
    char expbuf[64];

    if (need)
        sbuf_putc(b, '(');
    if (base_need)
        sbuf_putc(b, '(');
    emit_binding_func_expr(expr->u.powi.base, b, base_need ? BIND_PREC_LOWEST : BIND_PREC_POW);
    if (base_need)
        sbuf_putc(b, ')');
    snprintf(expbuf, sizeof(expbuf), "%ld", expr->u.powi.exponent);
    sbuf_putc(b, '^');
    sbuf_puts(b, expbuf);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_func_unary_op(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    const dval_ops_t *ops = expr->u.unary_op.ops;
    bool need = BIND_PREC_UNARY < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    if (ops == &ops_neg) {
        sbuf_putc(b, '-');
        emit_binding_func_expr(expr->u.unary_op.child, b, BIND_PREC_UNARY);
    } else {
        sbuf_puts(b, (ops && ops->name) ? ops->name : "?");
        sbuf_putc(b, '(');
        emit_binding_func_expr(expr->u.unary_op.child, b, BIND_PREC_LOWEST);
        sbuf_putc(b, ')');
    }
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_func_binary_op(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    const dval_ops_t *ops = expr->u.binary_op.ops;

    if (ops == &ops_pow) {
        bool need = BIND_PREC_POW < parent_prec;
        bool base_need = binding_expr_needs_pow_base_parens(expr->u.binary_op.left);
        bool exp_need = !binding_expr_is_atomic(expr->u.binary_op.right);

        if (need)
            sbuf_putc(b, '(');
        if (base_need)
            sbuf_putc(b, '(');
        emit_binding_func_expr(expr->u.binary_op.left, b, base_need ? BIND_PREC_LOWEST : BIND_PREC_POW);
        if (base_need)
            sbuf_putc(b, ')');
        sbuf_puts(b, " ^ ");
        if (exp_need)
            sbuf_putc(b, '(');
        emit_binding_func_expr(expr->u.binary_op.right, b, BIND_PREC_LOWEST);
        if (exp_need)
            sbuf_putc(b, ')');
        if (need)
            sbuf_putc(b, ')');
        return;
    }

    sbuf_puts(b, (ops && ops->name) ? ops->name : "?");
    sbuf_putc(b, '(');
    emit_binding_func_expr(expr->u.binary_op.left, b, BIND_PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_binding_func_expr(expr->u.binary_op.right, b, BIND_PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_binding_tex_binary_op(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    const dval_ops_t *ops = expr->u.binary_op.ops;

    (void)parent_prec;
    if (ops == &ops_pow) {
        bool base_need = binding_expr_needs_pow_base_parens(expr->u.binary_op.left);

        if (base_need)
            sbuf_puts(b, "\\left(");
        emit_binding_tex_expr(expr->u.binary_op.left, b, base_need ? BIND_PREC_LOWEST : BIND_PREC_POW);
        if (base_need)
            sbuf_puts(b, "\\right)");
        sbuf_puts(b, "^{");
        emit_binding_tex_expr(expr->u.binary_op.right, b, BIND_PREC_LOWEST);
        sbuf_putc(b, '}');
        return;
    }

    if (ops && ops->tex_name)
        sbuf_puts(b, ops->tex_name);
    else {
        sbuf_puts(b, "\\operatorname{");
        sbuf_puts(b, (ops && ops->name) ? ops->name : "?");
        sbuf_putc(b, '}');
    }
    sbuf_putc(b, '(');
    emit_binding_tex_expr(expr->u.binary_op.left, b, BIND_PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_binding_tex_expr(expr->u.binary_op.right, b, BIND_PREC_LOWEST);
    sbuf_putc(b, ')');
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

static void emit_binding_func_expr(const dv_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    const binding_expr_ops_t *ops;

    if (!expr) {
        sbuf_puts(b, "0");
        return;
    }

    ops = binding_expr_ops_for_kind(expr->kind);
    if (!ops || !ops->emit_func) {
        sbuf_puts(b, "?");
        return;
    }

    ops->emit_func(expr, b, parent_prec);
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

char *dv_binding_expr_to_function_string(const dv_binding_expr_t *expr)
{
    sbuf_t b;

    sbuf_init(&b);
    emit_binding_func_expr(expr, &b, BIND_PREC_LOWEST);
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
