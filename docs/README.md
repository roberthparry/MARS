# MARS Documentation

This directory contains the longer module documentation for MARS.

## Getting Started

- [Building](building.md)
- [Testing](testing.md)
- [Benchmarks](benchmarks.md)

## Modules

- [`mint_t`](mint.md) — arbitrary-precision signed integers and number theory helpers
- [`mfloat_t`](mfloat.md) — opaque multiprecision floating-point arithmetic
- [`mcomplex_t`](mcomplex.md) — opaque multiprecision complex arithmetic backed by `mfloat_t`
- [`qfloat_t`](qfloat.md) — double-double arithmetic and special functions
- [`qcomplex_t`](qcomplex.md) — double-double complex arithmetic and special functions
- [`matrix_t`](matrix.md) — generic high-precision matrix with pluggable element types and storage kinds
- [`dval_t`](dval.md) — differentiable expression DAGs and symbolic helper APIs
- [`datetime_t`](datetime.md) — civil and astronomical date/time utilities
- [`dictionary_t`](dictionary.md) — generic key/value storage
- [`set_t`](set.md) — generic set storage
- [`array_t`](array.md) — generic array storage
- [`string_t`](string.md) — UTF-8-aware dynamic strings
- [`bitset_t`](bitset.md) — dynamic thread-safe bitset
- [`integrator_t`](integrator.md) — adaptive G7K15 / Turan T15/T4 integrator with symbolic fast paths

## Guides

- [`dictionary_t` ownership models](dictionary.md#ownership-models)
- [`set_t` ownership models](set.md#ownership-models)
- [`array_t` ownership models](array.md#ownership-models)

## Notes

- The repository landing page is [`../README.md`](../README.md).
- These documents focus on API shape, examples, and implementation notes.
- Headers in `../include/` are the public surface. Headers in
  `../include/internal/` are shared implementation headers for MARS itself and
  are not intended as stable external API.
