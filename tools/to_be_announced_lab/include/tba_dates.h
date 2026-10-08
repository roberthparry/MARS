/**
 * @file tba_dates.h
 * @brief Validated Gregorian dates and forecasting period boundaries.
 *
 * Dates are encoded as YYYYMMDD, with zero meaning absent. Arithmetic uses the
 * MARS datetime module; fiscal years end in March. Parsing accepts the legacy
 * ISO and UK input formats, including Python-compatible two-digit year pivots.
 */
#ifndef TBA_DATES_H
#define TBA_DATES_H
#include "tba_support.h"
/** @brief Parse a date. @param text Borrowed input. @return YYYYMMDD or zero. */
int tba_date_parse(const string_t *text);
/** @brief Parse a UTF-8 date. @param text Borrowed text. @return YYYYMMDD or zero. */
int tba_date_cstr(const char *text);
/** @brief Format a date. @param date Encoded date. @param uk Select DD/MM/YYYY. @return Owned text, empty for zero. */
string_t *tba_date_text(int date, bool uk);
/** @brief Convert a date to a day index. @param date Valid encoded date. @return Julian day number, or zero. */
long tba_date_jdn(int date);
/** @brief Advance one observation. @param date Date. @param frequency Frequency token. @return New date or zero. */
int tba_date_next(int date, const char *frequency);
/** @brief Find the containing period end. @param date Date. @param frequency Frequency. @param fiscal March year end. @return Date or zero. */
int tba_date_end(int date, const char *frequency, bool fiscal);
/** @brief Count future periods. @param start Last observation. @param end Forecast end. @param frequency Frequency. @param fiscal Fiscal year. @return Count, capped at 100000; zero for invalid input. */
size_t tba_date_horizon(int start, int end, const char *frequency, bool fiscal);
/** @brief Label a frequency. @param frequency Frequency token. @return Static English label. */
const char *tba_date_frequency_label(const char *frequency);
#endif
