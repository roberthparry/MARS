/**
 * @file tba_series.h
 * @brief Opaque dated CSV analysis for the native forecasting workbench.
 *
 * Detects date columns and observation frequencies, describes usable driver
 * ranges, finds robust outliers and estimates lag correlations. Missing and
 * non-finite values never become zero observations. Results are owned JSON.
 */
#ifndef TBA_SERIES_H
#define TBA_SERIES_H
#include "tba_csv.h"
#include "tba_dates.h"
typedef struct tba_series tba_series_t;
/** @brief Load and index dates. @param path Borrowed CSV path. @param date_column Optional header. @return Owned series or NULL. */
tba_series_t *tba_series_open(const char *path, const char *date_column);
/** @brief Release observations. @param series Owned series; NULL is safe. */
void tba_series_free(tba_series_t *series);
/** @brief Describe target/driver data. @param series Series. @param target Optional value column. @param drivers Comma-separated selection. @return Owned metadata or NULL. */
json_t *tba_series_details(const tba_series_t *series, const char *target, const char *drivers);
/** @brief Describe each target variable. @param series Series. @return Owned object keyed by header, or NULL. */
json_t *tba_series_targets(const tba_series_t *series);
/** @brief Compute driver overlap diagnostics. @param series Series. @param drivers Selection. @return Owned warning object or NULL. */
json_t *tba_series_overlap(const tba_series_t *series, const char *drivers);
/** @brief Enumerate supported forecast ends. @param target Target metadata. @param drivers Optional driver series. @param selection Driver headers. @param fiscal Fiscal year. @return Owned array of ISO dates. */
json_t *tba_series_ends(const json_t *target, const tba_series_t *drivers, const char *selection, bool fiscal);
/** @brief Adjust selected observations without editing the input. @param series Series. @param target Value header. @param mode flag/cap/exclude. @param dates ISO date selection. @param note Receives owned explanatory text. @return Owned CSV text or NULL. */
string_t *tba_series_adjust(tba_series_t *series, const char *target, const char *mode, const char *dates, string_t **note);
#endif
