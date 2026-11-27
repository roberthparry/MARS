/**
 * @file test_format_TeX_header.c
 * @brief Check notation interpolation with only the public TeX header included.
 *
 * Builds an immutable fraction, borrows it through a const typed binding and checks the native TeX source.
 * Neither the formatter nor descriptor acquires ownership; the fixture releases the notation and builder.
 */
#include <string.h>

#include "TeX-t.h"
#include "test_harness.h"
#define MARS_TEST_FORMAT_INTERNAL_ACCESS
#include "test_format_internal.h"

void test_format_TeX_header(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TeX_symbol(builder, "alpha");
    TeX_integer(builder, 2);
    TeX_frac(builder);
    TeX_t *value = TeX_builder_finish(builder);
    const TeX_t *borrowed = value;
    string_t *out = fmt_string("{:latex}", FMT(borrowed));
    TEST_ASSERT_TRUE(out && !strcmp(string_c_str(out), "\\frac{\\alpha}{2}"),
                     "TeX header enables typed immutable notation without adding delimiters");
    string_free(out);
    TeX_free(value);
    TeX_builder_free(builder);
}
