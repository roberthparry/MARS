/**
 * @file lab_presentation_internal.h
 * @brief Private bounded text and editor facilities for the native Lab adapter.
 *
 * Shared only by the native evaluation presentation, calculus and editor files. Returned
 * strings and JSON trees are owned by the caller; input trees and strings are
 * borrowed. Route handlers should use the public lab_presentation.h facade.
 */
#ifndef LAB_PRESENTATION_INTERNAL_H
#define LAB_PRESENTATION_INTERNAL_H

#include "json.h"

/**
 * @brief Append escaped text-node content, retaining exact Unicode and carriage returns.
 * @param html Borrowed mutable destination string.
 * @param text Borrowed source string; never interpreted as markup.
 * @return True on success; false on allocation failure.
 */
bool lab_pres_html_text(string_t *html, const string_t *text);

/**
 * @brief Render prepared matrix terms without interpreting their mathematics.
 * @param terms Borrowed native term array with rectangular string-cell rows.
 * @return Caller-owned HTML, empty for no terms, or NULL on failure; release with string_free.
 */
string_t *lab_pres_matrix_markup(const json_t *terms);

/**
 * @brief Attach normalised derivative and integral card models to metadata.
 * @param metadata Borrowed destination object; receives an owned-by-copy calculus object.
 * @param fields Borrowed native worker response; mathematical fields remain unchanged.
 * @param lines Borrowed array receiving exact line-to-expression mappings.
 * @return True on success, false on allocation failure.
 */
bool lab_pres_calculus_cards(json_t *metadata, const json_t *fields, json_t *lines);

/** @brief Append a temporary JSON value by copy and release it, including on failure. */
bool lab_pres_append(json_t *array, json_t *value);

/** @brief Return an owned trimmed encoded-byte slice of a borrowed string. */
string_t *lab_pres_slice(const string_t *source, size_t start, size_t end);

/** @brief Split at bounded top-level syntax separators, retaining exact token substrings. */
json_t *lab_pres_split(const string_t *source, const char *separators);

/** @brief Append text, inserting a separator only when the destination is non-empty. */
bool lab_pres_join(string_t *out, const char *text, const char *separator);

/** @brief Return an owned envelope with one additive integration constant removed. */
string_t *lab_pres_unset_one(const string_t *source, const string_t *name);

/** @brief Return owned exact-source editor metadata or NULL for malformed input/allocation failure. */
json_t *lab_pres_editor_metadata(const string_t *source);

/** @brief Apply a validated structured editor action and return owned metadata. */
json_t *lab_pres_editor_action(const json_t *payload, const string_t *text);

#endif
