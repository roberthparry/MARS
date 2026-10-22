/**
 * @file protobuf.c
 * @brief Bounded native Protocol Buffers wire encoding.
 *
 * Owns ordered field records and parses or writes supported wire types with explicit byte and field limits.
 * Repeated and unknown fields remain available to schema-aware callers; no external parser or schema compiler is
 * used. The allocation-free primitives in protobuf_wire.c are also compiled
 * for WebAssembly; only this native layer owns arrays and copied field data.
 *
 * This implements protobuf_t. Applications supply schema knowledge and manage message ownership; the HTTP module
 * provides transport and gRPC framing.
 */

/* Native bounded Protocol Buffers wire records; no external parser or schema compiler. */
#include <stdlib.h>
#include <string.h>

#include "protobuf-t.h"

typedef struct {
    uint32_t field;
    protobuf_wire_t wire;
    uint64_t integer;
    unsigned char *bytes;
    size_t size;
} record_t;

typedef struct _protobuf_t {
    array_t *records;
    size_t limit, fields, encoded;
} protobuf_t;

static void release_record(void *pointer)
{
    free(((record_t *)pointer)->bytes);
}

/* Create an explicitly bounded ordered wire-message builder. */
protobuf_t *protobuf_new(size_t max_bytes, size_t max_fields)
{
    if (max_bytes > 67108864 || max_fields > 1048576)
        return NULL;
    protobuf_t *message = calloc(1, sizeof(*message));
    if (!message)
        return NULL;
    message->limit = max_bytes ? max_bytes : 16777216;
    message->fields = max_fields ? max_fields : 4096;
    message->records = array_create(sizeof(record_t), NULL, release_record);
    if (!message->records) {
        free(message);
        return NULL;
    }
    return message;
}

/* Release all copied length-delimited payloads. */
void protobuf_free(protobuf_t *message)
{
    if (message) {
        array_destroy(message->records);
        free(message);
    }
}

static bool append(protobuf_t *message, record_t *record)
{
    if (!message || !record->field || record->field > 536870911 || array_size(message->records) == message->fields)
        return false;
    size_t size = protobuf_wire_varint_size(((uint64_t)record->field << 3) | record->wire);
    size_t payload = record->wire == PROTOBUF_BYTES     ? protobuf_wire_varint_size(record->size) + record->size
                     : record->wire == PROTOBUF_VARINT  ? protobuf_wire_varint_size(record->integer)
                     : record->wire == PROTOBUF_FIXED32 ? 4
                                                        : 8;
    if (payload > message->limit - message->encoded || size > message->limit - message->encoded - payload ||
        !array_add(message->records, record))
        return false;
    message->encoded += size + payload;
    return true;
}

/* Append a numeric record without interpreting its schema type. */
bool protobuf_add_integer(protobuf_t *message, uint32_t field, protobuf_wire_t wire, uint64_t value)
{
    if ((wire != PROTOBUF_VARINT && wire != PROTOBUF_FIXED32 && wire != PROTOBUF_FIXED64) ||
        (wire == PROTOBUF_FIXED32 && value > UINT32_MAX))
        return false;
    record_t record = {.field = field, .wire = wire, .integer = value};
    return append(message, &record);
}

/* Copy bytes for a string, nested message, packed values or opaque payload. */
bool protobuf_add_bytes(protobuf_t *message, uint32_t field, const void *data, size_t size)
{
    if (!message || (!data && size) || size > message->limit)
        return false;
    record_t record = {.field = field, .wire = PROTOBUF_BYTES, .size = size};
    if (size) {
        record.bytes = malloc(size);
        if (!record.bytes)
            return false;
        memcpy(record.bytes, data, size);
    }
    if (append(message, &record))
        return true;
    free(record.bytes);
    return false;
}

/* Decode bounded binary framing; length-delimited values remain opaque until schema interpretation. */
protobuf_t *protobuf_decode(const void *data, size_t size, size_t max_bytes, size_t max_fields)
{
    protobuf_t *message = protobuf_new(max_bytes, max_fields);
    if (!message || (!data && size) || size > message->limit)
        goto fail;
    const unsigned char *bytes = data;
    size_t offset = 0;
    while (offset < size) {
        protobuf_wire_field_t field;
        if (!protobuf_wire_read_field(bytes, size, &offset, &field))
            goto fail;
        bool ok = field.wire == PROTOBUF_BYTES
                      ? protobuf_add_bytes(message, field.field, field.data, field.size)
                      : protobuf_add_integer(message, field.field, field.wire, field.integer);
        if (!ok)
            goto fail;
    }
    return message;
fail:
    protobuf_free(message);
    return NULL;
}

static bool write_varint(array_t *bytes, uint64_t value)
{
    unsigned char buffer[10];
    size_t size = 0;
    return protobuf_wire_write_varint(buffer, sizeof(buffer), &size, value) &&
           array_append_carray(bytes, buffer, size);
}

/* Serialise in insertion order; unknown fields and repeated occurrences are retained. */
array_t *protobuf_encode(const protobuf_t *message)
{
    if (!message)
        return NULL;
    array_t *bytes = array_create(1, NULL, NULL);
    bool ok = bytes != NULL;
    for (size_t i = 0; ok && i < array_size(message->records); ++i) {
        const record_t *record = array_get(message->records, i);
        ok = write_varint(bytes, ((uint64_t)record->field << 3) | record->wire);
        if (!ok)
            break;
        if (record->wire == PROTOBUF_VARINT)
            ok = write_varint(bytes, record->integer);
        else if (record->wire == PROTOBUF_BYTES)
            ok = write_varint(bytes, record->size) &&
                 (!record->size || array_append_carray(bytes, record->bytes, record->size));
        else {
            unsigned char buffer[8];
            size_t size = 0;
            ok = protobuf_wire_write_fixed(buffer, sizeof(buffer), &size, record->wire, record->integer) &&
                 array_append_carray(bytes, buffer, size);
        }
    }
    if (!ok) {
        array_destroy(bytes);
        return NULL;
    }
    return bytes;
}

static const record_t *record_at(const protobuf_t *message, size_t index)
{
    return message && index < array_size(message->records) ? array_get(message->records, index) : NULL;
}

/* Count ordered field occurrences. */
size_t protobuf_count(const protobuf_t *message)
{
    return message ? array_size(message->records) : 0;
}

/* Obtain the field number without scanning for duplicate occurrences. */
uint32_t protobuf_field(const protobuf_t *message, size_t index)
{
    const record_t *record = record_at(message, index);
    return record ? record->field : 0;
}

/* Read a record's wire discriminator. */
int protobuf_wire(const protobuf_t *message, size_t index)
{
    const record_t *record = record_at(message, index);
    return record ? (int)record->wire : -1;
}

/* Borrow raw numeric bits; the schema decides signedness and floating-point meaning. */
bool protobuf_integer(const protobuf_t *message, size_t index, uint64_t *value)
{
    const record_t *record = record_at(message, index);
    if (!record || !value || record->wire == PROTOBUF_BYTES)
        return false;
    *value = record->integer;
    return true;
}

/* Borrow a length-delimited field without treating it as a NUL-terminated string. */
const void *protobuf_bytes(const protobuf_t *message, size_t index, size_t *size)
{
    if (!size)
        return NULL;
    const record_t *record = record_at(message, index);
    bool valid = record && record->wire == PROTOBUF_BYTES;
    *size = valid ? record->size : 0;
    return valid ? record->bytes : NULL;
}

/* ZigZag conversion uses defined unsigned arithmetic even for INT64_MIN. */
uint64_t protobuf_zigzag_encode(int64_t value)
{
    return ((uint64_t)value << 1) ^ (value < 0 ? UINT64_MAX : 0);
}

/* Recover signed values without out-of-range unsigned-to-signed conversion. */
int64_t protobuf_zigzag_decode(uint64_t value)
{
    return (value & 1) ? -(int64_t)(value >> 1) - 1 : (int64_t)(value >> 1);
}
