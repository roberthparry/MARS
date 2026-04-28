#include "test_dval.h"

void test_add(void)
{
    dval_t *x0 = dv_new_var_d(2.0);
    dval_t *x1 = dv_new_var_d(3.0);
    dval_t *f = dv_add(x0, x1);

    check_q_at(__FILE__, __LINE__, 1, "2+3", dv_eval_qf(f), qf_from_double(5));
    print_expr_of(f);

    dv_free(f);
    dv_free(x0);
    dv_free(x1);
}

void test_sub(void)
{
    dval_t *x0 = dv_new_var_d(10);
    dval_t *c0  = dv_new_const_d(4);
    dval_t *f   = dv_sub(x0, c0);

    check_q_at(__FILE__, __LINE__, 1, "10-4", dv_eval_qf(f), qf_from_double(6));
    print_expr_of(f);

    dv_free(f);
    dv_free(c0);
    dv_free(x0);
}

void test_mul(void)
{
    dval_t *c7 = dv_new_var_d(7);
    dval_t *c6 = dv_new_const_d(6);
    dval_t *f  = dv_mul(c7, c6);

    check_q_at(__FILE__, __LINE__, 1, "7*6", dv_eval_qf(f), qf_from_double(42));
    print_expr_of(f);

    dv_free(f);
    dv_free(c6);
    dv_free(c7);
}

void test_div(void)
{
    dval_t *c7  = dv_new_var_d(7);
    dval_t *c22 = dv_new_const_d(22);
    dval_t *f   = dv_div(c22, c7);

    check_q_at(__FILE__, __LINE__, 1, "22/7", dv_eval_qf(f), qf_div(qf_from_double(22), qf_from_double(7)));
    print_expr_of(f);

    dv_free(f);
    dv_free(c22);
    dv_free(c7);
}

void test_mixed(void)
{
    dval_t *two   = dv_new_var_d(2);
    dval_t *three = dv_new_var_d(3);
    dval_t *ten   = dv_new_var_d(10);
    dval_t *four  = dv_new_var_d(4);

    dval_t *add_2_3 = dv_add(two, three);
    dval_t *sub_10_4 = dv_sub(ten, four);
    dval_t *mul_add_sub = dv_mul(add_2_3, sub_10_4);
    dval_t *f = dv_div(mul_add_sub, two);

    check_q_at(__FILE__, __LINE__, 1, "mixed", dv_eval_qf(f), qf_from_double(15));
    print_expr_of(f);

    dv_free(f);
    dv_free(mul_add_sub);
    dv_free(sub_10_4);
    dv_free(add_2_3);
    dv_free(two);
    dv_free(three);
    dv_free(ten);
    dv_free(four);
}

/* ------------------------------------------------------------------------- */
/* _d variants                                                                */
/* ------------------------------------------------------------------------- */

void test_add_d(void)
{
    dval_t *ten = dv_new_const_d(10);
    dval_t *f   = dv_add_d(ten, 2.5);

    check_q_at(__FILE__, __LINE__, 1, "10+2.5", dv_eval_qf(f), qf_from_double(12.5));
    print_expr_of(f);

    dv_free(f);
    dv_free(ten);
}

void test_mul_d(void)
{
    dval_t *three = dv_new_const_d(3);
    dval_t *f     = dv_mul_d(three, 4);

    check_q_at(__FILE__, __LINE__, 1, "3*4", dv_eval_qf(f), qf_from_double(12));
    print_expr_of(f);

    dv_free(f);
    dv_free(three);
}

void test_div_d(void)
{
    dval_t *nine = dv_new_const_d(9);
    dval_t *f    = dv_div_d(nine, 3);

    check_q_at(__FILE__, __LINE__, 1, "9/3", dv_eval_qf(f), qf_from_double(3));
    print_expr_of(f);

    dv_free(f);
    dv_free(nine);
}

void test_arithmetic(void)
{
    RUN_TEST(test_add, __func__);
    RUN_TEST(test_sub, __func__);
    RUN_TEST(test_mul, __func__);
    RUN_TEST(test_div, __func__);
    RUN_TEST(test_mixed, __func__);
}

void test_d_variants(void)
{
    RUN_TEST(test_add_d, __func__);
    RUN_TEST(test_mul_d, __func__);
    RUN_TEST(test_div_d, __func__);
}

/* ------------------------------------------------------------------------- */
/* Mathematical function tests                                                */
/* ------------------------------------------------------------------------- */
