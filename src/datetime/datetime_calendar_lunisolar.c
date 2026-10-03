/* Chinese, Hindu and Buddhist calendar views and astronomical observances. */
#include <float.h>
#include <limits.h>
#include <math.h>

#include "datetime_internal.h"
#include "datetime_astronomy_internal.h"
#include "ustring.h"

static long datetime_india_new_moon_jdn_in_window(int year, month_t startMonth, uint8_t startDay, month_t endMonth,
    uint8_t endDay);

/* Estimate Chinese New Year for the supported 1700–2400 interval. */
static long datetime_chinese_new_year_jdn(int year)
{
    if (year < 1700 || year > 2400)
        return LONG_MAX;

    /* 1. December solstice of the previous year (Terrestrial Time) */
    double solsticeTerrestrialTime = datetime_dec_solstice_tt(year - 1);

    /* Convert solstice from Terrestrial Time to UTC */
    double deltaTPreviousYearDays = datetime_delta_t_estimate(year - 1) / 86400.0;
    double solsticeUTC = solsticeTerrestrialTime - deltaTPreviousYearDays;

    /* 2. Estimate lunation index for new moons around the solstice */
    int lunationIndex = (int)((solsticeUTC - 2451550.09765) / 29.530588853) - 2;

    /* 3. First new moon after the solstice */
    double firstNewMoonTerrestrial = datetime_true_new_moon_tt(lunationIndex);
    double firstNewMoonUTC = firstNewMoonTerrestrial - deltaTPreviousYearDays;

    while (firstNewMoonUTC < solsticeUTC) {
        lunationIndex++;
        firstNewMoonTerrestrial = datetime_true_new_moon_tt(lunationIndex);
        firstNewMoonUTC = firstNewMoonTerrestrial - deltaTPreviousYearDays;
    }

    /* 4. Second new moon = Chinese New Year */
    double secondNewMoonTerrestrial = datetime_true_new_moon_tt(lunationIndex + 1);
    double deltaTCurrentYearDays = datetime_delta_t_estimate(year) / 86400.0;
    double secondNewMoonUTC = secondNewMoonTerrestrial - deltaTCurrentYearDays;

    /* Convert UTC → China Standard Time (UTC+8) */
    double secondNewMoonCST = secondNewMoonUTC + (8.0 / 24.0);

    /* Round to nearest civil day in CST */
    return (long)floor(secondNewMoonCST + 0.5);
}

/* Initialise a datetime with the Chinese New Year date. */
datetime_t *datetime_init_chinese_new_year(datetime_t *dttm, int year)
{
    // algorithm not reliable for years before 1700 or after 2400
    if (year < 1700 || year > 2400)
        return NULL;

    dttm->JulianDayNumber = datetime_chinese_new_year_jdn(year);
    dttm->hour = 0;
    dttm->minute = 0;
    dttm->second = 0.0;
    dttm->JulianDay = DBL_MAX;

    datetime_year(dttm);

    return dttm;
}

static bool datetime_lunar_month_contains_principal_term(long startJdn, long endJdn)
{
    double startLongitude;
    double endLongitude;
    double travelled;
    int firstPrincipalTerm;

    if (startJdn == LONG_MAX || endJdn == LONG_MAX || startJdn >= endJdn)
        return false;

    startLongitude = datetime_solar_ecliptic_longitude((double)startJdn - 0.5);
    endLongitude = datetime_solar_ecliptic_longitude((double)endJdn - 0.5);
    travelled = datetime_normalise_degrees(endLongitude - startLongitude);
    firstPrincipalTerm = (int)floor(startLongitude / 30.0) + 1;

    return firstPrincipalTerm * 30.0 <= startLongitude + travelled + 0.25;
}

/* Format the selected date in the Chinese lunisolar calendar. */
string_t *datetime_chinese_calendar_date_text(const datetime_t *dttm)
{
    static const char *zodiac[] = {"Rat",   "Ox",   "Tiger",  "Rabbit",  "Dragon", "Snake",
                                   "Horse", "Goat", "Monkey", "Rooster", "Dog",    "Pig"};
    long starts[16];
    int gregorianYear;
    int chineseYearStart;
    int monthCount = 1;
    int currentMonthIndex = 0;
    int leapMonthIndex = -1;
    int lunarMonth;
    int lunarDay;
    int zodiacIndex;
    long jdn;
    long cny;
    long nextCny;
    string_t *out;

    if (!dttm)
        return NULL;

    gregorianYear = datetime_year(dttm);
    jdn = datetime_jdn(dttm);
    if (gregorianYear < 1700 || gregorianYear > 2400 || jdn == LONG_MAX)
        return NULL;

    chineseYearStart = gregorianYear;
    cny = datetime_chinese_new_year_jdn(chineseYearStart);
    if (cny == LONG_MAX)
        return NULL;
    if (jdn < cny) {
        chineseYearStart--;
        cny = datetime_chinese_new_year_jdn(chineseYearStart);
    }
    nextCny = datetime_chinese_new_year_jdn(chineseYearStart + 1);
    if (cny == LONG_MAX || nextCny == LONG_MAX || jdn < cny || jdn >= nextCny)
        return NULL;

    starts[0] = cny;
    while (monthCount < 15) {
        long nextStart = datetime_next_local_new_moon_jdn(starts[monthCount - 1], chineseYearStart, 8.0);
        if (nextStart == LONG_MAX || nextStart >= nextCny)
            break;
        starts[monthCount++] = nextStart;
    }

    if (nextCny - cny > 360L) {
        for (int i = 1; i < monthCount; i++) {
            long endJdn = (i + 1 < monthCount) ? starts[i + 1] : nextCny;
            if (!datetime_lunar_month_contains_principal_term(starts[i], endJdn)) {
                leapMonthIndex = i;
                break;
            }
        }
    }

    for (int i = 0; i < monthCount; i++) {
        long endJdn = (i + 1 < monthCount) ? starts[i + 1] : nextCny;
        if (jdn >= starts[i] && jdn < endJdn) {
            currentMonthIndex = i;
            break;
        }
    }

    lunarDay = (int)(jdn - starts[currentMonthIndex] + 1L);
    lunarMonth = currentMonthIndex + 1;
    if (leapMonthIndex >= 0) {
        if (currentMonthIndex == leapMonthIndex)
            lunarMonth = currentMonthIndex;
        else if (currentMonthIndex > leapMonthIndex)
            lunarMonth--;
    }
    zodiacIndex = (chineseYearStart - 4) % 12;
    if (zodiacIndex < 0)
        zodiacIndex += 12;

    out = string_new();
    if (!out ||
        string_append_format(out, "Year %d (%s), %smonth %d, day %d", chineseYearStart + 2698, zodiac[zodiacIndex],
                             currentMonthIndex == leapMonthIndex ? "leap " : "", lunarMonth, lunarDay) < 0) {
        string_free(out);
        return NULL;
    }
    return out;
}

static const char *datetime_hindu_month_name(int month)
{
    static const char *names[] = {NULL,      "Chaitra", "Vaishakha",    "Jyeshtha", "Ashadha", "Shravana", "Bhadrapada",
                                  "Ashvina", "Kartika", "Margashirsha", "Pausha",   "Magha",   "Phalguna"};

    return month >= 1 && month <= 12 ? names[month] : "Unknown";
}

/* Format the selected date in an Indian Hindu lunisolar calendar style. */
string_t *datetime_hindu_calendar_date_text(const datetime_t *dttm)
{
    long starts[16];
    long jdn;
    long newYearJdn;
    long nextYearJdn;
    int gregorianYear;
    int hinduYearStart;
    int monthCount = 1;
    int monthIndex = 0;
    int month;
    int lunarDay;
    int vikramSamvatYear;
    int tithi;
    const char *paksha;
    string_t *out;

    if (!dttm)
        return NULL;

    gregorianYear = datetime_year(dttm);
    jdn = datetime_jdn(dttm);
    if (gregorianYear < 1700 || gregorianYear > 2400 || jdn == LONG_MAX)
        return NULL;

    hinduYearStart = gregorianYear;
    newYearJdn = datetime_india_new_moon_jdn_in_window(hinduYearStart, DT_March, 15, DT_April, 15);
    if (newYearJdn == LONG_MAX)
        return NULL;
    if (jdn < newYearJdn) {
        hinduYearStart--;
        newYearJdn = datetime_india_new_moon_jdn_in_window(hinduYearStart, DT_March, 15, DT_April, 15);
    }
    nextYearJdn = datetime_india_new_moon_jdn_in_window(hinduYearStart + 1, DT_March, 15, DT_April, 15);
    if (newYearJdn == LONG_MAX || nextYearJdn == LONG_MAX || jdn < newYearJdn || jdn >= nextYearJdn)
        return NULL;

    starts[0] = newYearJdn;
    while (monthCount < 15) {
        long nextStart = datetime_next_local_new_moon_jdn(starts[monthCount - 1], hinduYearStart, 5.5);
        if (nextStart == LONG_MAX || nextStart >= nextYearJdn)
            break;
        starts[monthCount++] = nextStart;
    }

    for (int i = 0; i < monthCount; i++) {
        long endJdn = (i + 1 < monthCount) ? starts[i + 1] : nextYearJdn;
        if (jdn >= starts[i] && jdn < endJdn) {
            monthIndex = i;
            break;
        }
    }

    month = monthIndex % 12 + 1;
    lunarDay = (int)(jdn - starts[monthIndex] + 1L);
    if (monthIndex + 1 < monthCount) {
        double fraction = (double)(jdn - starts[monthIndex]) / (double)(starts[monthIndex + 1] - starts[monthIndex]);
        tithi = (int)floor(fraction * 30.0) + 1;
    } else {
        tithi = lunarDay;
    }
    if (tithi < 1)
        tithi = 1;
    if (tithi > 30)
        tithi = 30;
    paksha = tithi <= 15 ? "Shukla" : "Krishna";
    vikramSamvatYear = hinduYearStart + 57;

    out = string_new();
    if (!out || string_append_format(out, "Vikram Samvat %d, %s %s %d, lunar day %d", vikramSamvatYear,
                                     datetime_hindu_month_name(month), paksha, tithi <= 15 ? tithi : tithi - 15,
                                     lunarDay) < 0) {
        string_free(out);
        return NULL;
    }
    return out;
}

/* Format the selected date in the Thai solar Buddhist Era. */
string_t *datetime_buddhist_calendar_date_text(const datetime_t *dttm)
{
    string_t *out;

    if (!dttm || datetime_year(dttm) == SHRT_MAX)
        return NULL;

    out = string_new();
    if (!out || string_append_format(out, "B.E. %04d-%02d-%02d (Thai solar)", datetime_year(dttm) + 543,
                                     (int)datetime_month(dttm), (int)datetime_day(dttm)) < 0) {
        string_free(out);
        return NULL;
    }
    return out;
}

static long datetime_india_new_moon_jdn_in_window(int year, month_t startMonth, uint8_t startDay, month_t endMonth,
    uint8_t endDay)
{
    long windowStart;
    long windowEnd;
    int lunationIndex;

    if (year < 1700 || year > 2400)
        return LONG_MAX;

    windowStart = datetime_ymd_to_jdn((short)year, startMonth, startDay);
    windowEnd = datetime_ymd_to_jdn((short)year, endMonth, endDay);
    lunationIndex = (int)((windowStart - 2451550.09765) / 29.530588853) - 2;

    for (int i = 0; i < 8; i++, lunationIndex++) {
        double newMoonTT = datetime_true_new_moon_tt(lunationIndex);
        double newMoonUTC = newMoonTT - datetime_delta_t_estimate(year) / 86400.0;
        long indiaJdn = (long)floor(newMoonUTC + 5.5 / 24.0 + 0.5);

        if (indiaJdn >= windowStart && indiaJdn <= windowEnd)
            return indiaJdn;
    }

    return LONG_MAX;
}

static long datetime_india_full_moon_jdn_between(int year, long windowStart, long windowEnd, double *dayFraction)
{
    int lunationIndex;

    if (year < 1700 || year > 2400 || windowStart > windowEnd)
        return LONG_MAX;

    lunationIndex = (int)((windowStart - 2451550.09765) / 29.530588853) - 2;

    for (int i = 0; i < 8; i++, lunationIndex++) {
        double fullMoonTT = datetime_true_full_moon_tt(lunationIndex);
        double fullMoonUTC = fullMoonTT - datetime_delta_t_estimate(year) / 86400.0;
        double fullMoonIndia = fullMoonUTC + 5.5 / 24.0;
        double civilDay = floor(fullMoonIndia + 0.5);
        long indiaJdn = (long)civilDay;

        if (indiaJdn >= windowStart && indiaJdn <= windowEnd) {
            if (dayFraction)
                *dayFraction = fullMoonIndia + 0.5 - civilDay;
            return indiaJdn;
        }
    }

    return LONG_MAX;
}

static long datetime_last_india_full_moon_jdn_between(int year, long windowStart, long windowEnd)
{
    long found = LONG_MAX;
    int lunationIndex;

    if (year < 1700 || year > 2400 || windowStart > windowEnd)
        return LONG_MAX;

    lunationIndex = (int)((windowStart - 2451550.09765) / 29.530588853) - 2;

    for (int i = 0; i < 8; i++, lunationIndex++) {
        double fullMoonTT = datetime_true_full_moon_tt(lunationIndex);
        double fullMoonUTC = fullMoonTT - datetime_delta_t_estimate(year) / 86400.0;
        double fullMoonIndia = fullMoonUTC + 5.5 / 24.0;
        long indiaJdn = (long)floor(fullMoonIndia + 0.5);

        if (indiaJdn >= windowStart && indiaJdn <= windowEnd)
            found = indiaJdn;
    }

    return found;
}

/* Initialise a datetime with estimated Diwali. */
datetime_t *datetime_init_diwali(datetime_t *dttm, int year)
{
    long indiaJdn;

    if (!dttm || year < 1700 || year > 2400)
        return NULL;

    indiaJdn = datetime_india_new_moon_jdn_in_window(year, DT_October, 15, DT_November, 20);
    return datetime_init_materialised_jdn(dttm, indiaJdn);
}

/* Initialise a datetime with estimated Holi. */
datetime_t *datetime_init_holi(datetime_t *dttm, int year)
{
    double dayFraction = 0.0;
    long newYearJdn;
    long indiaJdn;

    if (!dttm || year < 1700 || year > 2400)
        return NULL;

    newYearJdn = datetime_india_new_moon_jdn_in_window(year, DT_March, 15, DT_April, 15);
    indiaJdn = datetime_india_full_moon_jdn_between(year, newYearJdn - 25L, newYearJdn - 8L, &dayFraction);
    if (indiaJdn != LONG_MAX && dayFraction >= 16.0 / 24.0)
        indiaJdn++;

    return datetime_init_materialised_jdn(dttm, indiaJdn);
}

/* Initialise a datetime with estimated Hindu lunar New Year. */
datetime_t *datetime_init_hindu_new_year(datetime_t *dttm, int year)
{
    long indiaJdn;

    if (!dttm || year < 1700 || year > 2400)
        return NULL;

    indiaJdn = datetime_india_new_moon_jdn_in_window(year, DT_March, 15, DT_April, 15);
    if (indiaJdn == LONG_MAX)
        return NULL;

    return datetime_init_materialised_jdn(dttm, indiaJdn);
}

static datetime_t *datetime_init_india_full_moon_observance(datetime_t *dttm, int year, month_t month)
{
    long indiaJdn;

    if (!dttm || year < 1700 || year > 2400)
        return NULL;

    indiaJdn = datetime_india_full_moon_jdn_between(
        year, datetime_ymd_to_jdn((short)year, month, 1),
        datetime_ymd_to_jdn((short)year, month, (uint8_t)datetime_days_in_month((short)year, month)), NULL);

    return datetime_init_materialised_jdn(dttm, indiaJdn);
}

/* Initialise a datetime with estimated Theravada Buddhist New Year. */
datetime_t *datetime_init_buddhist_new_year(datetime_t *dttm, int year)
{
    return datetime_init_india_full_moon_observance(dttm, year, DT_April);
}

/* Initialise a datetime with estimated Vesak, or Buddha Day. */
datetime_t *datetime_init_vesak(datetime_t *dttm, int year)
{
    long indiaJdn;

    if (!dttm || year < 1700 || year > 2400)
        return NULL;

    indiaJdn = datetime_last_india_full_moon_jdn_between(year, datetime_ymd_to_jdn((short)year, DT_May, 1),
                                                         datetime_ymd_to_jdn((short)year, DT_May, 31));

    return datetime_init_materialised_jdn(dttm, indiaJdn);
}

/* Initialise a datetime with estimated Asalha Puja, or Dharma Day. */
datetime_t *datetime_init_asalha_puja(datetime_t *dttm, int year)
{
    return datetime_init_india_full_moon_observance(dttm, year, DT_July);
}
