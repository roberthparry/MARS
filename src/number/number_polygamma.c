/**
 * @file number_polygamma.c
 * @brief Coefficient precision budgets and finite polygamma recurrence blocks.
 *
 * @par Absolute precision budgets for polygamma asymptotic coefficients.
 *
 * Bounds Bernoulli-weighted powers before requesting their coefficients from the number module's shared cache.
 * The inequality |B_(2n)| < 4*(2n)!/32^n bounds every polygamma order up to sixty-four, including digamma.
 * A caller-owned factorial recurrence and directed scalar bounds shorten coefficients while preserving full-width
 * powers, terms and accumulators. Unsupported budgets retain the original coefficient precision. This helper is
 * private to the number module and retains no state between calls.
 *
 * @par Polynomial blocks for finite digamma and low-order polygamma recurrence sums.
 *
 * Evaluates reciprocal power sums from the logarithmic derivatives of short rising-factorial polynomials.
 * Exact integer coefficients and reusable powers replace a complex division at every recurrence shift.
 * The right-half-plane restriction bounds polynomial cancellation; other inputs retain the ordinary recurrence.
 * All scratch storage belongs to the current call, and no input-dependent values are retained.
 */
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

/* Estimate a sufficient Bernoulli prefix from the same conservative majorant used for coefficient precision. */
void number_polygamma_prepare(mpfr_srcptr inverse_upper, unsigned order, mpfr_prec_t target, mpfr_prec_t work)
{
    if (work < 1024 || work > 8320 || order > 64 || !mpfr_number_p(inverse_upper) ||
        mpfr_sgn(inverse_upper) <= 0 || mpfr_cmp_ui_2exp(inverse_upper, 1u, -5) > 0) return;
    mpfr_t factorial, bound, power, square;
    mpfr_inits2(64, factorial, bound, power, square, (mpfr_ptr)0);
    mpfr_pow_ui(power, inverse_upper, order + 2ul, MPFR_RNDU);
    mpfr_sqr(square, inverse_upper, MPFR_RNDU);
    size_t count = 0;
    for (size_t n = 1; n <= 1024; ++n) {
        number_polygamma_coefficient_precision(factorial, bound, power, n, order, work);
        if (mpfr_number_p(bound) && mpfr_sgn(bound) > 0 &&
            mpfr_get_exp(bound) - 5 * (mpfr_exp_t)n + 2 < -target - 24) { count = n; break; }
        mpfr_mul(power, power, square, MPFR_RNDU);
    }
    if (count) number_coefficient_bernoulli_prepare((count + 31u) / 32u * 32u, work);
    mpfr_clears(factorial, bound, power, square, (mpfr_ptr)0);
}

/* Keep each shortened coefficient's weighted absolute error below 2^(-work-32). */
mpfr_prec_t number_polygamma_coefficient_precision(mpfr_ptr factorial, mpfr_ptr bound, mpfr_srcptr power_upper,
                                                   size_t index, unsigned order, mpfr_prec_t work)
{
    if (work < 640 || work > 65536 || !index || index > 65536 || order > 64 ||
        !mpfr_number_p(power_upper) || mpfr_sgn(power_upper) <= 0) return work;
    if (index == 1) mpfr_fac_ui(factorial, order + 1ul, MPFR_RNDU);
    else {
        mpfr_mul_ui(factorial, factorial, 2ul * index + order - 2ul, MPFR_RNDU);
        mpfr_mul_ui(factorial, factorial, 2ul * index + order - 1ul, MPFR_RNDU);
    }
    mpfr_mul(bound, factorial, power_upper, MPFR_RNDU);
    if (!mpfr_number_p(bound) || mpfr_sgn(bound) <= 0) return work;
    /* For order m, the coefficient bound is 4*(2n+m-1)!/32^n. Forty extra bits cover the
     * Bernoulli conversion and up to 64 rounded integer factors, leaving 32 absolute guard bits. */
    mpfr_exp_t adjustment = mpfr_get_exp(bound) - 5 * (mpfr_exp_t)index + 40;
    if (adjustment >= 0) return work;
    return adjustment < 128 - work ? 128 : work + adjustment;
}

enum { POLYGAMMA_BLOCK = 16, POLYGAMMA_MAX_POWER = 4 };

/* Newton's identities recover sum(1/(base+k)^power) from the normalised polynomial derivatives. */
bool number_mpc_reciprocal_sum(mpc_ptr out, mpc_ptr base, unsigned power, unsigned long limit)
{
    mpfr_prec_t work = mpc_get_prec(out);
    if (work < 640 || work > 32768 || !power || power > POLYGAMMA_MAX_POWER || !limit || limit > 65536 ||
        mpc_get_prec(base) != work || !mpfr_number_p(mpc_realref(base)) ||
        !mpfr_number_p(mpc_imagref(base)) || mpfr_sgn(mpc_realref(base)) <= 0) return false;

    mpfr_t radius, threshold;
    mpfr_inits2(64, radius, threshold, (mpfr_ptr)0);
    mpfr_set_ui(threshold, limit, MPFR_RNDN); mpfr_sqr(threshold, threshold, MPFR_RNDN);
    mpc_norm(radius, base, MPFR_RNDD);
    if (mpfr_greaterequal_p(radius, threshold)) {
        mpc_set_ui(out, 0u, MPC_RNDNN);
        mpfr_clears(radius, threshold, (mpfr_ptr)0);
        return true;
    }
    mpc_t powers[POLYGAMMA_BLOCK + 1], derivatives[POLYGAMMA_MAX_POWER + 1];
    mpc_t sums[POLYGAMMA_MAX_POWER + 1], inverse, term, total;
    mpz_t coefficients[POLYGAMMA_BLOCK + 1], coefficient;
    for (unsigned j = 0; j <= POLYGAMMA_BLOCK; ++j) {
        mpc_init2(powers[j], work);
        mpz_init(coefficients[j]);
    }
    for (unsigned j = 0; j <= POLYGAMMA_MAX_POWER; ++j) {
        mpc_init2(derivatives[j], work);
        mpc_init2(sums[j], work);
    }
    mpc_init2(inverse, work); mpc_init2(term, work); mpc_init2(total, work);
    mpz_init(coefficient);
    mpc_set_ui(total, 0u, MPC_RNDNN);
    mpc_set_ui(powers[0], 1u, MPC_RNDNN);
    mpc_set(powers[1], base, MPC_RNDNN);
    for (unsigned j = 2; j <= POLYGAMMA_BLOCK; ++j)
        mpc_mul(powers[j], powers[j - 1], base, MPC_RNDNN);

    unsigned long offset = 0;
    while (true) {
        mpc_norm(radius, base, MPFR_RNDD);
        if (mpfr_greaterequal_p(radius, threshold)) break;
        mpz_set_ui(coefficients[0], 1u);
        for (unsigned j = 1; j <= POLYGAMMA_BLOCK; ++j) mpz_set_ui(coefficients[j], 0u);
        for (unsigned j = 0; j < POLYGAMMA_BLOCK; ++j) {
            for (unsigned degree = j + 1; degree > 0; --degree) {
                mpz_mul_ui(coefficients[degree], coefficients[degree], offset + j);
                mpz_add(coefficients[degree], coefficients[degree], coefficients[degree - 1]);
            }
            mpz_mul_ui(coefficients[0], coefficients[0], offset + j);
        }
        /* P(t)=prod(base+k+t). The coefficient of t^j is P^(j)(0)/j!.
         * For Re(base)>0, sum |polynomial terms|/|P| <= 2^(B/2). Newton's identities for
         * powers <=4 lose at most another O(4 log2(B)) bits, within the caller's 128 guard bits. */
        for (unsigned j = 0; j <= power; ++j) {
            mpc_set_ui(derivatives[j], 0u, MPC_RNDNN);
            for (unsigned degree = j; degree <= POLYGAMMA_BLOCK; ++degree) {
                unsigned long binomial = 1;
                for (unsigned k = 1; k <= j; ++k) binomial = binomial * (degree - k + 1) / k;
                mpz_mul_ui(coefficient, coefficients[degree], binomial);
                mpfr_mul_z(mpc_realref(term), mpc_realref(powers[degree - j]), coefficient, MPFR_RNDN);
                mpfr_mul_z(mpc_imagref(term), mpc_imagref(powers[degree - j]), coefficient, MPFR_RNDN);
                mpc_add(derivatives[j], derivatives[j], term, MPC_RNDNN);
            }
        }
        mpc_ui_div(inverse, 1u, derivatives[0], MPC_RNDNN);
        for (unsigned j = 1; j <= power; ++j) {
            mpc_mul(derivatives[j], derivatives[j], inverse, MPC_RNDNN);
            mpc_mul_ui(sums[j], derivatives[j], j, MPC_RNDNN);
            if (!(j & 1u)) mpc_neg(sums[j], sums[j], MPC_RNDNN);
            for (unsigned k = 1; k < j; ++k) {
                mpc_mul(term, derivatives[k], sums[j - k], MPC_RNDNN);
                if (k & 1u) mpc_add(sums[j], sums[j], term, MPC_RNDNN);
                else mpc_sub(sums[j], sums[j], term, MPC_RNDNN);
            }
        }
        mpc_add(total, total, sums[power], MPC_RNDNN);
        mpc_add_ui(base, base, POLYGAMMA_BLOCK, MPC_RNDNN);
        offset += POLYGAMMA_BLOCK;
    }
    mpc_set(out, total, MPC_RNDNN);
    mpfr_clears(radius, threshold, (mpfr_ptr)0); mpz_clear(coefficient);
    mpc_clear(total); mpc_clear(term); mpc_clear(inverse);
    for (unsigned j = 0; j <= POLYGAMMA_MAX_POWER; ++j) {
        mpc_clear(sums[j]); mpc_clear(derivatives[j]);
    }
    for (unsigned j = 0; j <= POLYGAMMA_BLOCK; ++j) { mpz_clear(coefficients[j]); mpc_clear(powers[j]); }
    return true;
}

/* Real inputs share the same exact polynomial construction; MPC's zero-imaginary operations stay real. */
bool number_mpfr_reciprocal_sum(mpfr_ptr out, mpfr_ptr base, unsigned power, unsigned long limit)
{
    mpfr_prec_t work = mpfr_get_prec(out);
    if (work < 640 || work > 32768 || !power || power > POLYGAMMA_MAX_POWER || !limit || limit > 65536 ||
        mpfr_get_prec(base) != work || !mpfr_number_p(base) || mpfr_sgn(base) <= 0) return false;
    mpc_t input, result;
    mpc_init2(input, work); mpc_init2(result, work);
    mpc_set_fr(input, base, MPC_RNDNN);
    bool ok = number_mpc_reciprocal_sum(result, input, power, limit);
    if (ok) {
        mpfr_set(out, mpc_realref(result), MPFR_RNDN);
        mpfr_set(base, mpc_realref(input), MPFR_RNDN);
    }
    mpc_clear(result); mpc_clear(input);
    return ok;
}
