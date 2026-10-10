/**
 * @file test_lab_wire.c
 * @brief Native Protobuf envelope and schema regressions for MARS Lab.
 *
 * Checks a golden wire vector shared with the browser, nested scalar fidelity,
 * malformed and incompatible envelopes, schema payload restrictions and bounds.
 * Uses the public protobuf module to construct deliberately hostile messages.
 */
#include <stdlib.h>
#include <string.h>

#include "lab_wire.h"
#include "test_harness.h"
#include "test_lab_support.h"

static json_t *lab_wire_test_decode_value(const unsigned char *bytes, size_t size)
{
    protobuf_t *envelope = protobuf_new(0, 0);
    bool ok = envelope && protobuf_add_integer(envelope, 1, PROTOBUF_VARINT, 1) &&
              protobuf_add_bytes(envelope, 2, bytes, size);
    array_t *encoded = ok ? protobuf_encode(envelope) : NULL;
    json_t *value = encoded ? lab_wire_decode(array_get(encoded, 0), array_size(encoded)) : NULL;
    array_destroy(encoded);
    protobuf_free(envelope);
    return value;
}

static void test_lab_wire_roundtrip(void)
{
    static const unsigned char golden[] = {8, 1, 18, 2, 8, 5};
    json_t *empty = json_new_object();
    array_t *bytes = lab_wire_encode(empty);
    bool ok = bytes && array_size(bytes) == sizeof golden && !memcmp(array_get(bytes, 0), golden, sizeof golden);
    array_destroy(bytes);
    json_free(empty);
    TEST_ASSERT_TRUE(ok, "empty envelope matches the browser golden vector");
    const char *source = "{\"unicode\":\"μσ e\\u0301 🪐\\u0000\",\"nil\":null,\"yes\":true,"
                         "\"no\":false,\"number\":1.25,\"exact\":\"1/3\",\"list\":[0,\"\",{\"nested\":[\"π\",42]}],"
                         "\"empty\":{},\"__proto__\":{\"safe\":true}}";
    json_t *input = test_lab_json(source);
    bytes = lab_wire_encode(input);
    json_t *output = bytes ? lab_wire_decode(array_get(bytes, 0), array_size(bytes)) : NULL;
    string_t *before = json_to_string(input), *after = output ? json_to_string(output) : NULL;
    ok = before && after && string_compare(before, after) == 0;
    string_free(before);
    string_free(after);
    array_destroy(bytes);
    json_free(output);
    json_free(input);
    TEST_ASSERT_TRUE(ok, "nested values preserve Unicode, NUL, numbers, booleans and mathematical text");
}

static void test_lab_wire_rejections(void)
{
    static const unsigned char golden[] = {8, 1, 18, 2, 8, 5};
    bool ok = true;
    for (size_t size = 0; size < sizeof golden; ++size) {
        json_t *value = lab_wire_decode(golden, size);
        ok = !value && ok;
        json_free(value);
    }
    static const unsigned char invalid[][12] = {
        {8, 2, 18, 2, 8, 5}, {8, 1, 18, 2, 8, 5, 8, 1}, {8, 1, 18, 2, 8, 5, 0},
        {8, 1, 18, 2, 8, 5, 27}, {8, 1, 18, 2, 8, 5, 18, 2, 8, 5}
    };
    static const size_t sizes[] = {6, 8, 7, 7, 10};
    for (size_t i = 0; i < sizeof sizes / sizeof *sizes; ++i) {
        json_t *value = lab_wire_decode(invalid[i], sizes[i]);
        ok = !value && ok;
        json_free(value);
    }
    static const unsigned char unknown[] = {8, 1, 18, 2, 8, 5, 120, 1};
    json_t *accepted = lab_wire_decode(unknown, sizeof unknown);
    ok = accepted && ok;
    json_free(accepted);
    TEST_ASSERT_TRUE(ok, "truncation, invalid tags, groups and duplicate/version fields rejected; unknown fields skipped");
}

static void test_lab_wire_schema(void)
{
    static const unsigned char invalid[][24] = {
        {8, 5, 8, 5}, /* Duplicate kind. */
        {8, 5, 16, 1}, /* Boolean payload on an object. */
        {8, 5, 50, 8, 10, 1, 120, 18, 3, 8, 3, 34}, /* Truncated string child. */
        {8, 5, 50, 11, 10, 1, 120, 18, 6, 8, 3, 34, 2, 192, 128}, /* Invalid UTF-8. */
        {8, 5, 50, 9, 10, 1, 120, 18, 4, 8, 1, 16, 2}, /* Invalid boolean. */
        {8, 5, 50, 5, 18, 3, 8, 0, 0}, /* Invalid nested tag. */
        {8, 5, 50, 4, 18, 2, 8, 0, 50, 4, 18, 2, 8, 0} /* Duplicate empty key. */
    };
    static const size_t sizes[] = {4, 4, 12, 15, 13, 9, 14};
    bool ok = true;
    for (size_t i = 0; i < sizeof sizes / sizeof *sizes; ++i) {
        json_t *value = lab_wire_test_decode_value(invalid[i], sizes[i]);
        ok = !value && ok;
        json_free(value);
    }
    json_t *deep = json_new_object();
    string_t *key = string_new_with("child");
    for (unsigned i = 0; deep && i < 34; ++i) {
        json_t *parent = json_new_object();
        bool ready = parent && key && json_object_set(parent, key, deep);
        json_free(deep);
        deep = ready ? parent : NULL;
        if (!ready)
            json_free(parent);
    }
    array_t *bytes = deep ? lab_wire_encode(deep) : NULL;
    ok = deep && !bytes && ok;
    array_destroy(bytes);
    json_free(deep);
    string_free(key);
    TEST_ASSERT_TRUE(ok, "kind mismatch, invalid payloads, duplicate keys and excessive nesting rejected");
}

static void test_lab_wire_number_policy(void)
{
    static const struct {
        const char *json;
        bool accepted;
    } cases[] = {
        {"{\"x\":0.1}", true},
        {"{\"x\":-0}", true},
        {"{\"x\":-0.0}", true},
        {"{\"x\":9007199254740992}", true},
        {"{\"x\":9007199254740994}", true},
        {"{\"x\":-9007199254740994}", true},
        {"{\"x\":9007199254740993}", false},
        {"{\"x\":-9007199254740993}", false},
        {"{\"x\":9007199254740993.0}", false},
        {"{\"x\":18446744073709551615}", false},
        {"{\"x\":1e-323}", true},
        {"{\"x\":-1e-323}", true},
        {"{\"x\":1e-400}", false},
        {"{\"x\":-1e-400}", false},
        {"{\"x\":1e400}", false},
        {"{\"x\":-1e400}", false},
        {"{\"x\":\"9007199254740993\"}", true}
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        json_t *value = test_lab_json(cases[i].json);
        array_t *bytes = value ? lab_wire_encode(value) : NULL;
        bool ok = value && (bytes != NULL) == cases[i].accepted;
        if (ok && bytes) {
            json_t *decoded = lab_wire_decode(array_get(bytes, 0), array_size(bytes));
            ok = decoded != NULL;
            if (ok && (i == 1 || i == 2)) {
                number_t number;
                ok = json_number_value(test_lab_member(decoded, "x"), &number);
                if (ok) {
                    union { double number; uint64_t bits; } zero = {.number = num_to_double(number)};
                    ok = zero.bits == UINT64_C(0x8000000000000000);
                    num_destroy(&number);
                }
            }
            json_free(decoded);
        }
        array_destroy(bytes);
        json_free(value);
        TEST_ASSERT_TRUE(ok, cases[i].json);
    }
}

static void test_lab_wire_number_vectors(void)
{
    /* Fixed64 little-endian bytes, independent of native floating-point formatting. */
    static const unsigned char payloads[][8] = {
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 128},
        {1, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 128},
        {255, 255, 255, 255, 255, 255, 15, 0},
        {0, 0, 0, 0, 0, 0, 16, 0},
        {154, 153, 153, 153, 153, 153, 185, 63},
        {255, 255, 255, 255, 255, 255, 239, 127},
        {0, 0, 0, 0, 0, 0, 240, 127},
        {0, 0, 0, 0, 0, 0, 240, 255},
        {1, 0, 0, 0, 0, 0, 248, 127}
    };
    for (size_t i = 0; i < sizeof payloads / sizeof *payloads; ++i) {
        unsigned char wire[24] = {8, 1, 18, 20, 8, 5, 50, 16, 10, 1, 120, 18, 11, 8, 2, 25};
        memcpy(wire + 16, payloads[i], 8);
        json_t *value = lab_wire_decode(wire, sizeof wire);
        array_t *bytes = value ? lab_wire_encode(value) : NULL;
        bool ok = i < 8 ? bytes && array_size(bytes) == sizeof wire &&
                                  !memcmp(array_get(bytes, 0), wire, sizeof wire) : !value;
        array_destroy(bytes);
        json_free(value);
        union { double number; uint64_t bits; } native = {0};
        for (unsigned byte = 0; byte < 8; ++byte)
            native.bits |= (uint64_t)payloads[i][byte] << (8 * byte);
        number_t number = num_create_from_double(native.number);
        json_t *member = json_new_number_value(number), *root = json_new_object();
        string_t *key = string_new_with("x");
        bool ready = member && root && key && json_object_set(root, key, member);
        bytes = ready ? lab_wire_encode(root) : NULL;
        ok = ok && ready && (i < 8 ? bytes && array_size(bytes) == sizeof wire &&
                                             !memcmp(array_get(bytes, 0), wire, sizeof wire) : !bytes);
        array_destroy(bytes);
        string_free(key);
        json_free(root);
        json_free(member);
        num_destroy(&number);
        TEST_ASSERT_TRUE(ok, "binary64 signed zero, subnormal, normal and finite-limit vectors; reject NaN/infinity");
    }
}

/* Boundary fixtures deliberately exceed the adapter budget using the public codec. */
static bool lab_wire_test_child(protobuf_t *parent, unsigned field, const protobuf_t *child)
{
    array_t *bytes = child ? protobuf_encode(child) : NULL;
    bool ok = bytes && protobuf_add_bytes(parent, field, array_get(bytes, 0), array_size(bytes));
    array_destroy(bytes);
    return ok;
}

static array_t *lab_wire_test_envelope(const protobuf_t *child)
{
    protobuf_t *entry = protobuf_new(4194368, 65540);
    protobuf_t *root = protobuf_new(4194368, 65540);
    protobuf_t *envelope = protobuf_new(4194368, 65540);
    bool ok = entry && root && envelope && protobuf_add_bytes(entry, 1, "x", 1) &&
              lab_wire_test_child(entry, 2, child) && protobuf_add_integer(root, 1, PROTOBUF_VARINT, JSON_OBJECT) &&
              lab_wire_test_child(root, 6, entry) && protobuf_add_integer(envelope, 1, PROTOBUF_VARINT, 1) &&
              lab_wire_test_child(envelope, 2, root);
    array_t *bytes = ok ? protobuf_encode(envelope) : NULL;
    protobuf_free(entry);
    protobuf_free(root);
    protobuf_free(envelope);
    return bytes;
}

static bool lab_wire_test_boundary(const protobuf_t *child, const json_t *native, bool accepted, size_t expected_size)
{
    array_t *wire = lab_wire_test_envelope(child);
    json_t *decoded = wire ? lab_wire_decode(array_get(wire, 0), array_size(wire)) : NULL;
    array_t *encoded = native ? lab_wire_encode(native) : NULL;
    bool ok = wire && native && (decoded != NULL) == accepted && (encoded != NULL) == accepted &&
              (!expected_size || array_size(wire) == expected_size);
    if (ok && accepted)
        ok = array_size(encoded) == array_size(wire) &&
             !memcmp(array_get(encoded, 0), array_get(wire, 0), array_size(wire));
    array_destroy(wire);
    array_destroy(encoded);
    json_free(decoded);
    return ok;
}

static void test_lab_wire_byte_boundary(void)
{
    enum { limit = 4194304, overhead = 29 };
    char *payload = malloc(limit - overhead + 1);
    bool ok = payload != NULL;
    if (payload)
        memset(payload, 'a', limit - overhead + 1);
    string_t *key = string_new_with("x");
    for (size_t extra = 0; ok && extra <= 1; ++extra) {
        size_t size = limit - overhead + extra;
        protobuf_t *child = protobuf_new(limit + 64, 4);
        string_t *text = string_new();
        ok = child && text && key && !string_append_utf8_exact(text, payload, size) &&
             protobuf_add_integer(child, 1, PROTOBUF_VARINT, JSON_STRING) &&
             protobuf_add_bytes(child, 4, payload, size);
        json_t *value = ok ? json_new_string(text) : NULL;
        json_t *root = json_new_object();
        ok = ok && value && root && json_object_set(root, key, value) &&
             lab_wire_test_boundary(child, root, extra == 0, limit + extra);
        json_free(root);
        json_free(value);
        string_free(text);
        protobuf_free(child);
    }
    string_free(key);
    free(payload);
    TEST_ASSERT_TRUE(ok, "exactly 4 MiB accepted; one byte above rejected by encoder and decoder");
}

static void test_lab_wire_node_boundary(void)
{
    static const unsigned char nil[] = {8, 0};
    protobuf_t *child = protobuf_new(4194368, 65540);
    json_t *items = json_new_array(), *null_value = json_new_null();
    string_t *key = string_new_with("x");
    bool ok = child && items && null_value && key && protobuf_add_integer(child, 1, PROTOBUF_VARINT, JSON_ARRAY);
    /* Root and array consume two of the 65536 value nodes. */
    for (size_t i = 0; ok && i < 65535; ++i) {
        ok = protobuf_add_bytes(child, 5, nil, sizeof nil) && json_array_append(items, null_value);
        if (ok && i >= 65533) {
            json_t *root = json_new_object();
            ok = root && json_object_set(root, key, items) && lab_wire_test_boundary(child, root, i == 65533, 0);
            json_free(root);
        }
    }
    protobuf_free(child);
    json_free(items);
    json_free(null_value);
    string_free(key);
    TEST_ASSERT_TRUE(ok, "65536 total value nodes accepted; 65537 rejected by encoder and decoder");
}

static void test_lab_wire_depth_boundary(void)
{
    protobuf_t *child = protobuf_new(0, 0);
    json_t *value = json_new_null();
    string_t *key = string_new_with("x");
    bool ok = child && value && key && protobuf_add_integer(child, 1, PROTOBUF_VARINT, JSON_NULL);
    /* The enclosing root is depth zero; its immediate child starts at one. */
    for (unsigned depth = 1; ok && depth <= 33; ++depth) {
        if (depth >= 32) {
            json_t *root = json_new_object();
            ok = root && json_object_set(root, key, value) && lab_wire_test_boundary(child, root, depth == 32, 0);
            json_free(root);
        }
        if (ok && depth < 33) {
            protobuf_t *parent = protobuf_new(0, 0);
            json_t *items = json_new_array();
            ok = parent && items && protobuf_add_integer(parent, 1, PROTOBUF_VARINT, JSON_ARRAY) &&
                 lab_wire_test_child(parent, 5, child) && json_array_append(items, value);
            protobuf_free(child);
            json_free(value);
            child = parent;
            value = items;
        }
    }
    protobuf_free(child);
    json_free(value);
    string_free(key);
    TEST_ASSERT_TRUE(ok, "depth 32 accepted; depth 33 rejected by encoder and decoder, with root at depth zero");
}

/* Register transport checks before the suite's README examples. */
void test_lab_wire_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_wire_roundtrip, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_wire_rejections, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_wire_schema, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_wire_number_policy, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_wire_number_vectors, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_wire_byte_boundary, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_wire_node_boundary, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_wire_depth_boundary, tests, NULL);
}
