/**
 * @file equation_solution_pair.c
 * @brief Structural presentation of paired exact solution rows.
 *
 * Extracts shared reciprocal integer factors and recognises additive centres with opposite displacements,
 * including square-root radicands and identical expressions with exactly opposite real scalar coefficients.
 * Operates on borrowed trees and retains the existing temporary-binding plan;
 * neither the solver's algebra nor the numerical values are changed by presentation.
 * The equation module owns these structural decisions. TeX constructors supply
 * radicals, symmetric pairs and fractions without exposing their command syntax.
 */

#include <stdlib.h>

#include "number-t.h"
#include "string-t.h"
#include "TeX-t.h"

#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"

/* Adapt native rendered parts to independently owned notation. */
static string_t *equ_pair_TeX(const char *centre, const char *offset, const char *denominator, bool group_offset)
{
    TeX_builder_t *builder = TeX_builder_new();
    TeX_raw(builder, centre);
    TeX_raw(builder, offset);
    if (denominator)
        TeX_raw(builder, denominator);
    TeX_integer(builder, denominator != NULL);
    TeX_list(builder);
    TeX_symmetric_pair(builder, group_offset);
    TeX_t *pair = TeX_builder_finish(builder);
    string_t *out = pair ? TeX_render(pair) : NULL;
    TeX_free(pair);
    TeX_builder_free(builder);
    return out;
}

typedef struct {
    const expr_t *expr;
    bool subtract;
} equ_pair_term_t;

typedef struct {
    equ_pair_term_t terms[8];
    size_t count;
} equ_pair_sum_t;

static bool equ_pair_collect(const expr_t *expr, bool subtract, equ_pair_sum_t *sum, unsigned depth)
{
    const expr_t *left = NULL, *right = NULL;
    bool inner_subtract = false;
    if (depth > 16u)
        return false;
    if (expr_match_add_sub_expr(expr, &left, &right, &inner_subtract))
        return equ_pair_collect(left, subtract, sum, depth + 1u) &&
               equ_pair_collect(right, subtract != inner_subtract, sum, depth + 1u);
    if (sum->count == sizeof(sum->terms) / sizeof(*sum->terms))
        return false;
    sum->terms[sum->count++] = (equ_pair_term_t){expr, subtract};
    return true;
}

static bool equ_pair_opposites(equ_pair_term_t a, equ_pair_term_t b)
{
    expr_t *negative = a.subtract == b.subtract ? expr_simplify_owned(expr_neg(a.expr)) : NULL;
    bool equal = a.subtract != b.subtract ? expr_struct_eq(a.expr, b.expr) :
                                           negative && expr_struct_eq(negative, b.expr);
    expr_free(negative);
    if (!equal) {
        const expr_t *a_base = NULL, *b_base = NULL;
        number_t a_scale = num_new(), b_scale = num_new();
        if (expr_match_scaled_expr(a.expr, &a_scale, &a_base) &&
            expr_match_scaled_expr(b.expr, &b_scale, &b_base) && expr_struct_eq(a_base, b_base) &&
            num_is_exact(a_scale) && num_is_exact(b_scale) && num_is_real(a_scale) && num_is_real(b_scale) &&
            num_is_finite(a_scale) && num_is_finite(b_scale)) {
            number_t expected = a.subtract == b.subtract ? num_neg(a_scale) : num_clone(a_scale);
            equal = num_cmp(expected, b_scale) == 0;
            num_destroy(&expected);
        }
        num_destroy(&b_scale);
        num_destroy(&a_scale);
    }
    return equal;
}

static bool equ_pair_negative(const expr_t *expr, unsigned depth)
{
    if (depth > 16u)
        return false;
    const expr_t *base = NULL, *left = NULL, *right = NULL;
    number_t scale = num_clone(NUM_ONE);
    bool scalar = expr_match_scaled_expr(expr, &scale, &base) || expr_match_const_value(expr, &scale);
    bool negative = scalar && num_lt(scale, NUM_ZERO);
    num_destroy(&scale);
    if (!scalar && expr_match_mul_expr(expr, &left, &right))
        negative = equ_pair_negative(left, depth + 1u) != equ_pair_negative(right, depth + 1u);
    if (!scalar && expr_match_div_expr(expr, &left, &right))
        negative = equ_pair_negative(left, depth + 1u) != equ_pair_negative(right, depth + 1u);
    return negative;
}

static bool equ_pair_append_term(string_t *output, const expr_function_temporaries_t *plan,
                                  equ_pair_term_t term, style_t style, bool displacement)
{
    expr_t *positive = NULL;
    if (displacement && equ_pair_negative(term.expr, 0u)) {
        positive = expr_simplify_owned(expr_neg(term.expr));
        if (!positive)
            return false;
        term.expr = positive;
        term.subtract = !term.subtract;
    }
    string_t *text = expr_temporaries_math_expression(plan, term.expr, style);
    const char *sign = string_length(output) ? (term.subtract ? " - " : " + ") : (term.subtract ? "-" : "");
    bool ok = text && string_append_format(output, "%s%s", sign, string_c_str(text)) >= 0;
    string_free(text);
    expr_free(positive);
    return ok;
}

static bool equ_pair_symmetric(const expr_function_temporaries_t *plan, const expr_t *a, const expr_t *b,
                                style_t style, string_t **a_text, string_t **b_text)
{
    const expr_t *a_arg = NULL, *b_arg = NULL;
    bool roots = expr_match_sqrt_expr(a, &a_arg) && expr_match_sqrt_expr(b, &b_arg);
    equ_pair_sum_t sums[2] = {0};
    if (!equ_pair_collect(roots ? a_arg : a, false, &sums[0], 0u) ||
        !equ_pair_collect(roots ? b_arg : b, false, &sums[1], 0u) ||
        sums[0].count < 2u || sums[0].count != sums[1].count)
        return true;

    size_t common[8], common_count = 0u, unmatched_a = 0u, unmatched_b = 0u;
    bool matched[8] = {false};
    /* At most eight additive terms: bounded structural matching also preserves their authored common order. */
    for (size_t i = 0u; i < sums[0].count; ++i) {
        size_t j = 0u;
        for (; j < sums[1].count; ++j)
            if (!matched[j] && sums[0].terms[i].subtract == sums[1].terms[j].subtract &&
                expr_struct_eq(sums[0].terms[i].expr, sums[1].terms[j].expr))
                break;
        if (j == sums[1].count)
            unmatched_a = i;
        else {
            matched[j] = true;
            common[common_count++] = i;
        }
    }
    if (common_count + 1u != sums[0].count)
        return true;
    for (size_t j = 0u; j < sums[1].count; ++j)
        if (!matched[j])
            unmatched_b = j;
    if (!equ_pair_opposites(sums[0].terms[unmatched_a], sums[1].terms[unmatched_b]))
        return true;

    string_t *texts[2] = {string_new(), string_new()};
    bool ok = texts[0] && texts[1];
    for (size_t side = 0u; ok && side < 2u; ++side) {
        for (size_t i = 0u; ok && i < common_count; ++i)
            ok = equ_pair_append_term(texts[side], plan, sums[0].terms[common[i]], style, false);
        if (ok)
            ok = equ_pair_append_term(texts[side], plan,
                                     sums[side].terms[side ? unmatched_b : unmatched_a], style, true);
        if (ok && roots) {
            string_t *root;
            if (style == style_LATEX) {
                TeX_builder_t *builder = TeX_builder_new();
                TeX_raw(builder, string_c_str(texts[side]));
                TeX_sqrt(builder);
                TeX_t *notation = TeX_builder_finish(builder);
                root = notation ? TeX_render(notation) : NULL;
                TeX_free(notation);
                TeX_builder_free(builder);
            } else {
                root = string_sprintf("√(%S)", texts[side]);
            }
            string_free(texts[side]);
            texts[side] = root;
            ok = root != NULL;
        }
    }
    if (ok) {
        string_free(*a_text);
        string_free(*b_text);
        *a_text = texts[0];
        *b_text = texts[1];
    } else {
        string_free(texts[0]);
        string_free(texts[1]);
    }
    return ok;
}

static long equ_pair_denominator(const expr_t **centre, const expr_t **offset)
{
    const expr_t *centre_base = NULL, *offset_base = NULL;
    number_t centre_scale = num_new(), offset_scale = num_new();
    long numerator = 0L, denominator = 1L;
    if (expr_match_scaled_expr(*centre, &centre_scale, &centre_base) &&
        expr_match_scaled_expr(*offset, &offset_scale, &offset_base) && num_eq(centre_scale, offset_scale) &&
        num_get_small_rational(centre_scale, &numerator, &denominator) && numerator == 1L && denominator > 1L) {
        *centre = centre_base;
        *offset = offset_base;
    } else
        denominator = 1L;
    num_destroy(&offset_scale);
    num_destroy(&centre_scale);
    return denominator;
}

/* Move shared exact scalar denominators into one fraction without altering either solution branch. */
static expr_t *equ_pair_common_denominator(expr_t **centre, expr_t **offset, const expr_t *denominator)
{
    const expr_t *centre_base = NULL, *offset_base = NULL;
    number_t centre_scale = num_new(), offset_scale = num_new();
    number_t centre_top = NUM_NAN, centre_bottom = NUM_NAN, offset_top = NUM_NAN, offset_bottom = NUM_NAN;
    expr_t *result = denominator ? expr_clone(denominator) : NULL;
    if (expr_match_scaled_expr(*centre, &centre_scale, &centre_base) &&
        expr_match_scaled_expr(*offset, &offset_scale, &offset_base) &&
        num_is_exact(centre_scale) && num_is_exact(offset_scale) && num_is_real(centre_scale) && num_is_real(offset_scale) &&
        num_get_fraction_parts(centre_scale, &centre_top, &centre_bottom) &&
        num_get_fraction_parts(offset_scale, &offset_top, &offset_bottom) &&
        !num_is_one(centre_bottom) && !num_is_one(offset_bottom)) {
        number_t common = num_lcm(centre_bottom, offset_bottom);
        expr_t *scaled_centre = expr_simplify_owned(expr_mul_num(*centre, &common));
        expr_t *scaled_offset = expr_simplify_owned(expr_mul_num(*offset, &common));
        expr_t *scaled_denominator = denominator ? expr_simplify_owned(expr_mul_num(denominator, &common)) :
                                                   expr_new_const(common);
        if (scaled_centre && scaled_offset && scaled_denominator) {
            expr_free(*centre);
            expr_free(*offset);
            expr_free(result);
            *centre = scaled_centre;
            *offset = scaled_offset;
            result = scaled_denominator;
        } else {
            expr_free(scaled_denominator);
            expr_free(scaled_offset);
            expr_free(scaled_centre);
        }
        num_destroy(&common);
    }
    num_destroy(&offset_bottom); num_destroy(&offset_top);
    num_destroy(&centre_bottom); num_destroy(&centre_top);
    num_destroy(&offset_scale); num_destroy(&centre_scale);
    return result;
}

/* A variable or positive integer is already an unambiguous denominator atom. */
static bool equ_pair_denominator_needs_grouping(const expr_t *denominator)
{
    number_t value = NUM_NAN;
    bool atom = expr_is_variable(denominator) ||
                (expr_match_const_value(denominator, &value) && num_is_real(value) && num_is_finite(value) &&
                 num_is_integer(value) && num_gt(value, NUM_ZERO));
    num_destroy(&value);
    return !atom;
}

/* Compact symbolic families without changing their branch parameters or underlying solution trees. */
string_t *equ_solution_pair_rhs_text(const equation_t *first, const equation_t *second, style_t style)
{
    if (!first || !second || !expr_struct_eq(equ_lhs(first), equ_lhs(second)))
        return NULL;
    const expr_t *a = equ_rhs(first), *b = equ_rhs(second);
    const expr_t *an = NULL, *ad = NULL, *bn = NULL, *bd = NULL, *denominator = NULL;
    if (expr_match_div_expr(a, &an, &ad) && expr_match_div_expr(b, &bn, &bd) && expr_struct_eq(ad, bd)) {
        a = an;
        b = bn;
        denominator = ad;
    }
    expr_t *expanded_a = expr_display_expanded(a);
    expr_t *expanded_b = expr_display_expanded(b);
    equ_pair_sum_t sums[2] = {0};
    bool opposite = false;
    size_t matched[2] = {0};
    if (expanded_a && expanded_b && equ_pair_collect(expanded_a, false, &sums[0], 0u) &&
        equ_pair_collect(expanded_b, false, &sums[1], 0u)) {
        /* The collector bounds this structural comparison to at most eight terms per side. */
        for (size_t i = 0u; !opposite && i < sums[0].count; ++i)
            for (size_t j = 0u; !opposite && j < sums[1].count; ++j) {
                opposite = equ_pair_opposites(sums[0].terms[i], sums[1].terms[j]);
                if (opposite) {
                    matched[0] = i;
                    matched[1] = j;
                }
            }
    }
    string_t *out = NULL;
    if (opposite) {
        expr_t *rest[2] = {expr_new_const(NUM_ZERO), expr_new_const(NUM_ZERO)};
        for (size_t side = 0u; side < 2u; ++side) {
            for (size_t i = 0u; i < sums[side].count; ++i) {
                if (i == matched[side])
                    continue;
                equ_pair_term_t term = sums[side].terms[i];
                expr_t *next = term.subtract ? expr_sub(rest[side], term.expr) : expr_add(rest[side], term.expr);
                expr_free(rest[side]);
                rest[side] = expr_simplify_owned(next);
            }
        }
        expr_t *sum = expr_add(rest[0], rest[1]);
        expr_t *difference = expr_simplify_owned(expr_sub(rest[0], rest[1]));
        expr_t *half_sum = expr_div_long(sum, 2L);
        expr_t *half_difference = expr_div_long(difference, 2L);
        expr_t *centre = expr_simplify(half_sum);
        equ_pair_term_t displacement = sums[0].terms[matched[0]];
        expr_t *offset = expr_simplify_owned(displacement.subtract ? expr_sub(half_difference, displacement.expr) :
                                                                    expr_add(half_difference, displacement.expr));
        expr_t *pretty_centre = expr_beautify(centre);
        expr_t *pretty_offset = expr_beautify(offset);
        expr_t *expanded_centre = expr_display_expanded(pretty_centre);
        expr_t *expanded_offset = expr_display_expanded(pretty_offset);
        if (expanded_centre) {
            expr_free(pretty_centre);
            pretty_centre = expanded_centre;
        }
        if (expanded_offset) {
            expr_free(pretty_offset);
            pretty_offset = expanded_offset;
        }
        /* Expand authored factors before inspecting their sign, including exact trigonometric coefficients. */
        if (pretty_offset && equ_pair_negative(pretty_offset, 0u)) {
            expr_t *positive = expr_simplify_owned(expr_neg(pretty_offset));
            expr_free(pretty_offset);
            pretty_offset = positive;
        }
        expr_t *factored_centre = expr_factor_common_post_calculus(pretty_centre);
        if (factored_centre) {
            expr_free(pretty_centre);
            pretty_centre = factored_centre;
        }
        expr_t *common_denominator = equ_pair_common_denominator(&pretty_centre, &pretty_offset, denominator);
        char *centre_text = pretty_centre ? (style == style_LATEX ? expr_to_TeX_body(pretty_centre) :
                                                                  expr_to_string(pretty_centre, style)) : NULL;
        char *offset_text = pretty_offset ? (style == style_LATEX ? expr_to_TeX_body(pretty_offset) :
                                                                  expr_to_string(pretty_offset, style)) : NULL;
        char *denominator_text = common_denominator ? (style == style_LATEX ? expr_to_TeX_body(common_denominator) :
                                                                      expr_to_string(common_denominator, style)) : NULL;
        number_t centre_value = NUM_NAN;
        bool zero_centre = pretty_centre && expr_match_const_value(pretty_centre, &centre_value) &&
                           num_is_zero(centre_value);
        const expr_t *offset_left = NULL, *offset_right = NULL;
        bool offset_subtract = false;
        bool group_offset = expr_match_add_sub_expr(pretty_offset, &offset_left, &offset_right, &offset_subtract);
        bool group_denominator = common_denominator && equ_pair_denominator_needs_grouping(common_denominator);
        if (centre_text && offset_text && (!denominator || common_denominator) &&
            (!common_denominator || denominator_text)) {
            if (style == style_LATEX)
                out = equ_pair_TeX(zero_centre ? "" : centre_text, offset_text, denominator_text, group_offset);
            else if (zero_centre) {
                if (common_denominator)
                    out = string_sprintf(group_denominator ? "±(%s)/(%s)" : "±(%s)/%s", offset_text, denominator_text);
                else
                    out = string_sprintf(group_offset ? "±(%s)" : "±%s", offset_text);
            } else if (common_denominator)
                out = string_sprintf(group_denominator ? "(%s ± (%s))/(%s)" : "(%s ± (%s))/%s",
                                     centre_text, offset_text, denominator_text);
            else
                out = string_sprintf("%s ± (%s)", centre_text, offset_text);
        }
        num_destroy(&centre_value);
        expr_free(common_denominator);
        free(denominator_text);
        free(offset_text);
        free(centre_text);
        expr_free(pretty_offset);
        expr_free(pretty_centre);
        expr_free(offset);
        expr_free(centre);
        expr_free(half_difference);
        expr_free(half_sum);
        expr_free(difference);
        expr_free(sum);
        expr_free(rest[1]);
        expr_free(rest[0]);
    }
    expr_free(expanded_b);
    expr_free(expanded_a);
    return out;
}

/* Render each pair as one row, retaining full ownership of intermediate text locally. */
bool equ_solution_pairs_text(const expr_function_temporaries_t *plan, const expr_t *const *rows,
                             size_t count, style_t style, string_t **rendered)
{
    if (count > 16u)
        return false;
    const expr_t *bases[16] = {0};
    string_t *parts[16] = {0};
    long denominators[16] = {0};
    bool ok = true;
    for (size_t i = 0u; i < count; ++i)
        bases[i] = rows[i];
    for (size_t i = count % 2u; i < count; i += 2u)
        denominators[i] = equ_pair_denominator(&bases[i], &bases[i + 1u]);
    for (size_t i = 0u; ok && i < count; ++i) {
        parts[i] = expr_temporaries_math_expression(plan, bases[i], style);
        ok = parts[i] != NULL;
    }
    for (size_t i = count % 2u; ok && i + 3u < count; i += 4u) {
        ok = equ_pair_symmetric(plan, bases[i], bases[i + 2u], style, &parts[i], &parts[i + 2u]) &&
             equ_pair_symmetric(plan, bases[i + 1u], bases[i + 3u], style, &parts[i + 1u], &parts[i + 3u]);
    }
    if (ok && count % 2u) {
        rendered[0] = parts[0];
        parts[0] = NULL;
    }
    for (size_t i = count % 2u; ok && i < count; i += 2u) {
        const expr_t *radicand = NULL;
        bool root = expr_match_sqrt_expr(bases[i + 1u], &radicand);
        if (style == style_LATEX) {
            string_t *denominator = denominators[i] > 1L ? string_sprintf("%ld", denominators[i]) : NULL;
            rendered[i] = denominators[i] > 1L && !denominator ? NULL
                : equ_pair_TeX(string_c_str(parts[i]), string_c_str(parts[i + 1u]),
                               denominator ? string_c_str(denominator) : NULL, !(denominators[i] > 1L && root));
            string_free(denominator);
        } else if (denominators[i] > 1L) {
            rendered[i] = string_sprintf(root ? "(%S ± %S)/%ld" : "(%S ± (%S))/%ld",
                                         parts[i], parts[i + 1u], denominators[i]);
        } else {
            rendered[i] = string_sprintf("%S ± (%S)", parts[i], parts[i + 1u]);
        }
        ok = rendered[i] != NULL;
    }
    for (size_t i = 0u; i < count; ++i)
        string_free(parts[i]);
    return ok;
}
