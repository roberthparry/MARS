#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "almanac.h"
#include "almanac_cartesian.h"
#include "almanac_internal.h"
#include "jurisdiction.h"
#include "sqlite.h"
#include "ustring.h"

typedef struct almanac_nutation_term_t {
    int multiplier_L;
    int multiplier_Lprime;
    int multiplier_omega;
    double sin_coeff_arcsec;
    double cos_coeff_arcsec;
} almanac_nutation_term_t;

typedef enum almanac_body_ref_id_t {
    ALMANAC_BODY_REF_ID_NONE = 0,
    ALMANAC_BODY_REF_ID_SUN = 1,
    ALMANAC_BODY_REF_ID_EARTH_BARYCENTER = 2,
    ALMANAC_BODY_REF_ID_MOON = 3,
    ALMANAC_BODY_REF_ID_MERCURY = 4,
    ALMANAC_BODY_REF_ID_VENUS = 5,
    ALMANAC_BODY_REF_ID_MARS = 6,
    ALMANAC_BODY_REF_ID_JUPITER = 7,
    ALMANAC_BODY_REF_ID_SATURN = 8,
    ALMANAC_BODY_REF_ID_EARTH = 9
} almanac_body_ref_id_t;

#define ALMANAC_BODY_REF_ID_LIMIT (ALMANAC_BODY_REF_ID_EARTH + 1)

typedef enum almanac_model_kind_t {
    ALMANAC_MODEL_KIND_UNKNOWN = 0,
    ALMANAC_MODEL_KIND_FIXED_EQUATORIAL,
    ALMANAC_MODEL_KIND_CHEBYSHEV_POSITION,
    ALMANAC_MODEL_KIND_LUNAR_CHEBYSHEV
} almanac_model_kind_t;

typedef enum almanac_brightness_model_t {
    ALMANAC_BRIGHTNESS_MODEL_UNKNOWN = 0,
    ALMANAC_BRIGHTNESS_MODEL_NONE,
    ALMANAC_BRIGHTNESS_MODEL_CATALOGUED,
    ALMANAC_BRIGHTNESS_MODEL_SUN_DISTANCE,
    ALMANAC_BRIGHTNESS_MODEL_PLANETARY_PHASE,
    ALMANAC_BRIGHTNESS_MODEL_LUNAR_PHASE
} almanac_brightness_model_t;

typedef struct almanac_model_row_t {
    almanac_body_id_t body_id;
    almanac_body_kind_t body_kind;
    almanac_model_kind_t model_kind;
    almanac_brightness_model_t brightness_model;
    int sort_order;
    double magnitude_constant;
    double magnitude_linear;
    double magnitude_quadratic;
    double magnitude_cubic;
    double magnitude_quartic;
    double fixed_epoch_jd;
    double fixed_ra_hours;
    double fixed_dec_degrees;
    double fixed_pm_ra_mas_per_year;
    double fixed_pm_dec_mas_per_year;
    double orbit_epoch_jd;
    double N0;
    double N_dot;
    double i0;
    double i_dot;
    double w0;
    double w_dot;
    double a0;
    double a_dot;
    double e0;
    double e_dot;
    double M0;
    double M_dot;
} almanac_model_row_t;

struct _almanac_entry_t {
    almanac_body_id_t body_id;
    almanac_body_kind_t body_kind;
    double moment_jd;
    double gha_aries_degrees;
    double sha_degrees;
    double declination_degrees;
    double right_ascension_hours;
    double geocentric_distance_au;
    double heliocentric_distance_au;
    double phase_angle_degrees;
    double visual_magnitude;
};

typedef struct almanac_state_t {
    cartesian3_t geocentric_equatorial_au;
    cartesian3_t geocentric_ecliptic_au;
    bool has_geocentric_vector;
    bool has_ecliptic_vector;
    bool has_direction_vector;
    bool apparent_place_complete;
    double right_ascension_hours;
    double declination_degrees;
} almanac_state_t;

typedef struct almanac_horizon_geometry_t {
    double altitude_degrees;
    double azimuth_degrees;
    double semi_diameter_degrees;
} almanac_horizon_geometry_t;

typedef struct almanac_rise_set_day_t {
    double start_jd;
    double end_jd;
    double offset_guess_hours;
    long local_jdn;
} almanac_rise_set_day_t;

#define ALMANAC_CHEB_COMPONENT_COUNT 3
#define ALMANAC_FRAME_ROTATION_COMPONENT_COUNT 9
#define ALMANAC_CHEB_MAX_COEFF_COUNT 33
#define ALMANAC_NUTATION_COEFF_COUNT 8

static const double ALMANAC_NUTATION_START_JD = 2287185.5;
static const double ALMANAC_NUTATION_END_JD = 2688952.5;
static const double ALMANAC_NUTATION_SEGMENT_DAYS = 365.25 * 5.0;
static const double ALMANAC_FIXED_EQUATORIAL_EPOCH_JD = 2451545.0;
static const double ALMANAC_GMST_BASE_DEG = 280.46061837;
static const double ALMANAC_GMST_RATE_DEG_PER_DAY = 360.98564736629;
static const double ALMANAC_GMST_QUADRATIC_DEG = 0.000387933;
static const double ALMANAC_GMST_CUBIC_DIVISOR = 38710000.0;
static const double ALMANAC_MEAN_OBLIQUITY_C0_ARCSEC = 84381.448;
static const double ALMANAC_MEAN_OBLIQUITY_C1_ARCSEC = -46.8150;
static const double ALMANAC_MEAN_OBLIQUITY_C2_ARCSEC = -0.00059;
static const double ALMANAC_MEAN_OBLIQUITY_C3_ARCSEC = 0.001813;
static const double ALMANAC_PRECESSION_ZETA_C1_DEG = 0.6406161;
static const double ALMANAC_PRECESSION_ZETA_C2_DEG = 0.0000839;
static const double ALMANAC_PRECESSION_ZETA_C3_DEG = 0.0000050;
static const double ALMANAC_PRECESSION_THETA_C1_DEG = 0.5567530;
static const double ALMANAC_PRECESSION_THETA_C2_DEG = -0.0001185;
static const double ALMANAC_PRECESSION_THETA_C3_DEG = -0.0000116;
static const double ALMANAC_PRECESSION_Z_C1_DEG = 0.6406161;
static const double ALMANAC_PRECESSION_Z_C2_DEG = 0.0003041;
static const double ALMANAC_PRECESSION_Z_C3_DEG = 0.0000051;
static const double ALMANAC_OMEGA_C0_DEG = 125.04452;
static const double ALMANAC_OMEGA_C1_DEG_PER_CENTURY = -1934.136261;
static const double ALMANAC_OMEGA_C2_DEG_PER_CENTURY2 = 0.0020708;
static const double ALMANAC_OMEGA_C3_CENTURY3_DIVISOR = 450000.0;
static const double ALMANAC_SOLAR_MEAN_LONGITUDE_C0_DEG = 280.4665;
static const double ALMANAC_SOLAR_MEAN_LONGITUDE_C1_DEG_PER_CENTURY = 36000.7698;
static const double ALMANAC_LUNAR_MEAN_LONGITUDE_C0_DEG = 218.3165;
static const double ALMANAC_LUNAR_MEAN_LONGITUDE_C1_DEG_PER_CENTURY = 481267.8813;
static const double ALMANAC_MOON_EARTH_SYSTEM_MASS_FRACTION = 0.01215058426954;
static const double ALMANAC_SPEED_OF_LIGHT_AU_PER_DAY = 173.1446326846693;

static bool almanac_bisect_event_residual(almanac_t *almanac,
                                          double (*residual)(almanac_t *, double, void *),
                                          void *context,
                                          double left_jd,
                                          double right_jd,
                                          double *out_jd);
static bool almanac_body_kind_from_text(const char *text, almanac_body_kind_t *out);
static bool almanac_model_kind_from_text(const char *text, almanac_model_kind_t *out);
static bool almanac_brightness_model_from_text(const char *text, almanac_brightness_model_t *out);

static const char *const ALMANAC_BODY_CODES[ALMANAC_BODY_ID_COUNT] = {
    [ALMANAC_BODY_ID_SUN] = "SUN",
    [ALMANAC_BODY_ID_MOON] = "MOON",
    [ALMANAC_BODY_ID_MERCURY] = "MERCURY",
    [ALMANAC_BODY_ID_VENUS] = "VENUS",
    [ALMANAC_BODY_ID_MARS] = "MARS",
    [ALMANAC_BODY_ID_JUPITER] = "JUPITER",
    [ALMANAC_BODY_ID_SATURN] = "SATURN",
    [ALMANAC_BODY_ID_URANUS] = "URANUS",
    [ALMANAC_BODY_ID_NEPTUNE] = "NEPTUNE",
    [ALMANAC_BODY_ID_ACAMAR] = "ACAMAR",
    [ALMANAC_BODY_ID_ACHERNAR] = "ACHERNAR",
    [ALMANAC_BODY_ID_ACRUX] = "ACRUX",
    [ALMANAC_BODY_ID_ADHARA] = "ADHARA",
    [ALMANAC_BODY_ID_ALNAIR] = "ALNAIR",
    [ALMANAC_BODY_ID_ALDEBARAN] = "ALDEBARAN",
    [ALMANAC_BODY_ID_ALIOTH] = "ALIOTH",
    [ALMANAC_BODY_ID_ALKAID] = "ALKAID",
    [ALMANAC_BODY_ID_ALNILAM] = "ALNILAM",
    [ALMANAC_BODY_ID_ALPHARD] = "ALPHARD",
    [ALMANAC_BODY_ID_ALPHECCA] = "ALPHECCA",
    [ALMANAC_BODY_ID_ALPHERATZ] = "ALPHERATZ",
    [ALMANAC_BODY_ID_ALTAIR] = "ALTAIR",
    [ALMANAC_BODY_ID_ANKAA] = "ANKAA",
    [ALMANAC_BODY_ID_ANTARES] = "ANTARES",
    [ALMANAC_BODY_ID_ARCTURUS] = "ARCTURUS",
    [ALMANAC_BODY_ID_ATRIA] = "ATRIA",
    [ALMANAC_BODY_ID_AVIOR] = "AVIOR",
    [ALMANAC_BODY_ID_BELLATRIX] = "BELLATRIX",
    [ALMANAC_BODY_ID_BETELGEUSE] = "BETELGEUSE",
    [ALMANAC_BODY_ID_CANOPUS] = "CANOPUS",
    [ALMANAC_BODY_ID_CAPELLA] = "CAPELLA",
    [ALMANAC_BODY_ID_DENEB] = "DENEB",
    [ALMANAC_BODY_ID_DENEBOLA] = "DENEBOLA",
    [ALMANAC_BODY_ID_DIPHDA] = "DIPHDA",
    [ALMANAC_BODY_ID_DUBHE] = "DUBHE",
    [ALMANAC_BODY_ID_ELNATH] = "ELNATH",
    [ALMANAC_BODY_ID_ELTANIN] = "ELTANIN",
    [ALMANAC_BODY_ID_ENIF] = "ENIF",
    [ALMANAC_BODY_ID_FOMALHAUT] = "FOMALHAUT",
    [ALMANAC_BODY_ID_GACRUX] = "GACRUX",
    [ALMANAC_BODY_ID_GIENAH] = "GIENAH",
    [ALMANAC_BODY_ID_HADAR] = "HADAR",
    [ALMANAC_BODY_ID_HAMAL] = "HAMAL",
    [ALMANAC_BODY_ID_KAUS_AUSTRALIS] = "KAUS_AUSTRALIS",
    [ALMANAC_BODY_ID_KOCHAB] = "KOCHAB",
    [ALMANAC_BODY_ID_MARKAB] = "MARKAB",
    [ALMANAC_BODY_ID_MENKAR] = "MENKAR",
    [ALMANAC_BODY_ID_MENKENT] = "MENKENT",
    [ALMANAC_BODY_ID_MIAPLACIDUS] = "MIAPLACIDUS",
    [ALMANAC_BODY_ID_MIRFAK] = "MIRFAK",
    [ALMANAC_BODY_ID_NUNKI] = "NUNKI",
    [ALMANAC_BODY_ID_PEACOCK] = "PEACOCK",
    [ALMANAC_BODY_ID_POLARIS] = "POLARIS",
    [ALMANAC_BODY_ID_POLLUX] = "POLLUX",
    [ALMANAC_BODY_ID_PROCYON] = "PROCYON",
    [ALMANAC_BODY_ID_RASALHAGUE] = "RASALHAGUE",
    [ALMANAC_BODY_ID_REGULUS] = "REGULUS",
    [ALMANAC_BODY_ID_RIGEL] = "RIGEL",
    [ALMANAC_BODY_ID_RIGIL_KENTAURUS] = "RIGIL_KENTAURUS",
    [ALMANAC_BODY_ID_SABIK] = "SABIK",
    [ALMANAC_BODY_ID_SCHEDAR] = "SCHEDAR",
    [ALMANAC_BODY_ID_SHAULA] = "SHAULA",
    [ALMANAC_BODY_ID_SIRIUS] = "SIRIUS",
    [ALMANAC_BODY_ID_SPICA] = "SPICA",
    [ALMANAC_BODY_ID_SUHAIL] = "SUHAIL",
    [ALMANAC_BODY_ID_VEGA] = "VEGA",
    [ALMANAC_BODY_ID_ZUBENELGENUBI] = "ZUBENELGENUBI",
    [ALMANAC_BODY_ID_SIGMA_OCTANTIS] = "SIGMA_OCTANTIS"
};

static const char *const ALMANAC_BODY_DISPLAY_NAMES[ALMANAC_BODY_ID_COUNT] = {
    [ALMANAC_BODY_ID_SUN] = "Sun",
    [ALMANAC_BODY_ID_MOON] = "Moon",
    [ALMANAC_BODY_ID_MERCURY] = "Mercury",
    [ALMANAC_BODY_ID_VENUS] = "Venus",
    [ALMANAC_BODY_ID_MARS] = "Mars",
    [ALMANAC_BODY_ID_JUPITER] = "Jupiter",
    [ALMANAC_BODY_ID_SATURN] = "Saturn",
    [ALMANAC_BODY_ID_URANUS] = "Uranus",
    [ALMANAC_BODY_ID_NEPTUNE] = "Neptune",
    [ALMANAC_BODY_ID_ACAMAR] = "Acamar",
    [ALMANAC_BODY_ID_ACHERNAR] = "Achernar",
    [ALMANAC_BODY_ID_ACRUX] = "Acrux",
    [ALMANAC_BODY_ID_ADHARA] = "Adhara",
    [ALMANAC_BODY_ID_ALNAIR] = "Alnair",
    [ALMANAC_BODY_ID_ALDEBARAN] = "Aldebaran",
    [ALMANAC_BODY_ID_ALIOTH] = "Alioth",
    [ALMANAC_BODY_ID_ALKAID] = "Alkaid",
    [ALMANAC_BODY_ID_ALNILAM] = "Alnilam",
    [ALMANAC_BODY_ID_ALPHARD] = "Alphard",
    [ALMANAC_BODY_ID_ALPHECCA] = "Alphecca",
    [ALMANAC_BODY_ID_ALPHERATZ] = "Alpheratz",
    [ALMANAC_BODY_ID_ALTAIR] = "Altair",
    [ALMANAC_BODY_ID_ANKAA] = "Ankaa",
    [ALMANAC_BODY_ID_ANTARES] = "Antares",
    [ALMANAC_BODY_ID_ARCTURUS] = "Arcturus",
    [ALMANAC_BODY_ID_ATRIA] = "Atria",
    [ALMANAC_BODY_ID_AVIOR] = "Avior",
    [ALMANAC_BODY_ID_BELLATRIX] = "Bellatrix",
    [ALMANAC_BODY_ID_BETELGEUSE] = "Betelgeuse",
    [ALMANAC_BODY_ID_CANOPUS] = "Canopus",
    [ALMANAC_BODY_ID_CAPELLA] = "Capella",
    [ALMANAC_BODY_ID_DENEB] = "Deneb",
    [ALMANAC_BODY_ID_DENEBOLA] = "Denebola",
    [ALMANAC_BODY_ID_DIPHDA] = "Diphda",
    [ALMANAC_BODY_ID_DUBHE] = "Dubhe",
    [ALMANAC_BODY_ID_ELNATH] = "Elnath",
    [ALMANAC_BODY_ID_ELTANIN] = "Eltanin",
    [ALMANAC_BODY_ID_ENIF] = "Enif",
    [ALMANAC_BODY_ID_FOMALHAUT] = "Fomalhaut",
    [ALMANAC_BODY_ID_GACRUX] = "Gacrux",
    [ALMANAC_BODY_ID_GIENAH] = "Gienah",
    [ALMANAC_BODY_ID_HADAR] = "Hadar",
    [ALMANAC_BODY_ID_HAMAL] = "Hamal",
    [ALMANAC_BODY_ID_KAUS_AUSTRALIS] = "Kaus Australis",
    [ALMANAC_BODY_ID_KOCHAB] = "Kochab",
    [ALMANAC_BODY_ID_MARKAB] = "Markab",
    [ALMANAC_BODY_ID_MENKAR] = "Menkar",
    [ALMANAC_BODY_ID_MENKENT] = "Menkent",
    [ALMANAC_BODY_ID_MIAPLACIDUS] = "Miaplacidus",
    [ALMANAC_BODY_ID_MIRFAK] = "Mirfak",
    [ALMANAC_BODY_ID_NUNKI] = "Nunki",
    [ALMANAC_BODY_ID_PEACOCK] = "Peacock",
    [ALMANAC_BODY_ID_POLARIS] = "Polaris",
    [ALMANAC_BODY_ID_POLLUX] = "Pollux",
    [ALMANAC_BODY_ID_PROCYON] = "Procyon",
    [ALMANAC_BODY_ID_RASALHAGUE] = "Rasalhague",
    [ALMANAC_BODY_ID_REGULUS] = "Regulus",
    [ALMANAC_BODY_ID_RIGEL] = "Rigel",
    [ALMANAC_BODY_ID_RIGIL_KENTAURUS] = "Rigil Kentaurus",
    [ALMANAC_BODY_ID_SABIK] = "Sabik",
    [ALMANAC_BODY_ID_SCHEDAR] = "Schedar",
    [ALMANAC_BODY_ID_SHAULA] = "Shaula",
    [ALMANAC_BODY_ID_SIRIUS] = "Sirius",
    [ALMANAC_BODY_ID_SPICA] = "Spica",
    [ALMANAC_BODY_ID_SUHAIL] = "Suhail",
    [ALMANAC_BODY_ID_VEGA] = "Vega",
    [ALMANAC_BODY_ID_ZUBENELGENUBI] = "Zubenelgenubi",
    [ALMANAC_BODY_ID_SIGMA_OCTANTIS] = "Sigma Octantis"
};

static const almanac_body_ref_id_t ALMANAC_BODY_REF_IDS[ALMANAC_BODY_ID_COUNT] = {
    [ALMANAC_BODY_ID_SUN] = ALMANAC_BODY_REF_ID_SUN,
    [ALMANAC_BODY_ID_MOON] = ALMANAC_BODY_REF_ID_MOON,
    [ALMANAC_BODY_ID_MERCURY] = ALMANAC_BODY_REF_ID_MERCURY,
    [ALMANAC_BODY_ID_VENUS] = ALMANAC_BODY_REF_ID_VENUS,
    [ALMANAC_BODY_ID_MARS] = ALMANAC_BODY_REF_ID_MARS,
    [ALMANAC_BODY_ID_JUPITER] = ALMANAC_BODY_REF_ID_JUPITER,
    [ALMANAC_BODY_ID_SATURN] = ALMANAC_BODY_REF_ID_SATURN
};

static bool almanac_body_code_equals(const char *left, const char *right)
{
    unsigned char lc;
    unsigned char rc;

    if (!left || !right)
        return false;
    while (*left && *right) {
        lc = (unsigned char)*left++;
        rc = (unsigned char)*right++;
        if (lc == '-' || lc == ' ')
            lc = '_';
        if (rc == '-' || rc == ' ')
            rc = '_';
        if (toupper(lc) != toupper(rc))
            return false;
    }
    return *left == '\0' && *right == '\0';
}

static unsigned int almanac_body_code_hash(const char *body_code)
{
    const char *p;
    unsigned int hash = 0u;

    if (!body_code)
        return 0u;
    for (p = body_code; *p; ++p)
        ++hash;
    for (p = body_code; *p; ++p) {
        unsigned char ch = (unsigned char)*p;

        if (ch == '-' || ch == ' ')
            ch = '_';
        hash = hash * 71u + (unsigned int)toupper(ch);
    }
    return hash;
}

almanac_body_id_t almanac_body_id_from_code(const char *body_code)
{
    static const almanac_body_id_t body_ids_by_hash[275] = {
        [6] = ALMANAC_BODY_ID_SCHEDAR,
        [7] = ALMANAC_BODY_ID_RASALHAGUE,
        [8] = ALMANAC_BODY_ID_HAMAL,
        [10] = ALMANAC_BODY_ID_DIPHDA,
        [13] = ALMANAC_BODY_ID_MENKENT,
        [15] = ALMANAC_BODY_ID_GIENAH,
        [18] = ALMANAC_BODY_ID_BETELGEUSE,
        [20] = ALMANAC_BODY_ID_HADAR,
        [22] = ALMANAC_BODY_ID_MIRFAK,
        [25] = ALMANAC_BODY_ID_ACRUX,
        [29] = ALMANAC_BODY_ID_VENUS,
        [31] = ALMANAC_BODY_ID_SIGMA_OCTANTIS,
        [44] = ALMANAC_BODY_ID_RIGEL,
        [49] = ALMANAC_BODY_ID_SUN,
        [57] = ALMANAC_BODY_ID_POLLUX,
        [61] = ALMANAC_BODY_ID_PEACOCK,
        [65] = ALMANAC_BODY_ID_CANOPUS,
        [66] = ALMANAC_BODY_ID_SUHAIL,
        [74] = ALMANAC_BODY_ID_ALIOTH,
        [75] = ALMANAC_BODY_ID_ALTAIR,
        [79] = ALMANAC_BODY_ID_ACHERNAR,
        [84] = ALMANAC_BODY_ID_ALNAIR,
        [87] = ALMANAC_BODY_ID_ATRIA,
        [89] = ALMANAC_BODY_ID_RIGIL_KENTAURUS,
        [90] = ALMANAC_BODY_ID_REGULUS,
        [91] = ALMANAC_BODY_ID_MENKAR,
        [92] = ALMANAC_BODY_ID_ELNATH,
        [97] = ALMANAC_BODY_ID_MOON,
        [98] = ALMANAC_BODY_ID_ANTARES,
        [105] = ALMANAC_BODY_ID_VEGA,
        [106] = ALMANAC_BODY_ID_NEPTUNE,
        [115] = ALMANAC_BODY_ID_ALDEBARAN,
        [123] = ALMANAC_BODY_ID_JUPITER,
        [133] = ALMANAC_BODY_ID_ALNILAM,
        [134] = ALMANAC_BODY_ID_ENIF,
        [136] = ALMANAC_BODY_ID_MIAPLACIDUS,
        [137] = ALMANAC_BODY_ID_NUNKI,
        [139] = ALMANAC_BODY_ID_DENEBOLA,
        [141] = ALMANAC_BODY_ID_MARS,
        [142] = ALMANAC_BODY_ID_ELTANIN,
        [148] = ALMANAC_BODY_ID_FOMALHAUT,
        [154] = ALMANAC_BODY_ID_ALPHERATZ,
        [166] = ALMANAC_BODY_ID_SIRIUS,
        [167] = ALMANAC_BODY_ID_POLARIS,
        [168] = ALMANAC_BODY_ID_DUBHE,
        [175] = ALMANAC_BODY_ID_BELLATRIX,
        [183] = ALMANAC_BODY_ID_SHAULA,
        [190] = ALMANAC_BODY_ID_SABIK,
        [192] = ALMANAC_BODY_ID_ADHARA,
        [195] = ALMANAC_BODY_ID_GACRUX,
        [202] = ALMANAC_BODY_ID_ARCTURUS,
        [207] = ALMANAC_BODY_ID_ZUBENELGENUBI,
        [211] = ALMANAC_BODY_ID_ACAMAR,
        [212] = ALMANAC_BODY_ID_ALKAID,
        [218] = ALMANAC_BODY_ID_KAUS_AUSTRALIS,
        [220] = ALMANAC_BODY_ID_MARKAB,
        [221] = ALMANAC_BODY_ID_CAPELLA,
        [231] = ALMANAC_BODY_ID_SPICA,
        [235] = ALMANAC_BODY_ID_KOCHAB,
        [239] = ALMANAC_BODY_ID_SATURN,
        [243] = ALMANAC_BODY_ID_DENEB,
        [244] = ALMANAC_BODY_ID_MERCURY,
        [247] = ALMANAC_BODY_ID_PROCYON,
        [256] = ALMANAC_BODY_ID_ALPHARD,
        [258] = ALMANAC_BODY_ID_AVIOR,
        [259] = ALMANAC_BODY_ID_ALPHECCA,
        [266] = ALMANAC_BODY_ID_ANKAA,
        [269] = ALMANAC_BODY_ID_URANUS
    };
    almanac_body_id_t body_id;
    unsigned int hash_index;

    if (!body_code)
        return ALMANAC_BODY_ID_UNKNOWN;
    hash_index = almanac_body_code_hash(body_code) % 275u;
    body_id = body_ids_by_hash[hash_index];
    if (body_id <= ALMANAC_BODY_ID_UNKNOWN || body_id >= ALMANAC_BODY_ID_COUNT)
        return ALMANAC_BODY_ID_UNKNOWN;
    if (!almanac_body_code_equals(body_code, ALMANAC_BODY_CODES[body_id]))
        return ALMANAC_BODY_ID_UNKNOWN;
    return body_id;
}

const char *almanac_body_code(almanac_body_id_t body_id)
{
    if (body_id <= ALMANAC_BODY_ID_UNKNOWN || body_id >= ALMANAC_BODY_ID_COUNT)
        return NULL;
    return ALMANAC_BODY_CODES[body_id];
}

const char *almanac_body_display_name(almanac_body_id_t body_id)
{
    if (body_id <= ALMANAC_BODY_ID_UNKNOWN || body_id >= ALMANAC_BODY_ID_COUNT)
        return NULL;
    return ALMANAC_BODY_DISPLAY_NAMES[body_id];
}

void almanac_entry_dealloc(almanac_entry_t *entry)
{
    free(entry);
}

static double almanac_entry_value_or_nan(const almanac_entry_t *entry, double value)
{
    return entry ? value : NAN;
}

almanac_body_id_t almanac_entry_body_id(const almanac_entry_t *entry)
{
    return entry ? entry->body_id : ALMANAC_BODY_ID_UNKNOWN;
}

almanac_body_kind_t almanac_entry_body_kind(const almanac_entry_t *entry)
{
    return entry ? entry->body_kind : ALMANAC_BODY_STAR;
}

double almanac_entry_moment_jd(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->moment_jd : NAN);
}

double almanac_entry_gha_aries_degrees(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->gha_aries_degrees : NAN);
}

double almanac_entry_sha_degrees(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->sha_degrees : NAN);
}

double almanac_entry_declination_degrees(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->declination_degrees : NAN);
}

double almanac_entry_right_ascension_hours(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->right_ascension_hours : NAN);
}

double almanac_entry_geocentric_distance_au(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->geocentric_distance_au : NAN);
}

double almanac_entry_heliocentric_distance_au(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->heliocentric_distance_au : NAN);
}

double almanac_entry_phase_angle_degrees(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->phase_angle_degrees : NAN);
}

double almanac_entry_visual_magnitude(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->visual_magnitude : NAN);
}

typedef struct almanac_chebyshev_position_segment_t {
    double start_jd;
    double end_jd;
    double reference_jd;
    double radius_days;
    int degree;
    double coeff[ALMANAC_CHEB_COMPONENT_COUNT][ALMANAC_CHEB_MAX_COEFF_COUNT];
} almanac_chebyshev_position_segment_t;

typedef struct almanac_frame_rotation_segment_t {
    double start_jd;
    double end_jd;
    double reference_jd;
    double radius_days;
    int degree;
    double coeff[ALMANAC_FRAME_ROTATION_COMPONENT_COUNT][ALMANAC_CHEB_MAX_COEFF_COUNT];
} almanac_frame_rotation_segment_t;

struct _almanac_t {
    sqlite_t *db;
    string_t *error;
    bool correction_model_loaded;
    almanac_nutation_term_t *nutation_terms;
    size_t nutation_term_count;
    bool chebyshev_position_segment_cached[ALMANAC_BODY_REF_ID_LIMIT];
    almanac_chebyshev_position_segment_t chebyshev_position_segment_cache[ALMANAC_BODY_REF_ID_LIMIT];
    bool frame_rotation_segment_cached;
    almanac_frame_rotation_segment_t frame_rotation_segment_cache;
    bool model_row_cached[ALMANAC_BODY_ID_COUNT];
    almanac_model_row_t model_row_cache[ALMANAC_BODY_ID_COUNT];
};

typedef struct almanac_poly8_t {
    double c0;
    double c1;
    double c2;
    double c3;
    double c4;
    double c5;
    double c6;
    double c7;
} almanac_poly8_t;

typedef struct almanac_nutation_segment_t {
    double start_jd;
    double end_jd;
    double reference_jd;
    double span_days;
    almanac_poly8_t dpsi;
    almanac_poly8_t deps;
} almanac_nutation_segment_t;

static bool almanac_lunar_geocentric_ecliptic_vector(almanac_t *almanac,
                                                     double jd,
                                                     cartesian3_t *out);
static bool almanac_lunar_geocentric_ecliptic_velocity(almanac_t *almanac,
                                                       double jd,
                                                       cartesian3_t *out_velocity_au_per_day);

static bool almanac_unpack_fixed_equatorial_blob(const unsigned char *blob,
                                                 size_t blob_size,
                                                 almanac_model_row_t *out)
{
    const size_t expected_size = sizeof(double) * 4u;

    if (!blob || !out || blob_size != expected_size)
        return false;

    memcpy(&out->fixed_ra_hours, blob + sizeof(double) * 0u, sizeof(double));
    memcpy(&out->fixed_dec_degrees, blob + sizeof(double) * 1u, sizeof(double));
    memcpy(&out->fixed_pm_ra_mas_per_year, blob + sizeof(double) * 2u, sizeof(double));
    memcpy(&out->fixed_pm_dec_mas_per_year, blob + sizeof(double) * 3u, sizeof(double));
    out->fixed_epoch_jd = ALMANAC_FIXED_EQUATORIAL_EPOCH_JD;
    return true;
}

static void almanac_set_error(almanac_t *almanac, const char *message)
{
    if (!almanac || !almanac->error)
        return;
    string_clear(almanac->error);
    (void)string_append_cstr(almanac->error, message ? message : "almanac error");
}

static void almanac_set_sqlite_error(almanac_t *almanac)
{
    const string_t *sqlite_error;

    if (!almanac)
        return;
    sqlite_error = almanac->db ? sqlite_last_error(almanac->db) : NULL;
    almanac_set_error(almanac, sqlite_error ? string_c_str(sqlite_error) : "almanac sqlite error");
}

static double normalize_degrees(double degrees)
{
    double value = fmod(degrees, 360.0);

    if (value < 0.0)
        value += 360.0;
    return value;
}

static double normalize_degrees_signed(double degrees)
{
    double value = fmod(degrees + 180.0, 360.0);

    if (value < 0.0)
        value += 360.0;
    return value - 180.0;
}

bool almanac_body_geographical_position(const almanac_entry_t *body,
                                        almanac_geographical_position_t *out)
{
    double gha_body_degrees;

    if (!body || !out)
        return false;

    gha_body_degrees = normalize_degrees(body->gha_aries_degrees + body->sha_degrees);
    if (!isfinite(body->declination_degrees) || !isfinite(gha_body_degrees))
        return false;

    out->latitude_degrees = body->declination_degrees;
    out->longitude_degrees = normalize_degrees_signed(-gha_body_degrees);
    return true;
}

static double degrees_to_radians(double degrees)
{
    return degrees * (M_PI / 180.0);
}

static double radians_to_degrees(double radians)
{
    return radians * (180.0 / M_PI);
}

static double clamp_unit(double value)
{
    if (value < -1.0)
        return -1.0;
    if (value > 1.0)
        return 1.0;
    return value;
}

static double almanac_disc_coverage_percent(double target_radius,
                                            double covering_radius,
                                            double centre_separation)
{
    double d;
    double target_area;
    double overlap_area;
    double term;

    if (!(target_radius > 0.0) || !(centre_separation >= 0.0))
        return NAN;
    if (!(covering_radius > 0.0))
        return 0.0;

    d = fabs(centre_separation);
    target_area = M_PI * target_radius * target_radius;
    if (d >= target_radius + covering_radius)
        return 0.0;
    if (d <= fabs(target_radius - covering_radius)) {
        overlap_area = M_PI * fmin(target_radius, covering_radius) * fmin(target_radius, covering_radius);
        return 100.0 * overlap_area / target_area;
    }

    term = (-d + target_radius + covering_radius)
         * ( d + target_radius - covering_radius)
         * ( d - target_radius + covering_radius)
         * ( d + target_radius + covering_radius);
    overlap_area =
        target_radius * target_radius *
            acos(clamp_unit((d * d + target_radius * target_radius - covering_radius * covering_radius) /
                            (2.0 * d * target_radius))) +
        covering_radius * covering_radius *
            acos(clamp_unit((d * d + covering_radius * covering_radius - target_radius * target_radius) /
                            (2.0 * d * covering_radius))) -
        0.5 * sqrt(fmax(0.0, term));

    return 100.0 * overlap_area / target_area;
}

static double normalize_radians_positive(double radians)
{
    double value = fmod(radians, 2.0 * M_PI);

    if (value < 0.0)
        value += 2.0 * M_PI;
    return value;
}

static bool almanac_body_id_radius_au(almanac_body_id_t body_id, double *out_radius_au)
{
    static const double AU_PER_KM = 1.0 / 149597870.7;
    static const double radius_km_by_body_id[ALMANAC_BODY_ID_COUNT] = {
        [ALMANAC_BODY_ID_UNKNOWN] = NAN,
        [ALMANAC_BODY_ID_SUN] = 695700.0,
        [ALMANAC_BODY_ID_MOON] = 1737.4,
        [ALMANAC_BODY_ID_MERCURY] = 2439.7,
        [ALMANAC_BODY_ID_VENUS] = 6051.8,
        [ALMANAC_BODY_ID_MARS] = 3389.5,
        [ALMANAC_BODY_ID_JUPITER] = 69911.0,
        [ALMANAC_BODY_ID_SATURN] = 58232.0,
        [ALMANAC_BODY_ID_URANUS] = 25362.0,
        [ALMANAC_BODY_ID_NEPTUNE] = 24622.0
    };
    double radius_km;

    if (!out_radius_au)
        return false;

    if (body_id <= ALMANAC_BODY_ID_UNKNOWN || body_id >= ALMANAC_BODY_ID_COUNT)
        return false;

    radius_km = radius_km_by_body_id[body_id];
    if (!(radius_km > 0.0))
        return false;
    *out_radius_au = radius_km * AU_PER_KM;
    return true;
}

static bool almanac_body_radius_au(const almanac_entry_t *body, double *out_radius_au)
{
    if (!body)
        return false;
    return almanac_body_id_radius_au(body->body_id, out_radius_au);
}

static double almanac_body_semi_diameter_from_distance_degrees(almanac_body_id_t body_id,
                                                              double distance_au)
{
    double radius_au;

    if (!(distance_au > 0.0))
        return NAN;
    if (!almanac_body_id_radius_au(body_id, &radius_au) || !(radius_au > 0.0))
        return NAN;
    if (distance_au < radius_au)
        return NAN;
    return radians_to_degrees(asin(clamp_unit(radius_au / distance_au)));
}

static double angular_separation_degrees(const cartesian3_t *a, const cartesian3_t *b)
{
    double len_a;
    double len_b;
    double cosine_angle;

    if (!a || !b)
        return NAN;
    len_a = cartesian_length(a);
    len_b = cartesian_length(b);
    if (len_a <= 0.0 || len_b <= 0.0)
        return NAN;
    cosine_angle = cartesian_dot(a, b) / (len_a * len_b);
    return radians_to_degrees(acos(clamp_unit(cosine_angle)));
}

static double ecliptic_longitude_degrees(const cartesian3_t *vector)
{
    if (!vector)
        return NAN;
    return normalize_degrees(radians_to_degrees(atan2(vector->y, vector->x)));
}

static double almanac_cheb8_eval(const almanac_poly8_t *poly, double x)
{
    double b_kplus1 = 0.0;
    double b_kplus2 = 0.0;
    double b_k;
    const double *coeffs;
    int i;

    if (!poly)
        return 0.0;
    coeffs = &poly->c0;
    for (i = 7; i >= 1; --i) {
        b_k = 2.0 * x * b_kplus1 - b_kplus2 + coeffs[i];
        b_kplus2 = b_kplus1;
        b_kplus1 = b_k;
    }
    return x * b_kplus1 - b_kplus2 + coeffs[0];
}

static char *dup_c_string(const char *text)
{
    size_t len;
    char *out;

    if (!text)
        return NULL;
    len = strlen(text);
    out = malloc(len + 1u);
    if (!out)
        return NULL;
    memcpy(out, text, len + 1u);
    return out;
}

static char *dup_printf_path3(const char *a, const char *b, const char *c)
{
    size_t len_a;
    size_t len_b;
    size_t len_c;
    char *out;

    if (!a || !b || !c)
        return NULL;
    len_a = strlen(a);
    len_b = strlen(b);
    len_c = strlen(c);
    out = malloc(len_a + len_b + len_c + 1u);
    if (!out)
        return NULL;
    memcpy(out, a, len_a);
    memcpy(out + len_a, b, len_b);
    memcpy(out + len_a + len_b, c, len_c);
    out[len_a + len_b + len_c] = '\0';
    return out;
}

static char *mars_home_path(void)
{
    const char *mars_home = getenv("MARS_HOME");
    const char *home = getenv("HOME");

    if (mars_home && *mars_home)
        return dup_c_string(mars_home);
    if (home && *home)
        return dup_printf_path3(home, "/.mars", "");
    return NULL;
}

static char *almanac_default_db_path(void)
{
    char *mars_home = mars_home_path();
    char *path;

    if (!mars_home)
        return NULL;
    path = dup_printf_path3(mars_home, "/almanac/", "almanac.db");
    free(mars_home);
    return path;
}

static char *almanac_config_path(void)
{
    char *mars_home = mars_home_path();
    char *path;

    if (!mars_home)
        return NULL;
    path = dup_printf_path3(mars_home, "/config/", "almanac-db.env");
    free(mars_home);
    return path;
}

static void trim_ascii_whitespace(char *text)
{
    char *start;
    char *end;

    if (!text || *text == '\0')
        return;
    start = text;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        start++;
    if (start != text)
        memmove(text, start, strlen(start) + 1u);
    end = text + strlen(text);
    while (end > text &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }
    *end = '\0';
}

static char *unquote_shell_value(const char *raw_value)
{
    char *value;
    size_t len;

    if (!raw_value)
        return NULL;
    value = dup_c_string(raw_value);
    if (!value)
        return NULL;
    trim_ascii_whitespace(value);
    len = strlen(value);
    if (len >= 2u &&
        ((value[0] == '"' && value[len - 1u] == '"') ||
         (value[0] == '\'' && value[len - 1u] == '\''))) {
        memmove(value, value + 1, len - 2u);
        value[len - 2u] = '\0';
    }
    return value;
}

static char *almanac_config_lookup(const char *key_name)
{
    FILE *fp;
    char *config_path;
    char line[4096];
    char *result = NULL;

    if (!key_name || *key_name == '\0')
        return NULL;

    config_path = almanac_config_path();
    if (!config_path)
        return NULL;

    fp = fopen(config_path, "r");
    free(config_path);
    if (!fp)
        return NULL;

    while (fgets(line, sizeof(line), fp)) {
        char *eq = strchr(line, '=');
        char *name;

        if (!eq)
            continue;
        *eq = '\0';
        name = line;
        trim_ascii_whitespace(name);
        if (strncmp(name, "export ", 7u) == 0) {
            name += 7;
            trim_ascii_whitespace(name);
        }
        if (*name == '\0' || *name == '#')
            continue;
        if (strcmp(name, key_name) != 0)
            continue;
        result = unquote_shell_value(eq + 1);
        break;
    }

    fclose(fp);
    return result;
}

static string_t *string_new_from_cstr(const char *text)
{
    if (!text || *text == '\0')
        return NULL;
    return string_new_with(text);
}

static bool almanac_unpack_magnitude_coeff_blob(const unsigned char *blob,
                                                size_t blob_size,
                                                almanac_model_row_t *out)
{
    double *coeffs[5];
    size_t i;

    if (!blob || !out || blob_size != 5u * sizeof(double))
        return false;

    coeffs[0] = &out->magnitude_constant;
    coeffs[1] = &out->magnitude_linear;
    coeffs[2] = &out->magnitude_quadratic;
    coeffs[3] = &out->magnitude_cubic;
    coeffs[4] = &out->magnitude_quartic;
    for (i = 0u; i < 5u; ++i)
        memcpy(coeffs[i], blob + i * sizeof(double), sizeof(double));
    return true;
}

static bool almanac_load_nutation_terms(almanac_t *almanac)
{
    static const char *sql =
        "select l_multiplier.multiplier_L, "
        "       lprime_multiplier.multiplier_Lprime, "
        "       omega_multiplier.multiplier_omega, "
        "       sin_coeff.sin_coeff_arcsec, "
        "       cos_coeff.cos_coeff_arcsec "
        "from almanac_nutation_term as term "
        "join almanac_nutation_term_l_multiplier as l_multiplier "
        "  on l_multiplier.term_id = term.term_id "
        "join almanac_nutation_term_lprime_multiplier as lprime_multiplier "
        "  on lprime_multiplier.term_id = term.term_id "
        "join almanac_nutation_term_omega_multiplier as omega_multiplier "
        "  on omega_multiplier.term_id = term.term_id "
        "join almanac_nutation_term_sin_coeff as sin_coeff "
        "  on sin_coeff.term_id = term.term_id "
        "join almanac_nutation_term_cos_coeff as cos_coeff "
        "  on cos_coeff.term_id = term.term_id "
        "join almanac_nutation_term_sort_order as sort_order "
        "  on sort_order.term_id = term.term_id "
        "order by sort_order.sort_order asc, term.term_id asc";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;
    almanac_nutation_term_t *terms = NULL;
    size_t count = 0u;

    if (!almanac || !almanac->db) {
        almanac_set_error(almanac, "invalid nutation model lookup");
        return false;
    }

    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        almanac_set_sqlite_error(almanac);
        return false;
    }

    while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        almanac_nutation_term_t *grown =
            realloc(terms, (count + 1u) * sizeof(*terms));

        if (!grown) {
            sqlite_stmt_finalize(stmt);
            free(terms);
            almanac_set_error(almanac, "failed to allocate nutation term storage");
            return false;
        }
        terms = grown;
        terms[count].multiplier_L = sqlite_stmt_column_int(stmt, 0);
        terms[count].multiplier_Lprime = sqlite_stmt_column_int(stmt, 1);
        terms[count].multiplier_omega = sqlite_stmt_column_int(stmt, 2);
        terms[count].sin_coeff_arcsec = sqlite_stmt_column_double(stmt, 3);
        terms[count].cos_coeff_arcsec = sqlite_stmt_column_double(stmt, 4);
        count++;
    }

    sqlite_stmt_finalize(stmt);
    if (rc != SQLITE_STEP_DONE) {
        free(terms);
        almanac_set_sqlite_error(almanac);
        return false;
    }
    if (count == 0u) {
        free(terms);
        almanac_set_error(almanac, "no nutation terms are configured in the almanac database");
        return false;
    }

    almanac->nutation_terms = terms;
    almanac->nutation_term_count = count;
    return true;
}

static bool almanac_fetch_nutation_segment(almanac_t *almanac,
                                           double jd,
                                           almanac_nutation_segment_t *out)
{
    static const char *sql =
        "select model.model_id, coeff.coefficient_blob "
        "from almanac_nutation_model as model "
        "join almanac_nutation_model_coeff as coeff "
        "  on coeff.model_id = model.model_id "
        "join almanac_nutation_model_sort_order as sort_order "
        "  on sort_order.model_id = model.model_id "
        "where model.model_id = ? "
        "order by sort_order.sort_order asc, model.model_id asc "
        "limit 1";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;
    const unsigned char *blob;
    size_t blob_size;
    size_t expected_size;
    int model_id;
    int coeff_index;
    double start_jd;
    double end_jd;

    if (!almanac || !almanac->db || !out) {
        almanac_set_error(almanac, "invalid nutation segment lookup");
        return false;
    }
    if (jd < ALMANAC_NUTATION_START_JD || jd > ALMANAC_NUTATION_END_JD) {
        almanac_set_error(almanac, "requested date is outside the nutation model range");
        return false;
    }
    model_id = (int)((jd - ALMANAC_NUTATION_START_JD) / ALMANAC_NUTATION_SEGMENT_DAYS) + 1;

    memset(out, 0, sizeof(*out));
    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        almanac_set_sqlite_error(almanac);
        return false;
    }
    sqlite_stmt_bind_int(stmt, 1, model_id);
    rc = sqlite_stmt_step(stmt);
    if (rc == SQLITE_STEP_DONE) {
        sqlite_stmt_finalize(stmt);
        return false;
    }
    if (rc != SQLITE_STEP_ROW) {
        sqlite_stmt_finalize(stmt);
        almanac_set_sqlite_error(almanac);
        return false;
    }

    model_id = sqlite_stmt_column_int(stmt, 0);
    blob = sqlite_stmt_column_blob(stmt, 1);
    blob_size = sqlite_stmt_column_bytes(stmt, 1);
    expected_size = (size_t)(ALMANAC_NUTATION_COEFF_COUNT * 2) * sizeof(double);
    if (model_id <= 0 || !blob || blob_size != expected_size) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "nutation segment is malformed");
        return false;
    }
    start_jd = ALMANAC_NUTATION_START_JD + (double)(model_id - 1) * ALMANAC_NUTATION_SEGMENT_DAYS;
    end_jd = start_jd + ALMANAC_NUTATION_SEGMENT_DAYS;
    if (end_jd > ALMANAC_NUTATION_END_JD)
        end_jd = ALMANAC_NUTATION_END_JD;
    out->start_jd = start_jd;
    out->end_jd = end_jd;
    out->reference_jd = 0.5 * (start_jd + end_jd);
    out->span_days = 0.5 * (end_jd - start_jd);
    if (!(out->span_days > 0.0)) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "nutation segment has invalid time span");
        return false;
    }
    for (coeff_index = 0; coeff_index < ALMANAC_NUTATION_COEFF_COUNT; ++coeff_index) {
        memcpy((&out->dpsi.c0) + coeff_index,
               blob + sizeof(double) * (size_t)coeff_index,
               sizeof(double));
        memcpy((&out->deps.c0) + coeff_index,
               blob + sizeof(double) * (size_t)(ALMANAC_NUTATION_COEFF_COUNT + coeff_index),
               sizeof(double));
    }
    sqlite_stmt_finalize(stmt);
    return out->span_days > 0.0;
}

static bool almanac_load_correction_model(almanac_t *almanac)
{
    if (!almanac)
        return false;
    if (almanac->correction_model_loaded)
        return true;

    if (!almanac_load_nutation_terms(almanac)) {
        return false;
    }

    almanac->correction_model_loaded = true;
    return true;
}

static bool almanac_load_model_row(almanac_t *almanac,
                                   almanac_body_id_t body_id,
                                   almanac_model_row_t *out)
{
    static const char *sql =
        "select b.body_id, "
        "       kind.body_kind, model.model_kind, brightness.brightness_model, sort.sort_order,"
        "       magnitude.magnitude_coeff_blob, "
        "       f.coefficient_blob "
        "from almanac_body as b "
        "join almanac_body_kind as kind on kind.body_id = b.body_id "
        "join almanac_body_model_kind as model on model.body_id = b.body_id "
        "join almanac_body_brightness_model as brightness on brightness.body_id = b.body_id "
        "join almanac_body_sort_order as sort on sort.body_id = b.body_id "
        "join almanac_body_magnitude_coeff as magnitude on magnitude.body_id = b.body_id "
        "left join almanac_fixed_equatorial_model as f on f.body_id = b.body_id "
        "where b.body_id = ?1";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;
    const unsigned char *magnitude_blob;
    size_t magnitude_blob_size;
    const unsigned char *fixed_blob;
    size_t fixed_blob_size;
    const char *body_kind_text;
    const char *model_kind_text;
    const char *brightness_model_text;

    if (!almanac || !almanac->db || body_id <= ALMANAC_BODY_ID_UNKNOWN ||
        body_id >= ALMANAC_BODY_ID_COUNT || !out) {
        almanac_set_error(almanac, "invalid almanac lookup");
        return false;
    }

    memset(out, 0, sizeof(*out));
    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        almanac_set_sqlite_error(almanac);
        return false;
    }
    if (!sqlite_stmt_bind_int(stmt, 1, (int)body_id)) {
        const string_t *stmt_error = sqlite_stmt_last_error(stmt);

        almanac_set_error(almanac, stmt_error ? string_c_str(stmt_error) : "failed to bind almanac lookup");
        sqlite_stmt_finalize(stmt);
        return false;
    }

    rc = sqlite_stmt_step(stmt);
    if (rc != SQLITE_STEP_ROW) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "requested almanac body was not found");
        return false;
    }

    out->body_id = (almanac_body_id_t)sqlite_stmt_column_int(stmt, 0);
    body_kind_text = sqlite_stmt_column_text(stmt, 1);
    model_kind_text = sqlite_stmt_column_text(stmt, 2);
    brightness_model_text = sqlite_stmt_column_text(stmt, 3);
    if (!almanac_body_kind_from_text(body_kind_text, &out->body_kind) ||
        !almanac_model_kind_from_text(model_kind_text, &out->model_kind) ||
        !almanac_brightness_model_from_text(brightness_model_text, &out->brightness_model)) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "almanac body classifiers are malformed");
        return false;
    }
    out->sort_order = sqlite_stmt_column_int(stmt, 4);
    magnitude_blob = sqlite_stmt_column_blob(stmt, 5);
    magnitude_blob_size = sqlite_stmt_column_bytes(stmt, 5);
    if (!almanac_unpack_magnitude_coeff_blob(magnitude_blob, magnitude_blob_size, out)) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "almanac body magnitude coefficients are malformed");
        return false;
    }
    fixed_blob = sqlite_stmt_column_blob(stmt, 6);
    fixed_blob_size = sqlite_stmt_column_bytes(stmt, 6);
    if (fixed_blob_size > 0u &&
        !almanac_unpack_fixed_equatorial_blob(fixed_blob, fixed_blob_size, out)) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "almanac fixed equatorial coefficients are malformed");
        return false;
    }

    sqlite_stmt_finalize(stmt);
    return true;
}

static bool almanac_fetch_model(almanac_t *almanac,
                                almanac_body_id_t body_id,
                                almanac_model_row_t *out)
{
    if (!almanac || !almanac->db || body_id <= ALMANAC_BODY_ID_UNKNOWN ||
        body_id >= ALMANAC_BODY_ID_COUNT || !out) {
        almanac_set_error(almanac, "invalid almanac lookup");
        return false;
    }
    if (!almanac->model_row_cached[body_id]) {
        if (!almanac_load_model_row(almanac, body_id, &almanac->model_row_cache[body_id]))
            return false;
        almanac->model_row_cached[body_id] = true;
    }
    *out = almanac->model_row_cache[body_id];
    return true;
}

static double almanac_gha_aries_for_jd(almanac_t *almanac, double jd)
{
    double T = (jd - 2451545.0) / 36525.0;
    double gmst;

    if (!almanac_load_correction_model(almanac))
        return DBL_MAX;
    gmst = ALMANAC_GMST_BASE_DEG
         + ALMANAC_GMST_RATE_DEG_PER_DAY * (jd - 2451545.0)
         + ALMANAC_GMST_QUADRATIC_DEG * T * T
         - (T * T * T) / ALMANAC_GMST_CUBIC_DIVISOR;

    return normalize_degrees(gmst);
}

static double almanac_mean_obliquity_radians(almanac_t *almanac, double jd)
{
    double T = (jd - 2451545.0) / 36525.0;
    double arcseconds;

    if (!almanac_load_correction_model(almanac))
        return DBL_MAX;
    arcseconds = ALMANAC_MEAN_OBLIQUITY_C0_ARCSEC
               + ALMANAC_MEAN_OBLIQUITY_C1_ARCSEC * T
               + ALMANAC_MEAN_OBLIQUITY_C2_ARCSEC * T * T
               + ALMANAC_MEAN_OBLIQUITY_C3_ARCSEC * T * T * T;

    return degrees_to_radians(arcseconds / 3600.0);
}

static bool almanac_nutation_angles(almanac_t *almanac,
                                    double jd,
                                    double *delta_psi_radians,
                                    double *delta_epsilon_radians)
{
    almanac_nutation_segment_t segment;
    double T = (jd - 2451545.0) / 36525.0;
    double omega;
    double L;
    double Lprime;
    double dpsi_arcseconds = 0.0;
    double deps_arcseconds = 0.0;
    size_t i;

    if (!almanac_load_correction_model(almanac))
        return false;
    if (almanac_fetch_nutation_segment(almanac, jd, &segment)) {
        double x = (jd - segment.reference_jd) / segment.span_days;

        if (delta_psi_radians)
            *delta_psi_radians = almanac_cheb8_eval(&segment.dpsi, x);
        if (delta_epsilon_radians)
            *delta_epsilon_radians = almanac_cheb8_eval(&segment.deps, x);
        return true;
    }

    omega = degrees_to_radians(normalize_degrees(ALMANAC_OMEGA_C0_DEG +
                                                 ALMANAC_OMEGA_C1_DEG_PER_CENTURY * T +
                                                 ALMANAC_OMEGA_C2_DEG_PER_CENTURY2 * T * T +
                                                 (T * T * T) / ALMANAC_OMEGA_C3_CENTURY3_DIVISOR));
    L = degrees_to_radians(normalize_degrees(ALMANAC_SOLAR_MEAN_LONGITUDE_C0_DEG +
                                             ALMANAC_SOLAR_MEAN_LONGITUDE_C1_DEG_PER_CENTURY * T));
    Lprime = degrees_to_radians(normalize_degrees(ALMANAC_LUNAR_MEAN_LONGITUDE_C0_DEG +
                                                  ALMANAC_LUNAR_MEAN_LONGITUDE_C1_DEG_PER_CENTURY * T));

    for (i = 0u; i < almanac->nutation_term_count; ++i) {
        const almanac_nutation_term_t *term = &almanac->nutation_terms[i];
        double argument = term->multiplier_L * L
                        + term->multiplier_Lprime * Lprime
                        + term->multiplier_omega * omega;

        dpsi_arcseconds += term->sin_coeff_arcsec * sin(argument);
        deps_arcseconds += term->cos_coeff_arcsec * cos(argument);
    }

    if (delta_psi_radians)
        *delta_psi_radians = degrees_to_radians(dpsi_arcseconds / 3600.0);
    if (delta_epsilon_radians)
        *delta_epsilon_radians = degrees_to_radians(deps_arcseconds / 3600.0);
    return true;
}

static double almanac_apparent_gha_aries_for_jd(almanac_t *almanac, double jd)
{
    double epsilon0 = almanac_mean_obliquity_radians(almanac, jd);
    double delta_psi = 0.0;
    double delta_epsilon = 0.0;
    double equation_of_equinoxes_degrees;

    if (epsilon0 == DBL_MAX || !almanac_nutation_angles(almanac, jd, &delta_psi, &delta_epsilon))
        return DBL_MAX;
    equation_of_equinoxes_degrees =
        radians_to_degrees(delta_psi * cos(epsilon0 + delta_epsilon));
    return normalize_degrees(almanac_gha_aries_for_jd(almanac, jd) + equation_of_equinoxes_degrees);
}

static bool almanac_body_kind_from_text(const char *text, almanac_body_kind_t *out)
{
    static const struct {
        const char *text;
        almanac_body_kind_t value;
    } body_kinds[] = {
        {"star", ALMANAC_BODY_STAR},
        {"planet", ALMANAC_BODY_PLANET},
        {"sun", ALMANAC_BODY_SUN},
        {"moon", ALMANAC_BODY_MOON}
    };
    size_t i;

    if (!text || !out)
        return false;
    for (i = 0u; i < sizeof(body_kinds) / sizeof(body_kinds[0]); ++i) {
        if (strcmp(text, body_kinds[i].text) == 0) {
            *out = body_kinds[i].value;
            return true;
        }
    }
    return false;
}

static bool almanac_model_kind_from_text(const char *text, almanac_model_kind_t *out)
{
    static const struct {
        const char *text;
        almanac_model_kind_t value;
    } model_kinds[] = {
        {"fixed_equatorial", ALMANAC_MODEL_KIND_FIXED_EQUATORIAL},
        {"chebyshev_position", ALMANAC_MODEL_KIND_CHEBYSHEV_POSITION},
        {"lunar_chebyshev", ALMANAC_MODEL_KIND_LUNAR_CHEBYSHEV}
    };
    size_t i;

    if (!text || !out)
        return false;
    for (i = 0u; i < sizeof(model_kinds) / sizeof(model_kinds[0]); ++i) {
        if (strcmp(text, model_kinds[i].text) == 0) {
            *out = model_kinds[i].value;
            return true;
        }
    }
    return false;
}

static bool almanac_brightness_model_from_text(const char *text, almanac_brightness_model_t *out)
{
    static const struct {
        const char *text;
        almanac_brightness_model_t value;
    } brightness_models[] = {
        {"none", ALMANAC_BRIGHTNESS_MODEL_NONE},
        {"catalogued", ALMANAC_BRIGHTNESS_MODEL_CATALOGUED},
        {"sun_distance", ALMANAC_BRIGHTNESS_MODEL_SUN_DISTANCE},
        {"planetary_phase", ALMANAC_BRIGHTNESS_MODEL_PLANETARY_PHASE},
        {"lunar_phase", ALMANAC_BRIGHTNESS_MODEL_LUNAR_PHASE}
    };
    size_t i;

    if (!text || !out)
        return false;
    for (i = 0u; i < sizeof(brightness_models) / sizeof(brightness_models[0]); ++i) {
        if (strcmp(text, brightness_models[i].text) == 0) {
            *out = brightness_models[i].value;
            return true;
        }
    }
    return false;
}

static bool almanac_fetch_chebyshev_position_segment(almanac_t *almanac,
                                                     almanac_body_ref_id_t body_ref_id,
                                                     double jd,
                                                     almanac_chebyshev_position_segment_t *out)
{
    static const char *sql =
        "select selected.start_jd, selected.end_jd, selected.segment_span_days, "
        "       selected.degree, selected.segment_index, segment.coefficient_blob "
        "from ( "
        "    select series.series_id, start.start_jd, finish.end_jd, "
        "           span.segment_span_days, degree.degree, "
        "           case when ?2 >= finish.end_jd then segment_count.segment_count - 1 "
        "                else cast((?2 - start.start_jd) / span.segment_span_days as integer) "
        "           end as segment_index "
        "    from almanac_chebyshev_position_series as series "
        "    join almanac_chebyshev_position_series_body_ref as body_ref "
        "      on body_ref.series_id = series.series_id "
        "    join almanac_chebyshev_position_series_frame as series_frame "
        "      on series_frame.series_id = series.series_id "
        "    join almanac_frame_code as frame_code "
        "      on frame_code.frame_id = series_frame.frame_id "
        "    join almanac_chebyshev_position_series_start_jd as start "
        "      on start.series_id = series.series_id "
        "    join almanac_chebyshev_position_series_end_jd as finish "
        "      on finish.series_id = series.series_id "
        "    join almanac_chebyshev_position_series_segment_span_days as span "
        "      on span.series_id = series.series_id "
        "    join almanac_chebyshev_position_series_segment_count as segment_count "
        "      on segment_count.series_id = series.series_id "
        "    join almanac_chebyshev_position_series_degree as degree "
        "      on degree.series_id = series.series_id "
        "    where body_ref.body_ref_id = ?1 and frame_code.frame_code = 'ECLIPJ2000' "
        "      and start.start_jd <= ?2 and finish.end_jd >= ?2 "
        "    order by start.start_jd asc "
        "    limit 1 "
        ") as selected "
        "join almanac_chebyshev_position_segment as segment "
        "  on segment.series_id = selected.series_id "
        " and segment.segment_index = selected.segment_index";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;
    const unsigned char *blob;
    size_t blob_size;
    size_t expected_size;
    int component;
    int coeff;
    int degree;
    int segment_index;
    double series_start_jd;
    double series_end_jd;
    double segment_span_days;

    if (!almanac || !almanac->db || body_ref_id <= ALMANAC_BODY_REF_ID_NONE || !out) {
        almanac_set_error(almanac, "invalid Chebyshev position segment lookup");
        return false;
    }
    if (body_ref_id < ALMANAC_BODY_REF_ID_LIMIT &&
        almanac->chebyshev_position_segment_cached[body_ref_id] &&
        jd >= almanac->chebyshev_position_segment_cache[body_ref_id].start_jd &&
        jd <= almanac->chebyshev_position_segment_cache[body_ref_id].end_jd) {
        *out = almanac->chebyshev_position_segment_cache[body_ref_id];
        return true;
    }

    memset(out, 0, sizeof(*out));
    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        almanac_set_sqlite_error(almanac);
        return false;
    }
    if (!sqlite_stmt_bind_int(stmt, 1, body_ref_id) ||
        !sqlite_stmt_bind_double(stmt, 2, jd)) {
        const string_t *stmt_error = sqlite_stmt_last_error(stmt);

        almanac_set_error(almanac, stmt_error ? string_c_str(stmt_error) : "failed to bind Chebyshev lookup");
        sqlite_stmt_finalize(stmt);
        return false;
    }

    rc = sqlite_stmt_step(stmt);
    if (rc != SQLITE_STEP_ROW) {
        sqlite_stmt_finalize(stmt);
        if (rc == SQLITE_STEP_DONE)
            almanac_set_error(almanac, "no Chebyshev position segment matched the requested date");
        else
            almanac_set_sqlite_error(almanac);
        return false;
    }

    series_start_jd = sqlite_stmt_column_double(stmt, 0);
    series_end_jd = sqlite_stmt_column_double(stmt, 1);
    segment_span_days = sqlite_stmt_column_double(stmt, 2);
    degree = sqlite_stmt_column_int(stmt, 3);
    segment_index = sqlite_stmt_column_int(stmt, 4);
    blob = sqlite_stmt_column_blob(stmt, 5);
    blob_size = sqlite_stmt_column_bytes(stmt, 5);

    if (degree < 1 || degree >= ALMANAC_CHEB_MAX_COEFF_COUNT ||
        segment_index < 0 || segment_span_days <= 0.0 || !blob) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "Chebyshev position segment is malformed");
        return false;
    }
    out->start_jd = series_start_jd + (double)segment_index * segment_span_days;
    out->end_jd = out->start_jd + segment_span_days;
    if (out->end_jd > series_end_jd)
        out->end_jd = series_end_jd;
    out->reference_jd = 0.5 * (out->start_jd + out->end_jd);
    out->radius_days = 0.5 * (out->end_jd - out->start_jd);
    if (out->radius_days <= 0.0) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "Chebyshev position segment has invalid time span");
        return false;
    }
    expected_size = (size_t)ALMANAC_CHEB_COMPONENT_COUNT * (size_t)(degree + 1) * sizeof(double);
    if (blob_size != expected_size) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "Chebyshev position coefficient blob has invalid size");
        return false;
    }

    out->degree = degree;
    for (component = 0; component < ALMANAC_CHEB_COMPONENT_COUNT; ++component) {
        for (coeff = 0; coeff <= degree; ++coeff) {
            double value;

            memcpy(&value,
                   blob + sizeof(double) * ((size_t)component * (size_t)(degree + 1) + (size_t)coeff),
                   sizeof(value));
            out->coeff[component][coeff] = value;
        }
    }
    sqlite_stmt_finalize(stmt);
    if (body_ref_id < ALMANAC_BODY_REF_ID_LIMIT) {
        almanac->chebyshev_position_segment_cache[body_ref_id] = *out;
        almanac->chebyshev_position_segment_cached[body_ref_id] = true;
    }
    return true;
}

static bool almanac_fetch_frame_rotation_segment(almanac_t *almanac,
                                                 double jd,
                                                 almanac_frame_rotation_segment_t *out)
{
    static const char *sql =
        "select selected.start_jd, selected.end_jd, selected.segment_span_days, "
        "       selected.degree, selected.segment_index, segment.coefficient_blob "
        "from ( "
        "    select series.series_id, start.start_jd, finish.end_jd, "
        "           span.segment_span_days, degree.degree, "
        "           case when ?1 >= finish.end_jd then segment_count.segment_count - 1 "
        "                else cast((?1 - start.start_jd) / span.segment_span_days as integer) "
        "           end as segment_index "
        "    from almanac_frame_rotation_series as series "
        "    join almanac_frame_rotation_series_source_frame as source_frame "
        "      on source_frame.series_id = series.series_id "
        "    join almanac_frame_rotation_series_target_frame as target_frame "
        "      on target_frame.series_id = series.series_id "
        "    join almanac_frame_code as source_frame_code "
        "      on source_frame_code.frame_id = source_frame.frame_id "
        "    join almanac_frame_code as target_frame_code "
        "      on target_frame_code.frame_id = target_frame.frame_id "
        "    join almanac_frame_rotation_series_start_jd as start "
        "      on start.series_id = series.series_id "
        "    join almanac_frame_rotation_series_end_jd as finish "
        "      on finish.series_id = series.series_id "
        "    join almanac_frame_rotation_series_segment_span_days as span "
        "      on span.series_id = series.series_id "
        "    join almanac_frame_rotation_series_segment_count as segment_count "
        "      on segment_count.series_id = series.series_id "
        "    join almanac_frame_rotation_series_degree as degree "
        "      on degree.series_id = series.series_id "
        "    where source_frame_code.frame_code = 'ECLIPJ2000' "
        "      and target_frame_code.frame_code = 'TRUE_EQUATOR_DATE' "
        "      and start.start_jd <= ?1 and finish.end_jd >= ?1 "
        "    order by start.start_jd asc "
        "    limit 1 "
        ") as selected "
        "join almanac_frame_rotation_segment as segment "
        "  on segment.series_id = selected.series_id "
        " and segment.segment_index = selected.segment_index";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;
    const unsigned char *blob;
    size_t blob_size;
    size_t expected_size;
    int component;
    int coeff;
    int degree;
    int segment_index;
    double series_start_jd;
    double series_end_jd;
    double segment_span_days;

    if (!almanac || !almanac->db || !out) {
        almanac_set_error(almanac, "invalid frame rotation segment lookup");
        return false;
    }
    if (almanac->frame_rotation_segment_cached &&
        jd >= almanac->frame_rotation_segment_cache.start_jd &&
        jd <= almanac->frame_rotation_segment_cache.end_jd) {
        *out = almanac->frame_rotation_segment_cache;
        return true;
    }

    memset(out, 0, sizeof(*out));
    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        almanac_set_sqlite_error(almanac);
        return false;
    }
    if (!sqlite_stmt_bind_double(stmt, 1, jd)) {
        const string_t *stmt_error = sqlite_stmt_last_error(stmt);

        almanac_set_error(almanac, stmt_error ? string_c_str(stmt_error) : "failed to bind frame rotation lookup");
        sqlite_stmt_finalize(stmt);
        return false;
    }

    rc = sqlite_stmt_step(stmt);
    if (rc != SQLITE_STEP_ROW) {
        sqlite_stmt_finalize(stmt);
        if (rc == SQLITE_STEP_DONE)
            almanac_set_error(almanac, "no frame rotation segment matched the requested date");
        else
            almanac_set_sqlite_error(almanac);
        return false;
    }

    series_start_jd = sqlite_stmt_column_double(stmt, 0);
    series_end_jd = sqlite_stmt_column_double(stmt, 1);
    segment_span_days = sqlite_stmt_column_double(stmt, 2);
    degree = sqlite_stmt_column_int(stmt, 3);
    segment_index = sqlite_stmt_column_int(stmt, 4);
    blob = sqlite_stmt_column_blob(stmt, 5);
    blob_size = sqlite_stmt_column_bytes(stmt, 5);

    if (degree < 1 || degree >= ALMANAC_CHEB_MAX_COEFF_COUNT ||
        segment_index < 0 || segment_span_days <= 0.0 || !blob) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "frame rotation segment is malformed");
        return false;
    }
    out->start_jd = series_start_jd + (double)segment_index * segment_span_days;
    out->end_jd = out->start_jd + segment_span_days;
    if (out->end_jd > series_end_jd)
        out->end_jd = series_end_jd;
    out->reference_jd = 0.5 * (out->start_jd + out->end_jd);
    out->radius_days = 0.5 * (out->end_jd - out->start_jd);
    if (out->radius_days <= 0.0) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "frame rotation segment has invalid time span");
        return false;
    }
    expected_size = (size_t)ALMANAC_FRAME_ROTATION_COMPONENT_COUNT * (size_t)(degree + 1) * sizeof(double);
    if (blob_size != expected_size) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "frame rotation coefficient blob has invalid size");
        return false;
    }

    out->degree = degree;
    for (component = 0; component < ALMANAC_FRAME_ROTATION_COMPONENT_COUNT; ++component) {
        for (coeff = 0; coeff <= degree; ++coeff) {
            double value;

            memcpy(&value,
                   blob + sizeof(double) * ((size_t)component * (size_t)(degree + 1) + (size_t)coeff),
                   sizeof(value));
            out->coeff[component][coeff] = value;
        }
    }
    sqlite_stmt_finalize(stmt);
    almanac->frame_rotation_segment_cache = *out;
    almanac->frame_rotation_segment_cached = true;
    return true;
}

static double almanac_chebyshev_eval(const double *coeff, int degree, double x)
{
    double b_kplus1 = 0.0;
    double b_kplus2 = 0.0;
    double b_k;
    int i;

    for (i = degree; i >= 1; --i) {
        b_k = 2.0 * x * b_kplus1 - b_kplus2 + coeff[i];
        b_kplus2 = b_kplus1;
        b_kplus1 = b_k;
    }
    return x * b_kplus1 - b_kplus2 + coeff[0];
}

static double almanac_chebyshev_derivative_eval(const double *coeff, int degree, double x)
{
    double u_minus2 = 1.0;
    double value = degree >= 1 ? coeff[1] : 0.0;
    double u_minus1 = 2.0 * x;
    int n;

    for (n = 2; n <= degree; ++n) {
        double u;

        if (n == 2) {
            u = u_minus1;
        } else {
            u = 2.0 * x * u_minus1 - u_minus2;
            u_minus2 = u_minus1;
            u_minus1 = u;
        }
        value += (double)n * coeff[n] * u;
    }
    return value;
}

static void almanac_eval_chebyshev_position_segment(const almanac_chebyshev_position_segment_t *segment,
                                                    double jd,
                                                    cartesian3_t *position_au,
                                                    cartesian3_t *velocity_au_per_day)
{
    double x;

    if (!segment)
        return;
    x = (jd - segment->reference_jd) / segment->radius_days;
    if (position_au) {
        position_au->x = almanac_chebyshev_eval(segment->coeff[0], segment->degree, x);
        position_au->y = almanac_chebyshev_eval(segment->coeff[1], segment->degree, x);
        position_au->z = almanac_chebyshev_eval(segment->coeff[2], segment->degree, x);
    }
    if (velocity_au_per_day) {
        velocity_au_per_day->x =
            almanac_chebyshev_derivative_eval(segment->coeff[0], segment->degree, x) / segment->radius_days;
        velocity_au_per_day->y =
            almanac_chebyshev_derivative_eval(segment->coeff[1], segment->degree, x) / segment->radius_days;
        velocity_au_per_day->z =
            almanac_chebyshev_derivative_eval(segment->coeff[2], segment->degree, x) / segment->radius_days;
    }
}

static void almanac_eval_frame_rotation_segment(const almanac_frame_rotation_segment_t *segment,
                                                double jd,
                                                double matrix[3][3])
{
    double x;
    int row;
    int column;

    if (!segment || !matrix)
        return;
    x = (jd - segment->reference_jd) / segment->radius_days;
    for (row = 0; row < 3; ++row) {
        for (column = 0; column < 3; ++column) {
            int component = row * 3 + column;

            matrix[row][column] = almanac_chebyshev_eval(segment->coeff[component], segment->degree, x);
        }
    }
}

static void almanac_state_fill_equatorial(almanac_state_t *state)
{
    double radius_xy;

    if (!state)
        return;
    radius_xy = sqrt(state->geocentric_equatorial_au.x * state->geocentric_equatorial_au.x +
                     state->geocentric_equatorial_au.y * state->geocentric_equatorial_au.y);
    state->right_ascension_hours =
        normalize_degrees(radians_to_degrees(atan2(state->geocentric_equatorial_au.y,
                                                   state->geocentric_equatorial_au.x))) / 15.0;
    state->declination_degrees =
        radians_to_degrees(atan2(state->geocentric_equatorial_au.z, radius_xy));
}

static bool almanac_earth_heliocentric_state(almanac_t *almanac,
                                             double jd,
                                             cartesian3_t *position_ecliptic_au,
                                             cartesian3_t *velocity_ecliptic_au_per_day)
{
    almanac_chebyshev_position_segment_t cheb_segment;
    cartesian3_t earth_moon_barycenter;
    cartesian3_t earth_moon_barycenter_velocity;
    cartesian3_t moon_geocentric;
    cartesian3_t moon_geocentric_velocity;
    cartesian3_t moon_offset;
    cartesian3_t position_storage;
    bool have_velocity = velocity_ecliptic_au_per_day != NULL;

    if (!almanac || (!position_ecliptic_au && !velocity_ecliptic_au_per_day)) {
        almanac_set_error(almanac, "invalid Earth-state request");
        return false;
    }
    if (!position_ecliptic_au)
        position_ecliptic_au = &position_storage;

    if (!almanac_load_correction_model(almanac))
        return false;

    if (!almanac_fetch_chebyshev_position_segment(almanac, ALMANAC_BODY_REF_ID_EARTH_BARYCENTER, jd, &cheb_segment))
        return false;
    almanac_eval_chebyshev_position_segment(&cheb_segment,
                                            jd,
                                            &earth_moon_barycenter,
                                            have_velocity ? &earth_moon_barycenter_velocity : NULL);
    if (!almanac_lunar_geocentric_ecliptic_vector(almanac, jd, &moon_geocentric))
        return false;
    moon_offset = cartesian_scale(&moon_geocentric, ALMANAC_MOON_EARTH_SYSTEM_MASS_FRACTION);
    *position_ecliptic_au = cartesian_subtract(&earth_moon_barycenter, &moon_offset);
    if (have_velocity) {
        if (!almanac_lunar_geocentric_ecliptic_velocity(almanac, jd, &moon_geocentric_velocity))
            return false;
        moon_offset = cartesian_scale(&moon_geocentric_velocity, ALMANAC_MOON_EARTH_SYSTEM_MASS_FRACTION);
        *velocity_ecliptic_au_per_day =
            cartesian_subtract(&earth_moon_barycenter_velocity, &moon_offset);
    }
    return true;
}

static cartesian3_t equatorial_from_ecliptic_vector(const cartesian3_t *ecliptic,
                                                    almanac_t *almanac,
                                                    double jd)
{
    return cartesian_rotate_x(ecliptic, almanac_mean_obliquity_radians(almanac, jd));
}

static bool almanac_true_equatorial_from_ecliptic(almanac_t *almanac,
                                                 const cartesian3_t *ecliptic,
                                                 double jd,
                                                 cartesian3_t *out_equatorial)
{
    almanac_frame_rotation_segment_t rotation_segment;
    double T;
    double epsilon_j2000;
    double epsilon_date;
    double cos_epsilon;
    double sin_epsilon;
    double zeta;
    double theta;
    double z;
    double delta_psi;
    double delta_epsilon;
    cartesian3_t equatorial;
    double x;
    double y;

    if (!almanac || !ecliptic || !out_equatorial)
        return false;

    if (almanac_fetch_frame_rotation_segment(almanac, jd, &rotation_segment)) {
        double matrix[3][3];
        double in_x = ecliptic->x;
        double in_y = ecliptic->y;
        double in_z = ecliptic->z;

        almanac_eval_frame_rotation_segment(&rotation_segment, jd, matrix);
        out_equatorial->x = matrix[0][0] * in_x
                          + matrix[0][1] * in_y
                          + matrix[0][2] * in_z;
        out_equatorial->y = matrix[1][0] * in_x
                          + matrix[1][1] * in_y
                          + matrix[1][2] * in_z;
        out_equatorial->z = matrix[2][0] * in_x
                          + matrix[2][1] * in_y
                          + matrix[2][2] * in_z;
        return true;
    }
    if (almanac->error)
        string_clear(almanac->error);

    T = (jd - 2451545.0) / 36525.0;
    epsilon_j2000 = almanac_mean_obliquity_radians(almanac, 2451545.0);
    epsilon_date = almanac_mean_obliquity_radians(almanac, jd);
    if (epsilon_j2000 == DBL_MAX || epsilon_date == DBL_MAX)
        return false;

    equatorial = cartesian_rotate_x(ecliptic, epsilon_j2000);

    zeta = degrees_to_radians(((ALMANAC_PRECESSION_ZETA_C3_DEG * T +
                                ALMANAC_PRECESSION_ZETA_C2_DEG) * T +
                               ALMANAC_PRECESSION_ZETA_C1_DEG) * T);
    theta = degrees_to_radians(((ALMANAC_PRECESSION_THETA_C3_DEG * T +
                                 ALMANAC_PRECESSION_THETA_C2_DEG) * T +
                                ALMANAC_PRECESSION_THETA_C1_DEG) * T);
    z = degrees_to_radians(((ALMANAC_PRECESSION_Z_C3_DEG * T +
                             ALMANAC_PRECESSION_Z_C2_DEG) * T +
                            ALMANAC_PRECESSION_Z_C1_DEG) * T);

    equatorial = cartesian_rotate_z(&equatorial, zeta);
    equatorial = cartesian_rotate_y(&equatorial, -theta);
    equatorial = cartesian_rotate_z(&equatorial, z);

    if (!almanac_nutation_angles(almanac, jd, &delta_psi, &delta_epsilon))
        return false;

    cos_epsilon = cos(epsilon_date);
    sin_epsilon = sin(epsilon_date);
    x = equatorial.x;
    y = equatorial.y;
    equatorial.x = x - y * delta_psi * cos_epsilon - equatorial.z * delta_psi * sin_epsilon;
    equatorial.y = x * delta_psi * cos_epsilon + y - equatorial.z * delta_epsilon;
    equatorial.z = x * delta_psi * sin_epsilon + y * delta_epsilon + equatorial.z;

    *out_equatorial = equatorial;
    return true;
}

static double almanac_relativistic_light_time_days(const cartesian3_t *earth_heliocentric_au,
                                                   const cartesian3_t *body_heliocentric_au,
                                                   const cartesian3_t *earth_to_body_au)
{
    const double gravitational_radius_twice_au = 1.9741257129241e-08;
    double earth_sun_distance;
    double body_sun_distance;
    double earth_body_distance;
    double numerator;
    double denominator;

    if (!earth_heliocentric_au || !body_heliocentric_au || !earth_to_body_au)
        return 0.0;

    earth_sun_distance = cartesian_length(earth_heliocentric_au);
    body_sun_distance = cartesian_length(body_heliocentric_au);
    earth_body_distance = cartesian_length(earth_to_body_au);
    if (earth_sun_distance <= 0.0 || body_sun_distance <= 0.0 || earth_body_distance <= 0.0)
        return earth_body_distance / ALMANAC_SPEED_OF_LIGHT_AU_PER_DAY;

    numerator = earth_sun_distance + earth_body_distance + body_sun_distance;
    denominator = earth_sun_distance - earth_body_distance + body_sun_distance;
    if (denominator <= 0.0 || numerator <= denominator)
        return earth_body_distance / ALMANAC_SPEED_OF_LIGHT_AU_PER_DAY;

    return (earth_body_distance + gravitational_radius_twice_au * log(numerator / denominator)) /
           ALMANAC_SPEED_OF_LIGHT_AU_PER_DAY;
}

static bool almanac_apply_gravitational_deflection(const cartesian3_t *earth_heliocentric_equatorial_au,
                                                   const cartesian3_t *mean_equatorial_direction,
                                                   const cartesian3_t *sun_to_body_direction_equatorial,
                                                   cartesian3_t *out_direction)
{
    const double gravitational_radius_twice_au = 1.9741257129241e-08;
    cartesian3_t earth_direction;
    cartesian3_t p_hat;
    cartesian3_t q_hat;
    cartesian3_t first_cross;
    cartesian3_t second_cross;
    double earth_distance;
    double denominator;
    cartesian3_t correction;

    if (!earth_heliocentric_equatorial_au || !mean_equatorial_direction ||
        !sun_to_body_direction_equatorial || !out_direction) {
        return false;
    }

    earth_distance = cartesian_length(earth_heliocentric_equatorial_au);
    if (earth_distance <= 0.0)
        return false;

    earth_direction = cartesian_scale(earth_heliocentric_equatorial_au, 1.0 / earth_distance);
    p_hat = *mean_equatorial_direction;
    q_hat = *sun_to_body_direction_equatorial;
    if (!cartesian_normalize_in_place(&p_hat) || !cartesian_normalize_in_place(&q_hat))
        return false;

    denominator = 1.0 + cartesian_dot(&q_hat, &earth_direction);
    if (fabs(denominator) < 1.0e-12) {
        *out_direction = p_hat;
        return true;
    }

    first_cross = cartesian_cross(&q_hat, &earth_direction);
    second_cross = cartesian_cross(&first_cross, &p_hat);
    correction = cartesian_scale(&second_cross, (earth_distance * gravitational_radius_twice_au) / denominator);
    *out_direction = cartesian_add(&p_hat, &correction);
    return cartesian_normalize_in_place(out_direction);
}

static bool almanac_apply_annual_aberration(const cartesian3_t *mean_equatorial_direction,
                                            const cartesian3_t *earth_heliocentric_velocity_au_per_day,
                                            cartesian3_t *out_direction)
{
    cartesian3_t beta;
    cartesian3_t direction;
    cartesian3_t parallel_component;
    cartesian3_t transverse_beta;
    double projection;

    if (!mean_equatorial_direction || !earth_heliocentric_velocity_au_per_day || !out_direction)
        return false;

    direction = *mean_equatorial_direction;
    if (!cartesian_normalize_in_place(&direction))
        return false;

    beta = cartesian_scale(earth_heliocentric_velocity_au_per_day, 1.0 / ALMANAC_SPEED_OF_LIGHT_AU_PER_DAY);
    projection = cartesian_dot(&direction, &beta);
    parallel_component = cartesian_scale(&direction, projection);
    transverse_beta = cartesian_subtract(&beta, &parallel_component);
    *out_direction = cartesian_add(&direction, &transverse_beta);
    return cartesian_normalize_in_place(out_direction);
}

static bool almanac_apply_nutation(almanac_t *almanac,
                                   cartesian3_t *mean_equatorial_direction,
                                   double jd)
{
    double epsilon0;
    double delta_psi;
    double delta_epsilon;
    cartesian3_t ecliptic;
    cartesian3_t nutated;

    if (!mean_equatorial_direction)
        return false;

    epsilon0 = almanac_mean_obliquity_radians(almanac, jd);
    if (epsilon0 == DBL_MAX || !almanac_nutation_angles(almanac, jd, &delta_psi, &delta_epsilon))
        return false;
    ecliptic = cartesian_rotate_x(mean_equatorial_direction, -epsilon0);
    ecliptic = cartesian_rotate_z(&ecliptic, delta_psi);
    nutated = cartesian_rotate_x(&ecliptic, epsilon0 + delta_epsilon);
    *mean_equatorial_direction = nutated;
    return cartesian_normalize_in_place(mean_equatorial_direction);
}

static bool almanac_apply_apparent_direction_corrections(almanac_t *almanac,
                                                         const almanac_model_row_t *model,
                                                         double jd,
                                                         almanac_state_t *state)
{
    cartesian3_t earth_velocity_ecliptic;
    cartesian3_t earth_velocity_equatorial;
    cartesian3_t earth_heliocentric_ecliptic;
    cartesian3_t earth_heliocentric_equatorial;
    cartesian3_t sun_to_body_direction;
    cartesian3_t direction;
    double distance;

    if (!almanac || !model || !state || !state->has_direction_vector)
        return false;
    if (!almanac_load_correction_model(almanac))
        return false;
    if (!almanac_earth_heliocentric_state(almanac, jd, &earth_heliocentric_ecliptic, &earth_velocity_ecliptic))
        return false;

    if (state->has_ecliptic_vector) {
        direction = state->geocentric_ecliptic_au;
        distance = cartesian_length(&direction);
        if (distance <= 0.0)
            return false;
        if (!cartesian_normalize_in_place(&direction))
            return false;

        if (model->body_id != ALMANAC_BODY_ID_SUN) {
            if (model->body_kind == ALMANAC_BODY_STAR) {
                sun_to_body_direction = direction;
            } else {
                sun_to_body_direction = cartesian_add(&earth_heliocentric_ecliptic,
                                                      &state->geocentric_ecliptic_au);
            }
            if (!almanac_apply_gravitational_deflection(&earth_heliocentric_ecliptic,
                                                        &direction,
                                                        &sun_to_body_direction,
                                                        &direction)) {
                return false;
            }
        }
        if (!almanac_apply_annual_aberration(&direction, &earth_velocity_ecliptic, &direction))
            return false;
        if (!almanac_true_equatorial_from_ecliptic(almanac, &direction, jd, &direction))
            return false;

        state->geocentric_equatorial_au = cartesian_scale(&direction, distance);
        return true;
    }

    earth_heliocentric_equatorial = equatorial_from_ecliptic_vector(&earth_heliocentric_ecliptic, almanac, jd);
    earth_velocity_equatorial = equatorial_from_ecliptic_vector(&earth_velocity_ecliptic, almanac, jd);
    direction = state->geocentric_equatorial_au;
    if (model->body_id != ALMANAC_BODY_ID_SUN) {
        if (model->body_kind == ALMANAC_BODY_STAR) {
            sun_to_body_direction = direction;
        } else {
            sun_to_body_direction = cartesian_add(&earth_heliocentric_equatorial, &state->geocentric_equatorial_au);
        }
        if (!almanac_apply_gravitational_deflection(&earth_heliocentric_equatorial,
                                                    &direction,
                                                    &sun_to_body_direction,
                                                    &direction)) {
            return false;
        }
    }
    if (!almanac_apply_annual_aberration(&direction, &earth_velocity_equatorial, &direction))
        return false;
    if (!almanac_apply_nutation(almanac, &direction, jd))
        return false;

    if (state->has_geocentric_vector) {
        double distance = cartesian_length(&state->geocentric_equatorial_au);
        state->geocentric_equatorial_au = cartesian_scale(&direction, distance);
    } else {
        state->geocentric_equatorial_au = direction;
    }
    return true;
}

static bool almanac_lunar_geocentric_ecliptic_vector(almanac_t *almanac,
                                                     double jd,
                                                     cartesian3_t *out)
{
    almanac_chebyshev_position_segment_t cheb_segment;

    if (!out) {
        almanac_set_error(almanac, "invalid lunar ecliptic vector request");
        return false;
    }

    if (!almanac_fetch_chebyshev_position_segment(almanac, ALMANAC_BODY_REF_ID_MOON, jd, &cheb_segment))
        return false;
    almanac_eval_chebyshev_position_segment(&cheb_segment, jd, out, NULL);
    return true;
}

static bool almanac_lunar_geocentric_ecliptic_velocity(almanac_t *almanac,
                                                       double jd,
                                                       cartesian3_t *out_velocity_au_per_day)
{
    almanac_chebyshev_position_segment_t cheb_segment;

    if (!out_velocity_au_per_day) {
        almanac_set_error(almanac, "invalid lunar ecliptic velocity request");
        return false;
    }

    if (!almanac_fetch_chebyshev_position_segment(almanac, ALMANAC_BODY_REF_ID_MOON, jd, &cheb_segment))
        return false;
    almanac_eval_chebyshev_position_segment(&cheb_segment, jd, NULL, out_velocity_au_per_day);
    return true;
}

static bool almanac_equatorial_from_moon(almanac_t *almanac,
                                         const almanac_model_row_t *model,
                                         double jd,
                                         almanac_state_t *state)
{
    cartesian3_t earth_velocity_ecliptic;
    double light_time_days = 0.0;
    int iteration;

    if (!almanac || !model || !state)
        return false;
    if (!almanac_load_correction_model(almanac))
        return false;

    for (iteration = 0; iteration < 3; ++iteration) {
        double distance_au;

        if (!almanac_lunar_geocentric_ecliptic_vector(almanac,
                                                      jd - light_time_days,
                                                      &state->geocentric_ecliptic_au))
            return false;
        distance_au = cartesian_length(&state->geocentric_ecliptic_au);
        if (distance_au <= 0.0)
            return false;
        light_time_days = distance_au / ALMANAC_SPEED_OF_LIGHT_AU_PER_DAY;
    }
    if (!almanac_earth_heliocentric_state(almanac, jd, NULL, &earth_velocity_ecliptic))
        return false;
    state->geocentric_ecliptic_au.x -= earth_velocity_ecliptic.x * light_time_days;
    state->geocentric_ecliptic_au.y -= earth_velocity_ecliptic.y * light_time_days;
    state->geocentric_ecliptic_au.z -= earth_velocity_ecliptic.z * light_time_days;
    state->has_geocentric_vector = true;
    state->has_ecliptic_vector = true;
    state->has_direction_vector = true;
    return true;
}

static bool almanac_equatorial_from_fixed(const almanac_model_row_t *model,
                                          double jd,
                                          almanac_state_t *state)
{
    const double j2000 = 2451545.0;
    double alpha0;
    double delta0;
    double years;
    double T;
    double t;
    double zeta;
    double z;
    double theta;
    double A;
    double B;
    double C;
    double alpha;
    double delta;

    if (!model || !state)
        return false;

    years = (jd - model->fixed_epoch_jd) / 365.25;
    alpha0 = degrees_to_radians(model->fixed_ra_hours * 15.0 +
                                (model->fixed_pm_ra_mas_per_year * years) / 3600000.0);
    delta0 = degrees_to_radians(model->fixed_dec_degrees +
                                (model->fixed_pm_dec_mas_per_year * years) / 3600000.0);

    T = (model->fixed_epoch_jd - j2000) / 36525.0;
    t = (jd - model->fixed_epoch_jd) / 36525.0;

    zeta = degrees_to_radians(
        (((2306.2181 + 1.39656 * T - 0.000139 * T * T) * t) +
         ((0.30188 - 0.000344 * T) * t * t) +
         (0.017998 * t * t * t)) / 3600.0);
    z = degrees_to_radians(
        (((2306.2181 + 1.39656 * T - 0.000139 * T * T) * t) +
         ((1.09468 + 0.000066 * T) * t * t) +
         (0.018203 * t * t * t)) / 3600.0);
    theta = degrees_to_radians(
        (((2004.3109 - 0.85330 * T - 0.000217 * T * T) * t) -
         ((0.42665 + 0.000217 * T) * t * t) -
         (0.041833 * t * t * t)) / 3600.0);

    A = cos(delta0) * sin(alpha0 + zeta);
    B = cos(theta) * cos(delta0) * cos(alpha0 + zeta) - sin(theta) * sin(delta0);
    C = sin(theta) * cos(delta0) * cos(alpha0 + zeta) + cos(theta) * sin(delta0);

    alpha = atan2(A, B) + z;
    delta = asin(C);

    state->right_ascension_hours = normalize_degrees(radians_to_degrees(alpha)) / 15.0;
    state->declination_degrees = radians_to_degrees(delta);
    state->has_geocentric_vector = false;
    state->has_direction_vector = true;
    state->geocentric_equatorial_au.x = cos(delta) * cos(alpha);
    state->geocentric_equatorial_au.y = cos(delta) * sin(alpha);
    state->geocentric_equatorial_au.z = sin(delta);
    return true;
}

static bool almanac_equatorial_from_chebyshev(almanac_t *almanac,
                                              const almanac_model_row_t *model,
                                              double jd,
                                              almanac_state_t *state)
{
    almanac_chebyshev_position_segment_t cheb_segment;
    cartesian3_t body_helio;
    cartesian3_t earth_helio;
    cartesian3_t geocentric;
    almanac_body_ref_id_t body_ref_id;
    double light_time_days = 0.0;
    int iteration;

    if (!almanac || !model || !state)
        return false;
    if (!almanac_earth_heliocentric_state(almanac, jd, &earth_helio, NULL))
        return false;

    if (model->body_id == ALMANAC_BODY_ID_SUN) {
        geocentric.x = -earth_helio.x;
        geocentric.y = -earth_helio.y;
        geocentric.z = -earth_helio.z;
    } else {
        body_ref_id = ALMANAC_BODY_REF_IDS[model->body_id];
        if (body_ref_id == ALMANAC_BODY_REF_ID_NONE) {
            almanac_set_error(almanac, "requested body does not have a Chebyshev body reference");
            return false;
        }
        for (iteration = 0; iteration < 3; ++iteration) {
            if (!almanac_fetch_chebyshev_position_segment(almanac,
                                                          body_ref_id,
                                                          jd - light_time_days,
                                                          &cheb_segment))
                return false;
            almanac_eval_chebyshev_position_segment(&cheb_segment, jd - light_time_days, &body_helio, NULL);
            geocentric.x = body_helio.x - earth_helio.x;
            geocentric.y = body_helio.y - earth_helio.y;
            geocentric.z = body_helio.z - earth_helio.z;
            if (!almanac_load_correction_model(almanac))
                return false;
            light_time_days = almanac_relativistic_light_time_days(&earth_helio, &body_helio, &geocentric);
        }
    }

    state->geocentric_ecliptic_au = geocentric;
    if (!almanac_true_equatorial_from_ecliptic(almanac,
                                               &state->geocentric_ecliptic_au,
                                               jd,
                                               &state->geocentric_equatorial_au)) {
        return false;
    }
    state->has_geocentric_vector = true;
    state->has_ecliptic_vector = true;
    state->has_direction_vector = true;
    return true;
}

static double almanac_phase_angle_degrees(const cartesian3_t *earth_to_object,
                                          const cartesian3_t *earth_to_sun)
{
    cartesian3_t object_to_earth;
    cartesian3_t object_to_sun;
    double len_earth;
    double len_sun;
    double cosine_angle;

    if (!earth_to_object || !earth_to_sun)
        return NAN;
    object_to_earth = cartesian_negate(earth_to_object);
    object_to_sun = cartesian_subtract(earth_to_sun, earth_to_object);
    len_earth = cartesian_length(&object_to_earth);
    len_sun = cartesian_length(&object_to_sun);
    if (len_earth <= 0.0 || len_sun <= 0.0)
        return NAN;
    cosine_angle = cartesian_dot(&object_to_earth, &object_to_sun) / (len_earth * len_sun);
    if (cosine_angle > 1.0)
        cosine_angle = 1.0;
    if (cosine_angle < -1.0)
        cosine_angle = -1.0;
    return radians_to_degrees(acos(cosine_angle));
}

static bool almanac_visual_magnitude(const almanac_model_row_t *model,
                                     const almanac_state_t *state,
                                     const almanac_state_t *sun_state,
                                     double *magnitude,
                                     double *phase_angle_degrees,
                                     double *heliocentric_distance_au)
{
    double delta;
    double phase_angle;
    cartesian3_t sun_to_body;
    double r;

    if (!model || !magnitude || !phase_angle_degrees || !heliocentric_distance_au)
        return false;

    *magnitude = NAN;
    *phase_angle_degrees = NAN;
    *heliocentric_distance_au = NAN;

    if (model->brightness_model == ALMANAC_BRIGHTNESS_MODEL_NONE)
        return true;
    if (model->brightness_model == ALMANAC_BRIGHTNESS_MODEL_CATALOGUED) {
        *magnitude = model->magnitude_constant;
        return true;
    }
    if (!state || !state->has_geocentric_vector)
        return true;

    delta = cartesian_length(&state->geocentric_equatorial_au);
    if (delta <= 0.0)
        return true;

    if (model->brightness_model == ALMANAC_BRIGHTNESS_MODEL_SUN_DISTANCE) {
        *magnitude = model->magnitude_constant + 5.0 * log10(delta);
        *heliocentric_distance_au = 0.0;
        return true;
    }

    if (!sun_state || !sun_state->has_geocentric_vector)
        return true;

    phase_angle = almanac_phase_angle_degrees(&state->geocentric_equatorial_au,
                                              &sun_state->geocentric_equatorial_au);
    sun_to_body = cartesian_subtract(&state->geocentric_equatorial_au, &sun_state->geocentric_equatorial_au);
    r = cartesian_length(&sun_to_body);
    *phase_angle_degrees = phase_angle;
    *heliocentric_distance_au = r;

    if (!(phase_angle == phase_angle) || r <= 0.0)
        return true;

    if (model->brightness_model == ALMANAC_BRIGHTNESS_MODEL_PLANETARY_PHASE) {
        *magnitude = model->magnitude_constant
                   + 5.0 * log10(r * delta)
                   + model->magnitude_linear * phase_angle
                   + model->magnitude_quadratic * phase_angle * phase_angle
                   + model->magnitude_cubic * phase_angle * phase_angle * phase_angle
                   + model->magnitude_quartic * phase_angle * phase_angle * phase_angle * phase_angle;
        return true;
    }

    if (model->brightness_model == ALMANAC_BRIGHTNESS_MODEL_LUNAR_PHASE) {
        *magnitude = model->magnitude_constant
                   + model->magnitude_linear * phase_angle
                   + model->magnitude_quadratic * phase_angle * phase_angle
                   + model->magnitude_cubic * phase_angle * phase_angle * phase_angle
                   + model->magnitude_quartic * phase_angle * phase_angle * phase_angle * phase_angle
                   + 5.0 * log10(delta / (384400.0 / 149597870.7));
        return true;
    }

    return true;
}

static bool almanac_compute_state_for_model(almanac_t *almanac,
                                            const almanac_model_row_t *model,
                                            const datetime_t *moment,
                                            almanac_state_t *state)
{
    double ephemeris_jd;

    if (!almanac || !model || !moment || !state) {
        almanac_set_error(almanac, "invalid almanac computation request");
        return false;
    }

    ephemeris_jd = datetime_jd_tdb(moment);
    if (ephemeris_jd == DBL_MAX) {
        almanac_set_error(almanac, "failed to derive Julian dates for almanac computation");
        return false;
    }
    memset(state, 0, sizeof(*state));
    if (model->model_kind == ALMANAC_MODEL_KIND_FIXED_EQUATORIAL) {
        if (!almanac_equatorial_from_fixed(model, ephemeris_jd, state)) {
            almanac_set_error(almanac, "failed to compute fixed-star position");
            return false;
        }
    } else if (model->model_kind == ALMANAC_MODEL_KIND_CHEBYSHEV_POSITION) {
        if (!almanac_equatorial_from_chebyshev(almanac, model, ephemeris_jd, state)) {
            if (!string_length(almanac->error))
                almanac_set_error(almanac, "failed to compute Chebyshev position");
            return false;
        }
    } else if (model->model_kind == ALMANAC_MODEL_KIND_LUNAR_CHEBYSHEV) {
        if (!almanac_equatorial_from_moon(almanac, model, ephemeris_jd, state)) {
            if (!string_length(almanac->error))
                almanac_set_error(almanac, "failed to compute lunar position");
            return false;
        }
    } else {
        almanac_set_error(almanac, "unsupported almanac model kind");
        return false;
    }

    if (!state->apparent_place_complete &&
        !almanac_apply_apparent_direction_corrections(almanac, model, ephemeris_jd, state)) {
        if (!string_length(almanac->error))
            almanac_set_error(almanac, "failed to apply apparent-place corrections");
        return false;
    }

    if (state->has_direction_vector)
        almanac_state_fill_equatorial(state);
    return true;
}

static bool almanac_compute_entry(almanac_t *almanac,
                                  const almanac_model_row_t *model,
                                  const datetime_t *moment,
                                  almanac_entry_t *out)
{
    double civil_jd;
    almanac_state_t state;
    almanac_state_t sun_state;

    if (!almanac || !model || !moment || !out) {
        almanac_set_error(almanac, "invalid almanac computation request");
        return false;
    }
    civil_jd = datetime_jd(moment);
    if (civil_jd == DBL_MAX) {
        almanac_set_error(almanac, "failed to derive Julian dates for almanac computation");
        return false;
    }
    memset(&sun_state, 0, sizeof(sun_state));
    if (!almanac_compute_state_for_model(almanac, model, moment, &state))
        return false;

    memset(out, 0, sizeof(*out));
    out->body_id = model->body_id;
    out->moment_jd = civil_jd;
    out->body_kind = model->body_kind;
    out->gha_aries_degrees = almanac_apparent_gha_aries_for_jd(almanac, civil_jd);
    if (out->gha_aries_degrees == DBL_MAX) {
        almanac_set_error(almanac, "failed to compute apparent GHA of Aries");
        return false;
    }
    out->right_ascension_hours = state.right_ascension_hours;
    out->sha_degrees = normalize_degrees(360.0 - (state.right_ascension_hours * 15.0));
    out->declination_degrees = state.declination_degrees;
    out->geocentric_distance_au = state.has_geocentric_vector ? cartesian_length(&state.geocentric_equatorial_au) : NAN;
    out->heliocentric_distance_au = NAN;
    out->phase_angle_degrees = NAN;
    out->visual_magnitude = NAN;

    if (model->brightness_model != ALMANAC_BRIGHTNESS_MODEL_CATALOGUED &&
        model->brightness_model != ALMANAC_BRIGHTNESS_MODEL_NONE &&
        model->body_id != ALMANAC_BODY_ID_SUN) {
        almanac_model_row_t sun_model;

        if (!almanac_fetch_model(almanac, ALMANAC_BODY_ID_SUN, &sun_model))
            return false;
        if (!almanac_compute_state_for_model(almanac, &sun_model, moment, &sun_state))
            return false;
    }

    if (!almanac_visual_magnitude(model,
                                  &state,
                                  model->body_id == ALMANAC_BODY_ID_SUN ? NULL : &sun_state,
                                  &out->visual_magnitude,
                                  &out->phase_angle_degrees,
                                  &out->heliocentric_distance_au)) {
        almanac_set_error(almanac, "failed to compute visual magnitude");
        return false;
    }
    if (model->body_id == ALMANAC_BODY_ID_MOON &&
        out->phase_angle_degrees == out->phase_angle_degrees &&
        out->phase_angle_degrees >= 175.0) {
        out->visual_magnitude = NAN;
    }
    return true;
}

almanac_t *almanac_open(void)
{
    almanac_t *almanac;
    char *path_text = NULL;
    char *key_text = NULL;
    char *configured = NULL;
    string_t *path = NULL;
    string_t *key = NULL;

    almanac = calloc(1u, sizeof(*almanac));
    if (!almanac)
        return NULL;
    almanac->error = string_new();
    if (!almanac->error) {
        free(almanac);
        return NULL;
    }

    configured = getenv("MARS_ALMANAC_DB_PATH") ? dup_c_string(getenv("MARS_ALMANAC_DB_PATH")) : NULL;
    if (!configured)
        configured = almanac_config_lookup("MARS_ALMANAC_DB_PATH");
    path_text = configured ? configured : almanac_default_db_path();

    configured = getenv("MARS_ALMANAC_DB_KEY") ? dup_c_string(getenv("MARS_ALMANAC_DB_KEY")) : NULL;
    if (!configured)
        configured = almanac_config_lookup("MARS_ALMANAC_DB_KEY");
    key_text = configured;

    path = string_new_from_cstr(path_text);
    key = string_new_from_cstr(key_text);
    if (!path || !key) {
        almanac_set_error(almanac,
                          "almanac configuration is incomplete; provide MARS_ALMANAC_DB_KEY and an almanac database");
        string_free(path);
        string_free(key);
        free(path_text);
        free(key_text);
        return almanac;
    }

    almanac->db = sqlite_open_encrypted(path, key);
    if (!almanac->db)
        almanac_set_error(almanac, "failed to open encrypted almanac database");

    string_free(path);
    string_free(key);
    free(path_text);
    free(key_text);
    return almanac;
}

void almanac_close(almanac_t *almanac)
{
    if (!almanac)
        return;
    free(almanac->nutation_terms);
    sqlite_close(almanac->db);
    string_free(almanac->error);
    free(almanac);
}

const char *almanac_last_error(const almanac_t *almanac)
{
    return (almanac && almanac->error) ? string_c_str(almanac->error) : NULL;
}

bool almanac_gha_aries(almanac_t *almanac,
                       const datetime_t *moment,
                       double *gha_aries_degrees)
{
    if (!almanac || !moment || !gha_aries_degrees) {
        almanac_set_error(almanac, "invalid GHA Aries request");
        return false;
    }
    if (!almanac->db) {
        almanac_set_error(almanac, "almanac database is not open");
        return false;
    }
    *gha_aries_degrees = almanac_gha_aries_for_jd(almanac, datetime_jd(moment));
    if (*gha_aries_degrees == DBL_MAX) {
        if (!string_length(almanac->error))
            almanac_set_error(almanac, "failed to compute GHA of Aries");
        return false;
    }
    return true;
}

static bool almanac_entry_fill_body(almanac_t *almanac,
                                    almanac_body_id_t body_id,
                                    const datetime_t *moment,
                                    almanac_entry_t *out)
{
    almanac_model_row_t model;

    if (!almanac || !moment || !out || body_id <= ALMANAC_BODY_ID_UNKNOWN ||
        body_id >= ALMANAC_BODY_ID_COUNT) {
        almanac_set_error(almanac, "invalid almanac entry request");
        return false;
    }
    if (!almanac->db) {
        almanac_set_error(almanac, "almanac database is not open");
        return false;
    }
    if (!almanac_fetch_model(almanac, body_id, &model))
        return false;
    return almanac_compute_entry(almanac, &model, moment, out);
}

almanac_entry_t *almanac_new_body_entry(almanac_t *almanac,
                                        almanac_body_id_t body_id,
                                        const datetime_t *moment)
{
    almanac_entry_t *entry;

    entry = calloc(1u, sizeof(*entry));
    if (!entry) {
        almanac_set_error(almanac, "failed to allocate almanac entry");
        return NULL;
    }
    if (!almanac_entry_fill_body(almanac, body_id, moment, entry)) {
        free(entry);
        return NULL;
    }
    return entry;
}

almanac_entry_t *almanac_new_entry(almanac_t *almanac,
                                   const char *body_code,
                                   const datetime_t *moment)
{
    almanac_body_id_t body_id = almanac_body_id_from_code(body_code);

    if (body_id == ALMANAC_BODY_ID_UNKNOWN) {
        almanac_set_error(almanac, "requested almanac body was not found");
        return NULL;
    }
    return almanac_new_body_entry(almanac, body_id, moment);
}

static bool almanac_observer_is_valid(almanac_t *almanac,
                                      const almanac_observer_t *observer)
{
    if (!observer) {
        almanac_set_error(almanac, "invalid almanac observer");
        return false;
    }
    if (!isfinite(observer->latitude_degrees) ||
        !isfinite(observer->longitude_degrees) ||
        !isfinite(observer->elevation_metres)) {
        almanac_set_error(almanac, "observer coordinates must be finite");
        return false;
    }
    if (observer->latitude_degrees < -90.0 || observer->latitude_degrees > 90.0) {
        almanac_set_error(almanac, "observer latitude must be in [-90, 90]");
        return false;
    }
    if (observer->longitude_degrees < -360.0 || observer->longitude_degrees > 360.0) {
        almanac_set_error(almanac, "observer longitude must be in [-360, 360]");
        return false;
    }
    return true;
}

static bool almanac_body_horizon_geometry(almanac_t *almanac,
                                          almanac_body_id_t body_id,
                                          const almanac_observer_t *observer,
                                          double jd,
                                          almanac_horizon_geometry_t *out);

bool almanac_observables(almanac_t *almanac,
                         const almanac_entry_t *body,
                         const almanac_observer_t *observer,
                         almanac_observables_t *out)
{
    double gha_body_degrees;
    double lha_radians;
    double latitude_radians;
    double declination_radians;
    double sin_altitude;
    double altitude_radians;
    double azimuth_radians;
    double radius_au;
    double semi_diameter_radians = NAN;
    almanac_horizon_geometry_t geometry;

    if (!almanac || !body || !observer || !out) {
        almanac_set_error(almanac, "invalid almanac observables request");
        return false;
    }
    if (!almanac_observer_is_valid(almanac, observer))
        return false;

    if (body->moment_jd > 0.0 &&
        isfinite(body->moment_jd) &&
        almanac_body_id_radius_au(body->body_id, &radius_au) &&
        almanac_body_horizon_geometry(almanac,
                                      body->body_id,
                                      observer,
                                      body->moment_jd,
                                      &geometry)) {
        memset(out, 0, sizeof(*out));
        out->altitude_degrees = geometry.altitude_degrees;
        out->azimuth_degrees = geometry.azimuth_degrees;
        out->semi_diameter_degrees = geometry.semi_diameter_degrees;
        out->above_horizon = out->altitude_degrees > 0.0;
        out->visible = out->altitude_degrees +
                       (out->semi_diameter_degrees == out->semi_diameter_degrees
                            ? out->semi_diameter_degrees
                            : 0.0) > 0.0;
        return true;
    }

    gha_body_degrees = normalize_degrees(body->gha_aries_degrees + body->sha_degrees);
    lha_radians = degrees_to_radians(normalize_degrees(gha_body_degrees + observer->longitude_degrees));
    latitude_radians = degrees_to_radians(observer->latitude_degrees);
    declination_radians = degrees_to_radians(body->declination_degrees);

    sin_altitude = sin(latitude_radians) * sin(declination_radians) +
                   cos(latitude_radians) * cos(declination_radians) * cos(lha_radians);
    altitude_radians = asin(clamp_unit(sin_altitude));
    azimuth_radians = atan2(sin(lha_radians),
                            cos(lha_radians) * sin(latitude_radians) -
                                tan(declination_radians) * cos(latitude_radians));
    azimuth_radians = normalize_radians_positive(azimuth_radians + M_PI);

    if (body->geocentric_distance_au > 0.0 &&
        almanac_body_radius_au(body, &radius_au) &&
        radius_au > 0.0 &&
        body->geocentric_distance_au >= radius_au) {
        semi_diameter_radians = asin(clamp_unit(radius_au / body->geocentric_distance_au));
    }

    memset(out, 0, sizeof(*out));
    out->altitude_degrees = radians_to_degrees(altitude_radians);
    out->azimuth_degrees = radians_to_degrees(azimuth_radians);
    out->semi_diameter_degrees = semi_diameter_radians == semi_diameter_radians
        ? radians_to_degrees(semi_diameter_radians)
        : NAN;
    out->above_horizon = out->altitude_degrees > 0.0;
    out->visible = out->altitude_degrees +
                       (out->semi_diameter_degrees == out->semi_diameter_degrees
                            ? out->semi_diameter_degrees
                            : 0.0) > 0.0;
    return true;
}

bool almanac_phase_details(const almanac_entry_t *body,
                           almanac_phase_details_t *out)
{
    double phase_angle_radians;
    double illuminated_fraction;

    if (!body || !out)
        return false;

    memset(out, 0, sizeof(*out));
    out->phase_angle_degrees = body->phase_angle_degrees;
    out->illuminated_fraction = NAN;
    out->phase_class = ALMANAC_PHASE_UNKNOWN;

    if (!(body->phase_angle_degrees == body->phase_angle_degrees))
        return true;

    phase_angle_radians = degrees_to_radians(body->phase_angle_degrees);
    illuminated_fraction = 0.5 * (1.0 + cos(phase_angle_radians));
    if (illuminated_fraction < 0.0)
        illuminated_fraction = 0.0;
    if (illuminated_fraction > 1.0)
        illuminated_fraction = 1.0;
    out->illuminated_fraction = illuminated_fraction;

    if (body->phase_angle_degrees <= 5.0)
        out->phase_class = ALMANAC_PHASE_FULL;
    else if (body->phase_angle_degrees >= 175.0)
        out->phase_class = ALMANAC_PHASE_NEW;
    else if (fabs(body->phase_angle_degrees - 90.0) <= 10.0)
        out->phase_class = ALMANAC_PHASE_QUARTER;
    else if (illuminated_fraction < 0.5)
        out->phase_class = ALMANAC_PHASE_CRESCENT;
    else
        out->phase_class = ALMANAC_PHASE_GIBBOUS;
    return true;
}

static bool almanac_state_for_body_at_jd(almanac_t *almanac,
                                         almanac_body_id_t body_id,
                                         double jd,
                                         almanac_state_t *out_state)
{
    datetime_t *moment;
    almanac_model_row_t model;
    bool ok;

    if (!almanac || body_id <= ALMANAC_BODY_ID_UNKNOWN ||
        body_id >= ALMANAC_BODY_ID_COUNT || !out_state) {
        almanac_set_error(almanac, "invalid almanac state request");
        return false;
    }
    moment = datetime_alloc();
    if (!moment) {
        almanac_set_error(almanac, "failed to allocate datetime for almanac state request");
        return false;
    }
    if (!datetime_init_jd(moment, jd)) {
        datetime_dealloc(moment);
        almanac_set_error(almanac, "failed to initialise datetime for almanac state request");
        return false;
    }
    ok = almanac_fetch_model(almanac, body_id, &model) &&
         almanac_compute_state_for_model(almanac, &model, moment, out_state);
    datetime_dealloc(moment);
    return ok;
}

static bool almanac_entry_fill_at_jd(almanac_t *almanac,
                                     almanac_body_id_t body_id,
                                     double jd,
                                     almanac_entry_t *out_entry)
{
    datetime_t *moment;
    bool ok;

    if (!almanac || body_id <= ALMANAC_BODY_ID_UNKNOWN ||
        body_id >= ALMANAC_BODY_ID_COUNT || !out_entry) {
        almanac_set_error(almanac, "invalid almanac entry-at-jd request");
        return false;
    }
    moment = datetime_alloc();
    if (!moment) {
        almanac_set_error(almanac, "failed to allocate datetime for almanac entry");
        return false;
    }
    if (!datetime_init_jd(moment, jd)) {
        datetime_dealloc(moment);
        almanac_set_error(almanac, "failed to initialise datetime for almanac entry");
        return false;
    }
    ok = almanac_entry_fill_body(almanac, body_id, moment, out_entry);
    datetime_dealloc(moment);
    return ok;
}

static double almanac_body_sun_longitude_residual_degrees(almanac_t *almanac,
                                                          almanac_body_id_t body_id,
                                                          double jd,
                                                          double target_degrees)
{
    almanac_state_t body_state;
    almanac_state_t sun_state;
    double body_longitude;
    double sun_longitude;

    if (!almanac_state_for_body_at_jd(almanac, body_id, jd, &body_state) ||
        !almanac_state_for_body_at_jd(almanac, ALMANAC_BODY_ID_SUN, jd, &sun_state)) {
        return NAN;
    }
    if (!body_state.has_ecliptic_vector || !sun_state.has_ecliptic_vector)
        return NAN;
    body_longitude = ecliptic_longitude_degrees(&body_state.geocentric_ecliptic_au);
    sun_longitude = ecliptic_longitude_degrees(&sun_state.geocentric_ecliptic_au);
    return normalize_degrees_signed((body_longitude - sun_longitude) - target_degrees);
}

static double almanac_moon_phase_target_degrees(almanac_moon_phase_kind_t kind)
{
    static const double target_degrees_by_kind[ALMANAC_MOON_PHASE_LAST_QUARTER + 1] = {
        [ALMANAC_MOON_PHASE_NEW] = 0.0,
        [ALMANAC_MOON_PHASE_FIRST_QUARTER] = 90.0,
        [ALMANAC_MOON_PHASE_FULL] = 180.0,
        [ALMANAC_MOON_PHASE_LAST_QUARTER] = 270.0
    };

    if (kind < ALMANAC_MOON_PHASE_NEW || kind > ALMANAC_MOON_PHASE_LAST_QUARTER)
        return NAN;
    return target_degrees_by_kind[kind];
}

static double almanac_mean_moon_phase_jd(double k)
{
    double T = k / 1236.85;

    return 2451550.09766
         + 29.530588861 * k
         + 0.00015437 * T * T
         - 0.000000150 * T * T * T
         + 0.00000000073 * T * T * T * T;
}

static double almanac_moon_argument_latitude_radians(double k)
{
    double T = k / 1236.85;
    double degrees = 160.7108
                   + 390.67050284 * k
                   - 0.0016118 * T * T
                   - 0.00000227 * T * T * T
                   + 0.000000011 * T * T * T * T;

    return degrees_to_radians(normalize_degrees(degrees));
}

static bool almanac_eclipse_candidate_near_node(double k)
{
    /*
     * ESAA 8.22 requires conjunction or opposition near a lunar node.
     * The 0.36 limit is deliberately conservative (about 21 degrees in
     * argument of latitude); exact geometry remains the final test.
     */
    return fabs(sin(almanac_moon_argument_latitude_radians(k))) <= 0.36;
}

static bool almanac_refine_body_sun_longitude_near(almanac_t *almanac,
                                                   almanac_body_id_t body_id,
                                                   double target_degrees,
                                                   double estimate_jd,
                                                   double max_step_days,
                                                   double *out_jd)
{
    static const double derivative_step_days = 1.0 / 48.0;
    double jd = estimate_jd;
    int iteration;

    if (!almanac || !out_jd || !(max_step_days > 0.0))
        return false;
    for (iteration = 0; iteration < 12; ++iteration) {
        double value = almanac_body_sun_longitude_residual_degrees(almanac,
                                                                   body_id,
                                                                   jd,
                                                                   target_degrees);
        double before;
        double after;
        double slope;
        double correction;

        if (!(value == value))
            return false;
        if (fabs(value) < 1e-9) {
            *out_jd = jd;
            return true;
        }
        before = almanac_body_sun_longitude_residual_degrees(almanac,
                                                              body_id,
                                                              jd - derivative_step_days,
                                                              target_degrees);
        after = almanac_body_sun_longitude_residual_degrees(almanac,
                                                             body_id,
                                                             jd + derivative_step_days,
                                                             target_degrees);
        if (!(before == before) || !(after == after))
            return false;
        slope = normalize_degrees_signed(after - before) / (2.0 * derivative_step_days);
        if (fabs(slope) < 1e-9)
            return false;
        correction = value / slope;
        if (correction > max_step_days)
            correction = max_step_days;
        else if (correction < -max_step_days)
            correction = -max_step_days;
        jd -= correction;
        if (fabs(correction) < 1e-9) {
            *out_jd = jd;
            return true;
        }
    }
    if (fabs(almanac_body_sun_longitude_residual_degrees(almanac,
                                                          body_id,
                                                          jd,
                                                          target_degrees)) >= 1e-7) {
        return false;
    }
    *out_jd = jd;
    return true;
}

bool almanac_next_moon_phase_exact(almanac_t *almanac,
                                   const datetime_t *after,
                                   almanac_moon_phase_kind_t kind,
                                   almanac_moon_phase_event_t *out)
{
    static const double synodic_month_days = 29.530588861;
    static const double base_new_moon_jd = 2451550.09766;
    double target_degrees;
    double phase_fraction;
    double after_jd;
    double k;
    double guess_jd;
    double root_jd;
    int attempt;
    almanac_entry_t moon_entry;
    almanac_phase_details_t phase_details;

    if (!almanac || !after || !out) {
        almanac_set_error(almanac, "invalid exact moon phase request");
        return false;
    }
    target_degrees = almanac_moon_phase_target_degrees(kind);
    if (!(target_degrees == target_degrees)) {
        almanac_set_error(almanac, "unsupported Moon phase kind");
        return false;
    }
    after_jd = datetime_jd(after);
    if (after_jd == DBL_MAX) {
        almanac_set_error(almanac, "failed to derive Julian date for Moon phase search");
        return false;
    }

    phase_fraction = target_degrees / 360.0;
    k = floor((after_jd - base_new_moon_jd) / synodic_month_days - phase_fraction)
      + 1.0
      + phase_fraction;

    root_jd = NAN;
    for (attempt = 0; attempt < 2; ++attempt) {
        guess_jd = almanac_mean_moon_phase_jd(k);
        if (!almanac_refine_body_sun_longitude_near(almanac,
                                                     ALMANAC_BODY_ID_MOON,
                                                     target_degrees,
                                                     guess_jd,
                                                     2.0,
                                                     &root_jd)) {
            almanac_set_error(almanac, "failed to bracket exact Moon phase");
            return false;
        }
        if (root_jd > after_jd + 1e-9)
            break;
        k += 1.0;
    }
    if (!(root_jd > after_jd + 1e-9)) {
        almanac_set_error(almanac, "failed to bracket exact Moon phase");
        return false;
    }
    if (!almanac_entry_fill_at_jd(almanac, ALMANAC_BODY_ID_MOON, root_jd, &moon_entry) ||
        !almanac_phase_details(&moon_entry, &phase_details)) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->kind = kind;
    out->time.valid = true;
    out->time.jd = root_jd;
    out->time.local_jd = root_jd;
    out->phase_angle_degrees = moon_entry.phase_angle_degrees;
    out->illuminated_fraction = phase_details.illuminated_fraction;
    return true;
}

static bool almanac_event_window_is_valid(almanac_t *almanac,
                                          const datetime_t *start,
                                          const datetime_t *end,
                                          double *out_start_jd,
                                          double *out_end_jd)
{
    double start_jd;
    double end_jd;

    if (!almanac || !start || !end || !out_start_jd || !out_end_jd) {
        almanac_set_error(almanac, "invalid almanac event window");
        return false;
    }
    start_jd = datetime_jd(start);
    end_jd = datetime_jd(end);
    if (start_jd == DBL_MAX || end_jd == DBL_MAX) {
        almanac_set_error(almanac, "failed to derive Julian date for almanac event window");
        return false;
    }
    if (end_jd < start_jd) {
        almanac_set_error(almanac, "almanac event window end precedes start");
        return false;
    }
    *out_start_jd = start_jd;
    *out_end_jd = end_jd;
    return true;
}

static void almanac_event_time_from_jds(double jd, double local_jd, almanac_event_time_t *out)
{
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    out->jd = NAN;
    out->local_jd = NAN;
    if (!(jd == jd) || !(local_jd == local_jd))
        return;

    out->valid = true;
    out->jd = jd;
    out->local_jd = local_jd;
}

static void almanac_event_time_from_jd(double jd, almanac_event_time_t *out)
{
    almanac_event_time_from_jds(jd, jd, out);
}

bool almanac_event_time_datetime(const almanac_event_time_t *event_time,
                                 datetime_t *out)
{
    if (!event_time || !event_time->valid || !out ||
        !(event_time->local_jd == event_time->local_jd)) {
        return false;
    }
    return datetime_init_jd(out, event_time->local_jd) != NULL;
}

static bool almanac_local_offset_for_jd(almanac_t *almanac,
                                        jurisdiction_t *jurisdiction,
                                        double local_jd,
                                        double *out_offset_hours)
{
    datetime_t *local_date;
    bool ok;

    if (!almanac || !jurisdiction || !out_offset_hours || !(local_jd == local_jd)) {
        almanac_set_error(almanac, "invalid local almanac time conversion");
        return false;
    }

    local_date = datetime_alloc();
    if (!local_date) {
        almanac_set_error(almanac, "failed to allocate local almanac datetime");
        return false;
    }
    if (!datetime_init_jd(local_date, local_jd)) {
        datetime_dealloc(local_date);
        almanac_set_error(almanac, "failed to initialise local almanac datetime");
        return false;
    }
    ok = jurisdict_default_gmt_offset(jurisdiction, local_date, out_offset_hours);
    datetime_dealloc(local_date);
    if (!ok) {
        const char *jurisdiction_error = jurisdict_last_error(jurisdiction);

        almanac_set_error(almanac,
                          jurisdiction_error ? jurisdiction_error
                                             : "failed to resolve jurisdiction GMT offset");
        return false;
    }
    return true;
}

static bool almanac_local_event_time_from_utc_jd(almanac_t *almanac,
                                                jurisdiction_t *jurisdiction,
                                                double utc_jd,
                                                double offset_guess_hours,
                                                almanac_event_time_t *out)
{
    double offset_hours = offset_guess_hours;
    double local_jd;
    int iteration;

    if (!out)
        return false;
    memset(out, 0, sizeof(*out));
    out->jd = NAN;
    if (!(utc_jd == utc_jd))
        return true;

    for (iteration = 0; iteration < 4; ++iteration) {
        double resolved_offset_hours;

        local_jd = utc_jd + offset_hours / 24.0;
        if (!almanac_local_offset_for_jd(almanac,
                                         jurisdiction,
                                         local_jd,
                                         &resolved_offset_hours)) {
            return false;
        }
        if (fabs(resolved_offset_hours - offset_hours) < 1e-9) {
            offset_hours = resolved_offset_hours;
            break;
        }
        offset_hours = resolved_offset_hours;
    }

    local_jd = utc_jd + offset_hours / 24.0;
    almanac_event_time_from_jds(utc_jd, local_jd, out);
    return true;
}

static bool almanac_local_day_utc_window_for_jdn(almanac_t *almanac,
                                                 jurisdiction_t *jurisdiction,
                                                 long jdn,
                                                 double *out_start_jd,
                                                 double *out_end_jd,
                                                 double *out_offset_guess_hours)
{
    datetime_t *local_start;
    datetime_t *local_end;
    double offset_start_hours;
    double offset_end_hours;
    double local_start_jd;
    bool ok;

    if (!almanac || !jurisdiction || jdn == LONG_MAX || !out_start_jd ||
        !out_end_jd || !out_offset_guess_hours) {
        almanac_set_error(almanac, "invalid almanac local day request");
        return false;
    }

    local_start = datetime_alloc();
    local_end = datetime_alloc();
    if (!local_start || !local_end) {
        datetime_dealloc(local_end);
        datetime_dealloc(local_start);
        almanac_set_error(almanac, "failed to allocate local day datetimes");
        return false;
    }

    ok = datetime_init_jdn(local_start, jdn) &&
         datetime_init_jdn(local_end, jdn + 1L) &&
         jurisdict_default_gmt_offset(jurisdiction, local_start, &offset_start_hours) &&
         jurisdict_default_gmt_offset(jurisdiction, local_end, &offset_end_hours);
    if (!ok) {
        const char *jurisdiction_error = jurisdict_last_error(jurisdiction);

        datetime_dealloc(local_end);
        datetime_dealloc(local_start);
        almanac_set_error(almanac,
                          jurisdiction_error ? jurisdiction_error
                                             : "failed to resolve jurisdiction GMT offset");
        return false;
    }

    local_start_jd = (double)jdn - 0.5;
    *out_start_jd = local_start_jd - offset_start_hours / 24.0;
    *out_end_jd = local_start_jd + 1.0 - offset_end_hours / 24.0;
    *out_offset_guess_hours = offset_start_hours;

    datetime_dealloc(local_end);
    datetime_dealloc(local_start);
    return true;
}

static bool almanac_observer_equatorial_position_au(almanac_t *almanac,
                                                    const almanac_observer_t *observer,
                                                    double jd,
                                                    cartesian3_t *out)
{
    static const double earth_equatorial_radius_au = 6378.137 / 149597870.7;
    static const double earth_flattening = 1.0 / 298.257223563;
    static const double au_per_metre = 1.0 / 149597870700.0;
    double latitude;
    double lst;
    double sin_latitude;
    double cos_latitude;
    double eccentricity2;
    double prime_vertical;
    double rho_xy;
    double rho_z;
    double gha_aries;

    if (!almanac || !observer || !out)
        return false;
    if (!almanac_observer_is_valid(almanac, observer))
        return false;
    gha_aries = almanac_apparent_gha_aries_for_jd(almanac, jd);
    if (gha_aries == DBL_MAX)
        return false;

    latitude = degrees_to_radians(observer->latitude_degrees);
    lst = degrees_to_radians(normalize_degrees(gha_aries + observer->longitude_degrees));
    sincos(latitude, &sin_latitude, &cos_latitude);
    eccentricity2 = earth_flattening * (2.0 - earth_flattening);
    prime_vertical = earth_equatorial_radius_au /
        sqrt(1.0 - eccentricity2 * sin_latitude * sin_latitude);
    rho_xy = (prime_vertical + observer->elevation_metres * au_per_metre) * cos_latitude;
    rho_z = ((1.0 - eccentricity2) * prime_vertical +
             observer->elevation_metres * au_per_metre) * sin_latitude;

    out->x = rho_xy * cos(lst);
    out->y = rho_xy * sin(lst);
    out->z = rho_z;
    return true;
}

static bool almanac_topocentric_vector(almanac_t *almanac,
                                       const cartesian3_t *geocentric_equatorial_au,
                                       const almanac_observer_t *observer,
                                       double jd,
                                       cartesian3_t *out)
{
    cartesian3_t observer_position;

    if (!geocentric_equatorial_au || !out)
        return false;
    if (!almanac_observer_equatorial_position_au(almanac, observer, jd, &observer_position))
        return false;
    *out = cartesian_subtract(geocentric_equatorial_au, &observer_position);
    return true;
}

static double almanac_topocentric_altitude_degrees(almanac_t *almanac,
                                                  const cartesian3_t *topocentric_equatorial_au,
                                                  const almanac_observer_t *observer,
                                                  double jd)
{
    double gha_aries;
    double latitude;
    double lst;
    double sin_latitude;
    double cos_latitude;
    cartesian3_t zenith;
    cartesian3_t direction;

    if (!almanac || !topocentric_equatorial_au || !observer)
        return NAN;
    if (!almanac_observer_is_valid(almanac, observer))
        return NAN;
    gha_aries = almanac_apparent_gha_aries_for_jd(almanac, jd);
    if (gha_aries == DBL_MAX)
        return NAN;

    direction = *topocentric_equatorial_au;
    if (!cartesian_normalize_in_place(&direction))
        return NAN;
    latitude = degrees_to_radians(observer->latitude_degrees);
    lst = degrees_to_radians(normalize_degrees(gha_aries + observer->longitude_degrees));
    sincos(latitude, &sin_latitude, &cos_latitude);
    zenith.x = cos_latitude * cos(lst);
    zenith.y = cos_latitude * sin(lst);
    zenith.z = sin_latitude;

    return radians_to_degrees(asin(clamp_unit(cartesian_dot(&direction, &zenith))));
}

static bool almanac_topocentric_horizontal_degrees(almanac_t *almanac,
                                                  const cartesian3_t *topocentric_equatorial_au,
                                                  const almanac_observer_t *observer,
                                                  double jd,
                                                  double *out_altitude_degrees,
                                                  double *out_azimuth_degrees)
{
    double gha_aries;
    double latitude;
    double lst;
    double sin_latitude;
    double cos_latitude;
    cartesian3_t direction;
    cartesian3_t zenith;
    cartesian3_t north;
    cartesian3_t east;
    double altitude;
    double azimuth;

    if (!almanac || !topocentric_equatorial_au || !observer ||
        !out_altitude_degrees || !out_azimuth_degrees)
        return false;
    if (!almanac_observer_is_valid(almanac, observer))
        return false;

    gha_aries = almanac_apparent_gha_aries_for_jd(almanac, jd);
    if (gha_aries == DBL_MAX)
        return false;

    direction = *topocentric_equatorial_au;
    if (!cartesian_normalize_in_place(&direction))
        return false;

    latitude = degrees_to_radians(observer->latitude_degrees);
    lst = degrees_to_radians(normalize_degrees(gha_aries + observer->longitude_degrees));
    sincos(latitude, &sin_latitude, &cos_latitude);

    zenith.x = cos_latitude * cos(lst);
    zenith.y = cos_latitude * sin(lst);
    zenith.z = sin_latitude;
    north.x = -sin_latitude * cos(lst);
    north.y = -sin_latitude * sin(lst);
    north.z = cos_latitude;
    east.x = -sin(lst);
    east.y = cos(lst);
    east.z = 0.0;

    altitude = asin(clamp_unit(cartesian_dot(&direction, &zenith)));
    azimuth = atan2(cartesian_dot(&direction, &east),
                    cartesian_dot(&direction, &north));

    *out_altitude_degrees = radians_to_degrees(altitude);
    *out_azimuth_degrees = radians_to_degrees(normalize_radians_positive(azimuth));
    return true;
}

typedef struct almanac_horizon_context_t {
    almanac_body_id_t body_id;
    const almanac_observer_t *observer;
} almanac_horizon_context_t;

static double almanac_horizon_dip_degrees(double elevation_metres)
{
    static const double earth_mean_radius_metres = 6371008.8;

    if (!(elevation_metres > 0.0))
        return 0.0;
    return radians_to_degrees(acos(earth_mean_radius_metres /
                                  (earth_mean_radius_metres + elevation_metres)));
}

static bool almanac_body_horizon_geometry(almanac_t *almanac,
                                          almanac_body_id_t body_id,
                                          const almanac_observer_t *observer,
                                          double jd,
                                          almanac_horizon_geometry_t *out)
{
    almanac_state_t body_state;
    cartesian3_t body_topocentric;
    double body_distance;
    double radius_au;

    if (!almanac || body_id <= ALMANAC_BODY_ID_UNKNOWN ||
        body_id >= ALMANAC_BODY_ID_COUNT || !observer || !out) {
        return false;
    }
    if (!almanac_body_id_radius_au(body_id, &radius_au))
        return false;
    if (!almanac_state_for_body_at_jd(almanac, body_id, jd, &body_state))
        return false;

    if (!almanac_topocentric_vector(almanac,
                                    &body_state.geocentric_equatorial_au,
                                    observer,
                                    jd,
                                    &body_topocentric)) {
        return false;
    }

    body_distance = cartesian_length(&body_topocentric);
    if (!(body_distance > radius_au))
        return false;
    if (!almanac_topocentric_horizontal_degrees(almanac,
                                               &body_topocentric,
                                               observer,
                                               jd,
                                               &out->altitude_degrees,
                                               &out->azimuth_degrees)) {
        return false;
    }
    out->semi_diameter_degrees = radians_to_degrees(asin(clamp_unit(radius_au / body_distance)));
    return true;
}

static double almanac_body_horizon_residual(almanac_t *almanac,
                                            double jd,
                                            void *context)
{
    static const double standard_refraction_degrees = 34.0 / 60.0;
    almanac_horizon_context_t *horizon_context = context;
    almanac_horizon_geometry_t geometry;
    double horizon_dip;

    if (!horizon_context || !horizon_context->observer)
        return NAN;
    if (!almanac_body_horizon_geometry(almanac,
                                       horizon_context->body_id,
                                       horizon_context->observer,
                                       jd,
                                       &geometry)) {
        return NAN;
    }

    horizon_dip = almanac_horizon_dip_degrees(horizon_context->observer->elevation_metres);
    return geometry.altitude_degrees +
           geometry.semi_diameter_degrees +
           standard_refraction_degrees +
           horizon_dip;
}

static bool almanac_find_body_horizon_crossing(almanac_t *almanac,
                                               almanac_body_id_t body_id,
                                               const almanac_observer_t *observer,
                                               double start_jd,
                                               double end_jd,
                                               bool rising,
                                               double *out_jd)
{
    static const double step_days = 1.0 / 288.0;
    almanac_horizon_context_t context = { body_id, observer };
    double left_jd = start_jd;
    double left_value;
    double probe_jd;

    if (!almanac || !observer || !out_jd || !(end_jd > start_jd))
        return false;

    left_value = almanac_body_horizon_residual(almanac, left_jd, &context);
    if (!(left_value == left_value))
        return false;

    for (probe_jd = start_jd + step_days;
         probe_jd <= end_jd + 1e-12;
         probe_jd += step_days) {
        double right_jd = probe_jd > end_jd ? end_jd : probe_jd;
        double right_value = almanac_body_horizon_residual(almanac, right_jd, &context);

        if (!(right_value == right_value))
            return false;
        if ((rising && left_value <= 0.0 && right_value > 0.0) ||
            (!rising && left_value >= 0.0 && right_value < 0.0)) {
            return almanac_bisect_event_residual(almanac,
                                                 almanac_body_horizon_residual,
                                                 &context,
                                                 left_jd,
                                                 right_jd,
                                                 out_jd);
        }
        left_jd = right_jd;
        left_value = right_value;
    }
    return false;
}

static bool almanac_find_solar_horizon_crossing(almanac_t *almanac,
                                                const almanac_rise_set_day_t *day,
                                                const almanac_observer_t *observer,
                                                bool rising,
                                                double *out_jd)
{
    static const double initial_radius_days = 10.0 / 1440.0;
    static const double max_radius_days = 8.0 / 24.0;
    almanac_horizon_context_t context = { ALMANAC_BODY_ID_SUN, observer };
    datetime_sun_status_t status = DATETIME_SUN_UNAVAILABLE;
    datetime_t *estimate;
    double estimate_utc_jd;
    double radius_days;

    if (!almanac || !day || !observer || !out_jd || !(day->end_jd > day->start_jd))
        return false;

    estimate = rising
        ? datetime_init_sunrise_checked(datetime_alloc(),
                                        day->local_jdn,
                                        observer->latitude_degrees,
                                        observer->longitude_degrees,
                                        day->offset_guess_hours,
                                        &status)
        : datetime_init_sunset_checked(datetime_alloc(),
                                       day->local_jdn,
                                       observer->latitude_degrees,
                                       observer->longitude_degrees,
                                       day->offset_guess_hours,
                                       &status);
    if (!estimate)
        return false;
    estimate_utc_jd = datetime_jd(estimate) - day->offset_guess_hours / 24.0;
    datetime_dealloc(estimate);
    if (status != DATETIME_SUN_OK || !(estimate_utc_jd == estimate_utc_jd))
        return false;

    for (radius_days = initial_radius_days;
         radius_days <= max_radius_days + 1e-12;
         radius_days *= 2.0) {
        double left_jd = estimate_utc_jd - radius_days;
        double right_jd = estimate_utc_jd + radius_days;
        double left_value;
        double right_value;

        if (left_jd < day->start_jd)
            left_jd = day->start_jd;
        if (right_jd > day->end_jd)
            right_jd = day->end_jd;
        if (!(right_jd > left_jd))
            continue;

        left_value = almanac_body_horizon_residual(almanac, left_jd, &context);
        right_value = almanac_body_horizon_residual(almanac, right_jd, &context);
        if (!(left_value == left_value) || !(right_value == right_value))
            return false;
        if ((rising && left_value <= 0.0 && right_value > 0.0) ||
            (!rising && left_value >= 0.0 && right_value < 0.0)) {
            return almanac_bisect_event_residual(almanac,
                                                 almanac_body_horizon_residual,
                                                 &context,
                                                 left_jd,
                                                 right_jd,
                                                 out_jd);
        }
    }

    return almanac_find_body_horizon_crossing(almanac,
                                             ALMANAC_BODY_ID_SUN,
                                             observer,
                                             day->start_jd,
                                             day->end_jd,
                                             rising,
                                             out_jd);
}

static almanac_rise_set_status_t almanac_body_rise_set_status_for_day(almanac_t *almanac,
                                                                      almanac_body_id_t body_id,
                                                                      const almanac_observer_t *observer,
                                                                      double start_jd,
                                                                      double end_jd)
{
    almanac_horizon_context_t context = { body_id, observer };
    double step_days = 1.0 / 48.0;
    double probe_jd;
    double max_value = -DBL_MAX;

    for (probe_jd = start_jd; probe_jd <= end_jd + 1e-12; probe_jd += step_days) {
        double value = almanac_body_horizon_residual(almanac, probe_jd, &context);

        if (!(value == value))
            return ALMANAC_RISE_SET_UNAVAILABLE;
        if (value > max_value)
            max_value = value;
    }
    return max_value > 0.0 ? ALMANAC_RISE_SET_NEVER_SETS : ALMANAC_RISE_SET_NEVER_RISES;
}

static void almanac_init_rise_set_event(almanac_rise_set_status_t missing_status,
                                        almanac_rise_set_event_t *event)
{
    if (!event)
        return;
    memset(event, 0, sizeof(*event));
    event->status = missing_status;
    event->time.jd = NAN;
    event->time.local_jd = NAN;
    event->azimuth_degrees = NAN;
}

static bool almanac_fill_rise_set_event(almanac_t *almanac,
                                        jurisdiction_t *jurisdiction,
                                        almanac_body_id_t body_id,
                                        const almanac_observer_t *observer,
                                        double utc_jd,
                                        double offset_guess_hours,
                                        almanac_rise_set_status_t missing_status,
                                        almanac_rise_set_event_t *event)
{
    almanac_horizon_geometry_t geometry;

    if (!event)
        return false;
    almanac_init_rise_set_event(missing_status, event);
    if (!(utc_jd == utc_jd))
        return true;

    if (!almanac_body_horizon_geometry(almanac, body_id, observer, utc_jd, &geometry))
        return false;
    event->status = ALMANAC_RISE_SET_OK;
    event->azimuth_degrees = geometry.azimuth_degrees;
    return almanac_local_event_time_from_utc_jd(almanac,
                                               jurisdiction,
                                               utc_jd,
                                               offset_guess_hours,
                                               &event->time);
}

static bool almanac_prepare_rise_set_window(almanac_t *almanac,
                                            jurisdiction_t *jurisdiction,
                                            const datetime_t *date,
                                            const almanac_observer_t *observer,
                                            const char *error_message,
                                            almanac_rise_set_day_t *day)
{
    long local_jdn;

    if (!almanac || !jurisdiction || !date || !observer ||
        !day) {
        almanac_set_error(almanac, error_message);
        return false;
    }
    if (!almanac_observer_is_valid(almanac, observer))
        return false;
    local_jdn = datetime_jdn(date);
    if (local_jdn == LONG_MAX)
        return false;
    memset(day, 0, sizeof(*day));
    day->local_jdn = local_jdn;
    return almanac_local_day_utc_window_for_jdn(almanac,
                                                jurisdiction,
                                                day->local_jdn,
                                                &day->start_jd,
                                                &day->end_jd,
                                                &day->offset_guess_hours);
}

bool almanac_body_rise_set(almanac_t *almanac,
                           jurisdiction_t *jurisdiction,
                           almanac_body_id_t body_id,
                           const datetime_t *date,
                           const almanac_observer_t *observer,
                           almanac_rise_set_t *out)
{
    almanac_rise_set_day_t day;
    double rise_jd = NAN;
    double set_jd = NAN;
    bool has_rise;
    bool has_set;
    almanac_rise_set_status_t missing_status = ALMANAC_RISE_SET_UNAVAILABLE;
    const char *error_message;

    if (!almanac || !jurisdiction || body_id <= ALMANAC_BODY_ID_UNKNOWN ||
        body_id >= ALMANAC_BODY_ID_COUNT || !date || !observer || !out) {
        almanac_set_error(almanac, "invalid almanac rise/set request");
        return false;
    }
    memset(out, 0, sizeof(*out));
    almanac_init_rise_set_event(ALMANAC_RISE_SET_UNAVAILABLE, &out->rise);
    almanac_init_rise_set_event(ALMANAC_RISE_SET_UNAVAILABLE, &out->set);

    error_message = body_id == ALMANAC_BODY_ID_SUN
        ? "invalid almanac sunrise/sunset request"
        : body_id == ALMANAC_BODY_ID_MOON
            ? "invalid almanac moonrise/moonset request"
            : "invalid almanac rise/set request";
    if (!almanac_prepare_rise_set_window(almanac,
                                         jurisdiction,
                                         date,
                                         observer,
                                         error_message,
                                         &day)) {
        return false;
    }

    if (body_id == ALMANAC_BODY_ID_SUN) {
        has_rise = almanac_find_solar_horizon_crossing(almanac,
                                                       &day,
                                                       observer,
                                                       true,
                                                       &rise_jd);
        has_set = almanac_find_solar_horizon_crossing(almanac,
                                                      &day,
                                                      observer,
                                                      false,
                                                      &set_jd);
    } else {
        has_rise = almanac_find_body_horizon_crossing(almanac,
                                                      body_id,
                                                      observer,
                                                      day.start_jd,
                                                      day.end_jd,
                                                      true,
                                                      &rise_jd);
        has_set = almanac_find_body_horizon_crossing(almanac,
                                                     body_id,
                                                     observer,
                                                     day.start_jd,
                                                     day.end_jd,
                                                     false,
                                                     &set_jd);
    }

    if (!has_rise || !has_set) {
        if (body_id != ALMANAC_BODY_ID_SUN && (has_rise || has_set)) {
            missing_status = ALMANAC_RISE_SET_NOT_ON_DATE;
        } else {
            missing_status = almanac_body_rise_set_status_for_day(almanac,
                                                                  body_id,
                                                                  observer,
                                                                  day.start_jd,
                                                                  day.end_jd);
        }
    }

    return almanac_fill_rise_set_event(almanac,
                                       jurisdiction,
                                       body_id,
                                       observer,
                                       has_rise ? rise_jd : NAN,
                                       day.offset_guess_hours,
                                       missing_status,
                                       &out->rise) &&
           almanac_fill_rise_set_event(almanac,
                                       jurisdiction,
                                       body_id,
                                       observer,
                                       has_set ? set_jd : NAN,
                                       day.offset_guess_hours,
                                       missing_status,
                                       &out->set);
}

bool almanac_sunrise_sunset(almanac_t *almanac,
                            jurisdiction_t *jurisdiction,
                            const datetime_t *date,
                            const almanac_observer_t *observer,
                            almanac_sun_times_t *out)
{
    return almanac_body_rise_set(almanac,
                                 jurisdiction,
                                 ALMANAC_BODY_ID_SUN,
                                 date,
                                 observer,
                                 out);
}

bool almanac_moonrise_moonset(almanac_t *almanac,
                              jurisdiction_t *jurisdiction,
                              const datetime_t *date,
                              const almanac_observer_t *observer,
                              almanac_moon_times_t *out)
{
    return almanac_body_rise_set(almanac,
                                 jurisdiction,
                                 ALMANAC_BODY_ID_MOON,
                                 date,
                                 observer,
                                 out);
}

typedef double (*almanac_event_residual_fn)(almanac_t *almanac, double jd, void *context);
typedef double (*almanac_event_metric_fn)(almanac_t *almanac, double jd, void *context);

typedef struct almanac_solar_eclipse_geometry_t {
    double separation;
    double sun_sd;
    double moon_sd;
    double apparent_separation;
    double sun_altitude_degrees;
} almanac_solar_eclipse_geometry_t;

typedef struct almanac_solar_eclipse_circumstance_t {
    almanac_solar_eclipse_kind_t kind;
    double magnitude;
    double totality_percent;
    bool central;
} almanac_solar_eclipse_circumstance_t;

typedef struct almanac_lunar_eclipse_geometry_t {
    double opposition_error;
    double sun_sd;
    double moon_sd;
    double delta_moon;
    double umbra_radius;
    double penumbra_radius;
    double moon_altitude_degrees;
} almanac_lunar_eclipse_geometry_t;

typedef struct almanac_solar_transit_geometry_t {
    double separation;
    double sun_sd;
    double body_sd;
    almanac_body_id_t body_id;
    double sun_altitude_degrees;
} almanac_solar_transit_geometry_t;

typedef enum almanac_contact_level_t {
    ALMANAC_CONTACT_LEVEL_OUTER = 0,
    ALMANAC_CONTACT_LEVEL_INNER,
    ALMANAC_CONTACT_LEVEL_TOTAL
} almanac_contact_level_t;

typedef struct almanac_eclipse_contact_context_t {
    almanac_body_id_t body_id;
    const almanac_observer_t *observer;
    almanac_contact_level_t contact_level;
} almanac_eclipse_contact_context_t;

static bool almanac_bisect_event_residual(almanac_t *almanac,
                                          almanac_event_residual_fn residual,
                                          void *context,
                                          double left_jd,
                                          double right_jd,
                                          double *out_jd)
{
    double left_value;
    double right_value;
    bool last_replaced_left = false;
    bool last_replaced_right = false;
    int iteration;

    if (!residual || !out_jd)
        return false;
    left_value = residual(almanac, left_jd, context);
    right_value = residual(almanac, right_jd, context);
    if (!(left_value == left_value) || !(right_value == right_value))
        return false;
    if (left_value == 0.0) {
        *out_jd = left_jd;
        return true;
    }
    if (right_value == 0.0) {
        *out_jd = right_jd;
        return true;
    }
    if (left_value * right_value > 0.0)
        return false;

    /*
     * ESAA 8.422 recommends inverse interpolation of a contact
     * discriminant.  Illinois regula falsi keeps the root bracketed like
     * bisection, but normally converges in a handful of ephemeris samples.
     */
    for (iteration = 0; iteration < 24; ++iteration) {
        double candidate_jd =
            (left_jd * right_value - right_jd * left_value) /
            (right_value - left_value);
        double candidate_value;

        if (!(candidate_jd > left_jd && candidate_jd < right_jd))
            candidate_jd = 0.5 * (left_jd + right_jd);
        candidate_value = residual(almanac, candidate_jd, context);
        if (!(candidate_value == candidate_value))
            return false;
        if (fabs(candidate_value) < 1e-9 || fabs(right_jd - left_jd) < 1e-8) {
            *out_jd = candidate_jd;
            return true;
        }
        if (left_value * candidate_value <= 0.0) {
            right_jd = candidate_jd;
            right_value = candidate_value;
            if (last_replaced_right)
                left_value *= 0.5;
            last_replaced_right = true;
            last_replaced_left = false;
        } else {
            left_jd = candidate_jd;
            left_value = candidate_value;
            if (last_replaced_left)
                right_value *= 0.5;
            last_replaced_left = true;
            last_replaced_right = false;
        }
    }

    *out_jd = 0.5 * (left_jd + right_jd);
    return true;
}

static double almanac_find_contact_jd(almanac_t *almanac,
                                      almanac_event_residual_fn residual,
                                      void *context,
                                      double greatest_jd,
                                      double direction,
                                      double max_span_days,
                                      double step_days)
{
    double previous_jd = greatest_jd;
    double previous_value;
    double offset;

    if (!residual || !(direction == -1.0 || direction == 1.0) ||
        !(max_span_days > 0.0) || !(step_days > 0.0)) {
        return NAN;
    }
    previous_value = residual(almanac, previous_jd, context);
    if (!(previous_value == previous_value) || previous_value > 0.0)
        return NAN;

    for (offset = step_days; offset <= max_span_days + 1e-9; offset += step_days) {
        double probe_jd = greatest_jd + direction * offset;
        double probe_value = residual(almanac, probe_jd, context);

        if (!(probe_value == probe_value))
            return NAN;
        if (probe_value > 0.0) {
            double contact_jd;
            double left_jd = direction < 0.0 ? probe_jd : previous_jd;
            double right_jd = direction < 0.0 ? previous_jd : probe_jd;

            if (almanac_bisect_event_residual(almanac,
                                              residual,
                                              context,
                                              left_jd,
                                              right_jd,
                                              &contact_jd)) {
                return contact_jd;
            }
            return NAN;
        }
        previous_jd = probe_jd;
        previous_value = probe_value;
    }

    (void)previous_value;
    return NAN;
}

static double almanac_find_local_minimum_jd(almanac_t *almanac,
                                           almanac_event_metric_fn metric,
                                           void *context,
                                           double centre_jd,
                                           double half_span_days,
                                           double sample_step_days)
{
    double best_jd = centre_jd;
    double best_value;
    double probe;
    double step;
    int iteration;

    if (!metric || !(half_span_days > 0.0) || !(sample_step_days > 0.0))
        return centre_jd;
    best_value = metric(almanac, centre_jd, context);
    if (!(best_value == best_value))
        best_value = DBL_MAX;

    for (probe = centre_jd - half_span_days; probe <= centre_jd + half_span_days + 1e-9; probe += sample_step_days) {
        double value = metric(almanac, probe, context);

        if (value == value && value < best_value) {
            best_value = value;
            best_jd = probe;
        }
    }
    if (best_value == DBL_MAX)
        return centre_jd;

    /*
     * ESAA 8.42 obtains greatest eclipse by rapidly interpolating the
     * minimum of the squared separation.  Three-point parabolic
     * interpolation is the scalar equivalent here.  The phase/conjunction
     * estimate has already put us close to the event, so this converges in
     * a handful of ephemeris evaluations instead of a long golden-section
     * search.
     */
    step = fmin(sample_step_days,
                fmin(best_jd - (centre_jd - half_span_days),
                     centre_jd + half_span_days - best_jd));
    if (!(step > 0.0))
        return best_jd;

    for (iteration = 0; iteration < 9 && step > 1e-10; ++iteration) {
        double left_value = metric(almanac, best_jd - step, context);
        double centre_value = metric(almanac, best_jd, context);
        double right_value = metric(almanac, best_jd + step, context);
        double denominator;
        double offset;
        double candidate_jd;
        double candidate_value;

        if (!(left_value == left_value) ||
            !(centre_value == centre_value) ||
            !(right_value == right_value)) {
            step *= 0.25;
            continue;
        }
        left_value *= left_value;
        centre_value *= centre_value;
        right_value *= right_value;
        denominator = left_value - 2.0 * centre_value + right_value;
        if (!(denominator > DBL_EPSILON)) {
            step *= 0.25;
            continue;
        }
        offset = 0.5 * step * (left_value - right_value) / denominator;
        if (offset > step)
            offset = step;
        else if (offset < -step)
            offset = -step;
        candidate_jd = best_jd + offset;
        candidate_value = metric(almanac, candidate_jd, context);
        if (candidate_value == candidate_value) {
            best_jd = candidate_jd;
            best_value = candidate_value;
        }
        step *= 0.25;
    }

    return best_jd;
}

static bool almanac_solar_eclipse_geometry(almanac_t *almanac,
                                           double jd,
                                           const almanac_observer_t *observer,
                                           almanac_solar_eclipse_geometry_t *out)
{
    almanac_state_t sun_state;
    almanac_state_t moon_state;
    cartesian3_t sun_topocentric;
    cartesian3_t moon_topocentric;
    double sun_distance;
    double moon_distance;

    if (!almanac || !observer || !out)
        return false;
    if (!almanac_observer_is_valid(almanac, observer))
        return false;
    if (!almanac_state_for_body_at_jd(almanac, ALMANAC_BODY_ID_SUN, jd, &sun_state) ||
        !almanac_state_for_body_at_jd(almanac, ALMANAC_BODY_ID_MOON, jd, &moon_state)) {
        return false;
    }
    if (!almanac_topocentric_vector(almanac, &sun_state.geocentric_equatorial_au, observer, jd, &sun_topocentric) ||
        !almanac_topocentric_vector(almanac, &moon_state.geocentric_equatorial_au, observer, jd, &moon_topocentric)) {
        return false;
    }
    sun_distance = cartesian_length(&sun_topocentric);
    moon_distance = cartesian_length(&moon_topocentric);

    memset(out, 0, sizeof(*out));
    out->separation = angular_separation_degrees(&sun_topocentric, &moon_topocentric);
    out->sun_sd = almanac_body_semi_diameter_from_distance_degrees(ALMANAC_BODY_ID_SUN, sun_distance);
    out->moon_sd = almanac_body_semi_diameter_from_distance_degrees(ALMANAC_BODY_ID_MOON, moon_distance);
    out->sun_altitude_degrees = almanac_topocentric_altitude_degrees(almanac, &sun_topocentric, observer, jd);
    if (!(out->separation == out->separation) || !(out->sun_sd > 0.0) ||
        !(out->moon_sd > 0.0) || !(out->sun_altitude_degrees == out->sun_altitude_degrees)) {
        return false;
    }
    out->apparent_separation = out->separation;
    return true;
}

static bool almanac_solar_eclipse_circumstance_from_geometry(const almanac_solar_eclipse_geometry_t *geometry,
                                                             almanac_solar_eclipse_circumstance_t *out)
{
    double magnitude;
    double totality_percent;
    bool central;

    if (!geometry || !out)
        return false;
    if (geometry->sun_altitude_degrees + geometry->sun_sd <= 0.0 ||
        geometry->separation >= geometry->sun_sd + geometry->moon_sd) {
        return false;
    }

    magnitude = (geometry->sun_sd + geometry->moon_sd - geometry->separation) / (2.0 * geometry->sun_sd);
    totality_percent = almanac_disc_coverage_percent(geometry->sun_sd,
                                                     geometry->moon_sd,
                                                     geometry->apparent_separation);
    if (!(magnitude == magnitude) || !(totality_percent == totality_percent))
        return false;

    memset(out, 0, sizeof(*out));
    central = geometry->separation <= fabs(geometry->sun_sd - geometry->moon_sd);
    out->magnitude = magnitude;
    out->totality_percent = fmax(0.0, fmin(100.0, totality_percent));
    out->central = central;
    if (central && geometry->moon_sd >= geometry->sun_sd)
        out->kind = ALMANAC_SOLAR_ECLIPSE_TOTAL;
    else if (central)
        out->kind = ALMANAC_SOLAR_ECLIPSE_ANNULAR;
    else
        out->kind = ALMANAC_SOLAR_ECLIPSE_PARTIAL;
    return true;
}

static double almanac_solar_totality_score_degrees(const almanac_solar_eclipse_geometry_t *geometry)
{
    if (!geometry)
        return NAN;
    if (geometry->sun_altitude_degrees + geometry->sun_sd <= 0.0 ||
        geometry->moon_sd < geometry->sun_sd) {
        return NAN;
    }
    return geometry->separation - (geometry->moon_sd - geometry->sun_sd);
}

static double almanac_solar_eclipse_contact_residual(almanac_t *almanac,
                                                     double jd,
                                                     void *context)
{
    almanac_eclipse_contact_context_t *contact = context;
    almanac_solar_eclipse_geometry_t geometry;

    if (!contact || !almanac_solar_eclipse_geometry(almanac, jd, contact->observer, &geometry))
        return NAN;
    if (contact->contact_level == ALMANAC_CONTACT_LEVEL_INNER)
        return geometry.apparent_separation - fabs(geometry.sun_sd - geometry.moon_sd);
    return geometry.separation - (geometry.sun_sd + geometry.moon_sd);
}

static double almanac_solar_eclipse_metric(almanac_t *almanac,
                                           double jd,
                                           void *context)
{
    almanac_eclipse_contact_context_t *contact = context;
    almanac_solar_eclipse_geometry_t geometry;

    if (!contact || !almanac_solar_eclipse_geometry(almanac, jd, contact->observer, &geometry))
        return NAN;
    if (geometry.sun_altitude_degrees + geometry.sun_sd <= 0.0)
        return NAN;
    return geometry.separation;
}

static bool almanac_lunar_eclipse_geometry(almanac_t *almanac,
                                           double jd,
                                           const almanac_observer_t *observer,
                                           almanac_lunar_eclipse_geometry_t *out)
{
    static const double earth_radius_au = 6378.137 / 149597870.7;
    static const double reduced_parallax_factor = 0.9983407;
    static const double shadow_enlargement = 1.02;
    almanac_state_t sun_state;
    almanac_state_t moon_state;
    cartesian3_t moon_topocentric;
    cartesian3_t antisolar_geocentric;
    double sun_distance;
    double moon_parallax;
    double sun_parallax;

    if (!almanac || !observer || !out)
        return false;
    if (!almanac_observer_is_valid(almanac, observer))
        return false;
    if (!almanac_state_for_body_at_jd(almanac, ALMANAC_BODY_ID_SUN, jd, &sun_state) ||
        !almanac_state_for_body_at_jd(almanac, ALMANAC_BODY_ID_MOON, jd, &moon_state)) {
        return false;
    }
    if (!almanac_topocentric_vector(almanac,
                                    &moon_state.geocentric_equatorial_au,
                                    observer,
                                    jd,
                                    &moon_topocentric)) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    antisolar_geocentric = cartesian_negate(&sun_state.geocentric_equatorial_au);
    out->opposition_error =
        angular_separation_degrees(&antisolar_geocentric, &moon_state.geocentric_equatorial_au);
    sun_distance = cartesian_length(&sun_state.geocentric_equatorial_au);
    out->delta_moon = cartesian_length(&moon_state.geocentric_equatorial_au);
    out->sun_sd = almanac_body_semi_diameter_from_distance_degrees(ALMANAC_BODY_ID_SUN,
                                                                   sun_distance);
    out->moon_sd = almanac_body_semi_diameter_from_distance_degrees(ALMANAC_BODY_ID_MOON,
                                                                    out->delta_moon);
    out->moon_altitude_degrees = almanac_topocentric_altitude_degrees(almanac, &moon_topocentric, observer, jd);
    if (!(out->opposition_error == out->opposition_error) || !(out->sun_sd > 0.0) ||
        !(out->moon_altitude_degrees == out->moon_altitude_degrees) ||
        !(out->moon_sd > 0.0) || !(out->delta_moon > 0.0)) {
        return false;
    }

    moon_parallax = radians_to_degrees(
        asin(clamp_unit(reduced_parallax_factor * earth_radius_au / out->delta_moon)));
    sun_parallax = radians_to_degrees(asin(clamp_unit(earth_radius_au / sun_distance)));
    out->umbra_radius =
        shadow_enlargement * (moon_parallax + sun_parallax - out->sun_sd);
    out->penumbra_radius =
        shadow_enlargement * (moon_parallax + sun_parallax + out->sun_sd);
    if (!(out->umbra_radius > 0.0) || !(out->penumbra_radius > 0.0))
        return false;
    return true;
}

static double almanac_lunar_eclipse_contact_residual(almanac_t *almanac,
                                                     double jd,
                                                     void *context)
{
    almanac_eclipse_contact_context_t *contact = context;
    almanac_lunar_eclipse_geometry_t geometry;

    if (!contact || !almanac_lunar_eclipse_geometry(almanac, jd, contact->observer, &geometry))
        return NAN;
    if (contact->contact_level == ALMANAC_CONTACT_LEVEL_OUTER)
        return geometry.opposition_error - (geometry.penumbra_radius + geometry.moon_sd);
    if (contact->contact_level == ALMANAC_CONTACT_LEVEL_INNER)
        return geometry.opposition_error - (geometry.umbra_radius + geometry.moon_sd);
    if (!(geometry.umbra_radius > geometry.moon_sd))
        return NAN;
    return geometry.opposition_error - (geometry.umbra_radius - geometry.moon_sd);
}

static double almanac_lunar_eclipse_metric(almanac_t *almanac,
                                           double jd,
                                           void *context)
{
    almanac_eclipse_contact_context_t *contact = context;
    almanac_lunar_eclipse_geometry_t geometry;

    if (!contact || !almanac_lunar_eclipse_geometry(almanac, jd, contact->observer, &geometry))
        return NAN;
    return geometry.opposition_error;
}

static bool almanac_solar_transit_geometry(almanac_t *almanac,
                                           almanac_body_id_t body_id,
                                           double jd,
                                           const almanac_observer_t *observer,
                                           almanac_solar_transit_geometry_t *out)
{
    almanac_state_t sun_state;
    almanac_state_t body_state;
    cartesian3_t sun_topocentric;
    cartesian3_t body_topocentric;
    double sun_distance;
    double body_distance;

    if (!almanac || body_id <= ALMANAC_BODY_ID_UNKNOWN ||
        body_id >= ALMANAC_BODY_ID_COUNT || !observer || !out) {
        return false;
    }
    if (!almanac_observer_is_valid(almanac, observer))
        return false;
    if (!almanac_state_for_body_at_jd(almanac, ALMANAC_BODY_ID_SUN, jd, &sun_state) ||
        !almanac_state_for_body_at_jd(almanac, body_id, jd, &body_state)) {
        return false;
    }
    if (!almanac_topocentric_vector(almanac, &sun_state.geocentric_equatorial_au, observer, jd, &sun_topocentric) ||
        !almanac_topocentric_vector(almanac, &body_state.geocentric_equatorial_au, observer, jd, &body_topocentric)) {
        return false;
    }
    sun_distance = cartesian_length(&sun_topocentric);
    body_distance = cartesian_length(&body_topocentric);

    memset(out, 0, sizeof(*out));
    out->body_id = body_id;
    out->sun_sd = almanac_body_semi_diameter_from_distance_degrees(ALMANAC_BODY_ID_SUN, sun_distance);
    out->body_sd = almanac_body_semi_diameter_from_distance_degrees(body_id, body_distance);
    out->separation = angular_separation_degrees(&sun_topocentric, &body_topocentric);
    out->sun_altitude_degrees = almanac_topocentric_altitude_degrees(almanac, &sun_topocentric, observer, jd);
    return out->sun_sd > 0.0 && out->body_sd > 0.0 &&
           out->separation == out->separation &&
           out->sun_altitude_degrees == out->sun_altitude_degrees;
}

static double almanac_solar_transit_contact_residual(almanac_t *almanac,
                                                     double jd,
                                                     void *context)
{
    almanac_eclipse_contact_context_t *contact = context;
    almanac_solar_transit_geometry_t geometry;

    if (!contact ||
        !almanac_solar_transit_geometry(almanac, contact->body_id, jd, contact->observer, &geometry))
        return NAN;
    if (contact->contact_level == ALMANAC_CONTACT_LEVEL_INNER)
        return geometry.separation - (geometry.sun_sd - geometry.body_sd);
    return geometry.separation - (geometry.sun_sd + geometry.body_sd);
}

static double almanac_solar_transit_metric(almanac_t *almanac,
                                           double jd,
                                           void *context)
{
    almanac_eclipse_contact_context_t *contact = context;
    almanac_solar_transit_geometry_t geometry;

    if (!contact ||
        !almanac_solar_transit_geometry(almanac, contact->body_id, jd, contact->observer, &geometry))
        return NAN;
    if (geometry.sun_altitude_degrees + geometry.sun_sd <= 0.0)
        return NAN;
    return geometry.separation;
}

struct _almanac_solar_eclipse_t {
    almanac_solar_eclipse_kind_t kind;
    almanac_event_time_t first_contact;
    almanac_event_time_t second_contact;
    almanac_event_time_t greatest_eclipse;
    almanac_event_time_t third_contact;
    almanac_event_time_t fourth_contact;
    double separation_degrees;
    double magnitude;
    double totality_percent;
    double sun_semi_diameter_degrees;
    double moon_semi_diameter_degrees;
    bool central;
};

struct _almanac_lunar_eclipse_t {
    almanac_lunar_eclipse_kind_t kind;
    almanac_event_time_t p1_contact;
    almanac_event_time_t u1_contact;
    almanac_event_time_t u2_contact;
    almanac_event_time_t greatest_eclipse;
    almanac_event_time_t u3_contact;
    almanac_event_time_t u4_contact;
    almanac_event_time_t p4_contact;
    double opposition_error_degrees;
    double umbral_magnitude;
    double penumbral_magnitude;
    double totality_percent;
    double umbral_radius_degrees;
    double penumbral_radius_degrees;
    double moon_semi_diameter_degrees;
};

struct _almanac_solar_transit_t {
    almanac_body_id_t body_id;
    almanac_event_time_t first_contact;
    almanac_event_time_t second_contact;
    almanac_event_time_t greatest_transit;
    almanac_event_time_t third_contact;
    almanac_event_time_t fourth_contact;
    double separation_degrees;
    double solar_semi_diameter_degrees;
    double planet_semi_diameter_degrees;
    double chord_distance_fraction;
    bool interior;
};

static bool almanac_fill_solar_eclipse(almanac_t *almanac,
                                       double jd,
                                       const almanac_observer_t *observer,
                                       almanac_solar_eclipse_t *out)
{
    almanac_solar_eclipse_geometry_t geometry;
    almanac_solar_eclipse_circumstance_t circumstance;
    almanac_eclipse_contact_context_t contact = { ALMANAC_BODY_ID_MOON, observer, 0 };
    static const double contact_step_days = 1.0 / 24.0;

    if (!almanac || !out)
        return false;
    if (!almanac_solar_eclipse_geometry(almanac, jd, observer, &geometry))
        return false;
    if (!almanac_solar_eclipse_circumstance_from_geometry(&geometry, &circumstance))
        return false;

    memset(out, 0, sizeof(*out));
    almanac_event_time_from_jd(almanac_find_contact_jd(almanac,
                                                       almanac_solar_eclipse_contact_residual,
                                                       &contact,
                                                       jd,
                                                       -1.0,
                                                       1.0,
                                                       contact_step_days),
                               &out->first_contact);
    almanac_event_time_from_jd(NAN, &out->second_contact);
    almanac_event_time_from_jd(jd, &out->greatest_eclipse);
    almanac_event_time_from_jd(NAN, &out->third_contact);
    almanac_event_time_from_jd(almanac_find_contact_jd(almanac,
                                                       almanac_solar_eclipse_contact_residual,
                                                       &contact,
                                                       jd,
                                                       1.0,
                                                       1.0,
                                                       contact_step_days),
                               &out->fourth_contact);
    out->separation_degrees = geometry.separation;
    out->magnitude = circumstance.magnitude;
    out->totality_percent = circumstance.totality_percent;
    out->sun_semi_diameter_degrees = geometry.sun_sd;
    out->moon_semi_diameter_degrees = geometry.moon_sd;
    out->central = circumstance.central;
    out->kind = circumstance.kind;
    if (out->kind == ALMANAC_SOLAR_ECLIPSE_TOTAL ||
        out->kind == ALMANAC_SOLAR_ECLIPSE_ANNULAR) {
        contact.contact_level = ALMANAC_CONTACT_LEVEL_INNER;
        almanac_event_time_from_jd(almanac_find_contact_jd(almanac,
                                                           almanac_solar_eclipse_contact_residual,
                                                           &contact,
                                                           jd,
                                                           -1.0,
                                                           1.0,
                                                           contact_step_days),
                                   &out->second_contact);
        almanac_event_time_from_jd(almanac_find_contact_jd(almanac,
                                                           almanac_solar_eclipse_contact_residual,
                                                           &contact,
                                                           jd,
                                                           1.0,
                                                           1.0,
                                                           contact_step_days),
                                   &out->third_contact);
    }
    return true;
}

static double almanac_surface_distance_km(double lat1_degrees,
                                          double lon1_degrees,
                                          double lat2_degrees,
                                          double lon2_degrees)
{
    static const double earth_radius_km = 6371.0088;
    double lat1 = degrees_to_radians(lat1_degrees);
    double lat2 = degrees_to_radians(lat2_degrees);
    double dlat = degrees_to_radians(lat2_degrees - lat1_degrees);
    double dlon = degrees_to_radians(lon2_degrees - lon1_degrees);
    double a = sin(dlat * 0.5) * sin(dlat * 0.5) +
               cos(lat1) * cos(lat2) * sin(dlon * 0.5) * sin(dlon * 0.5);

    return earth_radius_km * 2.0 * atan2(sqrt(a), sqrt(fmax(0.0, 1.0 - a)));
}

static void almanac_destination_point(double lat_degrees,
                                      double lon_degrees,
                                      double distance_km,
                                      double bearing_degrees,
                                      double *out_lat_degrees,
                                      double *out_lon_degrees)
{
    static const double earth_radius_km = 6371.0088;
    double angular_distance = distance_km / earth_radius_km;
    double bearing = degrees_to_radians(bearing_degrees);
    double lat1 = degrees_to_radians(lat_degrees);
    double lon1 = degrees_to_radians(lon_degrees);
    double sin_lat1 = sin(lat1);
    double cos_lat1 = cos(lat1);
    double sin_distance = sin(angular_distance);
    double cos_distance = cos(angular_distance);
    double lat2 = asin(clamp_unit(sin_lat1 * cos_distance +
                                  cos_lat1 * sin_distance * cos(bearing)));
    double lon2 = lon1 + atan2(sin(bearing) * sin_distance * cos_lat1,
                               cos_distance - sin_lat1 * sin(lat2));

    if (out_lat_degrees)
        *out_lat_degrees = radians_to_degrees(lat2);
    if (out_lon_degrees)
        *out_lon_degrees = normalize_degrees(radians_to_degrees(lon2) + 180.0) - 180.0;
}

static bool almanac_observer_at_location(almanac_t *almanac,
                                         const almanac_observer_t *origin,
                                         double latitude_degrees,
                                         double longitude_degrees,
                                         almanac_observer_t *out)
{
    if (!almanac || !origin || !out)
        return false;
    out->latitude_degrees = latitude_degrees;
    out->longitude_degrees = longitude_degrees;
    out->elevation_metres = origin->elevation_metres;
    return almanac_observer_is_valid(almanac, out);
}

static void almanac_fill_solar_totality_location(const almanac_observer_t *origin,
                                                 double latitude_degrees,
                                                 double longitude_degrees,
                                                 double jd,
                                                 double magnitude,
                                                 double totality_percent,
                                                 almanac_solar_totality_location_t *out)
{
    memset(out, 0, sizeof(*out));
    out->found = true;
    almanac_event_time_from_jd(jd, &out->greatest_eclipse);
    out->latitude_degrees = latitude_degrees;
    out->longitude_degrees = longitude_degrees;
    out->distance_km = almanac_surface_distance_km(origin->latitude_degrees,
                                                  origin->longitude_degrees,
                                                  latitude_degrees,
                                                  longitude_degrees);
    out->magnitude = magnitude;
    out->totality_percent = totality_percent;
}

static bool almanac_probe_solar_totality(almanac_t *almanac,
                                         const almanac_observer_t *origin,
                                         double seed_jd,
                                         double latitude_degrees,
                                         double longitude_degrees,
                                         almanac_solar_totality_location_t *out)
{
    almanac_observer_t observer;
    almanac_solar_eclipse_geometry_t geometry;
    almanac_solar_eclipse_circumstance_t circumstance;

    if (!almanac || !origin || !out)
        return false;
    if (!almanac_observer_at_location(almanac, origin, latitude_degrees, longitude_degrees, &observer))
        return false;

    if (!almanac_solar_eclipse_geometry(almanac, seed_jd, &observer, &geometry))
        return false;
    if (!almanac_solar_eclipse_circumstance_from_geometry(&geometry, &circumstance) ||
        circumstance.kind != ALMANAC_SOLAR_ECLIPSE_TOTAL) {
        return false;
    }

    almanac_fill_solar_totality_location(origin,
                                         latitude_degrees,
                                         longitude_degrees,
                                         seed_jd,
                                         circumstance.magnitude,
                                         circumstance.totality_percent,
                                         out);
    return true;
}

static bool almanac_probe_solar_totality_candidate(almanac_t *almanac,
                                                   const almanac_observer_t *origin,
                                                   double seed_jd,
                                                   double latitude_degrees,
                                                   double longitude_degrees,
                                                   double tolerance_degrees,
                                                   almanac_solar_totality_location_t *out)
{
    almanac_observer_t observer;
    almanac_solar_eclipse_geometry_t geometry;
    double score_degrees;

    if (!almanac || !origin || !out)
        return false;
    if (!almanac_observer_at_location(almanac, origin, latitude_degrees, longitude_degrees, &observer))
        return false;
    if (!almanac_solar_eclipse_geometry(almanac, seed_jd, &observer, &geometry))
        return false;
    score_degrees = almanac_solar_totality_score_degrees(&geometry);
    if (!(score_degrees == score_degrees) || score_degrees > tolerance_degrees) {
        return false;
    }

    almanac_fill_solar_totality_location(origin,
                                         latitude_degrees,
                                         longitude_degrees,
                                         seed_jd,
                                         NAN,
                                         NAN,
                                         out);
    return true;
}

static bool almanac_refine_solar_totality(almanac_t *almanac,
                                          const almanac_observer_t *origin,
                                          double seed_jd,
                                          const almanac_solar_totality_location_t *coarse,
                                          almanac_solar_totality_location_t *out)
{
    almanac_observer_t observer;
    almanac_eclipse_contact_context_t contact;
    almanac_solar_eclipse_geometry_t geometry;
    almanac_solar_eclipse_circumstance_t circumstance;
    double local_jd;

    if (!almanac || !origin || !coarse || !coarse->found || !out)
        return false;
    if (!almanac_observer_at_location(almanac,
                                      origin,
                                      coarse->latitude_degrees,
                                      coarse->longitude_degrees,
                                      &observer)) {
        return false;
    }

    contact.body_id = ALMANAC_BODY_ID_MOON;
    contact.observer = &observer;
    contact.contact_level = ALMANAC_CONTACT_LEVEL_OUTER;
    local_jd = almanac_find_local_minimum_jd(almanac,
                                             almanac_solar_eclipse_metric,
                                             &contact,
                                             seed_jd,
                                             0.125,
                                             1.0 / 96.0);
    if (!almanac_solar_eclipse_geometry(almanac, local_jd, &observer, &geometry))
        return false;
    if (!almanac_solar_eclipse_circumstance_from_geometry(&geometry, &circumstance) ||
        circumstance.kind != ALMANAC_SOLAR_ECLIPSE_TOTAL) {
        return false;
    }

    *out = *coarse;
    almanac_event_time_from_jd(local_jd, &out->greatest_eclipse);
    out->magnitude = circumstance.magnitude;
    out->totality_percent = circumstance.totality_percent;
    return true;
}

static void almanac_remember_nearest_totality(const almanac_solar_totality_location_t *candidate,
                                              almanac_solar_totality_location_t *best)
{
    if (!candidate || !candidate->found || !best)
        return;
    if (!best->found || candidate->distance_km < best->distance_km)
        *best = *candidate;
}

typedef struct almanac_land_box_t {
    double min_lat;
    double max_lat;
    double min_lon;
    double max_lon;
} almanac_land_box_t;

static const almanac_land_box_t ALMANAC_PROBABLE_LAND_BOXES[] = {
    {35.5, 44.5, -10.0, 4.5},     /* Iberia */
    {41.0, 51.5, -5.5, 10.5},     /* France */
    {49.0, 59.0, -11.0, 2.5},     /* Britain and Ireland */
    {63.0, 67.5, -25.0, -13.0},   /* Iceland */
    {59.0, 84.0, -75.0, -10.0},   /* Greenland */
    {-35.5, 37.5, -18.0, 52.0},   /* Africa */
    {5.0, 80.0, 25.0, 180.0},     /* Asia */
    {-47.0, 10.0, 95.0, 155.0},   /* Australia and nearby islands */
    {-56.0, 15.0, -82.0, -34.0},  /* South America */
    {15.0, 84.0, -170.0, -52.0},  /* North America */
    {-48.5, -33.0, 165.0, 180.0}, /* New Zealand */
    {-48.5, -33.0, -180.0, -175.0}
};

typedef struct almanac_land_box_distance_t {
    size_t index;
    double distance_km;
} almanac_land_box_distance_t;

static int almanac_land_box_distance_compare(const void *lhs, const void *rhs)
{
    const almanac_land_box_distance_t *left = lhs;
    const almanac_land_box_distance_t *right = rhs;

    if (left->distance_km < right->distance_km)
        return -1;
    if (left->distance_km > right->distance_km)
        return 1;
    return 0;
}

static double almanac_land_box_min_distance_km(const almanac_land_box_t *box,
                                               const almanac_observer_t *observer)
{
    double lat;
    double lon;

    if (!box || !observer)
        return DBL_MAX;
    lat = fmax(box->min_lat, fmin(box->max_lat, observer->latitude_degrees));
    lon = fmax(box->min_lon, fmin(box->max_lon, observer->longitude_degrees));
    return almanac_surface_distance_km(observer->latitude_degrees,
                                       observer->longitude_degrees,
                                       lat,
                                       lon);
}

static double almanac_land_box_min_distance_to_point_km(const almanac_land_box_t *box,
                                                        double latitude_degrees,
                                                        double longitude_degrees)
{
    double lat;
    double lon;

    if (!box)
        return DBL_MAX;
    lat = fmax(box->min_lat, fmin(box->max_lat, latitude_degrees));
    lon = fmax(box->min_lon, fmin(box->max_lon, longitude_degrees));
    return almanac_surface_distance_km(latitude_degrees,
                                       longitude_degrees,
                                       lat,
                                       lon);
}

static bool almanac_probable_land_point(double latitude_degrees, double longitude_degrees)
{
    size_t i;

    for (i = 0u; i < sizeof(ALMANAC_PROBABLE_LAND_BOXES) / sizeof(ALMANAC_PROBABLE_LAND_BOXES[0]); ++i) {
        if (latitude_degrees >= ALMANAC_PROBABLE_LAND_BOXES[i].min_lat &&
            latitude_degrees <= ALMANAC_PROBABLE_LAND_BOXES[i].max_lat &&
            longitude_degrees >= ALMANAC_PROBABLE_LAND_BOXES[i].min_lon &&
            longitude_degrees <= ALMANAC_PROBABLE_LAND_BOXES[i].max_lon) {
            return true;
        }
    }
    return false;
}

bool almanac_nearest_solar_totality(almanac_t *almanac,
                                    const almanac_observer_t *observer,
                                    const almanac_solar_eclipse_t *eclipse,
                                    almanac_solar_totality_location_t *out)
{
    almanac_solar_totality_location_t best;
    almanac_solar_totality_location_t candidate;
    double radius_km;
    bool coarse_found = false;

    if (!almanac || !observer || !eclipse || !out) {
        almanac_set_error(almanac, "invalid nearest solar totality request");
        return false;
    }
    memset(out, 0, sizeof(*out));
    memset(&best, 0, sizeof(best));
    if (!almanac_observer_is_valid(almanac, observer) ||
        !(eclipse->greatest_eclipse.jd == eclipse->greatest_eclipse.jd)) {
        return false;
    }

    if (almanac_probe_solar_totality(almanac,
                                     observer,
                                     eclipse->greatest_eclipse.jd,
                                     observer->latitude_degrees,
                                     observer->longitude_degrees,
                                     &candidate)) {
        *out = candidate;
        return true;
    }

    for (radius_km = 75.0; radius_km <= 12000.0 && !coarse_found; radius_km += 75.0) {
        double bearing_step = radius_km < 750.0 ? 15.0 : 7.5;
        double bearing;

        for (bearing = 0.0; bearing < 360.0; bearing += bearing_step) {
            double lat;
            double lon;

            almanac_destination_point(observer->latitude_degrees,
                                      observer->longitude_degrees,
                                      radius_km,
                                      bearing,
                                      &lat,
                                      &lon);
            if (almanac_probe_solar_totality_candidate(almanac,
                                                       observer,
                                                       eclipse->greatest_eclipse.jd,
                                                       lat,
                                                       lon,
                                                       1.0,
                                                       &candidate) &&
                almanac_refine_solar_totality(almanac,
                                              observer,
                                              eclipse->greatest_eclipse.jd,
                                              &candidate,
                                              &candidate)) {
                almanac_remember_nearest_totality(&candidate, &best);
                coarse_found = true;
            }
        }
    }

    if (best.found) {
        double north_km;
        almanac_solar_totality_location_t refined = best;

        for (north_km = -300.0; north_km <= 300.0; north_km += 25.0) {
            double east_km;

            for (east_km = -300.0; east_km <= 300.0; east_km += 25.0) {
                double lat;
                double lon;

                almanac_destination_point(best.latitude_degrees,
                                          best.longitude_degrees,
                                          hypot(north_km, east_km),
                                          radians_to_degrees(atan2(east_km, north_km)),
                                          &lat,
                                          &lon);
                if (almanac_probe_solar_totality(almanac,
                                                 observer,
                                                 eclipse->greatest_eclipse.jd,
                                                 lat,
                                                 lon,
                                                 &candidate)) {
                    almanac_remember_nearest_totality(&candidate, &refined);
                }
            }
        }
        best = refined;
        if (almanac_refine_solar_totality(almanac,
                                          observer,
                                          eclipse->greatest_eclipse.jd,
                                          &best,
                                          &candidate)) {
            best = candidate;
        }
    }

    *out = best;
    return true;
}

bool almanac_solar_eclipse_totality_at(almanac_t *almanac,
                                       const almanac_observer_t *observer,
                                       const almanac_solar_eclipse_t *eclipse,
                                       almanac_solar_totality_location_t *out)
{
    almanac_solar_totality_location_t candidate;
    almanac_solar_totality_location_t refined;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!almanac || !observer || !eclipse) {
        almanac_set_error(almanac, "invalid solar totality test request");
        return false;
    }
    if (!almanac_observer_is_valid(almanac, observer) ||
        !(eclipse->greatest_eclipse.jd == eclipse->greatest_eclipse.jd)) {
        return false;
    }
    if (!almanac_probe_solar_totality_candidate(almanac,
                                                observer,
                                                eclipse->greatest_eclipse.jd,
                                                observer->latitude_degrees,
                                                observer->longitude_degrees,
                                                1.0,
                                                &candidate)) {
        return false;
    }
    if (!almanac_refine_solar_totality(almanac,
                                       observer,
                                       eclipse->greatest_eclipse.jd,
                                       &candidate,
                                       &refined)) {
        return false;
    }
    if (out)
        *out = refined;
    return true;
}

bool almanac_solar_eclipse_totality_from_seed(almanac_t *almanac,
                                              const almanac_observer_t *origin,
                                              const almanac_observer_t *seed,
                                              const almanac_solar_eclipse_t *eclipse,
                                              double tolerance_degrees,
                                              almanac_solar_totality_location_t *out)
{
    almanac_solar_totality_location_t candidate;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!almanac || !origin || !seed || !eclipse || !out) {
        almanac_set_error(almanac, "invalid nearby solar totality request");
        return false;
    }
    if (!almanac_observer_is_valid(almanac, origin) ||
        !almanac_observer_is_valid(almanac, seed) ||
        !(eclipse->greatest_eclipse.jd == eclipse->greatest_eclipse.jd) ||
        !(tolerance_degrees >= 0.0)) {
        return false;
    }
    if (!almanac_probe_solar_totality_candidate(almanac,
                                                origin,
                                                eclipse->greatest_eclipse.jd,
                                                seed->latitude_degrees,
                                                seed->longitude_degrees,
                                                tolerance_degrees,
                                                &candidate)) {
        return false;
    }
    return almanac_refine_solar_totality(almanac,
                                         origin,
                                         eclipse->greatest_eclipse.jd,
                                         &candidate,
                                         out);
}

bool almanac_solar_eclipse_totality_seed_score(almanac_t *almanac,
                                               const almanac_observer_t *seed,
                                               const almanac_solar_eclipse_t *eclipse,
                                               double *out_score_degrees)
{
    almanac_solar_eclipse_geometry_t geometry;

    if (out_score_degrees)
        *out_score_degrees = NAN;
    if (!almanac || !seed || !eclipse || !out_score_degrees) {
        almanac_set_error(almanac, "invalid solar totality seed score request");
        return false;
    }
    if (!almanac_observer_is_valid(almanac, seed) ||
        !(eclipse->greatest_eclipse.jd == eclipse->greatest_eclipse.jd)) {
        return false;
    }
    if (!almanac_solar_eclipse_geometry(almanac, eclipse->greatest_eclipse.jd, seed, &geometry))
        return false;
    *out_score_degrees = almanac_solar_totality_score_degrees(&geometry);
    return *out_score_degrees == *out_score_degrees;
}

bool almanac_nearest_solar_totality_land(almanac_t *almanac,
                                         const almanac_observer_t *observer,
                                         const almanac_solar_eclipse_t *eclipse,
                                         almanac_solar_totality_location_t *out)
{
    almanac_solar_totality_location_t best;
    almanac_solar_totality_location_t candidate;
    almanac_solar_totality_location_t path_seed;
    almanac_land_box_distance_t box_distances[sizeof(ALMANAC_PROBABLE_LAND_BOXES) /
                                             sizeof(ALMANAC_PROBABLE_LAND_BOXES[0])];
    size_t box_count = sizeof(ALMANAC_PROBABLE_LAND_BOXES) / sizeof(ALMANAC_PROBABLE_LAND_BOXES[0]);
    size_t box_index;
    bool have_path_seed = false;

    if (!almanac || !observer || !eclipse || !out) {
        almanac_set_error(almanac, "invalid nearest land solar totality request");
        return false;
    }
    memset(out, 0, sizeof(*out));
    memset(&best, 0, sizeof(best));
    memset(&path_seed, 0, sizeof(path_seed));
    if (!almanac_observer_is_valid(almanac, observer) ||
        !(eclipse->greatest_eclipse.jd == eclipse->greatest_eclipse.jd)) {
        return false;
    }

    if (almanac_probable_land_point(observer->latitude_degrees, observer->longitude_degrees) &&
        almanac_probe_solar_totality(almanac,
                                     observer,
                                     eclipse->greatest_eclipse.jd,
                                     observer->latitude_degrees,
                                     observer->longitude_degrees,
                                     &candidate)) {
        *out = candidate;
        return true;
    }

    have_path_seed = almanac_nearest_solar_totality(almanac, observer, eclipse, &path_seed) &&
                     path_seed.found;
    if (have_path_seed &&
        almanac_probable_land_point(path_seed.latitude_degrees, path_seed.longitude_degrees)) {
        *out = path_seed;
        return true;
    }

    if (!best.found) {
        for (box_index = 0u; box_index < box_count; ++box_index) {
            box_distances[box_index].index = box_index;
            box_distances[box_index].distance_km = have_path_seed
                ? almanac_land_box_min_distance_to_point_km(&ALMANAC_PROBABLE_LAND_BOXES[box_index],
                                                            path_seed.latitude_degrees,
                                                            path_seed.longitude_degrees)
                : almanac_land_box_min_distance_km(&ALMANAC_PROBABLE_LAND_BOXES[box_index], observer);
        }
        qsort(box_distances, box_count, sizeof(box_distances[0]), almanac_land_box_distance_compare);

        for (box_index = 0u; box_index < box_count; ++box_index) {
            const almanac_land_box_t *box = &ALMANAC_PROBABLE_LAND_BOXES[box_distances[box_index].index];
            double lat_step = (box->max_lat - box->min_lat) > 20.0 ? 1.0 : 0.5;
            double lon_step = (box->max_lon - box->min_lon) > 20.0 ? 1.0 : 0.5;
            double lat;

            if (best.found && have_path_seed && box_distances[box_index].distance_km > 1500.0)
                break;
            if (best.found && !have_path_seed && box_distances[box_index].distance_km > best.distance_km + 25.0)
                break;

            for (lat = box->min_lat; lat <= box->max_lat + 1e-9; lat += lat_step) {
                double lon;

                for (lon = box->min_lon; lon <= box->max_lon + 1e-9; lon += lon_step) {
                    if (best.found &&
                        almanac_surface_distance_km(observer->latitude_degrees,
                                                    observer->longitude_degrees,
                                                    lat,
                                                    lon) > best.distance_km + 25.0) {
                        continue;
                    }
                    if (have_path_seed &&
                        almanac_surface_distance_km(path_seed.latitude_degrees,
                                                    path_seed.longitude_degrees,
                                                    lat,
                                                    lon) > 1500.0) {
                        continue;
                    }
                    if (almanac_probe_solar_totality_candidate(almanac,
                                                               observer,
                                                               eclipse->greatest_eclipse.jd,
                                                               lat,
                                                               lon,
                                                               0.35,
                                                               &candidate)) {
                        almanac_remember_nearest_totality(&candidate, &best);
                    }
                }
            }
        }
    }

    if (best.found) {
        static const double refine_radii_km[] = { 120.0, 45.0 };
        static const double refine_steps_km[] = { 30.0, 15.0 };
        size_t pass;

        for (pass = 0u; pass < sizeof(refine_radii_km) / sizeof(refine_radii_km[0]); ++pass) {
            almanac_solar_totality_location_t refined = best;
            double north_km;

            for (north_km = -refine_radii_km[pass]; north_km <= refine_radii_km[pass]; north_km += refine_steps_km[pass]) {
                double east_km;

                for (east_km = -refine_radii_km[pass]; east_km <= refine_radii_km[pass]; east_km += refine_steps_km[pass]) {
                    double lat;
                    double lon;

                    almanac_destination_point(best.latitude_degrees,
                                              best.longitude_degrees,
                                              hypot(north_km, east_km),
                                              radians_to_degrees(atan2(east_km, north_km)),
                                              &lat,
                                              &lon);
                    if (!almanac_probable_land_point(lat, lon))
                        continue;
                    if (almanac_probe_solar_totality(almanac,
                                                     observer,
                                                     eclipse->greatest_eclipse.jd,
                                                     lat,
                                                     lon,
                                                     &candidate)) {
                        almanac_remember_nearest_totality(&candidate, &refined);
                    }
                }
            }
            best = refined;
        }
        if (almanac_refine_solar_totality(almanac,
                                          observer,
                                          eclipse->greatest_eclipse.jd,
                                          &best,
                                          &candidate)) {
            best = candidate;
        }
    }

    *out = best;
    return true;
}

static bool almanac_fill_lunar_eclipse(almanac_t *almanac,
                                       double jd,
                                       const almanac_observer_t *observer,
                                       almanac_lunar_eclipse_t *out)
{
    almanac_lunar_eclipse_geometry_t geometry;
    almanac_eclipse_contact_context_t contact = { ALMANAC_BODY_ID_MOON, observer, 0 };
    static const double contact_step_days = 1.0 / 24.0;
    double umbral_magnitude;
    double penumbral_magnitude;
    double totality_percent;

    if (!almanac || !out)
        return false;
    if (!almanac_lunar_eclipse_geometry(almanac, jd, observer, &geometry))
        return false;
    if (geometry.moon_altitude_degrees + geometry.moon_sd <= 0.0)
        return false;

    penumbral_magnitude =
        (geometry.penumbra_radius + geometry.moon_sd - geometry.opposition_error) / (2.0 * geometry.moon_sd);
    umbral_magnitude =
        (geometry.umbra_radius + geometry.moon_sd - geometry.opposition_error) / (2.0 * geometry.moon_sd);
    if (penumbral_magnitude <= 0.0)
        return false;
    totality_percent = umbral_magnitude > 0.0
        ? almanac_disc_coverage_percent(geometry.moon_sd,
                                        geometry.umbra_radius,
                                        geometry.opposition_error)
        : 0.0;
    if (!(totality_percent == totality_percent))
        return false;
    totality_percent = fmax(0.0, fmin(100.0, totality_percent));

    memset(out, 0, sizeof(*out));
    almanac_event_time_from_jd(almanac_find_contact_jd(almanac,
                                                       almanac_lunar_eclipse_contact_residual,
                                                       &contact,
                                                       jd,
                                                       -1.0,
                                                       1.0,
                                                       contact_step_days),
                               &out->p1_contact);
    almanac_event_time_from_jd(NAN, &out->u1_contact);
    almanac_event_time_from_jd(NAN, &out->u2_contact);
    almanac_event_time_from_jd(jd, &out->greatest_eclipse);
    almanac_event_time_from_jd(NAN, &out->u3_contact);
    almanac_event_time_from_jd(NAN, &out->u4_contact);
    almanac_event_time_from_jd(almanac_find_contact_jd(almanac,
                                                       almanac_lunar_eclipse_contact_residual,
                                                       &contact,
                                                       jd,
                                                       1.0,
                                                       1.0,
                                                       contact_step_days),
                               &out->p4_contact);
    out->opposition_error_degrees = geometry.opposition_error;
    out->umbral_magnitude = umbral_magnitude;
    out->penumbral_magnitude = penumbral_magnitude;
    out->totality_percent = totality_percent;
    out->umbral_radius_degrees = geometry.umbra_radius;
    out->penumbral_radius_degrees = geometry.penumbra_radius;
    out->moon_semi_diameter_degrees = geometry.moon_sd;
    if (umbral_magnitude >= 1.0)
        out->kind = ALMANAC_LUNAR_ECLIPSE_TOTAL;
    else if (umbral_magnitude > 0.0)
        out->kind = ALMANAC_LUNAR_ECLIPSE_PARTIAL;
    else
        out->kind = ALMANAC_LUNAR_ECLIPSE_PENUMBRAL;
    if (out->kind == ALMANAC_LUNAR_ECLIPSE_PARTIAL ||
        out->kind == ALMANAC_LUNAR_ECLIPSE_TOTAL) {
        contact.contact_level = ALMANAC_CONTACT_LEVEL_INNER;
        almanac_event_time_from_jd(almanac_find_contact_jd(almanac,
                                                           almanac_lunar_eclipse_contact_residual,
                                                           &contact,
                                                           jd,
                                                           -1.0,
                                                           1.0,
                                                           contact_step_days),
                                   &out->u1_contact);
        almanac_event_time_from_jd(almanac_find_contact_jd(almanac,
                                                           almanac_lunar_eclipse_contact_residual,
                                                           &contact,
                                                           jd,
                                                           1.0,
                                                           1.0,
                                                           contact_step_days),
                                   &out->u4_contact);
    }
    if (out->kind == ALMANAC_LUNAR_ECLIPSE_TOTAL) {
        contact.contact_level = ALMANAC_CONTACT_LEVEL_TOTAL;
        almanac_event_time_from_jd(almanac_find_contact_jd(almanac,
                                                           almanac_lunar_eclipse_contact_residual,
                                                           &contact,
                                                           jd,
                                                           -1.0,
                                                           1.0,
                                                           contact_step_days),
                                   &out->u2_contact);
        almanac_event_time_from_jd(almanac_find_contact_jd(almanac,
                                                           almanac_lunar_eclipse_contact_residual,
                                                           &contact,
                                                           jd,
                                                           1.0,
                                                           1.0,
                                                           contact_step_days),
                                   &out->u3_contact);
    }
    return true;
}

static bool almanac_fill_solar_transit(almanac_t *almanac,
                                       almanac_body_id_t body_id,
                                       double jd,
                                       const almanac_observer_t *observer,
                                       almanac_solar_transit_t *out)
{
    almanac_solar_transit_geometry_t geometry;
    almanac_eclipse_contact_context_t contact = { body_id, observer, 0 };
    static const double contact_step_days = 1.0 / 24.0;

    if (!almanac || body_id <= ALMANAC_BODY_ID_UNKNOWN ||
        body_id >= ALMANAC_BODY_ID_COUNT || !out)
        return false;
    if (!almanac_solar_transit_geometry(almanac, body_id, jd, observer, &geometry))
        return false;
    if (geometry.separation >= geometry.sun_sd + geometry.body_sd)
        return false;
    if (geometry.sun_altitude_degrees + geometry.sun_sd <= 0.0)
        return false;

    memset(out, 0, sizeof(*out));
    out->body_id = geometry.body_id;
    almanac_event_time_from_jd(almanac_find_contact_jd(almanac,
                                                       almanac_solar_transit_contact_residual,
                                                       &contact,
                                                       jd,
                                                       -1.0,
                                                       0.75,
                                                       contact_step_days),
                               &out->first_contact);
    contact.contact_level = ALMANAC_CONTACT_LEVEL_INNER;
    almanac_event_time_from_jd(almanac_find_contact_jd(almanac,
                                                       almanac_solar_transit_contact_residual,
                                                       &contact,
                                                       jd,
                                                       -1.0,
                                                       0.75,
                                                       contact_step_days),
                               &out->second_contact);
    almanac_event_time_from_jd(jd, &out->greatest_transit);
    almanac_event_time_from_jd(almanac_find_contact_jd(almanac,
                                                       almanac_solar_transit_contact_residual,
                                                       &contact,
                                                       jd,
                                                       1.0,
                                                       0.75,
                                                       contact_step_days),
                               &out->third_contact);
    contact.contact_level = ALMANAC_CONTACT_LEVEL_OUTER;
    almanac_event_time_from_jd(almanac_find_contact_jd(almanac,
                                                       almanac_solar_transit_contact_residual,
                                                       &contact,
                                                       jd,
                                                       1.0,
                                                       0.75,
                                                       contact_step_days),
                               &out->fourth_contact);
    out->separation_degrees = geometry.separation;
    out->solar_semi_diameter_degrees = geometry.sun_sd;
    out->planet_semi_diameter_degrees = geometry.body_sd;
    out->chord_distance_fraction = geometry.separation / geometry.sun_sd;
    out->interior = geometry.separation <= geometry.sun_sd - geometry.body_sd;
    return true;
}

almanac_solar_eclipse_kind_t almanac_solar_eclipse_kind(const almanac_solar_eclipse_t *event)
{
    return event ? event->kind : ALMANAC_SOLAR_ECLIPSE_PARTIAL;
}

bool almanac_solar_eclipse_time(const almanac_solar_eclipse_t *event,
                                almanac_event_time_kind_t time_kind,
                                almanac_event_time_t *out)
{
    const almanac_event_time_t *time = NULL;

    if (!out)
        return false;
    almanac_event_time_from_jd(NAN, out);
    if (!event)
        return false;

    switch (time_kind) {
    case ALMANAC_EVENT_TIME_FIRST_CONTACT:
        time = &event->first_contact;
        break;
    case ALMANAC_EVENT_TIME_SECOND_CONTACT:
        time = &event->second_contact;
        break;
    case ALMANAC_EVENT_TIME_GREATEST:
        time = &event->greatest_eclipse;
        break;
    case ALMANAC_EVENT_TIME_THIRD_CONTACT:
        time = &event->third_contact;
        break;
    case ALMANAC_EVENT_TIME_FOURTH_CONTACT:
        time = &event->fourth_contact;
        break;
    default:
        return false;
    }
    *out = *time;
    return time->valid;
}

double almanac_solar_eclipse_separation_degrees(const almanac_solar_eclipse_t *event)
{
    return event ? event->separation_degrees : NAN;
}

double almanac_solar_eclipse_magnitude(const almanac_solar_eclipse_t *event)
{
    return event ? event->magnitude : NAN;
}

double almanac_solar_eclipse_totality_percent(const almanac_solar_eclipse_t *event)
{
    return event ? event->totality_percent : NAN;
}

double almanac_solar_eclipse_sun_semi_diameter_degrees(const almanac_solar_eclipse_t *event)
{
    return event ? event->sun_semi_diameter_degrees : NAN;
}

double almanac_solar_eclipse_moon_semi_diameter_degrees(const almanac_solar_eclipse_t *event)
{
    return event ? event->moon_semi_diameter_degrees : NAN;
}

bool almanac_solar_eclipse_is_central(const almanac_solar_eclipse_t *event)
{
    return event ? event->central : false;
}

almanac_lunar_eclipse_kind_t almanac_lunar_eclipse_kind(const almanac_lunar_eclipse_t *event)
{
    return event ? event->kind : ALMANAC_LUNAR_ECLIPSE_PENUMBRAL;
}

bool almanac_lunar_eclipse_time(const almanac_lunar_eclipse_t *event,
                                almanac_event_time_kind_t time_kind,
                                almanac_event_time_t *out)
{
    const almanac_event_time_t *time = NULL;

    if (!out)
        return false;
    almanac_event_time_from_jd(NAN, out);
    if (!event)
        return false;

    switch (time_kind) {
    case ALMANAC_EVENT_TIME_P1_CONTACT:
        time = &event->p1_contact;
        break;
    case ALMANAC_EVENT_TIME_U1_CONTACT:
        time = &event->u1_contact;
        break;
    case ALMANAC_EVENT_TIME_U2_CONTACT:
        time = &event->u2_contact;
        break;
    case ALMANAC_EVENT_TIME_GREATEST:
        time = &event->greatest_eclipse;
        break;
    case ALMANAC_EVENT_TIME_U3_CONTACT:
        time = &event->u3_contact;
        break;
    case ALMANAC_EVENT_TIME_U4_CONTACT:
        time = &event->u4_contact;
        break;
    case ALMANAC_EVENT_TIME_P4_CONTACT:
        time = &event->p4_contact;
        break;
    default:
        return false;
    }
    *out = *time;
    return time->valid;
}

double almanac_lunar_eclipse_opposition_error_degrees(const almanac_lunar_eclipse_t *event)
{
    return event ? event->opposition_error_degrees : NAN;
}

double almanac_lunar_eclipse_umbral_magnitude(const almanac_lunar_eclipse_t *event)
{
    return event ? event->umbral_magnitude : NAN;
}

double almanac_lunar_eclipse_penumbral_magnitude(const almanac_lunar_eclipse_t *event)
{
    return event ? event->penumbral_magnitude : NAN;
}

double almanac_lunar_eclipse_totality_percent(const almanac_lunar_eclipse_t *event)
{
    return event ? event->totality_percent : NAN;
}

double almanac_lunar_eclipse_umbral_radius_degrees(const almanac_lunar_eclipse_t *event)
{
    return event ? event->umbral_radius_degrees : NAN;
}

double almanac_lunar_eclipse_penumbral_radius_degrees(const almanac_lunar_eclipse_t *event)
{
    return event ? event->penumbral_radius_degrees : NAN;
}

double almanac_lunar_eclipse_moon_semi_diameter_degrees(const almanac_lunar_eclipse_t *event)
{
    return event ? event->moon_semi_diameter_degrees : NAN;
}

almanac_body_id_t almanac_solar_transit_body_id(const almanac_solar_transit_t *event)
{
    return event ? event->body_id : ALMANAC_BODY_ID_UNKNOWN;
}

bool almanac_solar_transit_time(const almanac_solar_transit_t *event,
                                almanac_event_time_kind_t time_kind,
                                almanac_event_time_t *out)
{
    const almanac_event_time_t *time = NULL;

    if (!out)
        return false;
    almanac_event_time_from_jd(NAN, out);
    if (!event)
        return false;

    switch (time_kind) {
    case ALMANAC_EVENT_TIME_FIRST_CONTACT:
        time = &event->first_contact;
        break;
    case ALMANAC_EVENT_TIME_SECOND_CONTACT:
        time = &event->second_contact;
        break;
    case ALMANAC_EVENT_TIME_GREATEST:
        time = &event->greatest_transit;
        break;
    case ALMANAC_EVENT_TIME_THIRD_CONTACT:
        time = &event->third_contact;
        break;
    case ALMANAC_EVENT_TIME_FOURTH_CONTACT:
        time = &event->fourth_contact;
        break;
    default:
        return false;
    }
    *out = *time;
    return time->valid;
}

double almanac_solar_transit_separation_degrees(const almanac_solar_transit_t *event)
{
    return event ? event->separation_degrees : NAN;
}

double almanac_solar_transit_solar_semi_diameter_degrees(const almanac_solar_transit_t *event)
{
    return event ? event->solar_semi_diameter_degrees : NAN;
}

double almanac_solar_transit_planet_semi_diameter_degrees(const almanac_solar_transit_t *event)
{
    return event ? event->planet_semi_diameter_degrees : NAN;
}

double almanac_solar_transit_chord_distance_fraction(const almanac_solar_transit_t *event)
{
    return event ? event->chord_distance_fraction : NAN;
}

bool almanac_solar_transit_is_interior(const almanac_solar_transit_t *event)
{
    return event ? event->interior : false;
}

array_t *almanac_find_solar_eclipses(almanac_t *almanac,
                                     const almanac_observer_t *observer,
                                     const datetime_t *start,
                                     const datetime_t *end)
{
    static const double synodic_month_days = 29.530588861;
    static const double base_new_moon_jd = 2451550.09766;
    double start_jd;
    double end_jd;
    long first_k;
    long last_k;
    long k;
    array_t *events;
    almanac_eclipse_contact_context_t contact = { ALMANAC_BODY_ID_MOON, observer, 0 };

    if (!almanac_event_window_is_valid(almanac, start, end, &start_jd, &end_jd))
        return NULL;
    if (!almanac_observer_is_valid(almanac, observer))
        return NULL;
    events = array_create(sizeof(almanac_solar_eclipse_t), NULL, NULL);
    if (!events) {
        almanac_set_error(almanac, "failed to allocate solar eclipse array");
        return NULL;
    }
    first_k = (long)floor((start_jd - base_new_moon_jd) / synodic_month_days) - 2L;
    last_k = (long)ceil((end_jd - base_new_moon_jd) / synodic_month_days) + 2L;
    for (k = first_k; k <= last_k; ++k) {
        almanac_solar_eclipse_t eclipse;
        double phase_jd;
        double local_jd;

        if (!almanac_eclipse_candidate_near_node((double)k))
            continue;
        if (!almanac_refine_body_sun_longitude_near(almanac,
                                                     ALMANAC_BODY_ID_MOON,
                                                     0.0,
                                                     almanac_mean_moon_phase_jd((double)k),
                                                     2.0,
                                                     &phase_jd)) {
            array_destroy(events);
            almanac_set_error(almanac, "failed to refine solar eclipse conjunction");
            return NULL;
        }
        if (phase_jd < start_jd - 1.0 || phase_jd > end_jd + 1.0)
            continue;
        local_jd = almanac_find_local_minimum_jd(almanac,
                                                 almanac_solar_eclipse_metric,
                                                 &contact,
                                                 phase_jd,
                                                 0.75,
                                                 1.0 / 8.0);
        if (local_jd >= start_jd - 1e-9 &&
            local_jd <= end_jd + 1e-9 &&
            almanac_fill_solar_eclipse(almanac, local_jd, observer, &eclipse) &&
            !array_add(events, &eclipse)) {
            array_destroy(events);
            almanac_set_error(almanac, "failed to append solar eclipse event");
            return NULL;
        }
    }

    return events;
}

bool almanac_solar_eclipse_in_progress(almanac_t *almanac,
                                       const almanac_observer_t *observer,
                                       const datetime_t *moment)
{
    almanac_solar_eclipse_geometry_t geometry;
    double jd;

    if (!almanac || !observer || !moment)
        return false;
    if (!almanac_observer_is_valid(almanac, observer))
        return false;
    jd = datetime_jd(moment);
    if (!isfinite(jd))
        return false;
    if (!almanac_solar_eclipse_geometry(almanac, jd, observer, &geometry))
        return false;
    return geometry.sun_altitude_degrees + geometry.sun_sd > 0.0 &&
           geometry.separation < geometry.sun_sd + geometry.moon_sd;
}

array_t *almanac_find_lunar_eclipses(almanac_t *almanac,
                                     const almanac_observer_t *observer,
                                     const datetime_t *start,
                                     const datetime_t *end)
{
    static const double synodic_month_days = 29.530588861;
    static const double base_new_moon_jd = 2451550.09766;
    double start_jd;
    double end_jd;
    long first_k;
    long last_k;
    long k;
    array_t *events;
    almanac_eclipse_contact_context_t contact = { ALMANAC_BODY_ID_MOON, observer, 0 };

    if (!almanac_event_window_is_valid(almanac, start, end, &start_jd, &end_jd))
        return NULL;
    if (!almanac_observer_is_valid(almanac, observer))
        return NULL;
    events = array_create(sizeof(almanac_lunar_eclipse_t), NULL, NULL);
    if (!events) {
        almanac_set_error(almanac, "failed to allocate lunar eclipse array");
        return NULL;
    }
    first_k = (long)floor((start_jd - base_new_moon_jd) / synodic_month_days - 0.5) - 2L;
    last_k = (long)ceil((end_jd - base_new_moon_jd) / synodic_month_days - 0.5) + 2L;
    for (k = first_k; k <= last_k; ++k) {
        almanac_lunar_eclipse_t eclipse;
        double phase_k = (double)k + 0.5;
        double phase_jd;
        double local_jd;

        if (!almanac_eclipse_candidate_near_node(phase_k))
            continue;
        if (!almanac_refine_body_sun_longitude_near(almanac,
                                                     ALMANAC_BODY_ID_MOON,
                                                     180.0,
                                                     almanac_mean_moon_phase_jd(phase_k),
                                                     2.0,
                                                     &phase_jd)) {
            array_destroy(events);
            almanac_set_error(almanac, "failed to refine lunar eclipse opposition");
            return NULL;
        }
        if (phase_jd < start_jd - 1.0 || phase_jd > end_jd + 1.0)
            continue;
        local_jd = almanac_find_local_minimum_jd(almanac,
                                                 almanac_lunar_eclipse_metric,
                                                 &contact,
                                                 phase_jd,
                                                 0.75,
                                                 1.0 / 8.0);
        if (local_jd >= start_jd - 1e-9 &&
            local_jd <= end_jd + 1e-9 &&
            almanac_fill_lunar_eclipse(almanac, local_jd, observer, &eclipse) &&
            !array_add(events, &eclipse)) {
            array_destroy(events);
            almanac_set_error(almanac, "failed to append lunar eclipse event");
            return NULL;
        }
    }

    return events;
}

array_t *almanac_find_solar_transits_for_body(almanac_t *almanac,
                                              almanac_body_id_t body_id,
                                              const almanac_observer_t *observer,
                                              const datetime_t *start,
                                              const datetime_t *end)
{
    double reference_jd;
    double synodic_period_days;
    double start_jd;
    double end_jd;
    double previous_root = DBL_MAX;
    long first_cycle;
    long last_cycle;
    long cycle;
    array_t *events;

    if (!almanac) {
        almanac_set_error(almanac, "invalid solar transit request");
        return NULL;
    }
    if (body_id != ALMANAC_BODY_ID_MERCURY &&
        body_id != ALMANAC_BODY_ID_VENUS) {
        almanac_set_error(almanac, "solar transits currently support MERCURY or VENUS only");
        return NULL;
    }
    if (!almanac_event_window_is_valid(almanac, start, end, &start_jd, &end_jd))
        return NULL;
    if (!almanac_observer_is_valid(almanac, observer))
        return NULL;
    if (body_id == ALMANAC_BODY_ID_MERCURY) {
        /*
         * ESAA 9.222 gives a 116-day mean synodic period.  This more precise
         * value and the 2019 transit epoch keep each inferior-conjunction
         * estimate close enough for rapid ephemeris refinement.
         */
        reference_jd = 2458799.139;
        synodic_period_days = 115.8774771;
    } else {
        reference_jd = 2456084.562;
        synodic_period_days = 583.921361;
    }
    events = array_create(sizeof(almanac_solar_transit_t), NULL, NULL);
    if (!events) {
        almanac_set_error(almanac, "failed to allocate solar transit array");
        return NULL;
    }

    first_cycle = (long)floor((start_jd - reference_jd) / synodic_period_days) - 1L;
    last_cycle = (long)ceil((end_jd - reference_jd) / synodic_period_days) + 1L;
    for (cycle = first_cycle; cycle <= last_cycle; ++cycle) {
        double estimate_jd = reference_jd + (double)cycle * synodic_period_days;
        double conjunction_jd;

        if (almanac_refine_body_sun_longitude_near(almanac,
                                                   body_id,
                                                   0.0,
                                                   estimate_jd,
                                                   8.0,
                                                   &conjunction_jd)) {
            almanac_solar_transit_t transit;
            almanac_eclipse_contact_context_t contact = { body_id, observer, 0 };
            double local_jd;

            if (conjunction_jd < start_jd - 1.0 || conjunction_jd > end_jd + 1.0)
                continue;
            if (previous_root != DBL_MAX && fabs(conjunction_jd - previous_root) < 1.0)
                continue;
            previous_root = conjunction_jd;
            local_jd = almanac_find_local_minimum_jd(almanac,
                                                     almanac_solar_transit_metric,
                                                     &contact,
                                                     conjunction_jd,
                                                     0.75,
                                                     1.0 / 8.0);
            if (local_jd < start_jd - 1e-9 || local_jd > end_jd + 1e-9)
                continue;
            if (almanac_fill_solar_transit(almanac, body_id, local_jd, observer, &transit) &&
                !array_add(events, &transit)) {
                array_destroy(events);
                almanac_set_error(almanac, "failed to append solar transit event");
                return NULL;
            }
        }
    }

    return events;
}

array_t *almanac_find_solar_transits(almanac_t *almanac,
                                     const char *body_code,
                                     const almanac_observer_t *observer,
                                     const datetime_t *start,
                                     const datetime_t *end)
{
    almanac_body_id_t body_id;

    if (!almanac || !body_code) {
        almanac_set_error(almanac, "invalid solar transit request");
        return NULL;
    }
    body_id = almanac_body_id_from_code(body_code);
    return almanac_find_solar_transits_for_body(almanac, body_id, observer, start, end);
}

array_t *almanac_snapshot(almanac_t *almanac, const datetime_t *moment)
{
    static const char *sql =
        "select b.body_id "
        "from almanac_body as b "
        "join almanac_body_enabled as enabled on enabled.body_id = b.body_id "
        "join almanac_body_sort_order as sort on sort.body_id = b.body_id "
        "where enabled.enabled = 'Y' "
        "order by sort.sort_order asc, b.body_id asc";
    sqlite_stmt_t *stmt = NULL;
    array_t *entries = NULL;
    sqlite_step_result_t rc;

    if (!almanac || !moment) {
        almanac_set_error(almanac, "invalid almanac snapshot request");
        return NULL;
    }
    if (!almanac->db) {
        almanac_set_error(almanac, "almanac database is not open");
        return NULL;
    }

    entries = array_create(sizeof(almanac_entry_t), NULL, NULL);
    if (!entries) {
        almanac_set_error(almanac, "failed to allocate almanac snapshot array");
        return NULL;
    }

    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        array_destroy(entries);
        almanac_set_sqlite_error(almanac);
        return NULL;
    }

    while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        almanac_body_id_t body_id = (almanac_body_id_t)sqlite_stmt_column_int(stmt, 0);
        almanac_entry_t entry;

        if (!almanac_entry_fill_body(almanac, body_id, moment, &entry) ||
            !array_add(entries, &entry)) {
            sqlite_stmt_finalize(stmt);
            array_destroy(entries);
            if (!string_length(almanac->error))
                almanac_set_error(almanac, "failed to build almanac snapshot");
            return NULL;
        }
    }

    sqlite_stmt_finalize(stmt);
    if (rc != SQLITE_STEP_DONE) {
        array_destroy(entries);
        almanac_set_sqlite_error(almanac);
        return NULL;
    }

    return entries;
}

bool almanac_serialize(const almanac_t *almanac,
                       string_t **out_type,
                       string_t **out_encoding,
                       void **out_data,
                       size_t *out_len)
{
    static const char sentinel[] = "default";
    void *payload;

    if (!almanac || !almanac->db || !out_type || !out_encoding || !out_data || !out_len)
        return false;

    *out_type = string_new_with("almanac_t");
    *out_encoding = string_new_with("mars/configured-engine-v1");
    if (!*out_type || !*out_encoding) {
        string_free(*out_type);
        string_free(*out_encoding);
        *out_type = NULL;
        *out_encoding = NULL;
        return false;
    }

    payload = malloc(sizeof(sentinel) - 1u);
    if (!payload) {
        string_free(*out_type);
        string_free(*out_encoding);
        *out_type = NULL;
        *out_encoding = NULL;
        return false;
    }

    memcpy(payload, sentinel, sizeof(sentinel) - 1u);
    *out_data = payload;
    *out_len = sizeof(sentinel) - 1u;
    return true;
}

almanac_t *almanac_deserialise(const void *data,
                               size_t len,
                               const string_t *type,
                               const string_t *encoding)
{
    static const char sentinel[] = "default";

    if (!data || !type || !encoding)
        return NULL;
    if (strcmp(string_c_str(type), "almanac_t") != 0)
        return NULL;
    if (strcmp(string_c_str(encoding), "mars/configured-engine-v1") != 0)
        return NULL;
    if (len != sizeof(sentinel) - 1u)
        return NULL;
    if (memcmp(data, sentinel, sizeof(sentinel) - 1u) != 0)
        return NULL;

    return almanac_open();
}
