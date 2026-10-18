/**
 * @file lab_handle.h
 * @brief Borrowed JavaScript-value references used by the WebAssembly bridge.
 *
 * The _jsref suffix denotes an opaque integer reference to a JavaScript value.
 * A reference identifies a value in the browser's table for one synchronous bridge
 * call. It is not a pointer into WebAssembly memory and must not be retained
 * beyond its scope. Zero denotes absence unless an API explicitly documents
 * another meaning. These integer aliases preserve the existing wasm32 i32 ABI;
 * C does not enforce distinct types for aliases of the same underlying integer.
 */
#ifndef MARS_LAB_HANDLE_H
#define MARS_LAB_HANDLE_H

/** @brief Borrowed browser value, array or object in the current bridge scope. */
typedef int jsref;

/** @brief Borrowed DOM node or event target in the current browser bridge scope. */
typedef jsref node_jsref;

/** @brief Borrowed browser event in the current browser bridge scope. */
typedef jsref event_jsref;

/** @brief Borrowed SVG plot chart carrying native geometry metadata. */
typedef node_jsref chart_jsref;

/** @brief Borrowed mutable browser record describing a plot gesture. */
typedef jsref gesture_jsref;

/** @brief Borrowed browser continuation or projection frame. */
typedef jsref frame_jsref;

/** @brief Borrowed browser view supplying live state and presentation resources. */
typedef jsref view_jsref;

/** @brief Borrowed browser snapshot used for restoration or freshness checks. */
typedef jsref snapshot_jsref;

/**
 * @brief Borrowed value in the codec's separate synchronous handle table.
 *
 * Not interchangeable with DOM-table handles. Codec APIs use -1 for an absent
 * parent or an error as documented by each operation; zero can be a valid value.
 */
typedef int codec_jsref;

#endif
