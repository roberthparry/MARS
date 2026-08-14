#include <stdio.h>

#include "integrator.h"
#include "test_expr.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

static void print_antiderivative_text(const char *label, const char *text)
{
    printf("ANTIDERIVATIVE: %s -> %s\n", label, text ? text : "(null)");
}

static void print_antiderivative_expr(const char *label, const expr_t *anti)
{
    char *text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;

    print_antiderivative_text(label, text);
    free(text);
}

static void assert_antiderivative_matches(const char *label, const expr_t *expr, expr_t *x, const double *points,
                                          size_t npoints)
{
    expr_t *anti = expr_integrate(expr, x);
    expr_t *deriv = anti ? expr_create_deriv(anti, x) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(deriv);
    print_antiderivative_expr(label, anti);

    for (size_t i = 0; i < npoints; ++i) {
        char point_label[160];

        test_expr_set_val_d(x, points[i]);
        snprintf(point_label, sizeof(point_label), "%s at x=%g", label, points[i]);
        check_q_at(__FILE__, __LINE__, 1, point_label, expr_eval_qf(deriv), expr_eval_qf(expr));
    }

    expr_free(deriv);
    expr_free(anti);
}

static void assert_string_antiderivative_matches(const char *input, const double *points, size_t npoints)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string(input, &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *simplified = expr ? expr_simplify(expr) : NULL;

    ASSERT_NOT_NULL(simplified);
    ASSERT_NOT_NULL(x);
    assert_antiderivative_matches(input, simplified, x, points, npoints);

    expr_free(simplified);
    expr_free(expr);
    expr_bindings_free(bindings);
}

static void assert_string_antiderivative_round_trips(const char *input)
{
    expr_bindings_t *bindings = NULL;
    expr_bindings_t *round_trip_bindings = NULL;
    expr_t *expr = expr_from_string(input, &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *anti = expr && x ? expr_integrate(expr, x) : NULL;
    char *text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;
    expr_t *round_trip = text ? expr_from_string(text, &round_trip_bindings) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(x);
    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(text);
    ASSERT_NOT_NULL(round_trip);
    ASSERT_TRUE(strstr(text, "LommelSPrime") == NULL);
    ASSERT_TRUE(strstr(text, "lommel_s_derivative") == NULL);

    expr_free(round_trip);
    expr_bindings_free(round_trip_bindings);
    free(text);
    expr_free(anti);
    expr_free(expr);
    expr_bindings_free(bindings);
}

static void assert_iterated_derivatives_integrate_back(const char *input, const char *const expected_derivatives[4],
                                                       const double *points, size_t npoints)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string(input, &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *previous = expr ? expr_simplify(expr) : NULL;

    ASSERT_NOT_NULL(previous);
    ASSERT_NOT_NULL(x);

    for (size_t order = 1u; order <= 4u; ++order) {
        expr_t *raw_deriv = expr_create_deriv(previous, x);
        expr_t *deriv = raw_deriv ? expr_simplify(raw_deriv) : NULL;
        expr_t *anti = deriv ? expr_integrate(deriv, x) : NULL;
        expr_t *anti_deriv = anti ? expr_create_deriv(anti, x) : NULL;
        char label[160];
        char *deriv_text = deriv ? expr_to_string(deriv, style_UNBOUND) : NULL;

        snprintf(label, sizeof(label), "%s derivative %zu", input, order);
        ASSERT_NOT_NULL(deriv);
        ASSERT_NOT_NULL(deriv_text);
        if (expected_derivatives && expected_derivatives[order - 1u]) {
            if (str_eq(deriv_text, expected_derivatives[order - 1u]))
                to_string_pass(label, deriv_text, expected_derivatives[order - 1u]);
            else
                to_string_fail(__FILE__, __LINE__, 1, label, deriv_text, expected_derivatives[order - 1u]);
        }
        ASSERT_NOT_NULL(anti);
        ASSERT_NOT_NULL(anti_deriv);

        for (size_t i = 0; i < npoints; ++i) {
            char point_label[320];

            test_expr_set_val_d(x, points[i]);
            snprintf(point_label, sizeof(point_label), "%s differentiates back at x=%g", label, points[i]);
            check_q_at(__FILE__, __LINE__, 1, point_label, expr_eval_qf(anti_deriv), expr_eval_qf(deriv));

            snprintf(point_label, sizeof(point_label), "%s integrates to previous derivative at x=%g", label,
                     points[i]);
            check_q_at(__FILE__, __LINE__, 1, point_label, expr_eval_qf(anti), expr_eval_qf(previous));
        }

        expr_free(anti_deriv);
        expr_free(anti);
        expr_free(raw_deriv);
        free(deriv_text);
        expr_free(previous);
        previous = deriv;
    }

    expr_free(previous);
    expr_free(expr);
    expr_bindings_free(bindings);
}

static void assert_nth_derivative_integrates_back(const char *input, size_t order, const double *points, size_t npoints)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string(input, &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *previous = expr ? expr_simplify(expr) : NULL;
    expr_t *deriv = NULL;
    expr_t *anti = NULL;
    expr_t *anti_deriv = NULL;

    ASSERT_NOT_NULL(previous);
    ASSERT_NOT_NULL(x);

    for (size_t current = 1u; current <= order; ++current) {
        expr_t *raw_deriv = expr_create_deriv(previous, x);

        deriv = raw_deriv ? expr_simplify(raw_deriv) : NULL;
        expr_free(raw_deriv);
        ASSERT_NOT_NULL(deriv);

        if (current < order) {
            expr_free(previous);
            previous = deriv;
            deriv = NULL;
        }
    }

    anti = deriv ? expr_integrate(deriv, x) : NULL;
    anti_deriv = anti ? expr_create_deriv(anti, x) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(anti_deriv);

    for (size_t i = 0; i < npoints; ++i) {
        char point_label[320];

        test_expr_set_val_d(x, points[i]);
        snprintf(point_label, sizeof(point_label), "%s derivative %zu differentiates back at x=%g", input, order,
                 points[i]);
        check_q_at(__FILE__, __LINE__, 1, point_label, expr_eval_qf(anti_deriv), expr_eval_qf(deriv));

        snprintf(point_label, sizeof(point_label), "%s derivative %zu integrates to previous derivative at x=%g", input,
                 order, points[i]);
        check_q_at(__FILE__, __LINE__, 1, point_label, expr_eval_qf(anti), expr_eval_qf(previous));
    }

    expr_free(anti_deriv);
    expr_free(anti);
    expr_free(deriv);
    expr_free(previous);
    expr_free(expr);
    expr_bindings_free(bindings);
}

static void assert_string_antiderivative_contains(const char *input, const char *expected)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string(input, &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *simplified = expr ? expr_simplify(expr) : NULL;
    expr_t *anti = simplified ? expr_integrate(simplified, x) : NULL;
    char *text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(simplified);
    ASSERT_NOT_NULL(x);
    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(text);
    print_antiderivative_text(input, text);
    ASSERT_TRUE(strstr(text, expected) != NULL);

    free(text);
    expr_free(anti);
    expr_free(simplified);
    expr_free(expr);
    expr_bindings_free(bindings);
}

static void assert_string_antiderivative_not_contains(const char *input, const char *forbidden)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string(input, &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *simplified = expr ? expr_simplify(expr) : NULL;
    expr_t *anti = simplified ? expr_integrate(simplified, x) : NULL;
    char *text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(simplified);
    ASSERT_NOT_NULL(x);
    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(text);
    print_antiderivative_text(input, text);
    ASSERT_TRUE(strstr(text, forbidden) == NULL);

    free(text);
    expr_free(anti);
    expr_free(simplified);
    expr_free(expr);
    expr_bindings_free(bindings);
}

static void assert_string_antiderivative_matches_with_a(const char *input, double a_value, const double *points,
                                                        size_t npoints)
{
    expr_bindings_t *bindings = NULL;
    expr_bindings_t *anti_bindings = NULL;
    expr_bindings_t *expected_bindings = NULL;
    expr_t *expr = expr_from_string(input, &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *a = bindings ? expr_bindings_get(bindings, "a") : NULL;
    expr_t *simplified = expr ? expr_simplify(expr) : NULL;
    expr_t *anti = simplified ? expr_integrate(simplified, x) : NULL;
    char *anti_text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;
    char *expected_text = simplified ? expr_to_string(simplified, style_UNBOUND) : NULL;
    size_t anti_input_len = anti_text ? strlen(anti_text) + 5u : 0u;
    size_t expected_input_len = expected_text ? strlen(expected_text) + 5u : 0u;
    char *anti_input = anti_text ? malloc(anti_input_len) : NULL;
    char *expected_input = expected_text ? malloc(expected_input_len) : NULL;
    expr_t *anti_eval = NULL;
    expr_t *deriv_eval = NULL;
    expr_t *expected_eval = NULL;
    expr_t *anti_x = NULL;
    expr_t *deriv_x = NULL;
    expr_t *expected_x = NULL;
    expr_t *anti_a = NULL;
    expr_t *deriv_a = NULL;
    expr_t *expected_a = NULL;

    ASSERT_NOT_NULL(simplified);
    ASSERT_NOT_NULL(x);
    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(anti_text);
    ASSERT_NOT_NULL(expected_text);
    ASSERT_NOT_NULL(anti_input);
    ASSERT_NOT_NULL(expected_input);
    print_antiderivative_text(input, anti_text);

    snprintf(anti_input, anti_input_len, "{ %s }", anti_text);
    snprintf(expected_input, expected_input_len, "{ %s }", expected_text);

    anti_eval = expr_from_string(anti_input, &anti_bindings);
    expected_eval = expr_from_string(expected_input, &expected_bindings);
    anti_x = anti_bindings ? expr_bindings_get(anti_bindings, "x") : NULL;
    anti_a = anti_bindings ? expr_bindings_get(anti_bindings, "a") : NULL;
    deriv_eval = anti_eval && anti_x ? expr_create_deriv(anti_eval, anti_x) : NULL;
    deriv_x = anti_x;
    deriv_a = anti_a;
    expected_x = expected_bindings ? expr_bindings_get(expected_bindings, "x") : NULL;
    expected_a = expected_bindings ? expr_bindings_get(expected_bindings, "a") : NULL;

    ASSERT_NOT_NULL(anti_eval);
    ASSERT_NOT_NULL(deriv_eval);
    ASSERT_NOT_NULL(expected_eval);
    ASSERT_NOT_NULL(anti_x);
    ASSERT_NOT_NULL(anti_a);
    ASSERT_NOT_NULL(deriv_x);
    ASSERT_NOT_NULL(expected_x);
    ASSERT_NOT_NULL(deriv_a);
    ASSERT_NOT_NULL(expected_a);

    test_expr_set_val_d(a, a_value);
    test_expr_set_val_d(deriv_a, a_value);
    test_expr_set_val_d(expected_a, a_value);
    for (size_t i = 0; i < npoints; ++i) {
        char point_label[160];

        test_expr_set_val_d(x, points[i]);
        test_expr_set_val_d(deriv_x, points[i]);
        test_expr_set_val_d(expected_x, points[i]);
        snprintf(point_label, sizeof(point_label), "%s at x=%g, a=%g", input, points[i], a_value);
        check_q_at(__FILE__, __LINE__, 1, point_label, expr_eval_qf(deriv_eval), expr_eval_qf(expected_eval));
    }

    free(expected_input);
    free(anti_input);
    free(expected_text);
    free(anti_text);
    expr_free(expected_eval);
    expr_bindings_free(expected_bindings);
    expr_free(deriv_eval);
    expr_free(anti_eval);
    expr_bindings_free(anti_bindings);
    expr_free(anti);
    expr_free(simplified);
    expr_free(expr);
    expr_bindings_free(bindings);
}

static void assert_string_antiderivative_matches_with_ab(const char *input, double a_value, double b_value,
                                                         const double *points, size_t npoints)
{
    expr_bindings_t *bindings = NULL;
    expr_bindings_t *anti_bindings = NULL;
    expr_bindings_t *expected_bindings = NULL;
    expr_t *expr = expr_from_string(input, &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *a = bindings ? expr_bindings_get(bindings, "a") : NULL;
    expr_t *b = bindings ? expr_bindings_get(bindings, "b") : NULL;
    expr_t *simplified = expr ? expr_simplify(expr) : NULL;
    expr_t *anti = simplified ? expr_integrate(simplified, x) : NULL;
    char *anti_text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;
    char *expected_text = simplified ? expr_to_string(simplified, style_UNBOUND) : NULL;
    size_t anti_input_len = anti_text ? strlen(anti_text) + 5u : 0u;
    size_t expected_input_len = expected_text ? strlen(expected_text) + 5u : 0u;
    char *anti_input = anti_text ? malloc(anti_input_len) : NULL;
    char *expected_input = expected_text ? malloc(expected_input_len) : NULL;
    expr_t *anti_eval = NULL;
    expr_t *deriv_eval = NULL;
    expr_t *expected_eval = NULL;
    expr_t *anti_x = NULL;
    expr_t *expected_x = NULL;
    expr_t *anti_a = NULL;
    expr_t *expected_a = NULL;
    expr_t *anti_b = NULL;
    expr_t *expected_b = NULL;

    ASSERT_NOT_NULL(simplified);
    ASSERT_NOT_NULL(x);
    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(anti_text);
    ASSERT_NOT_NULL(expected_text);
    ASSERT_NOT_NULL(anti_input);
    ASSERT_NOT_NULL(expected_input);
    print_antiderivative_text(input, anti_text);

    snprintf(anti_input, anti_input_len, "{ %s }", anti_text);
    snprintf(expected_input, expected_input_len, "{ %s }", expected_text);

    anti_eval = expr_from_string(anti_input, &anti_bindings);
    expected_eval = expr_from_string(expected_input, &expected_bindings);
    anti_x = anti_bindings ? expr_bindings_get(anti_bindings, "x") : NULL;
    anti_a = anti_bindings ? expr_bindings_get(anti_bindings, "a") : NULL;
    anti_b = anti_bindings ? expr_bindings_get(anti_bindings, "b") : NULL;
    deriv_eval = anti_eval && anti_x ? expr_create_deriv(anti_eval, anti_x) : NULL;
    expected_x = expected_bindings ? expr_bindings_get(expected_bindings, "x") : NULL;
    expected_a = expected_bindings ? expr_bindings_get(expected_bindings, "a") : NULL;
    expected_b = expected_bindings ? expr_bindings_get(expected_bindings, "b") : NULL;

    ASSERT_NOT_NULL(anti_eval);
    ASSERT_NOT_NULL(deriv_eval);
    ASSERT_NOT_NULL(expected_eval);
    ASSERT_NOT_NULL(anti_x);
    ASSERT_NOT_NULL(expected_x);
    ASSERT_NOT_NULL(anti_a);
    ASSERT_NOT_NULL(expected_a);
    ASSERT_NOT_NULL(anti_b);
    ASSERT_NOT_NULL(expected_b);

    test_expr_set_val_d(a, a_value);
    test_expr_set_val_d(b, b_value);
    test_expr_set_val_d(anti_a, a_value);
    test_expr_set_val_d(expected_a, a_value);
    test_expr_set_val_d(anti_b, b_value);
    test_expr_set_val_d(expected_b, b_value);
    for (size_t i = 0; i < npoints; ++i) {
        char point_label[160];

        test_expr_set_val_d(x, points[i]);
        test_expr_set_val_d(anti_x, points[i]);
        test_expr_set_val_d(expected_x, points[i]);
        snprintf(point_label, sizeof(point_label), "%s at x=%g, a=%g, b=%g", input, points[i], a_value, b_value);
        check_q_at(__FILE__, __LINE__, 1, point_label, expr_eval_qf(deriv_eval), expr_eval_qf(expected_eval));
    }

    free(expected_input);
    free(anti_input);
    free(expected_text);
    free(anti_text);
    expr_free(expected_eval);
    expr_bindings_free(expected_bindings);
    expr_free(deriv_eval);
    expr_free(anti_eval);
    expr_bindings_free(anti_bindings);
    expr_free(anti);
    expr_free(simplified);
    expr_free(expr);
    expr_bindings_free(bindings);
}

static void assert_string_antiderivative_matches_with_abc(const char *input, double a_value, double b_value,
                                                          double c_value, const double *points, size_t npoints)
{
    expr_bindings_t *bindings = NULL;
    expr_bindings_t *anti_bindings = NULL;
    expr_bindings_t *expected_bindings = NULL;
    expr_t *expr = expr_from_string(input, &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *a = bindings ? expr_bindings_get(bindings, "a") : NULL;
    expr_t *b = bindings ? expr_bindings_get(bindings, "b") : NULL;
    expr_t *c = bindings ? expr_bindings_get(bindings, "c") : NULL;
    expr_t *simplified = expr ? expr_simplify(expr) : NULL;
    expr_t *anti = simplified ? expr_integrate(simplified, x) : NULL;
    char *anti_text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;
    char *expected_text = simplified ? expr_to_string(simplified, style_UNBOUND) : NULL;
    size_t anti_input_len = anti_text ? strlen(anti_text) + 5u : 0u;
    size_t expected_input_len = expected_text ? strlen(expected_text) + 5u : 0u;
    char *anti_input = anti_text ? malloc(anti_input_len) : NULL;
    char *expected_input = expected_text ? malloc(expected_input_len) : NULL;
    expr_t *anti_eval = NULL;
    expr_t *deriv_eval = NULL;
    expr_t *expected_eval = NULL;
    expr_t *anti_x = NULL;
    expr_t *expected_x = NULL;
    expr_t *anti_a = NULL;
    expr_t *expected_a = NULL;
    expr_t *anti_b = NULL;
    expr_t *expected_b = NULL;
    expr_t *anti_c = NULL;
    expr_t *expected_c = NULL;

    ASSERT_NOT_NULL(simplified);
    ASSERT_NOT_NULL(x);
    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);
    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(anti_text);
    ASSERT_NOT_NULL(expected_text);
    ASSERT_NOT_NULL(anti_input);
    ASSERT_NOT_NULL(expected_input);
    print_antiderivative_text(input, anti_text);

    snprintf(anti_input, anti_input_len, "{ %s }", anti_text);
    snprintf(expected_input, expected_input_len, "{ %s }", expected_text);

    anti_eval = expr_from_string(anti_input, &anti_bindings);
    expected_eval = expr_from_string(expected_input, &expected_bindings);
    anti_x = anti_bindings ? expr_bindings_get(anti_bindings, "x") : NULL;
    anti_a = anti_bindings ? expr_bindings_get(anti_bindings, "a") : NULL;
    anti_b = anti_bindings ? expr_bindings_get(anti_bindings, "b") : NULL;
    anti_c = anti_bindings ? expr_bindings_get(anti_bindings, "c") : NULL;
    deriv_eval = anti_eval && anti_x ? expr_create_deriv(anti_eval, anti_x) : NULL;
    expected_x = expected_bindings ? expr_bindings_get(expected_bindings, "x") : NULL;
    expected_a = expected_bindings ? expr_bindings_get(expected_bindings, "a") : NULL;
    expected_b = expected_bindings ? expr_bindings_get(expected_bindings, "b") : NULL;
    expected_c = expected_bindings ? expr_bindings_get(expected_bindings, "c") : NULL;

    ASSERT_NOT_NULL(anti_eval);
    ASSERT_NOT_NULL(deriv_eval);
    ASSERT_NOT_NULL(expected_eval);
    ASSERT_NOT_NULL(anti_x);
    ASSERT_NOT_NULL(expected_x);
    ASSERT_NOT_NULL(anti_a);
    ASSERT_NOT_NULL(expected_a);
    ASSERT_NOT_NULL(anti_b);
    ASSERT_NOT_NULL(expected_b);
    ASSERT_NOT_NULL(anti_c);
    ASSERT_NOT_NULL(expected_c);

    test_expr_set_val_d(a, a_value);
    test_expr_set_val_d(b, b_value);
    test_expr_set_val_d(c, c_value);
    test_expr_set_val_d(anti_a, a_value);
    test_expr_set_val_d(expected_a, a_value);
    test_expr_set_val_d(anti_b, b_value);
    test_expr_set_val_d(expected_b, b_value);
    test_expr_set_val_d(anti_c, c_value);
    test_expr_set_val_d(expected_c, c_value);
    for (size_t i = 0; i < npoints; ++i) {
        char point_label[192];

        test_expr_set_val_d(x, points[i]);
        test_expr_set_val_d(anti_x, points[i]);
        test_expr_set_val_d(expected_x, points[i]);
        snprintf(point_label, sizeof(point_label), "%s at x=%g, a=%g, b=%g, c=%g", input, points[i], a_value, b_value,
                 c_value);
        check_q_at(__FILE__, __LINE__, 1, point_label, expr_eval_qf(deriv_eval), expr_eval_qf(expected_eval));
    }

    free(expected_input);
    free(anti_input);
    free(expected_text);
    free(anti_text);
    expr_free(expected_eval);
    expr_bindings_free(expected_bindings);
    expr_free(deriv_eval);
    expr_free(anti_eval);
    expr_bindings_free(anti_bindings);
    expr_free(anti);
    expr_free(simplified);
    expr_free(expr);
    expr_bindings_free(bindings);
}

static void test_integrate_polynomial_sum(void)
{
    static const double points[] = {-2.0, -0.5, 0.0, 1.25, 3.0};
    expr_t *x = test_expr_new_named_var_d(0.0, "x");
    expr_t *x2 = test_expr_pow_d(x, 2.0);
    expr_t *three_x2 = test_expr_mul_d(x2, 3.0);
    expr_t *two_x = test_expr_mul_d(x, 2.0);
    expr_t *partial = expr_add(three_x2, two_x);
    expr_t *f = test_expr_add_d(partial, 5.0);

    assert_antiderivative_matches("integral derivative of 3*x^2 + 2*x + 5", f, x, points,
                                  sizeof(points) / sizeof(points[0]));

    expr_free(f);
    expr_free(partial);
    expr_free(two_x);
    expr_free(three_x2);
    expr_free(x2);
    expr_free(x);
}

static void test_integrate_reciprocal_and_log(void)
{
    static const double points[] = {0.25, 0.75, 1.5, 4.0};
    static const double bounded_points[] = {-0.6, -0.2, 0.1, 0.5};
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *one_over_x = test_expr_d_div(1.0, x);
    expr_t *log_x = expr_log(x);
    expr_t *x2_recip = test_expr_pow_d(x, 2.0);
    expr_t *one_over_x2 = test_expr_d_div(1.0, x2_recip);
    expr_t *x2 = test_expr_pow_d(x, 2.0);
    expr_t *one_plus_x2 = test_expr_add_d(x2, 1.0);
    expr_t *one_minus_x2 = test_expr_d_sub(1.0, x2);
    expr_t *x2_for_plus = test_expr_pow_d(x, 2.0);
    expr_t *x2_plus_one = test_expr_add_d(x2_for_plus, 1.0);
    expr_t *one_a = test_expr_new_const_d(1.0);
    expr_t *one_b = test_expr_new_const_d(1.0);
    expr_t *one_c = test_expr_new_const_d(1.0);
    expr_t *one_d = test_expr_new_const_d(1.0);
    expr_t *one_e = test_expr_new_const_d(1.0);
    expr_t *sqrt_one_plus_x2 = one_plus_x2 ? expr_sqrt(one_plus_x2) : NULL;
    expr_t *sqrt_one_minus_x2 = one_minus_x2 ? expr_sqrt(one_minus_x2) : NULL;
    expr_t *sqrt_x2_plus_one = x2_plus_one ? expr_sqrt(x2_plus_one) : NULL;
    expr_t *inv_one_plus_x2 = (one_a && one_plus_x2) ? expr_div(one_a, one_plus_x2) : NULL;
    expr_t *inv_sqrt_one_plus_x2 = (one_b && sqrt_one_plus_x2) ? expr_div(one_b, sqrt_one_plus_x2) : NULL;
    expr_t *inv_one_minus_x2 = (one_c && one_minus_x2) ? expr_div(one_c, one_minus_x2) : NULL;
    expr_t *inv_sqrt_one_minus_x2 = (one_d && sqrt_one_minus_x2) ? expr_div(one_d, sqrt_one_minus_x2) : NULL;
    expr_t *inv_sqrt_x2_plus_one = (one_e && sqrt_x2_plus_one) ? expr_div(one_e, sqrt_x2_plus_one) : NULL;

    assert_antiderivative_matches("integral derivative of 1/x", one_over_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of 1/x^2", one_over_x2, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of log(x)", log_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of 1/(1 + x^2)", inv_one_plus_x2, x, bounded_points,
                                  sizeof(bounded_points) / sizeof(bounded_points[0]));
    assert_antiderivative_matches("integral derivative of 1/sqrt(1 + x^2)", inv_sqrt_one_plus_x2, x, bounded_points,
                                  sizeof(bounded_points) / sizeof(bounded_points[0]));
    assert_antiderivative_matches("integral derivative of 1/(1 - x^2)", inv_one_minus_x2, x, bounded_points,
                                  sizeof(bounded_points) / sizeof(bounded_points[0]));
    assert_antiderivative_matches("integral derivative of 1/sqrt(1 - x^2)", inv_sqrt_one_minus_x2, x, bounded_points,
                                  sizeof(bounded_points) / sizeof(bounded_points[0]));
    assert_antiderivative_matches("integral derivative of 1/sqrt(x^2 + 1)", inv_sqrt_x2_plus_one, x, bounded_points,
                                  sizeof(bounded_points) / sizeof(bounded_points[0]));

    expr_free(inv_sqrt_x2_plus_one);
    expr_free(inv_sqrt_one_minus_x2);
    expr_free(inv_one_minus_x2);
    expr_free(inv_sqrt_one_plus_x2);
    expr_free(inv_one_plus_x2);
    expr_free(sqrt_x2_plus_one);
    expr_free(sqrt_one_minus_x2);
    expr_free(sqrt_one_plus_x2);
    expr_free(one_e);
    expr_free(one_d);
    expr_free(one_c);
    expr_free(one_b);
    expr_free(one_a);
    expr_free(x2_plus_one);
    expr_free(x2_for_plus);
    expr_free(one_minus_x2);
    expr_free(one_plus_x2);
    expr_free(x2);
    expr_free(one_over_x2);
    expr_free(x2_recip);
    expr_free(log_x);
    expr_free(one_over_x);
    expr_free(x);
}

static void test_integrate_affine_elementary(void)
{
    static const double points[] = {-0.75, 0.0, 0.5, 1.25};
    expr_t *x = test_expr_new_named_var_d(0.0, "x");
    expr_t *two_x = test_expr_mul_d(x, 2.0);
    expr_t *exp_arg = test_expr_add_d(two_x, 3.0);
    expr_t *exp_term = expr_exp(exp_arg);
    expr_t *three_x = test_expr_mul_d(x, 3.0);
    expr_t *sin_arg = test_expr_sub_d(three_x, 1.0);
    expr_t *sin_term = expr_sin(sin_arg);
    expr_t *neg_two_x = test_expr_mul_d(x, -2.0);
    expr_t *cos_arg = test_expr_add_d(neg_two_x, 0.5);
    expr_t *cos_term = expr_cos(cos_arg);
    expr_t *first_sum = expr_add(exp_term, sin_term);
    expr_t *f = expr_sub(first_sum, cos_term);

    assert_antiderivative_matches("integral derivative of affine exp/sin/cos", f, x, points,
                                  sizeof(points) / sizeof(points[0]));

    expr_free(f);
    expr_free(first_sum);
    expr_free(cos_term);
    expr_free(cos_arg);
    expr_free(neg_two_x);
    expr_free(sin_term);
    expr_free(sin_arg);
    expr_free(three_x);
    expr_free(exp_term);
    expr_free(exp_arg);
    expr_free(two_x);
    expr_free(x);
}

static void test_integrate_affine_tangent(void)
{
    static const double points[] = {-0.5, -0.1, 0.0, 0.35};
    expr_t *x = test_expr_new_named_var_d(0.0, "x");
    expr_t *tan_x = expr_tan(x);
    expr_t *tanh_x = expr_tanh(x);
    expr_t *two_x = test_expr_mul_d(x, 2.0);
    expr_t *affine_arg = test_expr_sub_d(two_x, 1.0);
    expr_t *tan_affine = expr_tan(affine_arg);
    expr_t *tanh_affine = expr_tanh(affine_arg);

    assert_antiderivative_matches("integral derivative of tan(x)", tan_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of tan(2*x - 1)", tan_affine, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of tanh(x)", tanh_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of tanh(2*x - 1)", tanh_affine, x, points,
                                  sizeof(points) / sizeof(points[0]));

    expr_free(tanh_affine);
    expr_free(tan_affine);
    expr_free(affine_arg);
    expr_free(two_x);
    expr_free(tanh_x);
    expr_free(tan_x);
    expr_free(x);
}

static void test_integrate_affine_more_trig_and_stats(void)
{
    static const double trig_points[] = {0.3, 0.8, 1.1, 1.7};
    static const double hyper_points[] = {-1.25, -0.4, 0.2, 1.0};
    static const double stat_points[] = {-1.5, -0.25, 0.5, 1.75};
    expr_t *x = test_expr_new_named_var_d(0.0, "x");
    expr_t *two_x = test_expr_mul_d(x, 2.0);
    expr_t *sec_arg = test_expr_sub_d(two_x, 0.5);
    expr_t *sec_term = expr_sec(sec_arg);
    expr_t *three_x = test_expr_mul_d(x, 3.0);
    expr_t *cosec_arg = test_expr_add_d(three_x, 0.2);
    expr_t *cosec_term = expr_cosec(cosec_arg);
    expr_t *cot_arg = test_expr_sub_d(x, 0.4);
    expr_t *cot_term = expr_cot(cot_arg);
    expr_t *sech_arg = test_expr_add_d(two_x, 0.25);
    expr_t *sech_term = expr_sech(sech_arg);
    expr_t *coth_arg = test_expr_sub_d(x, 1.5);
    expr_t *coth_term = expr_coth(coth_arg);
    expr_t *pdf_arg = test_expr_sub_d(two_x, 1.0);
    expr_t *pdf_term = expr_normal_pdf(pdf_arg);
    expr_t *cdf_arg = test_expr_add_d(x, 0.75);
    expr_t *cdf_term = expr_normal_cdf(cdf_arg);
    expr_t *logpdf_arg = test_expr_sub_d(two_x, 1.0);
    expr_t *logpdf_term = expr_normal_logpdf(logpdf_arg);

    assert_antiderivative_matches("integral derivative of sec(2*x - 1/2)", sec_term, x, trig_points,
                                  sizeof(trig_points) / sizeof(trig_points[0]));
    assert_antiderivative_matches("integral derivative of cosec(3*x + 0.2)", cosec_term, x, trig_points,
                                  sizeof(trig_points) / sizeof(trig_points[0]));
    assert_antiderivative_matches("integral derivative of cot(x - 0.4)", cot_term, x, trig_points,
                                  sizeof(trig_points) / sizeof(trig_points[0]));
    assert_antiderivative_matches("integral derivative of sech(2*x + 0.25)", sech_term, x, hyper_points,
                                  sizeof(hyper_points) / sizeof(hyper_points[0]));
    assert_antiderivative_matches("integral derivative of coth(x - 1.5)", coth_term, x, hyper_points,
                                  sizeof(hyper_points) / sizeof(hyper_points[0]));
    assert_antiderivative_matches("integral derivative of normal_pdf(2*x - 1)", pdf_term, x, stat_points,
                                  sizeof(stat_points) / sizeof(stat_points[0]));
    assert_antiderivative_matches("integral derivative of normal_cdf(x + 0.75)", cdf_term, x, stat_points,
                                  sizeof(stat_points) / sizeof(stat_points[0]));
    assert_antiderivative_matches("integral derivative of normal_logpdf(2*x - 1)", logpdf_term, x, stat_points,
                                  sizeof(stat_points) / sizeof(stat_points[0]));

    expr_free(logpdf_term);
    expr_free(logpdf_arg);
    expr_free(cdf_term);
    expr_free(cdf_arg);
    expr_free(pdf_term);
    expr_free(pdf_arg);
    expr_free(coth_term);
    expr_free(coth_arg);
    expr_free(sech_term);
    expr_free(sech_arg);
    expr_free(cot_term);
    expr_free(cot_arg);
    expr_free(cosec_term);
    expr_free(cosec_arg);
    expr_free(three_x);
    expr_free(sec_term);
    expr_free(sec_arg);
    expr_free(two_x);
    expr_free(x);
}

static void test_integrate_affine_logs_inverse_and_specials(void)
{
    static const double positive_points[] = {0.4, 0.8, 1.2, 2.0};
    static const double bounded_points[] = {-0.6, -0.2, 0.1, 0.5};
    static const double mixed_points[] = {-1.1, -0.3, 0.4, 1.3};
    expr_t *x = test_expr_new_named_var_d(0.0, "x");
    expr_t *two_x = test_expr_mul_d(x, 2.0);
    expr_t *log_arg = test_expr_add_d(two_x, 3.0);
    expr_t *log_term = expr_log(log_arg);
    expr_t *log10_arg = test_expr_add_d(x, 2.5);
    expr_t *log10_term = expr_log10(log10_arg);
    expr_t *atan_arg = test_expr_sub_d(two_x, 0.3);
    expr_t *atan_term = expr_atan(atan_arg);
    expr_t *asin_arg = test_expr_sub_d(x, 0.1);
    expr_t *asin_term = expr_asin(asin_arg);
    expr_t *acos_arg = test_expr_add_d(x, 0.2);
    expr_t *acos_term = expr_acos(acos_arg);
    expr_t *asinh_arg = test_expr_sub_d(two_x, 1.0);
    expr_t *asinh_term = expr_asinh(asinh_arg);
    expr_t *atanh_arg = test_expr_div_d(x, 2.0);
    expr_t *atanh_term = expr_atanh(atanh_arg);
    expr_t *erf_arg = test_expr_sub_d(x, 0.25);
    expr_t *erf_term = expr_erf(erf_arg);
    expr_t *erfc_arg = test_expr_add_d(x, 0.75);
    expr_t *erfc_term = expr_erfc(erfc_arg);
    expr_t *ei_arg = test_expr_add_d(x, 2.0);
    expr_t *ei_term = expr_ei(ei_arg);
    expr_t *e1_arg = test_expr_add_d(x, 1.5);
    expr_t *e1_term = expr_e1(e1_arg);

    assert_antiderivative_matches("integral derivative of log(2*x + 3)", log_term, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of log10(x + 2.5)", log10_term, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of atan(2*x - 0.3)", atan_term, x, mixed_points,
                                  sizeof(mixed_points) / sizeof(mixed_points[0]));
    assert_antiderivative_matches("integral derivative of asin(x - 0.1)", asin_term, x, bounded_points,
                                  sizeof(bounded_points) / sizeof(bounded_points[0]));
    assert_antiderivative_matches("integral derivative of acos(x + 0.2)", acos_term, x, bounded_points,
                                  sizeof(bounded_points) / sizeof(bounded_points[0]));
    assert_antiderivative_matches("integral derivative of asinh(2*x - 1)", asinh_term, x, mixed_points,
                                  sizeof(mixed_points) / sizeof(mixed_points[0]));
    assert_antiderivative_matches("integral derivative of atanh(x / 2)", atanh_term, x, bounded_points,
                                  sizeof(bounded_points) / sizeof(bounded_points[0]));
    assert_antiderivative_matches("integral derivative of erf(x - 0.25)", erf_term, x, mixed_points,
                                  sizeof(mixed_points) / sizeof(mixed_points[0]));
    assert_antiderivative_matches("integral derivative of erfc(x + 0.75)", erfc_term, x, mixed_points,
                                  sizeof(mixed_points) / sizeof(mixed_points[0]));
    assert_antiderivative_matches("integral derivative of Ei(x + 2)", ei_term, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of E1(x + 1.5)", e1_term, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));

    expr_free(e1_term);
    expr_free(e1_arg);
    expr_free(ei_term);
    expr_free(ei_arg);
    expr_free(erfc_term);
    expr_free(erfc_arg);
    expr_free(erf_term);
    expr_free(erf_arg);
    expr_free(atanh_term);
    expr_free(atanh_arg);
    expr_free(asinh_term);
    expr_free(asinh_arg);
    expr_free(acos_term);
    expr_free(acos_arg);
    expr_free(asin_term);
    expr_free(asin_arg);
    expr_free(atan_term);
    expr_free(atan_arg);
    expr_free(log10_term);
    expr_free(log10_arg);
    expr_free(log_term);
    expr_free(log_arg);
    expr_free(two_x);
    expr_free(x);
}

static void test_integrate_affine_powers_and_remaining_inverse_families(void)
{
    static const double positive_points[] = {0.3, 0.8, 1.4, 2.0};
    static const double asech_points[] = {0.15, 0.3, 0.6, 0.9};
    static const double outer_points[] = {1.3, 1.7, 2.1, 2.8};
    expr_t *x = test_expr_new_named_var_d(0.0, "x");
    expr_t *two_x = test_expr_mul_d(x, 2.0);
    expr_t *affine = test_expr_add_d(two_x, 1.0);
    expr_t *reciprocal = test_expr_d_div(1.0, affine);
    expr_t *affine_pow3 = test_expr_pow_d(affine, 3.0);
    expr_t *affine_sqrt = expr_sqrt(affine);
    expr_t *cosech_arg = test_expr_add_d(x, 2.0);
    expr_t *cosech_term = expr_cosech(cosech_arg);
    expr_t *asec_arg = test_expr_add_d(x, 2.5);
    expr_t *asec_term = expr_asec(asec_arg);
    expr_t *acosec_arg = test_expr_add_d(x, 2.25);
    expr_t *acosec_term = expr_acosec(acosec_arg);
    expr_t *acot_arg = test_expr_sub_d(x, 0.3);
    expr_t *acot_term = expr_acot(acot_arg);
    expr_t *acosh_arg = test_expr_add_d(x, 2.0);
    expr_t *acosh_term = expr_acosh(acosh_arg);
    expr_t *asech_arg = test_expr_div_d(x, 3.0);
    expr_t *asech_term = expr_asech(asech_arg);
    expr_t *acosech_arg = test_expr_add_d(x, 1.25);
    expr_t *acosech_term = expr_acosech(acosech_arg);
    expr_t *acoth_arg = test_expr_add_d(x, 2.4);
    expr_t *acoth_term = expr_acoth(acoth_arg);

    assert_antiderivative_matches("integral derivative of 1/(2*x + 1)", reciprocal, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of (2*x + 1)^3", affine_pow3, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of sqrt(2*x + 1)", affine_sqrt, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of cosech(x + 2)", cosech_term, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of asec(x + 2.5)", asec_term, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of acosec(x + 2.25)", acosec_term, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of acot(x - 0.3)", acot_term, x, outer_points,
                                  sizeof(outer_points) / sizeof(outer_points[0]));
    assert_antiderivative_matches("integral derivative of acosh(x + 2)", acosh_term, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of asech(x / 3)", asech_term, x, asech_points,
                                  sizeof(asech_points) / sizeof(asech_points[0]));
    assert_antiderivative_matches("integral derivative of acosech(x + 1.25)", acosech_term, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of acoth(x + 2.4)", acoth_term, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));

    expr_free(acoth_term);
    expr_free(acoth_arg);
    expr_free(acosech_term);
    expr_free(acosech_arg);
    expr_free(asech_term);
    expr_free(asech_arg);
    expr_free(acosh_term);
    expr_free(acosh_arg);
    expr_free(acot_term);
    expr_free(acot_arg);
    expr_free(acosec_term);
    expr_free(acosec_arg);
    expr_free(asec_term);
    expr_free(asec_arg);
    expr_free(cosech_term);
    expr_free(cosech_arg);
    expr_free(affine_sqrt);
    expr_free(affine_pow3);
    expr_free(reciprocal);
    expr_free(affine);
    expr_free(two_x);
    expr_free(x);
}

static void test_integrate_affine_poly_times_specials(void)
{
    static const double points[] = {-0.6, -0.1, 0.4, 1.1};
    static const double bounded_points[] = {-0.6, -0.1, 0.4, 0.8};
    static const double positive_points[] = {0.4, 0.9, 1.6, 2.5};
    static const double atanh_points[] = {-0.6, -0.1, 0.4, 0.7};
    static const double outer_points[] = {1.3, 1.7, 2.1, 2.8};
    static const double asech_points[] = {0.15, 0.3, 0.6, 0.9};
    expr_t *x = test_expr_new_named_var_d(0.0, "x");
    expr_t *sin_x = expr_sin(x);
    expr_t *cos_x = expr_cos(x);
    expr_t *exp_x = expr_exp(x);
    expr_t *tan_x = expr_tan(x);
    expr_t *sec_x = expr_sec(x);
    expr_t *cosec_x = expr_cosec(x);
    expr_t *cot_x = expr_cot(x);
    expr_t *sinh_x = expr_sinh(x);
    expr_t *cosh_x = expr_cosh(x);
    expr_t *tanh_x = expr_tanh(x);
    expr_t *sech_x = expr_sech(x);
    expr_t *cosech_x = expr_cosech(x);
    expr_t *coth_x = expr_coth(x);
    expr_t *log_x = expr_log(x);
    expr_t *atan_x = expr_atan(x);
    expr_t *asin_x = expr_asin(x);
    expr_t *acos_x = expr_acos(x);
    expr_t *acot_x = expr_acot(x);
    expr_t *asinh_x = expr_asinh(x);
    expr_t *atanh_x = expr_atanh(x);
    expr_t *x_plus_two = test_expr_add_d(x, 2.0);
    expr_t *asec_x_plus_two = expr_asec(x_plus_two);
    expr_t *acosec_x_plus_two = expr_acosec(x_plus_two);
    expr_t *acosh_x_plus_two = expr_acosh(x_plus_two);
    expr_t *x_over_three = test_expr_div_d(x, 3.0);
    expr_t *asech_x_over_three = expr_asech(x_over_three);
    expr_t *acosech_x_plus_two = expr_acosech(x_plus_two);
    expr_t *acoth_x_plus_two = expr_acoth(x_plus_two);
    expr_t *x_sin_x = expr_mul(x, sin_x);
    expr_t *x_cos_x = expr_mul(x, cos_x);
    expr_t *x_exp_x = expr_mul(x, exp_x);
    expr_t *x_sinh_x = expr_mul(x, sinh_x);
    expr_t *x_cosh_x = expr_mul(x, cosh_x);
    expr_t *x_log_x = expr_mul(x, log_x);
    expr_t *x_atan_x = expr_mul(x, atan_x);
    expr_t *x_asin_x = expr_mul(x, asin_x);
    expr_t *x_acos_x = expr_mul(x, acos_x);
    expr_t *shifted_asec_product = expr_mul(x_plus_two, asec_x_plus_two);
    expr_t *shifted_acosec_product = expr_mul(x_plus_two, acosec_x_plus_two);
    expr_t *x_acot_x = expr_mul(x, acot_x);
    expr_t *x_asinh_x = expr_mul(x, asinh_x);
    expr_t *x_atanh_x = expr_mul(x, atanh_x);
    expr_t *shifted_acosh_product = expr_mul(x_plus_two, acosh_x_plus_two);
    expr_t *scaled_asech_product = expr_mul(x_over_three, asech_x_over_three);
    expr_t *shifted_acosech_product = expr_mul(x_plus_two, acosech_x_plus_two);
    expr_t *shifted_acoth_product = expr_mul(x_plus_two, acoth_x_plus_two);
    expr_t *erf_x = expr_erf(x);
    expr_t *erfc_x = expr_erfc(x);
    expr_t *x_erf_x = expr_mul(x, erf_x);
    expr_t *x_erfc_x = expr_mul(x, erfc_x);
    expr_t *erf_x_plus_two = expr_erf(x_plus_two);
    expr_t *erfc_x_plus_two = expr_erfc(x_plus_two);
    expr_t *shifted_erf_product = expr_mul(x_plus_two, erf_x_plus_two);
    expr_t *shifted_erfc_product = expr_mul(x_plus_two, erfc_x_plus_two);
    expr_t *normal_pdf_x = expr_normal_pdf(x);
    expr_t *normal_cdf_x = expr_normal_cdf(x);
    expr_t *x_normal_pdf_x = expr_mul(x, normal_pdf_x);
    expr_t *x_normal_cdf_x = expr_mul(x, normal_cdf_x);
    expr_t *normal_pdf_x_plus_two = expr_normal_pdf(x_plus_two);
    expr_t *normal_cdf_x_plus_two = expr_normal_cdf(x_plus_two);
    expr_t *normal_logpdf_x = expr_normal_logpdf(x);
    expr_t *normal_logpdf_x_plus_two = expr_normal_logpdf(x_plus_two);
    expr_t *shifted_normal_pdf_product = expr_mul(x_plus_two, normal_pdf_x_plus_two);
    expr_t *shifted_normal_cdf_product = expr_mul(x_plus_two, normal_cdf_x_plus_two);
    expr_t *x_normal_logpdf_x = expr_mul(x, normal_logpdf_x);
    expr_t *shifted_normal_logpdf_product = expr_mul(x_plus_two, normal_logpdf_x_plus_two);
    expr_t *ei_x_plus_two = expr_ei(x_plus_two);
    expr_t *e1_x_plus_two = expr_e1(x_plus_two);
    expr_t *shifted_ei_product = expr_mul(x_plus_two, ei_x_plus_two);
    expr_t *shifted_e1_product = expr_mul(x_plus_two, e1_x_plus_two);
    expr_t *exp_x_sin_x = expr_mul(exp_x, sin_x);
    expr_t *exp_x_cos_x = expr_mul(exp_x, cos_x);
    expr_t *exp_x_sinh_x = expr_mul(exp_x, sinh_x);
    expr_t *exp_x_cosh_x = expr_mul(exp_x, cosh_x);
    expr_t *two_x = test_expr_mul_d(x, 2.0);
    expr_t *exp_2x = expr_exp(two_x);
    expr_t *sin_2x = expr_sin(two_x);
    expr_t *cos_2x = expr_cos(two_x);
    expr_t *ln_2x = expr_log(two_x);
    expr_t *x2_for_scaled = test_expr_pow_d(x, 2.0);
    expr_t *x3_for_scaled = test_expr_pow_d(x, 3.0);
    expr_t *x_exp_2x = expr_mul(x, exp_2x);
    expr_t *x2_exp_2x = expr_mul(x2_for_scaled, exp_2x);
    expr_t *x_sin_2x = expr_mul(x, sin_2x);
    expr_t *x3_sin_2x = expr_mul(x3_for_scaled, sin_2x);
    expr_t *x_cos_2x = expr_mul(x, cos_2x);
    expr_t *x2_cos_2x = expr_mul(x2_for_scaled, cos_2x);
    expr_t *ln_2x_over_x = expr_div(ln_2x, x);
    expr_t *sin_x_sq = expr_mul(sin_x, sin_x);
    expr_t *cos_x_sq = expr_mul(cos_x, cos_x);
    expr_t *tan_x_sq = expr_mul(tan_x, tan_x);
    expr_t *sec_x_sq = expr_mul(sec_x, sec_x);
    expr_t *cosec_x_sq = expr_mul(cosec_x, cosec_x);
    expr_t *sec_x_tan_x = expr_mul(sec_x, tan_x);
    expr_t *cosec_x_cot_x = expr_mul(cosec_x, cot_x);
    expr_t *sin_x_cos_x = expr_mul(sin_x, cos_x);
    expr_t *sinh_x_sq = expr_mul(sinh_x, sinh_x);
    expr_t *cosh_x_sq = expr_mul(cosh_x, cosh_x);
    expr_t *tanh_x_sq = expr_mul(tanh_x, tanh_x);
    expr_t *sech_x_sq = expr_mul(sech_x, sech_x);
    expr_t *cosech_x_sq = expr_mul(cosech_x, cosech_x);
    expr_t *coth_x_sq = expr_mul(coth_x, coth_x);
    expr_t *sech_x_tanh_x = expr_mul(sech_x, tanh_x);
    expr_t *cosech_x_coth_x = expr_mul(cosech_x, coth_x);
    expr_t *sinh_x_cosh_x = expr_mul(sinh_x, cosh_x);
    expr_t *x_plus_one = test_expr_add_d(x, 1.0);
    expr_t *x_over_x_plus_one = expr_div(x, x_plus_one);

    assert_antiderivative_matches("integral derivative of x*sin(x)", x_sin_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x*cos(x)", x_cos_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x*exp(x)", x_exp_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x*sinh(x)", x_sinh_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x*cosh(x)", x_cosh_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x*log(x)", x_log_x, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of x*atan(x)", x_atan_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x*asin(x)", x_asin_x, x, bounded_points,
                                  sizeof(bounded_points) / sizeof(bounded_points[0]));
    assert_antiderivative_matches("integral derivative of x*acos(x)", x_acos_x, x, bounded_points,
                                  sizeof(bounded_points) / sizeof(bounded_points[0]));
    assert_antiderivative_matches("integral derivative of (x + 2)*asec(x + 2)", shifted_asec_product, x,
                                  positive_points, sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of (x + 2)*acosec(x + 2)", shifted_acosec_product, x,
                                  positive_points, sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of x*acot(x)", x_acot_x, x, outer_points,
                                  sizeof(outer_points) / sizeof(outer_points[0]));
    assert_antiderivative_matches("integral derivative of x*asinh(x)", x_asinh_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x*atanh(x)", x_atanh_x, x, atanh_points,
                                  sizeof(atanh_points) / sizeof(atanh_points[0]));
    assert_antiderivative_matches("integral derivative of (x + 2)*acosh(x + 2)", shifted_acosh_product, x,
                                  positive_points, sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of (x / 3)*asech(x / 3)", scaled_asech_product, x, asech_points,
                                  sizeof(asech_points) / sizeof(asech_points[0]));
    assert_antiderivative_matches("integral derivative of (x + 2)*acosech(x + 2)", shifted_acosech_product, x,
                                  positive_points, sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of (x + 2)*acoth(x + 2)", shifted_acoth_product, x,
                                  positive_points, sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of x*erf(x)", x_erf_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x*erfc(x)", x_erfc_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of (x + 2)*erf(x + 2)", shifted_erf_product, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of (x + 2)*erfc(x + 2)", shifted_erfc_product, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x*normal_pdf(x)", x_normal_pdf_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x*normal_cdf(x)", x_normal_cdf_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of (x + 2)*normal_pdf(x + 2)", shifted_normal_pdf_product, x,
                                  points, sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of (x + 2)*normal_cdf(x + 2)", shifted_normal_cdf_product, x,
                                  points, sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x*normal_logpdf(x)", x_normal_logpdf_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of (x + 2)*normal_logpdf(x + 2)", shifted_normal_logpdf_product,
                                  x, points, sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of (x + 2)*Ei(x + 2)", shifted_ei_product, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of (x + 2)*E1(x + 2)", shifted_e1_product, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of exp(x)*sin(x)", exp_x_sin_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of exp(x)*cos(x)", exp_x_cos_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of exp(x)*sinh(x)", exp_x_sinh_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of exp(x)*cosh(x)", exp_x_cosh_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x*exp(2*x)", x_exp_2x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x^2*exp(2*x)", x2_exp_2x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x*sin(2*x)", x_sin_2x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x^3*sin(2*x)", x3_sin_2x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x*cos(2*x)", x_cos_2x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x^2*cos(2*x)", x2_cos_2x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of ln(2*x)/x", ln_2x_over_x, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of sin(x)^2", sin_x_sq, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of cos(x)^2", cos_x_sq, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of tan(x)^2", tan_x_sq, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of sec(x)^2", sec_x_sq, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of cosec(x)^2", cosec_x_sq, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of sec(x)*tan(x)", sec_x_tan_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of cosec(x)*cot(x)", cosec_x_cot_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of sin(x)*cos(x)", sin_x_cos_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of sinh(x)^2", sinh_x_sq, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of cosh(x)^2", cosh_x_sq, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of tanh(x)^2", tanh_x_sq, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of sech(x)^2", sech_x_sq, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of cosech(x)^2", cosech_x_sq, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of coth(x)^2", coth_x_sq, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));
    assert_antiderivative_matches("integral derivative of sech(x)*tanh(x)", sech_x_tanh_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of cosech(x)*coth(x)", cosech_x_coth_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of sinh(x)*cosh(x)", sinh_x_cosh_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x/(x + 1)", x_over_x_plus_one, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));

    expr_free(x_over_x_plus_one);
    expr_free(x_plus_one);
    expr_free(sinh_x_cosh_x);
    expr_free(cosech_x_coth_x);
    expr_free(sech_x_tanh_x);
    expr_free(coth_x_sq);
    expr_free(cosech_x_sq);
    expr_free(sech_x_sq);
    expr_free(tanh_x_sq);
    expr_free(cosh_x_sq);
    expr_free(sinh_x_sq);
    expr_free(sin_x_cos_x);
    expr_free(cosec_x_sq);
    expr_free(sec_x_sq);
    expr_free(tan_x_sq);
    expr_free(cosec_x_cot_x);
    expr_free(sec_x_tan_x);
    expr_free(cos_x_sq);
    expr_free(sin_x_sq);
    expr_free(exp_x_cosh_x);
    expr_free(exp_x_sinh_x);
    expr_free(exp_x_cos_x);
    expr_free(exp_x_sin_x);
    expr_free(ln_2x_over_x);
    expr_free(x2_cos_2x);
    expr_free(x_cos_2x);
    expr_free(x3_sin_2x);
    expr_free(x_sin_2x);
    expr_free(x2_exp_2x);
    expr_free(x_exp_2x);
    expr_free(x3_for_scaled);
    expr_free(x2_for_scaled);
    expr_free(ln_2x);
    expr_free(cos_2x);
    expr_free(sin_2x);
    expr_free(exp_2x);
    expr_free(two_x);
    expr_free(shifted_e1_product);
    expr_free(shifted_ei_product);
    expr_free(e1_x_plus_two);
    expr_free(ei_x_plus_two);
    expr_free(shifted_erfc_product);
    expr_free(shifted_erf_product);
    expr_free(erfc_x_plus_two);
    expr_free(erf_x_plus_two);
    expr_free(shifted_normal_cdf_product);
    expr_free(shifted_normal_pdf_product);
    expr_free(shifted_normal_logpdf_product);
    expr_free(x_normal_logpdf_x);
    expr_free(normal_logpdf_x_plus_two);
    expr_free(normal_logpdf_x);
    expr_free(normal_cdf_x_plus_two);
    expr_free(normal_pdf_x_plus_two);
    expr_free(x_normal_cdf_x);
    expr_free(x_normal_pdf_x);
    expr_free(normal_cdf_x);
    expr_free(normal_pdf_x);
    expr_free(x_erfc_x);
    expr_free(x_erf_x);
    expr_free(erfc_x);
    expr_free(erf_x);
    expr_free(shifted_acoth_product);
    expr_free(shifted_acosech_product);
    expr_free(scaled_asech_product);
    expr_free(shifted_acosh_product);
    expr_free(x_atanh_x);
    expr_free(x_asinh_x);
    expr_free(x_acot_x);
    expr_free(shifted_acosec_product);
    expr_free(shifted_asec_product);
    expr_free(x_acos_x);
    expr_free(x_asin_x);
    expr_free(x_atan_x);
    expr_free(x_log_x);
    expr_free(x_cosh_x);
    expr_free(x_sinh_x);
    expr_free(x_exp_x);
    expr_free(x_cos_x);
    expr_free(x_sin_x);
    expr_free(acosh_x_plus_two);
    expr_free(x_plus_two);
    expr_free(acoth_x_plus_two);
    expr_free(acosech_x_plus_two);
    expr_free(asech_x_over_three);
    expr_free(x_over_three);
    expr_free(atanh_x);
    expr_free(asinh_x);
    expr_free(acot_x);
    expr_free(acosec_x_plus_two);
    expr_free(asec_x_plus_two);
    expr_free(acos_x);
    expr_free(asin_x);
    expr_free(atan_x);
    expr_free(log_x);
    expr_free(coth_x);
    expr_free(cosech_x);
    expr_free(sech_x);
    expr_free(tanh_x);
    expr_free(cot_x);
    expr_free(cosh_x);
    expr_free(sinh_x);
    expr_free(cosec_x);
    expr_free(sec_x);
    expr_free(tan_x);
    expr_free(exp_x);
    expr_free(cos_x);
    expr_free(sin_x);
    expr_free(x);
}

static void test_integrate_other_variable_as_constant(void)
{
    static const double points[] = {-1.0, 0.0, 2.0};
    expr_t *x = test_expr_new_named_var_d(0.0, "x");
    expr_t *y = test_expr_new_named_var_d(7.0, "y");

    assert_antiderivative_matches("integral derivative treats y as constant", y, x, points,
                                  sizeof(points) / sizeof(points[0]));

    expr_free(y);
    expr_free(x);
}

static void test_integrate_definite_symbolic_bounds(void)
{
    expr_t *x = test_expr_new_named_var_d(0.0, "x");
    expr_t *a = test_expr_new_named_var_d(0.0, "a");
    expr_t *b = test_expr_new_named_var_d(0.0, "b");
    expr_t *x2 = test_expr_pow_d(x, 2.0);
    expr_t *anti = expr_integrate(x2, x);
    expr_t *upper = anti ? expr_substitute(anti, x, b) : NULL;
    expr_t *lower = anti ? expr_substitute(anti, x, a) : NULL;
    expr_t *diff = (upper && lower) ? expr_sub(upper, lower) : NULL;
    expr_t *simplified = diff ? expr_simplify(diff) : NULL;
    char *text = simplified ? expr_to_string(simplified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(upper);
    ASSERT_NOT_NULL(lower);
    ASSERT_NOT_NULL(diff);
    ASSERT_NOT_NULL(simplified);
    ASSERT_NOT_NULL(text);
    print_antiderivative_expr("{ x^2 }", anti);
    ASSERT_TRUE(strstr(text, "a") != NULL);
    ASSERT_TRUE(strstr(text, "b") != NULL);

    test_expr_set_val_d(a, 2.0);
    test_expr_set_val_d(b, 5.0);
    check_q_at(__FILE__, __LINE__, 1, "definite integral x^2 from a to b", expr_eval_qf(simplified),
               qf_from_double(39.0));

    test_expr_set_val_d(a, 0.0);
    test_expr_set_val_d(b, 1.0);
    check_q_at(__FILE__, __LINE__, 1, "definite integral x^2 from 0 to 1", expr_eval_qf(simplified),
               qf_div(qf_from_double(1.0), qf_from_double(3.0)));

    free(text);
    expr_free(simplified);
    expr_free(diff);
    expr_free(lower);
    expr_free(upper);
    expr_free(anti);
    expr_free(x2);
    expr_free(b);
    expr_free(a);
    expr_free(x);
}

static void test_integrate_definite_quadratic_compact_exact_result(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *integrand = expr_from_string("{ (x+1)/(x^2+3*x+5) }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    expr_t *zero = expr_new_const(NUM_ZERO);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    expr_t *upper = (anti && one) ? expr_substitute(anti, x, one) : NULL;
    expr_t *lower = (anti && zero) ? expr_substitute(anti, x, zero) : NULL;
    expr_t *difference = (upper && lower) ? expr_sub(upper, lower) : NULL;
    expr_t *simplified = difference ? expr_simplify(difference) : NULL;
    char *text = simplified ? expr_to_string(simplified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(text);
    ASSERT_TRUE(str_eq(text, "½·ln(9/5) - atan(√(11)/13)/√(11)"));

    free(text);
    expr_free(simplified);
    expr_free(difference);
    expr_free(lower);
    expr_free(upper);
    expr_free(anti);
    expr_free(one);
    expr_free(zero);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);
}

static void test_integrate_definite_quadratic_combines_exact_constants(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *integrand = expr_from_string("{ (x+1)/(x^2+4*x+5) }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    expr_t *zero = expr_new_const(NUM_ZERO);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    expr_t *upper = (anti && one) ? expr_substitute(anti, x, one) : NULL;
    expr_t *lower = (anti && zero) ? expr_substitute(anti, x, zero) : NULL;
    expr_t *difference = (upper && lower) ? expr_sub(upper, lower) : NULL;
    expr_t *simplified = difference ? expr_simplify(difference) : NULL;
    char *text = simplified ? expr_to_string(simplified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(text);
    TEST_ASSERT_STR_EQ(text, "½·ln(2) - atan(⅐)");

    free(text);
    expr_free(simplified);
    expr_free(difference);
    expr_free(lower);
    expr_free(upper);
    expr_free(anti);
    expr_free(one);
    expr_free(zero);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);
}

static void test_integrate_inverse_square_with_exact_bound(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *one_over_x2 = expr_from_string("{ 1/x^2 }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *simplified_integrand = one_over_x2 ? expr_simplify(one_over_x2) : NULL;
    expr_t *one_raw = expr_from_string("{ 1 }", NULL);
    expr_t *sqrt_three_raw = expr_from_string("{ sqrt(3) }", NULL);
    expr_t *one = one_raw ? expr_simplify(one_raw) : NULL;
    expr_t *sqrt_three = sqrt_three_raw ? expr_simplify(sqrt_three_raw) : NULL;
    expr_t *anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    expr_t *upper = (anti && sqrt_three) ? expr_substitute(anti, x, sqrt_three) : NULL;
    expr_t *lower = (anti && one) ? expr_substitute(anti, x, one) : NULL;
    expr_t *diff = (upper && lower) ? expr_sub(upper, lower) : NULL;
    expr_t *simplified = diff ? expr_simplify(diff) : NULL;
    number_t value = simplified ? expr_eval(simplified) : num_new();

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(upper);
    ASSERT_NOT_NULL(lower);
    ASSERT_NOT_NULL(diff);
    ASSERT_NOT_NULL(simplified);
    print_antiderivative_expr("{ 1/x^2 }", anti);
    ASSERT_FALSE(num_is_nan(value));
    check_q_at(__FILE__, __LINE__, 1, "definite integral 1/x^2 from 1 to sqrt(3)", expr_eval_qf(simplified),
               qf_from_double(1.0 - 1.0 / sqrt(3.0)));

    num_destroy(&value);
    expr_free(simplified);
    expr_free(diff);
    expr_free(lower);
    expr_free(upper);
    expr_free(anti);
    expr_free(sqrt_three);
    expr_free(one);
    expr_free(sqrt_three_raw);
    expr_free(one_raw);
    expr_free(simplified_integrand);
    expr_free(one_over_x2);
    expr_bindings_free(bindings);
}

static void assert_symbolic_shifted_inverse_square(const char *input, const char *first_term, const char *second_term)
{
    expr_bindings_t *bindings = NULL;
    expr_t *integrand = expr_from_string(input, &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    expr_t *one_raw = expr_from_string("{ 1 }", NULL);
    expr_t *sqrt_three_raw = expr_from_string("{ sqrt(3) }", NULL);
    expr_t *one = one_raw ? expr_simplify(one_raw) : NULL;
    expr_t *sqrt_three = sqrt_three_raw ? expr_simplify(sqrt_three_raw) : NULL;
    expr_t *anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    expr_t *upper = (anti && sqrt_three) ? expr_substitute(anti, x, sqrt_three) : NULL;
    expr_t *lower = (anti && one) ? expr_substitute(anti, x, one) : NULL;
    expr_t *diff = (upper && lower) ? expr_sub(upper, lower) : NULL;
    expr_t *simplified = diff ? expr_simplify(diff) : NULL;
    char *text = simplified ? expr_to_string(simplified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(upper);
    ASSERT_NOT_NULL(lower);
    ASSERT_NOT_NULL(diff);
    ASSERT_NOT_NULL(simplified);
    ASSERT_NOT_NULL(text);
    print_antiderivative_expr(input, anti);
    ASSERT_TRUE(strstr(text, first_term) != NULL);
    ASSERT_TRUE(strstr(text, second_term) != NULL);

    free(text);
    expr_free(simplified);
    expr_free(diff);
    expr_free(lower);
    expr_free(upper);
    expr_free(anti);
    expr_free(sqrt_three);
    expr_free(one);
    expr_free(sqrt_three_raw);
    expr_free(one_raw);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);
}

static void test_integrate_shifted_inverse_square_with_symbolic_constant(void)
{
    assert_symbolic_shifted_inverse_square("{ 1/(x+c)^2 }", "1/(c + 1)", "1/(c + √(3))");
    assert_symbolic_shifted_inverse_square("{ 1/(c-x)^2 }", "1/(c - √(3))", "1/(c - 1)");
}

static void test_integrate_symbolic_power_exponent(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *integrand = expr_from_string("{ x^n }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    expr_t *one_raw = expr_from_string("{ 1 }", NULL);
    expr_t *sqrt_three_raw = expr_from_string("{ sqrt(3) }", NULL);
    expr_t *one = one_raw ? expr_simplify(one_raw) : NULL;
    expr_t *sqrt_three = sqrt_three_raw ? expr_simplify(sqrt_three_raw) : NULL;
    expr_t *anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    expr_t *upper = (anti && sqrt_three) ? expr_substitute(anti, x, sqrt_three) : NULL;
    expr_t *lower = (anti && one) ? expr_substitute(anti, x, one) : NULL;
    expr_t *diff = (upper && lower) ? expr_sub(upper, lower) : NULL;
    expr_t *simplified = diff ? expr_simplify(diff) : NULL;
    char *anti_text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;
    char *text = simplified ? expr_to_string(simplified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(anti_text);
    print_antiderivative_text("{ x^n }", anti_text);
    ASSERT_TRUE(strstr(anti_text, "x^(n + 1)/(n + 1)") != NULL);
    ASSERT_NOT_NULL(upper);
    ASSERT_NOT_NULL(lower);
    ASSERT_NOT_NULL(diff);
    ASSERT_NOT_NULL(simplified);
    ASSERT_NOT_NULL(text);
    ASSERT_TRUE(strstr(text, "√(3)^(n + 1)/(n + 1)") != NULL);
    ASSERT_TRUE(strstr(text, "1/(n + 1)") != NULL);
    ASSERT_TRUE(strstr(text, "1^") == NULL);

    free(text);
    free(anti_text);
    expr_free(simplified);
    expr_free(diff);
    expr_free(lower);
    expr_free(upper);
    expr_free(anti);
    expr_free(sqrt_three);
    expr_free(one);
    expr_free(sqrt_three_raw);
    expr_free(one_raw);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);

    bindings = NULL;
    integrand = expr_from_string("{ (x+1)^n }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    anti_text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(anti_text);
    print_antiderivative_text("{ (x+1)^n }", anti_text);
    ASSERT_TRUE(strstr(anti_text, "(x + 1)^(n + 1)/(n + 1)") != NULL);

    free(anti_text);
    expr_free(anti);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);

    bindings = NULL;
    integrand = expr_from_string("{ (a*x+b)^n }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    anti_text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(anti_text);
    print_antiderivative_text("{ (a*x+b)^n }", anti_text);
    ASSERT_TRUE(strstr(anti_text, "(ax + b)^(n + 1)/(a·(n + 1))") != NULL);

    free(anti_text);
    expr_free(anti);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);

    bindings = NULL;
    integrand = expr_from_string("{ (b-a*x)^n }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    anti_text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(anti_text);
    print_antiderivative_text("{ (b-a*x)^n }", anti_text);
    ASSERT_TRUE(strstr(anti_text, "-(b - ax)^(n + 1)/(a·(n + 1))") != NULL);

    free(anti_text);
    expr_free(anti);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);
}

static void test_integrate_constant_base_affine_exponent(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *integrand = expr_from_string("{ a^(3*x+1) | x = NAN; a = NAN }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *antiderivative = (integrand && x) ? expr_integrate(integrand, x) : NULL;
    expr_t *derivative = (antiderivative && x) ? expr_create_deriv(antiderivative, x) : NULL;
    expr_t *difference = derivative ? expr_sub(derivative, integrand) : NULL;
    expr_t *simplified = difference ? expr_simplify(difference) : NULL;
    char *antiderivative_text = antiderivative ? expr_to_string(antiderivative, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(antiderivative);
    ASSERT_NOT_NULL(antiderivative_text);
    ASSERT_TRUE(strstr(antiderivative_text, "a^(3x + 1)") != NULL);
    ASSERT_TRUE(strstr(antiderivative_text, "ln(a)") != NULL);
    ASSERT_NOT_NULL(simplified);
    ASSERT_TRUE(expr_is_exact_zero(simplified));

    free(antiderivative_text);
    expr_free(simplified);
    expr_free(difference);
    expr_free(derivative);
    expr_free(antiderivative);
    expr_free(integrand);
    expr_bindings_free(bindings);
}

static void test_integrate_symbolic_shifted_sqrt(void)
{
    static const double quotient_minus_points[] = {0.25, 1.0, 2.0, 3.5};
    static const double root_denom_minus_points[] = {2.5, 3.0, 4.5, 7.0};
    static const double affine_root_points[] = {0.5, 1.0, 1.75, 2.5};
    expr_bindings_t *bindings = NULL;
    expr_t *integrand = expr_from_string("{ sqrt(x-a) }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    expr_t *anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    char *text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;
    char *tex = NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(text);
    print_antiderivative_text("{ sqrt(x-a) }", text);
    ASSERT_TRUE(strstr(text, "⅔·(x - a)^³⁄₂") != NULL);

    free(text);
    expr_free(anti);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);

    bindings = NULL;
    integrand = expr_from_string("{ sqrt(x+a) }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(text);
    print_antiderivative_text("{ sqrt(x+a) }", text);
    ASSERT_TRUE(strstr(text, "⅔·(x + a)^³⁄₂") != NULL);

    free(text);
    expr_free(anti);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);

    bindings = NULL;
    integrand = expr_from_string("{ sqrt(a+x) }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(text);
    print_antiderivative_text("{ sqrt(a+x) }", text);
    ASSERT_TRUE(strstr(text, "⅔·(x + a)^³⁄₂") != NULL);

    free(text);
    expr_free(anti);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);

    bindings = NULL;
    integrand = expr_from_string("{ sqrt(a-x) }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(text);
    print_antiderivative_text("{ sqrt(a-x) }", text);
    ASSERT_TRUE(strstr(text, "-⅔·(a - x)^³⁄₂") != NULL);

    free(text);
    expr_free(anti);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);

    bindings = NULL;
    integrand = expr_from_string("{ 1/sqrt(x-a) }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(text);
    print_antiderivative_text("{ 1/sqrt(x-a) }", text);
    ASSERT_TRUE(strstr(text, "2·√(x - a)") != NULL);

    free(text);
    expr_free(anti);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);

    bindings = NULL;
    integrand = expr_from_string("{ 1/sqrt(x+a) }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(text);
    print_antiderivative_text("{ 1/sqrt(x+a) }", text);
    ASSERT_TRUE(strstr(text, "2·√(x + a)") != NULL);

    free(text);
    expr_free(anti);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);

    bindings = NULL;
    integrand = expr_from_string("{ 1/sqrt(a+x) }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(text);
    print_antiderivative_text("{ 1/sqrt(a+x) }", text);
    ASSERT_TRUE(strstr(text, "2·√(x + a)") != NULL);

    free(text);
    expr_free(anti);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);

    bindings = NULL;
    integrand = expr_from_string("{ 1/sqrt(a-x) }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;
    tex = anti ? expr_to_string(anti, style_LATEX) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(text);
    ASSERT_NOT_NULL(tex);
    print_antiderivative_text("{ 1/sqrt(a-x) }", text);
    ASSERT_TRUE(strstr(text, "-2·√(a - x)") != NULL);
    ASSERT_TRUE(strstr(tex, "\\sqrt{a - x}") != NULL);
    ASSERT_TRUE(strstr(tex, "^{\\frac{1}{2}}") == NULL);

    free(tex);
    free(text);
    expr_free(anti);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);

    assert_string_antiderivative_matches_with_a("{ sqrt(x/(a-x)) }", 5.0, quotient_minus_points,
                                                sizeof(quotient_minus_points) / sizeof(quotient_minus_points[0]));
    assert_string_antiderivative_contains("{ sqrt(x/(a+x)) }", "atanh");
    assert_string_antiderivative_contains("{ x/sqrt(x+a) }", "⅔·(x - 2a)·√(x + a)");
    assert_string_antiderivative_matches_with_a("{ x/sqrt(x-a) }", 2.0, root_denom_minus_points,
                                                sizeof(root_denom_minus_points) / sizeof(root_denom_minus_points[0]));
    assert_string_antiderivative_contains("{ x/sqrt(x-a) }", "⅔·(x + 2a)·√(x - a)");
    assert_string_antiderivative_matches_with_ab("{ x/sqrt(a*x+b) }", 2.0, 3.0, affine_root_points,
                                                 sizeof(affine_root_points) / sizeof(affine_root_points[0]));
    assert_string_antiderivative_contains("{ x/sqrt(a*x+b) }", "⅔·(ax - 2b)·√(ax + b)/a²");

    bindings = NULL;
    integrand = expr_from_string("{ sqrt(a-bx) }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(text);
    print_antiderivative_text("{ sqrt(a-bx) }", text);
    ASSERT_TRUE(strstr(text, "-⅔·(a - bx)^³⁄₂/b") != NULL);

    free(text);
    expr_free(anti);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);

    bindings = NULL;
    integrand = expr_from_string("{ 1/sqrt(a-bx) }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;
    tex = anti ? expr_to_string(anti, style_LATEX) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(text);
    ASSERT_NOT_NULL(tex);
    print_antiderivative_text("{ 1/sqrt(a-bx) }", text);
    ASSERT_TRUE(strstr(text, "-2·√(a - bx)/b") != NULL);
    ASSERT_TRUE(strstr(tex, "\\sqrt{a - b x}") != NULL);
    ASSERT_TRUE(strstr(tex, "^{\\frac{1}{2}}") == NULL);

    free(tex);
    free(text);
    expr_free(anti);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);

    bindings = NULL;
    integrand = expr_from_string("{ 1/sqrt((a-bx)^2) }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;

    ASSERT_NULL(anti);

    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);

    bindings = NULL;
    integrand = expr_from_string("{ 1/sqrt((a-bx)^3) }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;
    tex = anti ? expr_to_string(anti, style_LATEX) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(text);
    ASSERT_NOT_NULL(tex);
    print_antiderivative_text("{ 1/sqrt((a-bx)^3) }", text);
    ASSERT_TRUE(strstr(text, "2/(b·√(a - bx))") != NULL);
    ASSERT_TRUE(strstr(tex, "\\frac{2}{b \\cdot \\sqrt{a - b x}}") != NULL);
    ASSERT_TRUE(strstr(tex, "\\sqrt{a - b x}") != NULL);
    ASSERT_TRUE(strstr(tex, "^{\\frac{1}{2}}") == NULL);

    free(tex);
    free(text);
    expr_free(anti);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);

    bindings = NULL;
    integrand = expr_from_string("{ 1/sqrt((a-bx)^4) }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;
    tex = anti ? expr_to_string(anti, style_LATEX) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(text);
    ASSERT_NOT_NULL(tex);
    print_antiderivative_text("{ 1/sqrt((a-bx)^4) }", text);
    ASSERT_TRUE(strstr(text, "1/(b·(a - bx))") != NULL);
    ASSERT_TRUE(strstr(tex, "\\frac{1}{b \\cdot \\left(a - b x\\right)}") != NULL);

    free(tex);
    free(text);
    expr_free(anti);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);

    bindings = NULL;
    integrand = expr_from_string("{ 1/sqrt((a-bx)^5) }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    simplified_integrand = integrand ? expr_simplify(integrand) : NULL;
    anti = simplified_integrand ? expr_integrate(simplified_integrand, x) : NULL;
    text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;
    tex = anti ? expr_to_string(anti, style_LATEX) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(text);
    ASSERT_NOT_NULL(tex);
    print_antiderivative_text("{ 1/sqrt((a-bx)^5) }", text);
    ASSERT_TRUE(strstr(text, "2/(3b·(a - bx)^³⁄₂)") != NULL);
    ASSERT_TRUE(strstr(tex, "\\frac{2}{3 b \\cdot \\left(a - b x\\right)^{\\frac{3}{2}}}") != NULL);
    ASSERT_TRUE(strstr(tex, "\\frac{\\frac{2}{3}}") == NULL);

    free(tex);
    free(text);
    expr_free(anti);
    expr_free(simplified_integrand);
    expr_free(integrand);
    expr_bindings_free(bindings);
}

static void test_integrate_poly_times_affine_power(void)
{
    static const double positive_points[] = {0.5, 1.0, 1.7, 2.5};
    static const double shifted_positive_points[] = {2.5, 3.0, 3.7, 4.5};
    static const double reflected_points[] = {-1.5, -0.5, 0.25, 1.0};

    assert_string_antiderivative_matches("{ x*(x+2)^3 }", positive_points,
                                         sizeof(positive_points) / sizeof(positive_points[0]));
    assert_string_antiderivative_matches("{ x/(x+2)^2 }", positive_points,
                                         sizeof(positive_points) / sizeof(positive_points[0]));
    assert_string_antiderivative_matches("{ x*sqrt(x-2) }", shifted_positive_points,
                                         sizeof(shifted_positive_points) / sizeof(shifted_positive_points[0]));
    assert_string_antiderivative_matches("{ x/sqrt(x+2) }", positive_points,
                                         sizeof(positive_points) / sizeof(positive_points[0]));
    assert_string_antiderivative_matches("{ x/sqrt(2-x) }", reflected_points,
                                         sizeof(reflected_points) / sizeof(reflected_points[0]));
    assert_string_antiderivative_matches("{ x*sqrt(x+2) }", positive_points,
                                         sizeof(positive_points) / sizeof(positive_points[0]));
    assert_string_antiderivative_matches("{ x*sqrt(2*x+3) }", positive_points,
                                         sizeof(positive_points) / sizeof(positive_points[0]));
    assert_string_antiderivative_matches_with_ab("{ x*sqrt(a*x+b) }", 2.0, 3.0, positive_points,
                                                 sizeof(positive_points) / sizeof(positive_points[0]));
    assert_string_antiderivative_matches_with_ab("{ x^2/sqrt(a*x+b) }", 2.0, 3.0, positive_points,
                                                 sizeof(positive_points) / sizeof(positive_points[0]));
    assert_string_antiderivative_matches_with_ab("{ x^2*sqrt(a*x+b) }", 2.0, 3.0, positive_points,
                                                 sizeof(positive_points) / sizeof(positive_points[0]));
}

static void test_integrate_poly_over_centered_quadratic(void)
{
    static const double points[] = {-1.5, -0.4, 0.6, 1.8};

    assert_string_antiderivative_matches("{ 1/(4+x^2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ x/(4+x^2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ x^2/(4+x^2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ x^3/(4+x^2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ 1/(9+4*x^2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ 1/(x^2+2) }", "1/√(2)·atan(x/√(2))");
    assert_string_antiderivative_matches_with_a("{ 1/(x^2+a^2) }", 2.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_a("{ 1/(a^2+x^2) }", 2.0, points, sizeof(points) / sizeof(points[0]));
}

static void test_integrate_symbolic_general_quadratic_denominator(void)
{
    static const double points[] = {-1.0, -0.25, 0.5, 2.0};
    expr_bindings_t *bindings = NULL;
    expr_bindings_t *completed_square_bindings = NULL;
    expr_bindings_t *hidden_zero_bindings = NULL;
    expr_t *antiderivative = expr_from_string("{ 1/2*(ln(x^2+3*x+5)-2*atan((2*x+3)/sqrt(11))/sqrt(11)) }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *derivative = (antiderivative && x) ? expr_create_deriv(antiderivative, x) : NULL;
    char *derivative_text = derivative ? expr_to_string(derivative, style_UNBOUND) : NULL;
    expr_t *completed_square_antiderivative =
        expr_from_string("{ 1/2*(ln(x^2+4*x+5)-2*atan(x+2)) }", &completed_square_bindings);
    expr_t *completed_square_x = completed_square_bindings ? expr_bindings_get(completed_square_bindings, "x") : NULL;
    expr_t *completed_square_derivative = (completed_square_antiderivative && completed_square_x)
                                              ? expr_create_deriv(completed_square_antiderivative, completed_square_x)
                                              : NULL;
    char *completed_square_derivative_text =
        completed_square_derivative ? expr_to_string(completed_square_derivative, style_UNBOUND) : NULL;
    expr_t *hidden_zero_antiderivative = expr_from_string("{ 1/2*(ln(x^2+4*x+5)-2*atan(x+2)"
                                                          "+2*(2*x+x*(x+2)+5)/(x^2+4*x+5)-2) }",
                                                          &hidden_zero_bindings);
    expr_t *hidden_zero_x = hidden_zero_bindings ? expr_bindings_get(hidden_zero_bindings, "x") : NULL;
    expr_t *hidden_zero_derivative = (hidden_zero_antiderivative && hidden_zero_x)
                                         ? expr_create_deriv(hidden_zero_antiderivative, hidden_zero_x)
                                         : NULL;
    char *hidden_zero_derivative_text =
        hidden_zero_derivative ? expr_to_string(hidden_zero_derivative, style_UNBOUND) : NULL;

    assert_string_antiderivative_contains("{ 1/(a*x^2+b*x+c) }", "2·atan((2ax + b)/√(4ac - b²))/√(4ac - b²)");
    assert_string_antiderivative_contains("{ x/(a*x^2+b*x+c) }", "ln(ax² + bx + c)/a");
    assert_string_antiderivative_contains("{ x/(a*x^2+b*x+c) }", "atan((2ax + b)/√(4ac - b²))");
    assert_string_antiderivative_matches("{ (x+1)/(x^2+3*x+5) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ (x+1)/(x^2+3*x+5) }", "atan((2x + 3)/√(11))/√(11)");
    assert_string_antiderivative_contains("{ (x+1)/(x^2+4*x+5) }", "atan(x + 2)");

    ASSERT_NOT_NULL(derivative_text);
    if (str_eq(derivative_text, "(x + 1)/(x² + 3x + 5)"))
        to_string_pass("exact quadratic antiderivative simplifies back", derivative_text, "(x + 1)/(x² + 3x + 5)");
    else
        to_string_fail(__FILE__, __LINE__, 1, "exact quadratic antiderivative simplifies back", derivative_text,
                       "(x + 1)/(x² + 3x + 5)");

    ASSERT_NOT_NULL(completed_square_derivative_text);
    if (str_eq(completed_square_derivative_text, "(x + 1)/(x² + 4x + 5)"))
        to_string_pass("completed-square denominators combine", completed_square_derivative_text,
                       "(x + 1)/(x² + 4x + 5)");
    else
        to_string_fail(__FILE__, __LINE__, 1, "completed-square denominators combine", completed_square_derivative_text,
                       "(x + 1)/(x² + 4x + 5)");

    ASSERT_NOT_NULL(hidden_zero_derivative_text);
    if (str_eq(hidden_zero_derivative_text, "(x + 1)/(x² + 4x + 5)"))
        to_string_pass("polynomial quotient hidden zero simplifies", hidden_zero_derivative_text,
                       "(x + 1)/(x² + 4x + 5)");
    else
        to_string_fail(__FILE__, __LINE__, 1, "polynomial quotient hidden zero simplifies", hidden_zero_derivative_text,
                       "(x + 1)/(x² + 4x + 5)");

    free(hidden_zero_derivative_text);
    expr_free(hidden_zero_derivative);
    expr_free(hidden_zero_antiderivative);
    expr_bindings_free(hidden_zero_bindings);
    free(completed_square_derivative_text);
    expr_free(completed_square_derivative);
    expr_free(completed_square_antiderivative);
    expr_bindings_free(completed_square_bindings);
    free(derivative_text);
    expr_free(derivative);
    expr_free(antiderivative);
    expr_bindings_free(bindings);
}

static void test_integrate_symbolic_general_quadratic_roots(void)
{
    static const double points[] = {-1.2, -0.2, 0.5, 1.6};

    assert_string_antiderivative_matches_with_abc("{ sqrt(a*x^2+b*x+c) }", 2.0, 3.0, 5.0, points,
                                                  sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_abc("{ 1/sqrt(a*x^2+b*x+c) }", 2.0, 3.0, 5.0, points,
                                                  sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_abc("{ x/sqrt(a*x^2+b*x+c) }", 2.0, 3.0, 5.0, points,
                                                  sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_abc("{ x*sqrt(a*x^2+b*x+c) }", 2.0, 3.0, 5.0, points,
                                                  sizeof(points) / sizeof(points[0]));
}

static void test_integrate_log_quadratic(void)
{
    static const double points[] = {-1.5, -0.4, 0.6, 1.8};
    static const double positive_points[] = {0.2, 0.7, 1.3, 2.1};
    static const double bounded_points[] = {0.2, 0.7, 1.2, 2.0};

    assert_string_antiderivative_matches("{ ln(x^2+1) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ ln(2*x^2+3*x+5) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ ln(a*x^2+b*x+c) | a=2; b=3; c=5 }", points,
                                         sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_ab("{ ln(a*x+b) }", 2.0, 3.0, positive_points,
                                                 sizeof(positive_points) / sizeof(positive_points[0]));
    assert_string_antiderivative_matches_with_ab("{ (a*cos(x)-b*sin(x))/(a*sin(x)+b*cos(x)) }", 2.0, 3.0,
                                                 positive_points, sizeof(positive_points) / sizeof(positive_points[0]));
    assert_string_antiderivative_matches_with_ab("{ 3*(a*cos(x)-b*sin(x))/(a*sin(x)+b*cos(x)) }", 2.0, 3.0,
                                                 positive_points, sizeof(positive_points) / sizeof(positive_points[0]));
    assert_string_antiderivative_matches_with_a("{ ln(a*x)/x }", 2.0, positive_points,
                                                sizeof(positive_points) / sizeof(positive_points[0]));
    assert_string_antiderivative_matches_with_ab("{ x*ln(a*x+b) }", 2.0, 3.0, positive_points,
                                                 sizeof(positive_points) / sizeof(positive_points[0]));
    assert_string_antiderivative_matches_with_abc("{ x*ln(a*x^2+b*x+c) }", 2.0, 3.0, 5.0, points,
                                                  sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_ab("{ x*ln(a^2-b^2*x^2) }", 3.0, 1.0, bounded_points,
                                                 sizeof(bounded_points) / sizeof(bounded_points[0]));
}

static void test_integrate_negative_quadratic_exponential(void)
{
    static const double points[] = {-1.5, -0.4, 0.6, 1.8};
    static const double positive_points[] = {0.2, 0.7, 1.3, 2.1};

    assert_string_antiderivative_matches("{ exp(-x^2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ exp(-2*x^2-5*x-3) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ exp(-3*(2*x+1)^2+5) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_a("{ exp(a*x) }", 2.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_a("{ x*exp(a*x) }", 2.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_a("{ x^2*exp(a*x) }", 2.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_a("{ x^3*exp(a*x) }", 2.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_a("{ sqrt(x)*exp(a*x) }", -2.0, positive_points,
                                                sizeof(positive_points) / sizeof(positive_points[0]));
    assert_string_antiderivative_matches_with_a("{ exp(a*x^2) }", -2.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ 4*exp(x^2+3)*(4*x^4+12*x^2+3) }", points,
                                         sizeof(points) / sizeof(points[0]));
}

static void test_integrate_centered_quadratic_roots(void)
{
    static const double points[] = {-1.5, -0.4, 0.6, 1.8};
    static const double bounded_points[] = {-1.5, -0.4, 0.6, 1.5};
    static const double outer_points[] = {2.3, 2.8, 3.4, 4.2};

    assert_string_antiderivative_matches("{ sqrt(1+x^2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sqrt(1+(x+1)^2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sqrt(4+x^2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ 1/sqrt(4+x^2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ x/sqrt(4+x^2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ x*sqrt(4+x^2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sqrt(4-x^2) }", bounded_points,
                                         sizeof(bounded_points) / sizeof(bounded_points[0]));
    assert_string_antiderivative_matches("{ 1/sqrt(4-x^2) }", bounded_points,
                                         sizeof(bounded_points) / sizeof(bounded_points[0]));
    assert_string_antiderivative_matches("{ x/sqrt(4-x^2) }", bounded_points,
                                         sizeof(bounded_points) / sizeof(bounded_points[0]));
    assert_string_antiderivative_matches("{ x*sqrt(4-x^2) }", bounded_points,
                                         sizeof(bounded_points) / sizeof(bounded_points[0]));
    assert_string_antiderivative_matches("{ sqrt(x^2-4) }", outer_points,
                                         sizeof(outer_points) / sizeof(outer_points[0]));
    assert_string_antiderivative_matches("{ 1/sqrt(x^2-4) }", outer_points,
                                         sizeof(outer_points) / sizeof(outer_points[0]));
    assert_string_antiderivative_matches("{ x/sqrt(x^2-4) }", outer_points,
                                         sizeof(outer_points) / sizeof(outer_points[0]));
    assert_string_antiderivative_matches("{ x*sqrt(x^2-4) }", outer_points,
                                         sizeof(outer_points) / sizeof(outer_points[0]));
    assert_string_antiderivative_matches_with_a("{ sqrt(x^2+a^2) }", 2.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_a("{ 1/sqrt(x^2+a^2) }", 2.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_a("{ x/sqrt(x^2+a^2) }", 2.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_a("{ x*sqrt(x^2+a^2) }", 2.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_a("{ sqrt(a^2-x^2) }", 2.0, bounded_points,
                                                sizeof(bounded_points) / sizeof(bounded_points[0]));
    assert_string_antiderivative_matches_with_a("{ 1/sqrt(a^2-x^2) }", 2.0, bounded_points,
                                                sizeof(bounded_points) / sizeof(bounded_points[0]));
    assert_string_antiderivative_matches_with_a("{ x/sqrt(a^2-x^2) }", 2.0, bounded_points,
                                                sizeof(bounded_points) / sizeof(bounded_points[0]));
    assert_string_antiderivative_matches_with_a("{ x*sqrt(a^2-x^2) }", 2.0, bounded_points,
                                                sizeof(bounded_points) / sizeof(bounded_points[0]));
    assert_string_antiderivative_matches_with_a("{ sqrt(x^2-a^2) }", 2.0, outer_points,
                                                sizeof(outer_points) / sizeof(outer_points[0]));
    assert_string_antiderivative_matches_with_a("{ 1/sqrt(x^2-a^2) }", 2.0, outer_points,
                                                sizeof(outer_points) / sizeof(outer_points[0]));
    assert_string_antiderivative_matches_with_a("{ x/sqrt(x^2-a^2) }", 2.0, outer_points,
                                                sizeof(outer_points) / sizeof(outer_points[0]));
    assert_string_antiderivative_matches_with_a("{ x*sqrt(x^2-a^2) }", 2.0, outer_points,
                                                sizeof(outer_points) / sizeof(outer_points[0]));
}

static void test_integrate_trig_power_products(void)
{
    static const double points[] = {0.25, 0.55, 0.9, 1.2};

    assert_string_antiderivative_matches("{ sin(x)^3 }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ cos(x)^3 }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ tan(x)^3 }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ cot(x)^2 }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ tan(x)^2 - cot(x)^2 }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_ab("{ tan(a*x+b)^2 - cot(a*x+b)^2 }", 0.7, 0.2, points,
                                                 sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_a("{ a*(x^2-1)/x^2 }", 0.7, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_ab("{ a^2*(tan(a*x+b)^2+1)*(tan(a*x+b)^2-1)/tan(a*x+b)^2 }", 0.7, 0.2,
                                                 points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ (tan(x)^2+1)*(4*tan(x)^2*(tan(x)^2*(3*tan(x)^2+2)+1)"
                                         "-6*(tan(x)^2+1)*(tan(x)^4+1))/tan(x)^4 }",
                                         points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ 2*(tan(x)+1/tan(x)^3+1/tan(x)+tan(x)^3) }", points,
                                         sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sec(x)^3 }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sin(x)*sec(x)^3 }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ cos(x)*sec(x)^3 }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ cosec(x)^3 }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sin(x)^2*cos(x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sin(x)*cos(x)^2 }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sec(x)^2*tan(x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ cosec(x)^2*cot(x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_a("{ sec(x)^a*tan(x) }", 4.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_a("{ cosec(x)^a*cot(x) }", 4.0, points,
                                                sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sec(x)*cosec(x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sin(x)^2*cos(x)^2 }", points, sizeof(points) / sizeof(points[0]));
}

static void test_integrate_mixed_frequency_exp_unary(void)
{
    static const double points[] = {-0.6, -0.1, 0.4, 1.1};

    assert_string_antiderivative_matches_with_a("{ x*sin(a*x) }", 2.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_a("{ x^2*sin(a*x) }", 2.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_a("{ x*cos(a*x) }", 2.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_a("{ x^2*cos(a*x) }", 2.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ exp(3*x)*sin(2*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ exp(3*x)*cos(2*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_ab("{ exp(b*x)*sin(a*x) }", 2.0, 3.0, points,
                                                 sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_ab("{ exp(b*x)*cos(a*x) }", 2.0, 3.0, points,
                                                 sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ (sin(x) + cos(x))/exp(x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ (sin(x) + cos(x))/e^x }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sin(2*x)*exp(3*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ cos(2*x)*exp(3*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ x*exp(x)*sin(x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ x*exp(x)*cos(x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ exp(2*x)*sinh(3*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ exp(2*x)*cosh(3*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sinh(3*x)*exp(2*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ cosh(3*x)*exp(2*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sinh(2*x)*cosh(3*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ cosh(3*x)*sinh(2*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ exp(sin(x))*(1 - sin(x) - sin(x)^2) }", points,
                                         sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ exp(sin(2*x))*(1 - sin(2*x) - sin(2*x)^2) }", points,
                                         sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ exp(cos(x))*(1 - cos(x) - cos(x)^2) }", points,
                                         sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_ab("{ exp(a*sin(x)+b*cos(x))*((a*cos(x)-b*sin(x))^2-a*sin(x)-b*cos(x)) }",
                                                 2.0, 3.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_abc("{ exp(a*sin(x)+b*cos(x)+c*tan(x))"
                                                  "*(b*sin(x)-a*cos(x)+2*c*(tan(x)^2+1)*(3*tan(x)^2+1)"
                                                  "+(a*cos(x)-b*sin(x)+c*(tan(x)^2+1))"
                                                  "*(2*c*tan(x)*(tan(x)^2+1)-a*sin(x)-b*cos(x)"
                                                  "+(a*cos(x)-b*sin(x)+c*(tan(x)^2+1))^2)"
                                                  "+2*(a*cos(x)-b*sin(x)+c*(tan(x)^2+1))"
                                                  "*(2*c*tan(x)*(tan(x)^2+1)-a*sin(x)-b*cos(x))) }",
                                                  2.0, 3.0, 0.4, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches_with_abc("{ a*sinh(a*x+b)*exp(cosh(a*x+b)) + c*(a*x+b)/a }", 2.0, 0.3, 0.7,
                                                  points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ exp(sin(x))*(cos(x)*(cos(x)^2 - sin(x)) - sin(2*x) - cos(x)) }", points,
                                         sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches(
        "{ exp(sin(x))*(sin(x) - 2*cos(2*x) + cos(x)*(cos(x)*(cos(x)^2 - sin(x)) - sin(2*x) - cos(x)) + "
        "cos(x)*(-sin(2*x) - cos(x)) - sin(x)*(cos(x)^2 - sin(x))) }",
        points, sizeof(points) / sizeof(points[0]));
}

static void test_integrate_iterated_exp_unary_derivatives(void)
{
    static const double points[] = {0.45, 0.75, 1.05, 1.25};
    static const char *const exp_sin_derivatives[4] = {"cos(x)·exp(sin(x))", "exp(sin(x))·(cos²(x) - sin(x))",
                                                       "exp(sin(x))·(cos(x)·(cos²(x) - sin(x)) - sin(2x) - cos(x))",
                                                       "exp(sin(x))·(sin(x) - 2·cos(2x) + "
                                                       "cos(x)·(cos(x)·(cos²(x) - sin(x)) - sin(2x) - cos(x)) + "
                                                       "cos(x)·(-sin(2x) - cos(x)) - sin(x)·(cos²(x) - sin(x)))"};
    static const char *const exp_cos_derivatives[4] = {"-sin(x)·exp(cos(x))", "exp(cos(x))·(sin²(x) - cos(x))",
                                                       "exp(cos(x))·(sin(2x) + sin(x) - sin(x)·(sin²(x) - cos(x)))",
                                                       "exp(cos(x))·(2·cos(2x) + cos(x) - "
                                                       "sin(x)·(sin(2x) + sin(x) - sin(x)·(sin²(x) - cos(x))) - "
                                                       "cos(x)·(sin²(x) - cos(x)) - sin(x)·(sin(2x) + sin(x)))"};
    static const char *const exp_tan_derivatives[4] = {"exp(tan(x))·(tan²(x) + 1)",
                                                       "exp(tan(x))·(tan²(x) + 1)·(tan(x)·(tan(x) + 2) + 1)",
                                                       "exp(tan(x))·(tan²(x) + 1)·"
                                                       "(2·(tan²(x) + 1)·(tan(x) + 1) + (tan(x)·(tan(x) + 2) + 1)²)",
                                                       "exp(tan(x))·(tan²(x) + 1)·"
                                                       "((tan(x)·(tan(x) + 2) + 1)·"
                                                       "(2·(tan²(x) + 1)·(tan(x) + 1) + "
                                                       "(tan(x)·(tan(x) + 2) + 1)²) + "
                                                       "2·(tan²(x) + 1)·(tan(x)·(3·tan(x) + 2) + "
                                                       "2·(tan(x) + 1)·(tan(x)·(tan(x) + 2) + 1) + 1))"};
    static const char *const exp_cot_derivatives[4] = {"-cosec²(x)·exp(cot(x))",
                                                       "cosec²(x)·exp(cot(x))·(2·cot(x) + cosec²(x))",
                                                       "cosec²(x)·exp(cot(x))·"
                                                       "((-2·cot(x) - cosec²(x))·(2·cot(x) + cosec²(x)) - "
                                                       "2·cosec²(x)·(cot(x) + 1))",
                                                       "cosec²(x)·exp(cot(x))·"
                                                       "((-2·cot(x) - cosec²(x))·"
                                                       "((-2·cot(x) - cosec²(x))·(2·cot(x) + cosec²(x)) - "
                                                       "2·cosec²(x)·(cot(x) + 1)) + "
                                                       "2·cosec²(x)·(2·(cot(x) + 1)·(3·cot(x) + cosec²(x)) + "
                                                       "cosec²(x)))"};
    static const char *const exp_cosec_derivatives[4] = {"-cosec(x)·cot(x)·exp(cosec(x))",
                                                         "cosec(x)·exp(cosec(x))·(cot²(x)·(cosec(x) + 1) + cosec²(x))",
                                                         "cosec(x)·cot(x)·exp(cosec(x))·"
                                                         "(cosec(x)·(-2·cosec(x)·(cosec(x) + 2) - cot²(x)) - "
                                                         "(cosec(x) + 1)·(cot²(x)·(cosec(x) + 1) + cosec²(x)))",
                                                         "cosec(x)·exp(cosec(x))·"
                                                         "((-cot²(x)·(cosec(x) + 1) - cosec²(x))·"
                                                         "(cosec(x)·(-2·cosec(x)·(cosec(x) + 2) - cot²(x)) - "
                                                         "(cosec(x) + 1)·(cot²(x)·(cosec(x) + 1) + cosec²(x))) + "
                                                         "cosec(x)·cot²(x)·(2·(cosec(x) + 1)·"
                                                         "(cosec(x)·(cosec(x) + 6) + cot²(x)) + cot²(x) + cosec²(x)))"};
    static const char *const exp_sec_derivatives[4] = {"sec(x)·tan(x)·exp(sec(x))",
                                                       "sec(x)·exp(sec(x))·(tan²(x)·(sec(x) + 2) + 1)",
                                                       "sec(x)·tan(x)·exp(sec(x))·"
                                                       "((sec(x) + 1)·(tan²(x)·(sec(x) + 2) + 1) + "
                                                       "2·(tan²(x) + 1)·(sec(x) + 2) + tan²(x)·sec(x))",
                                                       "sec(x)·exp(sec(x))·"
                                                       "((tan²(x)·(sec(x) + 2) + 1)·"
                                                       "((sec(x) + 1)·(tan²(x)·(sec(x) + 2) + 1) + "
                                                       "2·(tan²(x) + 1)·(sec(x) + 2) + tan²(x)·sec(x)) + "
                                                       "tan²(x)·(sec(x)·(tan²(x)·(sec(x) + 2) + 1) + "
                                                       "(sec(x) + 1)·(2·(tan²(x) + 1)·(sec(x) + 2) + tan²(x)·sec(x)) + "
                                                       "2·(tan²(x) + 1)·(3·sec(x) + 4) + sec(x)·(3·tan²(x) + 2)))"};
    static const char *const exp_sinh_derivatives[4] = {
        "cosh(x)·exp(sinh(x))", "exp(sinh(x))·(sinh(x) + cosh²(x))",
        "exp(sinh(x))·(cosh(x) + sinh(2x) + cosh(x)·(sinh(x) + cosh²(x)))",
        "exp(sinh(x))·(sinh(x) + 2·cosh(2x) + "
        "cosh(x)·(cosh(x) + sinh(2x) + cosh(x)·(sinh(x) + cosh²(x))) + "
        "sinh(x)·(sinh(x) + cosh²(x)) + cosh(x)·(cosh(x) + sinh(2x)))"};
    static const char *const exp_cosh_derivatives[4] = {
        "sinh(x)·exp(cosh(x))", "exp(cosh(x))·(cosh(x) + sinh²(x))",
        "exp(cosh(x))·(sinh(x) + sinh(2x) + sinh(x)·(cosh(x) + sinh²(x)))",
        "exp(cosh(x))·(cosh(x) + 2·cosh(2x) + "
        "sinh(x)·(sinh(x) + sinh(2x) + sinh(x)·(cosh(x) + sinh²(x))) + "
        "cosh(x)·(cosh(x) + sinh²(x)) + sinh(x)·(sinh(x) + sinh(2x)))"};
    static const char *const exp_tanh_hyperbolic_derivatives[4] = {
        "(1 - tanh²(x))·exp(tanh(x))", "(1 - tanh²(x))·exp(tanh(x))·(1 - 2·tanh(x) - tanh²(x))",
        "(1 - tanh²(x))·exp(tanh(x))·"
        "(2·tanh(x)·(tanh(x)·(tanh(x) + 1) - 1) + "
        "(1 - 2·tanh(x) - tanh²(x))² - 2)",
        "(1 - tanh²(x))·exp(tanh(x))·"
        "((1 - 2·tanh(x) - tanh²(x))·"
        "(2·tanh(x)·(tanh(x)·(tanh(x) + 1) - 1) + "
        "(1 - 2·tanh(x) - tanh²(x))² - 2) + "
        "2·(1 - tanh²(x))·(tanh(x)·(3·tanh(x) + 2) - 1) + "
        "4·(1 - 2·tanh(x) - tanh²(x))·"
        "(tanh(x)·(tanh(x)·(tanh(x) + 1) - 1) - 1))"};
    static const char *const exp_sech_derivatives[4] = {"-sech(x)·tanh(x)·exp(sech(x))",
                                                        "sech(x)·exp(sech(x))·(tanh²(x)·(sech(x) + 2) - 1)",
                                                        "sech(x)·tanh(x)·exp(sech(x))·"
                                                        "(2·(1 - tanh²(x))·(sech(x) + 2) - "
                                                        "(sech(x) + 1)·(tanh²(x)·(sech(x) + 2) - 1) - "
                                                        "tanh²(x)·sech(x))",
                                                        "sech(x)·exp(sech(x))·"
                                                        "((1 - tanh²(x)·(sech(x) + 1) - tanh²(x))·"
                                                        "(2·(1 - tanh²(x))·(sech(x) + 2) - "
                                                        "(sech(x) + 1)·(tanh²(x)·(sech(x) + 2) - 1) - "
                                                        "tanh²(x)·sech(x)) + "
                                                        "tanh²(x)·(2·(1 - tanh²(x))·(-3·sech(x) - 4) - "
                                                        "(sech(x) + 1)·(2·(1 - tanh²(x))·(sech(x) + 2) - "
                                                        "tanh²(x)·sech(x)) + sech(x)·(tanh²(x)·(sech(x) + 2) - 1) - "
                                                        "sech(x)·(2 - 3·tanh²(x))))"};
    static const char *const exp_cosech_hyperbolic_derivatives[4] = {
        "-cosech(x)·coth(x)·exp(cosech(x))", "cosech(x)·exp(cosech(x))·(coth²(x)·(cosech(x) + 1) + cosech²(x))",
        "cosech(x)·coth(x)·exp(cosech(x))·"
        "(cosech(x)·(-2·cosech(x)·(cosech(x) + 2) - coth²(x)) - "
        "(cosech(x) + 1)·(coth²(x)·(cosech(x) + 1) + cosech²(x)))",
        "cosech(x)·exp(cosech(x))·"
        "((-coth²(x)·(cosech(x) + 1) - cosech²(x))·"
        "(cosech(x)·(-2·cosech(x)·(cosech(x) + 2) - coth²(x)) - "
        "(cosech(x) + 1)·(coth²(x)·(cosech(x) + 1) + cosech²(x))) + "
        "cosech(x)·coth²(x)·(2·(cosech(x) + 1)·"
        "(cosech(x)·(cosech(x) + 6) + coth²(x)) + coth²(x) + cosech²(x)))"};
    static const char *const exp_coth_hyperbolic_derivatives[4] = {
        "-cosech²(x)·exp(coth(x))", "cosech²(x)·exp(coth(x))·(2·coth(x) + cosech²(x))",
        "cosech²(x)·exp(coth(x))·"
        "((-2·coth(x) - cosech²(x))·(2·coth(x) + cosech²(x)) - "
        "2·cosech²(x)·(coth(x) + 1))",
        "cosech²(x)·exp(coth(x))·"
        "((-2·coth(x) - cosech²(x))·"
        "((-2·coth(x) - cosech²(x))·(2·coth(x) + cosech²(x)) - "
        "2·cosech²(x)·(coth(x) + 1)) + "
        "2·cosech²(x)·(2·(coth(x) + 1)·(3·coth(x) + cosech²(x)) + "
        "cosech²(x)))"};

    assert_iterated_derivatives_integrate_back("{ exp(sin(x)) }", exp_sin_derivatives, points,
                                               sizeof(points) / sizeof(points[0]));
    assert_iterated_derivatives_integrate_back("{ exp(cos(x)) }", exp_cos_derivatives, points,
                                               sizeof(points) / sizeof(points[0]));
    assert_iterated_derivatives_integrate_back("{ exp(tan(x)) }", exp_tan_derivatives, points,
                                               sizeof(points) / sizeof(points[0]));
    assert_iterated_derivatives_integrate_back("{ exp(cot(x)) }", exp_cot_derivatives, points,
                                               sizeof(points) / sizeof(points[0]));
    assert_iterated_derivatives_integrate_back("{ exp(cosec(x)) }", exp_cosec_derivatives, points,
                                               sizeof(points) / sizeof(points[0]));
    assert_iterated_derivatives_integrate_back("{ exp(sec(x)) }", exp_sec_derivatives, points,
                                               sizeof(points) / sizeof(points[0]));
    assert_iterated_derivatives_integrate_back("{ exp(sinh(x)) }", exp_sinh_derivatives, points,
                                               sizeof(points) / sizeof(points[0]));
    assert_iterated_derivatives_integrate_back("{ exp(cosh(x)) }", exp_cosh_derivatives, points,
                                               sizeof(points) / sizeof(points[0]));
    assert_iterated_derivatives_integrate_back("{ exp(tanh(x)) }", exp_tanh_hyperbolic_derivatives, points,
                                               sizeof(points) / sizeof(points[0]));
    assert_iterated_derivatives_integrate_back("{ exp(sech(x)) }", exp_sech_derivatives, points,
                                               sizeof(points) / sizeof(points[0]));
    assert_iterated_derivatives_integrate_back("{ exp(cosech(x)) }", exp_cosech_hyperbolic_derivatives, points,
                                               sizeof(points) / sizeof(points[0]));
    assert_iterated_derivatives_integrate_back("{ exp(coth(x)) }", exp_coth_hyperbolic_derivatives, points,
                                               sizeof(points) / sizeof(points[0]));
    assert_nth_derivative_integrates_back("{ exp(cos(x)) }", 5u, points, sizeof(points) / sizeof(points[0]));
    assert_nth_derivative_integrates_back("{ exp(cos(x)) }", 6u, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ -e^cos(x)*(-sin^2(x)*(sin^4(x) - 20*sin^2(x) + 16) + "
                                          "15*cos^3(x) + (15 - 45*sin^2(x))*cos^2(x) + "
                                          "(15*sin^4(x) - 75*sin^2(x) + 1)*cos(x)) }",
                                          "sin(x)·exp(cos(x))·(cos(x)·(cos(x)·(cos(x)·(-cos(x) - 10) - 23) - 5) + 8)");
    assert_string_antiderivative_matches("{ -e^cos(x)*(-sin^2(x)*(sin^4(x) - 20*sin^2(x) + 16) + "
                                         "15*cos^3(x) + (15 - 45*sin^2(x))*cos^2(x) + "
                                         "(15*sin^4(x) - 75*sin^2(x) + 1)*cos(x)) }",
                                         points, sizeof(points) / sizeof(points[0]));
}

static void test_integrate_hyperbolic_table_tail(void)
{
    static const double points[] = {-0.6, -0.1, 0.4, 1.1};

    assert_string_antiderivative_contains("{ cosh(x) }", "sinh(x)");
    assert_string_antiderivative_matches("{ cosh(x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ exp(a*x)*cosh(b*x) }", "exp(ax)·(a·cosh(bx) - b·sinh(bx))/(a² - b²)");
    assert_string_antiderivative_matches_with_ab("{ exp(a*x)*cosh(b*x) }", 3.0, 2.0, points,
                                                 sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ sinh(x) }", "cosh(x)");
    assert_string_antiderivative_matches("{ sinh(x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ exp(a*x)*sinh(b*x) }", "exp(ax)·(a·sinh(bx) - b·cosh(bx))/(a² - b²)");
    assert_string_antiderivative_matches_with_ab("{ exp(a*x)*sinh(b*x) }", 3.0, 2.0, points,
                                                 sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ exp(x)*tanh(x) }", "exp(x) - 2·atan(exp(x))");
    assert_string_antiderivative_matches("{ exp(x)*tanh(x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ tanh(a*x) }", "ln(cosh(ax))/a");
    assert_string_antiderivative_matches_with_a("{ tanh(a*x) }", 2.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ cos(a*x)*cosh(b*x) }",
                                          "(a·sin(ax)·cosh(bx) + b·cos(ax)·sinh(bx))/(a² + b²)");
    assert_string_antiderivative_matches_with_ab("{ cos(a*x)*cosh(b*x) }", 2.0, 3.0, points,
                                                 sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ cos(a*x)*sinh(b*x) }",
                                          "(a·sin(ax)·sinh(bx) + b·cos(ax)·cosh(bx))/(a² + b²)");
    assert_string_antiderivative_matches_with_ab("{ cos(a*x)*sinh(b*x) }", 2.0, 3.0, points,
                                                 sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ sin(a*x)*cosh(b*x) }",
                                          "(b·sin(ax)·sinh(bx) - a·cos(ax)·cosh(bx))/(a² + b²)");
    assert_string_antiderivative_matches_with_ab("{ sin(a*x)*cosh(b*x) }", 2.0, 3.0, points,
                                                 sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ sin(a*x)*sinh(b*x) }",
                                          "(b·sin(ax)·cosh(bx) - a·cos(ax)·sinh(bx))/(a² + b²)");
    assert_string_antiderivative_matches_with_ab("{ sin(a*x)*sinh(b*x) }", 2.0, 3.0, points,
                                                 sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ sinh(a*x)^2 }", "(sinh(2ax) - 2ax)/(4a)");
    assert_string_antiderivative_matches_with_a("{ sinh(a*x)^2 }", 2.0, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ sinh(a*x)*sinh(b*x) }",
                                          "(b·cosh(bx)·sinh(ax) - a·cosh(ax)·sinh(bx))/(b² - a²)");
    assert_string_antiderivative_matches_with_ab("{ sinh(a*x)*sinh(b*x) }", 2.0, 3.0, points,
                                                 sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ sinh(a*x)*cosh(b*x) }",
                                          "(a·cosh(ax)·cosh(bx) - b·sinh(ax)·sinh(bx))/(a² - b²)");
    assert_string_antiderivative_matches_with_ab("{ sinh(a*x)*cosh(b*x) }", 2.0, 3.0, points,
                                                 sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ sinh(a*x)*cosh(a*x) }", "cosh(2ax)/(4a)");
    assert_string_antiderivative_matches_with_a("{ sinh(a*x)*cosh(a*x) }", 2.0, points,
                                                sizeof(points) / sizeof(points[0]));
}

static void test_integrate_frequency_product_families(void)
{
    static const double points[] = {-0.6, -0.1, 0.4, 1.1};

    assert_string_antiderivative_matches("{ sin(2*x)*sin(3*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ cos(2*x)*cos(3*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sin(2*x)*cos(3*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sin(2*x+1)*sin(3*x-2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ cos(2*x+1)*cos(3*x-2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sin(2*x+1)*cos(3*x-2) }", points, sizeof(points) / sizeof(points[0]));

    assert_string_antiderivative_matches("{ sinh(2*x)*sinh(3*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ cosh(2*x)*cosh(3*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sinh(2*x)*cosh(3*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sinh(2*x+1)*sinh(3*x-2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ cosh(2*x+1)*cosh(3*x-2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sinh(2*x+1)*cosh(3*x-2) }", points, sizeof(points) / sizeof(points[0]));

    assert_string_antiderivative_matches("{ cos(2*x)*cosh(3*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ cos(2*x)*sinh(3*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sin(2*x)*cosh(3*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sin(2*x)*sinh(3*x) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ cos(2*x+1)*cosh(3*x-2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ cos(2*x+1)*sinh(3*x-2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sin(2*x+1)*cosh(3*x-2) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sin(2*x+1)*sinh(3*x-2) }", points, sizeof(points) / sizeof(points[0]));
}

static void test_integrate_more_by_parts(void)
{
    static const double points[] = {0.25, 0.8, 1.5, 3.0};
    static const double outer_points[] = {1.3, 1.8, 2.4, 3.5};
    static const double rational_atan_points[] = {-2.0, -0.5, 0.25, 2.0};
    expr_t *x = test_expr_new_named_var_d(0.0, "x");
    expr_t *x_sq = test_expr_pow_d(x, 2.0);
    expr_t *atan_x = expr_atan(x);
    expr_t *acot_x = expr_acot(x);
    expr_t *x_sq_atan_x = (x_sq && atan_x) ? expr_mul(x_sq, atan_x) : NULL;
    expr_t *x_sq_acot_x = (x_sq && acot_x) ? expr_mul(x_sq, acot_x) : NULL;

    assert_antiderivative_matches("integral derivative of x^2*atan(x)", x_sq_atan_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of x^2*acot(x)", x_sq_acot_x, x, outer_points,
                                  sizeof(outer_points) / sizeof(outer_points[0]));
    assert_string_antiderivative_matches("{ atan(x/(1-x^2)) }", rational_atan_points,
                                         sizeof(rational_atan_points) / sizeof(rational_atan_points[0]));
    assert_string_antiderivative_contains("{ atan(x/(1-x^2)) + C }", "ln(x⁴ - x² + 1)");
    assert_string_antiderivative_not_contains("{ atan(x/(1-x^2)) + C }", "∫");
    assert_string_antiderivative_matches("{ 1/4*(-ln(x^4-x^2+1)+4*x*atan(x/(1-x^2))"
                                         "-2*sqrt(3)*atan((2*x^2-1)/sqrt(3))) }",
                                         rational_atan_points,
                                         sizeof(rational_atan_points) / sizeof(rational_atan_points[0]));
    assert_string_antiderivative_not_contains("{ 1/4*(-ln(x^4-x^2+1)+4*x*atan(x/(1-x^2))"
                                              "-2*sqrt(3)*atan((2*x^2-1)/sqrt(3))) }",
                                              "∫");
    assert_string_antiderivative_not_contains("{ 1/4*(-ln(x^4-x^2+1)+4*x*atan(x/(1-x^2))"
                                              "-2*sqrt(3)*atan((2*x^2-1)/sqrt(3))) }",
                                              "atanh");
    assert_string_antiderivative_matches("{ -1/4*(ln(x^4-x^2+1)-4*x*atan(x/(1-x^2))"
                                         "+2*sqrt(3)*atan((2*x^2-1)/sqrt(3))) }",
                                         rational_atan_points,
                                         sizeof(rational_atan_points) / sizeof(rational_atan_points[0]));
    assert_string_antiderivative_not_contains("{ -1/4*(ln(x^4-x^2+1)-4*x*atan(x/(1-x^2))"
                                              "+2*sqrt(3)*atan((2*x^2-1)/sqrt(3))) }",
                                              "∫");
    assert_string_antiderivative_matches("{ ln(x^4+x^2+1) }", rational_atan_points,
                                         sizeof(rational_atan_points) / sizeof(rational_atan_points[0]));
    assert_string_antiderivative_not_contains("{ ln(x^4+x^2+1) }", "∫");
    assert_string_antiderivative_matches("{ (2*x+1)*ln(x^2+1) }", rational_atan_points,
                                         sizeof(rational_atan_points) / sizeof(rational_atan_points[0]));
    assert_string_antiderivative_not_contains("{ (2*x+1)*ln(x^2+1) }", "∫");

    expr_free(x_sq_acot_x);
    expr_free(x_sq_atan_x);
    expr_free(acot_x);
    expr_free(atan_x);
    expr_free(x_sq);
    expr_free(x);
}

static void test_integrate_partial_fractions(void)
{
    static const double points[] = {-2.5, -0.5, 0.5, 1.5};
    static const double positive_points[] = {0.25, 0.75, 1.5, 2.5};
    static const double quartic_points[] = {-2.0, -0.5, 0.0, 0.75, 2.0};
    number_t three_num = num_create_from_long(3);
    expr_t *x = test_expr_new_named_var_d(0.0, "x");
    expr_t *x_plus_one_a = expr_add_num(x, &NUM_ONE);
    expr_t *x_plus_two_a = expr_add_num(x, &NUM_TWO);
    expr_t *denom_distinct_a = (x_plus_one_a && x_plus_two_a) ? expr_mul(x_plus_one_a, x_plus_two_a) : NULL;
    expr_t *recip_distinct = denom_distinct_a ? expr_num_div(&NUM_ONE, denom_distinct_a) : NULL;

    expr_t *two_x = expr_mul_num(x, &NUM_TWO);
    expr_t *three = expr_new_const(three_num);
    expr_t *linear_numer = (two_x && three) ? expr_add(two_x, three) : NULL;
    expr_t *x_plus_one_b = expr_add_num(x, &NUM_ONE);
    expr_t *x_plus_two_b = expr_add_num(x, &NUM_TWO);
    expr_t *denom_distinct_b = (x_plus_one_b && x_plus_two_b) ? expr_mul(x_plus_one_b, x_plus_two_b) : NULL;
    expr_t *linear_over_distinct = (linear_numer && denom_distinct_b) ? expr_div(linear_numer, denom_distinct_b) : NULL;

    expr_t *x_sq_den = test_expr_pow_d(x, 2.0);
    expr_t *x_sq_minus_one = x_sq_den ? expr_sub_num(x_sq_den, &NUM_ONE) : NULL;
    expr_t *quadratic_recip = x_sq_minus_one ? expr_num_div(&NUM_ONE, x_sq_minus_one) : NULL;

    assert_antiderivative_matches("integral derivative of 1/((x + 1)(x + 2))", recip_distinct, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of (2x + 3)/((x + 1)(x + 2))", linear_over_distinct, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of 1/(x^2 - 1)", quadratic_recip, x, positive_points,
                                  sizeof(positive_points) / sizeof(positive_points[0]));
    assert_string_antiderivative_matches("{ 1/((x+1)(x+2)(x+3)(x+4)) }", positive_points,
                                         sizeof(positive_points) / sizeof(positive_points[0]));
    assert_string_antiderivative_contains("{ 1/((x+1)(x+2)(x+3)(x+4)) }", "⅙·(");
    assert_string_antiderivative_not_contains("{ 1/((x+1)(x+2)(x+3)(x+4)) }", "0.166666");
    assert_string_antiderivative_matches("{ (x^2+1)/(x^4-x^2+1) }", quartic_points,
                                         sizeof(quartic_points) / sizeof(quartic_points[0]));
    assert_string_antiderivative_contains("{ (x^2+1)/(x^4-x^2+1) }", "atan(x/(1 - x²))");
    assert_string_antiderivative_matches("{ (2*x^3+3*x^2+5*x+7)/(x^4+x^2+1) }", quartic_points,
                                         sizeof(quartic_points) / sizeof(quartic_points[0]));
    assert_string_antiderivative_not_contains("{ (2*x^3+3*x^2+5*x+7)/(x^4+x^2+1) }", "∫");

    expr_free(quadratic_recip);
    expr_free(x_sq_minus_one);
    expr_free(x_sq_den);
    expr_free(linear_over_distinct);
    expr_free(denom_distinct_b);
    expr_free(x_plus_two_b);
    expr_free(x_plus_one_b);
    expr_free(linear_numer);
    expr_free(recip_distinct);
    expr_free(denom_distinct_a);
    expr_free(x_plus_two_a);
    expr_free(x_plus_one_a);
    expr_free(three);
    expr_free(two_x);
    expr_free(x);
    num_destroy(&three_num);
}

static void test_integrate_quotient_rule_derivative(void)
{
    static const double points[] = {-0.1, 0.25, 1.0, 2.0};
    const char *quartic_power_input = "{ 24*x*(16*x^6-x^10-6*x^8+2*x^4-9*x^2+1)"
                                      "/(x^4-x^2+1)^4 }";

    assert_nth_derivative_integrates_back("{ (3*x^2 + 4*x + 5)/(x^3 + 2*x^2 + 5*x + 1) }", 1u, points,
                                          sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ (3*x - (x + 1)*(2*x + 3) + x^2 + 5)/"
                                         "(x^2 + 3*x + 5)^2 }",
                                         points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ (2*(-x - 1)*(x^2 + 3*x + 5) - "
                                         "2*(2*x + 3)*(3*x - (x + 1)*(2*x + 3) + x^2 + 5))/"
                                         "(x^2 + 3*x + 5)^3 }",
                                         points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ 120*(30*x^4 + 84*x - x^6 - 6*x^5 + 220*x^3 + "
                                         "345*x^2 - 73)/(x^2 + 3*x + 5)^6 }",
                                         points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ (40320*(x^2 + 3*x + 5)*(42*x^5 - 511*x - x^7 - "
                                         "7*x^6 + 385*x^4 + 805*x^3 + 294*x^2 - 289) - "
                                         "40320*(2*x + 3)*(56*x^6 - 2312*x - x^8 - 8*x^7 + "
                                         "616*x^5 + 1610*x^4 + 784*x^3 - 2044*x^2 - 502))/"
                                         "(x^2 + 3*x + 5)^9 }",
                                         points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ ((x^2 + 3*x + 5)*(10*x^9) - "
                                         "10*(2*x + 3)*x^10)/(x^2 + 3*x + 5)^11 }",
                                         points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ 1/(x^2 + 3*x + 5)^2 }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ (-4*x^3 - 14*x^2 - 22*x - 15)/(x^2 + 3*x + 5)^2 }", points,
                                         sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ (-4*x^3 - 14*x^2 - 22*x - 15)/(x^2 + 3*x + 5)^2 }",
                                          "(-2x - 5)/(x² + 3x + 5)");
    assert_string_antiderivative_matches(quartic_power_input, points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains(quartic_power_input, "(6x⁸ + 22x⁶ - 42x⁴ + 4)/(x⁴ - x² + 1)³");
    assert_string_antiderivative_matches("{ (-4*x^4-6*x^2+2*x-2)/(x^3+x+1)^3 }", points,
                                         sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ (-4*x^4-6*x^2+2*x-2)/(x^3+x+1)^3 }", "(x² + 1)/(x³ + x + 1)²");
}

static void test_integrate_unevaluated_integral_derivative(void)
{
    static const double points[] = {-1.0, -0.25, 0.5, 1.25};
    expr_t *x = test_expr_new_named_var_d(0.0, "x");
    expr_t *x_sq = test_expr_pow_d(x, 2.0);
    expr_t *integral = expr_integral(x_sq, x);
    expr_t *deriv = integral ? expr_create_deriv(integral, x) : NULL;
    char *text = integral ? expr_to_string(integral, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(integral);
    ASSERT_NOT_NULL(deriv);
    ASSERT_NOT_NULL(text);
    print_antiderivative_text("unevaluated integral", text);
    ASSERT_TRUE(strstr(text, "∫^x t²·dt") != NULL);

    for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
        char label[160];

        test_expr_set_val_d(x, points[i]);
        snprintf(label, sizeof(label), "d/dx integral to x at x=%g", points[i]);
        check_q_at(__FILE__, __LINE__, 1, label, expr_eval_qf(deriv), expr_eval_qf(x_sq));
    }

    free(text);
    expr_free(deriv);
    expr_free(integral);
    expr_free(x_sq);
    expr_free(x);
}

static void test_integrate_unevaluated_integral_leibniz_derivative(void)
{
    expr_bindings_t *bindings = NULL;
    expr_bindings_t *dx_bindings = NULL;
    expr_bindings_t *dy_bindings = NULL;
    expr_bindings_t *du_bindings = NULL;
    expr_t *integral = expr_from_string("{ ∫_x^y z*u dz }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *y = bindings ? expr_bindings_get(bindings, "y") : NULL;
    expr_t *u = bindings ? expr_bindings_get(bindings, "u") : NULL;
    expr_t *dx = (integral && x) ? expr_create_deriv(integral, x) : NULL;
    expr_t *dy = (integral && y) ? expr_create_deriv(integral, y) : NULL;
    expr_t *du = (integral && u) ? expr_create_deriv(integral, u) : NULL;
    expr_t *dx_eval = NULL;
    expr_t *dy_eval = NULL;
    expr_t *du_eval = NULL;
    expr_t *dx_x = NULL;
    expr_t *dx_u = NULL;
    expr_t *dy_y = NULL;
    expr_t *dy_u = NULL;
    expr_t *du_x = NULL;
    expr_t *du_y = NULL;
    char *du_text = du ? expr_to_string(du, style_UNBOUND) : NULL;
    char *dx_text = dx ? expr_to_string(dx, style_UNBOUND) : NULL;
    char *dy_text = dy ? expr_to_string(dy, style_UNBOUND) : NULL;
    size_t dx_input_len = dx_text ? strlen(dx_text) + 5u : 0u;
    size_t dy_input_len = dy_text ? strlen(dy_text) + 5u : 0u;
    size_t du_input_len = du_text ? strlen(du_text) + 5u : 0u;
    char *dx_input = dx_text ? malloc(dx_input_len) : NULL;
    char *dy_input = dy_text ? malloc(dy_input_len) : NULL;
    char *du_input = du_text ? malloc(du_input_len) : NULL;

    ASSERT_NOT_NULL(integral);
    ASSERT_NOT_NULL(x);
    ASSERT_NOT_NULL(y);
    ASSERT_NOT_NULL(u);
    ASSERT_NOT_NULL(dx);
    ASSERT_NOT_NULL(dy);
    ASSERT_NOT_NULL(du);
    ASSERT_NOT_NULL(dx_text);
    ASSERT_NOT_NULL(dy_text);
    ASSERT_NOT_NULL(du_text);
    ASSERT_NOT_NULL(dx_input);
    ASSERT_NOT_NULL(dy_input);
    ASSERT_NOT_NULL(du_input);
    ASSERT_TRUE(strstr(du_text, "∫^y_x z·dz") != NULL);

    snprintf(dx_input, dx_input_len, "{ %s }", dx_text);
    snprintf(dy_input, dy_input_len, "{ %s }", dy_text);
    snprintf(du_input, du_input_len, "{ %s }", du_text);

    dx_eval = expr_from_string(dx_input, &dx_bindings);
    dy_eval = expr_from_string(dy_input, &dy_bindings);
    du_eval = expr_from_string(du_input, &du_bindings);
    dx_x = dx_bindings ? expr_bindings_get(dx_bindings, "x") : NULL;
    dx_u = dx_bindings ? expr_bindings_get(dx_bindings, "u") : NULL;
    dy_y = dy_bindings ? expr_bindings_get(dy_bindings, "y") : NULL;
    dy_u = dy_bindings ? expr_bindings_get(dy_bindings, "u") : NULL;
    du_x = du_bindings ? expr_bindings_get(du_bindings, "x") : NULL;
    du_y = du_bindings ? expr_bindings_get(du_bindings, "y") : NULL;

    ASSERT_NOT_NULL(dx_eval);
    ASSERT_NOT_NULL(dy_eval);
    ASSERT_NOT_NULL(du_eval);
    ASSERT_NOT_NULL(dx_x);
    ASSERT_NOT_NULL(dx_u);
    ASSERT_NOT_NULL(dy_y);
    ASSERT_NOT_NULL(dy_u);
    ASSERT_NOT_NULL(du_x);
    ASSERT_NOT_NULL(du_y);

    test_expr_set_val_d(dx_x, 1.0);
    test_expr_set_val_d(dx_u, 4.0);
    test_expr_set_val_d(dy_y, 3.0);
    test_expr_set_val_d(dy_u, 4.0);
    test_expr_set_val_d(du_x, 1.0);
    test_expr_set_val_d(du_y, 3.0);

    check_q_at(__FILE__, __LINE__, 1, "d/dx ∫_x^y z*u dz = -u*x", expr_eval_qf(dx_eval), qf_from_double(-4.0));
    check_q_at(__FILE__, __LINE__, 1, "d/dy ∫_x^y z*u dz = u*y", expr_eval_qf(dy_eval), qf_from_double(12.0));
    check_q_at(__FILE__, __LINE__, 1, "d/du ∫_x^y z*u dz = ∫_x^y z dz", expr_eval_qf(du_eval), qf_from_double(4.0));

    free(du_input);
    free(dy_input);
    free(dx_input);
    free(dy_text);
    free(dx_text);
    free(du_text);
    expr_free(du_eval);
    expr_bindings_free(du_bindings);
    expr_free(dy_eval);
    expr_bindings_free(dy_bindings);
    expr_free(dx_eval);
    expr_bindings_free(dx_bindings);
    expr_free(du);
    expr_free(dy);
    expr_free(dx);
    expr_free(integral);
    expr_bindings_free(bindings);
}

static void test_integrate_unevaluated_integral_evaluation(void)
{
    static const double points[] = {-1.0, -0.25, 0.5, 1.25};
    expr_t *x = test_expr_new_named_var_d(0.0, "x");
    expr_t *x_sq = test_expr_pow_d(x, 2.0);
    expr_t *integral = expr_integral(x_sq, x);

    ASSERT_NOT_NULL(integral);

    for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
        char label[160];

        test_expr_set_val_d(x, points[i]);
        snprintf(label, sizeof(label), "integral to x at x=%g", points[i]);
        check_q_at(__FILE__, __LINE__, 1, label, expr_eval_qf(integral),
                   qf_from_double((points[i] * points[i] * points[i]) / 3.0));
    }

    expr_free(integral);
    expr_free(x_sq);
    expr_free(x);
}

static void test_integrate_unevaluated_integral_constant_upper(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ ∫^3 sin(t) dt }", &bindings);
    expr_t *expected = expr_from_string("{ 1 - cos(3) }", NULL);
    expr_t *z = bindings ? expr_bindings_get(bindings, "z") : NULL;
    expr_t *t = bindings ? expr_bindings_get(bindings, "t") : NULL;
    char *text = expr ? expr_to_string(expr, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(expected);
    ASSERT_TRUE(z == NULL);
    ASSERT_TRUE(t == NULL);
    ASSERT_NOT_NULL(text);
    ASSERT_TRUE(strstr(text, "∫^3 sin(t)·dt") != NULL);
    check_q_at(__FILE__, __LINE__, 1, "integral to 3 of sin(t)", expr_eval_qf(expr), expr_eval_qf(expected));

    free(text);
    expr_free(expected);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_integrate_unevaluated_integral_chain_rule_upper(void)
{
    static const double points[] = {-1.0, -0.25, 0.5, 1.25};
    expr_bindings_t *bindings = NULL;
    expr_bindings_t *expected_deriv_bindings = NULL;
    expr_bindings_t *expected_value_bindings = NULL;
    expr_t *expr = expr_from_string("{ ∫^(x+1) sin(t) dt | x = NAN }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    expr_t *expected_deriv = expr_from_string("{ sin(x + 1) | x = NAN }", &expected_deriv_bindings);
    expr_t *expected_value = expr_from_string("{ 1 - cos(x + 1) | x = NAN }", &expected_value_bindings);
    expr_t *expected_deriv_x = expected_deriv_bindings ? expr_bindings_get(expected_deriv_bindings, "x") : NULL;
    expr_t *expected_value_x = expected_value_bindings ? expr_bindings_get(expected_value_bindings, "x") : NULL;
    char *text = expr ? expr_to_string(expr, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(x);
    ASSERT_NOT_NULL(deriv);
    ASSERT_NOT_NULL(expected_deriv);
    ASSERT_NOT_NULL(expected_value);
    ASSERT_NOT_NULL(expected_deriv_x);
    ASSERT_NOT_NULL(expected_value_x);
    ASSERT_NOT_NULL(text);
    ASSERT_TRUE(strstr(text, "∫^(x + 1) sin(t)·dt") != NULL);

    for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
        char label[160];

        test_expr_set_val_d(x, points[i]);
        test_expr_set_val_d(expected_deriv_x, points[i]);
        test_expr_set_val_d(expected_value_x, points[i]);
        snprintf(label, sizeof(label), "d/dx integral to x+1 at x=%g", points[i]);
        check_q_at(__FILE__, __LINE__, 1, label, expr_eval_qf(deriv), expr_eval_qf(expected_deriv));

        snprintf(label, sizeof(label), "integral to x+1 at x=%g", points[i]);
        check_q_at(__FILE__, __LINE__, 1, label, expr_eval_qf(expr), expr_eval_qf(expected_value));
    }

    free(text);
    expr_free(expected_value);
    expr_bindings_free(expected_value_bindings);
    expr_free(expected_deriv);
    expr_bindings_free(expected_deriv_bindings);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_integrate_unevaluated_integral_explicit_bounds(void)
{
    static const double points[] = {-1.0, -0.25, 0.5, 1.25};
    expr_bindings_t *bindings = NULL;
    expr_bindings_t *expected_deriv_bindings = NULL;
    expr_t *expr = expr_from_string("{ ∫^x_1 t² dt | x = NAN }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    expr_t *expected_deriv = expr_from_string("{ x² | x = NAN }", &expected_deriv_bindings);
    expr_t *expected_deriv_x = expected_deriv_bindings ? expr_bindings_get(expected_deriv_bindings, "x") : NULL;
    char *text = expr ? expr_to_string(expr, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(x);
    ASSERT_NOT_NULL(deriv);
    ASSERT_NOT_NULL(expected_deriv);
    ASSERT_NOT_NULL(expected_deriv_x);
    ASSERT_NOT_NULL(text);
    ASSERT_TRUE(strstr(text, "∫^x_1 t²·dt") != NULL);

    for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
        char label[160];

        test_expr_set_val_d(x, points[i]);
        test_expr_set_val_d(expected_deriv_x, points[i]);
        snprintf(label, sizeof(label), "d/dx integral from 1 to x at x=%g", points[i]);
        check_q_at(__FILE__, __LINE__, 1, label, expr_eval_qf(deriv), expr_eval_qf(expected_deriv));
    }

    for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
        char label[160];
        double x_value = points[i];
        qfloat_t expected = qf_from_double((x_value * x_value * x_value - 1.0) / 3.0);

        test_expr_set_val_d(x, x_value);
        snprintf(label, sizeof(label), "integral from 1 to x at x=%g", x_value);
        check_q_at(__FILE__, __LINE__, 1, label, expr_eval_qf(expr), expected);
    }

    free(text);
    expr_free(expected_deriv);
    expr_bindings_free(expected_deriv_bindings);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_integrate_partial_symbolic_with_unevaluated_term(void)
{
    static const double points[] = {-0.75, -0.2, 0.4, 1.0};

    assert_string_antiderivative_matches("{ exp(cosh(x)) + x }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ exp(cosh(x)) + x }", "∫^x exp(cosh(t))·dt");
}

static void test_integrate_unevaluated_integral_display_symbolic_result(void)
{
    expr_bindings_t *bindings = NULL;
    expr_bindings_t *indefinite_bindings = NULL;
    expr_bindings_t *mismatch_bindings = NULL;
    expr_bindings_t *unsupported_bindings = NULL;
    expr_t *expr = expr_from_string("{ ∫^x sec²(t)·ln(tan(t) + cot(t))·dt | x = NAN }", &bindings);
    expr_t *display = expr ? expr_display_simplified(expr) : NULL;
    expr_t *indefinite = expr_from_string("{ @S sec²(x)·ln(tan(x) + cot(x))·dx }", &indefinite_bindings);
    expr_t *indefinite_display = indefinite ? expr_display_simplified(indefinite) : NULL;
    expr_t *mismatch =
        expr_from_string("{ ∫_0^t sec²(x)·ln(tan(x) + cot(x))·dt | t = pi/6, x = NAN }", &mismatch_bindings);
    expr_t *finite_improper = expr_from_string("{ ∫_0^(pi/6) sec²(x)·ln(tan(x) + cot(x))·dx }", NULL);
    expr_t *unsupported = expr_from_string("{ ∫^x exp(cosh(t)) dt | x = NAN }", &unsupported_bindings);
    expr_t *unsupported_display = unsupported ? expr_display_simplified(unsupported) : NULL;
    char *text = display ? expr_to_string(display, style_UNBOUND) : NULL;
    char *indefinite_text = indefinite_display ? expr_to_string(indefinite_display, style_UNBOUND) : NULL;
    char *unsupported_text = unsupported_display ? expr_to_string(unsupported_display, style_UNBOUND) : NULL;
    char mismatch_note[256];
    char finite_improper_note[256];

    mismatch_note[0] = '\0';
    finite_improper_note[0] = '\0';

    ASSERT_NOT_NULL(text);
    ASSERT_TRUE(strstr(text, "2x + tan(x)·(ln(tan(x) + cot(x)) - 1)") != NULL);
    ASSERT_TRUE(strstr(text, "∫") == NULL);
    ASSERT_TRUE(strstr(text, "+ C") == NULL);

    ASSERT_NOT_NULL(indefinite_text);
    ASSERT_TRUE(strstr(indefinite_text, "tan(x)·(ln(tan(x) + cot(x)) - 1)") != NULL);
    ASSERT_TRUE(strstr(indefinite_text, "+ C") != NULL);
    ASSERT_TRUE(strstr(indefinite_text, "C₀") == NULL);
    ASSERT_TRUE(strstr(indefinite_text, "∫") == NULL);

    ASSERT_TRUE(expr_integral_value_note(mismatch, mismatch_note, sizeof(mismatch_note)));
    ASSERT_TRUE(strstr(mismatch_note, "The differential is dt") != NULL);
    ASSERT_TRUE(strstr(mismatch_note, "Use dx") != NULL);
    ASSERT_TRUE(!expr_integral_value_note(finite_improper, finite_improper_note, sizeof(finite_improper_note)));

    ASSERT_NOT_NULL(unsupported_text);
    ASSERT_TRUE(strstr(unsupported_text, "∫^x exp(cosh(t))·dt") != NULL);
    ASSERT_TRUE(strstr(unsupported_text, "integral_meta") == NULL);

    free(unsupported_text);
    free(indefinite_text);
    free(text);
    expr_free(unsupported_display);
    expr_bindings_free(unsupported_bindings);
    expr_free(unsupported);
    expr_free(indefinite_display);
    expr_bindings_free(indefinite_bindings);
    expr_free(indefinite);
    expr_bindings_free(mismatch_bindings);
    expr_free(mismatch);
    expr_free(finite_improper);
    expr_free(display);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_integrate_symbolic_improper_endpoint_value(void)
{
    const qfloat_t expected = qf_from_string("0."
                                             "9530826542768191111486074290113740766671921043717468940128832979402964267"
                                             "9413097217556799413057238923482062905166545");
    expr_bindings_t *bound_bindings = NULL;
    expr_t *bound = expr_from_string("{ ∫_0^t sec²(x)·ln(tan(x) + cot(x))·dx | t = pi/6 }", &bound_bindings);
    expr_t *constant = expr_from_string("{ ∫_0^(pi/6) sec²(x)·ln(tan(x) + cot(x))·dx }", NULL);

    ASSERT_NOT_NULL(bound);
    ASSERT_NOT_NULL(constant);
    check_q_at(__FILE__, __LINE__, 1, "symbolic improper endpoint integral to t=pi/6", expr_eval_qf(bound), expected);
    check_q_at(__FILE__, __LINE__, 1, "symbolic improper endpoint integral to pi/6", expr_eval_qf(constant), expected);

    expr_free(constant);
    expr_bindings_free(bound_bindings);
    expr_free(bound);
}

static void test_integrate_inverse_one_plus_unit_circle_root(void)
{
    static const double points[] = {-0.5, -0.1, 0.25, 0.75};
    expr_bindings_t *bindings = NULL;
    expr_t *integral = expr_from_string("{ ∫ 1/(1 + sqrt(1 - x^2)) dx | x = NAN }", &bindings);
    expr_t *display = integral ? expr_display_simplified(integral) : NULL;
    char *text = display ? expr_to_string(display, style_UNBOUND) : NULL;

    assert_string_antiderivative_matches("{ 1/(1 + sqrt(1 - x^2)) }", points, sizeof(points) / sizeof(points[0]));

    ASSERT_NOT_NULL(text);
    ASSERT_TRUE(strstr(text, "asin(x)") != NULL);
    ASSERT_TRUE(strstr(text, "x/(√(1 - x²) + 1)") != NULL);
    ASSERT_TRUE(strstr(text, "+ C") != NULL);

    free(text);
    expr_free(display);
    expr_bindings_free(bindings);
    expr_free(integral);
}

static void test_integrate_sin_integer_multiple_quotient(void)
{
    static const double points[] = {0.05, 0.2, 0.45, 0.8};
    expr_bindings_t *bindings = NULL;
    expr_t *integral = expr_from_string("{ ∫ sin(5*x)/sin(9*x) dx | x = NAN }", &bindings);
    expr_t *display = integral ? expr_display_simplified(integral) : NULL;
    char *text = display ? expr_to_string(display, style_UNBOUND) : NULL;

    assert_string_antiderivative_matches("{ sin(5*x)/sin(9*x) }", points, sizeof(points) / sizeof(points[0]));

    ASSERT_NOT_NULL(text);
    ASSERT_TRUE(strstr(text, "ln(") != NULL);
    ASSERT_TRUE(strstr(text, "sin(") != NULL);
    ASSERT_TRUE(strstr(text, "+ C") != NULL);
    ASSERT_TRUE(strstr(text, "∫") == NULL);

    free(text);
    expr_free(display);
    expr_bindings_free(bindings);
    expr_free(integral);
}

static void test_integrate_inverse_sqrt_sin_cos_sin3_cos(void)
{
    static const double points[] = {0.05, 0.2, 0.45, 0.65};
    expr_bindings_t *bindings = NULL;
    expr_t *integral = expr_from_string("{ ∫ 1/(sqrt(sin(x)+cos(x))*sqrt(sin(3*x)+cos(x))) dx | x = NAN }", &bindings);
    expr_t *display = integral ? expr_display_simplified(integral) : NULL;
    char *text = display ? expr_to_string(display, style_UNBOUND) : NULL;

    assert_string_antiderivative_matches("{ 1/(sqrt(sin(x)+cos(x))*sqrt(sin(3*x)+cos(x))) }", points,
                                         sizeof(points) / sizeof(points[0]));

    ASSERT_NOT_NULL(text);
    ASSERT_TRUE(strstr(text, "atan(") != NULL);
    ASSERT_TRUE(strstr(text, "tan(x)") != NULL);
    ASSERT_TRUE(strstr(text, "+ C") != NULL);
    ASSERT_TRUE(strstr(text, "∫") == NULL);

    free(text);
    expr_free(display);
    expr_bindings_free(bindings);
    expr_free(integral);
}

static void test_integrate_inverse_quartic_appell_f1(void)
{
    static const double points[] = {1.2, 1.4, 1.8, 2.3};
    expr_bindings_t *bindings = NULL;
    expr_bindings_t *definite_bindings = NULL;
    expr_t *integral = expr_from_string("{ ∫ 1/((x^4-1)*(2*x^4-1)^(1/8)) dx | x = NAN }", &bindings);
    expr_t *display = integral ? expr_display_simplified(integral) : NULL;
    char *text = display ? expr_to_string(display, style_UNBOUND) : NULL;
    expr_t *definite = expr_from_string("{ ∫_2^5 1/((x^4-1)*(2*x^4-1)^(1/8)) dx | x = NAN }", &definite_bindings);
    expr_t *definite_display = definite ? expr_display_simplified(definite) : NULL;
    char *definite_text = definite_display ? expr_to_string(definite_display, style_UNBOUND) : NULL;
    number_t definite_value = num_new();

    assert_string_antiderivative_matches("{ 1/((x^4-1)*(2*x^4-1)^(1/8)) }", points, sizeof(points) / sizeof(points[0]));

    ASSERT_NOT_NULL(text);
    ASSERT_TRUE(strstr(text, "atan(") != NULL);
    ASSERT_TRUE(strstr(text, "atanh(") != NULL);
    ASSERT_TRUE(strstr(text, "√2") != NULL);
    ASSERT_TRUE(strstr(text, "¼·atan(") != NULL);
    ASSERT_TRUE(strstr(text, "⅛2") == NULL);
    ASSERT_TRUE(strstr(text, "⅛·2") == NULL);
    ASSERT_TRUE(strstr(text, "F₁(") == NULL);
    ASSERT_TRUE(strstr(text, "+ C") != NULL);
    ASSERT_TRUE(strstr(text, "∫") == NULL);

    ASSERT_NOT_NULL(definite_text);
    ASSERT_TRUE(strstr(definite_text, "atan(") != NULL);
    ASSERT_TRUE(strstr(definite_text, "atanh(") != NULL);
    ASSERT_TRUE(strstr(definite_text, "¼·atan(") != NULL);
    ASSERT_TRUE(strstr(definite_text, "⅛2") == NULL);
    ASSERT_TRUE(strstr(definite_text, "⅛·2") == NULL);
    ASSERT_TRUE(strstr(definite_text, "√2") != NULL);
    ASSERT_TRUE(strstr(definite_text, "0.176776") == NULL);
    ASSERT_TRUE(strstr(definite_text, "7.071067") == NULL);
    ASSERT_TRUE(strstr(definite_text, "2.828427") == NULL);
    ASSERT_TRUE(strstr(definite_text, "F₁(") == NULL);
    ASSERT_TRUE(strstr(definite_text, "∫") == NULL);
    num_destroy(&definite_value);
    definite_value = expr_eval(definite_display);
    ASSERT_TRUE(num_is_real(definite_value));
    ASSERT_TRUE(num_is_finite(definite_value));

    num_destroy(&definite_value);
    free(definite_text);
    expr_free(definite_display);
    expr_bindings_free(definite_bindings);
    expr_free(definite);
    free(text);
    expr_free(display);
    expr_bindings_free(bindings);
    expr_free(integral);
}

static void test_integrate_sec_double_angle_log_tan_cot(void)
{
    static const double points[] = {0.1, 0.2, 0.5};
    expr_bindings_t *bindings = NULL;
    expr_t *expr = NULL;
    expr_t *x = NULL;
    expr_t *lo = NULL;
    expr_t *hi = NULL;
    expr_t *vars[1];
    expr_t *los[1];
    expr_t *his[1];
    intg_bound_kind_t kinds[1] = {
        INTG_BOUND_DEFINITE,
    };
    expr_t *symbolic = NULL;
    char *symbolic_text = NULL;

    assert_string_antiderivative_matches("{ sec(2*x)*ln(tan(x)+cot(x)) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_matches("{ sec(x)^2*ln(tan(x)+cot(x)) }", points, sizeof(points) / sizeof(points[0]));
    assert_string_antiderivative_contains("{ sec(x)^2*ln(tan(x)+cot(x)) }", "tan(x)·(ln(tan(x) + cot(x)) - 1)");
    assert_string_antiderivative_contains("{ sec(2*x)*ln(tan(x)+cot(x)) }", "χ₂");
    assert_string_antiderivative_contains("{ sec(2*x)*ln(tan(x)+cot(x)) }", "tan(¼·(π - 4x))");
    assert_string_antiderivative_matches("{ ln(cot(x)+tan(x))*sec(2*x) }", points, sizeof(points) / sizeof(points[0]));

    expr = expr_from_string("{ sec(2*x)*ln(tan(x)+cot(x)) | x = NAN }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    lo = expr_from_string("{ 0 }", NULL);
    hi = expr_from_string("{ pi/6 }", NULL);
    vars[0] = x;
    los[0] = lo;
    his[0] = hi;
    symbolic = (expr && x && lo && hi)
                   ? intg_integrate_iterated_symbolic(expr, 1u, vars, kinds, los, his, 1u, NULL, NULL)
                   : NULL;
    symbolic_text = symbolic ? expr_to_string(symbolic, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(symbolic_text);
    ASSERT_TRUE(strstr(symbolic_text, "¹⁄₁₂π") != NULL);
    ASSERT_TRUE(strstr(symbolic_text, "2.094") == NULL);

    free(symbolic_text);
    expr_free(symbolic);
    expr_free(hi);
    expr_free(lo);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_integrate_exact_symbolic_atan_affine_scale(void)
{
    static const double points[] = {-1.0, -0.25, 0.5, 2.0};
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ 1/2*(ln(x^2 + 3*x + 5)"
                                    " - 2*atan((2*x + 3)/sqrt(11))/sqrt(11)) | x = NAN }",
                                    &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *anti = (expr && x) ? expr_integrate(expr, x) : NULL;
    expr_t *deriv = (anti && x) ? expr_create_deriv(anti, x) : NULL;
    char *text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(deriv);
    ASSERT_NOT_NULL(text);
    ASSERT_TRUE(strstr(text, "√(11)") != NULL);
    ASSERT_TRUE(strstr(text, "1.658312") == NULL);

    for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
        char label[160];

        test_expr_set_val_d(x, points[i]);
        snprintf(label, sizeof(label), "exact symbolic atan affine scale at x=%g", points[i]);
        check_q_at(__FILE__, __LINE__, 1, label, expr_eval_qf(deriv), expr_eval_qf(expr));
    }

    free(text);
    expr_free(deriv);
    expr_free(anti);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_integrate_unsupported_product_returns_null(void)
{
    expr_t *x = test_expr_new_named_var_d(0.5, "x");
    expr_t *w0_x = expr_lambert_w0(x);
    expr_t *x_w0_x = expr_mul(x, w0_x);
    expr_t *anti = expr_integrate(x_w0_x, x);

    ASSERT_TRUE(anti == NULL);

    expr_free(anti);
    expr_free(x_w0_x);
    expr_free(w0_x);
    expr_free(x);
}

static void test_integrate_scaled_symbolic_sum_before_product_search(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ 1/2*(C_0*x^2 + 2*C_1*x "
                                    "+ 4*(x^3 + 3*x^2 - 6*x - 11)/(x^2 + 3*x + 5)^3) + C_2 "
                                    "| x = NAN; C_0 = NAN, C_1 = NAN, C_2 = NAN }",
                                    &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *anti = (expr && x) ? expr_integrate(expr, x) : NULL;
    char *text = anti ? expr_to_string(anti, style_EXPRESSION) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(text);
    ASSERT_TRUE(strstr(text, "C₀") != NULL);
    ASSERT_TRUE(strstr(text, "C₁") != NULL);
    ASSERT_TRUE(strstr(text, "C₂") != NULL);
    ASSERT_TRUE(strstr(text, "(x² + 3x + 5)²") != NULL);

    free(text);
    expr_free(anti);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_integrate_collects_repeated_inverse_and_log_terms(void)
{
    static const double points[] = {-0.7, 0.2, 1.3};
    const char *input = "1/4*(-ln(x^4-x^2+1)+4*x*atan(x/(1-x^2))"
                        "-2*sqrt(3)*atan((2*x^2-1)/sqrt(3)))";
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string(input, &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *anti = (expr && x) ? expr_integrate(expr, x) : NULL;
    expr_t *deriv = (anti && x) ? expr_create_deriv(anti, x) : NULL;
    expr_t *display_deriv = deriv ? expr_display_expanded(deriv) : NULL;
    char *text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;
    char *deriv_text = display_deriv ? expr_to_string(display_deriv, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(deriv);
    ASSERT_NOT_NULL(text);
    ASSERT_NOT_NULL(deriv_text);
    ASSERT_TRUE(strstr(text, "2·(2x² + 1)·atan(x/(1 - x²))") != NULL);
    ASSERT_TRUE(strstr(text, "+ √3·ln(") != NULL);
    ASSERT_TRUE(strstr(text, "(2√3 - √3)") == NULL);
    ASSERT_TRUE(strstr(deriv_text, "3.464101") == NULL);
    ASSERT_TRUE(strstr(deriv_text, "/(x⁴ - x² + 1)") == NULL);
    ASSERT_TRUE(strstr(deriv_text, "¼·(") == NULL);
    ASSERT_TRUE(strstr(deriv_text, "x·atan(x/(1 - x²)) - ¼·ln(x⁴ - x² + 1)") != NULL);
    ASSERT_TRUE(strstr(deriv_text, "- ½√3·atan((2x² - 1)/√3)") != NULL);
    ASSERT_TRUE(strstr(deriv_text, "ln(x⁴ - x² + 1)") != NULL);
    ASSERT_TRUE(strstr(deriv_text, "atan((2x² - 1)/√3)") != NULL);

    for (size_t i = 0u; i < sizeof(points) / sizeof(points[0]); ++i) {
        char label[160];

        test_expr_set_val_d(x, points[i]);
        snprintf(label, sizeof(label), "collected inverse/log antiderivative at x=%g", points[i]);
        check_q_at(__FILE__, __LINE__, 1, label, expr_eval_qf(deriv), expr_eval_qf(expr));
    }

    free(deriv_text);
    free(text);
    expr_free(display_deriv);
    expr_free(deriv);
    expr_free(anti);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_integrate_polynomial_exponential_with_symbolic_rate(void)
{
    static const double x_points[] = {0.5, 1.25, 3.0};
    static const double y_points[] = {-0.7, 0.2, 1.1};
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ y^3*exp(2*x*y) | x = NAN, y = NAN }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *y = bindings ? expr_bindings_get(bindings, "y") : NULL;
    expr_t *anti = (expr && y) ? expr_integrate(expr, y) : NULL;
    expr_t *deriv = (anti && y) ? expr_create_deriv(anti, y) : NULL;
    char *text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(deriv);
    ASSERT_NOT_NULL(text);
    ASSERT_TRUE(strstr(text, "4y³·exp(2xy)/x") != NULL);
    ASSERT_TRUE(strstr(text, "6y·exp(2xy)/x³") != NULL);
    ASSERT_TRUE(strstr(text, "(2x)³") == NULL);
    ASSERT_TRUE(strstr(text, "(2x)⁴") == NULL);

    for (size_t i = 0u; i < sizeof(x_points) / sizeof(x_points[0]); ++i) {
        char label[160];

        test_expr_set_val_d(x, x_points[i]);
        test_expr_set_val_d(y, y_points[i]);
        snprintf(label, sizeof(label), "symbolic exponential rate at x=%g, y=%g", x_points[i], y_points[i]);
        check_q_at(__FILE__, __LINE__, 1, label, expr_eval_qf(deriv), expr_eval_qf(expr));
    }

    free(text);
    expr_free(deriv);
    expr_free(anti);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_integrate_log_times_affine_trigonometric_family(void)
{
    static const char *const inputs[] = {"ln(x)*sin(x)", "ln(x)*cos(x)", "ln(x)*sin(2*x+1)", "ln(x)*cos(3*x-2)"};

    for (size_t i = 0u; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        expr_bindings_t *bindings = NULL;
        expr_t *expr = expr_from_string(inputs[i], &bindings);
        expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
        expr_t *anti = (expr && x) ? expr_integrate(expr, x) : NULL;
        expr_t *deriv = (anti && x) ? expr_create_deriv(anti, x) : NULL;
        char *anti_text = anti ? expr_to_string(anti, style_UNBOUND) : NULL;

        ASSERT_NOT_NULL(anti);
        ASSERT_NOT_NULL(deriv);
        ASSERT_NOT_NULL(anti_text);
        ASSERT_TRUE(strstr(anti_text, "integral_meta") == NULL);
        ASSERT_TRUE(strstr(anti_text, "Si(") != NULL || strstr(anti_text, "Ci(") != NULL);

        for (size_t point = 0u; point < 3u; ++point) {
            static const double values[] = {0.25, 1.3, 2.5};

            test_expr_set_val_d(x, values[point]);
            check_q_at(__FILE__, __LINE__, 1, inputs[i], expr_eval_qf(deriv), expr_eval_qf(expr));
        }

        free(anti_text);
        expr_free(deriv);
        expr_free(anti);
        expr_free(expr);
        expr_bindings_free(bindings);
    }
}

static void test_integrate_power_composed_bessel_j_family(void)
{
    static const double points[] = {0.75, 1.0, 1.4};
    static const char *const inputs[] = {"BesselJ(-1/4, 1/2*x^2)", "BesselJ(1/5, 2/5*x^(5/2))"};

    for (size_t i = 0u; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        assert_string_antiderivative_matches(inputs[i], points, sizeof(points) / sizeof(points[0]));
        assert_string_antiderivative_round_trips(inputs[i]);
    }
}

static void test_integrate_power_composed_bessel_y_family(void)
{
    static const double points[] = {0.75, 1.0, 1.4};
    static const char *const inputs[] = {"BesselY(1/3, 2/3*x^(3/2))", "BesselY(2/5, 3/4*x^2)"};

    for (size_t i = 0u; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        assert_string_antiderivative_matches(inputs[i], points, sizeof(points) / sizeof(points[0]));
        assert_string_antiderivative_round_trips(inputs[i]);
    }
}

static void test_integrate_power_composed_lommel_s_family(void)
{
    static const double points[] = {0.75, 1.0, 1.4};
    static const char *const inputs[] = {"LommelS(0, 1/3, 2/3*x^(3/2))", "LommelS(1/2, 1/4, 3/4*x^2)"};

    for (size_t i = 0u; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        assert_string_antiderivative_matches(inputs[i], points, sizeof(points) / sizeof(points[0]));
        assert_string_antiderivative_round_trips(inputs[i]);
    }
}

static void test_integrate_hypergeometric_pFq_monomial_rule(void)
{
    static const double points[] = {0.2, 0.5, 0.8};
    static const char *const inputs[] = {"pFq(0, 0, x)", "pFq(0, 0, x^2)", "pFq(1, 1, 1/2, 3/2, x^3)"};

    for (size_t i = 0u; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        assert_string_antiderivative_matches(inputs[i], points, sizeof(points) / sizeof(points[0]));
        assert_string_antiderivative_round_trips(inputs[i]);
    }
}

void test_symbolic_integration(void)
{
    TEST_RUN_SUBTEST(test_integrate_polynomial_sum, NULL);
    TEST_RUN_SUBTEST(test_integrate_reciprocal_and_log, NULL);
    TEST_RUN_SUBTEST(test_integrate_affine_elementary, NULL);
    TEST_RUN_SUBTEST(test_integrate_affine_tangent, NULL);
    TEST_RUN_SUBTEST(test_integrate_affine_more_trig_and_stats, NULL);
    TEST_RUN_SUBTEST(test_integrate_affine_logs_inverse_and_specials, NULL);
    TEST_RUN_SUBTEST(test_integrate_affine_powers_and_remaining_inverse_families, NULL);
    TEST_RUN_SUBTEST(test_integrate_affine_poly_times_specials, NULL);
    TEST_RUN_SUBTEST(test_integrate_other_variable_as_constant, NULL);
    TEST_RUN_SUBTEST(test_integrate_definite_symbolic_bounds, NULL);
    TEST_RUN_SUBTEST(test_integrate_definite_quadratic_compact_exact_result, NULL);
    TEST_RUN_SUBTEST(test_integrate_definite_quadratic_combines_exact_constants, NULL);
    TEST_RUN_SUBTEST(test_integrate_inverse_square_with_exact_bound, NULL);
    TEST_RUN_SUBTEST(test_integrate_shifted_inverse_square_with_symbolic_constant, NULL);
    TEST_RUN_SUBTEST(test_integrate_symbolic_power_exponent, NULL);
    TEST_RUN_SUBTEST(test_integrate_constant_base_affine_exponent, NULL);
    TEST_RUN_SUBTEST(test_integrate_symbolic_shifted_sqrt, NULL);
    TEST_RUN_SUBTEST(test_integrate_poly_times_affine_power, NULL);
    TEST_RUN_SUBTEST(test_integrate_poly_over_centered_quadratic, NULL);
    TEST_RUN_SUBTEST(test_integrate_symbolic_general_quadratic_denominator, NULL);
    TEST_RUN_SUBTEST(test_integrate_symbolic_general_quadratic_roots, NULL);
    TEST_RUN_SUBTEST(test_integrate_log_quadratic, NULL);
    TEST_RUN_SUBTEST(test_integrate_negative_quadratic_exponential, NULL);
    TEST_RUN_SUBTEST(test_integrate_centered_quadratic_roots, NULL);
    TEST_RUN_SUBTEST(test_integrate_trig_power_products, NULL);
    TEST_RUN_SUBTEST(test_integrate_mixed_frequency_exp_unary, NULL);
    TEST_RUN_SUBTEST(test_integrate_iterated_exp_unary_derivatives, NULL);
    TEST_RUN_SUBTEST(test_integrate_hyperbolic_table_tail, NULL);
    TEST_RUN_SUBTEST(test_integrate_frequency_product_families, NULL);
    TEST_RUN_SUBTEST(test_integrate_more_by_parts, NULL);
    TEST_RUN_SUBTEST(test_integrate_partial_fractions, NULL);
    TEST_RUN_SUBTEST(test_integrate_quotient_rule_derivative, NULL);
    TEST_RUN_SUBTEST(test_integrate_unevaluated_integral_derivative, NULL);
    TEST_RUN_SUBTEST(test_integrate_unevaluated_integral_leibniz_derivative, NULL);
    TEST_RUN_SUBTEST(test_integrate_unevaluated_integral_evaluation, NULL);
    TEST_RUN_SUBTEST(test_integrate_unevaluated_integral_constant_upper, NULL);
    TEST_RUN_SUBTEST(test_integrate_unevaluated_integral_chain_rule_upper, NULL);
    TEST_RUN_SUBTEST(test_integrate_unevaluated_integral_explicit_bounds, NULL);
    TEST_RUN_SUBTEST(test_integrate_partial_symbolic_with_unevaluated_term, NULL);
    TEST_RUN_SUBTEST(test_integrate_unevaluated_integral_display_symbolic_result, NULL);
    TEST_RUN_SUBTEST(test_integrate_symbolic_improper_endpoint_value, NULL);
    TEST_RUN_SUBTEST(test_integrate_inverse_one_plus_unit_circle_root, NULL);
    TEST_RUN_SUBTEST(test_integrate_sin_integer_multiple_quotient, NULL);
    TEST_RUN_SUBTEST(test_integrate_inverse_sqrt_sin_cos_sin3_cos, NULL);
    TEST_RUN_SUBTEST(test_integrate_inverse_quartic_appell_f1, NULL);
    TEST_RUN_SUBTEST(test_integrate_sec_double_angle_log_tan_cot, NULL);
    TEST_RUN_SUBTEST(test_integrate_exact_symbolic_atan_affine_scale, NULL);
    TEST_RUN_SUBTEST(test_integrate_scaled_symbolic_sum_before_product_search, NULL);
    TEST_RUN_SUBTEST(test_integrate_collects_repeated_inverse_and_log_terms, NULL);
    TEST_RUN_SUBTEST(test_integrate_polynomial_exponential_with_symbolic_rate, NULL);
    TEST_RUN_SUBTEST(test_integrate_log_times_affine_trigonometric_family, NULL);
    TEST_RUN_SUBTEST(test_integrate_power_composed_bessel_j_family, NULL);
    TEST_RUN_SUBTEST(test_integrate_power_composed_bessel_y_family, NULL);
    TEST_RUN_SUBTEST(test_integrate_power_composed_lommel_s_family, NULL);
    TEST_RUN_SUBTEST(test_integrate_hypergeometric_pFq_monomial_rule, NULL);
    TEST_RUN_SUBTEST(test_integrate_unsupported_product_returns_null, NULL);
}
