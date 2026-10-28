/**
 * @file expr_notation_terms.c
 * @brief Select native additive terms for the legacy wrapping adapter.
 *
 * Expression owns traversal, sign handling and display ordering. The temporary
 * selection borrows nodes from the source expression and contains no TeX syntax.
 */
#include <stdint.h>
#include <stdlib.h>

#include "expression-t.h"

#define MARS_SHARED_EXPR_NOTATION_INTERNAL_ACCESS
#include "internal/expr_notation_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"

typedef struct {
    const expr_t *expression;
    int sign;
} notation_term_t;

struct _expr_notation_terms_t {
    notation_term_t *items;
    size_t count;
    size_t capacity;
};

static bool append_term(expr_notation_terms_t *terms, const expr_t *expression, int sign)
{
    if (terms->count == terms->capacity) {
        size_t capacity = terms->capacity ? terms->capacity * 2u : 8u;
        if (capacity < terms->capacity || capacity > SIZE_MAX / sizeof *terms->items)
            return false;
        notation_term_t *items = realloc(terms->items, capacity * sizeof *items);
        if (!items)
            return false;
        terms->items = items;
        terms->capacity = capacity;
    }
    terms->items[terms->count++] = (notation_term_t){expression, sign < 0 ? -1 : 1};
    return true;
}

static bool collect_terms(expr_notation_terms_t *terms, const expr_t *expression, int sign)
{
    if (!expression)
        return false;
    const expr_t *nodes[96];
    int signs[96];
    size_t count = 0u;
    if (expr_display_ordered_sum(expression, nodes, signs, &count, 96u)) {
        for (size_t i = 0u; i < count; ++i)
            if (!append_term(terms, nodes[i], sign * signs[i]))
                return false;
        return true;
    }
    if (expr_is_neg(expression))
        return collect_terms(terms, expression->a, -sign);
    if (expr_is_op(expression, &ops_add) || expr_is_op(expression, &ops_sub))
        return collect_terms(terms, expression->a, sign) &&
               collect_terms(terms, expression->b, expr_is_op(expression, &ops_sub) ? -sign : sign);
    return append_term(terms, expression, sign);
}

/* Allocate a selection whose contents remain borrowed from the input graph. */
expr_notation_terms_t *expr_notation_terms_new(const expr_t *expression)
{
    expr_notation_terms_t *terms = expression && expr_is_addsub(expression) ? calloc(1u, sizeof *terms) : NULL;
    if (terms && !collect_terms(terms, expression, 1)) {
        expr_notation_terms_free(terms);
        return NULL;
    }
    return terms;
}

/* Identify a single additive operand without exposing the native product graph. */
bool expr_notation_scaled_sum(const expr_t *expression, const expr_t **factor, const expr_t **sum)
{
    if (!expression || !factor || !sum || !expr_is_mul(expression))
        return false;
    bool left = expr_is_addsub(expression->a), right = expr_is_addsub(expression->b);
    if (left == right)
        return false;
    *sum = left ? expression->a : expression->b;
    *factor = left ? expression->b : expression->a;
    return true;
}

/* Expose selection size without exposing its representation. */
size_t expr_notation_terms_count(const expr_notation_terms_t *terms)
{
    return terms ? terms->count : 0u;
}

/* Return one borrowed term and its sign. */
const expr_t *expr_notation_terms_get(const expr_notation_terms_t *terms, size_t index, int *sign)
{
    if (!terms || index >= terms->count || !sign)
        return NULL;
    *sign = terms->items[index].sign;
    return terms->items[index].expression;
}

/* Free the selection, retaining the caller's expression graph. */
void expr_notation_terms_free(expr_notation_terms_t *terms)
{
    if (terms) {
        free(terms->items);
        free(terms);
    }
}
