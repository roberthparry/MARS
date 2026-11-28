/**
 * @file test_format_datetime_format_first.c
 * @brief Check datetime interpolation when formatting is included before the owning type header.
 *
 * Keeps the opposite include order separate from the datetime-first isolation check. Mutable and const values
 * must use the datetime adapter, rather than the ordinary pointer fallback. Releases every local date and string.
 */
#include <string.h>

#include "format-t.h"
#include "datetime-t.h"
#include "test_harness.h"
#define MARS_TEST_FORMAT_INTERNAL_ACCESS
#include "test_format_internal.h"

void test_format_datetime_format_first(void)
{
    datetime_t *date = datetime_init_ymd(datetime_alloc(), 2026, DT_June, 10);
    const datetime_t *borrowed = date;
    string_t *text = fmt_string("{date:date}/{borrowed:yyyy-MM-dd}", FMT(date), FMT(borrowed));
    TEST_ASSERT_TRUE(text && !strcmp(string_c_str(text), "2026-06-10/2026-06-10"),
                     "format-first inclusion selects datetime adapters for mutable and const pointers");
    string_free(text);
    datetime_dealloc(date);
}
