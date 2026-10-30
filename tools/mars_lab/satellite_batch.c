/**
 * @file satellite_batch.c
 * @brief Supervise resumable satellite rendering, bounded progress reports and validated source-package publication.
 *
 * All paths and task plans come from an explicit JSON job file. Child renderers read remote raster windows and
 * checkpoint individual coast-masked PNGs. Progress is written atomically and optionally announced on the local
 * desktop every fifteen minutes and at phase boundaries. A verified package is published by adding immutable
 * image links before atomically replacing its manifest. Installed databases are never modified here.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "file-t.h"
#include "globe-t.h"
#include "json-t.h"
#include "string-t.h"

extern char **environ;
static bool notifications = true;

static const json_t *field(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = key ? json_object_get(object, key) : NULL;
    string_free(key);
    return value;
}

static const char *text(const json_t *value)
{
    const string_t *s = json_string_value(value);
    return s ? string_c_str(s) : "";
}

static json_t *load(const char *path)
{
    file_t *file = file_new_cstr(path);
    file_info_t *info = file ? file_get_info(file) : NULL;
    string_t *s = info && file_info_size(info) <= GLOBE_TEXTURE_MANIFEST_LIMIT ? file_read_all_text(file) : NULL;
    json_t *value = s ? json_from_text(s) : NULL;
    string_free(s); file_info_free(info); file_free(file);
    return value;
}

static bool save(const char *path, const string_t *value)
{
    string_t *part = string_sprintf("%s.part", path);
    file_t *temporary = part ? file_new(part) : NULL, *target = file_new_cstr(path);
    bool ok = temporary && target && value && file_write_all_text(temporary, value) && file_move(temporary, target, true);
    file_free(temporary); file_free(target); string_free(part);
    return ok;
}

static void notify(const string_t *message)
{
    if (!message || !notifications) return;
    char *const args[] = {"notify-send", "--app-name=MARS", "MARS satellite tiles", (char *)string_c_str(message), NULL};
    pid_t child;
    if (!posix_spawnp(&child, args[0], NULL, NULL, args, environ))
        while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {}
}

static bool status(const json_t *job, const char *phase, size_t completed, size_t total, const char *state)
{
    string_t *value = string_sprintf("{\"state\":\"%s\",\"phase\":\"%s\",\"completed\":%zu,\"total\":%zu,"
                                    "\"updated\":%lld}\n", state, phase, completed, total, (long long)time(NULL));
    bool ok = save(text(field(job, "status")), value);
    string_free(value);
    return ok;
}

static size_t progress(const char *path, int64_t *offset, size_t previous)
{
    file_t *file = file_new_cstr(path);
    bool ok = file && file_open_text(file) && file_seek(file, *offset, FILE_SEEK_BEGIN);
    string_t *line = NULL;
    while (ok && file_read_line(file, &line) && line) {
        size_t completed, total;
        if (sscanf(string_c_str(line), "Tiles completed: %zu/%zu", &completed, &total) == 2 && completed <= total)
            previous = completed;
        string_free(line); line = NULL;
        ok = file_tell(file, offset);
    }
    string_free(line); file_free(file);
    return previous;
}

static bool run(char *const args[], const char *log, const json_t *job, const char *phase,
                size_t base, size_t total)
{
    posix_spawn_file_actions_t actions;
    bool initialised = !posix_spawn_file_actions_init(&actions);
    bool ok = initialised && !posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, log,
                                                             O_WRONLY | O_CREAT | O_TRUNC, 0600) &&
              !posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
    pid_t child = -1;
    if (ok) ok = !posix_spawn(&child, args[0], &actions, NULL, args, environ);
    if (initialised) posix_spawn_file_actions_destroy(&actions);
    int result = 0;
    size_t completed = 0;
    int64_t offset = 0;
    time_t last_save = 0, last_notice = time(NULL);
    while (ok) {
        pid_t waited = waitpid(child, &result, WNOHANG);
        if (waited == child) break;
        if (waited < 0 && errno != EINTR) { ok = false; break; }
        time_t now = time(NULL);
        if (now - last_save >= 10) {
            completed = progress(log, &offset, completed);
            ok = status(job, phase, base + completed, total, "working");
            last_save = now;
        }
        if (now - last_notice >= 900) {
            string_t *message = string_sprintf("%s: %zu/%zu tiles completed in the batch.", phase, base + completed, total);
            notify(message); string_free(message); last_notice = now;
        }
        struct timespec pause = {.tv_sec = 1};
        nanosleep(&pause, NULL);
    }
    if (!ok && child > 0) {
        kill(child, SIGTERM);
        while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {}
    }
    return ok && WIFEXITED(result) && WEXITSTATUS(result) == 0;
}

static bool publish(const json_t *job, const char *staging)
{
    string_t *path = string_sprintf("%s/manifest.json", staging);
    json_t *manifest = path ? load(string_c_str(path)) : NULL;
    const json_t *assets = field(manifest, "assets");
    string_t *base_path = string_sprintf("%s/manifest.json", text(field(job, "package")));
    file_t *base = base_path ? file_new(base_path) : NULL;
    string_t *sha = base ? file_sha256(base) : NULL;
    bool ok = manifest && json_type(assets) == JSON_OBJECT && sha &&
              !strcmp(string_c_str(sha), text(field(job, "source_manifest_sha256")));
    for (size_t i = 0; ok && i < json_object_size(assets); ++i) {
        const string_t *relative = json_object_key_at(assets, i);
        string_t *from = string_sprintf("%s/%s", staging, string_c_str(relative));
        string_t *to = string_sprintf("%s/%s", text(field(job, "package")), string_c_str(relative));
        file_t *source = from ? file_new(from) : NULL, *target = to ? file_new(to) : NULL;
        ok = source && target;
        if (ok && file_exists(target)) {
            string_t *digest = file_sha256(target);
            ok = digest && !strcmp(string_c_str(digest), text(field(json_object_value_at(assets, i), "sha256")));
            string_free(digest);
        } else if (ok) ok = file_create_hard_link(source, target);
        file_free(source); file_free(target); string_free(from); string_free(to);
    }
    file_t *prepared = path ? file_new(path) : NULL;
    ok = ok && prepared && base && file_move(prepared, base, true);
    if (ok) {
        /* Only remove verified staging links after the active manifest owns every image. */
        for (size_t i = 0; i < json_object_size(assets); ++i) {
            string_t *image = string_sprintf("%s/%s", staging, string_c_str(json_object_key_at(assets, i)));
            file_t *file = image ? file_new(image) : NULL;
            if (file) file_delete(file);
            file_free(file); string_free(image);
        }
        static const char *const directories[] = {"regional", "earth_tiles", ""};
        for (unsigned i = 0; i < 3; ++i) {
            string_t *name = string_sprintf("%s/%s", staging, directories[i]);
            file_t *directory = name ? file_new(name) : NULL;
            if (directory) file_remove_directory(directory);
            file_free(directory); string_free(name);
        }
    }
    file_free(prepared); file_free(base); string_free(path); string_free(base_path); string_free(sha); json_free(manifest);
    return ok;
}

/* Supervise an explicit finite rendering job; preserve all completed checkpoints on failure. */
int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "Usage: %s job.json\n", argv[0]); return EXIT_FAILURE; }
    json_t *job = load(argv[1]);
    if (field(job, "notifications") && !json_bool_value(field(job, "notifications"), &notifications)) {
        json_free(job); return EXIT_FAILURE;
    }
    const json_t *tasks = field(job, "tasks");
    size_t count = json_array_size(tasks), total = 0, completed = 0;
    bool ok = count > 0 && count <= 16;
    static const char *const required[] = {"renderer", "assembler", "package", "shoreline", "staging", "status",
                                          "source_manifest_sha256", "validation_log"};
    for (unsigned i = 0; ok && i < sizeof required / sizeof *required; ++i)
        ok = *text(field(job, required[i])) != 0;
    for (size_t i = 0; ok && i < count; ++i) {
        const json_t *task = json_array_get(tasks, i);
        const char *label = text(field(task, "label"));
        ok = *label && *text(field(task, "plan")) && *text(field(task, "directory")) && *text(field(task, "log"));
        for (const unsigned char *p = (const unsigned char *)label; ok && *p; ++p)
            ok = (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
                 *p == ' ' || *p == '-';
        json_t *plan = ok ? load(text(field(task, "plan"))) : NULL;
        size_t tiles = json_array_size(field(plan, "tiles"));
        ok = ok && tiles > 0 && tiles <= GLOBE_TEXTURE_REGION_LIMIT && total <= GLOBE_TEXTURE_REGION_LIMIT - tiles;
        total += tiles; json_free(plan);
    }
    if (!ok) { json_free(job); fputs("Invalid or incomplete satellite job.\n", stderr); return EXIT_FAILURE; }
    json_t *saved_status = load(text(field(job, "status")));
    bool ready = !strcmp(text(field(saved_status, "state")), "ready");
    json_free(saved_status);
    if (ready) { json_free(job); puts("This satellite job is already ready for installation."); return EXIT_SUCCESS; }
    string_t *lock_path = string_sprintf("%s.lock", text(field(job, "status")));
    file_t *lock = lock_path ? file_new(lock_path) : NULL;
    ok = ok && lock && file_open(lock, FILE_MODE_OPEN_OR_CREATE, FILE_ACCESS_READ_WRITE) && file_lock(lock, true, false);
    if (!ok) {
        fputs("Cannot lock the satellite job; another instance may own it.\n", stderr);
        file_free(lock); string_free(lock_path); json_free(job);
        return EXIT_FAILURE;
    }
    if (ok) {
        string_t *message = string_sprintf("Satellite tiling started: %zu coast-masked tiles; "
                                           "checkpoints and fifteen-minute updates enabled.", total);
        notify(message); string_free(message);
    }
    for (size_t i = 0; ok && i < count; ++i) {
        const json_t *task = json_array_get(tasks, i);
        const char *label = text(field(task, "label"));
        char *const args[] = {(char *)text(field(job, "renderer")), "--render", (char *)text(field(task, "plan")),
            "--remote", (char *)text(field(task, "directory")), (char *)text(field(job, "shoreline")), NULL};
        bool rendered = false;
        for (unsigned attempt = 0; !rendered && attempt < 3; ++attempt) {
            rendered = run(args, text(field(task, "log")), job, label, completed, total);
            if (!rendered) { struct timespec pause = {.tv_sec = 15}; nanosleep(&pause, NULL); }
        }
        ok = rendered;
        if (ok) {
            json_t *plan = load(text(field(task, "plan")));
            completed += json_array_size(field(plan, "tiles")); json_free(plan);
            status(job, label, completed, total, "working");
            string_t *message = string_sprintf("%s phase finished; %zu/%zu tiles completed.", label, completed, total);
            notify(message); string_free(message);
        }
    }
    string_t *staging = string_sprintf("%s.%ld", text(field(job, "staging")), (long)getpid());
    char *assemble[4 + 16 * 3] = {(char *)text(field(job, "assembler")), (char *)text(field(job, "package")),
                                 (char *)string_c_str(staging)};
    for (size_t i = 0; i < count && i < 16; ++i) {
        const json_t *task = json_array_get(tasks, i);
        assemble[3 + i * 3] = (char *)text(field(task, "label"));
        assemble[4 + i * 3] = (char *)text(field(task, "plan"));
        assemble[5 + i * 3] = (char *)text(field(task, "directory"));
    }
    if (ok) {
        string_t *notice = string_new_with("All tiles rendered; validating the complete coast-masked PNG catalogue.");
        notify(notice); string_free(notice);
        ok = run(assemble, text(field(job, "validation_log")), job, "Validating", completed, total) &&
             publish(job, string_c_str(staging));
    }
    status(job, ok ? "Ready for make release" : "Stopped; checkpoints retained", completed, total, ok ? "ready" : "failed");
    string_t *notice = string_new_with(ok ? "Satellite imagery is coast-masked and ready for make release."
        : "Satellite batch stopped. Completed tiles are safe; inspect its log before resuming.");
    notify(notice); string_free(notice);
    file_free(lock); string_free(lock_path); string_free(staging); json_free(job);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
