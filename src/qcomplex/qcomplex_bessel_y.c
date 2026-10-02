#include "number.h"

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
