/**
 * @file number_lerch.c
 * @brief Guarded real, blocked and finite rational evaluations of Lerch's transcendent.
 *
 * @par Checked finite rational evaluations of Lerch's transcendent at non-positive integer orders.
 *
 * For s=-m, applying a+z*d/dz repeatedly to 1/(1-z) gives P_m(z)/(1-z)^(m+1). A descending
 * coefficient recurrence constructs P_m without numerical differentiation or an infinite series. Extra working
 * precision and a uniform absolute error bound cover coefficient growth, cancellation and the denominator.
 * The number module uses this bounded attempt for real and complex arguments; failures leave its checked
 * defining-series fallback in charge. Short dyadic components use exact Gaussian-integer coefficients, with
 * only the final rational divisions rounded. Input conversions and coefficient storage are local and released on return.
 *
 * @par Guarded MPFR defining sums for real Lerch parameters.
 *
 * Positive offsets keep every inverse power real. Short Horner blocks and decreasing coefficient precision
 * reduce arithmetic work while directed geometric-tail and roundoff bounds check the final relative accuracy.
 * The bounded inverse-power cache retains its exact MPC keys, but real cache misses and all accumulation use
 * MPFR. No input or cache storage escapes; unsupported domains remain the caller's responsibility.
 *
 * @par Weighted binomial blocks for high-precision Lerch defining sums.
 *
 * Groups consecutive terms around a common centre. A binomial recurrence supplies their inverse powers,
 * while moments of the finite geometric weights sum the whole block without evaluating each power separately.
 * Moments belong to the current call and are generated lazily, with bounded storage. Geometric bounds cover
 * each binomial remainder and the infinite defining-series tail; an absolute roundoff budget covers shortened
 * arithmetic and cancellation. Unsupported parameters or an exhausted budget leave the ordinary series in charge.
 * A single checked result, bounded to small input significands, preserves cheap unchanged-input evaluations.
 * Cold and changing-input evaluation uses the block algorithm independently of that result. This implementation
 * is private to the number module; callers receive copies and no moment storage escapes.
 * Symmetric geometric-weight pairs halve moment updates. Fixed-point integer weights retain separate even
 * and odd moments, with extra fractional bits bounding truncation independently of cancellation.
 */
#include <pthread.h>
#include <stdlib.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

typedef struct _lerch_gaussian_t lerch_gaussian_t;
struct _lerch_gaussian_t {
    mpz_t real, imaginary;
};

static void lerch_gaussian_init(lerch_gaussian_t *value)
{
    mpz_init(value->real); mpz_init(value->imaginary);
}

static void lerch_gaussian_clear(lerch_gaussian_t *value)
{
    mpz_clear(value->imaginary); mpz_clear(value->real);
}

/* Three disjoint temporaries permit either operand to alias the result. */
static void lerch_gaussian_multiply(lerch_gaussian_t *out, const lerch_gaussian_t *left,
                                     const lerch_gaussian_t *right, mpz_t *scratch)
{
    mpz_mul(scratch[0], left->real, right->real);
    mpz_mul(scratch[1], left->imaginary, right->imaginary);
    mpz_mul(scratch[2], left->real, right->imaginary);
    mpz_addmul(scratch[2], left->imaginary, right->real);
    mpz_sub(out->real, scratch[0], scratch[1]); mpz_set(out->imaginary, scratch[2]);
}

/* Return a common binary denominator for short dyadic components. Larger significands use guarded MPC. */
static bool lerch_gaussian_input(lerch_gaussian_t *out, unsigned *denominator_bits, mpc_srcptr input)
{
    mpfr_srcptr parts[] = {mpc_realref(input), mpc_imagref(input)};
    mpz_ptr values[] = {out->real, out->imaginary};
    unsigned bits = 0;
    for (unsigned k = 0; k < 2; ++k) {
        if (mpfr_zero_p(parts[k])) continue;
        mpfr_prec_t precision = mpfr_min_prec(parts[k]);
        mpfr_exp_t least = mpfr_get_exp(parts[k]) - precision;
        if (precision > 64 || least < -64 || mpfr_get_exp(parts[k]) > 64) return false;
        if (least < 0 && (unsigned)-least > bits) bits = (unsigned)-least;
    }
    for (unsigned k = 0; k < 2; ++k) {
        if (mpfr_zero_p(parts[k])) { mpz_set_ui(values[k], 0u); continue; }
        mpfr_exp_t exponent = mpfr_get_z_2exp(values[k], parts[k]);
        mp_bitcnt_t trailing = mpz_scan1(values[k], 0u);
        mpz_tdiv_q_2exp(values[k], values[k], trailing);
        exponent += (mpfr_exp_t)trailing;
        mpz_mul_2exp(values[k], values[k], (mp_bitcnt_t)(exponent + bits));
    }
    *denominator_bits = bits;
    return true;
}

/* Evaluate the rational function exactly for converted dyadics with at most 64 significant and denominator bits.
 * Coefficients use a common denominator D^m; Horner evaluation at Z/E gives E*R/[D^m*(E-Z)^(m+1)].
 * Gaussian-integer arithmetic remains bounded to short polynomials; only the two final divisions are rounded. */
static bool lerch_integer_dyadic(mpc_ptr out, mpc_srcptr argument, mpc_srcptr offset, unsigned degree)
{
    lerch_gaussian_t z, a, factor, left, right, value, denominator, coefficients[65];
    lerch_gaussian_init(&z); lerch_gaussian_init(&a);
    unsigned z_bits = 0, a_bits = 0;
    bool ok = lerch_gaussian_input(&z, &z_bits, argument) && lerch_gaussian_input(&a, &a_bits, offset);
    if (!ok) { lerch_gaussian_clear(&a); lerch_gaussian_clear(&z); return false; }
    lerch_gaussian_init(&factor); lerch_gaussian_init(&left); lerch_gaussian_init(&right);
    lerch_gaussian_init(&value); lerch_gaussian_init(&denominator);
    for (unsigned k = 0; k <= degree; ++k) lerch_gaussian_init(coefficients + k);
    mpz_set_ui(coefficients[0].real, 1u);
    mpz_t scratch[3], norm;
    for (unsigned k = 0; k < 3; ++k) mpz_init(scratch[k]);
    mpz_init(norm);
    for (unsigned n = 0; n < degree; ++n) {
        for (unsigned k = n + 2; k-- > 0;) {
            if (k <= n) {
                mpz_set_ui(factor.real, k); mpz_mul_2exp(factor.real, factor.real, a_bits);
                mpz_add(factor.real, factor.real, a.real); mpz_set(factor.imaginary, a.imaginary);
                lerch_gaussian_multiply(&left, coefficients + k, &factor, scratch);
            } else { mpz_set_ui(left.real, 0u); mpz_set_ui(left.imaginary, 0u); }
            if (k) {
                mpz_set_ui(factor.real, n + 2 - k); mpz_mul_2exp(factor.real, factor.real, a_bits);
                mpz_sub(factor.real, factor.real, a.real); mpz_neg(factor.imaginary, a.imaginary);
                lerch_gaussian_multiply(&right, coefficients + k - 1, &factor, scratch);
                mpz_add(left.real, left.real, right.real);
                mpz_add(left.imaginary, left.imaginary, right.imaginary);
            }
            mpz_set(coefficients[k].real, left.real); mpz_set(coefficients[k].imaginary, left.imaginary);
        }
    }
    mpz_set(value.real, coefficients[degree].real); mpz_set(value.imaginary, coefficients[degree].imaginary);
    for (unsigned k = degree; k > 0; --k) {
        lerch_gaussian_multiply(&value, &value, &z, scratch);
        mp_bitcnt_t shift = (mp_bitcnt_t)z_bits * (degree - k + 1u);
        mpz_mul_2exp(left.real, coefficients[k - 1].real, shift);
        mpz_mul_2exp(left.imaginary, coefficients[k - 1].imaginary, shift);
        mpz_add(value.real, value.real, left.real); mpz_add(value.imaginary, value.imaginary, left.imaginary);
    }
    mpz_set_ui(factor.real, 1u); mpz_mul_2exp(factor.real, factor.real, z_bits);
    mpz_sub(factor.real, factor.real, z.real); mpz_neg(factor.imaginary, z.imaginary);
    mpz_set_ui(denominator.real, 1u);
    for (unsigned k = 0; k <= degree; ++k)
        lerch_gaussian_multiply(&denominator, &denominator, &factor, scratch);
    mpz_mul(norm, denominator.real, denominator.real);
    mpz_addmul(norm, denominator.imaginary, denominator.imaginary);
    mpz_mul_2exp(norm, norm, (mp_bitcnt_t)a_bits * degree);
    mpz_mul(left.real, value.real, denominator.real); mpz_addmul(left.real, value.imaginary, denominator.imaginary);
    mpz_mul(left.imaginary, value.imaginary, denominator.real);
    mpz_submul(left.imaginary, value.real, denominator.imaginary);
    mpq_t quotient;
    mpq_init(quotient);
    mpq_set_den(quotient, norm);
    mpz_mul_2exp(scratch[0], left.real, z_bits); mpq_set_num(quotient, scratch[0]); mpq_canonicalize(quotient);
    mpfr_set_q(mpc_realref(out), quotient, MPFR_RNDN);
    mpq_set_den(quotient, norm);
    mpz_mul_2exp(scratch[0], left.imaginary, z_bits); mpq_set_num(quotient, scratch[0]); mpq_canonicalize(quotient);
    mpfr_set_q(mpc_imagref(out), quotient, MPFR_RNDN);
    mpq_clear(quotient);
    mpz_clear(norm);
    for (unsigned k = 0; k < 3; ++k) mpz_clear(scratch[k]);
    for (unsigned k = 0; k <= degree; ++k) lerch_gaussian_clear(coefficients + k);
    lerch_gaussian_clear(&denominator); lerch_gaussian_clear(&value); lerch_gaussian_clear(&right);
    lerch_gaussian_clear(&left); lerch_gaussian_clear(&factor); lerch_gaussian_clear(&a); lerch_gaussian_clear(&z);
    return true;
}

/* Replace an infinite defining sum by a bounded rational polynomial when its arithmetic error is small enough. */
bool number_lerch_integer(mpc_ptr out, const number_t argument, const number_t offset,
                          unsigned degree, mpfr_prec_t target)
{
    if (degree > 64 || target < MPFR_PREC_MIN || target > 65536 - 16 * (mpfr_prec_t)degree - 192 ||
        !num_is_finite(argument) || !num_is_finite(offset)) return false;
    mpfr_prec_t work = target + 16 * (mpfr_prec_t)degree + 192;
    mpc_t z, a, factor, left, right, value, denominator, coefficients[65];
    mpc_init2(z, work); mpc_init2(a, work); mpc_init2(factor, work); mpc_init2(left, work);
    mpc_init2(right, work); mpc_init2(value, work); mpc_init2(denominator, work);
    bool coefficients_ready = false;
    mpfr_t radius, bound, scale, scratch[4];
    mpfr_inits2(64, radius, bound, scale, (mpfr_ptr)0);
    for (unsigned k = 0; k < 4; ++k) mpfr_init2(scratch[k], work);
    number_t *z_box = number_coerce(&argument, NUMBER_COMPLEX), *a_box = number_coerce(&offset, NUMBER_COMPLEX);
    bool ok = z_box && a_box &&
        !number_complex_get_mpc(z, number_impl_const(z_box)->value.cx, work) &&
        !number_complex_get_mpc(a, number_impl_const(a_box)->value.cx, work);
    number_box_free(a_box); number_box_free(z_box);
    mpc_abs(radius, z, MPFR_RNDU); mpc_abs(bound, a, MPFR_RNDU);
    ok = ok && mpfr_number_p(radius) && mpfr_cmp_d(radius, 0.9375) <= 0 &&
        mpfr_number_p(bound) && mpfr_cmp_ui(bound, 16u) <= 0;
    if (!ok) goto done;
    if (lerch_integer_dyadic(value, z, a, degree)) goto check;
    for (unsigned k = 0; k <= degree; ++k) {
        mpc_init2(coefficients[k], work); mpc_set_ui(coefficients[k], k ? 0u : 1u, MPC_RNDNN);
    }
    coefficients_ready = true;

    /* p_(n+1,k) = (a+k)*p_(n,k) + (n+2-a-k)*p_(n,k-1), starting with P_0=1.
     * Descending indices retain both old coefficients until they have been consumed. */
    for (unsigned n = 0; n < degree; ++n) {
        for (unsigned k = n + 2; k-- > 0;) {
            if (k <= n) {
                mpc_add_ui(factor, a, k, MPC_RNDNN);
                number_mpc_mul_guarded(left, coefficients[k], factor, scratch);
            } else mpc_set_ui(left, 0u, MPC_RNDNN);
            if (k) {
                mpc_ui_sub(factor, n + 2 - k, a, MPC_RNDNN);
                number_mpc_mul_guarded(right, coefficients[k - 1], factor, scratch);
                mpc_add(left, left, right, MPC_RNDNN);
            }
            mpc_set(coefficients[k], left, MPC_RNDNN);
        }
    }
    mpc_set(value, coefficients[degree], MPC_RNDNN);
    for (unsigned k = degree; k > 0; --k) {
        number_mpc_mul_guarded(value, value, z, scratch);
        mpc_add(value, value, coefficients[k - 1], MPC_RNDNN);
    }
    mpc_ui_sub(denominator, 1u, z, MPC_RNDNN);
    mpc_pow_ui(denominator, denominator, degree + 1u, MPC_RNDNN);
    mpc_div(value, value, denominator, MPC_RNDNN);
check:
    /* ||a||_1<23 makes recurrence factors <128 and coefficients <2^(8m). With ||z||_1<2,
     * Horner's absolute condition bound is <2^(9m+8), and |1-z|^(-m-1)<=2^(4m+4).
     * Conversion, denominator sensitivity and at most O(m^2) rounded operations fit the remaining
     * 3m+52 bits for m<=64. The absolute error is therefore below 2^(16m+64-work).
     * A relative check rejects severe cancellation; no cache agreement is used as evidence of accuracy. */
    mpfr_set_ui_2exp(bound, 1u, 16 * (mpfr_exp_t)degree + 64 - work, MPFR_RNDU);
    number_mpc_abs_lower(scale, value); mpfr_mul_2si(scale, scale, -target - 8, MPFR_RNDD);
    ok = mpfr_number_p(mpc_realref(value)) && mpfr_number_p(mpc_imagref(value)) &&
        mpfr_number_p(scale) && mpfr_lessequal_p(bound, scale);
    if (ok) mpc_set(out, value, MPC_RNDNN);
done:
    for (unsigned k = 0; k < 4; ++k) mpfr_clear(scratch[k]);
    mpfr_clears(radius, bound, scale, (mpfr_ptr)0);
    if (coefficients_ready) for (unsigned k = 0; k <= degree; ++k) mpc_clear(coefficients[k]);
    mpc_clear(denominator); mpc_clear(value); mpc_clear(right); mpc_clear(left);
    mpc_clear(factor); mpc_clear(a); mpc_clear(z);
    return ok;
}

/* Sum real defining terms, accepting a result only after bounding truncation and accumulated roundoff. */
bool number_lerch_real(mpfr_ptr out, mpc_srcptr argument, mpc_srcptr order, mpc_srcptr offset, mpfr_prec_t target)
{
    if (mpfr_sgn(mpc_realref(offset)) <= 0) return false;
    mpfr_prec_t work = target + 128;
    enum { LERCH_REAL_BLOCK = 16 };
    unsigned width = target >= 512 ? LERCH_REAL_BLOCK : 1u;
    mpfr_t coefficients[LERCH_REAL_BLOCK], sum, power, step, term, base;
    for (unsigned j = 0; j < width; ++j) mpfr_init2(coefficients[j], work);
    mpfr_inits2(work, sum, power, step, term, base, (mpfr_ptr)0);
    mpfr_set_zero(sum, 1); mpfr_set_ui(power, 1u, MPFR_RNDN);
    mpfr_pow_ui(step, mpc_realref(argument), width, MPFR_RNDN);
    mpc_t cached;
    mpc_init2(cached, work);
    mpfr_t radius, magnitude, absolute_sum, error, temporary, last, ratio, tail, tolerance;
    mpfr_inits2(64, radius, magnitude, absolute_sum, error, temporary, last, ratio, tail, tolerance, (mpfr_ptr)0);
    mpfr_abs(radius, mpc_realref(argument), MPFR_RNDU);
    mpfr_set_zero(absolute_sum, 1); mpfr_set_zero(error, 1);
    bool integral = mpfr_integer_p(mpc_realref(order)) && mpfr_fits_slong_p(mpc_realref(order), MPFR_RNDN);
    long exponent = integral ? mpfr_get_si(mpc_realref(order), MPFR_RNDN) : 0;
    bool complete = false;
    for (unsigned long k = 0; mpfr_cmp_ui(radius, 1u) < 0 && k < 200000; k += width) {
        mpfr_abs(magnitude, power, MPFR_RNDU);
        for (unsigned j = 0; j < width; ++j) {
            mpfr_exp_t size = mpfr_zero_p(magnitude) ? 0 : mpfr_get_exp(magnitude);
            mpfr_prec_t precision = size < 0 ? (size < 128 - work ? 128 : work + size) : work;
            if (mpfr_sgn(mpc_realref(order)) < 0) {
                /* base < 2^E implies base^(-s) <= 2^(-s*E) for negative real s. Account for that
                 * growth before shortening coefficients; powers of z alone can underestimate the work. */
                mpfr_add_ui(base, mpc_realref(offset), k + j, MPFR_RNDN);
                if (!mpfr_number_p(base)) goto done;
                mpfr_set_si(temporary, mpfr_get_exp(base), MPFR_RNDN);
                mpfr_mul(temporary, temporary, mpc_realref(order), MPFR_RNDD);
                mpfr_neg(temporary, temporary, MPFR_RNDU); mpfr_add_si(temporary, temporary, size, MPFR_RNDU);
                precision = mpfr_sgn(temporary) >= 0 ? work :
                    (mpfr_cmp_si(temporary, 128 - work) < 0 ? 128 : work + mpfr_get_si(temporary, MPFR_RNDU));
            }
            if (mpfr_get_prec(coefficients[j]) != precision) mpfr_set_prec(coefficients[j], precision);
            if (integral) {
                mpfr_add_ui(base, mpc_realref(offset), k + j, MPFR_RNDN);
                if (exponent < 0)
                    mpfr_pow_ui(coefficients[j], base, 0ul - (unsigned long)exponent, MPFR_RNDN);
                else {
                    mpfr_pow_si(base, base, exponent, MPFR_RNDN);
                    mpfr_ui_div(coefficients[j], 1u, base, MPFR_RNDN);
                }
            } else {
                if (mpc_get_prec(cached) != precision) mpc_set_prec(cached, precision);
                number_dirichlet_power(cached, offset, order, k + j);
                mpfr_set(coefficients[j], mpc_realref(cached), MPFR_RNDN);
            }
            mpfr_abs(last, coefficients[j], MPFR_RNDU);
            mpfr_mul(last, last, magnitude, MPFR_RNDU);
            mpfr_add(absolute_sum, absolute_sum, last, MPFR_RNDU);
            mpfr_mul_2si(temporary, last, -precision + 10, MPFR_RNDU);
            mpfr_add(error, error, temporary, MPFR_RNDU);
            if (j + 1 < width) mpfr_mul(magnitude, magnitude, radius, MPFR_RNDU);
        }
        mpfr_set(term, coefficients[width - 1], MPFR_RNDN);
        for (unsigned j = width - 1; j > 0; --j) {
            mpfr_mul(term, term, mpc_realref(argument), MPFR_RNDN);
            mpfr_add(term, term, coefficients[j - 1], MPFR_RNDN);
        }
        mpfr_mul(term, term, power, MPFR_RNDN); mpfr_add(sum, sum, term, MPFR_RNDN);
        if (!mpfr_number_p(sum)) break;
        mpfr_set(ratio, radius, MPFR_RNDU);
        if (mpfr_sgn(mpc_realref(order)) < 0) {
            mpfr_add_ui(temporary, mpc_realref(offset), k + width - 1, MPFR_RNDD);
            mpfr_ui_div(temporary, 1u, temporary, MPFR_RNDU);
            mpfr_add_ui(temporary, temporary, 1u, MPFR_RNDU);
            mpfr_neg(tail, mpc_realref(order), MPFR_RNDU);
            mpfr_pow(temporary, temporary, tail, MPFR_RNDU);
            mpfr_mul(ratio, ratio, temporary, MPFR_RNDU);
        }
        if (mpfr_number_p(ratio) && mpfr_cmp_ui(ratio, 1u) < 0) {
            mpfr_ui_sub(temporary, 1u, ratio, MPFR_RNDD);
            mpfr_div(tail, ratio, temporary, MPFR_RNDU); mpfr_mul(tail, tail, last, MPFR_RNDU);
            mpfr_add(tail, tail, error, MPFR_RNDU);
            mpfr_mul_ui(temporary, absolute_sum, (k + width) * 32, MPFR_RNDU);
            mpfr_mul_2si(temporary, temporary, -work + 8, MPFR_RNDU);
            mpfr_add(tail, tail, temporary, MPFR_RNDU);
            mpfr_abs(tolerance, sum, MPFR_RNDD); mpfr_mul_2si(tolerance, tolerance, -target - 8, MPFR_RNDD);
            if (mpfr_lessequal_p(tail, tolerance)) { complete = true; break; }
        }
        mpfr_mul(power, power, step, MPFR_RNDN);
    }
done:
    if (complete) mpfr_set(out, sum, MPFR_RNDN);
    mpfr_clears(radius, magnitude, absolute_sum, error, temporary, last, ratio, tail, tolerance, (mpfr_ptr)0);
    mpc_clear(cached);
    mpfr_clears(sum, power, step, term, base, (mpfr_ptr)0);
    for (unsigned j = 0; j < width; ++j) mpfr_clear(coefficients[j]);
    return complete;
}

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
    mpz_t real_weights[LERCH_WIDTH], imaginary_weights[LERCH_WIDTH];
    mpz_t real_sum, imaginary_sum, temporary;
    mpc_t order, binomial, factor;
    mpfr_t scratch[4];
    mpc_t *values;
    unsigned long count, capacity;
    mpfr_prec_t precision, weight_precision[2];
};

static void lerch_moments_clear(lerch_moments_t *moments)
{
    for (unsigned long j = 0; j < moments->count; ++j) mpc_clear(moments->values[j]);
    for (unsigned k = 0; k < LERCH_WIDTH; ++k) {
        mpz_clear(moments->real_weights[k]); mpz_clear(moments->imaginary_weights[k]);
    }
    mpz_clears(moments->real_sum, moments->imaginary_sum, moments->temporary, (mpz_ptr)0);
    mpc_clear(moments->factor); mpc_clear(moments->binomial); mpc_clear(moments->order);
    for (unsigned k = 0; k < 4; ++k) mpfr_clear(moments->scratch[k]);
    free(moments->values);
}

static bool lerch_moments_init(lerch_moments_t *moments, mpc_srcptr argument, mpc_srcptr order,
                                mpfr_prec_t precision)
{
    moments->count = 0;
    moments->capacity = (unsigned long)precision + 1024;
    moments->precision = precision;
    moments->values = calloc(moments->capacity, sizeof *moments->values);
    if (!moments->values) return false;
    mpc_init2(moments->order, precision); mpc_init2(moments->binomial, precision);
    mpc_init2(moments->factor, precision);
    mpc_set(moments->order, order, MPC_RNDNN); mpc_set_ui(moments->binomial, 1u, MPC_RNDNN);
    for (unsigned k = 0; k < 4; ++k) mpfr_init2(moments->scratch[k], precision);
    mpz_inits(moments->real_sum, moments->imaginary_sum, moments->temporary, (mpz_ptr)0);
    mpfr_prec_t weight_precision = precision + 96;
    moments->weight_precision[0] = weight_precision; moments->weight_precision[1] = weight_precision;
    mpc_t power;
    mpfr_t scaled;
    mpc_init2(power, weight_precision); mpfr_init2(scaled, weight_precision);
    mpc_set_ui(power, 1u, MPC_RNDNN);
    for (unsigned k = 0; k < LERCH_WIDTH; ++k) {
        mpz_init(moments->real_weights[k]); mpz_init(moments->imaginary_weights[k]);
        mpfr_mul_2ui(scaled, mpc_realref(power), (unsigned long)weight_precision, MPFR_RNDN);
        mpfr_get_z(moments->real_weights[k], scaled, MPFR_RNDN);
        mpfr_mul_2ui(scaled, mpc_imagref(power), (unsigned long)weight_precision, MPFR_RNDN);
        mpfr_get_z(moments->imaginary_weights[k], scaled, MPFR_RNDN);
        mpc_mul(power, power, argument, MPC_RNDNN);
    }
    mpfr_clear(scaled); mpc_clear(power);
    /* At centre B/2, pair z^(B/2+k) with z^(B/2-k). The right slot stores the odd moment's
     * initial k/(B/2) factor; each family then advances by the squared distance. */
    for (unsigned k = 1; k < LERCH_HALF; ++k) {
        mpz_ptr even = moments->real_weights[LERCH_HALF - k], odd = moments->real_weights[LERCH_HALF + k];
        mpz_sub(moments->temporary, odd, even); mpz_add(even, even, odd);
        mpz_mul_ui(odd, moments->temporary, k); mpz_tdiv_q_2exp(odd, odd, LERCH_HALF_BITS);
        even = moments->imaginary_weights[LERCH_HALF - k]; odd = moments->imaginary_weights[LERCH_HALF + k];
        mpz_sub(moments->temporary, odd, even); mpz_add(even, even, odd);
        mpz_mul_ui(odd, moments->temporary, k); mpz_tdiv_q_2exp(odd, odd, LERCH_HALF_BITS);
    }
    return true;
}

/* M_j = sum_(k=0)^(B-1) z^k ((k-B/2)/(B/2))^j. Integer multiplication and exact binary scaling
 * advance paired even/odd weights by squared distances, then multiply by (s)_j/j! once.
 * The unpaired left endpoint contributes (-1)^j, and the centre contributes z^(B/2) only for j=0.
 * Each fixed-point update contracts by k^2/1024<1 and truncates by less than 2^(-precision-96).
 * At most 9408 updates of 31 pairs, including scale reductions, leave the moment error below
 * 2^(-precision-72). At full precision, |(s)_j/j!|*2^-j <= (j+16)^16*2^-j <2^62.
 * Once precision shortens by j-256, coefficient growth <2^224 leaves still more guard bits.
 * Both regions fit inside the existing absolute roundoff allowance. */
static mpc_srcptr lerch_moment(lerch_moments_t *moments, unsigned long index)
{
    if (index >= moments->capacity) return NULL;
    while (moments->count <= index) {
        mpc_ptr value = moments->values[moments->count];
        /* All accepted block radii are at most one half. Reserve the same coefficient-growth allowance
         * as Horner evaluation, so no later block needs more precision than the stored coefficient. */
        long adjustment = 256 - (long)moments->count;
        mpfr_prec_t precision = moments->precision;
        if (adjustment < 0) precision = adjustment < 128 - precision ? 128 : precision + adjustment;
        mpc_init2(value, precision);
        unsigned parity = moments->count & 1u;
        mpfr_prec_t weight_precision = precision + 96;
        mp_bitcnt_t shift = (mp_bitcnt_t)(moments->weight_precision[parity] - weight_precision);
        if (moments->count >= 2) shift += 2 * LERCH_HALF_BITS;
        mpz_set_si(moments->real_sum, parity ? -1 : 1);
        mpz_mul_2exp(moments->real_sum, moments->real_sum, (mp_bitcnt_t)weight_precision);
        mpz_set_ui(moments->imaginary_sum, 0u);
        if (!moments->count) {
            mpz_add(moments->real_sum, moments->real_sum, moments->real_weights[LERCH_HALF]);
            mpz_add(moments->imaginary_sum, moments->imaginary_sum, moments->imaginary_weights[LERCH_HALF]);
        }
        for (unsigned k = 1; k < LERCH_HALF; ++k) {
            unsigned slot = parity ? LERCH_HALF + k : LERCH_HALF - k;
            mpz_ptr real = moments->real_weights[slot], imaginary = moments->imaginary_weights[slot];
            if (!mpz_sgn(real) && !mpz_sgn(imaginary)) continue;
            if (moments->count >= 2) { mpz_mul_ui(real, real, k * k); mpz_mul_ui(imaginary, imaginary, k * k); }
            mpz_tdiv_q_2exp(real, real, shift); mpz_tdiv_q_2exp(imaginary, imaginary, shift);
            mpz_add(moments->real_sum, moments->real_sum, real);
            mpz_add(moments->imaginary_sum, moments->imaginary_sum, imaginary);
        }
        moments->weight_precision[parity] = weight_precision;
        mpfr_set_z(mpc_realref(value), moments->real_sum, MPFR_RNDN);
        mpfr_div_2ui(mpc_realref(value), mpc_realref(value), (unsigned long)weight_precision, MPFR_RNDN);
        mpfr_set_z(mpc_imagref(value), moments->imaginary_sum, MPFR_RNDN);
        mpfr_div_2ui(mpc_imagref(value), mpc_imagref(value), (unsigned long)weight_precision, MPFR_RNDN);
        if (moments->count) {
            mpfr_prec_round(mpc_realref(moments->binomial), precision, MPFR_RNDN);
            mpfr_prec_round(mpc_imagref(moments->binomial), precision, MPFR_RNDN);
            if (mpc_get_prec(moments->factor) != precision) {
                mpc_set_prec(moments->factor, precision);
                for (unsigned k = 0; k < 4; ++k) mpfr_set_prec(moments->scratch[k], precision);
            }
            mpc_add_ui(moments->factor, moments->order, moments->count - 1, MPC_RNDNN);
            number_mpc_mul_guarded(moments->binomial, moments->binomial, moments->factor, moments->scratch);
            mpc_div_ui(moments->binomial, moments->binomial, moments->count, MPC_RNDNN);
        }
        number_mpc_mul_guarded(value, value, moments->binomial, moments->scratch);
        ++moments->count;
    }
    return moments->values[index];
}

/* Binomial-weighted moments depend only on z and s, so each block evaluates the same polynomial
 * at x=-B/(2*centre). Horner accumulation replaces repeated complex coefficient recurrences. */
static bool lerch_block(mpc_ptr out, mpfr_ptr tail, unsigned long *used, lerch_moments_t *moments,
                         mpc_srcptr centre, mpfr_srcptr radius, mpfr_srcptr order_size,
                         mpfr_srcptr geometric, mpfr_prec_t precision)
{
    mpfr_t coefficient, ratio, bound, tolerance, logarithm, exponent;
    mpfr_inits2(64, coefficient, ratio, bound, tolerance, logarithm, exponent, (mpfr_ptr)0);
    mpfr_set_ui(coefficient, 1u, MPFR_RNDN);
    mpfr_set_ui_2exp(tolerance, 1u, -precision + 64, MPFR_RNDD);
    bool complete = false;
    for (unsigned long j = 1; j < moments->capacity; ++j) {
        mpfr_add_ui(ratio, order_size, j - 1, MPFR_RNDU);
        mpfr_div_ui(ratio, ratio, j, MPFR_RNDU); mpfr_mul(ratio, ratio, radius, MPFR_RNDU);
        mpfr_mul(coefficient, coefficient, ratio, MPFR_RNDU);
        *used = j;
        if (j < 32 || j % 8) continue;
        mpfr_div_ui(ratio, order_size, j + 1, MPFR_RNDU);
        mpfr_add_ui(ratio, ratio, 1u, MPFR_RNDU); mpfr_mul(ratio, ratio, radius, MPFR_RNDU);
        mpfr_ui_sub(bound, 1u, ratio, MPFR_RNDD);
        if (mpfr_sgn(bound) <= 0) continue;
        mpfr_div(tail, ratio, bound, MPFR_RNDU); mpfr_mul(tail, tail, coefficient, MPFR_RNDU);
        mpfr_mul(tail, tail, geometric, MPFR_RNDU);
        if (mpfr_lessequal_p(tail, tolerance)) { complete = true; break; }
    }
    if (complete && lerch_moment(moments, *used)) {
        mpc_t argument, temporary, term;
        mpfr_t scratch[4];
        for (unsigned i = 0; i < 4; ++i) mpfr_init2(scratch[i], precision);
        mpc_init2(argument, precision); mpc_init2(temporary, precision); mpc_init2(term, precision);
        mpc_ui_div(argument, LERCH_HALF, centre, MPC_RNDNN);
        mpc_neg(argument, argument, MPC_RNDNN); mpc_set_ui(out, 0u, MPC_RNDNN);
        mpfr_log2(logarithm, radius, MPFR_RNDU);
        for (unsigned long j = *used + 1; j-- > 0;) {
            /* For j<capacity<=9408 and |s|<=16, |(s)_j/j!| <= (j+16)^16 < 2^224.
             * The extra 256 bits cover that growth and geometric moments. Every shortened step's weighted
             * absolute error stays below 2^(-precision-32); full-precision steps retain the ordinary budget. */
            mpfr_mul_ui(exponent, logarithm, j, MPFR_RNDU);
            long adjustment = mpfr_get_si(exponent, MPFR_RNDU) + 256;
            mpfr_prec_t next = precision;
            if (adjustment < 0) next = adjustment < 128 - precision ? 128 : precision + adjustment;
            mpfr_prec_round(mpc_realref(out), next, MPFR_RNDN);
            mpfr_prec_round(mpc_imagref(out), next, MPFR_RNDN);
            if (mpc_get_prec(temporary) != next) { mpc_set_prec(temporary, next); mpc_set_prec(term, next); }
            for (unsigned i = 0; i < 4; ++i)
                if (mpfr_get_prec(scratch[i]) != next) mpfr_set_prec(scratch[i], next);
            mpc_set(temporary, argument, MPC_RNDNN); number_mpc_mul_guarded(out, out, temporary, scratch);
            mpc_set(term, moments->values[j], MPC_RNDNN); mpc_add(out, out, term, MPC_RNDNN);
        }
        mpc_clear(term); mpc_clear(temporary); mpc_clear(argument);
        for (unsigned i = 0; i < 4; ++i) mpfr_clear(scratch[i]);
    } else complete = false;
    mpfr_clears(coefficient, ratio, bound, tolerance, logarithm, exponent, (mpfr_ptr)0);
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
    if (!lerch_moments_init(&moments, argument, order, work)) { eligible = false; goto bounds_done; }
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
    for (unsigned long k = LERCH_WIDTH; finite && k < 200000;) {
        unsigned width = LERCH_WIDTH;
        mpfr_pow_ui(end_weight, radius, width - 1, MPFR_RNDU);
        mpc_add_ui(centre, offset, k + width / 2, MPC_RNDNN);
        mpc_abs(scale, centre, MPFR_RNDD); mpfr_ui_div(block_radius, width / 2, scale, MPFR_RNDU);
        if (mpfr_cmp_d(block_radius, 0.5) > 0) break;
        mpc_abs(magnitude, power, MPFR_RNDU);
        if (!mpfr_number_p(magnitude) || mpfr_zero_p(magnitude)) break;
        /* Bound the whole block before shortening its arithmetic. Powers of z alone miss growth from
         * negative orders. Re(centre)>0 and |arg(centre)|<=|Im(centre)|/Re(centre) bound the anchor's phase. */
        if (mpfr_sgn(mpc_realref(order)) < 0) number_mpc_abs_upper(scale, centre);
        else number_mpc_abs_lower(scale, centre);
        mpfr_neg(temporary, mpc_realref(order), MPFR_RNDU);
        mpfr_pow(scale, scale, temporary, MPFR_RNDU);
        mpfr_abs(condition, mpc_imagref(centre), MPFR_RNDU);
        mpfr_div(condition, condition, mpc_realref(centre), MPFR_RNDU);
        mpfr_abs(temporary, mpc_imagref(order), MPFR_RNDU);
        mpfr_mul(condition, condition, temporary, MPFR_RNDU); mpfr_exp(condition, condition, MPFR_RNDU);
        mpfr_mul(scale, scale, condition, MPFR_RNDU);
        mpfr_ui_sub(condition, 1u, block_radius, MPFR_RNDD);
        mpfr_neg(temporary, order_size, MPFR_RNDN); mpfr_pow(condition, condition, temporary, MPFR_RNDU);
        mpfr_mul(scale, scale, condition, MPFR_RNDU); mpfr_mul(scale, scale, geometric, MPFR_RNDU);
        if (mpfr_cmp_ui(scale, 1u) < 0) mpfr_set_ui(scale, 1u, MPFR_RNDN);
        mpfr_mul(scale, scale, magnitude, MPFR_RNDU);
        if (!mpfr_number_p(scale) || mpfr_zero_p(scale)) break;
        mpfr_exp_t exponent = mpfr_get_exp(scale);
        mpfr_prec_t precision = work;
        if (exponent < 0) precision = exponent < 128 - work ? 128 : work + exponent;
        mpc_set_prec(polynomial, precision); mpc_set_prec(anchor, precision); mpc_set_prec(term, precision);
        number_dirichlet_power(anchor, offset, order, k + width / 2);
        unsigned long used = 0;
        if (!lerch_block(polynomial, local_error, &used, &moments, centre, block_radius,
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
        mpfr_ui_sub(temporary, k + width - 1, offset_size, MPFR_RNDD);
        mpfr_ui_div(temporary, 1u, temporary, MPFR_RNDU);
        mpfr_ui_sub(temporary, 1u, temporary, MPFR_RNDD);
        mpfr_neg(ratio, order_size, MPFR_RNDN); mpfr_pow(ratio, temporary, ratio, MPFR_RNDU);
        mpfr_mul(ratio, ratio, radius, MPFR_RNDU);
        if (mpfr_cmp_ui(ratio, 1u) < 0) {
            mpfr_ui_sub(temporary, 1u, ratio, MPFR_RNDD);
            mpfr_div(ratio, ratio, temporary, MPFR_RNDU);
            mpfr_mul(remainder, remainder, ratio, MPFR_RNDU);
            mpfr_add(remainder, remainder, error, MPFR_RNDU);
            mpfr_mul_ui(local_error, absolute_sum, k + width, MPFR_RNDU);
            mpfr_mul_ui(local_error, local_error, k + width, MPFR_RNDU);
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
        k += width;
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
