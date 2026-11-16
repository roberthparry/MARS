/**
 * @file number_lerch_integer.c
 * @brief Checked finite rational evaluations of Lerch's transcendent at non-positive integer orders.
 *
 * For s=-m, applying a+z*d/dz repeatedly to 1/(1-z) gives P_m(z)/(1-z)^(m+1). A descending
 * coefficient recurrence constructs P_m without numerical differentiation or an infinite series. Extra working
 * precision and a uniform absolute error bound cover coefficient growth, cancellation and the denominator.
 * The number module uses this bounded attempt for real and complex arguments; failures leave its checked
 * defining-series fallback in charge. Short dyadic components use exact Gaussian-integer coefficients, with
 * only the final rational divisions rounded. Input conversions and coefficient storage are local and released on return.
 */
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
