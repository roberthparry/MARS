# Physical quantities

Include `quantity-t.h` for the opaque `quantity_t` class. A quantity owns a
`number_t` scalar, a unit and rational physical dimensions. It is immutable;
operations return independent objects, and never consume their inputs.

This is a native library module. It does not yet add unit-bearing expressions to
the MARS language or Lab interface, nor does it model measurement uncertainty.

## Ownership and API

| API | Behaviour |
| --- | --- |
| `quantity_new` | Copy a finite scalar and unit; ordinary K, degC and degF mean temperature readings. |
| `quantity_new_difference` | Construct a temperature difference without applying an offset; reject other dimensions. |
| `quantity_new_point` | Copy a real scalar as a temperature coordinate in K, degC or degF. |
| `quantity_from_string` | Parse a scalar followed by a bracketed unit; ordinary temperatures are readings; difference(...) marks a temperature interval. |
| `quantity_clone`, `quantity_free` | Independent copy and destruction; freeing NULL is harmless. |
| `quantity_value` | Return an owned scalar in the chosen unit; release with `num_destroy`. |
| `quantity_unit` | Borrow the canonical alias or normalised compound spelling. |
| `quantity_is_point` | Distinguish an affine temperature point from a difference. |
| `quantity_compatible` | Compare physical dimensions, not point/difference kinds. |
| `quantity_convert`, `quantity_to_si` | Convert compatible units or use coherent SI base units. |
| `quantity_add`, `quantity_sub` | Dimension-checked addition/subtraction, normally retaining the left unit. |
| `quantity_mul`, `quantity_div` | Multiply/divide linear quantities; return coherent SI units. |
| `quantity_neg`, `quantity_scale` | Negate or multiply by a scalar, retaining the unit. |
| `quantity_pow_int`, `quantity_root` | Integer powers and positive-degree principal roots, with rational dimensions. |
| `quantity_compare` | Order compatible real quantities of the same point/difference kind. |
| `quantity_sin`, `quantity_cos`, `quantity_tan` | Convert angles to radians, or accept dimensionless arguments; return dimensionless results. |
| `quantity_exp`, `quantity_log` | Require dimensionless arguments; logarithms use the principal branch. |
| `quantity_to_string` | Return owned text; release with `string_free`. |

Inputs are borrowed for each call. No user-defined registry, global conversion
cache or additional library dependency is introduced. Concurrent access follows
the [number module's](number.md) precision and threading rules; callers must not
free an object while another call uses it.

Failures return NULL or false and set `errno`: `EINVAL` for invalid arguments,
unrecognised units or malformed text; `EDOM` for incompatible dimensions,
non-finite scalars, zero divisors or invalid affine operations; `EOVERFLOW` for
parser/exponent limits; and `ENOMEM` for allocation failure. A false
`quantity_compatible` result normally just means different dimensions.
`quantity_value(NULL)` returns an invalid number and sets `EINVAL`.

## Units and conventions

Symbols are case-sensitive. The table lists canonical spellings; common English
singular/plural names are also recognised. No automatic SI-prefix expansion or
locale-dependent case folding occurs.

| Kind | Canonical units and selected aliases |
| --- | --- |
| Length | m, cm, mm, km; in (inches), ft (feet), yd (yards), mi (miles), fur (furlongs), lea (leagues) |
| Astronomical length | au (astronomical units, AU), ly (light years), pc (parsecs) |
| Mass | kg, g, lb (pounds, lbs), st (stone), oz (avoirdupois ounces), oz_t (troy oz, ozt) |
| Large masses | t (tonne, tonnes, metric tons), uk_ton (UK tons, long tons, imperial tons), us_ton (US tons, short tons) |
| Volume | L (litres), mL; uk_fl_oz, us_fl_oz, uk_pt, us_pt, uk_gal, us_gal |
| Time | s (seconds), min (minutes), h (hr, hrs, hour, hours), d; ms, us (also µs and μs), ns, ps |
| Temperature | K, degC (°C, deg C), degF (°F, deg F), delta_degC (Δ°C), delta_degF (Δ°F) |
| Angle | rad (radians), deg (degrees, °), arcmin (arc minutes, ′), arcsec (arc seconds, ″) |
| Force and energy | N (newtons), lbf (pound-force), J (joules), ft_lbf (foot pounds) |
| Particle energy | eV (electron volts), GeV (giga electron volts) |
| Other SI units | A, mol, cd, Hz, W, Pa |
| Dimensionless | 1, percent (%) |

Qualified volume names include “uk pint”, “us pint”, “uk gallon”, “us gallon”,
“uk fluid oz” and “us fluid oz”, with uppercase UK/US variants. Unqualified
“pint”, “gallon” and “fluid oz” are rejected rather than silently choosing a
system. US volumes are **liquid**, not dry, measures.

A tonne (t, also called a metric ton) is exactly 1000 kg. A UK long ton is
exactly 2240 lb (1016.0469088 kg), and a US short ton is exactly 2000 lb
(907.18474 kg). These are mass units, not ton-force or shipping-volume measures.
Bare `ton` and `tons` are rejected as ambiguous; choose `tonne`, `UK ton`
or `US ton`. Singular/plural, lowercase uk/us and long-ton/short-ton spellings
are recognised. The definitions use the exact international pound, not rounded
conversion-table decimals. See the [NIST ton definitions](https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8)
and [tonne definition](https://www.nist.gov/pml/special-publication-811/nist-guide-si-chapter-5-units-outside-si).

A pound is mass; pound-force is force. “Foot pounds” means foot-pound-force
energy, whereas ft*lb is mass times length and cannot be converted to joules.
A league means three international miles, not a nautical or historical league.
Feet are international feet, not US survey feet.

Definitions use exact rational factors, including the international inch,
avoirdupois pound, troy ounce, imperial gallon and US liquid gallon. Pound-force
uses the conventional standard gravitational acceleration, exactly
9.80665 m/s². The UK pint is one eighth of its gallon and contains 20 UK fluid
ounces; the US liquid pint is one eighth of its gallon and contains 16 US fluid
ounces. Sources: [NIST unit tables](https://www.nist.gov/pml/special-publication-811/nist-guide-si-appendix-b-conversion-factors/nist-guide-si-appendix-b8)
and [NIST international length definitions](https://www.nist.gov/pml/us-surveyfoot/revised-unit-conversion-factors).
The implementation derives exact products and ratios rather than copying
rounded printed conversion factors.

Electronvolts use the exact elementary-charge definition: 1 eV equals
1.602176634 × 10⁻¹⁹ J, and 1 GeV equals 10⁹ eV (1.602176634 × 10⁻¹⁰ J).
Both factors are stored as exact rationals, not rounded decimal numbers.
Their integer numerators and denominators are kept separately so unit factors
can cancel before conversion. Numerical results still follow the selected
`number_t` precision and rational-size budget; increasing precision later does
not require recreating the unit definition.
See the [NIST electronvolt–joule relationship](https://physics.nist.gov/cgi-bin/cuu/Value?Revj).
The spellings electronvolt/electronvolts, electron volt/electron volts and
electron-volt/electron-volts are recognised; GeV also accepts gigaelectronvolt(s),
giga electronvolt(s), giga electron volt(s) and giga-electronvolt(s).
Symbols remain case-sensitive: eV and GeV, not ev or gev.

An astronomical unit is exactly 149597870700 m. A light-year uses the Julian
year of 365.25 days and the exact speed of light, 299792458 m/s; its length is
9460730472580800 m. A parsec is exactly 648000/π au, rather than the older
tangent construction. See the [IAU astronomical-unit explanation](https://iauarchive.eso.org/public/themes/measuring/)
and [IAU 2015 Resolution B2](https://arxiv.org/abs/1510.06262).

Angle factors retain symbolic powers of π. Degrees-to-arcseconds conversion is
therefore exact, while degrees-to-radians and parsecs-to-metres realise π using
the selected `num_get_default_prec_bits()` setting. Constructors, conversion
and arithmetic results, `quantity_value()` and all output styles honour that
setting, even when an input was created at higher precision. Exact integers
and rationals within the numerical size budget remain exact. Input objects are
not modified when a result or a displayed copy is rounded.

There are no double-precision conversion intermediates. Raising precision cannot
recover digits already rounded in an ordinary stored scalar; recompute from the
original input and exact unit definition. Use rational input for exact fractions.
These rules also apply to complex scalars, irrational roots and transcendental
functions.

## Dimensions, temperature and parsing

The seven SI base dimensions are length, mass, time, electric current,
temperature, amount of substance and luminous intensity. An eighth **angle**
dimension keeps accidental metre/radian and angle/dimensionless conversions
invalid. Trigonometric calls explicitly permit angles.

Addition requires compatible dimensions. Products combine dimensions without
guessing whether an energy-shaped result represents energy or torque. Roots
retain fractional exponents; they do not discard units.

Temperature differences are linear: `difference(18 [degF])` converts to `difference(10 [K])`. Temperature
readings are affine: `32 [degF]` converts to `0 [degC]` or `273.15 [K]`. The exact
temperature definitions follow [NIST](https://www.nist.gov/pml/owm/si-units-temperature).

- Point minus point produces a K difference.
- Point plus/minus difference produces a point in the point's unit.
- Difference plus point also produces a point.
- Point plus point, difference minus point, point negation, multiplication and
  powers are rejected. Converting a point to K does not silently linearise it.
- This is coordinate arithmetic, not a thermodynamic state validator; it does
  not impose an absolute-zero bound.

The parser accepts `number [unit]` and `difference(number [unit])`. Numbers use
`num_create_from_text` syntax, including rationals and complex scalars; points
must be real. Compound units use explicit *, /, parentheses and integer or
rational powers such as m^(1/2). Multiplication and division associate left to
right; group a compound denominator explicitly. Spaces are allowed around
operators, and registered multiword names also work inside compounds.
Celsius/Fahrenheit point spellings cannot appear inside compounds: use K or
delta_degC/delta_degF for rates and products.

Ordinary temperatures need no marker: `20 [degC]` is a reading, whereas
`difference(20 [degC])` is an interval. Explicit delta unit aliases such as
`20 [delta_degC]` also denote differences. Formatting preserves that distinction
without adding any marker to readings. The old @ prefix and point(...) text
are not accepted. `quantity_new_difference()` provides the corresponding C
constructor; `quantity_new_point()` remains available when an API caller wants
to require a temperature reading and reject non-temperature units.

Bounds are 64 KiB for the complete quantity text, 1024 bytes for the unit text,
16 nested groups, and magnitude 32767 for reduced exponent numerators and
positive denominators. Root degree and integer-power magnitude have the same
bound. Input must be valid UTF-8 without embedded NUL.

Unit aliases use a checked two-level perfect hash: 61 buckets and 317 slots for
260 spellings. Conversions dispatch between linear and affine operations.
Fixed-size dimension loops do not scan the unit registry.

## Formatting

`style_FUNCTION` emits parseable quantity notation, using difference(...) only for temperature differences.
It is **not** a function declaration for the MARS expression language.
`style_EXPRESSION` and `style_UNBOUND` use native mathematical scalar
formatting, still followed by bracketed units. `style_LATEX` uses native
mathematical scalars and upright, escaped units, with an explicit difference label only for temperature differences.
All styles describe the same scalar and units; TeX is presentation, not an
input format.

## Exact speed conversion

```c
#include <string.h>

#include "quantity-t.h"

int main(void)
{
    string_t *text = string_new_with("36 [km/h]");
    string_t *unit = string_new_with("m/s");
    quantity_t *speed = quantity_from_string(text);
    quantity_t *converted = quantity_convert(speed, unit);
    string_t *output = quantity_to_string(converted, style_FUNCTION);
    string_printf("%S\n", output);
    if (!output || strcmp(string_c_str(output), "10 [m/s]"))
        return 1;
    string_free(output);
    quantity_free(converted);
    quantity_free(speed);
    string_free(unit);
    string_free(text);
    return 0;
}
```

Output:

```text
10 [m/s]
```

## Temperature point and difference

```c
#include <string.h>

#include "quantity-t.h"

int main(void)
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
    if (!output || strcmp(string_c_str(output), "10 [degC]"))
        return 1;
    string_free(output);
    quantity_free(celsius);
    quantity_free(warmer);
    quantity_free(step);
    quantity_free(point);
    string_free(unit);
    string_free(step_text);
    string_free(point_text);
    return 0;
}
```

Output:

```text
10 [degC]
```

## Angular conversion

```c
#include <string.h>

#include "quantity-t.h"

int main(void)
{
    string_t *text = string_new_with("1 [degrees]");
    string_t *unit = string_new_with("arc seconds");
    quantity_t *angle = quantity_from_string(text);
    quantity_t *converted = quantity_convert(angle, unit);
    string_t *output = quantity_to_string(converted, style_FUNCTION);
    string_printf("%S\n", output);
    if (!output || strcmp(string_c_str(output), "3600 [arcsec]"))
        return 1;
    string_free(output);
    quantity_free(converted);
    quantity_free(angle);
    string_free(unit);
    string_free(text);
    return 0;
}
```

Output:

```text
3600 [arcsec]
```

## Electronvolt conversion

```c
#include <string.h>

#include "quantity-t.h"

int main(void)
{
    string_t *text = string_new_with("2 [giga electron volts]");
    string_t *unit = string_new_with("eV");
    quantity_t *energy = quantity_from_string(text);
    quantity_t *converted = quantity_convert(energy, unit);
    string_t *output = quantity_to_string(converted, style_FUNCTION);
    string_printf("%S\n", output);
    if (!output || strcmp(string_c_str(output), "2000000000 [eV]"))
        return 1;
    string_free(output);
    quantity_free(converted);
    quantity_free(energy);
    string_free(unit);
    string_free(text);
    return 0;
}
```

Output:

```text
2000000000 [eV]
```

These examples also run last in `tests/quantity/test_quantity.c`. Run
`make -j1 test_quantity`, followed by `make -j1 test-readme-examples`.

## Shared constants and selected precision

Conversions use the [physical immortal constants](number.md#physical-immortal-constants)
declared in `number-t.h`: `NUM_ELEMENTARY_CHARGE` for eV/GeV,
`NUM_SPEED_OF_LIGHT` and `NUM_JULIAN_YEAR` for light-years,
`NUM_ASTRONOMICAL_UNIT` for au/pc and `NUM_STANDARD_GRAVITY` for lbf/ft_lbf.
Their exact integer components are used before any rounding, so inverse units
and scale ratios can cancel even at double precision. Planck, Boltzmann and
Avogadro constants are also available to construct unit-bearing quantities.
These are scalar constants: attach their documented SI units explicitly.

This example calculates one light-year, then converts the same angle at two
precision settings. No units or constants need to be recreated when precision changes.

```c
#include <stdio.h>
#include <string.h>

#include "quantity-t.h"

int main(void)
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
    if (!output || strcmp(string_c_str(output), "1 [ly]"))
        return 1;
    string_t *angle_text = string_new_with("1 [deg]");
    string_t *angle_unit = string_new_with("rad");
    quantity_t *angle = quantity_from_string(angle_text);
    const size_t precisions[] = {53, 256};
    for (size_t i = 0; i < 2; ++i) {
        num_set_default_prec_bits(precisions[i]);
        quantity_t *radians = quantity_convert(angle, angle_unit);
        number_t value = quantity_value(radians);
        printf("radians: %zu bits\n", num_get_prec_bits(value));
        if (num_get_prec_bits(value) != precisions[i])
            return 1;
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
    return 0;
}
```

Output:

```text
1 [ly]
radians: 53 bits
radians: 256 bits
```

## Tonnes and qualified tons

This example keeps the three mass definitions distinct.

```c
#include <string.h>

#include "quantity-t.h"

int main(void)
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
        if (!output || strcmp(string_c_str(output), cases[i].expected))
            return 1;
        string_free(output);
        quantity_free(converted);
        quantity_free(mass);
        string_free(unit);
        string_free(input);
    }
    return 0;
}
```

Output:

```text
1000 [kg]
2240 [lb]
2000 [lb]
```

## Compound acceleration conversion

`mi/hr/hr` means `(mi/hr)/hr`, the same as `mi/hr^2` or `mi/(hr*hr)`.
The aliases `h`, `hr`, `hrs`, `hour` and `hours` all use 3600 seconds.
One international mile is exactly 1609.344 metres, so the exact conversion is
1 mi/hr/hr = 1397/11250000 m/s², approximately 0.000124177777777778 m/s².

Unit factors and dimension exponents are combined in
`src/quantity/quantity_units.c`. Its `quantity_convert_value()` function uses
a function-pointer table to dispatch to `quantity_linear()` or
`quantity_affine()`. Linear conversion applies the source/target scale ratio;
ordinary temperatures additionally apply origin offsets. Temperature differences
use linear conversion. There is no per-unit-pair switch or converter.

This example retrieves the exact rational result rather than rounding its display.

```c
#include <string.h>

#include "quantity-t.h"

int main(void)
{
    string_t *input = string_new_with("1 [mi/hr/hr]");
    string_t *unit = string_new_with("m/s^2");
    quantity_t *acceleration = quantity_from_string(input);
    quantity_t *converted = quantity_convert(acceleration, unit);
    number_t value = quantity_value(converted);
    number_t numerator = {0}, denominator = {0};
    bool exact = num_get_fraction_parts(value, &numerator, &denominator);
    if (!exact)
        return 1;
    string_t *top = num_to_string(numerator), *bottom = num_to_string(denominator);
    string_t *output = string_sprintf("%S/%S [%S]", top, bottom, quantity_unit(converted));
    string_printf("%S\n", output);
    if (!output || strcmp(string_c_str(output), "1397/11250000 [m/s^2]"))
        return 1;
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
    return 0;
}
```

Output:

```text
1397/11250000 [m/s^2]
```
