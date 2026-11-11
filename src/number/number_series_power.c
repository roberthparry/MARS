/**
 * @file number_series_power.c
 * @brief Exact machine-word powers for multiprecision series denominators.
 *
 * Polylogarithm and Legendre chi series often divide by small integer powers. Checked binary exponentiation
 * identifies denominators that fit unsigned long, allowing MPFR/MPC's exact integer division without building
 * a floating-point power. Overflow selects the existing arbitrary-precision calculation; no precision is lost.
 */
#include <limits.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

/* Calculate an exact small power, reporting overflow without wrapping an intermediate product. */
unsigned long number_series_integer_power(unsigned long base, unsigned long exponent)
{
    unsigned long result = 1;
    while (exponent) {
        if (exponent & 1u) {
            if (base && result > ULONG_MAX / base) return 0;
            result *= base;
        }
        exponent >>= 1u;
        if (exponent) {
            if (base && base > ULONG_MAX / base) return 0;
            base *= base;
        }
    }
    return result;
}
