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

/**
 * @brief Classify a host value.
 * @param id Borrowed codec handle.
 * @return Kind: null 0, boolean 1, number 2, string 3, array 4 or object 5.
 */
int lab_host_kind(int id);

/**
 * @brief Read a number or boolean.
 * @param id Borrowed codec handle for a numeric or boolean value.
 * @return Numeric value; booleans become zero or one.
 */
double lab_host_number(int id);

/**
 * @brief Count a container's entries.
 * @param id Borrowed array or object handle.
 * @return Number of entries.
 */
int lab_host_count(int id);

/**
 * @brief Obtain a child's handle and kind in one host call.
 * @param id Borrowed array or object handle.
 * @param index Zero-based entry index.
 * @return Child handle shifted left by three bits, with its kind in the low bits.
 */
int lab_host_child_info(int id, int index);

/**
 * @brief Copy an object key as UTF-8.
 * @param id Borrowed object handle.
 * @param index Zero-based entry index.
 * @param target Writable WASM destination.
 * @param capacity Destination capacity in bytes.
 * @return Byte length, or a negative result when the key cannot fit.
 */
int lab_host_key_text(int id, int index, unsigned char *target, uint32_t capacity);

/**
 * @brief Copy a string value as UTF-8.
 * @param id Borrowed string handle.
 * @param target Writable WASM destination.
 * @param capacity Destination capacity in bytes.
 * @return Byte length, or a negative result when the string cannot fit.
 */
int lab_host_text(int id, unsigned char *target, uint32_t capacity);

/**
 * @brief Create a decoded value and attach it to its parent.
 * @param kind Value kind in 0..5, as for lab_host_kind.
 * @param number Numeric or boolean payload.
 * @param text Borrowed UTF-8 bytes for a string, or null for other kinds.
 * @param length String byte count.
 * @param parent Parent container handle, or -1 for the root.
 * @param key Borrowed UTF-8 object key, or null for a root/array element.
 * @param key_length Object key byte count.
 * @return New host handle, or a negative result on failure.
 */
int lab_host_value(int kind, double number, const unsigned char *text, uint32_t length, int parent,
                   const unsigned char *key, uint32_t key_length);

#endif
