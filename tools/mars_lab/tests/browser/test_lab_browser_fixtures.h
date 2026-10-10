/**
 * @file test_lab_browser_fixtures.h
 * @brief Private fixture builder for the native Lab browser-test harness.
 *
 * Separates native presentation/Protobuf fixture construction from process
 * supervision. Only the disposable browser test page consumes this API.
 */
#ifndef TEST_LAB_BROWSER_FIXTURES_H
#define TEST_LAB_BROWSER_FIXTURES_H

#include "ustring.h"

/**
 * @brief Append encoded native calendar fixtures to the browser test script.
 * @param script Borrowed mutable script string.
 * @return True on success, false on allocation or encoding failure.
 */
bool lab_browser_almanac_fixture(string_t *script);

#endif
