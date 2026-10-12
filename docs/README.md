# MARS Documentation

This directory contains the longer module documentation for MARS.

## Getting Started

- [MARS Lab](./mars-lab.md) — browser workspace with Cartesian complex
  expression and calculus results, native series recognition and value cards
- [Building](./building.md)
- [Testing](./testing.md)
- [Benchmarks](./benchmarks.md)
- [Licensing and dependency continuity](./licensing.md)
- [Compliance status](./compliance-status.md)
- [Privacy](./privacy.md)
- [Almanac data provenance](./almanac-data-provenance.md)
- [Visual asset provenance](./visual-asset-provenance.md)
- [About the author](./about-the-author.md)

## Modules

- [`csv_t`](./csv.md) — bounded UTF-8 CSV streams, quoting and indexed headers
- [`file_t`, `file_info_t`, `file_key_t`](./file.md) — Linux file I/O,
  metadata, compression, authenticated encryption and SQLCipher transfers
- [`number_t`](./number.md) — generic numeric value cluster over exact,
  fixed-precision and multiprecision backends, including Li₁ and the Lerch
  transcendent
- [`qfloat_t`](./qfloat.md) — independent double-double arithmetic and real
  special functions, including harmonic polynomials, Riemann and Hurwitz zeta,
  Li₁ and the Lerch transcendent
- [`quantity_t`](./quantity.md) — immutable physical quantities, exact unit factors,
  dimensional arithmetic, temperature points and astronomical/angular conversions
- [`qcomplex_t`](./qcomplex.md) — independent double-double complex arithmetic
  and complex special functions, including harmonic polynomials, Li₁ and the
  Lerch transcendent
- [`matrix_t`](./matrix.md) — generic high-precision matrix with numeric and
  symbolic entries, native expression parsing, symbolic calculus and
  matrix-wide simplification
- [`equation_t`](./equation.md) — parsed equations with symbolic isolation and numeric fallback
- [`diffequ_t`](./diffequation.md) — ODE/PDE parsing and symbolic solving, including selected linearisations and symmetry metadata
- [`expr_t`](./expression.md) — differentiable expression DAGs, symbolic
  derivatives, numeric reverse-mode gradients, Cartesian complex presentation,
  algebraic-series and extensible finite-progression recognition, harmonic
  polynomials, symbolic sums and products, Li₁ and Lerch-transcendent calculus,
  integration and helper APIs
- [`almanac_t`](./almanac.md) — ephemeris-backed SHA/declination lookups and snapshot queries
- [`datetime_t`](./datetime.md) — civil and astronomical date/time utilities
- [`jurisdiction_t`](./jurisdiction.md) — jurisdiction-aware holiday and working-day queries
- [`timeseries_t`](./timeseries.md) — datetime-indexed forecasting and time-series analysis
- [`json_t`](./json.md) — opaque JSON value tree with string-backed parsing and `number_t` fidelity
- [`xml_t`](./xml.md) — native streaming XML and dictionary-backed attributes
- [`http_client_t`](./http.md) — HTTP/HTTPS, SOAP, forms, event streams, WebSocket and unary gRPC
- [`websrv_t`](./webserver.md) — bounded synchronous Linux web services and exact routing
- [`protobuf_t`](./protobuf.md) — native bounded Protocol Buffers encoding and decoding
- [`sqlite_t`](./sqlite.md) — opaque SQLCipher-backed SQLite storage for encrypted object persistence
- [`dictionary_t`](./dictionary.md) — generic key/value storage
- [`set_t`](./set.md) — generic set storage
- [`array_t`](./array.md) — generic array storage
- [`regex_t`](./regex.md) — compiled UTF-8 regular expressions and capture spans
- [`string_t`](./string.md) — UTF-8-aware dynamic strings
- [`bitset_t`](./bitset.md) — dynamic thread-safe bitset
- [`integrator_t`](./integrator.md) — adaptive tanh–sinh / Gauss–Kronrod integration with symbolic fast paths
- [`computation_t`](./computation.md) — shared cooperative cancellation, monotonic deadlines, work budgets and progress
- [`expr_context_t`](./expr_context.md) — owned assumptions, symbol roles and binding-aware expression snapshots

## Guides

- [`dictionary_t` copying and cleanup](./dictionary.md#copying-and-cleanup)
- [`set_t` copying and cleanup](./set.md#copying-and-cleanup)
- [`array_t` copying and cleanup](./array.md#copying-and-cleanup)

## Design Notes

- [Ophelia expression language](./design-notes/expression-language.md)
- [Integral transform syntax and implemented pairs](./design-notes/integral-transforms.md)
- [Lerch transcendent support](./design-notes/lerch-transcendent.md)
- [Future differential-equation work](./design-notes/differential-equations.md)
- [Literate comments and notebook output](./design-notes/literate-comments-and-notebooks.md)

## Acknowledgements

`number_t` uses the GNU multiprecision libraries internally for its exact and
multiprecision backends:

- [GMP](https://gmplib.org/) for integer and rational arithmetic
- [MPFR](https://www.mpfr.org/) for correctly rounded multiprecision floats
- [MPC](https://www.multiprecision.org/mpc/) for multiprecision complex numbers

`sqlite_t` uses [SQLCipher](https://www.zetetic.net/sqlcipher/) for encrypted
SQLite-compatible storage.

File compression and authenticated encryption use Zstandard and libsodium.
HTTP transport uses libcurl; nghttp2 supplies HTTP/2 in the verified libcurl
build. The native webserver, XML and Protobuf modules add no external runtime
library. Clang and LLD build the Lab's C/WebAssembly browser module; TeX Live
and dvisvgm render its mathematical output. OpenSSL is also used directly by
the local protocol test fixtures, not by the public webserver module.

The repository's [third-party notices](../THIRD_PARTY_NOTICES.md) record the
required SQLCipher notice and the licences and provenance of other external
components. The accompanying [`DEPENDENCIES.spdx`](../DEPENDENCIES.spdx) file
is the machine-readable dependency inventory.

## Notes

- The repository landing page is [`../README.md`](../README.md).
- These documents focus on API shape, examples, and implementation notes.
- Headers in `../include/` are the public surface. Shared implementation
  headers in `../src/internal/` are for MARS itself and are not intended as
  stable external API.
