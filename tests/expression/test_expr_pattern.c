#include "test_expr.h"
#include "internal/expr_internal.h"
#include "internal/number_internal.h"

static bool expr_match_unary_affine_kind_qf_local(const expr_t *expr,
                                                expr_pattern_unary_affine_kind_t unary_kind,
                                                size_t nvars,
                                                expr_t *const *vars,
                                                qfloat_t *constant_out,
                                                qfloat_t *coeffs_out)
{
    number_t constant = number_invalid();
    number_t *coeffs_num = NULL;
    bool ok;

    if (!constant_out)
        return false;
    coeffs_num = malloc(nvars * sizeof(*coeffs_num));
    if ((nvars > 0) && !coeffs_num)
        return false;
    for (size_t i = 0; i < nvars; ++i)
        coeffs_num[i] = number_invalid();
    ok = expr_match_unary_affine_kind(expr, unary_kind, nvars, vars, &constant, coeffs_num);
    if (ok)
        *constant_out = num_to_qfloat(constant);
    if (ok)
        for (size_t i = 0; i < nvars; ++i)
            coeffs_out[i] = num_to_qfloat(coeffs_num[i]);
    for (size_t i = 0; i < nvars; ++i)
        num_destroy(&coeffs_num[i]);
    free(coeffs_num);
    num_destroy(&constant);
    return ok;
}

static bool expr_match_affine_poly_deg4_qf_local(const expr_t *expr,
                                               size_t nvars,
                                               expr_t *const *vars,
                                               qfloat_t *poly_coeffs_out,
                                               qfloat_t *constant_out,
                                               qfloat_t *coeffs_out)
{
    number_t constant = number_invalid();
    number_t poly_num[5];
    number_t *coeffs_num = NULL;
    bool ok;

    if (!constant_out)
        return false;
    coeffs_num = malloc(nvars * sizeof(*coeffs_num));
    if ((nvars > 0) && !coeffs_num)
        return false;
    for (size_t i = 0; i < nvars; ++i)
        coeffs_num[i] = number_invalid();
    for (size_t i = 0; i < 5; ++i)
        poly_num[i] = number_invalid();
    ok = expr_match_affine_poly_deg4(expr, nvars, vars, poly_num, &constant, coeffs_num);
    if (ok)
        *constant_out = num_to_qfloat(constant);
    if (ok) {
        for (size_t i = 0; i < 5; ++i)
            poly_coeffs_out[i] = num_to_qfloat(poly_num[i]);
        for (size_t i = 0; i < nvars; ++i)
            coeffs_out[i] = num_to_qfloat(coeffs_num[i]);
    }
    for (size_t i = 0; i < 5; ++i)
        num_destroy(&poly_num[i]);
    for (size_t i = 0; i < nvars; ++i)
        num_destroy(&coeffs_num[i]);
    free(coeffs_num);
    num_destroy(&constant);
    return ok;
}

static bool expr_match_affine_poly_deg4_times_unary_affine_kind_qf_local(
    const expr_t *expr,
    expr_pattern_unary_affine_kind_t unary_kind,
    size_t nvars,
    expr_t *const *vars,
    qfloat_t *poly_coeffs_out,
    qfloat_t *constant_out,
    qfloat_t *coeffs_out)
{
    number_t constant = number_invalid();
    number_t poly_num[5];
    number_t *coeffs_num = NULL;
    bool ok;

    if (!constant_out)
        return false;
    coeffs_num = malloc(nvars * sizeof(*coeffs_num));
    if ((nvars > 0) && !coeffs_num)
        return false;
    for (size_t i = 0; i < nvars; ++i)
        coeffs_num[i] = number_invalid();
    for (size_t i = 0; i < 5; ++i)
        poly_num[i] = number_invalid();
    ok = expr_match_affine_poly_deg4_times_unary_affine_kind(
        expr, unary_kind, nvars, vars, poly_num, &constant, coeffs_num);
    if (ok)
        *constant_out = num_to_qfloat(constant);
    if (ok) {
        for (size_t i = 0; i < 5; ++i)
            poly_coeffs_out[i] = num_to_qfloat(poly_num[i]);
        for (size_t i = 0; i < nvars; ++i)
            coeffs_out[i] = num_to_qfloat(coeffs_num[i]);
    }
    for (size_t i = 0; i < 5; ++i)
        num_destroy(&poly_num[i]);
    for (size_t i = 0; i < nvars; ++i)
        num_destroy(&coeffs_num[i]);
    free(coeffs_num);
    num_destroy(&constant);
    return ok;
}

#define expr_match_unary_affine_kind(...) \
    expr_match_unary_affine_kind_qf_local(__VA_ARGS__)
#define expr_match_affine_poly_deg4(...) \
    expr_match_affine_poly_deg4_qf_local(__VA_ARGS__)
#define expr_match_affine_poly_deg4_times_unary_affine_kind(...) \
    expr_match_affine_poly_deg4_times_unary_affine_kind_qf_local(__VA_ARGS__)

static void test_match_affine_families(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];

    expr_t *two_x = expr_mul_d(x, 2.0);
    expr_t *y_over_four = expr_div_d(y, 4.0);
    expr_t *linear = expr_sub(two_x, y_over_four);
    expr_t *affine = expr_add_d(linear, 3.0);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *neg_two_x_inner = expr_mul_d(x, 2.0);
    expr_t *neg_two_x = expr_neg(neg_two_x_inner);
    expr_t *sin_arg = expr_add_d(neg_two_x, 3.0);
    expr_t *sin_affine = expr_sin(sin_arg);

    ASSERT_TRUE(expr_match_unary_affine_kind(exp_affine, EXPR_PATTERN_UNARY_EXP,
                                           2, vars, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "exp affine constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "exp affine coeff x", coeffs[0], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "exp affine coeff y", coeffs[1], qf_from_double(-0.25));

    ASSERT_TRUE(expr_match_unary_affine_kind(sin_affine, EXPR_PATTERN_UNARY_SIN,
                                           2, vars, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "sin affine constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "sin affine coeff x", coeffs[0], qf_from_double(-2.0));
    check_q_at(__FILE__, __LINE__, 1, "sin affine coeff y", coeffs[1], QF_ZERO);

    expr_free(sin_affine);
    expr_free(sin_arg);
    expr_free(neg_two_x);
    expr_free(neg_two_x_inner);
    expr_free(exp_affine);
    expr_free(affine);
    expr_free(linear);
    expr_free(y_over_four);
    expr_free(two_x);
    expr_free(y);
    expr_free(x);
}

static void test_generic_unary_affine_matchers(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *sin_affine = expr_sin(affine);

    ASSERT_TRUE(expr_match_unary_affine_kind(sin_affine, EXPR_PATTERN_UNARY_SIN,
                                           2, vars, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "generic unary constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "generic unary coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "generic unary coeff y", coeffs[1], qf_from_double(2.0));
    ASSERT_TRUE(!expr_match_unary_affine_kind(sin_affine, EXPR_PATTERN_UNARY_COS,
                                            2, vars, &constant, coeffs));

    expr_free(sin_affine);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
}

static void test_pattern_rejections(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *c = test_expr_new_named_const_d(5.0, "c");
    expr_t *vars[] = { x, y };
    number_t value = num_new();
    qfloat_t constant;
    qfloat_t coeffs[2];
    number_t scale = num_new();
    const expr_t *base = NULL;

    expr_t *xy = expr_mul(x, y);
    expr_t *exp_xy = expr_exp(xy);
    expr_t *x_over_y = expr_div(x, y);

    ASSERT_TRUE(!expr_match_const_value(c, &value));
    ASSERT_TRUE(!expr_match_unary_affine_kind(exp_xy, EXPR_PATTERN_UNARY_EXP,
                                            2, vars, &constant, coeffs));
    ASSERT_TRUE(!expr_match_scaled_expr(x_over_y, &scale, &base));

    num_destroy(&scale);
    num_destroy(&value);

    expr_free(x_over_y);
    expr_free(exp_xy);
    expr_free(xy);
    expr_free(c);
    expr_free(y);
    expr_free(x);
}

static void test_scaled_expr_and_var_usage(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *z = test_expr_new_named_var_d(3.0, "z");
    expr_t *vars[] = { x, y, z };
    number_t scale = num_new();
    const expr_t *base = NULL;
    bool used[3];

    expr_t *scaled_inner = expr_mul_d(x, 6.0);
    expr_t *scaled_neg = expr_neg(scaled_inner);
    expr_t *scaled = expr_div_d(scaled_neg, 3.0);
    expr_t *usage_scaled = expr_mul_d(x, 2.0);
    expr_t *usage_exp = expr_exp(y);
    expr_t *usage_expr = expr_add(usage_scaled, usage_exp);

    ASSERT_TRUE(expr_match_scaled_expr(scaled, &scale, &base));
    check_q_at(__FILE__, __LINE__, 1, "scaled expr factor", num_to_qfloat(scale), qf_from_double(-2.0));
    ASSERT_TRUE(base == x);

    ASSERT_TRUE(expr_collect_var_usage(usage_expr, 3, vars, used));
    ASSERT_TRUE(used[0]);
    ASSERT_TRUE(used[1]);
    ASSERT_TRUE(!used[2]);

    num_destroy(&scale);
    expr_free(usage_expr);
    expr_free(usage_exp);
    expr_free(usage_scaled);
    expr_free(scaled);
    expr_free(scaled_neg);
    expr_free(scaled_inner);
    expr_free(z);
    expr_free(y);
    expr_free(x);
}

static void test_substitute_and_powd(void)
{
    expr_t *x = test_expr_new_named_var_d(2.0, "x");
    expr_t *y = test_expr_new_named_var_d(3.0, "y");
    expr_t *c = test_expr_new_named_const_d(2.0, "c");

    expr_t *x2 = expr_pow_d(x, 2.0);
    expr_t *expr = expr_add(x2, c);
    expr_t *replacement = expr_add_d(y, 1.0);
    expr_t *sub = expr_substitute(expr, x, replacement);
    char *s = expr_to_string(sub, style_EXPRESSION);

    ASSERT_NOT_NULL(sub);
    ASSERT_NOT_NULL(s);
    check_q_at(__FILE__, __LINE__, 1, "substitute initial eval", expr_eval_qf(sub), qf_from_double(18.0));

    test_expr_set_val_d(x, 10.0);
    check_q_at(__FILE__, __LINE__, 1, "substitute ignores old x", expr_eval_qf(sub), qf_from_double(18.0));

    test_expr_set_val_d(y, 4.0);
    check_q_at(__FILE__, __LINE__, 1, "substitute tracks replacement y", expr_eval_qf(sub), qf_from_double(27.0));

    ASSERT_TRUE(strstr(s, "y") != NULL);
    ASSERT_TRUE(strstr(s, "c") != NULL);

    free(s);
    expr_free(sub);
    expr_free(replacement);
    expr_free(expr);
    expr_free(x2);
    expr_free(c);
    expr_free(y);
    expr_free(x);
}

static void test_square_affine_matchers(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *vars[] = { x, y };
    qfloat_t poly[5];
    qfloat_t constant;
    qfloat_t coeffs[2];
    expr_t *two_x = expr_mul_d(x, 2.0);
    expr_t *y_over_four = expr_div_d(y, 4.0);
    expr_t *linear = expr_sub(two_x, y_over_four);
    expr_t *affine = expr_add_d(linear, 3.0);
    expr_t *pow_square = expr_pow_d(affine, 2.0);
    expr_t *mul_square = expr_mul(affine, affine);

    ASSERT_TRUE(expr_match_affine_poly_deg4(pow_square, 2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "square affine pow constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "square affine pow coeff x", coeffs[0], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "square affine pow coeff y", coeffs[1], qf_from_double(-0.25));
    check_q_at(__FILE__, __LINE__, 1, "square affine pow poly a^2", poly[2], QF_ONE);

    ASSERT_TRUE(expr_match_affine_poly_deg4(mul_square, 2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "square affine mul constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "square affine mul coeff x", coeffs[0], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "square affine mul coeff y", coeffs[1], qf_from_double(-0.25));

    expr_free(mul_square);
    expr_free(pow_square);
    expr_free(affine);
    expr_free(linear);
    expr_free(y_over_four);
    expr_free(two_x);
    expr_free(y);
    expr_free(x);
}

static void test_cube_affine_matchers(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *vars[] = { x, y };
    qfloat_t poly[5];
    qfloat_t constant;
    qfloat_t coeffs[2];
    expr_t *two_x = expr_mul_d(x, 2.0);
    expr_t *y_over_four = expr_div_d(y, 4.0);
    expr_t *linear = expr_sub(two_x, y_over_four);
    expr_t *affine = expr_add_d(linear, 3.0);
    expr_t *pow_cube = expr_pow_d(affine, 3.0);
    expr_t *mul_cube_square = expr_mul(affine, affine);
    expr_t *mul_cube = expr_mul(mul_cube_square, affine);

    ASSERT_TRUE(expr_match_affine_poly_deg4(pow_cube, 2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "cube affine pow constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "cube affine pow coeff x", coeffs[0], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "cube affine pow coeff y", coeffs[1], qf_from_double(-0.25));
    check_q_at(__FILE__, __LINE__, 1, "cube affine pow poly a^3", poly[3], QF_ONE);

    ASSERT_TRUE(expr_match_affine_poly_deg4(mul_cube, 2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "cube affine mul constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "cube affine mul coeff x", coeffs[0], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "cube affine mul coeff y", coeffs[1], qf_from_double(-0.25));

    expr_free(mul_cube);
    expr_free(mul_cube_square);
    expr_free(pow_cube);
    expr_free(affine);
    expr_free(linear);
    expr_free(y_over_four);
    expr_free(two_x);
    expr_free(y);
    expr_free(x);
}

static void test_quartic_affine_matchers(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *vars[] = { x, y };
    qfloat_t poly[5];
    qfloat_t constant;
    qfloat_t coeffs[2];
    expr_t *two_x = expr_mul_d(x, 2.0);
    expr_t *y_over_four = expr_div_d(y, 4.0);
    expr_t *linear = expr_sub(two_x, y_over_four);
    expr_t *affine = expr_add_d(linear, 3.0);
    expr_t *pow_quartic = expr_pow_d(affine, 4.0);
    expr_t *mul_quartic_lhs = expr_mul(affine, affine);
    expr_t *mul_quartic_rhs = expr_mul(affine, affine);
    expr_t *mul_quartic = expr_mul(mul_quartic_lhs, mul_quartic_rhs);

    ASSERT_TRUE(expr_match_affine_poly_deg4(pow_quartic, 2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "quartic affine pow constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "quartic affine pow coeff x", coeffs[0], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "quartic affine pow coeff y", coeffs[1], qf_from_double(-0.25));
    check_q_at(__FILE__, __LINE__, 1, "quartic affine pow poly a^4", poly[4], QF_ONE);

    ASSERT_TRUE(expr_match_affine_poly_deg4(mul_quartic, 2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "quartic affine mul constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "quartic affine mul coeff x", coeffs[0], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "quartic affine mul coeff y", coeffs[1], qf_from_double(-0.25));

    expr_free(mul_quartic);
    expr_free(mul_quartic_rhs);
    expr_free(mul_quartic_lhs);
    expr_free(pow_quartic);
    expr_free(affine);
    expr_free(linear);
    expr_free(y_over_four);
    expr_free(two_x);
    expr_free(y);
    expr_free(x);
}

static void test_affine_times_exp_affine_matcher(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *expr = expr_mul(affine, exp_affine);
    expr_t *mismatch_affine = expr_add_d(x, 1.0);
    expr_t *mismatch = expr_mul(mismatch_affine, exp_affine);

    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(expr, EXPR_PATTERN_UNARY_EXP,
                                                                  2, vars, (qfloat_t[5]){0},
                                                                  &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "affexp constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "affexp coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "affexp coeff y", coeffs[1], qf_from_double(2.0));
    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_EXP,
                                                                   2, vars, (qfloat_t[5]){0},
                                                                   &constant, coeffs));

    expr_free(mismatch);
    expr_free(mismatch_affine);
    expr_free(expr);
    expr_free(exp_affine);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
}

static void test_square_affine_times_exp_affine_matcher(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    qfloat_t poly[5];
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_mul(affine, affine);
    expr_t *pow_square = expr_pow_d(affine, 2.0);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *expr = expr_mul(square, exp_affine);
    expr_t *expr_pow = expr_mul(exp_affine, pow_square);
    expr_t *mismatch_lhs = expr_add_d(x, 1.0);
    expr_t *mismatch_rhs = expr_add_d(x, 1.0);
    expr_t *mismatch_square = expr_mul(mismatch_lhs, mismatch_rhs);
    expr_t *mismatch = expr_mul(mismatch_square, exp_affine);

    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(expr, EXPR_PATTERN_UNARY_EXP,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "sqaffexp constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffexp coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffexp coeff y", coeffs[1], qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffexp poly a^2", poly[2], QF_ONE);

    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(expr_pow, EXPR_PATTERN_UNARY_EXP,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_EXP,
                                                                   2, vars, poly, &constant, coeffs));

    expr_free(mismatch);
    expr_free(mismatch_square);
    expr_free(mismatch_rhs);
    expr_free(mismatch_lhs);
    expr_free(expr_pow);
    expr_free(expr);
    expr_free(exp_affine);
    expr_free(pow_square);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
}

static void test_square_affine_times_trig_affine_matchers(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    qfloat_t poly[5];
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_mul(affine, affine);
    expr_t *pow_square = expr_pow_d(affine, 2.0);
    expr_t *sin_affine = expr_sin(affine);
    expr_t *cos_affine = expr_cos(affine);
    expr_t *sin_expr = expr_mul(square, sin_affine);
    expr_t *cos_expr = expr_mul(cos_affine, pow_square);
    expr_t *mismatch_lhs = expr_add_d(x, 1.0);
    expr_t *mismatch_rhs = expr_add_d(x, 1.0);
    expr_t *mismatch_square = expr_mul(mismatch_lhs, mismatch_rhs);
    expr_t *mismatch = expr_mul(mismatch_square, sin_affine);

    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(sin_expr, EXPR_PATTERN_UNARY_SIN,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "sqaffsin constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffsin coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffsin coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(cos_expr, EXPR_PATTERN_UNARY_COS,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "sqaffcos constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffcos coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffcos coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_SIN,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_COS,
                                                                   2, vars, poly, &constant, coeffs));

    expr_free(mismatch);
    expr_free(mismatch_square);
    expr_free(mismatch_rhs);
    expr_free(mismatch_lhs);
    expr_free(cos_expr);
    expr_free(sin_expr);
    expr_free(cos_affine);
    expr_free(sin_affine);
    expr_free(pow_square);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
}

static void test_affine_times_trig_affine_matchers(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    qfloat_t poly[5];
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *sin_affine = expr_sin(affine);
    expr_t *cos_affine = expr_cos(affine);
    expr_t *sin_expr = expr_mul(affine, sin_affine);
    expr_t *cos_expr = expr_mul(cos_affine, affine);
    expr_t *mismatch_affine = expr_add_d(x, 1.0);
    expr_t *mismatch = expr_mul(mismatch_affine, sin_affine);

    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(sin_expr, EXPR_PATTERN_UNARY_SIN,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "affsin constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "affsin coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "affsin coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(cos_expr, EXPR_PATTERN_UNARY_COS,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "affcos constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "affcos coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "affcos coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_SIN,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_COS,
                                                                   2, vars, poly, &constant, coeffs));

    expr_free(mismatch);
    expr_free(mismatch_affine);
    expr_free(cos_expr);
    expr_free(sin_expr);
    expr_free(cos_affine);
    expr_free(sin_affine);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
}

static void test_affine_times_hyperbolic_affine_matchers(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    qfloat_t poly[5];
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *sinh_affine = expr_sinh(affine);
    expr_t *cosh_affine = expr_cosh(affine);
    expr_t *sinh_expr = expr_mul(affine, sinh_affine);
    expr_t *cosh_expr = expr_mul(cosh_affine, affine);
    expr_t *mismatch_affine = expr_add_d(x, 1.0);
    expr_t *mismatch = expr_mul(mismatch_affine, sinh_affine);

    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(sinh_expr, EXPR_PATTERN_UNARY_SINH,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "affsinh constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "affsinh coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "affsinh coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(cosh_expr, EXPR_PATTERN_UNARY_COSH,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "affcosh constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "affcosh coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "affcosh coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_SINH,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_COSH,
                                                                   2, vars, poly, &constant, coeffs));

    expr_free(mismatch);
    expr_free(mismatch_affine);
    expr_free(cosh_expr);
    expr_free(sinh_expr);
    expr_free(cosh_affine);
    expr_free(sinh_affine);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
}

static void test_square_affine_times_hyperbolic_affine_matchers(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    qfloat_t poly[5];
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_mul(affine, affine);
    expr_t *pow_square = expr_pow_d(affine, 2.0);
    expr_t *sinh_affine = expr_sinh(affine);
    expr_t *cosh_affine = expr_cosh(affine);
    expr_t *sinh_expr = expr_mul(square, sinh_affine);
    expr_t *cosh_expr = expr_mul(cosh_affine, pow_square);
    expr_t *mismatch_lhs = expr_add_d(x, 1.0);
    expr_t *mismatch_rhs = expr_add_d(x, 1.0);
    expr_t *mismatch_square = expr_mul(mismatch_lhs, mismatch_rhs);
    expr_t *mismatch = expr_mul(mismatch_square, sinh_affine);

    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(sinh_expr, EXPR_PATTERN_UNARY_SINH,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "sqaffsinh constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffsinh coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffsinh coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(cosh_expr, EXPR_PATTERN_UNARY_COSH,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "sqaffcosh constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffcosh coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "sqaffcosh coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_SINH,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_COSH,
                                                                   2, vars, poly, &constant, coeffs));

    expr_free(mismatch);
    expr_free(mismatch_square);
    expr_free(mismatch_rhs);
    expr_free(mismatch_lhs);
    expr_free(cosh_expr);
    expr_free(sinh_expr);
    expr_free(cosh_affine);
    expr_free(sinh_affine);
    expr_free(pow_square);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
}

static void test_cube_affine_times_unary_affine_matchers(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    qfloat_t poly[5];
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *cube_square = expr_mul(affine, affine);
    expr_t *cube = expr_mul(cube_square, affine);
    expr_t *pow_cube = expr_pow_d(affine, 3.0);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *sin_affine = expr_sin(affine);
    expr_t *cos_affine = expr_cos(affine);
    expr_t *sinh_affine = expr_sinh(affine);
    expr_t *cosh_affine = expr_cosh(affine);
    expr_t *exp_expr = expr_mul(cube, exp_affine);
    expr_t *sin_expr = expr_mul(pow_cube, sin_affine);
    expr_t *cos_expr = expr_mul(cos_affine, cube);
    expr_t *sinh_expr = expr_mul(pow_cube, sinh_affine);
    expr_t *cosh_expr = expr_mul(cosh_affine, cube);
    expr_t *mismatch_a = expr_add_d(x, 1.0);
    expr_t *mismatch_b = expr_add_d(x, 1.0);
    expr_t *mismatch_c = expr_add_d(x, 1.0);
    expr_t *mismatch_square = expr_mul(mismatch_a, mismatch_b);
    expr_t *mismatch_cube = expr_mul(mismatch_square, mismatch_c);
    expr_t *mismatch = expr_mul(mismatch_cube, exp_affine);

    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(exp_expr, EXPR_PATTERN_UNARY_EXP,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "cubaffexp constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "cubaffexp coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "cubaffexp coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(sin_expr, EXPR_PATTERN_UNARY_SIN,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(cos_expr, EXPR_PATTERN_UNARY_COS,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(sinh_expr, EXPR_PATTERN_UNARY_SINH,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(cosh_expr, EXPR_PATTERN_UNARY_COSH,
                                                                  2, vars, poly, &constant, coeffs));

    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_EXP,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_SIN,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_COS,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_SINH,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_COSH,
                                                                   2, vars, poly, &constant, coeffs));

    expr_free(mismatch);
    expr_free(mismatch_cube);
    expr_free(mismatch_square);
    expr_free(mismatch_c);
    expr_free(mismatch_b);
    expr_free(mismatch_a);
    expr_free(cosh_expr);
    expr_free(sinh_expr);
    expr_free(cos_expr);
    expr_free(sin_expr);
    expr_free(exp_expr);
    expr_free(cosh_affine);
    expr_free(sinh_affine);
    expr_free(cos_affine);
    expr_free(sin_affine);
    expr_free(exp_affine);
    expr_free(pow_cube);
    expr_free(cube_square);
    expr_free(cube);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
}

static void test_quartic_affine_times_unary_affine_matchers(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *vars[] = { x, y };
    qfloat_t constant;
    qfloat_t coeffs[2];
    qfloat_t poly[5];
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *quartic_lhs = expr_mul(affine, affine);
    expr_t *quartic_rhs = expr_mul(affine, affine);
    expr_t *quartic = expr_mul(quartic_lhs, quartic_rhs);
    expr_t *pow_quartic = expr_pow_d(affine, 4.0);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *sin_affine = expr_sin(affine);
    expr_t *cos_affine = expr_cos(affine);
    expr_t *sinh_affine = expr_sinh(affine);
    expr_t *cosh_affine = expr_cosh(affine);
    expr_t *exp_expr = expr_mul(quartic, exp_affine);
    expr_t *sin_expr = expr_mul(pow_quartic, sin_affine);
    expr_t *cos_expr = expr_mul(cos_affine, quartic);
    expr_t *sinh_expr = expr_mul(pow_quartic, sinh_affine);
    expr_t *cosh_expr = expr_mul(cosh_affine, quartic);
    expr_t *mismatch_a = expr_add_d(x, 1.0);
    expr_t *mismatch_b = expr_add_d(x, 1.0);
    expr_t *mismatch_c = expr_add_d(x, 1.0);
    expr_t *mismatch_d = expr_add_d(x, 2.0);
    expr_t *mismatch_lhs = expr_mul(mismatch_a, mismatch_b);
    expr_t *mismatch_rhs = expr_mul(mismatch_c, mismatch_d);
    expr_t *mismatch_quartic = expr_mul(mismatch_lhs, mismatch_rhs);
    expr_t *mismatch = expr_mul(mismatch_quartic, exp_affine);

    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(exp_expr, EXPR_PATTERN_UNARY_EXP,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "qrtaffexp constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "qrtaffexp coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "qrtaffexp coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(sin_expr, EXPR_PATTERN_UNARY_SIN,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(cos_expr, EXPR_PATTERN_UNARY_COS,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(sinh_expr, EXPR_PATTERN_UNARY_SINH,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(cosh_expr, EXPR_PATTERN_UNARY_COSH,
                                                                  2, vars, poly, &constant, coeffs));

    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_EXP,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_SIN,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_COS,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_SINH,
                                                                   2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_COSH,
                                                                   2, vars, poly, &constant, coeffs));

    expr_free(mismatch);
    expr_free(mismatch_quartic);
    expr_free(mismatch_rhs);
    expr_free(mismatch_lhs);
    expr_free(mismatch_d);
    expr_free(mismatch_c);
    expr_free(mismatch_b);
    expr_free(mismatch_a);
    expr_free(cosh_expr);
    expr_free(sinh_expr);
    expr_free(cos_expr);
    expr_free(sin_expr);
    expr_free(exp_expr);
    expr_free(cosh_affine);
    expr_free(sinh_affine);
    expr_free(cos_affine);
    expr_free(sin_affine);
    expr_free(exp_affine);
    expr_free(pow_quartic);
    expr_free(quartic_rhs);
    expr_free(quartic_lhs);
    expr_free(quartic);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
}

static void test_affine_poly_deg4_times_unary_affine_matchers(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *vars[] = { x, y };
    qfloat_t poly[5];
    qfloat_t constant;
    qfloat_t coeffs[2];
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_pow_d(affine, 2.0);
    expr_t *quartic = expr_pow_d(affine, 4.0);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *sin_affine = expr_sin(affine);
    expr_t *cosh_affine = expr_cosh(affine);
    expr_t *quartic_scaled = expr_mul_d(quartic, 3.0);
    expr_t *quartic_plus_five = expr_add_d(quartic_scaled, 5.0);
    expr_t *square_scaled = expr_mul_d(square, -2.0);
    expr_t *square_plus_affine = expr_add(square_scaled, affine);
    expr_t *poly_expr = expr_add(quartic_plus_five, square_plus_affine);
    expr_t *exp_expr = expr_mul(poly_expr, exp_affine);
    expr_t *sin_expr = expr_mul(sin_affine, poly_expr);
    expr_t *cosh_expr = expr_mul(poly_expr, cosh_affine);
    expr_t *mismatch_affine = expr_add_d(x, 1.0);
    expr_t *mismatch_poly = expr_add(poly_expr, mismatch_affine);
    expr_t *mismatch = expr_mul(mismatch_poly, exp_affine);

    ASSERT_TRUE(expr_match_affine_poly_deg4(poly_expr, 2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "poly constant term", poly[0], qf_from_double(5.0));
    check_q_at(__FILE__, __LINE__, 1, "poly linear term", poly[1], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "poly square term", poly[2], qf_from_double(-2.0));
    check_q_at(__FILE__, __LINE__, 1, "poly cube term", poly[3], QF_ZERO);
    check_q_at(__FILE__, __LINE__, 1, "poly quartic term", poly[4], qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "poly affine constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "poly affine coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "poly affine coeff y", coeffs[1], qf_from_double(2.0));

    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(exp_expr, EXPR_PATTERN_UNARY_EXP,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(sin_expr, EXPR_PATTERN_UNARY_SIN,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(cosh_expr, EXPR_PATTERN_UNARY_COSH,
                                                                  2, vars, poly, &constant, coeffs));
    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(mismatch, EXPR_PATTERN_UNARY_EXP,
                                                                   2, vars, poly, &constant, coeffs));

    expr_free(mismatch);
    expr_free(mismatch_poly);
    expr_free(mismatch_affine);
    expr_free(cosh_expr);
    expr_free(sin_expr);
    expr_free(exp_expr);
    expr_free(poly_expr);
    expr_free(square_plus_affine);
    expr_free(square_scaled);
    expr_free(quartic_plus_five);
    expr_free(quartic_scaled);
    expr_free(cosh_affine);
    expr_free(sin_affine);
    expr_free(exp_affine);
    expr_free(quartic);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
}

static void test_generic_affine_poly_deg4_times_unary_matcher(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *y = test_expr_new_named_var_d(2.0, "y");
    expr_t *vars[] = { x, y };
    qfloat_t poly[5];
    qfloat_t constant;
    qfloat_t coeffs[2];
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_pow_d(affine, 2.0);
    expr_t *quartic = expr_pow_d(affine, 4.0);
    expr_t *quartic_scaled = expr_mul_d(quartic, 3.0);
    expr_t *quartic_plus_five = expr_add_d(quartic_scaled, 5.0);
    expr_t *square_scaled = expr_mul_d(square, -2.0);
    expr_t *square_plus_affine = expr_add(square_scaled, affine);
    expr_t *poly_expr = expr_add(quartic_plus_five, square_plus_affine);
    expr_t *cosh_affine = expr_cosh(affine);
    expr_t *expr = expr_mul(poly_expr, cosh_affine);

    ASSERT_TRUE(expr_match_affine_poly_deg4_times_unary_affine_kind(expr, EXPR_PATTERN_UNARY_COSH,
                                                                  2, vars, poly, &constant, coeffs));
    check_q_at(__FILE__, __LINE__, 1, "generic poly coeff a^0", poly[0], qf_from_double(5.0));
    check_q_at(__FILE__, __LINE__, 1, "generic poly coeff a^1", poly[1], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "generic poly coeff a^2", poly[2], qf_from_double(-2.0));
    check_q_at(__FILE__, __LINE__, 1, "generic poly coeff a^4", poly[4], qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "generic poly constant", constant, qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "generic poly coeff x", coeffs[0], qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "generic poly coeff y", coeffs[1], qf_from_double(2.0));
    ASSERT_TRUE(!expr_match_affine_poly_deg4_times_unary_affine_kind(expr,
                                                                   (expr_pattern_unary_affine_kind_t)-1,
                                                                   2, vars, poly, &constant, coeffs));

    expr_free(expr);
    expr_free(cosh_affine);
    expr_free(poly_expr);
    expr_free(square_plus_affine);
    expr_free(square_scaled);
    expr_free(quartic_plus_five);
    expr_free(quartic_scaled);
    expr_free(quartic);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
}

void test_expr_pattern_helpers(void)
{
    TEST_RUN_SUBTEST(test_match_affine_families, NULL);
    TEST_RUN_SUBTEST(test_generic_unary_affine_matchers, NULL);
    TEST_RUN_SUBTEST(test_pattern_rejections, NULL);
    TEST_RUN_SUBTEST(test_scaled_expr_and_var_usage, NULL);
    TEST_RUN_SUBTEST(test_substitute_and_powd, NULL);
    TEST_RUN_SUBTEST(test_square_affine_matchers, NULL);
    TEST_RUN_SUBTEST(test_cube_affine_matchers, NULL);
    TEST_RUN_SUBTEST(test_quartic_affine_matchers, NULL);
    TEST_RUN_SUBTEST(test_affine_times_exp_affine_matcher, NULL);
    TEST_RUN_SUBTEST(test_square_affine_times_exp_affine_matcher, NULL);
    TEST_RUN_SUBTEST(test_square_affine_times_trig_affine_matchers, NULL);
    TEST_RUN_SUBTEST(test_affine_times_trig_affine_matchers, NULL);
    TEST_RUN_SUBTEST(test_affine_times_hyperbolic_affine_matchers, NULL);
    TEST_RUN_SUBTEST(test_square_affine_times_hyperbolic_affine_matchers, NULL);
    TEST_RUN_SUBTEST(test_cube_affine_times_unary_affine_matchers, NULL);
    TEST_RUN_SUBTEST(test_quartic_affine_times_unary_affine_matchers, NULL);
    TEST_RUN_SUBTEST(test_affine_poly_deg4_times_unary_affine_matchers, NULL);
    TEST_RUN_SUBTEST(test_generic_affine_poly_deg4_times_unary_matcher, NULL);
}
