/**
 * @file test_protobuf.c
 * @brief Protocol Buffers codec regression suite.
 *
 * Checks supported wire round trips, malformed inputs and size or field-count boundaries. The complete README
 * round-trip example runs after ordinary codec assertions.
 *
 * Used by the project test harness for regression verification. Select cases through tests/test_config.json and
 * run suites sequentially; this source is not part of the installed library.
 */

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "protobuf-t.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

#define main protobuf_example_main
#include "../../scratch/protobuf_roundtrip.c"
#undef main

static void test_protobuf_bounded_primitives(void)
{
    const uint64_t values[] = {0, 1, 127, 128, 16383, 16384, UINT32_MAX, UINT64_MAX};
    unsigned char bytes[12];
    for (size_t i = 0; i < sizeof values / sizeof *values; ++i) {
        memset(bytes, 0xa5, sizeof bytes);
        size_t width = protobuf_wire_varint_size(values[i]), offset = 1;
        TEST_ASSERT_TRUE(protobuf_wire_write_varint(bytes, width + 1, &offset, values[i]), "exact varint capacity");
        TEST_ASSERT_TRUE(offset == width + 1 && bytes[0] == 0xa5 && bytes[offset] == 0xa5, "write canaries");
        for (size_t end = 1; end < offset; ++end) {
            size_t cursor = 1;
            uint64_t value = 42;
            TEST_ASSERT_TRUE(!protobuf_wire_read_varint(bytes, end, &cursor, &value) && cursor == 1 && value == 42,
                             "truncated read is transactional");
        }
        size_t cursor = 1;
        uint64_t value = 0;
        TEST_ASSERT_TRUE(protobuf_wire_read_varint(bytes, offset, &cursor, &value) &&
                             cursor == offset && value == values[i], "full-width varint round trip");
        memset(bytes, 0xa5, sizeof bytes);
        offset = 1;
        TEST_ASSERT_TRUE(!protobuf_wire_write_varint(bytes, width, &offset, values[i]) && offset == 1,
                         "short varint output rejected");
        for (size_t j = 0; j < sizeof bytes; ++j)
            TEST_ASSERT_INT_EQ(bytes[j], 0xa5);
    }
    const protobuf_wire_t wires[] = {PROTOBUF_FIXED32, PROTOBUF_FIXED64};
    for (size_t i = 0; i < 2; ++i) {
        size_t offset = 1, width = i ? 8 : 4;
        uint64_t value = i ? UINT64_C(0x0123456789abcdef) : UINT64_C(0x89abcdef);
        memset(bytes, 0xa5, sizeof bytes);
        TEST_ASSERT_TRUE(!protobuf_wire_write_fixed(bytes, width, &offset, wires[i], value) && offset == 1 &&
                             bytes[1] == 0xa5, "short fixed output rejected before writing");
        TEST_ASSERT_TRUE(protobuf_wire_write_fixed(bytes, width + 1, &offset, wires[i], value), "fixed output");
        TEST_ASSERT_TRUE(bytes[1] == 0xef && bytes[width] == (i ? 0x01 : 0x89) && bytes[offset] == 0xa5,
                         "little-endian unaligned output and canary");
        bytes[0] = (1 << 3) | wires[i];
        protobuf_wire_field_t field = {0};
        size_t cursor = 0;
        TEST_ASSERT_TRUE(protobuf_wire_read_field(bytes, offset, &cursor, &field) && cursor == offset &&
                             field.integer == value && field.wire == wires[i] && field.field == 1,
                         "fixed field decoder uses the shared wire core");
    }
    size_t offset = 0;
    uint64_t value = 42;
    TEST_ASSERT_TRUE(!protobuf_wire_read_varint(NULL, 0, &offset, &value) && value == 42, "empty input");
    TEST_ASSERT_TRUE(!protobuf_wire_write_varint(NULL, 0, &offset, 0), "empty output");
    TEST_ASSERT_TRUE(!protobuf_wire_write_fixed(bytes, sizeof bytes, &offset, PROTOBUF_FIXED32, UINT64_MAX),
                     "fixed32 overflow");
    TEST_ASSERT_TRUE(!protobuf_wire_write_fixed(bytes, sizeof bytes, &offset, PROTOBUF_BYTES, 0), "wrong wire type");
    offset = SIZE_MAX;
    TEST_ASSERT_TRUE(!protobuf_wire_write_varint(bytes, sizeof bytes, &offset, 1) &&
                         !protobuf_wire_read_varint(bytes, sizeof bytes, &offset, &value), "invalid cursor");
    TEST_ASSERT_TRUE(!protobuf_wire_write_varint(bytes, sizeof bytes, NULL, 1) &&
                         !protobuf_wire_read_varint(bytes, sizeof bytes, NULL, &value), "missing cursor");

    const unsigned char payload[] = {10, 3, 0, 255, 42, 18, 0};
    protobuf_wire_field_t field = {0};
    offset = 0;
    TEST_ASSERT_TRUE(protobuf_wire_read_field(payload, sizeof payload, &offset, &field) && offset == 5 &&
                         field.wire == PROTOBUF_BYTES && field.data == payload + 2 && field.size == 3,
                     "byte fields borrow the caller's exact span without allocation");
    TEST_ASSERT_TRUE(protobuf_wire_read_field(payload, sizeof payload, &offset, &field) &&
                         offset == sizeof payload && field.field == 2 && field.size == 0 &&
                         field.data == payload + sizeof payload, "empty field may borrow the end pointer");
    TEST_ASSERT_TRUE(!protobuf_wire_read_field(payload, sizeof payload, &offset, &field) &&
                         offset == sizeof payload && field.field == 2, "end of input preserves the last field");
    offset = 0;
    TEST_ASSERT_TRUE(!protobuf_wire_read_field(payload, 4, &offset, &field) && offset == 0 && field.field == 2,
                     "a containing-message bound cannot be crossed");
    TEST_ASSERT_TRUE(!protobuf_wire_read_field(payload, sizeof payload, &offset, NULL) && offset == 0,
                     "missing field output rejected");
    TEST_ASSERT_TRUE(!protobuf_wire_read_varint(payload, sizeof payload, &offset, NULL) && offset == 0,
                     "missing integer output rejected");
}

static void test_protobuf_wire_roundtrip(void)
{
    protobuf_t *message = protobuf_new(1024, 16);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_TRUE(protobuf_add_integer(message, 1, PROTOBUF_VARINT, 150), "varint");
    TEST_ASSERT_TRUE(protobuf_add_integer(message, 2, PROTOBUF_FIXED32, 0x12345678), "fixed32");
    TEST_ASSERT_TRUE(protobuf_add_integer(message, 3, PROTOBUF_FIXED64, UINT64_MAX), "fixed64");
    const unsigned char bytes[] = {0, 255, 42};
    TEST_ASSERT_TRUE(protobuf_add_bytes(message, 1, bytes, sizeof(bytes)), "duplicate field number");
    TEST_ASSERT_TRUE(protobuf_add_bytes(message, 536870911, NULL, 0), "empty field at maximum number");
    array_t *encoded = protobuf_encode(message);
    TEST_ASSERT_NOT_NULL(encoded);
    protobuf_t *decoded = protobuf_decode(array_get(encoded, 0), array_size(encoded), 1024, 16);
    TEST_ASSERT_NOT_NULL(decoded);
    TEST_ASSERT_INT_EQ(protobuf_count(decoded), 5);
    uint64_t number = 0;
    TEST_ASSERT_TRUE(protobuf_integer(decoded, 0, &number) && number == 150, "varint roundtrip");
    TEST_ASSERT_TRUE(protobuf_integer(decoded, 1, &number) && number == 0x12345678, "fixed32 endian order");
    TEST_ASSERT_TRUE(protobuf_integer(decoded, 2, &number) && number == UINT64_MAX, "fixed64 endian order");
    size_t size;
    const void *value = protobuf_bytes(decoded, 3, &size);
    TEST_ASSERT_TRUE(size == 3 && !memcmp(value, bytes, 3), "binary field");
    TEST_ASSERT_INT_EQ(protobuf_field(decoded, 3), 1);
    TEST_ASSERT_INT_EQ(protobuf_wire(decoded, 4), PROTOBUF_BYTES);
    TEST_ASSERT_TRUE(!protobuf_bytes(decoded, 4, &size) && size == 0, "empty byte field");
    array_t *again = protobuf_encode(decoded);
    TEST_ASSERT_TRUE(array_size(again) == array_size(encoded) &&
                         !memcmp(array_get(again, 0), array_get(encoded, 0), array_size(encoded)),
                     "stable normal encoding");
    array_destroy(again);
    array_destroy(encoded);
    protobuf_free(decoded);
    protobuf_free(message);
}

static void test_protobuf_rejects_malformed(void)
{
    static const unsigned char bad[][12] = {{0},
                                            {8, 128},
                                            {8, 255, 255, 255, 255, 255, 255, 255, 255, 255, 2},
                                            {10, 5, 1},
                                            {13, 1},
                                            {9, 1},
                                            {11},
                                            {12},
                                            {14},
                                            {15},
                                            {255, 255, 255, 255, 31, 0}};
    static const size_t sizes[] = {1, 2, 11, 3, 2, 2, 1, 1, 1, 1, 6};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        size_t cursor = 0;
        protobuf_wire_field_t field = {.field = 123, .integer = 42};
        TEST_ASSERT_TRUE(!protobuf_wire_read_field(bad[i], sizes[i], &cursor, &field) &&
                             cursor == 0 && field.field == 123 && field.integer == 42,
                         "malformed field leaves borrowed output and cursor unchanged");
        protobuf_t *message = protobuf_decode(bad[i], sizes[i], 1024, 16);
        bool rejected = message == NULL;
        protobuf_free(message);
        TEST_ASSERT_TRUE(rejected, "malformed wire message rejected");
    }
    const unsigned char nonminimal[] = {8, 128, 0};
    protobuf_t *message = protobuf_decode(nonminimal, sizeof(nonminimal), 3, 1);
    TEST_ASSERT_NOT_NULL(message);
    array_t *encoded = protobuf_encode(message);
    TEST_ASSERT_INT_EQ(array_size(encoded), 2);
    protobuf_free(message);
    array_destroy(encoded);
}

static void test_protobuf_limits_and_boundaries(void)
{
    protobuf_t *message = protobuf_new(2, 1);
    TEST_ASSERT_TRUE(protobuf_add_integer(message, 1, PROTOBUF_VARINT, 1), "exact budget");
    TEST_ASSERT_TRUE(!protobuf_add_integer(message, 2, PROTOBUF_VARINT, 0), "field count limit");
    TEST_ASSERT_TRUE(!protobuf_add_integer(message, 0, PROTOBUF_VARINT, 1), "zero field forbidden");
    TEST_ASSERT_TRUE(!protobuf_add_integer(message, 1, PROTOBUF_FIXED32, UINT64_MAX), "fixed32 overflow");
    TEST_ASSERT_TRUE(!protobuf_add_integer(message, 1, PROTOBUF_BYTES, 1), "wrong numeric wire type");
    TEST_ASSERT_TRUE(!protobuf_add_bytes(message, 1, NULL, 1), "NULL data rejected");
    TEST_ASSERT_INT_EQ(protobuf_count(message), 1);
    uint64_t unchanged = 42;
    TEST_ASSERT_TRUE(!protobuf_integer(message, 5, &unchanged) && unchanged == 42, "unchanged invalid output");
    TEST_ASSERT_INT_EQ(protobuf_wire(message, 5), -1);
    TEST_ASSERT_INT_EQ(protobuf_field(message, 5), 0);
    protobuf_free(message);
    const unsigned char body[] = {8, 1, 8, 2};
    TEST_ASSERT_TRUE(!protobuf_decode(body, sizeof(body), 3, 2), "input-byte limit");
    TEST_ASSERT_TRUE(!protobuf_decode(body, sizeof(body), 4, 1), "decoded field limit");
    TEST_ASSERT_TRUE(!protobuf_new(67108865, 1) && !protobuf_new(1, 1048577), "maximum budgets");
    message = protobuf_decode(NULL, 0, 0, 0);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_INT_EQ(protobuf_count(message), 0);
    array_t *empty = protobuf_encode(message);
    TEST_ASSERT_NOT_NULL(empty);
    TEST_ASSERT_INT_EQ(array_size(empty), 0);
    array_destroy(empty);
    protobuf_free(message);
    const int64_t values[] = {0, 1, -1, INT64_MIN, INT64_MAX, -123456};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
        TEST_ASSERT_TRUE(protobuf_zigzag_decode(protobuf_zigzag_encode(values[i])) == values[i], "ZigZag boundary");
    TEST_ASSERT_TRUE(protobuf_zigzag_encode(INT64_MIN) == UINT64_MAX, "minimum signed value");
}

/* README example runs the documented complete program after all ordinary tests. */
static void example_protobuf_roundtrip(void)
{
    TEST_ASSERT_INT_EQ(protobuf_example_main(), EXIT_SUCCESS);
}

/* README example: docs/protobuf.md, allocation-free wire operations. */
static void example_protobuf_wire_buffer(void)
{
    unsigned char bytes[3];
    size_t written = 0;
    bool ok = protobuf_wire_write_varint(bytes, sizeof bytes, &written, 8) &&
              protobuf_wire_write_varint(bytes, sizeof bytes, &written, 150);
    size_t offset = 0;
    protobuf_wire_field_t field;
    ok = ok && protobuf_wire_read_field(bytes, written, &offset, &field);
    TEST_ASSERT_TRUE(ok && offset == written && field.field == 1 && field.integer == 150, "borrowed wire example");
    if (ok)
        printf("Field %u: %llu (%zu bytes)\n", (unsigned)field.field, (unsigned long long)field.integer, written);
}

int tests_main(void)
{
    TEST_SECTION("Protocol Buffers");
    TEST_RUN_IN_GROUP(test_protobuf_bounded_primitives, tests, NULL);
    TEST_RUN_IN_GROUP(test_protobuf_wire_roundtrip, tests, NULL);
    TEST_RUN_IN_GROUP(test_protobuf_rejects_malformed, tests, NULL);
    TEST_RUN_IN_GROUP(test_protobuf_limits_and_boundaries, tests, NULL);
    TEST_SECTION("README Output Examples");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_protobuf_roundtrip, readme_examples, "protobuf,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_protobuf_wire_buffer, readme_examples, "protobuf,readme,output");
    return TEST_EXIT_CODE();
}
