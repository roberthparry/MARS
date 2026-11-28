/**
 * @file jurisdiction_date_locale.c
 * @brief Immutable Lingua date snapshots loaded from the jurisdiction catalogue.
 *
 * Loads translated month names and already compiled date parts once. Rendering dispatches directly through eight
 * field handlers without SQL, CLDR parsing or host locale dependencies. Calendar arithmetic stays in datetime.
 * Arabic and Hebrew month spellings share the catalogue used by the local calendar SQL view.
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_SHARED_DATETIME_CALENDAR_INTERNAL_ACCESS
#include "internal/datetime_calendar_internal.h"
#define MARS_JURISDICTION_INTERNAL_ACCESS
#include "jurisdiction_internal.h"

typedef struct {
    unsigned field;
    char *literal;
} jurisdiction_date_part_t;

struct _jurisdict_date_locale_t {
    char *months[14][2];
    char *leap_month;
    jurisdiction_date_part_t *parts;
    size_t count;
    unsigned calendar;
    bool ordinal;
};

typedef struct {
    const jurisdict_date_locale_t *locale;
    int year, month, day;
    string_t *out;
} jurisdiction_date_render_t;

static unsigned jurisdiction_date_field(const char *name)
{
    /* Eight field names occupy eight slots; verify membership before using the handler index. */
    static const char *const fields[8] = {
        "day_padded", "month_full", "first_day_suffix", "month_short", "year", "literal", "month", "day",
    };
    uint32_t hash = 447u;
    for (const unsigned char *at = (const unsigned char *)name; *at; ++at)
        hash = (hash ^ *at) * UINT32_C(16777619);
    unsigned slot = (hash ^ (hash >> 16u)) & 7u;
    return strcmp(name, fields[slot]) ? UINT_MAX : slot;
}

/* Release all copied catalogue data, including partially loaded snapshots. */
void jurisdict_date_locale_free(jurisdict_date_locale_t *locale)
{
    if (!locale)
        return;
    for (size_t month = 0; month < 14; ++month) {
        free(locale->months[month][0]);
        free(locale->months[month][1]);
    }
    for (size_t i = 0; i < locale->count; ++i)
        free(locale->parts[i].literal);
    free(locale->parts);
    free(locale->leap_month);
    free(locale);
}

static bool jurisdiction_date_months(jurisdiction_t *engine, jurisdict_date_locale_t *locale, const char *code)
{
    const char *sql = locale->calendar
        ? "SELECT month_no,full_name,full_name,leap_name FROM calendar_native_month WHERE calendar=?1 ORDER BY month_no"
        : "SELECT month_no,short_name,full_name,full_name FROM calendar_locale_month WHERE locale=?1 ORDER BY month_no";
    sqlite_stmt_t *query = sqlite_stmt_prepare(engine->db, sql);
    const char *key = locale->calendar ? (locale->calendar == 1 ? "islamic" : "hebrew") : code;
    bool ok = query && sqlite_stmt_bind_text(query, 1, key);
    sqlite_step_result_t step = SQLITE_STEP_ERROR;
    unsigned count = 0;
    while (ok && (step = sqlite_stmt_step(query)) == SQLITE_STEP_ROW) {
        int month = sqlite_stmt_column_int(query, 0);
        ok = month >= 1 && month <= (locale->calendar == 2 ? 13 : 12) && !locale->months[month][0];
        if (!ok)
            break;
        locale->months[month][0] = jurisdiction_dup_c_string(sqlite_stmt_column_text(query, 1));
        locale->months[month][1] = jurisdiction_dup_c_string(sqlite_stmt_column_text(query, 2));
        if (locale->calendar == 2 && month == 7)
            locale->leap_month = jurisdiction_dup_c_string(sqlite_stmt_column_text(query, 3));
        ok = locale->months[month][0] && locale->months[month][1] &&
             (locale->calendar != 2 || month != 7 || locale->leap_month);
        ++count;
    }
    sqlite_stmt_finalize(query);
    return ok && step == SQLITE_STEP_DONE && count == (locale->calendar == 2 ? 13u : 12u);
}

static bool jurisdiction_date_parts(jurisdiction_t *engine, jurisdict_date_locale_t *locale, const char *code)
{
    sqlite_stmt_t *query = sqlite_stmt_prepare(engine->db,
        "SELECT p.field,p.literal FROM calendar_locale_date_pattern AS l "
        "JOIN calendar_date_pattern_part AS p ON p.pattern_id=l.pattern_id WHERE l.locale=?1 ORDER BY p.position");
    bool ok = query && sqlite_stmt_bind_text(query, 1, code);
    sqlite_step_result_t step = SQLITE_STEP_ERROR;
    while (ok && (step = sqlite_stmt_step(query)) == SQLITE_STEP_ROW) {
        const char *name = sqlite_stmt_column_text(query, 0), *literal = sqlite_stmt_column_text(query, 1);
        unsigned field = name ? jurisdiction_date_field(name) : UINT_MAX;
        if (field == UINT_MAX || !literal || locale->count >= 64u) {
            ok = false;
            break;
        }
        jurisdiction_date_part_t *parts = realloc(locale->parts, (locale->count + 1u) * sizeof(*parts));
        if (!parts) {
            ok = false;
            break;
        }
        locale->parts = parts;
        char *copy = jurisdiction_dup_c_string(literal);
        if (!copy) {
            ok = false;
            break;
        }
        locale->parts[locale->count++] = (jurisdiction_date_part_t){field, copy};
    }
    sqlite_stmt_finalize(query);
    return ok && step == SQLITE_STEP_DONE && locale->count;
}

/* Snapshot the installed Lingua choice or an explicit catalogue locale. */
jurisdict_date_locale_t *jurisdict_date_locale_load(jurisdiction_t *engine, const char *requested)
{
    if (!engine)
        return NULL;
    jurisdiction_set_error(engine, "");
    char *code = NULL, *jurisdiction = NULL;
    sqlite_stmt_t *query = sqlite_stmt_prepare(engine->db,
        "SELECT coalesce(nullif(?1,''),nullif(s.locale,''),local.locale,territory.locale),"
        "coalesce(s.jurisdiction,?2) FROM (SELECT 1) "
        "LEFT JOIN calendar_local_settings AS s ON s.singleton=1 "
        "LEFT JOIN calendar_territory_locale AS local ON local.territory=coalesce(s.jurisdiction,?2) "
        "LEFT JOIN calendar_territory_locale AS territory ON territory.territory=substr(coalesce(s.jurisdiction,?2),1,2)");
    bool ok = query && sqlite_stmt_bind_text(query, 1, requested ? requested : "") &&
              sqlite_stmt_bind_text(query, 2, engine->jurisdiction) && sqlite_stmt_step(query) == SQLITE_STEP_ROW;
    if (ok) {
        code = jurisdiction_dup_c_string(sqlite_stmt_column_text(query, 0));
        jurisdiction = jurisdiction_dup_c_string(sqlite_stmt_column_text(query, 1));
    }
    sqlite_stmt_finalize(query);
    jurisdict_date_locale_t *locale = code && jurisdiction ? calloc(1u, sizeof(*locale)) : NULL;
    if (locale) {
        locale->calendar = (!strcmp(code, "ar") || !strncmp(code, "ar_", 3)) ? 1u :
                           (!strcmp(code, "he") || !strncmp(code, "he_", 3)) ? 2u : 0u;
        locale->ordinal = (!strcmp(jurisdiction, "GB") || !strncmp(jurisdiction, "GB-", 3)) &&
                          (!strcmp(code, "en") || !strncmp(code, "en_", 3));
        if (!jurisdiction_date_months(engine, locale, code) || !jurisdiction_date_parts(engine, locale, code)) {
            jurisdict_date_locale_free(locale);
            locale = NULL;
        }
    }
    free(code);
    free(jurisdiction);
    if (!locale)
        jurisdiction_set_error(engine, "Lingua locale is unavailable or its calendar catalogue is incomplete");
    return locale;
}

static bool jurisdiction_date_number(jurisdiction_date_render_t *render, int value, unsigned width)
{
    char digits[24];
    int count = snprintf(digits, sizeof(digits), "%0*d", (int)width, value);
    if (count < 0 || (size_t)count >= sizeof(digits))
        return false;
    if (render->locale->calendar != 1)
        return string_append_bytes(render->out, digits, (size_t)count) >= 0;
    static const char arabic_digits[10][3] = {"٠", "١", "٢", "٣", "٤", "٥", "٦", "٧", "٨", "٩"};
    for (int i = 0; i < count; ++i) {
        unsigned digit = (unsigned char)digits[i] - '0';
        if (digit < 10u ? string_append_bytes(render->out, arabic_digits[digit], 2u) < 0 :
                         string_append_bytes(render->out, digits + i, 1u) < 0)
            return false;
    }
    return true;
}

static bool jurisdiction_date_day(jurisdiction_date_render_t *render, unsigned width)
{
    if (!jurisdiction_date_number(render, render->day, width))
        return false;
    if (!render->locale->ordinal)
        return true;
    static const char *const suffix[10] = {"ᵗʰ", "ˢᵗ", "ⁿᵈ", "ʳᵈ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ", "ᵗʰ"};
    unsigned day = (unsigned)render->day;
    return string_append_cstr(render->out, suffix[day % 100u >= 11u && day % 100u <= 13u ? 0u : day % 10u]) >= 0;
}

static bool jurisdiction_date_day_padded(jurisdiction_date_render_t *r, const char *literal)
{
    (void)literal;
    return jurisdiction_date_day(r, 2u);
}

static bool jurisdiction_date_day_plain(jurisdiction_date_render_t *r, const char *literal)
{
    (void)literal;
    return jurisdiction_date_day(r, 1u);
}

static bool jurisdiction_date_month_name(jurisdiction_date_render_t *r, unsigned form)
{
    bool leap = (7 * r->year + 1) % 19 < 7;
    const char *name = r->locale->calendar == 2 && r->month == 7 && leap ? r->locale->leap_month :
                       r->locale->months[r->month][form];
    return name && string_append_cstr(r->out, name) >= 0;
}

static bool jurisdiction_date_month_full(jurisdiction_date_render_t *r, const char *literal)
{
    (void)literal;
    return jurisdiction_date_month_name(r, 1u);
}

static bool jurisdiction_date_month_short(jurisdiction_date_render_t *r, const char *literal)
{
    (void)literal;
    return jurisdiction_date_month_name(r, 0u);
}

static bool jurisdiction_date_first_suffix(jurisdiction_date_render_t *r, const char *literal)
{
    return r->day != 1 || string_append_cstr(r->out, literal) >= 0;
}

static bool jurisdiction_date_year(jurisdiction_date_render_t *r, const char *literal)
{
    (void)literal;
    return jurisdiction_date_number(r, r->year, 4u);
}

static bool jurisdiction_date_literal(jurisdiction_date_render_t *r, const char *literal)
{
    return string_append_cstr(r->out, literal) >= 0;
}

static bool jurisdiction_date_month_number(jurisdiction_date_render_t *r, const char *literal)
{
    (void)literal;
    return jurisdiction_date_number(r, r->month, 1u);
}

/* Render from copied names and direct field dispatch, with no live database dependency. */
string_t *jurisdict_date_locale_text(const jurisdict_date_locale_t *locale, const datetime_t *date)
{
    if (!locale || !date)
        return NULL;
    jurisdiction_date_render_t render = {.locale = locale, .year = datetime_year(date),
                                         .month = datetime_month(date), .day = datetime_day(date)};
    if (render.year < 1 || render.year == SHRT_MAX || render.month < 1 || render.month > 12 ||
        render.day < 1 || render.day > 31)
        return NULL;
    if (locale->calendar == 1 && !datetime_islamic_components(date, &render.year, &render.month, &render.day))
        return NULL;
    if (locale->calendar == 2 && !datetime_hebrew_components(date, &render.year, &render.month, &render.day))
        return NULL;
    static bool (*const fields[8])(jurisdiction_date_render_t *, const char *) = {
        jurisdiction_date_day_padded, jurisdiction_date_month_full, jurisdiction_date_first_suffix,
        jurisdiction_date_month_short, jurisdiction_date_year, jurisdiction_date_literal,
        jurisdiction_date_month_number, jurisdiction_date_day_plain,
    };
    render.out = string_new();
    if (!render.out)
        return NULL;
    for (size_t i = 0; i < locale->count; ++i) {
        if (!fields[locale->parts[i].field](&render, locale->parts[i].literal)) {
            string_free(render.out);
            return NULL;
        }
    }
    return render.out;
}
