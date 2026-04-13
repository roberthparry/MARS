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
```

## Notes

- Run commands from the repository root.
- The test output is intended to read cleanly in a normal terminal or in the
  Visual Studio Code integrated terminal.