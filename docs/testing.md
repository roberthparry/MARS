# Testing MARS

The project provides per-module test targets.

## Run Tests

```sh
make test_qfloat
make test_dval
make test_datetime
make test_dictionary
make test_set
make test_string
make test_matrix
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
make test_qfloat
make test_dval
make test_datetime
make test_dictionary
make test_set
make test_string
make test_matrix
```

## Notes

- Run commands from the repository root.
- The test output is intended to read cleanly in a normal terminal or in the
  Visual Studio Code integrated terminal.

---

## Enabling and Disabling Tests

Individual tests and whole groups can be skipped without recompilation by editing `tests/test_config.json`. The harness reads this file at startup and writes it back on exit, so any new tests that have never appeared in the file are automatically added as `true` on first run.

A missing key always means **enabled**. Set a value to `false` to skip it.

### Flat tests

Most test files list their tests as simple booleans at the top level of their entry:

```json
{
  "tests/test_array.c": {
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
  "tests/test_dval.c": {
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
| `TEST_CONFIG_LOCAL` | `tests/<basename>.json` (one file per test binary) |

All current test files use `TEST_CONFIG_GLOBAL`, so `tests/test_config.json` is the single place to control everything.