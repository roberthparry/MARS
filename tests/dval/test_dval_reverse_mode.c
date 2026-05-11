#include "test_dval.h"

static int number_close_for_qfloat_precision(const number_t got,
                                             const number_t expected)
{
    number_t diff = num_sub(got, expected);
    number_t error;
    number_t one = num_create_from_double(1.0);
    number_t tolerance;
    int ok;

    ASSERT_EQ_INT(num_set_prec_bits(&one, 106u), 0);
    tolerance = num_ldexp(one, 4 - 106);
    num_destroy(&one);

    if (num_is_real(diff))
        error = num_abs(diff);
    else {
        number_t real = num_real_part(diff);
        number_t imag = num_imag_part(diff);

        error = num_hypot(real, imag);
        num_destroy(&imag);
        num_destroy(&real);
    }
    ok = num_le(error, tolerance);
    num_destroy(&tolerance);
    num_destroy(&error);
    num_destroy(&diff);
    return ok;
}

static void test_reverse_gradient_polynomial(void)
{
    dval_t *x  = test_dv_new_named_var_d(1.0, "x");
    dval_t *y  = test_dv_new_named_var_d(2.0, "y");
    dval_t *x2 = dv_pow_d(x, 2.0);
    dval_t *xy = dv_mul(x, y);
    dval_t *y2 = dv_pow_d(y, 2.0);
    dval_t *t0 = dv_add(x2, xy);
    dval_t *f  = dv_add(t0, y2);
    const dval_t *vars[2] = { x, y };
    number_t value;
    number_t grads[2];
    number_t expect;

    if (dv_eval_derivatives(f, 2, vars, &value, grads) != 0) {
        printf(C_BOLD C_RED "FAIL" C_RESET " reverse polynomial gradient returned error\n");
        TEST_FAIL();
    }

    expect = num_create_from_string("7");
    ASSERT_TRUE(num_eq(value, expect));
    num_destroy(&expect);
    expect = num_create_from_string("4");
    ASSERT_TRUE(num_eq(grads[0], expect));
    num_destroy(&expect);
    expect = num_create_from_string("5");
    ASSERT_TRUE(num_eq(grads[1], expect));
    num_destroy(&expect);

    num_destroy(&grads[1]);
    num_destroy(&grads[0]);
    num_destroy(&value);
    dv_free(f); dv_free(t0); dv_free(y2); dv_free(xy); dv_free(x2); dv_free(y); dv_free(x);
}

static void test_reverse_gradient_shared_subexpression(void)
{
    dval_t *x = test_dv_new_named_var_d(1.5, "x");
    dval_t *y = test_dv_new_named_var_d(-0.5, "y");
    dval_t *s = dv_add(x, y);
    dval_t *f = dv_mul(s, s);
    const dval_t *vars[2] = { x, y };
    number_t value;
    number_t grads[2];
    number_t expect;

    if (dv_eval_derivatives(f, 2, vars, &value, grads) != 0) {
        printf(C_BOLD C_RED "FAIL" C_RESET " reverse shared-subexpression gradient returned error\n");
        TEST_FAIL();
    }

    expect = num_create_from_string("1");
    ASSERT_TRUE(num_eq(value, expect));
    num_destroy(&expect);
    expect = num_create_from_string("2");
    ASSERT_TRUE(num_eq(grads[0], expect));
    ASSERT_TRUE(num_eq(grads[1], expect));
    num_destroy(&expect);

    num_destroy(&grads[1]);
    num_destroy(&grads[0]);
    num_destroy(&value);
    dv_free(f); dv_free(s); dv_free(y); dv_free(x);
}

static void test_reverse_matches_forward_composite(void)
{
    dval_t *x = test_dv_new_named_var_d(1.0, "x");
    dval_t *y = test_dv_new_named_var_d(2.0, "y");
    dval_t *xy = dv_mul(x, y);
    dval_t *sin_xy = dv_sin(xy);
    dval_t *exp_term = dv_exp(sin_xy);
    dval_t *log_y = dv_log(y);
    dval_t *x_log_y = dv_mul(x, log_y);
    dval_t *f = dv_add(exp_term, x_log_y);
    const dval_t *vars[2] = { x, y };
    number_t grads[2];
    number_t value;
    dval_t *df_dx = dv_create_deriv(f, x);
    dval_t *df_dy = dv_create_deriv(f, y);
    number_t expect;

    if (dv_eval_derivatives(f, 2, vars, &value, grads) != 0) {
        printf(C_BOLD C_RED "FAIL" C_RESET " reverse composite gradient returned error\n");
        TEST_FAIL();
    }

    expect = dv_eval_num(f);
    ASSERT_TRUE(num_eq(value, expect));
    num_destroy(&expect);
    expect = dv_eval_num(df_dx);
    ASSERT_TRUE(num_eq(grads[0], expect) ||
                number_close_for_qfloat_precision(grads[0], expect));
    num_destroy(&expect);
    expect = dv_eval_num(df_dy);
    ASSERT_TRUE(num_eq(grads[1], expect) ||
                number_close_for_qfloat_precision(grads[1], expect));
    num_destroy(&expect);

    num_destroy(&grads[1]);
    num_destroy(&grads[0]);
    num_destroy(&value);
    dv_free(df_dy); dv_free(df_dx);
    dv_free(f); dv_free(x_log_y); dv_free(log_y); dv_free(exp_term); dv_free(sin_xy); dv_free(xy);
    dv_free(y); dv_free(x);
}

static void test_reverse_gradient_missing_variable(void)
{
    dval_t *x = test_dv_new_named_var_d(3.0, "x");
    dval_t *z = test_dv_new_named_var_d(9.0, "z");
    dval_t *f = dv_mul_d(x, 4.0);
    const dval_t *vars[2] = { x, z };
    number_t grads[2];
    number_t expect;

    if (dv_eval_derivatives(f, 2, vars, NULL, grads) != 0) {
        printf(C_BOLD C_RED "FAIL" C_RESET " reverse missing-variable gradient returned error\n");
        TEST_FAIL();
    }

    expect = num_create_from_string("4");
    ASSERT_TRUE(num_eq(grads[0], expect));
    num_destroy(&expect);
    ASSERT_TRUE(num_eq(grads[1], NUM_ZERO));

    num_destroy(&grads[1]);
    num_destroy(&grads[0]);
    dv_free(f); dv_free(z); dv_free(x);
}

static void test_reverse_gradient_polynomial_num(void)
{
    size_t old_prec_bits = num_get_default_prec_bits();
    number_t x0;
    number_t y0;
    number_t expect;
    dval_t *x;
    dval_t *y;
    dval_t *x2;
    dval_t *xy;
    dval_t *y2;
    dval_t *t0;
    dval_t *f;
    const dval_t *vars[2];
    number_t value;
    number_t grads[2];

    ASSERT_EQ_INT(num_set_default_prec_bits(384u), 0);
    x0 = num_create_from_string("1");
    y0 = num_create_from_string("2");
    x = dv_new_named_var_num(x0, "x");
    y = dv_new_named_var_num(y0, "y");
    num_destroy(&y0);
    num_destroy(&x0);
    x2 = dv_pow_d(x, 2.0);
    xy = dv_mul(x, y);
    y2 = dv_pow_d(y, 2.0);
    t0 = dv_add(x2, xy);
    f = dv_add(t0, y2);
    vars[0] = x;
    vars[1] = y;

    ASSERT_EQ_INT(dv_eval_derivatives(f, 2u, vars, &value, grads), 0);
    expect = num_create_from_string("7");
    ASSERT_TRUE(num_eq(value, expect));
    num_destroy(&expect);
    expect = num_create_from_string("4");
    ASSERT_TRUE(num_eq(grads[0], expect));
    num_destroy(&expect);
    expect = num_create_from_string("5");
    ASSERT_TRUE(num_eq(grads[1], expect));
    num_destroy(&expect);
    ASSERT_EQ_INT((int)num_get_prec_bits(value), 384);
    ASSERT_EQ_INT((int)num_get_prec_bits(grads[0]), 384);
    ASSERT_EQ_INT((int)num_get_prec_bits(grads[1]), 384);

    num_destroy(&grads[1]);
    num_destroy(&grads[0]);
    num_destroy(&value);
    dv_free(f); dv_free(t0); dv_free(y2); dv_free(xy); dv_free(x2); dv_free(y); dv_free(x);
    ASSERT_EQ_INT(num_set_default_prec_bits(old_prec_bits), 0);
}

static void test_reverse_gradient_complex_number_t(void)
{
    number_t z0 = num_create_from_string("1 + 2i");
    dval_t *z = dv_new_named_var_num(z0, "z");
    dval_t *exp_z = dv_exp(z);
    dval_t *z2 = dv_pow_d(z, 2.0);
    dval_t *f = dv_add(exp_z, z2);
    dval_t *df_dz = dv_create_deriv(f, z);
    const dval_t *vars[1] = { z };
    number_t value;
    number_t grad;
    number_t expect_value;
    number_t expect_grad;

    ASSERT_EQ_INT(dv_eval_derivatives(f, 1u, vars, &value, &grad), 0);
    expect_value = dv_eval_num(f);
    expect_grad = dv_eval_num(df_dz);

    ASSERT_TRUE(num_eq(value, expect_value) ||
                number_close_for_qfloat_precision(value, expect_value));
    ASSERT_TRUE(num_eq(grad, expect_grad) ||
                number_close_for_qfloat_precision(grad, expect_grad));
    ASSERT_TRUE(!num_is_real(value));
    ASSERT_TRUE(!num_is_real(grad));

    num_destroy(&expect_grad);
    num_destroy(&expect_value);
    num_destroy(&grad);
    num_destroy(&value);
    dv_free(df_dz);
    dv_free(f);
    dv_free(z2);
    dv_free(exp_z);
    dv_free(z);
    num_destroy(&z0);
}

void test_reverse_mode(void)
{
    RUN_SUBTEST(test_reverse_gradient_polynomial);
    RUN_SUBTEST(test_reverse_gradient_shared_subexpression);
    RUN_SUBTEST(test_reverse_matches_forward_composite);
    RUN_SUBTEST(test_reverse_gradient_missing_variable);
    RUN_SUBTEST(test_reverse_gradient_polynomial_num);
    RUN_SUBTEST(test_reverse_gradient_complex_number_t);
}
