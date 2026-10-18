/**
 * @file protobuf_wire-t.h
 * @brief Allocation-free Protocol Buffers wire primitives for bounded buffers.
 *
 * Shared by the owning native codec in protobuf-t.h and freestanding WebAssembly
 * adapters. Reads borrow their payloads; writes use caller-owned storage. No
 * arrays, locks, heap allocation, schema interpretation or host services are used.
 * Callers enforce message, field-count and nesting budgets. Deprecated groups are
 * unsupported. Operations leave cursors, outputs and destination bytes unchanged
 * on failure; input and output objects must not alias the supplied byte buffer.
 */
#ifndef MARS_PROTOBUF_WIRE_H
#define MARS_PROTOBUF_WIRE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Supported wire types; deprecated groups are rejected. */
typedef enum { PROTOBUF_VARINT = 0, PROTOBUF_FIXED64 = 1, PROTOBUF_BYTES = 2, PROTOBUF_FIXED32 = 5 } protobuf_wire_t;

/** @brief One borrowed field; byte payloads remain valid only while the input buffer remains unchanged. */
typedef struct {
    uint32_t field;            /**< Non-zero field number, at most 536870911. */
    protobuf_wire_t wire;      /**< Validated wire discriminator. */
    uint64_t integer;          /**< Numeric bits, or zero for a byte field. */
    const unsigned char *data; /**< Borrowed byte payload; NULL for numeric fields. */
    size_t size;              /**< Byte payload length; zero for numeric fields. */
} protobuf_wire_field_t;

/**
 * @brief Calculate the shortest varint encoding length, from one to ten bytes.
 * @param value Unsigned integer to encode.
 * @return Required byte count.
 */
size_t protobuf_wire_varint_size(uint64_t value);

/**
 * @brief Read an unsigned varint, accepting non-minimal encodings but rejecting overflow and truncation.
 * @param data Borrowed input buffer; NULL is permitted only when size is zero.
 * @param size Available input bytes.
 * @param offset Required cursor, advanced only on success; initially no greater than size.
 * @param value Required integer output, unchanged on failure.
 * @return True on success; false on invalid arguments, exhausted input or malformed encoding.
 */
bool protobuf_wire_read_varint(const unsigned char *data, size_t size, size_t *offset, uint64_t *value);

/**
 * @brief Write a shortest-length unsigned varint into bounded storage.
 * @param data Caller-owned output buffer; NULL is permitted only when size is zero.
 * @param size Output capacity in bytes.
 * @param offset Required cursor, advanced only on success; initially no greater than size.
 * @param value Unsigned integer to encode.
 * @return True on success; false on invalid arguments or insufficient capacity, without partial writes.
 */
bool protobuf_wire_write_varint(unsigned char *data, size_t size, size_t *offset, uint64_t value);

/**
 * @brief Read one field, validating its tag and complete payload before advancing the cursor.
 * @param data Borrowed input buffer; NULL is permitted only when size is zero.
 * @param size Available input bytes; nested callers pass their containing message's bound.
 * @param offset Required cursor, advanced only on success; initially no greater than size.
 * @param field Required borrowed field output, unchanged on failure.
 * @return True on success; false at end of input or for invalid arguments, tags, wire types or payloads.
 */
bool protobuf_wire_read_field(const unsigned char *data, size_t size, size_t *offset, protobuf_wire_field_t *field);

/**
 * @brief Write little-endian fixed-width numeric bits without a field tag.
 * @param data Caller-owned output buffer; NULL is permitted only when size is zero.
 * @param size Output capacity in bytes.
 * @param offset Required cursor, advanced only on success; initially no greater than size.
 * @param wire PROTOBUF_FIXED32 or PROTOBUF_FIXED64; other types are rejected.
 * @param value Numeric bits; FIXED32 rejects values exceeding UINT32_MAX.
 * @return True on success; false on invalid arguments or insufficient capacity, without partial writes.
 */
bool protobuf_wire_write_fixed(unsigned char *data, size_t size, size_t *offset, protobuf_wire_t wire, uint64_t value);

#endif
