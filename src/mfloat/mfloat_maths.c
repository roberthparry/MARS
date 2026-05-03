#include "mfloat_internal.h"
#include "internal/mint_support.h"

#include <limits.h>
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define MFLOAT_CONST_LITERAL_BITS 256u
#define MFLOAT_CONST_GUARD_BITS   32u
#define MFLOAT_QFLOAT_EFFECTIVE_BITS 106u
#define MFLOAT_GAMMA_MIN_SHIFT    12u
#define MFLOAT_GAMMA_MAX_SHIFT    18u
#define MFLOAT_LGAMMA_ASYMPTOTIC_TERM_COUNT 40u

typedef struct mfloat_asymp_term_t {
    long num;
    long den;
    int sign;
    long mult;
} mfloat_asymp_term_t;

/* Stack-backed scratch slots avoid hot-path mfloat container allocation. */
typedef struct mfloat_scratch_slot_t {
    mfloat_t value;
    mint_t mantissa;
} mfloat_scratch_slot_t;

static mfloat_t *mfloat_clone_prec(const mfloat_t *src, size_t precision);
static int mfloat_round_to_precision(mfloat_t *mfloat, size_t precision);
static int mfloat_compute_pi(mfloat_t *dst, size_t precision);
static int mfloat_compute_e(mfloat_t *dst, size_t precision);
static int mfloat_compute_euler_gamma(mfloat_t *dst, size_t precision);
static int mfloat_compute_sqrt_pi(mfloat_t *dst, size_t precision);
static int mfloat_compute_half_ln_pi(mfloat_t *dst, size_t precision);
static void mfloat_scratch_init_slot(mfloat_scratch_slot_t *slot, size_t precision);
static void mfloat_scratch_reset_slot(mfloat_scratch_slot_t *slot, size_t precision);
static void mfloat_scratch_release_slot(mfloat_scratch_slot_t *slot);
static int mfloat_scratch_copy(mfloat_t *dst, const mfloat_t *src);
static int mfloat_is_below_neg_bits(const mfloat_t *mfloat, long bits);
static int mfloat_get_exact_long_value(const mfloat_t *mfloat, long *out);
static int mfloat_equals_exact_long(const mfloat_t *mfloat, long value);
static int mfloat_is_exact_half(const mfloat_t *mfloat, short sign);

static void mfloat_scratch_init_slot(mfloat_scratch_slot_t *slot, size_t precision)
{
    memset(slot, 0, sizeof(*slot));
    slot->value.kind = MFLOAT_KIND_FINITE;
    slot->value.precision = precision;
    slot->value.mantissa = &slot->mantissa;
}

static void mfloat_scratch_reset_slot(mfloat_scratch_slot_t *slot, size_t precision)
{
    slot->value.kind = MFLOAT_KIND_FINITE;
    slot->value.sign = 0;
    slot->value.exponent2 = 0;
    slot->value.precision = precision;
    slot->value.mantissa = &slot->mantissa;
    slot->mantissa.sign = 0;
    slot->mantissa.length = 0;
}

static void mfloat_scratch_release_slot(mfloat_scratch_slot_t *slot)
{
    free(slot->mantissa.storage);
    slot->mantissa.storage = NULL;
    slot->mantissa.sign = 0;
    slot->mantissa.length = 0;
    slot->mantissa.capacity = 0;
}

static int mfloat_scratch_copy(mfloat_t *dst, const mfloat_t *src)
{
    if (!dst || !src || !dst->mantissa || !src->mantissa)
        return -1;
    if (mint_copy_value(dst->mantissa, src->mantissa) != 0)
        return -1;
    dst->kind = src->kind;
    dst->sign = src->sign;
    dst->exponent2 = src->exponent2;
    return 0;
}

static int mfloat_get_exact_long_value(const mfloat_t *mfloat, long *out)
{
    long mant_long;
    unsigned long magnitude;
    long value;

    if (!mfloat || !out || !mfloat_is_finite(mfloat))
        return 0;
    if (mf_is_zero(mfloat)) {
        *out = 0;
        return 1;
    }
    if (mfloat->exponent2 < 0)
        return 0;
    if (!mi_fits_long(mfloat->mantissa) || !mi_get_long(mfloat->mantissa, &mant_long))
        return 0;
    if (mant_long < 0)
        return 0;
    magnitude = (unsigned long)mant_long;
    if (mfloat->exponent2 >= (long)(sizeof(unsigned long) * CHAR_BIT))
        return 0;
    if (mfloat->sign < 0) {
        if (mfloat->exponent2 > 0 &&
            magnitude > ((((unsigned long)LONG_MAX) + 1u) >> mfloat->exponent2))
            return 0;
        value = (long)(magnitude << mfloat->exponent2);
        if (value > 0)
            return 0;
        *out = value;
        return 1;
    }
    if (mfloat->exponent2 > 0 && magnitude > ((unsigned long)LONG_MAX >> mfloat->exponent2))
        return 0;
    *out = (long)(magnitude << mfloat->exponent2);
    return 1;
}

static int mfloat_equals_exact_long(const mfloat_t *mfloat, long value)
{
    long actual;

    return mfloat_get_exact_long_value(mfloat, &actual) && actual == value;
}

static int mfloat_is_exact_half(const mfloat_t *mfloat, short sign)
{
    long mantissa_value;

    if (!mfloat || !mfloat_is_finite(mfloat) || mfloat->sign != sign || mfloat->exponent2 != -1)
        return 0;
    if (!mi_fits_long(mfloat->mantissa) || !mi_get_long(mfloat->mantissa, &mantissa_value))
        return 0;
    return mantissa_value == 1;
}

static int mfloat_set_from_binary_seed(mfloat_t *dst, const mfloat_t *seed, size_t precision)
{
    mfloat_t *tmp = NULL;
    int rc = -1;

    if (!dst || !seed || precision == 0 || precision > seed->precision)
        return -1;
    tmp = mfloat_clone_prec(seed, seed->precision);
    if (!tmp)
        return -1;
    if (mfloat_round_to_precision(tmp, precision) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, tmp);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(tmp);
    return rc;
}

static mfloat_t *mfloat_new_from_long_prec(long value, size_t precision)
{
    mfloat_t *mfloat = mf_new_prec(precision);

    if (!mfloat)
        return NULL;
    if (mf_set_long(mfloat, value) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

static mfloat_t *mfloat_new_from_qfloat_prec(qfloat_t value, size_t precision)
{
    mfloat_t *mfloat = mf_new_prec(precision);

    if (!mfloat)
        return NULL;
    if (mf_set_qfloat(mfloat, value) != 0 || mf_set_precision(mfloat, precision) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

static mfloat_t *mfloat_new_pi_prec(size_t precision)
{
    mfloat_t *mfloat = mf_new_prec(precision);

    if (!mfloat)
        return NULL;
    if ((precision <= MF_PI->precision
            ? mfloat_set_from_binary_seed(mfloat, MF_PI, precision)
            : mfloat_compute_pi(mfloat, precision)) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

static mfloat_t *mfloat_new_euler_gamma_prec(size_t precision)
{
    mfloat_t *mfloat = mf_new_prec(precision);

    /* Internal series code needs Euler's constant at the actual working precision. */
    if (!mfloat)
        return NULL;
    if ((precision <= MF_EULER_MASCHERONI->precision
            ? mfloat_set_from_binary_seed(mfloat, MF_EULER_MASCHERONI, precision)
            : mfloat_compute_euler_gamma(mfloat, precision)) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

static mfloat_t *mfloat_new_sqrt_pi_prec(size_t precision)
{
    mfloat_t *mfloat = mf_new_prec(precision);

    if (!mfloat)
        return NULL;
    if ((precision <= MF_SQRT_PI->precision
            ? mfloat_set_from_binary_seed(mfloat, MF_SQRT_PI, precision)
            : mfloat_compute_sqrt_pi(mfloat, precision)) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

static mfloat_t *mfloat_new_half_ln_pi_prec(size_t precision)
{
    mfloat_t *mfloat = mf_new_prec(precision);

    if (!mfloat)
        return NULL;
    if ((precision <= MFLOAT_INTERNAL_HALF_LN_PI->precision
            ? mfloat_set_from_binary_seed(mfloat, MFLOAT_INTERNAL_HALF_LN_PI, precision)
            : mfloat_compute_half_ln_pi(mfloat, precision)) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

static int mfloat_div_long_inplace(mfloat_t *mfloat, long value)
{
    mint_t *num = NULL, *q = NULL;
    size_t shift_bits;
    size_t odd_bits;
    long exponent2;
    long rem = 0;
    unsigned long magnitude;
    unsigned long odd_part;
    unsigned int shift_twos = 0u;

    if (!mfloat || value == 0 || !mfloat->mantissa)
        return -1;
    if (!mfloat_is_finite(mfloat)) {
        if (mfloat->kind == MFLOAT_KIND_NAN)
            return 0;
        return mf_set_double(mfloat, ((value < 0) ^ (mfloat->sign < 0)) ? -INFINITY : INFINITY);
    }
    if (mf_is_zero(mfloat))
        return 0;
    if (value == 1)
        return 0;
    if (value == -1) {
        mfloat->sign = (short)-mfloat->sign;
        return 0;
    }

    magnitude = value < 0 ? (unsigned long)(-(value + 1)) + 1ul
                          : (unsigned long)value;
    odd_part = magnitude;
    while ((odd_part & 1ul) == 0ul) {
        odd_part >>= 1;
        shift_twos++;
    }

    if (shift_twos > 0u)
        mfloat->exponent2 -= (long)shift_twos;
    if (odd_part == 1ul) {
        if (value < 0)
            mfloat->sign = (short)-mfloat->sign;
        return 0;
    }

    num = mi_clone(mfloat->mantissa);
    q = mi_new();
    if (!num || !q)
        goto cleanup;

    odd_bits = (size_t)(sizeof(unsigned long) * CHAR_BIT - __builtin_clzl(odd_part));
    shift_bits = mfloat->precision + odd_bits + MFLOAT_PARSE_GUARD_BITS;
    if (mi_shl(num, (long)shift_bits) != 0)
        goto cleanup;
    if (mi_div_long(num, (long)odd_part, &rem) != 0)
        goto cleanup;
    if (mint_set_magnitude_u64(q, (uint64_t)(rem < 0 ? -rem : rem), 1) != 0)
        goto cleanup;
    if (mi_mul_long(q, 2) != 0)
        goto cleanup;
    if (mi_cmp_long(q, (long)odd_part) >= 0) {
        if (mi_inc(num) != 0)
            goto cleanup;
    }

    mi_clear(mfloat->mantissa);
    if (mi_add(mfloat->mantissa, num) != 0)
        goto cleanup;

    exponent2 = mfloat->exponent2 - (long)shift_bits;
    if (value < 0)
        mfloat->sign = (short)-mfloat->sign;
    mfloat->exponent2 = exponent2;
    {
        int rc = mfloat_normalise(mfloat);
        mi_free(num);
        mi_free(q);
        return rc;
    }

cleanup:
    mi_free(num);
    mi_free(q);
    return -1;
}

static int mfloat_compute_sqrt_pi(mfloat_t *dst, size_t precision)
{
    mfloat_t *pi = mfloat_new_pi_prec(precision);
    int rc = -1;

    if (!pi)
        return -1;
    if (mf_sqrt(pi) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, pi);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(pi);
    return rc;
}

static int mfloat_compute_half_ln_pi(mfloat_t *dst, size_t precision)
{
    mfloat_t *pi = mfloat_new_pi_prec(precision);
    int rc = -1;

    if (!pi)
        return -1;
    if (mf_log(pi) != 0 || mfloat_div_long_inplace(pi, 2) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, pi);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(pi);
    return rc;
}

static int mfloat_mul_long_inplace(mfloat_t *mfloat, long value)
{
    return mf_mul_long(mfloat, value);
}

static int mfloat_add_signed_rational_multiple(mfloat_t *sum,
                                               const mfloat_t *factor,
                                               long num,
                                               long den,
                                               int sign,
                                               long mult,
                                               size_t precision)
{
    mfloat_t *term = NULL;
    int rc = -1;

    if (!sum || !factor || den == 0 || mult == 0)
        return -1;
    term = mfloat_clone_prec(factor, precision);
    if (!term)
        goto cleanup;
    if (mult != 1 && mf_mul_long(term, mult) != 0)
        goto cleanup;
    if (num != 1 && mf_mul_long(term, num) != 0)
        goto cleanup;
    if (mfloat_div_long_inplace(term, den) != 0)
        goto cleanup;
    if (sign < 0 && mf_neg(term) != 0)
        goto cleanup;
    rc = mf_add(sum, term);

cleanup:
    mf_free(term);
    return rc;
}

static int mfloat_add_half_ln_2pi(mfloat_t *sum, size_t precision)
{
    mfloat_t *half_ln_2pi = NULL;
    int rc = -1;

    if (!sum)
        goto cleanup;
    half_ln_2pi = mfloat_clone_immortal_prec_internal(MFLOAT_INTERNAL_HALF_LN_2PI, precision);
    if (!half_ln_2pi)
        goto cleanup;
    if (mf_add(sum, half_ln_2pi) != 0)
        goto cleanup;
    rc = 0;

cleanup:
    mf_free(half_ln_2pi);
    return rc;
}

static int mfloat_copy_cached_lgamma_asymptotic_term(mfloat_t *dst, size_t index, size_t precision)
{
    mfloat_t *tmp = NULL;
    int rc = -1;

    if (!dst || index >= MFLOAT_LGAMMA_ASYMPTOTIC_TERM_COUNT)
        return -1;

    tmp = mf_new_prec(precision);
    if (!tmp ||
        mfloat_copy_lgamma_asymptotic_term_internal(tmp, index, precision) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, tmp);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(tmp);
    return rc;
}

static int mfloat_is_near_integer_pole(const mfloat_t *x)
{
    double xd;

    if (!x || !mfloat_is_finite(x))
        return 0;
    xd = mf_to_double(x);
    return xd <= 0.0 && fabs(xd - nearbyint(xd)) < 1e-15;
}

static int mfloat_lgamma_asymptotic(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    mfloat_t *logx = NULL, *sum = NULL, *xi = NULL, *xi2 = NULL, *xpow = NULL, *term = NULL;
    int rc = -1;

    logx = mfloat_clone_prec(x, precision);
    sum = mfloat_clone_prec(x, precision);
    xi = mfloat_clone_prec(MF_ONE, precision);
    if (!logx || !sum || !xi)
        goto cleanup;
    if (mf_log(logx) != 0)
        goto cleanup;
    if (mf_sub(sum, MF_HALF) != 0)
        goto cleanup;
    if (mf_mul(sum, logx) != 0 || mf_sub(sum, x) != 0)
        goto cleanup;
    if (mfloat_add_half_ln_2pi(sum, precision) != 0)
        goto cleanup;

    if (mf_div(xi, x) != 0)
        goto cleanup;
    xi2 = mf_clone(xi);
    xpow = mf_clone(xi);
    term = mf_new_prec(precision);
    if (!xi2 || !xpow || !term)
        goto cleanup;
    if (mf_mul(xi2, xi) != 0)
        goto cleanup;

    for (size_t i = 0; i < MFLOAT_LGAMMA_ASYMPTOTIC_TERM_COUNT; ++i) {
        if (mfloat_copy_cached_lgamma_asymptotic_term(term, i, precision) != 0 ||
            mf_mul(term, xpow) != 0 || mf_add(sum, term) != 0)
            goto cleanup;
        if (mf_mul(xpow, xi2) != 0)
            goto cleanup;
    }

    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(logx);
    mf_free(sum);
    mf_free(xi);
    mf_free(xi2);
    mf_free(xpow);
    mf_free(term);
    return rc;
}

static int mfloat_digamma_asymptotic(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    static const mfloat_asymp_term_t terms[] = {
        {1, 12, -1, 1},
        {1, 120, 1, 1},
        {1, 252, -1, 1},
        {1, 240, 1, 1},
        {1, 132, -1, 1},
        {691, 32760, 1, 1},
        {1, 12, -1, 1},
        {3617, 8160, 1, 1}
    };
    mfloat_t *sum = NULL, *xi = NULL, *xi2 = NULL, *xpow = NULL, *term = NULL;
    int rc = -1;

    sum = mfloat_clone_prec(x, precision);
    xi = mfloat_clone_prec(MF_ONE, precision);
    if (!sum || !xi)
        goto cleanup;
    if (mf_log(sum) != 0 || mf_div(xi, x) != 0)
        goto cleanup;

    term = mf_clone(xi);
    if (!term || mfloat_div_long_inplace(term, 2) != 0 || mf_sub(sum, term) != 0)
        goto cleanup;
    mf_free(term);
    term = NULL;

    xi2 = mf_clone(xi);
    if (!xi2 || mf_mul(xi2, xi) != 0)
        goto cleanup;
    xpow = mf_clone(xi2);
    if (!xpow)
        goto cleanup;

    for (size_t i = 0; i < sizeof(terms) / sizeof(terms[0]); ++i) {
        if (mfloat_add_signed_rational_multiple(sum, xpow,
                                                terms[i].num, terms[i].den,
                                                terms[i].sign, terms[i].mult,
                                                precision) != 0)
            goto cleanup;
        if (mf_mul(xpow, xi2) != 0)
            goto cleanup;
    }

    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(sum);
    mf_free(xi);
    mf_free(xi2);
    mf_free(xpow);
    mf_free(term);
    return rc;
}

static int mfloat_trigamma_asymptotic(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    static const mfloat_asymp_term_t terms[] = {
        {1, 6, 1, 1},
        {1, 30, -1, 1},
        {1, 42, 1, 1},
        {1, 30, -1, 1},
        {5, 66, 1, 1},
        {691, 2730, -1, 1},
        {7, 6, 1, 1},
        {3617, 510, -1, 1}
    };
    mfloat_t *sum = NULL, *xi = NULL, *xi2 = NULL, *xpow = NULL, *term = NULL;
    int rc = -1;

    sum = mfloat_clone_prec(MF_ONE, precision);
    xi = mfloat_clone_prec(MF_ONE, precision);
    if (!sum || !xi)
        goto cleanup;
    if (mf_div(sum, x) != 0 || mf_div(xi, x) != 0)
        goto cleanup;

    term = mf_clone(xi);
    if (!term || mf_mul(term, xi) != 0 || mfloat_div_long_inplace(term, 2) != 0 ||
        mf_add(sum, term) != 0)
        goto cleanup;
    mf_free(term);
    term = NULL;

    xi2 = mf_clone(xi);
    if (!xi2 || mf_mul(xi2, xi) != 0)
        goto cleanup;
    xpow = mf_clone(xi2);
    if (!xpow || mf_mul(xpow, xi) != 0)
        goto cleanup;

    for (size_t i = 0; i < sizeof(terms) / sizeof(terms[0]); ++i) {
        if (mfloat_add_signed_rational_multiple(sum, xpow,
                                                terms[i].num, terms[i].den,
                                                terms[i].sign, terms[i].mult,
                                                precision) != 0)
            goto cleanup;
        if (mf_mul(xpow, xi2) != 0)
            goto cleanup;
    }

    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(sum);
    mf_free(xi);
    mf_free(xi2);
    mf_free(xpow);
    mf_free(term);
    return rc;
}

static int mfloat_tetragamma_asymptotic(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    static const mfloat_asymp_term_t terms[] = {
        {1, 6, 1, 3},
        {1, 30, -1, 5},
        {1, 42, 1, 7},
        {1, 30, -1, 9},
        {5, 66, 1, 11},
        {691, 2730, -1, 13},
        {7, 6, 1, 15},
        {3617, 510, -1, 17},
        {43867, 798, 1, 19},
        {174611, 330, -1, 21},
        {854513, 138, 1, 23},
        {236364091, 2730, -1, 25},
        {8553103, 6, 1, 27},
        {23749461029LL, 870, -1, 29},
        {8615841276005LL, 14322, 1, 31},
        {7709321041217LL, 510, -1, 33},
        {2577687858367LL, 6, 1, 35}
    };
    mfloat_t *sum = NULL, *xi = NULL, *xi2 = NULL, *xpow = NULL;
    int rc = -1;

    sum = mfloat_clone_prec(MF_ONE, precision);
    xi = mfloat_clone_prec(MF_ONE, precision);
    if (!sum || !xi)
        goto cleanup;
    if (mf_div(sum, x) != 0 || mf_div(xi, x) != 0 || mf_mul(sum, xi) != 0)
        goto cleanup;

    xpow = mf_clone(sum);
    if (!xpow || mf_mul(xpow, xi) != 0 || mf_add(sum, xpow) != 0)
        goto cleanup;
    xi2 = mf_clone(xi);
    if (!xi2 || mf_mul(xi2, xi) != 0 || mf_mul(xpow, xi) != 0)
        goto cleanup;

    for (size_t i = 0; i < sizeof(terms) / sizeof(terms[0]); ++i) {
        if (mfloat_add_signed_rational_multiple(sum, xpow,
                                                terms[i].num, terms[i].den,
                                                terms[i].sign, terms[i].mult,
                                                precision) != 0)
            goto cleanup;
        if (mf_mul(xpow, xi2) != 0)
            goto cleanup;
    }
    if (mf_neg(sum) != 0)
        goto cleanup;

    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(sum);
    mf_free(xi);
    mf_free(xi2);
    mf_free(xpow);
    return rc;
}

static int mfloat_eval_reflected_gamma_family(mfloat_t *dst,
                                              const mfloat_t *x,
                                              size_t precision,
                                              int order)
{
    mfloat_t *one_minus_x = NULL, *pix = NULL, *sinpix = NULL, *cospix = NULL;
    mfloat_t *poly = NULL, *tmp = NULL;
    int rc = -1;

    one_minus_x = mfloat_clone_prec(MF_ONE, precision);
    pix = mfloat_new_pi_prec(precision);
    if (!one_minus_x || !pix)
        goto cleanup;
    if (mf_sub(one_minus_x, x) != 0 || mf_mul(pix, x) != 0)
        goto cleanup;

    if (order == 0) {
        if (mf_digamma(one_minus_x) != 0)
            goto cleanup;
        sinpix = mf_clone(pix);
        cospix = mf_clone(pix);
        if (!sinpix || !cospix || mf_sin(sinpix) != 0 || mf_cos(cospix) != 0 ||
            mf_div(cospix, sinpix) != 0 || mf_mul(cospix, pix) != 0 ||
            mf_sub(one_minus_x, cospix) != 0)
            goto cleanup;
        rc = mfloat_copy_value(dst, one_minus_x);
        if (rc == 0)
            dst->precision = precision;
        goto cleanup;
    }

    if (order == 1) {
        if (mf_trigamma(one_minus_x) != 0)
            goto cleanup;
        sinpix = mf_clone(pix);
        if (!sinpix || mf_sin(sinpix) != 0 || mf_mul(sinpix, sinpix) != 0)
            goto cleanup;
        tmp = mf_clone(pix);
        if (!tmp || mf_mul(tmp, pix) != 0 || mf_div(tmp, sinpix) != 0 ||
            mf_sub(tmp, one_minus_x) != 0)
            goto cleanup;
        rc = mfloat_copy_value(dst, tmp);
        if (rc == 0)
            dst->precision = precision;
        goto cleanup;
    }

    if (order == 2) {
        if (mf_tetragamma(one_minus_x) != 0)
            goto cleanup;
        sinpix = mf_clone(pix);
        cospix = mf_clone(pix);
        poly = mf_clone(pix);
        tmp = mf_clone(pix);
        if (!sinpix || !cospix || !poly || !tmp ||
            mf_sin(sinpix) != 0 || mf_cos(cospix) != 0)
            goto cleanup;
        if (mf_mul(tmp, pix) != 0 || mf_mul(tmp, pix) != 0 || mf_mul_long(tmp, 2) != 0)
            goto cleanup;
        if (mf_mul(poly, cospix) != 0 || mf_mul(poly, tmp) != 0)
            goto cleanup;
        if (mf_mul(sinpix, sinpix) != 0 || mf_mul(sinpix, sinpix) != 0 ||
            mf_div(poly, sinpix) != 0 || mf_add(poly, one_minus_x) != 0)
            goto cleanup;
        rc = mfloat_copy_value(dst, poly);
        if (rc == 0)
            dst->precision = precision;
    }

cleanup:
    mf_free(one_minus_x);
    mf_free(pix);
    mf_free(sinpix);
    mf_free(cospix);
    mf_free(poly);
    mf_free(tmp);
    return rc;
}

static int mfloat_set_factorial(mfloat_t *dst, long n, size_t precision)
{
    mfloat_t *value = NULL;
    int rc = -1;

    if (!dst || n < 0)
        return -1;
    value = mfloat_clone_prec(MF_ONE, precision);
    if (!value)
        return -1;
    for (long k = 2; k <= n; ++k) {
        if (mf_mul_long(value, k) != 0)
            goto cleanup;
    }
    rc = mfloat_copy_value(dst, value);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(value);
    return rc;
}

static int mfloat_compute_two_over_sqrt_pi(mfloat_t *dst, size_t precision)
{
    mfloat_t *pi = NULL;
    int rc = -1;

    if (!dst)
        return -1;
    pi = mfloat_new_pi_prec(precision);
    if (!pi || mf_sqrt(pi) != 0 || mf_inv(pi) != 0 || mf_mul_long(pi, 2) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, pi);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(pi);
    return rc;
}

static int mfloat_ei_series(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    mfloat_t *sum = NULL, *term = NULL, *piece = NULL, *abs_x = NULL, *log_abs_x = NULL, *gamma = NULL;
    int rc = -1;

    if (!dst || !x)
        return -1;
    if (mf_is_zero(x))
        return mf_set_double(dst, -INFINITY);

    sum = mfloat_clone_prec(MF_ZERO, precision);
    term = mfloat_clone_prec(x, precision);
    abs_x = mfloat_clone_prec(x, precision);
    if (!sum || !term || !abs_x || mf_abs(abs_x) != 0)
        goto cleanup;
    log_abs_x = mf_clone(abs_x);
    gamma = mfloat_new_euler_gamma_prec(precision);
    if (!log_abs_x || !gamma || mf_log(log_abs_x) != 0 ||
        mf_add(sum, gamma) != 0 || mf_add(sum, log_abs_x) != 0)
        goto cleanup;

    for (long k = 1; k < 4096; ++k) {
        piece = mf_clone(term);
        if (!piece || mfloat_div_long_inplace(piece, k) != 0 || mf_add(sum, piece) != 0)
            goto cleanup;
        mf_free(piece);
        piece = NULL;
        if (mfloat_is_below_neg_bits(term, (long)precision + 8l))
            break;
        if (mf_mul(term, x) != 0 || mfloat_div_long_inplace(term, k + 1) != 0)
            goto cleanup;
    }

    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(sum);
    mf_free(term);
    mf_free(piece);
    mf_free(abs_x);
    mf_free(log_abs_x);
    mf_free(gamma);
    return rc;
}

static int mfloat_e1_series_positive(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    mfloat_t *sum = NULL, *term = NULL, *piece = NULL, *log_x = NULL, *gamma = NULL;
    int rc = -1;

    if (!dst || !x)
        return -1;
    if (mf_le(x, MF_ZERO))
        return mf_set_double(dst, NAN);

    sum = mfloat_clone_prec(MF_ZERO, precision);
    term = mfloat_clone_prec(x, precision);
    if (!sum || !term || mf_neg(term) != 0)
        goto cleanup;
    log_x = mfloat_clone_prec(x, precision);
    gamma = mfloat_new_euler_gamma_prec(precision);
    if (!log_x || !gamma || mf_log(log_x) != 0 ||
        mf_sub(sum, gamma) != 0 || mf_sub(sum, log_x) != 0)
        goto cleanup;

    for (long k = 1; k < 4096; ++k) {
        piece = mf_clone(term);
        if (!piece || mfloat_div_long_inplace(piece, k) != 0 || mf_sub(sum, piece) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(piece, (long)precision + 32l)) {
            mf_free(piece);
            piece = NULL;
            break;
        }
        mf_free(piece);
        piece = NULL;
        if (mf_mul(term, x) != 0 || mf_neg(term) != 0 || mfloat_div_long_inplace(term, k + 1) != 0)
            goto cleanup;
    }

    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(sum);
    mf_free(term);
    mf_free(piece);
    mf_free(log_x);
    mf_free(gamma);
    return rc;
}

static int mfloat_gammainc_series_P(mfloat_t *dst,
                                    const mfloat_t *s,
                                    const mfloat_t *x,
                                    size_t precision)
{
    mfloat_t *lgamma_s = NULL, *log_x = NULL, *pref = NULL;
    mfloat_t *sum = NULL, *term = NULL, *nq = NULL, *spn = NULL, *tol = NULL;
    int rc = -1;

    lgamma_s = mfloat_clone_prec(s, precision);
    log_x = mfloat_clone_prec(x, precision);
    if (!lgamma_s || !log_x)
        goto cleanup;
    if (mf_lgamma(lgamma_s) != 0 || mf_log(log_x) != 0)
        goto cleanup;

    pref = mf_clone(log_x);
    if (!pref || mf_mul(pref, s) != 0 || mf_sub(pref, x) != 0 || mf_sub(pref, lgamma_s) != 0 ||
        mf_exp(pref) != 0)
        goto cleanup;

    sum = mfloat_clone_prec(MF_ONE, precision);
    term = mfloat_clone_prec(MF_ONE, precision);
    if (!sum || !term || mf_div(sum, s) != 0 || mf_div(term, s) != 0)
        goto cleanup;
    tol = mfloat_clone_prec(MF_ONE, precision);
    if (!tol || mf_ldexp(tol, -(int)precision) != 0)
        goto cleanup;

    for (int n = 1; n < 2000; ++n) {
        nq = mfloat_new_from_long_prec(n, precision);
        spn = mf_clone(s);
        if (!nq || !spn || mf_add(spn, nq) != 0 || mf_mul(term, x) != 0 ||
            mf_div(term, spn) != 0 || mf_div(term, nq) != 0 || mf_add(sum, term) != 0)
            goto cleanup;
        if (mf_abs(term) != 0)
            goto cleanup;
        if (mf_le(term, tol))
            break;
        mf_free(nq);
        mf_free(spn);
        nq = spn = NULL;
    }

    if (mf_mul(pref, sum) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, pref);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(lgamma_s);
    mf_free(log_x);
    mf_free(pref);
    mf_free(sum);
    mf_free(term);
    mf_free(nq);
    mf_free(spn);
    mf_free(tol);
    return rc;
}

static int mfloat_gammainc_cf_Q(mfloat_t *dst,
                                const mfloat_t *s,
                                const mfloat_t *x,
                                size_t precision)
{
    mfloat_t *lgamma_s = NULL, *log_x = NULL, *pref = NULL;
    mfloat_t *tiny = NULL, *zero = NULL, *one = NULL;
    mfloat_t *C = NULL, *D = NULL, *f = NULL;
    mfloat_t *a = NULL, *b = NULL, *delta = NULL;
    mfloat_t *nq = NULL, *tmp = NULL, *smn = NULL;
    int rc = -1;

    lgamma_s = mfloat_clone_prec(s, precision);
    log_x = mfloat_clone_prec(x, precision);
    one = mfloat_clone_prec(MF_ONE, precision);
    zero = mfloat_clone_prec(MF_ZERO, precision);
    tiny = mf_new_prec(precision);
    if (tiny && (mf_set_long(tiny, 1) != 0 || mf_mul_pow10(tiny, -300) != 0)) {
        mf_free(tiny);
        tiny = NULL;
    }
    if (!lgamma_s || !log_x || !one || !zero || !tiny)
        goto cleanup;
    if (mf_lgamma(lgamma_s) != 0 || mf_log(log_x) != 0)
        goto cleanup;

    pref = mf_clone(log_x);
    if (!pref || mf_mul(pref, s) != 0 || mf_sub(pref, x) != 0 || mf_sub(pref, lgamma_s) != 0 ||
        mf_exp(pref) != 0)
        goto cleanup;

    C = mf_clone(tiny);
    D = mf_clone(zero);
    f = mfloat_clone_prec(MF_ONE, precision);
    if (!C || !D || !f)
        goto cleanup;

    for (int n = 1; n < 2000; ++n) {
        nq = mfloat_new_from_long_prec(n, precision);
        smn = mf_clone(s);
        a = mf_clone(nq);
        if (!nq || !smn || !a)
            goto cleanup;
        if (mf_sub(smn, nq) != 0 || mf_mul(a, smn) != 0)
            goto cleanup;

        b = mfloat_clone_prec(x, precision);
        tmp = mf_clone(nq);
        if (!b || !tmp || mf_mul_long(tmp, 2) != 0 || mf_add(b, tmp) != 0 || mf_sub(b, s) != 0)
            goto cleanup;

        if (mf_mul(a, D) != 0 || mf_add(a, b) != 0)
            goto cleanup;
        mf_free(D);
        D = a;
        a = NULL;
        if (mf_eq(D, zero) && mfloat_copy_value(D, tiny) != 0)
            goto cleanup;
        if (mf_inv(D) != 0)
            goto cleanup;

        a = mf_clone(nq);
        if (!a || mfloat_copy_value(a, smn) != 0)
            goto cleanup;
        if (mf_mul(a, nq) != 0 || mf_div(a, C) != 0 || mf_add(a, b) != 0)
            goto cleanup;
        mf_free(C);
        C = a;
        a = NULL;
        if (mf_eq(C, zero) && mfloat_copy_value(C, tiny) != 0)
            goto cleanup;

        delta = mf_clone(C);
        if (!delta || mf_mul(delta, D) != 0 || mf_mul(f, delta) != 0)
            goto cleanup;
        if (mf_sub(delta, one) != 0 || mf_abs(delta) != 0 || mf_le(delta, tiny)) {
            mf_free(a);
            mf_free(b);
            mf_free(delta);
            mf_free(nq);
            mf_free(tmp);
            mf_free(smn);
            a = b = delta = nq = tmp = smn = NULL;
            break;
        }

        mf_free(a);
        mf_free(b);
        mf_free(delta);
        mf_free(nq);
        mf_free(tmp);
        mf_free(smn);
        a = b = delta = nq = tmp = smn = NULL;
    }

    if (mf_mul(pref, f) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, pref);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(lgamma_s);
    mf_free(log_x);
    mf_free(pref);
    mf_free(tiny);
    mf_free(zero);
    mf_free(one);
    mf_free(C);
    mf_free(D);
    mf_free(f);
    mf_free(a);
    mf_free(b);
    mf_free(delta);
    mf_free(nq);
    mf_free(tmp);
    mf_free(smn);
    return rc;
}

static int mfloat_round_to_precision(mfloat_t *mfloat, size_t precision)
{
    return mfloat_round_to_precision_internal(mfloat, precision);
}

static int mfloat_is_below_neg_bits(const mfloat_t *mfloat, long bits)
{
    size_t mant_bits;
    long top_bit;

    if (!mfloat || mf_is_zero(mfloat))
        return 1;

    mant_bits = mf_get_mantissa_bits(mfloat);
    top_bit = mfloat->exponent2 + (long)mant_bits - 1l;
    return top_bit < -bits;
}

static long mfloat_estimate_positive_unit_steps(const mfloat_t *value, long threshold)
{
    double x, delta;
    long steps;

    if (!value || !mfloat_is_finite(value))
        return -1;
    x = mf_to_double(value);
    if (!isfinite(x) || x >= (double)threshold)
        return 0;
    delta = (double)threshold - x;
    steps = (long)delta;
    if ((double)steps < delta)
        steps++;
    return steps;
}

static int mfloat_compute_e(mfloat_t *dst, size_t precision)
{
    mfloat_t *sum = NULL, *term = NULL;
    long k;
    int rc = -1;
    size_t work_prec = precision + MFLOAT_CONST_GUARD_BITS;

    sum = mfloat_clone_prec(MF_ONE, work_prec);
    term = mfloat_clone_prec(MF_ONE, work_prec);
    if (!sum || !term)
        goto cleanup;

    for (k = 1; k < LONG_MAX; ++k) {
        if (mfloat_div_long_inplace(term, k) != 0)
            goto cleanup;
        if (mf_add(sum, term) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(term, (long)work_prec + 8l))
            break;
    }

    if (mfloat_round_to_precision(sum, precision) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(sum);
    mf_free(term);
    return rc;
}

static mint_t *mfloat_compute_arctan_inverse_scaled(long q, size_t bits)
{
    mint_t *sum = NULL, *term = NULL, *next = NULL;
    long q2;
    long k;

    if (q <= 1 || bits == 0 || q > 3037000499l)
        return NULL;
    q2 = q * q;

    term = mi_create_2pow((uint64_t)bits);
    if (!term)
        return NULL;
    if (mi_div_long(term, q, NULL) != 0)
        goto fail;

    sum = mi_clone(term);
    if (!sum)
        goto fail;

    for (k = 0; k < LONG_MAX / 2; ++k) {
        long num = 2l * k + 1l;
        long den = 2l * k + 3l;

        next = mi_clone(term);
        if (!next)
            goto fail;
        if (mi_abs(next) != 0)
            goto fail;
        if (mi_mul_long(next, num) != 0 ||
            mi_div_long(next, q2, NULL) != 0 ||
            mi_div_long(next, den, NULL) != 0)
            goto fail;
        if (mi_is_zero(next)) {
            mi_free(next);
            next = NULL;
            break;
        }
        if (!mi_is_negative(term)) {
            if (mi_neg(next) != 0)
                goto fail;
        }
        if (mi_add(sum, next) != 0)
            goto fail;
        mi_free(term);
        term = next;
        next = NULL;
    }

    mi_free(term);
    return sum;

fail:
    mi_free(sum);
    mi_free(term);
    mi_free(next);
    return NULL;
}

static int mfloat_compute_pi(mfloat_t *dst, size_t precision)
{
    mint_t *a = NULL, *b = NULL;
    long exponent2;
    size_t work_prec = precision + MFLOAT_CONST_GUARD_BITS;
    int rc = -1;

    a = mfloat_compute_arctan_inverse_scaled(5, work_prec);
    b = mfloat_compute_arctan_inverse_scaled(239, work_prec);
    if (!a || !b)
        goto cleanup;
    if (mi_mul_long(a, 16) != 0 || mi_mul_long(b, 4) != 0 || mi_sub(a, b) != 0)
        goto cleanup;
    exponent2 = -(long)work_prec;
    if (mfloat_set_from_signed_mint(dst, a, exponent2) != 0)
        goto cleanup;
    if (mfloat_round_to_precision(dst, precision) != 0)
        goto cleanup;
    dst->precision = precision;
    rc = 0;

cleanup:
    mi_free(a);
    mi_free(b);
    return rc;
}

static int mfloat_sin_kernel(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    mfloat_t *sum = NULL, *term = NULL, *r2 = NULL;
    long n;
    int rc = -1;

    sum = mfloat_clone_prec(x, precision);
    term = mfloat_clone_prec(x, precision);
    r2 = mfloat_clone_prec(x, precision);
    if (!sum || !term || !r2)
        goto cleanup;
    if (mf_mul(r2, x) != 0)
        goto cleanup;

    for (n = 1; n < LONG_MAX / 2; ++n) {
        long a = 2l * n;
        long b = 2l * n + 1l;

        if (mf_mul(term, r2) != 0)
            goto cleanup;
        if (mfloat_div_long_inplace(term, a) != 0 ||
            mfloat_div_long_inplace(term, b) != 0 ||
            mf_neg(term) != 0)
            goto cleanup;
        if (mf_add(sum, term) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(term, (long)precision + 8l))
            break;
    }

    if (mfloat_round_to_precision(sum, precision - MFLOAT_CONST_GUARD_BITS) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision - MFLOAT_CONST_GUARD_BITS;

cleanup:
    mf_free(sum);
    mf_free(term);
    mf_free(r2);
    return rc;
}

static int mfloat_cos_kernel(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    mfloat_t *sum = NULL, *term = NULL, *r2 = NULL;
    long n;
    int rc = -1;

    sum = mfloat_clone_prec(MF_ONE, precision);
    term = mfloat_clone_prec(MF_ONE, precision);
    r2 = mfloat_clone_prec(x, precision);
    if (!sum || !term || !r2)
        goto cleanup;
    if (mf_mul(r2, x) != 0)
        goto cleanup;

    for (n = 1; n < LONG_MAX / 2; ++n) {
        long a = 2l * n - 1l;
        long b = 2l * n;

        if (mf_mul(term, r2) != 0)
            goto cleanup;
        if (mfloat_div_long_inplace(term, a) != 0 ||
            mfloat_div_long_inplace(term, b) != 0 ||
            mf_neg(term) != 0)
            goto cleanup;
        if (mf_add(sum, term) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(term, (long)precision + 8l))
            break;
    }

    if (mfloat_round_to_precision(sum, precision - MFLOAT_CONST_GUARD_BITS) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision - MFLOAT_CONST_GUARD_BITS;

cleanup:
    mf_free(sum);
    mf_free(term);
    mf_free(r2);
    return rc;
}

static int mfloat_atan_kernel(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    mfloat_t *sum = NULL, *term = NULL, *r2 = NULL, *piece = NULL;
    long n;
    int rc = -1;

    sum = mfloat_clone_prec(x, precision);
    term = mfloat_clone_prec(x, precision);
    r2 = mfloat_clone_prec(x, precision);
    if (!sum || !term || !r2)
        goto cleanup;
    if (mf_mul(r2, x) != 0)
        goto cleanup;

    for (n = 1; n < LONG_MAX / 2; ++n) {
        long denom = 2l * n + 1l;

        if (mf_mul(term, r2) != 0 || mf_neg(term) != 0)
            goto cleanup;
        piece = mf_clone(term);
        if (!piece)
            goto cleanup;
        if (mfloat_div_long_inplace(piece, denom) != 0)
            goto cleanup;
        if (mf_add(sum, piece) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(piece, (long)precision + 8l))
            break;
        mf_free(piece);
        piece = NULL;
    }

    if (mfloat_round_to_precision(sum, precision - MFLOAT_CONST_GUARD_BITS) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision - MFLOAT_CONST_GUARD_BITS;

cleanup:
    mf_free(sum);
    mf_free(term);
    mf_free(r2);
    mf_free(piece);
    return rc;
}

static int mfloat_reduce_trig_argument(const mfloat_t *src,
                                       size_t precision,
                                       mfloat_t **r_out,
                                       int *quadrant_out)
{
    mfloat_t *r = NULL, *half_pi = NULL, *kterm = NULL;
    double xd, half_pid;
    long k;

    if (!src || !r_out || !quadrant_out)
        return -1;

    r = mfloat_clone_prec(src, precision);
    half_pi = mfloat_new_pi_prec(precision);
    if (!r || !half_pi)
        goto fail;
    if (mfloat_div_long_inplace(half_pi, 2) != 0)
        goto fail;

    xd = mf_to_double(r);
    half_pid = mf_to_double(half_pi);
    k = (long)nearbyint(xd / half_pid);

    kterm = mf_clone(half_pi);
    if (!kterm)
        goto fail;
    if (mf_mul_long(kterm, k) != 0)
        goto fail;
    if (mf_sub(r, kterm) != 0)
        goto fail;

    *quadrant_out = (int)(((k % 4l) + 4l) % 4l);
    *r_out = r;
    mf_free(half_pi);
    mf_free(kterm);
    return 0;

fail:
    mf_free(r);
    mf_free(half_pi);
    mf_free(kterm);
    return -1;
}

static int mfloat_finish_result(mfloat_t *dst, mfloat_t *src, size_t precision)
{
    int rc;

    if (!dst || !src)
        return -1;
    if (mfloat_round_to_precision(src, precision) != 0)
        return -1;
    rc = mfloat_copy_value(dst, src);
    if (rc == 0)
        dst->precision = precision;
    return rc;
}

static size_t mfloat_transcendental_work_prec(size_t precision)
{
    size_t work_prec = precision + 64u;

    if (work_prec < precision + MFLOAT_CONST_GUARD_BITS)
        work_prec = precision + MFLOAT_CONST_GUARD_BITS;
    return work_prec;
}

static int mfloat_get_small_positive_integer(const mfloat_t *mfloat, long *out)
{
    double value;
    mfloat_t *check = NULL;
    int ok = 0;

    if (!mfloat || !out || !mfloat_is_finite(mfloat) || mfloat->sign <= 0)
        return 0;
    value = mf_to_double(mfloat);
    if (!(value >= 1.0 && value <= (double)LONG_MAX) || floor(value) != value)
        return 0;
    check = mf_create_long((long)value);
    if (!check)
        return 0;
    ok = mf_eq(mfloat, check);
    if (ok)
        *out = (long)value;
    mf_free(check);
    return ok;
}

static int mfloat_get_small_positive_half_integer(const mfloat_t *mfloat, long *out_n)
{
    mfloat_t *twice = NULL;
    long twice_n = 0;
    int ok = 0;

    if (!mfloat || !out_n || !mfloat_is_finite(mfloat) || mfloat->sign <= 0)
        return 0;
    twice = mfloat_clone_prec(mfloat, mfloat->precision);
    if (!twice)
        return 0;
    if (mf_mul_long(twice, 2) != 0)
        goto cleanup;
    if (!mfloat_get_small_positive_integer(twice, &twice_n) || (twice_n & 1L) == 0 || twice_n < 1)
        goto cleanup;
    *out_n = (twice_n - 1) / 2;
    ok = 1;

cleanup:
    mf_free(twice);
    return ok;
}

static int mfloat_get_small_positive_half_step(const mfloat_t *mfloat, long *out_twice_n)
{
    mfloat_t *twice = NULL;
    long twice_n = 0;
    int ok = 0;

    if (!mfloat || !out_twice_n || !mfloat_is_finite(mfloat) || mfloat->sign <= 0)
        return 0;
    twice = mfloat_clone_prec(mfloat, mfloat->precision);
    if (!twice)
        return 0;
    if (mf_mul_long(twice, 2) != 0)
        goto cleanup;
    if (!mfloat_get_small_positive_integer(twice, &twice_n) || twice_n < 1)
        goto cleanup;
    *out_twice_n = twice_n;
    ok = 1;

cleanup:
    mf_free(twice);
    return ok;
}

static int mfloat_build_small_halfstep_gamma_parts(long twice_n, size_t precision,
                                                   mfloat_t *num, mfloat_t *den, int *pi_half)
{
    long m;

    if (twice_n < 1 || !num || !den || !pi_half)
        return -1;
    if ((twice_n & 1L) == 0) {
        m = twice_n / 2;
        *pi_half = 0;
        if (m <= 1)
            return 0;
        return mfloat_set_factorial(num, m - 1, precision);
    }

    m = (twice_n - 1) / 2;
    *pi_half = 1;
    if (m == 0)
        return 0;
    if (mfloat_set_factorial(num, 2 * m, precision) != 0 || mfloat_set_factorial(den, m, precision) != 0)
        return -1;
    return mf_ldexp(den, 2 * (int)m);
}

static int mfloat_try_small_exact_beta(mfloat_t *dst, const mfloat_t *a, const mfloat_t *b, size_t precision)
{
    long twice_a, twice_b, twice_sum;
    int pi_half_a, pi_half_b, pi_half_sum, pi_factor;
    mfloat_t *num_a = NULL, *den_a = NULL, *num_b = NULL, *den_b = NULL;
    mfloat_t *num_sum = NULL, *den_sum = NULL, *value = NULL, *pi = NULL;
    int rc = -1;

    if (!mfloat_get_small_positive_half_step(a, &twice_a) || !mfloat_get_small_positive_half_step(b, &twice_b))
        return 1;
    if (twice_a > LONG_MAX - twice_b)
        return 1;
    twice_sum = twice_a + twice_b;

    num_a = mfloat_clone_prec(MF_ONE, precision);
    den_a = mfloat_clone_prec(MF_ONE, precision);
    num_b = mfloat_clone_prec(MF_ONE, precision);
    den_b = mfloat_clone_prec(MF_ONE, precision);
    num_sum = mfloat_clone_prec(MF_ONE, precision);
    den_sum = mfloat_clone_prec(MF_ONE, precision);
    value = mfloat_clone_prec(MF_ONE, precision);
    if (!num_a || !den_a || !num_b || !den_b || !num_sum || !den_sum || !value)
        goto cleanup;
    if (mfloat_build_small_halfstep_gamma_parts(twice_a, precision, num_a, den_a, &pi_half_a) != 0 ||
        mfloat_build_small_halfstep_gamma_parts(twice_b, precision, num_b, den_b, &pi_half_b) != 0 ||
        mfloat_build_small_halfstep_gamma_parts(twice_sum, precision, num_sum, den_sum, &pi_half_sum) != 0)
        goto cleanup;

    if (mf_mul(value, num_a) != 0 || mf_mul(value, num_b) != 0 || mf_mul(value, den_sum) != 0 ||
        mf_div(value, den_a) != 0 || mf_div(value, den_b) != 0 || mf_div(value, num_sum) != 0)
        goto cleanup;

    pi_factor = pi_half_a + pi_half_b - pi_half_sum;
    if (pi_factor == 2) {
        pi = mfloat_new_pi_prec(precision);
        if (!pi || mf_mul(value, pi) != 0)
            goto cleanup;
    } else if (pi_factor != 0) {
        goto cleanup;
    }

    rc = mfloat_finish_result(dst, value, precision);

cleanup:
    mf_free(num_a);
    mf_free(den_a);
    mf_free(num_b);
    mf_free(den_b);
    mf_free(num_sum);
    mf_free(den_sum);
    mf_free(value);
    mf_free(pi);
    return rc;
}

static int mfloat_compute_euler_gamma(mfloat_t *dst, size_t precision)
{
    static const unsigned powers[] = {
        2u, 4u, 6u, 8u, 10u, 12u, 14u, 16u,
        18u, 20u, 22u, 24u, 26u, 28u, 30u, 32u
    };
    mfloat_t *sum = NULL, *term = NULL, *ln2 = NULL, *tmp = NULL, *corr = NULL;
    size_t work_prec = precision + MFLOAT_CONST_GUARD_BITS;
    unsigned shift_bits = (unsigned)((precision + 47u) / 34u);
    unsigned long n, k;
    int rc = -1;

    if (shift_bits < MFLOAT_GAMMA_MIN_SHIFT)
        shift_bits = MFLOAT_GAMMA_MIN_SHIFT;
    if (shift_bits > MFLOAT_GAMMA_MAX_SHIFT)
        shift_bits = MFLOAT_GAMMA_MAX_SHIFT;
    n = 1ul << shift_bits;

    sum = mfloat_clone_prec(MF_ZERO, work_prec);
    term = mfloat_clone_prec(MF_ONE, work_prec);
    ln2 = mfloat_clone_immortal_prec_internal(MFLOAT_INTERNAL_LN2, work_prec);
    if (!sum || !term || !ln2)
        goto cleanup;

    for (k = 1; k <= n; ++k) {
        if (mfloat_copy_value(term, MF_ONE) != 0 || mf_set_precision(term, work_prec) != 0 ||
            mfloat_div_long_inplace(term, (long)k) != 0)
            goto cleanup;
        if (mf_add(sum, term) != 0)
            goto cleanup;
    }

    tmp = mf_clone(ln2);
    if (!tmp || mfloat_mul_long_inplace(tmp, (long)shift_bits) != 0)
        goto cleanup;
    if (mf_sub(sum, tmp) != 0)
        goto cleanup;
    mf_free(tmp);
    tmp = NULL;

    tmp = mfloat_clone_prec(MF_ONE, work_prec);
    if (!tmp)
        goto cleanup;
    if (mf_ldexp(tmp, -((int)shift_bits + 1)) != 0)
        goto cleanup;
    if (mf_sub(sum, tmp) != 0)
        goto cleanup;
    mf_free(tmp);
    tmp = NULL;

    for (k = 0; k < sizeof(powers) / sizeof(powers[0]); ++k) {
        corr = mfloat_clone_prec(MF_ONE, work_prec);
        if (!corr)
            goto cleanup;
        if (mf_ldexp(corr, -(int)(shift_bits * powers[k])) != 0)
            goto cleanup;
        if (mfloat_mul_euler_gamma_coeff_internal(corr, k, work_prec) != 0)
            goto cleanup;
        if (mf_add(sum, corr) != 0)
            goto cleanup;
        mf_free(corr);
        corr = NULL;
    }

    if (mfloat_round_to_precision(sum, precision) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(sum);
    mf_free(term);
    mf_free(ln2);
    mf_free(tmp);
    mf_free(corr);
    return rc;
}

mfloat_t *mf_pi(void)
{
    size_t precision = mfloat_get_default_precision_internal();
    mfloat_t *tmp = mf_new_prec(precision);

    if (!tmp)
        return NULL;
    if ((precision <= MF_PI->precision
            ? mfloat_set_from_binary_seed(tmp, MF_PI, precision)
            : mfloat_compute_pi(tmp, precision)) != 0) {
        mf_free(tmp);
        return NULL;
    }
    return tmp;
}

mfloat_t *mf_e(void)
{
    size_t precision = mfloat_get_default_precision_internal();
    mfloat_t *tmp = mf_new_prec(precision);

    if (!tmp)
        return NULL;
    if ((precision <= MF_E->precision
            ? mfloat_set_from_binary_seed(tmp, MF_E, precision)
            : mfloat_compute_e(tmp, precision)) != 0) {
        mf_free(tmp);
        return NULL;
    }
    return tmp;
}

mfloat_t *mf_euler_mascheroni(void)
{
    size_t precision = mfloat_get_default_precision_internal();
    mfloat_t *tmp = mf_new_prec(precision);

    if (!tmp)
        return NULL;
    if ((precision <= MF_EULER_MASCHERONI->precision
            ? mfloat_set_from_binary_seed(tmp, MF_EULER_MASCHERONI, precision)
            : mfloat_compute_euler_gamma(tmp, precision)) != 0) {
        mf_free(tmp);
        return NULL;
    }
    return tmp;
}

mfloat_t *mf_max(void)
{
    return mf_create_double(DBL_MAX);
}

static int mfloat_store_qfloat_result(mfloat_t *mfloat, qfloat_t value)
{
    size_t precision;

    if (!mfloat)
        return -1;
    precision = mfloat->precision;
    if (mf_set_qfloat(mfloat, value) != 0)
        return -1;
    return mf_set_precision(mfloat, precision);
}

static int mfloat_apply_qfloat_unary(mfloat_t *mfloat, qfloat_t (*fn)(qfloat_t))
{
    qfloat_t x;

    if (!mfloat || !fn)
        return -1;
    x = mf_to_qfloat(mfloat);
    return mfloat_store_qfloat_result(mfloat, fn(x));
}

static int mfloat_apply_qfloat_binary(mfloat_t *mfloat,
                                      const mfloat_t *other,
                                      qfloat_t (*fn)(qfloat_t, qfloat_t))
{
    qfloat_t x, y;

    if (!mfloat || !other || !fn)
        return -1;
    x = mf_to_qfloat(mfloat);
    y = mf_to_qfloat(other);
    return mfloat_store_qfloat_result(mfloat, fn(x, y));
}

static int mfloat_apply_qfloat_ternary(mfloat_t *mfloat,
                                       const mfloat_t *a,
                                       const mfloat_t *b,
                                       qfloat_t (*fn)(qfloat_t, qfloat_t, qfloat_t))
{
    qfloat_t x, y, z;

    if (!mfloat || !a || !b || !fn)
        return -1;
    x = mf_to_qfloat(mfloat);
    y = mf_to_qfloat(a);
    z = mf_to_qfloat(b);
    return mfloat_store_qfloat_result(mfloat, fn(x, y, z));
}

static mfloat_t *mfloat_clone_prec(const mfloat_t *src, size_t precision)
{
    mfloat_t *copy;

    if (!src)
        return NULL;
    copy = mf_new_prec(precision);
    if (!copy)
        return NULL;
    if (mfloat_copy_value(copy, src) != 0) {
        mf_free(copy);
        return NULL;
    }
    copy->precision = precision;
    return copy;
}

int mf_exp(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *ln2 = NULL, *kln2 = NULL, *r = NULL, *sum = NULL, *term = NULL;
    double xd, ln2d;
    long k;
    int squarings = 0;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_NEGINF) {
        mf_clear(mfloat);
        return 0;
    }
    if (mf_is_zero(mfloat)) {
        precision = mfloat->precision;
        if (mfloat_copy_value(mfloat, MF_ONE) != 0)
            return -1;
        mfloat->precision = precision;
        return 0;
    }

    precision = mfloat->precision;
    work_prec = precision + (precision / 2u);
    if (work_prec < precision + 96u)
        work_prec = precision + 96u;
    x = mfloat_clone_prec(mfloat, work_prec);
    ln2 = mfloat_clone_immortal_prec_internal(MFLOAT_INTERNAL_LN2, work_prec);
    if (!x || !ln2)
        goto cleanup;

    xd = mf_to_double(x);
    if (isnan(xd)) {
        rc = mf_set_double(mfloat, NAN);
        goto cleanup;
    }
    if (isinf(xd)) {
        rc = mf_set_double(mfloat, xd > 0.0 ? INFINITY : 0.0);
        goto cleanup;
    }

    ln2d = mf_to_double(ln2);
    k = (long)nearbyint(xd / ln2d);

    kln2 = mf_clone(ln2);
    if (!kln2)
        goto cleanup;
    if (mf_mul_long(kln2, k) != 0)
        goto cleanup;

    r = mf_clone(x);
    if (!r || mf_sub(r, kln2) != 0)
        goto cleanup;

    xd = fabs(mf_to_double(r));
    while (xd > 0.125 && squarings < 8) {
        if (mf_ldexp(r, -1) != 0)
            goto cleanup;
        xd *= 0.5;
        squarings++;
    }

    sum = mfloat_clone_prec(MF_ONE, work_prec);
    term = mfloat_clone_prec(MF_ONE, work_prec);
    if (!sum || !term)
        goto cleanup;

    for (long n = 1; n < LONG_MAX; ++n) {
        if (mf_mul(term, r) != 0)
            goto cleanup;
        if (mfloat_div_long_inplace(term, n) != 0)
            goto cleanup;
        if (mf_add(sum, term) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(term, (long)work_prec + 8l))
            break;
    }

    for (int i = 0; i < squarings; ++i) {
        if (mf_mul(sum, sum) != 0)
            goto cleanup;
    }

    if (mf_ldexp(sum, (int)k) != 0)
        goto cleanup;
    if (mfloat_round_to_precision(sum, precision) != 0)
        goto cleanup;
    rc = mfloat_copy_value(mfloat, sum);
    if (rc == 0)
        mfloat->precision = precision;

cleanup:
    mf_free(x);
    mf_free(ln2);
    mf_free(kln2);
    mf_free(r);
    mf_free(sum);
    mf_free(term);
    return rc;
}

int mf_log(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    size_t mant_bits;
    long exp2;
    mint_t *mant = NULL;
    mfloat_scratch_slot_t slots[7];
    mfloat_t *m, *u, *u2, *y, *term, *piece, *ln2;
    double md;
    int rc = -1;
    size_t i;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_NEGINF || mfloat->sign < 0)
        return mf_set_double(mfloat, NAN);
    if (mf_is_zero(mfloat))
        return mf_set_double(mfloat, -INFINITY);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_log);
    work_prec = precision + (precision / 2u);
    if (work_prec < precision + 96u)
        work_prec = precision + 96u;
    for (i = 0; i < 7u; ++i)
        mfloat_scratch_init_slot(&slots[i], work_prec);
    m = &slots[0].value;
    u = &slots[1].value;
    u2 = &slots[2].value;
    y = &slots[3].value;
    term = &slots[4].value;
    piece = &slots[5].value;
    ln2 = &slots[6].value;
    mant_bits = mi_bit_length(mfloat->mantissa);
    exp2 = mfloat->exponent2 + (long)mant_bits - 1l;
    mant = mi_clone(mfloat->mantissa);
    if (!mant)
        goto cleanup;
    if (mfloat_set_from_signed_mint(m, mant, -((long)mant_bits - 1l)) != 0)
        goto cleanup;

    md = mf_to_double(m);
    if (!(md > 0.0))
        goto cleanup;
    if (mfloat_scratch_copy(u, m) != 0 || mfloat_scratch_copy(term, m) != 0)
        goto cleanup;
    if (mf_add_long(u, -1) != 0)
        goto cleanup;
    if (mf_add_long(term, 1) != 0)
        goto cleanup;
    if (mf_div(u, term) != 0)
        goto cleanup;
    mfloat_scratch_reset_slot(&slots[4], work_prec);

    if (mfloat_scratch_copy(y, u) != 0 || mfloat_scratch_copy(term, u) != 0 ||
        mfloat_scratch_copy(u2, u) != 0)
        goto cleanup;
    if (mf_mul(u2, u) != 0)
        goto cleanup;

    for (long k = 1; k < LONG_MAX / 2; ++k) {
        long denom = 2l * k + 1l;

        if (mf_mul(term, u2) != 0)
            goto cleanup;
        if (mfloat_scratch_copy(piece, term) != 0)
            goto cleanup;
        if (mfloat_div_long_inplace(piece, denom) != 0)
            goto cleanup;
        if (mf_add(y, piece) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(piece, (long)work_prec + 8l))
            break;
    }

    if (mf_mul_long(y, 2) != 0)
        goto cleanup;

    mfloat_scratch_reset_slot(&slots[0], work_prec);

    if (exp2 != 0) {
        if (mfloat_scratch_copy(ln2, MFLOAT_INTERNAL_LN2) != 0)
            goto cleanup;
        if (mf_mul_long(ln2, exp2) != 0)
            goto cleanup;
        if (mf_add(y, ln2) != 0)
            goto cleanup;
    }

    if (mfloat_round_to_precision(y, precision) != 0)
        goto cleanup;
    rc = mfloat_copy_value(mfloat, y);
    if (rc == 0)
        mfloat->precision = precision;

cleanup:
    mi_free(mant);
    for (i = 0; i < 7u; ++i)
        mfloat_scratch_release_slot(&slots[i]);
    return rc;
}

int mf_pow(mfloat_t *mfloat, const mfloat_t *exponent)
{
    mfloat_t *tmp = NULL;
    long exp_long = 0;
    double ed;

    if (!mfloat || !exponent)
        return -1;

    if (mfloat_get_exact_long_value(exponent, &exp_long)) {
        if (exp_long >= INT_MIN && exp_long <= INT_MAX)
            return mf_pow_int(mfloat, (int)exp_long);
    }

    if (mfloat->sign <= 0)
        return mfloat_apply_qfloat_binary(mfloat, exponent, qf_pow);

    tmp = mfloat_clone_prec(exponent, mfloat->precision + MFLOAT_CONST_GUARD_BITS);
    if (!tmp)
        return -1;
    if (mf_log(mfloat) != 0) {
        mf_free(tmp);
        return -1;
    }
    if (mf_mul(mfloat, tmp) != 0) {
        mf_free(tmp);
        return -1;
    }
    ed = mf_to_double(mfloat);
    if (isnan(ed) || isinf(ed)) {
        mf_free(tmp);
        return mf_set_double(mfloat, ed);
    }
    mf_free(tmp);
    return mf_exp(mfloat);
}

mfloat_t *mf_pow10(int exponent10)
{
    qfloat_t q = qf_pow10(exponent10);

    return mf_create_qfloat(q);
}

int mf_sqr(mfloat_t *mfloat)
{
    mfloat_t *tmp;
    int rc;

    if (!mfloat)
        return -1;
    tmp = mf_clone(mfloat);
    if (!tmp)
        return -1;
    rc = mf_mul(mfloat, tmp);
    mf_free(tmp);
    return rc;
}

int mf_floor(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_floor);
}

int mf_mul_pow10(mfloat_t *mfloat, int exponent10)
{
    qfloat_t x;

    if (!mfloat)
        return -1;
    x = mf_to_qfloat(mfloat);
    return mfloat_store_qfloat_result(mfloat, qf_mul_pow10(x, exponent10));
}

int mf_hypot(mfloat_t *mfloat, const mfloat_t *other)
{
    return mfloat_apply_qfloat_binary(mfloat, other, qf_hypot);
}

int mf_sin(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *r = NULL;
    int quadrant;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);
    if (mf_is_zero(mfloat))
        return 0;

    precision = mfloat->precision;
    work_prec = mfloat_transcendental_work_prec(precision);
    if (mfloat_reduce_trig_argument(mfloat, work_prec, &r, &quadrant) != 0)
        goto cleanup;

    switch (quadrant) {
    case 0:
        rc = mfloat_sin_kernel(mfloat, r, work_prec);
        break;
    case 1:
        rc = mfloat_cos_kernel(mfloat, r, work_prec);
        break;
    case 2:
        rc = mfloat_sin_kernel(mfloat, r, work_prec);
        if (rc == 0)
            rc = mf_neg(mfloat);
        break;
    default:
        rc = mfloat_cos_kernel(mfloat, r, work_prec);
        if (rc == 0)
            rc = mf_neg(mfloat);
        break;
    }
    if (rc == 0)
        rc = mfloat_round_to_precision(mfloat, precision);

cleanup:
    mf_free(r);
    return rc;
}

int mf_cos(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *r = NULL;
    int quadrant;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    work_prec = mfloat_transcendental_work_prec(precision) + 128u;
    if (mfloat_reduce_trig_argument(mfloat, work_prec, &r, &quadrant) != 0)
        goto cleanup;

    switch (quadrant) {
    case 0:
        rc = mfloat_cos_kernel(mfloat, r, work_prec);
        break;
    case 1:
        rc = mfloat_sin_kernel(mfloat, r, work_prec);
        if (rc == 0)
            rc = mf_neg(mfloat);
        break;
    case 2:
        rc = mfloat_cos_kernel(mfloat, r, work_prec);
        if (rc == 0)
            rc = mf_neg(mfloat);
        break;
    default:
        rc = mfloat_sin_kernel(mfloat, r, work_prec);
        break;
    }
    if (rc == 0)
        rc = mfloat_round_to_precision(mfloat, precision);

cleanup:
    mf_free(r);
    return rc;
}

int mf_tan(mfloat_t *mfloat)
{
    mfloat_t *c = NULL;
    int rc;

    if (!mfloat)
        return -1;
    c = mf_clone(mfloat);
    if (!c)
        return -1;
    if (mf_sin(mfloat) != 0) {
        mf_free(c);
        return -1;
    }
    rc = mf_cos(c);
    if (rc == 0)
        rc = mf_div(mfloat, c);
    mf_free(c);
    return rc;
}

int mf_atan(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *r = NULL, *pi = NULL, *half_pi = NULL, *quarter_pi = NULL;
    double xd;
    int negate = 0;
    int add_quarter_pi = 0;
    int subtract_from_half_pi = 0;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF || mfloat->kind == MFLOAT_KIND_NEGINF) {
        pi = mfloat_new_pi_prec(mfloat->precision + MFLOAT_CONST_GUARD_BITS);
        if (!pi || mfloat_div_long_inplace(pi, 2) != 0) {
            mf_free(pi);
            return -1;
        }
        if (mfloat->kind == MFLOAT_KIND_NEGINF && mf_neg(pi) != 0) {
            mf_free(pi);
            return -1;
        }
        rc = mfloat_copy_value(mfloat, pi);
        mf_free(pi);
        return rc;
    }
    if (mf_is_zero(mfloat))
        return 0;

    precision = mfloat->precision;
    work_prec = precision + MFLOAT_CONST_GUARD_BITS;
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;
    if (x->sign < 0) {
        negate = 1;
        if (mf_abs(x) != 0)
            goto cleanup;
    }

    xd = fabs(mf_to_double(x));
    r = mf_clone(x);
    if (!r)
        goto cleanup;

    if (xd > 1.0) {
        if (mf_inv(r) != 0)
            goto cleanup;
        subtract_from_half_pi = 1;
    } else if (xd > 0.4142135623730951) {
        mfloat_t *num = mf_clone(r);
        mfloat_t *den = mf_clone(r);

        if (!num || !den) {
            mf_free(num);
            mf_free(den);
            goto cleanup;
        }
        if (mf_add_long(num, -1) != 0 ||
            mf_add_long(den, 1) != 0 ||
            mf_div(num, den) != 0) {
            mf_free(num);
            mf_free(den);
            goto cleanup;
        }
        mf_free(den);
        mf_free(r);
        r = num;
        add_quarter_pi = 1;
    }

    if (mfloat_atan_kernel(mfloat, r, work_prec) != 0)
        goto cleanup;

    if (add_quarter_pi || subtract_from_half_pi) {
        pi = mfloat_new_pi_prec(work_prec);
        if (!pi)
            goto cleanup;
        if (add_quarter_pi) {
            quarter_pi = mf_clone(pi);
            if (!quarter_pi || mfloat_div_long_inplace(quarter_pi, 4) != 0)
                goto cleanup;
            if (mf_add(mfloat, quarter_pi) != 0)
                goto cleanup;
        } else {
            half_pi = mf_clone(pi);
            if (!half_pi || mfloat_div_long_inplace(half_pi, 2) != 0)
                goto cleanup;
            if (mf_sub(half_pi, mfloat) != 0)
                goto cleanup;
            if (mfloat_copy_value(mfloat, half_pi) != 0)
                goto cleanup;
            mfloat->precision = work_prec;
        }
    }

    if (negate && mf_neg(mfloat) != 0)
        goto cleanup;
    if (mfloat_round_to_precision(mfloat, precision) != 0)
        goto cleanup;
    mfloat->precision = precision;
    rc = 0;

cleanup:
    mf_free(x);
    mf_free(r);
    mf_free(pi);
    mf_free(half_pi);
    mf_free(quarter_pi);
    return rc;
}

int mf_atan2(mfloat_t *mfloat, const mfloat_t *other)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *y = NULL, *pi = NULL, *absx = NULL, *absy = NULL;
    long xv = 0, yv = 0;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    if (!mfloat_is_finite(mfloat) || !mfloat_is_finite(other))
        return mfloat_apply_qfloat_binary(mfloat, other, qf_atan2);
    if (mfloat_equals_exact_long(mfloat, 1) && mfloat_equals_exact_long(other, -1)) {
        precision = mfloat->precision;
        work_prec = mfloat_transcendental_work_prec(precision);
        pi = mfloat_new_pi_prec(work_prec);
        if (!pi || mfloat_div_long_inplace(pi, 4) != 0 || mf_mul_long(pi, 3) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, pi, precision);
        goto cleanup;
    }
    if (mfloat_equals_exact_long(mfloat, -1) && mfloat_equals_exact_long(other, -1)) {
        precision = mfloat->precision;
        work_prec = mfloat_transcendental_work_prec(precision);
        pi = mfloat_new_pi_prec(work_prec);
        if (!pi || mfloat_div_long_inplace(pi, 4) != 0 || mf_mul_long(pi, -3) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, pi, precision);
        goto cleanup;
    }
    if (mfloat_get_exact_long_value(mfloat, &yv) &&
        mfloat_get_exact_long_value(other, &xv) &&
        (yv == 1 || yv == -1) && (xv == 1 || xv == -1) && labs(yv) == labs(xv)) {
        precision = mfloat->precision;
        work_prec = mfloat_transcendental_work_prec(precision);
        pi = mfloat_new_pi_prec(work_prec);
        if (!pi || mfloat_div_long_inplace(pi, 4) != 0)
            goto cleanup;
        if (xv < 0) {
            if (yv > 0) {
                if (mf_mul_long(pi, 3) != 0)
                    goto cleanup;
            } else {
                if (mf_mul_long(pi, -3) != 0)
                    goto cleanup;
            }
        } else if (yv < 0) {
            if (mf_neg(pi) != 0)
                goto cleanup;
        }
        rc = mfloat_finish_result(mfloat, pi, precision);
        goto cleanup;
    }

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_binary(mfloat, other, qf_atan2);
    work_prec = mfloat_transcendental_work_prec(precision) + 384u;

    y = mfloat_clone_prec(mfloat, work_prec);
    x = mfloat_clone_prec(other, work_prec);
    absx = mf_clone(x);
    absy = mf_clone(y);
    if (!x || !y || !absx || !absy)
        goto cleanup;
    if (mf_abs(absx) != 0 || mf_abs(absy) != 0)
        goto cleanup;

    if (mf_eq(absx, absy)) {
        pi = mfloat_new_pi_prec(work_prec);
        if (!pi || mfloat_div_long_inplace(pi, 4) != 0)
            goto cleanup;
        if (x->sign < 0) {
            if (y->sign >= 0) {
                if (mf_mul_long(pi, 3) != 0)
                    goto cleanup;
            } else {
                if (mf_mul_long(pi, -3) != 0)
                    goto cleanup;
            }
        } else if (y->sign < 0) {
            if (mf_neg(pi) != 0)
                goto cleanup;
        }
        rc = mfloat_finish_result(mfloat, pi, precision);
        goto cleanup;
    }

    if (mf_is_zero(x)) {
        if (mf_is_zero(y))
            return mf_set_double(mfloat, NAN);
        pi = mfloat_new_pi_prec(work_prec);
        if (!pi || mfloat_div_long_inplace(pi, 2) != 0)
            goto cleanup;
        if (y->sign < 0 && mf_neg(pi) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, pi, precision);
        goto cleanup;
    }

    if (mf_div(y, x) != 0 || mf_atan(y) != 0)
        goto cleanup;

    if (x->sign < 0) {
        pi = mfloat_new_pi_prec(work_prec);
        if (!pi)
            goto cleanup;
        if (y->sign >= 0) {
            if (mf_add(y, pi) != 0)
                goto cleanup;
        } else {
            if (mf_sub(y, pi) != 0)
                goto cleanup;
        }
    }

    rc = mfloat_finish_result(mfloat, y, precision);

cleanup:
    mf_free(x);
    mf_free(y);
    mf_free(pi);
    mf_free(absx);
    mf_free(absy);
    return rc;
}

int mf_asin(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *one = NULL, *y = NULL, *sin_y = NULL, *cos_y = NULL, *delta = NULL;
    int negate = 0;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_asin);
    work_prec = precision + MFLOAT_CONST_GUARD_BITS;

    x = mfloat_clone_prec(mfloat, work_prec);
    one = mfloat_clone_prec(MF_ONE, work_prec);
    if (!x || !one)
        goto cleanup;

    if (x->sign < 0) {
        negate = 1;
        if (mf_abs(x) != 0)
            goto cleanup;
    }
    if (mf_gt(x, one)) {
        rc = mf_set_double(mfloat, NAN);
        goto cleanup;
    }
    if (mf_eq(x, one)) {
        mfloat_t *pi = mfloat_new_pi_prec(work_prec);
        if (!pi || mfloat_div_long_inplace(pi, 2) != 0) {
            mf_free(pi);
            goto cleanup;
        }
        if (negate && mf_neg(pi) != 0) {
            mf_free(pi);
            goto cleanup;
        }
        rc = mfloat_finish_result(mfloat, pi, precision);
        mf_free(pi);
        goto cleanup;
    }
    y = mfloat_new_from_qfloat_prec(qf_asin(mf_to_qfloat(x)), work_prec);
    if (!y)
        goto cleanup;

    for (int iter = 0; iter < 8; ++iter) {
        sin_y = mf_clone(y);
        cos_y = mf_clone(y);
        if (!sin_y || !cos_y)
            goto cleanup;
        if (mf_sin(sin_y) != 0 || mf_cos(cos_y) != 0)
            goto cleanup;
        if (mf_sub(sin_y, x) != 0 || mf_div(sin_y, cos_y) != 0)
            goto cleanup;
        delta = sin_y;
        sin_y = NULL;
        if (mf_sub(y, delta) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(delta, (long)work_prec + 8l))
            break;
        mf_free(delta);
        delta = NULL;
        mf_free(cos_y);
        cos_y = NULL;
    }

    if (negate && mf_neg(y) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, y, precision);

cleanup:
    mf_free(x);
    mf_free(one);
    mf_free(y);
    mf_free(sin_y);
    mf_free(cos_y);
    mf_free(delta);
    return rc;
}

int mf_acos(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *tmp = NULL, *half_pi = NULL, *pi = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_acos);

    work_prec = mfloat_transcendental_work_prec(precision);
    tmp = mfloat_clone_prec(mfloat, work_prec);
    pi = mfloat_new_pi_prec(work_prec);
    if (!tmp || !pi)
        goto cleanup;
    if (mf_asin(tmp) != 0 || mfloat_div_long_inplace(pi, 2) != 0)
        goto cleanup;
    half_pi = pi;
    pi = NULL;
    if (mf_sub(half_pi, tmp) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, half_pi, precision);

cleanup:
    mf_free(tmp);
    mf_free(half_pi);
    mf_free(pi);
    return rc;
}

int mf_sinh(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *ex = NULL, *emx = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF || mfloat->kind == MFLOAT_KIND_NEGINF)
        return 0;

    precision = mfloat->precision;
    work_prec = mfloat_transcendental_work_prec(precision);

    ex = mfloat_clone_prec(mfloat, work_prec);
    emx = mfloat_clone_prec(mfloat, work_prec);
    if (!ex || !emx)
        goto cleanup;
    if (mf_exp(ex) != 0 || mf_neg(emx) != 0 || mf_exp(emx) != 0)
        goto cleanup;
    if (mf_sub(ex, emx) != 0 || mfloat_div_long_inplace(ex, 2) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, ex, precision);

cleanup:
    mf_free(ex);
    mf_free(emx);
    return rc;
}

int mf_cosh(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *ex = NULL, *emx = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF || mfloat->kind == MFLOAT_KIND_NEGINF)
        return mf_set_double(mfloat, INFINITY);

    precision = mfloat->precision;
    work_prec = mfloat_transcendental_work_prec(precision);

    ex = mfloat_clone_prec(mfloat, work_prec);
    emx = mfloat_clone_prec(mfloat, work_prec);
    if (!ex || !emx)
        goto cleanup;
    if (mf_exp(ex) != 0 || mf_neg(emx) != 0 || mf_exp(emx) != 0)
        goto cleanup;
    if (mf_add(ex, emx) != 0 || mfloat_div_long_inplace(ex, 2) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, ex, precision);

cleanup:
    mf_free(ex);
    mf_free(emx);
    return rc;
}

int mf_tanh(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *twox = NULL, *num = NULL, *den = NULL;
    int negate = 0;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return mfloat_copy_value(mfloat, MF_ONE);
    if (mfloat->kind == MFLOAT_KIND_NEGINF) {
        rc = mfloat_copy_value(mfloat, MF_ONE);
        if (rc == 0)
            rc = mf_neg(mfloat);
        return rc;
    }

    precision = mfloat->precision;
    work_prec = mfloat_transcendental_work_prec(precision);

    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;
    if (x->sign < 0) {
        negate = 1;
        if (mf_abs(x) != 0)
            goto cleanup;
    }

    twox = mfloat_clone_prec(x, work_prec);
    num = mfloat_clone_prec(MF_ONE, work_prec);
    den = mfloat_clone_prec(MF_ONE, work_prec);
    if (!twox || !num || !den)
        goto cleanup;
    if (mf_mul_long(twox, 2) != 0 || mf_exp(twox) != 0)
        goto cleanup;
    if (mfloat_copy_value(num, twox) != 0 || mf_sub(num, MF_ONE) != 0)
        goto cleanup;
    if (mfloat_copy_value(den, twox) != 0 || mf_add(den, MF_ONE) != 0 ||
        mf_div(num, den) != 0)
        goto cleanup;
    if (negate && mf_neg(num) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, num, precision);

cleanup:
    mf_free(x);
    mf_free(twox);
    mf_free(num);
    mf_free(den);
    return rc;
}

int mf_asinh(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *y = NULL, *sinh_y = NULL, *cosh_y = NULL, *delta = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF || mfloat->kind == MFLOAT_KIND_NEGINF)
        return 0;

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_asinh);

    work_prec = mfloat_transcendental_work_prec(precision);
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;
    y = mfloat_new_from_qfloat_prec(qf_asinh(mf_to_qfloat(x)), work_prec);
    if (!y)
        goto cleanup;
    for (int iter = 0; iter < 8; ++iter) {
        sinh_y = mf_clone(y);
        cosh_y = mf_clone(y);
        if (!sinh_y || !cosh_y)
            goto cleanup;
        if (mf_sinh(sinh_y) != 0 || mf_cosh(cosh_y) != 0)
            goto cleanup;
        if (mf_sub(sinh_y, x) != 0 || mf_div(sinh_y, cosh_y) != 0)
            goto cleanup;
        delta = sinh_y;
        sinh_y = NULL;
        if (mf_sub(y, delta) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(delta, (long)work_prec + 8l))
            break;
        mf_free(delta);
        delta = NULL;
        mf_free(cosh_y);
        cosh_y = NULL;
    }
    rc = mfloat_finish_result(mfloat, y, precision);

cleanup:
    mf_free(x);
    mf_free(y);
    mf_free(sinh_y);
    mf_free(cosh_y);
    mf_free(delta);
    return rc;
}

int mf_acosh(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *one = NULL, *y = NULL, *cosh_y = NULL, *sinh_y = NULL, *delta = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_NEGINF || mfloat->sign < 0)
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_acosh);

    work_prec = mfloat_transcendental_work_prec(precision);
    x = mfloat_clone_prec(mfloat, work_prec);
    one = mfloat_clone_prec(MF_ONE, work_prec);
    if (!x || !one)
        goto cleanup;
    if (mf_lt(x, one)) {
        rc = mf_set_double(mfloat, NAN);
        goto cleanup;
    }
    if (mf_eq(x, one)) {
        mf_clear(mfloat);
        rc = 0;
        goto cleanup;
    }
    y = mfloat_new_from_qfloat_prec(qf_acosh(mf_to_qfloat(x)), work_prec);
    if (!y)
        goto cleanup;
    for (int iter = 0; iter < 8; ++iter) {
        cosh_y = mf_clone(y);
        sinh_y = mf_clone(y);
        if (!cosh_y || !sinh_y)
            goto cleanup;
        if (mf_cosh(cosh_y) != 0 || mf_sinh(sinh_y) != 0)
            goto cleanup;
        if (mf_sub(cosh_y, x) != 0 || mf_div(cosh_y, sinh_y) != 0)
            goto cleanup;
        delta = cosh_y;
        cosh_y = NULL;
        if (mf_sub(y, delta) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(delta, (long)work_prec + 8l))
            break;
        mf_free(delta);
        delta = NULL;
        mf_free(sinh_y);
        sinh_y = NULL;
    }
    rc = mfloat_finish_result(mfloat, y, precision);

cleanup:
    mf_free(x);
    mf_free(one);
    mf_free(y);
    mf_free(cosh_y);
    mf_free(sinh_y);
    mf_free(delta);
    return rc;
}

int mf_atanh(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *one = NULL, *y = NULL, *tanh_y = NULL, *deriv = NULL, *delta = NULL, *tmp = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_atanh);

    work_prec = mfloat_transcendental_work_prec(precision);
    x = mfloat_clone_prec(mfloat, work_prec);
    one = mfloat_clone_prec(MF_ONE, work_prec);
    if (!x || !one)
        goto cleanup;
    if (x->sign < 0) {
        mfloat_t *absx = mf_clone(x);
        if (!absx)
            goto cleanup;
        if (mf_abs(absx) != 0) {
            mf_free(absx);
            goto cleanup;
        }
        if (mf_ge(absx, one)) {
            mf_free(absx);
            rc = mf_set_double(mfloat, NAN);
            goto cleanup;
        }
        mf_free(absx);
    } else {
        if (mf_ge(x, one)) {
            rc = mf_set_double(mfloat, NAN);
            goto cleanup;
        }
    }
    y = mfloat_new_from_qfloat_prec(qf_atanh(mf_to_qfloat(x)), work_prec);
    if (!y)
        goto cleanup;
    for (int iter = 0; iter < 8; ++iter) {
        tanh_y = mf_clone(y);
        deriv = mfloat_clone_prec(MF_ONE, work_prec);
        tmp = mf_clone(y);
        if (!tanh_y || !deriv || !tmp)
            goto cleanup;
        if (mf_tanh(tanh_y) != 0 || mf_tanh(tmp) != 0 || mf_mul(tmp, tmp) != 0)
            goto cleanup;
        if (mf_sub(tanh_y, x) != 0 || mf_sub(deriv, tmp) != 0)
            goto cleanup;
        delta = mf_clone(tanh_y);
        if (!delta)
            goto cleanup;
        if (mf_div(delta, deriv) != 0 || mf_sub(y, delta) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(delta, (long)work_prec + 8l))
            break;
        mf_free(delta);
        delta = NULL;
        mf_free(tanh_y);
        tanh_y = NULL;
        mf_free(deriv);
        deriv = NULL;
        mf_free(tmp);
        tmp = NULL;
    }
    rc = mfloat_finish_result(mfloat, y, precision);

cleanup:
    mf_free(x);
    mf_free(one);
    mf_free(y);
    mf_free(tanh_y);
    mf_free(deriv);
    mf_free(delta);
    mf_free(tmp);
    return rc;
}

int mf_gamma(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *tmp = NULL, *pi = NULL, *den = NULL;
    long n = 0, half_n = 0;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_NEGINF || mfloat_is_near_integer_pole(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_gamma);
    if (mfloat_get_exact_long_value(mfloat, &n) && n >= 1)
        return mfloat_set_factorial(mfloat, n - 1, precision);
    if (mfloat_get_small_positive_half_integer(mfloat, &half_n)) {
        tmp = mf_new_prec(precision);
        den = mf_new_prec(precision);
        pi = mfloat_new_sqrt_pi_prec(precision);
        if (!tmp || !den || !pi)
            goto cleanup;
        if (mfloat_set_factorial(tmp, 2 * half_n, precision) != 0 ||
            mfloat_set_factorial(den, half_n, precision) != 0 ||
            mf_div(tmp, den) != 0)
            goto cleanup;
        if (half_n > 0 && mf_ldexp(tmp, -2 * (int)half_n) != 0)
            goto cleanup;
        if (mf_mul(tmp, pi) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, tmp, precision);
        goto cleanup;
    }
    work_prec = mfloat_transcendental_work_prec(precision);
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;

    if (mf_lt(x, MF_HALF)) {
        pi = mfloat_new_pi_prec(work_prec);
        tmp = mfloat_clone_prec(MF_ONE, work_prec);
        if (!pi || !tmp)
            goto cleanup;
        if (mf_sub(tmp, x) != 0 || mf_gamma(tmp) != 0)
            goto cleanup;
        if (mf_mul(pi, x) != 0 || mf_sin(pi) != 0 || mf_mul(pi, tmp) != 0 || mf_inv(pi) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, pi, precision);
        goto cleanup;
    }

    if (mf_lgamma(x) != 0 || mf_exp(x) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, x, precision);

cleanup:
    mf_free(x);
    mf_free(tmp);
    mf_free(pi);
    mf_free(den);
    return rc;
}

int mf_erf(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *x2 = NULL, *sum = NULL, *term = NULL, *coef = NULL;
    int negate = 0;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return mfloat_copy_value(mfloat, MF_ONE);
    if (mfloat->kind == MFLOAT_KIND_NEGINF) {
        rc = mfloat_copy_value(mfloat, MF_ONE);
        if (rc == 0)
            rc = mf_neg(mfloat);
        return rc;
    }

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_erf);
    work_prec = mfloat_transcendental_work_prec(precision);
    if (mfloat_is_exact_half(mfloat, 1)) {
        sum = mfloat_clone_immortal_prec_internal(MFLOAT_INTERNAL_ERF_HALF, work_prec);
        if (!sum)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, sum, precision);
        goto cleanup;
    }
    if (mfloat_is_exact_half(mfloat, -1)) {
        sum = mfloat_clone_immortal_prec_internal(MFLOAT_INTERNAL_ERF_HALF, work_prec);
        if (sum && mf_neg(sum) != 0) {
            mf_free(sum);
            sum = NULL;
        }
        if (!sum)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, sum, precision);
        goto cleanup;
    }
    x = mfloat_clone_prec(mfloat, work_prec);
    x2 = mf_clone(x);
    sum = mf_clone(x);
    term = mf_clone(x);
    coef = mf_new_prec(work_prec);
    if (coef && mfloat_compute_two_over_sqrt_pi(coef, work_prec) != 0) {
        mf_free(coef);
        coef = NULL;
    }
    if (!x || !x2 || !sum || !term || !coef)
        goto cleanup;
    if (x->sign < 0) {
        negate = 1;
        if (mf_abs(x) != 0 || mf_abs(x2) != 0 || mf_abs(sum) != 0 || mf_abs(term) != 0)
            goto cleanup;
    }
    if (mf_mul(x2, x2) != 0)
        goto cleanup;
    for (long n = 0; n < 512; ++n) {
        mfloat_t *next = mf_clone(term);

        if (!next)
            goto cleanup;
        if (mf_mul(next, x2) != 0 || mf_neg(next) != 0 ||
            mf_mul_long(next, 2 * n + 1) != 0 ||
            mfloat_div_long_inplace(next, (n + 1) * (2 * n + 3)) != 0 ||
            mf_add(sum, next) != 0) {
            mf_free(next);
            goto cleanup;
        }
        mf_free(term);
        term = next;
        if (mfloat_is_below_neg_bits(term, (long)work_prec + 8l))
            break;
    }
    if (mf_mul(sum, coef) != 0)
        goto cleanup;
    if (negate && mf_neg(sum) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, sum, precision);

cleanup:
    mf_free(x);
    mf_free(x2);
    mf_free(sum);
    mf_free(term);
    mf_free(coef);
    return rc;
}

int mf_erfc(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *one = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF) {
        mf_clear(mfloat);
        return 0;
    }
    if (mfloat->kind == MFLOAT_KIND_NEGINF)
        return mf_set_long(mfloat, 2);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_erfc);
    work_prec = mfloat_transcendental_work_prec(precision);
    if (mfloat_is_exact_half(mfloat, 1)) {
        one = mfloat_clone_prec(MF_ONE, work_prec);
        if (one) {
            mfloat_t *erf_half = mfloat_clone_immortal_prec_internal(MFLOAT_INTERNAL_ERF_HALF, work_prec);
            if (!erf_half || mf_sub(one, erf_half) != 0) {
                mf_free(one);
                one = NULL;
            }
            mf_free(erf_half);
        }
        if (!one)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, one, precision);
        goto cleanup;
    }
    x = mfloat_clone_prec(mfloat, work_prec);
    one = mfloat_clone_prec(MF_ONE, work_prec);
    if (!x || !one || mf_erf(x) != 0 || mf_sub(one, x) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, one, precision);

cleanup:
    mf_free(x);
    mf_free(one);
    return rc;
}

int mf_erfinv(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *y = NULL, *erfy = NULL, *fp = NULL, *step = NULL;
    mfloat_t *neg_one = NULL, *two_over_sqrt_pi = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_erfinv);
    work_prec = mfloat_transcendental_work_prec(precision);
    if (mfloat_is_exact_half(mfloat, 1)) {
        y = mfloat_clone_immortal_prec_internal(MFLOAT_INTERNAL_ERFINV_HALF, work_prec);
        if (!y)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, y, precision);
        goto cleanup;
    }
    if (mfloat_is_exact_half(mfloat, -1)) {
        y = mfloat_clone_immortal_prec_internal(MFLOAT_INTERNAL_ERFINV_HALF, work_prec);
        if (y && mf_neg(y) != 0) {
            mf_free(y);
            y = NULL;
        }
        if (!y)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, y, precision);
        goto cleanup;
    }
    x = mfloat_clone_prec(mfloat, work_prec);
    neg_one = mfloat_new_from_long_prec(-1, work_prec);
    two_over_sqrt_pi = mf_new_prec(work_prec);
    if (!x || !neg_one || !two_over_sqrt_pi || mfloat_compute_two_over_sqrt_pi(two_over_sqrt_pi, work_prec) != 0 ||
        mf_ge(x, MF_ONE) || mf_le(x, neg_one))
        goto cleanup;
    y = mfloat_new_from_qfloat_prec(qf_erfinv(mf_to_qfloat(x)), work_prec);
    if (!y)
        goto cleanup;

    for (int i = 0; i < 6; ++i) {
        erfy = mf_clone(y);
        fp = mf_clone(y);
        if (!erfy || !fp || mf_erf(erfy) != 0 || mf_sub(erfy, x) != 0)
            goto cleanup;
        if (mf_mul(fp, fp) != 0 || mf_neg(fp) != 0 || mf_exp(fp) != 0 ||
            mf_mul(fp, two_over_sqrt_pi) != 0)
            goto cleanup;
        step = mf_clone(erfy);
        if (!step || mf_div(step, fp) != 0 || mf_sub(y, step) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(step, (long)work_prec + 8l))
            break;
        mf_free(erfy);
        mf_free(fp);
        mf_free(step);
        erfy = fp = step = NULL;
    }
    rc = mfloat_finish_result(mfloat, y, precision);

cleanup:
    mf_free(x);
    mf_free(y);
    mf_free(erfy);
    mf_free(fp);
    mf_free(step);
    mf_free(neg_one);
    mf_free(two_over_sqrt_pi);
    return rc;
}

int mf_erfcinv(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *one = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_erfcinv);
    work_prec = mfloat_transcendental_work_prec(precision);
    if (mfloat_is_exact_half(mfloat, 1))
        return mfloat_set_from_immortal_internal(mfloat, MFLOAT_INTERNAL_ERFINV_HALF, precision);
    x = mfloat_clone_prec(mfloat, work_prec);
    one = mfloat_clone_prec(MF_ONE, work_prec);
    if (!x || !one || mf_sub(one, x) != 0 || mf_erfinv(one) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, one, precision);

cleanup:
    mf_free(x);
    mf_free(one);
    return rc;
}

int mf_lgamma(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *z = NULL, *acc = NULL, *tmp = NULL, *pi = NULL, *threshold = NULL, *den = NULL;
    long n = 0;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_NEGINF || mfloat_is_near_integer_pole(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_lgamma);
    if (mfloat_get_exact_long_value(mfloat, &n) && n >= 1) {
        if (n == 1) {
            mf_clear(mfloat);
            return 0;
        }
        tmp = mf_new_prec(precision);
        if (!tmp)
            return -1;
        if (mfloat_set_factorial(tmp, n - 1, precision) != 0 || mf_log(tmp) != 0) {
            mf_free(tmp);
            return -1;
        }
        rc = mfloat_finish_result(mfloat, tmp, precision);
        mf_free(tmp);
        return rc;
    }
    if (mfloat_get_small_positive_half_integer(mfloat, &n)) {
        tmp = mf_new_prec(precision);
        den = mf_new_prec(precision);
        pi = mfloat_new_half_ln_pi_prec(precision);
        if (!tmp || !den || !pi)
            goto cleanup;
        if (mfloat_set_factorial(tmp, 2 * n, precision) != 0 ||
            mfloat_set_factorial(den, n, precision) != 0 ||
            mf_div(tmp, den) != 0)
            goto cleanup;
        if (n > 0 && mf_ldexp(tmp, -2 * (int)n) != 0)
            goto cleanup;
        if (mf_log(tmp) != 0 || mf_add(tmp, pi) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, tmp, precision);
        goto cleanup;
    }
    work_prec = mfloat_transcendental_work_prec(precision);
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;

    if (mf_lt(x, MF_HALF)) {
        mfloat_t *one_minus_x = mfloat_clone_prec(MF_ONE, work_prec);
        mfloat_t *sin_term = NULL;
        if (!one_minus_x)
            goto cleanup;
        if (mf_sub(one_minus_x, x) != 0)
            goto cleanup;
        if (mf_lgamma(one_minus_x) != 0)
            goto cleanup;
        sin_term = mfloat_new_pi_prec(work_prec);
        if (!sin_term)
            goto cleanup;
        if (mf_mul(sin_term, x) != 0 || mf_sin(sin_term) != 0 || mf_abs(sin_term) != 0 ||
            mf_log(sin_term) != 0)
            goto cleanup;
        pi = mfloat_new_pi_prec(work_prec);
        if (!pi || mf_log(pi) != 0)
            goto cleanup;
        if (mf_sub(pi, sin_term) != 0 || mf_sub(pi, one_minus_x) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, pi, precision);
        mf_free(one_minus_x);
        mf_free(sin_term);
        goto cleanup;
    }

    z = x;
    x = NULL;
    acc = mfloat_clone_prec(MF_ZERO, work_prec);
    threshold = mfloat_new_from_long_prec(100, work_prec);
    tmp = mf_new_prec(work_prec);
    if (!z || !acc || !threshold || !tmp)
        goto cleanup;
    {
        long steps = mfloat_estimate_positive_unit_steps(z, 100);

        if (steps < 0)
            goto cleanup;
        for (long i = 0; i < steps; ++i) {
            if (mfloat_copy_value(tmp, z) != 0 || mf_log(tmp) != 0 ||
                mf_add(acc, tmp) != 0 || mf_add_long(z, 1) != 0)
                goto cleanup;
        }
    }
    while (mf_lt(z, threshold)) {
        if (mfloat_copy_value(tmp, z) != 0 || mf_log(tmp) != 0 ||
            mf_add(acc, tmp) != 0 || mf_add_long(z, 1) != 0)
            goto cleanup;
    }
    if (mfloat_lgamma_asymptotic(z, z, work_prec) != 0 || mf_sub(z, acc) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, z, precision);

cleanup:
    mf_free(x);
    mf_free(z);
    mf_free(acc);
    mf_free(tmp);
    mf_free(pi);
    mf_free(threshold);
    mf_free(den);
    return rc;
}

int mf_digamma(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *z = NULL, *acc = NULL, *tmp = NULL, *twenty = NULL;
    long n = 0;
    long steps = -1;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat) || mfloat_is_near_integer_pole(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_digamma);
    if (mfloat_equals_exact_long(mfloat, 1)) {
        x = mf_euler_mascheroni();
        if (x && mf_set_precision(x, precision) == 0)
            (void)mf_neg(x);
        if (!x)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, x, precision);
        goto cleanup;
    }
    if (mfloat_get_exact_long_value(mfloat, &n) && n >= 1) {
        tmp = mfloat_clone_prec(MF_ZERO, precision);
        z = NULL;
        if (!tmp)
            return -1;
        for (long k = 1; k < n; ++k) {
            z = mfloat_clone_prec(MF_ONE, precision);
            if (!z || mfloat_div_long_inplace(z, k) != 0 || mf_add(tmp, z) != 0)
                goto cleanup;
            mf_free(z);
            z = NULL;
        }
        x = mf_euler_mascheroni();
        if (!x || mf_set_precision(x, precision) != 0 || mf_neg(x) != 0 || mf_add(tmp, x) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, tmp, precision);
        goto cleanup;
    }
    work_prec = mfloat_transcendental_work_prec(precision);
    if (precision > 384u)
        work_prec += 256u;
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;

    if (mf_lt(x, MF_HALF)) {
        rc = mfloat_eval_reflected_gamma_family(mfloat, x, precision, 0);
        goto cleanup;
    }

    z = mfloat_clone_prec(x, work_prec);
    acc = mfloat_clone_prec(MF_ZERO, work_prec);
    twenty = mfloat_new_from_long_prec(20, work_prec);
    tmp = mf_new_prec(work_prec);
    if (!z || !acc || !twenty || !tmp)
        goto cleanup;
    steps = mfloat_estimate_positive_unit_steps(z, 20);
    if (steps < 0)
        goto cleanup;
    for (long i = 0; i < steps; ++i) {
        if (mfloat_copy_value(tmp, z) != 0 || mf_div(tmp, z) != 0 ||
            mf_sub(acc, tmp) != 0 || mf_add_long(z, 1) != 0)
            goto cleanup;
    }
    while (mf_lt(z, twenty)) {
        if (mfloat_copy_value(tmp, z) != 0 || mf_div(tmp, z) != 0 ||
            mf_sub(acc, tmp) != 0 || mf_add_long(z, 1) != 0)
            goto cleanup;
    }
    if (mfloat_digamma_asymptotic(z, z, work_prec) != 0 || mf_add(z, acc) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, z, precision);

cleanup:
    mf_free(x);
    mf_free(z);
    mf_free(acc);
    mf_free(tmp);
    mf_free(twenty);
    return rc;
}

int mf_trigamma(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *z = NULL, *acc = NULL, *tmp = NULL, *twenty = NULL;
    long n = 0;
    long steps = -1;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat) || mfloat_is_near_integer_pole(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_trigamma);
    work_prec = mfloat_transcendental_work_prec(precision);
    if (mfloat_equals_exact_long(mfloat, 1)) {
        tmp = mfloat_new_pi_prec(work_prec);
        if (tmp && (mf_mul(tmp, tmp) != 0 || mfloat_div_long_inplace(tmp, 6) != 0)) {
            mf_free(tmp);
            tmp = NULL;
        }
        if (!tmp)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, tmp, precision);
        goto cleanup;
    }
    if (mfloat_get_exact_long_value(mfloat, &n) && n >= 1) {
        tmp = mfloat_new_pi_prec(work_prec);
        if (!tmp || mf_mul(tmp, tmp) != 0 || mfloat_div_long_inplace(tmp, 6) != 0)
            goto cleanup;
        for (long k = 1; k < n; ++k) {
            x = mfloat_new_from_long_prec(k, work_prec);
            if (!x || mf_mul(x, x) != 0 || mf_inv(x) != 0 || mf_sub(tmp, x) != 0)
                goto cleanup;
            mf_free(x);
            x = NULL;
        }
        rc = mfloat_finish_result(mfloat, tmp, precision);
        goto cleanup;
    }
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;

    if (mf_lt(x, MF_HALF)) {
        rc = mfloat_eval_reflected_gamma_family(mfloat, x, precision, 1);
        goto cleanup;
    }

    z = mfloat_clone_prec(x, work_prec);
    acc = mfloat_clone_prec(MF_ZERO, work_prec);
    twenty = mfloat_new_from_long_prec(20, work_prec);
    tmp = mf_new_prec(work_prec);
    if (!z || !acc || !twenty || !tmp)
        goto cleanup;
    steps = mfloat_estimate_positive_unit_steps(z, 20);
    if (steps < 0)
        goto cleanup;
    for (long i = 0; i < steps; ++i) {
        if (mfloat_copy_value(tmp, z) != 0 || mf_mul(tmp, z) != 0 ||
            mf_inv(tmp) != 0 || mf_add(acc, tmp) != 0 ||
            mf_add_long(z, 1) != 0)
            goto cleanup;
    }
    while (mf_lt(z, twenty)) {
        if (mfloat_copy_value(tmp, z) != 0 || mf_mul(tmp, z) != 0 ||
            mf_inv(tmp) != 0 || mf_add(acc, tmp) != 0 ||
            mf_add_long(z, 1) != 0)
            goto cleanup;
    }
    if (mfloat_trigamma_asymptotic(z, z, work_prec) != 0 || mf_add(z, acc) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, z, precision);

cleanup:
    mf_free(x);
    mf_free(z);
    mf_free(acc);
    mf_free(tmp);
    mf_free(twenty);
    return rc;
}

int mf_tetragamma(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *z = NULL, *acc = NULL, *tmp = NULL, *twenty = NULL;
    long steps = -1;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat) || mfloat_is_near_integer_pole(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_tetragamma);
    if (mfloat_equals_exact_long(mfloat, 1)) {
        tmp = mfloat_clone_immortal_prec_internal(MFLOAT_INTERNAL_TETRAGAMMA_1, precision);
        if (!tmp)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, tmp, precision);
        goto cleanup;
    }
    work_prec = mfloat_transcendental_work_prec(precision);
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;

    if (mf_lt(x, MF_HALF)) {
        rc = mfloat_eval_reflected_gamma_family(mfloat, x, precision, 2);
        goto cleanup;
    }

    z = mfloat_clone_prec(x, work_prec);
    acc = mfloat_clone_prec(MF_ZERO, work_prec);
    twenty = mfloat_new_from_long_prec(20, work_prec);
    tmp = mf_new_prec(work_prec);
    if (!z || !acc || !twenty || !tmp)
        goto cleanup;
    steps = mfloat_estimate_positive_unit_steps(z, 20);
    if (steps < 0)
        goto cleanup;
    for (long i = 0; i < steps; ++i) {
        if (mfloat_copy_value(tmp, z) != 0 || mf_mul(tmp, z) != 0 ||
            mf_mul(tmp, z) != 0 || mf_inv(tmp) != 0 ||
            mf_mul_long(tmp, 2) != 0 || mf_sub(acc, tmp) != 0 || mf_add_long(z, 1) != 0)
            goto cleanup;
    }
    while (mf_lt(z, twenty)) {
        if (mfloat_copy_value(tmp, z) != 0 || mf_mul(tmp, z) != 0 ||
            mf_mul(tmp, z) != 0 || mf_inv(tmp) != 0 ||
            mf_mul_long(tmp, 2) != 0 || mf_sub(acc, tmp) != 0 || mf_add_long(z, 1) != 0)
            goto cleanup;
    }
    if (mfloat_tetragamma_asymptotic(z, z, work_prec) != 0 || mf_add(z, acc) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, z, precision);

cleanup:
    mf_free(x);
    mf_free(z);
    mf_free(acc);
    mf_free(tmp);
    mf_free(twenty);
    return rc;
}

int mf_gammainv(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *y = NULL, *logy = NULL, *x = NULL, *lg = NULL, *psi = NULL, *step = NULL;
    mfloat_t *one = NULL, *minval = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat) || mfloat->sign <= 0)
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_gammainv);
    work_prec = mfloat_transcendental_work_prec(precision);

    y = mfloat_clone_prec(mfloat, work_prec);
    one = mfloat_clone_prec(MF_ONE, work_prec);
    minval = mfloat_clone_immortal_prec_internal(MFLOAT_INTERNAL_GAMMAINV_MIN, work_prec);
    if (!y || !one || !minval)
        goto cleanup;
    if (mf_lt(y, minval)) {
        rc = mf_set_double(mfloat, NAN);
        goto cleanup;
    }
    if (mfloat_equals_exact_long(mfloat, 3)) {
        rc = mfloat_set_from_immortal_internal(mfloat, MFLOAT_INTERNAL_GAMMAINV_3, precision);
        goto cleanup;
    }

    logy = mf_clone(y);
    if (!logy || mf_log(logy) != 0)
        goto cleanup;
    if (mf_le(y, one)) {
        x = mfloat_clone_prec(MF_ONE, work_prec);
    } else {
        x = mfloat_new_from_qfloat_prec(qf_gammainv(mf_to_qfloat(y)), work_prec);
        if (!x)
            x = mf_clone(logy);
        if (x && mf_lt(x, MF_ONE) &&
            mfloat_set_from_immortal_internal(x, MFLOAT_INTERNAL_GAMMAINV_ARGMIN, work_prec) != 0)
            goto cleanup;
    }
    if (!x)
        goto cleanup;

    for (int i = 0; i < 8; ++i) {
        lg = mf_clone(x);
        psi = mf_clone(x);
        if (!lg || !psi || mf_lgamma(lg) != 0 || mf_digamma(psi) != 0)
            goto cleanup;
        if (mf_sub(lg, logy) != 0)
            goto cleanup;
        step = mf_clone(lg);
        if (!step || mf_div(step, psi) != 0 || mf_sub(x, step) != 0)
            goto cleanup;
        if (mf_gt(y, one) && mf_lt(x, MF_ONE) &&
            mfloat_set_from_immortal_internal(x, MFLOAT_INTERNAL_GAMMAINV_ARGMIN, work_prec) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(step, (long)work_prec + 8l))
            break;
        mf_free(lg);
        mf_free(psi);
        mf_free(step);
        lg = psi = step = NULL;
    }
    rc = mfloat_finish_result(mfloat, x, precision);

cleanup:
    mf_free(y);
    mf_free(logy);
    mf_free(x);
    mf_free(lg);
    mf_free(psi);
    mf_free(step);
    mf_free(one);
    mf_free(minval);
    return rc;
}

int mf_lambert_w0(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *w = NULL, *ew = NULL, *num = NULL, *den = NULL, *step = NULL;
    mfloat_t *one = NULL, *two = NULL, *wplus2 = NULL, *corr = NULL;
    long n = 0;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_lambert_w0);
    work_prec = mfloat_transcendental_work_prec(precision);
    if (mfloat_equals_exact_long(mfloat, 1)) {
        w = mfloat_clone_immortal_prec_internal(MFLOAT_INTERNAL_LAMBERT_W0_1, work_prec);
        if (!w)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, w, precision);
        goto cleanup;
    }
    if (mfloat_get_exact_long_value(mfloat, &n) && n == 1) {
        w = mfloat_clone_immortal_prec_internal(MFLOAT_INTERNAL_LAMBERT_W0_1, work_prec);
        if (!w)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, w, precision);
        goto cleanup;
    }
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;
    one = mfloat_clone_prec(MF_ONE, work_prec);
    two = mfloat_new_from_long_prec(2, work_prec);
    w = mfloat_new_from_qfloat_prec(qf_lambert_w0(mf_to_qfloat(x)), work_prec);
    if (!w || !one || !two)
        goto cleanup;

    for (int i = 0; i < 20; ++i) {
        ew = mf_clone(w);
        num = mf_clone(w);
        den = mf_clone(w);
        wplus2 = mf_clone(w);
        corr = mf_clone(w);
        if (!ew || !num || !den || !wplus2 || !corr || mf_exp(ew) != 0)
            goto cleanup;
        if (mf_mul(num, ew) != 0 || mf_sub(num, x) != 0)
            goto cleanup;
        if (mf_add(den, one) != 0 || mf_mul(den, ew) != 0)
            goto cleanup;
        if (mf_add(wplus2, two) != 0 || mf_mul(wplus2, ew) != 0)
            goto cleanup;
        if (mfloat_copy_value(corr, num) != 0 || mf_div(corr, den) != 0 ||
            mf_mul(corr, wplus2) != 0 || mfloat_div_long_inplace(corr, 2) != 0)
            goto cleanup;
        if (mf_sub(den, corr) != 0)
            goto cleanup;
        step = mf_clone(num);
        if (!step || mf_div(step, den) != 0 || mf_sub(w, step) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(step, (long)work_prec + 8l))
            break;
        mf_free(ew);
        mf_free(num);
        mf_free(den);
        mf_free(step);
        mf_free(wplus2);
        mf_free(corr);
        ew = num = den = step = wplus2 = corr = NULL;
    }
    rc = mfloat_finish_result(mfloat, w, precision);

cleanup:
    mf_free(x);
    mf_free(w);
    mf_free(ew);
    mf_free(num);
    mf_free(den);
    mf_free(step);
    mf_free(one);
    mf_free(two);
    mf_free(wplus2);
    mf_free(corr);
    return rc;
}

int mf_lambert_wm1(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *w = NULL, *ew = NULL, *num = NULL, *den = NULL, *step = NULL, *corr = NULL, *two = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_lambert_wm1);
    work_prec = mfloat_transcendental_work_prec(precision);

    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;
    w = mfloat_new_from_qfloat_prec(qf_lambert_wm1(mf_to_qfloat(x)), work_prec);
    if (!w)
        goto cleanup;

    two = mfloat_new_from_long_prec(2, work_prec);
    if (!two)
        goto cleanup;

    for (int i = 0; i < 12; ++i) {
        ew = mf_clone(w);
        num = mf_clone(w);
        den = mf_clone(w);
        corr = mf_clone(w);
        if (!ew || !num || !den || !corr || mf_exp(ew) != 0)
            goto cleanup;
        if (mf_mul(num, ew) != 0 || mf_sub(num, x) != 0)
            goto cleanup;
        if (mf_add_long(den, 1) != 0 || mf_mul(den, ew) != 0)
            goto cleanup;
        if (mf_add(corr, two) != 0 || mf_mul(corr, num) != 0 || mfloat_div_long_inplace(corr, 2) != 0 ||
            mf_div(corr, den) != 0 || mf_sub(den, corr) != 0)
            goto cleanup;
        step = mf_clone(num);
        if (!step || mf_div(step, den) != 0 || mf_sub(w, step) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(step, (long)work_prec + 8l))
            break;
        mf_free(ew);
        mf_free(num);
        mf_free(den);
        mf_free(step);
        mf_free(corr);
        ew = num = den = step = corr = NULL;
    }
    rc = mfloat_finish_result(mfloat, w, precision);

cleanup:
    mf_free(x);
    mf_free(w);
    mf_free(ew);
    mf_free(num);
    mf_free(den);
    mf_free(step);
    mf_free(corr);
    mf_free(two);
    return rc;
}

int mf_beta(mfloat_t *mfloat, const mfloat_t *other)
{
    long a = 0, b = 0;
    size_t precision;
    mfloat_t *num = NULL, *den = NULL;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_binary(mfloat, other, qf_beta);

    if (mfloat_get_small_positive_integer(mfloat, &a) &&
        mfloat_get_small_positive_integer(other, &b)) {
        long k;

        num = mfloat_clone_prec(MF_ONE, mfloat_transcendental_work_prec(precision));
        den = mfloat_clone_prec(MF_ONE, mfloat_transcendental_work_prec(precision));
        if (!num || !den)
            goto cleanup;
        for (k = 1; k <= b - 1; ++k) {
            if (mf_mul_long(num, k) != 0)
                goto cleanup;
        }
        for (k = 0; k <= b - 1; ++k) {
            if (mf_mul_long(den, a + k) != 0)
                goto cleanup;
        }
        if (mf_div(num, den) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, num, precision);
        goto cleanup;
    }

    return mfloat_apply_qfloat_binary(mfloat, other, qf_beta);

cleanup:
    mf_free(num);
    mf_free(den);
    return rc;
}

int mf_logbeta(mfloat_t *mfloat, const mfloat_t *other)
{
    size_t precision, work_prec;
    mfloat_t *a = NULL, *b = NULL, *sum = NULL, *beta = NULL;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_binary(mfloat, other, qf_logbeta);
    work_prec = mfloat_transcendental_work_prec(precision);
    beta = mf_new_prec(work_prec);
    if (!beta)
        goto cleanup;
    if (mfloat_try_small_exact_beta(beta, mfloat, other, work_prec) == 0) {
        if (mf_log(beta) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, beta, precision);
        goto cleanup;
    }
    work_prec += 128u;
    a = mfloat_clone_prec(mfloat, work_prec);
    b = mfloat_clone_prec(other, work_prec);
    sum = mf_clone(a);
    if (!a || !b || !sum)
        goto cleanup;
    if (mf_add(sum, b) != 0 || mf_lgamma(a) != 0 || mf_lgamma(b) != 0 || mf_lgamma(sum) != 0)
        goto cleanup;
    if (mf_add(a, b) != 0 || mf_sub(a, sum) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, a, precision);

cleanup:
    mf_free(a);
    mf_free(b);
    mf_free(sum);
    mf_free(beta);
    return rc;
}

int mf_binomial(mfloat_t *mfloat, const mfloat_t *other)
{
    size_t precision, work_prec;
    mfloat_t *a1 = NULL, *b1 = NULL, *amb1 = NULL;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_binary(mfloat, other, qf_binomial);
    work_prec = mfloat_transcendental_work_prec(precision);
    a1 = mfloat_clone_prec(mfloat, work_prec);
    b1 = mfloat_clone_prec(other, work_prec);
    amb1 = mfloat_clone_prec(mfloat, work_prec);
    if (!a1 || !b1 || !amb1)
        goto cleanup;
    if (mf_add_long(a1, 1) != 0 || mf_add_long(b1, 1) != 0 ||
        mf_sub(amb1, other) != 0 || mf_add_long(amb1, 1) != 0)
        goto cleanup;
    if (mf_lgamma(a1) != 0 || mf_lgamma(b1) != 0 || mf_lgamma(amb1) != 0)
        goto cleanup;
    if (mf_sub(a1, b1) != 0 || mf_sub(a1, amb1) != 0 || mf_exp(a1) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, a1, precision);

cleanup:
    mf_free(a1);
    mf_free(b1);
    mf_free(amb1);
    return rc;
}

int mf_beta_pdf(mfloat_t *mfloat, const mfloat_t *a, const mfloat_t *b)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *aa = NULL, *bb = NULL, *one_minus_x = NULL, *logpdf = NULL, *logb = NULL;
    int rc = -1;

    if (!mfloat || !a || !b)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_ternary(mfloat, a, b, qf_beta_pdf);
    work_prec = mfloat_transcendental_work_prec(precision) + 64u;
    x = mfloat_clone_prec(mfloat, work_prec);
    aa = mfloat_clone_prec(a, work_prec);
    bb = mfloat_clone_prec(b, work_prec);
    one_minus_x = mfloat_clone_prec(MF_ONE, work_prec);
    logpdf = mfloat_clone_prec(mfloat, work_prec);
    logb = mfloat_clone_prec(a, work_prec);
    if (!x || !aa || !bb || !one_minus_x || !logpdf || !logb)
        goto cleanup;
    if (mf_sub(one_minus_x, x) != 0 || mf_log(logpdf) != 0 || mf_sub(aa, MF_ONE) != 0 ||
        mf_mul(logpdf, aa) != 0)
        goto cleanup;
    if (mf_log(one_minus_x) != 0 || mf_sub(bb, MF_ONE) != 0 || mf_mul(one_minus_x, bb) != 0 ||
        mf_add(logpdf, one_minus_x) != 0 || mf_logbeta(logb, b) != 0 || mf_sub(logpdf, logb) != 0 ||
        mf_exp(logpdf) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, logpdf, precision);

cleanup:
    mf_free(x);
    mf_free(aa);
    mf_free(bb);
    mf_free(one_minus_x);
    mf_free(logpdf);
    mf_free(logb);
    return rc;
}

int mf_logbeta_pdf(mfloat_t *mfloat, const mfloat_t *a, const mfloat_t *b)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *aa = NULL, *bb = NULL, *one_minus_x = NULL, *logb = NULL;
    int rc = -1;

    if (!mfloat || !a || !b)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_ternary(mfloat, a, b, qf_logbeta_pdf);
    work_prec = mfloat_transcendental_work_prec(precision) + 64u;
    x = mfloat_clone_prec(mfloat, work_prec);
    aa = mfloat_clone_prec(a, work_prec);
    bb = mfloat_clone_prec(b, work_prec);
    one_minus_x = mfloat_clone_prec(MF_ONE, work_prec);
    logb = mfloat_clone_prec(a, work_prec);
    if (!x || !aa || !bb || !one_minus_x || !logb)
        goto cleanup;
    if (mf_sub(one_minus_x, x) != 0 || mf_log(x) != 0 || mf_sub(aa, MF_ONE) != 0 || mf_mul(x, aa) != 0)
        goto cleanup;
    if (mf_log(one_minus_x) != 0 || mf_sub(bb, MF_ONE) != 0 || mf_mul(one_minus_x, bb) != 0 ||
        mf_add(x, one_minus_x) != 0 || mf_logbeta(logb, b) != 0 || mf_sub(x, logb) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, x, precision);

cleanup:
    mf_free(x);
    mf_free(aa);
    mf_free(bb);
    mf_free(one_minus_x);
    mf_free(logb);
    return rc;
}

int mf_normal_pdf(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x2 = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_normal_pdf);
    work_prec = mfloat_transcendental_work_prec(precision) + 32u;
    x2 = mfloat_clone_prec(mfloat, work_prec);
    if (!x2)
        goto cleanup;
    if (mf_normal_logpdf(x2) != 0 || mf_exp(x2) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, x2, precision);

cleanup:
    mf_free(x2);
    return rc;
}

int mf_normal_cdf(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *t = NULL, *half = NULL, *sqrt2 = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_normal_cdf);
    if (mf_is_zero(mfloat))
        return mfloat_copy_value(mfloat, MF_HALF);
    work_prec = mfloat_transcendental_work_prec(precision);
    t = mfloat_clone_prec(mfloat, work_prec);
    half = mfloat_clone_prec(MF_HALF, work_prec);
    sqrt2 = mfloat_clone_immortal_prec_internal(MF_SQRT2, work_prec);
    if (!t || !half || !sqrt2 || mf_div(t, sqrt2) != 0 ||
        mf_erf(t) != 0 || mf_add(t, MF_ONE) != 0 || mf_mul(t, half) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, t, precision);

cleanup:
    mf_free(t);
    mf_free(half);
    mf_free(sqrt2);
    return rc;
}

int mf_normal_logpdf(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x2 = NULL, *c = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_normal_logpdf);
    work_prec = mfloat_transcendental_work_prec(precision);
    x2 = mfloat_clone_prec(mfloat, work_prec);
    c = mfloat_clone_immortal_prec_internal(MFLOAT_INTERNAL_HALF_LN_2PI, work_prec);
    if (!x2 || !c ||
        mf_mul(x2, x2) != 0 || mfloat_div_long_inplace(x2, 2) != 0 ||
        mf_add(x2, c) != 0 || mf_neg(x2) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, x2, precision);

cleanup:
    mf_free(x2);
    mf_free(c);
    return rc;
}

int mf_productlog(mfloat_t *mfloat)
{
    return mf_lambert_w0(mfloat);
}

int mf_gammainc_lower(mfloat_t *mfloat, const mfloat_t *other)
{
    size_t precision, work_prec;
    mfloat_t *p = NULL, *g = NULL;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_binary(mfloat, other, qf_gammainc_lower);
    work_prec = mfloat_transcendental_work_prec(precision);
    p = mfloat_clone_prec(mfloat, work_prec);
    g = mfloat_clone_prec(mfloat, work_prec);
    if (!p || !g || mf_gammainc_P(p, other) != 0 || mf_gamma(g) != 0 || mf_mul(p, g) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, p, precision);

cleanup:
    mf_free(p);
    mf_free(g);
    return rc;
}

int mf_gammainc_upper(mfloat_t *mfloat, const mfloat_t *other)
{
    size_t precision, work_prec;
    mfloat_t *q = NULL, *g = NULL;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_binary(mfloat, other, qf_gammainc_upper);
    work_prec = mfloat_transcendental_work_prec(precision);
    q = mfloat_clone_prec(mfloat, work_prec);
    g = mfloat_clone_prec(mfloat, work_prec);
    if (!q || !g || mf_gammainc_Q(q, other) != 0 || mf_gamma(g) != 0 || mf_mul(q, g) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, q, precision);

cleanup:
    mf_free(q);
    mf_free(g);
    return rc;
}

int mf_gammainc_P(mfloat_t *mfloat, const mfloat_t *other)
{
    size_t precision, work_prec;
    long s_long = 0;
    mfloat_t *s = NULL, *x = NULL, *tmp = NULL, *s_plus_one = NULL;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_binary(mfloat, other, qf_gammainc_P);
    work_prec = mfloat_transcendental_work_prec(precision);
    s = mfloat_clone_prec(mfloat, work_prec);
    x = mfloat_clone_prec(other, work_prec);
    if (!s || !x)
        goto cleanup;

    if (mf_le(x, MF_ZERO)) {
        mf_clear(mfloat);
        rc = 0;
        goto cleanup;
    }
    if (mf_le(s, MF_ZERO)) {
        rc = mf_set_double(mfloat, NAN);
        goto cleanup;
    }

    if (mfloat_is_finite(mfloat) &&
        mfloat->sign > 0 &&
        mfloat->exponent2 == 0 &&
        mi_fits_long(mfloat->mantissa) &&
        mi_get_long(mfloat->mantissa, &s_long) &&
        s_long == 1) {
        tmp = mfloat_clone_prec(other, work_prec);
        if (!tmp)
            goto cleanup;
        if (mf_neg(tmp) != 0 || mf_exp(tmp) != 0)
            goto cleanup;
        if (mfloat_copy_value(x, MF_ONE) != 0 || mf_set_precision(x, work_prec) != 0)
            goto cleanup;
        if (mf_sub(x, tmp) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, x, precision);
        goto cleanup;
    }
    s_plus_one = mf_clone(s);
    if (!s_plus_one || mf_add_long(s_plus_one, 1) != 0)
        goto cleanup;
    if (mf_lt(x, s_plus_one)) {
        if (mfloat_gammainc_series_P(x, s, x, work_prec) != 0)
            goto cleanup;
    } else {
        tmp = mf_clone(s);
        if (!tmp || mfloat_gammainc_cf_Q(tmp, s, x, work_prec) != 0)
            goto cleanup;
        if (mfloat_copy_value(x, MF_ONE) != 0 || mf_set_precision(x, work_prec) != 0 || mf_sub(x, tmp) != 0)
            goto cleanup;
    }
    rc = mfloat_finish_result(mfloat, x, precision);

cleanup:
    mf_free(s);
    mf_free(x);
    mf_free(tmp);
    mf_free(s_plus_one);
    return rc;
}

int mf_gammainc_Q(mfloat_t *mfloat, const mfloat_t *other)
{
    size_t precision, work_prec;
    long s_long = 0;
    mfloat_t *p = NULL, *x = NULL;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_binary(mfloat, other, qf_gammainc_Q);
    if (mfloat_equals_exact_long(mfloat, 1) && mfloat_equals_exact_long(other, 1)) {
        x = mf_e();
        if (!x || mf_set_precision(x, precision) != 0 || mf_inv(x) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, x, precision);
        goto cleanup;
    }
    if (mfloat_get_exact_long_value(mfloat, &s_long) && s_long == 1) {
        work_prec = mfloat_transcendental_work_prec(precision);
        x = mfloat_clone_prec(other, work_prec);
        if (!x || mf_neg(x) != 0 || mf_exp(x) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, x, precision);
        goto cleanup;
    }
    work_prec = mfloat_transcendental_work_prec(precision);
    p = mfloat_clone_prec(mfloat, work_prec);
    if (!p || mf_gammainc_P(p, other) != 0)
        goto cleanup;
    if (mfloat_copy_value(mfloat, MF_ONE) != 0 || mf_set_precision(mfloat, work_prec) != 0 || mf_sub(mfloat, p) != 0)
        goto cleanup;
    rc = mfloat_round_to_precision(mfloat, precision);

cleanup:
    mf_free(p);
    mf_free(x);
    return rc;
}

int mf_ei(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_NEGINF)
        return mf_clear(mfloat), 0;

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_ei);
    work_prec = mfloat_transcendental_work_prec(precision);
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;
    rc = mfloat_ei_series(x, x, work_prec);
    if (rc == 0)
        rc = mfloat_finish_result(mfloat, x, precision);

cleanup:
    mf_free(x);
    return rc;
}

int mf_e1(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF) {
        mf_clear(mfloat);
        return 0;
    }
    if (mfloat->kind == MFLOAT_KIND_NEGINF || !mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_e1);
    work_prec = mfloat_transcendental_work_prec(precision) + 64u;
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x || mf_le(x, MF_ZERO))
        goto cleanup;
    if (mfloat_e1_series_positive(x, x, work_prec) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, x, precision);

cleanup:
    mf_free(x);
    return rc;
}
