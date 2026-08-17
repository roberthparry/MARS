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

Optional libraries:

- libunistring, enabled by default with `ENABLE_UNISTRING=1`

On Debian/Ubuntu, install everything used by the default build with:

```sh
sudo apt install build-essential libgmp-dev libmpfr-dev libmpc-dev libsqlcipher-dev libunistring-dev
```

If you disable libunistring support, it is not required:

```sh
sudo apt install build-essential libgmp-dev libmpfr-dev libmpc-dev libsqlcipher-dev
make ENABLE_UNISTRING=0
```

Before building or installing, you can ask MARS to check for the required
development headers and link libraries:

```sh
make check-deps
```

If a required dependency is missing, the check prints the Debian/Ubuntu package
name to install, for example `sudo apt install libmpfr-dev`.

MARS Lab uses server-side TeX rendering, and the desktop installer now uses the
`sqlcipher` CLI to bootstrap the jurisdiction database, so the desktop Lab also
needs `latex`, `dvisvgm`, and `sqlcipher`:

```sh
sudo apt install texlive-latex-base dvisvgm sqlcipher
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
```

That places libraries in `/usr/local/lib` and public headers in
`/usr/local/include/mars`. Override paths as needed:

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
`~/.mars/jurisdiction/mars_jurisdiction_rules.db`. If you choose to enable
weather lookups, the optional WeatherAPI key is stored in
`~/.mars/config/weather.env`. Reinstalling MARS Lab recreates `~/.mars` while
preserving `weather.env`.

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
and qcomplex modules remain independent of MPFR and MPC:

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
- `libm`, pthreads, GMP, MPFR, MPC, and SQLCipher are required.
- `libunistring` is optional but enabled by default through `ENABLE_UNISTRING=1`.
- `make install` installs MARS headers and libraries only. It does not install
  external dependencies such as GMP, MPFR, MPC, SQLCipher, or libunistring;
  install those through your OS package manager before building MARS.
- Benchmarks are discovered automatically from `bench/bench_*.c`.
- Current benchmark targets include `bench_integrator` and
  `bench_matrix_expr`.
- The build currently adds `include/`, `src/`, `tests/`, and `tests/include/`
  to the compiler search path so project modules and tests can share internal
  headers.
  External consumers should treat only `include/` as public API.
