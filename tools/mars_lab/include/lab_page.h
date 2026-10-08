/**
 * @file lab_page.h
 * @brief Native MARS Lab page rendering and packaged defaults.
 *
 * Used by the Lab server, calendar and state store. The HTML template contains
 * page structure; its sibling catalogue.json supplies packaged settings. Jurisdiction
 * choices and towns are read separately from configured database storage. Returned values
 * are caller-owned; no Python interpreter is needed at runtime.
 */
#ifndef MARS_LAB_PAGE_H
#define MARS_LAB_PAGE_H

#include "json.h"

/**
 * @brief Render the page, returning an owned string or NULL on failure.
 *
 * State supplies the initial expression. Optional control_token is supplied only
 * after the server authorises this request; it is never read from the environment.
 * Optional mobile metadata is supplied under state.mobile by lab_mobile_details();
 * without it, mobile access is explicitly local-only. Supplied SVG is ignored and
 * regenerated natively from the URL. Release the result with string_free().
 * The asset is resolved via MARS_LAB_ASSET_FILE, MARS_ROOT, or the compiled asset path.
 * @param state Borrowed worksheet and optional mobile metadata; NULL uses defaults.
 * @return Owned page text released with string_free, or NULL on failure.
 */
string_t *lab_page_render(const json_t *state);

/**
 * @brief Return the packaged worksheet defaults as an owned object.
 *
 * Uses the JSON settings catalogue beside the page asset, retaining the snapshot's
 * explicit dates and locations.
 * Release with json_free(); NULL denotes an unavailable or invalid asset.
 * @return Owned default-state object, or NULL on failure.
 */
json_t *lab_page_defaults(void);

/**
 * @brief Read and validate the packaged defaults and presentation constants.
 *
 * Resolves catalogue.json beside the configured page template. Geographic records
 * are not stored here; use lab_page_jurisdictions for database-backed choices.
 * @return Owned JSON object released with json_free, or NULL for a missing or invalid catalogue.
 */
json_t *lab_page_catalogue(void);

/**
 * @brief Obtain browser choices, representative locations and towns from the jurisdiction database.
 *
 * Contains available, options, locations and towns members. An unavailable database
 * returns available=false, empty collections and an explanatory error, not packaged
 * fallback rows. Reads configured storage through the public jurisdiction API.
 * @return Owned JSON object released with json_free, or NULL on allocation failure.
 */
json_t *lab_page_jurisdictions(void);

#endif
