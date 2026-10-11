/**
 * @file test_tba_support.h
 * @brief Private fixture cleanup shared by native and browser forecasting tests.
 *
 * Only temporary directories created by these tests may be passed here. Directory
 * traversal is bounded and never follows symbolic links.
 */
#ifndef TEST_TBA_SUPPORT_H
#define TEST_TBA_SUPPORT_H

#include "file_t.h"

/**
 * @brief Remove a completed test's private directory and its contents.
 * @param directory Borrowed, closed handle for this test's freshly created mars-tba- fixture.
 * @return Success, or false for an unsafe path or exceeded traversal limit.
 */
bool test_tba_cleanup(file_t *directory);

/** @brief Exercise encrypted transport and expiry without files or network I/O.
 * @return True when all authenticated boundary checks pass. */
bool test_tba_stream_checks(void);

#endif
