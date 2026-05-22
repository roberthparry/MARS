#ifndef NUMBER_SHARED_INTERNAL_H
#define NUMBER_SHARED_INTERNAL_H

#include "number.h"

number_t number_invalid(void);
bool num_is_immortal(number_t number);
bool num_is_mfloat_backend(number_t number);
bool num_is_mcomplex_backend(number_t number);
bool num_get_small_rational(number_t number, long *numerator, long *denominator);
number_t num_as_mfloat_prec(number_t number, size_t precision_bits);
number_t num_as_mcomplex_prec(number_t number, size_t precision_bits);
num_scope_t *number_scope_suspend(void);
void number_scope_resume(num_scope_t *scope);
void num_scope_resume_cleanup(num_scope_t **scope);

#define NUM_SCOPE_SUSPEND(name) \
    __attribute__((cleanup(num_scope_resume_cleanup))) num_scope_t *(name) = number_scope_suspend()

#endif
