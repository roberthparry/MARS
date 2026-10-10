/**
 * @file lab_calendar.c
 * @brief Request validation and native worker execution for MARS Lab calendars.
 *
 * Implements the five calendar routes without a shell or Python interpreter.
 * Only allowlisted arguments reach the worker programmes. Process failures are
 * distinguished from optional holiday database unavailability. Native almanac
 * and jurisdiction modules load their own database paths and keys, including
 * MARS_HOME/config files; no process-global environment changes are made here.
 */
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "file.h"
#include "lab_calendar_internal.h"
#include "lab_page.h"
#include "lab_process.h"
#include "lab_state.h"

/* Borrow an object member without transferring ownership. */
const json_t *lab_cal_get(const json_t *object, const char *key)
{
    string_t *name = string_new_with(key);
    const json_t *value = json_object_get(object, name);
    string_free(name);
    return value;
}

/* Borrow scalar text used by both the browser and worker interfaces. */
const char *lab_cal_text(const json_t *object, const char *key)
{
    const json_t *value = lab_cal_get(object, key);
    const string_t *text = json_type(value) == JSON_NUMBER ? json_number_text(value) : json_string_value(value);
    return text ? string_c_str(text) : "";
}

/* Copy then dispose of an owned JSON value. */
void lab_cal_take(json_t *object, const char *key, json_t *value)
{
    string_t *name = string_new_with(key);
    json_object_set(object, name, value);
    string_free(name);
    json_free(value);
}

/* Store copied UTF-8 text. */
void lab_cal_set(json_t *object, const char *key, const char *value)
{
    string_t *text = string_new_with(value ? value : "");
    lab_cal_take(object, key, json_new_string(text));
    string_free(text);
}

/* Append by copy, following the JSON module's ownership contract. */
void lab_cal_append(json_t *array, json_t *value)
{
    json_array_append(array, value);
    json_free(value);
}

/* Return a browser-compatible error envelope. */
json_t *lab_cal_error(unsigned *status, unsigned code, const char *message)
{
    json_t *response = json_new_object();
    if (status)
        *status = code;
    lab_cal_take(response, "ok", json_new_bool(false));
    lab_cal_set(response, "error", message);
    return response;
}

/* Read only the requested key, never interpreting shell syntax. */
string_t *lab_cal_config(const char *filename, const char *key)
{
    const char *environment = getenv(key);
    if (environment && *environment) {
        string_t *value = string_new_with(environment);
        string_trim(value);
        if (string_byte_length(value))
            return value;
        string_free(value);
    }
    const char *home = getenv("HOME");
    const char *mars_home = getenv("MARS_HOME");
    string_t *base =
        mars_home && *mars_home ? string_new_with(mars_home) : string_sprintf("%s/.mars", home ? home : ".");
    string_t *path = string_sprintf("%s/config/%s", string_c_str(base), filename);
    file_t *file = file_new(path);
    string_t *result = NULL;
    string_t *line = NULL;
    if (file_open_read(file)) {
        while (file_read_line(file, &line) && line) {
            string_trim(line);
            string_cursor_t *cursor = string_cursor_new(line);
            string_cursor_consume(cursor, "export ");
            if (string_cursor_consume(cursor, key) && string_cursor_consume(cursor, "=")) {
                result = string_cursor_slice_between(string_cursor_position(cursor), string_cursor_end_position(cursor),
                                                     cursor);
                string_trim(result);
                size_t size = string_length(result);
                rune_t first = string_at(result, 0), last = string_at(result, size ? size - 1 : 0);
                if (size >= 2 && (rune_is_equal(first, '\'') || rune_is_equal(first, '"')) &&
                    rune_value(first) == rune_value(last)) {
                    string_t *unquoted = string_substring(result, 1, size - 2);
                    string_free(result);
                    result = unquoted;
                }
                string_cursor_free(cursor);
                string_free(line);
                line = NULL;
                break;
            }
            string_cursor_free(cursor);
            string_free(line);
            line = NULL;
        }
    }
    string_free(line);
    file_free(file);
    string_free(path);
    string_free(base);
    return result ? result : string_new();
}

static json_t *lab_cal_parse_output(const string_t *output)
{
    json_t *fields = json_new_object();
    size_t count = 0;
    string_t **lines = lab_cal_split(output, "\n", &count);
    string_t *section = NULL;
    for (size_t i = 0; i < count; ++i) {
        if (section && string_find(lines[i], "\t") >= 0 && !string_starts_with(lines[i], "calendar_section_")) {
            string_t *value =
                string_sprintf("%s\n%s", lab_cal_text(fields, string_c_str(section)), string_c_str(lines[i]));
            lab_cal_set(fields, string_c_str(section), string_c_str(value));
            string_free(value);
            continue;
        }
        string_free(section);
        section = NULL;
        string_cursor_t *cursor = string_cursor_new(lines[i]);
        string_pos_t start = string_cursor_position(cursor);
        while (!string_cursor_done(cursor) && !string_cursor_match(cursor, " "))
            string_cursor_next(cursor);
        if (string_cursor_done(cursor)) {
            string_cursor_free(cursor);
            continue;
        }
        string_t *key = string_cursor_extract(start, cursor);
        string_cursor_next(cursor);
        const char *name = string_c_str(key);
        string_t *value =
            string_cursor_slice_between(string_cursor_position(cursor), string_cursor_end_position(cursor), cursor);
        string_cursor_free(cursor);
        string_trim(value);
        bool repeated = !strcmp(name, "snapshot") || !strcmp(name, "event") || !strcmp(name, "bank_holiday") ||
                        !strcmp(name, "holiday_notice");
        if (repeated && *lab_cal_text(fields, name)) {
            string_t *joined = string_sprintf("%s\n%s", lab_cal_text(fields, name), string_c_str(value));
            string_free(value);
            value = joined;
        }
        lab_cal_set(fields, name, string_c_str(value));
        if (string_starts_with(key, "calendar_section_"))
            section = string_clone(key);
        string_free(key);
        string_free(value);
    }
    string_free(section);
    string_split_free(lines, count);
    return fields;
}

/* Execute argv directly; the caller owns any returned diagnostic. */
json_t *lab_cal_run(const char *name, const json_t *options, unsigned timeout, string_t **diagnostic)
{
    const char *root = getenv("MARS_LAB_ROOT");
    if (!root || !*root)
        root = ".";
    string_t *path = lab_proc_worker_path(name);
    if (!path) {
        int resolution_error = errno;
        if (diagnostic)
            *diagnostic = string_sprintf("Cannot resolve native calendar worker %s.", name);
        errno = resolution_error;
        return NULL;
    }
    size_t count = json_object_size(options);
    if (count > 24) {
        string_free(path);
        return NULL;
    }
    string_t *arguments[24] = {0};
    const char *argv[26] = {string_c_str(path)};
    for (size_t i = 0; i < count; ++i) {
        const string_t *key = json_object_key_at(options, i);
        arguments[i] = string_sprintf("%s=%s", string_c_str(key), lab_cal_text(options, string_c_str(key)));
        argv[i + 1] = string_c_str(arguments[i]);
    }
    string_t *output = NULL;
    int exit_status = -1;
    bool ran = lab_proc_run(argv, root, timeout, 4u * 1024u * 1024u, &output, &exit_status);
    int process_error = errno;
    json_t *fields = ran && exit_status == 0 && output ? lab_cal_parse_output(output) : NULL;
    if (!fields) {
        /* Always retain crash/exit information, even if the child printed ordinary fields first. */
        if (!output)
            output = string_new();
        string_append_format(output, "\n%s failed (exit %d, process error %d).", name, exit_status,
                             ran ? 0 : process_error);
    }
    if (diagnostic)
        *diagnostic = output;
    else
        string_free(output);
    for (size_t i = 0; i < count; ++i)
        string_free(arguments[i]);
    string_free(path);
    errno = ran ? (exit_status == 0 ? 0 : EIO) : process_error;
    return fields;
}

static bool lab_cal_valid_date(const char *text)
{
    string_t *source = string_new_with(text);
    size_t count = 0;
    string_t **parts = lab_cal_split(source, "-", &count);
    unsigned year = 0, month = 0, day = 0;
    bool valid = count == 3 && string_length(parts[0]) == 4 && string_length(parts[1]) == 2 &&
                 string_length(parts[2]) == 2 && lab_cal_integer(string_c_str(parts[0]), 9999, &year) &&
                 lab_cal_integer(string_c_str(parts[1]), 12, &month) &&
                 lab_cal_integer(string_c_str(parts[2]), 31, &day);
    string_split_free(parts, count);
    string_free(source);
    static const int month_days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (!valid || year < 1 || month < 1)
        return false;
    int days = month_days[month] + (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    return day >= 1 && day <= (unsigned)days;
}

static bool lab_cal_numeric(const char *text, double minimum, double maximum)
{
    double value;
    return lab_cal_number(text, &value) && value >= minimum && value <= maximum;
}

static bool lab_cal_valid_time(const char *text)
{
    string_t *source = string_new_with(text);
    size_t count = 0;
    string_t **parts = lab_cal_split(source, ":", &count);
    unsigned hour, minute;
    bool valid = (count == 2 || count == 3) && string_length(parts[0]) == 2 && string_length(parts[1]) == 2 &&
                 lab_cal_integer(string_c_str(parts[0]), 23, &hour) &&
                 lab_cal_integer(string_c_str(parts[1]), 59, &minute);
    if (valid && count == 3) {
        string_cursor_t *cursor = string_cursor_new(parts[2]);
        unsigned char ch;
        for (size_t i = 0; i < 2; ++i) {
            valid = valid && string_cursor_peek_ascii(cursor, &ch) && ch >= '0' && ch <= '9';
            string_cursor_next(cursor);
        }
        if (!string_cursor_done(cursor)) {
            valid = valid && string_cursor_consume(cursor, ".") && !string_cursor_done(cursor);
            while (!string_cursor_done(cursor)) {
                valid = valid && string_cursor_peek_ascii(cursor, &ch) && ch >= '0' && ch <= '9';
                string_cursor_next(cursor);
            }
        }
        valid = valid && lab_cal_numeric(string_c_str(parts[2]), 0, 59.999999999);
        string_cursor_free(cursor);
    }
    string_split_free(parts, count);
    string_free(source);
    return valid;
}

static void lab_cal_option(json_t *options, const json_t *payload, const char *key, const char *source,
                           const char *fallback)
{
    const json_t *value = lab_cal_get(payload, source);
    string_t *text = string_new_with(value ? lab_cal_text(payload, source) : fallback);
    string_trim(text);
    lab_cal_set(options, key, string_c_str(text));
    string_free(text);
}

static json_t *lab_cal_location(const json_t *options, unsigned *status)
{
    json_t *args = json_new_object();
    lab_cal_set(args, "date", lab_cal_text(options, "date"));
    lab_cal_set(args, "jurisdiction", lab_cal_text(options, "jurisdiction"));
    string_t *diagnostic = NULL;
    json_t *fields = lab_cal_run("holiday_lab", args, 10000, &diagnostic);
    json_free(args);
    if (!fields) {
        json_t *failure = lab_cal_error(status, 422, diagnostic ? string_c_str(diagnostic) : "Holiday helper failed");
        string_free(diagnostic);
        return failure;
    }
    string_free(diagnostic);
    if (strcmp(lab_cal_text(fields, "jurisdiction_status"), "ok")) {
        json_free(fields);
        return lab_cal_error(status, 200, "Jurisdiction location unavailable");
    }
    json_t *response = lab_cal_location_fields(fields, options);
    json_free(fields);
    return response;
}

/* Dispatch only the exact supported paths, leaving other adapters untouched. */
json_t *lab_cal_request(const string_t *route, const json_t *payload, unsigned *status)
{
    const char *path = route ? string_c_str(route) : "";
    bool is_datetime = !strcmp(path, "/datetime-eval");
    bool is_weather = !strcmp(path, "/datetime-weather");
    bool is_location = !strcmp(path, "/datetime-jurisdiction-location");
    bool is_almanac = !strcmp(path, "/almanac-eval");
    bool is_totality = !strcmp(path, "/almanac-land-totality");
    if (!is_datetime && !is_weather && !is_location && !is_almanac && !is_totality)
        return NULL;
    if (status)
        *status = 200;
    if (json_type(payload) != JSON_OBJECT)
        return lab_cal_error(status, 400, "Bad request: expected a JSON object");
    /* Embedded NULs cannot be represented in argv; reject them before conversion. */
    for (size_t i = 0; i < json_object_size(payload); ++i) {
        const string_t *value = json_string_value(json_object_value_at(payload, i));
        if (value) {
            string_cursor_t *cursor = string_cursor_new(value);
            bool nul = false;
            while (!string_cursor_done(cursor)) {
                nul = nul || rune_value(string_cursor_peek(cursor)) == 0;
                string_cursor_next(cursor);
            }
            string_cursor_free(cursor);
            if (nul)
                return lab_cal_error(status, 400, "Bad request: embedded NUL in a request field");
        }
    }
    time_t now = time(NULL);
    struct tm local = {0}, utc = {0};
    localtime_r(&now, &local);
    gmtime_r(&now, &utc);
    char today[16], clock[16];
    strftime(today, sizeof(today), "%Y-%m-%d", &local);
    strftime(clock, sizeof(clock), "%H:%M:%S", &utc);
    json_t *options = json_new_object();
    lab_cal_option(options, payload, "date", "date", today);
    json_t *defaults = NULL;
    if (!lab_cal_get(payload, "jurisdiction") ||
        (!is_location && (!lab_cal_get(payload, "latitude") || !lab_cal_get(payload, "longitude")))) {
        defaults = lab_page_defaults();
        if (!defaults) {
            json_free(options);
            return lab_cal_error(status, 500, "Calendar defaults are unavailable: cannot read the Lab page catalogue");
        }
    }
    lab_cal_option(options, payload, "lat", "latitude", lab_cal_text(defaults, "datetime_latitude"));
    lab_cal_option(options, payload, "lon", "longitude", lab_cal_text(defaults, "datetime_longitude"));
    lab_cal_option(options, payload, "jurisdiction", "jurisdiction", lab_cal_text(defaults, "datetime_jurisdiction"));
    json_free(defaults);
    lab_cal_option(options, payload, "town", "town", "");
    lab_cal_option(options, payload, "timezone", "timezone", "");
    const char *error = NULL;
    if (!lab_cal_valid_date(lab_cal_text(options, "date")))
        error = "Bad request: Date must look like YYYY-MM-DD and be a valid calendar date";
    else if (!is_location && !lab_cal_numeric(lab_cal_text(options, "lat"), -90, 90))
        error = "Bad request: Latitude must be between -90 and 90";
    else if (!is_location && !lab_cal_numeric(lab_cal_text(options, "lon"), -180, 180))
        error = "Bad request: Longitude must be between -180 and 180";
    if (is_datetime) {
        lab_cal_option(options, payload, "town", "town", "");
        lab_cal_option(options, payload, "start", "start", lab_cal_text(options, "date"));
        lab_cal_option(options, payload, "end", "end", lab_cal_text(options, "date"));
        string_t *date_value = string_new_with(lab_cal_text(options, "date"));
        string_t *year = string_substring(date_value, 0, 4);
        lab_cal_option(options, payload, "year", "year", string_c_str(year));
        string_free(year);
        string_free(date_value);
        lab_cal_option(options, payload, "elevation", "elevation", "0");
        lab_cal_option(options, payload, "gmt_offset", "gmt_offset", "");
        lab_cal_option(options, payload, "jdn", "jdn", "");
        if (!lab_cal_valid_date(lab_cal_text(options, "start")) || !lab_cal_valid_date(lab_cal_text(options, "end")))
            error = "Bad request: invalid range date";
        unsigned value;
        if (!lab_cal_integer(lab_cal_text(options, "year"), 1000000000, &value))
            error = "Bad request: Year must be an integer";
        else {
            string_t *clamped = string_sprintf("%u", value < 1 ? 1 : value > 9999 ? 9999 : value);
            lab_cal_set(options, "year", string_c_str(clamped));
            string_free(clamped);
        }
        if (!lab_cal_numeric(lab_cal_text(options, "elevation"), -1e308, 1e308))
            error = "Bad request: Elevation must be finite";
        const char *offset = lab_cal_text(options, "gmt_offset");
        if (*offset && !lab_cal_numeric(offset, -14, 14))
            error = "Bad request: GMT offset must be between -14 and 14";
        if (*lab_cal_text(options, "jdn") && !lab_cal_integer(lab_cal_text(options, "jdn"), 2147483647, &value))
            error = "Bad request: Julian Day Number must be a positive integer";
    }
    if (is_almanac || is_totality) {
        lab_cal_option(options, payload, "elevation", "elevation", "0");
        lab_cal_option(options, payload, "zone", "zone", "0");
        lab_cal_option(options, payload, "time", "time", clock);
        lab_cal_option(options, payload, "visibility", "visibility", "all");
        lab_cal_option(options, payload, "town", "town", "");
        if (!lab_cal_numeric(lab_cal_text(options, "zone"), -14, 14))
            error = "Bad request: Zone must be between -14 and 14";
        if (!lab_cal_valid_time(lab_cal_text(options, "time")))
            error = "Bad request: Time must look like HH:MM or HH:MM:SS";
        const char *visibility = lab_cal_text(options, "visibility");
        if (strcmp(visibility, "all") && strcmp(visibility, "visible"))
            error = "Bad request: Show bodies must be all or visible";
        string_t *date_value = string_new_with(lab_cal_text(options, "date"));
        bool last_year = string_starts_with(date_value, "9999-");
        string_free(date_value);
        if (is_almanac && last_year)
            error = "Bad request: upcoming event window exceeds year 9999";
    }
    if (error) {
        json_free(options);
        return lab_cal_error(status, 400, error);
    }
    string_t *timezone = lab_cal_town_timezone(options);
    lab_cal_set(options, "timezone", string_c_str(timezone));
    if (is_datetime && !*lab_cal_text(options, "gmt_offset")) {
        string_t *date = string_new_with(lab_cal_text(options, "date"));
        double offset;
        if (lab_cal_timezone_noon(timezone, date, &offset)) {
            string_t *text = string_sprintf("%.10g", offset);
            lab_cal_set(options, "gmt_offset", string_c_str(text));
            string_free(text);
        }
        string_free(date);
    }
    string_free(timezone);
    json_t *response = NULL;
    if (is_location)
        response = lab_cal_location(options, status);
    else if (is_weather)
        response = lab_cal_weather(options);
    else if (is_totality)
        response = lab_cal_totality(payload, options, status);
    else {
        json_t *args = json_new_object();
        static const char *datetime_keys[] = {"date", "start",     "end",        "year",         "lat",
                                              "lon",  "elevation", "gmt_offset", "jurisdiction", "jdn"};
        static const char *almanac_keys[] = {"date", "time", "zone", "lat", "lon"};
        const char *const *keys = is_datetime ? datetime_keys : almanac_keys;
        size_t count =
            is_datetime ? sizeof(datetime_keys) / sizeof(*datetime_keys) : sizeof(almanac_keys) / sizeof(*almanac_keys);
        for (size_t i = 0; i < count; ++i)
            if (*lab_cal_text(options, keys[i]))
                lab_cal_set(args, keys[i], lab_cal_text(options, keys[i]));
        if (is_almanac)
            lab_cal_set(args, "body", "MOON");
        string_t *diagnostic = NULL;
        json_t *fields = lab_cal_run(is_datetime ? "datetime_lab" : "almanac_lab", args, 10000, &diagnostic);
        json_free(args);
        if (!fields)
            response = lab_cal_error(status, 422,
                                     diagnostic && string_byte_length(diagnostic)
                                         ? string_c_str(diagnostic)
                                         : "Native calendar backend unavailable or timed out; run make lab-workers.");
        else if (is_datetime) {
            if (!lab_cal_valid_date(lab_cal_text(fields, "date")) || !*lab_cal_text(fields, "julian_day_number")) {
                json_free(fields);
                string_free(diagnostic);
                json_free(options);
                return lab_cal_error(status, 422, "Malformed native datetime output");
            }
            args = json_new_object();
            lab_cal_set(args, "start", lab_cal_text(options, "start"));
            lab_cal_set(args, "end", lab_cal_text(options, "end"));
            lab_cal_set(args, "jurisdiction", lab_cal_text(options, "jurisdiction"));
            string_t *holiday_error = NULL;
            json_t *holidays = lab_cal_run("holiday_lab", args, 10000, &holiday_error);
            json_free(args);
            if (!holidays) {
                response =
                    lab_cal_error(status, 422, holiday_error ? string_c_str(holiday_error) : "Holiday helper failed");
                string_free(holiday_error);
                json_free(fields);
                string_free(diagnostic);
                json_free(options);
                return response;
            }
            string_free(holiday_error);
            for (size_t i = 0; i < json_object_size(holidays); ++i)
                json_object_set(fields, json_object_key_at(holidays, i), json_object_value_at(holidays, i));
            if (strcmp(lab_cal_text(holidays, "holiday_status"), "ok"))
                lab_cal_set(fields, "holiday_notice",
                            "No holiday rules are available yet for the selected jurisdiction "
                            "and date range.");
            json_free(holidays);
            response = lab_cal_datetime(fields);
        } else
            response = lab_cal_almanac(fields, options, status);
        json_free(fields);
        string_free(diagnostic);
    }
    bool ok = false;
    if ((is_datetime || is_almanac) && json_bool_value(lab_cal_get(response, "ok"), &ok) && ok) {
        static const char *source_keys[] = {"date", "start", "end",       "year",      "jurisdiction",
                                            "town", "lat",   "lon",       "elevation", "gmt_offset",
                                            "time", "zone",  "visibility"};
        static const char *state_keys[] = {"date", "start",    "end",       "year",      "jurisdiction",
                                           "town", "latitude", "longitude", "elevation", "gmt_offset",
                                           "time", "zone",     "visibility"};
        json_t *updates = json_new_object();
        for (size_t i = 0; i < sizeof(source_keys) / sizeof(*source_keys); ++i) {
            if (!lab_cal_get(options, source_keys[i]))
                continue;
            string_t *key = string_sprintf("%s_%s", is_datetime ? "datetime" : "almanac", state_keys[i]);
            lab_cal_set(updates, string_c_str(key), lab_cal_text(options, source_keys[i]));
            string_free(key);
        }
        if (is_datetime) {
            const json_t *fields = lab_cal_get(response, "fields");
            lab_cal_set(updates, "datetime_date", lab_cal_text(fields, "date"));
            lab_cal_set(updates, "datetime_jdn", lab_cal_text(fields, "julian_day_number"));
        }
        if (!lab_state_save(updates)) {
            json_free(response);
            response = lab_cal_error(status, 500, "Calendar calculation succeeded but saving Lab state failed");
        }
        json_free(updates);
    }
    json_free(options);
    return response;
}
