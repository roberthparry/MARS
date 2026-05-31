#include "timeseries_internal.h"

char *ts_to_string(const timeseries_t *series, ts_string_style_t style)
{
    ts_string_builder_t sb = {0};
    size_t i;

    if (!series)
        return NULL;
    if (style == TS_STRING_CSV)
        ts_appendf(&sb, "date,value\n");
    for (i = 0u; i < series->length; ++i) {
        char *date_text = NULL;

        if (series->has_index && series->index[i])
            date_text = datetime_format(series->index[i], "%dd/%mm/%yyyy");
        if (style == TS_STRING_CSV) {
            ts_appendf(&sb, "%s,", date_text ? date_text : "");
            if (series->missing[i])
                ts_appendf(&sb, "\n");
            else {
                ts_append_number_text(&sb, series->values[i]);
                ts_appendf(&sb, "\n");
            }
        } else if (style == TS_STRING_PRETTY) {
            ts_appendf(&sb, "%s%s", date_text ? date_text : "", date_text ? " : " : "");
            if (series->missing[i])
                ts_appendf(&sb, "(missing)\n");
            else {
                ts_append_number_text(&sb, series->values[i]);
                ts_appendf(&sb, "\n");
            }
        } else {
            if (i > 0u)
                ts_appendf(&sb, "; ");
            if (date_text)
                ts_appendf(&sb, "%s=", date_text);
            if (series->missing[i])
                ts_appendf(&sb, "NA");
            else
                ts_append_number_text(&sb, series->values[i]);
        }
        free(date_text);
    }
    return ts_builder_detach(&sb);
}

static int ts_vformat(char *out, size_t out_size, const char *fmt, va_list ap)
{
    ts_string_builder_t sb = {0};
    const char *p = fmt;

    while (p && *p) {
        if (*p != '%') {
            if (ts_appendf(&sb, "%c", *p) != 0) {
                ts_builder_free(&sb);
                return -1;
            }
            ++p;
            continue;
        }
        ++p;
        if (*p == '%') {
            ts_appendf(&sb, "%%");
            ++p;
            continue;
        }
        if (*p == 't' || *p == 'T' || *p == 'C') {
            timeseries_t *series = va_arg(ap, timeseries_t *);
            char *text = ts_to_string(series,
                                      *p == 'T' ? TS_STRING_PRETTY :
                                      (*p == 'C' ? TS_STRING_CSV : TS_STRING_INLINE));

            if (!text) {
                ts_builder_free(&sb);
                return -1;
            }
            ts_appendf(&sb, "%s", text);
            free(text);
            ++p;
            continue;
        }
        if (*p == 's') {
            const char *s = va_arg(ap, const char *);

            ts_appendf(&sb, "%s", s ? s : "(null)");
            ++p;
            continue;
        }
        if (*p == 'd') {
            int v = va_arg(ap, int);

            ts_appendf(&sb, "%d", v);
            ++p;
            continue;
        }
        if (*p == 'z' && p[1] == 'u') {
            size_t v = va_arg(ap, size_t);

            ts_appendf(&sb, "%zu", v);
            p += 2;
            continue;
        }
        ts_appendf(&sb, "%%%c", *p);
        ++p;
    }
    if (!out || out_size == 0u) {
        int n = (int)sb.len;

        ts_builder_free(&sb);
        return n;
    }
    snprintf(out, out_size, "%s", sb.buf ? sb.buf : "");
    {
        int n = (int)sb.len;
        ts_builder_free(&sb);
        return n;
    }
}

int ts_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    int rc;
    va_list ap;

    va_start(ap, fmt);
    rc = ts_vformat(out, out_size, fmt, ap);
    va_end(ap);
    return rc;
}

int ts_printf(const char *fmt, ...)
{
    va_list ap;
    char *text = NULL;
    int n;

    va_start(ap, fmt);
    n = ts_vformat(NULL, 0u, fmt, ap);
    va_end(ap);
    if (n < 0)
        return -1;
    text = malloc((size_t)n + 1u);
    if (!text)
        return -1;
    va_start(ap, fmt);
    ts_vformat(text, (size_t)n + 1u, fmt, ap);
    va_end(ap);
    fputs(text, stdout);
    free(text);
    return n;
}

void ts_print(const timeseries_t *series)
{
    char *text = ts_to_string(series, TS_STRING_PRETTY);

    if (!text)
        return;
    fputs(text, stdout);
    free(text);
}

char *ts_forecast_to_string(const ts_forecast_t *forecast, ts_string_style_t style)
{
    ts_string_builder_t sb = {0};
    size_t i;

    if (!forecast || !forecast->mean)
        return NULL;
    if (style == TS_STRING_CSV)
        ts_appendf(&sb, "date,mean,stderr,lower,upper\n");
    for (i = 0u; i < ts_length(forecast->mean); ++i) {
        datetime_t *dt = ts_start_datetime(forecast->mean);
        char *date_text;
        number_t value, stderr_v, lower_v, upper_v;

        if (dt)
            datetime_dealloc(dt);
        date_text = (forecast->mean->has_index && forecast->mean->index[i])
            ? datetime_format(forecast->mean->index[i], "%dd/%mm/%yyyy") : NULL;
        value = mat_get_num(ts_to_column_matrix(forecast->mean), i, 0);
        stderr_v = forecast->stderr ? forecast->stderr->values[i] : num_clone(NUM_NAN);
        lower_v = forecast->lower ? forecast->lower->values[i] : num_clone(NUM_NAN);
        upper_v = forecast->upper ? forecast->upper->values[i] : num_clone(NUM_NAN);
        if (style == TS_STRING_CSV) {
            ts_appendf(&sb, "%s,", date_text ? date_text : "");
            ts_append_number_fixed(&sb, value, 2);
            ts_appendf(&sb, ",");
            ts_append_number_fixed(&sb, stderr_v, 2);
            ts_appendf(&sb, ",");
            ts_append_number_fixed(&sb, lower_v, 2);
            ts_appendf(&sb, ",");
            ts_append_number_fixed(&sb, upper_v, 2);
            ts_appendf(&sb, "\n");
        } else {
            ts_appendf(&sb, "%s: best estimate ", date_text ? date_text : "");
            ts_append_number_fixed(&sb, value, 2);
            ts_appendf(&sb, ", plausible lower end: ");
            ts_append_number_fixed(&sb, lower_v, 2);
            ts_appendf(&sb, ", plausible upper end: ");
            ts_append_number_fixed(&sb, upper_v, 2);
            ts_appendf(&sb, "\n");
        }
        free(date_text);
        num_destroy(&value);
        if (!forecast->stderr) num_destroy(&stderr_v);
        if (!forecast->lower) num_destroy(&lower_v);
        if (!forecast->upper) num_destroy(&upper_v);
    }
    return ts_builder_detach(&sb);
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

static const char *ts_r2_band(number_t value)
{
    double v = num_to_double(value);

    if (v > 0.95)
        return "exceptional";
    if (v >= 0.90)
        return "excellent";
    if (v >= 0.80)
        return "very good";
    if (v >= 0.65)
        return "good";
    if (v >= 0.40)
        return "ok";
    return "poor";
}

static const char *ts_relative_error_band(number_t value, double baseline)
{
    double ratio;

    if (!isfinite(baseline) || baseline <= 0.0)
        return "comparison";
    ratio = fabs(num_to_double(value)) / baseline;
    if (ratio < 0.05)
        return "exceptional";
    if (ratio < 0.10)
        return "excellent";
    if (ratio < 0.15)
        return "very good";
    if (ratio < 0.25)
        return "good";
    if (ratio < 0.40)
        return "ok";
    return "poor";
}

static const char *ts_sigma2_band(number_t sigma2, double baseline)
{
    double ratio;

    if (!isfinite(baseline) || baseline <= 0.0)
        return "comparison";
    ratio = fabs(num_to_double(sigma2)) / baseline;
    if (ratio < 0.0025)
        return "exceptional";
    if (ratio < 0.01)
        return "excellent";
    if (ratio < 0.0225)
        return "very good";
    if (ratio < 0.0625)
        return "good";
    if (ratio < 0.16)
        return "ok";
    return "poor";
}

static int ts_band_score(const char *band)
{
    if (!band)
        return 0;
    if (strcmp(band, "exceptional") == 0)
        return 6;
    if (strcmp(band, "excellent") == 0)
        return 5;
    if (strcmp(band, "very good") == 0)
        return 4;
    if (strcmp(band, "good") == 0)
        return 3;
    if (strcmp(band, "ok") == 0)
        return 2;
    if (strcmp(band, "poor") == 0)
        return 1;
    return 0;
}

static void ts_append_overall_assessment(ts_string_builder_t *sb,
                                         const char *band,
                                         bool comparison_only)
{
    if (!sb || !band)
        return;
    if (comparison_only) {
        ts_appendf(sb,
                   "Overall assessment: You've chosen a %s model so far. Proceed, but compare it with another candidate before deciding. To improve it in this app, run the forecast again with one simpler set of settings and one more seasonal set of settings, then keep the version with the lower comparison score on the same data.\n",
                   band);
        return;
    }
    if (strcmp(band, "exceptional") == 0 || strcmp(band, "excellent") == 0 ||
        strcmp(band, "very good") == 0 || strcmp(band, "good") == 0) {
        ts_appendf(sb,
                   "Overall assessment: You've chosen a %s model. Proceed. If you want to improve it further, try two extra runs in this app: first, make the model simpler by lowering one of p, q, P, or Q; second, if the data is monthly, try a seasonal version with season period 12 and P or D set to 1. Keep the version that gives lower error and still looks realistic.\n",
                   band);
    } else if (strcmp(band, "ok") == 0) {
        ts_appendf(sb,
                   "Overall assessment: You've chosen an ok model. Proceed with caution, and revisit it if the forecast looks unrealistic. To get a better result, first check that the target dates and exogenous dates line up properly. Then run it again with a seasonal model if the data is monthly, or try a different model type such as SARIMAX instead of regression alone. After that, compare the typical forecast error and keep the version that is lower and still believable.\n");
    } else {
        ts_appendf(sb,
                   "Overall assessment: You've chosen a poor model. Revisit the settings, the data, or both before relying on this forecast. To improve it, work through these steps: check for missing or misaligned dates, use a longer clean history if available, try a seasonal model for monthly data with season period 12, reduce over-complicated settings by lowering p, q, P, or Q, and test whether the exogenous inputs are actually helping by running the model with and without them.\n");
    }
}

char *ts_regression_summary_to_string(const ts_regression_result_t *result)
{
    ts_string_builder_t sb = {0};
    size_t i;
    double fitted_scale;
    const char *fit_band;
    const char *error_band;
    const char *overall_band;

    if (!result || !result->coefficients)
        return NULL;
    fitted_scale = ts_mean_abs_series_value(result->fitted);
    fit_band = ts_r2_band(result->adj_r2);
    error_band = ts_relative_error_band(result->rmse, fitted_scale);
    overall_band = ts_band_score(fit_band) < ts_band_score(error_band) ? fit_band : error_band;
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
    return ts_builder_detach(&sb);
}

char *ts_arima_summary_to_string(const ts_arima_result_t *result)
{
    ts_string_builder_t sb = {0};
    ts_arima_meta_t *meta;
    size_t i;
    size_t offset = 0u;
    double fitted_scale;
    const char *sigma_band;

    if (!result)
        return NULL;
    meta = ts_arima_meta_find(result);
    fitted_scale = ts_mean_abs_series_value(result->fitted);
    sigma_band = ts_sigma2_band(result->summary.sigma2, fitted_scale * fitted_scale);
    ts_appendf(&sb, "ARIMA summary\n");
    ts_append_overall_assessment(&sb, sigma_band, strcmp(sigma_band, "comparison") == 0);
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
    return ts_builder_detach(&sb);
}

int ts_write_file(const char *path,
                  const timeseries_t *series,
                  ts_string_style_t style)
{
    char *text = ts_to_string(series, style);
    int rc = text ? ts_write_text_file(path, text) : -1;

    free(text);
    return rc;
}

int ts_forecast_write_file(const char *path,
                           const ts_forecast_t *forecast,
                           ts_string_style_t style)
{
    char *text = ts_forecast_to_string(forecast, style);
    int rc = text ? ts_write_text_file(path, text) : -1;

    free(text);
    return rc;
}

int ts_regression_summary_write_file(const char *path,
                                     const ts_regression_result_t *result,
                                     ts_string_style_t style)
{
    char *text;

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
        text = ts_builder_detach(&sb);
    } else {
        text = ts_regression_summary_to_string(result);
    }
    if (!text)
        return -1;
    {
        int rc = ts_write_text_file(path, text);
        free(text);
        return rc;
    }
}

int ts_arima_summary_write_file(const char *path,
                                const ts_arima_result_t *result,
                                ts_string_style_t style)
{
    char *text;

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
        text = ts_builder_detach(&sb);
    } else {
        text = ts_arima_summary_to_string(result);
    }
    if (!text)
        return -1;
    {
        int rc = ts_write_text_file(path, text);
        free(text);
        return rc;
    }
}
