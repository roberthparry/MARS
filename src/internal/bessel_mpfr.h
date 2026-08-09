#ifndef MARS_BESSEL_MPFR_H
#define MARS_BESSEL_MPFR_H

#include <mpfr.h>

int mars_mpfr_bessel_j(mpfr_ptr out, mpfr_srcptr order,
                       mpfr_srcptr argument, mpfr_rnd_t rounding);
int mars_mpfr_bessel_y(mpfr_ptr out, mpfr_srcptr order,
                       mpfr_srcptr argument, mpfr_rnd_t rounding);

#endif
