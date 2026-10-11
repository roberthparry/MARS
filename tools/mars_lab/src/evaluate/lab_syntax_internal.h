/**
 * @file lab_syntax_internal.h
 * @brief Private Function-card and matrix-heading metadata for native Lab presentation.
 *
 * Used only by the evaluation presentation adapter and its lexical implementation.
 * Input remains borrowed and unchanged; returned JSON owns exact text spans, not
 * browser offsets, and escaped HTML for direct display. Numeric display tokens
 * use powers of ten without changing source spans. Matrix headings additionally carry native display labels.
 * This is highlighting, not programme validation or execution.
 * External callers use lab_presentation_adapt from lab_presentation.h.
 */
#ifndef LAB_SYNTAX_INTERNAL_H
#define LAB_SYNTAX_INTERNAL_H

#include "json_t.h"

/**
 * @brief Format one unsigned numeric token for display without changing its digits.
 * @param source Borrowed complete numeric token; other text is returned unchanged.
 * @return Owned Unicode decimal or power-of-ten text, or NULL on allocation failure.
 */
string_t *lab_pres_number_literal(const string_t *source);

/**
 * @brief Format numeric tokens in display text, retaining identifiers, comments and quoted strings.
 * @param source Borrowed exact text; never modified or evaluated.
 * @return Owned display text, or NULL on allocation failure or excessive expansion.
 */
string_t *lab_pres_display_numbers(const string_t *source);

/**
 * @brief Classify bounded Function source into exact lexical text spans.
 * @param source Borrowed source, including incomplete or malformed programmes.
 * @return Owned metadata released with json_free, or NULL on invalid input/allocation failure.
 * @details Sources over 64 KiB or 1024 lexical spans are returned unhighlighted
 * with their exact source and empty spans, without HTML. Embedded NUL is invalid. No algebra is changed.
 */
json_t *lab_pres_function_syntax(const string_t *source);

/**
 * @brief Recognise standalone matrix section headings in native result text.
 * @param source Borrowed exact result text, unchanged by classification.
 * @return Owned source/spans metadata, or NULL on invalid input/allocation failure.
 * @details Heading spans retain original text and carry lowercase display text.
 * Sources exceeding 64 KiB or 1024 spans fall back to an empty span array.
 */
json_t *lab_pres_matrix_headings(const string_t *source);

#endif
