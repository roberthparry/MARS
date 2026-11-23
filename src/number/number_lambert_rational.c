/**
 * @file number_lambert_rational.c
 * @brief Reusable working storage for non-positive-order multiprecision Lambert sums.
 *
 * MPFR evaluates real principal powers when possible; MPC handles the remaining branches. Eulerian
 * polynomials replace negative-order polylogarithms. Every working value is allocated once per call,
 * and a geometric/polynomial majorant controls infinite tails independently of cancellation. Finite
 * constant-base moments collapse directly; order minus one near the unit circle uses a blocked reciprocal recurrence.
 * Inputs and
 * the thread's default precision remain unchanged. The general Lambert dispatcher owns domain validation.
 */
#include <math.h>
#include <stdint.h>

#include "computation-t.h"
#include "number-t.h"
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

static bool lambert_set_real(mpfr_ptr out, number_t value)
{
    const number_private_t *impl = number_impl_const(&value);
    if (impl->kind == NUMBER_MPZ)
        mpfr_set_z(out, number_mpz_value(impl->value.mpz), MPFR_RNDN);
    else if (impl->kind == NUMBER_MPQ)
        mpfr_set_q(out, number_mpq_value(impl->value.mpq), MPFR_RNDN);
    else if (impl->kind == NUMBER_MPFR) {
        if (impl->value.mpfr->constant_id != NUMBER_CONST_COUNT) {
            number_mpfr_t *copy = number_mpfr_from_const_id(impl->value.mpfr->constant_id,
                                                            (size_t)mpfr_get_prec(out));
            if (!copy)
                return false;
            mpfr_set(out, number_mpfr_value(copy), MPFR_RNDN);
            number_mpfr_free(copy);
        } else
            mpfr_set(out, number_mpfr_value(impl->value.mpfr), MPFR_RNDN);
    } else if (impl->kind == NUMBER_COMPLEX)
        return lambert_set_real(out, impl->value.cx->real);
    else {
        qfloat_t fixed = num_to_qfloat(value);
        mpfr_set_d(out, fixed.hi, MPFR_RNDN);
        mpfr_add_d(out, out, fixed.lo, MPFR_RNDN);
    }
    return mpfr_number_p(out);
}

static bool lambert_set_complex(mpc_ptr out, number_t value)
{
    NUM_SCOPE(scope);
    return lambert_set_real(mpc_realref(out), num_real_part(value)) &&
           lambert_set_real(mpc_imagref(out), num_imag_part(value));
}

/* Every coefficient is below 32!, so even the smallest guarded precision stores it exactly. */
static void lambert_eulerian(mpfr_t *coefficients, unsigned degree, mpfr_ptr scratch, mpfr_prec_t work)
{
    for (unsigned j = 0; j <= degree; ++j) {
        mpfr_init2(coefficients[j], work);
        mpfr_set_ui(coefficients[j], j == 0, MPFR_RNDN);
    }
    for (unsigned p = 2; p <= degree; ++p) {
        for (unsigned j = p - 1; j > 0; --j) {
            mpfr_mul_ui(scratch, coefficients[j - 1], p - j, MPFR_RNDN);
            mpfr_mul_ui(coefficients[j], coefficients[j], j + 1, MPFR_RNDN);
            mpfr_add(coefficients[j], coefficients[j], scratch, MPFR_RNDN);
        }
    }
}

/* Directed magnitudes and the exponent retain useful bounds even below binary64's exponent range. */
static bool lambert_tail(mpfr_srcptr magnitude, double radius, double z_size, size_t n,
                         unsigned weight, unsigned degree, size_t bits)
{
    if (mpfr_zero_p(magnitude))
        return true;
    double rho = nextafter(mpfr_get_d(magnitude, MPFR_RNDU), INFINITY);
    double distance = nextafter((double)n + z_size, INFINITY);
    if (!(rho < 1) || !(distance > 0))
        return false;
    double ratio = nextafter(radius * pow(1 + 1 / distance, weight), INFINITY);
    if (!(ratio < 1))
        return false;
    double bound = (double)mpfr_get_exp(magnitude) + 2 + weight * log2(distance) -
                   (degree + 1) * log2(1 - rho) + log2(ratio / (1 - ratio));
    for (unsigned j = 2; j <= degree; ++j)
        bound += log2(j);
    return bound + 4 < -(double)bits - 16;
}

static number_t lambert_real(number_t a, number_t q, number_t z, unsigned weight, unsigned degree,
                              size_t count, size_t bits)
{
    mpfr_prec_t work = bits < 32 ? 128 : (mpfr_prec_t)bits + 96;
    mpfr_t amplitude, base, coordinate, w, sum, power, polynomial, denominator, term, scratch, magnitude;
    mpfr_inits2(work, amplitude, base, coordinate, w, sum, power, polynomial, denominator, term, scratch,
                magnitude, (mpfr_ptr)0);
    bool ok = lambert_set_real(amplitude, a) && lambert_set_real(base, q) && lambert_set_real(coordinate, z);
    double radius = nextafter(fabs(mpfr_get_d(base, MPFR_RNDA)), INFINITY);
    double z_size = nextafter(fabs(mpfr_get_d(coordinate, MPFR_RNDA)), INFINITY);
    bool infinite = count == SIZE_MAX, finished = false;
    bool stationary = !infinite && weight <= 1 && mpfr_cmp_ui(base, 1) == 0;
    ok = ok && (!infinite || (radius < 1 && isfinite(z_size)));
    if (mpfr_zero_p(coordinate))
        mpfr_set_ui(w, 1, MPFR_RNDN);
    else if (mpfr_cmp_ui(coordinate, 1) == 0)
        mpfr_set(w, base, MPFR_RNDN);
    else
        mpfr_pow(w, base, coordinate, MPFR_RNDN);
    mpfr_mul(w, w, amplitude, MPFR_RNDN);
    mpfr_set_zero(sum, 1);
    if (stationary) {
        mpfr_set_ui(power, count, MPFR_RNDN);
        if (weight) {
            mpfr_mul(power, power, coordinate, MPFR_RNDN);
            mpfr_set_ui(scratch, count, MPFR_RNDN);
            mpfr_mul_ui(scratch, scratch, count - 1, MPFR_RNDN);
            mpfr_div_2ui(scratch, scratch, 1, MPFR_RNDN);
            mpfr_add(power, power, scratch, MPFR_RNDN);
        }
    }
    mpfr_t coefficients[33];
    lambert_eulerian(coefficients, degree, scratch, work);
    computation_t *control = computation_current();
    size_t limit = infinite ? 100000 : count;
    mpfr_exp_t tail_exponent = -(mpfr_exp_t)ceil(sqrt((double)bits));
    for (size_t n = 0; ok && n < limit; ++n) {
        if (control && !computation_checkpoint(control, 1)) { ok = false; break; }
        mpfr_set(polynomial, coefficients[degree ? degree - 1 : 0], MPFR_RNDN);
        for (unsigned j = degree ? degree - 1 : 0; j > 0; --j) {
            mpfr_mul(polynomial, polynomial, w, MPFR_RNDN);
            mpfr_add(polynomial, polynomial, coefficients[j - 1], MPFR_RNDN);
        }
        mpfr_mul(term, w, polynomial, MPFR_RNDN);
        mpfr_ui_sub(denominator, 1, w, MPFR_RNDN);
        mpfr_pow_ui(denominator, denominator, degree + 1, MPFR_RNDN);
        mpfr_div(term, term, denominator, MPFR_RNDN);
        if (stationary)
            mpfr_mul(term, term, power, MPFR_RNDN);
        else if (weight == 1)
            mpfr_mul(term, term, coordinate, MPFR_RNDN);
        else if (weight) {
            mpfr_pow_ui(power, coordinate, weight, MPFR_RNDN);
            mpfr_mul(term, term, power, MPFR_RNDN);
        }
        mpfr_add(sum, sum, term, MPFR_RNDN);
        ok = mpfr_number_p(sum);
        if (stationary || (!infinite && n + 1 == count)) { finished = true; break; }
        if (radius < 1 && (n & 15u) == 15u) {
            mpfr_abs(magnitude, w, MPFR_RNDU);
            if (lambert_tail(magnitude, radius, z_size, n, weight, degree, bits)) { finished = true; break; }
        }
        mpfr_mul(w, w, base, MPFR_RNDN);
        mpfr_add_ui(coordinate, coordinate, 1, MPFR_RNDN);
        if (weight <= 1 && degree <= 4 && radius < 0.9375 && (n & 15u) == 15u &&
            (infinite || count - n - 1 > 32) && !mpfr_zero_p(w) && mpfr_get_exp(w) < tail_exponent &&
            number_mpfr_lambert_tail(term, base, w, coordinate, weight, degree,
                                    infinite ? SIZE_MAX : count - n - 1, bits)) {
            mpfr_add(sum, sum, term, MPFR_RNDN);
            ok = mpfr_number_p(sum);
            finished = true;
            break;
        }
    }
    number_t result = ok && finished ? number_take_mpfr(number_mpfr_from_mpfr(sum, bits)) : num_clone(NUM_NAN);
    for (unsigned j = 0; j <= degree; ++j)
        mpfr_clear(coefficients[j]);
    mpfr_clears(amplitude, base, coordinate, w, sum, power, polynomial, denominator, term, scratch, magnitude,
                (mpfr_ptr)0);
    return result;
}

/* The stable form avoids cancellation in w + 1/w - 2 when w is close to one. */
static void lambert_mpc_difference(mpc_ptr out, mpc_srcptr w)
{
    mpc_sub_ui(out, w, 1, MPC_RNDNN);
    mpc_sqr(out, out, MPC_RNDNN);
    mpc_div(out, out, w, MPC_RNDNN);
}

static number_t lambert_complex(number_t a, number_t q, number_t z, unsigned weight, unsigned degree,
                                 size_t count, size_t bits)
{
    mpfr_prec_t work = bits < 32 ? 128 : (mpfr_prec_t)bits + 96;
    mpc_t amplitude, base, coordinate, w, sum, power, polynomial, denominator, term;
    mpc_init2(amplitude, work); mpc_init2(base, work); mpc_init2(coordinate, work); mpc_init2(w, work);
    mpc_init2(sum, work); mpc_init2(power, work); mpc_init2(polynomial, work);
    mpc_init2(denominator, work); mpc_init2(term, work);
    mpfr_t magnitude, scratch, coefficients[33];
    mpfr_init2(magnitude, 64); mpfr_init2(scratch, work);
    bool ok = lambert_set_complex(amplitude, a) && lambert_set_complex(base, q) && lambert_set_complex(coordinate, z);
    mpc_abs(magnitude, base, MPFR_RNDU);
    double radius = nextafter(mpfr_get_d(magnitude, MPFR_RNDU), INFINITY);
    mpc_abs(magnitude, coordinate, MPFR_RNDU);
    double z_size = nextafter(mpfr_get_d(magnitude, MPFR_RNDU), INFINITY);
    bool infinite = count == SIZE_MAX, finished = false;
    bool stationary = !infinite && weight <= 1 && mpc_cmp_si(base, 1) == 0;
    bool recurrence = !stationary && !infinite && degree == 1 && count >= 64 && radius >= 0.999 && radius <= 1.001;
    ok = ok && (!infinite || (radius < 1 && isfinite(z_size)));
    if (mpfr_zero_p(mpc_imagref(coordinate)) && mpfr_zero_p(mpc_realref(coordinate)))
        mpc_set_ui(w, 1, MPC_RNDNN);
    else if (mpfr_zero_p(mpc_imagref(coordinate)) && mpfr_cmp_ui(mpc_realref(coordinate), 1) == 0)
        mpc_set(w, base, MPC_RNDNN);
    else
        mpc_pow(w, base, coordinate, MPC_RNDNN);
    mpc_mul(w, w, amplitude, MPC_RNDNN);
    mpc_set_ui(sum, 0, MPC_RNDNN);
    lambert_eulerian(coefficients, degree, scratch, work);
    if (stationary) {
        mpc_set_ui(power, count, MPC_RNDNN);
        if (weight) {
            mpc_mul(power, power, coordinate, MPC_RNDNN);
            mpfr_set_ui(scratch, count, MPFR_RNDN);
            mpfr_mul_ui(scratch, scratch, count - 1, MPFR_RNDN);
            mpfr_div_2ui(scratch, scratch, 1, MPFR_RNDN);
            mpc_add_fr(power, power, scratch, MPC_RNDNN);
        }
    }
    mpc_t delta, next, step;
    bool recurrence_storage = recurrence;
    if (recurrence) {
        mpc_init2(delta, work); mpc_init2(next, work); mpc_init2(step, work);
        lambert_mpc_difference(delta, base);
        mpc_pow_ui(step, base, 32, MPC_RNDNN);
    }
    computation_t *control = computation_current();
    size_t limit = infinite ? 100000 : count;
    mpfr_exp_t tail_exponent = -(mpfr_exp_t)ceil(sqrt((double)bits));
    for (size_t n = 0; ok && n < limit; ++n) {
        if (control && !computation_checkpoint(control, 1)) { ok = false; break; }
        if (recurrence) {
            /* Li_-1(w_n)=1/d_n, d_n=w_n+1/w_n-2. Re-anchor every 32 terms to bound rounding drift. */
            if (!(n & 31u)) {
                lambert_mpc_difference(denominator, w);
                mpc_mul(next, w, base, MPC_RNDNN);
                lambert_mpc_difference(polynomial, next);
            }
            /* Near a pole, cancellation can exceed the guard budget: resume the direct, stable quotient. */
            if (!mpfr_number_p(mpc_realref(denominator)) || !mpfr_number_p(mpc_imagref(denominator)) ||
                ((mpfr_zero_p(mpc_realref(denominator)) || mpfr_get_exp(mpc_realref(denominator)) < -32) &&
                 (mpfr_zero_p(mpc_imagref(denominator)) || mpfr_get_exp(mpc_imagref(denominator)) < -32))) {
                if (n & 31u) {
                    mpc_pow_ui(next, base, n & 31u, MPC_RNDNN);
                    mpc_mul(w, w, next, MPC_RNDNN);
                }
                recurrence = false;
            }
        }
        if (recurrence) {
            mpc_ui_div(term, 1, denominator, MPC_RNDNN);
        } else {
            mpc_set_fr(polynomial, coefficients[degree ? degree - 1 : 0], MPC_RNDNN);
            for (unsigned j = degree ? degree - 1 : 0; j > 0; --j) {
                mpc_mul(polynomial, polynomial, w, MPC_RNDNN);
                mpc_add_fr(polynomial, polynomial, coefficients[j - 1], MPC_RNDNN);
            }
            mpc_mul(term, w, polynomial, MPC_RNDNN);
            mpc_ui_sub(denominator, 1, w, MPC_RNDNN);
            mpc_pow_ui(denominator, denominator, degree + 1, MPC_RNDNN);
            mpc_div(term, term, denominator, MPC_RNDNN);
        }
        if (stationary)
            mpc_mul(term, term, power, MPC_RNDNN);
        else if (weight && mpfr_zero_p(mpc_imagref(coordinate))) {
            if (weight == 1)
                mpc_mul_fr(term, term, mpc_realref(coordinate), MPC_RNDNN);
            else {
                mpfr_pow_ui(scratch, mpc_realref(coordinate), weight, MPFR_RNDN);
                mpc_mul_fr(term, term, scratch, MPC_RNDNN);
            }
        } else if (weight) {
            mpc_pow_ui(power, coordinate, weight, MPC_RNDNN);
            mpc_mul(term, term, power, MPC_RNDNN);
        }
        mpc_add(sum, sum, term, MPC_RNDNN);
        ok = mpfr_number_p(mpc_realref(sum)) && mpfr_number_p(mpc_imagref(sum));
        if (stationary || (!infinite && n + 1 == count)) { finished = true; break; }
        if (!recurrence && radius < 1 && (n & 15u) == 15u) {
            mpc_abs(magnitude, w, MPFR_RNDU);
            if (lambert_tail(magnitude, radius, z_size, n, weight, degree, bits)) { finished = true; break; }
        }
        if (recurrence) {
            if ((n & 31u) == 31u)
                mpc_mul(w, w, step, MPC_RNDNN);
            else {
                /* d_(n+2)=2*d_(n+1)-d_n+(q+1/q-2)*(d_(n+1)+2). */
                mpc_add_ui(next, polynomial, 2, MPC_RNDNN);
                mpc_mul(next, next, delta, MPC_RNDNN);
                mpc_mul_2ui(amplitude, polynomial, 1, MPC_RNDNN);
                mpc_add(next, next, amplitude, MPC_RNDNN);
                mpc_sub(next, next, denominator, MPC_RNDNN);
                mpc_set(denominator, polynomial, MPC_RNDNN);
                mpc_swap(polynomial, next);
            }
        } else
            mpc_mul(w, w, base, MPC_RNDNN);
        mpc_add_ui(coordinate, coordinate, 1, MPC_RNDNN);
        if (!recurrence && weight <= 1 && degree <= 4 && radius < 0.9375 && (n & 15u) == 15u &&
            (infinite || count - n - 1 > 32)) {
            mpc_abs(magnitude, w, MPFR_RNDU);
            if (!mpfr_zero_p(magnitude) && mpfr_get_exp(magnitude) < tail_exponent &&
                number_mpc_lambert_tail(term, base, w, coordinate, weight, degree,
                                       infinite ? SIZE_MAX : count - n - 1, bits)) {
                mpc_add(sum, sum, term, MPC_RNDNN);
                ok = mpfr_number_p(mpc_realref(sum)) && mpfr_number_p(mpc_imagref(sum));
                finished = true;
                break;
            }
        }
    }
    number_t result = ok && finished ? number_take(number_wrap_complex_mpc(sum, bits)) : num_clone(NUM_NAN);
    for (unsigned j = 0; j <= degree; ++j)
        mpfr_clear(coefficients[j]);
    mpfr_clear(magnitude); mpfr_clear(scratch);
    if (recurrence_storage) { mpc_clear(delta); mpc_clear(next); mpc_clear(step); }
    mpc_clear(amplitude); mpc_clear(base); mpc_clear(coordinate); mpc_clear(w); mpc_clear(sum);
    mpc_clear(power); mpc_clear(polynomial); mpc_clear(denominator); mpc_clear(term);
    return result;
}

/* Internal dispatch preserves the principal-power branch and the caller's selected precision. */
number_t number_lambert_rational(number_t a, number_t q, number_t z, unsigned weight, unsigned degree,
                                 size_t count, size_t bits)
{
    bool real = num_is_real(a) && num_is_real(q) && num_is_real(z) && (num_gt(q, NUM_ZERO) || num_is_integer(z));
    return real ? lambert_real(a, q, z, weight, degree, count, bits) :
                  lambert_complex(a, q, z, weight, degree, count, bits);
}
