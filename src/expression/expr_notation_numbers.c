/**
 * @file expr_notation_numbers.c
 * @brief Decompose native numeric values into exact mathematical notation nodes.
 *
 * Expression retains fractions, complex components and recognised constant identities as mathematics. Retained
 * number identities index a compact recipe table directly. Untagged values use a fixed perfect-hash candidate
 * lookup followed by full-precision equality. No TeX source is created or parsed in this provider.
 */
#include <math.h>

#include "expression-t.h"

#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_notation_internal.h"

typedef struct {
    const char *left;
    const char *right;
    TeX_node_kind_t operation;
    unsigned numerator;
    unsigned denominator;
    bool negative;
} numeric_root_recipe_t;

static const numeric_root_recipe_t *constant_recipe(number_const_id_t id)
{
    enum { FIRST = NUMBER_CONST_SQRT_HALF };
    static const numeric_root_recipe_t recipes[] = {
        [NUMBER_CONST_SQRT_HALF - FIRST]            = {"1", "2", TeX_NODE_FRACTION, 0, 0, false},
        [NUMBER_CONST_SQRT2 - FIRST]                = {"2", NULL, TeX_NODE_NUMBER, 0, 0, false},
        [NUMBER_CONST_SQRT3 - FIRST]                = {"3", NULL, TeX_NODE_NUMBER, 0, 0, false},
        [NUMBER_CONST_SQRT2_OVER_TWO - FIRST]       = {"1", "2", TeX_NODE_FRACTION, 0, 0, false},
        [NUMBER_CONST_SQRT3_OVER_TWO - FIRST]       = {"3", NULL, TeX_NODE_NUMBER, 0, 2, false},
        [NUMBER_CONST_SQRT_2PI - FIRST]             = {"2", "π", TeX_NODE_PRODUCT, 0, 0, false},
        [NUMBER_CONST_SQRT_PI - FIRST]              = {"π", NULL, TeX_NODE_IDENTIFIER, 0, 0, false},
        [NUMBER_CONST_SQRT_PI_OVER_TWO - FIRST]     = {"π", "2", TeX_NODE_FRACTION, 0, 0, false},
        [NUMBER_CONST_SQRT1ONPI - FIRST]            = {"π", NULL, TeX_NODE_IDENTIFIER, 1, 0, false},
        [NUMBER_CONST_2_SQRTPI - FIRST]             = {"π", NULL, TeX_NODE_IDENTIFIER, 2, 0, false},
        [NUMBER_CONST_NEG_TWO_OVER_SQRT_PI - FIRST] = {"π", NULL, TeX_NODE_IDENTIFIER, 2, 0, true},
        [NUMBER_CONST_INV_SQRT_2PI - FIRST]         = {"2", "π", TeX_NODE_PRODUCT, 1, 0, false}
    };
    unsigned index = (unsigned)id - FIRST;
    return index < sizeof recipes / sizeof *recipes ? recipes + index : NULL;
}

static const numeric_root_recipe_t *known_constant(number_t value)
{
    number_const_id_t id;
    if (number_const_id_from_immortal(&value, &id))
        return constant_recipe(id);
    static const struct { number_const_id_t id; const number_t *value; } candidates[] = {
        [0]  = {NUMBER_CONST_SQRT_PI_OVER_TWO,     &NUM_SQRT_PI_OVER_TWO},
        [1]  = {NUMBER_CONST_INV_SQRT_2PI,         &NUM_INV_SQRT_2PI},
        [2]  = {NUMBER_CONST_SQRT1ONPI,            &NUM_SQRT1ONPI},
        [3]  = {NUMBER_CONST_2_SQRTPI,             &NUM_2_SQRTPI},
        [4]  = {NUMBER_CONST_SQRT3,               &NUM_SQRT3},
        [5]  = {NUMBER_CONST_NEG_TWO_OVER_SQRT_PI, &NUM_NEG_TWO_OVER_SQRT_PI},
        [6]  = {NUMBER_CONST_SQRT3_OVER_TWO,       &NUM_SQRT3_OVER_TWO},
        [7]  = {NUMBER_CONST_SQRT2,               &NUM_SQRT2},
        [8]  = {NUMBER_CONST_SQRT_PI,             &NUM_SQRT_PI},
        [9]  = {NUMBER_CONST_SQRT_HALF,           &NUM_SQRT_HALF},
        [10] = {NUMBER_CONST_SQRT_2PI,            &NUM_SQRT_2PI}
    };
    double approximation = num_to_double(value);
    if (!isfinite(approximation) || approximation < -2.0 || approximation > 3.0)
        return NULL;
    /* Approximate bins select one candidate; equality always uses the complete native value. */
    unsigned bin = (unsigned)((approximation + 2.0) * 16.0);
    size_t index = ((bin * 61u) >> 4u) % (sizeof candidates / sizeof *candidates);
    if (num_eq(value, *candidates[index].value))
        return constant_recipe(candidates[index].id);
    if (candidates[index].id == NUMBER_CONST_SQRT_HALF && num_eq(value, NUM_SQRT2_OVER_TWO)) {
        static const numeric_root_recipe_t alternative = {"2", NULL, TeX_NODE_NUMBER, 0, 2, false};
        return &alternative;
    }
    return NULL;
}

static const TeX_model_node_t *atom(notation_build_t *build, const char *value)
{
    return notation_build_literal(build, (unsigned char)value[0] >= 128u ? TeX_NODE_IDENTIFIER : TeX_NODE_NUMBER, value);
}

static const TeX_model_node_t *pair(notation_build_t *build, TeX_node_kind_t kind,
                                    const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *parts[] = {left, right};
    return notation_build_node(build, (TeX_node_view_t){.kind = kind, .child_count = 2u}, parts);
}

static const TeX_model_node_t *root_constant(notation_build_t *build, const numeric_root_recipe_t *recipe)
{
    const TeX_model_node_t *radicand = atom(build, recipe->left);
    if (recipe->right)
        radicand = pair(build, recipe->operation, radicand, atom(build, recipe->right));
    const TeX_model_node_t *result = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_ROOT, .child_count = 1u}, &radicand);
    if (recipe->numerator)
        result = pair(build, TeX_NODE_FRACTION, atom(build, recipe->numerator == 1u ? "1" : "2"), result);
    if (recipe->denominator)
        result = pair(build, TeX_NODE_FRACTION, result, atom(build, "2"));
    if (recipe->negative)
        result = notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &result);
    return result;
}

static const TeX_model_node_t *imaginary_number(notation_build_t *build, number_t value)
{
    if (num_is_finite(value) && !num_is_exact(value)) {
        double approximate = num_to_double(value);
        /* Six possible dyadic denominators recover exactly representable small complex coefficients. */
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

/* Preserve exact fractions, complex components and constant identities as mathematics. */
const TeX_model_node_t *notation_build_number(notation_build_t *build, number_t value)
{
    if (num_is_nan(value))
        return notation_build_literal(build, TeX_NODE_IDENTIFIER, "NaN");
    if (num_is_inf(value))
        return notation_build_literal(build, TeX_NODE_IDENTIFIER, num_get_sign(value) < 0 ? "−∞" : "∞");
    if (!num_is_real(value)) {
        number_t real = num_real_part(value), imaginary = num_imag_part(value);
        const TeX_model_node_t *unit = notation_build_literal(build, TeX_NODE_IDENTIFIER, "i");
        const TeX_model_node_t *result;
        if (num_is_zero(real)) {
            bool negative = num_get_sign(imaginary) < 0;
            number_t magnitude = negative ? num_neg(imaginary) : num_clone(imaginary);
            result = num_eq(magnitude, NUM_ONE) ? unit
                : pair(build, TeX_NODE_PRODUCT, imaginary_number(build, magnitude), unit);
            if (negative)
                result = notation_build_node(build,
                    (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &result);
            num_destroy(&magnitude);
        } else {
            result = pair(build, TeX_NODE_SUM, notation_build_number(build, real),
                pair(build, TeX_NODE_PRODUCT, imaginary_number(build, imaginary), unit));
        }
        num_destroy(&real);
        num_destroy(&imaginary);
        return result;
    }
    const numeric_root_recipe_t *recipe = num_is_exact(value) ? NULL : known_constant(value);
    if (recipe)
        return root_constant(build, recipe);
    if (num_get_sign(value) < 0) {
        number_t magnitude = num_abs(value);
        const TeX_model_node_t *positive = notation_build_number(build, magnitude);
        num_destroy(&magnitude);
        return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &positive);
    }
    number_t numerator = num_new(), denominator = num_new();
    bool fraction = num_get_fraction_parts(value, &numerator, &denominator) && !num_eq(denominator, NUM_ONE);
    const TeX_model_node_t *result = fraction ? pair(build, TeX_NODE_FRACTION,
        notation_build_number(build, numerator), notation_build_number(build, denominator)) : NULL;
    num_destroy(&numerator);
    num_destroy(&denominator);
    if (fraction)
        return result;
    string_t *text = num_to_string(value);
    result = text ? notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_NUMBER, .text = text}, NULL) : NULL;
    string_free(text);
    return result;
}
