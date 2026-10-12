/**
 * @file equation_context.c
 * @brief Context-aware presentation of independently solved algebraic equations.
 *
 * Keeps solving and assumption-based result simplification separate. Search metadata
 * remains owned by the original result collection; no roots are filtered by samples.
 */
#include "equation-t.h"
#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"

/* Apply one context consistently to all solved formulae, retaining search metadata. */
equation_solutions_t *equ_derive_solutions_with_context(const equation_t *equation, const expr_context_t *context)
{
    equation_solutions_t *result = equ_derive_solutions(equation);
    if (!result)
        return NULL;
    for (size_t i = 0; i < result->count; ++i) {
        equation_t *simplified = equ_simplify_with_context(result->solutions[i], context);
        if (!simplified) {
            equ_solutions_free(result);
            return NULL;
        }
        equ_free(result->solutions[i]);
        result->solutions[i] = simplified;
    }
    return result;
}
