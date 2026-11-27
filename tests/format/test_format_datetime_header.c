/**
 * @file test_format_datetime_header.c
 * @brief Check civil datetime formatting with only its owning public header included.
 *
 * Verifies transitive formatter availability and both mutable and const pointer selections, including fractional
 * seconds in an Excel custom code. The fixture owns its datetime and formatted output until the check finishes.
 */
#include <string.h>

#include "datetime-t.h"
#include "test_harness.h"
#define MARS_TEST_FORMAT_INTERNAL_ACCESS
#include "test_format_internal.h"

void test_format_datetime_header(void)
{
    datetime_t *value = datetime_init_ymdt(datetime_alloc(), 2026, DT_June, 10, 13, 5, 6.125);
    const datetime_t *borrowed = value;
    string_t *out = fmt_string("{0:date} {1:hh:mm:ss.000}", FMT(value), FMT(borrowed));
    TEST_ASSERT_TRUE(out && !strcmp(string_c_str(out), "2026-06-10 13:05:06.125"),
                     "datetime header enables mutable and const typed interpolation with fractional seconds");
    string_free(out);
    datetime_dealloc(value);
}
