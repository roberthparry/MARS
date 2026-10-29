/**
 * @file expr_notation_calculus.c
 * @brief Separate scalar coefficients from calculus factors before presentation.
 *
 * Native expressions determine which factors contain derivatives or integrals.
 * The resulting semantic quotient keeps those factors outside algebraic fractions
 * without changing their order, signs or denominator restrictions.
 */
#include <stdbool.h>

#include "expression-t.h"
#include "TeX-t.h"

#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_notation_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"

typedef struct {
    expr_t *coefficient;
    const TeX_model_node_t *calculus;
    bool negative;
} calculus_factors_t;

static bool contains_transform(const expr_t *expression)
{
    return expression && (expr_is_integral_transform(expression) || contains_transform(expression->a) ||
                          contains_transform(expression->b));
}

/* Transform derivatives use an ordinary derivative when all coordinates coincide. */
bool expr_notation_is_transform_derivative(const expr_t *expression)
{
    if (!expr_is_formal_derivative(expression) || !expression->formal_wrt_count ||
        !contains_transform(expression->a))
        return false;
    for (size_t i = 1u; i < expression->formal_wrt_count; ++i)
        if (!expr_struct_eq(expression->formal_wrts[0], expression->formal_wrts[i]))
            return false;
    return true;
}

static bool contains_calculus(const expr_t *expression, unsigned depth)
{
    if (!expression || depth >= 256u)
        return false;
    return expr_is_formal_derivative(expression) || expr_is_integral_transform(expression) ||
           expr_is_op(expression, &ops_integral) || contains_calculus(expression->a, depth + 1u) ||
           contains_calculus(expression->b, depth + 1u);
}

/* Preserve exact reciprocal-sum structure without enclosing calculus in a fraction. */
const TeX_model_node_t *expr_notation_reciprocal_sum(notation_build_t *build, const expr_t *expression)
{
    const expr_t *factor = NULL, *sum = NULL;
    long numerator = 0, denominator = 0;
    if (!expr_notation_scaled_sum(expression, &factor, &sum) || !expr_is_unnamed_const(factor) ||
        (factor->binding_expr && !expr_binding_expr_is_numeric_literal(factor->binding_expr)) ||
        !num_get_small_rational(factor->c, &numerator, &denominator) || denominator <= 1 ||
        (numerator != 1 && numerator != -1) || contains_calculus(sum, 0u))
        return NULL;
    number_t divisor = num_create_from_long(denominator);
    const TeX_model_node_t *children[] = {notation_build_expr(build, sum), notation_build_number(build, divisor)};
    num_destroy(&divisor);
    const TeX_model_node_t *fraction = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_FRACTION, .child_count = 2u}, children);
    return numerator < 0 ? notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &fraction) : fraction;
}

/* An exponential never vanishes, so its reciprocal is an exact negative exponential. */
const TeX_model_node_t *expr_notation_inverse_exponential(notation_build_t *build, const expr_t *expression)
{
    const expr_t *denominator = expression->b;
    if (!expr_is_op(denominator, &ops_exp) || !contains_calculus(denominator->a, 0u))
        return NULL;
    const TeX_model_node_t *exponent = notation_build_expr(build, denominator->a);
    exponent = notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &exponent);
    const TeX_model_node_t *parts[] = {notation_build_literal(build, TeX_NODE_IDENTIFIER, "e"), exponent};
    const TeX_model_node_t *power = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_POWER, .child_count = 2u}, parts);
    if (expr_is_const(expression->a) && num_is_one(expression->a->c))
        return power;
    const TeX_model_node_t *product[] = {notation_build_expr(build, expression->a), power};
    return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_PRODUCT, .child_count = 2u}, product);
}

static bool collect_factors(notation_build_t *build, const expr_t *expression,
                            calculus_factors_t *factors, unsigned depth)
{
    if (!expression || depth >= 256u)
        return false;
    const expr_t *magnitude = NULL;
    if (expr_match_neg_expr(expression, &magnitude)) {
        factors->negative = !factors->negative;
        return collect_factors(build, magnitude, factors, depth + 1u);
    }
    if (expr_tostring_is_negative_const(expression)) {
        number_t positive = num_abs(expression->c);
        expr_t *value = expr_new_const(positive);
        num_destroy(&positive);
        factors->negative = !factors->negative;
        bool valid = value && collect_factors(build, value, factors, depth + 1u);
        expr_free(value);
        return valid;
    }
    if (expr_is_op(expression, &ops_mul))
        return collect_factors(build, expression->a, factors, depth + 1u) &&
               collect_factors(build, expression->b, factors, depth + 1u);
    if (contains_calculus(expression, 0u)) {
        const TeX_model_node_t *node = notation_build_expr(build, expression);
        const TeX_model_node_t *parts[] = {factors->calculus, node};
        factors->calculus = factors->calculus ? notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_PRODUCT, .child_count = 2u}, parts) : node;
        return factors->calculus != NULL;
    }
    expr_t *coefficient = factors->coefficient ? expr_mul(factors->coefficient, expression) : expr_clone(expression);
    expr_free(factors->coefficient);
    factors->coefficient = coefficient;
    return coefficient != NULL;
}

static bool product_factors(const expr_t *expression, expr_t **factors, size_t *count, unsigned depth)
{
    if (!expression || depth >= 256u || *count >= 256u)
        return false;
    if (expr_is_op(expression, &ops_mul))
        return product_factors(expression->a, factors, count, depth + 1u) &&
               product_factors(expression->b, factors, count, depth + 1u);
    factors[(*count)++] = (expr_t *)expression;
    return true;
}

/* Combine one exact rational factor with an enclosing denominator without moving calculus. */
const TeX_model_node_t *expr_notation_flat_fraction(notation_build_t *build, const expr_t *expression)
{
    if (contains_calculus(expression->a, 0u) || contains_calculus(expression->b, 0u))
        return NULL;
    expr_t *factors[256];
    size_t count = 0u, rational_index = 256u;
    number_t numerator = num_new(), denominator = num_new();
    if (!product_factors(expression->a, factors, &count, 0u))
        goto no_match;
    for (size_t i = 0u; i < count; ++i) {
        if (expr_is_const(factors[i]) && !factors[i]->name && num_is_real(factors[i]->c) &&
            num_get_sign(factors[i]->c) > 0 && num_get_fraction_parts(factors[i]->c, &numerator, &denominator) &&
            !num_eq(denominator, NUM_ONE)) {
            rational_index = i;
            break;
        }
    }
    if (rational_index == 256u)
        goto no_match;
    const TeX_model_node_t *top = num_eq(numerator, NUM_ONE) ? NULL : notation_build_number(build, numerator);
    for (size_t i = 0u; i < count; ++i) {
        if (i == rational_index)
            continue;
        const TeX_model_node_t *part = notation_build_expr(build, factors[i]);
        const TeX_model_node_t *children[] = {top, part};
        top = top ? notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_PRODUCT, .child_count = 2u},
                                        children) : part;
        if (!top)
            goto no_match;
    }
    if (!top)
        top = notation_build_literal(build, TeX_NODE_NUMBER, "1");
    const TeX_model_node_t *bottom_parts[] = {notation_build_number(build, denominator),
                                              notation_build_expr(build, expression->b)};
    const TeX_model_node_t *fraction_parts[] = {top, notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_PRODUCT, .child_count = 2u}, bottom_parts)};
    const TeX_model_node_t *result = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_FRACTION, .child_count = 2u}, fraction_parts);
    num_destroy(&numerator); num_destroy(&denominator);
    return result;
no_match:
    num_destroy(&numerator); num_destroy(&denominator);
    return NULL;
}

/* Preserve native factor ordering, then place scalar coefficients before calculus factors. */
const TeX_model_node_t *expr_notation_product(notation_build_t *build, const expr_t *expression)
{
    expr_t *factors[256];
    size_t count = 0u;
    if (!product_factors(expression, factors, &count, 0u))
        return NULL;
    sort_factors(factors, (int)count);
    bool calculus[256];
    for (size_t i = 0u; i < count; ++i)
        calculus[i] = contains_calculus(factors[i], 0u);
    const TeX_model_node_t *result = NULL;
    bool negative = false;
    /* Two bounded passes preserve the sorted order within both mathematical groups. */
    for (unsigned group = 0u; group < 2u; ++group) {
        for (size_t i = 0u; i < count; ++i) {
            if (calculus[i] != (group != 0u))
                continue;
            const expr_t *factor = factors[i];
            const TeX_model_node_t *part;
            if (expr_match_neg_expr(factor, &factor))
                negative = !negative;
            if (expr_tostring_is_negative_const(factor)) {
                negative = !negative;
                number_t positive = num_abs(factor->c);
                bool unit = num_eq(positive, NUM_ONE);
                const expr_binding_expr_t *binding = expr_tostring_should_emit_binding_expr(factor)
                    ? factor->binding_expr : NULL;
                if (binding && binding->kind == EXPR_BINDING_EXPR_NEG)
                    binding = binding->u.unary.child;
                else if (binding && binding->kind == EXPR_BINDING_EXPR_UNARY_OP &&
                         binding->u.unary_op.ops == &ops_neg)
                    binding = binding->u.unary_op.child;
                else
                    binding = NULL;
                part = unit ? NULL : binding ? expr_notation_binding(build, binding)
                                             : notation_build_number(build, positive);
                num_destroy(&positive);
                if (unit)
                    continue;
            } else {
                part = notation_build_expr(build, factor);
            }
            if (!part)
                return NULL;
            const TeX_model_node_t *parts[] = {result, part};
            result = result ? notation_build_node(build,
                (TeX_node_view_t){.kind = TeX_NODE_PRODUCT, .child_count = 2u}, parts) : part;
        }
    }
    if (!result)
        result = notation_build_literal(build, TeX_NODE_NUMBER, "1");
    return negative ? notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &result) : result;
}

/* Preserve calculus factors at full size alongside their scalar quotient. */
const TeX_model_node_t *expr_notation_calculus_quotient(notation_build_t *build, const expr_t *expression)
{
    if (!contains_calculus(expression->a, 0u) || contains_calculus(expression->b, 0u))
        return NULL;
    calculus_factors_t factors = {0};
    if (!collect_factors(build, expression->a, &factors, 0u)) {
        expr_free(factors.coefficient);
        return NULL;
    }
    const expr_t *denominator = expression->b, *magnitude = NULL;
    if (expr_match_neg_expr(denominator, &magnitude)) {
        factors.negative = !factors.negative;
        denominator = magnitude;
    }
    const TeX_model_node_t *scalar = factors.coefficient ? notation_build_expr(build, factors.coefficient) : NULL;
    const TeX_model_node_t *coefficient = NULL;
    if (expr_is_op(denominator, &ops_exp)) {
        const TeX_model_node_t *exponent = notation_build_expr(build, denominator->a);
        exponent = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &exponent);
        const TeX_model_node_t *power[] = {notation_build_literal(build, TeX_NODE_IDENTIFIER, "e"), exponent};
        const TeX_model_node_t *reciprocal = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_POWER, .child_count = 2u}, power);
        const TeX_model_node_t *parts[] = {scalar, reciprocal};
        coefficient = scalar ? notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_PRODUCT, .child_count = 2u}, parts) : reciprocal;
    } else {
        const TeX_model_node_t *fraction[] = {
            scalar ? scalar : notation_build_literal(build, TeX_NODE_NUMBER, "1"),
            notation_build_expr(build, denominator)
        };
        coefficient = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_FRACTION, .child_count = 2u}, fraction);
    }
    expr_free(factors.coefficient);
    const TeX_model_node_t *product[] = {coefficient, factors.calculus};
    const TeX_model_node_t *result = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_PRODUCT, .child_count = 2u}, product);
    return factors.negative ? notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &result) : result;
}
