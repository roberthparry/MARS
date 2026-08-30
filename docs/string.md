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
#include "ustring.h"

int main(void) {
    string_t *s = string_new_with("Héllo");

    string_append_cstr(s, " 🌍");
    string_insert(s, 1, "🙂");

    string_printf("%S\n", s);

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
#include "ustring.h"

int main(void) {
    string_t *s = string_new_with("👨‍👩‍👧‍👦 café 🇬🇧");

    rune_t first = string_at(s, 0);
    string_t *first_text = rune_to_string(first);
    string_t *word = string_substring(s, 2, 4);

    string_printf("Characters: %zu\n", string_length(s));
    string_printf("First: %S\n", first_text);
    string_printf("Word: %S\n", word);

    string_reverse(s);
    string_printf("Reversed: %S\n", s);

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

## Example: Escaping Wide C Strings

```c
#include "ustring.h"

int main(void) {
    string_t *s = string_new_wide(L"hello π");

    string_printf("%S\n", s);

    string_free(s);
    return 0;
}
```

Expected output:

```text
hello π
```

## Example: Character Iteration

```c
#include "ustring.h"

int main(void) {
    string_t *s = string_new_with("👨‍👩‍👧‍👦 family");

    size_t count = string_length(s);
    string_printf("Characters: %zu\n", count);

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

## Example: Using `string_t` With a Parser

`string_cursor_t` is the parser-facing companion to `string_t`. It lets a
parser move through text, remember positions, classify runes, and copy matched
slices without inspecting raw storage.

```c
#include <stdbool.h>
#include "ustring.h"

static bool name_rune(rune_t rune) {
    return rune_is_alpha_numeric(rune) || rune_is_equal(rune, '_');
}

typedef enum {
    TOKEN_ID,
    TOKEN_NUMBER,
    TOKEN_OPERATOR
} token_kind_t;

static void append_token(string_t *out, token_kind_t kind, const string_t *text) {
    if (string_length(out) > 0)
        string_append_cstr(out, " ");

    switch (kind) {
        case TOKEN_ID:
            string_append_cstr(out, "id[");
            break;
        case TOKEN_NUMBER:
            string_append_cstr(out, "number[");
            break;
        case TOKEN_OPERATOR:
            string_append_cstr(out, "op[");
            break;
    }

    string_append_format(out, "%S]", text);
}

static bool read_name(string_cursor_t *cursor, string_t *out) {
    string_pos_t start = string_cursor_position(cursor);
    rune_t rune = string_cursor_peek(cursor);
    string_t *token;

    if (!name_rune(rune) || rune_is_digit(rune))
        return false;

    do {
        string_cursor_next(cursor);
        rune = string_cursor_peek(cursor);
    } while (!rune_is_none(rune) && name_rune(rune));

    token = string_cursor_extract(start, cursor);
    append_token(out, TOKEN_ID, token);
    string_free(token);
    return true;
}

static bool read_number(string_cursor_t *cursor, string_t *out) {
    string_pos_t start = string_cursor_position(cursor);
    rune_t rune = string_cursor_peek(cursor);
    string_t *token;

    if (!rune_is_digit(rune))
        return false;

    do {
        string_cursor_next(cursor);
        rune = string_cursor_peek(cursor);
    } while (!rune_is_none(rune) &&
             (rune_is_digit(rune) || rune_is_equal(rune, '/')));

    token = string_cursor_extract(start, cursor);
    append_token(out, TOKEN_NUMBER, token);
    string_free(token);
    return true;
}

static void read_operator(string_cursor_t *cursor, string_t *out) {
    string_t *token = rune_to_string(string_cursor_peek(cursor));

    append_token(out, TOKEN_OPERATOR, token);
    string_free(token);
    string_cursor_next(cursor);
}

int main(void) {
    string_t *source = string_new_with("cdf(α_1) + 355/113");
    string_cursor_t *cursor = string_cursor_new(source);
    string_t *tokens = string_new();

    while (!string_cursor_done(cursor)) {
        string_cursor_skip_spaces(cursor);
        if (string_cursor_done(cursor))
            break;
        if (read_name(cursor, tokens))
            continue;
        if (read_number(cursor, tokens))
            continue;
        read_operator(cursor, tokens);
    }

    string_printf("%S\n", tokens);

    string_free(tokens);
    string_cursor_free(cursor);
    string_free(source);
    return 0;
}
```

Expected output:

```text
id[cdf] op[(] id[α_1] op[)] op[+] number[355/113]
```

## Example: Fixed-Capacity Buffer

```c
#include "ustring.h"

int main(void) {
    char storage[64];
    string_buffer_t buf;
    string_buffer_init(&buf, storage, sizeof(storage));
    string_buffer_append(&buf, "Hello");
    string_buffer_append_char(&buf, '!');
    string_printf("%s\n", string_buffer_c_str(&buf));
    return 0;
}
```

Expected output:

```text
Hello!
```

## Design Notes

### Characters, Not Storage Units

The simple API works in user-visible characters. `string_length()`,
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
Appends that exceed capacity are silently truncated at a valid character
boundary.

### C String Boundaries

`string_t` is intended to be the owning text abstraction inside MARS code.
Functions such as `string_new_with()`, `string_append_cstr()` and
`string_c_str()` are boundary helpers for interoperability with legacy C APIs,
tests, examples and printing. Once text has entered a `string_t`, ordinary
string-to-string operations should be preferred so the implementation remains
free to change its storage model.

### Parser Use

Use `string_cursor_t` when code needs to read text progressively. A cursor
borrows a `string_t` or `string_view_t`, exposes the current rune, supports
position save/restore, and can copy or borrow matched slices. That keeps the
parser focused on grammar rules while the string module owns storage,
normalisation and rune boundaries.

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
- `string_offset_t` — signed encoded-position result type (`long`)
- `string_cursor_t` — opaque cursor for parser-style reading
- `string_pos_t` — saved cursor position used for seeking and slicing

### Construction and Lifetime

- `string_t *string_new(void)` — empty string with small initial capacity
- `string_t *string_new_with(const char *init)` — copy and validate a C string
- `string_t *string_new_wide(const wchar_t *init)` — boundary helper for copying `L"..."` wide C strings into `string_t`
- `string_t *string_clone(const string_t *src)` — deep copy
- `void string_free(string_t *s)` — destroy and free; safe to call with NULL

### Access

- `const char *string_c_str(const string_t *s)` — boundary helper exporting transient null-terminated text, valid until next mutation
- `size_t string_length(const string_t *s)` — number of user-visible characters

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
- `rune_t rune_from_ascii(char c)` — stack rune for one ASCII character
- `uint32_t rune_value(rune_t rune)` — numeric rune value, useful for mathematical symbols
- `bool rune_to_ascii(rune_t rune, char *out)` — export a rune as ASCII when possible
- `bool rune_is_none(rune_t rune)` — true when a rune value represents no character
- `bool rune_is_equal(rune_t rune, char ch)` — compare a rune with one ASCII character
- `bool rune_is_digit(rune_t rune)` — true for decimal digit runes
- `bool rune_is_alpha_numeric(rune_t rune)` — true for alphabetic or decimal digit runes
- `string_t *rune_to_string(rune_t rune)` — copy a rune into a new `string_t`
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

- `string_offset_t string_find_string(const string_t *s, const string_t *needle)` — encoded position of first match, or -1
- `string_offset_t string_find(const string_t *s, const char *needle)` — boundary helper for C-string needles
- `int string_compare(const string_t *a, const string_t *b)` — canonical encoded lexicographic order (< 0 / 0 / > 0)
- `bool string_starts_with_string(const string_t *s, const string_t *prefix)` — true if `s` begins with `prefix`
- `bool string_ends_with_string(const string_t *s, const string_t *suffix)` — true if `s` ends with `suffix`
- `bool string_starts_with(const string_t *s, const char *prefix)` — boundary helper for C-string prefixes
- `bool string_ends_with(const string_t *s, const char *suffix)` — boundary helper for C-string suffixes

### Low-Level Substring Extraction

- `string_t *string_substr(const string_t *s, size_t pos, size_t len)` — extract an encoded range; prefer `string_substring()` for text

### Reversal

- `void string_reverse(string_t *s)` — reverse by user-visible characters

### Split and Join

- `string_t **string_split_string(const string_t *s, const string_t *delim, size_t *out_count)` — split by an exact string delimiter into newly allocated strings
- `string_t **string_split(const string_t *s, const char *delim, size_t *out_count)` — boundary helper using C-string delimiter characters
- `void string_split_free(string_t **arr, size_t count)` — free an array from `string_split()`
- `string_t *string_join_string(string_t **arr, size_t count, const string_t *sep)` — join with a string separator into a new string
- `string_t *string_join(string_t **arr, size_t count, const char *sep)` — boundary helper for C-string separators

### Views and Cursor Parsing

- `string_view_t string_view_all(const string_t *s)` — borrow a view over a whole string
- `string_view_t string_view(const string_t *s, size_t pos, size_t len)` — borrow a view over an encoded range
- `string_view_t string_view_slice(string_view_t view, size_t pos, size_t len)` — borrow a sub-view
- `string_t *string_from_view(const string_view_t *v)` — copy a view into an owning string
- `size_t string_view_length(string_view_t view)` — encoded length of a view
- `int string_view_is_empty(string_view_t view)` — true when a view is empty
- `int string_view_equals_view(string_view_t a, string_view_t b)` — byte-for-byte view equality
- `string_view_t string_view_trim(string_view_t view)` — trim whitespace from a view
- `bool string_view_equals_literal(string_view_t view, const char *literal)` — boundary helper for comparing with typed text
- `bool string_view_starts_with(string_view_t view, const string_t *literal, bool case_insensitive)` — prefix test using a string
- `bool string_view_starts_with_view(string_view_t view, string_view_t literal, bool case_insensitive)` — prefix test using another view
- `string_cursor_t *string_cursor_new(const string_t *s)` — create a cursor over a string
- `string_cursor_t *string_cursor_new_view(string_view_t view)` — create a cursor over a borrowed view
- `string_cursor_t *string_cursor_clone(const string_cursor_t *cursor)` — clone a cursor for speculative parsing
- `void string_cursor_free(string_cursor_t *cursor)` — free a cursor
- `bool string_cursor_done(const string_cursor_t *cursor)` — true at end of input
- `string_pos_t string_cursor_position(const string_cursor_t *cursor)` — current cursor position
- `int string_cursor_seek(string_cursor_t *cursor, string_pos_t pos)` — return to a saved position
- `rune_t string_cursor_peek(const string_cursor_t *cursor)` — current rune without advancing
- `int string_cursor_next(string_cursor_t *cursor)` — advance by one rune
- `bool string_cursor_match(const string_cursor_t *cursor, const char *literal)` — boundary helper for matching typed literal text
- `bool string_cursor_consume(string_cursor_t *cursor, const char *literal)` — match and advance over typed literal text
- `void string_cursor_skip_spaces(string_cursor_t *cursor)` — skip whitespace runes
- `string_t *string_cursor_slice_between(string_pos_t start, string_pos_t end, const string_cursor_t *cursor)` — copy a matched slice
- `string_t *string_cursor_extract(string_pos_t start, const string_cursor_t *cursor)` — copy from a saved position to the current cursor position
- `int string_cursor_append_slice_between(string_t *out, string_pos_t start, string_pos_t end, const string_cursor_t *cursor)` — append a matched slice
- `string_view_t string_cursor_view_between(string_pos_t start, string_pos_t end, const string_cursor_t *cursor)` — borrow a matched slice
- `string_view_t string_cursor_view_extract(string_pos_t start, const string_cursor_t *cursor)` — borrow from a saved position to the current cursor position

### Case Conversion

- `void string_to_upper(string_t *s)` — uppercase text; Unicode-aware when built with `libunistring`, ASCII fallback otherwise
- `void string_to_lower(string_t *s)` — lowercase text; Unicode-aware when built with `libunistring`, ASCII fallback otherwise

### Hashing

- `unsigned long string_hash(const string_t *s)` — hash of the canonical encoded contents; suitable for hash tables

### Fixed-Capacity Buffer (`string_buffer_t`)

- `void string_buffer_init(string_buffer_t *b, char *storage, size_t capacity)` — initialise a buffer over caller-supplied storage
- `int string_buffer_append(string_buffer_t *b, const char *text)` — append; 0 if fully written, non-zero if truncated
- `int string_buffer_append_string(string_buffer_t *b, const string_t *text)` — append a `string_t`; 0 if fully written, non-zero if truncated
- `int string_buffer_append_char(string_buffer_t *b, char c)` — append one character; non-zero if buffer is full
- `const char *string_buffer_c_str(const string_buffer_t *b)` — boundary helper exporting transient null-terminated UTF-8 text
- `size_t string_buffer_length(const string_buffer_t *b)` — current encoded length
- `size_t string_buffer_capacity(const string_buffer_t *b)` — total encoded capacity including the terminator

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

### `rune_from_value()`

Creates or reconstructs the public value described by from value.

```c
rune_t rune_from_value(uint32_t value);
```

### `rune_is_control()`

Reports whether the condition described by is control holds.

```c
bool rune_is_control(rune_t rune);
```

### `rune_is_fraction()`

Reports whether the condition described by is fraction holds.

```c
bool rune_is_fraction(rune_t rune);
```

### `string_append_vformat_with_callback()`

Returns the public result described by append vformat with callback.

```c
int string_append_vformat_with_callback(string_t *s, const char *fmt, va_list ap, string_format_callback_t callback, void *user);
```

### `string_byte_length()`

Returns the public result described by byte length.

```c
size_t string_byte_length(const string_t *s);
```

### `string_cursor_end_position()`

Returns the public result described by cursor end position.

```c
string_pos_t string_cursor_end_position(const string_cursor_t *cursor);
```

### `string_cursor_match_at()`

Reports whether the condition described by cursor match at holds.

```c
bool string_cursor_match_at(const string_cursor_t *cursor, string_pos_t pos, const char *literal);
```

### `string_cursor_peek_ascii()`

Reports whether the condition described by cursor peek ascii holds.

```c
bool string_cursor_peek_ascii(const string_cursor_t *cursor, unsigned char *out);
```

### `string_cursor_peek_ascii_at()`

Reports whether the condition described by cursor peek ascii at holds.

```c
bool string_cursor_peek_ascii_at(const string_cursor_t *cursor, string_pos_t pos, unsigned char *out);
```

### `string_cursor_peek_at()`

Returns the public result described by cursor peek at.

```c
rune_t string_cursor_peek_at(const string_cursor_t *cursor, string_pos_t pos);
```

### `string_cursor_skip()`

Reports whether the condition described by cursor skip holds.

```c
bool string_cursor_skip(string_cursor_t *cursor, string_pos_t span);
```

### `string_fprintf()`

Returns the public result described by fprintf.

```c
int string_fprintf(FILE *stream, const char *fmt, ...);
```

### `string_view_empty()`

Returns the public result described by view empty.

```c
string_view_t string_view_empty(void);
```

### `string_view_peek_ascii()`

Reports whether the condition described by view peek ascii holds.

```c
bool string_view_peek_ascii(string_view_t view, string_pos_t pos, unsigned char *out);
```

### `string_view_peek_rune_value()`

Reports whether the condition described by view peek rune value holds.

```c
bool string_view_peek_rune_value(string_view_t view, string_pos_t pos, uint32_t *out, string_pos_t *next_pos_out);
```

### `string_vsprintf_with_callback()`

Returns the public result described by vsprintf with callback.

```c
string_t *string_vsprintf_with_callback(const char *fmt, va_list ap, string_format_callback_t callback, void *user);
```
