/**
 * @file format_stream.c
 * @brief Bounded template reuse and allocation-free staging for common typed stream output.
 *
 * Small plans are cached by exact template content and binding layout, never by borrowed pointer identity.
 * Values remain call-local. Pinned entries survive recursive callbacks; worker-thread destructors and process
 * exit release cached plans. Complete output is staged before writing so formatting failures remain atomic.
 */
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "format-t.h"
#define MARS_FORMAT_INTERNAL_ACCESS
#include "format_internal.h"

enum { FMT_CACHE_SLOTS = 8, FMT_CACHE_ARGS = 16, FMT_CACHE_BYTES = 512, FMT_STREAM_BYTES = 1024 };

typedef struct {
    fmt_t *format;
    unsigned users;
    fmt_adapter_t adapters[FMT_CACHE_ARGS];
} fmt_cached_t;

typedef struct {
    fmt_cached_t entries[FMT_CACHE_SLOTS];
    fmt_cached_t *last;
} fmt_cache_t;

typedef struct {
    char *bytes;
    size_t length;
    size_t capacity;
    char local[FMT_STREAM_BYTES];
} fmt_stream_buffer_t;

static _Thread_local fmt_cache_t *fmt_local_cache;
static pthread_key_t fmt_cache_key;
static pthread_once_t fmt_cache_once = PTHREAD_ONCE_INIT;
static bool fmt_cache_available;

static void fmt_cache_destroy(void *data)
{
    fmt_cache_t *cache = data;
    if (!cache)
        return;
    for (size_t i = 0; i < FMT_CACHE_SLOTS; ++i)
        fmt_free(cache->entries[i].format);
    if (fmt_local_cache == cache)
        fmt_local_cache = NULL;
    free(cache);
}

static void fmt_cache_exit(void)
{
    if (fmt_local_cache) {
        pthread_setspecific(fmt_cache_key, NULL);
        fmt_cache_destroy(fmt_local_cache);
    }
}

static void fmt_cache_init(void)
{
    if (pthread_key_create(&fmt_cache_key, fmt_cache_destroy) != 0)
        return;
    if (atexit(fmt_cache_exit) != 0) {
        pthread_key_delete(fmt_cache_key);
        return;
    }
    fmt_cache_available = true;
}

static fmt_cache_t *fmt_cache_get(void)
{
    if (fmt_local_cache)
        return fmt_local_cache;
    pthread_once(&fmt_cache_once, fmt_cache_init);
    if (!fmt_cache_available)
        return NULL;
    fmt_cache_t *cache = calloc(1u, sizeof(*cache));
    if (cache && pthread_setspecific(fmt_cache_key, cache) != 0) {
        free(cache);
        cache = NULL;
    }
    fmt_local_cache = cache;
    return cache;
}

static bool fmt_cache_matches(const fmt_t *format, const char *pattern, const fmt_arg_impl_t *args, size_t count)
{
    if (!format || format->arguments != count || strcmp(pattern, string_c_str(format->text)))
        return false;
    for (size_t i = 0; i < count; ++i) {
        const fmt_layout_t *layout = &format->layout[i];
        const char *name = layout->name ? string_c_str(layout->name) : NULL;
        if (layout->kind != args[i].kind || !!name != !!args[i].name || (name && strcmp(name, args[i].name)))
            return false;
        if (args[i].kind == FMT_KIND_ADAPTER) {
            const fmt_adapter_t *current = args[i].value.adapter.type;
            if (!current || layout->adapter->validate != current->validate ||
                layout->adapter->render != current->render || layout->adapter->numeric != current->numeric)
                return false;
        }
    }
    return true;
}

static fmt_t *fmt_stream_plan(const char *pattern, const fmt_arg_t *arguments, const fmt_arg_impl_t *decoded,
                              size_t count, fmt_cached_t **entry, fmt_error_t *error)
{
    fmt_cache_t *cache = pattern && count <= FMT_CACHE_ARGS && (!count || arguments) ? fmt_cache_get() : NULL;
    if (!cache)
        return fmt_compile(pattern, arguments, count, error);
    if (cache->last && fmt_cache_matches(cache->last->format, pattern, decoded, count)) {
        *entry = cache->last;
        ++(*entry)->users;
        return (*entry)->format;
    }
    size_t length = strnlen(pattern, FMT_CACHE_BYTES + 1u), names = 0u;
    for (size_t i = 0; i < count && names <= FMT_CACHE_BYTES; ++i)
        if (decoded[i].name)
            names += strnlen(decoded[i].name, FMT_CACHE_BYTES + 1u);
    if (length > FMT_CACHE_BYTES || names > FMT_CACHE_BYTES)
        return fmt_compile(pattern, arguments, count, error);
    uint32_t hash = UINT32_C(2166136261);
    for (size_t i = 0; i < length; ++i)
        hash = (hash ^ (unsigned char)pattern[i]) * UINT32_C(16777619);
    fmt_cached_t *slot = &cache->entries[hash & (FMT_CACHE_SLOTS - 1u)];
    if (fmt_cache_matches(slot->format, pattern, decoded, count)) {
        cache->last = *entry = slot;
        ++slot->users;
        return slot->format;
    }
    fmt_t *format = fmt_compile(pattern, arguments, count, error);
    if (!format || slot->users)
        return format;
    fmt_free(slot->format);
    /* One-shot callers lend adapter objects only for this call; cached validation uses owned callback snapshots. */
    for (size_t i = 0; i < count; ++i) {
        if (decoded[i].kind == FMT_KIND_ADAPTER) {
            slot->adapters[i] = *decoded[i].value.adapter.type;
            format->layout[i].adapter = &slot->adapters[i];
        }
    }
    slot->format = format;
    slot->users = 1u;
    cache->last = *entry = slot;
    return format;
}

static bool fmt_stream_append(fmt_stream_buffer_t *out, const char *bytes, size_t length,
                              const fmt_part_t *part, fmt_error_t *error)
{
    if (length > (size_t)INT_MAX - out->length)
        return fmt_error_set(error, FMT_ERROR_RANGE, part->offset, part->argument, "formatted output exceeds INT_MAX");
    size_t needed = out->length + length;
    if (needed > out->capacity) {
        size_t capacity = out->capacity <= (size_t)INT_MAX / 2u ? out->capacity * 2u : (size_t)INT_MAX;
        if (capacity < needed)
            capacity = needed;
        char *grown = out->bytes == out->local ? malloc(capacity) : realloc(out->bytes, capacity);
        if (!grown)
            return fmt_error_set(error, FMT_ERROR_MEMORY, part->offset, part->argument, "cannot stage stream output");
        if (out->bytes == out->local)
            memcpy(grown, out->local, out->length);
        out->bytes = grown;
        out->capacity = capacity;
    }
    if (length)
        memcpy(out->bytes + out->length, bytes, length);
    out->length = needed;
    return true;
}

/* Pin an exact template layout and complete staging before making the single checked stream write. */
int fmt_stream_write(FILE *stream, const char *pattern, const fmt_arg_t *arguments, size_t count, fmt_error_t *error)
{
    fmt_error_clear(error);
    if (!count && pattern) {
        size_t length = 0u;
        while (pattern[length] && (unsigned char)pattern[length] < 128u &&
               pattern[length] != '{' && pattern[length] != '}')
            ++length;
        if (!pattern[length]) {
            if (length > INT_MAX) {
                fmt_error_set(error, FMT_ERROR_RANGE, SIZE_MAX, SIZE_MAX, "formatted output exceeds INT_MAX");
                return -1;
            }
            if (fwrite(pattern, 1u, length, stream) != length || ferror(stream)) {
                fmt_error_set(error, FMT_ERROR_IO, SIZE_MAX, SIZE_MAX, "cannot write complete formatted output");
                return -1;
            }
            return (int)length;
        }
    }
    fmt_arg_impl_t decoded[FMT_CACHE_ARGS];
    if (count <= FMT_CACHE_ARGS && arguments)
        for (size_t i = 0; i < count; ++i)
            decoded[i] = fmt_arg_unpack(&arguments[i]);
    fmt_cached_t *entry = NULL;
    fmt_t *format = fmt_stream_plan(pattern, arguments, decoded, count, &entry, error);
    if (!format)
        return -1;
    fmt_stream_buffer_t out;
    out.bytes = out.local;
    out.length = 0u;
    out.capacity = FMT_STREAM_BYTES;
    int result = -1;
    for (size_t i = 0; i < format->count; ++i) {
        const fmt_part_t *part = &format->parts[i];
        const char *bytes = string_c_str(format->text) + part->offset;
        size_t length = part->length;
        string_t *owned = NULL;
        char scratch[128];
        if (part->argument != SIZE_MAX) {
            fmt_arg_impl_t argument = count <= FMT_CACHE_ARGS ? decoded[part->argument] :
                                                               fmt_arg_unpack(&arguments[part->argument]);
            if (!fmt_render_fast(&argument, part, scratch, sizeof(scratch), &bytes, &length)) {
                owned = fmt_render_field(&argument, part, error);
                if (!owned)
                    goto done;
                bytes = string_c_str(owned);
                length = string_byte_length(owned);
            }
        }
        bool appended = fmt_stream_append(&out, bytes, length, part, error);
        string_free(owned);
        if (!appended)
            goto done;
    }
    if (fwrite(out.bytes, 1u, out.length, stream) != out.length || ferror(stream)) {
        fmt_error_set(error, FMT_ERROR_IO, SIZE_MAX, SIZE_MAX, "cannot write complete formatted output");
        goto done;
    }
    result = (int)out.length;
    fmt_error_clear(error);

done:
    if (out.bytes != out.local)
        free(out.bytes);
    if (entry)
        --entry->users;
    else
        fmt_free(format);
    return result;
}
