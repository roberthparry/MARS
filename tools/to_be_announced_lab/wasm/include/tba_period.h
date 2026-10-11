/**
 * @file tba_period.h
 * @brief Civil-date forecast horizon arithmetic for native and WASM consumers.
 *
 * Dates cross the browser boundary as YYYYMMDD integers, never local timestamps.
 * Gregorian years 1..9999 are supported, independently of daylight-saving time.
 * Frequencies use the identifiers in tba_model.h. Invalid input returns zero.
 */
#ifndef TBA_PERIOD_H
#define TBA_PERIOD_H

/**
 * @brief Find the inclusive end of a forecasting period.
 * @param date Valid YYYYMMDD date.
 * @param frequency Unknown 0, daily 1, monthly 2, quarterly 3 or yearly 4.
 * @param fiscal Non-zero for a financial year ending on 31 March.
 * @return Period-end date, or zero on invalid input or overflow.
 */
int tba_period_end(int date, unsigned frequency, int fiscal);

/**
 * @brief Advance a forecast horizon by whole periods.
 * @param date Valid YYYYMMDD date.
 * @param frequency Frequency identifier as for tba_period_end.
 * @param count Non-negative number of periods, at most 10000.
 * @param fiscal Non-zero for a financial year ending on 31 March.
 * @return Advanced period-end date, or zero on invalid input or overflow.
 */
int tba_period_add(int date, unsigned frequency, unsigned count, int fiscal);

/**
 * @brief Build the selectable forecast horizon.
 * @param target Last target date.
 * @param driver Last driver date or zero.
 * @param frequency Frequency identifier.
 * @param fiscal March financial year.
 * @return Number of options, at most 1200.
 */
unsigned tba_period_options(int target, int driver, unsigned frequency, int fiscal);

/** @brief Read a generated horizon option. @param index Zero-based option index. @return YYYYMMDD or zero. */
int tba_period_option(unsigned index);

#endif
