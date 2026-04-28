#include "test_dval.h"

static void test_reverse_gradient_polynomial(void)
{
    dval_t *x  = dv_new_named_var_d(1.0, "x");
    dval_t *y  = dv_new_named_var_d(2.0, "y");
    dval_t *x2 = dv_pow_d(x, 2.0);
    dval_t *xy = dv_mul(x, y);
    dval_t *y2 = dv_pow_d(y, 2.0);
    dval_t *t0 = dv_add(x2, xy);
    dval_t *f  = dv_add(t0, y2);
    dval_t *vars[2] = { x, y };
    qfloat_t value;
    qfloat_t grads[2];

    if (dv_eval_derivatives(f, 2, vars, &value, grads) != 0) {
        printf(C_BOLD C_RED "FAIL" C_RESET " reverse polynomial gradient returned error\n");
        TEST_FAIL();
    }

    check_q_at(__FILE__, __LINE__, 1, "reverse value polynomial", value, qf_from_double(7.0));
    check_q_at(__FILE__, __LINE__, 1, "reverse df/dx polynomial", grads[0], qf_from_double(4.0));
    check_q_at(__FILE__, __LINE__, 1, "reverse df/dy polynomial", grads[1], qf_from_double(5.0));

    dv_free(f); dv_free(t0); dv_free(y2); dv_free(xy); dv_free(x2); dv_free(y); dv_free(x);
}

static void test_reverse_gradient_shared_subexpression(void)
{
    dval_t *x = dv_new_named_var_d(1.5, "x");
    dval_t *y = dv_new_named_var_d(-0.5, "y");
    dval_t *s = dv_add(x, y);
    dval_t *f = dv_mul(s, s);
    dval_t *vars[2] = { x, y };
    qfloat_t value;
    qfloat_t grads[2];

    if (dv_eval_derivatives(f, 2, vars, &value, grads) != 0) {
        printf(C_BOLD C_RED "FAIL" C_RESET " reverse shared-subexpression gradient returned error\n");
        TEST_FAIL();
    }

    check_q_at(__FILE__, __LINE__, 1, "reverse value shared", value, qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "reverse df/dx shared", grads[0], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "reverse df/dy shared", grads[1], qf_from_double(2.0));

    dv_free(f); dv_free(s); dv_free(y); dv_free(x);
}

static void test_reverse_matches_forward_composite(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *xy = dv_mul(x, y);
    dval_t *sin_xy = dv_sin(xy);
    dval_t *exp_term = dv_exp(sin_xy);
    dval_t *log_y = dv_log(y);
    dval_t *x_log_y = dv_mul(x, log_y);
    dval_t *f = dv_add(exp_term, x_log_y);
    dval_t *vars[2] = { x, y };
    qfloat_t grads[2];
    qfloat_t value;
    dval_t *df_dx = dv_create_deriv(f, x);
    dval_t *df_dy = dv_create_deriv(f, y);

    if (dv_eval_derivatives(f, 2, vars, &value, grads) != 0) {
        printf(C_BOLD C_RED "FAIL" C_RESET " reverse composite gradient returned error\n");
        TEST_FAIL();
    }

    check_q_at(__FILE__, __LINE__, 1, "reverse value composite", value, dv_eval_qf(f));
    check_q_at(__FILE__, __LINE__, 1, "reverse matches forward df/dx", grads[0], dv_eval_qf(df_dx));
    check_q_at(__FILE__, __LINE__, 1, "reverse matches forward df/dy", grads[1], dv_eval_qf(df_dy));

    dv_free(df_dy); dv_free(df_dx);
    dv_free(f); dv_free(x_log_y); dv_free(log_y); dv_free(exp_term); dv_free(sin_xy); dv_free(xy);
    dv_free(y); dv_free(x);
}

static void test_reverse_gradient_missing_variable(void)
{
    dval_t *x = dv_new_named_var_d(3.0, "x");
    dval_t *z = dv_new_named_var_d(9.0, "z");
    dval_t *f = dv_mul_d(x, 4.0);
    dval_t *vars[2] = { x, z };
    qfloat_t grads[2];

    if (dv_eval_derivatives(f, 2, vars, NULL, grads) != 0) {
        printf(C_BOLD C_RED "FAIL" C_RESET " reverse missing-variable gradient returned error\n");
        TEST_FAIL();
    }

    check_q_at(__FILE__, __LINE__, 1, "reverse df/dx present", grads[0], qf_from_double(4.0));
    check_q_at(__FILE__, __LINE__, 1, "reverse df/dz missing", grads[1], QF_ZERO);

    dv_free(f); dv_free(z); dv_free(x);
}

void test_reverse_mode(void)
{
    RUN_TEST(test_reverse_gradient_polynomial, __func__);
    RUN_TEST(test_reverse_gradient_shared_subexpression, __func__);
    RUN_TEST(test_reverse_matches_forward_composite, __func__);
    RUN_TEST(test_reverse_gradient_missing_variable, __func__);
}
