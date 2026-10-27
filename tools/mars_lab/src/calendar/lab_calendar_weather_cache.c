/**
 * @file lab_calendar_weather_cache.c
 * @brief Bounded private on-disk cache of recent local weather observations for calendar fallback estimates.
 *
 * DateTime and Almanac workers share a bounded, unencrypted SQLite cache beside the configured
 * Lab state. Composite location/time indexes retain typed observations, never credentials or whole provider
 * responses. Transactions protect concurrent workers; contention skips the optional cache rather
 * than delaying calculations. Legacy JSON files are removed only after a successful import commit.
 * Calculated provider conditions are retained for 15 minutes, matched to the exact
 * coordinates, UTC moment and elevation, so returning to a worksheet need not flash seasonal estimates.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "file-t.h"
#include "sqlite-t.h"
#include "lab_state.h"
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_internal.h"

static string_t *weather_location(const json_t *options)
{
    double latitude, longitude;
    if (!lab_cal_number(lab_cal_text(options, "lat"), &latitude) || fabs(latitude) > 90 ||
        !lab_cal_number(lab_cal_text(options, "lon"), &longitude) || fabs(longitude) > 180) return NULL;
    return string_sprintf("%.3f,%.3f", latitude == 0 ? 0 : latitude, longitude == 0 ? 0 : longitude);
}

typedef struct _weather_entry_t {
    sqlite_t *db;
    string_t *key;
    int kind;
    double latitude, longitude, altitude, moment;
} weather_entry_t;

static bool weather_key(weather_entry_t *entry, const char *key, int kind)
{
    entry->kind = kind;
    entry->altitude = entry->moment = 0;
    if (kind) {
        const char *prefix = "observer-pressure-v4|";
        if (strncmp(key, prefix, strlen(prefix))) return false;
        key += strlen(prefix);
    }
    double *fields[] = {&entry->latitude, &entry->longitude, &entry->altitude, &entry->moment};
    unsigned count = kind ? 4 : 2;
    for (unsigned i = 0; i < count; ++i) {
        char *end = NULL;
        *fields[i] = strtod(key, &end);
        if (end == key || !isfinite(*fields[i]) ||
            (i + 1 < count ? *end != (kind ? '|' : ',') : *end != 0)) return false;
        key = end + (i + 1 < count);
    }
    return fabs(entry->latitude) <= 90 && fabs(entry->longitude) <= 180;
}

static bool weather_bind_key(sqlite_stmt_t *statement, const weather_entry_t *entry)
{
    return sqlite_stmt_bind_int(statement, 1, entry->kind) &&
           sqlite_stmt_bind_double(statement, 2, entry->latitude) &&
           sqlite_stmt_bind_double(statement, 3, entry->longitude) &&
           sqlite_stmt_bind_double(statement, 4, entry->altitude);
}

/* Native columns avoid serialisation on normal writes; legacy JSON is parsed only during migration. */
static bool weather_write(sqlite_t *db, unsigned kind, const json_t *record, bool importing)
{
    weather_entry_t entry = {0};
    double retrieved, temperature, pressure = 0, sea_level = 0;
    const char *source = kind ? lab_cal_text(record, "weather_source") : "WeatherAPI observation";
    const char *note = kind ? lab_cal_text(record, "atmosphere_note") : "";
    if (!weather_key(&entry, lab_cal_text(record, kind ? "key" : "location"), kind) ||
        !lab_cal_number(lab_cal_text(record, "retrieved"), &retrieved) || !isfinite(retrieved) ||
        !lab_cal_number(lab_cal_text(record, kind ? "temperature_c" : "temp_c"), &temperature) ||
        !isfinite(temperature) || temperature < -100 || temperature > 60 ||
        strlen(source) > 256 || strlen(note) > 1024 ||
        (kind && (strncmp(source, "WeatherAPI", 10) ||
                  !lab_cal_number(lab_cal_text(record, "pressure_hpa"), &pressure) ||
                  !isfinite(pressure) || pressure <= 0 || pressure > 1200)) ||
        (!kind && (!lab_cal_number(lab_cal_text(record, "observed"), &entry.moment) ||
                   !isfinite(entry.moment)))) return true;
    bool has_sea_level = kind && lab_cal_number(lab_cal_text(record, "pressure_msl_hpa"), &sea_level) &&
                         isfinite(sea_level) && sea_level > 0 && sea_level <= 1200;
    sqlite_stmt_t *statement = sqlite_stmt_prepare(db, importing ?
        "INSERT OR IGNORE INTO weather_records (\n"
        "    kind, latitude, longitude, altitude_m, valid_at, retrieved_at, expires_at,\n"
        "    temperature_c, observer_pressure_hpa, sea_level_pressure_hpa, source, note,\n"
        "    unit_x, unit_y, unit_z\n"
        ") VALUES (\n"
        "    ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15\n"
        ")" :
        "INSERT OR REPLACE INTO weather_records (\n"
        "    kind, latitude, longitude, altitude_m, valid_at, retrieved_at, expires_at,\n"
        "    temperature_c, observer_pressure_hpa, sea_level_pressure_hpa, source, note,\n"
        "    unit_x, unit_y, unit_z\n"
        ") VALUES (\n"
        "    ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15\n"
        ")");
    double latitude = entry.latitude * M_PI / 180, longitude = entry.longitude * M_PI / 180;
    bool ok = statement && weather_bind_key(statement, &entry) &&
        sqlite_stmt_bind_double(statement, 5, entry.moment) &&
        sqlite_stmt_bind_double(statement, 6, retrieved) &&
        sqlite_stmt_bind_double(statement, 7, kind ? retrieved + 900 : fmin(retrieved + 172800, entry.moment + 3600)) &&
        sqlite_stmt_bind_double(statement, 8, temperature) &&
        (kind ? sqlite_stmt_bind_double(statement, 9, pressure) : sqlite_stmt_bind_null(statement, 9)) &&
        (has_sea_level ? sqlite_stmt_bind_double(statement, 10, sea_level) : sqlite_stmt_bind_null(statement, 10)) &&
        sqlite_stmt_bind_text(statement, 11, source) && sqlite_stmt_bind_text(statement, 12, note) &&
        sqlite_stmt_bind_double(statement, 13, cos(latitude) * cos(longitude)) &&
        sqlite_stmt_bind_double(statement, 14, cos(latitude) * sin(longitude)) &&
        sqlite_stmt_bind_double(statement, 15, sin(latitude)) &&
        sqlite_stmt_step(statement) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(statement);
    if (!ok) return false;
    /* A fixed indexed record limit keeps eviction bounded; SQLite also enforces a one-MiB page limit. */
    return sqlite_exec_cstr(db,
        "DELETE FROM weather_records\n"
        "WHERE expires_at < CAST(strftime('%s', 'now') AS INTEGER);\n"
        "\n"
        "DELETE FROM weather_records\n"
        "WHERE rowid IN (\n"
        "    SELECT rowid\n"
        "    FROM weather_records\n"
        "    ORDER BY retrieved_at DESC, rowid DESC\n"
        "    LIMIT -1 OFFSET 512\n"
        ");");
}

static bool weather_migrate_slots(sqlite_t *db)
{
    sqlite_stmt_t *table = sqlite_stmt_prepare(db,
        "SELECT 1\n"
        "FROM sqlite_master\n"
        "WHERE type = 'table' AND name = 'weather_cache'");
    bool exists = table && sqlite_stmt_step(table) == SQLITE_STEP_ROW;
    sqlite_stmt_finalize(table);
    if (!exists) return true;
    sqlite_stmt_t *query = sqlite_stmt_prepare(db, "SELECT slot,record FROM weather_cache");
    if (!query) return false;
    sqlite_step_result_t step;
    bool ok = true;
    while (ok && (step = sqlite_stmt_step(query)) == SQLITE_STEP_ROW) {
        string_t *text = string_new_with(sqlite_stmt_column_text(query, 1));
        json_t *record = text ? json_from_text(text) : NULL;
        ok = !record || weather_write(db, sqlite_stmt_column_int(query, 0) >= 64, record, true);
        json_free(record);
        string_free(text);
    }
    sqlite_stmt_finalize(query);
    return ok && step == SQLITE_STEP_DONE && sqlite_exec_cstr(db, "DROP TABLE weather_cache");
}

static json_t *weather_legacy_record(file_t *file)
{
    char bytes[2049];
    size_t count = 0;
    if (!file || !file_read(file, bytes, sizeof bytes, &count) || !count || count > 2048) return NULL;
    string_t *text = string_new();
    json_t *record = text && !string_append_bytes(text, bytes, count) ? json_from_text(text) : NULL;
    string_free(text);
    return record;
}

/* Import only the 128 known cache filenames; leave unexpected directory contents untouched. */
static bool weather_migrate(sqlite_t *db)
{
    const char *configured = getenv("MARS_HOME"), *home = getenv("HOME");
    string_t *base = configured && *configured ? string_new_with(configured) :
                     home && *home ? string_sprintf("%s/.mars", home) : NULL;
    string_t *path = base ? string_sprintf("%S/lab/weather-cache", base) : NULL;
    file_t *directory = path ? file_new(path) : NULL;
    file_t *files[128] = {0};
    bool ok = weather_migrate_slots(db);
    bool exists = directory && file_exists(directory);
    for (unsigned i = 0; ok && exists && i < 128; ++i) {
        string_t *name = string_sprintf("%S/%s%02u.json", path, i < 64 ? "" : "conditions-", i % 64);
        file_t *file = name ? file_new(name) : NULL;
        string_free(name);
        if (file && file_exists(file)) {
            ok = file_open(file, FILE_MODE_OPEN, FILE_ACCESS_READ) && file_lock(file, false, false);
            json_t *record = ok ? weather_legacy_record(file) : NULL;
            if (record && json_type(record) == JSON_OBJECT) ok = weather_write(db, i >= 64, record, true);
            json_free(record);
            files[i] = file;
        } else file_free(file);
    }
    ok = ok && sqlite_exec_cstr(db, "COMMIT");
    if (!ok) (void)sqlite_exec_cstr(db, "ROLLBACK");
    for (unsigned i = 0; i < 128; ++i) {
        if (files[i]) {
            file_close(files[i]);
            if (ok) (void)file_delete(files[i]);
            file_free(files[i]);
        }
    }
    if (ok && directory) (void)file_remove_directory(directory);
    file_free(directory);
    string_free(path);
    string_free(base);
    return ok;
}

static weather_entry_t *weather_open(const string_t *location, bool writing, const char *prefix)
{
    (void)writing;
    string_t *path = lab_state_private_path("weather_cache.db");
    file_t *file = path ? file_new(path) : NULL;
    mode_t previous = umask(0077);
    bool ready = file && file_open(file, FILE_MODE_OPEN_OR_CREATE, FILE_ACCESS_READ_WRITE) &&
                 file_close(file) && file_chmod(file, 0600);
    sqlite_t *db = ready ? sqlite_open_plain(path) : NULL;
    bool ok = db && sqlite_exec_cstr(db,
        "PRAGMA busy_timeout = 0;\n"
        "PRAGMA journal_mode = DELETE;\n"
        "PRAGMA synchronous = FULL;\n"
        "PRAGMA page_size = 4096;\n"
        "PRAGMA max_page_count = 256;\n"
        "BEGIN IMMEDIATE;\n"
        "\n"
        "CREATE TABLE IF NOT EXISTS weather_records (\n"
        "    kind                   INTEGER NOT NULL CHECK (kind IN (0, 1)),\n"
        "    latitude               REAL NOT NULL CHECK (latitude BETWEEN -90 AND 90),\n"
        "    longitude              REAL NOT NULL CHECK (longitude BETWEEN -180 AND 180),\n"
        "    altitude_m             REAL NOT NULL,\n"
        "    valid_at               REAL NOT NULL,\n"
        "    retrieved_at           REAL NOT NULL,\n"
        "    expires_at             REAL NOT NULL,\n"
        "    temperature_c          REAL NOT NULL CHECK (temperature_c BETWEEN -100 AND 60),\n"
        "    observer_pressure_hpa  REAL CHECK (observer_pressure_hpa > 0 AND observer_pressure_hpa <= 1200),\n"
        "    sea_level_pressure_hpa REAL CHECK (sea_level_pressure_hpa > 0 AND sea_level_pressure_hpa <= 1200),\n"
        "    source                 TEXT NOT NULL CHECK (length(CAST(source AS BLOB)) <= 256),\n"
        "    note                   TEXT NOT NULL CHECK (length(CAST(note AS BLOB)) <= 1024),\n"
        "    unit_x                 REAL NOT NULL,\n"
        "    unit_y                 REAL NOT NULL,\n"
        "    unit_z                 REAL NOT NULL,\n"
        "    PRIMARY KEY (kind, latitude, longitude, altitude_m, valid_at)\n"
        ");\n"
        "\n"
        "CREATE INDEX IF NOT EXISTS weather_expiry ON weather_records (expires_at);\n"
        "CREATE INDEX IF NOT EXISTS weather_retrieval ON weather_records (retrieved_at);\n"
        "\n"
        "CREATE VIEW IF NOT EXISTS weather_readable AS\n"
        "SELECT *,\n"
        "    datetime(valid_at, 'unixepoch')     AS valid_utc,\n"
        "    datetime(retrieved_at, 'unixepoch') AS retrieved_utc,\n"
        "    datetime(expires_at, 'unixepoch')   AS expires_utc\n"
        "FROM weather_records;") && weather_migrate(db);
    weather_entry_t *entry = ok ? calloc(1, sizeof *entry) : NULL;
    if (entry) {
        entry->db = db;
        entry->key = string_new_with(string_c_str(location));
        if (!entry->key || !weather_key(entry, string_c_str(location), *prefix != 0) ||
            !sqlite_exec_cstr(db, "BEGIN IMMEDIATE")) { string_free(entry->key); free(entry); entry = NULL; }
    }
    if (!entry) sqlite_close(db);
    umask(previous);
    file_free(file);
    string_free(path);
    return entry;
}

static void weather_close(weather_entry_t *entry)
{
    if (!entry) return;
    if (!sqlite_exec_cstr(entry->db, "COMMIT")) (void)sqlite_exec_cstr(entry->db, "ROLLBACK");
    sqlite_close(entry->db);
    string_free(entry->key);
    free(entry);
}

static json_t *weather_record(weather_entry_t *entry, double selected, time_t now)
{
    if (!entry) return NULL;
    /* ?1–?4 identify the location; ?5 is selected UTC, ?6 is now, ?7–?9 are its unit vector,
       and ?10 is the cosine of the maximum angular separation. Rank in SQL, returning one row. */
    sqlite_stmt_t *query = sqlite_stmt_prepare(entry->db, entry->kind ?
        "SELECT valid_at, retrieved_at, temperature_c, observer_pressure_hpa,\n"
        "       sea_level_pressure_hpa, source, note\n"
        "FROM weather_records\n"
        "WHERE kind = ?1\n"
        "  AND latitude = ?2\n"
        "  AND longitude = ?3\n"
        "  AND altitude_m = ?4\n"
        "  AND valid_at = ?5" :
        "SELECT valid_at, retrieved_at, temperature_c, observer_pressure_hpa,\n"
        "       sea_level_pressure_hpa, source, note,\n"
        "       unit_x * ?7 + unit_y * ?8 + unit_z * ?9 AS proximity,\n"
        "       latitude, longitude\n"
        "FROM weather_records\n"
        "WHERE kind = ?1\n"
        "  AND latitude BETWEEN ?2 - 5 AND ?2 + 5\n"
        "  AND altitude_m = ?4\n"
        "  AND unit_x * ?7 + unit_y * ?8 + unit_z * ?9 >= ?10\n"
        "  AND valid_at BETWEEN ?5 - 3600 AND ?5 + 3600\n"
        "  AND valid_at BETWEEN ?6 - 3600 AND ?6\n"
        "  AND retrieved_at <= ?6\n"
        "  AND expires_at >= ?6\n"
        "ORDER BY\n"
        "    (latitude = ?2 AND longitude = ?3) DESC,\n"
        "    (1 - (unit_x * ?7 + unit_y * ?8 + unit_z * ?9)) / (1 - ?10)\n"
        "        + abs(valid_at - ?5) / 3600.0,\n"
        "    retrieved_at DESC\n"
        "LIMIT 1");
    double latitude = entry->latitude * M_PI / 180, longitude = entry->longitude * M_PI / 180;
    bool found = query && weather_bind_key(query, entry) &&
        sqlite_stmt_bind_double(query, 5, entry->kind ? entry->moment : selected) &&
        (entry->kind || (sqlite_stmt_bind_int64(query, 6, now) &&
                       sqlite_stmt_bind_double(query, 7, cos(latitude) * cos(longitude)) &&
                       sqlite_stmt_bind_double(query, 8, cos(latitude) * sin(longitude)) &&
                       sqlite_stmt_bind_double(query, 9, sin(latitude)) &&
                       sqlite_stmt_bind_double(query, 10, cos(500.0 / 6371.0088)))) &&
        sqlite_stmt_step(query) == SQLITE_STEP_ROW;
    json_t *record = found ? json_new_object() : NULL;
    if (record) {
        lab_cal_set(record, entry->kind ? "key" : "location", string_c_str(entry->key));
        if (!entry->kind) {
            double proximity = sqlite_stmt_column_double(query, 7);
            string_t *distance = string_sprintf("%.3f", 6371.0088 * acos(fmax(-1, fmin(1, proximity))));
            string_t *origin = string_sprintf("%.3f,%.3f", sqlite_stmt_column_double(query, 8),
                                               sqlite_stmt_column_double(query, 9));
            lab_cal_set(record, "distance_km", string_c_str(distance));
            lab_cal_set(record, "location", string_c_str(origin));
            string_free(distance);
            string_free(origin);
        }
        const char *names[] = {"observed", "retrieved", entry->kind ? "temperature_c" : "temp_c",
                              "pressure_hpa", "pressure_msl_hpa", "weather_source", "atmosphere_note"};
        for (unsigned i = 0; i < 7; ++i) {
            if (i < 5) {
                const char *value = sqlite_stmt_column_text(query, i);
                string_t *number = value ? string_sprintf(i < 2 ? "%.17g" : "%.15g",
                                                           sqlite_stmt_column_double(query, i)) : NULL;
                if (number) lab_cal_set(record, names[i], string_c_str(number));
                string_free(number);
            } else lab_cal_set(record, names[i], sqlite_stmt_column_text(query, i));
        }
    }
    sqlite_stmt_finalize(query);
    return record;
}

/* Cache real current observations from either calendar mode; forecasts and estimates do not refresh age. */
void lab_cal_weather_remember(const json_t *options, const json_t *weather, time_t now)
{
    const json_t *current = lab_cal_get(weather, "current");
    double observed, temperature;
    if (!lab_cal_number(lab_cal_text(current, "last_updated_epoch"), &observed) ||
        observed > now || now - observed >= 172800 ||
        !lab_cal_number(lab_cal_text(current, "temp_c"), &temperature) || temperature < -100 || temperature > 60) return;
    string_t *location = weather_location(options);
    weather_entry_t *entry = location ? weather_open(location, true, "") : NULL;
    if (entry) {
        json_t *record = json_new_object();
        string_t *retrieved = string_sprintf("%lld", (long long)now);
        lab_cal_set(record, "location", string_c_str(location));
        lab_cal_set(record, "retrieved", string_c_str(retrieved));
        lab_cal_set(record, "observed", lab_cal_text(current, "last_updated_epoch"));
        lab_cal_set(record, "temp_c", lab_cal_text(current, "temp_c"));
        (void)weather_write(entry->db, entry->kind, record, false);
        string_free(retrieved);
        json_free(record);
    }
    weather_close(entry);
    string_free(location);
}

/* Reuse temperature only as an estimate; pressure keeps its independent provider/elevation policy. */
json_t *lab_cal_weather_recent(const json_t *options, time_t now)
{
    string_t *location = weather_location(options);
    weather_entry_t *entry = location ? weather_open(location, false, "") : NULL;
    json_t *record = weather_record(entry, lab_cal_weather_epoch(options), now);
    double retrieved, observed, temperature, epoch = lab_cal_weather_epoch(options);
    bool valid = record &&
                 lab_cal_number(lab_cal_text(record, "retrieved"), &retrieved) && retrieved <= now && now - retrieved < 172800 &&
                 lab_cal_number(lab_cal_text(record, "observed"), &observed) && observed <= now && now - observed < 172800 &&
                 isfinite(epoch) && fabs(epoch - observed) <= 3600 && now - observed <= 3600 &&
                 lab_cal_number(lab_cal_text(record, "temp_c"), &temperature) && temperature >= -100 && temperature <= 60;
    if (!valid) { json_free(record); record = NULL; }
    weather_close(entry);
    string_free(location);
    return record;
}

static string_t *weather_conditions_key(const json_t *options)
{
    double latitude, longitude, elevation, epoch = lab_cal_weather_epoch(options);
    if (!lab_cal_number(lab_cal_text(options, "lat"), &latitude) || fabs(latitude) > 90 ||
        !lab_cal_number(lab_cal_text(options, "lon"), &longitude) || fabs(longitude) > 180 ||
        !lab_cal_number(lab_cal_text(options, "elevation"), &elevation) || !isfinite(epoch)) return NULL;
    /* Version the pressure interpretation so former sea-level values cannot serve as observer pressure. */
    return string_sprintf("observer-pressure-v4|%.17g|%.17g|%.17g|%.17g", latitude, longitude, elevation, epoch);
}

static bool weather_conditions_valid(const json_t *record)
{
    double temperature, pressure;
    return !strncmp(lab_cal_text(record, "weather_source"), "WeatherAPI", 10) &&
           lab_cal_number(lab_cal_text(record, "temperature_c"), &temperature) && temperature >= -100 && temperature <= 60 &&
           lab_cal_number(lab_cal_text(record, "pressure_hpa"), &pressure) && pressure > 0 && pressure <= 1200;
}

static void weather_conditions_copy(json_t *to, const json_t *from)
{
    static const char *const keys[] = {"temperature_c", "pressure_hpa", "pressure_msl_hpa",
                                       "atmosphere_note", "weather_source"};
    for (unsigned i = 0; i < sizeof keys / sizeof *keys; ++i)
        lab_cal_set(to, keys[i], lab_cal_text(from, keys[i]));
}

/* Save only provider-derived conditions; failed refreshes must not replace them with estimates. */
void lab_cal_weather_conditions_remember(const json_t *options, time_t now)
{
    if (!weather_conditions_valid(options)) return;
    string_t *key = weather_conditions_key(options);
    weather_entry_t *entry = key ? weather_open(key, true, "conditions-") : NULL;
    if (entry) {
        json_t *record = json_new_object();
        string_t *retrieved = string_sprintf("%lld", (long long)now);
        lab_cal_set(record, "key", string_c_str(key));
        lab_cal_set(record, "retrieved", string_c_str(retrieved));
        weather_conditions_copy(record, options);
        (void)weather_write(entry->db, entry->kind, record, false);
        string_free(retrieved);
        json_free(record);
    }
    weather_close(entry);
    string_free(key);
}

/* An exact, fresh match can serve the initial calculation without waiting for network access. */
bool lab_cal_weather_conditions_restore(json_t *options, time_t now)
{
    string_t *key = weather_conditions_key(options);
    weather_entry_t *entry = key ? weather_open(key, false, "conditions-") : NULL;
    json_t *record = weather_record(entry, lab_cal_weather_epoch(options), now);
    double retrieved;
    bool valid = key && record && !strcmp(lab_cal_text(record, "key"), string_c_str(key)) &&
                 lab_cal_number(lab_cal_text(record, "retrieved"), &retrieved) && retrieved <= now && now - retrieved < 900 &&
                 weather_conditions_valid(record);
    if (valid) weather_conditions_copy(options, record);
    json_free(record);
    weather_close(entry);
    string_free(key);
    return valid;
}
