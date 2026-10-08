/**
 * @file cfg_calendar_snapshot.c
 * @brief Native holiday/weekend expansion and transactional local-calendar snapshots.
 *
 * The jurisdiction engine reads the committed encrypted staging file through a
 * temporary, restored environment scope. Daily offsets use validated zoneinfo at
 * local noon. The caller's database connection and its earlier snapshot remain
 * owned by the caller; failed expansion or view validation cannot publish a partial
 * calendar. Calls must be serialised because environment and TZ are process-wide.
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "cfg_calendar_internal.h"
#include "datetime.h"
#include "jurisdiction.h"

typedef struct {
    sqlite_stmt_t *label;
    sqlite_stmt_t *event;
    bool failed;
} cfg_calendar_collector_t;

static bool cfg_calendar_collect(const holiday_event_t *event, void *context)
{
    cfg_calendar_collector_t *collector = context;
    if (!event || !event->holiday_date) {
        collector->failed = true;
        return false;
    }
    const char *source = event->holiday_name ? event->holiday_name : "Holiday";
    sqlite_stmt_reset(collector->label);
    bool ok = sqlite_stmt_bind_int(collector->label, 1, event->holiday_id) &&
              sqlite_stmt_bind_text(collector->label, 2, source);
    sqlite_step_result_t step = ok ? sqlite_stmt_step(collector->label) : SQLITE_STEP_ERROR;
    const char *name = step == SQLITE_STEP_ROW ? sqlite_stmt_column_text(collector->label, 0) : source;
    string_t *date = string_sprintf("%04d-%02d-%02d", datetime_year(event->holiday_date),
                                    datetime_month(event->holiday_date), datetime_day(event->holiday_date));
    sqlite_stmt_reset(collector->event);
    ok = ok && step != SQLITE_STEP_ERROR && date && name &&
         sqlite_stmt_bind_text(collector->event, 1, string_c_str(date)) &&
         sqlite_stmt_bind_text(collector->event, 2, name) && sqlite_stmt_step(collector->event) == SQLITE_STEP_DONE;
    string_free(date);
    collector->failed = !ok;
    return ok;
}

/* Expand an explicit range; installation uses 2015 onwards and regressions also cover historical rules. */
bool cfg_calendar_policy(sqlite_t *db, const string_t *path, const string_t *key, const cfg_calendar_place_t *place,
                         int first_year, int last_year, bool *weekends)
{
    if (!db || !place || !weekends || first_year < 1 || last_year < first_year || last_year >= SHRT_MAX ||
        !sqlite_exec_cstr(db, "DROP TABLE IF EXISTS temp.cfg_calendar_events;"
                              "CREATE TEMP TABLE cfg_calendar_events(date TEXT,name TEXT,PRIMARY KEY(date,name));"))
        return false;
    cfg_calendar_environment_t path_scope = {0}, key_scope = {0};
    bool ok = cfg_calendar_environment(&path_scope, "MARS_JURISDICTION_DB_PATH", path) &&
              cfg_calendar_environment(&key_scope, "MARS_JURISDICTION_DB_KEY", key);
    jurisdiction_t *engine = ok ? jurisdict_open(string_c_str(place->jurisdiction)) : NULL;
    datetime_t *first = datetime_alloc(), *last = datetime_alloc(), *day = datetime_alloc();
    cfg_calendar_collector_t collector = {0};
    if (ok && engine && first && last && day && datetime_init_ymd(first, (short)first_year, 1, 1) &&
        datetime_init_ymd(last, (short)last_year, 12, 31)) {
        collector.label =
            sqlite_stmt_prepare(db, "SELECT translated FROM cfg_calendar_labels WHERE holiday=?1 AND source=?2");
        collector.event = sqlite_stmt_prepare(db, "INSERT OR IGNORE INTO cfg_calendar_events VALUES(?1,?2)");
        ok = collector.label && collector.event &&
             jurisdict_each_holiday_between(engine, first, last, cfg_calendar_collect, &collector) && !collector.failed;
        const char *error = jurisdict_last_error(engine);
        ok = ok && (!error || !*error);
        for (int year = first_year; year <= last_year && ok; ++year) {
            for (int index = 1; index <= 7 && ok; ++index) {
                ok = datetime_init_ymd(day, (short)year, 1, (uint8_t)index) != NULL;
                if (ok) {
                    bool weekend = jurisdict_is_weekend(engine, day);
                    error = jurisdict_last_error(engine);
                    ok = !error || !*error;
                    weekends[(year - first_year) * 7 + (datetime_weekday(day) + 5) % 7] = weekend;
                }
            }
        }
    } else
        ok = false;
    if (!ok)
        fputs("Cannot expand the native calendar holiday/weekend policy.\n", stderr);
    sqlite_stmt_finalize(collector.event);
    sqlite_stmt_finalize(collector.label);
    datetime_dealloc(day);
    datetime_dealloc(last);
    datetime_dealloc(first);
    jurisdict_close(engine);
    bool key_restored = cfg_calendar_restore(&key_scope);
    bool path_restored = cfg_calendar_restore(&path_scope);
    if (!key_restored || !path_restored)
        fputs("Cannot restore the calendar database environment.\n", stderr);
    return ok && key_restored && path_restored;
}

static bool cfg_calendar_settings(sqlite_t *db, const cfg_calendar_place_t *place, const string_t *locale,
                                  int last_year)
{
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(
        db, "INSERT INTO calendar_local_settings "
            "(singleton,location,jurisdiction,timezone,latitude,longitude,first_year,last_year,locale) "
            "VALUES(1,?1,?2,?3,?4,?5,2015,?6,?7)");
    bool ok = stmt && sqlite_stmt_bind_text(stmt, 1, string_c_str(place->name)) &&
              sqlite_stmt_bind_text(stmt, 2, string_c_str(place->jurisdiction)) &&
              sqlite_stmt_bind_text(stmt, 3, string_c_str(place->timezone)) &&
              sqlite_stmt_bind_double(stmt, 4, place->latitude) && sqlite_stmt_bind_double(stmt, 5, place->longitude) &&
              sqlite_stmt_bind_int(stmt, 6, last_year) &&
              (locale && string_byte_length(locale) ? sqlite_stmt_bind_text(stmt, 7, string_c_str(locale))
                                                    : sqlite_stmt_bind_null(stmt, 7)) &&
              sqlite_stmt_step(stmt) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(stmt);
    return ok;
}

static bool cfg_calendar_days(sqlite_t *db, int last_year, const bool *weekends, long *expected)
{
    datetime_t *day = datetime_alloc();
    long first = datetime_ymd_to_jdn(2015, 1, 1), last = datetime_ymd_to_jdn((short)last_year, 12, 31);
    *expected = last - first + 1;
    sqlite_stmt_t *write = sqlite_stmt_prepare(
        db, "INSERT INTO calendar_local_days(calendar_date,holiday_name,is_weekend,utc_offset_hours) "
            "VALUES(?1,?2,?3,?4)");
    sqlite_stmt_t *read = sqlite_stmt_prepare(db, "SELECT name FROM cfg_calendar_events WHERE date=?1 ORDER BY name");
    bool ok = day && write && read;
    for (long jdn = first; jdn <= last && ok; ++jdn) {
        ok = datetime_init_jdn(day, jdn) != NULL;
        string_t *date =
            ok ? string_sprintf("%04d-%02d-%02d", datetime_year(day), datetime_month(day), datetime_day(day)) : NULL;
        string_t *names = string_new();
        sqlite_stmt_reset(read);
        ok = ok && date && names && sqlite_stmt_bind_text(read, 1, string_c_str(date));
        sqlite_step_result_t step = SQLITE_STEP_DONE;
        bool any = false;
        while (ok && (step = sqlite_stmt_step(read)) == SQLITE_STEP_ROW) {
            if (any)
                ok = !string_append_cstr(names, "; ");
            ok = ok && !string_append_cstr(names, sqlite_stmt_column_text(read, 0));
            any = true;
        }
        double offset = 0;
        ok = ok && step == SQLITE_STEP_DONE &&
             cfg_calendar_noon(datetime_year(day), datetime_month(day), datetime_day(day), &offset);
        sqlite_stmt_reset(write);
        ok = ok && sqlite_stmt_bind_text(write, 1, string_c_str(date)) &&
             (any ? sqlite_stmt_bind_text(write, 2, string_c_str(names)) : sqlite_stmt_bind_null(write, 2)) &&
             sqlite_stmt_bind_int(write, 3,
                                  weekends[(datetime_year(day) - 2015) * 7 + (datetime_weekday(day) + 5) % 7]) &&
             sqlite_stmt_bind_double(write, 4, offset) && sqlite_stmt_step(write) == SQLITE_STEP_DONE;
        string_free(names);
        string_free(date);
    }
    sqlite_stmt_finalize(read);
    sqlite_stmt_finalize(write);
    datetime_dealloc(day);
    return ok;
}

static bool cfg_calendar_validate(sqlite_t *db, long expected)
{
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(
        db, "SELECT count(*),count(DISTINCT FullDateAlternateKey),count([Date UK]),count([Date Lingua]),"
            "count([Date Regional]) FROM calendar_local");
    bool ok = stmt && sqlite_stmt_step(stmt) == SQLITE_STEP_ROW;
    for (int column = 0; column < 5 && ok; ++column)
        ok = sqlite_stmt_column_int64(stmt, column) == expected;
    ok = ok && sqlite_stmt_step(stmt) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(stmt);
    return ok;
}

/* Expand first, then replace under a savepoint and validate every date rendering. */
bool cfg_calendar_snapshot(sqlite_t *db, const string_t *path, const string_t *key, const cfg_calendar_place_t *place,
                           const string_t *locale)
{
    cfg_calendar_environment_t timezone = {0};
    bool ok = cfg_calendar_zone_enter(place->timezone, &timezone);
    time_t now = time(NULL);
    struct tm local;
    ok = ok && now != (time_t)-1 && localtime_r(&now, &local);
    int last_year = ok ? local.tm_year + 1900 + 7 : 0;
    ok = ok && last_year >= 2015 && last_year < SHRT_MAX;
    bool *weekends = ok ? calloc((size_t)(last_year - 2015 + 1) * 7, sizeof(bool)) : NULL;
    ok = ok && weekends && cfg_calendar_labels(db, place->jurisdiction, locale) &&
         cfg_calendar_policy(db, path, key, place, 2015, last_year, weekends);
    bool transaction = ok && sqlite_exec_cstr(db, "SAVEPOINT cfg_calendar_snapshot");
    ok = ok && transaction &&
         sqlite_exec_cstr(db, "DELETE FROM calendar_local_days; DELETE FROM calendar_local_settings;") &&
         cfg_calendar_settings(db, place, locale, last_year);
    long expected = 0;
    ok = ok && cfg_calendar_days(db, last_year, weekends, &expected) && cfg_calendar_validate(db, expected);
    /* Restore process state before committing, so a restoration failure also rolls back. */
    bool restored = cfg_calendar_restore(&timezone);
    tzset();
    ok = ok && restored;
    if (transaction) {
        if (ok)
            ok = sqlite_exec_cstr(db, "RELEASE cfg_calendar_snapshot");
        if (!ok && !sqlite_exec_cstr(db, "ROLLBACK TO cfg_calendar_snapshot; RELEASE cfg_calendar_snapshot"))
            fputs("Cannot roll back the incomplete calendar snapshot. Discard the staging database.\n", stderr);
    }
    if (!ok)
        fprintf(stderr, "Incomplete calendar for %s, %s; check native policy, timezone and locale mappings.\n",
                string_c_str(place->name), string_c_str(place->jurisdiction));
    free(weekends);
    return ok;
}
