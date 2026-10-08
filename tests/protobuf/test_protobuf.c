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

#include "protobuf.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

#define main protobuf_example_main
#include "../../scratch/protobuf_roundtrip.c"
#undef main

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

int tests_main(void)
{
    TEST_SECTION("Protocol Buffers");
    TEST_RUN_IN_GROUP(test_protobuf_wire_roundtrip, tests, NULL);
    TEST_RUN_IN_GROUP(test_protobuf_rejects_malformed, tests, NULL);
    TEST_RUN_IN_GROUP(test_protobuf_limits_and_boundaries, tests, NULL);
    TEST_SECTION("README Output Examples");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_protobuf_roundtrip, readme_examples, "protobuf,readme,output");
    return TEST_EXIT_CODE();
}
