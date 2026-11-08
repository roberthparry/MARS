/**
 * @file diffequ_interval.c
 * @brief Native strict real intervals for differential-equation coordinates.
 *
 * Parses one-sided and chained inequalities separately from initial/boundary data,
 * intersects finite real endpoints, and supplies native domain restrictions and
 * serialisation. Mathematical expression and relation parsing remain delegated
 * to their owning modules. Interval endpoints never determine integration constants.
 */
#include <stdlib.h>
#include <stdint.h>

#include "diffequation-t.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
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

static bool interval_relation(diffequ_t *de, expr_t *left, expr_t *right, equ_relation_kind_t kind)
{
    if (kind != EQU_RELATION_LESS && kind != EQU_RELATION_GREATER)
        return false;
    for (size_t i = 0; i < de->independent_count; ++i) {
        if (expr_struct_eq(left, de->independent_vars[i]))
            return install_bound(de, i, kind == EQU_RELATION_GREATER, right);
        if (expr_struct_eq(right, de->independent_vars[i]))
            return install_bound(de, i, kind == EQU_RELATION_LESS, left);
    }
    return false;
}

/* Parse inequalities as coordinate metadata, using the equation module's single relation scanner. */
int de_parse_interval(diffequ_t *de, const string_t *text, const string_t *const *names,
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
        if (++count > 2u || (count == 2u && relation.kind != previous_kind) ||
            (relation.kind != EQU_RELATION_LESS && relation.kind != EQU_RELATION_GREATER)) {
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

/* Keep interval predicates on the complete solution, independently of the constants it contains. */
expr_t *de_restrict_intervals(const diffequ_t *de, const expr_t *expression)
{
    if (!de || !expression)
        return NULL;
    if (de->independent_count > (SIZE_MAX - 1u) / 6u)
        return NULL;
    size_t capacity = 1u + 6u * de->independent_count, count = 1u;
    expr_t **arguments = calloc(capacity, sizeof *arguments);
    expr_t **predicates = calloc(de->independent_count ? de->independent_count : 1u, sizeof *predicates);
    if (!arguments || !predicates) {
        free(predicates);
        free(arguments);
        return NULL;
    }
    arguments[0] = (expr_t *)expression;
    expr_t *zero = expr_const_zero();
    for (size_t i = 0; i < de->independent_count; ++i) {
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
    expr_t *restricted = count > 1u ? expr_real_domain_from_args(count, arguments) : expr_retain_expr(expression);
    /* Arithmetic domain guards belong to the complete result, including arbitrary homogeneous terms. */
    expr_t *result = restricted ? expr_simplify(restricted) : NULL;
    expr_free(restricted);
    for (size_t i = 0u; i < de->independent_count; ++i)
        expr_free(predicates[i]);
    expr_free(zero);
    free(predicates);
    free(arguments);
    return result;
}

/* Preserve mathematical endpoint expressions in canonical, round-trippable native inequalities. */
string_t *de_intervals_to_text(const diffequ_t *de)
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
    return out;
}

/* Install all restricted equations together; failed allocations leave the original result usable. */
bool de_result_apply_intervals(const diffequ_t *de, diffequ_solve_result_t *result)
{
    if (!de || !result)
        return false;
    if (!de->interval_lower || result->intervals_applied)
        return true;
    equation_t **replacements = calloc(result->solution_count ? result->solution_count : 1u, sizeof *replacements);
    if (!replacements)
        return false;
    bool ok = true;
    for (size_t i = 0; ok && i < result->solution_count; ++i) {
        const equation_t *solution = result->solutions[i];
        expr_t *restricted = de_restrict_intervals(de, equ_rhs(solution));
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
        result->intervals_applied = true;
    free(replacements);
    return ok;
}
