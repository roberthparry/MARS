/**
 * @file test_lab_runtime.c
 * @brief Isolated encrypted cache-key creation and preservation regressions.
 *
 * Synthetic key providers avoid the desktop key store. Tests verify private configuration,
 * stable keys, explicit overrides and rejection of invalid bootstrap JSON without plaintext sidecars.
 */
#include <stdlib.h>
#include <string.h>

#include "file-t.h"
#include "mars_settings.h"
#include "lab_runtime.h"
#include "test_harness.h"
#include "test_lab_support.h"

static string_t *runtime_key(bool create)
{
    (void)create;
    return string_new_with("isolated-runtime-configuration-key");
}

static bool runtime_environment(void)
{
    mars_settings_key_provider(runtime_key);
    return !unsetenv("MARS_CONFIG_FILE") && !unsetenv("MARS_LAB_OBJECT_STORE_KEY") &&
           !unsetenv("MARS_LAB_OBJECT_STORE_PATH");
}

static file_t *runtime_file(const char *directory, const char *name)
{
    string_t *path = string_sprintf("%s/%s", directory, name);
    file_t *file = path ? file_new(path) : NULL;
    string_free(path);
    return file;
}

static bool runtime_creation(const char *directory)
{
    bool ok = runtime_environment() && lab_runtime_prepare();
    const char *environment_key = getenv("MARS_LAB_OBJECT_STORE_KEY");
    string_t *key = environment_key ? string_new_with(environment_key) : NULL;
    string_t *stored = mars_settings_read("MARS_LAB_OBJECT_STORE_KEY");
    file_t *configuration = runtime_file(directory, "mars.db");
    file_info_t *info = configuration ? file_get_info(configuration) : NULL;
    file_t *obsolete = runtime_file(directory, "config/mars-lab.env");
    file_t *lock = runtime_file(directory, "config/mars-lab.env.lock");
    ok = ok && key && stored && string_byte_length(key) == 64 && !string_compare(key, stored) &&
         info && (file_info_permissions(info) & 0777) == 0600 &&
         obsolete && !file_exists(obsolete) && lock && !file_exists(lock);
    ok = ok && !unsetenv("MARS_LAB_OBJECT_STORE_KEY") && lab_runtime_prepare() &&
         getenv("MARS_LAB_OBJECT_STORE_KEY") && !strcmp(getenv("MARS_LAB_OBJECT_STORE_KEY"), string_c_str(key));
    string_free(stored); string_free(key);
    file_info_free(info); file_free(configuration); file_free(obsolete); file_free(lock);
    return ok;
}

static bool runtime_preservation(const char *directory)
{
    bool ok = runtime_environment();
    string_t *settings = string_new_with("{\"MARS_LAB_OBJECT_STORE_KEY\":\"synthetic-fixture-key\"}");
    ok = ok && settings && mars_settings_import(settings, true) && lab_runtime_prepare() &&
         !strcmp(getenv("MARS_LAB_OBJECT_STORE_KEY"), "synthetic-fixture-key");
    ok = ok && !setenv("MARS_LAB_OBJECT_STORE_KEY", "synthetic-environment-key", 1) && lab_runtime_prepare() &&
         !strcmp(getenv("MARS_LAB_OBJECT_STORE_KEY"), "synthetic-environment-key");
    string_t *stored = mars_settings_read("MARS_LAB_OBJECT_STORE_KEY");
    ok = ok && stored && !strcmp(string_c_str(stored), "synthetic-fixture-key");
    file_t *locator = runtime_file(directory, "config/mars.json");
    string_t *broken = string_new_with("{broken json");
    ok = ok && locator && broken && file_write_all_text(locator, broken) &&
         !unsetenv("MARS_LAB_OBJECT_STORE_KEY") && !lab_runtime_prepare();
    string_free(broken); file_free(locator); string_free(stored); string_free(settings);
    return ok;
}

static void test_lab_runtime_creation(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(runtime_creation), "cache secrets persist only in encrypted settings");
}

static void test_lab_runtime_preservation(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(runtime_preservation), "saved keys survive overrides and malformed locators");
}

/* Register runtime checks before documented examples. */
void test_lab_runtime_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_runtime_creation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_runtime_preservation, tests, NULL);
}
