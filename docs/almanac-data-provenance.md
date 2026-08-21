# Almanac data provenance

This record identifies the origin and transformation of the astronomical data
distributed with MARS. It distinguishes Robert H. Parry's AstroNav workbook
from third-party scientific inputs and generation tools.

## AstroNav workbook

[`AstroNav 2000-2040.ods`](../src/almanac/AstroNav%202000-2040.ods) is the
personal creation of Robert H. Parry. It is original MARS material distributed
under the repository's [MIT Licence](../LICENSE), rather than a third-party
spreadsheet. Robert H. Parry first assembled it in Microsoft Excel before the
year 2000, subsequently converted it to an OpenOffice document, and later
maintained it as a LibreOffice document. More recently, he extended it to
improve its accuracy for the 2000–2040 period. He confirmed this history,
authorship and ownership on 21 August 2026 for the purpose of this provenance
record. Its MARS repository history begins at commit
`1f700a4295200d3651d3bae8cfff052c9a7e357d` on 29 June 2026.

The workbook includes calculated astronomical data. In particular, its Moon
state data was generated from the short DE440 kernel described below, with
SPICE light-time and stellar-aberration correction (`LT+S`) and ERFA's IAU 2006
precession and IAU 2000A nutation transformation. The workbook itself states
that it is an aid for study and planning and is not an official nautical
almanac, chart or safety instrument.

## Packaged ephemeris coefficients

The almanac database includes position-only Chebyshev coefficients covering
1550-01-01 through the end of 2649. The source was JPL Development Ephemeris
DE440, published by the Jet Propulsion Laboratory, California Institute of
Technology. The generation used unmodified kernels downloaded from NASA's
Navigation and Ancillary Information Facility (NAIF):

| Input | Version and official source | Published checksum |
|---|---|---|
| `de440.bsp` | [DE440 full kernel, 21 December 2020](https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440.bsp) | MD5 `c9d581bfd84209dbeee8b1583939b148` |
| `de440s.bsp` | [DE440 short kernel, 21 December 2020](https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440s.bsp) | MD5 `3917ee56769db332790c751e2168843d` |
| `naif0012.tls` | [NAIF leap-seconds kernel, revision 14 July 2016](https://naif.jpl.nasa.gov/pub/naif/generic_kernels/lsk/naif0012.tls) | Identified by the immutable filename and revision recorded inside the kernel |
| `pck00011.tpc` | [NAIF planetary constants kernel, 27 December 2022](https://naif.jpl.nasa.gov/pub/naif/generic_kernels/pck/pck00011.tpc) | Identified by the immutable filename and creation date recorded inside the kernel |

NAIF's [published checksum list](https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/aa_checksums.txt)
is authoritative for the two DE440 binary kernels. MARS does not redistribute
the kernels or the SPICE Toolkit. The packaged files contain fitted numerical
coefficients derived from calculations performed with those inputs.

The position generator is preserved in repository history at commit
`1f700a4295200d3651d3bae8cfff052c9a7e357d` as
`tools/generate_almanac_chebyshev_sql.py`. It used NumPy and SpiceyPy to:

1. convert Julian TDB dates to SPICE ephemeris time;
2. obtain geometric (`NONE`) ECLIPJ2000 positions from DE440;
3. convert kilometres to astronomical units using
   `149597870.7 km/AU`;
4. fit each Cartesian component by least-squares Chebyshev interpolation; and
5. store component-major, little-endian IEEE 754 binary64 coefficients.

The fitted bodies, centres, segment spans and polynomial degrees are:

| Body | Centre | Span | Degree |
|---|---|---:|---:|
| Earth barycentre | Sun | 16 days | 12 |
| Moon | Earth | 8 days | 9 |
| Mercury barycentre | Sun | 8 days | 13 |
| Venus barycentre | Sun | 16 days | 9 |
| Mars barycentre | Sun | 32 days | 10 |
| Jupiter barycentre | Sun | 32 days | 7 |
| Saturn barycentre | Sun | 32 days | 6 |

Each fit used at least degree plus eight samples and was checked at least
degree plus twelve validation points. The generated data first appeared in
commit `fd952ecb3c7e2109f2fd4018c167abc1c78ee370`; commit
`38261c2` changed its SQL schema representation without changing the coefficient
blob stream.

## Frame rotations and ERFA

The frame-rotation generator is preserved in the same historical commit as
`tools/generate_almanac_frame_rotation_sql.py`. It used NumPy and PyERFA, whose
`erfa` module contains the independently maintained
[Essential Routines for Fundamental Astronomy](https://github.com/liberfa/erfa)
(ERFA), distributed under the BSD 3-Clause Licence.

For each date, the generator called `erfa.pnm06a()` for the IAU 2006
precession and IAU 2000A nutation matrix, then multiplied it by the fixed J2000
mean-obliquity rotation of 84,381.448 arcseconds. It fitted all nine matrix
components in 365.25-day, degree-12 Chebyshev segments and stored little-endian
IEEE 754 binary64 coefficients. The resulting transformation is from
ECLIPJ2000 to the true equator and equinox of date.

NumPy, SpiceyPy and PyERFA were generation-time tools, not MARS runtime
dependencies. Their exact installed package versions were not retained by the
historical generation environment. The algorithms, generator revision, input
kernel identities and generated-output checksums are retained here. Any future
regeneration must additionally record exact tool versions and SHA-256
checksums for every downloaded kernel before replacing the packaged data.

## Current file checksums

These SHA-256 values identify the data reviewed for this provenance record:

| Distributed file | SHA-256 |
|---|---|
| `packaging/almanac-db/mars_almanac_chebyshev.sql` | `e3b8168f71ade640f5676916c06c40af21f4597875a5e41dfebeb4ec5fac824f` |
| `packaging/almanac-db/mars_almanac_frame_rotation.sql` | `5885aa96b8cecda3fd79fb2a9db62567e6c7a66493aca913fb054319b074a22f` |
| `packaging/almanac-db/mars_almanac.sql` | `addc990c0e5aedcbd04c634cdd7bc91808d39f30886167241992311d7a0183a2` |
| `src/almanac/AstroNav 2000-2040.ods` | `0d985fcdb2726de02035d8104af7b9a53ff9f1a856d25755885a3760d832a8ea` |

The checksums must be updated whenever any listed file is intentionally
regenerated or edited.

## Attribution and use

DE440 was produced by JPL/Caltech. The SPICE system is implemented and
maintained by Caltech's Jet Propulsion Laboratory under contract to NASA and is
sponsored by NASA's Planetary Science Division. MARS follows the
[NAIF rules regarding use of SPICE](https://naif.jpl.nasa.gov/naif/rules.html)
and does not imply endorsement or support by NASA, JPL, Caltech, NAIF, ERFA or
the generation-tool authors.

Astronomical outputs are computational aids, not certified navigation or
safety products. Users must use authoritative publications and appropriate
professional procedures where safety or legal responsibility depends on the
result.
