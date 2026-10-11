/**
 * @file equation_solution_pair.c
 * @brief Structural presentation of paired exact solution rows.
 *
 * Extracts shared reciprocal integer factors and recognises additive centres with opposite displacements,
 * including square-root radicands. Operates on borrowed trees and retains the existing temporary-binding plan;
 * neither the solver's algebra nor the numerical values are changed by presentation.
 */

#include "number.h"
#include "ustring.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"

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
    if (a.subtract != b.subtract)
        return expr_struct_eq(a.expr, b.expr);
    expr_t *negative = expr_simplify_owned(expr_neg(a.expr));
    bool equal = negative && expr_struct_eq(negative, b.expr);
    expr_free(negative);
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
    if (!scalar && expr_match_div_expr(expr, &left, &right))
        negative = equ_pair_negative(left, depth + 1u);
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
            string_t *root = string_sprintf(style == style_LATEX ? "\\sqrt{%S}" : "√(%S)", texts[side]);
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
        if (denominators[i] > 1L && root)
            rendered[i] = string_sprintf(style == style_LATEX ? "\\frac{%S \\pm %S}{%ld}" : "(%S ± %S)/%ld",
                                          parts[i], parts[i + 1u], denominators[i]);
        else if (denominators[i] > 1L)
            rendered[i] = string_sprintf(style == style_LATEX ? "\\frac{%S \\pm \\left(%S\\right)}{%ld}" :
                                                               "(%S ± (%S))/%ld",
                                          parts[i], parts[i + 1u], denominators[i]);
        else
            rendered[i] = string_sprintf(style == style_LATEX ? "%S \\pm \\left(%S\\right)" : "%S ± (%S)",
                                          parts[i], parts[i + 1u]);
        ok = rendered[i] != NULL;
    }
    for (size_t i = 0u; i < count; ++i)
        string_free(parts[i]);
    return ok;
}
