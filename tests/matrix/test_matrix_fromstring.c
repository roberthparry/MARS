#include "test_matrix.h"

static binding_t *find_binding(binding_t *bindings, size_t n, const char *name)
{
    for (size_t i = 0; i < n; ++i) {
        if (strcmp(bindings[i].name, name) == 0)
            return &bindings[i];
    }
    return NULL;
}

static void test_mat_from_string_numeric_qf(void)
{
    matrix_t *A = mat_from_string("[[1 2][3 4]]", NULL, NULL);
    qfloat_t x = QF_ZERO;

    check_bool("mat_from_string qfloat matrix non-null", A != NULL);
    check_bool("mat_from_string qfloat matrix type", A && mat_typeof(A) == MAT_TYPE_QFLOAT);
    check_bool("mat_from_string qfloat rows", A && mat_get_row_count(A) == 2);
    check_bool("mat_from_string qfloat cols", A && mat_get_col_count(A) == 2);
    if (A) {
        mat_get(A, 1, 0, &x);
        check_qf_val("mat_from_string qfloat A[1,0]", x, qf_from_double(3.0), 1e-18);
    }

    mat_free(A);
}

static void test_mat_from_string_numeric_qc(void)
{
    matrix_t *A = mat_from_string("[[(1,2) 3i-1][4 (5,-6)][3 2j+4]]", NULL, NULL);
    qcomplex_t z = QC_ZERO;

    check_bool("mat_from_string qcomplex matrix non-null", A != NULL);
    check_bool("mat_from_string qcomplex matrix type", A && mat_typeof(A) == MAT_TYPE_QCOMPLEX);
    if (A) {
        mat_get(A, 0, 0, &z);
        check_qc_val("mat_from_string qcomplex A[0,0]",
                     z, qc_make(qf_from_double(1.0), qf_from_double(2.0)), 1e-18);
        mat_get(A, 1, 1, &z);
        check_qc_val("mat_from_string qcomplex A[1,1]",
                     z, qc_make(qf_from_double(5.0), qf_from_double(-6.0)), 1e-18);
        mat_get(A, 2, 1, &z);
        check_qc_val("mat_from_string qcomplex A[2,1]",
                     z, qc_make(qf_from_double(4.0), qf_from_double(2.0)), 1e-18);
    }

    mat_free(A);
}

static void test_mat_from_string_symbolic_wrapped(void)
{
    binding_t *bindings = NULL;
    size_t nbindings = 0;
    matrix_t *A = mat_from_string("{ [[x 1][1 c1]] | x = 2; c1 = 3 }",
                                  &bindings, &nbindings);
    dval_t *dv = NULL;
    binding_t *x_binding;
    binding_t *c_binding;

    check_bool("mat_from_string wrapped symbolic matrix non-null", A != NULL);
    check_bool("mat_from_string wrapped symbolic matrix type", A && mat_typeof(A) == MAT_TYPE_DVAL);
    check_bool("mat_from_string wrapped bindings count", nbindings == 2);

    x_binding = find_binding(bindings, nbindings, "x");
    c_binding = find_binding(bindings, nbindings, "c₁");
    check_bool("wrapped symbolic binding x present", x_binding != NULL);
    check_bool("wrapped symbolic binding c₁ present", c_binding != NULL);

    if (A) {
        mat_get(A, 1, 1, &dv);
        check_qf_val("wrapped symbolic A[1,1] initial",
                     dv_eval_qf(dv), qf_from_double(3.0), 1e-18);
        if (c_binding)
            dv_set_val_qf(c_binding->symbol, qf_from_double(5.0));
        check_qf_val("wrapped symbolic A[1,1] tracks binding update",
                     dv_eval_qf(dv), qf_from_double(5.0), 1e-18);
    }

    free(bindings);
    mat_free(A);
}

static void test_mat_from_string_symbolic_bare(void)
{
    binding_t *bindings = NULL;
    size_t nbindings = 0;
    matrix_t *A = mat_from_string("[[c1 c2*y c2*x][x y z][a b c]]",
                                  &bindings, &nbindings);
    dval_t *dv = NULL;
    binding_t *x_binding;
    binding_t *y_binding;
    binding_t *c2_binding;

    check_bool("mat_from_string bare symbolic matrix non-null", A != NULL);
    check_bool("mat_from_string bare symbolic matrix type", A && mat_typeof(A) == MAT_TYPE_DVAL);
    check_bool("mat_from_string bare bindings returned", bindings != NULL);
    check_bool("mat_from_string bare bindings count", nbindings == 8);

    x_binding = find_binding(bindings, nbindings, "x");
    y_binding = find_binding(bindings, nbindings, "y");
    c2_binding = find_binding(bindings, nbindings, "c₂");
    check_bool("bare symbolic x binding present", x_binding != NULL);
    check_bool("bare symbolic y binding present", y_binding != NULL);
    check_bool("bare symbolic c₂ binding present", c2_binding != NULL);
    check_bool("bare symbolic c₂ recognised as constant",
               c2_binding && c2_binding->is_constant);

    if (x_binding)
        dv_set_val_qf(x_binding->symbol, qf_from_double(2.0));
    if (y_binding)
        dv_set_val_qf(y_binding->symbol, qf_from_double(3.0));
    if (c2_binding)
        dv_set_val_qf(c2_binding->symbol, qf_from_double(5.0));

    if (A) {
        mat_get(A, 0, 1, &dv);
        check_qf_val("bare symbolic c₂*y",
                     dv_eval_qf(dv), qf_from_double(15.0), 1e-18);
        mat_get(A, 0, 2, &dv);
        check_qf_val("bare symbolic c₂*x",
                     dv_eval_qf(dv), qf_from_double(10.0), 1e-18);
    }

    free(bindings);
    mat_free(A);
}

void run_matrix_fromstring_tests(void)
{
    test_mat_from_string_numeric_qf();
    test_mat_from_string_numeric_qc();
    test_mat_from_string_symbolic_wrapped();
    test_mat_from_string_symbolic_bare();
}
