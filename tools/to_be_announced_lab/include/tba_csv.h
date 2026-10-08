/**
 * @file tba_csv.h
 * @brief Opaque, bounded CSV documents for forecasting inputs and output tables.
 *
 * Implements quoted fields, doubled quotes, UTF-8 BOMs and CR/LF record endings
 * using string cursors. Header lookups are indexed. Documents retain row order
 * and missing cells; callers decide which observations are usable.
 */
#ifndef TBA_CSV_H
#define TBA_CSV_H
#include "tba_support.h"
typedef struct tba_csv tba_csv_t;
/** @brief Parse CSV text. @param text Borrowed UTF-8 input. @return Owned document or NULL; release with tba_csv_free. */
tba_csv_t *tba_csv_parse(const string_t *text);
/** @brief Read up to 16 MiB of CSV. @param path Borrowed path. @return Owned document or NULL. */
tba_csv_t *tba_csv_open(const char *path);
/** @brief Release a document. @param csv Owned document; NULL is safe. */
void tba_csv_free(tba_csv_t *csv);
/** @brief Count data rows. @param csv Borrowed document. @return Count excluding the header. */
size_t tba_csv_rows(const tba_csv_t *csv);
/** @brief Count columns. @param csv Borrowed document. @return Header width. */
size_t tba_csv_columns(const tba_csv_t *csv);
/** @brief Borrow a header. @param csv Document. @param column Zero-based index. @return Text or empty literal. */
const char *tba_csv_header(const tba_csv_t *csv, size_t column);
/** @brief Borrow a cell. @param csv Document. @param row Data-row index. @param column Column index. @return Text or NULL. */
const string_t *tba_csv_cell(const tba_csv_t *csv, size_t row, size_t column);
/** @brief Find a column. @param csv Document. @param name Header text. @return Index or SIZE_MAX. */
size_t tba_csv_column(const tba_csv_t *csv, const char *name);
/** @brief Copy a cell. @param csv Document. @param row Row index. @param column Column index. @param text Text. @return Success. */
bool tba_csv_set(tba_csv_t *csv, size_t row, size_t column, const string_t *text);
/** @brief Serialise with correct quoting. @param csv Document. @return Owned CSV text, or NULL. */
string_t *tba_csv_text(const tba_csv_t *csv);
#endif
