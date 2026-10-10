/**
 * @file lab_browser.c
 * @brief Freestanding C browser transport and worksheet limits for MARS Lab.
 *
 * Compiled to WebAssembly without libc or Python. Owns bounded Protobuf traversal,
 * wire validation and encoding for proto/lab.proto. Host imports only access
 * JavaScript values and UTF-8 strings; mathematics remains in native MARS.
 * One synchronous call owns the fixed buffers; callers copy output before awaiting.
 */
#include <stddef.h>
#include <stdint.h>

#include "lab_host.h"
#include "lab_browser.h"

enum {
    lab_browser_limit = 4194304,
    lab_browser_nodes = 65536,
    lab_browser_depth = 32,
    lab_browser_scratch = lab_browser_limit + 512
};
/* At most two open messages per value depth, plus envelope/text, reserve four
 * excess length bytes each. Scratch headroom is not part of the wire budget. */
static unsigned char lab_browser_input[lab_browser_limit], lab_browser_output[lab_browser_scratch];
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
    do {
        lab_browser_byte((unsigned char)((value & 127) | (value > 127 ? 128 : 0)));
        value >>= 7;
    } while (value);
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
    for (uint32_t i = 0; i < length; ++i)
        lab_browser_output[start + i] = lab_browser_output[position + 5 + i];
    lab_browser_used += length;
}

static void lab_browser_text(int id, unsigned field, int key_index)
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

static void lab_browser_write(int info, unsigned depth)
{
    if (lab_browser_failed || depth > lab_browser_depth || ++lab_browser_count > lab_browser_nodes) {
        lab_browser_failed = 1;
        return;
    }
    int kind = info & 7, id = info >> 3;
    if (info < 0 || kind > 5) {
        lab_browser_failed = 1;
        return;
    }
    lab_browser_varint(8);
    lab_browser_varint((unsigned)kind);
    if (kind == 1) {
        lab_browser_varint(16);
        lab_browser_varint(lab_host_number(id) != 0);
    } else if (kind == 2) {
        union {
            double number;
            uint64_t bits;
        } value = {.number = lab_host_number(id)};
        if (((value.bits >> 52) & 2047) == 2047) {
            lab_browser_failed = 1;
            return;
        }
        lab_browser_varint(25);
        for (unsigned i = 0; i < 8; ++i)
            lab_browser_byte((unsigned char)(value.bits >> (8 * i)));
    } else if (kind == 3) {
        lab_browser_text(id, 4, -1);
    } else if (kind >= 4) {
        int count = lab_host_count(id);
        if (count < 0 || count > lab_browser_nodes) {
            lab_browser_failed = 1;
            return;
        }
        for (int i = 0; i < count && !lab_browser_failed; ++i) {
            uint32_t entry = lab_browser_begin(kind == 4 ? 5 : 6);
            uint32_t child = 0;
            if (kind == 5) {
                lab_browser_text(id, 1, i);
                child = lab_browser_begin(2);
            }
            lab_browser_write(lab_host_child_info(id, i), depth + 1);
            if (kind == 5)
                lab_browser_end(child);
            lab_browser_end(entry);
        }
    }
}

typedef struct {
    const unsigned char *bytes;
    uint32_t position, end;
} lab_browser_reader_t;

static int lab_browser_read_varint(lab_browser_reader_t *reader, uint64_t *value)
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

static int lab_browser_field(lab_browser_reader_t *reader, unsigned *field, unsigned *wire, uint64_t *value,
                             lab_browser_reader_t *slice)
{
    uint64_t tag;
    if (!lab_browser_read_varint(reader, &tag) || tag < 8 || tag >> 3 > 536870911)
        return 0;
    *field = (unsigned)(tag >> 3);
    *wire = tag & 7;
    if (*wire == 0)
        return lab_browser_read_varint(reader, value);
    uint64_t length = *wire == 1 ? 8 : *wire == 5 ? 4 : 0;
    if (*wire == 2) {
        if (!lab_browser_read_varint(reader, &length))
            return 0;
    } else if (!length) {
        return 0;
    }
    if (length > reader->end - reader->position)
        return 0;
    *slice = (lab_browser_reader_t){reader->bytes, reader->position, reader->position + (uint32_t)length};
    *value = 0;
    if (*wire != 2)
        for (unsigned i = 0; i < length; ++i)
            *value |= (uint64_t)reader->bytes[reader->position + i] << (8 * i);
    reader->position += (uint32_t)length;
    return 1;
}

static int lab_browser_read(lab_browser_reader_t reader, unsigned depth, int parent, const unsigned char *key,
                            uint32_t key_length);

static int lab_browser_entry(lab_browser_reader_t reader, int parent, unsigned depth)
{
    lab_browser_reader_t key = {0}, child = {0};
    unsigned seen = 0, fields = 0;
    while (reader.position < reader.end) {
        if (++fields > lab_browser_nodes)
            return 0;
        unsigned field, wire;
        uint64_t value;
        lab_browser_reader_t slice = {0};
        if (!lab_browser_field(&reader, &field, &wire, &value, &slice))
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
    return lab_browser_read(child, depth, parent, key.bytes ? key.bytes + key.position : NULL,
                            key.end - key.position) >= 0;
}

static int lab_browser_read(lab_browser_reader_t reader, unsigned depth, int owner, const unsigned char *key,
                            uint32_t key_length)
{
    if (depth > lab_browser_depth || ++lab_browser_count > lab_browser_nodes)
        return -1;
    lab_browser_reader_t original = reader, text = {0};
    unsigned kind = 99, seen = 0, containers = 0, fields = 0;
    uint64_t boolean = 0;
    union {
        double number;
        uint64_t bits;
    } number = {0};
    while (reader.position < reader.end) {
        if (++fields > lab_browser_nodes)
            return -1;
        unsigned field, wire;
        uint64_t value;
        lab_browser_reader_t slice = {0};
        if (!lab_browser_field(&reader, &field, &wire, &value, &slice))
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
    int parent = lab_host_value((int)kind, kind == 1 ? (double)boolean : number.number,
                                text.bytes ? text.bytes + text.position : NULL, text.end - text.position, owner, key,
                                key_length);
    if (parent < 0 || kind < 4)
        return parent;
    reader = original;
    while (reader.position < reader.end) {
        unsigned field, wire;
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
    return 30;
}

/* Borrow encoded output until the next call. */
unsigned char *lab_browser_output_buffer(void)
{
    return lab_browser_output;
}

/* Encode a host object as a version-one envelope; zero means invalid or too large. */
uint32_t lab_browser_encode(int root)
{
    lab_browser_used = lab_browser_count = 0;
    lab_browser_failed = 0;
    if (lab_host_kind(root) != 5)
        return 0;
    lab_browser_varint(8);
    lab_browser_varint(1);
    uint32_t position = lab_browser_begin(2);
    lab_browser_write((root << 3) | 5, 0);
    lab_browser_end(position);
    return lab_browser_failed || lab_browser_used > lab_browser_limit ? 0 : lab_browser_used;
}

/* Validate the complete envelope before returning its host object handle. */
int lab_browser_decode(uint32_t size)
{
    if (size > lab_browser_limit)
        return -1;
    lab_browser_count = 0;
    lab_browser_reader_t reader = {lab_browser_input, 0, size}, root = {0};
    unsigned seen = 0, fields = 0;
    while (reader.position < reader.end) {
        if (++fields > lab_browser_nodes)
            return -1;
        unsigned field, wire;
        uint64_t value;
        lab_browser_reader_t slice = {0};
        if (!lab_browser_field(&reader, &field, &wire, &value, &slice))
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
    int id = seen == 6 ? lab_browser_read(root, 0, -1, NULL, 0) : -1;
    return id >= 0 && lab_host_kind(id) == 5 ? id : -1;
}

/* Clamp an already parsed precision value; NaN means use the caller's fallback. */
double lab_browser_precision(double value, double fallback)
{
    if (value != value || value > 1.7976931348623157e308 || value < -1.7976931348623157e308)
        return fallback;
    return value < 17 ? 17 : value > 1048576 ? 1048576 : value;
}

/* Accept only the five bounded work budgets offered by the native Lab. */
double lab_browser_intervals(double value, double fallback)
{
    return value == 500 || value == 5000 || value == 20000 || value == 50000 || value == 100000 ? value : fallback;
}
