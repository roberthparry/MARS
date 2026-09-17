#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#include "test_harness.h"
#include "test_diffequ_heat.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

static const char heat_source[] = "u_t - u_xx + au = 0; u(x,0)=0; u(0,t)=g(t)";

void test_diffequ_half_line_heat(void)
{
    static const char *const sources[] = {
        heat_source,
        "2*u_t - 2*u_xx + 2*a*u = 0; u(0,t)=g(t); u(x,0)=0",
        "a*u - u_xx + u_t = 0; u(x,0)=0; u(0,t)=g(t)",
        "w_s - 4*w_rr + 3*w = 0; w(r,0)=0; w(0,s)=h(s)",
        "u_t - u_xx = 0; u(x,0)=0; u(0,t)=t",
        "u_t - u_xx - 2*u = 0; u(x,0)=0; u(0,t)=sin(t)",
        "u_t - u_xx + s*u = 0; u(x,0)=0; u(0,t)=g(t)",
        "u_t - u_xx = 0; u(t,0)=g(t); u(0,x)=0",
        "u_t - u_xx = 0; u(x,0)=0; u(0,t)=0",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
        const equation_t *solution = de_solve_result_at(result, 0u);
        bool solved = de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                      de_solve_result_solver(result) == DE_SOLVER_HALF_LINE_HEAT;
        de_free(de);
        string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
        char *TeX = solution ? equ_to_TeX_body_wrapped(solution, SIZE_MAX) : NULL;
        bool retained = text && TeX && strstr(string_c_str(text), "half-line") &&
                        strstr(TeX, "right-hand spatial limit") && !strstr(TeX, "NAN");
        printf("  %s\n  %s\n", sources[i], text ? string_c_str(text) : "NULL");
        if (i != 8u)
            retained = retained && strstr(TeX, "\\int") && !strstr(TeX, "\\frac{\\int");
        else
            retained = retained && expr_is_exact_zero(equ_rhs(solution));
        free(TeX); string_free(text); de_solve_result_free(result);
        ASSERT_TRUE(solved);
        ASSERT_TRUE(retained);
    }
}

void test_diffequ_half_line_heat_scope(void)
{
    static const char *const sources[] = {
        "u_t + u_xx + a*u=0; u(x,0)=0; u(0,t)=g(t)",
        "u_t - b*u_xx + a*u=0; u(x,0)=0; u(0,t)=g(t)",
        "u_t - u_xx + x*u=0; u(x,0)=0; u(0,t)=g(t)",
        "u_t - u_xx + u^2=0; u(x,0)=0; u(0,t)=g(t)",
        "u_t - u_xx + a*u=1; u(x,0)=0; u(0,t)=g(t)",
        "u_t - u_xx + i*u=0; u(x,0)=0; u(0,t)=g(t)",
        "u_t - u_xx=0; u(x,0)=f(x); u(0,t)=g(t)",
        "u_t - u_xx=0; u(x,0)=0; u_x(0,t)=g(t)",
        "u_t - u_xx=0; u(x,0)=0; u(0,t)=1",
        "u_t - u_xx=0; u(x,0)=0; u(1,t)=g(t)",
        "u_t - u_xx=0; u(x,0)=0; u(0,t)=g(t); u(1,t)=0",
        "u_t - u_xx=0; u(x,0)=0; u(0,t)=x",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        bool declined = de && result && de_solve_result_solver(result) != DE_SOLVER_HALF_LINE_HEAT;
        de_solve_result_free(result); de_free(de);
        ASSERT_TRUE(declined);
    }
}

static const expr_t *heat_integral(const expr_t *expr)
{
    const expr_t *body = NULL, *left = NULL, *right = NULL;
    if (expr_match_integral_expr(expr, &body, NULL)) return expr;
    if (!expr_child_exprs(expr, &left, &right)) return NULL;
    const expr_t *found = heat_integral(left);
    return found ? found : heat_integral(right);
}

static const expr_t *heat_integrand(const expr_t *expr)
{
    const expr_t *integral = heat_integral(expr), *body = NULL;
    return integral && expr_match_integral_expr(integral, &body, NULL) ? body : NULL;
}

static double heat_sample(const expr_t *expr, double x, double t, double s)
{
    const char *names[] = {"x", "t", "s", "a"};
    double values[] = {x, t, s, 0.75};
    expr_t *at = expr_clone(expr);
    for (size_t i = 0u; at && i < 4u; ++i) {
        number_t value = num_create_from_double(values[i]);
        expr_t *variable = expr_new_named_var(NUM_NAN, names[i]), *point = expr_new_const(value);
        expr_t *next = expr_substitute(at, variable, point);
        num_destroy(&value); expr_free(point); expr_free(variable); expr_free(at); at = next;
    }
    number_t value = at ? expr_eval(at) : num_clone(NUM_NAN);
    double result = num_to_double(value);
    num_destroy(&value); expr_free(at);
    return result;
}

/* Check the actual native integrand, the PDE identity, and an independently integrated boundary ramp. */
void test_diffequ_half_line_heat_kernel(void)
{
    const char *source = "u_t - u_xx + a*u=0; u(x,0)=0; u(0,t)=t";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = de_solve_result_at(result, 0u);
    const expr_t *body = solution ? heat_integrand(equ_rhs(solution)) : NULL;
    const expr_t *coordinate = de && strcmp(expr_symbol_name(de_independent_at(de, 0u)), "x") == 0
                                   ? de_independent_at(de, 0u) : de_independent_at(de, 1u);
    expr_t *x = coordinate ? expr_clone(coordinate) : NULL;
    expr_t *kernel = body ? expr_mul(x, body) : NULL;
    expr_t *residual = de ? equ_residual(de_equation(de)) : NULL;
    expr_t *applied = kernel ? expr_substitute(residual, equ_lhs(solution), kernel) : NULL;
    bool valid = kernel && applied;
    for (size_t i = 0u; valid && i < 3u; ++i) {
        double spatial = 0.5 + i*0.25, time = 1.25, history = 0.25;
        double expected = spatial*history*exp(-spatial*spatial/4.0-0.75);
        double actual = heat_sample(kernel, spatial, time, history), error = heat_sample(applied, spatial, time, history);
        printf("  kernel: want %.12g, got %.12g, PDE residual %.12g\n", expected, actual, error);
        valid = isfinite(actual) && fabs(actual-expected) < 1e-12 && isfinite(error) && fabs(error) < 1e-12;
    }
    expr_free(applied); expr_free(residual); expr_free(kernel); expr_free(x);
    de_solve_result_free(result); de_free(de);
    ASSERT_TRUE(valid);

    /* For a=0 and g(t)=t, the convolution is known explicitly in terms of erfc. */
    de = de_from_string("u_t - u_xx=0; u(x,0)=0; u(0,t)=t");
    result = de ? de_solve(de) : NULL;
    solution = de_solve_result_at(result, 0u);
    body = solution ? heat_integrand(equ_rhs(solution)) : NULL;
    expr_t *one = expr_const_one();
    expr_t *factor = body ? expr_substitute(equ_rhs(solution), heat_integral(equ_rhs(solution)), one) : NULL;
    valid = body && factor;
    double pi = acos(-1.0), spatial = 0.8, time = 1.0, integral = 0.0;
    const size_t intervals = 400u;
    for (size_t i = 0u; valid && i < intervals; ++i) {
        double history = (i+0.5)*time/intervals;
        double value = heat_sample(body, spatial, time, history);
        valid = isfinite(value);
        integral += value*time/intervals;
    }
    double actual = factor ? heat_sample(factor, spatial, time, 0.0)*integral : NAN;
    double expected = (time+spatial*spatial/2.0)*erfc(spatial/(2.0*sqrt(time))) -
                      spatial*sqrt(time/pi)*exp(-spatial*spatial/(4.0*time));
    valid = valid && fabs(actual-expected) < 2e-6;
    /* Resolve the narrow boundary layer by tau=x^2/(4*q^2), using the native integrand and prefactor. */
    spatial = 0.02;
    double lower = spatial/(2.0*sqrt(time)), step = (10.0-lower)/800.0;
    integral = 0.0;
    for (size_t i = 0u; valid && i < 800u; ++i) {
        double q = lower+(i+0.5)*step, history = time-spatial*spatial/(4.0*q*q);
        double value = heat_sample(body, spatial, time, history)*spatial*spatial/(2.0*q*q*q);
        valid = isfinite(value);
        integral += step*value;
    }
    actual = factor ? heat_sample(factor, spatial, time, 0.0)*integral : NAN;
    expected = (time+spatial*spatial/2.0)*erfc(spatial/(2.0*sqrt(time))) -
               spatial*sqrt(time/pi)*exp(-spatial*spatial/(4.0*time));
    valid = valid && fabs(actual-expected) < 0.002 && fabs(actual-time) < 0.03;
    expr_free(factor); expr_free(one);
    de_solve_result_free(result); de_free(de);
    ASSERT_TRUE(valid);
}

/* README example from docs/diffequation.md: zero initial data and a symbolic boundary history. */
void example_diffequation_half_line_heat(void)
{
    diffequ_t *de = de_from_string(heat_source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = de_solve_result_at(result, 0u);
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    const char *expected = "u = x/(2·√(π))·∫^t_0 g(s)·(t - s)^-³⁄₂·exp(-¼·(4a·(t - s) + x²/(t - s)))·ds; "
        "x > 0, t > 0; right half-line, decay at infinity; continuous locally bounded boundary data, "
        "zero at the corner; boundary value by the right-hand spatial limit.";
    bool valid = text && de_solve_result_solver(result) == DE_SOLVER_HALF_LINE_HEAT &&
                 strcmp(string_c_str(text), expected) == 0;
    printf("  %s\n  %s\n", heat_source, text ? string_c_str(text) : "NULL");
    string_free(text); de_solve_result_free(result); de_free(de);
    ASSERT_TRUE(valid);
}
