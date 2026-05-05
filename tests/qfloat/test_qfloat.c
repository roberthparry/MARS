#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#define TEST_CONFIG_MAIN
#include "test_qfloat.h"

/* Helper to print qfloat_t */
void print_q(const char *label, qfloat_t x) {
    char buf[256];
    qf_to_string(x, buf, sizeof(buf));
    printf("%s = %s\n", label, buf);
}

/* Compare qfloat_t to double with tolerance */
int approx_equal(qfloat_t a, double b, double tol) {
    double diff = fabs(qf_to_double(a) - b);
    return diff < tol;
}

int qf_close(qfloat_t a, qfloat_t b, double rel)
{
    return qf_abs(qf_sub(a, b)).hi <= rel;
}

int qf_close_rel(qfloat_t a, qfloat_t b, double rel)
{
    return qf_abs(qf_sub(qf_div(a,b), (qfloat_t){1,0})).hi <= rel;
}

static void check_bool(const char *label, int cond)
{
    if (!cond)
        tests_failed++;
    printf(cond ? C_GREEN "  OK: %s\n" C_RESET
                : C_RED   "  FAIL: %s\n" C_RESET, label);
}

void test_difficult_qfloat_cases(void)
{
    qfloat_t x = qf_from_string("2.3");
    qfloat_t lhs = qf_lgamma(qf_from_string("3.3"));
    qfloat_t rhs = qf_lgamma(x);
    qfloat_t tmp = qf_log(x);
    qfloat_t one = qf_from_double(1.0);
    qfloat_t y = qf_from_string("1e-20");
    qfloat_t s = qf_from_double(0.5);
    qfloat_t gx = qf_from_double(1.0);
    qfloat_t w = qf_productlog(qf_from_string("-0.35"));
    qfloat_t a = qf_from_string("2.5");
    qfloat_t b = qf_from_string("3.5");
    qfloat_t logb;
    qfloat_t beta;
    qfloat_t ident;

    printf(C_CYAN "TEST: difficult qfloat cases\n" C_RESET);

    lhs = qf_sub(qf_sub(lhs, rhs), tmp);
    print_q("    lgamma(3.3) - lgamma(2.3) - log(2.3)", lhs);
    check_bool("lgamma(3.3) - lgamma(2.3) - log(2.3) = 0",
               qf_abs(lhs).hi < 1e-28);

    y = qf_exp(qf_log(y));
    print_q("    exp(log(1e-20))", y);
    check_bool("exp(log(1e-20)) = 1e-20",
               qf_close_rel(y, qf_from_string("1e-20"), 1e-28));

    ident = qf_add(qf_gammainc_P(s, gx), qf_gammainc_Q(s, gx));
    print_q("    gammainc_P(0.5,1) + gammainc_Q(0.5,1)", ident);
    check_bool("gammainc_P(0.5,1) + gammainc_Q(0.5,1) = 1",
               qf_close(ident, one, 1e-28));

    ident = qf_sub(qf_mul(w, qf_exp(w)), qf_from_string("-0.35"));
    print_q("    productlog(-0.35) * exp(productlog(-0.35)) - (-0.35)", ident);
    check_bool("productlog(-0.35) * exp(productlog(-0.35)) = -0.35",
               qf_abs(ident).hi < 1e-28);

    logb = qf_logbeta(a, b);
    beta = qf_beta(a, b);
    ident = qf_sub(qf_exp(logb), beta);
    print_q("    exp(logbeta(2.5,3.5)) - beta(2.5,3.5)", ident);
    check_bool("exp(logbeta(2.5,3.5)) = beta(2.5,3.5)",
               qf_abs(ident).hi < 1e-28);
}

int tests_main() {
    // qfloat_t x = qf_from_string("1.7724538509055160272981674833411451827975494561223871282138");
    // char* s = "QF_SQRT_PI";
    // printf("const qfloat_t %s = {\n    %.17g,\n    %.17g\n};\n", s, x.hi, x.lo);
    // printf("extern const qfloat_t %s;\n", s);
    // exit(0);

    printf(C_YELLOW "Running qfloat_t tests...\n\n" C_RESET);

    RUN_TEST(test_arithmetic, NULL);
    RUN_TEST(test_arithmetic_extensions, NULL);
    RUN_TEST(test_strings, NULL);
    RUN_TEST(test_printf, NULL);
    RUN_TEST(test_vsprintf, NULL);
    RUN_TEST(test_power, NULL);
    RUN_TEST(test_trigonometric, NULL);
    RUN_TEST(test_hyperbolic, NULL);
    RUN_TEST(test_hypotenus, NULL);
    RUN_TEST(test_gamma_erf_erfc_erfinv_erfcinv_digamma, NULL);
    RUN_TEST(test_lambert_w, NULL);
    RUN_TEST(test_beta_logbeta_binomial_beta_pdf_logbeta_pdf_normal_pdf_cdf_logpdf, NULL);
    RUN_TEST(test_gammainc_ei_e1, NULL);
    RUN_TEST(test_difficult_qfloat_cases, NULL);

    printf(C_YELLOW "\nRunning README examples...\n" C_RESET);
    RUN_TEST(test_readme_examples, NULL);

    printf("\n" C_YELLOW "Done.\n" C_RESET);

    return tests_failed;
}
