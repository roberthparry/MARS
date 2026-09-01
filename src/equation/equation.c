#include <stdbool.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "equation.h"
#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"
#include "expression.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#include "ustring.h"

struct equation_t {
    expr_t *lhs;
    expr_t *rhs;
    expr_bindings_t *bindings;
    string_t *lhs_display_TeX;
    string_t *rhs_display_TeX;
};

static void equ_solutions_reset(equation_solutions_t *solutions)
{
    if (!solutions)
        return;
    solutions->solutions = NULL;
    solutions->count = 0u;
    solutions->status = EQUATION_SOLVE_INVALID;
}

static equation_solutions_t *equ_solutions_new(void)
{
    equation_solutions_t *solutions = calloc(1u, sizeof(*solutions));

    if (!solutions)
        return NULL;
    equ_solutions_reset(solutions);
    return solutions;
}

void equ_solutions_free(equation_solutions_t *solutions)
{
    if (!solutions)
        return;
    equ_solutions_clear(solutions);
    free(solutions);
}

equation_t *equ_new_with_owned_bindings(const expr_t *lhs, const expr_t *rhs, expr_bindings_t *bindings)
{
    equation_t *equation;

    if (!lhs || !rhs)
        return NULL;

    equation = calloc(1u, sizeof(*equation));
    if (!equation)
        return NULL;

    equation->lhs = (expr_t *)lhs;
    equation->rhs = (expr_t *)rhs;
    equation->bindings = bindings;
    expr_retain(equation->lhs);
    expr_retain(equation->rhs);
    return equation;
}

equation_t *equ_new(const expr_t *lhs, const expr_t *rhs)
{
    return equ_new_with_owned_bindings(lhs, rhs, NULL);
}

void equ_free(equation_t *equation)
{
    if (!equation)
        return;
    string_free(equation->rhs_display_TeX);
    string_free(equation->lhs_display_TeX);
    expr_bindings_free(equation->bindings);
    expr_free(equation->lhs);
    expr_free(equation->rhs);
    free(equation);
}

int equ_set_display_TeX(equation_t *equation, const string_t *lhs, const string_t *rhs)
{
    string_t *lhs_copy = lhs ? string_clone(lhs) : NULL;
    string_t *rhs_copy = rhs ? string_clone(rhs) : NULL;

    if (!equation || (lhs && !lhs_copy) || (rhs && !rhs_copy)) {
        string_free(rhs_copy);
        string_free(lhs_copy);
        return -1;
    }
    string_free(equation->lhs_display_TeX);
    string_free(equation->rhs_display_TeX);
    equation->lhs_display_TeX = lhs_copy;
    equation->rhs_display_TeX = rhs_copy;
    return 0;
}

const string_t *equ_lhs_display_TeX(const equation_t *equation)
{
    return equation ? equation->lhs_display_TeX : NULL;
}

const string_t *equ_rhs_display_TeX(const equation_t *equation)
{
    return equation ? equation->rhs_display_TeX : NULL;
}

const expr_t *equ_lhs(const equation_t *equation)
{
    return equation ? equation->lhs : NULL;
}

const expr_t *equ_rhs(const equation_t *equation)
{
    return equation ? equation->rhs : NULL;
}

expr_bindings_t *equ_bindings(const equation_t *equation)
{
    return equation ? equation->bindings : NULL;
}

expr_bindings_t *equ_bindings_borrow(const equation_t *equation)
{
    return equ_bindings(equation);
}

expr_t *equ_binding(const equation_t *equation, const char *name)
{
    expr_bindings_t *bindings = equ_bindings(equation);

    if (!bindings || !name)
        return NULL;
    return expr_bindings_get(bindings, name);
}

expr_t *equ_residual(const equation_t *equation)
{
    expr_t *raw;
    expr_t *residual;

    if (!equation)
        return NULL;

    raw = expr_sub(equation->lhs, equation->rhs);
    residual = raw ? expr_simplify(raw) : NULL;
    expr_free(raw);
    return residual;
}

static bool equ_expr_uses_wrt(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    bool used = false;

    if (!expr || !wrt)
        return false;

    vars[0] = (expr_t *)wrt;
    return expr_collect_var_usage(expr, 1u, vars, &used) && used;
}

static bool equ_lhs_is_wrt(const equation_t *equation, const expr_t *wrt)
{
    expr_t *vars[1];
    size_t index = 0u;

    if (!equation || !wrt)
        return false;

    vars[0] = (expr_t *)wrt;
    return expr_match_var_expr(equation->lhs, 1u, vars, &index) && index == 0u;
}

bool equ_is_solved_for(const equation_t *equation, const expr_t *wrt)
{
    return equ_lhs_is_wrt(equation, wrt) && !equ_expr_uses_wrt(equation->rhs, wrt);
}

static int equ_solutions_append(equation_solutions_t *solutions, equation_t *solution)
{
    equation_t **items;

    if (!solutions || !solution)
        return -1;

    for (size_t i = 0u; i < solutions->count; ++i) {
        equation_t *existing = solutions->solutions[i];

        if (!existing)
            continue;
        if (!expr_struct_eq(equ_lhs(existing), equ_lhs(solution)))
            continue;
        if (!expr_struct_eq(equ_rhs(existing), equ_rhs(solution)))
            continue;

        equ_free(solution);
        return 0;
    }

    items = realloc(solutions->solutions, (solutions->count + 1u) * sizeof(*solutions->solutions));
    if (!items)
        return -1;

    solutions->solutions = items;
    solutions->solutions[solutions->count++] = solution;
    solutions->status = EQUATION_SOLVE_SOLVED;
    return 0;
}

static int equ_append_existing_solution(const equation_t *equation, equation_solutions_t *solutions)
{
    equation_t *solution = equ_new_with_owned_bindings(equation->lhs, equation->rhs, NULL);

    if (!solution)
        return -1;

    if (equ_solutions_append(solutions, solution) != 0) {
        equ_free(solution);
        return -1;
    }

    return 0;
}

static number_t *equ_snapshot_binding_values(expr_bindings_t *bindings);
static void equ_restore_binding_values(expr_bindings_t *bindings, number_t *values);
static void equ_free_binding_value_snapshot(expr_bindings_t *bindings, number_t *values);

int equ_append_solution_value(const expr_t *wrt, number_t value, equation_solutions_t *solutions)
{
    expr_t *rhs = expr_new_const(value);
    equation_t *solution = rhs ? equ_new(wrt, rhs) : NULL;
    int rc = -1;

    if (!solution)
        goto cleanup;

    if (equ_solutions_append(solutions, solution) != 0)
        goto cleanup;

    solution = NULL;
    rc = 0;

cleanup:
    equ_free(solution);
    expr_free(rhs);
    return rc;
}

int equ_append_solution_expr(const expr_t *wrt, const expr_t *rhs, equation_solutions_t *solutions)
{
    equation_t *solution = equ_new(wrt, rhs);

    if (!solution)
        return -1;

    if (equ_solutions_append(solutions, solution) != 0) {
        equ_free(solution);
        return -1;
    }

    return 0;
}

static size_t equ_variable_binding_count(expr_bindings_t *bindings)
{
    size_t count = 0u;
    size_t binding_count;

    if (!bindings)
        return 0u;

    binding_count = expr_bindings_count(bindings);
    for (size_t i = 0u; i < binding_count; ++i) {
        if (!expr_bindings_is_constant_at(bindings, i))
            ++count;
    }

    return count;
}

static int equ_merge_solutions(equation_solutions_t *dst, const equation_solutions_t *src)
{
    if (!dst || !src)
        return -1;

    for (size_t i = 0u; i < src->count; ++i) {
        const equation_t *solution = src->solutions[i];

        if (!solution)
            continue;
        if (equ_append_solution_expr(equ_lhs(solution), equ_rhs(solution), dst) != 0)
            return -1;
    }

    return 0;
}

static int equ_default_goal_seek_options(expr_goal_seek_options_t *options)
{
    size_t digits = num_get_default_prec_digits();

    if (!options)
        return -1;
    if (digits == 0u)
        digits = 64u;

    *options = (expr_goal_seek_options_t){.precision_digits = digits,
                                          .max_iterations = 0u,
                                          .allow_complex = true,
                                          .simplify_result = false,
                                          .tolerance = num_new()};
    return 0;
}

static int equ_derive_symbolic_solutions(const equation_t *equation, expr_bindings_t *bindings,
                                         equation_solutions_t *solutions)
{
    number_t *original_values = NULL;
    size_t binding_count;
    int rc = 0;

    if (!bindings)
        return 0;
    binding_count = expr_bindings_count(bindings);
    if (binding_count > 0u) {
        original_values = equ_snapshot_binding_values(bindings);
        if (!original_values)
            return -1;
    }

    for (size_t i = 0u; i < binding_count; ++i) {
        expr_t *expr = expr_bindings_expr_at(bindings, i);

        if (!expr_bindings_is_constant_at(bindings, i) && expr)
            expr_set_val(expr, NUM_NAN);
    }

    for (size_t i = 0u; i < binding_count; ++i) {
        expr_t *expr = expr_bindings_expr_at(bindings, i);
        equation_solutions_t partial;
        int solve_rc;

        if (expr_bindings_is_constant_at(bindings, i) || !expr)
            continue;

        equ_solutions_reset(&partial);
        solve_rc = equ_solve_for_into(equation, expr, &partial);
        if (solve_rc < 0) {
            equ_solutions_clear(&partial);
            rc = -1;
            goto cleanup;
        }
        if (partial.count > 0u && equ_merge_solutions(solutions, &partial) != 0)
            rc = -1;
        equ_solutions_clear(&partial);
        if (rc != 0)
            goto cleanup;
    }

cleanup:
    equ_restore_binding_values(bindings, original_values);
    equ_free_binding_value_snapshot(bindings, original_values);
    return rc;
}

static int equ_derive_without_bindings(const equation_t *equation, equation_solutions_t *solutions)
{
    const expr_t *lhs;

    if (!equation || !solutions)
        return -1;

    lhs = equ_lhs(equation);
    if (lhs && equ_is_solved_for(equation, lhs))
        return equ_append_existing_solution(equation, solutions);

    return 0;
}

static bool equ_expr_is_zero(const expr_t *expr);
static bool equ_expr_is_one(const expr_t *expr);
static expr_t *equ_symbolic_pi_expr(void);
static expr_t *equ_symbolic_two_pi_expr(void);
static expr_t *equ_exact_tan_sqrt_three_family_expr(void);

static int equ_try_solve_symbolic_affine(const expr_t *residual, const expr_t *wrt, equation_solutions_t *solutions);
static int equ_try_solve_unary_periodic(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions);
static int equ_try_solve_unary_inverse(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions);
static int equ_try_solve_atan_sum(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions);
static int equ_try_solve_self_power(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions);
static number_t equ_quadratic_discriminant(number_t constant, number_t linear, number_t quadratic);

typedef struct {
    const expr_t **nodes;
    size_t *powers;
    size_t count;
    size_t capacity;
    const expr_t *base;
    bool invalid;
} equ_repeated_power_terms_t;

static bool equ_positive_integer_coefficient(number_t coefficient, size_t *power_out)
{
    double approximate;
    number_t exact;
    bool ok;

    if (!power_out || !num_is_real(coefficient) || !num_is_integer(coefficient))
        return false;
    approximate = num_to_double(coefficient);
    if (!isfinite(approximate) || approximate < 1.0 || approximate > (double)LONG_MAX)
        return false;
    *power_out = (size_t)approximate;
    exact = num_create_from_long((long)*power_out);
    ok = num_eq(coefficient, exact);
    num_destroy(&exact);
    return ok;
}

static bool equ_repeated_power_term(const expr_t *expr, const expr_t *wrt, const expr_t **base_out, size_t *power_out)
{
    const expr_t *base = NULL;
    const expr_t *exponent = NULL;
    number_t constant = num_new();
    number_t coefficient = num_new();
    bool ok;

    ok = expr_match_pow_expr(expr, &base, &exponent) && base && exponent && !equ_expr_uses_wrt(base, wrt) &&
         equ_match_affine_linear_expr(exponent, wrt, true, &constant, &coefficient) && num_is_zero(constant) &&
         equ_positive_integer_coefficient(coefficient, power_out);
    if (ok)
        *base_out = base;
    num_destroy(&coefficient);
    num_destroy(&constant);
    return ok;
}

static void equ_collect_repeated_power_terms(const expr_t *expr, const expr_t *wrt, equ_repeated_power_terms_t *terms)
{
    const expr_t *base = NULL;
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    size_t power;

    if (!expr || !terms || terms->invalid)
        return;
    if (equ_repeated_power_term(expr, wrt, &base, &power)) {
        if (terms->base && !expr_struct_eq(terms->base, base)) {
            terms->invalid = true;
            return;
        }
        if (terms->count == terms->capacity) {
            size_t capacity = terms->capacity ? terms->capacity * 2u : 4u;
            const expr_t **nodes = realloc(terms->nodes, capacity * sizeof(*nodes));
            size_t *powers;

            if (!nodes) {
                terms->invalid = true;
                return;
            }
            terms->nodes = nodes;
            powers = realloc(terms->powers, capacity * sizeof(*powers));
            if (!powers) {
                terms->invalid = true;
                return;
            }
            terms->powers = powers;
            terms->capacity = capacity;
        }
        terms->base = base;
        terms->nodes[terms->count] = expr;
        terms->powers[terms->count++] = power;
        return;
    }
    if (expr_child_exprs(expr, &left, &right)) {
        equ_collect_repeated_power_terms(left, wrt, terms);
        equ_collect_repeated_power_terms(right, wrt, terms);
    }
}

static int equ_exactify_cubic_power_solutions(const expr_t *polynomial, const expr_t *power_variable,
                                              equation_solutions_t *power_solutions, number_t *real_root_out,
                                              number_t *quadratic_out, bool *exactified_out)
{
    number_t coefficients[4];
    number_t quadratic[3];
    number_t exact_root = num_new();
    bool found = false;
    int rc = 0;

    *exactified_out = false;

    for (size_t i = 0u; i < 4u; ++i)
        coefficients[i] = num_new();
    for (size_t i = 0u; i < 3u; ++i)
        quadratic[i] = num_new();

    if (!equ_match_polynomial_expr(polynomial, power_variable, 3u, coefficients) || num_is_zero(coefficients[3]))
        goto cleanup;
    for (size_t i = 0u; i < 4u; ++i)
        if (!num_is_exact(coefficients[i]))
            goto cleanup;

    for (size_t i = 0u; i < power_solutions->count; ++i) {
        number_t candidate = expr_eval(equ_rhs(power_solutions->solutions[i]));

        if (num_is_real(candidate)) {
            double approximate = num_to_double(candidate);

            if (isfinite(approximate) && approximate >= (double)LONG_MIN && approximate <= (double)LONG_MAX) {
                long rounded = lround(approximate);
                number_t integer = num_create_from_long(rounded);
                number_t value = num_clone(coefficients[3]);

                for (size_t degree = 3u; degree-- > 0u;) {
                    number_t product = num_mul(value, integer);
                    number_t next = num_add(product, coefficients[degree]);

                    num_destroy(&product);
                    num_destroy(&value);
                    value = next;
                }
                if (num_is_zero(value)) {
                    num_destroy(&exact_root);
                    exact_root = integer;
                    found = true;
                } else {
                    num_destroy(&integer);
                }
                num_destroy(&value);
            }
        }
        num_destroy(&candidate);
        if (found)
            break;
    }
    if (!found)
        goto cleanup;

    num_destroy(&quadratic[2]);
    quadratic[2] = num_clone(coefficients[3]);
    {
        number_t product = num_mul(exact_root, quadratic[2]);

        num_destroy(&quadratic[1]);
        quadratic[1] = num_add(coefficients[2], product);
        num_destroy(&product);
    }
    {
        number_t product = num_mul(exact_root, quadratic[1]);

        num_destroy(&quadratic[0]);
        quadratic[0] = num_add(coefficients[1], product);
        num_destroy(&product);
    }

    equ_solutions_clear(power_solutions);
    equ_solutions_reset(power_solutions);
    if (equ_append_solution_value(power_variable, exact_root, power_solutions) != 0 ||
        equ_solve_quadratic_coefficients(quadratic, power_variable, power_solutions) != 0)
        rc = -1;
    if (rc == 0) {
        num_destroy(real_root_out);
        *real_root_out = num_clone(exact_root);
        for (size_t i = 0u; i < 3u; ++i) {
            num_destroy(&quadratic_out[i]);
            quadratic_out[i] = num_clone(quadratic[i]);
        }
        *exactified_out = true;
    }

cleanup:
    num_destroy(&exact_root);
    for (size_t i = 0u; i < 3u; ++i)
        num_destroy(&quadratic[i]);
    for (size_t i = 0u; i < 4u; ++i)
        num_destroy(&coefficients[i]);
    return rc;
}

static expr_t *equ_logarithmic_power_family_root(const expr_t *base, const expr_t *power_root)
{
    expr_t *log_root = power_root ? expr_log(power_root) : NULL;
    expr_t *two_pi = equ_symbolic_two_pi_expr();
    expr_t *imaginary = expr_new_named_const(NUM_I, "i");
    expr_t *n = expr_new_named_var(NUM_NAN, "n");
    expr_t *imaginary_period = (two_pi && imaginary) ? expr_mul(two_pi, imaginary) : NULL;
    expr_t *period = (imaginary_period && n) ? expr_mul(imaginary_period, n) : NULL;
    expr_t *numerator = (log_root && period) ? expr_add(log_root, period) : NULL;
    expr_t *denominator = base ? expr_log(base) : NULL;
    expr_t *quotient = (numerator && denominator) ? expr_div(numerator, denominator) : NULL;
    expr_t *root = expr_simplify_owned(quotient);

    expr_free(denominator);
    expr_free(numerator);
    expr_free(period);
    expr_free(imaginary_period);
    expr_free(n);
    expr_free(imaginary);
    expr_free(two_pi);
    expr_free(log_root);
    return root;
}

static expr_t *equ_cartesian_power_family(const expr_t *base, const expr_t *power_root, const number_t *quadratic,
                                          int quadratic_sign)
{
    expr_t *log_base = base ? expr_log(base) : NULL;
    expr_t *real_part = NULL;
    expr_t *angle = NULL;
    expr_t *two_pi = equ_symbolic_two_pi_expr();
    expr_t *n = expr_new_named_var(NUM_NAN, "n");
    expr_t *period = (two_pi && n) ? expr_mul(two_pi, n) : NULL;
    expr_t *phase = NULL;
    expr_t *imaginary_part = NULL;
    expr_t *imaginary = expr_new_named_const(NUM_I, "i");
    expr_t *imaginary_term = NULL;
    expr_t *sum = NULL;
    expr_t *root = NULL;

    if (quadratic_sign == 0) {
        expr_t *log_root = power_root ? expr_log(power_root) : NULL;
        number_t root_value = power_root ? expr_eval(power_root) : num_new();
        number_t base_value = base ? expr_eval(base) : num_new();
        number_t log_root_value = num_log(root_value);
        number_t log_base_value = num_log(base_value);
        number_t quotient_value = num_div(log_root_value, log_base_value);

        real_part = num_is_integer(quotient_value) ? expr_new_const(quotient_value)
                                                   : (log_root && log_base
                                                          ? expr_simplify_owned(expr_div(log_root, log_base))
                                                          : NULL);
        num_destroy(&quotient_value);
        num_destroy(&log_base_value);
        num_destroy(&log_root_value);
        num_destroy(&base_value);
        num_destroy(&root_value);
        expr_free(log_root);
    } else {
        number_t modulus_squared = num_div(quadratic[0], quadratic[2]);
        number_t discriminant = equ_quadratic_discriminant(quadratic[0], quadratic[1], quadratic[2]);
        number_t radicand_value = num_neg(discriminant);
        number_t real_numerator_value = num_neg(quadratic[1]);
        number_t real_magnitude_value = num_abs(real_numerator_value);
        expr_t *modulus_squared_expr = expr_new_const(modulus_squared);
        expr_t *modulus = modulus_squared_expr ? expr_sqrt(modulus_squared_expr) : NULL;
        expr_t *log_modulus = modulus ? expr_log(modulus) : NULL;
        expr_t *radicand = expr_new_const(radicand_value);
        expr_t *surd = radicand ? expr_sqrt(radicand) : NULL;
        expr_t *real_magnitude = expr_new_const(real_magnitude_value);
        expr_t *ratio = (surd && real_magnitude) ? expr_div(surd, real_magnitude) : NULL;
        expr_t *acute = ratio ? expr_atan(ratio) : NULL;

        real_part = (log_modulus && log_base) ? expr_simplify_owned(expr_div(log_modulus, log_base)) : NULL;
        if (num_sign(real_numerator_value) < 0) {
            expr_t *pi = equ_symbolic_pi_expr();

            angle = (pi && acute) ? expr_sub(pi, acute) : NULL;
            expr_free(pi);
        } else {
            angle = expr_clone(acute);
        }
        if (quadratic_sign < 0) {
            expr_t *negative = angle ? expr_neg(angle) : NULL;

            expr_free(angle);
            angle = negative;
        }
        expr_free(acute);
        expr_free(ratio);
        expr_free(real_magnitude);
        expr_free(surd);
        expr_free(radicand);
        expr_free(log_modulus);
        expr_free(modulus);
        expr_free(modulus_squared_expr);
        num_destroy(&real_magnitude_value);
        num_destroy(&real_numerator_value);
        num_destroy(&radicand_value);
        num_destroy(&discriminant);
        num_destroy(&modulus_squared);
    }

    phase = quadratic_sign == 0 ? expr_clone(period) : (angle && period ? expr_add(angle, period) : NULL);
    imaginary_part = (phase && log_base) ? expr_simplify_owned(expr_div(phase, log_base)) : NULL;
    imaginary_term = (imaginary && imaginary_part) ? expr_mul(imaginary, imaginary_part) : NULL;
    sum = (real_part && imaginary_term) ? expr_add(real_part, imaginary_term) : NULL;
    root = sum;
    sum = NULL;

    expr_free(imaginary_term);
    expr_free(imaginary);
    expr_free(imaginary_part);
    expr_free(phase);
    expr_free(period);
    expr_free(n);
    expr_free(two_pi);
    expr_free(angle);
    expr_free(real_part);
    expr_free(log_base);
    return root;
}

static int equ_try_solve_repeated_power_polynomial(const equation_t *equation, const expr_t *wrt,
                                                   equation_solutions_t *solutions)
{
    equ_repeated_power_terms_t terms = {0};
    equation_solutions_t power_solutions;
    expr_t *residual = NULL;
    expr_t *transformed = NULL;
    expr_t *power_variable = NULL;
    expr_t *zero = NULL;
    equation_t *reduced = NULL;
    number_t exact_real_root = num_new();
    number_t exact_quadratic[3] = {num_new(), num_new(), num_new()};
    bool exactified = false;
    int rc = 1;

    residual = equ_residual(equation);
    if (!residual)
        goto cleanup;
    equ_collect_repeated_power_terms(residual, wrt, &terms);
    if (terms.invalid || terms.count < 2u)
        goto cleanup;

    power_variable = expr_new_named_var(NUM_NAN, "power_value");
    transformed = expr_clone(residual);
    if (!power_variable || !transformed) {
        rc = -1;
        goto cleanup;
    }
    for (size_t i = 0u; i < terms.count; ++i) {
        equ_repeated_power_terms_t current = {0};
        expr_t *replacement;
        expr_t *next;

        equ_collect_repeated_power_terms(transformed, wrt, &current);
        if (current.invalid || current.count == 0u) {
            free(current.powers);
            free(current.nodes);
            rc = -1;
            goto cleanup;
        }
        replacement = current.powers[0] == 1u ? expr_clone(power_variable)
                                              : expr_pow_long(power_variable, (long)current.powers[0]);
        next = replacement ? expr_substitute(transformed, current.nodes[0], replacement) : NULL;

        expr_free(replacement);
        free(current.powers);
        free(current.nodes);
        if (!next) {
            rc = -1;
            goto cleanup;
        }
        expr_free(transformed);
        transformed = next;
    }

    zero = expr_const_zero();
    reduced = zero ? equ_new(transformed, zero) : NULL;
    if (!reduced) {
        rc = -1;
        goto cleanup;
    }
    equ_solutions_reset(&power_solutions);
    rc = equ_solve_for_into(reduced, power_variable, &power_solutions);
    if (rc != 0 || power_solutions.count == 0u) {
        equ_solutions_clear(&power_solutions);
        rc = rc < 0 ? -1 : 1;
        goto cleanup;
    }
    {
        expr_t *reduced_polynomial = equ_residual(reduced);

        if (!reduced_polynomial ||
            equ_exactify_cubic_power_solutions(reduced_polynomial, power_variable, &power_solutions, &exact_real_root,
                                               exact_quadratic, &exactified) != 0) {
            expr_free(reduced_polynomial);
            equ_solutions_clear(&power_solutions);
            rc = -1;
            goto cleanup;
        }
        expr_free(reduced_polynomial);
    }

    for (size_t i = 0u; i < power_solutions.count; ++i) {
        const expr_t *power_root = equ_rhs(power_solutions.solutions[i]);
        number_t value = expr_eval(power_root);
        int quadratic_sign = exactified && i > 0u ? (i == 1u ? 1 : -1) : 0;
        expr_t *family = !num_is_zero(value)
                             ? (exactified ? equ_cartesian_power_family(terms.base, power_root, exact_quadratic,
                                                                        quadratic_sign)
                                           : equ_logarithmic_power_family_root(terms.base, power_root))
                             : NULL;

        if (!family || equ_append_solution_expr(wrt, family, solutions) != 0)
            rc = -1;
        expr_free(family);
        num_destroy(&value);
        if (rc < 0)
            break;
    }
    equ_solutions_clear(&power_solutions);
    if (rc >= 0)
        rc = solutions->count > 0u ? 0 : 1;

cleanup:
    for (size_t i = 0u; i < 3u; ++i)
        num_destroy(&exact_quadratic[i]);
    num_destroy(&exact_real_root);
    equ_free(reduced);
    expr_free(zero);
    expr_free(power_variable);
    expr_free(transformed);
    expr_free(residual);
    free(terms.powers);
    free(terms.nodes);
    return rc;
}

static int equ_try_solve_affine(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions)
{
    expr_t *residual = equ_residual(equation);
    number_t constant = num_new();
    number_t coeff = num_new();
    number_t neg_constant;
    number_t solution_value;
    bool ok;
    int rc = -1;

    if (!residual)
        goto cleanup;

    ok = equ_match_affine_linear_expr(residual, wrt, true, &constant, &coeff);
    if (!ok) {
        rc = equ_try_solve_symbolic_affine(residual, wrt, solutions);
        goto cleanup;
    }

    neg_constant = num_neg(constant);
    solution_value = num_div(neg_constant, coeff);
    if (equ_append_solution_value(wrt, solution_value, solutions) != 0) {
        num_destroy(&solution_value);
        num_destroy(&neg_constant);
        goto cleanup;
    }

    num_destroy(&solution_value);
    num_destroy(&neg_constant);
    rc = 0;

cleanup:
    num_destroy(&coeff);
    num_destroy(&constant);
    expr_free(residual);
    return rc;
}

static number_t equ_quadratic_discriminant(number_t constant, number_t linear, number_t quadratic)
{
    number_t linear_sq = num_mul(linear, linear);
    number_t quad_constant = num_mul(quadratic, constant);
    number_t four_quad_constant = num_mul_long(quad_constant, 4L);
    number_t discriminant = num_sub(linear_sq, four_quad_constant);

    num_destroy(&four_quad_constant);
    num_destroy(&quad_constant);
    num_destroy(&linear_sq);
    return discriminant;
}

static number_t equ_quadratic_root(number_t neg_linear, number_t signed_sqrt_discriminant, number_t denominator)
{
    number_t numerator = num_add(neg_linear, signed_sqrt_discriminant);
    number_t root = num_div(numerator, denominator);

    num_destroy(&numerator);
    return root;
}

static expr_t *equ_quadratic_surd_term(number_t discriminant)
{
    number_t radicand_value = num_sign(discriminant) < 0 ? num_neg(discriminant) : num_clone(discriminant);
    expr_t *radicand = expr_new_const(radicand_value);
    expr_t *root = radicand ? expr_sqrt(radicand) : NULL;
    expr_t *imaginary_unit = num_sign(discriminant) < 0 ? expr_new_const(NUM_I) : NULL;
    expr_t *term = imaginary_unit && root ? expr_mul(imaginary_unit, root) : root ? expr_clone(root) : NULL;

    expr_free(imaginary_unit);
    expr_free(root);
    expr_free(radicand);
    num_destroy(&radicand_value);
    return term;
}

static expr_t *equ_quadratic_surd_root(number_t neg_linear, number_t discriminant, number_t denominator,
                                       bool positive)
{
    expr_t *linear = expr_new_const(neg_linear);
    expr_t *surd = equ_quadratic_surd_term(discriminant);
    expr_t *numerator = linear && surd ? (positive ? expr_add(linear, surd) : expr_sub(linear, surd)) : NULL;
    expr_t *denominator_expr = expr_new_const(denominator);
    expr_t *root = numerator && denominator_expr ? expr_div(numerator, denominator_expr) : NULL;

    expr_free(denominator_expr);
    expr_free(numerator);
    expr_free(surd);
    expr_free(linear);
    return root;
}

int equ_solve_quadratic_coefficients(const number_t *coeffs, const expr_t *wrt, equation_solutions_t *solutions)
{
    number_t discriminant;
    number_t sqrt_discriminant;
    number_t neg_sqrt_discriminant;
    number_t neg_linear;
    number_t denominator;
    number_t root;

    if (!coeffs || !wrt || !solutions || num_is_zero(coeffs[2]))
        return -1;

    discriminant = equ_quadratic_discriminant(coeffs[0], coeffs[1], coeffs[2]);
    sqrt_discriminant = num_sqrt(discriminant);
    neg_linear = num_neg(coeffs[1]);
    denominator = num_mul_long(coeffs[2], 2L);

    if (num_is_exact(coeffs[0]) && num_is_exact(coeffs[1]) && num_is_exact(coeffs[2]) &&
        num_is_real(discriminant) && !num_is_zero(sqrt_discriminant) && !num_is_exact(sqrt_discriminant)) {
        expr_t *positive_root = equ_quadratic_surd_root(neg_linear, discriminant, denominator, true);
        expr_t *negative_root = equ_quadratic_surd_root(neg_linear, discriminant, denominator, false);

        if (!positive_root || !negative_root || equ_append_solution_expr(wrt, positive_root, solutions) != 0 ||
            equ_append_solution_expr(wrt, negative_root, solutions) != 0) {
            expr_free(negative_root);
            expr_free(positive_root);
            goto error;
        }
        expr_free(negative_root);
        expr_free(positive_root);
        num_destroy(&denominator);
        num_destroy(&neg_linear);
        num_destroy(&sqrt_discriminant);
        num_destroy(&discriminant);
        return 0;
    }

    root = equ_quadratic_root(neg_linear, sqrt_discriminant, denominator);
    if (equ_append_solution_value(wrt, root, solutions) != 0) {
        num_destroy(&root);
        goto error;
    }
    num_destroy(&root);

    if (!num_is_zero(sqrt_discriminant)) {
        neg_sqrt_discriminant = num_neg(sqrt_discriminant);
        root = equ_quadratic_root(neg_linear, neg_sqrt_discriminant, denominator);
        num_destroy(&neg_sqrt_discriminant);
        if (equ_append_solution_value(wrt, root, solutions) != 0) {
            num_destroy(&root);
            goto error;
        }
        num_destroy(&root);
    }

    num_destroy(&denominator);
    num_destroy(&neg_linear);
    num_destroy(&sqrt_discriminant);
    num_destroy(&discriminant);
    return 0;

error:
    equ_solutions_clear(solutions);
    num_destroy(&denominator);
    num_destroy(&neg_linear);
    num_destroy(&sqrt_discriminant);
    num_destroy(&discriminant);
    return -1;
}

static expr_t *equ_symbolic_pi_expr(void)
{
    number_t pi_value = num_clone(NUM_PI);
    expr_t *pi = expr_new_named_const(pi_value, "@pi");

    num_destroy(&pi_value);
    return pi;
}

static expr_t *equ_symbolic_two_pi_expr(void)
{
    number_t two_value = num_create_from_long(2L);
    expr_t *two = expr_new_const(two_value);
    expr_t *pi = equ_symbolic_pi_expr();
    expr_t *out = (two && pi) ? expr_mul(two, pi) : NULL;

    expr_free(pi);
    expr_free(two);
    num_destroy(&two_value);
    return out;
}

static expr_t *equ_exact_sin_one_family_expr(void)
{
    number_t two_value = num_create_from_long(2L);
    number_t four_value = num_create_from_long(4L);
    number_t one_value = num_create_from_long(1L);
    expr_t *pi = equ_symbolic_pi_expr();
    expr_t *two = expr_new_const(two_value);
    expr_t *four = expr_new_const(four_value);
    expr_t *one = expr_new_const(one_value);
    expr_t *n = expr_new_named_var(NUM_NAN, "n");
    expr_t *pi_over_two = (pi && two) ? expr_div(pi, two) : NULL;
    expr_t *four_n = (four && n) ? expr_mul(four, n) : NULL;
    expr_t *affine = (four_n && one) ? expr_add(four_n, one) : NULL;
    expr_t *out = (pi_over_two && affine) ? expr_mul(pi_over_two, affine) : NULL;

    expr_free(affine);
    expr_free(four_n);
    expr_free(pi_over_two);
    expr_free(n);
    expr_free(one);
    expr_free(four);
    expr_free(two);
    expr_free(pi);
    num_destroy(&one_value);
    num_destroy(&four_value);
    num_destroy(&two_value);
    return out;
}

static expr_t *equ_exact_tan_sqrt_three_family_expr(void)
{
    number_t one_value = num_create_from_long(1L);
    number_t three_value = num_create_from_long(3L);
    number_t one_third_value = num_div(one_value, three_value);
    expr_t *one = expr_new_const(one_value);
    expr_t *three = expr_new_const(three_value);
    expr_t *pi = equ_symbolic_pi_expr();
    expr_t *n = expr_new_named_var(NUM_NAN, "n");
    expr_t *one_third = expr_new_const(one_third_value);
    expr_t *three_n = (three && n) ? expr_mul(three, n) : NULL;
    expr_t *affine = (three_n && one) ? expr_add(three_n, one) : NULL;
    expr_t *pi_affine = (pi && affine) ? expr_mul(pi, affine) : NULL;
    expr_t *out = (one_third && pi_affine) ? expr_mul(one_third, pi_affine) : NULL;

    if (out)
        expr_set_binding_pi_linear_family(out, 3L, 3L, 1L);

    expr_free(pi_affine);
    expr_free(affine);
    expr_free(three_n);
    expr_free(one_third);
    expr_free(n);
    expr_free(pi);
    expr_free(three);
    expr_free(one);
    num_destroy(&one_third_value);
    num_destroy(&three_value);
    num_destroy(&one_value);
    return out;
}

static expr_t *equ_symbolic_linear_root(const expr_t *constant, const expr_t *linear)
{
    expr_t *neg_constant = expr_neg(constant);
    expr_t *quotient = neg_constant ? expr_div(neg_constant, linear) : NULL;
    expr_t *root = expr_simplify_owned(quotient);

    expr_free(neg_constant);
    return root;
}

static expr_t *equ_symbolic_linear_phase_root(const expr_t *constant, const expr_t *linear, const expr_t *phase)
{
    if (equ_expr_is_zero(constant) && equ_expr_is_one(linear)) {
        expr_t *copy = (expr_t *)phase;

        expr_retain(copy);
        return expr_simplify_owned(copy);
    }

    expr_t *shifted_constant = (constant && phase) ? expr_sub(constant, phase) : NULL;
    expr_t *root = shifted_constant ? equ_symbolic_linear_root(shifted_constant, linear) : NULL;

    expr_free(shifted_constant);
    return root;
}

static expr_t *equ_periodic_family_expr(const expr_t *base, const expr_t *period, const expr_t *n)
{
    expr_t *period_term = (period && n) ? expr_mul(period, n) : NULL;
    expr_t *sum = (base && period_term) ? expr_add(base, period_term) : NULL;
    expr_t *out = expr_simplify_owned(sum);

    expr_free(period_term);
    return out;
}

static expr_t *equ_periodic_sub_family_expr(const expr_t *offset, const expr_t *period, const expr_t *n,
                                            const expr_t *subtrahend)
{
    expr_t *period_term = (period && n) ? expr_mul(period, n) : NULL;
    expr_t *offset_sum = (offset && period_term) ? expr_add(offset, period_term) : NULL;
    expr_t *difference = (offset_sum && subtrahend) ? expr_sub(offset_sum, subtrahend) : NULL;
    expr_t *out = expr_simplify_owned(difference);

    expr_free(offset_sum);
    expr_free(period_term);
    return out;
}

static int equ_append_trig_family_root(const expr_t *wrt, const expr_t *constant, const expr_t *linear,
                                       const expr_t *base, const expr_t *period, const expr_t *n,
                                       equation_solutions_t *solutions)
{
    expr_t *family = equ_periodic_family_expr(base, period, n);
    expr_t *root = family ? equ_symbolic_linear_phase_root(constant, linear, family) : NULL;
    int rc = -1;

    if (!root)
        goto cleanup;
    if (equ_append_solution_expr(wrt, root, solutions) != 0)
        goto cleanup;

    rc = 0;

cleanup:
    expr_free(root);
    expr_free(family);
    return rc;
}

typedef enum { EQU_PERIODIC_TRIG_SIN, EQU_PERIODIC_TRIG_COS, EQU_PERIODIC_TRIG_TAN } equ_periodic_trig_kind_t;

static int equ_try_solve_periodic_trig_kind(equ_periodic_trig_kind_t kind, const expr_t *inner, const expr_t *target,
                                            const expr_t *wrt, equation_solutions_t *solutions)
{
    expr_t *constant = NULL;
    expr_t *linear = NULL;
    expr_t *n = NULL;
    expr_t *base = NULL;
    expr_t *alt_base = NULL;
    expr_t *period = NULL;
    expr_t *pi = NULL;
    expr_t *neg_base = NULL;
    expr_t *exact_family = NULL;
    expr_t *exact_root = NULL;
    int rc = -1;

    if (!inner || !target || !wrt || !solutions)
        return -1;
    if (!equ_match_symbolic_linear_expr(inner, wrt, &constant, &linear)) {
        rc = 1;
        goto cleanup;
    }

    n = expr_new_named_var(NUM_NAN, "n");
    if (!n)
        goto cleanup;

    switch (kind) {
        case EQU_PERIODIC_TRIG_SIN:
            if (equ_expr_is_one(target)) {
                exact_family = equ_exact_sin_one_family_expr();
                if (!exact_family)
                    goto cleanup;
                if (equ_expr_is_zero(constant) && equ_expr_is_one(linear)) {
                    rc = equ_append_solution_expr(wrt, exact_family, solutions);
                    break;
                }
                exact_root = equ_symbolic_linear_phase_root(constant, linear, exact_family);
                if (!exact_root)
                    goto cleanup;
                rc = equ_append_solution_expr(wrt, exact_root, solutions);
                break;
            }
            base = expr_simplify_owned(expr_asin(target));
            period = equ_symbolic_two_pi_expr();
            if (!base || !period)
                goto cleanup;
            rc = equ_append_trig_family_root(wrt, constant, linear, base, period, n, solutions);
            if (rc != 0)
                goto cleanup;
            pi = equ_symbolic_pi_expr();
            alt_base = equ_periodic_sub_family_expr(pi, period, n, base);
            if (!alt_base)
                goto cleanup;
            exact_root = equ_symbolic_linear_phase_root(constant, linear, alt_base);
            if (!exact_root) {
                rc = -1;
                goto cleanup;
            }
            if (equ_append_solution_expr(wrt, exact_root, solutions) != 0) {
                equ_solutions_clear(solutions);
                rc = -1;
                goto cleanup;
            }
            rc = 0;
            break;

        case EQU_PERIODIC_TRIG_COS:
            base = expr_simplify_owned(expr_acos(target));
            period = equ_symbolic_two_pi_expr();
            if (!base || !period)
                goto cleanup;
            rc = equ_append_trig_family_root(wrt, constant, linear, base, period, n, solutions);
            if (rc != 0)
                goto cleanup;
            neg_base = base ? expr_neg(base) : NULL;
            alt_base = expr_simplify_owned(neg_base);
            neg_base = NULL;
            if (!alt_base)
                goto cleanup;
            if (equ_append_trig_family_root(wrt, constant, linear, alt_base, period, n, solutions) != 0) {
                equ_solutions_clear(solutions);
                rc = -1;
                goto cleanup;
            }
            rc = 0;
            break;

        case EQU_PERIODIC_TRIG_TAN: {
            number_t target_value = num_new();
            bool is_sqrt_three = expr_match_const_value(target, &target_value) && num_eq(target_value, NUM_SQRT3);

            num_destroy(&target_value);
            if (is_sqrt_three) {
                exact_family = equ_exact_tan_sqrt_three_family_expr();
                if (!exact_family)
                    goto cleanup;
                if (equ_expr_is_zero(constant) && equ_expr_is_one(linear)) {
                    rc = equ_append_solution_expr(wrt, exact_family, solutions);
                    break;
                }
                exact_root = equ_symbolic_linear_phase_root(constant, linear, exact_family);
                if (!exact_root)
                    goto cleanup;
                rc = equ_append_solution_expr(wrt, exact_root, solutions);
                break;
            }
            base = expr_simplify_owned(expr_atan(target));
            period = equ_symbolic_pi_expr();
            if (!base || !period)
                goto cleanup;
            rc = equ_append_trig_family_root(wrt, constant, linear, base, period, n, solutions);
            break;
        }

        default:
            rc = 1;
            break;
    }

cleanup:
    expr_free(exact_root);
    expr_free(exact_family);
    expr_free(neg_base);
    expr_free(pi);
    expr_free(period);
    expr_free(alt_base);
    expr_free(base);
    expr_free(n);
    expr_free(linear);
    expr_free(constant);
    return rc;
}

static int equ_try_solve_unary_periodic_side(const expr_t *lhs, const expr_t *rhs, const expr_t *wrt,
                                             equation_solutions_t *solutions)
{
    const expr_t *inner = NULL;

    if (!lhs || !rhs || !wrt || !solutions)
        return -1;
    if (!equ_expr_uses_wrt(lhs, wrt) || equ_expr_uses_wrt(rhs, wrt))
        return 1;

    if (expr_match_sin_expr(lhs, &inner))
        return equ_try_solve_periodic_trig_kind(EQU_PERIODIC_TRIG_SIN, inner, rhs, wrt, solutions);
    if (expr_match_cos_expr(lhs, &inner))
        return equ_try_solve_periodic_trig_kind(EQU_PERIODIC_TRIG_COS, inner, rhs, wrt, solutions);
    if (expr_match_tan_expr(lhs, &inner))
        return equ_try_solve_periodic_trig_kind(EQU_PERIODIC_TRIG_TAN, inner, rhs, wrt, solutions);

    return 1;
}

static int equ_try_solve_unary_periodic(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions)
{
    int rc;

    if (!equation || !wrt || !solutions)
        return -1;

    rc = equ_try_solve_unary_periodic_side(equation->lhs, equation->rhs, wrt, solutions);
    if (rc != 1)
        return rc;
    return equ_try_solve_unary_periodic_side(equation->rhs, equation->lhs, wrt, solutions);
}

typedef enum { EQU_UNARY_INVERSE_EXP, EQU_UNARY_INVERSE_LOG } equ_unary_inverse_kind_t;

static expr_t *equ_unary_inverse_rhs(equ_unary_inverse_kind_t kind, const expr_t *rhs)
{
    switch (kind) {
        case EQU_UNARY_INVERSE_EXP:
            return rhs ? expr_simplify_owned(expr_log(rhs)) : NULL;
        case EQU_UNARY_INVERSE_LOG:
            return rhs ? expr_simplify_owned(expr_exp(rhs)) : NULL;
        default:
            return NULL;
    }
}

static int equ_try_solve_unary_inverse_side(const expr_t *lhs, const expr_t *rhs, const expr_t *wrt,
                                            equation_solutions_t *solutions)
{
    const expr_t *inner = NULL;
    expr_t *inverse_rhs = NULL;
    equation_t *reduced = NULL;
    int rc = 1;

    if (!lhs || !rhs || !wrt || !solutions)
        return -1;
    if (!equ_expr_uses_wrt(lhs, wrt) || equ_expr_uses_wrt(rhs, wrt))
        return 1;

    if (expr_match_exp_expr(lhs, &inner))
        inverse_rhs = equ_unary_inverse_rhs(EQU_UNARY_INVERSE_EXP, rhs);
    else if (expr_match_log_expr(lhs, &inner))
        inverse_rhs = equ_unary_inverse_rhs(EQU_UNARY_INVERSE_LOG, rhs);
    else
        return 1;

    if (!inverse_rhs)
        return -1;

    reduced = equ_new(inner, inverse_rhs);
    if (!reduced) {
        rc = -1;
        goto cleanup;
    }

    rc = equ_solve_for_into(reduced, wrt, solutions);

cleanup:
    equ_free(reduced);
    expr_free(inverse_rhs);
    return rc;
}

static int equ_try_solve_unary_inverse(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions)
{
    int rc;

    if (!equation || !wrt || !solutions)
        return -1;

    rc = equ_try_solve_unary_inverse_side(equation->lhs, equation->rhs, wrt, solutions);
    if (rc != 1)
        return rc;
    return equ_try_solve_unary_inverse_side(equation->rhs, equation->lhs, wrt, solutions);
}

static bool equ_expr_is_self_power(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *base = NULL;
    const expr_t *exponent = NULL;

    return expr_match_pow_expr(expr, &base, &exponent) && expr_struct_eq(base, wrt) && expr_struct_eq(exponent, wrt);
}

static expr_t *equ_self_power_log_family_arg(const expr_t *rhs)
{
    number_t i_value = num_clone(NUM_I);
    expr_t *log_rhs = rhs ? expr_log(rhs) : NULL;
    expr_t *two_pi = equ_symbolic_two_pi_expr();
    expr_t *i_const = expr_new_named_const(i_value, "i");
    expr_t *n = expr_new_named_var(NUM_NAN, "n");
    expr_t *two_pi_i = (two_pi && i_const) ? expr_mul(two_pi, i_const) : NULL;
    expr_t *period_term = (two_pi_i && n) ? expr_mul(two_pi_i, n) : NULL;
    expr_t *sum = (log_rhs && period_term) ? expr_add(log_rhs, period_term) : NULL;
    expr_t *out = expr_simplify_owned(sum);

    expr_free(period_term);
    expr_free(two_pi_i);
    expr_free(n);
    expr_free(i_const);
    expr_free(two_pi);
    expr_free(log_rhs);
    num_destroy(&i_value);
    return out;
}

static expr_t *equ_self_power_lambert_family_root(const expr_t *rhs)
{
    expr_t *arg = equ_self_power_log_family_arg(rhs);
    expr_t *k = expr_new_named_var(NUM_NAN, "k");
    expr_t *lambert = (arg && k) ? expr_lambert_wn(k, arg) : NULL;
    expr_t *root = lambert ? expr_exp(lambert) : NULL;
    expr_t *out = expr_simplify_owned(root);

    expr_free(lambert);
    expr_free(k);
    expr_free(arg);
    return out;
}

static bool equ_candidate_satisfies_equation(const expr_t *lhs, const expr_t *rhs, const expr_t *wrt,
                                             const expr_t *candidate)
{
    expr_t *lhs_at = NULL;
    expr_t *rhs_at = NULL;
    expr_t *raw_residual = NULL;
    expr_t *residual = NULL;
    number_t value = NUM_ZERO;
    number_t magnitude = NUM_ZERO;
    number_t rhs_value = NUM_ZERO;
    number_t rhs_scale = NUM_ZERO;
    number_t tolerance = NUM_ZERO;
    number_t scaled_tolerance = NUM_ZERO;
    bool ok = true;

    if (!lhs || !rhs || !wrt || !candidate)
        return false;

    lhs_at = expr_substitute(lhs, wrt, candidate);
    rhs_at = expr_substitute(rhs, wrt, candidate);
    raw_residual = (lhs_at && rhs_at) ? expr_sub(lhs_at, rhs_at) : NULL;
    residual = raw_residual ? expr_simplify_owned(raw_residual) : NULL;
    raw_residual = NULL;
    if (!residual) {
        ok = false;
        goto cleanup;
    }

    value = expr_eval(residual);
    if (!num_is_finite(value))
        goto cleanup;

    magnitude = num_abs(value);
    rhs_value = expr_eval(rhs_at);
    rhs_scale = num_abs(rhs_value);
    if (!num_is_finite(rhs_scale) || num_lt(rhs_scale, NUM_ONE)) {
        num_destroy(&rhs_scale);
        rhs_scale = num_clone(NUM_ONE);
    }

    tolerance = num_create_from_string("1e-24");
    scaled_tolerance = num_mul(tolerance, rhs_scale);
    ok = num_is_finite(magnitude) && num_le(magnitude, scaled_tolerance);

cleanup:
    num_destroy(&scaled_tolerance);
    num_destroy(&tolerance);
    num_destroy(&rhs_scale);
    num_destroy(&rhs_value);
    num_destroy(&magnitude);
    num_destroy(&value);
    expr_free(residual);
    expr_free(raw_residual);
    expr_free(rhs_at);
    expr_free(lhs_at);
    return ok;
}

static int equ_append_self_power_root_if_valid(const expr_t *lhs, const expr_t *rhs, const expr_t *wrt,
                                               equation_solutions_t *solutions, expr_t *root, bool *appended_out)
{
    int rc = 0;

    if (appended_out)
        *appended_out = false;

    if (!root)
        return -1;

    if (equ_candidate_satisfies_equation(lhs, rhs, wrt, root)) {
        if (equ_append_solution_expr(wrt, root, solutions) != 0)
            rc = -1;
        else if (appended_out)
            *appended_out = true;
    }

    return rc;
}

static int equ_try_solve_self_power_side(const expr_t *lhs, const expr_t *rhs, const expr_t *wrt,
                                         equation_solutions_t *solutions)
{
    expr_t *family_root = NULL;
    bool appended;
    bool saw_solution = false;

    if (!lhs || !rhs || !wrt || !solutions)
        return -1;
    if (!equ_expr_uses_wrt(lhs, wrt) || equ_expr_uses_wrt(rhs, wrt))
        return 1;
    if (!equ_expr_is_self_power(lhs, wrt))
        return 1;

    family_root = equ_self_power_lambert_family_root(rhs);
    if (equ_append_self_power_root_if_valid(lhs, rhs, wrt, solutions, family_root, &appended) != 0) {
        expr_free(family_root);
        return -1;
    }
    saw_solution = saw_solution || appended;
    expr_free(family_root);

    return saw_solution ? 0 : 1;
}

static int equ_try_solve_self_power(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions)
{
    int rc;

    if (!equation || !wrt || !solutions)
        return -1;

    rc = equ_try_solve_self_power_side(equation->lhs, equation->rhs, wrt, solutions);
    if (rc != 1)
        return rc;
    return equ_try_solve_self_power_side(equation->rhs, equation->lhs, wrt, solutions);
}

static int equ_try_solve_symbolic_affine(const expr_t *residual, const expr_t *wrt, equation_solutions_t *solutions)
{
    expr_t *constant = NULL;
    expr_t *linear = NULL;
    expr_t *root = NULL;
    int rc = -1;

    if (!equ_match_symbolic_linear_expr(residual, wrt, &constant, &linear)) {
        rc = 1;
        goto cleanup;
    }

    root = equ_symbolic_linear_root(constant, linear);
    if (!root)
        goto cleanup;

    if (equ_append_solution_expr(wrt, root, solutions) != 0)
        goto cleanup;

    rc = 0;

cleanup:
    expr_free(root);
    expr_free(linear);
    expr_free(constant);
    return rc;
}

static expr_t *equ_symbolic_quadratic_discriminant(const expr_t *constant, const expr_t *linear,
                                                   const expr_t *quadratic)
{
    number_t four = num_create_from_long(4L);
    expr_t *linear_sq = expr_pow(linear, &NUM_TWO);
    expr_t *quad_constant = expr_mul(quadratic, constant);
    expr_t *four_quad_constant = quad_constant ? expr_mul_num(quad_constant, &four) : NULL;
    expr_t *discriminant = (linear_sq && four_quad_constant) ? expr_sub(linear_sq, four_quad_constant) : NULL;
    expr_t *out = expr_simplify_owned(discriminant);

    expr_free(four_quad_constant);
    expr_free(quad_constant);
    expr_free(linear_sq);
    num_destroy(&four);
    return out;
}

static expr_t *equ_symbolic_quadratic_root(const expr_t *linear, const expr_t *quadratic,
                                           const expr_t *sqrt_discriminant, bool add_root)
{
    number_t two = num_create_from_long(2L);
    expr_t *neg_linear = expr_neg(linear);
    expr_t *numerator = add_root ? ((neg_linear && sqrt_discriminant) ? expr_add(neg_linear, sqrt_discriminant) : NULL)
                                 : ((neg_linear && sqrt_discriminant) ? expr_sub(neg_linear, sqrt_discriminant) : NULL);
    expr_t *denominator = expr_mul_num(quadratic, &two);
    expr_t *quotient = (numerator && denominator) ? expr_div(numerator, denominator) : NULL;
    expr_t *root = expr_simplify_owned(quotient);

    expr_free(denominator);
    expr_free(numerator);
    expr_free(neg_linear);
    num_destroy(&two);
    return root;
}

static bool equ_expr_simplifies_equal(const expr_t *left, const expr_t *right)
{
    expr_t *diff = (left && right) ? expr_sub(left, right) : NULL;
    expr_t *simplified = diff ? expr_simplify(diff) : NULL;
    bool equal = simplified && expr_is_exact_zero(simplified);

    expr_free(simplified);
    expr_free(diff);
    return equal;
}

static bool equ_expr_text_equal(const expr_t *left, const expr_t *right)
{
    string_t *left_text = left ? expr_to_text(left, style_EXPRESSION) : NULL;
    string_t *right_text = right ? expr_to_text(right, style_EXPRESSION) : NULL;
    bool equal = left_text && right_text && strcmp(string_c_str(left_text), string_c_str(right_text)) == 0;

    string_free(right_text);
    string_free(left_text);
    return equal;
}

static bool equ_expr_matches_expected(const expr_t *expr, const expr_t *expected)
{
    return equ_expr_text_equal(expr, expected) || equ_expr_simplifies_equal(expr, expected);
}

static bool equ_expr_is_zero(const expr_t *expr)
{
    number_t value = num_new();
    bool ok = expr_match_const_value(expr, &value) && num_eq(value, NUM_ZERO);

    num_destroy(&value);
    return ok;
}

static bool equ_expr_is_one(const expr_t *expr)
{
    number_t value = num_new();
    bool ok = expr_match_const_value(expr, &value) && num_eq(value, NUM_ONE);

    num_destroy(&value);
    return ok;
}

static expr_t *equ_symbolic_negated_sum(const expr_t *left, const expr_t *right)
{
    expr_t *neg_left = left ? expr_neg(left) : NULL;
    expr_t *neg_right = right ? expr_neg(right) : NULL;
    expr_t *sum = (neg_left && neg_right) ? expr_add(neg_left, neg_right) : NULL;
    expr_t *out = expr_simplify_owned(sum);

    expr_free(neg_right);
    expr_free(neg_left);
    return out;
}

static int equ_try_solve_symbolic_quadratic_product_roots(const expr_t *constant, const expr_t *linear,
                                                          const expr_t *quadratic, const expr_t *wrt,
                                                          equation_solutions_t *solutions)
{
    const expr_t *first = NULL;
    const expr_t *second = NULL;
    expr_t *expected_linear = NULL;
    bool reversed = false;
    int rc = 1;

    if (!equ_expr_is_one(quadratic))
        return 1;
    if (!expr_match_mul_expr(constant, &first, &second))
        return 1;
    if (equ_expr_uses_wrt(first, wrt) || equ_expr_uses_wrt(second, wrt))
        return 1;

    expected_linear = equ_symbolic_negated_sum(first, second);
    if (!expected_linear)
        return -1;

    if (!equ_expr_matches_expected(linear, expected_linear)) {
        expr_free(expected_linear);
        expected_linear = equ_symbolic_negated_sum(second, first);
        if (!expected_linear)
            return -1;
        if (!equ_expr_matches_expected(linear, expected_linear))
            goto cleanup;
        reversed = true;
    }

    if (equ_append_solution_expr(wrt, reversed ? second : first, solutions) != 0)
        goto error;
    if (equ_append_solution_expr(wrt, reversed ? first : second, solutions) != 0)
        goto error_clear;

    rc = 0;
    goto cleanup;

error_clear:
    equ_solutions_clear(solutions);
error:
    rc = -1;

cleanup:
    expr_free(expected_linear);
    return rc;
}

static int equ_try_solve_symbolic_quadratic(const expr_t *residual, const expr_t *wrt, equation_solutions_t *solutions)
{
    expr_t *constant = NULL;
    expr_t *linear = NULL;
    expr_t *quadratic = NULL;
    expr_t *discriminant = NULL;
    expr_t *sqrt_discriminant = NULL;
    expr_t *root_plus = NULL;
    expr_t *root_minus = NULL;
    int rc = -1;

    if (!equ_match_symbolic_quadratic_expr(residual, wrt, &constant, &linear, &quadratic)) {
        rc = 1;
        goto cleanup;
    }

    rc = equ_try_solve_symbolic_quadratic_product_roots(constant, linear, quadratic, wrt, solutions);
    if (rc <= 0)
        goto cleanup;

    discriminant = equ_symbolic_quadratic_discriminant(constant, linear, quadratic);
    sqrt_discriminant = discriminant ? expr_sqrt(discriminant) : NULL;
    sqrt_discriminant = expr_simplify_owned(sqrt_discriminant);
    root_plus = sqrt_discriminant ? equ_symbolic_quadratic_root(linear, quadratic, sqrt_discriminant, true) : NULL;
    root_minus = sqrt_discriminant ? equ_symbolic_quadratic_root(linear, quadratic, sqrt_discriminant, false) : NULL;

    if (!root_plus || !root_minus)
        goto cleanup;

    if (equ_append_solution_expr(wrt, root_plus, solutions) != 0)
        goto cleanup;
    if (equ_append_solution_expr(wrt, root_minus, solutions) != 0) {
        equ_solutions_clear(solutions);
        goto cleanup;
    }

    rc = 0;

cleanup:
    expr_free(root_minus);
    expr_free(root_plus);
    expr_free(sqrt_discriminant);
    expr_free(discriminant);
    expr_free(quadratic);
    expr_free(linear);
    expr_free(constant);
    return rc;
}

static int equ_try_solve_quadratic(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions)
{
    expr_t *residual = equ_residual(equation);
    number_t coeffs[3] = {num_new(), num_new(), num_new()};
    bool ok;
    int rc = -1;

    if (!residual)
        goto cleanup;

    ok = equ_match_quadratic_expr(residual, wrt, &coeffs[0], &coeffs[1], &coeffs[2]);
    if (!ok) {
        rc = equ_try_solve_symbolic_quadratic(residual, wrt, solutions);
        goto cleanup;
    }

    rc = equ_solve_quadratic_coefficients(coeffs, wrt, solutions);

cleanup:
    for (size_t i = 0u; i < 3u; ++i)
        num_destroy(&coeffs[i]);
    expr_free(residual);
    return rc;
}

static bool equ_expr_is_pi_over_four(const expr_t *expr)
{
    number_t value = expr ? expr_eval(expr) : num_new();
    number_t four = num_create_from_long(4L);
    number_t expected = num_div(NUM_PI, four);
    bool matches = num_is_finite(value) && num_eq(value, expected);

    num_destroy(&expected);
    num_destroy(&four);
    num_destroy(&value);
    return matches;
}

static int equ_try_solve_atan_sum_side(const expr_t *lhs, const expr_t *rhs, const expr_t *wrt,
                                       equation_solutions_t *solutions)
{
    const expr_t *first = NULL;
    const expr_t *second = NULL;
    expr_t *vars[1] = {(expr_t *)wrt};
    number_t first_constant = num_new();
    number_t second_constant = num_new();
    number_t first_coefficient = num_new();
    number_t second_coefficient = num_new();
    number_t quadratic = num_new();
    number_t linear = num_new();
    expr_t *wrt_squared = NULL;
    expr_t *quadratic_term = NULL;
    expr_t *linear_term = NULL;
    expr_t *sum = NULL;
    expr_t *one = NULL;
    expr_t *raw_residual = NULL;
    expr_t *residual = NULL;
    equation_solutions_t candidates;
    bool is_sub = false;
    int solve_rc;
    int rc = 1;

    equ_solutions_reset(&candidates);

    if (!lhs || !rhs || !wrt || !solutions) {
        rc = -1;
        goto cleanup;
    }
    if (!equ_expr_is_pi_over_four(rhs) || !expr_match_add_sub_expr(lhs, &first, &second, &is_sub) || is_sub)
        goto cleanup;
    if (!expr_match_unary_affine_kind(first, EXPR_PATTERN_UNARY_ATAN, 1u, vars, &first_constant, &first_coefficient) ||
        !expr_match_unary_affine_kind(second, EXPR_PATTERN_UNARY_ATAN, 1u, vars, &second_constant,
                                      &second_coefficient) ||
        !num_is_zero(first_constant) || !num_is_zero(second_constant) || num_is_zero(first_coefficient) ||
        num_is_zero(second_coefficient))
        goto cleanup;

    num_destroy(&quadratic);
    quadratic = num_mul(first_coefficient, second_coefficient);
    num_destroy(&linear);
    linear = num_add(first_coefficient, second_coefficient);

    wrt_squared = expr_pow(wrt, &NUM_TWO);
    quadratic_term = wrt_squared ? expr_mul_num(wrt_squared, &quadratic) : NULL;
    linear_term = expr_mul_num(wrt, &linear);
    sum = (quadratic_term && linear_term) ? expr_add(quadratic_term, linear_term) : NULL;
    one = expr_const_one();
    raw_residual = (sum && one) ? expr_sub(sum, one) : NULL;
    residual = raw_residual ? expr_simplify_owned(raw_residual) : NULL;
    raw_residual = NULL;
    if (!residual) {
        rc = -1;
        goto cleanup;
    }

    solve_rc = equ_try_solve_symbolic_quadratic(residual, wrt, &candidates);
    if (solve_rc != 0) {
        rc = solve_rc;
        goto cleanup;
    }

    for (size_t i = 0u; i < candidates.count; ++i) {
        const equation_t *candidate = candidates.solutions[i];
        const expr_t *root = candidate ? equ_rhs(candidate) : NULL;

        if (!root || !equ_candidate_satisfies_equation(lhs, rhs, wrt, root))
            continue;
        if (equ_append_solution_expr(wrt, root, solutions) != 0) {
            rc = -1;
            goto cleanup;
        }
    }
    rc = 0;

cleanup:
    equ_solutions_clear(&candidates);
    expr_free(residual);
    expr_free(raw_residual);
    expr_free(one);
    expr_free(sum);
    expr_free(linear_term);
    expr_free(quadratic_term);
    expr_free(wrt_squared);
    num_destroy(&linear);
    num_destroy(&quadratic);
    num_destroy(&second_coefficient);
    num_destroy(&first_coefficient);
    num_destroy(&second_constant);
    num_destroy(&first_constant);
    return rc;
}

static int equ_try_solve_atan_sum(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions)
{
    int rc;

    if (!equation || !wrt || !solutions)
        return -1;

    rc = equ_try_solve_atan_sum_side(equation->lhs, equation->rhs, wrt, solutions);
    if (rc != 1)
        return rc;
    return equ_try_solve_atan_sum_side(equation->rhs, equation->lhs, wrt, solutions);
}

static int equ_try_solve_zero_product_factor(const expr_t *factor, const expr_t *wrt, const expr_t *zero,
                                             equation_solutions_t *solutions, bool *saw_solved_factor)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    equation_t *factor_equation = NULL;
    equation_solutions_t factor_result;
    int rc = -1;

    equ_solutions_reset(&factor_result);

    if (expr_match_mul_expr(factor, &left, &right)) {
        rc = equ_try_solve_zero_product_factor(left, wrt, zero, solutions, saw_solved_factor);
        if (rc != 0)
            goto cleanup;
        rc = equ_try_solve_zero_product_factor(right, wrt, zero, solutions, saw_solved_factor);
        goto cleanup;
    }

    if (!equ_expr_uses_wrt(factor, wrt)) {
        rc = expr_is_exact_zero(factor) ? 1 : 0;
        goto cleanup;
    }

    factor_equation = equ_new(factor, zero);
    if (!factor_equation)
        goto cleanup;

    if (equ_solve_for_into(factor_equation, wrt, &factor_result) != 0)
        goto cleanup;
    if (factor_result.status != EQUATION_SOLVE_SOLVED || factor_result.count == 0u) {
        rc = 1;
        goto cleanup;
    }

    for (size_t i = 0u; i < factor_result.count; ++i) {
        if (equ_append_solution_expr(wrt, equ_rhs(factor_result.solutions[i]), solutions) != 0)
            goto cleanup;
    }

    *saw_solved_factor = true;
    rc = 0;

cleanup:
    equ_solutions_clear(&factor_result);
    equ_free(factor_equation);
    return rc;
}

static int equ_try_solve_zero_product(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions)
{
    const expr_t *product = NULL;
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *zero = NULL;
    bool saw_solved_factor = false;
    int rc;

    if (!equation || !wrt || !solutions)
        return -1;

    if (expr_is_exact_zero(equation->rhs))
        product = equation->lhs;
    else if (expr_is_exact_zero(equation->lhs))
        product = equation->rhs;
    else
        return 1;

    if (!expr_match_mul_expr(product, &left, &right))
        return 1;

    zero = expr_new_const(NUM_ZERO);
    if (!zero)
        return -1;

    rc = equ_try_solve_zero_product_factor(product, wrt, zero, solutions, &saw_solved_factor);
    expr_free(zero);

    if (rc != 0 || !saw_solved_factor) {
        equ_solutions_clear(solutions);
        return rc < 0 ? -1 : 1;
    }

    return 0;
}

static int equ_append_numeric_binding_solutions(expr_bindings_t *bindings, equation_solutions_t *solutions)
{
    size_t binding_count;

    if (!bindings || !solutions)
        return -1;

    binding_count = expr_bindings_count(bindings);
    for (size_t i = 0u; i < binding_count; ++i) {
        expr_t *expr = expr_bindings_expr_at(bindings, i);
        number_t value;
        expr_t *rhs;
        equation_t *solution;

        if (expr_bindings_is_constant_at(bindings, i) || !expr)
            continue;

        value = expr_eval(expr);
        rhs = expr_new_const(value);
        num_destroy(&value);
        if (!rhs)
            return -1;

        solution = equ_new(expr, rhs);
        expr_free(rhs);
        if (!solution)
            return -1;

        if (equ_solutions_append(solutions, solution) != 0) {
            equ_free(solution);
            return -1;
        }
    }

    return solutions->count > 0u ? 0 : 1;
}

static size_t equ_numeric_precision_digits(const expr_goal_seek_options_t *options)
{
    if (options && options->precision_digits > 0u)
        return options->precision_digits;
    return num_get_default_prec_digits();
}

static number_t equ_numeric_tolerance(const expr_goal_seek_options_t *options)
{
    if (options && num_is_finite(options->tolerance) && !num_is_zero(options->tolerance))
        return num_abs(options->tolerance);

    return num_pow10(-(int)equ_numeric_precision_digits(options));
}

static bool equ_numeric_residual_is_solved(const expr_t *residual, const expr_goal_seek_options_t *options)
{
    number_t value;
    number_t mag;
    number_t tolerance;
    bool ok;

    if (!residual)
        return false;

    value = expr_eval(residual);
    if (!num_is_finite(value)) {
        num_destroy(&value);
        return false;
    }

    mag = num_abs(value);
    tolerance = equ_numeric_tolerance(options);
    ok = num_is_finite(mag) && num_le(mag, tolerance);

    num_destroy(&tolerance);
    num_destroy(&mag);
    num_destroy(&value);
    return ok;
}

static number_t *equ_snapshot_binding_values(expr_bindings_t *bindings)
{
    number_t *values;
    size_t binding_count;

    binding_count = expr_bindings_count(bindings);
    if (binding_count == 0u)
        return NULL;

    values = calloc(binding_count, sizeof(*values));
    if (!values)
        return NULL;

    for (size_t i = 0u; i < binding_count; ++i) {
        expr_t *expr = expr_bindings_expr_at(bindings, i);

        values[i] = expr ? expr_eval(expr) : num_new();
    }
    return values;
}

static void equ_restore_binding_values(expr_bindings_t *bindings, number_t *values)
{
    size_t binding_count;

    if (!bindings || !values)
        return;

    binding_count = expr_bindings_count(bindings);
    for (size_t i = 0u; i < binding_count; ++i) {
        expr_t *expr = expr_bindings_expr_at(bindings, i);

        if (expr)
            expr_set_val(expr, values[i]);
    }
}

static void equ_free_binding_value_snapshot(expr_bindings_t *bindings, number_t *values)
{
    size_t binding_count;

    if (!bindings || !values)
        return;

    binding_count = expr_bindings_count(bindings);
    for (size_t i = 0u; i < binding_count; ++i)
        num_destroy(&values[i]);
    free(values);
}

int equ_solve_numeric_into(const equation_t *equation, expr_bindings_t *bindings,
                           const expr_goal_seek_options_t *options, equation_solutions_t *solutions)
{
    expr_t *residual = NULL;
    number_t target = num_create_from_long(0L);
    number_t *original_values = NULL;
    expr_goal_seek_result_t goal_result;
    int rc = -1;

    if (!solutions)
        return -1;

    equ_solutions_reset(solutions);
    if (!equation || !bindings)
        goto cleanup_no_goal;

    solutions->status = EQUATION_SOLVE_UNSOLVED;
    residual = equ_residual(equation);
    if (!residual)
        goto cleanup_no_goal;
    original_values = equ_snapshot_binding_values(bindings);
    if (expr_bindings_count(bindings) > 0u && !original_values)
        goto cleanup_no_goal;

    if (expr_goal_seek(residual, bindings, target, options, &goal_result) != 0) {
        equ_restore_binding_values(bindings, original_values);
        rc = 0;
        goto cleanup_no_goal;
    }

    if (!equ_numeric_residual_is_solved(residual, options)) {
        equ_restore_binding_values(bindings, original_values);
        rc = 0;
        expr_goal_seek_result_clear(&goal_result);
        goto cleanup_no_goal;
    }

    rc = equ_append_numeric_binding_solutions(bindings, solutions);
    if (rc < 0)
        equ_solutions_clear(solutions);
    else if (rc > 0)
        solutions->status = EQUATION_SOLVE_UNSOLVED;
    expr_goal_seek_result_clear(&goal_result);

cleanup_no_goal:
    equ_free_binding_value_snapshot(bindings, original_values);
    num_destroy(&target);
    expr_free(residual);
    return rc < 0 ? -1 : 0;
}

int equ_solve_for_into(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions)
{
    int unary_inverse_rc;
    int unary_periodic_rc;
    int atan_sum_rc;
    int self_power_rc;
    int repeated_power_rc;
    int affine_rc;
    int zero_product_rc;
    int quadratic_rc;
    int cubic_rc;
    int quartic_rc;
    int quintic_rc;
    int general_polynomial_rc;

    if (!solutions)
        return -1;

    equ_solutions_reset(solutions);

    if (!equation || !wrt)
        return -1;

    solutions->status = EQUATION_SOLVE_UNSOLVED;

    if (equ_is_solved_for(equation, wrt))
        return equ_append_existing_solution(equation, solutions);

    unary_inverse_rc = equ_try_solve_unary_inverse(equation, wrt, solutions);
    if (unary_inverse_rc == 0)
        return 0;
    if (unary_inverse_rc < 0)
        return -1;

    unary_periodic_rc = equ_try_solve_unary_periodic(equation, wrt, solutions);
    if (unary_periodic_rc == 0)
        return 0;
    if (unary_periodic_rc < 0)
        return -1;

    atan_sum_rc = equ_try_solve_atan_sum(equation, wrt, solutions);
    if (atan_sum_rc == 0)
        return 0;
    if (atan_sum_rc < 0)
        return -1;

    self_power_rc = equ_try_solve_self_power(equation, wrt, solutions);
    if (self_power_rc == 0)
        return 0;
    if (self_power_rc < 0)
        return -1;

    repeated_power_rc = equ_try_solve_repeated_power_polynomial(equation, wrt, solutions);
    if (repeated_power_rc == 0)
        return 0;
    if (repeated_power_rc < 0)
        return -1;

    affine_rc = equ_try_solve_affine(equation, wrt, solutions);
    if (affine_rc == 0)
        return 0;
    if (affine_rc < 0)
        return -1;

    /*
     * High-degree products may be internally regrouped into factors with
     * complex coefficients. Collect the complete polynomial first so
     * conjugate coefficients cancel before any individual factor is solved.
     */
    general_polynomial_rc = equ_try_solve_general_polynomial(equation, wrt, solutions);
    if (general_polynomial_rc == 0)
        return 0;
    if (general_polynomial_rc < 0)
        return -1;

    zero_product_rc = equ_try_solve_zero_product(equation, wrt, solutions);
    if (zero_product_rc == 0)
        return 0;
    if (zero_product_rc < 0)
        return -1;

    quadratic_rc = equ_try_solve_quadratic(equation, wrt, solutions);
    if (quadratic_rc == 0)
        return 0;
    if (quadratic_rc < 0)
        return -1;

    cubic_rc = equ_try_solve_cubic(equation, wrt, solutions);
    if (cubic_rc == 0)
        return 0;
    if (cubic_rc < 0)
        return -1;

    quartic_rc = equ_try_solve_quartic(equation, wrt, solutions);
    if (quartic_rc == 0)
        return 0;
    if (quartic_rc < 0)
        return -1;

    quintic_rc = equ_try_solve_quintic(equation, wrt, solutions);
    if (quintic_rc == 0)
        return 0;
    if (quintic_rc < 0)
        return -1;

    return 0;
}

equation_solutions_t *equ_derive_solutions(const equation_t *equation)
{
    equation_solutions_t *solutions = equ_solutions_new();
    expr_bindings_t *bindings = equ_bindings(equation);
    expr_goal_seek_options_t options;
    bool used_numeric = false;

    if (!solutions)
        return NULL;

    if (!equation)
        return solutions;

    if (!bindings) {
        if (equ_derive_without_bindings(equation, solutions) != 0)
            goto error;
        return solutions;
    }

    if (equ_derive_symbolic_solutions(equation, bindings, solutions) != 0)
        goto error;

    if (equ_solutions_count(solutions) == 0u && equ_variable_binding_count(bindings) > 0u) {
        if (equ_default_goal_seek_options(&options) != 0)
            goto error;
        used_numeric = true;
        if (equ_solve_numeric_into(equation, bindings, &options, solutions) != 0) {
            num_destroy(&options.tolerance);
            goto error;
        }
        num_destroy(&options.tolerance);
    }

    return solutions;

error:
    if (used_numeric)
        num_destroy(&options.tolerance);
    if (solutions) {
        equ_solutions_free(solutions);
    }
    return NULL;
}

size_t equ_solutions_count(const equation_solutions_t *solutions)
{
    return solutions ? solutions->count : 0u;
}

const equation_t *equ_solutions_at(const equation_solutions_t *solutions, size_t index)
{
    if (!solutions || index >= solutions->count)
        return NULL;
    return solutions->solutions[index];
}

void equ_solutions_clear(equation_solutions_t *solutions)
{
    if (!solutions)
        return;

    for (size_t i = 0u; i < solutions->count; ++i)
        equ_free(solutions->solutions[i]);
    free(solutions->solutions);
    equ_solutions_reset(solutions);
}
