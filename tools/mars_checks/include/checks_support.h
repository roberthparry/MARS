/**
 * @file checks_support.h
 * @brief Shared text, collection and repository I/O for the native checkers.
 *
 * Tool modules use these owned string collections and file.h adapters instead of
 * accessing MARS internals. Byte offsets are used for C and Markdown syntax;
 * strings retain their original UTF-8 bytes. Allocation failures terminate with
 * status 2. Returned strings and collections belong to the caller. No global
 * mutable state is maintained, apart from the command's accumulated diagnostics.
 */
#ifndef MARS_CHECKS_SUPPORT_H
#define MARS_CHECKS_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>

#include "json.h"
#include "ustring.h"

/** Opaque growable collection of owned strings. */
typedef struct checks_strings checks_strings_t;

/**
 * @brief Allocate an empty collection; terminate on allocation failure.
 * @return Owned result; release with the corresponding free function. NULL denotes an inspection error where
 * documented.
 */
checks_strings_t *checks_strings_new(void);

/**
 * @brief Release a collection and its strings; NULL is accepted.
 * @param items Owned object to release; NULL is accepted.
 * @return No value.
 */
void checks_strings_free(checks_strings_t *items);

/**
 * @brief Append an owned string, transferring ownership.
 * @param items Borrowed string collection; ownership is unchanged.
 * @param text Owned string transferred to the collection.
 * @return No value.
 */
void checks_strings_add(checks_strings_t *items, string_t *text);

/**
 * @brief Borrow an indexed string; return NULL outside the collection.
 * @param items Borrowed string collection; ownership is unchanged.
 * @param index Zero-based item index.
 * @return Borrowed value, or NULL when absent; do not free it.
 */
const string_t *checks_strings_get(const checks_strings_t *items, size_t index);

/**
 * @brief Return the number of strings.
 * @param items Borrowed string collection; ownership is unchanged.
 * @return The requested count or one-based source line, as described above.
 */
size_t checks_strings_count(const checks_strings_t *items);

/**
 * @brief Sort lexically for deterministic diagnostics and binary lookup.
 * @param items Borrowed string collection; ownership is unchanged.
 * @return No value.
 */
void checks_strings_sort(checks_strings_t *items);

/**
 * @brief Find a literal using binary search in a sorted collection.
 * @param items Borrowed string collection; ownership is unchanged.
 * @param text Borrowed UTF-8 input string.
 * @return True when the condition or operation described above succeeds; false otherwise.
 */
bool checks_strings_has(const checks_strings_t *items, const char *text);

/**
 * @brief Return a newly allocated string, terminating on allocation failure.
 * @param text Borrowed UTF-8 input string.
 * @return Owned string; release with string_free.
 */
string_t *checks_text(const char *text);

/**
 * @brief Append exact UTF-8 text without NFC conversion; terminate on failure.
 * @param destination Borrowed destination string, modified in place.
 * @param source Borrowed source string; ownership is unchanged.
 * @return No value.
 */
void checks_append(string_t *destination, const string_t *source);

/**
 * @brief Copy a byte range from a string, preserving UTF-8 exactly.
 * @param text Borrowed UTF-8 input string.
 * @param start First byte offset in the input.
 * @param length Number of UTF-8 bytes to copy.
 * @return Owned string; release with string_free.
 */
string_t *checks_slice(const string_t *text, size_t start, size_t length);

/**
 * @brief Read an ASCII syntax byte, returning 128 for non-ASCII bytes and zero outside the string.
 * @param text Borrowed UTF-8 input string.
 * @param offset Byte offset to inspect.
 * @return The ASCII syntax byte, 128 for a non-ASCII byte, or zero outside the string.
 */
unsigned char checks_byte(const string_t *text, size_t offset);

/**
 * @brief Match an ASCII literal at a byte offset.
 * @param text Borrowed UTF-8 input string.
 * @param offset Byte offset to inspect.
 * @param literal Borrowed NUL-terminated literal for comparison.
 * @return True when the condition or operation described above succeeds; false otherwise.
 */
bool checks_at(const string_t *text, size_t offset, const char *literal);

/**
 * @brief Compare a string to an ASCII literal.
 * @param text Borrowed UTF-8 input string.
 * @param literal Borrowed NUL-terminated literal for comparison.
 * @return True when the condition or operation described above succeeds; false otherwise.
 */
bool checks_equal(const string_t *text, const char *literal);

/**
 * @brief Join a root and path, retaining absolute paths.
 * @param root Borrowed repository root path.
 * @param path Borrowed path; ownership is unchanged.
 * @return Owned string; release with string_free.
 */
string_t *checks_path(const string_t *root, const char *path);

/**
 * @brief Read a complete UTF-8 file using file.h; NULL indicates an I/O error.
 * @param path Borrowed path; ownership is unchanged.
 * @return Owned string, or NULL on an I/O error; release with string_free.
 */
string_t *checks_read(const string_t *path);

/**
 * @brief Write a complete UTF-8 file using file.h.
 * @param path Borrowed path; ownership is unchanged.
 * @param text Borrowed UTF-8 input string.
 * @return True when the condition or operation described above succeeds; false otherwise.
 */
bool checks_write(const string_t *path, const string_t *text);

/**
 * @brief Test for a regular file, following its final symbolic link.
 * @param path Borrowed path; ownership is unchanged.
 * @return True when the condition or operation described above succeeds; false otherwise.
 */
bool checks_is_file(const string_t *path);

/**
 * @brief Create a directory and missing parents.
 * @param path Borrowed path; ownership is unchanged.
 * @return True when the condition or operation described above succeeds; false otherwise.
 */
bool checks_mkdir(const string_t *path);

/**
 * @brief Remove a caller-created private tree without following symbolic links.
 * @param path Borrowed path; ownership is unchanged.
 * @return True when the condition or operation described above succeeds; false otherwise.
 */
bool checks_remove_tree(const string_t *path);

/**
 * @brief List sorted relative files with a suffix; optionally recurse, excluding build and local guidance trees.
 * @param root Borrowed repository root path.
 * @param directory Borrowed repository-relative directory name.
 * @param suffix Borrowed filename suffix to select.
 * @param recursive Whether to descend into permitted subdirectories.
 * @return Owned result; release with the corresponding free function. NULL denotes an inspection error where
 * documented.
 */
checks_strings_t *checks_files(const string_t *root, const char *directory, const char *suffix, bool recursive);

/**
 * @brief Split at LF or NUL, preserving empty interior records.
 * @param text Borrowed UTF-8 input string.
 * @param separator ASCII byte delimiting the records.
 * @return Owned result; release with the corresponding free function. NULL denotes an inspection error where
 * documented.
 */
checks_strings_t *checks_split(const string_t *text, unsigned char separator);

/**
 * @brief Match a POSIX extended expression; optionally return owned capture zero or a subgroup.
 * @param text Borrowed UTF-8 input string.
 * @param pattern Borrowed POSIX extended regular expression.
 * @param group Capture index, from zero through seven.
 * @param capture Optional destination for an owned matched string; release with string_free.
 * @return True when the condition or operation described above succeeds; false otherwise.
 */
bool checks_match(const string_t *text, const char *pattern, size_t group, string_t **capture);

/**
 * @brief Collapse whitespace between C tokens.
 * @param text Borrowed UTF-8 input string.
 * @return Owned string; release with string_free.
 */
string_t *checks_normalise_space(const string_t *text);

/**
 * @brief Strip C comments, and optionally preprocessing lines, preserving token separation.
 * @param text Borrowed UTF-8 input string.
 * @param directives Borrowed argument; ownership is unchanged.
 * @return Owned string; release with string_free.
 */
string_t *checks_strip_c(const string_t *text, bool directives);

/**
 * @brief Borrow a JSON object member, or NULL.
 * @param object Borrowed JSON object.
 * @param key Borrowed NUL-terminated member name.
 * @return Borrowed value, or NULL when absent; do not free it.
 */
const json_t *checks_member(const json_t *object, const char *key);

/**
 * @brief Store a string member by value.
 * @param object Borrowed JSON object.
 * @param key Borrowed NUL-terminated member name.
 * @param value Value copied into the JSON object.
 * @return No value.
 */
void checks_json_text(json_t *object, const char *key, const string_t *value);

/**
 * @brief Store an integer member by value.
 * @param object Borrowed JSON object.
 * @param key Borrowed NUL-terminated member name.
 * @param value Value copied into the JSON object.
 * @return No value.
 */
void checks_json_integer(json_t *object, const char *key, long value);

/**
 * @brief Stop on an infrastructure error with exit status 2.
 * @param message Borrowed infrastructure-error description.
 * @return No value.
 */
void checks_fatal(const char *message);

#endif
