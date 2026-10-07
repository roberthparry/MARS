/* Incremental Server-Sent Events framing, with strict UTF-8 and bounded storage. */
#include <stdlib.h>
#include <string.h>
#define MARS_HTTP_INTERNAL_ACCESS
#include "http_internal.h"

struct _http_event_reader_t {
    array_t *line;
    string_t *data, *event, *id;
    http_event_fn callback;
    void *user_data;
    size_t limit;
    uint64_t retry;
    bool retry_set, data_seen, first, after_cr, failed, finished, active;
};

/* Create independent line/event buffers and persistent reconnection metadata. */
http_event_reader_t *http_event_reader_new(size_t limit, http_event_fn callback, void *user_data)
{
    if (!callback || limit > 67108864)
        return NULL;
    http_event_reader_t *reader = calloc(1, sizeof(*reader));
    if (!reader)
        return NULL;
    reader->limit = limit ? limit : 1048576;
    reader->callback = callback;
    reader->user_data = user_data;
    reader->first = true;
    reader->line = array_create(1, NULL, NULL);
    reader->data = string_new();
    reader->event = string_new();
    reader->id = string_new();
    if (!reader->line || !reader->data || !reader->event || !reader->id) {
        http_event_reader_free(reader);
        return NULL;
    }
    return reader;
}

/* Release an idle reader; callback code must not destroy its active reader. */
void http_event_reader_free(http_event_reader_t *reader)
{
    if (!reader || reader->active)
        return;
    array_destroy(reader->line);
    string_free(reader->data);
    string_free(reader->event);
    string_free(reader->id);
    free(reader);
}

static bool replace(string_t **target, const string_t *value)
{
    string_t *copy = string_clone(value);
    if (!copy)
        return false;
    string_free(*target);
    *target = copy;
    return true;
}

static bool dispatch(http_event_reader_t *reader)
{
    bool ok = true;
    if (reader->data_seen) {
        string_t *data = string_substr(reader->data, 0, string_byte_length(reader->data) - 1);
        string_t *event = string_byte_length(reader->event) ? string_clone(reader->event) : string_new_with("message");
        ok = data && event &&
             reader->callback(event, data, reader->id, reader->retry_set, reader->retry, reader->user_data);
        string_free(data);
        string_free(event);
    }
    string_free(reader->data);
    string_free(reader->event);
    reader->data = string_new();
    reader->event = string_new();
    reader->data_seen = false;
    return ok && reader->data && reader->event;
}

static bool line(http_event_reader_t *reader)
{
    string_t *text = string_new();
    if (!text || string_append_utf8_exact(text, array_get(reader->line, 0), array_size(reader->line))) {
        string_free(text);
        return false;
    }
    array_clear(reader->line);
    if (reader->first) {
        reader->first = false;
        if (string_find(text, "\xEF\xBB\xBF") == 0) {
            string_t *without_bom = string_substr(text, 3, string_byte_length(text) - 3);
            string_free(text);
            text = without_bom;
            if (!text)
                return false;
        }
    }
    if (!string_byte_length(text)) {
        string_free(text);
        return dispatch(reader);
    }
    string_offset_t colon = string_find(text, ":");
    if (colon == 0) {
        string_free(text);
        return true;
    }
    string_t *name = colon < 0 ? string_clone(text) : string_substr(text, 0, colon);
    size_t start = colon < 0 ? string_byte_length(text) : (size_t)colon + 1;
    unsigned char ch;
    if (string_view_peek_ascii(string_view_all(text), start, &ch) && ch == ' ')
        ++start;
    string_t *value = string_substr(text, start, string_byte_length(text) - start);
    bool ok = name && value;
    if (ok && string_view_equals_literal(string_view_all(name), "data")) {
        size_t used = string_byte_length(reader->data), count = string_byte_length(value);
        ok = used < reader->limit && count < reader->limit - used &&
             !string_append_utf8_exact(reader->data, string_c_str(value), count) &&
             !string_append_char(reader->data, '\n');
        reader->data_seen = true;
    } else if (ok && string_view_equals_literal(string_view_all(name), "event")) {
        ok = replace(&reader->event, value);
    } else if (ok && string_view_equals_literal(string_view_all(name), "id")) {
        if (!memchr(string_c_str(value), 0, string_byte_length(value)))
            ok = replace(&reader->id, value);
    } else if (ok && string_view_equals_literal(string_view_all(name), "retry")) {
        uint64_t number = 0;
        bool digits = string_byte_length(value) != 0;
        string_cursor_t *cursor = string_cursor_new(value);
        if (!cursor)
            ok = false;
        while (ok && digits && !string_cursor_done(cursor)) {
            digits = string_cursor_peek_ascii(cursor, &ch) && ch >= '0' && ch <= '9' &&
                     number <= (UINT64_MAX - (ch - '0')) / 10;
            if (digits)
                number = number * 10 + ch - '0';
            string_cursor_next(cursor);
        }
        if (ok && digits) {
            reader->retry = number;
            reader->retry_set = true;
        }
        string_cursor_free(cursor);
    }
    if (ok) {
        size_t used = string_byte_length(reader->data);
        size_t event_size = string_byte_length(reader->event), id_size = string_byte_length(reader->id);
        ok = event_size <= reader->limit - used && id_size <= reader->limit - used - event_size;
    }
    string_free(name);
    string_free(value);
    string_free(text);
    return ok;
}

/* Frame raw fragments into complete lines before interpreting fields with string_t. */
bool http_event_reader_feed(http_event_reader_t *reader, const void *data, size_t size)
{
    if (!reader || reader->active || reader->finished || reader->failed || (!data && size))
        return false;
    reader->active = true;
    const unsigned char *bytes = data;
    for (size_t i = 0; i < size && !reader->failed; ++i) {
        unsigned char ch = bytes[i];
        if (reader->after_cr) {
            reader->after_cr = false;
            if (ch == '\n')
                continue;
        }
        if (ch == '\n' || ch == '\r') {
            reader->after_cr = ch == '\r';
            reader->failed = !line(reader);
        } else {
            reader->failed = array_size(reader->line) >= reader->limit || !array_add(reader->line, &ch);
        }
    }
    reader->active = false;
    return !reader->failed;
}

/* Validate a trailing fragment but never dispatch an unterminated event at EOF. */
bool http_event_reader_finish(http_event_reader_t *reader)
{
    if (!reader || reader->active || reader->failed)
        return false;
    if (reader->finished)
        return true;
    reader->finished = true;
    string_t *tail = string_new();
    reader->failed = !tail || string_append_utf8_exact(tail, array_get(reader->line, 0), array_size(reader->line)) != 0;
    string_free(tail);
    return !reader->failed;
}

/* Borrow reconnection metadata even when no data event has been dispatched. */
const string_t *http_event_reader_id(const http_event_reader_t *reader)
{
    return reader ? reader->id : NULL;
}

/* Read the persistent retry delay without resetting it. */
bool http_event_reader_retry(const http_event_reader_t *reader, uint64_t *milliseconds)
{
    if (!reader || !milliseconds || !reader->retry_set)
        return false;
    *milliseconds = reader->retry;
    return true;
}
