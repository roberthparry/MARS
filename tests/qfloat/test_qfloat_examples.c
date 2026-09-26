#include "test_qfloat.h"

void test_readme_examples(void)
{
    /* Compute W0(x) for several representative values */
    const char *inputs[] = {"0", "1e-6", "0.1", "1", "5", "-0.3678794411714423215955237701614609", /* -1/e */
                            NULL};

    for (int i = 0; inputs[i] != NULL; i++) {
        /* Parse x from a decimal string */
        qfloat_t x = qf_from_string(inputs[i]);

        /* Compute the principal branch W0(x) */
        qfloat_t w = qf_lambert_w0(x);

        qf_printf("W0(%s) = %q\n", inputs[i], w);
    }

    /* README example: modified Struve L (docs/qfloat.md). */
    qfloat_t value = qf_struve_l(QF_ZERO, QF_ONE);
    qf_printf("L_0(1) = %.16q\n", value);
    char output[80];
    qf_sprintf(output, sizeof(output), "L_0(1) = %.16q", value);
    TEST_ASSERT_TRUE(strcmp(output, "L_0(1) = 0.7102431859378909") == 0, "Struve L README output");

    /* README example: modified Bessel I (docs/qfloat.md). */
    qfloat_t bessel = qf_bessel_i(QF_ZERO, QF_ONE);
    qf_printf("I_0(1) = %.16q\n", bessel);
    qf_sprintf(output, sizeof(output), "I_0(1) = %.16q", bessel);
    TEST_ASSERT_TRUE(strcmp(output, "I_0(1) = 1.2660658777520083") == 0, "Bessel I README output");

    /* README example: ordinary Struve H (docs/qfloat.md). */
    qfloat_t ordinary = qf_struve_h(QF_ZERO, QF_ONE);
    qf_printf("H_0(1) = %.15q\n", ordinary);
    qf_sprintf(output, sizeof(output), "H_0(1) = %.15q", ordinary);
    TEST_ASSERT_TRUE(strcmp(output, "H_0(1) = 0.568656627048288") == 0, "Struve H README output");
}
