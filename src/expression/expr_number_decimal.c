/**
 * @file expr_number_decimal.c
 * @brief Lossless, size-aware decimal alternatives to exact rational numeric atoms.
 *
 * Native output and semantic notation share this presentation decision. A terminating rational may use fixed
 * or scientific decimal notation when that spelling saves at least two characters over its ASCII fraction.
 * Repeating rationals and compact familiar fractions stay fractional. Integer arithmetic preserves every digit;
 * no floating-point approximation or change to the expression graph is involved.
 */
#include <gmp.h>
#include <stdlib.h>
#include <string.h>

#include "number-t.h"
#include "string-t.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"

/* Compare complete spellings before allocating padding or committing to one notation. */
string_t *expr_number_preferred_decimal_local(number_t value)
{
    if (!num_is_exact(value) || !num_is_real(value) || num_is_integer(value))
        return NULL;
    number_t numerator = NUM_NAN, denominator = NUM_NAN;
    if (!num_get_fraction_parts(value, &numerator, &denominator))
        return NULL;
    bool fractional = !num_eq(denominator, NUM_ONE);
    string_t *top = fractional ? num_to_string(numerator) : NULL;
    string_t *bottom = fractional ? num_to_string(denominator) : NULL;
    num_destroy(&numerator);
    num_destroy(&denominator);
    if (!top || !bottom) {
        string_free(top);
        string_free(bottom);
        return NULL;
    }
    string_t *out = expr_rational_preferred_decimal_local(top, bottom);
    string_free(top);
    string_free(bottom);
    return out;
}

/* Binding literals retain exact digits independently of the current numeric working-precision budget. */
string_t *expr_rational_preferred_decimal_local(const string_t *top, const string_t *bottom)
{
    if (!top || !bottom)
        return NULL;
    mpz_t scaled, rest, factor;
    mpz_inits(scaled, rest, factor, NULL);
    bool valid = !mpz_set_str(scaled, string_c_str(top), 10) &&
                 !mpz_set_str(rest, string_c_str(bottom), 10) && mpz_sgn(rest) > 0;
    string_t *out = NULL;
    char *raw = NULL;
    size_t fraction_length = 0;
    if (!valid)
        goto done;
    mpz_gcd(factor, scaled, rest);
    mpz_divexact(scaled, scaled, factor);
    mpz_divexact(rest, rest, factor);
    if (mpz_cmp_ui(rest, 1) == 0)
        goto done;
    char *canonical_top = mpz_get_str(NULL, 10, scaled), *canonical_bottom = mpz_get_str(NULL, 10, rest);
    if (canonical_top && canonical_bottom)
        fraction_length = strlen(canonical_top) + strlen(canonical_bottom) + 1;
    free(canonical_top);
    free(canonical_bottom);
    if (!fraction_length)
        goto done;
    mp_bitcnt_t twos = mpz_scan1(rest, 0);
    mpz_tdiv_q_2exp(rest, rest, twos);
    mpz_set_ui(factor, 5);
    mp_bitcnt_t fives = mpz_remove(rest, rest, factor);
    if (mpz_cmp_ui(rest, 1))
        goto done;
    size_t scale = twos > fives ? twos : fives;
    mpz_mul_2exp(scaled, scaled, scale - twos);
    mpz_ui_pow_ui(factor, 5, scale - fives);
    mpz_mul(scaled, scaled, factor);
    raw = mpz_get_str(NULL, 10, scaled);
    if (!raw)
        goto done;
    bool negative = raw[0] == '-';
    const char *digits = raw + negative;
    size_t length = strlen(digits);
    bool exponent_negative = length <= scale;
    size_t exponent = exponent_negative ? scale - length + 1 : length - scale - 1;
    size_t exponent_digits = 1;
    for (size_t remaining = exponent; remaining >= 10; remaining /= 10)
        ++exponent_digits;
    size_t fixed_length = negative + (length > scale ? length + 1 : scale + 2);
    size_t scientific_length = negative + length + (length > 1) + 1 + exponent_negative + exponent_digits;
    bool scientific = exponent && scientific_length + 2 <= fixed_length;
    size_t chosen_length = scientific ? scientific_length : fixed_length;
    if (chosen_length + 2 > fraction_length)
        goto done;
    out = string_new();
    if (!out || (negative && string_append_char(out, '-') != 0))
        goto failed;
    if (scientific) {
        if (string_append_bytes(out, digits, 1) != 0 ||
            (length > 1 && (string_append_char(out, '.') != 0 ||
                            string_append_bytes(out, digits + 1, length - 1) != 0)) ||
            string_append_format(out, "e%s%zu", exponent_negative ? "-" : "", exponent) < 0)
            goto failed;
    } else if (length > scale) {
        size_t whole = length - scale;
        if (string_append_bytes(out, digits, whole) != 0 || string_append_char(out, '.') != 0 ||
            string_append_bytes(out, digits + whole, scale) != 0)
            goto failed;
    } else {
        if (string_append_cstr(out, "0.") != 0)
            goto failed;
        /* Padding is emitted only after the complete candidate has won the bounded size comparison. */
        for (size_t i = length; i < scale; ++i)
            if (string_append_char(out, '0') != 0)
                goto failed;
        if (string_append_bytes(out, digits, length) != 0)
            goto failed;
    }
    goto done;
failed:
    string_free(out);
    out = NULL;
done:
    free(raw);
    mpz_clears(scaled, rest, factor, NULL);
    return out;
}
