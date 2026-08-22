# Licensing and dependency continuity

This document records the MARS policy for open-source licence compliance and
for continuity if an upstream project changes ownership, governance or future
licensing. It is an engineering policy, not legal advice.

## Acquisition risk

An acquisition cannot normally revoke the open-source permissions already
granted for a version that MARS has lawfully received, provided that MARS
continues to meet that licence's conditions. A new owner can change the licence
of later releases only where it controls the necessary rights. It can also stop
publishing new releases, change trademarks, sell support separately or make
future development proprietary.

Consequently, an acquisition of SQLite or SQLCipher would not turn an existing
public-domain SQLite release or BSD-licensed SQLCipher Community Edition
release into a subscription product. The realistic risks are loss of future
open development, delayed security maintenance, confusing branding, or Linux
distributions eventually dropping an unmaintained package.

## MARS safeguards

Every MARS release should apply the following controls:

1. Keep [`LICENSE`](../LICENSE),
   [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md) and
   [`DEPENDENCIES.spdx`](../DEPENDENCIES.spdx) in the source and installed
   documentation. Keep the [privacy notice](./privacy.md) and
   [almanac data provenance](./almanac-data-provenance.md) with every release
   containing the generated ephemeris or AstroNav workbook.
2. Record the exact dependency versions used for release binaries, together
   with source archive checksums and the corresponding licence files.
3. Build from distribution packages by default, but retain reproducible source
   build instructions for critical libraries.
4. Review the software bill of materials before distributing a binary,
   container or appliance. Add every bundled transitive dependency; a source
   tree inventory is not a substitute for an artefact scan.
5. Treat any licence, copyright, source URL or cryptographic-provider change as
   a release-review event rather than accepting it silently during an upgrade.
6. Keep persistence behind the MARS `sqlite_t` API so that storage migration is
   localised if the preferred encrypted database implementation ever changes.
7. Run `make check-compliance` before release. It includes the public-
   distribution guard, checks required legal records and installed documents,
   verifies SPDX references and confirms recorded almanac checksums. The
   public-distribution guard rejects any tracked path reserved for the private
   ESAA reference copy. The repository's tracked pre-commit hook applies these
   checks when enabled with
   `git config core.hooksPath .githooks`.
8. Keep the [visual asset provenance record](./visual-asset-provenance.md) with
   every distribution containing MARS Lab artwork or screenshots, and record
   any future third-party visual material before it is added.

## Dependency risk register

| Component | Role | Present licence model | Continuity risk | MARS response |
|---|---|---|---|---|
| SQLite | Database engine within SQLCipher | Public domain | Very low | Preserve the exact upstream release and public-domain statement. A future owner cannot withdraw the existing dedication. |
| SQLCipher Community Edition | Encrypted SQLite storage | BSD 3-Clause | Medium | Preserve a known-good Community Edition source release and its full notice. If future releases close, maintain or adopt a community fork and keep database migration tests. |
| OpenSSL, zlib and Zstandard | Normal SQLCipher cryptographic provider and its verified transitive compression libraries on Debian and Ubuntu | Apache 2.0; zlib Licence; BSD 3-Clause or GPL 2 | Low | Use distribution security updates; record the actual dynamic closure in binary SBOMs because SQLCipher and OpenSSL can be built with alternatives. |
| GMP, MPFR and MPC | Multiprecision number backends | GNU LGPL/GPL families | Low | Dynamically link to distribution packages, retain notices and avoid copying private implementation code. Existing releases remain available under their granted terms. |
| libunistring | Optional Unicode support | GNU LGPL/GPL families | Low | Keep it optional and preserve the internal fallback path. |
| glibc, libm and pthreads | GNU/Linux platform runtime | Primarily GNU LGPL, with per-file exceptions | Low | Depend on the target distribution and include its exact copyright material when shipping a self-contained runtime. |
| Python 3 standard library | MARS Lab server and installers | PSF Licence | Low | Use only the standard library and keep the Lab client free of package-manager dependencies. |
| TeX Live and dvisvgm | MARS Lab TeX rendering | Per-package licences; dvisvgm is GPL 3 or later | Low to medium | Invoke separately installed programs. A packaged appliance must inventory the actual TeX packages included. |
| python-holidays and Workalendar | Generation sources for bundled holiday data | MIT | Low | Retain versioned provenance in the SQL source and notices; they are not runtime dependencies. |
| IANA Time Zone Database | Country and time-zone seed data | Public domain | Very low | Retain the release identifier and source URL in generated data provenance. |
| Unicode CLDR | Calendar identifiers and territory weekend conventions | Unicode Licence v3 | Very low | Retain the version, source URL and Unicode copyright and permission notice with distributions of derived jurisdiction data. |
| Tailscale | Optional private access to MARS Lab | BSD 3-Clause client | Low to medium | Keep it optional; normal local and LAN operation must not depend on it. |
| WeatherAPI.com | Optional hosted weather data | Revocable service terms rather than an open-source licence | Medium | Supply no shared account or key; require an installer who enables weather to use their own account; keep the integration optional and server-side; publish the privacy notice; credit the provider; disclose transmitted and locally retained fields; display the mandatory end-user warning; retain no weather responses; protect the API key; and review the terms before each release. Calendar and astronomical output must remain independent of the service. |
| JPL DE440 and NAIF auxiliary kernels | Generation sources for bundled almanac coefficients and workbook Moon data | NASA/JPL/Caltech NAIF use and redistribution rules | Low to medium | Do not bundle the kernels or SPICE Toolkit; retain official source URLs, kernel versions, published checksums, transformation details and output hashes in the almanac provenance record. Acknowledge NASA, JPL, Caltech and NAIF without implying endorsement. |
| NumPy, SpiceyPy, PyERFA and ERFA | Generation tools for almanac coefficients, frame rotations and test oracles | BSD 3-Clause or MIT | Low | They are not runtime dependencies. Retain their licences and record exact package versions for every future regeneration. |
| AstroNav 2000-2040 workbook | Original MARS navigation worksheet | MIT | Low | Preserve the dated authorship statement, repository history and checksum in the almanac provenance record. |

## SQLCipher contingency

SQLCipher is the dependency for which an ownership or governance change would
have the greatest operational impact. MARS should therefore preserve, for each
release that uses it:

- the exact Community Edition source archive and checksum;
- the complete BSD notice and SQLite public-domain statement;
- the cryptographic provider and its version;
- encrypted database compatibility and export/import tests; and
- the ability to rebuild against the preserved source on supported Linux
  distributions.

If upstream future versions become unsuitable, the preferred order is to keep
using a distribution-maintained BSD release, move to a reputable compatible
community fork, or replace the `sqlite_t` backend while providing an explicit
data migration tool. The existing licence permits continued maintenance of the
received community source; MARS must avoid implying endorsement by Zetetic and
must respect the SQLCipher trademark.

## Release evidence

The repository inventory deliberately leaves system-selected versions as
`NOASSERTION`. A release artefact should supplement it with the package manager
version list and the dynamic dependency closure of `libmars.so` and every
shipped executable. On GNU/Linux, tools such as `readelf` or `ldd` can help
discover that closure, but their output must be reviewed because dynamically
loaded providers and data packages may not appear there.

`make release-evidence` builds the release library from a clean worktree and
writes `build/compliance/release-evidence.json`. The record contains the source
commit, artefact and dependency hashes, available tool versions, package
versions and installed package-copyright hashes. Retain that JSON file with
the release artefacts and review entries that have no package ownership or
licence record.

The current automated controls and matters that still require a human decision
are recorded in the [compliance status](./compliance-status.md).
