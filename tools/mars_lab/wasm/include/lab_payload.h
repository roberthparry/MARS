/**
 * @file lab_payload.h
 * @brief Application API: request payload construction and background reply application for MARS Lab.
 *
 * Selects evaluation fields, persistence flags and control defaults, validates
 * structured integration bounds, and projects native land-totality and mobile
 * records. Mathematical source and native metadata remain opaque. JavaScript
 * retains fetch, promises, cancellation and freshness checks across awaits.
 * All browser handles are scoped to one synchronous call. Event lists are
 * traversed once to construct payloads or an exact-key reply index; temporary
 * handles are released per entry so list size does not exhaust the handle table.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 */
#ifndef LAB_WASM_PAYLOAD_H
#define LAB_WASM_PAYLOAD_H

#include "lab_handle.h"

/** @brief Native request payload builders; ordering differs from worksheet modes. */
typedef enum {
    LAB_PAYLOAD_EXPRESSION = 0, /**< Expression request record. */
    LAB_PAYLOAD_MATRIX = 1, /**< Matrix request record. */
    LAB_PAYLOAD_EQUATION = 2, /**< Equation request record. */
    LAB_PAYLOAD_DIFFERENTIAL_EQUATION = 3, /**< Differential-equation request record. */
    LAB_PAYLOAD_INTEGRATOR = 4, /**< Integrator request record. */
    LAB_PAYLOAD_FUNCTION = 5, /**< Function request record. */
    LAB_PAYLOAD_WEATHER = 6 /**< Weather request record. */
} lab_payload_kind_t;

/** @brief Construct a wire record; positional inputs contain opaque caller values, including boxed numbers.
 * @param kind Expression 0, matrix 1, equation 2, differential equation 3, integrator 4, function 5 or weather 6.
 * @param inputs Scoped browser handle to the selected builder's positional array; numeric values are array members,
 * not handles. Expression: source, binding source, binding value, variable, action, mode, preview flag, bounds.
 * Matrix: options, fallback source, explicit-operand flag. Equation/differential equation: source.
 * Integrator: source, bounds, interval cap. Function: programme source. Weather: date/latitude/longitude record.
 * @param request Scoped request record with channel and token, or zero to use the caller's fallback policy.
 * @param precision Requested significant decimal digits; omitted from weather payloads.
 * @param updated_at Expression update timestamp in Unix milliseconds; ignored by other builders.
 * @post Publishes the wire record through lab_dom_return, or null for an unsupported builder kind.
 */
void lab_payload_build(lab_payload_kind_t kind, jsref inputs, jsref request, double precision,
                       double updated_at);

/** @brief Check every active bound without parsing or rewriting its mathematical contents.
 * @param bounds Scoped array of active integration bounds with name, lo and hi properties.
 * @post Publishes a diagnostic through lab_dom_return for the first non-empty lower bound with an empty upper bound;
 * publishes null when no such invalid one-sided bound exists.
 */
void lab_payload_bounds_error(jsref bounds);

/** @brief Snapshot native event identifiers and resolve only request-field fallbacks.
 * @param data Scoped almanac response supplying event_year and optional location fields.
 * @param cells Scoped list of cells whose data-almanac-land-totality values identify events.
 * @param config Scoped bootstrap defaults used when response fields and form controls are blank.
 * @post Publishes an event/location request record through lab_dom_return.
 */
void lab_payload_land(jsref data, jsref cells, jsref config);

/** @brief Apply an already-current reply, indexing exact identifiers once and keeping native HTML intact.
 * @param cells Scoped list of destination land-totality cells.
 * @param payload Scoped current response with items and optional timed_out flag.
 * @param failure Successful reply 0, timeout/abort 1 or unavailable/error 2; non-zero ignores response items.
 */
void lab_payload_land_apply(jsref cells, jsref payload, unsigned failure);

/** @brief Project the server's access description and QR markup without interpreting either.
 * @param data Scoped native mobile-access record containing title, hint, url and QR markup.
 */
void lab_payload_mobile(jsref data);

#endif
