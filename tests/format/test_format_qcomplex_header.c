/**
 * @file test_format_qcomplex_header.c
 * @brief Verify typed interpolation using only the qcomplex module's public header.
 *
 * No separate formatting or aggregate header is included. A live native fixture checks that the owning header
 * enables its own constructor association and provides the output API, including const object pointers where used.
 */
#include <string.h>

#include "qcomplex-t.h"
#include "test_harness.h"
#define MARS_TEST_FORMAT_INTERNAL_ACCESS
#include "test_format_internal.h"

void test_format_qcomplex_header(void)
{
    qcomplex_t value = {{1.25, 0.0}, {-2.5, 0.0}};
    string_t *out = fmt_string("{:.2f}", FMT(value));
    TEST_ASSERT_TRUE(out && !strcmp(string_c_str(out), "1.25 - 2.50i"),
                     "including only qcomplex-t.h enables native typed formatting");
    string_free(out);
}
