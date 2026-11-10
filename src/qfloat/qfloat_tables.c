/**
 * @file qfloat_tables.c
 * @brief Double-double mathematical constants and approximation tables.
 *
 * Keeps public constant pairs and internal coefficient arrays together as immutable numerical data. The original
 * high and low components, table order and element counts are retained. Arithmetic and special-function
 * implementations consume these definitions through their existing public or private declarations.
 */

#include <math.h>

#define MARS_QFLOAT_INTERNAL_ACCESS
#include "qfloat_internal.h"

/* Exact and sentinel values. */
const qfloat_t QF_ZERO = {0.0, 0.0};
const qfloat_t QF_ONE = {1.0, 0.0};
const qfloat_t QF_NEG_ONE = {-1.0, 0.0};
const qfloat_t QF_HALF = {0.5, 0.0};
const qfloat_t QF_TWO = {2.0, 0.0};
const qfloat_t QF_TEN = {10.0, 0.0};
const qfloat_t QF_NAN = {NAN, NAN};
const qfloat_t QF_INF = {INFINITY, 0.0};
const qfloat_t QF_NINF = {-INFINITY, 0.0};
const qfloat_t QF_MAX = {1.79769313486231570815e+308, 9.97920154767359795037e+291};

/* Transcendental and derived constants. */
const qfloat_t QF_PI = {3.14159265358979311600e+00, 1.2246467991473532072e-16};
const qfloat_t QF_2PI = {6.28318530717958623200e+00, 2.4492935982947064144e-16};
const qfloat_t QF_PI_2 = {1.57079632679489655800e+00, 6.123233995736766036e-17};
const qfloat_t QF_NEG_PI_2 = {-1.57079632679489655800e+00, -6.123233995736766036e-17};
const qfloat_t QF_PI_4 = {7.8539816339744827900e-01, 3.061616997868383018e-17};
const qfloat_t QF_3PI_4 = {2.356194490192344837e+00, 9.1848509936051484375e-17};
const qfloat_t QF_PI_6 = {0.52359877559829893, -5.3604088322554549e-17};
const qfloat_t QF_PI_3 = {1.0471975511965979, -1.072081766451091e-16};
const qfloat_t QF_2_PI = {0.63661977236758138, -3.9357353350364972e-17};
const qfloat_t QF_E = {2.718281828459045091e+00, 1.445646891729250158e-16};
const qfloat_t QF_INV_E = {0.36787944117144233, -1.2428753672788363e-17};
const qfloat_t QF_NEG_INV_E = {-0.36787944117144233, 1.2428753672788363e-17};
const qfloat_t QF_LN2 = {6.9314718055994530941723212145817656e-01, 2.3190468138462995584177710792423e-17};
const qfloat_t QF_LN10 = {2.302585092994046, -2.1707562233822495e-16};
const qfloat_t QF_INVLN2 = {1.4426950408889634073599246810019e+00, 2.0355273740931032980408127082449e-17};
const qfloat_t QF_SQRT_HALF = {0.70710678118654757, -4.8336466567264567e-17};
const qfloat_t QF_SQRT_PI = {1.7724538509055161, -7.6665864998257987e-17};
const qfloat_t QF_SQRT1ONPI = {0.56418958354775628, 7.6677298065829391e-18};
const qfloat_t QF_SQRT3 = {1.7320508075688772, 1.0035084221806903e-16};
const qfloat_t QF_2_SQRTPI = {1.1283791670955126, 1.5335459613165878e-17};
const qfloat_t QF_INV_SQRT_2PI = {0.3989422804014327, -2.49232720227773e-17};
const qfloat_t QF_LOG_SQRT_2PI = {0.91893853320467278, -3.8782941580672414e-17};
const qfloat_t QF_LN_2PI = {1.8378770664093456, -7.7565883161344829e-17};
const qfloat_t QF_EULER_MASCHERONI = {0.57721566490153287, -4.9429151524306449e-18};
const qfloat_t QF_PHI = {1.6180339887498949, -5.4321152036825055e-17};

/* Machine epsilon for the qfloat format. */
const double QF_EPS = 4.93038065763132e-32;

enum {
    QFI_LANCZOS_COEFF_COUNT_VALUE = 9,
    QFI_BERNOULLI_EVEN_TERM_COUNT_VALUE = 17
};

const size_t QFI_LANCZOS_COEFF_COUNT = QFI_LANCZOS_COEFF_COUNT_VALUE;
const size_t QFI_BERNOULLI_EVEN_TERM_COUNT = QFI_BERNOULLI_EVEN_TERM_COUNT_VALUE;

const qfloat_bernoulli_even_term_t QFI_BERNOULLI_EVEN_TERMS[QFI_BERNOULLI_EVEN_TERM_COUNT_VALUE] = {
    {1.0, 6.0, 1},                 /* B_2  =  1/6             */
    {1.0, 30.0, -1},               /* B_4  = -1/30            */
    {1.0, 42.0, 1},                /* B_6  =  1/42            */
    {1.0, 30.0, -1},               /* B_8  = -1/30            */
    {5.0, 66.0, 1},                /* B_10 =  5/66            */
    {691.0, 2730.0, -1},           /* B_12 = -691/2730        */
    {7.0, 6.0, 1},                 /* B_14 =  7/6             */
    {3617.0, 510.0, -1},           /* B_16 = -3617/510        */
    {43867.0, 798.0, 1},           /* B_18 =  43867/798       */
    {174611.0, 330.0, -1},         /* B_20 = -174611/330      */
    {854513.0, 138.0, 1},          /* B_22 =  854513/138      */
    {236364091.0, 2730.0, -1},     /* B_24 = -236364091/2730  */
    {8553103.0, 6.0, 1},           /* B_26 =  8553103/6       */
    {23749461029.0, 870.0, -1},    /* B_28 = -23749461029/870 */
    {8615841276005.0, 14322.0, 1}, /* B_30 =  8615841276005/14322 */
    {7709321041217.0, 510.0, -1},  /* B_32 = -7709321041217/510   */
    {2577687858367.0, 6.0, 1},     /* B_34 =  2577687858367/6     */
};

const qfloat_t QFI_LANCZOS_C[QFI_LANCZOS_COEFF_COUNT_VALUE] = {
    {1.00000000000000000e+00, -6.99999999999999971e-34},  {6.76520368121885099e+02, -2.77349972750868088e-16},
    {-1.25913921672240281e+03, -6.33356109449210117e-14}, {7.71323428777653135e+02, -5.57539348441983815e-14},
    {-1.76615029162140587e+02, -1.20612308688560953e-14}, {1.25073432786869052e+01, -3.93560260890580655e-16},
    {-1.38571095265720118e-01, 9.21762572455474946e-19},  {9.98436957801957158e-06, -7.23679346146658305e-22},
    {1.50563273514931162e-07, -5.92527880744794851e-24},
};

const qfloat_t QF_SQRT_2PI = {2.5066282746310007, -1.8328579980459169e-16};
const qfloat_t QF_SQRT2 = {1.4142135623730951, -9.6672933134529704e-17};
const qfloat_t QF_SQRT2_OVER_TWO = {0.70710678118654757, -4.8336466567264851e-17};
const qfloat_t QF_SQRT3_OVER_TWO = {0.8660254037844386, 5.0175421109034515e-17};
const qfloat_t QF_PI_SQUARED = {9.869604401089358, 2.4674011002723397e-16};
const qfloat_t QF_2PI_CUBED = {62.01255336059964, 8.3283893970576566e-16};
const qfloat_t QF_SQRT_PI_OVER_TWO = {0.88622692545275805, -3.8332932499129024e-17};
const qfloat_t QF_NEG_TWO_OVER_SQRT_PI = {-1.1283791670955126, -1.5335459613165487e-17};
const qfloat_t QF_ONE_AND_HALF = {1.5, 0.0};
const qfloat_t QFI_LANCZOS_SHIFT = {7.5, 0.0};
const qfloat_t QF_ONE_THIRD = {0.33333333333333331, 1.8503717077085941e-17};
const qfloat_t QFI_ELEVEN_OVER_SEVENTY_TWO = {0.15277777777777779, -6.1679056923619819e-18};
const qfloat_t QF_QUARTER = {0.25, 0.0};
const qfloat_t QF_ONE_SIXTH = {0.16666666666666666, 9.2518585385429707e-18};
const qfloat_t QF_ONE_EIGHTH = {0.125, 0.0};
const qfloat_t QF_ONE_TENTH = {0.10000000000000001, -5.551115123125783e-18};
const qfloat_t QFI_BERNOULLI_B2 = {0.16666666666666666, 9.2518585385429707e-18};
const qfloat_t QFI_BERNOULLI_B4 = {-0.033333333333333333, -4.6259292692714855e-19};
const qfloat_t QFI_BERNOULLI_B6 = {0.023809523809523808, 1.3216940769347101e-18};
const qfloat_t QFI_BERNOULLI_B8 = {-0.033333333333333333, -4.6259292692714855e-19};
const qfloat_t QFI_BERNOULLI_B10 = {0.07575757575757576, -2.1026951223961299e-18};
