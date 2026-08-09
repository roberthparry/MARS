#include <stdbool.h>

#define MARS_EXPR_INTEGRATE_INTERNAL_ACCESS
#include "expr_integrate_internal.h"

expr_t *expr_integrate_build_unsigned_expr_power(const expr_t *base, unsigned int exponent)
{
    number_t n;
    expr_t *out;

    if (!base)
        return NULL;
    if (exponent == 0u)
        return expr_new_const(NUM_ONE);
    if (exponent == 1u) {
        expr_retain(base);
        return (expr_t *)base;
    }

    n = num_create_from_long((long)exponent);
    out = expr_pow(base, &n);
    num_destroy(&n);
    return out;
}

bool expr_integrate_number_matches_uint_at_most(number_t value, unsigned int max_value, unsigned int *out)
{
    for (unsigned int i = 0u; i <= max_value; ++i) {
        number_t candidate = num_create_from_long((long)i);
        bool ok = num_eq(value, candidate);

        num_destroy(&candidate);
        if (ok) {
            if (out)
                *out = i;
            return true;
        }
    }

    return false;
}

bool match_exp_proportional_wrt_coeff(const expr_t *expr, const expr_t *wrt, expr_t **coeff_out)
{
    expr_t *constant = NULL;
    expr_t *coeff = NULL;
    bool ok = false;

    if (!expr || !wrt || !coeff_out || !expr->ops || expr->ops->kind != EXPR_KIND_EXP || !expr->a)
        return false;

    if (!match_symbolic_affine_constant_and_coeff(expr->a, wrt, &constant, &coeff))
        goto cleanup;
    if (!expr_const_is_zero(constant) || expr_const_is_zero(coeff))
        goto cleanup;

    *coeff_out = coeff;
    coeff = NULL;
    ok = true;

cleanup:
    expr_free(coeff);
    expr_free(constant);
    return ok;
}

bool expr_integrate_match_wrt_power_factor_exponent(const expr_t *expr, const expr_t *wrt, expr_t **exponent_out)
{
    if (!expr || !wrt || !exponent_out)
        return false;

    if (is_wrt(expr, wrt)) {
        *exponent_out = expr_new_const(NUM_ONE);
        return *exponent_out != NULL;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_SQRT && expr->a && is_wrt(expr->a, wrt)) {
        *exponent_out = expr_new_const(NUM_HALF);
        return *exponent_out != NULL;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a && is_wrt(expr->a, wrt)) {
        *exponent_out = expr_new_const(expr->c);
        return *exponent_out != NULL;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW && expr->a && expr->b && is_wrt(expr->a, wrt) &&
        !depends_on_wrt(expr->b, wrt)) {
        *exponent_out = expr_clone(expr->b);
        return *exponent_out != NULL;
    }

    return false;
}

static bool match_power_exp_product(const expr_t *expr, const expr_t *wrt, const expr_t **power_out,
                                    const expr_t **exp_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *exponent = NULL;
    expr_t *coeff = NULL;

    if (!expr || !wrt || !power_out || !exp_out || !expr_match_mul_expr(expr, &left, &right))
        return false;

    if (expr_integrate_match_wrt_power_factor_exponent(left, wrt, &exponent) &&
        match_exp_proportional_wrt_coeff(right, wrt, &coeff)) {
        expr_free(coeff);
        expr_free(exponent);
        *power_out = left;
        *exp_out = right;
        return true;
    }
    expr_free(coeff);
    expr_free(exponent);
    coeff = NULL;
    exponent = NULL;

    if (expr_integrate_match_wrt_power_factor_exponent(right, wrt, &exponent) &&
        match_exp_proportional_wrt_coeff(left, wrt, &coeff)) {
        expr_free(coeff);
        expr_free(exponent);
        *power_out = right;
        *exp_out = left;
        return true;
    }

    expr_free(coeff);
    expr_free(exponent);
    return false;
}

enum { polynomial_exp_coeff_count = 5u, polynomial_exp_equation_count = (polynomial_exp_coeff_count - 1u) * 2u };

static void polynomial_exp_matrix_zero(number_t matrix[polynomial_exp_equation_count][polynomial_exp_coeff_count],
                                       number_t *rhs)
{
    for (size_t row = 0u; row < polynomial_exp_equation_count; ++row) {
        for (size_t col = 0u; col < polynomial_exp_coeff_count; ++col)
            matrix[row][col] = num_new();
        rhs[row] = num_new();
    }
}

static void polynomial_exp_matrix_clear(number_t matrix[polynomial_exp_equation_count][polynomial_exp_coeff_count],
                                        number_t *rhs)
{
    for (size_t row = 0u; row < polynomial_exp_equation_count; ++row) {
        for (size_t col = 0u; col < polynomial_exp_coeff_count; ++col)
            num_destroy(&matrix[row][col]);
        num_destroy(&rhs[row]);
    }
}

static void polynomial_exp_swap_numbers(number_t *left, number_t *right)
{
    number_t tmp = *left;

    *left = *right;
    *right = tmp;
}

static void polynomial_exp_matrix_swap_rows(number_t matrix[polynomial_exp_equation_count][polynomial_exp_coeff_count],
                                            number_t *rhs, size_t left, size_t right)
{
    if (left == right)
        return;

    for (size_t col = 0u; col < polynomial_exp_coeff_count; ++col)
        polynomial_exp_swap_numbers(&matrix[left][col], &matrix[right][col]);
    polynomial_exp_swap_numbers(&rhs[left], &rhs[right]);
}

static void polynomial_exp_matrix_add_to(number_t *target, number_t value)
{
    number_t next = num_add(*target, value);

    num_destroy(target);
    *target = next;
}

static bool
polynomial_exp_solve_linear_system(number_t matrix[polynomial_exp_equation_count][polynomial_exp_coeff_count],
                                   number_t *rhs, number_t *solution)
{
    size_t pivot_rows[polynomial_exp_coeff_count];
    bool has_pivot[polynomial_exp_coeff_count];
    size_t pivot_row = 0u;

    for (size_t col = 0u; col < polynomial_exp_coeff_count; ++col) {
        pivot_rows[col] = 0u;
        has_pivot[col] = false;
    }

    for (size_t col = 0u; col < polynomial_exp_coeff_count && pivot_row < polynomial_exp_equation_count; ++col) {
        size_t selected = polynomial_exp_equation_count;

        for (size_t row = pivot_row; row < polynomial_exp_equation_count; ++row) {
            if (!num_is_zero(matrix[row][col])) {
                selected = row;
                break;
            }
        }
        if (selected == polynomial_exp_equation_count)
            continue;

        polynomial_exp_matrix_swap_rows(matrix, rhs, pivot_row, selected);

        {
            number_t pivot = num_clone(matrix[pivot_row][col]);

            for (size_t c = col; c < polynomial_exp_coeff_count; ++c) {
                number_t normalized = num_div(matrix[pivot_row][c], pivot);

                num_destroy(&matrix[pivot_row][c]);
                matrix[pivot_row][c] = normalized;
            }
            {
                number_t normalized_rhs = num_div(rhs[pivot_row], pivot);

                num_destroy(&rhs[pivot_row]);
                rhs[pivot_row] = normalized_rhs;
            }
            num_destroy(&pivot);
        }

        for (size_t row = 0u; row < polynomial_exp_equation_count; ++row) {
            if (row == pivot_row || num_is_zero(matrix[row][col]))
                continue;

            number_t factor = num_clone(matrix[row][col]);

            for (size_t c = col; c < polynomial_exp_coeff_count; ++c) {
                number_t scaled = num_mul(factor, matrix[pivot_row][c]);
                number_t reduced = num_sub(matrix[row][c], scaled);

                num_destroy(&matrix[row][c]);
                matrix[row][c] = reduced;
                num_destroy(&scaled);
            }
            {
                number_t scaled_rhs = num_mul(factor, rhs[pivot_row]);
                number_t reduced_rhs = num_sub(rhs[row], scaled_rhs);

                num_destroy(&rhs[row]);
                rhs[row] = reduced_rhs;
                num_destroy(&scaled_rhs);
            }
            num_destroy(&factor);
        }

        has_pivot[col] = true;
        pivot_rows[col] = pivot_row;
        ++pivot_row;
    }

    for (size_t row = 0u; row < polynomial_exp_equation_count; ++row) {
        bool all_zero = true;

        for (size_t col = 0u; col < polynomial_exp_coeff_count; ++col) {
            if (!num_is_zero(matrix[row][col])) {
                all_zero = false;
                break;
            }
        }
        if (all_zero && !num_is_zero(rhs[row]))
            return false;
    }

    for (size_t col = 0u; col < polynomial_exp_coeff_count; ++col) {
        num_destroy(&solution[col]);
        solution[col] = has_pivot[col] ? num_clone(rhs[pivot_rows[col]]) : num_clone(NUM_ZERO);
    }

    return true;
}

static bool polynomial_exp_solve_antiderivative_coeffs(const number_t *integrand, const number_t *exponent,
                                                       number_t *anti)
{
    number_t matrix[polynomial_exp_equation_count][polynomial_exp_coeff_count];
    number_t rhs[polynomial_exp_equation_count];
    bool ok = false;

    polynomial_exp_matrix_zero(matrix, rhs);
    for (size_t row = 0u; row < polynomial_exp_equation_count; ++row) {
        if (row < polynomial_exp_coeff_count) {
            num_destroy(&rhs[row]);
            rhs[row] = num_clone(integrand[row]);
        }

        if (row + 1u < polynomial_exp_coeff_count) {
            number_t derivative_scale = num_create_from_long((long)(row + 1u));

            polynomial_exp_matrix_add_to(&matrix[row][row + 1u], derivative_scale);
            num_destroy(&derivative_scale);
        }

        for (size_t exponent_degree = 1u; exponent_degree < polynomial_exp_coeff_count; ++exponent_degree) {
            size_t derivative_degree = exponent_degree - 1u;

            if (row < derivative_degree || row - derivative_degree >= polynomial_exp_coeff_count ||
                num_is_zero(exponent[exponent_degree])) {
                continue;
            }

            size_t anti_degree = row - derivative_degree;
            number_t degree_scale = num_create_from_long((long)exponent_degree);
            number_t derivative_coeff = num_mul(exponent[exponent_degree], degree_scale);

            polynomial_exp_matrix_add_to(&matrix[row][anti_degree], derivative_coeff);
            num_destroy(&derivative_coeff);
            num_destroy(&degree_scale);
        }
    }

    ok = polynomial_exp_solve_linear_system(matrix, rhs, anti);

    polynomial_exp_matrix_clear(matrix, rhs);
    return ok;
}

static expr_t *polynomial_exp_combine_factors(expr_t *left, expr_t *right)
{
    expr_t *combined = NULL;

    if (left && right) {
        combined = expr_mul(left, right);
    } else if (left) {
        combined = left;
        left = NULL;
    } else if (right) {
        combined = right;
        right = NULL;
    }

    expr_free(right);
    expr_free(left);
    return simplify_owned(combined);
}

static expr_t *polynomial_exp_extract_factor(const expr_t *expr, const expr_t **exp_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *left_remainder = NULL;
    expr_t *right_remainder = NULL;
    expr_t *retained = NULL;

    if (!expr || !exp_out)
        return NULL;

    if (expr->ops && expr->ops->kind == EXPR_KIND_EXP) {
        *exp_out = expr;
        return expr_const_one();
    }

    if (!expr_match_mul_expr(expr, &left, &right))
        return NULL;

    left_remainder = polynomial_exp_extract_factor(left, exp_out);
    if (left_remainder) {
        retained = expr_retain_expr(right);
        return polynomial_exp_combine_factors(left_remainder, retained);
    }

    right_remainder = polynomial_exp_extract_factor(right, exp_out);
    if (right_remainder) {
        retained = expr_retain_expr(left);
        return polynomial_exp_combine_factors(retained, right_remainder);
    }

    return NULL;
}

expr_t *integrate_polynomial_times_polynomial_exp(const expr_t *expr, const expr_t *wrt)
{
    expr_t *poly_expr = NULL;
    const expr_t *exp_expr = NULL;
    expr_t *vars[1];
    number_t poly[polynomial_exp_coeff_count];
    number_t exponent[polynomial_exp_coeff_count];
    number_t anti[polynomial_exp_coeff_count];
    number_t poly_constant = num_new();
    number_t exponent_constant = num_new();
    number_t poly_coeffs[1];
    number_t exponent_coeffs[1];
    expr_t *basis = NULL;
    expr_t *anti_poly = NULL;
    expr_t *exp_clone = NULL;
    expr_t *out = NULL;

    number_array_zero_local(poly, polynomial_exp_coeff_count);
    number_array_zero_local(exponent, polynomial_exp_coeff_count);
    number_array_zero_local(anti, polynomial_exp_coeff_count);
    poly_coeffs[0] = num_new();
    exponent_coeffs[0] = num_new();

    poly_expr = polynomial_exp_extract_factor(expr, &exp_expr);
    if (!poly_expr || !exp_expr || !exp_expr->a)
        goto cleanup;

    vars[0] = (expr_t *)wrt;
    if (!expr_match_affine_poly_deg4(exp_expr->a, 1u, vars, exponent, &exponent_constant, exponent_coeffs) ||
        num_is_zero(exponent_coeffs[0]) ||
        !expr_match_affine_poly_deg4(poly_expr, 1u, vars, poly, &poly_constant, poly_coeffs) ||
        !expr_integrate_rewrite_poly_deg4_to_affine_basis(poly, poly_constant, poly_coeffs[0], exponent_constant,
                                                          exponent_coeffs[0])) {
        goto cleanup;
    }

    for (size_t i = 0u; i < polynomial_exp_coeff_count; ++i) {
        number_t scaled = num_div(poly[i], exponent_coeffs[0]);

        num_destroy(&poly[i]);
        poly[i] = scaled;
    }

    if (!polynomial_exp_solve_antiderivative_coeffs(poly, exponent, anti))
        goto cleanup;

    basis = build_affine_from_match(wrt, exponent_constant, exponent_coeffs[0]);
    anti_poly = basis ? build_polynomial_expr(basis, anti, polynomial_exp_coeff_count) : NULL;
    expr_retain(exp_expr);
    exp_clone = (expr_t *)exp_expr;
    out = (anti_poly && exp_clone) ? expr_mul(anti_poly, exp_clone) : NULL;
    out = simplify_owned(out);

cleanup:
    expr_free(exp_clone);
    expr_free(anti_poly);
    expr_free(basis);
    expr_free(poly_expr);
    num_destroy(&exponent_coeffs[0]);
    num_destroy(&poly_coeffs[0]);
    num_destroy(&exponent_constant);
    num_destroy(&poly_constant);
    number_array_clear_local(anti, polynomial_exp_coeff_count);
    number_array_clear_local(exponent, polynomial_exp_coeff_count);
    number_array_clear_local(poly, polynomial_exp_coeff_count);
    return out;
}

static expr_t *build_symbolic_integer_power_exp_integral(unsigned int degree, const expr_t *exp_expr,
                                                         const expr_t *coeff, const expr_t *wrt)
{
    expr_t *sum = NULL;
    long falling = 1L;

    if (!exp_expr || !coeff || !wrt)
        return NULL;

    for (unsigned int k = 0u; k <= degree; ++k) {
        long signed_falling;
        expr_t *x_power = NULL;
        expr_t *coeff_power = NULL;
        expr_t *term = NULL;
        expr_t *exp_clone = NULL;
        expr_t *term_product = NULL;

        if (k > 0u)
            falling *= (long)(degree - k + 1u);
        signed_falling = (k % 2u) ? -falling : falling;

        if (degree == k && signed_falling != 1L) {
            number_t scale = num_create_from_long(signed_falling);

            x_power = expr_new_const(scale);
            num_destroy(&scale);
        } else {
            x_power = expr_integrate_build_unsigned_expr_power(wrt, degree - k);
        }
        if (x_power && degree != k && signed_falling != 1L) {
            number_t scale = num_create_from_long(signed_falling);

            x_power = mul_number_owned(x_power, scale);
            num_destroy(&scale);
        }
        {
            number_t scale = num_new();
            const expr_t *base = NULL;

            if (expr_match_scaled_expr(coeff, &scale, &base) && base && base != coeff) {
                number_t scale_power = num_pow_int(scale, (int)(k + 1u));
                expr_t *scale_expr = expr_new_const(scale_power);
                expr_t *base_power = expr_integrate_build_unsigned_expr_power(base, k + 1u);

                coeff_power = scale_expr && base_power ? expr_mul(scale_expr, base_power) : NULL;
                coeff_power = simplify_owned(coeff_power);
                expr_free(base_power);
                expr_free(scale_expr);
                num_destroy(&scale_power);
            } else {
                coeff_power = expr_integrate_build_unsigned_expr_power(coeff, k + 1u);
            }
            num_destroy(&scale);
        }
        term = (x_power && coeff_power) ? expr_div(x_power, coeff_power) : NULL;
        term = simplify_owned(term);
        expr_retain(exp_expr);
        exp_clone = (expr_t *)exp_expr;
        term_product = (term && exp_clone) ? expr_mul(term, exp_clone) : NULL;
        term_product = simplify_owned(term_product);

        if (term_product) {
            sum = sum ? expr_add_owned(sum, term_product) : term_product;
            term_product = NULL;
        }

        expr_free(term_product);
        expr_free(exp_clone);
        expr_free(term);
        expr_free(coeff_power);
        expr_free(x_power);
    }

    return simplify_owned(sum);
}

expr_t *integrate_symbolic_integer_power_times_exp(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *power_expr = NULL;
    const expr_t *exp_expr = NULL;
    expr_t *exponent_expr = NULL;
    expr_t *coeff = NULL;
    number_t exponent = num_new();
    expr_t *out = NULL;
    unsigned int degree = 0u;

    if (!match_power_exp_product(expr, wrt, &power_expr, &exp_expr))
        goto cleanup;
    if (!expr_integrate_match_wrt_power_factor_exponent(power_expr, wrt, &exponent_expr) ||
        !expr_match_const_value(exponent_expr, &exponent) ||
        !expr_integrate_number_matches_uint_at_most(exponent, 4u, &degree) ||
        !match_exp_proportional_wrt_coeff(exp_expr, wrt, &coeff))
        goto cleanup;

    out = build_symbolic_integer_power_exp_integral(degree, exp_expr, coeff, wrt);

cleanup:
    num_destroy(&exponent);
    expr_free(coeff);
    expr_free(exponent_expr);
    return out;
}

expr_t *integrate_symbolic_power_times_exp_gamma(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *power_expr = NULL;
    const expr_t *exp_expr = NULL;
    expr_t *exponent = NULL;
    expr_t *coeff = NULL;
    expr_t *next_exponent = NULL;
    expr_t *neg_coeff = NULL;
    expr_t *arg = NULL;
    expr_t *gamma = NULL;
    expr_t *log_neg_coeff = NULL;
    expr_t *denom_exponent = NULL;
    expr_t *denominator = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;
    number_t exponent_value = num_new();

    if (!match_power_exp_product(expr, wrt, &power_expr, &exp_expr) ||
        !expr_integrate_match_wrt_power_factor_exponent(power_expr, wrt, &exponent) ||
        !expr_match_const_value(exponent, &exponent_value) || !match_exp_proportional_wrt_coeff(exp_expr, wrt, &coeff))
        goto cleanup;

    next_exponent = expr_add_num(exponent, &NUM_ONE);
    neg_coeff = coeff ? expr_neg(coeff) : NULL;
    arg = (neg_coeff && wrt) ? expr_mul(neg_coeff, wrt) : NULL;
    gamma = (next_exponent && arg) ? expr_gammainc_lower(next_exponent, arg) : NULL;
    log_neg_coeff = neg_coeff ? expr_log(neg_coeff) : NULL;
    denom_exponent = (next_exponent && log_neg_coeff) ? expr_mul(next_exponent, log_neg_coeff) : NULL;
    denominator = denom_exponent ? expr_exp(denom_exponent) : NULL;
    quotient = (gamma && denominator) ? expr_div(gamma, denominator) : NULL;
    out = simplify_owned(quotient);
    quotient = NULL;

cleanup:
    expr_free(quotient);
    expr_free(denominator);
    expr_free(denom_exponent);
    expr_free(log_neg_coeff);
    expr_free(gamma);
    expr_free(arg);
    expr_free(neg_coeff);
    expr_free(next_exponent);
    expr_free(coeff);
    expr_free(exponent);
    num_destroy(&exponent_value);
    return out;
}
