#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "array.h"
#include "datetime.h"
#include "expression.h"
#include "sqlite.h"
#include "test_harness.h"
#include "timeseries.h"
#include "ustring.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static string_t *s(const char *text)
{
    return string_new_with(text);
}

static int file_contains(const char *path, const char *needle)
{
    FILE *f;
    char buffer[4096];
    size_t needle_len = strlen(needle);
    size_t carry = 0u;

    if (!path || !needle || needle_len == 0u)
        return 0;

    f = fopen(path, "rb");
    if (!f)
        return 0;

    while (!feof(f)) {
        size_t n = fread(buffer + carry, 1u, sizeof(buffer) - carry, f);
        size_t total = carry + n;

        if (total >= needle_len) {
            for (size_t i = 0u; i + needle_len <= total; i++) {
                if (memcmp(buffer + i, needle, needle_len) == 0) {
                    fclose(f);
                    return 1;
                }
            }
            carry = needle_len - 1u;
            memmove(buffer, buffer + total - carry, carry);
        } else {
            carry = total;
        }
    }

    fclose(f);
    return 0;
}

static void test_sqlite_string_round_trip_is_encrypted(void)
{
    const char *path_text = test_case_temp_path("mars-secure-string.db");
    string_t *path = s(path_text);
    string_t *key = s("correct horse battery staple");
    string_t *name = s("secret-note");
    string_t *value = s("classified: π and Mars dust");
    string_t *loaded = NULL;
    sqlite_t *db;

    db = sqlite_open_encrypted(path, key);
    TEST_ASSERT_NOT_NULL(db);
    TEST_ASSERT_TRUE(sqlite_init_object_store(db), "object store initialises");
    TEST_ASSERT_TRUE(sqlite_store_string(db, name, value), "string stores");
    sqlite_close(db);

    TEST_ASSERT_TRUE(!file_contains(path_text, "SQLite format 3"),
                     "file does not expose a plain SQLite header");
    TEST_ASSERT_TRUE(!file_contains(path_text, "classified"),
                     "file does not expose plaintext content");

    db = sqlite_open_encrypted(path, key);
    TEST_ASSERT_NOT_NULL(db);
    TEST_ASSERT_TRUE(sqlite_load_string(db, name, &loaded), "string loads");
    TEST_ASSERT_STR_EQ(string_c_str(loaded), string_c_str(value));

    string_free(loaded);
    sqlite_close(db);
    string_free(value);
    string_free(name);
    string_free(key);
    string_free(path);
}

static void test_sqlite_rejects_wrong_key(void)
{
    const char *path_text = test_case_temp_path("mars-secure-wrong-key.db");
    string_t *path = s(path_text);
    string_t *key = s("right key");
    string_t *wrong_key = s("wrong key");
    string_t *name = s("answer");
    string_t *value = s("42");
    sqlite_t *db;

    db = sqlite_open_encrypted(path, key);
    TEST_ASSERT_NOT_NULL(db);
    TEST_ASSERT_TRUE(sqlite_init_object_store(db), "object store initialises");
    TEST_ASSERT_TRUE(sqlite_store_string(db, name, value), "string stores");
    sqlite_close(db);

    db = sqlite_open_encrypted(path, wrong_key);
    TEST_ASSERT_NULL(db);

    string_free(value);
    string_free(name);
    string_free(wrong_key);
    string_free(key);
    string_free(path);
}

static void test_sqlite_object_blob_round_trip(void)
{
    const char *path_text = test_case_temp_path("mars-secure-blob.db");
    const uint8_t payload[] = { 0x00u, 0x01u, 0x02u, 0xffu, 'M', 'A', 'R', 'S' };
    string_t *path = s(path_text);
    string_t *key = s("blob key");
    string_t *name = s("raw-object");
    string_t *type = s("test/blob");
    string_t *encoding = s("raw");
    string_t *loaded_type = NULL;
    string_t *loaded_encoding = NULL;
    void *loaded = NULL;
    size_t loaded_len = 0u;
    sqlite_t *db = sqlite_open_encrypted(path, key);

    TEST_ASSERT_NOT_NULL(db);
    TEST_ASSERT_TRUE(sqlite_init_object_store(db), "object store initialises");
    TEST_ASSERT_TRUE(sqlite_store_object(db,
                                        name,
                                        type,
                                        encoding,
                                        payload,
                                        sizeof(payload)),
                     "blob stores");
    TEST_ASSERT_TRUE(sqlite_load_object(db,
                                       name,
                                       &loaded_type,
                                       &loaded_encoding,
                                       &loaded,
                                       &loaded_len),
                     "blob loads");
    ASSERT_EQ_INT((int)loaded_len, (int)sizeof(payload));
    TEST_ASSERT_TRUE(memcmp(loaded, payload, sizeof(payload)) == 0,
                     "blob payload round-trips exactly");
    TEST_ASSERT_STR_EQ(string_c_str(loaded_type), "test/blob");
    TEST_ASSERT_STR_EQ(string_c_str(loaded_encoding), "raw");

    sqlite_free_object_data(loaded);
    string_free(loaded_encoding);
    string_free(loaded_type);
    sqlite_close(db);
    string_free(encoding);
    string_free(type);
    string_free(name);
    string_free(key);
    string_free(path);
}

static void test_sqlite_requires_key(void)
{
    string_t *path = s(test_case_temp_path("mars-no-key.db"));
    string_t *empty_key = s("");
    sqlite_t *db = sqlite_open_encrypted(path, empty_key);

    TEST_ASSERT_NULL(db);

    string_free(empty_key);
    string_free(path);
}

static void example_sqlite_readme_prepared_statement_query(void)
{
    string_t *path = s(test_case_temp_path("mars-readme-query.db"));
    string_t *key = s("correct horse battery staple");
    sqlite_t *db = sqlite_open_encrypted(path, key);
    sqlite_stmt_t *stmt = NULL;

    ASSERT_NOT_NULL(path);
    ASSERT_NOT_NULL(key);
    ASSERT_NOT_NULL(db);

    TEST_ASSERT_TRUE(sqlite_exec_cstr(db,
                                      "create table if not exists city ("
                                      "  name text not null,"
                                      "  population integer not null"
                                      ");"),
                     "city table creates");
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "delete from city;"),
                     "city table clears");
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db,
                                      "insert into city(name, population) values "
                                      "('Liverpool', 486100),"
                                      "('Leeds', 536280),"
                                      "('Manchester', 395500),"
                                      "('Rhyl', 25743);"),
                     "city rows insert");

    stmt = sqlite_stmt_prepare(db,
                               "select name, population from city "
                               "where population >= ?1 "
                               "order by population desc;");
    ASSERT_NOT_NULL(stmt);
    TEST_ASSERT_TRUE(sqlite_stmt_bind_int(stmt, 1, 350000),
                     "population threshold binds");

    while (sqlite_stmt_step(stmt) == SQLITE_STEP_ROW) {
        printf("%s: %d\n",
               sqlite_stmt_column_text(stmt, 0),
               sqlite_stmt_column_int(stmt, 1));
    }

    sqlite_stmt_finalize(stmt);
    sqlite_close(db);
    string_free(key);
    string_free(path);
}

static void example_sqlite_readme_encrypted_string_storage(void)
{
    string_t *path = s(test_case_temp_path("mars-readme-string.db"));
    string_t *key = s("correct horse battery staple");
    string_t *name = s("note");
    string_t *value = s("机密：Mars dust 🚀🔴");
    string_t *loaded = NULL;
    sqlite_t *db = sqlite_open_encrypted(path, key);

    ASSERT_NOT_NULL(path);
    ASSERT_NOT_NULL(key);
    ASSERT_NOT_NULL(name);
    ASSERT_NOT_NULL(value);
    ASSERT_NOT_NULL(db);

    TEST_ASSERT_TRUE(sqlite_init_object_store(db), "object store initialises");
    TEST_ASSERT_TRUE(sqlite_store_string(db, name, value), "string stores");
    TEST_ASSERT_TRUE(sqlite_load_string(db, name, &loaded), "string loads");
    ASSERT_NOT_NULL(loaded);

    string_printf("%S\n", loaded);

    string_free(loaded);
    sqlite_close(db);
    string_free(value);
    string_free(name);
    string_free(key);
    string_free(path);
}

static void example_sqlite_readme_array_object_storage(void)
{
    string_t *path = s(test_case_temp_path("mars-readme-array.db"));
    string_t *key = s("correct horse battery staple");
    string_t *name = s("fibonacci");
    string_t *type = NULL;
    string_t *encoding = NULL;
    string_t *loaded_type = NULL;
    string_t *loaded_encoding = NULL;
    sqlite_t *db = sqlite_open_encrypted(path, key);
    array_t *values = array_create(sizeof(int32_t), NULL, NULL);
    array_t *loaded_values = NULL;
    void *payload = NULL;
    void *loaded_payload = NULL;
    size_t payload_len = 0u;
    size_t loaded_payload_len = 0u;
    int32_t nums[] = {1, 1, 2, 3, 5, 8};
    size_t i;

    ASSERT_NOT_NULL(path);
    ASSERT_NOT_NULL(key);
    ASSERT_NOT_NULL(name);
    ASSERT_NOT_NULL(db);
    ASSERT_NOT_NULL(values);

    for (i = 0u; i < sizeof(nums) / sizeof(nums[0]); ++i)
        ASSERT_TRUE(array_add(values, &nums[i]));

    ASSERT_TRUE(array_serialize(values, &type, &encoding, &payload, &payload_len));
    TEST_ASSERT_TRUE(sqlite_init_object_store(db), "object store initialises");
    TEST_ASSERT_TRUE(sqlite_store_object(db, name, type, encoding, payload, payload_len),
                     "array payload stores");
    TEST_ASSERT_TRUE(sqlite_load_object(db,
                                        name,
                                        &loaded_type,
                                        &loaded_encoding,
                                        &loaded_payload,
                                        &loaded_payload_len),
                     "array payload loads");

    loaded_values = array_deserialise(loaded_payload,
                                      loaded_payload_len,
                                      loaded_type,
                                      loaded_encoding);
    ASSERT_NOT_NULL(loaded_values);

    for (i = 0u; i < array_size(loaded_values); ++i)
        printf("%d%s",
               *(int32_t *)array_get(loaded_values, i),
               (i + 1u < array_size(loaded_values)) ? " " : "\n");

    array_destroy(loaded_values);
    sqlite_free_object_data(loaded_payload);
    free(payload);
    string_free(loaded_encoding);
    string_free(loaded_type);
    array_destroy(values);
    sqlite_close(db);
    string_free(encoding);
    string_free(type);
    string_free(name);
    string_free(key);
    string_free(path);
}

static void example_sqlite_readme_timeseries_object_storage(void)
{
    string_t *path = s(test_case_temp_path("mars-readme-timeseries.db"));
    string_t *key = s("correct horse battery staple");
    string_t *name = s("weekly-signal");
    string_t *type = NULL;
    string_t *encoding = NULL;
    string_t *loaded_type = NULL;
    string_t *loaded_encoding = NULL;
    sqlite_t *db = sqlite_open_encrypted(path, key);
    datetime_t *start = datetime_from_string("2026-01-01");
    const double values[] = {12.5, 13.0, 15.25};
    timeseries_t *series = NULL;
    timeseries_t *loaded_series = NULL;
    string_t *loaded_text = NULL;
    void *payload = NULL;
    void *loaded_payload = NULL;
    size_t payload_len = 0u;
    size_t loaded_payload_len = 0u;

    ASSERT_NOT_NULL(path);
    ASSERT_NOT_NULL(key);
    ASSERT_NOT_NULL(name);
    ASSERT_NOT_NULL(db);
    ASSERT_NOT_NULL(start);

    series = ts_new_regular_from_doubles(values,
                                         sizeof(values) / sizeof(values[0]),
                                         start,
                                         TS_FREQ_DAILY,
                                         TS_YEAR_CALENDAR);
    ASSERT_NOT_NULL(series);

    ASSERT_TRUE(ts_serialize(series, &type, &encoding, &payload, &payload_len));

    TEST_ASSERT_TRUE(sqlite_init_object_store(db), "object store initialises");
    TEST_ASSERT_TRUE(sqlite_store_object(db,
                                         name,
                                         type,
                                         encoding,
                                         payload,
                                         payload_len),
                     "timeseries text stores");
    TEST_ASSERT_TRUE(sqlite_load_object(db,
                                        name,
                                        &loaded_type,
                                        &loaded_encoding,
                                        &loaded_payload,
                                        &loaded_payload_len),
                     "timeseries text loads");

    loaded_series = ts_deserialise(loaded_payload,
                                   loaded_payload_len,
                                   loaded_type,
                                   loaded_encoding);
    ASSERT_NOT_NULL(loaded_series);
    loaded_text = ts_to_text(loaded_series, TS_STRING_CSV);
    ASSERT_NOT_NULL(loaded_text);

    string_printf("%S", loaded_text);

    string_free(loaded_text);
    ts_free(loaded_series);
    sqlite_free_object_data(loaded_payload);
    free(payload);
    string_free(loaded_encoding);
    string_free(loaded_type);
    ts_free(series);
    datetime_dealloc(start);
    sqlite_close(db);
    string_free(encoding);
    string_free(type);
    string_free(name);
    string_free(key);
    string_free(path);
}

static void example_sqlite_readme_expression_object_storage(void)
{
    string_t *path = s(test_case_temp_path("mars-readme-expression.db"));
    string_t *key = s("correct horse battery staple");
    string_t *name = s("trajectory");
    string_t *type = NULL;
    string_t *encoding = NULL;
    string_t *loaded_type = NULL;
    string_t *loaded_encoding = NULL;
    string_t *source = s("{ exp(@pi*sqrt(H_9)) | ; H_9 = 163 }");
    sqlite_t *db = sqlite_open_encrypted(path, key);
    expr_t *expr = NULL;
    expr_t *loaded_expr = NULL;
    string_t *loaded_roundtrip = NULL;
    string_t *value_text = NULL;
    number_t value;
    void *payload = NULL;
    size_t payload_len = 0u;
    void *loaded_payload = NULL;
    size_t loaded_payload_len = 0u;

    ASSERT_NOT_NULL(path);
    ASSERT_NOT_NULL(key);
    ASSERT_NOT_NULL(name);
    ASSERT_NOT_NULL(source);
    ASSERT_NOT_NULL(db);

    expr = expr_from_text(source, NULL);
    ASSERT_NOT_NULL(expr);
    ASSERT_TRUE(expr_serialize(expr, &type, &encoding, &payload, &payload_len));

    TEST_ASSERT_TRUE(sqlite_init_object_store(db), "object store initialises");
    TEST_ASSERT_TRUE(sqlite_store_object(db,
                                         name,
                                         type,
                                         encoding,
                                         payload,
                                         payload_len),
                     "expression text stores");
    TEST_ASSERT_TRUE(sqlite_load_object(db,
                                        name,
                                        &loaded_type,
                                        &loaded_encoding,
                                        &loaded_payload,
                                        &loaded_payload_len),
                     "expression text loads");

    loaded_expr = expr_deserialise(loaded_payload,
                                   loaded_payload_len,
                                   loaded_type,
                                   loaded_encoding);
    ASSERT_NOT_NULL(loaded_expr);
    loaded_roundtrip = expr_to_text(loaded_expr, style_EXPRESSION);
    ASSERT_NOT_NULL(loaded_roundtrip);
    value = expr_eval(loaded_expr);
    value_text = num_to_string(value);
    ASSERT_NOT_NULL(value_text);

    string_printf("%S\n", loaded_roundtrip);
    string_printf("%S\n", value_text);

    string_free(value_text);
    num_destroy(&value);
    string_free(loaded_roundtrip);
    expr_free(loaded_expr);
    sqlite_free_object_data(loaded_payload);
    free(payload);
    string_free(loaded_encoding);
    string_free(loaded_type);
    expr_free(expr);
    sqlite_close(db);
    string_free(source);
    string_free(encoding);
    string_free(type);
    string_free(name);
    string_free(key);
    string_free(path);
}

int tests_main(void)
{
    TEST_SECTION("SQLCipher Storage");
    TEST_RUN_IN_GROUP(test_sqlite_string_round_trip_is_encrypted, tests, NULL);
    TEST_RUN_IN_GROUP(test_sqlite_rejects_wrong_key, tests, NULL);
    TEST_RUN_IN_GROUP(test_sqlite_object_blob_round_trip, tests, NULL);
    TEST_RUN_IN_GROUP(test_sqlite_requires_key, tests, NULL);

    TEST_SECTION("README Output Examples");
    printf(C_BOLD C_YELLOW "Running README examples...\n" C_RESET);
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_sqlite_readme_encrypted_string_storage,
                                  readme_examples,
                                  "sqlite,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_sqlite_readme_array_object_storage,
                                  readme_examples,
                                  "sqlite,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_sqlite_readme_timeseries_object_storage,
                                  readme_examples,
                                  "sqlite,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_sqlite_readme_expression_object_storage,
                                  readme_examples,
                                  "sqlite,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_sqlite_readme_prepared_statement_query,
                                  readme_examples,
                                  "sqlite,readme,output");

    return TEST_EXIT_CODE();
}
