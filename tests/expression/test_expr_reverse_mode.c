#include "test_expr.h"

static void test_reverse_gradient_polynomial(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *x2 = expr_pow_d(x, 2.0);
    expr_t *xy = expr_mul(x, y);
    expr_t *y2 = expr_pow_d(y, 2.0);
    expr_t *t0 = expr_add(x2, xy);
    expr_t *f = expr_add(t0, y2);
    const expr_t *vars[2] = {x, y};
    number_t value;
    number_t grads[2];
    number_t expect;

    if (expr_eval_derivatives(f, 2, vars, &value, grads) != 0) {
        printf(C_BOLD C_RED "FAIL" C_RESET " reverse polynomial gradient returned error\n");
        TEST_FAIL();
    }

    expect = num_create_from_string("7");
    ASSERT_EXPR_NUMBER_EQ(value, expect);
    num_destroy(&expect);
    expect = num_create_from_string("4");
    ASSERT_EXPR_NUMBER_EQ(grads[0], expect);
    num_destroy(&expect);
    expect = num_create_from_string("5");
    ASSERT_EXPR_NUMBER_EQ(grads[1], expect);
    num_destroy(&expect);

    num_destroy(&grads[1]);
    num_destroy(&grads[0]);
    num_destroy(&value);
    expr_free(f);
    expr_free(t0);
    expr_free(y2);
    expr_free(xy);
    expr_free(x2);
    expr_free(y);
    expr_free(x);
}

static void test_reverse_gradient_shared_subexpression(void)
{
    expr_t *x = test_expr_new_named_var_d(1.5, "x");
    expr_t *y = test_expr_new_named_var_d(-0.5, "y");
    expr_t *s = expr_add(x, y);
    expr_t *f = expr_mul(s, s);
    const expr_t *vars[2] = {x, y};
    number_t value;
    number_t grads[2];
    number_t expect;

    if (expr_eval_derivatives(f, 2, vars, &value, grads) != 0) {
        printf(C_BOLD C_RED "FAIL" C_RESET " reverse shared-subexpression gradient returned error\n");
        TEST_FAIL();
    }

    expect = num_create_from_string("1");
    ASSERT_EXPR_NUMBER_EQ(value, expect);
    num_destroy(&expect);
    expect = num_create_from_string("2");
    ASSERT_EXPR_NUMBER_EQ(grads[0], expect);
    ASSERT_EXPR_NUMBER_EQ(grads[1], expect);
    num_destroy(&expect);

    num_destroy(&grads[1]);
    num_destroy(&grads[0]);
    num_destroy(&value);
    expr_free(f);
    expr_free(s);
    expr_free(y);
    expr_free(x);
}

static void test_reverse_matches_symbolic_composite(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *xy = expr_mul(x, y);
    expr_t *sin_xy = expr_sin(xy);
    expr_t *exp_term = expr_exp(sin_xy);
    expr_t *log_y = expr_log(y);
    expr_t *x_log_y = expr_mul(x, log_y);
    expr_t *f = expr_add(exp_term, x_log_y);
    const expr_t *vars[2] = {x, y};
    number_t grads[2];
    number_t value;
    expr_t *df_dx = expr_create_deriv(f, x);
    expr_t *df_dy = expr_create_deriv(f, y);
    number_t expect;

    if (expr_eval_derivatives(f, 2, vars, &value, grads) != 0) {
        printf(C_BOLD C_RED "FAIL" C_RESET " reverse composite gradient returned error\n");
        TEST_FAIL();
    }

    expect = expr_eval(f);
    ASSERT_EXPR_NUMBER_EQ(value, expect);
    num_destroy(&expect);
    expect = expr_eval(df_dx);
    ASSERT_EXPR_NUMBER_CLOSE(grads[0], expect);
    num_destroy(&expect);
    expect = expr_eval(df_dy);
    ASSERT_EXPR_NUMBER_CLOSE(grads[1], expect);
    num_destroy(&expect);

    num_destroy(&grads[1]);
    num_destroy(&grads[0]);
    num_destroy(&value);
    expr_free(df_dy);
    expr_free(df_dx);
    expr_free(f);
    expr_free(x_log_y);
    expr_free(log_y);
    expr_free(exp_term);
    expr_free(sin_xy);
    expr_free(xy);
    expr_free(y);
    expr_free(x);
}

static void test_reverse_gradient_missing_variable(void)
{
    expr_t *x = test_expr_new_named_var_d(3.0, "x");
    expr_t *z = test_expr_new_named_var_d(9.0, "z");
    expr_t *f = expr_mul_d(x, 4.0);
    const expr_t *vars[2] = {x, z};
    number_t grads[2];
    number_t expect;

    if (expr_eval_derivatives(f, 2, vars, NULL, grads) != 0) {
        printf(C_BOLD C_RED "FAIL" C_RESET " reverse missing-variable gradient returned error\n");
        TEST_FAIL();
    }

    expect = num_create_from_string("4");
    ASSERT_EXPR_NUMBER_EQ(grads[0], expect);
    num_destroy(&expect);
    ASSERT_EXPR_NUMBER_EQ(grads[1], NUM_ZERO);

    num_destroy(&grads[1]);
    num_destroy(&grads[0]);
    expr_free(f);
    expr_free(z);
    expr_free(x);
}

static void test_reverse_gradient_polynomial_num(void)
{
    size_t old_prec_bits = num_get_default_prec_bits();
    number_t x0;
    number_t y0;
    number_t expect;
    expr_t *x;
    expr_t *y;
    expr_t *x2;
    expr_t *xy;
    expr_t *y2;
    expr_t *t0;
    expr_t *f;
    const expr_t *vars[2];
    number_t value;
    number_t grads[2];

    ASSERT_EQ_INT(num_set_default_prec_bits(384u), 0);
    x0 = num_create_from_string("1");
    y0 = num_create_from_string("2");
    x = expr_new_named_var(x0, "x");
    y = expr_new_named_var(y0, "y");
    num_destroy(&y0);
    num_destroy(&x0);
    x2 = expr_pow_d(x, 2.0);
    xy = expr_mul(x, y);
    y2 = expr_pow_d(y, 2.0);
    t0 = expr_add(x2, xy);
    f = expr_add(t0, y2);
    vars[0] = x;
    vars[1] = y;

    ASSERT_EQ_INT(expr_eval_derivatives(f, 2u, vars, &value, grads), 0);
    expect = num_create_from_string("7");
    ASSERT_EXPR_NUMBER_EQ(value, expect);
    num_destroy(&expect);
    expect = num_create_from_string("4");
    ASSERT_EXPR_NUMBER_EQ(grads[0], expect);
    num_destroy(&expect);
    expect = num_create_from_string("5");
    ASSERT_EXPR_NUMBER_EQ(grads[1], expect);
    num_destroy(&expect);
    ASSERT_EQ_INT((int)num_get_prec_bits(value), 384);
    ASSERT_EQ_INT((int)num_get_prec_bits(grads[0]), 53);
    ASSERT_EQ_INT((int)num_get_prec_bits(grads[1]), 53);

    num_destroy(&grads[1]);
    num_destroy(&grads[0]);
    num_destroy(&value);
    expr_free(f);
    expr_free(t0);
    expr_free(y2);
    expr_free(xy);
    expr_free(x2);
    expr_free(y);
    expr_free(x);
    ASSERT_EQ_INT(num_set_default_prec_bits(old_prec_bits), 0);
}

static void test_reverse_gradient_complex_number_t(void)
{
    number_t z0 = num_create_from_string("1 + 2i");
    expr_t *z = expr_new_named_var(z0, "z");
    expr_t *exp_z = expr_exp(z);
    expr_t *z2 = expr_pow_d(z, 2.0);
    expr_t *f = expr_add(exp_z, z2);
    expr_t *df_dz = expr_create_deriv(f, z);
    const expr_t *vars[1] = {z};
    number_t value;
    number_t grad;
    number_t expect_value;
    number_t expect_grad;

    ASSERT_EQ_INT(expr_eval_derivatives(f, 1u, vars, &value, &grad), 0);
    expect_value = expr_eval(f);
    expect_grad = expr_eval(df_dz);

    ASSERT_EXPR_NUMBER_CLOSE(value, expect_value);
    ASSERT_EXPR_NUMBER_CLOSE(grad, expect_grad);
    ASSERT_TRUE(!num_is_real(value));
    ASSERT_TRUE(!num_is_real(grad));

    num_destroy(&expect_grad);
    num_destroy(&expect_value);
    num_destroy(&grad);
    num_destroy(&value);
    expr_free(df_dz);
    expr_free(f);
    expr_free(z2);
    expr_free(exp_z);
    expr_free(z);
    num_destroy(&z0);
}

static void assert_reverse_matches_symbolic(expr_t *function, size_t variable_count, expr_t *const *variables)
{
    const expr_t **wrt = calloc(variable_count, sizeof(*wrt));
    number_t *gradients = calloc(variable_count, sizeof(*gradients));
    number_t value;

    ASSERT_NOT_NULL(wrt);
    ASSERT_NOT_NULL(gradients);
    for (size_t i = 0u; i < variable_count; ++i)
        wrt[i] = variables[i];
    ASSERT_EQ_INT(expr_eval_derivatives(function, variable_count, wrt, &value, gradients), 0);
    for (size_t i = 0u; i < variable_count; ++i) {
        expr_t *derivative = expr_create_deriv(function, variables[i]);
        number_t expected;

        ASSERT_NOT_NULL(derivative);
        expected = expr_eval(derivative);
        ASSERT_EXPR_NUMBER_CLOSE(gradients[i], expected);
        num_destroy(&expected);
        expr_free(derivative);
        num_destroy(&gradients[i]);
    }
    num_destroy(&value);
    free(gradients);
    free(wrt);
}

static void test_reverse_special_function_arguments(void)
{
    expr_t *order = test_expr_new_const_d(0.25);
    expr_t *mu = test_expr_new_const_d(0.5);
    expr_t *nu = test_expr_new_const_d(0.25);
    expr_t *x = test_expr_new_named_var_d(1.2, "x");
    expr_t *bessel_j = expr_bessel_j(order, x);
    expr_t *bessel_y = expr_bessel_y(order, x);
    expr_t *lommel = expr_lommel_s(mu, nu, x);
    expr_t *variables[] = {x};

    ASSERT_NOT_NULL(bessel_j);
    ASSERT_NOT_NULL(bessel_y);
    ASSERT_NOT_NULL(lommel);
    assert_reverse_matches_symbolic(bessel_j, 1u, variables);
    assert_reverse_matches_symbolic(bessel_y, 1u, variables);
    assert_reverse_matches_symbolic(lommel, 1u, variables);

    expr_free(lommel);
    expr_free(bessel_y);
    expr_free(bessel_j);
    expr_free(x);
    expr_free(nu);
    expr_free(mu);
    expr_free(order);
}

static void test_reverse_hypergeometric_argument(void)
{
    expr_t *upper_value = test_expr_new_const_d(0.5);
    expr_t *lower_value = test_expr_new_const_d(1.5);
    expr_t *x = test_expr_new_named_var_d(0.2, "x");
    const expr_t *upper[] = {upper_value};
    const expr_t *lower[] = {lower_value};
    expr_t *function = expr_hypergeometric_pFq(1u, upper, 1u, lower, x);
    expr_t *variables[] = {x};

    ASSERT_NOT_NULL(function);
    assert_reverse_matches_symbolic(function, 1u, variables);

    expr_free(function);
    expr_free(x);
    expr_free(lower_value);
    expr_free(upper_value);
}

static void test_reverse_appell_variables(void)
{
    expr_t *a = test_expr_new_const_d(0.5);
    expr_t *b1 = test_expr_new_const_d(0.25);
    expr_t *b2 = test_expr_new_const_d(0.4);
    expr_t *c = test_expr_new_const_d(1.5);
    expr_t *x = test_expr_new_named_var_d(0.1, "x");
    expr_t *y = test_expr_new_named_var_d(0.2, "y");
    expr_t *function = expr_appell_f1(a, b1, b2, c, x, y);
    expr_t *variables[] = {x, y};

    ASSERT_NOT_NULL(function);
    assert_reverse_matches_symbolic(function, 2u, variables);

    expr_free(function);
    expr_free(y);
    expr_free(x);
    expr_free(c);
    expr_free(b2);
    expr_free(b1);
    expr_free(a);
}

static void test_reverse_lauricella_variables(void)
{
    expr_t *a = test_expr_new_const_d(0.5);
    expr_t *b1 = test_expr_new_const_d(0.25);
    expr_t *b2 = test_expr_new_const_d(0.4);
    expr_t *b3 = test_expr_new_const_d(0.3);
    expr_t *c = test_expr_new_const_d(1.5);
    expr_t *x = test_expr_new_named_var_d(0.1, "x");
    expr_t *y = test_expr_new_named_var_d(0.15, "y");
    expr_t *z = test_expr_new_named_var_d(0.2, "z");
    const expr_t *parameters[] = {b1, b2, b3};
    const expr_t *arguments[] = {x, y, z};
    expr_t *function = expr_lauricella_f(a, 3u, parameters, c, arguments);
    expr_t *variables[] = {x, y, z};

    ASSERT_NOT_NULL(function);
    assert_reverse_matches_symbolic(function, 3u, variables);

    expr_free(function);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    expr_free(c);
    expr_free(b3);
    expr_free(b2);
    expr_free(b1);
    expr_free(a);
}

void test_reverse_mode(void)
{
    TEST_RUN_SUBTEST(test_reverse_gradient_polynomial, NULL);
    TEST_RUN_SUBTEST(test_reverse_gradient_shared_subexpression, NULL);
    TEST_RUN_SUBTEST(test_reverse_matches_symbolic_composite, NULL);
    TEST_RUN_SUBTEST(test_reverse_gradient_missing_variable, NULL);
    TEST_RUN_SUBTEST(test_reverse_gradient_polynomial_num, NULL);
    TEST_RUN_SUBTEST(test_reverse_gradient_complex_number_t, NULL);
    TEST_RUN_SUBTEST(test_reverse_special_function_arguments, NULL);
    TEST_RUN_SUBTEST(test_reverse_hypergeometric_argument, NULL);
    TEST_RUN_SUBTEST(test_reverse_appell_variables, NULL);
    TEST_RUN_SUBTEST(test_reverse_lauricella_variables, NULL);
}
