/**
 * @file texture_pyramid.c
 * @brief Offline preparation of independently addressed PNG blocks at every photographic resolution.
 *
 * Reads the packaged manifest and creates one immutable SQLite shard per source content digest. The image module owns
 * PNG decoding, alpha-weighted resizing and padded blocks; globe publishes each complete source atomically.
 * Interrupted runs resume from completed shards. Optional installation streams each completed source into a
 * caller-selected plaintext globe; original photographs remain available throughout preparation.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "array-t.h"
#include "file-t.h"
#include "globe-t.h"
#include "image-t.h"
#include "json-t.h"
#include "sqlite-t.h"
#include "string-t.h"
#define MARS_TEXTURE_PYRAMID_INTERNAL_ACCESS
#include "texture_pyramid.h"

typedef struct {
    image_t *image;
    unsigned blocks;
} pyramid_pixels_t;

static const json_t *field(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = key ? json_object_get(object, key) : NULL;
    string_free(key);
    return value;
}

static array_t *produce(unsigned tier, unsigned row, unsigned column, void *context)
{
    pyramid_pixels_t *state = context;
    if (tier != image_width(state->image)) {
        image_t *smaller = image_resize(state->image, tier, tier);
        if (!smaller) return NULL;
        image_free(state->image);
        state->image = smaller;
    }
    image_t *block = image_block(state->image, 512, 1, row, column);
    array_t *encoded = block ? image_png_encode(block) : NULL;
    image_free(block);
    if (encoded) ++state->blocks;
    return encoded;
}

static bool complete(sqlite_t *db, const char *digest, unsigned edge)
{
    sqlite_stmt_t *q = db ? sqlite_stmt_prepare(db,
        "SELECT 1 FROM globe_texture_block_source WHERE sha256=?1 AND width=?2 AND format_version=1") : NULL;
    bool ok = q && sqlite_stmt_bind_text(q, 1, digest) && sqlite_stmt_bind_int(q, 2, edge) &&
        sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    sqlite_stmt_finalize(q);
    return ok;
}

static bool prepare(const string_t *root, const string_t *relative, const char *digest, unsigned edge,
                    sqlite_t *installed)
{
    if (!digest || strlen(digest) != 64) return false;
    for (unsigned i = 0; i < 64; ++i)
        if (!((digest[i] >= '0' && digest[i] <= '9') || (digest[i] >= 'a' && digest[i] <= 'f'))) return false;
    string_t *path = string_sprintf("%s/pyramid/%s.db", string_c_str(root), digest);
    string_t *temporary = path ? string_sprintf("%s.part", string_c_str(path)) : NULL;
    file_t *output = path ? file_new(path) : NULL, *part = temporary ? file_new(temporary) : NULL;
    bool exists = output && file_exists(output), ok = output && part;
    sqlite_t *db = ok ? sqlite_open_plain(exists ? path : temporary) : NULL;
    bool ready = complete(db, digest, edge);
    if (ok && !ready && !exists) {
        string_t *source_path = string_sprintf("%s/%s", string_c_str(root), string_c_str(relative));
        file_t *source = source_path ? file_new(source_path) : NULL;
        string_t *actual = source ? file_sha256(source) : NULL;
        array_t *encoded = actual && !strcmp(string_c_str(actual), digest) ? file_read_all_bytes(source) : NULL;
        pyramid_pixels_t pixels = {.image = encoded ? image_png_decode(array_get(encoded, 0), array_size(encoded)) : NULL};
        array_destroy(encoded);
        ok = pixels.image && image_width(pixels.image) == edge && image_height(pixels.image) == edge &&
            sqlite_exec_cstr(db, "PRAGMA journal_mode=DELETE; PRAGMA synchronous=FULL; PRAGMA cache_size=-2048;") &&
            globe_texture_pyramid_build(db, digest, edge, produce, &pixels);
        image_free(pixels.image);
        file_free(source); string_free(actual); string_free(source_path);
        ready = ok;
    }
    sqlite_close(db);
    ok = ok && ready && (exists || file_move(part, output, false));
    if (ok && installed && !complete(installed, digest, edge)) {
        bool transaction = sqlite_exec_cstr(installed, "BEGIN IMMEDIATE");
        ok = transaction && globe_texture_pyramid_import(installed, output);
        if (ok) ok = sqlite_exec_cstr(installed, "COMMIT");
        if (!ok && transaction) sqlite_exec_cstr(installed, "ROLLBACK");
        if (!ok) fputs("Prepared source retained; installation could not obtain or complete its writer transaction.\n", stderr);
    }
    file_free(output); file_free(part); string_free(path); string_free(temporary);
    return ok;
}

int main(int argc, char **argv)
{
    bool notifications = argc > 2 && !strcmp(argv[argc - 1], "--notify");
    if (notifications) --argc;
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s texture-directory [relative-image|all] [plaintext-globe.db] [--notify]\n", argv[0]);
        return EXIT_FAILURE;
    }
    string_t *root = string_new_with(argv[1]), *manifest_path = string_sprintf("%s/manifest.json", argv[1]);
    string_t *folder_path = string_sprintf("%s/pyramid", argv[1]);
    file_t *manifest_file = file_new(manifest_path), *folder = file_new(folder_path);
    file_info_t *info = file_get_info(manifest_file);
    string_t *text = info && file_info_size(info) <= GLOBE_TEXTURE_MANIFEST_LIMIT ?
        file_read_all_text(manifest_file) : NULL;
    json_t *manifest = text ? json_from_text(text) : NULL;
    const json_t *assets = field(manifest, "assets");
    string_t *installed_path = argc == 4 ? string_new_with(argv[3]) : NULL;
    file_t *installed_file = installed_path ? file_new(installed_path) : NULL;
    sqlite_t *installed = installed_file && file_exists(installed_file) ? sqlite_open_plain(installed_path) : NULL;
    bool ok = root && assets && folder && file_create_directory(folder, 0755, true) && (argc < 4 || installed);
    if (installed) {
        ok = ok && sqlite_exec_cstr(installed, "PRAGMA busy_timeout=30000; PRAGMA cache_size=-8192;");
        sqlite_stmt_t *catalogue = ok ? sqlite_stmt_prepare(installed,
            "SELECT json FROM globe_texture_manifest WHERE id=1") : NULL;
        ok = catalogue && sqlite_stmt_step(catalogue) == SQLITE_STEP_ROW &&
            sqlite_stmt_column_bytes(catalogue, 0) == string_byte_length(text) &&
            !memcmp(sqlite_stmt_column_blob(catalogue, 0), string_c_str(text), string_byte_length(text));
        sqlite_stmt_finalize(catalogue);
        if (!ok) fputs("Installed photograph catalogue differs; update the globe catalogue before importing blocks.\n", stderr);
    }
    bool single = argc >= 3 && strcmp(argv[2], "all");
    string_t *selected = single ? string_new_with(argv[2]) : NULL;
    size_t entries = single ? 1 : json_object_size(assets), total = 0, prepared = 0;
    for (size_t i = 0; i < entries; ++i) {
        const string_t *relative = single ? selected : json_object_key_at(assets, i);
        const json_t *asset = json_object_get(assets, relative);
        const string_t *width = json_number_text(field(asset, "width")), *height = json_number_text(field(asset, "height"));
        if (width && height && !strcmp(string_c_str(width), string_c_str(height))) ++total;
    }
    time_t start = time(NULL), last_notification = start;
    texture_pyramid_notify(notifications, "Preparing small photographic blocks; completed sources are retained when resuming.");
    for (size_t i = 0; ok && i < entries; ++i) {
        const string_t *relative = single ? selected : json_object_key_at(assets, i);
        const json_t *asset = json_object_get(assets, relative);
        const string_t *width = json_number_text(field(asset, "width")), *height = json_number_text(field(asset, "height"));
        const string_t *sha = json_string_value(field(asset, "sha256"));
        unsigned edge = width ? (unsigned)strtoul(string_c_str(width), NULL, 10) : 0;
        if (!width || !height || strcmp(string_c_str(width), string_c_str(height)) ||
            (edge != 1800 && edge != 3600) || !sha || string_byte_length(sha) != 64) continue;
        ok = prepare(root, relative, string_c_str(sha), edge, installed);
        printf("%zu/%zu sources; %s; %llds; %s\n", ++prepared, total,
            string_c_str(relative), (long long)(time(NULL) - start), ok ? "ready" : "FAILED");
        fflush(stdout);
        if (time(NULL) - last_notification >= 900) {
            string_t *message = string_sprintf("%zu/%zu photographic sources ready%s.", prepared, total,
                                               installed ? " and installed" : "");
            texture_pyramid_notify(notifications, message ? string_c_str(message) : NULL);
            string_free(message); last_notification = time(NULL);
        }
    }
    ok = ok && prepared > 0;
    texture_pyramid_notify(notifications, ok ? "Photographic block preparation completed." :
                                             "Photographic preparation stopped; rerun to resume.");
    sqlite_close(installed); file_free(installed_file); string_free(installed_path); string_free(selected);
    json_free(manifest); string_free(text); file_info_free(info); file_free(manifest_file); file_free(folder);
    string_free(manifest_path); string_free(folder_path); string_free(root);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
