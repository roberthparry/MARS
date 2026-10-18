/**
 * @file tba_browser.h
 * @brief Protobuf transport buffers and scalar limits for the freestanding browser.
 *
 * Defines the browser codec ABI for proto/tba.proto. The JavaScript adapter stages or copies bytes in
 * bounded WASM memory. Buffers are borrowed until the next synchronous codec call;
 * callers must copy results before awaiting browser work. No mathematics is parsed.
 * Other browser responsibilities have their own headers in this private directory.
 */
#ifndef TBA_WASM_BROWSER_H
#define TBA_WASM_BROWSER_H

#include <stdint.h>

#include "tba_handle.h"

/** @brief Maximum encoded message size shared by the codec and worker admission. */
enum { tba_browser_limit = 8 * 1024 * 1024 };

/** @brief Return the maximum encoded message size in bytes for host-side admission. */
unsigned tba_browser_message_limit(void);

/** @brief Return the maximum member count admitted by a host container snapshot. */
unsigned tba_browser_member_limit(void);

/** @brief Return the capacity of the host's temporary codec-value table. */
unsigned tba_browser_handle_capacity(void);

/** @brief Return the maximum base64 character count for a saved encoded message. */
unsigned tba_browser_base64_limit(void);

/**
 * @brief Borrow the codec's fixed input staging buffer.
 * @return Writable region of 8 MiB, valid for the module lifetime.
 */
unsigned char *tba_browser_input_buffer(void);

/**
 * @brief Identify the browser ABI independently of the stable Protobuf schema.
 * @return ABI version required by the matching JavaScript adapter and native bootstrap.
 */
unsigned tba_codec_abi_version(void);

/**
 * @brief Borrow the encoded output.
 * @return Module-owned bytes; use only the length returned by tba_browser_encode and copy before the next encoding.
 */
unsigned char *tba_browser_output_buffer(void);

/**
 * @brief Encode a host object in a version-one Protobuf envelope.
 * @param root Borrowed host handle for an object, not an array or scalar.
 * @return Encoded byte count, or zero for invalid, excessively deep or oversized data.
 */
uint32_t tba_browser_encode(codec_handle root);

/**
 * @brief Decode a complete version-one envelope from the input staging buffer.
 * @param size Staged byte count, at most 8 MiB.
 * @return Decoded host object handle, or -1 for invalid data or exceeded limits.
 */
codec_handle tba_browser_decode(uint32_t size);

#endif
