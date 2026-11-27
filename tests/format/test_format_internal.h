/**
 * @file test_format_internal.h
 * @brief Private format-suite contracts for isolated core and owning-header checks.
 *
 * Shares regression entry points between the core-only translation unit, single-type-header fixtures, README
 * examples and the full format suite. This header is local to format tests; ordinary callers use format-t.h or
 * a type's own API.
 */
#ifndef MARS_TEST_FORMAT_INTERNAL_H
#define MARS_TEST_FORMAT_INTERNAL_H

#if !defined(MARS_TEST_FORMAT_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_TEST_FORMAT_INTERNAL_ACCESS."
#endif

/**
 * @brief Check core-only capture, adapter payload lifetime, validation and compiled type identity.
 * @note Takes no parameters or ownership. Reports failures through the ordinary test harness and releases all
 * local templates and text before returning; run sequentially with the rest of the format suite.
 */
void test_format_core(void);

/**
 * @brief Verify interpolation with only qfloat-t.h as the library header.
 * @note Takes no parameters or ownership. Reports failures through the test harness, releases fixture resources
 * and returns after the module-header selection check. Run sequentially with the ordinary suite.
 */
void test_format_qfloat_header(void);

/**
 * @brief Verify interpolation with only qcomplex-t.h as the library header.
 * @note Takes no parameters or ownership. Reports failures through the test harness, releases fixture resources
 * and returns after the module-header selection check. Run sequentially with the ordinary suite.
 */
void test_format_qcomplex_header(void);

/**
 * @brief Verify interpolation with only number-t.h as the library header.
 * @note Takes no parameters or ownership. Reports failures through the test harness, releases fixture resources
 * and returns after the module-header selection check. Run sequentially with the ordinary suite.
 */
void test_format_number_header(void);

/**
 * @brief Verify interpolation with only matrix-t.h as the library header.
 * @note Takes no parameters or ownership. Reports failures through the test harness, releases fixture resources
 * and returns after the module-header selection check. Run sequentially with the ordinary suite.
 */
void test_format_matrix_header(void);

/**
 * @brief Verify interpolation with only expression-t.h as the library header.
 * @note Takes no parameters or ownership. Reports failures through the test harness, releases fixture resources
 * and returns after the module-header selection check. Run sequentially with the ordinary suite.
 */
void test_format_expression_header(void);

/**
 * @brief Run the format guide's named README example and check its result.
 * @note Takes no parameters or ownership. Produces the documented output and reports failures through the test
 * harness; releases local resources before returning. Run after ordinary checks with the other README examples.
 */
void example_format_named(void);

/**
 * @brief Run the format guide's positions README example and check its result.
 * @note Takes no parameters or ownership. Produces the documented output and reports failures through the test
 * harness; releases local resources before returning. Run after ordinary checks with the other README examples.
 */
void example_format_positions(void);

/**
 * @brief Run the format guide's string README example and check its result.
 * @note Takes no parameters or ownership. Produces the documented output and reports failures through the test
 * harness; releases local resources before returning. Run after ordinary checks with the other README examples.
 */
void example_format_string(void);

/**
 * @brief Run the format guide's builder README example and check its result.
 * @note Takes no parameters or ownership. Produces the documented output and reports failures through the test
 * harness; releases local resources before returning. Run after ordinary checks with the other README examples.
 */
void example_format_builder(void);

/**
 * @brief Run the format guide's native README example and check its result.
 * @note Takes no parameters or ownership. Produces the documented output and reports failures through the test
 * harness; releases local resources before returning. Run after ordinary checks with the other README examples.
 */
void example_format_native(void);

/**
 * @brief Run the format guide's compiled README example and check its result.
 * @note Takes no parameters or ownership. Produces the documented output and reports failures through the test
 * harness; releases local resources before returning. Run after ordinary checks with the other README examples.
 */
void example_format_compiled(void);

/**
 * @brief Run the format guide's custom README example and check its result.
 * @note Takes no parameters or ownership. Produces the documented output and reports failures through the test
 * harness; releases local resources before returning. Run after ordinary checks with the other README examples.
 */
void example_format_custom(void);

/**
 * @brief Run the format guide's error README example and check its result.
 * @note Takes no parameters or ownership. Produces the documented output and reports failures through the test
 * harness; releases local resources before returning. Run after ordinary checks with the other README examples.
 */
void example_format_error(void);

/**
 * @brief Run the format guide's emoji README example and check its output byte count.
 * @note Takes no parameters or ownership. Prints the documented UTF-8 text and Unicode scalar representations,
 * reporting failures through the test harness. Run after ordinary checks with the other README examples.
 */
void example_format_emoji(void);

/**
 * @brief Run the format guide's owned_string README programme and check its output size.
 * @note Takes no parameters or ownership. Prints the documented output and reports failures through the test
 * harness, releasing local values before returning. Run after ordinary checks with the other README examples.
 */
void example_format_owned_string(void);

/**
 * @brief Run the format guide's datetime README programme and check its output size.
 * @note Takes no parameters or ownership. Prints the documented output and reports failures through the test
 * harness, releasing local values before returning. Run after ordinary checks with the other README examples.
 */
void example_format_datetime(void);

/**
 * @brief Run the format guide's symbolic README programme and check its output size.
 * @note Takes no parameters or ownership. Prints the documented output and reports failures through the test
 * harness, releasing local values before returning. Run after ordinary checks with the other README examples.
 */
void example_format_symbolic(void);

/**
 * @brief Run the format guide's excel README programme and check its output size.
 * @note Takes no parameters or ownership. Prints the documented output and reports failures through the test
 * harness, releasing local values before returning. Run after ordinary checks with the other README examples.
 */
void example_format_excel(void);

/**
 * @brief Verify typed datetime interpolation with only the owning public header.
 * @note Takes no parameters or ownership. Reports assertions through the ordinary harness and releases local
 * fixture values before returning; run sequentially with the other ordinary format checks.
 */
void test_format_datetime_header(void);

/**
 * @brief Verify typed equation interpolation with only the owning public header.
 * @note Takes no parameters or ownership. Reports assertions through the ordinary harness and releases local
 * fixture values before returning; run sequentially with the other ordinary format checks.
 */
void test_format_equation_header(void);

/**
 * @brief Verify typed TeX interpolation with only the owning public header.
 * @note Takes no parameters or ownership. Reports assertions through the ordinary harness and releases local
 * fixture values before returning; run sequentially with the other ordinary format checks.
 */
void test_format_TeX_header(void);

/**
 * @brief Run the literal-date README programme with explicit month and ordinal-day fields.
 * @note Takes no parameters or ownership. Produces the documented ordinary and superscript suffixes, reports
 * assertions through the harness and releases its datetime. Run after ordinary format checks.
 */
void example_format_date_literals(void);

#endif
