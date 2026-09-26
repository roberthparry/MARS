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
