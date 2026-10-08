/**
 * @file cfg_calendar.c
 * @brief Calendar installer entry point and ownership-safe selection orchestration.
 *
 * A borrowed encrypted staging connection supplies the catalogue and locale data.
 * Selection errors and cancellation leave its existing snapshot untouched. The
 * successful caller receives canonical owned town and locale strings.
 */
#include <stdio.h>

#include "cfg_calendar_internal.h"

/* Populate a staging calendar without taking ownership of its connection or inputs. */
bool cfg_calendar_populate(sqlite_t *db, const string_t *path, const string_t *key, const string_t *requested,
                           const string_t *saved, bool interactive, const string_t *language,
                           const string_t *saved_language, string_t **chosen_location, string_t **chosen_language)
{
    if (!chosen_location || !chosen_language || chosen_location == chosen_language)
        return false;
    *chosen_location = NULL;
    *chosen_language = NULL;
    const string_t *inputs[] = {path, key, requested, saved, language, saved_language};
    bool ok = db && path && key && string_byte_length(path) && string_byte_length(key);
    for (size_t i = 0; i < sizeof(inputs) / sizeof(*inputs) && ok; ++i)
        ok = cfg_calendar_text_valid(inputs[i]);
    cfg_calendar_place_t place = {0};
    ok = ok && cfg_calendar_catalogue(db) && cfg_calendar_choose_place(db, requested, saved, interactive, &place);
    string_t *locale = ok ? cfg_calendar_choose_language(db, &place, language, saved_language, interactive) : NULL;
    string_t *label =
        locale ? string_sprintf("%s, %s", string_c_str(place.name), string_c_str(place.jurisdiction)) : NULL;
    ok = ok && locale && label && cfg_calendar_snapshot(db, path, key, &place, locale);
    if (ok) {
        *chosen_location = label;
        *chosen_language = locale;
    } else {
        string_free(label);
        string_free(locale);
        fputs("Calendar selection or population failed; the existing installed database has not been replaced.\n",
              stderr);
    }
    cfg_calendar_place_clear(&place);
    return ok;
}
