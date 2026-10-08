/**
 * @file test_lab_math_support.h
 * @brief Native mathematical regression fixtures for the prebuilt Lab workers.
 *
 * Private to test_lab_math suites. Results and strings are borrowed from a
 * sequential per-test arena until lab_math_reset. Worker protocol decoding uses
 * the controlled evaluator façade; no browser adaptation or rendering is run.
 * Independent references use C complex arithmetic or MPFR, never Python.
 */
#ifndef TEST_LAB_MATH_SUPPORT_H
#define TEST_LAB_MATH_SUPPORT_H

#include <complex.h>
#include <stdbool.h>
#include <stddef.h>

#include "json.h"
#include "test_harness.h"

/**
 * @brief Run the expression worker and require a successful exit; borrow decoded fields.
 * @param source Borrowed UTF-8 expression.
 * @param variable Borrowed differentiation variable.
 * @param action Borrowed worker action, usually evaluate, derivative or integral.
 * @param precision Requested decimal digits, clamped to at least 17.
 * @return Borrowed decoded fields valid until lab_math_reset; NULL on allocation failure.
 */
const json_t *lab_math_fields(const char *source, const char *variable, const char *action, unsigned precision);

/**
 * @brief Run a worker allowing an expected non-zero exit; borrow raw text and decoded fields.
 * @param worker Borrowed worker basename: mars_lab or equation_lab.
 * @param source Borrowed expression or equation text.
 * @param variable Borrowed differentiation variable; ignored for equations.
 * @param action Borrowed expression action; empty text selects the worker default.
 * @param precision Requested decimal digits, clamped to at least 17.
 * @param status Required destination for the child exit status.
 * @param raw Optional destination for borrowed merged worker output.
 * @return Borrowed decoded fields valid until lab_math_reset; NULL on allocation failure.
 */
const json_t *lab_math_worker(const char *worker, const char *source, const char *variable, const char *action,
                             unsigned precision, int *status, const char **raw);

/**
 * @brief Borrow a field; absence records a failure and yields an empty string.
 * @param fields Borrowed decoded worker object.
 * @param key Borrowed protocol field key.
 * @return Borrowed UTF-8 text valid until lab_math_reset; failures record a harness assertion.
 */
const char *lab_math_text(const json_t *fields, const char *key);

/**
 * @brief Parse the complete native real or complex numerical field; malformed text fails.
 * @param fields Borrowed decoded worker object.
 * @param key Borrowed numerical field key.
 * @return Numerical result; NAN on parsing, allocation or invalid-argument failure.
 */
double complex lab_math_number(const json_t *fields, const char *key);

/**
 * @brief Parse a complete numerical string, accepting the native Unicode minus sign.
 * @param text Borrowed native numerical text; malformed input records a failure.
 * @return Numerical result; NAN on parsing, allocation or invalid-argument failure.
 */
double complex lab_math_parse_number(const char *text);

/**
 * @brief Format a borrowed arena string with the string_t formatter.
 * @param format Borrowed string_t-compatible format.
 * @param ... Values matching the format conversions.
 * @return Borrowed UTF-8 text valid until lab_math_reset; failures record a harness assertion.
 */
const char *lab_math_format(const char *format, ...);

/**
 * @brief Copy a string and replace every literal occurrence; borrow the result.
 * @param text Borrowed input text.
 * @param old Borrowed non-empty literal to replace.
 * @param replacement Borrowed replacement text.
 * @return Borrowed UTF-8 text valid until lab_math_reset; failures record a harness assertion.
 */
const char *lab_math_replace(const char *text, const char *old, const char *replacement);

/**
 * @brief Borrow Expression algebra preceding the first binding bar and optional opening brace.
 * @param fields Borrowed decoded object containing Expression output.
 * @return Borrowed UTF-8 text valid until lab_math_reset; failures record a harness assertion.
 */
const char *lab_math_algebra(const json_t *fields);

/**
 * @brief Borrow text following the first delimiter; absence records a failure.
 * @param text Borrowed input text.
 * @param delimiter Borrowed non-empty delimiter.
 * @return Borrowed UTF-8 text valid until lab_math_reset; failures record a harness assertion.
 */
const char *lab_math_after(const char *text, const char *delimiter);

/**
 * @brief Read a repository fixture with file_t and borrow its complete text.
 * @param path Borrowed repository-relative UTF-8 fixture path.
 * @return Borrowed UTF-8 text valid until lab_math_reset; failures record a harness assertion.
 */
const char *lab_math_read(const char *path);

/**
 * @brief Execute a Function programme through the prebuilt Ophelia worker and borrow output.
 * @param source Borrowed Function programme supplied as standard input.
 * @param precision Requested decimal digits for Ophelia.
 * @return Borrowed UTF-8 text valid until lab_math_reset; failures record a harness assertion.
 */
const char *lab_math_programme(const char *source, unsigned precision);

/**
 * @brief Assert exact string equality with both strings in failure diagnostics.
 * @param actual Borrowed observed text.
 * @param expected Borrowed expected text.
 */
void lab_math_equal(const char *actual, const char *expected);

/**
 * @brief Record a condition without returning from the caller; return its truth value.
 * @param condition Assertion result.
 * @param detail Borrowed failure context.
 * @return The supplied condition after recording its assertion result.
 */
bool lab_math_check(bool condition, const char *detail);

/**
 * @brief Assert the presence or absence of a literal substring.
 * @param actual Borrowed observed text.
 * @param fragment Borrowed literal substring.
 * @param present True requires the fragment; false forbids it.
 */
void lab_math_contains(const char *actual, const char *fragment, bool present);

/**
 * @brief Assert a POSIX extended regular-expression match or non-match.
 * @param actual Borrowed observed text.
 * @param pattern Borrowed POSIX extended regular expression.
 * @param present True requires a match; false forbids it.
 */
void lab_math_regex(const char *actual, const char *pattern, bool present);

/**
 * @brief Assert a strict absolute complex error bound, as in the original assertLess cases.
 * @param actual Observed numerical value.
 * @param expected Independent reference value.
 * @param tolerance Strict positive absolute error limit.
 */
void lab_math_close(double complex actual, double complex expected, double tolerance);

/**
 * @brief Assert Python assertAlmostEqual's decimal-place absolute bound.
 * @param actual Observed numerical value.
 * @param expected Reference value.
 * @param places Decimal places used for the absolute rounding threshold.
 */
void lab_math_places(double complex actual, double complex expected, unsigned places);

/**
 * @brief Composite Simpson quadrature with an even, caller-selected interval count.
 * @param function Borrowed integrand callback.
 * @param context Borrowed opaque callback context.
 * @param left Lower integration endpoint.
 * @param right Upper integration endpoint.
 * @param steps Positive even number of integration intervals.
 * @return Numerical result; NAN on parsing, allocation or invalid-argument failure.
 */
double complex lab_math_simpson(double complex (*function)(double, void *), void *context,
                                double left, double right, size_t steps);

/**
 * @brief Release all borrowed results and strings after a sequential registered case.
 */
void lab_math_reset(void);

#endif
