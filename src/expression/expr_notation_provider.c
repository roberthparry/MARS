/**
 * @file expr_notation_provider.c
 * @brief Prepare expression meaning for callback-driven mathematical rendering.
 *
 * Native operation recognition, argument lists and exact numeric decomposition
 * remain here. The snapshot contains semantic nodes and plain names or numbers;
 * no TeX command strings or native object layouts cross the provider boundary.
 */
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "dictionary-t.h"
#include "expression-t.h"
#include "TeX-t.h"

#define MARS_SHARED_EXPR_NOTATION_INTERNAL_ACCESS
#include "internal/expr_notation_internal.h"
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

struct _notation_build_t {
    TeX_model_t *model;
    dictionary_t *expressions;
    size_t depth;
    size_t anonymous_count;
    const expr_t *shift_centre;
    const expr_t *detached_distribution;
    const expr_t *distribution_domain;
    bool partial;
    bool dependent_variables;
    bool source_order;
    bool resolve_transforms;
};

typedef struct {
    const expr_t *expression;
    const expr_t *centre;
    bool partial;
    bool dependent_variables;
    bool source_order;
    bool resolve_transforms;
} notation_expression_key_t;

static size_t expression_hash(const void *key)
{
    const notation_expression_key_t *item = key;
    return (size_t)(((uintptr_t)item->expression >> 3u) ^ ((uintptr_t)item->centre >> 4u) ^
                    (item->partial ? 1u : 0u) ^ (item->dependent_variables ? 2u : 0u) ^
                    (item->source_order ? 4u : 0u) ^ (item->resolve_transforms ? 8u : 0u));
}

static int expression_compare(const void *left, const void *right)
{
    const notation_expression_key_t *a = left, *b = right;
    if (a->expression != b->expression)
        return (uintptr_t)a->expression < (uintptr_t)b->expression ? -1 : 1;
    if (a->centre != b->centre)
        return (uintptr_t)a->centre < (uintptr_t)b->centre ? -1 : 1;
    if (a->partial != b->partial)
        return a->partial ? 1 : -1;
    if (a->dependent_variables != b->dependent_variables)
        return a->dependent_variables ? 1 : -1;
    if (a->source_order != b->source_order)
        return a->source_order ? 1 : -1;
    return a->resolve_transforms == b->resolve_transforms ? 0 : a->resolve_transforms ? 1 : -1;
}

static void expression_key_clone(void *destination, const void *source)
{
    const notation_expression_key_t *item = source;
    *(notation_expression_key_t *)destination = *item;
    expr_retain(item->expression);
    if (item->centre)
        expr_retain(item->centre);
}

static void expression_key_destroy(void *key)
{
    notation_expression_key_t *item = key;
    expr_free((expr_t *)item->expression);
    expr_free((expr_t *)item->centre);
}

/* Allocate one native memoisation dictionary for the complete snapshot. */
notation_build_t *notation_build_new(void)
{
    notation_build_t *build = calloc(1u, sizeof *build);
    if (!build)
        return NULL;
    build->partial = true;
    build->model = TeX_model_new();
    build->expressions = dictionary_create(sizeof(notation_expression_key_t), sizeof(TeX_model_node_t *), expression_hash,
                                            expression_compare, expression_key_clone, expression_key_destroy,
                                            NULL, NULL, NULL);
    if (!build->model || !build->expressions) {
        notation_build_free(build);
        return NULL;
    }
    return build;
}

/* The owning mathematical module chooses ordinary or partial differentiation. */
void notation_build_derivatives(notation_build_t *build, bool partial)
{
    if (build)
        build->partial = partial;
}

/* The differential-equation adapter identifies the dependent-variable interpretation. */
void notation_build_dependent_variables(notation_build_t *build, bool enabled)
{
    if (build)
        build->dependent_variables = enabled;
}

/* Authored differential equations retain their term order throughout the snapshot. */
void notation_build_source_order(notation_build_t *build, bool enabled)
{
    if (build)
        build->source_order = enabled;
}

/* Render evaluated transforms and their distribution qualifications as results. */
void notation_build_result_mode(notation_build_t *build, bool enabled)
{
    if (build)
        build->resolve_transforms = enabled;
}

/* Keep formal input and evaluated-result snapshots distinct. */
bool expr_notation_results_enabled(const notation_build_t *build)
{
    return build && build->resolve_transforms;
}

/* A detached qualification belongs only to the selected root domain. */
const expr_t *expr_notation_domain_distribution(const notation_build_t *build, const expr_t *expression)
{
    return build && build->distribution_domain == expression ? build->detached_distribution : NULL;
}

/* Release retained preparation keys and any snapshot not transferred to its context. */
void notation_build_free(notation_build_t *build)
{
    if (build) {
        dictionary_destroy(build->expressions);
        TeX_model_free(build->model);
        free(build);
    }
}

/* Copy a semantic descriptor into the immutable snapshot. */
const TeX_model_node_t *notation_build_node(notation_build_t *build, TeX_node_view_t view,
                                            const TeX_model_node_t *const *children)
{
    return build ? TeX_model_add(build->model, view, children) : NULL;
}

/* Copy an atom's plain semantic spelling without transferring the caller's string. */
const TeX_model_node_t *notation_build_literal(notation_build_t *build, TeX_node_kind_t kind, const char *text)
{
    string_t *value = text ? string_new_with(text) : NULL;
    const TeX_model_node_t *result = value
        ? notation_build_node(build, (TeX_node_view_t){.kind = kind, .text = value}, NULL) : NULL;
    string_free(value);
    return result;
}

/* Recover small exactly representable dyadic coefficients from complex storage. */
static const TeX_model_node_t *notation_build_imaginary_number(notation_build_t *build, const number_t value)
{
    if (num_is_finite(value) && !num_is_exact(value)) {
        double approximate = num_to_double(value);
        for (long denominator = 2; denominator <= 64; denominator *= 2) {
            double scaled = approximate * (double)denominator;
            if (!isfinite(scaled) || fabs(scaled) > 1000000 || trunc(scaled) != scaled)
                continue;
            number_t rational = num_create_from_frac((long)scaled, denominator);
            bool equal = num_eq(value, rational);
            const TeX_model_node_t *node = equal ? notation_build_number(build, rational) : NULL;
            num_destroy(&rational);
            if (equal)
                return node;
        }
    }
    return notation_build_number(build, value);
}

/* Keep exact fractions and complex components as mathematics, not formatted syntax. */
const TeX_model_node_t *notation_build_number(notation_build_t *build, const number_t value)
{
    if (num_is_nan(value))
        return notation_build_literal(build, TeX_NODE_IDENTIFIER, "NaN");
    if (num_is_inf(value))
        return notation_build_literal(build, TeX_NODE_IDENTIFIER, num_get_sign(value) < 0 ? "−∞" : "∞");
    if (!num_is_real(value)) {
        number_t real = num_real_part(value), imaginary = num_imag_part(value);
        if (num_is_zero(real)) {
            bool negative = num_get_sign(imaginary) < 0;
            number_t magnitude = negative ? num_neg(imaginary) : num_clone(imaginary);
            const TeX_model_node_t *unit = notation_build_literal(build, TeX_NODE_IDENTIFIER, "i");
            const TeX_model_node_t *factor = num_eq(magnitude, NUM_ONE) ? unit : notation_build_node(build,
                (TeX_node_view_t){.kind = TeX_NODE_PRODUCT, .child_count = 2u},
                (const TeX_model_node_t *const[]){notation_build_imaginary_number(build, magnitude), unit});
            const TeX_model_node_t *result = negative ? notation_build_node(build,
                (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &factor) : factor;
            num_destroy(&magnitude);
            num_destroy(&real);
            num_destroy(&imaginary);
            return result;
        }
        const TeX_model_node_t *imaginary_parts[] = {
            notation_build_imaginary_number(build, imaginary), notation_build_literal(build, TeX_NODE_IDENTIFIER, "i")
        };
        const TeX_model_node_t *sum[] = {
            notation_build_number(build, real),
            notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_PRODUCT, .child_count = 2u}, imaginary_parts)
        };
        num_destroy(&real);
        num_destroy(&imaginary);
        return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_SUM, .child_count = 2u}, sum);
    }
    if (num_get_sign(value) < 0) {
        number_t magnitude = num_abs(value);
        const TeX_model_node_t *positive = notation_build_number(build, magnitude);
        num_destroy(&magnitude);
        return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &positive);
    }
    number_t numerator = num_new(), denominator = num_new();
    bool fraction = num_get_fraction_parts(value, &numerator, &denominator) && !num_eq(denominator, NUM_ONE);
    if (fraction) {
        const TeX_model_node_t *parts[] = {
            notation_build_number(build, numerator), notation_build_number(build, denominator)
        };
        num_destroy(&numerator);
        num_destroy(&denominator);
        return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_FRACTION, .child_count = 2u}, parts);
    }
    num_destroy(&numerator);
    num_destroy(&denominator);
    string_t *text = num_to_string(value);
    const TeX_model_node_t *result = text
        ? notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_NUMBER, .text = text}, NULL) : NULL;
    string_free(text);
    return result;
}

static const TeX_node_kind_t binary_kinds[EXPR_KIND_COUNT] = {
    [EXPR_KIND_ADD] = TeX_NODE_SUM,
    [EXPR_KIND_SUB] = TeX_NODE_DIFFERENCE,
    [EXPR_KIND_MUL] = TeX_NODE_PRODUCT,
    [EXPR_KIND_DIV] = TeX_NODE_FRACTION,
    [EXPR_KIND_POW] = TeX_NODE_POWER,
    [EXPR_KIND_ROOT] = TeX_NODE_ROOT,
    [EXPR_KIND_INDEXED_SYMBOL] = TeX_NODE_SUBSCRIPT
};

/* Share mathematical operation interpretation with exact binding-value snapshots. */
const TeX_model_node_t *expr_notation_operation(notation_build_t *build, const expr_ops_t *operation,
                                               const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    if (!operation || !left || (unsigned)operation->kind >= EXPR_KIND_COUNT)
        return NULL;
    expr_op_kind_t kind = operation->kind;
    const TeX_model_node_t *children[] = {left, right};
    if (binary_kinds[kind])
        return notation_build_node(build,
            (TeX_node_view_t){.kind = binary_kinds[kind], .child_count = 2u}, children);
    if (kind == EXPR_KIND_NEG || kind == EXPR_KIND_SQRT)
        return notation_build_node(build, (TeX_node_view_t){
            .kind = kind == EXPR_KIND_NEG ? TeX_NODE_NEGATIVE : TeX_NODE_ROOT, .child_count = 1u}, children);
    if (kind == EXPR_KIND_CUBRT) {
        children[1] = notation_build_literal(build, TeX_NODE_NUMBER, "3");
        return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_ROOT, .child_count = 2u}, children);
    }
    if (kind == EXPR_KIND_ABS || kind == EXPR_KIND_FLOOR || kind == EXPR_KIND_CEIL) {
        TeX_fence_t fence = kind == EXPR_KIND_ABS ? TeX_FENCE_ABSOLUTE
            : kind == EXPR_KIND_FLOOR ? TeX_FENCE_FLOOR : TeX_FENCE_CEILING;
        return notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_FENCED, .fence = fence, .child_count = 1u}, children);
    }
    if (kind == EXPR_KIND_EXP) {
        children[0] = notation_build_literal(build, TeX_NODE_IDENTIFIER, "e");
        children[1] = left;
        return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_POWER, .child_count = 2u}, children);
    }
    if (kind == EXPR_KIND_POLYGAMMA) {
        const TeX_model_node_t *parts[] = {notation_build_literal(build, TeX_NODE_IDENTIFIER, "ψ"), left, right};
        return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_FUNCTION_ORDER, .child_count = 3u}, parts);
    }
    static const bool indexed[EXPR_KIND_COUNT] = {
        [EXPR_KIND_QDIGAMMA]      = true,
        [EXPR_KIND_POLYLOG]       = true,
        [EXPR_KIND_LEGENDRE_CHI]  = true,
        [EXPR_KIND_HARMONIC_POLY] = true,
        [EXPR_KIND_CHEBYSHEV_T]   = true,
        [EXPR_KIND_CHEBYSHEV_U]   = true,
        [EXPR_KIND_HERMITE_H]     = true,
        [EXPR_KIND_BESSEL_J]      = true,
        [EXPR_KIND_BESSEL_Y]      = true,
        [EXPR_KIND_BESSEL_K]      = true,
        [EXPR_KIND_BESSEL_I]      = true,
        [EXPR_KIND_STRUVE_L]      = true,
        [EXPR_KIND_STRUVE_H]      = true,
        [EXPR_KIND_LAMBERT_WN]    = true
    };
    if (indexed[kind]) {
        const TeX_model_node_t *symbol[] = {
            notation_build_literal(build, TeX_NODE_FUNCTION_NAME, operation->function_name), left
        };
        children[0] = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_SUBSCRIPT, .child_count = 2u}, symbol);
        return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_APPLY, .child_count = 2u}, children);
    }
    string_t *name = operation->function_name ? string_new_with(operation->function_name) : NULL;
    const TeX_model_node_t *result = name ? notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_CALL, .text = name, .child_count = right ? 2u : 1u}, children) : NULL;
    string_free(name);
    return result;
}

static bool collect_arguments(notation_build_t *build, const expr_t *argument,
                              const TeX_model_node_t **children, size_t *count, size_t depth)
{
    if (!argument || depth >= 256u || *count >= 256u)
        return false;
    if (argument->ops->kind == EXPR_KIND_ARGUMENT_LIST)
        return collect_arguments(build, argument->a, children, count, depth + 1u) &&
               collect_arguments(build, argument->b, children, count, depth + 1u);
    children[*count] = notation_build_expr(build, argument);
    return children[(*count)++] != NULL;
}

/* Split the native trailing subscript digits into their mathematical index. */
const TeX_model_node_t *expr_notation_name(notation_build_t *build, const char *name)
{
    if (!build || !name)
        return NULL;
    const unsigned char *start = (const unsigned char *)name;
    const unsigned char *p = start;
    while (*p && !(p[0] == 0xe2u && p[1] && p[2] && p[1] == 0x82u && p[2] >= 0x80u && p[2] <= 0x89u))
        ++p;
    if (!*p || p == start || p - start > 1)
        return notation_build_literal(build, TeX_NODE_IDENTIFIER, name);
    const unsigned char *digits = p;
    while (p[0] == 0xe2u && p[1] && p[2] && p[1] == 0x82u && p[2] >= 0x80u && p[2] <= 0x89u)
        p += 3;
    if (*p)
        return notation_build_literal(build, TeX_NODE_IDENTIFIER, name);
    string_t *base = string_new(), *index = string_new();
    bool valid = base && index && string_append_bytes(base, name, (size_t)(digits - start)) == 0;
    for (p = digits; valid && *p; p += 3)
        valid = string_append_char(index, (char)('0' + p[2] - 0x80u)) == 0;
    const TeX_model_node_t *children[] = {
        valid ? notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_IDENTIFIER, .text = base}, NULL) : NULL,
        valid ? notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_NUMBER, .text = index}, NULL) : NULL
    };
    const TeX_model_node_t *result = valid ? notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_SUBSCRIPT, .child_count = 2u}, children) : NULL;
    string_free(index); string_free(base);
    return result;
}

static const TeX_model_node_t *build_expression(notation_build_t *build, const expr_t *expression)
{
    if (expression == build->detached_distribution)
        return notation_build_expr(build, expression->a);
    expr_op_kind_t kind = expression->ops->kind;
    if (build->resolve_transforms && (kind == EXPR_KIND_PRINCIPAL_VALUE || kind == EXPR_KIND_FINITE_PART)) {
        const TeX_model_node_t *operand = notation_build_expr(build, expression->a);
        bool half_line = expr_is_half_line_finite_part(expression);
        string_t *caption = half_line ? NULL : string_new_with(kind == EXPR_KIND_PRINCIPAL_VALUE
            ? "principal value" : "finite part");
        const TeX_model_node_t *result = half_line || caption ? notation_build_node(build, (TeX_node_view_t){
            .kind = half_line ? TeX_NODE_FINITE_PART : TeX_NODE_DISTRIBUTION_INLINE,
            .text = caption, .child_count = 1u}, &operand) : NULL;
        string_free(caption);
        return result;
    }
    if (kind == EXPR_KIND_REAL_DOMAIN)
        return expr_notation_domain(build, expression);
    if (kind == EXPR_KIND_CONST && expr_tostring_should_emit_binding_expr(expression))
        return expr_notation_binding(build, expression->binding_expr);
    if (kind == EXPR_KIND_VAR || (kind == EXPR_KIND_CONST && expression->name)) {
        if (!expression->name) {
            string_t *index = string_sprintf("%zu", build->anonymous_count++);
            const TeX_model_node_t *parts[] = {
                notation_build_literal(build, TeX_NODE_IDENTIFIER, "x"),
                index ? notation_build_node(build,
                    (TeX_node_view_t){.kind = TeX_NODE_NUMBER, .text = index}, NULL) : NULL
            };
            string_free(index);
            return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_SUBSCRIPT, .child_count = 2u}, parts);
        }
        return expr_notation_name(build, expression->name);
    }
    if (kind == EXPR_KIND_CONST)
        return notation_build_number(build, expression->c);
    if (kind == EXPR_KIND_EXP && expr_is_const(expression->a)) {
        long numerator = 0, denominator = 0;
        if (num_get_small_rational(expression->a->c, &numerator, &denominator) && numerator == 1 &&
            denominator > 1) {
            number_t degree = num_create_from_long(denominator);
            const TeX_model_node_t *children[] = {
                notation_build_literal(build, TeX_NODE_IDENTIFIER, "e"), notation_build_number(build, degree)
            };
            num_destroy(&degree);
            return notation_build_node(build, (TeX_node_view_t){
                .kind = TeX_NODE_ROOT, .child_count = denominator == 2 ? 1u : 2u}, children);
        }
    }
    if (kind == EXPR_KIND_CONJ || kind == EXPR_KIND_REAL_BOUND || kind == EXPR_KIND_IMAG_COORDINATE) {
        const TeX_model_node_t *child = notation_build_expr(build, expression->a);
        TeX_node_kind_t presentation = kind == EXPR_KIND_CONJ ? TeX_NODE_CONJUGATE
            : kind == EXPR_KIND_REAL_BOUND ? TeX_NODE_REAL_PROJECTION : TeX_NODE_IMAG_PROJECTION;
        return notation_build_node(build, (TeX_node_view_t){.kind = presentation, .child_count = 1u}, &child);
    }
    if (kind == EXPR_KIND_NEG || kind == EXPR_KIND_SQRT) {
        const TeX_model_node_t *children[] = {notation_build_expr(build, expression->a)};
        return notation_build_node(build, (TeX_node_view_t){
            .kind = kind == EXPR_KIND_NEG ? TeX_NODE_NEGATIVE : TeX_NODE_ROOT, .child_count = 1u}, children);
    }
    if ((kind == EXPR_KIND_POW_D || kind == EXPR_KIND_POW) &&
        (kind == EXPR_KIND_POW_D || (expr_is_const(expression->b) && !expression->b->name))) {
        number_t exponent = kind == EXPR_KIND_POW_D ? expression->c : expression->b->c;
        long numerator = 0, denominator = 0;
        if (num_get_small_rational(exponent, &numerator, &denominator) && numerator == 1 && denominator > 1) {
            number_t degree = num_create_from_long(denominator);
            const TeX_model_node_t *children[] = {
                notation_build_expr(build, expression->a), notation_build_number(build, degree)
            };
            num_destroy(&degree);
            return notation_build_node(build, (TeX_node_view_t){
                .kind = TeX_NODE_ROOT, .child_count = denominator == 2 ? 1u : 2u}, children);
        }
    }
    if (kind == EXPR_KIND_POW_D || kind == EXPR_KIND_CUBRT) {
        const TeX_model_node_t *base = NULL;
        if (kind == EXPR_KIND_POW_D && expr_is_const(expression->a) && !expression->a->name &&
            !num_is_real(expression->a->c)) {
            number_t real = num_real_part(expression->a->c), imaginary = num_imag_part(expression->a->c);
            if (!num_is_zero(real)) {
                const TeX_model_node_t *parts[] = {
                    notation_build_number(build, real), notation_build_number(build, imaginary)
                };
                base = notation_build_node(build,
                    (TeX_node_view_t){.kind = TeX_NODE_COMPLEX_LITERAL, .child_count = 2u}, parts);
            }
            num_destroy(&imaginary);
            num_destroy(&real);
        }
        const TeX_model_node_t *children[] = {
            base ? base : notation_build_expr(build, expression->a),
            kind == EXPR_KIND_POW_D ? notation_build_number(build, expression->c)
                                    : notation_build_literal(build, TeX_NODE_NUMBER, "3")
        };
        return notation_build_node(build, (TeX_node_view_t){
            .kind = kind == EXPR_KIND_POW_D ? TeX_NODE_POWER : TeX_NODE_ROOT, .child_count = 2u}, children);
    }
    if ((unsigned)kind < EXPR_KIND_COUNT && binary_kinds[kind]) {
        if (kind == EXPR_KIND_ADD || kind == EXPR_KIND_SUB) {
            const expr_t *centre = NULL, *coefficient = NULL;
            bool centre_negative = false, coefficient_negative = false;
            if (expr_display_complex_shift(expression, &centre, &coefficient,
                                           &centre_negative, &coefficient_negative)) {
                const TeX_model_node_t *real = notation_build_expr(build, centre);
                if (centre_negative)
                    real = notation_build_node(build,
                        (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &real);
                const TeX_model_node_t *imaginary = notation_build_expr(build, coefficient);
                imaginary = notation_build_node(build,
                    (TeX_node_view_t){.kind = TeX_NODE_IMAGINARY_DISPLACEMENT, .child_count = 1u}, &imaginary);
                const TeX_model_node_t *parts[] = {real, imaginary};
                return notation_build_node(build, (TeX_node_view_t){
                    .kind = coefficient_negative ? TeX_NODE_DIFFERENCE : TeX_NODE_SUM, .child_count = 2u}, parts);
            }
            const TeX_model_node_t *conjugates = expr_notation_conjugate_sum(build, expression);
            if (conjugates)
                return conjugates;
            const TeX_model_node_t *ordered = build->source_order ? NULL : expr_notation_ordered_sum(build, expression);
            if (ordered)
                return ordered;
        }
        if (kind == EXPR_KIND_MUL)
            return expr_notation_product(build, expression);
        if (kind == EXPR_KIND_DIV) {
            const TeX_model_node_t *inverse_exponential = expr_notation_inverse_exponential(build, expression);
            if (inverse_exponential)
                return inverse_exponential;
            const TeX_model_node_t *calculus = expr_notation_calculus_quotient(build, expression);
            if (calculus)
                return calculus;
            const expr_t *magnitude = NULL;
            if (expr_match_neg_expr(expression->a, &magnitude)) {
                const TeX_model_node_t *parts[] = {notation_build_expr(build, magnitude),
                                                   notation_build_expr(build, expression->b)};
                const TeX_model_node_t *fraction = notation_build_node(build,
                    (TeX_node_view_t){.kind = TeX_NODE_FRACTION, .child_count = 2u}, parts);
                return notation_build_node(build,
                    (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &fraction);
            }
            const TeX_model_node_t *flat = expr_notation_flat_fraction(build, expression);
            if (flat)
                return flat;
        }
        const TeX_model_node_t *children[] = {
            notation_build_expr(build, expression->a), notation_build_expr(build, expression->b)
        };
        return notation_build_node(build, (TeX_node_view_t){.kind = binary_kinds[kind], .child_count = 2u}, children);
    }
    if (kind == EXPR_KIND_INTEGRAL) {
        const expr_t *lower = expr_integral_lower_bound_expr(expression);
        const expr_t *upper = expr_integral_upper_bound_expr(expression);
        const expr_t *dummy = expr_integral_dummy_expr(expression);
        if (!lower && expr_struct_eq(upper, dummy))
            upper = NULL;
        const TeX_model_node_t *children[] = {
            notation_build_expr(build, expression->a),
            notation_build_expr(build, dummy),
            lower ? notation_build_expr(build, lower) : upper ? notation_build_expr(build, upper) : NULL,
            upper ? notation_build_expr(build, upper) : NULL
        };
        if (lower && !upper) {
            errno = ENOTSUP;
            return NULL;
        }
        return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_INTEGRAL,
                                    .child_count = lower ? 4u : upper ? 3u : 2u}, children);
    }
    if (kind == EXPR_KIND_FORMAL_DERIVATIVE && expression->formal_wrt_count) {
        const TeX_model_node_t **children = calloc(expression->formal_wrt_count + 1u, sizeof *children);
        if (!children)
            return NULL;
        children[0] = notation_build_expr(build, expression->a);
        for (size_t i = 0; i < expression->formal_wrt_count; ++i)
            children[i + 1u] = notation_build_expr(build, expression->formal_wrts[i]);
        const TeX_model_node_t *result = notation_build_node(build, (TeX_node_view_t){
            .kind = build->dependent_variables ? TeX_NODE_VARIABLE_DERIVATIVE : TeX_NODE_DERIVATIVE,
            .child_count = expression->formal_wrt_count + 1u,
            .order = (unsigned)expression->formal_wrt_count,
            .partial = build->partial && !expr_notation_is_transform_derivative(expression)}, children);
        free(children);
        return result;
    }
    if (kind == EXPR_KIND_ARBITRARY_FUNCTION) {
        const TeX_model_node_t *children[256];
        size_t count = 0u;
        if (!collect_arguments(build, expression->a, children, &count, 0u))
            return NULL;
        string_t *name = expression->name ? string_new_with(expression->name) : NULL;
        const TeX_model_node_t *result = name ? notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_CALL, .text = name, .child_count = count}, children) : NULL;
        string_free(name);
        return result;
    }
    if (kind == EXPR_KIND_ORDERED_DERIVATIVE) {
        const expr_t *function = expression->a;
        if (!function || !function->ops)
            return NULL;
        const char *name = function->ops->kind == EXPR_KIND_ARBITRARY_FUNCTION
            ? function->name : function->ops->function_name;
        const TeX_model_node_t *children[256] = {
            notation_build_literal(build, TeX_NODE_FUNCTION_NAME, name),
            notation_build_expr(build, expression->b)
        };
        size_t count = 2u;
        if (!collect_arguments(build, function->a, children, &count, 0u) ||
            (function->b && !collect_arguments(build, function->b, children, &count, 0u)))
            return NULL;
        return notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_ORDERED_DERIVATIVE, .child_count = count}, children);
    }
    if (kind == EXPR_KIND_SUMMATION || kind == EXPR_KIND_PRODUCT) {
        const expr_t *index = expression->b, *lower = NULL, *upper = NULL;
        if (expr_is_op(index, &ops_argument_list)) {
            index = expression->b->a;
            upper = expression->b->b;
            if (expr_is_op(upper, &ops_argument_list)) {
                lower = upper->a;
                upper = upper->b;
            }
        }
        const TeX_model_node_t *children[] = {
            notation_build_expr(build, expression->a), notation_build_expr(build, index),
            lower ? notation_build_expr(build, lower) : notation_build_literal(build, TeX_NODE_NUMBER, "0"),
            upper ? notation_build_expr(build, upper) : notation_build_literal(build, TeX_NODE_IDENTIFIER, "∞")
        };
        return notation_build_node(build, (TeX_node_view_t){
            .kind = kind == EXPR_KIND_SUMMATION ? TeX_NODE_SUMMATION : TeX_NODE_INDEXED_PRODUCT,
            .child_count = 4u}, children);
    }
    if (expr_is_integral_transform(expression)) {
        if (build->resolve_transforms) {
            expr_t *result = expr_transform_result(expression);
            if (result) {
                const TeX_model_node_t *node = notation_build_expr(build, result);
                expr_free(result);
                return node;
            }
        }
        static const TeX_transform_kind_t transforms[EXPR_KIND_COUNT] = {
            [EXPR_KIND_LAPLACE]         = TeX_TRANSFORM_LAPLACE,
            [EXPR_KIND_INVERSE_LAPLACE] = TeX_TRANSFORM_INVERSE_LAPLACE,
            [EXPR_KIND_FOURIER]         = TeX_TRANSFORM_FOURIER,
            [EXPR_KIND_INVERSE_FOURIER] = TeX_TRANSFORM_INVERSE_FOURIER
        };
        if (!expression->b || !expression->b->b)
            return NULL;
        const TeX_model_node_t *children[] = {
            notation_build_expr(build, expression->a),
            notation_build_expr(build, expression->b->a),
            notation_build_expr(build, expression->b->b->a)
        };
        return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_TRANSFORM,
            .child_count = 3u, .transform = transforms[kind]}, children);
    }
    if (kind == EXPR_KIND_LERCH_PHI || kind == EXPR_KIND_LOMMEL_S || kind == EXPR_KIND_APPELL_F1 ||
        kind == EXPR_KIND_LAURICELLA_F || kind == EXPR_KIND_HYPERGEOMETRIC_PFQ)
        return expr_notation_packed_function(build, expression);
    /* Packing and domain-marker nodes are interpreted only by their owning parents. */
    if (kind == EXPR_KIND_ARGUMENT_LIST ||
        kind == EXPR_KIND_LERCH_PHI_PACK || kind == EXPR_KIND_LOMMEL_S_PACK || kind == EXPR_KIND_APPELL_F1_PACK ||
        kind == EXPR_KIND_HYPERGEOMETRIC_PFQ_PACK ||
        kind == EXPR_KIND_NONNEGATIVE_INTEGER || kind == EXPR_KIND_REAL_PARAMETER) {
        errno = ENOTSUP;
        return NULL;
    }
    if (expression->ops->arity == EXPR_OP_ATOM) {
        errno = ENOTSUP;
        return NULL;
    }
    const TeX_model_node_t *left = expression->a ? notation_build_expr(build, expression->a) : NULL;
    const TeX_model_node_t *right = expression->b ? notation_build_expr(build, expression->b) : NULL;
    if (expression->b && !right)
        return NULL;
    return expr_notation_operation(build, expression->ops, left, right);
}

/* Query native structure once per shared node; no source parsing or TeX formatting occurs here. */
const TeX_model_node_t *notation_build_expr(notation_build_t *build, const expr_t *expression)
{
    if (!build || !expression || build->depth >= 256u) {
        errno = EINVAL;
        return NULL;
    }
    const expr_t *saved_centre = build->shift_centre;
    const expr_t *centre = expr_display_symmetric_shift_centre(expression);
    if (centre)
        build->shift_centre = centre;
    notation_expression_key_t key = {expression, build->shift_centre, build->partial, build->dependent_variables,
                                     build->source_order, build->resolve_transforms};
    const TeX_model_node_t *node = NULL;
    if (dictionary_get(build->expressions, &key, &node)) {
        build->shift_centre = saved_centre;
        return node;
    }
    ++build->depth;
    const expr_t *shift = NULL;
    bool subtract = false;
    if (build->shift_centre && expr_display_centred_shift_parts(expression, build->shift_centre, &shift, &subtract)) {
        const TeX_model_node_t *parts[] = {notation_build_expr(build, build->shift_centre), notation_build_expr(build, shift)};
        node = notation_build_node(build, (TeX_node_view_t){
            .kind = subtract ? TeX_NODE_DIFFERENCE : TeX_NODE_SUM, .child_count = 2u}, parts);
    } else {
        node = build_expression(build, expression);
    }
    --build->depth;
    build->shift_centre = saved_centre;
    if (node && !dictionary_set(build->expressions, &key, &node))
        return NULL;
    return node;
}

/* Interpret mathematical source in its owning module, then copy its semantic structure. */
const TeX_model_node_t *notation_build_formula(notation_build_t *build, const char *source)
{
    string_t *text = source ? string_new_with(source) : NULL;
    expr_t *expression = text ? expr_from_presentation_text_internal(text) : NULL;
    const TeX_model_node_t *result = expression ? notation_build_expr(build, expression) : NULL;
    expr_free(expression);
    string_free(text);
    return result;
}

/* Give the semantic context sole ownership of the prepared snapshot. */
TeX_context_t *notation_build_context(notation_build_t *build, const TeX_model_node_t *root)
{
    TeX_context_t *context = build && root ? TeX_model_context(build->model, root) : NULL;
    if (context)
        build->model = NULL;
    return context;
}

/* Snapshot expression meaning while keeping native objects and all ownership with their caller. */
TeX_context_t *expr_TeX_context(const expr_t *expression)
{
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t *root = notation_build_expr(build, expression);
    TeX_context_t *context = notation_build_context(build, root);
    notation_build_free(build);
    return context;
}

static bool has_qualified_distribution(const expr_t *expression)
{
    return expression && (expr_is_op(expression, &ops_principal_value) || expr_is_op(expression, &ops_finite_part) ||
                          has_qualified_distribution(expression->a) || has_qualified_distribution(expression->b));
}

/* Results resolve available transforms; formal contexts retain the authored operation. */
TeX_context_t *expr_TeX_result_context(const expr_t *expression)
{
    /* Nested qualifications still require the legacy scoped renderer. */
    bool qualified = expression && (expr_is_op(expression, &ops_principal_value) ||
                                     expr_is_op(expression, &ops_finite_part)) &&
                     !expr_is_distribution(expression->a);
    const expr_t *detached = NULL;
    if (!qualified && has_qualified_distribution(expression)) {
        if (expr_is_integral_transform(expression))
            return NULL;
        detached = expr_distribution_detachable(expression);
    }
    notation_build_t *build = notation_build_new();
    if (build) {
        build->resolve_transforms = true;
        build->detached_distribution = detached;
        build->distribution_domain = detached && expr_is_op(expression, &ops_real_domain) ? expression : NULL;
    }
    const TeX_model_node_t *root;
    if (qualified) {
        const TeX_model_node_t *operand = notation_build_expr(build, expression->a);
        bool half_line = expr_is_half_line_finite_part(expression);
        string_t *caption = half_line ? NULL : string_new_with(expr_is_op(expression, &ops_principal_value)
            ? "principal value" : "finite part");
        root = half_line || caption ? notation_build_node(build, (TeX_node_view_t){
            .kind = half_line ? TeX_NODE_FINITE_PART : TeX_NODE_DISTRIBUTION_QUALIFICATION,
            .text = caption, .child_count = 1u}, &operand) : NULL;
        string_free(caption);
    } else if (detached && !expr_is_op(expression, &ops_real_domain)) {
        const TeX_model_node_t *children[] = {
            notation_build_expr(build, expression), notation_build_expr(build, detached->a)
        };
        string_t *caption = string_new_with(expr_is_op(detached, &ops_principal_value)
            ? "principal value" : "finite part");
        root = caption ? notation_build_node(build, (TeX_node_view_t){
            .kind = TeX_NODE_DISTRIBUTION_QUALIFICATION, .text = caption, .child_count = 2u}, children) : NULL;
        string_free(caption);
    } else {
        root = notation_build_expr(build, expression);
    }
    TeX_context_t *context = notation_build_context(build, root);
    notation_build_free(build);
    return context;
}

/* Legacy string APIs render their native semantic model through the shared callback boundary. */
string_t *notation_build_render(notation_build_t *build, const TeX_model_node_t *root)
{
    TeX_context_t *context = notation_build_context(build, root);
    TeX_t *notation = context ? TeX_from_context(context) : NULL;
    string_t *text = notation ? TeX_render(notation) : NULL;
    TeX_free(notation);
    TeX_context_free(context);
    return text;
}
