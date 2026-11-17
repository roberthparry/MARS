/**
 * @file number_gamma.c
 * @brief Guarded gamma, logarithmic gamma, incomplete-gamma series and inverse refinement.
 *
 * @par Guarded complex gamma and logarithmic gamma at the requested precision.
 *
 * Shifts to the right half-plane and uses the Stirling expansion with cached Bernoulli coefficients. A product
 * accumulates positive-real-part recurrence factors, recording complete turns so the analytic logarithm is not
 * replaced by the principal logarithm of that product. Negative-real-part factors are handled individually.
 * Local scratch storage and bounded iteration leave coefficient retention to the number module's shared cache.
 *
 * @par Binary splitting of the lower incomplete-gamma defining series.
 *
 * Forms a common denominator for consecutive Pochhammer factors, avoiding a complex division at every term.
 * Shared powers of the argument and guarded products serve the recursive polynomial sum. Directed magnitude
 * majorants and a geometric tail validate each attempt; other parameters retain the ordinary gamma series.
 * All scratch values belong to the current call, with no cache or cross-thread state.
 *
 * @par Progressive-precision real and complex inverse gamma refinement.
 *
 * Preserves the established fixed-precision seed and its real branch selection, then refines the logarithmic
 * defining equation with Newton iteration. Early stages use small significands; precision grows only after the
 * previous stage supplies half-stage accuracy. The Newton slope needs only half-stage precision plus guard bits;
 * the small residual multiplies its rounding error. The final stage still checks both its correction and the
 * full-precision logarithmic residual. Seeds are never accepted as full-precision answers. This implementation
 * is private to the number module.
 */
#include <limits.h>
#include <math.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

enum { GAMMA_BLOCK = 16 };

/* Evaluate rising-factorial blocks with exact integer coefficients and reusable powers of the initial base.
 * For Re(base)>0 their absolute condition number is at most 2^(B/2); the 128 guard bits cover B<=16.
 * A block's argument advance is less than one radian, so the existing negative-axis crossing count stays valid. */
static bool gamma_rising_product(mpc_ptr product, mpc_ptr z, unsigned long limit, unsigned *shifts, long *turns)
{
    mpfr_prec_t work = mpc_get_prec(product);
    mpc_t powers[GAMMA_BLOCK + 1], polynomial, term;
    mpz_t coefficients[GAMMA_BLOCK + 1];
    for (unsigned j = 0; j <= GAMMA_BLOCK; ++j) {
        mpc_init2(powers[j], work);
        mpz_init(coefficients[j]);
    }
    mpc_init2(polynomial, work); mpc_init2(term, work);
    mpfr_t angle_bound;
    mpfr_init2(angle_bound, 64);
    mpc_set_ui(powers[0], 1u, MPC_RNDNN);
    bool powers_ready = false, ok = true;
    unsigned long offset = 0;
    int direction = mpfr_sgn(mpc_imagref(z));
    while (mpfr_cmp_ui(mpc_realref(z), limit) < 0) {
        mpfr_abs(angle_bound, mpc_imagref(z), MPFR_RNDU);
        mpfr_mul_ui(angle_bound, angle_bound, GAMMA_BLOCK, MPFR_RNDU);
        unsigned width = work >= 640 && mpfr_less_p(angle_bound, mpc_realref(z)) ? GAMMA_BLOCK : 1u;
        if (*shifts > 100000u - width) { ok = false; break; }
        if (width == 1) mpc_set(polynomial, z, MPC_RNDNN);
        else {
            if (!powers_ready) {
                mpc_sub_ui(powers[1], z, offset, MPC_RNDNN);
                for (unsigned j = 2; j <= GAMMA_BLOCK; ++j)
                    mpc_mul(powers[j], powers[j - 1], powers[1], MPC_RNDNN);
                powers_ready = true;
            }
            mpz_set_ui(coefficients[0], 1u);
            for (unsigned j = 1; j <= width; ++j) mpz_set_ui(coefficients[j], 0u);
            for (unsigned j = 0; j < width; ++j) {
                for (unsigned degree = j + 1; degree > 0; --degree) {
                    mpz_mul_ui(coefficients[degree], coefficients[degree], offset + j);
                    mpz_add(coefficients[degree], coefficients[degree], coefficients[degree - 1]);
                }
                mpz_mul_ui(coefficients[0], coefficients[0], offset + j);
            }
            mpc_set_z(polynomial, coefficients[0], MPC_RNDNN);
            for (unsigned degree = 1; degree <= width; ++degree) {
                mpfr_mul_z(mpc_realref(term), mpc_realref(powers[degree]), coefficients[degree], MPFR_RNDN);
                mpfr_mul_z(mpc_imagref(term), mpc_imagref(powers[degree]), coefficients[degree], MPFR_RNDN);
                mpc_add(polynomial, polynomial, term, MPC_RNDNN);
            }
        }
        int previous = mpfr_sgn(mpc_imagref(product));
        mpc_mul(term, product, polynomial, MPC_RNDNN);
        int next = mpfr_sgn(mpc_imagref(term));
        if (mpfr_sgn(mpc_realref(product)) < 0 || mpfr_sgn(mpc_realref(term)) < 0) {
            if (direction > 0 && previous >= 0 && next < 0) ++*turns;
            if (direction < 0 && previous <= 0 && next > 0) --*turns;
        }
        mpc_set(product, term, MPC_RNDNN);
        mpc_add_ui(z, z, width, MPC_RNDNN);
        offset += width;
        *shifts += width;
    }
    mpfr_clear(angle_bound);
    mpc_clear(term); mpc_clear(polynomial);
    for (unsigned j = 0; j <= GAMMA_BLOCK; ++j) { mpc_clear(powers[j]); mpz_clear(coefficients[j]); }
    return ok;
}

/* Rectangular splitting removes most full-width complex products from the Stirling correction polynomial.
 * Directed scalar magnitudes retain the original first-omitted-term and sector stopping bound. */
static bool gamma_stirling_correction(mpc_ptr out, mpc_srcptr inverse, mpfr_prec_t target)
{
    mpfr_prec_t work = mpc_get_prec(out);
    unsigned width = work >= 640 ? GAMMA_BLOCK : 1u;
    mpc_t baby[GAMMA_BLOCK], square, giant, factor, polynomial, term;
    for (unsigned j = 0; j < width; ++j) mpc_init2(baby[j], work);
    mpc_init2(square, work); mpc_init2(giant, work); mpc_init2(factor, work);
    mpc_init2(polynomial, work); mpc_init2(term, work);
    mpc_sqr(square, inverse, MPC_RNDNN);
    mpc_set_ui(baby[0], 1u, MPC_RNDNN);
    for (unsigned j = 1; j < width; ++j) mpc_mul(baby[j], baby[j - 1], square, MPC_RNDNN);
    mpc_mul(giant, baby[width - 1], square, MPC_RNDNN);
    mpc_set(factor, inverse, MPC_RNDNN); mpc_set_ui(polynomial, 0u, MPC_RNDNN);
    mpc_set_ui(out, 0u, MPC_RNDNN);
    mpfr_t coefficient, inverse_size, square_size, power_size, factorial_size, bound;
    mpfr_init2(coefficient, work);
    mpfr_inits2(64, inverse_size, square_size, power_size, factorial_size, bound, (mpfr_ptr)0);
    mpc_abs(inverse_size, inverse, MPFR_RNDU);
    mpfr_sqr(square_size, inverse_size, MPFR_RNDU); mpfr_set(power_size, inverse_size, MPFR_RNDU);
    mpfr_set_ui(factorial_size, 1u, MPFR_RNDN);
    bool ok = false;
    unsigned index = 0;
    for (unsigned long n = 1; n < (unsigned long)work; ++n) {
        /* |B_(2n)/(2n(2n-1))| < 4*(2n-2)!/32^n. Only the coefficient loses bits;
         * all complex powers and accumulators retain work precision. */
        if (n > 1) mpfr_mul_ui(factorial_size, factorial_size, (2 * n - 2) * (2 * n - 3), MPFR_RNDU);
        mpfr_mul(bound, factorial_size, power_size, MPFR_RNDU);
        mpfr_exp_t adjustment = mpfr_number_p(bound) && !mpfr_zero_p(bound) ?
            mpfr_get_exp(bound) - 5 * (mpfr_exp_t)n + 34 : 0;
        mpfr_prec_t precision = work;
        if (adjustment < 0) precision = adjustment < 128 - work ? 128 : work + adjustment;
        if (mpfr_get_prec(coefficient) != precision) mpfr_set_prec(coefficient, precision);
        if (number_coefficient_bernoulli(coefficient, n)) break;
        mpfr_div_ui(coefficient, coefficient, 2 * n * (2 * n - 1), MPFR_RNDN);
        mpfr_abs(bound, coefficient, MPFR_RNDU); mpfr_mul(bound, bound, power_size, MPFR_RNDU);
        if (mpfr_cmp_ui_2exp(bound, 1u, -target - 48 - (long)n) < 0) { ok = true; break; }
        mpc_mul_fr(term, baby[index], coefficient, MPC_RNDNN);
        mpc_add(polynomial, polynomial, term, MPC_RNDNN);
        if (++index == width) {
            mpc_mul(term, polynomial, factor, MPC_RNDNN); mpc_add(out, out, term, MPC_RNDNN);
            mpc_set_ui(polynomial, 0u, MPC_RNDNN); mpc_mul(factor, factor, giant, MPC_RNDNN);
            index = 0;
        }
        mpfr_mul(power_size, power_size, square_size, MPFR_RNDU);
    }
    if (ok && index) { mpc_mul(term, polynomial, factor, MPC_RNDNN); mpc_add(out, out, term, MPC_RNDNN); }
    mpfr_clear(coefficient); mpfr_clears(inverse_size, square_size, power_size, factorial_size, bound, (mpfr_ptr)0);
    mpc_clear(term); mpc_clear(polynomial); mpc_clear(factor); mpc_clear(giant); mpc_clear(square);
    for (unsigned j = 0; j < width; ++j) mpc_clear(baby[j]);
    return ok;
}

/* Evaluate log Gamma continuously on the plane cut along the non-positive real axis. */
int number_mpc_lgamma(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding)
{
    mpfr_prec_t target = mpc_get_prec(out), work = target + 128;
    if (work > 65536 || !mpfr_number_p(mpc_realref(input)) || !mpfr_number_p(mpc_imagref(input))) {
        mpc_set_nan(out);
        return -1;
    }
    mpc_t z, product, correction, sum, inverse, term;
    mpc_init2(z, work); mpc_init2(product, work); mpc_init2(correction, work); mpc_init2(sum, work);
    mpc_init2(inverse, work); mpc_init2(term, work);
    mpfr_t coefficient, pi;
    mpfr_inits2(work, coefficient, pi, (mpfr_ptr)0);
    mpc_set(z, input, MPC_RNDNN); mpc_set_ui(product, 1u, MPC_RNDNN); mpc_set_ui(correction, 0u, MPC_RNDNN);
    bool ok = false;
    unsigned shifts = 0;
    while (mpfr_sgn(mpc_realref(z)) <= 0) {
        if (++shifts > 100000 || (mpfr_zero_p(mpc_realref(z)) && mpfr_zero_p(mpc_imagref(z)))) goto done;
        mpc_log(term, z, MPC_RNDNN);
        mpc_add(correction, correction, term, MPC_RNDNN);
        mpc_add_ui(z, z, 1u, MPC_RNDNN);
    }
    long turns = 0;
    unsigned long limit = (unsigned long)work / 3 + 16;
    if (!gamma_rising_product(product, z, limit, &shifts, &turns)) goto done;
    mpc_log(term, product, MPC_RNDNN);
    mpfr_const_pi(pi, MPFR_RNDN);
    mpfr_mul_si(coefficient, pi, 2 * turns, MPFR_RNDN);
    mpfr_add(mpc_imagref(term), mpc_imagref(term), coefficient, MPFR_RNDN);
    mpc_add(correction, correction, term, MPC_RNDNN);
    mpc_log(term, z, MPC_RNDNN);
    mpc_set(sum, z, MPC_RNDNN);
    mpfr_sub_d(mpc_realref(sum), mpc_realref(sum), 0.5, MPFR_RNDN);
    mpc_mul(sum, sum, term, MPC_RNDNN);
    mpc_sub(sum, sum, z, MPC_RNDNN);
    mpfr_mul_2ui(coefficient, pi, 1u, MPFR_RNDN);
    mpfr_log(coefficient, coefficient, MPFR_RNDN);
    mpfr_div_2ui(coefficient, coefficient, 1u, MPFR_RNDN);
    mpfr_add(mpc_realref(sum), mpc_realref(sum), coefficient, MPFR_RNDN);
    mpc_ui_div(inverse, 1u, z, MPC_RNDNN);
    ok = gamma_stirling_correction(term, inverse, target);
    if (ok) mpc_add(sum, sum, term, MPC_RNDNN);
    if (ok) {
        mpc_sub(sum, sum, correction, MPC_RNDNN);
        mpc_set(out, sum, rounding);
    }
done:
    if (!ok) mpc_set_nan(out);
    mpfr_clears(coefficient, pi, (mpfr_ptr)0);
    mpc_clear(z); mpc_clear(product); mpc_clear(correction); mpc_clear(sum);
    mpc_clear(inverse); mpc_clear(term);
    return ok ? 0 : -1;
}

/* Exponentiate guarded log Gamma rather than using a fixed-precision Lanczos approximation. */
int number_mpc_gamma(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding)
{
    if (!mpfr_number_p(mpc_realref(input)) || !mpfr_number_p(mpc_imagref(input))) {
        mpc_set_nan(out);
        return -1;
    }
    mpfr_exp_t extra = 32;
    const mpfr_srcptr parts[] = {mpc_realref(input), mpc_imagref(input)};
    for (size_t i = 0; i < 2; ++i)
        if (!mpfr_zero_p(parts[i]) && mpfr_get_exp(parts[i]) > extra) extra = mpfr_get_exp(parts[i]);
    if (extra > 65000 || mpc_get_prec(out) > 65536 - extra - 128) { mpc_set_nan(out); return -1; }
    mpc_t logarithm;
    mpc_init2(logarithm, mpc_get_prec(out) + extra);
    int status = number_mpc_lgamma(logarithm, input, MPC_RNDNN);
    if (!status) mpc_exp(out, logarithm, rounding);
    else mpc_set_nan(out);
    mpc_clear(logarithm);
    return status;
}

/* Bound the lower series and retain extra bits before subtracting it from complete gamma. */
int number_mpc_gammainc(mpc_ptr out, mpc_srcptr order, mpc_srcptr argument,
                        number_gamma_mode_t mode, mpc_rnd_t rounding)
{
    mpfr_prec_t target = mpc_get_prec(out);
    mpfr_t radius;
    mpfr_init2(radius, 64);
    mpc_abs(radius, argument, MPFR_RNDU);
    if (!mpfr_number_p(radius) || mpfr_cmp_ui(radius, 8000u) > 0 ||
        !mpfr_number_p(mpc_realref(order)) || !mpfr_number_p(mpc_imagref(order))) {
        mpfr_clear(radius); mpc_set_nan(out); return -1;
    }
    mpfr_prec_t work = target + 160 + 4 * mpfr_get_ui(radius, MPFR_RNDU);
    if (work > 65536) { mpfr_clear(radius); mpc_set_nan(out); return -1; }
    mpc_t a, z, term, sum, denominator, factor, complete;
    mpc_init2(a, work); mpc_init2(z, work); mpc_init2(term, work); mpc_init2(sum, work);
    mpc_init2(denominator, work); mpc_init2(factor, work); mpc_init2(complete, work);
    mpfr_t order_size, ratio, tail, scale, absolute_sum, error;
    mpfr_inits2(work, order_size, ratio, tail, scale, absolute_sum, error, (mpfr_ptr)0);
    mpc_set(a, order, MPC_RNDNN); mpc_set(z, argument, MPC_RNDNN);
    mpc_abs(order_size, a, MPFR_RNDU);
    bool ok = false;
    if (mpfr_zero_p(mpc_imagref(a)) && mpfr_integer_p(mpc_realref(a)) && mpfr_sgn(mpc_realref(a)) <= 0)
        goto done;
    if (mpfr_zero_p(radius) && mpfr_sgn(mpc_realref(a)) <= 0) goto done;
    mpc_ui_div(term, 1u, a, MPC_RNDNN); mpc_set(sum, term, MPC_RNDNN);
    number_mpc_abs_upper(absolute_sum, term);
    if (number_mpc_gamma_series(sum, absolute_sum, a, z, target + 32)) {
        mpc_div(sum, sum, a, MPC_RNDNN);
        number_mpc_abs_lower(scale, a);
        mpfr_div(absolute_sum, absolute_sum, scale, MPFR_RNDU);
        ok = true;
    }
    for (unsigned long n = 1; !ok && n < 100000; ++n) {
        mpc_add_ui(denominator, a, n, MPC_RNDNN);
        mpc_mul(term, term, z, MPC_RNDNN); mpc_div(term, term, denominator, MPC_RNDNN);
        mpc_add(sum, sum, term, MPC_RNDNN);
        number_mpc_abs_upper(tail, term); mpfr_add(absolute_sum, absolute_sum, tail, MPFR_RNDU);
        mpfr_ui_sub(ratio, n + 1, order_size, MPFR_RNDD);
        if (mpfr_sgn(ratio) <= 0) continue;
        mpfr_div(ratio, radius, ratio, MPFR_RNDU);
        if (mpfr_cmp_d(ratio, 0.5) > 0) continue;
        number_mpc_abs_lower(scale, sum);
        if (number_series_small(tail, scale, work - 40, 1u)) { ok = true; break; }
    }
    if (!ok) goto done;
    if (mpfr_zero_p(radius)) mpc_set_ui(factor, 0u, MPC_RNDNN);
    else {
        mpc_log(factor, z, MPC_RNDNN); mpc_mul(factor, factor, a, MPC_RNDNN);
        mpc_sub(factor, factor, z, MPC_RNDNN); mpc_exp(factor, factor, MPC_RNDNN);
    }
    number_mpc_abs_upper(error, factor);
    mpfr_mul(error, error, absolute_sum, MPFR_RNDU);
    mpfr_mul_2si(error, error, -work + 32, MPFR_RNDU);
    mpc_mul(sum, sum, factor, MPC_RNDNN);
    if (mode != NUMBER_GAMMA_LOWER) {
        if (number_mpc_gamma(complete, a, MPC_RNDNN)) { ok = false; goto done; }
        number_mpc_abs_upper(scale, complete);
        mpfr_mul_2si(scale, scale, -work + 32, MPFR_RNDU); mpfr_add(error, error, scale, MPFR_RNDU);
        if (mode == NUMBER_GAMMA_UPPER || mode == NUMBER_GAMMA_Q) mpc_sub(sum, complete, sum, MPC_RNDNN);
    }
    number_mpc_abs_lower(scale, sum);
    mpfr_mul_2si(scale, scale, -target - 16, MPFR_RNDD);
    ok = mpfr_lessequal_p(error, scale);
    if (mode == NUMBER_GAMMA_P || mode == NUMBER_GAMMA_Q) mpc_div(sum, sum, complete, MPC_RNDNN);
    if (ok) mpc_set(out, sum, rounding);
done:
    if (!ok) mpc_set_nan(out);
    mpc_clear(a); mpc_clear(z); mpc_clear(term); mpc_clear(sum); mpc_clear(denominator);
    mpc_clear(factor); mpc_clear(complete);
    mpfr_clears(radius, order_size, ratio, tail, scale, absolute_sum, error, (mpfr_ptr)0);
    return ok ? 0 : -1;
}

enum { GAMMA_SERIES_POWERS = 14, GAMMA_SERIES_MAX_TERMS = 8192 };

typedef struct _gamma_series_t gamma_series_t;
struct _gamma_series_t {
    mpc_srcptr order;
    mpfr_prec_t precision;
    mpc_t powers[GAMMA_SERIES_POWERS];
    mpfr_t power_bounds[GAMMA_SERIES_POWERS];
    mpfr_t products[4];
};

/* T/Q sums this interval; its left powers start at one, independent of the interval's global index. */
static void gamma_series_split(mpc_ptr q, mpc_ptr t, mpfr_ptr majorant, gamma_series_t *state,
                               unsigned first, unsigned count)
{
    if (count == 1u) {
        mpc_add_ui(q, state->order, first + 1u, MPC_RNDNN);
        mpc_set(t, q, MPC_RNDNN);
        number_mpc_abs_upper(majorant, q);
        return;
    }
    unsigned index = sizeof(unsigned) * CHAR_BIT - 1u - (unsigned)__builtin_clz(count - 1u);
    unsigned left = 1u << index;
    gamma_series_split(q, t, majorant, state, first, left);
    mpc_t right_q, right_t, scratch;
    mpc_init2(right_q, state->precision); mpc_init2(right_t, state->precision);
    mpc_init2(scratch, state->precision);
    mpfr_t right_majorant, q_size;
    mpfr_inits2(64, right_majorant, q_size, (mpfr_ptr)0);
    gamma_series_split(right_q, right_t, right_majorant, state, first + left, count - left);
    number_mpc_abs_upper(q_size, right_q);
    mpfr_mul(majorant, majorant, q_size, MPFR_RNDU);
    mpfr_mul(right_majorant, right_majorant, state->power_bounds[index], MPFR_RNDU);
    mpfr_add(majorant, majorant, right_majorant, MPFR_RNDU);
    number_mpc_mul_guarded(scratch, t, right_q, state->products);
    number_mpc_mul_guarded(t, state->powers[index], right_t, state->products);
    mpc_add(t, t, scratch, MPC_RNDNN);
    number_mpc_mul_guarded(q, q, right_q, state->products);
    mpfr_clears(right_majorant, q_size, (mpfr_ptr)0);
    mpc_clear(right_q); mpc_clear(right_t); mpc_clear(scratch);
}

/* Return a checked sum of z^n/(a+1)_n; callers supply the exponential prefactor separately. */
bool number_mpc_gamma_series(mpc_ptr out, mpfr_ptr absolute_sum, mpc_srcptr a, mpc_srcptr z,
                              mpfr_prec_t target)
{
    mpfr_prec_t precision = mpc_get_prec(out);
    if (precision < 640 || precision > 32768 || target > precision - 32 ||
        !mpfr_number_p(mpc_realref(a)) || !mpfr_number_p(mpc_imagref(a)) ||
        !mpfr_number_p(mpc_realref(z)) || !mpfr_number_p(mpc_imagref(z)) ||
        mpfr_cmp_d(mpc_realref(a), 0.125) < 0 || mpfr_cmp_ui(mpc_realref(a), 16u) > 0 ||
        mpfr_cmpabs_ui(mpc_imagref(a), 1u) > 0) return false;
    mpfr_t radius, term, denominator, ratio, tail, majorant, scale, error;
    mpfr_inits2(64, radius, term, denominator, ratio, tail, majorant, scale, error, (mpfr_ptr)0);
    mpc_abs(radius, z, MPFR_RNDU);
    unsigned count = 0;
    if (mpfr_zero_p(radius) || mpfr_cmp_ui(radius, 8u) > 0) goto done;
    mpfr_set_ui(term, 1u, MPFR_RNDN);
    for (unsigned n = 1; n <= GAMMA_SERIES_MAX_TERMS; ++n) {
        mpfr_add_ui(denominator, mpc_realref(a), n, MPFR_RNDD);
        mpfr_div(ratio, radius, denominator, MPFR_RNDU);
        mpfr_mul(term, term, ratio, MPFR_RNDU);
        if (mpfr_cmp_d(ratio, 0.5) <= 0 && mpfr_cmp_ui_2exp(term, 1u, -precision - 16) < 0) {
            count = n;
            mpfr_mul_2ui(tail, term, 1u, MPFR_RNDU);
            break;
        }
    }
    if (!count) goto done;
    gamma_series_t state = {.order = a, .precision = precision};
    unsigned powers = 0, remaining = count;
    while (remaining) { ++powers; remaining >>= 1; }
    for (unsigned j = 0; j < powers; ++j) {
        mpc_init2(state.powers[j], precision);
        mpfr_init2(state.power_bounds[j], 64);
        if (j) mpc_sqr(state.powers[j], state.powers[j - 1], MPC_RNDNN);
        else mpc_set(state.powers[j], z, MPC_RNDNN);
        number_mpc_abs_upper(state.power_bounds[j], state.powers[j]);
    }
    for (unsigned j = 0; j < 4; ++j) mpfr_init2(state.products[j], precision);
    mpc_t q, t, sum;
    mpc_init2(q, precision); mpc_init2(t, precision); mpc_init2(sum, precision);
    gamma_series_split(q, t, majorant, &state, 0u, count);
    mpc_div(sum, t, q, MPC_RNDNN);
    number_mpc_abs_lower(denominator, q);
    mpfr_div(majorant, majorant, denominator, MPFR_RNDU);
    /* Fewer than 8192 nodes, each with three guarded products and one sum. */
    mpfr_mul_2si(error, majorant, -precision + 24, MPFR_RNDU);
    mpfr_add(error, error, tail, MPFR_RNDU);
    number_mpc_abs_lower(scale, sum);
    mpfr_mul_2si(scale, scale, -target, MPFR_RNDD);
    bool ok = mpfr_number_p(error) && mpfr_number_p(scale) && mpfr_lessequal_p(error, scale);
    if (ok) {
        mpc_set(out, sum, MPC_RNDNN);
        mpfr_set(absolute_sum, majorant, MPFR_RNDU);
    }
    mpc_clear(q); mpc_clear(t); mpc_clear(sum);
    for (unsigned j = 0; j < 4; ++j) mpfr_clear(state.products[j]);
    for (unsigned j = 0; j < powers; ++j) {
        mpc_clear(state.powers[j]); mpfr_clear(state.power_bounds[j]);
    }
    mpfr_clears(radius, term, denominator, ratio, tail, majorant, scale, error, (mpfr_ptr)0);
    return ok;
done:
    mpfr_clears(radius, term, denominator, ratio, tail, majorant, scale, error, (mpfr_ptr)0);
    return false;
}

/* Work backwards from the final precision so its penultimate stage is about half-sized, never almost full-sized. */
static mpfr_prec_t inverse_gamma_initial_precision(mpfr_prec_t final)
{
    while (final > 128) final = (final + 1) / 2;
    return final;
}

static bool inverse_gamma_real_residual(mpfr_srcptr residual, mpfr_srcptr logarithm, mpfr_prec_t target,
                                       mpfr_ptr magnitude, mpfr_ptr bound)
{
    mpfr_abs(magnitude, residual, MPFR_RNDU);
    mpfr_abs(bound, logarithm, MPFR_RNDD);
    if (mpfr_cmp_ui(bound, 1u) < 0) mpfr_set_ui(bound, 1u, MPFR_RNDN);
    mpfr_mul_2si(bound, bound, -target - 8, MPFR_RNDD);
    return mpfr_lessequal_p(magnitude, bound);
}

static bool inverse_gamma_complex_residual(mpc_srcptr residual, mpc_srcptr logarithm, mpfr_prec_t target,
                                          mpfr_ptr magnitude, mpfr_ptr bound)
{
    number_mpc_abs_upper(magnitude, residual);
    number_mpc_abs_lower(bound, logarithm);
    if (mpfr_cmp_ui(bound, 1u) < 0) mpfr_set_ui(bound, 1u, MPFR_RNDN);
    mpfr_mul_2si(bound, bound, -target - 16, MPFR_RNDD);
    return mpfr_lessequal_p(magnitude, bound);
}

/* Refine the existing real inverse-gamma branch without doing early iterations at the final precision. */
int number_mpfr_gammainv_mut(mpfr_t value)
{
    mpfr_prec_t target = mpfr_get_prec(value), final = target + 96;
    if (!mpfr_number_p(value) || mpfr_sgn(value) <= 0) { mpfr_set_nan(value); return 0; }
    mpfr_t source, minimum, x, log_y, log_gamma, derivative, residual, step, bound;
    mpfr_inits2(final, source, minimum, (mpfr_ptr)0);
    mpfr_set(source, value, MPFR_RNDN);
    mpfr_set_str(minimum, "0.885603194410888700278815900582588733207951533669903448871200165", 10, MPFR_RNDN);
    if (mpfr_cmp(source, minimum) < 0) {
        mpfr_set_nan(value);
        mpfr_clears(source, minimum, (mpfr_ptr)0);
        return 0;
    }
    mpfr_prec_t stage = inverse_gamma_initial_precision(final);
    mpfr_inits2(stage, x, log_y, log_gamma, derivative, residual, step, bound, (mpfr_ptr)0);
    mpfr_set_prec(derivative, stage / 2 + 32);
    qfloat_t seed = qf_gammainv(qf_from_double(mpfr_get_d(source, MPFR_RNDN)));
    double initial = qf_to_double(seed);
    if (isfinite(initial) && initial > 0) mpfr_set_d(x, initial, MPFR_RNDN);
    else if (mpfr_cmp_ui(source, 1u) <= 0) mpfr_set_ui(x, 1u, MPFR_RNDN);
    else {
        mpfr_log(x, source, MPFR_RNDN);
        if (mpfr_cmp_d(x, 1.5) < 0) mpfr_set_d(x, 1.5, MPFR_RNDN);
    }
    bool complete = false;
    for (;;) {
        mpfr_log(log_y, source, MPFR_RNDN);
        /* One Newton correction approximately doubles the known bits. Intermediate stages need only show
         * half-stage input accuracy before that correction; the final stage retains both original checks. */
        mpfr_prec_t accuracy = stage == final ? target + 8 : stage / 2 - 16;
        bool converged = false;
        for (unsigned i = 0; i < 64; ++i) {
            mpfr_lngamma(log_gamma, x, MPFR_RNDN);
            mpfr_sub(residual, log_gamma, log_y, MPFR_RNDN);
            mpfr_digamma(derivative, x, MPFR_RNDN);
            if (mpfr_zero_p(derivative) || !mpfr_number_p(derivative)) break;
            mpfr_div(step, residual, derivative, MPFR_RNDN);
            mpfr_abs(bound, step, MPFR_RNDN);
            bool small = mpfr_zero_p(bound) || mpfr_cmp_ui_2exp(bound, 1u, -(long)accuracy) <= 0;
            if (small && stage == final &&
                inverse_gamma_real_residual(residual, log_y, target, log_gamma, bound)) {
                /* This value already passed both checks. Keep it, rather than apply an immaterial correction
                 * and pay for another full-precision logarithmic gamma solely to verify the changed value. */
                complete = converged = true;
                break;
            }
            mpfr_sub(x, x, step, MPFR_RNDN);
            if (!mpfr_number_p(x) || mpfr_sgn(x) <= 0) break;
            if (small) {
                converged = true;
                break;
            }
        }
        if (!converged) break;
        if (stage == final) {
            if (complete) break;
            mpfr_lngamma(log_gamma, x, MPFR_RNDN);
            mpfr_sub(residual, log_gamma, log_y, MPFR_RNDN);
            complete = inverse_gamma_real_residual(residual, log_y, target, log_gamma, bound);
            break;
        }
        stage = stage > final / 2 ? final : stage * 2;
        mpfr_prec_round(x, stage, MPFR_RNDN);
        mpfr_set_prec(log_y, stage); mpfr_set_prec(log_gamma, stage);
        mpfr_set_prec(derivative, stage / 2 + 32);
        mpfr_set_prec(residual, stage); mpfr_set_prec(step, stage); mpfr_set_prec(bound, stage);
    }
    if (complete) mpfr_set(value, x, MPFR_RNDN);
    else mpfr_set_nan(value);
    mpfr_clears(source, minimum, x, log_y, log_gamma, derivative, residual, step, bound, (mpfr_ptr)0);
    return complete ? 0 : -1;
}

/* Refine the logarithmic branch at increasing precision and verify the final defining residual. */
int number_mpc_gammainv(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding)
{
    mpfr_prec_t target = mpc_get_prec(out), final = target + 128;
    if (final > 65536 || !mpfr_number_p(mpc_realref(input)) || !mpfr_number_p(mpc_imagref(input)) ||
        (mpfr_zero_p(mpc_realref(input)) && mpfr_zero_p(mpc_imagref(input)))) {
        mpc_set_nan(out);
        return -1;
    }
    mpfr_prec_t stage = inverse_gamma_initial_precision(final);
    mpc_t value, logarithm, residual, derivative, step;
    mpc_init2(value, stage); mpc_init2(logarithm, stage); mpc_init2(residual, stage);
    mpc_init2(derivative, stage); mpc_init2(step, stage);
    mpc_set_prec(derivative, stage / 2 + 48);
    mpfr_t magnitude, scale;
    mpfr_inits2(stage, magnitude, scale, (mpfr_ptr)0);
    mpc_log(value, input, MPC_RNDNN);
    mpfr_add_d(mpc_realref(value), mpc_realref(value), 1.5, MPFR_RNDN);
    bool complete = false;
    for (;;) {
        mpc_log(logarithm, input, MPC_RNDNN);
        bool converged = false;
        for (unsigned i = 0; i < 48; ++i) {
            if (number_mpc_lgamma(residual, value, MPC_RNDNN) ||
                number_mpc_digamma_mut(derivative, value, MPC_RNDNN)) break;
            mpc_sub(residual, residual, logarithm, MPC_RNDNN);
            mpc_div(step, residual, derivative, MPC_RNDNN);
            number_mpc_abs_upper(magnitude, step);
            number_mpc_abs_lower(scale, value);
            if (mpfr_cmp_ui(scale, 1u) < 0) mpfr_set_ui(scale, 1u, MPFR_RNDN);
            mpfr_mul_2si(scale, scale, stage == final ? -target - 24 : -stage / 2 + 16, MPFR_RNDD);
            bool small = mpfr_number_p(magnitude) && mpfr_lessequal_p(magnitude, scale);
            if (small && stage == final &&
                inverse_gamma_complex_residual(residual, logarithm, target, magnitude, scale)) {
                complete = converged = true;
                break;
            }
            mpc_sub(value, value, step, MPC_RNDNN);
            if (small) { converged = true; break; }
        }
        if (!converged) break;
        if (stage == final) {
            if (complete) break;
            if (number_mpc_lgamma(residual, value, MPC_RNDNN)) break;
            mpc_sub(residual, residual, logarithm, MPC_RNDNN);
            complete = inverse_gamma_complex_residual(residual, logarithm, target, magnitude, scale);
            break;
        }
        stage = stage > final / 2 ? final : stage * 2;
        mpfr_prec_round(mpc_realref(value), stage, MPFR_RNDN);
        mpfr_prec_round(mpc_imagref(value), stage, MPFR_RNDN);
        mpc_set_prec(logarithm, stage); mpc_set_prec(residual, stage);
        mpc_set_prec(derivative, stage / 2 + 48);
        mpc_set_prec(step, stage); mpfr_set_prec(magnitude, stage); mpfr_set_prec(scale, stage);
    }
    if (complete) mpc_set(out, value, rounding);
    else mpc_set_nan(out);
    mpc_clear(value); mpc_clear(logarithm); mpc_clear(residual); mpc_clear(derivative); mpc_clear(step);
    mpfr_clears(magnitude, scale, (mpfr_ptr)0);
    return complete ? 0 : -1;
}
