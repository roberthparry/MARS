# MARS Documentation

This directory contains the longer module documentation for MARS.

## Getting Started

- [Building](building.md)
- [Testing](testing.md)
- [Benchmarks](benchmarks.md)

## Modules

- [`number_t`](number.md) — generic numeric value cluster over exact, fixed-precision, and multiprecision backends
- [`qfloat_t`](qfloat.md) — double-double arithmetic and special functions
- [`qcomplex_t`](qcomplex.md) — double-double complex arithmetic and special functions
- [`matrix_t`](matrix.md) — generic high-precision matrix with pluggable element types and storage kinds
- [`equation_t`](equation.md) — parsed equations with symbolic isolation and numeric fallback
- [`expr_t`](expression.md) — differentiable expression DAGs and symbolic helper APIs
- [`almanac_t`](almanac.md) — ephemeris-backed SHA/declination lookups and snapshot queries
- [`datetime_t`](datetime.md) — civil and astronomical date/time utilities
- [`jurisdiction_t`](jurisdiction.md) — jurisdiction-aware holiday and working-day queries
- [`timeseries_t`](timeseries.md) — datetime-indexed forecasting and time-series analysis
- [`json_t`](json.md) — opaque JSON value tree with string-backed parsing and `number_t` fidelity
- [`sqlite_t`](sqlite.md) — opaque SQLCipher-backed SQLite storage for encrypted object persistence
- [`dictionary_t`](dictionary.md) — generic key/value storage
- [`set_t`](set.md) — generic set storage
- [`array_t`](array.md) — generic array storage
- [`string_t`](string.md) — UTF-8-aware dynamic strings
- [`bitset_t`](bitset.md) — dynamic thread-safe bitset
- [`integrator_t`](integrator.md) — adaptive G7K15 / Turan T15/T4 integrator with symbolic fast paths

## Guides

- [`dictionary_t` copying and cleanup](dictionary.md#copying-and-cleanup)
- [`set_t` copying and cleanup](set.md#copying-and-cleanup)
- [`array_t` copying and cleanup](array.md#copying-and-cleanup)

## Acknowledgements

`number_t` uses the GNU multiprecision libraries internally for its exact and
multiprecision backends:

- [GMP](https://gmplib.org/) for integer and rational arithmetic
- [MPFR](https://www.mpfr.org/) for correctly rounded multiprecision floats
- [MPC](https://www.multiprecision.org/mpc/) for multiprecision complex numbers

`sqlite_t` uses [SQLCipher](https://www.zetetic.net/sqlcipher/) for encrypted
SQLite-compatible storage.

## Notes

- The repository landing page is [`../README.md`](../README.md).
- These documents focus on API shape, examples, and implementation notes.
- Headers in `../include/` are the public surface. Shared implementation
  headers in `../src/internal/` are for MARS itself and are not intended as
  stable external API.
