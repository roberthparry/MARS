#ifndef SRC_INTERNAL_MRATIONAL_INTERNAL_H
#define SRC_INTERNAL_MRATIONAL_INTERNAL_H

#include <stddef.h>

#include "mrational.h"

size_t mr_bernoulli_even_term_count(void);
const mrational_t *mr_bernoulli_even_term(size_t index);

#endif
