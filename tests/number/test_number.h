/**
 * @file test_number.h
 * @brief Shared assertions and sequential entry points for number-module tests.
 *
 * Declares numeric test groups, owned-value assertions and README test entry points. Only number test translation
 * units consume this support header; installed library clients use number-t.h. Groups report through the shared
 * harness and run sequentially. README groups follow ordinary checks, with their displayed output kept in sync.
 */
#ifndef TEST_NUMBER_H
#define TEST_NUMBER_H

#include <stddef.h>

#include "number-t.h"
#include "string-t.h"
#include "test_harness.h"

const test_validity_contract_t *number_validity_contract_exact(void);

void assert_number_string(const char *label, number_t number, const char *want_text);
void assert_number_string_prefix(const char *label, number_t number, const char *want_prefix);

void run_number_parse_tests(void);
void run_number_exact_backend_tests(void);
void run_number_fixed_precision_tests(void);
void run_number_multiprecision_tests(void);
void run_number_promotion_tests(void);
void run_number_constant_tests(void);

/** @brief Check exact physical constants, ownership and requested precision. */
void run_number_physical_constant_tests(void);
void run_number_public_api_tests(void);
void run_number_formatting_tests(void);
void run_number_special_function_tests(void);
/**
 * @brief Check SQL-authored special-function identities at fixed and multiprecision working sizes.
 * @return Nothing; records failures through the test harness. Restores the caller's default precision.
 */
void run_number_benchmark_regressions(void);
void run_number_bessel_k_ownership_tests(void);
void run_number_bessel_y_tests(void);
void run_number_bessel_y_qfloat_ownership_tests(void);
void run_number_bessel_y_qcomplex_ownership_tests(void);
void run_number_bessel_y_scope_lifetime_tests(void);
void run_number_bessel_y_readme_tests(void);
void run_number_nonfinite_series_tests(void);

/**
 * @brief Run stateful number-module cache and conservative-magnitude regressions.
 * @return Nothing; records assertion results through the active harness case.
 * @note Tests exact logarithm keys, signed zeros, precision upgrades, uncached indices and concurrent callers.
 * Owns and releases all temporary MPC values and joined threads. Reports failures through the ordinary test
 * harness; call sequentially with other test groups. No ownership or scratch pointers escape this void function.
 */
void run_number_cache_tests(void);

/**
 * @brief Check every real and complex Appell output alias against independent product identities.
 * @return Nothing; records assertion failures through the active test harness.
 * Owns and clears all MPFR and MPC temporaries. Exercises compact dyadic and full-precision parameters at
 * multiple precisions; call sequentially with other test groups. No input, scratch value or ownership escapes.
 */
void run_number_appell_alias_tests(void);

void run_number_backend_parity_tests(void);
void run_number_readme_example_tests(void);
void run_number_readme_mersenne_prime_search(void);

#define TEST_ASSERT_NUMBER_EQ(got, want)                                                                        \
    do {                                                                                                               \
        number_t test_number_got__ = (got);                                                                      \
        number_t test_number_want__ = (want);                                                                  \
        TEST_ASSERT_VALID_NAMED("number-exact", &test_number_got__, &test_number_want__);                       \
    } while (0)

#define ASSERT_NUMBER_EQ(got, want) TEST_ASSERT_NUMBER_EQ((got), (want))

#endif
