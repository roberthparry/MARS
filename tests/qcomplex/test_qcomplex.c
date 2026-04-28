#include <stdio.h>
#include <math.h>
#include <string.h>

#include "qcomplex.h"
#include "qfloat.h"

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#define TEST_CONFIG_MAIN
#include "test_harness.h"

/* ------------------------------------------------------------------ helpers */

static void print_qc(const char *label, qcomplex_t z)
{
    char buf[256];
    qc_to_string(z, buf, sizeof(buf));
    printf("    %s = %s\n", label, buf);
    fflush(stdout);
}

static void check_qc(const char *label, qcomplex_t got, qcomplex_t expected, double tol)
{
    double err = qf_to_double(qc_abs(qc_sub(got, expected)));
    int ok = err < tol;
    tests_run++;
    if (!ok) tests_failed++;
    printf(ok ? C_GREEN "  OK: %s\n" C_RESET : C_RED "  FAIL: %s\n" C_RESET, label);
    print_qc("got     ", got);
    print_qc("expected", expected);
    printf("    error    = %g\n", err);
}

static void check_qc_rel(const char *label, qcomplex_t got, qcomplex_t expected, double rel)
{
    qcomplex_t one = qc_make(qf_from_double(1.0), qf_from_double(0.0));
    double err = qf_to_double(qc_abs(qc_sub(qc_div(got, expected), one)));
    int ok = err < rel;
    tests_run++;
    if (!ok) tests_failed++;
    printf(ok ? C_GREEN "  OK: %s\n" C_RESET : C_RED "  FAIL: %s\n" C_RESET, label);
    print_qc("got     ", got);
    print_qc("expected", expected);
    printf("    rel-err  = %g\n", err);
}

static void check_qf(const char *label, qfloat_t got, qfloat_t expected, double tol)
{
    double err = qf_abs(qf_sub(got, expected)).hi;
    int ok = err < tol;
    char gs[128], es[128];
    qf_to_string(got, gs, sizeof(gs));
    qf_to_string(expected, es, sizeof(es));
    tests_run++;
    if (!ok) tests_failed++;
    printf(ok ? C_GREEN "  OK: %s\n" C_RESET : C_RED "  FAIL: %s\n" C_RESET, label);
    printf("    got      = %s\n", gs);
    printf("    expected = %s\n", es);
    printf("    error    = %g\n", err);
}

static void check_bool(const char *label, int cond)
{
    tests_run++;
    if (!cond) tests_failed++;
    printf(cond ? C_GREEN "  OK: %s\n" C_RESET : C_RED "  FAIL: %s\n" C_RESET, label);
}

static qcomplex_t qcr(double x)
{
    return qc_make(qf_from_double(x), qf_from_double(0.0));
}
static qcomplex_t qci(double y)
{
    return qc_make(qf_from_double(0.0), qf_from_double(y));
}
static qcomplex_t qcz(double x, double y)
{
    return qc_make(qf_from_double(x), qf_from_double(y));
}
static qcomplex_t qcrf(qfloat_t x)
{
    return qc_make(x, qf_from_double(0.0));
}
static qcomplex_t qcrs(const char *x)
{
    return qc_make(qf_from_string(x), qf_from_double(0.0));
}
static qcomplex_t qcis(const char *y)
{
    return qc_make(qf_from_double(0.0), qf_from_string(y));
}
static qcomplex_t qczs(const char *x, const char *y)
{
    return qc_make(qf_from_string(x), qf_from_string(y));
}

/* ====================================================================
   Arithmetic
   ==================================================================== */

static void test_add_sub(void)
{
    printf(C_CYAN "TEST: add/sub\n" C_RESET);

    qcomplex_t a = qcz(3.0, 4.0);
    qcomplex_t b = qcz(1.0, -2.0);

    check_qc("(3+4i)+(1-2i) = 4+2i", qc_add(a, b), qcz(4.0, 2.0), 1e-30);
    check_qc("(3+4i)-(1-2i) = 2+6i", qc_sub(a, b), qcz(2.0, 6.0), 1e-30);
    check_qc("z + (-z) = 0", qc_add(a, qc_neg(a)), qcr(0.0), 1e-30);
}

static void test_mul_div(void)
{
    printf(C_CYAN "TEST: mul/div\n" C_RESET);

    qcomplex_t a = qcz(3.0, 4.0);
    qcomplex_t b = qcz(1.0, -2.0);

    check_qc("(3+4i)*(1-2i) = 11-2i", qc_mul(a, b), qcz(11.0, -2.0), 1e-30);

    qcomplex_t c = qcz(1.0, 2.0);
    check_qc_rel("(3+4i)/(1+2i) = (11-2i)/5",
                 qc_div(a, c), qc_div(qcz(11.0, -2.0), qcr(5.0)), 1e-30);

    qcomplex_t z = qcz(3.0, 4.0);
    check_qc("z * conj(z) = 25 (purely real)",
             qc_mul(z, qc_conj(z)), qcr(25.0), 1e-30);

    check_qc("z * (1/z) = 1", qc_mul(z, qc_div(qcr(1.0), z)), qcr(1.0), 1e-29);

    check_qc("i² = -1", qc_mul(qci(1.0), qci(1.0)), qcr(-1.0), 1e-30);
}

static void test_conj(void)
{
    printf(C_CYAN "TEST: conj\n" C_RESET);

    qcomplex_t z  = qcz(3.0, 4.0);
    check_qc("conj(3+4i) = 3-4i", qc_conj(z), qcz(3.0, -4.0), 1e-30);

    qcomplex_t a = qcz(1.5, -2.5);
    qcomplex_t b = qcz(-0.5, 3.0);
    check_qc("conj(a+b) = conj(a)+conj(b)",
             qc_conj(qc_add(a, b)), qc_add(qc_conj(a), qc_conj(b)), 1e-30);
}

/* ====================================================================
   Magnitude and argument
   ==================================================================== */

static void test_abs_arg(void)
{
    printf(C_CYAN "TEST: abs/arg\n" C_RESET);

    check_qf("|3+4i| = 5", qc_abs(qcz(3.0, 4.0)), qf_from_double(5.0), 1e-30);
    check_qf("|1+i| = sqrt(2)", qc_abs(qcz(1.0, 1.0)), qf_sqrt(qf_from_double(2.0)), 1e-30);
    check_qf("arg(i) = π/2", qc_arg(qci(1.0)), QF_PI_2, 1e-30);
    check_qf("arg(-1) = π", qc_arg(qcr(-1.0)), QF_PI, 1e-30);
    check_qf("arg(-i) = -π/2", qc_arg(qci(-1.0)), qf_neg(QF_PI_2), 1e-30);

    qcomplex_t z = qcz(3.0, 4.0);
    check_qf("|conj(z)| = |z|", qc_abs(qc_conj(z)), qc_abs(z), 1e-30);

    qcomplex_t w = qcz(1.0, 2.0);
    check_qf("|z*w| = |z|*|w|",
             qc_abs(qc_mul(z, w)), qf_mul(qc_abs(z), qc_abs(w)), 1e-29);
}

/* ====================================================================
   exp and log
   ==================================================================== */

static void test_exp(void)
{
    printf(C_CYAN "TEST: exp\n" C_RESET);

    check_qc("exp(iπ) = -1",
             qc_exp(qcis("3.14159265358979323846264338327950288419716939937510")),
             qcr(-1.0), 1e-30);

    check_qc("exp(iπ/2) = i",
             qc_exp(qcis("1.57079632679489661923132169163975144209858469968755")),
             qci(1.0), 1e-30);

    check_qc_rel("exp(1) = e",
                 qc_exp(qcr(1.0)),
                 qcrs("2.71828182845904523536028747135266249775724709369996"), 1e-28);

    {
        qcomplex_t z = qczs("1.0", "0.78539816339744830961566084581988");
        qfloat_t e_over_sqrt2 = qf_div(
            qf_from_string("2.71828182845904523536028747135266"),
            qf_from_string("1.41421356237309504880168872420969"));
        qcomplex_t expected = qc_make(e_over_sqrt2, e_over_sqrt2);
        check_qc_rel("exp(1+iπ/4) = e/√2 * (1+i)", qc_exp(z), expected, 1e-26);
    }

    {
        qcomplex_t a = qcz(0.5, 1.0);
        qcomplex_t b = qcz(0.3, -0.7);
        check_qc_rel("exp(a+b) = exp(a)*exp(b)",
                     qc_exp(qc_add(a, b)), qc_mul(qc_exp(a), qc_exp(b)), 1e-27);
    }
}

static void test_euler_identity(void)
{
    printf(C_CYAN "TEST: README example — Euler's identity\n" C_RESET);

    qcomplex_t z = qc_make(qf_from_double(0.0), QF_PI);
    qcomplex_t r = qc_add(qc_exp(z), qc_make(qf_from_double(1.0), qf_from_double(0.0)));

    char re[128], im[128];
    qf_to_string(r.re, re, sizeof(re));
    qf_to_string(r.im, im, sizeof(im));
    printf("    exp(iπ) + 1 = %s + %si\n", re, im);

    check_qc("exp(iπ)+1 ≈ 0 (QF_PI residual < 2e-19)", r, qcr(0.0), 2e-19);
}

static void test_log(void)
{
    printf(C_CYAN "TEST: log\n" C_RESET);

    check_qc("log(-1) = iπ",
             qc_log(qcr(-1.0)),
             qcis("3.14159265358979323846264338327950288419716939937510"), 1e-30);

    check_qc("log(i) = iπ/2",
             qc_log(qci(1.0)),
             qcis("1.57079632679489661923132169163975144209858469968755"), 1e-30);

    check_qc("log(e) = 1",
             qc_log(qcrs("2.71828182845904523536028747135266249775724709369996")),
             qcr(1.0), 1e-30);

    {
        qcomplex_t z = qcz(0.7, 1.2);
        check_qc("log(exp(z)) = z", qc_log(qc_exp(z)), z, 1e-27);
    }

    {
        qcomplex_t a = qcz(1.5, 0.5);
        qcomplex_t b = qcz(0.8, 1.3);
        check_qc("log(a*b) = log(a)+log(b)",
                 qc_log(qc_mul(a, b)), qc_add(qc_log(a), qc_log(b)), 1e-27);
    }
}

static void test_pow_sqrt(void)
{
    printf(C_CYAN "TEST: pow/sqrt\n" C_RESET);

    check_qc("sqrt(-1) = i", qc_sqrt(qcr(-1.0)), qci(1.0), 1e-30);
    check_qc("sqrt(2i) = 1+i", qc_sqrt(qci(2.0)), qcz(1.0, 1.0), 1e-30);

    {
        qcomplex_t z = qcz(3.0, -5.0);
        qcomplex_t sq = qc_sqrt(z);
        check_qc("sqrt(z)² = z", qc_mul(sq, sq), z, 1e-28);
        check_qc("pow(z,2) = z*z", qc_pow(z, qcr(2.0)), qc_mul(z, z), 1e-27);
    }

    check_qc("pow(-1, 0.5) = i", qc_pow(qcr(-1.0), qcr(0.5)), qci(1.0), 1e-30);

    {
        qcomplex_t a  = qcz(1.5, 0.5);
        qcomplex_t ea = qcz(0.3, 0.1);
        qcomplex_t eb = qcz(0.4, -0.2);
        check_qc_rel("z^a * z^b = z^(a+b)",
                     qc_mul(qc_pow(a, ea), qc_pow(a, eb)),
                     qc_pow(a, qc_add(ea, eb)), 1e-26);
    }
}

/* ====================================================================
   Trigonometric
   ==================================================================== */

static void test_trig(void)
{
    printf(C_CYAN "TEST: trig (sin/cos/tan)\n" C_RESET);

    check_qc("sin(i) = i*sinh(1)",
             qc_sin(qci(1.0)),
             qcis("1.17520119364380145688238185059560081515977451151758"), 1e-29);

    check_qc("cos(i) = cosh(1)",
             qc_cos(qci(1.0)),
             qcrs("1.54308063481524377847790562075546200166693062024094"), 1e-29);

    {
        qcomplex_t z = qcz(1.0, 1.0);
        qcomplex_t s = qc_sin(z);
        qcomplex_t c = qc_cos(z);
        check_qc("sin²(1+i)+cos²(1+i) = 1",
                 qc_add(qc_mul(s, s), qc_mul(c, c)), qcr(1.0), 1e-28);
    }

    {
        qcomplex_t z2 = qcz(0.8, 0.6);
        check_qc("sin(-z) = -sin(z)", qc_sin(qc_neg(z2)), qc_neg(qc_sin(z2)), 1e-29);
    }

    {
        qcomplex_t z3 = qcz(0.5, 0.5);
        check_qc("tan(z) = sin(z)/cos(z)",
                 qc_tan(z3), qc_div(qc_sin(z3), qc_cos(z3)), 1e-29);
    }

    check_qc("atan(i/2) = i*atanh(1/2)",
             qc_atan(qcz(0.0, 0.5)),
             qcis("0.54930614433405484569762261846126285232374527891137"), 1e-29);

    {
        qcomplex_t z4 = qcz(0.6, 0.4);
        check_qc("asin(sin(z)) = z", qc_asin(qc_sin(z4)), z4, 1e-27);
    }

    {
        qcomplex_t z5 = qcz(0.4, 0.3);
        check_qc("acos(cos(z)) = z", qc_acos(qc_cos(z5)), z5, 1e-27);
    }

    check_qc("atan2(0,1) = 0",   qc_atan2(qcr(0.0), qcr( 1.0)), qcr(0.0),          1e-30);
    check_qc("atan2(1,0) = π/2", qc_atan2(qcr(1.0), qcr( 0.0)), qcrs("1.57079632679489661923132169163975144209858469968755"), 1e-30);
    check_qc("atan2(0,-1) = π",  qc_atan2(qcr(0.0), qcr(-1.0)), qcrs("3.14159265358979323846264338327950288419716939937510"), 1e-30);
    check_qc("atan2(-1,0) = -π/2", qc_atan2(qcr(-1.0), qcr(0.0)), qcrs("-1.57079632679489661923132169163975144209858469968755"), 1e-30);

    {
        qcomplex_t y6 = qcz(0.7, 0.0);
        qcomplex_t x6 = qcz(0.5, 0.0);
        check_qc_rel("atan2(y,x) = atan(y/x) for Re(x)>0",
                     qc_atan2(y6, x6), qc_atan(qc_div(y6, x6)), 1e-29);
    }
}

/* ====================================================================
   Hyperbolic
   ==================================================================== */

static void test_hyperbolic(void)
{
    printf(C_CYAN "TEST: hyperbolic (sinh/cosh/tanh)\n" C_RESET);

    check_qc("sinh(iπ/2) = i",
             qc_sinh(qcis("1.57079632679489661923132169163975144209858469968755")),
             qci(1.0), 1e-30);

    check_qc("cosh(iπ) = -1",
             qc_cosh(qcis("3.14159265358979323846264338327950288419716939937510")),
             qcr(-1.0), 1e-30);

    check_qc("tanh(iπ/4) = i",
             qc_tanh(qcis("0.78539816339744830961566084581988")),
             qci(1.0), 1e-30);

    {
        qcomplex_t z  = qcz(0.8, 0.6);
        qcomplex_t ch = qc_cosh(z);
        qcomplex_t sh = qc_sinh(z);
        check_qc("cosh²(z)-sinh²(z) = 1",
                 qc_sub(qc_mul(ch, ch), qc_mul(sh, sh)), qcr(1.0), 1e-28);
        check_qc("tanh(z) = sinh(z)/cosh(z)", qc_tanh(z), qc_div(sh, ch), 1e-29);
    }

    {
        qcomplex_t z2 = qcz(0.5, 0.7);
        check_qc("asinh(sinh(z)) = z", qc_asinh(qc_sinh(z2)), z2, 1e-27);
    }

    {
        qcomplex_t z3 = qcz(1.2, 0.3);
        check_qc("acosh(cosh(z)) = z  (Re z > 0)", qc_acosh(qc_cosh(z3)), z3, 1e-27);
    }

    {
        qcomplex_t z4 = qcz(0.3, 0.4);
        check_qc("atanh(tanh(z)) = z", qc_atanh(qc_tanh(z4)), z4, 1e-27);
    }

    check_qc("atanh(1/2) = (1/2)ln(3)",
             qc_atanh(qcr(0.5)),
             qcrs("0.54930614433405484569762261846126285232374527891137"), 1e-30);
}

/* ====================================================================
   erf family
   ==================================================================== */

static void test_erf(void)
{
    printf(C_CYAN "TEST: erf/erfc\n" C_RESET);

    check_qc("erf(0) = 0", qc_erf(qcr(0.0)), qcr(0.0), 1e-30);

    check_qc_rel("erf(1) = 0.84270...",
                 qc_erf(qcr(1.0)),
                 qcrs("0.84270079294971486934122063508260925929606699796630"), 1e-27);

    {
        qcomplex_t z = qcr(0.7);
        check_qc("erf(-z) = -erf(z)", qc_erf(qc_neg(z)), qc_neg(qc_erf(z)), 1e-28);
        check_qc("erf(z)+erfc(z) = 1",
                 qc_add(qc_erf(z), qc_erfc(z)), qcr(1.0), 1e-28);
    }

    check_qc("erfc(0) = 1", qc_erfc(qcr(0.0)), qcr(1.0), 1e-30);

    {
        /* complex path (Faddeeva approximation — ~15 sig digits) */
        qcomplex_t zc = qcz(0.5, 0.3);
        check_qc("erf(-z) = -erf(z) for complex z",
                 qc_erf(qc_neg(zc)), qc_neg(qc_erf(zc)), 1e-14);
        check_qc("erf(z)+erfc(z) = 1 for complex z",
                 qc_add(qc_erf(zc), qc_erfc(zc)), qcr(1.0), 1e-14);
    }
}

static void test_erfinv(void)
{
    printf(C_CYAN "TEST: erfinv/erfcinv\n" C_RESET);

    double xs[] = { 0.1, 0.5, 0.9, -0.3, -0.7 };
    for (int i = 0; i < 5; i++) {
        qcomplex_t z  = qcr(xs[i]);
        qcomplex_t rt = qc_erfinv(qc_erf(z));
        char label[64];
        snprintf(label, sizeof(label), "erfinv(erf(%.1f)) = %.1f", xs[i], xs[i]);
        check_qc(label, rt, z, 1e-26);
    }

    double ys[] = { 0.1, 0.5, 0.8 };
    for (int i = 0; i < 3; i++) {
        qcomplex_t z  = qcr(ys[i]);
        qcomplex_t rt = qc_erfcinv(qc_erfc(z));
        char label[64];
        snprintf(label, sizeof(label), "erfcinv(erfc(%.1f)) = %.1f", ys[i], ys[i]);
        check_qc(label, rt, z, 1e-26);
    }

    {
        qcomplex_t x = qcr(0.6);
        check_qc("erfinv(-x) = -erfinv(x)",
                 qc_erfinv(qc_neg(x)), qc_neg(qc_erfinv(x)), 1e-28);
    }
}

/* ====================================================================
   Gamma family
   ==================================================================== */

static void test_gamma(void)
{
    printf(C_CYAN "TEST: gamma/lgamma\n" C_RESET);

    check_qc("Γ(1) = 1", qc_gamma(qcr(1.0)), qcr(1.0), 1e-30);
    check_qc("Γ(2) = 1", qc_gamma(qcr(2.0)), qcr(1.0), 1e-30);
    check_qc("Γ(3) = 2", qc_gamma(qcr(3.0)), qcr(2.0), 1e-30);

    check_qc_rel("Γ(1/2) = √π",
                 qc_gamma(qcr(0.5)),
                 qcrs("1.77245385090551602729816748334114518279754945612238"), 1e-28);

    {
        qcomplex_t z = qcz(1.5, 0.7);
        check_qc_rel("Γ(z+1) = z*Γ(z) at complex z",
                     qc_gamma(qc_add(z, qcr(1.0))), qc_mul(z, qc_gamma(z)), 1e-14);
    }

    {
        qcomplex_t g1i = qc_gamma(qcz(1.0, 1.0));
        qfloat_t abs2  = qf_sqr(qc_abs(g1i));
        qfloat_t pi_over_sinh_pi = qf_div(QF_PI,
            qf_from_string("11.548739357257748337410243973894060883480498326634"));
        check_qf("|Γ(1+i)|² = π/sinh(π)", abs2, pi_over_sinh_pi, 1e-14);
    }

    {
        qcomplex_t z3 = qcr(1.0/3.0);
        qfloat_t sin_pi_3 = qf_sin(qf_div(QF_PI, qf_from_double(3.0)));
        qcomplex_t rhs3   = qcrf(qf_div(QF_PI, sin_pi_3));
        check_qc_rel("Γ(z)*Γ(1-z) = π/sin(πz) at z=1/3",
                     qc_mul(qc_gamma(z3), qc_gamma(qc_sub(qcr(1.0), z3))),
                     rhs3, 1e-14);
    }

    {
        qcomplex_t z4 = qcz(2.5, 0.8);
        check_qc_rel("exp(lgamma(z)) = gamma(z)",
                     qc_exp(qc_lgamma(z4)), qc_gamma(z4), 1e-25);
    }
}

static void test_digamma(void)
{
    printf(C_CYAN "TEST: digamma/trigamma/tetragamma\n" C_RESET);

    check_qc("ψ(1) = -γ",
             qc_digamma(qcr(1.0)),
             qcrs("-0.57721566490153286060651209008240243104215933593992"), 1e-29);

    {
        qcomplex_t z = qcz(2.0, 1.0);
        check_qc("ψ(z+1) = ψ(z) + 1/z",
                 qc_digamma(qc_add(z, qcr(1.0))),
                 qc_add(qc_digamma(z), qc_div(qcr(1.0), z)), 1e-26);
    }

    {
        qcomplex_t z2 = qcz(1.5, 0.5);
        check_qc("ψ₁(z+1) = ψ₁(z) - 1/z²",
                 qc_trigamma(qc_add(z2, qcr(1.0))),
                 qc_sub(qc_trigamma(z2), qc_div(qcr(1.0), qc_mul(z2, z2))), 1e-25);
    }

    {
        /* ψ₂(z+1) = ψ₂(z) - 2/z³  (tetragamma = -ψ^(2), recurrence subtracts) */
        qcomplex_t z3 = qcz(2.0, 0.5);
        check_qc("ψ₂(z+1) = ψ₂(z) - 2/z³",
                 qc_tetragamma(qc_add(z3, qcr(1.0))),
                 qc_sub(qc_tetragamma(z3),
                        qc_div(qcr(2.0), qc_mul(z3, qc_mul(z3, z3)))), 1e-24);
    }
}

static void test_gammainv(void)
{
    printf(C_CYAN "TEST: gammainv\n" C_RESET);

    double xs[] = { 1.0, 3.0, 4.0, 2.5 };
    for (int i = 0; i < 4; i++) {
        qcomplex_t z   = qcr(xs[i]);
        qcomplex_t got = qc_gammainv(qc_gamma(z));
        char label[64];
        snprintf(label, sizeof(label), "gammainv(Γ(%.1f)) = %.1f", xs[i], xs[i]);
        check_qc(label, got, z, 1e-26);
    }
}

/* ====================================================================
   Beta and related
   ==================================================================== */

static void test_beta(void)
{
    printf(C_CYAN "TEST: beta/logbeta/binomial\n" C_RESET);

    check_qc("B(1,1) = 1", qc_beta(qcr(1.0), qcr(1.0)), qcr(1.0), 1e-30);

    check_qc_rel("B(2,3) = 1/12",
                 qc_beta(qcr(2.0), qcr(3.0)),
                 qcrs("0.083333333333333333333333333333333"), 1e-28);

    {
        qcomplex_t a = qcz(1.5, 0.5);
        qcomplex_t b = qcz(2.0, -0.3);
        check_qc_rel("B(a,b) = B(b,a)", qc_beta(a, b), qc_beta(b, a), 1e-27);
    }

    {
        qcomplex_t a2 = qcz(1.2, 0.4);
        qcomplex_t b2 = qcz(0.8, 0.2);
        check_qc_rel("beta = exp(logbeta)",
                     qc_beta(a2, b2), qc_exp(qc_logbeta(a2, b2)), 1e-25);
    }

    check_qc("C(5,2) = 10", qc_binomial(qcr(5.0), qcr(2.0)), qcr(10.0), 1e-28);

    /* beta_pdf(x,a,b) = x^(a-1)*(1-x)^(b-1)/B(a,b) */
    check_qc("beta_pdf(0.5,1,1) = 1",
             qc_beta_pdf(qcr(0.5), qcr(1.0), qcr(1.0)), qcr(1.0), 1e-29);
    check_qc("beta_pdf(0.5,2,2) = 1.5",
             qc_beta_pdf(qcr(0.5), qcr(2.0), qcr(2.0)), qcr(1.5), 1e-29);

    {
        qcomplex_t x2 = qcr(0.3);
        qcomplex_t a2 = qcr(2.5);
        qcomplex_t b2 = qcr(1.5);
        check_qc_rel("beta_pdf = exp(logbeta_pdf)",
                     qc_beta_pdf(x2, a2, b2), qc_exp(qc_logbeta_pdf(x2, a2, b2)), 1e-28);
    }
}

/* ====================================================================
   Normal distribution
   ==================================================================== */

static void test_normal(void)
{
    printf(C_CYAN "TEST: normal (pdf/cdf/logpdf)\n" C_RESET);

    /* φ(0) = 1/√(2π) */
    check_qc_rel("normal_pdf(0) = 1/√(2π)",
                 qc_normal_pdf(qcr(0.0)),
                 qcrs("0.39894228040143267793994605993438186847585863116494"), 1e-28);

    /* symmetry */
    {
        qcomplex_t z = qcr(1.5);
        check_qc("normal_pdf(-z) = normal_pdf(z)",
                 qc_normal_pdf(qc_neg(z)), qc_normal_pdf(z), 1e-29);
    }

    /* logpdf consistency */
    {
        qcomplex_t z = qcr(0.8);
        check_qc_rel("exp(normal_logpdf(z)) = normal_pdf(z)",
                     qc_exp(qc_normal_logpdf(z)), qc_normal_pdf(z), 1e-28);
    }

    /* ln φ(0) = -½ ln(2π) */
    check_qc_rel("normal_logpdf(0) = -½ln(2π)",
                 qc_normal_logpdf(qcr(0.0)),
                 qcrs("-0.91893853320467274178032973640561763986139747363778"), 1e-28);

    /* Φ(0) = 0.5 */
    check_qc("normal_cdf(0) = 0.5", qc_normal_cdf(qcr(0.0)), qcr(0.5), 1e-29);

    /* Φ(-z) + Φ(z) = 1 */
    {
        qcomplex_t z = qcr(1.2);
        check_qc("normal_cdf(-z) + normal_cdf(z) = 1",
                 qc_add(qc_normal_cdf(qc_neg(z)), qc_normal_cdf(z)), qcr(1.0), 1e-29);
    }

    /* Φ(z) = (1 + erf(z/√2)) / 2 */
    {
        qcomplex_t z    = qcr(0.7);
        qcomplex_t sqrt2 = qcrs("1.41421356237309504880168872420969807856967187537694");
        qcomplex_t rhs  = qc_ldexp(qc_add(qcr(1.0), qc_erf(qc_div(z, sqrt2))), -1);
        check_qc_rel("normal_cdf(z) = (1+erf(z/√2))/2",
                     qc_normal_cdf(z), rhs, 1e-28);
    }
}

/* ====================================================================
   Product log (Lambert W)
   ==================================================================== */

static void test_productlog(void)
{
    printf(C_CYAN "TEST: productlog (Lambert W)\n" C_RESET);

    check_qc("W(0) = 0", qc_productlog(qcr(0.0)), qcr(0.0), 1e-30);

    check_qc("W(e) = 1",
             qc_productlog(qcrs("2.71828182845904523536028747135266249775724709369996")),
             qcr(1.0), 1e-26);

    double zs[] = { 1.0, 2.0, 5.0, 0.5, 10.0 };
    for (int i = 0; i < 5; i++) {
        qcomplex_t z = qcr(zs[i]);
        qcomplex_t w = qc_productlog(z);
        char label[64];
        snprintf(label, sizeof(label), "W(%.1f)*exp(W(%.1f)) = %.1f", zs[i], zs[i], zs[i]);
        check_qc_rel(label, qc_mul(w, qc_exp(w)), z, 1e-25);
    }

    {
        qcomplex_t zc = qcz(1.0, 1.0);
        qcomplex_t wc = qc_productlog(zc);
        check_qc_rel("W(1+i)*exp(W(1+i)) = 1+i",
                     qc_mul(wc, qc_exp(wc)), zc, 1e-24);
    }
}

static void test_lambert_wm1(void)
{
    printf(C_CYAN "TEST: lambert_wm1 (complex branch)\n" C_RESET);

    check_qc("W_-1(-0.2) matches real branch",
             qc_lambert_wm1(qcr(-0.2)),
             qcrf(qf_lambert_wm1(qf_from_double(-0.2))), 1e-28);

    {
        qcomplex_t z = qcz(-0.2, -0.1);
        qcomplex_t w = qc_lambert_wm1(z);
        qcomplex_t w0 = qc_productlog(z);

        check_qc_rel("W_-1(-0.2-0.1i) * exp(W_-1(-0.2-0.1i)) = -0.2-0.1i",
                     qc_mul(w, qc_exp(w)), z, 1e-24);
        check_bool("W_-1(-0.2-0.1i) is distinct from W0",
                   qf_gt(qc_abs(qc_sub(w, w0)), qf_from_double(1e-8)));
        check_bool("W_-1(-0.2-0.1i) has negative imaginary part",
                   qf_lt(w.im, qf_from_double(0.0)));
    }
}

/* ====================================================================
   Incomplete gamma
   ==================================================================== */

static void test_gammainc(void)
{
    printf(C_CYAN "TEST: gammainc (lower/upper/P/Q)\n" C_RESET);

    double xs[] = { 0.5, 1.0, 2.0, 5.0 };
    for (int i = 0; i < 4; i++) {
        qcomplex_t s       = qcr(1.0);
        qcomplex_t x       = qcr(xs[i]);
        qcomplex_t exp_val = qc_sub(qcr(1.0), qc_exp(qc_neg(x)));
        char label[64];
        snprintf(label, sizeof(label), "γ(1, %.1f) = 1-exp(-%.1f)", xs[i], xs[i]);
        check_qc(label, qc_gammainc_lower(s, x), exp_val, 1e-26);
    }

    {
        qcomplex_t s2 = qcr(2.0);
        qcomplex_t x2 = qcr(1.5);
        check_qc("P(s,x) + Q(s,x) = 1",
                 qc_add(qc_gammainc_P(s2, x2), qc_gammainc_Q(s2, x2)), qcr(1.0), 1e-26);
    }

    {
        qcomplex_t s3 = qcr(2.5);
        qcomplex_t x3 = qcr(1.0);
        check_qc_rel("γ(s,x) + Γ(s,x) = Γ(s)",
                     qc_add(qc_gammainc_lower(s3, x3), qc_gammainc_upper(s3, x3)),
                     qc_gamma(s3), 1e-25);
    }
}

/* ====================================================================
   Exponential integrals
   ==================================================================== */

static void test_ei_e1(void)
{
    printf(C_CYAN "TEST: ei/e1\n" C_RESET);

    check_qc_rel("Ei(1) = 1.89511...",
                 qc_ei(qcr(1.0)),
                 qcrs("1.89511781635593675546652093433163253689966848312547"), 1e-26);

    check_qc_rel("E1(1) = 0.21938...",
                 qc_e1(qcr(1.0)),
                 qcrs("0.21938393439552027367716377546049389941229571528030"), 1e-26);

    double zs[] = { 0.5, 1.0, 2.0 };
    for (int i = 0; i < 3; i++) {
        qcomplex_t z = qcr(zs[i]);
        char label[64];
        snprintf(label, sizeof(label), "E1(%.1f) = -Ei(-%.1f)", zs[i], zs[i]);
        check_qc_rel(label, qc_e1(z), qc_neg(qc_ei(qc_neg(z))), 1e-27);
    }

    check_bool("Ei monotone on positive real axis",
               qf_gt(qc_abs(qc_ei(qcr(2.0))), qc_abs(qc_ei(qcr(1.0)))));

    {
        qcomplex_t zc  = qcz(1.0, 0.5);
        qcomplex_t e1c = qc_e1(zc);
        check_bool("E1(1+0.5i) is finite", !qc_isnan(e1c) && !qc_isinf(e1c));
    }
}

/* ====================================================================
   Utility
   ==================================================================== */

static void test_utility(void)
{
    printf(C_CYAN "TEST: utility (ldexp/floor/hypot)\n" C_RESET);

    qcomplex_t z = qcz(3.0, 5.0);
    check_qc("ldexp(3+5i, 2) = 12+20i", qc_ldexp(z, 2), qcz(12.0, 20.0), 1e-30);
    check_qc("ldexp(3+5i, -1) = 1.5+2.5i", qc_ldexp(z, -1), qcz(1.5, 2.5), 1e-30);
    check_qc("floor(3.7 - 2.1i) = 3 - 3i", qc_floor(qcz(3.7, -2.1)), qcz(3.0, -3.0), 1e-30);
    check_qc("hypot(3, 4) = 5", qc_hypot(qcr(3.0), qcr(4.0)), qcr(5.0), 1e-30);

    {
        qcomplex_t x2 = qcz(1.0, 2.0);
        qcomplex_t y2 = qcz(3.0, -1.0);
        qfloat_t h2_ref = qf_sqrt(qf_add(qf_sqr(qc_abs(x2)), qf_sqr(qc_abs(y2))));
        check_qf("hypot agrees with sqrt(|x|²+|y|²)",
                 qc_abs(qc_hypot(x2, y2)), h2_ref, 1e-28);
    }
}

/* ====================================================================
   Comparison
   ==================================================================== */

static void test_comparison(void)
{
    printf(C_CYAN "TEST: comparison predicates\n" C_RESET);

    qcomplex_t a = qcz(1.0, 2.0);
    qcomplex_t b = qcz(1.0, 2.0);
    qcomplex_t c = qcz(1.0, 3.0);
    check_bool("qc_eq: equal and unequal", qc_eq(a, b) && !qc_eq(a, c));

    qcomplex_t nan_z = qc_make(qf_from_double(0.0/0.0), qf_from_double(0.0));
    check_bool("qc_isnan: NaN detected", qc_isnan(nan_z) && !qc_isnan(a));

    double inf = 1.0 / 0.0;
    qcomplex_t inf_z = qc_make(qf_from_double(inf), qf_from_double(0.0));
    check_bool("qc_isinf: inf detected", qc_isinf(inf_z) && !qc_isinf(a));

    qcomplex_t posinf = qc_make(qf_from_double( inf), qf_from_double(0.0));
    qcomplex_t neginf = qc_make(qf_from_double(-inf), qf_from_double(0.0));
    check_bool("qc_isposinf", qc_isposinf(posinf) && !qc_isposinf(neginf));
    check_bool("qc_isneginf", qc_isneginf(neginf) && !qc_isneginf(posinf));
}

/* ====================================================================
   Polar form
   ==================================================================== */

static void test_polar(void)
{
    printf(C_CYAN "TEST: qc_from_polar / qc_to_polar\n" C_RESET);

    /* from_polar then to_polar round-trips */
    {
        qfloat_t r     = qf_from_double(5.0);
        qfloat_t theta = QF_PI_2;
        qcomplex_t z   = qc_from_polar(r, theta);
        check_qc("from_polar(5, π/2) = 5i", z, qcz(0.0, 5.0), 1e-28);
    }
    {
        qfloat_t r     = qf_from_double(1.0);
        qfloat_t theta = QF_PI;
        qcomplex_t z   = qc_from_polar(r, theta);
        check_qc("from_polar(1, π) = -1", z, qcr(-1.0), 1e-28);
    }
    {
        qfloat_t r     = qf_from_double(2.0);
        qfloat_t theta = qf_from_double(0.0);
        qcomplex_t z   = qc_from_polar(r, theta);
        check_qc("from_polar(2, 0) = 2", z, qcr(2.0), 1e-30);
    }
    {
        /* Euler: e^(iπ/4) = (1+i)/√2 */
        qfloat_t r     = qf_from_double(1.0);
        qfloat_t theta = qf_ldexp(QF_PI, -2);   /* π/4 */
        qcomplex_t z   = qc_from_polar(r, theta);
        qfloat_t inv_sqrt2 = qf_div(qf_from_double(1.0), qf_sqrt(qf_from_double(2.0)));
        check_qc("from_polar(1, π/4) = (1+i)/√2", z, qc_make(inv_sqrt2, inv_sqrt2), 1e-28);
    }

    /* to_polar recovers r and theta */
    {
        qcomplex_t z = qcz(3.0, 4.0);
        qfloat_t r, theta;
        qc_to_polar(z, &r, &theta);
        check_qf("to_polar(3+4i): r = 5",   r,     qf_from_double(5.0),  1e-30);
        check_qf("to_polar(3+4i): theta = atan2(4,3)", theta, qf_atan2(qf_from_double(4.0), qf_from_double(3.0)), 1e-30);
    }
    {
        qcomplex_t z = qcr(-2.0);
        qfloat_t r, theta;
        qc_to_polar(z, &r, &theta);
        check_qf("to_polar(-2): r = 2",   r,     qf_from_double(2.0), 1e-30);
        check_qf("to_polar(-2): theta = π", theta, QF_PI,              1e-30);
    }

    /* from_polar(to_polar(z)) == z */
    {
        qcomplex_t z = qcz(1.5, -2.5);
        qfloat_t r, theta;
        qc_to_polar(z, &r, &theta);
        check_qc("round-trip: from_polar(to_polar(1.5-2.5i))", qc_from_polar(r, theta), z, 1e-28);
    }
}

/* ====================================================================
   printf (qc_sprintf / qc_vsprintf)
   ==================================================================== */

static void check_str(const char *label, const char *got, const char *expected)
{
    tests_run++;
    int ok = (strcmp(got, expected) == 0);
    if (!ok) tests_failed++;
    if (ok)
        printf(C_GREEN "  OK: %s\n" C_RESET, label);
    else
        printf(C_RED "  FAIL: %s\n    got      = \"%s\"\n    expected = \"%s\"\n" C_RESET,
               label, got, expected);
}

static void test_printf(void)
{
    char buf[512];
    qcomplex_t z35   = qcz(3.0,  5.0);
    qcomplex_t zneg  = qcz(3.0, -5.0);
    qcomplex_t pure_im = qci(2.5);
    qcomplex_t pure_re = qcr(-1.0);

    /* Basic fixed %z */
    qc_sprintf(buf, sizeof(buf), "%z", z35);
    check_str("%z basic positive im", buf, "3 + 5i");

    qc_sprintf(buf, sizeof(buf), "%z", zneg);
    check_str("%z negative im", buf, "3 - 5i");

    /* Precision */
    qc_sprintf(buf, sizeof(buf), "%.2z", z35);
    check_str("%.2z precision 2", buf, "3.00 + 5.00i");

    qc_sprintf(buf, sizeof(buf), "%.4z", qcz(1.5, -2.25));
    check_str("%.4z negative im precision 4", buf, "1.5000 - 2.2500i");

    /* Scientific %Z */
    qc_sprintf(buf, sizeof(buf), "%.2Z", z35);
    check_str("%.2Z scientific", buf, "3.00e+0 + 5.00e+0i");

    /* Width and alignment */
    qc_sprintf(buf, sizeof(buf), "%20.2z", z35);
    check_str("%20.2z right-align", buf, "        3.00 + 5.00i");

    qc_sprintf(buf, sizeof(buf), "%-20.2z|", z35);
    check_str("%-20.2z left-align", buf, "3.00 + 5.00i        |");

    /* Pure imaginary / pure real */
    qc_sprintf(buf, sizeof(buf), "%.1z", pure_im);
    check_str("%.1z pure imaginary", buf, "0.0 + 2.5i");

    qc_sprintf(buf, sizeof(buf), "%.1z", pure_re);
    check_str("%.1z pure real", buf, "-1.0 + 0.0i");

    /* %q / %Q passthrough */
    qfloat_t pi = QF_PI;
    qc_sprintf(buf, sizeof(buf), "%.5q", pi);
    char ref[64];
    qf_sprintf(ref, sizeof(ref), "%.5q", pi);
    check_str("%q delegates to qf_sprintf", buf, ref);

    qc_sprintf(buf, sizeof(buf), "%.3Q", pi);
    qf_sprintf(ref, sizeof(ref), "%.3Q", pi);
    check_str("%Q delegates to qf_sprintf", buf, ref);

    /* Standard specifiers */
    qc_sprintf(buf, sizeof(buf), "%d + %s", 42, "hello");
    check_str("%d and %s", buf, "42 + hello");

    qc_sprintf(buf, sizeof(buf), "%.2f", 3.14159);
    check_str("%.2f", buf, "3.14");

    qc_sprintf(buf, sizeof(buf), "%%");
    check_str("%%%%", buf, "%");

    /* Mixed complex and standard in one format */
    qc_sprintf(buf, sizeof(buf), "z=%.2z n=%d", z35, 7);
    check_str("mixed %%z and %%d", buf, "z=3.00 + 5.00i n=7");

    /* Return value: count of characters */
    int n = qc_sprintf(buf, sizeof(buf), "%.1z", z35);
    check_bool("qc_sprintf return value matches strlen", n == (int)strlen(buf));

    /* Dry-run (NULL buffer): count only */
     int n2 = qc_sprintf(NULL, 0, "%.1z", z35);
     check_bool("qc_sprintf dry-run count matches", n == n2);
}

/* ====================================================================
   Test groups
   ==================================================================== */

static void test_arithmetic_group(void)
{
    RUN_TEST(test_add_sub,  __func__);
    RUN_TEST(test_mul_div,  __func__);
    RUN_TEST(test_conj,     __func__);
    RUN_TEST(test_abs_arg,  __func__);
    RUN_TEST(test_polar,    __func__);
}

static void test_elementary_group(void)
{
    RUN_TEST(test_exp,      __func__);
    RUN_TEST(test_log,      __func__);
    RUN_TEST(test_pow_sqrt, __func__);
}

static void test_trig_group(void)
{
    RUN_TEST(test_trig,       __func__);
    RUN_TEST(test_hyperbolic, __func__);
}

static void test_special_group(void)
{
    RUN_TEST(test_erf,        __func__);
    RUN_TEST(test_erfinv,     __func__);
    RUN_TEST(test_gamma,      __func__);
    RUN_TEST(test_digamma,    __func__);
    RUN_TEST(test_gammainv,   __func__);
    RUN_TEST(test_beta,       __func__);
    RUN_TEST(test_normal,     __func__);
    RUN_TEST(test_productlog, __func__);
    RUN_TEST(test_lambert_wm1, __func__);
    RUN_TEST(test_gammainc,   __func__);
    RUN_TEST(test_ei_e1,      __func__);
}

static void test_util_group(void)
{
    RUN_TEST(test_utility,   __func__);
    RUN_TEST(test_comparison, __func__);
    RUN_TEST(test_printf,    __func__);
}

/* ====================================================================
   qc_from_string — full coverage tests with file/line reporting
   ==================================================================== */
static void test_from_string(void)
{
    printf(C_CYAN "TEST: qc_from_string\n" C_RESET);

    struct {
        const char *file;
        int         line;
        const char *desc;
        const char *input;
        const char *re_exp;   /* NULL => special handling (polar) */
        const char *im_exp;
        double      tol;
        int         expect_nan;
    } cases[] = {

        /* PURE REAL */
        { __FILE__, __LINE__, "pure real: integer",
          "3", "3", "0", 1e-60, 0 },

        { __FILE__, __LINE__, "pure real: negative",
          "-2.5", "-2.5", "0", 1e-60, 0 },

        { __FILE__, __LINE__, "pure real: scientific",
          "1e-30", "1e-30", "0", 1e-60, 0 },

        { __FILE__, __LINE__, "pure real: high precision",
          "3.14159265358979323846264338327950288419716939937510",
          "3.14159265358979323846264338327950288419716939937510",
          "0", 1e-60, 0 },

        /* PURE IMAGINARY */
        { __FILE__, __LINE__, "pure imaginary: i",
          "i", "0", "1", 1e-60, 0 },

        { __FILE__, __LINE__, "pure imaginary: -i",
          "-i", "0", "-1", 1e-60, 0 },

        { __FILE__, __LINE__, "pure imaginary: +i",
          "+i", "0", "1", 1e-60, 0 },

        { __FILE__, __LINE__, "pure imaginary: 5i",
          "5i", "0", "5", 1e-60, 0 },

        { __FILE__, __LINE__, "pure imaginary: scientific",
          "1e2j", "0", "100", 1e-60, 0 },

        { __FILE__, __LINE__, "pure imaginary: high precision",
          "2.71828182845904523536028747135266249775724709369996i",
          "0",
          "2.71828182845904523536028747135266249775724709369996",
          1e-60, 0 },

        /* a ± bi */
        { __FILE__, __LINE__, "a + i",
          "1 + i", "1", "1", 1e-60, 0 },

        { __FILE__, __LINE__, "a - i",
          "1 - i", "1", "-1", 1e-60, 0 },

        { __FILE__, __LINE__, "a + 1i",
          "1 + 1i", "1", "1", 1e-60, 0 },

        { __FILE__, __LINE__, "a - 1j",
          "1 - 1j", "1", "-1", 1e-60, 0 },

        { __FILE__, __LINE__, "a + bi (high precision)",
          "1.23456789012345678901234567890123456789012345678901 + "
          "9.87654321098765432109876543210987654321098765432109i",
          "1.23456789012345678901234567890123456789012345678901",
          "9.87654321098765432109876543210987654321098765432109",
          1e-60, 0 },

        { __FILE__, __LINE__, "bi + a",
          "2j+4", "4", "2", 1e-60, 0 },

        { __FILE__, __LINE__, "bi - a",
          "2j-4", "-4", "2", 1e-60, 0 },

        /* (a,b) tuple */
        { __FILE__, __LINE__, "(a,b) tuple",
          "(1,2)", "1", "2", 1e-60, 0 },

        { __FILE__, __LINE__, "(a,b) high precision",
          "(3.14159265358979323846264338327950288419716939937510,"
           "2.71828182845904523536028747135266249775724709369996)",
          "3.14159265358979323846264338327950288419716939937510",
          "2.71828182845904523536028747135266249775724709369996",
          1e-60, 0 },

        /* Scientific a + bj */
        { __FILE__, __LINE__, "scientific a + bj",
          "-1.0e-3 + 1.0e2j", "-1.0e-3", "1.0e2", 1e-60, 0 },

        /* Polar r*exp(theta i) — expected computed dynamically */
        { __FILE__, __LINE__, "polar: r*exp(theta i)",
          "1.732*exp(2.2i)", NULL, NULL, 1e-30, 0 },

        /* Full complex exponent r*exp(a+bi) — expected computed dynamically */
        { __FILE__, __LINE__, "polar: r*exp(a+bi)",
          "1.732*exp(1.1+2.2i)", NULL, NULL, 1e-30, 0 },

        /* Whitespace */
        { __FILE__, __LINE__, "whitespace: a + i",
          "   1   +   i   ", "1", "1", 1e-60, 0 },

        { __FILE__, __LINE__, "whitespace: (a,b)",
          " ( 1 , 2 ) ", "1", "2", 1e-60, 0 },

        /* FAILURE CASES — only those that actually yield NaN */
        { __FILE__, __LINE__, "fail: empty",
          "", NULL, NULL, 0, 1 },

        { __FILE__, __LINE__, "fail: (1 2)",
          "(1 2)", NULL, NULL, 0, 1 },

        { __FILE__, __LINE__, "fail: exp(2i) (missing r*)",
          "exp(2i)", NULL, NULL, 0, 1 },

        { __FILE__, __LINE__, "fail: 1*exp()",
          "1*exp()", NULL, NULL, 0, 1 },

        { __FILE__, __LINE__, "fail: 1*exp(1+)",
          "1*exp(1+)", NULL, NULL, 0, 1 },

        { __FILE__, __LINE__, "fail: 1*exp(i)",
          "1*exp(i)", NULL, NULL, 0, 1 },

        { __FILE__, __LINE__, "fail: 1*exp(1+i (missing ')')",
          "1*exp(1+i", NULL, NULL, 0, 1 },
    };

    const size_t N = sizeof(cases) / sizeof(cases[0]);

    for (size_t i = 0; i < N; i++) {

        const char *desc  = cases[i].desc;
        const char *input = cases[i].input;
        const char *file  = cases[i].file;
        int         line  = cases[i].line;
        double      tol   = cases[i].tol;
        int         expect_nan = cases[i].expect_nan;

        qcomplex_t z = qc_from_string(input);

        printf(C_YELLOW "TEST: %s (%s:%d)\n" C_RESET, desc, file, line);
        printf("    input    = \"%s\"\n", input);

        /* Failure cases: expect NaN */
        if (expect_nan) {
            tests_run++;
            int ok = qc_isnan(z);
            if (!ok) tests_failed++;

            printf(ok ? C_GREEN "  OK\n" C_RESET
                      : C_RED   "  FAIL\n" C_RESET);

            print_qc("    got     ", z);
            print_qc("    expected", qc_make(QF_NAN, QF_NAN));
            print_qc("    error   ", qc_make(QF_NAN, QF_NAN));
            //printf("\n");
            continue;
        }

        /* Success cases */

        qcomplex_t expected;

        /* Polar cases: compute expected dynamically */
        if (cases[i].re_exp == NULL && cases[i].im_exp == NULL &&
            strstr(desc, "polar: r*exp(theta i)") == desc) {

            qfloat_t r = qf_from_string("1.732");
            qfloat_t t = qf_from_string("2.2");
            expected = qc_make(qf_mul(r, qf_cos(t)),
                               qf_mul(r, qf_sin(t)));

        } else if (cases[i].re_exp == NULL && cases[i].im_exp == NULL &&
                   strstr(desc, "polar: r*exp(a+bi)") == desc) {

            qfloat_t r = qf_from_string("1.732");
            qcomplex_t e = qc_exp(qc_make(qf_from_string("1.1"),
                                          qf_from_string("2.2")));
            expected = qc_mul(qc_make(r, qf_from_double(0.0)), e);

        } else {
            qfloat_t re_exp = qf_from_string(cases[i].re_exp);
            qfloat_t im_exp = qf_from_string(cases[i].im_exp);
            expected = qc_make(re_exp, im_exp);
        }

        qcomplex_t diff = qc_sub(z, expected);
        double err = qf_to_double(qc_abs(diff));

        tests_run++;
        int ok = err < tol;
        if (!ok) tests_failed++;

        printf(ok ? C_GREEN "  OK\n" C_RESET
                  : C_RED   "  FAIL\n" C_RESET);

        print_qc("    got     ", z);
        print_qc("    expected", expected);
        print_qc("    error   ", diff);
    }
}

/* ====================================================================
   Entry point
   ==================================================================== */

int tests_main(void)
{
    RUN_TEST(test_arithmetic_group, NULL);
    RUN_TEST(test_elementary_group, NULL);
    RUN_TEST(test_trig_group,       NULL);
    RUN_TEST(test_special_group,    NULL);
    RUN_TEST(test_util_group,       NULL);
    RUN_TEST(test_from_string,      NULL);
    RUN_TEST(test_euler_identity,   NULL);

    return tests_failed;
}
