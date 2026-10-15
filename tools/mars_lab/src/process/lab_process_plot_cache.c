/**
 * @file lab_process_plot_cache.c
 * @brief Bounded shared-memory reuse of native solver plot work.
 *
 * Eight direct-indexed slots per record kind are shared by the existing request
 * processes. Exact keys protect against hash collisions, changes of source,
 * precision, viewport and rebuilt executables. A robust process-shared mutex
 * protects short record copies only; calculations and compression never hold it.
 * Oversized SVG records are compressed into the existing bounded slots. Nothing is
 * written to disk and cache allocation failure leaves normal execution intact.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <zstd.h>

#include "file-t.h"
#include "lab_process.h"
#define MARS_LAB_PROCESS_INTERNAL_ACCESS
#include "lab_process_plot_cache.h"

enum { CACHE_SLOTS = 8, KEY_LIMIT = 16384, VALUE_LIMIT = 1024 * 1024, PREP_LIMIT = 256 * 1024,
       DECODE_LIMIT = 8 * 1024 * 1024 };
typedef struct {
    size_t key_size, value_size, decoded_size;
    char key[KEY_LIMIT], value[VALUE_LIMIT];
} plot_cache_entry_t;
typedef struct {
    pthread_mutex_t mutex;
    plot_cache_entry_t entries[2][CACHE_SLOTS];
} plot_cache_t;
static plot_cache_t *cache;

/* Allocate before request workers are forked; failure merely disables this optimisation. */
void lab_proc_plot_cache_init(void)
{
    if (cache)
        return;
    plot_cache_t *memory = mmap(NULL, sizeof(*memory), PROT_READ | PROT_WRITE,
                               MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED)
        return;
    pthread_mutexattr_t attributes;
    if (pthread_mutexattr_init(&attributes)) {
        munmap(memory, sizeof(*memory));
        return;
    }
    bool ok = !pthread_mutexattr_setpshared(&attributes, PTHREAD_PROCESS_SHARED) &&
              !pthread_mutexattr_setrobust(&attributes, PTHREAD_MUTEX_ROBUST) &&
              !pthread_mutex_init(&memory->mutex, &attributes);
    pthread_mutexattr_destroy(&attributes);
    if (ok)
        cache = memory;
    else
        munmap(memory, sizeof(*memory));
}

static bool lock_cache(void)
{
    if (!cache)
        return false;
    int status = pthread_mutex_lock(&cache->mutex);
    if (status == EOWNERDEAD) {
        memset(cache->entries, 0, sizeof(cache->entries));
        if (!pthread_mutex_consistent(&cache->mutex))
            return true;
        pthread_mutex_unlock(&cache->mutex);
        return false;
    }
    return status == 0;
}

static size_t slot_for(const string_t *key)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    const unsigned char *bytes = (const unsigned char *)string_c_str(key);
    for (size_t i = 0; i < string_byte_length(key); ++i)
        hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
    return hash % CACHE_SLOTS;
}

/* Keys use lengths, not delimiters that authored expressions could contain. */
string_t *lab_proc_plot_key(const char *const argv[], const char *cwd, const string_t *path, bool prepared)
{
    const char *disabled = getenv("MARS_LAB_PLOT_CACHE");
    const char *limit = getenv("MARS_LAB_WORK_LIMIT");
    if (!cache || (disabled && !strcmp(disabled, "0")) || (limit && *limit && strcmp(limit, "0")))
        return NULL;
    file_t *executable = file_new(path);
    file_info_t *info = executable ? file_get_info(executable) : NULL;
    int64_t seconds = 0;
    long nanos = 0;
    bool known = info && file_info_status_change_time(info, &seconds, &nanos);
    string_t *key = known ? string_sprintf("plot-v1:%lld:%ld:%llu:", (long long)seconds, nanos,
                                         (unsigned long long)file_info_size(info)) : NULL;
    file_info_free(info);
    file_free(executable);
    const char *prefix[] = {string_c_str(path), cwd ? cwd : ""};
    for (size_t i = 0; key && i < 2; ++i)
        if (string_append_format(key, "%zu:%s", strlen(prefix[i]), prefix[i]) < 0) {
            string_free(key);
            key = NULL;
        }
    for (size_t i = 0; key && argv[i]; ++i) {
        if (prepared && i > 3 && i != 18)
            continue;
        if (string_append_format(key, "%zu:%s", strlen(argv[i]), argv[i]) < 0 ||
            string_byte_length(key) >= KEY_LIMIT) {
            string_free(key);
            key = NULL;
        }
    }
    return key;
}

/* Copy while locked; no borrowed cache pointer escapes to the caller. */
string_t *lab_proc_plot_cache_get(const string_t *key, bool prepared)
{
    if (!key || !lock_cache())
        return NULL;
    plot_cache_entry_t *entry = &cache->entries[prepared][slot_for(key)];
    size_t length = string_byte_length(key);
    bool match = entry->key_size == length && !memcmp(entry->key, string_c_str(key), length);
    size_t encoded_size = match ? entry->value_size : 0, decoded_size = match ? entry->decoded_size : 0;
    char *encoded = decoded_size && decoded_size <= DECODE_LIMIT && encoded_size && encoded_size <= VALUE_LIMIT
                        ? malloc(encoded_size) : NULL;
    if (encoded)
        memcpy(encoded, entry->value, encoded_size);
    pthread_mutex_unlock(&cache->mutex);
    string_t *value = NULL;
    if (encoded && encoded_size == decoded_size && !encoded[encoded_size - 1]) {
        value = string_new_with(encoded);
    } else if (encoded) {
        char *decoded = malloc(decoded_size);
        size_t actual = decoded ? ZSTD_decompress(decoded, decoded_size, encoded, encoded_size) : 0;
        if (decoded && actual == decoded_size && !decoded[decoded_size - 1])
            value = string_new_with(decoded);
        free(decoded);
    }
    free(encoded);
    return value;
}

/* Publish only complete bounded records, invalidating the slot before copying. */
void lab_proc_plot_cache_put(const string_t *key, const string_t *value, bool prepared)
{
    if (!key || !value || string_byte_length(key) >= KEY_LIMIT ||
        string_byte_length(value) >= (prepared ? PREP_LIMIT : DECODE_LIMIT))
        return;
    size_t decoded_size = string_byte_length(value) + 1, encoded_size = decoded_size;
    const char *data = string_c_str(value);
    char *compressed = NULL;
    if (decoded_size > VALUE_LIMIT) {
        compressed = malloc(VALUE_LIMIT);
        encoded_size = compressed ? ZSTD_compress(compressed, VALUE_LIMIT, data, decoded_size, 1) : 0;
        if (!encoded_size || ZSTD_isError(encoded_size)) {
            free(compressed);
            return;
        }
        data = compressed;
    }
    if (!lock_cache()) {
        free(compressed);
        return;
    }
    plot_cache_entry_t *entry = &cache->entries[prepared][slot_for(key)];
    entry->key_size = 0;
    entry->value_size = encoded_size;
    entry->decoded_size = decoded_size;
    memcpy(entry->value, data, encoded_size);
    memcpy(entry->key, string_c_str(key), string_byte_length(key) + 1);
    entry->key_size = string_byte_length(key);
    pthread_mutex_unlock(&cache->mutex);
    free(compressed);
}

/* Preparation is a single JSON line, never exposed to browser result cards. */
string_t *lab_proc_plot_cache_extract(string_t **output)
{
    const char *text = output && *output ? string_c_str(*output) : NULL;
    const char *record = text ? strstr(text, "plot_prepared ") : NULL;
    while (record && record != text && record[-1] != '\n')
        record = strstr(record + 14, "plot_prepared ");
    if (!record)
        return NULL;
    const char *end = strchr(record, '\n');
    if (!end)
        return NULL;
    string_t *prepared = string_substr(*output, (size_t)(record - text) + 14, (size_t)(end - record) - 14);
    string_t *clean = string_new();
    if (!prepared || !clean || string_append_chars(clean, text, (size_t)(record - text)) ||
        string_append_cstr(clean, end + 1) < 0) {
        string_free(prepared);
        string_free(clean);
        return NULL;
    }
    string_free(*output);
    *output = clean;
    return prepared;
}
