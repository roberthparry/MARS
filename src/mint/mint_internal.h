#ifndef MINT_INTERNAL_H
#define MINT_INTERNAL_H

#include "internal/mint_layout.h"
#include "mint.h"

#define MINT_SIEVE_SEGMENT_ODDS 32768ul
#define MINT_SMALL_PRIMES_COUNT 47u

extern const unsigned long mint_small_primes[MINT_SMALL_PRIMES_COUNT];

typedef enum mint_ec_step_result_t {
    MINT_EC_STEP_OK = 0,
    MINT_EC_STEP_INFINITY = 1,
    MINT_EC_STEP_COMPOSITE = 2,
    MINT_EC_STEP_ERROR = 3
} mint_ec_step_result_t;

typedef struct mint_ec_point_t {
    mint_t *x;
    mint_t *y;
    int infinity;
} mint_ec_point_t;

int mint_is_immortal(const mint_t *mint);
int mint_is_zero_internal(const mint_t *mint);
void mint_normalise(mint_t *mint);
int mint_cmp_abs(const mint_t *a, const mint_t *b);
int mint_has_negative_operand(const mint_t *a, const mint_t *b);
int mint_is_abs_one(const mint_t *mint);
int mint_is_even_internal(const mint_t *mint);
int mint_is_odd_internal(const mint_t *mint);
void mint_zero_spare_limbs(mint_t *mint, size_t from);
int mint_ensure_capacity(mint_t *mint, size_t needed);
int mint_set_magnitude_u64(mint_t *mint, uint64_t magnitude, short sign);
int mint_abs_add_inplace(mint_t *dst, const mint_t *src);
int mint_cmp_abs_u64(const mint_t *mint, uint64_t value);
void mint_abs_sub_inplace(mint_t *dst, const mint_t *src);
void mint_abs_sub_raw_inplace(uint64_t *dst_storage, size_t *dst_length,
                              const uint64_t *src_storage, size_t src_length);
int mint_mul_small(mint_t *mint, uint32_t factor);
int mint_mul_word_inplace(mint_t *mint, uint64_t factor);
void mint_mul_schoolbook_raw(uint64_t *out, const uint64_t *lhs_storage,
                             size_t lhs_length, const uint64_t *rhs_storage,
                             size_t rhs_length);
unsigned mint_clz64(uint64_t value);
uint64_t mint_shift_left_bits_raw(uint64_t *dst, const uint64_t *src,
                                  size_t len, unsigned shift);
void mint_shift_right_bits_raw(uint64_t *dst, const uint64_t *src,
                               size_t len, unsigned shift);
int mint_sub_small(mint_t *mint, uint32_t subtrahend);
int mint_add_small(mint_t *mint, uint32_t addend);
int mint_add_word(mint_t *mint, uint64_t addend);
int mint_sub_word(mint_t *mint, uint64_t subtrahend);
int mint_add_signed_word_inplace(mint_t *mint, uint64_t magnitude, short sign);
uint64_t mint_div_word_inplace(mint_t *mint, uint64_t divisor);
uint32_t mint_div_small_inplace(mint_t *mint, uint32_t divisor);
int mint_hex_digit_value(unsigned char ch);
uint64_t mint_mod_u64(const mint_t *mint, uint64_t divisor);
int mint_mod_positive_inplace(mint_t *mint, const mint_t *modulus);
int mint_half_mod_odd_inplace(mint_t *mint, const mint_t *modulus);
int mint_jacobi_u64(uint64_t a, uint64_t n);
int mint_jacobi_small_over_mint(long a, const mint_t *n);
void mint_ec_point_clear(mint_ec_point_t *point);
int mint_ec_point_init(mint_ec_point_t *point);
int mint_ec_point_copy(mint_ec_point_t *dst, const mint_ec_point_t *src);
mint_ec_step_result_t mint_ec_make_slope(const mint_t *numerator,
                                         const mint_t *denominator,
                                         const mint_t *modulus,
                                         mint_t *slope);
mint_ec_step_result_t mint_ec_add(const mint_ec_point_t *p,
                                  const mint_ec_point_t *q,
                                  const mint_t *a,
                                  const mint_t *modulus,
                                  mint_ec_point_t *out);
mint_ec_step_result_t mint_ec_scalar_mul(const mint_ec_point_t *point,
                                         unsigned long scalar,
                                         const mint_t *a,
                                         const mint_t *modulus,
                                         mint_ec_point_t *out);
mint_ec_step_result_t mint_ec_scalar_mul_big(const mint_ec_point_t *point,
                                             const mint_t *scalar,
                                             const mint_t *a,
                                             const mint_t *modulus,
                                             mint_ec_point_t *out);
int mint_abs_diff(mint_t *dst, const mint_t *a, const mint_t *b);
int mint_copy_value(mint_t *dst, const mint_t *src);
mint_t *mint_dup_value(const mint_t *src);
size_t mint_bit_length_internal(const mint_t *mint);
int mint_get_bit(const mint_t *mint, size_t bit_index);
__uint128_t mint_extract_top_bits(const mint_t *mint, size_t prefix_bits);
uint64_t mint_isqrt_u128(__uint128_t value);
size_t mint_lehmer_collect_quotients(const mint_t *a, const mint_t *b,
                                     unsigned long *qs, size_t max_qs);
int mint_lehmer_apply_quotients(mint_t *a, mint_t *b,
                                const unsigned long *qs, size_t qcount,
                                mint_t *tmp);
uint64_t mint_gcd_u64(uint64_t a, uint64_t b);
int mint_fibonacci_pair(mint_t *fn, mint_t *fn1, unsigned long n);
int mint_set_bit_internal(mint_t *mint, size_t bit_index);
void mint_keep_low_bits(mint_t *mint, size_t bits);
int mint_shift_right_bits_to(const mint_t *src, size_t bits, mint_t *dst);
int mint_mod_mersenne_with_scratch(mint_t *mint, size_t exponent,
                                   const mint_t *modulus, mint_t *scratch);
int mint_div_abs(const mint_t *numerator, const mint_t *denominator,
                 mint_t *quotient, mint_t *remainder);
int mint_mod_abs(const mint_t *numerator, const mint_t *denominator,
                 mint_t *remainder);
int mint_sqrt_initial_guess(mint_t *guess, const mint_t *value);
int mint_get_ulong_if_fits(const mint_t *mint, unsigned long *out);
int mint_isprime_sieved_upto_ulong(const mint_t *mint, unsigned long limit);
int mint_detect_power_of_two_exponent(const mint_t *mint, size_t *exponent_out);
int mint_isprime_small_ulong(unsigned long n);
size_t mint_find_wheel210_index(uint64_t rem);
int mint_adjust_to_next_wheel210(mint_t *mint);
int mint_adjust_to_prev_wheel210(mint_t *mint);
long mint_nextprime_wheel210_step(uint64_t rem);
long mint_prevprime_wheel210_step(uint64_t rem);
int mint_isprime_lucas_lehmer(const mint_t *mersenne, size_t exponent);
int mint_isprime_strong_probable_prime_base2(const mint_t *mint);
int mint_isprime_lucas_selfridge(const mint_t *mint);
mint_primality_result_t mint_prove_prime_pocklington(const mint_t *mint);
mint_primality_result_t mint_prove_prime_ec_nplus1_supersingular(const mint_t *mint);
mint_primality_result_t mint_prove_prime_ec_witness(const mint_t *mint);
int mint_collect_proven_factors_partial(const mint_t *n, mint_factors_t *proven,
                                        mint_t *remaining);
mint_primality_result_t mint_prove_prime_internal(const mint_t *mint);
int mint_factors_append(mint_factors_t *factors, const mint_t *prime,
                        unsigned long exponent);
int mint_factor_item_cmp(const void *lhs, const void *rhs);
void mint_factors_sort_and_compress(mint_factors_t *factors);
int mint_pollard_rho_step(mint_t *state, const mint_t *c, const mint_t *n);
int mint_pollard_rho_factor(const mint_t *n, mint_t *factor);
int mint_factor_recursive(const mint_t *n, mint_factors_t *factors);

int mint_add_inplace(mint_t *mint, const mint_t *other);
int mint_and_inplace(mint_t *mint, const mint_t *other);
int mint_mul_inplace(mint_t *mint, const mint_t *other);
int mint_div_inplace(mint_t *mint, const mint_t *other, mint_t *rem);
int mint_mod_inplace(mint_t *mint, const mint_t *other);
int mint_neg_inplace(mint_t *mint);
int mint_not_inplace(mint_t *mint);
int mint_or_inplace(mint_t *mint, const mint_t *other);
int mint_pow_inplace(mint_t *mint, unsigned long exponent);
int mint_powmod_inplace(mint_t *mint, const mint_t *exponent,
                        const mint_t *modulus);
int mint_pow10_inplace(mint_t *mint, long exponent);
int mint_shl_inplace(mint_t *mint, long bits);
int mint_shr_inplace(mint_t *mint, long bits);
int mint_sqrt_inplace(mint_t *mint);
int mint_xor_inplace(mint_t *mint, const mint_t *other);

#endif
