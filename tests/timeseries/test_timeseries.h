#ifndef TESTS_TIMESERIES_TEST_TIMESERIES_H
#define TESTS_TIMESERIES_TEST_TIMESERIES_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "datetime-t.h"
#include "matrix-t.h"
#include "number-t.h"
#include "test_harness.h"
#include "timeseries-t.h"

void run_timeseries_core_tests(void);
void run_timeseries_output_tests(void);
void run_timeseries_model_tests(void);

#endif
