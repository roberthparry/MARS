/**
 * @file datetime_calendar_american.c
 * @brief Cherokee, Mayan and Aztec calendar views.
 *
 * Constructs supported observance dates and formatted calendar representations using the module's civil and lunar
 * helpers. These are calendar computations, not jurisdiction-specific public-holiday policy.
 *
 * This is part of the datetime.h implementation. Jurisdiction holiday policy belongs to the jurisdiction module,
 * while catalogue-backed apparent sky positions belong to almanac.
 */

/* Cherokee, Mayan and Aztec calendar views and observances. */
#include <limits.h>

#include "datetime_internal.h"
#include "datetime_astronomy_internal.h"
#include "ustring.h"

static const char *datetime_cherokee_civil_month_name(month_t month)
{
    static const char *names[] = {NULL,          "Cold Moon",     "Bony Moon",       "Windy Moon",
                                  "Flower Moon", "Planting Moon", "Green Corn Moon", "Ripe Corn Moon",
                                  "Fruit Moon",  "Nut Moon",      "Harvest Moon",    "Trading Moon",
                                  "Snow Moon"};

    return month >= DT_January && month <= DT_December ? names[month] : "Unknown";
}

/* Format the selected date in an adapted Cherokee civil calendar style. */
string_t *datetime_cherokee_calendar_date_text(const datetime_t *dttm)
{
    string_t *out;

    if (!dttm || datetime_jdn(dttm) == LONG_MAX)
        return NULL;

    out = string_new();
    if (!out || string_append_format(out, "Cherokee civil %s, day %d, year %d",
                                     datetime_cherokee_civil_month_name(datetime_month(dttm)), (int)datetime_day(dttm),
                                     (int)datetime_year(dttm)) < 0) {
        string_free(out);
        return NULL;
    }
    return out;
}

static datetime_t *datetime_init_cherokee_window_new_moon(datetime_t *dttm, int year, month_t startMonth,
    month_t endMonth)
{
    long jdn;

    /* Cherokee Nation civil centre: Tahlequah / central time approximation. */
    jdn = datetime_local_new_moon_jdn_in_window(year, startMonth, 1, endMonth,
                                                (uint8_t)datetime_days_in_month((short)year, endMonth), -6.0);
    return datetime_init_materialised_jdn(dttm, jdn);
}

/* Initialise a datetime with an estimated Cherokee New Moon Festival date. */
datetime_t *datetime_init_cherokee_new_moon_festival(datetime_t *dttm, int year)
{
    return datetime_init_cherokee_window_new_moon(dttm, year, DT_January, DT_January);
}

/* Initialise a datetime with an estimated Cherokee Green Corn Ceremony date. */
datetime_t *datetime_init_cherokee_green_corn_ceremony(datetime_t *dttm, int year)
{
    return datetime_init_cherokee_window_new_moon(dttm, year, DT_July, DT_July);
}

/* Initialise a datetime with an estimated Cherokee Ripe Corn Ceremony date. */
datetime_t *datetime_init_cherokee_ripe_corn_ceremony(datetime_t *dttm, int year)
{
    return datetime_init_cherokee_window_new_moon(dttm, year, DT_August, DT_August);
}

/* Initialise a datetime with an estimated Cherokee Great New Moon Festival date. */
datetime_t *datetime_init_cherokee_great_new_moon_festival(datetime_t *dttm, int year)
{
    return datetime_init_cherokee_window_new_moon(dttm, year, DT_September, DT_September);
}

/* Format the selected date in the Mayan calendar. */
string_t *datetime_mayan_calendar_date_text(const datetime_t *dttm)
{
    static const char *tzolkin_names[] = {"Imix",  "Ik'",   "Ak'bal", "K'an",     "Chikchan", "Kimi", "Manik'",
                                          "Lamat", "Muluk", "Ok",     "Chuwen",   "Eb'",      "B'en", "Ix",
                                          "Men",   "K'ib'", "Kab'an", "Etz'nab'", "Kawak",    "Ajaw"};
    static const char *haab_names[] = {"Pop",   "Wo'",   "Sip",    "Sotz'",  "Sek",  "Xul", "Yaxk'in",
                                       "Mol",   "Ch'en", "Yax",    "Sak'",   "Keh",  "Mak", "K'ank'in",
                                       "Muwan", "Pax",   "K'ayab", "Kumk'u", "Wayeb"};
    static const long mayan_epoch_jdn = 584283L;
    long jdn;
    long days;
    long baktun;
    long katun;
    long tun;
    long uinal;
    long kin;
    long tzolkin_number;
    long tzolkin_name_index;
    long haab_count;
    long haab_month_index;
    long haab_day;
    string_t *out;

    if (!dttm || datetime_jdn(dttm) == LONG_MAX)
        return NULL;

    jdn = datetime_jdn(dttm);
    days = jdn - mayan_epoch_jdn;
    if (days < 0)
        return NULL;

    baktun = days / 144000L;
    days %= 144000L;
    katun = days / 7200L;
    days %= 7200L;
    tun = days / 360L;
    days %= 360L;
    uinal = days / 20L;
    kin = days % 20L;

    days = jdn - mayan_epoch_jdn;
    tzolkin_number = ((days + 3L) % 13L) + 1L;
    tzolkin_name_index = (days + 19L) % 20L;
    haab_count = (days + 348L) % 365L;
    if (haab_count < 360L) {
        haab_month_index = haab_count / 20L;
        haab_day = haab_count % 20L;
    } else {
        haab_month_index = 18L;
        haab_day = haab_count - 360L;
    }

    out = string_new();
    if (!out || string_append_format(out, "Long Count %ld.%ld.%ld.%ld.%ld; Tzolk'in %ld %s; Haab %ld %s", baktun, katun,
                                     tun, uinal, kin, tzolkin_number, tzolkin_names[tzolkin_name_index], haab_day,
                                     haab_names[haab_month_index]) < 0) {
        string_free(out);
        return NULL;
    }
    return out;
}

static void datetime_mayan_haab_components(long jdn, int *monthIndex, int *day)
{
    long haabCount = (jdn - 584283L + 348L) % 365L;

    if (haabCount < 0)
        haabCount += 365L;
    if (haabCount < 360L) {
        *monthIndex = (int)(haabCount / 20L);
        *day = (int)(haabCount % 20L);
    } else {
        *monthIndex = 18;
        *day = (int)(haabCount - 360L);
    }
}

static datetime_t *datetime_init_mayan_haab_marker(datetime_t *dttm, int gregorianYear, int targetMonthIndex,
    int targetDay)
{
    long startJdn;
    long endJdn;

    if (!dttm || gregorianYear < 1 || gregorianYear > 9999)
        return NULL;

    startJdn = datetime_ymd_to_jdn((short)gregorianYear, DT_January, 1);
    endJdn = datetime_ymd_to_jdn((short)(gregorianYear + 1), DT_January, 1);
    for (long jdn = startJdn; jdn < endJdn; jdn++) {
        int monthIndex;
        int day;

        datetime_mayan_haab_components(jdn, &monthIndex, &day);
        if (monthIndex == targetMonthIndex && day == targetDay)
            return datetime_init_materialised_jdn(dttm, jdn);
    }
    return NULL;
}

/* Initialise a datetime with the Mayan Haab New Year in the requested Gregorian year. */
datetime_t *datetime_init_mayan_haab_new_year(datetime_t *dttm, int year)
{
    return datetime_init_mayan_haab_marker(dttm, year, 0, 0);
}

/* Initialise a datetime with the first day of Wayeb in the requested Gregorian year. */
datetime_t *datetime_init_mayan_wayeb_start(datetime_t *dttm, int year)
{
    return datetime_init_mayan_haab_marker(dttm, year, 18, 0);
}

static datetime_t *datetime_init_aztec_year_start(datetime_t *dttm, int gregorianYear)
{
    return datetime_init_ymd(dttm, gregorianYear, DT_February, 23);
}

/* Initialise a datetime with the Aztec Xiuhpohualli New Year in the requested Gregorian year. */
datetime_t *datetime_init_aztec_xiuhpohualli_new_year(datetime_t *dttm, int year)
{
    return datetime_init_aztec_year_start(dttm, year);
}

/* Initialise a datetime with the first day of Nemontemi in the requested Gregorian year. */
datetime_t *datetime_init_aztec_nemontemi_start(datetime_t *dttm, int year)
{
    if (!dttm || year < 1 || year > 9999)
        return NULL;
    return datetime_init_ymd(dttm, (short)year, DT_February, 18);
}

/* Format the selected date in an Aztec calendar style. */
string_t *datetime_aztec_calendar_date_text(const datetime_t *dttm)
{
    static const char *tonalpohualli_names[] = {"Cipactli",      "Ehecatl",   "Calli",   "Cuetzpalin", "Coatl",
                                                "Miquiztli",     "Mazatl",    "Tochtli", "Atl",        "Itzcuintli",
                                                "Ozomatli",      "Malinalli", "Acatl",   "Ocelotl",    "Cuauhtli",
                                                "Cozcacuauhtli", "Ollin",     "Tecpatl", "Quiahuitl",  "Xochitl"};
    static const char *xiuhpohualli_names[] = {
        "Atlcahualo",      "Tlacaxipehualiztli", "Tozoztontli",     "Hueytozoztli", "Toxcatl",     "Etzalcualiztli",
        "Tecuilhuitontli", "Huey Tecuilhuitl",   "Tlaxochimaco",    "Xocotlhuetzi", "Ochpaniztli", "Teotleco",
        "Tepeilhuitl",     "Quecholli",          "Panquetzaliztli", "Atemoztli",    "Tititl",      "Izcalli",
        "Nemontemi"};
    static const char *year_bearer_names[] = {"Calli", "Tochtli", "Acatl", "Tecpatl"};
    long jdn;
    long aztec_number;
    long aztec_name_index;
    long day_index;
    long xiuh_month_index;
    long xiuh_day;
    int civilYear;
    int year_number;
    int year_bearer_index;
    datetime_t *year_start;
    string_t *out;

    if (!dttm || datetime_jdn(dttm) == LONG_MAX)
        return NULL;

    jdn = datetime_jdn(dttm);
    aztec_number = ((jdn + 3L) % 13L) + 1L;
    aztec_name_index = (jdn + 13L) % 20L;

    civilYear = datetime_year(dttm);
    year_start = datetime_init_aztec_year_start(datetime_alloc(), civilYear);
    if (!year_start) {
        datetime_dealloc(year_start);
        return NULL;
    }
    if (datetime_compare(dttm, year_start) < 0) {
        civilYear--;
        datetime_init_aztec_year_start(year_start, civilYear);
    }

    day_index = datetime_jdn(dttm) - datetime_jdn(year_start);
    if (day_index < 0)
        day_index = 0;
    if (day_index < 360L) {
        xiuh_month_index = day_index / 20L;
        xiuh_day = (day_index % 20L) + 1L;
    } else {
        xiuh_month_index = 18L;
        xiuh_day = (day_index - 360L) + 1L;
    }

    year_number = ((civilYear - 2013) % 13 + 13) % 13 + 1;
    year_bearer_index = ((civilYear - 2013) % 4 + 4) % 4;

    out = string_new();
    if (!out ||
        string_append_format(out, "Tonalpohualli %ld %s; Xiuhpohualli day %ld of %s; year %d %s", aztec_number,
                             tonalpohualli_names[aztec_name_index], xiuh_day, xiuhpohualli_names[xiuh_month_index],
                             year_number, year_bearer_names[year_bearer_index]) < 0) {
        string_free(out);
        out = NULL;
    }

    datetime_dealloc(year_start);
    return out;
}
