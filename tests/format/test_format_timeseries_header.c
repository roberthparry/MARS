/**
 * @file test_format_timeseries_header.c
 * @brief Verify native time-series capture through its owning public header.
 *
 * Mutable and const pointers select the module-owned adapter. A live series remains caller-owned throughout
 * formatting; NULL is rejected with an argument diagnostic. Releases every local series and output string.
 */
#include <string.h>

#include "format-t.h"
#include "timeseries-t.h"
#include "test_harness.h"
#define MARS_TEST_FORMAT_INTERNAL_ACCESS
#include "test_format_internal.h"

void test_format_timeseries_header(void)
{
    const double values[] = {1.0, 2.0, 3.0};
    timeseries_t *series = ts_new_from_doubles(values, 3u);
    const timeseries_t *borrowed = series;
    string_t *text = fmt_string("{series}|{borrowed}", FMT(series), FMT(borrowed));
    TEST_ASSERT_TRUE(text && !strcmp(string_c_str(text), "1; 2; 3|1; 2; 3"),
                     "time-series owning header registers mutable and const pointer captures");
    string_free(text);
    borrowed = NULL;
    text = fmt_string("{borrowed}", FMT(borrowed));
    TEST_ASSERT_TRUE(!text && fmt_last_error()->code == FMT_ERROR_ARGUMENT,
                     "NULL series are checked arguments rather than ordinary pointer output");
    string_free(text);
    ts_free(series);
}
