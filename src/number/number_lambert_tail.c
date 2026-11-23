/**
 * @file number_lambert_tail.c
 * @brief Geometric-moment acceleration of small-argument Lambert tails.
 *
 * Interchanges Li_-p(w*q^n) with its absolutely convergent power series. The inner finite or infinite
 * geometric sum and its first moment are explicit, so a long decaying tail needs only a short outer series.
 * Directed magnitude bounds control truncation without relying on cancellation. These private MPFR/MPC
 * attempts use call-local scratch storage, preserve their inputs and leave output unchanged on failure.
 */
#include <math.h>
#include <stdint.h>

#include "computation-t.h"
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

static double lambert_moment_bound(double radius, double coordinate, unsigned weight)
{
    double inverse = nextafter(1 / (1 - radius), INFINITY);
    double bound = weight ? nextafter(coordinate * inverse + radius * inverse * inverse, INFINITY) : inverse;
    return log2(bound) + 2;
}

static bool lambert_power_tail(mpfr_srcptr power, double radius, double moment, unsigned degree,
                               unsigned m, size_t bits)
{
    if (mpfr_zero_p(power))
        return true;
    double ratio = nextafter(radius * pow(1 + 1.0 / m, degree), INFINITY);
    if (!(ratio < 1))
        return false;
    double bound = (double)mpfr_get_exp(power) + 2 + degree * log2(m) + moment + log2(ratio / (1 - ratio));
    return bound < -(double)bits - 48;
}

bool number_mpfr_lambert_tail(mpfr_ptr out, mpfr_srcptr q, mpfr_srcptr w, mpfr_srcptr z,
                              unsigned weight, unsigned degree, size_t count, size_t bits)
{
    double radius = nextafter(fabs(mpfr_get_d(q, MPFR_RNDA)), INFINITY);
    double rho = nextafter(fabs(mpfr_get_d(w, MPFR_RNDA)), INFINITY);
    double coordinate = nextafter(fabs(mpfr_get_d(z, MPFR_RNDA)), INFINITY);
    if (weight > 1 || degree > 4 || !(radius < 0.9375) || !(rho < 1) || !isfinite(coordinate))
        return false;
    double moment = lambert_moment_bound(radius, coordinate, weight);
    if (!isfinite(moment))
        return false;
    mpfr_prec_t work = mpfr_get_prec(out);
    mpfr_t base, power, endpoint_step, endpoint, denominator, numerator, scratch, term, sum;
    mpfr_inits2(work, base, power, endpoint_step, endpoint, denominator, numerator, scratch, term, sum, (mpfr_ptr)0);
    mpfr_set(base, q, MPFR_RNDN); mpfr_set(power, w, MPFR_RNDN); mpfr_set_zero(sum, 1);
    bool finite = count != SIZE_MAX, finished = false;
    if (finite) {
        mpfr_pow_ui(endpoint_step, q, count, MPFR_RNDN);
        mpfr_set(endpoint, endpoint_step, MPFR_RNDN);
    }
    computation_t *control = computation_current();
    for (unsigned m = 1; m <= 4096; ++m) {
        if (control && !computation_checkpoint(control, 1))
            break;
        mpfr_ui_sub(denominator, 1, base, MPFR_RNDN);
        if (weight) {
            mpfr_mul(numerator, z, denominator, MPFR_RNDN);
            mpfr_add(numerator, numerator, base, MPFR_RNDN);
            if (finite) {
                mpfr_add_ui(scratch, z, count, MPFR_RNDN);
                mpfr_mul(scratch, scratch, denominator, MPFR_RNDN);
                mpfr_add(scratch, scratch, base, MPFR_RNDN);
                mpfr_mul(scratch, scratch, endpoint, MPFR_RNDN);
                mpfr_sub(numerator, numerator, scratch, MPFR_RNDN);
            }
            mpfr_sqr(denominator, denominator, MPFR_RNDN);
        } else if (finite)
            mpfr_ui_sub(numerator, 1, endpoint, MPFR_RNDN);
        else
            mpfr_set_ui(numerator, 1, MPFR_RNDN);
        mpfr_div(term, numerator, denominator, MPFR_RNDN);
        mpfr_mul(term, term, power, MPFR_RNDN);
        mpfr_set_ui(scratch, m, MPFR_RNDN);
        mpfr_pow_ui(scratch, scratch, degree, MPFR_RNDN);
        mpfr_mul(term, term, scratch, MPFR_RNDN);
        mpfr_add(sum, sum, term, MPFR_RNDN);
        if (!mpfr_number_p(sum))
            break;
        if (lambert_power_tail(power, rho, moment, degree, m, bits)) { finished = true; break; }
        mpfr_mul(base, base, q, MPFR_RNDN); mpfr_mul(power, power, w, MPFR_RNDN);
        if (finite)
            mpfr_mul(endpoint, endpoint, endpoint_step, MPFR_RNDN);
    }
    if (finished)
        mpfr_set(out, sum, MPFR_RNDN);
    mpfr_clears(base, power, endpoint_step, endpoint, denominator, numerator, scratch, term, sum, (mpfr_ptr)0);
    return finished;
}

bool number_mpc_lambert_tail(mpc_ptr out, mpc_srcptr q, mpc_srcptr w, mpc_srcptr z,
                             unsigned weight, unsigned degree, size_t count, size_t bits)
{
    mpfr_t magnitude, factor;
    mpfr_init2(magnitude, 64); mpfr_init2(factor, mpc_get_prec(out));
    mpc_abs(magnitude, q, MPFR_RNDU);
    double radius = nextafter(mpfr_get_d(magnitude, MPFR_RNDU), INFINITY);
    mpc_abs(magnitude, w, MPFR_RNDU);
    double rho = nextafter(mpfr_get_d(magnitude, MPFR_RNDU), INFINITY);
    mpc_abs(magnitude, z, MPFR_RNDU);
    double coordinate = nextafter(mpfr_get_d(magnitude, MPFR_RNDU), INFINITY);
    double moment = lambert_moment_bound(radius, coordinate, weight);
    if (weight > 1 || degree > 4 || !(radius < 0.9375) || !(rho < 1) || !isfinite(moment)) {
        mpfr_clear(magnitude); mpfr_clear(factor);
        return false;
    }
    mpfr_prec_t work = mpc_get_prec(out);
    mpc_t base, power, endpoint_step, endpoint, denominator, numerator, scratch, term, sum;
    mpc_init2(base, work); mpc_init2(power, work); mpc_init2(endpoint_step, work); mpc_init2(endpoint, work);
    mpc_init2(denominator, work); mpc_init2(numerator, work); mpc_init2(scratch, work);
    mpc_init2(term, work); mpc_init2(sum, work);
    mpc_set(base, q, MPC_RNDNN); mpc_set(power, w, MPC_RNDNN); mpc_set_ui(sum, 0, MPC_RNDNN);
    bool finite = count != SIZE_MAX, finished = false;
    if (finite) {
        mpc_pow_ui(endpoint_step, q, count, MPC_RNDNN);
        mpc_set(endpoint, endpoint_step, MPC_RNDNN);
    }
    computation_t *control = computation_current();
    for (unsigned m = 1; m <= 4096; ++m) {
        if (control && !computation_checkpoint(control, 1))
            break;
        mpc_ui_sub(denominator, 1, base, MPC_RNDNN);
        if (weight) {
            mpc_mul(numerator, z, denominator, MPC_RNDNN);
            mpc_add(numerator, numerator, base, MPC_RNDNN);
            if (finite) {
                mpc_add_ui(scratch, z, count, MPC_RNDNN);
                mpc_mul(scratch, scratch, denominator, MPC_RNDNN);
                mpc_add(scratch, scratch, base, MPC_RNDNN);
                mpc_mul(scratch, scratch, endpoint, MPC_RNDNN);
                mpc_sub(numerator, numerator, scratch, MPC_RNDNN);
            }
            mpc_sqr(denominator, denominator, MPC_RNDNN);
        } else if (finite)
            mpc_ui_sub(numerator, 1, endpoint, MPC_RNDNN);
        else
            mpc_set_ui(numerator, 1, MPC_RNDNN);
        mpc_div(term, numerator, denominator, MPC_RNDNN);
        mpc_mul(term, term, power, MPC_RNDNN);
        mpfr_set_ui(factor, m, MPFR_RNDN); mpfr_pow_ui(factor, factor, degree, MPFR_RNDN);
        mpc_mul_fr(term, term, factor, MPC_RNDNN);
        mpc_add(sum, sum, term, MPC_RNDNN);
        if (!mpfr_number_p(mpc_realref(sum)) || !mpfr_number_p(mpc_imagref(sum)))
            break;
        mpc_abs(magnitude, power, MPFR_RNDU);
        if (lambert_power_tail(magnitude, rho, moment, degree, m, bits)) { finished = true; break; }
        mpc_mul(base, base, q, MPC_RNDNN); mpc_mul(power, power, w, MPC_RNDNN);
        if (finite)
            mpc_mul(endpoint, endpoint, endpoint_step, MPC_RNDNN);
    }
    if (finished)
        mpc_set(out, sum, MPC_RNDNN);
    mpfr_clear(magnitude); mpfr_clear(factor);
    mpc_clear(base); mpc_clear(power); mpc_clear(endpoint_step); mpc_clear(endpoint);
    mpc_clear(denominator); mpc_clear(numerator); mpc_clear(scratch); mpc_clear(term); mpc_clear(sum);
    return finished;
}
