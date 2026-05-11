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
               inline_pretty && strcmp(inline_pretty, "(1, 2; 3, 4)") == 0);
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
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string("{ (x, 1; 1, c1) | x = 2; c1 = 3 }",
                                  &bindings);
    char *inline_pretty = mat_to_string(A, MAT_STRING_INLINE_PRETTY);
    char *layout_pretty = mat_to_string(A, MAT_STRING_LAYOUT_PRETTY);

    check_bool("mat_to_string symbolic inline non-null", inline_pretty != NULL);
    check_bool("mat_to_string symbolic layout non-null", layout_pretty != NULL);
    check_bool("mat_to_string symbolic inline wrapped",
               inline_pretty && strstr(inline_pretty, "{ (") != NULL);
    check_bool("mat_to_string symbolic inline has bindings",
               inline_pretty && strstr(inline_pretty, "x = 2") != NULL);
    check_bool("mat_to_string symbolic layout has newline",
               layout_pretty && strchr(layout_pretty, '\n') != NULL);

    free(inline_pretty);
    free(layout_pretty);
    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_to_string_symbolic_all_nan_elides_wrapper(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string("(x, c1)", &bindings);
    char *inline_pretty = mat_to_string(A, MAT_STRING_INLINE_PRETTY);
    char *layout_pretty = mat_to_string(A, MAT_STRING_LAYOUT_PRETTY);

    check_bool("mat_to_string symbolic all-NaN inline string non-null", inline_pretty != NULL);
    check_bool("mat_to_string symbolic all-NaN layout string non-null", layout_pretty != NULL);
    check_bool("mat_to_string symbolic all-NaN inline omits wrapper",
               inline_pretty && strcmp(inline_pretty, "(x, c₁)") == 0);
    check_bool("mat_to_string symbolic all-NaN layout omits wrapper",
               layout_pretty && strcmp(layout_pretty, "(\n  x c₁\n)") == 0);

    free(inline_pretty);
    free(layout_pretty);
    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_to_string_symbolic_roundtrip(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string("(x, c1; x*y, [radius])", &bindings);
    char *inline_pretty = NULL;
    mat_bindings_t *roundtrip_bindings = NULL;
    matrix_t *roundtrip = NULL;
    dval_t *dv = NULL;

    check_bool("mat_to_string symbolic roundtrip source non-null", A != NULL);
    check_bool("mat_to_string symbolic roundtrip source bindings returned",
               bindings != NULL);
    check_bool("mat_to_string symbolic roundtrip set x",
               test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
    check_bool("mat_to_string symbolic roundtrip set y",
               test_mat_bindings_set_d(bindings, "y", 3.0) == 0);
    check_bool("mat_to_string symbolic roundtrip set c₁",
               test_mat_bindings_set_d(bindings, "c₁", 5.0) == 0);
    check_bool("mat_to_string symbolic roundtrip set [radius]",
               test_mat_bindings_set_d(bindings, "[radius]", 7.0) == 0);

    inline_pretty = mat_to_string(A, MAT_STRING_INLINE_PRETTY);
    check_bool("mat_to_string symbolic roundtrip string non-null", inline_pretty != NULL);
    check_bool("mat_to_string symbolic roundtrip keeps wrapper",
               inline_pretty && strstr(inline_pretty, "{ (") != NULL);

    roundtrip = mat_from_string(inline_pretty, &roundtrip_bindings);
    check_bool("mat_to_string symbolic roundtrip reparses", roundtrip != NULL);
    check_bool("mat_to_string symbolic roundtrip reparsed type",
               roundtrip && mat_typeof(roundtrip) == MAT_TYPE_DVAL);
    check_bool("mat_to_string symbolic roundtrip x binding present",
               mat_bindings_get(roundtrip_bindings, "x") != NULL);
    check_bool("mat_to_string symbolic roundtrip c₁ binding present",
               mat_bindings_get(roundtrip_bindings, "c₁") != NULL);
    check_bool("mat_to_string symbolic roundtrip [radius] binding present",
               mat_bindings_get(roundtrip_bindings, "[radius]") != NULL);

    if (roundtrip) {
        mat_get(roundtrip, 0, 0, &dv);
        check_qf_val("mat_to_string symbolic roundtrip x entry",
                     dv_eval_qf(dv), qf_from_double(2.0), 1e-18);
        mat_get(roundtrip, 0, 1, &dv);
        check_qf_val("mat_to_string symbolic roundtrip c₁ entry",
                     dv_eval_qf(dv), qf_from_double(5.0), 1e-18);
        mat_get(roundtrip, 1, 0, &dv);
        check_qf_val("mat_to_string symbolic roundtrip x*y entry",
                     dv_eval_qf(dv), qf_from_double(6.0), 1e-18);
        mat_get(roundtrip, 1, 1, &dv);
        check_qf_val("mat_to_string symbolic roundtrip [radius] entry",
                     dv_eval_qf(dv), qf_from_double(7.0), 1e-18);
    }

    free(inline_pretty);
    mat_bindings_free(roundtrip_bindings);
    mat_free(roundtrip);
    mat_bindings_free(bindings);
    mat_free(A);
}

static void test_mat_to_string_symbolic_derivative_roundtrip(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string("(x, c1; x*y, y)", &bindings);
    dval_t *x_binding = NULL;
    matrix_t *Dx = NULL;
    char *inline_pretty = NULL;
    mat_bindings_t *roundtrip_bindings = NULL;
    matrix_t *roundtrip = NULL;
    dval_t *dv = NULL;

    check_bool("mat_to_string symbolic derivative source non-null", A != NULL);
    x_binding = mat_bindings_get(bindings, "x");
    check_bool("mat_to_string symbolic derivative x binding present", x_binding != NULL);
    check_bool("mat_to_string symbolic derivative set y",
               test_mat_bindings_set_d(bindings, "y", 4.0) == 0);
    check_bool("mat_to_string symbolic derivative set c₁",
               test_mat_bindings_set_d(bindings, "c₁", 7.0) == 0);

    if (A && x_binding)
        Dx = mat_deriv(A, x_binding);
    check_bool("mat_to_string symbolic derivative matrix non-null", Dx != NULL);

    inline_pretty = mat_to_string(Dx, MAT_STRING_INLINE_PRETTY);
    check_bool("mat_to_string symbolic derivative string non-null", inline_pretty != NULL);
    check_bool("mat_to_string symbolic derivative keeps wrapper for concrete remaining bindings",
               inline_pretty && strstr(inline_pretty, "{ (") != NULL);
    check_bool("mat_to_string symbolic derivative keeps y binding value",
               inline_pretty && strstr(inline_pretty, "y = 4") != NULL);

    roundtrip = mat_from_string(inline_pretty, &roundtrip_bindings);
    check_bool("mat_to_string symbolic derivative reparses", roundtrip != NULL);
    check_bool("mat_to_string symbolic derivative reparsed type",
               roundtrip && mat_typeof(roundtrip) == MAT_TYPE_DVAL);
    check_bool("mat_to_string symbolic derivative reparsed has y",
               mat_bindings_get(roundtrip_bindings, "y") != NULL);
    check_bool("mat_to_string symbolic derivative reparsed omits x",
               mat_bindings_get(roundtrip_bindings, "x") == NULL);
    check_bool("mat_to_string symbolic derivative reparsed set y",
               test_mat_bindings_set_d(roundtrip_bindings, "y", 4.0) == 0);

    if (roundtrip) {
        mat_get(roundtrip, 0, 0, &dv);
        check_qf_val("mat_to_string symbolic derivative reparsed [0,0]",
                     dv_eval_qf(dv), qf_from_double(1.0), 1e-18);
        mat_get(roundtrip, 0, 1, &dv);
        check_qf_val("mat_to_string symbolic derivative reparsed [0,1]",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
        mat_get(roundtrip, 1, 0, &dv);
        check_qf_val("mat_to_string symbolic derivative reparsed [1,0]",
                     dv_eval_qf(dv), qf_from_double(4.0), 1e-18);
        mat_get(roundtrip, 1, 1, &dv);
        check_qf_val("mat_to_string symbolic derivative reparsed [1,1]",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
    }

    free(inline_pretty);
    mat_bindings_free(roundtrip_bindings);
    mat_free(roundtrip);
    mat_free(Dx);
    mat_bindings_free(bindings);
    mat_free(A);
}

void run_matrix_tostring_tests(void)
{
    test_mat_to_string_numeric();
    test_mat_to_string_symbolic();
    test_mat_to_string_symbolic_all_nan_elides_wrapper();
    test_mat_to_string_symbolic_roundtrip();
    test_mat_to_string_symbolic_derivative_roundtrip();
}
