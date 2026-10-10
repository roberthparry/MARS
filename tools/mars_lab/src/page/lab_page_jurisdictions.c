/**
 * @file lab_page_jurisdictions.c
 * @brief Database-backed browser jurisdiction choices and observer catalogues.
 *
 * Converts jurisdiction visitors to the browser's JSON representation without
 * inspecting database tables or SQL seed files. Grouped town enumeration appends
 * to one array at a time, avoiding repeated copying of an ever-growing catalogue.
 * The forms API supplies selection keys and coordinate display text with each
 * town, avoiding a browser round trip to prepare that same catalogue again.
 * Missing or unreadable databases yield explicit unavailable metadata and empty
 * choices; mathematical worksheets do not depend on the jurisdiction database.
 */
#include <string.h>

#include "jurisdiction.h"
#include "lab_forms.h"
#include "lab_page.h"

struct lab_page_places {
    json_t *options;
    json_t *locations;
    json_t *towns;
    json_t *group;
    string_t *group_code;
    bool ok;
};

static bool lab_page_catalogue_set(json_t *object, const char *name, const json_t *value)
{
    string_t *key = string_new_with(name);
    bool ok = key && value && json_object_set(object, key, value);
    string_free(key);
    return ok;
}

static bool lab_page_catalogue_text(json_t *object, const char *name, const char *text)
{
    string_t *s = string_new_with(text ? text : "");
    json_t *value = s ? json_new_string(s) : NULL;
    bool ok = lab_page_catalogue_set(object, name, value);
    json_free(value);
    string_free(s);
    return ok;
}

static bool lab_page_catalogue_append(json_t *array, const char *text)
{
    string_t *s = string_new_with(text ? text : "");
    json_t *value = s ? json_new_string(s) : NULL;
    bool ok = value && json_array_append(array, value);
    json_free(value);
    string_free(s);
    return ok;
}

static bool lab_page_choice(const char *code, const char *label, void *context)
{
    struct lab_page_places *places = context;
    json_t *row = json_new_array();
    places->ok = row && lab_page_catalogue_append(row, code) && lab_page_catalogue_append(row, label) &&
                 json_array_append(places->options, row);
    json_free(row);
    return places->ok;
}

static bool lab_page_location(const jurisdict_place_t *place, void *context)
{
    struct lab_page_places *places = context;
    json_t *row = json_new_array();
    places->ok = row && lab_page_catalogue_append(row, place->latitude) &&
                 lab_page_catalogue_append(row, place->longitude) && lab_page_catalogue_append(row, place->timezone) &&
                 lab_page_catalogue_append(row, place->name) &&
                 lab_page_catalogue_set(places->locations, place->jurisdiction_code, row);
    json_free(row);
    return places->ok;
}

static bool lab_page_town_flush(struct lab_page_places *places)
{
    return !places->group || (places->group_code && json_object_set(places->towns, places->group_code, places->group));
}

static bool lab_page_town(const jurisdict_place_t *place, void *context)
{
    struct lab_page_places *places = context;
    if (!places->group_code || strcmp(string_c_str(places->group_code), place->jurisdiction_code)) {
        places->ok = lab_page_town_flush(places);
        json_free(places->group);
        string_free(places->group_code);
        places->group = json_new_array();
        places->group_code = string_new_with(place->jurisdiction_code);
    }
    json_t *row = json_new_object(), *is_default = json_new_bool(place->is_default);
    places->ok = places->ok && places->group && places->group_code && row && is_default &&
                 lab_page_catalogue_text(row, "name", place->name) &&
                 lab_page_catalogue_text(row, "latitude", place->latitude) &&
                 lab_page_catalogue_text(row, "longitude", place->longitude) &&
                 lab_page_catalogue_text(row, "elevation", place->elevation) &&
                 lab_page_catalogue_text(row, "timezone", place->timezone) &&
                 lab_page_catalogue_set(row, "default", is_default) && lab_forms_town_presentation(row) &&
                 json_array_append(places->group, row);
    json_free(is_default);
    json_free(row);
    return places->ok;
}

/* Obtain the live database catalogue without preventing ordinary maths when it is unavailable. */
json_t *lab_page_jurisdictions(void)
{
    jurisdiction_t *engine = jurisdict_open(NULL);
    struct lab_page_places places = {
        .options = json_new_array(), .locations = json_new_object(), .towns = json_new_object(), .ok = true};
    bool available = engine && places.options && places.locations && places.towns &&
                     jurisdict_each_choice(engine, lab_page_choice, &places) && places.ok &&
                     jurisdict_each_location(engine, lab_page_location, &places) && places.ok &&
                     jurisdict_each_town(engine, lab_page_town, &places) && places.ok && lab_page_town_flush(&places);
    if (!available) {
        json_free(places.options);
        json_free(places.locations);
        json_free(places.towns);
        places.options = json_new_array();
        places.locations = json_new_object();
        places.towns = json_new_object();
    }
    json_t *result = json_new_object(), *flag = json_new_bool(available);
    bool ok = result && flag && lab_page_catalogue_set(result, "available", flag) &&
              lab_page_catalogue_set(result, "options", places.options) &&
              lab_page_catalogue_set(result, "locations", places.locations) &&
              lab_page_catalogue_set(result, "towns", places.towns);
    if (ok && !available)
        ok = lab_page_catalogue_text(
            result, "error",
            "Jurisdiction database unavailable. Install or configure jurisdiction-db to select countries and towns.");
    if (!ok) {
        json_free(result);
        result = NULL;
    }
    json_free(flag);
    json_free(places.options);
    json_free(places.locations);
    json_free(places.towns);
    json_free(places.group);
    string_free(places.group_code);
    jurisdict_close(engine);
    return result;
}
