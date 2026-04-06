#ifndef TEST_STRING_H
#define TEST_STRING_H

#include <stdio.h>
#include <string.h>

#define CLR_RESET   "\x1b[0m"
#define CLR_RED     "\x1b[31m"
#define CLR_GREEN   "\x1b[32m"
#define CLR_YELLOW  "\x1b[33m"
#define CLR_CYAN    "\x1b[36m"

#define TEST(name)                                                  \
    do {                                                            \
        printf(CLR_CYAN "Running %s..." CLR_RESET "\n", #name);     \
        int _result = name();                                       \
        if (_result == 0)                                           \
            printf(CLR_GREEN "PASS: %s" CLR_RESET "\n\n", #name);   \
        else                                                        \
            printf(CLR_RED "FAIL: %s" CLR_RESET "\n\n", #name);     \
    } while (0)

/* ------------------------------------------------------------------------- */
/* Assertions with success echo                                              */
/* ------------------------------------------------------------------------- */

#define ASSERT_TRUE(expr)                                           \
    do {                                                            \
        if (!(expr)) {                                              \
            printf(CLR_RED                                          \
                   "  Assertion failed at %s:%d\n"                  \
                   "    expression: %s\n"                           \
                   "    expected:   true\n"                         \
                   "    actual:     false\n"                        \
                   CLR_RESET,                                       \
                   __FILE__, __LINE__, #expr);                      \
            return 1;                                               \
        }                                                           \
        printf(CLR_GREEN                                            \
               "  OK at %s:%d: %s == true\n"                        \
               CLR_RESET,                                           \
               __FILE__, __LINE__, #expr);                          \
    } while (0)

#define ASSERT_EQ(actual, expected)                                 \
    do {                                                            \
        long _a = (long)(actual);                                   \
        long _e = (long)(expected);                                 \
        if (_a != _e) {                                             \
            printf(CLR_RED                                          \
                   "  Value mismatch at %s:%d\n"                    \
                   "    expression: %s == %s\n"                     \
                   "    expected:   %ld\n"                          \
                   "    actual:     %ld\n"                          \
                   CLR_RESET,                                       \
                   __FILE__, __LINE__, #actual, #expected, _e, _a); \
            return 1;                                               \
        }                                                           \
        printf(CLR_GREEN                                            \
               "  OK at %s:%d: %s == %s (%ld)\n"                    \
               CLR_RESET,                                           \
               __FILE__, __LINE__, #actual, #expected, _a);         \
    } while (0)

#define ASSERT_STREQ(actual, expected)                              \
    do {                                                            \
        const char *_a = (actual);                                  \
        const char *_e = (expected);                                \
        if (strcmp(_a, _e) != 0) {                                  \
            printf(CLR_RED                                          \
                   "  String mismatch at %s:%d\n"                   \
                   "    expression: %s == %s\n"                     \
                   "    expected:   \"%s\"\n"                       \
                   "    actual:     \"%s\"\n"                       \
                   CLR_RESET,                                       \
                   __FILE__, __LINE__, #actual, #expected, _e, _a); \
            return 1;                                               \
        }                                                           \
        printf(CLR_GREEN                                            \
               "  OK at %s:%d: \"%s\" == \"%s\"\n"                  \
               CLR_RESET,                                           \
               __FILE__, __LINE__, _a, _e);                         \
    } while (0)

#define ASSERT_NOT_NULL(ptr)                                        \
    do {                                                            \
        if ((ptr) == NULL) {                                        \
            printf(CLR_RED                                          \
                   "  Null pointer at %s:%d\n"                      \
                   "    expression: %s\n"                           \
                   "    expected:   non-NULL\n"                     \
                   "    actual:     NULL\n"                         \
                   CLR_RESET,                                       \
                   __FILE__, __LINE__, #ptr);                       \
            return 1;                                               \
        }                                                           \
        printf(CLR_GREEN                                            \
               "  OK at %s:%d: %s != NULL\n"                        \
               CLR_RESET,                                           \
               __FILE__, __LINE__, #ptr);                           \
    } while (0)

#define ASSERT_NULL(ptr)                                            \
    do {                                                            \
        if ((ptr) != NULL) {                                        \
            printf(CLR_RED                                          \
                   "  Unexpected non-NULL pointer at %s:%d\n"       \
                   "    expression: %s\n"                           \
                   "    expected:   NULL\n"                         \
                   "    actual:     %p\n"                           \
                   CLR_RESET,                                       \
                   __FILE__, __LINE__, #ptr, (void *)(ptr));        \
            return 1;                                               \
        }                                                           \
        printf(CLR_GREEN                                            \
               "  OK at %s:%d: %s == NULL\n"                        \
               CLR_RESET,                                           \
               __FILE__, __LINE__, #ptr);                           \
    } while (0)

#define ASSERT_OK(expr)                                             \
    do {                                                            \
        int _r = (expr);                                            \
        if (_r != 0) {                                              \
            printf(CLR_RED                                          \
                   "  Unexpected error at %s:%d\n"                  \
                   "    expression: %s\n"                           \
                   "    expected:   0 (success)\n"                  \
                   "    actual:     %d\n"                           \
                   CLR_RESET,                                       \
                   __FILE__, __LINE__, #expr, _r);                  \
            return 1;                                               \
        }                                                           \
        printf(CLR_GREEN                                            \
               "  OK at %s:%d: %s returned 0\n"                     \
               CLR_RESET,                                           \
               __FILE__, __LINE__, #expr);                          \
    } while (0)

#define ASSERT_FAIL(msg)                                            \
    do {                                                            \
        printf(CLR_RED                                              \
               "  Failure triggered at %s:%d\n"                     \
               "    message: %s\n"                                  \
               CLR_RESET,                                           \
               __FILE__, __LINE__, msg);                            \
        return 1;                                                   \
    } while (0)

#endif
