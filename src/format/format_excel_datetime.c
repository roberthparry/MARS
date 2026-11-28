/**
 * @file format_excel_datetime.c
 * @brief Gregorian calendar and elapsed-time rendering for Excel custom codes.
 *
 * Provides locale-independent English calendar names, context-sensitive month/minute fields, fractional seconds,
 * twelve-hour time and elapsed durations. Numeric dates use Excel's 1900 serial convention, including serial 60;
 * native civil datetimes retain their actual Gregorian date and need no dependency on the datetime module.
 */
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "format-t.h"
#define MARS_FORMAT_INTERNAL_ACCESS
#include "format_internal.h"
#define MARS_FORMAT_INTERNAL_ACCESS
#include "format_excel_internal.h"

static const char *fx_day_suffix(unsigned day, bool superscript)
{
    static const char *const ordinary[4] = {"th", "st", "nd", "rd"};
    static const char *const raised[4] = {"ᵗʰ", "ˢᵗ", "ⁿᵈ", "ʳᵈ"};
    unsigned last_two = day % 100u;
    unsigned index = last_two >= 11u && last_two <= 13u ? 0u : day % 10u;
    if (index > 3u)
        index = 0u;
    return superscript ? raised[index] : ordinary[index];
}

static int64_t fx_civil_days(int year, unsigned month, unsigned day)
{
    year -= month <= 2u;
    int era = (year >= 0 ? year : year - 399) / 400;
    unsigned yoe = (unsigned)(year - era * 400);
    unsigned doy = (153u * (month > 2u ? month - 3u : month + 9u) + 2u) / 5u + day - 1u;
    return (int64_t)era * 146097 + yoe * 365u + yoe / 4u - yoe / 100u + doy;
}

static void fx_days_civil(int64_t days, int *year, unsigned *month, unsigned *day)
{
    int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    unsigned doe = (unsigned)(days - era * 146097);
    unsigned yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    int y = (int)yoe + (int)era * 400;
    unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    unsigned mp = (5u * doy + 2u) / 153u;
    *day = doy - (153u * mp + 2u) / 5u + 1u;
    *month = mp < 10u ? mp + 3u : mp - 9u;
    *year = y + (*month <= 2u);
}

typedef struct {
    const char *text;
    size_t length;
} fx_calendar_name_t;

static const fx_calendar_name_t fx_months[12] = {
    {"January", 7u}, {"February", 8u}, {"March", 5u}, {"April", 5u}, {"May", 3u}, {"June", 4u},
    {"July", 4u}, {"August", 6u}, {"September", 9u}, {"October", 7u}, {"November", 8u}, {"December", 8u},
};
static const fx_calendar_name_t fx_weekdays[7] = {
    {"Sunday", 6u}, {"Monday", 6u}, {"Tuesday", 7u}, {"Wednesday", 9u},
    {"Thursday", 8u}, {"Friday", 6u}, {"Saturday", 8u},
};

typedef struct {
    const fmt_excel_t *plan;
    const fmt_excel_section_t *section;
    string_t *out;
    fmt_excel_fill_t fill;
    fmt_error_t *error;
    int year;
    unsigned month, day, weekday, hour, minute;
    uint64_t second, fractional;
    long double elapsed;
    bool range_error;
} fx_calendar_state_t;

typedef bool (*fx_calendar_fn)(fx_calendar_state_t *, const fmt_excel_token_t *);

static bool fx_calendar_literal(fx_calendar_state_t *state, const fmt_excel_token_t *token)
{
    return fmt_excel_literal(state->out, state->plan, token, &state->fill);
}

static bool fx_calendar_name(string_t *out, const fx_calendar_name_t *name, size_t width)
{
    static const size_t lengths[6] = {0u, 0u, 0u, 3u, SIZE_MAX, 1u};
    size_t length = lengths[width];
    return string_append_bytes(out, name->text, length == SIZE_MAX ? name->length : length) == 0;
}

static bool fx_calendar_year(fx_calendar_state_t *state, const fmt_excel_token_t *token)
{
    bool short_year = token->length <= 2u;
    unsigned value = short_year ? (unsigned)state->year % 100u : (unsigned)state->year;
    return fmt_excel_append_unsigned(state->out, value, short_year ? 2u : 4u);
}

static bool fx_calendar_month(fx_calendar_state_t *state, const fmt_excel_token_t *token)
{
    if (token->length <= 2u)
        return fmt_excel_append_unsigned(state->out, state->month, token->length);
    return fx_calendar_name(state->out, &fx_months[state->month - 1u], token->length);
}

static bool fx_calendar_day(fx_calendar_state_t *state, const fmt_excel_token_t *token)
{
    if (token->length <= 2u)
        return fmt_excel_append_unsigned(state->out, state->day, token->length);
    return fx_calendar_name(state->out, &fx_weekdays[state->weekday], token->length);
}

static bool fx_calendar_ordinal(fx_calendar_state_t *state, const fmt_excel_token_t *token)
{
    return fmt_excel_append_unsigned(state->out, state->day, token->length) &&
           string_append_cstr(state->out, fx_day_suffix(state->day, (token->flags & FX_SUPERSCRIPT) != 0u)) == 0;
}

static bool fx_calendar_hour(fx_calendar_state_t *state, const fmt_excel_token_t *token)
{
    unsigned hour = state->section->ampm ? state->hour % 12u : state->hour;
    if (state->section->ampm && !hour)
        hour = 12u;
    return fmt_excel_append_unsigned(state->out, hour, token->length);
}

static bool fx_calendar_minute(fx_calendar_state_t *state, const fmt_excel_token_t *token)
{
    return fmt_excel_append_unsigned(state->out, state->minute, token->length);
}

static bool fx_calendar_second(fx_calendar_state_t *state, const fmt_excel_token_t *token)
{
    return fmt_excel_append_unsigned(state->out, state->second, token->length);
}

static bool fx_calendar_period(fx_calendar_state_t *state, const fmt_excel_token_t *token)
{
    static const char *const periods[2][2] = {{"am", "pm"}, {"AM", "PM"}};
    const char *text = periods[(token->flags & FX_UPPER_PERIOD) != 0u][state->hour >= 12u];
    return string_append_bytes(state->out, text, token->length == 3u ? 1u : 2u) == 0;
}

static bool fx_calendar_elapsed(fx_calendar_state_t *state, const fmt_excel_token_t *token)
{
    static const unsigned divisors[FX_KIND_COUNT] = {
        [FX_ELAPSED_HOUR]   = 3600u,
        [FX_ELAPSED_MINUTE] = 60u,
        [FX_ELAPSED_SECOND] = 1u,
    };
    long double value = floorl(state->elapsed / divisors[token->kind]);
    if (value < 0.0L || value > (long double)UINT64_MAX) {
        state->range_error = true;
        return fmt_error_set(state->error,
            FMT_ERROR_RANGE,
            SIZE_MAX,
            SIZE_MAX,
            "elapsed time exceeds the supported range");
    }
    return fmt_excel_append_unsigned(state->out, (uint64_t)value, token->length);
}

static bool fx_calendar_subsecond(fx_calendar_state_t *state, const fmt_excel_token_t *token)
{
    return string_append_char(state->out, '.') == 0 &&
           (!token->length || fmt_excel_append_unsigned(state->out, state->fractional, token->length));
}

static bool fx_calendar_long_date(fx_calendar_state_t *state, const fmt_excel_token_t *token)
{
    (void)token;
    const fx_calendar_name_t *weekday = &fx_weekdays[state->weekday], *month = &fx_months[state->month - 1u];
    return string_append_bytes(state->out, weekday->text, weekday->length) == 0 &&
           string_append_cstr(state->out, ", ") == 0 && string_append_bytes(state->out, month->text, month->length) == 0 &&
           string_append_char(state->out, ' ') == 0 && fmt_excel_append_unsigned(state->out, state->day, 1u) &&
           string_append_cstr(state->out, ", ") == 0 && fmt_excel_append_unsigned(state->out, (unsigned)state->year, 4u);
}

static bool fx_calendar_long_time(fx_calendar_state_t *state, const fmt_excel_token_t *token)
{
    (void)token;
    return fmt_excel_append_unsigned(state->out, state->hour, 2u) && string_append_char(state->out, ':') == 0 &&
           fmt_excel_append_unsigned(state->out, state->minute, 2u) && string_append_char(state->out, ':') == 0 &&
           fmt_excel_append_unsigned(state->out, state->second, 2u);
}

static const fx_calendar_fn fx_calendar_dispatch[FX_KIND_COUNT] = {
    [FX_YEAR]           = fx_calendar_year,
    [FX_MONTH]          = fx_calendar_month,
    [FX_DAY]            = fx_calendar_day,
    [FX_DAY_ORDINAL]    = fx_calendar_ordinal,
    [FX_HOUR]           = fx_calendar_hour,
    [FX_MINUTE]         = fx_calendar_minute,
    [FX_SECOND]         = fx_calendar_second,
    [FX_AMPM]           = fx_calendar_period,
    [FX_ELAPSED_HOUR]   = fx_calendar_elapsed,
    [FX_ELAPSED_MINUTE] = fx_calendar_elapsed,
    [FX_ELAPSED_SECOND] = fx_calendar_elapsed,
    [FX_LONG_DATE]      = fx_calendar_long_date,
    [FX_LONG_TIME]      = fx_calendar_long_time,
    [FX_SUBSECOND]      = fx_calendar_subsecond,
};

static string_t *fx_calendar_render(const fmt_options_t *options, const fmt_excel_section_t *section, int year,
                                    unsigned month, unsigned day, long double serial, unsigned weekday,
                                    unsigned hour, unsigned minute, long double second, fmt_error_t *error)
{
    const fmt_excel_t *plan = options->excel;
    unsigned precision = section->second_precision;
    if (precision > 9u) {
        fmt_error_set(error, FMT_ERROR_RANGE, SIZE_MAX, SIZE_MAX, "date fractional seconds support at most nine places");
        return NULL;
    }
    uint64_t scale = 1u;
    for (unsigned i = 0u; i < precision; ++i)
        scale *= 10u;
    long double rounded = floorl(second * (long double)scale + 0.5L) / (long double)scale;
    if (rounded >= 60.0L) {
        rounded = 0.0L;
        if (++minute == 60u) {
            minute = 0u;
            if (++hour == 24u) {
                hour = 0u;
                int64_t days = fx_civil_days(year, month, day) + 1;
                fx_days_civil(days, &year, &month, &day);
                weekday = (weekday + 1u) % 7u;
            }
        }
    }
    if (year > 9999)
        return string_new_with("########");
    uint64_t fractional = (uint64_t)floorl((rounded - floorl(rounded)) * (long double)scale + 0.5L);
    long double total = floorl(serial * 86400.0L * (long double)scale + 0.5L) / (long double)scale;
    fx_calendar_state_t state = {.plan = plan, .section = section, .out = string_new(), .error = error,
        .year = year, .month = month, .day = day, .weekday = weekday, .hour = hour, .minute = minute,
        .second = (uint64_t)rounded, .fractional = fractional, .elapsed = total};
    size_t end = section->first + section->count;
    bool ok = state.out != NULL;
    for (size_t i = section->first; ok && i < end;) {
        const fmt_excel_token_t *token = &plan->tokens[i];
        fx_calendar_fn render = fx_calendar_dispatch[token->kind];
        ok = (render ? render : fx_calendar_literal)(&state, token);
        i = token->next;
    }
    if (!ok) {
        string_free(state.out);
        if (!state.range_error)
            fmt_error_set(error, FMT_ERROR_MEMORY, SIZE_MAX, SIZE_MAX, "cannot allocate Excel date output");
        return NULL;
    }
    return fmt_excel_finish(state.out, plan, section, options, state.fill);

}

string_t *fmt_excel_calendar(const fmt_options_t *options,
    const fmt_excel_section_t *section,
    long double serial,
    fmt_error_t *error)
{
    if (!isfinite(serial) || serial < 0.0L || serial >= 2958466.0L)
        return string_new_with("########");
    unsigned precision = section->second_precision;
    long double scale = powl(10.0L, (long double)(precision > 9u ? 9u : precision));
    long double ticks = floorl(serial * 86400.0L * scale + 0.5L);
    long double day_ticks = 86400.0L * scale;
    int64_t whole = (int64_t)floorl(ticks / day_ticks);
    int year;
    unsigned month, day;
    if (whole == 60) {
        year = 1900;
        month = 2u;
        day = 29u;
    } else if (whole == 0) {
        year = 1900;
        month = 1u;
        day = 0u;
    } else {
        int64_t days = fx_civil_days(1899, 12u, 31u) + whole - (whole > 60);
        fx_days_civil(days, &year, &month, &day);
    }
    long double time = (ticks - (long double)whole * day_ticks) / scale;
    unsigned hour = (unsigned)floorl(time / 3600.0L);
    unsigned minute = (unsigned)floorl(time / 60.0L) % 60u;
    long double second = time - (long double)(hour * 3600u + minute * 60u);
    unsigned weekday = (unsigned)((whole + 6) % 7);
    return fx_calendar_render(options, section, year, month, day, serial, weekday, hour, minute, second, error);
}

/* Render a real Gregorian datetime without converting its fractional seconds through a serial-day double. */
string_t *fmt_excel_datetime(const fmt_options_t *options,
    int year,
    unsigned month,
    unsigned day,
    unsigned hour,
    unsigned minute,
    double second,
    fmt_error_t *error)
{
    static const unsigned month_days[12] = {31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u};
    bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    if (!options || !options->excel || year < 1 || year > 9999 || month < 1u || month > 12u ||
        day < 1u || day > month_days[month - 1u] + (month == 2u && leap) ||
        hour >= 24u || minute >= 60u || !isfinite(second) || second < 0.0 || second >= 60.0) {
        fmt_error_set(error, FMT_ERROR_ARGUMENT, SIZE_MAX, SIZE_MAX, "invalid Gregorian datetime components");
        return NULL;
    }
    const fmt_excel_t *plan = options->excel;
    const fmt_excel_section_t *section = &plan->section[0];
    if (!section->date) {
        fmt_error_set(error, FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "datetime requires calendar or time fields");
        return NULL;
    }
    int64_t days = fx_civil_days(year, month, day);
    int64_t epoch = fx_civil_days(1899, 12u, 31u);
    long double serial = (long double)(days - epoch) + (days >= fx_civil_days(1900, 3u, 1u)) +
                         ((long double)(hour * 3600u + minute * 60u) + second) / 86400.0L;
    int64_t weekday = (days - fx_civil_days(1970, 1u, 1u) + 4) % 7;
    if (weekday < 0)
        weekday += 7;
    string_t *out = fx_calendar_render(options, section, year, month, day, serial, (unsigned)weekday,
                                       hour, minute, second, error);
    return out;
}
