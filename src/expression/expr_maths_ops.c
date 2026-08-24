#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr_maths.h"
#include "ustring.h"

static inline expr_t *expr_math_wrap_unary(const expr_ops_t *ops, const expr_t *a)
{
    if (!a)
        return NULL;
    expr_retain(a);
    return expr_new_unary_internal(ops, a);
}

static inline expr_t *expr_math_wrap_binary(const expr_ops_t *ops, const expr_t *a, const expr_t *b)
{
    if (!a || !b)
        return NULL;
    expr_retain(a);
    expr_retain(b);
    return expr_new_binary_internal(ops, a, b);
}

static expr_t *expr_appell_f1_pack(const expr_t *left, const expr_t *right);
static expr_t *expr_appell_f1_from_packs(const expr_t *params, const expr_t *vars);
static expr_t *expr_lauricella_f_from_packs(const expr_t *params, const expr_t *vars);
static expr_t *expr_hypergeometric_pFq_pack(const expr_t *left, const expr_t *right);
static expr_t *expr_hypergeometric_pFq_from_pack(const expr_t *parameters, const expr_t *argument);
static expr_t *expr_lommel_s_pack(const expr_t *mu, const expr_t *nu);
static expr_t *expr_lommel_s_from_pack(const expr_t *parameters, const expr_t *argument);
expr_t *expr_finite_progression_closed_form(const expr_t *expr);

static number_t eval_formal_series_component(expr_t *dv)
{
    (void)dv;
    return num_clone(NUM_NAN);
}

static bool expr_finite_weighted_sinh_parts(const expr_t *expr, const expr_t **upper_out, const expr_t **step_out);
static number_t eval_finite_weighted_sinh(expr_t *dv, long upper_value);

static const expr_t *expr_find_lerch_phi(const expr_t *expr)
{
    const expr_t *found;

    if (!expr)
        return NULL;
    if (expr_is_op(expr, &ops_lerch_phi))
        return expr;
    found = expr_find_lerch_phi(expr->a);
    return found ? found : expr_find_lerch_phi(expr->b);
}

static bool expr_summation_bound_to_long(const expr_t *bound, long *out)
{
    number_t value;
    string_t *text;
    char *end = NULL;
    long parsed;

    if (!bound || !out)
        return false;
    value = expr_eval(bound);
    if (!num_is_real(value) || !num_is_finite(value) || !num_is_integer(value)) {
        num_destroy(&value);
        return false;
    }
    text = num_to_string(value);
    num_destroy(&value);
    if (!text)
        return false;
    parsed = strtol(string_c_str(text), &end, 10);
    if (!end || *end != '\0') {
        string_free(text);
        return false;
    }
    string_free(text);
    *out = parsed;
    return true;
}

static number_t eval_finite_summation(expr_t *dv)
{
    const long maximum_terms = 10000L;
    expr_t *index;
    const expr_t *lower = NULL;
    const expr_t *upper = NULL;
    number_t sum;
    long lower_value;
    long upper_value;

    if (!dv || !dv->a || !expr_is_op(dv->b, &ops_argument_list))
        return num_clone(NUM_NAN);
    index = dv->b->a;
    upper = dv->b->b;
    if (expr_is_op(upper, &ops_argument_list)) {
        lower = upper->a;
        upper = upper->b;
    }
    lower_value = 0L;
    if (!expr_is_var(index) || !upper || (lower && !expr_summation_bound_to_long(lower, &lower_value)) ||
        !expr_summation_bound_to_long(upper, &upper_value))
        return num_clone(NUM_NAN);
    if (upper_value >= lower_value && upper_value - lower_value >= maximum_terms) {
        expr_t *closed_form = lower_value == 1L ? expr_finite_progression_closed_form(dv) : NULL;

        if (closed_form) {
            number_t closed_value = expr_eval(closed_form);

            expr_free(closed_form);
            if (num_is_finite(closed_value))
                return closed_value;
            num_destroy(&closed_value);
        }
        return lower_value == 1L ? eval_finite_weighted_sinh(dv, upper_value) : num_clone(NUM_NAN);
    }

    sum = num_clone(NUM_ZERO);
    for (long value = lower_value; value <= upper_value; ++value) {
        number_t index_value = num_create_from_long(value);
        expr_t *index_expression = expr_new_const(index_value);
        expr_t *term_expression = index_expression ? expr_substitute(dv->a, index, index_expression) : NULL;
        number_t term;
        number_t updated;

        num_destroy(&index_value);
        term = term_expression ? expr_eval(term_expression) : num_clone(NUM_NAN);
        expr_free(term_expression);
        expr_free(index_expression);
        if (!num_is_finite(term)) {
            num_destroy(&term);
            num_destroy(&sum);
            sum = num_clone(NUM_NAN);
            break;
        }
        updated = num_add(sum, term);
        num_destroy(&term);
        num_destroy(&sum);
        sum = updated;
        if (value == LONG_MAX)
            break;
    }
    return sum;
}

static number_t eval_finite_product(expr_t *dv)
{
    const long maximum_terms = 1000000L;
    expr_t *index;
    const expr_t *lower = NULL;
    const expr_t *upper = NULL;
    number_t product;
    long lower_value;
    long upper_value;

    if (!dv || !dv->a || !expr_is_op(dv->b, &ops_argument_list))
        return num_clone(NUM_NAN);
    index = dv->b->a;
    upper = dv->b->b;
    if (expr_is_op(upper, &ops_argument_list)) {
        lower = upper->a;
        upper = upper->b;
    }
    lower_value = 0L;
    if (!expr_is_var(index) || !upper || (lower && !expr_summation_bound_to_long(lower, &lower_value)) ||
        !expr_summation_bound_to_long(upper, &upper_value))
        return num_clone(NUM_NAN);
    if (upper_value >= lower_value && upper_value - lower_value >= maximum_terms)
        return num_clone(NUM_NAN);

    product = num_clone(NUM_ONE);
    for (long value = lower_value; value <= upper_value; ++value) {
        number_t index_value = num_create_from_long(value);
        expr_t *index_expression = expr_new_const(index_value);
        expr_t *factor_expression = index_expression ? expr_substitute(dv->a, index, index_expression) : NULL;
        number_t factor;
        number_t updated;

        num_destroy(&index_value);
        factor = factor_expression ? expr_eval(factor_expression) : num_clone(NUM_NAN);
        expr_free(factor_expression);
        expr_free(index_expression);
        if (!num_is_finite(factor)) {
            num_destroy(&factor);
            num_destroy(&product);
            product = num_clone(NUM_NAN);
            break;
        }
        updated = num_mul(product, factor);
        num_destroy(&factor);
        num_destroy(&product);
        product = updated;
        if (value == LONG_MAX)
            break;
    }
    return product;
}

static expr_t *deriv_indexed_symbol(expr_t *dv)
{
    expr_t *index_derivative = expr_get_dx_internal(dv->b);
    expr_t *out = index_derivative && expr_is_exact_zero(index_derivative) ? expr_new_const(NUM_ZERO) : NULL;

    expr_free(index_derivative);
    return out;
}

static expr_t *deriv_summation(expr_t *dv)
{
    expr_t *raw_derivative = expr_get_dx_internal(dv->a);
    expr_t *term_derivative = raw_derivative ? expr_simplify(raw_derivative) : NULL;
    expr_t *out = term_derivative ? expr_new_summation(term_derivative, dv->b) : NULL;

    expr_free(raw_derivative);
    expr_free(term_derivative);
    return out;
}

static expr_t *deriv_product(expr_t *dv)
{
    expr_t *raw_derivative = expr_get_dx_internal(dv->a);
    expr_t *factor_derivative = raw_derivative ? expr_simplify(raw_derivative) : NULL;
    expr_t *log_derivative = factor_derivative ? expr_div(factor_derivative, dv->a) : NULL;
    expr_t *sum = log_derivative ? expr_new_summation(log_derivative, dv->b) : NULL;
    expr_t *product = sum ? expr_new_product(dv->a, dv->b) : NULL;
    expr_t *out = product ? expr_mul(product, sum) : NULL;

    expr_free(product);
    expr_free(sum);
    expr_free(log_derivative);
    expr_free(factor_derivative);
    expr_free(raw_derivative);
    return out;
}

typedef enum {
    EXPR_FINITE_PROGRESSION_NONE,
    EXPR_FINITE_PROGRESSION_SIN,
    EXPR_FINITE_PROGRESSION_COS,
    EXPR_FINITE_PROGRESSION_SINH,
    EXPR_FINITE_PROGRESSION_COSH,
} expr_finite_progression_kind_t;

static bool expr_finite_progression_parts(const expr_t *expr, const expr_t **upper_out, const expr_t **step_out,
                                          expr_finite_progression_kind_t *kind_out)
{
    const expr_t *index;
    const expr_t *bounds;
    const expr_t *lower;
    const expr_t *upper;
    const expr_t *argument;
    const expr_t *left;
    const expr_t *right;
    const expr_t *step;
    number_t lower_value = NUM_NAN;
    expr_finite_progression_kind_t kind = EXPR_FINITE_PROGRESSION_NONE;
    bool matched = false;

    if (!expr || !upper_out || !step_out || !kind_out || !expr_is_op(expr, &ops_summation) ||
        !expr_is_op(expr->b, &ops_argument_list))
        return false;
    index = expr->b->a;
    bounds = expr->b->b;
    if (!index || !expr_is_var(index) || !expr_is_op(bounds, &ops_argument_list))
        return false;
    lower = bounds->a;
    upper = bounds->b;
    lower_value = expr_eval(lower);
    if (!num_is_exact(lower_value) || !num_is_one(lower_value))
        goto cleanup;
    if (expr_is_op(expr->a, &ops_sin))
        kind = EXPR_FINITE_PROGRESSION_SIN;
    else if (expr_is_op(expr->a, &ops_cos))
        kind = EXPR_FINITE_PROGRESSION_COS;
    else if (expr_is_op(expr->a, &ops_sinh))
        kind = EXPR_FINITE_PROGRESSION_SINH;
    else if (expr_is_op(expr->a, &ops_cosh))
        kind = EXPR_FINITE_PROGRESSION_COSH;
    else
        goto cleanup;
    argument = expr->a->a;
    if (!expr_match_mul_expr(argument, &left, &right))
        goto cleanup;
    if (left == index || (expr_is_var(left) && left->var_id != 0u && left->var_id == index->var_id))
        step = right;
    else if (right == index || (expr_is_var(right) && right->var_id != 0u && right->var_id == index->var_id))
        step = left;
    else
        goto cleanup;

    *upper_out = upper;
    *step_out = step;
    *kind_out = kind;
    matched = true;

cleanup:
    num_destroy(&lower_value);
    return matched;
}

static bool expr_finite_weighted_sinh_parts(const expr_t *expr, const expr_t **upper_out, const expr_t **step_out)
{
    const expr_t *index;
    const expr_t *bounds;
    const expr_t *lower;
    const expr_t *upper;
    const expr_t *numerator;
    const expr_t *denominator;
    const expr_t *left;
    const expr_t *right;
    number_t lower_value = NUM_NAN;
    bool matched = false;

    if (!expr || !expr_is_op(expr, &ops_summation) || !expr_is_op(expr->b, &ops_argument_list))
        return false;
    index = expr->b->a;
    bounds = expr->b->b;
    if (!index || !expr_is_var(index) || !expr_is_op(bounds, &ops_argument_list))
        return false;
    lower = bounds->a;
    upper = bounds->b;
    lower_value = expr_eval(lower);
    if (!num_is_exact(lower_value) || !num_is_one(lower_value) ||
        !expr_is_op(expr->a, &ops_div) || !(numerator = expr->a->a) || !(denominator = expr->a->b) ||
        !(denominator == index || (expr_is_var(denominator) && denominator->var_id == index->var_id)) ||
        !expr_is_op(numerator, &ops_sinh) || !expr_match_mul_expr(numerator->a, &left, &right))
        goto cleanup;
    if (left == index)
        *step_out = right;
    else if (right == index)
        *step_out = left;
    else
        goto cleanup;
    *upper_out = upper;
    matched = true;

cleanup:
    num_destroy(&lower_value);
    return matched;
}

static number_t eval_finite_weighted_sinh(expr_t *dv, long upper_value)
{
    const expr_t *upper;
    const expr_t *step_expr;
    number_t step;
    number_t magnitude;
    number_t q;
    number_t q_power;
    number_t dominant_scaled;
    number_t decaying;
    number_t dominant;
    number_t result;
    double step_double;
    const long maximum_tail_terms = 10000L;

    if (!expr_finite_weighted_sinh_parts(dv, &upper, &step_expr) || upper_value < 1L)
        return num_clone(NUM_NAN);
    (void)upper;
    step = expr_eval(step_expr);
    if (!num_is_real(step) || !num_is_finite(step) || num_is_zero(step)) {
        bool zero = num_is_zero(step);

        num_destroy(&step);
        return zero ? num_clone(NUM_ZERO) : num_clone(NUM_NAN);
    }
    step_double = num_to_double(step);
    magnitude = step_double < 0.0 ? num_neg(step) : num_clone(step);
    q = num_exp(num_neg(magnitude));
    q_power = num_clone(NUM_ONE);
    dominant_scaled = num_clone(NUM_ZERO);
    decaying = num_clone(NUM_ZERO);
    for (long offset = 0L; offset < upper_value && offset < maximum_tail_terms; ++offset) {
        number_t denominator = num_create_from_long(upper_value - offset);
        number_t term = num_div(q_power, denominator);
        number_t updated = num_add(dominant_scaled, term);
        number_t next_power = num_mul(q_power, q);
        double term_size = num_to_double(num_abs(term));
        double total_size = num_to_double(num_abs(updated));

        num_destroy(&dominant_scaled);
        num_destroy(&q_power);
        num_destroy(&term);
        num_destroy(&denominator);
        dominant_scaled = updated;
        q_power = next_power;
        if (offset > 8L && term_size < 1e-30 * (1.0 + total_size))
            break;
    }
    num_destroy(&q_power);
    q_power = num_clone(q);
    for (long k = 1L; k <= upper_value && k <= maximum_tail_terms; ++k) {
        number_t denominator = num_create_from_long(k);
        number_t term = num_div(q_power, denominator);
        number_t updated = num_add(decaying, term);
        number_t next_power = num_mul(q_power, q);
        double term_size = num_to_double(num_abs(term));
        double total_size = num_to_double(num_abs(updated));

        num_destroy(&decaying);
        num_destroy(&q_power);
        num_destroy(&term);
        num_destroy(&denominator);
        decaying = updated;
        q_power = next_power;
        if (k > 8L && term_size < 1e-30 * (1.0 + total_size))
            break;
    }
    dominant = num_mul(num_exp(num_mul_long(magnitude, upper_value)), dominant_scaled);
    result = num_div(num_sub(dominant, decaying), NUM_TWO);
    if (step_double < 0.0) {
        number_t negative = num_neg(result);

        num_destroy(&result);
        result = negative;
    }
    num_destroy(&dominant);
    num_destroy(&decaying);
    num_destroy(&dominant_scaled);
    num_destroy(&q_power);
    num_destroy(&q);
    num_destroy(&magnitude);
    num_destroy(&step);
    return result;
}

expr_t *expr_finite_weighted_sinh_lerch_form(const expr_t *expr)
{
    const expr_t *n;
    const expr_t *x;
    expr_t *one = NULL;
    expr_t *n_plus_one = NULL;
    expr_t *negative_x = NULL;
    expr_t *positive_z = NULL;
    expr_t *negative_z = NULL;
    expr_t *positive_polylog = NULL;
    expr_t *negative_polylog = NULL;
    expr_t *positive_power = NULL;
    expr_t *negative_power = NULL;
    expr_t *positive_phi = NULL;
    expr_t *negative_phi = NULL;
    expr_t *positive_tail = NULL;
    expr_t *negative_tail = NULL;
    expr_t *positive_polylog_half = NULL;
    expr_t *negative_polylog_half = NULL;
    expr_t *positive_tail_half = NULL;
    expr_t *negative_tail_half = NULL;
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *out = NULL;

    if (!expr_finite_weighted_sinh_parts(expr, &n, &x))
        return NULL;
    one = expr_new_const(NUM_ONE);
    n_plus_one = expr_add_long(n, 1L);
    negative_x = expr_neg(x);
    positive_z = expr_exp(x);
    negative_z = expr_exp(negative_x);
    positive_polylog = expr_polylog1(positive_z);
    negative_polylog = expr_polylog1(negative_z);
    positive_power = expr_pow_xp(positive_z, n_plus_one);
    negative_power = expr_pow_xp(negative_z, n_plus_one);
    positive_phi = expr_lerch_phi(positive_z, one, n_plus_one);
    negative_phi = expr_lerch_phi(negative_z, one, n_plus_one);
    positive_tail = expr_mul(positive_power, positive_phi);
    negative_tail = expr_mul(negative_power, negative_phi);
    positive_polylog_half = expr_mul_num(positive_polylog, &NUM_HALF);
    negative_polylog_half = expr_mul_num(negative_polylog, &NUM_HALF);
    positive_tail_half = expr_mul_num(positive_tail, &NUM_HALF);
    negative_tail_half = expr_mul_num(negative_tail, &NUM_HALF);
    left = expr_sub(positive_polylog_half, negative_polylog_half);
    right = expr_sub(negative_tail_half, positive_tail_half);
    out = expr_add(left, right);
    expr_free(right);
    expr_free(left);
    expr_free(negative_tail_half);
    expr_free(positive_tail_half);
    expr_free(negative_polylog_half);
    expr_free(positive_polylog_half);
    expr_free(negative_tail);
    expr_free(positive_tail);
    expr_free(negative_phi);
    expr_free(positive_phi);
    expr_free(negative_power);
    expr_free(positive_power);
    expr_free(negative_polylog);
    expr_free(positive_polylog);
    expr_free(negative_z);
    expr_free(positive_z);
    expr_free(negative_x);
    expr_free(n_plus_one);
    expr_free(one);
    return out;
}

/* Recover the finite weighted hyperbolic sum represented by a matching Lerch-Phi expression. */
expr_t *expr_finite_weighted_sinh_from_lerch_form(const expr_t *expr)
{
    const expr_t *phi = expr_find_lerch_phi(expr);
    const expr_t *z;
    const expr_t *s;
    const expr_t *a;
    const expr_t *x;
    expr_t *n = NULL;
    expr_t *candidate = NULL;
    expr_t *recognised_sum = NULL;
    expr_t *difference = NULL;
    expr_t *simplified = NULL;
    number_t s_value = NUM_NAN;
    expr_t *out = NULL;

    if (!expr || !phi || !expr_lerch_phi_unpack(phi, &z, &s, &a) ||
        !expr_is_op(z, &ops_exp) || !(x = z->a))
        return NULL;
    if (expr_is_op(x, &ops_neg) && x->a)
        x = x->a;
    s_value = expr_eval(s);
    if (!num_is_exact(s_value) || !num_is_one(s_value))
        goto cleanup;
    {
        expr_t *one = expr_new_const(NUM_ONE);
        expr_t *raw_n = one ? expr_sub(a, one) : NULL;

        n = raw_n ? expr_simplify(raw_n) : NULL;
        expr_free(raw_n);
        expr_free(one);
    }
    if (!n)
        goto cleanup;
    {
        expr_t *index = expr_new_var(NUM_NAN);
        expr_t *argument;
        expr_t *numerator;
        expr_t *term;
        expr_t *lower;
        expr_t *sum;

        if (index)
            expr_set_name(index, "k");
        argument = index ? expr_mul(index, x) : NULL;
        numerator = argument ? expr_sinh(argument) : NULL;
        term = numerator ? expr_div(numerator, index) : NULL;
        lower = expr_new_const(NUM_ONE);
        sum = term && lower ? expr_new_finite_summation_range(term, index, lower, n) : NULL;

        candidate = sum ? expr_finite_weighted_sinh_lerch_form(sum) : NULL;
        recognised_sum = sum;
        expr_free(lower);
        expr_free(term);
        expr_free(numerator);
        expr_free(argument);
        expr_free(index);
    }
    difference = candidate ? expr_sub(expr, candidate) : NULL;
    simplified = difference ? expr_simplify(difference) : NULL;
    if (!simplified || !expr_is_exact_zero(simplified))
        goto cleanup;
    out = recognised_sum;
    recognised_sum = NULL;

cleanup:
    num_destroy(&s_value);
    expr_free(simplified);
    expr_free(difference);
    expr_free(candidate);
    expr_free(recognised_sum);
    expr_free(n);
    return out;
}

/* Return whether an expression is the recognised finite weighted hyperbolic sum in Lerch-Phi form. */
bool expr_is_finite_weighted_sinh_lerch_form(const expr_t *expr)
{
    expr_t *sum = expr_finite_weighted_sinh_from_lerch_form(expr);
    bool matched = sum != NULL;

    expr_free(sum);
    return matched;
}

/* Evaluate a recognised finite weighted hyperbolic sum in Lerch-Phi form. */
bool expr_finite_weighted_sinh_lerch_value(const expr_t *expr, number_t *value_out)
{
    const expr_t *upper;
    const expr_t *step;
    expr_t *sum;
    long upper_value;

    if (!value_out || !(sum = expr_finite_weighted_sinh_from_lerch_form(expr)))
        return false;
    if (!expr_finite_weighted_sinh_parts(sum, &upper, &step) ||
        !expr_summation_bound_to_long(upper, &upper_value) || upper_value < 1L) {
        expr_free(sum);
        return false;
    }
    (void)step;
    *value_out = eval_finite_weighted_sinh(sum, upper_value);
    expr_free(sum);
    return num_is_finite(*value_out);
}

static expr_t *expr_simplify_summation_with_special_forms(const expr_t *expr, expr_t *term, expr_t *bounds)
{
    return expr_simplify_binary_operator(expr, term, bounds);
}

/* Return the closed form of a recognised finite circular or hyperbolic progression. */
expr_t *expr_finite_progression_closed_form(const expr_t *expr)
{
    const expr_t *upper;
    const expr_t *step;
    expr_finite_progression_kind_t kind;
    expr_t *upper_times_step = NULL;
    expr_t *first_argument = NULL;
    expr_t *upper_plus_one = NULL;
    expr_t *second_product = NULL;
    expr_t *second_argument = NULL;
    expr_t *first_factor = NULL;
    expr_t *second_factor = NULL;
    expr_t *numerator = NULL;
    expr_t *denominator_argument = NULL;
    expr_t *denominator = NULL;
    expr_t *closed_form = NULL;

    if (!expr_finite_progression_parts(expr, &upper, &step, &kind))
        return NULL;
    upper_times_step = expr_mul(upper, step);
    first_argument = upper_times_step ? expr_div_long(upper_times_step, 2L) : NULL;
    upper_plus_one = expr_add_long(upper, 1L);
    second_product = upper_plus_one ? expr_mul(upper_plus_one, step) : NULL;
    second_argument = second_product ? expr_div_long(second_product, 2L) : NULL;
    if (kind == EXPR_FINITE_PROGRESSION_SIN || kind == EXPR_FINITE_PROGRESSION_COS) {
        first_factor = first_argument ? expr_sin(first_argument) : NULL;
        second_factor = second_argument
                            ? (kind == EXPR_FINITE_PROGRESSION_SIN ? expr_sin(second_argument)
                                                                   : expr_cos(second_argument))
                            : NULL;
    } else {
        first_factor = first_argument ? expr_sinh(first_argument) : NULL;
        second_factor = second_argument
                            ? (kind == EXPR_FINITE_PROGRESSION_SINH ? expr_sinh(second_argument)
                                                                    : expr_cosh(second_argument))
                            : NULL;
    }
    numerator = first_factor && second_factor ? expr_mul(first_factor, second_factor) : NULL;
    denominator_argument = expr_div_long(step, 2L);
    denominator = denominator_argument
                      ? (kind == EXPR_FINITE_PROGRESSION_SIN || kind == EXPR_FINITE_PROGRESSION_COS
                             ? expr_sin(denominator_argument)
                             : expr_sinh(denominator_argument))
                      : NULL;
    closed_form = numerator && denominator ? expr_div(numerator, denominator) : NULL;

    expr_free(denominator);
    expr_free(denominator_argument);
    expr_free(numerator);
    expr_free(second_factor);
    expr_free(first_factor);
    expr_free(second_argument);
    expr_free(second_product);
    expr_free(upper_plus_one);
    expr_free(first_argument);
    expr_free(upper_times_step);
    return closed_form;
}

/* Render a native finite circular or hyperbolic summation together with its geometric-series identity. */
char *expr_finite_progression_identity_TeX(const expr_t *expr)
{
    const expr_t *upper;
    const expr_t *step;
    expr_t *symbolic_step = NULL;
    char *summation_TeX = NULL;
    char *upper_TeX = NULL;
    char *step_TeX = NULL;
    char *identity_TeX = NULL;
    expr_finite_progression_kind_t kind;

    if (!expr_finite_progression_parts(expr, &upper, &step, &kind))
        return NULL;

    if (!step->name || !*step->name)
        return NULL;
    symbolic_step = expr_new_named_var(NUM_NAN, step->name);
    summation_TeX = expr_to_TeX_body(expr);
    upper_TeX = expr_to_TeX_body(upper);
    step_TeX = symbolic_step ? expr_to_TeX_body(symbolic_step) : NULL;
    if (summation_TeX && upper_TeX && step_TeX) {
        const bool circular = kind == EXPR_FINITE_PROGRESSION_SIN || kind == EXPR_FINITE_PROGRESSION_COS;
        const char *first_function = circular ? "\\sin" : "\\sinh";
        const char *second_function = kind == EXPR_FINITE_PROGRESSION_SIN
                                          ? "\\sin"
                                      : kind == EXPR_FINITE_PROGRESSION_COS
                                          ? "\\cos"
                                      : kind == EXPR_FINITE_PROGRESSION_SINH ? "\\sinh" : "\\cosh";
        const char *format = "%s = \\frac{%s(\\frac{%s\\mkern-2mu %s}{2})\\mkern-2mu "
                             "%s(\\frac{%s}{2}\\mkern-2mu \\left(%s + 1\\right))}{%s(\\frac{%s}{2})}";
        const size_t length = (size_t)snprintf(NULL, 0, format, summation_TeX, first_function, upper_TeX,
                                               step_TeX, second_function, step_TeX, upper_TeX, first_function,
                                               step_TeX) +
                              1u;

        identity_TeX = malloc(length);
        if (identity_TeX)
            snprintf(identity_TeX, length, format, summation_TeX, first_function, upper_TeX, step_TeX,
                     second_function, step_TeX, upper_TeX, first_function, step_TeX);
    }

    free(step_TeX);
    free(upper_TeX);
    free(summation_TeX);
    expr_free(symbolic_step);
    return identity_TeX;
}

static expr_t *integrate_summation(const expr_t *expr, const expr_t *wrt)
{
    expr_t *integrated_term;
    expr_t *out;

    if (!expr || !expr->a || !expr->b || !wrt)
        return NULL;
    integrated_term = expr_integrate(expr->a, wrt);
    if (!integrated_term)
        integrated_term = expr_integral(expr->a, wrt);
    out = integrated_term ? expr_new_summation(integrated_term, expr->b) : NULL;
    expr_free(integrated_term);
    return out;
}

static expr_t *integrate_harmonic_poly(const expr_t *expr, const expr_t *wrt)
{
    expr_t *degree_derivative = NULL;
    expr_t *next_degree = NULL;
    expr_t *scaled = NULL;
    expr_t *next = NULL;
    expr_t *difference = NULL;
    expr_t *out = NULL;

    if (!expr || !expr->a || !expr->b || !wrt || (expr->b != wrt && !expr_struct_eq(expr->b, wrt)))
        return NULL;
    degree_derivative = expr_create_deriv(expr->a, wrt);
    if (!degree_derivative || !expr_is_exact_zero(degree_derivative)) {
        expr_free(degree_derivative);
        return NULL;
    }
    expr_free(degree_derivative);

    next_degree = expr_add_long(expr->a, 1L);
    scaled = expr_mul(expr->b, expr);
    next = next_degree ? expr_harmonic_poly(next_degree, expr->b) : NULL;
    difference = scaled && next ? expr_sub(scaled, next) : NULL;
    out = difference ? expr_add(difference, expr->b) : NULL;

    expr_free(difference);
    expr_free(next);
    expr_free(scaled);
    expr_free(next_degree);
    return out;
}

static expr_t *integrate_zeta_formal(const expr_t *expr, const expr_t *wrt)
{
    return expr_integral(expr, wrt);
}

static expr_t *integrate_zetap(const expr_t *expr, const expr_t *wrt)
{
    if (!expr || !wrt || !expr->a || expr->a != wrt)
        return NULL;
    return expr_zeta(wrt);
}

static expr_t *integrate_zetah(const expr_t *expr, const expr_t *wrt)
{
    expr_t *one;
    expr_t *s_minus_one;
    expr_t *one_minus_s;
    expr_t *shifted;
    expr_t *out;

    if (!expr || !wrt || !expr->a || !expr->b)
        return NULL;
    if (expr->b != wrt && !expr_struct_eq(expr->b, wrt))
        return expr_integral(expr, wrt);
    one = expr_new_const(NUM_ONE);
    s_minus_one = expr_sub(expr->a, one);
    one_minus_s = expr_sub(one, expr->a);
    shifted = expr_zetah(s_minus_one, expr->b);
    out = expr_div(shifted, one_minus_s);
    expr_free(shifted);
    expr_free(one_minus_s);
    expr_free(s_minus_one);
    expr_free(one);
    return out;
}

static expr_t *integrate_zatahp(const expr_t *expr, const expr_t *wrt)
{
    if (!expr || !wrt || !expr->a || !expr->b || (expr->a != wrt && !expr_struct_eq(expr->a, wrt)))
        return expr_integral(expr, wrt);
    return expr_zetah(expr->a, expr->b);
}

static expr_t *expr_inverse_log10_internal(const expr_t *a)
{
    expr_t *ten = expr_new_const(NUM_TEN);
    expr_t *out = expr_pow_xp(ten, a);

    expr_free(ten);
    return out;
}

static expr_t *expr_inverse_sqrt_internal(const expr_t *a)
{
    return expr_pow(a, &NUM_TWO);
}

static expr_t *expr_inverse_lambert_internal(const expr_t *a)
{
    expr_t *exp_a = expr_exp(a);
    expr_t *out = exp_a ? expr_mul(a, exp_a) : NULL;

    expr_free(exp_a);
    return out;
}

const expr_ops_t ops_atan2 = {.eval = eval_atan2,
                              .deriv = deriv_atan2,
                              .reverse = expr_reverse_atan2,
                              .kind = EXPR_KIND_ATAN2,
                              .arity = EXPR_OP_BINARY,
                              .name = "atan2",
                              .TeX_name = "\\operatorname{atan2}",
                              .apply_unary = NULL,
                              .apply_binary = expr_atan2,
                              .simplify = expr_simplify_binary_operator,
                              .fold_const_unary = NULL};

const expr_ops_t ops_sin = {.eval = eval_sin,
                            .deriv = deriv_sin,
                            .reverse = expr_reverse_sin,
                            .kind = EXPR_KIND_SIN,
                            .arity = EXPR_OP_UNARY,
                            .name = "sin",
                            .TeX_name = "\\sin",
                            .direct_inverse = &ops_asin,
                            .inverse_unary = expr_asin,
                            .apply_unary = expr_sin,
                            .apply_binary = NULL,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = expr_fold_zero_to_zero};
const expr_ops_t ops_cos = {.eval = eval_cos,
                            .deriv = deriv_cos,
                            .reverse = expr_reverse_cos,
                            .kind = EXPR_KIND_COS,
                            .arity = EXPR_OP_UNARY,
                            .name = "cos",
                            .TeX_name = "\\cos",
                            .direct_inverse = &ops_acos,
                            .inverse_unary = expr_acos,
                            .apply_unary = expr_cos,
                            .apply_binary = NULL,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = expr_fold_cos_const};
const expr_ops_t ops_tan = {.eval = eval_tan,
                            .deriv = deriv_tan,
                            .reverse = expr_reverse_tan,
                            .kind = EXPR_KIND_TAN,
                            .arity = EXPR_OP_UNARY,
                            .name = "tan",
                            .TeX_name = "\\tan",
                            .direct_inverse = &ops_atan,
                            .inverse_unary = expr_atan,
                            .apply_unary = expr_tan,
                            .apply_binary = NULL,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = expr_fold_zero_to_zero};
const expr_ops_t ops_sec = {.eval = eval_sec,
                            .deriv = deriv_sec,
                            .reverse = expr_reverse_sec,
                            .kind = EXPR_KIND_SEC,
                            .arity = EXPR_OP_UNARY,
                            .name = "sec",
                            .TeX_name = "\\sec",
                            .direct_inverse = &ops_asec,
                            .inverse_unary = expr_asec,
                            .apply_unary = expr_sec,
                            .apply_binary = NULL,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_cosec = {.eval = eval_cosec,
                              .deriv = deriv_cosec,
                              .reverse = expr_reverse_cosec,
                              .kind = EXPR_KIND_COSEC,
                              .arity = EXPR_OP_UNARY,
                              .name = "cosec",
                              .TeX_name = "\\operatorname{cosec}",
                              .direct_inverse = &ops_acosec,
                              .inverse_unary = expr_acosec,
                              .apply_unary = expr_cosec,
                              .apply_binary = NULL,
                              .integrate = expr_integrate_dispatch_primitive,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_cot = {.eval = eval_cot,
                            .deriv = deriv_cot,
                            .reverse = expr_reverse_cot,
                            .kind = EXPR_KIND_COT,
                            .arity = EXPR_OP_UNARY,
                            .name = "cot",
                            .TeX_name = "\\cot",
                            .direct_inverse = &ops_acot,
                            .inverse_unary = expr_acot,
                            .apply_unary = expr_cot,
                            .apply_binary = NULL,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_versin = {.eval = eval_versin,
                               .deriv = deriv_versin,
                               .reverse = expr_reverse_versin,
                               .kind = EXPR_KIND_VERSIN,
                               .arity = EXPR_OP_UNARY,
                               .name = "versin",
                               .TeX_name = "\\operatorname{versin}",
                               .direct_inverse = &ops_arcversin,
                               .inverse_unary = expr_arcversin,
                               .apply_unary = expr_versin,
                               .apply_binary = NULL,
                               .simplify = expr_simplify_unary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_vercos = {.eval = eval_vercos,
                               .deriv = deriv_vercos,
                               .reverse = expr_reverse_vercos,
                               .kind = EXPR_KIND_VERCOS,
                               .arity = EXPR_OP_UNARY,
                               .name = "vercos",
                               .TeX_name = "\\operatorname{vercos}",
                               .direct_inverse = &ops_arcvercos,
                               .inverse_unary = expr_arcvercos,
                               .apply_unary = expr_vercos,
                               .apply_binary = NULL,
                               .simplify = expr_simplify_unary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_coversin = {.eval = eval_coversin,
                                 .deriv = deriv_coversin,
                                 .reverse = expr_reverse_coversin,
                                 .kind = EXPR_KIND_COVERSIN,
                                 .arity = EXPR_OP_UNARY,
                                 .name = "coversin",
                                 .TeX_name = "\\operatorname{coversin}",
                                 .direct_inverse = &ops_arccoversin,
                                 .inverse_unary = expr_arccoversin,
                                 .apply_unary = expr_coversin,
                                 .apply_binary = NULL,
                                 .simplify = expr_simplify_unary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_covercos = {.eval = eval_covercos,
                                 .deriv = deriv_covercos,
                                 .reverse = expr_reverse_covercos,
                                 .kind = EXPR_KIND_COVERCOS,
                                 .arity = EXPR_OP_UNARY,
                                 .name = "covercos",
                                 .TeX_name = "\\operatorname{covercos}",
                                 .direct_inverse = &ops_arccovercos,
                                 .inverse_unary = expr_arccovercos,
                                 .apply_unary = expr_covercos,
                                 .apply_binary = NULL,
                                 .simplify = expr_simplify_unary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_haversin = {.eval = eval_haversin,
                                 .deriv = deriv_haversin,
                                 .reverse = expr_reverse_haversin,
                                 .kind = EXPR_KIND_HAVERSIN,
                                 .arity = EXPR_OP_UNARY,
                                 .name = "haversin",
                                 .TeX_name = "\\operatorname{haversin}",
                                 .direct_inverse = &ops_archaversin,
                                 .inverse_unary = expr_archaversin,
                                 .apply_unary = expr_haversin,
                                 .apply_binary = NULL,
                                 .simplify = expr_simplify_unary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_havercos = {.eval = eval_havercos,
                                 .deriv = deriv_havercos,
                                 .reverse = expr_reverse_havercos,
                                 .kind = EXPR_KIND_HAVERCOS,
                                 .arity = EXPR_OP_UNARY,
                                 .name = "havercos",
                                 .TeX_name = "\\operatorname{havercos}",
                                 .direct_inverse = &ops_archavercos,
                                 .inverse_unary = expr_archavercos,
                                 .apply_unary = expr_havercos,
                                 .apply_binary = NULL,
                                 .simplify = expr_simplify_unary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_hacoversin = {.eval = eval_hacoversin,
                                   .deriv = deriv_hacoversin,
                                   .reverse = expr_reverse_hacoversin,
                                   .kind = EXPR_KIND_HACOVERSIN,
                                   .arity = EXPR_OP_UNARY,
                                   .name = "hacoversin",
                                   .TeX_name = "\\operatorname{hacoversin}",
                                   .direct_inverse = &ops_archacoversin,
                                   .inverse_unary = expr_archacoversin,
                                   .apply_unary = expr_hacoversin,
                                   .apply_binary = NULL,
                                   .simplify = expr_simplify_unary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_hacovercos = {.eval = eval_hacovercos,
                                   .deriv = deriv_hacovercos,
                                   .reverse = expr_reverse_hacovercos,
                                   .kind = EXPR_KIND_HACOVERCOS,
                                   .arity = EXPR_OP_UNARY,
                                   .name = "hacovercos",
                                   .TeX_name = "\\operatorname{hacovercos}",
                                   .direct_inverse = &ops_archacovercos,
                                   .inverse_unary = expr_archacovercos,
                                   .apply_unary = expr_hacovercos,
                                   .apply_binary = NULL,
                                   .simplify = expr_simplify_unary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_sinh = {.eval = eval_sinh,
                             .deriv = deriv_sinh,
                             .reverse = expr_reverse_sinh,
                             .kind = EXPR_KIND_SINH,
                             .arity = EXPR_OP_UNARY,
                             .name = "sinh",
                             .TeX_name = "\\sinh",
                             .direct_inverse = &ops_asinh,
                             .inverse_unary = expr_asinh,
                             .apply_unary = expr_sinh,
                             .apply_binary = NULL,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_cosh = {.eval = eval_cosh,
                             .deriv = deriv_cosh,
                             .reverse = expr_reverse_cosh,
                             .kind = EXPR_KIND_COSH,
                             .arity = EXPR_OP_UNARY,
                             .name = "cosh",
                             .TeX_name = "\\cosh",
                             .direct_inverse = &ops_acosh,
                             .inverse_unary = expr_acosh,
                             .apply_unary = expr_cosh,
                             .apply_binary = NULL,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_tanh = {.eval = eval_tanh,
                             .deriv = deriv_tanh,
                             .reverse = expr_reverse_tanh,
                             .kind = EXPR_KIND_TANH,
                             .arity = EXPR_OP_UNARY,
                             .name = "tanh",
                             .TeX_name = "\\tanh",
                             .direct_inverse = &ops_atanh,
                             .inverse_unary = expr_atanh,
                             .apply_unary = expr_tanh,
                             .apply_binary = NULL,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_sech = {.eval = eval_sech,
                             .deriv = deriv_sech,
                             .reverse = expr_reverse_sech,
                             .kind = EXPR_KIND_SECH,
                             .arity = EXPR_OP_UNARY,
                             .name = "sech",
                             .TeX_name = "\\operatorname{sech}",
                             .direct_inverse = &ops_asech,
                             .inverse_unary = expr_asech,
                             .apply_unary = expr_sech,
                             .apply_binary = NULL,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_cosech = {.eval = eval_cosech,
                               .deriv = deriv_cosech,
                               .reverse = expr_reverse_cosech,
                               .kind = EXPR_KIND_COSECH,
                               .arity = EXPR_OP_UNARY,
                               .name = "cosech",
                               .TeX_name = "\\operatorname{cosech}",
                               .direct_inverse = &ops_acosech,
                               .inverse_unary = expr_acosech,
                               .apply_unary = expr_cosech,
                               .apply_binary = NULL,
                               .integrate = expr_integrate_dispatch_primitive,
                               .simplify = expr_simplify_unary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_coth = {.eval = eval_coth,
                             .deriv = deriv_coth,
                             .reverse = expr_reverse_coth,
                             .kind = EXPR_KIND_COTH,
                             .arity = EXPR_OP_UNARY,
                             .name = "coth",
                             .TeX_name = "\\coth",
                             .direct_inverse = &ops_acoth,
                             .inverse_unary = expr_acoth,
                             .apply_unary = expr_coth,
                             .apply_binary = NULL,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_asin = {.eval = eval_asin,
                             .deriv = deriv_asin,
                             .reverse = expr_reverse_asin,
                             .kind = EXPR_KIND_ASIN,
                             .arity = EXPR_OP_UNARY,
                             .name = "asin",
                             .TeX_name = "\\sin^{-1}",
                             .inverse_unary = expr_sin,
                             .apply_unary = expr_asin,
                             .apply_binary = NULL,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = expr_fold_asin_const};
const expr_ops_t ops_acos = {.eval = eval_acos,
                             .deriv = deriv_acos,
                             .reverse = expr_reverse_acos,
                             .kind = EXPR_KIND_ACOS,
                             .arity = EXPR_OP_UNARY,
                             .name = "acos",
                             .TeX_name = "\\cos^{-1}",
                             .inverse_unary = expr_cos,
                             .apply_unary = expr_acos,
                             .apply_binary = NULL,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = expr_fold_acos_const};
const expr_ops_t ops_atan = {.eval = eval_atan,
                             .deriv = deriv_atan,
                             .reverse = expr_reverse_atan,
                             .kind = EXPR_KIND_ATAN,
                             .arity = EXPR_OP_UNARY,
                             .name = "atan",
                             .TeX_name = "\\arctan",
                             .inverse_unary = expr_tan,
                             .apply_unary = expr_atan,
                             .apply_binary = NULL,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = expr_fold_atan_const};
const expr_ops_t ops_asec = {.eval = eval_asec,
                             .deriv = deriv_asec,
                             .reverse = expr_reverse_asec,
                             .kind = EXPR_KIND_ASEC,
                             .arity = EXPR_OP_UNARY,
                             .name = "asec",
                             .TeX_name = "\\sec^{-1}",
                             .inverse_unary = expr_sec,
                             .apply_unary = expr_asec,
                             .apply_binary = NULL,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = expr_fold_asec_const};
const expr_ops_t ops_acosec = {.eval = eval_acosec,
                               .deriv = deriv_acosec,
                               .reverse = expr_reverse_acosec,
                               .kind = EXPR_KIND_ACOSEC,
                               .arity = EXPR_OP_UNARY,
                               .name = "acosec",
                               .TeX_name = "\\operatorname{cosec}^{-1}",
                               .inverse_unary = expr_cosec,
                               .apply_unary = expr_acosec,
                               .apply_binary = NULL,
                               .integrate = expr_integrate_dispatch_primitive,
                               .simplify = expr_simplify_unary_operator,
                               .fold_const_unary = expr_fold_acosec_const};
const expr_ops_t ops_acot = {.eval = eval_acot,
                             .deriv = deriv_acot,
                             .reverse = expr_reverse_acot,
                             .kind = EXPR_KIND_ACOT,
                             .arity = EXPR_OP_UNARY,
                             .name = "acot",
                             .TeX_name = "\\cot^{-1}",
                             .inverse_unary = expr_cot,
                             .apply_unary = expr_acot,
                             .apply_binary = NULL,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = expr_fold_acot_const};
const expr_ops_t ops_arcversin = {.eval = eval_arcversin,
                                  .deriv = deriv_arcversin,
                                  .reverse = expr_reverse_arcversin,
                                  .kind = EXPR_KIND_ARCVERSIN,
                                  .arity = EXPR_OP_UNARY,
                                  .name = "arcversin",
                                  .TeX_name = "\\operatorname{arcversin}",
                                  .inverse_unary = expr_versin,
                                  .apply_unary = expr_arcversin,
                                  .apply_binary = NULL,
                                  .simplify = expr_simplify_unary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_arcvercos = {.eval = eval_arcvercos,
                                  .deriv = deriv_arcvercos,
                                  .reverse = expr_reverse_arcvercos,
                                  .kind = EXPR_KIND_ARCVERCOS,
                                  .arity = EXPR_OP_UNARY,
                                  .name = "arcvercos",
                                  .TeX_name = "\\operatorname{arcvercos}",
                                  .inverse_unary = expr_vercos,
                                  .apply_unary = expr_arcvercos,
                                  .apply_binary = NULL,
                                  .simplify = expr_simplify_unary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_arccoversin = {.eval = eval_arccoversin,
                                    .deriv = deriv_arccoversin,
                                    .reverse = expr_reverse_arccoversin,
                                    .kind = EXPR_KIND_ARCCOVERSIN,
                                    .arity = EXPR_OP_UNARY,
                                    .name = "arccoversin",
                                    .TeX_name = "\\operatorname{arccoversin}",
                                    .inverse_unary = expr_coversin,
                                    .apply_unary = expr_arccoversin,
                                    .apply_binary = NULL,
                                    .simplify = expr_simplify_unary_operator,
                                    .fold_const_unary = NULL};
const expr_ops_t ops_arccovercos = {.eval = eval_arccovercos,
                                    .deriv = deriv_arccovercos,
                                    .reverse = expr_reverse_arccovercos,
                                    .kind = EXPR_KIND_ARCCOVERCOS,
                                    .arity = EXPR_OP_UNARY,
                                    .name = "arccovercos",
                                    .TeX_name = "\\operatorname{arccovercos}",
                                    .inverse_unary = expr_covercos,
                                    .apply_unary = expr_arccovercos,
                                    .apply_binary = NULL,
                                    .simplify = expr_simplify_unary_operator,
                                    .fold_const_unary = NULL};
const expr_ops_t ops_archaversin = {.eval = eval_archaversin,
                                    .deriv = deriv_archaversin,
                                    .reverse = expr_reverse_archaversin,
                                    .kind = EXPR_KIND_ARCHAVERSIN,
                                    .arity = EXPR_OP_UNARY,
                                    .name = "archaversin",
                                    .TeX_name = "\\operatorname{archaversin}",
                                    .inverse_unary = expr_haversin,
                                    .apply_unary = expr_archaversin,
                                    .apply_binary = NULL,
                                    .simplify = expr_simplify_unary_operator,
                                    .fold_const_unary = NULL};
const expr_ops_t ops_archavercos = {.eval = eval_archavercos,
                                    .deriv = deriv_archavercos,
                                    .reverse = expr_reverse_archavercos,
                                    .kind = EXPR_KIND_ARCHAVERCOS,
                                    .arity = EXPR_OP_UNARY,
                                    .name = "archavercos",
                                    .TeX_name = "\\operatorname{archavercos}",
                                    .inverse_unary = expr_havercos,
                                    .apply_unary = expr_archavercos,
                                    .apply_binary = NULL,
                                    .simplify = expr_simplify_unary_operator,
                                    .fold_const_unary = NULL};
const expr_ops_t ops_archacoversin = {.eval = eval_archacoversin,
                                      .deriv = deriv_archacoversin,
                                      .reverse = expr_reverse_archacoversin,
                                      .kind = EXPR_KIND_ARCHACOVERSIN,
                                      .arity = EXPR_OP_UNARY,
                                      .name = "archacoversin",
                                      .TeX_name = "\\operatorname{archacoversin}",
                                      .inverse_unary = expr_hacoversin,
                                      .apply_unary = expr_archacoversin,
                                      .apply_binary = NULL,
                                      .simplify = expr_simplify_unary_operator,
                                      .fold_const_unary = NULL};
const expr_ops_t ops_archacovercos = {.eval = eval_archacovercos,
                                      .deriv = deriv_archacovercos,
                                      .reverse = expr_reverse_archacovercos,
                                      .kind = EXPR_KIND_ARCHACOVERCOS,
                                      .arity = EXPR_OP_UNARY,
                                      .name = "archacovercos",
                                      .TeX_name = "\\operatorname{archacovercos}",
                                      .inverse_unary = expr_hacovercos,
                                      .apply_unary = expr_archacovercos,
                                      .apply_binary = NULL,
                                      .simplify = expr_simplify_unary_operator,
                                      .fold_const_unary = NULL};
const expr_ops_t ops_asinh = {.eval = eval_asinh,
                              .deriv = deriv_asinh,
                              .reverse = expr_reverse_asinh,
                              .kind = EXPR_KIND_ASINH,
                              .arity = EXPR_OP_UNARY,
                              .name = "asinh",
                              .TeX_name = "\\sinh^{-1}",
                              .inverse_unary = expr_sinh,
                              .apply_unary = expr_asinh,
                              .apply_binary = NULL,
                              .integrate = expr_integrate_dispatch_primitive,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_acosh = {.eval = eval_acosh,
                              .deriv = deriv_acosh,
                              .reverse = expr_reverse_acosh,
                              .kind = EXPR_KIND_ACOSH,
                              .arity = EXPR_OP_UNARY,
                              .name = "acosh",
                              .TeX_name = "\\cosh^{-1}",
                              .inverse_unary = expr_cosh,
                              .apply_unary = expr_acosh,
                              .apply_binary = NULL,
                              .integrate = expr_integrate_dispatch_primitive,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_atanh = {.eval = eval_atanh,
                              .deriv = deriv_atanh,
                              .reverse = expr_reverse_atanh,
                              .kind = EXPR_KIND_ATANH,
                              .arity = EXPR_OP_UNARY,
                              .name = "atanh",
                              .TeX_name = "\\tanh^{-1}",
                              .inverse_unary = expr_tanh,
                              .apply_unary = expr_atanh,
                              .apply_binary = NULL,
                              .integrate = expr_integrate_dispatch_primitive,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_asech = {.eval = eval_asech,
                              .deriv = deriv_asech,
                              .reverse = expr_reverse_asech,
                              .kind = EXPR_KIND_ASECH,
                              .arity = EXPR_OP_UNARY,
                              .name = "asech",
                              .TeX_name = "\\operatorname{sech}^{-1}",
                              .inverse_unary = expr_sech,
                              .apply_unary = expr_asech,
                              .apply_binary = NULL,
                              .integrate = expr_integrate_dispatch_primitive,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_acosech = {.eval = eval_acosech,
                                .deriv = deriv_acosech,
                                .reverse = expr_reverse_acosech,
                                .kind = EXPR_KIND_ACOSECH,
                                .arity = EXPR_OP_UNARY,
                                .name = "acosech",
                                .TeX_name = "\\operatorname{cosech}^{-1}",
                                .inverse_unary = expr_cosech,
                                .apply_unary = expr_acosech,
                                .apply_binary = NULL,
                                .integrate = expr_integrate_dispatch_primitive,
                                .simplify = expr_simplify_unary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_acoth = {.eval = eval_acoth,
                              .deriv = deriv_acoth,
                              .reverse = expr_reverse_acoth,
                              .kind = EXPR_KIND_ACOTH,
                              .arity = EXPR_OP_UNARY,
                              .name = "acoth",
                              .TeX_name = "\\coth^{-1}",
                              .inverse_unary = expr_coth,
                              .apply_unary = expr_acoth,
                              .apply_binary = NULL,
                              .integrate = expr_integrate_dispatch_primitive,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_exp = {.eval = eval_exp,
                            .deriv = deriv_exp,
                            .reverse = expr_reverse_exp,
                            .kind = EXPR_KIND_EXP,
                            .arity = EXPR_OP_UNARY,
                            .name = "exp",
                            .TeX_name = "\\exp",
                            .direct_inverse = &ops_log,
                            .inverse_unary = expr_log,
                            .apply_unary = expr_exp,
                            .apply_binary = NULL,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = expr_fold_exp_const};
const expr_ops_t ops_log = {.eval = eval_log,
                            .deriv = deriv_log,
                            .reverse = expr_reverse_log,
                            .kind = EXPR_KIND_LOG,
                            .arity = EXPR_OP_UNARY,
                            .name = "ln",
                            .TeX_name = "\\ln",
                            .direct_inverse = &ops_exp,
                            .inverse_unary = expr_exp,
                            .apply_unary = expr_log,
                            .apply_binary = NULL,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = expr_fold_log_const};
const expr_ops_t ops_log10 = {.eval = eval_log10,
                              .deriv = deriv_log10,
                              .reverse = expr_reverse_log10,
                              .kind = EXPR_KIND_LOG10,
                              .arity = EXPR_OP_UNARY,
                              .name = "lg",
                              .TeX_name = "\\lg",
                              .inverse_unary = expr_inverse_log10_internal,
                              .apply_unary = expr_log10,
                              .apply_binary = NULL,
                              .integrate = expr_integrate_dispatch_primitive,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_sqrt = {.eval = eval_sqrt,
                             .deriv = deriv_sqrt,
                             .reverse = expr_reverse_sqrt,
                             .kind = EXPR_KIND_SQRT,
                             .arity = EXPR_OP_UNARY,
                             .name = "sqrt",
                             .TeX_name = "\\sqrt",
                             .inverse_unary = expr_inverse_sqrt_internal,
                             .apply_unary = expr_sqrt,
                             .apply_binary = NULL,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = expr_fold_sqrt_const};
const expr_ops_t ops_cubrt = {.eval = eval_cubrt,
                              .deriv = deriv_cubrt,
                              .reverse = expr_reverse_cubrt,
                              .kind = EXPR_KIND_CUBRT,
                              .arity = EXPR_OP_UNARY,
                              .name = "cubrt",
                              .TeX_name = "\\sqrt[3]",
                              .apply_unary = expr_cubrt,
                              .apply_binary = NULL,
                              .integrate = expr_integrate_dispatch_primitive,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = expr_fold_cubrt_const};
const expr_ops_t ops_root = {.eval = eval_root,
                             .deriv = deriv_root,
                             .reverse = expr_reverse_root,
                             .kind = EXPR_KIND_ROOT,
                             .arity = EXPR_OP_BINARY,
                             .name = "root",
                             .TeX_name = "\\sqrt",
                             .apply_unary = NULL,
                             .apply_binary = expr_root,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_root_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_floor = {.eval = eval_floor,
                              .deriv = deriv_floor,
                              .reverse = expr_reverse_floor,
                              .kind = EXPR_KIND_FLOOR,
                              .arity = EXPR_OP_UNARY,
                              .name = "floor",
                              .TeX_name = "\\lfloor",
                              .apply_unary = expr_floor,
                              .apply_binary = NULL,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = expr_fold_floor_const};
const expr_ops_t ops_ceil = {.eval = eval_ceil,
                             .deriv = deriv_ceil,
                             .reverse = expr_reverse_ceil,
                             .kind = EXPR_KIND_CEIL,
                             .arity = EXPR_OP_UNARY,
                             .name = "ceil",
                             .TeX_name = "\\lceil",
                             .apply_unary = expr_ceil,
                             .apply_binary = NULL,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_abs = {.eval = eval_abs,
                            .deriv = deriv_abs,
                            .reverse = expr_reverse_abs,
                            .kind = EXPR_KIND_ABS,
                            .arity = EXPR_OP_UNARY,
                            .name = "abs",
                            .TeX_name = NULL,
                            .apply_unary = expr_abs,
                            .apply_binary = NULL,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_conj = {.eval = eval_conj,
                             .deriv = deriv_conj,
                             .reverse = expr_reverse_conj,
                             .kind = EXPR_KIND_CONJ,
                             .arity = EXPR_OP_UNARY,
                             .name = "conj",
                             .TeX_name = "\\operatorname{conj}",
                             .direct_inverse = &ops_conj,
                             .inverse_unary = expr_conj,
                             .apply_unary = expr_conj,
                             .apply_binary = NULL,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_erf = {.eval = eval_erf,
                            .deriv = deriv_erf,
                            .reverse = expr_reverse_erf,
                            .kind = EXPR_KIND_ERF,
                            .arity = EXPR_OP_UNARY,
                            .name = "erf",
                            .TeX_name = "\\operatorname{erf}",
                            .direct_inverse = &ops_erfinv,
                            .inverse_unary = expr_erfinv,
                            .apply_unary = expr_erf,
                            .apply_binary = NULL,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = expr_fold_erf_const};
const expr_ops_t ops_erfc = {.eval = eval_erfc,
                             .deriv = deriv_erfc,
                             .reverse = expr_reverse_erfc,
                             .kind = EXPR_KIND_ERFC,
                             .arity = EXPR_OP_UNARY,
                             .name = "erfc",
                             .TeX_name = "\\operatorname{erfc}",
                             .direct_inverse = &ops_erfcinv,
                             .inverse_unary = expr_erfcinv,
                             .apply_unary = expr_erfc,
                             .apply_binary = NULL,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = expr_fold_erfc_const};
const expr_ops_t ops_lgamma = {.eval = eval_lgamma,
                               .deriv = deriv_lgamma,
                               .reverse = expr_reverse_lgamma,
                               .kind = EXPR_KIND_LGAMMA,
                               .arity = EXPR_OP_UNARY,
                               .name = "lgamma",
                               .TeX_name = "\\log\\Gamma",
                               .apply_unary = expr_lgamma,
                               .apply_binary = NULL,
                               .simplify = expr_simplify_unary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_hypot = {.eval = eval_hypot,
                              .deriv = deriv_hypot,
                              .reverse = expr_reverse_hypot,
                              .kind = EXPR_KIND_HYPOT,
                              .arity = EXPR_OP_BINARY,
                              .name = "hypot",
                              .TeX_name = "\\operatorname{hypot}",
                              .apply_unary = NULL,
                              .apply_binary = expr_hypot,
                              .simplify = expr_simplify_hypot_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_erfinv = {.eval = eval_erfinv,
                               .deriv = deriv_erfinv,
                               .reverse = expr_reverse_erfinv,
                               .kind = EXPR_KIND_ERFINV,
                               .arity = EXPR_OP_UNARY,
                               .name = "erfinv",
                               .TeX_name = "\\operatorname{erf}^{-1}",
                               .inverse_unary = expr_erf,
                               .apply_unary = expr_erfinv,
                               .apply_binary = NULL,
                               .simplify = expr_simplify_unary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_erfcinv = {.eval = eval_erfcinv,
                                .deriv = deriv_erfcinv,
                                .reverse = expr_reverse_erfcinv,
                                .kind = EXPR_KIND_ERFCINV,
                                .arity = EXPR_OP_UNARY,
                                .name = "erfcinv",
                                .TeX_name = "\\operatorname{erfc}^{-1}",
                                .inverse_unary = expr_erfc,
                                .apply_unary = expr_erfcinv,
                                .apply_binary = NULL,
                                .simplify = expr_simplify_unary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_gamma = {.eval = eval_gamma,
                              .deriv = deriv_gamma,
                              .reverse = expr_reverse_gamma,
                              .kind = EXPR_KIND_GAMMA,
                              .arity = EXPR_OP_UNARY,
                              .name = "gamma",
                              .TeX_name = "\\Gamma",
                              .direct_inverse = &ops_gammainv,
                              .inverse_unary = expr_gammainv,
                              .apply_unary = expr_gamma,
                              .apply_binary = NULL,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_digamma = {.eval = eval_digamma,
                                .deriv = deriv_digamma,
                                .reverse = expr_reverse_digamma,
                                .kind = EXPR_KIND_DIGAMMA,
                                .arity = EXPR_OP_UNARY,
                                .name = "digamma",
                                .TeX_name = "\\psi^{(0)}",
                                .apply_unary = expr_digamma,
                                .apply_binary = NULL,
                                .simplify = expr_simplify_unary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_trigamma = {.eval = eval_trigamma,
                                 .deriv = deriv_trigamma,
                                 .reverse = expr_reverse_trigamma,
                                 .kind = EXPR_KIND_TRIGAMMA,
                                 .arity = EXPR_OP_UNARY,
                                 .name = "trigamma",
                                 .TeX_name = "\\psi^{(1)}",
                                 .apply_unary = expr_trigamma,
                                 .apply_binary = NULL,
                                 .simplify = expr_simplify_unary_operator,
                                 .fold_const_unary = expr_fold_trigamma_const};
const expr_ops_t ops_polygamma = {.eval = eval_polygamma,
                                  .deriv = deriv_polygamma,
                                  .reverse = expr_reverse_polygamma,
                                  .kind = EXPR_KIND_POLYGAMMA,
                                  .arity = EXPR_OP_BINARY,
                                  .name = "polygamma",
                                  .TeX_name = "\\psi",
                                  .apply_unary = NULL,
                                  .apply_binary = expr_polygamma_xp,
                                  .simplify = expr_simplify_binary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_zeta = {.eval = eval_zeta,
                             .deriv = deriv_zeta,
                             .reverse = expr_reverse_zeta,
                             .kind = EXPR_KIND_ZETA,
                             .arity = EXPR_OP_UNARY,
                             .name = "zeta",
                             .TeX_name = "\\zeta",
                             .apply_unary = expr_zeta,
                             .apply_binary = NULL,
                             .integrate = integrate_zeta_formal,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_zetap = {.eval = eval_zetap,
                              .deriv = deriv_zetap,
                              .kind = EXPR_KIND_ZETAP,
                              .arity = EXPR_OP_UNARY,
                              .name = "zetap",
                              .TeX_name = "\\zeta'",
                              .apply_unary = expr_zetap,
                              .apply_binary = NULL,
                              .integrate = integrate_zetap,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_zetah = {.eval = eval_zetah,
                              .deriv = deriv_zetah,
                              .kind = EXPR_KIND_ZETAH,
                              .arity = EXPR_OP_BINARY,
                              .name = "zetah",
                              .TeX_name = "\\zeta",
                              .apply_unary = NULL,
                              .apply_binary = expr_zetah,
                              .integrate = integrate_zetah,
                              .simplify = expr_simplify_binary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_zatahp = {.eval = eval_zatahp,
                               .deriv = deriv_zatahp,
                               .kind = EXPR_KIND_ZATAHP,
                               .arity = EXPR_OP_BINARY,
                               .name = "zatahp",
                               .TeX_name = "\\zeta'",
                               .apply_unary = NULL,
                               .apply_binary = expr_zatahp,
                               .integrate = integrate_zatahp,
                               .simplify = expr_simplify_binary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_dilog = {.eval = eval_dilog,
                              .deriv = deriv_dilog,
                              .reverse = expr_reverse_dilog,
                              .kind = EXPR_KIND_DILOG,
                              .arity = EXPR_OP_UNARY,
                              .name = "dilog",
                              .TeX_name = "\\operatorname{Li}_{2}",
                              .apply_unary = expr_dilog,
                              .apply_binary = NULL,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_polylog1 = {.eval = eval_polylog1,
                                 .deriv = deriv_polylog1,
                                 .reverse = expr_reverse_polylog1,
                                 .kind = EXPR_KIND_POLYLOG1,
                                 .arity = EXPR_OP_UNARY,
                                 .name = "Li1",
                                 .TeX_name = "\\operatorname{Li}_{1}",
                                 .apply_unary = expr_polylog1,
                                 .apply_binary = NULL,
                                 .integrate = integrate_polylog1,
                                 .simplify = expr_simplify_unary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_polylog = {.eval = eval_polylog,
                                .deriv = deriv_polylog,
                                .reverse = expr_reverse_polylog,
                                .kind = EXPR_KIND_POLYLOG,
                                .arity = EXPR_OP_BINARY,
                                .name = "polylog",
                                .TeX_name = "\\operatorname{Li}",
                                .apply_unary = NULL,
                                .apply_binary = expr_polylog_xp,
                                .simplify = expr_simplify_binary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_harmonic_poly = {.eval = eval_harmonic_poly,
                                      .deriv = deriv_harmonic_poly,
                                      .kind = EXPR_KIND_HARMONIC_POLY,
                                      .arity = EXPR_OP_BINARY,
                                      .name = "Hn",
                                      .TeX_name = "H",
                                      .apply_unary = NULL,
                                      .apply_binary = expr_harmonic_poly,
                                      .integrate = integrate_harmonic_poly,
                                      .simplify = expr_simplify_binary_operator,
                                      .fold_const_unary = NULL};
const expr_ops_t ops_lerch_phi = {.eval = eval_lerch_phi,
                                  .deriv = deriv_lerch_phi,
                                  .reverse_many = expr_reverse_lerch_phi_many,
                                  .kind = EXPR_KIND_LERCH_PHI,
                                  .arity = EXPR_OP_BINARY,
                                  .name = "LerchPhi",
                                  .TeX_name = "\\Phi",
                                  .apply_unary = NULL,
                                  .apply_binary = NULL,
                                  .integrate = expr_integrate_dispatch_primitive,
                                  .simplify = expr_simplify_rebuild_binary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_lerch_phi_pack = {.eval = eval_lerch_phi_pack,
                                       .deriv = deriv_lerch_phi_pack,
                                       .reverse = expr_reverse_parameter_pack,
                                       .kind = EXPR_KIND_LERCH_PHI_PACK,
                                       .arity = EXPR_OP_BINARY,
                                       .name = "lerch_phi_pack",
                                       .TeX_name = "\\operatorname{pack}",
                                       .apply_unary = NULL,
                                       .apply_binary = NULL,
                                       .simplify = expr_simplify_rebuild_binary_operator,
                                       .fold_const_unary = NULL};
const expr_ops_t ops_legendre_chi = {.eval = eval_legendre_chi,
                                     .deriv = deriv_legendre_chi,
                                     .reverse = expr_reverse_legendre_chi,
                                     .kind = EXPR_KIND_LEGENDRE_CHI,
                                     .arity = EXPR_OP_BINARY,
                                     .name = "legendre_chi",
                                     .TeX_name = "\\chi",
                                     .apply_unary = NULL,
                                     .apply_binary = expr_legendre_chi_xp,
                                     .simplify = expr_simplify_binary_operator,
                                     .fold_const_unary = NULL};
const expr_ops_t ops_bessel_j = {.eval = eval_bessel_j,
                                 .deriv = deriv_bessel_j,
                                 .reverse = expr_reverse_bessel_j,
                                 .kind = EXPR_KIND_BESSEL_J,
                                 .arity = EXPR_OP_BINARY,
                                 .name = "BesselJ",
                                 .TeX_name = "J",
                                 .apply_unary = NULL,
                                 .apply_binary = expr_bessel_j,
                                 .integrate = expr_integrate_dispatch_primitive,
                                 .simplify = expr_simplify_binary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_bessel_y = {.eval = eval_bessel_y,
                                 .deriv = deriv_bessel_y,
                                 .reverse = expr_reverse_bessel_y,
                                 .kind = EXPR_KIND_BESSEL_Y,
                                 .arity = EXPR_OP_BINARY,
                                 .name = "BesselY",
                                 .TeX_name = "Y",
                                 .apply_unary = NULL,
                                 .apply_binary = expr_bessel_y,
                                 .integrate = expr_integrate_dispatch_primitive,
                                 .simplify = expr_simplify_binary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_lommel_s = {.eval = eval_lommel_s,
                                 .deriv = deriv_lommel_s,
                                 .reverse = expr_reverse_lommel_s,
                                 .kind = EXPR_KIND_LOMMEL_S,
                                 .arity = EXPR_OP_BINARY,
                                 .name = "LommelS",
                                 .TeX_name = "s",
                                 .apply_unary = NULL,
                                 .apply_binary = expr_lommel_s_from_pack,
                                 .integrate = expr_integrate_dispatch_primitive,
                                 .simplify = expr_simplify_rebuild_binary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_lommel_s_pack = {.eval = eval_lommel_s_pack,
                                      .deriv = deriv_lommel_s_pack,
                                      .reverse = expr_reverse_parameter_pack,
                                      .kind = EXPR_KIND_LOMMEL_S_PACK,
                                      .arity = EXPR_OP_BINARY,
                                      .diff_kind = EXPR_DIFF_NONE,
                                      .name = "lommel_s_pack",
                                      .TeX_name = "\\operatorname{pack}",
                                      .apply_unary = NULL,
                                      .apply_binary = expr_lommel_s_pack,
                                      .simplify = expr_simplify_rebuild_binary_operator,
                                      .fold_const_unary = NULL};
const expr_ops_t ops_appell_f1 = {.eval = eval_appell_f1,
                                  .deriv = deriv_appell_f1,
                                  .reverse = expr_reverse_not_differentiable,
                                  .reverse_many = expr_reverse_appell_f1_many,
                                  .kind = EXPR_KIND_APPELL_F1,
                                  .arity = EXPR_OP_BINARY,
                                  .name = "appell_f1",
                                  .TeX_name = "\\operatorname{F}_{1}",
                                  .apply_unary = NULL,
                                  .apply_binary = expr_appell_f1_from_packs,
                                  .simplify = expr_simplify_rebuild_binary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_appell_f1_pack = {.eval = eval_appell_f1_pack,
                                       .deriv = deriv_appell_f1_pack,
                                       .reverse = expr_reverse_parameter_pack,
                                       .kind = EXPR_KIND_APPELL_F1_PACK,
                                       .arity = EXPR_OP_BINARY,
                                       .diff_kind = EXPR_DIFF_NONE,
                                       .name = "appell_f1_pack",
                                       .TeX_name = "\\operatorname{pack}",
                                       .apply_unary = NULL,
                                       .apply_binary = expr_appell_f1_pack,
                                       .simplify = expr_simplify_rebuild_binary_operator,
                                       .fold_const_unary = NULL};
const expr_ops_t ops_lauricella_f = {.eval = eval_lauricella_f,
                                     .deriv = deriv_lauricella_f,
                                     .reverse = expr_reverse_not_differentiable,
                                     .reverse_many = expr_reverse_lauricella_f_many,
                                     .kind = EXPR_KIND_LAURICELLA_F,
                                     .arity = EXPR_OP_BINARY,
                                     .name = "lauricella_f",
                                     .TeX_name = "F_D",
                                     .apply_unary = NULL,
                                     .apply_binary = expr_lauricella_f_from_packs,
                                     .simplify = expr_simplify_rebuild_binary_operator,
                                     .fold_const_unary = NULL};
const expr_ops_t ops_hypergeometric_pFq = {.eval = eval_hypergeometric_pFq,
                                           .deriv = deriv_hypergeometric_pFq,
                                           .reverse = expr_reverse_hypergeometric_pFq,
                                           .kind = EXPR_KIND_HYPERGEOMETRIC_PFQ,
                                           .arity = EXPR_OP_BINARY,
                                           .name = "HypergeometricpFq",
                                           .TeX_name = "F",
                                           .apply_unary = NULL,
                                           .apply_binary = expr_hypergeometric_pFq_from_pack,
                                           .integrate = expr_integrate_dispatch_primitive,
                                           .simplify = expr_simplify_rebuild_binary_operator,
                                           .fold_const_unary = NULL};
const expr_ops_t ops_hypergeometric_pFq_pack = {.eval = eval_hypergeometric_pFq_pack,
                                                .deriv = deriv_hypergeometric_pFq_pack,
                                                .reverse = expr_reverse_parameter_pack,
                                                .kind = EXPR_KIND_HYPERGEOMETRIC_PFQ_PACK,
                                                .arity = EXPR_OP_BINARY,
                                                .diff_kind = EXPR_DIFF_NONE,
                                                .name = "hypergeometric_pFq_pack",
                                                .TeX_name = "\\operatorname{pack}",
                                                .apply_unary = NULL,
                                                .apply_binary = expr_hypergeometric_pFq_pack,
                                                .simplify = expr_simplify_rebuild_binary_operator,
                                                .fold_const_unary = NULL};
const expr_ops_t ops_gammainv = {.eval = eval_gammainv,
                                 .deriv = deriv_gammainv,
                                 .reverse = expr_reverse_gammainv,
                                 .kind = EXPR_KIND_GAMMAINV,
                                 .arity = EXPR_OP_UNARY,
                                 .name = "gammainv",
                                 .TeX_name = "\\Gamma^{-1}",
                                 .inverse_unary = expr_gamma,
                                 .apply_unary = expr_gammainv,
                                 .apply_binary = NULL,
                                 .simplify = expr_simplify_unary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_lambert_w = {.eval = eval_lambert_w,
                                  .deriv = deriv_lambert_w,
                                  .reverse = expr_reverse_lambert_w,
                                  .kind = EXPR_KIND_LAMBERT_W,
                                  .arity = EXPR_OP_UNARY,
                                  .name = "W",
                                  .TeX_name = "W",
                                  .inverse_unary = expr_inverse_lambert_internal,
                                  .apply_unary = expr_lambert_w,
                                  .apply_binary = NULL,
                                  .simplify = expr_simplify_unary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_lambert_wn = {.eval = eval_lambert_wn,
                                   .deriv = deriv_lambert_wn,
                                   .reverse = expr_reverse_lambert_wn,
                                   .kind = EXPR_KIND_LAMBERT_WN,
                                   .arity = EXPR_OP_BINARY,
                                   .name = "Wₙ",
                                   .TeX_name = "W",
                                   .apply_unary = NULL,
                                   .apply_binary = expr_lambert_wn_xp,
                                   .simplify = expr_simplify_binary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_lambert_w0 = {.eval = eval_lambert_w0,
                                   .deriv = deriv_lambert_w0,
                                   .reverse = expr_reverse_lambert_w0,
                                   .kind = EXPR_KIND_LAMBERT_W0,
                                   .arity = EXPR_OP_UNARY,
                                   .name = "W₀",
                                   .TeX_name = "W_{0}",
                                   .inverse_unary = expr_inverse_lambert_internal,
                                   .apply_unary = expr_lambert_w0,
                                   .apply_binary = NULL,
                                   .simplify = expr_simplify_unary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_lambert_wm1 = {.eval = eval_lambert_wm1,
                                    .deriv = deriv_lambert_wm1,
                                    .reverse = expr_reverse_lambert_wm1,
                                    .kind = EXPR_KIND_LAMBERT_WM1,
                                    .arity = EXPR_OP_UNARY,
                                    .name = "W₋₁",
                                    .TeX_name = "W_{-1}",
                                    .inverse_unary = expr_inverse_lambert_internal,
                                    .apply_unary = expr_lambert_wm1,
                                    .apply_binary = NULL,
                                    .simplify = expr_simplify_unary_operator,
                                    .fold_const_unary = NULL};
const expr_ops_t ops_normal_pdf = {.eval = eval_normal_pdf,
                                   .deriv = deriv_normal_pdf,
                                   .reverse = expr_reverse_normal_pdf,
                                   .kind = EXPR_KIND_NORMAL_PDF,
                                   .arity = EXPR_OP_UNARY,
                                   .name = "normal_pdf",
                                   .TeX_name = "\\operatorname{normal\\_pdf}",
                                   .apply_unary = expr_normal_pdf,
                                   .apply_binary = NULL,
                                   .integrate = expr_integrate_dispatch_primitive,
                                   .simplify = expr_simplify_unary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_normal_cdf = {.eval = eval_normal_cdf,
                                   .deriv = deriv_normal_cdf,
                                   .reverse = expr_reverse_normal_cdf,
                                   .kind = EXPR_KIND_NORMAL_CDF,
                                   .arity = EXPR_OP_UNARY,
                                   .name = "normal_cdf",
                                   .TeX_name = "\\operatorname{normal\\_cdf}",
                                   .apply_unary = expr_normal_cdf,
                                   .apply_binary = NULL,
                                   .integrate = expr_integrate_dispatch_primitive,
                                   .simplify = expr_simplify_unary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_normal_logpdf = {.eval = eval_normal_logpdf,
                                      .deriv = deriv_normal_logpdf,
                                      .reverse = expr_reverse_normal_logpdf,
                                      .kind = EXPR_KIND_NORMAL_LOGPDF,
                                      .arity = EXPR_OP_UNARY,
                                      .name = "normal_logpdf",
                                      .TeX_name = "\\operatorname{normal\\_logpdf}",
                                      .apply_unary = expr_normal_logpdf,
                                      .apply_binary = NULL,
                                      .integrate = expr_integrate_dispatch_primitive,
                                      .simplify = expr_simplify_unary_operator,
                                      .fold_const_unary = NULL};
const expr_ops_t ops_pdf = {.eval = eval_normal_pdf,
                            .deriv = deriv_pdf,
                            .reverse = expr_reverse_normal_pdf,
                            .kind = EXPR_KIND_NORMAL_PDF,
                            .arity = EXPR_OP_UNARY,
                            .name = "pdf",
                            .TeX_name = "\\operatorname{pdf}",
                            .apply_unary = expr_pdf,
                            .apply_binary = NULL,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_cdf = {.eval = eval_normal_cdf,
                            .deriv = deriv_cdf,
                            .reverse = expr_reverse_normal_cdf,
                            .kind = EXPR_KIND_NORMAL_CDF,
                            .arity = EXPR_OP_UNARY,
                            .name = "cdf",
                            .TeX_name = "\\operatorname{cdf}",
                            .apply_unary = expr_cdf,
                            .apply_binary = NULL,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_logpdf = {.eval = eval_normal_logpdf,
                               .deriv = deriv_logpdf,
                               .reverse = expr_reverse_normal_logpdf,
                               .kind = EXPR_KIND_NORMAL_LOGPDF,
                               .arity = EXPR_OP_UNARY,
                               .name = "logpdf",
                               .TeX_name = "\\operatorname{logpdf}",
                               .apply_unary = expr_logpdf,
                               .apply_binary = NULL,
                               .integrate = expr_integrate_dispatch_primitive,
                               .simplify = expr_simplify_unary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_Ei = {.eval = eval_Ei,
                           .deriv = deriv_Ei,
                           .reverse = expr_reverse_Ei,
                           .kind = EXPR_KIND_EI,
                           .arity = EXPR_OP_UNARY,
                           .name = "Ei",
                           .TeX_name = "\\operatorname{Ei}",
                           .apply_unary = expr_Ei,
                           .apply_binary = NULL,
                           .integrate = expr_integrate_dispatch_primitive,
                           .simplify = expr_simplify_unary_operator,
                           .fold_const_unary = NULL};
const expr_ops_t ops_E1 = {.eval = eval_E1,
                           .deriv = deriv_E1,
                           .reverse = expr_reverse_E1,
                           .kind = EXPR_KIND_E1,
                           .arity = EXPR_OP_UNARY,
                           .name = "E1",
                           .TeX_name = "\\operatorname{E1}",
                           .apply_unary = expr_E1,
                           .apply_binary = NULL,
                           .integrate = expr_integrate_dispatch_primitive,
                           .simplify = expr_simplify_unary_operator,
                           .fold_const_unary = NULL};
const expr_ops_t ops_beta = {.eval = eval_beta,
                             .deriv = deriv_beta,
                             .reverse = expr_reverse_beta,
                             .kind = EXPR_KIND_BETA,
                             .arity = EXPR_OP_BINARY,
                             .name = "beta",
                             .TeX_name = "\\operatorname{beta}",
                             .apply_unary = NULL,
                             .apply_binary = expr_beta,
                             .simplify = expr_simplify_binary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_indexed_symbol = {.eval = eval_formal_series_component,
                                       .deriv = deriv_indexed_symbol,
                                       .reverse = expr_reverse_not_differentiable,
                                       .kind = EXPR_KIND_INDEXED_SYMBOL,
                                       .arity = EXPR_OP_BINARY,
                                       .name = "indexed",
                                       .TeX_name = NULL,
                                       .apply_unary = NULL,
                                       .apply_binary = NULL,
                                       .simplify = expr_simplify_binary_operator,
                                       .fold_const_unary = NULL};
const expr_ops_t ops_summation = {.eval = eval_finite_summation,
                                  .deriv = deriv_summation,
                                  .reverse = expr_reverse_not_differentiable,
                                  .kind = EXPR_KIND_SUMMATION,
                                  .arity = EXPR_OP_BINARY,
                                  .name = "sum",
                                  .TeX_name = NULL,
                                  .apply_unary = NULL,
                                  .apply_binary = NULL,
                                  .integrate = integrate_summation,
                                  .simplify = expr_simplify_summation_with_special_forms,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_product = {.eval = eval_finite_product,
                                .deriv = deriv_product,
                                .reverse = expr_reverse_not_differentiable,
                                .kind = EXPR_KIND_PRODUCT,
                                .arity = EXPR_OP_BINARY,
                                .name = "product",
                                .TeX_name = NULL,
                                .apply_unary = NULL,
                                .apply_binary = NULL,
                                .simplify = expr_simplify_binary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_logbeta = {.eval = eval_logbeta,
                                .deriv = deriv_logbeta,
                                .reverse = expr_reverse_logbeta,
                                .kind = EXPR_KIND_LOGBETA,
                                .arity = EXPR_OP_BINARY,
                                .name = "logbeta",
                                .TeX_name = "\\operatorname{logbeta}",
                                .apply_unary = NULL,
                                .apply_binary = expr_logbeta,
                                .simplify = expr_simplify_binary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_gammainc_lower = {.eval = eval_gammainc_lower,
                                       .deriv = deriv_gammainc_lower,
                                       .reverse = expr_reverse_gammainc_lower,
                                       .kind = EXPR_KIND_GAMMAINC_LOWER,
                                       .arity = EXPR_OP_BINARY,
                                       .name = "gammainc_lower",
                                       .TeX_name = "\\gamma",
                                       .apply_unary = NULL,
                                       .apply_binary = expr_gammainc_lower,
                                       .simplify = expr_simplify_binary_operator,
                                       .fold_const_unary = NULL};
const expr_ops_t ops_gammainc_upper = {.eval = eval_gammainc_upper,
                                       .deriv = deriv_gammainc_upper,
                                       .reverse = expr_reverse_gammainc_upper,
                                       .kind = EXPR_KIND_GAMMAINC_UPPER,
                                       .arity = EXPR_OP_BINARY,
                                       .name = "gammainc_upper",
                                       .TeX_name = "\\Gamma",
                                       .apply_unary = NULL,
                                       .apply_binary = expr_gammainc_upper,
                                       .simplify = expr_simplify_binary_operator,
                                       .fold_const_unary = NULL};
const expr_ops_t ops_gammainc_P = {.eval = eval_gammainc_P,
                                   .deriv = deriv_gammainc_P,
                                   .reverse = expr_reverse_gammainc_P,
                                   .kind = EXPR_KIND_GAMMAINC_P,
                                   .arity = EXPR_OP_BINARY,
                                   .name = "gammainc_P",
                                   .TeX_name = "\\operatorname{P}",
                                   .apply_unary = NULL,
                                   .apply_binary = expr_gammainc_P,
                                   .simplify = expr_simplify_binary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_gammainc_Q = {.eval = eval_gammainc_Q,
                                   .deriv = deriv_gammainc_Q,
                                   .reverse = expr_reverse_gammainc_Q,
                                   .kind = EXPR_KIND_GAMMAINC_Q,
                                   .arity = EXPR_OP_BINARY,
                                   .name = "gammainc_Q",
                                   .TeX_name = "\\operatorname{Q}",
                                   .apply_unary = NULL,
                                   .apply_binary = expr_gammainc_Q,
                                   .simplify = expr_simplify_binary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_factorial = {.eval = eval_factorial,
                                  .deriv = deriv_not_differentiable,
                                  .reverse = expr_reverse_not_differentiable,
                                  .kind = EXPR_KIND_FACTORIAL,
                                  .arity = EXPR_OP_UNARY,
                                  .diff_kind = EXPR_DIFF_NONE,
                                  .name = "factorial",
                                  .TeX_name = "\\operatorname{factorial}",
                                  .apply_unary = expr_factorial,
                                  .apply_binary = NULL,
                                  .simplify = expr_simplify_unary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_fibonacci = {.eval = eval_fibonacci,
                                  .deriv = deriv_not_differentiable,
                                  .reverse = expr_reverse_not_differentiable,
                                  .kind = EXPR_KIND_FIBONACCI,
                                  .arity = EXPR_OP_UNARY,
                                  .diff_kind = EXPR_DIFF_NONE,
                                  .name = "fibonacci",
                                  .TeX_name = "\\operatorname{fibonacci}",
                                  .apply_unary = expr_fibonacci,
                                  .apply_binary = NULL,
                                  .simplify = expr_simplify_unary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_partition = {.eval = eval_partition,
                                  .deriv = deriv_not_differentiable,
                                  .reverse = expr_reverse_not_differentiable,
                                  .kind = EXPR_KIND_PARTITION,
                                  .arity = EXPR_OP_UNARY,
                                  .diff_kind = EXPR_DIFF_NONE,
                                  .name = "partition",
                                  .TeX_name = "\\operatorname{partition}",
                                  .apply_unary = expr_partition,
                                  .apply_binary = NULL,
                                  .simplify = expr_simplify_unary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_isqrt = {.eval = eval_isqrt,
                              .deriv = deriv_not_differentiable,
                              .reverse = expr_reverse_not_differentiable,
                              .kind = EXPR_KIND_ISQRT,
                              .arity = EXPR_OP_UNARY,
                              .diff_kind = EXPR_DIFF_NONE,
                              .name = "isqrt",
                              .TeX_name = "\\operatorname{isqrt}",
                              .apply_unary = expr_isqrt,
                              .apply_binary = NULL,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_gcd = {.eval = eval_gcd,
                            .deriv = deriv_not_differentiable,
                            .reverse = expr_reverse_not_differentiable,
                            .kind = EXPR_KIND_GCD,
                            .arity = EXPR_OP_BINARY,
                            .diff_kind = EXPR_DIFF_NONE,
                            .name = "gcd",
                            .TeX_name = "\\gcd",
                            .apply_unary = NULL,
                            .apply_binary = expr_gcd,
                            .simplify = expr_simplify_binary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_lcm = {.eval = eval_lcm,
                            .deriv = deriv_not_differentiable,
                            .reverse = expr_reverse_not_differentiable,
                            .kind = EXPR_KIND_LCM,
                            .arity = EXPR_OP_BINARY,
                            .diff_kind = EXPR_DIFF_NONE,
                            .name = "lcm",
                            .TeX_name = "\\operatorname{lcm}",
                            .apply_unary = NULL,
                            .apply_binary = expr_lcm,
                            .simplify = expr_simplify_binary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_mod = {.eval = eval_mod,
                            .deriv = deriv_not_differentiable,
                            .reverse = expr_reverse_not_differentiable,
                            .kind = EXPR_KIND_MOD,
                            .arity = EXPR_OP_BINARY,
                            .diff_kind = EXPR_DIFF_NONE,
                            .name = "mod",
                            .TeX_name = "\\operatorname{mod}",
                            .apply_unary = NULL,
                            .apply_binary = expr_mod,
                            .simplify = expr_simplify_binary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_modinv = {.eval = eval_modinv,
                               .deriv = deriv_not_differentiable,
                               .reverse = expr_reverse_not_differentiable,
                               .kind = EXPR_KIND_MODINV,
                               .arity = EXPR_OP_BINARY,
                               .diff_kind = EXPR_DIFF_NONE,
                               .name = "modinv",
                               .TeX_name = "\\operatorname{modinv}",
                               .apply_unary = NULL,
                               .apply_binary = expr_modinv,
                               .simplify = expr_simplify_binary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_is_prime = {.eval = eval_is_prime,
                                 .deriv = deriv_not_differentiable,
                                 .reverse = expr_reverse_not_differentiable,
                                 .kind = EXPR_KIND_IS_PRIME,
                                 .arity = EXPR_OP_UNARY,
                                 .diff_kind = EXPR_DIFF_NONE,
                                 .name = "is_prime",
                                 .TeX_name = "\\operatorname{is\\_prime}",
                                 .apply_unary = expr_is_prime,
                                 .apply_binary = NULL,
                                 .simplify = expr_simplify_unary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_next_prime = {.eval = eval_next_prime,
                                   .deriv = deriv_not_differentiable,
                                   .reverse = expr_reverse_not_differentiable,
                                   .kind = EXPR_KIND_NEXT_PRIME,
                                   .arity = EXPR_OP_UNARY,
                                   .diff_kind = EXPR_DIFF_NONE,
                                   .name = "next_prime",
                                   .TeX_name = "\\operatorname{next\\_prime}",
                                   .apply_unary = expr_next_prime,
                                   .apply_binary = NULL,
                                   .simplify = expr_simplify_unary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_prev_prime = {.eval = eval_prev_prime,
                                   .deriv = deriv_not_differentiable,
                                   .reverse = expr_reverse_not_differentiable,
                                   .kind = EXPR_KIND_PREV_PRIME,
                                   .arity = EXPR_OP_UNARY,
                                   .diff_kind = EXPR_DIFF_NONE,
                                   .name = "prev_prime",
                                   .TeX_name = "\\operatorname{prev\\_prime}",
                                   .apply_unary = expr_prev_prime,
                                   .apply_binary = NULL,
                                   .simplify = expr_simplify_unary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_bit_and = {.eval = eval_bit_and,
                                .deriv = deriv_not_differentiable,
                                .reverse = expr_reverse_not_differentiable,
                                .kind = EXPR_KIND_BIT_AND,
                                .arity = EXPR_OP_BINARY,
                                .diff_kind = EXPR_DIFF_NONE,
                                .name = "AND",
                                .TeX_name = "\\operatorname{AND}",
                                .apply_unary = NULL,
                                .apply_binary = expr_bit_and,
                                .simplify = expr_simplify_binary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_bit_or = {.eval = eval_bit_or,
                               .deriv = deriv_not_differentiable,
                               .reverse = expr_reverse_not_differentiable,
                               .kind = EXPR_KIND_BIT_OR,
                               .arity = EXPR_OP_BINARY,
                               .diff_kind = EXPR_DIFF_NONE,
                               .name = "OR",
                               .TeX_name = "\\operatorname{OR}",
                               .apply_unary = NULL,
                               .apply_binary = expr_bit_or,
                               .simplify = expr_simplify_binary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_bit_xor = {.eval = eval_bit_xor,
                                .deriv = deriv_not_differentiable,
                                .reverse = expr_reverse_not_differentiable,
                                .kind = EXPR_KIND_BIT_XOR,
                                .arity = EXPR_OP_BINARY,
                                .diff_kind = EXPR_DIFF_NONE,
                                .name = "XOR",
                                .TeX_name = "\\operatorname{XOR}",
                                .apply_unary = NULL,
                                .apply_binary = expr_bit_xor,
                                .simplify = expr_simplify_binary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_bit_not = {.eval = eval_bit_not,
                                .deriv = deriv_not_differentiable,
                                .reverse = expr_reverse_not_differentiable,
                                .kind = EXPR_KIND_BIT_NOT,
                                .arity = EXPR_OP_UNARY,
                                .diff_kind = EXPR_DIFF_NONE,
                                .name = "NOT",
                                .TeX_name = "\\operatorname{NOT}",
                                .apply_unary = expr_bit_not,
                                .apply_binary = NULL,
                                .simplify = expr_simplify_unary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_shl = {.eval = eval_shl,
                            .deriv = deriv_not_differentiable,
                            .reverse = expr_reverse_not_differentiable,
                            .kind = EXPR_KIND_SHL,
                            .arity = EXPR_OP_BINARY,
                            .diff_kind = EXPR_DIFF_NONE,
                            .name = "SHL",
                            .TeX_name = "\\operatorname{SHL}",
                            .apply_unary = NULL,
                            .apply_binary = expr_shl,
                            .simplify = expr_simplify_binary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_shr = {.eval = eval_shr,
                            .deriv = deriv_not_differentiable,
                            .reverse = expr_reverse_not_differentiable,
                            .kind = EXPR_KIND_SHR,
                            .arity = EXPR_OP_BINARY,
                            .diff_kind = EXPR_DIFF_NONE,
                            .name = "SHR",
                            .TeX_name = "\\operatorname{SHR}",
                            .apply_unary = NULL,
                            .apply_binary = expr_shr,
                            .simplify = expr_simplify_binary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_factors = {.eval = eval_factors,
                                .deriv = deriv_not_differentiable,
                                .reverse = expr_reverse_not_differentiable,
                                .kind = EXPR_KIND_FACTORS,
                                .arity = EXPR_OP_UNARY,
                                .diff_kind = EXPR_DIFF_NONE,
                                .name = "factors",
                                .TeX_name = "\\operatorname{factors}",
                                .apply_unary = expr_factors,
                                .apply_binary = NULL,
                                .simplify = expr_simplify_unary_operator,
                                .fold_const_unary = NULL};

expr_t *expr_apply_unary_kind(expr_op_kind_t kind, const expr_t *arg)
{
    static const expr_ops_t *const unary_ops_by_kind[EXPR_KIND_COUNT] = {
        [EXPR_KIND_NEG] = &ops_neg,
        [EXPR_KIND_SIN] = &ops_sin,
        [EXPR_KIND_COS] = &ops_cos,
        [EXPR_KIND_TAN] = &ops_tan,
        [EXPR_KIND_SEC] = &ops_sec,
        [EXPR_KIND_COSEC] = &ops_cosec,
        [EXPR_KIND_COT] = &ops_cot,
        [EXPR_KIND_VERSIN] = &ops_versin,
        [EXPR_KIND_VERCOS] = &ops_vercos,
        [EXPR_KIND_COVERSIN] = &ops_coversin,
        [EXPR_KIND_COVERCOS] = &ops_covercos,
        [EXPR_KIND_HAVERSIN] = &ops_haversin,
        [EXPR_KIND_HAVERCOS] = &ops_havercos,
        [EXPR_KIND_HACOVERSIN] = &ops_hacoversin,
        [EXPR_KIND_HACOVERCOS] = &ops_hacovercos,
        [EXPR_KIND_SINH] = &ops_sinh,
        [EXPR_KIND_COSH] = &ops_cosh,
        [EXPR_KIND_TANH] = &ops_tanh,
        [EXPR_KIND_SECH] = &ops_sech,
        [EXPR_KIND_COSECH] = &ops_cosech,
        [EXPR_KIND_COTH] = &ops_coth,
        [EXPR_KIND_ASIN] = &ops_asin,
        [EXPR_KIND_ACOS] = &ops_acos,
        [EXPR_KIND_ATAN] = &ops_atan,
        [EXPR_KIND_ASEC] = &ops_asec,
        [EXPR_KIND_ACOSEC] = &ops_acosec,
        [EXPR_KIND_ACOT] = &ops_acot,
        [EXPR_KIND_ARCVERSIN] = &ops_arcversin,
        [EXPR_KIND_ARCVERCOS] = &ops_arcvercos,
        [EXPR_KIND_ARCCOVERSIN] = &ops_arccoversin,
        [EXPR_KIND_ARCCOVERCOS] = &ops_arccovercos,
        [EXPR_KIND_ARCHAVERSIN] = &ops_archaversin,
        [EXPR_KIND_ARCHAVERCOS] = &ops_archavercos,
        [EXPR_KIND_ARCHACOVERSIN] = &ops_archacoversin,
        [EXPR_KIND_ARCHACOVERCOS] = &ops_archacovercos,
        [EXPR_KIND_ASINH] = &ops_asinh,
        [EXPR_KIND_ACOSH] = &ops_acosh,
        [EXPR_KIND_ATANH] = &ops_atanh,
        [EXPR_KIND_ASECH] = &ops_asech,
        [EXPR_KIND_ACOSECH] = &ops_acosech,
        [EXPR_KIND_ACOTH] = &ops_acoth,
        [EXPR_KIND_EXP] = &ops_exp,
        [EXPR_KIND_LOG] = &ops_log,
        [EXPR_KIND_LOG10] = &ops_log10,
        [EXPR_KIND_SQRT] = &ops_sqrt,
        [EXPR_KIND_CUBRT] = &ops_cubrt,
        [EXPR_KIND_FLOOR] = &ops_floor,
        [EXPR_KIND_CEIL] = &ops_ceil,
        [EXPR_KIND_ABS] = &ops_abs,
        [EXPR_KIND_CONJ] = &ops_conj,
        [EXPR_KIND_ERF] = &ops_erf,
        [EXPR_KIND_ERFC] = &ops_erfc,
        [EXPR_KIND_LGAMMA] = &ops_lgamma,
        [EXPR_KIND_ERFINV] = &ops_erfinv,
        [EXPR_KIND_ERFCINV] = &ops_erfcinv,
        [EXPR_KIND_GAMMA] = &ops_gamma,
        [EXPR_KIND_DIGAMMA] = &ops_digamma,
        [EXPR_KIND_TRIGAMMA] = &ops_trigamma,
        [EXPR_KIND_ZETA] = &ops_zeta,
        [EXPR_KIND_ZETAP] = &ops_zetap,
        [EXPR_KIND_ZETAH] = &ops_zetah,
        [EXPR_KIND_ZATAHP] = &ops_zatahp,
        [EXPR_KIND_LERCH_PHI] = &ops_lerch_phi,
        [EXPR_KIND_LERCH_PHI_PACK] = &ops_lerch_phi_pack,
        [EXPR_KIND_DILOG] = &ops_dilog,
        [EXPR_KIND_POLYLOG1] = &ops_polylog1,
        [EXPR_KIND_GAMMAINV] = &ops_gammainv,
        [EXPR_KIND_LAMBERT_W] = &ops_lambert_w,
        [EXPR_KIND_LAMBERT_WN] = &ops_lambert_wn,
        [EXPR_KIND_LAMBERT_W0] = &ops_lambert_w0,
        [EXPR_KIND_LAMBERT_WM1] = &ops_lambert_wm1,
        [EXPR_KIND_NORMAL_PDF] = &ops_normal_pdf,
        [EXPR_KIND_NORMAL_CDF] = &ops_normal_cdf,
        [EXPR_KIND_NORMAL_LOGPDF] = &ops_normal_logpdf,
        [EXPR_KIND_EI] = &ops_Ei,
        [EXPR_KIND_E1] = &ops_E1,
        [EXPR_KIND_FACTORIAL] = &ops_factorial,
        [EXPR_KIND_FIBONACCI] = &ops_fibonacci,
        [EXPR_KIND_PARTITION] = &ops_partition,
        [EXPR_KIND_ISQRT] = &ops_isqrt,
        [EXPR_KIND_IS_PRIME] = &ops_is_prime,
        [EXPR_KIND_NEXT_PRIME] = &ops_next_prime,
        [EXPR_KIND_PREV_PRIME] = &ops_prev_prime,
        [EXPR_KIND_BIT_NOT] = &ops_bit_not,
        [EXPR_KIND_FACTORS] = &ops_factors};
    const expr_ops_t *ops = NULL;

    if ((unsigned)kind < (unsigned)EXPR_KIND_COUNT)
        ops = unary_ops_by_kind[kind];
    if (ops && ops->apply_unary)
        return ops->apply_unary(arg);

    return NULL;
}

/* Construct the single-valued principal square root. */
expr_t *expr_sqrt(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_sqrt, a);
}
/* Construct the single-valued principal cube root. */
expr_t *expr_cubrt(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_cubrt, a);
}
/* Construct the single-valued principal root. */
expr_t *expr_root(const expr_t *a, const expr_t *order)
{
    return expr_math_wrap_binary(&ops_root, a, order);
}
expr_t *expr_exp(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_exp, a);
}
expr_t *expr_log(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_log, a);
}

/* Construct the natural-logarithm shorthand using the canonical log operation. */
expr_t *expr_ln(const expr_t *a)
{
    return expr_log(a);
}

expr_t *expr_log10(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_log10, a);
}

/* Construct the common-logarithm shorthand using the canonical log10 operation. */
expr_t *expr_lg(const expr_t *a)
{
    return expr_log10(a);
}
expr_t *expr_floor(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_floor, a);
}
expr_t *expr_ceil(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_ceil, a);
}
expr_t *expr_sin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_sin, a);
}
expr_t *expr_cos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_cos, a);
}
expr_t *expr_tan(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_tan, a);
}
expr_t *expr_sec(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_sec, a);
}
expr_t *expr_cosec(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_cosec, a);
}
expr_t *expr_cot(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_cot, a);
}
expr_t *expr_versin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_versin, a);
}
expr_t *expr_vercos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_vercos, a);
}
expr_t *expr_coversin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_coversin, a);
}
expr_t *expr_covercos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_covercos, a);
}
expr_t *expr_haversin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_haversin, a);
}
expr_t *expr_havercos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_havercos, a);
}
expr_t *expr_hacoversin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_hacoversin, a);
}
expr_t *expr_hacovercos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_hacovercos, a);
}
expr_t *expr_sinh(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_sinh, a);
}
expr_t *expr_cosh(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_cosh, a);
}
expr_t *expr_tanh(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_tanh, a);
}
expr_t *expr_sech(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_sech, a);
}
expr_t *expr_cosech(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_cosech, a);
}
expr_t *expr_coth(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_coth, a);
}
expr_t *expr_asin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_asin, a);
}
expr_t *expr_acos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_acos, a);
}
expr_t *expr_atan(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_atan, a);
}
expr_t *expr_asec(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_asec, a);
}
expr_t *expr_acosec(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_acosec, a);
}
expr_t *expr_acot(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_acot, a);
}
expr_t *expr_arcversin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_arcversin, a);
}
expr_t *expr_arcvercos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_arcvercos, a);
}
expr_t *expr_arccoversin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_arccoversin, a);
}
expr_t *expr_arccovercos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_arccovercos, a);
}
expr_t *expr_archaversin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_archaversin, a);
}
expr_t *expr_archavercos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_archavercos, a);
}
expr_t *expr_archacoversin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_archacoversin, a);
}
expr_t *expr_archacovercos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_archacovercos, a);
}
expr_t *expr_atan2(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_atan2, a, b);
}
expr_t *expr_asinh(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_asinh, a);
}
expr_t *expr_acosh(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_acosh, a);
}
expr_t *expr_atanh(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_atanh, a);
}
expr_t *expr_asech(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_asech, a);
}
expr_t *expr_acosech(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_acosech, a);
}
expr_t *expr_acoth(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_acoth, a);
}
expr_t *expr_abs(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_abs, a);
}
/* Construct the complex conjugate of an expression. */
expr_t *expr_conj(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_conj, a);
}
expr_t *expr_erf(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_erf, a);
}
expr_t *expr_erfc(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_erfc, a);
}
expr_t *expr_lgamma(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_lgamma, a);
}
expr_t *expr_hypot(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_hypot, a, b);
}
expr_t *expr_erfinv(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_erfinv, a);
}
expr_t *expr_erfcinv(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_erfcinv, a);
}
expr_t *expr_gamma(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_gamma, a);
}
expr_t *expr_digamma(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_digamma, a);
}
expr_t *expr_trigamma(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_trigamma, a);
}
/* Construct a Riemann zeta function expression. */
expr_t *expr_zeta(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_zeta, a);
}
/* Construct a first Riemann zeta derivative expression. */
expr_t *expr_zetap(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_zetap, a);
}
/* Construct a Hurwitz zeta function expression. */
expr_t *expr_zetah(const expr_t *s, const expr_t *a)
{
    return expr_math_wrap_binary(&ops_zetah, s, a);
}
/* Construct a first Hurwitz zeta derivative expression. */
expr_t *expr_zatahp(const expr_t *s, const expr_t *a)
{
    return expr_math_wrap_binary(&ops_zatahp, s, a);
}
expr_t *expr_polygamma_xp(const expr_t *order, const expr_t *arg)
{
    return expr_math_wrap_binary(&ops_polygamma, order, arg);
}
expr_t *expr_polygamma(unsigned int order, const expr_t *a)
{
    NUM_SCOPE(scope);
    number_t order_value = num_create_from_long((long)order);
    expr_t *order_xp = expr_new_const(order_value);
    expr_t *out = expr_polygamma_xp(order_xp, a);

    expr_free(order_xp);
    return out;
}
expr_t *expr_dilog(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_dilog, a);
}
/* Construct an order-one polylogarithm expression. */
expr_t *expr_polylog1(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_polylog1, a);
}
expr_t *expr_polylog_xp(const expr_t *order, const expr_t *arg)
{
    return expr_math_wrap_binary(&ops_polylog, order, arg);
}
expr_t *expr_polylog(unsigned int order, const expr_t *a)
{
    NUM_SCOPE(scope);
    number_t order_value = num_create_from_long((long)order);
    expr_t *order_xp = expr_new_const(order_value);
    expr_t *out = expr_polylog_xp(order_xp, a);

    expr_free(order_xp);
    return out;
}
/* Construct a native harmonic-polynomial expression. */
expr_t *expr_harmonic_poly(const expr_t *degree, const expr_t *argument)
{
    return expr_math_wrap_binary(&ops_harmonic_poly, degree, argument);
}
/* Construct a native Lerch-transcendent expression. */
expr_t *expr_lerch_phi(const expr_t *z, const expr_t *s, const expr_t *a)
{
    expr_t *parameters = expr_math_wrap_binary(&ops_lerch_phi_pack, z, s);
    expr_t *out = parameters ? expr_math_wrap_binary(&ops_lerch_phi, parameters, a) : NULL;

    expr_free(parameters);
    return out;
}
expr_t *expr_legendre_chi_xp(const expr_t *order, const expr_t *arg)
{
    return expr_math_wrap_binary(&ops_legendre_chi, order, arg);
}
expr_t *expr_legendre_chi(unsigned int order, const expr_t *a)
{
    NUM_SCOPE(scope);
    number_t order_value = num_create_from_long((long)order);
    expr_t *order_xp = expr_new_const(order_value);
    expr_t *out = expr_legendre_chi_xp(order_xp, a);

    expr_free(order_xp);
    return out;
}
expr_t *expr_bessel_j(const expr_t *order, const expr_t *argument)
{
    return expr_math_wrap_binary(&ops_bessel_j, order, argument);
}
expr_t *expr_bessel_y(const expr_t *order, const expr_t *argument)
{
    return expr_math_wrap_binary(&ops_bessel_y, order, argument);
}

expr_t *expr_lommel_s(const expr_t *mu, const expr_t *nu, const expr_t *argument)
{
    expr_t *parameters = expr_lommel_s_pack(mu, nu);
    expr_t *out = parameters ? expr_math_wrap_binary(&ops_lommel_s, parameters, argument) : NULL;

    expr_free(parameters);
    return out;
}

static expr_t *expr_lommel_s_hypergeometric_argument_derivative_expansion(const expr_t *mu, const expr_t *nu,
                                                                          const expr_t *argument)
{
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *two = expr_new_const(NUM_TWO);
    expr_t *mu_plus_one = expr_add_long(mu, 1);
    expr_t *mu_plus_two = expr_add_long(mu, 2);
    expr_t *mu_minus_nu = expr_sub(mu, nu);
    expr_t *mu_plus_nu = expr_add(mu, nu);
    expr_t *lower_a_numerator = mu_minus_nu ? expr_add_long(mu_minus_nu, 3) : NULL;
    expr_t *lower_b_numerator = mu_plus_nu ? expr_add_long(mu_plus_nu, 3) : NULL;
    expr_t *lower_a = lower_a_numerator ? expr_div_num(lower_a_numerator, &NUM_TWO) : NULL;
    expr_t *lower_b = lower_b_numerator ? expr_div_num(lower_b_numerator, &NUM_TWO) : NULL;
    expr_t *lower_a_shifted = lower_a ? expr_add_long(lower_a, 1) : NULL;
    expr_t *lower_b_shifted = lower_b ? expr_add_long(lower_b, 1) : NULL;
    expr_t *argument_squared = expr_pow_long(argument, 2);
    expr_t *negative_argument_squared = argument_squared ? expr_neg(argument_squared) : NULL;
    number_t four = num_create_from_long(4);
    expr_t *hyper_argument = negative_argument_squared ? expr_div_num(negative_argument_squared, &four) : NULL;
    const expr_t *upper[1] = {one};
    const expr_t *upper_shifted[1] = {two};
    const expr_t *lower[2] = {lower_a, lower_b};
    const expr_t *lower_shifted[2] = {lower_a_shifted, lower_b_shifted};
    expr_t *hyper = (one && lower_a && lower_b && hyper_argument)
                        ? expr_hypergeometric_pFq(1u, upper, 2u, lower, hyper_argument)
                        : NULL;
    expr_t *hyper_shifted = (two && lower_a_shifted && lower_b_shifted && hyper_argument)
                                ? expr_hypergeometric_pFq(1u, upper_shifted, 2u, lower_shifted, hyper_argument)
                                : NULL;
    expr_t *argument_to_mu = expr_pow_xp(argument, mu);
    expr_t *first_coefficient = (mu_plus_one && argument_to_mu) ? expr_mul(mu_plus_one, argument_to_mu) : NULL;
    expr_t *first_term = (first_coefficient && hyper) ? expr_mul(first_coefficient, hyper) : NULL;
    expr_t *argument_to_mu_plus_two = mu_plus_two ? expr_pow_xp(argument, mu_plus_two) : NULL;
    expr_t *lower_product = (lower_a && lower_b) ? expr_mul(lower_a, lower_b) : NULL;
    expr_t *twice_lower_product = (two && lower_product) ? expr_mul(two, lower_product) : NULL;
    expr_t *second_numerator =
        (argument_to_mu_plus_two && hyper_shifted) ? expr_mul(argument_to_mu_plus_two, hyper_shifted) : NULL;
    expr_t *second_term = (second_numerator && twice_lower_product) ? expr_div(second_numerator, twice_lower_product) : NULL;
    expr_t *numerator = (first_term && second_term) ? expr_sub(first_term, second_term) : NULL;
    expr_t *mu_plus_one_squared = mu_plus_one ? expr_pow_long(mu_plus_one, 2) : NULL;
    expr_t *nu_squared = expr_pow_long(nu, 2);
    expr_t *denominator = (mu_plus_one_squared && nu_squared) ? expr_sub(mu_plus_one_squared, nu_squared) : NULL;
    expr_t *raw = (numerator && denominator) ? expr_div(numerator, denominator) : NULL;
    expr_t *out = raw ? expr_simplify(raw) : NULL;

    expr_free(raw);
    expr_free(denominator);
    expr_free(nu_squared);
    expr_free(mu_plus_one_squared);
    expr_free(numerator);
    expr_free(second_term);
    expr_free(second_numerator);
    expr_free(twice_lower_product);
    expr_free(lower_product);
    expr_free(argument_to_mu_plus_two);
    expr_free(first_term);
    expr_free(first_coefficient);
    expr_free(argument_to_mu);
    expr_free(hyper_shifted);
    expr_free(hyper);
    expr_free(hyper_argument);
    num_destroy(&four);
    expr_free(negative_argument_squared);
    expr_free(argument_squared);
    expr_free(lower_b_shifted);
    expr_free(lower_a_shifted);
    expr_free(lower_b);
    expr_free(lower_a);
    expr_free(lower_b_numerator);
    expr_free(lower_a_numerator);
    expr_free(mu_plus_nu);
    expr_free(mu_minus_nu);
    expr_free(mu_plus_two);
    expr_free(mu_plus_one);
    expr_free(two);
    expr_free(one);
    return out;
}

expr_t *expr_lommel_s_argument_derivative_expansion(const expr_t *mu, const expr_t *nu, const expr_t *argument)
{
    expr_t *mu_plus_nu = expr_add(mu, nu);
    expr_t *coefficient = mu_plus_nu ? expr_add_long(mu_plus_nu, -1) : NULL;
    expr_t *simplified_coefficient = coefficient ? expr_simplify(coefficient) : NULL;
    expr_t *mu_minus_one = expr_add_long(mu, -1);
    expr_t *nu_minus_one = expr_add_long(nu, -1);
    expr_t *shifted = (mu_minus_one && nu_minus_one) ? expr_lommel_s(mu_minus_one, nu_minus_one, argument) : NULL;
    expr_t *left = (coefficient && shifted) ? expr_mul(coefficient, shifted) : NULL;
    expr_t *original = expr_lommel_s(mu, nu, argument);
    expr_t *nu_times_original = original ? expr_mul(nu, original) : NULL;
    expr_t *right = nu_times_original ? expr_div(nu_times_original, argument) : NULL;
    expr_t *raw = (left && right) ? expr_sub(left, right) : NULL;
    expr_t *out = NULL;

    if (simplified_coefficient && expr_is_exact_zero(simplified_coefficient))
        out = expr_lommel_s_hypergeometric_argument_derivative_expansion(mu, nu, argument);
    else
        out = raw ? expr_simplify(raw) : NULL;

    expr_free(raw);
    expr_free(right);
    expr_free(nu_times_original);
    expr_free(original);
    expr_free(left);
    expr_free(shifted);
    expr_free(nu_minus_one);
    expr_free(mu_minus_one);
    expr_free(simplified_coefficient);
    expr_free(coefficient);
    expr_free(mu_plus_nu);
    return out;
}

static expr_t *expr_lommel_s_pack(const expr_t *mu, const expr_t *nu)
{
    return expr_math_wrap_binary(&ops_lommel_s_pack, mu, nu);
}

static expr_t *expr_lommel_s_from_pack(const expr_t *parameters, const expr_t *argument)
{
    return expr_math_wrap_binary(&ops_lommel_s, parameters, argument);
}

static expr_t *expr_appell_f1_pack(const expr_t *left, const expr_t *right)
{
    return expr_math_wrap_binary(&ops_appell_f1_pack, left, right);
}

static expr_t *expr_appell_f1_from_packs(const expr_t *params, const expr_t *vars)
{
    return expr_math_wrap_binary(&ops_appell_f1, params, vars);
}

expr_t *expr_appell_f1(const expr_t *a, const expr_t *b1, const expr_t *b2, const expr_t *c, const expr_t *x,
                       const expr_t *y)
{
    expr_t *ab = NULL;
    expr_t *bc = NULL;
    expr_t *params = NULL;
    expr_t *vars = NULL;
    expr_t *out = NULL;

    if (!a || !b1 || !b2 || !c || !x || !y)
        return NULL;

    ab = expr_appell_f1_pack(a, b1);
    bc = expr_appell_f1_pack(b2, c);
    params = ab && bc ? expr_appell_f1_pack(ab, bc) : NULL;
    vars = expr_appell_f1_pack(x, y);
    out = params && vars ? expr_math_wrap_binary(&ops_appell_f1, params, vars) : NULL;

    expr_free(vars);
    expr_free(params);
    expr_free(bc);
    expr_free(ab);
    return out;
}

static expr_t *expr_hypergeometric_pFq_pack(const expr_t *left, const expr_t *right)
{
    return expr_math_wrap_binary(&ops_hypergeometric_pFq_pack, left, right);
}

static expr_t *expr_hypergeometric_pFq_from_pack(const expr_t *parameters, const expr_t *argument)
{
    return expr_math_wrap_binary(&ops_hypergeometric_pFq, parameters, argument);
}

static expr_t *expr_lauricella_f_from_packs(const expr_t *params, const expr_t *vars)
{
    return expr_math_wrap_binary(&ops_lauricella_f, params, vars);
}

static expr_t *expr_hypergeometric_parameter_values(size_t count, const expr_t *const *values, const expr_t *empty)
{
    expr_t *list;

    if (count == 0u)
        return expr_clone(empty);
    if (!values || !values[0])
        return NULL;
    expr_retain(values[0]);
    list = (expr_t *)values[0];
    for (size_t i = 1u; list && i < count; ++i) {
        expr_t *next;

        if (!values[i]) {
            expr_free(list);
            return NULL;
        }
        next = expr_hypergeometric_pFq_pack(list, values[i]);
        expr_free(list);
        list = next;
    }
    return list;
}

static expr_t *expr_hypergeometric_parameter_set(size_t count, const expr_t *const *values)
{
    number_t count_value;
    expr_t *count_expr;
    expr_t *value_expr;
    expr_t *set;

    if (count > (size_t)LONG_MAX)
        return NULL;
    count_value = num_create_from_long((long)count);
    count_expr = expr_new_const(count_value);
    value_expr = count_expr ? expr_hypergeometric_parameter_values(count, values, count_expr) : NULL;
    set = count_expr && value_expr ? expr_hypergeometric_pFq_pack(count_expr, value_expr) : NULL;
    expr_free(value_expr);
    expr_free(count_expr);
    return set;
}

expr_t *expr_hypergeometric_pFq(size_t upper_count, const expr_t *const *upper, size_t lower_count,
                                const expr_t *const *lower, const expr_t *argument)
{
    expr_t *upper_set = NULL;
    expr_t *lower_set = NULL;
    expr_t *parameters = NULL;
    expr_t *out = NULL;

    if ((upper_count > 0u && !upper) || (lower_count > 0u && !lower) || !argument)
        return NULL;

    upper_set = expr_hypergeometric_parameter_set(upper_count, upper);
    lower_set = expr_hypergeometric_parameter_set(lower_count, lower);
    parameters = upper_set && lower_set ? expr_hypergeometric_pFq_pack(upper_set, lower_set) : NULL;
    out = parameters ? expr_math_wrap_binary(&ops_hypergeometric_pFq, parameters, argument) : NULL;

    expr_free(parameters);
    expr_free(lower_set);
    expr_free(upper_set);
    return out;
}

expr_t *expr_hypergeometric_pFq_from_args(size_t argument_count, expr_t *const *arguments)
{
    double upper_count_value;
    double lower_count_value;
    size_t upper_count;
    size_t lower_count;

    if (!arguments || argument_count < 3u || !expr_is_const(arguments[0]) || !expr_is_const(arguments[1]) ||
        !num_is_real(arguments[0]->c) || !num_is_integer(arguments[0]->c) || !num_is_real(arguments[1]->c) ||
        !num_is_integer(arguments[1]->c) || num_get_sign(arguments[0]->c) < 0 || num_get_sign(arguments[1]->c) < 0)
        return NULL;
    upper_count_value = num_to_double(arguments[0]->c);
    lower_count_value = num_to_double(arguments[1]->c);
    if (!isfinite(upper_count_value) || !isfinite(lower_count_value) || upper_count_value > 1024.0 ||
        lower_count_value > 1024.0)
        return NULL;
    upper_count = (size_t)upper_count_value;
    lower_count = (size_t)lower_count_value;
    if (argument_count != upper_count + lower_count + 3u)
        return NULL;
    return expr_hypergeometric_pFq(upper_count, (const expr_t *const *)&arguments[2], lower_count,
                                   (const expr_t *const *)&arguments[2u + upper_count], arguments[argument_count - 1u]);
}

expr_t *expr_lauricella_f(const expr_t *a, size_t variable_count, const expr_t *const *b, const expr_t *c,
                          const expr_t *const *x)
{
    expr_t *a_c = NULL;
    expr_t *b_set = NULL;
    expr_t *x_set = NULL;
    expr_t *parameters = NULL;
    expr_t *out = NULL;

    if (!a || !c || (variable_count > 0u && (!b || !x)))
        return NULL;
    a_c = expr_hypergeometric_pFq_pack(a, c);
    b_set = expr_hypergeometric_parameter_set(variable_count, b);
    x_set = expr_hypergeometric_parameter_set(variable_count, x);
    parameters = a_c && b_set ? expr_hypergeometric_pFq_pack(a_c, b_set) : NULL;
    out = parameters && x_set ? expr_math_wrap_binary(&ops_lauricella_f, parameters, x_set) : NULL;
    expr_free(parameters);
    expr_free(x_set);
    expr_free(b_set);
    expr_free(a_c);
    return out;
}

expr_t *expr_lauricella_f_from_args(size_t argument_count, expr_t *const *arguments)
{
    double count_value;
    size_t variable_count;

    if (!arguments || argument_count < 3u || !expr_is_const(arguments[0]) || !num_is_real(arguments[0]->c) ||
        !num_is_integer(arguments[0]->c) || num_get_sign(arguments[0]->c) < 0)
        return NULL;
    count_value = num_to_double(arguments[0]->c);
    if (!isfinite(count_value) || count_value > 512.0)
        return NULL;
    variable_count = (size_t)count_value;
    if (argument_count != 2u * variable_count + 3u)
        return NULL;
    return expr_lauricella_f(arguments[1], variable_count, (const expr_t *const *)&arguments[2],
                             arguments[2u + variable_count], (const expr_t *const *)&arguments[3u + variable_count]);
}

expr_t *expr_gammainv(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_gammainv, a);
}
expr_t *expr_lambert_w(const expr_t *a)
{
    expr_t *inverse = expr_simplify_try_lambert_argument((expr_t *)a);

    if (inverse)
        return inverse;
    return expr_math_wrap_unary(&ops_lambert_w, a);
}
expr_t *expr_lambert_wn_xp(const expr_t *branch, const expr_t *arg)
{
    if (branch && expr_is_const(branch)) {
        if (num_eq(branch->c, NUM_ZERO))
            return expr_lambert_w0(arg);
        if (num_eq(branch->c, NUM_NEG_ONE))
            return expr_lambert_wm1(arg);
    }
    return expr_math_wrap_binary(&ops_lambert_wn, branch, arg);
}
expr_t *expr_lambert_wn(const expr_t *branch, const expr_t *arg)
{
    return expr_lambert_wn_xp(branch, arg);
}
expr_t *expr_lambert_w0(const expr_t *a)
{
    expr_t *inverse = expr_simplify_try_lambert_argument((expr_t *)a);

    if (inverse)
        return inverse;
    return expr_math_wrap_unary(&ops_lambert_w0, a);
}
expr_t *expr_lambert_wm1(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_lambert_wm1, a);
}
expr_t *expr_normal_pdf(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_normal_pdf, a);
}
expr_t *expr_normal_cdf(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_normal_cdf, a);
}
expr_t *expr_normal_logpdf(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_normal_logpdf, a);
}
expr_t *expr_pdf(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_pdf, a);
}
expr_t *expr_cdf(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_cdf, a);
}
expr_t *expr_logpdf(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_logpdf, a);
}
expr_t *expr_Ei(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_Ei, a);
}
expr_t *expr_E1(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_E1, a);
}
expr_t *expr_beta(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_beta, a, b);
}
expr_t *expr_logbeta(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_logbeta, a, b);
}
expr_t *expr_new_indexed_symbol(const char *name, const expr_t *index)
{
    expr_t *symbol = expr_new_named_const(NUM_NAN, name);
    expr_t *out = symbol && index ? expr_math_wrap_binary(&ops_indexed_symbol, symbol, index) : NULL;

    expr_free(symbol);
    return out;
}

expr_t *expr_new_summation(const expr_t *term, const expr_t *index)
{
    return expr_math_wrap_binary(&ops_summation, term, index);
}
expr_t *expr_new_finite_summation(const expr_t *term, const expr_t *index, const expr_t *upper)
{
    expr_t *bounds = expr_math_wrap_binary(&ops_argument_list, index, upper);
    expr_t *out = bounds ? expr_math_wrap_binary(&ops_summation, term, bounds) : NULL;

    expr_free(bounds);
    return out;
}

/* Build a formal finite summation with explicit inclusive bounds. */
expr_t *expr_new_finite_summation_range(const expr_t *term, const expr_t *index, const expr_t *lower,
                                        const expr_t *upper)
{
    expr_t *range = expr_math_wrap_binary(&ops_argument_list, lower, upper);
    expr_t *bounds = range ? expr_math_wrap_binary(&ops_argument_list, index, range) : NULL;
    expr_t *out = bounds ? expr_math_wrap_binary(&ops_summation, term, bounds) : NULL;

    expr_free(bounds);
    expr_free(range);
    return out;
}

expr_t *expr_new_product(const expr_t *term, const expr_t *index)
{
    return expr_math_wrap_binary(&ops_product, term, index);
}

/* Build a formal finite product with explicit inclusive bounds. */
expr_t *expr_new_finite_product_range(const expr_t *term, const expr_t *index, const expr_t *lower,
                                     const expr_t *upper)
{
    expr_t *range = expr_math_wrap_binary(&ops_argument_list, lower, upper);
    expr_t *bounds = range ? expr_math_wrap_binary(&ops_argument_list, index, range) : NULL;
    expr_t *out = bounds ? expr_math_wrap_binary(&ops_product, term, bounds) : NULL;

    expr_free(bounds);
    expr_free(range);
    return out;
}
expr_t *expr_gammainc_lower(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_gammainc_lower, a, b);
}
expr_t *expr_gammainc_upper(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_gammainc_upper, a, b);
}
expr_t *expr_gammainc_P(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_gammainc_P, a, b);
}
expr_t *expr_gammainc_Q(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_gammainc_Q, a, b);
}
expr_t *expr_factorial(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_factorial, a);
}
expr_t *expr_fibonacci(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_fibonacci, a);
}
expr_t *expr_partition(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_partition, a);
}
expr_t *expr_isqrt(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_isqrt, a);
}
expr_t *expr_gcd(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_gcd, a, b);
}
expr_t *expr_lcm(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_lcm, a, b);
}
expr_t *expr_mod(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_mod, a, b);
}
expr_t *expr_modinv(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_modinv, a, b);
}
expr_t *expr_is_prime(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_is_prime, a);
}
expr_t *expr_next_prime(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_next_prime, a);
}
expr_t *expr_prev_prime(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_prev_prime, a);
}
expr_t *expr_bit_and(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_bit_and, a, b);
}
expr_t *expr_bit_or(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_bit_or, a, b);
}
expr_t *expr_bit_xor(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_bit_xor, a, b);
}
expr_t *expr_bit_not(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_bit_not, a);
}
expr_t *expr_shl(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_shl, a, b);
}
expr_t *expr_shr(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_shr, a, b);
}

static expr_t *expr_factor_product_from_number(number_t value)
{
    enum { FACTOR_MAX_BITS = 1024 };
    number_factors_t *factors;
    expr_t *out = NULL;

    if (!num_is_integer(value) || num_get_sign(value) <= 0 || num_bit_length(value) > FACTOR_MAX_BITS)
        return NULL;

    factors = num_factors(value);
    if (!factors)
        return NULL;

    if (factors->count == 0u) {
        num_factors_free(factors);
        return expr_new_const(NUM_ONE);
    }

    for (size_t i = 0u; i < factors->count; ++i) {
        char name[32];
        expr_t *base;
        expr_t *factor;

        snprintf(name, sizeof(name), "a%zu", i);
        base = expr_new_named_const(factors->items[i].prime, name);
        {
            string_t *prime_text = num_to_string(factors->items[i].prime);

            base->binding_expr = expr_binding_expr_new_number_text(prime_text ? string_c_str(prime_text) : "NAN");
            string_free(prime_text);
        }
        if (factors->items[i].exponent > 1u) {
            number_t exponent = num_create_from_long((long)factors->items[i].exponent);

            factor = expr_pow(base, &exponent);
            num_destroy(&exponent);
            expr_free(base);
        } else {
            factor = base;
        }

        if (out) {
            expr_t *next = expr_mul(out, factor);

            expr_free(out);
            expr_free(factor);
            out = next;
        } else {
            out = factor;
        }
    }

    num_factors_free(factors);
    return out;
}

expr_t *expr_factors(const expr_t *a)
{
    expr_t *product;

    if (!a)
        return NULL;

    product = expr_factor_product_from_number(expr_eval_num_internal(a));
    if (product)
        return product;

    return expr_math_wrap_unary(&ops_factors, a);
}

expr_t *expr_logbeta_pdf(const expr_t *x, const expr_t *a, const expr_t *b)
{
    expr_t *one;
    expr_t *a_minus_one;
    expr_t *b_minus_one;
    expr_t *log_x;
    expr_t *one_minus_x;
    expr_t *log_one_minus_x;
    expr_t *left;
    expr_t *right;
    expr_t *sum;
    expr_t *log_beta;
    expr_t *out;

    if (!x || !a || !b)
        return NULL;

    one = expr_new_const(NUM_ONE);
    a_minus_one = expr_sub(a, one);
    b_minus_one = expr_sub(b, one);
    log_x = expr_log(x);
    one_minus_x = expr_sub(one, x);
    log_one_minus_x = expr_log(one_minus_x);
    left = expr_mul(a_minus_one, log_x);
    right = expr_mul(b_minus_one, log_one_minus_x);
    sum = expr_add(left, right);
    log_beta = expr_logbeta(a, b);
    out = expr_sub(sum, log_beta);

    expr_free(log_beta);
    expr_free(sum);
    expr_free(right);
    expr_free(left);
    expr_free(log_one_minus_x);
    expr_free(one_minus_x);
    expr_free(log_x);
    expr_free(b_minus_one);
    expr_free(a_minus_one);
    expr_free(one);
    return out;
}

expr_t *expr_beta_pdf(const expr_t *x, const expr_t *a, const expr_t *b)
{
    expr_t *log_pdf = expr_logbeta_pdf(x, a, b);
    expr_t *out = log_pdf ? expr_exp(log_pdf) : NULL;

    expr_free(log_pdf);
    return out;
}

expr_t *expr_binomial(const expr_t *n, const expr_t *k)
{
    expr_t *one;
    expr_t *n_plus_one;
    expr_t *k_plus_one;
    expr_t *n_minus_k;
    expr_t *n_minus_k_plus_one;
    expr_t *gamma_n;
    expr_t *gamma_k;
    expr_t *gamma_n_minus_k;
    expr_t *denominator;
    expr_t *out;

    if (!n || !k)
        return NULL;

    one = expr_new_const(NUM_ONE);
    n_plus_one = expr_add(n, one);
    k_plus_one = expr_add(k, one);
    n_minus_k = expr_sub(n, k);
    n_minus_k_plus_one = expr_add(n_minus_k, one);
    gamma_n = expr_gamma(n_plus_one);
    gamma_k = expr_gamma(k_plus_one);
    gamma_n_minus_k = expr_gamma(n_minus_k_plus_one);
    denominator = expr_mul(gamma_k, gamma_n_minus_k);
    out = expr_div(gamma_n, denominator);

    expr_free(denominator);
    expr_free(gamma_n_minus_k);
    expr_free(gamma_k);
    expr_free(gamma_n);
    expr_free(n_minus_k_plus_one);
    expr_free(n_minus_k);
    expr_free(k_plus_one);
    expr_free(n_plus_one);
    expr_free(one);
    return out;
}
