/**
 * @file expr_stringout_symshift.c
 * @brief Centred presentation of symmetric shifts.
 *
 * Recognises arguments with a common centre and opposite offsets and emits that centre consistently first. The
 * structural rule applies independently of function names or variable spellings. Symbolic complex shifts
 * likewise retain their centre first and place i before the symbolic displacement.
 *
 * This is part of the expr_t implementation. Preserve expression ownership, symbol identity and mathematical
 * domain restrictions when extending these operations.
 */

#include <string.h>

/* Symmetric-shift presentation: retain a common centre in a+b and a-b. */
#define MARS_EXPR_INTERNAL_ACCESS
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

static bool display_imaginary_unit(const expr_t *expr)
{
    return expr_is_const(expr) && (!expr_is_named_const(expr) || !strcmp(expr_symbol_name(expr), "i")) &&
           num_eq(expr->c, NUM_I);
}

static bool display_contains_imaginary_unit(const expr_t *expr)
{
    return expr && (display_imaginary_unit(expr) || display_contains_imaginary_unit(expr->a) ||
                   display_contains_imaginary_unit(expr->b));
}

/* Select the mathematical centre and imaginary displacement without rendering either. */
bool expr_display_complex_shift(const expr_t *expr, const expr_t **centre_out, const expr_t **coefficient_out,
                                bool *centre_negative, bool *coefficient_negative)
{
    const expr_t *terms[2];
    bool negative[2];
    if (!display_shift_terms(expr, terms, negative))
        return false;
    for (unsigned side = 0; side < 2; ++side) {
        const expr_t *term = terms[side], *centre = terms[1 - side];
        if (!expr_is_mul(term) || display_contains_imaginary_unit(centre))
            continue;
        const expr_t *coefficient = display_imaginary_unit(term->a) ? term->b :
                                    display_imaginary_unit(term->b) ? term->a : NULL;
        if (!coefficient || expr_is_const(coefficient) || display_contains_imaginary_unit(coefficient))
            continue;
        *centre_out = centre;
        *coefficient_out = coefficient;
        *centre_negative = negative[1u - side];
        *coefficient_negative = negative[side];
        return true;
    }
    return false;
}

/* Display a symbolic centre plus an explicit imaginary displacement without changing the expression tree. */
bool expr_emit_complex_shift(const expr_t *expr, sbuf_t *buffer, int parent_prec, style_t style)
{
    const expr_t *centre = NULL, *coefficient = NULL;
    bool centre_negative = false, coefficient_negative = false;
    if (expr_display_complex_shift(expr, &centre, &coefficient, &centre_negative, &coefficient_negative)) {
        void (*emit)(const expr_t *, sbuf_t *, int) = style == style_FUNCTION ? emit_func : emit_expr;
        bool bracketed = parent_prec > PREC_ADD;
        if (bracketed)
            sbuf_putc(buffer, '(');
        if (centre_negative)
            sbuf_putc(buffer, '-');
        emit(centre, buffer, centre_negative ? PREC_MUL : PREC_ADD);
        sbuf_puts(buffer, coefficient_negative ? " - i" : " + i");
        sbuf_puts(buffer, style == style_FUNCTION ? "." : "·");
        emit(coefficient, buffer, PREC_MUL);
        if (bracketed)
            sbuf_putc(buffer, ')');
        return true;
    }
    return false;
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
