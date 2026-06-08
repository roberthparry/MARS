#include "timeseries_internal.h"
#include "ustring.h"

static char *ts_cstring_from_text(string_t *text)
{
    char *out;
    size_t len;

    if (!text)
        return NULL;

    len = string_view_length(string_view_all(text));
    out = malloc(len + 1u);
    if (out)
        memcpy(out, string_c_str(text), len + 1u);

    string_free(text);
    return out;
}

static string_t *ts_format_date_text(const datetime_t *date)
{
    string_t *format;
    string_t *text = NULL;

    if (!date)
        return NULL;

    format = string_new_with("%dd/%mm/%yyyy");
    if (!format)
        return NULL;
    text = datetime_format_text(date, format);
    string_free(format);
    return text;
}

string_t *ts_to_text(const timeseries_t *series, ts_string_style_t style)
{
    ts_string_builder_t sb = {0};
    size_t i;

    if (!series)
        return NULL;
    if (style == TS_STRING_CSV)
        ts_appendf(&sb, "date,value\n");
    for (i = 0u; i < series->length; ++i) {
        string_t *date_text = NULL;

        if (series->has_index && series->index[i])
            date_text = ts_format_date_text(series->index[i]);
        if (style == TS_STRING_CSV) {
            if (date_text)
                ts_append_text(&sb, date_text);
            ts_appendf(&sb, ",");
            if (series->missing[i])
                ts_appendf(&sb, "\n");
            else {
                ts_append_number_text(&sb, series->values[i]);
                ts_appendf(&sb, "\n");
            }
        } else if (style == TS_STRING_PRETTY) {
            if (date_text) {
                ts_append_text(&sb, date_text);
                ts_appendf(&sb, " : ");
            }
            if (series->missing[i])
                ts_appendf(&sb, "(missing)\n");
            else {
                ts_append_number_text(&sb, series->values[i]);
                ts_appendf(&sb, "\n");
            }
        } else {
            if (i > 0u)
                ts_appendf(&sb, "; ");
            if (date_text) {
                ts_append_text(&sb, date_text);
                ts_appendf(&sb, "=");
            }
            if (series->missing[i])
                ts_appendf(&sb, "NA");
            else
                ts_append_number_text(&sb, series->values[i]);
        }
        string_free(date_text);
    }
    return ts_builder_detach_text(&sb);
}

char *ts_to_string(const timeseries_t *series, ts_string_style_t style)
{
    return ts_cstring_from_text(ts_to_text(series, style));
}

static int ts_append_padding(string_t *out, int count)
{
    for (int i = 0; i < count; ++i) {
        if (string_append_char(out, ' ') != 0)
            return -1;
    }
    return 0;
}

static string_format_result_t ts_format_callback(string_t *out,
                                                 const string_format_spec_t *spec,
                                                 va_list ap,
                                                 void *user)
{
    timeseries_t *series;
    string_t *text;
    ts_string_style_t style;
    bool left;
    int width;
    size_t text_len;
    int pad;

    (void)user;

    if (!out || !spec)
        return STRING_FORMAT_ERROR;
    if (spec->conversion != 't' &&
        spec->conversion != 'T' &&
        spec->conversion != 'C')
        return STRING_FORMAT_UNHANDLED;
    if (spec->length[0] != '\0')
        return STRING_FORMAT_ERROR;

    width = spec->width_from_argument ? va_arg(ap, int) : spec->width;
    if (spec->precision_from_argument)
        (void)va_arg(ap, int);
    left = spec->flag_left;
    if (width < 0) {
        left = true;
        width = -width;
    }

    style = spec->conversion == 'T'
        ? TS_STRING_PRETTY
        : (spec->conversion == 'C' ? TS_STRING_CSV : TS_STRING_INLINE);
    series = va_arg(ap, timeseries_t *);
    text = ts_to_text(series, style);
    if (!text)
        return STRING_FORMAT_ERROR;

    text_len = string_length(text);
    pad = width > (int)text_len ? width - (int)text_len : 0;
    if (!left && ts_append_padding(out, pad) != 0)
        goto fail;
    if (string_append_string(out, text) != 0)
        goto fail;
    if (left && ts_append_padding(out, pad) != 0)
        goto fail;

    string_free(text);
    return STRING_FORMAT_HANDLED;

fail:
    string_free(text);
    return STRING_FORMAT_ERROR;
}

string_t *ts_vsprintf_text(const char *fmt, va_list ap)
{
    return string_vsprintf_with_callback(fmt, ap, ts_format_callback, NULL);
}

int ts_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    int rc;
    va_list ap;
    string_t *text;
    size_t len;

    va_start(ap, fmt);
    text = ts_vsprintf_text(fmt, ap);
    va_end(ap);
    if (!text)
        return -1;

    len = string_view_length(string_view_all(text));
    if (out && out_size > 0u) {
        size_t copy_len = len < out_size - 1u ? len : out_size - 1u;

        memcpy(out, string_c_str(text), copy_len);
        out[copy_len] = '\0';
    }

    rc = len <= (size_t)INT_MAX ? (int)len : -1;
    string_free(text);
    return rc;
}

string_t *ts_sprintf_text(const char *fmt, ...)
{
    va_list ap;
    string_t *text;

    va_start(ap, fmt);
    text = ts_vsprintf_text(fmt, ap);
    va_end(ap);
    return text;
}

int ts_printf(const char *fmt, ...)
{
    va_list ap;
    string_t *text;
    int written;

    va_start(ap, fmt);
    text = ts_vsprintf_text(fmt, ap);
    va_end(ap);
    if (!text)
        return -1;

    written = string_printf("%S", text);
    string_free(text);
    return written;
}

void ts_print(const timeseries_t *series)
{
    string_t *text = ts_to_text(series, TS_STRING_PRETTY);

    if (!text)
        return;
    string_printf("%S", text);
    string_free(text);
}

string_t *ts_forecast_to_text(const ts_forecast_t *forecast, ts_string_style_t style)
{
    ts_string_builder_t sb = {0};
    size_t i;

    if (!forecast || !forecast->mean)
        return NULL;
    if (style == TS_STRING_CSV)
        ts_appendf(&sb, "date,mean,stderr,lower,upper\n");
    for (i = 0u; i < ts_length(forecast->mean); ++i) {
        datetime_t *dt = ts_start_datetime(forecast->mean);
        string_t *date_text;
        number_t value, stderr_v, lower_v, upper_v;

        if (dt)
            datetime_dealloc(dt);
        date_text = (forecast->mean->has_index && forecast->mean->index[i])
            ? ts_format_date_text(forecast->mean->index[i]) : NULL;
        value = mat_get_num(ts_to_column_matrix(forecast->mean), i, 0);
        stderr_v = forecast->stderr ? forecast->stderr->values[i] : num_clone(NUM_NAN);
        lower_v = forecast->lower ? forecast->lower->values[i] : num_clone(NUM_NAN);
        upper_v = forecast->upper ? forecast->upper->values[i] : num_clone(NUM_NAN);
        if (style == TS_STRING_CSV) {
            if (date_text)
                ts_append_text(&sb, date_text);
            ts_appendf(&sb, ",");
            ts_append_number_fixed(&sb, value, 2);
            ts_appendf(&sb, ",");
            ts_append_number_fixed(&sb, stderr_v, 2);
            ts_appendf(&sb, ",");
            ts_append_number_fixed(&sb, lower_v, 2);
            ts_appendf(&sb, ",");
            ts_append_number_fixed(&sb, upper_v, 2);
            ts_appendf(&sb, "\n");
        } else {
            if (date_text)
                ts_append_text(&sb, date_text);
            ts_appendf(&sb, ": best estimate ");
            ts_append_number_fixed(&sb, value, 2);
            ts_appendf(&sb, ", plausible lower end: ");
            ts_append_number_fixed(&sb, lower_v, 2);
            ts_appendf(&sb, ", plausible upper end: ");
            ts_append_number_fixed(&sb, upper_v, 2);
            ts_appendf(&sb, "\n");
        }
        string_free(date_text);
        num_destroy(&value);
        if (!forecast->stderr) num_destroy(&stderr_v);
        if (!forecast->lower) num_destroy(&lower_v);
        if (!forecast->upper) num_destroy(&upper_v);
    }
    return ts_builder_detach_text(&sb);
}

char *ts_forecast_to_string(const ts_forecast_t *forecast, ts_string_style_t style)
{
    return ts_cstring_from_text(ts_forecast_to_text(forecast, style));
}

static void ts_append_r2_rating(ts_string_builder_t *sb, number_t value)
{
    double v = num_to_double(value);

    if (!sb)
        return;
    if (v > 0.95)
        ts_appendf(sb, " (exceptional)");
    else if (v >= 0.90)
        ts_appendf(sb, " (excellent)");
    else if (v >= 0.80)
        ts_appendf(sb, " (very good)");
    else if (v >= 0.65)
        ts_appendf(sb, " (good)");
    else if (v >= 0.40)
        ts_appendf(sb, " (mediocre; a good result is usually above 0.65 and a very good one above 0.80)");
    else
        ts_appendf(sb, " (poor; a good result is usually above 0.65 and a very good one above 0.80)");
}

static double ts_mean_abs_series_value(const timeseries_t *series)
{
    size_t i, count = 0u;
    double total = 0.0;

    if (!series)
        return NAN;
    for (i = 0u; i < series->length; ++i) {
        if (series->missing[i])
            continue;
        total += fabs(num_to_double(series->values[i]));
        count++;
    }
    return count ? total / (double)count : NAN;
}

static void ts_append_relative_error_rating(ts_string_builder_t *sb,
                                            const char *label,
                                            number_t value,
                                            double baseline)
{
    double ratio;

    if (!sb)
        return;
    if (!isfinite(baseline) || baseline <= 0.0) {
        ts_appendf(sb, " (comparison only; smaller is better)");
        return;
    }
    ratio = fabs(num_to_double(value)) / baseline;
    if (ratio < 0.05)
        ts_appendf(sb, " (exceptional)");
    else if (ratio < 0.10)
        ts_appendf(sb, " (excellent)");
    else if (ratio < 0.15)
        ts_appendf(sb, " (very good)");
    else if (ratio < 0.25)
        ts_appendf(sb, " (good)");
    else if (ratio < 0.40)
        ts_appendf(sb, " (mediocre; good is usually below about 25%% of the usual monthly level)");
    else
        ts_appendf(sb, " (poor; good is usually below about 25%% of the usual monthly level, and very good below about 15%%)");
    (void)label;
}

static void ts_append_information_criterion_rating(ts_string_builder_t *sb)
{
    if (!sb)
        return;
    ts_appendf(sb, " (comparison only; lower is better when comparing models on the same series)");
}

static void ts_append_sigma2_rating(ts_string_builder_t *sb, number_t sigma2, double baseline)
{
    double ratio;

    if (!sb)
        return;
    if (!isfinite(baseline) || baseline <= 0.0) {
        ts_appendf(sb, " (comparison only; lower is better)");
        return;
    }
    ratio = fabs(num_to_double(sigma2)) / baseline;
    if (ratio < 0.0025)
        ts_appendf(sb, " (exceptional)");
    else if (ratio < 0.01)
        ts_appendf(sb, " (excellent)");
    else if (ratio < 0.0225)
        ts_appendf(sb, " (very good)");
    else if (ratio < 0.0625)
        ts_appendf(sb, " (good)");
    else if (ratio < 0.16)
        ts_appendf(sb, " (mediocre; good is usually much smaller relative to the scale of the series)");
    else
        ts_appendf(sb, " (poor; good is usually much smaller relative to the scale of the series, and very good smaller still)");
}

typedef enum {
    TS_BAND_COMPARISON = 0,
    TS_BAND_POOR = 1,
    TS_BAND_OK = 2,
    TS_BAND_GOOD = 3,
    TS_BAND_VERY_GOOD = 4,
    TS_BAND_EXCELLENT = 5,
    TS_BAND_EXCEPTIONAL = 6
} ts_rating_band_t;

static const char *ts_band_label(ts_rating_band_t band)
{
    switch (band) {
        case TS_BAND_EXCEPTIONAL: return "exceptional";
        case TS_BAND_EXCELLENT: return "excellent";
        case TS_BAND_VERY_GOOD: return "very good";
        case TS_BAND_GOOD: return "good";
        case TS_BAND_OK: return "ok";
        case TS_BAND_POOR: return "poor";
        case TS_BAND_COMPARISON:
        default:
            return "comparison";
    }
}

static ts_rating_band_t ts_r2_band(number_t value)
{
    double v = num_to_double(value);

    if (v > 0.95)
        return TS_BAND_EXCEPTIONAL;
    if (v >= 0.90)
        return TS_BAND_EXCELLENT;
    if (v >= 0.80)
        return TS_BAND_VERY_GOOD;
    if (v >= 0.65)
        return TS_BAND_GOOD;
    if (v >= 0.40)
        return TS_BAND_OK;
    return TS_BAND_POOR;
}

static ts_rating_band_t ts_relative_error_band(number_t value, double baseline)
{
    double ratio;

    if (!isfinite(baseline) || baseline <= 0.0)
        return TS_BAND_COMPARISON;
    ratio = fabs(num_to_double(value)) / baseline;
    if (ratio < 0.05)
        return TS_BAND_EXCEPTIONAL;
    if (ratio < 0.10)
        return TS_BAND_EXCELLENT;
    if (ratio < 0.15)
        return TS_BAND_VERY_GOOD;
    if (ratio < 0.25)
        return TS_BAND_GOOD;
    if (ratio < 0.40)
        return TS_BAND_OK;
    return TS_BAND_POOR;
}

static ts_rating_band_t ts_sigma2_band(number_t sigma2, double baseline)
{
    double ratio;

    if (!isfinite(baseline) || baseline <= 0.0)
        return TS_BAND_COMPARISON;
    ratio = fabs(num_to_double(sigma2)) / baseline;
    if (ratio < 0.0025)
        return TS_BAND_EXCEPTIONAL;
    if (ratio < 0.01)
        return TS_BAND_EXCELLENT;
    if (ratio < 0.0225)
        return TS_BAND_VERY_GOOD;
    if (ratio < 0.0625)
        return TS_BAND_GOOD;
    if (ratio < 0.16)
        return TS_BAND_OK;
    return TS_BAND_POOR;
}

static void ts_append_overall_assessment(ts_string_builder_t *sb,
                                         ts_rating_band_t band,
                                         bool comparison_only)
{
    const char *label = ts_band_label(band);

    if (!sb)
        return;
    if (comparison_only) {
        ts_appendf(sb,
                   "Overall assessment: You've chosen a %s model so far. Proceed, but compare it with another candidate before deciding. To improve it in this app, run the forecast again with one simpler set of settings and one more seasonal set of settings, then keep the version with the lower comparison score on the same data.\n",
                   label);
        return;
    }
    if (band >= TS_BAND_GOOD) {
        ts_appendf(sb,
                   "Overall assessment: You've chosen a %s model. Proceed. If you want to improve it further, try two extra runs in this app: first, make the model simpler by lowering one of p, q, P, or Q; second, if the data is monthly, try a seasonal version with season period 12 and P or D set to 1. Keep the version that gives lower error and still looks realistic.\n",
                   label);
    } else if (band == TS_BAND_OK) {
        ts_appendf(sb,
                   "Overall assessment: You've chosen an ok model. Proceed with caution, and revisit it if the forecast looks unrealistic. To get a better result, first check that the target dates and exogenous dates line up properly. Then run it again with a seasonal model if the data is monthly, or try a different model type such as SARIMAX instead of regression alone. After that, compare the typical forecast error and keep the version that is lower and still believable.\n");
    } else {
        ts_appendf(sb,
                   "Overall assessment: You've chosen a poor model. Revisit the settings, the data, or both before relying on this forecast. To improve it, work through these steps: check for missing or misaligned dates, use a longer clean history if available, try a seasonal model for monthly data with season period 12, reduce over-complicated settings by lowering p, q, P, or Q, and test whether the exogenous inputs are actually helping by running the model with and without them.\n");
    }
}

string_t *ts_regression_summary_to_text(const ts_regression_result_t *result)
{
    ts_string_builder_t sb = {0};
    size_t i;
    double fitted_scale;
    ts_rating_band_t fit_band;
    ts_rating_band_t error_band;
    ts_rating_band_t overall_band;

    if (!result || !result->coefficients)
        return NULL;
    fitted_scale = ts_mean_abs_series_value(result->fitted);
    fit_band = ts_r2_band(result->adj_r2);
    error_band = ts_relative_error_band(result->rmse, fitted_scale);
    overall_band = fit_band < error_band ? fit_band : error_band;
    ts_appendf(&sb, "Regression summary\n");
    ts_append_overall_assessment(&sb, overall_band, false);
    ts_appendf(&sb, "Overall fit score (R2): ");
    ts_append_number_fixed(&sb, result->r2, 4);
    ts_append_r2_rating(&sb, result->r2);
    ts_appendf(&sb, "\nAdjusted fit score (Adj R2): ");
    ts_append_number_fixed(&sb, result->adj_r2, 4);
    ts_append_r2_rating(&sb, result->adj_r2);
    ts_appendf(&sb, "\nTypical forecast error (RMSE): ");
    ts_append_number_fixed(&sb, result->rmse, 2);
    ts_append_relative_error_rating(&sb, "RMSE", result->rmse, fitted_scale);
    ts_appendf(&sb, "\nModel comparison score (AIC): ");
    ts_append_number_fixed(&sb, result->summary.aic, 2);
    ts_append_information_criterion_rating(&sb);
    ts_appendf(&sb, "\nModel comparison score (BIC): ");
    ts_append_number_fixed(&sb, result->summary.bic, 2);
    ts_append_information_criterion_rating(&sb);
    ts_appendf(&sb, "\n\nHow to read this:\n");
    ts_appendf(&sb, "- Higher fit scores are better.\n");
    ts_appendf(&sb, "- Lower error is better.\n");
    ts_appendf(&sb, "- Lower AIC/BIC is only better when comparing models on the same data.\n");
    ts_appendf(&sb, "\nCoefficients\n");
    for (i = 0u; i < mat_get_row_count(result->coefficients); ++i) {
        number_t beta = mat_get_num(result->coefficients, i, 0);
        number_t se = result->stderr ? mat_get_num(result->stderr, i, 0) : num_clone(NUM_NAN);
        number_t p = result->p_value ? mat_get_num(result->p_value, i, 0) : num_clone(NUM_NAN);

        ts_appendf(&sb, "beta[%zu] = ", i);
        ts_append_number_sig(&sb, beta, 6);
        ts_appendf(&sb, "  stderr = ");
        ts_append_number_sig(&sb, se, 4);
        ts_appendf(&sb, "  p = ");
        ts_append_number_sig(&sb, p, 4);
        ts_appendf(&sb, "\n");
        num_destroy(&beta);
        num_destroy(&se);
        num_destroy(&p);
    }
    return ts_builder_detach_text(&sb);
}

char *ts_regression_summary_to_string(const ts_regression_result_t *result)
{
    return ts_cstring_from_text(ts_regression_summary_to_text(result));
}

string_t *ts_arima_summary_to_text(const ts_arima_result_t *result)
{
    ts_string_builder_t sb = {0};
    ts_arima_meta_t *meta;
    size_t i;
    size_t offset = 0u;
    double fitted_scale;
    ts_rating_band_t sigma_band;

    if (!result)
        return NULL;
    meta = ts_arima_meta_find(result);
    fitted_scale = ts_mean_abs_series_value(result->fitted);
    sigma_band = ts_sigma2_band(result->summary.sigma2, fitted_scale * fitted_scale);
    ts_appendf(&sb, "ARIMA summary\n");
    ts_append_overall_assessment(&sb, sigma_band, sigma_band == TS_BAND_COMPARISON);
    if (meta) {
        ts_appendf(&sb, "Model: (%zu,%zu,%zu)x(%zu,%zu,%zu)[%zu]\n",
                   meta->spec.p, meta->spec.d, meta->spec.q,
                   meta->spec.P, meta->spec.D, meta->spec.Q,
                   meta->spec.season_period);
    }
    ts_appendf(&sb, "Model comparison score (AIC): ");
    ts_append_number_fixed(&sb, result->summary.aic, 2);
    ts_append_information_criterion_rating(&sb);
    ts_appendf(&sb, "\nModel comparison score (BIC): ");
    ts_append_number_fixed(&sb, result->summary.bic, 2);
    ts_append_information_criterion_rating(&sb);
    ts_appendf(&sb, "\nUnexplained variation left in the model (Sigma2): ");
    ts_append_number_fixed(&sb, result->summary.sigma2, 2);
    ts_append_sigma2_rating(&sb, result->summary.sigma2, fitted_scale * fitted_scale);
    ts_appendf(&sb, "\n\nHow to read this:\n");
    ts_appendf(&sb, "- Lower AIC/BIC is only better when comparing models on the same data.\n");
    ts_appendf(&sb, "- Lower unexplained variation is better.\n");
    ts_appendf(&sb, "- Use the forecast table and interval width as well, not just one score.\n");
    ts_appendf(&sb, "\nAR parameters\n");
    if (result->ar_params) {
        for (i = 0u; i < mat_get_row_count(result->ar_params); ++i) {
            number_t v = mat_get_num(result->ar_params, i, 0);

            ts_appendf(&sb, "phi[%zu] = ", i + 1u);
            ts_append_number_sig(&sb, v, 6);
            ts_appendf(&sb, "\n");
            num_destroy(&v);
        }
    }
    if (result->seasonal_ar_params && meta && meta->spec.season_period > 0u) {
        for (i = 0u; i < mat_get_row_count(result->seasonal_ar_params); ++i) {
            number_t v = mat_get_num(result->seasonal_ar_params, i, 0);

            ts_appendf(&sb, "Phi[%zu] = ", i + 1u);
            ts_append_number_sig(&sb, v, 6);
            ts_appendf(&sb, "  (lag %zu)\n", (i + 1u) * meta->spec.season_period);
            num_destroy(&v);
        }
    }
    ts_appendf(&sb, "\nMA parameters\n");
    if (result->ma_params) {
        for (i = 0u; i < mat_get_row_count(result->ma_params); ++i) {
            number_t v = mat_get_num(result->ma_params, i, 0);

            ts_appendf(&sb, "theta[%zu] = ", i + 1u);
            ts_append_number_sig(&sb, v, 6);
            ts_appendf(&sb, "\n");
            num_destroy(&v);
        }
    }
    if (result->seasonal_ma_params && meta && meta->spec.season_period > 0u) {
        for (i = 0u; i < mat_get_row_count(result->seasonal_ma_params); ++i) {
            number_t v = mat_get_num(result->seasonal_ma_params, i, 0);

            ts_appendf(&sb, "Theta[%zu] = ", i + 1u);
            ts_append_number_sig(&sb, v, 6);
            ts_appendf(&sb, "  (lag %zu)\n", (i + 1u) * meta->spec.season_period);
            num_destroy(&v);
        }
    }
    if (result->xreg_params) {
        size_t display_index = 0u;

        ts_appendf(&sb, "\nCoefficients\n");
        if (meta && meta->has_intercept) {
            number_t v = mat_get_num(result->xreg_params, 0u, 0u);
            number_t se = result->param_stderr ? mat_get_num(result->param_stderr, 0u, 0u) : num_clone(NUM_NAN);
            number_t p = result->param_p_value ? mat_get_num(result->param_p_value, 0u, 0u) : num_clone(NUM_NAN);

            ts_appendf(&sb, "beta[0] = ");
            ts_append_number_sig(&sb, v, 6);
            ts_appendf(&sb, "  stderr = ");
            ts_append_number_sig(&sb, se, 4);
            ts_appendf(&sb, "  p = ");
            ts_append_number_sig(&sb, p, 4);
            ts_appendf(&sb, "\n");
            num_destroy(&v);
            num_destroy(&se);
            num_destroy(&p);
            offset = 1u;
            display_index = 1u;
        }
        if (meta && meta->has_drift) {
            number_t v = mat_get_num(result->xreg_params, offset, 0u);
            number_t se = result->param_stderr ? mat_get_num(result->param_stderr, offset, 0u) : num_clone(NUM_NAN);
            number_t p = result->param_p_value ? mat_get_num(result->param_p_value, offset, 0u) : num_clone(NUM_NAN);

            ts_appendf(&sb, "drift = ");
            ts_append_number_sig(&sb, v, 6);
            ts_appendf(&sb, "  stderr = ");
            ts_append_number_sig(&sb, se, 4);
            ts_appendf(&sb, "  p = ");
            ts_append_number_sig(&sb, p, 4);
            ts_appendf(&sb, "\n");
            num_destroy(&v);
            num_destroy(&se);
            num_destroy(&p);
            offset += 1u;
        }
        if (meta) {
            offset += meta->spec.p;
            offset += ts_arima_effective_seasonal_count(meta->spec.P, meta->spec.season_period);
            offset += meta->spec.q;
            offset += ts_arima_effective_seasonal_count(meta->spec.Q, meta->spec.season_period);
        }
        for (i = offset; i < mat_get_row_count(result->xreg_params); ++i) {
            number_t v = mat_get_num(result->xreg_params, i, 0);
            number_t se = result->param_stderr ? mat_get_num(result->param_stderr, i, 0u) : num_clone(NUM_NAN);
            number_t p = result->param_p_value ? mat_get_num(result->param_p_value, i, 0u) : num_clone(NUM_NAN);

            ts_appendf(&sb, "beta[%zu] = ", display_index);
            ts_append_number_sig(&sb, v, 6);
            ts_appendf(&sb, "  stderr = ");
            ts_append_number_sig(&sb, se, 4);
            ts_appendf(&sb, "  p = ");
            ts_append_number_sig(&sb, p, 4);
            ts_appendf(&sb, "\n");
            num_destroy(&v);
            num_destroy(&se);
            num_destroy(&p);
            display_index += 1u;
        }
    }
    return ts_builder_detach_text(&sb);
}

char *ts_arima_summary_to_string(const ts_arima_result_t *result)
{
    return ts_cstring_from_text(ts_arima_summary_to_text(result));
}

static int ts_write_owned_text_to_path(const char *path, string_t *text)
{
    string_t *path_text;
    int rc = -1;

    if (!path || !text) {
        string_free(text);
        return -1;
    }

    path_text = string_new_with(path);
    if (path_text)
        rc = ts_write_text_file(path_text, text);

    string_free(path_text);
    string_free(text);
    return rc;
}

int ts_write_file(const char *path,
                  const timeseries_t *series,
                  ts_string_style_t style)
{
    return ts_write_owned_text_to_path(path, ts_to_text(series, style));
}

int ts_forecast_write_file(const char *path,
                           const ts_forecast_t *forecast,
                           ts_string_style_t style)
{
    return ts_write_owned_text_to_path(path,
                                       ts_forecast_to_text(forecast, style));
}

int ts_regression_summary_write_file(const char *path,
                                     const ts_regression_result_t *result,
                                     ts_string_style_t style)
{
    string_t *text;

    if (style == TS_STRING_CSV && result && result->coefficients) {
        ts_string_builder_t sb = {0};
        size_t i;

        ts_appendf(&sb, "term,estimate,stderr,t_stat,p_value\n");
        for (i = 0u; i < mat_get_row_count(result->coefficients); ++i) {
            number_t est = mat_get_num(result->coefficients, i, 0);
            number_t se = result->stderr ? mat_get_num(result->stderr, i, 0) : num_clone(NUM_NAN);
            number_t t = result->t_stat ? mat_get_num(result->t_stat, i, 0) : num_clone(NUM_NAN);
            number_t p = result->p_value ? mat_get_num(result->p_value, i, 0) : num_clone(NUM_NAN);

            ts_appendf(&sb, "beta%zu,", i);
            ts_append_number_sig(&sb, est, 6);
            ts_appendf(&sb, ",");
            ts_append_number_sig(&sb, se, 6);
            ts_appendf(&sb, ",");
            ts_append_number_sig(&sb, t, 6);
            ts_appendf(&sb, ",");
            ts_append_number_sig(&sb, p, 6);
            ts_appendf(&sb, "\n");
            num_destroy(&est);
            num_destroy(&se);
            num_destroy(&t);
            num_destroy(&p);
        }
        text = ts_builder_detach_text(&sb);
    } else {
        text = ts_regression_summary_to_text(result);
    }
    return ts_write_owned_text_to_path(path, text);
}

int ts_arima_summary_write_file(const char *path,
                                const ts_arima_result_t *result,
                                ts_string_style_t style)
{
    string_t *text;

    if (style == TS_STRING_CSV && result && result->ar_params) {
        ts_string_builder_t sb = {0};
        size_t i;

        ts_appendf(&sb, "term,estimate\n");
        for (i = 0u; i < mat_get_row_count(result->ar_params); ++i) {
            number_t est = mat_get_num(result->ar_params, i, 0);

            ts_appendf(&sb, "phi%zu,", i + 1u);
            ts_append_number_sig(&sb, est, 6);
            ts_appendf(&sb, "\n");
            num_destroy(&est);
        }
        text = ts_builder_detach_text(&sb);
    } else {
        text = ts_arima_summary_to_text(result);
    }
    return ts_write_owned_text_to_path(path, text);
}
