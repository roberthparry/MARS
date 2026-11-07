/**
 * @file test_cfg_storage.c
 * @brief Literal configuration and path regressions for native installers.
 *
 * Round-trips synthetic settings through private files, rejects malformed input,
 * and verifies that shell-looking content is preserved without evaluation.
 * All filesystem changes are confined to the isolated test directory.
 */
#include <stdlib.h>

#include "string-t.h"
#include "json-t.h"
#include "mars_settings.h"
#include "cfg_storage.h"
#include "test_harness.h"
#include "test_cfg_support.h"

static bool test_cfg_storage_round_trip(const char *directory)
{
    (void)directory;
    const char *values[] = {"", "plain", "a'b\"c", "$HOME $(false) literal", "clé café", "cle\xcc\x81 cafe\xcc\x81"};
    bool ok = true;
    for (size_t i = 0; ok && i < sizeof values / sizeof *values; ++i) {
        string_t *value = cfg_storage_value(values[i]), *name = string_new_with("MARS_TEST_LITERAL");
        json_t *object = json_new_object(), *entry = value ? json_new_string(value) : NULL;
        ok = object && name && entry && json_object_set(object, name, entry);
        string_t *text = ok ? json_to_string(object) : NULL;
        ok = ok && text && mars_settings_import(text, true);
        string_t *actual = ok ? mars_settings_read("MARS_TEST_LITERAL") : NULL;
        ok = ok && actual && !string_compare(value, actual);
        if (!ok) fprintf(stderr, "Literal round-trip failed at case %zu (input %zu, result %zu bytes).\n",
                         i, string_byte_length(value), string_byte_length(actual));
        string_free(actual); string_free(text); json_free(entry); json_free(object);
        string_free(name); string_free(value);
    }
    string_t *invalid_utf8 = cfg_storage_value("\xff");
    ok = ok && !invalid_utf8;
    string_free(invalid_utf8);
    return ok;
}

static bool test_cfg_storage_paths(const char *directory)
{
    bool ok = !setenv("HOME", directory, 1) && !setenv("MARS_HOME", "~/chosen", 1);
    string_t *home = cfg_storage_home();
    string_t *expected = string_sprintf("%s/chosen", directory);
    ok = ok && home && expected && !string_compare(home, expected);
    string_t *parent = home ? cfg_storage_parent(home) : NULL;
    ok = ok && parent && string_view_equals_literal(string_view_all(parent), directory);
    string_free(parent);
    string_free(expected);
    string_free(home);
    const char *inputs[] = {"leaf", "/leaf", "/a/b", "a/b"};
    const char *parents[] = {".", "/", "/a", "a"};
    for (size_t i = 0; ok && i < sizeof(inputs) / sizeof(*inputs); ++i) {
        string_t *input = string_new_with(inputs[i]);
        parent = cfg_storage_parent(input);
        ok = parent && string_view_equals_literal(string_view_all(parent), parents[i]);
        string_free(parent);
        string_free(input);
    }
    return ok;
}

static void test_cfg_storage_literals(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_storage_round_trip),
                     "literal settings preserve punctuation and Unicode");
}

static void test_cfg_storage_path_resolution(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_storage_paths),
                     "private root expansion and parent paths are consistent");
}

/* Register storage checks before installation and README examples. */
void test_cfg_storage_cases(void)
{
    TEST_RUN_IN_GROUP(test_cfg_storage_literals, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_storage_path_resolution, tests, NULL);
}
