/**
 * @file tba_chart.h
 * @brief Bounded chart geometry shared by WebAssembly and native tests.
 *
 * Owns temporary numeric buffers for one synchronous chart operation. Input
 * dates must be monotone for tick selection. Missing ordinates use NaN; no text
 * or CSV parsing takes place here. Callers copy results before the next setup.
 */
#ifndef TBA_CHART_H
#define TBA_CHART_H
#include <stddef.h>

/**
 * @brief Borrow chart input storage.
 * @param kind 0 dates, 1 values, 2 hover x, 3 hover y.
 * @return Buffer of 32768 doubles, or NULL.
 */
double *tba_chart_buffer(unsigned kind);
/**
 * @brief Start bounds accumulation.
 * @param count Number of dates, 1..32768.
 * @param end Optional final timestamp.
 * @return Valid capacity.
 */
int tba_chart_begin(unsigned count, double end);
/** @brief Include ordinates in vertical bounds. @param count Number of buffered values, at most the date count. */
void tba_chart_include(unsigned count);
/** @brief Finish padded bounds. @return Non-zero when finite plottable values exist. */
int tba_chart_finish(void);
/** @brief Map an observation to SVG x. @param index Observation index. @return Coordinate or NaN. */
double tba_chart_x(unsigned index);
/** @brief Map an ordinate to SVG y. @param value Finite ordinate. @return Coordinate. */
double tba_chart_y(double value);
/** @brief Read a vertical tick. @param index Tick 0..4, highest first. @return Value or NaN. */
double tba_chart_y_tick(unsigned index);
/**
 * @brief Select a horizontal tick using binary search.
 * @param slot Tick position.
 * @param count Requested count, 1..6.
 * @return Observation index or -1.
 */
int tba_chart_tick(unsigned slot, unsigned count);
/** @brief Test whether timestamps can be used. @return Non-zero for a finite increasing time axis. */
int tba_chart_time_axis(void);
/** @brief Read the last time-axis timestamp. @return Timestamp. */
double tba_chart_end(void);
/**
 * @brief Find the nearest buffered hover point.
 * @param count Point count, at most 32768.
 * @param x Cursor x.
 * @param y Cursor y.
 * @return Index, or -1.
 */
int tba_chart_nearest(unsigned count, double x, double y);
#endif
