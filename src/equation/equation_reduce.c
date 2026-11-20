/**
 * @file equation_reduce.c
 * @brief Reduce exact numerical offsets and common scales in equation presentation.
 *
 * Separates rational addends and coefficients from a bounded additive side, moves its constant offset
 * across the equality and removes a common numerical scale. Two-term relations with a factored product
 * retain that product on one side, clear rational denominators and factor the other side's integer content.
 * Otherwise shared numerical denominators become
 * separate reduced fractions. Affine shifts expose their constant contributions while dependent corrections
 * retain factored numerators and follow the principal terms. Only proven non-zero rational factors
 * are divided out. Symbolic denominators, domains and the original equation remain unchanged.
 */

#include <stdbool.h>
#include <stddef.h>

#include "expression-t.h"
#include "number-t.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"

typedef struct {
    expr_t *expression;
    number_t coefficient;
} relation_term_t;

typedef struct {
    relation_term_t terms[64];
    size_t count;
    number_t offset;
} relation_sum_t;

static bool relation_rational(const expr_t *expression, number_t *value)
{
    return expr_match_const_value(expression, value) && num_is_exact(*value) &&
           num_is_finite(*value) && num_is_real(*value);
}

static bool relation_linear_term(const expr_t *expression)
{
    if (expr_is_variable(expression))
        return true;
    number_t coefficient = NUM_NAN;
    const expr_t *base = NULL;
    bool linear = expr_match_scaled_expr(expression, &coefficient, &base) && expr_is_variable(base) &&
                  num_is_exact(coefficient) && num_is_real(coefficient) && !num_is_zero(coefficient);
    num_destroy(&coefficient);
    return linear;
}

static bool relation_linear_shift(const expr_t *expression, const expr_t **linear,
                                   number_t *offset, bool *negative)
{
    const expr_t *left = NULL, *right = NULL;
    bool subtract = false;
    if (!expr_match_add_sub_expr(expression, &left, &right, &subtract))
        return false;
    if (relation_linear_term(left) && relation_rational(right, offset)) {
        *linear = left;
        *negative = false;
        if (subtract) {
            number_t value = num_neg(*offset);
            num_destroy(offset);
            *offset = value;
        }
        return true;
    }
    if (relation_rational(left, offset) && relation_linear_term(right)) {
        *linear = right;
        *negative = subtract;
        return true;
    }
    return false;
}

static bool relation_collect(const expr_t *expression, number_t scale, relation_sum_t *sum, unsigned depth,
                               bool expose_shifts)
{
    if (!expression || depth > 32u)
        return false;
    number_t scalar = NUM_NAN;
    const expr_t *left = NULL, *right = NULL;
    bool subtract = false, ok = true;
    if (relation_rational(expression, &scalar)) {
        number_t contribution = num_mul(scale, scalar), next = num_add(sum->offset, contribution);
        num_destroy(&sum->offset);
        sum->offset = next;
        num_destroy(&contribution);
    } else if (expr_match_add_sub_expr(expression, &left, &right, &subtract)) {
        number_t right_scale = subtract ? num_neg(scale) : num_clone(scale);
        ok = relation_collect(left, scale, sum, depth + 1u, expose_shifts) &&
             relation_collect(right, right_scale, sum, depth + 1u, expose_shifts);
        num_destroy(&right_scale);
    } else if (expr_match_neg_expr(expression, &left)) {
        number_t negative = num_neg(scale);
        ok = relation_collect(left, negative, sum, depth + 1u, expose_shifts);
        num_destroy(&negative);
    } else {
        bool product = expr_match_mul_expr(expression, &left, &right);
        bool quotient = !product && expr_match_div_expr(expression, &left, &right);
        const expr_t *body = NULL;
        if (product && relation_rational(left, &scalar) && !num_is_zero(scalar))
            body = right;
        else if ((product || quotient) && relation_rational(right, &scalar) && !num_is_zero(scalar))
            body = left;
        if (body) {
            number_t next_scale = quotient ? num_div(scale, scalar) : num_mul(scale, scalar);
            ok = relation_collect(body, next_scale, sum, depth + 1u, expose_shifts);
            num_destroy(&next_scale);
        } else if (product && expose_shifts) {
            const expr_t *linear = NULL, *other = right;
            bool negative = false;
            bool shifted = relation_linear_shift(left, &linear, &scalar, &negative);
            if (!shifted) {
                shifted = relation_linear_shift(right, &linear, &scalar, &negative);
                other = left;
            }
            if (shifted) {
                number_t constant_scale = num_mul(scale, scalar);
                number_t dependent_scale = negative ? num_neg(scale) : num_clone(scale);
                expr_t *correction = expr_mul(linear, other);
                ok = correction && relation_collect(other, constant_scale, sum, depth + 1u, expose_shifts) &&
                     sum->count < sizeof sum->terms / sizeof *sum->terms;
                if (ok)
                    sum->terms[sum->count++] = (relation_term_t){expr_retain_expr(correction),
                                                                num_clone(dependent_scale)};
                num_destroy(&dependent_scale);
                num_destroy(&constant_scale);
                expr_free(correction);
            } else if (sum->count < sizeof sum->terms / sizeof *sum->terms)
                sum->terms[sum->count++] = (relation_term_t){expr_retain_expr(expression), num_clone(scale)};
            else
                ok = false;
        } else if (sum->count < sizeof sum->terms / sizeof *sum->terms) {
            sum->terms[sum->count++] = (relation_term_t){expr_retain_expr(expression), num_clone(scale)};
        } else
            ok = false;
    }
    num_destroy(&scalar);
    return ok;
}

static bool relation_factored_sum(const expr_t *expression, unsigned depth)
{
    const expr_t *left = NULL, *right = NULL;
    bool subtract = false;
    if (depth > 32u)
        return false;
    if (expr_match_add_sub_expr(expression, &left, &right, &subtract))
        return true;
    if (expr_match_mul_expr(expression, &left, &right))
        return relation_factored_sum(left, depth + 1u) || relation_factored_sum(right, depth + 1u);
    return false;
}

static number_t relation_common_scale(const relation_sum_t *sum)
{
    number_t first = num_abs(sum->terms[0].coefficient), common = num_clone(first);
    bool uniform = true, integers = num_is_integer(first);
    /* At most sixty-four additive terms; retain ordinary fractions when their coefficients differ. */
    for (size_t i = 1u; i < sum->count; ++i) {
        number_t magnitude = num_abs(sum->terms[i].coefficient);
        uniform = uniform && num_eq(magnitude, first);
        integers = integers && num_is_integer(magnitude);
        if (integers) {
            number_t divisor = num_gcd(common, magnitude);
            num_destroy(&common);
            common = divisor;
        }
        num_destroy(&magnitude);
    }
    number_t result = num_clone(uniform ? first : integers ? common : NUM_ONE);
    num_destroy(&common);
    num_destroy(&first);
    return result;
}

static bool relation_shared_denominator(const expr_t *expression)
{
    number_t scale = NUM_NAN;
    const expr_t *body = NULL, *left = NULL, *right = NULL;
    bool subtract = false;
    bool shared = expr_match_scaled_expr(expression, &scale, &body) && num_is_exact(scale) &&
                  num_is_real(scale) && !num_is_integer(scale) &&
                  expr_match_add_sub_expr(body, &left, &right, &subtract);
    num_destroy(&scale);
    return shared;
}

/* Keep each exact coefficient reduced without recreating a denominator shared by unrelated summands. */
static expr_t *relation_fraction_term(const expr_t *expression, number_t coefficient)
{
    number_t numerator = NUM_NAN, denominator = NUM_NAN;
    if (!num_get_fraction_parts(coefficient, &numerator, &denominator)) {
        num_destroy(&denominator);
        num_destroy(&numerator);
        return NULL;
    }
    expr_t *top = num_is_one(numerator) ? expr_retain_expr(expression) : expr_mul_num(expression, &numerator);
    expr_t *bottom = num_is_one(denominator) ? NULL : expr_new_const(denominator);
    expr_t *result = num_is_one(denominator) ? expr_retain_expr(top) :
                        (top && bottom ? expr_div(top, bottom) : NULL);
    expr_free(bottom);
    expr_free(top);
    num_destroy(&denominator);
    num_destroy(&numerator);
    return result;
}

static void relation_sum_clear(relation_sum_t *sum)
{
    num_destroy(&sum->offset);
    for (size_t i = 0u; i < sum->count; ++i) {
        num_destroy(&sum->terms[i].coefficient);
        expr_free(sum->terms[i].expression);
    }
    sum->count = 0u;
}

/* Retain a product and move its companion term across the equality, using only exact integer content. */
static bool relation_factored_pair(const relation_sum_t *sum, number_t target,
                                    expr_t **product_out, expr_t **remainder_out)
{
    if (sum->count != 2u)
        return false;
    bool first = relation_factored_sum(sum->terms[0].expression, 0u);
    bool second = relation_factored_sum(sum->terms[1].expression, 0u);
    if (first == second)
        return false;
    size_t product = first ? 0u : 1u, other = 1u - product;
    number_t shifted = num_sub(target, sum->offset);
    number_t coefficients[] = {sum->terms[product].coefficient, sum->terms[other].coefficient, shifted};
    number_t values[3] = {NUM_NAN, NUM_NAN, NUM_NAN};
    number_t denominator = num_clone(NUM_ONE), content = NUM_NAN, right_content = NUM_NAN;
    expr_t *left = NULL, *right = NULL, *centre = NULL, *term = NULL, *difference = NULL;
    bool ok = false;
    for (size_t i = 0u; i < 3u; ++i) {
        number_t numerator = NUM_NAN, bottom = NUM_NAN;
        bool rational = num_is_exact(coefficients[i]) && num_is_finite(coefficients[i]) &&
                        num_is_real(coefficients[i]) &&
                        num_get_fraction_parts(coefficients[i], &numerator, &bottom);
        number_t next = rational ? num_lcm(denominator, bottom) : num_clone(NUM_NAN);
        num_destroy(&bottom);
        num_destroy(&numerator);
        num_destroy(&denominator);
        denominator = next;
        if (!num_is_finite(denominator) || num_is_zero(denominator))
            goto cleanup;
    }
    for (size_t i = 0u; i < 3u; ++i) {
        values[i] = num_mul(coefficients[i], denominator);
        if (!num_is_finite(values[i]) || !num_is_integer(values[i]))
            goto cleanup;
        number_t divisor = i ? num_gcd(content, values[i]) : num_abs(values[i]);
        num_destroy(&content);
        content = divisor;
    }
    if (num_is_zero(values[0]) || num_is_zero(values[1]) || num_is_zero(content))
        goto cleanup;
    bool negative = num_lt(values[0], NUM_ZERO);
    for (size_t i = 0u; i < 3u; ++i) {
        number_t reduced = num_div(values[i], content);
        num_destroy(&values[i]);
        values[i] = negative ? num_neg(reduced) : num_clone(reduced);
        num_destroy(&reduced);
        if (!num_is_finite(values[i]) || !num_is_integer(values[i]))
            goto cleanup;
    }
    right_content = num_gcd(values[1], values[2]);
    if (!num_is_finite(right_content) || num_is_zero(right_content))
        goto cleanup;
    number_t constant = num_div(values[2], right_content), coefficient = num_div(values[1], right_content);
    centre = num_is_finite(constant) ? expr_new_const(constant) : NULL;
    term = num_is_finite(coefficient) ? relation_fraction_term(sum->terms[other].expression, coefficient) : NULL;
    difference = centre && term ? (num_is_zero(constant) ? expr_neg(term) : expr_sub(centre, term)) : NULL;
    num_destroy(&coefficient);
    num_destroy(&constant);
    left = relation_fraction_term(sum->terms[product].expression, values[0]);
    right = difference ? relation_fraction_term(difference, right_content) : NULL;
    ok = left && right;
    if (ok) {
        *product_out = left;
        *remainder_out = right;
        left = right = NULL;
    }
cleanup:
    expr_free(right);
    expr_free(left);
    expr_free(difference);
    expr_free(term);
    expr_free(centre);
    num_destroy(&right_content);
    num_destroy(&content);
    num_destroy(&denominator);
    for (size_t i = 0u; i < 3u; ++i)
        num_destroy(&values[i]);
    num_destroy(&shifted);
    return ok;
}

/* Reduce affine rational transformations of a symbolic side without cancelling unknown expressions. */
bool equ_reduce_rational_sides(expr_t **lhs, expr_t **rhs)
{
    if (!lhs || !rhs || lhs == rhs || !*lhs || !*rhs)
        return false;
    number_t target = NUM_NAN;
    bool reversed = !relation_rational(*rhs, &target);
    if (reversed && !relation_rational(*lhs, &target)) {
        num_destroy(&target);
        return false;
    }
    expr_t **body_side = reversed ? rhs : lhs, **constant_side = reversed ? lhs : rhs;
    expr_t *expanded = expr_expand_preserved_for_display(*body_side), *body = NULL, *constant = NULL;
    relation_sum_t sum = {.offset = num_clone(NUM_ZERO)};
    number_t scale = NUM_NAN;
    bool changed = false;
    if (!expanded || !relation_collect(expanded, NUM_ONE, &sum, 0u, false) || !sum.count)
        goto cleanup;
    if (relation_factored_pair(&sum, target, &body, &constant))
        goto publish;
    relation_sum_clear(&sum);
    sum.offset = num_clone(NUM_ZERO);
    if (!relation_collect(expanded, NUM_ONE, &sum, 0u, true) || !sum.count)
        goto cleanup;
    scale = relation_common_scale(&sum);
    bool separate_fractions = sum.count > 1u && relation_shared_denominator(expanded);
    if (!num_is_finite(scale) || num_is_zero(scale) ||
        (num_is_one(scale) && num_is_zero(sum.offset) && !separate_fractions))
        goto cleanup;
    /* Two bounded stable passes keep principal terms before factored dependent corrections. */
    for (size_t slot = 0u; slot < 2u * sum.count; ++slot) {
        size_t i = slot % sum.count;
        if (relation_factored_sum(sum.terms[i].expression, 0u) != (slot >= sum.count))
            continue;
        number_t coefficient = num_div(sum.terms[i].coefficient, scale);
        if (!num_is_finite(coefficient) || !num_is_exact(coefficient)) {
            num_destroy(&coefficient);
            goto cleanup;
        }
        expr_t *term = relation_fraction_term(sum.terms[i].expression, coefficient);
        expr_t *next = body && term ? expr_add(body, term) : expr_retain_expr(term);
        num_destroy(&coefficient);
        expr_free(term);
        expr_free(body);
        body = next;
        if (!body)
            goto cleanup;
    }
    number_t shifted = num_sub(target, sum.offset), reduced = num_div(shifted, scale);
    constant = num_is_finite(reduced) && num_is_exact(reduced) ? expr_new_const(reduced) : NULL;
    num_destroy(&reduced);
    num_destroy(&shifted);
    /* The summands are individually reduced; simplifying their sum would recreate the common denominator. */
publish:
    expr_t *linked = body ? expr_clone_linked_symbols(body, *body_side) : NULL;
    expr_free(body);
    body = linked;
    linked = constant ? expr_clone_linked_symbols(constant, *body_side) : NULL;
    expr_free(constant);
    constant = linked;
    if (!body || !constant)
        goto cleanup;
    expr_free(*body_side);
    expr_free(*constant_side);
    *body_side = body;
    *constant_side = constant;
    body = constant = NULL;
    changed = true;
cleanup:
    num_destroy(&scale);
    num_destroy(&target);
    relation_sum_clear(&sum);
    expr_free(constant);
    expr_free(body);
    expr_free(expanded);
    return changed;
}
