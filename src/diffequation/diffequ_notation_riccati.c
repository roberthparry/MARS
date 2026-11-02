/**
 * @file diffequ_notation_riccati.c
 * @brief Present the native Riccati linearisation and its local domain as semantic mathematics.
 *
 * The solver supplies the normal potential and any computed fundamental solutions. Native coefficient
 * simplification is shared by the plain and semantic derivations. The logarithmic derivative is shown
 * directly as v′/v, with any remaining coefficient alongside it; TeX owns all typesetting syntax.
 */
#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

static const TeX_model_node_t *de_riccati_pair(notation_build_t *build, TeX_node_kind_t kind,
                                             const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *children[] = {left, right};
    return notation_build_node(build, (TeX_node_view_t){.kind = kind, .child_count = 2u}, children);
}

static const TeX_model_node_t *de_riccati_prime(notation_build_t *build,
                                              const TeX_model_node_t *value, unsigned int order)
{
    return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_PRIME, .child_count = 1u, .order = order},
                                &value);
}

/* Join a simplified native term, preserving its rendered sign and omitting a zero contribution. */
static bool de_riccati_append_term(string_t *out, const expr_t *term)
{
    if (expr_is_exact_zero(term))
        return true;
    string_t *text = term ? expr_to_text(term, style_UNBOUND) : NULL;
    const char *source = text ? string_c_str(text) : NULL;
    bool negative = source && *source == '-';
    bool valid = source && string_append_format(out, negative ? " - %s" : " + %s", source + negative) >= 0;
    string_free(text);
    return valid;
}

/* Keep the analytical reduction visible even when its linear equation has no supported closed form. */
bool de_riccati_steps(diffequ_solve_result_t *result, const expr_t *x, const expr_t *y,
                      const expr_t *a, const expr_t *shift, const expr_t *potential,
                      const expr_t *first, const expr_t *second, bool bessel)
{
    expr_t *v = expr_new_named_var(NUM_NAN, "v");
    expr_t *coefficient = expr_negate_owned(expr_div_simplify_owned(expr_const_one(), expr_clone(a)));
    expr_t *offset = expr_negate_owned(expr_div_simplify_owned(expr_clone(shift), expr_clone(a)));
    expr_t *potential_term = expr_mul_simplify_owned(expr_clone(potential), expr_clone(v));
    expr_t *first_simple = first ? expr_beautify(first) : NULL;
    expr_t *second_simple = second ? expr_beautify(second) : NULL;
    string_t *y_text = de_notation_symbol_text(y), *a_text = de_notation_symbol_text(a);
    const expr_t *left = NULL, *right = NULL, *factor = coefficient;
    bool is_sub = false;
    (void)expr_match_neg_expr(coefficient, &factor);
    bool grouped = expr_match_add_sub_expr(factor, &left, &right, &is_sub);
    number_t coefficient_value = num_new();
    bool constant_coefficient = expr_match_const_value(coefficient, &coefficient_value);
    bool unit = constant_coefficient && num_eq(coefficient_value, NUM_ONE);
    bool negative_unit = constant_coefficient && num_eq(coefficient_value, NUM_NEG_ONE);
    num_destroy(&coefficient_value);
    string_t *substitution_text = unit ? string_new_with("v′/v")
        : negative_unit ? string_new_with("-v′/v")
        : coefficient ? expr_sprintf_text(grouped ? "(%nu)·v′/v" : "%nu·v′/v", coefficient) : NULL;
    string_t *linear_text = potential_term ? string_new_with("v″") : NULL;
    if (substitution_text && !de_riccati_append_term(substitution_text, offset)) {
        string_free(substitution_text); substitution_text = NULL;
    }
    if (linear_text && !de_riccati_append_term(linear_text, potential_term)) {
        string_free(linear_text); linear_text = NULL;
    }
    string_t *first_text = first_simple ? expr_to_text(first_simple, style_UNBOUND) : NULL;
    string_t *second_text = second_simple ? expr_to_text(second_simple, style_UNBOUND) : NULL;
    number_t a_value = num_new();
    bool regular_constant = expr_match_const_value(a, &a_value) && num_is_finite(a_value) && !num_is_zero(a_value);
    num_destroy(&a_value);
    string_t *domain = a_text ? (regular_constant
        ? string_new_with("Work on intervals where the original coefficients are regular and v ≠ 0.")
        : string_sprintf("Work on intervals where the original coefficients are regular, with %S ≠ 0 and v ≠ 0.",
                          a_text)) : NULL;
    const char *x_name = expr_symbol_name(x);
    string_t *plain = y_text && substitution_text && linear_text && domain && x_name ? string_sprintf(
        "Riccati linearisation (SL(2, ℝ) Lie system)\n"
        "Primes denote differentiation with respect to %s.\n"
        "Set %S = %S.\n"
        "The auxiliary linear equation is %S = 0.\n"
        "%S\n", x_name, y_text, substitution_text, linear_text, domain) : NULL;
    bool valid = plain != NULL;
    if (valid && first && second)
        valid = first_text && second_text && string_append_format(plain,
            "u₀ = %S\nu₁ = %S\n"
            "%s\n"
            "Without an initial value, use v = u₀ + C·u₁ and also v = u₁.\n"
            "With a supported initial value, the coefficients are fixed at that point.\n"
            "Zeros of v are poles of the Riccati solution.", first_text, second_text,
            bessel ? "Use the continuous real extensions at zero. The common scale cancels, leaving one constant."
                   : "Their Wronskian is one. They are analytic at zero and on the real axis.") >= 0;
    else if (valid)
        valid = string_append_cstr(plain, "The linear reduction is available; no explicit solution is claimed.") == 0;

    notation_build_t *build = valid ? notation_build_new() : NULL;
    string_t *rendered = NULL;
    if (build) {
        const TeX_model_node_t *v_node = notation_build_expr(build, v);
        const TeX_model_node_t *zero = notation_build_literal(build, TeX_NODE_NUMBER, "0");
        const TeX_model_node_t *ratio = de_riccati_pair(build, TeX_NODE_FRACTION,
            de_riccati_prime(build, v_node, 1u), v_node);
        if (negative_unit)
            ratio = notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &ratio);
        else if (!unit)
            ratio = de_riccati_pair(build, TeX_NODE_PRODUCT, notation_build_expr(build, coefficient), ratio);
        const TeX_model_node_t *right = expr_is_exact_zero(offset) ? ratio
            : de_riccati_pair(build, TeX_NODE_SUM, ratio, notation_build_expr(build, offset));
        const TeX_model_node_t *second_derivative = de_riccati_prime(build, v_node, 2u);
        const TeX_model_node_t *linear_node = expr_is_exact_zero(potential_term) ? second_derivative
            : de_riccati_pair(build, TeX_NODE_SUM, second_derivative, notation_build_expr(build, potential_term));
        const TeX_model_node_t *rows[7] = {
            notation_build_literal(build, TeX_NODE_TEXT, "Riccati linearisation: SL(2, ℝ) Lie system"),
            de_riccati_pair(build, TeX_NODE_EQUATION, notation_build_expr(build, y), right),
            de_riccati_pair(build, TeX_NODE_EQUATION, linear_node, zero),
            notation_build_literal(build, TeX_NODE_TEXT, string_c_str(domain))
        };
        size_t count = 4u;
        if (first && second) {
            rows[count++] = de_riccati_pair(build, TeX_NODE_EQUATION, notation_build_formula(build, "u_0"),
                                           notation_build_expr(build, first_simple));
            rows[count++] = de_riccati_pair(build, TeX_NODE_EQUATION, notation_build_formula(build, "u_1"),
                                           notation_build_expr(build, second_simple));
            rows[count++] = notation_build_literal(build, TeX_NODE_TEXT,
                "Without an initial value: v = u0 + C*u1, together with v = u1. Zeros of v are poles.");
        }
        const TeX_model_node_t *root = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = count}, rows);
        rendered = notation_build_render(build, root);
    }
    valid = rendered && de_solve_result_set_steps(result, string_c_str(plain)) == 0 &&
                        de_solve_result_set_steps_TeX(result, string_c_str(rendered)) == 0;
    string_free(rendered);
    notation_build_free(build);
    string_free(plain);
    string_free(second_text); string_free(first_text);
    string_free(domain); string_free(linear_text); string_free(substitution_text);
    string_free(a_text); string_free(y_text);
    expr_free(second_simple); expr_free(first_simple);
    expr_free(potential_term); expr_free(offset); expr_free(coefficient); expr_free(v);
    return valid;
}
