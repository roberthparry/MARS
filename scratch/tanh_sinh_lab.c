#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression.h"
#include "number.h"

static char *wrap_expression(const char *raw_input)
{
    size_t n;
    char *wrapped_input;

    if (!raw_input || raw_input[0] == '{')
        return NULL;

    n = strlen(raw_input);
    wrapped_input = malloc(n + 5u);
    if (!wrapped_input)
        return NULL;

    memcpy(wrapped_input, "{ ", 2u);
    memcpy(wrapped_input + 2u, raw_input, n);
    memcpy(wrapped_input + 2u + n, " }", 3u);
    return wrapped_input;
}

static int parse_number_expression(const char *text, int precision, number_t *out)
{
    const char *input = text;
    char *wrapped_input = NULL;
    expr_t *expr = NULL;
    int rc = 1;

    if (!text || !out)
        return 1;

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 48u);

    wrapped_input = wrap_expression(text);
    if (wrapped_input)
        input = wrapped_input;

    expr = expr_from_string(input, NULL);
    if (!expr)
        goto cleanup;

    *out = expr_eval(expr);
    rc = 0;

cleanup:
    expr_free(expr);
    free(wrapped_input);
    return rc;
}

static void print_owned(const char *label, number_t value)
{
    char *text = num_to_string(value);

    printf("%-12s%s\n", label, text ? text : "(null)");
    free(text);
    num_destroy(&value);
}

static int eval_real_expr(expr_t *expr, expr_t *x_var, const number_t x, number_t *out)
{
    number_t value;

    if (!expr || !x_var || !out)
        return -1;
    expr_set_val(x_var, x);
    value = expr_eval(expr);
    if (!num_is_real(value) || !num_is_finite(value)) {
        num_destroy(&value);
        return -1;
    }
    *out = value;
    return 0;
}

static int tanh_sinh_sum_for_h(expr_t *expr, expr_t *x_var,
                               const number_t center, const number_t half_width,
                               const number_t pi_over_two, const number_t h,
                               const number_t term_tolerance, int max_steps,
                               number_t *sum_out, number_t *last_term_out,
                               int *used_steps_out)
{
    number_t sum = num_clone(NUM_ZERO);
    number_t last_term = num_clone(NUM_ZERO);
    int used_steps = 0;
    int quiet_count = 0;

    for (int k = 0; k <= max_steps; ++k) {
        number_t k_num = num_create_from_long((long)k);
        number_t t = num_mul(h, k_num);
        number_t sh = num_sinh(t);
        number_t scaled_sh = num_mul(pi_over_two, sh);
        number_t u = num_tanh(scaled_sh);
        number_t ch = num_cosh(t);
        number_t cosh_scaled = num_cosh(scaled_sh);
        number_t denom = num_sqr(cosh_scaled);
        number_t weight_core = num_div(num_mul(pi_over_two, ch), denom);
        number_t weight = num_mul(num_mul(weight_core, half_width), h);
        number_t x_offset = num_mul(half_width, u);
        number_t x_pos = num_add(center, x_offset);
        number_t x_neg = num_sub(center, x_offset);
        number_t f_pos = num_new();
        number_t f_neg = num_new();
        number_t contrib = num_new();
        number_t term_mag = num_new();

        if (eval_real_expr(expr, x_var, x_pos, &f_pos) != 0 ||
            (k > 0 && eval_real_expr(expr, x_var, x_neg, &f_neg) != 0)) {
            num_destroy(&term_mag);
            num_destroy(&contrib);
            num_destroy(&f_neg);
            num_destroy(&f_pos);
            num_destroy(&x_neg);
            num_destroy(&x_pos);
            num_destroy(&x_offset);
            num_destroy(&weight);
            num_destroy(&weight_core);
            num_destroy(&denom);
            num_destroy(&cosh_scaled);
            num_destroy(&ch);
            num_destroy(&u);
            num_destroy(&scaled_sh);
            num_destroy(&sh);
            num_destroy(&t);
            num_destroy(&k_num);
            num_destroy(&last_term);
            num_destroy(&sum);
            return -1;
        }

        if (k == 0) {
            contrib = num_mul(weight, f_pos);
        } else {
            number_t pair_sum = num_add(f_pos, f_neg);

            num_destroy(&contrib);
            contrib = num_mul(weight, pair_sum);
            num_destroy(&pair_sum);
        }

        term_mag = num_abs(contrib);
        {
            number_t next_sum = num_add(sum, contrib);

            num_destroy(&sum);
            sum = next_sum;
        }
        num_destroy(&last_term);
        last_term = num_clone(term_mag);
        used_steps = k;

        if (num_lt(term_mag, term_tolerance)) {
            quiet_count += 1;
            if (quiet_count >= 4) {
                num_destroy(&term_mag);
                num_destroy(&contrib);
                num_destroy(&f_neg);
                num_destroy(&f_pos);
                num_destroy(&x_neg);
                num_destroy(&x_pos);
                num_destroy(&x_offset);
                num_destroy(&weight);
                num_destroy(&weight_core);
                num_destroy(&denom);
                num_destroy(&cosh_scaled);
                num_destroy(&ch);
                num_destroy(&u);
                num_destroy(&scaled_sh);
                num_destroy(&sh);
                num_destroy(&t);
                num_destroy(&k_num);
                *sum_out = sum;
                *last_term_out = last_term;
                if (used_steps_out)
                    *used_steps_out = used_steps;
                return 0;
            }
        } else {
            quiet_count = 0;
        }

        num_destroy(&term_mag);
        num_destroy(&contrib);
        num_destroy(&f_neg);
        num_destroy(&f_pos);
        num_destroy(&x_neg);
        num_destroy(&x_pos);
        num_destroy(&x_offset);
        num_destroy(&weight);
        num_destroy(&weight_core);
        num_destroy(&denom);
        num_destroy(&cosh_scaled);
        num_destroy(&ch);
        num_destroy(&u);
        num_destroy(&scaled_sh);
        num_destroy(&sh);
        num_destroy(&t);
        num_destroy(&k_num);
    }

    *sum_out = sum;
    *last_term_out = last_term;
    if (used_steps_out)
        *used_steps_out = used_steps;
    return 1;
}

static int tanh_sinh_integrate(expr_t *expr, expr_t *x_var,
                               const number_t a, const number_t b,
                               int precision_digits, int max_steps,
                               number_t *result_out, number_t *last_term_out,
                               int *used_steps_out)
{
    number_t half = num_clone(NUM_HALF);
    number_t ab_sum = num_add(a, b);
    number_t half_width = num_mul(num_sub(b, a), half);
    number_t center = num_mul(ab_sum, half);
    number_t pi_over_two = num_const_prec_digits(NUM_PI_2, (size_t)precision_digits + 48u);
    number_t h = num_clone(NUM_ONE);
    number_t refine_tol = num_pow10(-(precision_digits > 0 ? precision_digits + 2 : 24));
    number_t term_tol = num_pow10(-(precision_digits > 0 ? precision_digits + 8 : 32));
    number_t sum = num_new();
    number_t last_term = num_clone(NUM_ZERO);
    number_t prev_sum = num_new();
    int used_steps = 0;
    int rc = -1;
    int refine_limit = precision_digits > 0 ? precision_digits + 12 : 48;

    num_destroy(&ab_sum);

    for (int level = 0; level < refine_limit; ++level) {
        number_t level_sum = num_new();
        number_t level_last_term = num_new();
        int level_steps = 0;
        int level_rc = tanh_sinh_sum_for_h(expr, x_var, center, half_width, pi_over_two,
                                           h, term_tol, max_steps, &level_sum,
                                           &level_last_term, &level_steps);

        if (level_rc < 0) {
            num_destroy(&level_last_term);
            num_destroy(&level_sum);
            goto cleanup;
        }

        num_destroy(&sum);
        sum = level_sum;
        num_destroy(&last_term);
        last_term = level_last_term;
        used_steps = level_steps;

        if (!num_is_nan(prev_sum)) {
            number_t delta = num_abs(num_sub(sum, prev_sum));

            if (num_lt(delta, refine_tol)) {
                num_destroy(&delta);
                rc = 0;
                break;
            }
            num_destroy(&delta);
        }

        num_destroy(&prev_sum);
        prev_sum = num_clone(sum);
        {
            number_t two = num_create_from_long(2L);
            number_t next_h = num_div(h, two);

            num_destroy(&two);
            num_destroy(&h);
            h = next_h;
        }

        if (level_rc > 0)
            break;
    }

    if (rc != 0)
        rc = 1;

cleanup:
    if (result_out)
        *result_out = num_clone(sum);
    if (last_term_out)
        *last_term_out = num_clone(last_term);
    if (used_steps_out)
        *used_steps_out = used_steps;
    num_destroy(&prev_sum);
    num_destroy(&last_term);
    num_destroy(&sum);
    num_destroy(&term_tol);
    num_destroy(&refine_tol);
    num_destroy(&h);
    num_destroy(&pi_over_two);
    num_destroy(&center);
    num_destroy(&half_width);
    num_destroy(&half);
    return rc;
}

int main(int argc, char **argv)
{
    const char *raw_input = argc > 1 ? argv[1] : "sin(x)";
    int precision = argc > 2 ? atoi(argv[2]) : 80;
    const char *var_name = argc > 3 ? argv[3] : "x";
    const char *lo_text = argc > 4 ? argv[4] : "0";
    const char *hi_text = argc > 5 ? argv[5] : "pi/2";
    int max_steps = argc > 6 ? atoi(argv[6]) : 256;
    const char *input = raw_input;
    char *wrapped_input = NULL;
    expr_bindings_t *bindings = NULL;
    expr_t *expr = NULL;
    expr_t *x_var = NULL;
    number_t a = num_new();
    number_t b = num_new();
    number_t result = num_new();
    number_t last_term = num_new();
    char *expr_text = NULL;
    int used_steps = 0;
    int rc = 1;
    int status;

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 48u);

    wrapped_input = wrap_expression(raw_input);
    if (wrapped_input)
        input = wrapped_input;

    expr = expr_from_string(input, &bindings);
    if (!expr || !bindings) {
        fprintf(stderr, "Could not parse integrand expression\n");
        goto cleanup;
    }

    x_var = expr_bindings_get(bindings, var_name);
    if (!x_var) {
        fprintf(stderr, "No binding named '%s'\n", var_name);
        goto cleanup;
    }

    if (parse_number_expression(lo_text, precision, &a) != 0 ||
        parse_number_expression(hi_text, precision, &b) != 0) {
        fprintf(stderr, "Could not parse bounds\n");
        goto cleanup;
    }

    expr_text = expr_to_string(expr, style_UNBOUND);
    status = tanh_sinh_integrate(expr, x_var, a, b, precision, max_steps,
                                 &result, &last_term, &used_steps);

    printf("input       %s\n", input);
    printf("expression  %s\n", expr_text ? expr_text : "(null)");
    printf("method      tanh-sinh\n");
    printf("bound       %s in [%s, %s]\n", var_name, lo_text, hi_text);
    print_owned("value       ", num_clone(result));
    print_owned("last_term   ", num_clone(last_term));
    printf("steps       %d\n", used_steps);
    printf("status      %s\n", status == 0 ? "converged" : "max steps reached");
    rc = 0;

cleanup:
    free(expr_text);
    num_destroy(&last_term);
    num_destroy(&result);
    num_destroy(&b);
    num_destroy(&a);
    expr_bindings_free(bindings);
    expr_free(expr);
    free(wrapped_input);
    return rc;
}
