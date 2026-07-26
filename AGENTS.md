# MARS Coding Style

- In C `switch` statements, indent every `case` and `default` label one level
  inside the `switch` body. Indent the statements belonging to each label one
  additional level.

  ```c
  switch (value) {
      case FIRST:
          handle_first();
          break;

      case SECOND:
          handle_second();
          break;

      default:
          handle_default();
          break;
  }
  ```
