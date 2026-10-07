# `xml_t`

The XML module is implemented in pure C using MARS `string_t`, `dictionary_t`,
`array_t` and `file_t`. It has no external XML-library dependency.

## Representation and ownership

An opaque document contains one root element and optional comments and processing
instructions. Each element stores attributes in a `dictionary_t` whose keys and
values are `string_t *`. `xml_attributes()` exposes a borrowed read-only view:
use dictionary lookups and iteration, but do not mutate, clone for independent
ownership, or destroy that dictionary. Attribute names include qualified names
and namespace declarations such as `xmlns:p`.

Children are an ordered sequence, not a dictionary keyed by tag name: repeated
elements and mixed content must retain their order. Text, comments and processing
instructions are explicit nodes. Constructors and `xml_append_child()` copy
their inputs. Accessors return borrowed pointers; `xml_clone()` makes an
independent tree. Release trees with `xml_free()` and returned strings with
`string_free()`. Attribute pointers expire on replacement; other borrowed
pointers expire when their owning tree is destroyed.

## Input and security

The supported format is a restricted, non-validating XML 1.0 UTF-8 profile.
It supports qualified names and namespace declarations, the five predefined
entities, decimal and hexadecimal character references, comments, processing
instructions and CDATA. DTDs, custom entities, external entities and non-UTF-8
encoding declarations are rejected. It does not provide schema validation,
XPath, XInclude or network access. It is not a full conforming XML processor:
in particular, it deliberately does not accept UTF-16 or DTDs.

`xml_from_text()` and `xml_from_file()` build trees. For incremental input,
create an `xml_reader_t`, call `xml_reader_feed()` for each byte chunk, then
`xml_reader_finish()`. Chunks may end inside a UTF-8 character or XML token.
Only successful finalisation establishes that the whole document is valid.

The parser preserves Unicode spelling rather than applying NFC normalisation.
Ordinary MARS string construction can normalise text before XML sees it: use
`xml_reader_feed()` for original encoded input, or `string_append_utf8_exact()`
when assembling a source `string_t` whose exact spelling matters.

The grammar follows the applicable character, name and well-formedness rules
of [XML 1.0](https://www.w3.org/TR/xml/) within the restricted profile above.

A NULL event callback selects tree construction; afterwards,
`xml_reader_take_document()` transfers ownership once. A non-NULL callback
selects event-only processing without retaining a document tree. All callback
arguments, including attribute dictionaries, are borrowed for that invocation.
Copy anything needed afterwards. Return false to cancel. Events before an error
are provisional: applications needing transactional effects must defer committing
them until finalisation succeeds. Do not re-enter or free a reader in its callback.

Default limits are 64 MiB encoded input, 128 nested elements, and 1,000,000
element/comment/instruction nodes. Zero limit fields select defaults; nesting
cannot exceed 128. Text fragments are bounded by the input-byte limit, rather
than counted as nodes. Event mode retains open-element state and incomplete
tokens, so memory depends on nesting and token size, not just a fixed chunk.
Tree mode additionally retains the whole document. Errors are available through
`xml_reader_error()`; a failed or finished reader cannot be reused.

## Output and file I/O

`xml_write()` validates the document before invoking the destination callback,
then emits UTF-8 `string_t` fragments of at most 4096 bytes. It escapes text and
attributes and preserves character-reference distinctions needed for carriage
returns and attribute whitespace. Output failure can leave a partial stream.
Neither parsing nor writing preserves entity spelling, quote style, CDATA
delimiters, the XML declaration or empty-tag spelling. CDATA becomes text.
Whitespace inside elements is retained; insignificant whitespace outside the
root is not retained in the tree. No pretty printer is supplied because inserting
whitespace changes mixed-content XML.

File input/output uses the file module, rejects symlinks and special files, and
checks read/write/close failures. Saving validates before opening the destination
but truncates an existing file: it is not an atomic replacement operation, and an
I/O failure may leave a partial file.

Sink callbacks must not modify or destroy the source tree during writing.

`xml_serialize()` produces type `xml`, encoding `xml-utf8`, and compact UTF-8
bytes suitable for the existing generic SQLCipher object-storage interface.
It does not open a database or encrypt payloads itself. Release its labels with
`string_free()` and its byte buffer with `free()`.
`xml_deserialise()` checks both labels and parses with default limits.

## Example: dictionary lookup

Each example includes `xml.h` and `ustring.h`; assertions in the matching
README tests check the output. Production callers should check allocation and
operation failures before inspecting results.

```c
#include "xml.h"
#include "ustring.h"

int main(void)
{
    string_t *source = string_new_with("<book language='cy'>MARS</book>");
    string_t *key = string_new_with("language");
    xml_t *document = xml_from_text(source);
    const xml_t *book = xml_child_at(document, 0);
    dictionary_entry_t *entry = NULL;
    bool found = dictionary_get_entry(xml_attributes(book), &key, &entry);
    const string_t *language = found ? *(string_t *const *)dictionary_entry_value(entry) : NULL;
    string_printf("%S: %S\n", xml_name(book), language);
    xml_free(document);
    string_free(key);
    string_free(source);
    return 0;
}
```

Output:

```text
book: cy
```

## Example: build and escape XML

```c
#include "xml.h"
#include "ustring.h"

int main(void)
{
    string_t *name = string_new_with("message");
    string_t *value = string_new_with("Hello & goodbye");
    xml_t *message = xml_new_element(name);
    xml_t *text = xml_new_text(value);
    xml_append_child(message, text);
    string_t *output = xml_to_string(message);
    string_printf("%S\n", output);
    string_free(output);
    xml_free(text);
    xml_free(message);
    string_free(value);
    string_free(name);
    return 0;
}
```

Output:

```text
<message>Hello &amp; goodbye</message>
```

## Example: incremental events

This example counts elements without constructing a tree.

```c
#include "xml.h"
#include "ustring.h"

typedef struct { size_t starts, ends, texts, bytes; bool stop; } event_counts_t;

static bool count_events(xml_event_t event, const string_t *name, const string_t *text,
                         const dictionary_t *attributes, void *context)
{
    event_counts_t *counts = context;
    (void)name;
    (void)attributes;
    counts->starts += event == XML_EVENT_START;
    counts->ends += event == XML_EVENT_END;
    counts->texts += event == XML_EVENT_TEXT;
    counts->bytes += text ? string_byte_length(text) : 0;
    return !counts->stop;
}

int main(void)
{
    event_counts_t counts = {0};
    xml_reader_t *reader = xml_reader_new(NULL, count_events, &counts);
    bool ok = xml_reader_feed(reader, "<items><item", 12) &&
              xml_reader_feed(reader, "/><item/></items>", 17) && xml_reader_finish(reader);
    string_printf("elements=%zu complete=%d\n", counts.starts, (int)ok);
    xml_reader_free(reader);
    return 0;
}
```

Output:

```text
elements=3 complete=1
```

## Public API reference

All public declarations, parameter contracts and ownership rules are documented
in [xml.h](../include/xml.h).

| Function | Purpose |
| --- | --- |
| `xml_new_document()` | Receive a streaming XML event; all arguments are borrowed for the call. |
| `xml_new_element()` | Create an element with a qualified XML name. |
| `xml_new_text()` | Create a text node. |
| `xml_new_comment()` | Create a comment node. |
| `xml_new_processing_instruction()` | Create a processing instruction. |
| `xml_clone()` | Deep-copy an XML tree. |
| `xml_free()` | Release a tree and all owned descendants. |
| `xml_type()` | Read a node's type. |
| `xml_name()` | Borrow an element name or processing-instruction target. |
| `xml_text()` | Borrow the content of a text, comment or processing-instruction node. |
| `xml_attributes()` | Borrow an element's attribute dictionary. |
| `xml_set_attribute()` | Copy an attribute into an element, replacing an existing value. |
| `xml_get_attribute()` | Look up an attribute by qualified name. |
| `xml_child_count()` | Read the number of ordered children. |
| `xml_child_at()` | Borrow a child in document order. |
| `xml_append_child()` | Append a deep copy of a child without consuming it. |
| `xml_reader_new()` | Create an incremental UTF-8 XML reader. |
| `xml_reader_feed()` | Feed encoded bytes to the XML engine, allowing split UTF-8 sequences. |
| `xml_reader_finish()` | Validate end-of-input and close the document. |
| `xml_reader_take_document()` | Transfer the completed tree from a tree-building reader. |
| `xml_reader_error()` | Borrow the reader's error message. |
| `xml_reader_free()` | Release a reader and any unclaimed tree. |
| `xml_from_text()` | Parse a complete XML document using default limits. |
| `xml_from_file()` | Parse a UTF-8 file incrementally through the file module. |
| `xml_write()` | Validate and stream compact UTF-8 XML to a callback. |
| `xml_to_string()` | Serialise XML into a UTF-8 string without introducing whitespace. |
| `xml_to_file()` | Stream XML to a file through the file module. |
| `xml_serialize()` | Produce a SQLite-ready XML payload. |
| `xml_deserialise()` | Restore an XML document from a labelled payload. |
