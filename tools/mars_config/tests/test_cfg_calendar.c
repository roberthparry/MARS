/**
 * @file test_cfg_calendar.c
 * @brief Isolated catalogue, language-menu and native-policy calendar regressions.
 *
 * Uses the packaged encrypted jurisdiction database beneath a disposable home.
 * Fixtures cover indexed selection, explicit menu input, locale eligibility,
 * translation precedence, local-noon offsets and snapshot rollback. Rendering
 * oracles belong to test_cfg_rendering.c; README snapshots register separately
 * so the parent runner can execute them after every ordinary test group.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../src/calendar/cfg_calendar_internal.h"
#include "cfg_calendar.h"
#include "file.h"
#include "test_cfg_calendar_fixture.h"
#include "test_cfg_support.h"
#include "test_harness.h"

/** Register documented calendar snapshots after all ordinary test groups. */
void test_cfg_calendar_readme_cases(void);

static bool test_calendar_text(const string_t *text, const char *expected)
{
    return expected ? text && string_view_equals_literal(string_view_all(text), expected) : text == NULL;
}

static bool test_calendar_query(sqlite_t *db, const char *sql, const char *expected)
{
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db, sql);
    bool ok = stmt && sqlite_stmt_step(stmt) == SQLITE_STEP_ROW;
    const char *raw = ok ? sqlite_stmt_column_text(stmt, 0) : NULL;
    string_t *actual = raw ? string_new_with(raw) : NULL;
    ok = ok && test_calendar_text(actual, expected) && sqlite_stmt_step(stmt) == SQLITE_STEP_DONE;
    if (!ok)
        fprintf(stderr, "Calendar regression query mismatch: %s; expected '%s', received '%s'\n", sql,
                expected ? expected : "NULL", actual ? string_c_str(actual) : "NULL");
    string_free(actual);
    sqlite_stmt_finalize(stmt);
    return ok;
}

static bool test_calendar_place(sqlite_t *db, const char *requested, const char *saved, bool interactive,
                                const char *label)
{
    string_t *request = string_new_with(requested), *previous = string_new_with(saved);
    cfg_calendar_place_t place = {0};
    bool selected = request && previous && cfg_calendar_choose_place(db, request, previous, interactive, &place);
    string_t *actual =
        selected ? string_sprintf("%s, %s", string_c_str(place.name), string_c_str(place.jurisdiction)) : NULL;
    bool ok = test_calendar_text(actual, label) && (label ? selected : !selected);
    if (!ok)
        fprintf(stderr,
                "Calendar town selection mismatch for '%s' (saved '%s'): expected '%s', received '%s' "
                "(selected=%d)\n",
                requested ? requested : "", saved ? saved : "", label ? label : "NULL",
                actual ? string_c_str(actual) : "NULL", selected);
    string_free(actual);
    string_free(previous);
    string_free(request);
    cfg_calendar_place_clear(&place);
    return ok;
}

static bool test_calendar_language(sqlite_t *db, const char *town, const char *jurisdiction, const char *zone,
                                   const char *request, const char *saved, bool interactive, const char *expected)
{
    cfg_calendar_place_t place = {.name = string_new_with(town),
                                  .jurisdiction = string_new_with(jurisdiction),
                                  .timezone = string_new_with(zone)};
    string_t *requested = string_new_with(request), *previous = string_new_with(saved);
    bool allocated = place.name && place.jurisdiction && place.timezone && requested && previous;
    string_t *locale = allocated ? cfg_calendar_choose_language(db, &place, requested, previous, interactive) : NULL;
    bool ok = allocated && test_calendar_text(locale, expected);
    if (!ok)
        fprintf(stderr,
                "Calendar language mismatch for '%s', '%s', request '%s' (saved '%s'): "
                "expected '%s', received '%s'\n",
                town, jurisdiction, request ? request : "", saved ? saved : "", expected ? expected : "NULL",
                locale ? string_c_str(locale) : "NULL");
    string_free(locale);
    string_free(requested);
    string_free(previous);
    cfg_calendar_place_clear(&place);
    return ok;
}

static int test_calendar_input(const char *input)
{
    int descriptors[2];
    if (pipe(descriptors))
        return -1;
    string_t *text = string_new_with(input);
    size_t length = text ? string_byte_length(text) : 0;
    bool ok = text && write(descriptors[1], string_c_str(text), length) == (ssize_t)length;
    string_free(text);
    ok = close(descriptors[1]) == 0 && ok;
    int saved = ok ? dup(STDIN_FILENO) : -1;
    ok = saved >= 0 && dup2(descriptors[0], STDIN_FILENO) >= 0 && ok;
    ok = close(descriptors[0]) == 0 && ok;
    clearerr(stdin);
    if (!ok && saved >= 0) {
        if (dup2(saved, STDIN_FILENO) < 0)
            fputs("Cannot restore regression input.\n", stderr);
        close(saved);
    }
    return ok ? saved : -1;
}

static bool test_calendar_input_restore(int saved)
{
    bool ok = saved >= 0 && dup2(saved, STDIN_FILENO) >= 0;
    if (saved >= 0)
        ok = close(saved) == 0 && ok;
    clearerr(stdin);
    return ok;
}

static bool test_calendar_catalogue_fixture(const char *directory)
{
    string_t *path = NULL, *key = NULL;
    sqlite_t *db = test_cfg_calendar_database(directory, &path, &key);
    bool ok = db && cfg_calendar_catalogue(db);
    static const struct {
        const char *input;
        const char *label;
    } cases[] = {{"LiMmEn", "Limmen, NL"},
                 {"Shrewsbury", "Shrewsbury, GB-ENG"},
                 {"Shrewsbury, GB", "Shrewsbury, GB"},
                 {"Quebec", "Québec, CA-QC"},
                 {"Québec", "Québec, CA-QC"},
                 {"QUÉBEC", "Québec, CA-QC"},
                 {"  Quebec  City ", "Québec, CA-QC"},
                 {"Québec, ca-qc", "Québec, CA-QC"},
                 {"Quebec, CA", "Québec, CA"},
                 {"Montreal", "Montréal, CA"},
                 {"Montréal", "Montréal, CA"},
                 {"Montréal", "Montréal, CA"},
                 {"limmen'; drop table jurisdiction; --", NULL},
                 {"Quebc", NULL}};
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases) && ok; ++i)
        ok = test_calendar_place(db, cases[i].input, "", false, cases[i].label);
    ok = ok && test_calendar_place(db, "limmen", "Shrewsbury", false, "Limmen, NL") &&
         test_calendar_place(db, "", "Limmen, NL", false, "Limmen, NL") &&
         test_calendar_place(db, "", "", false, "Shrewsbury, GB-ENG") && !setenv("LANG", "nl_NL.UTF-8", 1) &&
         test_calendar_place(db, "", "", false, "Limmen, NL") && !setenv("LANG", "C", 1) &&
         test_calendar_place(db, "", "", false, NULL) && !setenv("MARS_HOLIDAY_JURISDICTION", "GB", 1) &&
         test_calendar_place(db, "", "", false, "Shrewsbury, GB-ENG") && !unsetenv("MARS_HOLIDAY_JURISDICTION") &&
         sqlite_exec_cstr(db, "INSERT INTO cfg_calendar_places VALUES"
                              "('ambiguous','Ambiguous','NL',52,4,'Europe/Amsterdam',NULL),"
                              "('ambiguous','Ambiguous','GB-ENG',53,-2,'Europe/London',NULL),"
                              "('accent','Áccent','NL',52,4,'Europe/Amsterdam',NULL),"
                              "('accent','Accent','NL',53,5,'Europe/Amsterdam',NULL),"
                              "('washington, d.c.','Washington, D.C.','US-DC',38.9,-77,'America/New_York',NULL)") &&
         test_calendar_place(db, "ambiguous", "", false, NULL) &&
         test_calendar_place(db, "ambiguous, NL", "", false, "Ambiguous, NL") &&
         test_calendar_place(db, "accent", "", false, NULL) &&
         test_calendar_place(db, "Washington, D.C.", "", false, "Washington, D.C., US-DC") &&
         test_calendar_place(db, "Washington, D.C., US-DC", "", false, "Washington, D.C., US-DC");
    sqlite_close(db);
    string_free(path);
    string_free(key);
    return ok;
}

static bool test_calendar_menu_fixture(const char *directory)
{
    string_t *path = NULL, *key = NULL;
    sqlite_t *db = test_cfg_calendar_database(directory, &path, &key);
    bool ok = db && cfg_calendar_catalogue(db) && !setenv("LANG", "C", 1) && !setvbuf(stdin, NULL, _IONBF, 0);
    static const struct {
        const char *input;
        const char *request;
        const char *saved;
        const char *expected;
    } cases[] = {{"n\np\n999\nQuebc\n1\n", "", "", "Québec, CA-QC"},
                 {"/\nno-such-town-9182\nQuebec\n1\n", "Quebc", "", "Québec, CA-QC"},
                 {"\n", "", "Limmen, NL", "Limmen, NL"},
                 {"q\n", "Quebec", "", NULL},
                 {"", "Quebec", "", NULL},
                 {"\nq\n", "Quebc", "", NULL}};
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases) && ok; ++i) {
        int input = test_calendar_input(cases[i].input);
        ok = input >= 0 && test_calendar_place(db, cases[i].request, cases[i].saved, true, cases[i].expected);
        ok = test_calendar_input_restore(input) && ok;
    }
    static const struct {
        const char *input;
        const char *jurisdiction;
        const char *expected;
    } languages[] = {{"\n", "CA-QC", "fr_CA"},
                     {"1\n", "CA-QC", "en_CA"},
                     {"2\n", "CA-QC", "fr_CA"},
                     {"en-CA\n", "CA-QC", "en_CA"},
                     {"999\n2\n", "VA", "la_VA"},
                     {"q\n", "VA", NULL},
                     {"", "VA", NULL}};
    for (size_t i = 0; i < sizeof(languages) / sizeof(*languages) && ok; ++i) {
        int input = test_calendar_input(languages[i].input);
        ok = input >= 0 &&
             test_calendar_language(db, "", languages[i].jurisdiction, "", "", "", true, languages[i].expected);
        ok = test_calendar_input_restore(input) && ok;
    }
    int input = ok ? test_calendar_input("3\nHebrew\nYiddish\nLadino\n2\n") : -1;
    ok = ok && input >= 0 && test_calendar_language(db, "Ramallah", "PS", "Asia/Hebron", "", "yi", true, "en");
    ok = test_calendar_input_restore(input) && ok;
    sqlite_close(db);
    string_free(path);
    string_free(key);
    return ok;
}

static bool test_calendar_languages_fixture(const char *directory)
{
    string_t *path = NULL, *key = NULL;
    sqlite_t *db = test_cfg_calendar_database(directory, &path, &key);
    bool ok = db != NULL;
    static const struct {
        const char *town, *jurisdiction, *zone, *request, *saved, *expected;
    } cases[] = {{"Québec", "CA-QC", "America/Toronto", "", "", "fr_CA"},
                 {"Québec", "CA-QC", "America/Toronto", "English", "", "en_CA"},
                 {"Québec", "CA-QC", "America/Toronto", "French", "en_CA", "fr_CA"},
                 {"Québec", "CA-QC", "America/Toronto", "", "en_CA", "en_CA"},
                 {"Vatican City", "VA", "Europe/Vatican", "la-VA", "", "la_VA"},
                 {"Vatican City", "VA", "Europe/Vatican", "", "la_VA", "la_VA"},
                 {"Vatican City", "VA", "Europe/Vatican", "", "nl_NL", "it_VA"},
                 {"Vatican City", "VA", "Europe/Vatican", "Dutch", "", NULL},
                 {"Paris", "FR", "Europe/Paris", "Breton", "", "br_FR"},
                 {"Paris", "FR", "Europe/Paris", "br", "", "br_FR"},
                 {"Paris", "FR", "Europe/Paris", "br_FR", "", "br_FR"},
                 {"Paris", "FR", "Europe/Paris", "br-FR", "", "br_FR"},
                 {"Paris", "FR", "Europe/Paris", "brezhoneg", "", "br_FR"},
                 {"Paris", "FR", "Europe/Paris", "", "br_FR", "br_FR"},
                 {"Paris", "FR", "Europe/Paris", "", "", "fr_FR"},
                 {"Paris", "FR", "Europe/Paris", "Cornish", "", NULL},
                 {"Shrewsbury", "GB-ENG", "Europe/London", "Kernewek", "", "kw_GB"},
                 {"Shrewsbury", "GB-ENG", "Europe/London", "Cornish", "", "kw_GB"},
                 {"Shrewsbury", "GB-ENG", "Europe/London", "kw", "", "kw_GB"},
                 {"Shrewsbury", "GB-ENG", "Europe/London", "kw_GB", "", "kw_GB"},
                 {"Shrewsbury", "GB-ENG", "Europe/London", "kw-GB", "", "kw_GB"},
                 {"Shrewsbury", "GB-ENG", "Europe/London", "", "kw_GB", "kw_GB"},
                 {"Shrewsbury", "GB-ENG", "Europe/London", "Breton", "", NULL},
                 {"Shrewsbury", "GB-ENG", "Europe/London", "", "", "en_GB"},
                 {"New York City", "US-NY", "America/New_York", "Cajun", "", "frc"},
                 {"New York City", "US-NY", "America/New_York", "français cadien", "", "frc"},
                 {"New Orleans", "US-LA", "America/Chicago", "Louisiana French", "", "frc"},
                 {"Philadelphia", "US-PA", "America/New_York", "Amish", "", "pdc"},
                 {"Philadelphia", "US-PA", "America/New_York", "Pennsylvania Dutch", "", "pdc"},
                 {"Philadelphia", "US-PA", "America/New_York", "Pennsilfaanisch Deitsch", "", "pdc"},
                 {"Philadelphia", "US-PA", "America/New_York", "Deitsch", "", "pdc"},
                 {"Philadelphia", "US-PA", "America/New_York", "", "", "en_US"},
                 {"Anchorage", "US-AK", "America/Anchorage", "ipk", "", "ik"},
                 {"Anchorage", "US-AK", "America/Anchorage", "Iñupiatun", "", "ik"},
                 {"Anchorage", "US-AK", "America/Anchorage", "Alaskan Inuit", "", "ik"},
                 {"Anchorage", "US-AK", "America/Anchorage", "Iñupiaq (North Slope)", "", "ik"},
                 {"Anchorage", "US-AK", "America/Anchorage", "", "", "en_US"},
                 {"Anchorage", "US-AK", "America/Anchorage", "Inuktitut", "", NULL},
                 {"Anchorage", "US-AK", "America/Anchorage", "Yup'ik", "", NULL},
                 {"New York City", "US-NY", "America/New_York", "Iñupiaq", "", NULL},
                 {"Limmen", "NL", "Europe/Amsterdam", "Amish", "", NULL},
                 {"Limmen", "NL", "Europe/Amsterdam", "Cajun", "", NULL},
                 {"Jerusalem", "IL", "Asia/Jerusalem", "", "", "he_IL"},
                 {"Gaza", "PS", "Asia/Gaza", "", "", "ar_PS"},
                 {"Gaza", "PS", "Asia/Gaza", "Hebrew", "", "he"},
                 {"Gaza", "PS", "Asia/Gaza", "ייִדיש", "", "yi"},
                 {"Gaza", "PS", "Asia/Gaza", "Djudeo-espanyol", "", "lad"},
                 {"New York City", "US-NY", "America/New_York", "Yiddish", "", "yi"},
                 {"Ramallah", "PS", "Asia/Hebron", "Hebrew", "", NULL},
                 {"Ramallah", "PS", "Asia/Hebron", "עברית", "", NULL},
                 {"Ramallah", "PS", "Asia/Hebron", "Yiddish", "", NULL},
                 {"Ramallah", "PS", "Asia/Hebron", "Ladino", "", NULL},
                 {"Ramallah", "PS", "Asia/Hebron", "", "he", "ar_PS"},
                 {"Ramallah", "PS", "Asia/Hebron", "", "yi", "ar_PS"},
                 {"East Jerusalem", "PS", "Asia/Jerusalem", "", "lad", "ar_PS"},
                 {"Shrewsbury", "GB-ENG", "Europe/London", "Patwa", "", NULL},
                 {"Shrewsbury", "GB-ENG", "Europe/London", "", "jam", "en_GB"},
                 {"London'; drop table holiday_name; --", "GB-ENG", "Europe/London", "Patwa", "", NULL}};
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases) && ok; ++i)
        ok = test_calendar_language(db, cases[i].town, cases[i].jurisdiction, cases[i].zone, cases[i].request,
                                    cases[i].saved, false, cases[i].expected);
    const char *towns[] = {"Birmingham", "Bristol", "Leeds", "Leicester", "London", "Manchester", "Nottingham"};
    const char *aliases[] = {"Patwa",          "Patwah",          "Patois",          "Jamaican Patois",
                             "Jamaican Patwa", "Jamaican Patwah", "Jamaican Creole", "Jamaican English Patois"};
    for (size_t i = 0; i < sizeof(towns) / sizeof(*towns) && ok; ++i)
        ok = test_calendar_language(db, towns[i], "GB-ENG", "Europe/London", "jam", "", false, "jam") &&
             test_calendar_language(db, towns[i], "GB", "Europe/London", "jam", "", false, "jam") &&
             test_calendar_language(db, towns[i], "GB-ENG", "Europe/London", "", "", false, "en_GB");
    for (size_t i = 0; i < sizeof(aliases) / sizeof(*aliases) && ok; ++i)
        ok = test_calendar_language(db, "Kingston", "JM", "America/Jamaica", aliases[i], "", false, "jam");
    /* Collision fixtures must not silently pick one language. */
    ok = ok &&
         sqlite_exec_cstr(db, "UPDATE calendar_language_name SET native_name='shared-name' "
                              "WHERE language IN('en','fr')") &&
         test_calendar_language(db, "Québec", "CA-QC", "America/Toronto", "shared-name", "", false, NULL) &&
         test_calendar_language(db, "Québec", "CA-QC", "America/Toronto", "", "shared-name", false, "fr_CA");
    sqlite_close(db);
    string_free(path);
    string_free(key);
    return ok;
}

static bool test_calendar_labels(sqlite_t *db, const char *jurisdiction, const char *locale)
{
    string_t *code = string_new_with(jurisdiction), *language = string_new_with(locale);
    bool ok = code && language && cfg_calendar_labels(db, code, language);
    string_free(code);
    string_free(language);
    return ok;
}

static bool test_calendar_label(sqlite_t *db, const char *source, const char *expected)
{
    sqlite_stmt_t *stmt =
        sqlite_stmt_prepare(db, "SELECT DISTINCT translated FROM cfg_calendar_labels WHERE source=?1");
    bool ok = stmt && sqlite_stmt_bind_text(stmt, 1, source) && sqlite_stmt_step(stmt) == SQLITE_STEP_ROW;
    string_t *actual = ok ? string_new_with(sqlite_stmt_column_text(stmt, 0)) : NULL;
    ok = ok && test_calendar_text(actual, expected) && sqlite_stmt_step(stmt) == SQLITE_STEP_DONE;
    string_free(actual);
    sqlite_stmt_finalize(stmt);
    return ok;
}

static bool test_calendar_labels_fixture(const char *directory)
{
    string_t *path = NULL, *key = NULL;
    sqlite_t *db = test_cfg_calendar_database(directory, &path, &key);
    bool ok = db != NULL;
    static const struct {
        const char *jurisdiction, *locale, *source, *translation;
    } golden[] = {{"US-NY", "es_US", "Armistice Day", "Día del Armisticio"},
                  {"US-NY", "es_US", "Christmas Day", "Día de Navidad"},
                  {"US-NY", "es_US", "Columbus Day", "Día de la Raza"},
                  {"US-NY", "es_US", "Independence Day", "Día de la Independencia"},
                  {"US-NY", "es_US", "Juneteenth National Independence Day", "Día de la Liberación (Juneteenth)"},
                  {"US-NY", "es_US", "Labor Day", "Día del Trabajo"},
                  {"US-NY", "es_US", "Martin Luther King Jr. Day", "Día de Martin Luther King, Jr."},
                  {"US-NY", "es_US", "Memorial Day", "Día de la Conmemoración de los Caídos"},
                  {"US-NY", "es_US", "New Year's Day", "Día de Año Nuevo"},
                  {"US-NY", "es_US", "Thanksgiving Day", "Día de Acción de Gracias"},
                  {"US-NY", "es_US", "Veterans Day", "Día de los Veteranos"},
                  {"US-NY", "es_US", "Washington's Birthday", "Natalicio de George Washington"},
                  {"CA-QC", "fr_CA", "New Year's Day", "Jour de l’An"},
                  {"CA-QC", "fr_CA", "Good Friday", "Vendredi saint"},
                  {"CA-QC", "fr_CA", "Canada Day", "Fête du Canada"},
                  {"CA-QC", "fr_CA", "Labour Day", "Fête du Travail"},
                  {"CA-QC", "fr_CA", "Christmas Day", "Noël"},
                  {"CA-QC", "fr_CA", "Dominion Day", "Fête du Dominion"},
                  {"NL", "fy_NL", "Nieuwjaarsdag", "Nijjiersdei"},
                  {"NL", "fy_NL", "Goede Vrijdag", "Goed Freed"},
                  {"NL", "fy_NL", "Eerste Paasdag", "Earste Peaskedei"},
                  {"NL", "fy_NL", "Tweede Paasdag", "Twadde Peaskedei"},
                  {"NL", "fy_NL", "Koningsdag", "Keningsdei"},
                  {"NL", "fy_NL", "Bevrijdingsdag", "Befrijingsdei"},
                  {"NL", "fy_NL", "Hemelvaartsdag", "Himelfeartsdei"},
                  {"NL", "fy_NL", "Eerste Pinksterdag", "Earste Pinksterdei"},
                  {"NL", "fy_NL", "Tweede Pinksterdag", "Twadde Pinksterdei"},
                  {"NL", "fy_NL", "Eerste Kerstdag", "Earste Krystdei"},
                  {"NL", "fy_NL", "Tweede Kerstdag", "Twadde Krystdei"},
                  {"NL", "fy_NL", "Koninginnedag", "Keninginnedei"}};
    for (size_t i = 0; i < sizeof(golden) / sizeof(*golden) && ok; ++i)
        ok = test_calendar_labels(db, golden[i].jurisdiction, golden[i].locale) &&
             test_calendar_label(db, golden[i].source, golden[i].translation);
    ok = ok &&
         sqlite_exec_cstr(db, "INSERT INTO jurisdiction_entity VALUES('TEST-CHILD');"
                              "INSERT INTO jurisdiction_parent_jurisdiction_id VALUES('TEST-CHILD','GB-ENG');"
                              "INSERT INTO holiday_name(holiday_id,locale,localized_name,is_primary) VALUES"
                              "(1,'ar','رأس السنة','N'),(1,'ar-GB','رأس السنة الميلادية','N');"
                              "INSERT INTO holiday_event_localized_name VALUES"
                              "(1,'ar','Named closure','عام'),(1,'ar-GB','Named closure','خاص');") &&
         test_calendar_labels(db, "TEST-CHILD", "AR_gb") &&
         test_calendar_query(db,
                             "SELECT translated FROM cfg_calendar_labels WHERE holiday=1 "
                             "AND source='New Years Day'",
                             "رأس السنة الميلادية") &&
         test_calendar_query(db,
                             "SELECT translated FROM cfg_calendar_labels WHERE holiday=1 "
                             "AND source='Named closure'",
                             "خاص") &&
         test_calendar_query(db,
                             "SELECT count(*) FROM cfg_calendar_labels WHERE source IN "
                             "('Unknown closure','New Years Day (substitute day)')",
                             "0") &&
         test_calendar_labels(db, "TEST-CHILD", "ar_PS") &&
         test_calendar_query(db,
                             "SELECT translated FROM cfg_calendar_labels WHERE holiday=1 "
                             "AND source='New Years Day'",
                             "رأس السنة") &&
         test_calendar_query(db,
                             "SELECT translated FROM cfg_calendar_labels WHERE holiday=1 "
                             "AND source='Named closure'",
                             "عام") &&
         test_calendar_labels(db, "US-NY", "es_US") &&
         test_calendar_query(db,
                             "SELECT translated FROM cfg_calendar_labels WHERE holiday=1005125 "
                             "AND source='Independence Day (observed)'",
                             "Día de la Independencia (día de observancia)") &&
         test_calendar_query(db,
                             "SELECT count(*) FROM cfg_calendar_labels WHERE source IN "
                             "('Independence Day (special closure)','Unknown Day (observed)',"
                             "'Independence Day (observed) (observed)')",
                             "0") &&
         sqlite_exec_cstr(db,
                          "INSERT INTO holiday_name_qualifier VALUES('es_US',' (observed)',' (festivo trasladado)')") &&
         test_calendar_labels(db, "US", "ES-us") &&
         test_calendar_query(db,
                             "SELECT translated FROM cfg_calendar_labels WHERE holiday=1005125 "
                             "AND source='Independence Day (observed)'",
                             "Día de la Independencia (festivo trasladado)") &&
         test_calendar_labels(db, "US", "es_MX") &&
         test_calendar_query(db,
                             "SELECT translated FROM cfg_calendar_labels WHERE holiday=1005125 "
                             "AND source='Independence Day (observed)'",
                             "Día de la Independencia (día de observancia)") &&
         test_calendar_labels(db, "GB-ENG", "") &&
         test_calendar_query(db, "SELECT count(*) FROM cfg_calendar_labels", "0");
    sqlite_close(db);
    string_free(path);
    string_free(key);
    return ok;
}

static bool test_calendar_populate(sqlite_t *db, const string_t *path, const string_t *key, const char *town,
                                   const char *language, bool expected)
{
    string_t *request = string_new_with(town), *locale = string_new_with(language);
    string_t *chosen = NULL, *chosen_language = NULL;
    bool allocated = request && locale;
    bool result = allocated &&
                  cfg_calendar_populate(db, path, key, request, NULL, false, locale, NULL, &chosen, &chosen_language);
    bool ok = allocated && result == expected && (expected ? chosen && chosen_language : !chosen && !chosen_language);
    string_free(request);
    string_free(locale);
    string_free(chosen);
    string_free(chosen_language);
    return ok;
}

static bool test_calendar_snapshot_fixture(const char *directory)
{
    string_t *path = NULL, *key = NULL;
    sqlite_t *db = test_cfg_calendar_database(directory, &path, &key);
    bool ok = db && !setenv("TZ", "Pacific/Honolulu", 1) && !setenv("MARS_JURISDICTION_DB_PATH", "sentinel", 1) &&
              !setenv("MARS_JURISDICTION_DB_KEY", "", 1) &&
              test_calendar_populate(db, path, key, "Shrewsbury", "English", true);
    string_t *tz = string_new_with(getenv("TZ")), *old_path = string_new_with(getenv("MARS_JURISDICTION_DB_PATH"));
    string_t *old_key = string_new_with(getenv("MARS_JURISDICTION_DB_KEY"));
    ok = ok && test_calendar_text(tz, "Pacific/Honolulu") && test_calendar_text(old_path, "sentinel") &&
         test_calendar_text(old_key, "") &&
         test_calendar_query(db, "SELECT min(calendar_date) FROM calendar_local_days", "2015-01-01") &&
         test_calendar_query(db, "SELECT holiday_name FROM calendar_local_days WHERE calendar_date='2022-06-03'",
                             "Platinum Jubilee Bank Holiday") &&
         test_calendar_query(db, "SELECT holiday_name FROM calendar_local_days WHERE calendar_date='2022-09-19'",
                             "State Funeral of Queen Elizabeth II") &&
         test_calendar_query(db, "SELECT holiday_name FROM calendar_local_days WHERE calendar_date='2023-05-08'",
                             "Coronation of King Charles III") &&
         test_calendar_query(db,
                             "SELECT sum(is_weekend) FROM calendar_local_days "
                             "WHERE calendar_date BETWEEN '2026-01-01' AND '2026-01-07'",
                             "2");
    string_free(tz);
    string_free(old_path);
    string_free(old_key);
    static const struct {
        int year;
        const char *dates;
    } bank_holidays[] = {{2019, "01-01 04-19 04-22 05-06 05-27 08-26 12-25 12-26"},
                         {2020, "01-01 04-10 04-13 05-08 05-25 08-31 12-25 12-28"},
                         {2021, "01-01 04-02 04-05 05-03 05-31 08-30 12-27 12-28"},
                         {2022, "01-03 04-15 04-18 05-02 06-02 06-03 08-29 09-19 12-26 12-27"},
                         {2023, "01-02 04-07 04-10 05-01 05-08 05-29 08-28 12-25 12-26"}};
    for (size_t i = 0; i < sizeof(bank_holidays) / sizeof(*bank_holidays) && ok; ++i) {
        string_t *sql = string_sprintf(
            "SELECT group_concat(day,' ') FROM "
            "(SELECT substr(calendar_date,6) AS day FROM calendar_local_days WHERE holiday_name IS NOT NULL "
            "AND calendar_date BETWEEN '%04d-01-01' AND '%04d-12-31' ORDER BY calendar_date)",
            bank_holidays[i].year, bank_holidays[i].year);
        ok = sql && test_calendar_query(db, string_c_str(sql), bank_holidays[i].dates);
        string_free(sql);
    }
    ok = ok &&
         sqlite_exec_cstr(db, "CREATE TEMP TABLE expected_days AS SELECT * FROM calendar_local_days;"
                              "CREATE TEMP TABLE expected_settings AS SELECT * FROM calendar_local_settings;"
                              "CREATE TEMP TABLE expected_patterns AS SELECT * FROM calendar_locale_date_pattern;") &&
         test_calendar_populate(db, path, key, "Quebc", "", false) &&
         test_calendar_populate(db, path, key, "Limmen", "Latin", false) &&
         sqlite_exec_cstr(db, "DELETE FROM calendar_locale_date_pattern WHERE locale='en_GB'");
    /* Validation failures must not retain even the new settings row. */
    ok = ok && test_calendar_populate(db, path, key, "Rhyl", "English", false) &&
         test_calendar_query(db,
                             "SELECT count(*) FROM (SELECT * FROM calendar_local_days EXCEPT "
                             "SELECT * FROM expected_days)",
                             "0") &&
         test_calendar_query(db,
                             "SELECT count(*) FROM (SELECT * FROM calendar_local_settings EXCEPT "
                             "SELECT * FROM expected_settings)",
                             "0") &&
         sqlite_exec_cstr(db, "INSERT OR REPLACE INTO calendar_locale_date_pattern SELECT * FROM expected_patterns;"
                              "DELETE FROM calendar_locale_date_pattern WHERE locale='fr_CA'") &&
         test_calendar_populate(db, path, key, "Quebec", "English", false) &&
         test_calendar_query(db,
                             "SELECT count(*) FROM (SELECT * FROM calendar_local_settings EXCEPT "
                             "SELECT * FROM expected_settings)",
                             "0") &&
         sqlite_exec_cstr(db, "INSERT OR REPLACE INTO calendar_locale_date_pattern SELECT * FROM expected_patterns;"
                              "DELETE FROM calendar_month_names WHERE name_set='cy' AND month_no=4") &&
         test_calendar_populate(db, path, key, "Rhyl", "Welsh", false) &&
         test_calendar_query(db,
                             "SELECT count(*) FROM (SELECT * FROM calendar_local_days EXCEPT "
                             "SELECT * FROM expected_days)",
                             "0") &&
         test_calendar_query(db,
                             "SELECT count(*) FROM (SELECT * FROM calendar_local_settings EXCEPT "
                             "SELECT * FROM expected_settings)",
                             "0");
    sqlite_close(db);
    string_free(path);
    string_free(key);
    return ok;
}

static bool test_calendar_offsets_fixture(const char *directory)
{
    (void)directory;
    static const struct {
        const char *zone;
        double january, july;
    } cases[] = {{"America/New_York", -5, -4},
                 {"America/Los_Angeles", -8, -7},
                 {"Europe/London", 0, 1},
                 {"Asia/Kathmandu", 5.75, 5.75},
                 {"Australia/Adelaide", 10.5, 9.5}};
    bool ok = !unsetenv("TZ");
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases) && ok; ++i) {
        string_t *name = string_new_with(cases[i].zone);
        cfg_calendar_environment_t scope = {0};
        double january = 0, july = 0;
        ok = name && cfg_calendar_zone_enter(name, &scope) && cfg_calendar_noon(2026, 1, 15, &january) &&
             cfg_calendar_noon(2026, 7, 15, &july) && january == cases[i].january && july == cases[i].july;
        ok = cfg_calendar_restore(&scope) && ok;
        tzset();
        ok = ok && getenv("TZ") == NULL;
        string_free(name);
    }
    const char *bad[] = {"No/Such_Zone", "../etc/passwd", "/etc/passwd", "UTC0", "right/UTC", "posix/UTC"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(*bad) && ok; ++i) {
        string_t *name = string_new_with(bad[i]);
        cfg_calendar_environment_t scope = {0};
        ok = name && !cfg_calendar_zone_enter(name, &scope) && !scope.changed && getenv("TZ") == NULL;
        string_free(name);
    }
    return ok;
}

static bool test_calendar_translated_snapshot(sqlite_t *db, const string_t *path, const string_t *key, const char *town,
                                              const char *locale, const char *date, const char *expected)
{
    string_t *request = string_new_with(town), *language = string_new_with(locale);
    string_t *empty = string_new();
    cfg_calendar_place_t place = {0};
    bool ok =
        request && language && empty && cfg_calendar_catalogue(db) &&
        cfg_calendar_choose_place(db, request, NULL, false, &place) &&
        cfg_calendar_snapshot(db, path, key, &place, empty) &&
        sqlite_exec_cstr(
            db,
            "DROP TABLE IF EXISTS temp.untranslated_days; DROP TABLE IF EXISTS temp.untranslated_events;"
            "CREATE TEMP TABLE untranslated_days AS SELECT calendar_date,is_weekend,utc_offset_hours "
            "FROM calendar_local_days; CREATE TEMP TABLE untranslated_events AS SELECT * FROM cfg_calendar_events;") &&
        cfg_calendar_snapshot(db, path, key, &place, language) &&
        test_calendar_query(db,
                            "SELECT count(*) FROM (SELECT calendar_date,is_weekend,utc_offset_hours "
                            "FROM calendar_local_days EXCEPT SELECT * FROM untranslated_days)",
                            "0") &&
        test_calendar_query(db,
                            "SELECT count(*) FROM (SELECT * FROM untranslated_days EXCEPT "
                            "SELECT calendar_date,is_weekend,utc_offset_hours FROM calendar_local_days)",
                            "0") &&
        test_calendar_query(db,
                            "SELECT count(*) FROM (SELECT DISTINCT date FROM untranslated_events EXCEPT "
                            "SELECT DISTINCT date FROM cfg_calendar_events)",
                            "0") &&
        test_calendar_query(db,
                            "SELECT count(*) FROM (SELECT DISTINCT date FROM cfg_calendar_events EXCEPT "
                            "SELECT DISTINCT date FROM untranslated_events)",
                            "0") &&
        test_calendar_query(db,
                            "SELECT count(*) FROM (SELECT date,coalesce(l.translated,e.name) "
                            "FROM untranslated_events e LEFT JOIN cfg_calendar_labels l ON l.source=e.name "
                            "EXCEPT SELECT * FROM cfg_calendar_events)",
                            "0") &&
        test_calendar_query(db,
                            "SELECT count(*) FROM (SELECT * FROM cfg_calendar_events EXCEPT "
                            "SELECT date,coalesce(l.translated,e.name) FROM untranslated_events e "
                            "LEFT JOIN cfg_calendar_labels l ON l.source=e.name)",
                            "0") &&
        test_calendar_query(db,
                            "SELECT count(*)=count(DISTINCT FullDateAlternateKey) "
                            "AND count(*)=count([Date UK]) AND count(*)=count([Date Lingua]) "
                            "AND count(*)=count([Date Regional]) FROM calendar_local",
                            "1");
    string_t *query =
        string_sprintf("SELECT [Public Holiday] FROM calendar_local WHERE FullDateAlternateKey='%s'", date);
    ok = ok && query && test_calendar_query(db, string_c_str(query), expected);
    string_free(query);
    cfg_calendar_place_clear(&place);
    string_free(empty);
    string_free(request);
    string_free(language);
    return ok;
}

static bool test_calendar_translation_snapshots_fixture(const char *directory)
{
    string_t *path = NULL, *key = NULL;
    sqlite_t *db = test_cfg_calendar_database(directory, &path, &key);
    bool ok = db != NULL;
    static const struct {
        const char *town, *locale, *date, *holiday;
    } cases[] = {{"Québec", "fr_CA", "2026-07-01", "Fête du Canada"},
                 {"Limmen", "fy_NL", "2026-12-25", "Earste Krystdei"},
                 {"Rhyl", "cy_GB", "2026-12-28", "Dydd San Steffan (diwrnod amgen)"},
                 {"Rhyl", "ga_GB", "2026-12-28", "Lá Fhéile Stiofáin (lá ionaid)"},
                 {"Kingston, JM", "jam", "2026-08-06", "Independence Deh"},
                 {"New York City", "es_US", "2026-07-04", "Día de la Independencia"},
                 {"New Orleans", "frc", "2026-07-04", "Fête de l'Indépendance"},
                 {"Philadelphia", "pdc", "2026-07-04", "Der Viert vun Tschulei"},
                 {"Anchorage", "ik", "2026-12-25", "Kuraisimaġvik"},
                 {"Anchorage", "ik", "2026-07-04", "Independence Day"},
                 {"Haifa", "he_IL", "2026-04-02", "פסח"},
                 {"Haifa", "ar_IL", "2026-04-02", "عيد الفصح اليهودي"},
                 {"Jerusalem", "yi", "2026-04-02", "פּסח"},
                 {"Jerusalem", "lad", "2026-04-02", "Pesah"},
                 {"New York City", "yi", "2026-07-04", "אומאָפּהענגיקייט־טאָג"},
                 {"New York City", "lad", "2026-07-04", "Diya de la independensya"}};
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases) && ok; ++i)
        ok = test_calendar_translated_snapshot(db, path, key, cases[i].town, cases[i].locale, cases[i].date,
                                               cases[i].holiday);
    /* Qualified events retain their dates and have exact, non-guessed translations. */
    ok = ok &&
         sqlite_exec_cstr(
             db, "INSERT INTO holiday_observance_rule(holiday_id,holiday_rule_id,observed_rule_kind,observed_name,"
                 "weekend_mask,suppress_original,valid_from_year,valid_to_year,priority) VALUES"
                 "(1005125,3279799,'previous_weekday','Independence Day (observed)','6,7','N',2026,2026,100)") &&
         test_calendar_translated_snapshot(db, path, key, "New York City", "es_US", "2026-07-03",
                                           "Día de la Independencia (día de observancia)");
    sqlite_close(db);
    string_free(path);
    string_free(key);
    return ok;
}

static bool test_calendar_seed_reload_fixture(const char *directory)
{
    string_t *path = NULL, *key = NULL;
    sqlite_t *db = test_cfg_calendar_database(directory, &path, &key);
    bool ok = db != NULL;
    const char *tables[] = {"holiday_name",           "holiday_event_localized_name",   "holiday_name_qualifier",
                            "calendar_town_language", "calendar_jurisdiction_language", "calendar_language_name"};
    for (size_t i = 0; i < sizeof(tables) / sizeof(*tables) && ok; ++i) {
        string_t *sql = string_sprintf("CREATE TEMP TABLE before_%s AS SELECT * FROM %s", tables[i], tables[i]);
        ok = sql && sqlite_exec(db, sql);
        string_free(sql);
    }
    const char *scripts[] = {"mars_calendar_locale_names.sql", "mars_holiday_localized_names.sql"};
    for (unsigned pass = 0; pass < 2 && ok; ++pass) {
        for (size_t i = 0; i < sizeof(scripts) / sizeof(*scripts) && ok; ++i) {
            string_t *source = string_sprintf("%s/packaging/jurisdiction-db/%s", MARS_CONFIG_ROOT_DIR, scripts[i]);
            file_t *file = source ? file_new(source) : NULL;
            string_t *sql = file ? file_read_all_text(file) : NULL;
            ok = sql && sqlite_exec(db, sql);
            string_free(sql);
            file_free(file);
            string_free(source);
        }
        for (size_t i = 0; i < sizeof(tables) / sizeof(*tables) && ok; ++i) {
            string_t *sql = string_sprintf("SELECT (SELECT count(*) FROM %s)=(SELECT count(*) FROM before_%s) "
                                           "AND NOT EXISTS(SELECT * FROM %s EXCEPT SELECT * FROM before_%s) "
                                           "AND NOT EXISTS(SELECT * FROM before_%s EXCEPT SELECT * FROM %s)",
                                           tables[i], tables[i], tables[i], tables[i], tables[i], tables[i]);
            ok = sql && test_calendar_query(db, string_c_str(sql), "1");
            string_free(sql);
        }
    }
    ok = ok && test_calendar_query(db, "SELECT count(*) FROM holiday_name WHERE locale='fy'", "14") &&
         test_calendar_query(db, "SELECT count(*) FROM holiday_name WHERE locale='ik'", "2") &&
         test_calendar_query(db, "SELECT count(*) FROM holiday_name WHERE locale='frc'", "12") &&
         test_calendar_query(db, "SELECT count(*) FROM holiday_name WHERE locale='pdc'", "12") &&
         test_calendar_query(db, "SELECT count(*) FROM holiday_name WHERE locale='jam'", "17") &&
         test_calendar_query(db, "SELECT count(*) FROM calendar_town_language", "7") &&
         test_calendar_query(
             db,
             "SELECT count(*) FROM calendar_jurisdiction_language c "
             "LEFT JOIN calendar_language_name n USING(language) LEFT JOIN calendar_locale_name_set s USING(locale) "
             "WHERE n.english_name IS NULL OR n.native_name IS NULL OR (c.locale IS NOT NULL AND s.month_set IS NULL)",
             "0") &&
         test_calendar_query(db,
                             "SELECT count(*) FROM (SELECT jurisdiction FROM calendar_jurisdiction_language "
                             "GROUP BY jurisdiction HAVING count(locale)=0)",
                             "0");
    sqlite_close(db);
    string_free(path);
    string_free(key);
    return ok;
}

static bool test_calendar_historical_fixture(const char *directory)
{
    string_t *path = NULL, *key = NULL;
    sqlite_t *db = test_cfg_calendar_database(directory, &path, &key);
    bool ok = db != NULL;
    static const struct {
        const char *jurisdiction, *locale, *date, *name;
        int year;
    } cases[] = {{"CA-QC", "fr", "1982-07-01", "Fête du Dominion", 1982},
                 {"CA-QC", "fr-CA", "1983-07-01", "Fête du Canada", 1983},
                 {"NL", "fy_NL", "1927-08-31", "Keninginnedei", 1927},
                 {"NL", "fy-NL", "1950-05-01", "Keninginnedei", 1950},
                 {"NL", "fy", "1989-04-29", "Keninginnedei", 1989},
                 {"NL", "fy_NL", "2025-04-26", "Keningsdei", 2025},
                 {"IE", "ga_IE", "1960-06-06", "Luan Cincíse", 1960},
                 {"US", "es", "1944-11-11", "Día del Armisticio", 1944}};
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases) && ok; ++i) {
        cfg_calendar_place_t place = {.jurisdiction = string_new_with(cases[i].jurisdiction)};
        bool original[7] = {0}, translated[7] = {0};
        ok = place.jurisdiction && test_calendar_labels(db, cases[i].jurisdiction, "") &&
             cfg_calendar_policy(db, path, key, &place, cases[i].year, cases[i].year, original) &&
             sqlite_exec_cstr(db, "DROP TABLE IF EXISTS temp.historical_source;"
                                  "CREATE TEMP TABLE historical_source AS SELECT * FROM cfg_calendar_events;") &&
             test_calendar_labels(db, cases[i].jurisdiction, cases[i].locale) &&
             cfg_calendar_policy(db, path, key, &place, cases[i].year, cases[i].year, translated) &&
             test_calendar_query(db, "SELECT count(*)>0 FROM cfg_calendar_events", "1") &&
             test_calendar_query(db,
                                 "SELECT count(*) FROM (SELECT date,coalesce(l.translated,s.name) AS name "
                                 "FROM historical_source s LEFT JOIN cfg_calendar_labels l ON l.source=s.name "
                                 "EXCEPT SELECT * FROM cfg_calendar_events)",
                                 "0") &&
             test_calendar_query(db,
                                 "SELECT count(*) FROM (SELECT * FROM cfg_calendar_events EXCEPT "
                                 "SELECT date,coalesce(l.translated,s.name) FROM historical_source s "
                                 "LEFT JOIN cfg_calendar_labels l ON l.source=s.name)",
                                 "0");
        for (size_t weekday = 0; weekday < 7 && ok; ++weekday)
            ok = original[weekday] == translated[weekday];
        string_t *sql = string_sprintf("SELECT name FROM cfg_calendar_events WHERE date='%s'", cases[i].date);
        ok = ok && sql && test_calendar_query(db, string_c_str(sql), cases[i].name);
        string_free(sql);
        cfg_calendar_place_clear(&place);
    }
    sqlite_close(db);
    string_free(path);
    string_free(key);
    return ok;
}

static bool test_calendar_readme_fixture(const char *directory)
{
    string_t *path = NULL, *key = NULL;
    sqlite_t *db = test_cfg_calendar_database(directory, &path, &key);
    string_t *guide_path = string_sprintf("%s/docs/jurisdiction.md", MARS_CONFIG_ROOT_DIR);
    file_t *file = guide_path ? file_new(guide_path) : NULL;
    string_t *guide = file ? file_read_all_text(file) : NULL;
    bool ok = db && guide;
    /* README examples: use the documented projections after actual native population. */
    static const struct {
        const char *town, *language, *query, *header, *output;
    } cases[] = {
        {"Québec", "French",
         "select [Date Lingua], [Date Regional], [Public Holiday]\n"
         "from calendar_local where FullDateAlternateKey = '2026-07-01';",
         "Date Lingua|Date Regional|Public Holiday", "1ᵉʳ juillet 2026|1ᵉʳ juillet 2026|Fête du Canada"},
        {"Limmen", "Western Frisian",
         "select [Date Lingua], [Date Regional], [Public Holiday]\n"
         "from calendar_local where FullDateAlternateKey = '2026-12-25';",
         "Date Lingua|Date Regional|Public Holiday", "25 Desimber 2026|25 december 2026|Earste Krystdei"},
        {"Rhyl", "Welsh",
         "select FullDateAlternateKey, [Date Lingua], [Public Holiday]\nfrom calendar_local\n"
         "where FullDateAlternateKey between '2026-12-25' and '2026-12-28'\n"
         "  and [Public Holiday] is not null\norder by FullDateAlternateKey;",
         "FullDateAlternateKey|Date Lingua|Public Holiday",
         "2026-12-25|25 Rhagfyr 2026|Dydd Nadolig\n2026-12-28|28 Rhagfyr 2026|Dydd San Steffan (diwrnod amgen)"},
        {"Rhyl", "Irish",
         "select FullDateAlternateKey, [Date Lingua], [Public Holiday]\nfrom calendar_local\n"
         "where FullDateAlternateKey between '2026-12-25' and '2026-12-28'\n"
         "  and [Public Holiday] is not null\norder by FullDateAlternateKey;",
         "FullDateAlternateKey|Date Lingua|Public Holiday",
         "2026-12-25|25 Nollaig 2026|Lá Nollag\n2026-12-28|28 Nollaig 2026|Lá Fhéile Stiofáin (lá ionaid)"},
        {"Kingston, JM", "Patwa",
         "select FullDateAlternateKey, [Date Lingua], [Day Name], [Public Holiday]\nfrom calendar_local\n"
         "where FullDateAlternateKey = '2026-08-06';",
         "FullDateAlternateKey|Date Lingua|Day Name|Public Holiday",
         "2026-08-06|6 Aagus 2026|Turzdeh|Independence Deh"},
        {"New York City", "Spanish",
         "select FullDateAlternateKey, [Date Lingua], [Public Holiday]\nfrom calendar_local\n"
         "where FullDateAlternateKey in ('2026-01-01', '2026-07-04', '2026-12-25')\n"
         "order by FullDateAlternateKey;",
         "FullDateAlternateKey|Date Lingua|Public Holiday",
         "2026-01-01|1 de enero de 2026|Día de Año Nuevo\n"
         "2026-07-04|4 de julio de 2026|Día de la Independencia\n2026-12-25|25 de diciembre de 2026|Día de Navidad"},
        {"Vatican City", "Latin",
         "select [Date Lingua], \"Day Name\", \"Day Name Abbrev\", \"Month Name\", \"Month Name Abbrev\", [Date UK]\n"
         "from calendar_local\nwhere FullDateAlternateKey = '2024-06-21';",
         "Date Lingua|Day Name|Day Name Abbrev|Month Name|Month Name Abbrev|Date UK",
         "21 Iunii 2024|Veneris|Ven|Iunius|Iun|21ˢᵗ June 2024"}};
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases) && ok; ++i) {
        ok = string_find(guide, cases[i].query) >= 0 && string_find(guide, cases[i].output) >= 0 &&
             test_calendar_populate(db, path, key, cases[i].town, cases[i].language, true);
        sqlite_stmt_t *stmt = ok ? sqlite_stmt_prepare(db, cases[i].query) : NULL;
        string_t *output = string_new();
        string_t *header = string_new_with(cases[i].header);
        size_t columns = 1;
        string_cursor_t *cursor = header ? string_cursor_new(header) : NULL;
        while (cursor && !string_cursor_done(cursor)) {
            columns += rune_value(string_cursor_peek(cursor)) == '|';
            string_cursor_next(cursor);
        }
        ok = ok && stmt && output && cursor;
        string_cursor_free(cursor);
        string_free(header);
        sqlite_step_result_t step = SQLITE_STEP_DONE;
        bool any = false;
        while (ok && (step = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
            if (any)
                ok = !string_append_char(output, '\n');
            for (size_t column = 0; column < columns && ok; ++column) {
                const char *value = sqlite_stmt_column_text(stmt, (int)column);
                if (column)
                    ok = !string_append_char(output, '|');
                ok = ok && value && !string_append_cstr(output, value);
            }
            any = true;
        }
        ok = ok && step == SQLITE_STEP_DONE && test_calendar_text(output, cases[i].output);
        if (ok)
            printf("%s\n%s\n", cases[i].header, string_c_str(output));
        else
            fprintf(stderr, "README calendar snapshot failed: %s / %s\n", cases[i].town, cases[i].language);
        string_free(output);
        sqlite_stmt_finalize(stmt);
    }
    string_free(guide);
    string_free(guide_path);
    file_free(file);
    sqlite_close(db);
    string_free(path);
    string_free(key);
    return ok;
}

static void test_calendar_catalogue(void)
{
    TEST_ASSERT_TRUE(test_cfg_calendar_isolated(test_calendar_catalogue_fixture),
                     "indexed town resolution and precedence");
}

static void test_calendar_unicode_keys(void)
{
    static const struct {
        const char *input, *expected;
    } cases[] = {{"Québec", "quebec"},  {"QUÉBEC", "quebec"},           {"  Québec\t City  ", "quebec city"},
                 {"Straße", "strasse"}, {"Ångström", "angstrom"},       {"Ångström", "angstrom"},
                 {"ייִדיש", "יידיש"},    {"👩‍💻", "👩‍💻"}, {"क्ष", "कष"}};
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        string_t *text = string_new_with(cases[i].input);
        string_t *key = text ? cfg_calendar_key(text, false) : NULL;
#ifdef HAVE_UNISTRING
        bool ok = text && test_calendar_text(key, cases[i].expected);
#else
        bool ok = text && !key;
#endif
        string_free(text);
        string_free(key);
        TEST_ASSERT_TRUE(ok, "scalar normalisation strips accents without losing bases or ZWJ");
    }
}

static void test_calendar_menus(void)
{
    TEST_ASSERT_TRUE(test_cfg_calendar_isolated(test_calendar_menu_fixture),
                     "paginated explicit menus, invalid choices and EOF");
}

static void test_calendar_languages(void)
{
    TEST_ASSERT_TRUE(test_cfg_calendar_isolated(test_calendar_languages_fixture),
                     "language aliases, eligibility and defaults");
}

static void test_calendar_translations(void)
{
    TEST_ASSERT_TRUE(test_cfg_calendar_isolated(test_calendar_labels_fixture),
                     "translation inheritance and exact precedence");
}

static void test_calendar_snapshot(void)
{
    TEST_ASSERT_TRUE(test_cfg_calendar_isolated(test_calendar_snapshot_fixture),
                     "native policies, restored state and rollback");
}

static void test_calendar_offsets(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_calendar_offsets_fixture), "validated zoneinfo and local-noon offsets");
}

static void test_calendar_translation_snapshots(void)
{
    TEST_ASSERT_TRUE(test_cfg_calendar_isolated(test_calendar_translation_snapshots_fixture),
                     "translated native snapshots retain every policy date, weekend and offset");
}

static void test_calendar_readme(void)
{
    TEST_ASSERT_TRUE(test_cfg_calendar_isolated(test_calendar_readme_fixture), "README translated calendar snapshots");
}

static void test_calendar_seed_reload(void)
{
    TEST_ASSERT_TRUE(test_cfg_calendar_isolated(test_calendar_seed_reload_fixture),
                     "holiday translations and language eligibility seeds reload without changing rows or identifiers");
}

static void test_calendar_historical(void)
{
    TEST_ASSERT_TRUE(test_cfg_calendar_isolated(test_calendar_historical_fixture),
                     "historical names and observances retain native dates and weekend policy");
}

/* Register ordinary cases only; documented examples have their own final group. */
void test_cfg_calendar_cases(void)
{
    TEST_RUN_IN_GROUP(test_calendar_unicode_keys, tests, NULL);
    TEST_RUN_IN_GROUP(test_calendar_offsets, tests, NULL);
#ifndef HAVE_UNISTRING
    return;
#endif
    TEST_RUN_IN_GROUP(test_calendar_catalogue, tests, NULL);
    TEST_RUN_IN_GROUP(test_calendar_menus, tests, NULL);
    TEST_RUN_IN_GROUP(test_calendar_languages, tests, NULL);
    TEST_RUN_IN_GROUP(test_calendar_translations, tests, NULL);
    TEST_RUN_IN_GROUP(test_calendar_snapshot, tests, NULL);
    TEST_RUN_IN_GROUP(test_calendar_translation_snapshots, tests, NULL);
    TEST_RUN_IN_GROUP(test_calendar_seed_reload, tests, NULL);
    TEST_RUN_IN_GROUP(test_calendar_historical, tests, NULL);
}

/* The parent calls this only after all ordinary configuration and rendering cases. */
void test_cfg_calendar_readme_cases(void)
{
#ifndef HAVE_UNISTRING
    return;
#endif
    TEST_RUN_IN_GROUP(test_calendar_readme, readme_examples, NULL);
}
