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

typedef struct {
    expr_t **items;
    size_t count;
    size_t capacity;
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

static int de_construct_basis(
    const de_constant_linear_form_t *form,
    const expr_t *independent,
    de_basis_t *basis)
{
    expr_t *root_variable =
        expr_new_named_var(NUM_ZERO, "r");
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

    if (!roots ||
        equ_solve_for_into(
            characteristic, root_variable, roots) != 0 ||
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

    if (!condition || !left || !de->condition_points[index])
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
    *point_out = de->condition_points[index];
    *value_out = equ_rhs(condition);
    return true;
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
        !expr_match_exp_expr(form->forcing, &exponent) ||
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
        particular = expr_div_simplify_owned(
            expr_clone(form->forcing), characteristic_value);
        characteristic_value = NULL;
    }

cleanup:
    expr_free(characteristic_value);
    num_destroy(&rate);
    num_destroy(&offset);
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

    if (expr_is_exact_zero(form->forcing))
        return expr_const_zero();

    particular = form->order > 2u
        ? de_exponential_particular_solution(form, independent)
        : NULL;
    if (particular)
        return particular;

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

static expr_t *de_general_solution(
    const de_basis_t *basis,
    const expr_t *particular)
{
    expr_t *solution = expr_clone(particular);

    for (size_t i = 0u; solution && i < basis->count; ++i) {
        char name[32];
        expr_t *constant;
        expr_t *term;

        snprintf(name, sizeof(name), "C%zu", i + 1u);
        constant = expr_new_named_const(NUM_NAN, name);
        term = constant
            ? expr_mul_simplify_owned(
                  constant, expr_clone(basis->items[i]))
            : NULL;
        solution = term
            ? expr_add_simplify_owned(solution, term)
            : NULL;
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
        solution = de_general_solution(&basis, particular);
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
