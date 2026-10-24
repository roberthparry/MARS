/**
 * @file lab_wire.c
 * @brief Typed, bounded Protobuf adaptation of native Lab request and result trees.
 *
 * Implements proto/lab.proto using MARS protobuf_t and string_t. Rejects duplicate
 * known singular fields, duplicate keys, invalid UTF-8, non-finite numbers and
 * mismatched payload kinds. Unknown supported wire fields are ignored. Traversal
 * visits every message once and is bounded by depth, node and encoded-byte budgets.
 *
 * Numbers use finite binary64 browser semantics, not an exact decimal transport:
 * fractional native values may round (including 0.1 and nonzero subnormals), but
 * integer values must remain exact and nonzero values must not become zero.
 * Signed zero is retained, including a negative JSON zero spelling. Callers must
 * use text for mathematical values requiring precision beyond binary64. Checks
 * apply to the stored native number; precision lost before this adapter cannot
 * be recovered here.
 */
#include <math.h>
#include <string.h>

#include "lab_wire.h"

enum { lab_wire_limit = 8388608, lab_wire_nodes = 65536, lab_wire_depth = 32 };

static bool lab_wire_child(protobuf_t *parent, unsigned field, protobuf_t *child)
{
    array_t *bytes = child ? protobuf_encode(child) : NULL;
    bool ok = bytes && protobuf_add_bytes(parent, field, array_size(bytes) ? array_get(bytes, 0) : NULL,
                                          array_size(bytes));
    array_destroy(bytes);
    protobuf_free(child);
    return ok;
}

static bool lab_wire_text(protobuf_t *message, unsigned field, const string_t *text)
{
    return text && protobuf_add_bytes(message, field, string_c_str(text), string_byte_length(text));
}

static bool lab_wire_number(const json_t *value, double *converted)
{
    number_t number;
    if (!json_number_value(value, &number))
        return false;
    *converted = num_to_double(number);
    bool ok = num_is_real(number) && num_is_finite(number) && isfinite(*converted) &&
              (*converted != 0.0 || num_is_zero(number));
    if (ok && num_is_integer(number) && num_get_prec_bits(number) != 0) {
        /* Preserve the original floating-point representation and precision so
         * the comparison does not depend on the default working precision. */
        number_t exact = num_clone(number);
        ok = num_set_double(&exact, *converted) == 0 && num_eq(number, exact);
        num_destroy(&exact);
    } else if (ok && num_is_integer(number)) {
        /* Comparing directly with NUMBER_DOUBLE would coerce exact integers to
         * double first. Parse its full integer spelling instead (at most 309 digits). */
        string_t *text = string_new();
        ok = text && string_append_format(text, "%.0f", *converted) >= 0;
        if (ok) {
            number_t exact = num_create_from_text(text);
            ok = num_eq(number, exact);
            num_destroy(&exact);
        }
        string_free(text);
    }
    if (ok && *converted == 0.0) {
        const string_t *text = json_number_text(value);
        if (text && string_byte_length(text) && string_c_str(text)[0] == '-')
            *converted = -0.0;
    }
    num_destroy(&number);
    return ok;
}

/* Wrap opaque bytes in the native tree; the reserved wrapper never reaches the browser. */
json_t *lab_wire_binary(const void *bytes, size_t size)
{
    string_t *raw = string_new();
    json_t *object = json_new_object();
    bool ok = raw && object && size <= lab_wire_limit && string_append_bytes(raw, bytes, size) == 0;
    json_t *value = ok ? json_new_string(raw) : NULL;
    string_t *key = string_new_with("$lab_binary");
    ok = value && key && json_object_set(object, key, value);
    string_free(key);
    json_free(value);
    string_free(raw);
    if (!ok) { json_free(object); return NULL; }
    return object;
}

static protobuf_t *lab_wire_write(const json_t *value, unsigned depth, unsigned *nodes)
{
    if (!value || depth > lab_wire_depth || ++*nodes > lab_wire_nodes)
        return NULL;
    unsigned kind = (unsigned)json_type(value);
    string_t *binary_key = kind == JSON_OBJECT && json_object_size(value) == 1 ? string_new_with("$lab_binary") : NULL;
    const json_t *binary = binary_key
        ? json_object_get(value, binary_key) : NULL;
    string_free(binary_key);
    if (binary && json_type(binary) == JSON_STRING) kind = 6;
    if (kind == 6 && depth == 0) return NULL;
    protobuf_t *message = protobuf_new(lab_wire_limit, lab_wire_nodes);
    bool ok = message && kind <= 6 && protobuf_add_integer(message, 1, PROTOBUF_VARINT, kind);
    if (ok && kind == 6) {
        ok = lab_wire_text(message, 7, json_string_value(binary));
        if (!ok) { protobuf_free(message); return NULL; }
        return message;
    }
    if (ok && kind == JSON_BOOL) {
        bool flag;
        ok = json_bool_value(value, &flag) && protobuf_add_integer(message, 2, PROTOBUF_VARINT, flag);
    } else if (ok && kind == JSON_NUMBER) {
        union { double number; uint64_t bits; } converted = {0};
        ok = lab_wire_number(value, &converted.number) &&
             protobuf_add_integer(message, 3, PROTOBUF_FIXED64, converted.bits);
    } else if (ok && kind == JSON_STRING) {
        ok = lab_wire_text(message, 4, json_string_value(value));
    } else if (ok && kind >= JSON_ARRAY) {
        size_t count = kind == JSON_ARRAY ? json_array_size(value) : json_object_size(value);
        if (count > lab_wire_nodes)
            ok = false;
        for (size_t i = 0; ok && i < count; ++i) {
            if (kind == JSON_ARRAY) {
                ok = lab_wire_child(message, 5, lab_wire_write(json_array_get(value, i), depth + 1, nodes));
            } else {
                protobuf_t *entry = protobuf_new(lab_wire_limit, 2);
                bool ready = entry && lab_wire_text(entry, 1, json_object_key_at(value, i)) &&
                             lab_wire_child(entry, 2, lab_wire_write(json_object_value_at(value, i), depth + 1, nodes));
                if (!ready) {
                    protobuf_free(entry);
                    ok = false;
                } else {
                    ok = lab_wire_child(message, 6, entry);
                }
            }
        }
    }
    if (!ok) {
        protobuf_free(message);
        message = NULL;
    }
    return message;
}

static string_t *lab_wire_read_text(const protobuf_t *message, size_t index)
{
    size_t size;
    const void *bytes = protobuf_bytes(message, index, &size);
    string_t *text = string_new();
    if (text && string_append_utf8_exact(text, bytes, size)) {
        string_free(text);
        text = NULL;
    }
    return text;
}

static protobuf_t *lab_wire_read_child(const protobuf_t *message, size_t index)
{
    size_t size;
    const void *bytes = protobuf_bytes(message, index, &size);
    return protobuf_decode(bytes, size, lab_wire_limit, lab_wire_nodes);
}

static json_t *lab_wire_read(const protobuf_t *message, unsigned depth, unsigned *nodes);

static bool lab_wire_entry(json_t *object, const protobuf_t *entry, unsigned depth, unsigned *nodes)
{
    string_t *key = NULL;
    json_t *value = NULL;
    unsigned seen = 0;
    bool ok = entry != NULL;
    for (size_t i = 0; ok && i < protobuf_count(entry); ++i) {
        unsigned field = protobuf_field(entry, i);
        if (field > 2)
            continue;
        ok = !(seen & (1u << field)) && protobuf_wire(entry, i) == PROTOBUF_BYTES;
        seen |= 1u << field;
        if (ok && field == 1)
            ok = (key = lab_wire_read_text(entry, i)) != NULL;
        else if (ok) {
            protobuf_t *child = lab_wire_read_child(entry, i);
            value = lab_wire_read(child, depth, nodes);
            protobuf_free(child);
            ok = value != NULL;
        }
    }
    if (ok && !key)
        key = string_new();
    ok = ok && key && value && !json_object_get(object, key) && json_object_set(object, key, value);
    string_free(key);
    json_free(value);
    return ok;
}

static json_t *lab_wire_read(const protobuf_t *message, unsigned depth, unsigned *nodes)
{
    if (!message || depth > lab_wire_depth || ++*nodes > lab_wire_nodes)
        return NULL;
    size_t indexes[8] = {0};
    unsigned seen = 0, containers = 0;
    uint64_t kind = 99;
    for (size_t i = 0; i < protobuf_count(message); ++i) {
        unsigned field = protobuf_field(message, i);
        if (field <= 4 || field == 7) {
            static const int wires[] = {0, PROTOBUF_VARINT, PROTOBUF_VARINT, PROTOBUF_FIXED64, PROTOBUF_BYTES,
                                       0, 0, PROTOBUF_BYTES};
            if ((seen & (1u << field)) || protobuf_wire(message, i) != wires[field])
                return NULL;
            seen |= 1u << field;
            indexes[field] = i;
        } else if (field <= 6) {
            if (protobuf_wire(message, i) != PROTOBUF_BYTES)
                return NULL;
            containers |= 1u << field;
        }
    }
    if (!(seen & 2) || !protobuf_integer(message, indexes[1], &kind) || kind > 6 ||
        (seen & ~((kind == 6 ? 128u : kind >= 1 && kind <= 3 ? 1u << (kind + 1) : 0) | 2u)) ||
        (containers & ~(kind >= 4 && kind <= 5 ? 1u << (kind + 1) : 0)))
        return NULL;
    if (kind == 6) {
        if (depth == 0) return NULL;
        size_t length = 0;
        const void *bytes = seen & 128 ? protobuf_bytes(message, indexes[7], &length) : NULL;
        return lab_wire_binary(bytes, length);
    }
    json_t *value = NULL;
    if (kind == JSON_NULL)
        value = json_new_null();
    else if (kind == JSON_BOOL) {
        uint64_t flag = 0;
        if ((!(seen & 4) || protobuf_integer(message, indexes[2], &flag)) && flag <= 1)
            value = json_new_bool(flag != 0);
    } else if (kind == JSON_NUMBER) {
        union { double number; uint64_t bits; } converted = {0};
        if ((!(seen & 8) || protobuf_integer(message, indexes[3], &converted.bits)) && isfinite(converted.number)) {
            number_t number = num_create_from_double(converted.number);
            value = json_new_number_value(number);
            num_destroy(&number);
        }
    } else if (kind == JSON_STRING) {
        string_t *text = seen & 16 ? lab_wire_read_text(message, indexes[4]) : string_new();
        value = text ? json_new_string(text) : NULL;
        string_free(text);
    } else {
        value = kind == JSON_ARRAY ? json_new_array() : json_new_object();
        for (size_t i = 0; value && i < protobuf_count(message); ++i) {
            if (protobuf_field(message, i) != kind + 1)
                continue;
            protobuf_t *child = lab_wire_read_child(message, i);
            bool ok;
            if (kind == JSON_ARRAY) {
                json_t *element = lab_wire_read(child, depth + 1, nodes);
                ok = element && json_array_append(value, element);
                json_free(element);
            } else {
                ok = lab_wire_entry(value, child, depth + 1, nodes);
            }
            protobuf_free(child);
            if (!ok) {
                json_free(value);
                value = NULL;
            }
        }
    }
    return value;
}

/* Encode only object roots; the schema version is explicit on every message. */
array_t *lab_wire_encode(const json_t *value)
{
    unsigned nodes = 0;
    protobuf_t *message = protobuf_new(lab_wire_limit, 2);
    bool ok = value && json_type(value) == JSON_OBJECT && message &&
              protobuf_add_integer(message, 1, PROTOBUF_VARINT, 1) &&
              lab_wire_child(message, 2, lab_wire_write(value, 0, &nodes));
    array_t *bytes = ok ? protobuf_encode(message) : NULL;
    protobuf_free(message);
    return bytes;
}

static const json_t *response_field(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = key ? json_object_get(object, key) : NULL;
    string_free(key);
    return value;
}

/* Keep the presentation image once; diagnostic records remain intact on failed evaluations. */
array_t *lab_wire_encode_response(const json_t *value, bool *failed)
{
    const json_t *plot = response_field(response_field(response_field(value, "presentation"), "calculus"), "plot");
    const string_t *svg = json_string_value(response_field(plot, "svg"));
    bool success = false;
    json_bool_value(response_field(value, "ok"), &success);
    json_t *compact = success && svg && string_byte_length(svg) ? json_new_object() : NULL;
    bool ready = !(success && svg && string_byte_length(svg)) || compact;
    for (size_t i = 0; ready && compact && i < json_object_size(value); ++i) {
        const string_t *key = json_object_key_at(value, i);
        const char *name = string_c_str(key);
        if (!strcmp(name, "raw") || !strcmp(name, "plot_svg"))
            continue;
        ready = json_object_set(compact, key, json_object_value_at(value, i));
    }
    array_t *bytes = ready ? lab_wire_encode(compact ? compact : value) : NULL;
    json_free(compact);
    if (failed)
        *failed = !bytes;
    if (!bytes) {
        string_t *text = string_new_with("{\"ok\":false,\"error\":\"Lab response could not be encoded within the "
                                          "8 MiB, 65536-value or depth-32 transport limits. Reduce result size.\"}");
        json_t *error = text ? json_from_text(text) : NULL;
        bytes = error ? lab_wire_encode(error) : NULL;
        json_free(error);
        string_free(text);
    }
    return bytes;
}

/* Reject incomplete, duplicate or incompatible envelopes before dispatch. */
json_t *lab_wire_decode(const void *bytes, size_t size)
{
    protobuf_t *message = protobuf_decode(bytes, size, lab_wire_limit, lab_wire_nodes);
    unsigned seen = 0, nodes = 0;
    json_t *result = NULL;
    bool ok = message != NULL;
    for (size_t i = 0; ok && i < protobuf_count(message); ++i) {
        unsigned field = protobuf_field(message, i);
        if (field > 2)
            continue;
        ok = !(seen & (1u << field));
        seen |= 1u << field;
        if (ok && field == 1) {
            uint64_t version;
            ok = protobuf_wire(message, i) == PROTOBUF_VARINT && protobuf_integer(message, i, &version) && version == 1;
        } else if (ok) {
            ok = protobuf_wire(message, i) == PROTOBUF_BYTES;
            protobuf_t *child = ok ? lab_wire_read_child(message, i) : NULL;
            result = lab_wire_read(child, 0, &nodes);
            protobuf_free(child);
            ok = result && json_type(result) == JSON_OBJECT;
        }
    }
    protobuf_free(message);
    if (!ok || seen != 6) {
        json_free(result);
        result = NULL;
    }
    return result;
}
