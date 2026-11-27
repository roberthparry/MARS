/**
 * @file test_format_matrix_header.c
 * @brief Verify typed interpolation using only the matrix module's public header.
 *
 * No separate formatting or aggregate header is included. A live native fixture checks that the owning header
 * enables its own constructor association and provides the output API, including const object pointers where used.
 */
#include <string.h>

#include "matrix-t.h"
#include "test_harness.h"
#define MARS_TEST_FORMAT_INTERNAL_ACCESS
#include "test_format_internal.h"

void test_format_matrix_header(void)
{
    matrix_t *value = mat_from_string("(1,2;3,4)");
    const matrix_t *borrowed = value;
    string_t *expected = mat_to_text(value, MAT_STRING_LATEX);
    string_t *out = fmt_string("{:latex}", FMT(borrowed));
    TEST_ASSERT_TRUE(expected && out && !strcmp(string_c_str(out), string_c_str(expected)),
                     "including only matrix-t.h enables native typed formatting");
    string_free(out);
    string_free(expected);
    mat_free(value);
}
