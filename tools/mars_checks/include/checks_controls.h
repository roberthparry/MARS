/**
 * @file checks_controls.h
 * @brief Mechanical legal, distribution and Markdown API controls.
 *
 * These tools-only APIs accept a repository root explicitly, allowing isolated
 * C fixtures. Validation routines append owned diagnostics to the supplied
 * collection. Command functions print Python-compatible messages and return
 * zero for success, one for failed controls and two for inspection errors.
 */
#ifndef MARS_CHECKS_CONTROLS_H
#define MARS_CHECKS_CONTROLS_H

#include "checks_support.h"

/**
 * @brief Run compliance checks, optionally accepting untracked files and suppressing success output.
 * @param root Borrowed repository root path.
 * @param allow_untracked Whether existing files may satisfy the required-path policy before being tracked.
 * @param quiet Whether to suppress successful compliance output.
 * @return Zero for success, one for a failed control, or two for an inspection or usage error.
 */
int checks_compliance(const string_t *root, bool allow_untracked, bool quiet);

/**
 * @brief Validate SPDX 2.3 declarations, mandatory packages and relationship endpoints.
 * @param source Borrowed source string; ownership is unchanged.
 * @param errors Borrowed diagnostic collection, or output destination as described in the brief.
 * @return No value.
 */
void checks_spdx(const string_t *source, checks_strings_t *errors);

/**
 * @brief Validate the two simple legal installation variables.
 * @param makefile Borrowed root Makefile text.
 * @param errors Borrowed diagnostic collection, or output destination as described in the brief.
 * @return No value.
 */
void checks_installed_documents(const string_t *makefile, checks_strings_t *errors);

/**
 * @brief Validate the almanac provenance rows against files under root; false means an I/O error.
 * @param root Borrowed repository root path.
 * @param source Borrowed source string; ownership is unchanged.
 * @param errors Borrowed diagnostic collection, or output destination as described in the brief.
 * @return True when the condition or operation described above succeeds; false otherwise.
 */
bool checks_provenance(const string_t *root, const string_t *source, checks_strings_t *errors);

/**
 * @brief Return an owned lowercase SHA-256 hex digest, using file.h streaming; NULL means an I/O error.
 * @param path Borrowed path; ownership is unchanged.
 * @return Owned string, or NULL on an I/O error; release with string_free.
 */
string_t *checks_sha256(const string_t *path);

/**
 * @brief Determine whether a case-insensitive repository path is private ESAA material.
 * @param path Borrowed path; ownership is unchanged.
 * @return True when the condition or operation described above succeeds; false otherwise.
 */
bool checks_forbidden_path(const string_t *path);

/**
 * @brief Inspect the whole index or staged ACMR paths for private material.
 * @param root Borrowed repository root path.
 * @param staged Whether to inspect only staged added, copied, modified and renamed paths.
 * @return Zero for success, one for a failed control, or two for an inspection or usage error.
 */
int checks_public_distribution(const string_t *root, bool staged);

/**
 * @brief Extract sorted unique top-level public function names from header text.
 * @param source Borrowed source string; ownership is unchanged.
 * @return Owned result; release with the corresponding free function. NULL denotes an inspection error where
 * documented.
 */
checks_strings_t *checks_header_functions(const string_t *source);

/**
 * @brief Test an exact ASCII identifier occurrence in Markdown text.
 * @param name Borrowed public function identifier.
 * @param text Borrowed UTF-8 input string.
 * @return True when the condition or operation described above succeeds; false otherwise.
 */
bool checks_function_mentioned(const string_t *name, const string_t *text);

/**
 * @brief Verify all installed header functions are mentioned in their assigned guides.
 * @param root Borrowed repository root path.
 * @return Zero for success, one for a failed control, or two for an inspection or usage error.
 */
int checks_markdown_api(const string_t *root);

#endif
