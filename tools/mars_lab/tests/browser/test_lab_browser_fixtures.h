/**
 * @file test_lab_browser_fixtures.h
 * @brief Private fixture builder for the native Lab browser-test harness.
 *
 * Separates native presentation/Protobuf fixture construction from process
 * supervision. The browser harness consumes this API; its image probe checks the
 * private server before browser decoding begins.
 */
#ifndef TEST_LAB_BROWSER_FIXTURES_H
#define TEST_LAB_BROWSER_FIXTURES_H

#include "string-t.h"

/**
 * @brief Append encoded native calendar fixtures to the browser test script.
 * @param script Borrowed mutable script string.
 * @return True on success, false on allocation or encoding failure.
 */
bool lab_browser_almanac_fixture(string_t *script);

/**
 * @brief Verify that the loopback fixture sends an entire unchanged PNG body.
 * @param port Port of the running private fixture listener.
 * @return True for an exact successful transfer; false for any transport or byte mismatch.
 */
bool lab_browser_texture_probe(unsigned port);

#endif
