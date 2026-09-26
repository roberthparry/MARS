#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

static size_t cylindrical_input_precision(number_t value)
{
    const number_private_t *impl = number_impl_const(&value);
    if (impl->kind != NUMBER_COMPLEX)
        return num_is_exact(value) ? num_get_default_prec_bits() : num_get_effective_prec_bits(value);

    /* NUMBER_COMPLEX's backend flag is inexact even when both components are exact rationals.
     * Such components have no stored precision, so the generic effective-precision fallback is
     * only 106 bits. Determine each component's contribution before that fallback is applied. */
    const complex_t *parts = impl->value.cx;
    size_t real = num_is_exact(parts->real) ? num_get_default_prec_bits() : num_get_effective_prec_bits(parts->real);
    size_t imag = num_is_exact(parts->imag) ? num_get_default_prec_bits() : num_get_effective_prec_bits(parts->imag);
    size_t precision = real > imag ? real : imag;
    return parts->precision_bits > precision ? parts->precision_bits : precision;
}

/* Retain exact complex components at the requested precision across the cylinder families. */
size_t number_cylindrical_precision(number_t order, number_t argument)
{
    size_t order_precision = cylindrical_input_precision(order);
    size_t argument_precision = cylindrical_input_precision(argument);
    return order_precision > argument_precision ? order_precision : argument_precision;
}
