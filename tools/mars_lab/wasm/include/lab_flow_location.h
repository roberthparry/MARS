/**
 * @file lab_flow_location.h
 * @brief Application API for native continuations for calendar location workflows.
 *
 * The shared flow dispatcher routes global kinds 26..35 to local kinds 0..9.
 * Frames contain browser-owned controls, callbacks and snapshots. The native
 * continuations choose requests, projection order and freshness checks; browser
 * services retain Intl timezone lookup, promises and DOM capabilities. No scoped
 * handle survives a return and no mathematical text is interpreted here.
 * Boolean arguments crossing the WebAssembly i32 ABI must be canonical 0 or 1;
 * C callers use false or true.
 */
#ifndef LAB_FLOW_LOCATION_H
#define LAB_FLOW_LOCATION_H

#include <stdbool.h>

/** @brief Calendar forms operations carried in frame.action. */
typedef enum {
    LAB_FLOW_LOCATION_FORMAT_TIME = 0, /**< Format authored time text. */
    LAB_FLOW_LOCATION_FORMAT_TOWN = 1, /**< Resolve an authored town key. */
    LAB_FLOW_LOCATION_MATCH_TOWN  = 2   /**< Match a key against the supplied town option. */
} lab_flow_location_form_action_t;

/** @brief Closed workflow selectors for lab_flow_location; numeric values are browser ABI. */
typedef enum {
    LAB_FLOW_LOCATION_FORMS         = 0, /**< Prepare calendar forms. */
    LAB_FLOW_LOCATION_RESTORE       = 1, /**< Restore a selected town. */
    LAB_FLOW_LOCATION_POPULATE      = 2, /**< Populate town choices. */
    LAB_FLOW_LOCATION_SYNC          = 3, /**< Synchronise location selectors. */
    LAB_FLOW_LOCATION_HISTORY       = 4, /**< Restore location history. */
    LAB_FLOW_LOCATION_TOTALITY      = 5, /**< Apply an eclipse action. */
    LAB_FLOW_LOCATION_REFRESH       = 6, /**< Refresh jurisdiction data. */
    LAB_FLOW_LOCATION_AUTO_EVALUATE = 7, /**< Automatically evaluate a calendar. */
    LAB_FLOW_LOCATION_TIME          = 8, /**< Format authored time. */
    LAB_FLOW_LOCATION_CONTROL       = 9, /**< Handle a calendar control change. */
} lab_flow_location_kind_t;

/** @brief Advance one calendar/location workflow.
 * @param kind Forms 0, restore town 1, populate town 2, synchronise selectors 3,
 * restore history 4, totality action 5, jurisdiction refresh 6, auto-evaluate 7,
 * format authored time 8 or calendar control event 9.
 * @param frame Scoped browser handle to the mutable frame containing stage, workflow inputs and service results.
 * @param view Reserved scoped browser-view handle; unused by these independent workflows.
 * @return Scoped browser handle to a continuation or completion plan, with explicit null for stale town restoration.
 */
int lab_flow_location(lab_flow_location_kind_t kind, int frame, int view);

/** @brief Plan synchronous local-calendar rendering before visibility changes.
 * @param text Scoped browser box handle with opaque fallback text in its value property; falsey values become empty.
 * @param sections Scoped browser handle to native calendar sections, passed unchanged to the renderer.
 * @param body Scoped handle to the local-calendar body node, or zero to omit rendering.
 * @param card Scoped handle to the local-calendar card node, or zero to omit visibility updates.
 * @post Publishes an ordered host-services plan through lab_dom_return without changing either node.
 */
void lab_flow_location_local(int text, int sections, int body, int card);

/** @brief Apply visibility after successful non-empty local-calendar rendering.
 * @param card Scoped browser handle to the local-calendar card node whose hidden class is updated.
 * @param mode Scoped browser handle to the current mode string, captured after rendering; datetime shows the card,
 * all other strings hide it.
 */
void lab_flow_location_visibility(int card, int mode);

/** @brief Choose a rounded-select callback without invoking it inside a native scope.
 * @param select Scoped browser handle to an optional select node; zero produces an empty plan.
 * @param capabilities Scoped browser record handle with lazy rebuild and sync boolean getters reporting
 * whether the corresponding rounded-select methods are callable.
 * @post Publishes a service plan through lab_dom_return, preferring rebuild, or an empty plan if neither is callable.
 */
void lab_flow_location_select(int select, int capabilities);

/** @brief Choose whether a completed date-picker move requires synchronous redraw.
 * @param changed True when the native movement policy changed the picker.
 * @post Publishes a redraw service plan through lab_dom_return, or an empty plan for unchanged movement.
 */
void lab_flow_location_picker(bool changed);

#endif
