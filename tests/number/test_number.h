#ifndef TEST_NUMBER_H
#define TEST_NUMBER_H

#include <stddef.h>

#include "number.h"
#include "test_harness.h"
#include "ustring.h"

const test_validity_contract_t *number_validity_contract_exact(void);

void assert_number_string(const char *label,
                          number_t number,
                          const char *expected_text);
void assert_number_string_prefix(const char *label,
                                 number_t number,
                                 const char *expected_prefix);

void run_number_parse_tests(void);
void run_number_exact_backend_tests(void);
void run_number_fixed_precision_tests(void);
void run_number_multiprecision_tests(void);
void run_number_promotion_tests(void);
void run_number_constant_tests(void);
void run_number_public_api_tests(void);
void run_number_formatting_tests(void);
void run_number_special_function_tests(void);
void run_number_backend_parity_tests(void);
void run_number_readme_example_tests(void);
void run_number_readme_mersenne_prime_search(void);

#define TEST_ASSERT_NUMBER_EQ(actual, expected) \
    do { \
        number_t test_number_actual__ = (actual); \
        number_t test_number_expected__ = (expected); \
        TEST_ASSERT_VALID_NAMED("number-exact", \
                                &test_number_actual__, \
                                &test_number_expected__); \
    } while (0)

#define ASSERT_NUMBER_EQ(actual, expected) \
    TEST_ASSERT_NUMBER_EQ((actual), (expected))

#endif
