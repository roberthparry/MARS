/**
 * @file test_lab_support.h
 * @brief Private fixtures and registrations for the sequential native Lab suite.
 *
 * Shared by this test directory only. JSON helpers borrow members, while the
 * isolated fixture runs a callback in a child with a private state directory and
 * the repository's page asset. Application callers use tools/mars_lab headers instead.
 */
#ifndef MARS_TEST_LAB_SUPPORT_H
#define MARS_TEST_LAB_SUPPORT_H

#include "json.h"

/** Parse literal JSON into a caller-owned tree. */
json_t *test_lab_json(const char *text);

/** Borrow an object member using an ASCII key. */
const json_t *test_lab_member(const json_t *object, const char *key);

/** Borrow a string member's text, or an empty string for absent/wrong types. */
const char *test_lab_text(const json_t *object, const char *key);

/** Check the explicitly boolean ok response member. */
bool test_lab_ok(const json_t *object, bool expected);

/** Run a callback with isolated state/environment and remove its bounded private tree without following links. */
bool test_lab_isolated(bool (*callback)(const char *directory));

/** Create and configure a private encrypted jurisdiction fixture beneath the supplied test directory. */
bool test_lab_catalogue_database(const char *directory);

/** Register and run mathematical and calendar adapter integration assertions. */
void test_lab_evaluation_cases(void);

/** Register native Function-card runtime and generated-programme regressions. */
void test_lab_function_cases(void);

/** Run documented Function-card programmes after the ordinary tests. */
void test_lab_function_readme_cases(void);

/** Register and run the additional evaluation regressions owned by the evaluator test helper. */
void test_lab_evaluation_extra_cases(void);

/** Register and run named-town timezone, location-response and DateTime card regressions. */
void test_lab_calendar_extra_cases(void);

/** Register and run state persistence and page escaping assertions. */
void test_lab_state_cases(void);

/** Register native QR geometry, input bounds and local-only mobile metadata assertions. */
void test_lab_mobile_cases(void);

/** Register isolated runtime key creation and preservation assertions without logging secrets. */
void test_lab_runtime_cases(void);

/** Register and run HTTP route integration assertions with sequential server children. */
void test_lab_route_cases(void);

/** @brief Run bounded native Protobuf schema regressions. @return No value. */
void test_lab_wire_cases(void);

/** @brief Run native presentation and structured editor regressions. @return No value. */
void test_lab_presentation_cases(void);

/** @brief Run native form parsing and validation regressions. @return No value. */
void test_lab_forms_cases(void);

/** @brief Run native Function lexical metadata regressions. @return No value. */
void test_lab_syntax_cases(void);

/** @brief Run native almanac display and clipboard regressions. @return No value. */
void test_lab_almanac_presentation_cases(void);

/** @brief Register native mathematical worker regressions. @return No value. */
void test_lab_math_cases(void);

/** @brief Run mathematical README examples after ordinary tests. @return No value. */
void test_lab_math_readme_cases(void);

#endif
