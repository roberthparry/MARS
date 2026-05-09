#ifndef TEST_NUMBER_H
#define TEST_NUMBER_H

#include <stddef.h>

#include "number.h"
#include "test_harness.h"

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

#endif
