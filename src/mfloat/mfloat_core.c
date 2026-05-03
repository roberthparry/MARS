#include "mfloat_internal.h"
#include "mfloat_coeff_tables.h"
#include "internal/mint_layout.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

static struct _mfloat_t mfloat_zero_static = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 0,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .flags = MFLOAT_FLAG_IMMORTAL,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_one_static = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 1,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .flags = MFLOAT_FLAG_IMMORTAL,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_half_static = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 1,
    .exponent2 = -1,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .flags = MFLOAT_FLAG_IMMORTAL,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_ten_static = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 1,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .flags = MFLOAT_FLAG_IMMORTAL,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_nan_static = {
    .kind = MFLOAT_KIND_NAN,
    .sign = 0,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .flags = MFLOAT_FLAG_IMMORTAL,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_inf_static = {
    .kind = MFLOAT_KIND_POSINF,
    .sign = 1,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .flags = MFLOAT_FLAG_IMMORTAL,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_ninf_static = {
    .kind = MFLOAT_KIND_NEGINF,
    .sign = -1,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .flags = MFLOAT_FLAG_IMMORTAL,
    .mantissa = NULL
};

static uint64_t mfloat_pi1024_storage[] = {
    0x3d35b9718e0cf535u, 0x9ede1b041f40a3e8u, 0x0a5545d68b8d4371u, 0xc5a5d4214278efa7u,
    0x1da034b3f62b2fb8u, 0xaf1edb0c0ffbae1cu, 0x066a4b776d5c5b6cu, 0x26f2e4a1b7fb424eu,
    0x2ac694de72f028ffu, 0xe76b027cfdfe66edu, 0xb65ed0f4dd5ac6a3u, 0x8a8bb965e3d9abe9u,
    0xe527b386ffa1ca2du, 0xf9726bf74082a81fu, 0x8cff46ca9a52c8abu, 0xc8d4711b3d85cad1u,
    0x99ebf06caba47b91u, 0xba734d22c7f51fa4u, 0x77e0b31b4906c38au, 0x8b67b8400f97142cu,
    0x63fcc9250cca3d9cu, 0x33f4b5d3e4822f89u, 0xd6fdbdc70d7f6b51u, 0xfdad617feb96de80u,
    0xcfd8de89885d34c6u, 0x3848bc90b6aecc4bu, 0xe286e9fc26adadaau, 0x48636605614dbe4bu,
    0x809bbdf2a33679a7u, 0x73644a29410f31c6u, 0xf98e804177d4c762u, 0x839a252049c1114cu,
    0x18469898cc51701bu, 0x00001921fb54442du
};
static uint64_t mfloat_e1024_storage[] = {
    0x033254b0cb54c1c7u, 0xbe57aef5c19813a0u, 0x4aa859e0bea7863cu, 0x4e7e6e78bcaee1b6u,
    0xc2498c03e9e71ec5u, 0x382c220ba0f2036eu, 0xf7a172c7491a654bu, 0x55efee3358d37eb0u,
    0x9b6ffc4c02d87c91u, 0xb2f9be400b5359bdu, 0x9a7d4aac598d5ae5u, 0xa938dd06579dd3ecu,
    0xe1577ff3ec4900f6u, 0x74fd23017594ab3du, 0xcd7344a9d6dba9dfu, 0x0dae75dd3c5aea8fu,
    0x1c7f772d5b56ec20u, 0x9f6c3a2115297659u, 0x4f2f578156305664u, 0xbce6a6159949e907u,
    0xfd0e43be2b1426d5u, 0xd16efc54d13b5e7du, 0xf926b309e18e1c1cu, 0xa35e76aae26bcfeau,
    0xcdda10ac6caaa7bdu, 0xacac24867ea3ebe0u, 0x8c2f5a7be3dababfu, 0x8ebb1ed0364055d8u,
    0x67df2fa5fc6c6c61u, 0x867f799273b9c493u, 0xa6d2b53c26c8228cu, 0xa79e3b1738b079c5u,
    0x695355fb8ac404e7u, 0x000015bf0a8b1457u
};
static uint64_t mfloat_gamma1024_storage[] = {
    0x07157049d78f1759u, 0xb0cb412d6a55c813u, 0xc781589028601cd8u, 0x939e94e76e4d99dfu,
    0x0956eda56cf63b6bu, 0x9259e420fe33c158u, 0x525f7907b4aa6dffu, 0x41bc162158d7f9c7u,
    0xd5d6ab34ba2e9dbbu, 0xb2c3ea6afdcf66a6u, 0x9c2706d90390affbu, 0x86a4c0f0f2b650a2u,
    0x8d7b054c736113cau, 0x7ecbd38ffe30586du, 0x89727d82448a5db2u, 0xe40c19d18ba0a7fcu,
    0xb427e3f0a19639f5u, 0xfa976d53f9c398f9u, 0x71d1a58550a8f38eu, 0x5dc8979ab0bc2f58u,
    0xfe190fb1f09c609eu, 0xee1bf4a87a87798bu, 0x6aab830275322dadu, 0xcdb3e2a5b4559e26u,
    0xb5ccf6f9efce0552u, 0xebfa9637ae1e3321u, 0xd682390ed19cf5d2u, 0x15f44415aba44c84u,
    0x7c3bb4192732d884u, 0xbfef6392d67e80eau, 0x30064300f7cd1c26u, 0xb2d5a873b30ebd97u,
    0x31e9346f8fe04054u, 0x000024f119f8df6cu
};
static uint64_t mfloat_sqrt_pi1024_storage[] = {
    0x4dd5b0b494a84e99u, 0xe2880092eeb7fc68u, 0xae6871f47474f728u, 0x42a41a74227f42a3u,
    0x7aae307974a2e3b5u, 0x76ecb0cfffbf574au, 0x9591e11b8be9ea26u, 0x7745fb2db2f56be8u,
    0xd68b0eccb4c4effdu, 0x1f75783760dfc140u, 0xcae5bb5523255143u, 0xfb59caff25ca248cu,
    0x7bd95fdce0968675u, 0xe3850d5ac2d20f90u, 0x398503060b2d278bu, 0xbccc9a4092cd1364u,
    0xa26f98db0102ed04u, 0xfdec59b7ca2d74b3u, 0x8e39e9867fed6ebau, 0x6db6048dd0729e22u,
    0x71387b27023d028fu, 0xbecea42e2c5c5e0du, 0x989b8b1c0bc49345u, 0xfa9b140caaa28446u,
    0x6ceb3ed54eb79196u, 0x085d372ebf7c4274u, 0x0d61454912430d29u, 0x31dd1db148511b77u,
    0x7c76eb3639d85078u, 0x51d1bb5dbff5be50u, 0xec94b728402f4fa8u, 0xe4e0ff8e48551bd8u,
    0xdaa9e70ec1483576u, 0x00000716fe246d3bu
};
static uint64_t mfloat_sqrt2_1024_storage[] = {
    0xa6c912abcd7d473du, 0x17116c2a40cbb896u, 0x4ffcd3051a73eb80u, 0x1fd65860c4575948u,
    0xede8e7a76aa772acu, 0x258e1238dd48bbd6u, 0x7b76641560957c6eu, 0x5ca8f7b5b0779173u,
    0xf46912e9d6daa8e7u, 0x15b1606967bb85a2u, 0xbd898ab34086a034u, 0x2ae8b92e295be293u,
    0x2b5c3167727c07b6u, 0x27ecb679944c4e70u, 0xb6b563c0b3636abeu, 0xfb12dc6d12b74f95u,
    0xd0efdf4d3a02cebau, 0x0ca4a81394ab6d8fu, 0xe582eeaa4a089904u, 0x3c84df52f120f836u,
    0x7e9dccb2a634331fu, 0xc4e33c6d5a8a38bbu, 0x8458a460abc722f7u, 0xc337bcab1bc91168u,
    0xf1f4e53059c6011bu, 0xa2768d2202e8742au, 0xc7b4a780487363dfu, 0x3db390f74a85e439u,
    0x8a2c3a8b1fe6fdc8u, 0x399154afc83043abu, 0xba84ced17ac85833u, 0xabe9f1d6f60ba893u,
    0xe6484597d89b3754u, 0x00000b504f333f9du
};
static uint64_t mfloat_tenth_256_storage[] = {
    0xcccccccccccccccdu, 0xccccccccccccccccu, 0xccccccccccccccccu, 0xccccccccccccccccu,
    0x000000000000000cu
};

static struct _mint_t mfloat_pi1024_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_pi1024_storage };
static struct _mint_t mfloat_e1024_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_e1024_storage };
static struct _mint_t mfloat_gamma1024_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_gamma1024_storage };
static struct _mint_t mfloat_sqrt_pi1024_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_sqrt_pi1024_storage };
static struct _mint_t mfloat_sqrt2_1024_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_sqrt2_1024_storage };
static struct _mint_t mfloat_tenth_256_mint = { .sign = 1, .length = 5u, .capacity = 5u, .storage = mfloat_tenth_256_storage };

static struct _mfloat_t mfloat_pi1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2155, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_pi1024_mint };
static struct _mfloat_t mfloat_e1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2155, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_e1024_mint };
static struct _mfloat_t mfloat_gamma1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2158, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_gamma1024_mint };
static struct _mfloat_t mfloat_sqrt_pi1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2154, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_sqrt_pi1024_mint };
static struct _mfloat_t mfloat_sqrt2_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2155, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_sqrt2_1024_mint };
static struct _mfloat_t mfloat_tenth_256_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -263, .precision = 256u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_tenth_256_mint };
static size_t mfloat_default_precision_bits = MFLOAT_DEFAULT_PRECISION_BITS;

const mfloat_t * const MF_ZERO = &mfloat_zero_static;
const mfloat_t * const MF_ONE = &mfloat_one_static;
const mfloat_t * const MF_HALF = &mfloat_half_static;
const mfloat_t * const MF_TENTH = &mfloat_tenth_256_static;
const mfloat_t * const MF_TEN = &mfloat_ten_static;
const mfloat_t * const MF_PI = &mfloat_pi1024_static;
const mfloat_t * const MF_E = &mfloat_e1024_static;
const mfloat_t * const MF_EULER_MASCHERONI = &mfloat_gamma1024_static;
const mfloat_t * const MF_SQRT2 = &mfloat_sqrt2_1024_static;
const mfloat_t * const MF_SQRT_PI = &mfloat_sqrt_pi1024_static;
const mfloat_t * const MF_NAN = &mfloat_nan_static;
const mfloat_t * const MF_INF = &mfloat_inf_static;
const mfloat_t * const MF_NINF = &mfloat_ninf_static;

static void mfloat_init_constants(void)
{
    static int initialised = 0;

    if (initialised)
        return;
    initialised = 1;

    mfloat_zero_static.mantissa = (mint_t *)MI_ZERO;
    mfloat_one_static.mantissa = (mint_t *)MI_ONE;
    mfloat_half_static.mantissa = (mint_t *)MI_ONE;
    mfloat_ten_static.mantissa = (mint_t *)MI_TEN;
    mfloat_nan_static.mantissa = (mint_t *)MI_ZERO;
    mfloat_inf_static.mantissa = (mint_t *)MI_ZERO;
    mfloat_ninf_static.mantissa = (mint_t *)MI_ZERO;
}

__attribute__((constructor))
static void mfloat_init_constants_ctor(void)
{
    mfloat_init_constants();
}

int mfloat_is_immortal(const mfloat_t *mfloat)
{
    return mfloat && (mfloat->flags & MFLOAT_FLAG_IMMORTAL) != 0u;
}

int mfloat_is_finite(const mfloat_t *mfloat)
{
    return mfloat && mfloat->kind == MFLOAT_KIND_FINITE;
}

int mfloat_normalise(mfloat_t *mfloat)
{
    if (!mfloat || !mfloat->mantissa)
        return -1;
    if (!mfloat_is_finite(mfloat))
        return -1;

    if (mi_is_zero(mfloat->mantissa)) {
        mfloat->sign = 0;
        mfloat->exponent2 = 0;
        mfloat->kind = MFLOAT_KIND_FINITE;
        return 0;
    }

    while (mi_is_even(mfloat->mantissa)) {
        if (mi_shr(mfloat->mantissa, 1) != 0)
            return -1;
        mfloat->exponent2++;
    }

    if (mfloat->sign == 0)
        mfloat->sign = 1;
    return 0;
}

int mfloat_round_to_precision_internal(mfloat_t *mfloat, size_t precision)
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

static mfloat_t *mfloat_alloc(size_t precision_bits)
{
    mfloat_t *mfloat = calloc(1, sizeof(*mfloat));

    mfloat_init_constants();

    if (!mfloat)
        return NULL;

    mfloat->mantissa = mi_new();
    if (!mfloat->mantissa) {
        free(mfloat);
        return NULL;
    }

    mfloat->precision = precision_bits > 0 ? precision_bits
                                           : mfloat_default_precision_bits;
    mfloat->kind = MFLOAT_KIND_FINITE;
    mfloat->flags = MFLOAT_FLAG_NONE;
    return mfloat;
}

static int mfloat_set_double_exact(mfloat_t *mfloat, double value)
{
    union {
        double d;
        uint64_t u;
    } bits;
    uint64_t frac;
    uint64_t mantissa_u64;
    int exp_bits;
    long exponent2;

    if (!mfloat || !mfloat->mantissa)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;

    if (isnan(value)) {
        mi_clear(mfloat->mantissa);
        mfloat->kind = MFLOAT_KIND_NAN;
        mfloat->sign = 0;
        mfloat->exponent2 = 0;
        return 0;
    }
    if (isinf(value)) {
        mi_clear(mfloat->mantissa);
        mfloat->kind = value < 0.0 ? MFLOAT_KIND_NEGINF : MFLOAT_KIND_POSINF;
        mfloat->sign = value < 0.0 ? (short)-1 : (short)1;
        mfloat->exponent2 = 0;
        return 0;
    }
    if (value == 0.0) {
        mf_clear(mfloat);
        if (signbit(value))
            mfloat->sign = -0;
        return 0;
    }

    bits.d = value;
    frac = bits.u & ((UINT64_C(1) << 52) - 1u);
    exp_bits = (int)((bits.u >> 52) & 0x7ffu);

    if (exp_bits == 0) {
        mantissa_u64 = frac;
        exponent2 = -1074l;
    } else {
        mantissa_u64 = (UINT64_C(1) << 52) | frac;
        exponent2 = (long)exp_bits - 1023l - 52l;
    }

    if (mi_set_ulong(mfloat->mantissa, (unsigned long)mantissa_u64) != 0)
        return -1;
    mfloat->kind = MFLOAT_KIND_FINITE;
    mfloat->sign = (bits.u >> 63) ? (short)-1 : (short)1;
    mfloat->exponent2 = exponent2;
    return mfloat_normalise(mfloat);
}

int mfloat_copy_value(mfloat_t *dst, const mfloat_t *src)
{
    if (!dst || !src || !dst->mantissa || !src->mantissa)
        return -1;
    if (mi_clear(dst->mantissa), mi_add(dst->mantissa, src->mantissa) != 0)
        return -1;
    dst->kind = src->kind;
    dst->sign = src->sign;
    dst->exponent2 = src->exponent2;
    dst->precision = src->precision;
    return 0;
}

int mfloat_set_from_signed_mint(mfloat_t *dst, mint_t *value, long exponent2)
{
    if (!dst || !value || !dst->mantissa)
        return -1;
    if (mfloat_is_immortal(dst))
        return -1;

    if (mi_is_zero(value)) {
        mf_clear(dst);
        return 0;
    }

    dst->sign = mi_is_negative(value) ? (short)-1 : (short)1;
    if (dst->sign < 0 && mi_abs(value) != 0)
        return -1;

    mi_clear(dst->mantissa);
    if (mi_add(dst->mantissa, value) != 0)
        return -1;
    dst->kind = MFLOAT_KIND_FINITE;
    dst->exponent2 = exponent2;
    return mfloat_normalise(dst);
}

mint_t *mfloat_to_scaled_mint(const mfloat_t *mfloat, long target_exp)
{
    mint_t *value;
    long shift;

    if (!mfloat || !mfloat->mantissa)
        return NULL;
    if (!mfloat_is_finite(mfloat))
        return NULL;

    value = mi_clone(mfloat->mantissa);
    if (!value)
        return NULL;

    shift = mfloat->exponent2 - target_exp;
    if (shift > 0) {
        if (mi_shl(value, shift) != 0) {
            mi_free(value);
            return NULL;
        }
    } else if (shift < 0) {
        if (mi_shr(value, -shift) != 0) {
            mi_free(value);
            return NULL;
        }
    }

    if (mfloat->sign < 0 && mi_neg(value) != 0) {
        mi_free(value);
        return NULL;
    }

    return value;
}

static int mfloat_parse_decimal_components(const char *text,
                                           short *out_sign,
                                           mint_t *digits,
                                           long *out_exp10)
{
    const unsigned char *p = (const unsigned char *)text;
    short sign = 1;
    long frac_digits = 0;
    long exp10 = 0;
    bool seen_digit = false;
    bool seen_dot = false;

    if (!text || !out_sign || !digits || !out_exp10)
        return -1;

    while (isspace(*p))
        ++p;

    if (*p == '+' || *p == '-') {
        if (*p == '-')
            sign = -1;
        ++p;
    }

    if (mi_set_long(digits, 0) != 0)
        return -1;

    while (*p) {
        if (isdigit(*p)) {
            seen_digit = true;
            if (mi_mul_long(digits, 10) != 0 ||
                mi_add_long(digits, (long)(*p - '0')) != 0)
                return -1;
            if (seen_dot)
                frac_digits++;
            ++p;
            continue;
        }
        if (*p == '.' && !seen_dot) {
            seen_dot = true;
            ++p;
            continue;
        }
        break;
    }

    if (!seen_digit)
        return -1;

    if (*p == 'e' || *p == 'E') {
        bool neg_exp = false;
        long parsed = 0;

        ++p;
        if (*p == '+' || *p == '-') {
            neg_exp = (*p == '-');
            ++p;
        }
        if (!isdigit(*p))
            return -1;
        while (isdigit(*p)) {
            if (parsed > (LONG_MAX - 9) / 10)
                return -1;
            parsed = parsed * 10 + (long)(*p - '0');
            ++p;
        }
        exp10 = neg_exp ? -parsed : parsed;
    }

    while (isspace(*p))
        ++p;
    if (*p != '\0')
        return -1;

    *out_sign = sign;
    *out_exp10 = exp10 - frac_digits;
    return 0;
}

static int mfloat_set_from_decimal_parts(mfloat_t *mfloat,
                                         short sign,
                                         mint_t *digits,
                                         long exp10)
{
    mint_t *work = NULL, *factor = NULL, *q = NULL, *r = NULL, *twor = NULL;
    size_t shift_bits;
    int rc = -1;

    if (!mfloat || !digits || !mfloat->mantissa)
        return -1;

    if (mi_is_zero(digits)) {
        mf_clear(mfloat);
        return 0;
    }

    work = mi_clone(digits);
    if (!work)
        goto cleanup;

    if (exp10 >= 0) {
        factor = mi_create_long(5);
        if (!factor || mi_pow(factor, (unsigned long)exp10) != 0)
            goto cleanup;
        if (mi_mul(work, factor) != 0)
            goto cleanup;

        mi_clear(mfloat->mantissa);
        if (mi_add(mfloat->mantissa, work) != 0)
            goto cleanup;
        mfloat->kind = MFLOAT_KIND_FINITE;
        mfloat->sign = sign;
        mfloat->exponent2 = exp10;
        rc = mfloat_normalise(mfloat);
        goto cleanup;
    }

    factor = mi_create_long(5);
    if (!factor || mi_pow(factor, (unsigned long)(-exp10)) != 0)
        goto cleanup;

    shift_bits = mfloat->precision + mi_bit_length(factor) + MFLOAT_PARSE_GUARD_BITS;
    if (mi_shl(work, (long)shift_bits) != 0)
        goto cleanup;

    q = mi_new();
    r = mi_new();
    if (!q || !r)
        goto cleanup;
    if (mi_divmod(work, factor, q, r) != 0)
        goto cleanup;

    twor = mi_clone(r);
    if (!twor || mi_mul_long(twor, 2) != 0)
        goto cleanup;
    if (mi_cmp(twor, factor) >= 0) {
        if (mi_inc(q) != 0)
            goto cleanup;
    }

    mi_clear(mfloat->mantissa);
    if (mi_add(mfloat->mantissa, q) != 0)
        goto cleanup;
    mfloat->kind = MFLOAT_KIND_FINITE;
    mfloat->sign = sign;
    mfloat->exponent2 = exp10 - (long)shift_bits;
    rc = mfloat_normalise(mfloat);

cleanup:
    mi_free(work);
    mi_free(factor);
    mi_free(q);
    mi_free(r);
    mi_free(twor);
    return rc;
}

mfloat_t *mf_new(void)
{
    return mf_new_prec(mfloat_default_precision_bits);
}

mfloat_t *mf_new_prec(size_t precision_bits)
{
    mfloat_init_constants();
    return mfloat_alloc(precision_bits);
}

size_t mfloat_get_default_precision_internal(void)
{
    return mfloat_default_precision_bits;
}

mfloat_t *mfloat_clone_immortal_prec_internal(const mfloat_t *src, size_t precision)
{
    mfloat_t *dst;

    if (!src)
        return NULL;
    dst = mf_new_prec(precision);
    if (!dst)
        return NULL;
    if (mfloat_copy_value(dst, src) != 0) {
        mf_free(dst);
        return NULL;
    }
    if (precision < src->precision &&
        mfloat_round_to_precision_internal(dst, precision) != 0) {
        mf_free(dst);
        return NULL;
    }
    dst->precision = precision;
    return dst;
}

int mfloat_set_from_immortal_internal(mfloat_t *dst, const mfloat_t *src, size_t precision)
{
    if (src && precision == src->precision) {
        int rc = mfloat_copy_value(dst, src);

        if (rc == 0)
            dst->precision = precision;
        return rc;
    }
    if (src && precision < src->precision) {
        int rc = mfloat_copy_value(dst, src);

        if (rc != 0)
            return rc;
        if (mfloat_round_to_precision_internal(dst, precision) != 0)
            return -1;
        dst->precision = precision;
        return 0;
    }
    mfloat_t *tmp = mfloat_clone_immortal_prec_internal(src, precision);
    int rc;

    if (!tmp)
        return -1;
    rc = mfloat_copy_value(dst, tmp);
    if (rc == 0)
        dst->precision = precision;
    mf_free(tmp);
    return rc;
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

mfloat_t *mf_clone(const mfloat_t *mfloat)
{
    mfloat_t *copy;

    if (!mfloat)
        return NULL;
    copy = mf_new_prec(mfloat->precision);
    if (!copy)
        return NULL;
    if (mfloat_copy_value(copy, mfloat) != 0) {
        mf_free(copy);
        return NULL;
    }
    return copy;
}

void mf_free(mfloat_t *mfloat)
{
    if (!mfloat)
        return;
    if (mfloat_is_immortal(mfloat))
        return;
    mi_free(mfloat->mantissa);
    free(mfloat);
}

void mf_clear(mfloat_t *mfloat)
{
    if (!mfloat || !mfloat->mantissa)
        return;
    if (mfloat_is_immortal(mfloat))
        return;
    mi_clear(mfloat->mantissa);
    mfloat->kind = MFLOAT_KIND_FINITE;
    mfloat->sign = 0;
    mfloat->exponent2 = 0;
}

int mf_set_precision(mfloat_t *mfloat, size_t precision_bits)
{
    if (!mfloat || precision_bits == 0)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;
    mfloat->precision = precision_bits;
    return 0;
}

int mf_set_default_precision(size_t precision_bits)
{
    if (precision_bits == 0)
        return -1;
    mfloat_default_precision_bits = precision_bits;
    return 0;
}

size_t mf_get_default_precision(void)
{
    return mfloat_default_precision_bits;
}

size_t mf_get_precision(const mfloat_t *mfloat)
{
    return mfloat ? mfloat->precision : 0;
}

int mf_set_long(mfloat_t *mfloat, long value)
{
    if (!mfloat || !mfloat->mantissa)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;

    if (value == 0) {
        mf_clear(mfloat);
        return 0;
    }

    if (mi_set_long(mfloat->mantissa, value < 0 ? -value : value) != 0)
        return -1;
    mfloat->kind = MFLOAT_KIND_FINITE;
    mfloat->sign = value < 0 ? (short)-1 : (short)1;
    mfloat->exponent2 = 0;
    return mfloat_normalise(mfloat);
}

int mf_set_double(mfloat_t *mfloat, double value)
{
    return mfloat_set_double_exact(mfloat, value);
}

int mf_set_qfloat(mfloat_t *mfloat, qfloat_t value)
{
    mfloat_t *tmp = NULL;
    int rc;

    if (!mfloat)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;
    if (qf_isnan(value)) {
        mi_clear(mfloat->mantissa);
        mfloat->kind = MFLOAT_KIND_NAN;
        mfloat->sign = 0;
        mfloat->exponent2 = 0;
        return 0;
    }
    if (qf_isposinf(value)) {
        mi_clear(mfloat->mantissa);
        mfloat->kind = MFLOAT_KIND_POSINF;
        mfloat->sign = 1;
        mfloat->exponent2 = 0;
        return 0;
    }
    if (qf_isneginf(value)) {
        mi_clear(mfloat->mantissa);
        mfloat->kind = MFLOAT_KIND_NEGINF;
        mfloat->sign = -1;
        mfloat->exponent2 = 0;
        return 0;
    }

    rc = mfloat_set_double_exact(mfloat, value.hi);
    if (rc != 0)
        return rc;

    if (value.lo == 0.0)
        return 0;

    tmp = mf_create_double(value.lo);
    if (!tmp)
        return -1;
    if (mf_set_precision(tmp, mfloat->precision) != 0) {
        mf_free(tmp);
        return -1;
    }

    rc = mf_add(mfloat, tmp);
    mf_free(tmp);
    return rc;
}

int mf_set_string(mfloat_t *mfloat, const char *text)
{
    mint_t *digits = NULL;
    short sign = 1;
    long exp10 = 0;
    int rc;

    if (!mfloat || !text)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;

    if (text[0] == 'N' && text[1] == 'A' && text[2] == 'N' && text[3] == '\0') {
        mfloat->kind = MFLOAT_KIND_NAN;
        mfloat->sign = 0;
        mfloat->exponent2 = 0;
        mi_clear(mfloat->mantissa);
        return 0;
    }
    if (text[0] == 'I' && text[1] == 'N' && text[2] == 'F' && text[3] == '\0') {
        mfloat->kind = MFLOAT_KIND_POSINF;
        mfloat->sign = 1;
        mfloat->exponent2 = 0;
        mi_clear(mfloat->mantissa);
        return 0;
    }
    if (text[0] == '-' && text[1] == 'I' && text[2] == 'N' && text[3] == 'F' && text[4] == '\0') {
        mfloat->kind = MFLOAT_KIND_NEGINF;
        mfloat->sign = -1;
        mfloat->exponent2 = 0;
        mi_clear(mfloat->mantissa);
        return 0;
    }

    digits = mi_new();
    if (!digits)
        return -1;

    rc = mfloat_parse_decimal_components(text, &sign, digits, &exp10);
    if (rc == 0)
        rc = mfloat_set_from_decimal_parts(mfloat, sign, digits, exp10);

    mi_free(digits);
    return rc;
}

bool mf_is_zero(const mfloat_t *mfloat)
{
    if (!mfloat_is_finite(mfloat))
        return false;
    return !mfloat || mfloat->sign == 0 || !mfloat->mantissa ||
           mi_is_zero(mfloat->mantissa);
}

short mf_get_sign(const mfloat_t *mfloat)
{
    return mfloat ? mfloat->sign : 0;
}

long mf_get_exponent2(const mfloat_t *mfloat)
{
    return mfloat ? mfloat->exponent2 : 0;
}

size_t mf_get_mantissa_bits(const mfloat_t *mfloat)
{
    if (!mfloat || !mfloat->mantissa)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return 0;
    return mi_bit_length(mfloat->mantissa);
}

bool mf_get_mantissa_u64(const mfloat_t *mfloat, uint64_t *out)
{
    long value;

    if (!mfloat || !out || !mfloat->mantissa || mi_is_negative(mfloat->mantissa))
        return false;
    if (!mfloat_is_finite(mfloat))
        return false;
    if (mi_bit_length(mfloat->mantissa) > (sizeof(long) * 8u - 1u))
        return false;
    if (!mi_get_long(mfloat->mantissa, &value) || value < 0)
        return false;
    *out = (uint64_t)value;
    return true;
}
