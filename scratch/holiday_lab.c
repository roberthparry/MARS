#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlcipher/sqlite3.h>

#include "datetime.h"
#include "ustring.h"

#define HOLIDAY_DB_PATH_ENV "MARS_HOLIDAY_DB_PATH"
#define HOLIDAY_DB_KEY_ENV "MARS_HOLIDAY_DB_KEY"

typedef struct holiday_lab_options_t {
    short start_year;
    month_t start_month;
    uint8_t start_day;
    short end_year;
    month_t end_month;
    uint8_t end_day;
    char jurisdiction[32];
} holiday_lab_options_t;

static bool parse_date_text(const char *text, short *year, month_t *month, uint8_t *day)
{
    int y;
    int m;
    int d;
    char tail;

    if (!text || !year || !month || !day)
        return false;
    if (sscanf(text, "%d-%d-%d%c", &y, &m, &d, &tail) != 3)
        return false;
    if (y < 1 || y > 9999 || m < 1 || m > 12 || d < 1 || d > 31)
        return false;
    if (!datetime_valid_ymd((short)y, (month_t)m, (uint8_t)d))
        return false;

    *year = (short)y;
    *month = (month_t)m;
    *day = (uint8_t)d;
    return true;
}

static bool key_equals(const char *got, size_t got_len, const char *want)
{
    return strlen(want) == got_len && strncmp(got, want, got_len) == 0;
}

static void init_defaults(holiday_lab_options_t *options)
{
    options->start_year = 2026;
    options->start_month = DT_January;
    options->start_day = 1;
    options->end_year = 2026;
    options->end_month = DT_December;
    options->end_day = 31;
    strcpy(options->jurisdiction, "GB-ENG");
}

static bool apply_arg(holiday_lab_options_t *options, const char *arg)
{
    const char *equals = strchr(arg, '=');
    const char *value;
    size_t key_len;
    short y;
    month_t m;
    uint8_t d;

    if (!equals)
        return false;

    key_len = (size_t)(equals - arg);
    value = equals + 1;

    if (key_equals(arg, key_len, "start")) {
        if (!parse_date_text(value, &y, &m, &d))
            return false;
        options->start_year = y;
        options->start_month = m;
        options->start_day = d;
        return true;
    }
    if (key_equals(arg, key_len, "end")) {
        if (!parse_date_text(value, &y, &m, &d))
            return false;
        options->end_year = y;
        options->end_month = m;
        options->end_day = d;
        return true;
    }
    if (key_equals(arg, key_len, "jurisdiction")) {
        size_t value_len = strlen(value);

        if (value_len == 0 || value_len >= sizeof(options->jurisdiction))
            return false;
        memcpy(options->jurisdiction, value, value_len + 1u);
        return true;
    }

    return false;
}

static const char *holiday_db_path(void)
{
    const char *path = getenv(HOLIDAY_DB_PATH_ENV);

    return (path && *path) ? path : NULL;
}

static const char *holiday_db_key(void)
{
    const char *key = getenv(HOLIDAY_DB_KEY_ENV);

    return (key && *key) ? key : NULL;
}

static char *format_date(const datetime_t *dttm)
{
    string_t *format = string_new_with("%yyyy-%mm-%dd");
    string_t *text = format ? datetime_format_text(dttm, format) : NULL;
    const char *c_text = text ? string_c_str(text) : NULL;
    char *out = NULL;

    if (c_text) {
        size_t len = strlen(c_text);
        out = malloc(len + 1u);
        if (out)
            memcpy(out, c_text, len + 1u);
    }

    string_free(text);
    string_free(format);
    return out;
}

static bool print_bank_holidays_from_database(const char *jurisdiction,
                                              const datetime_t *start,
                                              const datetime_t *end)
{
    static const char sql[] =
        "SELECT holiday_name, holiday_date "
        "FROM holiday_instance "
        "WHERE jurisdiction_id = ?1 "
        "  AND holiday_date >= ?2 "
        "  AND holiday_date <= ?3 "
        "ORDER BY holiday_date, holiday_name;";
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    char *start_text = NULL;
    char *end_text = NULL;
    const char *db_path;
    const char *db_key;
    bool found_rows = false;
    int rc;

    if (!jurisdiction || *jurisdiction == '\0' || !start || !end)
        return false;

    db_path = holiday_db_path();
    db_key = holiday_db_key();
    if (!db_path)
        return false;

    start_text = format_date(start);
    end_text = format_date(end);
    if (!start_text || !end_text)
        goto fail;

    rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK)
        goto fail;

    if (db_key) {
        rc = sqlite3_key(db, db_key, (int)strlen(db_key));
        if (rc != SQLITE_OK)
            goto fail;
    }

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        goto fail;

    if (sqlite3_bind_text(stmt, 1, jurisdiction, -1, SQLITE_STATIC) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, start_text, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 3, end_text, -1, SQLITE_TRANSIENT) != SQLITE_OK)
        goto fail;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char *holiday_name = sqlite3_column_text(stmt, 0);
        const unsigned char *holiday_date = sqlite3_column_text(stmt, 1);

        found_rows = true;
        printf("bank_holiday %s: %s\n",
               holiday_name ? (const char *)holiday_name : "unavailable",
               holiday_date ? (const char *)holiday_date : "unavailable");
    }
    if (rc != SQLITE_DONE)
        goto fail;

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    free(start_text);
    free(end_text);
    return found_rows;

fail:
    if (stmt)
        sqlite3_finalize(stmt);
    if (db)
        sqlite3_close(db);
    free(start_text);
    free(end_text);
    return false;
}

int main(int argc, char **argv)
{
    holiday_lab_options_t options;
    datetime_t *start;
    datetime_t *end;
    bool produced = false;

    init_defaults(&options);
    for (int i = 1; i < argc; i++) {
        if (!apply_arg(&options, argv[i])) {
            fprintf(stderr, "Bad holiday argument: %s\n", argv[i]);
            return 2;
        }
    }

    start = datetime_alloc();
    end = datetime_alloc();
    if (!start || !end) {
        fprintf(stderr, "Holiday allocation failed\n");
        datetime_dealloc(start);
        datetime_dealloc(end);
        return 1;
    }

    datetime_init_ymd(start, options.start_year, options.start_month, options.start_day);
    datetime_init_ymd(end, options.end_year, options.end_month, options.end_day);

    produced = print_bank_holidays_from_database(options.jurisdiction, start, end);

    printf("holiday_status %s\n", produced ? "ok" : "unavailable");

    datetime_dealloc(start);
    datetime_dealloc(end);
    return 0;
}
