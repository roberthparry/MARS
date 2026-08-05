#include <stdbool.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "array.h"
#include "jurisdiction.h"
#include "sqlite.h"
#include "ustring.h"

typedef struct _jurisdiction_t {
    sqlite_t *db;
    string_t *error;
    char jurisdiction[32];
} jurisdiction_t;

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

typedef struct timezone_era_row_t {
    int sequence_no;
    int gmtoff_minutes;
    char *rules_kind;
    int fixed_save_minutes;
    char *rule_name;
    char *until_day_kind;
    int until_year;
    int until_month;
    int until_day_value;
    int until_weekday;
    int until_seconds;
    char until_suffix;
} timezone_era_row_t;

typedef struct timezone_transition_rule_row_t {
    char *rule_name;
    int from_year;
    int to_year;
    int in_month;
    char *on_kind;
    int on_day;
    int on_weekday;
    int at_seconds;
    char at_suffix;
    int save_minutes;
} timezone_transition_rule_row_t;

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

typedef struct timezone_transition_occurrence_t {
    short year;
    month_t month;
    uint8_t day;
    int seconds;
    char at_suffix;
    int gmtoff_minutes;
} timezone_transition_occurrence_t;

typedef struct timezone_named_rule_ref_t {
    char *rule_name;
    int gmtoff_minutes;
} timezone_named_rule_ref_t;

typedef struct pointer_vec_t {
    void *items;
    size_t count;
    size_t capacity;
    size_t item_size;
} pointer_vec_t;

static bool text_equals(const char *left, const char *right)
{
    return left && right && strcmp(left, right) == 0;
}

static void jurisdiction_set_error(jurisdiction_t *jurisdiction, const char *message)
{
    if (!jurisdiction || !jurisdiction->error)
        return;
    string_clear(jurisdiction->error);
    (void)string_append_cstr(jurisdiction->error, message ? message : "jurisdiction error");
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
        return dup_c_string(mars_home);
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

static char *config_lookup_at_path(char *config_path, const char *name)
{
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
        return dup_c_string(path_env);
    path_env = getenv("MARS_HOLIDAY_DB_PATH");
    if (path_env && *path_env)
        return dup_c_string(path_env);
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
        return dup_c_string(env_key);
    env_key = getenv("MARS_HOLIDAY_DB_KEY");
    if (env_key && *env_key)
        return dup_c_string(env_key);
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

static char *dup_text(const char *text)
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
    cmp = text_compare(a->holiday_name, b->holiday_name);
    if (cmp != 0)
        return cmp;
    cmp = text_compare(a->holiday_class, b->holiday_class);
    if (cmp != 0)
        return cmp;
    if (a->derived_from_observance != b->derived_from_observance)
        return a->derived_from_observance ? 1 : -1;
    if (a->holiday_id != b->holiday_id)
        return (a->holiday_id < b->holiday_id) ? -1 : 1;
    if (a->rule_id != b->rule_id)
        return (a->rule_id < b->rule_id) ? -1 : 1;
    return 0;
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

static void free_timezone_era_rows(pointer_vec_t *rows)
{
    size_t i;
    timezone_era_row_t *items = rows ? rows->items : NULL;

    if (!items)
        return;
    for (i = 0u; i < rows->count; ++i) {
        free(items[i].rules_kind);
        free(items[i].rule_name);
        free(items[i].until_day_kind);
    }
    vec_free(rows);
}

static void free_timezone_transition_rule_rows(pointer_vec_t *rows)
{
    size_t i;
    timezone_transition_rule_row_t *items = rows ? rows->items : NULL;

    if (!items)
        return;
    for (i = 0u; i < rows->count; ++i) {
        free(items[i].rule_name);
        free(items[i].on_kind);
    }
    vec_free(rows);
}

static void free_timezone_named_rule_refs(pointer_vec_t *rows)
{
    size_t i;
    timezone_named_rule_ref_t *items = rows ? rows->items : NULL;

    if (!items)
        return;
    for (i = 0u; i < rows->count; ++i)
        free(items[i].rule_name);
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

static char *evaluate_sql_rule_date(sqlite_t *db,
                                    const holiday_rule_row_t *rule,
                                    int year,
                                    const char *jurisdiction)
{
    sqlite_stmt_t *stmt = NULL;
    char *sql = NULL;
    const char *date_text;
    char *out = NULL;
    sqlite_step_result_t rc;
    size_t sql_len;
    static const char prefix[] =
        "with holiday_rule_context(rule_year, jurisdiction_id, holiday_id, holiday_rule_id) as ("
        " select ?1, ?2, ?3, ?4"
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

    stmt = sqlite_stmt_prepare(db, sql);
    free(sql);
    if (!stmt)
        return NULL;
    if (!sqlite_stmt_bind_int(stmt, 1, year) ||
        !sqlite_stmt_bind_text(stmt, 2, jurisdiction) ||
        !sqlite_stmt_bind_int(stmt, 3, rule->holiday_id) ||
        !sqlite_stmt_bind_int(stmt, 4, rule->rule_id)) {
        sqlite_stmt_finalize(stmt);
        return NULL;
    }

    rc = sqlite_stmt_step(stmt);
    if (rc == SQLITE_STEP_ROW) {
        date_text = sqlite_stmt_column_text(stmt, 0);
        if (date_text)
            out = dup_c_string(date_text);
    }
    sqlite_stmt_finalize(stmt);
    return out;
}

static bool load_lineage(sqlite_t *db, const char *jurisdiction, pointer_vec_t *lineage_rows)
{
    static const char sql[] =
        "with recursive lineage(jurisdiction_id, depth) as ("
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
        if (!vec_push(lineage_rows, &row))
            goto fail;
    }

    sqlite_stmt_finalize(stmt);
    return rc == SQLITE_STEP_DONE;

fail:
    sqlite_stmt_finalize(stmt);
    return false;
}

static bool load_weekend_rules(sqlite_t *db, const pointer_vec_t *lineage_rows, pointer_vec_t *weekend_rules)
{
    static const char sql[] =
        "select jurisdiction_id, weekend_mask, valid_from_year, valid_to_year "
        "from jurisdiction_weekend_rule where jurisdiction_id = ?1 "
        "order by coalesce(valid_from_year, -999999), coalesce(valid_to_year, 999999);";
    sqlite_stmt_t *stmt = NULL;
    const lineage_row_t *lineage = lineage_rows ? lineage_rows->items : NULL;
    size_t i;

    if (!lineage)
        return false;
    stmt = sqlite_stmt_prepare(db, sql);
    if (!stmt)
        return false;

    for (i = 0; i < lineage_rows->count; ++i) {
        sqlite_step_result_t rc;

        sqlite_stmt_reset(stmt);
        sqlite_stmt_clear_bindings(stmt);
        if (!sqlite_stmt_bind_text(stmt, 1, lineage[i].jurisdiction_id))
            goto fail;
        while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
            weekend_rule_t row;

            memset(&row, 0, sizeof(row));
            snprintf(row.jurisdiction_id, sizeof(row.jurisdiction_id), "%s", lineage[i].jurisdiction_id);
            row.depth = lineage[i].depth;
            row.weekend_mask = normalise_weekend_mask(sqlite_stmt_column_text(stmt, 1));
            row.valid_from_year = sqlite_stmt_column_is_null(stmt, 2) ? 0 : sqlite_stmt_column_int(stmt, 2);
            row.valid_to_year = sqlite_stmt_column_is_null(stmt, 3) ? 0 : sqlite_stmt_column_int(stmt, 3);
            if (!row.weekend_mask || !vec_push(weekend_rules, &row))
                goto fail;
        }
        if (rc != SQLITE_STEP_DONE)
            goto fail;
    }

    sqlite_stmt_finalize(stmt);
    return true;

fail:
    sqlite_stmt_finalize(stmt);
    return false;
}

static bool load_default_location(sqlite_t *db,
                                  const pointer_vec_t *lineage_rows,
                                  double *latitude,
                                  double *longitude)
{
    static const char sql[] =
        "select lat.latitude, lon.longitude "
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
        if (!parse_double_text(lat_text, &parsed_latitude) ||
            !parse_double_text(lon_text, &parsed_longitude)) {
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

static bool load_default_timezone_name(sqlite_t *db,
                                       const pointer_vec_t *lineage_rows,
                                       char *timezone_name,
                                       size_t timezone_name_size)
{
    static const char sql[] =
        "select code.timezone_name "
        "from jurisdiction_location_default as loc "
        "join jurisdiction_location_default_timezone as tz "
        "  on tz.jurisdiction_id = loc.jurisdiction_id "
        "join timezone_code as code "
        "  on code.timezone_code = tz.timezone_code "
        "where loc.jurisdiction_id = ?1;";
    sqlite_stmt_t *stmt = NULL;
    const lineage_row_t *lineage = lineage_rows ? lineage_rows->items : NULL;
    size_t i;
    bool found = false;

    if (!lineage || !timezone_name || timezone_name_size == 0u)
        return false;
    stmt = sqlite_stmt_prepare(db, sql);
    if (!stmt)
        return false;

    timezone_name[0] = '\0';
    for (i = 0; i < lineage_rows->count; ++i) {
        const char *tz_text;
        sqlite_step_result_t rc;

        sqlite_stmt_reset(stmt);
        sqlite_stmt_clear_bindings(stmt);
        if (!sqlite_stmt_bind_text(stmt, 1, lineage[i].jurisdiction_id))
            goto done;
        rc = sqlite_stmt_step(stmt);
        if (rc != SQLITE_STEP_ROW)
            continue;
        tz_text = sqlite_stmt_column_text(stmt, 0);
        if (!tz_text || *tz_text == '\0')
            goto done;
        snprintf(timezone_name, timezone_name_size, "%s", tz_text);
        found = true;
        break;
    }

done:
    sqlite_stmt_finalize(stmt);
    return found;
}

static bool load_timezone_eras(sqlite_t *db,
                               const char *timezone_name,
                               pointer_vec_t *rows)
{
    static const char canonical_sql[] =
        "select canonical_timezone_name "
        "from timezone_canonical where timezone_name = ?1;";
    static const char sql[] =
        "select sequence_no, gmtoff_minutes, rules_kind, fixed_save_minutes, rule_name, "
        "       until_year, until_month, until_day_kind, until_day_value, "
        "       until_weekday, until_seconds, until_suffix "
        "from timezone_era where timezone_name = ?1 order by sequence_no;";
    sqlite_stmt_t *stmt = NULL;
    sqlite_stmt_t *canonical_stmt = NULL;
    const char *query_timezone_name = timezone_name;
    char canonical_timezone_name[128];
    sqlite_step_result_t rc = SQLITE_STEP_DONE;
    bool retried_with_canonical = false;

    if (!db || !timezone_name || !*timezone_name || !rows)
        return false;
    canonical_timezone_name[0] = '\0';

retry:
    stmt = sqlite_stmt_prepare(db, sql);
    if (!stmt)
        return false;
    if (!sqlite_stmt_bind_text(stmt, 1, query_timezone_name))
        goto fail;

    while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        timezone_era_row_t row;

        memset(&row, 0, sizeof(row));
        row.sequence_no = sqlite_stmt_column_int(stmt, 0);
        row.gmtoff_minutes = sqlite_stmt_column_int(stmt, 1);
        row.rules_kind = dup_text(sqlite_stmt_column_text(stmt, 2));
        row.fixed_save_minutes = sqlite_stmt_column_is_null(stmt, 3) ? 0 : sqlite_stmt_column_int(stmt, 3);
        row.rule_name = dup_text(sqlite_stmt_column_text(stmt, 4));
        row.until_year = sqlite_stmt_column_is_null(stmt, 5) ? 0 : sqlite_stmt_column_int(stmt, 5);
        row.until_month = sqlite_stmt_column_is_null(stmt, 6) ? 0 : sqlite_stmt_column_int(stmt, 6);
        row.until_day_kind = dup_text(sqlite_stmt_column_text(stmt, 7));
        row.until_day_value = sqlite_stmt_column_is_null(stmt, 8) ? 0 : sqlite_stmt_column_int(stmt, 8);
        row.until_weekday = sqlite_stmt_column_is_null(stmt, 9) ? 0 : sqlite_stmt_column_int(stmt, 9);
        row.until_seconds = sqlite_stmt_column_is_null(stmt, 10) ? 0 : sqlite_stmt_column_int(stmt, 10);
        row.until_suffix = sqlite_stmt_column_is_null(stmt, 11)
            ? '\0'
            : sqlite_stmt_column_text(stmt, 11)[0];
        if (!row.rules_kind || !vec_push(rows, &row)) {
            free(row.rules_kind);
            free(row.rule_name);
            free(row.until_day_kind);
            goto fail;
        }
    }

    sqlite_stmt_finalize(stmt);
    stmt = NULL;
    if (rows->count == 0u && !retried_with_canonical) {
        canonical_stmt = sqlite_stmt_prepare(db, canonical_sql);
        if (!canonical_stmt)
            return false;
        if (!sqlite_stmt_bind_text(canonical_stmt, 1, timezone_name))
            goto fail;
        if (sqlite_stmt_step(canonical_stmt) == SQLITE_STEP_ROW) {
            const char *canonical_text = sqlite_stmt_column_text(canonical_stmt, 0);

            if (canonical_text && *canonical_text) {
                snprintf(canonical_timezone_name,
                         sizeof(canonical_timezone_name),
                         "%s",
                         canonical_text);
                if (strcmp(canonical_timezone_name, timezone_name) != 0) {
                    query_timezone_name = canonical_timezone_name;
                    retried_with_canonical = true;
                }
            }
        }
        sqlite_stmt_finalize(canonical_stmt);
        canonical_stmt = NULL;
        if (retried_with_canonical)
            goto retry;
    }
    return rc == SQLITE_STEP_DONE;

fail:
    sqlite_stmt_finalize(stmt);
    sqlite_stmt_finalize(canonical_stmt);
    return false;
}

static bool load_timezone_transition_rules(sqlite_t *db,
                                           const char *rule_name,
                                           pointer_vec_t *rows)
{
    static const char sql[] =
        "select rule_name, from_year, to_year, in_month, on_kind, on_day, on_weekday, "
        "       at_seconds, at_suffix, save_minutes "
        "from timezone_transition_rule where rule_name = ?1 "
        "order by coalesce(from_year, -999999), coalesce(to_year, 999999), in_month, on_day;";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;

    if (!db || !rule_name || !*rule_name || !rows)
        return false;
    stmt = sqlite_stmt_prepare(db, sql);
    if (!stmt)
        return false;
    if (!sqlite_stmt_bind_text(stmt, 1, rule_name))
        goto fail;

    while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        timezone_transition_rule_row_t row;

        memset(&row, 0, sizeof(row));
        row.rule_name = dup_text(sqlite_stmt_column_text(stmt, 0));
        row.from_year = sqlite_stmt_column_is_null(stmt, 1) ? 0 : sqlite_stmt_column_int(stmt, 1);
        row.to_year = sqlite_stmt_column_is_null(stmt, 2) ? 0 : sqlite_stmt_column_int(stmt, 2);
        row.in_month = sqlite_stmt_column_int(stmt, 3);
        row.on_kind = dup_text(sqlite_stmt_column_text(stmt, 4));
        row.on_day = sqlite_stmt_column_int(stmt, 5);
        row.on_weekday = sqlite_stmt_column_is_null(stmt, 6) ? 0 : sqlite_stmt_column_int(stmt, 6);
        row.at_seconds = sqlite_stmt_column_int(stmt, 7);
        row.at_suffix = sqlite_stmt_column_is_null(stmt, 8)
            ? 'w'
            : sqlite_stmt_column_text(stmt, 8)[0];
        row.save_minutes = sqlite_stmt_column_int(stmt, 9);
        if (!row.rule_name || !row.on_kind || !vec_push(rows, &row)) {
            free(row.rule_name);
            free(row.on_kind);
            goto fail;
        }
    }

    sqlite_stmt_finalize(stmt);
    return rc == SQLITE_STEP_DONE;

fail:
    sqlite_stmt_finalize(stmt);
    return false;
}

static bool resolve_month_day(short year,
                              month_t month,
                              const char *day_kind,
                              int day_value,
                              int weekday,
                              uint8_t *resolved_day)
{
    datetime_t *probe = NULL;
    int target_day = day_value;
    weekday_t current_weekday;
    int delta;
    bool ok = false;

    if (!resolved_day || !datetime_valid_ymd(year, month, 1))
        return false;

    if (!day_kind || strcmp(day_kind, "day_of_month") == 0) {
        target_day = day_value;
    } else if (strcmp(day_kind, "last_weekday") == 0) {
        target_day = (int)datetime_days_in_month(year, month);
        probe = datetime_init_ymd(datetime_alloc(), year, month, (uint8_t)target_day);
        if (!probe)
            goto done;
        current_weekday = datetime_weekday(probe);
        delta = ((int)current_weekday - weekday + 7) % 7;
        target_day -= delta;
    } else if (strcmp(day_kind, "weekday_on_or_after") == 0) {
        target_day = day_value;
        if (!datetime_valid_ymd(year, month, (uint8_t)target_day) ||
            !(probe = datetime_init_ymd(datetime_alloc(), year, month, (uint8_t)target_day)))
            goto done;
        current_weekday = datetime_weekday(probe);
        delta = (weekday - (int)current_weekday + 7) % 7;
        target_day += delta;
    } else if (strcmp(day_kind, "weekday_on_or_before") == 0) {
        target_day = day_value;
        if (!datetime_valid_ymd(year, month, (uint8_t)target_day) ||
            !(probe = datetime_init_ymd(datetime_alloc(), year, month, (uint8_t)target_day)))
            goto done;
        current_weekday = datetime_weekday(probe);
        delta = ((int)current_weekday - weekday + 7) % 7;
        target_day -= delta;
    } else {
        goto done;
    }

    if (!datetime_valid_ymd(year, month, (uint8_t)target_day))
        goto done;
    *resolved_day = (uint8_t)target_day;
    ok = true;

done:
    datetime_dealloc(probe);
    return ok;
}

static bool normalise_day_time(short *year,
                               month_t *month,
                               uint8_t *day,
                               int *seconds)
{
    datetime_t *date = NULL;
    long shift_days = 0;
    bool ok = false;

    if (!year || !month || !day || !seconds)
        return false;
    date = datetime_init_ymd(datetime_alloc(), *year, *month, *day);
    if (!date)
        goto done;

    while (*seconds < 0) {
        *seconds += 86400;
        shift_days -= 1;
    }
    while (*seconds >= 86400) {
        *seconds -= 86400;
        shift_days += 1;
    }
    if (shift_days != 0) {
        if (!datetime_add_days(date, shift_days))
            goto done;
        *year = datetime_year(date);
        *month = datetime_month(date);
        *day = datetime_day(date);
    }
    ok = true;

done:
    datetime_dealloc(date);
    return ok;
}

static int compare_local_noon_to_boundary(const datetime_t *date,
                                          short boundary_year,
                                          month_t boundary_month,
                                          uint8_t boundary_day,
                                          int boundary_seconds)
{
    datetime_t *boundary = NULL;
    int cmp;

    if (!date)
        return 0;
    boundary = datetime_init_ymd(datetime_alloc(), boundary_year, boundary_month, boundary_day);
    if (!boundary)
        return 0;
    cmp = datetime_compare(date, boundary);
    datetime_dealloc(boundary);
    if (cmp != 0)
        return cmp;
    if (12 * 3600 < boundary_seconds)
        return -1;
    if (12 * 3600 > boundary_seconds)
        return 1;
    return 0;
}

static const timezone_era_row_t *select_timezone_era_for_date(const pointer_vec_t *eras,
                                                              const datetime_t *date)
{
    const timezone_era_row_t *items = eras ? eras->items : NULL;
    size_t i;

    if (!items || !date)
        return NULL;
    for (i = 0u; i < eras->count; ++i) {
        short boundary_year;
        month_t boundary_month;
        uint8_t boundary_day;
        int cmp;

        if (items[i].until_year == 0)
            return &items[i];
        boundary_year = (short)items[i].until_year;
        boundary_month = (month_t)(items[i].until_month ? items[i].until_month : 1);
        if (!resolve_month_day(boundary_year,
                               boundary_month,
                               items[i].until_day_kind ? items[i].until_day_kind : "day_of_month",
                               items[i].until_day_value ? items[i].until_day_value : 1,
                               items[i].until_weekday,
                               &boundary_day))
            return NULL;
        cmp = compare_local_noon_to_boundary(date,
                                             boundary_year,
                                             boundary_month,
                                             boundary_day,
                                             items[i].until_seconds);
        if (cmp < 0)
            return &items[i];
    }
    return eras->count ? &items[eras->count - 1u] : NULL;
}

static bool compute_transition_occurrence(const timezone_transition_rule_row_t *rule,
                                          int year,
                                          short *out_year,
                                          month_t *out_month,
                                          uint8_t *out_day,
                                          int *out_seconds)
{
    uint8_t day;
    short rule_year;
    month_t rule_month;
    int rule_seconds;

    if (!rule || !out_year || !out_month || !out_day || !out_seconds)
        return false;
    if (!year_in_range(year, rule->from_year, rule->to_year))
        return false;

    rule_year = (short)year;
    rule_month = (month_t)rule->in_month;
    if (!resolve_month_day(rule_year,
                           rule_month,
                           rule->on_kind ? rule->on_kind : "day_of_month",
                           rule->on_day,
                           rule->on_weekday,
                           &day))
        return false;

    rule_seconds = rule->at_seconds;
    if (!normalise_day_time(&rule_year, &rule_month, &day, &rule_seconds))
        return false;

    *out_year = rule_year;
    *out_month = rule_month;
    *out_day = day;
    *out_seconds = rule_seconds;
    return true;
}

static int compare_boundary_to_boundary(short left_year,
                                        month_t left_month,
                                        uint8_t left_day,
                                        int left_seconds,
                                        short right_year,
                                        month_t right_month,
                                        uint8_t right_day,
                                        int right_seconds)
{
    datetime_t *left = NULL;
    datetime_t *right = NULL;
    int cmp;

    left = datetime_init_ymd(datetime_alloc(), left_year, left_month, left_day);
    right = datetime_init_ymd(datetime_alloc(), right_year, right_month, right_day);
    if (!left || !right) {
        datetime_dealloc(right);
        datetime_dealloc(left);
        return 0;
    }
    cmp = datetime_compare(left, right);
    datetime_dealloc(right);
    datetime_dealloc(left);
    if (cmp != 0)
        return cmp;
    if (left_seconds < right_seconds)
        return -1;
    if (left_seconds > right_seconds)
        return 1;
    return 0;
}

static int transition_occurrence_compare(const void *lhs, const void *rhs)
{
    const timezone_transition_occurrence_t *left = lhs;
    const timezone_transition_occurrence_t *right = rhs;

    return compare_boundary_to_boundary(left->year,
                                        left->month,
                                        left->day,
                                        left->seconds,
                                        right->year,
                                        right->month,
                                        right->day,
                                        right->seconds);
}

static bool materialise_transition_display_datetime(const timezone_transition_occurrence_t *occurrence,
                                                    double previous_offset_hours,
                                                    datetime_t **out)
{
    short year;
    month_t month;
    uint8_t day;
    int seconds;
    double standard_offset_hours;
    double previous_save_hours;

    if (out)
        *out = NULL;
    if (!occurrence || !out)
        return false;

    year = occurrence->year;
    month = occurrence->month;
    day = occurrence->day;
    seconds = occurrence->seconds;
    standard_offset_hours = (double)occurrence->gmtoff_minutes / 60.0;
    previous_save_hours = previous_offset_hours - standard_offset_hours;

    switch (occurrence->at_suffix) {
    case 'u':
    case 'g':
    case 'z':
        seconds += (int)lround(previous_offset_hours * 3600.0);
        break;
    case 's':
        seconds += (int)lround(previous_save_hours * 3600.0);
        break;
    case 'w':
    default:
        break;
    }

    if (!normalise_day_time(&year, &month, &day, &seconds))
        return false;
    *out = datetime_init_ymdt(datetime_alloc(),
                              year,
                              month,
                              day,
                              (uint8_t)(seconds / 3600),
                              (uint8_t)((seconds % 3600) / 60),
                              (double)(seconds % 60));
    return *out != NULL;
}

static bool resolve_named_rule_save_minutes(sqlite_t *db,
                                            const char *rule_name,
                                            const datetime_t *date,
                                            int *save_minutes)
{
    pointer_vec_t rule_rows = {0};
    const timezone_transition_rule_row_t *items;
    int probe_years[2];
    size_t i;
    bool found = false;
    short best_year = 0;
    month_t best_month = 0;
    uint8_t best_day = 0;
    int best_seconds = 0;
    int best_save = 0;

    if (!db || !rule_name || !*rule_name || !date || !save_minutes)
        return false;

    rule_rows.item_size = sizeof(timezone_transition_rule_row_t);
    if (!load_timezone_transition_rules(db, rule_name, &rule_rows))
        return false;
    items = rule_rows.items;
    probe_years[0] = (int)datetime_year(date) - 1;
    probe_years[1] = (int)datetime_year(date);

    for (i = 0u; i < rule_rows.count; ++i) {
        size_t year_index;

        for (year_index = 0u; year_index < 2u; ++year_index) {
            short occurrence_year;
            month_t occurrence_month;
            uint8_t occurrence_day;
            int occurrence_seconds;
            int candidate_cmp;

            if (!compute_transition_occurrence(&items[i],
                                               probe_years[year_index],
                                               &occurrence_year,
                                               &occurrence_month,
                                               &occurrence_day,
                                               &occurrence_seconds))
                continue;

            candidate_cmp = compare_local_noon_to_boundary(date,
                                                           occurrence_year,
                                                           occurrence_month,
                                                           occurrence_day,
                                                           occurrence_seconds);
            if (candidate_cmp < 0)
                continue;

            if (!found ||
                compare_boundary_to_boundary(occurrence_year,
                                            occurrence_month,
                                            occurrence_day,
                                            occurrence_seconds,
                                            best_year,
                                            best_month,
                                            best_day,
                                            best_seconds) > 0) {
                found = true;
                best_year = occurrence_year;
                best_month = occurrence_month;
                best_day = occurrence_day;
                best_seconds = occurrence_seconds;
                best_save = items[i].save_minutes;
            }
        }
    }

    free_timezone_transition_rule_rows(&rule_rows);
    *save_minutes = found ? best_save : 0;
    return true;
}

static bool timezone_offset_for_name_on_date(sqlite_t *db,
                                             const char *timezone_name,
                                             const datetime_t *date,
                                             double *offset_hours)
{
    pointer_vec_t era_rows = {0};
    const timezone_era_row_t *era;
    int save_minutes = 0;

    if (!db || !timezone_name || !*timezone_name || !date || !offset_hours)
        return false;

    era_rows.item_size = sizeof(timezone_era_row_t);
    if (!load_timezone_eras(db, timezone_name, &era_rows))
        return false;
    era = select_timezone_era_for_date(&era_rows, date);
    if (!era) {
        free_timezone_era_rows(&era_rows);
        return false;
    }

    if (strcmp(era->rules_kind, "fixed") == 0) {
        save_minutes = era->fixed_save_minutes;
    } else if (strcmp(era->rules_kind, "named") == 0) {
        if (!resolve_named_rule_save_minutes(db, era->rule_name, date, &save_minutes)) {
            free_timezone_era_rows(&era_rows);
            return false;
        }
    } else {
        save_minutes = 0;
    }

    *offset_hours = (double)(era->gmtoff_minutes + save_minutes) / 60.0;
    free_timezone_era_rows(&era_rows);
    return true;
}

static bool load_holiday_rules(sqlite_t *db,
                               const pointer_vec_t *lineage_rows,
                               int start_year,
                               int end_year,
                               pointer_vec_t *rules)
{
    static const char sql[] =
        "select hd.holiday_id, hr.holiday_rule_id, hd.jurisdiction_id, "
        "       coalesce(hn.localized_name, hd.default_name), hd.holiday_class, "
        "       hr.rule_kind, hr.month, hr.day, hr.weekday, hr.ordinal, hr.offset_days, "
        "       hr.holiday_date, hr.expression_language, hr.expression_text, "
        "       hr.valid_from_year, hr.valid_to_year "
        "from holiday_definition hd "
        "join holiday_rule hr on hr.holiday_id = hd.holiday_id "
        "left join holiday_name hn on hn.holiday_id = hd.holiday_id and hn.is_primary = 'Y' "
        "where hd.jurisdiction_id = ?1 "
        "  and (hr.valid_from_year is null or hr.valid_from_year <= ?2) "
        "  and (hr.valid_to_year is null or hr.valid_to_year >= ?3) "
        "order by hd.holiday_id, hr.priority, hr.sequence_no;";
    sqlite_stmt_t *stmt = NULL;
    const lineage_row_t *lineage = lineage_rows ? lineage_rows->items : NULL;
    size_t i;

    if (!lineage)
        return false;
    stmt = sqlite_stmt_prepare(db, sql);
    if (!stmt)
        return false;

    for (i = 0; i < lineage_rows->count; ++i) {
        sqlite_step_result_t rc;

        sqlite_stmt_reset(stmt);
        sqlite_stmt_clear_bindings(stmt);
        if (!sqlite_stmt_bind_text(stmt, 1, lineage[i].jurisdiction_id) ||
            !sqlite_stmt_bind_int(stmt, 2, end_year) ||
            !sqlite_stmt_bind_int(stmt, 3, start_year))
            goto fail;
        while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
            holiday_rule_row_t row;

            memset(&row, 0, sizeof(row));
            row.holiday_id = sqlite_stmt_column_int(stmt, 0);
            row.rule_id = sqlite_stmt_column_int(stmt, 1);
            snprintf(row.jurisdiction_id, sizeof(row.jurisdiction_id), "%s", sqlite_stmt_column_text(stmt, 2));
            row.holiday_name = dup_text(sqlite_stmt_column_text(stmt, 3));
            row.holiday_class = dup_text(sqlite_stmt_column_text(stmt, 4));
            row.rule_kind = dup_text(sqlite_stmt_column_text(stmt, 5));
            row.month = sqlite_stmt_column_is_null(stmt, 6) ? 0 : sqlite_stmt_column_int(stmt, 6);
            row.day = sqlite_stmt_column_is_null(stmt, 7) ? 0 : sqlite_stmt_column_int(stmt, 7);
            row.weekday = sqlite_stmt_column_is_null(stmt, 8) ? 0 : sqlite_stmt_column_int(stmt, 8);
            row.ordinal = sqlite_stmt_column_is_null(stmt, 9) ? 0 : sqlite_stmt_column_int(stmt, 9);
            row.offset_days = sqlite_stmt_column_is_null(stmt, 10) ? 0 : sqlite_stmt_column_int(stmt, 10);
            row.holiday_date = dup_text(sqlite_stmt_column_text(stmt, 11));
            row.expression_language = dup_text(sqlite_stmt_column_text(stmt, 12));
            row.expression_text = dup_text(sqlite_stmt_column_text(stmt, 13));
            row.valid_from_year = sqlite_stmt_column_is_null(stmt, 14) ? 0 : sqlite_stmt_column_int(stmt, 14);
            row.valid_to_year = sqlite_stmt_column_is_null(stmt, 15) ? 0 : sqlite_stmt_column_int(stmt, 15);
            if (!row.holiday_name || !row.holiday_class || !row.rule_kind || !vec_push(rules, &row))
                goto fail;
        }
        if (rc != SQLITE_STEP_DONE)
            goto fail;
    }

    sqlite_stmt_finalize(stmt);
    return true;

fail:
    sqlite_stmt_finalize(stmt);
    return false;
}

static bool load_observance_rules(sqlite_t *db, const pointer_vec_t *lineage_rows, pointer_vec_t *observances)
{
    static const char sql[] =
        "select hor.holiday_id, hor.holiday_rule_id, hor.observed_rule_kind, hor.observed_name, "
        "       hor.weekend_mask, hor.suppress_original, hor.valid_from_year, hor.valid_to_year "
        "from holiday_observance_rule hor "
        "join holiday_definition hd on hd.holiday_id = hor.holiday_id "
        "where hd.jurisdiction_id = ?1 "
        "order by hor.holiday_id, hor.priority;";
    sqlite_stmt_t *stmt = NULL;
    const lineage_row_t *lineage = lineage_rows ? lineage_rows->items : NULL;
    size_t i;

    if (!lineage)
        return false;
    stmt = sqlite_stmt_prepare(db, sql);
    if (!stmt)
        return false;

    for (i = 0; i < lineage_rows->count; ++i) {
        sqlite_step_result_t rc;

        sqlite_stmt_reset(stmt);
        sqlite_stmt_clear_bindings(stmt);
        if (!sqlite_stmt_bind_text(stmt, 1, lineage[i].jurisdiction_id))
            goto fail;
        while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
            observance_rule_row_t row;

            memset(&row, 0, sizeof(row));
            row.holiday_id = sqlite_stmt_column_int(stmt, 0);
            row.applies_to_rule_id = sqlite_stmt_column_is_null(stmt, 1) ? 0 : sqlite_stmt_column_int(stmt, 1);
            row.observed_rule_kind = dup_text(sqlite_stmt_column_text(stmt, 2));
            row.observed_name = dup_text(sqlite_stmt_column_text(stmt, 3));
            row.weekend_mask = normalise_weekend_mask(sqlite_stmt_column_text(stmt, 4));
            row.suppress_original = text_equals(sqlite_stmt_column_text(stmt, 5), "Y");
            row.valid_from_year = sqlite_stmt_column_is_null(stmt, 6) ? 0 : sqlite_stmt_column_int(stmt, 6);
            row.valid_to_year = sqlite_stmt_column_is_null(stmt, 7) ? 0 : sqlite_stmt_column_int(stmt, 7);
            if (!row.observed_rule_kind || !row.weekend_mask || !vec_push(observances, &row))
                goto fail;
        }
        if (rc != SQLITE_STEP_DONE)
            goto fail;
    }

    sqlite_stmt_finalize(stmt);
    return true;

fail:
    sqlite_stmt_finalize(stmt);
    return false;
}

static bool load_exceptions(sqlite_t *db, const pointer_vec_t *lineage_rows, pointer_vec_t *exceptions)
{
    static const char sql[] =
        "select holiday_id, holiday_rule_id, holiday_date, action, name, valid_from_year, valid_to_year "
        "from holiday_exception where jurisdiction_id = ?1 order by holiday_date, priority;";
    sqlite_stmt_t *stmt = NULL;
    const lineage_row_t *lineage = lineage_rows ? lineage_rows->items : NULL;
    size_t i;

    if (!lineage)
        return false;
    stmt = sqlite_stmt_prepare(db, sql);
    if (!stmt)
        return false;

    for (i = 0; i < lineage_rows->count; ++i) {
        sqlite_step_result_t rc;

        sqlite_stmt_reset(stmt);
        sqlite_stmt_clear_bindings(stmt);
        if (!sqlite_stmt_bind_text(stmt, 1, lineage[i].jurisdiction_id))
            goto fail;
        while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
            holiday_exception_row_t row;

            memset(&row, 0, sizeof(row));
            row.holiday_id = sqlite_stmt_column_is_null(stmt, 0) ? 0 : sqlite_stmt_column_int(stmt, 0);
            row.target_rule_id = sqlite_stmt_column_is_null(stmt, 1) ? 0 : sqlite_stmt_column_int(stmt, 1);
            row.holiday_date = dup_text(sqlite_stmt_column_text(stmt, 2));
            row.action = dup_text(sqlite_stmt_column_text(stmt, 3));
            row.name = dup_text(sqlite_stmt_column_text(stmt, 4));
            row.valid_from_year = sqlite_stmt_column_is_null(stmt, 5) ? 0 : sqlite_stmt_column_int(stmt, 5);
            row.valid_to_year = sqlite_stmt_column_is_null(stmt, 6) ? 0 : sqlite_stmt_column_int(stmt, 6);
            if (!row.holiday_date || !row.action || !vec_push(exceptions, &row))
                goto fail;
        }
        if (rc != SQLITE_STEP_DONE)
            goto fail;
    }

    sqlite_stmt_finalize(stmt);
    return true;

fail:
    sqlite_stmt_finalize(stmt);
    return false;
}

static char *evaluate_rule_date(sqlite_t *db,
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

static bool holiday_events_equivalent(const holiday_event_row_t *lhs,
                                      const holiday_event_row_t *rhs)
{
    if (!lhs || !rhs)
        return false;
    return text_compare(lhs->holiday_date, rhs->holiday_date) == 0 &&
           text_compare(lhs->holiday_name, rhs->holiday_name) == 0 &&
           text_compare(lhs->holiday_class, rhs->holiday_class) == 0;
}

static void dedupe_sorted_events(pointer_vec_t *events)
{
    holiday_event_row_t *items = events ? events->items : NULL;
    holiday_event_row_t *previous = NULL;
    size_t i;

    if (!items)
        return;

    for (i = 0u; i < events->count; ++i) {
        if (items[i].removed)
            continue;
        if (previous && holiday_events_equivalent(previous, &items[i])) {
            items[i].removed = true;
            continue;
        }
        previous = &items[i];
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
    path = string_new_from_cstr(resolved_path);
    key = string_new_from_cstr(resolved_key);
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

void jurisdict_close(jurisdiction_t *jurisdiction)
{
    if (!jurisdiction)
        return;
    sqlite_close(jurisdiction->db);
    string_free(jurisdiction->error);
    free(jurisdiction);
}

const char *jurisdict_last_error(const jurisdiction_t *jurisdiction)
{
    return (jurisdiction && jurisdiction->error) ? string_c_str(jurisdiction->error) : NULL;
}

bool jurisdict_default_location(jurisdiction_t *holiday,
                              double *latitude,
                              double *longitude)
{
    sqlite_t *db = holiday ? holiday->db : NULL;
    const char *jurisdiction = holiday ? holiday->jurisdiction : NULL;
    pointer_vec_t lineage_rows = {0};
    bool ok;

    lineage_rows.item_size = sizeof(lineage_row_t);

    if (!holiday || !db || !jurisdiction || *jurisdiction == '\0' || !latitude || !longitude) {
        jurisdiction_set_error(holiday, "invalid holiday query");
        return false;
    }
    if (!load_lineage(db, jurisdiction, &lineage_rows)) {
        jurisdiction_set_error(holiday, "failed to load jurisdiction lineage");
        vec_free(&lineage_rows);
        return false;
    }
    ok = load_default_location(db, &lineage_rows, latitude, longitude);
    vec_free(&lineage_rows);
    if (!ok)
        jurisdiction_set_error(holiday, "default jurisdiction location unavailable");
    return ok;
}

bool jurisdict_default_gmt_offset(jurisdiction_t *holiday,
                                const datetime_t *date,
                                double *offset_hours)
{
    sqlite_t *db = holiday ? holiday->db : NULL;
    const char *jurisdiction = holiday ? holiday->jurisdiction : NULL;
    pointer_vec_t lineage_rows = {0};
    char timezone_name[128];
    bool ok;

    lineage_rows.item_size = sizeof(lineage_row_t);

    if (!holiday || !db || !jurisdiction || *jurisdiction == '\0' || !date || !offset_hours) {
        jurisdiction_set_error(holiday, "invalid holiday query");
        return false;
    }
    if (!load_lineage(db, jurisdiction, &lineage_rows)) {
        jurisdiction_set_error(holiday, "failed to load jurisdiction lineage");
        vec_free(&lineage_rows);
        return false;
    }
    ok = load_default_timezone_name(db, &lineage_rows, timezone_name, sizeof(timezone_name));
    vec_free(&lineage_rows);
    if (!ok) {
        jurisdiction_set_error(holiday, "default jurisdiction timezone unavailable");
        return false;
    }
    if (!timezone_offset_for_name_on_date(db, timezone_name, date, offset_hours)) {
        jurisdiction_set_error(holiday, "failed to resolve jurisdiction timezone offset");
        return false;
    }
    return true;
}

static bool timezone_load_named_rule_names_for_year(sqlite_t *db,
                                                    const char *timezone_name,
                                                    int year,
                                                    pointer_vec_t *rule_names)
{
    pointer_vec_t era_rows = {0};
    datetime_t *jan1 = NULL;
    datetime_t *dec31 = NULL;
    const timezone_era_row_t *jan_era;
    const timezone_era_row_t *dec_era;
    timezone_named_rule_ref_t ref;
    size_t i;
    bool ok = false;

    if (!db || !timezone_name || !*timezone_name || !rule_names || year < 1 || year > 9999)
        return false;

    era_rows.item_size = sizeof(timezone_era_row_t);
    jan1 = datetime_init_ymd(datetime_alloc(), (short)year, DT_January, 1u);
    dec31 = datetime_init_ymd(datetime_alloc(), (short)year, DT_December, 31u);
    if (!jan1 || !dec31)
        goto done;
    if (!load_timezone_eras(db, timezone_name, &era_rows))
        goto done;
    jan_era = select_timezone_era_for_date(&era_rows, jan1);
    dec_era = select_timezone_era_for_date(&era_rows, dec31);

    if (jan_era && jan_era->rules_kind &&
        strcmp(jan_era->rules_kind, "named") == 0 &&
        jan_era->rule_name && *jan_era->rule_name) {
        memset(&ref, 0, sizeof(ref));
        ref.rule_name = dup_c_string(jan_era->rule_name);
        ref.gmtoff_minutes = jan_era->gmtoff_minutes;
        if (!ref.rule_name || !vec_push(rule_names, &ref)) {
            free(ref.rule_name);
            goto done;
        }
    }

    if (dec_era && dec_era->rules_kind &&
        strcmp(dec_era->rules_kind, "named") == 0 &&
        dec_era->rule_name && *dec_era->rule_name) {
        bool seen = false;
        const timezone_named_rule_ref_t *names = rule_names->items;

        for (i = 0u; names && i < rule_names->count; ++i) {
            if (names[i].rule_name && strcmp(names[i].rule_name, dec_era->rule_name) == 0) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            memset(&ref, 0, sizeof(ref));
            ref.rule_name = dup_c_string(dec_era->rule_name);
            ref.gmtoff_minutes = dec_era->gmtoff_minutes;
            if (!ref.rule_name || !vec_push(rule_names, &ref)) {
                free(ref.rule_name);
                goto done;
            }
        }
    }

    ok = true;

done:
    datetime_dealloc(dec31);
    datetime_dealloc(jan1);
    free_timezone_era_rows(&era_rows);
    return ok;
}

static bool timezone_collect_transition_occurrences(sqlite_t *db,
                                                    const timezone_named_rule_ref_t *rule_ref,
                                                    int year,
                                                    pointer_vec_t *occurrences)
{
    pointer_vec_t rule_rows = {0};
    const timezone_transition_rule_row_t *items;
    size_t i;
    bool ok = false;

    if (!db || !rule_ref || !rule_ref->rule_name || !*rule_ref->rule_name || !occurrences || year < 1 || year > 9999)
        return false;

    rule_rows.item_size = sizeof(timezone_transition_rule_row_t);
    if (!load_timezone_transition_rules(db, rule_ref->rule_name, &rule_rows))
        return false;
    items = rule_rows.items;

    for (i = 0u; i < rule_rows.count; ++i) {
        timezone_transition_occurrence_t occurrence;

        memset(&occurrence, 0, sizeof(occurrence));
        if (!compute_transition_occurrence(&items[i],
                                           year,
                                           &occurrence.year,
                                           &occurrence.month,
                                           &occurrence.day,
                                           &occurrence.seconds))
            continue;
        occurrence.at_suffix = items[i].at_suffix;
        occurrence.gmtoff_minutes = rule_ref->gmtoff_minutes;
        if (!vec_push(occurrences, &occurrence))
            goto done;
    }

    ok = true;

done:
    free_timezone_transition_rule_rows(&rule_rows);
    return ok;
}

bool jurisdict_dst_transition_details(jurisdiction_t *holiday,
                                      int year,
                                      datetime_t **clocks_forward,
                                      double *forward_from_offset_hours,
                                      double *forward_to_offset_hours,
                                      datetime_t **clocks_back,
                                      double *back_from_offset_hours,
                                      double *back_to_offset_hours)
{
    sqlite_t *db = holiday ? holiday->db : NULL;
    const char *jurisdiction = holiday ? holiday->jurisdiction : NULL;
    pointer_vec_t lineage_rows = {0};
    char timezone_name[128];
    pointer_vec_t rule_names = {0};
    pointer_vec_t occurrences = {0};
    const timezone_named_rule_ref_t *rule_name_items = NULL;
    timezone_transition_occurrence_t *occurrence_items = NULL;
    size_t i;
    bool ok = false;

    if (clocks_forward)
        *clocks_forward = NULL;
    if (clocks_back)
        *clocks_back = NULL;
    if (forward_from_offset_hours)
        *forward_from_offset_hours = DBL_MAX;
    if (forward_to_offset_hours)
        *forward_to_offset_hours = DBL_MAX;
    if (back_from_offset_hours)
        *back_from_offset_hours = DBL_MAX;
    if (back_to_offset_hours)
        *back_to_offset_hours = DBL_MAX;

    lineage_rows.item_size = sizeof(lineage_row_t);
    rule_names.item_size = sizeof(timezone_named_rule_ref_t);
    occurrences.item_size = sizeof(timezone_transition_occurrence_t);

    if (!holiday || !db || !jurisdiction || *jurisdiction == '\0' || year < 1 || year > 9999) {
        jurisdiction_set_error(holiday, "invalid daylight-saving query");
        return false;
    }
    if (!load_lineage(db, jurisdiction, &lineage_rows)) {
        jurisdiction_set_error(holiday, "failed to load jurisdiction lineage");
        goto done;
    }
    if (!load_default_timezone_name(db, &lineage_rows, timezone_name, sizeof(timezone_name))) {
        jurisdiction_set_error(holiday, "default jurisdiction timezone unavailable");
        goto done;
    }
    if (!timezone_load_named_rule_names_for_year(db, timezone_name, year, &rule_names)) {
        jurisdiction_set_error(holiday, "failed to load daylight-saving rules");
        goto done;
    }

    rule_name_items = rule_names.items;
    for (i = 0u; rule_name_items && i < rule_names.count; ++i) {
        if (!timezone_collect_transition_occurrences(db, &rule_name_items[i], year, &occurrences)) {
            jurisdiction_set_error(holiday, "failed to collect daylight-saving transitions");
            goto done;
        }
    }

    occurrence_items = occurrences.items;
    if (occurrence_items && occurrences.count > 1u)
        qsort(occurrence_items, occurrences.count, sizeof(*occurrence_items), transition_occurrence_compare);

    for (i = 0u; occurrence_items && i < occurrences.count; ++i) {
        datetime_t *transition_date = NULL;
        datetime_t *previous_date = NULL;
        datetime_t *transition_probe = NULL;
        double previous_offset = 0.0;
        double current_offset = 0.0;
        double delta;

        previous_date = datetime_init_ymd(datetime_alloc(),
                                          occurrence_items[i].year,
                                          occurrence_items[i].month,
                                          occurrence_items[i].day);
        transition_probe = datetime_init_ymd(datetime_alloc(),
                                             occurrence_items[i].year,
                                             occurrence_items[i].month,
                                             occurrence_items[i].day);
        if (!previous_date || !transition_probe) {
            datetime_dealloc(transition_probe);
            datetime_dealloc(previous_date);
            jurisdiction_set_error(holiday, "failed to materialise daylight-saving transition date");
            goto done;
        }
        if (!datetime_add_days(previous_date, -1) ||
            !timezone_offset_for_name_on_date(db, timezone_name, previous_date, &previous_offset) ||
            !timezone_offset_for_name_on_date(db, timezone_name, transition_probe, &current_offset)) {
            datetime_dealloc(transition_probe);
            datetime_dealloc(previous_date);
            jurisdiction_set_error(holiday, "failed to evaluate daylight-saving transition");
            goto done;
        }
        datetime_dealloc(transition_probe);
        transition_probe = NULL;
        datetime_dealloc(previous_date);
        previous_date = NULL;

        if (!materialise_transition_display_datetime(&occurrence_items[i],
                                                     previous_offset,
                                                     &transition_date)) {
            jurisdiction_set_error(holiday, "failed to materialise daylight-saving transition time");
            goto done;
        }

        delta = current_offset - previous_offset;
        if (delta > 0.0) {
            if (clocks_forward && !*clocks_forward) {
                *clocks_forward = transition_date;
                if (forward_from_offset_hours)
                    *forward_from_offset_hours = previous_offset;
                if (forward_to_offset_hours)
                    *forward_to_offset_hours = current_offset;
            } else
                datetime_dealloc(transition_date);
        } else if (delta < 0.0) {
            if (clocks_back && !*clocks_back) {
                *clocks_back = transition_date;
                if (back_from_offset_hours)
                    *back_from_offset_hours = previous_offset;
                if (back_to_offset_hours)
                    *back_to_offset_hours = current_offset;
            } else
                datetime_dealloc(transition_date);
        } else {
            datetime_dealloc(transition_date);
        }
    }

    ok = true;

done:
    if (!ok) {
        if (clocks_forward) {
            datetime_dealloc(*clocks_forward);
            *clocks_forward = NULL;
        }
        if (clocks_back) {
            datetime_dealloc(*clocks_back);
            *clocks_back = NULL;
        }
        if (forward_from_offset_hours)
            *forward_from_offset_hours = DBL_MAX;
        if (forward_to_offset_hours)
            *forward_to_offset_hours = DBL_MAX;
        if (back_from_offset_hours)
            *back_from_offset_hours = DBL_MAX;
        if (back_to_offset_hours)
            *back_to_offset_hours = DBL_MAX;
    }
    free_timezone_named_rule_refs(&rule_names);
    vec_free(&occurrences);
    vec_free(&lineage_rows);
    return ok;
}

bool jurisdict_dst_transition_datetimes(jurisdiction_t *holiday,
                                        int year,
                                        datetime_t **clocks_forward,
                                        datetime_t **clocks_back)
{
    return jurisdict_dst_transition_details(holiday,
                                            year,
                                            clocks_forward,
                                            NULL,
                                            NULL,
                                            clocks_back,
                                            NULL,
                                            NULL);
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

array_t *jurisdict_holidays_between(jurisdiction_t *holiday,
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
    if (!jurisdict_each_holiday_between(holiday, start, end, holiday_collect_event, &ctx)) {
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

bool jurisdict_is_national_holiday(jurisdiction_t *holiday, const datetime_t *date)
{
    array_t *events;
    bool is_holiday;

    if (!holiday || !date) {
        jurisdiction_set_error(holiday, "invalid holiday query");
        return false;
    }
    events = jurisdict_holidays_between(holiday, date, date);
    if (!events)
        return false;
    is_holiday = array_size(events) > 0u;
    array_destroy(events);
    return is_holiday;
}

bool jurisdict_is_weekend(jurisdiction_t *holiday, const datetime_t *date)
{
    sqlite_t *db = holiday ? holiday->db : NULL;
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
        jurisdiction_set_error(holiday, "invalid holiday query");
        return false;
    }
    if (!load_lineage(db, jurisdiction, &lineage_rows) ||
        !load_weekend_rules(db, &lineage_rows, &weekend_rules)) {
        jurisdiction_set_error(holiday, "failed to load weekend rules");
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

long jurisdict_working_days_between(jurisdiction_t *holiday,
                                  const datetime_t *start,
                                  const datetime_t *end)
{
    array_t *events;
    datetime_t *cursor = NULL;
    long working_days = 0;

    if (!holiday || !start || !end || datetime_compare(start, end) > 0) {
        jurisdiction_set_error(holiday, "invalid holiday query");
        return -1;
    }

    events = jurisdict_holidays_between(holiday, start, end);
    if (!events)
        return -1;

    cursor = datetime_init_copy(datetime_alloc(), start);
    if (!cursor) {
        array_destroy(events);
        jurisdiction_set_error(holiday, "failed to initialise working day cursor");
        return -1;
    }

    while (datetime_compare(cursor, end) <= 0) {
        if (!jurisdict_is_weekend(holiday, cursor) &&
            !holiday_has_event_on_date(events, cursor)) {
            working_days++;
        }
        if (!datetime_add_days(cursor, 1)) {
            working_days = -1;
            jurisdiction_set_error(holiday, "failed to advance working day cursor");
            break;
        }
    }

    datetime_dealloc(cursor);
    array_destroy(events);
    return working_days;
}

bool jurisdict_each_holiday_between(jurisdiction_t *holiday,
                                    const datetime_t *start,
                                    const datetime_t *end,
                                    jurisdict_visit_fn visitor,
                                    void *ctx)
{
    sqlite_t *db = holiday ? holiday->db : NULL;
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
        jurisdiction_set_error(holiday, "invalid holiday query");
        return false;
    }

    start_text = format_date(start);
    end_text = format_date(end);
    if (!start_text || !end_text) {
        jurisdiction_set_error(holiday, "failed to format date range");
        goto done;
    }

    if (!load_lineage(db, jurisdiction, &lineage_rows) ||
        !load_weekend_rules(db, &lineage_rows, &weekend_rules) ||
        !load_holiday_rules(db, &lineage_rows, datetime_year(start), datetime_year(end), &rules) ||
        !load_observance_rules(db, &lineage_rows, &observances) ||
        !load_exceptions(db, &lineage_rows, &exceptions)) {
        jurisdiction_set_error(holiday, "failed to load holiday rules");
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
                    jurisdiction_set_error(holiday, "failed to collect holiday event");
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
    dedupe_sorted_events(&events);
    for (i = 0; i < events.count; ++i) {
        holiday_event_t public_event;
        datetime_t *public_date = NULL;
        short year_value;
        month_t month_value;
        uint8_t day_value;

        if (event_items[i].removed)
            continue;
        if (!parse_date_text(event_items[i].holiday_date, &year_value, &month_value, &day_value)) {
            jurisdiction_set_error(holiday, "failed to parse holiday date");
            goto done;
        }
        public_date = datetime_init_ymd(datetime_alloc(), year_value, month_value, day_value);
        if (!public_date) {
            jurisdiction_set_error(holiday, "failed to materialise holiday date");
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

bool jurisdict_serialize(const jurisdiction_t *jurisdiction,
                         string_t **out_type,
                         string_t **out_encoding,
                         void **out_data,
                         size_t *out_len)
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

jurisdiction_t *jurisdict_deserialise(const void *data,
                                      size_t len,
                                      const string_t *type,
                                      const string_t *encoding)
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
