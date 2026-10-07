/**
 * @file datetime_lunar.c
 * @brief Lunar phases and local new-moon estimates.
 *
 * Calculates phase times and searches for new moons in local-day windows. Lunisolar calendar constructors use
 * these lightweight astronomical helpers; catalogue-backed sky positions belong to the almanac module.
 *
 * This is part of the datetime.h implementation. Jurisdiction holiday policy belongs to the jurisdiction module,
 * while catalogue-backed apparent sky positions belong to almanac.
 */

/* Lunar phase estimates and local new-moon searches. */
#include <limits.h>
#include <math.h>

#include "datetime_internal.h"
#include "datetime_astronomy_internal.h"

/* Reference new Moon and mean synodic month used for phase classification. */
#define NEWMOON_JDN 2451550
#define SYNODIC_MONTH_LENGTH 29.53058867

/* Estimate the true new Moon in Terrestrial Time for a lunation index. */
double datetime_true_new_moon_tt(int lunationIndex)
{
    /* Time in Julian centuries from J2000 */
    double T = lunationIndex / 1236.85;
    double T2 = T * T;
    double T3 = T2 * T;
    double T4 = T3 * T;

    /* Mean new moon (JDE) */
    double jdeMean =
        2451550.09765 + 29.530588853 * lunationIndex + 0.0001337 * T2 - 0.000000150 * T3 + 0.00000000073 * T4;

    /* Sun's mean anomaly (degrees) */
    double sunMeanAnomaly = 2.5534 + 29.10535670 * lunationIndex - 0.0000014 * T2 - 0.00000011 * T3;

    /* Moon's mean anomaly (degrees) */
    double moonMeanAnomaly =
        201.5643 + 385.81693528 * lunationIndex + 0.0107582 * T2 + 0.00001238 * T3 - 0.000000058 * T4;

    /* Moon's argument of latitude (degrees) */
    double moonArgumentLatitude =
        160.7108 + 390.67050284 * lunationIndex - 0.0016118 * T2 - 0.00000227 * T3 + 0.000000011 * T4;

    /* Longitude of ascending node (degrees) */
    double ascendingNodeLongitude = 124.7746 - 1.56375580 * lunationIndex + 0.0020691 * T2 + 0.00000215 * T3;

    /* Convert to radians */
    const double degToRad = M_PI / 180.0;
    sunMeanAnomaly *= degToRad;
    moonMeanAnomaly *= degToRad;
    moonArgumentLatitude *= degToRad;
    ascendingNodeLongitude *= degToRad;

    /* Eccentricity correction factor */
    double E = 1 - 0.002516 * T - 0.0000074 * T2;

    /* Periodic correction terms (Meeus Table 49.A) */
    double correction =
        -0.40720 * sin(moonMeanAnomaly) + 0.17241 * E * sin(sunMeanAnomaly) + 0.01608 * sin(2 * moonMeanAnomaly) +
        0.01039 * sin(2 * moonArgumentLatitude) + 0.00739 * E * sin(moonMeanAnomaly - sunMeanAnomaly) -
        0.00514 * E * sin(moonMeanAnomaly + sunMeanAnomaly) + 0.00208 * E * E * sin(2 * sunMeanAnomaly) -
        0.00111 * sin(moonMeanAnomaly - 2 * moonArgumentLatitude) -
        0.00057 * sin(moonMeanAnomaly + 2 * moonArgumentLatitude) +
        0.00056 * E * sin(2 * moonMeanAnomaly + sunMeanAnomaly) - 0.00042 * sin(3 * moonMeanAnomaly) +
        0.00042 * E * sin(sunMeanAnomaly + 2 * moonArgumentLatitude) +
        0.00038 * E * sin(sunMeanAnomaly - 2 * moonArgumentLatitude) -
        0.00024 * E * sin(2 * moonMeanAnomaly - sunMeanAnomaly) - 0.00017 * sin(ascendingNodeLongitude) -
        0.00007 * sin(moonMeanAnomaly + 2 * sunMeanAnomaly) +
        0.00004 * sin(2 * moonMeanAnomaly - 2 * moonArgumentLatitude) + 0.00004 * sin(3 * sunMeanAnomaly) +
        0.00003 * sin(moonMeanAnomaly + sunMeanAnomaly - 2 * moonArgumentLatitude) +
        0.00003 * sin(2 * moonMeanAnomaly + 2 * moonArgumentLatitude) -
        0.00003 * sin(moonMeanAnomaly - sunMeanAnomaly + 2 * moonArgumentLatitude) -
        0.00002 * sin(moonMeanAnomaly - sunMeanAnomaly - 2 * moonArgumentLatitude) -
        0.00002 * sin(3 * moonMeanAnomaly + sunMeanAnomaly) + 0.00002 * sin(4 * moonMeanAnomaly);

    return jdeMean + correction;
}

/* Estimate the true full Moon in Terrestrial Time for a lunation index. */
double datetime_true_full_moon_tt(int lunationIndex)
{
    double k = lunationIndex + 0.5;
    double T = k / 1236.85;
    double T2 = T * T;
    double T3 = T2 * T;
    double T4 = T3 * T;
    double jdeMean = 2451550.09765 + 29.530588853 * k + 0.0001337 * T2 - 0.000000150 * T3 + 0.00000000073 * T4;
    double sunMeanAnomaly = 2.5534 + 29.10535670 * k - 0.0000014 * T2 - 0.00000011 * T3;
    double moonMeanAnomaly = 201.5643 + 385.81693528 * k + 0.0107582 * T2 + 0.00001238 * T3 - 0.000000058 * T4;
    double moonArgumentLatitude = 160.7108 + 390.67050284 * k - 0.0016118 * T2 - 0.00000227 * T3 + 0.000000011 * T4;
    double ascendingNodeLongitude = 124.7746 - 1.56375580 * k + 0.0020691 * T2 + 0.00000215 * T3;
    const double degToRad = M_PI / 180.0;
    double E = 1 - 0.002516 * T - 0.0000074 * T2;
    double correction;

    sunMeanAnomaly *= degToRad;
    moonMeanAnomaly *= degToRad;
    moonArgumentLatitude *= degToRad;
    ascendingNodeLongitude *= degToRad;

    correction = -0.40614 * sin(moonMeanAnomaly) + 0.17302 * E * sin(sunMeanAnomaly) +
                 0.01614 * sin(2 * moonMeanAnomaly) + 0.01043 * sin(2 * moonArgumentLatitude) +
                 0.00734 * E * sin(moonMeanAnomaly - sunMeanAnomaly) -
                 0.00515 * E * sin(moonMeanAnomaly + sunMeanAnomaly) + 0.00209 * E * E * sin(2 * sunMeanAnomaly) -
                 0.00111 * sin(moonMeanAnomaly - 2 * moonArgumentLatitude) -
                 0.00057 * sin(moonMeanAnomaly + 2 * moonArgumentLatitude) +
                 0.00056 * E * sin(2 * moonMeanAnomaly + sunMeanAnomaly) - 0.00042 * sin(3 * moonMeanAnomaly) +
                 0.00042 * E * sin(sunMeanAnomaly + 2 * moonArgumentLatitude) +
                 0.00038 * E * sin(sunMeanAnomaly - 2 * moonArgumentLatitude) -
                 0.00024 * E * sin(2 * moonMeanAnomaly - sunMeanAnomaly) - 0.00017 * sin(ascendingNodeLongitude) -
                 0.00007 * sin(moonMeanAnomaly + 2 * sunMeanAnomaly) +
                 0.00004 * sin(2 * moonMeanAnomaly - 2 * moonArgumentLatitude) + 0.00004 * sin(3 * sunMeanAnomaly) +
                 0.00003 * sin(moonMeanAnomaly + sunMeanAnomaly - 2 * moonArgumentLatitude) +
                 0.00003 * sin(2 * moonMeanAnomaly + 2 * moonArgumentLatitude) -
                 0.00003 * sin(moonMeanAnomaly - sunMeanAnomaly + 2 * moonArgumentLatitude) -
                 0.00002 * sin(moonMeanAnomaly - sunMeanAnomaly - 2 * moonArgumentLatitude) -
                 0.00002 * sin(3 * moonMeanAnomaly + sunMeanAnomaly) + 0.00002 * sin(4 * moonMeanAnomaly);

    return jdeMean + correction;
}

static long datetime_local_new_moon_jdn_for_lunation(int lunationIndex, int year, double gmtOffsetHours)
{
    double newMoonTT = datetime_true_new_moon_tt(lunationIndex);
    double newMoonUTC = newMoonTT - datetime_delta_t_estimate(year) / 86400.0;
    double localNewMoon = newMoonUTC + gmtOffsetHours / 24.0;

    return (long)floor(localNewMoon + 0.5);
}

long datetime_next_local_new_moon_jdn(long afterJdn, int year, double gmtOffsetHours)
{
    int lunationIndex = (int)floor((afterJdn - 2451550.09765) / 29.530588853) - 2;
    long best = LONG_MAX;

    for (int i = 0; i < 10; i++, lunationIndex++) {
        long newMoonJdn = datetime_local_new_moon_jdn_for_lunation(lunationIndex, year, gmtOffsetHours);
        if (newMoonJdn > afterJdn && newMoonJdn < best)
            best = newMoonJdn;
    }

    return best;
}

long datetime_local_new_moon_jdn_in_window(int year, month_t startMonth, uint8_t startDay, month_t endMonth,
    uint8_t endDay, double gmtOffsetHours)
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
        long localJdn = datetime_local_new_moon_jdn_for_lunation(lunationIndex, year, gmtOffsetHours);
        if (localJdn >= windowStart && localJdn <= windowEnd)
            return localJdn;
    }

    return LONG_MAX;
}

/* Classify the phase from the reference new Moon and mean synodic month. */
static moon_phase_t datetime_moon_phase_on_jdn(long julianDayNumber)
{
    // The moon phase is calculated based on the difference between the given Julian Day Number and a known new moon
    // date, divided by the length of a synodic month (the average time between new moons). The result is then
    // multiplied by 8 and rounded to get an integer value representing the moon phase. The moon phases are typically
    // categorized as follows: 0: New Moon 1: Waxing Crescent 2: First Quarter 3: Waxing Gibbous 4: Full Moon 5: Waning
    // Gibbous 6: Last Quarter 7: Waning Crescent

    double moonPhase = fmod((julianDayNumber - NEWMOON_JDN) / SYNODIC_MONTH_LENGTH, 1.0);
    if (moonPhase < 0) {
        moonPhase += 1.0; // Ensure moonPhase is in the range [0, 1)
    }

    return (moon_phase_t)(int)(moonPhase * 8 + 0.5) % 8; // Round to nearest integer and wrap around using modulo
}

/* Get the moon phase for a given datetime object. */
moon_phase_t datetime_moon_phase(const datetime_t *dttm)
{
    long julianDayNumber = datetime_jdn(dttm);
    if (julianDayNumber == LONG_MAX) {
        return DT_NewMoon; // Default value if datetime is not initialised
    }
    return datetime_moon_phase_on_jdn(julianDayNumber);
}

/* Get a display name for a moon phase. */
const char *datetime_moon_phase_name(moon_phase_t phase)
{
    static const char *names[] = {"New Moon",  "Waxing Crescent", "First Quarter", "Waxing Gibbous",
                                  "Full Moon", "Waning Gibbous",  "Last Quarter",  "Waning Crescent"};

    if (phase < DT_NewMoon || phase > DT_WaningCrescent)
        return "Unknown";
    return names[phase];
}

/* Find the next datetime with a specific moon phase after a given datetime. */
datetime_t *datetime_next_moon_phase(const datetime_t *dttm, moon_phase_t phase)
{
    static const double phase_fraction = 0.125; // Each moon phase corresponds to 1/8 of the synodic month

    if (dttm == NULL)
        return NULL; // Invalid input

    if (dttm->year == SHRT_MAX) {
        if (datetime_year(dttm) == SHRT_MAX) {
            return NULL; // Datetime is not initialised
        }
    }

    long jdn = datetime_jdn(dttm);

    // Current phase fraction (0 = New Moon, 0.5 = Full Moon, etc.)
    double currentPhase = fmod((jdn - NEWMOON_JDN) / SYNODIC_MONTH_LENGTH, 1.0);
    if (currentPhase < 0.0)
        currentPhase += 1.0;

    // Target phase fraction
    double targetPhase = (double)phase * phase_fraction;

    // Compute how far ahead the next target phase is
    double delta = targetPhase - currentPhase;

    delta = fmod(delta + 1.0, 1.0);
    if (delta <= 0.03386)
        delta += 1.0;

    // Convert phase fraction difference → days
    double daysToAdd = delta * SYNODIC_MONTH_LENGTH;

    // Create new datetime and add fractional days
    datetime_t *result = datetime_init_copy(datetime_alloc(), dttm);
    datetime_add_days(result, daysToAdd);

    return result;
}
