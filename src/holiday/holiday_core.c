#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "array.h"
#include "holiday.h"
#include "sqlite/sqlite_internal.h"
#include "sqlite.h"
#include "ustring.h"

typedef struct _holiday_t {
    sqlite_t *db;
    string_t *error;
    char jurisdiction[32];
} holiday_t;

typedef struct lineage_row_t {
    char jurisdiction_id[32];
    int depth;
} lineage_row_t;

typedef struct weekend_rule_t {
    char jurisdiction_id[32];
    int depth;
    char *weekend_mask;
    int valid_from_year;
    int valid_to_year;
} weekend_rule_t;

typedef struct holiday_rule_row_t {
    int holiday_id;
    int rule_id;
    char jurisdiction_id[32];
    char *holiday_name;
    char *holiday_class;
    char *rule_kind;
    int month;
    int day;
    int weekday;
    int ordinal;
    int offset_days;
    char *holiday_date;
    char *expression_language;
    char *expression_text;
    int valid_from_year;
    int valid_to_year;
} holiday_rule_row_t;

typedef struct observance_rule_row_t {
    int holiday_id;
    int applies_to_rule_id;
    char *observed_rule_kind;
    char *observed_name;
    char *weekend_mask;
    int suppress_original;
    int valid_from_year;
    int valid_to_year;
} observance_rule_row_t;

typedef struct holiday_exception_row_t {
    int holiday_id;
    int target_rule_id;
    char *holiday_date;
    char *action;
    char *name;
    int valid_from_year;
    int valid_to_year;
} holiday_exception_row_t;

typedef struct holiday_event_row_t {
    int holiday_id;
    int rule_id;
    int event_year;
    char *holiday_date;
    char *holiday_name;
    char *holiday_class;
    bool removed;
    bool derived_from_observance;
} holiday_event_row_t;

typedef struct pointer_vec_t {
    void *items;
    size_t count;
    size_t capacity;
    size_t item_size;
} pointer_vec_t;

static void holiday_set_error(holiday_t *holiday, const char *message)
{
    if (!holiday || !holiday->error)
        return;
    string_clear(holiday->error);
    (void)string_append_cstr(holiday->error, message ? message : "holiday error");
}

static string_t *string_new_from_cstr(const char *text)
{
    if (!text || *text == '\0')
        return NULL;
    return string_new_with(text);
}

static char *dup_c_string(const char *text)
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

static char *mars_home_path(void)
{
    const char *mars_home = getenv("MARS_HOME");
    const char *home = getenv("HOME");

    if (mars_home && *mars_home)
        return dup_c_string(mars_home);
    if (home && *home)
        return dup_printf_path3(home, "/.mars", "");
    return NULL;
}

static char *holiday_config_path(void)
{
    char *mars_home = mars_home_path();
    char *config_path;

    if (!mars_home)
        return NULL;
    config_path = dup_printf_path3(mars_home, "/config/", "holiday-db.env");
    free(mars_home);
    return config_path;
}

static void trim_ascii_whitespace(char *text)
{
    char *start;
    char *end;

    if (!text || *text == '\0')
        return;
    start = text;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        start++;
    if (start != text)
        memmove(text, start, strlen(start) + 1u);
    end = text + strlen(text);
    while (end > text &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }
    *end = '\0';
}

static char *unquote_shell_value(const char *raw_value)
{
    char *value;
    size_t len;

    if (!raw_value)
        return NULL;
    value = dup_c_string(raw_value);
    if (!value)
        return NULL;
    trim_ascii_whitespace(value);
    len = strlen(value);
    if (len >= 2u &&
        ((value[0] == '\'' && value[len - 1u] == '\'') ||
         (value[0] == '"' && value[len - 1u] == '"'))) {
        memmove(value, value + 1, len - 2u);
        value[len - 2u] = '\0';
    }
    return value;
}

static char *holiday_config_lookup(const char *name)
{
    char *config_path = holiday_config_path();
    FILE *file = NULL;
    char line[4096];
    char *result = NULL;
    size_t name_len;

    if (!config_path || !name)
        goto done;
    file = fopen(config_path, "r");
    if (!file)
        goto done;

    name_len = strlen(name);
    while (fgets(line, sizeof(line), file)) {
        char *cursor = line;
        char *equals;
        char *value;

        trim_ascii_whitespace(cursor);
        if (strncmp(cursor, "export ", 7) == 0)
            cursor += 7;
        if (strncmp(cursor, name, name_len) != 0)
            continue;
        equals = strchr(cursor, '=');
        if (!equals)
            continue;
        if ((size_t)(equals - cursor) != name_len)
            continue;
        value = unquote_shell_value(equals + 1);
        if (value && *value) {
            result = value;
            break;
        }
        free(value);
    }

done:
    if (file)
        fclose(file);
    free(config_path);
    return result;
}

static char *holiday_db_path_from_env(void)
{
    const char *path_env = getenv("MARS_HOLIDAY_DB_PATH");
    char *configured_path;
    char *mars_home;

    if (path_env && *path_env)
        return dup_c_string(path_env);
    configured_path = holiday_config_lookup("MARS_HOLIDAY_DB_PATH");
    if (configured_path)
        return configured_path;
    mars_home = mars_home_path();
    if (mars_home) {
        char *default_path = dup_printf_path3(mars_home, "/holiday/", "mars_holiday_rules.db");

        free(mars_home);
        return default_path;
    }
    return NULL;
}

static const char *holiday_db_key_from_env(void)
{
    const char *env_key;
    static char *config_key;

    env_key = getenv("MARS_HOLIDAY_DB_KEY");
    if (env_key && *env_key)
        return env_key;
    if (!config_key)
        config_key = holiday_config_lookup("MARS_HOLIDAY_DB_KEY");
    return (config_key && *config_key) ? config_key : NULL;
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
    if (!locale_value || !*locale_value || strcmp(locale_value, "C") == 0 ||
        strcmp(locale_value, "POSIX") == 0)
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

static void vec_free(pointer_vec_t *vec)
{
    free(vec ? vec->items : NULL);
    if (vec) {
        vec->items = NULL;
        vec->count = 0u;
        vec->capacity = 0u;
    }
}

static bool vec_push(pointer_vec_t *vec, const void *item)
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

static char *dup_text(const unsigned char *text)
{
    size_t len;
    char *out;

    if (!text)
        return NULL;
    len = strlen((const char *)text);
    out = malloc(len + 1u);
    if (!out)
        return NULL;
    memcpy(out, text, len + 1u);
    return out;
}

static bool year_in_range(int year, int valid_from_year, int valid_to_year)
{
    if (valid_from_year != 0 && year < valid_from_year)
        return false;
    if (valid_to_year != 0 && year > valid_to_year)
        return false;
    return true;
}

static int text_compare(const char *a, const char *b)
{
    if (!a && !b)
        return 0;
    if (!a)
        return -1;
    if (!b)
        return 1;
    return strcmp(a, b);
}

static int event_compare(const void *lhs, const void *rhs)
{
    const holiday_event_row_t *a = lhs;
    const holiday_event_row_t *b = rhs;
    int cmp = text_compare(a->holiday_date, b->holiday_date);

    if (cmp != 0)
        return cmp;
    return text_compare(a->holiday_name, b->holiday_name);
}

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

static void free_holiday_rule_rows(pointer_vec_t *rows)
{
    size_t i;
    holiday_rule_row_t *items = rows ? rows->items : NULL;

    if (!items)
        return;
    for (i = 0; i < rows->count; ++i) {
        free(items[i].holiday_name);
        free(items[i].holiday_class);
        free(items[i].rule_kind);
        free(items[i].holiday_date);
        free(items[i].expression_language);
        free(items[i].expression_text);
    }
    vec_free(rows);
}

static void free_observance_rule_rows(pointer_vec_t *rows)
{
    size_t i;
    observance_rule_row_t *items = rows ? rows->items : NULL;

    if (!items)
        return;
    for (i = 0; i < rows->count; ++i) {
        free(items[i].observed_rule_kind);
        free(items[i].observed_name);
        free(items[i].weekend_mask);
    }
    vec_free(rows);
}

static void free_exception_rows(pointer_vec_t *rows)
{
    size_t i;
    holiday_exception_row_t *items = rows ? rows->items : NULL;

    if (!items)
        return;
    for (i = 0; i < rows->count; ++i) {
        free(items[i].holiday_date);
        free(items[i].action);
        free(items[i].name);
    }
    vec_free(rows);
}

static void free_weekend_rules(pointer_vec_t *rows)
{
    size_t i;
    weekend_rule_t *items = rows ? rows->items : NULL;

    if (!items)
        return;
    for (i = 0; i < rows->count; ++i)
        free(items[i].weekend_mask);
    vec_free(rows);
}

static void free_events(pointer_vec_t *rows)
{
    size_t i;
    holiday_event_row_t *items = rows ? rows->items : NULL;

    if (!items)
        return;
    for (i = 0; i < rows->count; ++i) {
        free(items[i].holiday_date);
        free(items[i].holiday_name);
        free(items[i].holiday_class);
    }
    vec_free(rows);
}

static char *format_date(const datetime_t *dttm)
{
    string_t *format = string_new_with("%yyyy-%MM-%dd");
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

static char *date_text_from_datetime(const datetime_t *dttm)
{
    return format_date(dttm);
}

static int iso_weekday_from_datetime_weekday(int weekday)
{
    switch (weekday) {
    case DT_Monday: return 1;
    case DT_Tuesday: return 2;
    case DT_Wednesday: return 3;
    case DT_Thursday: return 4;
    case DT_Friday: return 5;
    case DT_Saturday: return 6;
    case DT_Sunday: return 7;
    default: return 0;
    }
}

static int iso_weekday_from_date_text(const char *text)
{
    short year;
    month_t month;
    uint8_t day;
    datetime_t *dttm;
    int weekday;

    if (!parse_date_text(text, &year, &month, &day))
        return 0;
    dttm = datetime_init_ymd(datetime_alloc(), year, month, day);
    if (!dttm)
        return 0;
    weekday = iso_weekday_from_datetime_weekday((int)datetime_weekday(dttm));
    datetime_dealloc(dttm);
    return weekday;
}

static char *date_text_add_days(const char *text, long days)
{
    short year;
    month_t month;
    uint8_t day;
    datetime_t *dttm;
    char *out;

    if (!parse_date_text(text, &year, &month, &day))
        return NULL;
    dttm = datetime_init_ymd(datetime_alloc(), year, month, day);
    if (!dttm)
        return NULL;
    datetime_add_days(dttm, days);
    out = date_text_from_datetime(dttm);
    datetime_dealloc(dttm);
    return out;
}

static char *date_text_from_ymd(short year, month_t month, uint8_t day)
{
    datetime_t *dttm = datetime_init_ymd(datetime_alloc(), year, month, day);
    char *out;

    if (!dttm)
        return NULL;
    out = date_text_from_datetime(dttm);
    datetime_dealloc(dttm);
    return out;
}

static char *easter_related_text(int year, int offset_days, bool orthodox)
{
    datetime_t *dttm = orthodox
        ? datetime_init_orthodox_easter(datetime_alloc(), year)
        : datetime_init_easter(datetime_alloc(), year);
    char *out;

    if (!dttm)
        return NULL;
    datetime_add_days(dttm, offset_days);
    out = date_text_from_datetime(dttm);
    datetime_dealloc(dttm);
    return out;
}

static char *nth_weekday_of_month_text(int year, int month, int weekday, int ordinal)
{
    datetime_t *dttm = datetime_init_ymd(datetime_alloc(), (short)year, (month_t)month, 1u);
    int current_weekday;
    int delta;

    if (!dttm || ordinal < 1)
        return NULL;
    current_weekday = iso_weekday_from_datetime_weekday((int)datetime_weekday(dttm));
    delta = weekday - current_weekday;
    if (delta < 0)
        delta += 7;
    datetime_add_days(dttm, delta + (ordinal - 1) * 7);

    if ((int)datetime_month(dttm) != month) {
        datetime_dealloc(dttm);
        return NULL;
    }

    {
        char *out = date_text_from_datetime(dttm);
        datetime_dealloc(dttm);
        return out;
    }
}

static char *last_weekday_of_month_text(int year, int month, int weekday)
{
    datetime_t *dttm;
    int current_weekday;
    int delta;

    if (month < 1 || month > 12)
        return NULL;
    if (month == 12)
        dttm = datetime_init_ymd(datetime_alloc(), (short)(year + 1), DT_January, 1u);
    else
        dttm = datetime_init_ymd(datetime_alloc(), (short)year, (month_t)(month + 1), 1u);
    if (!dttm)
        return NULL;

    datetime_add_days(dttm, -1);
    current_weekday = iso_weekday_from_datetime_weekday((int)datetime_weekday(dttm));
    delta = current_weekday - weekday;
    if (delta < 0)
        delta += 7;
    datetime_add_days(dttm, -delta);

    {
        char *out = date_text_from_datetime(dttm);
        datetime_dealloc(dttm);
        return out;
    }
}

static char *weekday_after_date_text(int year, int month, int day, int weekday)
{
    datetime_t *dttm = datetime_init_ymd(datetime_alloc(), (short)year, (month_t)month, (uint8_t)day);
    int current_weekday;
    int delta;

    if (!dttm)
        return NULL;
    current_weekday = iso_weekday_from_datetime_weekday((int)datetime_weekday(dttm));
    delta = weekday - current_weekday;
    if (delta < 0)
        delta += 7;
    datetime_add_days(dttm, delta);

    {
        char *out = date_text_from_datetime(dttm);
        datetime_dealloc(dttm);
        return out;
    }
}

static char *weekday_before_date_text(int year, int month, int day, int weekday)
{
    datetime_t *dttm = datetime_init_ymd(datetime_alloc(), (short)year, (month_t)month, (uint8_t)day);
    int current_weekday;
    int delta;

    if (!dttm)
        return NULL;
    current_weekday = iso_weekday_from_datetime_weekday((int)datetime_weekday(dttm));
    delta = current_weekday - weekday;
    if (delta < 0)
        delta += 7;
    datetime_add_days(dttm, -delta);

    {
        char *out = date_text_from_datetime(dttm);
        datetime_dealloc(dttm);
        return out;
    }
}

static char *normalise_weekend_mask(const char *mask)
{
    size_t i;
    size_t out_len = 0u;
    char *out = malloc(16u);

    if (!out)
        return NULL;
    if (!mask || !*mask) {
        strcpy(out, "6,7");
        return out;
    }
    for (i = 0u; mask[i] != '\0'; ++i) {
        if ((mask[i] >= '1' && mask[i] <= '7') || mask[i] == ',')
            out[out_len++] = mask[i];
    }
    if (out_len == 0u) {
        strcpy(out, "6,7");
        return out;
    }
    out[out_len] = '\0';
    return out;
}

static bool weekend_mask_contains(const char *mask, int weekday)
{
    char token[2];

    if (!mask || weekday < 1 || weekday > 7)
        return false;
    token[0] = (char)('0' + weekday);
    token[1] = '\0';
    return strstr(mask, token) != NULL;
}

static const char *effective_weekend_mask_for_year(const weekend_rule_t *rules, size_t count, int year)
{
    size_t i;

    for (i = 0; i < count; ++i) {
        if (year_in_range(year, rules[i].valid_from_year, rules[i].valid_to_year))
            return rules[i].weekend_mask;
    }
    return "6,7";
}

static bool date_is_occupied(const holiday_event_row_t *events, size_t count, const char *holiday_date)
{
    size_t i;

    for (i = 0; i < count; ++i) {
        if (events[i].removed || !events[i].holiday_date)
            continue;
        if (strcmp(events[i].holiday_date, holiday_date) == 0)
            return true;
    }
    return false;
}

static char *next_observed_date(const char *base_date,
                                const char *weekend_mask,
                                const holiday_event_row_t *occupied_events,
                                size_t occupied_count,
                                bool avoid_occupied,
                                bool force_monday)
{
    long delta;

    for (delta = 1; delta <= 14; ++delta) {
        char *candidate = date_text_add_days(base_date, delta);
        int weekday = candidate ? iso_weekday_from_date_text(candidate) : 0;
        bool is_weekend = weekend_mask_contains(weekend_mask, weekday);
        bool is_monday = weekday == 1;
        bool occupied = candidate && avoid_occupied
            ? date_is_occupied(occupied_events, occupied_count, candidate)
            : false;

        if (candidate && !is_weekend && (!force_monday || is_monday) && !occupied)
            return candidate;
        free(candidate);
    }
    return NULL;
}

static char *previous_observed_date(const char *base_date,
                                    const char *weekend_mask,
                                    const holiday_event_row_t *occupied_events,
                                    size_t occupied_count,
                                    bool avoid_occupied)
{
    long delta;

    for (delta = 1; delta <= 14; ++delta) {
        char *candidate = date_text_add_days(base_date, -delta);
        int weekday = candidate ? iso_weekday_from_date_text(candidate) : 0;
        bool is_weekend = weekend_mask_contains(weekend_mask, weekday);
        bool occupied = candidate && avoid_occupied
            ? date_is_occupied(occupied_events, occupied_count, candidate)
            : false;

        if (candidate && !is_weekend && !occupied)
            return candidate;
        free(candidate);
    }
    return NULL;
}

static char *evaluate_sql_rule_date(sqlite3 *db,
                                    const holiday_rule_row_t *rule,
                                    int year,
                                    const char *jurisdiction)
{
    sqlite3_stmt *stmt = NULL;
    char *sql = NULL;
    const unsigned char *date_text;
    char *out = NULL;
    int rc;
    size_t sql_len;
    static const char prefix[] =
        "WITH holiday_rule_context(rule_year, jurisdiction_id, holiday_id, rule_id) AS ("
        " SELECT ?1, ?2, ?3, ?4"
        ") ";

    if (!db || !rule || !rule->expression_language || !rule->expression_text || !jurisdiction)
        return NULL;
    if (strcmp(rule->expression_language, "mars_sql") != 0)
        return NULL;

    sql_len = strlen(prefix) + strlen(rule->expression_text) + 1u;
    sql = malloc(sql_len);
    if (!sql)
        return NULL;
    snprintf(sql, sql_len, "%s%s", prefix, rule->expression_text);

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    free(sql);
    if (rc != SQLITE_OK)
        return NULL;
    if (sqlite3_bind_int(stmt, 1, year) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, jurisdiction, -1, SQLITE_STATIC) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 3, rule->holiday_id) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 4, rule->rule_id) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return NULL;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        date_text = sqlite3_column_text(stmt, 0);
        if (date_text)
            out = dup_text(date_text);
    }
    sqlite3_finalize(stmt);
    return out;
}

static bool load_lineage(sqlite3 *db, const char *jurisdiction, pointer_vec_t *lineage_rows)
{
    static const char sql[] =
        "WITH RECURSIVE lineage(jurisdiction_id, depth) AS ("
        "  SELECT ?1, 0 "
        "  UNION ALL "
        "  SELECT j.parent_jurisdiction_id, lineage.depth + 1 "
        "  FROM jurisdiction j "
        "  JOIN lineage ON j.jurisdiction_id = lineage.jurisdiction_id "
        "  WHERE j.parent_jurisdiction_id IS NOT NULL"
        ") "
        "SELECT jurisdiction_id, depth FROM lineage ORDER BY depth;";
    sqlite3_stmt *stmt = NULL;
    int rc;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return false;
    if (sqlite3_bind_text(stmt, 1, jurisdiction, -1, SQLITE_STATIC) != SQLITE_OK)
        goto fail;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        lineage_row_t row;
        const unsigned char *jurisdiction_id = sqlite3_column_text(stmt, 0);

        memset(&row, 0, sizeof(row));
        if (!jurisdiction_id)
            goto fail;
        snprintf(row.jurisdiction_id, sizeof(row.jurisdiction_id), "%s", (const char *)jurisdiction_id);
        row.depth = sqlite3_column_int(stmt, 1);
        if (!vec_push(lineage_rows, &row))
            goto fail;
    }

    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;

fail:
    sqlite3_finalize(stmt);
    return false;
}

static bool load_weekend_rules(sqlite3 *db, const pointer_vec_t *lineage_rows, pointer_vec_t *weekend_rules)
{
    static const char sql[] =
        "SELECT jurisdiction_id, weekend_mask, valid_from_year, valid_to_year "
        "FROM jurisdiction_weekend_rule WHERE jurisdiction_id = ?1 "
        "ORDER BY COALESCE(valid_from_year, -999999), COALESCE(valid_to_year, 999999);";
    sqlite3_stmt *stmt = NULL;
    const lineage_row_t *lineage = lineage_rows ? lineage_rows->items : NULL;
    size_t i;

    if (!lineage)
        return false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return false;

    for (i = 0; i < lineage_rows->count; ++i) {
        int rc;

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        if (sqlite3_bind_text(stmt, 1, lineage[i].jurisdiction_id, -1, SQLITE_STATIC) != SQLITE_OK)
            goto fail;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            weekend_rule_t row;

            memset(&row, 0, sizeof(row));
            snprintf(row.jurisdiction_id, sizeof(row.jurisdiction_id), "%s", lineage[i].jurisdiction_id);
            row.depth = lineage[i].depth;
            row.weekend_mask = normalise_weekend_mask((const char *)sqlite3_column_text(stmt, 1));
            row.valid_from_year = sqlite3_column_type(stmt, 2) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 2);
            row.valid_to_year = sqlite3_column_type(stmt, 3) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 3);
            if (!row.weekend_mask || !vec_push(weekend_rules, &row))
                goto fail;
        }
        if (rc != SQLITE_DONE)
            goto fail;
    }

    sqlite3_finalize(stmt);
    return true;

fail:
    sqlite3_finalize(stmt);
    return false;
}

static bool load_holiday_rules(sqlite3 *db, const pointer_vec_t *lineage_rows, pointer_vec_t *rules)
{
    static const char sql[] =
        "SELECT hd.holiday_id, hr.rule_id, hd.jurisdiction_id, "
        "       COALESCE(hn.localized_name, hd.default_name), hd.holiday_class, "
        "       hr.rule_kind, hr.month, hr.day, hr.weekday, hr.ordinal, hr.offset_days, "
        "       hr.holiday_date, hr.expression_language, hr.expression_text, "
        "       hr.valid_from_year, hr.valid_to_year "
        "FROM holiday_definition hd "
        "JOIN holiday_rule hr ON hr.holiday_id = hd.holiday_id "
        "LEFT JOIN holiday_name hn ON hn.holiday_id = hd.holiday_id AND hn.is_primary = 1 "
        "WHERE hd.jurisdiction_id = ?1 "
        "ORDER BY hd.holiday_id, hr.priority, hr.sequence_no;";
    sqlite3_stmt *stmt = NULL;
    const lineage_row_t *lineage = lineage_rows ? lineage_rows->items : NULL;
    size_t i;

    if (!lineage)
        return false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return false;

    for (i = 0; i < lineage_rows->count; ++i) {
        int rc;

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        if (sqlite3_bind_text(stmt, 1, lineage[i].jurisdiction_id, -1, SQLITE_STATIC) != SQLITE_OK)
            goto fail;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            holiday_rule_row_t row;

            memset(&row, 0, sizeof(row));
            row.holiday_id = sqlite3_column_int(stmt, 0);
            row.rule_id = sqlite3_column_int(stmt, 1);
            snprintf(row.jurisdiction_id, sizeof(row.jurisdiction_id), "%s", (const char *)sqlite3_column_text(stmt, 2));
            row.holiday_name = dup_text(sqlite3_column_text(stmt, 3));
            row.holiday_class = dup_text(sqlite3_column_text(stmt, 4));
            row.rule_kind = dup_text(sqlite3_column_text(stmt, 5));
            row.month = sqlite3_column_type(stmt, 6) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 6);
            row.day = sqlite3_column_type(stmt, 7) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 7);
            row.weekday = sqlite3_column_type(stmt, 8) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 8);
            row.ordinal = sqlite3_column_type(stmt, 9) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 9);
            row.offset_days = sqlite3_column_type(stmt, 10) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 10);
            row.holiday_date = dup_text(sqlite3_column_text(stmt, 11));
            row.expression_language = dup_text(sqlite3_column_text(stmt, 12));
            row.expression_text = dup_text(sqlite3_column_text(stmt, 13));
            row.valid_from_year = sqlite3_column_type(stmt, 14) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 14);
            row.valid_to_year = sqlite3_column_type(stmt, 15) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 15);
            if (!row.holiday_name || !row.holiday_class || !row.rule_kind || !vec_push(rules, &row))
                goto fail;
        }
        if (rc != SQLITE_DONE)
            goto fail;
    }

    sqlite3_finalize(stmt);
    return true;

fail:
    sqlite3_finalize(stmt);
    return false;
}

static bool load_observance_rules(sqlite3 *db, const pointer_vec_t *lineage_rows, pointer_vec_t *observances)
{
    static const char sql[] =
        "SELECT hor.holiday_id, hor.applies_to_rule_id, hor.observed_rule_kind, hor.observed_name, "
        "       hor.weekend_mask, hor.suppress_original, hor.valid_from_year, hor.valid_to_year "
        "FROM holiday_observance_rule hor "
        "JOIN holiday_definition hd ON hd.holiday_id = hor.holiday_id "
        "WHERE hd.jurisdiction_id = ?1 "
        "ORDER BY hor.holiday_id, hor.priority;";
    sqlite3_stmt *stmt = NULL;
    const lineage_row_t *lineage = lineage_rows ? lineage_rows->items : NULL;
    size_t i;

    if (!lineage)
        return false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return false;

    for (i = 0; i < lineage_rows->count; ++i) {
        int rc;

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        if (sqlite3_bind_text(stmt, 1, lineage[i].jurisdiction_id, -1, SQLITE_STATIC) != SQLITE_OK)
            goto fail;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            observance_rule_row_t row;

            memset(&row, 0, sizeof(row));
            row.holiday_id = sqlite3_column_int(stmt, 0);
            row.applies_to_rule_id = sqlite3_column_type(stmt, 1) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 1);
            row.observed_rule_kind = dup_text(sqlite3_column_text(stmt, 2));
            row.observed_name = dup_text(sqlite3_column_text(stmt, 3));
            row.weekend_mask = normalise_weekend_mask((const char *)sqlite3_column_text(stmt, 4));
            row.suppress_original = sqlite3_column_int(stmt, 5) != 0;
            row.valid_from_year = sqlite3_column_type(stmt, 6) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 6);
            row.valid_to_year = sqlite3_column_type(stmt, 7) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 7);
            if (!row.observed_rule_kind || !row.weekend_mask || !vec_push(observances, &row))
                goto fail;
        }
        if (rc != SQLITE_DONE)
            goto fail;
    }

    sqlite3_finalize(stmt);
    return true;

fail:
    sqlite3_finalize(stmt);
    return false;
}

static bool load_exceptions(sqlite3 *db, const pointer_vec_t *lineage_rows, pointer_vec_t *exceptions)
{
    static const char sql[] =
        "SELECT holiday_id, target_rule_id, holiday_date, action, name, valid_from_year, valid_to_year "
        "FROM holiday_exception WHERE jurisdiction_id = ?1 ORDER BY holiday_date, priority;";
    sqlite3_stmt *stmt = NULL;
    const lineage_row_t *lineage = lineage_rows ? lineage_rows->items : NULL;
    size_t i;

    if (!lineage)
        return false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return false;

    for (i = 0; i < lineage_rows->count; ++i) {
        int rc;

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        if (sqlite3_bind_text(stmt, 1, lineage[i].jurisdiction_id, -1, SQLITE_STATIC) != SQLITE_OK)
            goto fail;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            holiday_exception_row_t row;

            memset(&row, 0, sizeof(row));
            row.holiday_id = sqlite3_column_type(stmt, 0) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 0);
            row.target_rule_id = sqlite3_column_type(stmt, 1) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 1);
            row.holiday_date = dup_text(sqlite3_column_text(stmt, 2));
            row.action = dup_text(sqlite3_column_text(stmt, 3));
            row.name = dup_text(sqlite3_column_text(stmt, 4));
            row.valid_from_year = sqlite3_column_type(stmt, 5) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 5);
            row.valid_to_year = sqlite3_column_type(stmt, 6) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 6);
            if (!row.holiday_date || !row.action || !vec_push(exceptions, &row))
                goto fail;
        }
        if (rc != SQLITE_DONE)
            goto fail;
    }

    sqlite3_finalize(stmt);
    return true;

fail:
    sqlite3_finalize(stmt);
    return false;
}

static char *evaluate_rule_date(sqlite3 *db,
                                const holiday_rule_row_t *rule,
                                int year,
                                const char *jurisdiction)
{
    if (!rule || !rule->rule_kind || !year_in_range(year, rule->valid_from_year, rule->valid_to_year))
        return NULL;

    if (strcmp(rule->rule_kind, "fixed_date") == 0)
        return date_text_from_ymd((short)year, (month_t)rule->month, (uint8_t)rule->day);
    if (strcmp(rule->rule_kind, "one_off") == 0) {
        if (rule->holiday_date && atoi(rule->holiday_date) == year)
            return dup_c_string(rule->holiday_date);
        return NULL;
    }
    if (strcmp(rule->rule_kind, "easter_offset") == 0)
        return easter_related_text(year, rule->offset_days, false);
    if (strcmp(rule->rule_kind, "orthodox_easter_offset") == 0)
        return easter_related_text(year, rule->offset_days, true);
    if (strcmp(rule->rule_kind, "nth_weekday") == 0 && rule->ordinal > 0)
        return nth_weekday_of_month_text(year, rule->month, rule->weekday, rule->ordinal);
    if (strcmp(rule->rule_kind, "last_weekday") == 0)
        return last_weekday_of_month_text(year, rule->month, rule->weekday);
    if (strcmp(rule->rule_kind, "weekday_after_date") == 0)
        return weekday_after_date_text(year, rule->month, rule->day, rule->weekday);
    if (strcmp(rule->rule_kind, "weekday_before_date") == 0)
        return weekday_before_date_text(year, rule->month, rule->day, rule->weekday);
    if (strcmp(rule->rule_kind, "algorithmic") == 0 || strcmp(rule->rule_kind, "rrule") == 0)
        return evaluate_sql_rule_date(db, rule, year, jurisdiction);

    return NULL;
}

static bool add_event(pointer_vec_t *events,
                      int holiday_id,
                      int rule_id,
                      int event_year,
                      const char *holiday_date,
                      const char *holiday_name,
                      const char *holiday_class,
                      bool derived_from_observance)
{
    holiday_event_row_t event;

    memset(&event, 0, sizeof(event));
    event.holiday_id = holiday_id;
    event.rule_id = rule_id;
    event.event_year = event_year;
    event.holiday_date = dup_c_string(holiday_date);
    event.holiday_name = dup_c_string(holiday_name);
    event.holiday_class = dup_c_string(holiday_class ? holiday_class : "public");
    event.derived_from_observance = derived_from_observance;

    if (!event.holiday_date || !event.holiday_name || !event.holiday_class || !vec_push(events, &event)) {
        free(event.holiday_date);
        free(event.holiday_name);
        free(event.holiday_class);
        return false;
    }
    return true;
}

static void filter_events_to_range(pointer_vec_t *events,
                                   const char *start_text,
                                   const char *end_text)
{
    holiday_event_row_t *items = events ? events->items : NULL;
    size_t i;

    if (!items || !start_text || !end_text)
        return;

    for (i = 0u; i < events->count; ++i) {
        if (items[i].removed || !items[i].holiday_date)
            continue;
        if (strcmp(items[i].holiday_date, start_text) < 0 ||
            strcmp(items[i].holiday_date, end_text) > 0)
            items[i].removed = true;
    }
}

static void apply_exceptions(pointer_vec_t *events, const pointer_vec_t *exceptions)
{
    holiday_event_row_t *event_items = events ? events->items : NULL;
    const holiday_exception_row_t *exception_items = exceptions ? exceptions->items : NULL;
    size_t i;
    size_t j;

    if (!event_items || !exception_items)
        return;

    for (i = 0; i < exceptions->count; ++i) {
        int exception_year = atoi(exception_items[i].holiday_date);

        if (!year_in_range(exception_year, exception_items[i].valid_from_year, exception_items[i].valid_to_year))
            continue;

        if (strcmp(exception_items[i].action, "add") == 0 || strcmp(exception_items[i].action, "replace") == 0) {
            const char *name = exception_items[i].name;
            const char *holiday_class = "public";

            for (j = 0; j < events->count; ++j) {
                if (event_items[j].removed)
                    continue;
                if (event_items[j].holiday_id == exception_items[i].holiday_id && event_items[j].holiday_class) {
                    holiday_class = event_items[j].holiday_class;
                    if (!name)
                        name = event_items[j].holiday_name;
                    break;
                }
            }
            if (strcmp(exception_items[i].action, "replace") == 0) {
                for (j = 0; j < events->count; ++j) {
                    if (event_items[j].removed || event_items[j].event_year != exception_year)
                        continue;
                    if ((exception_items[i].target_rule_id != 0 && event_items[j].rule_id == exception_items[i].target_rule_id) ||
                        (exception_items[i].target_rule_id == 0 && event_items[j].holiday_id == exception_items[i].holiday_id))
                        event_items[j].removed = true;
                }
            }
            add_event(events,
                      exception_items[i].holiday_id,
                      exception_items[i].target_rule_id,
                      exception_year,
                      exception_items[i].holiday_date,
                      name ? name : "Holiday",
                      holiday_class,
                      false);
            event_items = events->items;
        } else if (strcmp(exception_items[i].action, "suppress") == 0) {
            for (j = 0; j < events->count; ++j) {
                if (event_items[j].removed)
                    continue;
                if (strcmp(event_items[j].holiday_date, exception_items[i].holiday_date) == 0 &&
                    (exception_items[i].target_rule_id == 0 || event_items[j].rule_id == exception_items[i].target_rule_id) &&
                    (exception_items[i].holiday_id == 0 || event_items[j].holiday_id == exception_items[i].holiday_id))
                    event_items[j].removed = true;
            }
        } else if (strcmp(exception_items[i].action, "rename") == 0 && exception_items[i].name) {
            for (j = 0; j < events->count; ++j) {
                if (event_items[j].removed)
                    continue;
                if (strcmp(event_items[j].holiday_date, exception_items[i].holiday_date) == 0 &&
                    (exception_items[i].target_rule_id == 0 || event_items[j].rule_id == exception_items[i].target_rule_id) &&
                    (exception_items[i].holiday_id == 0 || event_items[j].holiday_id == exception_items[i].holiday_id)) {
                    free(event_items[j].holiday_name);
                    event_items[j].holiday_name = dup_c_string(exception_items[i].name);
                }
            }
        }
    }
}

static void apply_observances(pointer_vec_t *events,
                              const pointer_vec_t *observances,
                              const weekend_rule_t *weekend_rules,
                              size_t weekend_rule_count)
{
    holiday_event_row_t *event_items = events ? events->items : NULL;
    const observance_rule_row_t *observance_items = observances ? observances->items : NULL;
    size_t i;
    size_t j;

    if (!event_items || !observance_items)
        return;

    for (i = 0; i < observances->count; ++i) {
        for (j = 0; j < events->count; ++j) {
            const char *weekend_mask;
            int weekday;
            char *observed_date = NULL;

            if (event_items[j].removed || event_items[j].derived_from_observance)
                continue;
            if (event_items[j].holiday_id != observance_items[i].holiday_id)
                continue;
            if (observance_items[i].applies_to_rule_id != 0 &&
                event_items[j].rule_id != observance_items[i].applies_to_rule_id)
                continue;
            if (!year_in_range(event_items[j].event_year,
                               observance_items[i].valid_from_year,
                               observance_items[i].valid_to_year))
                continue;

            weekend_mask = observance_items[i].weekend_mask && *observance_items[i].weekend_mask
                ? observance_items[i].weekend_mask
                : effective_weekend_mask_for_year(weekend_rules, weekend_rule_count, event_items[j].event_year);
            weekday = iso_weekday_from_date_text(event_items[j].holiday_date);
            if (!weekend_mask_contains(weekend_mask, weekday))
                continue;

            if (strcmp(observance_items[i].observed_rule_kind, "next_weekday") == 0)
                observed_date = next_observed_date(event_items[j].holiday_date, weekend_mask, event_items, events->count, false, false);
            else if (strcmp(observance_items[i].observed_rule_kind, "next_monday") == 0)
                observed_date = next_observed_date(event_items[j].holiday_date, weekend_mask, event_items, events->count, false, true);
            else if (strcmp(observance_items[i].observed_rule_kind, "next_non_holiday") == 0)
                observed_date = next_observed_date(event_items[j].holiday_date, weekend_mask, event_items, events->count, true, false);
            else if (strcmp(observance_items[i].observed_rule_kind, "previous_weekday") == 0)
                observed_date = previous_observed_date(event_items[j].holiday_date, weekend_mask, event_items, events->count, false);
            else if (strcmp(observance_items[i].observed_rule_kind, "christmas_pair") == 0)
                observed_date = next_observed_date(event_items[j].holiday_date, weekend_mask, event_items, events->count, true, false);

            if (!observed_date)
                continue;
            if (observance_items[i].suppress_original)
                event_items[j].removed = true;
            add_event(events,
                      event_items[j].holiday_id,
                      event_items[j].rule_id,
                      event_items[j].event_year,
                      observed_date,
                      observance_items[i].observed_name ? observance_items[i].observed_name : event_items[j].holiday_name,
                      event_items[j].holiday_class,
                      true);
            free(observed_date);
            event_items = events->items;
        }
    }
}

holiday_t *holiday_open(const char *jurisdiction)
{
    holiday_t *holiday;
    const char *resolved_key = holiday_db_key_from_env();
    string_t *path = NULL;
    string_t *key = NULL;
    const char *resolved_path = holiday_db_path_from_env();

    if (!resolved_path || !*resolved_path || !resolved_key || !*resolved_key) {
        free((char *)resolved_path);
        return NULL;
    }
    holiday = calloc(1u, sizeof(*holiday));
    if (!holiday) {
        free((char *)resolved_path);
        return NULL;
    }
    holiday->error = string_new();
    if (!holiday->error) {
        free((char *)resolved_path);
        free(holiday);
        return NULL;
    }
    if (jurisdiction && *jurisdiction) {
        size_t len = strlen(jurisdiction);

        if (len >= sizeof(holiday->jurisdiction))
            len = sizeof(holiday->jurisdiction) - 1u;
        memcpy(holiday->jurisdiction, jurisdiction, len);
        holiday->jurisdiction[len] = '\0';
    } else {
        detect_default_jurisdiction(holiday->jurisdiction);
    }
    path = string_new_from_cstr(resolved_path);
    key = string_new_from_cstr(resolved_key);
    holiday->db = (path && key) ? sqlite_open_encrypted(path, key) : NULL;
    string_free(key);
    string_free(path);
    free((char *)resolved_path);
    if (!holiday->db) {
        string_free(holiday->error);
        free(holiday);
        return NULL;
    }
    return holiday;
}

void holiday_close(holiday_t *holiday)
{
    if (!holiday)
        return;
    sqlite_close(holiday->db);
    string_free(holiday->error);
    free(holiday);
}

const char *holiday_last_error(const holiday_t *holiday)
{
    return (holiday && holiday->error) ? string_c_str(holiday->error) : NULL;
}

static void holiday_event_destroy_owned(holiday_event_t *event)
{
    if (!event)
        return;
    datetime_dealloc((datetime_t *)event->holiday_date);
    free((char *)event->holiday_name);
    free((char *)event->holiday_class);
    memset(event, 0, sizeof(*event));
}

typedef struct holiday_collect_ctx_t {
    array_t *events;
} holiday_collect_ctx_t;

static bool holiday_collect_event(const holiday_event_t *event, void *ctx)
{
    holiday_collect_ctx_t *collect_ctx = ctx;
    holiday_event_t owned;

    if (!event || !collect_ctx || !collect_ctx->events)
        return false;

    memset(&owned, 0, sizeof(owned));
    owned.holiday_id = event->holiday_id;
    owned.rule_id = event->rule_id;
    owned.event_year = event->event_year;
    owned.holiday_date = event->holiday_date
        ? datetime_init_copy(datetime_alloc(), event->holiday_date)
        : NULL;
    owned.holiday_name = event->holiday_name ? dup_c_string(event->holiday_name) : NULL;
    owned.holiday_class = event->holiday_class ? dup_c_string(event->holiday_class) : NULL;
    owned.derived_from_observance = event->derived_from_observance;

    if ((event->holiday_date && !owned.holiday_date) ||
        (event->holiday_name && !owned.holiday_name) ||
        (event->holiday_class && !owned.holiday_class) ||
        !array_add(collect_ctx->events, &owned)) {
        holiday_event_destroy_owned(&owned);
        return false;
    }
    return true;
}

array_t *holiday_between(holiday_t *holiday,
                         const datetime_t *start,
                         const datetime_t *end)
{
    holiday_collect_ctx_t ctx;
    array_t *events;

    events = array_create(sizeof(holiday_event_t), NULL, (array_destroy_fn)holiday_event_destroy_owned);
    if (!events)
        return NULL;

    memset(&ctx, 0, sizeof(ctx));
    ctx.events = events;
    if (!holiday_each_between(holiday, start, end, holiday_collect_event, &ctx)) {
        array_destroy(events);
        return NULL;
    }
    return events;
}

static bool holiday_date_matches(const datetime_t *lhs, const datetime_t *rhs)
{
    return lhs && rhs && datetime_compare(lhs, rhs) == 0;
}

static bool holiday_has_event_on_date(const array_t *events, const datetime_t *date)
{
    size_t i;

    if (!events || !date)
        return false;
    for (i = 0u; i < array_size(events); ++i) {
        const holiday_event_t *event = array_get(events, i);

        if (event && holiday_date_matches(event->holiday_date, date))
            return true;
    }
    return false;
}

bool holiday_is_national_holiday(holiday_t *holiday, const datetime_t *date)
{
    array_t *events;
    bool is_holiday;

    if (!holiday || !date) {
        holiday_set_error(holiday, "invalid holiday query");
        return false;
    }
    events = holiday_between(holiday, date, date);
    if (!events)
        return false;
    is_holiday = array_size(events) > 0u;
    array_destroy(events);
    return is_holiday;
}

bool holiday_is_weekend(holiday_t *holiday, const datetime_t *date)
{
    sqlite3 *db = holiday ? sqlite_native_handle(holiday->db) : NULL;
    const char *jurisdiction = holiday ? holiday->jurisdiction : NULL;
    pointer_vec_t lineage_rows = {0};
    pointer_vec_t weekend_rules = {0};
    weekend_rule_t *weekend_items;
    const char *weekend_mask;
    int iso_weekday;
    bool is_weekend = false;

    lineage_rows.item_size = sizeof(lineage_row_t);
    weekend_rules.item_size = sizeof(weekend_rule_t);

    if (!holiday || !db || !jurisdiction || *jurisdiction == '\0' || !date) {
        holiday_set_error(holiday, "invalid holiday query");
        return false;
    }
    if (!load_lineage(db, jurisdiction, &lineage_rows) ||
        !load_weekend_rules(db, &lineage_rows, &weekend_rules)) {
        holiday_set_error(holiday, "failed to load weekend rules");
        goto done;
    }

    weekend_items = weekend_rules.items;
    weekend_mask = effective_weekend_mask_for_year(weekend_items, weekend_rules.count, (int)datetime_year(date));
    iso_weekday = iso_weekday_from_datetime_weekday((int)datetime_weekday(date));
    is_weekend = weekend_mask_contains(weekend_mask, iso_weekday);

done:
    free_weekend_rules(&weekend_rules);
    vec_free(&lineage_rows);
    return is_weekend;
}

long holiday_working_days_between(holiday_t *holiday,
                                  const datetime_t *start,
                                  const datetime_t *end)
{
    array_t *events;
    datetime_t *cursor = NULL;
    long working_days = 0;

    if (!holiday || !start || !end || datetime_compare(start, end) > 0) {
        holiday_set_error(holiday, "invalid holiday query");
        return -1;
    }

    events = holiday_between(holiday, start, end);
    if (!events)
        return -1;

    cursor = datetime_init_copy(datetime_alloc(), start);
    if (!cursor) {
        array_destroy(events);
        holiday_set_error(holiday, "failed to initialise working day cursor");
        return -1;
    }

    while (datetime_compare(cursor, end) <= 0) {
        if (!holiday_is_weekend(holiday, cursor) &&
            !holiday_has_event_on_date(events, cursor)) {
            working_days++;
        }
        if (!datetime_add_days(cursor, 1)) {
            working_days = -1;
            holiday_set_error(holiday, "failed to advance working day cursor");
            break;
        }
    }

    datetime_dealloc(cursor);
    array_destroy(events);
    return working_days;
}

bool holiday_each_between(holiday_t *holiday,
                          const datetime_t *start,
                          const datetime_t *end,
                          holiday_visit_fn visitor,
                          void *ctx)
{
    sqlite3 *db = holiday ? sqlite_native_handle(holiday->db) : NULL;
    const char *jurisdiction = holiday ? holiday->jurisdiction : NULL;
    char *start_text = NULL;
    char *end_text = NULL;
    pointer_vec_t lineage_rows = {0};
    pointer_vec_t weekend_rules = {0};
    pointer_vec_t rules = {0};
    pointer_vec_t observances = {0};
    pointer_vec_t exceptions = {0};
    pointer_vec_t events = {0};
    holiday_rule_row_t *rule_items;
    holiday_event_row_t *event_items;
    weekend_rule_t *weekend_items;
    int year;
    size_t i;
    bool ok = false;

    lineage_rows.item_size = sizeof(lineage_row_t);
    weekend_rules.item_size = sizeof(weekend_rule_t);
    rules.item_size = sizeof(holiday_rule_row_t);
    observances.item_size = sizeof(observance_rule_row_t);
    exceptions.item_size = sizeof(holiday_exception_row_t);
    events.item_size = sizeof(holiday_event_row_t);

    if (!holiday || !db || !jurisdiction || *jurisdiction == '\0' || !start || !end || !visitor) {
        holiday_set_error(holiday, "invalid holiday query");
        return false;
    }

    start_text = format_date(start);
    end_text = format_date(end);
    if (!start_text || !end_text) {
        holiday_set_error(holiday, "failed to format date range");
        goto done;
    }

    if (!load_lineage(db, jurisdiction, &lineage_rows) ||
        !load_weekend_rules(db, &lineage_rows, &weekend_rules) ||
        !load_holiday_rules(db, &lineage_rows, &rules) ||
        !load_observance_rules(db, &lineage_rows, &observances) ||
        !load_exceptions(db, &lineage_rows, &exceptions)) {
        holiday_set_error(holiday, "failed to load holiday rules");
        goto done;
    }

    rule_items = rules.items;
    weekend_items = weekend_rules.items;
    for (year = datetime_year(start); year <= datetime_year(end); ++year) {
        (void)effective_weekend_mask_for_year(weekend_items, weekend_rules.count, year);
        for (i = 0; i < rules.count; ++i) {
            char *holiday_date = evaluate_rule_date(db, &rule_items[i], year, jurisdiction);

            if (!holiday_date)
                continue;
            if (strcmp(holiday_date, start_text) >= 0 && strcmp(holiday_date, end_text) <= 0) {
                if (!add_event(&events,
                               rule_items[i].holiday_id,
                               rule_items[i].rule_id,
                               year,
                               holiday_date,
                               rule_items[i].holiday_name,
                               rule_items[i].holiday_class,
                               false)) {
                    free(holiday_date);
                    holiday_set_error(holiday, "failed to collect holiday event");
                    goto done;
                }
            }
            free(holiday_date);
        }
    }

    apply_exceptions(&events, &exceptions);
    apply_observances(&events, &observances, weekend_items, weekend_rules.count);
    filter_events_to_range(&events, start_text, end_text);

    event_items = events.items;
    qsort(event_items, events.count, sizeof(*event_items), event_compare);
    for (i = 0; i < events.count; ++i) {
        holiday_event_t public_event;
        datetime_t *public_date = NULL;
        short year_value;
        month_t month_value;
        uint8_t day_value;

        if (event_items[i].removed)
            continue;
        if (!parse_date_text(event_items[i].holiday_date, &year_value, &month_value, &day_value)) {
            holiday_set_error(holiday, "failed to parse holiday date");
            goto done;
        }
        public_date = datetime_init_ymd(datetime_alloc(), year_value, month_value, day_value);
        if (!public_date) {
            holiday_set_error(holiday, "failed to materialise holiday date");
            goto done;
        }
        memset(&public_event, 0, sizeof(public_event));
        public_event.holiday_id = event_items[i].holiday_id;
        public_event.rule_id = event_items[i].rule_id;
        public_event.event_year = event_items[i].event_year;
        public_event.holiday_date = public_date;
        public_event.holiday_name = event_items[i].holiday_name;
        public_event.holiday_class = event_items[i].holiday_class;
        public_event.derived_from_observance = event_items[i].derived_from_observance;
        if (!visitor(&public_event, ctx)) {
            datetime_dealloc(public_date);
            ok = true;
            goto done;
        }
        datetime_dealloc(public_date);
    }

    ok = true;

done:
    free_events(&events);
    free_exception_rows(&exceptions);
    free_observance_rule_rows(&observances);
    free_holiday_rule_rows(&rules);
    free_weekend_rules(&weekend_rules);
    vec_free(&lineage_rows);
    free(start_text);
    free(end_text);
    return ok;
}
