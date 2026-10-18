/**
 * @file tba_browser.c
 * @brief Freestanding C browser transport and bounded input validation for the private Lab.
 *
 * Compiled to WebAssembly without libc or Python. Owns bounded Protobuf traversal,
 * wire validation and encoding for proto/tba.proto. Host imports only access
 * JavaScript values and UTF-8 strings; mathematics remains in native MARS.
 * One synchronous call owns the fixed buffers; callers copy output before awaiting.
 */
#include <stddef.h>
#include <stdint.h>

#include "tba_host.h"
#include "tba_browser.h"

enum {
    tba_browser_nodes = 65536,
    tba_browser_depth = 32,
    tba_browser_scratch = tba_browser_limit + 512
};
/* At most two open messages per value depth, plus envelope/text, reserve four
 * excess length bytes each. Scratch headroom is not part of the wire budget. */
static unsigned char tba_browser_input[tba_browser_limit], tba_browser_output[tba_browser_scratch];
static uint32_t tba_browser_used, tba_browser_count;
static int tba_browser_failed;

static void tba_browser_byte(unsigned char value)
{
    if (tba_browser_used == tba_browser_scratch)
        tba_browser_failed = 1;
    else
        tba_browser_output[tba_browser_used++] = value;
}

static void tba_browser_varint(uint64_t value)
{
    do {
        tba_browser_byte((unsigned char)((value & 127) | (value > 127 ? 128 : 0)));
        value >>= 7;
    } while (value);
}

static uint32_t tba_browser_begin(unsigned field)
{
    tba_browser_varint(((uint64_t)field << 3) | 2);
    uint32_t position = tba_browser_used;
    for (unsigned i = 0; i < 5; ++i)
        tba_browser_byte(0);
    return position;
}

static void tba_browser_end(uint32_t position)
{
    if (tba_browser_failed)
        return;
    uint32_t end = tba_browser_used, length = end - position - 5;
    tba_browser_used = position;
    tba_browser_varint(length);
    uint32_t start = tba_browser_used;
    for (uint32_t i = 0; i < length; ++i)
        tba_browser_output[start + i] = tba_browser_output[position + 5 + i];
    tba_browser_used += length;
}

static void tba_browser_text(codec_jsref id, unsigned field, int key_index)
{
    uint32_t position = tba_browser_begin(field);
    if (tba_browser_failed)
        return;
    unsigned char *target = tba_browser_output + tba_browser_used;
    uint32_t capacity = tba_browser_scratch - tba_browser_used;
    int length =
        key_index < 0 ? tba_host_text(id, target, capacity) : tba_host_key_text(id, key_index, target, capacity);
    if (length < 0 || (uint32_t)length > tba_browser_scratch - tba_browser_used) {
        tba_browser_failed = 1;
        return;
    }
    tba_browser_used += (uint32_t)length;
    tba_browser_end(position);
}

static void tba_browser_write(int info, unsigned depth)
{
    if (tba_browser_failed || depth > tba_browser_depth || ++tba_browser_count > tba_browser_nodes) {
        tba_browser_failed = 1;
        return;
    }
    int kind = info & 7, id = info >> 3;
    if (info < 0 || kind > 5) {
        tba_browser_failed = 1;
        return;
    }
    tba_browser_varint(8);
    tba_browser_varint((unsigned)kind);
    if (kind == 1) {
        tba_browser_varint(16);
        tba_browser_varint(tba_host_number(id) != 0);
    } else if (kind == 2) {
        union {
            double number;
            uint64_t bits;
        } value = {.number = tba_host_number(id)};
        if (((value.bits >> 52) & 2047) == 2047) {
            tba_browser_failed = 1;
            return;
        }
        tba_browser_varint(25);
        for (unsigned i = 0; i < 8; ++i)
            tba_browser_byte((unsigned char)(value.bits >> (8 * i)));
    } else if (kind == 3) {
        tba_browser_text(id, 4, -1);
    } else if (kind >= 4) {
        int count = tba_host_count(id);
        if (count < 0 || count > tba_browser_nodes) {
            tba_browser_failed = 1;
            return;
        }
        for (int i = 0; i < count && !tba_browser_failed; ++i) {
            uint32_t entry = tba_browser_begin(kind == 4 ? 5 : 6);
            uint32_t child = 0;
            if (kind == 5) {
                tba_browser_text(id, 1, i);
                child = tba_browser_begin(2);
            }
            tba_browser_write(tba_host_child_info(id, i), depth + 1);
            if (kind == 5)
                tba_browser_end(child);
            tba_browser_end(entry);
        }
    }
}

typedef struct {
    const unsigned char *bytes;
    uint32_t position, end;
} tba_browser_reader_t;

static int tba_browser_read_varint(tba_browser_reader_t *reader, uint64_t *value)
{
    *value = 0;
    for (unsigned i = 0; i < 10 && reader->position < reader->end; ++i) {
        unsigned byte = reader->bytes[reader->position++];
        if (i == 9 && byte > 1)
            return 0;
        *value |= (uint64_t)(byte & 127) << (7 * i);
        if (!(byte & 128))
            return 1;
    }
    return 0;
}

static int tba_browser_field(tba_browser_reader_t *reader, unsigned *field, unsigned *wire, uint64_t *value,
                             tba_browser_reader_t *slice)
{
    uint64_t tag;
    if (!tba_browser_read_varint(reader, &tag) || tag < 8 || tag >> 3 > 536870911)
        return 0;
    *field = (unsigned)(tag >> 3);
    *wire = tag & 7;
    if (*wire == 0)
        return tba_browser_read_varint(reader, value);
    uint64_t length = *wire == 1 ? 8 : *wire == 5 ? 4 : 0;
    if (*wire == 2) {
        if (!tba_browser_read_varint(reader, &length))
            return 0;
    } else if (!length) {
        return 0;
    }
    if (length > reader->end - reader->position)
        return 0;
    *slice = (tba_browser_reader_t){reader->bytes, reader->position, reader->position + (uint32_t)length};
    *value = 0;
    if (*wire != 2)
        for (unsigned i = 0; i < length; ++i)
            *value |= (uint64_t)reader->bytes[reader->position + i] << (8 * i);
    reader->position += (uint32_t)length;
    return 1;
}

static codec_jsref tba_browser_read(tba_browser_reader_t reader, unsigned depth, codec_jsref parent, const unsigned char *key,
                            uint32_t key_length);

static int tba_browser_entry(tba_browser_reader_t reader, codec_jsref parent, unsigned depth)
{
    tba_browser_reader_t key = {0}, child = {0};
    unsigned seen = 0, fields = 0;
    while (reader.position < reader.end) {
        if (++fields > tba_browser_nodes)
            return 0;
        unsigned field, wire;
        uint64_t value;
        tba_browser_reader_t slice = {0};
        if (!tba_browser_field(&reader, &field, &wire, &value, &slice))
            return 0;
        if (field <= 2) {
            if (wire != 2 || (seen & (1u << field)))
                return 0;
            seen |= 1u << field;
            if (field == 1)
                key = slice;
            else
                child = slice;
        }
    }
    if (!(seen & 4))
        return 0;
    return tba_browser_read(child, depth, parent, key.bytes ? key.bytes + key.position : NULL,
                            key.end - key.position) >= 0;
}

static codec_jsref tba_browser_read(tba_browser_reader_t reader, unsigned depth, codec_jsref owner, const unsigned char *key,
                            uint32_t key_length)
{
    if (depth > tba_browser_depth || ++tba_browser_count > tba_browser_nodes)
        return -1;
    tba_browser_reader_t original = reader, text = {0};
    unsigned kind = 99, seen = 0, containers = 0, fields = 0;
    uint64_t boolean = 0;
    union {
        double number;
        uint64_t bits;
    } number = {0};
    while (reader.position < reader.end) {
        if (++fields > tba_browser_nodes)
            return -1;
        unsigned field, wire;
        uint64_t value;
        tba_browser_reader_t slice = {0};
        if (!tba_browser_field(&reader, &field, &wire, &value, &slice))
            return -1;
        if (field <= 4) {
            static const unsigned wires[] = {0, 0, 0, 1, 2};
            if (wire != wires[field] || (seen & (1u << field)))
                return -1;
            seen |= 1u << field;
            if (field == 1) {
                if (value > 5)
                    return -1;
                kind = (unsigned)value;
            } else if (field == 2) {
                if (value > 1)
                    return -1;
                boolean = value;
            } else if (field == 3) {
                number.bits = value;
                if (((value >> 52) & 2047) == 2047)
                    return -1;
            } else {
                text = slice;
            }
        } else if (field <= 6) {
            if (wire != 2)
                return -1;
            containers |= 1u << field;
        }
    }
    if (kind > 5 || !(seen & 2) || (seen & ~((kind >= 1 && kind <= 3 ? 1u << (kind + 1) : 0) | 2u)) ||
        (containers & ~(kind >= 4 ? 1u << (kind + 1) : 0)))
        return -1;
    int parent = tba_host_value((int)kind, kind == 1 ? (double)boolean : number.number,
                                text.bytes ? text.bytes + text.position : NULL, text.end - text.position, owner, key,
                                key_length);
    if (parent < 0 || kind < 4)
        return parent;
    reader = original;
    while (reader.position < reader.end) {
        unsigned field, wire;
        uint64_t value;
        tba_browser_reader_t slice = {0};
        if (!tba_browser_field(&reader, &field, &wire, &value, &slice))
            return -1;
        if (field == 6 && !tba_browser_entry(slice, parent, depth + 1))
            return -1;
        if (field == 5) {
            int id = tba_browser_read(slice, depth + 1, parent, NULL, 0);
            if (id < 0)
                return -1;
        }
    }
    return parent;
}

/* Borrow the fixed host-to-WebAssembly input region. */
unsigned char *tba_browser_input_buffer(void)
{
    return tba_browser_input;
}

/* Identify the complete browser ABI independently of the stable wire schema. */
unsigned tba_codec_abi_version(void)
{
    return 1;
}

/* Share the codec's staging bound with browser stream and buffer admission. */
unsigned tba_browser_message_limit(void)
{
    return tba_browser_limit;
}

/* Bound a host container before it snapshots members for native traversal. */
unsigned tba_browser_member_limit(void)
{
    return tba_browser_nodes;
}

/* Retain the host table's existing allowance independently of traversal counting. */
unsigned tba_browser_handle_capacity(void)
{
    return 2 * tba_browser_nodes + 1;
}

/* A padded base64 group carries three message bytes in four characters. */
unsigned tba_browser_base64_limit(void)
{
    return ((tba_browser_limit + 2) / 3) * 4;
}

/* Borrow encoded output until the next call. */
unsigned char *tba_browser_output_buffer(void)
{
    return tba_browser_output;
}

/* Encode a host object as a version-one envelope; zero means invalid or too large. */
uint32_t tba_browser_encode(codec_jsref root)
{
    tba_browser_used = tba_browser_count = 0;
    tba_browser_failed = 0;
    if (tba_host_kind(root) != 5)
        return 0;
    tba_browser_varint(8);
    tba_browser_varint(1);
    uint32_t position = tba_browser_begin(2);
    tba_browser_write((root << 3) | 5, 0);
    tba_browser_end(position);
    return tba_browser_failed || tba_browser_used > tba_browser_limit ? 0 : tba_browser_used;
}

/* Validate the complete envelope before returning its host object handle. */
codec_jsref tba_browser_decode(uint32_t size)
{
    if (size > tba_browser_limit)
        return -1;
    tba_browser_count = 0;
    tba_browser_reader_t reader = {tba_browser_input, 0, size}, root = {0};
    unsigned seen = 0, fields = 0;
    while (reader.position < reader.end) {
        if (++fields > tba_browser_nodes)
            return -1;
        unsigned field, wire;
        uint64_t value;
        tba_browser_reader_t slice = {0};
        if (!tba_browser_field(&reader, &field, &wire, &value, &slice))
            return -1;
        if (field <= 2) {
            if (seen & (1u << field))
                return -1;
            seen |= 1u << field;
            if (field == 1 && (wire != 0 || value != 1))
                return -1;
            if (field == 2) {
                if (wire != 2)
                    return -1;
                root = slice;
            }
        }
    }
    int id = seen == 6 ? tba_browser_read(root, 0, -1, NULL, 0) : -1;
    return id >= 0 && tba_host_kind(id) == 5 ? id : -1;
}
