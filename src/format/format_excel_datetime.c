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

static const char *const fx_months[12] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December",
};
static const char *const fx_weekdays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

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

static bool fx_calendar_integer(string_t *out, uint64_t value, size_t minimum)
{
    char digits[32];
    size_t count = 0u;
    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value);
    while (count < minimum && count < sizeof(digits))
        digits[count++] = '0';
    while (count) {
        if (string_append_char(out, digits[--count]) != 0)
            return false;
    }
    return true;
}

static unsigned fx_second_precision(const fmt_excel_t *plan, const fmt_excel_section_t *section)
{
    unsigned precision = 0u;
    size_t end = section->first + section->count;
    for (size_t i = section->first; i < end; ++i) {
        if (plan->tokens[i].kind != FX_DOT)
            continue;
        unsigned count = 0u;
        while (i + 1u < end && plan->tokens[i + 1u].kind == FX_DIGIT) {
            ++count;
            ++i;
        }
        if (count > precision)
            precision = count;
    }
    return precision;
}

static string_t *fx_calendar_render(const fmt_options_t *options, const fmt_excel_section_t *section, int year,
                                    unsigned month, unsigned day, long double serial, unsigned weekday,
                                    unsigned hour, unsigned minute, long double second, fmt_error_t *error)
{
    const fmt_excel_t *plan = options->excel;
    fmt_excel_fill_t fill = {0};
    unsigned precision = fx_second_precision(plan, section);
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
    string_t *out = string_new();
    size_t end = section->first + section->count;
    bool ok = out != NULL;
    for (size_t i = section->first; ok && i < end; ++i) {
        const fmt_excel_token_t *token = &plan->tokens[i];
        size_t count = token->length;
        switch (token->kind) {
            case FX_YEAR:
                ok = fx_calendar_integer(out, count <= 2u ? (unsigned)year % 100u : (unsigned)year,
                                          count <= 2u ? 2u : 4u);
                break;
            case FX_MONTH:
                if (count <= 2u)
                    ok = fx_calendar_integer(out, month, count);
                else
                    ok = string_append_bytes(out, fx_months[month - 1u], count == 3u ? 3u : count == 5u ? 1u :
                                              strlen(fx_months[month - 1u])) == 0;
                break;
            case FX_DAY:
                if (count <= 2u)
                    ok = fx_calendar_integer(out, day, count);
                else
                    ok = string_append_bytes(out, fx_weekdays[weekday], count == 3u ? 3u : strlen(fx_weekdays[weekday])) == 0;
                break;
            case FX_DAY_ORDINAL:
                ok = fx_calendar_integer(out, day, count) &&
                     string_append_cstr(out, fx_day_suffix(day, plan->code[token->start + count + 1u] == '^')) == 0;
                break;
            case FX_HOUR: {
                unsigned shown = section->ampm ? hour % 12u : hour;
                ok = fx_calendar_integer(out, section->ampm && !shown ? 12u : shown, count);
                break;
            }
            case FX_MINUTE:
                ok = fx_calendar_integer(out, minute, count);
                break;
            case FX_SECOND:
                ok = fx_calendar_integer(out, (uint64_t)rounded, count);
                break;
            case FX_AMPM: {
                bool lower = plan->code[token->start] == 'a';
                const char *period = hour < 12u ? (lower ? "am" : "AM") : (lower ? "pm" : "PM");
                ok = string_append_bytes(out, period, count == 3u ? 1u : 2u) == 0;
                break;
            }
            case FX_ELAPSED_HOUR:
            case FX_ELAPSED_MINUTE:
            case FX_ELAPSED_SECOND: {
                long double divisor = token->kind == FX_ELAPSED_HOUR ? 3600.0L :
                                      token->kind == FX_ELAPSED_MINUTE ? 60.0L : 1.0L;
                long double value = floorl(total / divisor);
                if (value < 0.0L || value > (long double)UINT64_MAX) {
                    string_free(out);
                    fmt_error_set(error, FMT_ERROR_RANGE, SIZE_MAX, SIZE_MAX, "elapsed time exceeds the supported range");
                    return NULL;
                }
                ok = fx_calendar_integer(out, (uint64_t)value, count);
                break;
            }
            case FX_DOT: {
                unsigned places = 0u;
                while (i + 1u < end && plan->tokens[i + 1u].kind == FX_DIGIT) {
                    ++i;
                    ++places;
                }
                ok = string_append_char(out, '.') == 0;
                if (places)
                    ok = ok && fx_calendar_integer(out, fractional, places);
                break;
            }
            case FX_LONG_DATE:
                ok = string_append_cstr(out, fx_weekdays[weekday]) == 0 && string_append_cstr(out, ", ") == 0 &&
                     string_append_cstr(out, fx_months[month - 1u]) == 0 && string_append_char(out, ' ') == 0 &&
                     fx_calendar_integer(out, day, 1u) && string_append_cstr(out, ", ") == 0 &&
                     fx_calendar_integer(out, (unsigned)year, 4u);
                break;
            case FX_LONG_TIME:
                ok = fx_calendar_integer(out, hour, 2u) && string_append_char(out, ':') == 0 &&
                     fx_calendar_integer(out, minute, 2u) && string_append_char(out, ':') == 0 &&
                     fx_calendar_integer(out, (uint64_t)rounded, 2u);
                break;
            default:
                ok = fmt_excel_literal(out, plan, token, &fill);
                break;
        }
    }
    if (!ok) {
        string_free(out);
        fmt_error_set(error, FMT_ERROR_MEMORY, SIZE_MAX, SIZE_MAX, "cannot allocate Excel date output");
        return NULL;
    }
    return fmt_excel_finish(out, plan, section, options, fill);
}

string_t *fmt_excel_calendar(const fmt_options_t *options, const fmt_excel_section_t *section,
                            long double serial, fmt_error_t *error)
{
    if (!isfinite(serial) || serial < 0.0L || serial >= 2958466.0L)
        return string_new_with("########");
    unsigned precision = fx_second_precision(options->excel, section);
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
string_t *fmt_excel_datetime(const fmt_options_t *options, int year, unsigned month, unsigned day,
                             unsigned hour, unsigned minute, double second, fmt_error_t *error)
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
