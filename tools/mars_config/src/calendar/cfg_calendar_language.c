/**
 * @file cfg_calendar_language.c
 * @brief Database-backed supported-language selection and installer language menu.
 *
 * Connection-local choice and alias indexes retain spelling collisions instead
 * of guessing. Explicit unavailable requests fail; saved unsupported choices do
 * not displace the catalogue default. The common prompt reader supplies input.
 */
#include <stdio.h>

#include "cfg_calendar_internal.h"
#include "cfg_prompt.h"

static bool cfg_calendar_alias(sqlite_stmt_t *write, int id, const char *spelling)
{
    if (!spelling || !*spelling)
        return true;
    string_t *text = string_new_with(spelling);
    string_t *key = text ? cfg_calendar_key(text, true) : NULL;
    sqlite_stmt_reset(write);
    bool ok = key && sqlite_stmt_bind_text(write, 1, string_c_str(key)) && sqlite_stmt_bind_int(write, 2, id) &&
              sqlite_stmt_step(write) == SQLITE_STEP_DONE;
    string_free(key);
    string_free(text);
    return ok;
}

static bool cfg_calendar_languages(sqlite_t *db, const cfg_calendar_place_t *place)
{
    bool ok = sqlite_exec_cstr(
        db, "DROP TABLE IF EXISTS temp.cfg_calendar_languages; DROP TABLE IF EXISTS temp.cfg_calendar_aliases;"
            "CREATE TEMP TABLE cfg_calendar_languages(id INTEGER PRIMARY KEY,language TEXT,locale TEXT,status TEXT,"
            "english TEXT,native TEXT,default_locale TEXT);"
            "CREATE TEMP TABLE cfg_calendar_aliases(spelling TEXT,id INTEGER,PRIMARY KEY(spelling,id));");
    string_t *town = ok ? cfg_calendar_key(place->name, false) : NULL;
    sqlite_stmt_t *insert =
        town ? sqlite_stmt_prepare(
                   db, "WITH town_choices AS (SELECT language,locale,status FROM calendar_town_language "
                       "WHERE country=substr(?1,1,2) AND town_key=?2),choices AS ("
                       "SELECT language,locale,status FROM calendar_jurisdiction_language WHERE jurisdiction=CASE "
                       "WHEN EXISTS(SELECT 1 FROM calendar_jurisdiction_language WHERE jurisdiction=?1) THEN ?1 "
                       "ELSE substr(?1,1,2) END AND language NOT IN(SELECT language FROM town_choices) "
                       "UNION ALL SELECT language,locale,status FROM town_choices) "
                       "INSERT INTO cfg_calendar_languages(language,locale,status,english,native,default_locale) "
                       "SELECT choices.language,choices.locale,choices.status,names.english_name,names.native_name,"
                       "coalesce(local.locale,country.locale) FROM choices JOIN calendar_language_name names "
                       "USING(language) "
                       "JOIN calendar_territory_locale country ON country.territory=substr(?1,1,2) "
                       "LEFT JOIN calendar_territory_locale local ON local.territory=?1 "
                       "WHERE NOT(?1='PS' AND ?3 IN('Asia/Hebron','Asia/Jerusalem') AND choices.language "
                       "IN('he','yi','lad')) "
                       "ORDER BY names.english_name,choices.language")
             : NULL;
    ok = insert && sqlite_stmt_bind_text(insert, 1, string_c_str(place->jurisdiction)) &&
         sqlite_stmt_bind_text(insert, 2, string_c_str(town)) &&
         sqlite_stmt_bind_text(insert, 3, string_c_str(place->timezone)) &&
         sqlite_stmt_step(insert) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(insert);
    string_free(town);
    sqlite_stmt_t *read =
        ok ? sqlite_stmt_prepare(db, "SELECT id,language,locale,english,native FROM cfg_calendar_languages ORDER BY id")
           : NULL;
    sqlite_stmt_t *write =
        read ? sqlite_stmt_prepare(db, "INSERT OR IGNORE INTO cfg_calendar_aliases VALUES(?1,?2)") : NULL;
    static const struct {
        const char *code;
        const char *spelling;
    } aliases[] = {{"frc", "Cajun"},
                   {"frc", "Louisiana French"},
                   {"ik", "Iñupiaq"},
                   {"ik", "Inupiaq"},
                   {"ik", "ipk"},
                   {"ik", "Alaskan Inuit"},
                   {"jam", "Jamaican Patwa"},
                   {"jam", "Jamaican Patwah"},
                   {"jam", "Jamaican Creole"},
                   {"jam", "Jamaican English Patois"},
                   {"jam", "Patwah"},
                   {"jam", "Patois"},
                   {"pdc", "Amish"},
                   {"pdc", "Pennsylvania Dutch"},
                   {"pdc", "Pennsylvania German"},
                   {"pdc", "Deitsch"}};
    ok = read && write;
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    while (ok && (step = sqlite_stmt_step(read)) == SQLITE_STEP_ROW) {
        int id = sqlite_stmt_column_int(read, 0);
        for (int column = 1; column <= 4 && ok; ++column)
            ok = cfg_calendar_alias(write, id, sqlite_stmt_column_text(read, column));
        string_t *code = string_new_with(sqlite_stmt_column_text(read, 1));
        ok = ok && code;
        /* Sixteen fixed compatibility spellings; not a catalogue-sized scan. */
        for (size_t i = 0; i < sizeof(aliases) / sizeof(*aliases) && ok; ++i)
            if (cfg_calendar_equal(code, aliases[i].code))
                ok = cfg_calendar_alias(write, id, aliases[i].spelling);
        string_free(code);
    }
    sqlite_stmt_finalize(write);
    sqlite_stmt_finalize(read);
    return ok && step == SQLITE_STEP_DONE;
}

static bool cfg_calendar_language_match(sqlite_t *db, const string_t *value, int *id)
{
    string_t *key = value ? cfg_calendar_key(value, true) : string_new();
    sqlite_stmt_t *stmt =
        key ? sqlite_stmt_prepare(
                  db, "SELECT CASE WHEN count(*)=1 THEN min(id) ELSE 0 END FROM cfg_calendar_aliases WHERE spelling=?1")
            : NULL;
    bool ok = stmt && sqlite_stmt_bind_text(stmt, 1, string_c_str(key)) && sqlite_stmt_step(stmt) == SQLITE_STEP_ROW;
    if (ok)
        *id = sqlite_stmt_column_int(stmt, 0);
    sqlite_stmt_finalize(stmt);
    string_free(key);
    return ok;
}

static bool cfg_calendar_language_locale(sqlite_t *db, int id, string_t **locale)
{
    *locale = NULL;
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db, "SELECT locale FROM cfg_calendar_languages WHERE id=?1");
    bool ok = stmt && sqlite_stmt_bind_int(stmt, 1, id);
    sqlite_step_result_t step = ok ? sqlite_stmt_step(stmt) : SQLITE_STEP_ERROR;
    if (step == SQLITE_STEP_ROW && !sqlite_stmt_column_is_null(stmt, 0)) {
        const char *raw = sqlite_stmt_column_text(stmt, 0);
        if (raw && *raw) {
            *locale = string_new_with(raw);
            ok = *locale != NULL;
        }
    } else
        ok = step != SQLITE_STEP_ERROR;
    sqlite_stmt_finalize(stmt);
    return ok;
}

static bool cfg_calendar_language_display(sqlite_t *db, int chosen, bool error)
{
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(
        db, "SELECT id,english,native,locale,CASE status WHEN 'official' THEN 'official' "
            "WHEN 'de_facto_official' THEN 'de facto official' WHEN 'official_regional' THEN 'regional official' "
            "WHEN 'regional' THEN 'regional language' WHEN 'additional' THEN 'additional language' "
            "WHEN 'holy_see' THEN 'Holy See' WHEN 'default' THEN 'calendar default' ELSE status END "
            "FROM cfg_calendar_languages ORDER BY id");
    if (!stmt)
        return false;
    sqlite_step_result_t step;
    while ((step = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        const char *locale = sqlite_stmt_column_text(stmt, 3);
        if (error) {
            if (locale && *locale)
                fprintf(stderr, " %s (%s);", sqlite_stmt_column_text(stmt, 1), locale);
        } else
            printf("  %d. %s — %s (%s; %s)%s\n", sqlite_stmt_column_int(stmt, 0), sqlite_stmt_column_text(stmt, 1),
                   sqlite_stmt_column_text(stmt, 2), sqlite_stmt_column_text(stmt, 4),
                   locale && *locale ? locale : "calendar names unavailable",
                   sqlite_stmt_column_int(stmt, 0) == chosen ? " [Enter]" : "");
    }
    sqlite_stmt_finalize(stmt);
    return step == SQLITE_STEP_DONE;
}

/* Preserve explicit-request errors and saved/default precedence, including alias collisions. */
string_t *cfg_calendar_choose_language(sqlite_t *db, const cfg_calendar_place_t *place, const string_t *requested,
                                       const string_t *saved, bool interactive)
{
    if (!cfg_calendar_languages(db, place))
        return NULL;
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(
        db, "SELECT id,(SELECT count(*) FROM cfg_calendar_languages) FROM cfg_calendar_languages "
            "WHERE locale IS NOT NULL AND locale<>'' ORDER BY "
            "CASE WHEN locale=(SELECT default_locale FROM cfg_calendar_languages ORDER BY id LIMIT 1) "
            "THEN 0 ELSE 1 END,id LIMIT 1");
    bool ok = stmt && sqlite_stmt_step(stmt) == SQLITE_STEP_ROW;
    int chosen = ok ? sqlite_stmt_column_int(stmt, 0) : 0;
    int count = ok ? sqlite_stmt_column_int(stmt, 1) : 0;
    sqlite_stmt_finalize(stmt);
    if (!ok) {
        fprintf(stderr, "No supported calendar language data for %s.\n", string_c_str(place->jurisdiction));
        return NULL;
    }
    const string_t *values[] = {saved, requested};
    bool invalid_request = false;
    for (size_t i = 0; i < 2 && ok; ++i) {
        int id = 0;
        string_t *locale = NULL;
        ok = cfg_calendar_language_match(db, values[i], &id) && cfg_calendar_language_locale(db, id, &locale);
        if (locale)
            chosen = id;
        else if (i == 1 && values[i] && string_byte_length(values[i])) {
            invalid_request = true;
            fprintf(stderr, "Calendar language '%s' is unavailable for %s. Choose:", string_c_str(values[i]),
                    string_c_str(place->jurisdiction));
            ok = ok && cfg_calendar_language_display(db, chosen, true);
            fputc('\n', stderr);
            if (!interactive)
                ok = false;
        }
        string_free(locale);
    }
    if (!interactive || (count == 1 && !invalid_request)) {
        string_t *locale = NULL;
        if (ok)
            ok = cfg_calendar_language_locale(db, chosen, &locale);
        if (ok && interactive)
            ok = cfg_calendar_language_display(db, chosen, false);
        if (!ok) {
            string_free(locale);
            locale = NULL;
        }
        return locale;
    }
    while (ok) {
        printf("\nCalendar language for %s\n", string_c_str(place->jurisdiction));
        if (!cfg_calendar_language_display(db, chosen, false))
            return NULL;
        string_t *answer = cfg_prompt_read("Language number or name [Enter = default; q = cancel]: ", false);
        string_t *folded = answer ? cfg_calendar_key(answer, true) : NULL;
        if (!folded || cfg_calendar_equal(folded, "q")) {
            fputs("Calendar language selection cancelled; existing database unchanged.\n", stderr);
            string_free(answer);
            string_free(folded);
            return NULL;
        }
        int id = chosen;
        unsigned number = 0;
        if (cfg_calendar_number(answer, &number))
            id = (int)number;
        else if (string_byte_length(answer))
            ok = cfg_calendar_language_match(db, answer, &id);
        string_t *locale = NULL;
        ok = ok && cfg_calendar_language_locale(db, id, &locale);
        string_free(answer);
        string_free(folded);
        if (locale)
            return locale;
        if (ok)
            puts("Choose a listed language with available calendar names.");
    }
    return NULL;
}
