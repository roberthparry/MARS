/**
 * @file lab_calendar_weather_cache.c
 * @brief Bounded private on-disk cache of recent local weather observations for calendar fallback estimates.
 *
 * DateTime and Almanac workers share 64 directly indexed slots under MARS_HOME. Coordinates rounded to
 * three decimal places form each key; collisions replace a slot but never match another location.
 * Non-blocking file locks keep initial evaluation independent of background weather work. Only a small
 * validated observation is stored, never API credentials or a whole provider response. Both retrieval
 * and observation ages must be less than 48 hours, and the selected moment must be within that interval.
 * A separate 64-slot cache retains calculated provider conditions for 15 minutes, matched to the exact
 * coordinates, UTC moment and elevation, so returning to a worksheet need not flash seasonal estimates.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "file-t.h"
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_internal.h"

static string_t *weather_location(const json_t *options)
{
    double latitude, longitude;
    if (!lab_cal_number(lab_cal_text(options, "lat"), &latitude) || fabs(latitude) > 90 ||
        !lab_cal_number(lab_cal_text(options, "lon"), &longitude) || fabs(longitude) > 180) return NULL;
    return string_sprintf("%.3f,%.3f", latitude == 0 ? 0 : latitude, longitude == 0 ? 0 : longitude);
}

static file_t *weather_slot(const string_t *location, bool writing, const char *prefix)
{
    const char *configured = getenv("MARS_HOME"), *home = getenv("HOME");
    string_t *base = configured && *configured ? string_new_with(configured) :
                     home && *home ? string_sprintf("%s/.mars", home) : NULL;
    string_t *path = base ? string_sprintf("%s/lab/weather-cache", string_c_str(base)) : NULL;
    file_t *directory = path ? file_new(path) : NULL;
    bool ready = directory && (!writing || file_create_directory(directory, 0700, true));
    string_t *name = ready ? string_sprintf("%s/%s%02lu.json", string_c_str(path), prefix, string_hash(location) % 64) : NULL;
    file_t *slot = name ? file_new(name) : NULL;
    mode_t previous = umask(0077);
    bool opened = slot && file_open(slot, writing ? FILE_MODE_OPEN_OR_CREATE : FILE_MODE_OPEN,
                                    writing ? FILE_ACCESS_READ_WRITE : FILE_ACCESS_READ);
    umask(previous);
    if (!opened || !file_lock(slot, writing, false)) { file_free(slot); slot = NULL; }
    file_free(directory);
    string_free(name);
    string_free(path);
    string_free(base);
    return slot;
}

static json_t *weather_record(file_t *slot)
{
    char bytes[2049];
    size_t count = 0;
    if (!slot || !file_read(slot, bytes, sizeof bytes, &count) || !count || count > 2048) return NULL;
    string_t *text = string_new();
    json_t *record = text && !string_append_bytes(text, bytes, count) ? json_from_text(text) : NULL;
    string_free(text);
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
    file_t *slot = location ? weather_slot(location, true, "") : NULL;
    json_t *old = weather_record(slot);
    double previous = 0;
    bool newer = old && !strcmp(lab_cal_text(old, "location"), string_c_str(location)) &&
                 lab_cal_number(lab_cal_text(old, "observed"), &previous) && previous > observed;
    if (slot && !newer) {
        json_t *record = json_new_object();
        string_t *retrieved = string_sprintf("%lld", (long long)now);
        lab_cal_set(record, "location", string_c_str(location));
        lab_cal_set(record, "retrieved", string_c_str(retrieved));
        lab_cal_set(record, "observed", lab_cal_text(current, "last_updated_epoch"));
        lab_cal_set(record, "temp_c", lab_cal_text(current, "temp_c"));
        string_t *text = json_to_string(record);
        if (text && file_seek(slot, 0, FILE_SEEK_BEGIN) && file_truncate(slot, 0))
            (void)file_write_text(slot, text);
        string_free(text);
        string_free(retrieved);
        json_free(record);
    }
    json_free(old);
    file_free(slot);
    string_free(location);
}

/* Reuse temperature only as an estimate; pressure keeps its independent provider/elevation policy. */
json_t *lab_cal_weather_recent(const json_t *options, time_t now)
{
    string_t *location = weather_location(options);
    file_t *slot = location ? weather_slot(location, false, "") : NULL;
    json_t *record = weather_record(slot);
    double retrieved, observed, temperature, epoch = lab_cal_weather_epoch(options);
    bool valid = record && !strcmp(lab_cal_text(record, "location"), string_c_str(location)) &&
                 lab_cal_number(lab_cal_text(record, "retrieved"), &retrieved) && retrieved <= now && now - retrieved < 172800 &&
                 lab_cal_number(lab_cal_text(record, "observed"), &observed) && observed <= now && now - observed < 172800 &&
                 isfinite(epoch) && fabs(epoch - observed) <= 3600 && now - observed <= 3600 &&
                 lab_cal_number(lab_cal_text(record, "temp_c"), &temperature) && temperature >= -100 && temperature <= 60;
    if (!valid) { json_free(record); record = NULL; }
    file_free(slot);
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
    file_t *slot = key ? weather_slot(key, true, "conditions-") : NULL;
    if (slot) {
        json_t *record = json_new_object();
        string_t *retrieved = string_sprintf("%lld", (long long)now);
        lab_cal_set(record, "key", string_c_str(key));
        lab_cal_set(record, "retrieved", string_c_str(retrieved));
        weather_conditions_copy(record, options);
        string_t *text = json_to_string(record);
        if (text && string_byte_length(text) <= 2048 && file_seek(slot, 0, FILE_SEEK_BEGIN) && file_truncate(slot, 0))
            (void)file_write_text(slot, text);
        string_free(text);
        string_free(retrieved);
        json_free(record);
    }
    file_free(slot);
    string_free(key);
}

/* An exact, fresh match can serve the initial calculation without waiting for network access. */
bool lab_cal_weather_conditions_restore(json_t *options, time_t now)
{
    string_t *key = weather_conditions_key(options);
    file_t *slot = key ? weather_slot(key, false, "conditions-") : NULL;
    json_t *record = weather_record(slot);
    double retrieved;
    bool valid = key && record && !strcmp(lab_cal_text(record, "key"), string_c_str(key)) &&
                 lab_cal_number(lab_cal_text(record, "retrieved"), &retrieved) && retrieved <= now && now - retrieved < 900 &&
                 weather_conditions_valid(record);
    if (valid) weather_conditions_copy(options, record);
    json_free(record);
    file_free(slot);
    string_free(key);
    return valid;
}
