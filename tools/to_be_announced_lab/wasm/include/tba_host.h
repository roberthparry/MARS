/**
 * @file tba_host.h
 * @brief Host-value imports used by the browser Protobuf codec.
 *
 * The JavaScript transport supplies these operations to tba_browser.c. Handles
 * belong to the current synchronous codec call and must not be retained by C.
 * Strings cross the boundary as UTF-8 bytes in WebAssembly memory. DOM projection
 * uses the separate tba_dom.h interface rather than these codec-specific handles.
 */
#ifndef TBA_WASM_HOST_H
#define TBA_WASM_HOST_H

#include <stdint.h>

#include "tba_handle.h"

/**
 * @brief Classify a host value.
 * @param id Borrowed codec handle.
 * @return Kind: null 0, boolean 1, number 2, string 3, array 4 or object 5.
 */
int tba_host_kind(codec_jsref id);

/**
 * @brief Read a number or boolean.
 * @param id Borrowed codec handle for a numeric or boolean value.
 * @return Numeric value; booleans become zero or one.
 */
double tba_host_number(codec_jsref id);

/**
 * @brief Count a container's entries.
 * @param id Borrowed array or object handle.
 * @return Number of entries.
 */
int tba_host_count(codec_jsref id);

/**
 * @brief Obtain a child's handle and kind in one host call.
 * @param id Borrowed array or object handle.
 * @param index Zero-based entry index.
 * @return Child handle shifted left by three bits, with its kind in the low bits.
 */
int tba_host_child_info(codec_jsref id, int index);

/**
 * @brief Copy an object key as UTF-8.
 * @param id Borrowed object handle.
 * @param index Zero-based entry index.
 * @param target Writable WASM destination.
 * @param capacity Destination capacity in bytes.
 * @return Byte length, or a negative result when the key cannot fit.
 */
int tba_host_key_text(codec_jsref id, int index, unsigned char *target, uint32_t capacity);

/**
 * @brief Copy a string value as UTF-8.
 * @param id Borrowed string handle.
 * @param target Writable WASM destination.
 * @param capacity Destination capacity in bytes.
 * @return Byte length, or a negative result when the string cannot fit.
 */
int tba_host_text(codec_jsref id, unsigned char *target, uint32_t capacity);

/**
 * @brief Create a decoded value and attach it to its parent.
 * @param kind Value kind in 0..5, as for tba_host_kind.
 * @param number Numeric or boolean payload.
 * @param text Borrowed UTF-8 bytes for a string, or null for other kinds.
 * @param length String byte count.
 * @param parent Parent container handle, or -1 for the root.
 * @param key Borrowed UTF-8 object key, or null for a root/array element.
 * @param key_length Object key byte count.
 * @return New host handle, or a negative result on failure.
 */
codec_jsref tba_host_value(int kind, double number, const unsigned char *text, uint32_t length, codec_jsref parent,
                   const unsigned char *key, uint32_t key_length);

#endif
