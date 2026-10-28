/**
 * @file diffequ_notation_steps.c
 * @brief Compose solver-owned derivation rows as semantic callback data.
 *
 * Solvers supply native mathematical expressions and literal explanatory prose.
 * Expression interprets each formula; this module specifies equation and row
 * structure. TeX alone decides their notation and alignment.
 */
#include <stdlib.h>

#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

/* Every row is projected before any result text is replaced. */
bool de_notation_steps(diffequ_solve_result_t *result, const char *plain,
                       const de_notation_step_t *steps, size_t count)
{
    if (!result || !plain || !steps || !count || count > 65536u)
        return false;
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t **rows = build ? calloc(count, sizeof *rows) : NULL;
    if (!rows) {
        notation_build_free(build);
        return false;
    }
    notation_build_dependent_variables(build, true);
    for (size_t i = 0u; i < count; ++i) {
        const TeX_model_node_t *math = steps[i].left ? notation_build_formula(build, steps[i].left) : NULL;
        if (steps[i].right) {
            const TeX_model_node_t *parts[] = {math, notation_build_formula(build, steps[i].right)};
            math = notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_EQUATION, .child_count = 2u}, parts);
        }
        const TeX_model_node_t *caption = steps[i].caption
            ? notation_build_literal(build, TeX_NODE_TEXT, steps[i].caption) : NULL;
        if ((steps[i].left && !math) || (steps[i].caption && !caption)) {
            break;
        }
        const TeX_model_node_t *parts[] = {caption, math};
        rows[i] = caption && math ? notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_STATEMENTS, .child_count = 2u}, parts) : math ? math : caption;
    }
    const TeX_model_node_t *root = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = count}, rows);
    string_t *rendered = notation_build_render(build, root);
    bool valid = rendered && de_solve_result_set_steps(result, plain) == 0 &&
                            de_solve_result_set_steps_TeX(result, string_c_str(rendered)) == 0;
    string_free(rendered);
    free(rows);
    notation_build_free(build);
    return valid;
}

/* Notation is rendered by TeX before the equation adopts independent copies of the display strings. */
bool de_notation_display(equation_t *solution, const string_t *left, const string_t *right,
                         notation_build_t *build, const TeX_model_node_t *body)
{
    TeX_context_t *context = solution ? expr_TeX_context(equ_lhs(solution)) : NULL;
    TeX_t *notation = context ? TeX_from_context(context) : NULL;
    string_t *left_notation = notation ? TeX_render(notation) : NULL;
    string_t *right_notation = notation_build_render(build, body);
    bool valid = left && right && left_notation && right_notation &&
        equ_set_display_unbound(solution, left, right) == 0 &&
        equ_set_display_TeX(solution, left_notation, right_notation) == 0;
    string_free(right_notation);
    string_free(left_notation);
    TeX_free(notation);
    TeX_context_free(context);
    return valid;
}
