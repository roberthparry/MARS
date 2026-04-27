#include "test_dval.h"

void test_dval_t_to_string(void)
{
    RUN_TEST(test_to_string_all, __func__);
    RUN_TEST(test_expressions, __func__);
    RUN_TEST(test_expressions_unnamed, __func__);
    RUN_TEST(test_expressions_longname, __func__);
}
