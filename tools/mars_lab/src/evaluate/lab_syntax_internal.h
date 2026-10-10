/**
 * @file lab_syntax_internal.h
 * @brief Private Function-card and matrix-heading metadata for native Lab presentation.
 *
 * Used only by the evaluation presentation adapter and its lexical implementation.
 * Input remains borrowed and unchanged; returned JSON owns exact text spans, not
 * browser offsets, and escaped HTML for direct display. Matrix headings additionally carry native display labels.
 * This is highlighting, not programme validation or execution.
 * External callers use lab_presentation_adapt from lab_presentation.h.
 */
#ifndef LAB_SYNTAX_INTERNAL_H
#define LAB_SYNTAX_INTERNAL_H

#include "json.h"

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
