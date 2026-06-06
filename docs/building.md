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

Optional libraries:

- libunistring, enabled by default with `ENABLE_UNISTRING=1`

On Debian/Ubuntu, install everything used by the default build with:

```sh
sudo apt install build-essential libgmp-dev libmpfr-dev libmpc-dev libunistring-dev
```

If you disable libunistring support, it is not required:

```sh
sudo apt install build-essential libgmp-dev libmpfr-dev libmpc-dev
make ENABLE_UNISTRING=0
```

Before building or installing, you can ask MARS to check for the required
development headers and link libraries:

```sh
make check-deps
```

If a required dependency is missing, the check prints the Debian/Ubuntu package
name to install, for example `sudo apt install libmpfr-dev`.

MARS Lab uses server-side TeX rendering, so the desktop Lab also needs `latex`
and `dvisvgm`:

```sh
sudo apt install texlive-latex-base dvisvgm
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

Remove installed MARS files:

```sh
sudo make uninstall
```

Run the full test suite:

```sh
make test
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

See [`benchmarks.md`](benchmarks.md) for notes on output units and current
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
- `libm`, pthreads, GMP, MPFR, and MPC are required.
- `libunistring` is optional but enabled by default through `ENABLE_UNISTRING=1`.
- `make install` installs MARS headers and libraries only. It does not install
  external dependencies such as GMP, MPFR, MPC, or libunistring; install those
  through your OS package manager before building MARS.
- Benchmarks are discovered automatically from `bench/bench_*.c`.
- Current benchmark targets include `bench_integrator` and
  `bench_matrix_expr`.
- The build currently adds `include/`, `src/`, `tests/`, and `tests/include/`
  to the compiler search path so project modules and tests can share internal
  headers.
  External consumers should treat only `include/` as public API.
