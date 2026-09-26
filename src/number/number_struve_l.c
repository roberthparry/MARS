#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

/* Evaluate principal modified Struve L through the shared guarded series. */
number_t num_struve_l(const number_t order, const number_t argument)
{
    number_t value = number_modified_series(order, argument, NUMBER_MODIFIED_STRUVE_L);
    number_scope_register_value(&value);
    return value;
}
