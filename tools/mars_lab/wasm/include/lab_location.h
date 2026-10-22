/**
 * @file lab_location.h
 * @brief Application API: calendar location selection, state projection and summaries for the Lab browser.
 *
 * Uses the calendar profile schema to resolve defaults and project controls, and
 * builds town options from the native catalogue without interpreting mathematics.
 * Browser capabilities supply opaque values, Unicode operations and HTML form
 * conversion. Native continuations own asynchronous request policy; JavaScript
 * supplies promises and Intl timezone lookup. Handles belong to one synchronous call.
 * Native-owned generation tokens live as opaque objects on the select nodes;
 * restoration snapshots and change listeners reject obsolete asynchronous work.
 * Catalogue rendering and coordinate matching visit each candidate because the
 * catalogue has no coordinate index. Per-row scopes bound temporary handle usage.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 * Boolean parameters use the wasm32 i32 ABI; browser callers must supply exactly 0 or 1.
 */
#ifndef LAB_WASM_LOCATION_H
#define LAB_WASM_LOCATION_H

#include <stdbool.h>

#include "lab_handle.h"
#include "lab_mode.h"
#include "lab_profile.h"

/** @brief Omit a GeoNames numeric disambiguator from displayed town text without changing its catalogue key. */
jsref lab_location_town_text(jsref text);

/** @brief Native event emitted by a town selector. */
typedef enum {
    LAB_LOCATION_SELECTION_CHANGED = 1 /**< Invalidate pending restoration after a select change. */
} lab_location_action_t;

/** @brief Guarded town-restoration phase around a compatibility lookup. */
typedef enum {
    LAB_LOCATION_RESTORE_INITIAL = 0, /**< Try the saved key before requesting compatibility matching. */
    LAB_LOCATION_RESTORE_RESPONSE = 1 /**< Apply the server match or fall back to coordinates. */
} lab_location_restore_stage_t;

/** @brief Validate an opaque form value using browser conversion and native calendar policy.
 * @param kind Date 1, jurisdiction 2 or visibility 3; other values only trim the supplied text.
 * @param boxed Scoped object whose value property contains the candidate, including boxed numbers.
 * @param fallback Scoped value returned when validation fails.
 * @param jurisdictions Scoped Set of accepted jurisdiction codes; used only for kind 2.
 * @post Publishes the validated text or fallback through lab_dom_return.
 */
void lab_location_validate(lab_profile_kind_t kind, jsref boxed, jsref fallback, jsref jurisdictions);

/** @brief Validate bootstrap data before projecting both jurisdiction menus and database availability.
 * @param catalogue Scoped bootstrap record with options, towns, available and optional error properties.
 * @return True after projection, including an unavailable database; false for invalid data or missing selects.
 */
bool lab_location_catalogue(jsref catalogue);

/** @brief Resolve a jurisdiction catalogue, retaining the country-level fallback.
 * @param context Scoped record containing config, jurisdictions and the towns catalogue.
 * @param boxed Scoped object whose value property is the requested jurisdiction.
 * @post Publishes the jurisdiction's town array through lab_dom_return, falling back to its country array or an empty
 * one.
 */
void lab_location_towns(jsref context, jsref boxed);

/** @brief Expose a native town key as opaque text.
 * @param town Scoped catalogue town record with an opaque value property.
 * @post Publishes the town key as text through lab_dom_return.
 */
void lab_location_option_value(jsref town);

/** @brief Build town options, invalidate pending restorations and register one native change listener.
 * @param select Scoped town-select node, or zero for no action.
 * @param towns Scoped array of catalogue town records in display order.
 * @param select_default True chooses the first marked default; false preserves a surviving selection.
 */
void lab_location_populate(node_jsref select, jsref towns, bool select_default);

/** @brief Invalidate restoration on a select change without consuming the event.
 * @param action LAB_LOCATION_SELECTION_CHANGED invalidates restoration; unknown actions do nothing.
 * @param event Scoped browser event whose target must be the supplied select.
 * @param select Scoped select node, borrowed again for each subscribed callback.
 * @post Publishes an empty event plan through lab_dom_return, without browser service calls.
 */
void lab_location_dispatch(lab_location_action_t action, event_jsref event, node_jsref select);

/** @brief Borrow the native select's chosen option without scanning its children.
 * @param select Scoped town-select node; absent or empty selection returns null through the host.
 */
void lab_location_selected(node_jsref select);

/** @brief Find a coordinate pair with native tolerance policy and reject absent/non-finite numbers.
 * @param select Scoped town-select node.
 * @param coordinates Scoped browser object handle containing latitude and longitude in degrees; not a flag.
 * @return True after selecting a coordinate match within 0.000001 degrees on each axis; false if absent,
 * invalid or unmatched.
 */
bool lab_location_coordinates(node_jsref select, jsref coordinates);

/** @brief Clear named-town selection only when edited coordinates no longer describe that town.
 * @param select Scoped town-select node.
 * @param latitude Scoped browser handle to the latitude input in degrees; compared within 0.000001 degrees.
 * @param longitude Scoped browser handle to the longitude input in degrees; compared within 0.000001 degrees.
 * @param elevation Scoped browser handle to the elevation input in metres; compared within 0.01 metres.
 * Blank input or catalogue elevation does not invalidate a coordinate match.
 */
void lab_location_clear_custom(node_jsref select, jsref latitude, jsref longitude, jsref elevation);

/** @brief Snapshot select identity, generation, option identities and values for restoration guards.
 * @param select Scoped town-select node whose option identities and values are captured.
 * @post Publishes the snapshot through lab_dom_return for the host to retain across awaits.
 */
void lab_location_snapshot(node_jsref select);

/** @brief Reject obsolete generations and replaced, reordered or edited options before restoration.
 * @param select Scoped current town-select node.
 * @param snapshot Scoped object returned by lab_location_snapshot before asynchronous work.
 * @return True if select identity, generation, options and selected value still match; false otherwise.
 */
bool lab_location_unchanged(node_jsref select, snapshot_jsref snapshot);

/** @brief Select an exact saved key or a native compatibility-match index without changing failed selections.
 * @param select Scoped town-select node.
 * @param selection Scoped object with an exact value string or a zero-based numeric match index.
 * @return True after selection; false for an invalid or missing match, preserving the previous value.
 */
bool lab_location_choose(node_jsref select, jsref selection);

/** @brief Resolve exact-key, server-match, coordinate-fallback and empty restoration policy in order.
 * @param stage Initial selection or server compatibility response; other stages return null.
 * @param select Scoped town-select node; an absent select produces restored=false without projection.
 * @param snapshot Scoped restoration snapshot supplying the ordered server candidate values.
 * @param selection Scoped record containing the saved value, latitude and longitude; values remain opaque.
 * @param response Scoped server response containing match_index for stage 1; ignored for stage 0.
 * @post lab_dom_return supplies a request record or a boolean restored result, with controls
 * for browser timezone lookup after a key/index match on a calendar select. Coordinate fallback deliberately
 * leaves authored coordinates and offsets untouched. The host must check lab_location_unchanged and its
 * external request-freshness predicate after each await before calling this synchronous projection.
 */
void lab_location_restore(lab_location_restore_stage_t stage, node_jsref select, snapshot_jsref snapshot,
                          jsref selection, jsref response);

/** @brief Copy selected coordinates and the browser-computed timezone offset into their controls.
 * @param controls Scoped town-control record returned by lab_location_controls.
 * @param offset Scoped browser handle to a formatted timezone offset in hours; empty text preserves the current offset.
 * @param context Scoped context record containing defaults, jurisdiction membership and mutable calendar ownership.
 * @return True when a selected town was applied; false when no town is selected.
 */
bool lab_location_apply_town(jsref controls, jsref offset, jsref context);

/** @brief Return schema metadata for existing persistence and event adapters.
 * @param mode DateTime mode 5 or almanac mode 6.
 * @post Publishes an array of native profile-field descriptors through lab_dom_return; invalid modes publish an empty
 * array.
 */
void lab_location_schema(lab_mode_t mode);

/** @brief Read calendar controls in the native schema order.
 * @param mode DateTime mode 5 or almanac mode 6.
 * @param context Scoped record supplying visibility for the almanac's non-DOM field.
 * @post Publishes the captured field record through lab_dom_return.
 */
void lab_location_read(lab_mode_t mode, jsref context);

/** @brief Resolve or apply state with native restore/capture/fill/reset policy; invalid operations return null.
 * @param mode DateTime mode 5 or almanac mode 6.
 * @param operation Calendar restore, capture, fill-blanks or reset operation.
 * @param source Scoped record containing authored calendar fields.
 * @param prefix Scoped string prepended to source keys, or an empty string.
 * @param context Scoped context record containing defaults, jurisdiction membership and mutable calendar ownership.
 * @param apply True projects resolved fields and updates context; false only returns the state.
 * @post Publishes resolved state through lab_dom_return, or null for an invalid mode or operation.
 */
void lab_location_state(lab_mode_t mode, lab_calendar_operation_t operation, jsref source,
                        jsref prefix, jsref context, bool apply);

/** @brief Return the current DOM nodes keyed by the native calendar schema.
 * @param mode DateTime mode 5 or almanac mode 6.
 * @post Publishes a field-to-node record through lab_dom_return; invalid modes publish an empty record.
 */
void lab_location_elements(lab_mode_t mode);

/** @brief Select each mode's town/offset controls, optionally leaving authored coordinates untouched.
 * @param mode DateTime mode 5 or almanac mode 6.
 * @param coordinates True includes latitude, longitude and elevation inputs in the returned record.
 * @post Publishes town, date and timezone control handles and resetOffsetTouched through lab_dom_return;
 * invalid modes publish an empty record.
 */
void lab_location_controls(lab_mode_t mode, bool coordinates);

/** @brief Fill blank defaults, then capture values with automatic DateTime offsets omitted.
 * @param mode DateTime mode 5 or almanac mode 6.
 * @param context Scoped context record containing defaults, jurisdiction membership and mutable calendar ownership.
 * @post Publishes the captured calendar-state record through lab_dom_return.
 */
void lab_location_current(lab_mode_t mode, jsref context);

/** @brief Format calendar summaries from already resolved opaque values.
 * @param mode DateTime mode 5 or almanac mode 6.
 * @param state Scoped resolved calendar-state record.
 * @param title Scoped almanac worksheet title; ignored for DateTime.
 * @post Publishes the formatted summary string through lab_dom_return.
 */
void lab_location_summary(lab_mode_t mode, jsref state, jsref title);

/** @brief Apply a current location response, preserving selected towns and manually edited offsets.
 * @param mode DateTime mode 5 or almanac mode 6.
 * @param data Scoped current jurisdiction-location response record.
 * @param context Scoped context record containing defaults, jurisdiction membership and mutable calendar ownership.
 * @param update_coordinates True permits coordinate updates when no selected town supplied them.
 * @param town_applied True preserves the coordinates and offset already supplied by a town.
 */
void lab_location_response(lab_mode_t mode, jsref data, jsref context, bool update_coordinates,
                           bool town_applied);

/** @brief Prepare an eclipse action before its guarded asynchronous town restoration.
 * @param button Scoped eclipse-action button carrying date and location dataset fields.
 * @param context Scoped record supplying defaults and jurisdiction membership.
 * @post Publishes the prepared state through lab_dom_return after projecting the date, time, zone and jurisdiction.
 */
void lab_location_totality_prepare(jsref button, jsref context);

/** @brief Complete a still-current eclipse action, using its explicit coordinates when supplied.
 * @param state Scoped state returned by lab_location_totality_prepare.
 * @param restored True retains the restored town; false clears it before applying explicit coordinates.
 */
void lab_location_totality_finish(jsref state, bool restored);

/** @brief Project evaluation metadata while retaining authored offsets and fallback guards.
 * @param mode DateTime mode 5 or almanac mode 6.
 * @param data Scoped evaluation response with an optional fields record.
 * @param context Scoped context record containing defaults, jurisdiction membership and mutable calendar ownership.
 */
void lab_location_evaluation(lab_mode_t mode, jsref data, jsref context);

#endif
