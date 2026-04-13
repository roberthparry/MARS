# Building MARS

MARS uses `make` as its main build entry point.

## Common Targets

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

## Notes

- Run commands from the repository root.
- A C99-capable compiler is required.
- On Linux, the default system toolchain is usually sufficient.