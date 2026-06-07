# `string_t`

`string_t` is an owning text string type. You give it ordinary text that can
be read or typed, and it handles accents, combined characters and emoji
sequences as characters.

## Capabilities

- heap-allocated dynamic string with automatic capacity growth
- append, insert, trim, replace, split, and join
- validation on construction; invalid byte sequences are replaced with U+FFFD
- character count, index access, substring extraction, iteration and reversal
- text case conversion
- internal canonical storage for reliable comparison/search behaviour
- fixed-capacity `string_buffer_t` for stack allocation
- `string_builder_t` alias for incremental construction
- hashing

## Example: Basic Text Manipulation

```c
#include <stdio.h>
#include "ustring.h"

int main(void) {
    string_t *s = string_new_with("Héllo");

    string_append_cstr(s, " 🌍");
    string_insert(s, 1, "🙂");

    printf("%s\n", string_c_str(s));

    string_free(s);
    return 0;
}
```

Expected output:

```text
H🙂éllo 🌍
```

## Example: Unicode Text Just Works

```c
#include <stdio.h>
#include "ustring.h"

int main(void) {
    string_t *s = string_new_with("👨‍👩‍👧‍👦 café 🇬🇧");

    rune_t first = string_at(s, 0);
    string_t *first_text = rune_to_string(first);
    string_t *word = string_substring(s, 2, 4);

    printf("Characters: %zu\n", string_count(s));
    printf("First: %s\n", string_c_str(first_text));
    printf("Word: %s\n", string_c_str(word));

    string_reverse(s);
    printf("Reversed: %s\n", string_c_str(s));

    string_free(first_text);
    string_free(word);
    string_free(s);
    return 0;
}
```

Expected output:

```text
Characters: 8
First: 👨‍👩‍👧‍👦
Word: café
Reversed: 🇬🇧 éfac 👨‍👩‍👧‍👦
```

## Example: Character Iteration

```c
#include <stdio.h>
#include "ustring.h"

int main(void) {
    string_t *s = string_new_with("👨‍👩‍👧‍👦 family");

    size_t count = string_count(s);
    printf("Characters: %zu\n", count);

    for (size_t i = 0; i < count; i++) {
        string_printf("[%zu] %R\n", i, string_at(s, i));
    }

    string_free(s);
    return 0;
}
```

Expected output:

```text
Characters: 8
[0] 👨‍👩‍👧‍👦
[1]  
[2] f
[3] a
[4] m
[5] i
[6] l
[7] y
```

## Example: Fixed-Capacity Buffer

```c
#include <stdio.h>
#include "ustring.h"

int main(void) {
    char storage[64];
    string_buffer_t buf;
    string_buffer_init(&buf, storage, sizeof(storage));
    string_buffer_append(&buf, "Hello");
    string_buffer_append_char(&buf, '!');
    printf("%s\n", string_buffer_c_str(&buf));   /* "Hello!" */
    return 0;
}
```

## Design Notes

### Characters, Not Storage Units

The simple API works in user-visible characters. `string_count()`,
`string_at()`, `string_substring()`, `string_each()` and `string_reverse()`
treat combined accents and emoji sequences as single characters. The storage
details stay inside the string implementation.

### Canonical Storage

`string_t` keeps text in a stable internal form when libunistring is available.
Callers do not need to choose a storage form; construction and mutation handle
that housekeeping for them.

### Ownership

Every function that returns a `string_t *` allocates a new heap string. The
caller owns the result and must free it with `string_free()`. Functions that
modify a `string_t *` in place do not transfer ownership.

### Builder

`string_builder_t` is a `typedef` for `string_t`. It carries no extra cost;
it is a naming convention that signals incremental construction. A builder
*is* the finished string — no separate finalisation step.

### Fixed Buffers

`string_buffer_t` wraps caller-supplied stack storage for short temporary
strings without heap allocation. Its storage is deliberately hidden behind
`string_buffer_*` functions, so callers do not depend on field layout.
Appends that exceed capacity are silently truncated at a valid UTF-8
codepoint boundary.

### C String Boundaries

`string_t` is intended to be the owning text abstraction inside MARS code.
Functions such as `string_new_with()`, `string_append_cstr()` and
`string_c_str()` are boundary helpers for interoperability with legacy C APIs,
tests, examples and printing. Once text has entered a `string_t`, ordinary
string-to-string operations should be preferred so the implementation remains
free to change its storage model.

## Tradeoffs

The design favours Unicode correctness over the simplicity of treating text as
raw bytes.

---

## API Reference

All declarations are in `include/ustring.h`.

### Types

- `string_t` — opaque heap-allocated dynamic UTF-8 string
- `rune_t` — stack value for one user-visible text rune
- `string_builder_t` — alias for `string_t`, signals incremental construction
- `string_view_t` — non-owning read-only slice for string/parser internals
- `string_buffer_t` — opaque fixed-capacity buffer over caller-supplied storage
- `string_offset_t` — signed byte-offset type (`long`)

### Construction and Lifetime

- `string_t *string_new(void)` — empty string with small initial capacity
- `string_t *string_new_with(const char *init)` — copy and validate a C string
- `string_t *string_clone(const string_t *src)` — deep copy
- `void string_free(string_t *s)` — destroy and free; safe to call with NULL

### Access

- `const char *string_c_str(const string_t *s)` — boundary helper exporting transient null-terminated text, valid until next mutation
- `size_t string_count(const string_t *s)` — number of user-visible characters
- `size_t string_length(const string_t *s)` — storage byte length for low-level interop

### Modification (in-place)

- `void string_clear(string_t *s)` — reset to empty without reallocating
- `int string_append_string(string_t *s, const string_t *suffix)` — append another `string_t`
- `int string_append_cstr(string_t *s, const char *suffix)` — boundary helper for appending a C string
- `int string_append_chars(string_t *s, const char *buffer, size_t size)` — low-level helper for appending `size` raw bytes verbatim
- `int string_append_char(string_t *s, char c)` — append one ASCII character
- `int string_insert_string(string_t *s, size_t pos, const string_t *text)` — insert another `string_t` at character index `pos`
- `int string_insert(string_t *s, size_t pos, const char *text)` — boundary helper for inserting C-string text at character index `pos`
- `string_t *string_substring(const string_t *s, size_t start, size_t count)` — extract by character range
- `rune_t string_at(const string_t *s, size_t index)` — read one rune
- `string_t *rune_to_string(rune_t rune)` — copy a rune into a new `string_t`
- `bool rune_is_empty(rune_t rune)` — true for out-of-range runes
- `int string_append_rune(string_t *s, rune_t rune)` — append one rune
- `int string_each(const string_t *s, string_each_fn fn, void *user)` — iterate over characters
- `void string_trim(string_t *s)` — remove leading/trailing ASCII whitespace
- `int string_replace_string(string_t *s, const string_t *search, const string_t *replace)` — replace all non-overlapping string occurrences
- `int string_replace(string_t *s, const char *search, const char *replace)` — boundary helper for C-string search/replace

### Printf-Style Formatting

- `int string_append_format(string_t *s, const char *fmt, ...)` — append formatted text
- `int string_append_vformat(string_t *s, const char *fmt, va_list ap)` — `va_list` append variant
- `string_t *string_sprintf(const char *fmt, ...)` — create a new formatted string
- `string_t *string_vsprintf(const char *fmt, va_list ap)` — `va_list` creation variant
- `int string_printf(const char *fmt, ...)` — print formatted text to stdout

Use `%S` for `const string_t *`, `%W` for `string_view_t`, and `%R` for
`rune_t`:

```c
string_t *name = string_new_with("MARS");
string_append_format(out, "name=%S first=%R", name, string_at(name, 0));
```

### Search and Comparison

- `string_offset_t string_find_string(const string_t *s, const string_t *needle)` — byte offset of first match, or -1
- `string_offset_t string_find(const string_t *s, const char *needle)` — boundary helper for C-string needles
- `int string_compare(const string_t *a, const string_t *b)` — bytewise lexicographic order (< 0 / 0 / > 0)
- `bool string_starts_with_string(const string_t *s, const string_t *prefix)` — true if `s` begins with `prefix`
- `bool string_ends_with_string(const string_t *s, const string_t *suffix)` — true if `s` ends with `suffix`
- `bool string_starts_with(const string_t *s, const char *prefix)` — boundary helper for C-string prefixes
- `bool string_ends_with(const string_t *s, const char *suffix)` — boundary helper for C-string suffixes

### Low-Level Substring Extraction

- `string_t *string_substr(const string_t *s, size_t pos, size_t len)` — extract `len` storage bytes starting at byte offset `pos`; prefer `string_substring()` for text

### Reversal

- `void string_reverse(string_t *s)` — reverse by user-visible characters

### Split and Join

- `string_t **string_split_string(const string_t *s, const string_t *delim, size_t *out_count)` — split by an exact string delimiter into newly allocated strings
- `string_t **string_split(const string_t *s, const char *delim, size_t *out_count)` — boundary helper using C-string delimiter characters
- `void string_split_free(string_t **arr, size_t count)` — free an array from `string_split()`
- `string_t *string_join_string(string_t **arr, size_t count, const string_t *sep)` — join with a string separator into a new string
- `string_t *string_join(string_t **arr, size_t count, const char *sep)` — boundary helper for C-string separators

### Internal Views

Non-owning string views and cursors are available to the string implementation
and parser internals only. Public callers should normally work with owning
`string_t` values and the ordinary split, join, search and mutation functions
above.

### Case Conversion

- `void string_to_upper(string_t *s)` — uppercase text; Unicode-aware when built with `libunistring`, ASCII fallback otherwise
- `void string_to_lower(string_t *s)` — lowercase text; Unicode-aware when built with `libunistring`, ASCII fallback otherwise

### Hashing

- `unsigned long string_hash(const string_t *s)` — hash of the UTF-8 byte contents; suitable for hash tables

### Fixed-Capacity Buffer (`string_buffer_t`)

- `void string_buffer_init(string_buffer_t *b, char *storage, size_t capacity)` — initialise a buffer over caller-supplied storage
- `int string_buffer_append(string_buffer_t *b, const char *text)` — append; 0 if fully written, non-zero if truncated
- `int string_buffer_append_string(string_buffer_t *b, const string_t *text)` — append a `string_t`; 0 if fully written, non-zero if truncated
- `int string_buffer_append_char(string_buffer_t *b, char c)` — append one character; non-zero if buffer is full
- `const char *string_buffer_c_str(const string_buffer_t *b)` — boundary helper exporting transient null-terminated UTF-8 text
- `size_t string_buffer_length(const string_buffer_t *b)` — current byte length
- `size_t string_buffer_capacity(const string_buffer_t *b)` — total byte capacity including the terminator

### Builder API (`string_builder_t`)

`string_builder_t` is a typedef for `string_t`. All `string_*` functions work
on a builder directly. The following helpers are provided as a semantic
convenience:

- `string_builder_t *string_builder_new(void)` — `string_new()`
- `void string_builder_free(string_builder_t *b)` — `string_free(b)`
- `int string_builder_append(string_builder_t *b, const char *s)` — `string_append_cstr(b, s)`
- `int string_builder_append_string(string_builder_t *b, const string_t *s)` — `string_append_string(b, s)`
- `int string_builder_append_char(string_builder_t *b, char c)` — `string_append_char(b, c)`
- `int string_builder_format(string_builder_t *b, const char *fmt, ...)` — append formatted text, including `%S`, `%W`, and `%R`
