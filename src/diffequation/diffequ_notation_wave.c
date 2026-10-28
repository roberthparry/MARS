/**
 * @file diffequ_notation_wave.c
 * @brief Project three-dimensional wave solutions and spherical means semantically.
 *
 * The native solver supplies the coordinates, arbitrary data and wave speed.
 * TeX renders the mathematical structures and never inspects the solver state.
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

static const TeX_model_node_t *pair(notation_build_t *build, TeX_node_kind_t kind,
                                   const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *children[] = {left, right};
    return notation_build_node(build, (TeX_node_view_t){.kind = kind, .child_count = 2u}, children);
}

static const TeX_model_node_t *single(notation_build_t *build, TeX_node_kind_t kind, const TeX_model_node_t *child)
{
    return notation_build_node(build, (TeX_node_view_t){.kind = kind, .child_count = 1u}, &child);
}

static const TeX_model_node_t *relation(notation_build_t *build, TeX_relation_t type,
                                       const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *children[] = {left, right};
    return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_RELATION,
                               .relation = type, .child_count = 2u}, children);
}

static const TeX_model_node_t *mean(notation_build_t *build, const TeX_model_node_t *radius,
                                   const TeX_model_node_t *function, const TeX_model_node_t *position)
{
    const TeX_model_node_t *m = single(build, TeX_NODE_CALLIGRAPHIC,
        notation_build_literal(build, TeX_NODE_IDENTIFIER, "M"));
    return pair(build, TeX_NODE_PRODUCT, pair(build, TeX_NODE_SUBSCRIPT, m, radius),
                pair(build, TeX_NODE_APPLY, function, position));
}

static const TeX_model_node_t *label(notation_build_t *build, const char *text, const TeX_model_node_t *value)
{
    return pair(build, TeX_NODE_STATEMENTS, notation_build_literal(build, TeX_NODE_TEXT, text), value);
}

static string_t *wave_render(expr_t *const *space, const expr_t *time, const expr_t *f, const expr_t *g,
                             const expr_t *c, const expr_t *speed, const expr_t *base,
                             const expr_t *dependent, bool steps)
{
    notation_build_t *build = notation_build_new();
    if (!build)
        return NULL;
    const TeX_model_node_t *position = single(build, TeX_NODE_BOLD,
        notation_build_literal(build, TeX_NODE_IDENTIFIER, "r"));
    const TeX_model_node_t *t = notation_build_expr(build, time);
    const TeX_model_node_t *fn = notation_build_expr(build, f), *gn = notation_build_expr(build, g);
    const TeX_model_node_t *cn = notation_build_expr(build, c), *speed_node = notation_build_expr(build, speed);
    const TeX_model_node_t *zero = notation_build_literal(build, TeX_NODE_NUMBER, "0");
    const TeX_model_node_t *one = notation_build_literal(build, TeX_NODE_NUMBER, "1");
    const TeX_model_node_t *radius = pair(build, TeX_NODE_PRODUCT, cn, t);
    const TeX_model_node_t *rows[12];
    size_t n = 0u;
    if (!steps) {
        const TeX_model_node_t *derivative_inputs[] = {pair(build, TeX_NODE_PRODUCT, t, mean(build, radius, fn, position)), t};
        const TeX_model_node_t *derivative = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_DERIVATIVE, .child_count = 2u, .order = 1u, .partial = true},
            derivative_inputs);
        rows[n++] = pair(build, TeX_NODE_SUM, derivative,
                          pair(build, TeX_NODE_PRODUCT, t, mean(build, radius, gn, position)));
        const TeX_model_node_t *coordinates[] = {notation_build_expr(build, space[0]),
            notation_build_expr(build, space[1]), notation_build_expr(build, space[2])};
        const TeX_model_node_t *list = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_LIST, .child_count = 3u}, coordinates);
        const TeX_model_node_t *tuple = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_FENCED, .child_count = 1u, .fence = TeX_FENCE_PARENTHESES}, &list);
        rows[n++] = pair(build, TeX_NODE_EQUATION, position, tuple);
        rows[n++] = pair(build, TeX_NODE_EQUATION, cn, speed_node);
        if (base) {
            const TeX_model_node_t *b = notation_build_expr(build, base);
            const TeX_model_node_t *reals = notation_build_node(build,
                (TeX_node_view_t){.kind = TeX_NODE_NUMBER_SET, .number_set = TeX_SET_REALS}, NULL);
            rows[n++] = pair(build, TeX_NODE_STATEMENTS, relation(build, TeX_RELATION_MEMBER, b, reals),
                              relation(build, TeX_RELATION_NOT_EQUAL, b, zero));
        } else {
            rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "real, non-zero wave speed");
        }
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "Spherical average at radius |a|.");
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "The initial field and velocity are arbitrary smooth spatial functions.");
    } else {
        const TeX_model_node_t *a = notation_build_literal(build, TeX_NODE_IDENTIFIER, "a");
        const TeX_model_node_t *H = notation_build_literal(build, TeX_NODE_FUNCTION_NAME, "H");
        const TeX_model_node_t *omega = single(build, TeX_NODE_BOLD,
            notation_build_literal(build, TeX_NODE_IDENTIFIER, "ω"));
        const TeX_model_node_t *norm = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_FENCED, .child_count = 1u, .fence = TeX_FENCE_ABSOLUTE}, &omega);
        const TeX_model_node_t *integrand = pair(build, TeX_NODE_APPLY, H,
            pair(build, TeX_NODE_SUM, position, pair(build, TeX_NODE_PRODUCT, a, omega)));
        const TeX_model_node_t *integral_children[] = {integrand, pair(build, TeX_NODE_EQUATION, norm, one),
            notation_build_literal(build, TeX_NODE_IDENTIFIER, "Ω")};
        const TeX_model_node_t *integral = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_INTEGRAL_CONDITION, .child_count = 3u}, integral_children);
        const TeX_model_node_t *four_pi = pair(build, TeX_NODE_PRODUCT,
            notation_build_literal(build, TeX_NODE_NUMBER, "4"), notation_build_literal(build, TeX_NODE_IDENTIFIER, "π"));
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "Kirchhoff's formula (three spatial dimensions)");
        rows[n++] = pair(build, TeX_NODE_EQUATION, mean(build, a, H, position),
                          pair(build, TeX_NODE_PRODUCT, pair(build, TeX_NODE_FRACTION, one, four_pi), integral));
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "Average over a sphere centred at r, radius |a|.");
        rows[n++] = pair(build, TeX_NODE_EQUATION, mean(build, zero, H, position), pair(build, TeX_NODE_APPLY, H, position));
        const TeX_model_node_t *u = notation_build_expr(build, dependent);
        const TeX_model_node_t *initial_args[] = {position, zero};
        const TeX_model_node_t *initial_u_children[] = {u, initial_args[0], initial_args[1]};
        const TeX_model_node_t *initial_u = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_APPLY, .child_count = 3u}, initial_u_children);
        const TeX_model_node_t *derivative_children[] = {u, t};
        const TeX_model_node_t *derivative = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_VARIABLE_DERIVATIVE, .child_count = 2u, .order = 1u, .partial = true},
            derivative_children);
        const TeX_model_node_t *initial_derivative_children[] = {derivative, initial_args[0], initial_args[1]};
        rows[n++] = pair(build, TeX_NODE_EQUATION, initial_u, pair(build, TeX_NODE_APPLY, fn, position));
        rows[n++] = pair(build, TeX_NODE_EQUATION, notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_APPLY, .child_count = 3u}, initial_derivative_children),
            pair(build, TeX_NODE_APPLY, gn, position));
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "Both functions are arbitrary; no initial data imposed.");
        rows[n++] = label(build, "Assumes finite non-zero constant coefficients and real positive speed:",
                          relation(build, TeX_RELATION_GREATER, cn, zero));
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "Whole space; no boundary conditions. Exact integral representation.");
    }
    const TeX_model_node_t *root = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = n}, rows);
    string_t *rendered = root ? notation_build_render(build, root) : NULL;
    notation_build_free(build);
    return rendered;
}

/* Retain the three-dimensional solution and optional explanation as independent strings. */
bool de_wave_notation(equation_t *solution, diffequ_solve_result_t *result, expr_t *const *space,
                      const expr_t *time, const expr_t *speed_squared, const expr_t *speed,
                      const expr_t *f, const expr_t *g, const diffequ_t *de, const expr_t *residual, bool include_steps)
{
    expr_t *speed_symbol = de_wave_fresh_symbol(de, residual, "c");
    const expr_t *base = de_wave_square_base(speed_squared);
    expr_t *display_speed = base ? expr_abs(base) : expr_clone(speed);
    char *speed_text = display_speed ? expr_to_string(display_speed, style_UNBOUND) : NULL;
    char *base_text = base ? expr_to_string(base, style_UNBOUND) : NULL;
    string_t *condition = base_text ? string_sprintf("; %s is real and non-zero (either sign)", base_text)
                                    : !base ? string_new_with(" (real, non-zero wave speed)") : NULL;
    const char *tn = expr_symbol_name(time), *fn = expr_symbol_name(f), *gn = expr_symbol_name(g);
    const char *cn = speed_symbol ? expr_symbol_name(speed_symbol) : NULL;
    const char *x = expr_symbol_name(space[0]), *y = expr_symbol_name(space[1]), *z = expr_symbol_name(space[2]);
    bool valid = solution && result && speed_symbol && display_speed && speed_text && condition && tn && fn && gn && cn && x && y && z;
    string_t *unbound = valid ? string_sprintf(
        "d/d%s[%s*M_(%s*%s) %s(r)] + %s*M_(%s*%s) %s(r)\n"
        "r = (%s, %s, %s); %s = %s%S.\n"
        "M_a H(r): average of H over the sphere centred at r, radius |a|.\n"
        "%s, %s: arbitrary smooth spatial functions.",
        tn, tn, cn, tn, fn, tn, cn, tn, gn, x, y, z, cn, speed_text, condition, fn, gn) : NULL;
    string_t *notation = valid ? wave_render(space, time, f, g, speed_symbol, display_speed, base, equ_lhs(solution), false) : NULL;
    valid = unbound && notation && equ_set_display_TeX(solution, NULL, notation) == 0 &&
            equ_set_display_unbound(solution, NULL, unbound) == 0;
    if (valid && include_steps) {
        string_t *plain = string_sprintf(
            "Kirchhoff's formula for the three-dimensional constant-speed wave equation.\n"
            "Time coordinate: %s; spatial coordinates: (%s, %s, %s).\n"
            "M_a H(r) = (1/(4*pi))*∫_{|omega|=1} H(r+a*omega) dOmega.\n"
            "This is the spherical average of H, centred at r with radius |a|; M_0 H = H.\n"
            "%s and %s are arbitrary initial field and initial time derivative; no initial data imposed.\n"
            "Require finite non-zero constant coefficients and a real positive squared speed.\n"
            "This is the general smooth whole-space family, with no boundary conditions imposed.\n"
            "It is an exact integral representation, not a finite elementary closed form.\n", tn, x, y, z, fn, gn);
        string_t *steps_notation = wave_render(space, time, f, g, speed_symbol, display_speed, base, equ_lhs(solution), true);
        valid = plain && steps_notation && de_solve_result_set_steps(result, string_c_str(plain)) == 0 &&
                de_solve_result_set_steps_TeX(result, string_c_str(steps_notation)) == 0;
        string_free(steps_notation); string_free(plain);
    }
    string_free(notation); string_free(unbound); string_free(condition);
    free(base_text); free(speed_text); expr_free(display_speed); expr_free(speed_symbol);
    return valid;
}
