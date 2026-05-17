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

int mi_div_long(mint_t *mint, long value, long *rem)
{
    mpz_t divisor;
    mpz_t remainder;
    int rc = -1;

    if (mint_prepare_mutable(mint) != 0 || value == 0)
        return -1;

    mpz_init_set_si(divisor, value);
    mpz_init(remainder);
    mpz_tdiv_qr(mint->value, remainder, mint->value, divisor);

    if (rem) {
        if (mpz_fits_slong_p(remainder) == 0)
            goto cleanup;
        *rem = mpz_get_si(remainder);
    }

    rc = 0;

cleanup:
    mpz_clear(divisor);
    mpz_clear(remainder);
    return rc;
}

int mi_mod_long(mint_t *mint, long value)
{
    mpz_t divisor;

    if (mint_prepare_mutable(mint) != 0 || value == 0)
        return -1;

    mpz_init_set_si(divisor, value);
    mpz_tdiv_r(mint->value, mint->value, divisor);
    mpz_clear(divisor);
    return 0;
}

int mi_divmod(const mint_t *numerator, const mint_t *denominator,
              mint_t *quotient, mint_t *remainder)
{
    if (!numerator || !denominator || !quotient || !remainder ||
        quotient->constant_id != MICONST_NONE ||
        remainder->constant_id != MICONST_NONE)
        return -1;

    mint_prepare_constant(numerator);
    mint_prepare_constant(denominator);
    if (mpz_sgn(denominator->value) == 0)
        return -1;

    mpz_tdiv_qr(quotient->value, remainder->value,
                numerator->value, denominator->value);
    return 0;
}

int mi_div(mint_t *mint, const mint_t *other, mint_t *rem)
{
    mpz_t remainder;

    if (mint_prepare_mutable(mint) != 0 || !other || rem == mint)
        return -1;
    if (rem && rem->constant_id != MICONST_NONE)
        return -1;

    mint_prepare_constant(other);
    if (mpz_sgn(other->value) == 0)
        return -1;

    if (rem) {
        mpz_tdiv_qr(mint->value, rem->value, mint->value, other->value);
        return 0;
    }

    mpz_init(remainder);
    mpz_tdiv_qr(mint->value, remainder, mint->value, other->value);
    mpz_clear(remainder);
    return 0;
}

int mi_mod(mint_t *mint, const mint_t *other)
{
    if (mint_prepare_mutable(mint) != 0 || !other)
        return -1;
    mint_prepare_constant(other);
    if (mpz_sgn(other->value) == 0)
        return -1;
    mpz_tdiv_r(mint->value, mint->value, other->value);
    return 0;
}
