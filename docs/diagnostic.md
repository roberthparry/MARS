# Structured diagnostics

Include `diagnostic-t.h` for the opaque `diagnostic_t` type. A diagnostic is an
immutable record containing a severity, application-defined domain and numeric
code, UTF-8 message, optional source location and optional cause. It provides
owned error information that can cross native thread boundaries; unlike `errno`,
it is not tied to the thread that detected the problem.

[`executor_t`](executor.md) uses these records for task errors and optional
successful-task warnings. This module does not log, throw exceptions, interpret
mathematics or change existing modules' error APIs.

Production adapters include [`csv_diagnostic()`](csv.md),
[`sqlite_diagnostic()` and `sqlite_stmt_diagnostic()`](sqlite.md), and native
[MARS Lab TeX rendering](mars-lab.md). CSV ingestion preserves the parser's
location as a cause beneath the database operation. Existing error text and
allocation-free reports remain available alongside these retained records.

Native Lab process and request adapters also use these records. Process snapshots
preserve errno and child-exit causes without copying command arguments or input.
The Lab explicitly projects outcome records into its Protobuf response envelope;
the diagnostic module itself does not define a wire format. See the
[Lab failure-response contract](mars-lab.md) for domains, bounded causes and
the distinction between safe metadata and potentially sensitive raw output.

## API and ownership

| API | Behaviour |
| --- | --- |
| `diagnostic_new(severity, domain, code, message, location, cause)` | Copy text and location; retain the optional cause. Domain and message must be non-empty. |
| `diagnostic_retain(diagnostic)` | Obtain another owning reference to the same immutable record. |
| `diagnostic_free(diagnostic)` | Release one reference, accepting NULL and preserving `errno`. |
| `diagnostic_severity(diagnostic)` | Read `DIAGNOSTIC_NOTE`, `DIAGNOSTIC_WARNING` or `DIAGNOSTIC_ERROR`. |
| `diagnostic_domain(diagnostic)` | Borrow the domain string. |
| `diagnostic_code(diagnostic)` | Read the signed 64-bit domain-specific code. |
| `diagnostic_message(diagnostic)` | Borrow the message string. |
| `diagnostic_location(diagnostic)` | Borrow the optional location, or NULL if none was supplied. |
| `diagnostic_cause(diagnostic)` | Borrow the optional cause. |
| `diagnostic_depth(diagnostic)` | Read the cached chain depth, including this record. |
| `diagnostic_to_string(diagnostic)` | Allocate plain-text output for the whole chain; release it with `string_free()`. |

Constructor inputs remain caller-owned. Strings follow `string_t` UTF-8
normalisation. Accessor results stay valid while an owning diagnostic reference
remains alive; do not modify or free borrowed data. Atomic reference counting
permits concurrent retain/release and readers, but freeing the very reference
another thread is using without coordination is not safe. No user callback runs
inside the module. Immutable construction prevents cause cycles.

`diagnostic_location_t` uses zero-based byte `offset` and `length`. A zero length
represents a point. `line` and `column` are optional one-based coordinates; zero
means unknown and a known column requires a known line. The caller supplies
coordinates: the module does not read files or derive positions from UTF-8 text.
A zero-valued location is distinct from no location. `source` is an optional
copied name; source contents are never retained.

Invalid arguments fail with `EINVAL`, byte-extent/reference overflow with
`EOVERFLOW`, allocation failures with `ENOMEM`. Chains have at most 64 records;
adding another fails with `ELOOP`. Release is iterative. NULL getters report
`EINVAL` and return NULL, zero or `DIAGNOSTIC_INVALID` as appropriate. Missing
optional fields do not indicate errors. Successful calls need not clear `errno`.

Formatting uses direct severity lookup and traverses each cause once. It returns
plain text without a trailing newline, not HTML, TeX, JSON or a wire format.
Applications must escape it for their destination. Messages may contain
newlines; this is not a sanitising log encoder. Machine consumers should use the
structured accessors instead of parsing presentation text.

## A located cause

```c
#include <stdbool.h>
#include <stdio.h>

#include "diagnostic-t.h"

int main(void)
{
    diagnostic_location_t location = {.source = "input", .offset = 4, .length = 1, .line = 1, .column = 5};
    diagnostic_t *cause = diagnostic_new(DIAGNOSTIC_ERROR, "parser", 7, "Expected a value", &location, NULL);
    diagnostic_t *error = cause ? diagnostic_new(DIAGNOSTIC_ERROR, "request", 2, "Cannot evaluate", NULL, cause) : NULL;
    diagnostic_free(cause);
    string_t *text = error ? diagnostic_to_string(error) : NULL;
    if (text)
        printf("%s\n", string_c_str(text));
    bool ok = text != NULL;
    string_free(text);
    diagnostic_free(error);
    return ok ? 0 : 1;
}
```

Output:

```text
error [request:2]: Cannot evaluate
Caused by: error [parser:7]: Expected a value (input:1:5; bytes 4+1)
```

Mirrored as a README example in `tests/diagnostic/test_diagnostic.c`.
