# Building MARS

MARS uses `make` as its main build entry point. The supported and guaranteed
build target is Linux with GCC or Clang.

## Requirements

Build tools:

- GCC or Clang on Linux
- `make`
- `ar`
- standard C library headers

Required libraries:

- `libm`
- pthreads
- GMP
- MPFR
- MPC
- SQLCipher
- Zstandard, with the streaming `ZSTD_compressStream2` API
- libsodium, with the XChaCha20-Poly1305 secretstream API
- libcurl 7.85.0 or newer, with HTTPS and thread-safe global initialisation

Optional libraries:

- libunistring, enabled by default with `ENABLE_UNISTRING=1`

On Debian/Ubuntu, install everything used by the default build with:

```sh
sudo apt install build-essential pkg-config libgmp-dev libmpfr-dev libmpc-dev libsqlcipher-dev libunistring-dev libzstd-dev libsodium-dev libcurl4-openssl-dev
```

If you disable libunistring support, it is not required:

```sh
sudo apt install build-essential pkg-config libgmp-dev libmpfr-dev libmpc-dev libsqlcipher-dev libzstd-dev libsodium-dev libcurl4-openssl-dev
make ENABLE_UNISTRING=0
```

Before building or installing, you can ask MARS to check for the required
development headers and link libraries:

```sh
make check-deps
```

If a required dependency is missing, the check prints the Debian/Ubuntu package
name to install, for example `sudo apt install libmpfr-dev`.

For file-module test coverage, `make -j1 coverage-file` additionally requires
`gcov` from the GCC toolchain and Python 3. It reports measured execution
coverage and enforces minimums; see the
[file testing guide](file.md#coverage-and-failure-path-tests) for details.

### HTTP transport dependency

The [HTTP module](http.md) uses system libcurl. Install the development package
`libcurl4-openssl-dev`, not merely the runtime library. Make queries the
`libcurl` pkg-config record and falls back to `-lcurl` if unavailable.
`CURL_CFLAGS` and `CURL_LIBS` can override discovery for non-standard installs.
Static archive consumers must also link libcurl and its transitive dependencies.
`make check-deps` checks the minimum header version and links a transport probe.
The ordinary HTTP tests additionally use Python 3's standard library for local
HTTP/HTTPS fixtures; they do not contact external services.

The [webserver module](webserver.md) uses Linux sockets and adds no dependency.
Its local tests and example need loopback sockets and process creation; they do
not need internet access. Source, header and test discovery includes the module
automatically. Run `make -j1 test_webserver` for its dedicated suite.

### File compression and encryption dependencies

The Makefile requires Zstandard and libsodium for the file module's streaming
compression and authenticated encryption APIs. Applications invoke these APIs
explicitly; existing file operations do not automatically compress or encrypt.

Install the development packages `libzstd-dev` and `libsodium-dev`, not just
their runtime libraries. Install `pkg-config` for library discovery: MARS queries
the `libzstd` and `libsodium` package records for compiler and linker flags.
If those records are unavailable, it falls back to `-lzstd` and `-lsodium`
and the compiler's standard header search paths. For non-standard installation
prefixes, configure `PKG_CONFIG_PATH` before invoking Make.

`make check-deps` compiles and links probes for Zstandard streaming compression
and libsodium secretstream, reporting the relevant development package if
either is unavailable. This is a build-availability check, not a compression
or cryptographic correctness test. A successful check produces no diagnostic
output and exits with status zero.

Both libraries are included in the shared-library and test link commands.
Applications linking the static MARS archive must also link its dependencies,
including Zstandard and libsodium. Neither the `zstd` command-line tool nor a
separate encryption command-line program is required.

See the [file guide](file.md#compression-encryption-and-object-storage)
for API behaviour and examples, and the
[third-party notices](../THIRD_PARTY_NOTICES.md) for redistribution information.

MARS Lab requires Python 3.10 or later and uses only the Python standard
library. It uses server-side TeX rendering, and the desktop installer uses the
`sqlcipher` CLI to bootstrap the jurisdiction database, so the desktop Lab also
needs `latex`, `dvisvgm`, and `sqlcipher`:

```sh
sudo apt install python3 texlive-latex-base dvisvgm sqlcipher
make check-lab-deps
```

## Common Targets

Default release build, shared library, tests, and any registered benchmarks:

```sh
make
```

Release build:

```sh
make release
```

Debug build:

```sh
make debug
```

Clean build outputs:

```sh
make clean
```

Install headers and libraries:

```sh
sudo make install
```

Check required development libraries before building or installing:

```sh
make check-deps
```

Check the development libraries and MARS Lab TeX rendering tools:

```sh
make check-lab-deps
```

By default, installation uses:

```text
PREFIX=/usr/local
LIBDIR=$(PREFIX)/lib
INCLUDEDIR=$(PREFIX)/include
DOCDIR=$(PREFIX)/share/doc/mars
```

That places libraries in `/usr/local/lib` and public headers in
`/usr/local/include/mars`. The MARS licence, third-party notices, SPDX
dependency inventory, licensing policy, privacy notice, almanac and visual
provenance records, and compliance status are installed in
`/usr/local/share/doc/mars`, so they remain with packaged installations.
Override paths as needed:

```sh
make install PREFIX=/opt/mars
make install DESTDIR=/tmp/package-root PREFIX=/usr
```

Install the desktop MARS Lab launcher with:

```sh
make install-mars-lab
```

During development, the Makefile can manage the local Lab process without a
manual `pgrep` and `kill` cycle:

```sh
make mars-lab
make mars-lab-stop
make mars-lab-restart
```

```text
MARS Lab running at http://localhost:<port>/
Stopped MARS Lab.
MARS Lab running at http://localhost:<port>/
```

`make mars-lab-restart` stops a Lab process belonging to the current user,
rebuilds the helper when necessary and launches it again.
The direct `tools/mars-lab` launcher performs the same helper build check
before starting the browser client.

That installer now prompts for a password to protect the private jurisdiction
database, stores the resulting configuration in
`~/.mars/config/jurisdiction-db.env`, and builds the encrypted jurisdiction database at
`~/.mars/jurisdiction/mars_jurisdiction_rules.db`. MARS supplies no shared
WeatherAPI account or key. If the installer creates their own WeatherAPI
account and chooses to enable weather lookups, that account's key is stored in
`~/.mars/config/weather.env`. Reinstalling MARS Lab recreates `~/.mars` while
preserving `weather.env`. Each lookup sends the configured key, selected date,
latitude and longitude from the local MARS Lab server to WeatherAPI.com over
HTTPS; the key is not sent to the browser. MARS does not cache or persist the
weather response, although the date and coordinates remain in private local
Lab state so that its inputs can be restored. The displayed weather is general,
probabilistic information and must not be the sole basis for safety-critical
decisions. See the [MARS privacy notice](privacy.md),
[WeatherAPI privacy policy](https://www.weatherapi.com/privacy.aspx) and
[WeatherAPI terms](https://www.weatherapi.com/terms.aspx).

Remove installed MARS files:

```sh
sudo make uninstall
```

Run the full test suite:

```sh
make test
```

The normal build also runs `check-native-numeric-boundaries`. This guard checks
both source references and undefined object symbols to ensure that the qfloat
and qcomplex modules do not include MPFR/MPC headers or call their APIs directly.
Bessel Y/I/K and Struve H/L deliberately delegate through the public number API;
their kernels use MPFR/MPC inside the number module. The guard enforces that
module boundary, not independence from transitive multiprecision dependencies:

```sh
make check-native-numeric-boundaries
```

Run a single test binary:

```sh
make test_expression
make test_matrix
make test_integrator
```

Run the integrator benchmark:

```sh
make bench_integrator
```

Run the symbolic `expr` matrix benchmark:

```sh
make bench_matrix_expr
```

See [`benchmarks.md`](./benchmarks.md) for notes on output units and current
sample results.

Show the target summary:

```sh
make help
```

## Notes

- Run commands from the repository root.
- The code is C99-style C with intentional GNU C extensions such as
  `__attribute__`, so GCC or Clang is required.
- The Linux system toolchain is the supported path; MSVC/Windows builds are not
  currently guaranteed.
- `libm`, pthreads, GMP, MPFR, MPC, SQLCipher, Zstandard, libsodium and libcurl are required.
- `libunistring` is optional but enabled by default through `ENABLE_UNISTRING=1`.
- `make install` installs MARS headers and libraries only. It does not install
  external dependencies such as GMP, MPFR, MPC, SQLCipher, Zstandard, libsodium, libcurl or libunistring;
  install those through your OS package manager before building MARS.
- Benchmarks are discovered automatically from `bench/bench_*.c`.
- Current benchmark targets include `bench_integrator` and
  `bench_matrix_expr`.
- The build currently adds `include/`, `src/`, `tests/`, and `tests/include/`
  to the compiler search path so project modules and tests can share internal
  headers.
  External consumers should treat only `include/` as public API.
