/**
 * @file cfg_weather.c
 * @brief Native weather-account opt-in, private key persistence and terminal prompts.
 *
 * Configures the weather account without contacting the provider. Terminal interaction
 * uses stdio and termios. A JSON update is committed by the encrypted settings owner;
 * failed writes retain the previous configuration and never create a plaintext sidecar.
 */
#include <errno.h>
#include <signal.h>
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "json-t.h"
#include "string-t.h"
#include "mars_settings.h"
#include "cfg_prompt.h"
#include "cfg_storage.h"
#include "cfg_weather.h"

static string_t *cfg_weather_text(const char *text)
{
    return cfg_storage_value(text);
}

static bool cfg_weather_valid(const string_t *key)
{
    size_t length = string_byte_length(key);
    if (!length || length > 4096)
        return false;
    string_view_t view = string_view_all(key);
    for (size_t i = 0; i < length; ++i) {
        unsigned char ch;
        if (!string_view_peek_ascii(view, i, &ch) || ch < 32 || ch > 126)
            return false;
    }
    return true;
}

static bool cfg_weather_yes(const char *prompt)
{
    string_t *answer = cfg_prompt_read(prompt, false);
    string_view_t view = string_view_all(answer);
    bool yes = answer && (string_view_equals_literal(view, "y") || string_view_equals_literal(view, "yes") ||
                          string_view_equals_literal(view, "Y") || string_view_equals_literal(view, "YES"));
    string_free(answer);
    return yes;
}

/* Configure only weather credentials; never initialise a cache, browser or listener. */
int cfg_weather_run(const char *explicit_key, bool interactive)
{
    string_t *key = cfg_weather_text(explicit_key);
    if (key && !string_byte_length(key)) {
        string_free(key);
        const char *override = getenv("MARS_WEATHER_API_KEY");
        key = override ? cfg_weather_text(override) : mars_settings_read("MARS_WEATHER_API_KEY");
    }
    bool ok = key != NULL;
    if (ok && interactive) {
        puts("Optional weather setup: use your own WeatherAPI account; no shared key is supplied.\n"
             "Weather stays disabled without a key. Sign up at https://www.weatherapi.com/signup.aspx\n"
             "The local server sends the key, date, latitude and longitude over HTTPS to WeatherAPI.com.\n"
             "The key is not sent to the browser. Weather responses are not cached or persisted.\n"
             "Date and location remain in private Lab state, not weather-specific logs.\n"
             "Weather is probabilistic: do not rely on it alone for safety-critical decisions.\n"
             "Terms: https://www.weatherapi.com/terms.aspx\nPrivacy: https://www.weatherapi.com/privacy.aspx");
        if (!cfg_weather_yes("Configure weather now? [y/N]: ")) {
            string_free(key);
            key = string_new();
        } else if (!string_byte_length(key) || cfg_weather_yes("A key is configured. Replace it? [y/N]: ")) {
            string_free(key);
            key = NULL;
            for (;;) {
                key = cfg_prompt_read("WeatherAPI key (blank skips setup): ", true);
                if (!key || !string_byte_length(key))
                    break;
                string_t *repeat = cfg_prompt_read("Confirm WeatherAPI key: ", true);
                bool cancelled = repeat == NULL;
                bool matches = repeat && !string_compare(key, repeat);
                string_free(repeat);
                if (matches)
                    break;
                string_free(key);
                key = NULL;
                if (cancelled)
                    break;
                puts("Keys did not match. Please try again.");
            }
        }
        ok = key != NULL;
    }
    if (ok && string_byte_length(key)) {
        json_t *settings = json_new_object();
        string_t *name = string_new_with("MARS_WEATHER_API_KEY");
        json_t *value = json_new_string(key);
        ok = cfg_weather_valid(key) && settings && name && value && json_object_set(settings, name, value);
        string_t *body = ok ? json_to_string(settings) : NULL;
        ok = body && mars_settings_sync(body);
        if (body) sodium_memzero((void *)string_c_str(body), string_byte_length(body));
        string_free(body); json_free(settings); json_free(value); string_free(name);
        if (ok)
            puts("Weather API configuration saved.");
    }
    if (!ok)
        fputs("Cannot save weather configuration; check the key and private configuration path.\n", stderr);
    string_free(key);
    return ok ? 0 : 1;
}
