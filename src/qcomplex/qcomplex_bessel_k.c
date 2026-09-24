#include "number.h"

/* Round the principal complex modified Bessel K after guarded evaluation. */
qcomplex_t qc_bessel_k(qcomplex_t order, qcomplex_t argument)
{
    NUM_SCOPE(scope);
    number_t value = num_bessel_k(num_create_from_qcomplex(order), num_create_from_qcomplex(argument));
    return qc_make(num_to_qfloat(num_real_part(value)), num_to_qfloat(num_imag_part(value)));
}
