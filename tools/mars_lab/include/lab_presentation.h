/**
 * @file lab_presentation.h
 * @brief Native presentation metadata and explicit editor transformations for MARS Lab.
 *
 * Supplies calculus record bodies, numerical solution classification, plain-text
 * solver TeX and matrix layout without browser mathematical parsing. Explicit
 * structured editor actions and integration-constant removal preserve authored
 * binding values and domain conditions. Editor analysis also supplies exact
 * goal-seek starts and independent abbreviated numeric display text.
 * All input is borrowed, responses are owned, and no workers or network requests
 * are launched. Call the metadata adapter after native evaluation adaptation.
 */
#ifndef LAB_PRESENTATION_H
#define LAB_PRESENTATION_H

#include "json.h"

/**
 * @brief Add a presentation object to an existing native evaluation response.
 * @param fields Borrowed mutable response; existing mathematical fields are retained.
 * @return True on success, false on invalid input or allocation failure.
 * @details Contains normalised calculus cards, calculus_lines, solution_lines, editors, expansions,
 * responsive_fit, Function lexical spans and solver metadata. Function highlighting
 * is bounded to 1024 spans per variant and 64 KiB of source; larger input retains
 * its exact text without highlighting. A browser
 * must install this metadata before dispatching the response to its view code.
 */
bool lab_presentation_adapt(json_t *fields);

/**
 * @brief Handle the payload of the asynchronous POST /presentation route.
 * @param payload Borrowed object with action and action-specific fields.
 * @param status Optional destination for HTTP 200, 400 or 500.
 * @return Owned response released with json_free, or NULL on allocation failure.
 * @details Actions are matrix (text), solver_text (text), editor (text, operation),
 * and unset_constants (expression, names array). Text must be NUL-free and is
 * limited to 64 KiB, names to 256. The last
 * action removes standalone additive integration constants and their constant
 * bindings; other names and authored exact values are left intact. Unknown
 * actions and malformed envelopes fail without returning rewritten input.
 * Editor operations are analyse (the default), bindings (bindings array, mode
 * replace or merge, optional unset_constants boolean), kind (name and kind),
 * goal_seek, and calculus (name and calculus set to derivative or integral).
 * Calculus wraps the existing body while retaining bindings and conditions.
 * Binding records contain name, kind and an exact string value.
 * Responses contain editor metadata with text, body, expression, display,
 * bindings, conditions, starts and goal_expression. Parsing does not evaluate
 * authored values; native identifier syntax is checked using the expression API.
 */
json_t *lab_presentation_request(const json_t *payload, unsigned *status);

#endif
