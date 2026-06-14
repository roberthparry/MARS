#include <stdbool.h>

#include "expr_integrate_internal.h"

bool depends_on_wrt(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    bool used[1];

    vars[0] = (expr_t *)wrt;
    return expr_collect_var_usage(expr, 1u, vars, used) && used[0];
}

bool expr_equal_exact_local(const expr_t *a, const expr_t *b)
{
    expr_t *diff;
    expr_t *simplified;
    bool equal;

    if (a == b)
        return true;
    if (!a || !b)
        return false;

    diff = expr_sub(a, b);
    simplified = simplify_owned(diff);
    if (!simplified)
        return false;
    equal = expr_is_exact_zero(simplified);
    expr_free(simplified);
    return equal;
}

bool is_wrt(const expr_t *expr, const expr_t *wrt)
{
    return expr && wrt && expr_is_var(expr) && expr_is_var(wrt) &&
           (expr == wrt ||
            (expr->var_id != 0 && expr->var_id == wrt->var_id));
}

static bool match_affine_linear_expr(const expr_t *expr,
                                     const expr_t *wrt,
                                     bool require_nonzero_coeff,
                                     number_t *constant_out,
                                     number_t *coeff_out)
{
    expr_t *vars[1];
    number_t poly[5];
    number_t basis_constant = num_new();
    number_t basis_coeffs[1];
    number_t affine_constant;
    number_t affine_coeff;
    bool ok;

    vars[0] = (expr_t *)wrt;
    for (size_t i = 0; i < 5; ++i)
        poly[i] = num_new();
    basis_coeffs[0] = num_new();
    ok = expr_match_affine_poly_deg4(expr, 1u, vars, poly, &basis_constant,
                                     basis_coeffs) &&
         num_is_zero(poly[2]) &&
         num_is_zero(poly[3]) &&
         num_is_zero(poly[4]) &&
         (!require_nonzero_coeff ||
          (!num_is_zero(poly[1]) && !num_is_zero(basis_coeffs[0])));
    if (!ok) {
        for (size_t i = 0; i < 5; ++i)
            num_destroy(&poly[i]);
        num_destroy(&basis_constant);
        num_destroy(&basis_coeffs[0]);
        return false;
    }

    {
        number_t constant_offset = num_mul(poly[1], basis_constant);

        affine_constant = num_add(poly[0], constant_offset);
        num_destroy(&constant_offset);
    }
    affine_coeff = num_mul(poly[1], basis_coeffs[0]);

    num_destroy(constant_out);
    *constant_out = affine_constant;
    num_destroy(coeff_out);
    *coeff_out = affine_coeff;

    for (size_t i = 0; i < 5; ++i)
        num_destroy(&poly[i]);
    num_destroy(&basis_constant);
    num_destroy(&basis_coeffs[0]);
    return true;
}

bool match_nonconstant_affine_linear_expr(const expr_t *expr,
                                          const expr_t *wrt,
                                          number_t *constant_out,
                                          number_t *coeff_out)
{
    return match_affine_linear_expr(expr, wrt, true, constant_out, coeff_out);
}

void number_array_zero_local(number_t *values, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        values[i] = num_new();
}

void number_array_clear_local(number_t *values, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        num_destroy(&values[i]);
}

expr_t *build_affine_from_match(const expr_t *wrt,
                                number_t constant,
                                number_t coeff)
{
    expr_t *scaled;
    expr_t *shifted;

    if (num_eq(coeff, NUM_ZERO))
        return expr_new_const(constant);

    scaled = expr_mul_num(wrt, &coeff);
    if (!scaled)
        return NULL;
    if (num_eq(constant, NUM_ZERO))
        return simplify_owned(scaled);

    shifted = expr_add_num(scaled, &constant);
    expr_free(scaled);
    return simplify_owned(shifted);
}

expr_t *build_polynomial_expr(const expr_t *var,
                              const number_t *coeffs,
                              size_t count)
{
    expr_t *acc = NULL;

    for (size_t i = count; i-- > 0u;) {
        expr_t *next = expr_new_const(coeffs[i]);

        if (!next) {
            expr_free(acc);
            return NULL;
        }
        if (acc) {
            expr_t *product = expr_mul(acc, var);
            expr_t *sum;

            expr_free(acc);
            if (!product) {
                expr_free(next);
                return NULL;
            }
            sum = expr_add(product, next);
            expr_free(product);
            expr_free(next);
            acc = sum;
        } else {
            acc = next;
        }
    }

    return simplify_owned(acc);
}

bool affine_linear_match_eq(number_t constant_a,
                            number_t coeff_a,
                            number_t constant_b,
                            number_t coeff_b)
{
    return num_eq(constant_a, constant_b) && num_eq(coeff_a, coeff_b);
}

static bool match_affine_square_expr(const expr_t *expr,
                                     const expr_t *wrt,
                                     number_t *constant_out,
                                     number_t *coeff_out)
{
    number_t exponent = num_new();
    bool ok = false;

    if (!expr) {
        num_destroy(&exponent);
        return false;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && num_eq(expr->c, NUM_TWO)) {
        ok = match_nonconstant_affine_linear_expr(expr->a, wrt, constant_out, coeff_out);
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
               expr->b && expr_match_const_value(expr->b, &exponent) &&
               num_eq(exponent, NUM_TWO)) {
        ok = match_nonconstant_affine_linear_expr(expr->a, wrt, constant_out, coeff_out);
    }

    num_destroy(&exponent);
    return ok;
}

bool match_one_plus_minus_affine_square(const expr_t *expr,
                                        const expr_t *wrt,
                                        bool *is_plus_out,
                                        number_t *constant_out,
                                        number_t *coeff_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool is_sub = false;
    number_t one = num_new();
    bool ok = false;
    const expr_t *inner = expr;

    if (expr && expr->ops && expr->ops->kind == EXPR_KIND_NEG)
        inner = expr->a;

    if (!expr_match_add_sub_expr(inner, &left, &right, &is_sub)) {
        num_destroy(&one);
        return false;
    }

    if (expr_match_const_value(left, &one) && num_eq(one, NUM_ONE) &&
        match_affine_square_expr(right, wrt, constant_out, coeff_out)) {
        *is_plus_out = !is_sub;
        ok = true;
    } else if (!is_sub &&
               expr_match_const_value(left, &one) && num_eq(one, NUM_ONE) &&
               right && right->ops && right->ops->kind == EXPR_KIND_NEG &&
               match_affine_square_expr(right->a, wrt, constant_out, coeff_out)) {
        *is_plus_out = false;
        ok = true;
    } else if (!is_sub &&
               expr_match_const_value(right, &one) && num_eq(one, NUM_ONE) &&
               match_affine_square_expr(left, wrt, constant_out, coeff_out)) {
        *is_plus_out = true;
        ok = true;
    } else if (!is_sub &&
               expr_match_const_value(right, &one) && num_eq(one, NUM_ONE) &&
               left && left->ops && left->ops->kind == EXPR_KIND_NEG &&
               match_affine_square_expr(left->a, wrt, constant_out, coeff_out)) {
        *is_plus_out = false;
        ok = true;
    }

    num_destroy(&one);
    return ok;
}

bool match_affine_unary_data(const expr_t *expr,
                             const expr_t *wrt,
                             expr_pattern_unary_affine_kind_t kind,
                             number_t *constant_out,
                             number_t *coeff_out)
{
    expr_t *vars[1];
    number_t coeffs[1];
    bool ok;

    vars[0] = (expr_t *)wrt;
    coeffs[0] = num_new();
    ok = expr_match_unary_affine_kind(expr, kind, 1u, vars, constant_out, coeffs) &&
         !num_eq(coeffs[0], NUM_ZERO);
    if (ok) {
        num_destroy(coeff_out);
        *coeff_out = coeffs[0];
    } else {
        num_destroy(&coeffs[0]);
    }
    return ok;
}

bool match_affine_unary(const expr_t *expr,
                        const expr_t *wrt,
                        expr_pattern_unary_affine_kind_t kind,
                        number_t *constant_out,
                        number_t *coeff_out)
{
    return match_affine_unary_data(expr, wrt, kind, constant_out, coeff_out);
}

expr_t *integrate_affine_unary_kind(const expr_t *expr,
                                    const expr_t *wrt,
                                    expr_pattern_unary_affine_kind_t kind,
                                    expr_apply_unary_fn antiderivative_fn,
                                    number_t sign)
{
    expr_t *vars[1];
    number_t constant = num_new();
    number_t coeffs[1];
    expr_t *anti;
    expr_t *out;

    vars[0] = (expr_t *)wrt;
    coeffs[0] = num_new();
    if (!expr_match_unary_affine_kind(expr, kind, 1u, vars, &constant, coeffs) ||
        num_eq(coeffs[0], NUM_ZERO)) {
        num_destroy(&coeffs[0]);
        num_destroy(&constant);
        return NULL;
    }

    anti = antiderivative_fn(expr->a);
    if (num_eq(sign, NUM_NEG_ONE)) {
        expr_t *negated = anti ? expr_neg(anti) : NULL;

        expr_free(anti);
        anti = negated;
    }

    out = div_number_owned_consuming(anti, &coeffs[0]);
    num_destroy(&constant);
    return out;
}

void exp_antiderivative_once_local(const number_t *src, size_t count, number_t *dst)
{
    number_array_zero_local(dst, count);
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

void trig_antiderivative_once_local(const number_t *a_src,
                                    const number_t *b_src,
                                    size_t count,
                                    number_t *a_dst,
                                    number_t *b_dst)
{
    number_array_zero_local(a_dst, count);
    number_array_zero_local(b_dst, count);
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

void hyperbolic_antiderivative_once_local(const number_t *a_src,
                                          const number_t *b_src,
                                          size_t count,
                                          number_t *a_dst,
                                          number_t *b_dst)
{
    number_array_zero_local(a_dst, count);
    number_array_zero_local(b_dst, count);
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
