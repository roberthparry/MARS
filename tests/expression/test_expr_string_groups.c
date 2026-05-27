#include "test_expr.h"

void test_expr_t_to_string(void)
{
    TEST_RUN_SUBTEST(test_to_string_all, NULL);
    TEST_RUN_SUBTEST(test_expressions, NULL);
    TEST_RUN_SUBTEST(test_expressions_unnamed, NULL);
    TEST_RUN_SUBTEST(test_expressions_longname, NULL);
}
