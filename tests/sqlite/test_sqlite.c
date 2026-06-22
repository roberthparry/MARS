#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "sqlite.h"
#include "test_harness.h"
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

    sqlite_free_blob(loaded);
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

int tests_main(void)
{
    TEST_SECTION("SQLCipher Storage");
    TEST_RUN_IN_GROUP(test_sqlite_string_round_trip_is_encrypted, tests, NULL);
    TEST_RUN_IN_GROUP(test_sqlite_rejects_wrong_key, tests, NULL);
    TEST_RUN_IN_GROUP(test_sqlite_object_blob_round_trip, tests, NULL);
    TEST_RUN_IN_GROUP(test_sqlite_requires_key, tests, NULL);

    return TEST_EXIT_CODE();
}
