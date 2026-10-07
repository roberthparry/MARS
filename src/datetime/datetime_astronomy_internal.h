/**
 * @file datetime_astronomy_internal.h
 * @brief Shared private astronomy helpers for calendar calculations.
 *
 * Declares new- and full-moon estimates, solar terms, Delta T and local-day event helpers. The specialised
 * calendar files use these routines to connect astronomical time to civil dates without adding public
 * implementation details.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef MARS_DATETIME_ASTRONOMY_INTERNAL_H
#define MARS_DATETIME_ASTRONOMY_INTERNAL_H

#include "datetime.h"

/* Private solar, lunar and time-scale helpers shared by calendar calculations. */
double datetime_true_new_moon_tt(int lunationIndex);
double datetime_true_full_moon_tt(int lunationIndex);
double datetime_delta_t_estimate(int year);
double datetime_dec_solstice_tt(int year);
double datetime_normalise_degrees(double degrees);
double datetime_solar_ecliptic_longitude(double jd);
long datetime_next_local_new_moon_jdn(long afterJdn, int year, double gmtOffsetHours);
long datetime_local_new_moon_jdn_in_window(int year, month_t startMonth, uint8_t startDay, month_t endMonth,
    uint8_t endDay, double gmtOffsetHours);

#endif
