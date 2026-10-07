/**
 * @file test_expr_string_groups.c
 * @brief Expression formatting test-group assembly.
 *
 * Registers the native, function and TeX string-output groups with the common expression harness. This file
 * coordinates tests defined across the formatting sources rather than implementing a formatter.
 *
 * Used by the project test harness for regression verification. Select cases through tests/test_config.json and
 * run suites sequentially; this source is not part of the installed library.
 */

#include "test_expr.h"

void test_expr_t_to_string(void)
{
    TEST_RUN_SUBTEST(test_to_string_all, NULL);
    TEST_RUN_SUBTEST(test_expressions, NULL);
    TEST_RUN_SUBTEST(test_expressions_unnamed, NULL);
    TEST_RUN_SUBTEST(test_expressions_longname, NULL);
}
