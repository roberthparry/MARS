/**
 * @file qcomplex_cylindrical.c
 * @brief Complex double-double adapters for Bessel and Struve functions.
 *
 * Groups the Bessel I, K and Y and Struve H/L entry points that use guarded number-layer kernels. Each adapter
 * preserves its existing domain checks, conversion, rounding and numeric-scope cleanup. The numerical kernels
 * remain in the number module; this file does not introduce another special-function implementation.
 */

#include "number.h"

/* Round principal modified Bessel I to complex double-double precision. */
qcomplex_t qc_bessel_i(qcomplex_t order, qcomplex_t argument)
{
    NUM_SCOPE(scope);
    number_t value = num_bessel_i(num_create_from_qcomplex(order), num_create_from_qcomplex(argument));
    if (num_is_nan(value))
        return QC_NAN;
    return qc_make(num_to_qfloat(num_real_part(value)), num_to_qfloat(num_imag_part(value)));
}

/* Round the principal complex modified Bessel K after guarded evaluation. */
qcomplex_t qc_bessel_k(qcomplex_t order, qcomplex_t argument)
{
    NUM_SCOPE(scope);
    number_t value = num_bessel_k(num_create_from_qcomplex(order), num_create_from_qcomplex(argument));
    qcomplex_t result = qc_make(num_to_qfloat(num_real_part(value)), num_to_qfloat(num_imag_part(value)));
    num_destroy(&value);
    return result;
}

/* Round the native principal Bessel Y value to complex double-double precision. */
qcomplex_t qc_bessel_y(qcomplex_t order, qcomplex_t argument)
{
    NUM_SCOPE(scope);
    number_t value = num_bessel_y(num_create_from_qcomplex(order), num_create_from_qcomplex(argument));
    qcomplex_t result = num_is_nan(value) ? QC_NAN :
                        qc_make(num_to_qfloat(num_real_part(value)), num_to_qfloat(num_imag_part(value)));

    num_destroy(&value);
    return result;
}

/* Round principal ordinary Struve H to complex double-double precision. */
qcomplex_t qc_struve_h(qcomplex_t order, qcomplex_t argument)
{
    NUM_SCOPE(scope);
    number_t value = num_struve_h(num_create_from_qcomplex(order), num_create_from_qcomplex(argument));
    if (num_is_nan(value))
        return QC_NAN;
    return qc_make(num_to_qfloat(num_real_part(value)), num_to_qfloat(num_imag_part(value)));
}

/* Round the guarded principal modified Struve L to complex double-double precision. */
qcomplex_t qc_struve_l(qcomplex_t order, qcomplex_t argument)
{
    NUM_SCOPE(scope);
    number_t value = num_struve_l(num_create_from_qcomplex(order), num_create_from_qcomplex(argument));
    if (num_is_nan(value))
        return QC_NAN;
    return qc_make(num_to_qfloat(num_real_part(value)), num_to_qfloat(num_imag_part(value)));
}
