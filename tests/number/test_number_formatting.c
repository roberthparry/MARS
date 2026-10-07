/**
 * @file test_number_formatting.c
 * @brief Generic numeric output regressions.
 *
 * Checks exact, real and complex textual formatting and supported precision options. It protects numeric spelling
 * independently of the expression and matrix renderers.
 *
 * Used by the project test harness for regression verification. Select cases through tests/test_config.json and
 * run suites sequentially; this source is not part of the installed library.
 */

#include <complex.h>
#include <stdio.h>
#include <string.h>

#include "test_number.h"

void run_number_formatting_tests(void)
{
    printf(C_CYAN "Testing formatting and extended public operations...\n" C_RESET);

    {
        char buf[128];
        number_t zeros[] = {
            num_create_from_cdouble(CMPLX(0.0, 0.0)),
            num_create_from_cdouble(CMPLX(-0.0, -0.0)),
            num_create_from_qcomplex(QC_ZERO),
            num_create_from_qcomplex(qc_make(qf_neg(QF_ZERO), qf_neg(QF_ZERO))),
            num_create_from_string("0i"),
            num_create_from_string("-0i"),
            num_mul(NUM_ZERO, NUM_I)
        };
        const char *formats[] = { "%n", "%.40n", "%N", "%.40N" };
        for (size_t i = 0u; i < sizeof(zeros) / sizeof(*zeros); ++i) {
            ASSERT_TRUE(num_is_zero(zeros[i]));
            for (size_t j = 0u; j < sizeof(formats) / sizeof(*formats); ++j) {
                ASSERT_EQ_INT(num_sprintf(buf, sizeof(buf), formats[j], zeros[i]), 1);
                ASSERT_TRUE(strcmp(buf, "0") == 0);
            }
            ASSERT_EQ_INT(num_sprintf(buf, sizeof(buf), "%+5n", zeros[i]), 5);
            ASSERT_TRUE(strcmp(buf, "   +0") == 0);
            num_destroy(&zeros[i]);
        }
        number_t tiny = num_create_from_string("1e-80i");
        ASSERT_TRUE(!num_is_zero(tiny));
        ASSERT_TRUE(num_sprintf(buf, sizeof(buf), "%.3n", tiny) > 0);
        ASSERT_TRUE(strchr(buf, 'i') != NULL);
        num_destroy(&tiny);
    }

    {
        char buf[512];
        number_t dec = num_create_from_string("32.123");
        number_t rat = num_create_from_string("5/6");
        number_t rat_general = num_create_from_string("355/113");
        number_t one = num_create_from_string("1");
        number_t two = num_create_from_string("2");
        number_t five = num_create_from_string("5");
        number_t beta = num_beta(two, two);
        number_t angle = num_atan2(one, one);
        number_t gamma5 = num_gamma(five);
        number_t ei1 = num_Ei(one);
        number_t cdouble_unit = num_create_from_cdouble(1.0 + 1.0 * I);
        number_t cdouble_neg_unit = num_create_from_cdouble(1.0 - 1.0 * I);
        number_t cdouble_pure_unit = num_create_from_cdouble(0.0 + 1.0 * I);
        int written;

        written = num_sprintf(buf, sizeof(buf), "%n", dec);
        ASSERT_TRUE(written > 0);
        printf(C_WHITE C_BOLD "num_sprintf(\"%%n\", 32.123)" C_RESET "\n");
        printf("    got      = %s\n\n", buf);
        ASSERT_TRUE(strcmp(buf, "32.123") == 0);

        written = num_sprintf(buf, sizeof(buf), "%N", dec);
        ASSERT_TRUE(written > 0);
        printf(C_WHITE C_BOLD "num_sprintf(\"%%N\", 32.123)" C_RESET "\n");
        printf("    got      = %s\n\n", buf);
        ASSERT_TRUE(strchr(buf, 'e') != NULL || strchr(buf, 'E') != NULL);

        written = num_sprintf(buf, sizeof(buf), "%N", rat);
        ASSERT_TRUE(written > 0);
        printf(C_WHITE C_BOLD "num_sprintf(\"%%N\", 5/6)" C_RESET "\n");
        printf("    got      = %s\n\n", buf);
        ASSERT_TRUE(strcmp(buf, "⅚") == 0);

        written = num_sprintf(NULL, 0u, "%n", rat_general);
        ASSERT_EQ_INT(written, (int)strlen("³⁵⁵⁄₁₁₃"));
        written = num_sprintf(buf, sizeof(buf), "%n", rat_general);
        ASSERT_EQ_INT(written, (int)strlen("³⁵⁵⁄₁₁₃"));
        ASSERT_TRUE(strcmp(buf, "³⁵⁵⁄₁₁₃") == 0);

        written = num_sprintf(buf, sizeof(buf), "%N", rat_general);
        ASSERT_EQ_INT(written, (int)strlen("³⁵⁵⁄₁₁₃"));
        ASSERT_TRUE(strcmp(buf, "³⁵⁵⁄₁₁₃") == 0);

        written = num_sprintf(buf, sizeof(buf), "value=%n!", rat_general);
        ASSERT_EQ_INT(written, (int)strlen("value=³⁵⁵⁄₁₁₃!"));
        ASSERT_TRUE(strcmp(buf, "value=³⁵⁵⁄₁₁₃!") == 0);

        assert_number_string_prefix("num_beta(2, 2)", beta, "0.166666666666666666666666666666");
        assert_number_string_prefix("num_atan2(1, 1)", angle, "0.785398163397448309615660845819");
        assert_number_string("num_gamma(5)", gamma5, "24");
        assert_number_string_prefix("num_Ei(1)", ei1, "1.895117816355936755466520934331");
        assert_number_string("num_create_from_cdouble(1 + i)", cdouble_unit, "1 + i");
        assert_number_string("num_create_from_cdouble(1 - i)", cdouble_neg_unit, "1 - i");
        assert_number_string("num_create_from_cdouble(i)", cdouble_pure_unit, "i");

        num_destroy(&dec);
        num_destroy(&rat);
        num_destroy(&rat_general);
        num_destroy(&one);
        num_destroy(&two);
        num_destroy(&five);
        num_destroy(&beta);
        num_destroy(&angle);
        num_destroy(&gamma5);
        num_destroy(&ei1);
        num_destroy(&cdouble_unit);
        num_destroy(&cdouble_neg_unit);
        num_destroy(&cdouble_pure_unit);
    }
}
