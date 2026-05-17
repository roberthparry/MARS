#include "mint_internal.h"

static void mint_prepare_constant(const mint_t *mint)
{
    if (mint && mint->constant_id != MICONST_NONE)
        mint_constant_ensure(mint);
}

static int mint_prepare_mutable(mint_t *mint)
{
    if (!mint || mint->constant_id != MICONST_NONE)
        return -1;
    return 0;
}

int mi_inc(mint_t *mint)
{
    return mi_add_long(mint, 1);
}

int mi_dec(mint_t *mint)
{
    return mi_sub_long(mint, 1);
}

int mi_add_long(mint_t *mint, long value)
{
    unsigned long magnitude;

    if (mint_prepare_mutable(mint) != 0)
        return -1;
    if (value >= 0) {
        mpz_add_ui(mint->value, mint->value, (unsigned long)value);
    } else {
        magnitude = (unsigned long)(-(value + 1)) + 1ul;
        mpz_sub_ui(mint->value, mint->value, magnitude);
    }
    return 0;
}

int mi_sub_long(mint_t *mint, long value)
{
    unsigned long magnitude;

    if (mint_prepare_mutable(mint) != 0)
        return -1;
    if (value >= 0) {
        mpz_sub_ui(mint->value, mint->value, (unsigned long)value);
    } else {
        magnitude = (unsigned long)(-(value + 1)) + 1ul;
        mpz_add_ui(mint->value, mint->value, magnitude);
    }
    return 0;
}

int mi_mul_long(mint_t *mint, long value)
{
    if (mint_prepare_mutable(mint) != 0)
        return -1;
    mpz_mul_si(mint->value, mint->value, value);
    return 0;
}

int mi_cmp(const mint_t *a, const mint_t *b)
{
    if (!a || !b)
        return 0;
    mint_prepare_constant(a);
    mint_prepare_constant(b);
    return mpz_cmp(a->value, b->value);
}

int mi_cmp_long(const mint_t *mint, long value)
{
    if (!mint)
        return 0;
    mint_prepare_constant(mint);
    return mpz_cmp_si(mint->value, value);
}

int mi_sub(mint_t *mint, const mint_t *other)
{
    if (mint_prepare_mutable(mint) != 0 || !other)
        return -1;
    mint_prepare_constant(other);
    mpz_sub(mint->value, mint->value, other->value);
    return 0;
}

int mi_abs(mint_t *mint)
{
    if (mint_prepare_mutable(mint) != 0)
        return -1;
    mpz_abs(mint->value, mint->value);
    return 0;
}

bool mi_is_zero(const mint_t *mint)
{
    mint_prepare_constant(mint);
    return !mint || mpz_sgn(mint->value) == 0;
}

bool mi_is_negative(const mint_t *mint)
{
    mint_prepare_constant(mint);
    return mint && mpz_sgn(mint->value) < 0;
}

bool mi_is_even(const mint_t *mint)
{
    mint_prepare_constant(mint);
    return !mint || mpz_even_p(mint->value) != 0;
}

bool mi_is_odd(const mint_t *mint)
{
    mint_prepare_constant(mint);
    return mint && mpz_odd_p(mint->value) != 0;
}

size_t mi_bit_length(const mint_t *mint)
{
    mint_prepare_constant(mint);
    if (!mint || mpz_sgn(mint->value) == 0)
        return 0u;
    return (size_t)mpz_sizeinbase(mint->value, 2);
}

size_t mi_log2(const mint_t *mint)
{
    size_t bits = mi_bit_length(mint);

    return bits == 0u ? 0u : bits - 1u;
}

bool mi_test_bit(const mint_t *mint, size_t bit_index)
{
    mint_prepare_constant(mint);
    if (!mint || mpz_sgn(mint->value) < 0)
        return false;
    return mpz_tstbit(mint->value, (mp_bitcnt_t)bit_index) != 0;
}

bool mi_fits_long(const mint_t *mint)
{
    mint_prepare_constant(mint);
    return mint && mpz_fits_slong_p(mint->value) != 0;
}

bool mi_get_long(const mint_t *mint, long *out)
{
    if (!mint || !out)
        return false;
    mint_prepare_constant(mint);
    if (mpz_fits_slong_p(mint->value) == 0)
        return false;
    *out = mpz_get_si(mint->value);
    return true;
}

int mi_square(mint_t *mint)
{
    if (mint_prepare_mutable(mint) != 0)
        return -1;
    mpz_mul(mint->value, mint->value, mint->value);
    return 0;
}

int mi_add(mint_t *mint, const mint_t *other)
{
    if (mint_prepare_mutable(mint) != 0 || !other)
        return -1;
    mint_prepare_constant(other);
    mpz_add(mint->value, mint->value, other->value);
    return 0;
}

int mi_and(mint_t *mint, const mint_t *other)
{
    if (mint_prepare_mutable(mint) != 0 || !other)
        return -1;
    mint_prepare_constant(other);
    mpz_and(mint->value, mint->value, other->value);
    return 0;
}

int mi_set_bit(mint_t *mint, size_t bit_index)
{
    if (mint_prepare_mutable(mint) != 0)
        return -1;
    mpz_setbit(mint->value, (mp_bitcnt_t)bit_index);
    return 0;
}

int mi_clear_bit(mint_t *mint, size_t bit_index)
{
    if (mint_prepare_mutable(mint) != 0)
        return -1;
    mpz_clrbit(mint->value, (mp_bitcnt_t)bit_index);
    return 0;
}

int mi_mul(mint_t *mint, const mint_t *other)
{
    if (mint_prepare_mutable(mint) != 0 || !other)
        return -1;
    mint_prepare_constant(other);
    mpz_mul(mint->value, mint->value, other->value);
    return 0;
}

int mi_neg(mint_t *mint)
{
    if (mint_prepare_mutable(mint) != 0)
        return -1;
    mpz_neg(mint->value, mint->value);
    return 0;
}

int mi_not(mint_t *mint)
{
    mp_bitcnt_t bits;
    mpz_t mask;

    if (mint_prepare_mutable(mint) != 0 || mpz_sgn(mint->value) < 0)
        return -1;

    bits = mpz_sizeinbase(mint->value, 2);
    if (bits == 0) {
        mpz_set_ui(mint->value, 1u);
        return 0;
    }

    mpz_init(mask);
    mpz_set_ui(mask, 1u);
    mpz_mul_2exp(mask, mask, bits);
    mpz_sub_ui(mask, mask, 1u);
    mpz_xor(mint->value, mint->value, mask);
    mpz_clear(mask);
    return 0;
}

int mi_or(mint_t *mint, const mint_t *other)
{
    if (mint_prepare_mutable(mint) != 0 || !other)
        return -1;
    mint_prepare_constant(other);
    mpz_ior(mint->value, mint->value, other->value);
    return 0;
}

int mi_pow(mint_t *mint, unsigned long exponent)
{
    if (mint_prepare_mutable(mint) != 0)
        return -1;
    mpz_pow_ui(mint->value, mint->value, exponent);
    return 0;
}

int mi_powmod(mint_t *mint, const mint_t *exponent, const mint_t *modulus)
{
    if (mint_prepare_mutable(mint) != 0 || !exponent || !modulus)
        return -1;
    mint_prepare_constant(exponent);
    mint_prepare_constant(modulus);
    if (mpz_sgn(exponent->value) < 0 || mpz_sgn(modulus->value) <= 0)
        return -1;
    mpz_powm(mint->value, mint->value, exponent->value, modulus->value);
    return 0;
}

int mi_pow10(mint_t *mint, long exponent)
{
    mpz_t factor;

    if (mint_prepare_mutable(mint) != 0 || exponent < 0)
        return -1;

    mpz_init(factor);
    mpz_ui_pow_ui(factor, 10u, (unsigned long)exponent);
    mpz_mul(mint->value, mint->value, factor);
    mpz_clear(factor);
    return 0;
}

int mi_shl(mint_t *mint, long bits)
{
    if (mint_prepare_mutable(mint) != 0)
        return -1;
    if (bits < 0)
        return mi_shr(mint, -bits);
    mpz_mul_2exp(mint->value, mint->value, (mp_bitcnt_t)bits);
    return 0;
}

int mi_shr(mint_t *mint, long bits)
{
    if (mint_prepare_mutable(mint) != 0)
        return -1;
    if (bits < 0)
        return mi_shl(mint, -bits);
    mpz_tdiv_q_2exp(mint->value, mint->value, (mp_bitcnt_t)bits);
    return 0;
}

int mi_sqrt(mint_t *mint)
{
    if (mint_prepare_mutable(mint) != 0 || mpz_sgn(mint->value) < 0)
        return -1;
    mpz_sqrt(mint->value, mint->value);
    return 0;
}

int mi_xor(mint_t *mint, const mint_t *other)
{
    if (mint_prepare_mutable(mint) != 0 || !other)
        return -1;
    mint_prepare_constant(other);
    mpz_xor(mint->value, mint->value, other->value);
    return 0;
}
