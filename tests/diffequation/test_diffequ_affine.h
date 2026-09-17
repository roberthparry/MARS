#ifndef TEST_DIFFEQU_AFFINE_H
#define TEST_DIFFEQU_AFFINE_H

#include "diffequation.h"

bool test_diffequ_affine_solution_verified(const diffequ_t *de, const equation_t *solution);

void test_diffequ_affine_transport(void);
void test_diffequ_affine_transport_scope(void);
void test_diffequ_time_affine_transport(void);
void test_diffequ_ordered_equation_TeX(void);
void example_diffequation_affine_transport(void);
void example_diffequation_time_affine_transport(void);
void example_diffequation_symbolic_affine_transport(void);

#endif
