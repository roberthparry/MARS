#include <gmp.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mfloat_internal.h"
#include "mrational/mrational_internal.h"

static const double MFLOAT_LOG10_2 = 0.3010299956639812;
static const double MFLOAT_LOG2_10 = 3.3219280948873626;

static size_t mfloat_default_precision_bits = MFLOAT_DEFAULT_PRECISION_BITS;

static void mfloat_prepare_constant(const mfloat_t *mfloat, mpfr_prec_t precision)
{
    if (mfloat && mfloat->constant_id != MFCONST_NONE)
        mfloat_constant_ensure(mfloat, precision);
}

static int mfloat_set_mpq_from_mrational(mpq_ptr dst, const mrational_t *value)
{
    if (!dst || !value)
        return -1;

    mrational_constant_ensure(value);
    mpq_set(dst, value->value);
    mpq_canonicalize(dst);
    return 0;
}

static mfloat_t *mfloat_alloc(size_t precision_bits)
{
    mfloat_t *mfloat;
    mpfr_prec_t precision;

    precision = (mpfr_prec_t)(precision_bits > 0 ? precision_bits
                                                  : mfloat_default_precision_bits);
    mfloat = calloc(1u, sizeof(*mfloat));
    if (!mfloat)
        return NULL;

    mpfr_init2(mfloat->value, precision);
    mpfr_set_zero(mfloat->value, 0);
    mfloat->constant_id = MFCONST_NONE;
    return mfloat;
}

bool mf_is_finite(const mfloat_t *mfloat)
{
    mfloat_prepare_constant(mfloat, (mpfr_prec_t)mf_get_default_precision());
    return mfloat && mpfr_number_p(mfloat->value) != 0;
}

bool mf_is_nan(const mfloat_t *mfloat)
{
    mfloat_prepare_constant(mfloat, (mpfr_prec_t)mf_get_default_precision());
    return mfloat && mpfr_nan_p(mfloat->value) != 0;
}

bool mf_is_inf(const mfloat_t *mfloat)
{
    mfloat_prepare_constant(mfloat, (mpfr_prec_t)mf_get_default_precision());
    return mfloat && mpfr_inf_p(mfloat->value) != 0;
}

mfloat_t *mf_new(void)
{
    return mf_new_prec(mfloat_default_precision_bits);
}

mfloat_t *mf_new_prec(size_t precision_bits)
{
    return mfloat_alloc(precision_bits);
}

mfloat_t *mf_create_long(long value)
{
    mfloat_t *mfloat = mf_new();

    if (!mfloat)
        return NULL;
    if (mf_set_long(mfloat, value) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

mfloat_t *mf_create_double(double value)
{
    mfloat_t *mfloat = mf_new();

    if (!mfloat)
        return NULL;
    if (mf_set_double(mfloat, value) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

mfloat_t *mf_create_qfloat(qfloat_t value)
{
    mfloat_t *mfloat = mf_new();

    if (!mfloat)
        return NULL;
    if (mf_set_qfloat(mfloat, value) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

mfloat_t *mf_create_mrational(const mrational_t *value)
{
    mfloat_t *mfloat = mf_new();

    if (!mfloat)
        return NULL;
    if (mf_set_mrational(mfloat, value) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

mfloat_t *mf_const_prec(const mfloat_t *constant, size_t precision_bits)
{
    mfloat_t *copy;

    if (!constant || precision_bits == 0)
        return NULL;
    mfloat_prepare_constant(constant, (mpfr_prec_t)precision_bits);
    copy = mf_new_prec(precision_bits);
    if (!copy)
        return NULL;
    mpfr_set(copy->value, constant->value, MPFR_RNDN);
    return copy;
}

mfloat_t *mf_const(const mfloat_t *constant)
{
    return mf_const_prec(constant, mf_get_default_precision());
}

mfloat_t *mf_clone(const mfloat_t *mfloat)
{
    if (!mfloat)
        return NULL;
    mfloat_prepare_constant(mfloat, (mpfr_prec_t)mf_get_default_precision());
    return mf_const_prec(mfloat, mf_get_precision(mfloat));
}

void mf_free(mfloat_t *mfloat)
{
    if (!mfloat || mfloat->constant_id != MFCONST_NONE)
        return;
    mpfr_clear(mfloat->value);
    free(mfloat);
}

void mf_clear(mfloat_t *mfloat)
{
    if (!mfloat || mfloat->constant_id != MFCONST_NONE)
        return;
    mpfr_set_zero(mfloat->value, 0);
}

int mf_set_precision(mfloat_t *mfloat, size_t precision_bits)
{
    if (!mfloat || precision_bits == 0 || mfloat->constant_id != MFCONST_NONE)
        return -1;
    mpfr_prec_round(mfloat->value, (mpfr_prec_t)precision_bits, MPFR_RNDN);
    return 0;
}

int mf_set_default_precision(size_t precision_bits)
{
    if (precision_bits == 0)
        return -1;
    mfloat_default_precision_bits = precision_bits;
    return 0;
}

static size_t mfloat_bits_to_decimal_digits(size_t precision_bits)
{
    size_t digits;

    if (precision_bits == 0u)
        return 0u;
    digits = (size_t)floor((double)precision_bits * MFLOAT_LOG10_2);
    return digits > 0u ? digits : 1u;
}

static size_t mfloat_decimal_digits_to_bits(size_t significant_digits)
{
    size_t bits;

    if (significant_digits == 0u)
        return 0u;
    bits = (size_t)ceil((double)significant_digits * MFLOAT_LOG2_10);
    return bits > 0u ? bits : 1u;
}

size_t mf_get_default_precision(void)
{
    return mfloat_default_precision_bits;
}

size_t mf_get_precision(const mfloat_t *mfloat)
{
    mfloat_prepare_constant(mfloat, (mpfr_prec_t)mf_get_default_precision());
    return mfloat ? (size_t)mpfr_get_prec(mfloat->value) : 0u;
}

int mf_set_default_precision_digits(size_t significant_digits)
{
    return mf_set_default_precision(
        mfloat_decimal_digits_to_bits(significant_digits));
}

size_t mf_get_default_precision_digits(void)
{
    return mfloat_bits_to_decimal_digits(mf_get_default_precision());
}

int mf_set_precision_digits(mfloat_t *mfloat, size_t significant_digits)
{
    return mf_set_precision(mfloat,
                            mfloat_decimal_digits_to_bits(significant_digits));
}

size_t mf_get_precision_digits(const mfloat_t *mfloat)
{
    return mfloat_bits_to_decimal_digits(mf_get_precision(mfloat));
}

int mf_set_long(mfloat_t *mfloat, long value)
{
    if (!mfloat || mfloat->constant_id != MFCONST_NONE)
        return -1;
    mpfr_set_si(mfloat->value, value, MPFR_RNDN);
    return 0;
}

int mf_set_double(mfloat_t *mfloat, double value)
{
    if (!mfloat || mfloat->constant_id != MFCONST_NONE)
        return -1;
    mpfr_set_d(mfloat->value, value, MPFR_RNDN);
    return 0;
}

int mf_set_qfloat(mfloat_t *mfloat, qfloat_t value)
{
    if (!mfloat || mfloat->constant_id != MFCONST_NONE)
        return -1;
    mpfr_set_d(mfloat->value, value.hi, MPFR_RNDN);
    if (isfinite(value.hi) && isfinite(value.lo) && value.lo != 0.0)
        mpfr_add_d(mfloat->value, mfloat->value, value.lo, MPFR_RNDN);
    return 0;
}

int mf_set_mrational(mfloat_t *mfloat, const mrational_t *value)
{
    mpq_t q;
    int rc = -1;

    if (!mfloat || !value || mfloat->constant_id != MFCONST_NONE)
        return -1;
    mpq_init(q);
    if (mfloat_set_mpq_from_mrational(q, value) == 0) {
        mpfr_set_q(mfloat->value, q, MPFR_RNDN);
        rc = 0;
    }
    mpq_clear(q);
    return rc;
}

static int mfloat_apply_mrational_inplace(mfloat_t *mfloat, const mrational_t *value,
                                          int (*op)(mpfr_ptr, mpfr_srcptr, mpq_srcptr,
                                                    mpfr_rnd_t))
{
    mpq_t rhs;
    int rc = -1;

    if (!mfloat || !value || !op || mfloat->constant_id != MFCONST_NONE)
        return -1;

    mpq_init(rhs);
    if (mfloat_set_mpq_from_mrational(rhs, value) == 0) {
        (void)op(mfloat->value, mfloat->value, rhs, MPFR_RNDN);
        rc = 0;
    }
    mpq_clear(rhs);
    return rc;
}

int mf_add_mrational(mfloat_t *mfloat, const mrational_t *value)
{
    return mfloat_apply_mrational_inplace(mfloat, value, mpfr_add_q);
}

int mf_sub_mrational(mfloat_t *mfloat, const mrational_t *value)
{
    return mfloat_apply_mrational_inplace(mfloat, value, mpfr_sub_q);
}

int mf_mul_mrational(mfloat_t *mfloat, const mrational_t *value)
{
    return mfloat_apply_mrational_inplace(mfloat, value, mpfr_mul_q);
}

int mf_set_string(mfloat_t *mfloat, const char *text)
{
    char *end = NULL;

    if (!mfloat || !text || mfloat->constant_id != MFCONST_NONE)
        return -1;

    mpfr_strtofr(mfloat->value, text, &end, 0, MPFR_RNDN);
    if (!end) {
        return -1;
    }
    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')
        ++end;
    if (*end != '\0') {
        return -1;
    }
    return 0;
}

bool mf_is_zero(const mfloat_t *mfloat)
{
    mfloat_prepare_constant(mfloat, (mpfr_prec_t)mf_get_default_precision());
    return mfloat && mpfr_zero_p(mfloat->value) != 0;
}

short mf_get_sign(const mfloat_t *mfloat)
{
    int sign;

    if (!mfloat)
        return 0;
    mfloat_prepare_constant(mfloat, (mpfr_prec_t)mf_get_default_precision());
    sign = mpfr_sgn(mfloat->value);
    if (sign < 0)
        return -1;
    if (sign > 0)
        return 1;
    return 0;
}

long mf_get_exponent2(const mfloat_t *mfloat)
{
    mpz_t z;
    mpfr_exp_t exponent2;
    mp_bitcnt_t trailing_zero_bits;

    if (!mfloat || !mf_is_finite(mfloat) || mf_is_zero(mfloat))
        return 0;

    mfloat_prepare_constant(mfloat, (mpfr_prec_t)mf_get_default_precision());

    mpz_init(z);
    exponent2 = mpfr_get_z_2exp(z, mfloat->value);
    if (mpz_sgn(z) < 0)
        mpz_neg(z, z);
    trailing_zero_bits = mpz_scan1(z, 0);
    if (trailing_zero_bits != (mp_bitcnt_t)-1)
        exponent2 += (mpfr_exp_t)trailing_zero_bits;
    mpz_clear(z);
    return (long)exponent2;
}

size_t mf_get_mantissa_bits(const mfloat_t *mfloat)
{
    mpz_t z;
    size_t bits;

    if (!mfloat || !mf_is_finite(mfloat) || mf_is_zero(mfloat))
        return 0u;

    mfloat_prepare_constant(mfloat, (mpfr_prec_t)mf_get_default_precision());

    mpz_init(z);
    (void)mpfr_get_z_2exp(z, mfloat->value);
    if (mpz_sgn(z) < 0)
        mpz_neg(z, z);
    {
        mp_bitcnt_t trailing_zero_bits = mpz_scan1(z, 0);

        if (trailing_zero_bits != (mp_bitcnt_t)-1)
            mpz_tdiv_q_2exp(z, z, trailing_zero_bits);
    }
    bits = (size_t)mpz_sizeinbase(z, 2);
    mpz_clear(z);
    return bits;
}

bool mf_get_mantissa_u64(const mfloat_t *mfloat, uint64_t *out)
{
    mpz_t z;

    if (!mfloat || !out || !mf_is_finite(mfloat) || mf_is_zero(mfloat))
        return false;

    mfloat_prepare_constant(mfloat, (mpfr_prec_t)mf_get_default_precision());

    mpz_init(z);
    (void)mpfr_get_z_2exp(z, mfloat->value);
    if (mpz_sgn(z) < 0)
        mpz_neg(z, z);
    {
        mp_bitcnt_t trailing_zero_bits = mpz_scan1(z, 0);

        if (trailing_zero_bits != (mp_bitcnt_t)-1)
            mpz_tdiv_q_2exp(z, z, trailing_zero_bits);
    }
    if (mpz_sizeinbase(z, 2) > 64u) {
        mpz_clear(z);
        return false;
    }
    *out = (uint64_t)mpz_get_ui(z);
    mpz_clear(z);
    return true;
}
