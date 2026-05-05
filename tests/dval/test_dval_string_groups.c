#include "test_dval.h"

void test_dval_t_to_string(void)
{
    RUN_SUBTEST(test_to_string_all);
    RUN_SUBTEST(test_expressions);
    RUN_SUBTEST(test_expressions_unnamed);
    RUN_SUBTEST(test_expressions_longname);
}
