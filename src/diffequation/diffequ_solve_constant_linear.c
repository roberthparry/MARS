#include <stdio.h>
#include <stdlib.h>

#include "matrix.h"

#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation/equation_internal.h"
#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"

typedef struct {
    expr_t **coefficients;
    expr_t *forcing;
    size_t order;
} de_constant_linear_form_t;

typedef enum {
    DE_BASIS_GROUP_NONE = 0,
    DE_BASIS_GROUP_REPEATED_REAL,
    DE_BASIS_GROUP_REPEATED_OSCILLATORY
} de_basis_group_t;

typedef struct {
    expr_t **items;
    size_t count;
    size_t capacity;
    de_basis_group_t group;
    size_t multiplicity;
} de_basis_t;

static void de_constant_linear_form_clear(
    de_constant_linear_form_t *form)
{
    if (!form)
        return;
    if (form->coefficients) {
        for (size_t i = 0u; i <= form->order; ++i)
            expr_free(form->coefficients[i]);
    }
    free(form->coefficients);
    expr_free(form->forcing);
    form->coefficients = NULL;
    form->forcing = NULL;
    form->order = 0u;
}

static void de_basis_clear(de_basis_t *basis)
{
    if (!basis)
        return;
    for (size_t i = 0u; i < basis->count; ++i)
        expr_free(basis->items[i]);
    free(basis->items);
    basis->items = NULL;
    basis->count = 0u;
    basis->capacity = 0u;
    basis->group = DE_BASIS_GROUP_NONE;
    basis->multiplicity = 0u;
}

static int de_basis_append(de_basis_t *basis, expr_t *item)
{
    expr_t **resized;
    size_t capacity;

    if (!basis || !item)
        return -1;
    if (basis->count == basis->capacity) {
        capacity = basis->capacity ? basis->capacity * 2u : 8u;
        resized = realloc(basis->items, capacity * sizeof(*resized));
        if (!resized)
            return -1;
        basis->items = resized;
        basis->capacity = capacity;
    }
    basis->items[basis->count++] = item;
    return 0;
}

static bool de_collect_derivative_nodes(
    const expr_t *expr,
    const expr_t *dependent,
    const expr_t *independent,
    size_t order,
    const expr_t **nodes)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    size_t derivative_order;

    if (!expr)
        return true;
    if (expr_is_formal_derivative(expr)) {
        derivative_order = expr_formal_derivative_order(expr);
        if (derivative_order == 0u ||
            derivative_order > order ||
            !expr_struct_eq(
                expr_formal_derivative_dependent(expr), dependent))
            return false;
        for (size_t i = 0u; i < derivative_order; ++i) {
            if (!expr_struct_eq(
                    expr_formal_derivative_wrt_at(expr, i),
                    independent))
                return false;
        }
        if (nodes[derivative_order] &&
            !expr_struct_eq(nodes[derivative_order], expr))
            return false;
        nodes[derivative_order] = expr;
        return true;
    }
    if (!expr_child_exprs(expr, &left, &right))
        return true;
    return de_collect_derivative_nodes(
               left,
               dependent,
               independent,
               order,
               nodes) &&
           de_collect_derivative_nodes(
               right,
               dependent,
               independent,
               order,
               nodes);
}

static bool de_decompose_constant_linear(
    const expr_t *residual,
    const expr_t *dependent,
    const expr_t *independent,
    size_t order,
    de_constant_linear_form_t *form)
{
    const expr_t **nodes = NULL;
    expr_t *remainder = NULL;
    expr_t *next = NULL;
    expr_t *constant = NULL;
    bool ok = false;

    form->order = order;
    form->coefficients =
        calloc(order + 1u, sizeof(*form->coefficients));
    nodes = calloc(order + 1u, sizeof(*nodes));
    remainder = expr_clone(residual);
    if (!form->coefficients || !nodes || !remainder ||
        !de_collect_derivative_nodes(
            residual,
            dependent,
            independent,
            order,
            nodes))
        goto cleanup;

    for (size_t k = order; k > 0u; --k) {
        if (!nodes[k]) {
            form->coefficients[k] = expr_const_zero();
            if (!form->coefficients[k])
                goto cleanup;
            continue;
        }
        if (!de_linear_decompose(
                remainder,
                nodes[k],
                &form->coefficients[k],
                &next))
            goto cleanup;
        expr_free(remainder);
        remainder = next;
        next = NULL;
    }

    if (!de_linear_decompose(
            remainder,
            dependent,
            &form->coefficients[0],
            &constant))
        goto cleanup;
    form->forcing =
        de_simplify_unary_owned(constant, expr_neg);
    constant = NULL;
    if (!form->forcing ||
        expr_is_exact_zero(form->coefficients[order]))
        goto cleanup;

    for (size_t k = 0u; k <= order; ++k) {
        if (de_expr_uses(form->coefficients[k], independent) ||
            de_expr_uses(form->coefficients[k], dependent))
            goto cleanup;
    }
    if (de_expr_uses(form->forcing, dependent))
        goto cleanup;
    ok = true;

cleanup:
    expr_free(constant);
    expr_free(next);
    expr_free(remainder);
    free(nodes);
    if (!ok)
        de_constant_linear_form_clear(form);
    return ok;
}

static expr_t *de_characteristic_polynomial(
    const de_constant_linear_form_t *form,
    const expr_t *root)
{
    expr_t *sum = expr_const_zero();

    for (size_t k = 0u; sum && k <= form->order; ++k) {
        expr_t *power = k == 0u
            ? expr_const_one()
            : expr_pow_long(root, (long)k);
        expr_t *term = power
            ? expr_mul_simplify_owned(
                  expr_clone(form->coefficients[k]), power)
            : NULL;

        sum = term ? expr_add_simplify_owned(sum, term) : NULL;
    }
    return sum;
}

static bool de_number_near_zero(number_t value)
{
    number_t magnitude = num_abs(value);
    number_t tolerance = num_create_from_string("1e-30");
    bool near = num_is_zero(magnitude) ||
                (num_is_real(magnitude) &&
                 num_lt(magnitude, tolerance));

    num_destroy(&tolerance);
    num_destroy(&magnitude);
    return near;
}

static bool de_expr_near_zero(const expr_t *expr)
{
    number_t value;
    bool near;

    if (expr_is_exact_zero(expr))
        return true;
    value = expr_eval(expr);
    near = num_is_finite(value) && de_number_near_zero(value);
    num_destroy(&value);
    return near;
}

static size_t de_root_multiplicity(
    const expr_t *polynomial,
    const expr_t *root_variable,
    const expr_t *root,
    size_t order)
{
    expr_t *derivative = expr_clone(polynomial);
    size_t multiplicity = 0u;

    while (derivative && multiplicity < order) {
        expr_t *at_root =
            expr_substitute(derivative, root_variable, root);
        expr_t *simplified =
            at_root ? expr_simplify_owned(at_root) : NULL;

        if (!simplified || !de_expr_near_zero(simplified)) {
            expr_free(simplified);
            break;
        }
        expr_free(simplified);
        multiplicity++;
        {
            expr_t *next =
                expr_create_deriv(derivative, root_variable);

            expr_free(derivative);
            derivative = next ? expr_simplify_owned(next) : NULL;
        }
    }
    expr_free(derivative);
    return multiplicity;
}

static bool de_numbers_near(number_t first, number_t second)
{
    number_t difference = num_sub(first, second);
    bool near = de_number_near_zero(difference);

    num_destroy(&difference);
    return near;
}

static bool de_root_seen(
    const equation_solutions_t *roots,
    size_t index)
{
    const expr_t *candidate =
        equ_rhs(equ_solutions_at(roots, index));
    number_t candidate_value = expr_eval(candidate);
    bool seen = false;

    for (size_t i = 0u; i < index && !seen; ++i) {
        const expr_t *previous =
            equ_rhs(equ_solutions_at(roots, i));
        number_t previous_value = expr_eval(previous);

        seen = expr_struct_eq(candidate, previous) ||
               de_numbers_near(candidate_value, previous_value);
        num_destroy(&previous_value);
    }
    num_destroy(&candidate_value);
    return seen;
}

static expr_t *de_polynomial_exponential_basis(
    const expr_t *independent,
    const expr_t *rate,
    size_t power)
{
    expr_t *argument =
        expr_mul_simplify_owned(
            expr_clone(rate), expr_clone(independent));
    expr_t *exponential =
        de_simplify_unary_owned(argument, expr_exp);

    if (power == 0u)
        return exponential;
    return exponential
        ? expr_mul_simplify_owned(
              expr_pow_long(independent, (long)power),
              exponential)
        : NULL;
}

static expr_t *de_oscillatory_basis(
    const expr_t *independent,
    const expr_t *alpha,
    const expr_t *beta,
    size_t power,
    bool sine)
{
    expr_t *envelope =
        de_polynomial_exponential_basis(
            independent, alpha, power);
    expr_t *phase =
        expr_mul_simplify_owned(
            expr_clone(beta), expr_clone(independent));
    expr_t *oscillation = phase
        ? (sine ? expr_sin(phase) : expr_cos(phase))
        : NULL;
    expr_t *basis = envelope && oscillation
        ? expr_mul_simplify_owned(envelope, oscillation)
        : NULL;

    if (envelope && oscillation) {
        envelope = NULL;
        oscillation = NULL;
    }
    expr_free(oscillation);
    expr_free(phase);
    expr_free(envelope);
    return basis;
}

static expr_t *de_binomial_coefficient_expr(size_t n, size_t k)
{
    number_t n_value = num_create_from_long((long)n);
    number_t k_value = num_create_from_long((long)k);
    number_t coefficient = num_binomial(n_value, k_value);
    expr_t *expr = num_is_finite(coefficient)
        ? expr_new_const(coefficient)
        : NULL;

    num_destroy(&coefficient);
    num_destroy(&k_value);
    num_destroy(&n_value);
    return expr;
}

static bool de_repeated_quadratic_coefficients_match(
    const de_constant_linear_form_t *form,
    const expr_t *square,
    bool negative_square,
    size_t multiplicity)
{
    if (!form || !square || multiplicity == 0u ||
        form->order != 2u * multiplicity)
        return false;

    for (size_t order = 1u; order < form->order; order += 2u) {
        if (!expr_is_exact_zero(form->coefficients[order]))
            return false;
    }

    for (size_t j = 0u; j <= multiplicity; ++j) {
        expr_t *binomial =
            de_binomial_coefficient_expr(multiplicity, j);
        expr_t *square_power = expr_pow_long(
            square, (long)(multiplicity - j));
        expr_t *factor = binomial && square_power
            ? expr_mul(binomial, square_power)
            : NULL;
        if (factor && negative_square &&
            ((multiplicity - j) & 1u) != 0u) {
            expr_t *negative = expr_neg(factor);

            expr_free(factor);
            factor = negative;
        }
        expr_t *expected = factor
            ? expr_mul(form->coefficients[form->order], factor)
            : NULL;
        expr_t *expected_display = expected
            ? expr_display_simplified(expected)
            : NULL;
        bool matches = expected_display && expr_struct_eq(
            form->coefficients[2u * j], expected_display);
        expr_t *difference = !matches && expected_display
            ? expr_sub(form->coefficients[2u * j], expected_display)
            : NULL;
        expr_t *check = difference
            ? expr_display_simplified(difference)
            : NULL;

        if (!matches)
            matches = check && expr_is_exact_zero(check);

        expr_free(check);
        expr_free(difference);
        expr_free(expected_display);
        expr_free(expected);
        expr_free(factor);
        expr_free(square_power);
        expr_free(binomial);
        if (!matches)
            return false;
    }
    return true;
}

static int de_append_symbolic_repeated_quadratic_basis(
    const de_constant_linear_form_t *form,
    const expr_t *independent,
    de_basis_t *basis)
{
    const expr_t *frequency = NULL;
    const expr_t *square = NULL;
    number_t exponent = num_new();
    expr_t *denominator = NULL;
    expr_t *signed_square = NULL;
    expr_t *display_square = NULL;
    expr_t *frequency_expr = NULL;
    expr_t *frequency_display = NULL;
    expr_t *negative_frequency = NULL;
    size_t multiplicity;
    size_t basis_start = 0u;
    de_basis_group_t group_start = DE_BASIS_GROUP_NONE;
    size_t multiplicity_start = 0u;
    bool real_roots;
    int rc = -1;

    if (!form || !independent || !basis || form->order < 4u ||
        (form->order & 1u) != 0u)
        goto cleanup;
    multiplicity = form->order / 2u;
    basis_start = basis->count;
    group_start = basis->group;
    multiplicity_start = basis->multiplicity;

    denominator = expr_mul_simplify_owned(
        expr_const_long((long)multiplicity),
        expr_clone(form->coefficients[form->order]));
    signed_square = denominator
        ? expr_div_simplify_owned(
              expr_clone(form->coefficients[form->order - 2u]),
              denominator)
        : NULL;
    if (denominator)
        denominator = NULL;
    display_square = signed_square
        ? expr_display_simplified(signed_square)
        : NULL;
    square = display_square;
    real_roots = display_square &&
        expr_match_neg_expr(display_square, &square);
    if (!display_square || expr_is_exact_zero(display_square) ||
        !de_repeated_quadratic_coefficients_match(
            form, square, real_roots, multiplicity))
        goto cleanup;

    if (expr_match_pow_const(square, &frequency, &exponent) &&
        num_eq(exponent, NUM_TWO)) {
        frequency_display = expr_clone(frequency);
    } else {
        frequency_expr = expr_sqrt(square);
        frequency_display = frequency_expr
            ? expr_display_simplified(frequency_expr)
            : NULL;
    }
    if (!frequency_display)
        goto cleanup;

    if (real_roots) {
        negative_frequency = expr_neg(frequency_display);
        for (size_t family = 0u; negative_frequency && family < 2u;
             ++family) {
            const expr_t *rate = family == 0u
                ? frequency_display
                : negative_frequency;

            for (size_t power = 0u; power < multiplicity; ++power) {
                expr_t *item = de_polynomial_exponential_basis(
                    independent, rate, power);

                if (!item || de_basis_append(basis, item) != 0) {
                    expr_free(item);
                    goto cleanup;
                }
            }
        }
    } else {
        expr_t *zero = expr_const_zero();

        for (size_t family = 0u; zero && family < 2u; ++family) {
            for (size_t power = 0u; power < multiplicity; ++power) {
                expr_t *item = de_oscillatory_basis(
                    independent,
                    zero,
                    frequency_display,
                    power,
                    family != 0u);

                if (!item || de_basis_append(basis, item) != 0) {
                    expr_free(item);
                    expr_free(zero);
                    goto cleanup;
                }
            }
        }
        expr_free(zero);
    }
    rc = basis->count == basis_start + 2u * multiplicity ? 0 : -1;
    if (rc == 0) {
        basis->group = real_roots
            ? DE_BASIS_GROUP_REPEATED_REAL
            : DE_BASIS_GROUP_REPEATED_OSCILLATORY;
        basis->multiplicity = multiplicity;
    }

cleanup:
    if (rc != 0 && basis) {
        while (basis->count > basis_start) {
            --basis->count;
            expr_free(basis->items[basis->count]);
            basis->items[basis->count] = NULL;
        }
        basis->group = group_start;
        basis->multiplicity = multiplicity_start;
    }
    expr_free(negative_frequency);
    expr_free(frequency_display);
    expr_free(frequency_expr);
    expr_free(display_square);
    expr_free(signed_square);
    expr_free(denominator);
    num_destroy(&exponent);
    return rc;
}

static int de_append_root_basis(
    de_basis_t *basis,
    const expr_t *independent,
    const expr_t *root,
    size_t multiplicity)
{
    number_t value = expr_eval(root);
    number_t real = num_real_part(value);
    number_t imaginary = num_imag_part(value);
    expr_t *real_expr = NULL;
    expr_t *imaginary_expr = NULL;
    int rc = -1;

    if (de_number_near_zero(imaginary)) {
        real_expr = expr_new_const(real);
        for (size_t k = 0u; real_expr && k < multiplicity; ++k) {
            expr_t *item = de_polynomial_exponential_basis(
                independent, real_expr, k);

            if (!item || de_basis_append(basis, item) != 0) {
                expr_free(item);
                goto cleanup;
            }
        }
        rc = real_expr ? 0 : -1;
        goto cleanup;
    }

    if (num_get_sign(imaginary) < 0) {
        rc = 0;
        goto cleanup;
    }

    real_expr = expr_new_const(real);
    imaginary_expr = expr_new_const(imaginary);
    for (size_t k = 0u;
         real_expr && imaginary_expr && k < multiplicity;
         ++k) {
        expr_t *cosine = de_oscillatory_basis(
            independent, real_expr, imaginary_expr, k, false);
        expr_t *sine = de_oscillatory_basis(
            independent, real_expr, imaginary_expr, k, true);

        if (!cosine || !sine ||
            de_basis_append(basis, cosine) != 0) {
            expr_free(sine);
            expr_free(cosine);
            goto cleanup;
        }
        cosine = NULL;
        if (de_basis_append(basis, sine) != 0) {
            expr_free(sine);
            goto cleanup;
        }
        sine = NULL;
    }
    rc = real_expr && imaginary_expr ? 0 : -1;

cleanup:
    expr_free(imaginary_expr);
    expr_free(real_expr);
    num_destroy(&imaginary);
    num_destroy(&real);
    num_destroy(&value);
    return rc;
}

static void de_characteristic_coefficients_free(number_t *coefficients,
                                                size_t degree)
{
    if (!coefficients)
        return;
    for (size_t i = 0u; i <= degree; ++i)
        num_destroy(&coefficients[i]);
    free(coefficients);
}

static int de_solve_characteristic(
    const equation_t *characteristic,
    const expr_t *polynomial,
    const expr_t *root_variable,
    equation_solutions_t *roots)
{
    number_t *coefficients = NULL;
    size_t degree = 0u;
    int rc;

    if (!characteristic || !polynomial || !root_variable || !roots ||
        !equ_match_polynomial_alloc(
            polynomial, root_variable, &coefficients, &degree))
        return -1;

    switch (degree) {
        case 2u:
            rc = equ_solve_quadratic_coefficients(
                coefficients, root_variable, roots);
            break;

        case 3u:
            rc = equ_solve_cubic_coefficients(
                coefficients, root_variable, roots);
            break;

        case 4u:
            rc = equ_solve_quartic_coefficients(
                coefficients, root_variable, roots);
            break;

        default:
            rc = equ_solve_for_into(
                characteristic, root_variable, roots);
            break;
    }

    de_characteristic_coefficients_free(coefficients, degree);
    return rc;
}

static int de_construct_basis(
    const de_constant_linear_form_t *form,
    const expr_t *independent,
    de_basis_t *basis)
{
    expr_t *root_variable =
        expr_new_named_var(NUM_NAN, "r");
    expr_t *polynomial = root_variable
        ? de_characteristic_polynomial(form, root_variable)
        : NULL;
    expr_t *zero = expr_const_zero();
    equation_t *characteristic =
        polynomial && zero ? equ_new(polynomial, zero) : NULL;
    equation_solutions_t roots_storage = { 0 };
    equation_solutions_t *roots = characteristic
        ? &roots_storage
        : NULL;
    int rc = -1;

    if (de_append_symbolic_repeated_quadratic_basis(
            form, independent, basis) == 0) {
        rc = 0;
        goto cleanup;
    }

    if (!roots ||
        de_solve_characteristic(
            characteristic, polynomial, root_variable, roots) != 0 ||
        equ_solutions_count(roots) == 0u)
        goto cleanup;

    for (size_t i = 0u; i < equ_solutions_count(roots); ++i) {
        const expr_t *root;
        size_t multiplicity;

        if (de_root_seen(roots, i))
            continue;
        root = equ_rhs(equ_solutions_at(roots, i));
        multiplicity = de_root_multiplicity(
            polynomial,
            root_variable,
            root,
            form->order);
        if (multiplicity == 0u)
            multiplicity = 1u;
        if (de_append_root_basis(
                basis,
                independent,
                root,
                multiplicity) != 0)
            goto cleanup;
    }
    if (basis->count == form->order)
        rc = 0;

cleanup:
    equ_solutions_clear(&roots_storage);
    equ_free(characteristic);
    expr_free(zero);
    expr_free(polynomial);
    expr_free(root_variable);
    return rc;
}

static expr_t *de_derivative_at(
    const expr_t *expr,
    const expr_t *independent,
    size_t order,
    const expr_t *point)
{
    expr_t *derivative = order
        ? expr_create_nth_deriv(
              (unsigned int)order, expr, independent)
        : expr_clone(expr);
    expr_t *at_point = derivative
        ? expr_substitute(derivative, independent, point)
        : NULL;
    expr_t *simplified =
        at_point ? expr_simplify_owned(at_point) : NULL;

    expr_free(derivative);
    return simplified;
}

static bool de_condition_data(
    const diffequ_t *de,
    size_t index,
    const expr_t *dependent,
    const expr_t *independent,
    size_t *order_out,
    const expr_t **point_out,
    const expr_t **value_out)
{
    const equation_t *condition = de->conditions[index];
    const expr_t *left = condition ? equ_lhs(condition) : NULL;

    if (!condition ||
        !left ||
        de->condition_point_counts[index] != 1u)
        return false;
    if (expr_struct_eq(left, dependent)) {
        *order_out = 0u;
    } else {
        size_t order;

        if (!expr_is_formal_derivative(left) ||
            !expr_struct_eq(
                expr_formal_derivative_dependent(left), dependent))
            return false;
        order = expr_formal_derivative_order(left);
        for (size_t i = 0u; i < order; ++i) {
            if (!expr_struct_eq(
                    expr_formal_derivative_wrt_at(left, i),
                    independent))
                return false;
        }
        *order_out = order;
    }
    *point_out = de->condition_points[index][0];
    *value_out = equ_rhs(condition);
    return true;
}

static bool de_match_e_power_forcing(const expr_t *forcing,
                                     const expr_t **exponent_out)
{
    const expr_t *base = NULL;
    const expr_t *exponent = NULL;
    expr_t *e = NULL;
    bool matches = false;

    if (!expr_match_pow_expr(forcing, &base, &exponent))
        return false;

    e = expr_new_named_const(NUM_E, "e");
    matches = e && expr_struct_eq(base, e);
    expr_free(e);
    if (matches && exponent_out)
        *exponent_out = exponent;
    return matches;
}

static bool de_match_exponential_forcing(const expr_t *forcing,
                                         const expr_t **exponent_out)
{
    return expr_match_exp_expr(forcing, exponent_out) ||
           de_match_e_power_forcing(forcing, exponent_out);
}

static bool de_is_unit_rate_exponential_forcing(
    const expr_t *forcing,
    const expr_t *independent)
{
    const expr_t *exponent = NULL;

    return de_match_exponential_forcing(forcing, &exponent) &&
           expr_struct_eq(exponent, independent);
}

static expr_t *de_exponential_particular_solution(
    const de_constant_linear_form_t *form,
    const expr_t *independent)
{
    const expr_t *exponent;
    number_t offset = num_new();
    number_t rate = num_new();
    expr_t *characteristic_value = NULL;
    expr_t *particular = NULL;

    if (!form || !form->forcing || !independent ||
        !de_match_exponential_forcing(form->forcing, &exponent) ||
        !equ_match_affine_linear_expr(
            exponent, independent, false, &offset, &rate))
        goto cleanup;

    characteristic_value = expr_const_zero();
    for (size_t i = 0u;
         characteristic_value && i <= form->order;
         ++i) {
        number_t power = num_pow_int(rate, (int)i);
        expr_t *term = expr_mul_simplify_owned(
            expr_clone(form->coefficients[i]),
            expr_new_const(power));

        num_destroy(&power);
        characteristic_value = term
            ? expr_add_simplify_owned(characteristic_value, term)
            : NULL;
    }
    if (characteristic_value &&
        !expr_is_exact_zero(characteristic_value)) {
        expr_t *coefficient = expr_div_simplify_owned(
            expr_const_one(), characteristic_value);

        characteristic_value = NULL;
        particular = coefficient
            ? expr_mul_simplify_owned(
                  coefficient, expr_clone(form->forcing))
            : NULL;
    }

cleanup:
    expr_free(characteristic_value);
    num_destroy(&rate);
    num_destroy(&offset);
    return particular;
}

static number_t *de_number_array_new(size_t count)
{
    number_t *values = calloc(count, sizeof(*values));

    if (!values)
        return NULL;
    for (size_t i = 0u; i < count; ++i)
        values[i] = num_new();
    return values;
}

static void de_number_array_free(number_t *values, size_t count)
{
    if (!values)
        return;
    for (size_t i = 0u; i < count; ++i)
        num_destroy(&values[i]);
    free(values);
}

static expr_t *de_add_owned_unsimplified(expr_t *left,
                                         expr_t *right,
                                         bool subtract)
{
    expr_t *result;

    if (!left || !right) {
        expr_free(right);
        expr_free(left);
        return NULL;
    }
    result = subtract ? expr_sub(left, right) : expr_add(left, right);
    expr_free(right);
    expr_free(left);
    return result;
}

static expr_t *de_append_sum_owned(expr_t *sum,
                                   expr_t *term,
                                   bool subtract)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool inner_subtraction = false;

    if (!term) {
        expr_free(sum);
        return NULL;
    }
    if (expr_match_add_sub_expr(
            term, &left, &right, &inner_subtraction)) {
        expr_t *left_copy = expr_clone(left);
        expr_t *right_copy = expr_clone(right);

        expr_free(term);
        sum = de_append_sum_owned(sum, left_copy, subtract);
        return de_append_sum_owned(
            sum,
            right_copy,
            subtract != inner_subtraction);
    }
    if (!sum)
        return subtract ? expr_negate_owned(term) : term;
    return de_add_owned_unsimplified(sum, term, subtract);
}

static expr_t *de_combine_particulars_owned(expr_t *left,
                                            expr_t *right,
                                            bool subtract)
{
    expr_t *sum = de_append_sum_owned(NULL, left, false);

    return de_append_sum_owned(sum, right, subtract);
}

static number_t de_derivative_factor(size_t degree, size_t order)
{
    number_t factor = num_create_from_long(1L);

    for (size_t i = 0u; i < order; ++i) {
        number_t next = num_mul_long(factor, (long)(degree - i));

        num_destroy(&factor);
        factor = next;
    }
    return factor;
}

static expr_t *de_polynomial_from_coefficients(
    const number_t *coefficients,
    size_t degree,
    const expr_t *independent)
{
    expr_t *polynomial = NULL;

    for (size_t cursor = degree + 1u; cursor > 0u; --cursor) {
        size_t i = cursor - 1u;
        expr_t *term;

        if (num_is_zero(coefficients[i]))
            continue;
        term = expr_new_const(coefficients[i]);
        if (i > 0u) {
            term = expr_mul_simplify_owned(
                term,
                expr_pow_long(independent, (long)i));
        }
        if (!term) {
            expr_free(polynomial);
            return NULL;
        }
        if (!polynomial) {
            polynomial = term;
        } else {
            polynomial = de_add_owned_unsimplified(
                polynomial, term, false);
        }
    }
    return polynomial ? polynomial : expr_const_zero();
}

static expr_t *de_polynomial_particular_solution(
    const de_constant_linear_form_t *form,
    const expr_t *independent)
{
    number_t *forcing = NULL;
    number_t *operator_coefficients = NULL;
    number_t *solution = NULL;
    size_t forcing_degree = 0u;
    size_t root_multiplicity = 0u;
    size_t solution_degree;
    expr_t *particular = NULL;

    if (!equ_match_polynomial_alloc(
            form->forcing,
            independent,
            &forcing,
            &forcing_degree))
        return NULL;

    operator_coefficients =
        de_number_array_new(form->order + 1u);
    if (!operator_coefficients)
        goto cleanup;
    for (size_t i = 0u; i <= form->order; ++i) {
        number_t value = num_new();

        if (!expr_match_const_value(
                form->coefficients[i], &value)) {
            num_destroy(&value);
            value = expr_eval(form->coefficients[i]);
        }
        if (!num_is_finite(value)) {
            num_destroy(&value);
            goto cleanup;
        }
        num_destroy(&operator_coefficients[i]);
        operator_coefficients[i] = value;
    }

    while (root_multiplicity <= form->order &&
           num_is_zero(operator_coefficients[root_multiplicity]))
        root_multiplicity++;
    if (root_multiplicity > form->order ||
        forcing_degree > SIZE_MAX - root_multiplicity)
        goto cleanup;

    solution_degree = forcing_degree + root_multiplicity;
    solution = de_number_array_new(solution_degree + 1u);
    if (!solution)
        goto cleanup;

    for (size_t cursor = forcing_degree + 1u; cursor > 0u; --cursor) {
        size_t degree = cursor - 1u;
        number_t rhs = num_clone(forcing[degree]);

        for (size_t order = root_multiplicity + 1u;
             order <= form->order &&
             degree + order <= solution_degree;
             ++order) {
            number_t factor =
                de_derivative_factor(degree + order, order);
            number_t scaled =
                num_mul(operator_coefficients[order], factor);
            number_t contribution =
                num_mul(scaled, solution[degree + order]);
            number_t next = num_sub(rhs, contribution);

            num_destroy(&contribution);
            num_destroy(&scaled);
            num_destroy(&factor);
            num_destroy(&rhs);
            rhs = next;
        }
        {
            size_t solution_index = degree + root_multiplicity;
            number_t factor = de_derivative_factor(
                solution_index, root_multiplicity);
            number_t denominator = num_mul(
                operator_coefficients[root_multiplicity], factor);
            number_t coefficient = num_div(rhs, denominator);

            num_destroy(&denominator);
            num_destroy(&factor);
            num_destroy(&rhs);
            num_destroy(&solution[solution_index]);
            solution[solution_index] = coefficient;
        }
    }

    particular = de_polynomial_from_coefficients(
        solution, solution_degree, independent);

cleanup:
    de_number_array_free(solution,
                         solution ? solution_degree + 1u : 0u);
    de_number_array_free(
        operator_coefficients, form->order + 1u);
    de_number_array_free(forcing, forcing_degree + 1u);
    return particular;
}

static expr_t *de_secant_cubed_particular_solution(
    const de_constant_linear_form_t *form,
    const expr_t *independent)
{
    const expr_t *secant = NULL;
    expr_t *variables[1] = { (expr_t *)independent };
    number_t exponent = num_new();
    number_t affine_constant = num_new();
    number_t affine_coefficient = num_new();
    number_t dependent_coefficient = num_new();
    number_t first_coefficient = num_new();
    number_t leading_coefficient = num_new();
    number_t affine_squared = num_new();
    number_t expected_dependent = num_new();
    number_t denominator = num_new();
    number_t scale = num_new();
    number_t three = num_create_from_long(3L);
    expr_t *particular = NULL;

    if (!form || !independent || form->order != 2u ||
        !expr_match_pow_const(form->forcing, &secant, &exponent) ||
        !num_eq(exponent, three) ||
        !expr_match_unary_affine_kind(
            secant,
            EXPR_PATTERN_UNARY_SEC,
            1u,
            variables,
            &affine_constant,
            &affine_coefficient) ||
        !expr_match_const_value(
            form->coefficients[0], &dependent_coefficient) ||
        !expr_match_const_value(
            form->coefficients[1], &first_coefficient) ||
        !expr_match_const_value(
            form->coefficients[2], &leading_coefficient) ||
        !num_eq(first_coefficient, NUM_ZERO) ||
        num_eq(dependent_coefficient, NUM_ZERO))
        goto cleanup;

    num_destroy(&affine_squared);
    affine_squared = num_mul(
        affine_coefficient, affine_coefficient);
    num_destroy(&expected_dependent);
    expected_dependent = num_mul(
        leading_coefficient, affine_squared);
    if (!num_eq(dependent_coefficient, expected_dependent))
        goto cleanup;

    num_destroy(&denominator);
    denominator = num_mul(NUM_TWO, dependent_coefficient);
    num_destroy(&scale);
    scale = num_div(NUM_ONE, denominator);
    particular = expr_clone(secant);
    if (particular) {
        expr_t *scaled = expr_mul_num(particular, &scale);

        expr_free(particular);
        particular = scaled ? expr_simplify_owned(scaled) : NULL;
    }

cleanup:
    num_destroy(&three);
    num_destroy(&scale);
    num_destroy(&denominator);
    num_destroy(&expected_dependent);
    num_destroy(&affine_squared);
    num_destroy(&leading_coefficient);
    num_destroy(&first_coefficient);
    num_destroy(&dependent_coefficient);
    num_destroy(&affine_coefficient);
    num_destroy(&affine_constant);
    num_destroy(&exponent);
    return particular;
}

static expr_t *de_particular_solution(
    const de_constant_linear_form_t *form,
    const expr_t *independent,
    const de_basis_t *basis)
{
    matrix_t *wronskian = NULL;
    matrix_t *forcing = NULL;
    matrix_t *rates = NULL;
    expr_t *particular = NULL;
    bool use_direct_exponential;
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *scaled_base = NULL;
    number_t scale = num_new();
    bool is_subtraction = false;

    if (expr_is_exact_zero(form->forcing))
        particular = expr_const_zero();
    if (particular)
        goto cleanup_scale;

    if (expr_match_scaled_expr(
            form->forcing, &scale, &scaled_base)) {
        de_constant_linear_form_t scaled_form = *form;
        const expr_t *scaled_left = NULL;
        const expr_t *scaled_right = NULL;
        bool scaled_subtraction = false;

        if (expr_match_add_sub_expr(
                scaled_base,
                &scaled_left,
                &scaled_right,
                &scaled_subtraction)) {
            de_constant_linear_form_t left_form = *form;
            de_constant_linear_form_t right_form = *form;
            expr_t *left_forcing = expr_mul_simplify_owned(
                expr_new_const(scale), expr_clone(scaled_left));
            expr_t *right_forcing = expr_mul_simplify_owned(
                expr_new_const(scale), expr_clone(scaled_right));
            expr_t *left_particular;
            expr_t *right_particular;

            if (!left_forcing || !right_forcing) {
                expr_free(right_forcing);
                expr_free(left_forcing);
                goto cleanup_scale;
            }
            left_form.forcing = left_forcing;
            right_form.forcing = right_forcing;
            left_particular = de_particular_solution(
                &left_form, independent, basis);
            right_particular = de_particular_solution(
                &right_form, independent, basis);
            expr_free(right_forcing);
            expr_free(left_forcing);
            particular = de_combine_particulars_owned(
                left_particular,
                right_particular,
                scaled_subtraction);
            goto cleanup_scale;
        }

        scaled_form.forcing = (expr_t *)scaled_base;
        particular = de_particular_solution(
            &scaled_form, independent, basis);
        if (particular) {
            expr_t *expanded;

            particular = expr_mul_simplify_owned(
                expr_new_const(scale), particular);
            expanded = particular
                ? expr_display_expanded(particular)
                : NULL;
            if (expanded) {
                expr_free(particular);
                particular = expanded;
            }
        }
        goto cleanup_scale;
    }

    if (expr_match_add_sub_expr(
            form->forcing, &left, &right, &is_subtraction)) {
        de_constant_linear_form_t left_form = *form;
        de_constant_linear_form_t right_form = *form;
        expr_t *left_particular;
        expr_t *right_particular;

        left_form.forcing = (expr_t *)left;
        right_form.forcing = (expr_t *)right;
        left_particular = de_particular_solution(
            &left_form, independent, basis);
        right_particular = de_particular_solution(
            &right_form, independent, basis);
        if (!left_particular || !right_particular) {
            expr_free(right_particular);
            expr_free(left_particular);
            goto cleanup_scale;
        }
        particular = de_combine_particulars_owned(
            left_particular,
            right_particular,
            is_subtraction);
        goto cleanup_scale;
    }

    use_direct_exponential =
        form->order > 2u ||
        de_is_unit_rate_exponential_forcing(
            form->forcing, independent);
    particular = use_direct_exponential
        ? de_exponential_particular_solution(form, independent)
        : NULL;
    if (particular)
        goto cleanup_scale;

    particular = de_polynomial_particular_solution(
        form, independent);
    if (particular)
        goto cleanup_scale;

    particular = de_secant_cubed_particular_solution(
        form, independent);
    if (particular)
        goto cleanup_scale;

    wronskian = mat_new_expr(form->order, form->order);
    forcing = mat_new_expr(form->order, 1u);
    if (!wronskian || !forcing)
        goto cleanup;

    for (size_t row = 0u; row < form->order; ++row) {
        expr_t *rhs = row + 1u == form->order
            ? expr_div_simplify_owned(
                  expr_clone(form->forcing),
                  expr_clone(form->coefficients[form->order]))
            : expr_const_zero();

        if (!rhs)
            goto cleanup;
        mat_set(forcing, row, 0u, &rhs);
        expr_free(rhs);

        for (size_t col = 0u; col < form->order; ++col) {
            expr_t *entry = row
                ? expr_create_nth_deriv(
                      (unsigned int)row,
                      basis->items[col],
                      independent)
                : expr_clone(basis->items[col]);

            if (!entry)
                goto cleanup;
            mat_set(wronskian, row, col, &entry);
            expr_free(entry);
        }
    }

    rates = mat_solve(wronskian, forcing);
    if (!rates)
        goto cleanup;
    particular = expr_const_zero();
    for (size_t i = 0u; particular && i < form->order; ++i) {
        expr_t *rate = NULL;
        expr_t *simplified_rate;
        expr_t *integral;
        expr_t *term;

        mat_get(rates, i, 0u, &rate);
        simplified_rate =
            rate ? expr_display_simplified(rate) : NULL;
        integral = simplified_rate
            ? de_integrate_or_formal(simplified_rate, independent)
            : NULL;
        expr_free(simplified_rate);
        term = integral
            ? expr_mul_simplify_owned(
                  expr_clone(basis->items[i]), integral)
            : NULL;
        particular = term
            ? expr_add_simplify_owned(particular, term)
            : NULL;
    }

cleanup:
    mat_free(rates);
    mat_free(forcing);
    mat_free(wronskian);
cleanup_scale:
    num_destroy(&scale);
    return particular;
}

static expr_t *de_apply_conditions(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const de_basis_t *basis,
    const expr_t *particular)
{
    matrix_t *matrix = NULL;
    matrix_t *right = NULL;
    matrix_t *constants = NULL;
    expr_t *solution = NULL;

    if (de->condition_count != basis->count)
        return NULL;

    matrix = mat_new_expr(basis->count, basis->count);
    right = mat_new_expr(basis->count, 1u);
    if (!matrix || !right)
        goto cleanup;

    for (size_t row = 0u; row < basis->count; ++row) {
        size_t derivative_order;
        const expr_t *point;
        const expr_t *value;
        expr_t *particular_at;
        expr_t *rhs;

        if (!de_condition_data(
                de,
                row,
                dependent,
                independent,
                &derivative_order,
                &point,
                &value))
            goto cleanup;
        particular_at = de_derivative_at(
            particular,
            independent,
            derivative_order,
            point);
        rhs = particular_at
            ? expr_sub_simplify_owned(
                  expr_clone(value), particular_at)
            : NULL;
        if (particular_at)
            particular_at = NULL;
        if (!rhs)
            goto cleanup;
        mat_set(right, row, 0u, &rhs);
        expr_free(rhs);

        for (size_t col = 0u; col < basis->count; ++col) {
            expr_t *entry = de_derivative_at(
                basis->items[col],
                independent,
                derivative_order,
                point);

            if (!entry)
                goto cleanup;
            mat_set(matrix, row, col, &entry);
            expr_free(entry);
        }
    }

    constants = mat_solve(matrix, right);
    if (!constants)
        goto cleanup;
    solution = expr_clone(particular);
    for (size_t i = 0u; solution && i < basis->count; ++i) {
        expr_t *constant = NULL;
        expr_t *term;

        mat_get(constants, i, 0u, &constant);
        term = constant
            ? expr_mul_simplify_owned(
                  expr_clone(constant),
                  expr_clone(basis->items[i]))
            : NULL;
        solution = term
            ? expr_add_simplify_owned(solution, term)
            : NULL;
    }
    if (solution) {
        expr_t *display = expr_display_simplified(solution);

        if (display) {
            expr_free(solution);
            solution = display;
        }
    }

cleanup:
    mat_free(constants);
    mat_free(right);
    mat_free(matrix);
    return solution;
}

static expr_t *de_indexed_arbitrary_constant(size_t index)
{
    char name[32];

    snprintf(name, sizeof(name), "C%zu", index);
    return expr_new_named_const(NUM_NAN, name);
}

static expr_t *de_explicit_amplitude(const expr_t *independent,
                                     size_t multiplicity,
                                     size_t first_constant)
{
    expr_t *amplitude = NULL;

    if (!independent || multiplicity == 0u)
        return NULL;
    for (size_t power = 0u; power < multiplicity; ++power) {
        expr_t *constant = de_indexed_arbitrary_constant(
            first_constant + power);
        expr_t *independent_power = power == 1u
            ? expr_clone(independent)
            : power > 1u
            ? expr_pow_long(independent, (long)power)
            : NULL;
        expr_t *term = power == 0u
            ? expr_clone(constant)
            : constant && independent_power
            ? expr_mul(constant, independent_power)
            : NULL;
        expr_t *sum = amplitude && term
            ? expr_add(amplitude, term)
            : term
            ? expr_clone(term)
            : NULL;

        expr_free(term);
        expr_free(independent_power);
        expr_free(constant);
        expr_free(amplitude);
        amplitude = sum;
        if (!amplitude)
            return NULL;
    }
    return amplitude;
}

static expr_t *de_summed_amplitude(const expr_t *independent,
                                   size_t multiplicity,
                                   size_t first_constant)
{
    expr_t *index = expr_new_named_var(NUM_NAN, "k");
    expr_t *constant_index = index
        ? expr_add_long(index, (long)first_constant)
        : NULL;
    expr_t *constant = constant_index
        ? expr_new_indexed_symbol("C", constant_index)
        : NULL;
    expr_t *power = index && independent
        ? expr_pow_xp(independent, index)
        : NULL;
    expr_t *term = constant && power
        ? expr_mul(constant, power)
        : NULL;
    expr_t *upper = multiplicity > 0u
        ? expr_const_long((long)(multiplicity - 1u))
        : NULL;
    expr_t *amplitude = term && index && upper
        ? expr_new_finite_summation(term, index, upper)
        : NULL;

    expr_free(upper);
    expr_free(term);
    expr_free(power);
    expr_free(constant);
    expr_free(constant_index);
    expr_free(index);
    return amplitude;
}

static expr_t *de_grouped_repeated_quadratic_solution(
    const de_basis_t *basis,
    const expr_t *independent,
    const expr_t *particular)
{
    expr_t *first_amplitude = NULL;
    expr_t *second_amplitude = NULL;
    expr_t *first_term = NULL;
    expr_t *second_term = NULL;
    expr_t *homogeneous = NULL;
    expr_t *solution = NULL;

    if (!basis || !independent || !particular ||
        basis->multiplicity < 2u ||
        basis->count != 2u * basis->multiplicity ||
        basis->group == DE_BASIS_GROUP_NONE)
        return NULL;

    if (basis->multiplicity < 4u) {
        first_amplitude = de_explicit_amplitude(
            independent, basis->multiplicity, 1u);
        second_amplitude = de_explicit_amplitude(
            independent,
            basis->multiplicity,
            basis->multiplicity + 1u);
    } else {
        first_amplitude = de_summed_amplitude(
            independent, basis->multiplicity, 1u);
        second_amplitude = de_summed_amplitude(
            independent,
            basis->multiplicity,
            basis->multiplicity + 1u);
    }
    first_term = first_amplitude
        ? expr_mul(first_amplitude, basis->items[0])
        : NULL;
    second_term = second_amplitude
        ? expr_mul(
              second_amplitude,
              basis->items[basis->multiplicity])
        : NULL;
    homogeneous = first_term && second_term
        ? expr_add(first_term, second_term)
        : NULL;
    if (!homogeneous)
        goto cleanup;

    if (expr_is_exact_zero(particular)) {
        solution = homogeneous;
        homogeneous = NULL;
    } else {
        solution = expr_add(particular, homogeneous);
    }

cleanup:
    expr_free(homogeneous);
    expr_free(second_term);
    expr_free(first_term);
    expr_free(second_amplitude);
    expr_free(first_amplitude);
    return solution;
}

static expr_t *de_general_solution(
    const de_basis_t *basis,
    const expr_t *independent,
    const expr_t *particular)
{
    bool separate_particular = !expr_is_exact_zero(particular);
    expr_t *solution = separate_particular
        ? expr_clone(particular)
        : expr_const_zero();

    if (basis && basis->group != DE_BASIS_GROUP_NONE) {
        expr_free(solution);
        return de_grouped_repeated_quadratic_solution(
            basis, independent, particular);
    }

    for (size_t i = 0u; i < basis->count; ++i) {
        char name[32];
        expr_t *constant;
        expr_t *term;

        snprintf(name, sizeof(name), "C%zu", i + 1u);
        constant = expr_new_named_const(NUM_NAN, name);
        term = constant
            ? expr_mul_simplify_owned(
                  constant, expr_clone(basis->items[i]))
            : NULL;
        if (!term) {
            expr_free(solution);
            return NULL;
        }
        if (!separate_particular) {
            solution = expr_add_simplify_owned(solution, term);
        } else {
            expr_t *sum = expr_add(solution, term);

            expr_free(term);
            expr_free(solution);
            solution = sum;
        }
    }
    return solution;
}

de_attempt_t de_attempt_constant_coefficient_linear(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    size_t order,
    const expr_t *residual,
    equation_t **solution_out)
{
    de_constant_linear_form_t form = { 0 };
    de_basis_t basis = { 0 };
    expr_t *particular = NULL;
    expr_t *solution = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    if (order < 2u ||
        !de_decompose_constant_linear(
            residual,
            dependent,
            independent,
            order,
            &form))
        goto cleanup;
    attempt = DE_ATTEMPT_FAILED;
    if (de_construct_basis(&form, independent, &basis) != 0)
        goto cleanup;
    particular =
        de_particular_solution(&form, independent, &basis);
    if (!particular)
        goto cleanup;

    if (de->condition_count == 0u)
        solution = de_general_solution(
            &basis, independent, particular);
    else
        solution = de_apply_conditions(
            de,
            independent,
            dependent,
            &basis,
            particular);
    if (!solution)
        goto cleanup;

    *solution_out = equ_new(dependent, solution);
    if (*solution_out)
        attempt = DE_ATTEMPT_SOLVED;

cleanup:
    expr_free(solution);
    expr_free(particular);
    de_basis_clear(&basis);
    de_constant_linear_form_clear(&form);
    return attempt;
}

de_attempt_t de_attempt_repeated_quadratic_operator(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *signed_square,
    size_t multiplicity,
    equation_t **solution_out)
{
    const expr_t *square = signed_square;
    const expr_t *frequency = NULL;
    de_constant_linear_form_t form = { 0 };
    de_basis_t basis = { 0 };
    number_t exponent = num_new();
    expr_t *frequency_expr = NULL;
    expr_t *frequency_display = NULL;
    expr_t *negative_frequency = NULL;
    expr_t *first_basis = NULL;
    expr_t *second_basis = NULL;
    expr_t *first_amplitude = NULL;
    expr_t *second_amplitude = NULL;
    expr_t *first_term = NULL;
    expr_t *second_term = NULL;
    expr_t *particular = NULL;
    expr_t *solution = NULL;
    bool real_roots;
    de_attempt_t attempt = DE_ATTEMPT_FAILED;

    if (!de || !independent || !dependent || !signed_square ||
        multiplicity < 2u || multiplicity > SIZE_MAX / 2u ||
        !solution_out)
        return DE_ATTEMPT_NOT_MATCHED;

    real_roots = expr_match_neg_expr(signed_square, &square);
    if (!square || expr_is_exact_zero(square))
        goto cleanup;
    if (expr_match_pow_const(square, &frequency, &exponent) &&
        num_eq(exponent, NUM_TWO)) {
        frequency_display = expr_clone(frequency);
    } else {
        frequency_expr = expr_sqrt(square);
        frequency_display = frequency_expr
            ? expr_display_simplified(frequency_expr)
            : NULL;
    }
    if (!frequency_display)
        goto cleanup;

    if (de->condition_count == 0u) {
        if (real_roots) {
            negative_frequency = expr_neg(frequency_display);
            first_basis = de_polynomial_exponential_basis(
                independent, frequency_display, 0u);
            second_basis = negative_frequency
                ? de_polynomial_exponential_basis(
                      independent, negative_frequency, 0u)
                : NULL;
        } else {
            expr_t *zero = expr_const_zero();

            first_basis = zero
                ? de_oscillatory_basis(
                      independent,
                      zero,
                      frequency_display,
                      0u,
                      false)
                : NULL;
            second_basis = zero
                ? de_oscillatory_basis(
                      independent,
                      zero,
                      frequency_display,
                      0u,
                      true)
                : NULL;
            expr_free(zero);
        }
        first_amplitude = multiplicity < 4u
            ? de_explicit_amplitude(independent, multiplicity, 1u)
            : de_summed_amplitude(independent, multiplicity, 1u);
        second_amplitude = multiplicity < 4u
            ? de_explicit_amplitude(
                  independent, multiplicity, multiplicity + 1u)
            : de_summed_amplitude(
                  independent, multiplicity, multiplicity + 1u);
        first_term = first_amplitude && first_basis
            ? expr_mul(first_amplitude, first_basis)
            : NULL;
        second_term = second_amplitude && second_basis
            ? expr_mul(second_amplitude, second_basis)
            : NULL;
        solution = first_term && second_term
            ? expr_add(first_term, second_term)
            : NULL;
        if (!solution)
            goto cleanup;
        *solution_out = equ_new(dependent, solution);
        if (*solution_out)
            attempt = DE_ATTEMPT_SOLVED;
        goto cleanup;
    }

    form.order = 2u * multiplicity;
    form.coefficients =
        calloc(form.order + 1u, sizeof(*form.coefficients));
    if (!form.coefficients)
        goto cleanup;

    for (size_t j = 0u; j <= multiplicity; ++j) {
        expr_t *binomial =
            de_binomial_coefficient_expr(multiplicity, j);
        expr_t *power = expr_pow_long(
            signed_square, (long)(multiplicity - j));
        expr_t *coefficient = binomial && power
            ? expr_mul(binomial, power)
            : NULL;

        form.coefficients[2u * j] = coefficient
            ? expr_display_simplified(coefficient)
            : NULL;
        expr_free(coefficient);
        expr_free(power);
        expr_free(binomial);
        if (!form.coefficients[2u * j])
            goto cleanup;
        if (j < multiplicity) {
            form.coefficients[2u * j + 1u] = expr_const_zero();
            if (!form.coefficients[2u * j + 1u])
                goto cleanup;
        }
    }

    if (de_construct_basis(&form, independent, &basis) != 0)
        goto cleanup;
    particular = expr_const_zero();
    if (!particular)
        goto cleanup;
    solution = de->condition_count == 0u
        ? de_general_solution(&basis, independent, particular)
        : de_apply_conditions(
              de,
              independent,
              dependent,
              &basis,
              particular);
    if (!solution)
        goto cleanup;

    *solution_out = equ_new(dependent, solution);
    if (*solution_out)
        attempt = DE_ATTEMPT_SOLVED;

cleanup:
    expr_free(second_term);
    expr_free(first_term);
    expr_free(second_amplitude);
    expr_free(first_amplitude);
    expr_free(second_basis);
    expr_free(first_basis);
    expr_free(negative_frequency);
    expr_free(frequency_display);
    expr_free(frequency_expr);
    num_destroy(&exponent);
    expr_free(solution);
    expr_free(particular);
    de_basis_clear(&basis);
    de_constant_linear_form_clear(&form);
    return attempt;
}
