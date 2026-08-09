#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

diffequ_solve_result_t *de_solve_result_new(de_solve_status_t status, de_solver_t solver, const char *diagnostic)
{
    diffequ_solve_result_t *result = calloc(1u, sizeof(*result));

    if (!result)
        return NULL;
    result->status = status;
    result->solver = solver;
    if (diagnostic) {
        result->diagnostic = strdup(diagnostic);
        if (!result->diagnostic) {
            free(result);
            return NULL;
        }
    }
    return result;
}

int de_solve_result_append(diffequ_solve_result_t *result, equation_t *solution)
{
    equation_t **solutions;

    if (!result || !solution)
        return -1;

    solutions = realloc(result->solutions, (result->solution_count + 1u) * sizeof(*solutions));
    if (!solutions)
        return -1;

    result->solutions = solutions;
    result->solutions[result->solution_count++] = solution;
    return 0;
}

int de_solve_result_set_steps(diffequ_solve_result_t *result, const char *steps)
{
    char *copy;

    if (!result || !steps)
        return -1;
    copy = strdup(steps);
    if (!copy)
        return -1;
    free(result->steps);
    result->steps = copy;
    return 0;
}

int de_solve_result_set_symmetry(diffequ_solve_result_t *result, const char *symmetry)
{
    char *copy;

    if (!result || !symmetry)
        return -1;
    copy = strdup(symmetry);
    if (!copy)
        return -1;
    free(result->symmetry);
    result->symmetry = copy;
    return 0;
}

int de_solve_result_set_steps_tex(diffequ_solve_result_t *result, const char *steps_tex)
{
    char *copy;

    if (!result || !steps_tex)
        return -1;
    copy = strdup(steps_tex);
    if (!copy)
        return -1;
    free(result->steps_tex);
    result->steps_tex = copy;
    return 0;
}

static const char *de_solver_rule_name(de_solver_t solver)
{
    switch (solver) {
        case DE_SOLVER_SEPARABLE:
            return "first-order separable rule";
        case DE_SOLVER_LINEAR:
            return "first-order linear integrating-factor rule";
        case DE_SOLVER_BERNOULLI:
            return "Bernoulli substitution rule";
        case DE_SOLVER_HOMOGENEOUS:
            return "first-order homogeneous substitution rule";
        case DE_SOLVER_LINEAR_SUBSTITUTION:
            return "affine-combination substitution rule";
        case DE_SOLVER_LINEAR_TRANSFORMATION:
            return "linear change-of-variables rule";
        case DE_SOLVER_STURM_LIOUVILLE:
            return "Sturm–Liouville reduction rule";
        case DE_SOLVER_POWER_LAW_BESSEL:
            return "power-law Bessel reduction rule";
        case DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR:
            return "constant-coefficient characteristic-polynomial rule";
        case DE_SOLVER_DERIVATIVE_QUADRATIC:
            return "autonomous derivative-quadratic rule";
        case DE_SOLVER_EXACT_DERIVATIVE_LINEARIZATION:
            return "exact-derivative linearisation rule";
        case DE_SOLVER_CONSTANT_COEFFICIENT_TRANSPORT:
            return "constant-coefficient transport characteristic rule";
        case DE_SOLVER_CHARACTERISTICS:
            return "method of characteristics";
        case DE_SOLVER_PARAMETER_LINEAR_PDE:
            return "parameter-dependent linear integrating-factor rule";
        case DE_SOLVER_STATIONARY_EIGENFUNCTION:
            return "stationary-eigenfunction evolution rule";
        case DE_SOLVER_EXACT_FIRST_ORDER:
            return "exact first-order differential rule";
        case DE_SOLVER_LAPLACE:
            return "Laplace harmonic-function rule";
        case DE_SOLVER_NONE:
        default:
            return "symbolic differential-equation rule";
    }
}

static const char *de_solver_rule_plain(de_solver_t solver)
{
    switch (solver) {
        case DE_SOLVER_SEPARABLE:
            return "Isolate y′ = X(x)Y(y), then integrate "
                   "∫dy/Y(y) = ∫X(x)dx + C.";
        case DE_SOLVER_LINEAR:
        case DE_SOLVER_PARAMETER_LINEAR_PDE:
            return "Write u_s + P(s)u = Q(s). Form "
                   "μ = exp(∫P(s)ds), integrate (μu)_s = μQ, "
                   "then divide by μ.";
        case DE_SOLVER_BERNOULLI:
            return "Write y′ + P(x)y = Q(x)y^n and set "
                   "v = y^(1−n), reducing the equation to a linear one in v.";
        case DE_SOLVER_HOMOGENEOUS:
            return "Write y′ = F(y/x), set v = y/x, and use "
                   "y′ = v + xv′ before separating v and x.";
        case DE_SOLVER_LINEAR_SUBSTITUTION:
            return "Identify the affine combination u = ax + by + c, "
                   "replace u′ by a + by′, and solve the reduced separable equation.";
        case DE_SOLVER_LINEAR_TRANSFORMATION:
            return "Construct the nonsingular linearising coordinates from "
                   "the parsed coefficients, transform the derivatives, and solve "
                   "the reduced equation.";
        case DE_SOLVER_STURM_LIOUVILLE:
            return "Normalise the second-order linear equation, construct its "
                   "self-adjoint or factorised basis, and determine constants from "
                   "the supplied conditions.";
        case DE_SOLVER_POWER_LAW_BESSEL:
            return "For y″ + a*x^m*y = b*x^n, set ν = 1/(m+2), "
                   "p = (m+2)/2, z = sqrt(a)*x^p/p, and y = sqrt(x)*u(z). "
                   "For monomial forcing b != 0, set "
                   "μ = (2n-m+1)/(m+2) and "
                   "K = b*(p/sqrt(a))^(μ+1)/p^2. The transformed equation "
                   "z^2*u″ + z*u′ + (z^2-ν^2)*u = K*z^(μ+1) has the "
                   "Lommel particular K*s_(μ,ν)(z).";
        case DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR:
            return "Form the characteristic polynomial P(m), generate one "
                   "basis term for each root and multiplicity, then add a verified "
                   "particular solution for the forcing.";
        case DE_SOLVER_DERIVATIVE_QUADRATIC:
            return "Set p = dy/dx, solve the quadratic relation for p(y), "
                   "then integrate dx/dy = 1/p(y), retaining every branch.";
        case DE_SOLVER_EXACT_DERIVATIVE_LINEARIZATION:
            return "Recognise and integrate the total derivative once, apply "
                   "the derived logarithmic substitution, then solve the resulting "
                   "linear recurrence.";
        case DE_SOLVER_CONSTANT_COEFFICIENT_TRANSPORT:
            return "Integrate the characteristic system dx_j/ds = a_j, carry "
                   "the boundary data along those curves, and integrate the forcing.";
        case DE_SOLVER_CHARACTERISTICS:
            return "Build dx/a = dy/b = du/(f−cu), derive first integrals "
                   "of that characteristic system, and express the arbitrary data "
                   "as a function of the invariants.";
        case DE_SOLVER_STATIONARY_EIGENFUNCTION:
            return "Apply the parsed spatial operator H to the supplied "
                   "initial state ψ₀. When Hψ₀ = Eψ₀, evolve it by "
                   "the derived exponential time factor.";
        case DE_SOLVER_EXACT_FIRST_ORDER:
            return "Write M(x,y)dx + N(x,y)dy = 0, verify M_y = N_x, "
                   "construct a potential Φ with Φ_x=M and Φ_y=N, then set Φ=C.";
        case DE_SOLVER_LAPLACE:
            return "Identify the Laplacian in the parsed coordinates and build "
                   "the harmonic family from conjugate characteristic coordinates.";
        case DE_SOLVER_NONE:
        default:
            return "Apply the selected symbolic rule to the parsed expression tree.";
    }
}

static const char *de_solver_rule_tex(de_solver_t solver)
{
    switch (solver) {
        case DE_SOLVER_SEPARABLE:
            return "y'=X(x)Y(y),\\quad "
                   "\\int\\frac{dy}{Y(y)}=\\int X(x)\\,dx+C";
        case DE_SOLVER_LINEAR:
        case DE_SOLVER_PARAMETER_LINEAR_PDE:
            return "u_s+P(s)u=Q(s),\\quad "
                   "\\mu=e^{\\int P(s)\\,ds},\\quad (\\mu u)_s=\\mu Q";
        case DE_SOLVER_BERNOULLI:
            return "y'+P(x)y=Q(x)y^n,\\quad v=y^{1-n}";
        case DE_SOLVER_HOMOGENEOUS:
            return "v=\\frac{y}{x},\\quad y'=v+xv'";
        case DE_SOLVER_LINEAR_SUBSTITUTION:
            return "u=ax+by+c,\\quad u'=a+by'";
        case DE_SOLVER_LINEAR_TRANSFORMATION:
            return "(X,Y)=(ax+by,cx+dy),\\quad ad-bc\\ne0";
        case DE_SOLVER_STURM_LIOUVILLE:
            return "(p(x)y')'+q(x)y=w(x)f(x)";
        case DE_SOLVER_POWER_LAW_BESSEL:
            return "y''+ax^m y=bx^n,\\quad "
                   "\\nu=\\frac1{m+2},\\quad "
                   "p=\\frac{m+2}{2},\\quad "
                   "z=\\frac{\\sqrt{a}}{p}x^p,\\quad "
                   "y=\\sqrt{x}\\,u(z),\\quad "
                   "\\mu=\\frac{2n-m+1}{m+2},\\quad "
                   "K=\\frac{b}{p^2}"
                   "\\left(\\frac{p}{\\sqrt{a}}\\right)^{\\mu+1},\\quad "
                   "z^2u''+zu'+(z^2-\\nu^2)u=Kz^{\\mu+1}";
        case DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR:
            return "P(D)y=f(x),\\quad P(m)=0";
        case DE_SOLVER_DERIVATIVE_QUADRATIC:
            return "p=y',\\quad A(y)p^2+B(y)p+D(y)=0,\\quad "
                   "\\frac{dx}{dy}=\\frac1p";
        case DE_SOLVER_EXACT_DERIVATIVE_LINEARIZATION:
            return "\\frac{d}{dx}G(y,y',y'')=f(x)";
        case DE_SOLVER_CONSTANT_COEFFICIENT_TRANSPORT:
            return "\\frac{dx_j}{ds}=a_j,\\quad "
                   "\\frac{du}{ds}=f-cu";
        case DE_SOLVER_CHARACTERISTICS:
            return "\\frac{dx}{a}=\\frac{dy}{b}="
                   "\\frac{du}{f-cu}";
        case DE_SOLVER_STATIONARY_EIGENFUNCTION:
            return "H\\psi_0=E\\psi_0,\\quad "
                   "\\psi(t)=\\psi_0e^{-iE(t-t_0)}";
        case DE_SOLVER_EXACT_FIRST_ORDER:
            return "M\\,dx+N\\,dy=0,\\quad M_y=N_x,\\quad "
                   "\\Phi_x=M,\\;\\Phi_y=N";
        case DE_SOLVER_LAPLACE:
            return "\\Delta u=0";
        case DE_SOLVER_NONE:
        default:
            return "\\mathcal R(E)=0";
    }
}

int de_solve_result_ensure_rule_steps(const diffequ_t *de, diffequ_solve_result_t *result)
{
    char *problem = NULL;
    char *problem_tex = NULL;
    string_t *steps = NULL;
    string_t *steps_tex = NULL;
    bool success = false;

    if (!result || result->status != DE_SOLVE_STATUS_SOLVED || result->solution_count == 0u)
        return 0;
    if (result->steps && result->steps_tex)
        return 0;

    problem = de_to_string(de, style_EXPRESSION);
    problem_tex = de_to_string(de, style_TEX);
    steps = string_new();
    steps_tex = string_new();
    if (!problem || !problem_tex || !steps || !steps_tex ||
        string_append_format(steps,
                             "Recognise the %s.\nParsed equation:\n      %s\nRule:\n"
                             "      %s\nDerived solution%s:\n",
                             de_solver_rule_name(result->solver), problem, de_solver_rule_plain(result->solver),
                             result->solution_count == 1u ? "" : "s") < 0 ||
        string_append_format(steps_tex,
                             "\\begin{aligned}[t]"
                             "\\text{Rule:}\\quad&\\text{%s}\\\\"
                             "\\text{Parsed equation:}\\quad&%s\\\\"
                             "\\text{Reduction:}\\quad&%s",
                             de_solver_rule_name(result->solver), problem_tex, de_solver_rule_tex(result->solver)) < 0)
        goto cleanup;

    for (size_t i = 0u; i < result->solution_count; ++i) {
        string_t *solution = equ_to_text(result->solutions[i], style_UNBOUND);
        char *solution_lhs_tex = expr_to_tex_body(equ_lhs(result->solutions[i]));
        char *solution_rhs_tex = expr_to_tex_body(equ_rhs(result->solutions[i]));
        string_t *solution_tex =
            solution_lhs_tex && solution_rhs_tex ? string_sprintf("%s = %s", solution_lhs_tex, solution_rhs_tex) : NULL;

        if (!solution || !solution_tex ||
            string_append_format(steps, "      %s%s", string_c_str(solution),
                                 i + 1u < result->solution_count ? "\n" : "") < 0 ||
            string_append_format(steps_tex, "\\\\\\text{Solution%s:}\\quad&%s",
                                 result->solution_count == 1u ? "" : " branch", string_c_str(solution_tex)) < 0) {
            free(solution_rhs_tex);
            free(solution_lhs_tex);
            string_free(solution_tex);
            string_free(solution);
            goto cleanup;
        }
        free(solution_rhs_tex);
        free(solution_lhs_tex);
        string_free(solution_tex);
        string_free(solution);
    }
    if (string_append_cstr(steps_tex, "\\end{aligned}") != 0)
        goto cleanup;

    if (!result->steps && de_solve_result_set_steps(result, string_c_str(steps)) != 0)
        goto cleanup;
    if (!result->steps_tex && de_solve_result_set_steps_tex(result, string_c_str(steps_tex)) != 0)
        goto cleanup;
    success = true;

cleanup:
    string_free(steps_tex);
    string_free(steps);
    free(problem_tex);
    free(problem);
    return success ? 0 : -1;
}

void de_solve_result_free(diffequ_solve_result_t *result)
{
    if (!result)
        return;

    for (size_t i = 0u; i < result->solution_count; ++i)
        equ_free(result->solutions[i]);
    free(result->solutions);
    free(result->symmetry);
    free(result->steps);
    free(result->steps_tex);
    free(result->diagnostic);
    free(result);
}

de_solve_status_t de_solve_result_status(const diffequ_solve_result_t *result)
{
    return result ? result->status : DE_SOLVE_STATUS_INVALID;
}

de_solver_t de_solve_result_solver(const diffequ_solve_result_t *result)
{
    return result ? result->solver : DE_SOLVER_NONE;
}

const char *de_solve_result_diagnostic(const diffequ_solve_result_t *result)
{
    return result ? result->diagnostic : NULL;
}

const char *de_solve_result_steps(const diffequ_solve_result_t *result)
{
    return result ? result->steps : NULL;
}

const char *de_solve_result_symmetry(const diffequ_solve_result_t *result)
{
    return result ? result->symmetry : NULL;
}

const char *de_solve_result_steps_tex(const diffequ_solve_result_t *result)
{
    return result ? result->steps_tex : NULL;
}

size_t de_solve_result_count(const diffequ_solve_result_t *result)
{
    return result ? result->solution_count : 0u;
}

const equation_t *de_solve_result_at(const diffequ_solve_result_t *result, size_t index)
{
    if (!result || index >= result->solution_count)
        return NULL;
    return result->solutions[index];
}
