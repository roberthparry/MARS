/**
 * @file xml.h
 * @brief Opaque XML trees and incremental UTF-8 input/output.
 *
 * Use xml_t to construct, inspect and serialise XML documents, with incremental
 * readers and writers for bounded streaming input and output. Dictionary-backed
 * attributes and ordered child nodes bridge XML data to the existing MARS
 * containers while retaining mixed text and element content.
 *
 * This native parser requires no external XML parsing library. It is suitable for
 * document interchange and the XML representation of SOAP payloads; http.h supplies
 * the SOAP and network protocol handling. It is not a validating XML processor or
 * a byte-for-byte document preservation tool.
 *
 * Attributes use dictionary_t with string_t * keys and values. Ordered children retain repeated elements and mixed
 * content. CDATA is normalised to text; entity spellings and the XML declaration are not retained.
 * DTDs and external entities are rejected. No schema validation, XPath, XInclude or network access is performed.
 * Use separate readers in different threads; a reader, its callback and a tree must not be used concurrently.
 */

#ifndef MARS_XML_H
#define MARS_XML_H

#include <stdbool.h>
#include <stddef.h>
#include "dictionary.h"
#include "ustring.h"

/** @brief Opaque owned XML node. */
typedef struct _xml_t xml_t;

/** @brief Opaque incremental parser. */
typedef struct _xml_reader_t xml_reader_t;

/** @brief XML node categories. */
typedef enum { XML_INVALID = -1, XML_DOCUMENT, XML_ELEMENT, XML_TEXT, XML_COMMENT, XML_PROCESSING_INSTRUCTION } xml_type_t;

/** @brief Streaming event categories; character data may arrive in several adjacent events. */
typedef enum { XML_EVENT_START, XML_EVENT_END, XML_EVENT_TEXT, XML_EVENT_COMMENT, XML_EVENT_PI } xml_event_t;

/** @brief Resource limits; zero fields select the documented defaults. */
typedef struct {
    size_t max_bytes; /**< Encoded input limit; default 64 MiB. */
    size_t max_depth; /**< Element nesting limit; default and maximum 128. */
    size_t max_nodes; /**< Element, comment and instruction count limit; default 1000000. */
} xml_limits_t;

/**
 * @brief Receive a streaming XML event; all arguments are borrowed for the call.
 * @param event Event category.
 * @param name Qualified name for start/end elements and processing instructions; NULL otherwise.
 * @param text Content for text, comments and processing instructions; NULL otherwise.
 * @param attributes Attribute dictionary for start events; NULL otherwise. Keys and values store string_t *.
 * @param user_data Caller-supplied context.
 * @return True to continue, false to stop with a reader error. Do not re-enter or destroy the reader in a callback.
 */
typedef bool (*xml_event_fn)(xml_event_t event, const string_t *name, const string_t *text,
                             const dictionary_t *attributes, void *user_data);

/**
 * @brief Consume a UTF-8 output fragment.
 * @param text Borrowed fragment; valid for this call only.
 * @param user_data Caller-supplied context.
 * @return True if consumed completely, false to stop.
 */
typedef bool (*xml_write_fn)(const string_t *text, void *user_data);

/**
 * @brief Create an empty document.
 * @return Owned document, or NULL on allocation failure; release with xml_free.
 */
xml_t *xml_new_document(void);

/**
 * @brief Create an element with a qualified XML name.
 * @param name Borrowed non-empty XML name; copied. Namespace declarations are supplied as attributes.
 * @return Owned element, or NULL for an invalid name or allocation failure.
 */
xml_t *xml_new_element(const string_t *name);

/**
 * @brief Create a text node.
 * @param text Borrowed XML 1.0 character data; copied, including whitespace.
 * @return Owned node, or NULL for invalid characters or allocation failure.
 */
xml_t *xml_new_text(const string_t *text);

/**
 * @brief Create a comment node.
 * @param text Borrowed comment content; copied. Must not contain -- or end with a hyphen.
 * @return Owned node, or NULL for invalid content or allocation failure.
 */
xml_t *xml_new_comment(const string_t *text);

/**
 * @brief Create a processing instruction.
 * @param name Borrowed XML target name; copied. The reserved target xml is rejected case-insensitively.
 * @param text Borrowed instruction content; copied. Must not contain ?>.
 * @return Owned node, or NULL for invalid content or allocation failure.
 */
xml_t *xml_new_processing_instruction(const string_t *name, const string_t *text);

/**
 * @brief Deep-copy an XML tree.
 * @param xml Borrowed source tree.
 * @return Independent owned tree, or NULL on invalid input or allocation failure.
 */
xml_t *xml_clone(const xml_t *xml);

/**
 * @brief Release a tree and all owned descendants.
 * @param xml Owned tree to destroy; NULL is harmless.
 */
void xml_free(xml_t *xml);

/**
 * @brief Read a node's type.
 * @param xml Borrowed node; may be NULL.
 * @return Node type, or XML_INVALID for NULL.
 */
xml_type_t xml_type(const xml_t *xml);

/**
 * @brief Borrow an element name or processing-instruction target.
 * @param xml Borrowed node.
 * @return Borrowed name valid until the node is freed, or NULL for other types.
 */
const string_t *xml_name(const xml_t *xml);

/**
 * @brief Borrow the content of a text, comment or processing-instruction node.
 * @param xml Borrowed node.
 * @return Borrowed content valid until mutation or destruction, or NULL for other types.
 */
const string_t *xml_text(const xml_t *xml);

/**
 * @brief Borrow an element's attribute dictionary.
 * @param xml Borrowed element.
 * @return Read-only dictionary storing string_t * keys and string_t * values, or NULL for other types. Do not mutate
 *   or destroy.
 */
const dictionary_t *xml_attributes(const xml_t *xml);

/**
 * @brief Copy an attribute into an element, replacing an existing value.
 * @param xml Element to modify; remains caller-owned.
 * @param name Borrowed qualified XML attribute name; copied, including xmlns declarations.
 * @param value Borrowed XML character data; copied.
 * @return True on success; false for invalid input or allocation failure.
 */
bool xml_set_attribute(xml_t *xml, const string_t *name, const string_t *value);

/**
 * @brief Look up an attribute by qualified name.
 * @param xml Borrowed element.
 * @param name Borrowed attribute name.
 * @return Borrowed value until replacement or destruction, or NULL if absent.
 */
const string_t *xml_get_attribute(const xml_t *xml, const string_t *name);

/**
 * @brief Read the number of ordered children.
 * @param xml Borrowed node.
 * @return Child count, or zero for NULL and leaf nodes.
 */
size_t xml_child_count(const xml_t *xml);

/**
 * @brief Borrow a child in document order.
 * @param xml Borrowed parent node.
 * @param index Zero-based child position.
 * @return Borrowed child until parent destruction, or NULL out of range.
 */
const xml_t *xml_child_at(const xml_t *xml, size_t index);

/**
 * @brief Append a deep copy of a child without consuming it.
 * @param xml Document or element to modify; document nodes accept one root element plus comments and instructions.
 * @param child Borrowed non-document child. Tree depth is limited to 128.
 * @return True on success; false for invalid structure, depth or allocation failure.
 */
bool xml_append_child(xml_t *xml, const xml_t *child);

/**
 * @brief Create an incremental UTF-8 XML reader.
 * @param limits Optional limits; NULL or zero fields select defaults. Maximum depth cannot exceed 128.
 * @param callback Optional event callback. NULL builds a tree; non-NULL selects event-only streaming without a
 *   retained tree.
 * @param user_data Borrowed context passed unchanged to callback.
 * @return Owned reader, or NULL for invalid limits or allocation failure.
 */
xml_reader_t *xml_reader_new(const xml_limits_t *limits, xml_event_fn callback, void *user_data);

/**
 * @brief Feed encoded bytes to the XML engine, allowing split UTF-8 sequences.
 * @param reader Reader to advance; cannot be reused after failure or finish.
 * @param data Borrowed UTF-8 bytes; may be NULL only for zero size.
 * @param size Byte count. Input must use XML 1.0 and UTF-8; DTDs and external entities are forbidden.
 * @return True if accepted so far; only xml_reader_finish confirms a complete document.
 */
bool xml_reader_feed(xml_reader_t *reader, const void *data, size_t size);

/**
 * @brief Validate end-of-input and close the document.
 * @param reader Reader to finish exactly once.
 * @return True for a complete well-formed document; false otherwise.
 */
bool xml_reader_finish(xml_reader_t *reader);

/**
 * @brief Transfer the completed tree from a tree-building reader.
 * @param reader Successfully finished reader created with a NULL callback.
 * @return Owned document exactly once, or NULL if unavailable.
 */
xml_t *xml_reader_take_document(xml_reader_t *reader);

/**
 * @brief Borrow the reader's error message.
 * @param reader Borrowed reader.
 * @return Borrowed diagnostic valid until reader destruction, or NULL if no diagnostic is available.
 */
const string_t *xml_reader_error(const xml_reader_t *reader);

/**
 * @brief Release a reader and any unclaimed tree.
 * @param reader Owned reader; NULL is harmless.
 */
void xml_reader_free(xml_reader_t *reader);

/**
 * @brief Parse a complete XML document using default limits.
 * @param text Borrowed XML source.
 * @return Owned document, or NULL on invalid XML, a limit breach or allocation failure.
 */
xml_t *xml_from_text(const string_t *text);

/**
 * @brief Parse a UTF-8 file incrementally through the file module.
 * @param path Borrowed file path; symlinks are rejected by the file module.
 * @return Owned document, or NULL on I/O or parsing failure.
 */
xml_t *xml_from_file(const string_t *path);

/**
 * @brief Validate and stream compact UTF-8 XML to a callback.
 * @param xml Borrowed document or standalone element. Must be namespace-well-formed.
 * @param callback Required sink receiving borrowed string_t fragments. False stops output.
 * @param user_data Borrowed sink context.
 * @return True on success. Sink failure may leave partial output; invalid trees are rejected before output.
 */
bool xml_write(const xml_t *xml, xml_write_fn callback, void *user_data);

/**
 * @brief Serialise XML into a UTF-8 string without introducing whitespace.
 * @param xml Borrowed document or standalone element.
 * @return Owned text released with string_free, or NULL on validation or allocation failure.
 */
string_t *xml_to_string(const xml_t *xml);

/**
 * @brief Stream XML to a file through the file module.
 * @param xml Borrowed document or standalone element.
 * @param path Borrowed destination path; existing content is truncated. Failure can leave a partial file.
 * @return Zero on success, or -1 on validation, allocation or I/O failure.
 */
int xml_to_file(const xml_t *xml, const string_t *path);

/**
 * @brief Produce a SQLite-ready XML payload.
 * @param xml Borrowed document or standalone element.
 * @param out_type Required output receiving an owned type label; release with string_free.
 * @param out_encoding Required output receiving an owned encoding label; release with string_free.
 * @param out_data Required output receiving owned bytes; release with free.
 * @param out_len Required output receiving the byte count.
 * @return True on success; outputs are cleared on failure when supplied.
 */
bool xml_serialize(const xml_t *xml, string_t **out_type, string_t **out_encoding, void **out_data, size_t *out_len);

/**
 * @brief Restore an XML document from a labelled payload.
 * @param data Borrowed encoded bytes of length len.
 * @param len Payload size in bytes.
 * @param type Borrowed type label; must equal xml.
 * @param encoding Borrowed encoding label; must equal xml-utf8.
 * @return Owned document, or NULL for invalid labels or payload.
 */
xml_t *xml_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding);

#endif /* MARS_XML_H */
