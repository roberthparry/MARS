/**
 * @file csv_internal.h
 * @brief Private CSV stream storage shared by parsing, headers and output.
 *
 * Owns bounded string buffers and indexed header names. Only src/csv consumes
 * this representation; application and timeseries callers use csv.h.
 */
#ifndef MARS_CSV_INTERNAL_H
#define MARS_CSV_INTERNAL_H

#include "dictionary.h"
#include "csv.h"

/** @brief Incremental field syntax, retained between input chunks. */
typedef enum { CSV_FIELD_START, CSV_FIELD_PLAIN, CSV_FIELD_QUOTED, CSV_FIELD_CLOSED } csv_field_state_t;

struct _csv_t {
    csv_options_t options;
    file_t *file;
    bool writer, final, finished, need_input, bom_checked, bom_written, skip_lf, last_cr;
    bool row_ready, started, header_ready;
    csv_field_state_t state;
    size_t position, bytes, line, records, record_bytes, width;
    string_t *buffer, *field;
    string_t **fields, **headers, **keys;
    size_t count, capacity, header_count;
    dictionary_t *index;
    csv_error_t error;
};

/** @brief Preserve the first failure. @param csv Borrowed handle. @param code errno value. @return False. */
bool csv_fail(csv_t *csv, int code);

/** @brief Free current row storage. @param csv Borrowed reader. */
void csv_clear_row(csv_t *csv);

/** @brief Install copied, indexed names. @param csv Handle. @param names Names. @param count Width. @return Success. */
bool csv_install_header(csv_t *csv, const string_t *const *names, size_t count);

/** @brief Validate UTF-8 and reject NUL. @param text Borrowed text. @return True for valid text. */
bool csv_valid_text(const string_t *text);
#endif
