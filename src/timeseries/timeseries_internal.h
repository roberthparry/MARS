#ifndef MARS_TIMESERIES_INTERNAL_H
#define MARS_TIMESERIES_INTERNAL_H

#if !defined(MARS_TIMESERIES_INTERNAL_ACCESS) &&                                                                       \
    (!defined(__INTELLISENSE__) || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "timeseries_internal.h is private to the timeseries module; include timeseries.h instead."
#endif

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "timeseries.h"

struct timeseries_t {
    size_t length;
    number_t *values;
    datetime_t **index;
    bool has_index;
    bool *missing;
    ts_frequency_t frequency;
    ts_year_type_t year_type;
    bool is_regular;
    size_t season_period;
};

typedef struct ts_arima_meta_t {
    const ts_arima_result_t *owner;
    ts_arima_spec_t spec;
    bool has_intercept;
    bool has_drift;
    size_t trim;
    size_t fit_rows;
    number_t intercept;
    matrix_t *xreg_history;
    struct ts_arima_meta_t *next;
} ts_arima_meta_t;

typedef struct {
    string_t *text;
} ts_string_builder_t;

int ts_appendf(ts_string_builder_t *sb, const char *fmt, ...);
int ts_append_text(ts_string_builder_t *sb, const string_t *text);
size_t ts_builder_encoded_length(const ts_string_builder_t *sb);
void ts_builder_free(ts_string_builder_t *sb);
string_t *ts_builder_detach_text(ts_string_builder_t *sb);

datetime_t *ts_datetime_clone(const datetime_t *dt);
int ts_step_regular_datetime(datetime_t *dt, ts_frequency_t frequency);
datetime_t *ts_advance_regular_datetime(datetime_t *dt, ts_frequency_t frequency);
int ts_infer_season_period(ts_frequency_t frequency);
int ts_parse_date_text(const string_t *text, datetime_t **out);
bool ts_datetime_same_bucket(const datetime_t *a, const datetime_t *b, ts_frequency_t frequency,
                             ts_year_type_t year_type);
datetime_t *ts_bucket_label(const datetime_t *dt, ts_frequency_t frequency, ts_year_type_t year_type);

int ts_append_number_text(ts_string_builder_t *sb, number_t value);
int ts_append_number_fixed(ts_string_builder_t *sb, number_t value, int decimals);
int ts_append_number_sig(ts_string_builder_t *sb, number_t value, int sig_figs);

timeseries_t *ts_alloc_empty(size_t length);
void ts_copy_value_slot(number_t *dst, const number_t *src);
int ts_set_index(timeseries_t *series, const datetime_t *const *index);
timeseries_t *ts_clone_shallow_shape(const timeseries_t *series);
int ts_copy_into(timeseries_t *dst, const timeseries_t *src);
timeseries_t *ts_make_empty_regular_series(size_t length, const datetime_t *start, ts_frequency_t frequency,
                                           ts_year_type_t year_type);

int ts_series_to_double_array(const timeseries_t *series, double **out_values, size_t *out_count, size_t start);
matrix_t *ts_make_column_matrix_from_doubles(const double *values, size_t n);
matrix_t *ts_make_matrix_from_doubles(const double *values, size_t rows, size_t cols);
matrix_t *ts_matrix_clone_local(const matrix_t *src);
matrix_t *ts_submatrix_rows(const matrix_t *A, size_t start_row, size_t row_count);

int ts_write_text_file(const string_t *path, const string_t *text);

ts_arima_meta_t *ts_arima_meta_find(const ts_arima_result_t *owner);
void ts_arima_meta_remove(const ts_arima_result_t *owner);
int ts_arima_meta_store(const ts_arima_result_t *owner, const ts_arima_spec_t *spec, bool has_intercept, bool has_drift,
                        size_t trim, size_t fit_rows, const number_t *intercept, const matrix_t *xreg_history);
int ts_arima_meta_transfer(const ts_arima_result_t *dst, const ts_arima_result_t *src);
size_t ts_arima_effective_seasonal_count(size_t count, size_t season_period);

int ts_regression_fit_internal(const timeseries_t *y, const matrix_t *xreg, bool include_intercept, bool include_trend,
                               const ts_fit_options_t *options, ts_regression_result_t *out);

void ts_result_clear_numbers(ts_fit_summary_t *summary);

#endif /* MARS_TIMESERIES_INTERNAL_H */
