/**
 * @file lab_forms.h
 * @brief Native text preparation and symbolic reference metadata for Lab forms.
 *
 * Accepts bounded JSON form requests, parses text with string_t cursors and uses
 * the public expression parser for parameter identity. Integrator bound spelling
 * and exact authored parameter names are preserved. Time-entry formatting and
 * legacy town keys are prepared here rather than parsed in the browser or WASM.
 * The catalogue builder also uses the structured town presentation helper so
 * browser options arrive with their selection keys and coordinate display text.
 * This synchronous adapter owns no global state, files, workers or sockets. The
 * server supplies the POST /forms transport; callers own every returned response.
 */
#ifndef LAB_FORMS_H
#define LAB_FORMS_H

#include "json.h"

/**
 * @brief Add a legacy selection key and coordinate display to a structured town.
 * @param town Borrowed mutable object with optional name, latitude, longitude and
 * elevation strings. Absent fields become empty; supplied strings are trimmed.
 * @return True on success. False means invalid input or allocation failure; discard
 * the object because fields may be partially added. Input text is limited to 64 KiB.
 * @details Adds value (four fields separated by vertical bars) and detail (signed
 * coordinates with four decimal places, or empty for invalid coordinates). Existing
 * source fields are not changed. Used when building the native jurisdiction catalogue.
 */
bool lab_forms_town_presentation(json_t *town);

/**
 * @brief Prepare integrator, time or town form data.
 * @param payload Borrowed object with action and action-specific fields.
 * @param status Optional destination for HTTP 200, 400 or 500.
 * @return Owned response released with json_free; NULL means allocation failure.
 * @details integrator accepts text or rows and optional expression, returning
 * rows with referenced flags, text, and references_valid. Incomplete expressions
 * retain all rows conservatively. time accepts text and returns text. town accepts
 * value and optional candidates (legacy key strings), returning decoded town,
 * display detail, candidate_details and matching candidate index (-1 if absent). Input text is
 * bounded to 64 KiB in aggregate per action, rows to 256 and candidates to 4096.
 * Unknown actions and malformed field types return ok=false with status 400.
 */
json_t *lab_forms_request(const json_t *payload, unsigned *status);

#endif
