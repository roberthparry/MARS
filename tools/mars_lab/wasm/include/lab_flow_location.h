/**
 * @file lab_flow_location.h
 * @brief Private native continuations for calendar location workflows.
 *
 * The shared flow dispatcher routes global kinds 26..35 to local kinds 0..9.
 * Frames contain browser-owned controls, callbacks and snapshots. The native
 * continuations choose requests, projection order and freshness checks; browser
 * services retain Intl timezone lookup, promises and DOM capabilities. No scoped
 * handle survives a return and no mathematical text is interpreted here.
 */
#ifndef LAB_FLOW_LOCATION_H
#define LAB_FLOW_LOCATION_H

/** @brief Advance one calendar/location workflow.
 * @param kind Forms 0, restore town 1, populate town 2, synchronise selectors 3,
 * restore history 4, totality action 5, jurisdiction refresh 6, auto-evaluate 7,
 * format authored time 8 or calendar control event 9.
 * @param frame Borrowed mutable browser frame containing the workflow inputs.
 * @param view Borrowed shared browser view; unused by these independent workflows.
 * @return Scoped continuation or completion plan, with explicit null for stale town restoration.
 */
int lab_flow_location(unsigned kind, int frame, int view);

/** @brief Plan synchronous local-calendar rendering before visibility changes.
 * @param text Borrowed box with opaque fallback text in its value property; falsey values become empty.
 * @param sections Borrowed native calendar sections, passed unchanged to the renderer.
 * @param body Borrowed optional local-calendar body node.
 * @param card Borrowed optional local-calendar card node.
 * @return Publishes an ordered host-services plan without changing either node.
 */
void lab_flow_location_local(int text, int sections, int body, int card);

/** @brief Apply visibility after successful non-empty local-calendar rendering.
 * @param card Borrowed local-calendar card node.
 * @param mode Borrowed current worksheet-mode string, captured after rendering.
 */
void lab_flow_location_visibility(int card, int mode);

/** @brief Choose a rounded-select callback without invoking it inside a native scope.
 * @param select Borrowed optional browser select node.
 * @param capabilities Borrowed lazy boolean getters reporting callable rebuild and sync methods.
 * @return Publishes an ordered plan preferring rebuild, or an empty plan when no method is available.
 */
void lab_flow_location_select(int select, int capabilities);

/** @brief Choose whether a completed date-picker move requires synchronous redraw.
 * @param changed Non-zero when the native movement policy changed the picker.
 * @return Publishes a redraw service plan, or an empty plan for unchanged movement.
 */
void lab_flow_location_picker(unsigned changed);

#endif
