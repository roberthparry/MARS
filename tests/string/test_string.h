#ifndef TESTS_STRING_TEST_STRING_H
#define TESTS_STRING_TEST_STRING_H

#include <string.h>

#include "test_harness.h"

#define ASSERT_TRUE(expr) TEST_ASSERT_TRUE((expr), #expr)

#define ASSERT_EQ(actual, expected) TEST_ASSERT_LONG_EQ((long)(actual), (long)(expected))

#define ASSERT_NOT_NULL(ptr) TEST_ASSERT_NOT_NULL((ptr))

#define ASSERT_OK(expr) TEST_ASSERT_INT_EQ((expr), 0)

#define ASSERT_STREQ(actual, expected)                                                                                 \
    do {                                                                                                               \
        const char *_actual_text = (actual);                                                                           \
        const char *_expected_text = (expected);                                                                       \
        if (!_actual_text || !_expected_text || strcmp(_actual_text, _expected_text) != 0) {                           \
            test_set_failure_detailf("expected \"%s\", got \"%s\"", _expected_text ? _expected_text : "(null)",        \
                                     _actual_text ? _actual_text : "(null)");                                          \
            test_mark_failure(__FILE__, __LINE__, "string equality");                                                  \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#endif
