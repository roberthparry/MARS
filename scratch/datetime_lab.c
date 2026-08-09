#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "almanac.h"
#include "array.h"
#include "datetime.h"
#include "jurisdiction.h"
#include "sqlite.h"
#include "ustring.h"

typedef struct datetime_lab_options_t {
    short date_year;
    month_t date_month;
    uint8_t date_day;
    long julian_day_number;
    bool use_julian_day_number;
    short start_year;
    month_t start_month;
    uint8_t start_day;
    short end_year;
    month_t end_month;
    uint8_t end_day;
    int year;
    double latitude;
    double longitude;
    double elevation_metres;
    double gmt_offset;
    char jurisdiction[32];
    bool cache_only;
    bool cache_put;
} datetime_lab_options_t;

static double monotonic_seconds(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static void trace_timing(const char *label, double *last_seconds)
{
    double now;

    if (!getenv("MARS_DATETIME_LAB_TIMING") || !label || !last_seconds)
        return;
    now = monotonic_seconds();
    fprintf(stderr, "timing %-28s %.3f ms\n", label, (now - *last_seconds) * 1000.0);
    *last_seconds = now;
}

#define DATETIME_LAB_CACHE_SCHEMA "datetime_lab_output_v4"
#define DATETIME_LAB_CACHE_PATH_ENV "MARS_LAB_OBJECT_STORE_PATH"
#define DATETIME_LAB_CACHE_KEY_ENV "MARS_LAB_OBJECT_STORE_KEY"

typedef struct calendar_section_row_t {
    char label[96];
    char value[160];
    const datetime_t *sort_date;
    int sequence;
} calendar_section_row_t;

typedef datetime_t *(*datetime_year_init_fn)(datetime_t *dttm, int year);
typedef string_t *(*calendar_text_fn)(const datetime_t *dttm);

typedef enum calendar_section_kind_t {
    CALENDAR_SECTION_CHRISTIAN = 0,
    CALENDAR_SECTION_CHINESE,
    CALENDAR_SECTION_HINDU,
    CALENDAR_SECTION_BUDDHIST,
    CALENDAR_SECTION_MUSLIM,
    CALENDAR_SECTION_JEWISH,
    CALENDAR_SECTION_CHEROKEE,
    CALENDAR_SECTION_MAYAN,
    CALENDAR_SECTION_AZTEC,
    CALENDAR_SECTION_ETHIOPIAN,
    CALENDAR_SECTION_COUNT
} calendar_section_kind_t;

typedef struct datetime_event_descriptor_t {
    const char *field_name;
    const char *section_label;
    datetime_year_init_fn init_fn;
    int sequence;
    const char *start_gmt_field_name;
    const char *start_gmt_section_label;
    int start_gmt_sequence;
} datetime_event_descriptor_t;

typedef struct calendar_section_descriptor_t {
    const char *field_name;
    calendar_text_fn current_text_fn;
    const datetime_event_descriptor_t *events;
    size_t event_count;
} calendar_section_descriptor_t;

typedef struct datetime_event_runtime_t {
    const datetime_event_descriptor_t *descriptor;
    datetime_t *value;
    datetime_t *start_local;
} datetime_event_runtime_t;

typedef struct calendar_section_runtime_t {
    const calendar_section_descriptor_t *descriptor;
    datetime_event_runtime_t *events;
    size_t event_count;
} calendar_section_runtime_t;

#define ARRAY_LEN(values) (sizeof(values) / sizeof((values)[0]))

static const datetime_event_descriptor_t christian_events[] = {
    {"easter", "Easter Sunday", datetime_init_easter, 0, NULL, NULL, 0},
    {"orthodox_easter", "Orthodox Easter Sunday", datetime_init_orthodox_easter, 1, NULL, NULL, 0},
    {"christmas", "Christmas Day", datetime_init_christmas, 2, NULL, NULL, 0},
    {"orthodox_christmas", "Orthodox Christmas Day", datetime_init_orthodox_christmas, 3, NULL, NULL, 0},
};

static const datetime_event_descriptor_t chinese_events[] = {
    {"chinese_new_year", "New Year", datetime_init_chinese_new_year, 0, NULL, NULL, 0},
};

static const datetime_event_descriptor_t hindu_events[] = {
    {"holi", "Holi (estimated)", datetime_init_holi, 0, NULL, NULL, 0},
    {"hindu_new_year", "Hindu New Year (estimated)", datetime_init_hindu_new_year, 1, NULL, NULL, 0},
    {"diwali", "Diwali (estimated)", datetime_init_diwali, 2, NULL, NULL, 0},
};

static const datetime_event_descriptor_t buddhist_events[] = {
    {"buddhist_new_year", "Buddhist New Year (estimated)", datetime_init_buddhist_new_year, 0, NULL, NULL, 0},
    {"vesak", "Vesak / Buddha Day (estimated)", datetime_init_vesak, 1, NULL, NULL, 0},
    {"asalha_puja", "Asalha Puja / Dharma Day (estimated)", datetime_init_asalha_puja, 2, NULL, NULL, 0},
};

static const datetime_event_descriptor_t muslim_events[] = {
    {"ramadan", "Ramadan begins (civil Islamic)", datetime_init_ramadan, 0, "ramadan_starts_local",
     "Ramadan begins at sunset (local time)", 1},
    {"eid_al_fitr", "Eid al-Fitr (civil Islamic)", datetime_init_eid_al_fitr, 2, "eid_al_fitr_starts_local",
     "Eid al-Fitr begins at sunset (local time)", 3},
    {"muslim_new_year", "Muslim New Year (civil Islamic)", datetime_init_muslim_new_year, 4,
     "muslim_new_year_starts_local", "Muslim New Year begins at sunset (local time)", 5},
};

static const datetime_event_descriptor_t jewish_events[] = {
    {"passover", "Passover", datetime_init_passover, 0, "passover_starts_local",
     "Passover begins at sunset (local time)", 1},
    {"jewish_new_year", "Jewish New Year", datetime_init_jewish_new_year, 2, "jewish_new_year_starts_local",
     "Jewish New Year begins at sunset (local time)", 3},
};

static const datetime_event_descriptor_t cherokee_events[] = {
    {"cherokee_new_moon_festival", "New Moon Festival (estimated)", datetime_init_cherokee_new_moon_festival, 0, NULL,
     NULL, 0},
    {"cherokee_green_corn_ceremony", "Green Corn Ceremony (estimated)", datetime_init_cherokee_green_corn_ceremony, 1,
     NULL, NULL, 0},
    {"cherokee_ripe_corn_ceremony", "Ripe Corn Ceremony (estimated)", datetime_init_cherokee_ripe_corn_ceremony, 2,
     NULL, NULL, 0},
    {"cherokee_great_new_moon_festival", "Great New Moon Festival (estimated)",
     datetime_init_cherokee_great_new_moon_festival, 3, NULL, NULL, 0},
};

static const datetime_event_descriptor_t mayan_events[] = {
    {"mayan_wayeb_start", "Wayeb begins", datetime_init_mayan_wayeb_start, 0, NULL, NULL, 0},
    {"mayan_haab_new_year", "Haab New Year", datetime_init_mayan_haab_new_year, 1, NULL, NULL, 0},
};

static const datetime_event_descriptor_t aztec_events[] = {
    {"aztec_nemontemi_start", "Nemontemi begins", datetime_init_aztec_nemontemi_start, 0, NULL, NULL, 0},
    {"aztec_xiuhpohualli_new_year", "Xiuhpohualli New Year", datetime_init_aztec_xiuhpohualli_new_year, 1, NULL, NULL,
     0},
};

static const datetime_event_descriptor_t ethiopian_events[] = {
    {"genna", "Genna", datetime_init_genna, 0, NULL, NULL, 0},
    {"timkat", "Timkat", datetime_init_timkat, 1, NULL, NULL, 0},
    {"fasika", "Fasika", datetime_init_fasika, 2, NULL, NULL, 0},
    {"ethiopian_new_year", "Enkutatash", datetime_init_ethiopian_new_year, 3, NULL, NULL, 0},
    {"meskel", "Meskel", datetime_init_meskel, 4, NULL, NULL, 0},
};

static const calendar_section_descriptor_t calendar_sections[] = {
    {"calendar_section_christian", datetime_christian_calendar_date_text, christian_events,
     ARRAY_LEN(christian_events)},
    {"calendar_section_chinese", datetime_chinese_calendar_date_text, chinese_events, ARRAY_LEN(chinese_events)},
    {"calendar_section_hindu", datetime_hindu_calendar_date_text, hindu_events, ARRAY_LEN(hindu_events)},
    {"calendar_section_buddhist", datetime_buddhist_calendar_date_text, buddhist_events, ARRAY_LEN(buddhist_events)},
    {"calendar_section_muslim", datetime_muslim_calendar_date_text, muslim_events, ARRAY_LEN(muslim_events)},
    {"calendar_section_jewish", datetime_jewish_calendar_date_text, jewish_events, ARRAY_LEN(jewish_events)},
    {"calendar_section_cherokee", datetime_cherokee_calendar_date_text, cherokee_events, ARRAY_LEN(cherokee_events)},
    {"calendar_section_mayan", datetime_mayan_calendar_date_text, mayan_events, ARRAY_LEN(mayan_events)},
    {"calendar_section_aztec", datetime_aztec_calendar_date_text, aztec_events, ARRAY_LEN(aztec_events)},
    {"calendar_section_ethiopian", datetime_ethiopian_calendar_date_text, ethiopian_events,
     ARRAY_LEN(ethiopian_events)},
};

static bool parse_date_text(const char *text, short *year, month_t *month, uint8_t *day)
{
    datetime_t *parsed;

    if (!text || !year || !month || !day)
        return false;
    parsed = datetime_from_string(text);
    if (!parsed)
        return false;

    *year = datetime_year(parsed);
    *month = datetime_month(parsed);
    *day = datetime_day(parsed);
    datetime_dealloc(parsed);
    return true;
}

static bool parse_double_text(const char *text, double *out)
{
    char *end = NULL;
    double value;

    if (!text || !out)
        return false;
    while (isspace((unsigned char)*text))
        text++;
    if (*text == '\0')
        return false;

    value = strtod(text, &end);
    if (end == text || !isfinite(value))
        return false;
    while (isspace((unsigned char)*end))
        end++;
    if (*end != '\0')
        return false;

    *out = value;
    return true;
}

static bool parse_int_text(const char *text, int *out)
{
    char *end = NULL;
    long value;

    if (!text || !out)
        return false;
    while (isspace((unsigned char)*text))
        text++;
    if (*text == '\0')
        return false;

    value = strtol(text, &end, 10);
    if (end == text || value < 1 || value > 9999)
        return false;
    while (isspace((unsigned char)*end))
        end++;
    if (*end != '\0')
        return false;

    *out = (int)value;
    return true;
}

static bool parse_long_text(const char *text, long *out)
{
    char *end = NULL;
    long value;

    if (!text || !out)
        return false;
    while (isspace((unsigned char)*text))
        text++;
    if (*text == '\0')
        return false;

    value = strtol(text, &end, 10);
    if (end == text || value < 1 || value == LONG_MAX || value == LONG_MIN)
        return false;
    while (isspace((unsigned char)*end))
        end++;
    if (*end != '\0')
        return false;

    *out = value;
    return true;
}

static bool key_equals(const char *got, size_t got_len, const char *want)
{
    return strlen(want) == got_len && strncmp(got, want, got_len) == 0;
}

static sqlite_t *open_lab_object_store(void)
{
    const char *path = getenv(DATETIME_LAB_CACHE_PATH_ENV);
    const char *key = getenv(DATETIME_LAB_CACHE_KEY_ENV);
    string_t *path_text = NULL;
    string_t *key_text = NULL;
    sqlite_t *db = NULL;

    if (!path || !*path || !key || !*key)
        return NULL;
    path_text = string_new_with(path);
    key_text = string_new_with(key);
    if (!path_text || !key_text)
        goto done;
    db = sqlite_open_encrypted(path_text, key_text);
    if (!db) {
        (void)remove(path);
        db = sqlite_open_encrypted(path_text, key_text);
    }
    if (db)
        (void)chmod(path, S_IRUSR | S_IWUSR);
    if (db && !sqlite_init_object_store(db)) {
        sqlite_close(db);
        db = NULL;
    }

done:
    string_free(key_text);
    string_free(path_text);
    return db;
}

static string_t *cache_key_for_options(const datetime_lab_options_t *options)
{
    char key[512];
    char gmt_offset[48];

    if (!options)
        return NULL;
    if (options->gmt_offset == DBL_MAX)
        snprintf(gmt_offset, sizeof(gmt_offset), "local");
    else
        snprintf(gmt_offset, sizeof(gmt_offset), "%.9f", options->gmt_offset);
    if (snprintf(key, sizeof(key),
                 "mars_lab/%s/date=%04d-%02d-%02d/jdn=%s%ld/start=%04d-%02d-%02d/end=%04d-%02d-%02d/year=%d/"
                 "jurisdiction=%s/lat=%.9f/lon=%.9f/elevation=%.9f/gmt_offset=%s",
                 DATETIME_LAB_CACHE_SCHEMA, options->date_year, (int)options->date_month, (int)options->date_day,
                 options->use_julian_day_number ? "" : "none:",
                 options->use_julian_day_number ? options->julian_day_number : 0L, options->start_year,
                 (int)options->start_month, (int)options->start_day, options->end_year, (int)options->end_month,
                 (int)options->end_day, options->year, options->jurisdiction, options->latitude, options->longitude,
                 options->elevation_metres, gmt_offset) >= (int)sizeof(key)) {
        return NULL;
    }
    return string_new_with(key);
}

static string_t *load_cached_output(const datetime_lab_options_t *options)
{
    sqlite_t *db = NULL;
    string_t *name = NULL;
    string_t *output = NULL;

    name = cache_key_for_options(options);
    if (!name)
        return NULL;
    db = open_lab_object_store();
    if (!db)
        goto done;
    if (!sqlite_load_string(db, name, &output))
        output = NULL;

done:
    sqlite_close(db);
    string_free(name);
    return output;
}

static bool store_cached_output(const datetime_lab_options_t *options, const string_t *output)
{
    sqlite_t *db = NULL;
    string_t *name = NULL;
    bool stored = false;

    if (!output)
        return false;
    name = cache_key_for_options(options);
    if (!name)
        return false;
    db = open_lab_object_store();
    if (db) {
        stored = sqlite_store_string(db, name, output);
        if (!stored) {
            const string_t *error = sqlite_last_error(db);

            fprintf(stderr, "failed to store datetime cache object: %s\n",
                    error ? string_c_str(error) : "unknown sqlite error");
        }
    } else {
        fprintf(stderr, "failed to open datetime object store\n");
    }
    sqlite_close(db);
    string_free(name);
    return stored;
}

static string_t *read_standard_input(void)
{
    char buffer[4096];
    string_t *input = string_new();

    if (!input)
        return NULL;
    for (;;) {
        size_t got = fread(buffer, 1u, sizeof(buffer), stdin);

        if (got > 0u && string_append_chars(input, buffer, got) != 0) {
            string_free(input);
            return NULL;
        }
        if (got < sizeof(buffer)) {
            if (ferror(stdin)) {
                string_free(input);
                return NULL;
            }
            break;
        }
    }
    return input;
}

static bool parse_boolean_text(const char *text, bool *out)
{
    if (!text || !out)
        return false;
    if (strcmp(text, "1") == 0 || strcmp(text, "true") == 0 || strcmp(text, "yes") == 0) {
        *out = true;
        return true;
    }
    if (strcmp(text, "0") == 0 || strcmp(text, "false") == 0 || strcmp(text, "no") == 0) {
        *out = false;
        return true;
    }
    return false;
}

static void set_today(datetime_lab_options_t *options)
{
    time_t now;
    struct tm local_tm;

    time(&now);
    local_tm = *localtime(&now);
    options->date_year = (short)(local_tm.tm_year + 1900);
    options->date_month = (month_t)(local_tm.tm_mon + 1);
    options->date_day = (uint8_t)local_tm.tm_mday;
    options->julian_day_number = 0;
    options->use_julian_day_number = false;
}

static void init_defaults(datetime_lab_options_t *options)
{
    set_today(options);
    options->start_year = options->date_year;
    options->start_month = options->date_month;
    options->start_day = options->date_day;
    options->end_year = options->date_year;
    options->end_month = options->date_month;
    options->end_day = options->date_day;
    options->year = options->date_year;
    options->latitude = 51.5074;
    options->longitude = -0.1278;
    options->elevation_metres = 0.0;
    options->gmt_offset = DBL_MAX;
    options->jurisdiction[0] = '\0';
    options->cache_only = false;
    options->cache_put = false;
}

static bool apply_arg(datetime_lab_options_t *options, const char *arg)
{
    const char *equals = strchr(arg, '=');
    const char *value;
    size_t key_len;
    short y;
    month_t m;
    uint8_t d;
    double number;
    int integer;
    long long_integer;

    if (!equals)
        return false;

    key_len = (size_t)(equals - arg);
    value = equals + 1;

    if (key_equals(arg, key_len, "date")) {
        if (!parse_date_text(value, &y, &m, &d))
            return false;
        options->date_year = y;
        options->date_month = m;
        options->date_day = d;
        options->use_julian_day_number = false;
        return true;
    }
    if (key_equals(arg, key_len, "jdn") || key_equals(arg, key_len, "julian_day_number")) {
        datetime_t *probe;

        if (!parse_long_text(value, &long_integer))
            return false;
        probe = datetime_init_jdn(datetime_alloc(), long_integer);
        if (!probe)
            return false;
        if (datetime_year(probe) < 1 || datetime_year(probe) > 9999 ||
            !datetime_valid_ymd(datetime_year(probe), datetime_month(probe), datetime_day(probe))) {
            datetime_dealloc(probe);
            return false;
        }
        options->julian_day_number = long_integer;
        options->date_year = datetime_year(probe);
        options->date_month = datetime_month(probe);
        options->date_day = datetime_day(probe);
        options->use_julian_day_number = true;
        datetime_dealloc(probe);
        return true;
    }
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
    if (key_equals(arg, key_len, "year")) {
        if (!parse_int_text(value, &integer))
            return false;
        options->year = integer;
        return true;
    }
    if (key_equals(arg, key_len, "lat") || key_equals(arg, key_len, "latitude")) {
        if (!parse_double_text(value, &number) || number < -90.0 || number > 90.0)
            return false;
        options->latitude = number;
        return true;
    }
    if (key_equals(arg, key_len, "lon") || key_equals(arg, key_len, "longitude")) {
        if (!parse_double_text(value, &number) || number < -180.0 || number > 180.0)
            return false;
        options->longitude = number;
        return true;
    }
    if (key_equals(arg, key_len, "elevation") || key_equals(arg, key_len, "elevation_metres")) {
        if (!parse_double_text(value, &number) || !isfinite(number))
            return false;
        options->elevation_metres = number;
        return true;
    }
    if (key_equals(arg, key_len, "gmt_offset")) {
        if (*value == '\0') {
            options->gmt_offset = DBL_MAX;
            return true;
        }
        if (!parse_double_text(value, &number) || number < -14.0 || number > 14.0)
            return false;
        options->gmt_offset = number;
        return true;
    }
    if (key_equals(arg, key_len, "jurisdiction")) {
        size_t value_len = strlen(value);

        if (value_len == 0 || value_len >= sizeof(options->jurisdiction))
            return false;
        memcpy(options->jurisdiction, value, value_len + 1u);
        return true;
    }
    if (key_equals(arg, key_len, "cache_only")) {
        if (!parse_boolean_text(value, &options->cache_only))
            return false;
        return true;
    }
    if (key_equals(arg, key_len, "cache_put")) {
        if (!parse_boolean_text(value, &options->cache_put))
            return false;
        return true;
    }
    return false;
}

static void resolve_jurisdiction_gmt_offset(datetime_lab_options_t *options)
{
    jurisdiction_t *holiday = NULL;
    datetime_t *date = NULL;
    double offset_hours;

    if (!options || options->gmt_offset != DBL_MAX || options->jurisdiction[0] == '\0')
        return;

    holiday = jurisdict_open(options->jurisdiction);
    if (!holiday)
        goto done;

    date = options->use_julian_day_number
               ? datetime_init_jdn(datetime_alloc(), options->julian_day_number)
               : datetime_init_ymd(datetime_alloc(), options->date_year, options->date_month, options->date_day);
    if (!date)
        goto done;
    if (!jurisdict_default_gmt_offset(holiday, date, &offset_hours))
        goto done;

    options->gmt_offset = offset_hours;

done:
    datetime_dealloc(date);
    jurisdict_close(holiday);
}

static bool resolve_jurisdiction_dst_transitions(const datetime_lab_options_t *options, datetime_t **clocks_forward,
                                                 double *forward_from_offset, double *forward_to_offset,
                                                 datetime_t **clocks_back, double *back_from_offset,
                                                 double *back_to_offset)
{
    jurisdiction_t *holiday = NULL;
    bool ok = false;

    if (clocks_forward)
        *clocks_forward = NULL;
    if (clocks_back)
        *clocks_back = NULL;
    if (forward_from_offset)
        *forward_from_offset = DBL_MAX;
    if (forward_to_offset)
        *forward_to_offset = DBL_MAX;
    if (back_from_offset)
        *back_from_offset = DBL_MAX;
    if (back_to_offset)
        *back_to_offset = DBL_MAX;
    if (!options || options->jurisdiction[0] == '\0')
        return false;

    holiday = jurisdict_open(options->jurisdiction);
    if (!holiday)
        goto done;
    ok = jurisdict_dst_transition_details(holiday, options->date_year, clocks_forward, forward_from_offset,
                                          forward_to_offset, clocks_back, back_from_offset, back_to_offset);

done:
    jurisdict_close(holiday);
    return ok;
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

static char *format_datetime_minutes(const datetime_t *dttm)
{
    string_t *format = string_new_with("%yyyy-%mm-%dd @Hh:@mm");
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

static char *format_datetime_iso_minutes(const datetime_t *dttm)
{
    return datetime_format(dttm, "%yyyy-%mm-%dd @Hh:@mm");
}

static char *format_almanac_event_seconds(const almanac_event_time_t *event_time)
{
    datetime_t *local_time;
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    char *out;

    if (!event_time || !event_time->valid)
        return NULL;

    local_time = datetime_alloc();
    if (!local_time)
        return NULL;
    if (!almanac_event_time_datetime(event_time, local_time)) {
        datetime_dealloc(local_time);
        return NULL;
    }

    year = datetime_year(local_time);
    month = (int)datetime_month(local_time);
    day = datetime_day(local_time);
    hour = datetime_hour(local_time);
    minute = datetime_minute(local_time);
    second = (int)lround(datetime_second(local_time));
    datetime_dealloc(local_time);

    if (second >= 60) {
        second -= 60;
        minute += 1;
    }
    if (minute >= 60) {
        minute -= 60;
        hour += 1;
    }
    if (hour >= 24) {
        unsigned short days_this_month;

        hour -= 24;
        days_this_month = datetime_days_in_month((short)year, (month_t)month);
        day += 1;
        if (day > (int)days_this_month) {
            day = 1;
            month += 1;
            if (month > 12) {
                month = 1;
                year += 1;
            }
        }
    }

    out = malloc(64u);
    if (!out)
        return NULL;
    snprintf(out, 64u, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
    return out;
}

static void print_date_field(const char *name, const datetime_t *dttm)
{
    char *text = format_date(dttm);
    printf("%s %s\n", name, text ? text : "unavailable");
    free(text);
}

static void print_time_field(const char *name, const datetime_t *dttm)
{
    char *text = format_datetime_minutes(dttm);
    printf("%s %s\n", name, text ? text : "unavailable");
    free(text);
}

static void print_iso_time_field(const char *name, const datetime_t *dttm)
{
    char *text = format_datetime_iso_minutes(dttm);
    printf("%s %s\n", name, text ? text : "unavailable");
    free(text);
}

static char *describe_adjacent_sun_event(long jdn, double latitude, double longitude, double gmt_offset, bool sunrise,
                                         bool previous)
{
    datetime_sun_status_t status = DATETIME_SUN_UNAVAILABLE;
    datetime_t *dttm = previous ? (sunrise ? datetime_init_previous_sunrise_checked(datetime_alloc(), jdn, latitude,
                                                                                    longitude, gmt_offset, &status)
                                           : datetime_init_previous_sunset_checked(datetime_alloc(), jdn, latitude,
                                                                                   longitude, gmt_offset, &status))
                                : (sunrise ? datetime_init_next_sunrise_checked(datetime_alloc(), jdn, latitude,
                                                                                longitude, gmt_offset, &status)
                                           : datetime_init_next_sunset_checked(datetime_alloc(), jdn, latitude,
                                                                               longitude, gmt_offset, &status));
    char *text = NULL;

    if (!dttm || status != DATETIME_SUN_OK) {
        datetime_dealloc(dttm);
        return NULL;
    }

    text = format_datetime_minutes(dttm);
    datetime_dealloc(dttm);
    return text;
}

static void print_polar_sun_status(const char *name, long jdn, double latitude, double longitude, double gmt_offset,
                                   datetime_sun_status_t status)
{
    char *last_event_text = NULL;
    char *next_event_text = NULL;

    if (status == DATETIME_SUN_NEVER_SETS) {
        last_event_text = describe_adjacent_sun_event(jdn, latitude, longitude, gmt_offset, true, true);
        next_event_text = describe_adjacent_sun_event(jdn, latitude, longitude, gmt_offset, false, false);
        printf("%s unavailable\n", name);
        if (last_event_text || next_event_text) {
            printf("%s_status sun remains above the horizon; last sunrise %s; next sunset %s\n", name,
                   last_event_text ? last_event_text : "unavailable",
                   next_event_text ? next_event_text : "unavailable");
        } else {
            printf("%s_status sun never sets\n", name);
        }
    } else if (status == DATETIME_SUN_NEVER_RISES) {
        last_event_text = describe_adjacent_sun_event(jdn, latitude, longitude, gmt_offset, false, true);
        next_event_text = describe_adjacent_sun_event(jdn, latitude, longitude, gmt_offset, true, false);
        printf("%s unavailable\n", name);
        if (last_event_text || next_event_text) {
            printf("%s_status sun remains below the horizon; last sunset %s; next sunrise %s\n", name,
                   last_event_text ? last_event_text : "unavailable",
                   next_event_text ? next_event_text : "unavailable");
        } else {
            printf("%s_status sun never rises\n", name);
        }
    } else {
        printf("%s unavailable\n", name);
        printf("%s_status unavailable\n", name);
    }

    free(last_event_text);
    free(next_event_text);
}

static void print_owned_text_field(const char *name, string_t *text)
{
    printf("%s %s\n", name, text ? string_c_str(text) : "unavailable");
    string_free(text);
}

static int calendar_section_row_compare(const void *lhs, const void *rhs)
{
    const calendar_section_row_t *a = lhs;
    const calendar_section_row_t *b = rhs;
    int date_cmp = 0;

    if (a->sort_date && b->sort_date)
        date_cmp = datetime_compare(a->sort_date, b->sort_date);
    else if (a->sort_date)
        date_cmp = -1;
    else if (b->sort_date)
        date_cmp = 1;

    if (date_cmp != 0)
        return date_cmp;
    return (a->sequence > b->sequence) - (a->sequence < b->sequence);
}

static void print_optional_date_field(const char *name, const datetime_t *dttm);
static datetime_t *datetime_init_observance_start_local(datetime_t *dttm, almanac_t *almanac,
                                                        jurisdiction_t *jurisdiction, const datetime_t *observance,
                                                        double latitude, double longitude, double elevation_metres,
                                                        double fallback_gmt_offset);
static void print_optional_start_local_field(const char *name, const datetime_t *local_start);

static bool calendar_section_add_row(array_t *rows, const char *label, const char *value, const datetime_t *sort_date,
                                     int sequence)
{
    calendar_section_row_t row;

    if (!rows || !label || !value || !sort_date || !*value)
        return false;

    memset(&row, 0, sizeof(row));
    snprintf(row.label, sizeof(row.label), "%s", label);
    snprintf(row.value, sizeof(row.value), "%s", value);
    row.sort_date = sort_date;
    row.sequence = sequence;
    return array_add(rows, &row);
}

static bool calendar_section_add_date_row(array_t *rows, const char *label, const datetime_t *date, int sequence)
{
    char date_text[32];

    if (!date || datetime_jdn(date) == LONG_MAX)
        return false;
    snprintf(date_text, sizeof(date_text), "%04d-%02d-%02d", (int)datetime_year(date), (int)datetime_month(date),
             (int)datetime_day(date));
    return calendar_section_add_row(rows, label, date_text, date, sequence);
}

static bool calendar_section_add_text_row(array_t *rows, const char *label, const char *value,
                                          const datetime_t *sort_date, int sequence)
{
    if (!sort_date || datetime_jdn(sort_date) == LONG_MAX)
        return false;
    return calendar_section_add_row(rows, label, value, sort_date, sequence);
}

static void print_calendar_section_field(const char *name, const char *current_value, array_t *rows)
{
    if (!name || !current_value)
        return;

    printf("%s current date\t%s\n", name, current_value);
    if (!rows || array_size(rows) == 0u)
        return;

    array_sort(rows, calendar_section_row_compare);
    for (size_t i = 0; i < array_size(rows); i++) {
        const calendar_section_row_t *row = array_get(rows, i);
        if (!row)
            continue;
        printf("%s\t%s\n", row->label, row->value);
    }
}

static bool init_section_runtimes(calendar_section_runtime_t *states, const calendar_section_descriptor_t *sections,
                                  size_t section_count)
{
    if (!states || !sections)
        return false;

    memset(states, 0, sizeof(*states) * section_count);
    for (size_t i = 0; i < section_count; i++) {
        states[i].descriptor = &sections[i];
        states[i].event_count = sections[i].event_count;
        states[i].events = calloc(sections[i].event_count, sizeof(datetime_event_runtime_t));
        if (!states[i].events) {
            for (size_t j = 0; j < i; j++) {
                free(states[j].events);
                states[j].events = NULL;
                states[j].event_count = 0;
                states[j].descriptor = NULL;
            }
            return false;
        }
        for (size_t j = 0; j < sections[i].event_count; j++)
            states[i].events[j].descriptor = &sections[i].events[j];
    }
    return true;
}

static void destroy_section_runtimes(calendar_section_runtime_t *states, size_t section_count)
{
    if (!states)
        return;

    for (size_t i = 0; i < section_count; i++) {
        if (!states[i].events)
            continue;
        for (size_t j = 0; j < states[i].event_count; j++) {
            datetime_dealloc(states[i].events[j].value);
            datetime_dealloc(states[i].events[j].start_local);
        }
        free(states[i].events);
        states[i].events = NULL;
        states[i].event_count = 0;
        states[i].descriptor = NULL;
    }
}

static bool allocate_section_event_datetimes(calendar_section_runtime_t *states, size_t section_count)
{
    if (!states)
        return false;

    for (size_t i = 0; i < section_count; i++) {
        for (size_t j = 0; j < states[i].event_count; j++) {
            states[i].events[j].value = datetime_alloc();
            if (!states[i].events[j].value) {
                destroy_section_runtimes(states, section_count);
                return false;
            }
        }
    }
    return true;
}

static void init_section_event_datetimes(calendar_section_runtime_t *states, size_t section_count, int year)
{
    if (!states)
        return;

    for (size_t i = 0; i < section_count; i++) {
        for (size_t j = 0; j < states[i].event_count; j++) {
            datetime_t *storage = states[i].events[j].value;
            datetime_t *initialised = states[i].events[j].descriptor->init_fn(storage, year);

            states[i].events[j].value = initialised;
            if (!initialised)
                datetime_dealloc(storage);
        }
    }
}

static void init_section_event_start_datetimes(calendar_section_runtime_t *states, size_t section_count,
                                               almanac_t *almanac, jurisdiction_t *jurisdiction, double latitude,
                                               double longitude, double elevation_metres, double fallback_gmt_offset)
{
    if (!states)
        return;

    for (size_t i = 0; i < section_count; i++) {
        for (size_t j = 0; j < states[i].event_count; j++) {
            datetime_event_runtime_t *event = &states[i].events[j];

            if (!event->descriptor->start_gmt_field_name || !event->value)
                continue;
            event->start_local =
                datetime_init_observance_start_local(datetime_alloc(), almanac, jurisdiction, event->value, latitude,
                                                     longitude, elevation_metres, fallback_gmt_offset);
        }
    }
}

static void print_event_fields(const datetime_event_runtime_t *events, size_t count)
{
    if (!events)
        return;

    for (size_t i = 0; i < count; i++) {
        print_optional_date_field(events[i].descriptor->field_name, events[i].value);
        if (events[i].descriptor->start_gmt_field_name) {
            print_optional_start_local_field(events[i].descriptor->start_gmt_field_name, events[i].start_local);
        }
    }
}

static void append_sunset_start_section_row(array_t *rows, const char *label, const datetime_t *observance,
                                            const datetime_t *local_start, int sequence)
{
    char start_text[32];

    if (!rows || !label || !observance || !local_start)
        return;

    snprintf(start_text, sizeof(start_text), "%04d-%02d-%02d %02d:%02d", (int)datetime_year(local_start),
             (int)datetime_month(local_start), (int)datetime_day(local_start), (int)datetime_hour(local_start),
             (int)datetime_minute(local_start));
    calendar_section_add_text_row(rows, label, start_text, observance, sequence);
}

static void print_section_event_fields(const calendar_section_runtime_t *states, size_t section_count)
{
    if (!states)
        return;

    for (size_t i = 0; i < section_count; i++) {
        print_event_fields(states[i].events, states[i].event_count);
    }
}

static void populate_calendar_section_rows(const calendar_section_runtime_t *states, size_t section_count,
                                           array_t **section_rows)
{
    if (!states || !section_rows)
        return;

    for (size_t i = 0; i < section_count; i++) {
        for (size_t j = 0; j < states[i].event_count; j++) {
            const datetime_event_descriptor_t *event_descriptor = states[i].events[j].descriptor;
            datetime_t *event = states[i].events[j].value;

            calendar_section_add_date_row(section_rows[i], event_descriptor->section_label, event,
                                          event_descriptor->sequence);
            if (event_descriptor->start_gmt_section_label) {
                append_sunset_start_section_row(section_rows[i], event_descriptor->start_gmt_section_label, event,
                                                states[i].events[j].start_local, event_descriptor->start_gmt_sequence);
            }
        }
    }
}

static void print_optional_date_field(const char *name, const datetime_t *dttm)
{
    if (!dttm) {
        printf("%s unavailable\n", name);
        return;
    }
    print_date_field(name, dttm);
}

static bool resolve_local_offset_for_datetime(jurisdiction_t *jurisdiction, const datetime_t *dttm,
                                              double fallback_gmt_offset, double *offset_hours)
{
    if (!dttm || !offset_hours)
        return false;
    if (jurisdiction) {
        if (jurisdict_default_gmt_offset(jurisdiction, dttm, offset_hours))
            return true;
    }
    if (fallback_gmt_offset != DBL_MAX) {
        *offset_hours = fallback_gmt_offset;
        return true;
    }
    *offset_hours = datetime_tz_offset(dttm);
    return *offset_hours != DBL_MAX;
}

static bool datetime_add_offset_hours(datetime_t *dttm, double offset_hours)
{
    long total_minutes;

    if (!dttm || !isfinite(offset_hours))
        return false;
    total_minutes = lround(offset_hours * 60.0);
    return datetime_add_minutes(dttm, (int)total_minutes) != NULL;
}

static datetime_t *datetime_init_almanac_event_time(datetime_t *dttm, const almanac_event_time_t *event_time)
{
    if (!dttm || !event_time || !event_time->valid)
        return NULL;
    return almanac_event_time_datetime(event_time, dttm) ? dttm : NULL;
}

static almanac_observer_t datetime_lab_observer(double latitude, double longitude, double elevation_metres)
{
    almanac_observer_t observer;

    observer.latitude_degrees = latitude;
    observer.longitude_degrees = longitude;
    observer.elevation_metres = elevation_metres;
    return observer;
}

static almanac_observer_t datetime_lab_observer_from_options(const datetime_lab_options_t *options)
{
    if (!options)
        return datetime_lab_observer(0.0, 0.0, 0.0);
    return datetime_lab_observer(options->latitude, options->longitude, options->elevation_metres);
}

static datetime_t *datetime_init_observance_start_local(datetime_t *dttm, almanac_t *almanac,
                                                        jurisdiction_t *jurisdiction, const datetime_t *observance,
                                                        double latitude, double longitude, double elevation_metres,
                                                        double fallback_gmt_offset)
{
    datetime_t *start_utc = NULL;
    double offset_hours;

    if (!dttm || !observance)
        return NULL;

    if (almanac && jurisdiction) {
        datetime_t *previous_day = datetime_init_copy(datetime_alloc(), observance);
        almanac_observer_t observer = datetime_lab_observer(latitude, longitude, elevation_metres);
        almanac_sun_times_t sun_times;

        if (previous_day && datetime_add_days(previous_day, -1) &&
            almanac_sunrise_sunset(almanac, jurisdiction, previous_day, &observer, &sun_times) &&
            sun_times.set.status == ALMANAC_RISE_SET_OK &&
            datetime_init_almanac_event_time(dttm, &sun_times.set.time)) {
            datetime_dealloc(previous_day);
            return dttm;
        }
        datetime_dealloc(previous_day);
    }

    start_utc = datetime_init_sunset_observance_start(datetime_alloc(), observance, latitude, longitude, 0.0);
    if (!start_utc)
        return NULL;
    if (!resolve_local_offset_for_datetime(jurisdiction, start_utc, fallback_gmt_offset, &offset_hours)) {
        datetime_dealloc(start_utc);
        return NULL;
    }
    datetime_init_copy(dttm, start_utc);
    datetime_dealloc(start_utc);
    if (!datetime_add_offset_hours(dttm, offset_hours)) {
        datetime_dealloc(dttm);
        return NULL;
    }
    return dttm;
}

static void print_optional_start_local_field(const char *name, const datetime_t *local_start)
{
    if (!local_start) {
        printf("%s unavailable\n", name);
        return;
    }

    print_time_field(name, local_start);
}

static void print_sun_time_field(const char *name, long jdn, double latitude, double longitude, double gmt_offset,
                                 bool sunrise)
{
    datetime_sun_status_t status = DATETIME_SUN_UNAVAILABLE;
    datetime_t *dttm;

    dttm = sunrise ? datetime_init_sunrise_checked(datetime_alloc(), jdn, latitude, longitude, gmt_offset, &status)
                   : datetime_init_sunset_checked(datetime_alloc(), jdn, latitude, longitude, gmt_offset, &status);
    if (!dttm) {
        printf("%s unavailable\n", name);
        printf("%s_status unavailable\n", name);
        return;
    }

    if (status == DATETIME_SUN_NEVER_RISES) {
        print_polar_sun_status(name, jdn, latitude, longitude, gmt_offset, status);
        datetime_dealloc(dttm);
        return;
    }
    if (status == DATETIME_SUN_NEVER_SETS) {
        print_polar_sun_status(name, jdn, latitude, longitude, gmt_offset, status);
        datetime_dealloc(dttm);
        return;
    }
    if (status != DATETIME_SUN_OK) {
        printf("%s unavailable\n", name);
        printf("%s_status unavailable\n", name);
        datetime_dealloc(dttm);
        return;
    }

    print_time_field(name, dttm);
    printf("%s_status ok\n", name);
    datetime_dealloc(dttm);
}

static const char *rise_set_status_text(almanac_rise_set_status_t status)
{
    static const char *const text_by_status[ALMANAC_RISE_SET_UNAVAILABLE + 1] = {
        [ALMANAC_RISE_SET_OK] = "ok",
        [ALMANAC_RISE_SET_NOT_ON_DATE] = "not on selected date",
        [ALMANAC_RISE_SET_NEVER_RISES] = "never rises",
        [ALMANAC_RISE_SET_NEVER_SETS] = "never sets",
        [ALMANAC_RISE_SET_UNAVAILABLE] = "unavailable"};

    if (status < ALMANAC_RISE_SET_OK || status > ALMANAC_RISE_SET_UNAVAILABLE)
        return "unavailable";
    return text_by_status[status];
}

static void print_rise_set_event_field(const char *name, const almanac_rise_set_event_t *event)
{
    char *text;

    if (!event || event->status != ALMANAC_RISE_SET_OK) {
        printf("%s unavailable\n", name);
        printf("%s_status %s\n", name, event ? rise_set_status_text(event->status) : "unavailable");
        return;
    }

    text = format_almanac_event_seconds(&event->time);
    printf("%s %s\n", name, text ? text : "unavailable");
    printf("%s_status %s\n", name, text ? "ok" : "unavailable");
    free(text);
}

static void print_unavailable_actual_sun_times(void)
{
    printf("actual_sunrise unavailable\n");
    printf("actual_sunrise_status unavailable\n");
    printf("actual_sunset unavailable\n");
    printf("actual_sunset_status unavailable\n");
}

static void print_unavailable_moon_times(void)
{
    printf("moonrise unavailable\n");
    printf("moonrise_status unavailable\n");
    printf("moonset unavailable\n");
    printf("moonset_status unavailable\n");
}

static bool print_actual_sun_times(const datetime_lab_options_t *options, almanac_t *almanac,
                                   jurisdiction_t *jurisdiction, const datetime_t *date)
{
    almanac_sun_times_t sun_times;
    almanac_observer_t observer;

    if (!options || !almanac || !jurisdiction || !date || options->jurisdiction[0] == '\0') {
        return false;
    }

    observer = datetime_lab_observer_from_options(options);
    if (!almanac_sunrise_sunset(almanac, jurisdiction, date, &observer, &sun_times)) {
        return false;
    }

    print_rise_set_event_field("sunrise", &sun_times.rise);
    print_rise_set_event_field("actual_sunrise", &sun_times.rise);
    print_rise_set_event_field("sunset", &sun_times.set);
    print_rise_set_event_field("actual_sunset", &sun_times.set);

    return true;
}

static bool print_moon_times(const datetime_lab_options_t *options, almanac_t *almanac, jurisdiction_t *jurisdiction,
                             const datetime_t *date)
{
    almanac_moon_times_t moon_times;
    almanac_observer_t observer;

    if (!options || !almanac || !jurisdiction || !date || options->jurisdiction[0] == '\0') {
        return false;
    }

    observer = datetime_lab_observer_from_options(options);
    if (!almanac_moonrise_moonset(almanac, jurisdiction, date, &observer, &moon_times)) {
        return false;
    }

    print_rise_set_event_field("moonrise", &moon_times.rise);
    print_rise_set_event_field("moonset", &moon_times.set);

    return true;
}

int main(int argc, char **argv)
{
    datetime_lab_options_t options;
    string_t *cached_output = NULL;
    datetime_t *date;
    datetime_t *start;
    datetime_t *end;
    calendar_section_runtime_t section_states[ARRAY_LEN(calendar_sections)];
    jurisdiction_t *observance_jurisdiction = NULL;
    almanac_t *observance_almanac = NULL;
    datetime_t *dst_forward = NULL;
    datetime_t *dst_back = NULL;
    double dst_forward_from_offset = DBL_MAX;
    double dst_forward_to_offset = DBL_MAX;
    double dst_back_from_offset = DBL_MAX;
    double dst_back_to_offset = DBL_MAX;
    datetime_span_t span;
    double days_between;
    double solar_declination;
    double solar_max_altitude;
    double solar_inclination;
    double timing_last;
    long jdn;

    init_defaults(&options);
    timing_last = monotonic_seconds();

    for (int i = 1; i < argc; i++) {
        if (!apply_arg(&options, argv[i])) {
            fprintf(stderr, "Bad datetime argument: %s\n", argv[i]);
            return 2;
        }
    }

    resolve_jurisdiction_gmt_offset(&options);

    if (options.cache_put) {
        string_t *input = read_standard_input();

        if (!input) {
            fprintf(stderr, "failed to read datetime cache input\n");
            return 1;
        }
        if (!store_cached_output(&options, input)) {
            fprintf(stderr, "failed to store datetime cache input\n");
            string_free(input);
            return 1;
        }
        string_free(input);
        return 0;
    }

    cached_output = load_cached_output(&options);
    if (cached_output) {
        fputs(string_c_str(cached_output), stdout);
        string_free(cached_output);
        return 0;
    }
    if (options.cache_only)
        return 3;

    (void)resolve_jurisdiction_dst_transitions(&options, &dst_forward, &dst_forward_from_offset, &dst_forward_to_offset,
                                               &dst_back, &dst_back_from_offset, &dst_back_to_offset);
    trace_timing("dst transitions", &timing_last);

    date = datetime_alloc();
    start = datetime_alloc();
    end = datetime_alloc();
    if (!init_section_runtimes(section_states, calendar_sections, ARRAY_LEN(calendar_sections))) {
        fprintf(stderr, "datetime section setup failed\n");
        datetime_dealloc(date);
        datetime_dealloc(start);
        datetime_dealloc(end);
        return 1;
    }
    {
        const size_t section_count = ARRAY_LEN(calendar_sections);

        if (!date || !start || !end || !allocate_section_event_datetimes(section_states, section_count)) {
            fprintf(stderr, "datetime allocation failed\n");
            datetime_dealloc(date);
            datetime_dealloc(start);
            datetime_dealloc(end);
            destroy_section_runtimes(section_states, section_count);
            return 1;
        }

        datetime_init_ymdt(date, options.date_year, options.date_month, options.date_day, 12, 0, 0.0);
        datetime_init_ymd(start, options.start_year, options.start_month, options.start_day);
        datetime_init_ymd(end, options.end_year, options.end_month, options.end_day);
        if (options.jurisdiction[0] != '\0') {
            observance_jurisdiction = jurisdict_open(options.jurisdiction);
            observance_almanac = almanac_open();
        }
        init_section_event_datetimes(section_states, section_count, options.year);
        init_section_event_start_datetimes(section_states, section_count, observance_almanac, observance_jurisdiction,
                                           options.latitude, options.longitude, options.elevation_metres,
                                           options.gmt_offset);
        if (!section_states[CALENDAR_SECTION_CHRISTIAN].events[0].value ||
            !section_states[CALENDAR_SECTION_CHRISTIAN].events[1].value) {
            fprintf(stderr, "datetime calculation failed\n");
            datetime_dealloc(date);
            datetime_dealloc(start);
            datetime_dealloc(end);
            destroy_section_runtimes(section_states, section_count);
            almanac_close(observance_almanac);
            jurisdict_close(observance_jurisdiction);
            datetime_dealloc(dst_forward);
            datetime_dealloc(dst_back);
            return 1;
        }

        jdn = datetime_jdn(date);
        days_between = datetime_duration(end, start, &span);
        solar_declination = datetime_solar_declination(date);
        solar_max_altitude = datetime_solar_max_altitude(date, options.latitude);
        solar_inclination = datetime_solar_inclination(date, options.latitude);

        print_date_field("date", date);
        printf("weekday %s\n", datetime_weekday_name(datetime_weekday(date)));
        printf("julian_day_number %ld\n", jdn);
        print_owned_text_field("christian_calendar_date", datetime_christian_calendar_date_text(date));
        print_owned_text_field("chinese_calendar_date", datetime_chinese_calendar_date_text(date));
        print_owned_text_field("hindu_calendar_date", datetime_hindu_calendar_date_text(date));
        print_owned_text_field("buddhist_calendar_date", datetime_buddhist_calendar_date_text(date));
        print_owned_text_field("muslim_calendar_date", datetime_muslim_calendar_date_text(date));
        print_owned_text_field("jewish_calendar_date", datetime_jewish_calendar_date_text(date));
        print_owned_text_field("cherokee_calendar_date", datetime_cherokee_calendar_date_text(date));
        print_owned_text_field("mayan_calendar_date", datetime_mayan_calendar_date_text(date));
        print_owned_text_field("aztec_calendar_date", datetime_aztec_calendar_date_text(date));
        print_owned_text_field("ethiopian_calendar_date", datetime_ethiopian_calendar_date_text(date));
        printf("moon_phase %s\n", datetime_moon_phase_name(datetime_moon_phase(date)));
        printf("solar_declination %.10g\n", solar_declination);
        printf("solar_max_altitude %.10g\n", solar_max_altitude);
        printf("solar_inclination %.10g\n", solar_inclination);
        printf("latitude %.10g\n", options.latitude);
        printf("longitude %.10g\n", options.longitude);
        printf("elevation_metres %.10g\n", options.elevation_metres);
        if (options.gmt_offset == DBL_MAX)
            printf("gmt_offset local\n");
        else
            printf("gmt_offset %.10g\n", options.gmt_offset);

        if (!print_actual_sun_times(&options, observance_almanac, observance_jurisdiction, date)) {
            print_sun_time_field("sunrise", jdn, options.latitude, options.longitude, options.gmt_offset, true);
            print_sun_time_field("sunset", jdn, options.latitude, options.longitude, options.gmt_offset, false);
            print_unavailable_actual_sun_times();
        }
        if (!print_moon_times(&options, observance_almanac, observance_jurisdiction, date))
            print_unavailable_moon_times();
        trace_timing("sun and moon times", &timing_last);
        if (dst_forward || dst_back) {
            print_iso_time_field("dst_forward", dst_forward);
            print_iso_time_field("dst_back", dst_back);
            if (dst_forward_from_offset != DBL_MAX)
                printf("dst_forward_from_offset %.10g\n", dst_forward_from_offset);
            if (dst_forward_to_offset != DBL_MAX)
                printf("dst_forward_to_offset %.10g\n", dst_forward_to_offset);
            if (dst_back_from_offset != DBL_MAX)
                printf("dst_back_from_offset %.10g\n", dst_back_from_offset);
            if (dst_back_to_offset != DBL_MAX)
                printf("dst_back_to_offset %.10g\n", dst_back_to_offset);
            printf("dst_status ok\n");
        } else {
            printf("dst_forward unavailable\n");
            printf("dst_back unavailable\n");
            printf("dst_status none\n");
        }

        print_date_field("start", start);
        print_date_field("end", end);
        printf("days_between %.17g\n", days_between);
        printf("days_between_abs %.17g\n", fabs(days_between));
        printf("duration_years %u\n", span.years);
        printf("duration_months %u\n", span.months);
        printf("duration_days %u\n", span.days);

        print_section_event_fields(section_states, section_count);
        trace_timing("event fields", &timing_last);

        {
            string_t *current_texts[CALENDAR_SECTION_COUNT] = {0};
            array_t *section_rows[CALENDAR_SECTION_COUNT] = {0};

            for (size_t i = 0; i < CALENDAR_SECTION_COUNT; i++) {
                current_texts[i] = calendar_sections[i].current_text_fn(date);
                section_rows[i] = array_create(sizeof(calendar_section_row_t), NULL, NULL);
            }

            populate_calendar_section_rows(section_states, section_count, section_rows);
            trace_timing("calendar rows", &timing_last);

            for (size_t i = 0; i < section_count; i++) {
                print_calendar_section_field(calendar_sections[i].field_name,
                                             current_texts[i] ? string_c_str(current_texts[i]) : "", section_rows[i]);
                string_free(current_texts[i]);
                array_destroy(section_rows[i]);
            }
        }

        datetime_dealloc(date);
        datetime_dealloc(start);
        datetime_dealloc(end);
        destroy_section_runtimes(section_states, section_count);
        almanac_close(observance_almanac);
        jurisdict_close(observance_jurisdiction);
        datetime_dealloc(dst_forward);
        datetime_dealloc(dst_back);
        return 0;
    }
    return 0;
}
