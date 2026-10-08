/**
 * @file tba_series_internal.h
 * @brief Private observation representation shared by series analysis files.
 *
 * Only series/ implementations may inspect this structure. Other modules use
 * tba_series.h; date and CSV storage remain owned by the opaque series object.
 */
#ifndef TBA_SERIES_INTERNAL_H
#define TBA_SERIES_INTERNAL_H
#include "tba_series.h"
struct tba_series {
    tba_csv_t *csv;
    int *dates;
    size_t date_column;
    json_t *date_candidates;
    string_t *path;
    int first, last;
    const char *frequency;
};
typedef struct {
    int date;
    size_t row;
    double value;
} tba_series_point_t;
/** @brief Gather sorted finite points. @param series Series. @param column Index. @param count Output count. @return Owned array or NULL. */
tba_series_point_t *tba_series_points(const tba_series_t *series, size_t column, size_t *count);
/** @brief Copy and sort for a median. @param values Values. @param count Count. @return Median, or NAN on failure/empty. */
double tba_series_median(const double *values, size_t count);
/** @brief Compute a finite Pearson correlation. @param a First vector. @param b Second vector. @param count Length. @return Correlation or zero for degeneracy. */
double tba_series_correlation(const double *a, const double *b, size_t count);
/** @brief Populate outliers and seasonality. @param series Series. @param column Target index. @param metadata Destination. @return Success. */
bool tba_series_statistics(const tba_series_t *series, size_t column, json_t *metadata);
/** @brief Store ISO and UK date properties. @param object Destination. @param stem Prefix without _iso. @param date Date. @return Success. */
bool tba_series_date_fields(json_t *object, const char *stem, int date);
/** @brief Resolve driver columns. @param series Series. @param selection Comma-separated headers. @param count Output count. @return Owned indexes or NULL. */
size_t *tba_series_columns(const tba_series_t *series, const char *selection, size_t *count);
#endif
