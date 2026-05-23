#ifndef MFLOAT_NUMBER_INTERNAL_H
#define MFLOAT_NUMBER_INTERNAL_H

#include <mpc.h>
#include <mpfr.h>

#include "mfloat.h"

mfloat_t *mf_create_from_mpfr_prec(mpfr_srcptr value, size_t precision_bits);
int mf_mpc_set_from_parts(mpc_t out, const mfloat_t *real,
                          const mfloat_t *imag);
int mf_complex_mul_parts(const mfloat_t *ar, const mfloat_t *ai,
                         const mfloat_t *br, const mfloat_t *bi,
                         mfloat_t **real_out, mfloat_t **imag_out);
int mf_complex_div_parts(const mfloat_t *ar, const mfloat_t *ai,
                         const mfloat_t *br, const mfloat_t *bi,
                         mfloat_t **real_out, mfloat_t **imag_out);

#endif
