# Regular expressions

Include `regex_t.h` for the opaque `regex_t` type. Patterns and subjects use
`string_t` from `string_t.h`. Compile once and reuse the handle for multiple
subjects; the handle does not retain input strings.

## Matching and ownership

The Linux libc engine implements POSIX extended regular expressions: leftmost,
longest matching, numbered parenthesised groups, alternation and repetition.
It is not PCRE; lookaround, named groups and lazy quantifiers are not supported.
The engine uses a private `C.UTF-8` locale and restores the calling thread's
locale after compilation and matching. No extra library dependency is needed.
Do not include the system `<regex.h>` alongside `regex_t.h`: both declare a
different `regex_t`. The adapter keeps the system type private.

Patterns and subjects must contain complete UTF-8 without NUL. Matching preserves
Unicode spelling; canonically equivalent spellings need not match. Character
classes and case-insensitive matching use `C.UTF-8`, not full Unicode case folding.
Offsets count UTF-8 bytes, not graphemes. A dot can match one Unicode scalar, not
necessarily a complete user-visible character.

`regex_options_default()` returns a policy with case-sensitive matching, multiline
disabled, a 64 KiB pattern limit, a 64 MiB subject limit and at most 1024 capture
groups. Initialise options with this function before changing individual fields:

| Field | Meaning |
| --- | --- |
| `ignore_case` | Enable locale-aware case-insensitive matching. |
| `multiline` | Make `^` and `$` line-aware; dot and negated classes exclude newline. |
| `max_pattern_bytes` | Positive pattern byte limit, at most `INT_MAX`. |
| `max_subject_bytes` | Positive subject byte limit, at most `INT_MAX`. |
| `max_groups` | Positive capture-group limit, at most 65535. |

These are input limits, **not** a timeout or a compiled-engine memory limit.
The group limit is checked after compilation. Do not accept arbitrary hostile
patterns without separate execution/resource isolation. Do not use one handle
concurrently or mutate its subject during a search.

## API

- `regex_new(pattern, options, error)` compiles a pattern, including an empty
  pattern. Pass NULL options for defaults. Returns an owned handle, or NULL with
  `errno` set.
- `regex_free(regex)` releases a handle; NULL is safe.
- `regex_group_count(regex)` counts parenthesised groups, excluding whole-match
  group zero. Returns zero for NULL.
- `regex_search(regex, subject, start, spans, count, error)` searches at or after
  a UTF-8 byte offset. The offset must be a scalar boundary, including EOF.
  It does not create a new beginning of line. Returns `REGEX_MATCH`,
  `REGEX_NO_MATCH` or `REGEX_ERROR`; only the last indicates failure.
- `regex_test(regex, subject, error)` searches from zero without captures.
- `regex_capture(subject, span)` copies a participating capture exactly. An empty
  capture produces an owned empty string. An absent group returns NULL with
  `errno=0`; invalid arguments or allocation failure return NULL with `errno` set.

`regex_span_t` contains `start`, exclusive `end` and `matched`. Group zero is the
whole match; subsequent indices correspond to opening parentheses. Supply up to
`max_groups+1` entries, or NULL and zero to discard captures. Valid output arrays
are cleared before searching; non-participating and excess groups remain
`matched=false`, with zero offsets. Returned offsets always refer to the original
subject. Free copied captures with `string_free()`.

For functions taking `error`, supply NULL or an empty `string_t *` output slot.
It becomes NULL on success/no match, or an owned diagnostic on failure if memory
permits. Free it before reusing the slot. Invalid arguments use `EINVAL`, malformed
UTF-8/NUL uses `EILSEQ`, configured size limits use `EOVERFLOW`, and allocation
failure uses `ENOMEM`. A failed search is not equivalent to no match.

When iterating after an empty match, advance past one complete Unicode scalar
using a string view, or stop at EOF. Reusing the empty match's end offset without
advancing would loop forever.

## Capture UTF-8 text

```c
#include "regex_t.h"

int main(void)
{
    string_t *pattern = string_new_with("([[:alpha:]]+)=([0-9]+)");
    string_t *subject = string_new_with("Zoë=42");
    regex_t *regex = regex_new(pattern, NULL, NULL);
    regex_span_t spans[3];
    bool ok = regex && subject && regex_search(regex, subject, 0, spans, 3, NULL) == REGEX_MATCH;
    string_t *name = ok ? regex_capture(subject, &spans[1]) : NULL;
    string_t *value = ok ? regex_capture(subject, &spans[2]) : NULL;
    ok = name && value;
    if (ok)
        string_printf("%S: %S\n", name, value);
    string_free(value);
    string_free(name);
    regex_free(regex);
    string_free(subject);
    string_free(pattern);
    return ok ? 0 : 1;
}
```

Output:

```text
Zoë: 42
```

## Reuse a compiled pattern

This pattern cannot match empty text, so advancing to each match's end is safe.

```c
#include "regex_t.h"

int main(void)
{
    string_t *pattern = string_new_with("[0-9]+");
    string_t *subject = string_new_with("item 12, item 34");
    regex_t *regex = regex_new(pattern, NULL, NULL);
    regex_status_t status = REGEX_ERROR;
    regex_span_t span;
    size_t start = 0;
    if (regex && subject) {
        while ((status = regex_search(regex, subject, start, &span, 1, NULL)) == REGEX_MATCH) {
            string_t *value = regex_capture(subject, &span);
            if (!value) {
                status = REGEX_ERROR;
                break;
            }
            string_printf("%S\n", value);
            string_free(value);
            start = span.end;
        }
    }
    regex_free(regex);
    string_free(subject);
    string_free(pattern);
    return status == REGEX_NO_MATCH ? 0 : 1;
}
```

Output:

```text
12
34
```

Both examples run last in `tests/regex/test_regex.c` and are compiled and run by
`make test-readme-examples`. See the
[Linux regex API](https://man7.org/linux/man-pages/man3/regcomp.3.html) for engine
semantics and the [string guide](string.md) for exact Unicode construction.
