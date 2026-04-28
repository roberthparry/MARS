#include "test_matrix.h"

static void test_mat_to_string_numeric(void)
{
    qfloat_t vals[4] = {
        qf_from_double(1.0), qf_from_double(2.0),
        qf_from_double(3.0), qf_from_double(4.0)
    };
    matrix_t *A = mat_create_qf(2, 2, vals);
    char *inline_pretty = mat_to_string(A, MAT_STRING_INLINE_PRETTY);
    char *layout_scientific = mat_to_string(A, MAT_STRING_LAYOUT_SCIENTIFIC);

    check_bool("mat_to_string qfloat inline string non-null", inline_pretty != NULL);
    check_bool("mat_to_string qfloat inline exact",
               inline_pretty && strcmp(inline_pretty, "[[1 2][3 4]]") == 0);
    check_bool("mat_to_string qfloat layout scientific non-null", layout_scientific != NULL);
    check_bool("mat_to_string qfloat layout scientific has newline",
               layout_scientific && strchr(layout_scientific, '\n') != NULL);
    check_bool("mat_to_string qfloat layout scientific has exponent",
               layout_scientific && strchr(layout_scientific, 'E') != NULL);

    free(inline_pretty);
    free(layout_scientific);
    mat_free(A);
}

static void test_mat_to_string_symbolic(void)
{
    binding_t *bindings = NULL;
    size_t nbindings = 0;
    matrix_t *A = mat_from_string("{ [[x 1][1 c1]] | x = 2; c1 = 3 }",
                                  &bindings, &nbindings);
    char *inline_pretty = mat_to_string(A, MAT_STRING_INLINE_PRETTY);
    char *layout_pretty = mat_to_string(A, MAT_STRING_LAYOUT_PRETTY);

    check_bool("mat_to_string symbolic inline non-null", inline_pretty != NULL);
    check_bool("mat_to_string symbolic layout non-null", layout_pretty != NULL);
    check_bool("mat_to_string symbolic inline wrapped",
               inline_pretty && strstr(inline_pretty, "{ [") != NULL);
    check_bool("mat_to_string symbolic inline has bindings",
               inline_pretty && strstr(inline_pretty, "x = 2") != NULL);
    check_bool("mat_to_string symbolic layout has newline",
               layout_pretty && strchr(layout_pretty, '\n') != NULL);

    free(inline_pretty);
    free(layout_pretty);
    free(bindings);
    mat_free(A);
}

void run_matrix_tostring_tests(void)
{
    test_mat_to_string_numeric();
    test_mat_to_string_symbolic();
}
