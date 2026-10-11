/**
 * @file csv_read.c
 * @brief Incremental, bounded CSV record parsing using string_t buffers.
 *
 * Shares one state machine between file_t input and caller-fed chunks. Quoted
 * line endings remain field content, doubled quotes decode once, and UTF-8 is
 * checked before any record is exposed. No partial row is returned on failure.
 */
#include <errno.h>
#include <stdlib.h>

#define MARS_CSV_INTERNAL_ACCESS
#include "csv_internal.h"

static bool csv_queue(csv_t *csv, const string_t *chunk, bool final)
{
    size_t remaining = string_byte_length(csv->buffer) - csv->position;
    size_t incoming = chunk ? string_byte_length(chunk) : 0;
    if (incoming > csv->options.max_buffer_bytes - remaining)
        return csv_fail(csv, EOVERFLOW);
    string_t *buffer = string_new();
    bool ok = buffer && string_append_bytes(buffer, string_c_str(csv->buffer) + csv->position, remaining) == 0 &&
              (!incoming || string_append_bytes(buffer, string_c_str(chunk), incoming) == 0);
    if (!ok) {
        string_free(buffer);
        return csv_fail(csv, ENOMEM);
    }
    string_free(csv->buffer);
    csv->buffer = buffer;
    csv->position = 0;
    csv->final = final;
    csv->need_input = false;
    return true;
}

/* Feeding copies raw bytes; field validation waits until split scalars are complete. */
bool csv_feed(csv_t *csv, const string_t *chunk, bool final)
{
    if (!csv || csv->writer || csv->file || csv->final || !csv->need_input || csv->error.code)
        return csv_fail(csv, EINVAL);
    return csv_queue(csv, chunk, final);
}

static bool csv_refill(csv_t *csv)
{
    char bytes[4096];
    size_t count = 0, available = csv->options.max_buffer_bytes -
                                  (string_byte_length(csv->buffer) - csv->position);
    size_t capacity = available < sizeof(bytes) ? available : sizeof(bytes);
    if (!capacity)
        return csv_fail(csv, EOVERFLOW);
    if (!file_read(csv->file, bytes, capacity, &count))
        return csv_fail(csv, errno ? errno : EIO);
    string_t *chunk = string_new();
    bool ok = chunk && string_append_bytes(chunk, bytes, count) == 0;
    if (ok)
        ok = csv_queue(csv, chunk, count == 0);
    else
        csv_fail(csv, ENOMEM);
    string_free(chunk);
    return ok;
}

static bool csv_push_field(csv_t *csv)
{
    if (csv->count == csv->options.max_columns)
        return csv_fail(csv, EOVERFLOW);
    if (!csv_valid_text(csv->field))
        return csv_fail(csv, errno);
    if (csv->count == csv->capacity) {
        size_t capacity = csv->capacity ? csv->capacity * 2 : 8;
        if (capacity < csv->capacity || capacity > csv->options.max_columns)
            capacity = csv->options.max_columns;
        string_t **fields = realloc(csv->fields, capacity * sizeof(*fields));
        if (!fields)
            return csv_fail(csv, ENOMEM);
        csv->fields = fields;
        csv->capacity = capacity;
    }
    string_t *next = string_new();
    if (!next)
        return csv_fail(csv, ENOMEM);
    csv->fields[csv->count++] = csv->field;
    csv->field = next;
    csv->state = CSV_FIELD_START;
    return true;
}

static bool csv_end_row(csv_t *csv)
{
    if (!csv_push_field(csv))
        return false;
    if (!csv->header_ready && csv->options.first_row_header) {
        if (!csv_install_header(csv, (const string_t *const *)csv->fields, csv->count))
            return false;
        csv_clear_row(csv);
    } else {
        if (csv->options.strict_columns && csv->width && csv->count != csv->width)
            return csv_fail(csv, EINVAL);
        if (!csv->width)
            csv->width = csv->count;
        csv->row_ready = true;
    }
    ++csv->records;
    csv->started = false;
    csv->record_bytes = 0;
    return true;
}

static bool csv_append_byte(csv_t *csv, unsigned char byte)
{
    if (string_byte_length(csv->field) == csv->options.max_field_bytes)
        return csv_fail(csv, EOVERFLOW);
    char c = (char)byte;
    return string_append_bytes(csv->field, &c, 1) == 0 || csv_fail(csv, ENOMEM);
}

/* Return only complete rows; partial fields are retained across NEED_INPUT. */
csv_status_t csv_next(csv_t *csv)
{
    if (!csv || csv->writer) {
        csv_fail(csv, EINVAL);
        return CSV_ERROR;
    }
    if (csv->row_ready)
        csv_clear_row(csv);
    if (csv->error.code)
        return CSV_ERROR;
    for (;;) {
        size_t available = string_byte_length(csv->buffer) - csv->position;
        if ((!available || (!csv->bom_checked && available < 3)) && !csv->final) {
            csv->need_input = true;
            if (!csv->file)
                return CSV_NEED_INPUT;
            if (!csv_refill(csv))
                return CSV_ERROR;
            continue;
        }
        if (!csv->bom_checked) {
            string_view_t start = string_view(csv->buffer, csv->position, available);
            if (available >= 3 && string_view_equals_literal(string_view_slice(start, 0, 3), "\xef\xbb\xbf")) {
                csv->position += 3;
                csv->bytes += 3;
                available -= 3;
            }
            csv->bom_checked = true;
        }
        if (!available) {
            if (!csv->final)
                continue;
            if (csv->state == CSV_FIELD_QUOTED) {
                csv_fail(csv, EINVAL);
                return CSV_ERROR;
            }
            if (csv->started && !csv_end_row(csv))
                return CSV_ERROR;
            return csv->row_ready ? CSV_ROW : CSV_END;
        }
        unsigned char byte = (unsigned char)string_c_str(csv->buffer)[csv->position++];
        ++csv->bytes;
        if (byte == '\r' || (byte == '\n' && !csv->last_cr))
            ++csv->line;
        csv->last_cr = byte == '\r';
        if (csv->skip_lf) {
            csv->skip_lf = false;
            if (byte == '\n')
                continue;
        }
        if (!byte) {
            csv_fail(csv, EILSEQ);
            return CSV_ERROR;
        }
        if (csv->state != CSV_FIELD_QUOTED && (byte == '\r' || byte == '\n')) {
            csv->skip_lf = byte == '\r';
            if (!csv_end_row(csv))
                return CSV_ERROR;
            if (csv->row_ready)
                return CSV_ROW;
            continue;
        }
        if (csv->record_bytes == csv->options.max_record_bytes) {
            csv_fail(csv, EOVERFLOW);
            return CSV_ERROR;
        }
        ++csv->record_bytes;
        csv->started = true;
        if (csv->state == CSV_FIELD_QUOTED) {
            if (byte == '"')
                csv->state = CSV_FIELD_CLOSED;
            else if (!csv_append_byte(csv, byte))
                return CSV_ERROR;
        } else if (byte == (unsigned char)csv->options.delimiter) {
            if (!csv_push_field(csv))
                return CSV_ERROR;
        } else if (byte == '"') {
            if (csv->state == CSV_FIELD_START)
                csv->state = CSV_FIELD_QUOTED;
            else if (csv->state == CSV_FIELD_CLOSED) {
                if (!csv_append_byte(csv, byte))
                    return CSV_ERROR;
                csv->state = CSV_FIELD_QUOTED;
            } else {
                csv_fail(csv, EINVAL);
                return CSV_ERROR;
            }
        } else {
            if (csv->state == CSV_FIELD_CLOSED) {
                csv_fail(csv, EINVAL);
                return CSV_ERROR;
            }
            csv->state = CSV_FIELD_PLAIN;
            if (!csv_append_byte(csv, byte))
                return CSV_ERROR;
        }
    }
}
