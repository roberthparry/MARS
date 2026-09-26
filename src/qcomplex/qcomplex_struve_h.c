#include "number.h"

/* Round principal ordinary Struve H to complex double-double precision. */
qcomplex_t qc_struve_h(qcomplex_t order, qcomplex_t argument)
{
    NUM_SCOPE(scope);
    number_t value = num_struve_h(num_create_from_qcomplex(order), num_create_from_qcomplex(argument));
    if (num_is_nan(value))
        return QC_NAN;
    return qc_make(num_to_qfloat(num_real_part(value)), num_to_qfloat(num_imag_part(value)));
}
