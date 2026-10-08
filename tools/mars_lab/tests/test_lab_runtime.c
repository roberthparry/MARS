/**
 * @file test_lab_runtime.c
 * @brief Isolated native Lab runtime key creation and preservation regressions.
 *
 * Exercises first-run secret generation, stable reloads, legacy configuration,
 * environment precedence and conflicting-key rejection under a private MARS_HOME.
 * Keys and configuration contents are compared only in memory and never printed.
 * All filesystem work uses file.h and the suite's bounded private-tree cleanup.
 */
#include <stdlib.h>

#include "file.h"
#include "lab_runtime.h"
#include "test_harness.h"
#include "test_lab_support.h"

static bool clear_runtime_environment(void)
{
    return !unsetenv("MARS_LAB_OBJECT_STORE_KEY") && !unsetenv("MARS_LAB_OBJECT_STORE_PATH") &&
           !unsetenv("MARS_LAB_CACHE_FILE");
}

static file_t *runtime_file(const char *directory, const char *name)
{
    string_t *path = string_sprintf("%s/%s", directory, name);
    file_t *file = path ? file_new(path) : NULL;
    string_free(path);
    return file;
}

static bool write_configuration(file_t *file, const char *contents)
{
    string_t *text = string_new_with(contents);
    bool ok = file && text && file_write_all_text(file, text) && file_chmod(file, 0600);
    string_free(text);
    return ok;
}

static bool runtime_creation(const char *directory)
{
    bool prepared = clear_runtime_environment() && lab_runtime_prepare();
    const char *environment_key = getenv("MARS_LAB_OBJECT_STORE_KEY");
    string_t *key = environment_key ? string_new_with(environment_key) : NULL;
    file_t *configuration = runtime_file(directory, "config/mars-lab.env");
    file_t *folder = runtime_file(directory, "config");
    file_t *cache = runtime_file(directory, "lab/mars_lab_object_store.sqlite3");
    string_t *before = configuration ? file_read_all_text(configuration) : NULL;
    file_info_t *info = configuration ? file_get_info(configuration) : NULL;
    file_info_t *folder_info = folder ? file_get_info(folder) : NULL;
    const char *cache_path = getenv("MARS_LAB_OBJECT_STORE_PATH");
    string_t *actual_path = cache_path ? string_new_with(cache_path) : NULL;
    bool ok = prepared && key && string_byte_length(key) == 64 && before && info && folder_info &&
              (file_info_permissions(info) & 0777) == 0600 && (file_info_permissions(folder_info) & 0777) == 0700 &&
              actual_path && cache && string_view_equals_literal(string_view_all(actual_path), file_path(cache)) &&
              !file_exists(cache) && !file_last_error(cache);
    bool reloaded = !unsetenv("MARS_LAB_OBJECT_STORE_KEY") && lab_runtime_prepare();
    environment_key = getenv("MARS_LAB_OBJECT_STORE_KEY");
    string_t *after = configuration ? file_read_all_text(configuration) : NULL;
    ok = ok && reloaded && environment_key && string_view_equals_literal(string_view_all(key), environment_key) &&
         before && after && string_compare(before, after) == 0;
    string_free(after);
    string_free(before);
    string_free(key);
    string_free(actual_path);
    file_info_free(info);
    file_info_free(folder_info);
    file_free(configuration);
    file_free(folder);
    file_free(cache);
    return ok;
}

static bool runtime_preservation(const char *directory)
{
    file_t *folder = runtime_file(directory, "config");
    file_t *configuration = runtime_file(directory, "config/mars-lab.env");
    static const char legacy[] = "# fixture comment\nexport UNRELATED_SETTING='preserve me'\n"
                                 "export MARS_LAB_OBJECT_STORE_KEY='synthetic-fixture-key'\n";
    bool prepared = clear_runtime_environment() && folder && file_create_directory(folder, 0700, false) &&
                    write_configuration(configuration, legacy) && lab_runtime_prepare();
    const char *key = getenv("MARS_LAB_OBJECT_STORE_KEY");
    string_t *key_copy = key ? string_new_with(key) : NULL;
    string_t *contents = configuration ? file_read_all_text(configuration) : NULL;
    bool ok = prepared && key_copy && string_view_equals_literal(string_view_all(key_copy), "synthetic-fixture-key") &&
              contents && string_view_equals_literal(string_view_all(contents), legacy);
    string_free(key_copy);
    string_free(contents);
    bool overridden = !setenv("MARS_LAB_OBJECT_STORE_KEY", "synthetic-environment-key", 1) && lab_runtime_prepare();
    key = getenv("MARS_LAB_OBJECT_STORE_KEY");
    key_copy = key ? string_new_with(key) : NULL;
    contents = configuration ? file_read_all_text(configuration) : NULL;
    ok = ok && overridden && key_copy &&
         string_view_equals_literal(string_view_all(key_copy), "synthetic-environment-key") && contents &&
         string_view_equals_literal(string_view_all(contents), legacy);
    string_free(key_copy);
    string_free(contents);
    static const char conflict[] = "export MARS_LAB_OBJECT_STORE_KEY='synthetic-one'\n"
                                   "export MARS_LAB_OBJECT_STORE_KEY='synthetic-two'\n";
    bool rejected = write_configuration(configuration, conflict) && !unsetenv("MARS_LAB_OBJECT_STORE_KEY") &&
                    !lab_runtime_prepare();
    contents = configuration ? file_read_all_text(configuration) : NULL;
    ok = ok && rejected && contents && string_view_equals_literal(string_view_all(contents), conflict);
    string_free(contents);
    file_free(configuration);
    file_free(folder);
    return ok;
}

static void test_lab_runtime_creation(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(runtime_creation), "new runtime keys are private, persistent and never rotated");
}

static void test_lab_runtime_preservation(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(runtime_preservation),
                     "existing keys survive overrides and conflicting configuration is preserved on rejection");
}

/* Register runtime cases without including secret values in assertion output. */
void test_lab_runtime_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_runtime_creation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_runtime_preservation, tests, NULL);
}
