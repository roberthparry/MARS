#include <limits.h>

#include "number.h"
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

/* Spend extra bits only on range reduction; large angles must not lose their fractional turn. */
static void number_clausen_reduce(mpfr_ptr out, mpfr_srcptr input)
{
    mpfr_prec_t precision = mpfr_get_prec(out);
    mpfr_exp_t exponent = mpfr_zero_p(input) ? 0 : mpfr_get_exp(input);
    mpfr_t period, reduced;

    if (exponent > 0 && exponent < MPFR_PREC_MAX - precision)
        precision += exponent;
    mpfr_inits2(precision, period, reduced, (mpfr_ptr)0);
    mpfr_const_pi(period, MPFR_RNDN);
    mpfr_mul_2ui(period, period, 1u, MPFR_RNDN);
    mpfr_remainder(reduced, input, period, MPFR_RNDN);
    mpfr_set(out, reduced, MPFR_RNDN);
    mpfr_clears(period, reduced, (mpfr_ptr)0);
}

static bool number_clausen_small(mpfr_srcptr term, mpfr_srcptr sum, mpfr_prec_t bits, mpfr_ptr scratch)
{
    mpfr_abs(scratch, sum, MPFR_RNDN);
    mpfr_div_2ui(scratch, scratch, (unsigned long)bits, MPFR_RNDN);
    return mpfr_cmpabs(term, scratch) <= 0;
}

/* The real path never constructs a complex value. */
static void number_mpfr_clausen(mpfr_ptr out, unsigned long order, mpfr_srcptr input)
{
    mpfr_prec_t bits = mpfr_get_prec(out) + 32;
    mpfr_prec_t precision = bits + 32;
    mpfr_t x, pi, square, term, sum, harmonic, ratio, coefficient, add, scratch, quarter;
    bool negative;
    unsigned long k;
    bool converged = false;

    if (order == 0ul || !mpfr_number_p(input)) {
        mpfr_set_nan(out);
        return;
    }
    mpfr_inits2(precision, x, pi, square, term, sum, harmonic, ratio, coefficient, add, scratch, quarter,
                (mpfr_ptr)0);
    number_clausen_reduce(x, input);
    negative = (order & 1ul) == 0ul && mpfr_sgn(x) < 0;
    mpfr_abs(x, x, MPFR_RNDN);
    mpfr_const_pi(pi, MPFR_RNDN);
    if (mpfr_zero_p(x)) {
        if (order == 1ul)
            mpfr_set_inf(sum, 1);
        else if (order & 1ul)
            mpfr_zeta_ui(sum, order, MPFR_RNDN);
        else
            mpfr_set_zero(sum, 0);
        goto done;
    }
    if (order == 1ul) {
        mpfr_div_2ui(term, x, 1u, MPFR_RNDN);
        mpfr_sin(term, term, MPFR_RNDN);
        mpfr_mul_2ui(term, term, 1u, MPFR_RNDN);
        mpfr_log(sum, term, MPFR_RNDN);
        mpfr_neg(sum, sum, MPFR_RNDN);
        goto done;
    }
    if (order > (unsigned long)bits + 32ul) {
        if (order & 1ul)
            mpfr_cos(sum, x, MPFR_RNDN);
        else
            mpfr_sin(sum, x, MPFR_RNDN);
        goto done;
    }
    mpfr_div_2ui(scratch, pi, 1u, MPFR_RNDN);
    if (order == 2ul && mpfr_cmp(x, scratch) > 0) {
        mpfr_sub(x, pi, x, MPFR_RNDN);
        mpfr_div(ratio, x, pi, MPFR_RNDN);
        mpfr_sqr(ratio, ratio, MPFR_RNDN);
        mpfr_set(term, ratio, MPFR_RNDN);
        mpfr_set_ui_2exp(quarter, 1ul, -2, MPFR_RNDN);
        mpfr_const_log2(sum, MPFR_RNDN);
        for (unsigned long j = 1ul; j <= (unsigned long)precision + 64ul; ++j) {
            mpfr_zeta_ui(coefficient, 2ul * j, MPFR_RNDN);
            mpfr_ui_sub(scratch, 1ul, quarter, MPFR_RNDN);
            mpfr_mul(add, coefficient, scratch, MPFR_RNDN);
            mpfr_mul(add, add, term, MPFR_RNDN);
            mpfr_div_ui(add, add, j, MPFR_RNDN);
            mpfr_div_ui(add, add, 2ul * j + 1ul, MPFR_RNDN);
            mpfr_sub(sum, sum, add, MPFR_RNDN);
            if (number_clausen_small(add, sum, bits, scratch)) {
                converged = true;
                break;
            }
            mpfr_mul(term, term, ratio, MPFR_RNDN);
            mpfr_div_2ui(quarter, quarter, 2u, MPFR_RNDN);
        }
        mpfr_mul(sum, sum, x, MPFR_RNDN);
        if (!converged)
            mpfr_set_nan(sum);
        goto done;
    }

    mpfr_sqr(square, x, MPFR_RNDN);
    k = (order - 1ul) & 1ul;
    if (k)
        mpfr_set(term, x, MPFR_RNDN);
    else
        mpfr_set_ui(term, 1ul, MPFR_RNDN);
    mpfr_set_zero(sum, 0);
    for (; k < order - 1ul; k += 2ul) {
        mpfr_zeta_ui(coefficient, order - k, MPFR_RNDN);
        mpfr_mul(add, term, coefficient, MPFR_RNDN);
        mpfr_add(sum, sum, add, MPFR_RNDN);
        mpfr_mul(term, term, square, MPFR_RNDN);
        mpfr_div_ui(term, term, k + 1ul, MPFR_RNDN);
        mpfr_div_ui(term, term, k + 2ul, MPFR_RNDN);
        mpfr_neg(term, term, MPFR_RNDN);
    }
    mpfr_set_zero(harmonic, 0);
    for (k = 1ul; k < order; ++k) {
        mpfr_set_ui(scratch, k, MPFR_RNDN);
        mpfr_ui_div(scratch, 1ul, scratch, MPFR_RNDN);
        mpfr_add(harmonic, harmonic, scratch, MPFR_RNDN);
    }
    mpfr_log(scratch, x, MPFR_RNDN);
    mpfr_sub(harmonic, harmonic, scratch, MPFR_RNDN);
    mpfr_mul(add, term, harmonic, MPFR_RNDN);
    mpfr_add(sum, sum, add, MPFR_RNDN);
    mpfr_mul_2ui(pi, pi, 1u, MPFR_RNDN);
    mpfr_div(ratio, x, pi, MPFR_RNDN);
    mpfr_sqr(ratio, ratio, MPFR_RNDN);
    mpfr_mul(term, term, ratio, MPFR_RNDN);
    mpfr_mul_2ui(term, term, 1u, MPFR_RNDN);
    mpfr_div_ui(term, term, order, MPFR_RNDN);
    mpfr_div_ui(term, term, order + 1ul, MPFR_RNDN);
    for (unsigned long j = 1ul; j <= (unsigned long)precision + 64ul; ++j) {
        mpfr_zeta_ui(coefficient, 2ul * j, MPFR_RNDN);
        mpfr_mul(add, term, coefficient, MPFR_RNDN);
        mpfr_add(sum, sum, add, MPFR_RNDN);
        if (number_clausen_small(add, sum, bits, scratch)) {
            converged = true;
            break;
        }
        mpfr_mul(term, term, ratio, MPFR_RNDN);
        mpfr_mul_ui(term, term, 2ul * j, MPFR_RNDN);
        mpfr_mul_ui(term, term, 2ul * j + 1ul, MPFR_RNDN);
        mpfr_div_ui(term, term, order + 2ul * j, MPFR_RNDN);
        mpfr_div_ui(term, term, order + 2ul * j + 1ul, MPFR_RNDN);
    }
    if (!converged)
        mpfr_set_nan(sum);
done:
    if (negative)
        mpfr_neg(sum, sum, MPFR_RNDN);
    mpfr_set(out, sum, MPFR_RNDN);
    mpfr_clears(x, pi, square, term, sum, harmonic, ratio, coefficient, add, scratch, quarter, (mpfr_ptr)0);
}

static bool number_clausen_complex_small(mpc_srcptr term, mpc_srcptr sum, mpfr_prec_t bits,
                                         mpfr_ptr magnitude, mpfr_ptr scratch)
{
    mpc_abs(magnitude, term, MPFR_RNDN);
    mpc_abs(scratch, sum, MPFR_RNDN);
    return number_clausen_small(magnitude, scratch, bits, scratch);
}

static void number_mpc_clausen(mpc_ptr out, unsigned long order, mpc_srcptr input)
{
    mpfr_prec_t bits = mpc_get_prec(out) + 32;
    mpfr_prec_t precision = bits + 32;
    mpfr_t pi, coefficient, harmonic, magnitude, scratch, quarter;
    mpc_t z, square, ratio, term, sum, add, work, power, polynomial;
    bool reflect, conjugate, converged = false;
    unsigned long k;

    if (order == 0ul || !mpfr_number_p(mpc_realref(input)) || !mpfr_number_p(mpc_imagref(input))) {
        mpfr_set_nan(mpc_realref(out));
        mpfr_set_nan(mpc_imagref(out));
        return;
    }
    if (mpfr_zero_p(mpc_imagref(input))) {
        number_mpfr_clausen(mpc_realref(out), order, mpc_realref(input));
        mpfr_set_zero(mpc_imagref(out), 0);
        return;
    }
    mpfr_inits2(precision, pi, coefficient, harmonic, magnitude, scratch, quarter, (mpfr_ptr)0);
    mpc_init2(z, precision);
    mpc_init2(square, precision);
    mpc_init2(ratio, precision);
    mpc_init2(term, precision);
    mpc_init2(sum, precision);
    mpc_init2(add, precision);
    mpc_init2(work, precision);
    mpc_init2(power, precision);
    mpc_init2(polynomial, precision);
    mpfr_const_pi(pi, MPFR_RNDN);
    number_clausen_reduce(mpc_realref(z), mpc_realref(input));
    reflect = mpfr_sgn(mpc_realref(z)) < 0;
    mpfr_abs(mpc_realref(z), mpc_realref(z), MPFR_RNDN);
    mpfr_set(mpc_imagref(z), mpc_imagref(input), MPFR_RNDN);
    if (reflect)
        mpfr_neg(mpc_imagref(z), mpc_imagref(z), MPFR_RNDN);
    conjugate = mpfr_sgn(mpc_imagref(z)) < 0;
    mpfr_abs(mpc_imagref(z), mpc_imagref(z), MPFR_RNDN);
    if (mpfr_zero_p(mpc_realref(z))) {
        mpfr_set_nan(mpc_realref(sum));
        mpfr_set_nan(mpc_imagref(sum));
        goto done;
    }
    mpfr_add_ui(scratch, mpc_imagref(z), (unsigned long)bits, MPFR_RNDN);
    mpfr_mul_ui(scratch, scratch, 8ul, MPFR_RNDN);
    if (mpfr_cmp_ui(scratch, order) < 0) {
        if (order & 1ul)
            mpc_cos(sum, z, MPC_RNDNN);
        else
            mpc_sin(sum, z, MPC_RNDNN);
        goto done;
    }

    mpc_set_fr(work, pi, MPC_RNDNN);
    mpc_sub(work, work, z, MPC_RNDNN);
    mpc_abs(magnitude, work, MPFR_RNDN);
    if (order == 2ul && mpfr_cmp_d(magnitude, 1.5) < 0) {
        mpc_sqr(ratio, work, MPC_RNDNN);
        mpfr_sqr(scratch, pi, MPFR_RNDN);
        mpc_div_fr(ratio, ratio, scratch, MPC_RNDNN);
        mpc_set(term, ratio, MPC_RNDNN);
        mpfr_set_ui_2exp(quarter, 1ul, -2, MPFR_RNDN);
        mpfr_const_log2(scratch, MPFR_RNDN);
        mpc_set_fr(sum, scratch, MPC_RNDNN);
        for (unsigned long j = 1ul; j <= (unsigned long)precision + 64ul; ++j) {
            mpfr_zeta_ui(coefficient, 2ul * j, MPFR_RNDN);
            mpfr_ui_sub(scratch, 1ul, quarter, MPFR_RNDN);
            mpfr_mul(coefficient, coefficient, scratch, MPFR_RNDN);
            mpfr_div_ui(coefficient, coefficient, j, MPFR_RNDN);
            mpfr_div_ui(coefficient, coefficient, 2ul * j + 1ul, MPFR_RNDN);
            mpc_mul_fr(add, term, coefficient, MPC_RNDNN);
            mpc_sub(sum, sum, add, MPC_RNDNN);
            if (number_clausen_complex_small(add, sum, bits, magnitude, scratch)) {
                converged = true;
                break;
            }
            mpc_mul(term, term, ratio, MPC_RNDNN);
            mpfr_div_2ui(quarter, quarter, 2u, MPFR_RNDN);
        }
        mpc_mul(sum, sum, work, MPC_RNDNN);
        goto finish_series;
    }
    if (mpfr_cmp_ui(mpc_imagref(z), 2ul) <= 0) {
        if (order == 1ul) {
            mpc_div_ui(work, z, 2ul, MPC_RNDNN);
            mpc_sin(work, work, MPC_RNDNN);
            mpc_mul_ui(work, work, 2ul, MPC_RNDNN);
            mpc_log(sum, work, MPC_RNDNN);
            mpc_neg(sum, sum, MPC_RNDNN);
            goto done;
        }
        mpc_sqr(square, z, MPC_RNDNN);
        k = (order - 1ul) & 1ul;
        if (k)
            mpc_set(term, z, MPC_RNDNN);
        else
            mpc_set_ui(term, 1ul, MPC_RNDNN);
        mpc_set_ui(sum, 0ul, MPC_RNDNN);
        for (; k < order - 1ul; k += 2ul) {
            mpfr_zeta_ui(coefficient, order - k, MPFR_RNDN);
            mpc_mul_fr(add, term, coefficient, MPC_RNDNN);
            mpc_add(sum, sum, add, MPC_RNDNN);
            mpc_mul(term, term, square, MPC_RNDNN);
            mpc_div_ui(term, term, k + 1ul, MPC_RNDNN);
            mpc_div_ui(term, term, k + 2ul, MPC_RNDNN);
            mpc_neg(term, term, MPC_RNDNN);
        }
        mpfr_set_zero(harmonic, 0);
        for (k = 1ul; k < order; ++k) {
            mpfr_set_ui(scratch, k, MPFR_RNDN);
            mpfr_ui_div(scratch, 1ul, scratch, MPFR_RNDN);
            mpfr_add(harmonic, harmonic, scratch, MPFR_RNDN);
        }
        mpc_log(work, z, MPC_RNDNN);
        mpc_neg(work, work, MPC_RNDNN);
        mpfr_add(mpc_realref(work), mpc_realref(work), harmonic, MPFR_RNDN);
        mpc_mul(add, term, work, MPC_RNDNN);
        mpc_add(sum, sum, add, MPC_RNDNN);
        mpfr_mul_2ui(scratch, pi, 1u, MPFR_RNDN);
        mpfr_sqr(scratch, scratch, MPFR_RNDN);
        mpc_div_fr(ratio, square, scratch, MPC_RNDNN);
        mpc_mul(term, term, ratio, MPC_RNDNN);
        mpc_mul_ui(term, term, 2ul, MPC_RNDNN);
        mpc_div_ui(term, term, order, MPC_RNDNN);
        mpc_div_ui(term, term, order + 1ul, MPC_RNDNN);
        for (unsigned long j = 1ul; j <= (unsigned long)precision + 64ul; ++j) {
            mpfr_zeta_ui(coefficient, 2ul * j, MPFR_RNDN);
            mpc_mul_fr(add, term, coefficient, MPC_RNDNN);
            mpc_add(sum, sum, add, MPC_RNDNN);
            if (number_clausen_complex_small(add, sum, bits, magnitude, scratch)) {
                converged = true;
                break;
            }
            mpc_mul(term, term, ratio, MPC_RNDNN);
            mpc_mul_ui(term, term, 2ul * j, MPC_RNDNN);
            mpc_mul_ui(term, term, 2ul * j + 1ul, MPC_RNDNN);
            mpc_div_ui(term, term, order + 2ul * j, MPC_RNDNN);
            mpc_div_ui(term, term, order + 2ul * j + 1ul, MPC_RNDNN);
        }
        goto finish_series;
    }

    /* Li_n(exp(i z)) + (-1)^n Li_n(exp(-i z)) is the finite inversion polynomial. */
    mpc_mul_i(work, z, 1, MPC_RNDNN);
    mpc_exp(ratio, work, MPC_RNDNN);
    mpc_set(power, ratio, MPC_RNDNN);
    mpc_set_ui(sum, 0ul, MPC_RNDNN);
    for (k = 1ul; k <= (unsigned long)precision + 64ul; ++k) {
        mpfr_set_ui(coefficient, k, MPFR_RNDN);
        mpfr_pow_ui(coefficient, coefficient, order, MPFR_RNDN);
        mpc_div_fr(add, power, coefficient, MPC_RNDNN);
        mpc_add(sum, sum, add, MPC_RNDNN);
        if (number_clausen_complex_small(add, sum, bits, magnitude, scratch)) {
            converged = true;
            break;
        }
        mpc_mul(power, power, ratio, MPC_RNDNN);
    }
    mpc_set_ui(polynomial, 0ul, MPC_RNDNN);
    mpc_set_ui(term, 1ul, MPC_RNDNN);
    for (k = 0ul; k < order; ++k) {
        if (k + 1ul == order) {
            mpc_mul_fr(add, term, pi, MPC_RNDNN);
            mpc_mul_i(add, add, 1, MPC_RNDNN);
            mpc_add(polynomial, polynomial, add, MPC_RNDNN);
        } else if (((order - k) & 1ul) == 0ul) {
            mpfr_zeta_ui(coefficient, order - k, MPFR_RNDN);
            mpfr_mul_2ui(coefficient, coefficient, 1u, MPFR_RNDN);
            mpc_mul_fr(add, term, coefficient, MPC_RNDNN);
            mpc_add(polynomial, polynomial, add, MPC_RNDNN);
        }
        mpc_mul(term, term, work, MPC_RNDNN);
        mpc_div_ui(term, term, k + 1ul, MPC_RNDNN);
        if (!mpfr_number_p(mpc_realref(term)) || !mpfr_number_p(mpc_imagref(term))) {
            converged = false;
            break;
        }
    }
    mpc_sub(polynomial, polynomial, term, MPC_RNDNN);
    mpc_div_ui(polynomial, polynomial, 2ul, MPC_RNDNN);
    mpc_sub(sum, sum, polynomial, MPC_RNDNN);
    if ((order & 1ul) == 0ul)
        mpc_mul_i(sum, sum, -1, MPC_RNDNN);
finish_series:
    if (!converged) {
        mpfr_set_nan(mpc_realref(sum));
        mpfr_set_nan(mpc_imagref(sum));
    }
done:
    if (conjugate)
        mpc_conj(sum, sum, MPC_RNDNN);
    if (reflect && (order & 1ul) == 0ul)
        mpc_neg(sum, sum, MPC_RNDNN);
    mpc_set(out, sum, MPC_RNDNN);
    mpc_clear(polynomial);
    mpc_clear(power);
    mpc_clear(work);
    mpc_clear(add);
    mpc_clear(sum);
    mpc_clear(term);
    mpc_clear(ratio);
    mpc_clear(square);
    mpc_clear(z);
    mpfr_clears(pi, coefficient, harmonic, magnitude, scratch, quarter, (mpfr_ptr)0);
}

/* Dispatch Clausen evaluation without lowering the input's native precision. */
number_t num_clausen(unsigned long order, const number_t theta)
{
    number_math_family_t family = number_math_family_value(&theta);
    number_t *promoted;
    size_t precision_bits;

    if (order == 0ul || !num_is_finite(theta))
        return num_clone(NUM_NAN);
    if (family == NUMBER_MATH_QREAL) {
        qfloat_t value = qf_clausen(order, number_value_to_qfloat(&theta));

        return number_kind_value(&theta) == NUMBER_DOUBLE ? num_create_from_double(qf_to_double(value))
                                                          : num_create_from_qfloat(value);
    }
    if (family == NUMBER_MATH_QCOMPLEX)
        return num_create_from_qcomplex(qc_clausen(order, number_value_to_qcomplex(&theta)));
    if (family == NUMBER_MATH_INVALID)
        return number_invalid();
    if (num_is_real(theta)) {
        promoted = number_coerce(&theta, NUMBER_MPFR);
        if (!promoted || number_mpfr_ensure(number_impl(promoted)->value.mpfr, num_get_prec_bits(*promoted)) != 0) {
            number_box_free(promoted);
            return number_invalid();
        }
        number_mpfr_clausen(number_impl(promoted)->value.mpfr->value, order,
                            number_impl_const(promoted)->value.mpfr->value);
        return number_take(promoted);
    }
    promoted = number_coerce(&theta, NUMBER_COMPLEX);
    if (!promoted)
        return number_invalid();
    precision_bits = num_get_prec_bits(*promoted);
    if (precision_bits == 0u)
        precision_bits = num_get_default_prec_bits();
    {
        mpc_t input, output;
        number_t *wrapped = NULL;

        mpc_init2(input, (mpfr_prec_t)precision_bits);
        mpc_init2(output, (mpfr_prec_t)precision_bits);
        if (number_complex_get_mpc(input, number_impl_const(promoted)->value.cx, precision_bits) == 0) {
            number_mpc_clausen(output, order, input);
            wrapped = number_wrap_complex_mpc(output, precision_bits);
        }
        mpc_clear(output);
        mpc_clear(input);
        number_box_free(promoted);
        return wrapped ? number_take(wrapped) : number_invalid();
    }
}

/* Evaluate the order-two Clausen function at native precision. */
number_t num_clausen2(const number_t theta)
{
    return num_clausen(2ul, theta);
}

