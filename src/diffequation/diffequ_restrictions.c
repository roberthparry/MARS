/**
 * @file diffequ_restrictions.c
 * @brief Native real restrictions on differential-equation coordinates and parameters.
 *
 * Parses one-sided and chained inequalities separately from initial/boundary data,
 * intersects finite real endpoints, and supplies native domain restrictions and
 * serialisation. Strict and inclusive parameter comparisons are retained separately.
 * Mathematical expression and relation parsing remain delegated to their owning
 * modules. These restrictions never determine integration constants.
 */
#include <stdlib.h>
#include <stdint.h>

#include "diffequation-t.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
#define MARS_SHARED_DIFFEQUATION_INTERNAL_ACCESS
#include "internal/diffequation_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

/* Borrow a strict endpoint; absent arrays and invalid indices have no bound. */
const expr_t *de_independent_lower_bound(const diffequ_t *de, size_t index)
{
    return de && index < de->independent_count && de->interval_lower ? de->interval_lower[index] : NULL;
}

/* Borrow a strict endpoint independently of initial and boundary data. */
const expr_t *de_independent_upper_bound(const diffequ_t *de, size_t index)
{
    return de && index < de->independent_count && de->interval_upper ? de->interval_upper[index] : NULL;
}

static bool has_coordinate(const diffequ_t *de, const expr_t *expression, unsigned depth)
{
    if (!expression || depth >= 256u)
        return expression != NULL;
    /* Declared coordinate lists are small; inspect each declaration once per visited node. */
    for (size_t i = 0; i < de->independent_count; ++i)
        if (expr_struct_eq(expression, de->independent_vars[i]))
            return true;
    const expr_t *left = NULL, *right = NULL;
    return expr_child_exprs(expression, &left, &right) &&
           (has_coordinate(de, left, depth + 1u) || has_coordinate(de, right, depth + 1u));
}

static bool install_bound(diffequ_t *de, size_t coordinate, bool lower, expr_t *bound)
{
    if (has_coordinate(de, bound, 0u))
        return false;
    number_t value = expr_eval(bound);
    bool valid = num_is_real(value) && num_is_finite(value);
    if (valid && !de->interval_lower) {
        de->interval_lower = calloc(de->independent_count, sizeof *de->interval_lower);
        de->interval_upper = calloc(de->independent_count, sizeof *de->interval_upper);
    }
    valid = valid && de->interval_lower && de->interval_upper;
    if (valid) {
        expr_t **slot = lower ? &de->interval_lower[coordinate] : &de->interval_upper[coordinate];
        number_t previous = *slot ? expr_eval(*slot) : num_clone(NUM_NAN);
        if (!*slot || (lower ? num_gt(value, previous) : num_lt(value, previous))) {
            expr_free(*slot);
            *slot = expr_retain_expr(bound);
        }
        num_destroy(&previous);
        const expr_t *opposite = lower ? de->interval_upper[coordinate] : de->interval_lower[coordinate];
        if (opposite) {
            number_t other = expr_eval(opposite), selected = expr_eval(*slot);
            valid = lower ? num_lt(selected, other) : num_lt(other, selected);
            num_destroy(&selected);
            num_destroy(&other);
        }
    }
    num_destroy(&value);
    return valid;
}

static bool uses_symbol(const expr_t *expression, const expr_t *symbol, unsigned depth)
{
    if (!expression || !symbol)
        return false;
    if (depth >= 256u || expr_struct_eq(expression, symbol))
        return true;
    const expr_t *left = NULL, *right = NULL;
    return expr_child_exprs(expression, &left, &right) &&
           (uses_symbol(left, symbol, depth + 1u) || uses_symbol(right, symbol, depth + 1u));
}

/* Visit the governing derivative nodes: a field restriction is not a constant-parameter assumption. */
static bool uses_dependent(const expr_t *expression, const expr_t *governing, unsigned depth)
{
    if (!governing)
        return false;
    if (depth >= 256u)
        return true;
    if (expr_is_formal_derivative(governing) &&
        uses_symbol(expression, expr_formal_derivative_dependent(governing), 0u))
        return true;
    const expr_t *left = NULL, *right = NULL;
    return expr_child_exprs(governing, &left, &right) &&
           (uses_dependent(expression, left, depth + 1u) || uses_dependent(expression, right, depth + 1u));
}

static bool parameter_relation(diffequ_t *de, expr_t *left, expr_t *right, equ_relation_kind_t kind)
{
    if (has_coordinate(de, left, 0u) || has_coordinate(de, right, 0u) ||
        uses_dependent(left, equ_lhs(de->equation), 0u) || uses_dependent(left, equ_rhs(de->equation), 0u) ||
        uses_dependent(right, equ_lhs(de->equation), 0u) || uses_dependent(right, equ_rhs(de->equation), 0u) ||
        expr_is_formal_derivative(left) || expr_is_formal_derivative(right) ||
        de->parameter_restriction_count >= 65535u)
        return false;
    bool reversed = kind == EQU_RELATION_LESS || kind == EQU_RELATION_LESS_EQUAL;
    bool inclusive = kind == EQU_RELATION_LESS_EQUAL || kind == EQU_RELATION_GREATER_EQUAL;
    expr_t *greater = reversed ? right : left, *lesser = reversed ? left : right;
    number_t a = expr_eval(greater), b = expr_eval(lesser);
    bool a_known = !num_is_nan(a), b_known = !num_is_nan(b);
    bool valid = (!a_known || (num_is_finite(a) && num_is_real(a))) &&
                 (!b_known || (num_is_finite(b) && num_is_real(b))) &&
                 (!a_known || !b_known || (inclusive ? num_ge(a, b) : num_gt(a, b)));
    num_destroy(&b); num_destroy(&a);
    if (!valid)
        return false;
    size_t count = de->parameter_restriction_count;
    if (count == de->parameter_restriction_capacity) {
        size_t capacity = count ? count * 2u : 4u;
        if (capacity > 65535u)
            capacity = 65535u;
        de_parameter_restriction_t *restrictions = realloc(de->parameter_restrictions,
                                                           capacity * sizeof *restrictions);
        if (!restrictions)
            return false;
        de->parameter_restrictions = restrictions;
        de->parameter_restriction_capacity = capacity;
    }
    de->parameter_restrictions[count] =
        (de_parameter_restriction_t){expr_retain_expr(greater), expr_retain_expr(lesser), inclusive};
    de->parameter_restriction_count = count + 1u;
    return true;
}

static bool interval_relation(diffequ_t *de, expr_t *left, expr_t *right, equ_relation_kind_t kind)
{
    bool strict = kind == EQU_RELATION_LESS || kind == EQU_RELATION_GREATER;
    if (!strict && kind != EQU_RELATION_LESS_EQUAL && kind != EQU_RELATION_GREATER_EQUAL)
        return false;
    for (size_t i = 0; i < de->independent_count; ++i) {
        if (expr_struct_eq(left, de->independent_vars[i]))
            return strict && install_bound(de, i, kind == EQU_RELATION_GREATER, right);
        if (expr_struct_eq(right, de->independent_vars[i]))
            return strict && install_bound(de, i, kind == EQU_RELATION_LESS, left);
    }
    return parameter_relation(de, left, right, kind);
}

/* Parse inequalities as coordinate metadata, using the equation module's single relation scanner. */
int de_parse_restriction(diffequ_t *de, const string_t *text, const string_t *const *names,
                      expr_t *const *symbols, size_t symbol_count)
{
    if (!de || !text)
        return -1;
    string_view_t view = string_view_all(text);
    size_t position = 0u, start = 0u;
    equ_input_relation_t relation;
    int found = equ_input_next_relation(view, &position, &relation);
    if (found == 0 || (found == 1 && relation.kind == EQU_RELATION_EQUAL))
        return 0;
    expr_t *previous = NULL;
    equ_relation_kind_t previous_kind = EQU_RELATION_COUNT;
    bool valid = found == 1;
    unsigned count = 0u;
    while (valid) {
        size_t end = found == 1 ? relation.position : string_view_length(view);
        string_t *part = string_sprintf("%W", string_view_trim(string_view_slice(view, start, end - start)));
        expr_t *current = part ? expr_from_expression_text_formal(part, names, symbols, symbol_count) : NULL;
        string_free(part);
        valid = current && (!previous || interval_relation(de, previous, current, previous_kind));
        expr_free(previous);
        previous = current;
        if (!valid || found == 0)
            break;
        bool less = relation.kind == EQU_RELATION_LESS || relation.kind == EQU_RELATION_LESS_EQUAL;
        bool greater = relation.kind == EQU_RELATION_GREATER || relation.kind == EQU_RELATION_GREATER_EQUAL;
        bool previous_less = previous_kind == EQU_RELATION_LESS || previous_kind == EQU_RELATION_LESS_EQUAL;
        if (++count > 2u || (count == 2u && less != previous_less) || (!less && !greater)) {
            valid = false;
            break;
        }
        previous_kind = relation.kind;
        start = position;
        found = equ_input_next_relation(view, &position, &relation);
        valid = found >= 0;
    }
    expr_free(previous);
    return valid ? 1 : -1;
}

/* Keep coordinate and parameter predicates on the complete solution, independently of its arbitrary constants. */
static expr_t *restrict_domain(const diffequ_t *de, const expr_t *expression, bool coordinates)
{
    if (!de || !expression)
        return NULL;
    if (de->parameter_restriction_count > (SIZE_MAX - 1u) / 2u ||
        de->independent_count > (SIZE_MAX - 1u - 2u * de->parameter_restriction_count) / 6u)
        return NULL;
    size_t capacity = 1u + 6u * de->independent_count + 2u * de->parameter_restriction_count, count = 1u;
    expr_t **arguments = calloc(capacity, sizeof *arguments);
    expr_t **predicates = calloc(de->independent_count ? de->independent_count : 1u, sizeof *predicates);
    if (!arguments || !predicates) {
        free(predicates);
        free(arguments);
        return NULL;
    }
    arguments[0] = (expr_t *)expression;
    expr_t *zero = expr_const_zero();
    for (size_t i = 0; coordinates && i < de->independent_count; ++i) {
        const expr_t *lower = de_independent_lower_bound(de, i), *upper = de_independent_upper_bound(de, i);
        if (lower || upper) {
            predicates[i] = expr_real_parameter(de->independent_vars[i]);
            arguments[count++] = predicates[i];
            arguments[count++] = zero;
        }
        if (lower) {
            arguments[count++] = de->independent_vars[i];
            arguments[count++] = (expr_t *)lower;
        }
        if (upper) {
            arguments[count++] = (expr_t *)upper;
            arguments[count++] = de->independent_vars[i];
        }
    }
    expr_t **comparisons = calloc(de->parameter_restriction_count ? de->parameter_restriction_count : 1u,
                                  sizeof *comparisons);
    if (!comparisons) {
        for (size_t i = 0; i < de->independent_count; ++i) expr_free(predicates[i]);
        expr_free(zero); free(predicates); free(arguments);
        return NULL;
    }
    for (size_t i = 0; i < de->parameter_restriction_count; ++i) {
        const de_parameter_restriction_t *restriction = de->parameter_restrictions + i;
        comparisons[i] = expr_real_comparison(restriction->greater, restriction->lesser, restriction->inclusive);
        arguments[count++] = comparisons[i];
        arguments[count++] = zero;
    }
    expr_t *restricted = count > 1u ? expr_real_domain_from_args(count, arguments) : expr_retain_expr(expression);
    /* Arithmetic domain guards belong to the complete result, including arbitrary homogeneous terms. */
    expr_t *result = restricted ? expr_simplify(restricted) : NULL;
    expr_free(restricted);
    for (size_t i = 0u; i < de->independent_count; ++i)
        expr_free(predicates[i]);
    for (size_t i = 0; i < de->parameter_restriction_count; ++i)
        expr_free(comparisons[i]);
    free(comparisons);
    expr_free(zero);
    free(predicates);
    free(arguments);
    return result;
}

/* Retain the original expression and every authored coordinate or parameter restriction. */
expr_t *de_restrict_domain(const diffequ_t *de, const expr_t *expression)
{
    return restrict_domain(de, expression, true);
}

/* Provide a native, live predicate for plot-local parameter choices without coordinate or solution poles. */
expr_t *de_parameter_domain(const diffequ_t *de)
{
    expr_t *one = expr_const_one();
    expr_t *result = restrict_domain(de, one, false);
    expr_free(one);
    return result;
}

/* Preserve mathematical endpoint expressions in canonical, round-trippable native inequalities. */
string_t *de_restrictions_to_text(const diffequ_t *de)
{
    string_t *out = de ? string_new() : NULL;
    for (size_t i = 0; out && i < de->independent_count; ++i) {
        const expr_t *lower = de_independent_lower_bound(de, i), *upper = de_independent_upper_bound(de, i);
        if (!lower && !upper)
            continue;
        const char *name = expr_symbol_name(de->independent_vars[i]);
        string_t *x = name ? string_new_with(name) : NULL;
        string_t *a = lower ? expr_to_text(lower, style_UNBOUND) : string_new();
        string_t *b = upper ? expr_to_text(upper, style_UNBOUND) : string_new();
        bool ok = x && a && b && string_append_format(out, "%s%S%s%S%s%S", string_byte_length(out) ? ", " : "",
                    a, lower ? " < " : "", x, upper ? " < " : "", b) >= 0;
        string_free(b); string_free(a); string_free(x);
        if (!ok) {
            string_free(out);
            out = NULL;
        }
    }
    for (size_t i = 0; out && i < de->parameter_restriction_count; ++i) {
        const de_parameter_restriction_t *restriction = de->parameter_restrictions + i;
        string_t *left = expr_to_text(restriction->greater, style_UNBOUND);
        string_t *right = expr_to_text(restriction->lesser, style_UNBOUND);
        bool ok = left && right && string_append_format(out, "%s%S %s %S", string_byte_length(out) ? ", " : "",
                                                        left, restriction->inclusive ? "≥" : ">", right) >= 0;
        string_free(right); string_free(left);
        if (!ok) { string_free(out); out = NULL; }
    }
    return out;
}

/* Install all restricted equations together; failed allocations leave the original result usable. */
bool de_result_apply_restrictions(const diffequ_t *de, diffequ_solve_result_t *result)
{
    if (!de || !result)
        return false;
    if ((!de->interval_lower && !de->parameter_restriction_count) || result->restrictions_applied)
        return true;
    equation_t **replacements = calloc(result->solution_count ? result->solution_count : 1u, sizeof *replacements);
    if (!replacements)
        return false;
    bool ok = true;
    for (size_t i = 0; ok && i < result->solution_count; ++i) {
        const equation_t *solution = result->solutions[i];
        expr_t *restricted = de_restrict_domain(de, equ_rhs(solution));
        replacements[i] = restricted ? equ_new(equ_lhs(solution), restricted) : NULL;
        expr_free(restricted);
        ok = replacements[i] != NULL;
    }
    for (size_t i = 0; i < result->solution_count; ++i) {
        if (ok) {
            equ_free(result->solutions[i]);
            result->solutions[i] = replacements[i];
        } else {
            equ_free(replacements[i]);
        }
    }
    if (ok)
        result->restrictions_applied = true;
    free(replacements);
    return ok;
}
