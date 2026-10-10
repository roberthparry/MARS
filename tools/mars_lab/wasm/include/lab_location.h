/**
 * @file lab_location.h
 * @brief Private interface: calendar location selection, state projection and summaries for the Lab browser.
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
 */
#ifndef LAB_WASM_LOCATION_H
#define LAB_WASM_LOCATION_H

/** @brief Validate an opaque form value using browser conversion and native calendar policy.
 * @param kind Date 1, jurisdiction 2 or visibility 3; other values only trim the supplied text.
 * @param boxed Scoped object whose value property contains the candidate, including boxed numbers.
 * @param fallback Scoped value returned when validation fails.
 * @param jurisdictions Scoped Set of accepted jurisdiction codes; used only for kind 2.
 */
void lab_location_validate(unsigned kind, int boxed, int fallback, int jurisdictions);

/** @brief Validate bootstrap data before projecting both jurisdiction menus and database availability.
 * @param catalogue Scoped bootstrap record with options, towns, available and optional error properties.
 * @return One after projection, including an unavailable database; zero for invalid data or missing selects.
 */
int lab_location_catalogue(int catalogue);

/** @brief Resolve a jurisdiction catalogue, retaining the country-level fallback.
 * @param context Scoped record containing config, jurisdictions and the towns catalogue.
 * @param boxed Scoped object whose value property is the requested jurisdiction.
 */
void lab_location_towns(int context, int boxed);

/** @brief Expose a native town key as opaque text.
 * @param town Scoped catalogue town record with an opaque value property.
 */
void lab_location_option_value(int town);

/** @brief Build town options, invalidate pending restorations and register one native change listener.
 * @param select Scoped town-select node, or zero for no action.
 * @param towns Scoped array of catalogue town records in display order.
 * @param select_default Non-zero chooses the first marked default; zero preserves a surviving selection.
 */
void lab_location_populate(int select, int towns, int select_default);

/** @brief Invalidate restoration on a select change without consuming the event.
 * @param action Change action 1; unknown actions do nothing.
 * @param event Scoped browser event whose target must be the supplied select.
 * @param select Scoped select node, borrowed again for each subscribed callback.
 * @return No C value; returns an empty event plan through lab_dom_return, without browser service calls.
 */
void lab_location_dispatch(unsigned action, int event, int select);

/** @brief Borrow the native select's chosen option without scanning its children.
 * @param select Scoped town-select node; absent or empty selection returns null through the host.
 */
void lab_location_selected(int select);

/** @brief Find a coordinate pair with native tolerance policy and reject absent/non-finite numbers.
 * @param select Scoped town-select node.
 * @param coordinates Scoped object containing latitude and longitude values; this is a handle, not a flag.
 * @return One after selecting a coordinate match within 0.000001; zero if absent, invalid or unmatched.
 */
int lab_location_coordinates(int select, int coordinates);

/** @brief Clear named-town selection only when edited coordinates no longer describe that town.
 * @param select Scoped town-select node.
 * @param latitude Scoped latitude input node.
 * @param longitude Scoped longitude input node.
 * @param elevation Scoped elevation input node; blank values do not invalidate a coordinate match.
 */
void lab_location_clear_custom(int select, int latitude, int longitude, int elevation);

/** @brief Snapshot select identity, generation, option identities and values for restoration guards.
 * @param select Scoped town-select node whose option identities and values are captured.
 * @return No C value; returns the snapshot through lab_dom_return for the host to retain across awaits.
 */
void lab_location_snapshot(int select);

/** @brief Reject obsolete generations and replaced, reordered or edited options before restoration.
 * @param select Scoped current town-select node.
 * @param snapshot Scoped object returned by lab_location_snapshot before asynchronous work.
 * @return One if select identity, generation, options and selected value still match; zero otherwise.
 */
int lab_location_unchanged(int select, int snapshot);

/** @brief Select an exact saved key or a native compatibility-match index without changing failed selections.
 * @param select Scoped town-select node.
 * @param selection Scoped object with an exact value string or a zero-based numeric match index.
 * @return One after selection; zero for an invalid or missing match, preserving the previous value.
 */
int lab_location_choose(int select, int selection);

/** @brief Resolve exact-key, server-match, coordinate-fallback and empty restoration policy in order.
 * @param stage Initial selection 0 or server compatibility response 1; other stages return null.
 * @param select Scoped town-select node; an absent select produces restored=false without projection.
 * @param snapshot Scoped restoration snapshot supplying the ordered server candidate values.
 * @param selection Scoped record containing the saved value, latitude and longitude; values remain opaque.
 * @param response Scoped server response containing match_index for stage 1; ignored for stage 0.
 * @return No C value; lab_dom_return supplies a request record or a boolean restored result, with controls
 * for browser timezone lookup after a key/index match on a calendar select. Coordinate fallback deliberately
 * leaves authored coordinates and offsets untouched. The host must check lab_location_unchanged and its
 * external request-freshness predicate after each await before calling this synchronous projection.
 */
void lab_location_restore(unsigned stage, int select, int snapshot, int selection, int response);

/** @brief Copy selected coordinates and the browser-computed timezone offset into their controls.
 * @param controls Scoped town-control record returned by lab_location_controls.
 * @param offset Scoped formatted timezone-offset string; empty text preserves the current offset.
 * @param context Scoped context record containing defaults, jurisdiction membership and mutable calendar ownership.
 * @return One when a selected town was applied; zero when no town is selected.
 */
int lab_location_apply_town(int controls, int offset, int context);

/** @brief Return schema metadata for existing persistence and event adapters.
 * @param mode DateTime mode 5 or almanac mode 6.
 */
void lab_location_schema(unsigned mode);

/** @brief Read calendar controls in the native schema order.
 * @param mode DateTime mode 5 or almanac mode 6.
 * @param context Scoped record supplying visibility for the almanac's non-DOM field.
 */
void lab_location_read(unsigned mode, int context);

/** @brief Resolve or apply state with native restore/capture/fill/reset policy; invalid operations return null.
 * @param mode DateTime mode 5 or almanac mode 6.
 * @param operation Restore 0, capture 1, fill blank defaults 2 or reset 3.
 * @param source Scoped record containing authored calendar fields.
 * @param prefix Scoped string prepended to source keys, or an empty string.
 * @param context Scoped context record containing defaults, jurisdiction membership and mutable calendar ownership.
 * @param apply Non-zero projects resolved fields and updates context; zero only returns the state.
 */
void lab_location_state(unsigned mode, unsigned operation, int source, int prefix, int context, int apply);

/** @brief Return the current DOM nodes keyed by the native calendar schema.
 * @param mode DateTime mode 5 or almanac mode 6.
 */
void lab_location_elements(unsigned mode);

/** @brief Select each mode's town/offset controls, optionally leaving authored coordinates untouched.
 * @param mode DateTime mode 5 or almanac mode 6.
 * @param coordinates Non-zero includes latitude, longitude and elevation inputs in the returned record.
 */
void lab_location_controls(unsigned mode, int coordinates);

/** @brief Fill blank defaults, then capture values with automatic DateTime offsets omitted.
 * @param mode DateTime mode 5 or almanac mode 6.
 * @param context Scoped context record containing defaults, jurisdiction membership and mutable calendar ownership.
 */
void lab_location_current(unsigned mode, int context);

/** @brief Format calendar summaries from already resolved opaque values.
 * @param mode DateTime mode 5 or almanac mode 6.
 * @param state Scoped resolved calendar-state record.
 * @param title Scoped almanac worksheet title; ignored for DateTime.
 */
void lab_location_summary(unsigned mode, int state, int title);

/** @brief Apply a current location response, preserving selected towns and manually edited offsets.
 * @param mode DateTime mode 5 or almanac mode 6.
 * @param data Scoped current jurisdiction-location response record.
 * @param context Scoped context record containing defaults, jurisdiction membership and mutable calendar ownership.
 * @param update_coordinates Non-zero permits coordinate updates when no selected town supplied them.
 * @param town_applied Non-zero preserves the coordinates and offset already supplied by a town.
 */
void lab_location_response(unsigned mode, int data, int context, int update_coordinates, int town_applied);

/** @brief Prepare an eclipse action before its guarded asynchronous town restoration.
 * @param button Scoped eclipse-action button carrying date and location dataset fields.
 * @param context Scoped record supplying defaults and jurisdiction membership.
 */
void lab_location_totality_prepare(int button, int context);

/** @brief Complete a still-current eclipse action, using its explicit coordinates when supplied.
 * @param state Scoped state returned by lab_location_totality_prepare.
 * @param restored Non-zero retains the restored town; zero clears it before applying explicit coordinates.
 */
void lab_location_totality_finish(int state, int restored);

/** @brief Project evaluation metadata while retaining authored offsets and fallback guards.
 * @param mode DateTime mode 5 or almanac mode 6.
 * @param data Scoped evaluation response with an optional fields record.
 * @param context Scoped context record containing defaults, jurisdiction membership and mutable calendar ownership.
 */
void lab_location_evaluation(unsigned mode, int data, int context);

#endif
