#include "test_expr.h"
#include "internal/expr_internal.h"

static void assert_antiderivative_matches(const char *label,
                                          const expr_t *expr,
                                          expr_t *x,
                                          const double *points,
                                          size_t npoints)
{
    expr_t *anti = expr_integrate(expr, x);
    expr_t *deriv = anti ? expr_create_deriv(anti, x) : NULL;

    ASSERT_NOT_NULL(anti);
    ASSERT_NOT_NULL(deriv);

    for (size_t i = 0; i < npoints; ++i) {
        char point_label[160];

        test_expr_set_val_d(x, points[i]);
        snprintf(point_label, sizeof(point_label), "%s at x=%g", label, points[i]);
        check_q_at(__FILE__, __LINE__, 1, point_label,
                   expr_eval_qf(deriv), expr_eval_qf(expr));
    }

    expr_free(deriv);
    expr_free(anti);
}

static void test_integrate_polynomial_sum(void)
{
    static const double points[] = { -2.0, -0.5, 0.0, 1.25, 3.0 };
    expr_t *x = test_expr_new_named_var_d(0.0, "x");
    expr_t *x2 = test_expr_pow_d(x, 2.0);
    expr_t *three_x2 = test_expr_mul_d(x2, 3.0);
    expr_t *two_x = test_expr_mul_d(x, 2.0);
    expr_t *partial = expr_add(three_x2, two_x);
    expr_t *f = test_expr_add_d(partial, 5.0);

    assert_antiderivative_matches("integral derivative of 3*x^2 + 2*x + 5",
                                  f, x, points, sizeof(points) / sizeof(points[0]));

    expr_free(f);
    expr_free(partial);
    expr_free(two_x);
    expr_free(three_x2);
    expr_free(x2);
    expr_free(x);
}

static void test_integrate_reciprocal_and_log(void)
{
    static const double points[] = { 0.25, 0.75, 1.5, 4.0 };
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *one_over_x = test_expr_d_div(1.0, x);
    expr_t *log_x = expr_log(x);

    assert_antiderivative_matches("integral derivative of 1/x",
                                  one_over_x, x, points,
                                  sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of log(x)",
                                  log_x, x, points,
                                  sizeof(points) / sizeof(points[0]));

    expr_free(log_x);
    expr_free(one_over_x);
    expr_free(x);
}

static void test_integrate_affine_elementary(void)
{
    static const double points[] = { -0.75, 0.0, 0.5, 1.25 };
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

    assert_antiderivative_matches("integral derivative of affine exp/sin/cos",
                                  f, x, points, sizeof(points) / sizeof(points[0]));

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
    static const double points[] = { -0.5, -0.1, 0.0, 0.35 };
    expr_t *x = test_expr_new_named_var_d(0.0, "x");
    expr_t *tan_x = expr_tan(x);
    expr_t *tanh_x = expr_tanh(x);
    expr_t *two_x = test_expr_mul_d(x, 2.0);
    expr_t *affine_arg = test_expr_sub_d(two_x, 1.0);
    expr_t *tan_affine = expr_tan(affine_arg);
    expr_t *tanh_affine = expr_tanh(affine_arg);

    assert_antiderivative_matches("integral derivative of tan(x)",
                                  tan_x, x, points, sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of tan(2*x - 1)",
                                  tan_affine, x, points, sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of tanh(x)",
                                  tanh_x, x, points, sizeof(points) / sizeof(points[0]));
    assert_antiderivative_matches("integral derivative of tanh(2*x - 1)",
                                  tanh_affine, x, points,
                                  sizeof(points) / sizeof(points[0]));

    expr_free(tanh_affine);
    expr_free(tan_affine);
    expr_free(affine_arg);
    expr_free(two_x);
    expr_free(tanh_x);
    expr_free(tan_x);
    expr_free(x);
}

static void test_integrate_other_variable_as_constant(void)
{
    static const double points[] = { -1.0, 0.0, 2.0 };
    expr_t *x = test_expr_new_named_var_d(0.0, "x");
    expr_t *y = test_expr_new_named_var_d(7.0, "y");

    assert_antiderivative_matches("integral derivative treats y as constant",
                                  y, x, points, sizeof(points) / sizeof(points[0]));

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
    ASSERT_TRUE(strstr(text, "a") != NULL);
    ASSERT_TRUE(strstr(text, "b") != NULL);

    test_expr_set_val_d(a, 2.0);
    test_expr_set_val_d(b, 5.0);
    check_q_at(__FILE__, __LINE__, 1, "definite integral x^2 from a to b",
               expr_eval_qf(simplified), qf_from_double(39.0));

    test_expr_set_val_d(a, 0.0);
    test_expr_set_val_d(b, 1.0);
    check_q_at(__FILE__, __LINE__, 1, "definite integral x^2 from 0 to 1",
               expr_eval_qf(simplified), qf_div(qf_from_double(1.0), qf_from_double(3.0)));

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

static void test_integrate_unsupported_product_returns_null(void)
{
    expr_t *x = test_expr_new_named_var_d(0.5, "x");
    expr_t *sin_x = expr_sin(x);
    expr_t *x_sin_x = expr_mul(x, sin_x);
    expr_t *anti = expr_integrate(x_sin_x, x);

    ASSERT_TRUE(anti == NULL);

    expr_free(anti);
    expr_free(x_sin_x);
    expr_free(sin_x);
    expr_free(x);
}

void test_symbolic_integration(void)
{
    TEST_RUN_SUBTEST(test_integrate_polynomial_sum, NULL);
    TEST_RUN_SUBTEST(test_integrate_reciprocal_and_log, NULL);
    TEST_RUN_SUBTEST(test_integrate_affine_elementary, NULL);
    TEST_RUN_SUBTEST(test_integrate_affine_tangent, NULL);
    TEST_RUN_SUBTEST(test_integrate_other_variable_as_constant, NULL);
    TEST_RUN_SUBTEST(test_integrate_definite_symbolic_bounds, NULL);
    TEST_RUN_SUBTEST(test_integrate_unsupported_product_returns_null, NULL);
}
