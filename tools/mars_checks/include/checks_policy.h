/**
 * @file checks_policy.h
 * @brief Native source-policy audits and scanner fixtures for developer checks.
 *
 * Audits physical inline definition lengths, macro visibility and expression
 * registry layout and perfect hashes, and rejects Python source and interpreter
 * shebangs to prevent build-time and runtime dependency regressions. These checks inspect every
 * preprocessor branch; they do not establish semantic triviality. Inputs are
 * borrowed MARS strings. Output collections own their appended diagnostics.
 * The synchronous command uses the public file API through checks_support.
 */
#ifndef MARS_CHECKS_POLICY_H
#define MARS_CHECKS_POLICY_H

#include "checks_support.h"

/**
 * @brief Scan physical inline definition spans and macros concealing inline tokens.
 * @param source Borrowed source text; ownership is unchanged.
 * @param spans Required caller-owned collection receiving inclusive "start:end" line spans.
 * @param macros Required caller-owned collection receiving macro start line numbers as strings.
 * @param errors Required caller-owned collection receiving scanner diagnostics.
 * @return True when no inline body is unterminated; false otherwise, with a diagnostic appended to errors.
 * @details Appended strings belong to their receiving collections. Existing entries are retained.
 */
bool checks_policy_scan_inline(const string_t *source, checks_strings_t *spans, checks_strings_t *macros,
                               checks_strings_t *errors);

/**
 * @brief Audit inline definition lengths and macros concealing inline code or declarations.
 * @param source Borrowed source text; ownership is unchanged.
 * @param errors Required caller-owned collection receiving owned diagnostics; existing entries are retained.
 * @return No value; length, macro and unterminated-body violations are appended to errors.
 */
void checks_policy_inline(const string_t *source, checks_strings_t *errors);

/**
 * @brief Audit an expression function registry's layout, aliases and perfect hash.
 * @param source Borrowed complete registry source text; ownership is unchanged.
 * @param binding True to inspect the binding registry; false to inspect the inline parser registry.
 * @param errors Required caller-owned collection receiving owned diagnostics; existing entries are retained.
 * @return No value; registry violations are appended to errors.
 */
void checks_policy_function_table(const string_t *source, bool binding, checks_strings_t *errors);

/**
 * @brief Run selected repository source-policy audits and print diagnostics to standard error.
 * @param root Borrowed repository root path; ownership is unchanged.
 * @param argc Number of selector arguments; zero selects every audit.
 * @param argv Borrowed array of argc selectors, excluding the command name; may be NULL when argc is zero.
 * @return 0 on success, 1 for audit or repository inspection failures, or 2 for invalid selectors.
 * @details Selectors are inline, functiontables and python, each optionally prefixed with two hyphens.
 * Multiple selectors combine audits. The argument strings and array are not modified.
 */
int checks_policy(const string_t *root, int argc, char **argv);

#endif
