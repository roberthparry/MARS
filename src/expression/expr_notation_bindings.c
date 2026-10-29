/**
 * @file expr_notation_bindings.c
 * @brief Prepare exact symbolic binding values for semantic notation callbacks.
 *
 * Binding arrays and arithmetic have their own expression-owned representation.
 * This adapter preserves that structure and literal decimal precision rather
 * than evaluating it or exposing its private union to a presentation module.
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "expression-t.h"
#include "TeX-t.h"

#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_binding_simplify.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_binding_render.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_notation_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"

static const TeX_model_node_t *build_binding(notation_build_t *build, const expr_binding_expr_t *binding, size_t depth);

static const TeX_model_node_t *build_number(notation_build_t *build, const expr_binding_expr_t *binding)
{
    const char *literal = binding->u.text ? binding->u.text : "";
    bool negative = *literal == '-';
    if (*literal == '+' || *literal == '-')
        ++literal;
    if (*literal == '@')
        ++literal;
    number_t value = binding_number_from_text(literal);
    if (num_is_inf(value) || num_is_nan(value)) {
        const TeX_model_node_t *node = num_is_inf(value)
            ? notation_build_literal(build, TeX_NODE_IDENTIFIER, negative ? "−∞" : "∞")
            : notation_build_number(build, value);
        num_destroy(&value);
        return node;
    }
    bool decimal = true;
    for (const unsigned char *p = (const unsigned char *)literal; *p; ++p)
        if (!(*p >= '0' && *p <= '9') && *p != '.' && *p != 'e' && *p != 'E' && *p != '+' && *p != '-') {
            decimal = false;
            break;
        }
    if (!decimal) {
        number_t signed_value = negative ? num_neg(value) : num_clone(value);
        const TeX_model_node_t *node = notation_build_number(build, signed_value);
        num_destroy(&signed_value);
        num_destroy(&value);
        return node;
    }
    num_destroy(&value);
    string_t *text = binding_decimal_display_text(literal);
    const TeX_model_node_t *node = text
        ? notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_NUMBER, .text = text}, NULL) : NULL;
    string_free(text);
    if (negative)
        node = notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &node);
    return node;
}

static const TeX_model_node_t *build_constant(notation_build_t *build, expr_binding_const_id_t constant)
{
    static const char *const names[] = {
        [EXPR_BINDING_CONST_E]     = "e",
        [EXPR_BINDING_CONST_I]     = "i",
        [EXPR_BINDING_CONST_PI]    = "π",
        [EXPR_BINDING_CONST_PHI]   = "φ",
        [EXPR_BINDING_CONST_GAMMA] = "γ"
    };
    return (unsigned)constant < sizeof names / sizeof *names
        ? notation_build_literal(build, TeX_NODE_IDENTIFIER, names[constant]) : NULL;
}

static const TeX_model_node_t *build_array(notation_build_t *build, const expr_binding_expr_t *binding, size_t depth)
{
    size_t count = binding->u.array.unspecified ? 1u : binding->u.array.count;
    if (count > 65536u)
        return NULL;
    const TeX_model_node_t **items = count ? calloc(count, sizeof *items) : NULL;
    if (count && !items)
        return NULL;
    for (size_t i = 0; i < count; ++i)
        items[i] = binding->u.array.unspecified ? notation_build_literal(build, TeX_NODE_IDENTIFIER, "?")
            : build_binding(build, binding->u.array.items[i], depth + 1u);
    const TeX_model_node_t *list = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_LIST, .child_count = count}, items);
    free(items);
    return notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_FENCED, .fence = TeX_FENCE_BRACKETS, .child_count = 1u}, &list);
}

static const TeX_model_node_t *build_binding(notation_build_t *build, const expr_binding_expr_t *binding, size_t depth)
{
    if (depth >= 256u) {
        errno = EOVERFLOW;
        return NULL;
    }
    if (!binding)
        return notation_build_literal(build, TeX_NODE_NUMBER, "0");
    static const TeX_node_kind_t binary[] = {
        [EXPR_BINDING_EXPR_ADD] = TeX_NODE_SUM,
        [EXPR_BINDING_EXPR_SUB] = TeX_NODE_DIFFERENCE,
        [EXPR_BINDING_EXPR_MUL] = TeX_NODE_PRODUCT,
        [EXPR_BINDING_EXPR_DIV] = TeX_NODE_FRACTION
    };
    if ((unsigned)binding->kind < sizeof binary / sizeof *binary && binary[binding->kind]) {
        const expr_binding_expr_t *left = binding->u.binary.left, *right = binding->u.binary.right;
        if (binding->kind == EXPR_BINDING_EXPR_MUL && binding_mul_should_swap_power_before_radical(left, right)) {
            const expr_binding_expr_t *swap = left;
            left = right;
            right = swap;
        }
        const TeX_model_node_t *parts[] = {
            build_binding(build, left, depth + 1u), build_binding(build, right, depth + 1u)
        };
        return notation_build_node(build,
            (TeX_node_view_t){.kind = binary[binding->kind], .child_count = 2u}, parts);
    }
    /* Remaining alternatives have distinct storage layouts, not a repeated operation-name search. */
    switch (binding->kind) {
        case EXPR_BINDING_EXPR_ARRAY:
            return build_array(build, binding, depth);

        case EXPR_BINDING_EXPR_NUMBER:
            return build_number(build, binding);

        case EXPR_BINDING_EXPR_CONST:
            return build_constant(build, binding->u.const_id);

        case EXPR_BINDING_EXPR_NEG: {
            const TeX_model_node_t *child = build_binding(build, binding->u.unary.child, depth + 1u);
            return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &child);
        }

        case EXPR_BINDING_EXPR_POWI: {
            number_t exponent = num_create_from_long(binding->u.powi.exponent);
            const TeX_model_node_t *parts[] = {
                build_binding(build, binding->u.powi.base, depth + 1u), notation_build_number(build, exponent)
            };
            num_destroy(&exponent);
            return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_POWER, .child_count = 2u}, parts);
        }

        case EXPR_BINDING_EXPR_UNARY_OP: {
            if (binding->u.unary_op.ops == &ops_exp) {
                number_t exponent = num_new();
                long numerator = 0L, denominator = 0L;
                bool root = expr_binding_expr_number_value(binding->u.unary_op.child, &exponent) &&
                    num_get_small_rational(exponent, &numerator, &denominator) && numerator == 1L && denominator > 1L;
                num_destroy(&exponent);
                if (root) {
                    number_t degree = num_create_from_long(denominator);
                    const TeX_model_node_t *parts[] = {
                        notation_build_literal(build, TeX_NODE_IDENTIFIER, "e"), notation_build_number(build, degree)
                    };
                    num_destroy(&degree);
                    return notation_build_node(build, (TeX_node_view_t){
                        .kind = TeX_NODE_ROOT, .child_count = denominator == 2L ? 1u : 2u}, parts);
                }
            }
            return expr_notation_operation(build, binding->u.unary_op.ops,
                build_binding(build, binding->u.unary_op.child, depth + 1u), NULL);
        }

        case EXPR_BINDING_EXPR_BINARY_OP: {
            const TeX_model_node_t *left = build_binding(build, binding->u.binary_op.left, depth + 1u);
            const TeX_model_node_t *right = build_binding(build, binding->u.binary_op.right, depth + 1u);
            return left && right ? expr_notation_operation(build, binding->u.binary_op.ops, left, right) : NULL;
        }

        default:
            errno = EINVAL;
            return NULL;
    }
}

/* Keep binding arithmetic symbolic while copying it into an independently owned snapshot. */
const TeX_model_node_t *expr_notation_binding(notation_build_t *build, const expr_binding_expr_t *binding)
{
    return build_binding(build, binding, 0u);
}

/* Preserve exact surface arithmetic unless formal derivatives require the native graph. */
TeX_context_t *expr_notation_surface_context(const expr_t *expression, bool preserve_root_symbol)
{
    if (!expression || !expression->binding_expr || expr_is_const(expression) ||
        (preserve_root_symbol && expr_is_var(expression)) || expr_notation_has_formal_derivative(expression))
        return NULL;
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t *root = expr_notation_binding(build, expression->binding_expr);
    TeX_context_t *context = notation_build_context(build, root);
    notation_build_free(build);
    return context;
}

/* Snapshot one native binding without exposing its private encoding to other modules. */
const TeX_model_node_t *notation_build_assignment(notation_build_t *build, const expr_t *symbol, bool *unset_out)
{
    if (!symbol)
        return NULL;
    bool unset = !symbol->binding_expr && num_is_nan(symbol->c);
    if (symbol->binding_expr && symbol->binding_expr->kind == EXPR_BINDING_EXPR_NUMBER) {
        number_t value = NUM_ZERO;
        unset = expr_binding_expr_number_value(symbol->binding_expr, &value) && num_is_nan(value);
        num_destroy(&value);
    }
    if (unset_out)
        *unset_out = unset;
    const TeX_model_node_t *value = unset ? notation_build_literal(build, TeX_NODE_IDENTIFIER, "?")
        : symbol->binding_expr ? expr_notation_binding(build, symbol->binding_expr)
                               : notation_build_number(build, symbol->c);
    const TeX_model_node_t *parts[] = {
        expr_notation_name(build, expr_name_or_default(symbol, "x")), value
    };
    return notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_BINDING_ASSIGNMENT, .child_count = 2u}, parts);
}

/* Collect the expression-owned binding set into a presentation-independent snapshot. */
bool expr_notation_bindings_context(const expr_t *expression, TeX_context_t **context)
{
    if (!expression || !context)
        return false;
    *context = NULL;
    varlist_t variables, constants;
    varlist_init(&variables);
    varlist_init(&constants);
    find_vars_dfs(expression, &variables);
    find_explicit_named_consts_dfs(expression, &constants);
    find_named_consts_dfs(expression, &constants);
    if (variables.count > 65536u || constants.count > 65536u - variables.count) {
        free(constants.vars);
        free(variables.vars);
        errno = EOVERFLOW;
        return false;
    }
    size_t count = variables.count + constants.count;
    notation_build_t *build = count ? notation_build_new() : NULL;
    const TeX_model_node_t **children = build ? calloc(count, sizeof *children) : NULL;
    if (children) {
        for (size_t i = 0u; i < count; ++i)
            children[i] = notation_build_assignment(build, i < variables.count ? variables.vars[i]
                                                                       : constants.vars[i - variables.count], NULL);
    }
    const TeX_model_node_t *root = children ? notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_BINDINGS, .rows = variables.count, .child_count = count}, children) : NULL;
    if (count)
        *context = notation_build_context(build, root);
    free(children);
    notation_build_free(build);
    free(constants.vars);
    free(variables.vars);
    return !count || *context != NULL;
}
