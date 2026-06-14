#include <stdlib.h>

#include "json.h"
#include "number.h"
#include "test_harness.h"
#include "ustring.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static string_t *s(const char *text)
{
    return string_new_with(text);
}

static const json_t *object_get_literal(const json_t *object, const char *key_text)
{
    string_t *key = s(key_text);
    const json_t *value = json_object_get(object, key);

    string_free(key);
    return value;
}

static bool json_number_equals(const json_t *json, number_t expected)
{
    number_t actual = NUM_NAN;
    bool ok = json_number_value(json, &actual);
    bool equal = ok && (num_is_nan(expected)
        ? num_is_nan(actual)
        : num_eq(actual, expected));

    if (ok)
        num_destroy(&actual);
    return equal;
}

static void test_parse_object_array_and_escapes(void)
{
    string_t *text = s("{"
                       "\"name\":\"mars\","
                       "\"array\":[true,false,null,123,-4.5e+6],"
                       "\"emoji\":\"\\uD83D\\uDE42\","
                       "\"nul\":\"\\u0000\","
                       "\"quote\":\"\\\"\\\\\\/\\b\\f\\n\\r\\t\""
                       "}");
    json_t *json = json_from_text(text);
    const json_t *name;
    const json_t *array;
    const json_t *emoji;
    const json_t *nul;
    const json_t *number;
    bool b = false;

    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_TRUE(json_type(json) == JSON_OBJECT, "root is an object");

    name = object_get_literal(json, "name");
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_STR_EQ(string_c_str(json_string_value(name)), "mars");

    array = object_get_literal(json, "array");
    TEST_ASSERT_NOT_NULL(array);
    TEST_ASSERT_TRUE(json_type(array) == JSON_ARRAY, "array member is an array");
    TEST_ASSERT_INT_EQ((int)json_array_size(array), 5);
    TEST_ASSERT_TRUE(json_bool_value(json_array_get(array, 0), &b) && b,
                     "true value parses");
    TEST_ASSERT_TRUE(json_bool_value(json_array_get(array, 1), &b) && !b,
                     "false value parses");
    TEST_ASSERT_TRUE(json_type(json_array_get(array, 2)) == JSON_NULL,
                     "null value parses");

    number = json_array_get(array, 4);
    TEST_ASSERT_NOT_NULL(number);
    TEST_ASSERT_STR_EQ(string_c_str(json_number_text(number)), "-4.5e+6");
    {
        number_t numeric = NUM_NAN;
        number_t expected = num_create_from_long(-4500000);
        bool parsed = json_number_value(number, &numeric);
        bool equal = parsed && num_eq(numeric, expected);

        num_destroy(&numeric);
        num_destroy(&expected);
        TEST_ASSERT_TRUE(equal, "JSON number converts to number_t");
    }

    emoji = object_get_literal(json, "emoji");
    TEST_ASSERT_NOT_NULL(emoji);
    TEST_ASSERT_STR_EQ(string_c_str(json_string_value(emoji)), "🙂");

    nul = object_get_literal(json, "nul");
    TEST_ASSERT_NOT_NULL(nul);
    TEST_ASSERT_TRUE(string_length(json_string_value(nul)) == 1u,
                     "escaped NUL is stored as real string content");
    TEST_ASSERT_TRUE(rune_value(string_at(json_string_value(nul), 0u)) == 0u,
                     "escaped NUL round-trips as a zero-valued rune");

    json_free(json);
    string_free(text);
}

static void test_rejects_non_json_number_forms(void)
{
    string_t *leading_zero = s("{\"bad\":01}");
    string_t *missing_fraction = s("{\"bad\":1.}");
    string_t *missing_exponent = s("{\"bad\":1e}");

    TEST_ASSERT_NULL(json_from_text(leading_zero));
    TEST_ASSERT_NULL(json_from_text(missing_fraction));
    TEST_ASSERT_NULL(json_from_text(missing_exponent));

    string_free(missing_exponent);
    string_free(missing_fraction);
    string_free(leading_zero);
}

static void test_number_t_extension_round_trip(void)
{
    number_t rational = num_create_from_frac(2, 3);
    number_t complex_value = num_create_from_string("3+4i");
    string_t *complex_text = num_to_string(complex_value);
    json_t *array = json_new_array();
    json_t *rational_json = json_new_number_value(rational);
    json_t *complex_json = json_new_number_value(complex_value);
    const string_t *rational_spelling;
    string_t *serialised;
    json_t *round_trip;

    TEST_ASSERT_NOT_NULL(complex_text);
    string_free(complex_text);

    TEST_ASSERT_NOT_NULL(array);
    TEST_ASSERT_NOT_NULL(rational_json);
    TEST_ASSERT_NOT_NULL(complex_json);
    rational_spelling = json_number_text(rational_json);
    TEST_ASSERT_NOT_NULL(rational_spelling);
    TEST_ASSERT_TRUE(json_array_append(array, rational_json),
                     "append rational number");
    TEST_ASSERT_TRUE(json_array_append(array, complex_json),
                     "append complex number");

    serialised = json_to_string(array);
    TEST_ASSERT_NOT_NULL(serialised);
    TEST_ASSERT_TRUE(string_find(serialised, "$mars.number") >= 0,
                     "extended number envelope is used");
    TEST_ASSERT_TRUE(string_find_string(serialised, rational_spelling) >= 0,
                     "rational spelling is preserved");

    round_trip = json_from_text(serialised);
    TEST_ASSERT_NOT_NULL(round_trip);
    TEST_ASSERT_TRUE(json_type(json_array_get(round_trip, 0)) == JSON_NUMBER,
                     "rational round-trips as a JSON number");
    TEST_ASSERT_TRUE(json_number_equals(json_array_get(round_trip, 0), rational),
                     "rational number_t value round-trips");
    TEST_ASSERT_TRUE(json_type(json_array_get(round_trip, 1)) == JSON_NUMBER,
                     "complex round-trips as a JSON number");
    TEST_ASSERT_TRUE(json_number_equals(json_array_get(round_trip, 1), complex_value),
                     "complex number_t value round-trips");

    json_free(round_trip);
    string_free(serialised);
    json_free(complex_json);
    json_free(rational_json);
    json_free(array);
    num_destroy(&complex_value);
    num_destroy(&rational);
}

static void test_number_t_constants_use_math_spelling(void)
{
    string_t *fallback_text = s("{\"$mars.number\":\"@pi\"}");
    json_t *pi = json_new_number_value(NUM_PI);
    json_t *fallback;
    string_t *serialised;
    string_t *fallback_serialised;
    json_t *round_trip;

    TEST_ASSERT_NOT_NULL(pi);
    serialised = json_to_string(pi);
    TEST_ASSERT_NOT_NULL(serialised);
    TEST_ASSERT_TRUE(string_find(serialised, "π") >= 0,
                     "pi serialises using the Greek symbol");
    TEST_ASSERT_TRUE(string_find(serialised, "NUM_PI") < 0,
                     "pi does not leak the C identifier");

    round_trip = json_from_text(serialised);
    TEST_ASSERT_NOT_NULL(round_trip);
    TEST_ASSERT_TRUE(json_type(round_trip) == JSON_NUMBER,
                     "pi extension parses as a JSON number");
    TEST_ASSERT_TRUE(json_number_equals(round_trip, NUM_PI),
                     "pi number_t value round-trips");

    fallback = json_from_text(fallback_text);
    TEST_ASSERT_NOT_NULL(fallback);
    TEST_ASSERT_TRUE(json_number_equals(fallback, NUM_PI),
                     "@pi fallback parses as pi");
    fallback_serialised = json_to_string(fallback);
    TEST_ASSERT_NOT_NULL(fallback_serialised);
    TEST_ASSERT_TRUE(string_find(fallback_serialised, "π") >= 0,
                     "@pi canonicalises back to π");
    TEST_ASSERT_TRUE(string_find(fallback_serialised, "NUM_PI") < 0,
                     "@pi fallback does not produce NUM_PI");

    string_free(fallback_serialised);
    json_free(fallback);
    json_free(round_trip);
    string_free(serialised);
    json_free(pi);
    string_free(fallback_text);
}

static void test_serialise_round_trip(void)
{
    json_t *object = json_new_object();
    json_t *array = json_new_array();
    string_t *name = s("name");
    string_t *items = s("items");
    string_t *value = s("MARS \"Lab\"");
    json_t *true_value = json_new_bool(true);
    json_t *null_value = json_new_null();
    json_t *text_value = json_new_string(value);
    string_t *serialised;
    json_t *round_trip;

    TEST_ASSERT_NOT_NULL(object);
    TEST_ASSERT_NOT_NULL(array);
    TEST_ASSERT_NOT_NULL(true_value);
    TEST_ASSERT_NOT_NULL(null_value);
    TEST_ASSERT_NOT_NULL(text_value);
    TEST_ASSERT_TRUE(json_array_append(array, true_value), "append true");
    TEST_ASSERT_TRUE(json_array_append(array, null_value), "append null");
    TEST_ASSERT_TRUE(json_object_set(object, name, text_value), "set string");
    TEST_ASSERT_TRUE(json_object_set(object, items, array),
                     "set array");

    serialised = json_to_string_pretty(object, 2);
    TEST_ASSERT_NOT_NULL(serialised);
    round_trip = json_from_text(serialised);
    TEST_ASSERT_NOT_NULL(round_trip);
    TEST_ASSERT_TRUE(json_type(object_get_literal(round_trip, "items")) == JSON_ARRAY,
                     "round-tripped array exists");

    json_free(round_trip);
    string_free(serialised);
    string_free(value);
    string_free(items);
    string_free(name);
    json_free(text_value);
    json_free(null_value);
    json_free(true_value);
    json_free(array);
    json_free(object);
}

static void test_file_round_trip(void)
{
    const char *path_text = test_case_temp_path("json-roundtrip.json");
    string_t *path = s(path_text);
    string_t *key = s("enabled");
    json_t *object = json_new_object();
    json_t *enabled_value = json_new_bool(true);
    json_t *loaded;
    bool enabled = false;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_NOT_NULL(object);
    TEST_ASSERT_NOT_NULL(enabled_value);
    TEST_ASSERT_TRUE(json_object_set(object, key, enabled_value), "set enabled");
    TEST_ASSERT_INT_EQ(json_to_file_pretty(object, path, 2), 0);

    loaded = json_from_file(path);
    TEST_ASSERT_NOT_NULL(loaded);
    TEST_ASSERT_TRUE(json_bool_value(object_get_literal(loaded, "enabled"), &enabled) &&
                     enabled,
                     "file round trip preserves boolean");

    json_free(loaded);
    json_free(enabled_value);
    json_free(object);
    string_free(key);
    string_free(path);
}

static void example_json_parse_and_inspect(void)
{
    string_t *text = s("{\"name\":\"mars\",\"enabled\":true,\"items\":[1,2,3]}");
    string_t *name_key = s("name");
    string_t *enabled_key = s("enabled");
    string_t *items_key = s("items");
    json_t *root = json_from_text(text);
    const json_t *name = json_object_get(root, name_key);
    const json_t *enabled_json = json_object_get(root, enabled_key);
    const json_t *items = json_object_get(root, items_key);
    bool enabled = false;

    json_bool_value(enabled_json, &enabled);

    string_printf("name=%S\n", json_string_value(name));
    string_printf("enabled=%s\n", enabled ? "true" : "false");
    string_printf("items=%zu\n", json_array_size(items));

    json_free(root);
    string_free(items_key);
    string_free(enabled_key);
    string_free(name_key);
    string_free(text);
}

static void example_json_build_and_serialise(void)
{
    json_t *items = json_new_array();
    string_t *name = s("MARS");
    string_t *two = s("2");
    json_t *name_value = json_new_string(name);
    json_t *true_value = json_new_bool(true);
    json_t *two_value = json_new_number(two);
    string_t *out;

    json_array_append(items, name_value);
    json_array_append(items, true_value);
    json_array_append(items, two_value);

    out = json_to_string_pretty(items, 2);
    string_printf("%S", out);

    string_free(out);
    json_free(two_value);
    json_free(true_value);
    json_free(name_value);
    string_free(two);
    string_free(name);
    json_free(items);
}

static void example_json_number_fidelity(void)
{
    number_t rational = num_create_from_frac(2, 3);
    json_t *json = json_new_number_value(rational);
    string_t *text = json_to_string(json);

    string_printf("%S\n", text);

    string_free(text);
    json_free(json);
    num_destroy(&rational);
}

static void example_json_file_round_trip(void)
{
    const char *path_text = test_case_temp_path("json-readme-settings.json");
    string_t *path = s(path_text);
    string_t *key = s("enabled");
    json_t *root = json_new_object();
    json_t *enabled = json_new_bool(true);
    json_t *loaded;

    json_object_set(root, key, enabled);
    json_to_file_pretty(root, path, 2);

    loaded = json_from_file(path);
    string_printf("type=%d\n", (int)json_type(loaded));

    json_free(loaded);
    json_free(enabled);
    json_free(root);
    string_free(key);
    string_free(path);
}

int tests_main(void)
{
    TEST_SECTION("JSON Parsing");
    TEST_RUN_IN_GROUP(test_parse_object_array_and_escapes, tests, NULL);
    TEST_RUN_IN_GROUP(test_rejects_non_json_number_forms, tests, NULL);
    TEST_RUN_IN_GROUP(test_number_t_extension_round_trip, tests, NULL);
    TEST_RUN_IN_GROUP(test_number_t_constants_use_math_spelling, tests, NULL);

    TEST_SECTION("JSON Serialisation");
    TEST_RUN_IN_GROUP(test_serialise_round_trip, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_round_trip, tests, NULL);

    TEST_SECTION("README Output Examples");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_json_parse_and_inspect,
                                  readme_examples,
                                  "json,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_json_build_and_serialise,
                                  readme_examples,
                                  "json,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_json_number_fidelity,
                                  readme_examples,
                                  "json,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_json_file_round_trip,
                                  readme_examples,
                                  "json,readme,output");

    return TEST_EXIT_CODE();
}
