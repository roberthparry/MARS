/**
 * @file number_special_series.c
 * @brief Precision-preserving defining series for pFq and the Lerch transcendent.
 *
 * Uses the widest participating number backend and guarded MPC arithmetic for real and complex series.
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
    for (unsigned long n = 0; ok && n < 200000; ++n) {
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
    mpc_pow_ui(step, argument, block, MPC_RNDNN);
    if (ok && !integer) converged = number_lerch_blocks(sum, argument, exponent, offset, (mpfr_prec_t)bits);
    for (unsigned attempt = 0; ok && !converged && attempt < 2; ++attempt) {
        bool adaptive = !integer && attempt == 0, retry = false;
        mpc_set_prec(base, work);
        mpc_set_ui(power, 1u, MPC_RNDNN); mpc_set_ui(sum, 0u, MPC_RNDNN);
        mpfr_set_zero(absolute_sum, 1); mpfr_set_zero(power_error, 1);
        for (unsigned long k = 0; ok && k < 200000; k += block) {
            if (integer && block == 1) {
                /* Retain the direct quotient for short integer-order calculations. */
                mpc_add_ui(base, offset, k, MPC_RNDNN);
                mpc_pow_si(base, base, integer_order, MPC_RNDNN);
                mpc_div(term, power, base, MPC_RNDNN);
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
                        mpc_pow_si(base, base, integer_order, MPC_RNDNN);
                        mpc_ui_div(coefficients[j], 1u, base, MPC_RNDNN);
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
