/**
 * @file lab_browser.h
 * @brief Protobuf transport buffers and scalar limits for the freestanding browser.
 *
 * Defines the browser codec ABI for proto/lab.proto, together with precision and
 * interval-budget validation and base64 storage envelopes. The JavaScript adapter stages or copies bytes in
 * bounded WASM memory. Buffers are borrowed until the next synchronous codec call;
 * callers must copy results before awaiting browser work. No mathematics is parsed.
 * Other browser responsibilities have their own headers in this application include directory.
 */
#ifndef LAB_WASM_BROWSER_H
#define LAB_WASM_BROWSER_H

#include <stdint.h>

/** @brief Maximum encoded message size shared by the codec and worker admission. */
enum { lab_browser_limit = 8 * 1024 * 1024 };

/** @brief Padded base64 capacity for one message; also the shared input buffer capacity. */
enum { lab_browser_storage_limit = ((lab_browser_limit + 2) / 3) * 4 };

/**
 * @brief Report the encoded-message admission limit.
 * @return Maximum encoded Protobuf message size in bytes (lab_browser_limit).
 */
unsigned lab_browser_message_limit(void);

/**
 * @brief Report the host container-snapshot admission limit.
 * @return Maximum number of members allowed in one container snapshot.
 */
unsigned lab_browser_member_limit(void);

/**
 * @brief Report the capacity required by the temporary codec-value handle table.
 * @return Number of handle-table slots, including the reserved null slot.
 */
unsigned lab_browser_handle_capacity(void);

/**
 * @brief Report the saved-record text admission limit.
 * @return Maximum padded base64 character count (lab_browser_storage_limit), excluding any terminator.
 */
unsigned lab_browser_base64_limit(void);

/**
 * @brief Borrow the codec's fixed input staging buffer.
 * @return Writable region of lab_browser_storage_limit bytes, valid for the module lifetime.
 * Binary messages remain limited to 8 MiB; only base64 text may use the extra capacity.
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
 * @param root Borrowed codec-table handle for a browser object, not an array or scalar; valid for this codec call.
 * @return Encoded byte count, or zero for invalid, excessively deep or oversized data.
 */
uint32_t lab_browser_encode(int root);

/**
 * @brief Decode a complete version-one envelope from the input staging buffer.
 * @param size Staged byte count, at most 8 MiB.
 * @return Decoded object handle in the current codec table, or -1 for invalid data or exceeded limits.
 */
int lab_browser_decode(uint32_t size);

/**
 * @brief Encode a host object as a padded base64 Protobuf storage record.
 * @param root Borrowed codec-table handle for a browser object, not an array or scalar; valid for this codec call.
 * @return Character count in lab_browser_input_buffer without a NUL terminator, or zero for invalid or oversized data.
 * Copy the result before another codec operation; the saved-record format is unchanged.
 */
uint32_t lab_browser_storage_encode(int root);

/**
 * @brief Decode a canonical padded base64 record staged in the input buffer.
 * @param size ASCII character count, at most lab_browser_storage_limit.
 * @return Decoded object handle in the current codec table, or -1 for malformed base64, Protobuf or exceeded limits.
 * Decodes in place; the caller must clear the staged region after consuming the result.
 */
int lab_browser_storage_decode(uint32_t size);

/**
 * @brief Clamp finite precision to the supported saved-value range.
 * @param value Parsed precision in bits.
 * @param fallback Precision in bits returned unchanged when value is not finite; it is not itself clamped.
 * @return Finite input clamped to 17..262144 bits without integer rounding, or fallback.
 */
double lab_browser_precision(double value, double fallback);

/**
 * @brief Validate a parsed integration work budget against the offered budgets.
 * @param value Parsed maximum integration interval count.
 * @param fallback Interval ceiling returned unchanged for an unsupported value; it is not validated.
 * @return Accepted value (500, 5000, 20000, 50000 or 100000), or fallback.
 */
double lab_browser_intervals(double value, double fallback);

#endif
