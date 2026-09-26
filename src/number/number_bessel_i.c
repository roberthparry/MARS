#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

/* Evaluate principal modified Bessel I, including its negative-integer order limits. */
number_t num_bessel_i(const number_t order, const number_t argument)
{
    number_t value = number_modified_series(order, argument, NUMBER_MODIFIED_BESSEL_I);
    number_scope_register_value(&value);
    return value;
}
