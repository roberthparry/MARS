/**
 * @file lab_calendar_almanac.c
 * @brief Native navigation worksheet and named land-totality JSON adapters.
 *
 * Uses the existing almanac and event workers, retaining their native
 * body formatting. Event times prefer each named town's validated IANA zone;
 * unavailable zones fall back to jurisdiction rules, then the supplied zone.
 * All child failures are returned as errors, never empty successful tables.
 */
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "datetime.h"
#include "jurisdiction.h"
#include "lab_calendar_internal.h"

static const char *lab_cal_part(string_t **parts, size_t count, size_t index)
{
    return index < count ? string_c_str(parts[index]) : "";
}

static string_t *lab_cal_jd_text(double jd, double zone, bool date_only)
{
    if (!isfinite(jd) || jd < 1721425.5 || jd > 5373484.5)
        return string_new();
    time_t stamp = (time_t)llround((jd - 2440587.5) * 86400 + zone * 3600);
    struct tm moment;
    if (!gmtime_r(&stamp, &moment))
        return string_new();
    char buffer[40];
    strftime(buffer, sizeof(buffer), date_only ? "%Y-%m-%d" : "%Y-%m-%d %H:%M:%S", &moment);
    return string_new_with(buffer);
}

static double lab_cal_event_zone(double jd, const char *jurisdiction, double fallback, const char *timezone)
{
    string_t *name = string_new_with(timezone);
    struct tm local;
    double named_offset;
    bool named = lab_cal_timezone_at(name, jd, &local, &named_offset);
    string_free(name);
    if (named)
        return named_offset;
    datetime_t *moment = datetime_alloc();
    datetime_init_jd(moment, jd);
    jurisdiction_t *engine = jurisdict_open(jurisdiction);
    double result = fallback;
    if (engine && !jurisdict_default_gmt_offset(engine, moment, &result))
        result = fallback;
    jurisdict_close(engine);
    datetime_dealloc(moment);
    return isfinite(result) && result >= -14 && result <= 14 ? result : fallback;
}

static string_t *lab_cal_civil_time(double jd, const char *jurisdiction, double fallback, const char *timezone)
{
    double zone = lab_cal_event_zone(jd, jurisdiction, fallback, timezone);
    string_t *text = lab_cal_jd_text(jd, zone, false);
    int minutes = (int)lround(fabs(zone) * 60);
    string_append_format(text, " GMT%c%d", zone < 0 ? '-' : '+', minutes / 60);
    if (minutes % 60)
        string_append_format(text, ":%02d", minutes % 60);
    return text;
}

static json_t *lab_cal_event_rows(const json_t *fields)
{
    string_t *text = string_new_with(lab_cal_text(fields, "event"));
    size_t count = 0;
    string_t **lines = lab_cal_split(text, "\n", &count);
    json_t *rows = json_new_array();
    static const char *keys[] = {"category",        "name", "kind", "jd", "first_jd", "last_jd", "magnitude", "percent",
                                 "nearest_totality"};
    for (size_t i = 0; i < count; ++i) {
        if (!string_length(lines[i]))
            continue;
        size_t columns = 0;
        string_t **parts = lab_cal_split(lines[i], "|", &columns);
        if (columns >= 8) {
            json_t *row = json_new_object();
            for (size_t j = 0; j < sizeof(keys) / sizeof(*keys); ++j) {
                if (j < columns)
                    string_trim(parts[j]);
                lab_cal_set(row, keys[j], lab_cal_part(parts, columns, j));
            }
            lab_cal_append(rows, row);
        } else {
            string_split_free(parts, columns);
            string_split_free(lines, count);
            string_free(text);
            json_free(rows);
            return NULL;
        }
        string_split_free(parts, columns);
    }
    string_split_free(lines, count);
    string_free(text);
    return rows;
}

static json_t *lab_cal_run_events(const json_t *options, const char *start, const char *end, bool land,
                                  string_t **error)
{
    json_t *args = json_new_object();
    lab_cal_set(args, "start", start);
    lab_cal_set(args, "end", end);
    lab_cal_set(args, "lat", lab_cal_text(options, "lat"));
    lab_cal_set(args, "lon", lab_cal_text(options, "lon"));
    lab_cal_set(args, "kind", land ? "solar" : "all");
    if (land)
        lab_cal_set(args, "totality", "land");
    json_t *fields = lab_cal_run("almanac_event_lab", args, 20000, error);
    int saved_error = errno;
    json_free(args);
    json_t *rows = fields ? lab_cal_event_rows(fields) : NULL;
    if (fields && !rows) {
        string_free(*error);
        *error = string_new_with("Malformed native almanac event output");
        saved_error = EPROTO;
    }
    json_free(fields);
    errno = saved_error;
    return rows;
}

static json_t *lab_cal_body_rows(const json_t *fields, string_t **error)
{
    json_t *rows = json_new_array();
    string_t *text = string_new_with(lab_cal_text(fields, "snapshot"));
    size_t count = 0;
    string_t **lines = lab_cal_split(text, "\n", &count);
    static const char *keys[] = {"code",      "name",     "kind",    "declination",   "right_ascension",
                                 "gha",       "sha",      "lha",     "distance_au",   "phase",
                                 "magnitude", "altitude", "azimuth", "semi_diameter", "visible"};
    static const size_t columns_for_key[] = {0, 1, 2, 15, 16, 17, 6, 7, 8, 9, 10, 18, 19, 20, 14};
    bool valid = true;
    for (size_t i = 0; i < count; ++i) {
        if (!string_length(lines[i]))
            continue;
        size_t columns = 0;
        string_t **parts = lab_cal_split(lines[i], "|", &columns);
        if (columns != 21) {
            valid = false;
            string_split_free(parts, columns);
            break;
        }
        for (size_t j = 0; j < columns; ++j)
            string_trim(parts[j]);
        if (strcmp(lab_cal_part(parts, columns, 2), "reference")) {
            json_t *row = json_new_object();
            for (size_t j = 0; j < sizeof(keys) / sizeof(*keys); ++j)
                lab_cal_set(row, keys[j], lab_cal_part(parts, columns, columns_for_key[j]));
            double magnitude;
            if (lab_cal_number(lab_cal_part(parts, columns, 10), &magnitude)) {
                double rounded = round(magnitude * 10) / 10;
                string_t *formatted = string_sprintf(rounded == trunc(rounded) ? "%.0f" : "%.1f", rounded);
                lab_cal_set(row, "magnitude", string_c_str(formatted));
                string_free(formatted);
            } else
                lab_cal_set(row, "magnitude", "");
            lab_cal_append(rows, row);
        }
        string_split_free(parts, columns);
    }
    string_split_free(lines, count);
    string_free(text);
    if (!valid || !json_array_size(rows)) {
        *error = string_new_with(
            "Almanac backend returned no valid body snapshot; run make tools/mars_lab/workers/almanac_lab.");
        json_free(rows);
        return NULL;
    }
    return rows;
}

static json_t *lab_cal_present_event(const json_t *raw, const json_t *options)
{
    double jd, first, last, magnitude, percent, zone;
    if (!lab_cal_number(lab_cal_text(raw, "jd"), &jd) || !lab_cal_number(lab_cal_text(raw, "first_jd"), &first) ||
        !lab_cal_number(lab_cal_text(raw, "last_jd"), &last) ||
        !lab_cal_number(lab_cal_text(raw, "magnitude"), &magnitude) ||
        !lab_cal_number(lab_cal_text(raw, "percent"), &percent) ||
        !lab_cal_number(lab_cal_text(options, "zone"), &zone))
        return NULL;
    json_t *event = json_clone(raw);
    const char *jurisdiction = lab_cal_text(options, "jurisdiction");
    const char *timezone = lab_cal_text(options, "timezone");
    string_t *greatest = lab_cal_civil_time(jd, jurisdiction, zone, timezone);
    string_t *begin = lab_cal_civil_time(first, jurisdiction, zone, timezone);
    string_t *end = lab_cal_civil_time(last, jurisdiction, zone, timezone);
    string_t *gmt = lab_cal_jd_text(jd, 0, false);
    string_t *mag = string_sprintf("%.3f", magnitude);
    bool near_total = strcmp(lab_cal_text(raw, "kind"), "total") && percent < 100 && round(percent * 10) >= 1000;
    string_t *obscuration = string_sprintf(near_total ? "%.3f%%" : "%.1f%%", percent);
    lab_cal_set(event, "time", string_c_str(greatest));
    lab_cal_set(event, "greatest", string_c_str(greatest));
    lab_cal_set(event, "first_contact", string_c_str(begin));
    lab_cal_set(event, "fourth_contact", string_c_str(end));
    lab_cal_set(event, "gmt_time", string_c_str(gmt));
    lab_cal_set(event, "magnitude", string_c_str(mag));
    lab_cal_set(event, "obscuration", string_c_str(obscuration));
    lab_cal_set(event, "details", "");
    lab_cal_set(event, "nearest_totality", "");
    lab_cal_take(event, "nearest_totality_action", json_new_object());
    string_free(greatest);
    string_free(begin);
    string_free(end);
    string_free(gmt);
    string_free(mag);
    string_free(obscuration);
    return event;
}

static int lab_cal_compare_events(const void *left, const void *right)
{
    const json_t *a = *(const json_t *const *)left, *b = *(const json_t *const *)right;
    return strcmp(lab_cal_text(a, "time"), lab_cal_text(b, "time"));
}

static json_t *lab_cal_sort_events(json_t *events)
{
    size_t count = json_array_size(events);
    const json_t **order = calloc(count ? count : 1, sizeof(*order));
    if (!order)
        return events;
    for (size_t i = 0; i < count; ++i)
        order[i] = json_array_get(events, i);
    qsort(order, count, sizeof(*order), lab_cal_compare_events);
    json_t *sorted = json_new_array();
    for (size_t i = 0; i < count; ++i)
        json_array_append(sorted, order[i]);
    free(order);
    json_free(events);
    return sorted;
}

static void lab_cal_add_transits(json_t *events, const char *start, const char *end)
{
    /* The same eight historical/future transit dates supplied by the legacy UI. */
    static const char *dates[] = {"2003-05-07", "2004-06-08", "2006-11-08", "2012-06-06",
                                  "2016-05-09", "2019-11-11", "2032-11-13", "2039-11-07"};
    static const char *bodies[] = {"Mercury", "Venus", "Mercury", "Venus", "Mercury", "Mercury", "Mercury", "Mercury"};
    for (size_t i = 0; i < sizeof(dates) / sizeof(*dates); ++i) {
        if (strcmp(dates[i], start) < 0 || strcmp(dates[i], end) > 0)
            continue;
        json_t *event = json_new_object();
        static const char *empty[] = {"nearest_totality", "jd",       "magnitude",      "obscuration",
                                      "first_contact",    "greatest", "fourth_contact", "gmt_time",
                                      "first_jd",         "last_jd"};
        for (size_t j = 0; j < sizeof(empty) / sizeof(*empty); ++j)
            lab_cal_set(event, empty[j], "");
        string_t *name = string_sprintf("%s transit", bodies[i]);
        string_t *details =
            string_sprintf("%s crosses the solar disc; circumstances depend on observer location.", bodies[i]);
        lab_cal_set(event, "name", string_c_str(name));
        lab_cal_set(event, "details", string_c_str(details));
        lab_cal_set(event, "category", "Inner planet");
        lab_cal_set(event, "kind", "solar transit");
        lab_cal_set(event, "time", dates[i]);
        lab_cal_append(events, event);
        string_free(name);
        string_free(details);
    }
}

/* Produce the worksheet contract consumed by the existing Almanac browser. */
json_t *lab_cal_almanac(json_t *fields, const json_t *options, unsigned *status)
{
    string_t *error = NULL;
    json_t *all_rows = lab_cal_body_rows(fields, &error);
    if (!all_rows) {
        json_t *failure = lab_cal_error(status, 422, string_c_str(error));
        string_free(error);
        return failure;
    }
    datetime_t *date = datetime_from_string(lab_cal_text(options, "date"));
    int year = datetime_year(date), month = datetime_month(date), day = datetime_day(date);
    if (month == 2 && day == 29)
        day = 28;
    string_t *end_date = string_sprintf("%04d-%02d-%02d", year + 1, month, day);
    size_t clock_count = 0;
    string_t *clock_text = string_new_with(lab_cal_text(options, "time"));
    string_t **clock_parts = lab_cal_split(clock_text, ":", &clock_count);
    double hour = 0, minute = 0, second = 0;
    bool clock_valid = lab_cal_number(lab_cal_part(clock_parts, clock_count, 0), &hour) &&
                       lab_cal_number(lab_cal_part(clock_parts, clock_count, 1), &minute) &&
                       (clock_count == 2 || lab_cal_number(lab_cal_part(clock_parts, clock_count, 2), &second));
    if (!clock_valid) {
        string_split_free(clock_parts, clock_count);
        string_free(clock_text);
        string_free(end_date);
        datetime_dealloc(date);
        json_free(all_rows);
        return lab_cal_error(status, 400, "Bad request: invalid almanac time");
    }
    double fraction = (hour * 3600 + minute * 60 + second) / 86400;
    double start_jd = datetime_jdn(date) - 0.5 + fraction;
    datetime_t *end_moment = datetime_from_string(string_c_str(end_date));
    double end_jd = datetime_jdn(end_moment) - 0.5 + fraction;
    datetime_dealloc(end_moment);
    string_t *normal_clock = string_sprintf("%02d:%02d:%02d", (int)hour, (int)minute, (int)second);
    if (second != trunc(second)) {
        string_free(normal_clock);
        normal_clock = string_sprintf("%02d:%02d:%09.6f", (int)hour, (int)minute, second);
    }
    string_split_free(clock_parts, clock_count);
    string_free(clock_text);
    string_t *search_start = lab_cal_jd_text(start_jd - 1, 0, true), *search_end = lab_cal_jd_text(end_jd + 1, 0, true);
    json_t *raw = lab_cal_run_events(options, string_c_str(search_start), string_c_str(search_end), false, &error);
    string_free(search_start);
    string_free(search_end);
    datetime_dealloc(date);
    if (!raw) {
        json_t *failure = lab_cal_error(status, 422, error ? string_c_str(error) : "Almanac event helper failed");
        string_free(error);
        string_free(end_date);
        string_free(normal_clock);
        json_free(all_rows);
        return failure;
    }
    string_free(error);
    json_t *events = json_new_array(), *rows = json_new_array();
    for (size_t i = 0; i < json_array_size(raw); ++i) {
        const json_t *entry = json_array_get(raw, i);
        double first_jd, last_jd;
        json_t *event = lab_cal_present_event(entry, options);
        if (!event || !lab_cal_number(lab_cal_text(entry, "first_jd"), &first_jd) ||
            !lab_cal_number(lab_cal_text(entry, "last_jd"), &last_jd)) {
            json_free(event);
            json_free(raw);
            json_free(events);
            json_free(rows);
            json_free(all_rows);
            string_free(end_date);
            string_free(normal_clock);
            return lab_cal_error(status, 422, "Malformed native almanac event row");
        }
        if (last_jd >= start_jd && first_jd <= end_jd)
            lab_cal_append(events, event);
        else
            json_free(event);
    }
    json_free(raw);
    lab_cal_add_transits(events, lab_cal_text(options, "date"), string_c_str(end_date));
    events = lab_cal_sort_events(events);
    bool visible_only = !strcmp(lab_cal_text(options, "visibility"), "visible");
    for (size_t i = 0; i < json_array_size(all_rows); ++i) {
        const json_t *row = json_array_get(all_rows, i);
        if (!visible_only || !strcmp(lab_cal_text(row, "visible"), "YES"))
            json_array_append(rows, row);
    }
    lab_cal_set(fields, "jurisdiction", lab_cal_text(options, "jurisdiction"));
    lab_cal_set(fields, "town", lab_cal_text(options, "town"));
    lab_cal_set(fields, "timezone", lab_cal_text(options, "timezone"));
    lab_cal_set(fields, "visibility", lab_cal_text(options, "visibility"));
    json_t *response = json_new_object();
    lab_cal_take(response, "ok", json_new_bool(true));
    lab_cal_set(response, "mode", "almanac");
    lab_cal_set(response, "worksheet_title", "AstroNav Navigation Almanac");
    string_t *moment =
        string_sprintf("GMT moment: %s %s", lab_cal_text(options, "date"), lab_cal_text(options, "time"));
    string_t *town = string_new_with(lab_cal_text(options, "town"));
    size_t town_count = 0;
    string_t **town_parts = lab_cal_split(town, "|", &town_count);
    string_t *observer =
        string_sprintf("Observer: %s%s%s, zone %s, latitude %s, longitude %s", lab_cal_part(town_parts, town_count, 0),
                       *lab_cal_part(town_parts, town_count, 0) ? ", " : "", lab_cal_text(options, "jurisdiction"),
                       lab_cal_text(options, "zone"), lab_cal_text(options, "lat"), lab_cal_text(options, "lon"));
    string_t *title =
        string_sprintf("Upcoming eclipses and inner planetary transits through %s", string_c_str(end_date));
    string_t *event_year = string_sprintf("%04d", year);
    string_t *window = string_sprintf("%sT%s+00:00|%sT%s+00:00", lab_cal_text(options, "date"),
                                      string_c_str(normal_clock), string_c_str(end_date), string_c_str(normal_clock));
    lab_cal_set(response, "moment_text", string_c_str(moment));
    lab_cal_set(response, "observer_text", string_c_str(observer));
    lab_cal_set(response, "body_text",
                visible_only ? "Location of Navigational Bodies; visible bodies only"
                             : "Location of Navigational Bodies; all bodies shown");
    lab_cal_set(response, "event_title", string_c_str(title));
    lab_cal_set(response, "event_year", string_c_str(event_year));
    lab_cal_set(response, "event_window", string_c_str(window));
    lab_cal_set(response, "selected_code", "");
    lab_cal_set(response, "selected_summary", "");
    lab_cal_set(response, "visibility", lab_cal_text(options, "visibility"));
    string_t *worksheet = string_sprintf("AstroNav Navigation Almanac\n%s\n%s\n\n"
                                         "Body | Declination | GHA | RA | Altitude | Azimuth | s.d. | Vmag. | Visible",
                                         string_c_str(moment), string_c_str(observer));
    static const char *body_columns[] = {"name",    "declination",   "gha",       "right_ascension", "altitude",
                                         "azimuth", "semi_diameter", "magnitude", "visible"};
    for (size_t i = 0; i < json_array_size(rows); ++i) {
        string_append_char(worksheet, '\n');
        for (size_t j = 0; j < sizeof(body_columns) / sizeof(*body_columns); ++j)
            string_append_format(worksheet, "%s%s", j ? " | " : "",
                                 lab_cal_text(json_array_get(rows, i), body_columns[j]));
    }
    string_t *listing = string_new();
    for (size_t i = 0; i < json_array_size(events); ++i) {
        const json_t *event = json_array_get(events, i);
        string_append_format(listing, "%s%s  %s  %s (%s)", i ? "\n" : "", lab_cal_text(event, "time"),
                             lab_cal_text(event, "category"), lab_cal_text(event, "name"), lab_cal_text(event, "kind"));
    }
    string_append_format(worksheet, "\n\n%s\n%s", string_c_str(title), string_c_str(listing));
    lab_cal_set(response, "worksheet", string_c_str(worksheet));
    lab_cal_set(response, "events_listing", string_c_str(listing));
    lab_cal_set(response, "notes",
                "Packaged ephemeris coverage: 1550-2649 GMT. "
                "Navigation body positions are reported rounded to the nearest arc-second.");
    lab_cal_take(response, "all_rows", all_rows);
    lab_cal_take(response, "rows", rows);
    lab_cal_take(response, "events", events);
    lab_cal_take(response, "fields", json_clone(fields));
    string_split_free(town_parts, town_count);
    string_free(town);
    string_free(end_date);
    string_free(moment);
    string_free(observer);
    string_free(title);
    string_free(event_year);
    string_free(window);
    string_free(normal_clock);
    string_free(worksheet);
    string_free(listing);
    if (!lab_cal_almanac_presentation(response)) {
        json_free(response);
        return lab_cal_error(status, 500, "Could not prepare bounded almanac presentation");
    }
    return response;
}

static json_t *lab_cal_totality_item(const char *requested_jd, const char *payload, const json_t *options)
{
    string_t *text = string_new_with(payload);
    size_t count = 0;
    string_t **parts = lab_cal_split(text, "\t", &count);
    double jd, latitude, longitude, distance, zone, elevation = 0;
    json_t *item = NULL;
    bool valid =
        count >= 8 &&
        (!strcmp(lab_cal_part(parts, count, 0), "town") || !strcmp(lab_cal_part(parts, count, 0), "near_town")) &&
        lab_cal_number(lab_cal_part(parts, count, 4), &latitude) &&
        lab_cal_number(lab_cal_part(parts, count, 5), &longitude) &&
        lab_cal_number(lab_cal_part(parts, count, 6), &jd) &&
        lab_cal_number(lab_cal_part(parts, count, 7), &distance) &&
        lab_cal_number(lab_cal_text(options, "zone"), &zone);
    if (valid && count > 8)
        valid = lab_cal_number(lab_cal_part(parts, count, 8), &elevation);
    if (valid) {
        const char *jurisdiction = lab_cal_part(parts, count, 2);
        const char *timezone = lab_cal_part(parts, count, 3);
        zone = lab_cal_event_zone(jd, jurisdiction, zone, timezone);
        string_t *local = lab_cal_jd_text(jd, zone, false);
        string_t *display_time = lab_cal_civil_time(jd, jurisdiction, zone, timezone);
        string_t *date = string_substring(local, 0, 10), *clock = string_substring(local, 11, 8);
        string_t *zone_text = string_sprintf("%.2f", zone);
        string_t *lat = string_sprintf("%.6f", latitude), *lon = string_sprintf("%.6f", longitude);
        string_t *height = string_sprintf("%.0f", elevation);
        string_t *town = string_sprintf("%s|%s|%s|%s", lab_cal_part(parts, count, 1), string_c_str(lat),
                                        string_c_str(lon), string_c_str(height));
        string_t *label = string_sprintf("%s%s, %s; %.4f, %.4f; %s; %.0f km from observer",
                                         !strcmp(lab_cal_part(parts, count, 0), "near_town") ? "Nearest town: " : "",
                                         lab_cal_part(parts, count, 1), jurisdiction, latitude, longitude,
                                         string_c_str(display_time), distance);
        json_t *action = json_new_object();
        lab_cal_set(action, "date", string_c_str(date));
        lab_cal_set(action, "time", string_c_str(clock));
        lab_cal_set(action, "zone", string_c_str(zone_text));
        lab_cal_set(action, "jurisdiction", jurisdiction);
        lab_cal_set(action, "town", string_c_str(town));
        lab_cal_set(action, "latitude", string_c_str(lat));
        lab_cal_set(action, "longitude", string_c_str(lon));
        lab_cal_set(action, "elevation", string_c_str(height));
        item = json_new_object();
        lab_cal_set(item, "jd", requested_jd);
        lab_cal_set(item, "nearest_totality", string_c_str(label));
        lab_cal_take(item, "nearest_totality_action", action);
        string_t *html = lab_cal_totality_markup(string_c_str(label), lab_cal_get(item, "nearest_totality_action"));
        if (html)
            lab_cal_set(item, "html", string_c_str(html));
        else {
            json_free(item);
            item = NULL;
        }
        string_free(html);
        string_free(local);
        string_free(display_time);
        string_free(date);
        string_free(clock);
        string_free(zone_text);
        string_free(lat);
        string_free(lon);
        string_free(height);
        string_free(town);
        string_free(label);
    }
    string_split_free(parts, count);
    string_free(text);
    return item;
}

/* Search the native event helper and preserve the browser's per-JD update keys. */
json_t *lab_cal_totality(const json_t *payload, const json_t *options, unsigned *status)
{
    unsigned year;
    string_t *date = string_new_with(lab_cal_text(options, "date"));
    string_t *default_year = string_substring(date, 0, 4);
    const char *year_text =
        lab_cal_get(payload, "event_year") ? lab_cal_text(payload, "event_year") : string_c_str(default_year);
    bool valid = lab_cal_integer(year_text, 9998, &year) && year >= 1;
    string_free(date);
    string_free(default_year);
    if (!valid)
        return lab_cal_error(status, 400, "Bad request: Event year is outside the supported range");
    const json_t *requested = lab_cal_get(payload, "events");
    if (requested && json_type(requested) != JSON_ARRAY)
        return lab_cal_error(status, 400, "Bad request: events must be an array");
    if (json_array_size(requested) > 32)
        return lab_cal_error(status, 400, "Bad request: at most 32 events may be requested");
    json_t *targets = json_new_array();
    for (size_t i = 0; i < json_array_size(requested); ++i) {
        const json_t *entry = json_array_get(requested, i);
        const string_t *scalar = json_type(entry) == JSON_NUMBER ? json_number_text(entry) : json_string_value(entry);
        const char *text = json_type(entry) == JSON_OBJECT ? lab_cal_text(entry, "jd")
                           : scalar                        ? string_c_str(scalar)
                                                           : "";
        double jd;
        if (!lab_cal_number(text, &jd) || jd < 1721426.5 || jd > 5373118.5) {
            json_free(targets);
            return lab_cal_error(status, 400, "Bad request: invalid event Julian date");
        }
        json_t *target = json_new_object();
        lab_cal_set(target, "jd", text);
        lab_cal_append(targets, target);
    }
    string_t *error = NULL;
    if (!json_array_size(targets)) {
        string_t *start = string_sprintf("%04u-01-01", year), *end = string_sprintf("%04u-01-01", year + 1);
        json_free(targets);
        targets = lab_cal_run_events(options, string_c_str(start), string_c_str(end), false, &error);
        string_free(start);
        string_free(end);
        if (!targets) {
            json_t *failure = lab_cal_error(status, 422, error ? string_c_str(error) : "Almanac event helper failed");
            string_free(error);
            return failure;
        }
        string_free(error);
        error = NULL;
    }
    json_t *items = json_new_array();
    bool timed_out = false;
    for (size_t i = 0; i < json_array_size(targets); ++i) {
        const json_t *target = json_array_get(targets, i);
        if (*lab_cal_text(target, "category") && strcmp(lab_cal_text(target, "category"), "Solar"))
            continue;
        double jd;
        if (!lab_cal_number(lab_cal_text(target, "jd"), &jd))
            continue;
        string_t *start = lab_cal_jd_text(jd - 1, 0, true), *end = lab_cal_jd_text(jd + 1, 0, true);
        json_t *land = lab_cal_run_events(options, string_c_str(start), string_c_str(end), true, &error);
        int saved_error = errno;
        string_free(start);
        string_free(end);
        if (!land) {
            if (saved_error == ETIMEDOUT) {
                timed_out = true;
                string_free(error);
                error = NULL;
                break;
            }
            json_t *failure = lab_cal_error(status, 422, error ? string_c_str(error) : "Land-totality helper failed");
            string_free(error);
            json_free(items);
            json_free(targets);
            return failure;
        }
        string_free(error);
        error = NULL;
        for (size_t j = 0; j < json_array_size(land); ++j) {
            const json_t *row = json_array_get(land, j);
            double found_jd;
            if (strcmp(lab_cal_text(row, "category"), "Solar") || !lab_cal_number(lab_cal_text(row, "jd"), &found_jd) ||
                fabs(found_jd - jd) > 0.1)
                continue;
            json_t *item =
                lab_cal_totality_item(lab_cal_text(target, "jd"), lab_cal_text(row, "nearest_totality"), options);
            if (item)
                lab_cal_append(items, item);
        }
        json_free(land);
    }
    json_free(targets);
    json_t *response = json_new_object();
    lab_cal_take(response, "ok", json_new_bool(true));
    lab_cal_take(response, "items", items);
    lab_cal_take(response, "timed_out", json_new_bool(timed_out));
    return response;
}
