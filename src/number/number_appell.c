/**
 * @file number_appell.c
 * @brief Guarded real and complex multiprecision algorithms for all four Appell functions.
 *
 * @par Guarded linear-work Appell F1 series for real and complex multiprecision values.
 *
 * Groups the defining double series by total degree. Coefficients of
 * (1-x*t)^(-b1)*(1-y*t)^(-b2) obey a three-term recurrence, avoiding both nested summation and a coefficient
 * array. Linear recurrence factors advance by addition between periodic refreshes. Shifted parameters retain
 * their exact binary data without unnecessary padding; compact complex parameters admit an exact quadratic
 * ratio recurrence with a real denominator. Guarded complex products reuse three real multiplications within
 * the series' absolute error allowance. Real inputs use MPFR directly, without complex temporaries.
 * Bounded rising-factorial cases can shorten late coefficient state while propagating absolute recurrence errors;
 * a failed error check retries the original full-precision recurrence before touching the output.
 * Domain checks and cancellation guards remain native number-module responsibilities.
 * The principal convergent series is supported for |x|, |y| < 0.95, matching the established numerical domain.
 *
 * @par Guarded Appell F2, F3 and F4 series at the active scalar precision.
 *
 * Uses total-degree recurrences for products of confluent hypergeometric generating functions. Resonant
 * auxiliary equations, terminating series and unstable product recurrences use direct triangular summation.
 * Normalised Pochhammer bounds supply an independent absolute tail majorant and working guard bits.
 * Evaluation covers the defining convergence regions and finite terminating polynomials; it does not choose
 * branches for analytic continuation outside those regions. Arguments are borrowed and results retain the
 * number module's precision and ownership policy. All numerical scratch storage belongs to the current call.
 */
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

enum { APPELL_MAX_TERMS = 100000, APPELL_MAX_WORK_BITS = 65536 };

typedef struct _appell_error_t appell_error_t;
struct _appell_error_t {
    mpfr_t current, previous, total, current_size, previous_size, left, right, magnitude, bound;
    bool narrowed;
};

static void appell_error_init(appell_error_t *error)
{
    mpfr_inits2(64, error->current, error->previous, error->total, error->current_size, error->previous_size,
                error->left, error->right, error->magnitude, error->bound, (mpfr_ptr)0);
    mpfr_set_zero(error->current, 1); mpfr_set_zero(error->previous, 1); mpfr_set_zero(error->total, 1);
    error->narrowed = false;
}

static void appell_error_clear(appell_error_t *error)
{
    mpfr_clears(error->current, error->previous, error->total, error->current_size, error->previous_size,
                error->left, error->right, error->magnitude, error->bound, (mpfr_ptr)0);
}

/* Propagate absolute errors through the coefficient recurrence. With |x|,|y|<=7/16 and |b1|+|b2|<=8,
 * the divided recurrence factors have moduli at most 7/2 and 441/256. A factor of 32 bounds their construction,
 * input rounding and the two guarded products even when a computed coefficient cancels to zero.
 * The 2^16 allowance also covers periodic refreshes, shortened input copies and division rounding. */
static mpfr_prec_t appell_error_precision(appell_error_t *error, mpfr_prec_t work, unsigned long n)
{
    mpfr_div_ui(error->left, error->left, n + 1u, MPFR_RNDU);
    mpfr_div_ui(error->right, error->right, n + 1u, MPFR_RNDU);
    mpfr_set_ui_2exp(error->bound, 1u, -work + 32, MPFR_RNDU);
    mpfr_add(error->left, error->left, error->bound, MPFR_RNDU);
    mpfr_add(error->right, error->right, error->bound, MPFR_RNDU);
    mpfr_add(error->magnitude, error->current_size, error->previous_size, MPFR_RNDU);
    mpfr_mul_2ui(error->magnitude, error->magnitude, 5u, MPFR_RNDU);
    mpfr_prec_t precision = work;
    if (n >= 128 && mpfr_number_p(error->magnitude) && !mpfr_zero_p(error->magnitude)) {
        mpfr_exp_t exponent = mpfr_get_exp(error->magnitude);
        if (exponent < -32) {
            precision = exponent < 96 - work ? 128 : work + exponent + 32;
            precision = ((precision + 63) / 64) * 64;
            if (precision > work) precision = work;
        }
    }
    error->narrowed = error->narrowed || precision < work;
    mpfr_mul(error->bound, error->current, error->left, MPFR_RNDU);
    mpfr_mul(error->current_size, error->previous, error->right, MPFR_RNDU);
    mpfr_add(error->bound, error->bound, error->current_size, MPFR_RNDU);
    mpfr_mul_2si(error->previous_size, error->magnitude, -precision + 16, MPFR_RNDU);
    mpfr_add(error->bound, error->bound, error->previous_size, MPFR_RNDU);
    mpfr_set(error->previous, error->current, MPFR_RNDU);
    mpfr_set(error->current, error->bound, MPFR_RNDU);
    /* The positive-real-part rising-factorial quotient has modulus at most one. */
    mpfr_add(error->total, error->total, error->current, MPFR_RNDU);
    return precision;
}

/* Retain every input bit and every integer shift, without padding simple dyadic parameters to full precision. */
static mpfr_prec_t appell_shift_precision(mpfr_srcptr input, mpfr_prec_t work)
{
    const mpfr_exp_t shift_bits = sizeof(unsigned) * CHAR_BIT - __builtin_clz((unsigned)APPELL_MAX_TERMS);
    if (mpfr_zero_p(input)) return shift_bits + 1;
    mpfr_prec_t significant = mpfr_min_prec(input);
    mpfr_exp_t exponent = mpfr_get_exp(input);
    if (significant > work || exponent > work || exponent < -work) return work;
    mpfr_exp_t lower = exponent - significant;
    mpfr_exp_t upper = exponent > shift_bits ? exponent : shift_bits;
    mpfr_prec_t needed = upper + 1 - (lower < 0 ? lower : 0);
    return needed < work ? needed : work;
}

/* Bound a common binary scale for both components and every permitted integer shift. */
static mpfr_prec_t appell_complex_shift_precision(mpc_srcptr input, mpfr_prec_t work)
{
    const mpfr_srcptr components[] = {mpc_realref(input), mpc_imagref(input)};
    mpfr_exp_t upper = sizeof(unsigned) * CHAR_BIT - __builtin_clz((unsigned)APPELL_MAX_TERMS), lower = 0;
    for (size_t i = 0; i < 2; ++i) {
        if (mpfr_zero_p(components[i])) continue;
        mpfr_prec_t significant = mpfr_min_prec(components[i]);
        mpfr_exp_t exponent = mpfr_get_exp(components[i]);
        if (significant > work || exponent > work || exponent < -work) return work;
        if (exponent > upper) upper = exponent;
        if (exponent - significant < lower) lower = exponent - significant;
    }
    mpfr_prec_t needed = upper + 1 - lower;
    return needed < work ? needed : work;
}

static bool appell_finite(mpc_srcptr value)
{
    return mpfr_number_p(mpc_realref(value)) && mpfr_number_p(mpc_imagref(value));
}

/* For c=a+d, (a)_n/(c)_n = (a)_d/(a+n)_d. Compact parameters keep both polynomials exact. */
static unsigned appell_rising_difference(mpfr_srcptr a, mpfr_srcptr c, mpfr_srcptr ai, mpfr_srcptr ci,
                                          mpfr_prec_t work, mpfr_prec_t *compact)
{
    if (work < 640 || mpfr_sgn(a) <= 0 || (ai && !mpfr_equal_p(ai, ci))) return 0;
    const mpfr_srcptr components[] = {a, c, ai, ci};
    mpfr_prec_t precision = MPFR_PREC_MIN;
    for (unsigned j = 0; j < 4; ++j) {
        if (!components[j]) continue;
        mpfr_prec_t needed = appell_shift_precision(components[j], work);
        if (needed > precision) precision = needed;
    }
    if (precision > work / 8) return 0;
    mpfr_t difference;
    mpfr_init2(difference, 2 * precision + 2);
    mpfr_sub(difference, c, a, MPFR_RNDN);
    unsigned count = mpfr_integer_p(difference) && mpfr_sgn(difference) > 0 &&
                     mpfr_cmp_ui(difference, 8u) <= 0 ? (unsigned)mpfr_get_ui(difference, MPFR_RNDN) : 0;
    mpfr_clear(difference);
    if (!count || 2 * count * (precision + 2) + 4 > work / 2) return 0;
    *compact = 2 * count * (precision + 2) + 4;
    return count;
}

static void appell_multiply(mpc_ptr out, mpc_srcptr left, mpc_srcptr right, mpfr_t *scratch)
{
    if (mpc_get_prec(out) >= 640) number_mpc_mul_guarded(out, left, right, scratch);
    else mpc_mul(out, left, right, MPC_RNDNN);
}

/* Evaluate the total-degree recurrence with explicit roundoff and convergence checks. */
static int appell_mpc_evaluate(mpc_ptr out, mpc_srcptr a, mpc_srcptr b1, mpc_srcptr b2, mpc_srcptr c,
                               mpc_srcptr x, mpc_srcptr y, mpc_rnd_t rounding, bool allow_short)
{
    mpfr_prec_t target = mpc_get_prec(out), work = target + 96;
    if (work > APPELL_MAX_WORK_BITS) { mpc_set_nan(out); return -1; }
    const mpc_srcptr inputs[] = {a, b1, b2, c, x, y};
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i)
        if (!appell_finite(inputs[i])) { mpc_set_nan(out); return -1; }
    mpc_t previous, current, next, ratio, sum, term, scratch, denominator, upper, xy, xy_sum, weighted, b_sum;
    mpc_t forward, backward, numerator, numerator_step, rising_coefficient;
    mpc_init2(previous, work); mpc_init2(current, work); mpc_init2(next, work); mpc_init2(ratio, work);
    mpc_init2(sum, work); mpc_init2(term, work); mpc_init2(scratch, work); mpc_init2(denominator, work);
    mpc_init2(upper, work); mpc_init2(xy, work); mpc_init2(xy_sum, work); mpc_init2(weighted, work);
    mpc_init2(b_sum, work);
    mpc_init2(forward, work); mpc_init2(backward, work);
    mpfr_prec_t ar = appell_shift_precision(mpc_realref(a), work);
    mpfr_prec_t ai = appell_shift_precision(mpc_imagref(a), work);
    mpfr_prec_t cr = appell_shift_precision(mpc_realref(c), work);
    mpfr_prec_t ci = appell_shift_precision(mpc_imagref(c), work);
    mpc_set_prec(upper, ar > ai ? ar : ai);
    mpc_set_prec(denominator, cr > ci ? cr : ci);
    mpfr_prec_t ap = appell_complex_shift_precision(a, work), cp = appell_complex_shift_precision(c, work);
    mpfr_prec_t compact = MPFR_PREC_MIN;
    unsigned rising = appell_rising_difference(mpc_realref(a), mpc_realref(c), mpc_imagref(a),
                                               mpc_imagref(c), work, &compact);
    bool exact_ratio = !rising && ap + cp + 1 <= work && 2 * cp + 1 <= work;
    mpc_init2(numerator, rising ? compact : exact_ratio ? ap + cp + 1 : MPFR_PREC_MIN);
    mpc_init2(numerator_step, rising ? compact : exact_ratio ? ap + cp + 1 : MPFR_PREC_MIN);
    mpc_init2(rising_coefficient, compact);
    mpfr_t magnitude, absolute_sum, threshold, radius, norm, norm_step;
    mpfr_t products[4];
    for (unsigned i = 0; i < 4; ++i) mpfr_init2(products[i], work);
    mpfr_inits2(work, magnitude, absolute_sum, threshold, radius, (mpfr_ptr)0);
    mpfr_inits2(rising ? compact : exact_ratio ? 2 * cp + 1 : MPFR_PREC_MIN, norm, norm_step, (mpfr_ptr)0);
    bool converged = false;
    bool shortened = false;
    appell_error_t error;
    mpc_t rounded_forward, rounded_backward;
    mpc_abs(radius, x, MPFR_RNDU);
    if (mpfr_cmp_d(radius, 0.95) >= 0) goto done;
    bool small_arguments = mpfr_cmp_d(radius, 0.4375) <= 0;
    if (rising) {
        mpc_set_ui(numerator, 1u, MPC_RNDNN);
        for (unsigned j = 0; j < rising; ++j) {
            mpc_add_ui(upper, a, j, MPC_RNDNN);
            mpc_mul(numerator, numerator, upper, MPC_RNDNN);
        }
    }
    if (exact_ratio) {
        /* P_n = (a+n)*conj(c+n), R_n = |c+n|^2; both quadratic recurrences remain exact. */
        mpc_conj(scratch, c, MPC_RNDNN);
        mpc_mul(numerator, a, scratch, MPC_RNDNN);
        mpc_add(numerator_step, a, scratch, MPC_RNDNN);
        mpc_add_ui(numerator_step, numerator_step, 1u, MPC_RNDNN);
        mpc_norm(norm, c, MPFR_RNDN);
        mpfr_mul_2ui(norm_step, mpc_realref(c), 1u, MPFR_RNDN);
        mpfr_add_ui(norm_step, norm_step, 1u, MPFR_RNDN);
    }
    mpc_abs(radius, y, MPFR_RNDU);
    if (mpfr_cmp_d(radius, 0.95) >= 0) goto done;
    small_arguments = small_arguments && mpfr_cmp_d(radius, 0.4375) <= 0;
    if (!mpc_cmp(a, c) && !(mpfr_zero_p(mpc_imagref(c)) &&
        mpfr_integer_p(mpc_realref(c)) && mpfr_sgn(mpc_realref(c)) <= 0) &&
        !mpc_ui_sub(scratch, 1u, x, MPC_RNDNN) && !mpc_ui_sub(xy, 1u, y, MPC_RNDNN)) {
        /* F1(a,b1,b2;a;x,y) = (1-x)^(-b1) (1-y)^(-b2). */
        mpc_neg(next, b1, MPC_RNDNN); mpc_pow(sum, scratch, next, MPC_RNDNN);
        mpc_neg(next, b2, MPC_RNDNN); mpc_pow(scratch, xy, next, MPC_RNDNN);
        mpc_mul(sum, sum, scratch, MPC_RNDNN);
        converged = appell_finite(sum);
        goto done;
    }
    mpc_mul(xy, x, y, MPC_RNDNN);
    mpc_add(xy_sum, x, y, MPC_RNDNN);
    mpc_mul(weighted, b1, x, MPC_RNDNN);
    mpc_mul(scratch, b2, y, MPC_RNDNN);
    mpc_add(weighted, weighted, scratch, MPC_RNDNN);
    mpc_add(b_sum, b1, b2, MPC_RNDNN);
    if (allow_short && rising && work >= 1024 && work <= 8320 && small_arguments) {
        mpc_abs(magnitude, xy_sum, MPFR_RNDU); mpc_abs(threshold, xy, MPFR_RNDU);
        mpfr_add(magnitude, magnitude, threshold, MPFR_RNDU);
        bool contraction = mpfr_cmp_d(magnitude, 0.875) <= 0;
        number_mpc_abs_upper(magnitude, b1); number_mpc_abs_upper(threshold, b2);
        mpfr_add(magnitude, magnitude, threshold, MPFR_RNDU);
        shortened = contraction && mpfr_cmp_ui(magnitude, 8u) <= 0;
        if (shortened) {
            appell_error_init(&error);
            mpc_init2(rounded_forward, work); mpc_init2(rounded_backward, work);
        }
    }
    mpc_set_ui(previous, 0u, MPC_RNDNN);
    mpc_set_ui(current, 1u, MPC_RNDNN);
    mpc_set_ui(ratio, 1u, MPC_RNDNN);
    mpc_set_ui(sum, 1u, MPC_RNDNN);
    mpfr_set_ui(absolute_sum, 1u, MPFR_RNDN);
    unsigned small = 0;
    for (unsigned long n = 0; n < APPELL_MAX_TERMS; ++n) {
        mpc_add_ui(upper, a, n, MPC_RNDNN);
        if (mpfr_zero_p(mpc_realref(upper)) && mpfr_zero_p(mpc_imagref(upper))) {
            converged = true;
            break;
        }
        if (exact_ratio) {
            if (mpfr_zero_p(norm)) goto done;
            mpc_mul(ratio, ratio, numerator, MPC_RNDNN);
            mpc_div_fr(ratio, ratio, norm, MPC_RNDNN);
        } else if (!rising) {
            mpc_add_ui(denominator, c, n, MPC_RNDNN);
            if (mpfr_zero_p(mpc_realref(denominator)) && mpfr_zero_p(mpc_imagref(denominator))) goto done;
            mpc_div(scratch, upper, denominator, MPC_RNDNN);
            mpc_mul(ratio, ratio, scratch, MPC_RNDNN);
        }
        if (!(n & 63ul)) {
            mpc_mul_ui(forward, xy_sum, n, MPC_RNDNN);
            mpc_add(forward, forward, weighted, MPC_RNDNN);
            mpc_add_si(backward, b_sum, (long)n - 1, MPC_RNDNN);
            mpc_mul(backward, backward, xy, MPC_RNDNN);
        }
        mpc_srcptr next_forward = forward, next_backward = backward;
        if (shortened) {
            number_mpc_abs_upper(error.current_size, current); number_mpc_abs_upper(error.previous_size, previous);
            mpc_abs(error.left, forward, MPFR_RNDU); mpc_abs(error.right, backward, MPFR_RNDU);
            mpfr_prec_t precision = appell_error_precision(&error, work, n);
            mpfr_prec_round(mpc_realref(current), precision, MPFR_RNDN);
            mpfr_prec_round(mpc_imagref(current), precision, MPFR_RNDN);
            mpfr_prec_round(mpc_realref(previous), precision, MPFR_RNDN);
            mpfr_prec_round(mpc_imagref(previous), precision, MPFR_RNDN);
            if (mpc_get_prec(next) != precision) mpc_set_prec(next, precision);
            if (mpc_get_prec(scratch) != precision) mpc_set_prec(scratch, precision);
            if (mpc_get_prec(rounded_forward) != precision) {
                mpc_set_prec(rounded_forward, precision); mpc_set_prec(rounded_backward, precision);
                for (unsigned i = 0; i < 4; ++i) mpfr_set_prec(products[i], precision);
            }
            mpc_set(rounded_forward, forward, MPC_RNDNN); mpc_set(rounded_backward, backward, MPC_RNDNN);
            next_forward = rounded_forward; next_backward = rounded_backward;
        }
        appell_multiply(next, next_forward, current, products);
        appell_multiply(scratch, next_backward, previous, products);
        mpc_sub(next, next, scratch, MPC_RNDNN);
        mpc_div_ui(next, next, n + 1, MPC_RNDNN);
        if (rising) {
            mpc_set_ui(numerator_step, 1u, MPC_RNDNN);
            for (unsigned j = 0; j < rising; ++j) {
                mpc_add_ui(upper, a, n + j + 1u, MPC_RNDNN);
                mpc_mul(numerator_step, numerator_step, upper, MPC_RNDNN);
            }
            mpc_conj(rising_coefficient, numerator_step, MPC_RNDNN);
            mpc_mul(rising_coefficient, rising_coefficient, numerator, MPC_RNDNN);
            mpc_norm(norm, numerator_step, MPFR_RNDN);
            mpc_mul(term, next, rising_coefficient, MPC_RNDNN);
            mpc_div_fr(term, term, norm, MPC_RNDNN);
        } else appell_multiply(term, ratio, next, products);
        mpc_add(sum, sum, term, MPC_RNDNN);
        if (!appell_finite(sum) || !appell_finite(term)) goto done;
        number_mpc_abs_upper(magnitude, term);
        mpfr_add(absolute_sum, absolute_sum, magnitude, MPFR_RNDU);
        number_mpc_abs_lower(threshold, sum);
        small = number_series_small(magnitude, threshold, target + 32, 0u) ? small + 1 : 0;
        if (small >= 8) { converged = true; break; }
        mpc_swap(previous, current);
        mpc_swap(current, next);
        mpc_add(forward, forward, xy_sum, MPC_RNDNN);
        mpc_add(backward, backward, xy, MPC_RNDNN);
        if (exact_ratio) {
            mpc_add(numerator, numerator, numerator_step, MPC_RNDNN);
            mpc_add_ui(numerator_step, numerator_step, 2u, MPC_RNDNN);
            mpfr_add(norm, norm, norm_step, MPFR_RNDN);
            mpfr_add_ui(norm_step, norm_step, 2u, MPFR_RNDN);
        }
    }
    if (converged) {
        mpfr_mul_2si(magnitude, absolute_sum, -work + 24, MPFR_RNDU);
        if (shortened) {
            mpfr_add(magnitude, magnitude, error.total, MPFR_RNDU);
            if (error.narrowed) {
                /* After n>=128 the factor-norm sum is below 15/16. Two steps shrink the homogeneous
                 * state error by this bound; 64 times both final errors covers its entire omitted tail. */
                mpfr_add(error.bound, error.current, error.previous, MPFR_RNDU);
                mpfr_mul_2ui(error.bound, error.bound, 6u, MPFR_RNDU);
                mpfr_add(magnitude, magnitude, error.bound, MPFR_RNDU);
            }
        }
        number_mpc_abs_lower(threshold, sum);
        mpfr_mul_2si(threshold, threshold, -target - 16, MPFR_RNDD);
        converged = mpfr_lessequal_p(magnitude, threshold);
    }
done:
    if (converged) mpc_set(out, sum, rounding);
    else if (!shortened) mpc_set_nan(out);
    if (shortened) {
        appell_error_clear(&error);
        mpc_clear(rounded_forward); mpc_clear(rounded_backward);
    }
    mpfr_clears(magnitude, absolute_sum, threshold, radius, norm, norm_step, (mpfr_ptr)0);
    for (unsigned i = 0; i < 4; ++i) mpfr_clear(products[i]);
    mpc_clear(previous); mpc_clear(current); mpc_clear(next); mpc_clear(ratio); mpc_clear(sum); mpc_clear(term);
    mpc_clear(scratch); mpc_clear(denominator); mpc_clear(upper); mpc_clear(xy); mpc_clear(xy_sum);
    mpc_clear(weighted); mpc_clear(b_sum);
    mpc_clear(forward); mpc_clear(backward);
    mpc_clear(numerator); mpc_clear(numerator_step);
    mpc_clear(rising_coefficient);
    if (shortened && !converged) return appell_mpc_evaluate(out, a, b1, b2, c, x, y, rounding, false);
    return converged ? 0 : -1;
}

/* Evaluate the guarded complex series, accepting shortened state only after its absolute error check. */
int number_mpc_appell_F1(mpc_ptr out, mpc_srcptr a, mpc_srcptr b1, mpc_srcptr b2, mpc_srcptr c,
                         mpc_srcptr x, mpc_srcptr y, mpc_rnd_t rounding)
{
    return appell_mpc_evaluate(out, a, b1, b2, c, x, y, rounding, true);
}

/* Evaluate real operands directly, retaining the complex kernel's convergence and cancellation guards. */
static int appell_mpfr_evaluate(mpfr_ptr out, mpfr_srcptr a, mpfr_srcptr b1, mpfr_srcptr b2, mpfr_srcptr c,
                                mpfr_srcptr x, mpfr_srcptr y, bool allow_short)
{
    mpfr_prec_t target = mpfr_get_prec(out), work = target + 96;
    if (work > APPELL_MAX_WORK_BITS) { mpfr_set_nan(out); return -1; }
    const mpfr_srcptr inputs[] = {a, b1, b2, c, x, y};
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i)
        if (!mpfr_number_p(inputs[i])) { mpfr_set_nan(out); return -1; }
    mpfr_t previous, current, next, ratio, sum, term, scratch, denominator, upper;
    mpfr_t xy, xy_sum, weighted, b_sum, forward, backward;
    mpfr_t magnitude, absolute_sum, threshold, cutoff;
    mpfr_inits2(work, previous, current, next, ratio, sum, term, scratch, denominator, upper,
                xy, xy_sum, weighted, b_sum, forward, backward, magnitude, absolute_sum, threshold,
                cutoff, (mpfr_ptr)0);
    mpfr_set_prec(upper, appell_shift_precision(a, work));
    mpfr_set_prec(denominator, appell_shift_precision(c, work));
    bool compact_ratio = mpfr_get_prec(upper) + mpfr_get_prec(denominator) + 1 <= work;
    mpfr_prec_t compact = MPFR_PREC_MIN;
    unsigned rising = appell_rising_difference(a, c, NULL, NULL, work, &compact);
    mpfr_t rising_numerator, rising_denominator;
    mpfr_inits2(compact, rising_numerator, rising_denominator, (mpfr_ptr)0);
    if (rising) {
        mpfr_set_ui(rising_numerator, 1u, MPFR_RNDN);
        for (unsigned j = 0; j < rising; ++j) {
            mpfr_add_ui(upper, a, j, MPFR_RNDN);
            mpfr_mul(rising_numerator, rising_numerator, upper, MPFR_RNDN);
        }
    }
    bool converged = false;
    bool shortened = false;
    appell_error_t error;
    mpfr_t rounded_forward, rounded_backward;
    mpfr_set_d(cutoff, 0.95, MPFR_RNDN);
    if (mpfr_cmpabs(x, cutoff) >= 0 || mpfr_cmpabs(y, cutoff) >= 0) goto done;
    if (mpfr_equal_p(a, c) && !(mpfr_integer_p(c) && mpfr_sgn(c) <= 0) &&
        !mpfr_ui_sub(scratch, 1u, x, MPFR_RNDN) && !mpfr_ui_sub(xy, 1u, y, MPFR_RNDN)) {
        mpfr_neg(next, b1, MPFR_RNDN); mpfr_pow(sum, scratch, next, MPFR_RNDN);
        mpfr_neg(next, b2, MPFR_RNDN); mpfr_pow(scratch, xy, next, MPFR_RNDN);
        mpfr_mul(sum, sum, scratch, MPFR_RNDN);
        converged = mpfr_number_p(sum);
        goto done;
    }
    mpfr_mul(xy, x, y, MPFR_RNDN);
    mpfr_add(xy_sum, x, y, MPFR_RNDN);
    mpfr_mul(weighted, b1, x, MPFR_RNDN);
    mpfr_mul(scratch, b2, y, MPFR_RNDN);
    mpfr_add(weighted, weighted, scratch, MPFR_RNDN);
    mpfr_add(b_sum, b1, b2, MPFR_RNDN);
    mpfr_set_d(cutoff, 0.4375, MPFR_RNDN);
    if (allow_short && rising && work >= 1024 && work <= 8320 &&
        mpfr_cmpabs(x, cutoff) <= 0 && mpfr_cmpabs(y, cutoff) <= 0) {
        mpfr_abs(magnitude, xy_sum, MPFR_RNDU); mpfr_abs(threshold, xy, MPFR_RNDU);
        mpfr_add(magnitude, magnitude, threshold, MPFR_RNDU);
        bool contraction = mpfr_cmp_d(magnitude, 0.875) <= 0;
        mpfr_abs(magnitude, b1, MPFR_RNDU); mpfr_abs(threshold, b2, MPFR_RNDU);
        mpfr_add(magnitude, magnitude, threshold, MPFR_RNDU);
        shortened = contraction && mpfr_cmp_ui(magnitude, 8u) <= 0;
        if (shortened) {
            appell_error_init(&error);
            mpfr_inits2(work, rounded_forward, rounded_backward, (mpfr_ptr)0);
        }
    }
    mpfr_set_zero(previous, 1);
    mpfr_set_ui(current, 1u, MPFR_RNDN);
    mpfr_set_ui(ratio, 1u, MPFR_RNDN);
    mpfr_set_ui(sum, 1u, MPFR_RNDN);
    mpfr_set_ui(absolute_sum, 1u, MPFR_RNDN);
    unsigned small = 0;
    for (unsigned long n = 0; n < APPELL_MAX_TERMS; ++n) {
        mpfr_add_ui(upper, a, n, MPFR_RNDN);
        if (mpfr_zero_p(upper)) { converged = true; break; }
        mpfr_add_ui(denominator, c, n, MPFR_RNDN);
        if (mpfr_zero_p(denominator)) goto done;
        if (!rising && compact_ratio) {
            mpfr_mul(ratio, ratio, upper, MPFR_RNDN);
            mpfr_div(ratio, ratio, denominator, MPFR_RNDN);
        } else if (!rising) {
            mpfr_div(scratch, upper, denominator, MPFR_RNDN);
            mpfr_mul(ratio, ratio, scratch, MPFR_RNDN);
        }
        if (!(n & 63ul)) {
            mpfr_mul_ui(forward, xy_sum, n, MPFR_RNDN);
            mpfr_add(forward, forward, weighted, MPFR_RNDN);
            mpfr_add_si(backward, b_sum, (long)n - 1, MPFR_RNDN);
            mpfr_mul(backward, backward, xy, MPFR_RNDN);
        }
        mpfr_srcptr next_forward = forward, next_backward = backward;
        if (shortened) {
            mpfr_abs(error.current_size, current, MPFR_RNDU); mpfr_abs(error.previous_size, previous, MPFR_RNDU);
            mpfr_abs(error.left, forward, MPFR_RNDU); mpfr_abs(error.right, backward, MPFR_RNDU);
            mpfr_prec_t precision = appell_error_precision(&error, work, n);
            mpfr_prec_round(current, precision, MPFR_RNDN); mpfr_prec_round(previous, precision, MPFR_RNDN);
            if (mpfr_get_prec(next) != precision) mpfr_set_prec(next, precision);
            if (mpfr_get_prec(scratch) != precision) mpfr_set_prec(scratch, precision);
            if (mpfr_get_prec(rounded_forward) != precision) {
                mpfr_set_prec(rounded_forward, precision); mpfr_set_prec(rounded_backward, precision);
            }
            mpfr_set(rounded_forward, forward, MPFR_RNDN); mpfr_set(rounded_backward, backward, MPFR_RNDN);
            next_forward = rounded_forward; next_backward = rounded_backward;
        }
        mpfr_mul(next, next_forward, current, MPFR_RNDN);
        mpfr_mul(scratch, next_backward, previous, MPFR_RNDN);
        mpfr_sub(next, next, scratch, MPFR_RNDN);
        mpfr_div_ui(next, next, n + 1, MPFR_RNDN);
        if (rising) {
            mpfr_set_ui(rising_denominator, 1u, MPFR_RNDN);
            for (unsigned j = 0; j < rising; ++j) {
                mpfr_add_ui(upper, a, n + j + 1u, MPFR_RNDN);
                mpfr_mul(rising_denominator, rising_denominator, upper, MPFR_RNDN);
            }
            mpfr_mul(term, next, rising_numerator, MPFR_RNDN);
            mpfr_div(term, term, rising_denominator, MPFR_RNDN);
        } else mpfr_mul(term, ratio, next, MPFR_RNDN);
        mpfr_add(sum, sum, term, MPFR_RNDN);
        if (!mpfr_number_p(sum) || !mpfr_number_p(term)) goto done;
        mpfr_abs(magnitude, term, MPFR_RNDU);
        mpfr_add(absolute_sum, absolute_sum, magnitude, MPFR_RNDU);
        mpfr_abs(threshold, sum, MPFR_RNDD);
        small = number_series_small(magnitude, threshold, target + 32, 0u) ? small + 1 : 0;
        if (small >= 8) { converged = true; break; }
        mpfr_swap(previous, current);
        mpfr_swap(current, next);
        mpfr_add(forward, forward, xy_sum, MPFR_RNDN);
        mpfr_add(backward, backward, xy, MPFR_RNDN);
    }
    if (converged) {
        mpfr_mul_2si(magnitude, absolute_sum, -work + 24, MPFR_RNDU);
        if (shortened) {
            mpfr_add(magnitude, magnitude, error.total, MPFR_RNDU);
            if (error.narrowed) {
                mpfr_add(error.bound, error.current, error.previous, MPFR_RNDU);
                mpfr_mul_2ui(error.bound, error.bound, 6u, MPFR_RNDU);
                mpfr_add(magnitude, magnitude, error.bound, MPFR_RNDU);
            }
        }
        mpfr_abs(threshold, sum, MPFR_RNDD);
        mpfr_mul_2si(threshold, threshold, -target - 16, MPFR_RNDD);
        converged = mpfr_lessequal_p(magnitude, threshold);
    }
done:
    if (converged) mpfr_set(out, sum, MPFR_RNDN);
    else if (!shortened) mpfr_set_nan(out);
    if (shortened) {
        appell_error_clear(&error);
        mpfr_clears(rounded_forward, rounded_backward, (mpfr_ptr)0);
    }
    mpfr_clears(previous, current, next, ratio, sum, term, scratch, denominator, upper,
                xy, xy_sum, weighted, b_sum, forward, backward, magnitude, absolute_sum, threshold,
                cutoff, (mpfr_ptr)0);
    mpfr_clears(rising_numerator, rising_denominator, (mpfr_ptr)0);
    if (shortened && !converged) return appell_mpfr_evaluate(out, a, b1, b2, c, x, y, false);
    return converged ? 0 : -1;
}

/* Evaluate the real series with the same checked precision selection and fallback as the complex kernel. */
int number_mpfr_appell_F1(mpfr_ptr out, mpfr_srcptr a, mpfr_srcptr b1, mpfr_srcptr b2, mpfr_srcptr c,
                          mpfr_srcptr x, mpfr_srcptr y)
{
    return appell_mpfr_evaluate(out, a, b1, b2, c, x, y, true);
}

enum { APPELL_MAX_DEGREE = 20000, APPELL_MAX_CELLS = 2000000, APPELL_MAX_WORK = 32768 };

static bool appell_set_real(mpfr_ptr out, number_t value)
{
    const number_private_t *impl = number_impl_const(&value);
    if (impl->kind == NUMBER_MPZ) mpfr_set_z(out, number_mpz_value(impl->value.mpz), MPFR_RNDN);
    else if (impl->kind == NUMBER_MPQ) mpfr_set_q(out, number_mpq_value(impl->value.mpq), MPFR_RNDN);
    else if (impl->kind == NUMBER_MPFR) {
        if (number_mpfr_ensure(impl->value.mpfr, (size_t)mpfr_get_prec(out)) != 0) return false;
        mpfr_set(out, number_mpfr_value(impl->value.mpfr), MPFR_RNDN);
    } else {
        qfloat_t fixed = num_to_qfloat(value);
        mpfr_set_d(out, fixed.hi, MPFR_RNDN); mpfr_add_d(out, out, fixed.lo, MPFR_RNDN);
    }
    return mpfr_number_p(out);
}

static bool appell_set(mpc_ptr out, number_t value)
{
    NUM_SCOPE(scope);
    return appell_set_real(mpc_realref(out), num_real_part(value)) &&
           appell_set_real(mpc_imagref(out), num_imag_part(value));
}

static long appell_termination(mpc_srcptr value)
{
    if (!mpfr_zero_p(mpc_imagref(value)) || mpfr_sgn(mpc_realref(value)) > 0 ||
        !mpfr_integer_p(mpc_realref(value))) return LONG_MAX;
    if (!mpfr_fits_slong_p(mpc_realref(value), MPFR_RNDN)) return APPELL_MAX_DEGREE + 1L;
    long integer = mpfr_get_si(mpc_realref(value), MPFR_RNDN);
    return integer < -APPELL_MAX_DEGREE ? APPELL_MAX_DEGREE + 1L : -integer;
}

/* |(z)_n/n!| <= (n+1)^ceil(|z-1|). The inverse has a bounded finite prefix and polynomial growth. */
static bool appell_parameter_bound(mpfr_ptr constant, unsigned *degree, mpc_srcptr value, bool inverse)
{
    mpfr_t distance, part, modulus, ratio;
    mpfr_inits2(64, distance, part, modulus, ratio, (mpfr_ptr)0);
    mpfr_sub_ui(distance, mpc_realref(value), 1u, MPFR_RNDA); mpfr_abs(distance, distance, MPFR_RNDU);
    mpfr_abs(part, mpc_imagref(value), MPFR_RNDU); mpfr_add(distance, distance, part, MPFR_RNDU);
    if (inverse) mpfr_mul_2ui(distance, distance, 1u, MPFR_RNDU);
    bool ok = mpfr_number_p(distance) && mpfr_cmp_ui(distance, 1024u) <= 0;
    unsigned exponent = ok ? (unsigned)mpfr_get_ui(distance, MPFR_RNDU) : 0;
    if (ok && inverse) {
        mpc_t shifted;
        mpc_init2(shifted, mpc_get_prec(value));
        for (unsigned j = 0; j <= exponent; ++j) {
            mpc_add_ui(shifted, value, j, MPC_RNDNN); mpc_abs(modulus, shifted, MPFR_RNDD);
            if (mpfr_zero_p(modulus)) { ok = false; break; }
            mpfr_ui_div(ratio, j + 1u, modulus, MPFR_RNDU);
            if (mpfr_cmp_ui(ratio, 1u) > 0) mpfr_mul(constant, constant, ratio, MPFR_RNDU);
        }
        mpc_clear(shifted);
    }
    *degree += exponent;
    mpfr_clears(distance, part, modulus, ratio, (mpfr_ptr)0);
    return ok;
}

/* Canonical argument layout: F2(a,b1,b2,c1,c2,x,y), F3(a1,a2,b1,b2,c,x,y), F4(a,b,c1,c2,x,y). */
static bool appell_bounds(unsigned family, mpc_t *z, mpfr_ptr radius, mpfr_ptr constant,
                           unsigned *degree, long *last, long *m_last, long *n_last)
{
    unsigned count = family == 1 || family == 4 ? 6u : 7u;
    mpfr_t x, y;
    mpfr_inits2(64, x, y, (mpfr_ptr)0);
    mpc_abs(x, z[count - 2], MPFR_RNDU); mpc_abs(y, z[count - 1], MPFR_RNDU);
    *last = *m_last = *n_last = LONG_MAX;
    if (mpfr_zero_p(x)) *m_last = 0;
    if (mpfr_zero_p(y)) *n_last = 0;
    mpfr_set_ui(constant, 1u, MPFR_RNDN); *degree = family == 1 || family == 3 ? 1u : 0u;
    bool ok = true;
    unsigned lower = family == 1 || family == 2 ? 3u : family == 3 ? 4u : 2u;
    unsigned lower_count = family == 1 || family == 3 ? 1u : 2u;
    for (unsigned j = 0; j < lower; ++j) {
        long end = appell_termination(z[j]);
        if (family == 4 || ((family == 1 || family == 2) && j == 0)) { if (end < *last) *last = end; }
        else {
            bool first = family == 1 || family == 2 ? j == 1 : (j == 0 || j == 2);
            long *axis = first ? m_last : n_last;
            if (end < *axis) *axis = end;
        }
        ok = appell_parameter_bound(constant, degree, z[j], false) && ok;
    }
    for (unsigned j = lower; j < lower + lower_count; ++j) {
        if (appell_termination(z[j]) != LONG_MAX) ok = false;
        ok = appell_parameter_bound(constant, degree, z[j], true) && ok;
    }
    if (*m_last != LONG_MAX && *n_last != LONG_MAX && *m_last + *n_last < *last)
        *last = *m_last + *n_last;
    if (family == 2) mpfr_add(radius, x, y, MPFR_RNDU);
    else if (family == 1 || family == 3) mpfr_max(radius, x, y, MPFR_RNDU);
    else {
        mpfr_sqrt(x, x, MPFR_RNDU); mpfr_sqrt(y, y, MPFR_RNDU);
        mpfr_add(radius, x, y, MPFR_RNDU); mpfr_sqr(radius, radius, MPFR_RNDU);
    }
    ok = ok && (*last != LONG_MAX || mpfr_cmp_ui(radius, 1u) < 0);
    mpfr_clears(x, y, (mpfr_ptr)0);
    return ok;
}

/* Bound the entire omitted diagonal tail by C*(k+2)^d*r^(k+1)/(1-r*((k+3)/(k+2))^d). */
static bool appell_tail(mpfr_ptr bound, mpfr_srcptr constant, mpfr_srcptr radius, mpfr_srcptr power,
                         unsigned degree, unsigned k)
{
    mpfr_t ratio, term;
    mpfr_inits2(64, ratio, term, (mpfr_ptr)0);
    mpfr_set_ui(ratio, k + 3u, MPFR_RNDU); mpfr_div_ui(ratio, ratio, k + 2u, MPFR_RNDU);
    mpfr_pow_ui(ratio, ratio, degree, MPFR_RNDU); mpfr_mul(ratio, ratio, radius, MPFR_RNDU);
    bool ok = mpfr_cmp_ui(ratio, 1u) < 0;
    if (ok) {
        mpfr_set_ui(term, k + 2u, MPFR_RNDU); mpfr_pow_ui(term, term, degree, MPFR_RNDU);
        mpfr_mul(term, term, power, MPFR_RNDU); mpfr_mul(term, term, constant, MPFR_RNDU);
        mpfr_ui_sub(ratio, 1u, ratio, MPFR_RNDD); mpfr_div(bound, term, ratio, MPFR_RNDU);
    }
    mpfr_clears(ratio, term, (mpfr_ptr)0);
    return ok;
}

static void appell_ratio(mpc_ptr out, mpc_t *z, unsigned family, unsigned m, unsigned n, bool first)
{
    unsigned total = m + n, count = family == 4 ? 6u : 7u;
    mpc_t factor, divisor;
    mpc_init2(factor, mpc_get_prec(out)); mpc_init2(divisor, mpc_get_prec(out));
    unsigned index = first ? m : n;
    mpc_set(out, z[count - (first ? 2u : 1u)], MPC_RNDNN);
    if (family == 2) {
        mpc_add_ui(factor, z[0], total, MPC_RNDNN); mpc_mul(out, out, factor, MPC_RNDNN);
        mpc_add_ui(factor, z[first ? 1 : 2], index, MPC_RNDNN); mpc_mul(out, out, factor, MPC_RNDNN);
        mpc_add_ui(divisor, z[first ? 3 : 4], index, MPC_RNDNN);
    } else if (family == 3) {
        mpc_add_ui(factor, z[first ? 0 : 1], index, MPC_RNDNN); mpc_mul(out, out, factor, MPC_RNDNN);
        mpc_add_ui(factor, z[first ? 2 : 3], index, MPC_RNDNN); mpc_mul(out, out, factor, MPC_RNDNN);
        mpc_add_ui(divisor, z[4], total, MPC_RNDNN);
    } else {
        mpc_add_ui(factor, z[0], total, MPC_RNDNN); mpc_mul(out, out, factor, MPC_RNDNN);
        mpc_add_ui(factor, z[1], total, MPC_RNDNN); mpc_mul(out, out, factor, MPC_RNDNN);
        mpc_add_ui(divisor, z[first ? 2 : 3], index, MPC_RNDNN);
    }
    mpc_mul_ui(divisor, divisor, index + 1u, MPC_RNDNN); mpc_div(out, out, divisor, MPC_RNDNN);
    mpc_clear(divisor); mpc_clear(factor);
}

/* A rectangular prefix of the triangular series also handles zero axes and terminating numerator parameters. */
static bool appell_direct(mpc_ptr sum, mpc_t *z, unsigned family, unsigned limit, long m_last, long n_last)
{
    size_t width = m_last < (long)limit ? (size_t)m_last + 1u : (size_t)limit + 1u;
    mpfr_prec_t work = mpc_get_prec(sum);
    if (width > 32u * 1024u * 1024u / (2u * (sizeof(mpc_t) + 2u * mpfr_custom_get_size(work)))) return false;
    mpc_t *row = calloc(width, sizeof *row), *next = calloc(width, sizeof *next);
    if (!row || !next) { free(row); free(next); return false; }
    for (size_t j = 0; j < width; ++j) {
        mpc_init2(row[j], work); mpc_init2(next[j], work); mpc_set_ui(row[j], 0u, MPC_RNDNN);
    }
    mpc_set_ui(row[0], 1u, MPC_RNDNN); mpc_set_ui(sum, 1u, MPC_RNDNN);
    mpc_t ratio;
    mpc_init2(ratio, work);
    size_t cells = 0;
    bool ok = true;
    for (unsigned k = 1; k <= limit; ++k) {
        unsigned low = k > (unsigned long)n_last ? k - (unsigned)n_last : 0u;
        unsigned high = k < width ? k : (unsigned)width - 1u;
        if (low > high || (cells += high - low + 1u) > APPELL_MAX_CELLS) { ok = false; break; }
        if (low) mpc_set_ui(next[low - 1u], 0u, MPC_RNDNN);
        for (unsigned m = low; m <= high; ++m) {
            if (!m) {
                appell_ratio(ratio, z, family, 0, k - 1u, false);
                mpc_mul(next[m], row[m], ratio, MPC_RNDNN);
            } else {
                appell_ratio(ratio, z, family, m - 1u, k - m, true);
                mpc_mul(next[m], row[m - 1u], ratio, MPC_RNDNN);
            }
            mpc_add(sum, sum, next[m], MPC_RNDNN);
        }
        mpc_t *swap = row; row = next; next = swap;
    }
    mpc_clear(ratio);
    for (size_t j = 0; j < width; ++j) { mpc_clear(next[j]); mpc_clear(row[j]); }
    free(next); free(row);
    return ok;
}

/* The four states are the scaled coefficients of fg, (theta f)g, f(theta g), (theta f)(theta g). */
static bool appell_linear(mpc_ptr sum, mpc_t *z, unsigned family, unsigned limit, mpfr_srcptr radius)
{
    mpfr_prec_t work = mpc_get_prec(sum);
    unsigned count = family == 4 ? 6u : 7u;
    mpc_t state[4], next[4], a0, c0, b, d, x, y, factor, divisor, term, omega;
    for (unsigned j = 0; j < 4; ++j) {
        mpc_init2(state[j], work); mpc_init2(next[j], work); mpc_set_ui(state[j], 0u, MPC_RNDNN);
    }
    mpc_init2(a0, work); mpc_init2(c0, work); mpc_init2(b, work); mpc_init2(d, work);
    mpc_init2(x, work); mpc_init2(y, work); mpc_init2(factor, work); mpc_init2(divisor, work);
    mpc_init2(term, work); mpc_init2(omega, work);
    mpc_set(x, z[count - 2], MPC_RNDNN); mpc_set(y, z[count - 1], MPC_RNDNN);
    bool ok = true;
    if (family == 3) {
        mpc_add(a0, z[0], z[2], MPC_RNDNN); mpc_add(c0, z[1], z[3], MPC_RNDNN);
        mpc_mul(b, z[0], z[2], MPC_RNDNN); mpc_mul(d, z[1], z[3], MPC_RNDNN);
        mpc_add(divisor, x, y, MPC_RNDNN); mpc_mul(omega, x, y, MPC_RNDNN);
        if (mpc_cmp_si(divisor, 0) == 0) ok = false;
        else {
            mpc_div(omega, omega, divisor, MPC_RNDNN);
            mpfr_t size;
            mpfr_init2(size, 64); mpc_abs(size, omega, MPFR_RNDU);
            ok = mpfr_lessequal_p(size, radius); mpfr_clear(size);
        }
    } else {
        unsigned lower = family == 2 ? 3u : 2u;
        mpc_ui_sub(a0, 1u, z[lower], MPC_RNDNN); mpc_ui_sub(c0, 1u, z[lower + 1], MPC_RNDNN);
        mpc_add(term, z[lower], z[lower + 1], MPC_RNDNN);
        /* Avoid auxiliary resonances and their near neighbours, which are not poles of Appell itself. */
        if (mpfr_cmp_ui(mpc_realref(term), 1u) <= 0) ok = false;
        if (family == 2) {
            mpc_mul(b, z[1], x, MPC_RNDNN); mpc_mul(d, z[2], y, MPC_RNDNN);
        } else { mpc_set(b, x, MPC_RNDNN); mpc_set(d, y, MPC_RNDNN); }
    }
    mpc_set_ui(state[0], 1u, MPC_RNDNN); mpc_set_ui(sum, 1u, MPC_RNDNN);
    for (unsigned k = 0; ok && k < limit; ++k) {
        if (family == 3) {
            mpc_add_ui(divisor, z[4], k, MPC_RNDNN); mpc_ui_div(factor, 1u, divisor, MPC_RNDNN);
            mpc_add(term, a0, c0, MPC_RNDNN); mpc_add_ui(term, term, k, MPC_RNDNN);
            mpc_mul(next[3], term, state[3], MPC_RNDNN);
            mpc_mul(term, b, state[2], MPC_RNDNN); mpc_add(next[3], next[3], term, MPC_RNDNN);
            mpc_mul(term, d, state[1], MPC_RNDNN); mpc_add(next[3], next[3], term, MPC_RNDNN);
            mpc_mul(next[3], next[3], omega, MPC_RNDNN); mpc_mul(next[3], next[3], factor, MPC_RNDNN);
            for (unsigned axis = 1; axis <= 2; ++axis) {
                mpc_add_ui(term, axis == 1 ? a0 : c0, k, MPC_RNDNN);
                mpc_mul(next[axis], term, state[axis], MPC_RNDNN);
                mpc_mul(term, axis == 1 ? b : d, state[0], MPC_RNDNN);
                mpc_add(next[axis], next[axis], term, MPC_RNDNN);
                mpc_sub(next[axis], next[axis], state[3], MPC_RNDNN);
                mpc_mul(next[axis], next[axis], axis == 1 ? x : y, MPC_RNDNN);
                mpc_mul(next[axis], next[axis], factor, MPC_RNDNN);
            }
        } else {
            mpc_add_ui(factor, z[0], k, MPC_RNDNN);
            if (family == 4) { mpc_add_ui(term, z[1], k, MPC_RNDNN); mpc_mul(factor, factor, term, MPC_RNDNN); }
            mpc_mul(next[3], b, state[2], MPC_RNDNN);
            mpc_mul(term, d, state[1], MPC_RNDNN); mpc_add(next[3], next[3], term, MPC_RNDNN);
            if (family == 2) {
                mpc_add(term, x, y, MPC_RNDNN); mpc_mul(term, term, state[3], MPC_RNDNN);
                mpc_add(next[3], next[3], term, MPC_RNDNN);
            }
            mpc_mul(next[3], next[3], factor, MPC_RNDNN);
            if (!k) mpc_set_ui(next[3], 0u, MPC_RNDNN);
            else {
                mpc_add(divisor, a0, c0, MPC_RNDNN); mpc_ui_sub(divisor, k + 1u, divisor, MPC_RNDNN);
                mpc_div(next[3], next[3], divisor, MPC_RNDNN);
            }
            for (unsigned axis = 1; axis <= 2; ++axis) {
                mpc_mul(next[axis], axis == 1 ? b : d, state[0], MPC_RNDNN);
                if (family == 2) {
                    mpc_mul(term, axis == 1 ? x : y, state[axis], MPC_RNDNN);
                    mpc_add(next[axis], next[axis], term, MPC_RNDNN);
                }
                mpc_mul(next[axis], next[axis], factor, MPC_RNDNN);
                mpc_add(next[axis], next[axis], next[3], MPC_RNDNN);
                mpc_ui_sub(divisor, k + 1u, axis == 1 ? a0 : c0, MPC_RNDNN);
                mpc_div(next[axis], next[axis], divisor, MPC_RNDNN);
            }
        }
        mpc_add(next[0], next[1], next[2], MPC_RNDNN); mpc_div_ui(next[0], next[0], k + 1u, MPC_RNDNN);
        mpc_add(sum, sum, next[0], MPC_RNDNN);
        for (unsigned j = 0; j < 4; ++j) mpc_set(state[j], next[j], MPC_RNDNN);
    }
    mpc_clear(omega); mpc_clear(term); mpc_clear(divisor); mpc_clear(factor);
    mpc_clear(y); mpc_clear(x); mpc_clear(d); mpc_clear(b); mpc_clear(c0); mpc_clear(a0);
    for (unsigned j = 0; j < 4; ++j) { mpc_clear(next[j]); mpc_clear(state[j]); }
    return ok;
}

/* Share the independently bounded truncation and arithmetic guard with commuting-matrix series. */
static bool appell_plan(unsigned family, mpc_t *z, size_t target, mpfr_ptr radius,
                         unsigned *limit_out, size_t *guard_out, long *last_out, long *m_out, long *n_out)
{
    mpfr_t constant, power, bound, logarithm, gap;
    mpfr_inits2(64, constant, power, bound, logarithm, gap, (mpfr_ptr)0);
    unsigned degree = 0, limit = 0;
    long last, m_last, n_last;
    bool ok = appell_bounds(family, z, radius, constant, &degree, &last, &m_last, &n_last);
    mpfr_set_ui(power, 1u, MPFR_RNDN);
    if (ok && last != LONG_MAX) { limit = (unsigned)last; ok = last <= APPELL_MAX_DEGREE; }
    else if (ok) {
        for (limit = 0; limit < APPELL_MAX_DEGREE; ++limit) {
            mpfr_mul(power, power, radius, MPFR_RNDU);
            if (appell_tail(bound, constant, radius, power, degree, limit) &&
                mpfr_cmp_ui_2exp(bound, 1u, -(mpfr_exp_t)target - 48) < 0) break;
        }
        ok = limit < APPELL_MAX_DEGREE;
    }
    size_t guard = 128;
    if (ok && last == LONG_MAX) {
        mpfr_ui_sub(gap, 1u, radius, MPFR_RNDD); mpfr_log2(gap, gap, MPFR_RNDD);
        mpfr_log2(logarithm, constant, MPFR_RNDU);
        mpfr_mul_ui(gap, gap, degree + 1u, MPFR_RNDD); mpfr_sub(logarithm, logarithm, gap, MPFR_RNDU);
        if (degree) {
            mpfr_set_ui(gap, degree, MPFR_RNDN); mpfr_log2(gap, gap, MPFR_RNDU);
            mpfr_mul_ui(gap, gap, degree, MPFR_RNDU); mpfr_add(logarithm, logarithm, gap, MPFR_RNDU);
        }
        if (!mpfr_number_p(logarithm) || mpfr_cmp_ui(logarithm, APPELL_MAX_WORK - target - guard) > 0) ok = false;
        else if (mpfr_sgn(logarithm) > 0) guard += (size_t)mpfr_get_ui(logarithm, MPFR_RNDU);
    }
    if (ok && last != LONG_MAX) {
        /* A finite polynomial may be evaluated outside its series disc; protect large individual terms. */
        mpfr_add_ui(logarithm, radius, 1u, MPFR_RNDU); mpfr_log2(logarithm, logarithm, MPFR_RNDU);
        mpfr_mul_ui(logarithm, logarithm, limit, MPFR_RNDU);
        mpfr_set_ui(gap, limit + 1u, MPFR_RNDU); mpfr_log2(gap, gap, MPFR_RNDU);
        mpfr_mul_ui(gap, gap, degree, MPFR_RNDU); mpfr_add(logarithm, logarithm, gap, MPFR_RNDU);
        mpfr_log2(gap, constant, MPFR_RNDU); mpfr_add(logarithm, logarithm, gap, MPFR_RNDU);
        if (!mpfr_number_p(logarithm) || mpfr_cmp_ui(logarithm, APPELL_MAX_WORK - target - guard) > 0) ok = false;
        else if (mpfr_sgn(logarithm) > 0) guard += (size_t)mpfr_get_ui(logarithm, MPFR_RNDU);
    }
    *limit_out = limit; *guard_out = guard; *last_out = last; *m_out = m_last; *n_out = n_last;
    mpfr_clears(constant, power, bound, logarithm, gap, (mpfr_ptr)0);
    return ok;
}

/* Supply the shared scalar majorant to commuting-matrix functional calculus without exposing MPC state. */
bool number_appell_series_plan(unsigned family, const number_t *parameters, number_t x_radius,
                                number_t y_radius, size_t precision, number_appell_plan_t *plan)
{
    if (family < 1 || family > 4 || !parameters || !plan || !precision || precision > APPELL_MAX_WORK - 256 ||
        !num_is_real(x_radius) || !num_is_real(y_radius) || num_lt(x_radius, NUM_ZERO) ||
        num_lt(y_radius, NUM_ZERO)) return false;
    unsigned count = family == 1 || family == 4 ? 6u : 7u;
    mpc_t z[7];
    bool ok = true;
    for (unsigned j = 0; j < count; ++j) {
        mpc_init2(z[j], (mpfr_prec_t)precision + 128);
        number_t input = j < count - 2u ? parameters[j] : j == count - 2u ? x_radius : y_radius;
        ok = num_is_finite(input) && appell_set(z[j], input) && ok;
    }
    mpfr_t radius;
    mpfr_init2(radius, 64);
    unsigned limit = 0;
    size_t guard = 128;
    long last, m_last, n_last;
    ok = ok && appell_plan(family, z, precision, radius, &limit, &guard, &last, &m_last, &n_last);
    if (ok) *plan = (number_appell_plan_t){limit, precision + guard, m_last, n_last};
    mpfr_clear(radius);
    for (unsigned j = 0; j < count; ++j) mpc_clear(z[j]);
    return ok;
}

static number_t appell_number(unsigned family, const number_t *arguments)
{
    NUM_SCOPE(scope);
    unsigned count = family == 4 ? 6u : 7u;
    size_t target = 0;
    bool real = true, native = true, fixed = false, complex_fixed = false, real_fixed = false;
    for (unsigned j = 0; j < count; ++j) {
        if (!num_is_finite(arguments[j])) return num_clone(NUM_NAN);
        size_t precision = num_get_prec_bits(arguments[j]);
        if (precision > target) target = precision;
        number_kind_t kind = number_impl_const(&arguments[j])->kind;
        real = real && num_is_real(arguments[j]);
        native = native && kind != NUMBER_MPFR && kind != NUMBER_COMPLEX;
        fixed = fixed || kind == NUMBER_QFLOAT || kind == NUMBER_DOUBLE;
        real_fixed = real_fixed || kind == NUMBER_QFLOAT;
        complex_fixed = complex_fixed || kind == NUMBER_QCOMPLEX;
    }
    if (native && (fixed || complex_fixed)) {
        qcomplex_t q[7];
        for (unsigned j = 0; j < count; ++j) q[j] = number_value_to_qcomplex(&arguments[j]);
        qcomplex_t result = family == 2 ? qc_appell_F2(q[0], q[1], q[2], q[3], q[4], q[5], q[6]) :
            family == 3 ? qc_appell_F3(q[0], q[1], q[2], q[3], q[4], q[5], q[6]) :
                          qc_appell_F4(q[0], q[1], q[2], q[3], q[4], q[5]);
        if (complex_fixed || !real) return num_create_from_qcomplex(result);
        return real_fixed ? num_create_from_qfloat(qc_real(result)) :
                            num_create_from_double(qf_to_double(qc_real(result)));
    }
    if (!target) target = num_get_default_prec_bits();
    if (target > APPELL_MAX_WORK - 256) return num_clone(NUM_NAN);
    mpc_t z[7], result;
    for (unsigned j = 0; j < count; ++j) mpc_init2(z[j], (mpfr_prec_t)target + 128);
    mpc_init2(result, (mpfr_prec_t)target + 128);
    bool ok = true;
    for (unsigned j = 0; j < count; ++j) ok = appell_set(z[j], arguments[j]) && ok;
    mpfr_t radius;
    mpfr_init2(radius, 64);
    unsigned limit = 0;
    size_t guard = 128;
    long last, m_last, n_last;
    ok = appell_plan(family, z, target, radius, &limit, &guard, &last, &m_last, &n_last) && ok;
    number_t output = num_clone(NUM_NAN);
    if (ok) {
        for (unsigned j = 0; j < count; ++j) {
            mpc_set_prec(z[j], (mpfr_prec_t)(target + guard));
            appell_set(z[j], arguments[j]);
        }
        mpc_set_prec(result, (mpfr_prec_t)(target + guard));
        if ((last == LONG_MAX && appell_linear(result, z, family, limit, radius)) ||
            appell_direct(result, z, family, limit, m_last, n_last)) {
            if (real) output = number_take_mpfr(number_mpfr_from_mpfr(mpc_realref(result), target));
            else output = number_take(number_wrap_complex_mpc(result, target));
        }
    }
    mpfr_clear(radius);
    mpc_clear(result);
    for (unsigned j = 0; j < count; ++j) mpc_clear(z[j]);
    return num_scope_detach(output);
}

/* Evaluate the first Appell function through its two-variable Lauricella representation. */
number_t num_appell_F1(const number_t a, const number_t b1, const number_t b2, const number_t c, const number_t x,
                       const number_t y)
{
    const number_t b[2] = {b1, b2};
    const number_t variables[2] = {x, y};

    return num_lauricella_F(a, b, c, variables, 2u);
}

/* Evaluate the second Appell function without reducing an arbitrary-precision argument. */
number_t num_appell_F2(number_t a, number_t b1, number_t b2, number_t c1, number_t c2, number_t x, number_t y)
{
    const number_t arguments[] = {a, b1, b2, c1, c2, x, y};
    number_t result = appell_number(2, arguments); number_scope_register_value(&result); return result;
}

/* Evaluate the third Appell function, including finite terminating double polynomials. */
number_t num_appell_F3(number_t a1, number_t a2, number_t b1, number_t b2, number_t c, number_t x, number_t y)
{
    const number_t arguments[] = {a1, a2, b1, b2, c, x, y};
    number_t result = appell_number(3, arguments); number_scope_register_value(&result); return result;
}

/* Evaluate the fourth Appell function in its defining convergence region or as a finite polynomial. */
number_t num_appell_F4(number_t a, number_t b, number_t c1, number_t c2, number_t x, number_t y)
{
    const number_t arguments[] = {a, b, c1, c2, x, y};
    number_t result = appell_number(4, arguments); number_scope_register_value(&result); return result;
}
