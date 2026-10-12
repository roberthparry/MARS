/**
 * @file csv.c
 * @brief CSV stream lifetime, policies and indexed header ownership.
 *
 * Constructs borrowed-file or memory streams, preserves error locations and
 * maintains an exact-name dictionary alongside the original ordered headers.
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#define MARS_CSV_INTERNAL_ACCESS
#include "csv_internal.h"

/* Return finite, explicit defaults for both reading and writing. */
csv_options_t csv_options_default(void)
{
    return (csv_options_t){.delimiter = ',', .crlf = true, .max_field_bytes = 1024 * 1024,
                           .max_record_bytes = 8 * 1024 * 1024, .max_columns = 4096,
                           .max_buffer_bytes = 8 * 1024 * 1024};
}

bool csv_fail(csv_t *csv, int code)
{
    if (csv && !csv->error.code)
        csv->error = (csv_error_t){code, csv->records + 1, csv->line, csv->count + 1, csv->bytes};
    errno = csv && csv->error.code ? csv->error.code : code;
    return false;
}

bool csv_valid_text(const string_t *text)
{
    if (!text) {
        errno = EINVAL;
        return false;
    }
    string_view_t view = string_view_all(text);
    for (string_pos_t at = 0, next; at < string_view_length(view); at = next) {
        uint32_t rune;
        if (!string_view_peek_rune_value(view, at, &rune, &next) || !rune) {
            errno = EILSEQ;
            return false;
        }
    }
    /* Exact validation also rejects overlong, surrogate and incomplete sequences. */
    errno = 0;
    string_t *copy = string_new();
    bool valid = copy && string_append_utf8_exact(copy, string_c_str(text), string_byte_length(text)) == 0;
    string_free(copy);
    if (!valid && errno != ENOMEM)
        errno = EILSEQ;
    return valid;
}

static csv_t *csv_new(file_t *file, const csv_options_t *options, bool writer)
{
    csv_options_t policy = options ? *options : csv_options_default();
    unsigned char delimiter = (unsigned char)policy.delimiter;
    if (!delimiter || delimiter >= 128 || delimiter == '"' || delimiter == '\r' || delimiter == '\n' ||
        !policy.max_columns || policy.max_columns > SIZE_MAX / sizeof(string_t *) || !policy.max_field_bytes ||
        !policy.max_record_bytes || policy.max_buffer_bytes < 4 || (file && !file_is_open(file))) {
        errno = EINVAL;
        return NULL;
    }
    csv_t *csv = calloc(1, sizeof(*csv));
    if (!csv)
        return NULL;
    csv->options = policy;
    csv->file = file;
    csv->writer = writer;
    csv->line = 1;
    csv->need_input = true;
    csv->buffer = string_new();
    csv->field = string_new();
    if (!csv->buffer || !csv->field) {
        csv_free(csv);
        return NULL;
    }
    return csv;
}

/* Borrow an open file, or accept caller-fed chunks. */
csv_t *csv_reader_new(file_t *file, const csv_options_t *options)
{
    return csv_new(file, options, false);
}

/* Borrow an open file, or retain bounded output in memory. */
csv_t *csv_writer_new(file_t *file, const csv_options_t *options)
{
    return csv_new(file, options, true);
}

void csv_clear_row(csv_t *csv)
{
    for (size_t i = 0; i < csv->count; ++i)
        string_free(csv->fields[i]);
    csv->count = 0;
    csv->row_ready = false;
}

/* Release CSV-owned storage only; a borrowed file remains open. */
void csv_free(csv_t *csv)
{
    if (!csv)
        return;
    csv_clear_row(csv);
    dictionary_destroy(csv->index);
    for (size_t i = 0; i < csv->header_count; ++i) {
        string_free(csv->headers[i]);
        string_free(csv->keys[i]);
    }
    free(csv->headers);
    free(csv->keys);
    free(csv->fields);
    string_free(csv->buffer);
    string_free(csv->field);
    free(csv);
}

static size_t csv_hash(const void *key)
{
    return string_hash(*(const string_t *const *)key);
}

static int csv_compare(const void *a, const void *b)
{
    return string_compare(*(const string_t *const *)a, *(const string_t *const *)b);
}

static string_t *csv_key(const csv_t *csv, const string_t *name)
{
    string_t *key = string_clone(name);
    if (key && csv->options.trim_headers)
        string_trim(key);
    return key;
}

bool csv_install_header(csv_t *csv, const string_t *const *names, size_t count)
{
    if (!names || !count || csv->header_ready)
        return csv_fail(csv, EINVAL);
    if (count > csv->options.max_columns)
        return csv_fail(csv, EOVERFLOW);
    csv->headers = calloc(count, sizeof(*csv->headers));
    csv->keys = calloc(count, sizeof(*csv->keys));
    csv->index = dictionary_create(sizeof(string_t *), sizeof(size_t), csv_hash, csv_compare,
                                    NULL, NULL, NULL, NULL, NULL);
    if (!csv->headers || !csv->keys || !csv->index)
        return csv_fail(csv, ENOMEM);
    csv->header_count = count;
    size_t bytes = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!csv_valid_text(names[i]))
            return csv_fail(csv, errno);
        size_t length = string_byte_length(names[i]);
        if (length > csv->options.max_field_bytes || length > csv->options.max_record_bytes - bytes)
            return csv_fail(csv, EOVERFLOW);
        bytes += length;
        csv->headers[i] = string_clone(names[i]);
        csv->keys[i] = csv_key(csv, names[i]);
        if (!csv->headers[i] || !csv->keys[i])
            return csv_fail(csv, ENOMEM);
        size_t old, position = i;
        if (dictionary_get(csv->index, &csv->keys[i], &old))
            position = SIZE_MAX;
        if (!dictionary_set(csv->index, &csv->keys[i], &position))
            return csv_fail(csv, ENOMEM);
    }
    csv->header_ready = true;
    csv->width = count;
    return true;
}

/* Caller-supplied headers also allow named access to headerless input. */
bool csv_set_header(csv_t *csv, const string_t *const *names, size_t count)
{
    if (!csv || csv->error.code || csv->finished || !csv->need_input || csv->records || csv->bytes ||
        string_byte_length(csv->buffer))
        return csv_fail(csv, EINVAL);
    if (!csv_install_header(csv, names, count))
        return false;
    return !csv->writer || csv_write(csv, names, count);
}

/* Fields belong to the current complete data record. */
size_t csv_field_count(const csv_t *csv)
{
    return csv && csv->row_ready ? csv->count : 0;
}

/* Report complete CSV records independently of supplied header metadata. */
size_t csv_record_count(const csv_t *csv)
{
    return csv ? csv->records : 0;
}

/* Empty and absent fields are deliberately distinct. */
const string_t *csv_field(const csv_t *csv, size_t column)
{
    return csv && csv->row_ready && column < csv->count ? csv->fields[column] : NULL;
}

/* Header order is independent of dictionary iteration order. */
size_t csv_header_count(const csv_t *csv)
{
    return csv && csv->header_ready ? csv->header_count : 0;
}

/* Borrow the original spelling, not the lookup key. */
const string_t *csv_header(const csv_t *csv, size_t column)
{
    return csv && csv->header_ready && column < csv->header_count ? csv->headers[column] : NULL;
}

/* Indexed lookup refuses ambiguous and empty names without poisoning a reader. */
bool csv_column(const csv_t *csv, const string_t *name, size_t *column)
{
    if (!csv || !name || !column || !csv->header_ready) {
        errno = EINVAL;
        return false;
    }
    string_t *key = csv_key(csv, name);
    if (!key)
        return false;
    size_t found = 0;
    bool ok = string_byte_length(key) && dictionary_get(csv->index, &key, &found);
    string_free(key);
    if (!ok || found == SIZE_MAX) {
        errno = ok ? EEXIST : ENOENT;
        return false;
    }
    *column = found;
    return true;
}

/* Error inspection does not modify the stream. */
csv_error_t csv_error(const csv_t *csv)
{
    return csv ? csv->error : (csv_error_t){.code = EINVAL};
}

/* Copy structured stream context without retaining any input field contents. */
diagnostic_t *csv_diagnostic(const csv_t *csv)
{
    static const char *const reasons[] = {
        [EINVAL]     = "Invalid CSV syntax or stream operation",
        [EILSEQ]     = "Invalid UTF-8 or NUL byte",
        [EOVERFLOW]  = "CSV size limit exceeded",
        [ENOMEM]     = "CSV allocation failed",
        [ECANCELED]  = "CSV operation cancelled",
        [ETIMEDOUT]  = "CSV operation timed out",
        [E2BIG]      = "CSV work budget exhausted",
        [EIO]        = "CSV input/output failed",
    };
    if (!csv) {
        errno = EINVAL;
        return NULL;
    }
    if (!csv->error.code)
        return NULL;
    const csv_error_t *error = &csv->error;
    const char *reason = (unsigned)error->code < sizeof(reasons) / sizeof(*reasons) ? reasons[error->code] : NULL;
    string_t *message = string_sprintf("%s (record %zu, field %zu)", reason ? reason : "CSV stream failure",
                                       error->record, error->field);
    diagnostic_location_t location = {.source = csv->file ? file_path(csv->file) : NULL,
                                       .offset = error->byte, .line = error->line};
    diagnostic_t *diagnostic = message ? diagnostic_new(DIAGNOSTIC_ERROR, "csv", error->code, string_c_str(message),
                                                        csv->writer ? NULL : &location, NULL) : NULL;
    string_free(message);
    return diagnostic;
}
