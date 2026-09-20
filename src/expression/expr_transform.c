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

/* Construct shared metadata whilst keeping the source bound and the target free. */
expr_t *expr_integral_transform_from_args(size_t count, expr_t *const *args, const expr_ops_t *ops)
{
    if (!args || count < 1u || count > 3u || !ops || !transform_descriptors[ops->kind].result)
        return NULL;
    const transform_descriptor_t *descriptor = &transform_descriptors[ops->kind];
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(args[0]);
    const expr_t *source = count > 1u ? args[1] : NULL;
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
            if (source) {
                expr_bindings_free(bindings);
                return NULL;
            }
            source = candidate;
        }
    }
    const char *target_name = descriptor->default_target;
    expr_t *implicit_source = NULL;
    if (!source && !target_name) {
        implicit_source = expr_new_named_var(NUM_NAN, descriptor->preferred_source);
        source = implicit_source;
    }
    if (!target_name && source && source->name)
        target_name = fourier_target(source->name, ops == &ops_inverse_fourier);
    expr_t *target = count > 2u ? expr_clone(args[2])
                              : target_name ? expr_new_named_var(NUM_NAN, target_name) : NULL;
    bool used = true;
    expr_t *variable = (expr_t *)source;
    if (target && source)
        expr_collect_var_usage(target, 1u, &variable, &used);
    bool collision = target && target->name && expr_bindings_get(bindings, target->name);
    expr_t *out = NULL;
    if (source && expr_is_var(source) && source->name && target && !used &&
        (!expr_is_var(target) || (target->name && !collision && strcmp(source->name, target->name) != 0))) {
        out = expr_alloc(ops);
        out->a = expr_clone(args[0]);
        out->b = expr_alloc(&ops_argument_list);
        out->b->a = expr_clone(source);
        out->b->b = expr_alloc(&ops_argument_list);
        out->b->b->a = expr_clone(target);
        out->b->b->b = expr_const_long((long)count);
    }
    expr_free(target);
    expr_free(implicit_source);
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
