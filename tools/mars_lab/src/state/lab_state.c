/**
 * @file lab_state.c
 * @brief Private, atomic Lab state persistence for cooperating prefork workers.
 *
 * A persistent sibling lock file protects the complete read/merge/replace cycle.
 * Each call opens its own lock descriptor after fork. MARS file APIs perform
 * locking, permission changes, synchronisation and replacement; JSON objects
 * retain unknown existing fields while accepting only the documented defaults'
 * keys from updates. Corrupt or unreadable state is never silently replaced.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "file.h"
#include "lab_page.h"
#include "lab_state.h"

static const json_t *lab_state_get(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = key ? json_object_get(object, key) : NULL;
    string_free(key);
    return value;
}

static bool lab_state_put(json_t *object, const char *name, const json_t *value)
{
    string_t *key = string_new_with(name);
    bool ok = key && value && json_object_set(object, key, value);
    string_free(key);
    return ok;
}

static string_t *lab_state_trimmed(const char *text)
{
    string_t *source = string_new_with(text ? text : "");
    if (!source)
        return NULL;
    string_view_t view = string_view_trim(string_view_all(source));
    string_t *result = string_from_view(&view);
    string_free(source);
    return result;
}

static string_t *lab_state_expanded(const char *raw)
{
    string_t *text = lab_state_trimmed(raw);
    if (!text)
        return NULL;
    if (!string_starts_with(text, "~"))
        return text;
    string_offset_t slash = string_find(text, "/");
    string_view_t user_view = string_view(text, 1, slash < 0 ? string_byte_length(text) - 1 : (size_t)slash - 1);
    string_view_t suffix_view =
        slash < 0 ? string_view_empty() : string_view(text, (size_t)slash, string_byte_length(text));
    string_t *user = string_from_view(&user_view), *suffix = string_from_view(&suffix_view);
    const char *home = NULL;
    struct passwd *entry = NULL;
    if (string_view_is_empty(user_view)) {
        home = getenv("HOME");
        if (!home || !*home)
            entry = getpwuid(getuid());
    } else {
        if (user)
            entry = getpwnam(string_c_str(user));
    }
    if (entry)
        home = entry->pw_dir;
    string_t *result = home && user && suffix ? string_sprintf("%s%S", home, suffix) : NULL;
    string_free(user);
    string_free(suffix);
    string_free(text);
    return result;
}

static string_t *lab_state_path(void)
{
    string_t *home = lab_state_expanded(getenv("MARS_HOME"));
    string_t *override = lab_state_expanded(getenv("MARS_LAB_STATE_FILE"));
    string_t *path = NULL;
    if (!home || !override)
        goto done;
    if (!string_byte_length(home)) {
        string_free(home);
        home = lab_state_expanded("~/.mars");
        if (!home)
            goto done;
    }
    const char *configured = string_c_str(override);
    if (string_starts_with(override, "/"))
        path = string_new_with(configured);
    else
        path = string_sprintf("%s/lab/%s", string_c_str(home),
                              string_byte_length(override) ? configured : "mars_lab_state.json");
done:
    string_free(home);
    string_free(override);
    return path;
}

static file_t *lab_state_lock_store(const string_t *path, bool exclusive, file_t **directory)
{
    string_t *parent = string_new();
    string_t *lock_path = string_sprintf("%S.lock", path);
    file_t *lock = lock_path ? file_new(lock_path) : NULL;
    string_cursor_t *cursor = string_cursor_new(path);
    bool ok = parent && lock && cursor, found = false;
    string_pos_t begin = cursor ? string_cursor_position(cursor) : 0, end = begin;
    while (cursor && !string_cursor_done(cursor)) {
        string_pos_t position = string_cursor_position(cursor);
        if (string_cursor_consume(cursor, "/")) {
            found = true;
            end = position == begin ? string_cursor_position(cursor) : position;
        } else
            string_cursor_next(cursor);
    }
    if (ok)
        ok = found ? !string_cursor_append_slice_between(parent, begin, end, cursor) : !string_append_cstr(parent, ".");
    string_cursor_free(cursor);
    *directory = ok ? file_new(parent) : NULL;
    /* Create private parents, but do not chmod an existing shared directory such as /tmp. */
    ok = ok && *directory && file_create_directory(*directory, 0700, true);
    /* Workers are single-threaded. Restrict creation before opening, not after data has been written. */
    mode_t previous_mask = umask(0077);
    if (ok)
        ok = file_open(lock, FILE_MODE_OPEN_OR_CREATE, FILE_ACCESS_READ_WRITE);
    umask(previous_mask);
    /* file_chmod requires a closed handle. The lock inode is never replaced. */
    ok = ok && file_close(lock) && file_chmod(lock, 0600) && file_open(lock, FILE_MODE_OPEN, FILE_ACCESS_READ_WRITE);
    if (ok) {
        do {
            ok = file_lock(lock, exclusive, true);
        } while (!ok && file_last_error(lock) == EINTR);
    }
    string_free(parent);
    string_free(lock_path);
    if (!ok) {
        file_free(lock);
        lock = NULL;
    }
    return lock;
}

static bool lab_state_integer(const json_t *value, long long *out)
{
    const string_t *text = json_type(value) == JSON_NUMBER ? json_number_text(value) : json_string_value(value);
    if (!text)
        return false;
    string_view_t view = string_view_trim(string_view_all(text));
    string_cursor_t *cursor = string_cursor_new_view(view);
    if (!cursor)
        return false;
    bool negative = string_cursor_consume(cursor, "-");
    if (!negative)
        string_cursor_consume(cursor, "+");
    unsigned long long number = 0, limit = (unsigned long long)LLONG_MAX + (negative ? 1u : 0u);
    bool any = false, valid = true;
    unsigned char ch;
    while (string_cursor_peek_ascii(cursor, &ch) && ch >= '0' && ch <= '9') {
        unsigned digit = ch - '0';
        if (number > (limit - digit) / 10) {
            valid = false;
            break;
        }
        number = number * 10 + digit;
        any = true;
        string_cursor_next(cursor);
    }
    valid = valid && any && string_cursor_done(cursor);
    if (valid)
        *out = negative ? (number == limit ? LLONG_MIN : -(long long)number) : (long long)number;
    string_cursor_free(cursor);
    return valid;
}

static bool lab_state_put_integer(json_t *object, const char *key, long long number)
{
    string_t *text = string_sprintf("%lld", number);
    json_t *value = text ? json_new_number(text) : NULL;
    bool ok = value && lab_state_put(object, key, value);
    json_free(value);
    string_free(text);
    return ok;
}

static json_t *lab_state_read_state(const string_t *path, const json_t *defaults)
{
    file_t *file = file_new(path);
    json_t *state = NULL;
    if (!file)
        return NULL;
    string_t *text = file_read_all_text(file);
    if (!text) {
        if (file_last_error(file) == ENOENT)
            state = json_clone(defaults);
        file_free(file);
        return state;
    }
    state = text ? json_from_text(text) : NULL;
    string_free(text);
    file_free(file);
    if (!state || json_type(state) != JSON_OBJECT) {
        json_free(state);
        return NULL;
    }
    for (size_t i = 0; i < json_object_size(defaults); ++i) {
        const string_t *key = json_object_key_at(defaults, i);
        const json_t *fallback = json_object_value_at(defaults, i), *saved = json_object_get(state, key);
        if ((!saved || json_type(saved) != json_type(fallback)) && !json_object_set(state, key, fallback))
            goto fail;
    }
    json_t *precision = (json_t *)lab_state_get(state, "precision_bits");
    const json_t *fallback = lab_state_get(defaults, "precision_bits");
    for (size_t i = 0; i < json_object_size(fallback); ++i) {
        const string_t *key = json_object_key_at(fallback, i);
        long long bits;
        if ((!lab_state_integer(json_object_get(precision, key), &bits) || bits < 17 || bits > 1048576) &&
            !json_object_set(precision, key, json_object_value_at(fallback, i)))
            goto fail;
    }
    return state;
fail:
    json_free(state);
    return NULL;
}

static bool lab_state_one_of(const string_t *text, const char *choices)
{
    /* Delimited fixed vocabularies are small (at most eleven entries). */
    if (!string_byte_length(text) || string_find(text, "|") >= 0)
        return false;
    string_t *needle = string_sprintf("|%S|", text), *haystack = string_new_with(choices);
    bool found = needle && haystack && string_find_string(haystack, needle) >= 0;
    string_free(haystack);
    string_free(needle);
    return found;
}

static bool lab_state_merge_precision(json_t *state, const json_t *updates, const json_t *defaults)
{
    json_t *precision = (json_t *)lab_state_get(state, "precision_bits");
    const json_t *accepted = lab_state_get(defaults, "precision_bits");
    for (size_t i = 0; i < json_object_size(updates); ++i) {
        const string_t *key = json_object_key_at(updates, i);
        long long bits;
        if (!json_object_get(accepted, key) || !lab_state_integer(json_object_value_at(updates, i), &bits))
            continue;
        bits = bits < 17 ? 17 : bits > 1048576 ? 1048576 : bits;
        if (!lab_state_put_integer(precision, string_c_str(key), bits))
            return false;
    }
    return true;
}

static bool lab_state_merge_versioned(json_t *state, const json_t *updates, const char *key, const char *stamp)
{
    const json_t *value = lab_state_get(updates, key), *timestamp = lab_state_get(updates, stamp);
    if (!value || json_type(value) != JSON_STRING)
        return true;
    long long saved = 0, incoming = 0;
    lab_state_integer(lab_state_get(state, stamp), &saved);
    if (timestamp) {
        if (!lab_state_integer(timestamp, &incoming))
            return true;
        if (incoming < 0)
            incoming = 0;
    } else {
        struct timespec now;
        if (clock_gettime(CLOCK_REALTIME, &now))
            return false;
        incoming = (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
    }
    if (incoming < saved)
        return true;
    string_t *text = lab_state_trimmed(string_c_str(json_string_value(value)));
    json_t *normalised = text ? json_new_string(text) : NULL;
    bool ok = normalised && (!string_byte_length(text) ||
                             (lab_state_put(state, key, normalised) && lab_state_put_integer(state, stamp, incoming)));
    json_free(normalised);
    string_free(text);
    return ok;
}

static bool lab_state_merge(json_t *state, const json_t *updates, const json_t *defaults)
{
    if (!lab_state_merge_versioned(state, updates, "expression", "expression_updated_at") ||
        !lab_state_merge_versioned(state, updates, "equation", "equation_updated_at"))
        return false;
    for (size_t i = 0; i < json_object_size(updates); ++i) {
        const string_t *key = json_object_key_at(updates, i);
        const char *name = string_c_str(key);
        const json_t *fallback = json_object_get(defaults, key), *value = json_object_value_at(updates, i);
        if (!fallback || !strcmp(name, "expression") || !strcmp(name, "equation") ||
            !strcmp(name, "expression_updated_at") || !strcmp(name, "equation_updated_at"))
            continue;
        if (!strcmp(name, "precision_bits")) {
            if (json_type(value) == JSON_OBJECT && !lab_state_merge_precision(state, value, defaults))
                return false;
            continue;
        }
        if (!strcmp(name, "integrator_interval_cap")) {
            long long cap;
            if (lab_state_integer(value, &cap) &&
                (cap == 500 || cap == 5000 || cap == 20000 || cap == 50000 || cap == 100000) &&
                !lab_state_put_integer(state, name, cap))
                return false;
            continue;
        }
        if (json_type(value) != JSON_STRING)
            continue;
        string_t *text = lab_state_trimmed(string_c_str(json_string_value(value)));
        if (!text)
            return false;
        bool valid = true;
        if (!strcmp(name, "lab_mode"))
            valid = lab_state_one_of(text, "|expression|equation|diffequation|matrix|integrator|datetime|almanac|");
        else if (!strcmp(name, "matrix_operation"))
            valid = lab_state_one_of(
                text, "|eval|inverse|multiply|eigenvalues|eigendecompose|charpoly|det|trace|rank|simplify|solve|");
        else if (!strcmp(name, "almanac_visibility"))
            valid = lab_state_one_of(text, "|all|visible|");
        else if (!strcmp(name, "matrix") || !strcmp(name, "diffequation") || !strcmp(name, "integrator_expression"))
            valid = string_byte_length(text) && string_find(text, "...") < 0;
        else if (!strcmp(name, "integrator_bounds") || !strcmp(name, "equation_variable"))
            valid = string_byte_length(text) != 0;
        json_t *normalised = valid ? json_new_string(text) : NULL;
        bool ok = !valid || (normalised && json_object_set(state, key, normalised));
        json_free(normalised);
        string_free(text);
        if (!ok)
            return false;
    }
    return true;
}

static bool lab_state_write_state(const string_t *path, file_t *directory, const json_t *state)
{
    string_t *text = json_to_string_pretty(state, 2);
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) {
        string_free(text);
        return false;
    }
    string_t *temporary_path =
        string_sprintf("%s.%ld.%lld.%ld.tmp", string_c_str(path), (long)getpid(), (long long)now.tv_sec, now.tv_nsec);
    file_t *temporary = temporary_path ? file_new(temporary_path) : NULL;
    file_t *destination = file_new(path);
    bool created = false, ok = text && temporary && destination && !string_append_char(text, '\n');
    mode_t previous_mask = umask(0077);
    if (ok)
        created = file_open(temporary, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE);
    umask(previous_mask);
    /* Exclusive creation under umask 0077 already established mode 0600. */
    ok = ok && created && file_write_text(temporary, text) && file_sync(temporary, false);
    if (created && !file_close(temporary))
        ok = false;
    if (ok)
        ok = file_move(temporary, destination, true);
    if (ok) {
        created = false;
        ok = file_open_directory(directory) && file_sync(directory, false);
    }
    if (created)
        file_delete(temporary);
    file_free(destination);
    file_free(temporary);
    string_free(temporary_path);
    string_free(text);
    return ok;
}

/* Load a coherent snapshot while cooperating workers hold off replacement. */
json_t *lab_state_load(void)
{
    string_t *path = lab_state_path();
    json_t *defaults = lab_page_defaults();
    file_t *directory = NULL, *lock = path && defaults ? lab_state_lock_store(path, false, &directory) : NULL;
    json_t *state = lock ? lab_state_read_state(path, defaults) : NULL;
    file_free(lock);
    file_free(directory);
    json_free(defaults);
    string_free(path);
    return state;
}

/* Re-read and merge inside the lock; never save an earlier worker's snapshot. */
bool lab_state_save(const json_t *updates)
{
    if (!updates || json_type(updates) != JSON_OBJECT)
        return false;
    string_t *path = lab_state_path();
    json_t *defaults = lab_page_defaults();
    file_t *directory = NULL, *lock = path && defaults ? lab_state_lock_store(path, true, &directory) : NULL;
    json_t *state = lock ? lab_state_read_state(path, defaults) : NULL;
    bool ok = state && lab_state_merge(state, updates, defaults) && lab_state_write_state(path, directory, state);
    json_free(state);
    file_free(lock);
    file_free(directory);
    json_free(defaults);
    string_free(path);
    return ok;
}
