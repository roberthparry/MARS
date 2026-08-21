#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr_binding_simplify.h"
#include "expr_bindings.h"
#include "expression.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#include "expr_stringin_scan.h"
#include "expr_stringout.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#include "ustring.h"

typedef struct {
    string_cursor_t *cursor;
    int error;
    string_t *errmsg;
} binding_parser_t;

typedef enum {
    BIND_PREC_LOWEST = 0,
    BIND_PREC_ADD = 1,
    BIND_PREC_MUL = 2,
    BIND_PREC_POW = 3,
    BIND_PREC_UNARY = 4,
    BIND_PREC_ATOM = 5
} binding_prec_t;

#define BINDING_CONST_COUNT 5u
#define BINDING_EXPR_KIND_COUNT 11u

typedef struct {
    expr_binding_const_id_t id;
    const char *canonical_name;
    const char *expr_name;
    const char *TeX_name;
    const number_t *value;
} binding_const_meta_t;

typedef struct {
    int precedence;
    bool atomic;
    void (*free_payload)(expr_binding_expr_t *expr);
    expr_binding_expr_t *(*clone)(const expr_binding_expr_t *expr);
    expr_t *(*eval_expr)(const expr_binding_expr_t *expr);
    bool (*number_value)(const expr_binding_expr_t *expr, number_t *out);
    expr_binding_expr_t *(*simplify)(expr_binding_expr_t *expr);
    bool (*struct_eq)(const expr_binding_expr_t *left, const expr_binding_expr_t *right);
    bool (*numeric_literal)(const expr_binding_expr_t *expr);
    bool (*exact_complex)(const expr_binding_expr_t *expr, binding_exact_complex_t *out);
    bool (*explicit_mul_separator)(const expr_binding_expr_t *expr);
    void (*emit_expr)(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
    void (*emit_func)(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
    void (*emit_TeX)(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
} binding_expr_ops_t;

typedef number_t (*binding_number_unary_fn)(number_t);
typedef number_t (*binding_number_binary_fn)(number_t, number_t);

static const char *s_binding_sup_digits[10] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};

static const char *s_binding_sub_digits[10] = {"₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉"};

static const binding_const_meta_t s_binding_consts[BINDING_CONST_COUNT] = {
    [EXPR_BINDING_CONST_E] = {EXPR_BINDING_CONST_E, "e", "e", "e", &NUM_E},
    [EXPR_BINDING_CONST_I] = {EXPR_BINDING_CONST_I, "i", "i", "i", &NUM_I},
    [EXPR_BINDING_CONST_PI] = {EXPR_BINDING_CONST_PI, "@pi", "π", "\\pi", &NUM_PI},
    [EXPR_BINDING_CONST_PHI] = {EXPR_BINDING_CONST_PHI, "@phi", "φ", "\\phi", &NUM_PHI},
    [EXPR_BINDING_CONST_GAMMA] = {EXPR_BINDING_CONST_GAMMA, "@gamma", "γ", "\\gamma", &NUM_EULER_MASCHERONI}};

static const binding_const_meta_t *binding_const_meta(expr_binding_const_id_t const_id)
{
    if ((unsigned)const_id >= BINDING_CONST_COUNT || s_binding_consts[const_id].value == NULL)
        return NULL;
    return &s_binding_consts[const_id];
}

static const char *binding_const_expr_name(expr_binding_const_id_t const_id)
{
    const binding_const_meta_t *meta = binding_const_meta(const_id);

    return meta ? meta->expr_name : "?";
}

static const char *binding_const_TeX_name(expr_binding_const_id_t const_id)
{
    const binding_const_meta_t *meta = binding_const_meta(const_id);

    return meta ? meta->TeX_name : "?";
}

static void binding_free_none(expr_binding_expr_t *expr);
static void binding_free_array(expr_binding_expr_t *expr);
static void binding_free_number(expr_binding_expr_t *expr);
static void binding_free_unary(expr_binding_expr_t *expr);
static void binding_free_binary(expr_binding_expr_t *expr);
static void binding_free_powi(expr_binding_expr_t *expr);
static void binding_free_unary_op(expr_binding_expr_t *expr);
static void binding_free_binary_op(expr_binding_expr_t *expr);

static expr_binding_expr_t *binding_clone_number(const expr_binding_expr_t *expr);
static expr_binding_expr_t *binding_clone_array(const expr_binding_expr_t *expr);
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
static expr_t *binding_eval_array(const expr_binding_expr_t *expr);
static expr_t *binding_eval_const(const expr_binding_expr_t *expr);
static expr_t *binding_eval_neg(const expr_binding_expr_t *expr);
static expr_t *binding_eval_add(const expr_binding_expr_t *expr);
static expr_t *binding_eval_sub(const expr_binding_expr_t *expr);
static expr_t *binding_eval_mul(const expr_binding_expr_t *expr);
static expr_t *binding_eval_div(const expr_binding_expr_t *expr);
static expr_t *binding_eval_powi(const expr_binding_expr_t *expr);
static expr_t *binding_eval_unary_op(const expr_binding_expr_t *expr);
static expr_t *binding_eval_binary_op(const expr_binding_expr_t *expr);

static bool binding_number_value_number(const expr_binding_expr_t *expr, number_t *out);
static bool binding_number_value_false(const expr_binding_expr_t *expr, number_t *out);
static bool binding_number_value_neg(const expr_binding_expr_t *expr, number_t *out);
static bool binding_number_value_add(const expr_binding_expr_t *expr, number_t *out);
static bool binding_number_value_sub(const expr_binding_expr_t *expr, number_t *out);
static bool binding_number_value_mul(const expr_binding_expr_t *expr, number_t *out);
static bool binding_number_value_div(const expr_binding_expr_t *expr, number_t *out);
static bool binding_number_value_powi(const expr_binding_expr_t *expr, number_t *out);

static bool binding_struct_eq_number(const expr_binding_expr_t *left, const expr_binding_expr_t *right);
static bool binding_struct_eq_array(const expr_binding_expr_t *left, const expr_binding_expr_t *right);
static bool binding_struct_eq_const(const expr_binding_expr_t *left, const expr_binding_expr_t *right);
static bool binding_struct_eq_unary(const expr_binding_expr_t *left, const expr_binding_expr_t *right);
static bool binding_struct_eq_binary(const expr_binding_expr_t *left, const expr_binding_expr_t *right);
static bool binding_struct_eq_powi(const expr_binding_expr_t *left, const expr_binding_expr_t *right);
static bool binding_struct_eq_unary_op(const expr_binding_expr_t *left, const expr_binding_expr_t *right);
static bool binding_struct_eq_binary_op(const expr_binding_expr_t *left, const expr_binding_expr_t *right);

static bool binding_numeric_literal_true(const expr_binding_expr_t *expr);
static bool binding_numeric_literal_const(const expr_binding_expr_t *expr);
static bool binding_numeric_literal_unary(const expr_binding_expr_t *expr);
static bool binding_numeric_literal_binary(const expr_binding_expr_t *expr);
static bool binding_numeric_literal_powi(const expr_binding_expr_t *expr);
static bool binding_numeric_literal_false(const expr_binding_expr_t *expr);

static bool binding_exact_complex_number(const expr_binding_expr_t *expr, binding_exact_complex_t *out);
static bool binding_exact_complex_const(const expr_binding_expr_t *expr, binding_exact_complex_t *out);
static bool binding_exact_complex_unary(const expr_binding_expr_t *expr, binding_exact_complex_t *out);
static bool binding_exact_complex_addsub(const expr_binding_expr_t *expr, binding_exact_complex_t *out);
static bool binding_exact_complex_mul(const expr_binding_expr_t *expr, binding_exact_complex_t *out);
static bool binding_exact_complex_div(const expr_binding_expr_t *expr, binding_exact_complex_t *out);
static bool binding_exact_complex_powi(const expr_binding_expr_t *expr, binding_exact_complex_t *out);

static bool binding_explicit_mul_separator_false(const expr_binding_expr_t *expr);
static bool binding_explicit_mul_separator_true(const expr_binding_expr_t *expr);
static bool binding_explicit_mul_separator_unary(const expr_binding_expr_t *expr);
static bool binding_explicit_mul_separator_mul(const expr_binding_expr_t *expr);
static bool binding_explicit_mul_separator_powi(const expr_binding_expr_t *expr);

static void emit_binding_expr_number(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_expr_array(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
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
static void emit_binding_func_array(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_const(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_neg(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_add(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_sub(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_mul_node(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_div(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_powi(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_unary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_binary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_func_expr(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);

static void emit_binding_TeX_number(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_TeX_array(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_TeX_const(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_TeX_neg(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_TeX_add(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_TeX_sub(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_TeX_mul_node(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_TeX_div(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_TeX_powi(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_TeX_unary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static void emit_binding_TeX_binary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);
static const binding_expr_ops_t s_binding_expr_ops[BINDING_EXPR_KIND_COUNT] = {
    [EXPR_BINDING_EXPR_ARRAY] = {.precedence = BIND_PREC_ATOM,
                                 .atomic = true,
                                 .free_payload = binding_free_array,
                                 .clone = binding_clone_array,
                                 .eval_expr = binding_eval_array,
                                 .number_value = binding_number_value_false,
                                 .simplify = expr_binding_simplify_atom,
                                 .struct_eq = binding_struct_eq_array,
                                 .numeric_literal = binding_numeric_literal_false,
                                 .exact_complex = NULL,
                                 .explicit_mul_separator = binding_explicit_mul_separator_false,
                                 .emit_expr = emit_binding_expr_array,
                                 .emit_func = emit_binding_func_array,
                                 .emit_TeX = emit_binding_TeX_array},
    [EXPR_BINDING_EXPR_NUMBER] = {.precedence = BIND_PREC_ATOM,
                                  .atomic = true,
                                  .free_payload = binding_free_number,
                                  .clone = binding_clone_number,
                                  .eval_expr = binding_eval_number,
                                  .number_value = binding_number_value_number,
                                  .simplify = expr_binding_simplify_atom,
                                  .struct_eq = binding_struct_eq_number,
                                  .numeric_literal = binding_numeric_literal_true,
                                  .exact_complex = binding_exact_complex_number,
                                  .explicit_mul_separator = binding_explicit_mul_separator_false,
                                  .emit_expr = emit_binding_expr_number,
                                  .emit_func = emit_binding_func_number,
                                  .emit_TeX = emit_binding_TeX_number},
    [EXPR_BINDING_EXPR_CONST] = {.precedence = BIND_PREC_ATOM,
                                 .atomic = true,
                                 .free_payload = binding_free_none,
                                 .clone = binding_clone_const,
                                 .eval_expr = binding_eval_const,
                                 .number_value = binding_number_value_false,
                                 .simplify = expr_binding_simplify_atom,
                                 .struct_eq = binding_struct_eq_const,
                                 .numeric_literal = binding_numeric_literal_const,
                                 .exact_complex = binding_exact_complex_const,
                                 .explicit_mul_separator = binding_explicit_mul_separator_false,
                                 .emit_expr = emit_binding_expr_const,
                                 .emit_func = emit_binding_func_const,
                                 .emit_TeX = emit_binding_TeX_const},
    [EXPR_BINDING_EXPR_NEG] = {.precedence = BIND_PREC_UNARY,
                               .atomic = false,
                               .free_payload = binding_free_unary,
                               .clone = binding_clone_neg,
                               .eval_expr = binding_eval_neg,
                               .number_value = binding_number_value_neg,
                               .simplify = expr_binding_simplify_neg,
                               .struct_eq = binding_struct_eq_unary,
                               .numeric_literal = binding_numeric_literal_unary,
                               .exact_complex = binding_exact_complex_unary,
                               .explicit_mul_separator = binding_explicit_mul_separator_unary,
                               .emit_expr = emit_binding_expr_neg,
                               .emit_func = emit_binding_func_neg,
                               .emit_TeX = emit_binding_TeX_neg},
    [EXPR_BINDING_EXPR_ADD] = {.precedence = BIND_PREC_ADD,
                               .atomic = false,
                               .free_payload = binding_free_binary,
                               .clone = binding_clone_add,
                               .eval_expr = binding_eval_add,
                               .number_value = binding_number_value_add,
                               .simplify = expr_binding_simplify_addsub,
                               .struct_eq = binding_struct_eq_binary,
                               .numeric_literal = binding_numeric_literal_binary,
                               .exact_complex = binding_exact_complex_addsub,
                               .explicit_mul_separator = binding_explicit_mul_separator_false,
                               .emit_expr = emit_binding_expr_add,
                               .emit_func = emit_binding_func_add,
                               .emit_TeX = emit_binding_TeX_add},
    [EXPR_BINDING_EXPR_SUB] = {.precedence = BIND_PREC_ADD,
                               .atomic = false,
                               .free_payload = binding_free_binary,
                               .clone = binding_clone_sub,
                               .eval_expr = binding_eval_sub,
                               .number_value = binding_number_value_sub,
                               .simplify = expr_binding_simplify_addsub,
                               .struct_eq = binding_struct_eq_binary,
                               .numeric_literal = binding_numeric_literal_binary,
                               .exact_complex = binding_exact_complex_addsub,
                               .explicit_mul_separator = binding_explicit_mul_separator_false,
                               .emit_expr = emit_binding_expr_sub,
                               .emit_func = emit_binding_func_sub,
                               .emit_TeX = emit_binding_TeX_sub},
    [EXPR_BINDING_EXPR_MUL] = {.precedence = BIND_PREC_MUL,
                               .atomic = false,
                               .free_payload = binding_free_binary,
                               .clone = binding_clone_mul,
                               .eval_expr = binding_eval_mul,
                               .number_value = binding_number_value_mul,
                               .simplify = expr_binding_simplify_mul,
                               .struct_eq = binding_struct_eq_binary,
                               .numeric_literal = binding_numeric_literal_binary,
                               .exact_complex = binding_exact_complex_mul,
                               .explicit_mul_separator = binding_explicit_mul_separator_mul,
                               .emit_expr = emit_binding_expr_mul_node,
                               .emit_func = emit_binding_func_mul_node,
                               .emit_TeX = emit_binding_TeX_mul_node},
    [EXPR_BINDING_EXPR_DIV] = {.precedence = BIND_PREC_MUL,
                               .atomic = false,
                               .free_payload = binding_free_binary,
                               .clone = binding_clone_div,
                               .eval_expr = binding_eval_div,
                               .number_value = binding_number_value_div,
                               .simplify = expr_binding_simplify_div,
                               .struct_eq = binding_struct_eq_binary,
                               .numeric_literal = binding_numeric_literal_binary,
                               .exact_complex = binding_exact_complex_div,
                               .explicit_mul_separator = binding_explicit_mul_separator_true,
                               .emit_expr = emit_binding_expr_div,
                               .emit_func = emit_binding_func_div,
                               .emit_TeX = emit_binding_TeX_div},
    [EXPR_BINDING_EXPR_POWI] = {.precedence = BIND_PREC_POW,
                                .atomic = true,
                                .free_payload = binding_free_powi,
                                .clone = binding_clone_powi,
                                .eval_expr = binding_eval_powi,
                                .number_value = binding_number_value_powi,
                                .simplify = expr_binding_simplify_powi,
                                .struct_eq = binding_struct_eq_powi,
                                .numeric_literal = binding_numeric_literal_powi,
                                .exact_complex = binding_exact_complex_powi,
                                .explicit_mul_separator = binding_explicit_mul_separator_powi,
                                .emit_expr = emit_binding_expr_powi,
                                .emit_func = emit_binding_func_powi,
                                .emit_TeX = emit_binding_TeX_powi},
    [EXPR_BINDING_EXPR_UNARY_OP] = {.precedence = BIND_PREC_UNARY,
                                    .atomic = false,
                                    .free_payload = binding_free_unary_op,
                                    .clone = binding_clone_unary_op,
                                    .eval_expr = binding_eval_unary_op,
                                    .number_value = binding_number_value_false,
                                    .simplify = expr_binding_simplify_unary_op,
                                    .struct_eq = binding_struct_eq_unary_op,
                                    .numeric_literal = binding_numeric_literal_false,
                                    .exact_complex = NULL,
                                    .explicit_mul_separator = binding_explicit_mul_separator_false,
                                    .emit_expr = emit_binding_expr_unary_op,
                                    .emit_func = emit_binding_func_unary_op,
                                    .emit_TeX = emit_binding_TeX_unary_op},
    [EXPR_BINDING_EXPR_BINARY_OP] = {.precedence = BIND_PREC_POW,
                                     .atomic = false,
                                     .free_payload = binding_free_binary_op,
                                     .clone = binding_clone_binary_op,
                                     .eval_expr = binding_eval_binary_op,
                                     .number_value = binding_number_value_false,
                                     .simplify = expr_binding_simplify_binary_op,
                                     .struct_eq = binding_struct_eq_binary_op,
                                     .numeric_literal = binding_numeric_literal_false,
                                     .exact_complex = NULL,
                                     .explicit_mul_separator = binding_explicit_mul_separator_false,
                                     .emit_expr = emit_binding_expr_binary_op,
                                     .emit_func = emit_binding_func_binary_op,
                                     .emit_TeX = emit_binding_TeX_binary_op}};

static const binding_expr_ops_t *binding_expr_ops_for_kind(expr_binding_expr_kind_t kind)
{
    if ((unsigned)kind >= BINDING_EXPR_KIND_COUNT || s_binding_expr_ops[kind].eval_expr == NULL)
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

static expr_binding_expr_t *expr_binding_expr_new_array(expr_binding_expr_t **items, size_t count, bool unspecified)
{
    expr_binding_expr_t *expr = binding_expr_alloc(EXPR_BINDING_EXPR_ARRAY);

    expr->u.array.items = items;
    expr->u.array.count = count;
    expr->u.array.unspecified = unspecified;
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

expr_binding_expr_t *expr_binding_expr_new_binary_op(const expr_ops_t *ops, expr_binding_expr_t *left,
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

static void binding_free_array(expr_binding_expr_t *expr)
{
    for (size_t i = 0u; i < expr->u.array.count; ++i)
        expr_binding_expr_free(expr->u.array.items[i]);
    free(expr->u.array.items);
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
    return ctor(expr_binding_expr_clone(expr->u.binary.left), expr_binding_expr_clone(expr->u.binary.right));
}

static expr_binding_expr_t *binding_clone_number(const expr_binding_expr_t *expr)
{
    return expr_binding_expr_new_number_text(expr->u.text);
}

static expr_binding_expr_t *binding_clone_array(const expr_binding_expr_t *expr)
{
    expr_binding_expr_t **items = NULL;

    if (expr->u.array.count > 0u) {
        items = calloc(expr->u.array.count, sizeof(*items));
        if (!items)
            abort();
        for (size_t i = 0u; i < expr->u.array.count; ++i)
            items[i] = expr_binding_expr_clone(expr->u.array.items[i]);
    }
    return expr_binding_expr_new_array(items, expr->u.array.count, expr->u.array.unspecified);
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
    return expr_binding_expr_new_powi(expr_binding_expr_clone(expr->u.powi.base), expr->u.powi.exponent);
}

static expr_binding_expr_t *binding_clone_unary_op(const expr_binding_expr_t *expr)
{
    return expr_binding_expr_new_unary_op(expr->u.unary_op.ops, expr_binding_expr_clone(expr->u.unary_op.child));
}

static expr_binding_expr_t *binding_clone_binary_op(const expr_binding_expr_t *expr)
{
    return expr_binding_expr_new_binary_op(expr->u.binary_op.ops, expr_binding_expr_clone(expr->u.binary_op.left),
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

static bool binding_cursor_peek_ascii_digit(const string_cursor_t *cursor, char *out)
{
    char ch = '\0';

    if (!rune_to_ascii(string_cursor_peek(cursor), &ch) || ch < '0' || ch > '9')
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
    return binding_ascii_is_alpha(ch) || binding_ascii_is_digit(ch) || ch == '_' || ch == '-';
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

    if (rune_is_equal(string_cursor_peek(cursor), '+') || rune_is_equal(string_cursor_peek(cursor), '-'))
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

    if (rune_is_equal(string_cursor_peek(cursor), 'e') || rune_is_equal(string_cursor_peek(cursor), 'E')) {
        have_decimal_marker = true;
        (void)string_cursor_next(cursor);
        if (rune_is_equal(string_cursor_peek(cursor), '+') || rune_is_equal(string_cursor_peek(cursor), '-'))
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

    have_digit = have_digit && have_decimal_marker && string_cursor_done(cursor);
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

    if (rune_is_equal(string_cursor_peek(cursor), '+') || rune_is_equal(string_cursor_peek(cursor), '-')) {
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

    if (rune_is_equal(string_cursor_peek(cursor), 'e') || rune_is_equal(string_cursor_peek(cursor), 'E')) {
        bool exp_negative = false;

        (void)string_cursor_next(cursor);
        if (rune_is_equal(string_cursor_peek(cursor), '+') || rune_is_equal(string_cursor_peek(cursor), '-')) {
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

static expr_t *binding_eval_array(const expr_binding_expr_t *expr)
{
    (void)expr;
    return expr_new_const(NUM_NAN);
}

static expr_t *binding_eval_const(const expr_binding_expr_t *expr)
{
    number_t value = binding_const_number(expr->u.const_id);
    expr_t *node = expr_new_named_const(value, binding_const_expr_name(expr->u.const_id));

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

static expr_t *binding_eval_binary(const expr_binding_expr_t *expr, expr_t *(*op)(const expr_t *, const expr_t *))
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

bool binding_expr_is_const_id(const expr_binding_expr_t *expr, expr_binding_const_id_t const_id)
{
    return expr && expr->kind == EXPR_BINDING_EXPR_CONST && expr->u.const_id == const_id;
}

bool expr_binding_expr_struct_eq(const expr_binding_expr_t *left, const expr_binding_expr_t *right)
{
    const binding_expr_ops_t *ops;

    if (left == right)
        return true;
    if (!left || !right || left->kind != right->kind)
        return false;

    ops = binding_expr_ops_for_kind(left->kind);
    return ops && ops->struct_eq ? ops->struct_eq(left, right) : false;
}

static bool binding_struct_eq_number(const expr_binding_expr_t *left, const expr_binding_expr_t *right)
{
    if (!left->u.text || !right->u.text)
        return left->u.text == right->u.text;
    return strcmp(left->u.text, right->u.text) == 0;
}

static bool binding_struct_eq_array(const expr_binding_expr_t *left, const expr_binding_expr_t *right)
{
    if (left->u.array.unspecified != right->u.array.unspecified || left->u.array.count != right->u.array.count)
        return false;
    for (size_t i = 0u; i < left->u.array.count; ++i) {
        if (!expr_binding_expr_struct_eq(left->u.array.items[i], right->u.array.items[i]))
            return false;
    }
    return true;
}

static bool binding_struct_eq_const(const expr_binding_expr_t *left, const expr_binding_expr_t *right)
{
    return left->u.const_id == right->u.const_id;
}

static bool binding_struct_eq_unary(const expr_binding_expr_t *left, const expr_binding_expr_t *right)
{
    return expr_binding_expr_struct_eq(left->u.unary.child, right->u.unary.child);
}

static bool binding_struct_eq_binary(const expr_binding_expr_t *left, const expr_binding_expr_t *right)
{
    return expr_binding_expr_struct_eq(left->u.binary.left, right->u.binary.left) &&
           expr_binding_expr_struct_eq(left->u.binary.right, right->u.binary.right);
}

static bool binding_struct_eq_powi(const expr_binding_expr_t *left, const expr_binding_expr_t *right)
{
    return left->u.powi.exponent == right->u.powi.exponent &&
           expr_binding_expr_struct_eq(left->u.powi.base, right->u.powi.base);
}

static bool binding_struct_eq_unary_op(const expr_binding_expr_t *left, const expr_binding_expr_t *right)
{
    return left->u.unary_op.ops == right->u.unary_op.ops &&
           expr_binding_expr_struct_eq(left->u.unary_op.child, right->u.unary_op.child);
}

static bool binding_struct_eq_binary_op(const expr_binding_expr_t *left, const expr_binding_expr_t *right)
{
    return left->u.binary_op.ops == right->u.binary_op.ops &&
           expr_binding_expr_struct_eq(left->u.binary_op.left, right->u.binary_op.left) &&
           expr_binding_expr_struct_eq(left->u.binary_op.right, right->u.binary_op.right);
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
    return expr && expr_binding_expr_needs_explicit_mul_separator(expr->u.unary.child);
}

static bool binding_explicit_mul_separator_mul(const expr_binding_expr_t *expr)
{
    return expr && expr_binding_expr_needs_explicit_mul_separator(expr->u.binary.left);
}

static bool binding_explicit_mul_separator_powi(const expr_binding_expr_t *expr)
{
    return expr && expr_binding_expr_needs_explicit_mul_separator(expr->u.powi.base);
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
    return ops && ops->explicit_mul_separator ? ops->explicit_mul_separator(expr) : false;
}

void expr_binding_exact_complex_clear(binding_exact_complex_t *value)
{
    if (!value)
        return;
    num_destroy(&value->imag);
    num_destroy(&value->real);
}

static void binding_exact_complex_set(binding_exact_complex_t *out, number_t real, number_t imag)
{
    out->real = num_scope_detach(real);
    out->imag = num_scope_detach(imag);
}

static bool binding_nearest_small_rational(number_t value, long *numerator_out, long *denominator_out)
{
    enum { MAX_DENOMINATOR = 64 };
    const double as_double = num_to_double(value);
    const double tolerance = 1e-12 * fmax(1.0, fabs(as_double));
    long denominator;

    if (!numerator_out || !denominator_out || !isfinite(as_double))
        return false;

    for (denominator = 1L; denominator <= MAX_DENOMINATOR; ++denominator) {
        const double scaled = as_double * (double)denominator;
        const double nearest = round(scaled);

        if (nearest < (double)LONG_MIN || nearest > (double)LONG_MAX)
            continue;
        if (fabs(as_double - nearest / (double)denominator) <= tolerance) {
            *numerator_out = (long)nearest;
            *denominator_out = denominator;
            return true;
        }
    }
    return false;
}

bool expr_exact_complex_rational_power(const binding_exact_complex_t *base, number_t exponent, number_t *value_out)
{
    NUM_SCOPE(scope);
    number_t base_value;
    number_t principal;
    number_t principal_real;
    number_t principal_imaginary;
    number_t candidate_real;
    number_t candidate_imaginary;
    number_t candidate;
    number_t candidate_power;
    number_t expected_power;
    number_t candidate_power_real;
    number_t candidate_power_imaginary;
    number_t expected_power_real;
    number_t expected_power_imaginary;
    long exponent_numerator;
    long exponent_denominator;
    long real_numerator;
    long real_denominator;
    long imaginary_numerator;
    long imaginary_denominator;
    bool verified;

    if (!base || !value_out || !num_get_small_rational(exponent, &exponent_numerator, &exponent_denominator) ||
        exponent_denominator <= 1L || exponent_denominator > INT_MAX || exponent_numerator < INT_MIN ||
        exponent_numerator > INT_MAX || num_is_zero(base->imag))
        return false;

    base_value = num_add(base->real, num_mul(base->imag, NUM_I));
    principal = num_pow(base_value, exponent);
    if (!num_is_finite(principal) || num_is_real(principal))
        return false;

    principal_real = num_real_part(principal);
    principal_imaginary = num_imag_part(principal);
    if (!binding_nearest_small_rational(principal_real, &real_numerator, &real_denominator) ||
        !binding_nearest_small_rational(principal_imaginary, &imaginary_numerator, &imaginary_denominator))
        return false;

    candidate_real = num_create_from_frac(real_numerator, real_denominator);
    candidate_imaginary = num_create_from_frac(imaginary_numerator, imaginary_denominator);
    candidate = num_add(candidate_real, num_mul(candidate_imaginary, NUM_I));
    candidate_power = num_pow_int(candidate, (int)exponent_denominator);
    expected_power = num_pow_int(base_value, (int)exponent_numerator);
    candidate_power_real = num_real_part(candidate_power);
    candidate_power_imaginary = num_imag_part(candidate_power);
    expected_power_real = num_real_part(expected_power);
    expected_power_imaginary = num_imag_part(expected_power);
    verified = num_eq(candidate_power_real, expected_power_real) &&
               num_eq(candidate_power_imaginary, expected_power_imaginary);
    if (verified) {
        num_destroy(value_out);
        *value_out = num_scope_detach(candidate);
    }
    return verified;
}

static bool binding_exact_complex_root_seed(const binding_exact_complex_t *base, long order, number_t *seed_out)
{
    enum { MAX_EXPLICIT_ROOTS = 32 };
    NUM_SCOPE(scope);
    number_t base_value;
    number_t reciprocal;
    number_t principal;

    if (!base || !seed_out || order < 2L || order > MAX_EXPLICIT_ROOTS)
        return false;

    base_value = num_add(base->real, num_mul(base->imag, NUM_I));
    reciprocal = num_create_from_frac(1L, order);
    principal = num_pow(base_value, reciprocal);
    if (!num_is_finite(principal))
        return false;

    for (long branch_index = 0L; branch_index < order; ++branch_index) {
        number_t angle = num_mul(num_create_from_frac(2L * branch_index, order), NUM_PI);
        number_t rotation = num_add(num_cos(angle), num_mul(num_sin(angle), NUM_I));
        number_t branch = num_mul(principal, rotation);
        number_t branch_real = num_real_part(branch);
        number_t branch_imaginary = num_imag_part(branch);
        long real_numerator;
        long real_denominator;
        long imaginary_numerator;
        long imaginary_denominator;

        if (binding_nearest_small_rational(branch_real, &real_numerator, &real_denominator) &&
            binding_nearest_small_rational(branch_imaginary, &imaginary_numerator, &imaginary_denominator)) {
            number_t candidate_real = num_create_from_frac(real_numerator, real_denominator);
            number_t candidate_imaginary = num_create_from_frac(imaginary_numerator, imaginary_denominator);
            number_t candidate = num_add(candidate_real, num_mul(candidate_imaginary, NUM_I));
            number_t candidate_power = num_pow_int(candidate, (int)order);
            number_t candidate_power_real = num_real_part(candidate_power);
            number_t candidate_power_imaginary = num_imag_part(candidate_power);

            if (num_eq(candidate_power_real, base->real) && num_eq(candidate_power_imaginary, base->imag)) {
                num_destroy(seed_out);
                *seed_out = num_scope_detach(candidate);
                return true;
            }
        }
    }
    return false;
}

bool expr_exact_complex_root_seed(const expr_t *expr, number_t *seed_out, long *order_out)
{
    binding_exact_complex_t base = {number_invalid(), number_invalid()};
    number_t exponent = number_invalid();
    const expr_t *base_expr = NULL;
    long numerator;
    long denominator;
    bool have_base = false;
    bool matched = false;
    bool named_root = false;
    bool success = false;

    if (!expr || !seed_out || !order_out)
        return false;

    if (expr->binding_expr && expr->binding_expr->kind == EXPR_BINDING_EXPR_BINARY_OP &&
        (expr->binding_expr->u.binary_op.ops == &ops_pow || expr->binding_expr->u.binary_op.ops == &ops_root)) {
        named_root = expr->binding_expr->u.binary_op.ops == &ops_root;
        have_base = expr_binding_expr_exact_complex(expr->binding_expr->u.binary_op.left, &base);
        matched = have_base && expr_binding_expr_number_value(expr->binding_expr->u.binary_op.right, &exponent);
    } else if (expr_is_op(expr, &ops_root) && expr->a && expr->b) {
        named_root = true;
        have_base = expr_exact_complex_value(expr->a, &base);
        matched = have_base && expr_match_const_value(expr->b, &exponent);
    } else if (expr_match_pow_const(expr, &base_expr, &exponent)) {
        have_base = expr_exact_complex_value(base_expr, &base);
        matched = have_base;
    }

    if (!matched || !num_get_small_rational(exponent, &numerator, &denominator))
        goto cleanup;
    if (named_root) {
        if (denominator != 1L)
            goto cleanup;
        denominator = numerator;
        numerator = 1L;
    }
    if (numerator != 1L ||
        denominator < 2L || !binding_exact_complex_root_seed(&base, denominator, seed_out))
        goto cleanup;

    *order_out = denominator;
    success = true;

cleanup:
    num_destroy(&exponent);
    if (have_base)
        expr_binding_exact_complex_clear(&base);
    else {
        num_destroy(&base.imag);
        num_destroy(&base.real);
    }
    return success;
}

/* Recover the authored base of an explicit reciprocal power. */
expr_t *expr_explicit_root_base(const expr_t *expr, long *order_out)
{
    number_t exponent = number_invalid();
    const expr_t *tree_base = NULL;
    expr_t *base = NULL;
    long numerator;
    long denominator;

    if (!expr || !order_out)
        return NULL;

    if (expr->binding_expr) {
        if (expr->binding_expr->kind != EXPR_BINDING_EXPR_BINARY_OP ||
            expr->binding_expr->u.binary_op.ops != &ops_pow ||
            !expr_binding_expr_number_value(expr->binding_expr->u.binary_op.right, &exponent)) {
            goto cleanup;
        }
        base = expr_binding_expr_eval_expr(expr->binding_expr->u.binary_op.left);
        if (base) {
            expr_binding_expr_free(base->binding_expr);
            base->binding_expr = expr_binding_expr_clone(expr->binding_expr->u.binary_op.left);
        }
    } else if (expr_match_pow_const(expr, &tree_base, &exponent)) {
        base = expr_clone(tree_base);
    }

    if (!base || !num_get_small_rational(exponent, &numerator, &denominator) || numerator != 1L || denominator < 2L) {
        expr_free(base);
        base = NULL;
        goto cleanup;
    }
    *order_out = denominator;

cleanup:
    num_destroy(&exponent);
    return base;
}

bool expr_explicit_root_order(const expr_t *expr, long *order_out)
{
    number_t exponent = number_invalid();
    const expr_t *base = NULL;
    long numerator;
    long denominator;
    bool matched = false;

    if (!expr || !order_out)
        return false;

    if (expr->binding_expr && expr->binding_expr->kind == EXPR_BINDING_EXPR_BINARY_OP &&
        expr->binding_expr->u.binary_op.ops == &ops_pow) {
        matched = expr_binding_expr_number_value(expr->binding_expr->u.binary_op.right, &exponent);
    } else {
        matched = expr_match_pow_const(expr, &base, &exponent);
    }

    if (!matched || !num_get_small_rational(exponent, &numerator, &denominator) || numerator != 1L ||
        denominator < 2L || denominator > 32L) {
        num_destroy(&exponent);
        return false;
    }

    num_destroy(&exponent);
    *order_out = denominator;
    return true;
}

static void binding_num_destroy_detached(number_t *value)
{
    if (!value)
        return;
    *value = num_scope_detach(*value);
    num_destroy(value);
}

bool expr_binding_expr_exact_complex(const expr_binding_expr_t *expr, binding_exact_complex_t *out)
{
    const binding_expr_ops_t *ops;

    if (!expr || !out)
        return false;

    ops = binding_expr_ops_for_kind(expr->kind);
    return ops && ops->exact_complex ? ops->exact_complex(expr, out) : false;
}

bool expr_exact_complex_value(const expr_t *expr, binding_exact_complex_t *out)
{
    binding_exact_complex_t left;
    binding_exact_complex_t right;

    if (!expr || !out)
        return false;

    if (expr->binding_expr && expr_binding_expr_exact_complex(expr->binding_expr, out))
        return true;

    if (expr_is_op(expr, &ops_const)) {
        number_t real = num_real_part(expr->c);
        number_t imaginary = num_imag_part(expr->c);

        if (!num_is_exact(real) || !num_is_exact(imaginary)) {
            num_destroy(&imaginary);
            num_destroy(&real);
            return false;
        }
        binding_exact_complex_set(out, real, imaginary);
        return true;
    }

    if (expr_is_op(expr, &ops_neg)) {
        if (!expr_exact_complex_value(expr->a, &left))
            return false;
        binding_exact_complex_set(out, num_neg(left.real), num_neg(left.imag));
        expr_binding_exact_complex_clear(&left);
        return true;
    }

    if (!expr_is_op(expr, &ops_add) && !expr_is_op(expr, &ops_sub) && !expr_is_op(expr, &ops_mul) &&
        !expr_is_op(expr, &ops_div))
        return false;
    if (!expr_exact_complex_value(expr->a, &left))
        return false;
    if (!expr_exact_complex_value(expr->b, &right)) {
        expr_binding_exact_complex_clear(&left);
        return false;
    }

    if (expr_is_op(expr, &ops_add) || expr_is_op(expr, &ops_sub)) {
        const bool subtract = expr_is_op(expr, &ops_sub);

        binding_exact_complex_set(out, subtract ? num_sub(left.real, right.real) : num_add(left.real, right.real),
                                  subtract ? num_sub(left.imag, right.imag) : num_add(left.imag, right.imag));
    } else if (expr_is_op(expr, &ops_mul)) {
        binding_exact_complex_set(out, num_sub(num_mul(left.real, right.real), num_mul(left.imag, right.imag)),
                                  num_add(num_mul(left.real, right.imag), num_mul(left.imag, right.real)));
    } else {
        number_t denominator = num_add(num_mul(right.real, right.real), num_mul(right.imag, right.imag));

        if (num_is_zero(denominator)) {
            num_destroy(&denominator);
            expr_binding_exact_complex_clear(&right);
            expr_binding_exact_complex_clear(&left);
            return false;
        }
        binding_exact_complex_set(
            out, num_div(num_add(num_mul(left.real, right.real), num_mul(left.imag, right.imag)), denominator),
            num_div(num_sub(num_mul(left.imag, right.real), num_mul(left.real, right.imag)), denominator));
        num_destroy(&denominator);
    }

    expr_binding_exact_complex_clear(&right);
    expr_binding_exact_complex_clear(&left);
    return true;
}

static bool binding_string_is_single_ascii(const string_t *text, char ch)
{
    return text && string_length(text) == 1u && rune_is_equal(string_at(text, 0u), ch);
}

static bool binding_number_string_exact_complex(const string_t *text, binding_exact_complex_t *out)
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

    if (have_rune && (rune_is_equal(last, 'i') || rune_is_equal(last, 'I'))) {
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
            binding_num_destroy_detached(&value);
            string_cursor_free(cursor);
            return false;
        }
        binding_exact_complex_set(out, num_clone(NUM_ZERO), value);
        string_cursor_free(cursor);
        return true;
    }

    value = binding_number_from_string(text);
    if (!num_is_exact(value) || !num_is_real(value)) {
        binding_num_destroy_detached(&value);
        string_cursor_free(cursor);
        return false;
    }
    binding_exact_complex_set(out, value, num_clone(NUM_ZERO));
    string_cursor_free(cursor);
    return true;
}

static bool binding_number_text_exact_complex(const char *text, binding_exact_complex_t *out)
{
    string_t *string = text ? string_new_with(text) : NULL;
    bool ok;

    if (!string)
        return false;

    ok = binding_number_string_exact_complex(string, out);
    string_free(string);
    return ok;
}

static bool binding_exact_complex_number(const expr_binding_expr_t *expr, binding_exact_complex_t *out)
{
    return binding_number_text_exact_complex(expr->u.text, out);
}

static bool binding_exact_complex_const(const expr_binding_expr_t *expr, binding_exact_complex_t *out)
{
    if (expr->u.const_id != EXPR_BINDING_CONST_I)
        return false;
    binding_exact_complex_set(out, num_clone(NUM_ZERO), num_clone(NUM_ONE));
    return true;
}

static bool binding_exact_complex_unary(const expr_binding_expr_t *expr, binding_exact_complex_t *out)
{
    binding_exact_complex_t child;

    if (!expr_binding_expr_exact_complex(expr->u.unary.child, &child))
        return false;

    binding_exact_complex_set(out, num_neg(child.real), num_neg(child.imag));
    expr_binding_exact_complex_clear(&child);
    return true;
}

static bool binding_exact_complex_addsub(const expr_binding_expr_t *expr, binding_exact_complex_t *out)
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

    binding_exact_complex_set(out, subtract ? num_sub(left.real, right.real) : num_add(left.real, right.real),
                              subtract ? num_sub(left.imag, right.imag) : num_add(left.imag, right.imag));
    expr_binding_exact_complex_clear(&right);
    expr_binding_exact_complex_clear(&left);
    return true;
}

static bool binding_exact_complex_mul(const expr_binding_expr_t *expr, binding_exact_complex_t *out)
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

    binding_exact_complex_set(out, num_sub(num_mul(left.real, right.real), num_mul(left.imag, right.imag)),
                              num_add(num_mul(left.real, right.imag), num_mul(left.imag, right.real)));
    expr_binding_exact_complex_clear(&right);
    expr_binding_exact_complex_clear(&left);
    return true;
}

static bool binding_exact_complex_div(const expr_binding_expr_t *expr, binding_exact_complex_t *out)
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

    denom = num_add(num_mul(right.real, right.real), num_mul(right.imag, right.imag));
    if (num_is_zero(denom)) {
        expr_binding_exact_complex_clear(&right);
        expr_binding_exact_complex_clear(&left);
        return false;
    }

    binding_exact_complex_set(out,
                              num_div(num_add(num_mul(left.real, right.real), num_mul(left.imag, right.imag)), denom),
                              num_div(num_sub(num_mul(left.imag, right.real), num_mul(left.real, right.imag)), denom));
    expr_binding_exact_complex_clear(&right);
    expr_binding_exact_complex_clear(&left);
    return true;
}

static bool binding_exact_complex_powi(const expr_binding_expr_t *expr, binding_exact_complex_t *out)
{
    binding_exact_complex_t result;
    binding_exact_complex_t base;

    if (expr->u.powi.exponent < 0 || !expr_binding_expr_exact_complex(expr->u.powi.base, &base))
        return false;

    binding_exact_complex_set(&result, num_clone(NUM_ONE), num_clone(NUM_ZERO));
    for (long i = 0; i < expr->u.powi.exponent; ++i) {
        NUM_SCOPE(scope);
        binding_exact_complex_t next;

        binding_exact_complex_set(&next, num_sub(num_mul(result.real, base.real), num_mul(result.imag, base.imag)),
                                  num_add(num_mul(result.real, base.imag), num_mul(result.imag, base.real)));
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

bool binding_expr_scaled_const_ratio(const expr_binding_expr_t *expr, long *numer_out, long *denom_out,
                                     expr_binding_const_id_t *const_id_out);

static bool binding_expr_scaled_const(const expr_binding_expr_t *expr, long *factor_out,
                                      expr_binding_const_id_t *const_id_out)
{
    long numer;
    long denom;
    expr_binding_const_id_t const_id;

    if (!binding_expr_scaled_const_ratio(expr, &numer, &denom, &const_id) || denom != 1L)
        return false;

    *factor_out = numer;
    *const_id_out = const_id;
    return true;
}

bool binding_expr_scaled_const_ratio(const expr_binding_expr_t *expr, long *numer_out, long *denom_out,
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
        binding_expr_scaled_const_ratio(expr->u.unary.child, &nested_numer, &nested_denom, &const_id)) {
        *numer_out = -nested_numer;
        *denom_out = nested_denom;
        *const_id_out = const_id;
        return true;
    }

    if (expr->kind != EXPR_BINDING_EXPR_MUL)
        return false;

    if (!binding_number_text_to_small_rational(expr->u.binary.left, &factor_numer, &factor_denom) ||
        !binding_expr_scaled_const_ratio(expr->u.binary.right, &nested_numer, &nested_denom, &const_id)) {
        if (!binding_number_text_to_small_rational(expr->u.binary.right, &factor_numer, &factor_denom) ||
            !binding_expr_scaled_const_ratio(expr->u.binary.left, &nested_numer, &nested_denom, &const_id))
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

bool binding_const_ratio_parts(const expr_binding_expr_t *numer_expr, const expr_binding_expr_t *denom_expr,
                               long *numer_out, long *denom_out, expr_binding_const_id_t *const_id_out)
{
    long numer;
    long denom;
    long gcd;
    expr_binding_const_id_t const_id;

    if (!binding_expr_scaled_const(numer_expr, &numer, &const_id) || !binding_number_text_to_long(denom_expr, &denom) ||
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

static expr_t *binding_eval_known_pi_ratio(const expr_binding_expr_t *numer, const expr_binding_expr_t *denom)
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
    expr_t *known = binding_eval_known_pi_ratio(expr->u.binary.left, expr->u.binary.right);

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

static void binding_expr_store_cached_value(expr_binding_expr_t *expr, number_t value, size_t precision_bits)
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

    if (mutable_expr->cached_value_valid && mutable_expr->cached_precision_bits >= num_get_default_prec_bits()) {
        return num_clone(mutable_expr->cached_value);
    }

    value = binding_expr_compute_value(mutable_expr);
    binding_expr_store_cached_value(mutable_expr, value, num_get_default_prec_bits());
    return value;
}

bool expr_binding_expr_eval_if_precision_increased(expr_binding_expr_t *expr, number_t *value_out)
{
    number_t value;
    size_t precision_bits;

    if (!expr || !value_out)
        return false;

    precision_bits = num_get_default_prec_bits();
    if (expr->cached_value_valid && expr->cached_precision_bits >= precision_bits) {
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

static int binding_peek_value(const binding_parser_t *p, uint32_t *out, size_t *width_out)
{
    return expr_parse_cursor_peek_value(p ? p->cursor : NULL, out, width_out);
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
    const char *kw;
    bool is_binary;
    bool is_ternary;
    const expr_ops_t *ops;
} binding_func_entry_t;

#define BINDING_FUNC_TABLE_SIZE 167u

static const unsigned char s_binding_func_displacements[BINDING_FUNC_TABLE_SIZE] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 4,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 1, 0, 1, 0, 0,
    0, 7, 2, 0, 2, 0, 0, 0, 0, 0, 2, 1, 0, 5, 2, 0,
    0, 0, 0, 0, 13, 0, 0, 0, 1, 0, 0, 0, 2, 1, 0, 1,
    0, 4, 0, 0, 0, 0, 2, 0, 0, 3, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 15, 0, 0, 4, 0, 0, 0, 0, 0, 0, 10, 2, 137,
    0, 0, 0, 0, 0, 0, 0, 1, 0, 5, 0, 0, 1, 0, 0, 1,
    2, 0, 0, 13, 2, 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 0,
    0, 3, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 22, 0, 2, 1,
    0, 1, 1, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0,
    162, 0, 0, 0, 0, 0, 0};

static const binding_func_entry_t s_binding_funcs[BINDING_FUNC_TABLE_SIZE] = {
    [1] = {.kw = "zetap", .is_binary = false, .ops = &ops_zetap},
    [2] = {.kw = "sin", .is_binary = false, .ops = &ops_sin},
    [3] = {.kw = "versin", .is_binary = false, .ops = &ops_versin},
    [8] = {.kw = "atan2", .is_binary = true, .ops = &ops_atan2},
    [11] = {.kw = "acsc", .is_binary = false, .ops = &ops_acosec},
    [12] = {.kw = "gammainc_Q", .is_binary = true, .ops = &ops_gammainc_Q},
    [13] = {.kw = "AND", .is_binary = true, .ops = &ops_bit_and},
    [14] = {.kw = "acosec", .is_binary = false, .ops = &ops_acosec},
    [15] = {.kw = "gammainc_upper", .is_binary = true, .ops = &ops_gammainc_upper},
    [16] = {.kw = "acsch", .is_binary = false, .ops = &ops_acosech},
    [17] = {.kw = "SHL", .is_binary = true, .ops = &ops_shl},
    [18] = {.kw = "coth", .is_binary = false, .ops = &ops_coth},
    [19] = {.kw = "abs", .is_binary = false, .ops = &ops_abs},
    [20] = {.kw = "lg", .is_binary = false, .ops = &ops_log10},
    [21] = {.kw = "cosech", .is_binary = false, .ops = &ops_cosech},
    [22] = {.kw = "factors", .is_binary = false, .ops = &ops_factors},
    [23] = {.kw = "cos", .is_binary = false, .ops = &ops_cos},
    [24] = {.kw = "cosec", .is_binary = false, .ops = &ops_cosec},
    [25] = {.kw = "asec", .is_binary = false, .ops = &ops_asec},
    [26] = {.kw = "archavercos", .is_binary = false, .ops = &ops_archavercos},
    [27] = {.kw = "W0", .is_binary = false, .ops = &ops_lambert_w0},
    [28] = {.kw = "Li2", .is_binary = false, .ops = &ops_dilog},
    [30] = {.kw = "erfcinv", .is_binary = false, .ops = &ops_erfcinv},
    [31] = {.kw = "W₀", .is_binary = false, .ops = &ops_lambert_w0},
    [32] = {.kw = "pdf", .is_binary = false, .ops = &ops_pdf},
    [33] = {.kw = "lgamma", .is_binary = false, .ops = &ops_lgamma},
    [34] = {.kw = "lambert_wm1", .is_binary = false, .ops = &ops_lambert_wm1},
    [35] = {.kw = "csch", .is_binary = false, .ops = &ops_cosech},
    [36] = {.kw = "tanh", .is_binary = false, .ops = &ops_tanh},
    [37] = {.kw = "acot", .is_binary = false, .ops = &ops_acot},
    [38] = {.kw = "asinh", .is_binary = false, .ops = &ops_asinh},
    [40] = {.kw = "next_prime", .is_binary = false, .ops = &ops_next_prime},
    [41] = {.kw = "normal_pdf", .is_binary = false, .ops = &ops_normal_pdf},
    [42] = {.kw = "asin", .is_binary = false, .ops = &ops_asin},
    [43] = {.kw = "zeta", .is_binary = false, .ops = &ops_zeta},
    [44] = {.kw = "OR", .is_binary = true, .ops = &ops_bit_or},
    [45] = {.kw = "lcm", .is_binary = true, .ops = &ops_lcm},
    [46] = {.kw = "cot", .is_binary = false, .ops = &ops_cot},
    [47] = {.kw = "arccoversin", .is_binary = false, .ops = &ops_arccoversin},
    [48] = {.kw = "W₋₁", .is_binary = false, .ops = &ops_lambert_wm1},
    [49] = {.kw = "is_prime", .is_binary = false, .ops = &ops_is_prime},
    [50] = {.kw = "W-1", .is_binary = false, .ops = &ops_lambert_wm1},
    [51] = {.kw = "arcsch", .is_binary = false, .ops = &ops_acosech},
    [52] = {.kw = "tan", .is_binary = false, .ops = &ops_tan},
    [53] = {.kw = "acoth", .is_binary = false, .ops = &ops_acoth},
    [55] = {.kw = "Ei", .is_binary = false, .ops = &ops_Ei},
    [56] = {.kw = "haversin", .is_binary = false, .ops = &ops_haversin},
    [57] = {.kw = "bessel_j", .is_binary = true, .ops = &ops_bessel_j},
    [58] = {.kw = "sech", .is_binary = false, .ops = &ops_sech},
    [59] = {.kw = "sinh", .is_binary = false, .ops = &ops_sinh},
    [60] = {.kw = "lommel_s", .is_ternary = true, .ops = &ops_lommel_s},
    [61] = {.kw = "hacoversin", .is_binary = false, .ops = &ops_hacoversin},
    [62] = {.kw = "BesselJ", .is_binary = true, .ops = &ops_bessel_j},
    [63] = {.kw = "W_n", .is_binary = true, .ops = &ops_lambert_wn},
    [64] = {.kw = "arccsc", .is_binary = false, .ops = &ops_acosec},
    [65] = {.kw = "NOT", .is_binary = false, .ops = &ops_bit_not},
    [66] = {.kw = "pow", .is_binary = true, .ops = &ops_pow},
    [68] = {.kw = "ceil", .is_binary = false, .ops = &ops_ceil},
    [69] = {.kw = "lambert_wn", .is_binary = true, .ops = &ops_lambert_wn},
    [70] = {.kw = "mod", .is_binary = true, .ops = &ops_mod},
    [71] = {.kw = "arcosech", .is_binary = false, .ops = &ops_acosech},
    [72] = {.kw = "modinv", .is_binary = true, .ops = &ops_modinv},
    [73] = {.kw = "isqrt", .is_binary = false, .ops = &ops_isqrt},
    [74] = {.kw = "arcvercos", .is_binary = false, .ops = &ops_arcvercos},
    [75] = {.kw = "digamma", .is_binary = false, .ops = &ops_digamma},
    [77] = {.kw = "erf", .is_binary = false, .ops = &ops_erf},
    [78] = {.kw = "gamma", .is_binary = false, .ops = &ops_gamma},
    [79] = {.kw = "dilog", .is_binary = false, .ops = &ops_dilog},
    [80] = {.kw = "E1", .is_binary = false, .ops = &ops_E1},
    [81] = {.kw = "erfc", .is_binary = false, .ops = &ops_erfc},
    [82] = {.kw = "cubrt", .is_binary = false, .ops = &ops_cubrt},
    [84] = {.kw = "acos", .is_binary = false, .ops = &ops_acos},
    [85] = {.kw = "atanh", .is_binary = false, .ops = &ops_atanh},
    [88] = {.kw = "chi", .is_binary = true, .ops = &ops_legendre_chi},
    [90] = {.kw = "BesselY", .is_binary = true, .ops = &ops_bessel_y},
    [92] = {.kw = "arsech", .is_binary = false, .ops = &ops_asech},
    [93] = {.kw = "normal_logpdf", .is_binary = false, .ops = &ops_normal_logpdf},
    [94] = {.kw = "ln", .is_binary = false, .ops = &ops_log},
    [96] = {.kw = "havercos", .is_binary = false, .ops = &ops_havercos},
    [97] = {.kw = "gammainc_lower", .is_binary = true, .ops = &ops_gammainc_lower},
    [99] = {.kw = "arcversin", .is_binary = false, .ops = &ops_arcversin},
    [100] = {.kw = "arcsec", .is_binary = false, .ops = &ops_asec},
    [102] = {.kw = "csc", .is_binary = false, .ops = &ops_cosec},
    [103] = {.kw = "beta", .is_binary = true, .ops = &ops_beta},
    [104] = {.kw = "gammainc_P", .is_binary = true, .ops = &ops_gammainc_P},
    [105] = {.kw = "gcd", .is_binary = true, .ops = &ops_gcd},
    [107] = {.kw = "hacovercos", .is_binary = false, .ops = &ops_hacovercos},
    [108] = {.kw = "W_-1", .is_binary = false, .ops = &ops_lambert_wm1},
    [110] = {.kw = "atan", .is_binary = false, .ops = &ops_atan},
    [112] = {.kw = "polylog", .is_binary = true, .ops = &ops_polylog},
    [113] = {.kw = "W_0", .is_binary = false, .ops = &ops_lambert_w0},
    [114] = {.kw = "floor", .is_binary = false, .ops = &ops_floor},
    [115] = {.kw = "exp", .is_binary = false, .ops = &ops_exp},
    [116] = {.kw = "prev_prime", .is_binary = false, .ops = &ops_prev_prime},
    [117] = {.kw = "vercos", .is_binary = false, .ops = &ops_vercos},
    [120] = {.kw = "ζ", .is_binary = false, .ops = &ops_zeta},
    [121] = {.kw = "arccot", .is_binary = false, .ops = &ops_acot},
    [122] = {.kw = "coversin", .is_binary = false, .ops = &ops_coversin},
    [123] = {.kw = "archacovercos", .is_binary = false, .ops = &ops_archacovercos},
    [125] = {.kw = "acosh", .is_binary = false, .ops = &ops_acosh},
    [126] = {.kw = "sec", .is_binary = false, .ops = &ops_sec},
    [127] = {.kw = "log10", .is_binary = false, .ops = &ops_log10},
    [128] = {.kw = "Wₙ", .is_binary = true, .ops = &ops_lambert_wn},
    [130] = {.kw = "trigamma", .is_binary = false, .ops = &ops_trigamma},
    [131] = {.kw = "erfinv", .is_binary = false, .ops = &ops_erfinv},
    [132] = {.kw = "SHR", .is_binary = true, .ops = &ops_shr},
    [133] = {.kw = "factorial", .is_binary = false, .ops = &ops_factorial},
    [134] = {.kw = "cosh", .is_binary = false, .ops = &ops_cosh},
    [135] = {.kw = "lambert_w0", .is_binary = false, .ops = &ops_lambert_w0},
    [136] = {.kw = "arccovercos", .is_binary = false, .ops = &ops_arccovercos},
    [137] = {.kw = "Wn", .is_binary = true, .ops = &ops_lambert_wn},
    [138] = {.kw = "LommelS", .is_ternary = true, .ops = &ops_lommel_s},
    [139] = {.kw = "arcoth", .is_binary = false, .ops = &ops_acoth},
    [140] = {.kw = "cdf", .is_binary = false, .ops = &ops_cdf},
    [141] = {.kw = "W", .is_binary = false, .ops = &ops_lambert_w},
    [142] = {.kw = "fibonacci", .is_binary = false, .ops = &ops_fibonacci},
    [143] = {.kw = "arccosec", .is_binary = false, .ops = &ops_acosec},
    [144] = {.kw = "hypot", .is_binary = true, .ops = &ops_hypot},
    [145] = {.kw = "archacoversin", .is_binary = false, .ops = &ops_archacoversin},
    [146] = {.kw = "gammainv", .is_binary = false, .ops = &ops_gammainv},
    [147] = {.kw = "archaversin", .is_binary = false, .ops = &ops_archaversin},
    [148] = {.kw = "conjugate", .is_binary = false, .ops = &ops_conj},
    [149] = {.kw = "legendre_chi", .is_binary = true, .ops = &ops_legendre_chi},
    [150] = {.kw = "logpdf", .is_binary = false, .ops = &ops_logpdf},
    [151] = {.kw = "XOR", .is_binary = true, .ops = &ops_bit_xor},
    [152] = {.kw = "logbeta", .is_binary = true, .ops = &ops_logbeta},
    [153] = {.kw = "log", .is_binary = false, .ops = &ops_log10},
    [154] = {.kw = "sqrt", .is_binary = false, .ops = &ops_sqrt},
    [155] = {.kw = "covercos", .is_binary = false, .ops = &ops_covercos},
    [156] = {.kw = "bessel_y", .is_binary = true, .ops = &ops_bessel_y},
    [157] = {.kw = "conj", .is_binary = false, .ops = &ops_conj},
    [158] = {.kw = "asech", .is_binary = false, .ops = &ops_asech},
    [159] = {.kw = "partition", .is_binary = false, .ops = &ops_partition},
    [161] = {.kw = "root", .is_binary = true, .ops = &ops_root},
    [162] = {.kw = "productlog", .is_binary = false, .ops = &ops_lambert_w},
    [164] = {.kw = "acosech", .is_binary = false, .ops = &ops_acosech},
    [166] = {.kw = "normal_cdf", .is_binary = false, .ops = &ops_normal_cdf},
};

static unsigned binding_func_hash_values(string_view_t kw, unsigned seed)
{
    size_t len = string_view_length(kw);
    size_t pos = 0u;
    unsigned h = seed ^ (unsigned)len;
    unsigned index = 1u;

    if (string_view_is_empty(kw))
        return 0u;

    while (pos < len) {
        uint32_t value = 0u;
        size_t width = 0u;

        if (!expr_parse_view_peek_value(kw, pos, &value, &width) || width == 0u)
            break;

        h ^= (unsigned)value + 0x9e3779b9u + index * 0x85ebca6bu;
        h *= 16777619u;
        h ^= h >> 13u;
        pos += width;
        ++index;
    }

    return h % BINDING_FUNC_TABLE_SIZE;
}

static unsigned binding_func_bucket_hash(string_view_t kw)
{
    return binding_func_hash_values(kw, 0x811c9dc5u);
}

static unsigned binding_func_slot_hash(string_view_t kw)
{
    return binding_func_hash_values(kw, 0x85ebca6bu);
}

static bool binding_func_entry_matches(const binding_func_entry_t *entry, string_view_t kw)
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
    slot = (binding_func_slot_hash(kw) + s_binding_func_displacements[bucket]) % BINDING_FUNC_TABLE_SIZE;
    entry = &s_binding_funcs[slot];

    if (binding_func_entry_matches(entry, kw))
        return entry;

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

    if (expr_parse_view_peek_value(text, 0u, &value, NULL) && value == 0x221Eu) {
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
    return string_cursor_view_between(0u, string_cursor_end_position(p->cursor), p->cursor);
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

    if (binding_peek_value(p, &cp, &len) && (cp == 0x221A || cp == 0x230A || cp == 0x2308))
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

    return expr_parse_cursor_peek_value_at(p->cursor, binding_pos(p), &cp, NULL) && cp == 0x00B7u;
}

static int binding_const_id_from_name(const char *name, expr_binding_const_id_t *const_id_out)
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

static expr_binding_expr_t *parse_binding_enclosed_expr(binding_parser_t *p, char closing, const char *errmsg)
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

static int parse_binding_two_args(binding_parser_t *p, expr_binding_expr_t **a_out, expr_binding_expr_t **b_out)
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

static int parse_binding_three_args(binding_parser_t *p, expr_binding_expr_t **a_out, expr_binding_expr_t **b_out,
                                    expr_binding_expr_t **c_out)
{
    expr_binding_expr_t *a = NULL;
    expr_binding_expr_t *b = NULL;
    expr_binding_expr_t *c;

    if (!parse_binding_two_args(p, &a, &b))
        return 0;
    if (!parse_binding_required_char(p, ',', "expected ',' in ternary function")) {
        expr_binding_expr_free(a);
        expr_binding_expr_free(b);
        return 0;
    }
    c = parse_binding_addexpr(p);
    if (!c) {
        expr_binding_expr_free(a);
        expr_binding_expr_free(b);
        return 0;
    }

    *a_out = a;
    *b_out = b;
    *c_out = c;
    return 1;
}

static const binding_func_entry_t *parse_binding_function_head(binding_parser_t *p, size_t *paren_pos_out)
{
    string_cursor_t *scan = string_cursor_clone(p->cursor);
    size_t id_start = binding_pos(p);
    size_t id_end = id_start;
    unsigned char b;

    if (!scan) {
        *paren_pos_out = 0u;
        return NULL;
    }

    while (string_cursor_peek_ascii(scan, &b) && binding_ascii_is_function_name_char(b)) {
        string_cursor_skip(scan, 1u);
        id_end = string_cursor_position(scan);
    }

    if (id_end > id_start) {
        string_view_t id = string_cursor_view_between(id_start, id_end, p->cursor);
        const binding_func_entry_t *entry = binding_func_lookup(id);

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
            size_t len;
        } unicode_aliases[] = {{"W₀", sizeof("W₀") - 1u}, {"W₋₁", sizeof("W₋₁") - 1u}};

        for (size_t i = 0u; i < sizeof(unicode_aliases) / sizeof(unicode_aliases[0]); ++i) {
            size_t len = unicode_aliases[i].len;
            string_view_t alias_span;
            const binding_func_entry_t *entry;

            if (!string_cursor_match_at(p->cursor, binding_pos(p), unicode_aliases[i].text))
                continue;

            alias_span = string_cursor_view_between(binding_pos(p), binding_pos(p) + len, p->cursor);

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

static expr_binding_expr_t *parse_binding_atom_mode(binding_parser_t *p, bool allow_ascii_rational_literal)
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
            unsigned char next;

            binding_skip(p, cp_len);
            binding_skip_spaces(p);
            if (binding_peek_ascii(p, &next) && next == '(') {
                binding_skip(p, 1u);
                arg = parse_binding_enclosed_expr(p, ')', "expected ')' after √ argument");
            } else {
                arg = parse_binding_atom_mode(p, false);
            }
            return arg ? expr_binding_expr_new_unary_op(&ops_sqrt, arg) : NULL;
        }

        if (binding_peek_value(p, &cp, &cp_len) && (cp == 0x230A || cp == 0x2308)) {
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
            if (!binding_peek_value(p, &close_cp, &close_len) || close_cp != closing) {
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
        size_t decimal_len = expr_parse_scan_decimal_len(binding_text(p), pos);
        string_view_t literal_view;
        string_t *text;
        number_t value;
        unsigned char slash;

        if (!allow_ascii_rational_literal && decimal_len > 0u && len > decimal_len &&
            expr_parse_view_peek_ascii(binding_text(p), pos + decimal_len, &slash) && slash == '/')
            len = decimal_len;

        literal_view = string_cursor_view_between(pos, pos + len, p->cursor);
        if (len == 0u || !parse_number_view(literal_view, &value)) {
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
            expr_binding_expr_t *expr = expr_binding_expr_new_number_text(string_c_str(text));
            string_free(text);
            return expr;
        }
    }

    {
        size_t paren_pos = 0u;
        const binding_func_entry_t *func = parse_binding_function_head(p, &paren_pos);

        if (func) {
            binding_set_pos(p, paren_pos + 1u);
            if (func->is_ternary) {
                expr_binding_expr_t *a = NULL;
                expr_binding_expr_t *b = NULL;
                expr_binding_expr_t *c = NULL;
                expr_binding_expr_t *parameters;

                if (!parse_binding_three_args(p, &a, &b, &c))
                    return NULL;
                if (!parse_binding_required_char(p, ')', "expected ')' after ternary function")) {
                    expr_binding_expr_free(a);
                    expr_binding_expr_free(b);
                    expr_binding_expr_free(c);
                    return NULL;
                }
                parameters = expr_binding_expr_new_binary_op(&ops_lommel_s_pack, a, b);
                return expr_binding_expr_new_binary_op(func->ops, parameters, c);
            } else if (func->is_binary) {
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
                expr_binding_expr_t *arg = parse_binding_enclosed_expr(p, ')', "expected ')' after function argument");

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

static expr_binding_expr_t *parse_binding_atom(binding_parser_t *p)
{
    return parse_binding_atom_mode(p, true);
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
            exponent = parse_binding_enclosed_expr(p, ')', "expected ')' after exponent");
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

        lhs = (op == '+') ? expr_binding_expr_new_add(lhs, rhs) : expr_binding_expr_new_sub(lhs, rhs);
    }

    return lhs;
}

expr_binding_expr_t *expr_binding_expr_parse_view(string_view_t text, string_t *errmsg)
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

expr_binding_expr_t *expr_binding_expr_parse_array_view(string_view_t text, string_t *errmsg)
{
    string_cursor_t *cursor;
    expr_binding_expr_t **items = NULL;
    size_t count = 0u;
    size_t capacity = 0u;
    unsigned char ch;

    if (errmsg)
        string_clear(errmsg);
    text = string_view_trim(text);
    cursor = string_cursor_new_view(text);
    if (!cursor)
        return NULL;
    string_cursor_skip_spaces(cursor);
    if (!expr_parse_cursor_consume_char(cursor, '['))
        goto syntax_error;
    string_cursor_skip_spaces(cursor);

    if (expr_parse_cursor_consume_char(cursor, ']')) {
        string_cursor_skip_spaces(cursor);
        if (!string_cursor_done(cursor))
            goto syntax_error;
        string_cursor_free(cursor);
        return expr_binding_expr_new_array(NULL, 0u, true);
    }

    if (expr_parse_cursor_consume_char(cursor, '?')) {
        string_cursor_skip_spaces(cursor);
        if (!expr_parse_cursor_consume_char(cursor, ']'))
            goto syntax_error;
        string_cursor_skip_spaces(cursor);
        if (!string_cursor_done(cursor))
            goto syntax_error;
        string_cursor_free(cursor);
        return expr_binding_expr_new_array(NULL, 0u, true);
    }

    for (;;) {
        size_t start = string_cursor_position(cursor);
        size_t end;
        unsigned paren_depth = 0u;
        unsigned bracket_depth = 0u;
        bool absolute_open = false;
        expr_binding_expr_t *item;
        string_view_t item_text;

        while (!string_cursor_done(cursor)) {
            if (!string_cursor_peek_ascii(cursor, &ch)) {
                (void)string_cursor_next(cursor);
                continue;
            }
            if (ch == '(') {
                paren_depth++;
            } else if (ch == ')' && paren_depth > 0u) {
                paren_depth--;
            } else if (ch == '[') {
                bracket_depth++;
            } else if (ch == ']' && bracket_depth > 0u) {
                bracket_depth--;
            } else if (ch == '|') {
                absolute_open = !absolute_open;
            } else if (paren_depth == 0u && bracket_depth == 0u && !absolute_open && (ch == ',' || ch == ']')) {
                break;
            }
            (void)string_cursor_next(cursor);
        }

        end = string_cursor_position(cursor);
        item_text = string_cursor_view_between(start, end, cursor);
        item_text = string_view_trim(item_text);
        if (string_view_is_empty(item_text))
            goto syntax_error;
        item = expr_binding_expr_parse_view(item_text, errmsg);
        if (!item)
            goto fail;
        item = expr_binding_expr_simplify(item);

        if (count == capacity) {
            size_t next_capacity = capacity ? capacity * 2u : 4u;
            expr_binding_expr_t **next = realloc(items, next_capacity * sizeof(*next));

            if (!next)
                abort();
            items = next;
            capacity = next_capacity;
        }
        items[count++] = item;

        if (!string_cursor_peek_ascii(cursor, &ch))
            goto syntax_error;
        (void)string_cursor_next(cursor);
        if (ch == ']')
            break;
        string_cursor_skip_spaces(cursor);
    }

    string_cursor_skip_spaces(cursor);
    if (!string_cursor_done(cursor))
        goto syntax_error;
    string_cursor_free(cursor);
    return expr_binding_expr_new_array(items, count, false);

syntax_error:
    if (errmsg) {
        string_clear(errmsg);
        string_append_cstr(errmsg, "invalid array binding");
    }
fail:
    for (size_t i = 0u; i < count; ++i)
        expr_binding_expr_free(items[i]);
    free(items);
    string_cursor_free(cursor);
    return NULL;
}

bool expr_binding_expr_is_array(const expr_binding_expr_t *expr)
{
    return expr && expr->kind == EXPR_BINDING_EXPR_ARRAY;
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
        bool atomic = !num_is_nan(value) && (num_is_real(value) || num_eq(value, NUM_I));

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
    bool negative;
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

static bool binding_string_is_simple_rational(const string_t *text, binding_simple_rational_t *out)
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

    if (rune_is_equal(string_cursor_peek(cursor), '+') || rune_is_equal(string_cursor_peek(cursor), '-')) {
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

static bool binding_text_is_simple_rational(const char *text, binding_simple_rational_t *out)
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

static void emit_binding_unicode_digits(sbuf_t *b, const string_t *digits, const char *const table[10])
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
    binding_simple_rational_t rational = {false, NULL, NULL};

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
static void emit_binding_TeX_expr(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec);

static bool binding_number_value_unary(const expr_binding_expr_t *expr, binding_number_unary_fn op, number_t *out)
{
    number_t child;

    if (!expr_binding_expr_number_value(expr->u.unary.child, &child))
        return false;
    *out = num_scope_detach(op(child));
    num_destroy(&child);
    return true;
}

static bool binding_number_value_binary(const expr_binding_expr_t *expr, binding_number_binary_fn op, number_t *out)
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

static bool binding_number_value_number(const expr_binding_expr_t *expr, number_t *out)
{
    *out = num_scope_detach(binding_number_from_text(expr->u.text));
    return true;
}

static bool binding_number_value_false(const expr_binding_expr_t *expr, number_t *out)
{
    (void)expr;
    (void)out;
    return false;
}

static bool binding_number_value_neg(const expr_binding_expr_t *expr, number_t *out)
{
    return binding_number_value_unary(expr, num_neg, out);
}

static bool binding_number_value_add(const expr_binding_expr_t *expr, number_t *out)
{
    return binding_number_value_binary(expr, num_add, out);
}

static bool binding_number_value_sub(const expr_binding_expr_t *expr, number_t *out)
{
    return binding_number_value_binary(expr, num_sub, out);
}

static bool binding_number_value_mul(const expr_binding_expr_t *expr, number_t *out)
{
    return binding_number_value_binary(expr, num_mul, out);
}

static bool binding_number_value_div(const expr_binding_expr_t *expr, number_t *out)
{
    return binding_number_value_binary(expr, num_div, out);
}

static bool binding_number_value_powi(const expr_binding_expr_t *expr, number_t *out)
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

static bool binding_expr_leading_number(const expr_binding_expr_t *expr, number_t *coeff_out,
                                        const expr_binding_expr_t **rest_out)
{
    if (expr_binding_expr_number_value(expr, coeff_out)) {
        *rest_out = NULL;
        return true;
    }

    if (expr && expr->kind == EXPR_BINDING_EXPR_MUL && expr_binding_expr_number_value(expr->u.binary.left, coeff_out)) {
        *rest_out = expr->u.binary.right;
        return true;
    }

    return false;
}

bool expr_binding_expr_split_leading_number(const expr_binding_expr_t *expr, number_t *coeff_out,
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

    if (expr->kind == EXPR_BINDING_EXPR_DIV && expr_binding_expr_number_value(expr->u.binary.right, &right_coeff)) {
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
        binding_const_ratio_parts(expr->u.binary.left, expr->u.binary.right, &numer, &denom, &const_id)) {
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

static void emit_binding_expr_mul_separator(const expr_binding_expr_t *left, const expr_binding_expr_t *right,
                                            sbuf_t *b)
{
    if (!(binding_expr_is_atomic(left) && binding_expr_is_atomic(right)))
        sbuf_puts(b, "·");
}

static void emit_binding_TeX_mul_separator(const expr_binding_expr_t *left, const expr_binding_expr_t *right, sbuf_t *b)
{
    (void)left;
    (void)right;
    sbuf_puts(b, "\\mkern-2mu ");
}

static bool binding_mul_should_swap_power_before_radical(const expr_binding_expr_t *left,
                                                         const expr_binding_expr_t *right)
{
    number_t left_exponent;
    number_t right_exponent;
    bool swap;

    if (!left || !right || left->kind != EXPR_BINDING_EXPR_BINARY_OP || right->kind != EXPR_BINDING_EXPR_BINARY_OP ||
        left->u.binary_op.ops != &ops_pow || right->u.binary_op.ops != &ops_pow)
        return false;

    left_exponent = expr_binding_expr_eval(left->u.binary_op.right);
    right_exponent = expr_binding_expr_eval(right->u.binary_op.right);
    swap = num_is_finite(left_exponent) && num_is_nan(right_exponent);
    num_destroy(&right_exponent);
    num_destroy(&left_exponent);
    return swap;
}

static void emit_binding_expr_mul(const expr_binding_expr_t *left, const expr_binding_expr_t *right, sbuf_t *b,
                                  int parent_prec)
{
    bool need = BIND_PREC_MUL < parent_prec;

    if ((left && left->kind == EXPR_BINDING_EXPR_UNARY_OP && left->u.unary_op.ops == &ops_sqrt && right &&
         right->kind == EXPR_BINDING_EXPR_BINARY_OP && right->u.binary_op.ops == &ops_pow) ||
        binding_mul_should_swap_power_before_radical(left, right)) {
        const expr_binding_expr_t *tmp = left;

        left = right;
        right = tmp;
    }

    if (need)
        sbuf_putc(b, '(');
    emit_binding_expr(left, b, BIND_PREC_MUL);
    emit_binding_expr_mul_separator(left, right, b);
    emit_binding_expr(right, b, BIND_PREC_MUL);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_TeX_mul(const expr_binding_expr_t *left, const expr_binding_expr_t *right, sbuf_t *b,
                                 int parent_prec)
{
    bool need = BIND_PREC_MUL < parent_prec;

    if ((left && left->kind == EXPR_BINDING_EXPR_UNARY_OP && left->u.unary_op.ops == &ops_sqrt && right &&
         right->kind == EXPR_BINDING_EXPR_BINARY_OP && right->u.binary_op.ops == &ops_pow) ||
        binding_mul_should_swap_power_before_radical(left, right)) {
        const expr_binding_expr_t *tmp = left;

        left = right;
        right = tmp;
    }

    if (need)
        sbuf_putc(b, '(');
    emit_binding_TeX_expr(left, b, BIND_PREC_MUL);
    emit_binding_TeX_mul_separator(left, right, b);
    emit_binding_TeX_expr(right, b, BIND_PREC_MUL);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_expr_number(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    (void)parent_prec;
    emit_binding_number_text(expr->u.text, b);
}

static void emit_binding_expr_array(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    (void)parent_prec;
    sbuf_putc(b, '[');
    if (expr->u.array.unspecified) {
        sbuf_putc(b, '?');
    } else {
        for (size_t i = 0u; i < expr->u.array.count; ++i) {
            if (i > 0u)
                sbuf_puts(b, ", ");
            emit_binding_expr(expr->u.array.items[i], b, BIND_PREC_LOWEST);
        }
    }
    sbuf_putc(b, ']');
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

static void emit_binding_expr_addsub(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec, const char *op,
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

static void emit_binding_expr_const_ratio_value(expr_binding_const_id_t const_id, long numer, long denom, sbuf_t *b)
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
    if (binding_const_ratio_parts(expr->u.binary.left, expr->u.binary.right, &numer, &denom, &const_id)) {
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
            emit_binding_expr(expr->u.powi.base, b, base_need ? BIND_PREC_LOWEST : BIND_PREC_POW);
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
    emit_binding_expr(expr->u.powi.base, b, base_need ? BIND_PREC_LOWEST : BIND_PREC_POW);
    if (base_need)
        sbuf_putc(b, ')');
    emit_binding_superscript_int(b, expr->u.powi.exponent);
}

static void emit_binding_TeX_number(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    string_t *clean;
    char *tex;
    binding_simple_rational_t rational = {false, NULL, NULL};

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

static void emit_binding_TeX_array(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    (void)parent_prec;
    sbuf_puts(b, "\\left[");
    if (expr->u.array.unspecified) {
        sbuf_putc(b, '?');
    } else {
        for (size_t i = 0u; i < expr->u.array.count; ++i) {
            if (i > 0u)
                sbuf_puts(b, ", ");
            emit_binding_TeX_expr(expr->u.array.items[i], b, BIND_PREC_LOWEST);
        }
    }
    sbuf_puts(b, "\\right]");
}

static void emit_binding_TeX_const(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    (void)parent_prec;
    sbuf_puts(b, binding_const_TeX_name(expr->u.const_id));
}

static void emit_binding_TeX_neg(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool need = BIND_PREC_UNARY < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    sbuf_putc(b, '-');
    emit_binding_TeX_expr(expr->u.unary.child, b, BIND_PREC_UNARY);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_TeX_addsub(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec, const char *op,
                                    int right_prec)
{
    bool need = BIND_PREC_ADD < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    emit_binding_TeX_expr(expr->u.binary.left, b, BIND_PREC_ADD);
    sbuf_puts(b, op);
    emit_binding_TeX_expr(expr->u.binary.right, b, right_prec);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_binding_TeX_add(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_TeX_addsub(expr, b, parent_prec, " + ", BIND_PREC_ADD);
}

static void emit_binding_TeX_sub(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_TeX_addsub(expr, b, parent_prec, " - ", BIND_PREC_ADD + 1);
}

static void emit_binding_TeX_mul_node(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_TeX_mul(expr->u.binary.left, expr->u.binary.right, b, parent_prec);
}

static void emit_binding_TeX_const_ratio_numer(expr_binding_const_id_t const_id, long numer, sbuf_t *b)
{
    long abs_numer = numer < 0L ? -numer : numer;

    if (numer < 0L)
        sbuf_putc(b, '-');
    if (abs_numer != 1L) {
        char nbuf[32];

        snprintf(nbuf, sizeof(nbuf), "%ld", abs_numer);
        sbuf_puts(b, nbuf);
    }
    sbuf_puts(b, binding_const_TeX_name(const_id));
}

static void emit_binding_TeX_const_ratio_value(expr_binding_const_id_t const_id, long numer, long denom, sbuf_t *b)
{
    if (denom == 1L) {
        emit_binding_TeX_const_ratio_numer(const_id, numer, b);
        return;
    }
    {
        char dbuf[32];

        snprintf(dbuf, sizeof(dbuf), "%ld", denom);
        sbuf_puts(b, "\\frac{");
        emit_binding_TeX_const_ratio_numer(const_id, numer, b);
        sbuf_puts(b, "}{");
        sbuf_puts(b, dbuf);
        sbuf_putc(b, '}');
    }
}

static void emit_binding_TeX_div(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    long numer;
    long denom;
    expr_binding_const_id_t const_id;

    (void)parent_prec;
    if (binding_const_ratio_parts(expr->u.binary.left, expr->u.binary.right, &numer, &denom, &const_id)) {
        emit_binding_TeX_const_ratio_value(const_id, numer, denom, b);
        return;
    }
    sbuf_puts(b, "\\frac{");
    emit_binding_TeX_expr(expr->u.binary.left, b, BIND_PREC_LOWEST);
    sbuf_puts(b, "}{");
    emit_binding_TeX_expr(expr->u.binary.right, b, BIND_PREC_LOWEST);
    sbuf_putc(b, '}');
}

static void emit_binding_TeX_powi(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    bool base_need = binding_expr_needs_pow_base_parens(expr->u.powi.base);
    char expbuf[64];

    (void)parent_prec;
    if (expr->u.powi.exponent < 0) {
        long positive_exponent = -expr->u.powi.exponent;

        sbuf_puts(b, "\\frac{1}{");
        if (positive_exponent == 1L) {
            emit_binding_TeX_expr(expr->u.powi.base, b, BIND_PREC_LOWEST);
        } else {
            if (base_need)
                sbuf_puts(b, "\\left(");
            emit_binding_TeX_expr(expr->u.powi.base, b, base_need ? BIND_PREC_LOWEST : BIND_PREC_POW);
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
    emit_binding_TeX_expr(expr->u.powi.base, b, base_need ? BIND_PREC_LOWEST : BIND_PREC_POW);
    if (base_need)
        sbuf_puts(b, "\\right)");
    snprintf(expbuf, sizeof(expbuf), "%ld", expr->u.powi.exponent);
    sbuf_puts(b, "^{");
    sbuf_puts(b, expbuf);
    sbuf_putc(b, '}');
}

static void emit_binding_expr_unary_call(const expr_ops_t *ops, const expr_binding_expr_t *child, sbuf_t *b)
{
    if (ops == &ops_gamma)
        sbuf_puts(b, "Γ");
    else if (ops == &ops_digamma)
        sbuf_puts(b, "ψ⁽⁰⁾");
    else if (ops == &ops_trigamma)
        sbuf_puts(b, "ψ⁽¹⁾");
    else if (ops == &ops_zeta)
        sbuf_puts(b, "ζ");
    else
        sbuf_puts(b, (ops && ops->name) ? ops->name : "?");
    sbuf_putc(b, '(');
    emit_binding_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_binding_TeX_unary_call(const expr_ops_t *ops, const expr_binding_expr_t *child, sbuf_t *b)
{
    const char *name = (ops && ops->TeX_name) ? ops->TeX_name : NULL;

    if (!name) {
        sbuf_puts(b, "\\operatorname{");
        sbuf_puts(b, (ops && ops->name) ? ops->name : "?");
        sbuf_putc(b, '}');
    } else {
        sbuf_puts(b, name);
    }
    sbuf_putc(b, '(');
    emit_binding_TeX_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_binding_expr_unary_sqrt(const expr_ops_t *ops, const expr_binding_expr_t *child, sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "√(");
    emit_binding_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_binding_TeX_unary_sqrt(const expr_ops_t *ops, const expr_binding_expr_t *child, sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "\\sqrt{");
    emit_binding_TeX_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, '}');
}

static void emit_binding_expr_unary_cubrt(const expr_ops_t *ops, const expr_binding_expr_t *child, sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "cubrt(");
    emit_binding_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_binding_TeX_unary_cubrt(const expr_ops_t *ops, const expr_binding_expr_t *child, sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "\\sqrt[3]{");
    emit_binding_TeX_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, '}');
}

static void emit_binding_expr_unary_abs(const expr_ops_t *ops, const expr_binding_expr_t *child, sbuf_t *b)
{
    (void)ops;
    sbuf_putc(b, '|');
    emit_binding_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, '|');
}

static void emit_binding_TeX_unary_abs(const expr_ops_t *ops, const expr_binding_expr_t *child, sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "\\left|");
    emit_binding_TeX_expr(child, b, BIND_PREC_LOWEST);
    sbuf_puts(b, "\\right|");
}

static void emit_binding_expr_unary_floor(const expr_ops_t *ops, const expr_binding_expr_t *child, sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "⌊");
    emit_binding_expr(child, b, BIND_PREC_LOWEST);
    sbuf_puts(b, "⌋");
}

static void emit_binding_TeX_unary_floor(const expr_ops_t *ops, const expr_binding_expr_t *child, sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "\\left\\lfloor ");
    emit_binding_TeX_expr(child, b, BIND_PREC_LOWEST);
    sbuf_puts(b, " \\right\\rfloor");
}

static void emit_binding_expr_unary_ceil(const expr_ops_t *ops, const expr_binding_expr_t *child, sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "⌈");
    emit_binding_expr(child, b, BIND_PREC_LOWEST);
    sbuf_puts(b, "⌉");
}

static void emit_binding_TeX_unary_ceil(const expr_ops_t *ops, const expr_binding_expr_t *child, sbuf_t *b)
{
    (void)ops;
    sbuf_puts(b, "\\left\\lceil ");
    emit_binding_TeX_expr(child, b, BIND_PREC_LOWEST);
    sbuf_puts(b, " \\right\\rceil");
}

static bool emit_binding_TeX_exp_unit_fraction_root(const expr_binding_expr_t *child, sbuf_t *b)
{
    number_t value;
    long numerator;
    long denominator;

    if (!expr_binding_expr_number_value(child, &value))
        return false;

    if (!num_get_small_rational(value, &numerator, &denominator) || numerator != 1L || denominator <= 1L) {
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

static void emit_binding_TeX_unary_exp(const expr_ops_t *ops, const expr_binding_expr_t *child, sbuf_t *b)
{
    (void)ops;
    if (emit_binding_TeX_exp_unit_fraction_root(child, b))
        return;

    sbuf_puts(b, "e^{");
    emit_binding_TeX_expr(child, b, BIND_PREC_LOWEST);
    sbuf_putc(b, '}');
}

static void emit_binding_expr_unary_neg_op(const expr_ops_t *ops, const expr_binding_expr_t *child, sbuf_t *b)
{
    (void)ops;
    sbuf_putc(b, '-');
    emit_binding_expr(child, b, BIND_PREC_UNARY);
}

static void emit_binding_TeX_unary_neg_op(const expr_ops_t *ops, const expr_binding_expr_t *child, sbuf_t *b)
{
    (void)ops;
    sbuf_putc(b, '-');
    emit_binding_TeX_expr(child, b, BIND_PREC_UNARY);
}

typedef void (*binding_unary_emit_fn)(const expr_ops_t *ops, const expr_binding_expr_t *child, sbuf_t *b);

typedef struct {
    binding_unary_emit_fn emit_expr;
    binding_unary_emit_fn emit_TeX;
} binding_unary_render_t;

static const binding_unary_render_t s_binding_unary_renderers[EXPR_KIND_COUNT] = {
    [EXPR_KIND_NEG] = {emit_binding_expr_unary_neg_op, emit_binding_TeX_unary_neg_op},
    [EXPR_KIND_SQRT] = {emit_binding_expr_unary_sqrt, emit_binding_TeX_unary_sqrt},
    [EXPR_KIND_CUBRT] = {emit_binding_expr_unary_cubrt, emit_binding_TeX_unary_cubrt},
    [EXPR_KIND_ABS] = {emit_binding_expr_unary_abs, emit_binding_TeX_unary_abs},
    [EXPR_KIND_FLOOR] = {emit_binding_expr_unary_floor, emit_binding_TeX_unary_floor},
    [EXPR_KIND_CEIL] = {emit_binding_expr_unary_ceil, emit_binding_TeX_unary_ceil},
    [EXPR_KIND_EXP] = {emit_binding_expr_unary_call, emit_binding_TeX_unary_exp}};

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

static void emit_binding_TeX_unary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    const binding_unary_render_t *renderer = binding_unary_renderer_for_ops(expr->u.unary_op.ops);
    bool need = BIND_PREC_UNARY < parent_prec;

    if (need)
        sbuf_putc(b, '(');
    if (renderer && renderer->emit_TeX)
        renderer->emit_TeX(expr->u.unary_op.ops, expr->u.unary_op.child, b);
    else
        emit_binding_TeX_unary_call(expr->u.unary_op.ops, expr->u.unary_op.child, b);
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

    if (ops == &ops_polygamma) {
        long order;

        if (binding_number_text_to_long(expr->u.binary_op.left, &order) && order >= 0L) {
            sbuf_puts(b, "ψ⁽");
            emit_binding_superscript_int(b, order);
            sbuf_puts(b, "⁾(");
            emit_binding_expr(expr->u.binary_op.right, b, BIND_PREC_LOWEST);
            sbuf_putc(b, ')');
            return;
        }
    }

    if (ops == &ops_zetah)
        sbuf_puts(b, "ζ");
    else
        sbuf_puts(b, (ops && ops->name) ? ops->name : "?");
    sbuf_putc(b, '(');
    emit_binding_expr(expr->u.binary_op.left, b, BIND_PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_binding_expr(expr->u.binary_op.right, b, BIND_PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_binding_func_number(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    emit_binding_expr_number(expr, b, parent_prec);
}

static void emit_binding_func_array(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    (void)parent_prec;
    sbuf_putc(b, '[');
    if (expr->u.array.unspecified) {
        sbuf_putc(b, '?');
    } else {
        for (size_t i = 0u; i < expr->u.array.count; ++i) {
            if (i > 0u)
                sbuf_puts(b, ", ");
            emit_binding_func_expr(expr->u.array.items[i], b, BIND_PREC_LOWEST);
        }
    }
    sbuf_putc(b, ']');
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

static void emit_binding_func_addsub(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec, const char *op,
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
    sbuf_putc(b, '/');
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
        sbuf_puts(b, "1/");
        if (positive_exponent == 1L) {
            emit_binding_func_expr(expr->u.powi.base, b, BIND_PREC_POW);
        } else {
            if (base_need)
                sbuf_putc(b, '(');
            emit_binding_func_expr(expr->u.powi.base, b, base_need ? BIND_PREC_LOWEST : BIND_PREC_POW);
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
        sbuf_putc(b, '^');
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

static void emit_binding_TeX_binary_op(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    const expr_ops_t *ops = expr->u.binary_op.ops;

    (void)parent_prec;
    if (ops == &ops_pow) {
        bool base_need = binding_expr_needs_pow_base_parens(expr->u.binary_op.left);

        if (base_need)
            sbuf_puts(b, "\\left(");
        emit_binding_TeX_expr(expr->u.binary_op.left, b, base_need ? BIND_PREC_LOWEST : BIND_PREC_POW);
        if (base_need)
            sbuf_puts(b, "\\right)");
        sbuf_puts(b, "^{");
        emit_binding_TeX_expr(expr->u.binary_op.right, b, BIND_PREC_LOWEST);
        sbuf_putc(b, '}');
        return;
    }

    if (ops == &ops_root) {
        sbuf_puts(b, "\\sqrt[");
        emit_binding_TeX_expr(expr->u.binary_op.right, b, BIND_PREC_LOWEST);
        sbuf_puts(b, "]{");
        emit_binding_TeX_expr(expr->u.binary_op.left, b, BIND_PREC_LOWEST);
        sbuf_putc(b, '}');
        return;
    }

    if (ops == &ops_polygamma) {
        sbuf_puts(b, "\\psi^{");
        emit_binding_TeX_expr(expr->u.binary_op.left, b, BIND_PREC_LOWEST);
        sbuf_puts(b, "}(");
        emit_binding_TeX_expr(expr->u.binary_op.right, b, BIND_PREC_LOWEST);
        sbuf_putc(b, ')');
        return;
    }

    if (ops && ops->TeX_name)
        sbuf_puts(b, ops->TeX_name);
    else {
        sbuf_puts(b, "\\operatorname{");
        sbuf_puts(b, (ops && ops->name) ? ops->name : "?");
        sbuf_putc(b, '}');
    }
    sbuf_putc(b, '(');
    emit_binding_TeX_expr(expr->u.binary_op.left, b, BIND_PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_binding_TeX_expr(expr->u.binary_op.right, b, BIND_PREC_LOWEST);
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

static void emit_binding_TeX_expr(const expr_binding_expr_t *expr, sbuf_t *b, int parent_prec)
{
    const binding_expr_ops_t *ops;

    if (!expr) {
        sbuf_puts(b, "0");
        return;
    }

    ops = binding_expr_ops_for_kind(expr->kind);
    if (!ops || !ops->emit_TeX) {
        sbuf_puts(b, "?");
        return;
    }

    ops->emit_TeX(expr, b, parent_prec);
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

char *expr_binding_expr_to_TeX(const expr_binding_expr_t *expr)
{
    sbuf_t b;

    sbuf_init(&b);
    emit_binding_TeX_expr(expr, &b, BIND_PREC_LOWEST);
    {
        char *out = sbuf_to_c_string(&b);
        sbuf_free(&b);
        return out;
    }
}

void expr_set_binding_pi_linear_family(expr_t *expr, long denominator, long n_coeff, long offset)
{
    char n_term[64];
    char offset_text[64];
    char denominator_text[64];
    expr_binding_expr_t *binding_expr;

    if (!expr || denominator == 0L)
        return;

    snprintf(n_term, sizeof(n_term), "%ldn", n_coeff);
    snprintf(offset_text, sizeof(offset_text), "%ld", offset);
    snprintf(denominator_text, sizeof(denominator_text), "%ld", denominator);

    binding_expr = expr_binding_expr_new_mul(
        expr_binding_expr_new_div(expr_binding_expr_new_number_text("1"),
                                  expr_binding_expr_new_number_text(denominator_text)),
        expr_binding_expr_new_mul(expr_binding_expr_new_const(EXPR_BINDING_CONST_PI),
                                  expr_binding_expr_new_add(expr_binding_expr_new_number_text(n_term),
                                                            expr_binding_expr_new_number_text(offset_text))));
    if (!binding_expr)
        return;

    expr_binding_expr_free(expr->binding_expr);
    expr->binding_expr = binding_expr;
}
