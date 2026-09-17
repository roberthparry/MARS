#ifndef TEST_DIFFEQU_WAVE_IVP_H
#define TEST_DIFFEQU_WAVE_IVP_H

#include "diffequation.h"

/* Shared numerical residual check for wave-equation tests. */
bool test_diffequ_wave_zero_at_samples(const expr_t *expr, const diffequ_t *de);

/* Polynomial and trigonometric wave IVPs, with independently configurable cases. */
void test_diffequ_wave_ivp_polynomial_data(void);
void test_diffequ_wave_ivp_zero_terms(void);
void example_diffequation_wave_zero_data(void);

#endif
