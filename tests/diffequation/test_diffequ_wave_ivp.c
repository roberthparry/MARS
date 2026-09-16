#include <stdio.h>
#include <string.h>

#include "test_diffequ_wave_ivp.h"
#include "test_harness.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

/* Check a residual at the same exact sample coordinates for every wave regression. */
bool test_diffequ_wave_zero_at_samples(const expr_t *expr, const diffequ_t *de)
{
    for (long sample = 0L; sample < 3L; ++sample) {
        expr_t *at = expr_clone(expr);
        for (size_t i = 0u; at && i < de_independent_count(de); ++i) {
            number_t coordinate = num_create_from_frac(sample + (long)i - 1L, 4L);
            expr_t *point = expr_new_const(coordinate);
            num_destroy(&coordinate);
            expr_t *next = expr_substitute(at, de_independent_at(de, i), point);
            expr_free(point);
            expr_free(at);
            at = next;
        }
        number_t value = at ? expr_eval(at) : num_clone(NUM_NAN);
        number_t magnitude = num_abs(value), tolerance = num_create_from_string("1e-24");
        bool zero = num_is_finite(value) && num_lt(magnitude, tolerance);
        num_destroy(&tolerance);
        num_destroy(&magnitude);
        num_destroy(&value);
        expr_free(at);
        if (!zero)
            return false;
    }
    return true;
}

static void test_diffequ_wave_ivp_case(const char *source, const char *expected_text,
                                     const char *time_name, long initial_time)
{
    printf("  %s\n", source);
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
    ASSERT_EQ_LONG(de_solve_result_status(result), DE_SOLVE_STATUS_SOLVED);
    ASSERT_EQ_LONG(de_solve_result_solver(result), DE_SOLVER_DALEMBERT_DUHAMEL);
    const equation_t *solution = de_solve_result_at(result, 0u);
    expr_t *expected = expr_from_string(expected_text, NULL);
    expr_t *difference = solution ? expr_sub(equ_rhs(solution), expected) : NULL;
    ASSERT_TRUE(difference && test_diffequ_wave_zero_at_samples(difference, de));
    string_t *text = equ_to_text(solution, style_UNBOUND);
    ASSERT_TRUE(text && !strstr(string_c_str(text), "∫"));
    printf("  %s\n", string_c_str(text));
    expr_t *residual = equ_residual(de_equation(de));
    expr_t *applied = expr_substitute(residual, equ_lhs(solution), equ_rhs(solution));
    ASSERT_TRUE(applied && test_diffequ_wave_zero_at_samples(applied, de));
    expr_t *time = expr_new_named_var(NUM_NAN, time_name);
    expr_t *point = expr_const_long(initial_time);
    for (size_t j = 0u; j < 2u; ++j) {
        const equation_t *condition = de_condition_at(de, j);
        expr_t *lhs = expr_substitute(equ_lhs(condition), equ_lhs(solution), equ_rhs(solution));
        expr_t *at = expr_substitute(lhs, time, point);
        expr_t *error = expr_sub(at, equ_rhs(condition));
        ASSERT_TRUE(error && test_diffequ_wave_zero_at_samples(error, de));
        expr_free(error);
        expr_free(at);
        expr_free(lhs);
    }
    ASSERT_TRUE(strstr(de_solve_result_steps(result), "Duhamel"));
    ASSERT_TRUE(!strstr(de_solve_result_steps_TeX(result), "NAN"));
    expr_free(point);
    expr_free(time);
    expr_free(applied);
    expr_free(residual);
    string_free(text);
    expr_free(difference);
    expr_free(expected);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_wave_ivp_polynomial_forcing(void)
{
    test_diffequ_wave_ivp_case("u_tt - 4u_xx = x*t; u(x,0) = x^2; u_t(x,0) = 1",
                              "x^2+4t^2+t+x*t^3/6", "t", 0L);
}

static void test_diffequ_wave_ivp_constant_forcing(void)
{
    test_diffequ_wave_ivp_case("u_tt - 9u_xx = 1; u(x,0) = 0; u_t(x,0) = 0",
                              "t^2/2", "t", 0L);
}

static void test_diffequ_wave_ivp_scaled_operator(void)
{
    test_diffequ_wave_ivp_case("8u_xx - 2u_tt = -2x*t; u_t(x,0) = 1; u(x,0) = x^2",
                              "x^2+4t^2+t+x*t^3/6", "t", 0L);
}

static void test_diffequ_wave_ivp_shifted_time(void)
{
    test_diffequ_wave_ivp_case("u_tt - 4u_xx = 0; u(x,2) = x^2; u_t(x,2) = x",
                              "x^2+x*(t-2)+4*(t-2)^2", "t", 2L);
}

static void test_diffequ_wave_ivp_renamed_coordinates(void)
{
    test_diffequ_wave_ivp_case("w_ss - 9w_rr = r*s; w(r,0) = r^2; w_s(r,0) = 1",
                              "r^2+9s^2+s+r*s^3/6", "s", 0L);
}

static void test_diffequ_wave_ivp_quadratic_forcing(void)
{
    test_diffequ_wave_ivp_case("u_tt - 4u_xx = x^2+t^2; u(x,0) = 0; u_t(x,0) = 0",
                              "t^2*x^2/2+5t^4/12", "t", 0L);
}

static void test_diffequ_wave_ivp_trigonometric_data(void)
{
    test_diffequ_wave_ivp_case("u_xx - u_tt = 0; u(x,0) = sin(x); u_t(x,0) = cos(x)",
                              "sin(x)*cos(t)+cos(x)*sin(t)", "t", 0L);
}

/* Keep each complete IVP independently selectable for resource-bounded memory checks. */
void test_diffequ_wave_ivp_polynomial_data(void)
{
    TEST_RUN_SUBTEST(test_diffequ_wave_ivp_polynomial_forcing, "diffequation,wave,ivp");
    TEST_RUN_SUBTEST(test_diffequ_wave_ivp_constant_forcing, "diffequation,wave,ivp");
    TEST_RUN_SUBTEST(test_diffequ_wave_ivp_scaled_operator, "diffequation,wave,ivp");
    TEST_RUN_SUBTEST(test_diffequ_wave_ivp_shifted_time, "diffequation,wave,ivp");
    TEST_RUN_SUBTEST(test_diffequ_wave_ivp_renamed_coordinates, "diffequation,wave,ivp");
    TEST_RUN_SUBTEST(test_diffequ_wave_ivp_quadratic_forcing, "diffequation,wave,ivp");
    TEST_RUN_SUBTEST(test_diffequ_wave_ivp_trigonometric_data, "diffequation,wave,ivp");
}
