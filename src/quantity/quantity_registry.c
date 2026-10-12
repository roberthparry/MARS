/**
 * @file quantity_registry.c
 * @brief Exact unit definitions and collision-free unit-alias recognition.
 *
 * Factors are rational multiples of coherent SI units and optional powers of
 * pi. See docs/quantity.md for the defining NIST and IAU references.
 * Two-level FNV-1a hashing uses 61 buckets and 317 collision-free alias slots.
 * Bucket seeds are chosen largest-bucket-first against already occupied slots.
 * A checked single-slot lookup rejects unknown names; there is no search loop.
 * When adding aliases, regenerate bucket seeds and designated slots together
 * (seed search starts at one in each bucket), then run the exhaustive alias
 * regressions. Only byte hashing is linear in input length.
 */
#include <string.h>

#define MARS_QUANTITY_INTERNAL_ACCESS
#include "quantity_internal.h"

enum {
    UNIT_ONE,
    UNIT_M,
    UNIT_CM,
    UNIT_MM,
    UNIT_KM,
    UNIT_IN,
    UNIT_FT,
    UNIT_YD,
    UNIT_MI,
    UNIT_FUR,
    UNIT_LEA,
    UNIT_AU,
    UNIT_LY,
    UNIT_PC,
    UNIT_KG,
    UNIT_G,
    UNIT_T,
    UNIT_UKTON,
    UNIT_USTON,
    UNIT_LB,
    UNIT_ST,
    UNIT_OZ,
    UNIT_OZT,
    UNIT_S,
    UNIT_MIN,
    UNIT_H,
    UNIT_DAY,
    UNIT_MS,
    UNIT_US,
    UNIT_NS,
    UNIT_PS,
    UNIT_A,
    UNIT_K,
    UNIT_C,
    UNIT_F,
    UNIT_DC,
    UNIT_DF,
    UNIT_MOL,
    UNIT_CD,
    UNIT_RAD,
    UNIT_DEG,
    UNIT_ARCMIN,
    UNIT_ARCSEC,
    UNIT_L,
    UNIT_ML,
    UNIT_UKGAL,
    UNIT_USGAL,
    UNIT_UKPT,
    UNIT_USPT,
    UNIT_UKFLOZ,
    UNIT_USFLOZ,
    UNIT_N,
    UNIT_J,
    UNIT_EV,
    UNIT_GEV,
    UNIT_LBF,
    UNIT_FTLBF,
    UNIT_HZ,
    UNIT_W,
    UNIT_PA,
    UNIT_PERCENT,
    UNIT_COUNT
};

typedef struct {
    const char *name;
    const char *numerator;
    const char *denominator;
    const char *offset;
    int8_t dimensions[QUANTITY_AXES];
    int8_t pi;
    bool point;
} quantity_definition_t;

static const quantity_definition_t definitions[UNIT_COUNT] = {
    [UNIT_ONE]     = {"1",           "1", "1", "0", {0}, 0, false},
    [UNIT_M]       = {"m",           "1", "1", "0", {1}, 0, false},
    [UNIT_CM]      = {"cm",          "1", "100", "0", {1}, 0, false},
    [UNIT_MM]      = {"mm",          "1", "1000", "0", {1}, 0, false},
    [UNIT_KM]      = {"km",          "1000", "1", "0", {1}, 0, false},
    [UNIT_IN]      = {"in",          "127", "5000", "0", {1}, 0, false},
    [UNIT_FT]      = {"ft",          "381", "1250", "0", {1}, 0, false},
    [UNIT_YD]      = {"yd",          "1143", "1250", "0", {1}, 0, false},
    [UNIT_MI]      = {"mi",          "201168", "125", "0", {1}, 0, false},
    [UNIT_FUR]     = {"fur",         "25146", "125", "0", {1}, 0, false},
    [UNIT_LEA]     = {"lea",         "603504", "125", "0", {1}, 0, false},
    [UNIT_AU]      = {"au",          "1", "1", "0", {1}, 0, false},
    [UNIT_LY]      = {"ly",          "1", "1", "0", {1}, 0, false},
    [UNIT_PC]      = {"pc",          "648000", "1", "0", {1}, -1, false},
    [UNIT_KG]      = {"kg",          "1", "1", "0", {0, 1}, 0, false},
    [UNIT_G]       = {"g",           "1", "1000", "0", {0, 1}, 0, false},
    [UNIT_T]      = {"t",           "1000", "1", "0", {0, 1}, 0, false},
    [UNIT_UKTON]  = {"uk_ton",      "317514659", "312500", "0", {0, 1}, 0, false},
    [UNIT_USTON]  = {"us_ton",      "45359237", "50000", "0", {0, 1}, 0, false},
    [UNIT_LB]      = {"lb",          "45359237", "100000000", "0", {0, 1}, 0, false},
    [UNIT_ST]      = {"st",          "317514659", "50000000", "0", {0, 1}, 0, false},
    [UNIT_OZ]      = {"oz",          "45359237", "1600000000", "0", {0, 1}, 0, false},
    [UNIT_OZT]     = {"oz_t",        "19439673", "625000000", "0", {0, 1}, 0, false},
    [UNIT_S]       = {"s",           "1", "1", "0", {0, 0, 1}, 0, false},
    [UNIT_MIN]     = {"min",         "60", "1", "0", {0, 0, 1}, 0, false},
    [UNIT_H]       = {"h",           "3600", "1", "0", {0, 0, 1}, 0, false},
    [UNIT_DAY]     = {"d",           "86400", "1", "0", {0, 0, 1}, 0, false},
    [UNIT_MS]      = {"ms",          "1", "1000", "0", {0, 0, 1}, 0, false},
    [UNIT_US]      = {"us",          "1", "1000000", "0", {0, 0, 1}, 0, false},
    [UNIT_NS]      = {"ns",          "1", "1000000000", "0", {0, 0, 1}, 0, false},
    [UNIT_PS]      = {"ps",          "1", "1000000000000", "0", {0, 0, 1}, 0, false},
    [UNIT_A]       = {"A",           "1", "1", "0", {0, 0, 0, 1}, 0, false},
    [UNIT_K]       = {"K",           "1", "1", "0", {0, 0, 0, 0, 1}, 0, true},
    [UNIT_C]       = {"degC",        "1", "1", "5463/20", {0, 0, 0, 0, 1}, 0, true},
    [UNIT_F]       = {"degF",        "5", "9", "45967/180", {0, 0, 0, 0, 1}, 0, true},
    [UNIT_DC]      = {"delta_degC",  "1", "1", "0", {0, 0, 0, 0, 1}, 0, false},
    [UNIT_DF]      = {"delta_degF",  "5", "9", "0", {0, 0, 0, 0, 1}, 0, false},
    [UNIT_MOL]     = {"mol",         "1", "1", "0", {0, 0, 0, 0, 0, 1}, 0, false},
    [UNIT_CD]      = {"cd",          "1", "1", "0", {0, 0, 0, 0, 0, 0, 1}, 0, false},
    [UNIT_RAD]     = {"rad",         "1", "1", "0", {0, 0, 0, 0, 0, 0, 0, 1}, 0, false},
    [UNIT_DEG]     = {"deg",         "1", "180", "0", {0, 0, 0, 0, 0, 0, 0, 1}, 1, false},
    [UNIT_ARCMIN]  = {"arcmin",      "1", "10800", "0", {0, 0, 0, 0, 0, 0, 0, 1}, 1, false},
    [UNIT_ARCSEC]  = {"arcsec",      "1", "648000", "0", {0, 0, 0, 0, 0, 0, 0, 1}, 1, false},
    [UNIT_L]       = {"L",           "1", "1000", "0", {3}, 0, false},
    [UNIT_ML]      = {"mL",          "1", "1000000", "0", {3}, 0, false},
    [UNIT_UKGAL]   = {"uk_gal",      "454609", "100000000", "0", {3}, 0, false},
    [UNIT_USGAL]   = {"us_gal",      "473176473", "125000000000", "0", {3}, 0, false},
    [UNIT_UKPT]    = {"uk_pt",       "454609", "800000000", "0", {3}, 0, false},
    [UNIT_USPT]    = {"us_pt",       "473176473", "1000000000000", "0", {3}, 0, false},
    [UNIT_UKFLOZ]  = {"uk_fl_oz",    "454609", "16000000000", "0", {3}, 0, false},
    [UNIT_USFLOZ]  = {"us_fl_oz",    "473176473", "16000000000000", "0", {3}, 0, false},
    [UNIT_N]       = {"N",           "1", "1", "0", {1, 1, -2}, 0, false},
    [UNIT_J]       = {"J",           "1", "1", "0", {2, 1, -2}, 0, false},
    [UNIT_EV]      = {"eV",          "1", "1", "0", {2, 1, -2}, 0, false},
    [UNIT_GEV]     = {"GeV",         "1000000000", "1", "0", {2, 1, -2}, 0, false},
    [UNIT_LBF]     = {"lbf",         "45359237", "100000000", "0", {1, 1, -2}, 0, false},
    [UNIT_FTLBF]   = {"ft_lbf",      "17281869297", "125000000000", "0", {2, 1, -2}, 0, false},
    [UNIT_HZ]      = {"Hz",          "1", "1", "0", {0, 0, -1}, 0, false},
    [UNIT_W]       = {"W",           "1", "1", "0", {2, 1, -3}, 0, false},
    [UNIT_PA]      = {"Pa",          "1", "1", "0", {-1, 1, -2}, 0, false},
    [UNIT_PERCENT] = {"percent",     "1", "100", "0", {0}, 0, false},
};

typedef struct {
    const char *name;
    unsigned unit;
} quantity_alias_t;

static const uint32_t bucket_seeds[61] = {
    5, 15, 5, 1, 1, 44, 8, 10, 38, 25, 5,
    3, 17, 13, 12, 4, 31, 21, 38, 11, 10, 22,
    29, 1, 97, 9, 74, 37, 9, 78, 2, 17, 3,
    289, 45, 1, 3, 6, 27, 5, 8, 4, 40, 1,
    4, 68, 23, 2, 10, 1, 23, 37, 4, 6, 1,
    11, 2, 49, 35, 76, 2,
};

static const quantity_alias_t aliases[317] = {
    [  0] = {"ozt",                     UNIT_OZT},
    [  1] = {"foot",                    UNIT_FT},
    [  2] = {"eV",                      UNIT_EV},
    [  3] = {"British tons",            UNIT_UKTON},
    [  4] = {"yd",                      UNIT_YD},
    [  5] = {"stone",                   UNIT_ST},
    [  6] = {"millilitre",              UNIT_ML},
    [  7] = {"giga-electronvolt",       UNIT_GEV},
    [  8] = {"liters",                  UNIT_L},
    [  9] = {"US pint",                 UNIT_USPT},
    [ 10] = {"delta_degC",              UNIT_DC},
    [ 11] = {"foot-pounds",             UNIT_FTLBF},
    [ 12] = {"UK fl oz",                UNIT_UKFLOZ},
    [ 14] = {"mile",                    UNIT_MI},
    [ 15] = {"UK fluid oz",             UNIT_UKFLOZ},
    [ 16] = {"kilogram",                UNIT_KG},
    [ 17] = {"imperial ton",            UNIT_UKTON},
    [ 18] = {"UK gallons",              UNIT_UKGAL},
    [ 19] = {"arc second",              UNIT_ARCSEC},
    [ 20] = {"ms",                      UNIT_MS},
    [ 22] = {"hertz",                   UNIT_HZ},
    [ 23] = {"1",                       UNIT_ONE},
    [ 24] = {"N",                       UNIT_N},
    [ 26] = {"metric tons",             UNIT_T},
    [ 27] = {"US fl oz",                UNIT_USFLOZ},
    [ 29] = {"electron volt",           UNIT_EV},
    [ 30] = {"centimetre",              UNIT_CM},
    [ 31] = {"short-ton",               UNIT_USTON},
    [ 33] = {"s",                       UNIT_S},
    [ 35] = {"us_ton",                  UNIT_USTON},
    [ 36] = {"troy ounce",              UNIT_OZT},
    [ 37] = {"leagues",                 UNIT_LEA},
    [ 38] = {"us fluid oz",             UNIT_USFLOZ},
    [ 39] = {"millisecond",             UNIT_MS},
    [ 40] = {"us pint",                 UNIT_USPT},
    [ 41] = {"centimetres",             UNIT_CM},
    [ 42] = {"giga electronvolt",       UNIT_GEV},
    [ 44] = {"pascal",                  UNIT_PA},
    [ 45] = {"litres",                  UNIT_L},
    [ 46] = {"joule",                   UNIT_J},
    [ 47] = {"hr",                      UNIT_H},
    [ 48] = {"seconds",                 UNIT_S},
    [ 49] = {"l",                       UNIT_L},
    [ 50] = {"A",                       UNIT_A},
    [ 51] = {"°F",                      UNIT_F},
    [ 52] = {"US fluid ounces",         UNIT_USFLOZ},
    [ 53] = {"microsecond",             UNIT_US},
    [ 55] = {"Pa",                      UNIT_PA},
    [ 57] = {"cd",                      UNIT_CD},
    [ 58] = {"hour",                    UNIT_H},
    [ 59] = {"long ton",                UNIT_UKTON},
    [ 60] = {"stones",                  UNIT_ST},
    [ 61] = {"light year",              UNIT_LY},
    [ 62] = {"fur",                     UNIT_FUR},
    [ 64] = {"US liquid pint",          UNIT_USPT},
    [ 65] = {"electron-volt",           UNIT_EV},
    [ 66] = {"grams",                   UNIT_G},
    [ 67] = {"degree",                  UNIT_DEG},
    [ 68] = {"rad",                     UNIT_RAD},
    [ 69] = {"delta_degF",              UNIT_DF},
    [ 70] = {"light years",             UNIT_LY},
    [ 71] = {"ly",                      UNIT_LY},
    [ 72] = {"microseconds",            UNIT_US},
    [ 73] = {"foot-pound",              UNIT_FTLBF},
    [ 74] = {"metre",                   UNIT_M},
    [ 76] = {"cm",                      UNIT_CM},
    [ 77] = {"Hz",                      UNIT_HZ},
    [ 78] = {"degrees",                 UNIT_DEG},
    [ 79] = {"newton",                  UNIT_N},
    [ 80] = {"ounces",                  UNIT_OZ},
    [ 81] = {"electronvolt",            UNIT_EV},
    [ 82] = {"Fahrenheit",              UNIT_F},
    [ 83] = {"light-years",             UNIT_LY},
    [ 84] = {"uk_fl_oz",                UNIT_UKFLOZ},
    [ 85] = {"arcminute",               UNIT_ARCMIN},
    [ 86] = {"day",                     UNIT_DAY},
    [ 87] = {"deg",                     UNIT_DEG},
    [ 88] = {"UK fluid ounce",          UNIT_UKFLOZ},
    [ 89] = {"US fluid ounce",          UNIT_USFLOZ},
    [ 90] = {"meter",                   UNIT_M},
    [ 91] = {"arcminutes",              UNIT_ARCMIN},
    [ 92] = {"ounce",                   UNIT_OZ},
    [ 93] = {"moles",                   UNIT_MOL},
    [ 95] = {"troy ounces",             UNIT_OZT},
    [ 96] = {"US fluid oz",             UNIT_USFLOZ},
    [ 97] = {"mi",                      UNIT_MI},
    [100] = {"′",                       UNIT_ARCMIN},
    [101] = {"degC",                    UNIT_C},
    [103] = {"hours",                   UNIT_H},
    [104] = {"astronomical unit",       UNIT_AU},
    [105] = {"inches",                  UNIT_IN},
    [108] = {"mol",                     UNIT_MOL},
    [109] = {"giga electronvolts",      UNIT_GEV},
    [110] = {"AU",                      UNIT_AU},
    [111] = {"US tons",                 UNIT_USTON},
    [112] = {"radian",                  UNIT_RAD},
    [113] = {"ft_lbf",                  UNIT_FTLBF},
    [115] = {"h",                       UNIT_H},
    [116] = {"degF",                    UNIT_F},
    [117] = {"gram",                    UNIT_G},
    [119] = {"uk fluid oz",             UNIT_UKFLOZ},
    [120] = {"league",                  UNIT_LEA},
    [121] = {"foot pound",              UNIT_FTLBF},
    [122] = {"watt",                    UNIT_W},
    [123] = {"Δ°F",                     UNIT_DF},
    [124] = {"imperial gallon",         UNIT_UKGAL},
    [125] = {"giga electron volt",      UNIT_GEV},
    [127] = {"µs",                      UNIT_US},
    [128] = {"feet",                    UNIT_FT},
    [129] = {"amperes",                 UNIT_A},
    [132] = {"newtons",                 UNIT_N},
    [133] = {"imperial tons",           UNIT_UKTON},
    [135] = {"us_fl_oz",                UNIT_USFLOZ},
    [136] = {"deg F",                   UNIT_F},
    [137] = {"ns",                      UNIT_NS},
    [138] = {"in",                      UNIT_IN},
    [139] = {"kilometres",              UNIT_KM},
    [140] = {"us_gal",                  UNIT_USGAL},
    [141] = {"ampere",                  UNIT_A},
    [143] = {"yards",                   UNIT_YD},
    [144] = {"kg",                      UNIT_KG},
    [145] = {"pounds",                  UNIT_LB},
    [146] = {"short tons",              UNIT_USTON},
    [148] = {"us",                      UNIT_US},
    [149] = {"British ton",             UNIT_UKTON},
    [150] = {"foot pounds",             UNIT_FTLBF},
    [151] = {"long-ton",                UNIT_UKTON},
    [152] = {"arcmin",                  UNIT_ARCMIN},
    [154] = {"parsecs",                 UNIT_PC},
    [155] = {"Δ°C",                     UNIT_DC},
    [156] = {"arc minute",              UNIT_ARCMIN},
    [157] = {"us fluid ounces",         UNIT_USFLOZ},
    [158] = {"lea",                     UNIT_LEA},
    [160] = {"imperial gallons",        UNIT_UKGAL},
    [161] = {"parsec",                  UNIT_PC},
    [162] = {"GeV",                     UNIT_GEV},
    [163] = {"percent",                 UNIT_PERCENT},
    [164] = {"hrs",                     UNIT_H},
    [165] = {"UK tons",                 UNIT_UKTON},
    [166] = {"us pints",                UNIT_USPT},
    [167] = {"uk ton",                  UNIT_UKTON},
    [169] = {"d",                       UNIT_DAY},
    [170] = {"joules",                  UNIT_J},
    [171] = {"arcsecond",               UNIT_ARCSEC},
    [172] = {"nanoseconds",             UNIT_NS},
    [173] = {"K",                       UNIT_K},
    [174] = {"imperial fluid ounce",    UNIT_UKFLOZ},
    [175] = {"ml",                      UNIT_ML},
    [176] = {"US liquid gallon",        UNIT_USGAL},
    [178] = {"lb",                      UNIT_LB},
    [181] = {"kilograms",               UNIT_KG},
    [182] = {"inch",                    UNIT_IN},
    [183] = {"liter",                   UNIT_L},
    [184] = {"uk gallons",              UNIT_UKGAL},
    [185] = {"electronvolts",           UNIT_EV},
    [187] = {"min",                     UNIT_MIN},
    [188] = {"uk fluid ounce",          UNIT_UKFLOZ},
    [189] = {"astronomical units",      UNIT_AU},
    [190] = {"lbs",                     UNIT_LB},
    [191] = {"metric-tons",             UNIT_T},
    [192] = {"days",                    UNIT_DAY},
    [193] = {"uk fluid ounces",         UNIT_UKFLOZ},
    [195] = {"candela",                 UNIT_CD},
    [196] = {"uk_pt",                   UNIT_UKPT},
    [197] = {"giga-electronvolts",      UNIT_GEV},
    [198] = {"mL",                      UNIT_ML},
    [199] = {"US pints",                UNIT_USPT},
    [200] = {"UK fluid ounces",         UNIT_UKFLOZ},
    [201] = {"radians",                 UNIT_RAD},
    [202] = {"giga electron volts",     UNIT_GEV},
    [203] = {"short ton",               UNIT_USTON},
    [205] = {"UK pints",                UNIT_UKPT},
    [206] = {"litre",                   UNIT_L},
    [208] = {"arc seconds",             UNIT_ARCSEC},
    [209] = {"deg C",                   UNIT_C},
    [210] = {"uk tons",                 UNIT_UKTON},
    [211] = {"st",                      UNIT_ST},
    [212] = {"troy oz",                 UNIT_OZT},
    [213] = {"sec",                     UNIT_S},
    [214] = {"yard",                    UNIT_YD},
    [215] = {"US gallon",               UNIT_USGAL},
    [216] = {"uk_gal",                  UNIT_UKGAL},
    [217] = {"oz",                      UNIT_OZ},
    [218] = {"ps",                      UNIT_PS},
    [219] = {"electron-volts",          UNIT_EV},
    [220] = {"nanosecond",              UNIT_NS},
    [222] = {"″",                       UNIT_ARCSEC},
    [223] = {"ft",                      UNIT_FT},
    [225] = {"millimetres",             UNIT_MM},
    [227] = {"arcseconds",              UNIT_ARCSEC},
    [229] = {"arcsec",                  UNIT_ARCSEC},
    [230] = {"picosecond",              UNIT_PS},
    [231] = {"picoseconds",             UNIT_PS},
    [232] = {"L",                       UNIT_L},
    [234] = {"oz_t",                    UNIT_OZT},
    [235] = {"kilometre",               UNIT_KM},
    [236] = {"UK gallon",               UNIT_UKGAL},
    [237] = {"pascals",                 UNIT_PA},
    [238] = {"dimensionless",           UNIT_ONE},
    [239] = {"candelas",                UNIT_CD},
    [241] = {"pc",                      UNIT_PC},
    [243] = {"°",                       UNIT_DEG},
    [245] = {"%",                       UNIT_PERCENT},
    [246] = {"us tons",                 UNIT_USTON},
    [248] = {"long tons",               UNIT_UKTON},
    [250] = {"km",                      UNIT_KM},
    [252] = {"metric ton",              UNIT_T},
    [253] = {"tonne",                   UNIT_T},
    [254] = {"gigaelectronvolts",       UNIT_GEV},
    [255] = {"g",                       UNIT_G},
    [256] = {"UK ton",                  UNIT_UKTON},
    [257] = {"long-tons",               UNIT_UKTON},
    [258] = {"furlongs",                UNIT_FUR},
    [259] = {"pound-force",             UNIT_LBF},
    [260] = {"us gallons",              UNIT_USGAL},
    [261] = {"furlong",                 UNIT_FUR},
    [262] = {"UK pint",                 UNIT_UKPT},
    [263] = {"au",                      UNIT_AU},
    [264] = {"°C",                      UNIT_C},
    [265] = {"minute",                  UNIT_MIN},
    [266] = {"uk_ton",                  UNIT_UKTON},
    [267] = {"millimetre",              UNIT_MM},
    [268] = {"us ton",                  UNIT_USTON},
    [269] = {"uk pints",                UNIT_UKPT},
    [270] = {"second",                  UNIT_S},
    [271] = {"metric-ton",              UNIT_T},
    [272] = {"US ton",                  UNIT_USTON},
    [273] = {"mole",                    UNIT_MOL},
    [274] = {"tonnes",                  UNIT_T},
    [275] = {"pounds-force",            UNIT_LBF},
    [276] = {"lbf",                     UNIT_LBF},
    [278] = {"t",                       UNIT_T},
    [279] = {"us fluid ounce",          UNIT_USFLOZ},
    [280] = {"kelvin",                  UNIT_K},
    [282] = {"W",                       UNIT_W},
    [283] = {"electron volts",          UNIT_EV},
    [284] = {"us_pt",                   UNIT_USPT},
    [285] = {"metres",                  UNIT_M},
    [286] = {"imperial pint",           UNIT_UKPT},
    [287] = {"watts",                   UNIT_W},
    [288] = {"milliseconds",            UNIT_MS},
    [289] = {"mm",                      UNIT_MM},
    [291] = {"miles",                   UNIT_MI},
    [292] = {"millilitres",             UNIT_ML},
    [293] = {"gigaelectronvolt",        UNIT_GEV},
    [296] = {"light-year",              UNIT_LY},
    [299] = {"pound",                   UNIT_LB},
    [300] = {"J",                       UNIT_J},
    [301] = {"short-tons",              UNIT_USTON},
    [302] = {"kelvins",                 UNIT_K},
    [303] = {"US gallons",              UNIT_USGAL},
    [305] = {"μs",                      UNIT_US},
    [306] = {"uk gallon",               UNIT_UKGAL},
    [307] = {"Celsius",                 UNIT_C},
    [308] = {"arc minutes",             UNIT_ARCMIN},
    [310] = {"meters",                  UNIT_M},
    [311] = {"us gallon",               UNIT_USGAL},
    [312] = {"uk pint",                 UNIT_UKPT},
    [314] = {"m",                       UNIT_M},
    [316] = {"minutes",                 UNIT_MIN},
};

/* Direct indexing keeps the shared physical definitions out of duplicated scale literals. */
static const number_t *const quantity_scale_constants[UNIT_COUNT][2] = {
    [UNIT_AU]    = {&NUM_ASTRONOMICAL_UNIT, NULL},
    [UNIT_LY]    = {&NUM_SPEED_OF_LIGHT,    &NUM_JULIAN_YEAR},
    [UNIT_PC]    = {&NUM_ASTRONOMICAL_UNIT, NULL},
    [UNIT_EV]    = {&NUM_ELEMENTARY_CHARGE, NULL},
    [UNIT_GEV]   = {&NUM_ELEMENTARY_CHARGE, NULL},
    [UNIT_LBF]   = {&NUM_STANDARD_GRAVITY,  NULL},
    [UNIT_FTLBF] = {&NUM_STANDARD_GRAVITY,  NULL},
};

static bool quantity_apply_scale_constant(quantity_unit_t *unit, const number_t *constant)
{
    if (!constant)
        return true;
    number_t numerator = {0}, denominator = {0};
    if (!num_get_fraction_parts(*constant, &numerator, &denominator))
        return false;
    number_t top = num_mul(unit->numerator, numerator);
    number_t bottom = num_mul(unit->denominator, denominator);
    num_destroy(&numerator);
    num_destroy(&denominator);
    num_destroy(&unit->numerator);
    num_destroy(&unit->denominator);
    unit->numerator = top;
    unit->denominator = bottom;
    return num_is_finite(top) && num_is_finite(bottom);
}

static uint32_t quantity_alias_hash(const char *name, size_t length, uint32_t seed)
{
    for (size_t i = 0; i < length; ++i)
        seed = (seed ^ (unsigned char)name[i]) * UINT32_C(16777619);
    return seed;
}

bool quantity_unit_lookup(quantity_unit_t *unit, const char *name, size_t length)
{
    uint32_t bucket = quantity_alias_hash(name, length, UINT32_C(2166136261)) % 61;
    uint32_t slot = quantity_alias_hash(name, length, bucket_seeds[bucket]) % 317;
    const quantity_alias_t *alias = &aliases[slot];
    if (!alias->name || strlen(alias->name) != length || memcmp(alias->name, name, length))
        return false;
    const quantity_definition_t *definition = &definitions[alias->unit];
    quantity_unit_init(unit);
    num_destroy(&unit->numerator);
    num_destroy(&unit->denominator);
    num_destroy(&unit->offset);
    unit->numerator = num_create_from_string(definition->numerator);
    unit->denominator = num_create_from_string(definition->denominator);
    unit->offset = num_create_from_string(definition->offset);
    if (!quantity_apply_scale_constant(unit, quantity_scale_constants[alias->unit][0]) ||
        !quantity_apply_scale_constant(unit, quantity_scale_constants[alias->unit][1])) {
        quantity_unit_clear(unit);
        return false;
    }
    for (unsigned i = 0; i < QUANTITY_AXES; ++i)
        unit->powers[i].numerator = definition->dimensions[i];
    unit->powers[QUANTITY_PI].numerator = definition->pi;
    unit->point_allowed = definition->point;
    unit->name = string_new_with(definition->name);
    return unit->name != NULL;
}
