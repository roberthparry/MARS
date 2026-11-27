/**
 * @file test_format_equation_header.c
 * @brief Check equation interpolation using only the owning public equation header.
 *
 * Compares typed output against the native equation renderer and checks that const pointer capture is available
 * without an aggregate integration header. All fixture resources are released before returning to the suite.
 */
#include <string.h>

#include "equation-t.h"
#include "test_harness.h"
#define MARS_TEST_FORMAT_INTERNAL_ACCESS
#include "test_format_internal.h"

void test_format_equation_header(void)
{
    equation_t *value = equ_from_string("x^2 = 4");
    const equation_t *borrowed = value;
    string_t *expected = equ_to_text(value, style_LATEX);
    string_t *out = fmt_string("{:latex}", FMT(borrowed));
    TEST_ASSERT_TRUE(expected && out && !strcmp(string_c_str(out), string_c_str(expected)),
                     "equation header enables native typed TeX formatting");
    string_free(out);
    string_free(expected);
    equ_free(value);
}
