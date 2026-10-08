/**
 * @file checks_readme.h
 * @brief Markdown programme discovery and sequential execution.
 *
 * Owns parsed C examples, their following documented output and source locations.
 * Reference-only fences are excluded. The runner uses the existing global JSON
 * test configuration and HTTP Python fixtures; compiler commands never use a
 * shell. Accessors borrow strings until the example collection is freed.
 */
#ifndef MARS_CHECKS_README_H
#define MARS_CHECKS_README_H

#include "checks_support.h"

/** Opaque owned Markdown example collection. */
typedef struct checks_examples checks_examples_t;

/** Opaque individual example borrowed from a collection. */
typedef struct checks_example checks_example_t;

/**
 * @brief Parse executable C fences from one repository-relative Markdown path.
 * @param path Borrowed path; ownership is unchanged.
 * @param text Borrowed UTF-8 input string.
 * @return Owned result; release with the corresponding free function. NULL denotes an inspection error where
 * documented.
 */
checks_examples_t *checks_examples_parse(const string_t *path, const string_t *text);

/**
 * @brief Release a parsed collection and its strings.
 * @param examples Owned object to release; NULL is accepted.
 * @return No value.
 */
void checks_examples_free(checks_examples_t *examples);

/**
 * @brief Return the number of executable examples.
 * @param examples Borrowed example collection, except when ownership is explicitly released.
 * @return The requested count or one-based source line, as described above.
 */
size_t checks_examples_count(const checks_examples_t *examples);

/**
 * @brief Borrow an example by index, or NULL.
 * @param examples Borrowed example collection, except when ownership is explicitly released.
 * @param index Zero-based item index.
 * @return Borrowed value, or NULL when absent; do not free it.
 */
const checks_example_t *checks_examples_get(const checks_examples_t *examples, size_t index);

/**
 * @brief Borrow the stable README configuration identifier.
 * @param example Borrowed example from its owning collection.
 * @return Borrowed value, or NULL when absent; do not free it.
 */
const string_t *checks_example_id(const checks_example_t *example);

/**
 * @brief Borrow the repository-relative Markdown path.
 * @param example Borrowed example from its owning collection.
 * @return Borrowed value, or NULL when absent; do not free it.
 */
const string_t *checks_example_path(const checks_example_t *example);

/**
 * @brief Borrow the exact programme source.
 * @param example Borrowed example from its owning collection.
 * @return Borrowed value, or NULL when absent; do not free it.
 */
const string_t *checks_example_code(const checks_example_t *example);

/**
 * @brief Borrow documented output, or NULL when absent.
 * @param example Borrowed example from its owning collection.
 * @return Borrowed value, or NULL when absent; do not free it.
 */
const string_t *checks_example_expected(const checks_example_t *example);

/**
 * @brief Return the one-based fence line.
 * @param example Borrowed example from its owning collection.
 * @return The requested count or one-based source line, as described above.
 */
size_t checks_example_line(const checks_example_t *example);

/**
 * @brief Test whether the programme text contains main().
 * @param example Borrowed example from its owning collection.
 * @return True when the condition or operation described above succeeds; false otherwise.
 */
bool checks_example_has_main(const checks_example_t *example);

/**
 * @brief Normalise output by removing trailing line whitespace and outer blank lines.
 * @param text Borrowed UTF-8 input string.
 * @return Owned string; release with string_free.
 */
string_t *checks_output_normalise(const string_t *text);

/**
 * @brief Run the README command; argv contains only its options, excluding the subcommand.
 * @param root Borrowed repository root path.
 * @param argc Number of command-option arguments.
 * @param argv Borrowed option argument vector, excluding the subcommand.
 * @return Zero for success, one for a failed control, or two for an inspection or usage error.
 */
int checks_readme(const string_t *root, int argc, char **argv);

#endif
