/**
 * @file lab_handle.h
 * @brief Borrowed browser-value handles used by the WebAssembly bridge.
 *
 * A handle identifies a value in the browser's table for one synchronous bridge
 * call. It is not a pointer into WebAssembly memory and must not be retained
 * beyond its scope. Zero denotes absence unless an API explicitly documents
 * another meaning. These integer aliases preserve the existing wasm32 i32 ABI;
 * C does not enforce distinct types for aliases of the same underlying integer.
 */
#ifndef MARS_LAB_HANDLE_H
#define MARS_LAB_HANDLE_H

/** @brief Borrowed browser value, array or object in the current bridge scope. */
typedef int browser_handle;

/** @brief Borrowed DOM node or event target in the current browser bridge scope. */
typedef browser_handle node_handle;

/** @brief Borrowed browser event in the current browser bridge scope. */
typedef browser_handle event_handle;

/** @brief Borrowed SVG plot chart carrying native geometry metadata. */
typedef node_handle chart_handle;

/** @brief Borrowed mutable browser record describing a plot gesture. */
typedef browser_handle gesture_handle;

/** @brief Borrowed browser continuation or projection frame. */
typedef browser_handle frame_handle;

/** @brief Borrowed browser view supplying live state and presentation resources. */
typedef browser_handle view_handle;

/** @brief Borrowed browser snapshot used for restoration or freshness checks. */
typedef browser_handle snapshot_handle;

/**
 * @brief Borrowed value in the codec's separate synchronous handle table.
 *
 * Not interchangeable with DOM-table handles. Codec APIs use -1 for an absent
 * parent or an error as documented by each operation; zero can be a valid value.
 */
typedef int codec_handle;

#endif
