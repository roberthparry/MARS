/**
 * @file expr_beautify_symmetry.c
 * @brief Preserve a structurally shared additive centre across a small expression family.
 *
 * Recognises signed summands common to every member and places that centre before each displacement.
 * Native equation presentation uses this bounded beautification after solving. No numerical matching,
 * function-specific recognition or algebraic simplification changes the values or signs of the family.
 */

#include <stdbool.h>
#include <stddef.h>

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

typedef struct {
    const expr_t *expression;
    bool subtract;
} symmetry_term_t;

typedef struct {
    symmetry_term_t terms[8];
    size_t count;
} symmetry_sum_t;

static bool symmetry_collect(const expr_t *expression, bool subtract, symmetry_sum_t *sum, unsigned depth)
{
    if (!expression || depth > 16u)
        return false;
    if (expr_is_neg(expression))
        return symmetry_collect(expression->a, !subtract, sum, depth + 1u);
    if (expr_is_addsub(expression))
        return symmetry_collect(expression->a, subtract, sum, depth + 1u) &&
               symmetry_collect(expression->b, subtract != expr_is_op(expression, &ops_sub), sum, depth + 1u);
    if (sum->count == sizeof sum->terms / sizeof *sum->terms)
        return false;
    sum->terms[sum->count++] = (symmetry_term_t){expression, subtract};
    return true;
}

static expr_t *symmetry_append(expr_t *owned_sum, symmetry_term_t term)
{
    expr_t *result = owned_sum ? (term.subtract ? expr_sub(owned_sum, term.expression) :
                                                  expr_add(owned_sum, term.expression)) :
                               (term.subtract ? expr_neg(term.expression) : expr_retain_expr(term.expression));
    expr_free(owned_sum);
    return result;
}

/* Reorder shared signed summands before each displacement, preserving every source graph and coefficient. */
bool expr_beautify_shared_centre(const expr_t *const *expressions, size_t count, expr_t **ordered)
{
    if (!expressions || !ordered || count < 2u || count > 16u)
        return false;
    symmetry_sum_t sums[16] = {0};
    expr_t *expanded[16] = {0}, *results[16] = {0};
    unsigned common[16] = {0};
    size_t leading[16][8] = {{0}}, common_count = 0u;
    bool ok = true;
    for (size_t row = 0u; row < count; ++row) {
        expanded[row] = expr_expand_preserved_for_display(expressions[row]);
        if (!expanded[row]) {
            ok = false;
            goto cleanup;
        }
        if (!symmetry_collect(expanded[row], false, &sums[row], 0u))
            goto cleanup;
    }
    /* At most sixteen rows of eight terms: bounded structural matching retains duplicate multiplicities. */
    for (size_t candidate = 0u; candidate < sums[0].count; ++candidate) {
        size_t matches[16] = {candidate};
        symmetry_term_t term = sums[0].terms[candidate];
        bool shared = true;
        for (size_t row = 1u; shared && row < count; ++row) {
            size_t column = 0u;
            for (; column < sums[row].count; ++column)
                if (!(common[row] & (1u << column)) && term.subtract == sums[row].terms[column].subtract &&
                    expr_struct_eq(term.expression, sums[row].terms[column].expression))
                    break;
            matches[row] = column;
            shared = column < sums[row].count;
        }
        if (shared) {
            for (size_t row = 0u; row < count; ++row) {
                leading[row][common_count] = matches[row];
                common[row] |= 1u << matches[row];
            }
            ++common_count;
        }
    }
    if (!common_count)
        goto cleanup;
    for (size_t row = 0u; row < count; ++row) {
        bool changed = false;
        for (size_t i = 0u; i < common_count; ++i)
            changed = changed || leading[row][i] != i;
        if (!changed)
            continue;
        for (size_t i = 0u; i < common_count; ++i) {
            results[row] = symmetry_append(results[row], sums[row].terms[leading[row][i]]);
            if (!results[row]) {
                ok = false;
                goto cleanup;
            }
        }
        for (size_t i = 0u; i < sums[row].count; ++i) {
            if (common[row] & (1u << i))
                continue;
            results[row] = symmetry_append(results[row], sums[row].terms[i]);
            if (!results[row]) {
                ok = false;
                goto cleanup;
            }
        }
        expr_t *linked = expr_clone_linked_symbols(results[row], expressions[row]);
        expr_free(results[row]);
        results[row] = linked;
        if (!linked) {
            ok = false;
            goto cleanup;
        }
    }
cleanup:
    for (size_t row = 0u; row < count; ++row) {
        if (ok) {
            ordered[row] = results[row];
            results[row] = NULL;
        }
        expr_free(results[row]);
        expr_free(expanded[row]);
    }
    return ok;
}
