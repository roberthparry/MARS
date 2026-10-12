/**
 * @file number_physical_constants.c
 * @brief Exact, immortal SI-valued physical and conventional constants.
 *
 * Shared definitions for numeric callers and quantity conversions. Integer
 * ratios preserve the BIPM defining values and IAU/NIST conventions without
 * decimal rounding; materialised results follow number_t's precision policy.
 * These scalars carry no unit metadata. See docs/number.md for units and sources.
 */
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

typedef union {
    number_private_t priv;
    number_t pub;
} number_physical_storage_t;

static number_mpq_t number_speed_of_light_value = {
    .constant_id = NUMBER_CONST_SPEED_OF_LIGHT, .immortal = true, .initialised = false};
static const number_physical_storage_t num_speed_of_light_storage = {
    .priv = {.kind = NUMBER_MPQ, .value.mpq = &number_speed_of_light_value}};
extern const number_t NUM_SPEED_OF_LIGHT __attribute__((alias("num_speed_of_light_storage")));

static number_mpq_t number_elementary_charge_value = {
    .constant_id = NUMBER_CONST_ELEMENTARY_CHARGE, .immortal = true, .initialised = false};
static const number_physical_storage_t num_elementary_charge_storage = {
    .priv = {.kind = NUMBER_MPQ, .value.mpq = &number_elementary_charge_value}};
extern const number_t NUM_ELEMENTARY_CHARGE __attribute__((alias("num_elementary_charge_storage")));

static number_mpq_t number_planck_value = {
    .constant_id = NUMBER_CONST_PLANCK, .immortal = true, .initialised = false};
static const number_physical_storage_t num_planck_storage = {
    .priv = {.kind = NUMBER_MPQ, .value.mpq = &number_planck_value}};
extern const number_t NUM_PLANCK __attribute__((alias("num_planck_storage")));

static number_mpq_t number_boltzmann_value = {
    .constant_id = NUMBER_CONST_BOLTZMANN, .immortal = true, .initialised = false};
static const number_physical_storage_t num_boltzmann_storage = {
    .priv = {.kind = NUMBER_MPQ, .value.mpq = &number_boltzmann_value}};
extern const number_t NUM_BOLTZMANN __attribute__((alias("num_boltzmann_storage")));

static number_mpq_t number_avogadro_value = {
    .constant_id = NUMBER_CONST_AVOGADRO, .immortal = true, .initialised = false};
static const number_physical_storage_t num_avogadro_storage = {
    .priv = {.kind = NUMBER_MPQ, .value.mpq = &number_avogadro_value}};
extern const number_t NUM_AVOGADRO __attribute__((alias("num_avogadro_storage")));

static number_mpq_t number_standard_gravity_value = {
    .constant_id = NUMBER_CONST_STANDARD_GRAVITY, .immortal = true, .initialised = false};
static const number_physical_storage_t num_standard_gravity_storage = {
    .priv = {.kind = NUMBER_MPQ, .value.mpq = &number_standard_gravity_value}};
extern const number_t NUM_STANDARD_GRAVITY __attribute__((alias("num_standard_gravity_storage")));

static number_mpq_t number_astronomical_unit_value = {
    .constant_id = NUMBER_CONST_ASTRONOMICAL_UNIT, .immortal = true, .initialised = false};
static const number_physical_storage_t num_astronomical_unit_storage = {
    .priv = {.kind = NUMBER_MPQ, .value.mpq = &number_astronomical_unit_value}};
extern const number_t NUM_ASTRONOMICAL_UNIT __attribute__((alias("num_astronomical_unit_storage")));

static number_mpq_t number_julian_year_value = {
    .constant_id = NUMBER_CONST_JULIAN_YEAR, .immortal = true, .initialised = false};
static const number_physical_storage_t num_julian_year_storage = {
    .priv = {.kind = NUMBER_MPQ, .value.mpq = &number_julian_year_value}};
extern const number_t NUM_JULIAN_YEAR __attribute__((alias("num_julian_year_storage")));

static const char *const number_physical_sources[NUMBER_CONST_COUNT]             = {
    [NUMBER_CONST_SPEED_OF_LIGHT]    = "299792458/1",
    [NUMBER_CONST_ELEMENTARY_CHARGE] = "1602176634/10000000000000000000000000000",
    [NUMBER_CONST_PLANCK]            = "662607015/1000000000000000000000000000000000000000000",
    [NUMBER_CONST_BOLTZMANN]         = "1380649/100000000000000000000000000000",
    [NUMBER_CONST_AVOGADRO]          = "602214076000000000000000/1",
    [NUMBER_CONST_STANDARD_GRAVITY]  = "196133/20000",
    [NUMBER_CONST_ASTRONOMICAL_UNIT] = "149597870700/1",
    [NUMBER_CONST_JULIAN_YEAR]       = "31557600/1",
};

static number_mpq_t *const number_physical_values[] = {
    &number_speed_of_light_value,
    &number_elementary_charge_value,
    &number_planck_value,
    &number_boltzmann_value,
    &number_avogadro_value,
    &number_standard_gravity_value,
    &number_astronomical_unit_value,
    &number_julian_year_value,
};

const char *number_physical_exact_source(number_const_id_t id)
{
    return (unsigned)id < NUMBER_CONST_COUNT ? number_physical_sources[id] : NULL;
}

void number_physical_constants_shutdown(void)
{
    for (size_t i = 0; i < sizeof(number_physical_values) / sizeof(*number_physical_values); ++i) {
        number_mpq_t *value = number_physical_values[i];
        if (value->initialised) {
            mpq_clear(value->value);
            value->initialised = false;
        }
    }
}
