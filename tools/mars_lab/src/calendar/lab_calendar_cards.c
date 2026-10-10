/**
 * @file lab_calendar_cards.c
 * @brief Browser section and DateTime card presentation for native MARS Lab.
 *
 * Converts native calendar fields and already ordered observance sections into
 * the label/value structures consumed by the existing browser. Plain text cards
 * are produced from the same sections, preserving native mathematical values.
 */
#include <math.h>
#include <string.h>
#include <time.h>

#include "datetime.h"
#include "lab_calendar_internal.h"

/* Keep named-town offsets and the location route's JSON contract in one adapter. */
json_t *lab_cal_location_fields(const json_t *fields, const json_t *options)
{
    json_t *response = json_new_object();
    lab_cal_take(response, "ok", json_new_bool(true));
    lab_cal_set(response, "jurisdiction", lab_cal_text(options, "jurisdiction"));
    lab_cal_set(response, "latitude", lab_cal_text(fields, "jurisdiction_latitude"));
    lab_cal_set(response, "longitude", lab_cal_text(fields, "jurisdiction_longitude"));
    lab_cal_set(response, "gmt_offset", lab_cal_text(fields, "jurisdiction_gmt_offset"));
    string_t *zone = lab_cal_town_timezone(options);
    string_t *date = string_new_with(lab_cal_text(options, "date"));
    double offset;
    if (lab_cal_timezone_noon(zone, date, &offset)) {
        string_t *text = string_sprintf("%.10g", offset);
        lab_cal_set(response, "gmt_offset", string_c_str(text));
        string_free(text);
    }
    string_free(zone);
    string_free(date);
    lab_cal_set(response, "source", "holiday_lab");
    return response;
}

/* Append a labelled value to a browser card. */
void lab_cal_row(json_t *rows, const char *label, const char *value)
{
    json_t *row = json_new_object();
    lab_cal_set(row, "label", label);
    lab_cal_set(row, "value", value);
    lab_cal_append(rows, row);
}

/* Copy section contents without retaining caller-owned arrays. */
void lab_cal_section(json_t *sections, const char *title, bool open, const json_t *rows)
{
    json_t *section = json_new_object();
    lab_cal_set(section, "title", title);
    lab_cal_take(section, "open", json_new_bool(open));
    lab_cal_take(section, "rows", json_clone(rows));
    string_t *html = lab_cal_section_markup(title, open, rows);
    if (html) lab_cal_set(section, "html", string_c_str(html));
    string_free(html);
    lab_cal_append(sections, section);
}

/* Keep text companions and structured cards in agreement. */
void lab_cal_sections(json_t *response, const char *name, json_t *sections)
{
    string_t *text = string_new();
    for (size_t i = 0; i < json_array_size(sections); ++i) {
        const json_t *section = json_array_get(sections, i);
        const json_t *rows = lab_cal_get(section, "rows");
        for (size_t j = 0; j < json_array_size(rows); ++j) {
            const json_t *row = json_array_get(rows, j);
            if (string_byte_length(text))
                string_append_char(text, '\n');
            string_append_format(text, "%s: %s", lab_cal_text(row, "label"), lab_cal_text(row, "value"));
        }
    }
    lab_cal_set(response, name, string_c_str(text));
    string_t *key = string_sprintf("%s_sections", name);
    lab_cal_take(response, string_c_str(key), sections);
    string_free(key);
    string_free(text);
}

static void lab_cal_field_row(json_t *rows, const json_t *fields, const char *label, const char *key,
                              const char *suffix)
{
    string_t *value = string_sprintf("%s%s", lab_cal_text(fields, key), suffix ? suffix : "");
    lab_cal_row(rows, label, string_c_str(value));
    string_free(value);
}

static void lab_cal_rise_row(json_t *rows, const json_t *fields, const char *label, const char *key)
{
    const char *value = lab_cal_text(fields, key);
    string_t *status = string_sprintf("%s_status", key);
    if (!*value || !strcmp(value, "unavailable"))
        value = lab_cal_text(fields, string_c_str(status));
    lab_cal_row(rows, label, *value ? value : "unavailable");
    string_free(status);
}

static string_t *lab_cal_offset_text(const char *raw)
{
    double value;
    if (!lab_cal_number(raw, &value))
        return string_new_with(raw);
    int minutes = (int)lround(fabs(value) * 60);
    string_t *text = string_sprintf("GMT%c%d", value < 0 ? '-' : '+', minutes / 60);
    if (minutes % 60)
        string_append_format(text, ":%02d", minutes % 60);
    return text;
}

static void lab_cal_transition_row(json_t *rows, const json_t *fields, const char *label, const char *key)
{
    const char *raw = lab_cal_text(fields, key);
    if (!*raw || !strcmp(raw, "unavailable"))
        return;
    string_t *source = string_new_with(raw);
    size_t count = 0;
    string_t **parts = lab_cal_split(source, " ", &count);
    datetime_t *date = count == 2 ? datetime_from_string(string_c_str(parts[0])) : NULL;
    double hours = 0, minutes = 0;
    if (count == 2) {
        size_t clock_count = 0;
        string_t **clock = lab_cal_split(parts[1], ":", &clock_count);
        if (clock_count != 2 || !lab_cal_number(string_c_str(clock[0]), &hours) ||
            !lab_cal_number(string_c_str(clock[1]), &minutes)) {
            datetime_dealloc(date);
            date = NULL;
        }
        string_split_free(clock, clock_count);
    }
    string_t *text = string_new_with(raw);
    if (date) {
        time_t stamp = (time_t)llround((datetime_jd(date) - 2440587.5) * 86400 + hours * 3600 + minutes * 60);
        struct tm moment;
        if (gmtime_r(&stamp, &moment)) {
            static const char *weekdays[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                             "Thursday", "Friday", "Saturday"};
            static const char *months[] = {"January", "February", "March",     "April",   "May",      "June",
                                           "July",    "August",   "September", "October", "November", "December"};
            int day = moment.tm_mday;
            const char *suffix = day >= 11 && day <= 13 ? "th"
                                 : day % 10 == 1        ? "st"
                                 : day % 10 == 2        ? "nd"
                                 : day % 10 == 3        ? "rd"
                                                        : "th";
            string_free(text);
            text = string_sprintf("%s %d%s %s %d at %02d:%02d", weekdays[moment.tm_wday], day, suffix,
                                  months[moment.tm_mon], moment.tm_year + 1900, moment.tm_hour, moment.tm_min);
        }
    }
    datetime_dealloc(date);
    string_split_free(parts, count);
    string_free(source);
    string_t *from_key = string_sprintf("%s_from_offset", key), *to_key = string_sprintf("%s_to_offset", key);
    const char *from = lab_cal_text(fields, string_c_str(from_key)), *to = lab_cal_text(fields, string_c_str(to_key));
    if (*from && *to) {
        string_t *a = lab_cal_offset_text(from), *b = lab_cal_offset_text(to);
        string_append_format(text, ", from %s to %s", string_c_str(a), string_c_str(b));
        string_free(a);
        string_free(b);
    }
    lab_cal_row(rows, label, string_c_str(text));
    string_free(from_key);
    string_free(to_key);
    string_free(text);
}

static void lab_cal_date_sections(json_t *response, const json_t *fields)
{
    static const char *names[] = {"christian", "chinese",  "hindu", "buddhist", "muslim",
                                  "jewish",    "cherokee", "mayan", "aztec",    "ethiopian"};
    static const char *titles[] = {"Christian", "Chinese",  "Hindu", "Buddhist", "Muslim",
                                   "Jewish",    "Cherokee", "Mayan", "Aztec",    "Ethiopian"};
    json_t *sections = json_new_array();
    /* Ten fixed output sections are enumerated, never searched by key. */
    for (size_t i = 0; i < sizeof(names) / sizeof(*names); ++i) {
        string_t *key = string_sprintf("calendar_section_%s", names[i]);
        string_t *text = string_new_with(lab_cal_text(fields, string_c_str(key)));
        json_t *rows = json_new_array();
        size_t count = 0;
        string_t **lines = lab_cal_split(text, "\n", &count);
        for (size_t j = 0; j < count; ++j) {
            size_t columns = 0;
            string_t **parts = lab_cal_split(lines[j], "\t", &columns);
            if (columns == 2)
                lab_cal_row(rows, j ? string_c_str(parts[0]) : "Current date", string_c_str(parts[1]));
            string_split_free(parts, columns);
        }
        if (!json_array_size(rows)) {
            string_t *current_key = string_sprintf("%s_calendar_date", names[i]);
            lab_cal_row(rows, "Current date", lab_cal_text(fields, string_c_str(current_key)));
            string_free(current_key);
        }
        lab_cal_section(sections, titles[i], false, rows);
        json_free(rows);
        string_split_free(lines, count);
        string_free(text);
        string_free(key);
    }
    lab_cal_sections(response, "calendar", sections);
}

/* Prepare all five DateTime cards and retain the raw native fields. */
json_t *lab_cal_datetime(json_t *fields)
{
    json_t *response = json_new_object();
    lab_cal_take(response, "ok", json_new_bool(true));
    lab_cal_set(response, "mode", "datetime");
    lab_cal_take(response, "fields", json_clone(fields));
    json_t *sections = json_new_array();
    json_t *rows = json_new_array();
    lab_cal_field_row(rows, fields, "Date", "date", NULL);
    lab_cal_field_row(rows, fields, "Weekday", "weekday", NULL);
    const char *offset = lab_cal_text(fields, "gmt_offset");
    string_t *basis = !strcmp(offset, "local") ? string_new_with("local machine GMT offset")
                                               : string_sprintf("GMT offset %s", offset);
    lab_cal_row(rows, "Time basis", string_c_str(basis));
    string_free(basis);
    lab_cal_section(sections, "Date", true, rows);
    json_free(rows);
    rows = json_new_array();
    lab_cal_rise_row(rows, fields, "Sunrise", "sunrise");
    lab_cal_rise_row(rows, fields, "Sunset", "sunset");
    lab_cal_rise_row(rows, fields, "Moonrise", "moonrise");
    lab_cal_rise_row(rows, fields, "Moonset", "moonset");
    lab_cal_field_row(rows, fields, "Moon phase", "moon_phase", NULL);
    if (!strcmp(lab_cal_text(fields, "dst_status"), "none"))
        lab_cal_row(rows, "Daylight saving", "No daylight saving changes this year");
    else {
        lab_cal_transition_row(rows, fields, "Clocks forward", "dst_forward");
        lab_cal_transition_row(rows, fields, "Clocks back", "dst_back");
    }
    lab_cal_section(sections, "Sun and Moon", true, rows);
    json_free(rows);
    lab_cal_sections(response, "overview", sections);

    sections = json_new_array();
    rows = json_new_array();
    lab_cal_field_row(rows, fields, "Start date", "start", NULL);
    lab_cal_field_row(rows, fields, "End date", "end", NULL);
    lab_cal_field_row(rows, fields, "Days between", "days_between", NULL);
    lab_cal_field_row(rows, fields, "Absolute days", "days_between_abs", NULL);
    string_t *span = string_sprintf("%s years, %s months, %s days", lab_cal_text(fields, "duration_years"),
                                    lab_cal_text(fields, "duration_months"), lab_cal_text(fields, "duration_days"));
    lab_cal_row(rows, "Calendar span", string_c_str(span));
    string_free(span);
    lab_cal_section(sections, "Range", true, rows);
    json_free(rows);
    lab_cal_sections(response, "range", sections);
    lab_cal_date_sections(response, fields);

    sections = json_new_array();
    rows = json_new_array();
    string_t *holiday = string_new_with(lab_cal_text(fields, "bank_holiday"));
    size_t count = 0;
    string_t **lines = lab_cal_split(holiday, "\n", &count);
    for (size_t i = 0; i < count; ++i) {
        if (!string_length(lines[i]))
            continue;
        string_cursor_t *cursor = string_cursor_new(lines[i]);
        string_pos_t start = string_cursor_position(cursor);
        while (!string_cursor_done(cursor) && !string_cursor_match(cursor, ":"))
            string_cursor_next(cursor);
        string_t *label = string_cursor_extract(start, cursor);
        string_cursor_consume(cursor, ":");
        string_cursor_skip_spaces(cursor);
        string_t *value =
            string_cursor_slice_between(string_cursor_position(cursor), string_cursor_end_position(cursor), cursor);
        lab_cal_row(rows, string_c_str(label), string_c_str(value));
        string_free(label);
        string_free(value);
        string_cursor_free(cursor);
    }
    string_split_free(lines, count);
    string_free(holiday);
    if (!json_array_size(rows) && *lab_cal_text(fields, "holiday_notice"))
        lab_cal_field_row(rows, fields, "Holiday database", "holiday_notice", NULL);
    if (json_array_size(rows)) {
        string_t *title = *lab_cal_text(fields, "bank_holiday")
                              ? string_sprintf("Local holidays (%s to %s)", lab_cal_text(fields, "start"),
                                               lab_cal_text(fields, "end"))
                              : string_new_with("Local holidays");
        lab_cal_section(sections, string_c_str(title), true, rows);
        string_free(title);
    }
    json_free(rows);
    lab_cal_sections(response, "local", sections);

    sections = json_new_array();
    rows = json_new_array();
    lab_cal_field_row(rows, fields, "Latitude", "latitude", NULL);
    lab_cal_field_row(rows, fields, "Longitude", "longitude", NULL);
    lab_cal_field_row(rows, fields, "Elevation", "elevation_metres", " m");
    lab_cal_section(sections, "Location", true, rows);
    json_free(rows);
    rows = json_new_array();
    lab_cal_field_row(rows, fields, "Solar declination", "solar_declination", "°");
    lab_cal_field_row(rows, fields, "Sun's noon inclination", "solar_inclination", "°");
    lab_cal_field_row(rows, fields, "Sun's maximum altitude", "solar_max_altitude", "°");
    lab_cal_section(sections, "Solar", true, rows);
    json_free(rows);
    lab_cal_sections(response, "solar", sections);
    return response;
}
