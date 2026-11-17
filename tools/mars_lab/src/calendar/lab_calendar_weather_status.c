/**
 * @file lab_calendar_weather_status.c
 * @brief Credential-free diagnostics for optional native Lab weather requests.
 *
 * Converts transport categories, HTTP status and WeatherAPI error codes into actionable messages shared by
 * DateTime and Almanac. Provider messages and request URLs are never copied into results. Only bounded,
 * recognised fragments of local transport diagnostics classify DNS and TLS failures. This module performs
 * no requests and does not read configuration, allowing deterministic tests without a weather account.
 */
#include <stddef.h>

#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_internal.h"

static const char *lab_cal_weather_provider_error(unsigned code)
{
    static const struct {
        unsigned code;
        const char *message;
    } errors[] = {
        {1002, "WeatherAPI did not receive an API key"},
        {1003, "WeatherAPI did not receive a location"},
        {1005, "WeatherAPI rejected the request URL"},
        {1006, "WeatherAPI could not identify the selected location"},
        {2006, "WeatherAPI rejected the configured API key; re-enter it in weather setup"},
        {2007, "WeatherAPI's monthly request quota has been exceeded"},
        {2008, "The configured WeatherAPI key has been disabled"},
        {2009, "The WeatherAPI account does not have access to this weather service"},
        {2010, "WeatherAPI rejected this computer because of the key's website, app or IP restrictions"},
        {2011, "The WeatherAPI key's monthly request cap has been reached"},
        {2012, "WeatherAPI is receiving requests too quickly; try again shortly"},
        {9999, "WeatherAPI reported an internal service error"},
    };
    size_t lower = 0, upper = sizeof errors / sizeof *errors;
    while (lower < upper) {
        size_t middle = lower + (upper - lower) / 2;
        if (errors[middle].code < code)
            lower = middle + 1;
        else
            upper = middle;
    }
    return lower < sizeof errors / sizeof *errors && errors[lower].code == code ? errors[lower].message : NULL;
}

static const char *lab_cal_weather_transport_error(http_error_t error, const string_t *detail)
{
    static const char *const messages[] = {
        [HTTP_ERROR_NONE]      = "The weather request did not complete",
        [HTTP_ERROR_ARGUMENT]  = "MARS could not prepare the weather request",
        [HTTP_ERROR_MEMORY]    = "MARS could not allocate memory for the weather request",
        [HTTP_ERROR_TRANSPORT] = "MARS could not connect to WeatherAPI; check the network connection",
        [HTTP_ERROR_TIMEOUT]   = "The WeatherAPI request timed out; try again",
        [HTTP_ERROR_LIMIT]     = "The WeatherAPI response exceeded MARS's supported size limit",
        [HTTP_ERROR_CANCELLED] = "The weather request was cancelled",
        [HTTP_ERROR_IO]        = "MARS could not read the weather response",
        [HTTP_ERROR_PROTOCOL]  = "The WeatherAPI connection used an unsupported protocol",
    };
    /* Classify bounded local libcurl text, but never publish it: it may contain request details. */
    if (error == HTTP_ERROR_TRANSPORT && detail && string_byte_length(detail) <= 4096) {
        if (string_find(detail, "certificate") >= 0 || string_find(detail, "SSL") >= 0 ||
            string_find(detail, "TLS") >= 0)
            return "MARS could not verify the HTTPS connection to WeatherAPI; check the system clock and CA certificates";
        if (string_find(detail, "resolve") >= 0)
            return "MARS could not resolve api.weatherapi.com; check DNS and the network connection";
    }
    return (unsigned)error < sizeof messages / sizeof *messages ? messages[error] : messages[HTTP_ERROR_NONE];
}

/* Explain a weather request without forwarding credentials, remote messages or URLs. */
string_t *lab_cal_weather_failure(bool client_ready, http_error_t error, const string_t *detail, long status,
                                 const json_t *payload)
{
    if (!client_ready)
        return string_new_with("MARS could not initialise its HTTPS client; check the installed libcurl runtime.");
    if (error != HTTP_ERROR_NONE || status <= 0)
        return string_sprintf("%s.", lab_cal_weather_transport_error(error, detail));
    unsigned code = 0;
    const json_t *provider_error = lab_cal_get(payload, "error");
    if (lab_cal_integer(lab_cal_text(provider_error, "code"), 9999, &code) && code) {
        const char *message = lab_cal_weather_provider_error(code);
        return message ? string_sprintf("%s (error %u).", message, code) :
                         string_sprintf("WeatherAPI reported error %u (HTTP %ld).", code, status);
    }
    if (status < 200 || status >= 300)
        return string_sprintf("WeatherAPI returned HTTP %ld; weather is unavailable.", status);
    if (json_type(payload) != JSON_OBJECT || json_type(provider_error) == JSON_OBJECT)
        return string_new_with("WeatherAPI returned a response MARS could not read.");
    return NULL;
}
