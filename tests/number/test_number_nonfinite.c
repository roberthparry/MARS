/**
 * @file test_number_nonfinite.c
 * @brief Non-finite numeric and series regressions.
 *
 * Checks supported infinity, NaN and exceptional series behaviour. It guards explicit edge-case semantics rather
 * than allowing accidental convergence or ordinary finite results.
 *
 * Used by the project test harness for regression verification. Select cases through tests/test_config.json and
 * run suites sequentially; this source is not part of the installed library.
 */

#include <math.h>

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
        number_t dilog = num_dilog(input), polylog = num_polylog(num_create_from_long(3), input);
        if ((i < 2u && !num_is_nan(dilog)) || (i >= 2u && num_is_finite(dilog)))
            printf("Non-finite input %zu: dilog finite=%d, nan=%d, inf=%d\n", i,
                   num_is_finite(dilog), num_is_nan(dilog), num_is_inf(dilog));
        ASSERT_TRUE(i < 2u ? num_is_nan(dilog) : !num_is_finite(dilog));
        ASSERT_TRUE(i < 2u ? num_is_nan(polylog) : !num_is_finite(polylog));
        ASSERT_TRUE(num_is_nan(num_appell_F1(NUM_HALF, NUM_ONE, NUM_ONE, NUM_TWO, input, NUM_HALF)));
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

    /* Double dispatch retains the real-domain endpoint contracts of the other fixed-precision backend. */
    number_t infinity = num_create_from_double(INFINITY), nan = num_create_from_double(NAN);
    number_t one = num_create_from_double(1.0), two = num_create_from_double(2.0);
    number_t negative = num_create_from_double(-INFINITY), half = num_create_from_double(0.5);
    ASSERT_TRUE(num_is_one(num_zeta(infinity)));
    ASSERT_TRUE(num_is_zero(num_zetap(infinity)));
    ASSERT_TRUE(num_is_nan(num_zetap(negative)));
    ASSERT_TRUE(num_is_nan(num_zetap(nan)));
    ASSERT_TRUE(num_is_zero(num_zetah(two, infinity)));
    ASSERT_TRUE(num_is_zero(num_zatahp(two, infinity)));
    ASSERT_TRUE(num_is_one(num_zetah(infinity, one)));
    ASSERT_TRUE(num_is_zero(num_zetah(infinity, two)));
    ASSERT_TRUE(num_is_zero(num_zatahp(infinity, one)));
    ASSERT_TRUE(num_is_nan(num_zetah(infinity, half)));
    ASSERT_TRUE(num_is_nan(num_zetah(two, negative)));
    ASSERT_TRUE(num_is_nan(num_zatahp(two, negative)));
    ASSERT_TRUE(num_is_nan(num_zatahp(two, nan)));
    ASSERT_TRUE(num_is_inf(num_zetah(one, two)));
    number_t derivative_pole = num_zatahp(one, two);
    ASSERT_TRUE(num_is_inf(derivative_pole) && num_sign(derivative_pole) < 0);
}
