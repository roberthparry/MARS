/**
 * @file diffequ_notation_lie.c
 * @brief Describe verified Lie invariants, generators and reduction semantically.
 *
 * The differential-equation module chooses the mathematical content and keeps
 * the native explanation; the TeX module renders the supplied semantic nodes.
 */
#include <stdint.h>
#include <stdlib.h>

#include "diffequation-t.h"
#include "matrix-t.h"

#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

static const TeX_model_node_t *lie_pair(notation_build_t *build, TeX_node_kind_t kind,
                                        const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *children[] = {left, right};
    return notation_build_node(build, (TeX_node_view_t){.kind = kind, .child_count = 2u}, children);
}

static const TeX_model_node_t *lie_index(notation_build_t *build, const char *name, size_t index)
{
    string_t *number = string_sprintf("%zu", index);
    const TeX_model_node_t *value = number ? notation_build_literal(build, TeX_NODE_NUMBER, string_c_str(number)) : NULL;
    const TeX_model_node_t *result = lie_pair(build, TeX_NODE_SUBSCRIPT,
        notation_build_literal(build, TeX_NODE_IDENTIFIER, name), value);
    string_free(number);
    return result;
}

/* Present borrowed native results without exposing typesetting syntax. */
bool de_lie_set_steps(diffequ_solve_result_t *result, expr_t *const *invariants, char *const *plain,
                      const matrix_t *generators, const equation_t *reduced)
{
    size_t count = generators ? mat_get_col_count(generators) : 0u;
    if (!result || !invariants || !plain || count > (SIZE_MAX - 6u) / 2u)
        return false;
    size_t capacity = 6u + 2u * count;
    const TeX_model_node_t **rows = calloc(capacity, sizeof *rows);
    notation_build_t *build = notation_build_new();
    string_t *steps = string_sprintf("Lie point-symmetry analysis of the normalised second-order equation.\n"
                                     "Computed Lie–Tressé invariants: I1 = %s; I2 = %s.\n"
                                     "Both must vanish identically for local point-linearisation.\n", plain[0], plain[1]);
    bool valid = rows && build && steps;
    size_t used = 0u;
    for (size_t i = 0u; valid && i < 2u; ++i)
        rows[used++] = lie_pair(build, TeX_NODE_EQUATION, lie_index(build, "I", i + 1u),
                                notation_build_expr(build, invariants[i]));
    if (valid && generators) {
        valid = string_append_format(steps, "Polynomial generator search (total degree <= 2): %zu verified.\n", count) >= 0;
        string_t *caption = string_sprintf("Polynomial search: total degree at most 2; %zu verified generators.", count);
        rows[used++] = caption ? notation_build_literal(build, TeX_NODE_TEXT, string_c_str(caption)) : NULL;
        string_free(caption);
        for (size_t j = 0u; valid && j < count; ++j) {
            for (size_t c = 0u; c < 2u; ++c) {
                const expr_t *entry = de_lie_entry(generators, c, j);
                char *component = entry ? expr_to_string(entry, style_UNBOUND) : NULL;
                valid = component && string_append_format(steps, "G%zu: %s = %s\n", j + 1u,
                                                          c ? "eta" : "xi", component) >= 0;
                free(component);
                rows[used++] = lie_pair(build, TeX_NODE_EQUATION, lie_index(build, c ? "η" : "ξ", j + 1u),
                                        notation_build_expr(build, entry));
            }
        }
        valid = valid && string_append_cstr(steps, "This finite search does not exhaust non-polynomial symmetries.\n") == 0;
        rows[used++] = notation_build_literal(build, TeX_NODE_TEXT,
            "Non-polynomial symmetries are not exhausted by this search.");
    } else if (valid) {
        valid = string_append_cstr(steps, "Polynomial coefficient search is unsupported for this normal form.\n") == 0;
        rows[used++] = notation_build_literal(build, TeX_NODE_TEXT, "Polynomial coefficient search is unsupported.");
    }
    if (valid && reduced) {
        string_t *text = equ_to_text(reduced, style_UNBOUND);
        valid = text && string_append_format(steps, "Translation symmetry gives the local order reduction: %s.\n"
            "Here the velocity is a function of the dependent coordinate; check equilibria separately in the original ODE.\n",
            string_c_str(text)) >= 0;
        string_free(text);
        const TeX_model_node_t *caption = notation_build_literal(build, TeX_NODE_TEXT, "Local order reduction:");
        const TeX_model_node_t *equation = lie_pair(build, TeX_NODE_EQUATION,
            notation_build_expr(build, equ_lhs(reduced)), notation_build_expr(build, equ_rhs(reduced)));
        rows[used++] = lie_pair(build, TeX_NODE_STATEMENTS, caption, equation);
        rows[used++] = notation_build_literal(build, TeX_NODE_TEXT, "Check equilibria separately in the original ODE.");
    }
    const TeX_model_node_t *root = valid ? notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = used}, rows) : NULL;
    string_t *rendered = root ? notation_build_render(build, root) : NULL;
    valid = rendered && de_solve_result_set_steps(result, string_c_str(steps)) == 0 &&
            de_solve_result_set_steps_TeX(result, string_c_str(rendered)) == 0;
    string_free(rendered);
    string_free(steps);
    notation_build_free(build);
    free(rows);
    return valid;
}
