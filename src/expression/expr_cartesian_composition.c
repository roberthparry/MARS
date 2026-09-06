#include <stdio.h>
#include <string.h>

#include "expr_stringout.h"

static bool composition_has_name(const expr_t *expr, const char *name)
{
    return expr && ((expr->name && strcmp(expr->name, name) == 0) ||
                    composition_has_name(expr->a, name) || composition_has_name(expr->b, name));
}

static expr_t *composition_name(const expr_t *root, const char *base)
{
    char name[64];
    unsigned suffix = 0u;

    snprintf(name, sizeof(name), "%s", base);
    while (composition_has_name(root, name))
        snprintf(name, sizeof(name), "%s%u", base, ++suffix);
    return expr_new_named_var(NUM_NAN, name);
}

static const expr_t *composition_source(const expr_t *expr)
{
    const expr_t *found;

    if (!expr)
        return NULL;
    if (expr_is_op(expr, &ops_Li) || expr_is_op(expr, &ops_Ei))
        return expr;
    found = composition_source(expr->a);
    return found ? found : composition_source(expr->b);
}

static expr_t *composition_binary(const expr_ops_t *ops, const expr_t *left, const expr_t *right)
{
    if (ops == &ops_pow && left && expr_is_const(right))
        return expr_new_pow_const_internal(expr_clone(left), right->c);
    expr_t *a = left ? expr_clone(left) : NULL;
    expr_t *b = right ? expr_clone(right) : NULL;
    expr_t *out = a && b ? expr_new_binary_internal(ops, a, b) : NULL;

    if (!out) {
        expr_free(b);
        expr_free(a);
    }
    return out;
}

static expr_t *composition_unary(const expr_ops_t *ops, const expr_t *arg)
{
    expr_t *a = arg ? expr_clone(arg) : NULL;
    expr_t *out = a ? expr_new_unary_internal(ops, a) : NULL;

    if (!out)
        expr_free(a);
    return out;
}

/* Substitute both fresh aliases at once, without evaluating or traversing the inserted series. */
static expr_t *composition_replace(const expr_t *expr, const expr_t *first, const expr_t *first_value,
                                    const expr_t *second, const expr_t *second_value)
{
    expr_t *out;

    if (!expr)
        return NULL;
    if (first && (expr == first || expr_struct_eq(expr, first)))
        return expr_clone(first_value);
    if (second && (expr == second || expr_struct_eq(expr, second)))
        return expr_clone(second_value);
    out = expr_clone(expr);
    if (!out)
        return NULL;
    if (expr->a) {
        expr_free(out->a);
        out->a = composition_replace(expr->a, first, first_value, second, second_value);
    }
    if (expr->b) {
        expr_free(out->b);
        out->b = composition_replace(expr->b, first, first_value, second, second_value);
    }
    if ((expr->a && !out->a) || (expr->b && !out->b)) {
        expr_free(out);
        return NULL;
    }
    if (expr->a || expr->b) {
        expr_binding_expr_free(out->binding_expr);
        out->binding_expr = NULL;
    }
    return out;
}

/* Build the real series themselves, so enclosing functions can operate on their coefficients. */
static bool composition_definitions(const expr_t *root, expr_cartesian_composition_t *view)
{
    static const char *indices[] = {"n", "m", "k", "l", "j"};
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    bool has_imaginary = false;
    const char *index_name = NULL;

    if (!expr_cartesian_parts_for_display(view->source->a, &real, &imaginary, &has_imaginary) || !has_imaginary) {
        expr_free(imaginary);
        expr_free(real);
        return false;
    }
    for (size_t i = 0u; i < sizeof(indices) / sizeof(indices[0]); ++i) {
        if (!composition_has_name(root, indices[i])) {
            index_name = indices[i];
            break;
        }
    }

    expr_t *index = index_name ? expr_new_named_var(NUM_NAN, index_name) : composition_name(root, "j");
    expr_t *two = expr_new_const(NUM_TWO);
    expr_t *real_squared = composition_binary(&ops_pow, real, two);
    expr_t *imaginary_squared = composition_binary(&ops_pow, imaginary, two);
    expr_t *argument_norm = composition_binary(&ops_add, real_squared, imaginary_squared);
    expr_t *log_norm = composition_unary(&ops_log, argument_norm);
    expr_t *log_real = composition_binary(&ops_div, log_norm, two);
    expr_t *log_imaginary = composition_binary(&ops_atan2, imaginary, real);
    const expr_t *a = expr_is_op(view->source, &ops_Li) ? log_real : real;
    const expr_t *b = expr_is_op(view->source, &ops_Li) ? log_imaginary : imaginary;
    expr_t *a_squared = composition_binary(&ops_pow, a, two);
    expr_t *b_squared = composition_binary(&ops_pow, b, two);
    expr_t *norm = composition_binary(&ops_add, a_squared, b_squared);
    expr_t *angle = composition_binary(&ops_atan2, b, a);
    expr_t *half_index = composition_binary(&ops_div, index, two);
    expr_t *power = composition_binary(&ops_pow, norm, half_index);
    expr_t *factorial = index ? expr_factorial(index) : NULL;
    expr_t *denominator = composition_binary(&ops_mul, index, factorial);
    expr_t *coefficient = composition_binary(&ops_div, power, denominator);
    expr_t *phase = composition_binary(&ops_mul, index, angle);
    expr_t *cosine = composition_unary(&ops_cos, phase);
    expr_t *sine = composition_unary(&ops_sin, phase);
    expr_t *real_term = composition_binary(&ops_mul, coefficient, cosine);
    expr_t *imaginary_term = composition_binary(&ops_mul, coefficient, sine);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *infinity = expr_new_const(NUM_INF);
    expr_t *real_sum = real_term && index && one && infinity
                           ? expr_new_finite_summation_range(real_term, index, one, infinity) : NULL;
    expr_t *imaginary_sum = imaginary_term && index && one && infinity
                                ? expr_new_finite_summation_range(imaginary_term, index, one, infinity) : NULL;
    expr_t *log_radius_squared = composition_unary(&ops_log, norm);
    expr_t *log_radius = composition_binary(&ops_div, log_radius_squared, two);
    expr_t *gamma = expr_new_const(NUM_EULER_MASCHERONI);
    expr_t *real_offset = composition_binary(&ops_add, gamma, log_radius);

    view->real_definition = composition_binary(&ops_add, real_offset, real_sum);
    view->imaginary_definition = composition_binary(&ops_add, angle, imaginary_sum);

    /* This bounded list owns the intermediates; the returned definitions retain their children. */
    expr_t *owned[] = {real, imaginary, index, two, real_squared, imaginary_squared, argument_norm, log_norm,
                       log_real, log_imaginary, a_squared, b_squared, norm, angle, half_index, power, factorial,
                       denominator, coefficient, phase, cosine, sine, real_term, imaginary_term, one, infinity,
                       real_sum, imaginary_sum, log_radius_squared, log_radius, gamma, real_offset};
    for (size_t i = 0u; i < sizeof(owned) / sizeof(owned[0]); ++i)
        expr_free(owned[i]);
    return view->real_definition && view->imaginary_definition;
}

/* Release every owning node in a Cartesian presentation. */
void expr_cartesian_composition_clear(expr_cartesian_composition_t *view)
{
    expr_free(view->expanded);
    expr_free(view->compact);
    expr_free(view->imaginary_definition);
    expr_free(view->real_definition);
    expr_free(view->imaginary_name);
    expr_free(view->real_name);
    memset(view, 0, sizeof(*view));
}

/* Normalise the enclosing algebra with real placeholders, then restore the explicit series. */
bool expr_cartesian_composition_init(const expr_t *expr, expr_cartesian_composition_t *view)
{
    expr_t *unit = NULL;
    expr_t *imaginary_term = NULL;
    expr_t *replacement = NULL;
    expr_t *substituted = NULL;
    expr_t *expanded = NULL;
    expr_t *simplified = NULL;
    bool ok = false;

    memset(view, 0, sizeof(*view));
    view->source = composition_source(expr);
    if (!view->source || view->source == expr || !composition_definitions(expr, view))
        goto cleanup;
    view->real_name = composition_name(expr, "p");
    view->imaginary_name = composition_name(expr, "q");
    unit = expr_new_const(NUM_I);
    imaginary_term = composition_binary(&ops_mul, view->imaginary_name, unit);
    replacement = composition_binary(&ops_add, view->real_name, imaginary_term);
    substituted = replacement ? composition_replace(expr, view->source, replacement, NULL, NULL) : NULL;
    expanded = substituted ? expr_complex_unary_cartesian_for_display(substituted) : NULL;
    simplified = substituted ? expr_simplify(expanded ? expanded : substituted) : NULL;
    view->compact = simplified ? expr_separate_cartesian_for_display(simplified) : NULL;
    if (view->compact) {
        expr_t *unit_last = expr_move_imaginary_unit_last_for_display(view->compact);

        if (unit_last) {
            expr_free(view->compact);
            view->compact = unit_last;
        }
    }
    view->expanded = composition_replace(view->compact, view->real_name, view->real_definition,
                                         view->imaginary_name, view->imaginary_definition);
    ok = view->expanded != NULL;

cleanup:
    expr_free(simplified);
    expr_free(expanded);
    expr_free(substituted);
    expr_free(replacement);
    expr_free(imaginary_term);
    expr_free(unit);
    if (!ok)
        expr_cartesian_composition_clear(view);
    return ok;
}
