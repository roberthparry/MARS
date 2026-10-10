/**
 * @file lab_wire.h
 * @brief Versioned Protobuf values for the native Lab HTTP boundary.
 *
 * Adapts the Lab's existing in-memory JSON value tree to typed Protobuf records,
 * not JSON text. Uses the public protobuf and string APIs. Limits protect both
 * directions; mathematical expressions and exact numbers remain string values.
 */
#ifndef LAB_WIRE_H
#define LAB_WIRE_H

#include "json.h"
#include "protobuf.h"

/**
 * @brief Encode a version-one object envelope.
 * @param value Borrowed object tree, limited to 4 MiB, 65536 nodes and depth 32.
 * @return Owned bytes to release with array_destroy, or NULL on invalid input.
 */
array_t *lab_wire_encode(const json_t *value);

/**
 * @brief Decode and validate a version-one object envelope.
 * @param bytes Borrowed encoded bytes; NULL is allowed only for zero size.
 * @param size Byte count, at most 4 MiB.
 * @return Owned object to release with json_free, or NULL on malformed input.
 */
json_t *lab_wire_decode(const void *bytes, size_t size);

#endif
