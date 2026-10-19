/**
 * @file lab_calendar_weather.c
 * @brief Optional WeatherAPI daily cards and hourly observing conditions for native MARS Lab.
 *
 * Reads the user's existing weather.env configuration through file_t, performs
 * a verified HTTPS request with a four-second deadline and emits the browser's
 * weather fields and sections. Provider errors remain an unavailable weather
 * card and never expose the API key in diagnostics. Almanac conditions use UTC
 * hour matching and explicitly labelled seasonal/pressure fallbacks.
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

static json_t *lab_cal_fetch_weather(const json_t *options, const string_t *key, int span)
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
        string_append_format(url, "&days=%d", span + 1);
    http_client_t *client = http_client_new();
    http_limits_t limits = {.connect_timeout_ms = 4000, .total_timeout_ms = 4000, .max_body_bytes = 2 * 1024 * 1024};
    http_client_set_limits(client, &limits);
    http_request_t *request = http_request_new(HTTP_GET, url);
    string_t *accept = string_new_with("Accept"), *json = string_new_with("application/json");
    string_t *agent = string_new_with("User-Agent"), *agent_value = string_new_with("MARS-Lab/1.0");
    http_request_set_header(request, accept, json);
    http_request_set_header(request, agent, agent_value);
    http_response_t *response = http_client_send(client, request);
    json_t *payload = http_response_ok(response) ? http_response_json(response) : NULL;
    lab_cal_weather_remember(options, payload, time(NULL));
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
        lab_cal_observing_conditions(conditions, payload);
        lab_cal_take(fields, "observing_conditions", conditions);
    }
    json_free(payload);
    http_response_free(response);
    http_request_free(request);
    http_client_free(client);
    string_free(accept);
    string_free(json);
    string_free(agent);
    string_free(agent_value);
    string_free(url);
    string_free(escaped);
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
        static const char *keys[] = {"temperature_c", "pressure_hpa", "atmosphere_note", "weather_source"};
        for (size_t i = 0; i < sizeof(keys) / sizeof(*keys); ++i)
            lab_cal_set(options, keys[i], lab_cal_text(conditions, keys[i]));
    } else
        lab_cal_observing_conditions(options, NULL);
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
static const json_t *lab_cal_observation_hour(const json_t *options, const json_t *weather)
{
    double epoch = lab_cal_weather_epoch(options), updated = 0;
    const json_t *current = lab_cal_get(weather, "current");
    if (isfinite(epoch) && lab_cal_provider_number(current, "last_updated_epoch", &updated) &&
        floor(epoch / 3600) == floor(updated / 3600)) return current;
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

/* Use provider station pressure directly; apply elevation only to the missing-pressure fallback. */
void lab_cal_observing_conditions(json_t *options, const json_t *weather)
{
    double latitude = 0, elevation = 0, temperature, pressure;
    (void)lab_cal_number(lab_cal_text(options, "lat"), &latitude);
    (void)lab_cal_number(lab_cal_text(options, "elevation"), &elevation);
    const json_t *hour = lab_cal_observation_hour(options, weather);
    bool measured = lab_cal_provider_number(hour, "temp_c", &temperature) && temperature >= -100 && temperature <= 60;
    bool pressure_available = lab_cal_provider_number(hour, "pressure_mb", &pressure) &&
                              pressure > 0 && pressure <= 1200;
    json_t *recent = measured ? NULL : lab_cal_weather_recent(options, time(NULL));
    bool cached = recent && lab_cal_provider_number(recent, "temp_c", &temperature);
    if (!measured && !cached) {
        datetime_t *date = datetime_from_string(lab_cal_text(options, "date"));
        long january = datetime_ymd_to_jdn(datetime_year(date), DT_January, 1);
        double day = (double)(datetime_jdn(date) - january);
        double latitude_radians = latitude * M_PI / 180;
        /* Deliberately coarse, not a climatological database: northern warmth peaks in July. */
        temperature = 27 - 45 * pow(sin(latitude_radians), 2) +
                      12 * sin(latitude_radians) * cos(2 * M_PI * (day - 200) / 365.25) - 0.0065 * elevation;
        temperature = fmax(-100, fmin(60, temperature));
        datetime_dealloc(date);
    }
    if (!pressure_available)
        pressure = 1013.25 * pow(1 - 2.25577e-5 * elevation, 5.25588);
    string_t *temperature_text = string_sprintf("%.6f", temperature);
    string_t *pressure_text = string_sprintf("%.6f", pressure);
    string_t *note = measured && pressure_available
                         ? string_sprintf("Temperature: %.1f °C · Pressure: %.0f hPa · Source: WeatherAPI",
                                          temperature, pressure)
                         : string_sprintf("Temperature: %.1f °C (%s) · Pressure: %.0f hPa (%s)",
                                          temperature, measured ? "WeatherAPI" : cached ? "recent weather estimate" : "seasonal estimate", pressure,
                                          pressure_available ? "WeatherAPI" : "elevation estimate");
    lab_cal_set(options, "temperature_c", string_c_str(temperature_text));
    lab_cal_set(options, "pressure_hpa", string_c_str(pressure_text));
    lab_cal_set(options, "atmosphere_note", string_c_str(note));
    lab_cal_set(options, "weather_source",
                measured && pressure_available ? "WeatherAPI" :
                measured ? "WeatherAPI temperature; estimated pressure" :
                cached ? "Recent weather estimate" :
                pressure_available ? "Estimated temperature; WeatherAPI pressure" : "Estimated");
    json_free(recent);
    string_free(note);
    string_free(temperature_text);
    string_free(pressure_text);
}
