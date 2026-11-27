/**
 * @file test_format_number_header.c
 * @brief Verify typed interpolation using only the number module's public header.
 *
 * No separate formatting or aggregate header is included. A live native fixture checks that the owning header
 * enables its own constructor association and provides the output API, including const object pointers where used.
 */
#include <string.h>

#include "number-t.h"
#include "test_harness.h"
#define MARS_TEST_FORMAT_INTERNAL_ACCESS
#include "test_format_internal.h"

void test_format_number_header(void)
{
    number_t value = num_create_from_string("5/6");
    string_t *out = fmt_string("{:n}", FMT(value));
    TEST_ASSERT_TRUE(out && !strcmp(string_c_str(out), "⅚"),
                     "including only number-t.h enables native typed formatting");
    string_free(out);
    num_destroy(&value);
}
