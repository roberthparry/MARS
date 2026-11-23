/**
 * @file checks_checkpoint.c
 * @brief Local, content-verified progress for sequential verification commands.
 *
 * The checker fingerprints its invocation and test data once, then reads or commits an indexed checkpoint at each
 * independent suite/example boundary. Storage connections close before execution. A failed lookup runs the test;
 * a failed save only loses an optimisation. Successful reuse never enters timing statistics or pretends to be a run.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <sodium.h>

#include "file-t.h"
#include "test_database.h"
#include "checks_checkpoint.h"

extern char **environ;

struct _checks_checkpoint_t {
    string_t *root;
    string_t *domain;
    string_t *scope;
    checks_strings_t *arguments;
    checks_strings_t *files;
    crypto_hash_sha256_state hash;
    bool resume;
};

static void checks_checkpoint_hash(crypto_hash_sha256_state *hash, const string_t *value)
{
    size_t size = value ? string_byte_length(value) : 0;
    crypto_hash_sha256_update(hash, (const unsigned char *)&size, sizeof size);
    if (size) crypto_hash_sha256_update(hash, (const unsigned char *)string_c_str(value), size);
}

static bool checks_checkpoint_file(crypto_hash_sha256_state *hash, const string_t *path)
{
    file_t *file = file_new(path);
    file_t *resolved = file ? file_resolve(file) : NULL;
    string_t *digest = resolved ? file_sha256(resolved) : NULL;
    checks_checkpoint_hash(hash, path);
    checks_checkpoint_hash(hash, digest);
    bool ok = digest != NULL;
    string_free(digest);
    file_free(resolved);
    file_free(file);
    return ok;
}

static json_t *checks_checkpoint_selection(const json_t *source)
{
    json_t *result = json_new_object();
    if (!result) checks_fatal("normalising test selections");
    for (size_t i = 0; i < json_object_size(source); ++i) {
        const string_t *key = json_object_key_at(source, i);
        const json_t *value = json_object_value_at(source, i);
        bool enabled;
        /* Missing choices default to true; discovering an explicitly enabled case changes no selection. */
        if (json_bool_value(value, &enabled) && enabled) continue;
        json_t *child = json_type(value) == JSON_OBJECT ? checks_checkpoint_selection(value) : NULL;
        bool retain = !child || json_object_size(child);
        if (retain && !json_object_set(result, key, child ? child : value))
            checks_fatal("retaining normalised test selections");
        json_free(child);
    }
    return result;
}

static void checks_checkpoint_environment(crypto_hash_sha256_state *hash)
{
    static const char *const names[] = {
        "PATH", "LD_LIBRARY_PATH", "LD_PRELOAD", "LANG", "LC_ALL", "LC_NUMERIC", "TZ",
        "ASAN_OPTIONS", "LSAN_OPTIONS", "MSAN_OPTIONS", "UBSAN_OPTIONS", "TSAN_OPTIONS", "OMP_NUM_THREADS"
    };
    checks_strings_t *values = checks_strings_new();
    for (size_t i = 0; i < sizeof names / sizeof *names; ++i) {
        const char *value = getenv(names[i]);
        checks_strings_add(values, string_sprintf("%s=%s", names[i], value ? value : ""));
    }
    for (size_t i = 0; environ && environ[i]; ++i)
        if (!strncmp(environ[i], "MARS_", 5)) checks_strings_add(values, checks_text(environ[i]));
    checks_strings_sort(values);
    for (size_t i = 0; i < checks_strings_count(values); ++i)
        checks_checkpoint_hash(hash, checks_strings_get(values, i));
    checks_strings_free(values);
}

/* Establish a local invocation identity without retaining a management connection during tests. */
checks_checkpoint_t *checks_checkpoint_new(const string_t *root, const char *scope,
                                          const checks_strings_t *arguments, const checks_strings_t *files, bool resume)
{
    string_t *schema = checks_path(root, "tests/test_database.sql");
    bool available = checks_is_file(schema);
    string_free(schema);
    if (!available) return NULL;
    checks_checkpoint_t *checkpoint = calloc(1, sizeof *checkpoint);
    if (!checkpoint) return NULL;
    checkpoint->root = string_clone(root);
    checkpoint->domain = checks_text(scope);
    checkpoint->arguments = checks_strings_new();
    checkpoint->files = checks_strings_new();
    for (size_t i = 0; i < checks_strings_count(arguments); ++i)
        checks_strings_add(checkpoint->arguments, string_clone(checks_strings_get(arguments, i)));
    for (size_t i = 0; i < checks_strings_count(files); ++i)
        checks_strings_add(checkpoint->files, string_clone(checks_strings_get(files, i)));
    struct utsname machine;
    bool ok = checkpoint->root && !uname(&machine);
    checkpoint->scope = ok ? string_sprintf("%s:%s:%s", machine.nodename, string_c_str(root), scope) : NULL;
    checkpoint->resume = resume;
    crypto_hash_sha256_init(&checkpoint->hash);
    string_t *version = checks_text("MARS verification checkpoint v1");
    checks_checkpoint_hash(&checkpoint->hash, version);
    string_free(version);
    string_t *error = NULL;
    test_database_t *database = ok ? test_database_open(root, NULL, true, &error) : NULL;
    string_t *inputs = database ? test_database_inputs_fingerprint(database, root) : NULL;
    string_t *self = checks_text("/proc/self/exe");
    ok = ok && checkpoint->scope && inputs && checks_checkpoint_file(&checkpoint->hash, self);
    string_free(self);
    checks_checkpoint_hash(&checkpoint->hash, inputs);
    checks_checkpoint_hash(&checkpoint->hash, checkpoint->scope);
    test_database_close(database);
    string_free(inputs);
    string_free(error);
    for (size_t i = 0; ok && i < checks_strings_count(arguments); ++i)
        checks_checkpoint_hash(&checkpoint->hash, checks_strings_get(arguments, i));
    for (size_t i = 0; ok && i < checks_strings_count(files); ++i)
        ok = checks_checkpoint_file(&checkpoint->hash, checks_strings_get(files, i));
    static const char *const libraries[] = {"build/release/libmars.so", "build/debug/libmars.so"};
    for (size_t i = 0; ok && i < sizeof libraries / sizeof *libraries; ++i) {
        string_t *path = checks_path(root, libraries[i]);
        if (checks_is_file(path)) ok = checks_checkpoint_file(&checkpoint->hash, path);
        string_free(path);
    }
    checks_strings_t *selections = ok ? checks_files(root, "tests", ".json", true) : NULL;
    for (size_t i = 0; ok && i < checks_strings_count(selections); ++i) {
        string_t *path = checks_path(root, string_c_str(checks_strings_get(selections, i)));
        json_t *selection = json_from_file(path);
        bool catalogue = checks_equal(checks_strings_get(selections, i), "tests/test_config.json");
        json_t *effective = catalogue && json_type(selection) == JSON_OBJECT ?
            checks_checkpoint_selection(selection) : NULL;
        string_t *canonical = selection ? json_to_string(effective ? effective : selection) : NULL;
        if (canonical) {
            checks_checkpoint_hash(&checkpoint->hash, path);
            checks_checkpoint_hash(&checkpoint->hash, canonical);
        } else ok = checks_checkpoint_file(&checkpoint->hash, path);
        string_free(canonical);
        json_free(effective);
        json_free(selection);
        string_free(path);
    }
    checks_strings_free(selections);
    checks_checkpoint_environment(&checkpoint->hash);
    if (!ok) {
        if (resume) fputs("Test checkpoints unavailable; running fresh.\n", stderr);
        checks_checkpoint_free(checkpoint);
        return NULL;
    }
    return checkpoint;
}

static string_t *checks_checkpoint_digest(const checks_checkpoint_t *checkpoint, const string_t *content)
{
    crypto_hash_sha256_state hash = checkpoint->hash;
    checks_checkpoint_hash(&hash, content);
    unsigned char digest[crypto_hash_sha256_BYTES];
    char hexadecimal[crypto_hash_sha256_BYTES * 2u + 1u];
    crypto_hash_sha256_final(&hash, digest);
    sodium_bin2hex(hexadecimal, sizeof hexadecimal, digest, sizeof digest);
    return checks_text(hexadecimal);
}

/* A cache miss is invalidated durably before another attempt can fail or be interrupted. */
bool checks_checkpoint_reuse(checks_checkpoint_t *checkpoint, const char *id, const string_t *content, size_t *passed)
{
    if (passed) *passed = 0;
    if (!checkpoint) return false;
    string_t *scope = string_sprintf("%s:%s", string_c_str(checkpoint->scope), id);
    string_t *digest = checks_checkpoint_digest(checkpoint, content);
    test_database_t *database = test_database_open(checkpoint->root, NULL, false, NULL);
    bool matched = false;
    bool ok = database && scope && digest && test_database_checkpoint_read(database, string_c_str(scope),
                                                                            string_c_str(digest), &matched, passed);
    bool reuse = ok && checkpoint->resume && matched;
    if (!reuse && database && scope)
        ok = test_database_checkpoint_write(database, string_c_str(scope), NULL, NULL) && ok;
    if (!ok) fputs("Cannot read/update test checkpoint; running fresh.\n", stderr);
    test_database_close(database);
    string_free(digest);
    string_free(scope);
    return reuse;
}

/* Preserve success separately from real timing reports. */
void checks_checkpoint_pass(checks_checkpoint_t *checkpoint, const char *id, const string_t *content,
                            const size_t *passed)
{
    if (!checkpoint) return;
    string_t *scope = string_sprintf("%s:%s", string_c_str(checkpoint->scope), id);
    string_t *digest = checks_checkpoint_digest(checkpoint, content);
    checks_checkpoint_t *after = checks_checkpoint_new(checkpoint->root, string_c_str(checkpoint->domain),
                                                      checkpoint->arguments, checkpoint->files, false);
    string_t *current = after ? checks_checkpoint_digest(after, content) : NULL;
    bool unchanged = digest && current && !string_compare(digest, current);
    string_free(current);
    checks_checkpoint_free(after);
    if (!unchanged) {
        fputs("Verification inputs changed during execution; checkpoint not saved.\n", stderr);
        string_free(digest);
        string_free(scope);
        return;
    }
    test_database_t *database = test_database_open(checkpoint->root, NULL, false, NULL);
    bool ok = database && scope && digest && test_database_checkpoint_write(database, string_c_str(scope),
                                                                           string_c_str(digest), passed);
    if (!ok) fputs("Cannot save test checkpoint; the next invocation will run fresh.\n", stderr);
    test_database_close(database);
    string_free(digest);
    string_free(scope);
}

/* Release the immutable invocation snapshot. */
void checks_checkpoint_free(checks_checkpoint_t *checkpoint)
{
    if (!checkpoint) return;
    string_free(checkpoint->scope);
    string_free(checkpoint->domain);
    string_free(checkpoint->root);
    checks_strings_free(checkpoint->files);
    checks_strings_free(checkpoint->arguments);
    free(checkpoint);
}
