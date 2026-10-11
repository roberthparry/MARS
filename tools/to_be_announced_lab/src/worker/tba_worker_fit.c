/**
 * @file tba_worker_fit.c
 * @brief Native model fitting and aligned driver preparation.
 *
 * Owns temporary time series, matrices and fit results for one isolated request.
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "datetime.h"
#include "matrix.h"
#include "number.h"
#include "tba_csv.h"
#include "tba_worker_internal.h"

static timeseries_t *tba_worker_observations(const tba_worker_config_t *cfg, bool driver, const char *column)
{
    const char *date = driver ? cfg->xreg_date_column : cfg->target_date_column;
    tba_csv_t *csv = tba_csv_parse(driver ? cfg->driver_text : cfg->target_text);
    size_t date_index = date && *date ? tba_csv_column(csv, date) : 0;
    size_t value_index = tba_csv_column(csv, column);
    ts_builder_t *builder = csv ? ts_builder_new(cfg->frequency, cfg->year_type) : NULL;
    bool ok = builder && date_index < tba_csv_columns(csv) && value_index < tba_csv_columns(csv);
    for (size_t row = 0; ok && row < tba_csv_rows(csv); ++row) {
        double value;
        if (tba_text_number(tba_csv_cell(csv, row, value_index), &value))
            ok = ts_builder_append_date_text_double(builder, tba_csv_cell(csv, row, date_index), value) == 0;
    }
    timeseries_t *result = ok ? ts_builder_build(builder) : NULL;
    ts_builder_destroy(builder);
    tba_csv_free(csv);
    return result;
}

static const char *tba_worker_model_name(tba_worker_model_t model)
{
    static const char *const names[] = {"Regression", "ARIMA", "ARIMAX", "SARIMA", "SARIMAX", "Auto-ARIMA"};
    return (unsigned)model < sizeof names / sizeof *names ? names[model] : "Forecast";
}

static void tba_worker_advance_datetime(datetime_t *dt, ts_frequency_t frequency)
{
    int preserve_month_end;

    if (!dt)
        return;
    preserve_month_end = datetime_day(dt) == datetime_days_in_month(datetime_year(dt), datetime_month(dt));
    switch (frequency) {
        case TS_FREQ_DAILY:
            datetime_add_days(dt, 1L);
            break;
        case TS_FREQ_MONTHLY:
            datetime_add_months(dt, 1);
            break;
        case TS_FREQ_QUARTERLY:
            datetime_add_months(dt, 3);
            break;
        case TS_FREQ_YEARLY:
            datetime_add_years(dt, 1);
            break;
        default:
            return;
    }
    if (preserve_month_end) {
        short year = datetime_year(dt);
        month_t month = datetime_month(dt);
        uint8_t last_day = (uint8_t)datetime_days_in_month(year, month);

        datetime_init_ymdt(dt, year, month, last_day, datetime_hour(dt), datetime_minute(dt), datetime_second(dt));
    }
}

static int tba_worker_load_x_columns(const tba_worker_config_t *cfg, const timeseries_t *y,
                                          timeseries_t **aligned_y_out, matrix_t **fit_x_out, matrix_t **future_x_out)
{
    timeseries_t **raw = NULL;
    timeseries_t **aligned = NULL;
    timeseries_t **future = NULL;
    timeseries_t *common_y = NULL;
    matrix_t *fit_x = NULL;
    matrix_t *future_x = NULL;
    size_t i;

    if (aligned_y_out)
        *aligned_y_out = NULL;
    if (fit_x_out)
        *fit_x_out = NULL;
    if (future_x_out)
        *future_x_out = NULL;

    if (!cfg || !y || !aligned_y_out || !fit_x_out || !future_x_out)
        return -1;
    if (cfg->xreg_column_count == 0u) {
        *aligned_y_out = ts_clone(y);
        return *aligned_y_out ? 0 : -1;
    }

    raw = calloc(cfg->xreg_column_count, sizeof(*raw));
    aligned = calloc(cfg->xreg_column_count, sizeof(*aligned));
    future = calloc(cfg->xreg_column_count, sizeof(*future));
    common_y = ts_clone(y);
    if (!raw || !aligned || !future || !common_y)
        goto fail;

    for (i = 0u; i < cfg->xreg_column_count; ++i) {
        raw[i] = tba_worker_observations(cfg, true, cfg->xreg_columns[i]);
        if (!raw[i])
            goto fail;
    }

    for (i = 0u; i < cfg->xreg_column_count; ++i) {
        timeseries_t *new_y = NULL;
        timeseries_t *x_aligned = NULL;

        if (ts_align_pair(common_y, raw[i], TS_JOIN_INNER, &new_y, &x_aligned) != 0)
            goto fail;
        ts_free(common_y);
        ts_free(x_aligned);
        common_y = new_y;
        if (!common_y || ts_length(common_y) == 0u)
            goto fail;
    }

    for (i = 0u; i < cfg->xreg_column_count; ++i) {
        timeseries_t *tmp_y = NULL;

        if (ts_align_pair(common_y, raw[i], TS_JOIN_INNER, &tmp_y, &aligned[i]) != 0)
            goto fail;
        ts_free(tmp_y);
        if (!aligned[i])
            goto fail;
    }
    fit_x = ts_bind_columns(aligned, cfg->xreg_column_count, TS_JOIN_INNER);
    if (!fit_x)
        goto fail;

    if (cfg->horizon > 0u) {
        datetime_t *future_start = ts_end_datetime(common_y);

        if (!future_start)
            goto fail;
        tba_worker_advance_datetime(future_start, cfg->frequency);
        for (i = 0u; i < cfg->xreg_column_count; ++i) {
            datetime_t *x_end = ts_end_datetime(raw[i]);
            timeseries_t *window;

            if (!x_end) {
                datetime_dealloc(future_start);
                goto fail;
            }
            window = ts_slice_date(raw[i], future_start, x_end);
            datetime_dealloc(x_end);
            if (!window) {
                datetime_dealloc(future_start);
                goto fail;
            }
            future[i] = window;
            if (!future[i] || ts_length(future[i]) == 0u) {
                datetime_dealloc(future_start);
                goto fail;
            }
        }
        datetime_dealloc(future_start);
        future_x = ts_bind_columns(future, cfg->xreg_column_count, TS_JOIN_INNER);
        if (!future_x || mat_get_row_count(future_x) == 0u)
            goto fail;
    }

    *aligned_y_out = common_y;
    *fit_x_out = fit_x;
    *future_x_out = future_x;

    for (i = 0u; i < cfg->xreg_column_count; ++i) {
        ts_free(raw[i]);
        ts_free(aligned[i]);
        ts_free(future[i]);
    }
    free(raw);
    free(aligned);
    free(future);
    return 0;

fail:
    ts_free(common_y);
    mat_free(fit_x);
    mat_free(future_x);
    if (raw) {
        for (i = 0u; i < cfg->xreg_column_count; ++i)
            ts_free(raw[i]);
    }
    if (aligned) {
        for (i = 0u; i < cfg->xreg_column_count; ++i)
            ts_free(aligned[i]);
    }
    if (future) {
        for (i = 0u; i < cfg->xreg_column_count; ++i)
            ts_free(future[i]);
    }
    free(raw);
    free(aligned);
    free(future);
    return -1;
}

static json_t *tba_worker_success(const tba_worker_config_t *cfg, const char *forecast_csv,
                                  const char *forecast_text, size_t fit_rows, size_t horizon,
                                  bool stationary, bool invertible, const char *summary_text)
{
    json_t *result = json_new_object();
    bool ok = result && tba_json_flag(result, "ok", true) &&
        tba_json_string(result, "model", tba_worker_model_name(cfg->model)) &&
        tba_json_number(result, "fit_rows", fit_rows) && tba_json_number(result, "horizon", horizon) &&
        tba_json_flag(result, "stationary", stationary) && tba_json_flag(result, "invertible", invertible) &&
        tba_json_string(result, "summary_text", summary_text) &&
        tba_json_string(result, "forecast_csv", forecast_csv) &&
        tba_json_string(result, "forecast_text", forecast_text);
    if (!ok) {
        json_free(result);
        return NULL;
    }
    return result;
}

/* Fit through the native timeseries module; return owned structured values. */
json_t *tba_worker_run(const tba_worker_config_t *cfg)
{
    json_t *result = NULL;
    timeseries_t *y = NULL;
    timeseries_t *fit_y = NULL;
    matrix_t *fit_x = NULL;
    matrix_t *future_x = NULL;
    char *summary_text = NULL;
    char *forecast_csv = NULL;
    char *forecast_text = NULL;
    ts_forecast_t forecast = {0};
    number_t level;
    size_t effective_horizon = 0u;

    if (!cfg || !cfg->target_text || !cfg->target_value_column)
        return tba_json_error("Target dataset and value column are required.");

    y = tba_worker_observations(cfg, false, cfg->target_value_column);
    if (!y)
        return tba_json_error("Could not load the target time series.");
    if (tba_worker_load_x_columns(cfg, y, &fit_y, &fit_x, &future_x) != 0) {
        ts_free(y);
        return tba_json_error("Could not align the exogenous data with the target series.");
    }
    if ((cfg->model == TBA_WORKER_MODEL_REGRESSION || cfg->model == TBA_WORKER_MODEL_ARIMAX ||
         cfg->model == TBA_WORKER_MODEL_SARIMAX) &&
        !fit_x) {
        ts_free(y);
        ts_free(fit_y);
        return tba_json_error("This model needs at least one exogenous regressor column.");
    }
    level = num_create_from_double(cfg->level);
    effective_horizon = cfg->horizon;
    if (future_x) {
        size_t future_rows = mat_get_row_count(future_x);

        if (effective_horizon == 0u || future_rows < effective_horizon)
            effective_horizon = future_rows;
    }

    if (cfg->model == TBA_WORKER_MODEL_REGRESSION) {
        ts_regression_result_t fit = {0};

        if (ts_regression_fit(fit_y, fit_x, NULL, &fit) != 0) {
            num_destroy(&level);
            ts_free(y);
            ts_free(fit_y);
            mat_free(fit_x);
            mat_free(future_x);
            return tba_json_error("Regression fitting failed.");
        }
        if (effective_horizon > 0u && future_x &&
            ts_regression_forecast(&fit, future_x, fit_y, cfg->frequency, cfg->year_type, level, &forecast) != 0) {
            ts_regression_result_clear(&fit);
            num_destroy(&level);
            ts_free(y);
            ts_free(fit_y);
            mat_free(fit_x);
            mat_free(future_x);
            return tba_json_error("Regression forecasting failed.");
        }
        summary_text = ts_regression_summary_to_string(&fit);
        if (tba_worker_build_results_outputs(fit_y, fit.fitted, &forecast, &forecast_csv, &forecast_text) != 0) {
            ts_regression_result_clear(&fit);
            ts_forecast_clear(&forecast);
            num_destroy(&level);
            ts_free(y);
            ts_free(fit_y);
            mat_free(fit_x);
            mat_free(future_x);
            free(summary_text);
            return tba_json_error("Regression output formatting failed.");
        }
        if (!summary_text || !forecast_csv || !forecast_text) {
            ts_regression_result_clear(&fit);
            ts_forecast_clear(&forecast);
            num_destroy(&level);
            ts_free(y);
            ts_free(fit_y);
            mat_free(fit_x);
            mat_free(future_x);
            free(summary_text);
            free(forecast_csv);
            free(forecast_text);
            return tba_json_error("Regression output formatting failed.");
        }
        result = tba_worker_success(cfg, forecast_csv, forecast_text, ts_length(fit_y), effective_horizon, true, true,
                                      summary_text);
        ts_regression_result_clear(&fit);
    } else {
        ts_arima_spec_t spec = cfg->spec;
        ts_arima_result_t fit = {0};
        bool uses_xreg = (cfg->model == TBA_WORKER_MODEL_ARIMAX || cfg->model == TBA_WORKER_MODEL_SARIMAX ||
                          cfg->model == TBA_WORKER_MODEL_AUTO_ARIMA);
        bool seasonal = (cfg->model == TBA_WORKER_MODEL_SARIMA || cfg->model == TBA_WORKER_MODEL_SARIMAX);

        if (!seasonal) {
            spec.P = 0u;
            spec.D = 0u;
            spec.Q = 0u;
            spec.season_period = 0u;
        }
        if (cfg->model == TBA_WORKER_MODEL_AUTO_ARIMA) {
            ts_arima_spec_t best_spec = {0};

            if (ts_auto_arima(fit_y, uses_xreg ? fit_x : NULL, spec.p, spec.d, spec.q, spec.P, spec.D, spec.Q,
                              spec.season_period, cfg->criterion, NULL, &best_spec, &fit) != 0) {
                num_destroy(&level);
                ts_free(y);
                ts_free(fit_y);
                mat_free(fit_x);
                mat_free(future_x);
                return tba_json_error("Auto-ARIMA search failed.");
            }
        } else if (ts_arima_fit(fit_y, uses_xreg ? fit_x : NULL, &spec, NULL, &fit) != 0) {
            num_destroy(&level);
            ts_free(y);
            ts_free(fit_y);
            mat_free(fit_x);
            mat_free(future_x);
            return tba_json_error("ARIMA-family fitting failed.");
        }
        if (effective_horizon > 0u &&
            ts_arima_forecast(&fit, fit_y, uses_xreg ? future_x : NULL, effective_horizon, level, &forecast) != 0) {
            ts_arima_result_clear(&fit);
            num_destroy(&level);
            ts_free(y);
            ts_free(fit_y);
            mat_free(fit_x);
            mat_free(future_x);
            return tba_json_error("ARIMA-family forecasting failed.");
        }
        summary_text = ts_arima_summary_to_string(&fit);
        if (tba_worker_build_results_outputs(fit_y, fit.fitted, &forecast, &forecast_csv, &forecast_text) != 0) {
            ts_arima_result_clear(&fit);
            ts_forecast_clear(&forecast);
            num_destroy(&level);
            ts_free(y);
            ts_free(fit_y);
            mat_free(fit_x);
            mat_free(future_x);
            free(summary_text);
            return tba_json_error("ARIMA-family output formatting failed.");
        }
        if (!summary_text || !forecast_csv || !forecast_text) {
            ts_arima_result_clear(&fit);
            ts_forecast_clear(&forecast);
            num_destroy(&level);
            ts_free(y);
            ts_free(fit_y);
            mat_free(fit_x);
            mat_free(future_x);
            free(summary_text);
            free(forecast_csv);
            free(forecast_text);
            return tba_json_error("ARIMA-family output formatting failed.");
        }
        result = tba_worker_success(cfg, forecast_csv, forecast_text, ts_length(fit_y), effective_horizon,
                                      ts_arima_is_stationary(&fit), ts_arima_is_invertible(&fit), summary_text);
        ts_arima_result_clear(&fit);
    }

    free(summary_text);
    free(forecast_csv);
    free(forecast_text);
    ts_forecast_clear(&forecast);
    num_destroy(&level);
    ts_free(y);
    ts_free(fit_y);
    mat_free(fit_x);
    mat_free(future_x);
    return result;
}
