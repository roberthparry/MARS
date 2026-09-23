#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#include <string.h>

typedef struct {
    const char *preferred_source;
    const char *default_target;
    expr_t *(*result)(const expr_t *);
} transform_descriptor_t;

static const transform_descriptor_t transform_descriptors[EXPR_KIND_COUNT] = {
    [EXPR_KIND_LAPLACE]         = { "t", "s", expr_laplace_result },
    [EXPR_KIND_INVERSE_LAPLACE] = { "s", "t", expr_inverse_laplace_result },
    [EXPR_KIND_FOURIER]         = { "t", NULL, expr_fourier_result },
    [EXPR_KIND_INVERSE_FOURIER] = { "ω", NULL, expr_fourier_result },
};

static const char *fourier_target(const char *source, bool inverse)
{
    /* Four conventional coordinate pairs; arbitrary names require an explicit target. */
    static const char *const pairs[4][2] = {{"t", "ω"}, {"x", "k"}, {"y", "m"}, {"z", "n"}};
    for (size_t i = 0u; i < 4u; ++i)
        if (strcmp(source, pairs[i][inverse]) == 0)
            return pairs[i][!inverse];
    return NULL;
}

/* A real-coordinate projection is a parameter, not the variable on its vertical line. */
bool expr_transform_real_coordinate_only(const expr_t *expr, const expr_t *symbol)
{
    if (!expr || expr->ops == &ops_real_bound)
        return true;
    if (expr->ops == &ops_real_domain)
        return expr_transform_real_coordinate_only(expr->a, symbol);
    if (expr_struct_eq(expr, symbol) || (expr->name && symbol->name && strcmp(expr->name, symbol->name) == 0))
        return false;
    return expr_transform_real_coordinate_only(expr->a, symbol) &&
           expr_transform_real_coordinate_only(expr->b, symbol);
}

/* Construct shared metadata whilst keeping the source bound and the target free. */
expr_t *expr_integral_transform_from_args(size_t count, expr_t *const *args, const expr_ops_t *ops)
{
    if (!args || count < 1u || count > 3u || !ops || !transform_descriptors[ops->kind].result)
        return NULL;
    const transform_descriptor_t *descriptor = &transform_descriptors[ops->kind];
    bool fourier = ops == &ops_fourier || ops == &ops_inverse_fourier;
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(args[0]);
    const expr_t *source = count > 1u ? args[1] : NULL;
    expr_t *coordinate_source = NULL;
    if (source && fourier && source->ops == &ops_imag_coordinate && source->a->name) {
        const expr_t *bound = expr_bindings_get(bindings, source->a->name);
        coordinate_source = expr_imag_coordinate(bound ? bound : source->a);
        source = coordinate_source;
    }
    /* A copied bound Expression owns its free symbols. Bind the coordinate in that
     * operand's scope, not a different outer symbol with the same spelling. */
    if (source && source->name) {
        const expr_t *bound = expr_bindings_get(bindings, source->name);
        if (bound && (expr_is_var(bound) || (expr_is_named_const(bound) && num_is_nan(bound->c))))
            source = bound;
    }
    if (!source) {
        const expr_t *preferred = expr_bindings_get(bindings, descriptor->preferred_source);
        if (preferred && expr_is_var(preferred))
            source = preferred;
    }
    if (!source) {
        for (size_t i = 0u; i < expr_bindings_count(bindings); ++i) {
            expr_t *candidate = expr_bindings_get(bindings, expr_bindings_name_at(bindings, i));
            if (!expr_is_var(candidate))
                continue;
            if (ops == &ops_inverse_fourier && !fourier_target(candidate->name, true) &&
                expr_transform_real_coordinate_only(args[0], candidate))
                continue;
            if (source) {
                expr_bindings_free(bindings);
                return NULL;
            }
            source = candidate;
        }
    }
    const char *target_name = descriptor->default_target;
    expr_t *implicit_source = NULL;
    /* Conventional spatial frequencies are otherwise parameter-like names in the parser.
     * An explicit coordinate (or the sole conventional inverse coordinate) binds that symbol. */
    if (!source && ops == &ops_inverse_fourier) {
        for (size_t i = 0u; i < expr_bindings_count(bindings); ++i) {
            expr_t *candidate = expr_bindings_get(bindings, expr_bindings_name_at(bindings, i));
            if (!expr_is_named_const(candidate) || !num_is_nan(candidate->c) || !fourier_target(candidate->name, true))
                continue;
            if (source) {
                expr_bindings_free(bindings);
                return NULL;
            }
            source = candidate;
        }
    }
    expr_t *promoted_body = NULL;
    if (source && expr_is_named_const(source) && num_is_nan(source->c)) {
        implicit_source = expr_new_named_var(NUM_NAN, source->name);
        /* Match every named occurrence, including separately cloned domain predicates. */
        promoted_body = implicit_source ? expr_substitute(args[0], implicit_source, implicit_source) : NULL;
        source = implicit_source;
    }
    if (!source && !target_name) {
        implicit_source = expr_new_named_var(NUM_NAN, descriptor->preferred_source);
        source = implicit_source;
    }
    /* Only the shorthand bare gamma request selects a vertical line automatically.
     * Explicit Fourier(gamma(x), x, k) keeps its usual real-axis meaning. */
    if (count == 1u && ops == &ops_fourier && args[0]->ops == &ops_gamma &&
        source && expr_is_var(source) && expr_struct_eq(args[0]->a, source)) {
        coordinate_source = expr_imag_coordinate(source);
        source = coordinate_source;
    }
    const expr_t *source_symbol = source && source->ops == &ops_imag_coordinate ? source->a : source;
    if (!target_name && source_symbol && source_symbol->name)
        target_name = fourier_target(source_symbol->name, ops == &ops_inverse_fourier);
    expr_t *target = count > 2u ? expr_clone(args[2])
                              : target_name ? expr_new_named_var(NUM_NAN, target_name) : NULL;
    if (count < 3u && ops == &ops_inverse_fourier && target && target->name) {
        const expr_t *parameter = expr_bindings_get(bindings, target->name);
        if (parameter && expr_transform_real_coordinate_only(args[0], parameter)) {
            expr_free(target);
            target = expr_imag_coordinate(parameter);
        }
    }
    bool used = true;
    expr_t *variable = (expr_t *)source;
    if (target && source)
        expr_collect_var_usage(target, 1u, &variable, &used);
    bool collision = target && target->name && expr_bindings_get(bindings, target->name);
    expr_t *out = NULL;
    if (source_symbol && expr_is_var(source_symbol) && source_symbol->name && target && !used &&
        (expr_is_var(source) || (fourier && source->ops == &ops_imag_coordinate)) &&
        (!expr_is_var(target) || (target->name && !collision && strcmp(source_symbol->name, target->name) != 0))) {
        out = expr_alloc(ops);
        out->a = expr_clone(promoted_body ? promoted_body : args[0]);
        out->b = expr_alloc(&ops_argument_list);
        out->b->a = expr_clone(source);
        out->b->b = expr_alloc(&ops_argument_list);
        out->b->b->a = expr_clone(target);
        out->b->b->b = expr_const_long((long)count);
    }
    expr_free(target);
    expr_free(implicit_source);
    expr_free(promoted_body);
    expr_free(coordinate_source);
    expr_bindings_free(bindings);
    return out;
}

/* Construct a unilateral Laplace transform. */
expr_t *expr_laplace_from_args(size_t count, expr_t *const *args)
{
    return expr_integral_transform_from_args(count, args, &ops_laplace);
}

/* Construct an inverse Laplace transform. */
expr_t *expr_inverse_laplace_from_args(size_t count, expr_t *const *args)
{
    return expr_integral_transform_from_args(count, args, &ops_inverse_laplace);
}

/* Construct an angular-frequency Fourier transform. */
expr_t *expr_fourier_from_args(size_t count, expr_t *const *args)
{
    return expr_integral_transform_from_args(count, args, &ops_fourier);
}

/* Construct an inverse angular-frequency Fourier transform. */
expr_t *expr_inverse_fourier_from_args(size_t count, expr_t *const *args)
{
    return expr_integral_transform_from_args(count, args, &ops_inverse_fourier);
}

/* Identify integral transforms without repeating operator lists throughout the module. */
bool expr_is_integral_transform(const expr_t *expr)
{
    return expr && transform_descriptors[expr->ops->kind].result;
}

/* Dispatch recognised formulas through the operator's native implementation. */
expr_t *expr_transform_result(const expr_t *transform)
{
    return expr_is_integral_transform(transform) ? transform_descriptors[transform->ops->kind].result(transform) : NULL;
}
