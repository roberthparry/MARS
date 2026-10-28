/**
 * @file diffequ_notation_wave_ivp.c
 * @brief Project one-dimensional wave initial-value solutions into semantic notation.
 *
 * The solver selects nonzero displacement, velocity and forcing contributions;
 * TeX presents the resulting mathematical expressions without solver internals.
 */
#include <stdlib.h>

#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

/* Keep the original nonzero terms, but omit contributions proven to vanish. */
bool de_wave_ivp_notation(equation_t *solution, const expr_t *displacement, const expr_t *velocity,
                          const expr_t *source, const expr_t *twice_speed)
{
    expr_t *terms[] = {expr_clone(displacement), expr_div(velocity, twice_speed), expr_div(source, twice_speed)};
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t *root = NULL;
    bool valid = solution && build;
    for (size_t i = 0u; valid && i < sizeof terms / sizeof *terms; ++i) {
        expr_t *reduced = terms[i] ? expr_simplify(terms[i]) : NULL;
        if (!reduced) {
            valid = false;
            break;
        }
        bool zero = expr_is_exact_zero(reduced);
        expr_free(reduced);
        if (zero)
            continue;
        const TeX_model_node_t *term = notation_build_expr(build, terms[i]);
        const TeX_model_node_t *children[] = {root, term};
        root = root ? notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_SUM, .child_count = 2u}, children)
                    : term;
        valid = root != NULL;
    }
    if (valid && !root)
        root = notation_build_literal(build, TeX_NODE_NUMBER, "0");
    string_t *display = valid && root ? notation_build_render(build, root) : NULL;
    valid = display && equ_set_display_TeX(solution, NULL, display) == 0;
    string_free(display);
    notation_build_free(build);
    for (size_t i = 0u; i < sizeof terms / sizeof *terms; ++i)
        expr_free(terms[i]);
    return valid;
}

static const TeX_model_node_t *wave_pair(notation_build_t *build, TeX_node_kind_t kind,
                                        const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *children[] = {left, right};
    return notation_build_node(build, (TeX_node_view_t){.kind = kind, .child_count = 2u}, children);
}

static const TeX_model_node_t *wave_relation(notation_build_t *build, TeX_relation_t relation,
                                            const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *children[] = {left, right};
    return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_RELATION,
                               .relation = relation, .child_count = 2u}, children);
}

/* Present the assumptions and interpretation of the exact one-dimensional solution. */
bool de_wave_ivp_steps(diffequ_solve_result_t *result, const expr_t *space, const expr_t *time,
                       const expr_t *initial_time, const expr_t *speed_squared, const expr_t *speed)
{
    const expr_t *base = de_wave_square_base(speed_squared);
    expr_t *display_speed = base ? expr_abs(base) : expr_clone(speed);
    char *speed_text = display_speed ? expr_to_string(display_speed, style_UNBOUND) : NULL;
    char *base_text = base ? expr_to_string(base, style_UNBOUND) : NULL;
    string_t *condition = base_text ? string_sprintf("; %s is real and non-zero (either sign)", base_text)
                                    : !base ? string_new_with(" (real, non-zero wave speed)") : NULL;
    const char *space_name = expr_symbol_name(space), *time_name = expr_symbol_name(time);
    string_t *plain = result && speed_text && condition && space_name && time_name ? string_sprintf(
        "d'Alembert's formula plus Duhamel's principle (one spatial dimension).\n"
        "Space: %s; time: %s. Wave speed: %s%S.\n"
        "The two supplied data are the displacement and time derivative on the same initial slice.\n"
        "Translate the displacement in both directions and average.\n"
        "Integrate the initial velocity between the two characteristic endpoints, with coefficient 1/(2*speed).\n"
        "Integrate the normalised forcing over the past characteristic triangle with the same coefficient.\n"
        "The forcing contribution and its first time derivative vanish on the initial slice.\n"
        "Use verified elementary primitives where available; otherwise retain exact definite integrals.\n"
        "Assumes sufficiently smooth data, finite constant coefficients, real positive squared speed,\n"
        "and a real initial time. Whole spatial line; no boundary conditions imposed.",
        space_name, time_name, speed_text, condition) : NULL;
    notation_build_t *build = plain ? notation_build_new() : NULL;
    string_t *display = NULL;
    if (build) {
        const TeX_model_node_t *x = notation_build_expr(build, space);
        const TeX_model_node_t *t = notation_build_expr(build, time);
        const TeX_model_node_t *t0 = notation_build_expr(build, initial_time);
        const TeX_model_node_t *c = notation_build_expr(build, display_speed);
        const TeX_model_node_t *rows[16];
        size_t n = 0u;
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "d'Alembert + Duhamel (one spatial dimension)");
        rows[n++] = wave_pair(build, TeX_NODE_STATEMENTS, notation_build_literal(build, TeX_NODE_TEXT, "Space:"), x);
        rows[n++] = wave_pair(build, TeX_NODE_STATEMENTS, notation_build_literal(build, TeX_NODE_TEXT, "Time:"), t);
        rows[n++] = wave_pair(build, TeX_NODE_STATEMENTS, notation_build_literal(build, TeX_NODE_TEXT, "Initial time:"), t0);
        rows[n++] = wave_pair(build, TeX_NODE_STATEMENTS, notation_build_literal(build, TeX_NODE_TEXT, "Wave speed:"), c);
        if (base) {
            const TeX_model_node_t *value = notation_build_expr(build, base);
            const TeX_model_node_t *set = notation_build_node(build,
                (TeX_node_view_t){.kind = TeX_NODE_NUMBER_SET, .number_set = TeX_SET_REALS}, NULL);
            const TeX_model_node_t *zero = notation_build_literal(build, TeX_NODE_NUMBER, "0");
            rows[n++] = wave_pair(build, TeX_NODE_STATEMENTS, wave_relation(build, TeX_RELATION_MEMBER, value, set),
                                  wave_relation(build, TeX_RELATION_NOT_EQUAL, value, zero));
        } else {
            rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "Real, non-zero wave speed.");
        }
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "Average the two translated initial displacements.");
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "Add the initial-velocity integral between the characteristic endpoints.");
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "Add the forcing integral over the past characteristic triangle.");
        const TeX_model_node_t *two = notation_build_literal(build, TeX_NODE_NUMBER, "2");
        const TeX_model_node_t *one = notation_build_literal(build, TeX_NODE_NUMBER, "1");
        rows[n++] = wave_pair(build, TeX_NODE_STATEMENTS, notation_build_literal(build, TeX_NODE_TEXT, "Each integral has coefficient"),
            wave_pair(build, TeX_NODE_FRACTION, one, wave_pair(build, TeX_NODE_PRODUCT, two, c)));
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "The forcing term and its first time derivative initially vanish.");
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "Verified elementary primitives, or exact definite integrals.");
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "Assumes smooth data, real initial time and finite constant coefficients.");
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "Whole spatial line; no boundary conditions imposed.");
        const TeX_model_node_t *root = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = n}, rows);
        display = notation_build_render(build, root);
    }
    bool valid = plain && display && de_solve_result_set_steps(result, string_c_str(plain)) == 0 &&
                 de_solve_result_set_steps_TeX(result, string_c_str(display)) == 0;
    string_free(display); notation_build_free(build); string_free(plain); string_free(condition);
    free(base_text); free(speed_text); expr_free(display_speed);
    return valid;
}
