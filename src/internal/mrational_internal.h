#ifndef MRATIONAL_SHARED_INTERNAL_H
#define MRATIONAL_SHARED_INTERNAL_H

#include <stddef.h>

#include "mrational.h"

extern mrational_t MR_HALF_VALUE;
extern mrational_t MR_ONE_AND_HALF_VALUE;
extern mrational_t MR_ONE_THIRD_VALUE;
extern mrational_t MR_QUARTER_VALUE;
extern mrational_t MR_ONE_SIXTH_VALUE;
extern mrational_t MR_ONE_EIGHTH_VALUE;
extern mrational_t MR_ONE_TENTH_VALUE;

size_t mr_bernoulli_even_term_count(void);
const mrational_t *mr_bernoulli_even_term(size_t index);

#endif
