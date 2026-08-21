# Third-party notices

MARS is distributed under the MIT Licence. Its own licence is in
[`LICENSE`](LICENSE). MARS uses system-supplied open-source libraries and tools,
and ships jurisdiction data generated with open-source calendar projects. The
machine-readable inventory is in [`DEPENDENCIES.spdx`](DEPENDENCIES.spdx).

MARS does not vendor the source or binary form of the linked libraries listed
below. A distributor that bundles those libraries must also include the exact
licence material supplied with the versions it redistributes. The notices in
this file do not replace those obligations.

## SQLCipher Community Edition

MARS links to SQLCipher and uses its command-line program to create encrypted
databases. SQLCipher incorporates SQLite and normally uses a separately
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

Copyright (c) The OpenSSL Project Authors. All rights reserved.

This product includes software developed by the OpenSSL Project for use in the
OpenSSL Toolkit.

The OpenSSL build verified for MARS also loads system zlib and Zstandard
libraries. They are not bundled by MARS. A self-contained distributor must
include the zlib licence and the selected BSD 3-Clause or GPL version 2 terms
for Zstandard.

OpenSSL licence information:
<https://openssl-library.org/source/license/index.html>

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
the BSD 3-Clause Licence. These are generation-time tools rather than MARS
runtime dependencies.

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

## WeatherAPI.com

WeatherAPI.com is an optional hosted service, not a library or data set shipped
by MARS. Its service and returned data are governed by the
[WeatherAPI terms](https://www.weatherapi.com/terms.aspx) and
[privacy policy](https://www.weatherapi.com/privacy.aspx). MARS supplies no
shared WeatherAPI account or key. Weather remains disabled unless the person
installing MARS Lab creates their own WeatherAPI account and configures that
account's key. MARS Lab credits WeatherAPI.com as the source whenever it
displays returned weather data. It does not cache or persist the response.

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

MARS Lab invokes Python 3, TeX Live, dvisvgm and, optionally, Tailscale and
desktop integration tools. These programs are installed separately and are not
incorporated into MARS. Their licences govern their own programs. TeX Live is a
collection with per-package licences; dvisvgm is licensed under the GNU GPL
version 3 or later.

## Distribution note

This notice records the repository's current dependency model. Before shipping
a self-contained binary, container, appliance or operating-system package,
regenerate the software bill of materials for that artefact and include the
copyright and licence files from every bundled package and transitive library.
