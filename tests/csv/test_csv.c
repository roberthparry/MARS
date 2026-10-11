/**
 * @file test_csv.c
 * @brief CSV streaming, Unicode, headers, limits and file ownership regressions.
 *
 * Exercises the public opaque API with deliberately fragmented input and
 * round trips. README examples run after all ordinary regression cases.
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "csv-t.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static string_t *csv_test_bytes(const void *bytes, size_t count)
{
    string_t *text = string_new();
    if (text && string_append_bytes(text, bytes, count) != 0) {
        string_free(text);
        text = NULL;
    }
    return text;
}

static csv_t *csv_test_reader(const char *source, const csv_options_t *options)
{
    csv_t *csv = csv_reader_new(NULL, options);
    string_t *text = csv_test_bytes(source, strlen(source));
    if (!text || !csv_feed(csv, text, true)) {
        csv_free(csv);
        csv = NULL;
    }
    string_free(text);
    return csv;
}

static void test_csv_records(void)
{
    csv_t *csv = csv_test_reader("a,\"b,c\",\"say \"\"yes\"\"\",\"x\r\ny\",\r\n\nlast,", NULL);
    TEST_ASSERT_NOT_NULL(csv);
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ROW);
    TEST_ASSERT_INT_EQ(csv_field_count(csv), 5);
    TEST_ASSERT_INT_EQ(csv_record_count(csv), 1);
    TEST_ASSERT_STR_EQ(string_c_str(csv_field(csv, 1)), "b,c");
    TEST_ASSERT_STR_EQ(string_c_str(csv_field(csv, 2)), "say \"yes\"");
    TEST_ASSERT_STR_EQ(string_c_str(csv_field(csv, 3)), "x\r\ny");
    TEST_ASSERT_STR_EQ(string_c_str(csv_field(csv, 4)), "");
    TEST_ASSERT_TRUE(csv_field(csv, 5) == NULL, "absent differs from empty");
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ROW);
    TEST_ASSERT_INT_EQ(csv_field_count(csv), 1);
    TEST_ASSERT_STR_EQ(string_c_str(csv_field(csv, 0)), "");
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ROW);
    TEST_ASSERT_INT_EQ(csv_field_count(csv), 2);
    TEST_ASSERT_STR_EQ(string_c_str(csv_field(csv, 1)), "");
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_END);
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_END);
    TEST_ASSERT_INT_EQ(csv_field_count(csv), 0);
    csv_free(csv);
    csv = csv_test_reader("", NULL);
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_END);
    csv_free(csv);
}

static void test_csv_chunk_boundaries(void)
{
    const char source[] = "\xef\xbb\xbf\"é\",\"a\"\"b\r\nc\"\r\nx,\r\ny,z";
    const char *expected[][2] = {{"é", "a\"b\r\nc"}, {"x", ""}, {"y", "z"}};
    /* Every two-chunk boundary, then a byte-at-a-time stream. */
    for (size_t split = 0; split <= sizeof(source); ++split) {
        csv_t *csv = csv_reader_new(NULL, NULL);
        size_t offset = 0, rows = 0;
        bool done = false;
        while (!done) {
            size_t length = split == sizeof(source) ? 1 : offset ? sizeof(source) - 1 - offset : split;
            if (length > sizeof(source) - 1 - offset)
                length = sizeof(source) - 1 - offset;
            /* A zero first chunk is followed by the entire input. */
            if (!length && !offset && split == 0)
                length = sizeof(source) - 1;
            bool final = offset + length == sizeof(source) - 1;
            string_t *chunk = csv_test_bytes(source + offset, length);
            TEST_ASSERT_TRUE(csv_feed(csv, chunk, final), "feed split bytes");
            string_free(chunk);
            offset += length;
            csv_status_t status;
            while ((status = csv_next(csv)) == CSV_ROW) {
                TEST_ASSERT_TRUE(rows < 3, "bounded row count");
                TEST_ASSERT_INT_EQ(csv_field_count(csv), 2);
                for (size_t column = 0; column < 2; ++column)
                    TEST_ASSERT_STR_EQ(string_c_str(csv_field(csv, column)), expected[rows][column]);
                ++rows;
            }
            TEST_ASSERT_INT_EQ(status, final ? CSV_END : CSV_NEED_INPUT);
            done = final;
        }
        TEST_ASSERT_INT_EQ(rows, 3);
        csv_free(csv);
    }
}

static void test_csv_headers(void)
{
    csv_options_t options = csv_options_default();
    options.first_row_header = true;
    options.trim_headers = true;
    csv_t *csv = csv_test_reader(" name ,π,π,,Case\nA,1,2,,3", &options);
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ROW);
    TEST_ASSERT_INT_EQ(csv_header_count(csv), 5);
    TEST_ASSERT_STR_EQ(string_c_str(csv_header(csv, 0)), " name ");
    TEST_ASSERT_TRUE(csv_header(csv, 5) == NULL, "header bounds");
    string_t *name = string_new_with("name");
    size_t column = 99;
    TEST_ASSERT_TRUE(csv_column(csv, name, &column), "trimmed indexed lookup");
    TEST_ASSERT_INT_EQ(column, 0);
    string_free(name);
    name = string_new_with("π");
    TEST_ASSERT_TRUE(!csv_column(csv, name, &column) && errno == EEXIST, "ambiguous name");
    string_free(name);
    name = string_new_with("");
    TEST_ASSERT_TRUE(!csv_column(csv, name, &column) && errno == ENOENT, "empty name");
    string_free(name);
    name = string_new_with("case");
    TEST_ASSERT_TRUE(!csv_column(csv, name, &column) && errno == ENOENT, "case sensitive");
    string_free(name);
    TEST_ASSERT_INT_EQ(csv_error(csv).code, 0);
    csv_free(csv);

    csv = csv_test_reader("a,b", &options);
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_END);
    TEST_ASSERT_INT_EQ(csv_header_count(csv), 2);
    csv_free(csv);
    csv = csv_reader_new(NULL, &options);
    name = string_new_with("provided");
    const string_t *headers[] = {name};
    TEST_ASSERT_TRUE(csv_set_header(csv, headers, 1), "provided header overrides first-row option");
    string_t *text = string_new_with("data\n");
    TEST_ASSERT_TRUE(csv_feed(csv, text, true), "headerless data");
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ROW);
    TEST_ASSERT_STR_EQ(string_c_str(csv_field(csv, 0)), "data");
    TEST_ASSERT_INT_EQ(csv_record_count(csv), 1);
    TEST_ASSERT_TRUE(csv_column(csv, name, &column), "provided lookup");
    TEST_ASSERT_INT_EQ(column, 0);
    string_free(text);
    string_free(name);
    csv_free(csv);

    /* Canonically equivalent spellings remain distinct byte sequences. */
    options.trim_headers = false;
    csv = csv_test_reader("é,e\xcc\x81\nx,y", &options);
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ROW);
    name = csv_test_bytes("e\xcc\x81", 3);
    TEST_ASSERT_TRUE(csv_column(csv, name, &column), "no implicit normalisation");
    TEST_ASSERT_INT_EQ(column, 1);
    TEST_ASSERT_INT_EQ(string_byte_length(csv_header(csv, 1)), 3);
    string_free(name);
    csv_free(csv);
}

static void test_csv_invalid_input(void)
{
    const char *bad[] = {"a\"b", "\"a\"x", "\"a\" ", "\"unclosed", "\xff", "\xc0\xaf",
                         "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); ++i) {
        csv_t *csv = csv_test_reader(bad[i], NULL);
        TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ERROR);
        csv_error_t error = csv_error(csv);
        TEST_ASSERT_INT_EQ(error.code, i < 4 ? EINVAL : EILSEQ);
        TEST_ASSERT_INT_EQ(error.record, 1);
        TEST_ASSERT_INT_EQ(error.line, 1);
        TEST_ASSERT_INT_EQ(error.field, 1);
        TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ERROR);
        TEST_ASSERT_INT_EQ(csv_error(csv).byte, error.byte);
        csv_free(csv);
    }
    string_t *zero = csv_test_bytes("a\0b", 3);
    csv_t *csv = csv_reader_new(NULL, NULL);
    TEST_ASSERT_TRUE(csv_feed(csv, zero, true), "raw NUL input");
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ERROR);
    TEST_ASSERT_INT_EQ(csv_error(csv).code, EILSEQ);
    csv_free(csv);
    csv = csv_writer_new(NULL, NULL);
    const string_t *fields[] = {zero};
    TEST_ASSERT_TRUE(!csv_write(csv, fields, 1), "writer rejects NUL");
    TEST_ASSERT_INT_EQ(string_byte_length(csv_output(csv)), 0);
    csv_free(csv);
    string_free(zero);
}

static void test_csv_limits_and_width(void)
{
    csv_options_t options = csv_options_default();
    options.max_field_bytes = 3;
    csv_t *csv = csv_test_reader("abc", &options);
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ROW);
    csv_free(csv);
    csv = csv_test_reader("abcd", &options);
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ERROR);
    TEST_ASSERT_INT_EQ(csv_error(csv).code, EOVERFLOW);
    csv_free(csv);
    options.max_record_bytes = 3;
    csv = csv_test_reader("a,b\r\n", &options);
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ROW);
    csv_free(csv);
    csv = csv_test_reader("\"ab\"", &options);
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ERROR);
    csv_free(csv);
    options = csv_options_default();
    options.max_columns = 1;
    csv = csv_test_reader("a,b", &options);
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ERROR);
    csv_free(csv);
    options = csv_options_default();
    options.strict_columns = true;
    csv = csv_test_reader("a,b\nc", &options);
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ROW);
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ERROR);
    TEST_ASSERT_INT_EQ(csv_error(csv).record, 2);
    csv_free(csv);
    options.first_row_header = true;
    csv = csv_test_reader("a,b\nc", &options);
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ERROR);
    csv_free(csv);
    options.max_buffer_bytes = 4;
    csv = csv_reader_new(NULL, &options);
    string_t *text = string_new_with("12345");
    TEST_ASSERT_TRUE(!csv_feed(csv, text, true), "bounded queue");
    TEST_ASSERT_INT_EQ(csv_error(csv).code, EOVERFLOW);
    csv_free(csv);
    csv = csv_writer_new(NULL, &options);
    const string_t *fields[] = {text};
    TEST_ASSERT_TRUE(!csv_write(csv, fields, 1), "bounded output");
    TEST_ASSERT_INT_EQ(csv_error(csv).code, EOVERFLOW);
    csv_free(csv);
    options.max_buffer_bytes = 1024;
    options.first_row_header = false;
    csv = csv_writer_new(NULL, &options);
    TEST_ASSERT_TRUE(csv_write(csv, fields, 1), "first width");
    const string_t *two[] = {text, text};
    TEST_ASSERT_TRUE(!csv_write(csv, two, 2), "writer width enforced");
    csv_free(csv);
    options.delimiter = '"';
    TEST_ASSERT_TRUE(csv_reader_new(NULL, &options) == NULL, "bad delimiter");
    csv_free(NULL);
    TEST_ASSERT_INT_EQ(csv_next(NULL), CSV_ERROR);
    TEST_ASSERT_INT_EQ(csv_error(NULL).code, EINVAL);
    string_free(text);
}

static void test_csv_writer_roundtrip(void)
{
    csv_options_t options = csv_options_default();
    options.delimiter = ';';
    options.quote_all = true;
    options.emit_bom = true;
    options.first_row_header = true;
    csv_t *writer = csv_writer_new(NULL, &options);
    string_t *name = string_new_with("場所"), *value = string_new_with("é;\"x\"\nnext");
    const string_t *headers[] = {name}, *fields[] = {value};
    TEST_ASSERT_TRUE(csv_set_header(writer, headers, 1), "write UTF-8 header");
    TEST_ASSERT_TRUE(csv_write(writer, fields, 1), "write escaped record");
    TEST_ASSERT_TRUE(csv_finish(writer) && csv_finish(writer), "idempotent finish");
    TEST_ASSERT_STR_EQ(string_c_str(csv_output(writer)), "\xef\xbb\xbf\"場所\"\r\n\"é;\"\"x\"\"\nnext\"\r\n");
    csv_t *reader = csv_reader_new(NULL, &options);
    TEST_ASSERT_TRUE(csv_feed(reader, csv_output(writer), true), "round trip input");
    TEST_ASSERT_INT_EQ(csv_next(reader), CSV_ROW);
    TEST_ASSERT_STR_EQ(string_c_str(csv_header(reader, 0)), "場所");
    TEST_ASSERT_STR_EQ(string_c_str(csv_field(reader, 0)), string_c_str(value));
    TEST_ASSERT_INT_EQ(csv_next(reader), CSV_END);
    TEST_ASSERT_TRUE(!csv_write(writer, fields, 1), "finished output is immutable");
    csv_free(reader);
    csv_free(writer);
    string_free(name);
    string_free(value);
}

static void test_csv_writer_limits(void)
{
    string_t *text = string_new_with("a,b");
    const string_t *fields[] = {text, text};
    for (unsigned limit = 0; limit < 3; ++limit) {
        csv_options_t options = csv_options_default();
        if (limit == 0)
            options.max_field_bytes = 2;
        else if (limit == 1)
            options.max_record_bytes = 4; /* Quoted form needs five bytes. */
        else
            options.max_columns = 1;
        csv_t *csv = csv_writer_new(NULL, &options);
        TEST_ASSERT_TRUE(!csv_write(csv, fields, limit == 2 ? 2 : 1), "enforce writer limits");
        TEST_ASSERT_INT_EQ(csv_error(csv).code, EOVERFLOW);
        TEST_ASSERT_INT_EQ(string_byte_length(csv_output(csv)), 0);
        csv_free(csv);
    }
    csv_options_t options = csv_options_default();
    options.emit_bom = true;
    csv_t *csv = csv_writer_new(NULL, &options);
    TEST_ASSERT_TRUE(csv_finish(csv), "empty stream BOM");
    TEST_ASSERT_INT_EQ(string_byte_length(csv_output(csv)), 3);
    csv_free(csv);
    csv = csv_reader_new(NULL, NULL);
    TEST_ASSERT_TRUE(csv_feed(csv, NULL, false), "empty non-final chunk");
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_NEED_INPUT);
    TEST_ASSERT_TRUE(csv_feed(csv, NULL, true), "separate final signal");
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_END);
    TEST_ASSERT_TRUE(!csv_feed(csv, NULL, true), "cannot feed after final");
    csv_free(csv);
    string_free(text);
}

static void test_csv_varied_roundtrips(void)
{
    const char *samples[] = {"", "plain", "a,b", "\"", "\r", "\n", "\r\n", "e\xcc\x81", "🙂", " space "};
    csv_t *writer = csv_writer_new(NULL, NULL);
    for (size_t i = 0; i < sizeof(samples) / sizeof(*samples); ++i) {
        string_t *text = csv_test_bytes(samples[i], strlen(samples[i]));
        const string_t *fields[] = {text, text};
        TEST_ASSERT_TRUE(csv_write(writer, fields, 2), "write varied row");
        string_free(text);
    }
    TEST_ASSERT_TRUE(csv_finish(writer), "finish varied rows");
    csv_t *reader = csv_reader_new(NULL, NULL);
    TEST_ASSERT_TRUE(csv_feed(reader, csv_output(writer), true), "read varied rows");
    for (size_t i = 0; i < sizeof(samples) / sizeof(*samples); ++i) {
        TEST_ASSERT_INT_EQ(csv_next(reader), CSV_ROW);
        TEST_ASSERT_INT_EQ(csv_field_count(reader), 2);
        TEST_ASSERT_STR_EQ(string_c_str(csv_field(reader, 0)), samples[i]);
        TEST_ASSERT_STR_EQ(string_c_str(csv_field(reader, 1)), samples[i]);
    }
    TEST_ASSERT_INT_EQ(csv_next(reader), CSV_END);
    csv_free(reader);
    csv_free(writer);
}

static void test_csv_files(void)
{
    char directory[] = "/tmp/mars-csv-XXXXXX";
    TEST_ASSERT_NOT_NULL(mkdtemp(directory));
    string_t *path = string_sprintf("%s/records.csv", directory);
    file_t *file = file_new(path), *dir = file_new_cstr(directory);
    string_free(path);
    TEST_ASSERT_TRUE(csv_reader_new(file, NULL) == NULL, "closed file rejected");
    TEST_ASSERT_TRUE(file_open(file, FILE_MODE_CREATE, FILE_ACCESS_WRITE), "open writer");
    csv_t *csv = csv_writer_new(file, NULL);
    string_t *large = string_new();
    for (size_t i = 0; i < 5000; ++i)
        TEST_ASSERT_INT_EQ(string_append_cstr(large, "é"), 0);
    const string_t *fields[] = {large};
    TEST_ASSERT_TRUE(csv_write(csv, fields, 1) && csv_finish(csv), "write beyond read-chunk boundary");
    TEST_ASSERT_TRUE(csv_output(csv) == NULL, "file output is not retained");
    csv_free(csv);
    TEST_ASSERT_TRUE(file_is_open(file), "borrowed writer stays open");
    TEST_ASSERT_TRUE(file_close(file), "close writer");
    TEST_ASSERT_TRUE(file_open(file, FILE_MODE_OPEN, FILE_ACCESS_READ), "open reader");
    csv_options_t options = csv_options_default();
    options.max_buffer_bytes = 7;
    csv = csv_reader_new(file, &options);
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ROW);
    TEST_ASSERT_TRUE(string_compare(csv_field(csv, 0), large) == 0, "bounded file streaming");
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_END);
    csv_free(csv);
    TEST_ASSERT_TRUE(file_is_open(file), "borrowed reader stays open");
    csv = csv_writer_new(file, NULL);
    TEST_ASSERT_TRUE(!csv_write(csv, fields, 1), "write to read-only handle fails");
    TEST_ASSERT_TRUE(csv_error(csv).code != 0, "I/O diagnostic");
    csv_free(csv);
    csv = csv_reader_new(file, NULL);
    TEST_ASSERT_TRUE(file_close(file), "simulate premature close");
    TEST_ASSERT_INT_EQ(csv_next(csv), CSV_ERROR);
    csv_free(csv);
    TEST_ASSERT_TRUE(file_delete(file), "delete temporary CSV");
    TEST_ASSERT_TRUE(file_remove_directory(dir), "delete temporary directory");
    string_free(large);
    file_free(file);
    file_free(dir);
}

/* README example: indexed UTF-8 headers and quoted fields. */
static void example_csv_read(void)
{
    csv_options_t options = csv_options_default();
    options.first_row_header = true;
    csv_t *csv = csv_reader_new(NULL, &options);
    string_t *text = string_new_with("name,note\nZoë,\"hello, world\"\n");
    string_t *name = string_new_with("note");
    size_t column = 0;
    bool ok = csv && text && name && csv_feed(csv, text, true) && csv_next(csv) == CSV_ROW &&
              csv_column(csv, name, &column);
    TEST_ASSERT_TRUE(ok, "README input");
    string_printf("%S: %S\n", csv_field(csv, 0), csv_field(csv, column));
    TEST_ASSERT_STR_EQ(string_c_str(csv_field(csv, 0)), "Zoë");
    TEST_ASSERT_STR_EQ(string_c_str(csv_field(csv, column)), "hello, world");
    string_free(name);
    string_free(text);
    csv_free(csv);
}

/* README example: header and escaping are handled by the writer. */
static void example_csv_write(void)
{
    csv_options_t options = csv_options_default();
    options.crlf = false;
    csv_t *csv = csv_writer_new(NULL, &options);
    string_t *name = string_new_with("name"), *note = string_new_with("note");
    string_t *person = string_new_with("Zoë"), *message = string_new_with("hello, world");
    const string_t *headers[] = {name, note}, *fields[] = {person, message};
    bool ok = csv && name && note && person && message && csv_set_header(csv, headers, 2) &&
              csv_write(csv, fields, 2) && csv_finish(csv);
    TEST_ASSERT_TRUE(ok, "README output");
    string_printf("%S", csv_output(csv));
    TEST_ASSERT_STR_EQ(string_c_str(csv_output(csv)), "name,note\nZoë,\"hello, world\"\n");
    string_free(message);
    string_free(person);
    string_free(note);
    string_free(name);
    csv_free(csv);
}

int tests_main(void)
{
    TEST_RUN_CASE(test_csv_records, NULL);
    TEST_RUN_CASE(test_csv_chunk_boundaries, NULL);
    TEST_RUN_CASE(test_csv_headers, NULL);
    TEST_RUN_CASE(test_csv_invalid_input, NULL);
    TEST_RUN_CASE(test_csv_limits_and_width, NULL);
    TEST_RUN_CASE(test_csv_writer_roundtrip, NULL);
    TEST_RUN_CASE(test_csv_writer_limits, NULL);
    TEST_RUN_CASE(test_csv_varied_roundtrips, NULL);
    TEST_RUN_CASE(test_csv_files, NULL);
    TEST_SECTION("README examples");
    TEST_RUN_OUTPUT(example_csv_read);
    TEST_RUN_OUTPUT(example_csv_write);
    return TEST_EXIT_CODE();
}
