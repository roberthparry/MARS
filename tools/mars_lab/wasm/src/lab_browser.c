/**
 * @file lab_browser.c
 * @brief Freestanding C browser transport and worksheet limits for MARS Lab.
 *
 * Compiled to WebAssembly with library byte operations and no host system imports. Owns bounded Protobuf traversal,
 * schema validation and encoding for proto/lab.proto, using MARSlib's shared
 * allocation-free wire primitives. Host imports only access
 * JavaScript values and UTF-8 strings; mathematics remains in native MARS.
 * One synchronous call owns the fixed buffers; callers copy output before awaiting.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "protobuf_wire-t.h"
#include "lab_host.h"
#include "lab_browser.h"

enum {
    lab_browser_nodes = 65536,
    lab_browser_depth = 32,
    lab_browser_scratch = lab_browser_limit + 512
};
/* At most two open messages per value depth, plus envelope/text, reserve four
 * excess length bytes each. Scratch headroom is not part of the wire budget. */
static unsigned char lab_browser_input[lab_browser_storage_limit], lab_browser_output[lab_browser_scratch];
static uint32_t lab_browser_used, lab_browser_count;
static int lab_browser_failed;

static void lab_browser_byte(unsigned char value)
{
    if (lab_browser_used == lab_browser_scratch)
        lab_browser_failed = 1;
    else
        lab_browser_output[lab_browser_used++] = value;
}

static void lab_browser_varint(uint64_t value)
{
    size_t position = lab_browser_used;
    if (!protobuf_wire_write_varint(lab_browser_output, lab_browser_scratch, &position, value))
        lab_browser_failed = 1;
    lab_browser_used = (uint32_t)position;
}

static uint32_t lab_browser_begin(unsigned field)
{
    lab_browser_varint(((uint64_t)field << 3) | 2);
    uint32_t position = lab_browser_used;
    for (unsigned i = 0; i < 5; ++i)
        lab_browser_byte(0);
    return position;
}

static void lab_browser_end(uint32_t position)
{
    if (lab_browser_failed)
        return;
    uint32_t end = lab_browser_used, length = end - position - 5;
    lab_browser_used = position;
    lab_browser_varint(length);
    uint32_t start = lab_browser_used;
    memmove(lab_browser_output + start, lab_browser_output + position + 5, length);
    lab_browser_used += length;
}

static void lab_browser_text(codec_jsref id, unsigned field, int key_index)
{
    uint32_t position = lab_browser_begin(field);
    if (lab_browser_failed)
        return;
    unsigned char *target = lab_browser_output + lab_browser_used;
    uint32_t capacity = lab_browser_scratch - lab_browser_used;
    int length =
        key_index < 0 ? lab_host_text(id, target, capacity) : lab_host_key_text(id, key_index, target, capacity);
    if (length < 0 || (uint32_t)length > lab_browser_scratch - lab_browser_used) {
        lab_browser_failed = 1;
        return;
    }
    lab_browser_used += (uint32_t)length;
    lab_browser_end(position);
}

/**
 * @brief Encode a host Boolean payload after its value-kind field.
 * @param id Host value identifier whose numeric truth value is read once.
 */
static void lab_browser_write_boolean(codec_jsref id);

/**
 * @brief Encode a finite binary64 payload after its value-kind field.
 * @param id Host value identifier; a non-finite number marks encoding as failed.
 */
static void lab_browser_write_number(codec_jsref id);

/**
 * @brief Encode a host string payload after its value-kind field.
 * @param id Host value identifier passed to the bounded text encoder.
 */
static void lab_browser_write_string(codec_jsref id);

/* Preserve Boolean field emission before reading the host's numeric value. */
static void lab_browser_write_boolean(codec_jsref id)
{
    lab_browser_varint(16);
    lab_browser_varint(lab_host_number(id) != 0);
}

/* Reject non-finite values before emitting their fixed-width payload. */
static void lab_browser_write_number(codec_jsref id)
{
    union {
        double number;
        uint64_t bits;
    } value = {.number = lab_host_number(id)};
    if (((value.bits >> 52) & 2047) == 2047) {
        lab_browser_failed = 1;
        return;
    }
    lab_browser_varint(25);
    size_t position = lab_browser_used;
    if (!protobuf_wire_write_fixed(lab_browser_output, lab_browser_scratch, &position, PROTOBUF_FIXED64, value.bits))
        lab_browser_failed = 1;
    lab_browser_used = (uint32_t)position;
}

/* Retain the bounded text encoder's failure and buffer-update ordering. */
static void lab_browser_write_string(codec_jsref id)
{
    lab_browser_text(id, 4, -1);
}

static void lab_browser_write(int info, unsigned depth)
{
    if (lab_browser_failed || depth > lab_browser_depth || ++lab_browser_count > lab_browser_nodes) {
        lab_browser_failed = 1;
        return;
    }
    lab_value_kind_t kind = (lab_value_kind_t)(info & 7);
    int id = info >> 3;
    if (info < 0 || kind > LAB_VALUE_BYTES) {
        lab_browser_failed = 1;
        return;
    }
    lab_browser_varint(8);
    lab_browser_varint((unsigned)kind);
    if (kind == LAB_VALUE_BYTES) { lab_browser_text(id, 7, -1); return; }
    static void (*const scalar_writers[LAB_VALUE_OBJECT + 1])(codec_jsref) = {
        [LAB_VALUE_BOOLEAN] = lab_browser_write_boolean,
        [LAB_VALUE_NUMBER]  = lab_browser_write_number,
        [LAB_VALUE_STRING]  = lab_browser_write_string,
    };
    if (scalar_writers[kind]) {
        scalar_writers[kind](id);
    } else if (kind >= LAB_VALUE_ARRAY) {
        int count = lab_host_count(id);
        if (count < 0 || count > lab_browser_nodes) {
            lab_browser_failed = 1;
            return;
        }
        for (int i = 0; i < count && !lab_browser_failed; ++i) {
            uint32_t entry = lab_browser_begin(kind == LAB_VALUE_ARRAY ? 5 : 6);
            uint32_t child = 0;
            if (kind == LAB_VALUE_OBJECT) {
                lab_browser_text(id, 1, i);
                child = lab_browser_begin(2);
            }
            lab_browser_write(lab_host_child_info(id, i), depth + 1);
            if (kind == LAB_VALUE_OBJECT)
                lab_browser_end(child);
            lab_browser_end(entry);
        }
    }
}

typedef struct {
    const unsigned char *bytes;
    uint32_t position, end;
} lab_browser_reader_t;

static bool lab_browser_field(lab_browser_reader_t *reader, unsigned *field, protobuf_wire_t *wire, uint64_t *value,
                              lab_browser_reader_t *slice)
{
    size_t position = reader->position;
    protobuf_wire_field_t decoded;
    if (!protobuf_wire_read_field(reader->bytes, reader->end, &position, &decoded))
        return false;
    reader->position = (uint32_t)position;
    *field = decoded.field;
    *wire = decoded.wire;
    *value = decoded.integer;
    if (decoded.wire == PROTOBUF_BYTES)
        *slice = (lab_browser_reader_t){decoded.data, 0, (uint32_t)decoded.size};
    return true;
}

/**
 * @brief Decode one bounded serialised browser value and its descendants.
 * @param reader Reader slice containing exactly one encoded value; its cursor is copied.
 * @param depth Current nesting depth, checked against the decoder's recursion limit.
 * @param parent Host container receiving this value, or -1 for the root value.
 * @param key Object-property key bytes; NULL for an array element or the root value.
 * @param key_length Number of bytes in key; zero when no key is supplied.
 * @return Host value identifier, or -1 for malformed input, exceeded limits or host rejection.
 */
static codec_jsref lab_browser_read(lab_browser_reader_t reader, unsigned depth, codec_jsref parent, const unsigned char *key,
                            uint32_t key_length);

static int lab_browser_entry(lab_browser_reader_t reader, codec_jsref parent, unsigned depth)
{
    lab_browser_reader_t key = {0}, child = {0};
    unsigned seen = 0, fields = 0;
    while (reader.position < reader.end) {
        if (++fields > lab_browser_nodes)
            return 0;
        unsigned field;
        protobuf_wire_t wire;
        uint64_t value;
        lab_browser_reader_t slice = {0};
        if (!lab_browser_field(&reader, &field, &wire, &value, &slice))
            return 0;
        if (field <= 2) {
            if (wire != PROTOBUF_BYTES || (seen & (1u << field)))
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
    return lab_browser_read(child, depth, parent, key.bytes ? key.bytes + key.position : NULL,
                            key.end - key.position) >= 0;
}

static codec_jsref lab_browser_read(lab_browser_reader_t reader, unsigned depth, codec_jsref owner, const unsigned char *key,
                            uint32_t key_length)
{
    if (depth > lab_browser_depth || ++lab_browser_count > lab_browser_nodes)
        return -1;
    lab_browser_reader_t original = reader, text = {0};
    lab_value_kind_t kind = LAB_VALUE_INVALID;
    unsigned seen = 0, containers = 0, fields = 0;
    uint64_t boolean = 0;
    union {
        double number;
        uint64_t bits;
    } number = {0};
    while (reader.position < reader.end) {
        if (++fields > lab_browser_nodes)
            return -1;
        unsigned field;
        protobuf_wire_t wire;
        uint64_t value;
        lab_browser_reader_t slice = {0};
        if (!lab_browser_field(&reader, &field, &wire, &value, &slice))
            return -1;
        if (field <= 4 || field == 7) {
            static const protobuf_wire_t wires[] = {
                PROTOBUF_VARINT, PROTOBUF_VARINT, PROTOBUF_VARINT, PROTOBUF_FIXED64, PROTOBUF_BYTES,
                PROTOBUF_BYTES, PROTOBUF_BYTES, PROTOBUF_BYTES
            };
            if (wire != wires[field] || (seen & (1u << field)))
                return -1;
            seen |= 1u << field;
            if (field == 1) {
                if (value > 6)
                    return -1;
                kind = (lab_value_kind_t)value;
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
            if (wire != PROTOBUF_BYTES)
                return -1;
            containers |= 1u << field;
        }
    }
    if (kind == LAB_VALUE_INVALID || kind > LAB_VALUE_BYTES || !(seen & 2) ||
        (seen & ~((kind == LAB_VALUE_BYTES ? 128u :
                   kind >= LAB_VALUE_BOOLEAN && kind <= LAB_VALUE_STRING ? 1u << (kind + 1) : 0) | 2u)) ||
        (containers & ~(kind >= LAB_VALUE_ARRAY && kind <= LAB_VALUE_OBJECT ? 1u << (kind + 1) : 0)))
        return -1;
    int parent = lab_host_value(kind, kind == LAB_VALUE_BOOLEAN ? (double)boolean : number.number,
                                text.bytes ? text.bytes + text.position : NULL, text.end - text.position, owner, key,
                                key_length);
    if (parent < 0 || kind < LAB_VALUE_ARRAY || kind == LAB_VALUE_BYTES)
        return parent;
    reader = original;
    while (reader.position < reader.end) {
        unsigned field;
        protobuf_wire_t wire;
        uint64_t value;
        lab_browser_reader_t slice = {0};
        if (!lab_browser_field(&reader, &field, &wire, &value, &slice))
            return -1;
        if (field == 6 && !lab_browser_entry(slice, parent, depth + 1))
            return -1;
        if (field == 5) {
            int id = lab_browser_read(slice, depth + 1, parent, NULL, 0);
            if (id < 0)
                return -1;
        }
    }
    return parent;
}

/* Borrow the fixed host-to-WebAssembly input region. */
unsigned char *lab_browser_input_buffer(void)
{
    return lab_browser_input;
}

/* Identify the complete browser ABI independently of the stable wire schema. */
unsigned lab_browser_abi_version(void)
{
    return 47;
}

/* Share the codec's staging bound with browser stream and buffer admission. */
unsigned lab_browser_message_limit(void)
{
    return lab_browser_limit;
}

/* Bound a host container before it snapshots members for native traversal. */
unsigned lab_browser_member_limit(void)
{
    return lab_browser_nodes;
}

/* Retain the host table's existing allowance independently of traversal counting. */
unsigned lab_browser_handle_capacity(void)
{
    return 2 * lab_browser_nodes + 1;
}

/* A padded base64 group carries three message bytes in four characters. */
unsigned lab_browser_base64_limit(void)
{
    return lab_browser_storage_limit;
}

/* Borrow encoded output until the next call. */
unsigned char *lab_browser_output_buffer(void)
{
    return lab_browser_output;
}

/* Encode a host object as a version-one envelope; zero means invalid or too large. */
uint32_t lab_browser_encode(codec_jsref root)
{
    lab_browser_used = lab_browser_count = 0;
    lab_browser_failed = 0;
    if (lab_host_kind(root) != LAB_VALUE_OBJECT)
        return 0;
    lab_browser_varint(8);
    lab_browser_varint(1);
    uint32_t position = lab_browser_begin(2);
    lab_browser_write((root << 3) | 5, 0);
    lab_browser_end(position);
    return lab_browser_failed || lab_browser_used > lab_browser_limit ? 0 : lab_browser_used;
}

/* Validate the complete envelope before returning its host object handle. */
codec_jsref lab_browser_decode(uint32_t size)
{
    if (size > lab_browser_limit)
        return -1;
    lab_browser_count = 0;
    lab_browser_reader_t reader = {lab_browser_input, 0, size}, root = {0};
    unsigned seen = 0, fields = 0;
    while (reader.position < reader.end) {
        if (++fields > lab_browser_nodes)
            return -1;
        unsigned field;
        protobuf_wire_t wire;
        uint64_t value;
        lab_browser_reader_t slice = {0};
        if (!lab_browser_field(&reader, &field, &wire, &value, &slice))
            return -1;
        if (field <= 2) {
            if (seen & (1u << field))
                return -1;
            seen |= 1u << field;
            if (field == 1 && (wire != PROTOBUF_VARINT || value != 1))
                return -1;
            if (field == 2) {
                if (wire != PROTOBUF_BYTES)
                    return -1;
                root = slice;
            }
        }
    }
    int id = seen == 6 ? lab_browser_read(root, 0, -1, NULL, 0) : -1;
    return id >= 0 && lab_host_kind(id) == LAB_VALUE_OBJECT ? id : -1;
}

/* Clamp an already parsed precision value; NaN means use the caller's fallback. */
double lab_browser_precision(double value, double fallback)
{
    if (value != value || value > 1.7976931348623157e308 || value < -1.7976931348623157e308)
        return fallback;
    return value < 17 ? 17 : value > 262144 ? 262144 : value;
}

/* Accept only the five bounded work budgets offered by the native Lab. */
double lab_browser_intervals(double value, double fallback)
{
    return value == 500 || value == 5000 || value == 20000 || value == 50000 || value == 100000 ? value : fallback;
}
