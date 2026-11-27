/**
 * @file test_format_qfloat_header.c
 * @brief Verify typed interpolation using only the qfloat module's public header.
 *
 * No separate formatting or aggregate header is included. A live native fixture checks that the owning header
 * enables its own constructor association and provides the output API, including const object pointers where used.
 */
#include <string.h>

#include "qfloat-t.h"
#include "test_harness.h"
#define MARS_TEST_FORMAT_INTERNAL_ACCESS
#include "test_format_internal.h"

#if defined(MARS_QCOMPLEX_H) || defined(MARS_NUMBER_H) || defined(MARS_MATRIX_H) || defined(MARS_EXPRESSION_H)
#error "The real type header must not include other mathematical modules for formatting."
#endif

static qfloat_t test_header_value(unsigned *calls)
{
    ++*calls;
    return qf_from_double(1.25);
}

void test_format_qfloat_header(void)
{
    qfloat_t value = qf_from_double(1.25);
    string_t *out = fmt_string("{:.3f}", FMT(value));
    TEST_ASSERT_TRUE(out && !strcmp(string_c_str(out), "1.250"),
                     "including only qfloat-t.h enables native typed formatting");
    string_free(out);
    unsigned calls = 0;
    out = fmt_string("{:.3f}", FMT(test_header_value(&calls)));
    TEST_ASSERT_TRUE(out && calls == 1u && !strcmp(string_c_str(out), "1.250"),
                     "composed type selection evaluates a native value exactly once");
    string_free(out);
}
