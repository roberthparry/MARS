# MARS Documentation

This directory contains the longer module documentation for MARS.

## Getting Started

- [MARS Lab](./mars-lab.md) — browser workspace with Cartesian complex
  expression and calculus results, native series recognition and value cards
- [Building](./building.md)
- [Testing](./testing.md)
- [API glossary](#api-glossary)
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
- [`sampler_t`](./sampler.md) — bounded numerical curve sampling, expression snapshots and explicit gaps
- [`plot_t`](./plot.md) — native in-memory SVG plots with axes, multiple series and clipping
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
- [`globe_t`](./globe.md) — geographic controller for shorelines, rivers and regional map layers
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
- [`TeX_t`](./TeX.md) — owned mathematical notation and escaped text
- [`bitset_t`](./bitset.md) — dynamic thread-safe bitset
- [`diagnostic_t`](./diagnostic.md) — immutable diagnostics, source locations and cause chains
- [`executor_t`](./executor.md) — bounded native worker pools, task ownership and cooperative stopping
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
- [Reverse Polish TeX construction](./TeX.md#reverse-polish-construction-contract)

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

## API glossary

**Borrowed** means access without a transfer of ownership. A borrowed parameter
remains owned by the caller or its existing owner; passing it to a function does
not make that function responsible for freeing it. A borrowed return value is
owned elsewhere and must not be freed by the recipient. If you already own an
object, lending it to a function does not remove your responsibility to release
it when appropriate.

A borrowed reference is valid only for the lifetime stated by the API. It may
become invalid when its owner is freed or modified. Borrowing alone does not
imply permission to modify the object or retain the reference beyond that lifetime.

**Owned** means responsibility for releasing an object using the documented
release function. An owned return value transfers that responsibility to the caller.

**Ownership transfer** means responsibility for releasing an object passes to
another owner. After transferring ownership, the previous owner must not release
the transferred reference. The API specifies when transfer occurs, including
whether ownership stays with the caller if an operation fails.

**Lifetime** is the period during which an object or reference remains valid.
An object's lifetime ends when it is destroyed. A reference into an object may
become invalid sooner if an operation replaces or reallocates the referenced data.

**Opaque type** is a type whose internal structure is hidden from callers.
Callers create, inspect, modify and release its objects through the documented
API rather than accessing their fields directly.

**Immutable** means an object's observable contents cannot be changed after
construction. An operation that produces different contents returns a separate
object. Immutability alone does not make concurrent destruction safe.

**Retain / release** applies to reference-counted objects. Retaining acquires an
additional ownership reference without copying the object. Releasing relinquishes
one such reference; the object is destroyed when its last ownership reference is
released. Each acquired reference must be released once, using the module's
documented functions. Copying a pointer does not itself retain the object.

**Deep copy / shallow copy** describes how a copy handles contained objects.
A deep copy duplicates the contained data within the scope documented by the API.
A shallow copy shares references to some or all of that data. Whether shared
references are retained or borrowed is a separate ownership question specified
by the API; neither term alone determines who must release them.

**View** provides access to another object's data, often a selected part without
copying it. The API specifies whether the view retains its source or borrows it,
whether modification is permitted, and which operations invalidate it.

**Caller / callee** means the code invoking a function and the function being
invoked, respectively. A caller-owned result is the caller's responsibility to
release; a callee does not take ownership of its inputs unless documented.

**Thread-safe** means an API supports the documented forms of concurrent use
without requiring additional synchronisation from the caller. Check the scope:
safe use of separate objects does not necessarily permit simultaneous access to
the same object, and concurrent destruction may require external synchronisation.

**Re-entrant** means a function can safely be entered again before an earlier
invocation finishes, under its documented conditions. Nested callbacks can cause
such re-entry. Thread safety does not automatically imply re-entrancy, and neither
term alone guarantees that calling the function from a signal handler is safe.

**Undefined behaviour** means behaviour for which the C language provides no
guarantees. Using an invalid pointer or accessing an array outside its bounds can
cause undefined behaviour. It need not produce an error or crash, even if the
same code appeared to work previously.

## Notes

- The repository landing page is [`../README.md`](../README.md).
- These documents focus on API shape, examples, and implementation notes.
- Headers in `../include/` are the public surface. Shared implementation
  headers in `../src/internal/` are for MARS itself and are not intended as
  stable external API.
