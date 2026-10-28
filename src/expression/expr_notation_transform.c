/**
 * @file expr_notation_transform.c
 * @brief Prepare integral-transform identities as native mathematical documents.
 *
 * Recognition, bound-constant specialisation and coordinate identity belong to
 * expression. The resulting semantic graph contains transformations, equalities,
 * definitions and convergence-region membership; TeX only presents that graph.
 */
#include <stdlib.h>
#include <string.h>

#include "expression-t.h"
#include "TeX-t.h"

#define MARS_SHARED_EXPR_NOTATION_INTERNAL_ACCESS
#include "internal/expr_notation_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

/* Derivative recognition belongs to expression, not to its presentation backend. */
bool expr_notation_has_formal_derivative(const expr_t *expression)
{
    return expression && (expr_is_formal_derivative(expression) ||
                          expr_notation_has_formal_derivative(expression->a) ||
                          expr_notation_has_formal_derivative(expression->b));
}

typedef struct {
    const expr_t *initial;
    size_t count;
    bool has_transform;
    bool name_collision;
} primitive_context_t;

static void find_primitive_initial(const expr_t *expr, primitive_context_t *context)
{
    if (!expr)
        return;
    context->has_transform |= expr_is_integral_transform(expr);
    context->name_collision |= expr->name && strcmp(expr->name, "F") == 0;
    if (expr_is_op(expr, &ops_integral) && !expr_integral_lower_bound_expr(expr) &&
        expr_const_is_zero(expr_integral_upper_bound_expr(expr))) {
        context->initial = expr;
        ++context->count;
    }
    find_primitive_initial(expr->a, context);
    find_primitive_initial(expr->b, context);
}

static bool replace_primitive(expr_t **display, const expr_t *source, const expr_t *initial, const expr_t *alias)
{
    if (!source || !*display)
        return true;
    if (source == initial) {
        expr_t *replacement = expr_clone(alias);
        if (!replacement)
            return false;
        expr_free(*display);
        *display = replacement;
        return true;
    }
    return replace_primitive(&(*display)->a, source->a, initial, alias) &&
           replace_primitive(&(*display)->b, source->b, initial, alias);
}

/* Select a local abbreviation without changing the mathematical result. */
expr_t *expr_notation_primitive_display(const expr_t *expr, expr_t **initial)
{
    if (!initial)
        return NULL;
    *initial = NULL;
    if (!expr)
        return NULL;
    primitive_context_t context = {0};
    expr_t *resolved = expr_is_integral_transform(expr) ? expr_transform_result(expr) : NULL;
    const expr_t *source = resolved ? resolved : expr;
    find_primitive_initial(source, &context);
    if (!context.has_transform || context.name_collision || context.count != 1u ||
        !expr_is_var(expr_integral_dummy_expr(context.initial))) {
        expr_free(resolved);
        return NULL;
    }
    expr_t *zero = expr_const_zero();
    expr_t *alias = zero ? expr_new_arbitrary_function("F", zero) : NULL;
    expr_t *display = alias ? expr_clone(source) : NULL;
    if (display && replace_primitive(&display, source, context.initial, alias))
        *initial = expr_clone(context.initial);
    if (!*initial) {
        expr_free(display);
        display = NULL;
    }
    expr_free(alias);
    expr_free(zero);
    expr_free(resolved);
    return display;
}

/* Supply the definition's mathematical operands without exposing integral storage. */
TeX_context_t *expr_notation_primitive_context(const expr_t *initial)
{
    if (!initial || !expr_is_op(initial, &ops_integral))
        return NULL;
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t *children[] = {
        notation_build_expr(build, expr_integral_dummy_expr(initial)), notation_build_expr(build, initial->a)
    };
    const TeX_model_node_t *root = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_ANTIDERIVATIVE_DEFINITION, .child_count = 2u}, children);
    TeX_context_t *context = notation_build_context(build, root);
    notation_build_free(build);
    return context;
}

static const TeX_model_node_t *build_coordinate(notation_build_t *build, const expr_t *coordinate)
{
    return coordinate && coordinate->name
        ? notation_build_literal(build, TeX_NODE_IDENTIFIER, coordinate->name)
        : notation_build_expr(build, coordinate);
}

static const TeX_model_node_t *build_transform(notation_build_t *build, const expr_t *source,
                                               const expr_t *operand)
{
    static const TeX_transform_kind_t kinds[EXPR_KIND_COUNT] = {
        [EXPR_KIND_LAPLACE]         = TeX_TRANSFORM_LAPLACE,
        [EXPR_KIND_INVERSE_LAPLACE] = TeX_TRANSFORM_INVERSE_LAPLACE,
        [EXPR_KIND_FOURIER]         = TeX_TRANSFORM_FOURIER,
        [EXPR_KIND_INVERSE_FOURIER] = TeX_TRANSFORM_INVERSE_FOURIER
    };
    const TeX_model_node_t *children[] = {
        notation_build_expr(build, operand), build_coordinate(build, source->b->a),
        build_coordinate(build, source->b->b->a)
    };
    return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_TRANSFORM,
        .transform = kinds[source->ops->kind], .child_count = 3u}, children);
}

static const TeX_model_node_t *build_call(notation_build_t *build, const char *name,
                                         const TeX_model_node_t *argument)
{
    string_t *text = string_new_with(name);
    const TeX_model_node_t *node = text ? notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_CALL, .text = text, .child_count = 1u}, &argument) : NULL;
    string_free(text);
    return node;
}

/* Preserve transformation and convergence meaning without passing any typesetting source. */
char *expr_transform_identity_TeX(const expr_t *source, const expr_t *result)
{
    while (source && source->ops == &ops_real_domain)
        source = source->a;
    while (result && result->ops == &ops_real_domain && !result->b)
        result = result->a;
    if (!expr_is_integral_transform(source) || !result)
        return NULL;
    const expr_t *source_target = source->b->b->a;
    const expr_t *result_target = result->ops == &ops_laplace ? result->b->b->a : NULL;
    bool same_target = result_target && (expr_struct_eq(source_target, result_target) ||
        (source_target->name && result_target->name && !strcmp(source_target->name, result_target->name)));
    bool shifted = source->ops == &ops_laplace && result->ops == &ops_laplace && !same_target;
    if (expr_is_integral_transform(result) && !shifted)
        return NULL;
    expr_t *operand = shifted ? expr_clone(source->a) : expr_transform_bound_constants(source->a);
    if (!operand)
        operand = expr_clone(source->a);
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t *left = build_transform(build, source, operand);
    notation_build_result_mode(build, true);
    const TeX_model_node_t *sides[] = {
        left, shifted
            ? build_call(build, "F", notation_build_expr(build, result_target)) : notation_build_expr(build, result)
    };
    const TeX_model_node_t *root = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_EQUATION, .child_count = 2u}, sides);
    if (shifted) {
        const TeX_model_node_t *definition[] = {
            build_call(build, "F", build_coordinate(build, source_target)), build_transform(build, source, result->a)
        };
        const TeX_model_node_t *membership[] = {
            notation_build_expr(build, result_target),
            build_call(build, "ROC", notation_build_literal(build, TeX_NODE_IDENTIFIER, "F"))
        };
        const TeX_model_node_t *statements[] = {
            root,
            notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_RELATION,
                .relation = TeX_RELATION_DEFINITION, .child_count = 2u}, definition),
            notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_RELATION,
                .relation = TeX_RELATION_MEMBER, .child_count = 2u}, membership)
        };
        root = notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_STATEMENTS, .child_count = 3u}, statements);
    }
    string_t *text = notation_build_render(build, root);
    char *output = text ? strdup(string_c_str(text)) : NULL;
    string_free(text);
    notation_build_free(build);
    expr_free(operand);
    return output;
}
