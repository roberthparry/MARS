/**
 * @file test_cfg_rendering.c
 * @brief Independent SQL calendar rendering and field regressions for native configuration tests.
 *
 * Exercises locale rendering and the pure calendar_local projections independently.
 * Disposable encrypted databases load the real
 * locale and view SQL through the public file and sqlite APIs. Expected labels are
 * fixed references, including a pinned Babel/CLDR fixture; Gregorian arithmetic and
 * civil Hijri expectations are independent of the SQL view. Optional ICU C calls
 * retain the long Hebrew and Hijri oracle comparisons without runtime Python.
 * The original ordered locale-data SHA-256 snapshot uses test-only libsodium;
 * Catalogue coverage loads only the schema and location-data prefix, excluding
 * holiday-rule history, with the original all-town locale coverage assertion.
 *
 * Supplied holiday/weekend/offset rows are fixtures, not native jurisdiction policy
 * tests. Menus, installation and holiday translation belong to other test helpers.
 * README cases have a separate registration so the parent can run them last.
 */
#include <dlfcn.h>
#include <math.h>
#include <sodium.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "datetime.h"
#include "file.h"
#include "sqlite.h"
#include "test_cfg_support.h"
#include "test_harness.h"

typedef struct {
    int year, month, day;
} civil_date_t;

static int test_cfg_rendering_append_literal(string_t *out, const char *text)
{
    return string_append_utf8_exact(out, text, strlen(text));
}

static int test_cfg_rendering_append_format(string_t *out, const char *format, ...)
{
    /* Normalise only the bounded row/fragment, never the accumulated expected result. */
    va_list args;
    va_start(args, format);
    string_t *fragment = string_vsprintf(format, args);
    va_end(args);
    int status = fragment ? string_append_utf8_exact(out, string_c_str(fragment), string_byte_length(fragment)) : -1;
    string_free(fragment);
    return status; /* Zero on success, negative on failure; no caller needs a character count. */
}

static const char *const english_months[] = {"January", "February", "March",     "April",   "May",      "June",
                                             "July",    "August",   "September", "October", "November", "December"};

static const char *const hijri_months[] = {"محرم", "صفر",   "ربيع الأول", "ربيع الآخر", "جمادى الأولى", "جمادى الآخرة",
                                           "رجب",  "شعبان", "رمضان",      "شوال",       "ذو القعدة",    "ذو الحجة"};

static bool test_cfg_rendering_leap_year(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static int test_cfg_rendering_month_length(civil_date_t date)
{
    static const int lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return lengths[date.month - 1] + (date.month == 2 && test_cfg_rendering_leap_year(date.year));
}

static int test_cfg_rendering_civil_index(civil_date_t date)
{
    static const int preceding[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int years = date.year - 1;
    return years * 365 + years / 4 - years / 100 + years / 400 + preceding[date.month - 1] + date.day - 1 +
           (date.month > 2 && test_cfg_rendering_leap_year(date.year));
}

static civil_date_t test_cfg_rendering_move_day(civil_date_t date, int distance)
{
    /* Calendar iteration or at most one week of displacement; no searching for keyed data. */
    while (distance > 0) {
        if (++date.day > test_cfg_rendering_month_length(date)) {
            date.day = 1;
            if (++date.month > 12) {
                date.month = 1;
                ++date.year;
            }
        }
        --distance;
    }
    while (distance < 0) {
        if (--date.day < 1) {
            if (--date.month < 1) {
                date.month = 12;
                --date.year;
            }
            date.day = test_cfg_rendering_month_length(date);
        }
        ++distance;
    }
    return date;
}

static string_t *test_cfg_rendering_date_text(civil_date_t date)
{
    return string_sprintf("%04d-%02d-%02d", date.year, date.month, date.day);
}

static const char *test_cfg_rendering_ordinal_suffix(int day)
{
    static const char *const endings[] = {"ᵗʰ", "ˢᵗ", "ⁿᵈ", "ʳᵈ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ"};
    return day >= 11 && day <= 13 ? "ᵗʰ" : endings[day % 10];
}

static string_t *test_cfg_rendering_uk_text(civil_date_t date)
{
    return string_sprintf("%d%s %s %04d", date.day, test_cfg_rendering_ordinal_suffix(date.day),
                          english_months[date.month - 1], date.year);
}

static bool test_cfg_rendering_sql_ok(sqlite_t *db, const char *sql)
{
    bool ok = sqlite_exec_cstr(db, sql);
    if (!ok) {
        const string_t *error = sqlite_last_error(db);
        string_fprintf(stderr, "calendar rendering SQL failed: %S\n", error);
    }
    return ok;
}

static bool test_cfg_rendering_load_sql(sqlite_t *db, const char *path)
{
    string_t *absolute = string_sprintf("%s/%s", MARS_CONFIG_ROOT_DIR, path);
    file_t *file = absolute ? file_new(absolute) : NULL;
    string_free(absolute);
    string_t *text = file ? file_read_all_text(file) : NULL;
    bool ok = text && sqlite_exec(db, text);
    if (!ok)
        string_fprintf(stderr, "calendar rendering could not load %s: %S\n", path, sqlite_last_error(db));
    string_free(text);
    file_free(file);
    return ok;
}

static sqlite_t *test_cfg_rendering_database(const char *directory)
{
    string_t *path = string_sprintf("%s/rendering.db", directory);
    string_t *key = string_new_with("calendar-rendering-regression-only");
    sqlite_t *db = path && key ? sqlite_open_encrypted(path, key) : NULL;
    string_free(path);
    string_free(key);
    bool ok = db && test_cfg_rendering_load_sql(db, "packaging/jurisdiction-db/mars_calendar_locale_names.sql") &&
              test_cfg_rendering_load_sql(db, "packaging/jurisdiction-db/mars_calendar_local.sql") &&
              test_cfg_rendering_sql_ok(db, "INSERT INTO calendar_local_settings VALUES"
                                            "(1,'Test','GB-ENG','Europe/London',52,-2,2024,2025,'en_GB')");
    if (!ok) {
        sqlite_close(db);
        db = NULL;
    }
    return db;
}

static bool test_cfg_rendering_settings(sqlite_t *db, const char *jurisdiction, const char *locale)
{
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db, "UPDATE calendar_local_settings SET jurisdiction=?1,locale=?2");
    bool ok = stmt && sqlite_stmt_bind_text(stmt, 1, jurisdiction) &&
              (locale ? sqlite_stmt_bind_text(stmt, 2, locale) : sqlite_stmt_bind_null(stmt, 2)) &&
              sqlite_stmt_step(stmt) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(stmt);
    return ok;
}

static bool test_cfg_rendering_column_equals(sqlite_stmt_t *stmt, int column, const string_t *expected)
{
    const char *actual = sqlite_stmt_column_text(stmt, column);
    bool ok = expected && actual && string_view_equals_literal(string_view_all(expected), actual);
    if (!ok)
        string_fprintf(stderr, "calendar column %d: expected <%S>, got <%s>\n", column, expected,
                       actual ? actual : "NULL");
    return ok;
}

static bool test_cfg_rendering_expect_rows(sqlite_t *db, const char *sql, const string_t *expected)
{
    size_t row_count = 0;
    string_t **rows = expected ? string_split(expected, "\n", &row_count) : NULL;
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db, sql);
    bool ok = expected && stmt && (!string_byte_length(expected) || rows);
    for (size_t row = 0; ok && row < row_count; ++row) {
        size_t columns = 0;
        string_t **fields = string_split(rows[row], "|", &columns);
        ok = fields && sqlite_stmt_step(stmt) == SQLITE_STEP_ROW;
        for (size_t column = 0; ok && column < columns; ++column)
            ok = string_view_equals_literal(string_view_all(fields[column]), "<NULL>")
                     ? sqlite_stmt_column_is_null(stmt, (int)column)
                     : test_cfg_rendering_column_equals(stmt, (int)column, fields[column]);
        string_split_free(fields, columns);
    }
    ok = ok && sqlite_stmt_step(stmt) == SQLITE_STEP_DONE;
    if (!ok)
        string_fprintf(stderr, "calendar result mismatch for %s: %S\n", sql, sqlite_last_error(db));
    sqlite_stmt_finalize(stmt);
    string_split_free(rows, row_count);
    return ok;
}

static bool test_cfg_rendering_expect_literal(sqlite_t *db, const char *sql, const char *expected)
{
    string_t *text = string_new_with(expected);
    bool ok = text && test_cfg_rendering_expect_rows(db, sql, text);
    string_free(text);
    return ok;
}

static bool test_cfg_rendering_add_day(sqlite_stmt_t *stmt, civil_date_t date, const char *holiday, int weekend,
                                       int offset)
{
    string_t *text = test_cfg_rendering_date_text(date);
    sqlite_stmt_reset(stmt);
    sqlite_stmt_clear_bindings(stmt);
    bool ok = text && sqlite_stmt_bind_text(stmt, 1, string_c_str(text)) &&
              (holiday ? sqlite_stmt_bind_text(stmt, 2, holiday) : sqlite_stmt_bind_null(stmt, 2)) &&
              sqlite_stmt_bind_int(stmt, 3, weekend) && sqlite_stmt_bind_int(stmt, 4, offset) &&
              sqlite_stmt_step(stmt) == SQLITE_STEP_DONE;
    string_free(text);
    return ok;
}

static bool test_cfg_rendering_fill_range(sqlite_t *db, civil_date_t first, civil_date_t last, bool policy)
{
    bool ok = test_cfg_rendering_sql_ok(db, "DELETE FROM calendar_local_days; BEGIN");
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db, "INSERT INTO calendar_local_days VALUES(?1,?2,?3,?4)");
    for (civil_date_t date = first; ok && test_cfg_rendering_civil_index(date) <= test_cfg_rendering_civil_index(last);
         date = test_cfg_rendering_move_day(date, 1))
        ok = stmt && test_cfg_rendering_add_day(stmt, date, policy && date.day == 1 ? "Fixture holiday" : NULL,
                                                policy && test_cfg_rendering_civil_index(date) % 7 >= 5, 0);
    sqlite_stmt_finalize(stmt);
    return test_cfg_rendering_sql_ok(db, ok ? "COMMIT" : "ROLLBACK") && ok;
}

static bool test_cfg_rendering_sample_dates(sqlite_t *db)
{
    return test_cfg_rendering_sql_ok(db, "DELETE FROM calendar_local_days; INSERT INTO calendar_local_days VALUES"
                                         "('2024-01-01',NULL,0,0),('2024-02-29',NULL,0,0),('2024-06-21',NULL,0,0),"
                                         "('2024-07-07',NULL,0,0),('2024-12-31',NULL,0,0),('2025-01-01',NULL,0,0)");
}

static bool test_cfg_rendering_uk_ordinals(const char *directory)
{
    sqlite_t *db = test_cfg_rendering_database(directory);
    bool ok = db && test_cfg_rendering_fill_range(db, (civil_date_t){2026, 1, 1}, (civil_date_t){2026, 1, 31}, false);
    string_t *expected = string_new();
    for (int day = 1; ok && day <= 31; ++day)
        ok = expected &&
             test_cfg_rendering_append_format(expected, "%d%s January 2026|%d%s January 2026|%d January 2026\n", day,
                                              test_cfg_rendering_ordinal_suffix(day), day,
                                              test_cfg_rendering_ordinal_suffix(day), day) >= 0;
    const char *jurisdictions[] = {"GB-ENG", "GB-WLS", "GB-SCT", "GB-NIR", "GB"};
    for (size_t i = 0; ok && i < sizeof jurisdictions / sizeof *jurisdictions; ++i)
        for (int explicit_locale = 0; ok && explicit_locale < 2; ++explicit_locale)
            ok = test_cfg_rendering_settings(db, jurisdictions[i], explicit_locale ? "en_GB" : NULL) &&
                 test_cfg_rendering_expect_rows(db,
                                                "SELECT [Date Lingua],[Date UK],[Date Regional] FROM calendar_local "
                                                "ORDER BY FullDateAlternateKey",
                                                expected);
    string_free(expected);
    ok = ok && test_cfg_rendering_sample_dates(db);
    const char *other[] = {"cy_GB", "ga_GB", "gd_GB", "kw_GB", "jam", "fr_CA"};
    for (size_t i = 0; ok && i < sizeof other / sizeof *other; ++i) {
        ok = test_cfg_rendering_settings(db, "NL", other[i]) &&
             test_cfg_rendering_sql_ok(db, "DROP TABLE IF EXISTS reference; CREATE TEMP TABLE reference AS "
                                           "SELECT FullDateAlternateKey,[Date Lingua] AS lingua FROM calendar_local") &&
             test_cfg_rendering_settings(db, "GB-WLS", other[i]) &&
             test_cfg_rendering_expect_literal(
                 db,
                 "SELECT count(*) FROM calendar_local JOIN reference USING(FullDateAlternateKey) "
                 "WHERE [Date Lingua] IS NOT lingua",
                 "0");
    }
    const struct {
        const char *jurisdiction, *locale, *date;
    } overseas[] = {{"IE", "en_GB", "1 January 2024"},     {"NL", "en_GB", "1 January 2024"},
                    {"US-NY", "en_US", "January 1, 2024"}, {"CA-QC", "en_CA", "January 1, 2024"},
                    {"IM", "en_GB", "1 January 2024"},     {"JE", "en_GB", "1 January 2024"}};
    for (size_t i = 0; ok && i < sizeof overseas / sizeof *overseas; ++i)
        ok = test_cfg_rendering_settings(db, overseas[i].jurisdiction, overseas[i].locale) &&
             test_cfg_rendering_expect_literal(
                 db, "SELECT [Date Lingua] FROM calendar_local WHERE FullDateAlternateKey='2024-01-01'",
                 overseas[i].date);
    sqlite_close(db);
    return ok;
}

static bool test_cfg_rendering_french_ordinals(const char *directory)
{
    sqlite_t *db = test_cfg_rendering_database(directory);
    bool ok = db && test_cfg_rendering_fill_range(db, (civil_date_t){2024, 1, 1}, (civil_date_t){2024, 12, 31}, false);
    const char *months[] = {"janvier", "février", "mars",      "avril",   "mai",      "juin",
                            "juillet", "août",    "septembre", "octobre", "novembre", "décembre"};
    string_t *expected = string_new();
    for (civil_date_t date = {2024, 1, 1}; ok && date.year == 2024; date = test_cfg_rendering_move_day(date, 1))
        ok = expected && test_cfg_rendering_append_format(expected, "%d%s %s 2024|%d%s %s 2024\n", date.day,
                                                          date.day == 1 ? "ᵉʳ" : "", months[date.month - 1], date.day,
                                                          date.day == 1 ? "ᵉʳ" : "", months[date.month - 1]) >= 0;
    sqlite_stmt_t *locales = ok ? sqlite_stmt_prepare(db, "SELECT locale FROM calendar_locale_date_pattern "
                                                          "WHERE locale='fr' OR locale GLOB 'fr_*' ORDER BY locale")
                                : NULL;
    size_t count = 0;
    sqlite_step_result_t step = SQLITE_STEP_ERROR;
    while (ok && locales && (step = sqlite_stmt_step(locales)) == SQLITE_STEP_ROW) {
        ++count;
        ok =
            test_cfg_rendering_settings(db, "CA-QC", sqlite_stmt_column_text(locales, 0)) &&
            test_cfg_rendering_expect_rows(
                db, "SELECT [Date Lingua],[Date Regional] FROM calendar_local ORDER BY FullDateAlternateKey", expected);
    }
    ok = ok && count && step == SQLITE_STEP_DONE && test_cfg_rendering_settings(db, "CA-QC", "en_CA") &&
         test_cfg_rendering_expect_literal(
             db, "SELECT [Date Lingua],[Date Regional] FROM calendar_local WHERE FullDateAlternateKey='2024-01-01'",
             "January 1, 2024|1ᵉʳ janvier 2024");
    sqlite_stmt_finalize(locales);
    string_free(expected);
    sqlite_close(db);
    return ok;
}

static bool test_cfg_rendering_regional_and_grammar(const char *directory)
{
    sqlite_t *db = test_cfg_rendering_database(directory);
    bool ok = db && test_cfg_rendering_sample_dates(db) &&
              test_cfg_rendering_sql_ok(db, "INSERT INTO calendar_local_days VALUES('2026-07-04',NULL,1,0)");
    const struct {
        const char *jurisdiction, *locale, *regional, *lingua;
    } regional[] = {{"US-NY", "yi", "July 4, 2026", "4טן יולי 2026"},
                    {"US-NY", "lad", "July 4, 2026", "4 de djulyo de 2026"},
                    {"US-NY", "es_US", "July 4, 2026", "4 de julio de 2026"},
                    {"CA-QC", "en_CA", "4 juillet 2026", "July 4, 2026"},
                    {"NL", "en_GB", "4 juli 2026", "4 July 2026"},
                    {"IL", "yi", "4 ביולי 2026", "4טן יולי 2026"},
                    {"PS", "lad", "٤ تموز ٢٠٢٦", "4 de djulyo de 2026"}};
    for (size_t i = 0; ok && i < sizeof regional / sizeof *regional; ++i) {
        string_t *expected = string_sprintf("%s|4ᵗʰ July 2026|%s", regional[i].regional, regional[i].lingua);
        ok = expected && test_cfg_rendering_settings(db, regional[i].jurisdiction, regional[i].locale) &&
             test_cfg_rendering_expect_rows(db,
                                            "SELECT [Date Regional],[Date UK],[Date Lingua] FROM calendar_local "
                                            "WHERE FullDateAlternateKey='2026-07-04'",
                                            expected);
        string_free(expected);
    }
    ok = ok && test_cfg_rendering_settings(db, "US-NY", NULL) &&
         test_cfg_rendering_expect_literal(
             db, "SELECT [Date Regional]=[Date Lingua] FROM calendar_local WHERE FullDateAlternateKey='2026-07-04'",
             "1");
    const struct {
        const char *locale, *date, *month;
    } grammar[] = {{"nl_NL", "21 juni 2024", "juni"},          {"fr_CA", "21 juin 2024", "juin"},
                   {"en_US", "June 21ˢᵗ, 2024", "June"},       {"en_GB", "21ˢᵗ June 2024", "June"},
                   {"de_DE", "21. Juni 2024", "Juni"},         {"ja_JP", "2024年6月21日", "6月"},
                   {"la_VA", "21 Iunii 2024", "Iunius"},       {"ru_RU", "21 июня 2024\u202fг.", "июнь"},
                   {"fi_FI", "21. kesäkuuta 2024", "kesäkuu"}, {"ar_EG", "١٤ ذو الحجة ١٤٤٥", "يونيو"},
                   {"th_TH", "21 มิถุนายน ค.ศ. 2024", "มิถุนายน"}};
    for (size_t i = 0; ok && i < sizeof grammar / sizeof *grammar; ++i) {
        string_t *expected = string_sprintf("%s|%s|21ˢᵗ June 2024", grammar[i].date, grammar[i].month);
        ok = expected && test_cfg_rendering_settings(db, "GB-ENG", grammar[i].locale) &&
             test_cfg_rendering_expect_rows(db,
                                            "SELECT [Date Lingua],[Month Name],[Date UK] FROM calendar_local WHERE "
                                            "FullDateAlternateKey='2024-06-21'",
                                            expected);
        string_free(expected);
    }
    ok = ok && test_cfg_rendering_settings(db, "GB-ENG", "la_VA") &&
         test_cfg_rendering_expect_literal(
             db,
             "SELECT [Day Name],[Month Name Abbrev],[Date UK],[Public Holiday] FROM calendar_local "
             "WHERE FullDateAlternateKey='2024-06-21'",
             "Veneris|Iun|21ˢᵗ June 2024|<NULL>");
    sqlite_close(db);
    return ok;
}

static bool test_cfg_rendering_supplemental_names(const char *directory)
{
    const struct {
        const char *locale, *jurisdiction, *prefix, *suffix, *months, *short_months, *days, *short_days;
        int day, weekday_month, weekday_first;
    } cases[] = {
        {"jam", "JM", "6 ", " 2026", "Januari|Febiweri|Maach|April|May|Juun|July|Aagus|Septemba|Aktoba|Novemba|Disemba",
         "Jan.|Feb.|Maa.|Apr.|May|Juun|July|Aag.|Sep.|Akt.|Nov.|Dis.",
         "Mondeh|Tuesdeh|Wenzdeh|Turzdeh|Frideh|Satdeh|Sundeh", "Mon.|Tue.|Wen.|Tur.|Fri.|Sat.|Sun.", 6, 8, 3},
        {"lad", "GB-ENG", "1 de ", " de 2026",
         "jenero|fevrero|marso|avril|mayo|djunyo|djulyo|agosto|septembre|oktovre|novembre|desembre",
         "jen.|fev.|mar.|avr.|may.|djun.|djul.|ago.|sep.|okt.|nov.|des.",
         "lunes|martes|mierkoles|djueves|viernes|shabat|alhad", "lun.|mar.|mie.|dju.|vie.|sha.|alh.", 1, 6, 15},
        {"frc", "US-LA", "4 de ", " 2026",
         "janvier|février|mars|avril|mai|juin|juillet|août|septembre|octobre|novembre|décembre",
         "janv.|févr.|mars|avr.|mai|juin|juil.|août|sept.|oct.|nov.|déc.",
         "lundi|mardi|mercredi|jeudi|vendredi|samedi|dimanche", "lun.|mar.|mer.|jeu.|ven.|sam.|dim.", 4, 6, 15},
        {"pdc", "US-LA", "4. ", " 2026",
         "Yenner|Hanning|Matz|Abrill|Moi|Tschunn|Tschulei|Aaguscht|September|Oktower|Nofember|Diesember",
         "Yen.|Han.|Matz|Abr.|Moi|Tschun.|Tschul.|Aag.|Sep.|Okt.|Nof.|Die.",
         "Mundaag|Dinschdaag|Mittwoch|Dunnerschdaag|Freidaag|Samschdaag|Sunndaag", "Mu.|Di.|Mi.|Du.|Fr.|Sa.|Su.", 4, 6,
         15},
        {"ik", "US-AK", "", " 4, 2026",
         "Siqiññaatchiaq|Siqiññaasugruk|Paniqsiqsiivik|Umiaqqavik|Suppivik|Iġñivik|"
         "Iñukkuksaivik|Amiġaiqsivik|Sikuaqtuġvik|Sikkuvik|Nippivik|Siqiñġiḷaq",
         "Siqiññaat.|Siqiññaas.|Pani.|Umia.|Supp.|Iġñ.|Iñuk.|Amiġ.|Siku.|Sikk.|Nipp.|Siqiñġi.",
         "Atautchiiġñiq|Aippiġñiq|Piŋatchiġñiq|Sisammiġñiq|Tallimmiġñiq|Itchaksriġñiq|Savaiññiq",
         "Ata.|Aip.|Piŋ.|Sis.|Tal.|Itc.|Sav.", 4, 6, 15}};
    sqlite_t *db = test_cfg_rendering_database(directory);
    bool ok = db != NULL;
    for (size_t i = 0; ok && i < sizeof cases / sizeof *cases; ++i) {
        const char *lists[] = {cases[i].months, cases[i].short_months, cases[i].days, cases[i].short_days};
        string_t **words[4] = {0};
        size_t counts[4] = {0};
        for (size_t j = 0; j < 4; ++j) {
            string_t *text = string_new_with(lists[j]);
            words[j] = text ? string_split(text, "|", &counts[j]) : NULL;
            ok = words[j] && counts[j] == (j < 2 ? 12u : 7u) && ok;
            string_free(text);
        }
        ok = ok && test_cfg_rendering_settings(db, cases[i].jurisdiction, cases[i].locale) &&
             test_cfg_rendering_sql_ok(db, "DELETE FROM calendar_local_days");
        sqlite_stmt_t *insert = sqlite_stmt_prepare(db, "INSERT INTO calendar_local_days VALUES(?1,?2,?3,?4)");
        string_t *expected = string_new();
        for (int month = 1; ok && month <= 12; ++month) {
            ok = insert && expected &&
                 test_cfg_rendering_add_day(insert, (civil_date_t){2026, month, cases[i].day}, NULL, 0, 0) &&
                 test_cfg_rendering_append_format(expected, "%s%S%s|%S|%S\n", cases[i].prefix, words[0][month - 1],
                                                  cases[i].suffix, words[0][month - 1], words[1][month - 1]) >= 0;
        }
        sqlite_stmt_finalize(insert);
        ok = ok &&
             test_cfg_rendering_expect_rows(db,
                                            "SELECT [Date Lingua],[Month Name],[Month Name Abbrev] FROM calendar_local "
                                            "ORDER BY FullDateAlternateKey",
                                            expected) &&
             test_cfg_rendering_fill_range(db, (civil_date_t){2026, cases[i].weekday_month, cases[i].weekday_first},
                                           (civil_date_t){2026, cases[i].weekday_month, cases[i].weekday_first + 6},
                                           false);
        if (expected)
            string_clear(expected);
        for (size_t day = 0; ok && day < 7; ++day)
            ok = test_cfg_rendering_append_format(expected, "%S|%S\n", words[2][day], words[3][day]) >= 0;
        ok = ok &&
             test_cfg_rendering_expect_rows(
                 db, "SELECT [Day Name],[Day Name Abbrev] FROM calendar_local ORDER BY FullDateAlternateKey", expected);
        string_free(expected);
        for (size_t j = 0; j < 4; ++j)
            string_split_free(words[j], counts[j]);
    }
    ok = ok && test_cfg_rendering_settings(db, "GB-ENG", "la_VA") &&
         test_cfg_rendering_fill_range(db, (civil_date_t){2027, 5, 16}, (civil_date_t){2027, 5, 22}, false) &&
         test_cfg_rendering_expect_literal(
             db,
             "SELECT [Date Lingua],[Day Name],[Day Name Abbrev],[Month Name] FROM calendar_local ORDER BY "
             "FullDateAlternateKey",
             "16 Maii 2027|Solis|Sol|Maius\n17 Maii 2027|Lunae|Lun|Maius\n18 Maii 2027|Martis|Mar|Maius\n"
             "19 Maii 2027|Mercurii|Mer|Maius\n20 Maii 2027|Iovis|Iov|Maius\n21 Maii 2027|Veneris|Ven|Maius\n"
             "22 Maii 2027|Saturni|Sat|Maius");
    sqlite_close(db);
    return ok;
}

static bool test_cfg_rendering_babel_reference(const char *directory)
{
    sqlite_t *db = test_cfg_rendering_database(directory);
    file_t *fixture = file_new_cstr(MARS_CONFIG_ROOT_DIR "/tools/mars_config/tests/fixtures/babel_2_17_0_calendar.tsv");
    bool ok = db && test_cfg_rendering_sample_dates(db) && fixture && file_open_text(fixture);
    sqlite_stmt_t *query = ok ? sqlite_stmt_prepare(db, "SELECT FullDateAlternateKey,[Date Lingua],[Month Name],[Month "
                                                        "Name Abbrev],[Day Name],[Day Name Abbrev] "
                                                        "FROM calendar_local ORDER BY FullDateAlternateKey")
                              : NULL;
    string_t *previous = NULL;
    size_t locales = 0, rows = 0;
    while (ok) {
        string_t *line = NULL;
        ok = query && file_read_line(fixture, &line);
        if (!ok || !line) {
            string_free(line);
            break;
        }
        if (string_starts_with(line, "#") || !string_byte_length(line)) {
            string_free(line);
            continue;
        }
        size_t count = 0;
        string_t **fields = string_split(line, "\t", &count);
        ok = fields && count == 7;
        if (ok && (!previous || string_compare(previous, fields[0]))) {
            if (previous)
                ok = rows == 6 && sqlite_stmt_step(query) == SQLITE_STEP_DONE;
            sqlite_stmt_reset(query);
            ok = ok && test_cfg_rendering_settings(db, "GB-ENG", string_c_str(fields[0]));
            string_free(previous);
            previous = string_new_with(string_c_str(fields[0]));
            ok = previous && ok;
            rows = 0;
            ++locales;
        }
        ok = ok && sqlite_stmt_step(query) == SQLITE_STEP_ROW;
        for (size_t column = 0; ok && column < 6; ++column)
            ok = test_cfg_rendering_column_equals(query, (int)column, fields[column + 1]);
        ++rows;
        if (!ok)
            string_fprintf(stderr, "Babel reference mismatch at %S\n", line);
        string_split_free(fields, count);
        string_free(line);
    }
    ok = ok && previous && rows == 6 && sqlite_stmt_step(query) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(query);
    string_t *expected = string_sprintf("%zu", locales);
    ok = ok && expected &&
         test_cfg_rendering_expect_rows(db,
                                        "SELECT count(*) FROM calendar_locale_name_set "
                                        "WHERE locale NOT IN ('frc','ik','jam','lad','pdc')",
                                        expected);
    string_free(expected);
    string_free(previous);
    file_free(fixture);
    sqlite_close(db);
    return ok;
}

static civil_date_t test_cfg_rendering_hijri_reference(civil_date_t date)
{
    int remaining = test_cfg_rendering_civil_index(date) - test_cfg_rendering_civil_index((civil_date_t){622, 7, 19});
    civil_date_t result = {remaining / 10631 * 30 + 1, 1, 1};
    remaining %= 10631;
    int year_days;
    while (remaining >= (year_days = 354 + ((11 * result.year + 14) % 30 < 11))) {
        remaining -= year_days;
        ++result.year;
    }
    while (result.month < 12 && remaining >= 30 - (result.month + 1) % 2) {
        remaining -= 30 - (result.month + 1) % 2;
        ++result.month;
    }
    result.day += remaining;
    return result;
}

static string_t *test_cfg_rendering_arabic_text(civil_date_t date)
{
    civil_date_t hijri = test_cfg_rendering_hijri_reference(date);
    string_t *latin_digits = string_sprintf("%d %s %d", hijri.day, hijri_months[hijri.month - 1], hijri.year);
    string_t *result = string_new();
    static const char *const digits[] = {"٠", "١", "٢", "٣", "٤", "٥", "٦", "٧", "٨", "٩"};
    string_view_t view = latin_digits ? string_view_all(latin_digits) : string_view_empty();
    string_pos_t at = 0, next;
    uint32_t value;
    bool ok = latin_digits && result;
    while (ok && string_view_peek_rune_value(view, at, &value, &next)) {
        const char *bytes = value >= '0' && value <= '9' ? digits[value - '0'] : string_c_str(latin_digits) + at;
        size_t length = value >= '0' && value <= '9' ? 2 : next - at;
        ok = string_append_utf8_exact(result, bytes, length) == 0;
        at = next;
    }
    ok = ok && at == string_view_length(view);
    string_free(latin_digits);
    if (!ok) {
        string_free(result);
        result = NULL;
    }
    return result;
}

static bool test_cfg_rendering_arabic_and_hebrew_boundaries(const char *directory)
{
    sqlite_t *db = test_cfg_rendering_database(directory);
    bool ok = db && test_cfg_rendering_fill_range(db, (civil_date_t){2026, 1, 10}, (civil_date_t){2026, 1, 19}, false);
    const char *locales[] = {"ar_IL", "ar_PS", "ar_EG", "ar_MA", "ar_DZ"};
    string_t *expected = string_new();
    unsigned digit_mask = 0;
    for (civil_date_t date = {2026, 1, 10}; ok && date.day <= 19; date = test_cfg_rendering_move_day(date, 1)) {
        string_t *arabic = test_cfg_rendering_arabic_text(date), *key = test_cfg_rendering_date_text(date),
                 *uk = test_cfg_rendering_uk_text(date);
        ok = expected && arabic && key && uk &&
             test_cfg_rendering_append_format(expected, "%S|%S|%S|2026|1|%d\n", key, arabic, uk, date.day) >= 0;
        string_free(uk);
        string_free(key);
        string_free(arabic);
    }
    for (size_t i = 0; ok && i < sizeof locales / sizeof *locales; ++i) {
        ok = test_cfg_rendering_settings(db, "GB-ENG", locales[i]) &&
             test_cfg_rendering_expect_rows(db,
                                            "SELECT FullDateAlternateKey,[Date Lingua],[Date UK],[Year],[Month],[Day] "
                                            "FROM calendar_local ORDER BY FullDateAlternateKey",
                                            expected);
        sqlite_stmt_t *query = sqlite_stmt_prepare(db, "SELECT [Date Lingua] FROM calendar_local");
        sqlite_step_result_t step = SQLITE_STEP_ERROR;
        digit_mask = 0;
        while (ok && query && (step = sqlite_stmt_step(query)) == SQLITE_STEP_ROW) {
            string_t *text = string_new_with(sqlite_stmt_column_text(query, 0));
            string_view_t view = text ? string_view_all(text) : string_view_empty();
            string_pos_t at = 0, next;
            uint32_t value;
            ok = text != NULL;
            while (ok && string_view_peek_rune_value(view, at, &value, &next)) {
                ok = value < '0' || value > '9';
                if (value >= 0x660 && value <= 0x669)
                    digit_mask |= 1u << (value - 0x660);
                at = next;
            }
            string_free(text);
        }
        ok = ok && step == SQLITE_STEP_DONE && digit_mask == 1023;
        sqlite_stmt_finalize(query);
    }
    string_free(expected);
    const struct {
        civil_date_t date;
        const char *text;
    } hebrew[] = {
        {{2005, 10, 3}, "29 באלול 5765"},  {{2005, 10, 4}, "1 בתשרי 5766"},  {{2024, 2, 10}, "1 באדר א׳ 5784"},
        {{2024, 3, 11}, "1 באדר ב׳ 5784"}, {{2024, 4, 23}, "15 בניסן 5784"}, {{2024, 10, 2}, "29 באלול 5784"},
        {{2024, 10, 3}, "1 בתשרי 5785"},   {{2025, 3, 1}, "1 באדר 5785"},    {{2026, 4, 2}, "15 בניסן 5786"},
        {{2028, 9, 20}, "29 באלול 5788"},  {{2028, 9, 21}, "1 בתשרי 5789"}};
    ok = ok && test_cfg_rendering_sql_ok(db, "DELETE FROM calendar_local_days");
    sqlite_stmt_t *insert = sqlite_stmt_prepare(db, "INSERT INTO calendar_local_days VALUES(?1,?2,?3,?4)");
    expected = string_new();
    for (size_t i = 0; ok && i < sizeof hebrew / sizeof *hebrew; ++i) {
        civil_date_t date = hebrew[i].date;
        string_t *key = test_cfg_rendering_date_text(date);
        ok = insert && expected && key && test_cfg_rendering_add_day(insert, date, NULL, 0, 0) &&
             test_cfg_rendering_append_format(expected, "%S|%s|%d|%d|%d\n", key, hebrew[i].text, date.year, date.month,
                                              date.day) >= 0;
        string_free(key);
    }
    sqlite_stmt_finalize(insert);
    const char *hebrew_locales[] = {"he", "he_IL"};
    for (size_t i = 0; ok && i < 2; ++i)
        ok = test_cfg_rendering_settings(db, "GB-ENG", hebrew_locales[i]) &&
             test_cfg_rendering_expect_rows(
                 db,
                 "SELECT FullDateAlternateKey,[Date Lingua],[Year],[Month],[Day] FROM calendar_local "
                 "ORDER BY FullDateAlternateKey",
                 expected) &&
             test_cfg_rendering_expect_literal(
                 db, "SELECT [Date UK] FROM calendar_local WHERE FullDateAlternateKey='2026-04-02'", "2ⁿᵈ April 2026");
    string_free(expected);
    sqlite_close(db);
    return ok;
}

static bool test_cfg_rendering_fields_fixture(const char *directory)
{
    time_t now = time(NULL);
    struct tm current;
    if (!gmtime_r(&now, &current))
        return false;
    civil_date_t first = {2015, 1, 1}, last = {current.tm_year + 1900 + 7, 12, 31};
    sqlite_t *db = test_cfg_rendering_database(directory);
    bool ok = db && test_cfg_rendering_fill_range(db, first, last, true);
    const char *query = "SELECT FullDateAlternateKey,[FullDate and End Time],[Year],[Month],[Day],"
                        "[Days in Month],[MC Dates],[ME Dates],[WC Dates],[WE Dates],[WEDates (Friday)],"
                        "[WE Dates plus End Time],[Day of Week],[Fiscal Year End],[Fiscal Month],[Fiscal Quarter No],"
                        "[Fiscal Qtr],[Fiscal Quarter Name],[Fiscal Years],[Date UK],[Date Lingua],[Workday Type],"
                        "[Working Day No],[Day Type],[Non working day Type],[Moon Phase %] "
                        "FROM calendar_local ORDER BY FullDateAlternateKey";
    string_t *expected = string_new();
    int total = 0;
    for (civil_date_t day = first; ok && test_cfg_rendering_civil_index(day) <= test_cfg_rendering_civil_index(last);
         day = test_cfg_rendering_move_day(day, 1)) {
        int weekday = test_cfg_rendering_civil_index(day) % 7, length = test_cfg_rendering_month_length(day),
            end_year = day.year + (day.month > 3);
        int fiscal_month = (day.month + 8) % 12 + 1, quarter = (fiscal_month - 1) / 3 + 1;
        bool work = weekday < 5 && day.day != 1;
        total += work;
        int moon_epoch = test_cfg_rendering_civil_index((civil_date_t){2000, 1, 6});
        double lunations = (test_cfg_rendering_civil_index(day) - moon_epoch) / 29.53;
        int moon = (int)(100 - fabs(lunations - floor(lunations) - 0.5) * 200 + 0.5);
        string_t *key = test_cfg_rendering_date_text(day), *uk = test_cfg_rendering_uk_text(day),
                 *monday = test_cfg_rendering_date_text(test_cfg_rendering_move_day(day, -weekday));
        string_t *sunday = test_cfg_rendering_date_text(test_cfg_rendering_move_day(day, 6 - weekday)),
                 *friday = test_cfg_rendering_date_text(test_cfg_rendering_move_day(day, 4 - weekday));
        ok = expected && key && uk && monday && sunday && friday && moon >= 0 && moon <= 100 &&
             test_cfg_rendering_append_format(
                 expected,
                 "%S|%S 23:59:59|%d|%d|%d|%d|%04d-%02d-01 00:00:00|%04d-%02d-%02d 23:59:59|"
                 "%S|%S|%S|%S 23:59:59|%d|%d|%d|%d|Qtr %d|%04d Q%d|%02d-%02d|%S|%S|%d|%d|%s|%s|%d\n",
                 key, key, day.year, day.month, day.day, length, day.year, day.month, day.year, day.month, length,
                 monday, sunday, friday, sunday, weekday, end_year, fiscal_month, quarter, quarter, end_year, quarter,
                 (end_year - 1) % 100, end_year % 100, uk, uk, work, total, work ? "WorkDay" : "NonWorkDay",
                 weekday >= 5   ? "Weekend"
                 : day.day == 1 ? "Public Holiday"
                                : "<NULL>",
                 moon) >= 0;
        string_free(key);
        string_free(uk);
        string_free(monday);
        string_free(sunday);
        string_free(friday);
    }
    ok = ok && test_cfg_rendering_expect_rows(db, query, expected) &&
         test_cfg_rendering_expect_literal(
             db,
             "SELECT [Day Name],[Month Name Abbrev],[ME Dates Text],[WE Dates Text] FROM calendar_local "
             "WHERE FullDateAlternateKey='2024-12-31'",
             "Tuesday|Dec|Dec 2024|05 Jan 2025") &&
         test_cfg_rendering_expect_literal(
             db, "SELECT count(*) FROM calendar_local WHERE typeof([Working Day No])!='integer'", "0");
    string_free(expected);
    /* Independently accumulate fixture inputs before applying caller filters and LIMIT. */
    const char *predicates[] = {"FullDateAlternateKey >= '2024-12-28'", "[Workday Type]=0"};
    for (size_t i = 0; ok && i < 2; ++i) {
        string_t *sql = string_sprintf(
            "SELECT count(*) FROM (SELECT FullDateAlternateKey,[Working Day No] AS n "
            "FROM calendar_local WHERE %s ORDER BY FullDateAlternateKey DESC LIMIT 10) AS v WHERE n != "
            "(SELECT count(*) FROM calendar_local_days AS d WHERE d.calendar_date<=v.FullDateAlternateKey "
            "AND d.is_weekend=0 AND d.holiday_name IS NULL)",
            predicates[i]);
        ok = sql && test_cfg_rendering_expect_literal(db, string_c_str(sql), "0");
        string_free(sql);
    }
    sqlite_close(db);
    return ok;
}

static bool test_cfg_rendering_working_fixture(const char *directory)
{
    sqlite_t *db = test_cfg_rendering_database(directory);
    bool ok =
        db && test_cfg_rendering_expect_literal(db, "SELECT * FROM calendar_local", "") &&
        test_cfg_rendering_sql_ok(
            db,
            "INSERT INTO calendar_local_days VALUES ('2025-01-01','Holiday',0,0),('2024-12-31',NULL,0,0),"
            "('2024-12-30','Holiday',0,0),('2024-12-29',NULL,0,0),('2024-12-28',NULL,1,0),('2024-12-27',NULL,1,0)") &&
        test_cfg_rendering_expect_literal(
            db, "SELECT [Working Day No],[Workday Type] FROM calendar_local ORDER BY FullDateAlternateKey",
            "0|0\n0|0\n1|1\n1|0\n2|1\n2|0") &&
        test_cfg_rendering_expect_literal(
            db, "SELECT [Working Day No] FROM calendar_local WHERE FullDateAlternateKey='2025-01-01'", "2") &&
        test_cfg_rendering_sql_ok(db, "DELETE FROM calendar_local_days WHERE calendar_date!='2024-12-29'") &&
        test_cfg_rendering_expect_literal(db, "SELECT [Working Day No],[Workday Type] FROM calendar_local", "1|1");
    sqlite_close(db);
    return ok;
}

static bool test_cfg_rendering_snapshot_string(string_t *out, const char *value)
{
    /* Match json.dumps(ensure_ascii=False), including its control-character escapes. */
    static const char *const controls[32] = {[8] = "\\b", [9] = "\\t", [10] = "\\n", [12] = "\\f", [13] = "\\r"};
    string_t *text = value ? string_new_with(value) : NULL;
    string_view_t view = text ? string_view_all(text) : string_view_empty();
    string_pos_t at = 0, next;
    uint32_t scalar;
    bool ok = text && test_cfg_rendering_append_literal(out, "\"") == 0;
    while (ok && string_view_peek_rune_value(view, at, &scalar, &next)) {
        if (scalar < 32)
            ok = controls[scalar] ? test_cfg_rendering_append_literal(out, controls[scalar]) == 0
                                  : test_cfg_rendering_append_format(out, "\\u%04x", (unsigned)scalar) >= 0;
        else if (scalar == '"' || scalar == '\\')
            ok = test_cfg_rendering_append_format(out, "\\%c", (int)scalar) >= 0;
        else
            ok = string_append_utf8_exact(out, string_c_str(text) + at, next - at) == 0;
        at = next;
    }
    ok = ok && at == string_view_length(view) && test_cfg_rendering_append_literal(out, "\"") == 0;
    string_free(text);
    return ok;
}

static bool test_cfg_rendering_seed_snapshot(sqlite_t *db)
{
    string_fprintf(stderr, "Calendar seed: checking original ordered locale-data SHA-256\n");
    const struct {
        const char *name, *columns, *filter;
        int count, integer_column;
    } tables[] = {{"calendar_territory_locale", "*", "length(territory)=2", 2, -1},
                  {"calendar_locale_month", "*",
                   "locale IN (SELECT locale FROM calendar_territory_locale "
                   "WHERE length(territory)=2) OR locale='en_GB'",
                   4, 1},
                  {"calendar_locale_weekday", "locale,weekday_no,day_name",
                   "locale IN "
                   "(SELECT locale FROM calendar_territory_locale WHERE length(territory)=2) OR locale='en_GB'",
                   3, 1}};
    string_t *snapshot = string_new_with("{");
    bool ok = snapshot != NULL;
    for (size_t table = 0; ok && table < sizeof tables / sizeof *tables; ++table) {
        string_t *sql = string_sprintf("SELECT %s FROM %s WHERE %s ORDER BY 1,2", tables[table].columns,
                                       tables[table].name, tables[table].filter);
        sqlite_stmt_t *stmt = sql ? sqlite_stmt_prepare(db, string_c_str(sql)) : NULL;
        ok = stmt && (!table || test_cfg_rendering_append_literal(snapshot, ", ") == 0) &&
             test_cfg_rendering_snapshot_string(snapshot, tables[table].name) &&
             test_cfg_rendering_append_literal(snapshot, ": [") == 0;
        size_t rows = 0;
        sqlite_step_result_t step = SQLITE_STEP_ERROR;
        while (ok && (step = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
            ok = (!rows || test_cfg_rendering_append_literal(snapshot, ", ") == 0) &&
                 test_cfg_rendering_append_literal(snapshot, "[") == 0;
            for (int column = 0; ok && column < tables[table].count; ++column) {
                ok = !column || test_cfg_rendering_append_literal(snapshot, ", ") == 0;
                if (ok)
                    ok = column == tables[table].integer_column
                             ? test_cfg_rendering_append_format(snapshot, "%d", sqlite_stmt_column_int(stmt, column)) >=
                                   0
                             : test_cfg_rendering_snapshot_string(snapshot, sqlite_stmt_column_text(stmt, column));
            }
            ok = ok && test_cfg_rendering_append_literal(snapshot, "]") == 0;
            ++rows;
        }
        ok = ok && rows && step == SQLITE_STEP_DONE && test_cfg_rendering_append_literal(snapshot, "]") == 0;
        sqlite_stmt_finalize(stmt);
        string_free(sql);
    }
    unsigned char digest[crypto_hash_sha256_BYTES];
    char hex[crypto_hash_sha256_BYTES * 2 + 1];
    ok = ok && test_cfg_rendering_append_literal(snapshot, "}") == 0 && sodium_init() >= 0 &&
         crypto_hash_sha256(digest, (const unsigned char *)string_c_str(snapshot), string_byte_length(snapshot)) == 0;
    if (ok) {
        sodium_bin2hex(hex, sizeof hex, digest, sizeof digest);
        string_t *actual = string_new_with(hex);
        /* Original Python assertion: semantic seed snapshot, not a newly blessed file checksum. */
        ok = actual && string_view_equals_literal(string_view_all(actual),
                                                  "96648e3c749f92cb73f50e809ceba5b3e4730671230074f137c9a1e42121645f");
        if (!ok)
            string_fprintf(stderr, "Original calendar seed SHA-256 snapshot changed: %s\n", hex);
        string_free(actual);
    }
    if (ok)
        string_fprintf(stderr, "Calendar seed: original SHA-256 verified\n");
    else
        string_fprintf(stderr, "Calendar seed: SHA-256 check failed (serialization/query/hash): %S\n",
                       sqlite_last_error(db));
    string_free(snapshot);
    return ok;
}

static bool test_cfg_rendering_catalogue_sql(sqlite_t *db)
{
    /* Read the real schema/location prefix only. Fail closed if its include order changes. */
    static const char *const includes[] = {"packaging/jurisdiction-db/mars_country_jurisdictions.sql",
                                           "packaging/jurisdiction-db/mars_target_subdivisions.sql",
                                           "packaging/jurisdiction-db/mars_timezone_rules.sql",
                                           "packaging/jurisdiction-db/mars_jurisdiction_location_defaults.sql",
                                           "packaging/jurisdiction-db/mars_jurisdiction_towns.sql"};
    file_t *file = file_new_cstr(MARS_CONFIG_ROOT_DIR "/packaging/jurisdiction-db/mars_holiday_rules.sql");
    string_t *sql = string_new();
    bool ok = file && sql && file_open_text(file);
    bool transaction = ok && test_cfg_rendering_sql_ok(db, "PRAGMA foreign_keys=ON; BEGIN");
    ok = ok && transaction;
    size_t imported = 0;
    string_fprintf(stderr, "Calendar seed: importing schema and five location seeds (no holiday-rule history)\n");
    while (ok && imported < sizeof includes / sizeof *includes) {
        string_t *line = NULL;
        ok = file_read_line(file, &line);
        if (!ok || !line) {
            string_free(line);
            break;
        }
        if (string_starts_with(line, ".read ")) {
            string_t *include = string_substr(line, 6, string_byte_length(line) - 6);
            if (include)
                string_trim(include);
            ok = include && string_view_equals_literal(string_view_all(include), includes[imported]);
            if (!ok)
                string_fprintf(stderr, "Calendar seed: expected include %s, found %S\n", includes[imported], include);
            if (ok) {
                string_fprintf(stderr, "Calendar seed: schema prefix then %s\n", includes[imported]);
                ok = sqlite_exec(db, sql) && test_cfg_rendering_load_sql(db, includes[imported]);
            }
            string_clear(sql);
            string_free(include);
            if (ok)
                ++imported;
        } else {
            ok = !string_starts_with(line, ".") &&
                 string_append_utf8_exact(sql, string_c_str(line), string_byte_length(line)) == 0 &&
                 test_cfg_rendering_append_literal(sql, "\n") == 0;
        }
        string_free(line);
    }
    /* Do not even read the remainder: it contains unrelated and very large holiday history. */
    ok = ok && imported == sizeof includes / sizeof *includes &&
         test_cfg_rendering_load_sql(db, "packaging/jurisdiction-db/mars_calendar_locale_names.sql");
    if (!ok)
        string_fprintf(stderr, "Calendar seed: scoped catalogue import failed after %zu/5 seeds: %S\n", imported,
                       sqlite_last_error(db));
    if (transaction) {
        if (ok)
            ok = test_cfg_rendering_sql_ok(db, "COMMIT");
        if (!ok)
            test_cfg_rendering_sql_ok(db, "ROLLBACK");
    }
    if (ok)
        string_fprintf(stderr, "Calendar seed: scoped catalogue import complete; checking all-town locale coverage\n");
    string_free(sql);
    file_free(file);
    return ok;
}

static bool test_cfg_rendering_seed_fixture(const char *directory)
{
    sqlite_t *db = test_cfg_rendering_database(directory);
    bool ok =
        db && test_cfg_rendering_seed_snapshot(db) &&
        test_cfg_rendering_expect_literal(
            db, "SELECT name FROM pragma_table_info('calendar_local') ORDER BY cid",
            "FullDateAlternateKey\nFullDate and End Time\nYear\nMonth\nDay\nDays in Month\nMC Dates\nME Dates\n"
            "ME Dates Text\nWC Dates\nWE Dates\nWEDates (Friday)\nWE Dates plus End Time\nWE Dates Text\nDay of Week\n"
            "Day Name\nMonth Name\nFiscal Year End\nFiscal Month\nFiscal Quarter No\nFiscal Qtr\nFiscal Quarter Name\n"
            "Fiscal Years\nPublic Holiday\nNon working day Type\nWorkday Type\nWorking Day No\nDay "
            "Type\nSunrise\nSunset\n"
            "Moon Phase %\nDate UK\nMonth Name Abbrev\nDay Name Abbrev\nDate Lingua\nDate Regional") &&
        test_cfg_rendering_expect_literal(
            db, "SELECT count(*) FROM calendar_territory_locale WHERE length(territory)=2", "249") &&
        test_cfg_rendering_expect_literal(
            db,
            "SELECT count(*) FROM calendar_territory_locale AS t WHERE "
            "(SELECT count(*) FROM calendar_locale_month AS m WHERE m.locale=t.locale)!=12 OR "
            "(SELECT count(*) FROM calendar_locale_weekday AS d WHERE d.locale=t.locale)!=7",
            "0") &&
        test_cfg_rendering_expect_literal(
            db, "SELECT short_name,full_name FROM calendar_locale_month WHERE locale='ar_DZ' AND month_no=1",
            "جانفي|جانفي") &&
        test_cfg_rendering_expect_literal(db,
                                          "SELECT locale,short_name FROM calendar_locale_month "
                                          "WHERE locale IN ('fr_CA','fr_FR') AND month_no=7 ORDER BY locale",
                                          "fr_CA|juill.\nfr_FR|juil.") &&
        test_cfg_rendering_expect_literal(
            db,
            "SELECT a.weekday_set=b.weekday_set,a.month_set!=b.month_set FROM calendar_locale_name_set a,"
            "calendar_locale_name_set b WHERE a.locale='en_GB' AND b.locale='en_US'",
            "1|1");
    const char *tables[] = {"calendar_month_names", "calendar_weekday_names"};
    for (size_t i = 0; ok && i < 2; ++i) {
        string_t *sql = string_sprintf("SELECT count(*)=0 FROM (SELECT name_set FROM %s GROUP BY name_set HAVING "
                                       "count(*)!=%d)",
                                       tables[i], i ? 7 : 12);
        ok = sql && test_cfg_rendering_expect_literal(db, string_c_str(sql), "1");
        string_free(sql);
        sql = string_sprintf("SELECT count(DISTINCT name_set)<245 FROM %s", tables[i]);
        ok = ok && sql && test_cfg_rendering_expect_literal(db, string_c_str(sql), "1");
        string_free(sql);
        const char *values = i ? "weekday_no||':'||hex(day_name)||':'||hex(short_name)"
                               : "month_no||':'||hex(short_name)||':'||hex(full_name)";
        sql = string_sprintf("WITH signatures AS (SELECT name_set,group_concat(value,';') AS signature FROM "
                             "(SELECT name_set,%s AS value FROM %s ORDER BY 1,2) GROUP BY name_set) "
                             "SELECT count(*)=count(DISTINCT signature) FROM signatures",
                             values, tables[i]);
        ok = ok && sql && test_cfg_rendering_expect_literal(db, string_c_str(sql), "1");
        string_free(sql);
    }
    ok = ok && test_cfg_rendering_expect_literal(
                   db,
                   "SELECT count(*)>1,min(weekday_set='fr'),min(month_set='fr') FROM calendar_locale_name_set "
                   "WHERE locale GLOB 'fr_*' AND locale IN (SELECT locale FROM calendar_territory_locale WHERE "
                   "length(territory)=2)",
                   "1|1|1");
    ok = ok && test_cfg_rendering_load_sql(db, "packaging/jurisdiction-db/mars_calendar_locale_names.sql") &&
         test_cfg_rendering_expect_literal(db,
                                           "SELECT (SELECT count(*) FROM calendar_locale_month)=12*count(*) "
                                           "FROM calendar_locale_name_set",
                                           "1") &&
         test_cfg_rendering_fill_range(db, (civil_date_t){2024, 12, 31}, (civil_date_t){2024, 12, 31}, false);
    const struct {
        const char *jurisdiction, *expected;
    } labels[] = {{"FR", "mardi|déc.|05 janv. 2025|31ˢᵗ December 2024"},
                  {"CA-QC", "mardi|déc.|05 janv. 2025|31ˢᵗ December 2024"},
                  {"DE", "Dienstag|Dez.|05 Jan. 2025|31ˢᵗ December 2024"},
                  {"NL", "dinsdag|dec|05 jan 2025|31ˢᵗ December 2024"}};
    for (size_t i = 0; ok && i < sizeof labels / sizeof *labels; ++i)
        ok = test_cfg_rendering_settings(db, labels[i].jurisdiction, NULL) &&
             test_cfg_rendering_expect_literal(
                 db, "SELECT [Day Name],[Month Name Abbrev],[WE Dates Text],[Date UK] FROM calendar_local",
                 labels[i].expected);
    ok = ok && test_cfg_rendering_catalogue_sql(db) &&
         test_cfg_rendering_expect_literal(db, "SELECT count(*)>0 FROM jurisdiction_town", "1") &&
         test_cfg_rendering_expect_literal(
             db,
             "SELECT DISTINCT t.jurisdiction_id FROM jurisdiction_town AS t LEFT JOIN calendar_territory_locale AS l "
             "ON l.territory=substr(t.jurisdiction_id,1,2) WHERE l.locale IS NULL",
             "") &&
         test_cfg_rendering_seed_snapshot(db);
    sqlite_close(db);
    return ok;
}

static bool test_cfg_rendering_solar_fixture(const char *directory)
{
    time_t now = time(NULL);
    struct tm current;
    if (!gmtime_r(&now, &current))
        return false;
    sqlite_t *db = test_cfg_rendering_database(directory);
    bool ok = db &&
              test_cfg_rendering_fill_range(db, (civil_date_t){2015, 1, 1},
                                            (civil_date_t){current.tm_year + 1900 + 7, 12, 31}, false) &&
              test_cfg_rendering_sql_ok(db, "UPDATE calendar_local_settings SET latitude=52.7077,longitude=-2.7541;"
                                            "UPDATE calendar_local_days SET utc_offset_hours=1 WHERE "
                                            "calendar_date BETWEEN '2024-03-31' AND '2024-10-26' OR "
                                            "calendar_date BETWEEN '2025-03-30' AND '2025-10-25'") &&
              test_cfg_rendering_expect_literal(
                  db,
                  "SELECT Sunrise,Sunset FROM calendar_local WHERE FullDateAlternateKey IN ('2024-06-21','2024-12-21') "
                  "ORDER BY FullDateAlternateKey",
                  "04:47:00|21:39:00\n08:21:00|15:58:00") &&
              test_cfg_rendering_expect_literal(
                  db,
                  "SELECT count(*) FROM calendar_local WHERE Sunrise>=Sunset OR Sunrise IS NULL OR Sunset IS NULL "
                  "OR substr(Sunrise,-3)!=':00' OR substr(Sunset,-3)!=':00'",
                  "0") &&
              test_cfg_rendering_expect_literal(
                  db,
                  "WITH pairs(a,b,delta) AS (VALUES('2024-03-30','2024-03-31',60),('2024-10-26','2024-10-27',-60),"
                  "('2025-03-29','2025-03-30',60),('2025-10-25','2025-10-26',-60)) SELECT count(*) FROM pairs "
                  "JOIN calendar_local x ON x.FullDateAlternateKey=a JOIN calendar_local y ON y.FullDateAlternateKey=b "
                  "WHERE abs((strftime('%s',y.Sunrise)-strftime('%s',x.Sunrise))/60-delta)>3 OR "
                  "abs((strftime('%s',y.Sunset)-strftime('%s',x.Sunset))/60-delta)>3",
                  "0");
    for (int i = 0; ok && i < 2; ++i)
        ok = test_cfg_rendering_load_sql(db, "packaging/jurisdiction-db/mars_calendar_local.sql") &&
             test_cfg_rendering_expect_literal(
                 db, "SELECT Sunrise,Sunset FROM calendar_local WHERE FullDateAlternateKey='2024-06-21'",
                 "04:47:00|21:39:00");
    ok = ok && test_cfg_rendering_sql_ok(db, "UPDATE calendar_local_settings SET latitude=90") &&
         test_cfg_rendering_expect_literal(
             db, "SELECT Sunrise,Sunset FROM calendar_local WHERE FullDateAlternateKey='2024-06-21'", "<NULL>|<NULL>");
    sqlite_close(db);
    return ok;
}

typedef struct {
    void *library, *calendar;
    void *(*open)(const uint16_t *, int32_t, const char *, int, int *);
    void (*set_millis)(void *, double, int *);
    int32_t (*get)(const void *, int, int *);
    void (*close)(void *);
} test_cfg_rendering_oracle_t;

static void *test_cfg_rendering_icu_symbol(void *library, const char *name)
{
    void *symbol = dlsym(library, name);
    /* ICU publishes versioned C symbols; bounded compatibility range, not locale dispatch. */
    for (int version = 100; !symbol && version >= 50; --version) {
        string_t *versioned = string_sprintf("%s_%d", name, version);
        if (versioned)
            symbol = dlsym(library, string_c_str(versioned));
        string_free(versioned);
    }
    return symbol;
}

static void test_cfg_rendering_oracle_close(test_cfg_rendering_oracle_t *oracle)
{
    if (oracle->calendar)
        oracle->close(oracle->calendar);
    if (oracle->library)
        dlclose(oracle->library);
    *oracle = (test_cfg_rendering_oracle_t){0};
}

static bool test_cfg_rendering_oracle_open(test_cfg_rendering_oracle_t *oracle, const char *locale)
{
    *oracle = (test_cfg_rendering_oracle_t){0};
    oracle->library = dlopen("libicui18n.so", RTLD_LAZY | RTLD_LOCAL);
    for (int version = 100; !oracle->library && version >= 50; --version) {
        string_t *name = string_sprintf("libicui18n.so.%d", version);
        if (name)
            oracle->library = dlopen(string_c_str(name), RTLD_LAZY | RTLD_LOCAL);
        string_free(name);
    }
    if (!oracle->library)
        return false;
    void *open_symbol = test_cfg_rendering_icu_symbol(oracle->library, "ucal_open");
    void *set_symbol = test_cfg_rendering_icu_symbol(oracle->library, "ucal_setMillis");
    void *get_symbol = test_cfg_rendering_icu_symbol(oracle->library, "ucal_get");
    void *close_symbol = test_cfg_rendering_icu_symbol(oracle->library, "ucal_close");
    bool ok = open_symbol && set_symbol && get_symbol && close_symbol && sizeof oracle->open == sizeof open_symbol &&
              sizeof oracle->set_millis == sizeof set_symbol && sizeof oracle->get == sizeof get_symbol &&
              sizeof oracle->close == sizeof close_symbol;
    if (ok) {
        memcpy(&oracle->open, &open_symbol, sizeof oracle->open);
        memcpy(&oracle->set_millis, &set_symbol, sizeof oracle->set_millis);
        memcpy(&oracle->get, &get_symbol, sizeof oracle->get);
        memcpy(&oracle->close, &close_symbol, sizeof oracle->close);
        const uint16_t utc[] = {85, 84, 67};
        int status = 0;
        oracle->calendar = oracle->open(utc, 3, locale, 0, &status);
        ok = oracle->calendar && status <= 0;
    }
    if (!ok)
        test_cfg_rendering_oracle_close(oracle);
    return ok;
}

static bool test_cfg_rendering_oracle_fields(test_cfg_rendering_oracle_t *oracle, civil_date_t date,
                                             civil_date_t *fields)
{
    int status = 0;
    int epoch = test_cfg_rendering_civil_index((civil_date_t){1970, 1, 1});
    double millis = (test_cfg_rendering_civil_index(date) - epoch) * 86400000.0;
    oracle->set_millis(oracle->calendar, millis, &status);
    fields->year = oracle->get(oracle->calendar, 1, &status);
    fields->month = oracle->get(oracle->calendar, 2, &status) + 1;
    fields->day = oracle->get(oracle->calendar, 5, &status);
    return status <= 0;
}

static bool test_cfg_rendering_cycle_fixture(const char *directory, bool hebrew)
{
    static const char *const hebrew_months[] = {"תשרי", "חשוון", "כסלו",  "טבת",  "שבט", "אדר א׳", "אדר",
                                                "ניסן", "אייר",  "סיוון", "תמוז", "אב",  "אלול"};
    static const char *const jewish_english[] = {"Tishrei", "Cheshvan", "Kislev", "Tevet",  "Shevat", "Adar I", "Adar",
                                                 "Nisan",   "Iyar",     "Sivan",  "Tammuz", "Av",     "Elul"};
    static const char *const muslim_english[] = {
        "Muharram", "Safar",   "Rabi al-awwal", "Rabi al-thani", "Jumada al-awwal", "Jumada al-thani",
        "Rajab",    "Sha'ban", "Ramadan",       "Shawwal",       "Dhu al-Qadah",    "Dhu al-Hijjah"};
    test_cfg_rendering_oracle_t oracle;
    bool ok =
        test_cfg_rendering_oracle_open(&oracle, hebrew ? "en_US@calendar=hebrew" : "en_US@calendar=islamic-civil");
    sqlite_t *db = ok ? test_cfg_rendering_database(directory) : NULL;
    civil_date_t first = hebrew ? (civil_date_t){2000, 1, 1} : (civil_date_t){2015, 1, 1};
    civil_date_t last = hebrew ? (civil_date_t){2037, 12, 31} : test_cfg_rendering_move_day(first, 10630);
    ok = ok && db && test_cfg_rendering_fill_range(db, first, last, false) &&
         test_cfg_rendering_settings(db, "GB-ENG", hebrew ? "he_IL" : "ar_EG");
    sqlite_stmt_t *query =
        ok ? sqlite_stmt_prepare(
                 db, "SELECT FullDateAlternateKey,[Date Lingua] FROM calendar_local ORDER BY FullDateAlternateKey")
           : NULL;
    int previous_day = 0, previous_start = -1;
    unsigned year_lengths = 0, month_lengths = 0, adars = 0;
    for (civil_date_t date = first; ok && test_cfg_rendering_civil_index(date) <= test_cfg_rendering_civil_index(last);
         date = test_cfg_rendering_move_day(date, 1)) {
        civil_date_t lunar = {0};
        ok = query && sqlite_stmt_step(query) == SQLITE_STEP_ROW &&
             test_cfg_rendering_oracle_fields(&oracle, date, &lunar) && lunar.month >= 1 &&
             lunar.month <= (hebrew ? 13 : 12);
        if (!ok)
            break;
        bool second_adar = hebrew && lunar.month == 7 && (7 * lunar.year + 1) % 19 < 7;
        const char *month =
            hebrew ? (second_adar ? "Adar II" : jewish_english[lunar.month - 1]) : muslim_english[lunar.month - 1];
        string_t *key = test_cfg_rendering_date_text(date);
        string_t *expected = hebrew
                                 ? string_sprintf("%d ב%s %d", lunar.day,
                                                  second_adar ? "אדר ב׳" : hebrew_months[lunar.month - 1], lunar.year)
                                 : test_cfg_rendering_arabic_text(date);
        string_t *native_expected = string_sprintf("%d %s %d %s", lunar.day, month, lunar.year, hebrew ? "AM" : "AH");
        datetime_t *native_date = key ? datetime_from_string(string_c_str(key)) : NULL;
        string_t *native_text = native_date ? (hebrew ? datetime_jewish_calendar_date_text(native_date)
                                                      : datetime_muslim_calendar_date_text(native_date))
                                            : NULL;
        ok = key && expected && native_expected && native_text && test_cfg_rendering_column_equals(query, 0, key) &&
             test_cfg_rendering_column_equals(query, 1, expected) && string_compare(native_expected, native_text) == 0;
        if (hebrew) {
            if (lunar.day == 1 && previous_day) {
                ok = ok && (previous_day == 29 || previous_day == 30);
                if (previous_day == 29 || previous_day == 30)
                    month_lengths |= 1u << (previous_day - 29);
            }
            if (lunar.month == 1 && lunar.day == 1) {
                int start = test_cfg_rendering_civil_index(date);
                if (previous_start >= 0) {
                    int length = start - previous_start;
                    ok = ok && ((length >= 353 && length <= 355) || (length >= 383 && length <= 385));
                    if (ok)
                        year_lengths |= 1u << (length >= 383 ? length - 380 : length - 353);
                }
                previous_start = start;
            }
            if (lunar.month == 6 || lunar.month == 7)
                adars |= lunar.month == 6 ? 1u : second_adar ? 2u : 4u;
            previous_day = lunar.day;
        } else {
            civil_date_t reference = test_cfg_rendering_hijri_reference(date);
            ok = ok && lunar.year == reference.year && lunar.month == reference.month && lunar.day == reference.day;
        }
        if (!ok)
            string_fprintf(stderr, "calendar cycle mismatch on %S; native expected %S, got %S\n", key, native_expected,
                           native_text);
        string_free(key);
        string_free(expected);
        string_free(native_expected);
        string_free(native_text);
        datetime_dealloc(native_date);
    }
    ok = ok && sqlite_stmt_step(query) == SQLITE_STEP_DONE &&
         (!hebrew || (year_lengths == 63 && month_lengths == 3 && adars == 7));
    sqlite_stmt_finalize(query);
    sqlite_close(db);
    test_cfg_rendering_oracle_close(&oracle);
    return ok;
}

static bool test_cfg_rendering_hebrew_fixture(const char *directory)
{
    return test_cfg_rendering_cycle_fixture(directory, true);
}

static bool test_cfg_rendering_hijri_fixture(const char *directory)
{
    return test_cfg_rendering_cycle_fixture(directory, false);
}

static bool test_cfg_rendering_readme_holidays(sqlite_t *db, const string_t *guide)
{
    const char *rhyl_query = "select FullDateAlternateKey, [Date Lingua], [Public Holiday]\nfrom calendar_local\n"
                             "where FullDateAlternateKey between '2026-12-25' and '2026-12-28'\n"
                             "  and [Public Holiday] is not null\norder by FullDateAlternateKey;";
    const char *spanish_query =
        "select FullDateAlternateKey, [Date Lingua], [Public Holiday]\nfrom calendar_local\n"
        "where FullDateAlternateKey in ('2026-01-01', '2026-07-04', '2026-12-25')\norder by FullDateAlternateKey;";
    const struct {
        const char *jurisdiction, *locale, *inputs, *expected;
    } cases[] = {
        {"GB-WLS", "cy_GB",
         "('2026-12-25','Dydd Nadolig',0,0),"
         "('2026-12-28','Dydd San Steffan (diwrnod amgen)',0,0)",
         "2026-12-25|25 Rhagfyr 2026|Dydd Nadolig\n2026-12-28|28 Rhagfyr 2026|Dydd San Steffan (diwrnod amgen)"},
        {"GB-WLS", "ga_GB",
         "('2026-12-25','Lá Nollag',0,0),"
         "('2026-12-28','Lá Fhéile Stiofáin (lá ionaid)',0,0)",
         "2026-12-25|25 Nollaig 2026|Lá Nollag\n2026-12-28|28 Nollaig 2026|Lá Fhéile Stiofáin (lá ionaid)"},
        {"US-NY", "es_US",
         "('2026-01-01','Día de Año Nuevo',0,0),"
         "('2026-07-04','Día de la Independencia',1,0),('2026-12-25','Día de Navidad',0,0)",
         "2026-01-01|1 de enero de 2026|Día de Año Nuevo\n"
         "2026-07-04|4 de julio de 2026|Día de la Independencia\n2026-12-25|25 de diciembre de 2026|Día de Navidad"}};
    bool ok = true;
    for (size_t i = 0; ok && i < sizeof cases / sizeof *cases; ++i) {
        const char *query = i == 2 ? spanish_query : rhyl_query;
        string_t *setup = string_sprintf("DELETE FROM calendar_local_days; INSERT INTO calendar_local_days VALUES %s",
                                         cases[i].inputs);
        string_t *output = string_sprintf("FullDateAlternateKey|Date Lingua|Public Holiday\n%s", cases[i].expected);
        ok = setup && output && string_find(guide, query) >= 0 && string_find_string(guide, output) >= 0 &&
             test_cfg_rendering_sql_ok(db, string_c_str(setup)) &&
             test_cfg_rendering_settings(db, cases[i].jurisdiction, cases[i].locale) &&
             test_cfg_rendering_expect_literal(db, query, cases[i].expected);
        if (ok)
            string_printf("README example (supplied holiday labels):\n%S\n", output);
        string_free(setup);
        string_free(output);
    }
    return ok;
}

static bool test_cfg_rendering_readme_fixture(const char *directory)
{
    /* README examples: only SQL projections; holiday labels below are supplied inputs, not policy assertions. */
    const struct {
        const char *jurisdiction, *locale, *sql, *header, *expected;
    } cases[] = {
        {"NL", "fy_NL",
         "select [Date Lingua], [Date Regional], [Public Holiday]\n"
         "from calendar_local where FullDateAlternateKey = '2026-12-25';",
         "Date Lingua|Date Regional|Public Holiday\n", "25 Desimber 2026|25 december 2026|Earste Krystdei"},
        {"JM", "jam",
         "select FullDateAlternateKey, [Date Lingua], [Day Name], [Public Holiday]\n"
         "from calendar_local\nwhere FullDateAlternateKey = '2026-08-06';",
         "FullDateAlternateKey|Date Lingua|Day Name|Public Holiday\n",
         "2026-08-06|6 Aagus 2026|Turzdeh|Independence Deh"},
        {"GB-ENG", "he_IL",
         "select [Date Lingua], [Date UK]\nfrom calendar_local\nwhere FullDateAlternateKey = '2026-04-02';", "",
         "15 בניסן 5786|2ⁿᵈ April 2026"},
        {"GB-ENG", "ar_PS",
         "select [Date Lingua], [Date UK]\nfrom calendar_local\nwhere FullDateAlternateKey = '2026-04-02';", "",
         "١٤ شوال ١٤٤٧|2ⁿᵈ April 2026"},
        {"GB-ENG", "br_FR",
         "select [Date Lingua], [Month Name], [Month Name Abbrev], [Day Name], [Day Name Abbrev]\n"
         "from calendar_local\nwhere FullDateAlternateKey = '2024-06-21';",
         "", "21 Mezheven 2024|Mezheven|Mezh.|Gwener|Gwe."},
        {"GB-ENG", "kw_GB",
         "select [Date Lingua], [Month Name], [Month Name Abbrev], [Day Name], [Day Name Abbrev]\n"
         "from calendar_local\nwhere FullDateAlternateKey = '2024-06-21';",
         "", "21 mis Metheven 2024|mis Metheven|Met|dy Gwener|Gwe"},
        {"CA-QC", "fr_CA",
         "select [Date Lingua], [Date Regional], [Public Holiday]\n"
         "from calendar_local where FullDateAlternateKey = '2026-07-01';",
         "", "1ᵉʳ juillet 2026|1ᵉʳ juillet 2026|Fête du Canada"},
        {"VA", "la_VA",
         "select [Date Lingua], \"Day Name\", \"Day Name Abbrev\", \"Month Name\", \"Month Name Abbrev\", [Date UK]\n"
         "from calendar_local\nwhere FullDateAlternateKey = '2024-06-21';",
         "Date Lingua|Day Name|Day Name Abbrev|Month Name|Month Name Abbrev|Date UK\n",
         "21 Iunii 2024|Veneris|Ven|Iunius|Iun|21ˢᵗ June 2024"},
        {"NL", "nl_NL",
         "select [Date Lingua], \"Day Name\", \"Day Name Abbrev\", \"Month Name\", \"Month Name Abbrev\",\n"
         "       \"ME Dates Text\", \"WE Dates Text\"\nfrom calendar_local\nwhere FullDateAlternateKey = '2024-06-21';",
         "Date Lingua|Day Name|Day Name Abbrev|Month Name|Month Name Abbrev|ME Dates Text|WE Dates Text\n",
         "21 juni 2024|vrijdag|vr|juni|jun|jun 2024|23 jun 2024"},
        {"GB-ENG", "en_GB",
         "select FullDateAlternateKey, [Date UK], [Date Lingua], \"Day Name\", Sunrise, Sunset\n"
         "from calendar_local\nwhere FullDateAlternateKey = '2024-06-21';",
         "FullDateAlternateKey|Date UK|Date Lingua|Day Name|Sunrise|Sunset\n",
         "2024-06-21|21ˢᵗ June 2024|21ˢᵗ June 2024|Friday|04:47:00|21:39:00"},
        {"GB-ENG", "en_GB",
         "select FullDateAlternateKey, [Fiscal Quarter Name]\nfrom calendar_local\n"
         "where FullDateAlternateKey in ('2024-03-31', '2024-04-01', '2024-07-01', '2025-01-01')\n"
         "order by FullDateAlternateKey;",
         "FullDateAlternateKey|Fiscal Quarter Name\n",
         "2024-03-31|2024 Q4\n2024-04-01|2025 Q1\n"
         "2024-07-01|2025 Q2\n2025-01-01|2025 Q4"},
        {"GB-ENG", "en_GB",
         "select FullDateAlternateKey, [Workday Type], [Working Day No]\nfrom calendar_local\n"
         "where FullDateAlternateKey between '2015-01-01' and '2015-01-05'\norder by FullDateAlternateKey;",
         "FullDateAlternateKey|Workday Type|Working Day No\n",
         "2015-01-01|0|0\n2015-01-02|1|1\n2015-01-03|0|1\n2015-01-04|0|1\n2015-01-05|1|2"}};
    sqlite_t *db = test_cfg_rendering_database(directory);
    file_t *file = file_new_cstr(MARS_CONFIG_ROOT_DIR "/docs/jurisdiction.md");
    string_t *guide = file ? file_read_all_text(file) : NULL;
    bool ok =
        db && guide &&
        test_cfg_rendering_sql_ok(
            db, "UPDATE calendar_local_settings SET latitude=52.7077,longitude=-2.7541;"
                "INSERT INTO calendar_local_days VALUES ('2015-01-01','New Years Day',0,0),('2015-01-02',NULL,0,0),"
                "('2015-01-03',NULL,1,0),('2015-01-04',NULL,1,0),('2015-01-05',NULL,0,0),('2024-03-31',NULL,1,0),"
                "('2024-04-01',NULL,0,1),('2024-06-21',NULL,0,1),('2024-07-01',NULL,0,1),('2025-01-01',NULL,0,0),"
                "('2026-04-02',NULL,0,1),('2026-07-01','Fête du Canada',0,1),('2026-08-06','Independence Deh',0,1),"
                "('2026-12-25','Earste Krystdei',0,0)");
    for (size_t i = 0; ok && i < sizeof cases / sizeof *cases; ++i) {
        string_t *output = string_sprintf("%s%s", cases[i].header, cases[i].expected);
        ok = output && string_find(guide, cases[i].sql) >= 0 && string_find_string(guide, output) >= 0 &&
             test_cfg_rendering_settings(db, cases[i].jurisdiction, cases[i].locale) &&
             test_cfg_rendering_expect_literal(db, cases[i].sql, cases[i].expected);
        if (ok)
            string_printf("README example:\n%S\n", output);
        else
            string_fprintf(stderr, "README SQL/output mismatch: %s\n%S\n", cases[i].sql, output);
        string_free(output);
    }
    ok = ok && test_cfg_rendering_readme_holidays(db, guide);
    string_free(guide);
    file_free(file);
    sqlite_close(db);
    return ok;
}

static void test_cfg_rendering_uk(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_rendering_uk_ordinals), "UK ordinal scope and overseas rendering");
}

static void test_cfg_rendering_french(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_rendering_french_ordinals), "French ordinals throughout a leap year");
}

static void test_cfg_rendering_regional(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_rendering_regional_and_grammar),
                     "regional dates and grammatical names");
}

static void test_cfg_rendering_supplements(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_rendering_supplemental_names), "supplemental months and weekdays");
}

static void test_cfg_rendering_babel(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_rendering_babel_reference),
                     "all packaged locales against Babel 2.17/CLDR 46");
}

static void test_cfg_rendering_boundaries(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_rendering_arabic_and_hebrew_boundaries),
                     "Hijri digits and Hebrew boundaries");
}

static void test_cfg_rendering_fields(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_rendering_fields_fixture),
                     "Gregorian, fiscal, workday and moon fields");
}

static void test_cfg_rendering_working(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_rendering_working_fixture),
                     "empty, singleton and non-standard weekends");
}

static void test_cfg_rendering_seed(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_rendering_seed_fixture),
                     "projection, original seed SHA-256, catalogue coverage and locale separation");
}

static void test_cfg_rendering_solar(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_rendering_solar_fixture),
                     "solar, DST, polar nulls and standalone reload");
}

static void test_cfg_rendering_hijri(void)
{
    test_cfg_rendering_oracle_t oracle;
    if (!test_cfg_rendering_oracle_open(&oracle, "en_US@calendar=islamic-civil")) {
        TEST_SKIP("Independent ICU C calendar oracle unavailable");
        return;
    }
    test_cfg_rendering_oracle_close(&oracle);
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_rendering_hijri_fixture),
                     "full civil Hijri cycle against ICU and native");
}

static void test_cfg_rendering_hebrew(void)
{
    test_cfg_rendering_oracle_t oracle;
    if (!test_cfg_rendering_oracle_open(&oracle, "en_US@calendar=hebrew")) {
        TEST_SKIP("Independent ICU C calendar oracle unavailable");
        return;
    }
    test_cfg_rendering_oracle_close(&oracle);
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_rendering_hebrew_fixture), "38 Hebrew years against ICU and native");
}

static void test_cfg_rendering_readme(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_rendering_readme_fixture), "README calendar SQL and displayed output");
}

/* Register ordinary rendering checks; the parent keeps every README example last. */
void test_cfg_rendering_cases(void)
{
    TEST_RUN_IN_GROUP(test_cfg_rendering_uk, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rendering_french, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rendering_regional, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rendering_supplements, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rendering_babel, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rendering_boundaries, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rendering_fields, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rendering_working, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rendering_seed, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rendering_solar, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rendering_hijri, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rendering_hebrew, tests, NULL);
}

/* Register pure SQL README examples separately from the ordinary rendering regressions. */
void test_cfg_rendering_readme_cases(void)
{
    TEST_RUN_IN_GROUP(test_cfg_rendering_readme, readme_examples, NULL);
}
