/**
 * @file test_format_datetime_header.c
 * @brief Check datetime-only header isolation followed by explicit formatting opt-in.
 *
 * Verifies that the datetime header imports no formatting API, then includes format-t.h and checks mutable and
 * const pointer selections, fractional seconds and calendar presentations. Owns and releases its local values.
 */
#include <string.h>

#include "datetime-t.h"

#if defined(MARS_FORMAT_H) || defined(FMT) || defined(fmt_print)
#error "Including datetime-t.h alone must not import the formatting API."
#endif

#include "format-t.h"
#include "test_harness.h"
#define MARS_TEST_FORMAT_INTERNAL_ACCESS
#include "test_format_internal.h"

void test_format_datetime_header(void)
{
    datetime_t *value = datetime_init_ymdt(datetime_alloc(), 2026, DT_June, 10, 13, 5, 6.125);
    const datetime_t *borrowed = value;
    string_t *out = fmt_string("{0:date} {1:hh:mm:ss.000}", FMT(value), FMT(borrowed));
    TEST_ASSERT_TRUE(out && !strcmp(string_c_str(out), "2026-06-10 13:05:06.125"),
                     "explicit formatting opt-in after datetime enables mutable and const interpolation");
    string_free(out);
    out = fmt_string("{when:cherokee}", FMT(when, borrowed));
    TEST_ASSERT_TRUE(out && !strcmp(string_c_str(out), "Cherokee civil Green Corn Moon, day 10, year 2026"),
                     "explicit formatting opt-in preserves named native calendar presentations");
    string_free(out);
    datetime_dealloc(value);
}
