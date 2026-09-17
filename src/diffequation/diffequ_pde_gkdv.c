#include <stdlib.h>
#include <string.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"

static const expr_t *de_gkdv_power(const expr_t *expr, const expr_t *field, expr_t **exponent)
{
    const expr_t *base = NULL, *left = NULL, *right = NULL, *symbolic = NULL;
    if (expr_match_pow_expr(expr, &base, &symbolic) && expr_struct_eq(base, field)) {
        *exponent = expr_clone(symbolic);
        return expr;
    }
    number_t numeric = num_new();
    bool power = expr_match_pow_const(expr, &base, &numeric) && expr_struct_eq(base, field);
    if (power)
        *exponent = expr_new_const(numeric);
    num_destroy(&numeric);
    if (power)
        return expr;
    if (!expr_child_exprs(expr, &left, &right))
        return NULL;
    const expr_t *found = de_gkdv_power(left, field, exponent);
    return found ? found : de_gkdv_power(right, field, exponent);
}

/* A symbolic exponent is conditional on the displayed positive-integer assumption, never silently specialised. */
static bool de_gkdv_exponent(const expr_t *n, const expr_t *field, const expr_t *time, const expr_t *space)
{
    if (!n || de_expr_uses(n, field) || de_expr_uses(n, time) || de_expr_uses(n, space))
        return false;
    number_t value = expr_eval(n);
    bool valid = (num_is_real(value) && num_is_finite(value) && num_is_integer(value) && num_gt(value, NUM_ZERO)) ||
                 (num_is_nan(value) && expr_symbol_name(n) && (expr_is_variable(n) || expr_is_named_const(n)));
    num_destroy(&value);
    return valid;
}

/* Prove the normalised coefficient is a known nonzero real, including factored/expanded symbolic n forms. */
static expr_t *de_gkdv_scale(const expr_t *coefficient, const expr_t *c, const expr_t *n)
{
    expr_t *normal = expr_div_simplify_owned(expr_mul_simplify_owned(expr_clone(c),
        expr_mul_simplify_owned(expr_add_simplify_owned(expr_clone(n), expr_const_one()),
                                expr_add_simplify_owned(expr_clone(n), expr_const_long(2L)))), expr_const_long(2L));
    expr_t *ratio = normal ? expr_div_simplify_owned(expr_clone(coefficient), expr_clone(normal)) : NULL;
    if (ratio) {
        number_t value = expr_eval(ratio);
        bool known = num_is_real(value) && num_is_finite(value) && !num_is_zero(value);
        num_destroy(&value);
        if (!known) {
            expr_t *difference = expr_sub(coefficient, normal), *sum = expr_add(coefficient, normal);
            expr_free(ratio);
            ratio = de_pde_is_symbolically_zero(difference) ? expr_const_one() :
                    de_pde_is_symbolically_zero(sum) ? expr_const_long(-1L) : NULL;
            expr_free(sum); expr_free(difference);
        }
    }
    expr_free(normal);
    return ratio;
}

static bool de_gkdv_present(equation_t *solution, const expr_t *n, const expr_t *v, const expr_t *position,
                            const expr_t *z, bool singular)
{
    const expr_t *values[] = {equ_lhs(solution), equ_rhs(solution), n, v, position, z};
    char *plain[6] = {NULL}, *TeX[6] = {NULL};
    bool valid = false;
    string_t *lhs = NULL, *rhs = NULL, *lhs_TeX = NULL, *rhs_TeX = NULL;
    for (size_t i = 0u; i < 6u; ++i) {
        plain[i] = expr_to_string(values[i], style_UNBOUND);
        TeX[i] = expr_to_TeX_body(values[i]);
        if (!plain[i] || !TeX[i])
            goto cleanup;
    }
    lhs = string_sprintf("%s travelling-wave family (not the general solution): %s",
                          singular ? "Singular" : "Solitary", plain[0]);
    rhs = string_sprintf("%s; %s is a positive integer; %s > 0, %s real; positive real root%s%s%s.",
        plain[1], plain[2], plain[3], plain[4], singular ? "; " : "",
        singular ? plain[5] : "", singular ? " != 0 (moving singularity excluded)" : "");
    lhs_TeX = string_sprintf("%s", TeX[0]);
    rhs_TeX = string_sprintf(
        "\\begin{aligned}[t]&%s\\\\&%s\\in\\mathbb{Z}_{>0},\\quad %s>0,\\quad %s\\in\\mathbb{R}"
        "\\\\&\\text{Positive real root; %s travelling-wave family, not the general solution.}%s%s%s\\end{aligned}",
        TeX[1], TeX[2], TeX[3], TeX[4], singular ? "singular" : "solitary",
        singular ? "\\\\&" : "", singular ? TeX[5] : "",
        singular ? "\\ne0\\quad\\text{(moving singularity excluded)}" : "");
    valid = lhs && rhs && lhs_TeX && rhs_TeX && equ_set_display_unbound(solution, lhs, rhs) == 0 &&
            equ_set_display_TeX(solution, lhs_TeX, rhs_TeX) == 0;
cleanup:
    string_free(rhs_TeX); string_free(lhs_TeX); string_free(rhs); string_free(lhs);
    for (size_t i = 0u; i < 6u; ++i) { free(TeX[i]); free(plain[i]); }
    return valid;
}

/* Solve the zero-energy travelling-wave reduction of A*u_t+B*u^n*u_x+C*u_xxx=0. */
diffequ_solve_result_t *de_pde_solve_gkdv(const diffequ_t *de, const expr_t *residual, const expr_t *field,
    const expr_t *time, const expr_t *space, const expr_t *a, const expr_t *c, const expr_t *nonlinear,
    bool include_steps)
{
    expr_t *n = NULL;
    const expr_t *power = de_gkdv_power(nonlinear, field, &n);
    expr_t *coefficient = NULL, *rest = NULL, *scale = NULL, *v = NULL, *position = NULL;
    expr_t *z = NULL, *phase = NULL, *profile = NULL, *right = NULL;
    equation_t *solution = NULL;
    diffequ_solve_result_t *result = NULL;
    if (!power || !de_gkdv_exponent(n, field, time, space) ||
        !de_linear_decompose(nonlinear, power, &coefficient, &rest) || !de_pde_is_symbolically_zero(rest) ||
        de_expr_uses(coefficient, field) || de_expr_uses(coefficient, time) || de_expr_uses(coefficient, space))
        goto cleanup;
    scale = de_gkdv_scale(coefficient, c, n);
    if (!scale)
        goto cleanup;
    number_t scale_value = expr_eval(scale);
    bool singular = num_lt(scale_value, NUM_ZERO);
    num_destroy(&scale_value);
    v = de_pde_kdv_parameter(de, residual, "v");
    position = de_pde_kdv_parameter(de, residual, "x0");
    expr_t *speed = v ? expr_mul_simplify_owned(expr_div(c, a), expr_clone(v)) : NULL;
    z = speed && position ? expr_sub_simplify_owned(expr_sub(space, position), expr_mul(speed, time)) : NULL;
    expr_free(speed);
    expr_t *root = v ? expr_sqrt(v) : NULL;
    phase = z && root ? expr_mul_simplify_owned(expr_div_simplify_owned(expr_mul(n, root), expr_const_long(2L)),
                                               expr_clone(z)) : NULL;
    expr_free(root);
    profile = phase ? (singular ? expr_cosech(phase) : expr_sech(phase)) : NULL;
    expr_t *magnitude = singular ? expr_neg(scale) : expr_clone(scale);
    expr_t *base = profile ? expr_mul_simplify_owned(expr_div_simplify_owned(expr_clone(v), expr_clone(magnitude)),
                                                    expr_pow_long(profile, 2L)) : NULL;
    expr_t *inverse_n = expr_div_simplify_owned(expr_const_one(), expr_clone(n));
    right = base && inverse_n ? expr_pow_xp(base, inverse_n) : NULL;
    expr_free(inverse_n); expr_free(base); expr_free(magnitude);
    solution = right ? de_pde_solution_equation(field, right) : NULL;
    if (!solution || !de_gkdv_present(solution, n, v, position, z, singular))
        goto cleanup;
    result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_GKDV_TRAVELLING_WAVE,
        singular ? "singular real travelling-wave family, not the general solution; positive-integer exponent; "
                   "positive real root, positive wave parameter, real position; exclude the moving singularity" :
                   "positive real solitary-wave family, not the general solution; positive-integer exponent; "
                   "positive real root, positive wave parameter and real position");
    if (!result)
        goto cleanup;
    if (include_steps && (de_solve_result_set_steps(result,
        "Write A*u_t+B*u^n*u_x+C*u_xxx=0 and g=2*B/(C*(n+1)*(n+2)).\n"
        "Assume n is a positive integer and use positive real roots.\n"
        "Set u=V(z), z=x-(C/A)*v*t-x0, with v>0.\n"
        "Integrate twice with zero integration constants: (V')^2=v*V^2-g*V^(n+2).\n"
        "For g>0, V^n=(v/g)*sech(n*sqrt(v)*z/2)^2 is a smooth pulse.\n"
        "For g<0, V^n=(-v/g)*csch(n*sqrt(v)*z/2)^2 is singular at z=0.\n"
        "Differentiation gives V''=v*V-(g*(n+2)/2)*V^(n+1), proving the original PDE.\n"
        "These are particular real travelling-wave families, not the general solution. No initial data are imposed.") != 0 ||
        de_solve_result_set_steps_TeX(result,
        "\\begin{aligned}&A u_t+B u^n u_x+C u_{xxx}=0,\\quad g=\\tfrac{2B}{C(n+1)(n+2)}"
        "\\\\&n\\in\\mathbb{Z}_{>0},\\quad v>0,\\quad u=V(z),\\quad z=x-\\tfrac{C}{A}vt-x_0"
        "\\\\&(V')^2=vV^2-gV^{n+2},\\quad V''=vV-\\tfrac{g(n+2)}{2}V^{n+1}"
        "\\\\&g>0:\\quad V^n=\\tfrac{v}{g}\\operatorname{sech}^2(\\tfrac{n\\sqrt{v}}{2}z)"
        "\\\\&g<0:\\quad V^n=-\\tfrac{v}{g}\\operatorname{csch}^2(\\tfrac{n\\sqrt{v}}{2}z),\\quad z\\ne0"
        "\\\\&\\text{Use the positive real root; singular branch excludes the moving pole.}"
        "\\\\&\\text{Particular travelling-wave families; not the general solution.}\\end{aligned}") != 0))
        goto failure;
    if (de_solve_result_append(result, solution) != 0)
        goto failure;
    solution = NULL;
    goto cleanup;
failure:
    de_solve_result_free(result); result = NULL;
cleanup:
    equ_free(solution); expr_free(right); expr_free(profile); expr_free(phase); expr_free(z);
    expr_free(position); expr_free(v); expr_free(scale); expr_free(rest); expr_free(coefficient); expr_free(n);
    return result;
}
