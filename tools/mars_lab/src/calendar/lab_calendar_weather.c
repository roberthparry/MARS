/**
 * @file lab_calendar_weather.c
 * @brief Optional WeatherAPI daily cards and hourly observing conditions for native MARS Lab.
 *
 * Reads the user's existing weather.env configuration through file_t, performs
 * verified HTTPS requests sharing a four-second deadline and emits the browser's
 * weather fields and sections. Provider errors remain an unavailable weather
 * card and never expose the API key in diagnostics. Almanac conditions use UTC
 * timestamp matching, adjacent-hour interpolation and explicitly labelled fallbacks.
 * Hourly sea-level pressure is converted to estimated observer pressure for refraction;
 * both references remain available for the worksheet and its copied text.
 * When UTC and provider-local dates differ, one adjacent-day request can recover the missing observation hour.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "datetime-t.h"
#include "http-t.h"
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_internal.h"

static bool lab_cal_provider_number(const json_t *day, const char *key, double *value)
{
    return lab_cal_number(lab_cal_text(day, key), value);
}

static void lab_cal_measurement(json_t *fields, const char *key, double value, const char *unit)
{
    double rounded = round(value * 10) / 10;
    string_t *text = string_sprintf(rounded == trunc(rounded) ? "%.0f%s" : "%.1f%s", rounded, unit);
    lab_cal_set(fields, key, string_c_str(text));
    string_free(text);
}

static json_t *lab_cal_weather_payload(const json_t *options, const string_t *key, int span, size_t timeout_ms)
{
    string_t *escaped = http_url_encode(key);
    double latitude, longitude;
    if (!lab_cal_number(lab_cal_text(options, "lat"), &latitude) ||
        !lab_cal_number(lab_cal_text(options, "lon"), &longitude)) {
        string_free(escaped);
        return json_new_object();
    }
    const char *endpoint = span < 0 ? "history" : span < 14 ? "forecast" : "future";
    string_t *url = string_sprintf("https://api.weatherapi.com/v1/%s.json?key=%s&q=%.6f,%.6f&dt=%s", endpoint,
                                   string_c_str(escaped), latitude, longitude, lab_cal_text(options, "date"));
    if (span >= 0 && span < 14)
        string_append_format(url, "&days=%d", span < 13 ? span + 2 : 14);
    http_client_t *client = http_client_new();
    http_limits_t limits = {.connect_timeout_ms = timeout_ms, .total_timeout_ms = timeout_ms,
                            .max_body_bytes = 2 * 1024 * 1024};
    http_client_set_limits(client, &limits);
    http_request_t *request = http_request_new(HTTP_GET, url);
    string_t *accept = string_new_with("Accept"), *json = string_new_with("application/json");
    string_t *agent = string_new_with("User-Agent"), *agent_value = string_new_with("MARS-Lab/1.0");
    http_request_set_header(request, accept, json);
    http_request_set_header(request, agent, agent_value);
    http_response_t *response = http_client_send(client, request);
    json_t *payload = http_response_ok(response) ? http_response_json(response) : NULL;
    http_response_free(response);
    http_request_free(request);
    http_client_free(client);
    string_free(accept);
    string_free(json);
    string_free(agent);
    string_free(agent_value);
    string_free(url);
    string_free(escaped);
    return payload;
}

/* Provider days are local; their absolute hourly epochs determine whether another date is needed. */
int lab_cal_weather_day_shift(const json_t *options, const json_t *weather)
{
    if (!*lab_cal_text(options, "observing_conditions")) return 0;
    double epoch = lab_cal_weather_epoch(options), first = INFINITY, last = -INFINITY;
    if (!isfinite(epoch)) return 0;
    const json_t *days = lab_cal_get(lab_cal_get(weather, "forecast"), "forecastday");
    for (size_t i = 0; i < json_array_size(days) && i < 14; ++i) {
        const json_t *day = json_array_get(days, i), *hours = lab_cal_get(day, "hour");
        bool selected_day = !strcmp(lab_cal_text(day, "date"), lab_cal_text(options, "date"));
        for (size_t j = 0; j < json_array_size(hours) && j < 25; ++j) {
            double hour;
            if (!lab_cal_provider_number(json_array_get(hours, j), "time_epoch", &hour)) continue;
            if (epoch >= hour && epoch < hour + 3600) return 0;
            if (selected_day) { first = fmin(first, hour); last = fmax(last, hour); }
        }
    }
    if (!isfinite(first) || !isfinite(last)) return 0;
    if (epoch < first && first - epoch <= 86400) return -1;
    if (epoch >= last + 3600 && epoch - (last + 3600) < 86400) return 1;
    return 0;
}

static json_t *lab_cal_weather_adjacent(const json_t *options, const string_t *key, int span, int shift,
                                       size_t timeout_ms)
{
    datetime_t *day = datetime_from_string(lab_cal_text(options, "date"));
    if (!day) return NULL;
    datetime_init_jdn(day, datetime_jdn(day) + shift);
    string_t *date = string_sprintf("%04d-%02d-%02d", datetime_year(day), datetime_month(day), datetime_day(day));
    datetime_dealloc(day);
    if (!date || strcmp(string_c_str(date), "2010-01-01") < 0 || span + shift > 300) {
        string_free(date);
        return NULL;
    }
    json_t *request = json_clone(options);
    lab_cal_set(request, "date", string_c_str(date));
    json_t *payload = lab_cal_weather_payload(request, key, span + shift, timeout_ms);
    json_free(request);
    string_free(date);
    return payload;
}

static json_t *lab_cal_fetch_weather(const json_t *options, const string_t *key, int span)
{
    struct timespec start = {0}, end = {0};
    bool timed = clock_gettime(CLOCK_MONOTONIC, &start) == 0;
    json_t *payload = lab_cal_weather_payload(options, key, span, 4000), *adjacent = NULL;
    int shift = lab_cal_weather_day_shift(options, payload);
    if (shift && timed && clock_gettime(CLOCK_MONOTONIC, &end) == 0) {
        double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
        if (elapsed >= 0 && elapsed < 3999)
            adjacent = lab_cal_weather_adjacent(options, key, span, shift, (size_t)(4000 - elapsed));
    }
    lab_cal_weather_remember(options, payload, time(NULL));
    lab_cal_weather_remember(options, adjacent, time(NULL));
    json_t *fields = json_new_object();
    const json_t *forecast = lab_cal_get(payload, "forecast");
    const json_t *days = lab_cal_get(forecast, "forecastday");
    /* At most fourteen forecast days: a bounded provider array, not a key lookup. */
    for (size_t i = 0; i < json_array_size(days) && i < 14; ++i) {
        const json_t *entry = json_array_get(days, i);
        if (strcmp(lab_cal_text(entry, "date"), lab_cal_text(options, "date")))
            continue;
        const json_t *day = lab_cal_get(entry, "day");
        double minimum, maximum, value;
        if (!lab_cal_provider_number(day, "mintemp_c", &minimum) ||
            !lab_cal_provider_number(day, "maxtemp_c", &maximum))
            break;
        lab_cal_measurement(fields, "weather_min_c", minimum, "°C");
        lab_cal_measurement(fields, "weather_max_c", maximum, "°C");
        if (lab_cal_provider_number(day, "avghumidity", &value))
            lab_cal_measurement(fields, "weather_humidity", value, "%");
        if (lab_cal_provider_number(day, "daily_chance_of_rain", &value))
            lab_cal_measurement(fields, "weather_rain_chance", value, "%");
        if (lab_cal_provider_number(day, "maxwind_kph", &value))
            lab_cal_measurement(fields, "weather_wind", value, " km/h");
        string_t *summary = string_sprintf("Min %s, max %s", lab_cal_text(fields, "weather_min_c"),
                                           lab_cal_text(fields, "weather_max_c"));
        lab_cal_set(fields, "weather_summary", string_c_str(summary));
        lab_cal_set(fields, "weather_source", "WeatherAPI.com");
        string_free(summary);
        break;
    }
    if (*lab_cal_text(options, "observing_conditions")) {
        json_t *conditions = json_clone(options);
        lab_cal_observing_conditions(conditions, adjacent ? adjacent : payload);
        lab_cal_take(fields, "observing_conditions", conditions);
    }
    json_free(payload);
    json_free(adjacent);
    return fields;
}

/* Return a successful envelope even when optional weather is disabled. */
json_t *lab_cal_weather(const json_t *options)
{
    const char *date = lab_cal_text(options, "date");
    datetime_t *selected = datetime_from_string(date);
    time_t now = time(NULL);
    struct tm local = {0};
    localtime_r(&now, &local);
    long today =
        datetime_ymd_to_jdn((short)(local.tm_year + 1900), (month_t)(local.tm_mon + 1), (uint8_t)local.tm_mday);
    long span = datetime_jdn(selected) - today;
    datetime_dealloc(selected);
    /* Both environment spellings take precedence over either file spelling. */
    const char *primary = getenv("MARS_WEATHER_API_KEY"), *legacy = getenv("WEATHERAPI_KEY");
    string_t *key = string_new_with(primary ? primary : "");
    string_trim(key);
    if (!string_byte_length(key) && legacy) {
        string_free(key);
        key = string_new_with(legacy);
        string_trim(key);
    }
    if (!string_byte_length(key)) {
        string_free(key);
        key = lab_cal_config("weather.env", "MARS_WEATHER_API_KEY");
    }
    if (!string_byte_length(key)) {
        string_free(key);
        key = lab_cal_config("weather.env", "WEATHERAPI_KEY");
    }
    const char *reason = NULL;
    if (strcmp(date, "2010-01-01") < 0 || span > 300)
        reason = "The selected date is outside the weather provider's supported range.";
    else if (!string_byte_length(key))
        reason = "Weather is disabled. Configure an API key from your own WeatherAPI account to enable it.";
    json_t *fields = reason ? json_new_object() : lab_cal_fetch_weather(options, key, (int)span);
    bool available = *lab_cal_text(fields, "weather_min_c") && *lab_cal_text(fields, "weather_max_c");
    if (!available && !reason)
        reason = "The weather provider did not return data.";
    string_free(key);
    json_t *response = json_new_object(), *sections = json_new_array(), *rows = json_new_array();
    lab_cal_take(response, "ok", json_new_bool(true));
    lab_cal_set(response, "mode", "datetime-weather");
    lab_cal_take(response, "available", json_new_bool(available));
    if (available) {
        static const char *keys[] = {"weather_min_c", "weather_max_c",       "weather_humidity",
                                     "weather_wind",  "weather_rain_chance", "weather_source"};
        static const char *labels[] = {"Minimum", "Maximum", "Humidity", "Wind", "Chance of rain", "Source"};
        for (size_t i = 0; i < sizeof(keys) / sizeof(*keys); ++i)
            if (*lab_cal_text(fields, keys[i]))
                lab_cal_row(rows, labels[i], lab_cal_text(fields, keys[i]));
    } else
        lab_cal_row(rows, "Status", reason);
    lab_cal_section(sections, "Weather", true, rows);
    json_free(rows);
    lab_cal_sections(response, "overview", sections);
    string_t *overview = string_new();
    if (available) {
        string_append_format(overview, "Temperature: %s", lab_cal_text(fields, "weather_summary"));
        if (*lab_cal_text(fields, "weather_humidity"))
            string_append_format(overview, "\nHumidity: %s", lab_cal_text(fields, "weather_humidity"));
        if (*lab_cal_text(fields, "weather_wind"))
            string_append_format(overview, "\nWind: %s", lab_cal_text(fields, "weather_wind"));
    }
    lab_cal_set(response, "overview", string_c_str(overview));
    string_free(overview);
    lab_cal_take(response, "fields", fields);
    return response;
}

/* Keep the rough atmosphere model native and independent of browser weather cards. */
void lab_cal_atmosphere(json_t *options)
{
    lab_cal_set(options, "observing_conditions", "yes");
    json_t *weather = lab_cal_weather(options);
    const json_t *conditions = lab_cal_get(lab_cal_get(weather, "fields"), "observing_conditions");
    if (conditions) {
        static const char *keys[] = {"temperature_c", "pressure_hpa", "pressure_msl_hpa",
                                     "atmosphere_note", "weather_source"};
        for (size_t i = 0; i < sizeof(keys) / sizeof(*keys); ++i)
            lab_cal_set(options, keys[i], lab_cal_text(conditions, keys[i]));
    } else
        lab_cal_observing_conditions(options, NULL);
    if (!strncmp(lab_cal_text(options, "weather_source"), "WeatherAPI", 10))
        lab_cal_weather_conditions_remember(options, time(NULL));
    else
        (void)lab_cal_weather_conditions_restore(options, time(NULL));
    json_free(weather);
}

/* Interpret the worksheet moment in UTC independently of the provider's local calendar date. */
double lab_cal_weather_epoch(const json_t *options)
{
    datetime_t *moment = datetime_from_string(lab_cal_text(options, "date"));
    string_t *clock = string_new_with(lab_cal_text(options, "time"));
    size_t count = 0;
    string_t **parts = lab_cal_split(clock, ":", &count);
    double hour = 0, minute = 0, second = 0;
    bool valid = (count == 2 || count == 3) &&
                 lab_cal_number(string_c_str(parts[0]), &hour) &&
                 lab_cal_number(string_c_str(parts[1]), &minute) &&
                 (count == 2 || lab_cal_number(string_c_str(parts[2]), &second));
    double epoch = moment && valid ? (datetime_jdn(moment) - 2440588.0) * 86400 + hour * 3600 + minute * 60 + second : NAN;
    datetime_dealloc(moment);
    string_split_free(parts, count);
    string_free(clock);
    return epoch;
}

/* Match absolute timestamps, including current observations when their UTC hour matches. */
static const json_t *lab_cal_observation_hour(const json_t *options, const json_t *weather, bool allow_current)
{
    double epoch = lab_cal_weather_epoch(options), updated = 0;
    const json_t *current = lab_cal_get(weather, "current");
    if (allow_current && isfinite(epoch) && lab_cal_provider_number(current, "last_updated_epoch", &updated) &&
        floor(epoch / 3600) == floor(updated / 3600) && fabs(epoch - updated) <= 1800) return current;
    const json_t *days = lab_cal_get(lab_cal_get(weather, "forecast"), "forecastday");
    /* Bounded provider arrays: fourteen days and up to twenty-five hours on a DST transition day. */
    for (size_t i = 0; i < json_array_size(days) && i < 14; ++i) {
        const json_t *hours = lab_cal_get(json_array_get(days, i), "hour");
        for (size_t j = 0; j < json_array_size(hours) && j < 25; ++j) {
            const json_t *hour = json_array_get(hours, j);
            double time;
            if (lab_cal_provider_number(hour, "time_epoch", &time) && epoch >= time - 0.001 && epoch < time + 3600)
                return hour;
        }
    }
    return NULL;
}

/* Provider arrays are bounded; absolute epochs handle midnight and local clock changes. */
static bool lab_cal_hourly_value(const json_t *options, const json_t *weather, const char *key,
                                 double minimum, double maximum, double *result)
{
    double epoch = lab_cal_weather_epoch(options), before = -INFINITY, after = INFINITY, low = 0, high = 0;
    if (!isfinite(epoch)) return false;
    const json_t *days = lab_cal_get(lab_cal_get(weather, "forecast"), "forecastday");
    for (size_t i = 0; i < json_array_size(days) && i < 14; ++i) {
        const json_t *hours = lab_cal_get(json_array_get(days, i), "hour");
        for (size_t j = 0; j < json_array_size(hours) && j < 25; ++j) {
            const json_t *hour = json_array_get(hours, j);
            double time, value;
            if (!lab_cal_provider_number(hour, "time_epoch", &time) ||
                !lab_cal_provider_number(hour, key, &value) || value < minimum || value > maximum) continue;
            if (time <= epoch && time > before) { before = time; low = value; }
            if (time >= epoch && time < after) { after = time; high = value; }
        }
    }
    if (!isfinite(before) || !isfinite(after) || after - before > 3600) return false;
    *result = after == before ? low : low + (high - low) * (epoch - before) / (after - before);
    return true;
}

/* Convert hourly mean sea-level pressure to an observer-pressure estimate for refraction. */
void lab_cal_observing_conditions(json_t *options, const json_t *weather)
{
    double latitude = 0, elevation = 0, temperature, pressure;
    (void)lab_cal_number(lab_cal_text(options, "lat"), &latitude);
    (void)lab_cal_number(lab_cal_text(options, "elevation"), &elevation);
    const json_t *hour = lab_cal_observation_hour(options, weather, true);
    const json_t *current = lab_cal_get(weather, "current");
    double observed = 0, epoch = lab_cal_weather_epoch(options);
    bool current_temperature = hour && hour == current &&
        lab_cal_provider_number(current, "last_updated_epoch", &observed) && fabs(epoch - observed) <= 1800 &&
        lab_cal_provider_number(current, "temp_c", &temperature) && temperature >= -100 && temperature <= 60;
    bool interpolated = !current_temperature &&
                        lab_cal_hourly_value(options, weather, "temp_c", -100, 60, &temperature);
    bool measured = current_temperature || interpolated ||
        (hour != current && lab_cal_provider_number(hour, "temp_c", &temperature) && temperature >= -100 && temperature <= 60);
    /* Current station pressure has no station elevation; it cannot be transferred to the observer reliably. */
    const json_t *pressure_hour = lab_cal_observation_hour(options, weather, false);
    bool pressure_available = lab_cal_hourly_value(options, weather, "pressure_mb", 1, 1200, &pressure) ||
                              (lab_cal_provider_number(pressure_hour, "pressure_mb", &pressure) &&
                               pressure >= 1 && pressure <= 1200);
    json_t *recent = measured ? NULL : lab_cal_weather_recent(options, time(NULL));
    bool cached = recent && lab_cal_provider_number(recent, "temp_c", &temperature);
    const char *source = current_temperature ? "WeatherAPI" : interpolated ? "WeatherAPI hourly interpolation" :
                         measured ? "WeatherAPI hourly value" : cached ? "Recent weather estimate" :
                         "Rough seasonal assumption";
    if (!measured && !cached) {
        datetime_t *date = datetime_from_string(lab_cal_text(options, "date"));
        long january = datetime_ymd_to_jdn(datetime_year(date), DT_January, 1);
        double day = (double)(datetime_jdn(date) - january);
        double latitude_radians = latitude * M_PI / 180;
        /* Deliberately coarse, not a climatological database: northern warmth peaks in July. */
        temperature = 27 - 45 * pow(sin(latitude_radians), 2) +
                      12 * sin(latitude_radians) * cos(2 * M_PI * (day - 200) / 365.25) - 0.0065 * elevation;
        temperature = round(fmax(-100, fmin(60, temperature)));
        datetime_dealloc(date);
    }
    double sea_level_pressure = pressure_available ? pressure : NAN;
    string_t *sea_level_text = pressure_available ? string_sprintf("%.6f", pressure) : string_new();
    lab_cal_set(options, "pressure_msl_hpa", string_c_str(sea_level_text));
    string_free(sea_level_text);
    /* Tropospheric barometric formula: L = 0.0065 K/m, T0 = 288.15 K and exponent g*M/(R*L). */
    const double lapse = 0.0065, sea_level_kelvin = 288.15;
    const double exponent = 9.80665 * 0.0289644 / (8.31432 * lapse);
    pressure = (pressure_available ? pressure : 1013.25) *
               pow(1 - lapse * elevation / sea_level_kelvin, exponent);
    string_t *temperature_text = string_sprintf("%.6f", temperature);
    string_t *pressure_text = string_sprintf("%.6f", pressure);
    string_t *source_text = string_new_with(source);
    if (cached && lab_cal_provider_number(recent, "observed", &observed))
        string_append_format(source_text, " (%.0f min old; %.0f min from selected time)",
                             difftime(time(NULL), (time_t)observed) / 60, fabs(epoch - observed) / 60);
    string_append_cstr(source_text, pressure_available ? "\nestimated pressure at observer altitude" :
                                                       "\nestimated pressure (standard atmosphere)");
    string_t *note = string_sprintf("Temperature: %.1f °C · Pressure: %.0f hPa (observer)",
                                    temperature, pressure);
    if (pressure_available) string_append_format(note, "\n%.0f hPa (sea level)", sea_level_pressure);
    string_append_format(note, " · Source: %s", string_c_str(source_text));
    lab_cal_set(options, "temperature_c", string_c_str(temperature_text));
    lab_cal_set(options, "pressure_hpa", string_c_str(pressure_text));
    lab_cal_set(options, "atmosphere_note", string_c_str(note));
    lab_cal_set(options, "weather_source", string_c_str(source_text));
    string_free(source_text);
    json_free(recent);
    string_free(note);
    string_free(temperature_text);
    string_free(pressure_text);
}
