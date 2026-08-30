# `timeseries_t`

`timeseries_t` is the planned MARS time-series and forecasting surface for
datetime-indexed numeric data.

It is designed to sit directly on top of:

- `number_t` for exact, fixed-precision, and multiprecision numeric work
- `matrix_t` for regression design matrices, exogenous regressors, and
  coefficient-oriented outputs
- `datetime_t` for date indexing, slicing, aggregation, and forecast horizon
  generation

## Scope

The initial API in [`include/timeseries.h`](../include/timeseries.h) is aimed
at practical forecasting workflows such as:

- multiple regression
- ARIMA
- ARIMAX
- SARIMA
- SARIMAX
- rolling backtests and forecast evaluation

The current frequency policy is intentionally narrow and explicit:

- daily
- monthly
- quarterly
- yearly

Sub-daily support is left for a later extension even though `datetime_t`
internally supports second-resolution values.

## Calendar conventions

The public API distinguishes between:

- calendar year reporting
- fiscal year reporting via `TS_YEAR_FISCAL_UK_APR`

The fiscal-year mode uses the UK local-government style year beginning on
`01/04/yyyy` and ending on `31/03/yyyy+1`.

This is useful for aggregations, fiscal-year rollups, seasonal summaries, and
forecast output intended for public-sector reporting.

## Design principles

- `timeseries_t` is opaque
- scalar observations remain `number_t`
- exogenous regressors and coefficient tables use `matrix_t`
- datetime alignment is a first-class concern
- regression and ARIMA-family models share common fit controls
- result structs own their embedded `matrix_t *` and `timeseries_t *` outputs
  until cleared with the matching `*_clear(...)` helper

## Data ingestion

The first concrete use case for this API is CSV-backed forecasting data.

The header therefore includes helpers to:

- load one dated numeric column as a `timeseries_t` with `ts_from_csv_text(...)`
- load several dated numeric columns as a `matrix_t` with `ts_matrix_from_csv_text(...)`
- use `ts_from_csv(...)` and `ts_matrix_from_csv(...)` as literal-friendly
  wrappers when a caller already has ordinary C strings at the boundary

This matches workflows where the target series and exogenous population series
already exist as monthly CSV files, such as:

- `sample_data/Monthly Target Numbers.csv`
- `sample_data/Monthly Population.csv`

## Main capability areas

- construction from arrays or datetime indices
- CSV loading
- text and CSV output
- date slicing and alignment
- lagging, differencing, seasonal differencing
- rolling statistics
- frequency conversion and aggregation
- regression fitting and forecasting
- ARIMA-family fitting and forecasting
- diagnostics such as ACF, PACF, Ljung-Box, ADF, and KPSS
- accuracy metrics and rolling-origin backtesting

## Output and reporting

The public API includes both string formatting helpers and file writers.

This is intended to support:

- quick terminal inspection
- plain-text model summaries for reports
- CSV exports for forecasts, fitted values, coefficient tables, and diagnostics

The main entry points are:

- `ts_to_text(...)`
- `ts_forecast_to_text(...)`
- `ts_regression_summary_to_text(...)`
- `ts_arima_summary_to_text(...)`
- `ts_write_file(...)`
- `ts_forecast_write_file(...)`
- `ts_regression_summary_write_file(...)`
- `ts_arima_summary_write_file(...)`

The older `*_to_string(...)` helpers remain as C-string convenience wrappers
for interoperability boundaries. New code should prefer the `string_t`
returning `*_to_text(...)` functions.

`TS_STRING_CSV` is the machine-friendly export mode. The text styles are for
human-readable output.

## Notes

- All public declarations are in `include/timeseries.h`.
- The header currently defines the intended stable public API surface.
- Implementation can be staged behind this API without committing to all
  internal algorithms at once.

## API Reference

### `ts_accuracy()`

Returns the public result described by accuracy.

```c
int ts_accuracy(const timeseries_t *actual, const timeseries_t *predicted, ts_accuracy_t *out);
```

### `ts_acf()`

Returns the public result described by acf.

```c
int ts_acf(const timeseries_t *series, size_t max_lag, matrix_t **out);
```

### `ts_adf()`

Returns the public result described by adf.

```c
int ts_adf(const timeseries_t *series, number_t *statistic, number_t *p_value);
```

### `ts_aggregate_max()`

Returns the public result described by aggregate max.

```c
timeseries_t *ts_aggregate_max(const timeseries_t *series, ts_frequency_t target_frequency, ts_year_type_t year_type);
```

### `ts_aggregate_mean()`

Returns the public result described by aggregate mean.

```c
timeseries_t *ts_aggregate_mean(const timeseries_t *series, ts_frequency_t target_frequency, ts_year_type_t year_type);
```

### `ts_aggregate_min()`

Returns the public result described by aggregate min.

```c
timeseries_t *ts_aggregate_min(const timeseries_t *series, ts_frequency_t target_frequency, ts_year_type_t year_type);
```

### `ts_aggregate_sum()`

Returns the public result described by aggregate sum.

```c
timeseries_t *ts_aggregate_sum(const timeseries_t *series, ts_frequency_t target_frequency, ts_year_type_t year_type);
```

### `ts_align_pair()`

Returns the public result described by align pair.

```c
int ts_align_pair(const timeseries_t *left, const timeseries_t *right, ts_join_type_t join_type, timeseries_t **left_out, timeseries_t **right_out);
```

### `ts_arima_fit()`

Returns the public result described by arima fit.

```c
int ts_arima_fit(const timeseries_t *y, const matrix_t *xreg, const ts_arima_spec_t *spec, const ts_fit_options_t *options, ts_arima_result_t *out);
```

### `ts_arima_forecast()`

Returns the public result described by arima forecast.

```c
int ts_arima_forecast(const ts_arima_result_t *model, const timeseries_t *history, const matrix_t *future_xreg, size_t horizon, number_t level, ts_forecast_t *out);
```

### `ts_arima_is_invertible()`

Reports whether the condition described by arima is invertible holds.

```c
bool ts_arima_is_invertible(const ts_arima_result_t *model);
```

### `ts_arima_is_stationary()`

Reports whether the condition described by arima is stationary holds.

```c
bool ts_arima_is_stationary(const ts_arima_result_t *model);
```

### `ts_arima_ljung_box()`

Returns the public result described by arima ljung box.

```c
int ts_arima_ljung_box(const ts_arima_result_t *model, size_t max_lag, number_t *statistic, number_t *p_value);
```

### `ts_arima_residual_acf()`

Returns the public result described by arima residual acf.

```c
int ts_arima_residual_acf(const ts_arima_result_t *model, size_t max_lag, matrix_t **out);
```

### `ts_arima_result_clear()`

Releases or clears the resources associated with arima result clear.

```c
void ts_arima_result_clear(ts_arima_result_t *out);
```

### `ts_arima_summary_to_string()`

Returns the public result described by arima summary to string.

```c
char *ts_arima_summary_to_string(const ts_arima_result_t *result);
```

### `ts_as_frequency()`

Returns the public result described by as frequency.

```c
timeseries_t *ts_as_frequency(const timeseries_t *series, ts_frequency_t target_frequency, ts_missing_policy_t missing_policy);
```

### `ts_auto_arima()`

Returns the public result described by auto arima.

```c
int ts_auto_arima(const timeseries_t *y, const matrix_t *xreg, size_t max_p, size_t max_d, size_t max_q, size_t max_P, size_t max_D, size_t max_Q, size_t season_period, ts_information_criterion_t criterion, const ts_fit_options_t *options, ts_arima_spec_t *best_spec, ts_arima_result_t *best_fit);
```

### `ts_backtest_arima()`

Returns the public result described by backtest arima.

```c
int ts_backtest_arima(const timeseries_t *y, const matrix_t *xreg, const ts_arima_spec_t *model_spec, const ts_fit_options_t *options, const ts_backtest_spec_t *spec, ts_accuracy_t *out);
```

### `ts_backtest_regression()`

Returns the public result described by backtest regression.

```c
int ts_backtest_regression(const timeseries_t *y, const matrix_t *xreg, const ts_fit_options_t *options, const ts_backtest_spec_t *spec, ts_accuracy_t *out);
```

### `ts_bind_columns()`

Returns the public result described by bind columns.

```c
matrix_t *ts_bind_columns(timeseries_t *const *series, size_t count, ts_join_type_t join_type);
```

### `ts_box_cox()`

Returns the public result described by box cox.

```c
timeseries_t *ts_box_cox(const timeseries_t *series, const number_t *lambda);
```

### `ts_builder_append()`

Returns the public result described by builder append.

```c
int ts_builder_append(ts_builder_t *builder, const datetime_t *datetime, const number_t *value);
```

### `ts_builder_append_date_string_double()`

Returns the public result described by builder append date string double.

```c
int ts_builder_append_date_string_double(ts_builder_t *builder, const char *date_text, double value);
```

### `ts_builder_append_date_text_double()`

Returns the public result described by builder append date text double.

```c
int ts_builder_append_date_text_double(ts_builder_t *builder, const string_t *date_text, double value);
```

### `ts_builder_append_double()`

Returns the public result described by builder append double.

```c
int ts_builder_append_double(ts_builder_t *builder, const datetime_t *datetime, double value);
```

### `ts_builder_build()`

Returns the public result described by builder build.

```c
timeseries_t *ts_builder_build(const ts_builder_t *builder);
```

### `ts_builder_destroy()`

Releases or clears the resources associated with builder destroy.

```c
void ts_builder_destroy(ts_builder_t *builder);
```

### `ts_builder_new()`

Creates or reconstructs the public value described by builder new.

```c
ts_builder_t *ts_builder_new(ts_frequency_t frequency, ts_year_type_t year_type);
```

### `ts_ccf()`

Returns the public result described by ccf.

```c
int ts_ccf(const timeseries_t *x, const timeseries_t *y, size_t max_lag, matrix_t **out);
```

### `ts_clone()`

Creates or reconstructs the public value described by clone.

```c
timeseries_t *ts_clone(const timeseries_t *series);
```

### `ts_cumsum()`

Returns the public result described by cumsum.

```c
timeseries_t *ts_cumsum(const timeseries_t *series);
```

### `ts_deserialise()`

Creates or reconstructs the public value described by deserialise.

```c
timeseries_t *ts_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding);
```

### `ts_design_matrix_lags()`

Returns the public result described by design matrix lags.

```c
matrix_t *ts_design_matrix_lags(const timeseries_t *series, size_t max_lag, bool include_intercept);
```

### `ts_design_matrix_seasonal_dummies()`

Returns the public result described by design matrix seasonal dummies.

```c
matrix_t *ts_design_matrix_seasonal_dummies(const timeseries_t *series, size_t season_period, bool drop_first);
```

### `ts_design_matrix_trend()`

Returns the public result described by design matrix trend.

```c
matrix_t *ts_design_matrix_trend(const timeseries_t *series, bool include_intercept, bool include_linear, bool include_quadratic);
```

### `ts_diff()`

Returns the public result described by diff.

```c
timeseries_t *ts_diff(const timeseries_t *series, size_t differences);
```

### `ts_drop_missing()`

Returns the public result described by drop missing.

```c
timeseries_t *ts_drop_missing(const timeseries_t *series);
```

### `ts_end_datetime()`

Returns the public result described by end datetime.

```c
datetime_t *ts_end_datetime(const timeseries_t *series);
```

### `ts_exp()`

Returns the public result described by exp.

```c
timeseries_t *ts_exp(const timeseries_t *series);
```

### `ts_fill_missing()`

Returns the public result described by fill missing.

```c
timeseries_t *ts_fill_missing(const timeseries_t *series, ts_missing_policy_t policy);
```

### `ts_forecast_clear()`

Releases or clears the resources associated with forecast clear.

```c
void ts_forecast_clear(ts_forecast_t *out);
```

### `ts_forecast_to_string()`

Returns the public result described by forecast to string.

```c
char *ts_forecast_to_string(const ts_forecast_t *forecast, ts_string_style_t style);
```

### `ts_free()`

Releases or clears the resources associated with free.

```c
void ts_free(timeseries_t *series);
```

### `ts_frequency()`

Returns the public result described by frequency.

```c
ts_frequency_t ts_frequency(const timeseries_t *series);
```

### `ts_get_datetime()`

Returns the public result described by get datetime.

```c
int ts_get_datetime(const timeseries_t *series, size_t index, datetime_t *out);
```

### `ts_get_value()`

Returns the public result described by get value.

```c
int ts_get_value(const timeseries_t *series, size_t index, number_t *out);
```

### `ts_has_missing()`

Reports whether the condition described by has missing holds.

```c
bool ts_has_missing(const timeseries_t *series);
```

### `ts_head()`

Returns the public result described by head.

```c
timeseries_t *ts_head(const timeseries_t *series, size_t n);
```

### `ts_index_info()`

Returns the public result described by index info.

```c
ts_index_info_t ts_index_info(const timeseries_t *series);
```

### `ts_is_regular()`

Reports whether the condition described by is regular holds.

```c
bool ts_is_regular(const timeseries_t *series);
```

### `ts_kpss()`

Returns the public result described by kpss.

```c
int ts_kpss(const timeseries_t *series, number_t *statistic, number_t *p_value);
```

### `ts_lag()`

Returns the public result described by lag.

```c
timeseries_t *ts_lag(const timeseries_t *series, size_t lag);
```

### `ts_lead()`

Returns the public result described by lead.

```c
timeseries_t *ts_lead(const timeseries_t *series, size_t lead);
```

### `ts_length()`

Returns the public result described by length.

```c
size_t ts_length(const timeseries_t *series);
```

### `ts_ljung_box()`

Returns the public result described by ljung box.

```c
int ts_ljung_box(const timeseries_t *series, size_t max_lag, number_t *statistic, number_t *p_value);
```

### `ts_log()`

Returns the public result described by log.

```c
timeseries_t *ts_log(const timeseries_t *series);
```

### `ts_new()`

Creates or reconstructs the public value described by new.

```c
timeseries_t *ts_new(const number_t *values, size_t length);
```

### `ts_new_from_doubles()`

Creates or reconstructs the public value described by new from doubles.

```c
timeseries_t *ts_new_from_doubles(const double *values, size_t length);
```

### `ts_new_indexed()`

Creates or reconstructs the public value described by new indexed.

```c
timeseries_t *ts_new_indexed(const number_t *values, const datetime_t *const *index, size_t length, ts_frequency_t frequency, ts_year_type_t year_type);
```

### `ts_new_indexed_from_doubles()`

Creates or reconstructs the public value described by new indexed from doubles.

```c
timeseries_t *ts_new_indexed_from_doubles(const double *values, const datetime_t *const *index, size_t length, ts_frequency_t frequency, ts_year_type_t year_type);
```

### `ts_new_regular()`

Creates or reconstructs the public value described by new regular.

```c
timeseries_t *ts_new_regular(const number_t *values, size_t length, const datetime_t *start, ts_frequency_t frequency, ts_year_type_t year_type);
```

### `ts_new_regular_from_doubles()`

Creates or reconstructs the public value described by new regular from doubles.

```c
timeseries_t *ts_new_regular_from_doubles(const double *values, size_t length, const datetime_t *start, ts_frequency_t frequency, ts_year_type_t year_type);
```

### `ts_pacf()`

Returns the public result described by pacf.

```c
int ts_pacf(const timeseries_t *series, size_t max_lag, matrix_t **out);
```

### `ts_print()`

Performs the public operation described by print.

```c
void ts_print(const timeseries_t *series);
```

### `ts_printf()`

Returns the public result described by printf.

```c
int ts_printf(const char *fmt, ...);
```

### `ts_regression_fit()`

Returns the public result described by regression fit.

```c
int ts_regression_fit(const timeseries_t *y, const matrix_t *xreg, const ts_fit_options_t *options, ts_regression_result_t *out);
```

### `ts_regression_forecast()`

Returns the public result described by regression forecast.

```c
int ts_regression_forecast(const ts_regression_result_t *model, const matrix_t *future_xreg, const timeseries_t *history, ts_frequency_t frequency, ts_year_type_t year_type, number_t level, ts_forecast_t *out);
```

### `ts_regression_result_clear()`

Releases or clears the resources associated with regression result clear.

```c
void ts_regression_result_clear(ts_regression_result_t *out);
```

### `ts_regression_summary_to_string()`

Returns the public result described by regression summary to string.

```c
char *ts_regression_summary_to_string(const ts_regression_result_t *result);
```

### `ts_roll_mean()`

Returns the public result described by roll mean.

```c
timeseries_t *ts_roll_mean(const timeseries_t *series, size_t window);
```

### `ts_roll_std()`

Returns the public result described by roll std.

```c
timeseries_t *ts_roll_std(const timeseries_t *series, size_t window);
```

### `ts_roll_sum()`

Returns the public result described by roll sum.

```c
timeseries_t *ts_roll_sum(const timeseries_t *series, size_t window);
```

### `ts_roll_var()`

Returns the public result described by roll var.

```c
timeseries_t *ts_roll_var(const timeseries_t *series, size_t window);
```

### `ts_seasonal_diff()`

Returns the public result described by seasonal diff.

```c
timeseries_t *ts_seasonal_diff(const timeseries_t *series, size_t differences, size_t season_period);
```

### `ts_serialize()`

Reports whether the condition described by serialize holds.

```c
bool ts_serialize(const timeseries_t *series, string_t **out_type, string_t **out_encoding, void **out_data, size_t *out_len);
```

### `ts_slice()`

Returns the public result described by slice.

```c
timeseries_t *ts_slice(const timeseries_t *series, size_t start, size_t length);
```

### `ts_slice_date()`

Returns the public result described by slice date.

```c
timeseries_t *ts_slice_date(const timeseries_t *series, const datetime_t *start, const datetime_t *end);
```

### `ts_sprintf()`

Returns the public result described by sprintf.

```c
int ts_sprintf(char *out, size_t out_size, const char *fmt, ...);
```

### `ts_sprintf_text()`

Returns the public result described by sprintf text.

```c
string_t *ts_sprintf_text(const char *fmt, ...);
```

### `ts_start_datetime()`

Returns the public result described by start datetime.

```c
datetime_t *ts_start_datetime(const timeseries_t *series);
```

### `ts_tail()`

Returns the public result described by tail.

```c
timeseries_t *ts_tail(const timeseries_t *series, size_t n);
```

### `ts_to_column_matrix()`

Returns the public result described by to column matrix.

```c
matrix_t *ts_to_column_matrix(const timeseries_t *series);
```

### `ts_to_row_matrix()`

Returns the public result described by to row matrix.

```c
matrix_t *ts_to_row_matrix(const timeseries_t *series);
```

### `ts_to_string()`

Returns the public result described by to string.

```c
char *ts_to_string(const timeseries_t *series, ts_string_style_t style);
```

### `ts_vsprintf_text()`

Returns the public result described by vsprintf text.

```c
string_t *ts_vsprintf_text(const char *fmt, va_list ap);
```

### `ts_year_type()`

Returns the public result described by year type.

```c
ts_year_type_t ts_year_type(const timeseries_t *series);
```
