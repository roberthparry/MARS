#ifndef MARS_INTERNAL_LOMMEL_MPFR_H
#define MARS_INTERNAL_LOMMEL_MPFR_H

#include <mpfr.h>

int mars_mpfr_lommel_s(mpfr_ptr out, mpfr_srcptr mu, mpfr_srcptr nu,
                       mpfr_srcptr argument, mpfr_rnd_t rounding);
int mars_mpfr_lommel_s_derivative(mpfr_ptr out, mpfr_srcptr mu,
                                  mpfr_srcptr nu, mpfr_srcptr argument,
                                  mpfr_rnd_t rounding);

#endif
