/**
 * @file test_number_readme_appell.c
 * @brief Runnable Appell example shared by the numerical and expression module guides.
 *
 * Exercises the public qfloat, qcomplex, number, expression and commuting-matrix APIs, then verifies the
 * documented output. The number suite runs this example after its ordinary numerical regression tests.
 */
#include <stdio.h>
#include <string.h>

#include "expression-t.h"
#include "matrix-t.h"
#include "number-t.h"
#include "qcomplex-t.h"
#include "qfloat-t.h"
#include "test_harness.h"
#include "test_number.h"

static int run_appell_example(char *output, size_t capacity)
{
    size_t used = 0;
    NUM_SCOPE(scope);
    qfloat_t fixed = qf_appell_F2(QF_ONE, QF_ONE, QF_ONE, QF_ONE, QF_ONE,
                                 qf_from_double(0.125), qf_from_double(0.25));
    qcomplex_t minus_one = qc_make(qf_from_double(-1), QF_ZERO);
    qcomplex_t complex_value = qc_appell_F3(minus_one, minus_one, QC_ONE, QC_ONE, QC_ONE,
                                     qc_make(qf_from_double(0.125), QF_ZERO),
                                     qc_make(qf_from_double(0.25), QF_ZERO));
    number_t polynomial = num_appell_F4(NUM_NEG_ONE, NUM_ONE, NUM_ONE, NUM_ONE, NUM_TWO,
                                        num_create_from_long(3));
    expr_t *expression = expr_from_string("appell_F2(1,1,1,1,1,1/8,1/4)", NULL);
    matrix_t *x = mat_from_string("(0,1/16;0,0)");
    matrix_t *y = mat_from_string("(0,1/32;0,0)");
    matrix_t *matrix = mat_appell_F2(NUM_ONE, NUM_ONE, NUM_ONE, NUM_ONE, NUM_ONE, x, y);
    if (!expression || !matrix) {
        mat_free(matrix); mat_free(y); mat_free(x); expr_free(expression);
        return 1;
    }
    used += snprintf(output + used, capacity - used, "qfloat F2 = %.6f\n", qf_to_double(fixed));
    used += snprintf(output + used, capacity - used, "qcomplex F3 = %.6f\n", qf_to_double(qc_real(complex_value)));
    used += snprintf(output + used, capacity - used, "number F4 = %.6f\n", num_to_double(polynomial));
    used += snprintf(output + used, capacity - used, "expression F2 = %.6f\n", num_to_double(expr_eval(expression)));
    used += snprintf(output + used, capacity - used, "matrix F2[0,1] = %.6f\n", num_to_double(mat_get_num(matrix, 0, 1)));
    mat_free(matrix); mat_free(y); mat_free(x); expr_free(expression);
    return used < capacity ? 0 : 1;
}

/* README example: reproduce every line of the cross-module Appell example in docs/number.md. */
void run_number_readme_appell(void)
{
    const char *expected =
        "qfloat F2 = 1.600000\n"
        "qcomplex F3 = 0.640625\n"
        "number F4 = -4.000000\n"
        "expression F2 = 1.600000\n"
        "matrix F2[0,1] = 0.093750\n";
    char output[256];
    bool ok = run_appell_example(output, sizeof output) == 0;
    if (ok) fputs(output, stdout);
    TEST_ASSERT_TRUE(ok && !strcmp(output, expected), "README Appell output agrees across all public modules");
}
