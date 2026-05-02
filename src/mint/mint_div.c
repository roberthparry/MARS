#include "mint_internal.h"

#include <limits.h>
#include <stdlib.h>
int mi_div_long(mint_t *mint, long value, long *rem)
{
    unsigned long magnitude;
    short qsign;
    short rsign;
    uint64_t rem_mag;

    if (!mint || value == 0)
        return -1;
    if (mint_is_immortal(mint))
        return -1;
    if (mint_is_zero_internal(mint)) {
        if (rem)
            *rem = 0;
        return 0;
    }
    if (value == 1) {
        if (rem)
            *rem = 0;
        return 0;
    }
    if (value == -1) {
        if (rem)
            *rem = 0;
        return mi_neg(mint);
    }

    magnitude = value < 0 ? (unsigned long)(-(value + 1)) + 1ul
                          : (unsigned long)value;
    qsign = mint->sign == (value < 0 ? -1 : 1) ? 1 : -1;
    rsign = mint->sign;
    rem_mag = mint_div_word_inplace(mint, (uint64_t)magnitude);
    mint->sign = mint_is_zero_internal(mint) ? 0 : qsign;

    if (rem) {
        if (rem_mag > (uint64_t)LONG_MAX + (rsign < 0 ? 1ull : 0ull))
            return -1;
        *rem = (long)rem_mag;
        if (rsign < 0)
            *rem = -*rem;
    }

    return 0;
}

int mi_mod_long(mint_t *mint, long value)
{
    unsigned long magnitude;
    short sign;
    uint64_t rem_mag;

    if (!mint || value == 0)
        return -1;
    if (mint_is_immortal(mint))
        return -1;
    if (mint_is_zero_internal(mint))
        return 0;

    magnitude = value < 0 ? (unsigned long)(-(value + 1)) + 1ul
                          : (unsigned long)value;
    sign = mint->sign;
    rem_mag = mint_mod_u64(mint, (uint64_t)magnitude);
    return mint_set_magnitude_u64(mint, rem_mag, rem_mag == 0 ? 0 : sign);
}
int mi_divmod(const mint_t *numerator, const mint_t *denominator,
                mint_t *quotient, mint_t *remainder)
{
    if (!quotient || !remainder || !numerator || !denominator)
        return -1;
    if (quotient == remainder)
        return -1;
    if (mint_is_immortal(quotient) || mint_is_immortal(remainder))
        return -1;
    if (mint_copy_value(quotient, numerator) != 0)
        return -1;
    return mint_div_inplace(quotient, denominator, remainder);
}
int mint_div_inplace(mint_t *mint, const mint_t *other, mint_t *rem)
{
    mint_t *numerator, *denominator, *quotient, *remainder;
    short qsign, rsign;
    size_t power_exponent;

    if (!mint || !other || mint_is_immortal(mint) || rem == mint)
        return -1;
    if (rem && mint_is_immortal(rem))
        return -1;
    if (mint_is_zero_internal(other))
        return -1;
    if (mint_is_zero_internal(mint)) {
        if (rem)
            mi_clear(rem);
        return 0;
    }
    if (mint_is_abs_one(other)) {
        if (other->sign < 0 && mint_neg_inplace(mint) != 0)
            return -1;
        if (rem)
            mi_clear(rem);
        return 0;
    }
    if (mint_detect_power_of_two_exponent(other, &power_exponent)) {
        short original_sign = mint->sign;

        if (rem) {
            if (mint_copy_value(rem, mint) != 0)
                return -1;
            mint_keep_low_bits(rem, power_exponent);
            rem->sign = mint_is_zero_internal(rem) ? 0 : original_sign;
        }
        if (mint_shr_inplace(mint, (long)power_exponent) != 0)
            return -1;
        if (!mint_is_zero_internal(mint) && other->sign < 0)
            mint->sign = (short)-mint->sign;
        return 0;
    }
    if (other->length == 1) {
        uint64_t rem_mag = mint_div_word_inplace(mint, other->storage[0]);

        qsign = mint_is_zero_internal(mint) ? 0 : (mint->sign == other->sign ? 1 : -1);
        rsign = rem_mag == 0 ? 0 : mint->sign;
        mint->sign = qsign;
        if (rem && mint_set_magnitude_u64(rem, rem_mag, rsign) != 0)
            return -1;
        return 0;
    }

    numerator = mint_dup_value(mint);
    denominator = mint_dup_value(other);
    quotient = mi_new();
    remainder = mi_new();
    if (!numerator || !denominator || !quotient || !remainder) {
        mi_free(numerator);
        mi_free(denominator);
        mi_free(quotient);
        mi_free(remainder);
        return -1;
    }

    numerator->sign = numerator->length == 0 ? 0 : 1;
    denominator->sign = denominator->length == 0 ? 0 : 1;

    if (mint_div_abs(numerator, denominator, quotient, remainder) != 0) {
        mi_free(numerator);
        mi_free(denominator);
        mi_free(quotient);
        mi_free(remainder);
        return -1;
    }

    qsign = mint_is_zero_internal(quotient) ? 0 : (mint->sign == other->sign ? 1 : -1);
    rsign = mint_is_zero_internal(remainder) ? 0 : mint->sign;

    if (mint_copy_value(mint, quotient) != 0) {
        mi_free(numerator);
        mi_free(denominator);
        mi_free(quotient);
        mi_free(remainder);
        return -1;
    }
    mint->sign = qsign;

    if (rem) {
        if (mint_copy_value(rem, remainder) != 0) {
            mi_free(numerator);
            mi_free(denominator);
            mi_free(quotient);
            mi_free(remainder);
            return -1;
        }
        rem->sign = rsign;
    }

    mi_free(numerator);
    mi_free(denominator);
    mi_free(quotient);
    mi_free(remainder);
    return 0;
}

int mint_mod_inplace(mint_t *mint, const mint_t *other)
{
    mint_t *numerator;
    mint_t *denominator;
    uint64_t rem_mag;
    size_t power_exponent;
    short sign;

    if (!mint || !other || mint_is_immortal(mint))
        return -1;
    if (mint_is_zero_internal(other))
        return -1;
    if (mint_is_zero_internal(mint))
        return 0;
    if (mint_is_abs_one(other)) {
        mi_clear(mint);
        return 0;
    }
    if (mint_detect_power_of_two_exponent(other, &power_exponent)) {
        short sign = mint->sign;

        mint_keep_low_bits(mint, power_exponent);
        mint->sign = mint_is_zero_internal(mint) ? 0 : sign;
        return 0;
    }
    if (other->length == 1) {
        rem_mag = mint_mod_u64(mint, other->storage[0]);
        return mint_set_magnitude_u64(mint, rem_mag, rem_mag == 0 ? 0 : mint->sign);
    }

    sign = mint->sign;
    numerator = mint_dup_value(mint);
    denominator = mint_dup_value(other);
    if (!numerator || !denominator) {
        mi_free(numerator);
        mi_free(denominator);
        return -1;
    }

    numerator->sign = numerator->length == 0 ? 0 : 1;
    denominator->sign = denominator->length == 0 ? 0 : 1;

    if (mint_mod_abs(numerator, denominator, mint) != 0) {
        mi_free(numerator);
        mi_free(denominator);
        return -1;
    }

    if (!mint_is_zero_internal(mint))
        mint->sign = sign;

    mi_free(numerator);
    mi_free(denominator);
    return 0;
}
int mi_div(mint_t *mint, const mint_t *other, mint_t *rem)
{
    return mint_div_inplace(mint, other, rem);
}

int mi_mod(mint_t *mint, const mint_t *other)
{
    return mint_mod_inplace(mint, other);
}
