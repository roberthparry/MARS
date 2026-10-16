/**
 * @file expr_distribution.c
 * @brief Native regular-part and impulse decomposition for distribution plots and presentation.
 *
 * Supports linear combinations, regular multipliers and affine Dirac supports.
 * A bounded sum preview contains its first 32 terms and reports that limitation.
 * Products of distributions and derivatives of impulses are not silently sampled.
 * Supported affine poles can be displayed as excluded ordinary coordinates,
 * separately from impulses, without modifying the underlying distribution.
 */
#include "computation-t.h"
#define MARS_SHARED_COMPUTATION_INTERNAL_ACCESS
#include "internal/computation_internal.h"
#define MARS_SHARED_EXPR_DISTRIBUTION_INTERNAL_ACCESS
#include "internal/expr_distribution_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_fourier_internal.h"

/* Distinguish unsupported distributions from ordinary expressions before numerical sampling. */
bool expr_is_distribution(const expr_t *f)
{
    return f && (f->ops == &ops_delta || f->ops == &ops_analytic_delta || f->ops == &ops_principal_value ||
                 expr_is_distribution(f->a) || expr_is_distribution(f->b));
}

/* Split only linear distribution algebra; unsupported forms keep their complete existing notation. */
static bool curve_parts(fourier_context_t *c, const expr_t *f, const expr_t *variable, expr_t **regular,
                        expr_t **impulses, string_t *conditions, unsigned depth)
{
    if (!f || depth > 128 || !computation_step_current("expression.distribution.presentation"))
        return false;
    if (!expr_is_distribution(f)) {
        *regular = expr_clone(f);
        *impulses = expr_const_zero();
    } else if (f->ops == &ops_delta) {
        *regular = expr_const_zero();
        *impulses = expr_clone(f);
    } else if (f->ops == &ops_principal_value && f->a && f->a->ops == &ops_div && !expr_is_distribution(f->a)) {
        expr_t *a = NULL, *b = NULL;
        if (!expr_fourier_affine(c, f->a->b, variable, &a, &b))
            return false;
        number_t coefficient = expr_eval(a);
        bool nonzero = num_is_finite(coefficient) && !num_is_zero(coefficient);
        num_destroy(&coefficient);
        if (!nonzero)
            return false;
        expr_t *ratio = expr_div(b, a), *negative = expr_neg(ratio), *root = expr_simplify(negative);
        number_t value = expr_eval(root);
        bool real = num_is_real(value) && num_is_finite(value);
        num_destroy(&value);
        char *point = real ? expr_to_TeX_body(root) : NULL;
        char *axis = expr_to_TeX_body(variable);
        if (point && axis) {
            if (string_length(conditions))
                string_append_cstr(conditions, ",\\quad ");
            string_append_cstr(conditions, axis);
            string_append_cstr(conditions, "\\ne ");
            string_append_cstr(conditions, point);
            *regular = expr_clone(f->a);
            *impulses = expr_const_zero();
        }
        free(axis);
        free(point);
        expr_free(root);
        expr_free(negative);
        expr_free(ratio);
    } else if (f->ops == &ops_real_domain || f->ops == &ops_neg) {
        expr_t *r = NULL, *d = NULL;
        bool ok = curve_parts(c, f->a, variable, &r, &d, conditions, depth + 1);
        if (ok) {
            *regular = expr_clone(f);
            *impulses = expr_clone(f);
            if (*regular && *impulses) {
                expr_free((*regular)->a);
                expr_free((*impulses)->a);
                (*regular)->a = r;
                (*impulses)->a = d;
                return true;
            }
        }
        expr_free(r);
        expr_free(d);
        return false;
    } else if (f->ops == &ops_add || f->ops == &ops_sub) {
        expr_t *ar = NULL, *ad = NULL, *br = NULL, *bd = NULL;
        bool ok = curve_parts(c, f->a, variable, &ar, &ad, conditions, depth + 1) &&
                  curve_parts(c, f->b, variable, &br, &bd, conditions, depth + 1);
        if (ok) {
            *regular = f->ops == &ops_add ? expr_add(ar, br) : expr_sub(ar, br);
            *impulses = f->ops == &ops_add ? expr_add(ad, bd) : expr_sub(ad, bd);
        }
        expr_free(ar);
        expr_free(ad);
        expr_free(br);
        expr_free(bd);
    } else if (f->ops == &ops_mul || f->ops == &ops_div) {
        const expr_t *body = f->a, *factor = f->b;
        if (f->ops == &ops_mul && !expr_is_distribution(body)) {
            body = f->b;
            factor = f->a;
        }
        if (expr_is_distribution(factor))
            return false;
        expr_t *r = NULL, *d = NULL;
        if (curve_parts(c, body, variable, &r, &d, conditions, depth + 1)) {
            *regular = f->ops == &ops_mul ? expr_mul(r, factor) : expr_div(r, factor);
            *impulses = f->ops == &ops_mul ? expr_mul(d, factor) : expr_div(d, factor);
        }
        expr_free(r);
        expr_free(d);
    }
    return *regular && *impulses;
}

/* Display the ordinary curve's excluded poles separately from the retained impulse expression. */
char *expr_distribution_curve_TeX(const expr_t *expression, const expr_t *variable)
{
    if (!expr_is_distribution(expression) || !variable || !expr_is_var(variable))
        return NULL;
    fourier_context_t c = {0};
    expr_t *regular = NULL, *impulses = NULL;
    string_t *conditions = string_new();
    expr_t *canonical = expr_substitute(expression, variable, variable);
    bool ok = conditions && curve_parts(&c, canonical, variable, &regular, &impulses, conditions, 0);
    expr_t *r = ok ? expr_simplify(regular) : NULL, *d = ok ? expr_simplify(impulses) : NULL;
    char *rt = r ? expr_to_TeX_body(r) : NULL, *dt = d ? expr_to_TeX_body(d) : NULL;
    string_t *text = rt && dt
                         ? string_sprintf("\\begin{gathered}\\text{Ordinary part: }%s%s%s\\\\"
                                          "\\text{Impulses (weights, not point values): }%s\\end{gathered}",
                                          rt, string_length(conditions) ? "\\quad " : "", string_c_str(conditions), dt)
                         : NULL;
    char *out = text ? strdup(string_c_str(text)) : NULL;
    string_free(text);
    string_free(conditions);
    free(rt);
    free(dt);
    expr_free(r);
    expr_free(d);
    expr_free(regular);
    expr_free(impulses);
    expr_free(canonical);
    for (size_t i = 0; i < c.count; ++i)
        expr_free(c.nodes[i]);
    free(c.nodes);
    expr_free(c.conditions);
    return out;
}

static expr_t *parts(fourier_context_t *c, const expr_t *f, const expr_t *w, const expr_t *scale,
                     expr_impulse_callback_t emit, void *context, bool *preview, unsigned depth)
{
    if (!f || depth > 128 || !computation_step_current("expression.distribution"))
        return NULL;
    if (!expr_is_distribution(f))
        return expr_mul(scale, f);
    if (f->ops == &ops_real_domain) {
        expr_t *guarded = expr_clone(f);
        expr_free(guarded->a);
        guarded->a = expr_clone(scale);
        expr_t *out = parts(c, f->a, w, guarded, emit, context, preview, depth + 1);
        expr_free(guarded);
        return out;
    }
    if (f->ops == &ops_principal_value)
        return expr_is_distribution(f->a) ? NULL : expr_mul(scale, f->a);
    if (f->ops == &ops_delta) {
        expr_t *a = NULL, *b = NULL;
        expr_t *argument = expr_substitute(f->a, w, w);
        bool affine = expr_fourier_affine(c, argument, w, &a, &b);
        expr_free(argument);
        if (!affine || expr_fourier_uses(a, w) || expr_fourier_uses(b, w))
            return NULL;
        number_t av = expr_eval(a), bv = expr_eval(b);
        bool valid = num_is_real(av) && num_is_finite(av) && !num_is_zero(av) && num_is_real(bv) && num_is_finite(bv);
        number_t ratio = valid ? num_div(bv, av) : NUM_NAN, root = num_neg(ratio), magnitude = num_abs(av);
        expr_t *point = expr_new_const(root), *weight_expr = expr_substitute(scale, w, point);
        number_t value = expr_eval(weight_expr), weight = num_div(value, magnitude);
        valid = valid && num_is_finite(weight) && emit(root, weight, context);
        num_destroy(&weight);
        num_destroy(&value);
        num_destroy(&magnitude);
        num_destroy(&root);
        num_destroy(&ratio);
        num_destroy(&bv);
        num_destroy(&av);
        expr_free(weight_expr);
        expr_free(point);
        return valid ? expr_const_zero() : NULL;
    }
    if (f->ops == &ops_add || f->ops == &ops_sub) {
        expr_t *left = parts(c, f->a, w, scale, emit, context, preview, depth + 1);
        expr_t *right_scale = f->ops == &ops_sub ? expr_neg(scale) : expr_clone(scale);
        expr_t *right = left ? parts(c, f->b, w, right_scale, emit, context, preview, depth + 1) : NULL;
        expr_t *out = left && right ? expr_add(left, right) : NULL;
        expr_free(right);
        expr_free(right_scale);
        expr_free(left);
        return out;
    }
    if (f->ops == &ops_neg || f->ops == &ops_mul || f->ops == &ops_div) {
        const expr_t *body = f->a;
        expr_t *factor = NULL;
        if (f->ops == &ops_neg)
            factor = expr_neg(scale);
        else if (!expr_is_distribution(f->b))
            factor = f->ops == &ops_div ? expr_div(scale, f->b) : expr_mul(scale, f->b);
        else if (f->ops == &ops_mul && !expr_is_distribution(f->a)) {
            body = f->b;
            factor = expr_mul(scale, f->a);
        }
        expr_t *out = factor ? parts(c, body, w, factor, emit, context, preview, depth + 1) : NULL;
        expr_free(factor);
        return out;
    }
    if (f->ops == &ops_summation && f->b && f->b->ops == &ops_argument_list && f->b->b &&
        f->b->b->ops == &ops_argument_list) {
        number_t low = expr_eval(f->b->b->a), high = expr_eval(f->b->b->b);
        bool infinite = num_is_inf(high) && num_get_sign(high) > 0;
        if (!num_is_real(low) || !num_is_finite(low) || (!infinite && !num_is_finite(high))) {
            num_destroy(&low);
            num_destroy(&high);
            return NULL;
        }
        expr_t *out = expr_const_zero();
        unsigned i = 0;
        for (; out && i < 32 && (infinite || !num_gt(low, high)); ++i) {
            expr_t *index = expr_new_const(low), *term = expr_substitute(f->a, f->b->a, index);
            expr_t *regular = parts(c, term, w, scale, emit, context, preview, depth + 1);
            expr_t *next = regular ? expr_add(out, regular) : NULL;
            expr_free(out);
            expr_free(regular);
            expr_free(term);
            expr_free(index);
            out = next;
            number_t next_index = num_add_long(low, 1);
            num_destroy(&low);
            low = next_index;
        }
        *preview |= infinite || !num_gt(low, high);
        num_destroy(&low);
        num_destroy(&high);
        return out;
    }
    return NULL;
}

/* Expose only native algebra and borrowed numeric impulse data across the module boundary. */
expr_t *expr_distribution_decompose(const expr_t *expression, const expr_t *variable, expr_impulse_callback_t callback,
                                    void *context, bool *preview)
{
    if (!expression || !variable || !expr_is_var(variable) || !callback || !preview ||
        !expr_is_distribution(expression))
        return NULL;
    *preview = false;
    fourier_context_t c = {0};
    expr_t *one = expr_const_one();
    /* Reparsed binders may use distinct IDs for the same free coordinate. Preserve local scopes. */
    expr_t *canonical = expr_substitute(expression, variable, variable);
    expr_t *out = parts(&c, canonical, variable, one, callback, context, preview, 0);
    expr_free(canonical);
    expr_free(one);
    for (size_t i = 0; i < c.count; ++i)
        expr_free(c.nodes[i]);
    free(c.nodes);
    expr_free(c.conditions);
    return out;
}
