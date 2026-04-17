# `string_t`

`string_t` is a UTF-8-aware dynamic string type with Unicode-oriented
operations at three levels of granularity: byte, codepoint, and grapheme
cluster.

## Capabilities

- heap-allocated dynamic string with automatic capacity growth
- append, insert, trim, replace, split, and join
- UTF-8 validation on construction; invalid bytes replaced with U+FFFD
- codepoint-aware case conversion (upper/lower) and reversal
- grapheme cluster count, index access, substring extraction, and reversal (UAX #29)
- Unicode normalisation: NFC, NFD, NFKC, NFKD (UAX #15)
- non-owning `string_view_t` for zero-copy slicing
- fixed-capacity `string_buffer_t` for stack allocation
- `string_builder_t` alias for incremental construction
- hashing

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
    printf("%s\n", buf.data);   /* "Hello!" */
    return 0;
}
```

## Design Notes

### Three Levels of Granularity

| Level | Unit | API prefix |
|---|---|---|
| Byte | raw UTF-8 bytes | `string_length`, `string_substr`, `string_reverse` |
| Codepoint | Unicode scalar values | `string_utf8_length`, `string_utf8_reverse` |
| Grapheme | user-perceived characters (UAX #29) | `string_grapheme_*` |

Byte-level operations are fastest but not safe for multi-byte content. For
most text, codepoint-level is correct. For emoji, Indic scripts, and flag
sequences, grapheme-level is required.

### Grapheme Cluster Boundaries

`string_grapheme_next()` implements the UAX #29 grapheme cluster break
algorithm, including:
- CR+LF pairs (GB3)
- Extend and SpacingMark sequences (GB9/GB9a) — combining marks
- ZWJ emoji sequences (GB11) — e.g. 👨‍👩‍👧‍👦
- Regional indicator pairs (GB12/GB13) — flag emoji

Walking backwards is done by forward-scanning from a safe anchor point
because ZWJ and regional indicator sequences cannot be reliably decomposed by
walking backwards alone.

### Normalisation

Normalisation requires the `libunistring` library (`HAVE_UNISTRING` build flag).
When built without it, `string_normalize()` is a no-op that returns 0 for
recognised forms.

### Ownership

Every function that returns a `string_t *` allocates a new heap string. The
caller owns the result and must free it with `string_free()`. Functions that
modify a `string_t *` in place do not transfer ownership.

### Builder

`string_builder_t` is a `typedef` for `string_t`. It carries no extra cost;
it is a naming convention that signals incremental construction. A builder
*is* the finished string — no separate finalization step.

### Fixed Buffers

`string_buffer_t` wraps caller-supplied stack storage for short temporary
strings without heap allocation. Appends that exceed capacity are silently
truncated at a valid UTF-8 codepoint boundary.

## Tradeoffs

The design favours Unicode correctness over the simplicity of treating text as
raw bytes.

---

## API Reference

All declarations are in `include/ustring.h`.

### Types

- `string_t` — opaque heap-allocated dynamic UTF-8 string
- `string_builder_t` — alias for `string_t`, signals incremental construction
- `string_view_t` — non-owning read-only slice: `{ const char *data; size_t len; }`
- `string_buffer_t` — fixed-capacity buffer: `{ char *data; size_t len; size_t cap; }`
- `string_offset_t` — signed byte-offset type (`long`)

### Construction and Lifetime

- `string_t *string_new(void)` — empty string with small initial capacity
- `string_t *string_new_with(const char *init)` — copy and validate a C string
- `string_t *string_clone(const string_t *src)` — deep copy
- `void string_free(string_t *s)` — destroy and free; safe to call with NULL

### Access

- `const char *string_c_str(const string_t *s)` — null-terminated UTF-8 buffer (valid until next mutation)
- `size_t string_length(const string_t *s)` — byte length (not codepoints or graphemes)

### Modification (in-place)

- `void string_clear(string_t *s)` — reset to empty without reallocating
- `int string_append_cstr(string_t *s, const char *suffix)` — append a C string; 0 on success
- `int string_append_chars(string_t *s, const char *buffer, size_t size)` — append `size` raw bytes verbatim
- `int string_append_char(string_t *s, char c)` — append one ASCII character
- `int string_insert(string_t *s, size_t pos, const char *text)` — insert at byte offset `pos` (must be a codepoint boundary)
- `void string_trim(string_t *s)` — remove leading/trailing ASCII whitespace
- `int string_replace(string_t *s, const char *search, const char *replace)` — replace all non-overlapping occurrences; returns count or negative on failure

### Printf-Style Append

- `int string_printf(string_t *s, const char *fmt, ...)` — append formatted text; returns bytes appended or negative on error
- `int string_vprintf(string_t *s, const char *fmt, va_list ap)` — `va_list` variant
- `int string_append_format(string_t *s, const char *fmt, ...)` — alias for `string_printf`

### Search and Comparison

- `string_offset_t string_find(const string_t *s, const char *needle)` — byte offset of first match, or -1
- `int string_compare(const string_t *a, const string_t *b)` — bytewise lexicographic order (< 0 / 0 / > 0)
- `bool string_starts_with(const string_t *s, const char *prefix)` — true if `s` begins with `prefix`
- `bool string_ends_with(const string_t *s, const char *suffix)` — true if `s` ends with `suffix`

### Substring Extraction

- `string_t *string_substr(const string_t *s, size_t pos, size_t len)` — extract `len` bytes starting at byte offset `pos`

### Reversal

- `void string_reverse(string_t *s)` — **bytewise** reversal; safe for ASCII only
- `void string_utf8_reverse(string_t *s)` — reverse by codepoints; safe for most Latin/CJK
- `void string_grapheme_reverse(string_t *s)` — reverse by grapheme clusters; Unicode-correct for all scripts

### Split and Join

- `string_t **string_split(const string_t *s, const char *delim, size_t *out_count)` — split into newly allocated strings; free with `string_split_free()`
- `void string_split_free(string_t **arr, size_t count)` — free an array from `string_split()`
- `string_t *string_join(string_t **arr, size_t count, const char *sep)` — join with separator into a new string

### Views

- `string_view_t string_view(const string_t *s, size_t pos, size_t len)` — create a non-owning view into `s`; valid until `s` is mutated or freed
- `int string_view_equals(const string_view_t *v, const char *cstr)` — non-zero if view contents equal `cstr`
- `string_t *string_from_view(const string_view_t *v)` — copy a view into a new string
- `string_view_t *string_split_view(const string_t *s, const char *delim, size_t *out_count)` — split into views (no allocation of strings); free array with `string_split_view_free()`
- `void string_split_view_free(string_view_t *views)` — free the view array (does not free string data)

### Codepoint Utilities

- `size_t utf8_next(const char *s, size_t len, size_t i)` — advance to next codepoint boundary
- `size_t string_utf8_prev(const char *s, size_t len, size_t i)` — retreat to previous codepoint boundary
- `size_t string_utf8_length(const string_t *s)` — number of Unicode codepoints
- `void string_utf8_to_upper(string_t *s)` — Unicode uppercase (all scripts)
- `void string_utf8_to_lower(string_t *s)` — Unicode lowercase (all scripts)

### ASCII Case Conversion

- `void string_to_upper(string_t *s)` — ASCII A–Z only
- `void string_to_lower(string_t *s)` — ASCII a–z only

### Grapheme Cluster Utilities

**Grapheme break class** (`grapheme_class_t`):

| Value | Meaning |
|---|---|
| `GB_Other` | Any other codepoint |
| `GB_CR` | U+000D carriage return |
| `GB_LF` | U+000A line feed |
| `GB_Control` | Other control characters |
| `GB_Extend` | Combining and enclosing marks |
| `GB_ZWJ` | U+200D zero-width joiner |
| `GB_Regional_Indicator` | Regional indicator symbols (flag emoji) |
| `GB_Extended_Pictographic` | Emoji and pictographics |
| `GB_SpacingMark` | Spacing combining marks (Indic scripts) |

- `grapheme_class_t string_grapheme_class(uint32_t cp)` — classify a codepoint by UAX #29 break class
- `size_t string_grapheme_next(const char *s, size_t len, size_t i)` — advance to next grapheme cluster boundary
- `size_t string_grapheme_prev(const char *s, size_t len, size_t i)` — retreat to previous grapheme cluster boundary
- `size_t string_grapheme_count(const string_t *s)` — number of grapheme clusters
- `string_t *string_grapheme_substr(const string_t *s, size_t gpos, size_t glen)` — extract `glen` graphemes starting at grapheme index `gpos`
- `string_t *string_grapheme_at(const string_t *s, size_t index)` — extract a single grapheme cluster; NULL on out-of-range

### Hashing

- `unsigned long string_hash(const string_t *s)` — hash of the UTF-8 byte contents; suitable for hash tables

### Fixed-Capacity Buffer (`string_buffer_t`)

- `void string_buffer_init(string_buffer_t *b, char *storage, size_t capacity)` — initialise a buffer over caller-supplied storage
- `int string_buffer_append(string_buffer_t *b, const char *text)` — append; 0 if fully written, non-zero if truncated
- `int string_buffer_append_char(string_buffer_t *b, char c)` — append one character; non-zero if buffer is full

### Builder API (`string_builder_t`)

`string_builder_t` is a typedef for `string_t`. All `string_*` functions work
on a builder directly. The following inline helpers are provided as a
semantic convenience:

- `string_builder_t *string_builder_new(void)` — `string_new()`
- `void string_builder_free(string_builder_t *b)` — `string_free(b)`
- `int string_builder_append(string_builder_t *b, const char *s)` — `string_append_cstr(b, s)`
- `int string_builder_append_char(string_builder_t *b, char c)` — `string_append_char(b, c)`
- `int string_builder_format(string_builder_t *b, const char *fmt, ...)` — `string_vprintf(b, fmt, ...)`

### Unicode Normalisation (`string_norm_form_t`)

| Form | Decomposition | Composition | Use case |
|---|---|---|---|
| `STRING_NORM_NFC` | Canonical | Yes | Storage and interchange (recommended) |
| `STRING_NORM_NFD` | Canonical | No | Accent stripping, low-level processing |
| `STRING_NORM_NFKC` | Compatibility | Yes | Security normalisation, search |
| `STRING_NORM_NFKD` | Compatibility | No | Indexing, ignoring stylistic variants |

- `int string_normalize(string_t *s, string_norm_form_t form)` — normalise in place. Requires `libunistring` (`HAVE_UNISTRING`). Returns 0 on success, non-zero on error. No-op when built without `libunistring`.
