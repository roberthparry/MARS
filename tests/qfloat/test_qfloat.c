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

    printf(C_YELLOW "\nRunning README examples...\n" C_RESET);
    RUN_TEST(test_readme_examples, NULL);

    printf("\n" C_YELLOW "Done.\n" C_RESET);

    return tests_failed;
}
