#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

#include "expr_bindings.h"
#include "expr_maths.h"
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number/number_internal.h"
#include "ustring.h"

static int expr_text_to_unsigned_long(const string_t *text, unsigned long *out)
{
    string_cursor_t *cursor;
    bool saw_digit = false;
    unsigned long value = 0u;

    if (!text || !out || string_length(text) == 0u)
        return 0;

    cursor = string_cursor_new(text);
    if (!cursor)
        return 0;

    if (rune_is_equal(string_cursor_peek(cursor), '+'))
        (void)string_cursor_next(cursor);

    while (!string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);
        char ch = '\0';
        unsigned int digit;

        if (!rune_to_ascii(rune, &ch) || ch < '0' || ch > '9') {
            string_cursor_free(cursor);
            return 0;
        }

        digit = (unsigned int)(ch - '0');
        if (value > (ULONG_MAX - digit) / 10u) {
            string_cursor_free(cursor);
            return 0;
        }
        value = value * 10u + digit;
        saw_digit = true;
        (void)string_cursor_next(cursor);
    }

    string_cursor_free(cursor);
    if (!saw_digit)
        return 0;

    *out = value;
    return 1;
}

static int expr_text_to_long(const string_t *text, long *out)
{
    string_cursor_t *cursor;
    bool negative = false;
    bool saw_digit = false;
    unsigned long value = 0u;
    unsigned long limit;

    if (!text || !out || string_length(text) == 0u)
        return 0;

    cursor = string_cursor_new(text);
    if (!cursor)
        return 0;

    if (rune_is_equal(string_cursor_peek(cursor), '-')) {
        negative = true;
        (void)string_cursor_next(cursor);
    }

    limit = negative ? (unsigned long)LONG_MAX + 1u : (unsigned long)LONG_MAX;
    while (!string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);
        char ch = '\0';
        unsigned int digit;

        if (!rune_to_ascii(rune, &ch) || ch < '0' || ch > '9') {
            string_cursor_free(cursor);
            return 0;
        }

        digit = (unsigned int)(ch - '0');
        if (value > (limit - digit) / 10u) {
            string_cursor_free(cursor);
            return 0;
        }
        value = value * 10u + digit;
        saw_digit = true;
        (void)string_cursor_next(cursor);
    }

    string_cursor_free(cursor);
    if (!saw_digit)
        return 0;

    *out = negative && value == (unsigned long)LONG_MAX + 1u ? LONG_MIN : (negative ? -(long)value : (long)value);
    return 1;
}

static inline number_t expr_eval_unary_num(expr_t *dv, number_t (*fn)(const number_t))
{
    return fn(expr_eval_num_internal(dv->a));
}

static inline number_t expr_eval_binary_num(expr_t *dv, number_t (*fn)(const number_t, const number_t))
{
    return fn(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv->b));
}

int expr_number_to_polygamma_order(number_t value, unsigned int *order)
{
    string_t *text;
    unsigned long parsed;

    if (!order || !num_is_real(value) || !num_is_integer(value) || num_get_sign(value) < 0)
        return 0;

    text = num_to_string(value);
    if (!text)
        return 0;
    if (!expr_text_to_unsigned_long(text, &parsed) || parsed > UINT_MAX) {
        string_free(text);
        return 0;
    }
    string_free(text);
    *order = (unsigned int)parsed;
    return 1;
}

static int expr_number_to_unsigned_long(number_t value, unsigned long *out)
{
    string_t *text;
    unsigned long parsed;

    if (!out || !num_is_real(value) || !num_is_integer(value) || num_get_sign(value) < 0)
        return 0;

    text = num_to_string(value);
    if (!text)
        return 0;
    if (!expr_text_to_unsigned_long(text, &parsed)) {
        string_free(text);
        return 0;
    }
    string_free(text);
    *out = parsed;
    return 1;
}

static int expr_number_to_long(number_t value, long *out)
{
    string_t *text;
    long parsed;

    if (!out || !num_is_real(value) || !num_is_integer(value))
        return 0;

    text = num_to_string(value);
    if (!text)
        return 0;
    if (!expr_text_to_long(text, &parsed)) {
        string_free(text);
        return 0;
    }
    string_free(text);
    *out = parsed;
    return 1;
}

static expr_t *expr_chain_rule_with_factor(const expr_t *dv, expr_t *factor)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *out = expr_mul(factor, da);
    expr_free(factor);
    expr_free(da);
    return out;
}

static expr_t *expr_const_num_local(number_t value)
{
    NUM_SCOPE(scope);
    expr_t *dv = expr_new_const(value);
    return dv;
}

static expr_t *expr_log10_scale_factor_local(void)
{
    NUM_SCOPE(scope);
    number_t value = num_log(NUM_TEN);
    expr_t *dv = expr_new_const(value);

    if (dv)
        dv->binding_expr = expr_binding_expr_new_unary_op(&ops_log, expr_binding_expr_new_number_text("10"));
    return dv;
}

static expr_t *expr_const_ratio_local(number_t numerator, number_t denominator)
{
    NUM_SCOPE(scope);
    number_t value = num_div(numerator, denominator);

    return expr_const_num_local(value);
}

static expr_t *expr_const_neg_ratio_local(number_t numerator, number_t denominator)
{
    NUM_SCOPE(scope);
    number_t value = num_div(numerator, denominator);
    number_t neg_value = num_neg(value);

    return expr_const_num_local(neg_value);
}

number_t eval_sin(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_sin);
}
number_t eval_cos(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_cos);
}
static bool binding_expr_is_number_long(const expr_binding_expr_t *expr, long expected)
{
    number_t value;
    number_t expected_value;
    bool match;

    if (!expr || expr->kind != EXPR_BINDING_EXPR_NUMBER)
        return false;

    value = expr_binding_expr_eval(expr);
    expected_value = num_create_from_long(expected);
    match = num_eq(value, expected_value);
    num_destroy(&expected_value);
    num_destroy(&value);
    return match;
}

static int binding_expr_pi_over_two_sign(const expr_binding_expr_t *expr)
{
    if (!expr)
        return 0;

    if (expr->kind == EXPR_BINDING_EXPR_NEG)
        return -binding_expr_pi_over_two_sign(expr->u.unary.child);

    if (expr->kind == EXPR_BINDING_EXPR_DIV && expr->u.binary.left &&
        expr->u.binary.left->kind == EXPR_BINDING_EXPR_CONST &&
        expr->u.binary.left->u.const_id == EXPR_BINDING_CONST_PI &&
        binding_expr_is_number_long(expr->u.binary.right, 2))
        return 1;

    return 0;
}

number_t eval_tan(expr_t *dv)
{
    int pole_sign = dv && dv->a && dv->a->binding_expr ? binding_expr_pi_over_two_sign(dv->a->binding_expr) : 0;

    if (pole_sign > 0)
        return NUM_INF;
    if (pole_sign < 0)
        return NUM_NINF;
    return expr_eval_unary_num(dv, num_tan);
}

number_t eval_sec(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_sec);
}
number_t eval_cosec(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_cosec);
}
number_t eval_cot(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_cot);
}
number_t eval_versin(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_versin);
}
number_t eval_vercos(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_vercos);
}
number_t eval_coversin(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_coversin);
}
number_t eval_covercos(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_covercos);
}
number_t eval_haversin(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_haversin);
}
number_t eval_havercos(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_havercos);
}
number_t eval_hacoversin(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_hacoversin);
}
number_t eval_hacovercos(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_hacovercos);
}

number_t eval_sinh(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_sinh);
}
number_t eval_cosh(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_cosh);
}
number_t eval_tanh(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_tanh);
}
number_t eval_sech(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_sech);
}
number_t eval_cosech(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_cosech);
}
number_t eval_coth(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_coth);
}

number_t eval_asin(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_asin);
}
number_t eval_acos(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_acos);
}
number_t eval_atan(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_atan);
}
number_t eval_asec(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_asec);
}
number_t eval_acosec(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_acosec);
}
number_t eval_acot(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_acot);
}
number_t eval_arcversin(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_arcversin);
}
number_t eval_arcvercos(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_arcvercos);
}
number_t eval_arccoversin(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_arccoversin);
}
number_t eval_arccovercos(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_arccovercos);
}
number_t eval_archaversin(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_archaversin);
}
number_t eval_archavercos(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_archavercos);
}
number_t eval_archacoversin(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_archacoversin);
}
number_t eval_archacovercos(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_archacovercos);
}

number_t eval_asinh(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_asinh);
}
number_t eval_acosh(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_acosh);
}
number_t eval_atanh(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_atanh);
}
number_t eval_asech(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_asech);
}
number_t eval_acosech(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_acosech);
}
number_t eval_acoth(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_acoth);
}

number_t eval_exp(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_exp);
}
number_t eval_log(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_log);
}
number_t eval_log10(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_log10);
}
number_t eval_sqrt(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_sqrt);
}
number_t eval_cubrt(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_cubrt);
}
number_t eval_root(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_root);
}
number_t eval_floor(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_floor);
}
number_t eval_ceil(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_ceil);
}
number_t eval_abs(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_abs);
}
number_t eval_conj(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_conj);
}
number_t eval_erf(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_erf);
}
number_t eval_erfc(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_erfc);
}
number_t eval_lgamma(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_lgamma);
}
number_t eval_erfinv(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_erfinv);
}
number_t eval_erfcinv(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_erfcinv);
}
number_t eval_gamma(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_gamma);
}
number_t eval_digamma(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_digamma);
}
number_t eval_trigamma(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_trigamma);
}
number_t eval_polygamma(expr_t *dv)
{
    number_t order_value = expr_eval_num_internal(dv->a);
    unsigned int order;

    if (!expr_number_to_polygamma_order(order_value, &order))
        return NUM_NAN;
    return num_polygamma(order, expr_eval_num_internal(dv->b));
}
number_t eval_zeta(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_zeta);
}
number_t eval_zetap(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_zetap);
}
number_t eval_zetah(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_zetah);
}
number_t eval_zatahp(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_zatahp);
}
number_t eval_dilog(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_dilog);
}
number_t eval_polylog(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_polylog);
}
number_t eval_harmonic_poly(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_harmonic_poly);
}
number_t eval_legendre_chi(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_legendre_chi);
}
number_t eval_bessel_j(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_bessel_j);
}
number_t eval_bessel_y(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_bessel_y);
}
number_t eval_lommel_s_pack(expr_t *dv)
{
    (void)dv;
    return NUM_NAN;
}
bool expr_lommel_s_unpack(const expr_t *expr, const expr_t **mu, const expr_t **nu, const expr_t **argument)
{
    const expr_t *parameters;

    if (mu)
        *mu = NULL;
    if (nu)
        *nu = NULL;
    if (argument)
        *argument = NULL;
    if (!expr || !expr_is_op(expr, &ops_lommel_s) || !expr->a || !expr->b)
        return false;

    parameters = expr->a;
    if (!expr_is_op(parameters, &ops_lommel_s_pack) || !parameters->a || !parameters->b)
        return false;

    if (mu)
        *mu = parameters->a;
    if (nu)
        *nu = parameters->b;
    if (argument)
        *argument = expr->b;
    return true;
}
number_t eval_lommel_s(expr_t *dv)
{
    const expr_t *mu = NULL;
    const expr_t *nu = NULL;
    const expr_t *argument = NULL;

    if (!expr_lommel_s_unpack(dv, &mu, &nu, &argument))
        return NUM_NAN;
    return num_lommel_s(expr_eval_num_internal((expr_t *)mu), expr_eval_num_internal((expr_t *)nu),
                        expr_eval_num_internal((expr_t *)argument));
}

number_t eval_appell_f1_pack(expr_t *dv)
{
    (void)dv;
    return NUM_NAN;
}
bool expr_appell_f1_unpack(const expr_t *expr, const expr_t **a, const expr_t **b1, const expr_t **b2, const expr_t **c,
                           const expr_t **x, const expr_t **y)
{
    const expr_t *params;
    const expr_t *vars;
    const expr_t *ab;
    const expr_t *bc;

    if (a)
        *a = NULL;
    if (b1)
        *b1 = NULL;
    if (b2)
        *b2 = NULL;
    if (c)
        *c = NULL;
    if (x)
        *x = NULL;
    if (y)
        *y = NULL;
    if (!expr || !expr_is_op(expr, &ops_appell_f1) || !expr->a || !expr->b)
        return false;

    params = expr->a;
    vars = expr->b;
    if (!expr_is_op(params, &ops_appell_f1_pack) || !expr_is_op(vars, &ops_appell_f1_pack) || !params->a ||
        !params->b || !vars->a || !vars->b)
        return false;

    ab = params->a;
    bc = params->b;
    if (!expr_is_op(ab, &ops_appell_f1_pack) || !expr_is_op(bc, &ops_appell_f1_pack) || !ab->a || !ab->b || !bc->a ||
        !bc->b)
        return false;

    if (a)
        *a = ab->a;
    if (b1)
        *b1 = ab->b;
    if (b2)
        *b2 = bc->a;
    if (c)
        *c = bc->b;
    if (x)
        *x = vars->a;
    if (y)
        *y = vars->b;
    return true;
}
number_t eval_appell_f1(expr_t *dv)
{
    const expr_t *a = NULL;
    const expr_t *b1 = NULL;
    const expr_t *b2 = NULL;
    const expr_t *c = NULL;
    const expr_t *x = NULL;
    const expr_t *y = NULL;

    if (!expr_appell_f1_unpack(dv, &a, &b1, &b2, &c, &x, &y))
        return NUM_NAN;
    return num_appell_f1(expr_eval_num_internal(a), expr_eval_num_internal(b1), expr_eval_num_internal(b2),
                         expr_eval_num_internal(c), expr_eval_num_internal(x), expr_eval_num_internal(y));
}

number_t eval_hypergeometric_pFq_pack(expr_t *dv)
{
    (void)dv;
    return NUM_NAN;
}

static bool hypergeometric_parameter_count(const expr_t *expr, size_t *out)
{
    unsigned long count;

    if (!expr || !out || !expr_is_const(expr) || !expr_number_to_unsigned_long(expr->c, &count) || count > SIZE_MAX)
        return false;
    *out = (size_t)count;
    return true;
}

static bool hypergeometric_parameter_values(const expr_t *values, size_t count, const expr_t **out)
{
    if (count == 0u)
        return values != NULL;
    if (!values || !out)
        return false;
    if (count == 1u) {
        out[0] = values;
        return true;
    }
    if (!expr_is_op(values, &ops_hypergeometric_pFq_pack) || !values->a || !values->b)
        return false;
    if (!hypergeometric_parameter_values(values->a, count - 1u, out))
        return false;
    out[count - 1u] = values->b;
    return true;
}

static bool hypergeometric_parameter_set_unpack(const expr_t *set, const expr_t ***values_out, size_t *count_out)
{
    const expr_t **values = NULL;
    size_t count = 0u;

    if (!set || !values_out || !count_out || !expr_is_op(set, &ops_hypergeometric_pFq_pack) || !set->a || !set->b ||
        !hypergeometric_parameter_count(set->a, &count))
        return false;
    if (count > 0u) {
        values = calloc(count, sizeof(*values));
        if (!values)
            return false;
    }
    if (!hypergeometric_parameter_values(set->b, count, values)) {
        free(values);
        return false;
    }
    *values_out = values;
    *count_out = count;
    return true;
}

bool expr_lauricella_f_unpack(const expr_t *expr, const expr_t **a, const expr_t ***b, const expr_t **c,
                              const expr_t ***x, size_t *variable_count)
{
    const expr_t *parameters;
    const expr_t *a_c;
    const expr_t **b_values = NULL;
    const expr_t **x_values = NULL;
    size_t b_count = 0u;
    size_t x_count = 0u;

    if (a)
        *a = NULL;
    if (b)
        *b = NULL;
    if (c)
        *c = NULL;
    if (x)
        *x = NULL;
    if (variable_count)
        *variable_count = 0u;
    if (!expr || !a || !b || !c || !x || !variable_count || !expr_is_op(expr, &ops_lauricella_f) || !expr->a ||
        !expr->b)
        return false;
    parameters = expr->a;
    if (!expr_is_op(parameters, &ops_hypergeometric_pFq_pack) || !parameters->a || !parameters->b)
        return false;
    a_c = parameters->a;
    if (!expr_is_op(a_c, &ops_hypergeometric_pFq_pack) || !a_c->a || !a_c->b ||
        !hypergeometric_parameter_set_unpack(parameters->b, &b_values, &b_count) ||
        !hypergeometric_parameter_set_unpack(expr->b, &x_values, &x_count) || b_count != x_count) {
        free(x_values);
        free(b_values);
        return false;
    }
    *a = a_c->a;
    *b = b_values;
    *c = a_c->b;
    *x = x_values;
    *variable_count = b_count;
    return true;
}

number_t eval_lauricella_f(expr_t *dv)
{
    const expr_t *a = NULL;
    const expr_t **b = NULL;
    const expr_t *c = NULL;
    const expr_t **x = NULL;
    number_t *b_values = NULL;
    number_t *x_values = NULL;
    number_t result = NUM_NAN;
    size_t variable_count = 0u;

    if (!expr_lauricella_f_unpack(dv, &a, &b, &c, &x, &variable_count))
        return NUM_NAN;
    if (variable_count > 0u) {
        b_values = calloc(variable_count, sizeof(*b_values));
        x_values = calloc(variable_count, sizeof(*x_values));
        if (!b_values || !x_values)
            goto cleanup;
    }
    for (size_t i = 0u; i < variable_count; ++i) {
        b_values[i] = expr_eval_num_internal(b[i]);
        x_values[i] = expr_eval_num_internal(x[i]);
    }
    result = num_lauricella_f(expr_eval_num_internal(a), b_values, expr_eval_num_internal(c), x_values, variable_count);

cleanup:
    free(x_values);
    free(b_values);
    free(x);
    free(b);
    return result;
}

bool expr_hypergeometric_pFq_unpack(const expr_t *expr, const expr_t ***upper, size_t *upper_count,
                                    const expr_t ***lower, size_t *lower_count, const expr_t **argument)
{
    const expr_t *parameters;
    const expr_t **upper_values = NULL;
    const expr_t **lower_values = NULL;
    size_t p = 0u;
    size_t q = 0u;

    if (upper)
        *upper = NULL;
    if (upper_count)
        *upper_count = 0u;
    if (lower)
        *lower = NULL;
    if (lower_count)
        *lower_count = 0u;
    if (argument)
        *argument = NULL;
    if (!expr || !upper || !upper_count || !lower || !lower_count || !expr_is_op(expr, &ops_hypergeometric_pFq) ||
        !expr->a || !expr->b)
        return false;

    parameters = expr->a;
    if (!expr_is_op(parameters, &ops_hypergeometric_pFq_pack) || !parameters->a || !parameters->b)
        return false;
    if (!hypergeometric_parameter_set_unpack(parameters->a, &upper_values, &p) ||
        !hypergeometric_parameter_set_unpack(parameters->b, &lower_values, &q)) {
        free(upper_values);
        free(lower_values);
        return false;
    }
    *upper = upper_values;
    *upper_count = p;
    *lower = lower_values;
    *lower_count = q;
    if (argument)
        *argument = expr->b;
    return true;
}

number_t eval_hypergeometric_pFq(expr_t *dv)
{
    const expr_t **upper = NULL;
    const expr_t **lower = NULL;
    const expr_t *argument = NULL;
    number_t *upper_values = NULL;
    number_t *lower_values = NULL;
    number_t result = NUM_NAN;
    size_t p = 0u;
    size_t q = 0u;

    if (!expr_hypergeometric_pFq_unpack(dv, &upper, &p, &lower, &q, &argument))
        return NUM_NAN;
    if (p > 0u) {
        upper_values = calloc(p, sizeof(*upper_values));
        if (!upper_values)
            goto cleanup;
    }
    if (q > 0u) {
        lower_values = calloc(q, sizeof(*lower_values));
        if (!lower_values)
            goto cleanup;
    }
    for (size_t i = 0u; i < p; ++i)
        upper_values[i] = expr_eval_num_internal(upper[i]);
    for (size_t i = 0u; i < q; ++i)
        lower_values[i] = expr_eval_num_internal(lower[i]);
    result = num_hypergeometric_pFq(upper_values, p, lower_values, q, expr_eval_num_internal(argument));

cleanup:
    free(lower_values);
    free(upper_values);
    free(lower);
    free(upper);
    return result;
}
number_t eval_gammainv(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_gammainv);
}
number_t eval_lambert_w(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_productlog);
}
number_t eval_lambert_wn(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_lambert_wn);
}
number_t eval_lambert_w0(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_lambert_w0);
}
number_t eval_lambert_wm1(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_lambert_wm1);
}
number_t eval_normal_pdf(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_normal_pdf);
}
number_t eval_normal_cdf(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_normal_cdf);
}
number_t eval_normal_logpdf(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_normal_logpdf);
}
number_t eval_Ei(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_Ei);
}
number_t eval_E1(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_E1);
}

number_t eval_hypot(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_hypot);
}

number_t eval_beta(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_beta);
}

number_t eval_logbeta(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_logbeta);
}

number_t eval_gammainc_lower(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_gammainc_lower);
}

number_t eval_gammainc_upper(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_gammainc_upper);
}

number_t eval_gammainc_P(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_gammainc_P);
}

number_t eval_gammainc_Q(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_gammainc_Q);
}

number_t eval_factorial(expr_t *dv)
{
    number_t value = expr_eval_num_internal(dv->a);
    unsigned long n;

    return expr_number_to_unsigned_long(value, &n) ? num_factorial(n) : NUM_NAN;
}

number_t eval_fibonacci(expr_t *dv)
{
    number_t value = expr_eval_num_internal(dv->a);
    unsigned long n;

    return expr_number_to_unsigned_long(value, &n) ? num_fibonacci(n) : NUM_NAN;
}

number_t eval_partition(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_partition);
}
number_t eval_isqrt(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_isqrt);
}
number_t eval_gcd(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_gcd);
}
number_t eval_lcm(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_lcm);
}
number_t eval_mod(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_mod);
}
number_t eval_modinv(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_modinv);
}

number_t eval_is_prime(expr_t *dv)
{
    return num_create_from_long(num_is_prime(expr_eval_num_internal(dv->a)) ? 1L : 0L);
}

number_t eval_next_prime(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_next_prime);
}
number_t eval_prev_prime(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_prev_prime);
}

number_t eval_bit_and(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_bit_and);
}
number_t eval_bit_or(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_bit_or);
}
number_t eval_bit_xor(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_bit_xor);
}
number_t eval_bit_not(expr_t *dv)
{
    return expr_eval_unary_num(dv, num_bit_not);
}

number_t eval_shl(expr_t *dv)
{
    number_t value = expr_eval_num_internal(dv->a);
    number_t bits_value = expr_eval_num_internal(dv->b);
    long bits;

    return expr_number_to_long(bits_value, &bits) ? num_shl(value, bits) : NUM_NAN;
}

number_t eval_shr(expr_t *dv)
{
    number_t value = expr_eval_num_internal(dv->a);
    number_t bits_value = expr_eval_num_internal(dv->b);
    long bits;

    return expr_number_to_long(bits_value, &bits) ? num_shr(value, bits) : NUM_NAN;
}

number_t eval_factors(expr_t *dv)
{
    return expr_eval_num_internal(dv->a);
}

number_t eval_atan2(expr_t *dv)
{
    return num_atan2(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv->b));
}

expr_t *deriv_sin(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_cos(dv->a));
}

expr_t *deriv_cos(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *sin_a = expr_sin(dv->a);
    expr_t *neg_sin = expr_neg(sin_a);
    expr_free(sin_a);
    expr_t *out = expr_mul(neg_sin, da);
    expr_free(da);
    expr_free(neg_sin);
    return out;
}

expr_t *deriv_tan(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *t = expr_tan(dv->a);
    expr_t *t2 = expr_pow_long(t, 2);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *fac = expr_add(one, t2);
    expr_t *out = expr_mul(fac, da);
    expr_free(da);
    expr_free(t);
    expr_free(t2);
    expr_free(one);
    expr_free(fac);
    return out;
}

expr_t *deriv_sec(expr_t *dv)
{
    expr_t *sec_a = expr_sec(dv->a);
    expr_t *tan_a = expr_tan(dv->a);
    expr_t *fac = expr_mul(sec_a, tan_a);

    expr_free(sec_a);
    expr_free(tan_a);
    return expr_chain_rule_with_factor(dv, fac);
}

expr_t *deriv_cosec(expr_t *dv)
{
    expr_t *cosec_a = expr_cosec(dv->a);
    expr_t *cot_a = expr_cot(dv->a);
    expr_t *product = expr_mul(cosec_a, cot_a);
    expr_t *fac = expr_neg(product);

    expr_free(cosec_a);
    expr_free(cot_a);
    expr_free(product);
    return expr_chain_rule_with_factor(dv, fac);
}

expr_t *deriv_cot(expr_t *dv)
{
    expr_t *cosec_a = expr_cosec(dv->a);
    expr_t *square = expr_pow_long(cosec_a, 2);
    expr_t *fac = expr_neg(square);

    expr_free(cosec_a);
    expr_free(square);
    return expr_chain_rule_with_factor(dv, fac);
}

static expr_t *expr_half_factor(expr_t *factor)
{
    expr_t *half = expr_new_const(NUM_HALF);
    expr_t *out = expr_mul(half, factor);

    expr_free(half);
    expr_free(factor);
    return out;
}

expr_t *deriv_versin(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_sin(dv->a));
}

expr_t *deriv_vercos(expr_t *dv)
{
    expr_t *sin_a = expr_sin(dv->a);
    expr_t *fac = expr_neg(sin_a);

    expr_free(sin_a);
    return expr_chain_rule_with_factor(dv, fac);
}

expr_t *deriv_coversin(expr_t *dv)
{
    expr_t *cos_a = expr_cos(dv->a);
    expr_t *fac = expr_neg(cos_a);

    expr_free(cos_a);
    return expr_chain_rule_with_factor(dv, fac);
}

expr_t *deriv_covercos(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_cos(dv->a));
}

expr_t *deriv_haversin(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_half_factor(expr_sin(dv->a)));
}

expr_t *deriv_havercos(expr_t *dv)
{
    expr_t *sin_a = expr_sin(dv->a);
    expr_t *neg_sin = expr_neg(sin_a);

    expr_free(sin_a);
    return expr_chain_rule_with_factor(dv, expr_half_factor(neg_sin));
}

expr_t *deriv_hacoversin(expr_t *dv)
{
    expr_t *cos_a = expr_cos(dv->a);
    expr_t *neg_cos = expr_neg(cos_a);

    expr_free(cos_a);
    return expr_chain_rule_with_factor(dv, expr_half_factor(neg_cos));
}

expr_t *deriv_hacovercos(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_half_factor(expr_cos(dv->a)));
}

expr_t *deriv_sinh(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_cosh(dv->a));
}

expr_t *deriv_cosh(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_sinh(dv->a));
}

expr_t *deriv_tanh(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *t = expr_tanh(dv->a);
    expr_t *t2 = expr_pow_long(t, 2);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *fac = expr_sub(one, t2);
    expr_t *out = expr_mul(fac, da);
    expr_free(da);
    expr_free(t);
    expr_free(t2);
    expr_free(one);
    expr_free(fac);
    return out;
}

expr_t *deriv_sech(expr_t *dv)
{
    expr_t *sech_a = expr_sech(dv->a);
    expr_t *tanh_a = expr_tanh(dv->a);
    expr_t *product = expr_mul(sech_a, tanh_a);
    expr_t *fac = expr_neg(product);

    expr_free(sech_a);
    expr_free(tanh_a);
    expr_free(product);
    return expr_chain_rule_with_factor(dv, fac);
}

expr_t *deriv_cosech(expr_t *dv)
{
    expr_t *cosech_a = expr_cosech(dv->a);
    expr_t *coth_a = expr_coth(dv->a);
    expr_t *product = expr_mul(cosech_a, coth_a);
    expr_t *fac = expr_neg(product);

    expr_free(cosech_a);
    expr_free(coth_a);
    expr_free(product);
    return expr_chain_rule_with_factor(dv, fac);
}

expr_t *deriv_coth(expr_t *dv)
{
    expr_t *cosech_a = expr_cosech(dv->a);
    expr_t *square = expr_pow_long(cosech_a, 2);
    expr_t *fac = expr_neg(square);

    expr_free(cosech_a);
    expr_free(square);
    return expr_chain_rule_with_factor(dv, fac);
}

expr_t *deriv_exp(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_exp(dv->a));
}

expr_t *deriv_log(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *out = expr_div(da, dv->a);
    expr_free(da);
    return out;
}

expr_t *deriv_log10(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *ln10 = expr_log10_scale_factor_local();
    expr_t *den = expr_mul(dv->a, ln10);
    expr_t *out = expr_div(da, den);

    expr_free(da);
    expr_free(ln10);
    expr_free(den);
    return out;
}

expr_t *deriv_sqrt(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *two = expr_const_long(2);
    expr_t *sqra = expr_sqrt(dv->a);
    expr_t *den = expr_mul(two, sqra);
    expr_free(sqra);
    expr_t *out = expr_div(da, den);
    expr_free(da);
    expr_free(two);
    expr_free(den);
    return out;
}

expr_t *deriv_cubrt(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *root = expr_cubrt(dv->a);
    expr_t *root_squared = expr_pow(root, &NUM_TWO);
    expr_t *three = expr_const_long(3);
    expr_t *denominator = expr_mul(three, root_squared);
    expr_t *out = expr_div(da, denominator);

    expr_free(da);
    expr_free(root);
    expr_free(root_squared);
    expr_free(three);
    expr_free(denominator);
    return out;
}

expr_t *deriv_root(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *dn = expr_get_dx_internal(dv->b);
    expr_t *root = expr_root(dv->a, dv->b);
    expr_t *an = expr_mul(dv->a, dv->b);
    expr_t *base_term = expr_div(da, an);
    expr_t *log_base = expr_log(dv->a);
    expr_t *order_squared = expr_pow(dv->b, &NUM_TWO);
    expr_t *order_term_numerator = expr_mul(dn, log_base);
    expr_t *order_term = expr_div(order_term_numerator, order_squared);
    expr_t *difference = expr_sub(base_term, order_term);
    expr_t *out = expr_mul(root, difference);

    expr_free(da);
    expr_free(dn);
    expr_free(root);
    expr_free(an);
    expr_free(base_term);
    expr_free(log_base);
    expr_free(order_squared);
    expr_free(order_term_numerator);
    expr_free(order_term);
    expr_free(difference);
    return out;
}

expr_t *deriv_floor(expr_t *dv)
{
    (void)dv;
    return expr_new_const(NUM_ZERO);
}

expr_t *deriv_ceil(expr_t *dv)
{
    (void)dv;
    return expr_new_const(NUM_ZERO);
}

expr_t *deriv_not_differentiable(expr_t *dv)
{
    (void)dv;
    return expr_new_const(NUM_NAN);
}

expr_t *deriv_asin(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *a2 = expr_pow_long(dv->a, 2);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *sub = expr_sub(one, a2);
    expr_t *den = expr_sqrt(sub);
    expr_free(sub);
    expr_t *out = expr_div(da, den);
    expr_free(da);
    expr_free(a2);
    expr_free(one);
    expr_free(den);
    return out;
}

expr_t *deriv_acos(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *a2 = expr_pow_long(dv->a, 2);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *sub = expr_sub(one, a2);
    expr_t *den = expr_sqrt(sub);
    expr_free(sub);
    expr_t *num = expr_neg(da);
    expr_t *out = expr_div(num, den);
    expr_free(da);
    expr_free(a2);
    expr_free(one);
    expr_free(den);
    expr_free(num);
    return out;
}

expr_t *deriv_atan(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *a2 = expr_pow_long(dv->a, 2);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *den = expr_add(one, a2);
    expr_t *out = expr_div(da, den);
    expr_free(da);
    expr_free(a2);
    expr_free(one);
    expr_free(den);
    return out;
}

/* Differentiate the principal inverse secant directly. */
expr_t *deriv_asec(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *square = expr_pow_long(dv->a, 2L);
    expr_t *radicand = square ? expr_add_long(square, -1L) : NULL;
    expr_t *root = radicand ? expr_sqrt(radicand) : NULL;
    expr_t *denominator = root ? expr_mul(dv->a, root) : NULL;
    expr_t *out = da && denominator ? expr_div(da, denominator) : NULL;

    expr_free(denominator);
    expr_free(root);
    expr_free(radicand);
    expr_free(square);
    expr_free(da);
    return out;
}

/* Differentiate the principal inverse cosecant directly. */
expr_t *deriv_acosec(expr_t *dv)
{
    expr_t *positive = deriv_asec(dv);

    return expr_negate_owned(positive);
}

/* Differentiate the principal inverse cotangent directly. */
expr_t *deriv_acot(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *square = expr_pow_long(dv->a, 2L);
    expr_t *denominator = square ? expr_add_long(square, 1L) : NULL;
    expr_t *negative_da = da ? expr_neg(da) : NULL;
    expr_t *out = negative_da && denominator ? expr_div(negative_da, denominator) : NULL;

    expr_free(negative_da);
    expr_free(denominator);
    expr_free(square);
    expr_free(da);
    return out;
}

static expr_t *expr_haversine_inverse_arg(const expr_t *arg, int scale, int offset_sign)
{
    expr_t *offset = expr_new_const(offset_sign < 0 ? NUM_NEG_ONE : NUM_ONE);
    expr_t *scaled = NULL;
    expr_t *out;

    if (scale == 1) {
        out = offset_sign < 0 ? expr_add(arg, offset) : expr_sub(offset, arg);
    } else {
        expr_t *scale_expr = expr_new_const(NUM_TWO);

        scaled = expr_mul(scale_expr, arg);
        out = offset_sign < 0 ? expr_add(scaled, offset) : expr_sub(offset, scaled);
        expr_free(scale_expr);
    }

    expr_free(scaled);
    expr_free(offset);
    return out;
}

static expr_t *expr_deriv_haversine_inverse(expr_t *dv, int use_acos, int scale, int offset_sign)
{
    expr_t *inner = expr_haversine_inverse_arg(dv->a, scale, offset_sign);
    expr_t *inverse = use_acos ? expr_acos(inner) : expr_asin(inner);
    expr_t *out = expr_get_dx_internal(inverse);

    expr_free(inverse);
    expr_free(inner);
    return out;
}

expr_t *deriv_arcversin(expr_t *dv)
{
    return expr_deriv_haversine_inverse(dv, 1, 1, 1);
}

expr_t *deriv_arcvercos(expr_t *dv)
{
    return expr_deriv_haversine_inverse(dv, 1, 1, -1);
}

expr_t *deriv_arccoversin(expr_t *dv)
{
    return expr_deriv_haversine_inverse(dv, 0, 1, 1);
}

expr_t *deriv_arccovercos(expr_t *dv)
{
    return expr_deriv_haversine_inverse(dv, 0, 1, -1);
}

expr_t *deriv_archaversin(expr_t *dv)
{
    return expr_deriv_haversine_inverse(dv, 1, 2, 1);
}

expr_t *deriv_archavercos(expr_t *dv)
{
    return expr_deriv_haversine_inverse(dv, 1, 2, -1);
}

expr_t *deriv_archacoversin(expr_t *dv)
{
    return expr_deriv_haversine_inverse(dv, 0, 2, 1);
}

expr_t *deriv_archacovercos(expr_t *dv)
{
    return expr_deriv_haversine_inverse(dv, 0, 2, -1);
}

expr_t *deriv_atan2(expr_t *dv)
{
    expr_t *y = dv->a;
    expr_t *x = dv->b;
    expr_t *dy = expr_get_dx_internal(y);
    expr_t *dx = expr_get_dx_internal(x);
    expr_t *x_dy = expr_mul(x, dy);
    expr_t *y_dx = expr_mul(y, dx);
    expr_t *num = expr_sub(x_dy, y_dx);
    expr_t *x2 = expr_mul(x, x);
    expr_t *y2 = expr_mul(y, y);
    expr_t *den = expr_add(x2, y2);
    expr_t *out = expr_div(num, den);
    expr_free(dy);
    expr_free(dx);
    expr_free(x_dy);
    expr_free(y_dx);
    expr_free(num);
    expr_free(x2);
    expr_free(y2);
    expr_free(den);
    return out;
}

expr_t *deriv_asinh(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *a2 = expr_pow_long(dv->a, 2);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *sum = expr_add(one, a2);
    expr_t *den = expr_sqrt(sum);
    expr_free(sum);
    expr_t *out = expr_div(da, den);
    expr_free(da);
    expr_free(a2);
    expr_free(one);
    expr_free(den);
    return out;
}

expr_t *deriv_acosh(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *am1 = expr_sub(dv->a, one);
    expr_t *ap1 = expr_add(dv->a, one);
    expr_t *s1 = expr_sqrt(am1);
    expr_t *s2 = expr_sqrt(ap1);
    expr_t *den = expr_mul(s1, s2);
    expr_t *out = expr_div(da, den);
    expr_free(da);
    expr_free(one);
    expr_free(am1);
    expr_free(ap1);
    expr_free(s1);
    expr_free(s2);
    expr_free(den);
    return out;
}

expr_t *deriv_atanh(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *a2 = expr_pow_long(dv->a, 2);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *den = expr_sub(one, a2);
    expr_t *out = expr_div(da, den);
    expr_free(da);
    expr_free(a2);
    expr_free(one);
    expr_free(den);
    return out;
}

/* Differentiate the principal inverse hyperbolic secant directly. */
expr_t *deriv_asech(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *square = expr_pow_long(dv->a, 2L);
    expr_t *radicand = NULL;
    expr_t *root = NULL;
    expr_t *denominator = NULL;
    expr_t *negative_da = NULL;
    expr_t *out;

    if (square) {
        radicand = expr_sub_simplify_owned(expr_const_one(), square);
        square = NULL;
    }
    root = radicand ? expr_sqrt(radicand) : NULL;
    denominator = root ? expr_mul(dv->a, root) : NULL;
    negative_da = da ? expr_neg(da) : NULL;
    out = negative_da && denominator ? expr_div(negative_da, denominator) : NULL;

    expr_free(negative_da);
    expr_free(denominator);
    expr_free(root);
    expr_free(radicand);
    expr_free(square);
    expr_free(da);
    return out;
}

/* Differentiate the principal inverse hyperbolic cosecant directly. */
expr_t *deriv_acosech(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *square = expr_pow_long(dv->a, 2L);
    expr_t *radicand = square ? expr_add_long(square, 1L) : NULL;
    expr_t *root = radicand ? expr_sqrt(radicand) : NULL;
    expr_t *denominator = root ? expr_mul(dv->a, root) : NULL;
    expr_t *negative_da = da ? expr_neg(da) : NULL;
    expr_t *out = negative_da && denominator ? expr_div(negative_da, denominator) : NULL;

    expr_free(negative_da);
    expr_free(denominator);
    expr_free(root);
    expr_free(radicand);
    expr_free(square);
    expr_free(da);
    return out;
}

/* Differentiate the principal inverse hyperbolic cotangent directly. */
expr_t *deriv_acoth(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *square = expr_pow_long(dv->a, 2L);
    expr_t *denominator = NULL;
    expr_t *out;

    if (square) {
        denominator = expr_sub_simplify_owned(expr_const_one(), square);
        square = NULL;
    }
    out = da && denominator ? expr_div(da, denominator) : NULL;

    expr_free(denominator);
    expr_free(square);
    expr_free(da);
    return out;
}

expr_t *deriv_abs(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *absa = expr_abs(dv->a);
    expr_t *sign = expr_div(dv->a, absa);
    expr_t *out = expr_mul(sign, da);
    expr_free(da);
    expr_free(absa);
    expr_free(sign);
    return out;
}

expr_t *deriv_conj(expr_t *dv)
{
    expr_t *derivative = expr_get_dx_internal(dv->a);
    expr_t *out = derivative ? expr_conj(derivative) : NULL;

    expr_free(derivative);
    return out;
}

expr_t *deriv_erf(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *c = expr_const_num_local(NUM_2_SQRTPI);
    expr_t *a2 = expr_pow_long(dv->a, 2);
    expr_t *neg_a2 = expr_neg(a2);
    expr_t *ea2 = expr_exp(neg_a2);
    expr_t *fac = expr_mul(c, ea2);
    expr_t *out = expr_mul(fac, da);
    expr_free(da);
    expr_free(c);
    expr_free(a2);
    expr_free(neg_a2);
    expr_free(ea2);
    expr_free(fac);
    return out;
}

expr_t *deriv_erfc(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *c = expr_const_num_local(NUM_NEG_TWO_OVER_SQRT_PI);
    expr_t *a2 = expr_pow_long(dv->a, 2);
    expr_t *neg_a2 = expr_neg(a2);
    expr_t *ea2 = expr_exp(neg_a2);
    expr_t *fac = expr_mul(c, ea2);
    expr_t *out = expr_mul(fac, da);
    expr_free(da);
    expr_free(c);
    expr_free(a2);
    expr_free(neg_a2);
    expr_free(ea2);
    expr_free(fac);
    return out;
}

expr_t *deriv_lgamma(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_digamma(dv->a));
}

expr_t *deriv_hypot(expr_t *dv)
{
    expr_t *a = dv->a;
    expr_t *b = dv->b;
    expr_t *da = expr_get_dx_internal(a);
    expr_t *db = expr_get_dx_internal(b);
    expr_t *a_da = expr_mul(a, da);
    expr_t *b_db = expr_mul(b, db);
    expr_t *num = expr_add(a_da, b_db);
    expr_t *h = expr_hypot(a, b);
    expr_t *out = expr_div(num, h);
    expr_free(da);
    expr_free(db);
    expr_free(a_da);
    expr_free(b_db);
    expr_free(num);
    expr_free(h);
    return out;
}

expr_t *deriv_erfinv(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *w = expr_erfinv(dv->a);
    expr_t *w2 = expr_pow_long(w, 2);
    expr_t *ew2 = expr_exp(w2);
    expr_t *c = expr_const_ratio_local(NUM_SQRT_PI, NUM_TWO);
    expr_t *fac = expr_mul(c, ew2);
    expr_t *out = expr_mul(fac, da);
    expr_free(da);
    expr_free(w);
    expr_free(w2);
    expr_free(ew2);
    expr_free(c);
    expr_free(fac);
    return out;
}

expr_t *deriv_erfcinv(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *w = expr_erfcinv(dv->a);
    expr_t *w2 = expr_pow_long(w, 2);
    expr_t *ew2 = expr_exp(w2);
    expr_t *c = expr_const_neg_ratio_local(NUM_SQRT_PI, NUM_TWO);
    expr_t *fac = expr_mul(c, ew2);
    expr_t *out = expr_mul(fac, da);
    expr_free(da);
    expr_free(w);
    expr_free(w2);
    expr_free(ew2);
    expr_free(c);
    expr_free(fac);
    return out;
}

expr_t *deriv_gamma(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *g = expr_gamma(dv->a);
    expr_t *dg = expr_digamma(dv->a);
    expr_t *gdg = expr_mul(g, dg);
    expr_t *out = expr_mul(gdg, da);
    expr_free(da);
    expr_free(g);
    expr_free(dg);
    expr_free(gdg);
    return out;
}

expr_t *deriv_digamma(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_trigamma(dv->a));
}

expr_t *deriv_trigamma(expr_t *dv)
{
    expr_t *factor = expr_polygamma(2u, dv->a);

    return expr_chain_rule_with_factor(dv, factor);
}

expr_t *deriv_polygamma(expr_t *dv)
{
    number_t order_value = expr_eval_num_internal(dv->a);
    unsigned int order;
    expr_t *factor;
    expr_t *db;
    expr_t *out;

    if (!expr_number_to_polygamma_order(order_value, &order))
        return expr_new_const(NUM_NAN);

    factor = expr_polygamma(order + 1u, dv->b);
    db = expr_get_dx_internal(dv->b);
    out = expr_mul(factor, db);
    expr_free(factor);
    expr_free(db);
    return out;
}

expr_t *deriv_zeta(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_zetap(dv->a));
}

expr_t *deriv_zetap(expr_t *dv)
{
    expr_t *wrts[1] = {dv->a};
    expr_t *factor = expr_new_formal_derivative(dv, 1u, wrts);

    return expr_chain_rule_with_factor(dv, factor);
}

expr_t *deriv_zetah(expr_t *dv)
{
    expr_t *ds = expr_get_dx_internal(dv->a);
    expr_t *da = expr_get_dx_internal(dv->b);
    expr_t *s_partial = expr_zatahp(dv->a, dv->b);
    expr_t *s_term = expr_mul(ds, s_partial);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *s_plus_one = expr_add(dv->a, one);
    expr_t *shift_zeta = expr_zetah(s_plus_one, dv->b);
    expr_t *scaled_shift_zeta = expr_mul(dv->a, shift_zeta);
    expr_t *a_partial = expr_neg(scaled_shift_zeta);
    expr_t *a_term = expr_mul(da, a_partial);
    expr_t *out = expr_add(s_term, a_term);

    expr_free(a_term);
    expr_free(a_partial);
    expr_free(scaled_shift_zeta);
    expr_free(shift_zeta);
    expr_free(s_plus_one);
    expr_free(one);
    expr_free(s_term);
    expr_free(s_partial);
    expr_free(da);
    expr_free(ds);
    return out;
}

expr_t *deriv_zatahp(expr_t *dv)
{
    expr_t *ds = expr_get_dx_internal(dv->a);
    expr_t *da = expr_get_dx_internal(dv->b);
    expr_t *wrts[1] = {dv->a};
    expr_t *s_partial = expr_new_formal_derivative(dv, 1u, wrts);
    expr_t *s_term = expr_mul(ds, s_partial);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *s_plus_one = expr_add(dv->a, one);
    expr_t *shift_zeta = expr_zetah(s_plus_one, dv->b);
    expr_t *shift_zetap = expr_zatahp(s_plus_one, dv->b);
    expr_t *scaled_shift_zetap = expr_mul(dv->a, shift_zetap);
    expr_t *a_partial_sum = expr_add(shift_zeta, scaled_shift_zetap);
    expr_t *a_partial = expr_neg(a_partial_sum);
    expr_t *a_term = expr_mul(da, a_partial);
    expr_t *out = expr_add(s_term, a_term);

    expr_free(a_term);
    expr_free(a_partial);
    expr_free(a_partial_sum);
    expr_free(scaled_shift_zetap);
    expr_free(shift_zetap);
    expr_free(shift_zeta);
    expr_free(s_plus_one);
    expr_free(one);
    expr_free(s_term);
    expr_free(s_partial);
    expr_free(da);
    expr_free(ds);
    return out;
}

expr_t *deriv_dilog(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *one_minus_a = expr_sub(one, dv->a);
    expr_t *log_term = expr_log(one_minus_a);
    expr_t *neg_log = expr_neg(log_term);
    expr_t *factor = expr_div(neg_log, dv->a);
    expr_t *out = expr_mul(factor, da);

    expr_free(da);
    expr_free(one);
    expr_free(one_minus_a);
    expr_free(log_term);
    expr_free(neg_log);
    expr_free(factor);
    return out;
}

expr_t *deriv_polylog(expr_t *dv)
{
    number_t order_value = expr_eval_num_internal(dv->a);
    unsigned int order;
    expr_t *factor = NULL;
    expr_t *db;
    expr_t *out;

    if (!expr_number_to_polygamma_order(order_value, &order))
        return expr_new_const(NUM_NAN);

    if (order == 0u) {
        expr_t *one = expr_new_const(NUM_ONE);
        expr_t *one_minus = expr_sub(one, dv->b);
        expr_t *den = expr_pow_long(one_minus, 2);

        factor = expr_div(one, den);
        expr_free(one);
        expr_free(one_minus);
        expr_free(den);
    } else {
        expr_t *prev = expr_polylog(order - 1u, dv->b);

        factor = expr_div(prev, dv->b);
        expr_free(prev);
    }

    db = expr_get_dx_internal(dv->b);
    out = expr_mul(factor, db);
    expr_free(factor);
    expr_free(db);
    return out;
}

expr_t *deriv_harmonic_poly(expr_t *dv)
{
    expr_t *degree_derivative = expr_get_dx_internal(dv->a);
    number_t degree_derivative_value = degree_derivative ? expr_eval_num_internal(degree_derivative) : NUM_NAN;
    expr_t *one = NULL;
    expr_t *power = NULL;
    expr_t *numerator = NULL;
    expr_t *denominator = NULL;
    expr_t *factor = NULL;
    expr_t *argument_derivative = NULL;
    expr_t *out = NULL;

    if (!num_is_zero(degree_derivative_value)) {
        num_destroy(&degree_derivative_value);
        expr_free(degree_derivative);
        return expr_new_const(NUM_NAN);
    }
    num_destroy(&degree_derivative_value);
    expr_free(degree_derivative);

    one = expr_new_const(NUM_ONE);
    power = one ? expr_pow_xp(dv->b, dv->a) : NULL;
    numerator = one && power ? expr_sub(one, power) : NULL;
    denominator = one ? expr_sub(one, dv->b) : NULL;
    factor = numerator && denominator ? expr_div(numerator, denominator) : NULL;
    argument_derivative = expr_get_dx_internal(dv->b);
    out = factor && argument_derivative ? expr_mul(factor, argument_derivative) : NULL;

    expr_free(argument_derivative);
    expr_free(factor);
    expr_free(denominator);
    expr_free(numerator);
    expr_free(power);
    expr_free(one);
    return out;
}

expr_t *deriv_legendre_chi(expr_t *dv)
{
    number_t order_value = expr_eval_num_internal(dv->a);
    unsigned int order;
    expr_t *factor = NULL;
    expr_t *db;
    expr_t *out;

    if (!expr_number_to_polygamma_order(order_value, &order))
        return expr_new_const(NUM_NAN);

    if (order == 0u) {
        expr_t *one = expr_new_const(NUM_ONE);
        expr_t *z_sq = expr_pow_long(dv->b, 2);
        expr_t *one_plus_z_sq = expr_add(one, z_sq);
        expr_t *one_minus_z_sq = expr_sub(one, z_sq);
        expr_t *den = expr_pow_long(one_minus_z_sq, 2);

        factor = expr_div(one_plus_z_sq, den);
        expr_free(one);
        expr_free(z_sq);
        expr_free(one_plus_z_sq);
        expr_free(one_minus_z_sq);
        expr_free(den);
    } else {
        expr_t *prev = expr_legendre_chi(order - 1u, dv->b);

        factor = expr_div(prev, dv->b);
        expr_free(prev);
    }

    db = expr_get_dx_internal(dv->b);
    out = expr_mul(factor, db);
    expr_free(factor);
    expr_free(db);
    return out;
}

expr_t *deriv_appell_f1_pack(expr_t *dv)
{
    (void)dv;
    return expr_new_const(NUM_NAN);
}

static bool expr_derivative_is_zero_local(const expr_t *expr)
{
    expr_t *dx = expr_get_dx_internal(expr);
    expr_t *simplified = dx ? expr_simplify(dx) : NULL;
    bool zero = simplified && expr_is_const(simplified) && num_is_zero(simplified->c);

    expr_free(simplified);
    expr_free(dx);
    return zero;
}

static expr_t *deriv_bessel_argument(expr_t *dv, expr_t *(*builder)(const expr_t *, const expr_t *))
{
    expr_t *one = NULL;
    expr_t *half = NULL;
    expr_t *lower_order = NULL;
    expr_t *upper_order = NULL;
    expr_t *lower = NULL;
    expr_t *upper = NULL;
    expr_t *difference = NULL;
    expr_t *factor = NULL;
    expr_t *argument_derivative = NULL;
    expr_t *out = NULL;

    if (!dv || !dv->a || !dv->b || !builder || !expr_derivative_is_zero_local(dv->a))
        return expr_new_const(NUM_NAN);

    one = expr_new_const(NUM_ONE);
    half = expr_new_const(NUM_HALF);
    lower_order = expr_sub(dv->a, one);
    upper_order = expr_add(dv->a, one);
    lower = builder(lower_order, dv->b);
    upper = builder(upper_order, dv->b);
    difference = lower && upper ? expr_sub(lower, upper) : NULL;
    factor = difference ? expr_mul(half, difference) : NULL;
    argument_derivative = expr_get_dx_internal(dv->b);
    out = factor && argument_derivative ? expr_mul(factor, argument_derivative) : NULL;

    expr_free(argument_derivative);
    expr_free(factor);
    expr_free(difference);
    expr_free(upper);
    expr_free(lower);
    expr_free(upper_order);
    expr_free(lower_order);
    expr_free(half);
    expr_free(one);
    return out ? out : expr_new_const(NUM_NAN);
}

expr_t *deriv_bessel_j(expr_t *dv)
{
    return deriv_bessel_argument(dv, expr_bessel_j);
}

expr_t *deriv_bessel_y(expr_t *dv)
{
    return deriv_bessel_argument(dv, expr_bessel_y);
}

expr_t *deriv_lommel_s_pack(expr_t *dv)
{
    (void)dv;
    return expr_new_const(NUM_NAN);
}

expr_t *deriv_lommel_s(expr_t *dv)
{
    const expr_t *mu = NULL;
    const expr_t *nu = NULL;
    const expr_t *argument = NULL;
    expr_t *factor = NULL;
    expr_t *argument_derivative = NULL;
    expr_t *out = NULL;

    if (!expr_lommel_s_unpack(dv, &mu, &nu, &argument) || !expr_derivative_is_zero_local(mu) ||
        !expr_derivative_is_zero_local(nu))
        return expr_new_const(NUM_NAN);

    factor = expr_lommel_s_argument_derivative_expansion(mu, nu, argument);
    argument_derivative = expr_get_dx_internal(argument);
    out = factor && argument_derivative ? expr_mul(factor, argument_derivative) : NULL;

    expr_free(argument_derivative);
    expr_free(factor);
    return out ? out : expr_new_const(NUM_NAN);
}

expr_t *deriv_appell_f1(expr_t *dv)
{
    const expr_t *a = NULL;
    const expr_t *b1 = NULL;
    const expr_t *b2 = NULL;
    const expr_t *c = NULL;
    const expr_t *x = NULL;
    const expr_t *y = NULL;
    const expr_t *parameters[2];
    const expr_t *variables[2];
    expr_t *lauricella = NULL;
    expr_t *out = NULL;

    if (!expr_appell_f1_unpack(dv, &a, &b1, &b2, &c, &x, &y))
        return expr_new_const(NUM_NAN);
    parameters[0] = b1;
    parameters[1] = b2;
    variables[0] = x;
    variables[1] = y;
    lauricella = expr_lauricella_f(a, 2u, parameters, c, variables);
    out = lauricella ? expr_get_dx_internal(lauricella) : NULL;
    expr_free(lauricella);
    return out ? out : expr_new_const(NUM_NAN);
}

expr_t *deriv_lauricella_f(expr_t *dv)
{
    const expr_t *a = NULL;
    const expr_t **b = NULL;
    const expr_t *c = NULL;
    const expr_t **x = NULL;
    const expr_t **shifted_b = NULL;
    expr_t *a1 = NULL;
    expr_t *c1 = NULL;
    expr_t *sum = NULL;
    expr_t *out = NULL;
    size_t variable_count = 0u;

    if (!expr_lauricella_f_unpack(dv, &a, &b, &c, &x, &variable_count) || !expr_derivative_is_zero_local(a) ||
        !expr_derivative_is_zero_local(c))
        goto cleanup;
    for (size_t i = 0u; i < variable_count; ++i) {
        if (!expr_derivative_is_zero_local(b[i]))
            goto cleanup;
    }
    if (variable_count == 0u) {
        out = expr_new_const(NUM_ZERO);
        goto cleanup;
    }
    shifted_b = calloc(variable_count, sizeof(*shifted_b));
    a1 = expr_add_long(a, 1);
    c1 = expr_add_long(c, 1);
    if (!shifted_b || !a1 || !c1)
        goto cleanup;
    for (size_t i = 0u; i < variable_count; ++i) {
        expr_t *bi1 = NULL;
        expr_t *shifted = NULL;
        expr_t *ab = NULL;
        expr_t *coefficient = NULL;
        expr_t *factor = NULL;
        expr_t *dx = NULL;
        expr_t *term = NULL;
        expr_t *next_sum = NULL;

        for (size_t j = 0u; j < variable_count; ++j)
            shifted_b[j] = b[j];
        bi1 = expr_add_long(b[i], 1);
        shifted_b[i] = bi1;
        shifted = bi1 ? expr_lauricella_f(a1, variable_count, shifted_b, c1, x) : NULL;
        ab = expr_mul(a, b[i]);
        coefficient = ab ? expr_div(ab, c) : NULL;
        factor = coefficient && shifted ? expr_mul(coefficient, shifted) : NULL;
        dx = expr_get_dx_internal(x[i]);
        term = factor && dx ? expr_mul(factor, dx) : NULL;
        if (term) {
            next_sum = sum ? expr_add(sum, term) : expr_clone(term);
            expr_free(sum);
            sum = next_sum;
        }
        expr_free(term);
        expr_free(dx);
        expr_free(factor);
        expr_free(coefficient);
        expr_free(ab);
        expr_free(shifted);
        expr_free(bi1);
    }
    out = sum ? sum : expr_new_const(NUM_ZERO);
    sum = NULL;

cleanup:
    expr_free(sum);
    expr_free(c1);
    expr_free(a1);
    free(shifted_b);
    free(x);
    free(b);
    return out ? out : expr_new_const(NUM_NAN);
}

expr_t *deriv_hypergeometric_pFq_pack(expr_t *dv)
{
    (void)dv;
    return expr_new_const(NUM_ZERO);
}

expr_t *deriv_hypergeometric_pFq(expr_t *dv)
{
    const expr_t **upper = NULL;
    const expr_t **lower = NULL;
    const expr_t *argument = NULL;
    expr_t **upper_shifted = NULL;
    expr_t **lower_shifted = NULL;
    expr_t *argument_derivative = NULL;
    expr_t *numerator = NULL;
    expr_t *denominator = NULL;
    expr_t *coefficient = NULL;
    expr_t *shifted = NULL;
    expr_t *factor = NULL;
    expr_t *out = NULL;
    size_t p = 0u;
    size_t q = 0u;

    if (!expr_hypergeometric_pFq_unpack(dv, &upper, &p, &lower, &q, &argument))
        return expr_new_const(NUM_NAN);
    for (size_t i = 0u; i < p; ++i) {
        if (!expr_derivative_is_zero_local(upper[i]))
            goto cleanup;
    }
    for (size_t i = 0u; i < q; ++i) {
        if (!expr_derivative_is_zero_local(lower[i]))
            goto cleanup;
    }

    if (p > 0u) {
        upper_shifted = calloc(p, sizeof(*upper_shifted));
        if (!upper_shifted)
            goto cleanup;
    }
    if (q > 0u) {
        lower_shifted = calloc(q, sizeof(*lower_shifted));
        if (!lower_shifted)
            goto cleanup;
    }

    argument_derivative = expr_get_dx_internal(argument);
    numerator = expr_new_const(NUM_ONE);
    denominator = expr_new_const(NUM_ONE);
    for (size_t i = 0u; i < p; ++i) {
        expr_t *next;

        upper_shifted[i] = expr_add_long(upper[i], 1);
        next = numerator ? expr_mul(numerator, upper[i]) : NULL;
        expr_free(numerator);
        numerator = next;
    }
    for (size_t i = 0u; i < q; ++i) {
        expr_t *next;

        lower_shifted[i] = expr_add_long(lower[i], 1);
        next = denominator ? expr_mul(denominator, lower[i]) : NULL;
        expr_free(denominator);
        denominator = next;
    }
    coefficient = numerator && denominator ? expr_div(numerator, denominator) : NULL;
    shifted = expr_hypergeometric_pFq(p, (const expr_t *const *)upper_shifted, q, (const expr_t *const *)lower_shifted,
                                      argument);
    factor = coefficient && shifted ? expr_mul(coefficient, shifted) : NULL;
    out = factor && argument_derivative ? expr_mul(factor, argument_derivative) : NULL;

cleanup:
    expr_free(factor);
    expr_free(shifted);
    expr_free(coefficient);
    expr_free(denominator);
    expr_free(numerator);
    expr_free(argument_derivative);
    for (size_t i = 0u; i < q; ++i)
        expr_free(lower_shifted ? lower_shifted[i] : NULL);
    for (size_t i = 0u; i < p; ++i)
        expr_free(upper_shifted ? upper_shifted[i] : NULL);
    free(lower_shifted);
    free(upper_shifted);
    free(lower);
    free(upper);
    return out ? out : expr_new_const(NUM_NAN);
}

expr_t *deriv_gammainv(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *y = expr_gammainv(dv->a);
    expr_t *psi = expr_digamma(y);
    expr_t *xpsi = expr_mul(dv->a, psi);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *fac = expr_div(one, xpsi);
    expr_t *out = expr_mul(fac, da);
    expr_free(da);
    expr_free(y);
    expr_free(psi);
    expr_free(xpsi);
    expr_free(one);
    expr_free(fac);
    return out;
}

expr_t *deriv_lambert_w0(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *w = expr_lambert_w0(dv->a);
    expr_t *wp1 = expr_add_long(w, 1);
    expr_t *den = expr_mul(dv->a, wp1);
    expr_t *fac = expr_div(w, den);
    expr_t *out = expr_mul(fac, da);
    expr_free(da);
    expr_free(w);
    expr_free(wp1);
    expr_free(den);
    expr_free(fac);
    return out;
}

expr_t *deriv_lambert_w(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *w = expr_lambert_w(dv->a);
    expr_t *wp1 = expr_add_long(w, 1);
    expr_t *den = expr_mul(dv->a, wp1);
    expr_t *fac = expr_div(w, den);
    expr_t *out = expr_mul(fac, da);
    expr_free(da);
    expr_free(w);
    expr_free(wp1);
    expr_free(den);
    expr_free(fac);
    return out;
}

expr_t *deriv_lambert_wn(expr_t *dv)
{
    expr_t *db = expr_get_dx_internal(dv->b);
    expr_t *w = expr_lambert_wn_xp(dv->a, dv->b);
    expr_t *wp1 = expr_add_long(w, 1);
    expr_t *den = expr_mul(dv->b, wp1);
    expr_t *fac = expr_div(w, den);
    expr_t *out = expr_mul(fac, db);
    expr_free(db);
    expr_free(w);
    expr_free(wp1);
    expr_free(den);
    expr_free(fac);
    return out;
}

expr_t *deriv_lambert_wm1(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *w = expr_lambert_wm1(dv->a);
    expr_t *wp1 = expr_add_long(w, 1);
    expr_t *den = expr_mul(dv->a, wp1);
    expr_t *fac = expr_div(w, den);
    expr_t *out = expr_mul(fac, da);
    expr_free(da);
    expr_free(w);
    expr_free(wp1);
    expr_free(den);
    expr_free(fac);
    return out;
}

expr_t *deriv_normal_pdf(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *neg_a = expr_neg(dv->a);
    expr_t *phi = expr_normal_pdf(dv->a);
    expr_t *fac = expr_mul(neg_a, phi);
    expr_t *out = expr_mul(fac, da);
    expr_free(da);
    expr_free(neg_a);
    expr_free(phi);
    expr_free(fac);
    return out;
}

expr_t *deriv_normal_cdf(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_normal_pdf(dv->a));
}

expr_t *deriv_normal_logpdf(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_neg(dv->a));
}

expr_t *deriv_pdf(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *neg_a = expr_neg(dv->a);
    expr_t *phi = expr_pdf(dv->a);
    expr_t *fac = expr_mul(neg_a, phi);
    expr_t *out = expr_mul(fac, da);
    expr_free(da);
    expr_free(neg_a);
    expr_free(phi);
    expr_free(fac);
    return out;
}

expr_t *deriv_cdf(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_pdf(dv->a));
}

expr_t *deriv_logpdf(expr_t *dv)
{
    return deriv_normal_logpdf(dv);
}

expr_t *deriv_Ei(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *ea = expr_exp(dv->a);
    expr_t *fac = expr_div(ea, dv->a);
    expr_t *out = expr_mul(fac, da);
    expr_free(da);
    expr_free(ea);
    expr_free(fac);
    return out;
}

expr_t *deriv_E1(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *neg_a = expr_neg(dv->a);
    expr_t *en_a = expr_exp(neg_a);
    expr_t *neg_en = expr_neg(en_a);
    expr_t *fac = expr_div(neg_en, dv->a);
    expr_t *out = expr_mul(fac, da);
    expr_free(da);
    expr_free(neg_a);
    expr_free(en_a);
    expr_free(neg_en);
    expr_free(fac);
    return out;
}

expr_t *deriv_beta(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *db = expr_get_dx_internal(dv->b);
    expr_t *apb = expr_add(dv->a, dv->b);
    expr_t *dg_a = expr_digamma(dv->a);
    expr_t *dg_b = expr_digamma(dv->b);
    expr_t *dg_ab = expr_digamma(apb);
    expr_t *diff_a = expr_sub(dg_a, dg_ab);
    expr_t *diff_b = expr_sub(dg_b, dg_ab);
    expr_t *beta_n = expr_beta(dv->a, dv->b);
    expr_t *ca = expr_mul(beta_n, diff_a);
    expr_t *cb = expr_mul(beta_n, diff_b);
    expr_t *ta = expr_mul(ca, da);
    expr_t *tb = expr_mul(cb, db);
    expr_t *out = expr_add(ta, tb);
    expr_free(da);
    expr_free(db);
    expr_free(apb);
    expr_free(dg_a);
    expr_free(dg_b);
    expr_free(dg_ab);
    expr_free(diff_a);
    expr_free(diff_b);
    expr_free(beta_n);
    expr_free(ca);
    expr_free(cb);
    expr_free(ta);
    expr_free(tb);
    return out;
}

expr_t *deriv_logbeta(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *db = expr_get_dx_internal(dv->b);
    expr_t *apb = expr_add(dv->a, dv->b);
    expr_t *dg_a = expr_digamma(dv->a);
    expr_t *dg_b = expr_digamma(dv->b);
    expr_t *dg_ab = expr_digamma(apb);
    expr_t *diff_a = expr_sub(dg_a, dg_ab);
    expr_t *diff_b = expr_sub(dg_b, dg_ab);
    expr_t *ta = expr_mul(diff_a, da);
    expr_t *tb = expr_mul(diff_b, db);
    expr_t *out = expr_add(ta, tb);
    expr_free(da);
    expr_free(db);
    expr_free(apb);
    expr_free(dg_a);
    expr_free(dg_b);
    expr_free(dg_ab);
    expr_free(diff_a);
    expr_free(diff_b);
    expr_free(ta);
    expr_free(tb);
    return out;
}

static expr_t *gammainc_x_density(const expr_t *s, const expr_t *x)
{
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *s_minus_one = expr_sub(s, one);
    expr_t *x_pow = expr_pow_xp(x, s_minus_one);
    expr_t *neg_x = expr_neg(x);
    expr_t *exp_neg_x = expr_exp(neg_x);
    expr_t *density = expr_mul(x_pow, exp_neg_x);

    expr_free(one);
    expr_free(s_minus_one);
    expr_free(x_pow);
    expr_free(neg_x);
    expr_free(exp_neg_x);
    return density;
}

static expr_t *deriv_gammainc_x_only(expr_t *dv, int sign, int regularised)
{
    expr_t *ds = expr_get_dx_internal(dv->a);
    expr_t *dx = expr_get_dx_internal(dv->b);
    expr_t *density;
    expr_t *factor;
    expr_t *out;

    if (!expr_const_is_zero(ds)) {
        expr_free(ds);
        expr_free(dx);
        return expr_new_const(NUM_NAN);
    }

    density = gammainc_x_density(dv->a, dv->b);
    factor = density;

    if (regularised) {
        expr_t *gamma_s = expr_gamma(dv->a);
        factor = expr_div(density, gamma_s);
        expr_free(density);
        expr_free(gamma_s);
    }

    if (sign < 0) {
        expr_t *neg = expr_neg(factor);
        expr_free(factor);
        factor = neg;
    }

    out = expr_mul(factor, dx);
    expr_free(ds);
    expr_free(dx);
    expr_free(factor);
    return out;
}

expr_t *deriv_gammainc_lower(expr_t *dv)
{
    return deriv_gammainc_x_only(dv, 1, 0);
}

expr_t *deriv_gammainc_upper(expr_t *dv)
{
    return deriv_gammainc_x_only(dv, -1, 0);
}

expr_t *deriv_gammainc_P(expr_t *dv)
{
    return deriv_gammainc_x_only(dv, 1, 1);
}

expr_t *deriv_gammainc_Q(expr_t *dv)
{
    return deriv_gammainc_x_only(dv, -1, 1);
}
