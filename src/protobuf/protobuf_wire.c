/**
 * @file protobuf_wire.c
 * @brief Shared bounded Protobuf wire parsing and numeric encoding.
 *
 * Native record ownership and browser value traversal both use these byte-level
 * operations. The component has no allocation, threading or library dependencies;
 * it compiles unchanged for native and WebAssembly targets. Reads validate into
 * local temporaries and writes check capacity before changing caller storage.
 */
#include "protobuf_wire-t.h"

/* Count the minimal unsigned base-128 representation. */
size_t protobuf_wire_varint_size(uint64_t value)
{
    size_t size = 1;
    while (value >= 128) {
        value >>= 7;
        ++size;
    }
    return size;
}

/* Validate all ten possible bytes before publishing the decoded value and cursor. */
bool protobuf_wire_read_varint(const unsigned char *data, size_t size, size_t *offset, uint64_t *value)
{
    if (!offset || !value || (!data && size) || *offset > size)
        return false;
    size_t position = *offset;
    uint64_t result = 0;
    for (unsigned i = 0; i < 10 && position < size; ++i) {
        unsigned char byte = data[position++];
        if (i == 9 && byte > 1)
            return false;
        result |= (uint64_t)(byte & 127) << (i * 7);
        if (!(byte & 128)) {
            *offset = position;
            *value = result;
            return true;
        }
    }
    return false;
}

/* Preflight capacity so a rejected write cannot leave a partial varint. */
bool protobuf_wire_write_varint(unsigned char *data, size_t size, size_t *offset, uint64_t value)
{
    if (!offset || (!data && size) || *offset > size || protobuf_wire_varint_size(value) > size - *offset)
        return false;
    do {
        unsigned char byte = value & 127;
        value >>= 7;
        data[(*offset)++] = byte | (value ? 128 : 0);
    } while (value);
    return true;
}

/* Decode numeric bits or borrow a length-delimited span within the supplied bound. */
bool protobuf_wire_read_field(const unsigned char *data, size_t size, size_t *offset, protobuf_wire_field_t *field)
{
    if (!offset || !field)
        return false;
    size_t position = *offset;
    uint64_t tag;
    if (!protobuf_wire_read_varint(data, size, &position, &tag) || tag > UINT32_MAX || !(tag >> 3))
        return false;
    protobuf_wire_field_t result = {.field = (uint32_t)(tag >> 3), .wire = (protobuf_wire_t)(tag & 7)};
    if (result.wire == PROTOBUF_VARINT) {
        if (!protobuf_wire_read_varint(data, size, &position, &result.integer))
            return false;
    } else if (result.wire == PROTOBUF_BYTES) {
        uint64_t length;
        if (!protobuf_wire_read_varint(data, size, &position, &length) || length > size - position)
            return false;
        result.data = data + position;
        result.size = (size_t)length;
        position += result.size;
    } else if (result.wire == PROTOBUF_FIXED32 || result.wire == PROTOBUF_FIXED64) {
        size_t width = result.wire == PROTOBUF_FIXED32 ? 4 : 8;
        if (width > size - position)
            return false;
        for (size_t i = 0; i < width; ++i)
            result.integer |= (uint64_t)data[position++] << (8 * i);
    } else {
        return false;
    }
    *offset = position;
    *field = result;
    return true;
}

/* Preserve raw integer or floating-point bits in the standard little-endian order. */
bool protobuf_wire_write_fixed(unsigned char *data, size_t size, size_t *offset, protobuf_wire_t wire, uint64_t value)
{
    if ((wire != PROTOBUF_FIXED32 && wire != PROTOBUF_FIXED64) ||
        (wire == PROTOBUF_FIXED32 && value > UINT32_MAX))
        return false;
    size_t width = wire == PROTOBUF_FIXED32 ? 4 : 8;
    if (!offset || (!data && size) || *offset > size || width > size - *offset)
        return false;
    for (size_t i = 0; i < width; ++i)
        data[(*offset)++] = (unsigned char)(value >> (8 * i));
    return true;
}
