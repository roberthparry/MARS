# Building MARS

MARS uses `make` as its main build entry point.

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

Run the full test suite:

```sh
make test
```

Run a single test binary:

```sh
make test_dval
make test_matrix
make test_integrator
```

Run the integrator benchmark:

```sh
make bench_integrator
```

Show the target summary:

```sh
make help
```

## Notes

- Run commands from the repository root.
- A C99-capable compiler is required.
- On Linux, the default system toolchain is usually sufficient.
- `libm` is required.
- `libunistring` is optional but enabled by default through `ENABLE_UNISTRING=1`.
- Benchmarks are discovered automatically from `bench/bench_*.c`.
