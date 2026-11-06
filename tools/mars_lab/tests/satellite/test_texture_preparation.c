/**
 * @file test_texture_preparation.c
 * @brief Native process regressions for bounded image-block preparation and published-batch recovery.
 *
 * Uses an isolated source package whose older photograph is deliberately unavailable. Only the selected
 * new image may be prepared. A second case resumes a published batch after its block builder fails,
 * proving that neither the renderer nor the assembler needs to run again. No installed database is opened.
 */
#include <errno.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define stack_t posix_signal_stack_t
#include <sys/wait.h>
#undef stack_t

#include "array-t.h"
#include "file-t.h"
#include "image-t.h"
#include "json-t.h"
#include "string-t.h"
#include "test_harness.h"

extern char **environ;
TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

typedef struct _preparation_fixture_t {
    file_t *directory;
    string_t *digest;
} preparation_fixture_t;

static file_t *fixture_file(const preparation_fixture_t *fixture, const char *relative)
{
    string_t *path = fixture->directory ? string_sprintf("%s/%s", file_path(fixture->directory), relative) : NULL;
    file_t *file = path ? file_new(path) : NULL;
    string_free(path);
    return file;
}

static bool write_text(const preparation_fixture_t *fixture, const char *relative, const char *text)
{
    file_t *file = fixture_file(fixture, relative);
    string_t *source = string_new_with(text);
    bool ok = file && source && file_write_all_text(file, source);
    string_free(source); file_free(file);
    return ok;
}

static int execute(char *const args[])
{
    pid_t child;
    if (posix_spawn(&child, args[0], NULL, NULL, args, environ)) return -1;
    int status;
    pid_t waited;
    do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
    return waited == child && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static bool fixture_new(preparation_fixture_t *fixture)
{
    string_t *parent_path = string_new_with("build/satellite-preparation");
    file_t *parent = parent_path ? file_new(parent_path) : NULL;
    bool ok = parent && file_create_directory(parent, 0755, true);
    fixture->directory = ok ? file_create_temp_directory(parent_path) : NULL;
    file_t *regional = fixture_file(fixture, "regional");
    ok = fixture->directory && regional && file_create_directory(regional, 0755, false);
    image_t *image = ok ? image_new(1800, 1800) : NULL;
    if (image) memset(image_pixels(image), 255, image_size(image));
    array_t *png = image ? image_png_encode(image) : NULL;
    file_t *photograph = fixture_file(fixture, "regional/01.png");
    ok = png && photograph && file_write_all_bytes(photograph, array_get(png, 0), array_size(png));
    fixture->digest = ok ? file_sha256(photograph) : NULL;
    string_t *manifest = fixture->digest ? string_sprintf(
        "{\"assets\":{\"regional/00.png\":{\"width\":1800,\"height\":1800,"
        "\"sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"},"
        "\"regional/01.png\":{\"width\":1800,\"height\":1800,\"sha256\":\"%s\"}}}",
        string_c_str(fixture->digest)) : NULL;
    ok = ok && manifest && write_text(fixture, "manifest.json", string_c_str(manifest)) &&
         write_text(fixture, "plan.json", "{\"tiles\":[{\"slot\":1}]}") &&
         write_text(fixture, "invalid-plan.json", "{\"tiles\":[{\"slot\":2}]}");
    string_free(manifest); file_free(photograph); array_destroy(png); image_free(image);
    file_free(regional); file_free(parent); string_free(parent_path);
    return ok;
}

static file_t *prepared_file(const preparation_fixture_t *fixture)
{
    string_t *relative = fixture->digest ? string_sprintf("pyramid/%s.db", string_c_str(fixture->digest)) : NULL;
    file_t *file = relative ? fixture_file(fixture, string_c_str(relative)) : NULL;
    string_free(relative);
    return file;
}

static void fixture_free(preparation_fixture_t *fixture)
{
    file_t *prepared = prepared_file(fixture);
    if (prepared && file_exists(prepared)) file_delete(prepared);
    file_free(prepared);
    static const char *const files[] = {"regional/01.png", "manifest.json", "plan.json", "invalid-plan.json",
                                      "publication", "job.json", "status.json", "status.json.lock", "blocks.log"};
    for (unsigned i = 0; i < sizeof files / sizeof *files; ++i) {
        file_t *file = fixture_file(fixture, files[i]);
        if (file && file_exists(file)) file_delete(file);
        file_free(file);
    }
    static const char *const directories[] = {"pyramid", "regional"};
    for (unsigned i = 0; i < sizeof directories / sizeof *directories; ++i) {
        file_t *directory = fixture_file(fixture, directories[i]);
        if (directory && file_exists(directory)) file_remove_directory(directory);
        file_free(directory);
    }
    if (fixture->directory) file_remove_directory(fixture->directory);
    file_free(fixture->directory); string_free(fixture->digest);
}

static void test_plan_prepares_only_selected_images(void)
{
    preparation_fixture_t fixture = {0};
    bool ok = fixture_new(&fixture);
    file_t *plan = fixture_file(&fixture, "plan.json"), *invalid = fixture_file(&fixture, "invalid-plan.json");
    char *const args[] = {TEXTURE_PREPARER, (char *)file_path(fixture.directory), "--plan", (char *)file_path(plan), NULL};
    if (ok) ok = execute(args) == 0;
    file_t *prepared = prepared_file(&fixture);
    file_info_t *info = prepared ? file_get_info(prepared) : NULL;
    int64_t seconds = 0, again_seconds = 0;
    long nanos = 0, again_nanos = 0;
    ok = ok && info && file_info_last_write_time(info, &seconds, &nanos) && execute(args) == 0;
    file_info_free(info);
    info = prepared ? file_get_info(prepared) : NULL;
    ok = ok && info && file_info_last_write_time(info, &again_seconds, &again_nanos) &&
         seconds == again_seconds && nanos == again_nanos;
    char *const bad[] = {TEXTURE_PREPARER, (char *)file_path(fixture.directory), "--plan", (char *)file_path(invalid), NULL};
    ok = ok && execute(bad) != 0;
    file_info_free(info); file_free(prepared); file_free(plan); file_free(invalid); fixture_free(&fixture);
    TEST_ASSERT_TRUE(ok, "a batch prepares only selected images, reuses complete shards and rejects unknown slots");
}

static bool write_job(const preparation_fixture_t *fixture, const char *builder)
{
    const char *root = file_path(fixture->directory);
    string_t *job = string_sprintf(
        "{\"renderer\":\"/unavailable-renderer\",\"assembler\":\"/unavailable-assembler\","
        "\"package\":\"%s\",\"shoreline\":\"unused\",\"staging\":\"%s/prepared\","
        "\"status\":\"%s/status.json\",\"source_manifest_sha256\":\"unused\","
        "\"validation_log\":\"%s/validation.log\",\"notifications\":false,"
        "\"pyramid_builder\":\"%s\",\"block_log\":\"%s/blocks.log\","
        "\"publication_marker\":\"%s/publication\",\"tasks\":[{\"label\":\"Fixture\","
        "\"plan\":\"%s/plan.json\",\"directory\":\"%s\",\"log\":\"%s/render.log\"}]}",
        root, root, root, root, builder, root, root, root, root, root);
    bool ok = job && write_text(fixture, "job.json", string_c_str(job));
    string_free(job);
    return ok;
}

static void test_published_batch_resumes_block_preparation(void)
{
    preparation_fixture_t fixture = {0};
    bool ok = fixture_new(&fixture);
    file_t *manifest = fixture_file(&fixture, "manifest.json"), *job = fixture_file(&fixture, "job.json");
    string_t *digest = manifest ? file_sha256(manifest) : NULL;
    ok = ok && digest && write_text(&fixture, "publication", string_c_str(digest)) &&
         write_job(&fixture, "/unavailable-builder");
    char *const args[] = {SATELLITE_BATCH, (char *)file_path(job), NULL};
    ok = ok && execute(args) != 0 && write_job(&fixture, TEXTURE_PREPARER) && execute(args) == 0;
    file_t *status = fixture_file(&fixture, "status.json"), *prepared = prepared_file(&fixture);
    string_t *source = status ? file_read_all_text(status) : NULL;
    json_t *state = source ? json_from_text(source) : NULL;
    string_t *key = string_new_with("state");
    const string_t *value = key && state ? json_string_value(json_object_get(state, key)) : NULL;
    ok = ok && value && !strcmp(string_c_str(value), "ready") && prepared && file_exists(prepared) && execute(args) == 0;
    string_free(key); json_free(state); string_free(source); string_free(digest);
    file_free(manifest); file_free(job); file_free(status); file_free(prepared); fixture_free(&fixture);
    TEST_ASSERT_TRUE(ok, "published batches recover block preparation without rerendering or republishing photographs");
}

/* Run the offline process checks sequentially without desktop notifications or remote data. */
int tests_main(void)
{
    TEST_RUN_IN_GROUP(test_plan_prepares_only_selected_images, tests, NULL);
    TEST_RUN_IN_GROUP(test_published_batch_resumes_block_preparation, tests, NULL);
    return TEST_EXIT_CODE();
}
