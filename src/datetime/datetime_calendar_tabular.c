/* Tabular Christian, Islamic, Jewish and Ethiopian calendars and observances. */
#include <float.h>
#include <limits.h>
#include <math.h>

#include "datetime_internal.h"
#include "ustring.h"

static long datetime_ethiopian_ymd_to_jdn(int year, int month, int day);

/* Calculate the date of Easter Sunday for a given year and initialise a datetime structure with that date. */
datetime_t *datetime_init_easter(datetime_t *dttm, int year)
{
    if (year < 1 || year > 9999)
        return NULL;

    int goldenNumber = year % 19;
    int daysIntoYear;

    if (year < 1583) {
        int posIn4YearLeapCycle = year % 4;
        int weekdayCycle = year % 7;
        int paschalFullMoon = (19 * goldenNumber + 15) % 30;
        int weekdayOffset = (2 * posIn4YearLeapCycle + 4 * weekdayCycle - paschalFullMoon + 34) % 7;
        daysIntoYear = paschalFullMoon + weekdayOffset + 114;
    } else {
        int century = year / 100;
        int yearInCentury = year % 100;
        int centuryLeapCorrections = century / 4;
        int priorLeapRemainder = century % 4;
        int gregorianCorrection = (century + 8) / 25;
        int leapSkipAdjustment = (century - gregorianCorrection + 1) / 3;
        int epact = (19 * goldenNumber + century - centuryLeapCorrections - leapSkipAdjustment + 15) % 30;
        int yearLeapCorrections = yearInCentury / 4;
        int leapOffset = yearInCentury % 4;
        int daysFromFullMoonToSunday = (32 + 2 * priorLeapRemainder + 2 * yearLeapCorrections - epact - leapOffset) % 7;
        int easterMonthAdjust = (goldenNumber + 11 * epact + 22 * daysFromFullMoonToSunday) / 451;
        daysIntoYear = epact + daysFromFullMoonToSunday - 7 * easterMonthAdjust + 114;
    }

    dttm->year = (short)year;
    dttm->month = (month_t)(daysIntoYear / 31);
    dttm->day = (uint8_t)((daysIntoYear % 31) + 1);
    dttm->hour = 0;
    dttm->minute = 0;
    dttm->second = 0.0;
    dttm->JulianDay = DBL_MAX;
    dttm->JulianDayNumber = LONG_MAX;

    return dttm;
}

static long datetime_julian_ymd_to_jdn(int year, int month, int day)
{
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    return day + (153 * m + 2) / 5 + 365 * y + y / 4 - 32083;
}

/* Initialise a datetime with Orthodox Easter Sunday. */
datetime_t *datetime_init_orthodox_easter(datetime_t *dttm, int year)
{
    int a;
    int b;
    int c;
    int d;
    int e;
    int daysIntoYear;
    int month;
    int day;

    if (!dttm || year < 1 || year > 9999)
        return NULL;

    a = year % 4;
    b = year % 7;
    c = year % 19;
    d = (19 * c + 15) % 30;
    e = (2 * a + 4 * b - d + 34) % 7;
    daysIntoYear = d + e + 114;
    month = daysIntoYear / 31;
    day = (daysIntoYear % 31) + 1;

    datetime_init_jdn(dttm, datetime_julian_ymd_to_jdn(year, month, day));
    datetime_year(dttm);
    return dttm;
}

/* Initialise a datetime with Christmas Day in the Gregorian calendar. */
datetime_t *datetime_init_christmas(datetime_t *dttm, int year)
{
    if (!dttm || year < 1 || year > 9999)
        return NULL;

    return datetime_init_ymd(dttm, (short)year, DT_December, 25);
}

/* Initialise a datetime with Orthodox Christmas Day observed in a Gregorian civil year. */
datetime_t *datetime_init_orthodox_christmas(datetime_t *dttm, int year)
{
    if (!dttm || year < 2 || year > 9999)
        return NULL;

    datetime_init_jdn(dttm, datetime_julian_ymd_to_jdn(year - 1, DT_December, 25));
    datetime_year(dttm);
    return dttm;
}

static const long islamic_civil_epoch = 1948440L;

static long datetime_islamic_ymd_to_jdn(int year, int month, int day)
{
    return day + (long)ceil(29.5 * (month - 1)) + (year - 1) * 354L + (3 + 11 * year) / 30 + islamic_civil_epoch - 1;
}

static datetime_t *datetime_init_civil_islamic_observance(datetime_t *dttm, int gregorianYear, int islamicMonth,
    int islamicDay)
{
    int estimateIslamicYear;

    if (!dttm || gregorianYear < 1 || gregorianYear > 9999)
        return NULL;

    estimateIslamicYear = (int)floor(((gregorianYear - 622) * 33.0) / 32.0) + 1;
    for (int islamicYear = estimateIslamicYear - 2; islamicYear <= estimateIslamicYear + 2; islamicYear++) {
        long jdn;
        datetime_t probe;

        if (islamicYear < 1)
            continue;

        jdn = datetime_islamic_ymd_to_jdn(islamicYear, islamicMonth, islamicDay);
        datetime_init_jdn(&probe, jdn);
        datetime_year(&probe);
        if (probe.year == gregorianYear)
            return datetime_init_materialised_jdn(dttm, jdn);
    }

    return NULL;
}

/* Initialise a datetime with the first day of Ramadan in the civil Islamic calendar. */
datetime_t *datetime_init_ramadan(datetime_t *dttm, int year)
{
    return datetime_init_civil_islamic_observance(dttm, year, 9, 1);
}

/* Initialise a datetime with Eid al-Fitr in the civil Islamic calendar. */
datetime_t *datetime_init_eid_al_fitr(datetime_t *dttm, int year)
{
    return datetime_init_civil_islamic_observance(dttm, year, 10, 1);
}

/* Initialise a datetime with Muslim New Year in the civil Islamic calendar. */
datetime_t *datetime_init_muslim_new_year(datetime_t *dttm, int year)
{
    return datetime_init_civil_islamic_observance(dttm, year, 1, 1);
}

static long datetime_hebrew_elapsed_days(int year)
{
    long months_elapsed = (235L * year - 234L) / 19L;
    long parts_elapsed = 12084L + 13753L * months_elapsed;
    long day = 29L * months_elapsed + parts_elapsed / 25920L;

    if ((3L * (day + 1L)) % 7L < 3L)
        day++;

    return day;
}

static long datetime_hebrew_new_year_jdn(int year)
{
    long day = datetime_hebrew_elapsed_days(year);

    /* Exceptional postponements prevent common years of 356 days and leap years of 382 days. */
    if (datetime_hebrew_elapsed_days(year + 1) - day == 356L)
        day += 2;
    else if (day - datetime_hebrew_elapsed_days(year - 1) == 382L)
        day++;

    return 347998L + day;
}

/* Initialise a datetime with Rosh Hashanah, the Jewish New Year. */
datetime_t *datetime_init_jewish_new_year(datetime_t *dttm, int year)
{
    if (!dttm || year < 1 || year > 9999)
        return NULL;

    return datetime_init_materialised_jdn(dttm, datetime_hebrew_new_year_jdn(year + 3761));
}

/* Initialise a datetime with Passover, Nisan 15 in the Jewish calendar. */
datetime_t *datetime_init_passover(datetime_t *dttm, int year)
{
    if (!dttm || year < 1 || year > 9999)
        return NULL;

    return datetime_init_materialised_jdn(dttm, datetime_hebrew_new_year_jdn(year + 3761) - 163L);
}

static void datetime_jdn_to_julian_ymd(long jdn, int *year, int *month, int *day)
{
    long c = jdn + 32082L;
    long d = (4L * c + 3L) / 1461L;
    long e = c - (1461L * d) / 4L;
    long m = (5L * e + 2L) / 153L;

    *day = (int)(e - (153L * m + 2L) / 5L + 1L);
    *month = (int)(m + 3L - 12L * (m / 10L));
    *year = (int)(d - 4800L + m / 10L);
}

/* Format the selected date in the Christian civil calendar systems. */
string_t *datetime_christian_calendar_date_text(const datetime_t *dttm)
{
    string_t *format;
    string_t *gregorian;
    string_t *out;
    int julianYear;
    int julianMonth;
    int julianDay;

    if (!dttm || datetime_jdn(dttm) == LONG_MAX)
        return NULL;

    format = string_new_with("%yyyy-%mm-%dd");
    gregorian = format ? datetime_format_text(dttm, format) : NULL;
    string_free(format);
    if (!gregorian)
        return NULL;

    datetime_jdn_to_julian_ymd(datetime_jdn(dttm), &julianYear, &julianMonth, &julianDay);
    out = string_new();
    if (!out || string_append_format(out, "Gregorian %s; Julian %04d-%02d-%02d", string_c_str(gregorian), julianYear,
                                     julianMonth, julianDay) < 0) {
        string_free(gregorian);
        string_free(out);
        return NULL;
    }

    string_free(gregorian);
    return out;
}

static void datetime_jdn_to_islamic_ymd(long jdn, int *year, int *month, int *day)
{
    *year = (int)((30L * (jdn - islamic_civil_epoch) + 10646L) / 10631L);
    if (*year < 1)
        *year = 1;

    *month = (int)ceil((jdn - (29L + datetime_islamic_ymd_to_jdn(*year, 1, 1))) / 29.5) + 1;
    if (*month < 1)
        *month = 1;
    if (*month > 12)
        *month = 12;

    *day = (int)(jdn - datetime_islamic_ymd_to_jdn(*year, *month, 1) + 1L);
    while (*day < 1) {
        (*month)--;
        if (*month < 1) {
            (*year)--;
            *month = 12;
        }
        *day = (int)(jdn - datetime_islamic_ymd_to_jdn(*year, *month, 1) + 1L);
    }
    while (*month < 12 && jdn >= datetime_islamic_ymd_to_jdn(*year, *month + 1, 1)) {
        (*month)++;
        *day = (int)(jdn - datetime_islamic_ymd_to_jdn(*year, *month, 1) + 1L);
    }
}

/* Format the selected date in the civil Islamic calendar. */
string_t *datetime_muslim_calendar_date_text(const datetime_t *dttm)
{
    static const char *monthNames[] = {
        NULL,    "Muharram", "Safar",   "Rabi al-awwal", "Rabi al-thani", "Jumada al-awwal", "Jumada al-thani",
        "Rajab", "Sha'ban",  "Ramadan", "Shawwal",       "Dhu al-Qadah",  "Dhu al-Hijjah"};
    int year;
    int month;
    int day;
    string_t *out;

    if (!dttm || datetime_jdn(dttm) == LONG_MAX)
        return NULL;

    datetime_jdn_to_islamic_ymd(datetime_jdn(dttm), &year, &month, &day);
    out = string_new();
    if (!out || string_append_format(out, "%d %s %d AH", day, monthNames[month], year) < 0) {
        string_free(out);
        return NULL;
    }
    return out;
}

static bool datetime_hebrew_leap_year(int year)
{
    return ((7 * year + 1) % 19) < 7;
}

static datetime_t *datetime_init_civil_ethiopian_observance(datetime_t *dttm, int gregorianYear, int ethiopianMonth,
    int ethiopianDay)
{
    datetime_t probe;
    int estimateEthiopianYear;

    if (!dttm || gregorianYear < 1 || gregorianYear > 9999)
        return NULL;

    estimateEthiopianYear = gregorianYear - 8;
    for (int ethiopianYear = estimateEthiopianYear - 1; ethiopianYear <= estimateEthiopianYear + 1; ethiopianYear++) {
        long jdn = datetime_ethiopian_ymd_to_jdn(ethiopianYear, ethiopianMonth, ethiopianDay);
        datetime_init_jdn(&probe, jdn);
        if (datetime_year(&probe) == gregorianYear)
            return datetime_init_materialised_jdn(dttm, jdn);
    }
    return NULL;
}

/* Initialise a datetime with Ethiopian New Year (Enkutatash). */
datetime_t *datetime_init_ethiopian_new_year(datetime_t *dttm, int year)
{
    return datetime_init_civil_ethiopian_observance(dttm, year, 1, 1);
}

/* Initialise a datetime with Genna, Ethiopian Christmas. */
datetime_t *datetime_init_genna(datetime_t *dttm, int year)
{
    return datetime_init_civil_ethiopian_observance(dttm, year, 4, 29);
}

/* Initialise a datetime with Timkat. */
datetime_t *datetime_init_timkat(datetime_t *dttm, int year)
{
    return datetime_init_civil_ethiopian_observance(dttm, year, 5, 11);
}

/* Initialise a datetime with Meskel. */
datetime_t *datetime_init_meskel(datetime_t *dttm, int year)
{
    return datetime_init_civil_ethiopian_observance(dttm, year, 1, 17);
}

/* Initialise a datetime with Fasika, Ethiopian Easter. */
datetime_t *datetime_init_fasika(datetime_t *dttm, int year)
{
    return datetime_init_orthodox_easter(dttm, year);
}

static int datetime_hebrew_year_length(int year)
{
    return (int)(datetime_hebrew_new_year_jdn(year + 1) - datetime_hebrew_new_year_jdn(year));
}

static bool datetime_hebrew_cheshvan_long(int year)
{
    return datetime_hebrew_year_length(year) % 10 == 5;
}

static bool datetime_hebrew_kislev_short(int year)
{
    return datetime_hebrew_year_length(year) % 10 == 3;
}

static int datetime_hebrew_month_length(int year, int monthIndexFromTishrei)
{
    static const int commonLengths[] = {30, 29, 30, 29, 30, 29, 30, 29, 30, 29, 30, 29};
    static const int leapLengths[] = {30, 29, 30, 29, 30, 30, 29, 30, 29, 30, 29, 30, 29};

    if (monthIndexFromTishrei == 1 && datetime_hebrew_cheshvan_long(year))
        return 30;
    if (monthIndexFromTishrei == 2 && datetime_hebrew_kislev_short(year))
        return 29;

    if (datetime_hebrew_leap_year(year))
        return leapLengths[monthIndexFromTishrei];
    return commonLengths[monthIndexFromTishrei];
}

static const char *datetime_hebrew_month_name(int year, int monthIndexFromTishrei)
{
    static const char *commonNames[] = {"Tishrei", "Cheshvan", "Kislev", "Tevet",  "Shevat", "Adar",
                                        "Nisan",   "Iyar",     "Sivan",  "Tammuz", "Av",     "Elul"};
    static const char *leapNames[] = {"Tishrei", "Cheshvan", "Kislev", "Tevet",  "Shevat", "Adar I", "Adar II",
                                      "Nisan",   "Iyar",     "Sivan",  "Tammuz", "Av",     "Elul"};

    if (datetime_hebrew_leap_year(year))
        return leapNames[monthIndexFromTishrei];
    return commonNames[monthIndexFromTishrei];
}

/* Format the selected date in the Jewish calendar. */
string_t *datetime_jewish_calendar_date_text(const datetime_t *dttm)
{
    long jdn;
    long newYearJdn;
    int hebrewYear;
    int dayOfYear;
    int monthIndex;
    int monthCount;
    int day;
    string_t *out;

    if (!dttm || datetime_jdn(dttm) == LONG_MAX)
        return NULL;

    jdn = datetime_jdn(dttm);
    hebrewYear = datetime_year(dttm) + 3760;
    while (jdn >= datetime_hebrew_new_year_jdn(hebrewYear + 1))
        hebrewYear++;
    while (jdn < datetime_hebrew_new_year_jdn(hebrewYear))
        hebrewYear--;

    newYearJdn = datetime_hebrew_new_year_jdn(hebrewYear);
    dayOfYear = (int)(jdn - newYearJdn) + 1;
    monthCount = datetime_hebrew_leap_year(hebrewYear) ? 13 : 12;
    monthIndex = 0;
    while (monthIndex < monthCount) {
        int monthLength = datetime_hebrew_month_length(hebrewYear, monthIndex);
        if (dayOfYear <= monthLength)
            break;
        dayOfYear -= monthLength;
        monthIndex++;
    }
    if (monthIndex >= monthCount)
        return NULL;
    day = dayOfYear;

    out = string_new();
    if (!out || string_append_format(out, "%d %s %d AM", day, datetime_hebrew_month_name(hebrewYear, monthIndex),
                                     hebrewYear) < 0) {
        string_free(out);
        return NULL;
    }
    return out;
}

static long datetime_ethiopian_ymd_to_jdn(int year, int month, int day)
{
    return 1724221L + 365L * (long)(year - 1) + (long)((year - 1) / 4) + 30L * (long)(month - 1) + (long)day - 1L;
}

static void datetime_jdn_to_ethiopian_ymd(long jdn, int *year, int *month, int *day)
{
    int estimate;
    long year_start;

    estimate = (int)((jdn - 1724221L) / 366L) + 1;
    if (estimate < 1)
        estimate = 1;

    while (datetime_ethiopian_ymd_to_jdn(estimate + 1, 1, 1) <= jdn)
        estimate++;
    while (datetime_ethiopian_ymd_to_jdn(estimate, 1, 1) > jdn)
        estimate--;

    year_start = datetime_ethiopian_ymd_to_jdn(estimate, 1, 1);
    *year = estimate;
    *month = (int)((jdn - year_start) / 30L) + 1;
    *day = (int)(jdn - datetime_ethiopian_ymd_to_jdn(*year, *month, 1) + 1L);
}

/* Format the selected date in the Ethiopian calendar. */
string_t *datetime_ethiopian_calendar_date_text(const datetime_t *dttm)
{
    static const char *month_names[] = {NULL,      "Meskerem", "Tikimt", "Hidar", "Tahsas", "Tir",     "Yekatit",
                                        "Megabit", "Miyazya",  "Genbot", "Sene",  "Hamle",  "Nehasse", "Pagume"};
    int year;
    int month;
    int day;
    string_t *out;

    if (!dttm || datetime_jdn(dttm) == LONG_MAX)
        return NULL;

    datetime_jdn_to_ethiopian_ymd(datetime_jdn(dttm), &year, &month, &day);
    if (month < 1 || month > 13)
        return NULL;

    out = string_new();
    if (!out || string_append_format(out, "%d %s %d EC", day, month_names[month], year) < 0) {
        string_free(out);
        return NULL;
    }
    return out;
}
