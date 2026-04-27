#include "test_qfloat.h"

void test_readme_examples(void) {
    /* Compute W0(x) for several representative values */
    const char *inputs[] = {
        "0",
        "1e-6",
        "0.1",
        "1",
        "5",
        "-0.3678794411714423215955237701614609", /* -1/e */
        NULL
    };

    for (int i = 0; inputs[i] != NULL; i++) {
        /* Parse x from a decimal string */
        qfloat_t x = qf_from_string(inputs[i]);

        /* Compute the principal branch W0(x) */
        qfloat_t w = qf_lambert_w0(x);

        qf_printf("W0(%s) = %q\n", inputs[i], w);
    }    
}
