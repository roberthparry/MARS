/**
 * @file test_format_matrix_header.c
 * @brief Verify typed interpolation using only the matrix module's public header.
 *
 * No separate formatting or aggregate header is included. A live native fixture checks that the owning header
 * enables its own constructor association and provides the output API, including const object pointers where used.
 */
#include <stdio.h>
#include <string.h>

#include "matrix-t.h"
#include "test_harness.h"
#define MARS_TEST_FORMAT_INTERNAL_ACCESS
#include "test_format_internal.h"

void test_format_matrix_header(void)
{
    matrix_t *value = mat_from_string("(1,2;3,4)");
    const matrix_t *borrowed = value;
    static const struct {
        const char *name;
        mat_string_style_t style;
    } styles[] = {
        {"expression",        MAT_STRING_EXPRESSION},
        {"function",          MAT_STRING_FUNCTION},
        {"latex",             MAT_STRING_LATEX},
        {"layout",            MAT_STRING_EXPRESSION_LAYOUT},
        {"pretty",            MAT_STRING_INLINE_PRETTY},
        {"pretty_layout",     MAT_STRING_LAYOUT_PRETTY},
        {"scientific",        MAT_STRING_INLINE_SCIENTIFIC},
        {"scientific_layout", MAT_STRING_LAYOUT_SCIENTIFIC},
    };
    bool ok = value != NULL;
    for (size_t i = 0; ok && i < sizeof(styles) / sizeof(*styles); ++i) {
        char pattern[48];
        snprintf(pattern, sizeof(pattern), "{:%s}", styles[i].name);
        string_t *expected = mat_to_text(value, styles[i].style);
        string_t *out = fmt_string(pattern, FMT(borrowed));
        ok = expected && out && !strcmp(string_c_str(out), string_c_str(expected));
        string_free(out);
        string_free(expected);
    }
    string_t *invalid = fmt_string("{:pretty_layout_extra}", FMT(borrowed));
    ok = ok && !invalid && fmt_last_error()->code == FMT_ERROR_TYPE;
    string_free(invalid);
    TEST_ASSERT_TRUE(ok, "including only matrix-t.h enables every native style and rejects unknown names");
    mat_free(value);
}
