/**
 * @file test_cfg_storage.c
 * @brief Literal configuration and path regressions for native installers.
 *
 * Round-trips synthetic settings through private files, rejects malformed input,
 * and verifies that shell-looking content is preserved without evaluation.
 * All filesystem changes are confined to the isolated test directory.
 */
#include <stdlib.h>

#include "cfg_storage.h"
#include "test_cfg_support.h"
#include "test_harness.h"
#include "ustring.h"

static bool test_cfg_storage_round_trip(const char *directory)
{
    string_t *root = string_new_with(directory);
    string_t *path = string_sprintf("%s/settings.env", directory);
    file_t *file = path ? file_new(path) : NULL;
    const char *values[] = {
        "", "plain", "a'b\"c", "$HOME $(false) `false` \\ literal", "clé café", "cle\xcc\x81 cafe\xcc\x81"};
    bool ok = root && file;
    for (size_t i = 0; ok && i < sizeof(values) / sizeof(*values); ++i) {
        string_t *value = cfg_storage_value(values[i]);
        string_t *quoted = value ? cfg_storage_quote(value) : NULL;
        string_t *body = quoted ? string_new_with("# fixture\nexport SETTING=") : NULL;
        ok = body && !string_append_utf8_exact(body, string_c_str(quoted), string_byte_length(quoted)) &&
             !string_append_char(body, '\n') && cfg_storage_publish(file, root, body);
        string_t *actual = ok ? cfg_storage_setting(file, "SETTING") : NULL;
        string_t *absent = ok ? cfg_storage_setting(file, "ABSENT") : NULL;
        ok = ok && actual && absent && !string_byte_length(absent) && !string_compare(value, actual);
        string_free(absent);
        string_free(actual);
        string_free(body);
        string_free(quoted);
        string_free(value);
    }
    const char *malformed[] = {"SETTING='unterminated\n", "SETTING=two words\n", "SETTING='one'\nSETTING='two'\n",
                               "SETTING=trail\\\n"};
    for (size_t i = 0; ok && i < sizeof(malformed) / sizeof(*malformed); ++i) {
        string_t *body = string_new_with(malformed[i]);
        ok = body && cfg_storage_publish(file, root, body);
        string_t *actual = ok ? cfg_storage_setting(file, "SETTING") : NULL;
        ok = ok && !actual;
        string_free(actual);
        string_free(body);
    }
    string_t *bad = string_new_with("line\nbreak");
    string_t *quoted = cfg_storage_quote(bad);
    string_t *invalid_utf8 = cfg_storage_value("\xff");
    ok = ok && !quoted && !invalid_utf8;
    string_free(invalid_utf8);
    string_free(quoted);
    string_free(bad);
    file_free(file);
    string_free(path);
    string_free(root);
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
