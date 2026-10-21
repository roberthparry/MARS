# Third-party notices

MARS is distributed under the MIT Licence. Its own licence is in
[`LICENSE`](LICENSE). MARS uses system-supplied open-source libraries and tools,
and ships jurisdiction data generated with open-source calendar projects. The
machine-readable inventory is in [`DEPENDENCIES.spdx`](DEPENDENCIES.spdx).

MARS does not vendor the source or archives of the linked libraries listed
below. The MARS Lab build embeds required WASI libc maths, memory and string routines in its generated
WebAssembly asset. A distributor that bundles libraries or generated assets containing
them must also include the exact
licence material supplied with the versions it redistributes. The notices in
this file do not replace those obligations.

## GeoNames Sardinian town catalogue

The 54 Sardinian town records added to
`packaging/jurisdiction-db/mars_jurisdiction_towns.sql` on 19 October 2026
come from the [GeoNames cities5000 extract](https://download.geonames.org/export/dump/cities5000.zip),
under [Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0/).
Credit: GeoNames, <https://www.geonames.org/>. No endorsement is implied.
The selection uses country `IT` and administrative region `14` (Sardinia),
excluding the neighbourhood records Li Punti-San Giovanni (3205826),
Monte Rosello (12188754), and the southern namesake La Maddalena (2524463).
La Maddalena (3175217) is the island town. Names, WGS84 coordinates, DEM
elevations and `Europe/Rome` timezones are retained; records are converted to
the existing SQL catalogue format. DEM heights are approximate town-reference
heights, not measurements of an observer's exact position.

Seven Corsican towns were added from the same extract on 19 October 2026,
selecting country `FR` and administrative region `94`: Ajaccio (3038334),
Bastia (3034640), Biguglia (3032668), Borgo (3031537), Calvi (3029090),
Corte (3023506) and Porto-Vecchio (2985947). Their names, WGS84 coordinates,
DEM elevations and `Europe/Paris` timezones are retained under the same licence.

## GeoNames worldwide town supplement

`packaging/jurisdiction-db/mars_jurisdiction_world_towns.sql` adds 46,455 populated
places from the same GeoNames cities5000 snapshot downloaded on 19 October 2026,
under Creative Commons Attribution 4.0. Credit: GeoNames,
<https://www.geonames.org/>. No endorsement is implied.

The extracted `cities5000.txt` SHA-256 is
`1322b711ac9257fef8ef4031683e2904faa6455b3946761b26ee8514172bd853`.

The native generator `tools/mars_config/catalogue_towns.c` selects `PPL`, `PPLC`
and `PPLA` through `PPLA4` records, excluding neighbourhoods, abandoned places
and other feature types. Generation is capped at 50,000 records; the current
package additionally retains the four targeted Sunshine Coast records below.
National capitals (`PPLC`), first-level regional seats (`PPLA`) and populations
of at least 100,000 have first priority, followed by the cruising-region country
list in the generator, then descending population and ascending GeoNames ID.
This is not a coastline-distance selection or a complete harbour directory.
It retains WGS84 coordinates and approximate DEM
elevations, and only accepts countries and timezones supported by the packaged
catalogue. Existing names (including ASCII spellings) within 0.1 degree on both
axes are retained instead of adding another copy. This is a deduplication
heuristic, not a shoreline accuracy guarantee.

US and British first-level codes are matched to existing subdivision codes;
other additions use their country jurisdiction. Ambiguous names within a
jurisdiction receive their stable GeoNames ID as a suffix. These edits and SQL
conversion do not imply that the source is complete or current in real time.

## GeoNames Sunshine Coast additions

Mooloolaba (GeoNames 2157109) and Maroochydore (GeoNames 2207268) use the
[GeoNames cities5000 extract](https://download.geonames.org/export/dump/cities5000.zip)
downloaded on 21 October 2026, under Creative Commons Attribution 4.0.
Credit: GeoNames; no endorsement is implied. Extracted `cities5000.txt` SHA-256:
`5efdc64a870f000b283a50b25f7b4845c5b9851bdad32737e7bb5f5353fc6755`.
Both are `PPLX` urban sections and are deliberately included as targeted coastal
seeds despite that feature type's exclusion from general worldwide generation.
Their source coordinates and DEM elevations are retained, using the existing
Australia/Brisbane timezone code. Each appears in both `AU` and `AU-QLD`;
the existing supplement is unchanged, giving 50,004 packaged catalogue records.

## GeoNames Fishguard and Llanfairpwllgwyngyll additions

Fishguard (GeoNames 2649372; alternative name Abergwaun) uses the
[British extract](https://download.geonames.org/export/dump/GB.zip), downloaded
on 19 October 2026, under Creative Commons Attribution 4.0. Credit: GeoNames;
no endorsement is implied. Extracted `GB.txt` SHA-256:
`287806663dc4a6381610d4022461e45e1c88bb91cef9265c2890180e03e28e9b`.
The source coordinates, approximate DEM elevation and Europe/London timezone
are retained. The reference point represents the town, not a harbour entrance.

Llanfairpwllgwyngyll (GeoNames 2644080), also known as
Llanfairpwllgwyngyllgogerychwyrndrobwllllantysiliogogogoch, uses the same
British extract and licence. Its source coordinates, approximate DEM elevation
and Europe/London timezone are retained; the shorter source name is used for
the town catalogue.

## GeoNames Cape Peninsula additions

Sixteen targeted seed locations use the [GeoNames South Africa extract](https://download.geonames.org/export/dump/ZA.zip),
downloaded on 19 October 2026, under Creative Commons Attribution 4.0. Credit:
GeoNames; no endorsement is implied. Extracted `ZA.txt` SHA-256:
`0c1a225028f4ea4cfce7efa6e06dda1fb582251fb58a4f2dbf57c8ab6cbf5261`.

The selected IDs are Fish Hoek (7289301), Noordhoek (3363610), Camps Bay (3369169),
Hout Bay (3366639), Clovelly (3369083), Kalk Bay (3366314), Kommetjie (3365272),
Llandudno (3364518), Scarborough (3361769), Glencairn (3367640), Saint James
(3361940), Clifton (3369090), Sea Point (3361711), Bantry Bay (3370202), Bakoven
(3370234) and Mouille Point (3363913). Kalk Bay uses the source's English alias
for Kalkbaai. Coordinates and approximate DEM elevations are retained with the
Africa/Johannesburg timezone. This targeted selection includes populated-place
sections (`PPLX`) as well as populated places (`PPL`); it does not globally import
every suburb. Reference points and DEM elevations are not harbour entrance or
surveyed observer positions.

## NASA Blue Marble globe imagery

`tools/mars_lab/assets/earth_surface.jpg` is the unmodified 2048 × 1024 NASA
Blue Marble composite, *Land Surface, Ocean Color and Sea Ice*, published
8 February 2002. It is reference imagery, not current or selected-date ice coverage.

Source: [NASA original JPEG](https://eoimages.gsfc.nasa.gov/images/imagerecords/57000/57730/land_ocean_ice_2048.jpg).
Credit: NASA Goddard Space Flight Center; Reto Stöckli (surface imagery),
Robert Simmon (ocean colour and compositing), the MODIS land, atmosphere, ocean
and science-support groups, USGS EROS and Flagstaff, and the Defense
Meteorological Satellite Program. No NASA endorsement is implied.

The image was obtained directly from NASA, under its
[Earth imagery reuse policy](https://science.nasa.gov/earth/faq/), which permits
reuse, including commercial reuse, except where another copyright is indicated,
and requests NASA credit. NASA branding and endorsement restrictions remain
separate. The bundled file is unchanged; the Lab adds display-time lighting and
independent shoreline and town overlays.

SHA-256: `d4dc80a6ef571939d0abe04a9bed3d3d1e6cd63e59514be1c5e43a6b069e6f1e`.
Size: 266,599 bytes. No online map service is required at runtime.

The optional zoom asset, `tools/mars_lab/assets/earth_surface_detail.png`, is the
unmodified [8192 × 4096 NASA PNG](https://eoimages.gsfc.nasa.gov/images/imagerecords/57000/57730/land_ocean_ice_8192.png)
of the same composite, with the same credits, reuse terms and reference-date limitations.
Size: 24,147,269 bytes. SHA-256:
`aaddcd967a9f09fb2d7ef50ff452bebfcf10192c520465d5eb1ad8446c716e98`.
Lower-capability graphics hardware may receive a display-time reduced-resolution texture;
neither packaged source image is modified.

## WASI libc maths, memory and strings

The MARS Lab page module statically links the required double-precision maths
routines and memory/string operations from the system `wasi-libc` package. Its musl-derived maths resides in
`libc.a`; it does not require a WASI host runtime or JavaScript maths callbacks.
The codec worker links the memory operations but not the maths routines.

Retain the exact package's licence and component notices when distributing
`lab_browser.wasm` or `lab_codec.wasm`, even when the archive itself is not distributed. Debian and
Ubuntu provide these in `/usr/share/doc/wasi-libc/copyright`. The notices below
identify the principal maths components; they do not replace the selected
version's complete licence material.

Copyright 2005–2019 Rich Felker and other musl contributors.
Copyright 2017–2018 Arm Limited.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

Copyright 1993, 2004 Sun Microsystems, Inc.
Copyright 2008, 2009–2011 Bruce D. Evans, Steven G. Kargl and David Schultz.

Developed at SunPro, a Sun Microsystems, Inc. business.
Permission to use, copy, modify, and distribute this software is freely
granted, provided that this notice is preserved.

Upstream: <https://github.com/WebAssembly/wasi-libc> and
<https://git.musl-libc.org/cgit/musl/tree/COPYRIGHT>.

## SQLCipher Community Edition

MARS links to SQLCipher; the native configuration tool uses its library API to
create encrypted databases. SQLCipher incorporates SQLite and normally uses a separately
supplied cryptographic provider. SQLCipher is a registered trademark of
Zetetic, LLC. MARS is not affiliated with or endorsed by Zetetic.

Copyright (c) 2025, ZETETIC LLC
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of ZETETIC LLC nor the names of its contributors may be
   used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY ZETETIC LLC "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL ZETETIC LLC BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

Upstream licence information:
<https://www.zetetic.net/sqlcipher/license/>

## SQLite

SQLCipher is based on SQLite. SQLite source code is dedicated to the public
domain. The SQLite project publishes its copyright statement at
<https://sqlite.org/copyright.html>.

This product includes software developed by the SQLite Project.

## Cryptographic provider

The SQLCipher package installed by the operating system selects its
cryptographic provider. The normal Debian and Ubuntu build uses OpenSSL. OpenSSL
3 is licensed under the Apache License 2.0; older OpenSSL releases use the
OpenSSL and original SSLeay licences. A distributor must use the notice that
matches the provider and version it actually ships.

The native HTTP, WebSocket and gRPC test fixtures also link OpenSSL directly
for TLS connections. They are test executables, not part of libmars or the
public webserver module. Retain the applicable OpenSSL notices if distributing
these test binaries.

Copyright (c) The OpenSSL Project Authors. All rights reserved.

This product includes software developed by the OpenSSL Project for use in the
OpenSSL Toolkit.

The OpenSSL build verified for MARS also loads system zlib and Zstandard
libraries. They are not bundled by MARS. A self-contained distributor must
include the zlib licence and the selected BSD 3-Clause or GPL version 2 terms
for Zstandard.

OpenSSL licence information:
<https://openssl-library.org/source/license/index.html>

## File compression and encryption libraries

The MARS build also directly links the system Zstandard and libsodium libraries
for the file module's streaming compression and authenticated encryption APIs.
Neither library is bundled with MARS.

Zstandard is available under BSD 3-Clause or GPL version 2 terms. Its direct
build dependency is separate from the transitive OpenSSL dependency above.
Upstream licence files:
<https://github.com/facebook/zstd/blob/dev/LICENSE> and
<https://github.com/facebook/zstd/blob/dev/COPYING>.

Libsodium's principal licence is ISC; its distribution also contains components
with other notices. A distributor bundling it must preserve the licence and
component notices shipped with the exact library version, not only this summary.
The HTTP module also uses libsodium for authentication and random-data helpers.
Upstream licence information:
<https://github.com/jedisct1/libsodium/blob/master/LICENSE>.

## HTTP transport

MARS links the system libcurl library for HTTP/HTTPS and WebSocket transport,
including HTTP/2 for unary gRPC. Protocol Buffers encoding is native C. libcurl uses
the curl licence (SPDX identifier `curl`); the authoritative terms and
copyright notices are at <https://curl.se/docs/copyright.html>.
Distributors bundling libcurl must retain the licence supplied with that
version and account for its TLS, compression and other linked dependencies.
The recommended Linux development package uses OpenSSL; MARS does not vendor
libcurl or its dependencies.

MARS acknowledges Daniel Stenberg and the curl contributors for libcurl, and
Tatsuhiro Tsujikawa and the nghttp2 contributors for the HTTP/2 library used by
the verified libcurl build. nghttp2 is distributed under the MIT (Expat) licence.
It is a transitive dependency of that libcurl build, not a direct MARS link
dependency. Retain its copyright and permission notice when bundling it, using
the terms supplied with the exact version. Upstream licence information:
<https://github.com/nghttp2/nghttp2/blob/v1.68.0/COPYING>.

The MARS `webserver` module is an original Linux-socket implementation and adds
no third-party server library. Native XML and Protocol Buffers code likewise
do not introduce libxml2, libprotobuf or a separate gRPC runtime dependency.

## Multiprecision and Unicode libraries

MARS dynamically links to the following system libraries:

- GNU MP (GMP): GNU LGPL version 3 or later, or GNU GPL version 2 or later;
- GNU MPFR: GNU LGPL version 3 or later;
- GNU MPC: GNU LGPL version 3 or later; and
- GNU libunistring, when enabled: GNU LGPL version 3 or later, or GNU GPL
  version 2 or later, subject to the terms applying to the selected library
  components.

MARS also uses the platform C library, mathematics library and threading
implementation. On the supported GNU/Linux target these are normally supplied
by glibc under the GNU LGPL version 2.1 or later, with separately licensed
components. Consult the copyright files from the exact operating-system
packages being redistributed.

The native `thread_t` and `lock_t` modules use that existing POSIX threading
implementation, including monotonic condition-variable waits. They introduce
no additional third-party library. Browser codec workers instead use the
browser's Worker API and independently instantiated WebAssembly memory.

Upstream licence information:

- GMP: <https://gmplib.org/manual/Copying.html>
- MPFR: <https://www.mpfr.org/mpfr-current/mpfr.html#Copying>
- MPC: <https://www.multiprecision.org/mpc/>
- libunistring: <https://www.gnu.org/software/libunistring/>
- glibc: <https://www.gnu.org/software/libc/>

## Generated jurisdiction data

The bundled jurisdiction data identifies its sources within the database and
SQL source. Some generated holiday instances were produced using
python-holidays 0.99 and Workalendar 17.0.0, both distributed under the MIT
Licence. Their project notices and contribution histories remain the
authoritative source of authorship information:

- python-holidays: <https://github.com/vacanza/holidays>
- Workalendar: <https://github.com/workalendar/workalendar>

Their retained MIT notices are:

Copyright (c) Vacanza Team and individual contributors (see the upstream
`CONTRIBUTORS` file)

Copyright (c) dr-prodigy <dr.prodigy.github@gmail.com>, 2017-2023

Copyright (c) ryanss <ryanssdev@icloud.com>, 2014-2017

Copyright (c) 2013-2021 Novapost/PeopleDoc, 2021 Workalendar Maintainers.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

The country seed also uses IANA Time Zone Database `iso3166.tab` data. The IANA
Time Zone Database is in the public domain:
<https://www.iana.org/time-zones>.

The resulting MARS SQL data files retain source names, source URLs and source
document identifiers so that provenance is not lost when the encrypted
jurisdiction database is created.

### Unicode CLDR week data and calendar names

Territory weekend conventions use the Unicode Common Locale Data Repository
(CLDR) week-data model and are cross-checked against the calendar sources named
above. Localised calendar month and weekday names and long-date patterns in
`packaging/jurisdiction-db/mars_calendar_locale_names.sql` are extracted from
Unicode CLDR 46 through Babel 2.17.0, except for the explicitly hand-maintained
Ladino, Cajun French, North Slope Iñupiaq, Jamaican Patois and Pennsylvania Dutch supplements. Their common calendar
vocabulary and chosen abbreviations are not attributed to CLDR; Yiddish calendar
names do use CLDR. Sources for the supplementary vocabulary are identified in
the native generator's supplementary input and the jurisdiction guide; no dictionary prose is bundled.
Language names, official-language options
and territory-to-locale defaults use CLDR's
likely-subtag and territory-language data. The native `mars_config locales`
generator uses a pinned, resolved CLDR snapshot and separate curated supplements
under `tools/mars_config/data/`. Babel 2.17.0 was used to extract the original
snapshot; neither Babel nor Python is required to regenerate the SQL seed.
Hebrew month spellings in `packaging/jurisdiction-db/mars_calendar_local.sql`
also follow [CLDR's Hebrew locale data](https://github.com/unicode-org/cldr/blob/main/common/main/he.xml).
The CLDR 46 source is
[unicode-org/cldr, release-46](https://github.com/unicode-org/cldr/tree/release-46).
Babel's original extraction tooling is under the BSD 3-Clause licence;
no Babel code is bundled in the snapshot or seed. Unicode CLDR data files are distributed
under the Unicode Licence v3.
The required notice follows.

Copyright © 1991-2026 Unicode, Inc.

NOTICE TO USER: Carefully read the following legal agreement. BY DOWNLOADING,
INSTALLING, COPYING OR OTHERWISE USING DATA FILES, AND/OR SOFTWARE, YOU
UNEQUIVOCALLY ACCEPT, AND AGREE TO BE BOUND BY, ALL OF THE TERMS AND CONDITIONS
OF THIS AGREEMENT. IF YOU DO NOT AGREE, DO NOT DOWNLOAD, INSTALL, COPY,
DISTRIBUTE OR USE THE DATA FILES OR SOFTWARE.

Permission is hereby granted, free of charge, to any person obtaining a copy of
data files and any associated documentation (the "Data Files") or software and
any associated documentation (the "Software") to deal in the Data Files or
Software without restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, and/or sell copies of the Data Files
or Software, and to permit persons to whom the Data Files or Software are
furnished to do so, provided that either (a) this copyright and permission
notice appear with all copies of the Data Files or Software, or (b) this
copyright and permission notice appear in associated Documentation.

THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD
PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS
NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL
DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE DATA FILES OR
SOFTWARE.

Except as contained in this notice, the name of a copyright holder shall not be
used in advertising or otherwise to promote the sale, use or other dealings in
these Data Files or Software without prior written authorization of the
copyright holder.

Upstream licence information: <https://www.unicode.org/license.txt>

## Shoreline data

The packaged full-resolution GSHHG 2.3.7 shoreline binary is third-party data
distributed under GNU LGPL version 3 or later, not MARS's MIT licence. The
unmodified binary is retained as `packaging/almanac-db/gshhs_f.b.zst`; the native
installer derives compressed, indexed shoreline tables from it.

The original upstream licence, LGPL text and README, together with the GPL
version 3 text, are retained in `packaging/almanac-db/GSHHG-*.txt`. Retain these
notices and the corresponding source when distributing the dataset or databases
derived from it. Source URL, checksums and transformations are recorded in
[almanac data provenance](docs/almanac-data-provenance.md#shoreline-data).

## River data

The replacement centreline import uses the OpenStreetMap planet snapshot dated
12 October 2026, copyright OpenStreetMap contributors, under the Open Database
Licence (ODbL) 1.0. The staged extract retains its source data separately; the
derived SQLite geometry keeps OSM way identities and waterway classes. Attribution
and applicable ODbL obligations continue to apply to derived databases and their
redistribution. Centreline display simplification does not establish navigability.
See [OpenStreetMap copyright and licence](https://www.openstreetmap.org/copyright).

The full-resolution WDBII river network distributed with GSHHG 2.3.7 was formerly
packaged as `packaging/jurisdiction-db/wdb_rivers_f.b.zst`, but has been withdrawn
because its geometry is unsuitable for close views. The following provenance is
retained for older installations. It is third-party data under GNU LGPL version 3 or later,
not MARS's MIT licence. The upstream licence, README, LGPL and GPL texts in
`packaging/almanac-db/GSHHG-*.txt` also apply to this dataset and must accompany
redistribution. The native installer no longer imports this dataset.

Source: <https://github.com/GenericMappingTools/gshhg-gmt/releases/download/2.3.7/gshhg-bin-2.3.7.zip>.
Archive SHA-256: `28600e8f7a08645aab43079326df6504212ec5ccb2b4bcf3b5f4f12ed60e82bc`.
Uncompressed `wdb_rivers_f.b` SHA-256:
`d91eaa3b22f08535b93f3cb8afeff4e48a43ec33e8a3b798696147fc7e6d4a21`.

The source is a historical, generalised network, not a nautical chart. Source
feature identifiers and river classes are retained; names, surveyed widths,
depths and navigability are not supplied. Centreline geometry does not imply
that the surrounding land is water.

## Astronomical data and generation tools

The packaged almanac coefficients were derived from JPL Development Ephemeris
DE440 kernels published by the Navigation and Ancillary Information Facility
(NAIF) of NASA's Jet Propulsion Laboratory, California Institute of Technology.
The transformations also used the NAIF leap-seconds and planetary-constants
kernels. MARS does not redistribute those kernels or the SPICE Toolkit.

The coefficient-generation process used NumPy, SpiceyPy and PyERFA. NumPy and
PyERFA are distributed under the BSD 3-Clause Licence; SpiceyPy is distributed
under the MIT Licence. PyERFA contains the independently maintained ERFA
implementation of the IAU's Standards of Fundamental Astronomy routines under
the BSD 3-Clause Licence. These historical generation tools remain recorded
for data provenance; they are not dependencies of the maintained native C
tooling or the MARS runtime.

The source URLs, versions, transformations and checksums are recorded in
[`docs/almanac-data-provenance.md`](docs/almanac-data-provenance.md). Use of
SPICE and NAIF resources is governed by the
[NAIF rules regarding use of SPICE](https://naif.jpl.nasa.gov/naif/rules.html).
MARS acknowledges NASA, JPL, Caltech and NAIF as the source and maintainers of
the SPICE resources and does not imply their endorsement or support.

`src/almanac/AstroNav 2000-2040.ods` is the personal creation of Robert H.
Parry and is original MARS material, not a third-party spreadsheet. He first
assembled it in Microsoft Excel before 2000, subsequently converted it to an
OpenOffice document, later maintained it as a LibreOffice document, and more
recently extended it to improve its accuracy for the 2000–2040 period.

The MARS Lab icons, icon concepts and documentation screenshots are original
MARS material rather than third-party assets. Their scope and authorship are
recorded in
[`docs/visual-asset-provenance.md`](docs/visual-asset-provenance.md).

## WeatherAPI.com

WeatherAPI.com is an optional hosted service, not a library or data set shipped
by MARS. Its service and returned data are governed by the
[WeatherAPI terms](https://www.weatherapi.com/terms.aspx) and
[privacy policy](https://www.weatherapi.com/privacy.aspx). MARS supplies no
shared WeatherAPI account or key. Weather remains disabled unless the person
installing MARS Lab creates their own WeatherAPI account and configures that
account's key. MARS Lab credits WeatherAPI.com as the source whenever it
displays returned weather data. It retains only a bounded local cache of
validated current temperatures for up to 48 hours; complete responses,
forecast arrays, pressure readings and API keys are not cached.

Each lookup transmits the configured API key, selected date, latitude and
longitude from the local MARS Lab server to WeatherAPI.com over HTTPS. The key
is not sent to the browser. It must remain confidential and must not be
committed to a public repository or embedded in client-side code. The date and
coordinates remain in private local Lab state so that its inputs can be
restored. MARS Lab displays the required end-user warning:
weather information is general and probabilistic, may be inaccurate for the
exact location or time, and must not be used as the sole basis for personal
safety, aviation, marine navigation, emergency planning or another
safety-critical decision. Users should consult official meteorological
services and relevant authorities where accuracy is critical.

## Separately installed tools

### LLVM, Clang and LLD

MARS acknowledges the LLVM project contributors for the LLVM compiler
infrastructure, Clang C compiler and LLD linker. The Lab build uses Clang's
WebAssembly backend and LLD's `wasm-ld` to produce its freestanding page and
codec-worker modules. These are separately installed build tools, not linked MARS runtime
libraries; MARS does not bundle their source or executables.

The project's principal licence is Apache License 2.0 with LLVM exceptions
(`Apache-2.0 WITH LLVM-exception`). Legacy and third-party components may have
additional terms. If redistributing the toolchain, retain the licence and
component notices supplied with the exact packages, rather than relying only
on this summary. See the [LLVM licence](https://github.com/llvm/llvm-project/blob/main/llvm/LICENSE.TXT)
and [licensing policy](https://llvm.org/docs/DeveloperPolicy.html#license).

### Rendering and optional integration tools

MARS Lab invokes TeX Live, dvisvgm and, optionally, Tailscale and desktop
integration tools. These programs are installed separately and are not
incorporated into MARS. Their licences govern their own programs. TeX Live is a
collection with per-package licences; dvisvgm is licensed under the GNU GPL
version 3 or later.

Maintained MARS application, configuration, generation, test, coverage and
release-evidence tooling is native C and does not require a Python interpreter.
The historical generation packages recorded above remain part of the bundled
data's provenance, with their licences and notices retained.

## Distribution note

This notice records the repository's current dependency model. Before shipping
a self-contained binary, container, appliance or operating-system package,
regenerate the software bill of materials for that artefact and include the
copyright and licence files from every bundled package and transitive library.
