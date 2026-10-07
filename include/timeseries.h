/**
 * @file timeseries.h
 * @brief Datetime-indexed forecasting and time-series analysis API.
 *
 * Use this module for dated observations, calendar-aware analysis and forecasting
 * in scientific or business applications. Series operations keep dates associated
 * with numeric values so that alignment, missing observations and forecast periods
 * can be handled explicitly.
 *
 * It combines datetime.h indexing with number.h arithmetic and statistical model
 * results. Choose matrix.h for unindexed numerical tables; this module adds temporal
 * semantics and supported forecasting workflows rather than a general real-time
 * stream-processing engine.
 *
 * `timeseries_t` is an opaque series type for regularly or irregularly
 * indexed numeric observations backed by the shared `number_t` layer.
 *
 * The initial public surface is aimed at forecasting workflows built on:
 *
 * - daily, monthly, quarterly, and yearly frequencies
 * - calendar-year and fiscal-year reporting
 * - regression, ARIMA, ARIMAX, SARIMA, and SARIMAX models
 * - datetime-aware slicing, alignment, lagging, differencing, and forecasting
 *
 * MARS's datetime layer currently has second resolution, but this module's
 * frequency semantics intentionally start at daily granularity. Sub-daily
 * frequencies are left for future expansion.
 */

#ifndef TIMESERIES_H
#define TIMESERIES_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include "datetime.h"
#include "matrix.h"
#include "number.h"

/** @brief Opaque series owning observations, missing flags and optional datetime indices. */
typedef struct timeseries_t timeseries_t;

/** @brief Opaque incremental builder owning appended observations and optional datetime indices. */
typedef struct ts_builder_t ts_builder_t;

/**
 * @brief Supported time-series sampling frequencies.
 */
typedef enum {
    TS_FREQ_UNKNOWN = 0,
    TS_FREQ_IRREGULAR,
    TS_FREQ_DAILY,
    TS_FREQ_MONTHLY,
    TS_FREQ_QUARTERLY,
    TS_FREQ_YEARLY
} ts_frequency_t;

/**
 * @brief Year-bucketing convention for reporting and aggregation.
 *
 * `TS_YEAR_FISCAL_UK_APR` uses the UK-style fiscal year starting on 1 April
 * and ending on 31 March of the following calendar year.
 */
typedef enum { TS_YEAR_CALENDAR = 0, TS_YEAR_FISCAL_UK_APR } ts_year_type_t;

/**
 * @brief Missing-data handling policy.
 */
typedef enum {
    TS_MISSING_ERROR = 0,
    TS_MISSING_KEEP,
    TS_MISSING_DROP,
    TS_MISSING_INTERPOLATE_LINEAR,
    TS_MISSING_FORWARD_FILL,
    TS_MISSING_BACKWARD_FILL
} ts_missing_policy_t;

/**
 * @brief Join policy for aligned series operations.
 */
typedef enum { TS_JOIN_INNER = 0, TS_JOIN_LEFT, TS_JOIN_RIGHT, TS_JOIN_OUTER } ts_join_type_t;

/**
 * @brief Estimation strategy for ARIMA-family models.
 */
typedef enum { TS_EST_CSS = 0, TS_EST_MLE, TS_EST_CSS_MLE } ts_estimation_t;

/**
 * @brief Information criterion selector for model search helpers.
 */
typedef enum { TS_IC_NONE = 0, TS_IC_AIC, TS_IC_AICC, TS_IC_BIC } ts_information_criterion_t;

/**
 * @brief Text rendering style for series and summary output.
 */
typedef enum { TS_STRING_INLINE = 0, TS_STRING_PRETTY, TS_STRING_CSV } ts_string_style_t;

/**
 * @brief Result of probing series index metadata.
 */
typedef struct {
    ts_frequency_t frequency;
    ts_year_type_t year_type;
    bool is_regular;
    bool has_datetimes;
    size_t season_period;
} ts_index_info_t;

/**
 * @brief Generic fit controls shared by regression and ARIMA-family models.
 */
typedef struct {
    size_t max_iterations;
    number_t abs_tol;
    number_t rel_tol;
    bool compute_covariance;
    bool compute_p_values;
    bool enforce_stationarity;
    bool enforce_invertibility;
} ts_fit_options_t;

/**
 * @brief Order specification for ARIMA, ARIMAX, SARIMA, and SARIMAX models.
 */
typedef struct {
    size_t p;
    size_t d;
    size_t q;
    size_t P;
    size_t D;
    size_t Q;
    size_t season_period;
    bool include_mean;
    bool include_drift;
    bool include_intercept;
    ts_estimation_t estimation;
} ts_arima_spec_t;

/**
 * @brief Summary statistics for a fitted forecasting model.
 */
typedef struct {
    number_t sigma2;
    number_t loglik;
    number_t aic;
    number_t aicc;
    number_t bic;
    int status;
    size_t iterations;
} ts_fit_summary_t;

/**
 * @brief Result of a regression fit.
 *
 * Coefficient-oriented members are returned as `matrix_t *` so one API can
 * handle scalar regressions, multiple exogenous regressors, and future
 * multivariate extensions without introducing a second container type.
 */
typedef struct {
    matrix_t *coefficients;
    matrix_t *stderr;
    matrix_t *t_stat;
    matrix_t *p_value;
    matrix_t *vcov;
    timeseries_t *fitted;
    timeseries_t *residuals;
    number_t r2;
    number_t adj_r2;
    number_t sse;
    number_t mse;
    number_t rmse;
    ts_fit_summary_t summary;
} ts_regression_result_t;

/**
 * @brief Result of an ARIMA-family fit.
 */
typedef struct {
    matrix_t *ar_params;
    matrix_t *ma_params;
    matrix_t *seasonal_ar_params;
    matrix_t *seasonal_ma_params;
    matrix_t *xreg_params;
    matrix_t *param_stderr;
    matrix_t *param_t_stat;
    matrix_t *param_p_value;
    matrix_t *vcov;
    timeseries_t *fitted;
    timeseries_t *residuals;
    timeseries_t *innovations;
    ts_fit_summary_t summary;
} ts_arima_result_t;

/**
 * @brief Forecast output for a point forecast and its interval.
 */
typedef struct {
    timeseries_t *mean;
    timeseries_t *stderr;
    timeseries_t *lower;
    timeseries_t *upper;
    number_t level;
} ts_forecast_t;

/**
 * @brief Standard forecast-accuracy metrics.
 */
typedef struct {
    number_t mae;
    number_t mse;
    number_t rmse;
    number_t mape;
    number_t smape;
    number_t mase;
} ts_accuracy_t;

/**
 * @brief Rolling-origin backtest configuration.
 */
typedef struct {
    size_t train_length;
    size_t test_length;
    size_t horizon;
    size_t step;
    bool expanding_window;
} ts_backtest_spec_t;

/* -------------------------------------------------------------------------
   Constructors / lifecycle
   ------------------------------------------------------------------------- */

/**
 * @brief Create an undated series by copying numeric values.
 *
 * Missing flags are initially false, even for NaN values. The frequency is unknown and the year convention is calendar.
 *
 * @param values Borrowed array of length numbers, or NULL to initialise all observations to zero.
 * @param length Number of observations; zero is allowed.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_new(const number_t *values, size_t length);

/**
 * @brief Create a series with a generated datetime index.
 *
 * Values and datetimes are copied. Regularity is asserted without validation. Date-step failures are ignored; unknown
 * or irregular frequencies repeat the start datetime.
 *
 * @param values Borrowed array of length numbers; may be NULL only when length is zero.
 * @param length Number of observations; zero is allowed.
 * @param start Non-NULL borrowed initial datetime.
 * @param frequency Sampling frequency metadata; daily, monthly, quarterly or yearly for advancing dates.
 * @param year_type Calendar or UK April fiscal-year convention, stored as metadata.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_new_regular(const number_t *values, size_t length, const datetime_t *start, ts_frequency_t frequency,
                             ts_year_type_t year_type);

/**
 * @brief Create a series with an optional explicit datetime index.
 *
 * Input order is preserved without sorting or checking spacing. Regularity remains false and numeric NaNs do not set
 * missing flags.
 *
 * @param values Borrowed array of length numbers, or NULL for zero-valued observations.
 * @param index Borrowed array of length non-NULL datetimes to clone, or NULL for an undated series.
 * @param length Number of observations; zero is allowed.
 * @param frequency Sampling frequency metadata; daily, monthly, quarterly or yearly for advancing dates.
 * @param year_type Calendar or UK April fiscal-year convention, stored as metadata.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_new_indexed(const number_t *values, const datetime_t *const *index, size_t length,
                             ts_frequency_t frequency, ts_year_type_t year_type);

/**
 * @brief Convenience constructor from ordinary C doubles.
 *
 * Values are converted with `num_create_from_double()`, as for converting the array and then calling `ts_new()`.
 *
 * NaN inputs are additionally marked missing.
 *
 * @param values Borrowed array of length doubles; may be NULL only when length is zero.
 * @param length Number of observations; zero is allowed.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_new_from_doubles(const double *values, size_t length);

/**
 * @brief Convenience constructor for a regular dated series from C doubles.
 *
 * The datetime index is generated from @p start and @p frequency.
 *
 * NaN inputs are marked missing. Index generation and unvalidated regularity follow ts_new_regular().
 *
 * @param values Borrowed array of length doubles; may be NULL only when length is zero.
 * @param length Number of observations; zero is allowed.
 * @param start Non-NULL borrowed initial datetime.
 * @param frequency Sampling frequency metadata; daily, monthly, quarterly or yearly for advancing dates.
 * @param year_type Calendar or UK April fiscal-year convention, stored as metadata.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_new_regular_from_doubles(const double *values, size_t length, const datetime_t *start,
                                          ts_frequency_t frequency, ts_year_type_t year_type);

/**
 * @brief Convenience constructor for an explicitly dated series from C doubles.
 *
 * NaN inputs are marked missing. Dates remain in input order and regularity is not inferred.
 *
 * @param values Borrowed array of length doubles; may be NULL only when length is zero.
 * @param index Borrowed array of length non-NULL datetimes; the array may be NULL only when length is zero.
 * @param length Number of observations; zero is allowed.
 * @param frequency Sampling frequency metadata; daily, monthly, quarterly or yearly for advancing dates.
 * @param year_type Calendar or UK April fiscal-year convention, stored as metadata.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_new_indexed_from_doubles(const double *values, const datetime_t *const *index, size_t length,
                                          ts_frequency_t frequency, ts_year_type_t year_type);

/**
 * @brief Deep-copy a series, its values, missing flags and datetimes.
 *
 * Metadata is copied unchanged.
 *
 * @param series Borrowed source series; NULL produces NULL.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_clone(const timeseries_t *series);

/**
 * @brief Release a series and all its owned storage.
 *
 * @param series Owned series to release; NULL is a no-op.
 */
void ts_free(timeseries_t *series);

/**
 * @brief Create a row-at-a-time timeseries builder.
 *
 * Builders are useful when the final row count is not known up front, such as data arriving from a UI, database cursor,
 * or parser. Appended datetimes are cloned, and appended values are copied into the builder.
 *
 * The first append selects dated or undated rows; subsequent appends must use the same form. Rows are not sorted.
 *
 * @param frequency Sampling frequency metadata; daily, monthly, quarterly or yearly for advancing dates.
 * @param year_type Calendar or UK April fiscal-year convention, stored as metadata.
 * @return Newly allocated builder, released with ts_builder_destroy(), or NULL on allocation failure.
 */
ts_builder_t *ts_builder_new(ts_frequency_t frequency, ts_year_type_t year_type);

/**
 * @brief Append a copied number and optional datetime to a builder.
 *
 * NaN is marked missing. A failed first append may still select the dated or undated row form.
 *
 * @param builder Non-NULL builder to modify.
 * @param datetime Borrowed datetime to clone, or NULL for an undated row; must match previous rows.
 * @param value Non-NULL borrowed number to copy.
 * @return 0 on success, or -1 on invalid input, inconsistent row form or allocation failure.
 */
int ts_builder_append(ts_builder_t *builder, const datetime_t *datetime, const number_t *value);

/**
 * @brief Append a double and optional datetime to a builder.
 *
 * Uses ts_builder_append(); NaN is marked missing and its row-form restrictions apply.
 *
 * @param builder Non-NULL builder to modify.
 * @param datetime Borrowed datetime to clone, or NULL for an undated row.
 * @param value Observation value, converted to number_t.
 * @return 0 on success, otherwise -1.
 */
int ts_builder_append_double(ts_builder_t *builder, const datetime_t *datetime, double value);

/**
 * @brief Parse a date and append a dated double observation.
 *
 * Whitespace is trimmed before datetime parsing; the parsed datetime is copied. All builder rows must be dated.
 *
 * @param builder Non-NULL builder to modify.
 * @param date_text Non-NULL borrowed date text.
 * @param value Observation value; NaN is marked missing.
 * @return 0 on success, or -1 on invalid input, date parsing, row-form or allocation failure.
 */
int ts_builder_append_date_text_double(ts_builder_t *builder, const string_t *date_text, double value);

/**
 * @brief Parse a date and append a dated double observation.
 *
 * Whitespace is trimmed before datetime parsing; the parsed datetime is copied. All builder rows must be dated.
 *
 * @param builder Non-NULL builder to modify.
 * @param date_text Non-NULL borrowed NUL-terminated date string.
 * @param value Observation value; NaN is marked missing.
 * @return 0 on success, or -1 on invalid input, date parsing, row-form or allocation failure.
 */
int ts_builder_append_date_string_double(ts_builder_t *builder, const char *date_text, double value);

/**
 * @brief Copy the current builder contents into an independent series.
 *
 * The builder remains valid and can be reused. Dates are not sorted and regularity is not inferred.
 *
 * @param builder Borrowed builder; NULL produces NULL.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_builder_build(const ts_builder_t *builder);

/**
 * @brief Release a builder and its copied rows.
 *
 * @param builder Owned builder to release; NULL is a no-op.
 */
void ts_builder_destroy(ts_builder_t *builder);

/* -------------------------------------------------------------------------
   CSV loading
   ------------------------------------------------------------------------- */

/**
 * @brief Load one numeric column from a dated CSV file into a timeseries.
 *
 * The CSV file must contain a date column and a value column identified by header name. Dates are parsed using the
 * datetime layer; for the current Sample forecasting inputs this is expected to support forms such as `31/05/2020`.
 *
 * Fields are split on commas without CSV quoting support. Rows retain file order; absent or unparseable dates are
 * skipped. Empty or invalid numeric fields become missing. TS_MISSING_ERROR retains missing values, DROP removes them,
 * and other fill policies have the limitations of ts_fill_missing(). Regularity follows the frequency label without
 * checking dates.
 *
 * @param path Non-NULL borrowed path text.
 * @param date_column Non-NULL borrowed date header name; an empty name selects the first column.
 * @param value_column Non-NULL borrowed numeric header name; an empty name selects the first column.
 * @param frequency Sampling frequency metadata; daily, monthly, quarterly or yearly for advancing dates.
 * @param year_type Calendar or UK April fiscal-year convention, stored as metadata.
 * @param missing_policy Missing-value policy; ERROR currently behaves as KEEP.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input, allocation or file I/O failure.
 */
timeseries_t *ts_from_csv_text(const string_t *path, const string_t *date_column, const string_t *value_column,
                               ts_frequency_t frequency, ts_year_type_t year_type, ts_missing_policy_t missing_policy);

/**
 * @brief Convenience wrapper for ts_from_csv_text().
 *
 * Loading, ordering and missing-policy limitations are those of ts_from_csv_text().
 *
 * @param path Non-NULL borrowed NUL-terminated file path.
 * @param date_column Non-NULL borrowed date header name; an empty name selects the first column.
 * @param value_column Non-NULL borrowed numeric header name; an empty name selects the first column.
 * @param frequency Sampling frequency metadata; daily, monthly, quarterly or yearly for advancing dates.
 * @param year_type Calendar or UK April fiscal-year convention, stored as metadata.
 * @param missing_policy Missing-value policy; ERROR currently behaves as KEEP.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input, allocation or file I/O failure.
 */
timeseries_t *ts_from_csv(const char *path, const char *date_column, const char *value_column, ts_frequency_t frequency,
                          ts_year_type_t year_type, ts_missing_policy_t missing_policy);

/**
 * @brief Load a dated CSV file into a design matrix of selected numeric columns.
 *
 * The returned matrix is numeric and is intended for exogenous regressors in regression and ARIMAX/SARIMAX workflows.
 *
 * Rows retain file order; dates are only checked for non-empty text, never parsed or sorted. Comma splitting does not
 * support quoted CSV fields. DROP and ERROR both discard rows with empty selected fields; other policies keep NaNs
 * without filling. Numeric conversion uses double precision.
 *
 * @param path Non-NULL borrowed file path text.
 * @param date_column Non-NULL borrowed date header name; an empty name selects the first column.
 * @param value_columns Non-NULL borrowed array of non-NULL header names; empty names select the first column.
 * @param value_column_count Positive number of selected numeric columns.
 * @param frequency Currently ignored.
 * @param missing_policy Row policy for empty numeric fields, with the limitations above.
 * @return Newly allocated numeric matrix, released with mat_free(), or NULL on invalid input or loading failure.
 */
matrix_t *ts_matrix_from_csv_text(const string_t *path, const string_t *date_column,
                                  const string_t *const *value_columns, size_t value_column_count,
                                  ts_frequency_t frequency, ts_missing_policy_t missing_policy);

/**
 * @brief Convenience wrapper for ts_matrix_from_csv_text().
 *
 * Rows retain file order; dates are only checked for non-empty text, never parsed or sorted. Comma splitting does not
 * support quoted CSV fields. DROP and ERROR both discard rows with empty selected fields; other policies keep NaNs
 * without filling. Numeric conversion uses double precision.
 *
 * @param path Non-NULL borrowed file path as a NUL-terminated string.
 * @param date_column Non-NULL borrowed date header name; an empty name selects the first column.
 * @param value_columns Non-NULL borrowed array of header strings; NULL entries become empty names selecting the first
 * column.
 * @param value_column_count Positive number of selected numeric columns.
 * @param frequency Currently ignored.
 * @param missing_policy Row policy for empty numeric fields, with the limitations above.
 * @return Newly allocated numeric matrix, released with mat_free(), or NULL on invalid input or loading failure.
 */
matrix_t *ts_matrix_from_csv(const char *path, const char *date_column, const char *const *value_columns,
                             size_t value_column_count, ts_frequency_t frequency, ts_missing_policy_t missing_policy);

/* -------------------------------------------------------------------------
   Formatting / file output
   ------------------------------------------------------------------------- */

/**
 * @brief Render a series as text.
 *
 * Dates use day/month/year without time of day. Missing values render as NA, (missing), or an empty CSV field. Empty
 * non-CSV series produce NULL. Some internal append failures are not propagated.
 *
 * @param series Borrowed series; NULL produces NULL.
 * @param style Inline, pretty or CSV style; unrecognised values use inline formatting.
 * @return Newly allocated text, released with string_free(), or NULL if no text is produced or formatting fails.
 */
string_t *ts_to_text(const timeseries_t *series, ts_string_style_t style);

/**
 * @brief Render a series as text.
 *
 * Dates use day/month/year without time of day. Missing values render as NA, (missing), or an empty CSV field. Empty
 * non-CSV series produce NULL. Some internal append failures are not propagated.
 *
 * @param series Borrowed series; NULL produces NULL.
 * @param style Inline, pretty or CSV style; unrecognised values use inline formatting.
 * @return Newly allocated NUL-terminated string, released with free(), or NULL if no text is produced or allocation
 * fails.
 */
char *ts_to_string(const timeseries_t *series, ts_string_style_t style);

/**
 * @brief Format text using a variable-argument list.
 *
 * Standard conversions are delegated to the string formatter. Series conversions support field width and left
 * alignment; precision is ignored and length modifiers are rejected.
 *
 * @param fmt Non-NULL borrowed NUL-terminated format string; `%t`, `%T` and `%C` render a timeseries_t * inline,
 * pretty or CSV.
 * @param ap Initialised argument list matching fmt; series arguments must be non-NULL. A va_copy is consumed.
 * @return Newly allocated text, released with string_free(), or NULL if no text is produced or formatting fails.
 */
string_t *ts_vsprintf_text(const char *fmt, va_list ap);

/**
 * @brief Format newly allocated text with series conversions.
 *
 * Standard conversions are delegated to the string formatter. Series conversions support field width and left
 * alignment; precision is ignored and length modifiers are rejected.
 *
 * @param fmt Non-NULL borrowed NUL-terminated format string; `%t`, `%T` and `%C` render a timeseries_t * inline,
 * pretty or CSV.
 * @param ... Arguments matching fmt, including non-NULL timeseries_t * values for series conversions.
 * @return Newly allocated text, released with string_free(), or NULL if no text is produced or formatting fails.
 */
string_t *ts_sprintf_text(const char *fmt, ...);

/**
 * @brief Format text into a bounded byte buffer.
 *
 * Uses ts_vsprintf_text(). Truncation is permitted; when out is non-NULL and out_size is positive, successful
 * formatting writes a NUL terminator. A formatting failure leaves the buffer unchanged; length overflow may be reported
 * after writing.
 *
 * @param out Borrowed output buffer, or NULL to request the required length without writing.
 * @param out_size Buffer capacity in bytes including the NUL terminator; zero disables writing.
 * @param fmt Non-NULL borrowed NUL-terminated format string; `%t`, `%T` and `%C` render a timeseries_t * inline,
 * pretty or CSV.
 * @param ... Arguments matching fmt, including non-NULL timeseries_t * values for series conversions.
 * @return Required byte count excluding the NUL terminator, even if truncated, or -1 on failure or INT_MAX overflow.
 */
int ts_sprintf(char *out, size_t out_size, const char *fmt, ...);

/**
 * @brief Print formatted text with series conversions to standard output.
 *
 * Uses ts_vsprintf_text() and the string output layer.
 *
 * @param fmt Non-NULL borrowed NUL-terminated format string; `%t`, `%T` and `%C` render a timeseries_t * inline,
 * pretty or CSV.
 * @param ... Arguments matching fmt, including non-NULL timeseries_t * values for series conversions.
 * @return The string output layer's written count, or -1 on formatting or output failure.
 */
int ts_printf(const char *fmt, ...);

/**
 * @brief Print a series in pretty style to standard output.
 *
 * Formatting and output errors are not reported.
 *
 * @param series Borrowed series; NULL or an empty series produces no output.
 */
void ts_print(const timeseries_t *series);

/**
 * @brief Render forecast means and intervals.
 *
 * CSV includes date, mean, stderr, lower and upper columns. Both text styles show mean and bounds identically. Finite
 * numbers use two decimal places and dates omit time of day; absent optional series render as NaN. Empty means produce
 * NULL outside CSV. The current renderer leaks temporary matrices and may not report append failures.
 *
 * @param forecast Non-NULL borrowed forecast with a non-NULL mean; any supplied interval series must be at least as
 * long.
 * @param style CSV for tabular output; all other values select the same readable text.
 * @return Newly allocated text, released with string_free(), or NULL if no text is produced or formatting fails.
 */
string_t *ts_forecast_to_text(const ts_forecast_t *forecast, ts_string_style_t style);

/**
 * @brief Render forecast means and intervals.
 *
 * CSV includes date, mean, stderr, lower and upper columns. Both text styles show mean and bounds identically. Finite
 * numbers use two decimal places and dates omit time of day; absent optional series render as NaN. Empty means produce
 * NULL outside CSV. The current renderer leaks temporary matrices and may not report append failures.
 *
 * @param forecast Non-NULL borrowed forecast with a non-NULL mean; any supplied interval series must be at least as
 * long.
 * @param style CSV for tabular output; all other values select the same readable text.
 * @return Newly allocated NUL-terminated string, released with free(), or NULL if no text is produced or allocation
 * fails.
 */
char *ts_forecast_to_string(const ts_forecast_t *forecast, ts_string_style_t style);

/**
 * @brief Render a readable regression fit summary.
 *
 * Includes fit statistics, coefficients and heuristic quality assessments. Formatting can lose numeric precision and
 * internal append failures are not consistently reported.
 *
 * @param result Borrowed result with non-NULL coefficients; NULL or absent coefficients produces NULL.
 * @return Newly allocated text, released with string_free(), or NULL if no text is produced or formatting fails.
 */
string_t *ts_regression_summary_to_text(const ts_regression_result_t *result);

/**
 * @brief Render a readable regression fit summary.
 *
 * Includes fit statistics, coefficients and heuristic quality assessments. Formatting can lose numeric precision and
 * internal append failures are not consistently reported.
 *
 * @param result Borrowed result with non-NULL coefficients; NULL or absent coefficients produces NULL.
 * @return Newly allocated NUL-terminated string, released with free(), or NULL if no text is produced or allocation
 * fails.
 */
char *ts_regression_summary_to_string(const ts_regression_result_t *result);

/**
 * @brief Render a readable ARIMA-family fit summary.
 *
 * Includes fit statistics, coefficients and heuristic quality assessments. Formatting can lose numeric precision and
 * internal append failures are not consistently reported. Model details depend on metadata associated with the original
 * result address.
 *
 * @param result Non-NULL borrowed initialised result; NULL produces NULL.
 * @return Newly allocated text, released with string_free(), or NULL if no text is produced or formatting fails.
 */
string_t *ts_arima_summary_to_text(const ts_arima_result_t *result);

/**
 * @brief Render a readable ARIMA-family fit summary.
 *
 * Includes fit statistics, coefficients and heuristic quality assessments. Formatting can lose numeric precision and
 * internal append failures are not consistently reported. Model details depend on metadata associated with the original
 * result address.
 *
 * @param result Non-NULL borrowed initialised result; NULL produces NULL.
 * @return Newly allocated NUL-terminated string, released with free(), or NULL if no text is produced or allocation
 * fails.
 */
char *ts_arima_summary_to_string(const ts_arima_result_t *result);

/**
 * @brief Write a series to a text or CSV file.
 *
 * The output format is chosen by @p style. `TS_STRING_CSV` produces a header row followed by one observation per row.
 * The text styles produce readable plain-text output intended for reports or quick inspection.
 *
 * Creates or overwrites the destination. Formatting limitations of the corresponding text renderer apply.
 *
 * @param path Non-NULL borrowed NUL-terminated output path.
 * @param series Non-NULL borrowed series.
 * @param style Inline, pretty or CSV output, as in ts_to_text().
 * @return 0 on success, or -1 on invalid input, formatting or file I/O failure.
 */
int ts_write_file(const char *path, const timeseries_t *series, ts_string_style_t style);

/**
 * @brief Write forecast output to a text or CSV file.
 *
 * CSV output is expected to include at least date, mean, standard error, lower bound, and upper bound columns.
 *
 * Creates or overwrites the destination. Formatting limitations of the corresponding text renderer apply.
 *
 * @param path Non-NULL borrowed NUL-terminated output path.
 * @param forecast Non-NULL borrowed forecast satisfying ts_forecast_to_text() requirements.
 * @param style CSV columns or readable text, as in ts_forecast_to_text().
 * @return 0 on success, or -1 on invalid input, formatting or file I/O failure.
 */
int ts_forecast_write_file(const char *path, const ts_forecast_t *forecast, ts_string_style_t style);

/**
 * @brief Write a regression summary to a text or CSV file.
 *
 * Text output is intended for human-readable model reports. CSV output is intended for coefficient tables that can be
 * consumed by spreadsheets or downstream tooling.
 *
 * Creates or overwrites the destination. Formatting limitations of the corresponding text renderer apply. CSV currently
 * contains coefficient rows only, without fit statistics.
 *
 * @param path Non-NULL borrowed NUL-terminated output path.
 * @param result Non-NULL borrowed regression result with coefficients.
 * @param style Output style; summary text styles share the same readable report.
 * @return 0 on success, or -1 on invalid input, formatting or file I/O failure.
 */
int ts_regression_summary_write_file(const char *path, const ts_regression_result_t *result, ts_string_style_t style);

/**
 * @brief Write an ARIMA-family summary to a text or CSV file.
 *
 * Creates or overwrites the destination. Formatting limitations of the corresponding text renderer apply. CSV contains
 * only non-seasonal AR estimates when ar_params is present; otherwise even CSV requests use text output.
 *
 * @param path Non-NULL borrowed NUL-terminated output path.
 * @param result Non-NULL borrowed initialised ARIMA result.
 * @param style Output style; summary text styles share the same readable report.
 * @return 0 on success, or -1 on invalid input, formatting or file I/O failure.
 */
int ts_arima_summary_write_file(const char *path, const ts_arima_result_t *result, ts_string_style_t style);

/**
 * @brief Serialise a timeseries into a SQLite-ready payload.
 *
 * The payload uses CSV text, while the encoding metadata records the series frequency and year convention needed for
 * reconstruction. On success, the caller owns @p out_type, @p out_encoding, and @p out_data and must release the labels
 * with @c string_free() and the buffer with @c free().
 *
 * All output pointers must be non-NULL and distinct; outputs are written only on success and existing storage is not
 * freed. Initialise slots to NULL and length to zero before use. Payload bytes are not NUL-terminated. CSV omits time
 * of day; undated or missing-valued rows cannot currently be read back by ts_deserialise().
 *
 * @param series Non-NULL borrowed series to serialise.
 * @param out_type Receives an owned type label; release with string_free().
 * @param out_encoding Receives owned frequency and year-convention metadata; release with string_free().
 * @param out_data Receives owned payload bytes; release with free().
 * @param out_len Receives payload length in bytes, excluding any NUL terminator.
 * @return true on success, otherwise false with outputs unchanged.
 */
bool ts_serialize(const timeseries_t *series, string_t **out_type, string_t **out_encoding, void **out_data,
                  size_t *out_len);

/**
 * @brief Reconstruct a timeseries from a serialised payload.
 *
 * Requires dated rows with non-empty values accepted in full by strtod(); missing fields and undated rows fail. Numeric
 * values are reconstructed through double precision. Unknown or absent metadata defaults to unknown frequency and
 * calendar years. Input bytes are copied; they need not end with NUL.
 *
 * @param data Non-NULL borrowed payload buffer of at least len bytes.
 * @param len Payload size in bytes; must permit allocation of len + 1 bytes.
 * @param type Non-NULL borrowed type label, exactly timeseries_t.
 * @param encoding Non-NULL borrowed encoding text containing optional frequency and year_type labels.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input, parsing or allocation failure.
 */
timeseries_t *ts_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding);

/* -------------------------------------------------------------------------
   Basic inspection
   ------------------------------------------------------------------------- */

/**
 * @brief Return the observation count.
 *
 * @param series Borrowed series; may be NULL.
 * @return Number of observations, or zero for NULL.
 */
size_t ts_length(const timeseries_t *series);

/**
 * @brief Return stored index metadata.
 *
 * The season period is measured in observations: monthly defaults to 12, quarterly to 4, yearly to 1, otherwise zero.
 * Metadata is not a validation of the actual index.
 *
 * @param series Borrowed series; may be NULL.
 * @return Metadata by value; NULL gives unknown frequency, calendar years, false flags and period zero.
 */
ts_index_info_t ts_index_info(const timeseries_t *series);

/**
 * @brief Return the stored sampling frequency.
 *
 * @param series Borrowed series; may be NULL.
 * @return Stored frequency, or TS_FREQ_UNKNOWN for NULL.
 */
ts_frequency_t ts_frequency(const timeseries_t *series);

/**
 * @brief Return the stored year convention.
 *
 * @param series Borrowed series; may be NULL.
 * @return Stored convention, or TS_YEAR_CALENDAR for NULL.
 */
ts_year_type_t ts_year_type(const timeseries_t *series);

/**
 * @brief Read the stored regularity flag.
 *
 * Does not check datetime spacing.
 *
 * @param series Borrowed series; may be NULL.
 * @return Stored regularity flag, or false for NULL.
 */
bool ts_is_regular(const timeseries_t *series);

/**
 * @brief Check whether any observation is flagged missing.
 *
 * Does not examine numeric NaNs independently of missing flags.
 *
 * @param series Borrowed series; may be NULL.
 * @return true if any stored missing flag is set, otherwise false, including for NULL.
 */
bool ts_has_missing(const timeseries_t *series);

/**
 * @brief Copy an observation into caller-owned numeric storage.
 *
 * The output is overwritten without destroying an existing number. Missing flags are not returned separately.
 *
 * @param series Non-NULL borrowed source series.
 * @param index Zero-based observation index, less than the series length.
 * @param out Non-NULL output slot, initially zeroed or cleared; destroy the returned number with num_destroy().
 * @return 0 on success, or -1 on invalid input with out unchanged.
 */
int ts_get_value(const timeseries_t *series, size_t index, number_t *out);

/**
 * @brief Copy an indexed datetime into caller-provided storage.
 *
 * Initialises the destination fields with datetime_init_copy(); ownership of its storage remains with the caller.
 *
 * @param series Non-NULL borrowed dated series.
 * @param index Zero-based observation index, less than the series length.
 * @param out Non-NULL writable datetime storage, for example from datetime_alloc(); no prior value is required.
 * @return 0 on success, or -1 for invalid input, absent index or absent datetime; validation failure leaves out
 * unchanged.
 */
int ts_get_datetime(const timeseries_t *series, size_t index, datetime_t *out);

/**
 * @brief Clone the first available datetime in row order.
 *
 * The result is not necessarily the chronological minimum; no sorting is performed.
 *
 * @param series Borrowed series; may be NULL, empty or undated.
 * @return Owned datetime, released with datetime_dealloc(), or NULL if none is available or allocation fails.
 */
datetime_t *ts_start_datetime(const timeseries_t *series);

/**
 * @brief Clone the last available datetime in row order.
 *
 * The result is not necessarily the chronological maximum; no sorting is performed.
 *
 * @param series Borrowed series; may be NULL, empty or undated.
 * @return Owned datetime, released with datetime_dealloc(), or NULL if none is available or allocation fails.
 */
datetime_t *ts_end_datetime(const timeseries_t *series);

/* -------------------------------------------------------------------------
   Subsetting / alignment
   ------------------------------------------------------------------------- */

/**
 * @brief Copy a contiguous range of observations.
 *
 * Copies metadata unchanged. The range must fit the source and start + length must not overflow.
 *
 * @param series Non-NULL borrowed source series.
 * @param start Zero-based starting observation index; may equal the series length for an empty slice.
 * @param length Number of observations to copy; zero is allowed.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_slice(const timeseries_t *series, size_t start, size_t length);

/**
 * @brief Copy the contiguous span between the first and last matching dates.
 *
 * Both bounds are inclusive. Supply dates in ascending row order: unsorted inputs can include intervening rows outside
 * the bounds.
 *
 * @param series Non-NULL borrowed dated series.
 * @param start Non-NULL borrowed inclusive lower datetime bound.
 * @param end Non-NULL borrowed inclusive upper datetime bound.
 * @return Owned series, released with ts_free(), or NULL for no match, invalid input or allocation failure.
 */
timeseries_t *ts_slice_date(const timeseries_t *series, const datetime_t *start, const datetime_t *end);

/**
 * @brief Copy the first observations of a series.
 *
 * Copies at most the series length and preserves metadata.
 *
 * @param series Non-NULL borrowed source series.
 * @param n Requested number of observations; zero yields an empty copy.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_head(const timeseries_t *series, size_t n);

/**
 * @brief Copy the last observations of a series.
 *
 * Copies at most the series length and preserves metadata.
 *
 * @param series Non-NULL borrowed source series.
 * @param n Requested number of observations; zero yields an empty copy.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_tail(const timeseries_t *series, size_t n);

/**
 * @brief Copy a series and apply the implemented missing-value fill policy.
 *
 * Forward fill propagates from a preceding present value. Backward fill makes one forward pass and only fills a gap
 * immediately before a present value. Linear interpolation only averages two present immediate neighbours, without
 * datetime weighting. Leading, trailing or longer gaps can remain. KEEP, ERROR, DROP and unknown policies return an
 * unchanged copy; use ts_drop_missing() to drop rows.
 *
 * @param series Non-NULL borrowed source series.
 * @param policy Requested missing-data policy, subject to the limitations above.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_fill_missing(const timeseries_t *series, ts_missing_policy_t policy);

/**
 * @brief Copy all observations whose missing flag is false.
 *
 * Preserves row order and metadata, including regularity even if removing rows leaves gaps. Does not independently
 * detect NaNs.
 *
 * @param series Non-NULL borrowed source series.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_drop_missing(const timeseries_t *series);

/**
 * @brief Align two series on their datetime index.
 *
 * On success, `left_out` and `right_out` receive newly allocated aligned series following the selected join policy.
 *
 * Both indices must be in ascending order; missing datetimes are skipped and unmatched retained rows receive missing
 * NaNs. Frequency and year metadata are copied, but regularity and season period are not inferred. Output slots are
 * written only on success; initialise them to NULL and release prior results before reuse.
 *
 * @param left Non-NULL borrowed dated left series.
 * @param right Non-NULL borrowed dated right series.
 * @param join_type Inner, left, right or outer datetime join.
 * @param left_out Non-NULL distinct output slot receiving an owned series; release with ts_free().
 * @param right_out Non-NULL distinct output slot receiving an owned series; release with ts_free().
 * @return 0 on success, or -1 on invalid input or allocation failure; outputs remain unchanged on failure.
 */
int ts_align_pair(const timeseries_t *left, const timeseries_t *right, ts_join_type_t join_type,
                  timeseries_t **left_out, timeseries_t **right_out);

/* -------------------------------------------------------------------------
   Transformations
   ------------------------------------------------------------------------- */

/**
 * @brief Shift observations later without changing the index.
 *
 * Vacated slots are marked missing NaNs. Zero shift gives a copy; shifts at least as large as the length make all slots
 * missing.
 *
 * @param series Non-NULL borrowed source series.
 * @param lag Shift in observation positions, not elapsed time.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_lag(const timeseries_t *series, size_t lag);

/**
 * @brief Shift observations earlier without changing the index.
 *
 * Vacated slots are marked missing NaNs. Zero shift gives a copy; shifts at least as large as the length make all slots
 * missing. The shift plus any row index must not overflow size_t.
 *
 * @param series Non-NULL borrowed source series.
 * @param lead Shift in observation positions, not elapsed time.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_lead(const timeseries_t *series, size_t lead);

/**
 * @brief Apply repeated first differences while retaining the original index.
 *
 * Each pass subtracts the preceding observation. The leading slot and any pair containing missing data become missing
 * NaNs.
 *
 * @param series Non-NULL borrowed source series.
 * @param differences Number of differencing passes; zero returns a copy.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_diff(const timeseries_t *series, size_t differences);

/**
 * @brief Apply repeated seasonal differences while retaining the index.
 *
 * Each pass subtracts the observation season_period positions earlier. Insufficient history or missing inputs yield
 * missing NaNs. A zero period subtracts each present value from itself.
 *
 * @param series Non-NULL borrowed source series.
 * @param differences Number of differencing passes; zero returns a copy.
 * @param season_period Seasonal lag in observations, not calendar units.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_seasonal_diff(const timeseries_t *series, size_t differences, size_t season_period);

/**
 * @brief Compute cumulative sums of present observations.
 *
 * Missing rows remain missing and do not reset the running total. Index and metadata are retained.
 *
 * @param series Non-NULL borrowed source series.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_cumsum(const timeseries_t *series);

/**
 * @brief Apply the natural logarithm to present observations.
 *
 * Delegates to the number layer, retaining index and missing flags. No real-domain validation is performed and newly
 * produced NaNs are not marked missing.
 *
 * @param series Non-NULL borrowed source series.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_log(const timeseries_t *series);

/**
 * @brief Apply the natural exponential to present observations.
 *
 * Delegates to the number layer, retaining index and missing flags. No real-domain validation is performed and newly
 * produced NaNs are not marked missing.
 *
 * @param series Non-NULL borrowed source series.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_exp(const timeseries_t *series);

/**
 * @brief Apply a Box-Cox transform to present observations.
 *
 * Uses log(x) for zero lambda and (x^lambda - 1) / lambda otherwise. No positivity or real-domain checks are made;
 * index and missing flags are retained even for newly produced NaNs.
 *
 * @param series Non-NULL borrowed source series.
 * @param lambda Non-NULL borrowed dimensionless transform parameter; not consumed.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_box_cox(const timeseries_t *series, const number_t *lambda);

/**
 * @brief Compute a trailing rolling mean.
 *
 * Uses double precision, skips flagged missing observations and permits partial windows at the beginning. A window with
 * no present values is missing. Index and metadata are retained.
 *
 * @param series Non-NULL borrowed source series.
 * @param window Positive window width in observation positions.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_roll_mean(const timeseries_t *series, size_t window);

/**
 * @brief Compute a trailing rolling population variance.
 *
 * Uses double precision, skips flagged missing observations and permits partial windows at the beginning. A window with
 * no present values is missing. Index and metadata are retained. Divides by the present count, not count minus one;
 * round-off can produce a negative variance.
 *
 * @param series Non-NULL borrowed source series.
 * @param window Positive window width in observation positions.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_roll_var(const timeseries_t *series, size_t window);

/**
 * @brief Compute a trailing rolling population standard deviation.
 *
 * Uses double precision, skips flagged missing observations and permits partial windows at the beginning. A window with
 * no present values is missing. Index and metadata are retained. Divides variance by the present count and clamps
 * negative round-off variance to zero.
 *
 * @param series Non-NULL borrowed source series.
 * @param window Positive window width in observation positions.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_roll_std(const timeseries_t *series, size_t window);

/**
 * @brief Compute a trailing rolling sum.
 *
 * Uses double precision, skips flagged missing observations and permits partial windows at the beginning. A window with
 * no present values is missing. Index and metadata are retained.
 *
 * @param series Non-NULL borrowed source series.
 * @param window Positive window width in observation positions.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_roll_sum(const timeseries_t *series, size_t window);

/* -------------------------------------------------------------------------
   Matrix / feature helpers
   ------------------------------------------------------------------------- */

/**
 * @brief Copy series values into a length-by-one matrix.
 *
 * Datetimes and missing flags are not transferred; numeric values are copied as stored.
 *
 * @param series Non-NULL borrowed source series.
 * @return Newly allocated matrix, released with mat_free(), or NULL on invalid input or allocation failure.
 */
matrix_t *ts_to_column_matrix(const timeseries_t *series);

/**
 * @brief Copy series values into a one-by-length matrix.
 *
 * Datetimes and missing flags are not transferred; numeric values are copied as stored.
 *
 * @param series Non-NULL borrowed source series.
 * @return Newly allocated matrix, released with mat_free(), or NULL on invalid input or allocation failure.
 */
matrix_t *ts_to_row_matrix(const timeseries_t *series);

/**
 * @brief Build a matrix of lagged values with an optional intercept.
 *
 * Rows correspond to source positions max_lag onwards; columns are optional ones followed by lags 1 through max_lag.
 * Values pass through double precision; missing flags and dates are not used.
 *
 * @param series Non-NULL borrowed source series.
 * @param max_lag Largest lag in observations; must be smaller than the series length.
 * @param include_intercept Whether to prepend a column of ones.
 * @return Newly allocated matrix, released with mat_free(), or NULL on invalid input or allocation failure.
 */
matrix_t *ts_design_matrix_lags(const timeseries_t *series, size_t max_lag, bool include_intercept);

/**
 * @brief Build positional polynomial trend columns.
 *
 * Produces one row per observation, with enabled columns in intercept, linear, quadratic order. The trend is the
 * zero-based row index, independent of datetimes.
 *
 * @param series Non-NULL borrowed source series.
 * @param include_intercept Whether to include a column of ones.
 * @param include_linear Whether to include the row index.
 * @param include_quadratic Whether to include the squared row index.
 * @return Newly allocated matrix, released with mat_free(), or NULL on invalid input or allocation failure.
 */
matrix_t *ts_design_matrix_trend(const timeseries_t *series, bool include_intercept, bool include_linear,
                                 bool include_quadratic);

/**
 * @brief Build seasonal indicator columns from row positions.
 *
 * The seasonal bucket is the zero-based row index modulo season_period; datetimes are not consulted.
 *
 * @param series Non-NULL borrowed source series.
 * @param season_period Positive number of observations per season.
 * @param drop_first Whether to omit bucket zero, yielding season_period - 1 columns.
 * @return Newly allocated matrix, released with mat_free(), or NULL on invalid input or allocation failure.
 */
matrix_t *ts_design_matrix_seasonal_dummies(const timeseries_t *series, size_t season_period, bool drop_first);

/**
 * @brief Build a date-aligned exogenous matrix from several series.
 *
 * Each input series becomes one numeric column in the result.
 *
 * Alignment is iterative, using double precision and NaNs for missing rows. For more than two inputs, earlier columns
 * are realigned with inner joins, so non-inner multi-series joins are not reliably preserved. Prefer a common index or
 * two inputs for non-inner joins.
 *
 * @param series Non-NULL borrowed array of non-NULL series; multiple inputs require sorted datetime indices.
 * @param count Positive number of series and output columns.
 * @param join_type Pairwise datetime join policy, subject to the multi-series limitation above.
 * @return Newly allocated matrix, released with mat_free(), or NULL on invalid input or allocation failure.
 */
matrix_t *ts_bind_columns(timeseries_t *const *series, size_t count, ts_join_type_t join_type);

/* -------------------------------------------------------------------------
   Calendar aggregation / reporting
   ------------------------------------------------------------------------- */

/**
 * @brief Copy a series or aggregate its values by mean at a requested frequency.
 *
 * An unchanged frequency returns a clone. Otherwise uses ts_aggregate_mean() with the source year convention. Does not
 * insert absent periods, interpolate or implement general upsampling.
 *
 * @param series Non-NULL borrowed source series.
 * @param target_frequency Target frequency; daily, monthly, quarterly or yearly when different from the source.
 * @param missing_policy Currently ignored.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_as_frequency(const timeseries_t *series, ts_frequency_t target_frequency,
                              ts_missing_policy_t missing_policy);

/**
 * @brief Aggregate consecutive calendar buckets by sum.
 *
 * Uses double precision and skips flagged missing values; all-missing buckets remain missing. Absent buckets are not
 * inserted, although the result is marked regular. Daily labels copy the first datetime, monthly labels use its date,
 * and quarterly/yearly labels use the period start.
 *
 * @param series Non-NULL borrowed series with non-NULL datetimes in ascending order.
 * @param target_frequency Daily, monthly, quarterly or yearly; unknown and irregular targets are unsupported.
 * @param year_type Calendar or UK April fiscal-year convention for grouping and output metadata.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_aggregate_sum(const timeseries_t *series, ts_frequency_t target_frequency, ts_year_type_t year_type);

/**
 * @brief Aggregate consecutive calendar buckets by mean.
 *
 * Uses double precision and skips flagged missing values; all-missing buckets remain missing. Absent buckets are not
 * inserted, although the result is marked regular. Daily labels copy the first datetime, monthly labels use its date,
 * and quarterly/yearly labels use the period start.
 *
 * @param series Non-NULL borrowed series with non-NULL datetimes in ascending order.
 * @param target_frequency Daily, monthly, quarterly or yearly; unknown and irregular targets are unsupported.
 * @param year_type Calendar or UK April fiscal-year convention for grouping and output metadata.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_aggregate_mean(const timeseries_t *series, ts_frequency_t target_frequency, ts_year_type_t year_type);

/**
 * @brief Aggregate consecutive calendar buckets by minimum.
 *
 * Uses double precision and skips flagged missing values; all-missing buckets remain missing. Absent buckets are not
 * inserted, although the result is marked regular. Daily labels copy the first datetime, monthly labels use its date,
 * and quarterly/yearly labels use the period start.
 *
 * @param series Non-NULL borrowed series with non-NULL datetimes in ascending order.
 * @param target_frequency Daily, monthly, quarterly or yearly; unknown and irregular targets are unsupported.
 * @param year_type Calendar or UK April fiscal-year convention for grouping and output metadata.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_aggregate_min(const timeseries_t *series, ts_frequency_t target_frequency, ts_year_type_t year_type);

/**
 * @brief Aggregate consecutive calendar buckets by maximum.
 *
 * Uses double precision and skips flagged missing values; all-missing buckets remain missing. Absent buckets are not
 * inserted, although the result is marked regular. Daily labels copy the first datetime, monthly labels use its date,
 * and quarterly/yearly labels use the period start.
 *
 * @param series Non-NULL borrowed series with non-NULL datetimes in ascending order.
 * @param target_frequency Daily, monthly, quarterly or yearly; unknown and irregular targets are unsupported.
 * @param year_type Calendar or UK April fiscal-year convention for grouping and output metadata.
 * @return Newly allocated series, released with ts_free(), or NULL on invalid input or allocation failure.
 */
timeseries_t *ts_aggregate_max(const timeseries_t *series, ts_frequency_t target_frequency, ts_year_type_t year_type);

/* -------------------------------------------------------------------------
   Diagnostics / identification
   ------------------------------------------------------------------------- */

/**
 * @brief Compute autocorrelations in a column matrix.
 *
 * Flagged missing observations are removed before computing lags, compressing gaps. Uses double precision and a common
 * centred sum-of-squares denominator; a constant series gives zero at every lag, including zero. The output slot is not
 * cleared on entry: early failures leave it unchanged, while a failed final allocation assigns NULL. Release any
 * previous matrix before reuse.
 *
 * @param series Non-NULL borrowed series with at least one present observation.
 * @param max_lag Maximum lag in retained observations; clamped to the retained count minus one.
 * @param out Non-NULL output slot, initially NULL; receives an owned matrix to release with mat_free().
 * @return 0 on success, or -1 on invalid input or allocation failure.
 */
int ts_acf(const timeseries_t *series, size_t max_lag, matrix_t **out);

/**
 * @brief Compute partial autocorrelations using an ACF recursion.
 *
 * Missing observations are removed as in ts_acf(). The first matrix entry is one. Unlike ts_acf(), this routine does
 * not clamp its own max_lag; callers must keep it below the retained count. The output slot is not cleared on entry:
 * early failures leave it unchanged, while a failed final allocation assigns NULL. Release any previous matrix before
 * reuse.
 *
 * @param series Non-NULL borrowed series with at least one present observation.
 * @param max_lag Largest lag in retained observations; must be smaller than the non-missing count.
 * @param out Non-NULL output slot, initially NULL; receives an owned matrix to release with mat_free().
 * @return 0 on success, or -1 on invalid input or allocation failure.
 */
int ts_pacf(const timeseries_t *series, size_t max_lag, matrix_t **out);

/**
 * @brief Compute non-negative-lag cross-covariances after an inner datetime join.
 *
 * Despite the name, values are unnormalised covariances, in the product of the input units. Entry k compares x[i] with
 * y[i-k], skipping missing pairs; no pairs yields NaN. Uses double precision. Empty joins are unsupported. The output
 * slot is not cleared on entry: early failures leave it unchanged, while a failed final allocation assigns NULL.
 * Release any previous matrix before reuse.
 *
 * @param x Non-NULL borrowed dated series with an ascending index.
 * @param y Non-NULL borrowed dated series with an ascending index and at least one date matching x.
 * @param max_lag Largest lag in aligned observations; clamped to aligned length minus one.
 * @param out Non-NULL output slot, initially NULL; receives an owned matrix to release with mat_free().
 * @return 0 on success, or -1 on invalid input or allocation failure.
 */
int ts_ccf(const timeseries_t *x, const timeseries_t *y, size_t max_lag, matrix_t **out);

/**
 * @brief Compute a Ljung-Box-style statistic and approximate tail value.
 *
 * Removes flagged missing observations before evaluating lags. The returned tail value is exp(-statistic / 2), without
 * a lag-dependent chi-squared distribution or fitted-parameter correction. Output numbers are assigned only on success,
 * without releasing previous values; initialise or clear both slots before use.
 *
 * @param series Non-NULL borrowed series containing present observations.
 * @param max_lag Largest lag in retained observations; must be smaller than the non-missing count.
 * @param statistic Non-NULL distinct output number slot, zeroed or cleared; destroy the returned value with
 * num_destroy().
 * @param p_value Non-NULL distinct output number slot for an approximate probability; release with num_destroy().
 * @return 0 on success, or -1 on invalid input or allocation failure.
 */
int ts_ljung_box(const timeseries_t *series, size_t max_lag, number_t *statistic, number_t *p_value);

/**
 * @brief Compute a simplified first-difference slope diagnostic.
 *
 * Returns sum(y[i-1] * diff[i]) / sum(y[i-1]^2), or zero when the denominator is zero, skipping missing pairs. The tail
 * value is exp(-abs(statistic)); this is not a full augmented Dickey-Fuller test or calibrated p-value. Output numbers
 * are assigned only on success, without releasing previous values; initialise or clear both slots before use.
 *
 * @param series Non-NULL borrowed series of at least two observations.
 * @param statistic Non-NULL distinct output number slot, zeroed or cleared; destroy the returned value with
 * num_destroy().
 * @param p_value Non-NULL distinct output number slot for an approximate probability; release with num_destroy().
 * @return 0 on success, or -1 on invalid input or allocation failure.
 */
int ts_adf(const timeseries_t *series, number_t *statistic, number_t *p_value);

/**
 * @brief Compute a simplified cumulative-residual diagnostic.
 *
 * Removes missing observations, demeans values and returns sum(cumulative residual^2) / (n^2 * sum(residual^2)), or
 * zero for constant data. The tail value is exp(-10 * statistic); there is no long-run variance estimate or calibrated
 * KPSS p-value. Output numbers are assigned only on success, without releasing previous values; initialise or clear
 * both slots before use.
 *
 * @param series Non-NULL borrowed series containing present observations.
 * @param statistic Non-NULL distinct output number slot, zeroed or cleared; destroy the returned value with
 * num_destroy().
 * @param p_value Non-NULL distinct output number slot for an approximate probability; release with num_destroy().
 * @return 0 on success, or -1 on invalid input or allocation failure.
 */
int ts_kpss(const timeseries_t *series, number_t *statistic, number_t *p_value);

/* -------------------------------------------------------------------------
   Regression
   ------------------------------------------------------------------------- */

/**
 * @brief Fit ordinary least squares with an automatically added intercept.
 *
 * Rows of xreg correspond positionally to y; no date alignment or missing-row removal is performed. Statistics use
 * double precision. Coefficient p-values use a normal approximation; vcov currently stores the unscaled inverse of X'X.
 * Optional inference matrices may be NULL even on success. MSE and sigma2 have squared response units, RMSE has
 * response units, and R-squared is dimensionless.
 *
 * @param y Non-NULL borrowed non-empty response; supply complete observations.
 * @param xreg Borrowed numeric regressors with one row per response, or NULL for an intercept-only fit.
 * @param options Optional borrowed controls; all fields are currently ignored.
 * @param out Non-NULL result storage; zero-initialise and clear before reuse with ts_regression_result_clear().
 * @return 0 on success, otherwise -1. Valid y/out cause out to be zeroed before fitting, without freeing previous
 * contents.
 */
int ts_regression_fit(const timeseries_t *y, const matrix_t *xreg, const ts_fit_options_t *options,
                      ts_regression_result_t *out);

/**
 * @brief Predict from a fitted regression and future regressor rows.
 *
 * Adds the fitted intercept. Dates start one requested period after the last stored history date. Standard errors equal
 * the model RMSE at every horizon and bounds are mean +/- 1.96 * RMSE, ignoring parameter uncertainty and level. Output
 * series retain response units.
 *
 * @param model Non-NULL borrowed fitted result with coefficients.
 * @param future_xreg Non-NULL borrowed matrix; rows set the horizon, columns must match coefficients excluding the
 * intercept.
 * @param history Non-NULL borrowed dated history containing a usable final datetime.
 * @param frequency Daily, monthly, quarterly or yearly date step; unsupported values do not advance the start.
 * @param year_type Calendar or UK April fiscal-year metadata for the forecast.
 * @param level Nominal dimensionless interval level, copied into output but neither validated nor used to set bounds.
 * @param out Non-NULL result storage; zero-initialise and clear before reuse with ts_forecast_clear().
 * @return 0 on success, otherwise -1; out is zeroed after initial pointer checks and remains zero on later failure.
 */
int ts_regression_forecast(const ts_regression_result_t *model, const matrix_t *future_xreg,
                           const timeseries_t *history, ts_frequency_t frequency, ts_year_type_t year_type,
                           number_t level, ts_forecast_t *out);

/**
 * @brief Release all owned members of a regression result and zero its fields.
 *
 * The structure itself is not freed. Repeated clearing of a zeroed structure is safe.
 *
 * @param out Owned regression result storage that is zero-initialised or valid; NULL is a no-op.
 */
void ts_regression_result_clear(ts_regression_result_t *out);

/* -------------------------------------------------------------------------
   ARIMA family
   ------------------------------------------------------------------------- */

/**
 * @brief Fit an ARIMA-family model using iterative lagged least squares.
 *
 * Differences the response and exogenous regressors, drops missing response rows and fits ordinary and seasonal lags
 * plus lagged residuals. Seasonal terms are additive lag terms, without multiplicative cross-terms. Internal missing
 * gaps with regressors can invalidate row correspondence. Estimation selection, relative tolerance and enforcement
 * flags are ignored; no MLE or root constraints are applied. Inference and covariance have the limitations of
 * ts_regression_fit(). Fitted values are reconstructed in response units; residuals, innovations and sigma2 describe
 * the transformed response (sigma2 in squared units). xreg_params contains the full coefficient vector, not just
 * exogenous terms. Summary status and iterations remain zero and do not certify convergence. Keep the result at the
 * same address: forecasting metadata is keyed by that address.
 *
 * @param y Non-NULL borrowed response with enough observations for differencing and all selected lags.
 * @param xreg Optional borrowed numeric regressors, positionally matched to the original response rows.
 * @param spec Non-NULL borrowed orders; seasonal periods are observation counts, with zero disabling seasonal lag
 * terms.
 * @param options Optional controls; positive max_iterations and finite positive abs_tol override defaults 8 and 1e-6.
 * @param out Non-NULL result storage; zero-initialise and clear before reuse with ts_arima_result_clear().
 * @return 0 when a fit is produced, even without convergence, or -1 on failure; out is zeroed after pointer checks.
 */
int ts_arima_fit(const timeseries_t *y, const matrix_t *xreg, const ts_arima_spec_t *spec,
                 const ts_fit_options_t *options, ts_arima_result_t *out);

/**
 * @brief Search a bounded grid of ARIMA orders by information criterion.
 *
 * Searches every combination from zero to each inclusive maximum, using CSS-labelled fits, intercept and mean enabled,
 * and drift disabled. ts_arima_fit() limitations apply. NONE and unrecognised criteria use AIC. The winning result must
 * remain at its output address for associated metadata.
 *
 * @param y Non-NULL borrowed response for ts_arima_fit().
 * @param xreg Optional borrowed regressors, positionally matched to y.
 * @param max_p Inclusive maximum non-seasonal AR order.
 * @param max_d Inclusive maximum ordinary differencing order.
 * @param max_q Inclusive maximum non-seasonal MA order.
 * @param max_P Inclusive maximum seasonal AR order.
 * @param max_D Inclusive maximum seasonal differencing order.
 * @param max_Q Inclusive maximum seasonal MA order.
 * @param season_period Number of observations per season; zero disables seasonal lag terms.
 * @param criterion AIC, AICc or BIC ranking; NONE currently uses AIC.
 * @param options Optional borrowed fit controls, with the limitations of ts_arima_fit().
 * @param best_spec Non-NULL output specification, written when a better candidate is found; otherwise unchanged.
 * @param best_fit Non-NULL result, zero-initialised or previously cleared; release with ts_arima_result_clear().
 * @return 0 for a finite winning score, otherwise -1. best_fit is zeroed after output-pointer checks; failure may
 * update outputs.
 */
int ts_auto_arima(const timeseries_t *y, const matrix_t *xreg, size_t max_p, size_t max_d, size_t max_q, size_t max_P,
                  size_t max_D, size_t max_Q, size_t season_period, ts_information_criterion_t criterion,
                  const ts_fit_options_t *options, ts_arima_spec_t *best_spec, ts_arima_result_t *best_fit);

/**
 * @brief Apply a coefficient-magnitude heuristic for stationarity.
 *
 * Checks only individual non-seasonal ar_params magnitudes against one. Does not examine polynomial roots, seasonal
 * terms or differencing, and therefore does not establish stationarity. NaN magnitudes do not fail the comparison.
 *
 * @param model Borrowed model; NULL or absent parameter matrix passes the heuristic.
 * @return false if any checked coefficient has magnitude at least one, otherwise true.
 */
bool ts_arima_is_stationary(const ts_arima_result_t *model);

/**
 * @brief Apply a coefficient-magnitude heuristic for invertibility.
 *
 * Checks only individual non-seasonal ma_params magnitudes against one. Does not examine polynomial roots, seasonal
 * terms or differencing, and therefore does not establish invertibility. NaN magnitudes do not fail the comparison.
 *
 * @param model Borrowed model; NULL or absent parameter matrix passes the heuristic.
 * @return false if any checked coefficient has magnitude at least one, otherwise true.
 */
bool ts_arima_is_invertible(const ts_arima_result_t *model);

/**
 * @brief Compute the residual ACF of an ARIMA result.
 *
 * Delegates to ts_acf(), including missing-value compression, output ownership and failure behaviour.
 *
 * @param model Non-NULL borrowed model with residuals.
 * @param max_lag Maximum residual lag in retained observations, clamped by ts_acf().
 * @param out Non-NULL output slot, initially NULL; receives an owned matrix to release with mat_free().
 * @return 0 on success, or -1 on invalid input or allocation failure.
 */
int ts_arima_residual_acf(const ts_arima_result_t *model, size_t max_lag, matrix_t **out);

/**
 * @brief Compute the simplified Ljung-Box diagnostic for model residuals.
 *
 * Delegates to ts_ljung_box(); its uncalibrated tail approximation and output initialisation requirements apply.
 *
 * @param model Non-NULL borrowed model with residuals.
 * @param max_lag Largest residual lag; must be smaller than the non-missing residual count.
 * @param statistic Non-NULL distinct output number slot, zeroed or cleared; destroy the returned value with
 * num_destroy().
 * @param p_value Non-NULL distinct output number slot for an approximate probability; release with num_destroy().
 * @return 0 on success, or -1 on invalid input or allocation failure.
 */
int ts_arima_ljung_box(const ts_arima_result_t *model, size_t max_lag, number_t *statistic, number_t *p_value);

/**
 * @brief Forecast recursively from an ARIMA-family result.
 *
 * Uses metadata associated with the fitted model's address for orders, differencing, intercept and regressors. History
 * is compacted to present values; the recurrence currently reads these undifferenced history values even for
 * differenced models, then reintegrates forecasts. This limits the validity of integrated-model forecasts. Bounds use a
 * fixed 1.96 multiplier regardless of level; sigma defaults to one when sigma2 is not positive. For entirely
 * non-negative history, lower bounds are clipped to zero. Means, standard errors and bounds are in response units.
 *
 * @param model Non-NULL borrowed fit at its original address; absent metadata omits model-specific terms.
 * @param history Non-NULL borrowed dated history with present observations, enough lags and a usable final datetime.
 * @param future_xreg Optional borrowed matrix, at least horizon rows; supply the fitted exogenous columns in the same
 * order.
 * @param horizon Positive number of future observations.
 * @param level Nominal dimensionless interval level, copied into output but neither validated nor used for bounds.
 * @param out Non-NULL result storage; zero-initialise and clear before reuse with ts_forecast_clear().
 * @return 0 on success, otherwise -1; out is zeroed after initial validation and remains zero on later failure.
 */
int ts_arima_forecast(const ts_arima_result_t *model, const timeseries_t *history, const matrix_t *future_xreg,
                      size_t horizon, number_t level, ts_forecast_t *out);

/**
 * @brief Release all owned members of an ARIMA result and zero its fields.
 *
 * The structure itself is not freed. Repeated clearing of a zeroed structure is safe. Also removes model metadata
 * associated with this result address.
 *
 * @param out Owned ARIMA result storage that is zero-initialised or valid; NULL is a no-op.
 */
void ts_arima_result_clear(ts_arima_result_t *out);

/* -------------------------------------------------------------------------
   Evaluation / backtesting
   ------------------------------------------------------------------------- */

/**
 * @brief Compute errors after an inner datetime alignment.
 *
 * Skips pairs flagged missing and uses double precision. MAE and RMSE have response units; MSE has squared units. MAPE
 * and sMAPE are fractions, not percentages. Zero denominators add zero to those sums but remain in the pair count. MASE
 * currently equals MAE without scaling. Destroy all six output numbers individually with num_destroy(); no
 * accuracy-clear helper is provided.
 *
 * @param actual Non-NULL borrowed dated actual values with an ascending index.
 * @param predicted Non-NULL borrowed dated predictions with an ascending index.
 * @param out Non-NULL output storage, zero-initialised or with all six number_t fields destroyed before reuse.
 * @return 0 on success, otherwise -1. out is zeroed after alignment succeeds; no usable pairs leave it zeroed.
 */
int ts_accuracy(const timeseries_t *actual, const timeseries_t *predicted, ts_accuracy_t *out);

/**
 * @brief Evaluate one training/test split using a regression model.
 *
 * Only the first train_length observations and the following test_length observations are used. horizon, step and
 * expanding_window are ignored: this is not rolling-origin backtesting. Forecasting and accuracy limitations of the
 * corresponding routines apply. Output ownership is as for ts_accuracy(); earlier failures leave out unchanged.
 *
 * @param y Non-NULL borrowed dated response with at least train_length + test_length rows.
 * @param xreg Non-NULL borrowed regressors with matching response rows.
 * @param options Optional borrowed controls passed to the fit routine.
 * @param spec Non-NULL split configuration; train_length and test_length must be positive observation counts.
 * @param out Non-NULL output storage, zero-initialised or with all six number_t fields destroyed before reuse.
 * @return 0 on success, or -1 on invalid input, fitting, forecasting or evaluation failure.
 */
int ts_backtest_regression(const timeseries_t *y, const matrix_t *xreg, const ts_fit_options_t *options,
                           const ts_backtest_spec_t *spec, ts_accuracy_t *out);

/**
 * @brief Evaluate one training/test split using an ARIMA-family model.
 *
 * Only the first train_length observations and the following test_length observations are used. horizon, step and
 * expanding_window are ignored: this is not rolling-origin backtesting. Forecasting and accuracy limitations of the
 * corresponding routines apply. Output ownership is as for ts_accuracy(); earlier failures leave out unchanged.
 *
 * @param y Non-NULL borrowed dated response with at least train_length + test_length rows.
 * @param xreg Optional borrowed regressors with matching response rows.
 * @param model_spec Non-NULL borrowed ARIMA orders and season period in observations.
 * @param options Optional borrowed controls passed to the fit routine.
 * @param spec Non-NULL split configuration; train_length and test_length must be positive observation counts.
 * @param out Non-NULL output storage, zero-initialised or with all six number_t fields destroyed before reuse.
 * @return 0 on success, or -1 on invalid input, fitting, forecasting or evaluation failure.
 */
int ts_backtest_arima(const timeseries_t *y, const matrix_t *xreg, const ts_arima_spec_t *model_spec,
                      const ts_fit_options_t *options, const ts_backtest_spec_t *spec, ts_accuracy_t *out);

/**
 * @brief Release all owned members of a forecast and zero its fields.
 *
 * The structure itself is not freed. Repeated clearing of a zeroed structure is safe.
 *
 * @param out Owned forecast storage that is zero-initialised or valid; NULL is a no-op.
 */
void ts_forecast_clear(ts_forecast_t *out);

#endif /* TIMESERIES_H */
