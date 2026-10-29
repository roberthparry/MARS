/**
 * @file equation_notation_provider.c
 * @brief Expose equation sides through the shared semantic callback interface.
 *
 * The equation module selects its mathematical sides. TeX receives only an
 * equality node with two semantic operands, without private equation metadata.
 */
#include "equation-t.h"
#include "TeX-t.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"

/* Preserve the native equality without solving it or transferring its operands. */
const TeX_model_node_t *notation_build_equation(notation_build_t *build, const equation_t *equation)
{
    if (!equation)
        return NULL;
    const TeX_model_node_t *sides[] = {
        notation_build_expr(build, equ_lhs(equation)), notation_build_expr(build, equ_rhs(equation))
    };
    return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_EQUATION, .child_count = 2u}, sides);
}

/* Snapshot an equation independently of the caller's original object. */
TeX_context_t *equ_TeX_context(const equation_t *equation)
{
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t *root = notation_build_equation(build, equation);
    TeX_context_t *context = notation_build_context(build, root);
    notation_build_free(build);
    return context;
}
