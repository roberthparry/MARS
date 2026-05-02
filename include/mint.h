#ifndef MINT_H
#define MINT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @file mint.h
 * @brief Multiprecision integers.
 *
 * The @c mint_t type stores signed integers of arbitrary size using a private
 * limb representation. Callers interact with it through this API only; the
 * concrete layout is intentionally opaque.
 */

/**
 * @brief Opaque multiprecision integer type.
 */
typedef struct _mint_t mint_t;

/**
 * @brief One prime factor together with its multiplicity.
 */
typedef struct mint_factor_t {
    mint_t *prime;           /**< Owned prime factor value. */
    unsigned long exponent;  /**< Exponent of @ref prime. */
} mint_factor_t;

/**
 * @brief Prime factorisation result.
 */
typedef struct mint_factors_t {
    size_t count;         /**< Number of entries in @ref items. */
    mint_factor_t *items; /**< Owned array of prime/exponent pairs. */
} mint_factors_t;

/**
 * @brief Exact primality classification result.
 */
typedef enum mint_primality_result_t {
    MINT_PRIMALITY_UNKNOWN = -1, /**< No proof either way from current exact methods. */
    MINT_PRIMALITY_COMPOSITE = 0, /**< Proven composite. */
    MINT_PRIMALITY_PRIME = 1      /**< Proven prime. */
} mint_primality_result_t;

/** @name Constants
 * Process-lifetime convenience values owned by the mint subsystem. They must
 * not be modified or freed by callers.
 * @{
 */
extern mint_t *MNT_ZERO;
extern mint_t *MNT_ONE;
extern mint_t *MNT_TEN;
/** @} */

/** @name Lifecycle
 * Allocation, copying, reset, and destruction helpers. Constructors return
 * newly allocated values, `mint_clone()` returns a deep copy, `mint_clear()`
 * resets an existing mutable value to canonical zero, and `mint_free()`
 * releases owned storage. Constructors return `NULL` on allocation or parse
 * failure.
 * @{
 */
mint_t *mint_new(void);
mint_t *mint_create_long(long value);
mint_t *mint_create_ulong(unsigned long value);
mint_t *mint_create_2pow(uint64_t n);
mint_t *mint_create_string(const char *text);
mint_t *mint_create_hex(const char *text);
mint_t *mint_clone(const mint_t *mint);
void mint_free(mint_t *mint);
void mint_clear(mint_t *mint);
/** @} */

/** @name Core arithmetic and comparison
 * In-place signed integer arithmetic. The first parameter is mutated on
 * success. Functions return `0` on success and `-1` on error unless stated
 * otherwise. Division truncates toward zero; if a remainder output is supplied
 * it receives the matching signed remainder.
 * @{
 */
int mint_inc(mint_t *mint);
int mint_dec(mint_t *mint);
int mint_add(mint_t *mint, const mint_t *other);
int mint_add_long(mint_t *mint, long value);
int mint_sub(mint_t *mint, const mint_t *other);
int mint_sub_long(mint_t *mint, long value);
int mint_mul(mint_t *mint, const mint_t *other);
int mint_mul_long(mint_t *mint, long value);
int mint_div(mint_t *mint, const mint_t *other, mint_t *rem);
int mint_div_long(mint_t *mint, long value, long *rem);
int mint_divmod(const mint_t *numerator, const mint_t *denominator,
                mint_t *quotient, mint_t *remainder);
int mint_mod(mint_t *mint, const mint_t *other);
int mint_mod_long(mint_t *mint, long value);
int mint_neg(mint_t *mint);
int mint_abs(mint_t *mint);
int mint_cmp(const mint_t *a, const mint_t *b);
int mint_cmp_long(const mint_t *mint, long value);
/** @} */

/** @name Predicates and checked conversions
 * Small query helpers. Predicates return `true` or `false`. Checked conversion
 * helpers report whether the value fits in a native `long`; on success
 * `mint_get_long()` stores the converted value in `*out`. `mint_bit_length()`
 * returns the number of significant bits in the magnitude, and
 * `mint_test_bit()` reports whether a zero-based bit in the magnitude is set.
 * @{
 */
bool mint_is_zero(const mint_t *mint);
bool mint_is_negative(const mint_t *mint);
bool mint_is_even(const mint_t *mint);
bool mint_is_odd(const mint_t *mint);
size_t mint_bit_length(const mint_t *mint);
bool mint_test_bit(const mint_t *mint, size_t bit_index);
bool mint_fits_long(const mint_t *mint);
bool mint_get_long(const mint_t *mint, long *out);
/** @} */

/** @name Bitwise operations and shifts
 * In-place bitwise and shift operations. Bitwise operators currently accept
 * only non-negative operands. `mint_not()` inverts only the active bit-width
 * of the current value. Bit setters and clearers address zero-based bits in
 * the magnitude. Negative shift counts delegate to the opposite direction.
 * @{
 */
int mint_and(mint_t *mint, const mint_t *other);
int mint_set_bit(mint_t *mint, size_t bit_index);
int mint_clear_bit(mint_t *mint, size_t bit_index);
int mint_not(mint_t *mint);
int mint_or(mint_t *mint, const mint_t *other);
int mint_shl(mint_t *mint, long bits);
int mint_shr(mint_t *mint, long bits);
int mint_xor(mint_t *mint, const mint_t *other);
/** @} */

/** @name Powers and roots
 * In-place power, modular power, decimal-scaling, and integer square-root
 * helpers. Exponents for `mint_pow()` and `mint_pow10()` are non-negative.
 * `mint_powmod()` requires a non-negative exponent and positive modulus.
 * `mint_sqrt()` replaces the value with `floor(sqrt(n))` and fails on
 * negative input.
 * @{
 */
int mint_square(mint_t *mint);
int mint_pow(mint_t *mint, unsigned long exponent);
int mint_powmod(mint_t *mint, const mint_t *exponent, const mint_t *modulus);
int mint_pow10(mint_t *mint, long exponent);
int mint_sqrt(mint_t *mint);
/** @} */

/** @name Parsing and formatting
 * Decimal and hexadecimal text conversion together with scalar setters.
 * `mint_set_string()` accepts leading and trailing whitespace, an optional
 * leading sign, and leading zeroes. `mint_set_hex()` accepts the same
 * whitespace/sign rules together with an optional `0x` or `0X` prefix.
 * `mint_to_string()` and `mint_to_hex()` return newly allocated text that
 * must be freed by the caller with `free()`.
 * @{
 */
int mint_set_long(mint_t *mint, long value);
int mint_set_ulong(mint_t *mint, unsigned long value);
int mint_set_string(mint_t *mint, const char *text);
int mint_set_hex(mint_t *mint, const char *text);
char *mint_to_string(const mint_t *mint);
char *mint_to_hex(const mint_t *mint);
/** @} */

/** @name Number theory
 * Primality testing and factorisation. `mint_isprime()` is a practical
 * probable-prime predicate. `mint_prove_prime()` is the exact proof-oriented
 * companion: it returns proven prime/composite when the current exact methods
 * can certify the result, otherwise `MINT_PRIMALITY_UNKNOWN`. The current
 * exact engine combines native-range sieving, Lucas-Lehmer for Mersenne
 * numbers, partial-factor Pocklington proofs, and a limited `n + 1`
 * supersingular elliptic-curve certificate path. `mint_factors()` returns
 * owned prime/exponent pairs for positive inputs; the result must be released
 * with `mint_factors_free()`. `mint_gcd()` and `mint_lcm()` mutate the first
 * argument in place. `mint_gcdext()` computes `g = gcd(a, b)` together with
 * Bezout coefficients satisfying `a*x + b*y = g`. `mint_modinv()` replaces
 * `mint` with its multiplicative inverse modulo `modulus` when one exists.
 * `mint_nextprime()` advances to the least prime greater than or equal to the
 * current value, with all values below `2` mapping to `2`. `mint_prevprime()`
 * moves to the greatest prime less than or equal to the current value; values
 * below `2` are rejected.
 * @{
 */
bool mint_isprime(const mint_t *mint);
mint_primality_result_t mint_prove_prime(const mint_t *mint);
int mint_nextprime(mint_t *mint);
int mint_prevprime(mint_t *mint);
int mint_gcd(mint_t *mint, const mint_t *other);
int mint_lcm(mint_t *mint, const mint_t *other);
int mint_gcdext(mint_t *g, mint_t *x, mint_t *y,
                const mint_t *a, const mint_t *b);
int mint_modinv(mint_t *mint, const mint_t *modulus);
mint_factors_t *mint_factors(const mint_t *mint);
void mint_factors_free(mint_factors_t *factors);
/** @} */

#endif
