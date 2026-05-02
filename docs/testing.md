# Testing MARS

The project provides per-module test targets.

## Run Tests

```sh
make test_mint
make tests/build/release/mfloat/test_mfloat
tests/build/release/mfloat/test_mfloat
make test_qfloat
make test_qcomplex
make test_dval
make test_datetime
make test_dictionary
make test_set
make test_array
make test_string
make test_bitset
make test_matrix
make test_integrator
```

## Typical Workflow

During development:

```sh
make debug
make test_qfloat
```

Before committing:

```sh
make release
make test_mint
make tests/build/release/mfloat/test_mfloat
tests/build/release/mfloat/test_mfloat
make test_qfloat
make test_qcomplex
make test_dval
make test_datetime
make test_dictionary
make test_set
make test_array
make test_string
make test_bitset
make test_matrix
make test_integrator
```

## Notes

- Run commands from the repository root.
- For now, prefer running test targets sequentially rather than overlapping them. The current codebase and build products are not yet fully thread-safe for concurrent test runs.
- The test output is intended to read cleanly in a normal terminal or in the
  Visual Studio Code integrated terminal.

## Benchmarks

The repository also includes focused benchmark targets. For the current symbolic
integrator work:

```sh
make bench_integrator
make bench_matrix_dval
make bench_mint_mul
make bench_mint_div
make bench_mfloat_maths
```

This benchmark reports both matched symbolic fast paths and nearby fallback
cases so performance changes are easy to spot. See
[`benchmarks.md`](benchmarks.md) for benchmark-specific notes and sample
results.

---

## Enabling and Disabling Tests

Individual tests and whole groups can be skipped without recompilation by editing `tests/test_config.json`. The harness reads this file at startup and writes it back on exit, so any new tests that have never appeared in the file are automatically added as `true` on first run.

A missing key always means **enabled**. Set a value to `false` to skip it.

### Flat tests

Most test files list their tests as simple booleans at the top level of their entry.
Entries are keyed by the test source's path under `tests/`:

```json
{
  "tests/array/test_array.c": {
    "test_ints": true,
    "test_strings": false,
    "test_swap_rotate": true
  }
}
```

Setting `"test_strings": false` causes that test to be reported as `SKIP` and excluded from the pass/fail count.

### Grouped tests

Some tests are organised into groups. A group object has an `"enabled"` key for the group itself, plus one key per member:

```json
{
  "tests/dval/test_dval.c": {
    "test_arithmetic": {
      "enabled": true,
      "test_add": true,
      "test_sub": false,
      "test_mul": true
    }
  }
}
```

Setting `"enabled": false` on the group skips every test inside it regardless of the individual values. Setting an individual member to `false` skips only that member while leaving the rest of the group active.

Groups can be nested to any depth; a test is only run if every ancestor in its chain is enabled.

### Modes

Test files declare one of two modes before including `test_harness.h`:

| Mode | File consulted |
|---|---|
| `TEST_CONFIG_GLOBAL` | `tests/test_config.json` (shared by all test binaries) |
| `TEST_CONFIG_LOCAL` | `<normalized test source path>.json` (one file per test binary) |

For example, `tests/test_config/test_test_config.c` uses `tests/test_config/test_test_config.json` in local mode.

All current test files use `TEST_CONFIG_GLOBAL`, so `tests/test_config.json` is the single place to control everything.
