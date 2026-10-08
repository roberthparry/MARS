/**
 * @file cfg_calendar_menu.c
 * @brief Locale-default resolution and the paginated explicit town-selection menu.
 *
 * Command-line and saved towns take precedence over locale hints. Discovery never
 * accepts a fuzzy match without a numbered selection. The common prompt API owns
 * terminal input, including EOF handling; this module owns all returned records.
 */
#include <stdio.h>
#include <stdlib.h>

#include "cfg_calendar_internal.h"
#include "cfg_prompt.h"

static string_t *cfg_calendar_locale_country(void)
{
    string_t *explicit = string_new_with(getenv("MARS_HOLIDAY_JURISDICTION"));
    if (!explicit)
        return NULL;
    string_trim(explicit);
    string_to_upper(explicit);
    if (string_byte_length(explicit)) {
        if (!cfg_calendar_equal(explicit, "GB"))
            return explicit;
        string_free(explicit);
        return string_new_with("GB-ENG");
    }
    string_free(explicit);
    const char *variables[] = {"LC_ALL", "LC_MESSAGES", "LANG"};
    for (size_t i = 0; i < sizeof(variables) / sizeof(*variables); ++i) {
        const char *raw = getenv(variables[i]);
        string_t *value = raw ? string_new_with(raw) : string_new();
        string_cursor_t *cursor = value ? string_cursor_new(value) : NULL;
        if (!cursor) {
            string_free(value);
            return NULL;
        }
        unsigned state = 0;
        char letters[3] = {0};
        bool found = false;
        while (!string_cursor_done(cursor)) {
            uint32_t ch = rune_value(string_cursor_peek(cursor));
            bool alpha = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
            if (state == 3 && (ch == '.' || ch == '@')) {
                found = true;
                break;
            }
            if ((state == 1 || state == 2) && alpha)
                letters[state++ - 1] = (char)ch;
            else
                state = ch == '_' ? 1 : 0;
            string_cursor_next(cursor);
        }
        found = found || state == 3;
        string_cursor_free(cursor);
        string_free(value);
        if (found) {
            string_t *country = string_new_with(letters);
            if (!country)
                return NULL;
            string_to_upper(country);
            if (!cfg_calendar_equal(country, "GB"))
                return country;
            string_free(country);
            return string_new_with("GB-ENG");
        }
    }
    return string_new();
}

static bool cfg_calendar_default(sqlite_t *db, cfg_calendar_place_t *place, bool *found)
{
    string_t *country = cfg_calendar_locale_country();
    sqlite_stmt_t *stmt =
        country
            ? sqlite_stmt_prepare(db, "SELECT name,jurisdiction,latitude,longitude,timezone FROM cfg_calendar_places "
                                      "WHERE default_for=?1 ORDER BY rowid DESC LIMIT 1")
            : NULL;
    bool ok = stmt && sqlite_stmt_bind_text(stmt, 1, string_c_str(country));
    sqlite_step_result_t step = ok ? sqlite_stmt_step(stmt) : SQLITE_STEP_ERROR;
    *found = step == SQLITE_STEP_ROW;
    if (*found) {
        *place = (cfg_calendar_place_t){.name = string_new_with(sqlite_stmt_column_text(stmt, 0)),
                                        .jurisdiction = string_new_with(sqlite_stmt_column_text(stmt, 1)),
                                        .latitude = sqlite_stmt_column_double(stmt, 2),
                                        .longitude = sqlite_stmt_column_double(stmt, 3),
                                        .timezone = string_new_with(sqlite_stmt_column_text(stmt, 4))};
        ok = place->name && place->jurisdiction && place->timezone;
    } else
        ok = step == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(stmt);
    string_free(country);
    return ok;
}

static bool cfg_calendar_copy_place(cfg_calendar_place_t *out, const cfg_calendar_place_t *in)
{
    *out = (cfg_calendar_place_t){.name = string_clone(in->name),
                                  .jurisdiction = string_clone(in->jurisdiction),
                                  .timezone = string_clone(in->timezone),
                                  .latitude = in->latitude,
                                  .longitude = in->longitude};
    return out->name && out->jurisdiction && out->timezone;
}

static bool cfg_calendar_verified(cfg_calendar_place_t *out, const cfg_calendar_place_t *in)
{
    cfg_calendar_environment_t timezone = {0};
    bool ok = cfg_calendar_zone_enter(in->timezone, &timezone);
    if (!ok)
        fprintf(stderr, "Calendar timezone is unavailable: %s\n", string_c_str(in->timezone));
    if (!cfg_calendar_restore(&timezone)) {
        fprintf(stderr, "Cannot restore the calendar timezone environment.\n");
        ok = false;
    }
    tzset();
    return ok && cfg_calendar_copy_place(out, in);
}

static bool cfg_calendar_resolve(sqlite_t *db, const string_t *chosen, cfg_calendar_place_t *suggested, bool *found)
{
    array_t *matches = cfg_calendar_matches(db, chosen);
    if (!matches)
        return false;
    *found = array_size(matches) == 1;
    bool ok = true;
    if (*found)
        ok = cfg_calendar_copy_place(suggested, array_get(matches, 0));
    else if (array_size(matches)) {
        fprintf(stderr, "Ambiguous calendar location: %s. Specify a jurisdiction:", string_c_str(chosen));
        for (size_t i = 0; i < array_size(matches); ++i) {
            cfg_calendar_place_t *place = array_get(matches, i);
            fprintf(stderr, "%s%s, %s", i ? "; " : " ", string_c_str(place->name), string_c_str(place->jurisdiction));
        }
        fputc('\n', stderr);
    } else {
        array_t *near = cfg_calendar_search(db, chosen);
        fprintf(stderr, "Unknown calendar location: %s.", string_c_str(chosen));
        if (near && array_size(near)) {
            fputs(" Did you mean: ", stderr);
            for (size_t i = 0; i < array_size(near) && i < 6; ++i) {
                cfg_calendar_place_t *place = array_get(near, i);
                fprintf(stderr, "%s%s, %s", i ? "; " : "", string_c_str(place->name),
                        string_c_str(place->jurisdiction));
            }
            fputc('?', stderr);
        }
        fputs(" Supply LOCATION='town, jurisdiction' or use the town menu.\n", stderr);
        ok = near != NULL;
        array_destroy(near);
    }
    array_destroy(matches);
    return ok;
}

/* Select, then validate zoneinfo before the caller modifies its snapshot. */
bool cfg_calendar_choose_place(sqlite_t *db, const string_t *requested, const string_t *saved, bool interactive,
                               cfg_calendar_place_t *place)
{
    string_t *chosen = cfg_calendar_value(requested);
    if (chosen && !string_byte_length(chosen)) {
        string_free(chosen);
        chosen = cfg_calendar_value(saved);
    }
    cfg_calendar_place_t suggested = {0};
    bool found = false;
    bool ok = chosen && (string_byte_length(chosen) ? cfg_calendar_resolve(db, chosen, &suggested, &found)
                                                    : cfg_calendar_default(db, &suggested, &found));
    if (!interactive || !ok) {
        if (ok && !found && !string_byte_length(chosen))
            fputs("No calendar location is configured. Supply a town after install-jurisdiction-db.\n", stderr);
        ok = ok && found && cfg_calendar_verified(place, &suggested);
        cfg_calendar_place_clear(&suggested);
        string_free(chosen);
        return ok;
    }
    array_t *choices = cfg_calendar_search(db, chosen);
    size_t page = 0;
    ok = choices != NULL;
    bool selected = false;
    puts("\nCalendar location — select a town, not your computer's language.");
    while (ok && !selected) {
        size_t count = array_size(choices), pages = count ? (count + 11) / 12 : 1;
        size_t visible = count - page * 12;
        if (visible > 12)
            visible = 12;
        printf("\nTowns — page %zu/%zu (%zu matches)\n", page + 1, pages, count);
        if (found)
            printf("  0. Use %s, %s [Enter]\n", string_c_str(suggested.name), string_c_str(suggested.jurisdiction));
        for (size_t i = 0; i < visible; ++i) {
            cfg_calendar_place_t *row = array_get(choices, page * 12 + i);
            printf("  %zu. %s, %s (%s)\n", i + 1, string_c_str(row->name), string_c_str(row->jurisdiction),
                   string_c_str(row->timezone));
        }
        if (!visible)
            puts("  No matching towns. Try a shorter name, or / to browse all towns.");
        puts("Number = select; type a town to filter; / = all towns; n/p = next/previous; q = cancel.");
        string_t *answer = cfg_prompt_read("Choice: ", false);
        string_t *folded = answer ? cfg_calendar_key(answer, false) : NULL;
        if (!answer || !folded || cfg_calendar_equal(folded, "q")) {
            fputs("Calendar location selection cancelled; existing database unchanged.\n", stderr);
            ok = false;
        } else {
            unsigned number = 0;
            if (found && (!string_byte_length(answer) || cfg_calendar_equal(answer, "0"))) {
                ok = cfg_calendar_verified(place, &suggested);
                selected = ok;
            } else if (cfg_calendar_number(answer, &number)) {
                if (number && number <= visible) {
                    ok = cfg_calendar_verified(place, array_get(choices, page * 12 + number - 1));
                    selected = ok;
                } else
                    puts("Choose one of the numbers shown.");
            } else if (cfg_calendar_equal(folded, "n")) {
                if (page + 1 < pages)
                    ++page;
            } else if (cfg_calendar_equal(folded, "p")) {
                if (page)
                    --page;
            } else if (string_byte_length(answer)) {
                string_t *filter = cfg_calendar_equal(answer, "/") ? string_new() : string_clone(answer);
                array_t *next = filter ? cfg_calendar_search(db, filter) : NULL;
                string_free(filter);
                array_destroy(choices);
                choices = next;
                ok = choices != NULL;
                page = 0;
            } else
                puts("Choose a town number or type a name to filter.");
        }
        string_free(folded);
        string_free(answer);
    }
    array_destroy(choices);
    cfg_calendar_place_clear(&suggested);
    string_free(chosen);
    return ok && selected;
}
