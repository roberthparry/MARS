# `string_t`

`string_t` is a UTF-8-aware dynamic string type with Unicode-oriented
operations.

## Capabilities

- append, insert, remove, replace, and trim
- UTF-8 validation
- normalization
- grapheme-aware operations
- split and join helpers
- builder-style construction
- fixed-capacity buffer support

## Example: Basic UTF-8 Manipulation

```c
#include <stdio.h>
#include "ustring.h"

int main(void) {
    string_t *s = string_new_with("Héllo");

    string_append_cstr(s, " 🌍");
    string_insert(s, 1, "🙂");
    string_normalize(s, STRING_NORM_NFC);

    printf("%s\n", string_c_str(s));

    string_free(s);
    return 0;
}
```

Expected output:

```text
H🙂éllo 🌍
```

## Example: Grapheme Iteration

```c
#include <stdio.h>
#include "ustring.h"

int main(void) {
    string_t *s = string_new_with("👨‍👩‍👧‍👦 family");

    size_t count = string_grapheme_count(s);
    printf("Graphemes: %zu\n", count);

    for (size_t i = 0; i < count; i++) {
        string_t *g = string_grapheme_at(s, i);
        printf("[%zu] %s\n", i, string_c_str(g));
        string_free(g);
    }

    string_free(s);
    return 0;
}
```

Expected output:

```text
Graphemes: 8
[0] 👨‍👩‍👧‍👦
[1]  
[2] f
[3] a
[4] m
[5] i
[6] l
[7] y
```

## API Reference

The full public API is declared in `include/ustring.h`. The functions below are
the main entry points most users will want first.

### Construction and Lifetime

- `string_new_with(const char *text)`  
  Create a new `string_t` from UTF-8 input.

- `string_free(string_t *s)`  
  Release a string and its owned storage.

- `string_c_str(const string_t *s)`  
  Return a NUL-terminated UTF-8 view of the underlying buffer.

### Core Mutation

- `string_append_cstr(string_t *s, const char *text)`  
  Append UTF-8 text to the end of the string.

- `string_insert(string_t *s, size_t index, const char *text)`  
  Insert UTF-8 text at the given position.

- `string_normalize(string_t *s, string_norm_t form)`  
  Rewrite the string into a chosen normalization form such as NFC or NFD.

### Grapheme Helpers

- `string_grapheme_count(const string_t *s)`  
  Return the number of user-visible grapheme clusters.

- `string_grapheme_at(const string_t *s, size_t index)`  
  Return the grapheme at `index` as a new `string_t`.

## Design Notes

### Representation

A `string_t` stores:

- a UTF-8 byte buffer
- current byte length
- capacity
- cached metadata where useful

The buffer remains NUL-terminated for convenience.

### Validation

Constructors validate input as UTF-8. Invalid input can be repaired into a safe
internal form so later operations do not have to walk malformed data.

### Grapheme Handling

Grapheme-aware operations follow Unicode grapheme cluster rules so that
user-visible characters can be handled as units instead of raw bytes.

### Normalization

Normalization support allows canonically equivalent strings to be rewritten into
a consistent form such as NFC or NFD.

### Builder Path

The builder path is intended for incremental construction with fewer repeated
checks during appends, followed by an explicit finalization step that produces a
normal `string_t`.

### Fixed Buffers

A fixed-capacity companion buffer API is useful for stack allocation, temporary
formatting, and constrained environments.

## Tradeoffs

The design favors correctness for UTF-8 and grapheme-oriented operations over
the simplicity of treating text as raw bytes.