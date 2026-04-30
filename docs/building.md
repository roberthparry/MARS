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

Run the symbolic `dval` matrix benchmark:

```sh
make bench_matrix_dval
```

See [`benchmarks.md`](benchmarks.md) for notes on output units and current
sample results.

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
- Current benchmark targets include `bench_integrator` and
  `bench_matrix_dval`.
- The build currently adds both `include/` and `include/internal/` to the
  compiler search path so project modules and tests can share internal headers.
  External consumers should treat only `include/` as public API.
