/**
 * @file test_format_expression_header.c
 * @brief Verify typed interpolation using only the expression module's public header.
 *
 * No separate formatting or aggregate header is included. A live native fixture checks that the owning header
 * enables its own constructor association and provides the output API, including const object pointers where used.
 */
#include <string.h>

#include "expression-t.h"
#include "test_harness.h"
#define MARS_TEST_FORMAT_INTERNAL_ACCESS
#include "test_format_internal.h"

void test_format_expression_header(void)
{
    expr_t *value = expr_from_string("x + 1", NULL);
    const expr_t *borrowed = value;
    string_t *expected = expr_to_text(value, style_FUNCTION);
    string_t *out = fmt_string("{:function}", FMT(borrowed));
    TEST_ASSERT_TRUE(expected && out && !strcmp(string_c_str(out), string_c_str(expected)),
                     "including only expression-t.h enables native typed formatting");
    string_free(out);
    string_free(expected);
    expr_free(value);
}
