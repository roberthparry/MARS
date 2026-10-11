/**
 * @file csv_write.c
 * @brief Bounded CSV record validation, escaping and output publication.
 *
 * Builds one validated record before writing any of its bytes. File errors can
 * still leave partial output; atomic replacement is a caller/file-t.h concern.
 * In-memory output is explicitly bounded and never implicitly taken on free.
 */
#include <errno.h>

#include "csv_internal.h"

static bool csv_publish(csv_t *csv, const string_t *text)
{
    if (!csv->file) {
        if (string_byte_length(text) > csv->options.max_buffer_bytes - string_byte_length(csv->buffer))
            return csv_fail(csv, EOVERFLOW);
        return string_append_bytes(csv->buffer, string_c_str(text), string_byte_length(text)) == 0 || csv_fail(csv, ENOMEM);
    }
    return file_write_text(csv->file, text) || csv_fail(csv, errno ? errno : EIO);
}

static bool csv_write_bom(csv_t *csv)
{
    if (csv->bom_written)
        return true;
    csv->bom_written = true;
    if (!csv->options.emit_bom)
        return true;
    string_t *bom = string_new();
    bool ok = bom && string_append_bytes(bom, "\xef\xbb\xbf", 3) == 0;
    if (ok)
        ok = csv_publish(csv, bom);
    else
        csv_fail(csv, ENOMEM);
    string_free(bom);
    return ok;
}

static bool csv_write_append(csv_t *csv, string_t *out, const char *bytes, size_t size)
{
    if (size > csv->options.max_record_bytes - string_byte_length(out))
        return csv_fail(csv, EOVERFLOW);
    return string_append_bytes(out, bytes, size) == 0 || csv_fail(csv, ENOMEM);
}

static bool csv_write_field(csv_t *csv, string_t *out, const string_t *field)
{
    if (!csv_valid_text(field))
        return csv_fail(csv, errno);
    size_t length = string_byte_length(field);
    if (length > csv->options.max_field_bytes)
        return csv_fail(csv, EOVERFLOW);
    bool quoted = csv->options.quote_all;
    string_view_t view = string_view_all(field);
    /* One bounded pass selects quoting; a second emits the field, escaping each quote. */
    for (size_t i = 0; !quoted && i < length; ++i) {
        unsigned char c;
        if (string_view_peek_ascii(view, i, &c))
            quoted = c == '"' || c == '\r' || c == '\n' || c == (unsigned char)csv->options.delimiter;
    }
    if (quoted && !csv_write_append(csv, out, "\"", 1))
        return false;
    size_t start = 0;
    for (size_t i = 0; quoted && i < length; ++i) {
        unsigned char c;
        if (string_view_peek_ascii(view, i, &c) && c == '"') {
            if (!csv_write_append(csv, out, string_c_str(field) + start, i - start) ||
                !csv_write_append(csv, out, "\"\"", 2))
                return false;
            start = i + 1;
        }
    }
    return csv_write_append(csv, out, string_c_str(field) + start, length - start) &&
           (!quoted || csv_write_append(csv, out, "\"", 1));
}

/* Validate complete records before publication, preserving user text exactly. */
bool csv_write(csv_t *csv, const string_t *const *fields, size_t count)
{
    if (!csv || !csv->writer || csv->finished || csv->error.code || !fields || !count)
        return csv_fail(csv, EINVAL);
    if (count > csv->options.max_columns)
        return csv_fail(csv, EOVERFLOW);
    if (csv->options.strict_columns && csv->width && count != csv->width)
        return csv_fail(csv, EINVAL);
    string_t *record = string_new();
    bool ok = record != NULL;
    if (!ok)
        csv_fail(csv, ENOMEM);
    for (size_t i = 0; ok && i < count; ++i) {
        csv->count = i;
        ok = (!i || csv_write_append(csv, record, &csv->options.delimiter, 1)) &&
             csv_write_field(csv, record, fields[i]);
    }
    csv->count = 0;
    if (ok && string_append_cstr(record, csv->options.crlf ? "\r\n" : "\n") != 0)
        ok = csv_fail(csv, ENOMEM);
    if (ok)
        ok = csv_write_bom(csv) && csv_publish(csv, record);
    string_free(record);
    if (ok) {
        ++csv->records;
        if (!csv->width)
            csv->width = count;
    }
    return ok;
}

/* Explicit finish makes final flush failures observable. */
bool csv_finish(csv_t *csv)
{
    if (!csv || !csv->writer || csv->error.code)
        return csv_fail(csv, EINVAL);
    if (csv->finished)
        return true;
    if (!csv_write_bom(csv) || (csv->file && !file_flush(csv->file)))
        return csv_fail(csv, errno ? errno : EIO);
    csv->finished = true;
    return true;
}

/* The output remains owned by the memory writer. */
const string_t *csv_output(const csv_t *csv)
{
    return csv && csv->writer && !csv->file ? csv->buffer : NULL;
}
