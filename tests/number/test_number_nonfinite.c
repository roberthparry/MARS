#include "test_number.h"

/* Non-finite complex inputs must not enter finite-exponent convergence arithmetic. */
void run_number_nonfinite_series_tests(void)
{
    NUM_SCOPE(scope);
    const qcomplex_t inputs[] = {
        qc_make(QF_NAN, QF_ONE), qc_make(QF_ONE, QF_NAN),
        qc_make(QF_INF, QF_ONE), qc_make(QF_ONE, QF_INF)
    };
    for (size_t i = 0u; i < sizeof(inputs) / sizeof(*inputs); ++i) {
        number_t input = num_create_from_qcomplex(inputs[i]);
        ASSERT_EQ_INT(num_set_prec_bits(&input, 256u), 0);
        ASSERT_TRUE(num_is_nan(num_Ei(input)));
        ASSERT_TRUE(num_is_nan(num_E1(input)));
        if (i < 2u)
            ASSERT_TRUE(num_is_nan(num_Li(input)));
        num_destroy(&input);
    }
    ASSERT_TRUE(num_is_nan(num_Ei(NUM_NAN)));
    ASSERT_TRUE(num_is_nan(num_E1(NUM_NAN)));
    ASSERT_TRUE(num_is_nan(num_Li(NUM_NAN)));
    number_t ei_zero = num_Ei(NUM_ZERO), e1_zero = num_E1(NUM_ZERO), li_one = num_Li(NUM_ONE);
    ASSERT_TRUE(num_is_inf(ei_zero) && num_sign(ei_zero) < 0);
    ASSERT_TRUE(num_is_inf(e1_zero) && num_sign(e1_zero) > 0);
    ASSERT_TRUE(num_is_inf(li_one) && num_sign(li_one) < 0);
}
