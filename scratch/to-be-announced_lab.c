#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "datetime.h"
#include "matrix.h"
#include "number.h"
#include "timeseries.h"
#include "ustring.h"

typedef enum {
    TO_BE_ANNOUNCED_MODEL_REGRESSION = 0,
    TO_BE_ANNOUNCED_MODEL_ARIMA,
    TO_BE_ANNOUNCED_MODEL_ARIMAX,
    TO_BE_ANNOUNCED_MODEL_SARIMA,
    TO_BE_ANNOUNCED_MODEL_SARIMAX,
    TO_BE_ANNOUNCED_MODEL_AUTO_ARIMA
} to_be_announced_model_t;

typedef struct {
    const char *target_path;
    const char *target_date_column;
    const char *target_value_column;
    const char *xreg_path;
    const char *xreg_date_column;
    char **xreg_columns;
    size_t xreg_column_count;
    ts_frequency_t frequency;
    ts_year_type_t year_type;
    to_be_announced_model_t model;
    ts_arima_spec_t spec;
    ts_information_criterion_t criterion;
    size_t horizon;
    double level;
} to_be_announced_config_t;

static void to_be_announced_print_json_string(const char *text)
{
    const unsigned char *bytes = (const unsigned char *)(text ? text : "");

    putchar('"');
    for (size_t i = 0u; bytes[i] != '\0'; ++i) {
        unsigned char ch = bytes[i];

        switch (ch) {
        case '\\': fputs("\\\\", stdout); break;
        case '"': fputs("\\\"", stdout); break;
        case '\b': fputs("\\b", stdout); break;
        case '\f': fputs("\\f", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (ch < 0x20u)
                printf("\\u%04x", (unsigned int)ch);
            else
                putchar((int)ch);
            break;
        }
    }
    putchar('"');
}

static int to_be_announced_fail(const char *message)
{
    fputs("{\"ok\":false,\"error\":", stdout);
    to_be_announced_print_json_string(message ? message : "Unknown error");
    fputs("}\n", stdout);
    return EXIT_FAILURE;
}

static int to_be_announced_parse_size(const char *text, size_t *out)
{
    size_t i = 0u;
    size_t value = 0u;
    int saw_digit = 0;

    if (!text || !out)
        return -1;
    if (text[i] == '+')
        i++;

    for (; text[i] != '\0'; ++i) {
        unsigned int digit;

        if (text[i] < '0' || text[i] > '9')
            return -1;
        digit = (unsigned int)(text[i] - '0');
        if (value > (((size_t)-1) - digit) / 10u)
            return -1;
        value = value * 10u + digit;
        saw_digit = 1;
    }

    if (!saw_digit)
        return -1;
    *out = value;
    return 0;
}

static int to_be_announced_parse_double(const char *text, double *out)
{
    number_t value;

    if (!text || !out)
        return -1;

    value = num_create_from_string(text);
    if (!num_is_real(value) || !num_is_finite(value)) {
        num_destroy(&value);
        return -1;
    }

    *out = num_to_double(value);
    num_destroy(&value);
    return 0;
}

static int to_be_announced_appendf(char **buf, size_t *len, size_t *cap, const char *fmt, ...)
{
    va_list ap;
    string_t *piece;
    const char *piece_text;
    size_t piece_len;
    size_t required;
    char *grown;

    if (!buf || !len || !cap || !fmt)
        return -1;
    va_start(ap, fmt);
    piece = string_vsprintf(fmt, ap);
    va_end(ap);
    if (!piece)
        return -1;

    piece_text = string_c_str(piece);
    piece_len = strlen(piece_text);
    required = *len + piece_len + 1u;
    if (required > *cap) {
        size_t new_cap = *cap ? *cap : 256u;

        while (new_cap < required)
            new_cap *= 2u;
        grown = realloc(*buf, new_cap);
        if (!grown) {
            string_free(piece);
            return -1;
        }
        *buf = grown;
        *cap = new_cap;
    }
    memcpy(*buf + *len, piece_text, piece_len + 1u);
    *len += piece_len;
    string_free(piece);
    return 0;
}

static char *to_be_announced_series_date_text(const timeseries_t *series, size_t index)
{
    datetime_t *dt;
    char *text;

    if (!series)
        return NULL;
    dt = datetime_alloc();
    if (!dt)
        return NULL;
    if (ts_get_datetime(series, index, dt) != 0) {
        datetime_dealloc(dt);
        return NULL;
    }
    text = datetime_format(dt, "%dd/%mm/%yyyy");
    datetime_dealloc(dt);
    return text;
}

static char *to_be_announced_series_value_text(const timeseries_t *series, size_t index)
{
    number_t value = NUM_ZERO;
    char buf[64];
    double d;
    char *text;

    if (!series || ts_get_value(series, index, &value) != 0)
        return NULL;
    d = num_to_double(value);
    if (isfinite(d)) {
        snprintf(buf, sizeof(buf), "%.2f", d);
        text = strdup(buf);
    } else {
        string_t *value_text = num_to_string(value);

        text = value_text ? strdup(string_c_str(value_text)) : NULL;
        string_free(value_text);
    }
    num_destroy(&value);
    return text;
}

static int to_be_announced_append_combined_csv_row(char **buf, size_t *len, size_t *cap,
                                           const char *date_text,
                                           const char *actual_text,
                                           const char *mean_text,
                                           const char *stderr_text,
                                           const char *lower_text,
                                           const char *upper_text)
{
    return to_be_announced_appendf(buf, len, cap, "%s,%s,%s,%s,%s,%s\n",
                           date_text ? date_text : "",
                           actual_text ? actual_text : "",
                           mean_text ? mean_text : "",
                           stderr_text ? stderr_text : "",
                           lower_text ? lower_text : "",
                           upper_text ? upper_text : "");
}

static int to_be_announced_append_combined_text_row(char **buf, size_t *len, size_t *cap,
                                            const char *date_text,
                                            const char *actual_text,
                                            const char *mean_text,
                                            const char *stderr_text,
                                            const char *lower_text,
                                            const char *upper_text)
{
    return to_be_announced_appendf(
        buf, len, cap,
        "%s: actual %s, mean %s%s%s%s%s%s%s\n",
        date_text ? date_text : "",
        actual_text ? actual_text : "n/a",
        mean_text ? mean_text : "n/a",
        stderr_text ? ", stderr " : "",
        stderr_text ? stderr_text : "",
        lower_text ? ", lower " : "",
        lower_text ? lower_text : "",
        upper_text ? ", upper " : "",
        upper_text ? upper_text : ""
    );
}

static int to_be_announced_build_results_outputs(const timeseries_t *actual,
                                         const timeseries_t *fitted,
                                         const ts_forecast_t *forecast,
                                         char **csv_out,
                                         char **text_out)
{
    char *csv = NULL, *text = NULL;
    size_t csv_len = 0u, csv_cap = 0u, text_len = 0u, text_cap = 0u;
    size_t i;

    if (!csv_out || !text_out)
        return -1;
    if (to_be_announced_appendf(&csv, &csv_len, &csv_cap, "date,actual,mean,stderr,lower,upper\n") != 0)
        goto fail;
    for (i = 0u; actual && fitted && i < ts_length(actual) && i < ts_length(fitted); ++i) {
        char *date_text = to_be_announced_series_date_text(actual, i);
        char *actual_text = to_be_announced_series_value_text(actual, i);
        char *mean_text = to_be_announced_series_value_text(fitted, i);

        if (to_be_announced_append_combined_csv_row(&csv, &csv_len, &csv_cap,
                                            date_text, actual_text, mean_text,
                                            NULL, NULL, NULL) != 0 ||
            to_be_announced_append_combined_text_row(&text, &text_len, &text_cap,
                                             date_text, actual_text, mean_text,
                                             NULL, NULL, NULL) != 0) {
            free(date_text); free(actual_text); free(mean_text);
            goto fail;
        }
        free(date_text); free(actual_text); free(mean_text);
    }
    for (i = 0u; forecast && forecast->mean && i < ts_length(forecast->mean); ++i) {
        char *date_text = to_be_announced_series_date_text(forecast->mean, i);
        char *mean_text = to_be_announced_series_value_text(forecast->mean, i);
        char *stderr_text = forecast->stderr ? to_be_announced_series_value_text(forecast->stderr, i) : NULL;
        char *lower_text = forecast->lower ? to_be_announced_series_value_text(forecast->lower, i) : NULL;
        char *upper_text = forecast->upper ? to_be_announced_series_value_text(forecast->upper, i) : NULL;

        if (to_be_announced_append_combined_csv_row(&csv, &csv_len, &csv_cap,
                                            date_text, NULL, mean_text,
                                            stderr_text, lower_text, upper_text) != 0 ||
            to_be_announced_append_combined_text_row(&text, &text_len, &text_cap,
                                             date_text, NULL, mean_text,
                                             stderr_text, lower_text, upper_text) != 0) {
            free(date_text); free(mean_text); free(stderr_text); free(lower_text); free(upper_text);
            goto fail;
        }
        free(date_text); free(mean_text); free(stderr_text); free(lower_text); free(upper_text);
    }
    *csv_out = csv;
    *text_out = text;
    return 0;

fail:
    free(csv);
    free(text);
    return -1;
}

static void to_be_announced_free_string_list(char **items, size_t count)
{
    size_t i;

    if (!items)
        return;
    for (i = 0u; i < count; ++i)
        free(items[i]);
    free(items);
}

static char **to_be_announced_split_columns(const char *text, size_t *count_out)
{
    char *copy;
    char *tok;
    char *save = NULL;
    size_t count = 0u, cap = 4u;
    char **items = NULL;

    if (count_out)
        *count_out = 0u;
    if (!text || !text[0])
        return NULL;
    copy = strdup(text);
    items = calloc(cap, sizeof(*items));
    if (!copy || !items) {
        free(copy);
        free(items);
        return NULL;
    }
    tok = strtok_r(copy, ",", &save);
    while (tok) {
        size_t start = 0u;
        size_t end = strlen(tok);
        char *item;

        while (tok[start] == ' ' || tok[start] == '\t')
            start++;
        while (end > start &&
               (tok[end - 1u] == ' ' ||
                tok[end - 1u] == '\t' ||
                tok[end - 1u] == '\r' ||
                tok[end - 1u] == '\n')) {
            end--;
        }
        tok[end] = '\0';
        if (tok[start]) {
            if (count == cap) {
                size_t new_cap = cap * 2u;
                char **new_items = realloc(items, new_cap * sizeof(*new_items));

                if (!new_items) {
                    to_be_announced_free_string_list(items, count);
                    free(copy);
                    return NULL;
                }
                items = new_items;
                cap = new_cap;
            }
            item = strdup(&tok[start]);
            if (!item) {
                to_be_announced_free_string_list(items, count);
                free(copy);
                return NULL;
            }
            items[count++] = item;
        }
        tok = strtok_r(NULL, ",", &save);
    }
    free(copy);
    if (count_out)
        *count_out = count;
    if (count == 0u) {
        free(items);
        return NULL;
    }
    return items;
}

static const char *to_be_announced_model_name(to_be_announced_model_t model)
{
    switch (model) {
    case TO_BE_ANNOUNCED_MODEL_REGRESSION: return "Regression";
    case TO_BE_ANNOUNCED_MODEL_ARIMA: return "ARIMA";
    case TO_BE_ANNOUNCED_MODEL_ARIMAX: return "ARIMAX";
    case TO_BE_ANNOUNCED_MODEL_SARIMA: return "SARIMA";
    case TO_BE_ANNOUNCED_MODEL_SARIMAX: return "SARIMAX";
    case TO_BE_ANNOUNCED_MODEL_AUTO_ARIMA: return "Auto-ARIMA";
    default: return "Forecast";
    }
}

static int to_be_announced_parse_frequency(const char *text, ts_frequency_t *out)
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

static int to_be_announced_parse_year_type(const char *text, ts_year_type_t *out)
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

static int to_be_announced_parse_model(const char *text, to_be_announced_model_t *out)
{
    if (!text || !out)
        return -1;
    if (strcmp(text, "regression") == 0)
        *out = TO_BE_ANNOUNCED_MODEL_REGRESSION;
    else if (strcmp(text, "arima") == 0)
        *out = TO_BE_ANNOUNCED_MODEL_ARIMA;
    else if (strcmp(text, "arimax") == 0)
        *out = TO_BE_ANNOUNCED_MODEL_ARIMAX;
    else if (strcmp(text, "sarima") == 0)
        *out = TO_BE_ANNOUNCED_MODEL_SARIMA;
    else if (strcmp(text, "sarimax") == 0)
        *out = TO_BE_ANNOUNCED_MODEL_SARIMAX;
    else if (strcmp(text, "auto-arima") == 0 || strcmp(text, "auto_arima") == 0)
        *out = TO_BE_ANNOUNCED_MODEL_AUTO_ARIMA;
    else
        return -1;
    return 0;
}

static int to_be_announced_parse_criterion(const char *text, ts_information_criterion_t *out)
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

static void to_be_announced_advance_datetime(datetime_t *dt, ts_frequency_t frequency)
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

        datetime_init_ymdt(dt, year, month, last_day,
                           datetime_hour(dt), datetime_minute(dt), datetime_second(dt));
    }
}

static int to_be_announced_load_x_columns(const to_be_announced_config_t *cfg,
                                  const timeseries_t *y,
                                  timeseries_t **aligned_y_out,
                                  matrix_t **fit_x_out,
                                  matrix_t **future_x_out)
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
    if (!cfg->xreg_path || cfg->xreg_column_count == 0u) {
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
        raw[i] = ts_from_csv(cfg->xreg_path,
                             cfg->xreg_date_column ? cfg->xreg_date_column : "DATE",
                             cfg->xreg_columns[i],
                             cfg->frequency,
                             cfg->year_type,
                             TS_MISSING_DROP);
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
        to_be_announced_advance_datetime(future_start, cfg->frequency);
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

static void to_be_announced_print_success(const to_be_announced_config_t *cfg,
                                  const char *forecast_csv,
                                  const char *forecast_text,
                                  size_t fit_rows,
                                  size_t effective_horizon,
                                  bool stationary,
                                  bool invertible,
                                  const char *summary_text)
{
    fputs("{\"ok\":true,\"model\":", stdout);
    to_be_announced_print_json_string(to_be_announced_model_name(cfg->model));
    printf(",\"fit_rows\":%zu,\"horizon\":%zu,\"stationary\":%s,\"invertible\":%s,",
           fit_rows, effective_horizon,
           stationary ? "true" : "false",
           invertible ? "true" : "false");
    fputs("\"summary_text\":", stdout);
    to_be_announced_print_json_string(summary_text ? summary_text : "");
    fputs(",\"forecast_csv\":", stdout);
    to_be_announced_print_json_string(forecast_csv ? forecast_csv : "");
    fputs(",\"forecast_text\":", stdout);
    to_be_announced_print_json_string(forecast_text ? forecast_text : "");
    fputs("}\n", stdout);
}

static int to_be_announced_run(const to_be_announced_config_t *cfg)
{
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

    if (!cfg || !cfg->target_path || !cfg->target_value_column)
        return to_be_announced_fail("Target dataset and value column are required.");

    y = ts_from_csv(cfg->target_path,
                    cfg->target_date_column ? cfg->target_date_column : "",
                    cfg->target_value_column,
                    cfg->frequency,
                    cfg->year_type,
                    TS_MISSING_DROP);
    if (!y)
        return to_be_announced_fail("Could not load the target time series.");
    if (to_be_announced_load_x_columns(cfg, y, &fit_y, &fit_x, &future_x) != 0) {
        ts_free(y);
        return to_be_announced_fail("Could not align the exogenous data with the target series.");
    }
    if ((cfg->model == TO_BE_ANNOUNCED_MODEL_REGRESSION ||
         cfg->model == TO_BE_ANNOUNCED_MODEL_ARIMAX ||
         cfg->model == TO_BE_ANNOUNCED_MODEL_SARIMAX) &&
        !fit_x) {
        ts_free(y);
        ts_free(fit_y);
        return to_be_announced_fail("This model needs at least one exogenous regressor column.");
    }
    level = num_create_from_double(cfg->level);
    effective_horizon = cfg->horizon;
    if (future_x) {
        size_t future_rows = mat_get_row_count(future_x);

        if (effective_horizon == 0u || future_rows < effective_horizon)
            effective_horizon = future_rows;
    }

    if (cfg->model == TO_BE_ANNOUNCED_MODEL_REGRESSION) {
        ts_regression_result_t fit = {0};

        if (ts_regression_fit(fit_y, fit_x, NULL, &fit) != 0) {
            num_destroy(&level);
            ts_free(y);
            ts_free(fit_y);
            mat_free(fit_x);
            mat_free(future_x);
            return to_be_announced_fail("Regression fitting failed.");
        }
        if (effective_horizon > 0u && future_x &&
            ts_regression_forecast(&fit, future_x, fit_y, cfg->frequency, cfg->year_type,
                                   level, &forecast) != 0) {
            ts_regression_result_clear(&fit);
            num_destroy(&level);
            ts_free(y);
            ts_free(fit_y);
            mat_free(fit_x);
            mat_free(future_x);
            return to_be_announced_fail("Regression forecasting failed.");
        }
        summary_text = ts_regression_summary_to_string(&fit);
        if (to_be_announced_build_results_outputs(fit_y, fit.fitted, &forecast,
                                          &forecast_csv, &forecast_text) != 0) {
            ts_regression_result_clear(&fit);
            ts_forecast_clear(&forecast);
            num_destroy(&level);
            ts_free(y);
            ts_free(fit_y);
            mat_free(fit_x);
            mat_free(future_x);
            free(summary_text);
            return to_be_announced_fail("Regression output formatting failed.");
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
            return to_be_announced_fail("Regression output formatting failed.");
        }
        to_be_announced_print_success(cfg, forecast_csv, forecast_text,
                              ts_length(fit_y), effective_horizon, true, true, summary_text);
        ts_regression_result_clear(&fit);
    } else {
        ts_arima_spec_t spec = cfg->spec;
        ts_arima_result_t fit = {0};
        bool uses_xreg = (cfg->model == TO_BE_ANNOUNCED_MODEL_ARIMAX ||
                          cfg->model == TO_BE_ANNOUNCED_MODEL_SARIMAX ||
                          cfg->model == TO_BE_ANNOUNCED_MODEL_AUTO_ARIMA);
        bool seasonal = (cfg->model == TO_BE_ANNOUNCED_MODEL_SARIMA ||
                         cfg->model == TO_BE_ANNOUNCED_MODEL_SARIMAX);

        if (!seasonal) {
            spec.P = 0u;
            spec.D = 0u;
            spec.Q = 0u;
            spec.season_period = 0u;
        }
        if (cfg->model == TO_BE_ANNOUNCED_MODEL_AUTO_ARIMA) {
            ts_arima_spec_t best_spec = {0};

            if (ts_auto_arima(fit_y, uses_xreg ? fit_x : NULL,
                              spec.p, spec.d, spec.q,
                              spec.P, spec.D, spec.Q,
                              spec.season_period, cfg->criterion,
                              NULL, &best_spec, &fit) != 0) {
                num_destroy(&level);
                ts_free(y);
                ts_free(fit_y);
                mat_free(fit_x);
                mat_free(future_x);
                return to_be_announced_fail("Auto-ARIMA search failed.");
            }
        } else if (ts_arima_fit(fit_y, uses_xreg ? fit_x : NULL, &spec, NULL, &fit) != 0) {
            num_destroy(&level);
            ts_free(y);
            ts_free(fit_y);
            mat_free(fit_x);
            mat_free(future_x);
            return to_be_announced_fail("ARIMA-family fitting failed.");
        }
        if (effective_horizon > 0u &&
            ts_arima_forecast(&fit, fit_y, uses_xreg ? future_x : NULL,
                              effective_horizon, level, &forecast) != 0) {
            ts_arima_result_clear(&fit);
            num_destroy(&level);
            ts_free(y);
            ts_free(fit_y);
            mat_free(fit_x);
            mat_free(future_x);
            return to_be_announced_fail("ARIMA-family forecasting failed.");
        }
        summary_text = ts_arima_summary_to_string(&fit);
        if (to_be_announced_build_results_outputs(fit_y, fit.fitted, &forecast,
                                          &forecast_csv, &forecast_text) != 0) {
            ts_arima_result_clear(&fit);
            ts_forecast_clear(&forecast);
            num_destroy(&level);
            ts_free(y);
            ts_free(fit_y);
            mat_free(fit_x);
            mat_free(future_x);
            free(summary_text);
            return to_be_announced_fail("ARIMA-family output formatting failed.");
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
            return to_be_announced_fail("ARIMA-family output formatting failed.");
        }
        to_be_announced_print_success(cfg, forecast_csv, forecast_text,
                              ts_length(fit_y), effective_horizon,
                              ts_arima_is_stationary(&fit),
                              ts_arima_is_invertible(&fit),
                              summary_text);
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
    return EXIT_SUCCESS;
}

static void to_be_announced_config_init(to_be_announced_config_t *cfg)
{
    if (!cfg)
        return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->target_path = "sample_data/Monthly Target Numbers.csv";
    cfg->target_date_column = "";
    cfg->target_value_column = "18-24 LD No Health contrib Tot.";
    cfg->xreg_path = "sample_data/Monthly Population.csv";
    cfg->xreg_date_column = "DATE";
    cfg->frequency = TS_FREQ_MONTHLY;
    cfg->year_type = TS_YEAR_FISCAL_UK_APR;
    cfg->model = TO_BE_ANNOUNCED_MODEL_SARIMAX;
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

static int to_be_announced_parse_args(to_be_announced_config_t *cfg, int argc, char **argv)
{
    int i;

    if (!cfg)
        return -1;
    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        const char *value;

        if (strcmp(arg, "--help") == 0) {
            puts("Usage: to-be-announced_lab [options]");
            puts("  --target PATH");
            puts("  --target-date-column NAME");
            puts("  --target-value-column NAME");
            puts("  --xreg PATH");
            puts("  --xreg-date-column NAME");
            puts("  --xreg-cols col1,col2");
            puts("  --model regression|arima|arimax|sarima|sarimax|auto-arima");
            puts("  --frequency daily|monthly|quarterly|yearly");
            puts("  --year-type calendar|fiscal");
            puts("  --horizon N");
            puts("  --p N --d N --q N --P N --D N --Q N --season-period N");
            puts("  --criterion aic|aicc|bic");
            puts("  --level 0.95");
            return 1;
        }
        if (i + 1 >= argc)
            return -1;
        value = argv[++i];
        if (strcmp(arg, "--target") == 0)
            cfg->target_path = value;
        else if (strcmp(arg, "--target-date-column") == 0)
            cfg->target_date_column = value;
        else if (strcmp(arg, "--target-value-column") == 0)
            cfg->target_value_column = value;
        else if (strcmp(arg, "--xreg") == 0)
            cfg->xreg_path = value;
        else if (strcmp(arg, "--xreg-date-column") == 0)
            cfg->xreg_date_column = value;
        else if (strcmp(arg, "--xreg-cols") == 0) {
            to_be_announced_free_string_list(cfg->xreg_columns, cfg->xreg_column_count);
            cfg->xreg_columns = to_be_announced_split_columns(value, &cfg->xreg_column_count);
            if (value[0] && !cfg->xreg_columns)
                return -1;
        } else if (strcmp(arg, "--model") == 0) {
            if (to_be_announced_parse_model(value, &cfg->model) != 0)
                return -1;
        } else if (strcmp(arg, "--frequency") == 0) {
            if (to_be_announced_parse_frequency(value, &cfg->frequency) != 0)
                return -1;
        } else if (strcmp(arg, "--year-type") == 0) {
            if (to_be_announced_parse_year_type(value, &cfg->year_type) != 0)
                return -1;
        } else if (strcmp(arg, "--horizon") == 0) {
            if (to_be_announced_parse_size(value, &cfg->horizon) != 0)
                return -1;
        } else if (strcmp(arg, "--p") == 0) {
            if (to_be_announced_parse_size(value, &cfg->spec.p) != 0)
                return -1;
        } else if (strcmp(arg, "--d") == 0) {
            if (to_be_announced_parse_size(value, &cfg->spec.d) != 0)
                return -1;
        } else if (strcmp(arg, "--q") == 0) {
            if (to_be_announced_parse_size(value, &cfg->spec.q) != 0)
                return -1;
        } else if (strcmp(arg, "--P") == 0) {
            if (to_be_announced_parse_size(value, &cfg->spec.P) != 0)
                return -1;
        } else if (strcmp(arg, "--D") == 0) {
            if (to_be_announced_parse_size(value, &cfg->spec.D) != 0)
                return -1;
        } else if (strcmp(arg, "--Q") == 0) {
            if (to_be_announced_parse_size(value, &cfg->spec.Q) != 0)
                return -1;
        } else if (strcmp(arg, "--season-period") == 0) {
            if (to_be_announced_parse_size(value, &cfg->spec.season_period) != 0)
                return -1;
        } else if (strcmp(arg, "--criterion") == 0) {
            if (to_be_announced_parse_criterion(value, &cfg->criterion) != 0)
                return -1;
        } else if (strcmp(arg, "--level") == 0) {
            if (to_be_announced_parse_double(value, &cfg->level) != 0)
                return -1;
        } else
            return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    to_be_announced_config_t cfg;
    int parse_rc;
    int rc;

    to_be_announced_config_init(&cfg);
    parse_rc = to_be_announced_parse_args(&cfg, argc, argv);
    if (parse_rc == 1) {
        to_be_announced_free_string_list(cfg.xreg_columns, cfg.xreg_column_count);
        return EXIT_SUCCESS;
    }
    if (parse_rc != 0) {
        to_be_announced_free_string_list(cfg.xreg_columns, cfg.xreg_column_count);
        return to_be_announced_fail("Invalid arguments. Run with --help for usage.");
    }
    rc = to_be_announced_run(&cfg);
    to_be_announced_free_string_list(cfg.xreg_columns, cfg.xreg_column_count);
    return rc;
}
