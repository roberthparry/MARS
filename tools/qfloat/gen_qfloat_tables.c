#include <stdio.h>
#include <string.h>

#include "mfloat.h"
#include "qfloat.h"

static qfloat_t qf_from_mfloat(mfloat_t *value)
{
    qfloat_t q = mf_to_qfloat(value);
    mf_free(value);
    return q;
}

static void emit_literal(const char *name, qfloat_t value)
{
    printf("const qfloat_t %s = {\n", name);
    printf("    %.17g,\n", value.hi);
    printf("    %.17g\n", value.lo);
    printf("};\n\n");
}

static void emit_from_decimal(const char *decimal)
{
    qfloat_t q = qf_from_string(decimal);
    printf("        { %.17g, %.17g },   /* %s */\n", q.hi, q.lo, decimal);
}

static qfloat_t mf_q_pi_scale(long num, long den)
{
    mfloat_t *x = mf_pi();
    mfloat_t *n = mf_create_long(num);
    mf_mul(x, n);
    mf_free(n);
    if (den != 1) {
        mfloat_t *d = mf_create_long(den);
        mf_div(x, d);
        mf_free(d);
    }
    return qf_from_mfloat(x);
}

static void emit_public_constants(void)
{
    mf_set_default_precision(256u);

    emit_literal("QF_PI", qf_from_mfloat(mf_pi()));
    emit_literal("QF_2PI", mf_q_pi_scale(2, 1));
    emit_literal("QF_PI_2", mf_q_pi_scale(1, 2));
    emit_literal("QF_PI_4", mf_q_pi_scale(1, 4));
    emit_literal("QF_3PI_4", mf_q_pi_scale(3, 4));
    emit_literal("QF_PI_6", mf_q_pi_scale(1, 6));
    emit_literal("QF_PI_3", mf_q_pi_scale(1, 3));

    {
        mfloat_t *pi = mf_pi();
        mfloat_t *two = mf_create_long(2);
        mf_div(two, pi);
        mf_free(pi);
        emit_literal("QF_2_PI", qf_from_mfloat(two));
    }

    emit_literal("QF_E", qf_from_mfloat(mf_e()));

    {
        mfloat_t *e = mf_e();
        mfloat_t *one = mf_create_long(1);
        mf_div(one, e);
        mf_free(e);
        emit_literal("QF_INV_E", qf_from_mfloat(one));
    }

    {
        mfloat_t *x = mf_create_long(2);
        mf_log(x);
        emit_literal("QF_LN2", qf_from_mfloat(x));
    }

    {
        mfloat_t *x = mf_create_long(2);
        mfloat_t *one = mf_create_long(1);
        mf_log(x);
        mf_div(one, x);
        mf_free(x);
        emit_literal("QF_INVLN2", qf_from_mfloat(one));
    }

    {
        mfloat_t *x = mf_clone(MF_HALF);
        mf_sqrt(x);
        emit_literal("QF_SQRT_HALF", qf_from_mfloat(x));
    }

    emit_literal("QF_SQRT_PI", qf_from_mfloat(mf_clone(MF_SQRT_PI)));

    {
        mfloat_t *pi = mf_pi();
        mfloat_t *one = mf_create_long(1);
        mf_div(one, pi);
        mf_free(pi);
        mf_sqrt(one);
        emit_literal("QF_SQRT1ONPI", qf_from_mfloat(one));
    }

    {
        mfloat_t *x = mf_create_long(2);
        mfloat_t *root_pi = mf_clone(MF_SQRT_PI);
        mf_div(x, root_pi);
        mf_free(root_pi);
        emit_literal("QF_2_SQRTPI", qf_from_mfloat(x));
    }

    {
        mfloat_t *pi = mf_pi();
        mfloat_t *two = mf_create_long(2);
        mfloat_t *one = mf_create_long(1);
        mf_mul(pi, two);
        mf_free(two);
        mf_sqrt(pi);
        mf_div(one, pi);
        mf_free(pi);
        emit_literal("QF_INV_SQRT_2PI", qf_from_mfloat(one));
    }

    {
        mfloat_t *x = mf_clone(MF_SQRT2);
        mfloat_t *root_pi = mf_clone(MF_SQRT_PI);
        mf_mul(x, root_pi);
        mf_free(root_pi);
        mf_log(x);
        emit_literal("QF_LOG_SQRT_2PI", qf_from_mfloat(x));
    }

    {
        mfloat_t *x = mf_create_long(2);
        mfloat_t *pi = mf_pi();
        mf_mul(x, pi);
        mf_free(pi);
        mf_log(x);
        emit_literal("QF_LN_2PI", qf_from_mfloat(x));
    }

    emit_literal("QF_EULER_MASCHERONI", qf_from_mfloat(mf_euler_mascheroni()));
}

static void emit_exp_coeffs(void)
{
    const char *coeffs[] = {
        "2.00782013270670897025569345450493539496719826451529e+00",
        "1.25244299622469650888488552059403016996167595573611e-01",
        "3.91133874719455603987662155448712302851673533750791e-03",
        "8.14597122438576124366623158150800836320647733580159e-05",
        "1.27255948939064291683039536327901417762621632314779e-06",
        "1.59049228564657595170125652231762639869286765571089e-08",
        "1.65660873382155469390145424913058671922198579085015e-10",
        "1.47901177883445555860443152263148239761296494738770e-12",
        "1.15541526964468264490943783326433895465049775927119e-14",
        "8.02336892617731203510960531285356603278155205852885e-17",
        "5.01442751497118536546682134254459299542628430307812e-19",
        "2.84902223415450362691164782217240099497173603853959e-21",
        "1.48382859258982102321175521167244276028875248448675e-23",
        "7.13363820472623450778157613108952173312683250216815e-26",
        "3.18460067642455931873764058222397983713240357784294e-28",
        "1.32689535221633809261226907806897950248487799967001e-30",
        "5.18311053478964681947948584290311686963786349166464e-33",
        "1.90553101885068255207022857815838575849458038608558e-35",
        "6.61635157902778484641103123076065326048415118568003e-38",
        "2.17640906805163038515836993176194751448446097344594e-40",
        "6.80121508284755295866382043328164513898264043772395e-43",
        "2.02415404134383859473931118210700100160333742738858e-45",
        "5.75039322552803397348614021217735954266816983331649e-48",
        "1.56259579706360721898274205704425841416460611722259e-50",
        "4.06923339594076296494422544885785424193219373742892e-53",
        "1.01730223542404441594846828423852626437220498786958e-55",
        "2.44542445853011503523119037491844433737859505921607e-58",
        "5.66067551656129229308827245342002270944323569890047e-61",
        "1.26353756367646170566750409945268991172372910631121e-63",
        "2.72312907438516978540878719719049209293026262692022e-66",
        "5.67316174292523782683995630158060408724742022221520e-69",
        "1.14377810556285256081724318021310515009202569273747e-71",
        "2.23393334891251864301277236025427909727862373965758e-74",
        NULL
    };
    for (int i = 0; coeffs[i] != NULL; i++)
        emit_from_decimal(coeffs[i]);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--exp-coef") == 0) {
        emit_exp_coeffs();
        return 0;
    }

    emit_public_constants();
    return 0;
}
