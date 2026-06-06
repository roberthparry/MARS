#include <stdbool.h>
#include <stddef.h>

#include "expr_integrate_internal.h"

typedef struct {
    expr_t *coeff[3];
} symbolic_poly2_t;

static expr_t *retain_expr_local_quadratic(const expr_t *expr)
{
    if (!expr)
        return NULL;
    expr_retain(expr);
    return (expr_t *)expr;
}

static void symbolic_poly2_clear(symbolic_poly2_t *poly)
{
    if (!poly)
        return;
    for (size_t i = 0; i < 3u; ++i) {
        expr_free(poly->coeff[i]);
        poly->coeff[i] = NULL;
    }
}

static expr_t *expr_const_long_local(long value)
{
    number_t number = num_create_from_long(value);
    expr_t *expr = expr_new_const(number);

    num_destroy(&number);
    return expr;
}

static bool symbolic_poly2_add_owned(symbolic_poly2_t *poly, size_t degree, expr_t *term)
{
    expr_t *sum;

    if (!poly || degree >= 3u) {
        expr_free(term);
        return false;
    }
    if (!term)
        return false;

    if (!poly->coeff[degree]) {
        poly->coeff[degree] = simplify_owned(term);
        return poly->coeff[degree] != NULL;
    }

    sum = expr_add(poly->coeff[degree], term);
    expr_free(term);
    expr_free(poly->coeff[degree]);
    poly->coeff[degree] = simplify_owned(sum);
    return poly->coeff[degree] != NULL;
}

static bool symbolic_poly2_add_const(symbolic_poly2_t *poly, size_t degree, long value)
{
    return symbolic_poly2_add_owned(poly, degree, expr_const_long_local(value));
}

static bool symbolic_poly2_collect(const expr_t *expr, const expr_t *wrt, symbolic_poly2_t *poly);

static bool symbolic_poly2_collect_mul(const expr_t *left,
                                       const expr_t *right,
                                       const expr_t *wrt,
                                       symbolic_poly2_t *poly)
{
    symbolic_poly2_t left_poly = { 0 };
    symbolic_poly2_t right_poly = { 0 };
    bool ok = false;

    if (!symbolic_poly2_collect(left, wrt, &left_poly) ||
        !symbolic_poly2_collect(right, wrt, &right_poly))
        goto cleanup;

    for (size_t i = 0; i < 3u; ++i) {
        if (!left_poly.coeff[i])
            continue;
        for (size_t j = 0; j < 3u; ++j) {
            expr_t *term;

            if (!right_poly.coeff[j])
                continue;
            if (i + j >= 3u)
                goto cleanup;

            term = expr_mul(left_poly.coeff[i], right_poly.coeff[j]);
            if (!symbolic_poly2_add_owned(poly, i + j, term))
                goto cleanup;
        }
    }

    ok = true;

cleanup:
    symbolic_poly2_clear(&right_poly);
    symbolic_poly2_clear(&left_poly);
    return ok;
}

static bool symbolic_poly2_collect(const expr_t *expr, const expr_t *wrt, symbolic_poly2_t *poly)
{
    number_t exponent = num_new();
    bool ok = false;

    if (!expr || !wrt || !poly)
        goto cleanup;

    if (!depends_on_wrt(expr, wrt)) {
        ok = symbolic_poly2_add_owned(poly, 0u, retain_expr_local_quadratic(expr));
        goto cleanup;
    }

    if (is_wrt(expr, wrt)) {
        ok = symbolic_poly2_add_const(poly, 1u, 1L);
        goto cleanup;
    }

    if (expr_is_op(expr, &ops_neg) && expr->a) {
        symbolic_poly2_t inner = { 0 };

        if (!symbolic_poly2_collect(expr->a, wrt, &inner)) {
            symbolic_poly2_clear(&inner);
            goto cleanup;
        }
        for (size_t i = 0; i < 3u; ++i) {
            expr_t *negated;

            if (!inner.coeff[i])
                continue;
            negated = expr_neg(inner.coeff[i]);
            if (!symbolic_poly2_add_owned(poly, i, negated)) {
                symbolic_poly2_clear(&inner);
                goto cleanup;
            }
        }
        symbolic_poly2_clear(&inner);
        ok = true;
        goto cleanup;
    }

    if (expr_is_op(expr, &ops_add) && expr->a && expr->b) {
        ok = symbolic_poly2_collect(expr->a, wrt, poly) &&
             symbolic_poly2_collect(expr->b, wrt, poly);
        goto cleanup;
    }

    if (expr_is_op(expr, &ops_sub) && expr->a && expr->b) {
        symbolic_poly2_t right_poly = { 0 };

        if (!symbolic_poly2_collect(expr->a, wrt, poly) ||
            !symbolic_poly2_collect(expr->b, wrt, &right_poly)) {
            symbolic_poly2_clear(&right_poly);
            goto cleanup;
        }
        for (size_t i = 0; i < 3u; ++i) {
            expr_t *negated;

            if (!right_poly.coeff[i])
                continue;
            negated = expr_neg(right_poly.coeff[i]);
            if (!symbolic_poly2_add_owned(poly, i, negated)) {
                symbolic_poly2_clear(&right_poly);
                goto cleanup;
            }
        }
        symbolic_poly2_clear(&right_poly);
        ok = true;
        goto cleanup;
    }

    if (expr_is_op(expr, &ops_mul) && expr->a && expr->b) {
        ok = symbolic_poly2_collect_mul(expr->a, expr->b, wrt, poly);
        goto cleanup;
    }

    if (expr_is_pow_d_expr(expr) && is_wrt(expr->a, wrt) &&
        num_eq(expr->c, NUM_TWO)) {
        ok = symbolic_poly2_add_const(poly, 2u, 1L);
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
        expr->a && expr->b &&
        is_wrt(expr->a, wrt) &&
        expr_match_const_value(expr->b, &exponent) &&
        num_eq(exponent, NUM_TWO)) {
        ok = symbolic_poly2_add_const(poly, 2u, 1L);
        goto cleanup;
    }

cleanup:
    num_destroy(&exponent);
    return ok;
}

static expr_t *symbolic_poly2_coeff_or_zero(const symbolic_poly2_t *poly, size_t degree)
{
    if (!poly || degree >= 3u || !poly->coeff[degree])
        return expr_const_long_local(0L);
    return retain_expr_local_quadratic(poly->coeff[degree]);
}

static bool symbolic_poly2_all_coeffs_numeric(const symbolic_poly2_t *poly)
{
    number_t value = num_new();
    bool all_numeric = true;

    if (!poly) {
        num_destroy(&value);
        return true;
    }

    for (size_t i = 0; i < 3u; ++i) {
        if (!poly->coeff[i])
            continue;
        if (!expr_match_const_value(poly->coeff[i], &value)) {
            all_numeric = false;
            break;
        }
    }

    num_destroy(&value);
    return all_numeric;
}

static bool symbolic_poly2_write_numeric_deg4(const symbolic_poly2_t *source,
                                              number_t *target)
{
    number_t value = num_new();
    bool ok = false;

    if (!source || !target)
        goto cleanup;

    for (size_t i = 0; i < 3u; ++i) {
        if (!source->coeff[i])
            continue;
        if (!expr_match_const_value(source->coeff[i], &value) ||
            !num_is_real(value))
            goto cleanup;
        num_destroy(&target[i]);
        target[i] = num_clone(value);
    }

    ok = true;

cleanup:
    num_destroy(&value);
    return ok;
}

static expr_t *symbolic_general_quadratic_inverse_integral_numeric(const expr_t *wrt,
                                                                   const expr_t *a,
                                                                   const expr_t *b,
                                                                   const expr_t *c)
{
    number_t av = num_new();
    number_t bv = num_new();
    number_t cv = num_new();
    number_t four = num_create_from_long(4L);
    number_t ac = num_new();
    number_t four_ac = num_new();
    number_t b_sq = num_new();
    number_t delta = num_new();
    number_t sqrt_delta = num_new();
    number_t two_a = num_new();
    number_t arg_scale = num_new();
    number_t arg_offset = num_new();
    number_t out_scale = num_new();
    expr_t *scaled_x = NULL;
    expr_t *arg = NULL;
    expr_t *atan_arg = NULL;
    expr_t *out = NULL;

    if (!wrt ||
        !expr_match_const_value(a, &av) ||
        !expr_match_const_value(b, &bv) ||
        !expr_match_const_value(c, &cv))
        goto cleanup;

    num_destroy(&ac);
    ac = num_scope_detach(num_mul(av, cv));
    num_destroy(&four_ac);
    four_ac = num_scope_detach(num_mul(four, ac));
    num_destroy(&b_sq);
    b_sq = num_scope_detach(num_mul(bv, bv));
    num_destroy(&delta);
    delta = num_scope_detach(num_sub(four_ac, b_sq));
    if (!num_is_finite(delta) || num_eq(delta, NUM_ZERO))
        goto cleanup;

    num_destroy(&sqrt_delta);
    sqrt_delta = num_scope_detach(num_sqrt(delta));
    if (!num_is_finite(sqrt_delta) || num_eq(sqrt_delta, NUM_ZERO))
        goto cleanup;

    num_destroy(&two_a);
    two_a = num_scope_detach(num_mul(NUM_TWO, av));
    num_destroy(&arg_scale);
    arg_scale = num_scope_detach(num_div(two_a, sqrt_delta));
    num_destroy(&arg_offset);
    arg_offset = num_scope_detach(num_div(bv, sqrt_delta));
    num_destroy(&out_scale);
    out_scale = num_scope_detach(num_div(NUM_TWO, sqrt_delta));

    scaled_x = expr_mul_num(wrt, &arg_scale);
    arg = scaled_x ? expr_add_num(scaled_x, &arg_offset) : NULL;
    atan_arg = arg ? expr_atan(arg) : NULL;
    out = atan_arg ? expr_mul_num(atan_arg, &out_scale) : NULL;

cleanup:
    expr_free(atan_arg);
    expr_free(arg);
    expr_free(scaled_x);
    num_destroy(&out_scale);
    num_destroy(&arg_offset);
    num_destroy(&arg_scale);
    num_destroy(&two_a);
    num_destroy(&sqrt_delta);
    num_destroy(&delta);
    num_destroy(&b_sq);
    num_destroy(&four_ac);
    num_destroy(&ac);
    num_destroy(&four);
    num_destroy(&cv);
    num_destroy(&bv);
    num_destroy(&av);
    return out;
}

static expr_t *symbolic_general_quadratic_inverse_integral(const expr_t *wrt,
                                                           const expr_t *a,
                                                           const expr_t *b,
                                                           const expr_t *c)
{
    expr_t *numeric = symbolic_general_quadratic_inverse_integral_numeric(wrt, a, b, c);
    expr_t *four = expr_const_long_local(4L);
    expr_t *two = expr_const_long_local(2L);
    expr_t *ac = (a && c) ? expr_mul(a, c) : NULL;
    expr_t *four_ac = (four && ac) ? expr_mul(four, ac) : NULL;
    expr_t *b_sq = b ? expr_mul(b, b) : NULL;
    expr_t *delta_raw = (four_ac && b_sq) ? expr_sub(four_ac, b_sq) : NULL;
    expr_t *delta = simplify_owned(delta_raw);
    expr_t *sqrt_delta = delta ? expr_sqrt(delta) : NULL;
    expr_t *two_a = (two && a) ? expr_mul(two, a) : NULL;
    expr_t *two_ax = (two_a && wrt) ? expr_mul(two_a, wrt) : NULL;
    expr_t *arg_num = (two_ax && b) ? expr_add(two_ax, b) : NULL;
    expr_t *arg = (arg_num && sqrt_delta) ? expr_div(arg_num, sqrt_delta) : NULL;
    expr_t *atan_arg = arg ? expr_atan(arg) : NULL;
    expr_t *scaled_atan = two ? expr_mul(two, atan_arg) : NULL;
    expr_t *out = (scaled_atan && sqrt_delta) ? expr_div(scaled_atan, sqrt_delta) : NULL;

    if (numeric) {
        expr_free(out);
        out = numeric;
        numeric = NULL;
    }

    expr_free(scaled_atan);
    expr_free(atan_arg);
    expr_free(arg);
    expr_free(arg_num);
    expr_free(two_ax);
    expr_free(two_a);
    expr_free(sqrt_delta);
    expr_free(delta);
    expr_free(b_sq);
    expr_free(four_ac);
    expr_free(ac);
    expr_free(two);
    expr_free(four);
    expr_free(numeric);
    return out;
}

static expr_t *add_symbolic_parts_owned(expr_t *left, expr_t *right)
{
    expr_t *sum;

    if (!left)
        return right;
    if (!right)
        return left;

    sum = expr_add(left, right);
    expr_free(right);
    expr_free(left);
    return sum;
}

static bool match_negative_quadratic_exponent(const expr_t *exponent,
                                              const expr_t *wrt,
                                              number_t *poly,
                                              number_t *basis_constant,
                                              number_t *basis_coeff)
{
    expr_t *vars[1];
    symbolic_poly2_t direct_poly = { 0 };
    bool ok = false;

    if (!exponent || !wrt || !poly || !basis_constant || !basis_coeff)
        return false;

    vars[0] = (expr_t *)wrt;
    if (expr_match_affine_poly_deg4(exponent, 1u, vars, poly,
                                    basis_constant, basis_coeff))
        return true;

    if (!symbolic_poly2_collect(exponent, wrt, &direct_poly) ||
        !symbolic_poly2_write_numeric_deg4(&direct_poly, poly))
        goto cleanup;

    num_destroy(basis_constant);
    *basis_constant = num_clone(NUM_ZERO);
    num_destroy(basis_coeff);
    *basis_coeff = num_clone(NUM_ONE);
    ok = true;

cleanup:
    symbolic_poly2_clear(&direct_poly);
    return ok;
}

expr_t *integrate_exp_of_negative_quadratic(const expr_t *expr, const expr_t *wrt)
{
    number_t poly[5];
    number_t basis_constant = num_new();
    number_t basis_coeff = num_new();
    number_t neg_a = num_new();
    number_t root = num_new();
    number_t two_a = num_new();
    number_t shift = num_new();
    number_t b_sq = num_new();
    number_t four = num_create_from_long(4L);
    number_t four_a = num_new();
    number_t correction = num_new();
    number_t offset = num_new();
    number_t two_root = num_new();
    number_t denom = num_new();
    expr_t *u = NULL;
    expr_t *shifted = NULL;
    expr_t *root_const = NULL;
    expr_t *root_expr = NULL;
    expr_t *arg = NULL;
    expr_t *erf_arg = NULL;
    expr_t *offset_const = NULL;
    expr_t *exp_offset = NULL;
    expr_t *pi_const = NULL;
    expr_t *sqrt_pi = NULL;
    expr_t *scale_factor = NULL;
    expr_t *numer = NULL;
    expr_t *two_const = NULL;
    expr_t *basis_coeff_const = NULL;
    expr_t *two_root_expr = NULL;
    expr_t *denom_expr = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;

    number_array_zero_local(poly, 5u);
    if (!expr || !wrt || !expr->a || !expr_is_op(expr, &ops_exp) ||
        !match_negative_quadratic_exponent(expr->a, wrt, poly,
                                           &basis_constant, &basis_coeff) ||
        !num_is_real(poly[0]) ||
        !num_is_real(poly[1]) ||
        !num_is_real(poly[2]) ||
        !num_is_zero(poly[3]) ||
        !num_is_zero(poly[4]) ||
        !num_lt(poly[2], NUM_ZERO) ||
        num_is_zero(basis_coeff))
        goto cleanup;

    num_destroy(&neg_a);
    neg_a = num_scope_detach(num_neg(poly[2]));
    num_destroy(&root);
    root = num_scope_detach(num_sqrt(neg_a));
    if (!num_is_finite(root) || num_is_zero(root))
        goto cleanup;

    num_destroy(&two_a);
    two_a = num_scope_detach(num_mul(NUM_TWO, poly[2]));
    num_destroy(&shift);
    shift = num_scope_detach(num_div(poly[1], two_a));

    num_destroy(&b_sq);
    b_sq = num_scope_detach(num_mul(poly[1], poly[1]));
    num_destroy(&four_a);
    four_a = num_scope_detach(num_mul(four, poly[2]));
    num_destroy(&correction);
    correction = num_scope_detach(num_div(b_sq, four_a));
    num_destroy(&offset);
    offset = num_scope_detach(num_sub(poly[0], correction));

    num_destroy(&two_root);
    two_root = num_scope_detach(num_mul(NUM_TWO, root));
    num_destroy(&denom);
    denom = num_scope_detach(num_mul(two_root, basis_coeff));
    if (!num_is_finite(denom) || num_is_zero(denom))
        goto cleanup;

    u = build_affine_from_match(wrt, basis_constant, basis_coeff);
    shifted = u ? expr_add_num(u, &shift) : NULL;
    root_const = expr_new_const(neg_a);
    root_expr = root_const ? expr_sqrt(root_const) : NULL;
    arg = (shifted && root_expr) ? expr_mul(root_expr, shifted) : NULL;
    erf_arg = arg ? expr_erf(arg) : NULL;
    offset_const = expr_new_const(offset);
    exp_offset = offset_const ? expr_exp(offset_const) : NULL;
    pi_const = expr_new_named_const(NUM_PI, "@pi");
    sqrt_pi = pi_const ? expr_sqrt(pi_const) : NULL;
    scale_factor = (exp_offset && sqrt_pi) ? expr_mul(exp_offset, sqrt_pi) : NULL;
    numer = (scale_factor && erf_arg) ? expr_mul(scale_factor, erf_arg) : NULL;
    two_const = expr_const_long_local(2L);
    basis_coeff_const = expr_new_const(basis_coeff);
    two_root_expr = (two_const && root_expr) ? expr_mul(two_const, root_expr) : NULL;
    denom_expr = (two_root_expr && basis_coeff_const)
        ? expr_mul(two_root_expr, basis_coeff_const)
        : NULL;
    quotient = (numer && denom_expr) ? expr_div(numer, denom_expr) : NULL;
    out = simplify_owned(quotient);
    quotient = NULL;

cleanup:
    expr_free(quotient);
    expr_free(denom_expr);
    expr_free(two_root_expr);
    expr_free(basis_coeff_const);
    expr_free(two_const);
    expr_free(numer);
    expr_free(scale_factor);
    expr_free(sqrt_pi);
    expr_free(pi_const);
    expr_free(exp_offset);
    expr_free(offset_const);
    expr_free(erf_arg);
    expr_free(arg);
    expr_free(root_expr);
    expr_free(root_const);
    expr_free(shifted);
    expr_free(u);
    num_destroy(&denom);
    num_destroy(&two_root);
    num_destroy(&offset);
    num_destroy(&correction);
    num_destroy(&four_a);
    num_destroy(&four);
    num_destroy(&b_sq);
    num_destroy(&shift);
    num_destroy(&two_a);
    num_destroy(&root);
    num_destroy(&neg_a);
    num_destroy(&basis_coeff);
    num_destroy(&basis_constant);
    number_array_clear_local(poly, 5u);
    return out;
}

expr_t *integrate_linear_over_symbolic_quadratic(const expr_t *expr, const expr_t *wrt)
{
    symbolic_poly2_t numer = { 0 };
    symbolic_poly2_t denom = { 0 };
    expr_t *a = NULL;
    expr_t *b = NULL;
    expr_t *c = NULL;
    expr_t *m = NULL;
    expr_t *n = NULL;
    expr_t *two = NULL;
    expr_t *two_a = NULL;
    expr_t *alpha = NULL;
    expr_t *alpha_for_log = NULL;
    expr_t *log_denom = NULL;
    expr_t *log_part = NULL;
    expr_t *alpha_b = NULL;
    expr_t *remainder = NULL;
    expr_t *inverse_integral = NULL;
    expr_t *remainder_part = NULL;
    expr_t *sum = NULL;

    if (!expr || !expr->a || !expr->b)
        return NULL;

    if (!symbolic_poly2_collect(expr->a, wrt, &numer) ||
        !symbolic_poly2_collect(expr->b, wrt, &denom) ||
        numer.coeff[2] ||
        !denom.coeff[2] ||
        (symbolic_poly2_all_coeffs_numeric(&numer) &&
         symbolic_poly2_all_coeffs_numeric(&denom)))
        goto cleanup;

    a = symbolic_poly2_coeff_or_zero(&denom, 2u);
    b = symbolic_poly2_coeff_or_zero(&denom, 1u);
    c = symbolic_poly2_coeff_or_zero(&denom, 0u);
    m = symbolic_poly2_coeff_or_zero(&numer, 1u);
    n = symbolic_poly2_coeff_or_zero(&numer, 0u);
    two = expr_const_long_local(2L);
    two_a = (two && a) ? expr_mul(two, a) : NULL;
    alpha = (m && two_a) ? expr_div(m, two_a) : NULL;
    alpha = simplify_owned(alpha);
    alpha_for_log = retain_expr_local_quadratic(alpha);
    log_denom = expr_log(expr->b);
    log_part = (alpha_for_log && log_denom) ? expr_mul(alpha_for_log, log_denom) : NULL;
    alpha_b = (alpha && b) ? expr_mul(alpha, b) : NULL;
    remainder = (n && alpha_b) ? expr_sub(n, alpha_b) : NULL;
    remainder = simplify_owned(remainder);
    inverse_integral = symbolic_general_quadratic_inverse_integral(wrt, a, b, c);
    remainder_part = (remainder && inverse_integral)
                         ? expr_mul(remainder, inverse_integral)
                         : NULL;
    sum = add_symbolic_parts_owned(log_part, remainder_part);
    log_part = NULL;
    remainder_part = NULL;

cleanup:
    expr_free(remainder_part);
    expr_free(inverse_integral);
    expr_free(remainder);
    expr_free(alpha_b);
    expr_free(log_part);
    expr_free(log_denom);
    expr_free(alpha_for_log);
    expr_free(alpha);
    expr_free(two_a);
    expr_free(two);
    expr_free(n);
    expr_free(m);
    expr_free(c);
    expr_free(b);
    expr_free(a);
    symbolic_poly2_clear(&denom);
    symbolic_poly2_clear(&numer);
    return sum;
}

expr_t *integrate_log_of_symbolic_quadratic(const expr_t *expr, const expr_t *wrt)
{
    symbolic_poly2_t poly = { 0 };
    expr_t *a = NULL;
    expr_t *b = NULL;
    expr_t *c = NULL;
    expr_t *log_q = NULL;
    expr_t *x_log_q = NULL;
    expr_t *two_x = NULL;
    expr_t *base = NULL;
    expr_t *two = NULL;
    expr_t *two_a = NULL;
    expr_t *alpha = NULL;
    expr_t *alpha_for_log = NULL;
    expr_t *log_q_tail = NULL;
    expr_t *log_part = NULL;
    expr_t *alpha_b = NULL;
    expr_t *two_c = NULL;
    expr_t *remainder = NULL;
    expr_t *inverse_integral = NULL;
    expr_t *remainder_part = NULL;
    expr_t *rational_part = NULL;
    expr_t *sum = NULL;

    if (!expr || !wrt || !expr->a ||
        !expr_is_op(expr, &ops_log) ||
        !symbolic_poly2_collect(expr->a, wrt, &poly) ||
        !poly.coeff[2] ||
        expr_is_exact_zero(poly.coeff[2])) {
        symbolic_poly2_clear(&poly);
        return NULL;
    }

    a = symbolic_poly2_coeff_or_zero(&poly, 2u);
    b = symbolic_poly2_coeff_or_zero(&poly, 1u);
    c = symbolic_poly2_coeff_or_zero(&poly, 0u);

    log_q = expr_log(expr->a);
    x_log_q = log_q ? expr_mul(wrt, log_q) : NULL;
    two_x = expr_mul_num(wrt, &NUM_TWO);
    base = (x_log_q && two_x) ? expr_sub(x_log_q, two_x) : NULL;

    two = expr_const_long_local(2L);
    two_a = (two && a) ? expr_mul(two, a) : NULL;
    alpha = (b && two_a) ? expr_div(b, two_a) : NULL;
    alpha = simplify_owned(alpha);
    alpha_for_log = retain_expr_local_quadratic(alpha);
    log_q_tail = expr_log(expr->a);
    log_part = (alpha_for_log && log_q_tail) ? expr_mul(alpha_for_log, log_q_tail) : NULL;
    alpha_b = (alpha && b) ? expr_mul(alpha, b) : NULL;
    two_c = c ? expr_mul_num(c, &NUM_TWO) : NULL;
    remainder = (two_c && alpha_b) ? expr_sub(two_c, alpha_b) : NULL;
    remainder = simplify_owned(remainder);
    inverse_integral = symbolic_general_quadratic_inverse_integral(wrt, a, b, c);
    remainder_part = (remainder && inverse_integral)
                         ? expr_mul(remainder, inverse_integral)
                         : NULL;
    rational_part = add_symbolic_parts_owned(log_part, remainder_part);
    log_part = NULL;
    remainder_part = NULL;

    if (base && rational_part) {
        sum = add_symbolic_parts_owned(base, rational_part);
        base = NULL;
        rational_part = NULL;
    }

    expr_free(rational_part);
    expr_free(remainder_part);
    expr_free(inverse_integral);
    expr_free(remainder);
    expr_free(two_c);
    expr_free(alpha_b);
    expr_free(log_part);
    expr_free(log_q_tail);
    expr_free(alpha_for_log);
    expr_free(alpha);
    expr_free(two_a);
    expr_free(two);
    expr_free(base);
    expr_free(two_x);
    expr_free(x_log_q);
    expr_free(log_q);
    expr_free(c);
    expr_free(b);
    expr_free(a);
    symbolic_poly2_clear(&poly);
    return sum;
}
