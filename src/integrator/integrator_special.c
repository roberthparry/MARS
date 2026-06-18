#include "integrator_internal.h"
#include "internal/expr_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    IG_POLY_COEFF_COUNT = 6,
    IG_POLY_WORK_COUNT = 11
};

typedef enum {
    IG_SPECIAL_POLY = 0,
    IG_SPECIAL_EXP,
    IG_SPECIAL_SIN,
    IG_SPECIAL_COS,
    IG_SPECIAL_SINH,
    IG_SPECIAL_COSH
} intg_special_kind_t;

typedef struct {
    size_t ndim;
    number_t constant;
    number_t coeffs[4];
    number_t poly[IG_POLY_COEFF_COUNT];
    intg_special_kind_t kind;
} intg_affine_form_t;

typedef struct {
    number_t scale;
    expr_t *factors[4];
} intg_separable_term_t;

static void intg_number_array_zero(number_t *values, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        values[i] = num_clone(NUM_ZERO);
}

static void intg_number_array_clear(number_t *values, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        num_destroy(&values[i]);
}

static void intg_affine_form_init(intg_affine_form_t *form, size_t ndim)
{
    form->ndim = ndim;
    form->constant = num_clone(NUM_ZERO);
    intg_number_array_zero(form->coeffs, 4u);
    intg_number_array_zero(form->poly, IG_POLY_COEFF_COUNT);
    form->kind = IG_SPECIAL_POLY;
}

static void intg_affine_form_clear(intg_affine_form_t *form)
{
    if (!form)
        return;
    num_destroy(&form->constant);
    intg_number_array_clear(form->coeffs, 4u);
    intg_number_array_clear(form->poly, IG_POLY_COEFF_COUNT);
}

static void intg_separable_term_init(intg_separable_term_t *term)
{
    term->scale = num_clone(NUM_ONE);
    for (size_t i = 0; i < 4u; ++i)
        term->factors[i] = NULL;
}

static void intg_separable_term_clear(intg_separable_term_t *term)
{
    if (!term)
        return;
    num_destroy(&term->scale);
    for (size_t i = 0; i < 4u; ++i) {
        expr_free(term->factors[i]);
        term->factors[i] = NULL;
    }
}

static bool intg_number_array_copy(number_t *dst, const number_t *src, size_t count)
{
    if (!dst || !src)
        return false;
    for (size_t i = 0; i < count; ++i) {
        num_destroy(&dst[i]);
        dst[i] = num_clone(src[i]);
    }
    return true;
}

static bool intg_match_affine_term_local(const expr_t *expr,
                                         size_t ndim,
                                         expr_t *const *vars,
                                         number_t scale,
                                         number_t *constant_io,
                                         number_t *coeffs_io)
{
    NUM_SCOPE(scope);
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *base = NULL;
    number_t value = num_new();
    number_t inner_scale = num_new();
    bool is_sub = false;
    size_t index;

    if (!expr || !constant_io || (ndim > 0u && (!vars || !coeffs_io)))
        return false;

    if (expr_match_const_value(expr, &value)) {
        number_t next = num_add(*constant_io, num_mul(scale, value));

        num_destroy(constant_io);
        *constant_io = num_scope_detach(next);
        return true;
    }

    if (expr_match_var_expr(expr, ndim, vars, &index)) {
        number_t next = num_add(coeffs_io[index], scale);

        num_destroy(&coeffs_io[index]);
        coeffs_io[index] = num_scope_detach(next);
        return true;
    }

    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        number_t right_scale = is_sub ? num_neg(scale) : num_clone(scale);

        return intg_match_affine_term_local(left, ndim, vars, scale,
                                            constant_io, coeffs_io) &&
               intg_match_affine_term_local(right, ndim, vars, right_scale,
                                            constant_io, coeffs_io);
    }

    if (expr_match_scaled_expr(expr, &inner_scale, &base) &&
        num_is_real(inner_scale)) {
        number_t product = num_mul(scale, inner_scale);

        return intg_match_affine_term_local(base, ndim, vars, product,
                                            constant_io, coeffs_io);
    }

    return false;
}

static bool intg_match_scaled_affine_power_deg5_local(const expr_t *expr,
                                                      size_t ndim,
                                                      expr_t *const *vars,
                                                      number_t *scale_out,
                                                      number_t *constant_out,
                                                      number_t *coeffs_out)
{
    NUM_SCOPE(scope);
    const expr_t *base = NULL;
    const expr_t *pow_base = NULL;
    number_t inner_scale = num_new();
    number_t degree = num_create_from_long(5);
    number_t pow_degree = num_new();
    number_t constant = num_clone(NUM_ZERO);

    if (!expr || !scale_out || !constant_out || (ndim > 0u && (!vars || !coeffs_out)))
        return false;

    intg_number_array_zero(coeffs_out, ndim);

    if (expr_match_pow_const(expr, &pow_base, &pow_degree) &&
        num_eq(pow_degree, degree) &&
        intg_match_affine_term_local(pow_base, ndim, vars, NUM_ONE, &constant, coeffs_out)) {
        num_destroy(scale_out);
        *scale_out = num_scope_detach(num_clone(NUM_ONE));
        num_destroy(constant_out);
        *constant_out = num_scope_detach(constant);
        return true;
    }
    num_destroy(&constant);

    intg_number_array_zero(coeffs_out, ndim);
    constant = num_clone(NUM_ZERO);
    if (expr_match_scaled_expr(expr, &inner_scale, &base) &&
        num_is_real(inner_scale) &&
        expr_match_pow_const(base, &pow_base, &pow_degree) &&
        num_eq(pow_degree, degree) &&
        intg_match_affine_term_local(pow_base, ndim, vars, NUM_ONE, &constant, coeffs_out)) {
        num_destroy(scale_out);
        *scale_out = num_scope_detach(inner_scale);
        num_destroy(constant_out);
        *constant_out = num_scope_detach(constant);
        return true;
    }

    num_destroy(&constant);
    return false;
}

static bool intg_bounds_are_unit_box(size_t ndim,
                                     const number_t *lo,
                                     const number_t *hi)
{
    if (!lo || !hi)
        return false;
    for (size_t i = 0; i < ndim; ++i) {
        if (!num_eq(lo[i], NUM_ZERO) || !num_eq(hi[i], NUM_ONE))
            return false;
    }
    return true;
}

static bool intg_collect_monomial_powers_local(const expr_t *expr,
                                               size_t ndim,
                                               expr_t *const *vars,
                                               number_t *scale_io,
                                               unsigned int *powers_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *base = NULL;
    number_t value = num_new();
    number_t inner_scale = num_new();
    number_t exponent = num_new();
    number_t degree_one = num_create_from_long(1);
    number_t degree_two = num_create_from_long(2);
    size_t index;

    if (!expr || !scale_io || !powers_out || ndim == 0u || !vars)
        return false;

    if (expr_match_const_value(expr, &value) &&
        num_is_real(value) && num_is_finite(value)) {
        number_t next = num_mul(*scale_io, value);

        num_destroy(scale_io);
        *scale_io = next;
        return true;
    }

    if (expr_match_var_expr(expr, ndim, vars, &index)) {
        powers_out[index] += 1u;
        return true;
    }

    if (expr_match_pow_const(expr, &base, &exponent) &&
        expr_match_var_expr(base, ndim, vars, &index) &&
        (num_eq(exponent, degree_one) || num_eq(exponent, degree_two))) {
        powers_out[index] += num_eq(exponent, degree_two) ? 2u : 1u;
        return true;
    }

    if (expr_match_scaled_expr(expr, &inner_scale, &base) &&
        base != expr &&
        num_is_real(inner_scale) &&
        num_is_finite(inner_scale)) {
        number_t next = num_mul(*scale_io, inner_scale);

        num_destroy(scale_io);
        *scale_io = next;
        return intg_collect_monomial_powers_local(base, ndim, vars, scale_io,
                                                  powers_out);
    }

    if (expr_match_mul_expr(expr, &left, &right))
        return intg_collect_monomial_powers_local(left, ndim, vars, scale_io,
                                                  powers_out) &&
               intg_collect_monomial_powers_local(right, ndim, vars, scale_io,
                                                  powers_out);

    return false;
}

static char *intg_dup_number_text_local(number_t value)
{
    string_t *text = num_to_string(value);
    char *copy = text ? strdup(string_c_str(text)) : NULL;

    string_free(text);
    return copy;
}

static expr_t *intg_build_unit_box_exp_square_product_exact_expr(number_t a)
{
    expr_t *result = NULL;
    char *a_text = NULL;
    char *input = NULL;
    size_t input_len;

    if (!num_is_real(a) || !num_is_finite(a) || !num_gt(a, NUM_ZERO))
        return NULL;

    a_text = intg_dup_number_text_local(a);
    if (!a_text)
        return NULL;

    if (num_eq(a, NUM_ONE)) {
        input = strdup("{ 2*exp(-1) - 2 + 2*sqrt(pi)*erf(1) - gamma - E1(1) }");
    } else {
        input_len = strlen(a_text) * 7u + 128u;
        input = malloc(input_len);
        if (input) {
            snprintf(input, input_len,
                     "{ (2*exp(-(%s)) - 2 + 2*sqrt(pi)*sqrt(%s)*erf(sqrt(%s)) - gamma - ln(%s) - E1(%s))/(%s) }",
                     a_text, a_text, a_text, a_text, a_text, a_text);
        }
    }

    if (input) {
        result = expr_from_string(input, NULL);
        if (result) {
            expr_t *simplified = expr_simplify(result);

            if (simplified) {
                expr_free(result);
                result = simplified;
            }
        }
    }

    free(input);
    free(a_text);
    return result;
}

static int intg_eval_unit_box_exp_square_product(const expr_t *expr,
                                                 size_t ndim,
                                                 expr_t *const *vars,
                                                 const number_t *lo,
                                                 const number_t *hi,
                                                 number_t *result,
                                                 expr_t **exact_result_out)
{
    const expr_t *arg = NULL;
    number_t scale = num_clone(NUM_ONE);
    unsigned int powers[4] = { 0u, 0u, 0u, 0u };
    size_t squared_count = 0u;
    size_t linear_count = 0u;

    if (exact_result_out)
        *exact_result_out = NULL;

    if (!expr || !vars || !result || ndim != 3u || !intg_bounds_are_unit_box(ndim, lo, hi))
        return 0;
    if (!expr_match_exp_expr(expr, &arg))
        return 0;

    if (!intg_collect_monomial_powers_local(arg, ndim, vars, &scale, powers)) {
        num_destroy(&scale);
        return 0;
    }
    if (!num_is_real(scale) || !num_is_finite(scale) || !num_lt(scale, NUM_ZERO)) {
        num_destroy(&scale);
        return 0;
    }

    for (size_t i = 0; i < ndim; ++i) {
        if (powers[i] == 2u)
            squared_count++;
        else if (powers[i] == 1u)
            linear_count++;
        else {
            num_destroy(&scale);
            return 0;
        }
    }
    if (squared_count != 1u || linear_count != 2u) {
        num_destroy(&scale);
        return 0;
    }

    {
        NUM_SCOPE(scope);
        number_t a = num_scope_detach(num_neg(scale));

        num_destroy(&scale);
        if (!num_is_real(a) || !num_is_finite(a) || !num_gt(a, NUM_ZERO)) {
            num_destroy(&a);
            return 0;
        }

        {
            number_t sqrt_a = num_sqrt(a);
            number_t exp_neg_a = num_exp(num_neg(a));
            number_t erf_sqrt_a = num_erf(sqrt_a);
            number_t sqrt_pi = num_sqrt(NUM_PI);
            number_t two = num_create_from_long(2L);
            number_t two_exp = num_mul_long(exp_neg_a, 2L);
            number_t two_sqrt_pi = num_mul_long(sqrt_pi, 2L);
            number_t two_sqrt_pi_sqrt_a = num_mul(two_sqrt_pi, sqrt_a);
            number_t erf_term = num_mul(two_sqrt_pi_sqrt_a, erf_sqrt_a);
            number_t e1_term = num_e1(a);
            number_t log_term = num_log(a);
            number_t sum = num_add(two_exp, erf_term);
            number_t out;

            sum = num_sub(sum, two);
            sum = num_sub(sum, NUM_EULER_MASCHERONI);
            sum = num_sub(sum, log_term);
            sum = num_sub(sum, e1_term);
            out = num_div(sum, a);

            num_destroy(result);
            *result = num_scope_detach(out);
        }
        if (exact_result_out)
            *exact_result_out = intg_build_unit_box_exp_square_product_exact_expr(a);
        num_destroy(&a);
    }

    return 1;
}

static int intg_match_affine_form(const expr_t *expr, size_t ndim,
                                expr_t *const *vars, intg_affine_form_t *form)
{
    number_t constant;
    number_t coeffs[4];
    number_t poly[IG_POLY_COEFF_COUNT];
    number_t scale;
    const expr_t *base = NULL;
    bool matched = false;

    if (!expr || !form || ndim > 4u)
        return 0;

    constant = num_clone(NUM_ZERO);
    scale = num_new();
    intg_number_array_zero(coeffs, 4u);
    intg_number_array_zero(poly, IG_POLY_COEFF_COUNT);

    if (expr_match_scaled_expr(expr, &scale, &base) && base != expr) {
        intg_affine_form_t inner;

        intg_affine_form_init(&inner, ndim);
        if (intg_match_affine_form(base, ndim, vars, &inner)) {
            form->kind = inner.kind;
            num_destroy(&form->constant);
            form->constant = num_clone(inner.constant);
            intg_number_array_copy(form->coeffs, inner.coeffs, 4u);
            for (size_t i = 0; i < IG_POLY_COEFF_COUNT; ++i) {
                number_t scaled = num_mul(inner.poly[i], scale);

                num_destroy(&form->poly[i]);
                form->poly[i] = scaled;
            }
            intg_affine_form_clear(&inner);
            intg_number_array_clear(poly, IG_POLY_COEFF_COUNT);
            intg_number_array_clear(coeffs, 4u);
            num_destroy(&scale);
            num_destroy(&constant);
            return 1;
        }
        intg_affine_form_clear(&inner);
    }
    num_destroy(&scale);

    if (expr_match_affine_poly_deg4(expr, ndim, vars, poly, &constant, coeffs)) {
        form->kind = IG_SPECIAL_POLY;
        matched = true;
    } else if (intg_match_scaled_affine_power_deg5_local(expr, ndim, vars,
                                                         &scale, &constant, coeffs)) {
        form->kind = IG_SPECIAL_POLY;
        num_destroy(&poly[5]);
        poly[5] = num_clone(scale);
        matched = true;
    } else if (expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_EXP, ndim, vars,
                                            &constant, coeffs)) {
        form->kind = IG_SPECIAL_EXP;
        num_destroy(&poly[0]);
        poly[0] = num_clone(NUM_ONE);
        matched = true;
    } else if (expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_SIN, ndim, vars,
                                            &constant, coeffs)) {
        form->kind = IG_SPECIAL_SIN;
        num_destroy(&poly[0]);
        poly[0] = num_clone(NUM_ONE);
        matched = true;
    } else if (expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_COS, ndim, vars,
                                            &constant, coeffs)) {
        form->kind = IG_SPECIAL_COS;
        num_destroy(&poly[0]);
        poly[0] = num_clone(NUM_ONE);
        matched = true;
    } else if (expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_SINH, ndim, vars,
                                            &constant, coeffs)) {
        form->kind = IG_SPECIAL_SINH;
        num_destroy(&poly[0]);
        poly[0] = num_clone(NUM_ONE);
        matched = true;
    } else if (expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_COSH, ndim, vars,
                                            &constant, coeffs)) {
        form->kind = IG_SPECIAL_COSH;
        num_destroy(&poly[0]);
        poly[0] = num_clone(NUM_ONE);
        matched = true;
    } else if (expr_match_affine_poly_deg4_times_unary_affine_kind(
                   expr, EXPR_PATTERN_UNARY_EXP, ndim, vars, poly, &constant, coeffs)) {
        form->kind = IG_SPECIAL_EXP;
        matched = true;
    } else if (expr_match_affine_poly_deg4_times_unary_affine_kind(
                   expr, EXPR_PATTERN_UNARY_SIN, ndim, vars, poly, &constant, coeffs)) {
        form->kind = IG_SPECIAL_SIN;
        matched = true;
    } else if (expr_match_affine_poly_deg4_times_unary_affine_kind(
                   expr, EXPR_PATTERN_UNARY_COS, ndim, vars, poly, &constant, coeffs)) {
        form->kind = IG_SPECIAL_COS;
        matched = true;
    } else if (expr_match_affine_poly_deg4_times_unary_affine_kind(
                   expr, EXPR_PATTERN_UNARY_SINH, ndim, vars, poly, &constant, coeffs)) {
        form->kind = IG_SPECIAL_SINH;
        matched = true;
    } else if (expr_match_affine_poly_deg4_times_unary_affine_kind(
                   expr, EXPR_PATTERN_UNARY_COSH, ndim, vars, poly, &constant, coeffs)) {
        form->kind = IG_SPECIAL_COSH;
        matched = true;
    }

    if (matched) {
        num_destroy(&form->constant);
        form->constant = num_clone(constant);
        intg_number_array_copy(form->coeffs, coeffs, 4u);
        intg_number_array_copy(form->poly, poly, IG_POLY_COEFF_COUNT);
    }

    intg_number_array_clear(poly, IG_POLY_COEFF_COUNT);
    intg_number_array_clear(coeffs, 4u);
    num_destroy(&constant);
    return matched ? 1 : 0;
}

static number_t intg_poly_eval(const number_t *coeffs, size_t count, number_t x)
{
    number_t acc = num_clone(NUM_ZERO);

    for (size_t i = count; i-- > 0u;) {
        number_t next = num_add(num_mul(acc, x), coeffs[i]);

        num_destroy(&acc);
        acc = next;
    }
    return acc;
}

static void intg_poly_antiderivative_once(const number_t *src, size_t src_count,
                                        number_t *dst, size_t dst_count)
{
    intg_number_array_zero(dst, dst_count);
    for (size_t i = 0; i < src_count && (i + 1u) < dst_count; ++i) {
        number_t denom = num_create_from_long((long)(i + 1u));

        num_destroy(&dst[i + 1u]);
        dst[i + 1u] = num_div(src[i], denom);
        num_destroy(&denom);
    }
}

static void intg_exp_antiderivative_once(const number_t *src, size_t count, number_t *dst)
{
    intg_number_array_zero(dst, count);
    if (count == 0u)
        return;

    num_destroy(&dst[count - 1u]);
    dst[count - 1u] = num_clone(src[count - 1u]);

    for (size_t i = count - 1u; i-- > 0u;) {
        number_t factor = num_create_from_long((long)(i + 1u));
        number_t tail = num_mul(factor, dst[i + 1u]);
        number_t next = num_sub(src[i], tail);

        num_destroy(&dst[i]);
        dst[i] = next;
        num_destroy(&tail);
        num_destroy(&factor);
    }
}

static void intg_trig_antiderivative_once(const number_t *a_src, const number_t *b_src,
                                        size_t count, number_t *a_dst, number_t *b_dst)
{
    intg_number_array_zero(a_dst, count);
    intg_number_array_zero(b_dst, count);
    if (count == 0u)
        return;

    num_destroy(&a_dst[count - 1u]);
    a_dst[count - 1u] = num_clone(b_src[count - 1u]);
    num_destroy(&b_dst[count - 1u]);
    b_dst[count - 1u] = num_neg(a_src[count - 1u]);

    for (size_t i = count - 1u; i-- > 0u;) {
        number_t factor = num_create_from_long((long)(i + 1u));
        number_t term_b = num_mul(factor, b_dst[i + 1u]);
        number_t term_a = num_mul(factor, a_dst[i + 1u]);
        number_t next_a = num_sub(b_src[i], term_b);
        number_t next_b = num_sub(term_a, a_src[i]);

        num_destroy(&a_dst[i]);
        a_dst[i] = next_a;
        num_destroy(&b_dst[i]);
        b_dst[i] = next_b;
        num_destroy(&term_a);
        num_destroy(&term_b);
        num_destroy(&factor);
    }
}

static void intg_hyperbolic_antiderivative_once(const number_t *a_src, const number_t *b_src,
                                              size_t count, number_t *a_dst, number_t *b_dst)
{
    intg_number_array_zero(a_dst, count);
    intg_number_array_zero(b_dst, count);
    if (count == 0u)
        return;

    num_destroy(&a_dst[count - 1u]);
    a_dst[count - 1u] = num_clone(b_src[count - 1u]);
    num_destroy(&b_dst[count - 1u]);
    b_dst[count - 1u] = num_clone(a_src[count - 1u]);

    for (size_t i = count - 1u; i-- > 0u;) {
        number_t factor = num_create_from_long((long)(i + 1u));
        number_t term_a = num_mul(factor, a_dst[i + 1u]);
        number_t term_b = num_mul(factor, b_dst[i + 1u]);
        number_t next_b = num_sub(a_src[i], term_a);
        number_t next_a = num_sub(b_src[i], term_b);

        num_destroy(&a_dst[i]);
        a_dst[i] = next_a;
        num_destroy(&b_dst[i]);
        b_dst[i] = next_b;
        num_destroy(&term_b);
        num_destroy(&term_a);
        num_destroy(&factor);
    }
}

static number_t intg_eval_special_antiderivative(const intg_affine_form_t *form,
                                               size_t order, number_t x)
{
    if (form->kind == IG_SPECIAL_POLY) {
        number_t current[IG_POLY_WORK_COUNT];
        number_t next[IG_POLY_WORK_COUNT];
        number_t out;

        intg_number_array_zero(current, IG_POLY_WORK_COUNT);
        intg_number_array_zero(next, IG_POLY_WORK_COUNT);
        for (size_t i = 0; i < IG_POLY_COEFF_COUNT; ++i) {
            num_destroy(&current[i]);
            current[i] = num_clone(form->poly[i]);
        }
        for (size_t i = 0; i < order; ++i) {
            intg_poly_antiderivative_once(current, IG_POLY_WORK_COUNT, next, IG_POLY_WORK_COUNT);
            intg_number_array_clear(current, IG_POLY_WORK_COUNT);
            for (size_t j = 0; j < IG_POLY_WORK_COUNT; ++j) {
                current[j] = next[j];
                next[j] = num_clone(NUM_ZERO);
            }
        }
        out = intg_poly_eval(current, IG_POLY_WORK_COUNT, x);
        intg_number_array_clear(current, IG_POLY_WORK_COUNT);
        intg_number_array_clear(next, IG_POLY_WORK_COUNT);
        return out;
    }

    if (form->kind == IG_SPECIAL_EXP) {
        number_t current[IG_POLY_COEFF_COUNT];
        number_t next[IG_POLY_COEFF_COUNT];
        number_t poly;
        number_t value;

        intg_number_array_zero(current, IG_POLY_COEFF_COUNT);
        intg_number_array_zero(next, IG_POLY_COEFF_COUNT);
        for (size_t i = 0; i < IG_POLY_COEFF_COUNT; ++i) {
            num_destroy(&current[i]);
            current[i] = num_clone(form->poly[i]);
        }
        for (size_t i = 0; i < order; ++i) {
            intg_exp_antiderivative_once(current, IG_POLY_COEFF_COUNT, next);
            intg_number_array_clear(current, IG_POLY_COEFF_COUNT);
            for (size_t j = 0; j < IG_POLY_COEFF_COUNT; ++j) {
                current[j] = next[j];
                next[j] = num_clone(NUM_ZERO);
            }
        }
        poly = intg_poly_eval(current, IG_POLY_COEFF_COUNT, x);
        value = num_mul(poly, num_exp(x));
        num_destroy(&poly);
        intg_number_array_clear(current, IG_POLY_COEFF_COUNT);
        intg_number_array_clear(next, IG_POLY_COEFF_COUNT);
        return value;
    }

    {
        number_t a_cur[IG_POLY_COEFF_COUNT];
        number_t b_cur[IG_POLY_COEFF_COUNT];
        number_t a_next[IG_POLY_COEFF_COUNT];
        number_t b_next[IG_POLY_COEFF_COUNT];
        number_t pa;
        number_t pb;
        number_t value;
        bool circular = (form->kind == IG_SPECIAL_SIN || form->kind == IG_SPECIAL_COS);

        intg_number_array_zero(a_cur, IG_POLY_COEFF_COUNT);
        intg_number_array_zero(b_cur, IG_POLY_COEFF_COUNT);
        intg_number_array_zero(a_next, IG_POLY_COEFF_COUNT);
        intg_number_array_zero(b_next, IG_POLY_COEFF_COUNT);

        if (form->kind == IG_SPECIAL_SIN || form->kind == IG_SPECIAL_SINH) {
            for (size_t i = 0; i < IG_POLY_COEFF_COUNT; ++i) {
                num_destroy(&a_cur[i]);
                a_cur[i] = num_clone(form->poly[i]);
            }
        } else {
            for (size_t i = 0; i < IG_POLY_COEFF_COUNT; ++i) {
                num_destroy(&b_cur[i]);
                b_cur[i] = num_clone(form->poly[i]);
            }
        }

        for (size_t i = 0; i < order; ++i) {
            if (circular) {
                intg_trig_antiderivative_once(a_cur, b_cur, IG_POLY_COEFF_COUNT, a_next, b_next);
            } else {
                intg_hyperbolic_antiderivative_once(a_cur, b_cur, IG_POLY_COEFF_COUNT, a_next, b_next);
            }
            intg_number_array_clear(a_cur, IG_POLY_COEFF_COUNT);
            intg_number_array_clear(b_cur, IG_POLY_COEFF_COUNT);
            for (size_t j = 0; j < IG_POLY_COEFF_COUNT; ++j) {
                a_cur[j] = a_next[j];
                b_cur[j] = b_next[j];
                a_next[j] = num_clone(NUM_ZERO);
                b_next[j] = num_clone(NUM_ZERO);
            }
        }

        pa = intg_poly_eval(a_cur, IG_POLY_COEFF_COUNT, x);
        pb = intg_poly_eval(b_cur, IG_POLY_COEFF_COUNT, x);
        if (circular) {
            number_t s = num_sin(x);
            number_t c = num_cos(x);
            number_t as = num_mul(pa, s);
            number_t bc = num_mul(pb, c);

            value = num_add(as, bc);
            num_destroy(&bc);
            num_destroy(&as);
            num_destroy(&c);
            num_destroy(&s);
        } else {
            number_t s = num_sinh(x);
            number_t c = num_cosh(x);
            number_t as = num_mul(pa, s);
            number_t bc = num_mul(pb, c);

            value = num_add(as, bc);
            num_destroy(&bc);
            num_destroy(&as);
            num_destroy(&c);
            num_destroy(&s);
        }
        num_destroy(&pb);
        num_destroy(&pa);
        intg_number_array_clear(a_cur, IG_POLY_COEFF_COUNT);
        intg_number_array_clear(b_cur, IG_POLY_COEFF_COUNT);
        intg_number_array_clear(a_next, IG_POLY_COEFF_COUNT);
        intg_number_array_clear(b_next, IG_POLY_COEFF_COUNT);
        return value;
    }
}

static int intg_affine_integral_box(const intg_affine_form_t *form,
                                  const number_t *lo, const number_t *hi,
                                  number_t *result)
{
    size_t active[4];
    size_t active_count = 0u;
    number_t zero_scale = num_clone(NUM_ONE);
    number_t denom = num_clone(NUM_ONE);
    number_t total = num_clone(NUM_ZERO);

    for (size_t i = 0; i < form->ndim; ++i) {
        if (num_is_zero(form->coeffs[i])) {
            number_t width = num_sub(hi[i], lo[i]);
            number_t next = num_mul(zero_scale, width);

            num_destroy(&zero_scale);
            zero_scale = next;
            num_destroy(&width);
        } else {
            active[active_count++] = i;
            {
                number_t next = num_mul(denom, form->coeffs[i]);

                num_destroy(&denom);
                denom = next;
            }
        }
    }

    if (active_count == 0u) {
        number_t s = num_clone(form->constant);
        number_t base = intg_eval_special_antiderivative(form, 0u, s);
        number_t value = num_mul(base, zero_scale);

        num_destroy(&base);
        num_destroy(&s);
        *result = value;
        num_destroy(&total);
        num_destroy(&denom);
        num_destroy(&zero_scale);
        return 1;
    }

    for (size_t mask = 0u; mask < (1u << active_count); ++mask) {
        number_t s = num_clone(form->constant);
        number_t term;

        for (size_t bit = 0; bit < active_count; ++bit) {
            size_t axis = active[bit];
            const number_t bound = ((mask >> bit) & 1u) ? hi[axis] : lo[axis];
            number_t contrib = num_mul(form->coeffs[axis], bound);
            number_t next = num_add(s, contrib);

            num_destroy(&s);
            s = next;
            num_destroy(&contrib);
        }

        term = intg_eval_special_antiderivative(form, active_count, s);
        if (((active_count - __builtin_popcount((unsigned)mask)) & 1u) != 0u) {
            number_t neg = num_neg(term);

            num_destroy(&term);
            term = neg;
        }
        {
            number_t next = num_add(total, term);

            num_destroy(&total);
            total = next;
        }
        num_destroy(&term);
        num_destroy(&s);
    }

    {
        number_t scaled = num_div(total, denom);
        number_t value = num_mul(scaled, zero_scale);

        num_destroy(&scaled);
        *result = value;
    }
    num_destroy(&total);
    num_destroy(&denom);
    num_destroy(&zero_scale);
    return 1;
}

static int intg_eval_affine_expr(const expr_t *expr,
                               size_t ndim,
                               expr_t *const *vars,
                               const number_t *lo,
                               const number_t *hi,
                               number_t *result)
{
    intg_affine_form_t form;
    int matched;

    intg_affine_form_init(&form, ndim);
    matched = intg_match_affine_form(expr, ndim, vars, &form);
    if (!matched) {
        intg_affine_form_clear(&form);
        return 0;
    }

    if (intg_affine_integral_box(&form, lo, hi, result) != 1) {
        intg_affine_form_clear(&form);
        return -1;
    }

    intg_affine_form_clear(&form);
    return 1;
}

static int intg_term_attach_factor(intg_separable_term_t *term,
                                 size_t axis, const expr_t *factor)
{
    expr_t *combined;

    if (!term || !factor || axis >= 4u)
        return 0;
    if (!term->factors[axis]) {
        expr_retain(factor);
        term->factors[axis] = (expr_t *)factor;
        return 1;
    }

    combined = expr_mul(term->factors[axis], factor);
    if (!combined)
        return 0;
    expr_free(term->factors[axis]);
    term->factors[axis] = combined;
    return 1;
}

static int intg_collect_separable_term(const expr_t *expr,
                                     size_t ndim,
                                     expr_t *const *vars,
                                     intg_separable_term_t *term)
{
    number_t scale = num_new();
    const expr_t *base = NULL;
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool used[4] = { false, false, false, false };
    size_t used_count = 0u;

    if (!expr || !term || ndim > 4u)
        return 0;

    if (expr_match_scaled_expr(expr, &scale, &base) && base != expr) {
        number_t next = num_mul(term->scale, scale);

        num_destroy(&term->scale);
        term->scale = next;
        num_destroy(&scale);
        return intg_collect_separable_term(base, ndim, vars, term);
    }
    num_destroy(&scale);

    if (expr_match_mul_expr(expr, &left, &right))
        return intg_collect_separable_term(left, ndim, vars, term) &&
               intg_collect_separable_term(right, ndim, vars, term);

    if (!expr_collect_var_usage(expr, ndim, vars, used))
        return 0;
    for (size_t i = 0; i < ndim; ++i)
        used_count += used[i] ? 1u : 0u;

    if (used_count == 0u) {
        number_t value = expr_eval(expr);
        number_t next;

        if (!num_is_real(value) || !num_is_finite(value)) {
            num_destroy(&value);
            return 0;
        }
        next = num_mul(term->scale, value);
        num_destroy(&term->scale);
        term->scale = next;
        num_destroy(&value);
        return 1;
    }

    if (used_count != 1u)
        return 0;

    for (size_t i = 0; i < ndim; ++i)
        if (used[i])
            return intg_term_attach_factor(term, i, expr);
    return 0;
}

static int intg_eval_separable_term(integrator_t *ig,
                                  const expr_t *expr,
                                  size_t ndim,
                                  expr_t *const *vars,
                                  const number_t *lo,
                                  const number_t *hi,
                                  number_t *result)
{
    intg_separable_term_t term;
    number_t value = num_new();

    intg_separable_term_init(&term);
    if (!intg_collect_separable_term(expr, ndim, vars, &term)) {
        intg_separable_term_clear(&term);
        return 0;
    }

    value = num_clone(term.scale);
    for (size_t i = 0; i < ndim; ++i) {
        number_t factor_value = num_new();
        int status;

        if (term.factors[i]) {
            expr_t *one_var[1];

            one_var[0] = vars[i];
            status = intg_eval_affine_expr(term.factors[i], 1u, one_var,
                                         &lo[i], &hi[i], &factor_value);
            if (status == 0)
                status = intg_integral(ig, term.factors[i], vars[i],
                                            lo[i], hi[i], &factor_value, NULL);
            if (status < 0) {
                intg_separable_term_clear(&term);
                num_destroy(&value);
                return -1;
            }
        } else {
            factor_value = num_sub(hi[i], lo[i]);
        }

        {
            number_t next = num_mul(value, factor_value);

            num_destroy(&value);
            value = next;
        }
        num_destroy(&factor_value);
    }

    *result = value;
    intg_separable_term_clear(&term);
    return 1;
}

static int intg_eval_separable_expr(integrator_t *ig,
                                  const expr_t *expr,
                                  size_t ndim,
                                  expr_t *const *vars,
                                  const number_t *lo,
                                  const number_t *hi,
                                  number_t *result)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool is_sub = false;
    int affine_status;

    affine_status = intg_eval_affine_expr(expr, ndim, vars, lo, hi, result);
    if (affine_status != 0)
        return affine_status;

    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        number_t lhs = num_new();
        number_t rhs = num_new();
        number_t out;
        int s_left = intg_eval_separable_expr(ig, left, ndim, vars, lo, hi, &lhs);
        int s_right = intg_eval_separable_expr(ig, right, ndim, vars, lo, hi, &rhs);

        if (s_left <= 0 || s_right <= 0) {
            num_destroy(&rhs);
            num_destroy(&lhs);
            return (s_left < 0 || s_right < 0) ? -1 : 0;
        }
        out = is_sub ? num_sub(lhs, rhs) : num_add(lhs, rhs);
        num_destroy(&rhs);
        num_destroy(&lhs);
        *result = out;
        return 1;
    }

    return intg_eval_separable_term(ig, expr, ndim, vars, lo, hi, result);
}

int try_integral_multi_special_affine(integrator_t *ig, expr_t *expr,
                                      size_t ndim, expr_t *const *vars,
                                      const number_t *lo, const number_t *hi,
                                      number_t *result, number_t *error_est)
{
    intg_affine_form_t form;
    int matched;
    number_t value = num_new();
    expr_t *exact_result = NULL;

    (void)ig;

    if (!expr || !vars || !lo || !hi || !result || ndim == 0u || ndim > 4u)
        return 0;

    matched = intg_eval_unit_box_exp_square_product(expr, ndim, vars, lo, hi, &value,
                                                    &exact_result);
    if (matched > 0) {
        *result = value;
        if (error_est)
            *error_est = num_clone(NUM_ZERO);
        intg_set_exact_result_owned(ig, exact_result);
        if (ig)
            ig->last_intervals = 1u;
        return 1;
    }

    intg_affine_form_init(&form, ndim);
    matched = intg_match_affine_form(expr, ndim, vars, &form);
    if (!matched) {
        int separable_status;

        if (ndim == 1u) {
            num_destroy(&value);
            intg_affine_form_clear(&form);
            return 0;
        }

        separable_status = intg_eval_separable_expr(ig, expr, ndim, vars, lo, hi, &value);

        if (separable_status > 0) {
            *result = value;
            if (error_est)
                *error_est = num_clone(NUM_ZERO);
            if (ig)
                ig->last_intervals = 1u;
            intg_affine_form_clear(&form);
            return 1;
        }
        num_destroy(&value);
        intg_affine_form_clear(&form);
        return separable_status < 0 ? -1 : 0;
    }

    if (intg_affine_integral_box(&form, lo, hi, &value) != 1) {
        intg_affine_form_clear(&form);
        num_destroy(&value);
        return -1;
    }

    *result = value;
    if (error_est)
        *error_est = num_clone(NUM_ZERO);
    if (ig)
        ig->last_intervals = 1u;
    intg_affine_form_clear(&form);
    return 1;
}
