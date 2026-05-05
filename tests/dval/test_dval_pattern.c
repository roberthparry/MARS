#include "test_dval.h"
#include "internal/dval_expr_match.h"
#include "internal/dval_pattern.h"

static void test_match_affine_families(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];

    dval_t *two_x = dv_mul_d(x, 2.0);
    dval_t *y_over_four = dv_div_d(y, 4.0);
    dval_t *linear = dv_sub(two_x, y_over_four);
    dval_t *affine = dv_add_d(linear, 3.0);
    dval_t *exp_affine = dv_exp(affine);
    dval_t *neg_two_x_inner = dv_mul_d(x, 2.0);
    dval_t *neg_two_x = dv_neg(neg_two_x_inner);
    dval_t *sin_arg = dv_add_d(neg_two_x, 3.0);
    dval_t *sin_affine = dv_sin(sin_arg);

    ASSERT_TRUE(dv_match_unary_affine_kind(exp_affine, DV_PATTERN_UNARY_EXP,
                                           2, vars, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "exp affine constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "exp affine coeff x", coeffs[0], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "exp affine coeff y", coeffs[1], qf_from_double(-0.25));

    ASSERT_TRUE(dv_match_unary_affine_kind(sin_affine, DV_PATTERN_UNARY_SIN,
                                           2, vars, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "sin affine constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "sin affine coeff x", coeffs[0], qf_from_double(-2.0));
    check_q_at(__FILE__, __LINE__, 1, "sin affine coeff y", coeffs[1], QF_ZERO);

    dv_free(sin_affine);
    dv_free(sin_arg);
    dv_free(neg_two_x);
    dv_free(neg_two_x_inner);
    dv_free(exp_affine);
    dv_free(affine);
    dv_free(linear);
    dv_free(y_over_four);
    dv_free(two_x);
    dv_free(y);
    dv_free(x);
}

static void test_generic_unary_affine_matchers(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    dval_t *two_y = dv_mul_d(y, 2.0);
    dval_t *sum_xy = dv_add(x, two_y);
    dval_t *affine = dv_add_d(sum_xy, 3.0);
    dval_t *sin_affine = dv_sin(affine);

    ASSERT_TRUE(dv_match_unary_affine_kind(sin_affine, DV_PATTERN_UNARY_SIN,
                                           2, vars, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "generic unary constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "generic unary coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "generic unary coeff y", coeffs[1], qf_from_double(2.0));
    ASSERT_TRUE(!dv_match_unary_affine_kind(sin_affine, (dv_pattern_unary_affine_kind_t)-1,
                                            2, vars, &constant, coeffs));

    dv_free(sin_affine);
    dv_free(affine);
    dv_free(sum_xy);
    dv_free(two_y);
    dv_free(y);
    dv_free(x);
}

static void test_pattern_rejections(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *c = dv_new_named_const_d(5.0, "c");
    dval_t *vars[] = { x, y };
    qfloat_t value;
    qfloat_t constant;
    qfloat_t coeffs[2];
    qfloat_t scale;
    const dval_t *base = NULL;

    dval_t *xy = dv_mul(x, y);
    dval_t *exp_xy = dv_exp(xy);
    dval_t *x_over_y = dv_div(x, y);

    ASSERT_TRUE(!dv_match_const_value(c, &value));
    ASSERT_TRUE(!dv_match_unary_affine_kind(exp_xy, DV_PATTERN_UNARY_EXP,
                                            2, vars, &constant, coeffs));
    ASSERT_TRUE(!dv_match_scaled_expr(x_over_y, &scale, &base));

    dv_free(x_over_y);
    dv_free(exp_xy);
    dv_free(xy);
    dv_free(c);
    dv_free(y);
    dv_free(x);
}

static void test_scaled_expr_and_var_usage(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *z = dv_new_named_var_d(3.0, "z");
    dval_t *vars[] = { x, y, z };
    qfloat_t scale;
    const dval_t *base = NULL;
    bool used[3];

    dval_t *scaled_inner = dv_mul_d(x, 6.0);
    dval_t *scaled_neg = dv_neg(scaled_inner);
    dval_t *scaled = dv_div_d(scaled_neg, 3.0);
    dval_t *usage_scaled = dv_mul_d(x, 2.0);
    dval_t *usage_exp = dv_exp(y);
    dval_t *usage_expr = dv_add(usage_scaled, usage_exp);

    ASSERT_TRUE(dv_match_scaled_expr(scaled, &scale, &base));
    check_q_at(__FILE__, __LINE__, 1, "scaled expr factor", scale, qf_from_double(-2.0));
    ASSERT_TRUE(base == x);

    ASSERT_TRUE(dv_collect_var_usage(usage_expr, 3, vars, used));
    ASSERT_TRUE(used[0]);
    ASSERT_TRUE(used[1]);
    ASSERT_TRUE(!used[2]);

    dv_free(usage_expr);
    dv_free(usage_exp);
    dv_free(usage_scaled);
    dv_free(scaled);
    dv_free(scaled_neg);
    dv_free(scaled_inner);
    dv_free(z);
    dv_free(y);
    dv_free(x);
}

static void test_substitute_and_powd(void)
{
    dval_t *x = dv_new_named_var_d(2.0, "x");
    dval_t *y = dv_new_named_var_d(3.0, "y");
    dval_t *c = dv_new_named_const_d(2.0, "c");

    dval_t *x2 = dv_pow_d(x, 2.0);
    dval_t *expr = dv_add(x2, c);
    dval_t *replacement = dv_add_d(y, 1.0);
    dval_t *sub = dv_substitute(expr, x, replacement);
    char *s = dv_to_string(sub, style_EXPRESSION);

    ASSERT_NOT_NULL(sub);
    ASSERT_NOT_NULL(s);
    check_q_at(__FILE__, __LINE__, 1, "substitute initial eval", dv_eval_qf(sub), qf_from_double(18.0));

    dv_set_val_d(x, 10.0);
    check_q_at(__FILE__, __LINE__, 1, "substitute ignores old x", dv_eval_qf(sub), qf_from_double(18.0));

    dv_set_val_d(y, 4.0);
    check_q_at(__FILE__, __LINE__, 1, "substitute tracks replacement y", dv_eval_qf(sub), qf_from_double(27.0));

    ASSERT_TRUE(strstr(s, "y") != NULL);
    ASSERT_TRUE(strstr(s, "c") != NULL);

    free(s);
    dv_free(sub);
    dv_free(replacement);
    dv_free(expr);
    dv_free(x2);
    dv_free(c);
    dv_free(y);
    dv_free(x);
}

static void test_square_affine_matchers(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *vars[] = { x, y };
    qfloat_t poly[5];
    qfloat_t constant;
    qfloat_t coeffs[2];
    dval_t *two_x = dv_mul_d(x, 2.0);
    dval_t *y_over_four = dv_div_d(y, 4.0);
    dval_t *linear = dv_sub(two_x, y_over_four);
    dval_t *affine = dv_add_d(linear, 3.0);
    dval_t *pow_square = dv_pow_d(affine, 2.0);
    dval_t *mul_square = dv_mul(affine, affine);

    ASSERT_TRUE(dv_match_affine_poly_deg4(pow_square, 2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "square affine pow constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "square affine pow coeff x", coeffs[0], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "square affine pow coeff y", coeffs[1], qf_from_double(-0.25));
    check_q_at(__FILE__, __LINE__, 1, "square affine pow poly a^2", poly[2], QF_ONE);

    ASSERT_TRUE(dv_match_affine_poly_deg4(mul_square, 2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "square affine mul constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "square affine mul coeff x", coeffs[0], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "square affine mul coeff y", coeffs[1], qf_from_double(-0.25));

    dv_free(mul_square);
    dv_free(pow_square);
    dv_free(affine);
    dv_free(linear);
    dv_free(y_over_four);
    dv_free(two_x);
    dv_free(y);
    dv_free(x);
}

static void test_cube_affine_matchers(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *vars[] = { x, y };
    qfloat_t poly[5];
    qfloat_t constant;
    qfloat_t coeffs[2];
    dval_t *two_x = dv_mul_d(x, 2.0);
    dval_t *y_over_four = dv_div_d(y, 4.0);
    dval_t *linear = dv_sub(two_x, y_over_four);
    dval_t *affine = dv_add_d(linear, 3.0);
    dval_t *pow_cube = dv_pow_d(affine, 3.0);
    dval_t *mul_cube_square = dv_mul(affine, affine);
    dval_t *mul_cube = dv_mul(mul_cube_square, affine);

    ASSERT_TRUE(dv_match_affine_poly_deg4(pow_cube, 2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "cube affine pow constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "cube affine pow coeff x", coeffs[0], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "cube affine pow coeff y", coeffs[1], qf_from_double(-0.25));
    check_q_at(__FILE__, __LINE__, 1, "cube affine pow poly a^3", poly[3], QF_ONE);

    ASSERT_TRUE(dv_match_affine_poly_deg4(mul_cube, 2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "cube affine mul constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "cube affine mul coeff x", coeffs[0], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "cube affine mul coeff y", coeffs[1], qf_from_double(-0.25));

    dv_free(mul_cube);
    dv_free(mul_cube_square);
    dv_free(pow_cube);
    dv_free(affine);
    dv_free(linear);
    dv_free(y_over_four);
    dv_free(two_x);
    dv_free(y);
    dv_free(x);
}

static void test_quartic_affine_matchers(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *vars[] = { x, y };
    qfloat_t poly[5];
    qfloat_t constant;
    qfloat_t coeffs[2];
    dval_t *two_x = dv_mul_d(x, 2.0);
    dval_t *y_over_four = dv_div_d(y, 4.0);
    dval_t *linear = dv_sub(two_x, y_over_four);
    dval_t *affine = dv_add_d(linear, 3.0);
    dval_t *pow_quartic = dv_pow_d(affine, 4.0);
    dval_t *mul_quartic_lhs = dv_mul(affine, affine);
    dval_t *mul_quartic_rhs = dv_mul(affine, affine);
    dval_t *mul_quartic = dv_mul(mul_quartic_lhs, mul_quartic_rhs);

    ASSERT_TRUE(dv_match_affine_poly_deg4(pow_quartic, 2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "quartic affine pow constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "quartic affine pow coeff x", coeffs[0], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "quartic affine pow coeff y", coeffs[1], qf_from_double(-0.25));
    check_q_at(__FILE__, __LINE__, 1, "quartic affine pow poly a^4", poly[4], QF_ONE);

    ASSERT_TRUE(dv_match_affine_poly_deg4(mul_quartic, 2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "quartic affine mul constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "quartic affine mul coeff x", coeffs[0], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "quartic affine mul coeff y", coeffs[1], qf_from_double(-0.25));

    dv_free(mul_quartic);
    dv_free(mul_quartic_rhs);
    dv_free(mul_quartic_lhs);
    dv_free(pow_quartic);
    dv_free(affine);
    dv_free(linear);
    dv_free(y_over_four);
    dv_free(two_x);
    dv_free(y);
    dv_free(x);
}

static void test_affine_times_exp_affine_matcher(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    dval_t *two_y = dv_mul_d(y, 2.0);
    dval_t *sum_xy = dv_add(x, two_y);
    dval_t *affine = dv_add_d(sum_xy, 3.0);
    dval_t *exp_affine = dv_exp(affine);
    dval_t *expr = dv_mul(affine, exp_affine);
    dval_t *mismatch_affine = dv_add_d(x, 1.0);
    dval_t *mismatch = dv_mul(mismatch_affine, exp_affine);

    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(expr, DV_PATTERN_UNARY_EXP,
                                                                  2, vars, (qfloat_t[5]){0},
                                                                  &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "affexp constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "affexp coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "affexp coeff y", coeffs[1], qf_from_double(2.0));
    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_EXP,
                                                                   2, vars, (qfloat_t[5]){0},
                                                                   &constant, coeffs));

    dv_free(mismatch);
    dv_free(mismatch_affine);
    dv_free(expr);
    dv_free(exp_affine);
    dv_free(affine);
    dv_free(sum_xy);
    dv_free(two_y);
    dv_free(y);
    dv_free(x);
}

static void test_square_affine_times_exp_affine_matcher(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    qfloat_t poly[5];
    dval_t *two_y = dv_mul_d(y, 2.0);
    dval_t *sum_xy = dv_add(x, two_y);
    dval_t *affine = dv_add_d(sum_xy, 3.0);
    dval_t *square = dv_mul(affine, affine);
    dval_t *pow_square = dv_pow_d(affine, 2.0);
    dval_t *exp_affine = dv_exp(affine);
    dval_t *expr = dv_mul(square, exp_affine);
    dval_t *expr_pow = dv_mul(exp_affine, pow_square);
    dval_t *mismatch_lhs = dv_add_d(x, 1.0);
    dval_t *mismatch_rhs = dv_add_d(x, 1.0);
    dval_t *mismatch_square = dv_mul(mismatch_lhs, mismatch_rhs);
    dval_t *mismatch = dv_mul(mismatch_square, exp_affine);

    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(expr, DV_PATTERN_UNARY_EXP,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "sqaffexp constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffexp coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffexp coeff y", coeffs[1], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffexp poly a^2", poly[2], QF_ONE);

    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(expr_pow, DV_PATTERN_UNARY_EXP,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_EXP,
                                                                   2, vars, poly, &constant, coeffs));

    dv_free(mismatch);
    dv_free(mismatch_square);
    dv_free(mismatch_rhs);
    dv_free(mismatch_lhs);
    dv_free(expr_pow);
    dv_free(expr);
    dv_free(exp_affine);
    dv_free(pow_square);
    dv_free(square);
    dv_free(affine);
    dv_free(sum_xy);
    dv_free(two_y);
    dv_free(y);
    dv_free(x);
}

static void test_square_affine_times_trig_affine_matchers(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    qfloat_t poly[5];
    dval_t *two_y = dv_mul_d(y, 2.0);
    dval_t *sum_xy = dv_add(x, two_y);
    dval_t *affine = dv_add_d(sum_xy, 3.0);
    dval_t *square = dv_mul(affine, affine);
    dval_t *pow_square = dv_pow_d(affine, 2.0);
    dval_t *sin_affine = dv_sin(affine);
    dval_t *cos_affine = dv_cos(affine);
    dval_t *sin_expr = dv_mul(square, sin_affine);
    dval_t *cos_expr = dv_mul(cos_affine, pow_square);
    dval_t *mismatch_lhs = dv_add_d(x, 1.0);
    dval_t *mismatch_rhs = dv_add_d(x, 1.0);
    dval_t *mismatch_square = dv_mul(mismatch_lhs, mismatch_rhs);
    dval_t *mismatch = dv_mul(mismatch_square, sin_affine);

    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(sin_expr, DV_PATTERN_UNARY_SIN,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "sqaffsin constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffsin coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffsin coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(cos_expr, DV_PATTERN_UNARY_COS,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "sqaffcos constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffcos coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffcos coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_SIN,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_COS,
                                                                   2, vars, poly, &constant, coeffs));

    dv_free(mismatch);
    dv_free(mismatch_square);
    dv_free(mismatch_rhs);
    dv_free(mismatch_lhs);
    dv_free(cos_expr);
    dv_free(sin_expr);
    dv_free(cos_affine);
    dv_free(sin_affine);
    dv_free(pow_square);
    dv_free(square);
    dv_free(affine);
    dv_free(sum_xy);
    dv_free(two_y);
    dv_free(y);
    dv_free(x);
}

static void test_affine_times_trig_affine_matchers(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    qfloat_t poly[5];
    dval_t *two_y = dv_mul_d(y, 2.0);
    dval_t *sum_xy = dv_add(x, two_y);
    dval_t *affine = dv_add_d(sum_xy, 3.0);
    dval_t *sin_affine = dv_sin(affine);
    dval_t *cos_affine = dv_cos(affine);
    dval_t *sin_expr = dv_mul(affine, sin_affine);
    dval_t *cos_expr = dv_mul(cos_affine, affine);
    dval_t *mismatch_affine = dv_add_d(x, 1.0);
    dval_t *mismatch = dv_mul(mismatch_affine, sin_affine);

    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(sin_expr, DV_PATTERN_UNARY_SIN,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "affsin constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "affsin coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "affsin coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(cos_expr, DV_PATTERN_UNARY_COS,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "affcos constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "affcos coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "affcos coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_SIN,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_COS,
                                                                   2, vars, poly, &constant, coeffs));

    dv_free(mismatch);
    dv_free(mismatch_affine);
    dv_free(cos_expr);
    dv_free(sin_expr);
    dv_free(cos_affine);
    dv_free(sin_affine);
    dv_free(affine);
    dv_free(sum_xy);
    dv_free(two_y);
    dv_free(y);
    dv_free(x);
}

static void test_affine_times_hyperbolic_affine_matchers(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    qfloat_t poly[5];
    dval_t *two_y = dv_mul_d(y, 2.0);
    dval_t *sum_xy = dv_add(x, two_y);
    dval_t *affine = dv_add_d(sum_xy, 3.0);
    dval_t *sinh_affine = dv_sinh(affine);
    dval_t *cosh_affine = dv_cosh(affine);
    dval_t *sinh_expr = dv_mul(affine, sinh_affine);
    dval_t *cosh_expr = dv_mul(cosh_affine, affine);
    dval_t *mismatch_affine = dv_add_d(x, 1.0);
    dval_t *mismatch = dv_mul(mismatch_affine, sinh_affine);

    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(sinh_expr, DV_PATTERN_UNARY_SINH,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "affsinh constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "affsinh coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "affsinh coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(cosh_expr, DV_PATTERN_UNARY_COSH,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "affcosh constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "affcosh coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "affcosh coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_SINH,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_COSH,
                                                                   2, vars, poly, &constant, coeffs));

    dv_free(mismatch);
    dv_free(mismatch_affine);
    dv_free(cosh_expr);
    dv_free(sinh_expr);
    dv_free(cosh_affine);
    dv_free(sinh_affine);
    dv_free(affine);
    dv_free(sum_xy);
    dv_free(two_y);
    dv_free(y);
    dv_free(x);
}

static void test_square_affine_times_hyperbolic_affine_matchers(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    qfloat_t poly[5];
    dval_t *two_y = dv_mul_d(y, 2.0);
    dval_t *sum_xy = dv_add(x, two_y);
    dval_t *affine = dv_add_d(sum_xy, 3.0);
    dval_t *square = dv_mul(affine, affine);
    dval_t *pow_square = dv_pow_d(affine, 2.0);
    dval_t *sinh_affine = dv_sinh(affine);
    dval_t *cosh_affine = dv_cosh(affine);
    dval_t *sinh_expr = dv_mul(square, sinh_affine);
    dval_t *cosh_expr = dv_mul(cosh_affine, pow_square);
    dval_t *mismatch_lhs = dv_add_d(x, 1.0);
    dval_t *mismatch_rhs = dv_add_d(x, 1.0);
    dval_t *mismatch_square = dv_mul(mismatch_lhs, mismatch_rhs);
    dval_t *mismatch = dv_mul(mismatch_square, sinh_affine);

    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(sinh_expr, DV_PATTERN_UNARY_SINH,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "sqaffsinh constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffsinh coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffsinh coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(cosh_expr, DV_PATTERN_UNARY_COSH,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "sqaffcosh constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffcosh coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffcosh coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_SINH,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_COSH,
                                                                   2, vars, poly, &constant, coeffs));

    dv_free(mismatch);
    dv_free(mismatch_square);
    dv_free(mismatch_rhs);
    dv_free(mismatch_lhs);
    dv_free(cosh_expr);
    dv_free(sinh_expr);
    dv_free(cosh_affine);
    dv_free(sinh_affine);
    dv_free(pow_square);
    dv_free(square);
    dv_free(affine);
    dv_free(sum_xy);
    dv_free(two_y);
    dv_free(y);
    dv_free(x);
}

static void test_cube_affine_times_unary_affine_matchers(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    qfloat_t poly[5];
    dval_t *two_y = dv_mul_d(y, 2.0);
    dval_t *sum_xy = dv_add(x, two_y);
    dval_t *affine = dv_add_d(sum_xy, 3.0);
    dval_t *cube_square = dv_mul(affine, affine);
    dval_t *cube = dv_mul(cube_square, affine);
    dval_t *pow_cube = dv_pow_d(affine, 3.0);
    dval_t *exp_affine = dv_exp(affine);
    dval_t *sin_affine = dv_sin(affine);
    dval_t *cos_affine = dv_cos(affine);
    dval_t *sinh_affine = dv_sinh(affine);
    dval_t *cosh_affine = dv_cosh(affine);
    dval_t *exp_expr = dv_mul(cube, exp_affine);
    dval_t *sin_expr = dv_mul(pow_cube, sin_affine);
    dval_t *cos_expr = dv_mul(cos_affine, cube);
    dval_t *sinh_expr = dv_mul(pow_cube, sinh_affine);
    dval_t *cosh_expr = dv_mul(cosh_affine, cube);
    dval_t *mismatch_a = dv_add_d(x, 1.0);
    dval_t *mismatch_b = dv_add_d(x, 1.0);
    dval_t *mismatch_c = dv_add_d(x, 1.0);
    dval_t *mismatch_square = dv_mul(mismatch_a, mismatch_b);
    dval_t *mismatch_cube = dv_mul(mismatch_square, mismatch_c);
    dval_t *mismatch = dv_mul(mismatch_cube, exp_affine);

    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(exp_expr, DV_PATTERN_UNARY_EXP,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "cubaffexp constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "cubaffexp coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "cubaffexp coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(sin_expr, DV_PATTERN_UNARY_SIN,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(cos_expr, DV_PATTERN_UNARY_COS,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(sinh_expr, DV_PATTERN_UNARY_SINH,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(cosh_expr, DV_PATTERN_UNARY_COSH,
                                                                  2, vars, poly, &constant, coeffs));

    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_EXP,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_SIN,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_COS,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_SINH,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_COSH,
                                                                   2, vars, poly, &constant, coeffs));

    dv_free(mismatch);
    dv_free(mismatch_cube);
    dv_free(mismatch_square);
    dv_free(mismatch_c);
    dv_free(mismatch_b);
    dv_free(mismatch_a);
    dv_free(cosh_expr);
    dv_free(sinh_expr);
    dv_free(cos_expr);
    dv_free(sin_expr);
    dv_free(exp_expr);
    dv_free(cosh_affine);
    dv_free(sinh_affine);
    dv_free(cos_affine);
    dv_free(sin_affine);
    dv_free(exp_affine);
    dv_free(pow_cube);
    dv_free(cube_square);
    dv_free(cube);
    dv_free(affine);
    dv_free(sum_xy);
    dv_free(two_y);
    dv_free(y);
    dv_free(x);
}

static void test_quartic_affine_times_unary_affine_matchers(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    qfloat_t poly[5];
    dval_t *two_y = dv_mul_d(y, 2.0);
    dval_t *sum_xy = dv_add(x, two_y);
    dval_t *affine = dv_add_d(sum_xy, 3.0);
    dval_t *quartic_lhs = dv_mul(affine, affine);
    dval_t *quartic_rhs = dv_mul(affine, affine);
    dval_t *quartic = dv_mul(quartic_lhs, quartic_rhs);
    dval_t *pow_quartic = dv_pow_d(affine, 4.0);
    dval_t *exp_affine = dv_exp(affine);
    dval_t *sin_affine = dv_sin(affine);
    dval_t *cos_affine = dv_cos(affine);
    dval_t *sinh_affine = dv_sinh(affine);
    dval_t *cosh_affine = dv_cosh(affine);
    dval_t *exp_expr = dv_mul(quartic, exp_affine);
    dval_t *sin_expr = dv_mul(pow_quartic, sin_affine);
    dval_t *cos_expr = dv_mul(cos_affine, quartic);
    dval_t *sinh_expr = dv_mul(pow_quartic, sinh_affine);
    dval_t *cosh_expr = dv_mul(cosh_affine, quartic);
    dval_t *mismatch_a = dv_add_d(x, 1.0);
    dval_t *mismatch_b = dv_add_d(x, 1.0);
    dval_t *mismatch_c = dv_add_d(x, 1.0);
    dval_t *mismatch_d = dv_add_d(x, 2.0);
    dval_t *mismatch_lhs = dv_mul(mismatch_a, mismatch_b);
    dval_t *mismatch_rhs = dv_mul(mismatch_c, mismatch_d);
    dval_t *mismatch_quartic = dv_mul(mismatch_lhs, mismatch_rhs);
    dval_t *mismatch = dv_mul(mismatch_quartic, exp_affine);

    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(exp_expr, DV_PATTERN_UNARY_EXP,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "qrtaffexp constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "qrtaffexp coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "qrtaffexp coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(sin_expr, DV_PATTERN_UNARY_SIN,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(cos_expr, DV_PATTERN_UNARY_COS,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(sinh_expr, DV_PATTERN_UNARY_SINH,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(cosh_expr, DV_PATTERN_UNARY_COSH,
                                                                  2, vars, poly, &constant, coeffs));

    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_EXP,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_SIN,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_COS,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_SINH,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_COSH,
                                                                   2, vars, poly, &constant, coeffs));

    dv_free(mismatch);
    dv_free(mismatch_quartic);
    dv_free(mismatch_rhs);
    dv_free(mismatch_lhs);
    dv_free(mismatch_d);
    dv_free(mismatch_c);
    dv_free(mismatch_b);
    dv_free(mismatch_a);
    dv_free(cosh_expr);
    dv_free(sinh_expr);
    dv_free(cos_expr);
    dv_free(sin_expr);
    dv_free(exp_expr);
    dv_free(cosh_affine);
    dv_free(sinh_affine);
    dv_free(cos_affine);
    dv_free(sin_affine);
    dv_free(exp_affine);
    dv_free(pow_quartic);
    dv_free(quartic_rhs);
    dv_free(quartic_lhs);
    dv_free(quartic);
    dv_free(affine);
    dv_free(sum_xy);
    dv_free(two_y);
    dv_free(y);
    dv_free(x);
}

static void test_affine_poly_deg4_times_unary_affine_matchers(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *vars[] = { x, y };
    qfloat_t poly[5];
    qfloat_t constant;
    qfloat_t coeffs[2];
    dval_t *two_y = dv_mul_d(y, 2.0);
    dval_t *sum_xy = dv_add(x, two_y);
    dval_t *affine = dv_add_d(sum_xy, 3.0);
    dval_t *square = dv_pow_d(affine, 2.0);
    dval_t *quartic = dv_pow_d(affine, 4.0);
    dval_t *exp_affine = dv_exp(affine);
    dval_t *sin_affine = dv_sin(affine);
    dval_t *cosh_affine = dv_cosh(affine);
    dval_t *quartic_scaled = dv_mul_d(quartic, 3.0);
    dval_t *quartic_plus_five = dv_add_d(quartic_scaled, 5.0);
    dval_t *square_scaled = dv_mul_d(square, -2.0);
    dval_t *square_plus_affine = dv_add(square_scaled, affine);
    dval_t *poly_expr = dv_add(quartic_plus_five, square_plus_affine);
    dval_t *exp_expr = dv_mul(poly_expr, exp_affine);
    dval_t *sin_expr = dv_mul(sin_affine, poly_expr);
    dval_t *cosh_expr = dv_mul(poly_expr, cosh_affine);
    dval_t *mismatch_affine = dv_add_d(x, 1.0);
    dval_t *mismatch_poly = dv_add(poly_expr, mismatch_affine);
    dval_t *mismatch = dv_mul(mismatch_poly, exp_affine);

    ASSERT_TRUE(dv_match_affine_poly_deg4(poly_expr, 2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "poly constant term", poly[0], qf_from_double(5.0));
    check_q_at(__FILE__, __LINE__, 1, "poly linear term", poly[1], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "poly square term", poly[2], qf_from_double(-2.0));
    check_q_at(__FILE__, __LINE__, 1, "poly cube term", poly[3], QF_ZERO);
    check_q_at(__FILE__, __LINE__, 1, "poly quartic term", poly[4], qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "poly affine constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "poly affine coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "poly affine coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(exp_expr, DV_PATTERN_UNARY_EXP,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(sin_expr, DV_PATTERN_UNARY_SIN,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(cosh_expr, DV_PATTERN_UNARY_COSH,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(mismatch, DV_PATTERN_UNARY_EXP,
                                                                   2, vars, poly, &constant, coeffs));

    dv_free(mismatch);
    dv_free(mismatch_poly);
    dv_free(mismatch_affine);
    dv_free(cosh_expr);
    dv_free(sin_expr);
    dv_free(exp_expr);
    dv_free(poly_expr);
    dv_free(square_plus_affine);
    dv_free(square_scaled);
    dv_free(quartic_plus_five);
    dv_free(quartic_scaled);
    dv_free(cosh_affine);
    dv_free(sin_affine);
    dv_free(exp_affine);
    dv_free(quartic);
    dv_free(square);
    dv_free(affine);
    dv_free(sum_xy);
    dv_free(two_y);
    dv_free(y);
    dv_free(x);
}

static void test_generic_affine_poly_deg4_times_unary_matcher(void)
{
    dval_t *x = dv_new_named_var_d(1.0, "x");
    dval_t *y = dv_new_named_var_d(2.0, "y");
    dval_t *vars[] = { x, y };
    qfloat_t poly[5];
    qfloat_t constant;
    qfloat_t coeffs[2];
    dval_t *two_y = dv_mul_d(y, 2.0);
    dval_t *sum_xy = dv_add(x, two_y);
    dval_t *affine = dv_add_d(sum_xy, 3.0);
    dval_t *square = dv_pow_d(affine, 2.0);
    dval_t *quartic = dv_pow_d(affine, 4.0);
    dval_t *quartic_scaled = dv_mul_d(quartic, 3.0);
    dval_t *quartic_plus_five = dv_add_d(quartic_scaled, 5.0);
    dval_t *square_scaled = dv_mul_d(square, -2.0);
    dval_t *square_plus_affine = dv_add(square_scaled, affine);
    dval_t *poly_expr = dv_add(quartic_plus_five, square_plus_affine);
    dval_t *cosh_affine = dv_cosh(affine);
    dval_t *expr = dv_mul(poly_expr, cosh_affine);

    ASSERT_TRUE(dv_match_affine_poly_deg4_times_unary_affine_kind(expr, DV_PATTERN_UNARY_COSH,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "generic poly coeff a^0", poly[0], qf_from_double(5.0));
    check_q_at(__FILE__, __LINE__, 1, "generic poly coeff a^1", poly[1], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "generic poly coeff a^2", poly[2], qf_from_double(-2.0));
    check_q_at(__FILE__, __LINE__, 1, "generic poly coeff a^4", poly[4], qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "generic poly constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "generic poly coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "generic poly coeff y", coeffs[1], qf_from_double(2.0));
    ASSERT_TRUE(!dv_match_affine_poly_deg4_times_unary_affine_kind(expr,
                                                                   (dv_pattern_unary_affine_kind_t)-1,
                                                                   2, vars, poly, &constant, coeffs));

    dv_free(expr);
    dv_free(cosh_affine);
    dv_free(poly_expr);
    dv_free(square_plus_affine);
    dv_free(square_scaled);
    dv_free(quartic_plus_five);
    dv_free(quartic_scaled);
    dv_free(quartic);
    dv_free(square);
    dv_free(affine);
    dv_free(sum_xy);
    dv_free(two_y);
    dv_free(y);
    dv_free(x);
}

void test_dval_pattern_helpers(void)
{
    RUN_SUBTEST(test_match_affine_families);
    RUN_SUBTEST(test_generic_unary_affine_matchers);
    RUN_SUBTEST(test_pattern_rejections);
    RUN_SUBTEST(test_scaled_expr_and_var_usage);
    RUN_SUBTEST(test_substitute_and_powd);
    RUN_SUBTEST(test_square_affine_matchers);
    RUN_SUBTEST(test_cube_affine_matchers);
    RUN_SUBTEST(test_quartic_affine_matchers);
    RUN_SUBTEST(test_affine_times_exp_affine_matcher);
    RUN_SUBTEST(test_square_affine_times_exp_affine_matcher);
    RUN_SUBTEST(test_square_affine_times_trig_affine_matchers);
    RUN_SUBTEST(test_affine_times_trig_affine_matchers);
    RUN_SUBTEST(test_affine_times_hyperbolic_affine_matchers);
    RUN_SUBTEST(test_square_affine_times_hyperbolic_affine_matchers);
    RUN_SUBTEST(test_cube_affine_times_unary_affine_matchers);
    RUN_SUBTEST(test_quartic_affine_times_unary_affine_matchers);
    RUN_SUBTEST(test_affine_poly_deg4_times_unary_affine_matchers);
    RUN_SUBTEST(test_generic_affine_poly_deg4_times_unary_matcher);
}
