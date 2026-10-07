/**
 * @file protobuf.h
 * @brief Native Protocol Buffers wire-format messages and binary encoding.
 *
 * The opaque protobuf_t message stores ordered fields, including repeated and
 * unknown fields. Readers and writers handle varints, fixed-width values and
 * length-delimited data, with helpers for signed ZigZag values and configurable
 * message-size and field-count limits.
 *
 * Use this module to build or inspect binary service payloads, including messages
 * passed to the unary gRPC helpers in http.h. The application supplies knowledge of
 * field numbers, types and nested schemas; length-delimited bytes are not
 * automatically interpreted as text or submessages.
 *
 * This is a native wire codec, not a .proto compiler, generated service client or
 * transport implementation. Deprecated group fields are unsupported. Follow the
 * documented ownership rules for message handles and returned byte arrays.
 */

#ifndef MARS_PROTOBUF_H
#define MARS_PROTOBUF_H
#include "array.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Opaque ordered Protocol Buffers wire message; field meaning comes from the application's schema.
 */
typedef struct _protobuf_t protobuf_t;

/**
 * @brief Supported wire types; deprecated groups are rejected.
 */
typedef enum { PROTOBUF_VARINT = 0, PROTOBUF_FIXED64 = 1, PROTOBUF_BYTES = 2, PROTOBUF_FIXED32 = 5 } protobuf_wire_t;

/**
 * @brief Create an empty bounded message.
 * @param max_bytes Encoded-byte budget; zero selects 16 MiB; maximum 64 MiB.
 * @param max_fields Field-count budget; zero selects 4096; maximum 1048576.
 * @return Owned message, or NULL; release with protobuf_free.
 */
protobuf_t *protobuf_new(size_t max_bytes, size_t max_fields);

/**
 * @brief Release a message and copied byte fields.
 * @param message Owned message; NULL is harmless.
 */
void protobuf_free(protobuf_t *message);

/**
 * @brief Decode wire records without guessing schema types; reject truncation, overflow and groups.
 * @param data Borrowed binary bytes; NULL only for size zero.
 * @param size Encoded byte count.
 * @param max_bytes Encoded-byte budget, as for protobuf_new.
 * @param max_fields Field-count budget, as for protobuf_new.
 * @return Owned message, or NULL on malformed input, exceeded limits or allocation failure.
 */
protobuf_t *protobuf_decode(const void *data, size_t size, size_t max_bytes, size_t max_fields);

/**
 * @brief Serialise ordered records with shortest-length varints.
 * @param message Borrowed message.
 * @return Owned byte array released with array_destroy, or NULL; this is not canonical schema serialisation.
 */
array_t *protobuf_encode(const protobuf_t *message);

/**
 * @brief Append an integer wire record, preserving duplicate field numbers.
 * @param message Message to modify.
 * @param field Field number from 1 to 536870911.
 * @param wire VARINT, FIXED32 or FIXED64; fixed values are raw bits, including floating-point representations.
 * @param value Unsigned value or bit pattern; FIXED32 rejects values exceeding UINT32_MAX.
 * @return True on success; false leaves the message unchanged.
 */
bool protobuf_add_integer(protobuf_t *message, uint32_t field, protobuf_wire_t wire, uint64_t value);

/**
 * @brief Copy a length-delimited field, including nested messages or packed fields.
 * @param message Message to modify.
 * @param field Valid non-zero field number.
 * @param data Borrowed bytes; NULL only when size is zero.
 * @param size Byte count.
 * @return True on success; false leaves the message unchanged.
 */
bool protobuf_add_bytes(protobuf_t *message, uint32_t field, const void *data, size_t size);

/**
 * @brief Count ordered wire records.
 * @param message Borrowed message.
 * @return Record count, or zero for NULL.
 */
size_t protobuf_count(const protobuf_t *message);

/**
 * @brief Read a record's field number in constant time.
 * @param message Borrowed message.
 * @param index Zero-based record index.
 * @return Field number, or zero if out of range.
 */
uint32_t protobuf_field(const protobuf_t *message, size_t index);

/**
 * @brief Read a record's wire type.
 * @param message Borrowed message.
 * @param index Zero-based record index.
 * @return Wire type, or -1 if out of range.
 */
int protobuf_wire(const protobuf_t *message, size_t index);

/**
 * @brief Read a VARINT/FIXED32/FIXED64 record without schema conversion.
 * @param message Borrowed message.
 * @param index Zero-based record index.
 * @param value Required output, unchanged on failure.
 * @return True on success, false for byte fields or invalid arguments.
 */
bool protobuf_integer(const protobuf_t *message, size_t index, uint64_t *value);

/**
 * @brief Borrow a length-delimited payload.
 * @param message Borrowed message.
 * @param index Zero-based record index.
 * @param size Required output byte count; set to zero for empty or invalid fields.
 * @return Borrowed bytes until message destruction; NULL for empty or invalid fields. Check protobuf_wire to
 * distinguish.
 */
const void *protobuf_bytes(const protobuf_t *message, size_t index, size_t *size);

/**
 * @brief Convert a signed sint64 value to its unsigned ZigZag wire representation.
 * @param value Signed value.
 * @return ZigZag representation.
 */
uint64_t protobuf_zigzag_encode(int64_t value);

/**
 * @brief Decode an unsigned ZigZag sint64 representation.
 * @param value Encoded value.
 * @return Signed value, including INT64_MIN.
 */
int64_t protobuf_zigzag_decode(uint64_t value);

#endif
