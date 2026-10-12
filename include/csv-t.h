/**
 * @file csv-t.h
 * @brief Bounded UTF-8 CSV record streams, headers and quoted output.
 *
 * csv_t is an opaque reader or writer. Readers consume a borrowed open file_t or
 * caller-fed string_t chunks; writers target a borrowed open file_t or bounded
 * in-memory text. Commas, doubled quotes, quoted newlines, UTF-8 BOMs and CR/LF
 * record endings are supported. Fields remain text, without numeric inference
 * or Unicode normalisation. Header lookup is exact and dictionary-indexed.
 *
 * Records are processed incrementally, not retained as a document. Borrowed
 * fields survive until the next csv_next call; header names survive until free.
 * Handles require external synchronisation when shared between threads. File
 * ownership, closing, atomic replacement and durability belong to file-t.h.
 *
 * Long-running calculation paths cooperate with an optional computation_run() scope.
 * Without a scope, existing calls remain supported. Cancellation, deadlines and
 * shared work budgets are cooperative; callers must check operation failure and
 * computation_status() before using outputs. See docs/csv.md for checkpoint
 * coverage, ownership and the corresponding executable example.
 * csv_diagnostic() copies a sticky stream failure into an independently owned
 * diagnostic_t, suitable for cross-thread reporting and database cause chains.
 */
#ifndef MARS_CSV_H
#define MARS_CSV_H

#include <stdbool.h>
#include <stddef.h>

#include "file-t.h"
#include "string-t.h"
#include "diagnostic-t.h"

/** @brief Opaque CSV reader or writer; release with csv_free(). */
typedef struct _csv_t csv_t;

/** @brief Record status: error, end of input, complete data row, or more chunks required. */
typedef enum { CSV_ERROR = -1, CSV_END = 0, CSV_ROW = 1, CSV_NEED_INPUT = 2 } csv_status_t;

/** @brief Copied stream policy; initialise using csv_options_default(), then change individual fields. */
typedef struct {
    char delimiter;            /**< ASCII delimiter other than NUL, quote, CR or LF; comma by default. */
    bool first_row_header;      /**< Reader consumes its first record as names; false by default. */
    bool trim_headers;         /**< Trim names for lookup only; original names remain intact. */
    bool strict_columns;       /**< Require header width, or first data-row width without headers. */
    bool crlf;                 /**< Writer uses CRLF when true, LF otherwise; true by default. */
    bool quote_all;            /**< Quote every written field rather than only fields requiring it. */
    bool emit_bom;             /**< Writer emits a UTF-8 BOM once, including for an empty stream. */
    size_t max_field_bytes;    /**< Maximum decoded field size; default 1 MiB. */
    size_t max_record_bytes;   /**< Maximum encoded record bytes excluding its final CR/LF; default 8 MiB. */
    size_t max_columns;        /**< Maximum fields per record; default 4096. */
    size_t max_buffer_bytes;   /**< Maximum queued input or memory output bytes; default 8 MiB. */
} csv_options_t;

/** @brief Sticky stream error; positions are one-based except the zero-based input byte offset. */
typedef struct {
    int code;                 /**< errno-style code; zero means no stream error. */
    size_t record;            /**< Logical CSV record, counting a header when present. */
    size_t line;              /**< Physical input line; embedded quoted newlines also count. */
    size_t field;             /**< Field being processed. */
    size_t byte;              /**< Input bytes consumed, or zero for writers. */
} csv_error_t;

/** @brief Obtain the documented default policy. @return Options suitable for either constructor. */
csv_options_t csv_options_default(void);

/**
 * @brief Create a reader without reading ahead.
 * @param file Borrowed open readable file, or NULL for caller-fed chunks; never closed by CSV.
 * @param options Borrowed options copied by value, or NULL for defaults; all limits must be positive.
 * @return Caller-owned reader, or NULL with errno set; release with csv_free().
 */
csv_t *csv_reader_new(file_t *file, const csv_options_t *options);

/**
 * @brief Create a writer without publishing bytes.
 * @param file Borrowed open writable file, or NULL for bounded memory output; never closed by CSV.
 * @param options Borrowed options copied by value, or NULL for defaults.
 * @return Caller-owned writer, or NULL with errno set; release with csv_free().
 */
csv_t *csv_writer_new(file_t *file, const csv_options_t *options);

/** @brief Release a handle without flushing or closing a borrowed file.
 * @param csv Owned handle, or NULL; finish a writer explicitly to detect flush failures. */
void csv_free(csv_t *csv);

/**
 * @brief Supply bytes after CSV_NEED_INPUT; chunks may split UTF-8 scalars or quoted fields.
 * @param csv Borrowed chunk-fed reader, not a file reader or writer.
 * @param chunk Borrowed string buffer copied verbatim; NULL means an empty chunk.
 * @param final True marks the last chunk; subsequent feeds are rejected.
 * @return True on success; false sets a sticky error. Drain rows before feeding again.
 */
bool csv_feed(csv_t *csv, const string_t *chunk, bool final);

/**
 * @brief Advance to the next complete data record, consuming an optional first-record header.
 * @param csv Borrowed reader; previous field pointers become invalid, even on EOF or error.
 * @return CSV_ROW, CSV_END, CSV_NEED_INPUT or CSV_ERROR. Empty lines are one empty field.
 * Empty input has no records. File readers never return CSV_NEED_INPUT.
 */
csv_status_t csv_next(csv_t *csv);

/** @brief Count fields in the current complete data row.
 * @param csv Borrowed reader. @return Field count, or zero when no data row is available. */
size_t csv_field_count(const csv_t *csv);

/**
 * @brief Count complete records read or successfully written.
 * @param csv Borrowed reader or writer, or NULL.
 * @return Complete record count, or zero for NULL. Includes a header actually read/written, but not names supplied
 * to a headerless reader. On CSV_ROW this is the current row's one-based input record number.
 */
size_t csv_record_count(const csv_t *csv);

/** @brief Borrow a field without copying, trimming or type conversion.
 * @param csv Borrowed reader with a current row. @param column Zero-based column.
 * @return Borrowed field, including an empty string, or NULL if absent; valid until csv_next(). */
const string_t *csv_field(const csv_t *csv, size_t column);

/**
 * @brief Supply headers before data; a writer emits the header exactly once.
 * @param csv Borrowed reader or writer, before reading/writing any records or feeding chunks.
 * @param names Borrowed ordered names, copied; duplicate and empty names are retained.
 * @param count Positive number of names, within configured limits.
 * @return True on success; false sets a sticky error. On readers this overrides first_row_header.
 */
bool csv_set_header(csv_t *csv, const string_t *const *names, size_t count);

/** @brief Count installed header names, including empty or duplicate names.
 * @param csv Borrowed handle. @return Number of names, or zero before headers are available. */
size_t csv_header_count(const csv_t *csv);

/** @brief Borrow the original header spelling by position.
 * @param csv Borrowed handle. @param column Zero-based column.
 * @return Borrowed name until csv_free(), or NULL for an absent column. */
const string_t *csv_header(const csv_t *csv, size_t column);

/**
 * @brief Resolve a unique non-empty header name using indexed, case-sensitive matching.
 * @param csv Borrowed handle whose header has been read or supplied.
 * @param name Borrowed name; optional trimming follows trim_headers, with no normalisation.
 * @param column Required output; modified only on success.
 * @return True on success; false sets errno to ENOENT, EEXIST for ambiguity, or EINVAL.
 * Lookup failures do not poison the stream. Duplicate/empty names remain positionally accessible.
 */
bool csv_column(const csv_t *csv, const string_t *name, size_t *column);

/**
 * @brief Validate, quote and write one complete record.
 * @param csv Borrowed writer, not finished or failed.
 * @param fields Borrowed ordered non-NULL strings; contents are neither trimmed nor converted.
 * @param count Positive field count within configured limits.
 * @return True on success; false sets a sticky error. Invalid UTF-8 and NUL bytes are rejected.
 * File failures can leave partial output. Spreadsheet formula-like text is preserved, not sanitised.
 */
bool csv_write(csv_t *csv, const string_t *const *fields, size_t count);

/** @brief Finish output and flush a borrowed file, without closing it; repeated success is safe.
 * @param csv Borrowed writer. @return True on success; false sets a sticky error. */
bool csv_finish(csv_t *csv);

/** @brief Borrow accumulated memory-writer output, which grows with successful writes.
 * @param csv Borrowed memory writer. @return Borrowed text until the next write or free, otherwise NULL. */
const string_t *csv_output(const csv_t *csv);

/** @brief Inspect the first stream error; successful calls do not clear a failure.
 * @param csv Borrowed handle. @return Error snapshot; NULL produces EINVAL with zero positions. */
csv_error_t csv_error(const csv_t *csv);

/**
 * @brief Create an owned structured snapshot of the sticky stream error; release with diagnostic_free().
 * The csv domain uses errno-style codes. Reader locations report consumed-byte offset and physical line,
 * not a guessed character column; record and field are included in the message. No input data is copied.
 * NULL with unchanged errno means no stream error; NULL input sets EINVAL and allocation failure sets ENOMEM.
 * The snapshot survives csv_free(); serialise access to the CSV handle as for csv_error().
 */
diagnostic_t *csv_diagnostic(const csv_t *csv);

#endif
