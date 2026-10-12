/**
 * @file test_quantity.c
 * @brief Exact conversions, dimensions, temperature semantics and quantity text regressions.
 *
 * Checks public ownership, complex scalars, high-precision astronomical units
 * and rejection of ambiguous units. Runnable README examples come last.
 */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "quantity-t.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static quantity_t *parse(const char *text)
{
    string_t *input = string_new_with(text);
    quantity_t *quantity = quantity_from_string(input);
    string_free(input);
    return quantity;
}

static quantity_t *convert(const quantity_t *quantity, const char *name)
{
    string_t *unit = string_new_with(name);
    quantity_t *result = quantity_convert(quantity, unit);
    string_free(unit);
    return result;
}

static bool equals(const quantity_t *quantity, const char *expected, bool exact)
{
    number_t value = quantity_value(quantity), reference = num_create_from_string(expected);
    bool ok = quantity && num_eq(value, reference) && (!exact || num_is_exact(value));
    num_destroy(&value);
    num_destroy(&reference);
    return ok;
}

static void test_quantity_aliases(void)
{
    static const struct {
        const char *alias;
        const char *canonical;
    } cases[] = {
        {"1", "1"},
        {"dimensionless", "1"},
        {"m", "m"},
        {"metre", "m"},
        {"metres", "m"},
        {"meter", "m"},
        {"meters", "m"},
        {"cm", "cm"},
        {"centimetre", "cm"},
        {"centimetres", "cm"},
        {"mm", "mm"},
        {"millimetre", "mm"},
        {"millimetres", "mm"},
        {"km", "km"},
        {"kilometre", "km"},
        {"kilometres", "km"},
        {"in", "in"},
        {"inch", "in"},
        {"inches", "in"},
        {"ft", "ft"},
        {"foot", "ft"},
        {"feet", "ft"},
        {"yd", "yd"},
        {"yard", "yd"},
        {"yards", "yd"},
        {"mi", "mi"},
        {"mile", "mi"},
        {"miles", "mi"},
        {"fur", "fur"},
        {"furlong", "fur"},
        {"furlongs", "fur"},
        {"lea", "lea"},
        {"league", "lea"},
        {"leagues", "lea"},
        {"au", "au"},
        {"AU", "au"},
        {"astronomical unit", "au"},
        {"astronomical units", "au"},
        {"ly", "ly"},
        {"light year", "ly"},
        {"light years", "ly"},
        {"light-year", "ly"},
        {"light-years", "ly"},
        {"pc", "pc"},
        {"parsec", "pc"},
        {"parsecs", "pc"},
        {"kg", "kg"},
        {"kilogram", "kg"},
        {"kilograms", "kg"},
        {"g", "g"},
        {"gram", "g"},
        {"grams", "g"},
        {"t", "t"},
        {"tonne", "t"},
        {"tonnes", "t"},
        {"metric ton", "t"},
        {"metric tons", "t"},
        {"metric-ton", "t"},
        {"metric-tons", "t"},
        {"uk_ton", "uk_ton"},
        {"uk ton", "uk_ton"},
        {"uk tons", "uk_ton"},
        {"UK ton", "uk_ton"},
        {"UK tons", "uk_ton"},
        {"long ton", "uk_ton"},
        {"long tons", "uk_ton"},
        {"long-ton", "uk_ton"},
        {"long-tons", "uk_ton"},
        {"imperial ton", "uk_ton"},
        {"imperial tons", "uk_ton"},
        {"British ton", "uk_ton"},
        {"British tons", "uk_ton"},
        {"us_ton", "us_ton"},
        {"us ton", "us_ton"},
        {"us tons", "us_ton"},
        {"US ton", "us_ton"},
        {"US tons", "us_ton"},
        {"short ton", "us_ton"},
        {"short tons", "us_ton"},
        {"short-ton", "us_ton"},
        {"short-tons", "us_ton"},
        {"lb", "lb"},
        {"lbs", "lb"},
        {"pound", "lb"},
        {"pounds", "lb"},
        {"st", "st"},
        {"stone", "st"},
        {"stones", "st"},
        {"oz", "oz"},
        {"ounce", "oz"},
        {"ounces", "oz"},
        {"oz_t", "oz_t"},
        {"ozt", "oz_t"},
        {"troy oz", "oz_t"},
        {"troy ounce", "oz_t"},
        {"troy ounces", "oz_t"},
        {"s", "s"},
        {"second", "s"},
        {"seconds", "s"},
        {"sec", "s"},
        {"min", "min"},
        {"minute", "min"},
        {"minutes", "min"},
        {"h", "h"},
        {"hr", "h"},
        {"hrs", "h"},
        {"hour", "h"},
        {"hours", "h"},
        {"d", "d"},
        {"day", "d"},
        {"days", "d"},
        {"ms", "ms"},
        {"millisecond", "ms"},
        {"milliseconds", "ms"},
        {"us", "us"},
        {"µs", "us"},
        {"μs", "us"},
        {"microsecond", "us"},
        {"microseconds", "us"},
        {"ns", "ns"},
        {"nanosecond", "ns"},
        {"nanoseconds", "ns"},
        {"ps", "ps"},
        {"picosecond", "ps"},
        {"picoseconds", "ps"},
        {"A", "A"},
        {"ampere", "A"},
        {"amperes", "A"},
        {"K", "K"},
        {"kelvin", "K"},
        {"kelvins", "K"},
        {"degC", "degC"},
        {"°C", "degC"},
        {"deg C", "degC"},
        {"Celsius", "degC"},
        {"degF", "degF"},
        {"°F", "degF"},
        {"deg F", "degF"},
        {"Fahrenheit", "degF"},
        {"delta_degC", "delta_degC"},
        {"Δ°C", "delta_degC"},
        {"delta_degF", "delta_degF"},
        {"Δ°F", "delta_degF"},
        {"mol", "mol"},
        {"mole", "mol"},
        {"moles", "mol"},
        {"cd", "cd"},
        {"candela", "cd"},
        {"candelas", "cd"},
        {"rad", "rad"},
        {"radian", "rad"},
        {"radians", "rad"},
        {"deg", "deg"},
        {"°", "deg"},
        {"degree", "deg"},
        {"degrees", "deg"},
        {"arcmin", "arcmin"},
        {"arc minute", "arcmin"},
        {"arc minutes", "arcmin"},
        {"arcminute", "arcmin"},
        {"arcminutes", "arcmin"},
        {"′", "arcmin"},
        {"arcsec", "arcsec"},
        {"arc second", "arcsec"},
        {"arc seconds", "arcsec"},
        {"arcsecond", "arcsec"},
        {"arcseconds", "arcsec"},
        {"″", "arcsec"},
        {"L", "L"},
        {"l", "L"},
        {"litre", "L"},
        {"litres", "L"},
        {"liter", "L"},
        {"liters", "L"},
        {"mL", "mL"},
        {"ml", "mL"},
        {"millilitre", "mL"},
        {"millilitres", "mL"},
        {"uk_gal", "uk_gal"},
        {"uk gallon", "uk_gal"},
        {"uk gallons", "uk_gal"},
        {"UK gallon", "uk_gal"},
        {"UK gallons", "uk_gal"},
        {"imperial gallon", "uk_gal"},
        {"imperial gallons", "uk_gal"},
        {"us_gal", "us_gal"},
        {"us gallon", "us_gal"},
        {"us gallons", "us_gal"},
        {"US gallon", "us_gal"},
        {"US gallons", "us_gal"},
        {"US liquid gallon", "us_gal"},
        {"uk_pt", "uk_pt"},
        {"uk pint", "uk_pt"},
        {"uk pints", "uk_pt"},
        {"UK pint", "uk_pt"},
        {"UK pints", "uk_pt"},
        {"imperial pint", "uk_pt"},
        {"us_pt", "us_pt"},
        {"us pint", "us_pt"},
        {"us pints", "us_pt"},
        {"US pint", "us_pt"},
        {"US pints", "us_pt"},
        {"US liquid pint", "us_pt"},
        {"uk_fl_oz", "uk_fl_oz"},
        {"uk fluid oz", "uk_fl_oz"},
        {"uk fluid ounce", "uk_fl_oz"},
        {"uk fluid ounces", "uk_fl_oz"},
        {"UK fluid oz", "uk_fl_oz"},
        {"UK fluid ounce", "uk_fl_oz"},
        {"UK fluid ounces", "uk_fl_oz"},
        {"UK fl oz", "uk_fl_oz"},
        {"imperial fluid ounce", "uk_fl_oz"},
        {"us_fl_oz", "us_fl_oz"},
        {"us fluid oz", "us_fl_oz"},
        {"us fluid ounce", "us_fl_oz"},
        {"us fluid ounces", "us_fl_oz"},
        {"US fluid oz", "us_fl_oz"},
        {"US fluid ounce", "us_fl_oz"},
        {"US fluid ounces", "us_fl_oz"},
        {"US fl oz", "us_fl_oz"},
        {"N", "N"},
        {"newton", "N"},
        {"newtons", "N"},
        {"J", "J"},
        {"joule", "J"},
        {"joules", "J"},
        {"eV", "eV"},
        {"electronvolt", "eV"},
        {"electronvolts", "eV"},
        {"electron volt", "eV"},
        {"electron volts", "eV"},
        {"electron-volt", "eV"},
        {"electron-volts", "eV"},
        {"GeV", "GeV"},
        {"gigaelectronvolt", "GeV"},
        {"gigaelectronvolts", "GeV"},
        {"giga electron volt", "GeV"},
        {"giga electron volts", "GeV"},
        {"giga electronvolt", "GeV"},
        {"giga electronvolts", "GeV"},
        {"giga-electronvolt", "GeV"},
        {"giga-electronvolts", "GeV"},
        {"lbf", "lbf"},
        {"pound-force", "lbf"},
        {"pounds-force", "lbf"},
        {"ft_lbf", "ft_lbf"},
        {"foot pound", "ft_lbf"},
        {"foot pounds", "ft_lbf"},
        {"foot-pound", "ft_lbf"},
        {"foot-pounds", "ft_lbf"},
        {"Hz", "Hz"},
        {"hertz", "Hz"},
        {"W", "W"},
        {"watt", "W"},
        {"watts", "W"},
        {"Pa", "Pa"},
        {"pascal", "Pa"},
        {"pascals", "Pa"},
        {"percent", "percent"},
        {"%", "percent"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        string_t *unit = string_new_with(cases[i].alias);
        quantity_t *quantity = quantity_new(NUM_ONE, unit);
        TEST_ASSERT_TRUE(quantity != NULL, cases[i].alias);
        TEST_ASSERT_STR_EQ(string_c_str(quantity_unit(quantity)), cases[i].canonical);
        quantity_free(quantity);
        string_append_cstr(unit, "!");
        quantity = quantity_new(NUM_ONE, unit);
        TEST_ASSERT_TRUE(quantity == NULL, "hash hits must validate the whole alias");
        quantity_free(quantity);
        string_free(unit);
    }
}

static void test_quantity_exact_conversions(void)
{
    static const struct {
        const char *source;
        const char *target;
        const char *expected;
    } cases[] = {
        {"1 [inches]", "m", "127/5000"},
        {"1 [feet]", "inches", "12"},
        {"1 [yards]", "feet", "3"},
        {"1 [miles]", "feet", "5280"},
        {"1 [furlongs]", "yards", "220"},
        {"1 [leagues]", "miles", "3"},
        {"1 [pounds]", "kg", "45359237/100000000"},
        {"1 [stone]", "pounds", "14"},
        {"1 [oz]", "lb", "1/16"},
        {"1 [troy oz]", "g", "19439673/625000"},
        {"1 [kg]", "g", "1000"},
        {"1 [tonnes]", "kg", "1000"},
        {"1 [metric tons]", "t", "1"},
        {"1 [UK tons]", "pounds", "2240"},
        {"1 [US tons]", "pounds", "2000"},
        {"1 [long ton]", "kg", "317514659/312500"},
        {"1 [short ton]", "kg", "45359237/50000"},
        {"1 [long tons]", "short tons", "28/25"},
        {"1 [short tons]", "long tons", "25/28"},
        {"1 [tonnes/s]", "kg/s", "1000"},
        {"1 [long tons/m^3]", "kg/m^3", "317514659/312500"},
        {"1 [short tons^-1]", "lb^-1", "1/2000"},
        {"1 [uk gallon]", "L", "454609/100000"},
        {"1 [us gallon]", "L", "473176473/125000000"},
        {"1 [uk pint]", "uk fluid oz", "20"},
        {"1 [us pint]", "us fluid oz", "16"},
        {"1 [uk gallon]", "uk pint", "8"},
        {"1 [us gallon]", "us pint", "8"},
        {"1 [us gallon]", "in^3", "231"},
        {"1 [newtons]", "kg*m/s^2", "1"},
        {"1 [joules]", "N*m", "1"},
        {"1 [electron volts]", "J", "801088317/5000000000000000000000000000"},
        {"1 [giga electron volts]", "J", "801088317/5000000000000000000"},
        {"1 [GeV]", "eV", "1000000000"},
        {"1 [eV]", "GeV", "1/1000000000"},
        {"1 [J]", "eV", "5000000000000000000000000000/801088317"},
        {"1 [J]", "GeV", "5000000000000000000/801088317"},
        {"1 [GeV/s]", "eV/s", "1000000000"},
        {"1 [lbf]", "N", "8896443230521/2000000000000"},
        {"1 [foot pounds]", "joules", "13558179483314004/10000000000000000"},
        {"1 [foot pounds]", "ft*lbf", "1"},
        {"1 [nanoseconds]", "picoseconds", "1000"},
        {"1 [milliseconds]", "nanoseconds", "1000000"},
        {"1 [minutes]", "seconds", "60"},
        {"1 [astronomical units]", "m", "149597870700"},
        {"1 [light years]", "m", "9460730472580800"},
        {"1 [degrees]", "arc seconds", "3600"},
        {"1 [arc minutes]", "arc seconds", "60"},
        {"1 [percent]", "1", "1/100"},
        {"1 [mi/hr/hr]", "m/s^2", "1397/11250000"},
        {"1 [mi/hr^2]", "m/s^2", "1397/11250000"},
        {"1 [mi/(hr*hr)]", "m/s^2", "1397/11250000"},
        {"1 [mi/hrs/hrs]", "m/s^2", "1397/11250000"},
        {"1 [mi/hr/hr]", "mi/hr^2", "1"},
        {"36 [km/h]", "m/s", "10"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        quantity_t *input = parse(cases[i].source), *result = convert(input, cases[i].target);
        TEST_ASSERT_TRUE(equals(result, cases[i].expected, true), cases[i].source);
        quantity_t *back = result ? quantity_convert(result, quantity_unit(input)) : NULL;
        TEST_ASSERT_TRUE(equals(back, "1", true) || (i == sizeof(cases) / sizeof(*cases) - 1 && equals(back, "36", true)),
                         "exact round trip");
        quantity_free(back);
        quantity_free(result);
        quantity_free(input);
    }
}

static void test_quantity_temperature(void)
{
    quantity_t *fahrenheit = parse("32 [deg F]"), *celsius = convert(fahrenheit, "degC");
    quantity_t *spaced = parse(" 32 [degF] ");
    TEST_ASSERT_TRUE(equals(spaced, "32", true) && quantity_is_point(spaced), "temperature whitespace");
    quantity_free(spaced);
    quantity_t *kelvin = quantity_to_si(fahrenheit), *interval = parse("difference(18 [degF])");
    TEST_ASSERT_TRUE(equals(celsius, "0", true) && quantity_is_point(celsius), "freezing point");
    TEST_ASSERT_TRUE(equals(kelvin, "5463/20", true), "absolute temperature");
    quantity_t *warmer = quantity_add(celsius, interval), *swapped = quantity_add(interval, celsius);
    TEST_ASSERT_TRUE(equals(warmer, "10", true) && equals(swapped, "10", true), "point translation");
    quantity_t *difference = quantity_sub(warmer, fahrenheit);
    TEST_ASSERT_TRUE(equals(difference, "10", true) && !quantity_is_point(difference), "point subtraction");
    TEST_ASSERT_STR_EQ(string_c_str(quantity_unit(difference)), "K");
    string_t *temperature_unit = string_new_with("degC");
    quantity_t *reading = quantity_new(NUM_TWO, temperature_unit);
    quantity_t *explicit_reading = quantity_new_point(NUM_TWO, temperature_unit);
    quantity_t *step = quantity_new_difference(NUM_TWO, temperature_unit);
    TEST_ASSERT_TRUE(quantity_is_point(reading) && quantity_is_point(explicit_reading) && !quantity_is_point(step),
                     "ordinary constructor means temperature, difference constructor means interval");
    quantity_t *spaced_step = parse(" difference ( 18 [degF] ) ");
    TEST_ASSERT_TRUE(equals(spaced_step, "18", true) && !quantity_is_point(spaced_step), "difference whitespace");
    quantity_free(spaced_step);
    quantity_free(step);
    quantity_free(explicit_reading);
    quantity_free(reading);
    string_free(temperature_unit);
    quantity_t *back = quantity_sub(warmer, interval);
    TEST_ASSERT_TRUE(equals(back, "0", true) && quantity_is_point(back), "subtract a difference");
    TEST_ASSERT_TRUE(!quantity_add(celsius, fahrenheit) && errno == EDOM, "cannot add points");
    TEST_ASSERT_TRUE(!quantity_sub(interval, celsius) && errno == EDOM, "difference minus point");
    TEST_ASSERT_TRUE(!quantity_mul(celsius, interval) && errno == EDOM, "point product");
    TEST_ASSERT_TRUE(!quantity_neg(celsius) && errno == EDOM, "point negation");
    TEST_ASSERT_TRUE(!convert(celsius, "delta_degC") && errno == EDOM, "no implicit linearisation");
    TEST_ASSERT_TRUE(!parse("difference(1 [m])") && errno == EDOM, "only temperature differences");
    TEST_ASSERT_TRUE(!parse("1+i [K]") && errno == EDOM, "real points only");
    int order = 99;
    TEST_ASSERT_TRUE(quantity_compare(fahrenheit, celsius, &order) && !order, "point equality");
    TEST_ASSERT_TRUE(!quantity_compare(celsius, interval, &order) && errno == EDOM, "distinct coordinate kinds");
    quantity_free(back);
    quantity_free(difference);
    quantity_free(swapped);
    quantity_free(warmer);
    quantity_free(interval);
    quantity_free(kelvin);
    quantity_free(celsius);
    quantity_free(fahrenheit);
}

static void test_quantity_arithmetic(void)
{
    quantity_t *metres = parse("2 [m]"), *centimetres = parse("50 [cm]"), *seconds = parse("2 [s]");
    quantity_t *sum = quantity_add(metres, centimetres), *area = quantity_mul(metres, centimetres);
    quantity_t *speed = quantity_div(metres, seconds), *root = quantity_root(area, 2);
    TEST_ASSERT_TRUE(equals(sum, "5/2", true), "converted sum");
    TEST_ASSERT_TRUE(equals(area, "1", true), "coherent product");
    TEST_ASSERT_STR_EQ(string_c_str(quantity_unit(area)), "m^2");
    TEST_ASSERT_TRUE(equals(root, "1", false) && quantity_compatible(root, metres), "root dimension");
    TEST_ASSERT_STR_EQ(string_c_str(quantity_unit(speed)), "m*s^-1");
    quantity_t *fractional = quantity_root(metres, 3), *cubed = quantity_pow_int(fractional, 3);
    TEST_ASSERT_STR_EQ(string_c_str(quantity_unit(fractional)), "m^(1/3)");
    TEST_ASSERT_TRUE(quantity_compatible(cubed, metres), "rational dimension cancellation");
    quantity_t *negative = quantity_neg(metres), *scaled = quantity_scale(metres, NUM_HALF);
    TEST_ASSERT_TRUE(equals(negative, "-2", true) && equals(scaled, "1", true), "linear scalar operations");
    quantity_t *zero = parse("0 [s]");
    TEST_ASSERT_TRUE(!quantity_div(metres, zero) && errno == EDOM, "zero divisor");
    TEST_ASSERT_TRUE(!quantity_add(metres, seconds) && errno == EDOM, "incompatible sum");
    TEST_ASSERT_TRUE(!convert(metres, "s") && errno == EDOM, "incompatible conversion");
    TEST_ASSERT_TRUE(!quantity_root(metres, 0) && errno == EINVAL, "zero-degree root");
    TEST_ASSERT_TRUE(!quantity_pow_int(metres, INT_MAX) && errno == EOVERFLOW, "bounded exponent");
    TEST_ASSERT_TRUE(!quantity_root(metres, UINT_MAX) && errno == EOVERFLOW, "bounded root degree");
    int ordering = 0;
    TEST_ASSERT_TRUE(quantity_compare(metres, centimetres, &ordering) && ordering > 0, "real ordering");
    quantity_free(zero);
    quantity_free(scaled);
    quantity_free(negative);
    quantity_free(cubed);
    quantity_free(fractional);
    quantity_free(root);
    quantity_free(speed);
    quantity_free(area);
    quantity_free(sum);
    quantity_free(seconds);
    quantity_free(centimetres);
    quantity_free(metres);
}

static void test_quantity_precision(void)
{
    size_t previous = num_get_default_prec_bits();
    num_set_default_prec_bits(53);
    quantity_t *long_ton = parse("1 [long ton]"), *short_ton = parse("1 [short ton]");
    quantity_t *long_kg = convert(long_ton, "kg"), *short_kg = convert(short_ton, "kg");
    TEST_ASSERT_TRUE(equals(long_kg, "317514659/312500", true) && equals(short_kg, "45359237/50000", true),
                     "ton factors remain exact at double precision");
    quantity_free(short_kg);
    quantity_free(long_kg);
    quantity_free(short_ton);
    quantity_free(long_ton);
    quantity_t *parsec = parse("1 [parsecs]"), *angle = parse("180 [degrees]");
    quantity_t *electronvolt = parse("1 [eV]"), *joules = convert(electronvolt, "J");
    TEST_ASSERT_TRUE(equals(joules, "801088317/5000000000000000000000000000", false),
                     "electronvolt output respects the selected precision");
    quantity_t *gev = convert(electronvolt, "GeV");
    TEST_ASSERT_TRUE(equals(gev, "1/1000000000", true), "large unit denominators cancel before rounding");
    quantity_t *large_energy = parse("10000000000000000000000000000 [eV]");
    quantity_t *whole_joules = convert(large_energy, "J");
    TEST_ASSERT_TRUE(equals(whole_joules, "1602176634", true), "scalar cancels the unit denominator before rounding");
    quantity_free(whole_joules);
    quantity_free(large_energy);
    quantity_t *inverse = parse("1 [eV^-1]"), *inverse_gev = convert(inverse, "GeV^-1");
    quantity_t *product = quantity_mul(electronvolt, inverse);
    TEST_ASSERT_TRUE(equals(inverse_gev, "1000000000", true), "negative unit powers preserve integer scale factors");
    TEST_ASSERT_TRUE(equals(product, "1", true), "product cancels large scale factors before conversion");
    quantity_free(product);
    quantity_free(inverse_gev);
    quantity_free(inverse);
    quantity_free(gev);
    quantity_free(joules);
    num_set_default_prec_bits(512);
    joules = convert(electronvolt, "J");
    TEST_ASSERT_TRUE(equals(joules, "801088317/5000000000000000000000000000", true),
                     "low-precision construction preserves exact unit definitions");
    quantity_free(joules);
    quantity_free(electronvolt);
    quantity_t *metres = quantity_to_si(parsec), *radians = convert(angle, "radians");
    number_t pi = num_const_prec(NUM_PI, 512), numerator = num_create_from_string("96939420213600000");
    number_t expected = num_div(numerator, pi), actual = quantity_value(metres);
    number_t difference = num_sub(actual, expected), error = num_abs(difference);
    number_t tolerance = num_create_from_string("1e-130"), angular = quantity_value(radians);
    TEST_ASSERT_TRUE(num_lt(error, tolerance), "parsec definition at requested precision");
    TEST_ASSERT_TRUE(num_eq(angular, pi) && num_get_prec_bits(angular) >= 512, "pi not frozen at creation precision");
    quantity_t *arcseconds = convert(angle, "arcsec");
    TEST_ASSERT_TRUE(equals(arcseconds, "648000", true), "symbolic pi cancellation");
    quantity_t *ratio = quantity_div(angle, arcseconds);
    TEST_ASSERT_TRUE(equals(ratio, "1", true), "cancel pi before dividing quantities");
    quantity_free(ratio);
    num_destroy(&angular);
    num_destroy(&tolerance);
    num_destroy(&error);
    num_destroy(&difference);
    num_destroy(&actual);
    num_destroy(&expected);
    num_destroy(&numerator);
    num_destroy(&pi);
    quantity_free(arcseconds);
    quantity_free(radians);
    quantity_free(metres);
    quantity_free(angle);
    quantity_free(parsec);
    num_set_default_prec_bits(previous);
}

static void test_quantity_selected_precision(void)
{
    size_t saved = num_get_default_prec_bits();
    num_set_default_prec_bits(512);
    quantity_t *high = parse("1.2345678901234567890123456789 [m]");
    quantity_t *complex = parse("1.234567890123456789+2.34567890123456789i [m]");
    number_t high_value = quantity_value(high);
    string_t *unit = string_new_with("m");
    quantity_t *degrees = parse("1.234567890123456789 [deg]");
    num_set_default_prec_bits(53);
    quantity_t *copied = quantity_clone(high), *constructed = quantity_new(high_value, unit);
    quantity_t *converted = convert(high, "cm"), *sum = quantity_add(high, high);
    quantity_t *product = quantity_mul(high, high), *quotient = quantity_div(high, copied);
    quantity_t *root = quantity_root(high, 2), *power = quantity_pow_int(high, 2);
    quantity_t *scaled = quantity_scale(high, NUM_PI), *negative = quantity_neg(high);
    quantity_t *radians = convert(degrees, "rad"), *sine = quantity_sin(degrees);
    quantity_t *complex_converted = convert(complex, "cm");
    quantity_t *results[] = {copied, constructed, converted, sum, product, quotient, root, power,
                             scaled, negative, radians, sine, complex_converted};
    for (size_t i = 0; i < sizeof(results) / sizeof(*results); ++i) {
        TEST_ASSERT_TRUE(results[i] != NULL, "precision-aware operation succeeds");
        number_t value = quantity_value(results[i]);
        TEST_ASSERT_TRUE(num_is_exact(value) || num_get_prec_bits(value) == 53, "selected result precision");
        num_destroy(&value);
        quantity_free(results[i]);
    }
    number_t viewed = quantity_value(high);
    TEST_ASSERT_TRUE(num_get_prec_bits(viewed) == 53 && num_get_prec_bits(high_value) == 512,
                     "read at selected precision without modifying input");
    const style_t styles[] = {style_FUNCTION, style_EXPRESSION, style_LATEX};
    for (size_t i = 0; i < sizeof(styles) / sizeof(*styles); ++i) {
        string_t *text = quantity_to_string(high, styles[i]);
        TEST_ASSERT_TRUE(text && string_byte_length(text) < 100, "formatting does not leak old precision");
        string_free(text);
    }
    num_destroy(&viewed);
    num_set_default_prec_bits(512);
    viewed = quantity_value(high);
    TEST_ASSERT_TRUE(num_get_prec_bits(viewed) == 512 && num_eq(viewed, high_value), "original remains intact");
    num_destroy(&viewed);
    num_destroy(&high_value);
    string_free(unit);
    quantity_free(degrees);
    quantity_free(complex);
    quantity_free(high);
    num_set_default_prec_bits(saved);
}

static void test_quantity_functions_complex(void)
{
    quantity_t *angle = parse("0 [deg]"), *metres = parse("2 [m]");
    quantity_t *sine = quantity_sin(angle), *cosine = quantity_cos(angle), *tangent = quantity_tan(angle);
    TEST_ASSERT_TRUE(equals(sine, "0", false) && equals(cosine, "1", false) && equals(tangent, "0", false),
                     "angle functions");
    TEST_ASSERT_TRUE(!quantity_exp(angle) && errno == EDOM, "angle is not dimensionless");
    TEST_ASSERT_TRUE(!quantity_sin(metres) && errno == EDOM, "length is not angle");
    quantity_t *one = parse("1 [1]"), *logarithm = quantity_log(one), *exponential = quantity_exp(logarithm);
    TEST_ASSERT_TRUE(equals(logarithm, "0", false) && equals(exponential, "1", false), "dimensionless functions");
    quantity_t *complex = parse("1+2i [m]"), *centimetres = convert(complex, "cm");
    TEST_ASSERT_TRUE(equals(centimetres, "100+200i", false), "complex linear conversion");
    int order = 45;
    TEST_ASSERT_TRUE(!quantity_compare(complex, metres, &order) && errno == EDOM && order == 45,
                     "complex ordering rejected without writing output");
    quantity_free(centimetres);
    quantity_free(complex);
    quantity_free(exponential);
    quantity_free(logarithm);
    quantity_free(one);
    quantity_free(tangent);
    quantity_free(cosine);
    quantity_free(sine);
    quantity_free(metres);
    quantity_free(angle);
}

static void test_quantity_parser_and_ownership(void)
{
    static const char *const invalid[] = {"1 [pint]", "1 [gallon]", "1 [fluid oz]", "1 [unknown]", "1 [M]",
        "1 [ev]", "1 [gev]", "1 [GEV]", "1 [ton]", "1 [tons]", "1 [ton/s]", "1 [T]",
        "1 [m+]", "1 [m^]", "1 [m^(1/0)]", "1 [m^32768]", "1 [m^32767*m]", "1 [m/s/]", "1 [degF/s]",
        "difference()", "difference( )", "difference(20 [degC]", "difference20 [degC])",
        "difference(20 [degC]))", "difference(20 [degC]) tail", "difference(difference(20 [degC]))",
        "difference [degC]", "difference(", "difference(20 [degC]) [K]",
        "@32 [degF]", "point(32 [degF])", "point()", "point( )", "point(20 [degC]", "point20 [degC])",
        "point(20 [degC]))", "point(20 [degC]) tail", "point(point(20 [degC]))",
        "point [degC]", "point(", "point(20 [degC]) [K]",
        "1 [m] tail", "1 [m]]", "1 [m", "[m]", "@", "", "garbage [m]", "NaN [m]", "1garbage [m]"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
        quantity_t *quantity = parse(invalid[i]);
        TEST_ASSERT_TRUE(!quantity, invalid[i]);
        quantity_free(quantity);
    }
    quantity_t *parenthesised = parse("4 [(km/h)^2]");
    quantity_t *converted = convert(parenthesised, "m^2/s^2");
    TEST_ASSERT_TRUE(equals(converted, "25/81", true), "grouped units");
    quantity_t *root = parse("1 [m^(1/2)]"), *squared = quantity_pow_int(root, 2);
    TEST_ASSERT_STR_EQ(string_c_str(quantity_unit(squared)), "m");
    quantity_t *original = parse("3/7 [uk_fl_oz]"), *copy = quantity_clone(original);
    number_t value = quantity_value(original);
    quantity_free(original);
    TEST_ASSERT_TRUE(equals(copy, "3/7", true) && num_is_finite(value), "independent ownership");
    num_destroy(&value);
    quantity_free(copy);
    quantity_free(squared);
    quantity_free(root);
    quantity_free(converted);
    quantity_free(parenthesised);
    string_t *nul = string_new();
    string_append_chars(nul, "m\0s", 3);
    TEST_ASSERT_TRUE(!quantity_new(NUM_ONE, nul) && errno == EINVAL, "embedded NUL unit");
    string_free(nul);
    string_t *deep = string_new_with("1 [");
    for (unsigned i = 0; i < 17; ++i)
        string_append_cstr(deep, "(");
    string_append_cstr(deep, "m");
    for (unsigned i = 0; i < 17; ++i)
        string_append_cstr(deep, ")");
    string_append_cstr(deep, "]");
    TEST_ASSERT_TRUE(!quantity_from_string(deep) && errno == EOVERFLOW, "bounded unit nesting");
    string_free(deep);
    quantity_t *force = parse("1 [lbf]"), *mass = parse("1 [lb]"), *mass_length = parse("1 [ft*lb]");
    TEST_ASSERT_TRUE(!quantity_compatible(force, mass), "mass is not force");
    TEST_ASSERT_TRUE(!convert(mass_length, "J") && errno == EDOM, "mass length is not foot-pound energy");
    quantity_free(mass_length);
    quantity_free(mass);
    quantity_free(force);
    TEST_ASSERT_TRUE(!quantity_clone(NULL) && errno == EINVAL, "NULL clone");
    TEST_ASSERT_TRUE(!quantity_convert(NULL, NULL) && errno == EINVAL, "NULL conversion");
    TEST_ASSERT_TRUE(!quantity_add(NULL, NULL) && errno == EINVAL, "NULL arithmetic");
    quantity_free(NULL);
}

static void test_quantity_text(void)
{
    static const char *const inputs[] = {"10 [m/s]", "3/7 [oz_t]", "1+2i [kg]", "32 [degF]",
                                        "1 [m^(1/2)]", "2 [uk fluid oz / seconds]", "3 [m ^ (1 / 2)]",
                                        "difference(20 [degC])", "difference(18 [degF])",
                                        "difference(10 [K])", "10 [delta_degC]", "273 [K]",
                                        "1 [electron volts]", "2 [giga electron volts]"};
    for (size_t i = 0; i < sizeof(inputs) / sizeof(*inputs); ++i) {
        quantity_t *quantity = parse(inputs[i]);
        string_t *function = quantity_to_string(quantity, style_FUNCTION);
        quantity_t *parsed = quantity_from_string(function);
        number_t a = quantity_value(quantity), b = quantity_value(parsed);
        TEST_ASSERT_TRUE(parsed && num_eq(a, b) && quantity_compatible(quantity, parsed) &&
                             quantity_is_point(quantity) == quantity_is_point(parsed), "text round trip");
        num_destroy(&a);
        num_destroy(&b);
        string_t *expression = quantity_to_string(quantity, style_EXPRESSION);
        string_t *TeX = quantity_to_string(quantity, style_LATEX);
        TEST_ASSERT_TRUE(expression && TeX && strstr(string_c_str(TeX), "\\mathrm{"), "native display styles");
        string_free(TeX);
        string_free(expression);
        quantity_free(parsed);
        string_free(function);
        quantity_free(quantity);
    }
    quantity_t *acceleration = parse("1 [m / s ^ 2]");
    string_t *TeX = quantity_to_string(acceleration, style_LATEX);
    TEST_ASSERT_TRUE(TeX && strstr(string_c_str(TeX), "\\mathrm{m}/\\mathrm{s}^{2}"), "unit-local TeX exponent");
    string_free(TeX);
    quantity_free(acceleration);
}

/* README example: exact speed conversion and independent ownership. */
static void example_quantity_speed(void)
{
    string_t *text = string_new_with("36 [km/h]");
    string_t *unit = string_new_with("m/s");
    quantity_t *speed = quantity_from_string(text);
    quantity_t *converted = quantity_convert(speed, unit);
    string_t *output = quantity_to_string(converted, style_FUNCTION);
    string_printf("%S\n", output);
    TEST_ASSERT_STR_EQ(string_c_str(output), "10 [m/s]");
    string_free(output);
    quantity_free(converted);
    quantity_free(speed);
    string_free(unit);
    string_free(text);
}

/* README example: an affine Fahrenheit point plus a Fahrenheit interval. */
static void example_quantity_temperature(void)
{
    string_t *point_text = string_new_with("32 [degF]");
    string_t *step_text = string_new_with("difference(18 [degF])");
    string_t *unit = string_new_with("degC");
    quantity_t *point = quantity_from_string(point_text);
    quantity_t *step = quantity_from_string(step_text);
    quantity_t *warmer = quantity_add(point, step);
    quantity_t *celsius = quantity_convert(warmer, unit);
    string_t *output = quantity_to_string(celsius, style_FUNCTION);
    string_printf("%S\n", output);
    TEST_ASSERT_STR_EQ(string_c_str(output), "10 [degC]");
    string_free(output);
    quantity_free(celsius);
    quantity_free(warmer);
    quantity_free(step);
    quantity_free(point);
    string_free(unit);
    string_free(step_text);
    string_free(point_text);
}

/* README example: exact conversion between angular units without evaluating pi. */
static void example_quantity_angles(void)
{
    string_t *text = string_new_with("1 [degrees]");
    string_t *unit = string_new_with("arc seconds");
    quantity_t *angle = quantity_from_string(text);
    quantity_t *converted = quantity_convert(angle, unit);
    string_t *output = quantity_to_string(converted, style_FUNCTION);
    string_printf("%S\n", output);
    TEST_ASSERT_STR_EQ(string_c_str(output), "3600 [arcsec]");
    string_free(output);
    quantity_free(converted);
    quantity_free(angle);
    string_free(unit);
    string_free(text);
}

/* README example: gigaelectronvolts and electronvolts retain exact SI-prefix scaling. */
static void example_quantity_energy(void)
{
    string_t *text = string_new_with("2 [giga electron volts]");
    string_t *unit = string_new_with("eV");
    quantity_t *energy = quantity_from_string(text);
    quantity_t *converted = quantity_convert(energy, unit);
    string_t *output = quantity_to_string(converted, style_FUNCTION);
    string_printf("%S\n", output);
    TEST_ASSERT_STR_EQ(string_c_str(output), "2000000000 [eV]");
    string_free(output);
    quantity_free(converted);
    quantity_free(energy);
    string_free(unit);
    string_free(text);
}

/* README example: immortal SI constants and conversions at the selected precision. */
static void example_quantity_constants_precision(void)
{
    size_t saved = num_get_default_prec_bits();
    string_t *speed_unit = string_new_with("m/s");
    string_t *time_unit = string_new_with("s");
    string_t *length_unit = string_new_with("ly");
    quantity_t *speed = quantity_new(NUM_SPEED_OF_LIGHT, speed_unit);
    quantity_t *time = quantity_new(NUM_JULIAN_YEAR, time_unit);
    quantity_t *distance = quantity_mul(speed, time);
    quantity_t *years = quantity_convert(distance, length_unit);
    string_t *output = quantity_to_string(years, style_FUNCTION);
    string_printf("%S\n", output);
    TEST_ASSERT_STR_EQ(string_c_str(output), "1 [ly]");
    string_t *angle_text = string_new_with("1 [deg]");
    string_t *angle_unit = string_new_with("rad");
    quantity_t *angle = quantity_from_string(angle_text);
    const size_t precisions[] = {53, 256};
    for (size_t i = 0; i < 2; ++i) {
        num_set_default_prec_bits(precisions[i]);
        quantity_t *radians = quantity_convert(angle, angle_unit);
        number_t value = quantity_value(radians);
        printf("radians: %zu bits\n", num_get_prec_bits(value));
        TEST_ASSERT_TRUE(num_get_prec_bits(value) == precisions[i], "selected precision");
        num_destroy(&value);
        quantity_free(radians);
    }
    num_set_default_prec_bits(saved);
    quantity_free(angle);
    string_free(angle_unit);
    string_free(angle_text);
    string_free(output);
    quantity_free(years);
    quantity_free(distance);
    quantity_free(time);
    quantity_free(speed);
    string_free(length_unit);
    string_free(time_unit);
    string_free(speed_unit);
}

/* README example: distinguish metric tonnes, UK long tons and US short tons. */
static void example_quantity_tons(void)
{
    static const struct {
        const char *source;
        const char *unit;
        const char *expected;
    } cases[] = {
        {"1 [tonne]", "kg", "1000 [kg]"},
        {"1 [UK ton]", "lb", "2240 [lb]"},
        {"1 [US ton]", "lb", "2000 [lb]"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        string_t *input = string_new_with(cases[i].source);
        string_t *unit = string_new_with(cases[i].unit);
        quantity_t *mass = quantity_from_string(input);
        quantity_t *converted = quantity_convert(mass, unit);
        string_t *output = quantity_to_string(converted, style_FUNCTION);
        string_printf("%S\n", output);
        TEST_ASSERT_STR_EQ(string_c_str(output), cases[i].expected);
        string_free(output);
        quantity_free(converted);
        quantity_free(mass);
        string_free(unit);
        string_free(input);
    }
}

/* README example: chained division combines exact acceleration unit factors. */
static void example_quantity_acceleration(void)
{
    string_t *input = string_new_with("1 [mi/hr/hr]");
    string_t *unit = string_new_with("m/s^2");
    quantity_t *acceleration = quantity_from_string(input);
    quantity_t *converted = quantity_convert(acceleration, unit);
    number_t value = quantity_value(converted);
    number_t numerator = {0}, denominator = {0};
    bool exact = num_get_fraction_parts(value, &numerator, &denominator);
    TEST_ASSERT_TRUE(exact, "exact acceleration conversion");
    string_t *top = num_to_string(numerator), *bottom = num_to_string(denominator);
    string_t *output = string_sprintf("%S/%S [%S]", top, bottom, quantity_unit(converted));
    string_printf("%S\n", output);
    TEST_ASSERT_STR_EQ(string_c_str(output), "1397/11250000 [m/s^2]");
    string_free(output);
    string_free(bottom);
    string_free(top);
    num_destroy(&denominator);
    num_destroy(&numerator);
    num_destroy(&value);
    quantity_free(converted);
    quantity_free(acceleration);
    string_free(unit);
    string_free(input);
}

int tests_main(void)
{
    TEST_RUN_CASE(test_quantity_aliases, NULL);
    TEST_RUN_CASE(test_quantity_exact_conversions, NULL);
    TEST_RUN_CASE(test_quantity_temperature, NULL);
    TEST_RUN_CASE(test_quantity_arithmetic, NULL);
    TEST_RUN_CASE(test_quantity_precision, NULL);
    TEST_RUN_CASE(test_quantity_selected_precision, NULL);
    TEST_RUN_CASE(test_quantity_functions_complex, NULL);
    TEST_RUN_CASE(test_quantity_parser_and_ownership, NULL);
    TEST_RUN_CASE(test_quantity_text, NULL);
    TEST_SECTION("README examples");
    TEST_RUN_OUTPUT(example_quantity_speed);
    TEST_RUN_OUTPUT(example_quantity_temperature);
    TEST_RUN_OUTPUT(example_quantity_angles);
    TEST_RUN_OUTPUT(example_quantity_energy);
    TEST_RUN_OUTPUT(example_quantity_constants_precision);
    TEST_RUN_OUTPUT(example_quantity_tons);
    TEST_RUN_OUTPUT(example_quantity_acceleration);
    return TEST_EXIT_CODE();
}
