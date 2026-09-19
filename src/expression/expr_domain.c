#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool domain_numeric(const expr_t *expr);

static bool domain_same_value(const expr_t *left, const expr_t *right)
{
    // Parsed conditions and inferred transform targets can have separate IDs for the same free name.
    if (expr_is_var(left) && expr_is_var(right) && left->name && right->name &&
        strcmp(left->name, right->name) == 0)
        return true;
    return expr_simplify_same_factor(left, right);
}

static number_t real_bound_eval(expr_t *expr)
{
    number_t value = expr_eval(expr->a);
    number_t result = num_is_finite(value) ? num_real_part(value) : num_clone(NUM_NAN);
    num_destroy(&value);
    return result;
}

static expr_t *real_bound_new(const expr_t *value)
{
    if (domain_numeric(value)) {
        number_t number = expr_eval(value);
        number_t real = num_is_finite(number) ? num_real_part(number) : num_clone(NUM_NAN);
        expr_t *result = expr_new_const(real);
        num_destroy(&real);
        num_destroy(&number);
        return result;
    }
    return expr_new_unary_internal(&ops_real_bound, expr_clone(value));
}

static expr_t *real_bound_simplify(const expr_t *expr, expr_t *a, expr_t *b)
{
    (void)expr;
    expr_t *result = real_bound_new(a);
    expr_free(a);
    expr_free(b);
    return result;
}

const expr_ops_t ops_real_bound = {
    .eval = real_bound_eval, .kind = EXPR_KIND_REAL_BOUND, .arity = EXPR_OP_UNARY,
    .expression_name = "Re", .function_name = "realpart", .TeX_name = "\\operatorname{Re}",
    .apply_unary = real_bound_new, .simplify = real_bound_simplify,
};

static number_t real_parameter_eval(expr_t *expr)
{
    number_t value = expr_eval(expr->a);
    bool valid = num_is_finite(value) && num_is_real(value);
    num_destroy(&value);
    return num_clone(valid ? NUM_ONE : NUM_ZERO);
}

static expr_t *real_parameter_new(const expr_t *value)
{
    return expr_new_unary_internal(&ops_real_parameter, expr_clone(value));
}

const expr_ops_t ops_real_parameter = {
    .eval = real_parameter_eval, .kind = EXPR_KIND_REAL_PARAMETER, .arity = EXPR_OP_UNARY,
    .expression_name = "real_parameter", .function_name = "real_parameter",
    .apply_unary = real_parameter_new,
};

static number_t nonnegative_integer_eval(expr_t *expr)
{
    number_t value = expr_eval(expr->a);
    bool valid = num_is_finite(value) && num_is_real(value) && num_is_integer(value) && num_ge(value, NUM_ZERO);
    num_destroy(&value);
    return num_clone(valid ? NUM_ONE : NUM_ZERO);
}

static expr_t *nonnegative_integer_new(const expr_t *value)
{
    return expr_new_unary_internal(&ops_nonnegative_integer, expr_clone(value));
}

const expr_ops_t ops_nonnegative_integer = {
    .eval = nonnegative_integer_eval, .kind = EXPR_KIND_NONNEGATIVE_INTEGER, .arity = EXPR_OP_UNARY,
    .expression_name = "nonnegative_integer", .function_name = "nonnegative_integer",
    .apply_unary = nonnegative_integer_new,
};

/* A formula restricted to finite arguments in open right half-planes. */
static number_t real_domain_eval(expr_t *expr)
{
    for (const expr_t *pair = expr->b; pair; pair = pair->b->b) {
        number_t value = expr_eval(pair->a);
        number_t limit = expr_eval(pair->b->a);
        number_t real = num_real_part(value);
        bool valid = num_is_finite(value) && num_is_finite(limit) && num_is_real(limit) && num_gt(real, limit);
        num_destroy(&real);
        num_destroy(&limit);
        num_destroy(&value);
        if (!valid)
            return num_clone(NUM_NAN);
    }
    return expr_eval(expr->a);
}

static expr_t *real_domain_deriv(expr_t *expr)
{
    const expr_t *wrt = expr_current_wrt_internal();
    expr_t *derivative = wrt ? expr_create_deriv(expr->a, wrt) : NULL;
    if (!derivative)
        return NULL;
    /* Differentiate on the interior of the existing domain; do not differentiate its predicates. */
    expr_t *out = expr_alloc(&ops_real_domain);
    out->a = derivative;
    out->b = expr_clone(expr->b);
    return out;
}

static bool domain_numeric(const expr_t *expr)
{
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(expr);
    bool numeric = expr_bindings_count(bindings) == 0u;
    expr_bindings_free(bindings);
    return numeric;
}

static expr_t *real_domain_simplify(const expr_t *expr, expr_t *a, expr_t *b)
{
    (void)expr;
    if (expr_is_laplace_transform(a)) {
        expr_t *evaluated = expr_transform_result(a);
        if (evaluated) {
            expr_free(a);
            a = evaluated;
        }
    }
    while (a && a->ops == &ops_real_domain) {
        expr_t *wrapper = a;
        a = wrapper->a;
        expr_t **end = &b;
        while (*end)
            end = &(*end)->b->b;
        *end = wrapper->b;
        wrapper->a = wrapper->b = NULL;
        expr_free(wrapper);
    }
    expr_t *out = expr_alloc(&ops_real_domain);
    out->a = a;
    expr_t **tail = &out->b;
    for (const expr_t *pair = b; pair; pair = pair->b->b) {
        // Domains are normally tiny; cap duplicate comparisons for large user-authored lists.
        bool duplicate = false;
        unsigned int compared = 0u;
        for (const expr_t *seen = out->b; seen && compared < 64u; seen = seen->b->b, ++compared) {
            if (domain_same_value(seen->a, pair->a) && domain_same_value(seen->b->a, pair->b->a)) {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
            continue;
        if (domain_numeric(pair->a) && domain_numeric(pair->b->a)) {
            number_t value = expr_eval(pair->a);
            number_t limit = expr_eval(pair->b->a);
            number_t real = num_real_part(value);
            bool valid = num_is_finite(value) && num_is_finite(limit) && num_is_real(limit) && num_gt(real, limit);
            num_destroy(&real);
            num_destroy(&limit);
            num_destroy(&value);
            if (!valid) {
                expr_free(out);
                expr_free(b);
                return expr_new_const(NUM_NAN);
            }
            continue;
        }
        *tail = expr_alloc(&ops_argument_list);
        (*tail)->a = expr_clone(pair->a);
        (*tail)->b = expr_alloc(&ops_argument_list);
        (*tail)->b->a = expr_clone(pair->b->a);
        tail = &(*tail)->b->b;
    }
    expr_free(b);
    if (!out->b) {
        out->a = NULL;
        expr_free(out);
        return a;
    }
    return out;
}

const expr_ops_t ops_real_domain = {
    .eval = real_domain_eval, .deriv = real_domain_deriv, .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_REAL_DOMAIN, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_SMOOTH,
    .expression_name = "where", .function_name = "where",
    .simplify = real_domain_simplify,
};

/* Arguments are the formula followed by (value, real lower bound) pairs. */
expr_t *expr_real_domain_from_args(size_t count, expr_t *const *args)
{
    if (!args || count < 3u || !(count & 1u))
        return NULL;
    expr_t *out = expr_alloc(&ops_real_domain);
    out->a = expr_clone(args[0]);
    expr_t **tail = &out->b;
    for (size_t index = 1u; index < count; ++index) {
        *tail = expr_alloc(&ops_argument_list);
        (*tail)->a = expr_clone(args[index]);
        tail = &(*tail)->b;
    }
    return out;
}

static bool contains_real_domain(const expr_t *expr)
{
    return expr && (expr->ops == &ops_real_domain || contains_real_domain(expr->a) || contains_real_domain(expr->b));
}

/* Work on an owned clone, including condition metadata which is not an algebraic operator. */
static void domain_replace_constant(expr_t **node, const char *name, const expr_t *replacement)
{
    expr_t *expr = *node;
    if (!expr)
        return;
    if (expr_is_const(expr) && expr->name && strcmp(expr->name, name) == 0) {
        *node = expr_clone(replacement);
        expr_free(expr);
        return;
    }
    domain_replace_constant(&expr->a, name, replacement);
    domain_replace_constant(&expr->b, name, replacement);
    expr->simplified = false;
    if (expr->ops == &ops_pow && domain_numeric(expr->b)) {
        number_t exponent = expr_eval(expr->b);
        if (num_is_integer(exponent)) {
            *node = expr_pow(expr->a, &exponent);
            expr_free(expr);
        }
        num_destroy(&exponent);
    } else if (expr->ops == &ops_gamma && domain_numeric(expr->a)) {
        number_t order = expr_eval(expr->a);
        if (num_is_integer(order) && num_gt(order, NUM_ZERO)) {
            number_t factorial = num_gamma(order);
            *node = expr_new_const(factorial);
            num_destroy(&factorial);
            expr_free(expr);
        }
        num_destroy(&order);
    }
}

static expr_t *domain_specialise_copy(const expr_t *expr)
{
    expr_t *result = expr_clone(expr);
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(expr);
    for (size_t index = 0u; index < expr_bindings_count(bindings); ++index) {
        expr_t *binding = expr_bindings_get(bindings, expr_bindings_name_at(bindings, index));
        if (!expr_is_const(binding))
            continue;
        number_t value = expr_eval(binding);
        if (num_is_finite(value)) {
            expr_t *replacement = binding->binding_expr ? expr_binding_expr_eval_expr(binding->binding_expr)
                                                       : expr_new_const(value);
            if (replacement)
                domain_replace_constant(&result, binding->name, replacement);
            expr_free(replacement);
        }
        num_destroy(&value);
    }
    expr_bindings_free(bindings);
    expr_t *out = expr_beautify(result);
    expr_free(result);
    return out;
}

/* Specialise supplied constant parameters, never the free transform variable. */
expr_t *expr_transform_specialise_constants(const expr_t *expr)
{
    expr_t *simplified = expr_simplify(expr);
    expr_t *out = contains_real_domain(simplified) ? domain_specialise_copy(simplified) : NULL;
    expr_free(simplified);
    return out;
}

/* Present an evaluated transform as an identity without changing the result algebra. */
char *expr_laplace_identity_TeX(const expr_t *source, const expr_t *result)
{
    while (source && source->ops == &ops_real_domain)
        source = source->a;
    while (result && result->ops == &ops_real_domain && !result->b)
        result = result->a;
    if (!expr_is_laplace_transform(source) || !result)
        return NULL;
    bool shifted_formal = source->ops == &ops_laplace && result->ops == &ops_laplace &&
                          !expr_struct_eq(source->b->b->a, result->b->b->a);
    if (expr_is_laplace_transform(result) && !shifted_formal)
        return NULL;
    expr_t *operand = shifted_formal ? expr_clone(source->a) : domain_specialise_copy(source->a);
    char *body = expr_to_TeX_body(operand);
    char *from = expr_to_TeX_body(source->b->a);
    char *to = expr_to_TeX_body(source->b->b->a);
    char *rhs = expr_to_TeX_body(result);
    char *out = NULL;
    if (body && from && to && rhs && shifted_formal) {
        char *argument = expr_to_TeX_body(result->b->b->a);
        char *function = expr_to_TeX_body(result->a);
        if (argument && function) {
            size_t size = strlen(body) + 2u * strlen(from) + 3u * strlen(to) +
                          2u * strlen(argument) + strlen(function) + 256u;
            out = malloc(size);
            if (out)
                snprintf(out, size,
                         "\\mathcal{L}_{%s\\to %s}\\left\\{%s\\right\\} = F\\left(%s\\right)"
                         "\\quad F(%s):=\\mathcal{L}_{%s\\to %s}\\left\\{%s\\right\\}"
                         "\\quad %s\\in\\operatorname{ROC}(F)",
                         from, to, body, argument, to, from, to, function, argument);
        }
        free(function);
        free(argument);
    } else if (body && from && to && rhs) {
        size_t size = strlen(body) + strlen(from) + strlen(to) + strlen(rhs) + 80u;
        out = malloc(size);
        if (out)
            snprintf(out, size, "%s_{%s\\to %s}\\left\\{%s\\right\\} = %s",
                     source->ops == &ops_inverse_laplace ? "\\mathcal{L}^{-1}" : "\\mathcal{L}",
                     from, to, body, rhs);
    }
    free(rhs);
    free(to);
    free(from);
    free(body);
    expr_free(operand);
    return out;
}
