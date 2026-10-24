/**
 * @file lab_host.h
 * @brief Host-value imports used by the browser Protobuf codec.
 *
 * The JavaScript transport supplies these operations to lab_browser.c. Handles
 * belong to the current synchronous codec call and must not be retained by C.
 * Strings cross the boundary as UTF-8 bytes in WebAssembly memory. DOM projection
 * uses the separate lab_dom.h interface rather than these codec-specific handles.
 */
#ifndef LAB_WASM_HOST_H
#define LAB_WASM_HOST_H

#include <stdint.h>

#include "lab_handle.h"
#include "lab_value.h"

/**
 * @brief Classify a host value.
 * @param id Scoped codec handle to the browser value being classified; not a DOM handle or linear-memory pointer.
 * @return Kind: null 0, boolean 1, number 2, string 3, array 4 or object 5; -1 for an unsupported or invalid handle.
 */
lab_value_kind_t lab_host_kind(codec_jsref id);

/**
 * @brief Read a number or boolean.
 * @param id Scoped codec handle to a numeric or boolean browser value.
 * @return Numeric value; booleans become zero or one.
 */
double lab_host_number(codec_jsref id);

/**
 * @brief Count a container's entries.
 * @param id Scoped codec handle to a browser array or snapshotted object.
 * @return Array length or captured enumerable-key count; zero when no container entries are available.
 */
int lab_host_count(codec_jsref id);

/**
 * @brief Obtain a child's handle and kind in one host call.
 * @param id Scoped codec handle to the parent browser array or snapshotted object.
 * @param index Zero-based array element index or index in the object's captured enumerable-key list.
 * @return Scoped child handle shifted left by three bits, with its kind in the low bits; -1 for an unsupported child.
 */
int lab_host_child_info(codec_jsref id, int index);

/**
 * @brief Copy an object key as UTF-8.
 * @param id Scoped codec handle to the snapshotted browser object.
 * @param index Zero-based index in the object's captured enumerable-key list.
 * @param[out] target Writable destination in WASM linear memory with at least capacity bytes available.
 * @param capacity Destination capacity in bytes; no NUL terminator is written.
 * @return UTF-8 byte count, or -1 if the key is absent, cannot fit or cannot round-trip through UTF-8.
 * A failed copy may leave partial bytes in target.
 */
int lab_host_key_text(codec_jsref id, int index, unsigned char *target, uint32_t capacity);

/**
 * @brief Copy a string as UTF-8 or an opaque byte value without conversion.
 * @param id Scoped codec handle to the browser string or non-shared Uint8Array to copy.
 * @param[out] target Writable destination in WASM linear memory with at least capacity bytes available.
 * @param capacity Destination capacity in bytes; no NUL terminator is written.
 * @return UTF-8 byte count, or -1 for a non-string value, insufficient capacity or a failed UTF-8 round-trip;
 * a failed copy may leave partial bytes in target.
 */
int lab_host_text(codec_jsref id, unsigned char *target, uint32_t capacity);

/**
 * @brief Create a decoded value and attach it to its parent.
 * @param kind Value kind in 0..5, as for lab_host_kind.
 * @param number Numeric payload for kind 2, or truth value for kind 1; ignored for other kinds.
 * @param text Borrowed length-byte UTF-8 span in WASM linear memory for kind 3; null for other kinds.
 * @param length String byte count, excluding any terminator; ignored unless kind is 3.
 * @param parent Scoped codec handle to the parent array/object, or -1 for the root; arrays append in call order.
 * @param key Borrowed key_length-byte UTF-8 span naming an object property; null for a root or array element.
 * @param key_length Object key byte count, excluding any terminator; ignored for a root or array element.
 * @return New scoped codec handle, or -1 for an invalid kind, invalid parent or duplicate object key.
 */
codec_jsref lab_host_value(lab_value_kind_t kind, double number, const unsigned char *text, uint32_t length, codec_jsref parent,
                   const unsigned char *key, uint32_t key_length);

#endif
