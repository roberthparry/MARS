#include "mfloat_internal.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>

#define MFLOAT_CONST_LITERAL_BITS 256u
#define MFLOAT_CONST_GUARD_BITS   32u
#define MFLOAT_QFLOAT_EFFECTIVE_BITS 106u
#define MFLOAT_GAMMA_MIN_SHIFT    12u
#define MFLOAT_GAMMA_MAX_SHIFT    18u

typedef struct mfloat_gamma_coeff_t {
    const char *num;
    const char *den;
    unsigned power;
} mfloat_gamma_coeff_t;

static mfloat_t *mfloat_clone_prec(const mfloat_t *src, size_t precision);
static int mfloat_compute_pi(mfloat_t *dst, size_t precision);
static int mfloat_compute_ln2(mfloat_t *dst, size_t precision);
static int mfloat_compute_e(mfloat_t *dst, size_t precision);
static int mfloat_compute_euler_gamma(mfloat_t *dst, size_t precision);
static int mfloat_set_from_seed(mfloat_t *dst, const char *text, size_t precision);

static mfloat_t *mfloat_cached_pi = NULL;
static size_t mfloat_cached_pi_prec = 0u;
static mfloat_t *mfloat_cached_ln2 = NULL;
static size_t mfloat_cached_ln2_prec = 0u;
static mfloat_t *mfloat_cached_e = NULL;
static size_t mfloat_cached_e_prec = 0u;
static mfloat_t *mfloat_cached_gamma = NULL;
static size_t mfloat_cached_gamma_prec = 0u;

static int mfloat_copy_cached_constant(mfloat_t *dst,
                                       mfloat_t **cache,
                                       size_t *cache_prec,
                                       size_t precision,
                                       int (*compute)(mfloat_t *, size_t))
{
    int rc;

    if (!dst || !cache || !cache_prec || !compute)
        return -1;

    if (!*cache || *cache_prec != precision) {
        mfloat_t *tmp = mf_new_prec(precision);

        if (!tmp)
            return -1;
        if (compute(tmp, precision) != 0) {
            mf_free(tmp);
            return -1;
        }
        mf_free(*cache);
        *cache = tmp;
        *cache_prec = precision;
    }

    rc = mfloat_copy_value(dst, *cache);
    if (rc == 0)
        dst->precision = precision;
    return rc;
}

static mfloat_t *mfloat_create_constant_cached(const char *seed,
                                               int (*compute)(mfloat_t *, size_t),
                                               mfloat_t **cache,
                                               size_t *cache_prec)
{
    size_t precision = mfloat_get_default_precision_internal();
    mfloat_t *tmp = mf_new_prec(precision);

    if (!tmp)
        return NULL;

    if (!*cache || *cache_prec != precision) {
        int rc;

        if (precision <= MFLOAT_CONST_LITERAL_BITS)
            rc = mfloat_set_from_seed(tmp, seed, precision);
        else
            rc = compute(tmp, precision);
        if (rc != 0) {
            mf_free(tmp);
            return NULL;
        }
        mf_free(*cache);
        *cache = tmp;
        *cache_prec = precision;
    } else {
        mf_free(tmp);
    }

    return mfloat_clone_prec(*cache, precision);
}

static mfloat_t *mfloat_new_from_string_prec(const char *text, size_t precision)
{
    mfloat_t *mfloat = mf_new_prec(precision);

    if (!mfloat)
        return NULL;
    if (mf_set_string(mfloat, text) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
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
    if (mfloat_copy_cached_constant(mfloat,
                                    &mfloat_cached_pi,
                                    &mfloat_cached_pi_prec,
                                    precision,
                                    mfloat_compute_pi) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

static int mfloat_div_long_inplace(mfloat_t *mfloat, long value)
{
    mfloat_t *tmp;
    int rc;

    tmp = mfloat_new_from_long_prec(value, mfloat ? mfloat->precision : MFLOAT_DEFAULT_PRECISION_BITS);
    if (!tmp)
        return -1;
    rc = mf_div(mfloat, tmp);
    mf_free(tmp);
    return rc;
}

static int mfloat_mul_long_inplace(mfloat_t *mfloat, long value)
{
    return mf_mul_long(mfloat, value);
}

static int mfloat_round_to_precision(mfloat_t *mfloat, size_t precision)
{
    size_t bitlen, excess;
    mint_t *hi = NULL, *trunc = NULL, *low = NULL, *half = NULL;
    int rc = -1;

    if (!mfloat || !mfloat->mantissa || !mfloat_is_finite(mfloat) || precision == 0)
        return -1;

    bitlen = mi_bit_length(mfloat->mantissa);
    if (bitlen <= precision) {
        mfloat->precision = precision;
        return 0;
    }

    excess = bitlen - precision;
    hi = mi_clone(mfloat->mantissa);
    low = mi_clone(mfloat->mantissa);
    if (!hi || !low)
        goto cleanup;

    if (mi_shr(hi, (long)excess) != 0)
        goto cleanup;

    trunc = mi_clone(hi);
    if (!trunc)
        goto cleanup;
    if (mi_shl(trunc, (long)excess) != 0)
        goto cleanup;
    if (mi_sub(low, trunc) != 0)
        goto cleanup;

    half = mi_create_2pow((uint64_t)(excess - 1u));
    if (!half)
        goto cleanup;
    if (mi_cmp(low, half) >= 0) {
        if (mi_inc(hi) != 0)
            goto cleanup;
    }

    mi_clear(mfloat->mantissa);
    if (mi_add(mfloat->mantissa, hi) != 0)
        goto cleanup;
    mfloat->exponent2 += (long)excess;
    mfloat->precision = precision;
    rc = mfloat_normalise(mfloat);

cleanup:
    mi_free(hi);
    mi_free(trunc);
    mi_free(low);
    mi_free(half);
    return rc;
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

static mfloat_t *mfloat_from_seed_or_null(const char *text, size_t precision)
{
    mfloat_t *mfloat = mfloat_new_from_string_prec(text, precision);

    if (!mfloat)
        return NULL;
    if (mfloat_round_to_precision(mfloat, precision) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

static int mfloat_set_from_seed(mfloat_t *dst, const char *text, size_t precision)
{
    mfloat_t *tmp = mfloat_from_seed_or_null(text, precision);
    int rc;

    if (!tmp)
        return -1;
    rc = mfloat_copy_value(dst, tmp);
    if (rc == 0)
        dst->precision = precision;
    mf_free(tmp);
    return rc;
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

static int mfloat_compute_ln2(mfloat_t *dst, size_t precision)
{
    mfloat_t *sum = NULL, *term = NULL, *piece = NULL;
    size_t work_prec = precision + MFLOAT_CONST_GUARD_BITS;
    long k;
    int rc = -1;

    sum = mfloat_clone_prec(MF_ZERO, work_prec);
    term = mfloat_clone_prec(MF_ONE, work_prec);
    if (!sum || !term)
        goto cleanup;
    if (mfloat_div_long_inplace(term, 3) != 0)
        goto cleanup;

    for (k = 0; k < LONG_MAX / 2; ++k) {
        long denom = 2l * k + 1l;

        piece = mf_clone(term);
        if (!piece)
            goto cleanup;
        if (mfloat_div_long_inplace(piece, denom) != 0)
            goto cleanup;
        if (mf_add(sum, piece) != 0)
            goto cleanup;
        mf_free(piece);
        piece = NULL;

        if (mfloat_is_below_neg_bits(term, (long)work_prec + 10l))
            break;
        if (mfloat_div_long_inplace(term, 9) != 0)
            goto cleanup;
    }

    if (mf_mul_long(sum, 2) != 0)
        goto cleanup;
    if (mfloat_round_to_precision(sum, precision) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(sum);
    mf_free(term);
    mf_free(piece);
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

static int mfloat_apply_rational(mfloat_t *mfloat,
                                 const char *num,
                                 const char *den,
                                 size_t precision)
{
    mfloat_t *n = NULL, *d = NULL;
    int rc = -1;

    n = mfloat_new_from_string_prec(num, precision);
    d = mfloat_new_from_string_prec(den, precision);
    if (!n || !d)
        goto cleanup;
    if (mf_div(n, d) != 0)
        goto cleanup;
    rc = mf_mul(mfloat, n);

cleanup:
    mf_free(n);
    mf_free(d);
    return rc;
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

static int mfloat_compute_euler_gamma(mfloat_t *dst, size_t precision)
{
    static const mfloat_gamma_coeff_t coeffs[] = {
        {  "1",                 "12",     2u },
        { "-1",                "120",     4u },
        {  "1",                "252",     6u },
        { "-1",                "240",     8u },
        {  "1",                "132",    10u },
        {"-691",             "32760",    12u },
        {  "1",                 "12",    14u },
        {"-3617",             "8160",    16u },
        {"43867",            "14364",    18u },
        {"-174611",           "6600",    20u },
        {"854513",            "3036",    22u },
        {"-236364091",       "65520",    24u },
        {"8553103",            "156",    26u },
        {"-23749461029",     "24360",    28u },
        {"8615841276005",   "458304",    30u },
        {"-7709321041217",   "16320",    32u }
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
    ln2 = mf_new_prec(work_prec);
    if (!sum || !term || !ln2)
        goto cleanup;
    if (mfloat_compute_ln2(ln2, work_prec) != 0)
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

    for (k = 0; k < sizeof(coeffs) / sizeof(coeffs[0]); ++k) {
        corr = mfloat_clone_prec(MF_ONE, work_prec);
        if (!corr)
            goto cleanup;
        if (mf_ldexp(corr, -(int)(shift_bits * coeffs[k].power)) != 0)
            goto cleanup;
        if (mfloat_apply_rational(corr, coeffs[k].num, coeffs[k].den, work_prec) != 0)
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
    return mfloat_create_constant_cached(
        "3.14159265358979323846264338327950288419716939937510"
        "58209749445923078164062862089986280348253421170679"
        "82148086513282306647093844609550582231725359408128",
        mfloat_compute_pi,
        &mfloat_cached_pi,
        &mfloat_cached_pi_prec);
}

mfloat_t *mf_e(void)
{
    return mfloat_create_constant_cached(
        "2.71828182845904523536028747135266249775724709369995"
        "95749669676277240766303535475945713821785251664274"
        "27466391932003059921817413596629043572900334295260",
        mfloat_compute_e,
        &mfloat_cached_e,
        &mfloat_cached_e_prec);
}

mfloat_t *mf_euler_mascheroni(void)
{
    return mfloat_create_constant_cached(
        "0.57721566490153286060651209008240243104215933593992"
        "35988057672348848677267776646709369470632917467495"
        "14631447249807082480960504014486542836224173997644",
        mfloat_compute_euler_gamma,
        &mfloat_cached_gamma,
        &mfloat_cached_gamma_prec);
}

mfloat_t *mf_max(void)
{
    return mfloat_from_seed_or_null("1.7976931348623157081452742373170436e308",
                                    mfloat_get_default_precision_internal());
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
    ln2 = mf_new_prec(work_prec);
    if (!x || !ln2)
        goto cleanup;
    if (mfloat_copy_cached_constant(ln2,
                                    &mfloat_cached_ln2,
                                    &mfloat_cached_ln2_prec,
                                    work_prec,
                                    mfloat_compute_ln2) != 0)
        goto cleanup;

    xd = mf_to_double(x);
    if (isnan(xd))
        return mf_set_double(mfloat, NAN);
    if (isinf(xd))
        return mf_set_double(mfloat, xd > 0.0 ? INFINITY : 0.0);

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
    mfloat_t *m = NULL, *u = NULL, *u2 = NULL, *y = NULL, *term = NULL;
    mfloat_t *piece = NULL, *ln2 = NULL;
    double md;
    int rc = -1;

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
    mant_bits = mi_bit_length(mfloat->mantissa);
    exp2 = mfloat->exponent2 + (long)mant_bits - 1l;
    mant = mi_clone(mfloat->mantissa);
    m = mf_new_prec(work_prec);
    if (!mant || !m)
        goto cleanup;
    if (mfloat_set_from_signed_mint(m, mant, -((long)mant_bits - 1l)) != 0)
        goto cleanup;
    mant = NULL;

    md = mf_to_double(m);
    if (!(md > 0.0))
        return mf_set_double(mfloat, NAN);
    u = mf_clone(m);
    term = mf_clone(m);
    if (!u || !term)
        goto cleanup;
    if (mf_add_long(u, -1) != 0)
        goto cleanup;
    if (mf_add_long(term, 1) != 0)
        goto cleanup;
    if (mf_div(u, term) != 0)
        goto cleanup;
    mf_free(term);
    term = NULL;

    y = mf_clone(u);
    term = mf_clone(u);
    u2 = mf_clone(u);
    if (!y || !term || !u2)
        goto cleanup;
    if (mf_mul(u2, u) != 0)
        goto cleanup;

    for (long k = 1; k < LONG_MAX / 2; ++k) {
        long denom = 2l * k + 1l;

        if (mf_mul(term, u2) != 0)
            goto cleanup;
        piece = mf_clone(term);
        if (!piece)
            goto cleanup;
        if (mfloat_div_long_inplace(piece, denom) != 0)
            goto cleanup;
        if (mf_add(y, piece) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(piece, (long)work_prec + 8l))
            break;
        mf_free(piece);
        piece = NULL;
    }
    mf_free(piece);
    piece = NULL;

    if (mf_mul_long(y, 2) != 0)
        goto cleanup;

    if (exp2 != 0) {
        ln2 = mf_new_prec(work_prec);
        if (!ln2)
            goto cleanup;
        if (mfloat_copy_cached_constant(ln2,
                                        &mfloat_cached_ln2,
                                        &mfloat_cached_ln2_prec,
                                        work_prec,
                                        mfloat_compute_ln2) != 0)
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
    mf_free(m);
    mf_free(u);
    mf_free(u2);
    mf_free(y);
    mf_free(term);
    mf_free(piece);
    mf_free(ln2);
    return rc;
}

int mf_pow(mfloat_t *mfloat, const mfloat_t *exponent)
{
    mfloat_t *tmp = NULL;
    long exp_long = 0;
    double ed;

    if (!mfloat || !exponent)
        return -1;

    if (mfloat_is_finite(exponent) &&
        mi_fits_long(exponent->mantissa) &&
        exponent->exponent2 == 0 &&
        mi_get_long(exponent->mantissa, &exp_long)) {
        if (exponent->sign < 0) {
            exp_long = -exp_long;
        }
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
    work_prec = precision + MFLOAT_CONST_GUARD_BITS;
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
    work_prec = precision + MFLOAT_CONST_GUARD_BITS;
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
    return mfloat_apply_qfloat_binary(mfloat, other, qf_atan2);
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
    work_prec = mfloat_transcendental_work_prec(precision);

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
        return 0;
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
    return mfloat_apply_qfloat_unary(mfloat, qf_gamma);
}

int mf_erf(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_erf);
}

int mf_erfc(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_erfc);
}

int mf_erfinv(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_erfinv);
}

int mf_erfcinv(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_erfcinv);
}

int mf_lgamma(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_lgamma);
}

int mf_digamma(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_digamma);
}

int mf_trigamma(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_trigamma);
}

int mf_tetragamma(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_tetragamma);
}

int mf_gammainv(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_gammainv);
}

int mf_lambert_w0(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_lambert_w0);
}

int mf_lambert_wm1(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_lambert_wm1);
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
    return mfloat_apply_qfloat_binary(mfloat, other, qf_logbeta);
}

int mf_binomial(mfloat_t *mfloat, const mfloat_t *other)
{
    return mfloat_apply_qfloat_binary(mfloat, other, qf_binomial);
}

int mf_beta_pdf(mfloat_t *mfloat, const mfloat_t *a, const mfloat_t *b)
{
    return mfloat_apply_qfloat_ternary(mfloat, a, b, qf_beta_pdf);
}

int mf_logbeta_pdf(mfloat_t *mfloat, const mfloat_t *a, const mfloat_t *b)
{
    return mfloat_apply_qfloat_ternary(mfloat, a, b, qf_logbeta_pdf);
}

int mf_normal_pdf(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_normal_pdf);
}

int mf_normal_cdf(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_normal_cdf);
}

int mf_normal_logpdf(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_normal_logpdf);
}

int mf_productlog(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_productlog);
}

int mf_gammainc_lower(mfloat_t *mfloat, const mfloat_t *other)
{
    return mfloat_apply_qfloat_binary(mfloat, other, qf_gammainc_lower);
}

int mf_gammainc_upper(mfloat_t *mfloat, const mfloat_t *other)
{
    return mfloat_apply_qfloat_binary(mfloat, other, qf_gammainc_upper);
}

int mf_gammainc_P(mfloat_t *mfloat, const mfloat_t *other)
{
    size_t precision, work_prec;
    long s_long = 0;
    mfloat_t *x = NULL, *tmp = NULL;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_binary(mfloat, other, qf_gammainc_P);

    if (mfloat_is_finite(mfloat) &&
        mfloat->sign > 0 &&
        mfloat->exponent2 == 0 &&
        mi_fits_long(mfloat->mantissa) &&
        mi_get_long(mfloat->mantissa, &s_long) &&
        s_long == 1) {
        work_prec = mfloat_transcendental_work_prec(precision);
        x = mfloat_clone_prec(other, work_prec);
        tmp = mfloat_clone_prec(other, work_prec);
        if (!x || !tmp)
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

    return mfloat_apply_qfloat_binary(mfloat, other, qf_gammainc_P);

cleanup:
    mf_free(x);
    mf_free(tmp);
    return rc;
}

int mf_gammainc_Q(mfloat_t *mfloat, const mfloat_t *other)
{
    return mfloat_apply_qfloat_binary(mfloat, other, qf_gammainc_Q);
}

int mf_ei(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_ei);
}

int mf_e1(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_e1);
}
