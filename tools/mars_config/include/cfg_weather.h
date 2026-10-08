/**
 * @file cfg_weather.h
 * @brief Native, optional weather-account configuration for MARS Lab installation.
 *
 * Runs before listener or cache initialisation. Uses the user's own WeatherAPI
 * account, never a shared key. Configuration is published atomically in a private
 * directory using file.h, with string_t parsing and no shell interpretation.
 * Intended for the single-threaded installer CLI; changes umask temporarily.
 */
#ifndef MARS_CONFIG_WEATHER_H
#define MARS_CONFIG_WEATHER_H

#include <stdbool.h>

/**
 * @brief Configure optional weather access without starting the Lab.
 * @param[in] key Optional borrowed explicit key; NULL/empty uses environment then stored configuration.
 * @param[in] interactive Offer opt-in and hidden, confirmed key entry on a terminal; false never prompts.
 * @return Zero for success or skipped setup, one for failure. Diagnostics never include key contents.
 * @details Primary MARS_WEATHER_API_KEY precedes WEATHERAPI_KEY. Keys must contain
 * at most 4096 printable ASCII characters, excluding apostrophes; outer whitespace
 * is trimmed. No files are created when no key is supplied. Existing configuration
 * is retained on declined setup or invalid input. MARS_HOME overrides ~/.mars.
 */
int cfg_weather_run(const char *key, bool interactive);

#endif
