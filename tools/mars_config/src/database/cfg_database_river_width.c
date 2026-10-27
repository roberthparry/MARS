/**
 * @file cfg_database_river_width.c
 * @brief Recorded-width parsing and open-water tag selection for native river preparation.
 *
 * Converts explicit OSM width tags to metres without guessing ranges, malformed values or estimated widths.
 * The parser is locale independent and is private to the native configuration tools.
 */
#include <ctype.h>
#include <math.h>
#include <string.h>

#include "json-t.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

static const json_t *water_tag(const json_t *properties, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = key ? json_object_get(properties, key) : NULL;
    string_free(key);
    return value;
}

static bool tag_is(const json_t *properties, const char *name, const char *expected)
{
    const string_t *value = json_string_value(water_tag(properties, name));
    return value && !strcmp(string_c_str(value), expected);
}

/* A mapped lake or lagoon supplies water coverage just as a mapped riverbank does. */
bool cfg_database_river_open_water(const json_t *properties)
{
    return tag_is(properties, "natural", "water") || tag_is(properties, "waterway", "riverbank") ||
           tag_is(properties, "water", "river") || tag_is(properties, "water", "stream") ||
           tag_is(properties, "water", "canal");
}

static const char *osm_width_space(const char *text)
{
    while (isspace((unsigned char)*text)) ++text;
    return text;
}

/* OSM widths use decimal points, regardless of the installer's current locale. */
static bool osm_width_decimal(const char **text, double *value)
{
    const char *p = *text;
    double number = 0, place = 0.1;
    bool digits = false;
    while (*p >= '0' && *p <= '9') {
        number = number * 10 + (*p++ - '0');
        digits = true;
    }
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9') {
            number += (*p++ - '0') * place;
            place *= 0.1;
            digits = true;
        }
    }
    *text = p;
    *value = number;
    return digits && isfinite(number);
}

/* Unrecognised, range-valued or conflicting tags remain unknown; they never become a zero-width section. */
bool cfg_database_river_width_metres(const json_t *properties, double *width)
{
    const json_t *tag = water_tag(properties, "width");
    /* An estimate is not sufficient evidence for permanently excluding source geometry. */
    const string_t *value = tag ? json_string_value(tag) : NULL;
    if (!value && tag) value = json_number_text(tag);
    if (!value || !string_byte_length(value) || string_byte_length(value) > 128) return false;
    const char *text = string_c_str(value);
    if (strlen(text) != string_byte_length(value)) return false;
    const char *end = osm_width_space(text);
    double number;
    if (!osm_width_decimal(&end, &number)) return false;
    if (*end == '\'') {
        if (floor(number) != number) return false;
        ++end;
        double inches;
        if (!osm_width_decimal(&end, &inches) || inches >= 12 || *end++ != '"' || *osm_width_space(end))
            return false;
        number = number * 0.3048 + inches * 0.0254;
    } else {
        const char *unit = osm_width_space(end);
        if (*unit) {
            if (unit == end) return false;
            /* This fixed eight-entry unit vocabulary has bounded scan cost. */
            static const struct { const char *name; double metres; } units[] = {
                {"m", 1}, {"cm", 0.01}, {"mm", 0.001}, {"km", 1000},
                {"ft", 0.3048}, {"in", 0.0254}, {"yd", 0.9144}, {"mi", 1609.344}
            };
            bool known = false;
            for (size_t i = 0; i < sizeof units / sizeof *units; ++i) {
                size_t n = strlen(units[i].name);
                if (!strncmp(unit, units[i].name, n) && !*osm_width_space(unit + n)) {
                    number *= units[i].metres;
                    known = true;
                    break;
                }
            }
            if (!known) return false;
        }
    }
    if (!isfinite(number) || number <= 0) return false;
    *width = number;
    return true;
}
