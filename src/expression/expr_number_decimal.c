/**
 * @file expr_number_decimal.c
 * @brief Lossless, size-aware decimal alternatives to exact rational numeric atoms.
 *
 * Native output and semantic notation share this presentation decision. A terminating rational may use fixed
 * or scientific decimal notation when that spelling saves at least two characters over its ASCII fraction.
 * Repeating rationals and compact familiar fractions stay fractional. Integer arithmetic preserves every digit;
 * no floating-point approximation or change to the expression graph is involved. Exact-coefficient rendering
 * scopes, such as Taylor equations, suppress decimal alternatives without changing ordinary output.
 * Display graph preparation also collects these coefficients beside their factors, distributing a numerical
 * scale over an additive numerator where necessary, without changing the underlying equation or sampling.
 */
#include <gmp.h>
#include <stdlib.h>
#include <string.h>

#include "number-t.h"
#include "string-t.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"

static _Thread_local bool expr_fractional_numbers = false;

static expr_t *decimal_scaled_body(const expr_t *expression, number_t scale, unsigned depth)
{
    if (!expression || depth >= 128)
        return NULL;
    number_t inner = NUM_NAN;
    const expr_t *base = NULL, *left = NULL, *right = NULL;
    bool subtract = false;
    if (expr_match_scaled_expr(expression, &inner, &base) && num_is_exact(inner) && num_is_real(inner)) {
        number_t combined = num_mul(scale, inner);
        expr_t *result = decimal_scaled_body(base, combined, depth + 1);
        num_destroy(&combined);
        num_destroy(&inner);
        return result;
    }
    num_destroy(&inner);
    if (expr_match_add_sub_expr(expression, &left, &right, &subtract)) {
        expr_t *a = decimal_scaled_body(left, scale, depth + 1);
        expr_t *b = decimal_scaled_body(right, scale, depth + 1);
        expr_t *result = a && b ? subtract ? expr_sub(a, b) : expr_add(a, b) : NULL;
        expr_free(b);
        expr_free(a);
        return result;
    }
    if (expr_is_unnamed_const(expression) && num_is_exact(expression->c)) {
        number_t value = num_mul(scale, expression->c);
        expr_t *result = expr_new_const(value);
        num_destroy(&value);
        return result;
    }
    if (num_is_one(scale))
        return expr_retain_expr(expression);
    expr_t *coefficient = expr_new_const(scale);
    expr_t *result = coefficient ? expr_mul(coefficient, expression) : NULL;
    expr_free(coefficient);
    return result;
}

/* Work on one private clone; replacing a scale keeps the unreduced source and all bindings independent. */
static bool decimal_coefficients_owned(expr_t **slot, unsigned depth)
{
    if (!*slot)
        return true;
    if (depth >= 128)
        return false;
    expr_t *expression = *slot;
    const expr_t *base = NULL;
    number_t scale = NUM_NAN;
    bool scaled = expr_match_scaled_expr(expression, &scale, &base);
    bool candidate = scaled && (expr_is_div(expression) ||
                               (expr_is_mul(expression) && expr_is_addsub(base)));
    string_t *decimal = candidate ? expr_number_preferred_decimal_local(scale) : NULL;
    if (decimal) {
        expr_t *replacement = decimal_scaled_body(base, scale, depth + 1);
        if (replacement) {
            expr_free(expression);
            *slot = expression = replacement;
        }
        string_free(decimal);
        num_destroy(&scale);
        if (!replacement)
            return false;
    } else {
        num_destroy(&scale);
    }
    const expr_t *left = NULL, *right = NULL;
    return !expr_child_exprs(expression, &left, &right) ||
           ((!left || decimal_coefficients_owned(&expression->a, depth + 1)) &&
            (!right || decimal_coefficients_owned(&expression->b, depth + 1)));
}

/* Both plain and semantic notation can render this exact coefficient graph without a fraction through a factor. */
expr_t *expr_decimal_coefficients_for_display(const expr_t *expression)
{
    expr_t *result = expr_clone(expression);
    if (result && !decimal_coefficients_owned(&result, 0)) {
        expr_free(result);
        result = NULL;
    }
    return result;
}

/* A caller's exact-coefficient rendering scope never changes another thread's display choices. */
bool expr_set_number_fractional_local(bool fractional)
{
    bool previous = expr_fractional_numbers;
    expr_fractional_numbers = fractional;
    return previous;
}

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
    if (expr_fractional_numbers || !top || !bottom)
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
