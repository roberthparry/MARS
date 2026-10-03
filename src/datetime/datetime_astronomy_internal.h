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
