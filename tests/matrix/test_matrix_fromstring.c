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

static void test_mat_from_string_with_bindings_shared_symbols(void)
{
    binding_t *lhs_bindings = NULL;
    binding_t *rhs_bindings = NULL;
    size_t nlhs = 0;
    size_t nrhs = 0;
    matrix_t *A = mat_from_string("(x, y)", &lhs_bindings, &nlhs);
    matrix_t *B = mat_from_string_with_bindings("(x, z)", lhs_bindings, nlhs,
                                                &rhs_bindings, &nrhs);
    binding_t *lhs_x = NULL;
    binding_t *rhs_x = NULL;
    binding_t *rhs_z = NULL;
    dval_t *dv = NULL;

    check_bool("mat_from_string_with_bindings source matrix non-null", A != NULL);
    check_bool("mat_from_string_with_bindings shared matrix non-null", B != NULL);

    lhs_x = mat_binding_find(lhs_bindings, nlhs, "x");
    rhs_x = mat_binding_find(rhs_bindings, nrhs, "x");
    rhs_z = mat_binding_find(rhs_bindings, nrhs, "z");

    check_bool("mat_from_string_with_bindings shared x present", lhs_x && rhs_x);
    check_bool("mat_from_string_with_bindings new z present", rhs_z != NULL);
    check_bool("mat_from_string_with_bindings reuses x symbol",
               lhs_x && rhs_x && lhs_x->symbol == rhs_x->symbol);
    check_bool("mat_from_string_with_bindings returns only referenced bindings",
               nrhs == 2);

    check_bool("mat_from_string_with_bindings set shared x via rhs bindings",
               mat_binding_set_d(rhs_bindings, nrhs, "x", 4.0) == 0);
    check_bool("mat_from_string_with_bindings set new z via rhs bindings",
               mat_binding_set_d(rhs_bindings, nrhs, "z", 9.0) == 0);

    if (A) {
        mat_get(A, 0, 0, &dv);
        check_qf_val("mat_from_string_with_bindings shared x updates source matrix",
                     dv_eval_qf(dv), qf_from_double(4.0), 1e-18);
    }
    if (B) {
        mat_get(B, 0, 0, &dv);
        check_qf_val("mat_from_string_with_bindings shared x updates target matrix",
                     dv_eval_qf(dv), qf_from_double(4.0), 1e-18);
        mat_get(B, 0, 1, &dv);
        check_qf_val("mat_from_string_with_bindings new z evaluates",
                     dv_eval_qf(dv), qf_from_double(9.0), 1e-18);
    }

    free(rhs_bindings);
    mat_free(B);
    free(lhs_bindings);
    mat_free(A);
}

static void test_mat_symbolic_derivative_helpers_by_name(void)
{
    binding_t *bindings = NULL;
    size_t nbindings = 0;
    matrix_t *A = mat_from_string("([radius], x*y; y, c1)", &bindings, &nbindings);
    matrix_t *Dr = NULL;
    dval_t *dtr = NULL;
    dval_t *ddet = NULL;
    dval_t *dv = NULL;

    check_bool("mat symbolic helpers source non-null", A != NULL);
    check_bool("mat symbolic helpers set x",
               mat_binding_set_d(bindings, nbindings, "x", 2.0) == 0);
    check_bool("mat symbolic helpers set y",
               mat_binding_set_d(bindings, nbindings, "y", 3.0) == 0);
    check_bool("mat symbolic helpers set [radius]",
               mat_binding_set_d(bindings, nbindings, "[radius]", 5.0) == 0);
    check_bool("mat symbolic helpers set c₁",
               mat_binding_set_d(bindings, nbindings, "c₁", 7.0) == 0);

    Dr = mat_deriv_by_name(A, bindings, nbindings, "[radius]");
    dtr = mat_deriv_trace_by_name(A, bindings, nbindings, "[radius]");
    ddet = mat_deriv_det_by_name(A, bindings, nbindings, "[radius]");

    check_bool("mat_deriv_by_name([radius]) not NULL", Dr != NULL);
    check_bool("mat_deriv_trace_by_name([radius]) not NULL", dtr != NULL);
    check_bool("mat_deriv_det_by_name([radius]) not NULL", ddet != NULL);
    check_bool("mat_deriv_by_name missing symbol returns NULL",
               mat_deriv_by_name(A, bindings, nbindings, "missing") == NULL);

    if (Dr) {
        mat_get(Dr, 0, 0, &dv);
        check_qf_val("mat_deriv_by_name [0,0] = 1",
                     dv_eval_qf(dv), qf_from_double(1.0), 1e-18);
        mat_get(Dr, 0, 1, &dv);
        check_qf_val("mat_deriv_by_name [0,1] = 0",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
        mat_get(Dr, 1, 0, &dv);
        check_qf_val("mat_deriv_by_name [1,0] = 0",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
        mat_get(Dr, 1, 1, &dv);
        check_qf_val("mat_deriv_by_name [1,1] = 0",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
    }

    if (dtr)
        check_qf_val("mat_deriv_trace_by_name([radius]) = 1",
                     dv_eval_qf(dtr), qf_from_double(1.0), 1e-18);
    if (ddet)
        check_qf_val("mat_deriv_det_by_name([radius]) = c₁",
                     dv_eval_qf(ddet), qf_from_double(7.0), 1e-18);

    dv_free(ddet);
    dv_free(dtr);
    mat_free(Dr);
    free(bindings);
    mat_free(A);
}

static void test_mat_symbolic_jacobian_helper_by_names(void)
{
    binding_t *bindings = NULL;
    size_t nbindings = 0;
    matrix_t *A = mat_from_string("(x, x*y)", &bindings, &nbindings);
    const char *names[2] = {"x", "y"};
    matrix_t *J = NULL;
    dval_t *dv = NULL;

    check_bool("mat symbolic Jacobian helper source non-null", A != NULL);
    check_bool("mat symbolic Jacobian helper set x",
               mat_binding_set_d(bindings, nbindings, "x", 2.0) == 0);
    check_bool("mat symbolic Jacobian helper set y",
               mat_binding_set_d(bindings, nbindings, "y", 3.0) == 0);

    J = mat_jacobian_by_names(A, bindings, nbindings, names, 2);
    check_bool("mat_jacobian_by_names not NULL", J != NULL);
    check_bool("mat_jacobian_by_names rows", J && mat_get_row_count(J) == 2);
    check_bool("mat_jacobian_by_names cols", J && mat_get_col_count(J) == 2);
    check_bool("mat_jacobian_by_names missing symbol returns NULL",
               mat_jacobian_by_names(A, bindings, nbindings,
                                     (const char *const[]){"x", "missing"}, 2) == NULL);

    if (J) {
        mat_get(J, 0, 0, &dv);
        check_qf_val("mat_jacobian_by_names [0,0] = 1",
                     dv_eval_qf(dv), qf_from_double(1.0), 1e-18);
        mat_get(J, 0, 1, &dv);
        check_qf_val("mat_jacobian_by_names [0,1] = 0",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
        mat_get(J, 1, 0, &dv);
        check_qf_val("mat_jacobian_by_names [1,0] = y",
                     dv_eval_qf(dv), qf_from_double(3.0), 1e-18);
        mat_get(J, 1, 1, &dv);
        check_qf_val("mat_jacobian_by_names [1,1] = x",
                     dv_eval_qf(dv), qf_from_double(2.0), 1e-18);
    }

    mat_free(J);
    free(bindings);
    mat_free(A);
}

static void test_mat_symbolic_matrix_calculus_helpers_by_name(void)
{
    binding_t *bindings = NULL;
    binding_t *rhs_bindings = NULL;
    size_t nbindings = 0;
    size_t nrhs = 0;
    matrix_t *A = mat_from_string("(x, 1; y, 2)", &bindings, &nbindings);
    binding_t *x_binding = NULL;
    matrix_t *B = mat_from_string_with_bindings("(x; y)", bindings, nbindings,
                                                &rhs_bindings, &nrhs);
    matrix_t *dAi = NULL;
    matrix_t *dAbi = NULL;
    matrix_t *dX = NULL;
    matrix_t *dXb = NULL;
    dval_t *dv = NULL;

    check_bool("mat symbolic calculus by-name source A non-null", A != NULL);
    x_binding = mat_binding_find(bindings, nbindings, "x");
    check_bool("mat symbolic calculus by-name shared x binding present", x_binding != NULL);
    check_bool("mat symbolic calculus by-name source B non-null", B != NULL);
    check_bool("mat symbolic calculus by-name B reuses x symbol",
               x_binding && mat_binding_find(rhs_bindings, nrhs, "x")
               && x_binding->symbol == mat_binding_find(rhs_bindings, nrhs, "x")->symbol);

    check_bool("mat symbolic calculus by-name set A x",
               mat_binding_set_d(bindings, nbindings, "x", 2.0) == 0);
    check_bool("mat symbolic calculus by-name set A y",
               mat_binding_set_d(bindings, nbindings, "y", 3.0) == 0);

    dAi = mat_deriv_inverse_by_name(A, bindings, nbindings, "x");
    dAbi = mat_deriv_block_inverse_by_name(A, 1, bindings, nbindings, "x");
    dX = mat_deriv_solve_by_name(A, B, bindings, nbindings, "x");
    dXb = mat_deriv_block_solve_by_name(A, B, 1, bindings, nbindings, "x");

    check_bool("mat_deriv_inverse_by_name(x) not NULL", dAi != NULL);
    check_bool("mat_deriv_block_inverse_by_name(x) not NULL", dAbi != NULL);
    check_bool("mat_deriv_solve_by_name(x) not NULL", dX != NULL);
    check_bool("mat_deriv_block_solve_by_name(x) not NULL", dXb != NULL);
    check_bool("mat_deriv_inverse_by_name missing symbol returns NULL",
               mat_deriv_inverse_by_name(A, bindings, nbindings, "missing") == NULL);

    if (dAi) {
        mat_get(dAi, 0, 0, &dv);
        check_qf_val("mat_deriv_inverse_by_name [0,0]",
                     dv_eval_qf(dv), qf_from_double(-4.0), 1e-18);
        mat_get(dAi, 1, 0, &dv);
        check_qf_val("mat_deriv_inverse_by_name [1,0]",
                     dv_eval_qf(dv), qf_from_double(6.0), 1e-18);
    }

    if (dAbi) {
        mat_get(dAbi, 0, 1, &dv);
        check_qf_val("mat_deriv_block_inverse_by_name [0,1]",
                     dv_eval_qf(dv), qf_from_double(2.0), 1e-18);
        mat_get(dAbi, 1, 1, &dv);
        check_qf_val("mat_deriv_block_inverse_by_name [1,1]",
                     dv_eval_qf(dv), qf_from_double(-3.0), 1e-18);
    }

    if (dX) {
        mat_get(dX, 0, 0, &dv);
        check_qf_val("mat_deriv_solve_by_name [0,0]",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
        mat_get(dX, 1, 0, &dv);
        check_qf_val("mat_deriv_solve_by_name [1,0]",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
    }

    if (dXb) {
        mat_get(dXb, 0, 0, &dv);
        check_qf_val("mat_deriv_block_solve_by_name [0,0]",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
        mat_get(dXb, 1, 0, &dv);
        check_qf_val("mat_deriv_block_solve_by_name [1,0]",
                     dv_eval_qf(dv), qf_from_double(0.0), 1e-18);
    }

    mat_free(dXb);
    mat_free(dX);
    mat_free(dAbi);
    mat_free(dAi);
    free(rhs_bindings);
    mat_free(B);
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
    test_mat_from_string_with_bindings_shared_symbols();
    test_mat_symbolic_derivative_helpers_by_name();
    test_mat_symbolic_jacobian_helper_by_names();
    test_mat_symbolic_matrix_calculus_helpers_by_name();
    test_mat_from_string_invalid_syntax();
}
