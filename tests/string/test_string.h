#ifndef TESTS_STRING_TEST_STRING_H
#define TESTS_STRING_TEST_STRING_H

#include <string.h>

#include "test_harness.h"

#define ASSERT_TRUE(expr) TEST_ASSERT_TRUE((expr), #expr)

#define ASSERT_EQ(got, want) TEST_ASSERT_LONG_EQ((long)(got), (long)(want))

#define ASSERT_NOT_NULL(ptr) TEST_ASSERT_NOT_NULL((ptr))

#define ASSERT_OK(expr) TEST_ASSERT_INT_EQ((expr), 0)

#define ASSERT_STREQ(got, want)                                                                                 \
    do {                                                                                                               \
        const char *_got_text = (got);                                                                           \
        const char *_want_text = (want);                                                                       \
        if (!_got_text || !_want_text || strcmp(_got_text, _want_text) != 0) {                           \
            test_set_failure_detailf("want \"%s\", got \"%s\"", _want_text ? _want_text : "(null)",        \
                                     _got_text ? _got_text : "(null)");                                          \
            test_mark_failure(__FILE__, __LINE__, "string equality");                                                  \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#endif
