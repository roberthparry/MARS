#include "test_expr.h"

void test_add(void)
{
    expr_t *x0 = test_expr_new_var_d(2.0);
    expr_t *x1 = test_expr_new_var_d(3.0);
    expr_t *f = expr_add(x0, x1);

    check_q_at(__FILE__, __LINE__, 1, "2+3", expr_eval_qf(f), qf_from_double(5));
    print_expr_of(f);

    expr_free(f);
    expr_free(x0);
    expr_free(x1);
}

void test_sub(void)
{
    expr_t *x0 = test_expr_new_var_d(10);
    expr_t *c0  = test_expr_new_const_d(4);
    expr_t *f   = expr_sub(x0, c0);

    check_q_at(__FILE__, __LINE__, 1, "10-4", expr_eval_qf(f), qf_from_double(6));
    print_expr_of(f);

    expr_free(f);
    expr_free(c0);
    expr_free(x0);
}

void test_mul(void)
{
    expr_t *c7 = test_expr_new_var_d(7);
    expr_t *c6 = test_expr_new_const_d(6);
    expr_t *f  = expr_mul(c7, c6);

    check_q_at(__FILE__, __LINE__, 1, "7*6", expr_eval_qf(f), qf_from_double(42));
    print_expr_of(f);

    expr_free(f);
    expr_free(c6);
    expr_free(c7);
}

void test_div(void)
{
    expr_t *c7  = test_expr_new_var_d(7);
    expr_t *c22 = test_expr_new_const_d(22);
    expr_t *f   = expr_div(c22, c7);

    check_q_at(__FILE__, __LINE__, 1, "22/7", expr_eval_qf(f), qf_div(qf_from_double(22), qf_from_double(7)));
    print_expr_of(f);

    expr_free(f);
    expr_free(c22);
    expr_free(c7);
}

void test_mixed(void)
{
    expr_t *two   = test_expr_new_var_d(2);
    expr_t *three = test_expr_new_var_d(3);
    expr_t *ten   = test_expr_new_var_d(10);
    expr_t *four  = test_expr_new_var_d(4);

    expr_t *add_2_3 = expr_add(two, three);
    expr_t *sub_10_4 = expr_sub(ten, four);
    expr_t *mul_add_sub = expr_mul(add_2_3, sub_10_4);
    expr_t *f = expr_div(mul_add_sub, two);

    check_q_at(__FILE__, __LINE__, 1, "mixed", expr_eval_qf(f), qf_from_double(15));
    print_expr_of(f);

    expr_free(f);
    expr_free(mul_add_sub);
    expr_free(sub_10_4);
    expr_free(add_2_3);
    expr_free(two);
    expr_free(three);
    expr_free(ten);
    expr_free(four);
}

/* ------------------------------------------------------------------------- */
/* Scalar number_t helpers                                                   */
/* ------------------------------------------------------------------------- */

void test_add_num(void)
{
    number_t scalar = num_create_from_double(2.5);
    expr_t *ten = test_expr_new_const_d(10);
    expr_t *f   = expr_add_num(ten, &scalar);

    check_q_at(__FILE__, __LINE__, 1, "10+2.5", expr_eval_qf(f), qf_from_double(12.5));
    print_expr_of(f);

    num_destroy(&scalar);
    expr_free(f);
    expr_free(ten);
}

void test_mul_num(void)
{
    number_t scalar = num_create_from_long(4);
    expr_t *three = test_expr_new_const_d(3);
    expr_t *f     = expr_mul_num(three, &scalar);

    check_q_at(__FILE__, __LINE__, 1, "3*4", expr_eval_qf(f), qf_from_double(12));
    print_expr_of(f);

    num_destroy(&scalar);
    expr_free(f);
    expr_free(three);
}

void test_div_num(void)
{
    number_t scalar = num_create_from_long(3);
    expr_t *nine = test_expr_new_const_d(9);
    expr_t *f    = expr_div_num(nine, &scalar);

    check_q_at(__FILE__, __LINE__, 1, "9/3", expr_eval_qf(f), qf_from_double(3));
    print_expr_of(f);

    num_destroy(&scalar);
    expr_free(f);
    expr_free(nine);
}

void test_arithmetic(void)
{
    TEST_RUN_SUBTEST(test_add, NULL);
    TEST_RUN_SUBTEST(test_sub, NULL);
    TEST_RUN_SUBTEST(test_mul, NULL);
    TEST_RUN_SUBTEST(test_div, NULL);
    TEST_RUN_SUBTEST(test_mixed, NULL);
}

void test_d_variants(void)
{
    TEST_RUN_SUBTEST(test_add_num, NULL);
    TEST_RUN_SUBTEST(test_mul_num, NULL);
    TEST_RUN_SUBTEST(test_div_num, NULL);
}

/* ------------------------------------------------------------------------- */
/* Mathematical function tests                                                */
/* ------------------------------------------------------------------------- */
