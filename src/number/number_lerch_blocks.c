/**
 * @file number_lerch_blocks.c
 * @brief Weighted binomial blocks for high-precision Lerch defining sums.
 *
 * Groups consecutive terms around a common centre. A binomial recurrence supplies their inverse powers,
 * while moments of the finite geometric weights sum the whole block without evaluating each power separately.
 * Moments belong to the current call and are generated lazily, with bounded storage. Geometric bounds cover
 * each binomial remainder and the infinite defining-series tail; an absolute roundoff budget covers shortened
 * arithmetic and cancellation. Unsupported parameters or an exhausted budget leave the ordinary series in charge.
 * A single checked result, bounded to small input significands, preserves cheap unchanged-input evaluations.
 * Cold and changing-input evaluation uses the block algorithm independently of that result. This implementation
 * is private to the number module; callers receive copies and no moment storage escapes.
 */
#include <pthread.h>
#include <stdlib.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

enum { LERCH_WIDTH = 64, LERCH_HALF = 32, LERCH_HALF_BITS = 5 };

static mpc_t lerch_saved_argument, lerch_saved_order, lerch_saved_offset, lerch_saved_value;
static mpfr_prec_t lerch_saved_bits;
static pthread_mutex_t lerch_result_lock = PTHREAD_MUTEX_INITIALIZER;

static bool lerch_same(mpc_srcptr left, mpc_srcptr right)
{
    return !mpc_cmp(left, right) &&
        (!mpfr_zero_p(mpc_realref(left)) || mpfr_signbit(mpc_realref(left)) == mpfr_signbit(mpc_realref(right))) &&
        (!mpfr_zero_p(mpc_imagref(left)) || mpfr_signbit(mpc_imagref(left)) == mpfr_signbit(mpc_imagref(right)));
}

static bool lerch_result_copy(mpc_ptr out, mpc_srcptr argument, mpc_srcptr order, mpc_srcptr offset,
                              mpfr_prec_t bits)
{
    if (pthread_mutex_lock(&lerch_result_lock)) return false;
    bool found = lerch_saved_bits >= bits && lerch_same(argument, lerch_saved_argument) &&
                 lerch_same(order, lerch_saved_order) && lerch_same(offset, lerch_saved_offset);
    if (found) mpc_set(out, lerch_saved_value, MPC_RNDNN);
    pthread_mutex_unlock(&lerch_result_lock);
    return found;
}

static void lerch_result_save(mpc_srcptr value, mpc_srcptr argument, mpc_srcptr order, mpc_srcptr offset,
                              mpfr_prec_t bits)
{
    if (mpc_get_prec(argument) > 16384 || mpc_get_prec(order) > 16384 || mpc_get_prec(offset) > 16384 ||
        pthread_mutex_lock(&lerch_result_lock)) return;
    if (!lerch_saved_bits) {
        mpc_init2(lerch_saved_argument, mpc_get_prec(argument)); mpc_init2(lerch_saved_order, mpc_get_prec(order));
        mpc_init2(lerch_saved_offset, mpc_get_prec(offset)); mpc_init2(lerch_saved_value, mpc_get_prec(value));
    } else {
        mpc_set_prec(lerch_saved_argument, mpc_get_prec(argument));
        mpc_set_prec(lerch_saved_order, mpc_get_prec(order));
        mpc_set_prec(lerch_saved_offset, mpc_get_prec(offset)); mpc_set_prec(lerch_saved_value, mpc_get_prec(value));
    }
    mpc_set(lerch_saved_argument, argument, MPC_RNDNN); mpc_set(lerch_saved_order, order, MPC_RNDNN);
    mpc_set(lerch_saved_offset, offset, MPC_RNDNN); mpc_set(lerch_saved_value, value, MPC_RNDNN);
    lerch_saved_bits = bits;
    pthread_mutex_unlock(&lerch_result_lock);
}

typedef struct _lerch_moments_t lerch_moments_t;
struct _lerch_moments_t {
    mpc_t weights[LERCH_WIDTH];
    mpc_t *values;
    unsigned long count, capacity;
    mpfr_prec_t precision;
};

static void lerch_moments_clear(lerch_moments_t *moments)
{
    for (unsigned long j = 0; j < moments->count; ++j) mpc_clear(moments->values[j]);
    for (unsigned k = 0; k < LERCH_WIDTH; ++k) mpc_clear(moments->weights[k]);
    free(moments->values);
}

static bool lerch_moments_init(lerch_moments_t *moments, mpc_srcptr argument, mpfr_prec_t precision)
{
    moments->count = 0;
    moments->capacity = (unsigned long)precision + 1024;
    moments->precision = precision;
    moments->values = calloc(moments->capacity, sizeof *moments->values);
    if (!moments->values) return false;
    for (unsigned k = 0; k < LERCH_WIDTH; ++k) {
        mpc_init2(moments->weights[k], precision);
        if (k) mpc_mul(moments->weights[k], moments->weights[k - 1], argument, MPC_RNDNN);
        else mpc_set_ui(moments->weights[k], 1u, MPC_RNDNN);
    }
    return true;
}

static mpfr_exp_t lerch_upper_exponent(mpc_srcptr value)
{
    bool real_zero = mpfr_zero_p(mpc_realref(value)), imaginary_zero = mpfr_zero_p(mpc_imagref(value));
    if (real_zero && imaginary_zero) return 0;
    mpfr_exp_t real = real_zero ? mpfr_get_exp(mpc_imagref(value)) : mpfr_get_exp(mpc_realref(value));
    mpfr_exp_t imaginary = imaginary_zero ? real : mpfr_get_exp(mpc_imagref(value));
    return (real > imaginary ? real : imaginary) + 1;
}

/* M_j = sum_(k=0)^(B-1) z^k ((k-B/2)/(B/2))^j. Integer multiplication and binary scaling
 * advance all moments without transcendental evaluations. This necessarily visits the B=64 geometric weights. */
static mpc_srcptr lerch_moment(lerch_moments_t *moments, unsigned long index)
{
    if (index >= moments->capacity) return NULL;
    while (moments->count <= index) {
        mpc_ptr value = moments->values[moments->count];
        mpc_init2(value, moments->precision);
        mpc_set_ui(value, 0u, MPC_RNDNN);
        for (int k = 0; k < LERCH_WIDTH; ++k) {
            if (mpfr_zero_p(mpc_realref(moments->weights[k])) &&
                mpfr_zero_p(mpc_imagref(moments->weights[k]))) continue;
            mpc_add(value, value, moments->weights[k], MPC_RNDNN);
            mpfr_exp_t exponent = lerch_upper_exponent(moments->weights[k]);
            if (exponent < -moments->precision - 32) {
                /* Subsequent factors have modulus at most one, so discarding this weight cannot grow
                 * its absolute contribution. It is already below the full-working-precision error allowance. */
                mpc_set_ui(moments->weights[k], 0u, MPC_RNDNN);
                continue;
            }
            mpfr_prec_t next = moments->precision;
            if (exponent < -32)
                next = exponent < 128 - moments->precision ? 128 : moments->precision + exponent + 32;
            mpfr_prec_round(mpc_realref(moments->weights[k]), next, MPFR_RNDN);
            mpfr_prec_round(mpc_imagref(moments->weights[k]), next, MPFR_RNDN);
            mpc_mul_si(moments->weights[k], moments->weights[k], k - LERCH_HALF, MPC_RNDNN);
            mpc_div_2ui(moments->weights[k], moments->weights[k], LERCH_HALF_BITS, MPC_RNDNN);
        }
        ++moments->count;
    }
    return moments->values[index];
}

/* Return an absolute polynomial-tail bound. For r=D/|w|, every subsequent coefficient ratio is
 * at most r*(1+|s|/(j+1)); the moment magnitudes are at most sum |z|^k <= 1/(1-|z|). */
static bool lerch_block_tail(mpfr_ptr tail, mpc_srcptr coefficient, mpfr_srcptr radius,
                             mpfr_srcptr order, mpfr_srcptr geometric, unsigned long index)
{
    mpfr_t ratio, denominator;
    mpfr_inits2(64, ratio, denominator, (mpfr_ptr)0);
    mpfr_div_ui(ratio, order, index + 1, MPFR_RNDU);
    mpfr_add_ui(ratio, ratio, 1u, MPFR_RNDU);
    mpfr_mul(ratio, ratio, radius, MPFR_RNDU);
    mpfr_ui_sub(denominator, 1u, ratio, MPFR_RNDD);
    bool bounded = mpfr_sgn(denominator) > 0;
    if (bounded) {
        mpc_abs(tail, coefficient, MPFR_RNDU);
        mpfr_mul(tail, tail, geometric, MPFR_RNDU);
        mpfr_mul(tail, tail, ratio, MPFR_RNDU);
        mpfr_div(tail, tail, denominator, MPFR_RNDU);
    }
    mpfr_clears(ratio, denominator, (mpfr_ptr)0);
    return bounded;
}

static bool lerch_block(mpc_ptr out, mpfr_ptr tail, unsigned long *used, lerch_moments_t *moments,
                         mpc_srcptr centre, mpc_srcptr order, mpfr_srcptr radius, mpfr_srcptr order_size,
                         mpfr_srcptr geometric, mpfr_prec_t precision)
{
    mpc_t coefficient, factor, increment, temporary, term;
    mpc_init2(coefficient, precision); mpc_init2(factor, precision); mpc_init2(increment, precision);
    mpc_init2(temporary, precision); mpc_init2(term, precision);
    mpc_ui_div(increment, LERCH_HALF, centre, MPC_RNDNN);
    mpc_neg(increment, increment, MPC_RNDNN);
    mpc_mul(factor, order, increment, MPC_RNDNN);
    mpc_set_ui(coefficient, 1u, MPC_RNDNN);
    mpc_set(out, lerch_moment(moments, 0), MPC_RNDNN);
    mpfr_t magnitude, tolerance;
    mpfr_inits2(64, magnitude, tolerance, (mpfr_ptr)0);
    mpfr_set_ui_2exp(tolerance, 1u, -precision + 64, MPFR_RNDD);
    bool complete = false;
    for (unsigned long j = 1; j < moments->capacity; ++j) {
        mpc_abs(magnitude, coefficient, MPFR_RNDU);
        if (!mpfr_number_p(magnitude) || mpfr_zero_p(magnitude)) break;
        mpfr_prec_t next = precision;
        mpfr_exp_t exponent = mpfr_get_exp(magnitude);
        if (exponent < -32) next = exponent < 128 - precision ? 128 : precision + exponent + 32;
        mpfr_prec_round(mpc_realref(coefficient), next, MPFR_RNDN);
        mpfr_prec_round(mpc_imagref(coefficient), next, MPFR_RNDN);
        if (mpc_get_prec(term) != next) { mpc_set_prec(term, next); mpc_set_prec(temporary, next); }
        mpc_set(temporary, factor, MPC_RNDNN);
        mpc_mul(coefficient, coefficient, temporary, MPC_RNDNN);
        mpc_div_ui(coefficient, coefficient, j, MPC_RNDNN);
        mpc_add(factor, factor, increment, MPC_RNDNN);
        mpc_set(temporary, lerch_moment(moments, j), MPC_RNDNN);
        mpc_mul(term, coefficient, temporary, MPC_RNDNN);
        mpc_add(out, out, term, MPC_RNDNN);
        *used = j;
        if (j >= 32 && !(j % 8) &&
            lerch_block_tail(tail, coefficient, radius, order_size, geometric, j) &&
            mpfr_lessequal_p(tail, tolerance)) {
            complete = true;
            break;
        }
    }
    mpfr_clears(magnitude, tolerance, (mpfr_ptr)0);
    mpc_clear(coefficient); mpc_clear(factor); mpc_clear(increment); mpc_clear(temporary); mpc_clear(term);
    return complete;
}

/* Sum weighted binomial blocks when their bounded work is preferable to individual complex powers. */
bool number_lerch_blocks(mpc_ptr out, mpc_srcptr argument, mpc_srcptr order, mpc_srcptr offset, mpfr_prec_t bits)
{
    if (bits < 512 || bits > 8192) return false;
    mpfr_t radius, order_size, offset_size, geometric, end_weight, block_radius, condition;
    mpfr_t magnitude, scale, absolute_sum, error, local_error, remainder, ratio, temporary;
    mpfr_inits2(64, radius, order_size, offset_size, geometric, end_weight, block_radius, condition,
                 magnitude, scale, absolute_sum, error, local_error, remainder, ratio, temporary, (mpfr_ptr)0);
    mpc_abs(radius, argument, MPFR_RNDU); mpc_abs(order_size, order, MPFR_RNDU);
    mpc_abs(offset_size, offset, MPFR_RNDU);
    bool eligible = mpfr_number_p(radius) && mpfr_cmp_d(radius, 0.01) >= 0 && mpfr_cmp_d(radius, 0.9) <= 0 &&
                    mpfr_number_p(order_size) && mpfr_cmp_ui(order_size, 16u) <= 0 &&
                    mpfr_number_p(offset_size) && mpfr_cmp_ui(offset_size, 16u) <= 0;
    if (!eligible) goto bounds_done;
    if (lerch_result_copy(out, argument, order, offset, bits)) goto bounds_done;
    mpfr_ui_sub(geometric, 1u, radius, MPFR_RNDD);
    mpfr_ui_div(geometric, 1u, geometric, MPFR_RNDU);
    mpfr_pow_ui(end_weight, radius, LERCH_WIDTH - 1, MPFR_RNDU);
    mpfr_prec_t work = bits + 192;
    lerch_moments_t moments;
    if (!lerch_moments_init(&moments, argument, work)) { eligible = false; goto bounds_done; }
    mpc_t sum, power, step, centre, anchor, polynomial, term;
    mpc_init2(sum, work); mpc_init2(power, work); mpc_init2(step, work); mpc_init2(centre, work);
    mpc_init2(anchor, work); mpc_init2(polynomial, work); mpc_init2(term, work);
    mpc_set_ui(sum, 0u, MPC_RNDNN); mpc_set_ui(power, 1u, MPC_RNDNN);
    mpc_pow_ui(step, argument, LERCH_WIDTH, MPC_RNDNN);
    mpfr_set_zero(absolute_sum, 1); mpfr_set_zero(error, 1);
    for (unsigned long k = 0; k < LERCH_WIDTH; ++k) {
        number_dirichlet_power(term, offset, order, k);
        mpc_mul(term, term, power, MPC_RNDNN); mpc_add(sum, sum, term, MPC_RNDNN);
        mpc_abs(magnitude, term, MPFR_RNDU); mpfr_add(absolute_sum, absolute_sum, magnitude, MPFR_RNDU);
        mpc_mul(power, power, argument, MPC_RNDNN);
    }
    bool complete = false;
    bool finite = mpfr_number_p(mpc_realref(sum)) && mpfr_number_p(mpc_imagref(sum));
    for (unsigned long k = LERCH_WIDTH; finite && k < 200000; k += LERCH_WIDTH) {
        mpc_add_ui(centre, offset, k + LERCH_HALF, MPC_RNDNN);
        mpc_abs(scale, centre, MPFR_RNDD); mpfr_ui_div(block_radius, LERCH_HALF, scale, MPFR_RNDU);
        if (mpfr_cmp_d(block_radius, 0.5) > 0) break;
        mpc_abs(magnitude, power, MPFR_RNDU);
        if (!mpfr_number_p(magnitude) || mpfr_zero_p(magnitude)) break;
        mpfr_exp_t exponent = mpfr_get_exp(magnitude);
        mpfr_prec_t precision = work;
        if (exponent < 0) precision = exponent < 128 - work ? 128 : work + exponent;
        mpc_set_prec(polynomial, precision); mpc_set_prec(anchor, precision); mpc_set_prec(term, precision);
        number_dirichlet_power(anchor, offset, order, k + LERCH_HALF);
        unsigned long used = 0;
        if (!lerch_block(polynomial, local_error, &used, &moments, centre, order, block_radius,
                          order_size, geometric, precision)) break;
        mpc_abs(scale, anchor, MPFR_RNDU); mpfr_mul(magnitude, magnitude, scale, MPFR_RNDU);
        mpfr_mul(local_error, local_error, magnitude, MPFR_RNDU);
        mpfr_add(error, error, local_error, MPFR_RNDU);
        mpfr_ui_sub(condition, 1u, block_radius, MPFR_RNDD);
        mpfr_neg(temporary, order_size, MPFR_RNDN);
        mpfr_pow(condition, condition, temporary, MPFR_RNDU);
        mpfr_mul(magnitude, magnitude, condition, MPFR_RNDU);
        mpfr_mul(remainder, magnitude, end_weight, MPFR_RNDU);
        mpfr_mul(magnitude, magnitude, geometric, MPFR_RNDU);
        mpfr_add(absolute_sum, absolute_sum, magnitude, MPFR_RNDU);
        /* Sum |C_j| <= (1-r)^(-|s|). Quadratic operation counts and 32 extra error bits cover coefficient
         * recurrences, moment generation/reuse, rounded operands and the omitted coefficient's rounding error. */
        mpfr_mul_ui(local_error, magnitude, used + 1, MPFR_RNDU);
        mpfr_mul_ui(local_error, local_error, used + 1, MPFR_RNDU);
        mpfr_mul_2si(local_error, local_error, -precision + 32, MPFR_RNDU);
        mpfr_add(error, error, local_error, MPFR_RNDU);
        mpc_mul(term, polynomial, anchor, MPC_RNDNN);
        mpc_set(anchor, power, MPC_RNDNN); mpc_mul(term, term, anchor, MPC_RNDNN);
        mpc_add(sum, sum, term, MPC_RNDNN);
        mpfr_ui_sub(temporary, k + LERCH_WIDTH - 1, offset_size, MPFR_RNDD);
        mpfr_ui_div(temporary, 1u, temporary, MPFR_RNDU);
        mpfr_ui_sub(temporary, 1u, temporary, MPFR_RNDD);
        mpfr_neg(ratio, order_size, MPFR_RNDN); mpfr_pow(ratio, temporary, ratio, MPFR_RNDU);
        mpfr_mul(ratio, ratio, radius, MPFR_RNDU);
        if (mpfr_cmp_ui(ratio, 1u) < 0) {
            mpfr_ui_sub(temporary, 1u, ratio, MPFR_RNDD);
            mpfr_div(ratio, ratio, temporary, MPFR_RNDU);
            mpfr_mul(remainder, remainder, ratio, MPFR_RNDU);
            mpfr_add(remainder, remainder, error, MPFR_RNDU);
            mpfr_mul_ui(local_error, absolute_sum, k + LERCH_WIDTH, MPFR_RNDU);
            mpfr_mul_ui(local_error, local_error, k + LERCH_WIDTH, MPFR_RNDU);
            mpfr_mul_2si(local_error, local_error, -work + 32, MPFR_RNDU);
            mpfr_add(remainder, remainder, local_error, MPFR_RNDU);
            number_mpc_abs_lower(scale, sum);
            mpfr_mul_2si(scale, scale, -bits - 8, MPFR_RNDD);
            if (mpfr_number_p(remainder) && mpfr_lessequal_p(remainder, scale)) {
                complete = true;
                break;
            }
            mpfr_sub(temporary, remainder, error, MPFR_RNDU);
            if (mpfr_lessequal_p(temporary, scale) && mpfr_greater_p(error, scale)) break;
        }
        mpc_mul(power, power, step, MPC_RNDNN);
    }
    if (complete) {
        lerch_result_save(sum, argument, order, offset, bits);
        mpc_set(out, sum, MPC_RNDNN);
    }
    eligible = complete;
    mpc_clear(sum); mpc_clear(power); mpc_clear(step); mpc_clear(centre); mpc_clear(anchor);
    mpc_clear(polynomial); mpc_clear(term); lerch_moments_clear(&moments);
bounds_done:
    mpfr_clears(radius, order_size, offset_size, geometric, end_weight, block_radius, condition,
                magnitude, scale, absolute_sum, error, local_error, remainder, ratio, temporary, (mpfr_ptr)0);
    return eligible;
}

static void __attribute__((destructor)) lerch_result_shutdown(void)
{
    if (!lerch_saved_bits) return;
    mpc_clear(lerch_saved_argument); mpc_clear(lerch_saved_order);
    mpc_clear(lerch_saved_offset); mpc_clear(lerch_saved_value);
}
