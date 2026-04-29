#include <stdint.h>

#include "test_matrix.h"

static void test_mat_from_string_numeric_qf(void)
{
    matrix_t *A = mat_from_string("(1, 2; 3, 4)", NULL, NULL);
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
    matrix_t *A = mat_from_string("((1,2), 3i-1; 4, (5,-6); 3, 2j+4)", NULL, NULL);
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
    matrix_t *A = mat_from_string("{ (x, 1; 1, c1) | x = 2; c1 = 3 }",
                                  &bindings, &nbindings);
    dval_t *dv = NULL;
    binding_t *x_binding;
    binding_t *c_binding;

    check_bool("mat_from_string wrapped symbolic matrix non-null", A != NULL);
    check_bool("mat_from_string wrapped symbolic matrix type", A && mat_typeof(A) == MAT_TYPE_DVAL);
    check_bool("mat_from_string wrapped bindings count", nbindings == 2);

    x_binding = mat_binding_find(bindings, nbindings, "x");
    c_binding = mat_binding_find(bindings, nbindings, "c₁");
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
    matrix_t *A = mat_from_string("(c1, c2*y, c2*x; x, y, z; a, b, c)",
                                  &bindings, &nbindings);
    dval_t *dv = NULL;
    binding_t *x_binding;
    binding_t *y_binding;
    binding_t *c2_binding;
    qfloat_t x_initial = QF_ZERO;
    qfloat_t c2_initial = QF_ZERO;

    check_bool("mat_from_string bare symbolic matrix non-null", A != NULL);
    check_bool("mat_from_string bare symbolic matrix type", A && mat_typeof(A) == MAT_TYPE_DVAL);
    check_bool("mat_from_string bare bindings returned", bindings != NULL);
    check_bool("mat_from_string bare bindings count", nbindings == 8);

    x_binding = mat_binding_find(bindings, nbindings, "x");
    y_binding = mat_binding_find(bindings, nbindings, "y");
    c2_binding = mat_binding_find(bindings, nbindings, "c₂");
    check_bool("bare symbolic x binding present", x_binding != NULL);
    check_bool("bare symbolic y binding present", y_binding != NULL);
    check_bool("bare symbolic c₂ binding present", c2_binding != NULL);
    check_bool("bare symbolic x recognised as variable",
               x_binding && !x_binding->is_constant);
    check_bool("bare symbolic c₂ recognised as constant",
               c2_binding && c2_binding->is_constant);

    if (x_binding)
        x_initial = dv_eval_qf(x_binding->symbol);
    if (c2_binding)
        c2_initial = dv_eval_qf(c2_binding->symbol);
    check_bool("bare symbolic x starts as NaN",
               x_binding && qf_isnan(x_initial));
    check_bool("bare symbolic c₂ starts as NaN",
               c2_binding && qf_isnan(c2_initial));

    check_bool("bare symbolic set x binding",
               mat_binding_set_qf(bindings, nbindings, "x", qf_from_double(2.0)) == 0);
    check_bool("bare symbolic set y binding",
               mat_binding_set_qf(bindings, nbindings, "y", qf_from_double(3.0)) == 0);
    check_bool("bare symbolic set c₂ binding",
               mat_binding_set_qf(bindings, nbindings, "c₂", qf_from_double(5.0)) == 0);

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

static void test_mat_from_string_symbolic_at_aliases(void)
{
    binding_t *bindings = NULL;
    size_t nbindings = 0;
    matrix_t *A = mat_from_string("(@DELTA, @OMEGA; @OMEGA, -@DELTA)",
                                  &bindings, &nbindings);
    dval_t *dv = NULL;

    check_bool("mat_from_string @alias symbolic matrix non-null", A != NULL);
    check_bool("mat_from_string @alias symbolic matrix type", A && mat_typeof(A) == MAT_TYPE_DVAL);
    check_bool("mat_from_string @alias Δ binding present",
               mat_binding_get(bindings, nbindings, "Δ") != NULL);
    check_bool("mat_from_string @alias Ω binding present",
               mat_binding_get(bindings, nbindings, "Ω") != NULL);
    check_bool("mat_from_string @alias @DELTA binding present",
               mat_binding_get(bindings, nbindings, "@DELTA") != NULL);
    check_bool("mat_from_string @alias @OMEGA binding present",
               mat_binding_get(bindings, nbindings, "@OMEGA") != NULL);
    check_bool("mat_from_string @alias set @DELTA",
               mat_binding_set_d(bindings, nbindings, "@DELTA", 2.0) == 0);
    check_bool("mat_from_string @alias set @OMEGA",
               mat_binding_set_d(bindings, nbindings, "@OMEGA", 3.0) == 0);

    if (A) {
        mat_get(A, 0, 0, &dv);
        check_qf_val("@alias symbolic Δ entry",
                     dv_eval_qf(dv), qf_from_double(2.0), 1e-18);
        mat_get(A, 0, 1, &dv);
        check_qf_val("@alias symbolic Ω entry",
                     dv_eval_qf(dv), qf_from_double(3.0), 1e-18);
        mat_get(A, 1, 1, &dv);
        check_qf_val("@alias symbolic -Δ entry",
                     dv_eval_qf(dv), qf_from_double(-2.0), 1e-18);
    }

    free(bindings);
    mat_free(A);
}

static void test_mat_from_string_symbolic_math_conventions(void)
{
    binding_t *bindings = NULL;
    size_t nbindings = 0;
    matrix_t *A = mat_from_string("(x, e; π, τ; [radius], c1; a, d_2)",
                                  &bindings, &nbindings);
    binding_t *x_binding;
    binding_t *e_binding;
    binding_t *pi_binding;
    binding_t *tau_binding;
    binding_t *radius_binding;
    binding_t *c1_binding;
    binding_t *a_binding;
    binding_t *d2_binding;

    check_bool("mat_from_string math-convention symbolic matrix non-null", A != NULL);
    check_bool("mat_from_string math-convention symbolic matrix type",
               A && mat_typeof(A) == MAT_TYPE_DVAL);

    x_binding = mat_binding_find(bindings, nbindings, "x");
    e_binding = mat_binding_find(bindings, nbindings, "e");
    pi_binding = mat_binding_find(bindings, nbindings, "π");
    tau_binding = mat_binding_find(bindings, nbindings, "τ");
    radius_binding = mat_binding_find(bindings, nbindings, "radius");
    c1_binding = mat_binding_find(bindings, nbindings, "c₁");
    a_binding = mat_binding_find(bindings, nbindings, "a");
    d2_binding = mat_binding_find(bindings, nbindings, "d₂");

    check_bool("math-convention x binding present", x_binding != NULL);
    check_bool("math-convention e binding present", e_binding != NULL);
    check_bool("math-convention π binding present", pi_binding != NULL);
    check_bool("math-convention τ binding present", tau_binding != NULL);
    check_bool("math-convention radius binding present", radius_binding != NULL);
    check_bool("math-convention c₁ binding present", c1_binding != NULL);
    check_bool("math-convention a binding present", a_binding != NULL);
    check_bool("math-convention d₂ binding present", d2_binding != NULL);

    check_bool("math-convention x inferred variable",
               x_binding && !x_binding->is_constant);
    check_bool("math-convention e inferred variable",
               e_binding && !e_binding->is_constant);
    check_bool("math-convention π inferred variable",
               pi_binding && !pi_binding->is_constant);
    check_bool("math-convention τ inferred variable",
               tau_binding && !tau_binding->is_constant);
    check_bool("math-convention radius inferred variable",
               radius_binding && !radius_binding->is_constant);
    check_bool("math-convention c₁ inferred constant",
               c1_binding && c1_binding->is_constant);
    check_bool("math-convention a inferred constant",
               a_binding && a_binding->is_constant);
    check_bool("math-convention d₂ inferred constant",
               d2_binding && d2_binding->is_constant);

    free(bindings);
    mat_free(A);
}

static void test_mat_from_string_invalid_syntax(void)
{
    binding_t *bindings = (binding_t *)(uintptr_t)1;
    size_t nbindings = 123;
    matrix_t *A;

    A = mat_from_string("(1, 2; 3)", &bindings, &nbindings);
    check_bool("mat_from_string rejects ragged matrix", A == NULL);
    check_bool("mat_from_string ragged clears bindings", bindings == NULL);
    check_bool("mat_from_string ragged clears count", nbindings == 0);

    bindings = (binding_t *)(uintptr_t)1;
    nbindings = 123;
    A = mat_from_string("(1, 2; 3, 4", &bindings, &nbindings);
    check_bool("mat_from_string rejects missing closing paren", A == NULL);
    check_bool("mat_from_string missing paren clears bindings", bindings == NULL);
    check_bool("mat_from_string missing paren clears count", nbindings == 0);

    bindings = (binding_t *)(uintptr_t)1;
    nbindings = 123;
    A = mat_from_string("{ (x, 1; 1, y) | x = }", &bindings, &nbindings);
    check_bool("mat_from_string rejects invalid binding syntax", A == NULL);
    check_bool("mat_from_string invalid binding clears bindings", bindings == NULL);
    check_bool("mat_from_string invalid binding clears count", nbindings == 0);

    bindings = (binding_t *)(uintptr_t)1;
    nbindings = 123;
    A = mat_from_string("(Δ, Ω; Ω, -)", &bindings, &nbindings);
    check_bool("mat_from_string rejects invalid symbolic expression", A == NULL);
    check_bool("mat_from_string invalid symbolic clears bindings", bindings == NULL);
    check_bool("mat_from_string invalid symbolic clears count", nbindings == 0);
}

static void test_mat_from_string_bracketed_names(void)
{
    binding_t *bindings = NULL;
    size_t nbindings = 0;
    matrix_t *A = mat_from_string("{ ([radius], [scale]*x; y, [offset]) | x = 2, y = 5; [radius] = 3, [scale] = 4, [offset] = 7 }",
                                  &bindings, &nbindings);
    dval_t *dv = NULL;

    check_bool("mat_from_string bracketed symbolic matrix non-null", A != NULL);
    check_bool("mat_from_string bracketed symbolic matrix type", A && mat_typeof(A) == MAT_TYPE_DVAL);
    check_bool("mat_from_string bracketed binding radius present",
               mat_binding_find(bindings, nbindings, "[radius]") != NULL);
    check_bool("mat_from_string bracketed binding scale present",
               mat_binding_find(bindings, nbindings, "scale") != NULL);
    check_bool("mat_from_string bracketed binding offset present",
               mat_binding_find(bindings, nbindings, "[offset]") != NULL);

    if (A) {
        mat_get(A, 0, 0, &dv);
        check_qf_val("bracketed symbolic [radius]",
                     dv_eval_qf(dv), qf_from_double(3.0), 1e-18);
        mat_get(A, 0, 1, &dv);
        check_qf_val("bracketed symbolic [scale]*x",
                     dv_eval_qf(dv), qf_from_double(8.0), 1e-18);
        mat_get(A, 1, 1, &dv);
        check_qf_val("bracketed symbolic [offset]",
                     dv_eval_qf(dv), qf_from_double(7.0), 1e-18);
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
    test_mat_from_string_symbolic_at_aliases();
    test_mat_from_string_symbolic_math_conventions();
    test_mat_from_string_bracketed_names();
    test_mat_from_string_invalid_syntax();
}
