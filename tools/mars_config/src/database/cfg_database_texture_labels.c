/**
 * @file cfg_database_texture_labels.c
 * @brief Optional geographic progress labels for image and prepared-block installation.
 *
 * Reads only a bounded catalogue snapshot and addresses regional entries by numeric slot. Public progress labels
 * omit acquisition suffixes and control characters; their absence never changes image validation or publication.
 * These database-module helpers supply display context without scanning image payloads or geographic tables.
 */
#include <string.h>

#include "globe-t.h"
#include "json-t.h"
#include "string-t.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

static const json_t *field(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = key ? json_object_get(object, key) : NULL;
    string_free(key);
    return value;
}

/* Catalogue descriptions remain optional even in older or synthetic stores without this metadata table. */
json_t *cfg_database_texture_catalogue(sqlite_t *db)
{
    sqlite_stmt_t *query = db ? sqlite_stmt_prepare(db,
        "SELECT json FROM globe_texture_manifest\n"
        "WHERE id = 1 AND length(CAST(json AS BLOB)) <= ?1") : NULL;
    bool ok = query && sqlite_stmt_bind_int64(query, 1, GLOBE_TEXTURE_MANIFEST_LIMIT) &&
              sqlite_stmt_step(query) == SQLITE_STEP_ROW;
    const char *value = ok ? sqlite_stmt_column_text(query, 0) : NULL;
    string_t *text = value ? string_new_with(value) : NULL;
    json_t *catalogue = text ? json_from_text(text) : NULL;
    string_free(text);
    sqlite_stmt_finalize(query);
    return catalogue;
}

/* Names do not affect import decisions, and regional lookup is indexed rather than a geographic search. */
string_t *cfg_database_texture_name(const json_t *catalogue, unsigned level, unsigned column)
{
    if (level != 3) return level <= 4 ? string_new_with("Global imagery") : NULL;
    const json_t *regions = field(catalogue, "regions");
    const json_t *region = json_array_get(regions, column);
    const string_t *text = json_string_value(field(region, "name"));
    if (!text || !string_byte_length(text) || string_byte_length(text) > 256) return NULL;
    const char *name = string_c_str(text), *suffix = strstr(name, " Sentinel tile ");
    size_t length = suffix ? (size_t)(suffix - name) : strlen(name);
    if (!length || length > 96 || strlen(name) != string_byte_length(text)) return NULL;
    string_cursor_t *cursor = string_cursor_new(text);
    bool safe = cursor != NULL;
    while (safe && !string_cursor_done(cursor)) {
        safe = !rune_is_control(string_cursor_peek(cursor));
        string_cursor_next(cursor);
    }
    string_cursor_free(cursor);
    unsigned char first = (unsigned char)name[0];
    if (first >= 'a' && first <= 'z') first = first - 'a' + 'A';
    return safe ? string_sprintf("%c%.*s", first, (int)length - 1, name + 1) : NULL;
}
