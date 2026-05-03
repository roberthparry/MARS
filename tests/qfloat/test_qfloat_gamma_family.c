#include "test_qfloat.h"

static void test_qf_gamma(void);
static void test_qf_erf(void);
static void test_qf_erfc(void);
static void test_qf_erfinv(void);
static void test_qf_erfcinv(void);
static void test_qf_lgamma(void);
static void test_qf_digamma(void);
static void test_qf_gammainv(void);

static int qf_close_value(qfloat_t got, qfloat_t expected, double tol)
{
    if (qf_eq(expected, qf_from_double(0.0)))
        return qf_close(got, expected, tol);
    return qf_close_rel(got, expected, tol);
}

static void test_qf_hypot(void)
{
    printf(C_CYAN "TEST: qf_hypot\n" C_RESET);

    char buf[128], buf_ref[128];

    /* 1) Full-precision qfloat_t reference (safe magnitudes) */
    struct {
        const char *xs, *ys, *label;
    } precise[] = {
        {"1", "0", "hypot(1,0)"},
        {"0", "1", "hypot(0,1)"},
        {"3", "4", "hypot(3,4)"},
        {"-3", "4", "hypot(-3,4)"},
        {"3", "-4", "hypot(3,-4)"},
        {"-3", "-4", "hypot(-3,-4)"},

        {"1", "1e-30", "hypot(1,1e-30)"},
        {"1e-30", "1", "hypot(1e-30,1)"},

        {"1e100", "3e99", "hypot(1e100,3e99)"},
        {"5e-100", "7e-100", "hypot(5e-100,7e-100)"},
    };

    int Np = (int)(sizeof(precise)/sizeof(precise[0]));

    for (int i = 0; i < Np; i++) {
        qfloat_t x = qf_from_string(precise[i].xs);
        qfloat_t y = qf_from_string(precise[i].ys);

        qfloat_t got = qf_hypot(x, y);

        /* direct qfloat_t reference: sqrt(x*x + y*y) */
        qfloat_t xx  = qf_mul(x, x);
        qfloat_t yy  = qf_mul(y, y);
        qfloat_t sum = qf_add(xx, yy);
        qfloat_t ref = qf_sqrt(sum);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(ref, buf_ref, sizeof(buf_ref));

        if (qf_close_rel(got, ref, 1e-31)) {
            printf("%s  OK: %s%s\n", C_GREEN, precise[i].label, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_ref);
        } else {
            printf("%s  FAIL: %s%s  [%s:%d]\n", C_RED, precise[i].label, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_ref);
            TEST_FAIL();
        }
    }

    /* symmetry (full precision) */
    {
        qfloat_t a = qf_from_double(1.2345);
        qfloat_t b = qf_from_double(9.8765);

        qfloat_t h1 = qf_hypot(a, b);
        qfloat_t h2 = qf_hypot(b, a);

        if (qf_close(h1, h2, 1e-32)) {
            printf("%s  OK: symmetry%s\n", C_GREEN, C_RESET);
        } else {
            printf("%s  FAIL: symmetry%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            TEST_FAIL();
        }
    }

    /* Extreme magnitudes: test qf_hypot via double projection only */
    struct {
        double x, y;
        const char *label;
    } extreme[] = {
        {1e300, 1e300,   "hypot(1e300,1e300)"},
        {1e300, 1e-300,  "hypot(1e300,1e-300)"},
        {1e200, 1e200,   "hypot(1e200,1e200)"},
        {1e200, 1e-200,  "hypot(1e200,1e-200)"},
    };

    int Ne = (int)(sizeof(extreme)/sizeof(extreme[0]));

    for (int i = 0; i < Ne; i++) {
        double xd = extreme[i].x;
        double yd = extreme[i].y;

        qfloat_t x = qf_from_double(xd);
        qfloat_t y = qf_from_double(yd);

        qfloat_t hq = qf_hypot(x, y);
        double  hq_d = qf_to_double(hq);
        double  href = hypot(xd, yd);
        double  err  = hq_d - href;

        qf_to_string(hq, buf, sizeof(buf));

        if (fabs(err) <= 1e-15 * fabs(href)) {
            printf("%s  OK: %s%s\n", C_GREEN, extreme[i].label, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    double   = %.17g\n", href);
            printf("    err      = %.3e\n", err);
        } else {
            printf("%s  FAIL: %s%s  [%s:%d]\n", C_RED, extreme[i].label, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    double   = %.17g\n", href);
            printf("    err      = %.3e\n", err);
            TEST_FAIL();
        }
    }

    printf("\n");
}

void test_hypotenus(void) {
    RUN_TEST(test_qf_hypot, __func__);
}

void test_gamma_erf_erfc_erfinv_erfcinv_digamma(void) {
    RUN_TEST(test_qf_gamma, __func__);
    RUN_TEST(test_qf_erf, __func__);
    RUN_TEST(test_qf_erfc, __func__);
    RUN_TEST(test_qf_erfinv, __func__);
    RUN_TEST(test_qf_erfcinv, __func__);
    RUN_TEST(test_qf_lgamma, __func__);
    RUN_TEST(test_qf_digamma, __func__);
    RUN_TEST(test_qf_gammainv, __func__);
    RUN_TEST(test_qf_trigamma, __func__);
    RUN_TEST(test_qf_tetragamma, __func__);
}

static void test_qf_gamma(void)
{
    printf(C_CYAN "TEST: qf_gamma\n" C_RESET);

    char buf[256], buf_exp[256];

    /* ------------------------------------------------------------
       1. High-precision expected values (fill these in yourself)
       ------------------------------------------------------------ */
    struct {
        const char *xs;      /* input x */
        const char *expected;/* expected gamma(x) as qfloat_t string */
        double tol;
    } tests[] = {

        /* Positive integers */
        { "1",   "1", 1e-30 },
        { "2",   "1", 1e-30 },
        { "3",   "2", 1e-30 },
        { "4",   "6", 1e-30 },
        { "5",   "24", 1e-30 },

        /* Half-integers */
        { "0.5",  "1.7724538509055160272981674833411451827975494561", 1e-30 },
        { "1.5",  "0.8862269254527580136490837416705725913987747281", 1e-30 },
        { "2.5",  "1.3293403881791370204736256125058588870981620921", 1e-30 },
        { "3.5",  "3.3233509704478425511840640312646472177454052302", 1e-30 },

        /* Positive non-integers */
        { "1.1",  "0.9513507698668731836292487177265402192550578626", 1e-30 },
        { "1.7",  "0.9086387328532904499768198254069681324488988194", 1e-30 },
        { "2.3",  "1.1667119051981603450418814412029179385339943497", 1e-30 },
        { "3.8",  "4.694174205740423202501646023091891", 1e-30 },
        { "4.2",  "7.7566895357931776386947595830098952250022722531", 1e-30 },
        { "7.1",  "868.95685880064040628832030517751832751591170159", 1e-30 },
        { "9.9",  "289867.7038401094067839862075034651", 1e-30 },

        /* Negative non-integers */
        { "-0.3", "-4.326851108825192618937237263842705392613803902", 1e-30 },
        { "-0.7", "-4.273669982410843754732166451292739701589722893", 1e-30 },
        { "-1.2", "4.850957140522097390151337242778876", 1e-30 },
        { "-2.3", "-1.447107394255917263858607780549399796860803981", 1e-30 },
        { "-3.8", "0.2996321345028458550807199167142564747437676221", 1e-30 },

        /* End marker */
        { NULL, NULL, 0.0 },
    };

    /* ------------------------------------------------------------
       2. Compare qf_gamma(x) against expected
       ------------------------------------------------------------ */
    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_gamma(x);
        qfloat_t exp = qf_from_string(tests[i].expected);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));

        if (qf_close_value(got, exp, tests[i].tol)) {
            printf("%s  OK: gamma(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: gamma(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }

    /* ------------------------------------------------------------
       3. Poles (should be NaN)
       ------------------------------------------------------------ */
    const char *poles[] = { "0", "-1", "-2", "-3", "-4" };
    for (size_t i = 0; i < sizeof(poles)/sizeof(poles[0]); i++) {
        qfloat_t x = qf_from_string(poles[i]);
        qfloat_t g = qf_gamma(x);

        if (isnan(g.hi)) {
            printf("%s  OK: gamma(%s) is NaN%s\n", C_GREEN, poles[i], C_RESET);
        } else {
            qf_to_string(g, buf, sizeof(buf));
            printf("%s  FAIL: gamma(%s) should be NaN%s  [%s:%d]\n", C_RED, poles[i], C_RESET, __FILE__, __LINE__);
            printf("    got = %s\n", buf);
            TEST_FAIL();
        }
    }

    /* ------------------------------------------------------------
       4. Extreme magnitudes (should be NaN)
       ------------------------------------------------------------ */
    const char *extreme[] = { "1e300", "-1e300" };
    for (size_t i = 0; i < sizeof(extreme)/sizeof(extreme[0]); i++) {
        qfloat_t x = qf_from_string(extreme[i]);
        qfloat_t g = qf_gamma(x);

        if (isnan(g.hi)) {
            printf("%s  OK: gamma(%s) extreme -> NaN%s\n",
                   C_GREEN, extreme[i], C_RESET);
        } else {
            qf_to_string(g, buf, sizeof(buf));
            printf("%s  FAIL: gamma(%s) should be NaN%s  [%s:%d]\n", C_RED, extreme[i], C_RESET, __FILE__, __LINE__);
            printf("    got = %s\n", buf);
            TEST_FAIL();
        }
    }

    printf("\n");
}

static void test_qf_erf(void)
{
    printf(C_CYAN "TEST: qf_erf\n" C_RESET);

    char buf[256], buf_exp[256];

    /* ------------------------------------------------------------
       1. Expected values (fill these in yourself)
       ------------------------------------------------------------ */
    struct {
        const char *xs;       /* input x */
        const char *expected; /* expected erf(x) as qfloat_t string */
    } tests[] = {

        /* Basic values */
        { "0",     "0" },
        { "0.5",   "0.5204998778130465376827466538919645287364515758" },
        { "1.0",   "0.8427007929497148693412206350826092592960669980" },
        { "1.2",   "0.9103139782296353802384057757153736772278970560" },
        { "1.5",   "0.9661051464753107270669762616459478586814104793" },
        { "2.0",   "0.9953222650189527341620692563672529286108917970" },
        { "2.5",   "0.9995930479825550410604357842600250872796513226" },
        { "3.0",   "0.9999779095030014145586272238704176796201522929" },

        /* Negative values */
        { "-0.5",  "-0.520499877813046537682746653891964528736451576" },
        { "-1.0",  "-0.842700792949714869341220635082609259296066998" },
        { "-1.7",  "-0.983790458590774563626242588121881213432720496" },
        { "-2.3",  "-0.998856823402643348534652540619230859805851309" },
        { "-3.0",  "-0.999977909503001414558627223870417679620152293" },

        /* Values near the cutoff (1.5) */
        { "1.49",  "0.9648978648432042121029072375635501433975444954" },
        { "1.51",  "0.9672767481287116359138259912486972279263387115" },

        /* Large but safe values */
        { "5.0",   "0.9999999999984625402055719651498116565146166211" },
        { "-5.0",  "-0.999999999998462540205571965149811656514616621" },

        /* extreme values */
        { "26.0",   "1" },
        { "30.0",   "1" },
        { "-30.0",  "-1" },
        { "1e300",  "1" },
        { "-1e300",  "-1" },

        /* End marker */
        { NULL, NULL }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_erf(x);
        qfloat_t exp = qf_from_string(tests[i].expected);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));

        if (qf_close_value(got, exp, 1e-30)) {
            printf("%s  OK: erf(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: erf(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }

    printf("\n");
}

static void test_qf_erfc(void)
{
    printf(C_CYAN "TEST: qf_erfc\n" C_RESET);

    char buf[256], buf_exp[256];

    struct {
        const char *xs;       /* input x */
        const char *expected; /* expected erfc(x) as qfloat_t string */
    } tests[] = {

        /* Basic values */
        { "0",     "1" },
        { "0.5",   "0.47950012218695346231725334610803547" },
        { "1.0",   "0.15729920705028513065877936491739074" },
        { "1.2",   "0.089686021770364619761594224284626322" },
        { "1.5",   "0.03389485352468927293302373835395015" },
        { "2.0",   "0.004677734981047265837930743632739447" },
        { "2.5",   "0.0004069520174449589395642157399749127203" },
        { "3.0",   "0.00002209049699858544137277612958232037985" },

        /* Negative values */
        { "-0.5",  "1.52049987781304653768274665389196453" },
        { "-1.0",  "1.84270079294971486934122063508260926" },
        { "-1.7",  "1.98379045859077456362624258812188121" },
        { "-2.3",  "1.99885682340264334853465254061923086" },
        { "-3.0",  "1.99997790950300141455862722387041768" },

        /* Values near the cutoff (1.5) */
        { "1.49",  "0.03510213515679578789709276243649683" },
        { "1.51",  "0.03272325187128836408617400875119170" },

        /* Larger but safe values */
        { "5.0",   "1.53745979442803485018834348538337889e-12" },
        { "-5.0",  "1.99999999999846254020557196514981166" },

        /* extreme values */
        { "26.0",   "5.6631924088561428464757278969260925e-296" },
        { "30.0",   "0" },
        { "-30.0",  "2" },
        { "1e300",  "0" },
        { "-1e300",  "2" },

        /* End marker */
        { NULL, NULL }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_erfc(x);
        qfloat_t exp = qf_from_string(tests[i].expected);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));

        
        if (qf_close_value(got, exp, 1e-30)) {
            printf("%s  OK: erfc(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: erfc(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }

    printf("\n");
}

static void test_qf_erfinv(void) {
    printf(C_CYAN "TEST: qf_erfinv\n" C_RESET);

    char buf[256], buf_exp[256];

    struct {
        const char *xs;       /* input x */
        const char *expected; /* expected erfc(x) as qfloat_t string */
        double acceptable_error;
    } tests[] = {

        /* Basic values */
        { "-0.9",  "-1.163087153676674086726254260562948", 1e-30 },
        { "-0.87", "-1.070631712142947435214622665410316", 1e-30 },
        { "-0.85", "-1.017902464832027643608729928245034", 1e-30 },
        { "-0.8",  "-0.9061938024368232200711627030956629", 1e-30 },
        { "-0.7",  "-0.7328690779592168522188174610580155", 1e-30 },
        { "-0.3",  "-0.2724627147267543556219575985875658", 1e-30 },
        { "-0.1",  "-0.08885599049425768701573725056779177", 1e-30 },
        { "0",     "0", 1e-30 },
        { "0.1",   "0.08885599049425768701573725056779177", 1e-30 },
        { "0.3",   "0.2724627147267543556219575985875658", 1e-30 },
        { "0.7",   "0.7328690779592168522188174610580155", 1e-30 },
        { "0.8",   "0.9061938024368232200711627030956629", 1e-30 },
        { "0.85",  "1.017902464832027643608729928245034", 1e-30 },
        { "0.87",  "1.070631712142947435214622665410316", 1e-30 },
        { "0.9",   "1.163087153676674086726254260562948", 1e-30 },

        /* Values near the cutoff */
        { "-0.9999999999",  "-4.572824967389485278741043673140672", 1e-22 },
        { "-0.99999999",  "-4.0522372438713892052307738897049963634", 1e-24 },
        { "-0.9999",  "-2.751063905712060796145513168539483", 1e-30 },
        { "0.9999",  "2.751063905712060796145513168539483", 1e-30 },
        { "0.99999999",  "4.0522372438713892052307738897049963634", 1e-24 },
        { "0.9999999999",  "4.572824967389485278741043673140672", 1e-22 },

        /* extreme values */
        { "1",  "NAN", 0.0 },
        { "-1",   "NAN", 0.0 },

        /* End marker */
        { NULL, NULL, 0.0 }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_erfinv(x);
        if (strcmp(tests[i].expected, "NAN") == 0) {
            if (qf_isnan(got)) {
                printf("%s  OK: erfinv(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            } else {
                printf("%s  FAIL: erfinv(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
                printf("    expected = NAN\n");
                TEST_FAIL();
            }
            continue;
        }
        else {
            qfloat_t exp = qf_from_string(tests[i].expected);

            qf_to_string(got, buf, sizeof(buf));
            qf_to_string(exp, buf_exp, sizeof(buf_exp));

            if (qf_close_value(got, exp, tests[i].acceptable_error)) {
                printf("%s  OK: erfinv(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
                printf("    got      = %s\n", buf);
                printf("    expected = %s\n", buf_exp);
            } else {
                printf("%s  FAIL: erfinv(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
                printf("    got      = %s\n", buf);
                printf("    expected = %s\n", buf_exp);
                TEST_FAIL();
            }
        }
    }

    printf("\n");
}

static void test_qf_erfcinv(void) {
    printf(C_CYAN "TEST: qf_erfcinv\n" C_RESET);

    char buf[256], buf_exp[256];

    struct {
        const char *xs;       /* input x */
        const char *expected; /* expected erfcinv(x) as qfloat_t string */
        double acceptable_error;
    } tests[] = {

        /* Basic values */
        { "0.1",   "1.163087153676674086726254260562948", 1e-30 },
        { "0.13",  "1.070631712142947435214622665410316", 1e-30 },
        { "0.15",  "1.017902464832027643608729928245034", 1e-30 },
        { "0.2",   "0.9061938024368232200711627030956629", 1e-30 },
        { "0.3",   "0.7328690779592168522188174610580155", 1e-30 },
        { "0.7",   "0.2724627147267543556219575985875658", 1e-30 },
        { "0.9",   "0.08885599049425768701573725056779177", 1e-30 },
        { "1",     "0", 1e-30 },
        { "1.1",   "-0.08885599049425768701573725056779177", 1e-30 },
        { "1.3",   "-0.2724627147267543556219575985875658", 1e-30 },
        { "1.7",   "-0.7328690779592168522188174610580155", 1e-30 },
        { "1.8",   "-0.9061938024368232200711627030956629", 1e-30 },
        { "1.85",  "-1.017902464832027643608729928245034", 1e-30 },
        { "1.87",  "-1.070631712142947435214622665410316", 1e-30 },
        { "1.9",   "-1.163087153676674086726254260562948", 1e-30 },

        /* Values near the cutoff */
        { "2e-10", "4.4981472895292597414585341748556", 1e-30 },
        { "2e-8",  "3.9682841357833348003564647370728", 1e-30 },
        { "2e-4",  "2.6297417762102729206203505927957", 1e-30 },
        { "1.9998", "-2.629741776210272920620350592792912", 1e-30 },
        { "1.99999998", "-3.9682841357833348003564636926937", 1e-23 },
        { "1.9999999998", "-4.4981472895292597414584255650214", 1e-21 },

        /* extreme values */
        { "0",  "POSINF", 0.0 },
        { "2",  "NEGINF", 0.0 },

        /* End marker */
        { NULL, NULL, 0.0 }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_erfcinv(x);

        /* Handle infinities */
        if (strcmp(tests[i].expected, "POSINF") == 0) {
            if (qf_isposinf(got)) {
                printf("%s  OK: erfcinv(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            } else {
                printf("%s  FAIL: erfcinv(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
                printf("    expected = +INF\n");
                TEST_FAIL();
            }
            continue;
        }

        if (strcmp(tests[i].expected, "NEGINF") == 0) {
            if (qf_isneginf(got)) {
                printf("%s  OK: erfcinv(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            } else {
                printf("%s  FAIL: erfcinv(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
                printf("    expected = -INF\n");
                TEST_FAIL();
            }
            continue;
        }

        /* Normal numeric case */
        qfloat_t exp = qf_from_string(tests[i].expected);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));

        if (qf_close_value(got, exp, tests[i].acceptable_error)) {
            printf("%s  OK: erfcinv(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: erfcinv(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }

    printf("\n");
}

static void test_qf_lgamma(void) {
    printf(C_CYAN "TEST: qf_lgamma\n" C_RESET);

    char buf[256], buf_exp[256];

    struct {
        const char *xs;
        const char *expected;
        double acceptable_error;
    } tests[] = {

        /* Basic values */
        { "0.5", "0.572364942924700087071713675676529",  1e-30 },
        { "1",    "0",                                   1e-32 },
        { "2",    "0",                                   1e-32 },
        { "3",    "0.693147180559945309417232121458176", 1e-30 },
        { "4",    "1.79175946922805500081247735838070",  1e-30 },

        /* Non-integers */
        { "1.5",  "-0.120782237635245222345518445781647", 1e-30 },
        { "2.5",   "0.284682870472919159632494669682701", 1e-30 },

        /* Larger x */
        { "4.5",   "2.4537365708424422205041425034357162", 1e-30 },
        { "5.5",   "3.957813967618716293877400855822591", 1e-30 },
        { "6.5",   "5.6625620598571415285221123123295437", 1e-30 },
        { "7.5",   "7.5343642367587329551583676324366858", 1e-30 },
        { "8.0",   "8.5251613610654143001655310363471251", 1e-30 },
        { "9.0",  "10.6046029027452502284172274007216548", 1e-30 },
        { "10",   "12.8018274800814696112077178745667", 1e-30 },
        { "12",   "17.5023078458738858392876529072162", 1e-30 },
        { "14",   "22.5521638531234228855708498286204", 1e-30 },
        { "16",   "27.899271383840891566089439263670467", 1e-30 },
        { "18",   "33.505073450136888884007902367376300", 1e-30 },
        { "20",   "39.3398841871994940362246523945670", 1e-30 },

        /* Poles */
        { "0",    "NAN", 0.0 },
        { "-1",   "NAN", 0.0 },
        { "-2",   "NAN", 0.0 },

        { NULL, NULL, 0.0 }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_lgamma(x);

        if (strcmp(tests[i].expected, "NAN") == 0) {
            if (qf_isnan(got)) {
                printf(C_GREEN "  OK: lgamma(%s)\n" C_RESET, tests[i].xs);
            } else {
                printf(C_RED "  FAIL: lgamma(%s)  [%s:%d]\n" C_RESET, tests[i].xs, __FILE__, __LINE__);
                printf("    expected = NAN\n");
                qf_to_string(got, buf, sizeof(buf));
                printf("    got      = %s\n", buf);
                TEST_FAIL();
            }
            continue;
        }

        qfloat_t exp = qf_from_string(tests[i].expected);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));
        
        if (qf_close_value(got, exp, tests[i].acceptable_error)) {
            printf(C_GREEN "  OK: lgamma(%s)\n" C_RESET, tests[i].xs);
        } else {
            printf(C_RED "  FAIL: lgamma(%s)  [%s:%d]\n" C_RESET, tests[i].xs, __FILE__, __LINE__);
            TEST_FAIL();
        }

        printf("    got      = %s\n", buf);
        printf("    expected = %s\n", buf_exp);
    }

    printf("\n");
}

static void test_qf_digamma(void) {
    printf(C_CYAN "TEST: qf_digamma\n" C_RESET);

    char buf[256], buf_exp[256];

    struct {
        const char *xs;
        const char *expected;
        double acceptable_error;
    } tests[] = {

        /* Known values */
        { "1",    "-0.577215664901532860606512090082402", 1e-30 },
        { "2",     "0.422784335098467139393487909917598", 1e-30 },
        { "3",     "0.922784335098467139393487909917598", 1e-30 },
        { "4",     "1.25611766843180047272682124325093",  1e-30 },

        /* Non-integers */
        { "1.5",   "0.036489973978576520559023667001244", 1e-30 },
        { "2.5",   "0.703156640645243187225690333667911", 1e-30 },

        /* Larger x */
        { "4.5", "1.388870926359528901511404619382172", 1e-30 },
        { "5.5", "1.611093148581751123733626841604400", 1e-30 },
        { "6.5", "1.792911330399932941915445023422581", 1e-30 },
        { "7.5", "1.946757484246086788069291177268746", 1e-30 },
        { "8.0", "2.015641477955609996536345052774742", 1e-30 },
        { "9.0", "2.140641477955609996536345052774718", 1e-30 },
        { "10",  "2.251752589066721107647456163885856", 1e-30 },
        { "12",  "2.442661679975812016738365254794945", 1e-30 },
        { "14",  "2.602918090232222273148621665051354", 1e-30 },
        { "16",  "2.741013328327460368386716903146574", 1e-30 },
        { "18",  "2.862336857739225074269069844323042", 1e-30 },
        { "20",  "2.970523992242149050877256978825989", 1e-30 },

        /* Poles */
        { "0",     "NAN", 0.0 },
        { "-1",    "NAN", 0.0 },
        { "-2",    "NAN", 0.0 },

        { NULL, NULL, 0.0 }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_digamma(x);

        if (strcmp(tests[i].expected, "NAN") == 0) {
            if (qf_isnan(got)) {
                printf(C_GREEN "  OK: digamma(%s)\n" C_RESET, tests[i].xs);
            } else {
                printf(C_RED "  FAIL: digamma(%s)  [%s:%d]\n" C_RESET, tests[i].xs, __FILE__, __LINE__);
                printf("    expected = NAN\n");
                qf_to_string(got, buf, sizeof(buf));
                printf("    got      = %s\n", buf);
                TEST_FAIL();
            }
            continue;
        }

        qfloat_t exp = qf_from_string(tests[i].expected);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));

        if (qf_close_value(got, exp, tests[i].acceptable_error)) {
            printf(C_GREEN "  OK: digamma(%s)\n" C_RESET, tests[i].xs);
        } else {
            printf(C_RED "  FAIL: digamma(%s)  [%s:%d]\n" C_RESET, tests[i].xs, __FILE__, __LINE__);
            TEST_FAIL();
        }

        printf("    got      = %s\n", buf);
        printf("    expected = %s\n", buf_exp);
    }

    printf("\n");
}

static void test_qf_gammainv(void) {
    printf(C_CYAN "TEST: qf_gammainv\n" C_RESET);

    char buf[256], buf_exp[256];

    struct {
        const char *xs;
        double acceptable_error;
    } tests[] = {

        { "1",    1e-30 },
        { "3",    1e-30 },
        { "4",    1e-30 },
        { "5",    1e-30 },
        { "7",    1e-30 },
        { "9",    1e-30 },
        { "9.5",  1e-30 },
        { "10",   1e-30 },
        { "12",   1e-30 },
        { "15",   1e-30 },
        { "20",   1e-30 },

        { NULL, 0.0 }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t y   = qf_gamma(x);
        qfloat_t got = qf_gammainv(y);

        qfloat_t exp = x;

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));

        if (qf_close_value(got, exp, tests[i].acceptable_error)) {
            printf(C_GREEN "  OK: gammainv(gamma(%s))\n" C_RESET, tests[i].xs);
        } else {
            printf(C_RED "  FAIL: gammainv(%s)  [%s:%d]\n" C_RESET, tests[i].xs, __FILE__, __LINE__);
            TEST_FAIL();
        }

        printf("    got      = %s\n", buf);
        printf("    expected = %s\n", buf_exp);
    }

    printf("\n");
}
