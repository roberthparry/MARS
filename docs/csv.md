# CSV

`csv-t.h` provides opaque `csv_t` readers and writers for UTF-8 comma-separated
records. It depends only on existing MARS modules: `string_t` for text,
`file_t` for file I/O and `dictionary_t` for indexed header lookup.
Use it for general tabular interchange; the [timeseries module](timeseries.md)
uses it for CSV syntax and supplies its own date and numeric conversions.
For database ingestion, use the [SQLite CSV APIs](sqlite.md#csv-ingestion-and-export).
SQLite owns type inference, column mapping, prepared inserts and transactional
rollback; CSV remains a text-format module. Query export uses the same CSV writer.

## Records and Unicode

Readers accept LF, CRLF and CR record endings, an optional initial UTF-8 BOM,
and a final record without a newline. Quoted fields can contain delimiters,
quotes escaped as `""`, and embedded line endings. Whitespace is data: spaces
outside a closing quote are rejected, not silently discarded. Blank records
contain one empty field; trailing delimiters introduce empty fields. An empty
file contains no records. A missing field returns NULL, unlike an empty field.

Completed fields must contain valid UTF-8 without NUL. Malformed sequences are
rejected, never replaced. CSV preserves the supplied bytes without Unicode
normalisation. Ordinary string constructors may already normalise their input;
use `string_append_utf8_exact` for exact complete text, or
`string_append_bytes` for fragmented or untrusted input which CSV will validate.
Other encodings must be converted by the caller. Delimiters are single ASCII
bytes other than NUL, quote, CR or LF; the default is comma.

Writers escape fields automatically and use CRLF by default. Set `crlf = false`
for LF, `quote_all = true` to quote every field, or `emit_bom = true` to emit
one initial BOM. Fields are always text: no automatic number/date conversion,
trimming, formula rewriting or locale-dependent delimiter detection occurs.
CSV quoting does not prevent spreadsheet formula injection; callers exporting
untrusted text to a spreadsheet must choose their own formula-safety policy.

## Streaming, ownership and limits

`csv_reader_new(file, options)` and `csv_writer_new(file, options)` borrow an
already-open `file_t`. The caller chooses the path, access mode and symlink
policy. The handle must remain open until the CSV object is freed. `csv_free`
does not close or flush it. Finish a writer explicitly and check `file_close`
yourself so that output errors are not lost.

A NULL file creates a chunk-fed reader or memory writer. Call `csv_feed` once,
then drain `csv_next` until `CSV_NEED_INPUT` before supplying another chunk.
Chunks may split any UTF-8 scalar, quote escape or line ending. Set `final`
on the last chunk, or pass NULL with `final = true` to signal EOF. Final input
cannot be extended. No partially parsed record is exposed. Memory writers
retain all output; file writers retain only the current record.

Initialise policies with `csv_options_default()`; NULL options use those same
defaults. Limits are positive and include:

| Policy | Default | Meaning |
| --- | --- | --- |
| `max_field_bytes` | 1 MiB | Decoded UTF-8 field size |
| `max_record_bytes` | 8 MiB | Encoded record bytes, including delimiters, quotes and quoted newlines; excluding its final record ending |
| `max_columns` | 4,096 | Fields per record, including headers |
| `max_buffer_bytes` | 8 MiB | Queued input bytes or complete memory-writer output; minimum four bytes |

Consumed input is discarded on refill. Live storage is bounded by the input
queue, current record and retained headers, including dictionary and string
overheads; the buffer limit is not a total allocation budget. Raise limits
explicitly for large records. Streaming does not limit the total number of rows.
Independent objects can be used separately; concurrent access to the same
object or borrowed file requires caller synchronisation.

## Headers

By default every record is data. Set `first_row_header = true` to consume the
first record as headers; `csv_next` returns the first data row, or `CSV_END`
for header-only input. An empty file has no header. Alternatively call
`csv_set_header` before feeding or reading to label headerless input. A reader's
supplied header overrides `first_row_header`. On a writer this call emits the
header once. Neither operation guesses whether input contains headers.

Names remain available in original order through `csv_header_count` and
`csv_header`. Named lookup uses a dictionary, not repeated column scans.
Matching is case-sensitive and byte-exact. `trim_headers` trims only lookup
keys, not the retained names or data fields. Empty and duplicate names remain
accessible by position; named lookup of an empty name fails with `ENOENT`,
and an ambiguous duplicate fails with `EEXIST`. Missing names fail with
`ENOENT`. These lookup failures do not poison the stream.

`strict_columns` enforces the header width, or the first record's width when
there are no headers. It is off by default. With it disabled, each row retains
its actual field count; fields are never implicitly padded or discarded.

## Public API

All names below are declared in `csv-t.h`; detailed parameter and ownership
contracts are also available in its Doxygen comments.

| Function | Contract |
| --- | --- |
| `csv_options_default()` | Return the default policy value for modification before construction. |
| `csv_reader_new(file, options)` | Allocate a reader; copy options and borrow the open file, or accept chunks if NULL. Return NULL on failure. |
| `csv_writer_new(file, options)` | Allocate a writer; copy options and borrow the open file, or retain output if NULL. Return NULL on failure. |
| `csv_free(csv)` | Free the object, current row and copied headers; NULL is safe. Neither finish nor close the file. |
| `csv_feed(csv, chunk, final)` | Copy raw input into a chunk-fed reader. NULL is an empty chunk. Only valid initially or after `CSV_NEED_INPUT`; return success. |
| `csv_next(csv)` | Return `CSV_ROW`, `CSV_NEED_INPUT`, `CSV_END` or `CSV_ERROR`. File readers refill internally. Invalidate the previous row. |
| `csv_field_count(csv)` | Return the current complete row's width, otherwise zero. |
| `csv_record_count(csv)` | Count complete records read/written, including a physical header but excluding names supplied to a headerless reader. On `CSV_ROW`, this identifies the current input record. NULL returns zero. |
| `csv_field(csv, column)` | Borrow a zero-based field until the next `csv_next`; return NULL if absent. |
| `csv_set_header(csv, names, count)` | Copy a non-empty array of non-NULL names before any input/output; on writers emit the header. Set only once. Return success. |
| `csv_header_count(csv)` | Return retained header width, or zero before headers are available. |
| `csv_header(csv, column)` | Borrow an original zero-based name until `csv_free`; return NULL if absent. |
| `csv_column(csv, name, column)` | Look up a header and write its zero-based index only on success. Inputs and output pointer are required. |
| `csv_write(csv, fields, count)` | Validate and write a non-empty array of non-NULL strings as one record. Copy/consume fields during the call; return success. |
| `csv_finish(csv)` | Emit any pending BOM and flush a writer; return success. Repeating it is safe; subsequent writes fail. |
| `csv_output(csv)` | Borrow all output from a memory writer until mutation/free; return NULL for other objects. |
| `csv_error(csv)` | Return the sticky first error and position, or a zero code when healthy; NULL returns `EINVAL`. |

Failures use `errno`-style codes: `EINVAL` for syntax, width or API misuse,
`EILSEQ` for invalid UTF-8/NUL, `EOVERFLOW` for configured limits and `ENOMEM`
for allocation failures. File failures retain the underlying error code.
`csv_error_t` contains `code`, logical `record`, physical `line`, `field` and
consumed input `byte` count. Record/line/field are one-based; headers count as
records, and quoted newlines count as physical lines. Positions describe where
failure was detected, not necessarily the first byte of a malformed scalar.
Writers report the record/field; input byte and line positions are not meaningful.

Invalid records are checked before publication, but write/flush failures can
leave partial file output. For atomic replacement use the file module's staging
facilities. Do not treat a failed CSV object as reusable; inspect and free it.

## Example: reading named columns

```c
#include "csv-t.h"

int main(void)
{
    csv_options_t options = csv_options_default();
    options.first_row_header = true;
    csv_t *csv = csv_reader_new(NULL, &options);
    string_t *text = string_new_with("name,note\nZoë,\"hello, world\"\n");
    string_t *name = string_new_with("note");
    size_t column = 0;
    bool ok = csv && text && name && csv_feed(csv, text, true) && csv_next(csv) == CSV_ROW &&
              csv_column(csv, name, &column);
    if (ok)
        string_printf("%S: %S\n", csv_field(csv, 0), csv_field(csv, column));
    string_free(name);
    string_free(text);
    csv_free(csv);
    return ok ? 0 : 1;
}
```

Output:

```text
Zoë: hello, world
```

## Example: writing headers and escaped text

```c
#include "csv-t.h"

int main(void)
{
    csv_options_t options = csv_options_default();
    options.crlf = false;
    csv_t *csv = csv_writer_new(NULL, &options);
    string_t *name = string_new_with("name"), *note = string_new_with("note");
    string_t *person = string_new_with("Zoë"), *message = string_new_with("hello, world");
    const string_t *headers[] = {name, note}, *fields[] = {person, message};
    bool ok = csv && name && note && person && message && csv_set_header(csv, headers, 2) &&
              csv_write(csv, fields, 2) && csv_finish(csv);
    if (ok)
        string_printf("%S", csv_output(csv));
    string_free(message);
    string_free(person);
    string_free(note);
    string_free(name);
    csv_free(csv);
    return ok ? 0 : 1;
}
```

Output:

```text
name,note
Zoë,"hello, world"
```

Both examples run last in `tests/csv/test_csv.c`. Run the suite with
`make -j1 test_csv`; the module needs no additional external library.

## Optional computation control

`csv_next`, `csv_write` and `csv_finish` inherit `computation_run`.
Record boundaries and blocks within long fields provide cooperative stopping.
A stopped reader returns `CSV_ERROR`; a stopped writer returns false. The
stream retains a sticky diagnostic, so create a new reader/writer to restart.
Already consumed input and previously published output cannot be recalled.
No partial parsed record is returned; file I/O itself is not made asynchronous.

Without a scope, the existing API and algorithm-specific limits are unchanged.
Stopping is cooperative, not a hard real-time guarantee. Work units mix nested
algorithm steps and are not a percentage or a count of input records; exact
counts may change with implementation. Progress observers run synchronously
and must not re-enter calculations. Check both the operation result and
`computation_status`; a false operation result with an active control is an
ordinary algorithm/input failure. See [computation_t](computation.md).

This deliberately small budget demonstrates stopping. Increase it, use a
timeout, or cancel the control from another thread for normal application use.

```c
#include <stdio.h>

#include "computation-t.h"
#include "csv-t.h"

typedef struct {
    csv_t *input;
} job_t;

static bool calculate(void *argument)
{
    job_t *job = argument;
    csv_status_t status;
    while ((status = csv_next(job->input)) == CSV_ROW) {
        /* Consume only complete records here. */
    }
    return status == CSV_END;
}

int main(void)
{
    job_t job = {.input = csv_reader_new(NULL, NULL)};
    string_t *text = string_new_with("one,two\nthree,four\n");
    if (!job.input || !text || !csv_feed(job.input, text, true)) {
        csv_free(job.input);
        string_free(text);
        return 1;
    }
    string_free(text);
    computation_options_t options = {.work_limit = 1};
    computation_t *control = computation_new(&options);
    bool ok = control && computation_run(control, calculate, &job);
    bool stopped = !ok && computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED;
    printf("Stopped: %d\n", stopped);
    csv_free(job.input);
    computation_free(control);
    return stopped ? 0 : 1;
}
```

Output:

```text
Stopped: 1
```
