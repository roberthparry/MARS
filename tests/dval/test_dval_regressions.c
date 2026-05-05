#include "test_dval.h"

static void test_cmp_qfloat_precision(void)
{
    dval_t *a = dv_new_const(qf_from_string("1.00000000000000000001"));
    dval_t *b = dv_new_const(qf_from_string("1.00000000000000000002"));
    int cmp = dv_cmp(a, b);

    if (cmp < 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " dv_cmp respects qfloat precision\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET
               " dv_cmp lost qfloat precision %s:%d:1 (got %d, expected < 0)\n",
               __FILE__, __LINE__, cmp);
        TEST_FAIL();
    }

    dv_free(b);
    dv_free(a);
}

static void test_get_val_updates_after_set(void)
{
    dval_t *x = dv_new_var_d(1.0);
    dval_t *f = dv_add_d(x, 2.0);

    check_q_at(__FILE__, __LINE__, 1, "dv_get_val initial", dv_get_val_qf(f), qf_from_double(3.0));
    dv_set_val_d(x, 5.0);
    check_q_at(__FILE__, __LINE__, 1, "dv_get_val after set", dv_get_val_qf(f), qf_from_double(7.0));

    dv_free(f);
    dv_free(x);
}

static void test_simplify_inverse_unary_pairs(void)
{
    dval_t *x = dv_new_named_var_d(3.0, "x");
    dval_t *log_x = dv_log(x);
    dval_t *exp_log_x = dv_exp(log_x);
    dval_t *exp_x = dv_exp(x);
    dval_t *log_exp_x = dv_log(exp_x);
    char *exp_log_s = dv_to_string(exp_log_x, style_EXPRESSION);
    char *log_exp_s = dv_to_string(log_exp_x, style_EXPRESSION);
    const char *expect = "{ x | x = 3 }";

    check_q_at(__FILE__, __LINE__, 1, "exp(log(x)) eval", dv_eval_qf(exp_log_x), qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "log(exp(x)) eval", dv_eval_qf(log_exp_x), qf_from_double(3.0));

    if (str_eq(exp_log_s, expect))
        to_string_pass("exp(log(x)) simplification (EXPR)", exp_log_s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "exp(log(x)) simplification (EXPR)", exp_log_s, expect);

    if (str_eq(log_exp_s, expect))
        to_string_pass("log(exp(x)) simplification (EXPR)", log_exp_s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "log(exp(x)) simplification (EXPR)", log_exp_s, expect);

    free(log_exp_s);
    free(exp_log_s);
    dv_free(log_exp_x);
    dv_free(exp_x);
    dv_free(exp_log_x);
    dv_free(log_x);
    dv_free(x);
}

void test_runtime_regressions(void)
{
    RUN_SUBTEST(test_cmp_qfloat_precision);
    RUN_SUBTEST(test_get_val_updates_after_set);
    RUN_SUBTEST(test_simplify_inverse_unary_pairs);
}

/* ------------------------------------------------------------------------- */
/* Reverse mode                                                              */
/* ------------------------------------------------------------------------- */
