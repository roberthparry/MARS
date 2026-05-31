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

- load one dated numeric column as a `timeseries_t`
- load several dated numeric columns as a `matrix_t` for exogenous regressors

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

- `ts_to_string(...)`
- `ts_forecast_to_string(...)`
- `ts_regression_summary_to_string(...)`
- `ts_arima_summary_to_string(...)`
- `ts_write_file(...)`
- `ts_forecast_write_file(...)`
- `ts_regression_summary_write_file(...)`
- `ts_arima_summary_write_file(...)`

`TS_STRING_CSV` is the machine-friendly export mode. The text styles are for
human-readable output.

## Notes

- All public declarations are in `include/timeseries.h`.
- The header currently defines the intended stable public API surface.
- Implementation can be staged behind this API without committing to all
  internal algorithms at once.
