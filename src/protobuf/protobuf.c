/**
 * @file protobuf.c
 * @brief Bounded native Protocol Buffers wire encoding.
 *
 * Owns ordered field records and parses or writes supported wire types with explicit byte and field limits.
 * Repeated and unknown fields remain available to schema-aware callers; no external parser or schema compiler is
 * used.
 *
 * This implements protobuf.h. Applications supply schema knowledge and manage message ownership; the HTTP module
 * provides transport and gRPC framing.
 */

/* Native bounded Protocol Buffers wire records; no external parser or schema compiler. */
#include <stdlib.h>
#include <string.h>

#include "protobuf.h"

typedef struct {
    uint32_t field;
    protobuf_wire_t wire;
    uint64_t integer;
    unsigned char *bytes;
    size_t size;
} record_t;
struct _protobuf_t {
    array_t *records;
    size_t limit, fields, encoded;
};

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

static size_t varint_size(uint64_t value)
{
    size_t size = 1;
    while (value >= 128) {
        value >>= 7;
        ++size;
    }
    return size;
}

static bool append(protobuf_t *message, record_t *record)
{
    if (!message || !record->field || record->field > 536870911 || array_size(message->records) == message->fields)
        return false;
    size_t size = varint_size(((uint64_t)record->field << 3) | record->wire);
    size_t payload = record->wire == PROTOBUF_BYTES     ? varint_size(record->size) + record->size
                     : record->wire == PROTOBUF_VARINT  ? varint_size(record->integer)
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

static bool read_varint(const unsigned char *data, size_t size, size_t *offset, uint64_t *value)
{
    *value = 0;
    for (unsigned i = 0; i < 10 && *offset < size; ++i) {
        unsigned char byte = data[(*offset)++];
        if (i == 9 && byte > 1)
            return false;
        *value |= (uint64_t)(byte & 127) << (i * 7);
        if (!(byte & 128))
            return true;
    }
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
        uint64_t tag, value;
        if (!read_varint(bytes, size, &offset, &tag) || tag > UINT32_MAX || !(tag >> 3))
            goto fail;
        uint32_t field = tag >> 3;
        unsigned wire = tag & 7;
        if (wire == PROTOBUF_VARINT) {
            if (!read_varint(bytes, size, &offset, &value) ||
                !protobuf_add_integer(message, field, PROTOBUF_VARINT, value))
                goto fail;
        } else if (wire == PROTOBUF_BYTES) {
            if (!read_varint(bytes, size, &offset, &value) || value > size - offset ||
                !protobuf_add_bytes(message, field, bytes + offset, (size_t)value))
                goto fail;
            offset += (size_t)value;
        } else if (wire == PROTOBUF_FIXED32 || wire == PROTOBUF_FIXED64) {
            size_t width = wire == PROTOBUF_FIXED32 ? 4 : 8;
            if (width > size - offset)
                goto fail;
            value = 0;
            for (size_t i = 0; i < width; ++i)
                value |= (uint64_t)bytes[offset++] << (8 * i);
            if (!protobuf_add_integer(message, field, (protobuf_wire_t)wire, value))
                goto fail;
        } else
            goto fail;
    }
    return message;
fail:
    protobuf_free(message);
    return NULL;
}

static bool write_varint(array_t *bytes, uint64_t value)
{
    do {
        unsigned char byte = value & 127;
        value >>= 7;
        if (value)
            byte |= 128;
        if (!array_add(bytes, &byte))
            return false;
    } while (value);
    return true;
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
            size_t width = record->wire == PROTOBUF_FIXED32 ? 4 : 8;
            for (size_t j = 0; ok && j < width; ++j) {
                unsigned char byte = record->integer >> (8 * j);
                ok = array_add(bytes, &byte);
            }
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
