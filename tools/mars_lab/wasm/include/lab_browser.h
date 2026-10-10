/**
 * @file lab_browser.h
 * @brief Protobuf transport buffers and scalar limits for the freestanding browser.
 *
 * Defines the browser codec ABI for proto/lab.proto, together with precision and
 * interval-budget validation. The JavaScript adapter stages or copies bytes in
 * bounded WASM memory. Buffers are borrowed until the next synchronous codec call;
 * callers must copy results before awaiting browser work. No mathematics is parsed.
 * Other browser responsibilities have their own headers in this private directory.
 */
#ifndef LAB_WASM_BROWSER_H
#define LAB_WASM_BROWSER_H

#include <stdint.h>

/**
 * @brief Borrow the codec's fixed input staging buffer.
 * @return Writable region of 4 MiB, valid for the module lifetime.
 */
unsigned char *lab_browser_input_buffer(void);

/**
 * @brief Identify the browser ABI independently of the stable Protobuf schema.
 * @return ABI version required by the matching JavaScript adapter and native bootstrap.
 */
unsigned lab_browser_abi_version(void);

/**
 * @brief Borrow the encoded output.
 * @return Module-owned bytes; use only the length returned by lab_browser_encode and copy before the next encoding.
 */
unsigned char *lab_browser_output_buffer(void);

/**
 * @brief Encode a host object in a version-one Protobuf envelope.
 * @param root Borrowed host handle for an object, not an array or scalar.
 * @return Encoded byte count, or zero for invalid, excessively deep or oversized data.
 */
uint32_t lab_browser_encode(int root);

/**
 * @brief Decode a complete version-one envelope from the input staging buffer.
 * @param size Staged byte count, at most 4 MiB.
 * @return Decoded host object handle, or -1 for invalid data or exceeded limits.
 */
int lab_browser_decode(uint32_t size);

/**
 * @brief Clamp finite precision to the supported saved-value range.
 * @param value Parsed precision in bits.
 * @param fallback Value to return when value is not finite.
 * @return Finite input clamped to 17..1048576, or fallback.
 */
double lab_browser_precision(double value, double fallback);

/**
 * @brief Validate a parsed integration work budget against the offered budgets.
 * @param value Parsed interval ceiling.
 * @param fallback Value to return for an unsupported ceiling.
 * @return Accepted value (500, 5000, 20000, 50000 or 100000), or fallback.
 */
double lab_browser_intervals(double value, double fallback);

#endif
