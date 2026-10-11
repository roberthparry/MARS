/**
 * @file lab_evaluate_internal.h
 * @brief Controlled white-box test access to native Lab evaluation records.
 *
 * Only protocol regression tests should use this façade. Application modules
 * must use lab_evaluate.h; parser and renderer helpers belong to evaluate/.
 */
#ifndef MARS_LAB_EVALUATE_TEST_INTERNAL_H
#define MARS_LAB_EVALUATE_TEST_INTERNAL_H

#if !defined(MARS_SHARED_LAB_EVALUATE_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_LAB_EVALUATE_INTERNAL_ACCESS."
#endif
#define MARS_LAB_EVALUATE_INTERNAL_ACCESS
#include "../evaluate/lab_evaluate_internal.h"
#endif
