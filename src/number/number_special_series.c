/**
 * @file number_special_series.c
 * @brief Precision-preserving defining series for pFq and the Lerch transcendent.
 *
 * Uses the widest participating number backend and guarded MPC arithmetic for real and complex series.
 * Bounded 1F1 parameters use additive ratio polynomials, real division and explicitly checked late-term precision.
 * Other bounded high-precision hypergeometric parameters use a balanced product tree with one final division;
 * real and complex trees retain the ordinary recurrence below their measured precision crossovers.
 * The same checked tree supplies two-factor cylinder sums and linearly weighted Lommel derivatives without
 * constructing and cancelling equal numerator and factorial factors at every leaf.
 * Moderate high-precision Lerch parameters first use weighted binomial blocks, with the defining series as fallback.
 * Geometric remainder bounds and accumulated rounding checks prevent a finite but under-resolved sum from being
 * accepted. Fixed-precision callers retain their existing kernels. Boundary continuation is not guessed.
 */
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

static size_t series_input_precision(number_t value)
{
    size_t bits = num_get_prec_bits(value);
    return bits ? bits : num_get_default_prec_bits();
}

static bool series_input(mpc_ptr out, const number_t *input)
{
    number_t *converted = number_coerce(input, NUMBER_COMPLEX);
    bool ok = converted && !number_complex_get_mpc(out, number_impl_const(converted)->value.cx, mpc_get_prec(out));
    number_box_free(converted);
    return ok && mpfr_number_p(mpc_realref(out)) && mpfr_number_p(mpc_imagref(out));
}

static number_t series_result(mpc_srcptr value, size_t bits, bool real)
{
    number_t *result = real ? number_wrap_mpfr(number_mpfr_from_mpfr(mpc_realref(value), bits)) :
                              number_wrap_complex_mpc(value, bits);
    return result ? number_take(result) : number_invalid();
}

/* A geometric tail bounds every future term; roundoff is checked against the absolute partial sum. */
static bool series_complete(mpc_srcptr sum, mpfr_srcptr term, mpfr_srcptr absolute_sum,
                            mpfr_srcptr ratio, mpfr_prec_t target, unsigned long operations,
                            mpfr_srcptr power_error)
{
    if (!mpfr_number_p(ratio) || mpfr_cmp_ui(ratio, 1u) >= 0) return false;
    mpfr_t tail, tolerance, temporary;
    mpfr_inits2(mpc_get_prec(sum), tail, tolerance, temporary, (mpfr_ptr)0);
    mpfr_ui_sub(temporary, 1u, ratio, MPFR_RNDD);
    mpfr_div(tail, ratio, temporary, MPFR_RNDU);
    mpfr_mul(tail, tail, term, MPFR_RNDU);
    mpfr_mul_ui(temporary, absolute_sum, operations, MPFR_RNDU);
    mpfr_mul_2si(temporary, temporary, -mpc_get_prec(sum) + 8, MPFR_RNDU);
    mpfr_add(tail, tail, temporary, MPFR_RNDU);
    if (power_error) mpfr_add(tail, tail, power_error, MPFR_RNDU);
    number_mpc_abs_lower(tolerance, sum);
    mpfr_mul_2si(tolerance, tolerance, -target - 8, MPFR_RNDD);
    bool ok = mpfr_lessequal_p(tail, tolerance);
    mpfr_clears(tail, tolerance, temporary, (mpfr_ptr)0);
    return ok;
}

enum { PFQ_SPLIT_MAX_TERMS = 8192 };

typedef struct _pfq_split_t pfq_split_t;
struct _pfq_split_t {
    mpc_t *parameters;
    size_t upper_count, lower_count;
    mpfr_prec_t precision;
    bool factorial;
    mpc_srcptr weight;
    mpfr_t products[4];
};

/* The interval represents its numerator product P, denominator product Q and local partial sum T/Q. */
static void pfq_split_interval(mpc_ptr p, mpc_ptr q, mpc_ptr t, mpfr_ptr p_size, mpfr_ptr q_size,
                                mpfr_ptr t_size, pfq_split_t *state, unsigned first, unsigned count)
{
    if (count == 1u) {
        mpc_t shift;
        mpc_init2(shift, state->precision);
        mpc_set(p, state->parameters[state->upper_count + state->lower_count], MPC_RNDNN);
        mpc_set_ui(q, state->factorial ? first + 1u : 1u, MPC_RNDNN);
        for (size_t j = 0; j < state->upper_count + state->lower_count; ++j) {
            mpc_add_ui(shift, state->parameters[j], first, MPC_RNDNN);
            if (j < state->upper_count) number_mpc_mul_guarded(p, p, shift, state->products);
            else number_mpc_mul_guarded(q, q, shift, state->products);
        }
        if (state->weight) {
            mpc_add_ui(shift, state->weight, 2u * first, MPC_RNDNN);
            number_mpc_mul_guarded(t, q, shift, state->products);
        } else mpc_set(t, q, MPC_RNDNN);
        number_mpc_abs_upper(p_size, p); number_mpc_abs_upper(q_size, q);
        number_mpc_abs_upper(t_size, t);
        mpc_clear(shift);
        return;
    }
    unsigned left = count / 2u;
    pfq_split_interval(p, q, t, p_size, q_size, t_size, state, first, left);
    mpc_t right_p, right_q, right_t, combined;
    mpc_init2(right_p, state->precision); mpc_init2(right_q, state->precision);
    mpc_init2(right_t, state->precision); mpc_init2(combined, state->precision);
    mpfr_t right_p_size, right_q_size, right_t_size, bound;
    mpfr_inits2(64, right_p_size, right_q_size, right_t_size, bound, (mpfr_ptr)0);
    pfq_split_interval(right_p, right_q, right_t, right_p_size, right_q_size, right_t_size,
                       state, first + left, count - left);
    mpfr_mul(t_size, t_size, right_q_size, MPFR_RNDU);
    mpfr_mul(bound, p_size, right_t_size, MPFR_RNDU);
    mpfr_add(t_size, t_size, bound, MPFR_RNDU);
    mpfr_mul(p_size, p_size, right_p_size, MPFR_RNDU);
    mpfr_mul(q_size, q_size, right_q_size, MPFR_RNDU);
    number_mpc_mul_guarded(combined, t, right_q, state->products);
    number_mpc_mul_guarded(t, p, right_t, state->products);
    mpc_add(t, t, combined, MPC_RNDNN);
    number_mpc_mul_guarded(p, p, right_p, state->products);
    number_mpc_mul_guarded(q, q, right_q, state->products);
    mpfr_clears(right_p_size, right_q_size, right_t_size, bound, (mpfr_ptr)0);
    mpc_clear(right_p); mpc_clear(right_q); mpc_clear(right_t); mpc_clear(combined);
}

/* Bound t_N and every later ratio independently of the product tree; all lower real parts are positive. */
static unsigned pfq_split_count(mpc_t *parameters, size_t p, size_t q, mpfr_t *radii,
                                 mpfr_srcptr radius, mpfr_prec_t work, mpfr_ptr tail, bool factorial,
                                 mpc_srcptr weight)
{
    mpfr_t term, ratio, factor, gap;
    mpfr_inits2(64, term, ratio, factor, gap, (mpfr_ptr)0);
    mpfr_set_ui(term, 1u, MPFR_RNDN);
    unsigned count = 0;
    for (unsigned n = 0; n < PFQ_SPLIT_MAX_TERMS; ++n) {
        mpfr_set(ratio, radius, MPFR_RNDU);
        if (factorial) mpfr_div_ui(ratio, ratio, n + 1u, MPFR_RNDU);
        for (size_t j = 0; j < p + q; ++j) {
            if (j < p) {
                mpfr_add_ui(factor, radii[j], n, MPFR_RNDU);
                mpfr_mul(ratio, ratio, factor, MPFR_RNDU);
            } else {
                mpfr_add_ui(factor, mpc_realref(parameters[j]), n, MPFR_RNDD);
                mpfr_div(ratio, ratio, factor, MPFR_RNDU);
            }
        }
        mpfr_mul(term, term, ratio, MPFR_RNDU);
        if (mpfr_cmp_ui_2exp(term, 1u, -work + 8) > 0) continue;
        /* For every k>=N, (k+|a|)/k <= 1+|a|/N and |k+b|/k >= 1. */
        unsigned next = n + 1u;
        mpfr_set(ratio, radius, MPFR_RNDU);
        if (p <= q)
            for (size_t j = 0; j < q - p + (factorial ? 1u : 0u); ++j)
                mpfr_div_ui(ratio, ratio, next, MPFR_RNDU);
        for (size_t j = 0; j < p; ++j) {
            mpfr_div_ui(factor, radii[j], next, MPFR_RNDU);
            mpfr_add_ui(factor, factor, 1u, MPFR_RNDU);
            mpfr_mul(ratio, ratio, factor, MPFR_RNDU);
        }
        if (mpfr_cmp_d(ratio, 0.875) > 0) continue;
        mpfr_ui_sub(gap, 1u, ratio, MPFR_RNDD);
        mpfr_div(tail, term, gap, MPFR_RNDU);
        if (weight) {
            /* Sum (|weight|+2*(N+j))*rho^j for every omitted weighted term. */
            mpfr_div(factor, ratio, gap, MPFR_RNDU);
            mpfr_mul_2ui(factor, factor, 1u, MPFR_RNDU);
            mpfr_add_ui(factor, factor, 2u * next, MPFR_RNDU);
            number_mpc_abs_upper(gap, weight);
            mpfr_add(factor, factor, gap, MPFR_RNDU);
            mpfr_mul(tail, tail, factor, MPFR_RNDU);
        }
        count = next;
        break;
    }
    mpfr_clears(term, ratio, factor, gap, (mpfr_ptr)0);
    return count;
}

/* Accept a finite partial sum only after checking its geometric tail and full product-tree error allowance. */
static bool pfq_split_sum(mpc_ptr out, mpc_t *parameters, size_t p, size_t q, mpfr_t *radii,
                           mpfr_srcptr radius, mpfr_prec_t target, bool factorial, mpc_srcptr weight)
{
    mpfr_prec_t work = mpc_get_prec(out);
    if (work < 1024 || work > 16384 || p + q > 8u || p > q + 1u || mpfr_zero_p(radius) ||
        mpfr_cmp_ui(radius, 8u) > 0 || (p == q + 1u && mpfr_cmp_d(radius, 0.625) > 0)) return false;
    bool real = mpfr_zero_p(mpc_imagref(parameters[p + q]));
    for (size_t j = 0; j < p + q; ++j) {
        if (mpfr_cmp_d(mpc_realref(parameters[j]), 0.125) < 0 ||
            mpfr_cmp_ui(mpc_realref(parameters[j]), 16u) > 0 ||
            mpfr_cmpabs_ui(mpc_imagref(parameters[j]), 1u) > 0) return false;
        real = real && mpfr_zero_p(mpc_imagref(parameters[j]));
    }
    if (work < (real ? 3072 : 1024)) return false;
    mpfr_t tail, p_size, q_size, majorant, scale, error;
    mpfr_inits2(64, tail, p_size, q_size, majorant, scale, error, (mpfr_ptr)0);
    unsigned count = pfq_split_count(parameters, p, q, radii, radius, work, tail, factorial, weight);
    bool ok = false;
    if (count) {
        pfq_split_t state = {.parameters = parameters, .upper_count = p, .lower_count = q, .precision = work,
                             .factorial = factorial, .weight = weight};
        for (unsigned j = 0; j < 4u; ++j) mpfr_init2(state.products[j], work);
        mpc_t numerator, denominator, total, sum;
        mpc_init2(numerator, work); mpc_init2(denominator, work);
        mpc_init2(total, work); mpc_init2(sum, work);
        pfq_split_interval(numerator, denominator, total, p_size, q_size, majorant, &state, 0u, count);
        mpc_div(sum, total, denominator, MPC_RNDNN);
        number_mpc_abs_lower(q_size, denominator);
        mpfr_div(majorant, majorant, q_size, MPFR_RNDU);
        /* At most 8192 leaves with eight bounded parameters and four guarded products per internal node.
         * 2^40 covers factor construction, accumulated product errors, sums and the final division. */
        mpfr_mul_2si(error, majorant, -work + 40, MPFR_RNDU);
        mpfr_add(error, error, tail, MPFR_RNDU);
        number_mpc_abs_lower(scale, sum);
        mpfr_mul_2si(scale, scale, -target - 16, MPFR_RNDD);
        ok = mpfr_number_p(error) && mpfr_number_p(scale) && mpfr_lessequal_p(error, scale);
        if (ok) mpc_set(out, sum, MPC_RNDNN);
        mpc_clear(numerator); mpc_clear(denominator); mpc_clear(total); mpc_clear(sum);
        for (unsigned j = 0; j < 4u; ++j) mpfr_clear(state.products[j]);
    }
    mpfr_clears(tail, p_size, q_size, majorant, scale, error, (mpfr_ptr)0);
    return ok;
}

/* Sum w^k/((a)_k*(b)_k), optionally weighted by power+2*k for a cylindrical argument derivative. */
bool number_mpc_two_lower_series(mpc_ptr out, mpc_srcptr a, mpc_srcptr b, mpc_srcptr w,
                                  mpc_srcptr power, mpfr_prec_t target)
{
    mpfr_prec_t work = mpc_get_prec(out);
    if (work < 1024 || work > 16384 || (power &&
        (!mpfr_number_p(mpc_realref(power)) || !mpfr_number_p(mpc_imagref(power)) ||
         mpfr_cmpabs_ui(mpc_realref(power), 32u) > 0 ||
         mpfr_cmpabs_ui(mpc_imagref(power), 2u) > 0))) return false;
    mpc_t parameters[3];
    mpfr_t radii[3];
    mpc_srcptr inputs[3] = {a, b, w};
    bool finite = true;
    for (unsigned j = 0; j < 3u; ++j) {
        mpc_init2(parameters[j], work); mpfr_init2(radii[j], 64);
        mpc_set(parameters[j], inputs[j], MPC_RNDNN);
        finite = finite && mpfr_number_p(mpc_realref(inputs[j])) && mpfr_number_p(mpc_imagref(inputs[j]));
        number_mpc_abs_upper(radii[j], parameters[j]);
    }
    bool ok = finite && pfq_split_sum(out, parameters, 0u, 2u, radii, radii[2], target, false, power);
    for (unsigned j = 0; j < 3u; ++j) { mpc_clear(parameters[j]); mpfr_clear(radii[j]); }
    return ok;
}

/* Rebuild the ratio polynomials every 64 steps; purely real lower parameters need only a quadratic denominator. */
static void pfq_kummer_refresh(mpc_ptr numerator, mpc_ptr step, mpfr_ptr denominator, mpfr_ptr first_difference,
                               mpfr_ptr second_difference, mpc_t *parameters, unsigned n, mpfr_t *products)
{
    mpfr_prec_t work = mpc_get_prec(numerator);
    mpc_t upper, lower, temporary;
    mpc_init2(upper, work); mpc_init2(lower, work); mpc_init2(temporary, work);
    mpc_add_ui(upper, parameters[0], n, MPC_RNDNN);
    mpc_add_ui(lower, parameters[1], n, MPC_RNDNN);
    if (mpfr_zero_p(mpc_imagref(lower))) {
        number_mpc_mul_guarded(numerator, upper, parameters[2], products);
        mpc_set(step, parameters[2], MPC_RNDNN);
        mpfr_mul_ui(denominator, mpc_realref(lower), n + 1u, MPFR_RNDN);
        mpfr_add_ui(first_difference, mpc_realref(lower), n + 2u, MPFR_RNDN);
        mpfr_set_ui(second_difference, 2u, MPFR_RNDN);
    } else {
        mpc_conj(lower, lower, MPC_RNDNN);
        number_mpc_mul_guarded(temporary, upper, lower, products);
        number_mpc_mul_guarded(numerator, temporary, parameters[2], products);
        mpc_add(temporary, upper, lower, MPC_RNDNN);
        mpc_add_ui(temporary, temporary, 1u, MPC_RNDNN);
        number_mpc_mul_guarded(step, temporary, parameters[2], products);
        mpc_norm(denominator, lower, MPFR_RNDN);
        mpfr_mul_ui(first_difference, mpc_realref(lower), 2u * (n + 2u), MPFR_RNDN);
        mpfr_add_ui(first_difference, first_difference, n + 2u, MPFR_RNDN);
        mpfr_add(first_difference, first_difference, denominator, MPFR_RNDN);
        mpfr_mul_ui(denominator, denominator, n + 1u, MPFR_RNDN);
        mpfr_mul_ui(second_difference, mpc_realref(lower), 4u, MPFR_RNDN);
        mpfr_add_ui(second_difference, second_difference, 2u * n + 8u, MPFR_RNDN);
    }
    mpc_clear(upper); mpc_clear(lower); mpc_clear(temporary);
}

/* 1F1 advances z*(a+n)*conj(b+n)/((n+1)*|b+n|^2), using real division and bounded late-term precision. */
static bool pfq_kummer_sum(mpc_ptr out, mpc_t *parameters, mpfr_t *radii,
                            mpfr_srcptr radius, mpfr_prec_t target)
{
    mpfr_prec_t work = mpc_get_prec(out);
    if (work < 640 || work > 16384 || mpfr_zero_p(radius) || mpfr_cmp_ui(radius, 8u) > 0) return false;
    for (unsigned j = 0; j < 2u; ++j) {
        if (mpfr_cmp_d(mpc_realref(parameters[j]), 0.125) < 0 ||
            mpfr_cmp_ui(mpc_realref(parameters[j]), 16u) > 0 ||
            mpfr_cmpabs_ui(mpc_imagref(parameters[j]), 1u) > 0) return false;
    }
    bool complex_lower = !mpfr_zero_p(mpc_imagref(parameters[1])), ok = false;
    mpc_t numerator, step, second, term, sum;
    mpc_init2(numerator, work); mpc_init2(step, work); mpc_init2(second, work);
    mpc_init2(term, work); mpc_init2(sum, work);
    mpc_mul_2ui(second, parameters[2], 1u, MPC_RNDNN);
    mpc_set_ui(term, 1u, MPC_RNDNN); mpc_set_ui(sum, 1u, MPC_RNDNN);
    mpfr_t denominator, first_difference, second_difference, magnitude, absolute_sum;
    mpfr_inits2(work, denominator, first_difference, second_difference, magnitude, absolute_sum, (mpfr_ptr)0);
    mpfr_set_ui(absolute_sum, 1u, MPFR_RNDN);
    mpfr_t ratio, bound, term_error, total_error, previous_size;
    mpfr_inits2(64, ratio, bound, term_error, total_error, previous_size, (mpfr_ptr)0);
    mpfr_set_zero(term_error, 1); mpfr_set_zero(total_error, 1);
    mpfr_t products[4], refresh_products[4];
    for (unsigned j = 0; j < 4u; ++j) {
        mpfr_init2(products[j], work); mpfr_init2(refresh_products[j], work);
    }
    for (unsigned n = 0; n < PFQ_SPLIT_MAX_TERMS; ++n) {
        if (!(n % 64u))
            pfq_kummer_refresh(numerator, step, denominator, first_difference, second_difference,
                                 parameters, n, refresh_products);
        if (n >= 64u) {
            number_mpc_abs_upper(previous_size, term);
            if (!mpfr_number_p(previous_size) || mpfr_zero_p(previous_size)) break;
            mpfr_exp_t difference = mpfr_get_exp(previous_size) - mpfr_get_exp(absolute_sum);
            mpfr_prec_t precision = work;
            if (difference < -32) precision = difference < 96 - work ? 128 : work + difference + 32;
            precision = ((precision + 63) / 64) * 64;
            if (precision > work) precision = work;
            if (mpc_get_prec(term) != precision) {
                mpfr_prec_round(mpc_realref(term), precision, MPFR_RNDN);
                mpfr_prec_round(mpc_imagref(term), precision, MPFR_RNDN);
                for (unsigned j = 0; j < 4u; ++j) mpfr_set_prec(products[j], precision);
            }
            /* After n>=64 the normalised ratio is below 1/4. Propagate all shortened-state errors explicitly. */
            mpfr_div_ui(ratio, radii[0], n, MPFR_RNDU);
            mpfr_add_ui(ratio, ratio, 1u, MPFR_RNDU);
            mpfr_mul(ratio, ratio, radius, MPFR_RNDU); mpfr_div_ui(ratio, ratio, n, MPFR_RNDU);
            mpfr_mul(bound, previous_size, ratio, MPFR_RNDU);
            mpfr_mul_2si(bound, bound, -precision + 16, MPFR_RNDU);
            mpfr_mul(term_error, term_error, ratio, MPFR_RNDU);
            mpfr_add(term_error, term_error, bound, MPFR_RNDU);
            mpfr_add(total_error, total_error, term_error, MPFR_RNDU);
        }
        number_mpc_mul_guarded(term, term, numerator, products);
        mpc_div_fr(term, term, denominator, MPC_RNDNN);
        mpc_add(sum, sum, term, MPC_RNDNN);
        if (!mpfr_number_p(mpc_realref(sum)) || !mpfr_number_p(mpc_imagref(sum))) break;
        number_mpc_abs_upper(magnitude, term);
        mpfr_add(absolute_sum, absolute_sum, magnitude, MPFR_RNDU);
        if (n >= 8u && !(n % 8u)) {
            unsigned next = n + 1u;
            mpfr_div_ui(ratio, radii[0], next, MPFR_RNDU);
            mpfr_add_ui(ratio, ratio, 1u, MPFR_RNDU);
            mpfr_mul(ratio, ratio, radius, MPFR_RNDU); mpfr_div_ui(ratio, ratio, next, MPFR_RNDU);
            mpfr_add(magnitude, magnitude, term_error, MPFR_RNDU);
            /* A 2^48 full-width allowance covers all 8192 terms, including polynomial construction and refresh.
             * The extra current error covers its entire omitted tail once the ratio is at most one half. */
            mpfr_mul_2si(bound, absolute_sum, -work + 48, MPFR_RNDU);
            mpfr_add(bound, bound, total_error, MPFR_RNDU);
            mpfr_add(bound, bound, term_error, MPFR_RNDU);
            if (series_complete(sum, magnitude, absolute_sum, ratio, target,
                                (unsigned long)next * 10ul, bound)) { ok = true; break; }
        }
        mpc_add(numerator, numerator, step, MPC_RNDNN);
        if (complex_lower) mpc_add(step, step, second, MPC_RNDNN);
        mpfr_add(denominator, denominator, first_difference, MPFR_RNDN);
        mpfr_add(first_difference, first_difference, second_difference, MPFR_RNDN);
        if (complex_lower) mpfr_add_ui(second_difference, second_difference, 6u, MPFR_RNDN);
    }
    if (ok) mpc_set(out, sum, MPC_RNDNN);
    for (unsigned j = 0; j < 4u; ++j) { mpfr_clear(products[j]); mpfr_clear(refresh_products[j]); }
    mpfr_clears(ratio, bound, term_error, total_error, previous_size, (mpfr_ptr)0);
    mpfr_clears(denominator, first_difference, second_difference, magnitude, absolute_sum, (mpfr_ptr)0);
    mpc_clear(numerator); mpc_clear(step); mpc_clear(second); mpc_clear(term); mpc_clear(sum);
    return ok;
}

/* The bounded 0F0 and 1F0 identities avoid summation; |z|<1 keeps 1-z in the principal right half-plane. */
static bool pfq_elementary_sum(mpc_ptr out, mpc_t *parameters, size_t p, size_t q, mpfr_srcptr radius)
{
    bool exponential = p == 0u && q == 0u && mpfr_cmp_ui(radius, 8u) <= 0;
    bool binomial = p == 1u && q == 0u && mpfr_cmp_ui(radius, 1u) < 0 &&
        mpfr_cmp_d(mpc_realref(parameters[0]), 0.125) >= 0 &&
        mpfr_cmp_ui(mpc_realref(parameters[0]), 16u) <= 0 &&
        mpfr_cmpabs_ui(mpc_imagref(parameters[0]), 1u) <= 0;
    if (!exponential && !binomial) return false;
    mpfr_prec_t work = mpc_get_prec(out);
    mpc_t value, base, exponent;
    mpc_init2(value, work); mpc_init2(base, work); mpc_init2(exponent, work);
    if (exponential) mpc_exp(value, parameters[0], MPC_RNDNN);
    else {
        mpc_ui_sub(base, 1u, parameters[1], MPC_RNDNN);
        mpc_neg(exponent, parameters[0], MPC_RNDNN);
        mpc_pow(value, base, exponent, MPC_RNDNN);
    }
    /* The 128 guard bits cover bounded exponentiation and source conversion; all destinations keep work bits. */
    bool ok = mpfr_number_p(mpc_realref(value)) && mpfr_number_p(mpc_imagref(value));
    if (ok) mpc_set(out, value, MPC_RNDNN);
    mpc_clear(value); mpc_clear(base); mpc_clear(exponent);
    return ok;
}

/* Evaluate generalised hypergeometric notation without converting multiprecision inputs to qfloat. */
number_t num_hypergeometric_pFq(const number_t *upper, size_t p, const number_t *lower, size_t q,
                               const number_t argument)
{
    if ((p && !upper) || (q && !lower) || q == SIZE_MAX || p > SIZE_MAX - q - 1 ||
        p + q + 1 > SIZE_MAX / sizeof(mpc_t)) return NUM_NAN;
    size_t bits = series_input_precision(argument);
    bool real = num_is_real(argument);
    bool native = number_math_family_value(&argument) >= NUMBER_MATH_MPFR;
    for (size_t i = 0; i < p + q; ++i) {
        const number_t *a = i < p ? upper + i : lower + i - p;
        if (series_input_precision(*a) > bits) bits = series_input_precision(*a);
        native = native || number_math_family_value(a) >= NUMBER_MATH_MPFR;
        real = real && num_is_real(*a);
    }
    if (!native) {
        qcomplex_t *a = p ? calloc(p, sizeof(*a)) : NULL, *b = q ? calloc(q, sizeof(*b)) : NULL;
        if ((p && !a) || (q && !b)) { free(a); free(b); return NUM_NAN; }
        for (size_t i = 0; i < p; ++i) a[i] = number_value_to_qcomplex(upper + i);
        for (size_t i = 0; i < q; ++i) b[i] = number_value_to_qcomplex(lower + i);
        qcomplex_t z = number_value_to_qcomplex(&argument);
        qcomplex_t value = qc_hypergeometric_pFq(a, p, b, q, z);
        free(a); free(b);
        return real ? num_create_from_qfloat(value.re) : num_create_from_qcomplex(value);
    }
    if (bits > 65536 - 128) return NUM_NAN;
    mpfr_prec_t work = (mpfr_prec_t)bits + 128;
    mpc_t *parameters = calloc(p + q + 1, sizeof(*parameters));
    mpfr_t *radii = p + q ? calloc(p + q, sizeof(*radii)) : NULL;
    if (!parameters || ((p + q) && !radii)) { free(parameters); free(radii); return NUM_NAN; }
    mpc_t term, sum, factor, denominator, shift;
    mpc_init2(term, work); mpc_init2(sum, work); mpc_init2(factor, work); mpc_init2(denominator, work);
    mpc_init2(shift, work);
    mpfr_t radius, ratio, temporary, magnitude, absolute_sum;
    mpfr_inits2(work, magnitude, absolute_sum, (mpfr_ptr)0);
    mpfr_inits2(64, radius, ratio, temporary, (mpfr_ptr)0);
    bool ok = true, terminating = false, converged = false, exact_arithmetic = true;
    for (size_t i = 0; i < p + q + 1; ++i) {
        mpc_init2(parameters[i], work);
        const number_t *a = i < p ? upper + i : i < p + q ? lower + i - p : &argument;
        ok = series_input(parameters[i], a) && ok;
        if (i < p + q) {
            mpfr_init2(radii[i], 64);
            mpc_abs(radii[i], parameters[i], MPFR_RNDU);
        }
        if (i < p && mpfr_zero_p(mpc_imagref(parameters[i])) &&
            mpfr_integer_p(mpc_realref(parameters[i])) && mpfr_sgn(mpc_realref(parameters[i])) <= 0)
            terminating = true;
    }
    mpc_abs(radius, parameters[p + q], MPFR_RNDU);
    ok = ok && (terminating || (p <= q || (p == q + 1 && mpfr_cmp_ui(radius, 1u) < 0)));
    mpc_set_ui(term, 1u, MPC_RNDNN); mpc_set_ui(sum, 1u, MPC_RNDNN);
    mpfr_set_ui(absolute_sum, 1u, MPFR_RNDN);
    if (ok && !terminating) {
        converged = pfq_elementary_sum(sum, parameters, p, q, radius);
        if (!converged && p == 1u && q == 1u)
            converged = pfq_kummer_sum(sum, parameters, radii, radius, (mpfr_prec_t)bits);
        if (!converged)
            converged = pfq_split_sum(sum, parameters, p, q, radii, radius, (mpfr_prec_t)bits, true, NULL);
    }
    for (unsigned long n = 0; ok && !converged && n < 200000; ++n) {
        mpc_set(factor, parameters[p + q], MPC_RNDNN);
        mpc_set_ui(denominator, n + 1, MPC_RNDNN);
        for (size_t i = 0; i < p + q; ++i) {
            if (mpc_add_ui(shift, parameters[i], n, MPC_RNDNN)) exact_arithmetic = false;
            int inexact = i < p ? mpc_mul(factor, factor, shift, MPC_RNDNN) :
                                  mpc_mul(denominator, denominator, shift, MPC_RNDNN);
            if (inexact) exact_arithmetic = false;
        }
        if (mpfr_zero_p(mpc_realref(denominator)) && mpfr_zero_p(mpc_imagref(denominator))) break;
        /* Multiply before dividing, preserving exact polynomial steps such as 3*(b+2)/(3*(b+2)). */
        if (mpc_mul(term, term, factor, MPC_RNDNN)) exact_arithmetic = false;
        if (mpc_div(term, term, denominator, MPC_RNDNN)) exact_arithmetic = false;
        if (mpc_add(sum, sum, term, MPC_RNDNN)) exact_arithmetic = false;
        if (!mpfr_number_p(mpc_realref(sum)) || !mpfr_number_p(mpc_imagref(sum))) break;
        number_mpc_abs_upper(magnitude, term);
        mpfr_add(absolute_sum, absolute_sum, magnitude, MPFR_RNDU);
        if (mpfr_zero_p(magnitude)) {
            mpfr_set_zero(ratio, 1);
            converged = exact_arithmetic ||
                series_complete(sum, magnitude, absolute_sum, ratio, bits, (n + 1) * (p + q + 8), NULL);
            break;
        }
        if (terminating || n < 8 || n % 8) continue;
        /* At the next index k=n+1, bound every subsequent ratio by monotone normalised factors. */
        unsigned long k = n + 1;
        mpfr_set(ratio, radius, MPFR_RNDU);
        if (p <= q)
            for (size_t i = 0; i <= q - p; ++i) mpfr_div_ui(ratio, ratio, k, MPFR_RNDU);
        bool bounded = true;
        for (size_t i = 0; i < p + q; ++i) {
            mpfr_div_ui(temporary, radii[i], k, MPFR_RNDU);
            if (i < p) {
                mpfr_add_ui(temporary, temporary, 1u, MPFR_RNDU);
                mpfr_mul(ratio, ratio, temporary, MPFR_RNDU);
            } else {
                mpfr_ui_sub(temporary, 1u, temporary, MPFR_RNDD);
                if (mpfr_sgn(temporary) <= 0) { bounded = false; break; }
                mpfr_div(ratio, ratio, temporary, MPFR_RNDU);
            }
        }
        if (bounded && series_complete(sum, magnitude, absolute_sum, ratio, bits, k * (p + q + 8), NULL)) {
            converged = true;
            break;
        }
    }
    number_t result = converged ? series_result(sum, bits, real) : NUM_NAN;
    for (size_t i = 0; i < p + q + 1; ++i) mpc_clear(parameters[i]);
    for (size_t i = 0; i < p + q; ++i) mpfr_clear(radii[i]);
    free(parameters); free(radii);
    mpc_clear(term); mpc_clear(sum); mpc_clear(factor); mpc_clear(denominator); mpc_clear(shift);
    mpfr_clears(radius, ratio, temporary, magnitude, absolute_sum, (mpfr_ptr)0);
    return result;
}

/* Evaluate the Lerch series on |z|<1, retaining principal powers and the widest input precision. */
number_t number_lerch_phi(const number_t z, const number_t s, const number_t a)
{
    if (!num_is_finite(z) || !num_is_finite(s) || !num_is_finite(a)) return NUM_NAN;
    if (num_is_zero(z)) {
        number_t negative = num_neg(s), result = num_pow(a, negative);
        num_destroy(&negative);
        return result;
    }
    if (num_is_zero(s)) {
        number_t denominator = num_sub(NUM_ONE, z), result = num_div(NUM_ONE, denominator);
        num_destroy(&denominator);
        return result;
    }
    if (num_eq(z, NUM_ONE)) return num_zetah(s, a);
    if (num_is_real(a) && num_le(a, NUM_ZERO)) return NUM_NAN;
    size_t bits = series_input_precision(z);
    if (series_input_precision(s) > bits) bits = series_input_precision(s);
    if (series_input_precision(a) > bits) bits = series_input_precision(a);
    if (bits < 106) bits = 106;
    if (bits > 65536 - 128) return NUM_NAN;
    mpfr_prec_t work = (mpfr_prec_t)bits + 128;
    if (num_is_real(s)) {
        number_t real_order = num_real_part(s);
        double degree = num_is_integer(real_order) ? -num_to_double(real_order) : 0;
        num_destroy(&real_order);
        if (degree >= 1 && degree <= 64) {
            mpc_t value;
            mpc_init2(value, work);
            bool complete = number_lerch_integer(value, z, a, (unsigned)degree, (mpfr_prec_t)bits);
            number_t result = complete ? series_result(value, bits, num_is_real(z) && num_is_real(a)) : NUM_NAN;
            mpc_clear(value);
            if (complete) return result;
        }
    }
    enum { LERCH_BLOCK = 16 };
    unsigned block = bits >= 512 ? LERCH_BLOCK : 1u;
    mpc_t argument, exponent, offset, base, power, term, sum, step;
    mpc_t coefficients[LERCH_BLOCK];
    mpc_init2(argument, work); mpc_init2(exponent, work); mpc_init2(offset, work); mpc_init2(base, work);
    mpc_init2(power, work); mpc_init2(term, work); mpc_init2(sum, work); mpc_init2(step, work);
    for (unsigned j = 0; j < block; ++j) mpc_init2(coefficients[j], work);
    bool ok = series_input(argument, &z) && series_input(exponent, &s) && series_input(offset, &a);
    bool real = num_is_real(z) && num_is_real(s) && num_is_real(a), converged = false;
    mpfr_t radius, order, offset_size, ratio, temporary, magnitude, absolute_sum, power_error, rounding;
    /* Directed error bounds need a reliable upper bound, not the result's full significand. */
    mpfr_inits2(64, radius, order, offset_size, ratio, temporary, magnitude, absolute_sum, power_error, rounding,
                 (mpfr_ptr)0);
    mpc_abs(radius, argument, MPFR_RNDU); mpc_abs(order, exponent, MPFR_RNDU);
    mpc_abs(offset_size, offset, MPFR_RNDU);
    ok = ok && mpfr_cmp_ui(radius, 1u) < 0;
    bool integer = mpfr_zero_p(mpc_imagref(exponent)) && mpfr_integer_p(mpc_realref(exponent)) &&
                   mpfr_fits_slong_p(mpc_realref(exponent), MPFR_RNDN);
    long integer_order = integer ? mpfr_get_si(mpc_realref(exponent), MPFR_RNDN) : 0;
    if (ok && !integer && !converged)
        converged = number_lerch_blocks(sum, argument, exponent, offset, (mpfr_prec_t)bits);
    if (ok && real && !converged) {
        converged = number_lerch_real(mpc_realref(sum), argument, exponent, offset, (mpfr_prec_t)bits);
        if (converged) mpfr_set_zero(mpc_imagref(sum), 1);
    }
    if (ok && !converged) mpc_pow_ui(step, argument, block, MPC_RNDNN);
    for (unsigned attempt = 0; ok && !converged && attempt < 2; ++attempt) {
        bool adaptive = !integer && attempt == 0, retry = false;
        mpc_set_prec(base, work);
        mpc_set_ui(power, 1u, MPC_RNDNN); mpc_set_ui(sum, 0u, MPC_RNDNN);
        mpfr_set_zero(absolute_sum, 1); mpfr_set_zero(power_error, 1);
        for (unsigned long k = 0; ok && k < 200000; k += block) {
            if (integer && block == 1) {
                /* Negative orders are positive powers; avoid inverting a power and then dividing by it. */
                mpc_add_ui(base, offset, k, MPC_RNDNN);
                if (integer_order < 0) {
                    mpc_pow_ui(base, base, 0ul - (unsigned long)integer_order, MPC_RNDNN);
                    mpc_mul(term, power, base, MPC_RNDNN);
                } else {
                    mpc_pow_si(base, base, integer_order, MPC_RNDNN);
                    mpc_div(term, power, base, MPC_RNDNN);
                }
                number_mpc_abs_upper(rounding, term);
                mpfr_add(absolute_sum, absolute_sum, rounding, MPFR_RNDU);
            } else {
                number_mpc_abs_upper(magnitude, power);
                for (unsigned j = 0; j < block; ++j) {
                    mpfr_exp_t exponent_bound = mpfr_zero_p(magnitude) ? 0 : mpfr_get_exp(magnitude);
                    mpfr_prec_t precision = work;
                    if (adaptive && exponent_bound < 0)
                        precision = exponent_bound < 128 - work ? 128 : work + exponent_bound;
                    if (mpc_get_prec(coefficients[j]) != precision) mpc_set_prec(coefficients[j], precision);
                    if (integer) {
                        mpc_add_ui(base, offset, k + j, MPC_RNDNN);
                        if (integer_order < 0)
                            mpc_pow_ui(coefficients[j], base, 0ul - (unsigned long)integer_order, MPC_RNDNN);
                        else {
                            mpc_pow_si(base, base, integer_order, MPC_RNDNN);
                            mpc_ui_div(coefficients[j], 1u, base, MPC_RNDNN);
                        }
                    } else number_dirichlet_power(coefficients[j], offset, exponent, k + j);
                    number_mpc_abs_upper(rounding, coefficients[j]);
                    mpfr_mul(rounding, rounding, magnitude, MPFR_RNDU);
                    mpfr_add(absolute_sum, absolute_sum, rounding, MPFR_RNDU);
                    /* Bounds use |z|^j times an upper bound for the block's initial power, rather than
                     * separately multiplying every complex z^k. Low-precision coefficient errors stay explicit. */
                    /* Also cover every rounded operation of the at-most-sixteen-term Horner block and
                     * its final multiplication. The absolute coefficient sum bounds cancellation inside it. */
                    mpfr_mul_2si(temporary, rounding, -precision + 10, MPFR_RNDU);
                    mpfr_add(power_error, power_error, temporary, MPFR_RNDU);
                    if (j + 1 < block) mpfr_mul(magnitude, magnitude, radius, MPFR_RNDU);
                }
                /* Rectangular splitting halves the full-width complex products: one Horner block, then one
                 * multiplication by z^k. The geometric tail uses the last individual term's directed upper bound. */
                mpfr_prec_t precision = mpc_get_prec(coefficients[0]);
                if (mpc_get_prec(term) != precision) mpc_set_prec(term, precision);
                if (mpc_get_prec(base) != precision) mpc_set_prec(base, precision);
                mpc_set(base, argument, MPC_RNDNN);
                mpc_set(term, coefficients[block - 1], MPC_RNDNN);
                for (unsigned j = block - 1; j > 0; --j) {
                    mpc_mul(term, term, base, MPC_RNDNN); mpc_add(term, term, coefficients[j - 1], MPC_RNDNN);
                }
                mpc_set(base, power, MPC_RNDNN);
                mpc_mul(term, term, base, MPC_RNDNN);
            }
            mpc_add(sum, sum, term, MPC_RNDNN);
            if (!mpfr_number_p(mpc_realref(sum)) || !mpfr_number_p(mpc_imagref(sum))) break;
            if (k > 8 && (block > 1 || k % 8 == 0)) {
                mpfr_ui_sub(temporary, k + block - 1, offset_size, MPFR_RNDD);
                if (mpfr_cmp_ui(temporary, 1u) > 0) {
                    mpfr_ui_div(temporary, 1u, temporary, MPFR_RNDU);
                    mpfr_ui_sub(temporary, 1u, temporary, MPFR_RNDD);
                    mpfr_neg(ratio, order, MPFR_RNDN);
                    mpfr_pow(ratio, temporary, ratio, MPFR_RNDU);
                    mpfr_mul(ratio, ratio, radius, MPFR_RNDU);
                    if (series_complete(sum, rounding, absolute_sum, ratio, bits, (k + block) * 32, power_error)) {
                        converged = true;
                        break;
                    }
                    if (adaptive && series_complete(sum, rounding, absolute_sum, ratio, bits, (k + block) * 32, NULL)) {
                        retry = true;
                        break;
                    }
                }
            }
            mpc_mul(power, power, step, MPC_RNDNN);
        }
        if (!retry) break;
    }
    number_t result = converged ? series_result(sum, bits, real) : NUM_NAN;
    mpfr_clears(radius, order, offset_size, ratio, temporary, magnitude, absolute_sum, power_error, rounding,
                 (mpfr_ptr)0);
    mpc_clear(argument); mpc_clear(exponent); mpc_clear(offset); mpc_clear(base);
    mpc_clear(power); mpc_clear(term); mpc_clear(sum); mpc_clear(step);
    for (unsigned j = 0; j < block; ++j) mpc_clear(coefficients[j]);
    return result;
}

/* Preserve the established alias with the same precision and convergence rules. */
number_t num_lerch_phi(const number_t z, const number_t s, const number_t a)
{
    return number_lerch_phi(z, s, a);
}
