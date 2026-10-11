/**
 * @file tba_worker_internal.h
 * @brief Private fitting configuration and output boundary within the worker module.
 *
 * Configuration borrows validated argument strings and, for in-memory fitting,
 * observation buffers that remain alive for the complete calculation.
 * Only worker sources consume this header; the server uses tba_worker.h.
 */
#ifndef TBA_WORKER_INTERNAL_H
#define TBA_WORKER_INTERNAL_H
#include "timeseries-t.h"
#include "tba_support.h"

typedef enum {
    TBA_WORKER_MODEL_REGRESSION = 0,
    TBA_WORKER_MODEL_ARIMA,
    TBA_WORKER_MODEL_ARIMAX,
    TBA_WORKER_MODEL_SARIMA,
    TBA_WORKER_MODEL_SARIMAX,
    TBA_WORKER_MODEL_AUTO_ARIMA
} tba_worker_model_t;

typedef struct {
    const string_t *target_text;
    const string_t *driver_text;
    const char *target_date_column;
    const char *target_value_column;
    const char *xreg_date_column;
    char **xreg_columns;
    size_t xreg_column_count;
    ts_frequency_t frequency;
    ts_year_type_t year_type;
    tba_worker_model_t model;
    ts_arima_spec_t spec;
    ts_information_criterion_t criterion;
    size_t horizon;
    double level;
} tba_worker_config_t;


/**
 * @brief Fit a model in the isolated worker.
 * @param cfg Borrowed validated configuration.
 * @return Owned structured result or error, or NULL on allocation failure.
 */
json_t *tba_worker_run(const tba_worker_config_t *cfg);

/**
 * @brief Build downloadable CSV and text for a fitted series.
 * @param actual Borrowed observations.
 * @param fitted Borrowed fitted observations.
 * @param forecast Borrowed future forecasts.
 * @param csv_out Required destination for malloc-owned CSV text.
 * @param text_out Required destination for malloc-owned readable text.
 * @return Zero on success, minus one on allocation failure; outputs remain unchanged on failure.
 */
int tba_worker_build_results_outputs(const timeseries_t *actual, const timeseries_t *fitted,
                                    const ts_forecast_t *forecast, char **csv_out, char **text_out);
#endif
