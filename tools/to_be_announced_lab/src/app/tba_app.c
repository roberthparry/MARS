/**
 * @file tba_app.c
 * @brief Forecasting configuration, locked Protobuf state and private uploads.
 *
 * Saved state uses .to_be_announced_lab_state.pb unless explicitly overridden.
 * File locks serialise prefork workers; writes are atomically published. Uploads
 * use generated names, never client filenames, and do not overwrite existing data.
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tba_app.h"

struct tba_app {
    string_t *base, *state_path, *upload_path, *result_path;
    json_t *defaults;
};

static bool tba_app_merge(json_t *state, const json_t *update)
{
    if (json_type(update) != JSON_OBJECT)
        return false;
    for (size_t i = 0; i < json_object_size(state); ++i) {
        const string_t *key = json_object_key_at(state, i);
        const json_t *value = json_object_get(update, key);
        if (value && (json_type(value) == JSON_STRING || json_type(value) == JSON_NUMBER) &&
            !json_object_set(state, key, value))
            return false;
    }
    return true;
}

/* Resolve application paths without creating or overwriting saved state. */
tba_app_t *tba_app_new(const char *base)
{
    tba_app_t *app = calloc(1, sizeof *app);
    if (!app)
        return NULL;
    const char *state = getenv("MARS_LAB_STATE_FILE"), *uploads = getenv("TBA_LAB_UPLOAD_DIR");
    const char *prefix = base ? base : getenv("MARS_LAB_PUBLIC_PATH");
    if (!prefix)
        prefix = "/to-be-announced";
    app->base = string_new_with(!strcmp(prefix, "/") ? "" : prefix);
    app->state_path = tba_path(state && *state ? state : ".to_be_announced_lab_state.pb");
    app->upload_path = tba_path(uploads && *uploads ? uploads : ".to_be_announced_uploads");
    app->result_path = app->upload_path ? string_sprintf("%S/latest_result.pb", app->upload_path) : NULL;
    app->defaults = tba_app_defaults();
    bool prefix_ok = app->base && (!*prefix || prefix[0] == '/') && string_find(app->base, "..") < 0 &&
                     string_find(app->base, "?") < 0 && string_find(app->base, "#") < 0;
    if (!prefix_ok || !app->state_path || !app->upload_path || !app->result_path ||
        json_type(app->defaults) != JSON_OBJECT) {
        tba_app_free(app);
        return NULL;
    }
    return app;
}

/* Release app-owned configuration. */
void tba_app_free(tba_app_t *app)
{
    if (app) {
        string_free(app->base);
        string_free(app->state_path);
        string_free(app->upload_path);
        string_free(app->result_path);
        json_free(app->defaults);
        free(app);
    }
}

/* Borrow the public URL prefix. */
const char *tba_app_base(const tba_app_t *app)
{
    return app ? string_c_str(app->base) : "";
}

/* Load only recognised saved scalar settings over packaged defaults. */
json_t *tba_app_state(const tba_app_t *app)
{
    if (!app)
        return NULL;
    json_t *state = json_clone(app->defaults);
    json_t *saved = tba_file_read_message(string_c_str(app->state_path));
    if (json_type(saved) == JSON_OBJECT && !tba_app_merge(state, saved)) {
        json_free(state);
        state = NULL;
    }
    json_free(saved);
    return state;
}

/* Serialise concurrent settings updates with a separate stable lock inode. */
bool tba_app_save(const tba_app_t *app, const json_t *update)
{
    if (!app || json_type(update) != JSON_OBJECT)
        return false;
    string_t *path = string_sprintf("%S.lock", app->state_path);
    file_t *lock = path ? file_new(path) : NULL;
    bool ok = lock && tba_file_open_private(lock, FILE_MODE_OPEN_OR_CREATE, FILE_ACCESS_READ_WRITE) &&
              file_lock(lock, true, true);
    json_t *state = ok ? tba_app_state(app) : NULL;
    ok = ok && state && tba_app_merge(state, update);
    ok = ok && tba_file_publish_message(string_c_str(app->state_path), state);
    json_free(state);
    file_free(lock);
    string_free(path);
    return ok;
}

static bool tba_app_upload_directory(const tba_app_t *app)
{
    file_t *directory = file_new(app->upload_path);
    bool ok = directory && file_create_directory(directory, 0700, true);
    file_free(directory);
    return ok;
}

/* Store complete UTF-8 upload bytes under an exclusively created private name. */
string_t *tba_app_upload(const tba_app_t *app, const string_t *text)
{
    if (!app || !text || !string_byte_length(text) || string_byte_length(text) > 16u * 1024u * 1024u ||
        !tba_app_upload_directory(app))
        return NULL;
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    string_t *path = string_sprintf("%S/upload-%lld-%ld-%ld.csv", app->upload_path, (long long)now.tv_sec, now.tv_nsec,
                                    (long)getpid());
    file_t *file = path ? file_new(path) : NULL;
    bool created = file && tba_file_open_private(file, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE);
    bool ok =
        created && file_write_text(file, text) && file_sync(file, false) && file_close(file);
    if (created && !ok) {
        file_close(file);
        file_delete(file);
    }
    file_free(file);
    if (!ok) {
        string_free(path);
        path = NULL;
    }
    return path;
}

/* Retain successful results across prefork request workers. */
bool tba_app_result_save(const tba_app_t *app, const json_t *result)
{
    return app && tba_json_bool(result, "ok") && tba_app_upload_directory(app) &&
           tba_file_publish_message(string_c_str(app->result_path), result);
}

/* Read the last complete published result. */
json_t *tba_app_result(const tba_app_t *app)
{
    return app ? tba_file_read_message(string_c_str(app->result_path)) : NULL;
}
