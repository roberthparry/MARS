/* Symmetric-shift presentation: retain a common centre in a+b and a-b. */
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"

static bool display_shift_terms(const expr_t *expr, const expr_t **terms, bool *negative)
{
    if (!expr_is_addsub(expr))
        return false;
    terms[0] = expr->a;
    terms[1] = expr->b;
    negative[0] = false;
    negative[1] = expr_is_op(expr, &ops_sub);
    for (size_t i = 0u; i < 2u; ++i) {
        while (expr_is_neg(terms[i])) {
            terms[i] = terms[i]->a;
            negative[i] = !negative[i];
        }
    }
    return terms[0] && terms[1];
}

/* Return the borrowed displacement and its sign when one summand is the supplied centre. */
bool expr_display_centred_shift_parts(const expr_t *expr, const expr_t *centre,
                                      const expr_t **shift, bool *subtract)
{
    const expr_t *terms[2];
    bool negative[2];
    if (!centre || !display_shift_terms(expr, terms, negative))
        return false;
    for (size_t i = 0u; i < 2u; ++i) {
        if (!negative[i] && expr_struct_eq(terms[i], centre)) {
            *shift = terms[1u - i];
            *subtract = negative[1u - i];
            return true;
        }
    }
    return false;
}

static const expr_t *display_function_argument(const expr_t *expr)
{
    if (!expr || !expr->ops || !expr->a || expr->b || expr_is_neg(expr) ||
        expr_is_op(expr->a, &ops_argument_list))
        return NULL;
    return expr_is_arbitrary_function(expr) || expr->ops->arity == EXPR_OP_UNARY ? expr->a : NULL;
}

/* Compare only the two endpoints or sibling calls; the four possible summand pairings have bounded cost. */
const expr_t *expr_display_symmetric_shift_centre(const expr_t *expr)
{
    const expr_t *left = NULL, *right = NULL;
    if (expr_is_op(expr, &ops_integral)) {
        left = expr_integral_lower_bound_expr(expr);
        right = expr_integral_upper_bound_expr(expr);
    } else if (expr_is_addsub(expr)) {
        left = display_function_argument(expr->a);
        right = display_function_argument(expr->b);
    }
    const expr_t *terms[2];
    bool negative[2];
    if (!display_shift_terms(left, terms, negative))
        return NULL;
    for (size_t i = 0u; i < 2u; ++i) {
        const expr_t *shift = NULL;
        bool subtract = false;
        if (!negative[i] && expr_display_centred_shift_parts(right, terms[i], &shift, &subtract) &&
            subtract != negative[1u - i] && expr_struct_eq(shift, terms[1u - i]))
            return terms[i];
    }
    return NULL;
}
