# `datetime_t`

`datetime_t` provides civil calendar utilities together with higher-level
astronomical helpers.

## Capabilities

- Gregorian calendar construction and field access
- Julian Day Number and floating-point Julian Day conversion
- date/time arithmetic (add/subtract days, weeks, months, years, hours, minutes, seconds, spans)
- comparison operators
- leap-year and days-in-month helpers
- DST and timezone-offset queries
- sunrise and sunset calculation
- moon phase classification and next-phase search
- weekday navigation
- holiday helpers: Easter (Anonymous Gregorian algorithm) and Chinese New Year (1700–2400)
- rich `printf`-style formatting

## Example: Chinese New Year

```c
#include <stdio.h>
#include "datetime.h"

int main(void) {
    int years[] = { 2020, 2021, 2022, 2023, 2024, 2025 };

    for (int i = 0; i < 6; i++) {
        datetime_t *dt = datetime_init_chinese_new_year(datetime_alloc(), years[i]);

        if (!dt) {
            printf("Year %d is outside the supported range.\n", years[i]);
            continue;
        }

        printf("Chinese New Year %d: %d-%02d-%02d\n",
               years[i],
               (int)datetime_year(dt),
               (int)datetime_month(dt),
               (int)datetime_day(dt));

        datetime_dealloc(dt);
    }
    return 0;
}
```

Expected output:

```text
Chinese New Year 2020: 2020-01-25
Chinese New Year 2021: 2021-02-12
Chinese New Year 2022: 2022-02-01
Chinese New Year 2023: 2023-01-22
Chinese New Year 2024: 2024-02-10
Chinese New Year 2025: 2025-01-29
```

## Example: Sunrise and Moon Phase

```c
#include <stdio.h>
#include "datetime.h"

int main(void) {
    /* Today at latitude 51.5 N, longitude -0.1 W (London), UTC+0 */
    datetime_t *today = datetime_init_now(datetime_alloc());
    long jdn = datetime_jdn(today);

    datetime_t *sr = datetime_init_sunrise(datetime_alloc(), jdn, 51.5, -0.1, 0.0);
    char *s = datetime_format(sr, "Sunrise: @hh:@mm:@ss");
    printf("%s\n", s);
    free(s);

    moon_phase_t phase = datetime_moon_phase(today);
    printf("Moon phase: %d\n", (int)phase);

    datetime_dealloc(sr);
    datetime_dealloc(today);
    return 0;
}
```

## Design Notes

### Civil Calendar Layer

Year, month, and day fields are integers. Julian Day Number is the canonical
interchange format — all constructors that set calendar fields also lazily
compute JDN on demand. Arithmetic on months handles variable month lengths and
leap years correctly, including edge cases such as adding one month to January 31.

### Astronomical Layer

Sunrise and sunset times are calculated using the NOAA Solar Calculator
algorithm (Jean Meeus, *Astronomical Algorithms*). Moon phases are approximated
by tracking the synodic cycle from a known reference epoch. These routines are
suitable for practical applications; they are not a full ephemeris engine.

### Ownership

`datetime_alloc()` returns a heap-allocated structure which the caller must
eventually pass to `datetime_dealloc()`. `datetime_next_moon_phase()` and
`datetime_next_weekday()` also return newly allocated structures that the
caller must free.

---

## API Reference

All declarations are in `include/datetime.h`.

### Enumerations

**`month_t`** — `DT_January` (1) through `DT_December` (12)

**`weekday_t`** — `DT_Sunday` (1) through `DT_Saturday` (7)

**`moon_phase_t`**

| Value | Name |
|---|---|
| `DT_NewMoon` | New moon |
| `DT_WaxingCrescent` | Waxing crescent |
| `DT_FirstQuarter` | First quarter |
| `DT_WaxingGibbous` | Waxing gibbous |
| `DT_FullMoon` | Full moon |
| `DT_WaningGibbous` | Waning gibbous |
| `DT_LastQuarter` | Last quarter |
| `DT_WaningCrescent` | Waning crescent |

**`datetime_span_t`** — duration or difference struct:

```c
typedef struct {
    unsigned short years;
    uint8_t months, days, hours, minutes;
    double seconds;
} datetime_span_t;
```

### Allocation

- `datetime_t *datetime_alloc()` — allocate an uninitialised `datetime_t`
- `void datetime_dealloc(datetime_t *dttm)` — free a `datetime_t`

### Constructors (init functions)

All `init` functions take a preallocated `datetime_t *` as their first argument,
initialise it, and return the same pointer (or NULL on error).

- `datetime_t *datetime_init_ymd(datetime_t *dttm, short year, month_t month, uint8_t day)` — from year/month/day
- `datetime_t *datetime_init_ymdt(datetime_t *dttm, short year, month_t month, uint8_t day, uint8_t hour, uint8_t minute, double second)` — from full date and time
- `datetime_t *datetime_init_copy(datetime_t *dest, const datetime_t *src)` — copy from another `datetime_t`
- `datetime_t *datetime_init_jdn(datetime_t *dttm, long JulianDayNumber)` — from Julian Day Number
- `datetime_t *datetime_init_jd(datetime_t *dttm, double JulianDay)` — from floating-point Julian Day
- `datetime_t *datetime_init_now(datetime_t *dttm)` — current local date and time
- `datetime_t *datetime_init_easter(datetime_t *dttm, int year)` — Easter Sunday for `year` (1–9999); returns NULL if year is out of range
- `datetime_t *datetime_init_chinese_new_year(datetime_t *dttm, int year)` — Chinese New Year for `year` (1700–2400); returns NULL if year is out of range

### Field Accessors

- `short datetime_year(const datetime_t *dttm)` — year; SHRT_MAX if uninitialised
- `month_t datetime_month(const datetime_t *dttm)` — month (1–12); 0 if uninitialised
- `uint8_t datetime_day(const datetime_t *dttm)` — day of month; 0 if uninitialised
- `uint8_t datetime_hour(const datetime_t *dttm)` — hour (0–23)
- `uint8_t datetime_minute(const datetime_t *dttm)` — minute (0–59)
- `double datetime_second(const datetime_t *dttm)` — second (0–59.xxx)
- `weekday_t datetime_weekday(const datetime_t *dttm)` — day of week (Sunday=1 … Saturday=7)

### Julian Day Utilities

- `long datetime_ymd_to_jdn(short year, month_t month, uint8_t day)` — compute JDN from calendar fields (free function)
- `long datetime_jdn(const datetime_t *dttm)` — JDN of a `datetime_t`
- `double datetime_jd(const datetime_t *dttm)` — floating-point Julian Day

### Calendar Helpers

- `bool datetime_is_leap_year(short year)` — true if `year` is a Gregorian leap year
- `unsigned short datetime_days_in_month(short year, month_t month)` — number of days in the given month
- `unsigned short datetime_days_in_this_month(const datetime_t *dttm)` — number of days in the month of `dttm`

### Timezone

- `double datetime_tz_offset(const datetime_t *dttm)` — local timezone offset in hours (DBL_MAX on failure)
- `datetime_t *datetime_to_gmt(datetime_t *dttm)` — convert in place to GMT/UTC; returns `dttm` or NULL on failure
- `bool datetime_is_dst(const datetime_t *dttm)` — true if the datetime falls in daylight saving time

### Comparison

- `bool datetime_equal(const datetime_t *dttm1, const datetime_t *dttm2)` — true if equal
- `bool datetime_lt(const datetime_t *dttm1, const datetime_t *dttm2)` — true if `dttm1 < dttm2`
- `bool datetime_le(const datetime_t *dttm1, const datetime_t *dttm2)` — true if `dttm1 <= dttm2`
- `bool datetime_gt(const datetime_t *dttm1, const datetime_t *dttm2)` — true if `dttm1 > dttm2`
- `bool datetime_ge(const datetime_t *dttm1, const datetime_t *dttm2)` — true if `dttm1 >= dttm2`
- `int datetime_compare(const datetime_t *dttm1, const datetime_t *dttm2)` — returns -1, 0, or 1

### Arithmetic (in-place, return `dttm`)

All arithmetic modifies `dttm` in place and returns `dttm` (or NULL on failure).
All counts may be negative (subtract by passing a negative value).

- `datetime_t *datetime_add_days(datetime_t *dttm, long days)`
- `datetime_t *datetime_add_weeks(datetime_t *dttm, int weeks)`
- `datetime_t *datetime_add_months(datetime_t *dttm, int months)` — clamped to valid month end
- `datetime_t *datetime_add_years(datetime_t *dttm, int years)` — handles Feb 29 edge case
- `datetime_t *datetime_add_hours(datetime_t *dttm, int hours)`
- `datetime_t *datetime_add_minutes(datetime_t *dttm, int minutes)`
- `datetime_t *datetime_add_seconds(datetime_t *dttm, double seconds)`
- `datetime_t *datetime_add_span(datetime_t *dttm, const datetime_span_t *span)` — add a compound span
- `datetime_t *datetime_sub_span(datetime_t *dttm, const datetime_span_t *span)` — subtract a compound span

### Duration

- `double datetime_duration(const datetime_t *dttm1, const datetime_t *dttm2, datetime_span_t *span)` — returns `dttm1 - dttm2` in days (positive if dttm1 is later); fills `*span` if non-NULL. Returns DBL_MAX on error.

### Hashing

- `unsigned int datetime_hash(const datetime_t *dttm)` — hash suitable for use as a hash-table key

### Formatting

- `char *datetime_format(const datetime_t *dttm, const char *format)` — format the datetime using a format string; returns a newly allocated C string that the caller must free. Returns NULL on allocation failure.

**Format placeholders:**

| Placeholder | Output |
|---|---|
| `%d` | day number, minimum digits |
| `%dd` | day number, always 2 digits |
| `%ddd` | abbreviated weekday, lowercase (mon, tue, …) |
| `%Ddd` | abbreviated weekday, title case (Mon, Tue, …) |
| `%DDD` | abbreviated weekday, uppercase (MON, TUE, …) |
| `%dddd` | full weekday, lowercase (monday, …) |
| `%Dddd` | full weekday, title case (Monday, …) |
| `%DDDD` | full weekday, uppercase (MONDAY, …) |
| `%o` | ordinal suffix, lowercase (st, nd, rd, th) |
| `%O` | ordinal suffix, uppercase (ST, ND, RD, TH) |
| `%m` | month number, minimum digits |
| `%mm` | month number, always 2 digits |
| `%mmm` | abbreviated month, lowercase (jan, …) |
| `%Mmm` | abbreviated month, title case (Jan, …) |
| `%MMM` | abbreviated month, uppercase (JAN, …) |
| `%mmmm` | full month, lowercase (january, …) |
| `%Mmmm` | full month, title case (January, …) |
| `%MMMM` | full month, uppercase (JANUARY, …) |
| `%yy` | 2-digit year (97) |
| `%yyyy` | 4-digit year (1997) |
| `@h` | hour, minimum digits |
| `@hh` | hour, always 2 digits (12-hour) |
| `@Hh` | hour, always 2 digits (24-hour) |
| `@m` | minute, minimum digits |
| `@mm` | minute, always 2 digits |
| `@s` | second, minimum digits |
| `@ss` | second, always 2 digits |
| `@p` | am/pm |
| `@P` | AM/PM |
| `%%` | literal `%` |
| `@@` | literal `@` |
| `^` | void character (suppresses adjacent separator) |

### Sunrise and Sunset

- `double datetime_sun_time(long julianDayNumber, double latitude, double longitude, bool isSunrise)` — sunrise or sunset time in GMT as decimal hours (e.g. 6.5 = 06:30). Returns -1.0 if the sun never rises, -2.0 if the sun never sets at that location on that date.
- `datetime_t *datetime_init_sunrise(datetime_t *dttm, long jdn, double latitude, double longitude, double timeZoneOffset)` — initialise `dttm` with the sunrise time for the given JDN and location (adjusted to local time)
- `datetime_t *datetime_init_sunset(datetime_t *dttm, long jdn, double latitude, double longitude, double timeZoneOffset)` — initialise `dttm` with the sunset time
- `void datetime_set_sunrise(datetime_t *dttm, double latitude, double longitude, double timeZoneOffset)` — set the time components of an existing `datetime_t` to the sunrise for its current date
- `void datetime_set_sunset(datetime_t *dttm, double latitude, double longitude, double timeZoneOffset)` — set the time components to sunset

### Moon Phases

- `moon_phase_t datetime_moon_phase(const datetime_t *dttm)` — return the moon phase for the date
- `datetime_t *datetime_next_moon_phase(const datetime_t *dttm, moon_phase_t phase)` — return a newly allocated `datetime_t` for the next occurrence of `phase` after `dttm`; caller must free. Returns NULL if `dttm` is uninitialised.

### Weekday Navigation

- `datetime_t *datetime_next_weekday(const datetime_t *dttm, weekday_t weekday)` — return a newly allocated `datetime_t` for the next occurrence of `weekday` after `dttm`; caller must free. Returns NULL if `dttm` is uninitialised.
