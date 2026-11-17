/**
 * @file expr_appell.c
 * @brief Native expression construction, evaluation and calculus for all four Appell functions.
 *
 * All four functions use shared argument lists, keeping parameters separate
 * from the two variables and preserving ordinary expression graph traversal and
 * substitution. Argument derivatives use exact contiguous relations and the chain rule. Parameter derivatives
 * remain formal; reverse-mode argument derivatives use the same symbolic partials. Affine antiderivatives are
 * constructed where the inverse contiguous relation has non-zero numerical factors and valid lower parameters.
 * Finite summation uses the ordinary expression evaluator, including its index-dependent bindings and caches.
 */
#include <stdlib.h>

#include "expression-t.h"
#include "number-t.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

/* Entries follow F1--F4; family numbers become zero-based indices at each lookup. */
static const unsigned char appell_shifts[4][2][3] = {
    [0] = {{0, 1, 3}, {0, 2, 3}},
    [1] = {{0, 1, 3}, {0, 2, 4}},
    [2] = {{0, 2, 4}, {1, 3, 4}},
    [3] = {{0, 1, 2}, {0, 1, 3}}
};

/* The alphabetically ordered Appell kinds map directly to their mathematical family numbers. */
unsigned expr_appell_family(const expr_t *expression)
{
    _Static_assert(EXPR_KIND_APPELL_F2 == EXPR_KIND_APPELL_F1 + 1 &&
                   EXPR_KIND_APPELL_F3 == EXPR_KIND_APPELL_F1 + 2 &&
                   EXPR_KIND_APPELL_F4 == EXPR_KIND_APPELL_F1 + 3, "Appell kinds must remain consecutive.");
    if (!expression || !expression->ops) return 0;
    unsigned offset = (unsigned)expression->ops->kind - (unsigned)EXPR_KIND_APPELL_F1;
    return offset < 4u ? offset + 1u : 0u;
}

bool expr_appell_unpack(const expr_t *expression, const expr_t **arguments, size_t *count)
{
    unsigned family = expr_appell_family(expression);
    if (!family || !arguments || !count || !expression->a || !expression->b ||
        expression->b->ops != &ops_argument_list) return false;
    *count = family == 1 || family == 4 ? 6u : 7u;
    const expr_t *parameters = expression->a;
    for (size_t j = *count - 2u; j > 1u; --j) {
        if (!parameters || parameters->ops != &ops_argument_list || !parameters->a || !parameters->b) return false;
        arguments[j - 1u] = parameters->b;
        parameters = parameters->a;
    }
    arguments[0] = parameters;
    arguments[*count - 2u] = expression->b->a;
    arguments[*count - 1u] = expression->b->b;
    return parameters && expression->b->a && expression->b->b;
}

static expr_t *appell_wrap(const expr_ops_t *operations, const expr_t *left, const expr_t *right)
{
    if (!left || !right) return NULL;
    expr_retain(left); expr_retain(right);
    return expr_new_binary_internal(operations, left, right);
}

static expr_t *appell_pack(const expr_t *left, const expr_t *right)
{
    return appell_wrap(&ops_argument_list, left, right);
}

static expr_t *appell_rebuild(unsigned family, const expr_t *parameters, const expr_t *variables)
{
    static const expr_ops_t *const operations[] = {
        [0] = &ops_appell_F1,
        [1] = &ops_appell_F2,
        [2] = &ops_appell_F3,
        [3] = &ops_appell_F4
    };
    if (family < 1u || family > 4u) return NULL;
    return appell_wrap(operations[family - 1u], parameters, variables);
}

static expr_t *appell_build(unsigned family, const expr_t *const *arguments)
{
    size_t count = family == 1 || family == 4 ? 6u : 7u;
    for (size_t j = 0; j < count; ++j) if (!arguments[j]) return NULL;
    expr_t *parameters = expr_clone(arguments[0]);
    for (size_t j = 1; parameters && j < count - 2u; ++j) {
        expr_t *next = appell_pack(parameters, arguments[j]);
        expr_free(parameters); parameters = next;
    }
    expr_t *variables = appell_pack(arguments[count - 2u], arguments[count - 1u]);
    expr_t *result = appell_rebuild(family, parameters, variables);
    expr_free(variables); expr_free(parameters);
    return result;
}

static number_t appell_eval(expr_t *expression)
{
    NUM_SCOPE(scope);
    const expr_t *arguments[7];
    size_t count = 0;
    if (!expr_appell_unpack(expression, arguments, &count)) return num_clone(NUM_NAN);
    number_t a[7];
    for (size_t j = 0; j < count; ++j) a[j] = expr_eval_num_internal(arguments[j]);
    unsigned family = expr_appell_family(expression);
    number_t result = family == 2 ? num_appell_F2(a[0], a[1], a[2], a[3], a[4], a[5], a[6]) :
        family == 3 ? num_appell_F3(a[0], a[1], a[2], a[3], a[4], a[5], a[6]) :
                      num_appell_F4(a[0], a[1], a[2], a[3], a[4], a[5]);
    return num_scope_detach(result);
}

static expr_t *appell_partial(const expr_t *expression, unsigned axis, int step)
{
    const expr_t *arguments[7], *shifted[7];
    size_t count = 0;
    unsigned family = expr_appell_family(expression);
    if (!family || axis > 1 || !expr_appell_unpack(expression, arguments, &count)) return NULL;
    const unsigned char *indices = appell_shifts[family - 1u][axis];
    expr_t *changed[3];
    for (size_t j = 0; j < count; ++j) shifted[j] = arguments[j];
    for (unsigned j = 0; j < 3; ++j) {
        changed[j] = expr_add_long(arguments[indices[j]], step);
        shifted[indices[j]] = changed[j];
    }
    expr_t *function = appell_build(family, shifted);
    expr_t *numerator = expr_mul(step > 0 ? arguments[indices[0]] : changed[0],
                                 step > 0 ? arguments[indices[1]] : changed[1]);
    expr_t *factor = step > 0 ? expr_div(numerator, arguments[indices[2]]) : expr_div(changed[2], numerator);
    expr_t *result = function && factor ? expr_mul(factor, function) : NULL;
    expr_free(factor); expr_free(numerator); expr_free(function);
    for (unsigned j = 0; j < 3; ++j) expr_free(changed[j]);
    return result;
}

static bool appell_zero(const expr_t *expression)
{
    return expression && expr_is_const(expression) && num_is_zero(expression->c);
}

static bool appell_depends(const expr_t *expression, const expr_t *wrt)
{
    expr_t *variable = (expr_t *)wrt;
    bool used = false;
    return !expr_collect_var_usage(expression, 1, &variable, &used) || used;
}

static expr_t *appell_derivative(expr_t *expression)
{
    const expr_t *arguments[7];
    size_t count = 0;
    if (!expr_appell_unpack(expression, arguments, &count)) return expr_new_const(NUM_NAN);
    expr_t *wrt = (expr_t *)expr_current_wrt_internal();
    if (!wrt) return expr_new_const(NUM_NAN);
    for (size_t j = 0; j < count - 2u; ++j) {
        if (appell_depends(arguments[j], wrt)) return expr_new_formal_derivative(expression, 1, &wrt);
    }
    expr_t *sum = expr_const_zero();
    for (unsigned axis = 0; axis < 2; ++axis) {
        if (!appell_depends(arguments[count - 2u + axis], wrt)) continue;
        expr_t *argument_derivative = expr_get_dx_internal(arguments[count - 2u + axis]);
        if (!appell_zero(argument_derivative)) {
            expr_t *partial = appell_partial(expression, axis, 1);
            expr_t *term = partial && argument_derivative ? expr_mul(partial, argument_derivative) : NULL;
            expr_t *next = term ? expr_add(sum, term) : NULL;
            expr_free(sum); sum = next;
            expr_free(term); expr_free(partial);
        }
        expr_free(argument_derivative);
        if (!sum) break;
    }
    return sum;
}

static int appell_reverse(const expr_t *expression, const number_t *out_bar,
                           expr_reverse_accumulate_fn accumulate, void *context)
{
    NUM_SCOPE(scope);
    const expr_t *arguments[7];
    size_t count = 0;
    if (!expr_appell_unpack(expression, arguments, &count)) return -1;
    for (size_t j = 0; j < count - 2u; ++j) {
        number_t unspecified = NUM_NAN;
        if (accumulate(context, arguments[j], &unspecified) != 0) return -1;
    }
    for (unsigned axis = 0; axis < 2; ++axis) {
        expr_t *partial = appell_partial(expression, axis, 1);
        if (!partial) return -1;
        number_t value = expr_eval_num_internal(partial), contribution = num_mul(value, *out_bar);
        expr_free(partial);
        if (accumulate(context, arguments[count - 2u + axis], &contribution) != 0) return -1;
    }
    return 0;
}

expr_t *expr_integrate_appell(const expr_t *expression, const expr_t *wrt)
{
    NUM_SCOPE(scope);
    const expr_t *arguments[7];
    size_t count = 0;
    unsigned family = expr_appell_family(expression);
    if (!family || !wrt || !expr_appell_unpack(expression, arguments, &count)) return NULL;
    for (size_t j = 0; j < count - 2u; ++j) if (appell_depends(arguments[j], wrt)) return NULL;
    for (unsigned axis = 0; axis < 2; ++axis) {
        if (appell_depends(arguments[count - 1u - axis], wrt)) continue;
        expr_t *slope = expr_create_deriv(arguments[count - 2u + axis], wrt);
        if (!slope || appell_depends(slope, wrt)) { expr_free(slope); continue; }
        number_t value = NUM_NAN;
        bool valid = expr_match_const_value(slope, &value) && num_is_finite(value) && !num_is_zero(value);
        for (unsigned j = 0; valid && j < 3; ++j) {
            valid = expr_match_const_value(arguments[appell_shifts[family - 1u][axis][j]], &value);
            value = valid ? num_sub(value, NUM_ONE) : NUM_NAN;
            if (!num_is_finite(value) || num_is_zero(value) ||
                (j == 2 && num_is_real(value) && num_is_integer(value) && num_le(value, NUM_ZERO))) valid = false;
        }
        expr_t *primitive = valid ? appell_partial(expression, axis, -1) : NULL;
        expr_t *result = primitive ? expr_div(primitive, slope) : NULL;
        expr_free(primitive); expr_free(slope);
        if (result) return result;
    }
    return NULL;
}

static expr_t *appell_F2_rebuild(const expr_t *parameters, const expr_t *variables)
{
    return appell_rebuild(2, parameters, variables);
}

static expr_t *appell_F3_rebuild(const expr_t *parameters, const expr_t *variables)
{
    return appell_rebuild(3, parameters, variables);
}

static expr_t *appell_F4_rebuild(const expr_t *parameters, const expr_t *variables)
{
    return appell_rebuild(4, parameters, variables);
}

bool expr_appell_F1_unpack(const expr_t *expr, const expr_t **a, const expr_t **b1, const expr_t **b2, const expr_t **c,
                           const expr_t **x, const expr_t **y)
{
    const expr_t *arguments[7] = {0};
    size_t count = 0;
    bool valid = expr_appell_family(expr) == 1 && expr_appell_unpack(expr, arguments, &count);
    const expr_t **outputs[] = {a, b1, b2, c, x, y};
    for (size_t j = 0; j < 6; ++j) {
        if (outputs[j]) *outputs[j] = valid ? arguments[j] : NULL;
    }
    return valid;
}

static number_t eval_appell_F1(expr_t *expr)
{
    const expr_t *a = NULL;
    const expr_t *b1 = NULL;
    const expr_t *b2 = NULL;
    const expr_t *c = NULL;
    const expr_t *x = NULL;
    const expr_t *y = NULL;

    if (!expr_appell_F1_unpack(expr, &a, &b1, &b2, &c, &x, &y))
        return NUM_NAN;
    return num_appell_F1(expr_eval_num_internal(a), expr_eval_num_internal(b1), expr_eval_num_internal(b2),
                         expr_eval_num_internal(c), expr_eval_num_internal(x), expr_eval_num_internal(y));
}

static expr_t *deriv_appell_F1(expr_t *expr)
{
    const expr_t *a = NULL;
    const expr_t *b1 = NULL;
    const expr_t *b2 = NULL;
    const expr_t *c = NULL;
    const expr_t *x = NULL;
    const expr_t *y = NULL;
    const expr_t *parameters[2];
    const expr_t *variables[2];
    expr_t *lauricella = NULL;
    expr_t *out = NULL;

    if (!expr_appell_F1_unpack(expr, &a, &b1, &b2, &c, &x, &y))
        return expr_new_const(NUM_NAN);
    parameters[0] = b1;
    parameters[1] = b2;
    variables[0] = x;
    variables[1] = y;
    lauricella = expr_lauricella_F(a, 2u, parameters, c, variables);
    out = lauricella ? expr_get_dx_internal(lauricella) : NULL;
    expr_free(lauricella);
    return out ? out : expr_new_const(NUM_NAN);
}

static expr_t *expr_appell_F1_from_packs(const expr_t *params, const expr_t *vars)
{
    return appell_rebuild(1, params, vars);
}

/* Construct Appell F1 using the shared argument-list representation and retained arguments. */
expr_t *expr_appell_F1(const expr_t *a, const expr_t *b1, const expr_t *b2, const expr_t *c, const expr_t *x,
                       const expr_t *y)
{
    const expr_t *arguments[] = {a, b1, b2, c, x, y};
    return appell_build(1, arguments);
}

const expr_ops_t ops_appell_F1 = {.eval = eval_appell_F1,
                                  .deriv = deriv_appell_F1,
                                  .reverse = expr_reverse_not_differentiable,
                                  .reverse_many = expr_reverse_appell_F1_many,
                                  .kind = EXPR_KIND_APPELL_F1,
                                  .arity = EXPR_OP_BINARY,
                                  .expression_name = "F₁",
                                  .function_name = "appellf1",
                                  .apply_unary = NULL,
                                  .apply_binary = expr_appell_F1_from_packs,
                                  .integrate = expr_integrate_dispatch_primitive,
                                  .simplify = expr_simplify_rebuild_binary_operator,
                                  .fold_const_unary = NULL};

/* Preserve the existing reverse-mode entry point within the owning Appell implementation. */
int expr_reverse_appell_F1_many(const expr_t *expression, const number_t *out_bar,
                                expr_reverse_accumulate_fn accumulate, void *context)
{
    return appell_reverse(expression, out_bar, accumulate, context);
}

const expr_ops_t ops_appell_F2 = {
    .eval = appell_eval, .deriv = appell_derivative, .reverse = expr_reverse_not_differentiable,
    .reverse_many = appell_reverse, .kind = EXPR_KIND_APPELL_F2, .arity = EXPR_OP_BINARY,
    .expression_name = "F₂", .function_name = "appellf2", .apply_binary = appell_F2_rebuild,
    .integrate = expr_integrate_dispatch_primitive,
    .simplify = expr_simplify_rebuild_binary_operator
};

const expr_ops_t ops_appell_F3 = {
    .eval = appell_eval, .deriv = appell_derivative, .reverse = expr_reverse_not_differentiable,
    .reverse_many = appell_reverse, .kind = EXPR_KIND_APPELL_F3, .arity = EXPR_OP_BINARY,
    .expression_name = "F₃", .function_name = "appellf3", .apply_binary = appell_F3_rebuild,
    .integrate = expr_integrate_dispatch_primitive,
    .simplify = expr_simplify_rebuild_binary_operator
};

const expr_ops_t ops_appell_F4 = {
    .eval = appell_eval, .deriv = appell_derivative, .reverse = expr_reverse_not_differentiable,
    .reverse_many = appell_reverse, .kind = EXPR_KIND_APPELL_F4, .arity = EXPR_OP_BINARY,
    .expression_name = "F₄", .function_name = "appellf4", .apply_binary = appell_F4_rebuild,
    .integrate = expr_integrate_dispatch_primitive,
    .simplify = expr_simplify_rebuild_binary_operator
};

/* Construct the second Appell function, retaining each borrowed argument. */
expr_t *expr_appell_F2(const expr_t *a, const expr_t *b1, const expr_t *b2, const expr_t *c1, const expr_t *c2,
                       const expr_t *x, const expr_t *y)
{
    const expr_t *arguments[] = {a, b1, b2, c1, c2, x, y};
    return appell_build(2, arguments);
}

/* Construct the third Appell function with its two separate upper parameter pairs. */
expr_t *expr_appell_F3(const expr_t *a1, const expr_t *a2, const expr_t *b1, const expr_t *b2, const expr_t *c,
                       const expr_t *x, const expr_t *y)
{
    const expr_t *arguments[] = {a1, a2, b1, b2, c, x, y};
    return appell_build(3, arguments);
}

/* Construct the fourth Appell function, retaining its two variables as distinct arguments. */
expr_t *expr_appell_F4(const expr_t *a, const expr_t *b, const expr_t *c1, const expr_t *c2,
                       const expr_t *x, const expr_t *y)
{
    const expr_t *arguments[] = {a, b, c1, c2, x, y};
    return appell_build(4, arguments);
}

expr_t *expr_appell_F2_args(size_t count, expr_t *const *a)
{
    return count == 7 ? expr_appell_F2(a[0], a[1], a[2], a[3], a[4], a[5], a[6]) : NULL;
}

expr_t *expr_appell_F3_args(size_t count, expr_t *const *a)
{
    return count == 7 ? expr_appell_F3(a[0], a[1], a[2], a[3], a[4], a[5], a[6]) : NULL;
}

expr_t *expr_appell_F4_args(size_t count, expr_t *const *a)
{
    return count == 6 ? expr_appell_F4(a[0], a[1], a[2], a[3], a[4], a[5]) : NULL;
}
