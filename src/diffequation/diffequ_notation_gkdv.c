/**
 * @file diffequ_notation_gkdv.c
 * @brief Prepare generalised travelling-wave qualifications and derivation rows.
 *
 * The differential-equation module supplies the verified family, root selection
 * and excluded singularity. Presentation receives semantic relations and native
 * formula snapshots, with no solver structures or typesetting strings.
 */
#include <stdlib.h>

#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"

static const TeX_model_node_t *relation(notation_build_t *build, TeX_relation_t kind,
                                       const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *parts[] = {left, right};
    return notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_RELATION, .relation = kind, .child_count = 2u}, parts);
}

/* Retain the positive-real-root and moving-singularity restrictions with the solution. */
bool de_gkdv_present(equation_t *solution, const expr_t *n, const expr_t *v, const expr_t *position,
                     const expr_t *z, bool singular)
{
    const expr_t *values[] = {equ_lhs(solution), equ_rhs(solution), n, v, position, z};
    char *plain[6] = {0};
    bool valid = true;
    for (size_t i = 0u; i < 6u; ++i) {
        plain[i] = expr_to_string(values[i], style_UNBOUND);
        valid = valid && plain[i];
    }
    string_t *left = valid ? string_sprintf("%s travelling-wave family (not the general solution): %s",
                                             singular ? "Singular" : "Solitary", plain[0]) : NULL;
    string_t *right = valid ? string_sprintf("%s; %s is a positive integer; %s > 0, %s real; positive real root%s%s%s.",
        plain[1], plain[2], plain[3], plain[4], singular ? "; " : "",
        singular ? plain[5] : "", singular ? " != 0 (moving singularity excluded)" : "") : NULL;
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t *zero = notation_build_literal(build, TeX_NODE_NUMBER, "0");
    const TeX_model_node_t *order = notation_build_expr(build, n);
    const TeX_model_node_t *integers = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_NUMBER_SET, .number_set = TeX_SET_INTEGERS}, NULL);
    const TeX_model_node_t *reals = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_NUMBER_SET, .number_set = TeX_SET_REALS}, NULL);
    const TeX_model_node_t *conditions[] = {
        relation(build, TeX_RELATION_MEMBER, order, integers),
        relation(build, TeX_RELATION_GREATER, order, zero),
        relation(build, TeX_RELATION_GREATER, notation_build_expr(build, v), zero),
        relation(build, TeX_RELATION_MEMBER, notation_build_expr(build, position), reals)
    };
    const TeX_model_node_t *exclusion[] = {
        relation(build, TeX_RELATION_NOT_EQUAL, notation_build_expr(build, z), zero),
        notation_build_literal(build, TeX_NODE_TEXT, "(moving singularity excluded)")
    };
    const TeX_model_node_t *rows[] = {
        notation_build_expr(build, equ_rhs(solution)),
        notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_STATEMENTS, .child_count = 4u}, conditions),
        notation_build_literal(build, TeX_NODE_TEXT, singular
            ? "Positive real root; singular travelling-wave family, not the general solution."
            : "Positive real root; solitary travelling-wave family, not the general solution."),
        notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_STATEMENTS, .child_count = 2u}, exclusion)
    };
    const TeX_model_node_t *body = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = singular ? 4u : 3u}, rows);
    valid = de_notation_display(solution, left, right, build, body);
    notation_build_free(build);
    string_free(right);
    string_free(left);
    for (size_t i = 0u; i < 6u; ++i)
        free(plain[i]);
    return valid;
}

/* Describe the verified identities using the native formula grammar. */
bool de_gkdv_set_steps(diffequ_solve_result_t *result)
{
    static const char plain[] =
        "Write A*u_t+B*u^n*u_x+C*u_xxx=0 and g=2*B/(C*(n+1)*(n+2)).\n"
        "Assume n is a positive integer and use positive real roots.\n"
        "Set u=V(z), z=x-(C/A)*v*t-x0, with v>0.\n"
        "Integrate twice with zero integration constants: (V')^2=v*V^2-g*V^(n+2).\n"
        "For g>0, V^n=(v/g)*sech(n*sqrt(v)*z/2)^2 is a smooth pulse.\n"
        "For g<0, V^n=(-v/g)*csch(n*sqrt(v)*z/2)^2 is singular at z=0.\n"
        "Differentiation gives V''=v*V-(g*(n+2)/2)*V^(n+1), proving the original PDE.\n"
        "These are particular real travelling-wave families, not the general solution. No initial data are imposed.";
    static const de_notation_step_t rows[] = {
        {NULL, "A*Dt(u)+B*u^n*Dx(u)+C*Dxxx(u)", "0"},
        {NULL, "g", "2*B/(C*(n+1)*(n+2))"},
        {"n is a positive integer; v is positive. Use the positive real root.", NULL, NULL},
        {NULL, "u", "V(z)"},
        {NULL, "z", "x-(C/A)*v*t-x0"},
        {NULL, "Dz(V)^2", "v*V^2-g*V^(n+2)"},
        {NULL, "Dzz(V)", "v*V-(g*(n+2)/2)*V^(n+1)"},
        {"For g > 0:", "V^n", "(v/g)*sech(n*sqrt(v)*z/2)^2"},
        {"For g < 0:", "V^n", "(-v/g)*csch(n*sqrt(v)*z/2)^2"},
        {"The singular branch excludes z = 0, the moving pole.", NULL, NULL},
        {"Particular travelling-wave families; not the general solution.", NULL, NULL}
    };
    return de_notation_steps(result, plain, rows, sizeof rows / sizeof *rows);
}
