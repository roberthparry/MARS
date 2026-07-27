#include <stdio.h>
#include <stdlib.h>

#include <limits.h>

#define MARS_INTEGRATOR_INTERNAL_ACCESS
#include "integrator_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"

typedef struct {
    number_t a;
    number_t b;
    number_t result;
    number_t error;
} mp_subinterval_t;

typedef int (*mp_eval_fn)(void *ctx, const number_t x, number_t *out);

typedef struct {
    expr_t *expr;
    expr_t *x_var;
} mp_expr_ctx_t;

typedef struct {
    integrator_t *ig;
    expr_t *expr;
    size_t ndim;
    expr_t * const *vars;
    const number_t *lo;
    const number_t *hi;
} mp_multi_ctx_t;

static void mp_subinterval_clear(mp_subinterval_t *interval)
{
    if (!interval)
        return;
    num_destroy(&interval->a);
    num_destroy(&interval->b);
    num_destroy(&interval->result);
    num_destroy(&interval->error);
}

static void intg_expr_set_val_num(expr_t *dv, number_t x)
{
    expr_set_val(dv, x);
}

static int intg_expr_eval_num_real(const expr_t *dv, number_t *out)
{
    number_t value;

    if (!out)
        return -1;

    value = expr_eval(dv);
    if (!num_is_real(value) || !num_is_finite(value)) {
        num_destroy(&value);
        return -1;
    }

    *out = value;
    return 0;
}

static number_t mp_midpoint(const number_t a, const number_t b)
{
    number_t sum = num_add(a, b);
    number_t mid = num_mul(sum, NUM_HALF);

    num_destroy(&sum);
    return mid;
}

static number_t mp_num_from_decimal(const char *text)
{
    return num_create_from_string(text);
}

static int mp_gk_pair_index(size_t kronrod_index)
{
    switch (kronrod_index) {
        case 1u: return 0;
        case 3u: return 1;
        case 5u: return 2;
        case 7u: return 3;
        default: return -1;
    }
}

static int mp_eval_at(expr_t *expr, expr_t *x_var, const number_t x, number_t *out)
{
    if (!expr || !x_var || !out)
        return -1;

    intg_expr_set_val_num(x_var, x);
    return intg_expr_eval_num_real(expr, out);
}

static int mp_eval_expr_ctx(void *ctx, const number_t x, number_t *out)
{
    mp_expr_ctx_t *expr_ctx = (mp_expr_ctx_t *)ctx;

    if (!expr_ctx)
        return -1;
    return mp_eval_at(expr_ctx->expr, expr_ctx->x_var, x, out);
}

static int mp_interval_init_eval(mp_eval_fn eval, void *ctx,
                                 const number_t a, const number_t b,
                                 mp_subinterval_t *out)
{
    static const char *xgk_text[8] = {
        "0.991455371120812639206854697526329",
        "0.949107912342758524526189684047851",
        "0.864864423359769072789712788640926",
        "0.741531185599394439863864773280788",
        "0.586087235467691130294144838258730",
        "0.405845151377397166906606412076961",
        "0.207784955007898467600689403773245",
        "0"
    };
    static const char *wg_text[4] = {
        "0.129484966168869693270611432679082",
        "0.279705391489276667901467771423780",
        "0.381830050505118944950369775488975",
        "0.417959183673469387755102040816327"
    };
    static const char *wgk_text[8] = {
        "0.022935322010529224963732008058970",
        "0.063092092629978553290700663189204",
        "0.104790010322250183839876322541518",
        "0.140653259715525918745189590510238",
        "0.169004726639267902826583426598550",
        "0.190350578064785409913256402421014",
        "0.204432940075298892414161999234649",
        "0.209482141084727828012999174891714"
    };
    number_t center = num_new();
    number_t half = num_new();
    number_t half_abs = num_new();
    number_t kronrod_sum = num_clone(NUM_ZERO);
    number_t gauss_sum = num_clone(NUM_ZERO);
    number_t f_center = num_new();
    number_t center_weight = num_new();
    number_t center_gauss_weight = num_new();
    number_t center_term = num_new();
    number_t center_gauss_term = num_new();
    number_t diff = num_new();
    number_t result = num_new();
    number_t error = num_new();
    int rc = -1;

    if (!eval || !out)
        return -1;

    center = mp_midpoint(a, b);
    half = num_mul(num_sub(b, a), NUM_HALF);
    half_abs = num_abs(half);

    if (eval(ctx, center, &f_center) != 0)
        goto cleanup;

    center_weight = mp_num_from_decimal(wgk_text[7]);
    center_gauss_weight = mp_num_from_decimal(wg_text[3]);
    center_term = num_mul(center_weight, f_center);
    center_gauss_term = num_mul(center_gauss_weight, f_center);
    num_destroy(&kronrod_sum);
    kronrod_sum = center_term;
    center_term = num_new();
    num_destroy(&gauss_sum);
    gauss_sum = center_gauss_term;
    center_gauss_term = num_new();

    for (size_t i = 0u; i < 7u; ++i) {
        number_t node = mp_num_from_decimal(xgk_text[i]);
        number_t wk = mp_num_from_decimal(wgk_text[i]);
        number_t offset = num_mul(half, node);
        number_t x_left = num_sub(center, offset);
        number_t x_right = num_add(center, offset);
        number_t f_left = num_new();
        number_t f_right = num_new();
        number_t f_sum = num_new();
        number_t k_term = num_new();

        if (eval(ctx, x_left, &f_left) != 0 ||
            eval(ctx, x_right, &f_right) != 0) {
            num_destroy(&k_term);
            num_destroy(&f_sum);
            num_destroy(&f_right);
            num_destroy(&f_left);
            num_destroy(&x_right);
            num_destroy(&x_left);
            num_destroy(&offset);
            num_destroy(&wk);
            num_destroy(&node);
            goto cleanup;
        }

        f_sum = num_add(f_left, f_right);
        k_term = num_mul(wk, f_sum);
        {
            number_t next_k = num_add(kronrod_sum, k_term);
            num_destroy(&kronrod_sum);
            kronrod_sum = next_k;
        }

        {
            int gauss_index = mp_gk_pair_index(i);
            if (gauss_index >= 0) {
                number_t wg = mp_num_from_decimal(wg_text[(size_t)gauss_index]);
                number_t g_term = num_mul(wg, f_sum);
                number_t next_g = num_add(gauss_sum, g_term);

                num_destroy(&gauss_sum);
                gauss_sum = next_g;
                num_destroy(&g_term);
                num_destroy(&wg);
            }
        }

        num_destroy(&k_term);
        num_destroy(&f_sum);
        num_destroy(&f_right);
        num_destroy(&f_left);
        num_destroy(&x_right);
        num_destroy(&x_left);
        num_destroy(&offset);
        num_destroy(&wk);
        num_destroy(&node);
    }

    result = num_mul(kronrod_sum, half);
    diff = num_sub(kronrod_sum, gauss_sum);
    error = num_mul(num_abs(diff), half_abs);

    out->a = num_clone(a);
    out->b = num_clone(b);
    out->result = result;
    out->error = error;
    result = num_new();
    error = num_new();
    rc = 0;

cleanup:
    num_destroy(&error);
    num_destroy(&result);
    num_destroy(&diff);
    num_destroy(&center_gauss_term);
    num_destroy(&center_term);
    num_destroy(&center_gauss_weight);
    num_destroy(&center_weight);
    num_destroy(&f_center);
    num_destroy(&gauss_sum);
    num_destroy(&kronrod_sum);
    num_destroy(&half_abs);
    num_destroy(&half);
    num_destroy(&center);
    return rc;
}

static int mp_interval_split_eval(mp_eval_fn eval, void *ctx,
                             const mp_subinterval_t *src,
                             mp_subinterval_t *left,
                             mp_subinterval_t *right)
{
    number_t m = num_new();
    int rc = -1;

    if (!eval || !src || !left || !right)
        return -1;

    m = mp_midpoint(src->a, src->b);
    if (mp_interval_init_eval(eval, ctx, src->a, m, left) != 0 ||
        mp_interval_init_eval(eval, ctx, m, src->b, right) != 0) {
        mp_subinterval_clear(left);
        mp_subinterval_clear(right);
        goto cleanup;
    }

    rc = 0;

cleanup:
    num_destroy(&m);
    return rc;
}

static number_t mp_tolerance_threshold(number_t abs_tol, number_t rel_tol,
                                       const number_t value)
{
    number_t threshold = num_clone(abs_tol);
    number_t abs_value = num_abs(value);
    number_t relative = num_mul(rel_tol, abs_value);

    if (num_gt(relative, threshold)) {
        num_destroy(&threshold);
        threshold = relative;
        relative = num_new();
    }

    num_destroy(&relative);
    num_destroy(&abs_value);
    return threshold;
}

static int mp_tanh_sinh_sum_for_h(mp_eval_fn eval, void *ctx,
                                  const number_t center,
                                  const number_t half_width,
                                  const number_t pi_over_two,
                                  const number_t h,
                                  const number_t term_tolerance,
                                  size_t min_steps,
                                  int max_steps,
                                  number_t *sum_out,
                                  number_t *last_term_out,
                                  size_t *used_steps_out)
{
    number_t sum = num_clone(NUM_ZERO);
    number_t last_term = num_clone(NUM_ZERO);
    size_t used_steps = 0u;
    int quiet_count = 0;

    if (!eval || !sum_out || !last_term_out)
        return -1;

    for (int k = 0; k <= max_steps; ++k) {
        number_t k_num = num_create_from_long((long)k);
        number_t t = num_mul(h, k_num);
        number_t sh = num_sinh(t);
        number_t scaled_sh = num_mul(pi_over_two, sh);
        number_t u = num_tanh(scaled_sh);
        number_t ch = num_cosh(t);
        number_t cosh_scaled = num_cosh(scaled_sh);
        number_t denom = num_sqr(cosh_scaled);
        number_t numerator = num_mul(pi_over_two, ch);
        number_t weight_core = num_div(numerator, denom);
        number_t width_weight = num_mul(weight_core, half_width);
        number_t weight = num_mul(width_weight, h);
        number_t x_offset = num_mul(half_width, u);
        number_t x_pos = num_add(center, x_offset);
        number_t x_neg = num_sub(center, x_offset);
        number_t f_pos = num_new();
        number_t f_neg = num_new();
        number_t contrib = num_new();
        number_t term_mag = num_new();
        int eval_ok;

        eval_ok = eval(ctx, x_pos, &f_pos) == 0;
        if (eval_ok && k > 0)
            eval_ok = eval(ctx, x_neg, &f_neg) == 0;

        if (!eval_ok) {
            num_destroy(&term_mag);
            num_destroy(&contrib);
            num_destroy(&f_neg);
            num_destroy(&f_pos);
            num_destroy(&x_neg);
            num_destroy(&x_pos);
            num_destroy(&x_offset);
            num_destroy(&weight);
            num_destroy(&width_weight);
            num_destroy(&weight_core);
            num_destroy(&numerator);
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
            num_destroy(&contrib);
            contrib = num_mul(weight, f_pos);
        } else {
            number_t pair_sum = num_add(f_pos, f_neg);

            num_destroy(&contrib);
            contrib = num_mul(weight, pair_sum);
            num_destroy(&pair_sum);
        }

        num_destroy(&term_mag);
        term_mag = num_abs(contrib);
        {
            number_t next_sum = num_add(sum, contrib);

            num_destroy(&sum);
            sum = next_sum;
        }
        num_destroy(&last_term);
        last_term = num_clone(term_mag);
        used_steps = (size_t)k + 1u;

        if (num_lt(term_mag, term_tolerance)) {
            quiet_count += 1;
            if (quiet_count >= 4 && used_steps >= min_steps) {
                num_destroy(&term_mag);
                num_destroy(&contrib);
                num_destroy(&f_neg);
                num_destroy(&f_pos);
                num_destroy(&x_neg);
                num_destroy(&x_pos);
                num_destroy(&x_offset);
                num_destroy(&weight);
                num_destroy(&width_weight);
                num_destroy(&weight_core);
                num_destroy(&numerator);
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
        num_destroy(&width_weight);
        num_destroy(&weight_core);
        num_destroy(&numerator);
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

static int mp_tanh_sinh_integral(mp_eval_fn eval, void *ctx,
                                 number_t a, number_t b,
                                 number_t abs_tol, number_t rel_tol,
                                 size_t max_intervals,
                                 number_t *result_out,
                                 number_t *error_out,
                                 size_t *steps_out)
{
    number_t ab_sum = num_add(a, b);
    number_t width = num_sub(b, a);
    number_t half_width = num_mul(width, NUM_HALF);
    number_t center = num_mul(ab_sum, NUM_HALF);
    number_t pi_over_two = num_const_prec_digits(NUM_PI_2,
        num_get_default_prec_digits() + 16u);
    number_t h = num_clone(NUM_ONE);
    number_t term_scale = num_create_from_string("1e-4");
    number_t term_tolerance = num_mul(abs_tol, term_scale);
    number_t sum = num_new();
    number_t previous_sum = num_new();
    number_t last_term = num_clone(NUM_ZERO);
    number_t error = num_new();
    number_t best_sum = num_new();
    number_t best_error = num_new();
    bool have_best = false;
    size_t used_steps = 0u;
    int max_steps = 256;
    int refine_limit = 18;
    int status = 1;

    if (!eval || !result_out || !error_out) {
        status = -1;
        goto cleanup;
    }

    if (max_intervals < 1000u) {
        max_steps = 128;
    } else if (max_intervals < 10000u) {
        max_steps = 512;
    } else {
        max_steps = 2048;
        /*
         * Large, precision-derived work budgets need a proportionally wider
         * transformed interval.  Keeping max_steps fixed at 2048 truncates
         * the tanh-sinh tails once h becomes small, so additional refinement
         * cannot improve the result.  Retain room for at least eight complete
         * refinement levels while growing the per-level range.
         */
        while (max_steps <= INT_MAX / 2) {
            size_t next_steps = (size_t)max_steps * 2u;

            if (next_steps > (SIZE_MAX - 1u) / 8u ||
                (next_steps + 1u) * 8u > max_intervals)
                break;
            max_steps *= 2;
        }
    }

    if (max_steps < 8)
        max_steps = 8;
    refine_limit = (int)((max_intervals + (size_t)max_steps) / ((size_t)max_steps + 1u)) + 2;
    if (refine_limit < 4)
        refine_limit = 4;

    for (int level = 0; level < refine_limit; ++level) {
        number_t level_sum = num_new();
        number_t level_last_term = num_new();
        number_t threshold = num_new();
        size_t level_steps = 0u;
        size_t remaining_steps = used_steps < max_intervals ? max_intervals - used_steps : 0u;
        int level_max_steps;
        size_t level_min_steps = remaining_steps < ((size_t)max_steps + 1u)
            ? remaining_steps
            : ((size_t)max_steps + 1u);
        if (remaining_steps == 0u) {
            num_destroy(&threshold);
            num_destroy(&level_last_term);
            num_destroy(&level_sum);
            break;
        }
        level_max_steps = max_steps;
        if (remaining_steps <= (size_t)level_max_steps)
            level_max_steps = (int)remaining_steps - 1;
        int sum_status = mp_tanh_sinh_sum_for_h(eval, ctx, center, half_width,
                                                pi_over_two, h,
                                                term_tolerance, level_min_steps,
                                                level_max_steps,
                                                &level_sum, &level_last_term,
                                                &level_steps);

        if (sum_status < 0) {
            num_destroy(&threshold);
            num_destroy(&level_last_term);
            num_destroy(&level_sum);
            status = -1;
            goto cleanup;
        }

        num_destroy(&sum);
        sum = level_sum;
        num_destroy(&last_term);
        last_term = level_last_term;
        used_steps += level_steps;

        if (!num_is_nan(previous_sum)) {
            number_t delta_raw = num_sub(sum, previous_sum);
            number_t delta = num_abs(delta_raw);

            num_destroy(&error);
            error = num_gt(delta, last_term) ? num_clone(delta) : num_clone(last_term);
            num_destroy(&threshold);
            threshold = mp_tolerance_threshold(abs_tol, rel_tol, sum);

            if (!have_best || num_lt(error, best_error)) {
                num_destroy(&best_sum);
                best_sum = num_clone(sum);
                num_destroy(&best_error);
                best_error = num_clone(error);
                have_best = true;
            }

            if (num_le(error, threshold)) {
                status = 0;
            }

            num_destroy(&delta);
            num_destroy(&delta_raw);
        }

        num_destroy(&threshold);
        if (status == 0)
            break;
        if (used_steps >= max_intervals)
            break;

        num_destroy(&previous_sum);
        previous_sum = num_clone(sum);

        {
            number_t next_h = num_mul(h, NUM_HALF);

            num_destroy(&h);
            h = next_h;
        }

        if (sum_status > 0 && used_steps >= max_intervals)
            break;
    }

    if (num_is_nan(error)) {
        num_destroy(&error);
        error = num_clone(last_term);
    }

    *result_out = num_clone(have_best ? best_sum : sum);
    *error_out = num_clone(have_best ? best_error : error);
    if (steps_out)
        *steps_out = used_steps > 0u ? used_steps : 1u;

cleanup:
    num_destroy(&best_error);
    num_destroy(&best_sum);
    num_destroy(&error);
    num_destroy(&last_term);
    num_destroy(&previous_sum);
    num_destroy(&sum);
    num_destroy(&term_tolerance);
    num_destroy(&term_scale);
    num_destroy(&h);
    num_destroy(&pi_over_two);
    num_destroy(&center);
    num_destroy(&half_width);
    num_destroy(&width);
    num_destroy(&ab_sum);
    return status;
}

static number_t mp_default_tolerance_from_precision(void)
{
    char text[64];
    size_t digits = num_get_default_prec_digits();
    size_t target_digits = digits > 20u ? digits - 20u : digits;

    if (target_digits < 27u)
        target_digits = 27u;
    snprintf(text, sizeof(text), "1e-%zu", target_digits);
    return num_create_from_string(text);
}

static number_t mp_configured_tolerance(number_t configured)
{
    number_t default_tol = num_create_from_string("1e-27");
    number_t out;

    if (num_eq(configured, default_tol)) {
        num_destroy(&default_tol);
        return mp_default_tolerance_from_precision();
    }
    out = num_clone(configured);
    num_destroy(&default_tol);
    return out;
}

static size_t mp_guard_precision_bits(size_t user_bits)
{
    size_t extra = 160u;

    if (user_bits >= ((size_t)-1) - extra)
        return user_bits;
    return user_bits + extra;
}

static int mp_adaptive_integral(mp_eval_fn eval, void *ctx,
                                integrator_t *ig,
                                number_t a, number_t b,
                                number_t *result, number_t *error_est)
{
    mp_subinterval_t *intervals = NULL;
    number_t total = num_new();
    number_t total_err = num_new();
    number_t abs_tol = num_new();
    number_t rel_tol = num_new();
    number_t a_work = num_new();
    number_t b_work = num_new();
    size_t capacity = 64u;
    size_t count = 0u;
    int status = 0;
    size_t user_bits;
    size_t guard_bits;
    size_t user_digits;

    if (!eval || !ig || !result)
        return -1;
    if (!num_is_real(a) || !num_is_real(b))
        return -1;

    user_bits = num_get_default_prec_bits();
    user_digits = num_get_default_prec_digits();
    abs_tol = mp_configured_tolerance(ig->abs_tol);
    rel_tol = mp_configured_tolerance(ig->rel_tol);
    guard_bits = mp_guard_precision_bits(user_bits);
    if (guard_bits > user_bits && num_set_default_prec_bits(guard_bits) != 0) {
        num_destroy(&rel_tol);
        num_destroy(&abs_tol);
        return -1;
    }

    a_work = num_as_inexact_real_prec(a, guard_bits);
    b_work = num_as_inexact_real_prec(b, guard_bits);

    if (num_eq(a_work, b_work)) {
        *result = num_clone(NUM_ZERO);
        if (error_est)
            *error_est = num_clone(NUM_ZERO);
        ig->last_intervals = 1u;
        num_set_default_prec_bits(user_bits);
        num_destroy(&b_work);
        num_destroy(&a_work);
        return 0;
    }

    {
        number_t ts_result = num_new();
        number_t ts_error = num_new();
        size_t ts_steps = 0u;
        int ts_status = mp_tanh_sinh_integral(eval, ctx, a_work, b_work,
                                              abs_tol, rel_tol,
                                              ig->max_intervals,
                                              &ts_result, &ts_error,
                                              &ts_steps);

        if (ts_status == 0 || (ts_status > 0 && user_digits >= 80u &&
                               num_is_finite(ts_error) && !num_is_nan(ts_error))) {
            *result = ts_result;
            ts_result = num_new();
            if (num_set_prec_bits(result, user_bits) != 0) {
                num_destroy(&ts_error);
                num_destroy(&ts_result);
                goto fail;
            }
            if (error_est) {
                *error_est = ts_error;
                ts_error = num_new();
                if (num_set_prec_bits(error_est, user_bits) != 0) {
                    num_destroy(&ts_error);
                    num_destroy(&ts_result);
                    goto fail;
                }
            }
            ig->last_intervals = ts_steps > 0u ? ts_steps : 1u;
            num_destroy(&ts_error);
            num_destroy(&ts_result);
            if (ts_status > 0)
                status = 1;
            goto cleanup;
        }

        num_destroy(&ts_error);
        num_destroy(&ts_result);
    }

    intervals = calloc(capacity, sizeof(*intervals));
    if (!intervals)
        goto fail;

    if (mp_interval_init_eval(eval, ctx, a_work, b_work, &intervals[0]) != 0)
        goto fail;

    count = 1u;
    total = num_clone(intervals[0].result);
    total_err = num_clone(intervals[0].error);

    while (1) {
        number_t thresh = mp_tolerance_threshold(abs_tol, rel_tol, total);
        size_t worst = 0u;

        if (num_le(total_err, thresh)) {
            num_destroy(&thresh);
            break;
        }
        num_destroy(&thresh);

        if (count >= ig->max_intervals) {
            status = 1;
            break;
        }

        for (size_t i = 1u; i < count; ++i) {
            if (num_gt(intervals[i].error, intervals[worst].error))
                worst = i;
        }

        if (count >= capacity) {
            mp_subinterval_t *tmp;

            capacity *= 2u;
            tmp = realloc(intervals, capacity * sizeof(*intervals));
            if (!tmp)
                goto fail;
            intervals = tmp;
        }

        {
            mp_subinterval_t left = {0};
            mp_subinterval_t right = {0};
            number_t old_result = num_clone(intervals[worst].result);
            number_t old_error = num_clone(intervals[worst].error);
            number_t split_result = num_new();
            number_t split_error = num_new();
            number_t total_minus_old = num_new();
            number_t err_minus_old = num_new();
            number_t next_total = num_new();
            number_t next_total_err = num_new();

            if (mp_interval_split_eval(eval, ctx, &intervals[worst], &left, &right) != 0) {
                num_destroy(&next_total_err);
                num_destroy(&next_total);
                num_destroy(&err_minus_old);
                num_destroy(&total_minus_old);
                num_destroy(&split_error);
                num_destroy(&split_result);
                num_destroy(&old_error);
                num_destroy(&old_result);
                goto fail;
            }

            split_result = num_add(left.result, right.result);
            split_error = num_add(left.error, right.error);
            total_minus_old = num_sub(total, old_result);
            err_minus_old = num_sub(total_err, old_error);
            next_total = num_add(total_minus_old, split_result);
            next_total_err = num_add(err_minus_old, split_error);

            num_destroy(&total);
            num_destroy(&total_err);
            total = next_total;
            total_err = next_total_err;
            next_total = num_new();
            next_total_err = num_new();

            mp_subinterval_clear(&intervals[worst]);
            intervals[worst] = left;
            intervals[count] = right;
            count += 1u;

            num_destroy(&next_total_err);
            num_destroy(&next_total);
            num_destroy(&err_minus_old);
            num_destroy(&total_minus_old);
            num_destroy(&split_error);
            num_destroy(&split_result);
            num_destroy(&old_error);
            num_destroy(&old_result);
        }
    }

    *result = total;
    total = num_new();
    if (num_set_prec_bits(result, user_bits) != 0)
        goto fail;
    if (error_est) {
        *error_est = num_clone(total_err);
        if (num_set_prec_bits(error_est, user_bits) != 0)
            goto fail;
    }
    ig->last_intervals = count;
    goto cleanup;

fail:
    status = -1;

cleanup:
    num_set_default_prec_bits(user_bits);
    num_destroy(&b_work);
    num_destroy(&a_work);
    for (size_t i = 0u; i < count; ++i)
        mp_subinterval_clear(&intervals[i]);
    free(intervals);
    num_destroy(&rel_tol);
    num_destroy(&abs_tol);
    num_destroy(&total_err);
    num_destroy(&total);
    return status;
}

static int mp_eval_multi_outer(void *ctx, const number_t x, number_t *out);

int intg_integral(integrator_t *ig, expr_t *expr, expr_t *x_var,
                       number_t a, number_t b,
                       number_t *result, number_t *error_est)
{
    mp_expr_ctx_t expr_ctx;
    expr_t *vars[1];
    number_t lo[1];
    number_t hi[1];
    int fast_status;

    if (!ig || !expr || !x_var || !result)
        return -1;

    intg_clear_exact_result(ig);
    vars[0] = x_var;
    lo[0] = a;
    hi[0] = b;
    fast_status = try_integral_multi_special_affine(ig, expr, 1u, vars, lo, hi,
                                                    result, error_est);
    if (fast_status != 0)
        return fast_status > 0 ? 0 : -1;

    expr_ctx.expr = expr;
    expr_ctx.x_var = x_var;
    return mp_adaptive_integral(mp_eval_expr_ctx, &expr_ctx, ig, a, b, result, error_est);
}

static int mp_eval_multi_outer(void *ctx, const number_t x, number_t *out)
{
    mp_multi_ctx_t *multi = (mp_multi_ctx_t *)ctx;
    size_t outer;
    size_t saved_last_intervals;
    expr_t *saved_exact_result = NULL;

    if (!multi || !out || multi->ndim == 0)
        return -1;

    outer = multi->ndim - 1u;
    expr_set_val(multi->vars[outer], x);

    if (multi->ndim == 1u)
        return mp_eval_at(multi->expr, multi->vars[0], x, out);

    saved_last_intervals = multi->ig->last_intervals;
    saved_exact_result = expr_retain_expr(intg_get_exact_result(multi->ig));
    {
        int status = intg_integral_multi_num(multi->ig, multi->expr, outer, multi->vars,
                                           multi->lo, multi->hi, out, NULL);

        if (status < 0) {
            multi->ig->last_intervals = saved_last_intervals;
            intg_set_exact_result_owned(multi->ig, saved_exact_result);
            return -1;
        }
    }
    multi->ig->last_intervals = saved_last_intervals;
    intg_set_exact_result_owned(multi->ig, saved_exact_result);
    return 0;
}

int intg_integral_multi_num(integrator_t *ig, expr_t *expr,
                          size_t ndim, expr_t * const *vars,
                          const number_t *lo, const number_t *hi,
                          number_t *result, number_t *error_est)
{
    mp_multi_ctx_t multi;
    int fast_status;

    if (!ig || !expr || !vars || !lo || !hi || !result || ndim == 0)
        return -1;

    fast_status = try_integral_multi_special_affine(ig, expr, ndim, vars, lo, hi,
                                                    result, error_est);
    if (fast_status != 0)
        return fast_status > 0 ? 0 : -1;

    multi.ig = ig;
    multi.expr = expr;
    multi.ndim = ndim;
    multi.vars = vars;
    multi.lo = lo;
    multi.hi = hi;

    return mp_adaptive_integral(mp_eval_multi_outer, &multi, ig,
                                lo[ndim - 1u], hi[ndim - 1u],
                                result, error_est);
}
