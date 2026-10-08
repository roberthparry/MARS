/**
 * @file tba_csv.c
 * @brief Bounded UTF-8 CSV parsing with indexed headers and round-trip quoting.
 *
 * Reads at most 100,000 records, 256 columns and one million cells. Malformed
 * quoting, duplicate/empty headings, NULs and over-budget input fail explicitly.
 * File handling is delegated to file_t; CSV parsing uses string_t cursors.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tba_csv.h"

struct tba_csv {
    json_t *rows;
    json_t *index;
    json_t *headers;
    json_t *overrides;
};

static bool tba_csv_field(json_t *row, string_t *field)
{
    json_t *value = json_new_string(field);
    bool ok = value && json_array_size(row) < 256 && json_array_append(row, value);
    json_free(value);
    string_clear(field);
    return ok;
}

/* Parse records without splitting quoted line breaks or UTF-8 characters. */
tba_csv_t *tba_csv_parse(const string_t *text)
{
    if (!text || string_byte_length(text) > 16u * 1024u * 1024u)
        return NULL;
    tba_csv_t *csv = calloc(1, sizeof *csv);
    string_cursor_t *cursor = string_cursor_new(text);
    json_t *row = json_new_array();
    string_t *field = string_new();
    if (csv) {
        csv->rows = json_new_array();
        csv->index = json_new_object();
        csv->headers = json_new_array();
        csv->overrides = json_new_object();
    }
    bool ok = csv && csv->rows && csv->index && csv->headers && csv->overrides && cursor && row && field;
    bool quoted = false, closed = false, pending = false;
    size_t cells = 0;
    if (ok)
        string_cursor_consume(cursor, "\xef\xbb\xbf");
    while (ok && !string_cursor_done(cursor)) {
        unsigned char ch = 0xff;
        bool ascii = string_cursor_peek_ascii(cursor, &ch);
        if (ascii && ch == 0) {
            ok = false;
            break;
        }
        if (ascii && ch == '"') {
            string_cursor_next(cursor);
            if (quoted) {
                if (string_cursor_consume(cursor, "\""))
                    ok = string_append_cstr(field, "\"") == 0;
                else {
                    quoted = false;
                    closed = true;
                }
            } else if (!closed && !string_byte_length(field))
                quoted = true;
            else
                ok = false;
            pending = true;
            continue;
        }
        if (!quoted && ascii && (ch == ',' || ch == '\r' || ch == '\n')) {
            ok = tba_csv_field(row, field) && ++cells <= 1000000;
            string_cursor_next(cursor);
            closed = false;
            pending = ch == ',';
            if (ch != ',') {
                if (ch == '\r')
                    string_cursor_consume(cursor, "\n");
                if (json_array_size(row) != 1 || *string_c_str(json_string_value(json_array_get(row, 0))))
                    ok = ok && json_array_size(csv->rows) <= 100000 && json_array_append(csv->rows, row);
                json_free(row);
                row = json_new_array();
                ok = ok && row;
            }
            continue;
        }
        if (closed) {
            ok = false;
            break;
        }
        string_pos_t start = string_cursor_position(cursor);
        string_cursor_next(cursor);
        ok = string_cursor_append_slice_between(field, start, string_cursor_position(cursor), cursor) == 0;
        pending = true;
    }
    if (ok && pending)
        ok = tba_csv_field(row, field) && ++cells <= 1000000 && json_array_size(csv->rows) <= 100000 &&
             json_array_append(csv->rows, row);
    ok = ok && !quoted && csv && json_array_size(csv->rows) > 0;
    if (ok) {
        const json_t *header = json_array_get(csv->rows, 0);
        for (size_t c = 0; ok && c < json_array_size(header); ++c) {
            const string_t *name = json_string_value(json_array_get(header, c));
            string_view_t view = string_view_trim(string_view_all(name));
            string_t *trimmed = string_from_view(&view);
            ok = trimmed && string_byte_length(trimmed) && !json_object_get(csv->index, trimmed) &&
                 tba_json_number(csv->index, string_c_str(trimmed), (double)c) &&
                 tba_json_append(csv->headers, string_c_str(trimmed));
            string_free(trimmed);
        }
    }
    string_free(field);
    string_cursor_free(cursor);
    json_free(row);
    if (!ok) {
        tba_csv_free(csv);
        return NULL;
    }
    return csv;
}

/* Load through bounded file storage before parsing. */
tba_csv_t *tba_csv_open(const char *path)
{
    string_t *text = tba_file_read(path, 16u * 1024u * 1024u);
    tba_csv_t *csv = text ? tba_csv_parse(text) : NULL;
    string_free(text);
    return csv;
}

/* Release every owned cell and index. */
void tba_csv_free(tba_csv_t *csv)
{
    if (csv) {
        json_free(csv->rows);
        json_free(csv->index);
        json_free(csv->headers);
        json_free(csv->overrides);
        free(csv);
    }
}

/* Exclude the header from the data count. */
size_t tba_csv_rows(const tba_csv_t *csv)
{
    return csv && json_array_size(csv->rows) ? json_array_size(csv->rows) - 1 : 0;
}

/* Report the retained header width. */
size_t tba_csv_columns(const tba_csv_t *csv)
{
    return csv ? json_array_size(json_array_get(csv->rows, 0)) : 0;
}

/* Return canonical trimmed header text from the index. */
const char *tba_csv_header(const tba_csv_t *csv, size_t column)
{
    const string_t *name = csv ? json_string_value(json_array_get(csv->headers, column)) : NULL;
    return name ? string_c_str(name) : "";
}

/* Borrow a data cell without manufacturing missing values. */
const string_t *tba_csv_cell(const tba_csv_t *csv, size_t row, size_t column)
{
    if (!csv || row >= tba_csv_rows(csv))
        return NULL;
    string_t *key = string_sprintf("%zu:%zu", row, column);
    const json_t *override = key ? json_object_get(csv->overrides, key) : NULL;
    string_free(key);
    return json_string_value(override ? override : json_array_get(json_array_get(csv->rows, row + 1), column));
}

/* Resolve a header through the JSON object's key index. */
size_t tba_csv_column(const tba_csv_t *csv, const char *name)
{
    const string_t *text = csv ? json_number_text(tba_json_get(csv->index, name)) : NULL;
    double index;
    return tba_text_number(text, &index) ? (size_t)index : SIZE_MAX;
}

/* Replace a cell in place, padding previously absent cells. */
bool tba_csv_set(tba_csv_t *csv, size_t row, size_t column, const string_t *text)
{
    if (!csv || row >= tba_csv_rows(csv) || column >= tba_csv_columns(csv) || !text)
        return false;
    string_t *key = string_sprintf("%zu:%zu", row, column);
    json_t *value = json_new_string(text);
    bool ok = key && value && json_object_set(csv->overrides, key, value);
    json_free(value);
    string_free(key);
    return ok;
}

/* Quote fields only when CSV syntax requires it. */
string_t *tba_csv_text(const tba_csv_t *csv)
{
    string_t *text = csv ? string_new() : NULL;
    bool ok = text != NULL;
    for (size_t r = 0; ok && r < json_array_size(csv->rows); ++r) {
        for (size_t c = 0; ok && c < tba_csv_columns(csv); ++c) {
            const string_t *value =
                r ? tba_csv_cell(csv, r - 1, c) : json_string_value(json_array_get(csv->headers, c));
            string_t *escaped = string_new_with(value ? string_c_str(value) : "");
            bool quote = escaped && (string_find(escaped, ",") >= 0 || string_find(escaped, "\"") >= 0 ||
                                     string_find(escaped, "\r") >= 0 || string_find(escaped, "\n") >= 0);
            ok = escaped && string_replace(escaped, "\"", "\"\"") >= 0 &&
                 string_append_format(text, "%s%s%S%s", c ? "," : "", quote ? "\"" : "", escaped, quote ? "\"" : "") >=
                     0;
            string_free(escaped);
        }
        ok = ok && string_append_cstr(text, "\n") == 0;
    }
    if (!ok) {
        string_free(text);
        text = NULL;
    }
    return text;
}
