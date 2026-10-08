/**
 * @file jurisdiction.c
 * @brief Jurisdiction engine configuration and shared storage.
 *
 * Manages engine lifetime, diagnostics, default jurisdiction and location data, and serialised configuration.
 * Calendar, holiday and timezone units query this shared configured state.
 *
 * This is part of jurisdiction.h and uses configured rule data. Results depend on that data's coverage and
 * currency rather than hard-coded assumptions about the host machine.
 */

/* Engine configuration, shared storage, jurisdiction defaults and serialisation. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jurisdiction_internal.h"
#include "file.h"

void jurisdiction_set_error(jurisdiction_t *jurisdiction, const char *message)
{
    if (!jurisdiction || !jurisdiction->error)
        return;
    string_clear(jurisdiction->error);
    (void)string_append_cstr(jurisdiction->error, message ? message : "jurisdiction error");
}

static string_t *jurisdiction_string_from_cstr(const char *text)
{
    if (!text || *text == '\0')
        return NULL;
    string_t *value = string_new();
    if (value && string_append_utf8_exact(value, text, strlen(text)) != 0) {
        string_free(value);
        value = NULL;
    }
    return value;
}

char *jurisdiction_dup_c_string(const char *text)
{
    size_t len;
    char *out;

    if (!text)
        return NULL;
    len = strlen(text);
    out = malloc(len + 1u);
    if (!out)
        return NULL;
    memcpy(out, text, len + 1u);
    return out;
}

static char *dup_printf_path3(const char *a, const char *b, const char *c)
{
    size_t len_a;
    size_t len_b;
    size_t len_c;
    char *out;

    if (!a || !b || !c)
        return NULL;
    len_a = strlen(a);
    len_b = strlen(b);
    len_c = strlen(c);
    out = malloc(len_a + len_b + len_c + 1u);
    if (!out)
        return NULL;
    memcpy(out, a, len_a);
    memcpy(out + len_a, b, len_b);
    memcpy(out + len_a + len_b, c, len_c);
    out[len_a + len_b + len_c] = '\0';
    return out;
}

static bool parse_double_text(const char *text, double *out)
{
    char *end = NULL;
    double value;

    if (!text || !*text || !out)
        return false;
    value = strtod(text, &end);
    if (end == text || (end && *end != '\0'))
        return false;
    *out = value;
    return true;
}

static char *mars_home_path(void)
{
    const char *mars_home = getenv("MARS_HOME");
    const char *home = getenv("HOME");

    if (mars_home && *mars_home)
        return jurisdiction_dup_c_string(mars_home);
    if (home && *home)
        return dup_printf_path3(home, "/.mars", "");
    return NULL;
}

static char *jurisdiction_config_path(void)
{
    char *mars_home = mars_home_path();
    char *config_path;

    if (!mars_home)
        return NULL;
    config_path = dup_printf_path3(mars_home, "/config/", "jurisdiction-db.env");
    free(mars_home);
    return config_path;
}

static char *legacy_holiday_config_path(void)
{
    char *mars_home = mars_home_path();
    char *config_path;

    if (!mars_home)
        return NULL;
    config_path = dup_printf_path3(mars_home, "/config/", "holiday-db.env");
    free(mars_home);
    return config_path;
}

/* Parse complete configuration lines as MARS strings; this is not a shell interpreter. */
static char *config_lookup_at_path(char *config_path, const char *name)
{
    file_t *file = NULL;
    string_t *wanted = NULL;
    string_t *line = NULL;
    char *result = NULL;

    if (!config_path || !name || !*name)
        goto done;
    wanted = string_new_with(name);
    file = file_new_cstr(config_path);
    if (!wanted || !file || !file_open_follow(file, FILE_MODE_OPEN, FILE_ACCESS_READ))
        goto done;

    while (file_read_line(file, &line) && line) {
        string_t *key = NULL;
        string_t *value = NULL;
        string_trim(line);
        if (string_starts_with(line, "export ")) {
            string_t *body = string_substr(line, 7, string_byte_length(line) - 7);
            string_free(line);
            line = body;
            if (!line)
                break;
        }
        string_offset_t equals = string_find(line, "=");
        if (equals >= 0) {
            key = string_substr(line, 0, (size_t)equals);
            value = string_substr(line, (size_t)equals + 1, string_byte_length(line) - (size_t)equals - 1);
        }
        bool matched = key && value && string_compare(key, wanted) == 0;
        if (matched) {
            string_trim(value);
            string_t *unquoted = string_unquote_shell(value);
            string_free(value);
            value = unquoted;
            if (value && string_byte_length(value))
                result = jurisdiction_dup_c_string(string_c_str(value));
        }
        string_free(key);
        string_free(value);
        string_free(line);
        line = NULL;
        if (result)
            break;
    }
    if (!file_close(file)) {
        free(result);
        result = NULL;
    }

done:
    string_free(line);
    string_free(wanted);
    file_free(file);
    free(config_path);
    return result;
}

static char *jurisdiction_config_lookup(const char *name)
{
    return config_lookup_at_path(jurisdiction_config_path(), name);
}

static char *legacy_holiday_config_lookup(const char *name)
{
    return config_lookup_at_path(legacy_holiday_config_path(), name);
}

static char *jurisdiction_db_path_from_env(void)
{
    const char *path_env = getenv("MARS_JURISDICTION_DB_PATH");
    char *configured_path;
    char *mars_home;

    if (path_env && *path_env)
        return jurisdiction_dup_c_string(path_env);
    path_env = getenv("MARS_HOLIDAY_DB_PATH");
    if (path_env && *path_env)
        return jurisdiction_dup_c_string(path_env);
    configured_path = jurisdiction_config_lookup("MARS_JURISDICTION_DB_PATH");
    if (!configured_path)
        configured_path = legacy_holiday_config_lookup("MARS_HOLIDAY_DB_PATH");
    if (configured_path)
        return configured_path;
    mars_home = mars_home_path();
    if (mars_home) {
        char *default_path = dup_printf_path3(mars_home, "/jurisdiction/", "mars_jurisdiction_rules.db");

        free(mars_home);
        return default_path;
    }
    return NULL;
}

static char *jurisdiction_db_key_from_env(void)
{
    const char *env_key;
    char *config_key;

    env_key = getenv("MARS_JURISDICTION_DB_KEY");
    if (env_key && *env_key)
        return jurisdiction_dup_c_string(env_key);
    env_key = getenv("MARS_HOLIDAY_DB_KEY");
    if (env_key && *env_key)
        return jurisdiction_dup_c_string(env_key);
    config_key = jurisdiction_config_lookup("MARS_JURISDICTION_DB_KEY");
    if (!config_key)
        config_key = legacy_holiday_config_lookup("MARS_HOLIDAY_DB_KEY");
    if (config_key && !*config_key) {
        free(config_key);
        return NULL;
    }
    return config_key;
}

static void map_country_code_to_jurisdiction(const char *country_code, char out[32])
{
    size_t len;

    if (!out)
        return;
    out[0] = '\0';
    if (!country_code || !*country_code)
        return;
    if (strcmp(country_code, "GB") == 0) {
        strcpy(out, "GB-ENG");
        return;
    }
    len = strlen(country_code);
    if (len >= 32u)
        len = 31u;
    memcpy(out, country_code, len);
    out[len] = '\0';
}

static void extract_locale_country_code(const char *locale_value, char out[32])
{
    const char *underscore;
    size_t len;

    if (!out)
        return;
    out[0] = '\0';
    if (!locale_value || !*locale_value || strcmp(locale_value, "C") == 0 || strcmp(locale_value, "POSIX") == 0)
        return;

    underscore = strchr(locale_value, '_');
    if (!underscore || !underscore[1])
        return;
    locale_value = underscore + 1;
    len = strcspn(locale_value, ".@");
    if (len == 0u)
        return;
    if (len >= 32u)
        len = 31u;
    memcpy(out, locale_value, len);
    out[len] = '\0';
}

static void uppercase_ascii(char *text)
{
    size_t i;

    if (!text)
        return;
    for (i = 0u; text[i] != '\0'; ++i) {
        if (text[i] >= 'a' && text[i] <= 'z')
            text[i] = (char)(text[i] - 'a' + 'A');
    }
}

static void detect_default_jurisdiction(char out[32])
{
    const char *env_jurisdiction;
    const char *locale_candidates[3];
    size_t i;
    char country_code[32];

    if (!out)
        return;

    env_jurisdiction = getenv("MARS_HOLIDAY_JURISDICTION");
    if (env_jurisdiction && *env_jurisdiction) {
        size_t len = strlen(env_jurisdiction);

        if (len >= 32u)
            len = 31u;
        memcpy(out, env_jurisdiction, len);
        out[len] = '\0';
        return;
    }

    locale_candidates[0] = getenv("LC_ALL");
    locale_candidates[1] = getenv("LC_MESSAGES");
    locale_candidates[2] = getenv("LANG");

    for (i = 0u; i < 3u; ++i) {
        extract_locale_country_code(locale_candidates[i], country_code);
        uppercase_ascii(country_code);
        if (country_code[0] != '\0') {
            map_country_code_to_jurisdiction(country_code, out);
            if (out[0] != '\0')
                return;
        }
    }

    strcpy(out, "GB-ENG");
}

void jurisdiction_vec_free(jurisdiction_vec_t *vec)
{
    free(vec ? vec->items : NULL);
    if (vec) {
        vec->items = NULL;
        vec->count = 0u;
        vec->capacity = 0u;
    }
}

bool jurisdiction_vec_push(jurisdiction_vec_t *vec, const void *item)
{
    void *grown;

    if (!vec || !item || vec->item_size == 0u)
        return false;
    if (vec->count == vec->capacity) {
        size_t new_capacity = vec->capacity ? vec->capacity * 2u : 16u;

        grown = realloc(vec->items, new_capacity * vec->item_size);
        if (!grown)
            return false;
        vec->items = grown;
        vec->capacity = new_capacity;
    }
    memcpy((char *)vec->items + vec->count * vec->item_size, item, vec->item_size);
    vec->count += 1u;
    return true;
}

bool jurisdiction_year_in_range(int year, int valid_from_year, int valid_to_year)
{
    if (valid_from_year != 0 && year < valid_from_year)
        return false;
    if (valid_to_year != 0 && year > valid_to_year)
        return false;
    return true;
}

bool jurisdiction_load_lineage(sqlite_t *db, const char *jurisdiction, jurisdiction_vec_t *lineage_rows)
{
    static const char sql[] = "with recursive lineage(jurisdiction_id, depth) as ("
                              "  select ?1, 0 "
                              "  union all "
                              "  select j.parent_jurisdiction_id, lineage.depth + 1 "
                              "  from jurisdiction j "
                              "  join lineage on j.jurisdiction_id = lineage.jurisdiction_id "
                              "  where j.parent_jurisdiction_id is not null"
                              ") "
                              "select jurisdiction_id, depth from lineage order by depth;";
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db, sql);
    sqlite_step_result_t rc;

    if (!stmt)
        return false;
    if (!sqlite_stmt_bind_text(stmt, 1, jurisdiction))
        goto fail;

    while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        lineage_row_t row;
        const char *jurisdiction_id = sqlite_stmt_column_text(stmt, 0);

        memset(&row, 0, sizeof(row));
        if (!jurisdiction_id)
            goto fail;
        snprintf(row.jurisdiction_id, sizeof(row.jurisdiction_id), "%s", jurisdiction_id);
        row.depth = sqlite_stmt_column_int(stmt, 1);
        if (!jurisdiction_vec_push(lineage_rows, &row))
            goto fail;
    }

    sqlite_stmt_finalize(stmt);
    return rc == SQLITE_STEP_DONE;

fail:
    sqlite_stmt_finalize(stmt);
    return false;
}

static bool load_default_location(sqlite_t *db, const jurisdiction_vec_t *lineage_rows, double *latitude, double *longitude)
{
    static const char sql[] = "select lat.latitude, lon.longitude "
                              "from jurisdiction_location_default as loc "
                              "join jurisdiction_location_default_latitude as lat "
                              "  on lat.jurisdiction_id = loc.jurisdiction_id "
                              "join jurisdiction_location_default_longitude as lon "
                              "  on lon.jurisdiction_id = loc.jurisdiction_id "
                              "where loc.jurisdiction_id = ?1;";
    sqlite_stmt_t *stmt = NULL;
    const lineage_row_t *lineage = lineage_rows ? lineage_rows->items : NULL;
    size_t i;
    bool found = false;

    if (!lineage || !latitude || !longitude)
        return false;
    stmt = sqlite_stmt_prepare(db, sql);
    if (!stmt)
        return false;

    for (i = 0; i < lineage_rows->count; ++i) {
        const char *lat_text;
        const char *lon_text;
        sqlite_step_result_t rc;
        double parsed_latitude;
        double parsed_longitude;

        sqlite_stmt_reset(stmt);
        sqlite_stmt_clear_bindings(stmt);
        if (!sqlite_stmt_bind_text(stmt, 1, lineage[i].jurisdiction_id))
            goto done;
        rc = sqlite_stmt_step(stmt);
        if (rc != SQLITE_STEP_ROW)
            continue;
        lat_text = sqlite_stmt_column_text(stmt, 0);
        lon_text = sqlite_stmt_column_text(stmt, 1);
        if (!parse_double_text(lat_text, &parsed_latitude) || !parse_double_text(lon_text, &parsed_longitude)) {
            goto done;
        }
        *latitude = parsed_latitude;
        *longitude = parsed_longitude;
        found = true;
        break;
    }

done:
    sqlite_stmt_finalize(stmt);
    return found;
}

/* Open the configured database and resolve the requested jurisdiction. */
jurisdiction_t *jurisdict_open(const char *jurisdiction_code)
{
    jurisdiction_t *jurisdiction;
    char *resolved_key = jurisdiction_db_key_from_env();
    string_t *path = NULL;
    string_t *key = NULL;
    const char *resolved_path = jurisdiction_db_path_from_env();

    if (!resolved_path || !*resolved_path || !resolved_key || !*resolved_key) {
        free((char *)resolved_path);
        free(resolved_key);
        return NULL;
    }
    jurisdiction = calloc(1u, sizeof(*jurisdiction));
    if (!jurisdiction) {
        free((char *)resolved_path);
        free(resolved_key);
        return NULL;
    }
    jurisdiction->error = string_new();
    if (!jurisdiction->error) {
        free((char *)resolved_path);
        free(resolved_key);
        free(jurisdiction);
        return NULL;
    }
    if (jurisdiction_code && *jurisdiction_code) {
        size_t len = strlen(jurisdiction_code);

        if (len >= sizeof(jurisdiction->jurisdiction))
            len = sizeof(jurisdiction->jurisdiction) - 1u;
        memcpy(jurisdiction->jurisdiction, jurisdiction_code, len);
        jurisdiction->jurisdiction[len] = '\0';
    } else {
        detect_default_jurisdiction(jurisdiction->jurisdiction);
    }
    path = jurisdiction_string_from_cstr(resolved_path);
    key = jurisdiction_string_from_cstr(resolved_key);
    jurisdiction->db = (path && key) ? sqlite_open_encrypted(path, key) : NULL;
    string_free(key);
    string_free(path);
    free((char *)resolved_path);
    free(resolved_key);
    if (!jurisdiction->db) {
        string_free(jurisdiction->error);
        free(jurisdiction);
        return NULL;
    }
    return jurisdiction;
}

/* Release the database and engine-owned storage. */
void jurisdict_close(jurisdiction_t *jurisdiction)
{
    if (!jurisdiction)
        return;
    sqlite_close(jurisdiction->db);
    string_free(jurisdiction->error);
    free(jurisdiction);
}

/* Return the engine's borrowed error text. */
const char *jurisdict_last_error(const jurisdiction_t *jurisdiction)
{
    return (jurisdiction && jurisdiction->error) ? string_c_str(jurisdiction->error) : NULL;
}

/* Find the first configured location in the jurisdiction's ancestry. */
bool jurisdict_default_location(jurisdiction_t *holiday, double *latitude, double *longitude)
{
    sqlite_t *db = holiday ? holiday->db : NULL;
    const char *jurisdiction = holiday ? holiday->jurisdiction : NULL;
    jurisdiction_vec_t lineage_rows = {0};
    bool ok;

    lineage_rows.item_size = sizeof(lineage_row_t);

    if (!holiday || !db || !jurisdiction || *jurisdiction == '\0' || !latitude || !longitude) {
        jurisdiction_set_error(holiday, "invalid holiday query");
        return false;
    }
    if (!jurisdiction_load_lineage(db, jurisdiction, &lineage_rows)) {
        jurisdiction_set_error(holiday, "failed to load jurisdiction lineage");
        jurisdiction_vec_free(&lineage_rows);
        return false;
    }
    ok = load_default_location(db, &lineage_rows, latitude, longitude);
    jurisdiction_vec_free(&lineage_rows);
    if (!ok)
        jurisdiction_set_error(holiday, "default jurisdiction location unavailable");
    return ok;
}

/* Serialise the jurisdiction code without copying private database state. */
bool jurisdict_serialize(const jurisdiction_t *jurisdiction, string_t **out_type, string_t **out_encoding,
                         void **out_data, size_t *out_len)
{
    string_t *type = NULL;
    string_t *encoding = NULL;
    char *payload;
    size_t len;

    if (!jurisdiction || !out_type || !out_encoding || !out_data || !out_len)
        return false;

    len = strlen(jurisdiction->jurisdiction);
    payload = malloc(len);
    if (!payload)
        return false;
    memcpy(payload, jurisdiction->jurisdiction, len);

    type = string_new_with("jurisdiction_t");
    encoding = string_new_with("jurisdiction-code/plain");
    if (!type || !encoding) {
        free(payload);
        string_free(type);
        string_free(encoding);
        return false;
    }

    *out_type = type;
    *out_encoding = encoding;
    *out_data = payload;
    *out_len = len;
    return true;
}

/* Reopen the configured engine from a serialised jurisdiction code. */
jurisdiction_t *jurisdict_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding)
{
    char code[32];

    if (!data || !type || !encoding || len == 0u || len >= sizeof(code))
        return NULL;
    if (strcmp(string_c_str(type), "jurisdiction_t") != 0 ||
        strcmp(string_c_str(encoding), "jurisdiction-code/plain") != 0)
        return NULL;

    memcpy(code, data, len);
    code[len] = '\0';
    return jurisdict_open(code);
}
