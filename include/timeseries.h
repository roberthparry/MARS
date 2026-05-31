#ifndef TIMESERIES_H
#define TIMESERIES_H

#include <stdbool.h>
#include <stddef.h>

#include "datetime.h"
#include "matrix.h"
#include "number.h"

/**
 * @file timeseries.h
 * @brief Datetime-indexed forecasting and time-series analysis API.
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

typedef struct _timeseries_t timeseries_t;

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
typedef enum {
    TS_YEAR_CALENDAR = 0,
    TS_YEAR_FISCAL_UK_APR
} ts_year_type_t;

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
typedef enum {
    TS_JOIN_INNER = 0,
    TS_JOIN_LEFT,
    TS_JOIN_RIGHT,
    TS_JOIN_OUTER
} ts_join_type_t;

/**
 * @brief Estimation strategy for ARIMA-family models.
 */
typedef enum {
    TS_EST_CSS = 0,
    TS_EST_MLE,
    TS_EST_CSS_MLE
} ts_estimation_t;

/**
 * @brief Information criterion selector for model search helpers.
 */
typedef enum {
    TS_IC_NONE = 0,
    TS_IC_AIC,
    TS_IC_AICC,
    TS_IC_BIC
} ts_information_criterion_t;

/**
 * @brief Text rendering style for series and summary output.
 */
typedef enum {
    TS_STRING_INLINE = 0,
    TS_STRING_PRETTY,
    TS_STRING_CSV
} ts_string_style_t;

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

timeseries_t *ts_new(const number_t *values, size_t length);

timeseries_t *ts_new_regular(const number_t *values, size_t length,
                             const datetime_t *start,
                             ts_frequency_t frequency,
                             ts_year_type_t year_type);

timeseries_t *ts_new_indexed(const number_t *values, const datetime_t *const *index,
                             size_t length, ts_frequency_t frequency,
                             ts_year_type_t year_type);

timeseries_t *ts_clone(const timeseries_t *series);
void          ts_free(timeseries_t *series);

/* -------------------------------------------------------------------------
   CSV loading
   ------------------------------------------------------------------------- */

/**
 * @brief Load one numeric column from a dated CSV file into a timeseries.
 *
 * The CSV file must contain a date column and a value column identified by
 * header name. Dates are parsed using the datetime layer; for the current
 * Sample forecasting inputs this is expected to support forms such as
 * `31/05/2020`.
 */
timeseries_t *ts_from_csv(const char *path,
                          const char *date_column,
                          const char *value_column,
                          ts_frequency_t frequency,
                          ts_year_type_t year_type,
                          ts_missing_policy_t missing_policy);

/**
 * @brief Load a dated CSV file into a design matrix of selected numeric columns.
 *
 * Rows are ordered by the parsed date column. The returned matrix is numeric
 * and is intended for exogenous regressors in regression and ARIMAX/SARIMAX
 * workflows.
 */
matrix_t *ts_matrix_from_csv(const char *path,
                             const char *date_column,
                             const char *const *value_columns,
                             size_t value_column_count,
                             ts_frequency_t frequency,
                             ts_missing_policy_t missing_policy);

/* -------------------------------------------------------------------------
   Formatting / file output
   ------------------------------------------------------------------------- */

char *ts_to_string(const timeseries_t *series, ts_string_style_t style);
int   ts_sprintf(char *out, size_t out_size, const char *fmt, ...);
int   ts_printf(const char *fmt, ...);
void  ts_print(const timeseries_t *series);

char *ts_forecast_to_string(const ts_forecast_t *forecast, ts_string_style_t style);
char *ts_regression_summary_to_string(const ts_regression_result_t *result);
char *ts_arima_summary_to_string(const ts_arima_result_t *result);

/**
 * @brief Write a series to a text or CSV file.
 *
 * The output format is chosen by @p style. `TS_STRING_CSV` produces a header
 * row followed by one observation per row. The text styles produce readable
 * plain-text output intended for reports or quick inspection.
 *
 * @return 0 on success, nonzero on error.
 */
int ts_write_file(const char *path,
                  const timeseries_t *series,
                  ts_string_style_t style);

/**
 * @brief Write forecast output to a text or CSV file.
 *
 * CSV output is expected to include at least date, mean, standard error,
 * lower bound, and upper bound columns.
 *
 * @return 0 on success, nonzero on error.
 */
int ts_forecast_write_file(const char *path,
                           const ts_forecast_t *forecast,
                           ts_string_style_t style);

/**
 * @brief Write a regression summary to a text or CSV file.
 *
 * Text output is intended for human-readable model reports. CSV output is
 * intended for coefficient tables and fit statistics that can be consumed by
 * spreadsheets or downstream tooling.
 *
 * @return 0 on success, nonzero on error.
 */
int ts_regression_summary_write_file(const char *path,
                                     const ts_regression_result_t *result,
                                     ts_string_style_t style);

/**
 * @brief Write an ARIMA-family summary to a text or CSV file.
 *
 * @return 0 on success, nonzero on error.
 */
int ts_arima_summary_write_file(const char *path,
                                const ts_arima_result_t *result,
                                ts_string_style_t style);

/* -------------------------------------------------------------------------
   Basic inspection
   ------------------------------------------------------------------------- */

size_t          ts_length(const timeseries_t *series);
ts_index_info_t ts_index_info(const timeseries_t *series);
ts_frequency_t  ts_frequency(const timeseries_t *series);
ts_year_type_t  ts_year_type(const timeseries_t *series);
bool            ts_is_regular(const timeseries_t *series);
bool            ts_has_missing(const timeseries_t *series);

int ts_get_value(const timeseries_t *series, size_t index, number_t *out);
int ts_get_datetime(const timeseries_t *series, size_t index, datetime_t *out);

datetime_t *ts_start_datetime(const timeseries_t *series);
datetime_t *ts_end_datetime(const timeseries_t *series);

/* -------------------------------------------------------------------------
   Subsetting / alignment
   ------------------------------------------------------------------------- */

timeseries_t *ts_slice(const timeseries_t *series, size_t start, size_t length);
timeseries_t *ts_slice_date(const timeseries_t *series,
                            const datetime_t *start,
                            const datetime_t *end);
timeseries_t *ts_head(const timeseries_t *series, size_t n);
timeseries_t *ts_tail(const timeseries_t *series, size_t n);

timeseries_t *ts_fill_missing(const timeseries_t *series, ts_missing_policy_t policy);
timeseries_t *ts_drop_missing(const timeseries_t *series);

/**
 * @brief Align two series on their datetime index.
 *
 * On success, `left_out` and `right_out` receive newly allocated aligned
 * series following the selected join policy.
 *
 * @return 0 on success, nonzero on error.
 */
int ts_align_pair(const timeseries_t *left, const timeseries_t *right,
                  ts_join_type_t join_type,
                  timeseries_t **left_out, timeseries_t **right_out);

/* -------------------------------------------------------------------------
   Transformations
   ------------------------------------------------------------------------- */

timeseries_t *ts_lag(const timeseries_t *series, size_t lag);
timeseries_t *ts_lead(const timeseries_t *series, size_t lead);
timeseries_t *ts_diff(const timeseries_t *series, size_t differences);
timeseries_t *ts_seasonal_diff(const timeseries_t *series,
                               size_t differences, size_t season_period);
timeseries_t *ts_cumsum(const timeseries_t *series);
timeseries_t *ts_log(const timeseries_t *series);
timeseries_t *ts_exp(const timeseries_t *series);
timeseries_t *ts_box_cox(const timeseries_t *series, const number_t *lambda);

timeseries_t *ts_roll_mean(const timeseries_t *series, size_t window);
timeseries_t *ts_roll_var(const timeseries_t *series, size_t window);
timeseries_t *ts_roll_std(const timeseries_t *series, size_t window);
timeseries_t *ts_roll_sum(const timeseries_t *series, size_t window);

/* -------------------------------------------------------------------------
   Matrix / feature helpers
   ------------------------------------------------------------------------- */

matrix_t *ts_to_column_matrix(const timeseries_t *series);
matrix_t *ts_to_row_matrix(const timeseries_t *series);

matrix_t *ts_design_matrix_lags(const timeseries_t *series, size_t max_lag,
                                bool include_intercept);
matrix_t *ts_design_matrix_trend(const timeseries_t *series,
                                 bool include_intercept,
                                 bool include_linear,
                                 bool include_quadratic);
matrix_t *ts_design_matrix_seasonal_dummies(const timeseries_t *series,
                                            size_t season_period,
                                            bool drop_first);

/**
 * @brief Build a date-aligned exogenous matrix from several series.
 *
 * Each input series becomes one numeric column in the result.
 */
matrix_t *ts_bind_columns(timeseries_t *const *series,
                          size_t count,
                          ts_join_type_t join_type);

/* -------------------------------------------------------------------------
   Calendar aggregation / reporting
   ------------------------------------------------------------------------- */

timeseries_t *ts_as_frequency(const timeseries_t *series,
                              ts_frequency_t target_frequency,
                              ts_missing_policy_t missing_policy);

timeseries_t *ts_aggregate_sum(const timeseries_t *series,
                               ts_frequency_t target_frequency,
                               ts_year_type_t year_type);
timeseries_t *ts_aggregate_mean(const timeseries_t *series,
                                ts_frequency_t target_frequency,
                                ts_year_type_t year_type);
timeseries_t *ts_aggregate_min(const timeseries_t *series,
                               ts_frequency_t target_frequency,
                               ts_year_type_t year_type);
timeseries_t *ts_aggregate_max(const timeseries_t *series,
                               ts_frequency_t target_frequency,
                               ts_year_type_t year_type);

/* -------------------------------------------------------------------------
   Diagnostics / identification
   ------------------------------------------------------------------------- */

int ts_acf(const timeseries_t *series, size_t max_lag, matrix_t **out);
int ts_pacf(const timeseries_t *series, size_t max_lag, matrix_t **out);
int ts_ccf(const timeseries_t *x, const timeseries_t *y, size_t max_lag, matrix_t **out);

int ts_ljung_box(const timeseries_t *series, size_t max_lag,
                 number_t *statistic, number_t *p_value);
int ts_adf(const timeseries_t *series,
           number_t *statistic, number_t *p_value);
int ts_kpss(const timeseries_t *series,
            number_t *statistic, number_t *p_value);

/* -------------------------------------------------------------------------
   Regression
   ------------------------------------------------------------------------- */

int ts_regression_fit(const timeseries_t *y,
                      const matrix_t *xreg,
                      const ts_fit_options_t *options,
                      ts_regression_result_t *out);

int ts_regression_forecast(const ts_regression_result_t *model,
                           const matrix_t *future_xreg,
                           const timeseries_t *history,
                           ts_frequency_t frequency,
                           ts_year_type_t year_type,
                           number_t level,
                           ts_forecast_t *out);

void ts_regression_result_clear(ts_regression_result_t *out);

/* -------------------------------------------------------------------------
   ARIMA family
   ------------------------------------------------------------------------- */

int ts_arima_fit(const timeseries_t *y,
                 const matrix_t *xreg,
                 const ts_arima_spec_t *spec,
                 const ts_fit_options_t *options,
                 ts_arima_result_t *out);

int ts_auto_arima(const timeseries_t *y,
                  const matrix_t *xreg,
                  size_t max_p, size_t max_d, size_t max_q,
                  size_t max_P, size_t max_D, size_t max_Q,
                  size_t season_period,
                  ts_information_criterion_t criterion,
                  const ts_fit_options_t *options,
                  ts_arima_spec_t *best_spec,
                  ts_arima_result_t *best_fit);

bool ts_arima_is_stationary(const ts_arima_result_t *model);
bool ts_arima_is_invertible(const ts_arima_result_t *model);

int ts_arima_residual_acf(const ts_arima_result_t *model,
                          size_t max_lag, matrix_t **out);
int ts_arima_ljung_box(const ts_arima_result_t *model,
                       size_t max_lag,
                       number_t *statistic, number_t *p_value);

int ts_arima_forecast(const ts_arima_result_t *model,
                      const timeseries_t *history,
                      const matrix_t *future_xreg,
                      size_t horizon,
                      number_t level,
                      ts_forecast_t *out);

void ts_arima_result_clear(ts_arima_result_t *out);

/* -------------------------------------------------------------------------
   Evaluation / backtesting
   ------------------------------------------------------------------------- */

int ts_accuracy(const timeseries_t *actual,
                const timeseries_t *predicted,
                ts_accuracy_t *out);

int ts_backtest_regression(const timeseries_t *y,
                           const matrix_t *xreg,
                           const ts_fit_options_t *options,
                           const ts_backtest_spec_t *spec,
                           ts_accuracy_t *out);

int ts_backtest_arima(const timeseries_t *y,
                      const matrix_t *xreg,
                      const ts_arima_spec_t *model_spec,
                      const ts_fit_options_t *options,
                      const ts_backtest_spec_t *spec,
                      ts_accuracy_t *out);

void ts_forecast_clear(ts_forecast_t *out);

#endif /* TIMESERIES_H */
