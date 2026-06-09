#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression.h"
#include "expr_binding_simplify.h"
#include "expr_bindings.h"
#include "expr_stringin_internal.h"
#include "expr_stringin_scan.h"
#include "expr_stringout.h"
#include "internal/number_internal.h"
#include "ustring.h"

typedef struct {
    string_cursor_t     *cursor;
    int                  error;
    string_t            *errmsg;
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
    expr_binding_const_id_t id;
    const char           *canonical_name;
    const char           *expr_name;
    const char           *tex_name;
    const number_t       *value;
} binding_const_meta_t;

typedef struct {
    int     precedence;
    bool    atomic;
    void  (*free_payload)(expr_binding_expr_t *expr);
    expr_binding_expr_t *(*clone)(const expr_binding_expr_t *expr);
    expr_t *(*eval_expr)(const expr_binding_expr_t *expr);
    bool  (*number_value)(const expr_binding_expr_t *expr, number_t *out);
    expr_binding_expr_t *(*simplify)(expr_binding_expr_t *expr);
    bool  (*struct_eq)(const expr_binding_expr_t *left,
                       const expr_binding_expr_t *right);
    bool  (*numeric_literal)(const expr_binding_expr_t *expr);
    bool  (*exact_complex)(const expr_binding_expr_t *expr,
                           binding_exact_complex_t *out);
    bool  (*explicit_mul_separator)(const expr_binding_expr_t *expr);
    void  (*emit_expr)(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
    void  (*emit_func)(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
    void  (*emit_tex)(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
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
    [EXPR_BINDING_CONST_E]     = { EXPR_BINDING_CONST_E,     "e",      "e", "e",       &NUM_E },
    [EXPR_BINDING_CONST_I]     = { EXPR_BINDING_CONST_I,     "i",      "i", "i",       &NUM_I },
    [EXPR_BINDING_CONST_PI]    = { EXPR_BINDING_CONST_PI,    "@pi",    "π", "\\pi",    &NUM_PI },
    [EXPR_BINDING_CONST_PHI]   = { EXPR_BINDING_CONST_PHI,   "@phi",   "φ", "\\phi",   &NUM_PHI },
    [EXPR_BINDING_CONST_GAMMA] = { EXPR_BINDING_CONST_GAMMA, "@gamma", "γ", "\\gamma", &NUM_EULER_MASCHERONI }
};

static const binding_const_meta_t *binding_const_meta(expr_binding_const_id_t const_id)
{
    if ((unsigned)const_id >= BINDING_CONST_COUNT ||
        s_binding_consts[const_id].value == NULL)
        return NULL;
    return &s_binding_consts[const_id];
}

static const char *binding_const_expr_name(expr_binding_const_id_t const_id)
{
    const binding_const_meta_t *meta = binding_const_meta(const_id);

    return meta ? meta->expr_name : "?";
}

static const char *binding_const_tex_name(expr_binding_const_id_t const_id)
{
    const binding_const_meta_t *meta = binding_const_meta(const_id);

    return meta ? meta->tex_name : "?";
}

static void binding_free_none(expr_binding_expr_t *expr);
static void binding_free_number(expr_binding_expr_t *expr);
static void binding_free_unary(expr_binding_expr_t *expr);
static void binding_free_binary(expr_binding_expr_t *expr);
static void binding_free_powi(expr_binding_expr_t *expr);
static void binding_free_unary_op(expr_binding_expr_t *expr);
static void binding_free_binary_op(expr_binding_expr_t *expr);

static expr_binding_expr_t *binding_clone_number(const expr_binding_expr_t *expr);
static expr_binding_expr_t *binding_clone_const(const expr_binding_expr_t *expr);
static expr_binding_expr_t *binding_clone_neg(const expr_binding_expr_t *expr);
static expr_binding_expr_t *binding_clone_add(const expr_binding_expr_t *expr);
static expr_binding_expr_t *binding_clone_sub(const expr_binding_expr_t *expr);
static expr_binding_expr_t *binding_clone_mul(const expr_binding_expr_t *expr);
static expr_binding_expr_t *binding_clone_div(const expr_binding_expr_t *expr);
static expr_binding_expr_t *binding_clone_powi(const expr_binding_expr_t *expr);
static expr_binding_expr_t *binding_clone_unary_op(const expr_binding_expr_t *expr);
static expr_binding_expr_t *binding_clone_binary_op(const expr_binding_expr_t *expr);

static expr_t *binding_eval_number(const expr_binding_expr_t *expr);
static expr_t *binding_eval_const(const expr_binding_expr_t *expr);
static expr_t *binding_eval_neg(const expr_binding_expr_t *expr);
static expr_t *binding_eval_add(const expr_binding_expr_t *expr);
static expr_t *binding_eval_sub(const expr_binding_expr_t *expr);
static expr_t *binding_eval_mul(const expr_binding_expr_t *expr);
static expr_t *binding_eval_div(const expr_binding_expr_t *expr);
static expr_t *binding_eval_powi(const expr_binding_expr_t *expr);
static expr_t *binding_eval_unary_op(const expr_binding_expr_t *expr);
static expr_t *binding_eval_binary_op(const expr_binding_expr_t *expr);

static bool binding_number_value_number(const expr_binding_expr_t *expr,
                                        number_t *out);
static bool binding_number_value_false(const expr_binding_expr_t *expr,
                                       number_t *out);
static bool binding_number_value_neg(const expr_binding_expr_t *expr,
                                     number_t *out);
static bool binding_number_value_add(const expr_binding_expr_t *expr,
                                     number_t *out);
static bool binding_number_value_sub(const expr_binding_expr_t *expr,
                                     number_t *out);
static bool binding_number_value_mul(const expr_binding_expr_t *expr,
                                     number_t *out);
static bool binding_number_value_div(const expr_binding_expr_t *expr,
                                     number_t *out);
static bool binding_number_value_powi(const expr_binding_expr_t *expr,
                                      number_t *out);

static bool binding_struct_eq_number(const expr_binding_expr_t *left,
                                     const expr_binding_expr_t *right);
static bool binding_struct_eq_const(const expr_binding_expr_t *left,
                                    const expr_binding_expr_t *right);
static bool binding_struct_eq_unary(const expr_binding_expr_t *left,
                                    const expr_binding_expr_t *right);
static bool binding_struct_eq_binary(const expr_binding_expr_t *left,
                                     const expr_binding_expr_t *right);
static bool binding_struct_eq_powi(const expr_binding_expr_t *left,
                                   const expr_binding_expr_t *right);
static bool binding_struct_eq_unary_op(const expr_binding_expr_t *left,
                                       const expr_binding_expr_t *right);
static bool binding_struct_eq_binary_op(const expr_binding_expr_t *left,
                                        const expr_binding_expr_t *right);

static bool binding_numeric_literal_true(const expr_binding_expr_t *expr);
static bool binding_numeric_literal_const(const expr_binding_expr_t *expr);
static bool binding_numeric_literal_unary(const expr_binding_expr_t *expr);
static bool binding_numeric_literal_binary(const expr_binding_expr_t *expr);
static bool binding_numeric_literal_powi(const expr_binding_expr_t *expr);
static bool binding_numeric_literal_false(const expr_binding_expr_t *expr);

static bool binding_exact_complex_number(const expr_binding_expr_t *expr,
                                         binding_exact_complex_t *out);
static bool binding_exact_complex_const(const expr_binding_expr_t *expr,
                                        binding_exact_complex_t *out);
static bool binding_exact_complex_unary(const expr_binding_expr_t *expr,
                                        binding_exact_complex_t *out);
static bool binding_exact_complex_addsub(const expr_binding_expr_t *expr,
                                         binding_exact_complex_t *out);
static bool binding_exact_complex_mul(const expr_binding_expr_t *expr,
                                      binding_exact_complex_t *out);
static bool binding_exact_complex_div(const expr_binding_expr_t *expr,
                                      binding_exact_complex_t *out);
static bool binding_exact_complex_powi(const expr_binding_expr_t *expr,
                                       binding_exact_complex_t *out);

static bool binding_explicit_mul_separator_false(const expr_binding_expr_t *expr);
static bool binding_explicit_mul_separator_true(const expr_binding_expr_t *expr);
static bool binding_explicit_mul_separator_unary(const expr_binding_expr_t *expr);
static bool binding_explicit_mul_separator_mul(const expr_binding_expr_t *expr);
static bool binding_explicit_mul_separator_powi(const expr_binding_expr_t *expr);

static void emit_binding_expr_number(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_const(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_neg(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_add(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_sub(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_mul_node(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_div(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_powi(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_unary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_binary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);

static void emit_binding_func_number(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_const(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_neg(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_add(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_sub(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_mul_node(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_div(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_powi(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_unary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_binary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);

static void emit_binding_tex_number(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_const(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_neg(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_add(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_sub(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_mul_node(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_div(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_powi(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_unary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_binary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static const binding_expr_ops_t s_binding_expr_ops[BINDING_EXPR_KIND_COUNT] = {
    [EXPR_BINDING_EXPR_NUMBER]    = { BIND_PREC_ATOM,  true,  binding_free_number,    binding_clone_number,    binding_eval_number,    binding_number_value_number, expr_binding_simplify_atom,      binding_struct_eq_number,    binding_numeric_literal_true,   binding_exact_complex_number, binding_explicit_mul_separator_false, emit_binding_expr_number,    emit_binding_func_number,    emit_binding_tex_number    },
    [EXPR_BINDING_EXPR_CONST]     = { BIND_PREC_ATOM,  true,  binding_free_none,      binding_clone_const,     binding_eval_const,     binding_number_value_false,  expr_binding_simplify_atom,      binding_struct_eq_const,     binding_numeric_literal_const,  binding_exact_complex_const,  binding_explicit_mul_separator_false, emit_binding_expr_const,     emit_binding_func_const,     emit_binding_tex_const     },
    [EXPR_BINDING_EXPR_NEG]       = { BIND_PREC_UNARY, false, binding_free_unary,     binding_clone_neg,       binding_eval_neg,       binding_number_value_neg,    expr_binding_simplify_neg,       binding_struct_eq_unary,     binding_numeric_literal_unary,  binding_exact_complex_unary,  binding_explicit_mul_separator_unary, emit_binding_expr_neg,       emit_binding_func_neg,       emit_binding_tex_neg       },
    [EXPR_BINDING_EXPR_ADD]       = { BIND_PREC_ADD,   false, binding_free_binary,    binding_clone_add,       binding_eval_add,       binding_number_value_add,    expr_binding_simplify_addsub,    binding_struct_eq_binary,    binding_numeric_literal_binary, binding_exact_complex_addsub, binding_explicit_mul_separator_false, emit_binding_expr_add,       emit_binding_func_add,       emit_binding_tex_add       },
    [EXPR_BINDING_EXPR_SUB]       = { BIND_PREC_ADD,   false, binding_free_binary,    binding_clone_sub,       binding_eval_sub,       binding_number_value_sub,    expr_binding_simplify_addsub,    binding_struct_eq_binary,    binding_numeric_literal_binary, binding_exact_complex_addsub, binding_explicit_mul_separator_false, emit_binding_expr_sub,       emit_binding_func_sub,       emit_binding_tex_sub       },
    [EXPR_BINDING_EXPR_MUL]       = { BIND_PREC_MUL,   false, binding_free_binary,    binding_clone_mul,       binding_eval_mul,       binding_number_value_mul,    expr_binding_simplify_mul,       binding_struct_eq_binary,    binding_numeric_literal_binary, binding_exact_complex_mul,    binding_explicit_mul_separator_mul, emit_binding_expr_mul_node,  emit_binding_func_mul_node,  emit_binding_tex_mul_node  },
    [EXPR_BINDING_EXPR_DIV]       = { BIND_PREC_MUL,   false, binding_free_binary,    binding_clone_div,       binding_eval_div,       binding_number_value_div,    expr_binding_simplify_div,       binding_struct_eq_binary,    binding_numeric_literal_binary, binding_exact_complex_div,    binding_explicit_mul_separator_true, emit_binding_expr_div,       emit_binding_func_div,       emit_binding_tex_div       },
    [EXPR_BINDING_EXPR_POWI]      = { BIND_PREC_POW,   true,  binding_free_powi,      binding_clone_powi,      binding_eval_powi,      binding_number_value_powi,   expr_binding_simplify_powi,      binding_struct_eq_powi,      binding_numeric_literal_powi,   binding_exact_complex_powi,   binding_explicit_mul_separator_powi, emit_binding_expr_powi,      emit_binding_func_powi,      emit_binding_tex_powi      },
    [EXPR_BINDING_EXPR_UNARY_OP]  = { BIND_PREC_UNARY, false, binding_free_unary_op,  binding_clone_unary_op,  binding_eval_unary_op,  binding_number_value_false,  expr_binding_simplify_unary_op,  binding_struct_eq_unary_op,  binding_numeric_literal_false,  NULL,                         binding_explicit_mul_separator_false, emit_binding_expr_unary_op,  emit_binding_func_unary_op,  emit_binding_tex_unary_op  },
    [EXPR_BINDING_EXPR_BINARY_OP] = { BIND_PREC_POW,   false, binding_free_binary_op, binding_clone_binary_op, binding_eval_binary_op, binding_number_value_false,  expr_binding_simplify_binary_op, binding_struct_eq_binary_op, binding_numeric_literal_false,  NULL,                         binding_explicit_mul_separator_false, emit_binding_expr_binary_op, emit_binding_func_binary_op, emit_binding_tex_binary_op }
};

static const binding_expr_ops_t *binding_expr_ops_for_kind(expr_binding_expr_kind_t kind)
{
    if ((unsigned)kind >= BINDING_EXPR_KIND_COUNT ||
        s_binding_expr_ops[kind].eval_expr == NULL)
        return NULL;
    return &s_binding_expr_ops[kind];
}

static expr_binding_expr_t *binding_expr_alloc(expr_binding_expr_kind_t kind)
{
    expr_binding_expr_t *expr = calloc(1u, sizeof(*expr));

    if (!expr)
        abort();
    expr->kind = kind;
    expr->cached_value = NUM_ZERO;
    return expr;
}

expr_binding_expr_t *expr_binding_expr_new_number_text(const char *text)
{
    expr_binding_expr_t *expr = binding_expr_alloc(EXPR_BINDING_EXPR_NUMBER);

    expr->u.text = text ? expr_tostring_xstrdup(text) : expr_tostring_xstrdup("0");
    return expr;
}

expr_binding_expr_t *expr_binding_expr_new_const(expr_binding_const_id_t const_id)
{
    expr_binding_expr_t *expr = binding_expr_alloc(EXPR_BINDING_EXPR_CONST);

    expr->u.const_id = const_id;
    return expr;
}

expr_binding_expr_t *expr_binding_expr_new_neg(expr_binding_expr_t *child)
{
    expr_binding_expr_t *expr = binding_expr_alloc(EXPR_BINDING_EXPR_NEG);

    expr->u.unary.child = child;
    return expr;
}

expr_binding_expr_t *expr_binding_expr_new_add(expr_binding_expr_t *left, expr_binding_expr_t *right)
{
    expr_binding_expr_t *expr = binding_expr_alloc(EXPR_BINDING_EXPR_ADD);

    expr->u.binary.left = left;
    expr->u.binary.right = right;
    return expr;
}

expr_binding_expr_t *expr_binding_expr_new_sub(expr_binding_expr_t *left, expr_binding_expr_t *right)
{
    expr_binding_expr_t *expr = binding_expr_alloc(EXPR_BINDING_EXPR_SUB);

    expr->u.binary.left = left;
    expr->u.binary.right = right;
    return expr;
}

expr_binding_expr_t *expr_binding_expr_new_mul(expr_binding_expr_t *left, expr_binding_expr_t *right)
{
    expr_binding_expr_t *expr = binding_expr_alloc(EXPR_BINDING_EXPR_MUL);

    expr->u.binary.left = left;
    expr->u.binary.right = right;
    return expr;
}

expr_binding_expr_t *expr_binding_expr_new_div(expr_binding_expr_t *left, expr_binding_expr_t *right)
{
    expr_binding_expr_t *expr = binding_expr_alloc(EXPR_BINDING_EXPR_DIV);

    expr->u.binary.left = left;
    expr->u.binary.right = right;
    return expr;
}

expr_binding_expr_t *expr_binding_expr_new_powi(expr_binding_expr_t *base, long exponent)
{
    expr_binding_expr_t *expr = binding_expr_alloc(EXPR_BINDING_EXPR_POWI);

    expr->u.powi.base = base;
    expr->u.powi.exponent = exponent;
    return expr;
}

expr_binding_expr_t *expr_binding_expr_new_unary_op(const expr_ops_t *ops, expr_binding_expr_t *child)
{
    expr_binding_expr_t *expr = binding_expr_alloc(EXPR_BINDING_EXPR_UNARY_OP);

    expr->u.unary_op.ops = ops;
    expr->u.unary_op.child = child;
    return expr;
}

expr_binding_expr_t *expr_binding_expr_new_binary_op(const expr_ops_t *ops,
                                                 expr_binding_expr_t *left,
                                                 expr_binding_expr_t *right)
{
    expr_binding_expr_t *expr = binding_expr_alloc(EXPR_BINDING_EXPR_BINARY_OP);

    expr->u.binary_op.ops = ops;
    expr->u.binary_op.left = left;
    expr->u.binary_op.right = right;
    return expr;
}

void expr_binding_expr_free(expr_binding_expr_t *expr)
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

static number_t binding_const_number(expr_binding_const_id_t const_id)
{
    const binding_const_meta_t *meta = binding_const_meta(const_id);

    return meta ? num_const(*meta->value) : num_clone(NUM_NAN);
}

static void binding_free_none(expr_binding_expr_t *expr)
{
    (void)expr;
}

static void binding_free_number(expr_binding_expr_t *expr)
{
    free(expr->u.text);
}

static void binding_free_unary(expr_binding_expr_t *expr)
{
    expr_binding_expr_free(expr->u.unary.child);
}

static void binding_free_binary(expr_binding_expr_t *expr)
{
    expr_binding_expr_free(expr->u.binary.left);
    expr_binding_expr_free(expr->u.binary.right);
}

static void binding_free_powi(expr_binding_expr_t *expr)
{
    expr_binding_expr_free(expr->u.powi.base);
}

static void binding_free_unary_op(expr_binding_expr_t *expr)
{
    expr_binding_expr_free(expr->u.unary_op.child);
}

static void binding_free_binary_op(expr_binding_expr_t *expr)
{
    expr_binding_expr_free(expr->u.binary_op.left);
    expr_binding_expr_free(expr->u.binary_op.right);
}

static expr_binding_expr_t *binding_clone_binary_plain(const expr_binding_expr_t *expr,
                                                     expr_binding_expr_t *(*ctor)(expr_binding_expr_t *,
                                                                               expr_binding_expr_t *))
{
    return ctor(expr_binding_expr_clone(expr->u.binary.left),
                expr_binding_expr_clone(expr->u.binary.right));
}

static expr_binding_expr_t *binding_clone_number(const expr_binding_expr_t *expr)
{
    return expr_binding_expr_new_number_text(expr->u.text);
}

static expr_binding_expr_t *binding_clone_const(const expr_binding_expr_t *expr)
{
    return expr_binding_expr_new_const(expr->u.const_id);
}

static expr_binding_expr_t *binding_clone_neg(const expr_binding_expr_t *expr)
{
    return expr_binding_expr_new_neg(expr_binding_expr_clone(expr->u.unary.child));
}

static expr_binding_expr_t *binding_clone_add(const expr_binding_expr_t *expr)
{
    return binding_clone_binary_plain(expr, expr_binding_expr_new_add);
}

static expr_binding_expr_t *binding_clone_sub(const expr_binding_expr_t *expr)
{
    return binding_clone_binary_plain(expr, expr_binding_expr_new_sub);
}

static expr_binding_expr_t *binding_clone_mul(const expr_binding_expr_t *expr)
{
    return binding_clone_binary_plain(expr, expr_binding_expr_new_mul);
}

static expr_binding_expr_t *binding_clone_div(const expr_binding_expr_t *expr)
{
    return binding_clone_binary_plain(expr, expr_binding_expr_new_div);
}

static expr_binding_expr_t *binding_clone_powi(const expr_binding_expr_t *expr)
{
    return expr_binding_expr_new_powi(expr_binding_expr_clone(expr->u.powi.base),
                                    expr->u.powi.exponent);
}

static expr_binding_expr_t *binding_clone_unary_op(const expr_binding_expr_t *expr)
{
    return expr_binding_expr_new_unary_op(expr->u.unary_op.ops,
                                        expr_binding_expr_clone(expr->u.unary_op.child));
}

static expr_binding_expr_t *binding_clone_binary_op(const expr_binding_expr_t *expr)
{
    return expr_binding_expr_new_binary_op(expr->u.binary_op.ops,
                                         expr_binding_expr_clone(expr->u.binary_op.left),
                                         expr_binding_expr_clone(expr->u.binary_op.right));
}

expr_binding_expr_t *expr_binding_expr_clone(const expr_binding_expr_t *expr)
{
    const binding_expr_ops_t *ops;

    if (!expr)
        return NULL;

    ops = binding_expr_ops_for_kind(expr->kind);
    return (ops && ops->clone) ? ops->clone(expr) : NULL;
}

static bool binding_cursor_peek_ascii_digit(const string_cursor_t *cursor,
                                            char *out)
{
    char ch = '\0';

    if (!rune_to_ascii(string_cursor_peek(cursor), &ch) ||
        ch < '0' || ch > '9')
        return false;

    if (out)
        *out = ch;
    return true;
}

static bool binding_ascii_is_digit(unsigned char ch)
{
    return ch >= '0' && ch <= '9';
}

static bool binding_ascii_is_alpha(unsigned char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

static bool binding_ascii_is_function_name_char(unsigned char ch)
{
    return binding_ascii_is_alpha(ch) ||
           binding_ascii_is_digit(ch) ||
           ch == '_' ||
           ch == '-';
}

static bool binding_cursor_consume_ascii(string_cursor_t *cursor, char ch)
{
    if (!rune_is_equal(string_cursor_peek(cursor), ch))
        return false;
    return string_cursor_next(cursor) == 0;
}

static bool binding_number_string_is_exact_decimal(const string_t *text)
{
    string_cursor_t *cursor;
    bool have_decimal_marker = false;
    bool have_digit = false;
    int exp_digits = 0;

    if (!text)
        return false;

    cursor = string_cursor_new(text);
    if (!cursor)
        return false;

    if (rune_is_equal(string_cursor_peek(cursor), '+') ||
        rune_is_equal(string_cursor_peek(cursor), '-'))
        (void)string_cursor_next(cursor);

    while (binding_cursor_peek_ascii_digit(cursor, NULL)) {
        have_digit = true;
        (void)string_cursor_next(cursor);
    }

    if (binding_cursor_consume_ascii(cursor, '.')) {
        have_decimal_marker = true;
        while (binding_cursor_peek_ascii_digit(cursor, NULL)) {
            have_digit = true;
            (void)string_cursor_next(cursor);
        }
    }

    if (rune_is_equal(string_cursor_peek(cursor), 'e') ||
        rune_is_equal(string_cursor_peek(cursor), 'E')) {
        have_decimal_marker = true;
        (void)string_cursor_next(cursor);
        if (rune_is_equal(string_cursor_peek(cursor), '+') ||
            rune_is_equal(string_cursor_peek(cursor), '-'))
            (void)string_cursor_next(cursor);
        while (binding_cursor_peek_ascii_digit(cursor, NULL)) {
            exp_digits++;
            (void)string_cursor_next(cursor);
        }
        if (exp_digits == 0) {
            string_cursor_free(cursor);
            return false;
        }
    }

    have_digit = have_digit && have_decimal_marker &&
                 string_cursor_done(cursor);
    string_cursor_free(cursor);
    return have_digit;
}

static number_t binding_number_from_exact_decimal_string(const string_t *text)
{
    string_cursor_t *cursor;
    string_t *digits;
    string_t *literal;
    number_t value = NUM_NAN;
    size_t digit_count = 0u;
    long frac_digits = 0;
    long exponent = 0;
    bool negative = false;
    bool seen_nonzero = false;
    char digit;

    if (!text)
        return num_clone(NUM_NAN);

    cursor = string_cursor_new(text);
    digits = string_new();
    if (!cursor || !digits) {
        string_cursor_free(cursor);
        string_free(digits);
        return num_clone(NUM_NAN);
    }

    if (rune_is_equal(string_cursor_peek(cursor), '+') ||
        rune_is_equal(string_cursor_peek(cursor), '-')) {
        negative = rune_is_equal(string_cursor_peek(cursor), '-');
        (void)string_cursor_next(cursor);
    }

    while (binding_cursor_peek_ascii_digit(cursor, &digit)) {
        if (digit != '0' || seen_nonzero) {
            seen_nonzero = true;
            string_append_char(digits, digit);
            digit_count++;
        }
        (void)string_cursor_next(cursor);
    }

    if (binding_cursor_consume_ascii(cursor, '.')) {
        while (binding_cursor_peek_ascii_digit(cursor, &digit)) {
            frac_digits++;
            if (digit != '0' || seen_nonzero) {
                seen_nonzero = true;
                string_append_char(digits, digit);
                digit_count++;
            }
            (void)string_cursor_next(cursor);
        }
    }

    if (rune_is_equal(string_cursor_peek(cursor), 'e') ||
        rune_is_equal(string_cursor_peek(cursor), 'E')) {
        bool exp_negative = false;

        (void)string_cursor_next(cursor);
        if (rune_is_equal(string_cursor_peek(cursor), '+') ||
            rune_is_equal(string_cursor_peek(cursor), '-')) {
            exp_negative = rune_is_equal(string_cursor_peek(cursor), '-');
            (void)string_cursor_next(cursor);
        }
        while (binding_cursor_peek_ascii_digit(cursor, &digit)) {
            exponent = exponent * 10 + (digit - '0');
            (void)string_cursor_next(cursor);
        }
        if (exp_negative)
            exponent = -exponent;
    }

    if (digit_count == 0u) {
        string_cursor_free(cursor);
        string_free(digits);
        return num_clone(NUM_ZERO);
    }

    frac_digits -= exponent;
    literal = string_new();
    if (!literal) {
        string_cursor_free(cursor);
        string_free(digits);
        return num_clone(NUM_NAN);
    }

    if (negative)
        string_append_char(literal, '-');
    string_append_string(literal, digits);

    if (frac_digits <= 0) {
        size_t zeros = (size_t)-frac_digits;

        while (zeros-- > 0u)
            string_append_char(literal, '0');
    } else {
        string_append_char(literal, '/');
        string_append_char(literal, '1');
        while (frac_digits-- > 0)
            string_append_char(literal, '0');
    }

    value = num_create_from_text(literal);
    string_free(literal);
    string_free(digits);
    string_cursor_free(cursor);
    return value;
}

static number_t binding_number_from_string(const string_t *text)
{
    if (text && string_view_equals_literal(string_view_all(text), "∞"))
        return num_clone(NUM_INF);
    if (text && string_view_equals_literal(string_view_all(text), "-∞"))
        return num_clone(NUM_NINF);
    if (binding_number_string_is_exact_decimal(text))
        return binding_number_from_exact_decimal_string(text);

    return num_create_from_text(text);
}

number_t binding_number_from_text(const char *text)
{
    string_t *string = text ? string_new_with(text) : NULL;
    number_t value;

    if (!string)
        return num_clone(NUM_NAN);

    value = binding_number_from_string(string);
    string_free(string);
    return value;
}

expr_t *expr_binding_expr_eval_expr(const expr_binding_expr_t *expr)
{
    const binding_expr_ops_t *ops;

    if (!expr)
        return expr_new_const(NUM_NAN);

    ops = binding_expr_ops_for_kind(expr->kind);
    if (ops && ops->eval_expr)
        return ops->eval_expr(expr);

    return expr_new_const(NUM_NAN);
}

static expr_t *binding_eval_number(const expr_binding_expr_t *expr)
{
    number_t value = binding_number_from_text(expr->u.text);
    expr_t *node = expr_new_const(value);

    num_destroy(&value);
    return node;
}

static expr_t *binding_eval_const(const expr_binding_expr_t *expr)
{
    number_t value = binding_const_number(expr->u.const_id);
    expr_t *node = expr_new_named_const(value,
                                      binding_const_expr_name(expr->u.const_id));

    num_destroy(&value);
    return node;
}

static expr_t *binding_eval_neg(const expr_binding_expr_t *expr)
{
    expr_t *child = expr_binding_expr_eval_expr(expr->u.unary.child);
    expr_t *node = expr_neg(child);

    expr_free(child);
    return node;
}

static expr_t *binding_eval_binary(const expr_binding_expr_t *expr,
                                   expr_t *(*op)(const expr_t *, const expr_t *))
{
    expr_t *left = expr_binding_expr_eval_expr(expr->u.binary.left);
    expr_t *right = expr_binding_expr_eval_expr(expr->u.binary.right);
    expr_t *node = op(left, right);

    expr_free(left);
    expr_free(right);
    return node;
}

static expr_t *binding_eval_add(const expr_binding_expr_t *expr)
{
    return binding_eval_binary(expr, expr_add);
}

static expr_t *binding_eval_sub(const expr_binding_expr_t *expr)
{
    return binding_eval_binary(expr, expr_sub);
}

static expr_t *binding_eval_mul(const expr_binding_expr_t *expr)
{
    return binding_eval_binary(expr, expr_mul);
}

bool binding_expr_is_const_id(const expr_binding_expr_t *expr,
                              expr_binding_const_id_t const_id)
{
    return expr &&
           expr->kind == EXPR_BINDING_EXPR_CONST &&
           expr->u.const_id == const_id;
}


bool expr_binding_expr_struct_eq(const expr_binding_expr_t *left,
                               const expr_binding_expr_t *right)
{
    const binding_expr_ops_t *ops;

    if (left == right)
        return true;
    if (!left || !right || left->kind != right->kind)
        return false;

    ops = binding_expr_ops_for_kind(left->kind);
    return ops && ops->struct_eq ? ops->struct_eq(left, right) : false;
}

static bool binding_struct_eq_number(const expr_binding_expr_t *left,
                                     const expr_binding_expr_t *right)
{
    if (!left->u.text || !right->u.text)
        return left->u.text == right->u.text;
    return strcmp(left->u.text, right->u.text) == 0;
}

static bool binding_struct_eq_const(const expr_binding_expr_t *left,
                                    const expr_binding_expr_t *right)
{
    return left->u.const_id == right->u.const_id;
}

static bool binding_struct_eq_unary(const expr_binding_expr_t *left,
                                    const expr_binding_expr_t *right)
{
    return expr_binding_expr_struct_eq(left->u.unary.child,
                                     right->u.unary.child);
}

static bool binding_struct_eq_binary(const expr_binding_expr_t *left,
                                     const expr_binding_expr_t *right)
{
    return expr_binding_expr_struct_eq(left->u.binary.left,
                                     right->u.binary.left) &&
           expr_binding_expr_struct_eq(left->u.binary.right,
                                     right->u.binary.right);
}

static bool binding_struct_eq_powi(const expr_binding_expr_t *left,
                                   const expr_binding_expr_t *right)
{
    return left->u.powi.exponent == right->u.powi.exponent &&
           expr_binding_expr_struct_eq(left->u.powi.base,
                                     right->u.powi.base);
}

static bool binding_struct_eq_unary_op(const expr_binding_expr_t *left,
                                       const expr_binding_expr_t *right)
{
    return left->u.unary_op.ops == right->u.unary_op.ops &&
           expr_binding_expr_struct_eq(left->u.unary_op.child,
                                     right->u.unary_op.child);
}

static bool binding_struct_eq_binary_op(const expr_binding_expr_t *left,
                                        const expr_binding_expr_t *right)
{
    return left->u.binary_op.ops == right->u.binary_op.ops &&
           expr_binding_expr_struct_eq(left->u.binary_op.left,
                                     right->u.binary_op.left) &&
           expr_binding_expr_struct_eq(left->u.binary_op.right,
                                     right->u.binary_op.right);
}

static bool binding_numeric_literal_true(const expr_binding_expr_t *expr)
{
    (void)expr;
    return true;
}

static bool binding_numeric_literal_const(const expr_binding_expr_t *expr)
{
    /*
     * These constants are lexical numeric atoms rather than symbolic display
     * constants.  Symbolic constants such as pi/gamma stay in the preserved
     * expression tree even though they also have numeric values.
     */
    return expr->u.const_id == EXPR_BINDING_CONST_I;
}

static bool binding_numeric_literal_unary(const expr_binding_expr_t *expr)
{
    return expr_binding_expr_is_numeric_literal(expr->u.unary.child);
}

static bool binding_numeric_literal_binary(const expr_binding_expr_t *expr)
{
    return expr_binding_expr_is_numeric_literal(expr->u.binary.left) &&
           expr_binding_expr_is_numeric_literal(expr->u.binary.right);
}

static bool binding_numeric_literal_powi(const expr_binding_expr_t *expr)
{
    return expr_binding_expr_is_numeric_literal(expr->u.powi.base);
}

static bool binding_numeric_literal_false(const expr_binding_expr_t *expr)
{
    (void)expr;
    return false;
}

static bool binding_explicit_mul_separator_false(const expr_binding_expr_t *expr)
{
    (void)expr;
    return false;
}

static bool binding_explicit_mul_separator_true(const expr_binding_expr_t *expr)
{
    (void)expr;
    return true;
}

static bool binding_explicit_mul_separator_unary(const expr_binding_expr_t *expr)
{
    return expr &&
           expr_binding_expr_needs_explicit_mul_separator(expr->u.unary.child);
}

static bool binding_explicit_mul_separator_mul(const expr_binding_expr_t *expr)
{
    return expr &&
           expr_binding_expr_needs_explicit_mul_separator(expr->u.binary.left);
}

static bool binding_explicit_mul_separator_powi(const expr_binding_expr_t *expr)
{
    return expr &&
           expr_binding_expr_needs_explicit_mul_separator(expr->u.powi.base);
}

bool expr_binding_expr_is_numeric_literal(const expr_binding_expr_t *expr)
{
    const binding_expr_ops_t *ops;

    if (!expr)
        return false;

    ops = binding_expr_ops_for_kind(expr->kind);
    return ops && ops->numeric_literal ? ops->numeric_literal(expr) : false;
}

bool expr_binding_expr_needs_explicit_mul_separator(const expr_binding_expr_t *expr)
{
    const binding_expr_ops_t *ops;

    if (!expr)
        return false;

    ops = binding_expr_ops_for_kind(expr->kind);
    return ops && ops->explicit_mul_separator
               ? ops->explicit_mul_separator(expr)
               : false;
}

void expr_binding_exact_complex_clear(binding_exact_complex_t *value)
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

bool expr_binding_expr_exact_complex(const expr_binding_expr_t *expr,
                                   binding_exact_complex_t *out)
{
    const binding_expr_ops_t *ops;

    if (!expr || !out)
        return false;

    ops = binding_expr_ops_for_kind(expr->kind);
    return ops && ops->exact_complex ? ops->exact_complex(expr, out) : false;
}

static bool binding_string_is_single_ascii(const string_t *text, char ch)
{
    return text && string_length(text) == 1u &&
           rune_is_equal(string_at(text, 0u), ch);
}

static bool binding_number_string_exact_complex(const string_t *text,
                                                binding_exact_complex_t *out)
{
    string_cursor_t *cursor;
    string_pos_t last_pos = 0u;
    rune_t last;
    string_t *coeff_text = NULL;
    number_t value;
    bool have_rune = false;

    if (!text || !out)
        return false;

    cursor = string_cursor_new(text);
    if (!cursor)
        return false;

    while (!string_cursor_done(cursor)) {
        last_pos = string_cursor_position(cursor);
        last = string_cursor_peek(cursor);
        have_rune = true;
        if (string_cursor_next(cursor) != 0)
            break;
    }

    if (have_rune &&
        (rune_is_equal(last, 'i') || rune_is_equal(last, 'I'))) {
        if (last_pos == 0u) {
            value = num_clone(NUM_ONE);
        } else {
            coeff_text = string_cursor_slice_between(0u, last_pos, cursor);
            if (!coeff_text) {
                string_cursor_free(cursor);
                return false;
            }

            if (binding_string_is_single_ascii(coeff_text, '+'))
                value = num_clone(NUM_ONE);
            else if (binding_string_is_single_ascii(coeff_text, '-'))
                value = num_clone(NUM_NEG_ONE);
            else
                value = binding_number_from_string(coeff_text);
            string_free(coeff_text);
        }
        if (!num_is_exact(value) || !num_is_real(value)) {
            num_destroy(&value);
            string_cursor_free(cursor);
            return false;
        }
        binding_exact_complex_set(out, num_clone(NUM_ZERO), value);
        string_cursor_free(cursor);
        return true;
    }

    value = binding_number_from_string(text);
    if (!num_is_exact(value) || !num_is_real(value)) {
        num_destroy(&value);
        string_cursor_free(cursor);
        return false;
    }
    binding_exact_complex_set(out, value, num_clone(NUM_ZERO));
    string_cursor_free(cursor);
    return true;
}

static bool binding_number_text_exact_complex(const char *text,
                                              binding_exact_complex_t *out)
{
    string_t *string = text ? string_new_with(text) : NULL;
    bool ok;

    if (!string)
        return false;

    ok = binding_number_string_exact_complex(string, out);
    string_free(string);
    return ok;
}

static bool binding_exact_complex_number(const expr_binding_expr_t *expr,
                                         binding_exact_complex_t *out)
{
    return binding_number_text_exact_complex(expr->u.text, out);
}

static bool binding_exact_complex_const(const expr_binding_expr_t *expr,
                                        binding_exact_complex_t *out)
{
    if (expr->u.const_id != EXPR_BINDING_CONST_I)
        return false;
    binding_exact_complex_set(out, num_clone(NUM_ZERO), num_clone(NUM_ONE));
    return true;
}

static bool binding_exact_complex_unary(const expr_binding_expr_t *expr,
                                        binding_exact_complex_t *out)
{
    binding_exact_complex_t child;

    if (!expr_binding_expr_exact_complex(expr->u.unary.child, &child))
        return false;

    binding_exact_complex_set(out, num_neg(child.real), num_neg(child.imag));
    expr_binding_exact_complex_clear(&child);
    return true;
}

static bool binding_exact_complex_addsub(const expr_binding_expr_t *expr,
                                         binding_exact_complex_t *out)
{
    binding_exact_complex_t left;
    binding_exact_complex_t right;
    bool subtract = expr->kind == EXPR_BINDING_EXPR_SUB;

    if (!expr_binding_expr_exact_complex(expr->u.binary.left, &left))
        return false;
    if (!expr_binding_expr_exact_complex(expr->u.binary.right, &right)) {
        expr_binding_exact_complex_clear(&left);
        return false;
    }

    binding_exact_complex_set(out,
                              subtract ? num_sub(left.real, right.real)
                                       : num_add(left.real, right.real),
                              subtract ? num_sub(left.imag, right.imag)
                                       : num_add(left.imag, right.imag));
    expr_binding_exact_complex_clear(&right);
    expr_binding_exact_complex_clear(&left);
    return true;
}

static bool binding_exact_complex_mul(const expr_binding_expr_t *expr,
                                      binding_exact_complex_t *out)
{
    NUM_SCOPE(scope);
    binding_exact_complex_t left;
    binding_exact_complex_t right;

    if (!expr_binding_expr_exact_complex(expr->u.binary.left, &left))
        return false;
    if (!expr_binding_expr_exact_complex(expr->u.binary.right, &right)) {
        expr_binding_exact_complex_clear(&left);
        return false;
    }

    binding_exact_complex_set(out,
        num_sub(num_mul(left.real, right.real), num_mul(left.imag, right.imag)),
        num_add(num_mul(left.real, right.imag), num_mul(left.imag, right.real)));
    expr_binding_exact_complex_clear(&right);
    expr_binding_exact_complex_clear(&left);
    return true;
}

static bool binding_exact_complex_div(const expr_binding_expr_t *expr,
                                      binding_exact_complex_t *out)
{
    NUM_SCOPE(scope);
    binding_exact_complex_t left;
    binding_exact_complex_t right;
    number_t denom;

    if (!expr_binding_expr_exact_complex(expr->u.binary.left, &left))
        return false;
    if (!expr_binding_expr_exact_complex(expr->u.binary.right, &right)) {
        expr_binding_exact_complex_clear(&left);
        return false;
    }

    denom = num_add(num_mul(right.real, right.real),
                    num_mul(right.imag, right.imag));
    if (num_is_zero(denom)) {
        expr_binding_exact_complex_clear(&right);
        expr_binding_exact_complex_clear(&left);
        return false;
    }

    binding_exact_complex_set(out,
        num_div(num_add(num_mul(left.real, right.real),
                        num_mul(left.imag, right.imag)), denom),
        num_div(num_sub(num_mul(left.imag, right.real),
                        num_mul(left.real, right.imag)), denom));
    expr_binding_exact_complex_clear(&right);
    expr_binding_exact_complex_clear(&left);
    return true;
}

static bool binding_exact_complex_powi(const expr_binding_expr_t *expr,
                                       binding_exact_complex_t *out)
{
    binding_exact_complex_t result;
    binding_exact_complex_t base;

    if (expr->u.powi.exponent < 0 ||
        !expr_binding_expr_exact_complex(expr->u.powi.base, &base))
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
        expr_binding_exact_complex_clear(&result);
        result = next;
    }

    expr_binding_exact_complex_clear(&base);
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

bool binding_expr_scaled_const_ratio(const expr_binding_expr_t *expr,
                                     long *numer_out,
                                     long *denom_out,
                                     expr_binding_const_id_t *const_id_out);

static bool binding_expr_scaled_const(const expr_binding_expr_t *expr,
                                      long *factor_out,
                                      expr_binding_const_id_t *const_id_out)
{
    long numer;
    long denom;
    expr_binding_const_id_t const_id;

    if (!binding_expr_scaled_const_ratio(expr, &numer, &denom, &const_id) ||
        denom != 1L)
        return false;

    *factor_out = numer;
    *const_id_out = const_id;
    return true;
}

bool binding_expr_scaled_const_ratio(const expr_binding_expr_t *expr,
                                     long *numer_out,
                                     long *denom_out,
                                     expr_binding_const_id_t *const_id_out)
{
    long factor_numer;
    long factor_denom;
    long nested_numer;
    long nested_denom;
    long gcd;
    expr_binding_const_id_t const_id;

    if (!expr)
        return false;

    if (expr->kind == EXPR_BINDING_EXPR_CONST) {
        *numer_out = 1L;
        *denom_out = 1L;
        *const_id_out = expr->u.const_id;
        return true;
    }

    if (expr->kind == EXPR_BINDING_EXPR_NEG &&
        binding_expr_scaled_const_ratio(expr->u.unary.child,
                                        &nested_numer,
                                        &nested_denom,
                                        &const_id)) {
        *numer_out = -nested_numer;
        *denom_out = nested_denom;
        *const_id_out = const_id;
        return true;
    }

    if (expr->kind != EXPR_BINDING_EXPR_MUL)
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

bool binding_const_ratio_parts(const expr_binding_expr_t *numer_expr,
                               const expr_binding_expr_t *denom_expr,
                               long *numer_out,
                               long *denom_out,
                               expr_binding_const_id_t *const_id_out)
{
    long numer;
    long denom;
    long gcd;
    expr_binding_const_id_t const_id;

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

static expr_t *binding_eval_known_pi_ratio(const expr_binding_expr_t *numer,
                                           const expr_binding_expr_t *denom)
{
    expr_t *node = NULL;

    if (!binding_expr_is_const_id(numer, EXPR_BINDING_CONST_PI))
        return NULL;

    if (binding_number_text_eq_long(denom, 2))
        node = expr_new_const(NUM_PI_2);
    else if (binding_number_text_eq_long(denom, 3))
        node = expr_new_const(NUM_PI_3);
    else if (binding_number_text_eq_long(denom, 4))
        node = expr_new_const(NUM_PI_4);
    else if (binding_number_text_eq_long(denom, 6))
        node = expr_new_const(NUM_PI_6);

    return node;
}

static expr_t *binding_eval_div(const expr_binding_expr_t *expr)
{
    expr_t *known = binding_eval_known_pi_ratio(expr->u.binary.left,
                                                expr->u.binary.right);

    if (known)
        return known;

    return binding_eval_binary(expr, expr_div);
}

static expr_t *binding_eval_powi(const expr_binding_expr_t *expr)
{
    expr_t *base = expr_binding_expr_eval_expr(expr->u.powi.base);
    number_t exponent = num_create_from_long(expr->u.powi.exponent);
    expr_t *node = expr_pow(base, &exponent);

    expr_free(base);
    num_destroy(&exponent);
    return node;
}

static expr_t *binding_eval_unary_op(const expr_binding_expr_t *expr)
{
    expr_t *child = expr_binding_expr_eval_expr(expr->u.unary_op.child);
    expr_t *node;

    if (!expr->u.unary_op.ops || !expr->u.unary_op.ops->apply_unary) {
        expr_free(child);
        return expr_new_const(NUM_NAN);
    }

    node = expr->u.unary_op.ops->apply_unary(child);
    expr_free(child);
    return node ? node : expr_new_const(NUM_NAN);
}

static expr_t *binding_eval_binary_op(const expr_binding_expr_t *expr)
{
    expr_t *left = expr_binding_expr_eval_expr(expr->u.binary_op.left);
    expr_t *right = expr_binding_expr_eval_expr(expr->u.binary_op.right);
    expr_t *node;

    if (!expr->u.binary_op.ops || !expr->u.binary_op.ops->apply_binary) {
        expr_free(left);
        expr_free(right);
        return expr_new_const(NUM_NAN);
    }

    node = expr->u.binary_op.ops->apply_binary(left, right);
    expr_free(left);
    expr_free(right);
    return node ? node : expr_new_const(NUM_NAN);
}

static void binding_expr_store_cached_value(expr_binding_expr_t *expr,
                                            number_t value,
                                            size_t precision_bits)
{
    if (expr->cached_value_valid)
        num_destroy(&expr->cached_value);
    expr->cached_value = num_scope_detach(num_clone(value));
    expr->cached_precision_bits = precision_bits;
    expr->cached_value_valid = true;
}

static number_t binding_expr_compute_value(const expr_binding_expr_t *expr)
{
    expr_t *node;
    number_t value;

    if (!expr)
        return num_clone(NUM_NAN);

    node = expr_binding_expr_eval_expr(expr);
    value = expr_eval(node);
    expr_free(node);
    return value;
}

number_t expr_binding_expr_eval(const expr_binding_expr_t *expr)
{
    expr_binding_expr_t *mutable_expr = (expr_binding_expr_t *)expr;
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

bool expr_binding_expr_eval_if_precision_increased(expr_binding_expr_t *expr,
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
        if (p->errmsg)
            string_append_cstr(p->errmsg, msg ? msg : "");
    }
}

static size_t binding_pos(const binding_parser_t *p)
{
    return string_cursor_position(p->cursor);
}

static int binding_at_end(const binding_parser_t *p)
{
    return string_cursor_done(p->cursor);
}

static int binding_peek_ascii(const binding_parser_t *p, unsigned char *out)
{
    return string_cursor_peek_ascii(p->cursor, out);
}

static int binding_peek_value(const binding_parser_t *p,
                              uint32_t *out,
                              size_t *width_out)
{
    return expr_parse_cursor_peek_value(p ? p->cursor : NULL,
                                        out,
                                        width_out);
}

static int binding_skip(binding_parser_t *p, size_t count)
{
    return string_cursor_skip(p->cursor, count);
}

static int binding_consume_char(binding_parser_t *p, unsigned char ch)
{
    return expr_parse_cursor_consume_char(p->cursor, (char)ch);
}

static int binding_set_pos(binding_parser_t *p, size_t pos)
{
    return string_cursor_seek(p->cursor, pos) == 0;
}

static void binding_skip_spaces(binding_parser_t *p)
{
    string_cursor_skip_spaces(p->cursor);
}

static size_t scan_unicode_fraction_len_view(string_view_t view, size_t pos)
{
    return expr_parse_scan_unicode_fraction_len(view, pos);
}

typedef struct {
    const char       *kw;
    size_t            klen;
    bool              is_binary;
    const expr_ops_t *ops;
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

static const binding_func_entry_t s_extra_binding_funcs[] = {
    BINDING_FUNC_ENTRY("pdf",           false, &ops_pdf),
    BINDING_FUNC_ENTRY("cdf",           false, &ops_cdf),
    BINDING_FUNC_ENTRY("logpdf",        false, &ops_logpdf),
    BINDING_FUNC_ENTRY("sec",           false, &ops_sec),
    BINDING_FUNC_ENTRY("lg",            false, &ops_log10),
    BINDING_FUNC_ENTRY("cosec",         false, &ops_cosec),
    BINDING_FUNC_ENTRY("csc",           false, &ops_cosec),
    BINDING_FUNC_ENTRY("cot",           false, &ops_cot),
    BINDING_FUNC_ENTRY("sech",          false, &ops_sech),
    BINDING_FUNC_ENTRY("cosech",        false, &ops_cosech),
    BINDING_FUNC_ENTRY("csch",          false, &ops_cosech),
    BINDING_FUNC_ENTRY("coth",          false, &ops_coth),
    BINDING_FUNC_ENTRY("asec",          false, &ops_asec),
    BINDING_FUNC_ENTRY("arcsec",        false, &ops_asec),
    BINDING_FUNC_ENTRY("acosec",        false, &ops_acosec),
    BINDING_FUNC_ENTRY("arccosec",      false, &ops_acosec),
    BINDING_FUNC_ENTRY("acsc",          false, &ops_acosec),
    BINDING_FUNC_ENTRY("arccsc",        false, &ops_acosec),
    BINDING_FUNC_ENTRY("acot",          false, &ops_acot),
    BINDING_FUNC_ENTRY("arccot",        false, &ops_acot),
    BINDING_FUNC_ENTRY("asech",         false, &ops_asech),
    BINDING_FUNC_ENTRY("arsech",        false, &ops_asech),
    BINDING_FUNC_ENTRY("acosech",       false, &ops_acosech),
    BINDING_FUNC_ENTRY("arcosech",      false, &ops_acosech),
    BINDING_FUNC_ENTRY("acsch",         false, &ops_acosech),
    BINDING_FUNC_ENTRY("arcsch",        false, &ops_acosech),
    BINDING_FUNC_ENTRY("acoth",         false, &ops_acoth),
    BINDING_FUNC_ENTRY("arcoth",        false, &ops_acoth),
};

static unsigned binding_func_bucket_hash(string_view_t kw)
{
    size_t len = string_view_length(kw);
    unsigned char first = 0u;
    unsigned char last = 0u;
    unsigned h;

    if (!expr_parse_view_peek_ascii(kw, 0u, &first) ||
        !expr_parse_view_peek_ascii(kw, len - 1u, &last))
        return 0u;

    h = (unsigned)len + first + last;

    for (size_t i = 1u; i + 1u < len; ++i) {
        unsigned char b = 0u;
        if (expr_parse_view_peek_ascii(kw, i, &b))
            h += (unsigned)(i + 1u) * b;
    }

    return h % BINDING_FUNC_TABLE_SIZE;
}

static unsigned binding_func_slot_hash(string_view_t kw)
{
    size_t len = string_view_length(kw);
    unsigned char first = 0u;
    unsigned char last = 0u;
    unsigned h;

    if (!expr_parse_view_peek_ascii(kw, 0u, &first) ||
        !expr_parse_view_peek_ascii(kw, len - 1u, &last))
        return 0u;

    h = first + last;

    for (size_t i = 0u; i < len; ++i) {
        unsigned char b = 0u;
        if (expr_parse_view_peek_ascii(kw, i, &b))
            h += (unsigned)(i + 3u) * b;
    }

    return h % BINDING_FUNC_TABLE_SIZE;
}

static bool binding_func_entry_matches(const binding_func_entry_t *entry,
                                       string_view_t kw)
{
    return string_view_equals_literal(kw, entry->kw);
}

static const binding_func_entry_t *binding_func_lookup(string_view_t kw)
{
    const binding_func_entry_t *entry;
    unsigned bucket;
    unsigned slot;

    if (string_view_is_empty(kw))
        return NULL;

    bucket = binding_func_bucket_hash(kw);
    slot = (binding_func_slot_hash(kw) +
            s_binding_func_displacements[bucket]) % BINDING_FUNC_TABLE_SIZE;
    entry = &s_binding_funcs[slot];

    if (binding_func_entry_matches(entry, kw))
        return entry;

    for (size_t i = 0u; i < BINDING_FUNC_TABLE_SIZE; ++i) {
        entry = &s_binding_funcs[i];
        if (binding_func_entry_matches(entry, kw))
            return entry;
    }

    for (size_t i = 0u; i < sizeof(s_extra_binding_funcs) / sizeof(s_extra_binding_funcs[0]); ++i) {
        entry = &s_extra_binding_funcs[i];
        if (binding_func_entry_matches(entry, kw))
            return entry;
    }

    return NULL;
}

static size_t scan_special_number_len_view(string_view_t view, size_t pos)
{
    return expr_parse_scan_special_number_len(view, pos, true, false);
}

static size_t scan_number_atom_len_view(string_view_t view, size_t pos)
{
    return expr_parse_scan_number_atom_len(view, pos, true);
}

static int parse_number_view(string_view_t text, number_t *out)
{
    size_t len;
    size_t atom_len;
    string_t *roundtrip;
    string_t *literal;
    uint32_t value = 0u;

    text = string_view_trim(text);

    if (string_view_is_empty(text))
        return 0;

    len = string_view_length(text);
    atom_len = scan_number_atom_len_view(text, 0u);
    if (atom_len != len)
        return 0;

    if (expr_parse_view_peek_value(text, 0u, &value, NULL) &&
        value == 0x221Eu) {
        *out = num_clone(NUM_INF);
        return 1;
    }

    literal = string_from_view(&text);
    if (!literal)
        return 0;

    *out = num_create_from_text(literal);
    string_free(literal);

    roundtrip = num_to_string(*out);
    if (!roundtrip) {
        num_destroy(out);
        return 0;
    }

    string_free(roundtrip);
    return 1;
}

static string_view_t binding_text(const binding_parser_t *p)
{
    return string_cursor_view_between(0u,
                                      string_cursor_end_position(p->cursor),
                                      p->cursor);
}

static string_t *binding_read_any_name_cursor(string_cursor_t *cursor)
{
    return expr_parse_read_name(cursor, true);
}

static int binding_can_start_atom(const binding_parser_t *p)
{
    unsigned char c = 0u;
    uint32_t cp = 0u;
    size_t len = 0u;
    string_view_t text;

    if (binding_at_end(p))
        return 0;

    text = binding_text(p);
    if (scan_unicode_fraction_len_view(text, binding_pos(p)) > 0u)
        return 1;

    if (binding_peek_value(p, &cp, &len) &&
        (cp == 0x221A || cp == 0x230A || cp == 0x2308))
        return 1;

    {
        string_cursor_t *cursor = string_cursor_clone(p->cursor);
        string_t *name = cursor ? binding_read_any_name_cursor(cursor) : NULL;

        if (name) {
            string_free(name);
            string_cursor_free(cursor);
            return 1;
        }
        string_cursor_free(cursor);
    }

    if (!binding_peek_ascii(p, &c))
        return 0;
    if (c == '(' || c == '+' || c == '-' || c == '|')
        return 1;
    if (binding_ascii_is_digit(c) || c == '.')
        return 1;

    return 0;
}

/* True if we're at the middle dot · (U+00B7, UTF-8: 0xC2 0xB7). */
static int binding_at_middle_dot(const binding_parser_t *p)
{
    uint32_t cp = 0u;

    return expr_parse_cursor_peek_value_at(p->cursor, binding_pos(p), &cp, NULL) &&
           cp == 0x00B7u;
}

static int binding_const_id_from_name(const char *name,
                                      expr_binding_const_id_t *const_id_out)
{
    const char *canon = expr_default_constant_canonical_name(name);

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

static expr_binding_expr_t *parse_binding_addexpr(binding_parser_t *p);
static expr_binding_expr_t *parse_binding_signed(binding_parser_t *p);
static expr_binding_expr_t *parse_binding_signed_operand(binding_parser_t *p);

static int parse_binding_required_char(binding_parser_t *p, char expected, const char *errmsg)
{
    binding_skip_spaces(p);
    if (!binding_consume_char(p, (unsigned char)expected)) {
        binding_set_error(p, errmsg);
        return 0;
    }
    return 1;
}

static expr_binding_expr_t *parse_binding_enclosed_expr(binding_parser_t *p,
                                                      char closing,
                                                      const char *errmsg)
{
    expr_binding_expr_t *inner = parse_binding_addexpr(p);

    if (!inner)
        return NULL;
    if (!parse_binding_required_char(p, closing, errmsg)) {
        expr_binding_expr_free(inner);
        return NULL;
    }

    return inner;
}

static int parse_binding_two_args(binding_parser_t *p,
                                  expr_binding_expr_t **a_out,
                                  expr_binding_expr_t **b_out)
{
    expr_binding_expr_t *a = parse_binding_addexpr(p);
    expr_binding_expr_t *b;

    if (!a)
        return 0;
    if (!parse_binding_required_char(p, ',', "expected ',' in binary function")) {
        expr_binding_expr_free(a);
        return 0;
    }
    b = parse_binding_addexpr(p);
    if (!b) {
        expr_binding_expr_free(a);
        return 0;
    }

    *a_out = a;
    *b_out = b;
    return 1;
}

static const binding_func_entry_t *parse_binding_function_head(binding_parser_t *p,
                                                               size_t *paren_pos_out)
{
    string_cursor_t *scan = string_cursor_clone(p->cursor);
    size_t id_start = binding_pos(p);
    size_t id_end = id_start;
    unsigned char b;

    if (!scan) {
        *paren_pos_out = 0u;
        return NULL;
    }

    while (string_cursor_peek_ascii(scan, &b) &&
           binding_ascii_is_function_name_char(b)) {
        string_cursor_skip(scan, 1u);
        id_end = string_cursor_position(scan);
    }

    if (id_end > id_start) {
        string_view_t id = string_cursor_view_between(id_start, id_end, p->cursor);
        const binding_func_entry_t *entry =
            binding_func_lookup(id);

        if (entry) {
            string_cursor_seek(scan, id_end);
            string_cursor_skip_spaces(scan);
            if (string_cursor_peek_ascii(scan, &b) && b == '(') {
                *paren_pos_out = string_cursor_position(scan);
                string_cursor_free(scan);
                return entry;
            }
        }
    }

    {
        static const struct {
            const char *text;
            size_t      len;
        } unicode_aliases[] = {
            { "W₀",  sizeof("W₀")  - 1u },
            { "W₋₁", sizeof("W₋₁") - 1u }
        };

        for (size_t i = 0u; i < sizeof(unicode_aliases) / sizeof(unicode_aliases[0]); ++i) {
            size_t len = unicode_aliases[i].len;
            string_view_t alias_span;
            const binding_func_entry_t *entry;

            if (!string_cursor_match_at(p->cursor,
                                                     binding_pos(p),
                                                     unicode_aliases[i].text))
                continue;

            alias_span = string_cursor_view_between(binding_pos(p),
                                                    binding_pos(p) + len,
                                                    p->cursor);

            entry = binding_func_lookup(alias_span);
            string_cursor_seek(scan, binding_pos(p));

            string_cursor_skip(scan, len);
            string_cursor_skip_spaces(scan);
            if (entry && string_cursor_peek_ascii(scan, &b) && b == '(') {
                *paren_pos_out = string_cursor_position(scan);
                string_cursor_free(scan);
                return entry;
            }
        }
    }

    *paren_pos_out = 0u;
    string_cursor_free(scan);
    return NULL;
}

static expr_binding_expr_t *parse_binding_atom(binding_parser_t *p)
{
    NUM_SCOPE(scope);
    unsigned char c;
    binding_skip_spaces(p);
    if (p->error || binding_at_end(p)) {
        binding_set_error(p, "expected binding expression");
        return NULL;
    }

    if (binding_peek_ascii(p, &c) && c == '(') {
        expr_binding_expr_t *inner;

        binding_skip(p, 1u);
        inner = parse_binding_addexpr(p);
        binding_skip_spaces(p);
        if (!inner)
            return NULL;
        if (!binding_consume_char(p, ')')) {
            expr_binding_expr_free(inner);
            binding_set_error(p, "expected ')'");
            return NULL;
        }
        return inner;
    }

    if (binding_peek_ascii(p, &c) && c == '|') {
        expr_binding_expr_t *inner;

        binding_skip(p, 1u);
        inner = parse_binding_enclosed_expr(p, '|', "expected '|'");
        return inner ? expr_binding_expr_new_unary_op(&ops_abs, inner) : NULL;
    }

    if (binding_peek_ascii(p, &c) && c == '?') {
        binding_skip(p, 1u);
        return expr_binding_expr_new_number_text("NAN");
    }

    {
        uint32_t cp = 0;
        size_t cp_len = 0u;

        if (binding_peek_value(p, &cp, &cp_len) && cp == 0x221A) {
            expr_binding_expr_t *arg;

            binding_skip(p, cp_len);
            if (!parse_binding_required_char(p, '(', "expected '(' after √"))
                return NULL;
            arg = parse_binding_enclosed_expr(p, ')', "expected ')' after √ argument");
            return arg ? expr_binding_expr_new_unary_op(&ops_sqrt, arg) : NULL;
        }

        if (binding_peek_value(p, &cp, &cp_len) &&
            (cp == 0x230A || cp == 0x2308)) {
            const expr_ops_t *ops = (cp == 0x230A) ? &ops_floor : &ops_ceil;
            uint32_t close_cp = 0;
            const uint32_t closing = (cp == 0x230A) ? 0x230B : 0x2309;
            const char *errmsg = (cp == 0x230A) ? "expected '⌋'" : "expected '⌉'";
            expr_binding_expr_t *inner;
            size_t close_len = 0u;

            binding_skip(p, cp_len);
            inner = parse_binding_addexpr(p);
            if (!inner)
                return NULL;
            binding_skip_spaces(p);
            if (!binding_peek_value(p, &close_cp, &close_len) ||
                close_cp != closing) {
                expr_binding_expr_free(inner);
                binding_set_error(p, errmsg);
                return NULL;
            }
            binding_skip(p, close_len);
            return expr_binding_expr_new_unary_op(ops, inner);
        }
    }

    if ((binding_peek_ascii(p, &c) && (binding_ascii_is_digit(c) || c == '.')) ||
        scan_special_number_len_view(binding_text(p), binding_pos(p)) > 0u ||
        scan_unicode_fraction_len_view(binding_text(p), binding_pos(p)) > 0u) {
        size_t pos = binding_pos(p);
        size_t len = scan_number_atom_len_view(binding_text(p), pos);
        string_view_t literal_view;
        string_t *text;
        number_t value;

        literal_view = string_cursor_view_between(pos, pos + len, p->cursor);
        if (len == 0u ||
            !parse_number_view(literal_view, &value)) {
            binding_set_error(p, "expected numeric literal");
            return NULL;
        }
        num_destroy(&value);
        text = string_from_view(&literal_view);
        if (!text) {
            binding_set_error(p, "could not preserve numeric literal");
            return NULL;
        }
        binding_skip(p, len);
        {
            expr_binding_expr_t *expr =
                expr_binding_expr_new_number_text(string_c_str(text));
            string_free(text);
            return expr;
        }
    }

    {
        size_t paren_pos = 0u;
        const binding_func_entry_t *func = parse_binding_function_head(p, &paren_pos);

        if (func) {
            binding_set_pos(p, paren_pos + 1u);
            if (func->is_binary) {
                expr_binding_expr_t *a = NULL;
                expr_binding_expr_t *b = NULL;

                if (!parse_binding_two_args(p, &a, &b))
                    return NULL;
                if (!parse_binding_required_char(p, ')', "expected ')' after binary function")) {
                    expr_binding_expr_free(a);
                    expr_binding_expr_free(b);
                    return NULL;
                }
                return expr_binding_expr_new_binary_op(func->ops, a, b);
            } else {
                expr_binding_expr_t *arg =
                    parse_binding_enclosed_expr(p, ')', "expected ')' after function argument");

                return arg ? expr_binding_expr_new_unary_op(func->ops, arg) : NULL;
            }
        }
    }

    {
        string_t *name = binding_read_any_name_cursor(p->cursor);
        expr_binding_const_id_t const_id;

        if (!name) {
            binding_set_error(p, "expected arithmetic constant");
            return NULL;
        }
        if (!binding_const_id_from_name(string_c_str(name), &const_id)) {
            string_free(name);
            binding_set_error(p, "binding expressions only allow numeric constants");
            return NULL;
        }
        string_free(name);
        return expr_binding_expr_new_const(const_id);
    }
}

static expr_binding_expr_t *parse_binding_power_operand(binding_parser_t *p)
{
    return parse_binding_atom(p);
}

static expr_binding_expr_t *parse_binding_power(binding_parser_t *p)
{
    expr_binding_expr_t *base = parse_binding_power_operand(p);
    if (!base)
        return NULL;

    for (;;) {
        expr_binding_expr_t *exponent;
        expr_binding_expr_t *result;
        long exponent_long;
        unsigned char c;

        binding_skip_spaces(p);
        if (!binding_peek_ascii(p, &c) || c != '^')
            return base;

        binding_skip(p, 1u);
        binding_skip_spaces(p);

        if (binding_peek_ascii(p, &c) && c == '(') {
            binding_skip(p, 1u);
            exponent = parse_binding_enclosed_expr(p, ')',
                                                   "expected ')' after exponent");
        } else {
            exponent = parse_binding_signed_operand(p);
        }

        if (!exponent) {
            expr_binding_expr_free(base);
            binding_set_error(p, "expected exponent after '^'");
            return NULL;
        }

        if (binding_number_text_to_long(exponent, &exponent_long)) {
            expr_binding_expr_free(exponent);
            result = expr_binding_expr_new_powi(base, exponent_long);
        } else {
            result = expr_binding_expr_new_binary_op(&ops_pow, base, exponent);
        }

        if (!result)
            return NULL;
        base = result;
    }
}

static expr_binding_expr_t *parse_binding_signed(binding_parser_t *p)
{
    unsigned char c;

    binding_skip_spaces(p);
    if (binding_peek_ascii(p, &c) && c == '+') {
        binding_skip(p, 1u);
        return parse_binding_signed(p);
    }
    if (binding_peek_ascii(p, &c) && c == '-') {
        expr_binding_expr_t *inner;

        binding_skip(p, 1u);
        inner = parse_binding_signed(p);
        if (!inner)
            return NULL;
        return expr_binding_expr_new_neg(inner);
    }

    return parse_binding_power(p);
}

static expr_binding_expr_t *parse_binding_signed_operand(binding_parser_t *p)
{
    unsigned char c;

    binding_skip_spaces(p);
    if (binding_peek_ascii(p, &c) && c == '+') {
        binding_skip(p, 1u);
        return parse_binding_signed_operand(p);
    }
    if (binding_peek_ascii(p, &c) && c == '-') {
        expr_binding_expr_t *inner;

        binding_skip(p, 1u);
        inner = parse_binding_signed_operand(p);
        if (!inner)
            return NULL;
        return expr_binding_expr_new_neg(inner);
    }

    return parse_binding_power_operand(p);
}

static expr_binding_expr_t *parse_binding_mulexpr(binding_parser_t *p)
{
    expr_binding_expr_t *numer = parse_binding_signed(p);
    expr_binding_expr_t *denom = NULL;

    if (!numer)
        return NULL;

    for (;;) {
        char op = '\0';
        unsigned char c = 0u;
        expr_binding_expr_t *rhs;

        binding_skip_spaces(p);
        if (binding_at_end(p))
            break;

        if (binding_at_middle_dot(p)) {
            op = '*';
            binding_skip(p, 2u);
        } else {
            if (binding_peek_ascii(p, &c)) {
                if (c == '+' || c == '-')
                    break;
                if (c == '*' || c == '/') {
                    op = (char)c;
                    binding_skip(p, 1u);
                }
            }
            if (!op && binding_can_start_atom(p))
                op = '*';
            if (!op)
                break;
        }

        rhs = parse_binding_signed(p);
        if (!rhs) {
            expr_binding_expr_free(numer);
            expr_binding_expr_free(denom);
            return NULL;
        }

        if (op == '*') {
            numer = expr_binding_expr_new_mul(numer, rhs);
        } else if (!denom) {
            denom = rhs;
        } else {
            denom = expr_binding_expr_new_mul(denom, rhs);
        }
    }

    if (denom)
        return expr_binding_expr_new_div(numer, denom);

    return numer;
}

static expr_binding_expr_t *parse_binding_addexpr(binding_parser_t *p)
{
    expr_binding_expr_t *lhs = parse_binding_mulexpr(p);

    if (!lhs)
        return NULL;

    for (;;) {
        char op;
        unsigned char c;
        expr_binding_expr_t *rhs;

        binding_skip_spaces(p);
        if (!binding_peek_ascii(p, &c) || (c != '+' && c != '-'))
            break;

        op = (char)c;
        binding_skip(p, 1u);
        rhs = parse_binding_mulexpr(p);
        if (!rhs) {
            expr_binding_expr_free(lhs);
            return NULL;
        }

        lhs = (op == '+')
            ? expr_binding_expr_new_add(lhs, rhs)
            : expr_binding_expr_new_sub(lhs, rhs);
    }

    return lhs;
}

expr_binding_expr_t *expr_binding_expr_parse_view(string_view_t text,
                                                  string_t *errmsg)
{
    binding_parser_t ps;
    expr_binding_expr_t *result;

    if (errmsg)
        string_clear(errmsg);

    if (string_view_is_empty(text))
        return NULL;

    text = string_view_trim(text);
    if (string_view_is_empty(text))
        return NULL;

    ps.cursor = string_cursor_new_view(text);
    if (!ps.cursor)
        return NULL;
    ps.error = 0;
    ps.errmsg = string_new();
    if (!ps.errmsg) {
        string_cursor_free(ps.cursor);
        return NULL;
    }

    result = parse_binding_addexpr(&ps);
    binding_skip_spaces(&ps);
    if (result && !ps.error && binding_at_end(&ps)) {
        string_free(ps.errmsg);
        string_cursor_free(ps.cursor);
        return result;
    }

    if (result)
        expr_binding_expr_free(result);
    if (!ps.error)
        binding_set_error(&ps, "trailing input");
    if (errmsg)
        string_append_string(errmsg, ps.errmsg);
    string_free(ps.errmsg);
    string_cursor_free(ps.cursor);
    return NULL;
}

static int binding_expr_prec(const expr_binding_expr_t *expr)
{
    const binding_expr_ops_t *ops;

    if (!expr)
        return BIND_PREC_LOWEST;

    ops = binding_expr_ops_for_kind(expr->kind);
    if (ops)
        return ops->precedence;

    return BIND_PREC_LOWEST;
}

static bool binding_expr_is_atomic(const expr_binding_expr_t *expr)
{
    const binding_expr_ops_t *ops;

    if (!expr)
        return false;

    if (expr->kind == EXPR_BINDING_EXPR_NUMBER) {
        number_t value = num_create_from_string(expr->u.text);
        bool atomic = !num_is_nan(value) &&
                      (num_is_real(value) || num_eq(value, NUM_I));

        num_destroy(&value);
        return atomic;
    }

    ops = binding_expr_ops_for_kind(expr->kind);
    return ops ? ops->atomic : false;
}

static bool binding_expr_needs_pow_base_parens(const expr_binding_expr_t *expr)
{
    number_t value;
    bool need;

    if (!expr)
        return false;
    if (binding_expr_prec(expr) < BIND_PREC_POW)
        return true;
    if (expr->kind != EXPR_BINDING_EXPR_NUMBER)
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

typedef struct {
    bool      negative;
    string_t *numer;
    string_t *denom;
} binding_simple_rational_t;

static void binding_simple_rational_clear(binding_simple_rational_t *rational)
{
    if (!rational)
        return;
    string_free(rational->numer);
    string_free(rational->denom);
    rational->negative = false;
    rational->numer = NULL;
    rational->denom = NULL;
}

static bool binding_string_is_simple_rational(const string_t *text,
                                              binding_simple_rational_t *out)
{
    string_cursor_t *cursor;
    string_pos_t numer_start;
    string_pos_t slash_pos;
    string_pos_t denom_start;
    size_t numer_digits = 0u;
    size_t denom_digits = 0u;
    bool ok = false;

    if (!text || !out)
        return false;

    out->negative = false;
    out->numer = NULL;
    out->denom = NULL;

    cursor = string_cursor_new(text);
    if (!cursor)
        return false;

    if (rune_is_equal(string_cursor_peek(cursor), '+') ||
        rune_is_equal(string_cursor_peek(cursor), '-')) {
        out->negative = rune_is_equal(string_cursor_peek(cursor), '-');
        (void)string_cursor_next(cursor);
    }

    numer_start = string_cursor_position(cursor);
    while (binding_cursor_peek_ascii_digit(cursor, NULL)) {
        numer_digits++;
        (void)string_cursor_next(cursor);
    }
    if (numer_digits == 0u || !rune_is_equal(string_cursor_peek(cursor), '/'))
        goto done;

    slash_pos = string_cursor_position(cursor);
    (void)string_cursor_next(cursor);
    denom_start = string_cursor_position(cursor);

    while (binding_cursor_peek_ascii_digit(cursor, NULL)) {
        denom_digits++;
        (void)string_cursor_next(cursor);
    }
    if (denom_digits == 0u || !string_cursor_done(cursor))
        goto done;

    out->numer = string_cursor_slice_between(numer_start, slash_pos, cursor);
    out->denom = string_cursor_extract(denom_start, cursor);
    ok = out->numer && out->denom;

done:
    string_cursor_free(cursor);
    if (!ok)
        binding_simple_rational_clear(out);
    return ok;
}

static bool binding_text_is_simple_rational(const char *text,
                                            binding_simple_rational_t *out)
{
    string_t *string = text ? string_new_with(text) : NULL;
    bool ok;

    if (!string)
        return false;

    ok = binding_string_is_simple_rational(string, out);
    string_free(string);
    return ok;
}

static void emit_binding_digits(sbuf_t *b, const string_t *digits)
{
    string_cursor_t *cursor = string_cursor_new(digits);

    if (!cursor)
        return;
    while (!string_cursor_done(cursor)) {
        char digit;

        if (binding_cursor_peek_ascii_digit(cursor, &digit))
            sbuf_putc(b, digit);
        if (string_cursor_next(cursor) != 0)
            break;
    }
    string_cursor_free(cursor);
}

static void emit_binding_unicode_digits(sbuf_t *b,
                                        const string_t *digits,
                                        const char *const table[10])
{
    string_cursor_t *cursor = string_cursor_new(digits);

    if (!cursor)
        return;
    while (!string_cursor_done(cursor)) {
        char digit;

        if (binding_cursor_peek_ascii_digit(cursor, &digit))
            sbuf_puts(b, table[digit - '0']);
        if (string_cursor_next(cursor) != 0)
            break;
    }
    string_cursor_free(cursor);
}

static string_t *binding_decimal_display_text(const char *text)
{
    string_t *source = string_new_with(text ? text : "");
    string_t *out = NULL;
    string_cursor_t *cursor = NULL;

    if (!source)
        return NULL;

    out = string_new();
    cursor = string_cursor_new(source);
    if (!out || !cursor)
        goto fail;

    while (!string_cursor_done(cursor)) {
        char digit;
        string_pos_t frac_start;
        string_pos_t keep_end;
        string_pos_t last_nonzero_end;
        string_pos_t zero_start = 0u;
        string_pos_t long_zero_start = 0u;
        size_t digit_count = 0u;
        size_t zero_run = 0u;
        bool seen_nonzero = false;
        bool long_zero = false;

        if (!rune_is_equal(string_cursor_peek(cursor), '.')) {
            if (string_append_rune(out, string_cursor_peek(cursor)) != 0)
                goto fail;
            if (string_cursor_next(cursor) != 0)
                goto fail;
            continue;
        }

        if (string_cursor_next(cursor) != 0)
            goto fail;
        frac_start = string_cursor_position(cursor);
        last_nonzero_end = frac_start;

        while (binding_cursor_peek_ascii_digit(cursor, &digit)) {
            string_pos_t digit_start = string_cursor_position(cursor);

            if (string_cursor_next(cursor) != 0)
                goto fail;
            digit_count++;

            if (digit == '0') {
                if (seen_nonzero) {
                    if (zero_run == 0u)
                        zero_start = digit_start;
                    zero_run++;
                    if (!long_zero && zero_run >= 24u) {
                        long_zero = true;
                        long_zero_start = zero_start;
                    }
                }
            } else {
                seen_nonzero = true;
                zero_run = 0u;
                if (!long_zero)
                    last_nonzero_end = string_cursor_position(cursor);
            }
        }

        if (digit_count == 0u) {
            if (string_append_char(out, '.') != 0)
                goto fail;
            continue;
        }

        keep_end = long_zero ? long_zero_start : last_nonzero_end;
        if (keep_end > frac_start) {
            if (string_append_char(out, '.') != 0)
                goto fail;
            if (string_cursor_append_slice_between(out, frac_start, keep_end, cursor) != 0)
                goto fail;
        }
    }

    string_cursor_free(cursor);
    string_free(source);
    return out;

fail:
    string_cursor_free(cursor);
    string_free(out);
    string_free(source);
    return NULL;
}

static void emit_binding_number_text(const char *text, sbuf_t *b)
{
    string_t *clean;
    binding_simple_rational_t rational = { false, NULL, NULL };

    if (binding_text_is_simple_rational(text, &rational)) {
        if (rational.negative)
            sbuf_putc(b, '-');
        if (binding_string_is_single_ascii(rational.denom, '1')) {
            emit_binding_digits(b, rational.numer);
            binding_simple_rational_clear(&rational);
            return;
        }
        emit_binding_unicode_digits(b, rational.numer, s_binding_sup_digits);
        sbuf_puts(b, "⁄");
        emit_binding_unicode_digits(b, rational.denom, s_binding_sub_digits);
        binding_simple_rational_clear(&rational);
        return;
    }

    clean = binding_decimal_display_text(text);
    if (clean) {
        sbuf_puts(b, string_c_str(clean));
        string_free(clean);
    }
}

static void emit_binding_expr(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_tex_expr(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);

static bool binding_number_value_unary(const expr_binding_expr_t *expr,
                                       binding_number_unary_fn op,
                                       number_t *out)
{
    number_t child;

    if (!expr_binding_expr_number_value(expr->u.unary.child, &child))
        return false;
    *out = num_scope_detach(op(child));
    num_destroy(&child);
    return true;
}

static bool binding_number_value_binary(const expr_binding_expr_t *expr,
                                        binding_number_binary_fn op,
                                        number_t *out)
{
    number_t left;
    number_t right;

    if (!expr_binding_expr_number_value(expr->u.binary.left, &left))
        return false;
    if (!expr_binding_expr_number_value(expr->u.binary.right, &right)) {
        num_destroy(&left);
        return false;
    }
    *out = num_scope_detach(op(left, right));
    num_destroy(&right);
    num_destroy(&left);
    return true;
}

static bool binding_number_value_number(const expr_binding_expr_t *expr,
                                        number_t *out)
{
    *out = num_scope_detach(binding_number_from_text(expr->u.text));
    return true;
}

static bool binding_number_value_false(const expr_binding_expr_t *expr,
                                       number_t *out)
{
    (void)expr;
    (void)out;
    return false;
}

static bool binding_number_value_neg(const expr_binding_expr_t *expr,
                                     number_t *out)
{
    return binding_number_value_unary(expr, num_neg, out);
}

static bool binding_number_value_add(const expr_binding_expr_t *expr,
                                     number_t *out)
{
    return binding_number_value_binary(expr, num_add, out);
}

static bool binding_number_value_sub(const expr_binding_expr_t *expr,
                                     number_t *out)
{
    return binding_number_value_binary(expr, num_sub, out);
}

static bool binding_number_value_mul(const expr_binding_expr_t *expr,
                                     number_t *out)
{
    return binding_number_value_binary(expr, num_mul, out);
}

static bool binding_number_value_div(const expr_binding_expr_t *expr,
                                     number_t *out)
{
    return binding_number_value_binary(expr, num_div, out);
}

static bool binding_number_value_powi(const expr_binding_expr_t *expr,
                                      number_t *out)
{
    number_t base;

    if (!expr_binding_expr_number_value(expr->u.powi.base, &base))
        return false;
    *out = num_scope_detach(num_pow_int(base, (int)expr->u.powi.exponent));
    num_destroy(&base);
    return true;
}

bool expr_binding_expr_number_value(const expr_binding_expr_t *expr, number_t *out)
{
    const binding_expr_ops_t *ops;

    if (!expr || !out)
        return false;

    ops = binding_expr_ops_for_kind(expr->kind);
    return ops && ops->number_value ? ops->number_value(expr, out) : false;
}

static bool binding_expr_leading_number(const expr_binding_expr_t *expr,
                                        number_t *coeff_out,
                                        const expr_binding_expr_t **rest_out)
{
    if (expr_binding_expr_number_value(expr, coeff_out)) {
        *rest_out = NULL;
        return true;
    }

    if (expr && expr->kind == EXPR_BINDING_EXPR_MUL &&
        expr_binding_expr_number_value(expr->u.binary.left, coeff_out)) {
        *rest_out = expr->u.binary.right;
        return true;
    }

    return false;
}

bool expr_binding_expr_split_leading_number(const expr_binding_expr_t *expr,
                                          number_t *coeff_out,
                                          expr_binding_expr_t **rest_out)
{
    number_t coeff;
    number_t right_coeff;
    number_t folded;
    const expr_binding_expr_t *rest = NULL;
    long numer;
    long denom;
    expr_binding_const_id_t const_id;

    if (!expr || !coeff_out || !rest_out)
        return false;

    if (expr_binding_expr_number_value(expr, &coeff)) {
        *coeff_out = coeff;
        *rest_out = NULL;
        return true;
    }

    if (expr->kind == EXPR_BINDING_EXPR_MUL) {
        if (expr_binding_expr_number_value(expr->u.binary.left, &coeff)) {
            *coeff_out = coeff;
            *rest_out = expr_binding_expr_clone(expr->u.binary.right);
            return true;
        }
        if (expr_binding_expr_number_value(expr->u.binary.right, &coeff)) {
            *coeff_out = coeff;
            *rest_out = expr_binding_expr_clone(expr->u.binary.left);
            return true;
        }
    }

    if (expr->kind == EXPR_BINDING_EXPR_DIV &&
        expr_binding_expr_number_value(expr->u.binary.right, &right_coeff)) {
        if (expr_binding_expr_split_leading_number(expr->u.binary.left, &coeff, rest_out)) {
            folded = num_scope_detach(num_div(coeff, right_coeff));
            num_destroy(&coeff);
            num_destroy(&right_coeff);
            *coeff_out = folded;
            return true;
        }
        num_destroy(&right_coeff);
    }

    if (expr->kind == EXPR_BINDING_EXPR_DIV &&
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
        *rest_out = expr_binding_expr_new_const(const_id);
        return true;
    }

    if (binding_expr_leading_number(expr, &coeff, &rest)) {
        *coeff_out = coeff;
        *rest_out = rest ? expr_binding_expr_clone(rest) : NULL;
        return true;
    }

    return false;
}


expr_binding_expr_t *expr_binding_expr_simplify(expr_binding_expr_t *expr)
{
    const binding_expr_ops_t *ops;

    if (!expr)
        return NULL;

    ops = binding_expr_ops_for_kind(expr->kind);
    return ops && ops->simplify ? ops->simplify(expr) : expr;
}

static void emit_binding_expr_mul_separator(const expr_binding_expr_t *left,
                                            const expr_binding_expr_t *right,
                                            sbuf_t *b)
{
    if (!(binding_expr_is_atomic(left) && binding_expr_is_atomic(right)))
        sbuf_puts(b, "·");
}

static void emit_binding_tex_mul_separator(const expr_binding_expr_t *left,
                                           const expr_binding_expr_t *right,
                                           sbuf_t *b)
{
    if (left && left->kind == EXPR_BINDING_EXPR_NUMBER &&
        binding_expr_is_const_id(right, EXPR_BINDING_CONST_I))
        return;
    if (binding_expr_is_atomic(left) && binding_expr_is_atomic(right))
        sbuf_putc(b, ' ');
    else
        sbuf_puts(b, " \\cdot ");
}

static void emit_binding_expr_mul(const expr_binding_expr_t *left,
                                  const expr_binding_expr_t *right,
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

static void emit_binding_tex_mul(const expr_binding_expr_t *left,
                                 const expr_binding_expr_t *right,
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

static void emit_binding_expr_number(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    (void)parent_prec;
    emit_binding_number_text(expr->u.text, b);
}

static void emit_binding_expr_const(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    (void)parent_prec;
    sbuf_puts(b, binding_const_expr_name(expr->u.const_id));
}

static void emit_binding_expr_neg(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = BIND_PREC_UNARY < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    sbuf_putc(b, '-');
    emit_binding_expr(expr->u.unary.child, b, BIND_PREC_UNARY);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_expr_addsub(const expr_binding_expr_t *expr,
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

static void emit_binding_expr_add(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_expr_addsub(expr, b, parent_prec, " + ", BIND_PREC_ADD);
}

static void emit_binding_expr_sub(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_expr_addsub(expr, b, parent_prec, " - ", BIND_PREC_ADD + 1);
}

static void emit_binding_expr_mul_node(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_expr_mul(expr->u.binary.left, expr->u.binary.right, b, parent_prec);
}

static void emit_binding_expr_const_ratio_value(expr_binding_const_id_t const_id,
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

static void emit_binding_expr_div(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = BIND_PREC_MUL < parent_prec;
    long numer;
    long denom;
    expr_binding_const_id_t const_id;

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

static void emit_binding_expr_powi(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool base_need = binding_expr_needs_pow_base_parens(expr->u.powi.base);

    if (expr->u.powi.exponent < 0) {
        bool need = BIND_PREC_MUL < parent_prec;
        long positive_exponent = -expr->u.powi.exponent;

        if (need)
            sbuf_putc(b, '(');
        sbuf_puts(b, "1/");
        if (positive_exponent == 1L) {
            emit_binding_expr(expr->u.powi.base, b, BIND_PREC_POW);
        } else {
            if (base_need)
                sbuf_putc(b, '(');
            emit_binding_expr(expr->u.powi.base, b,
                              base_need ? BIND_PREC_LOWEST : BIND_PREC_POW);
            if (base_need)
                sbuf_putc(b, ')');
            emit_binding_superscript_int(b, positive_exponent);
        }
        if (need)
            sbuf_putc(b, ')');
        return;
    }

    (void)parent_prec;
    if (base_need)
        sbuf_putc(b, '(');
    emit_binding_expr(expr->u.powi.base, b,
                      base_need ? BIND_PREC_LOWEST : BIND_PREC_POW);
    if (base_need)
        sbuf_putc(b, ')');
    emit_binding_superscript_int(b, expr->u.powi.exponent);
}

static void emit_binding_tex_number(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    string_t *clean;
    char *tex;
    binding_simple_rational_t rational = { false, NULL, NULL };

    (void)parent_prec;
    if (expr->u.text && strcmp(expr->u.text, "∞") == 0) {
        sbuf_puts(b, "\\infty");
        return;
    }
    if (expr->u.text && strcmp(expr->u.text, "-∞") == 0) {
        sbuf_puts(b, "-\\infty");
        return;
    }
    if (binding_text_is_simple_rational(expr->u.text, &rational)) {
        if (rational.negative)
            sbuf_putc(b, '-');
        sbuf_puts(b, "\\frac{");
        emit_binding_digits(b, rational.numer);
        sbuf_puts(b, "}{");
        emit_binding_digits(b, rational.denom);
        sbuf_putc(b, '}');
        binding_simple_rational_clear(&rational);
        return;
    }

    clean = binding_decimal_display_text(expr->u.text);
    tex = expr_tostring_texify(clean ? string_c_str(clean) : "");
    string_free(clean);
    if (tex) {
        sbuf_puts(b, tex);
        free(tex);
    }
}

static void emit_binding_tex_const(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    (void)parent_prec;
    sbuf_puts(b, binding_const_tex_name(expr->u.const_id));
}

static void emit_binding_tex_neg(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = BIND_PREC_UNARY < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    sbuf_putc(b, '-');
    emit_binding_tex_expr(expr->u.unary.child, b, BIND_PREC_UNARY);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_tex_addsub(const expr_binding_expr_t *expr,
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

static void emit_binding_tex_add(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_tex_addsub(expr, b, parent_prec, " + ", BIND_PREC_ADD);
}

static void emit_binding_tex_sub(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_tex_addsub(expr, b, parent_prec, " - ", BIND_PREC_ADD + 1);
}

static void emit_binding_tex_mul_node(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_tex_mul(expr->u.binary.left, expr->u.binary.right, b, parent_prec);
}

static void emit_binding_tex_const_ratio_numer(expr_binding_const_id_t const_id,
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

static void emit_binding_tex_const_ratio_value(expr_binding_const_id_t const_id,
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

static void emit_binding_tex_div(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    long numer;
    long denom;
    expr_binding_const_id_t const_id;

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

static void emit_binding_tex_powi(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool base_need = binding_expr_needs_pow_base_parens(expr->u.powi.base);
    char expbuf[64];

    (void)parent_prec;
    if (expr->u.powi.exponent < 0) {
        long positive_exponent = -expr->u.powi.exponent;

        sbuf_puts(b, "\\frac{1}{");
        if (positive_exponent == 1L) {
            emit_binding_tex_expr(expr->u.powi.base, b, BIND_PREC_LOWEST);
        } else {
            if (base_need)
                sbuf_puts(b, "\\left(");
            emit_binding_tex_expr(expr->u.powi.base, b,
                                  base_need ? BIND_PREC_LOWEST : BIND_PREC_POW);
            if (base_need)
                sbuf_puts(b, "\\right)");
            snprintf(expbuf, sizeof(expbuf), "%ld", positive_exponent);
            sbuf_puts(b, "^{");
            sbuf_puts(b, expbuf);
            sbuf_putc(b, '}');
        }
        sbuf_putc(b, '}');
        return;
    }

    if (base_need)
        sbuf_puts(b, "\\left(");
    emit_binding_tex_expr(expr->u.powi.base, b,
                          base_need ? BIND_PREC_LOWEST : BIND_PREC_POW);
    if (base_need)
        sbuf_puts(b, "\\right)");
    snprintf(expbuf, sizeof(expbuf), "%ld", expr->u.powi.exponent);
    sbuf_puts(b, "^{");
    sbuf_puts(b, expbuf);
    sbuf_putc(b, '}');
}

static void emit_binding_expr_unary_call(const expr_ops_t *ops,
                                         const expr_binding_expr_t *child,
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

static void emit_binding_tex_unary_call(const expr_ops_t *ops,
                                        const expr_binding_expr_t *child,
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

static void emit_binding_expr_unary_sqrt(const expr_ops_t *ops,
                                         const expr_binding_expr_t *child,
                                         sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "√(");
    emit_binding_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_binding_tex_unary_sqrt(const expr_ops_t *ops,
                                        const expr_binding_expr_t *child,
                                        sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "\\sqrt{");
    emit_binding_tex_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, '}');
}

static void emit_binding_expr_unary_abs(const expr_ops_t *ops,
                                        const expr_binding_expr_t *child,
                                        sbuf_t *b)
{
    (void)ops;
    sbuf_putc(b, '|');
    emit_binding_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, '|');
}

static void emit_binding_tex_unary_abs(const expr_ops_t *ops,
                                       const expr_binding_expr_t *child,
                                       sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "\\left|");
    emit_binding_tex_expr(child, b, BIND_PREC_LOWEST);
    sbuf_puts(b, "\\right|");
}

static void emit_binding_expr_unary_floor(const expr_ops_t *ops,
                                          const expr_binding_expr_t *child,
                                          sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "⌊");
    emit_binding_expr(child, b, BIND_PREC_LOWEST);
    sbuf_puts(b, "⌋");
}

static void emit_binding_tex_unary_floor(const expr_ops_t *ops,
                                         const expr_binding_expr_t *child,
                                         sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "\\left\\lfloor ");
    emit_binding_tex_expr(child, b, BIND_PREC_LOWEST);
    sbuf_puts(b, " \\right\\rfloor");
}

static void emit_binding_expr_unary_ceil(const expr_ops_t *ops,
                                         const expr_binding_expr_t *child,
                                         sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "⌈");
    emit_binding_expr(child, b, BIND_PREC_LOWEST);
    sbuf_puts(b, "⌉");
}

static void emit_binding_tex_unary_ceil(const expr_ops_t *ops,
                                        const expr_binding_expr_t *child,
                                        sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "\\left\\lceil ");
    emit_binding_tex_expr(child, b, BIND_PREC_LOWEST);
    sbuf_puts(b, " \\right\\rceil");
}

static bool emit_binding_tex_exp_unit_fraction_root(
    const expr_binding_expr_t *child, sbuf_t *b)
{
    number_t value;
    long numerator;
    long denominator;

    if (!expr_binding_expr_number_value(child, &value))
        return false;

    if (!num_get_small_rational(value, &numerator, &denominator) ||
        numerator != 1L ||
        denominator <= 1L) {
        num_destroy(&value);
        return false;
    }

    if (denominator == 2L) {
        sbuf_puts(b, "\\sqrt{e}");
    } else {
        char index_text[32];

        snprintf(index_text, sizeof(index_text), "%ld", denominator);
        sbuf_puts(b, "\\sqrt[");
        sbuf_puts(b, index_text);
        sbuf_puts(b, "]{e}");
    }
    num_destroy(&value);
    return true;
}

static void emit_binding_tex_unary_exp(const expr_ops_t *ops,
                                       const expr_binding_expr_t *child,
                                       sbuf_t *b)
{
    (void)ops;
    if (emit_binding_tex_exp_unit_fraction_root(child, b))
        return;

    sbuf_puts(b, "e^{");
    emit_binding_tex_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, '}');
}

static void emit_binding_expr_unary_neg_op(const expr_ops_t *ops,
                                           const expr_binding_expr_t *child,
                                           sbuf_t *b)
{
    (void)ops;
    sbuf_putc(b, '-');
    emit_binding_expr(child, b, BIND_PREC_UNARY);
}

static void emit_binding_tex_unary_neg_op(const expr_ops_t *ops,
                                          const expr_binding_expr_t *child,
                                          sbuf_t *b)
{
    (void)ops;
    sbuf_putc(b, '-');
    emit_binding_tex_expr(child, b, BIND_PREC_UNARY);
}

typedef void (*binding_unary_emit_fn)(const expr_ops_t *ops,
                                      const expr_binding_expr_t *child,
                                      sbuf_t *b);

typedef struct {
    binding_unary_emit_fn   emit_expr;
    binding_unary_emit_fn   emit_tex;
} binding_unary_render_t;

static const binding_unary_render_t s_binding_unary_renderers[EXPR_KIND_COUNT] = {
    [EXPR_KIND_NEG] = { emit_binding_expr_unary_neg_op, emit_binding_tex_unary_neg_op },
    [EXPR_KIND_SQRT] = { emit_binding_expr_unary_sqrt, emit_binding_tex_unary_sqrt },
    [EXPR_KIND_ABS] = { emit_binding_expr_unary_abs, emit_binding_tex_unary_abs },
    [EXPR_KIND_FLOOR] = { emit_binding_expr_unary_floor, emit_binding_tex_unary_floor },
    [EXPR_KIND_CEIL] = { emit_binding_expr_unary_ceil, emit_binding_tex_unary_ceil },
    [EXPR_KIND_EXP] = { emit_binding_expr_unary_call, emit_binding_tex_unary_exp }
};

static const binding_unary_render_t *binding_unary_renderer_for_ops(const expr_ops_t *ops)
{
    const binding_unary_render_t *renderer;

    if (!ops || (unsigned)ops->kind >= (unsigned)EXPR_KIND_COUNT)
        return NULL;
    renderer = &s_binding_unary_renderers[ops->kind];
    return renderer->emit_expr ? renderer : NULL;
}

static void emit_binding_expr_unary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
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

static void emit_binding_tex_unary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
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

static void emit_binding_expr_binary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    const expr_ops_t *ops = expr->u.binary_op.ops;

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

static void emit_binding_func_expr(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);

static void emit_binding_func_number(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_expr_number(expr, b, parent_prec);
}

static void emit_binding_func_const(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_expr_const(expr, b, parent_prec);
}

static void emit_binding_func_neg(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = BIND_PREC_UNARY < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    sbuf_putc(b, '-');
    emit_binding_func_expr(expr->u.unary.child, b, BIND_PREC_UNARY);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_func_addsub(const expr_binding_expr_t *expr,
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

static void emit_binding_func_add(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_func_addsub(expr, b, parent_prec, " + ", BIND_PREC_ADD);
}

static void emit_binding_func_sub(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_func_addsub(expr, b, parent_prec, " - ", BIND_PREC_ADD + 1);
}

static void emit_binding_func_mul_node(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
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

static void emit_binding_func_div(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
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

static void emit_binding_func_powi(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = BIND_PREC_POW < parent_prec;
    bool base_need = binding_expr_needs_pow_base_parens(expr->u.powi.base);
    char expbuf[64];

    if (expr->u.powi.exponent < 0) {
        bool recip_need = BIND_PREC_MUL < parent_prec;
        long positive_exponent = -expr->u.powi.exponent;

        if (recip_need)
            sbuf_putc(b, '(');
        sbuf_puts(b, "1 / ");
        if (positive_exponent == 1L) {
            emit_binding_func_expr(expr->u.powi.base, b, BIND_PREC_POW);
        } else {
            if (base_need)
                sbuf_putc(b, '(');
            emit_binding_func_expr(expr->u.powi.base, b,
                                   base_need ? BIND_PREC_LOWEST : BIND_PREC_POW);
            if (base_need)
                sbuf_putc(b, ')');
            snprintf(expbuf, sizeof(expbuf), "%ld", positive_exponent);
            sbuf_putc(b, '^');
            sbuf_puts(b, expbuf);
        }
        if (recip_need)
            sbuf_putc(b, ')');
        return;
    }

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

static void emit_binding_func_unary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    const expr_ops_t *ops = expr->u.unary_op.ops;
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

static void emit_binding_func_binary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    const expr_ops_t *ops = expr->u.binary_op.ops;

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

static void emit_binding_tex_binary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    const expr_ops_t *ops = expr->u.binary_op.ops;

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

static void emit_binding_expr(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
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

static void emit_binding_func_expr(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
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

static void emit_binding_tex_expr(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
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

char *expr_binding_expr_to_string(const expr_binding_expr_t *expr)
{
    sbuf_t b;

    sbuf_init(&b);
    emit_binding_expr(expr, &b, BIND_PREC_LOWEST);
    {
        char *out = sbuf_to_c_string(&b);
        sbuf_free(&b);
        return out;
    }
}

char *expr_binding_expr_to_function_string(const expr_binding_expr_t *expr)
{
    sbuf_t b;

    sbuf_init(&b);
    emit_binding_func_expr(expr, &b, BIND_PREC_LOWEST);
    {
        char *out = sbuf_to_c_string(&b);
        sbuf_free(&b);
        return out;
    }
}

char *expr_binding_expr_to_tex(const expr_binding_expr_t *expr)
{
    sbuf_t b;

    sbuf_init(&b);
    emit_binding_tex_expr(expr, &b, BIND_PREC_LOWEST);
    {
        char *out = sbuf_to_c_string(&b);
        sbuf_free(&b);
        return out;
    }
}
