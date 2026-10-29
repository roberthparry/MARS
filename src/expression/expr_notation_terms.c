/**
 * @file expr_notation_terms.c
 * @brief Own expression term selection and naming scopes for semantic line wrapping.
 *
 * Native callbacks select and order mathematical terms, including a distributed
 * scalar factor when appropriate. TeX receives semantic contexts and decides
 * line breaks and alignment. Applications use expr_to_TeX_body_wrapped without
 * managing a rendering context or knowing typesetting syntax.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "expression-t.h"
#include "TeX-t.h"

#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "internal/TeX_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_notation_internal.h"

typedef struct {
    const expr_t *expression;
    int sign;
} expr_wrap_term_t;

typedef struct {
    const expr_t *expression;
    const expr_t *factor;
    expr_wrap_term_t *terms;
    size_t count;
    size_t capacity;
    bool prepared;
} expr_wrap_state_t;

static bool append_term(expr_wrap_state_t *state, const expr_t *expression, int sign)
{
    if (state->count == state->capacity) {
        size_t capacity = state->capacity ? state->capacity * 2u : 8u;
        if (capacity < state->capacity || capacity > SIZE_MAX / sizeof *state->terms)
            return false;
        expr_wrap_term_t *terms = realloc(state->terms, capacity * sizeof *terms);
        if (!terms)
            return false;
        state->terms = terms;
        state->capacity = capacity;
    }
    state->terms[state->count++] = (expr_wrap_term_t){expression, sign < 0 ? -1 : 1};
    return true;
}

static bool collect_terms(expr_wrap_state_t *state, const expr_t *expression, int sign)
{
    if (!expression)
        return false;
    const expr_t *nodes[96];
    int signs[96];
    size_t count = 0u;
    if (expr_display_ordered_sum(expression, nodes, signs, &count, 96u)) {
        for (size_t i = 0u; i < count; ++i)
            if (!append_term(state, nodes[i], sign * signs[i]))
                return false;
        return true;
    }
    if (expr_is_neg(expression))
        return collect_terms(state, expression->a, -sign);
    if (expr_is_op(expression, &ops_add) || expr_is_op(expression, &ops_sub))
        return collect_terms(state, expression->a, sign) &&
               collect_terms(state, expression->b, expr_is_op(expression, &ops_sub) ? -sign : sign);
    return append_term(state, expression, sign);
}

/* Borrow a scalar and additive operand without changing the product's algebra. */
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

static size_t wrapped_term_count(void *context)
{
    expr_wrap_state_t *state = context;
    if (!state->prepared) {
        state->prepared = true;
        const expr_t *sum = state->expression;
        if (expr_is_mul(sum)) {
            bool left = expr_is_addsub(sum->a), right = expr_is_addsub(sum->b);
            if (left != right) {
                state->factor = left ? sum->b : sum->a;
                sum = left ? sum->a : sum->b;
            }
        }
        if (!expr_is_addsub(sum) || !collect_terms(state, sum, 1)) {
            free(state->terms);
            state->terms = NULL;
            state->count = state->capacity = 0u;
        }
    }
    return state->count;
}

static TeX_context_t *wrapped_term(void *context, size_t index, int *sign)
{
    expr_wrap_state_t *state = context;
    if (index >= state->count || !sign)
        return NULL;
    *sign = state->terms[index].sign;
    const expr_t *term = state->terms[index].expression;
    if (!state->factor)
        return expr_TeX_result_context(term);
    expr_t *product = expr_mul(state->factor, term);
    TeX_context_t *result = product ? expr_TeX_result_context(product) : NULL;
    expr_free(product);
    return result;
}

/* Keep expression selection and temporary names native while TeX owns every line-break command. */
char *expr_to_TeX_body_wrapped(const expr_t *expression, size_t line_limit)
{
    if (!expression)
        return NULL;
    if (expr_notation_requires_unwrapped_body(expression))
        return expr_to_TeX_body(expression);
    autoname_table_t names;
    autoname_init(&names);
    assign_unnamed_vars_dfs((expr_t *)expression, &names);
    expr_t *initial = NULL;
    expr_t *display = expr_notation_primitive_display(expression, &initial);
    const expr_t *selected = display ? display : expression;
    expr_wrap_state_t state = {.expression = selected};
    static const TeX_sum_provider_t provider = {.count = wrapped_term_count, .term = wrapped_term};
    TeX_context_t *context = expr_TeX_result_context(selected);
    TeX_t *body = TeX_wrap_sum(context, line_limit,
        expr_distribution_has_qualification(selected) ? NULL : &provider, &state);
    TeX_context_free(context);
    body = expr_notation_append_primitive(body, initial);
    char *result = body ? strdup(TeX_source(body)) : NULL;
    TeX_free(body);
    free(state.terms);
    expr_free(display);
    expr_free(initial);
    autoname_restore(&names);
    return result;
}
