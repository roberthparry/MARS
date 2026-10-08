/**
 * @file cfg_main.c
 * @brief Command-line dispatch for native MARS configuration and locale generation.
 *
 * Selects an installer or the pinned calendar-locale SQL generator without
 * starting either Lab. Options are parsed using string_t; credentials are never
 * printed in diagnostics. This executable links MARS directly and has no Python
 * or SQLCipher command-line dependency.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "cfg_database.h"
#include "cfg_locales.h"
#include "cfg_weather.h"
#include "ustring.h"

static bool cfg_app_is(const char *text, const char *literal)
{
    string_t *value = string_new_with(text);
    bool equal = value && string_view_equals_literal(string_view_all(value), literal);
    string_free(value);
    return equal;
}

/* Dispatch one explicit task; help and malformed options never touch configuration. */
int main(int argc, char **argv)
{
    if (argc == 2 && cfg_app_is(argv[1], "--help")) {
        puts("Usage: mars_config weather [--weather-key KEY] [--noninteractive]\n"
             "       mars_config almanac [--db-path PATH] [--db-key KEY] [--noninteractive]\n"
             "       mars_config jurisdiction [TOWN] [--language LANGUAGE] [--db-path PATH]\n"
             "                                [--db-key KEY] [--noninteractive]\n"
             "       mars_config locales [--check | --output PATH] [--data-dir DIR]");
        return 0;
    }
    if (argc < 2) {
        fputs("Choose weather, almanac, jurisdiction or locales; use --help for usage.\n", stderr);
        return 2;
    }
    if (cfg_app_is(argv[1], "locales"))
        return cfg_locales_run(argc - 2, argv + 2);
    bool weather = cfg_app_is(argv[1], "weather"), jurisdiction = cfg_app_is(argv[1], "jurisdiction");
    if (!weather && !jurisdiction && !cfg_app_is(argv[1], "almanac")) {
        fputs("Unknown configuration command\n", stderr);
        return 2;
    }
    const char *path = NULL, *key = NULL, *location = NULL, *language = NULL;
    bool interactive = isatty(STDIN_FILENO);
    for (int i = 2; i < argc; ++i) {
        if (cfg_app_is(argv[i], "--noninteractive")) {
            interactive = false;
        } else if (i + 1 < argc && cfg_app_is(argv[i], weather ? "--weather-key" : "--db-key")) {
            key = argv[++i];
        } else if (!weather && i + 1 < argc && cfg_app_is(argv[i], "--db-path")) {
            path = argv[++i];
        } else if (jurisdiction && i + 1 < argc && cfg_app_is(argv[i], "--language")) {
            language = argv[++i];
        } else if (jurisdiction && !location && argv[i][0] != '-') {
            location = argv[i];
        } else {
            fputs("Invalid configuration option; use --help for usage.\n", stderr);
            return 2;
        }
    }
    if (weather)
        return cfg_weather_run(key, interactive);
    if (!location)
        location = getenv("MARS_CALENDAR_LOCATION_ARGUMENT");
    if (!language)
        language = getenv("MARS_CALENDAR_LANGUAGE_ARGUMENT");
    return cfg_database_run(jurisdiction, path, key, location, language, interactive);
}
