/**
 * @file tba_worker.c
 * @brief Validated configuration for server-owned isolated forecast workers.
 *
 * Runs regression and ARIMA-family fitting through the MARS timeseries API.
 * The in-memory entry point returns results directly and never uses dataset
 * paths. The private command-line entry point publishes Protobuf results while
 * the server's transport is being migrated. Neither entry point selects a
 * default dataset or data column.
 */

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "string-t.h"
#include "tba_worker.h"
#define MARS_TBA_INTERNAL_ACCESS
#include "tba_worker_internal.h"

static json_t *tba_worker_fail(const char *message)
{
    return tba_json_error(message);
}

static int tba_worker_parse_size(const char *text, size_t *out)
{
    string_t *input = string_new_with(text);
    double value = 0;
    bool ok = input && tba_text_number(input, &value) && value >= 0 && value <= 10000 && value == floor(value);
    string_free(input);
    if (ok)
        *out = (size_t)value;
    return ok ? 0 : -1;
}

static int tba_worker_parse_double(const char *text, double *out)
{
    string_t *input = string_new_with(text);
    bool ok = input && out && tba_text_number(input, out) && *out > 0 && *out < 1;
    string_free(input);
    return ok ? 0 : -1;
}

static void tba_worker_free_string_list(char **items, size_t count)
{
    size_t i;

    if (!items)
        return;
    for (i = 0u; i < count; ++i)
        free(items[i]);
    free(items);
}

static char **tba_worker_split_columns(const char *text, size_t *count_out)
{
    *count_out = 0;
    if (!text || !*text)
        return NULL;
    string_t *input = string_new_with(text);
    size_t count = 0;
    string_t **parts = input ? string_split(input, ",", &count) : NULL;
    char **items = parts && count <= 32 ? calloc(count, sizeof *items) : NULL;
    bool ok = items != NULL;
    for (size_t i = 0; ok && i < count; ++i) {
        string_trim(parts[i]);
        items[i] = strdup(string_c_str(parts[i]));
        ok = items[i] && *items[i];
    }
    string_split_free(parts, count);
    string_free(input);
    if (!ok) {
        tba_worker_free_string_list(items, count);
        return NULL;
    }
    *count_out = count;
    return items;
}

static int tba_worker_parse_frequency(const char *text, ts_frequency_t *out)
{
    if (!text || !out)
        return -1;
    if (strcmp(text, "daily") == 0)
        *out = TS_FREQ_DAILY;
    else if (strcmp(text, "monthly") == 0)
        *out = TS_FREQ_MONTHLY;
    else if (strcmp(text, "quarterly") == 0)
        *out = TS_FREQ_QUARTERLY;
    else if (strcmp(text, "yearly") == 0)
        *out = TS_FREQ_YEARLY;
    else
        return -1;
    return 0;
}

static int tba_worker_parse_year_type(const char *text, ts_year_type_t *out)
{
    if (!text || !out)
        return -1;
    if (strcmp(text, "calendar") == 0)
        *out = TS_YEAR_CALENDAR;
    else if (strcmp(text, "fiscal") == 0)
        *out = TS_YEAR_FISCAL_UK_APR;
    else
        return -1;
    return 0;
}

static int tba_worker_parse_model(const char *text, tba_worker_model_t *out)
{
    if (!text || !out)
        return -1;
    if (strcmp(text, "regression") == 0)
        *out = TBA_WORKER_MODEL_REGRESSION;
    else if (strcmp(text, "arima") == 0)
        *out = TBA_WORKER_MODEL_ARIMA;
    else if (strcmp(text, "arimax") == 0)
        *out = TBA_WORKER_MODEL_ARIMAX;
    else if (strcmp(text, "sarima") == 0)
        *out = TBA_WORKER_MODEL_SARIMA;
    else if (strcmp(text, "sarimax") == 0)
        *out = TBA_WORKER_MODEL_SARIMAX;
    else if (strcmp(text, "auto-arima") == 0 || strcmp(text, "auto_arima") == 0)
        *out = TBA_WORKER_MODEL_AUTO_ARIMA;
    else
        return -1;
    return 0;
}

static int tba_worker_parse_criterion(const char *text, ts_information_criterion_t *out)
{
    if (!text || !out)
        return -1;
    if (strcmp(text, "aic") == 0)
        *out = TS_IC_AIC;
    else if (strcmp(text, "aicc") == 0)
        *out = TS_IC_AICC;
    else if (strcmp(text, "bic") == 0)
        *out = TS_IC_BIC;
    else
        return -1;
    return 0;
}

static void tba_worker_config_init(tba_worker_config_t *cfg)
{
    if (!cfg)
        return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->target_date_column = "";
    cfg->target_value_column = "";
    cfg->xreg_date_column = "";
    cfg->frequency = TS_FREQ_MONTHLY;
    cfg->year_type = TS_YEAR_FISCAL_UK_APR;
    cfg->model = TBA_WORKER_MODEL_SARIMAX;
    cfg->spec.p = 1u;
    cfg->spec.d = 1u;
    cfg->spec.q = 0u;
    cfg->spec.P = 1u;
    cfg->spec.D = 0u;
    cfg->spec.Q = 0u;
    cfg->spec.season_period = 12u;
    cfg->spec.include_mean = true;
    cfg->spec.include_intercept = true;
    cfg->spec.estimation = TS_EST_CSS;
    cfg->criterion = TS_IC_AIC;
    cfg->horizon = 6u;
    cfg->level = 0.95;
}

static int tba_worker_parse_args(tba_worker_config_t *cfg, int argc, const char *const *argv)
{
    int i;

    if (!cfg)
        return -1;
    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        const char *value;

        if (i + 1 >= argc)
            return -1;
        value = argv[++i];
        if (strcmp(arg, "--target-date-column") == 0)
            cfg->target_date_column = value;
        else if (strcmp(arg, "--target-value-column") == 0)
            cfg->target_value_column = value;
        else if (strcmp(arg, "--xreg-date-column") == 0)
            cfg->xreg_date_column = value;
        else if (strcmp(arg, "--xreg-cols") == 0) {
            tba_worker_free_string_list(cfg->xreg_columns, cfg->xreg_column_count);
            cfg->xreg_columns = tba_worker_split_columns(value, &cfg->xreg_column_count);
            if (value[0] && !cfg->xreg_columns)
                return -1;
        } else if (strcmp(arg, "--model") == 0) {
            if (tba_worker_parse_model(value, &cfg->model) != 0)
                return -1;
        } else if (strcmp(arg, "--frequency") == 0) {
            if (tba_worker_parse_frequency(value, &cfg->frequency) != 0)
                return -1;
        } else if (strcmp(arg, "--year-type") == 0) {
            if (tba_worker_parse_year_type(value, &cfg->year_type) != 0)
                return -1;
        } else if (strcmp(arg, "--horizon") == 0) {
            if (tba_worker_parse_size(value, &cfg->horizon) != 0)
                return -1;
        } else if (strcmp(arg, "--p") == 0) {
            if (tba_worker_parse_size(value, &cfg->spec.p) != 0)
                return -1;
        } else if (strcmp(arg, "--d") == 0) {
            if (tba_worker_parse_size(value, &cfg->spec.d) != 0)
                return -1;
        } else if (strcmp(arg, "--q") == 0) {
            if (tba_worker_parse_size(value, &cfg->spec.q) != 0)
                return -1;
        } else if (strcmp(arg, "--P") == 0) {
            if (tba_worker_parse_size(value, &cfg->spec.P) != 0)
                return -1;
        } else if (strcmp(arg, "--D") == 0) {
            if (tba_worker_parse_size(value, &cfg->spec.D) != 0)
                return -1;
        } else if (strcmp(arg, "--Q") == 0) {
            if (tba_worker_parse_size(value, &cfg->spec.Q) != 0)
                return -1;
        } else if (strcmp(arg, "--season-period") == 0) {
            if (tba_worker_parse_size(value, &cfg->spec.season_period) != 0)
                return -1;
        } else if (strcmp(arg, "--criterion") == 0) {
            if (tba_worker_parse_criterion(value, &cfg->criterion) != 0)
                return -1;
        } else if (strcmp(arg, "--level") == 0) {
            if (tba_worker_parse_double(value, &cfg->level) != 0)
                return -1;
        } else
            return -1;
    }
    return 0;
}

/* Borrow observations for this calculation; never resolve paths or publish files. */
json_t *tba_worker_memory(int argc, const char *const *argv, const string_t *target, const string_t *drivers)
{
    tba_worker_config_t cfg;
    tba_worker_config_init(&cfg);
    cfg.target_text = target;
    cfg.driver_text = drivers;
    int parsed = tba_worker_parse_args(&cfg, argc, argv);
    json_t *result = parsed ? tba_worker_fail("Invalid forecast worker arguments.") : tba_worker_run(&cfg);
    tba_worker_free_string_list(cfg.xreg_columns, cfg.xreg_column_count);
    return result;
}
