#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"
#include "xml.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static xml_t *parse(const char *source)
{
    string_t *text = string_new_with(source);
    xml_t *xml = xml_from_text(text);
    string_free(text);
    return xml;
}

static bool rejects(const char *source)
{
    xml_t *xml = parse(source);
    bool rejected = xml == NULL;
    xml_free(xml);
    return rejected;
}

static void test_xml_tree_and_dictionary(void)
{
    xml_t *doc = parse("<root enabled='yes'>hello<item id='1'/>there<item id='2'/></root>");
    const xml_t *root = xml_child_at(doc, 0);
    string_t *key = string_new_with("enabled");
    dictionary_entry_t *entry = NULL;
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_INT_EQ(xml_type(doc), XML_DOCUMENT);
    TEST_ASSERT_INT_EQ(xml_type(root), XML_ELEMENT);
    TEST_ASSERT_STR_EQ(string_c_str(xml_name(root)), "root");
    TEST_ASSERT_TRUE(dictionary_get_entry(xml_attributes(root), &key, &entry), "dictionary lookup");
    TEST_ASSERT_STR_EQ(string_c_str(*(string_t *const *)dictionary_entry_value(entry)), "yes");
    TEST_ASSERT_STR_EQ(string_c_str(xml_get_attribute(root, key)), "yes");
    TEST_ASSERT_INT_EQ(xml_child_count(root), 4);
    TEST_ASSERT_STR_EQ(string_c_str(xml_text(xml_child_at(root, 0))), "hello");
    TEST_ASSERT_STR_EQ(string_c_str(xml_name(xml_child_at(root, 1))), "item");
    TEST_ASSERT_STR_EQ(string_c_str(xml_text(xml_child_at(root, 2))), "there");
    TEST_ASSERT_TRUE(xml_child_at(root, 99) == NULL, "bounds");
    string_free(key);
    xml_free(doc);
}

static void test_xml_entities_and_content(void)
{
    xml_t *doc = parse("<?xml version='1.0' encoding='UTF-8'?><!--before--><r a='&quot;&#9;&#10;&#13;'>"
                       "&lt;&gt;&amp;&apos;&#65;&#x1F642;<![CDATA[<raw>&]]><?go now?><!--end--></r>");
    string_t *out = xml_to_string(doc);
    xml_t *round = out ? xml_from_text(out) : NULL;
    string_t *again = xml_to_string(round);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_NOT_NULL(round);
    TEST_ASSERT_TRUE(string_compare(out, again) == 0, "canonical round trip");
    TEST_ASSERT_TRUE(string_find(out, "&lt;raw&gt;&amp;") >= 0, "CDATA normalised");
    TEST_ASSERT_TRUE(string_find(out, "&#9;&#10;&#13;") >= 0, "attribute references retained");
    TEST_ASSERT_TRUE(string_find(out, "🙂") >= 0, "numeric Unicode reference");
    string_free(again);
    string_free(out);
    xml_free(round);
    xml_free(doc);
}

static void test_xml_rejects_invalid(void)
{
    static const char *const bad[] = {
        "", "text", "<a>", "<a/><b/>", "<a></b>", "<a x='1' x='2'/>", "<a x=1/>",
        "<a>&unknown;</a>", "<a>&#0;</a>", "<a>&#xD800;</a>", "<a>&#x110000;</a>",
        "<a>&#999999999999999999999999999;</a>", "<a>]]></a>", "<!--a--b--><r/>",
        "<a x='<b'/>", "<a / >", "<1a/>", "<a></a garbage>", "<a/><!--truncated",
        "<!DOCTYPE a><a/>", "<!DOCTYPE a SYSTEM 'file:///etc/passwd'><a/>",
        "<!DOCTYPE a [<!ENTITY x 'boom'>]><a>&x;</a>", "<?xml version='1.1'?><a/>",
        "<?xml version='1.0' encoding='ISO-8859-1'?><a/>", "<a><?xml version='1.0'?></a>",
        " <?xml version='1.0'?><a/>", "<a/>&#32;", "<a>\001</a>", "<?a:b?><r/>"
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
        TEST_ASSERT_TRUE(rejects(bad[i]), bad[i]);
}

static void test_xml_namespaces(void)
{
    const char *good = "<p:r xmlns:p='urn:one' xmlns='urn:default'><p:c xmlns:p='urn:two' p:a='v'/>"
                       "<c xmlns='' xml:lang='en'/></p:r>";
    xml_t *doc = parse(good);
    string_t *out = xml_to_string(doc);
    xml_t *round = out ? xml_from_text(out) : NULL;
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_NOT_NULL(round);
    TEST_ASSERT_TRUE(rejects("<p:r/>"), "unbound prefix");
    TEST_ASSERT_TRUE(rejects("<r p:a='x'/>"), "unbound attribute prefix");
    TEST_ASSERT_TRUE(rejects("<r xmlns:a='u' xmlns:b='u' a:x='1' b:x='2'/>"), "duplicate expanded name");
    TEST_ASSERT_TRUE(rejects("<r xmlns:xml='wrong'/>"), "reserved prefix");
    TEST_ASSERT_TRUE(rejects("<r xmlns:xmlns='u'/>"), "xmlns cannot be declared");
    TEST_ASSERT_TRUE(rejects("<r xmlns:p=''/>"), "prefix cannot be undeclared in namespace 1.0");
    TEST_ASSERT_TRUE(rejects("<:r/>"), "empty prefix");
    TEST_ASSERT_TRUE(rejects("<r:a:b xmlns:r='u'/>"), "multiple colons");
    xml_free(round);
    string_free(out);
    xml_free(doc);
}

static void test_xml_chunk_boundaries(void)
{
    const char *source = "<?xml version='1.0'?><r a='&#x41;'>é🙂&amp;<![CDATA[a<b]]><!--c--><?go x?></r>";
    size_t length = strlen(source);
    xml_t *baseline = parse(source);
    string_t *expected = xml_to_string(baseline);
    TEST_ASSERT_NOT_NULL(expected);
    for (size_t split = 0; split <= length; ++split) {
        xml_reader_t *reader = xml_reader_new(NULL, NULL, NULL);
        bool ok = reader && xml_reader_feed(reader, source, split) &&
                  xml_reader_feed(reader, source + split, length - split) && xml_reader_finish(reader);
        xml_t *doc = xml_reader_take_document(reader);
        string_t *out = xml_to_string(doc);
        bool equal = out && string_compare(out, expected) == 0;
        xml_free(doc);
        string_free(out);
        xml_reader_free(reader);
        TEST_ASSERT_TRUE(ok && equal, "every two-chunk split matches");
    }
    xml_reader_t *reader = xml_reader_new(NULL, NULL, NULL);
    bool ok = reader != NULL;
    for (size_t i = 0; ok && i < length; ++i)
        ok = xml_reader_feed(reader, source + i, 1);
    ok = ok && xml_reader_finish(reader);
    xml_t *doc = xml_reader_take_document(reader);
    string_t *out = xml_to_string(doc);
    TEST_ASSERT_TRUE(ok && out && string_compare(out, expected) == 0, "single-byte chunks");
    TEST_ASSERT_TRUE(xml_reader_take_document(reader) == NULL, "ownership transferred once");
    TEST_ASSERT_TRUE(!xml_reader_feed(reader, "", 0), "finished reader not reusable");
    string_free(out);
    xml_free(doc);
    xml_reader_free(reader);
    string_free(expected);
    xml_free(baseline);
}

static void test_xml_invalid_utf8_and_truncation(void)
{
    static const unsigned char invalid[][8] = {
        {0xc0, 0xaf}, {0xe0, 0x80, 0xaf}, {0xed, 0xa0, 0x80}, {0xf4, 0x90, 0x80, 0x80},
        {0x80}, {0xf5, 0x80, 0x80, 0x80}, {0xe2, 0x82}
    };
    static const size_t sizes[] = {2, 3, 3, 4, 1, 4, 2};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        xml_reader_t *reader = xml_reader_new(NULL, NULL, NULL);
        bool ok = xml_reader_feed(reader, "<r>", 3) &&
                  xml_reader_feed(reader, invalid[i], sizes[i]) && xml_reader_finish(reader);
        bool has_error = xml_reader_error(reader) != NULL;
        xml_reader_free(reader);
        TEST_ASSERT_TRUE(!ok && has_error, "reject malformed UTF-8");
    }
    xml_reader_t *reader = xml_reader_new(NULL, NULL, NULL);
    TEST_ASSERT_TRUE(xml_reader_feed(reader, "<r>", 3), "partial prefix accepted");
    TEST_ASSERT_TRUE(!xml_reader_finish(reader), "unclosed document rejected on finish");
    TEST_ASSERT_TRUE(xml_reader_take_document(reader) == NULL, "no incomplete tree");
    xml_reader_free(reader);
}

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

static void test_xml_stream_events_and_limits(void)
{
    event_counts_t counts = {0};
    xml_reader_t *reader = xml_reader_new(NULL, count_events, &counts);
    bool ok = xml_reader_feed(reader, "<r>", 3);
    for (size_t i = 0; ok && i < 10000; ++i)
        ok = xml_reader_feed(reader, "<i>value</i>", 12);
    ok = ok && xml_reader_feed(reader, "</r>", 4) && xml_reader_finish(reader);
    TEST_ASSERT_TRUE(ok, "many streamed elements");
    TEST_ASSERT_INT_EQ(counts.starts, 10001);
    TEST_ASSERT_INT_EQ(counts.ends, 10001);
    TEST_ASSERT_TRUE(xml_reader_take_document(reader) == NULL, "event mode retains no tree");
    xml_reader_free(reader);
    counts.stop = true;
    reader = xml_reader_new(NULL, count_events, &counts);
    TEST_ASSERT_TRUE(!xml_reader_feed(reader, "<r/>", 4), "callback abort");
    TEST_ASSERT_NOT_NULL(xml_reader_error(reader));
    xml_reader_free(reader);
    xml_limits_t limits = {.max_bytes = 3};
    reader = xml_reader_new(&limits, NULL, NULL);
    TEST_ASSERT_TRUE(!xml_reader_feed(reader, "<r/>", 4), "input byte limit");
    xml_reader_free(reader);
    limits = (xml_limits_t){.max_depth = 1};
    reader = xml_reader_new(&limits, NULL, NULL);
    TEST_ASSERT_TRUE(!xml_reader_feed(reader, "<r><i/></r>", 11), "depth limit");
    xml_reader_free(reader);
    limits = (xml_limits_t){.max_nodes = 1};
    reader = xml_reader_new(&limits, NULL, NULL);
    TEST_ASSERT_TRUE(!xml_reader_feed(reader, "<r><i/></r>", 11), "node limit");
    xml_reader_free(reader);
    limits.max_depth = 129;
    TEST_ASSERT_TRUE(xml_reader_new(&limits, NULL, NULL) == NULL, "hard nesting bound");
}

static void test_xml_build_clone_and_validation(void)
{
    string_t *name = string_new_with("r"), *key = string_new_with("a");
    string_t *value = string_new_with("old"), *replacement = string_new_with("new");
    string_t *text = string_new_with("<&>\r"), *comment = string_new_with("ok");
    xml_t *root = xml_new_element(name), *leaf = xml_new_text(text), *doc = xml_new_document();
    xml_t *note = xml_new_comment(comment), *pi = xml_new_processing_instruction(key, comment);
    TEST_ASSERT_TRUE(xml_set_attribute(root, key, value), "attribute insertion");
    TEST_ASSERT_TRUE(xml_set_attribute(root, key, replacement), "attribute replacement");
    TEST_ASSERT_TRUE(xml_append_child(root, leaf), "text copy");
    TEST_ASSERT_TRUE(xml_append_child(doc, note) && xml_append_child(doc, pi) && xml_append_child(doc, root),
                     "document content");
    TEST_ASSERT_TRUE(!xml_append_child(doc, root) && !xml_append_child(doc, leaf), "document shape enforced");
    xml_t *copy = xml_clone(doc);
    xml_free(doc);
    string_t *out = xml_to_string(copy);
    TEST_ASSERT_STR_EQ(string_c_str(out), "<!--ok--><?a ok?><r a=\"new\">&lt;&amp;&gt;&#13;</r>");
    TEST_ASSERT_INT_EQ(xml_type(NULL), XML_INVALID);
    TEST_ASSERT_TRUE(xml_name(leaf) == NULL && xml_attributes(leaf) == NULL, "leaf accessors");
    TEST_ASSERT_TRUE(!xml_set_attribute(leaf, key, value) && !xml_append_child(leaf, root), "leaf immutable shape");
    string_t *bad = string_new_with("bad name"), *bad_comment = string_new_with("bad--comment");
    TEST_ASSERT_TRUE(xml_new_element(bad) == NULL && xml_new_comment(bad_comment) == NULL, "invalid constructors");
    string_t *reserved = string_new_with("XmL"), *bad_pi = string_new_with("?>");
    TEST_ASSERT_TRUE(xml_new_processing_instruction(reserved, text) == NULL &&
                     xml_new_processing_instruction(key, bad_pi) == NULL, "invalid PI");
    string_free(reserved); string_free(bad_pi); string_free(bad); string_free(bad_comment);
    string_free(out); xml_free(copy); xml_free(root); xml_free(leaf); xml_free(note); xml_free(pi);
    string_free(name); string_free(key); string_free(value); string_free(replacement);
    string_free(text); string_free(comment);
}

typedef struct { string_t *text; size_t calls, largest; bool stop; } sink_t;
static bool collect(const string_t *text, void *context)
{
    sink_t *sink = context;
    size_t size = string_byte_length(text);
    ++sink->calls;
    if (size > sink->largest)
        sink->largest = size;
    return !sink->stop && string_append_utf8_exact(sink->text, string_c_str(text), string_byte_length(text)) == 0;
}

static void test_xml_writer_limits_and_failure(void)
{
    string_t *name = string_new_with("r"), *body = string_new();
    for (size_t i = 0; i < 12000; ++i)
        string_append_utf8_exact(body, "&é", 3);
    xml_t *root = xml_new_element(name), *text = xml_new_text(body);
    TEST_ASSERT_TRUE(xml_append_child(root, text), "large text");
    sink_t sink = {.text = string_new()};
    TEST_ASSERT_TRUE(xml_write(root, collect, &sink), "stream write");
    TEST_ASSERT_TRUE(sink.calls > 1 && sink.largest <= 4096, "bounded fragments");
    xml_t *round = xml_from_text(sink.text);
    TEST_ASSERT_NOT_NULL(round);
    sink.stop = true;
    TEST_ASSERT_TRUE(!xml_write(root, collect, &sink), "sink failure propagated");
    string_t *unbound = string_new_with("p:r");
    xml_t *bad = xml_new_element(unbound);
    sink.calls = 0;
    TEST_ASSERT_TRUE(!xml_write(bad, collect, &sink) && sink.calls == 0, "preflight before sink");
    xml_free(bad); string_free(unbound); xml_free(round); string_free(sink.text);
    xml_free(text); xml_free(root); string_free(body); string_free(name);
}

static void test_xml_files_and_storage(void)
{
    string_t *path = string_new_with(test_case_temp_path("xml-roundtrip.xml"));
    xml_t *doc = parse("<r a='é'>🙂</r>");
    TEST_ASSERT_INT_EQ(xml_to_file(doc, path), 0);
    xml_t *loaded = xml_from_file(path);
    TEST_ASSERT_NOT_NULL(loaded);
    string_t *type = NULL, *encoding = NULL;
    void *data = NULL;
    size_t length = 0;
    TEST_ASSERT_TRUE(xml_serialize(loaded, &type, &encoding, &data, &length), "serialise");
    TEST_ASSERT_STR_EQ(string_c_str(type), "xml");
    TEST_ASSERT_STR_EQ(string_c_str(encoding), "xml-utf8");
    xml_t *restored = xml_deserialise(data, length, type, encoding);
    TEST_ASSERT_NOT_NULL(restored);
    string_t *wrong = string_new_with("json");
    TEST_ASSERT_TRUE(xml_deserialise(data, length, wrong, encoding) == NULL, "label validation");
    free(data); string_free(type); string_free(encoding); string_free(wrong);
    xml_free(restored); xml_free(loaded); xml_free(doc);
    file_t *file = file_new(path);
    TEST_ASSERT_TRUE(file_delete(file), "remove test file");
    file_free(file);
    string_free(path);
}

/* README example: inspect a dictionary-backed attribute. */
static void example_xml_dictionary(void)
{
    string_t *source = string_new_with("<book language='cy'>MARS</book>");
    string_t *key = string_new_with("language");
    xml_t *document = xml_from_text(source);
    const xml_t *book = xml_child_at(document, 0);
    dictionary_entry_t *entry = NULL;
    bool found = dictionary_get_entry(xml_attributes(book), &key, &entry);
    const string_t *language = found ? *(string_t *const *)dictionary_entry_value(entry) : NULL;
    string_printf("%S: %S\n", xml_name(book), language);
    TEST_ASSERT_TRUE(found, "README dictionary lookup");
    TEST_ASSERT_STR_EQ(string_c_str(language), "cy");
    xml_free(document);
    string_free(key);
    string_free(source);
}

/* README example: build and escape XML. */
static void example_xml_build(void)
{
    string_t *name = string_new_with("message");
    string_t *value = string_new_with("Hello & goodbye");
    xml_t *message = xml_new_element(name);
    xml_t *text = xml_new_text(value);
    bool appended = xml_append_child(message, text);
    string_t *output = xml_to_string(message);
    string_printf("%S\n", output);
    TEST_ASSERT_TRUE(appended, "README append");
    TEST_ASSERT_STR_EQ(string_c_str(output), "<message>Hello &amp; goodbye</message>");
    string_free(output);
    xml_free(text);
    xml_free(message);
    string_free(value);
    string_free(name);
}

/* README example: consume incremental events without building a tree. */
static void example_xml_stream(void)
{
    event_counts_t counts = {0};
    xml_reader_t *reader = xml_reader_new(NULL, count_events, &counts);
    bool ok = xml_reader_feed(reader, "<items><item", 12) &&
              xml_reader_feed(reader, "/><item/></items>", 17) && xml_reader_finish(reader);
    string_printf("elements=%zu complete=%d\n", counts.starts, (int)ok);
    TEST_ASSERT_TRUE(ok && counts.starts == 3, "README streaming");
    xml_reader_free(reader);
}


static void test_xml_unicode_spelling_and_normalisation(void)
{
    const char source[] = "<r e\xcc\x81='one' \xc3\xa9='two'>e\xcc\x81</r>";
    xml_reader_t *reader = xml_reader_new(NULL, NULL, NULL);
    bool ok = xml_reader_feed(reader, source, sizeof(source) - 1) && xml_reader_finish(reader);
    xml_t *doc = xml_reader_take_document(reader);
    string_t *output = xml_to_string(doc);
    /* Attribute quotes are canonicalised, so compare an explicitly double-quoted spelling. */
    const char expected[] = "<r e\xcc\x81=\"one\" \xc3\xa9=\"two\">e\xcc\x81</r>";
    bool same = output && string_byte_length(output) == sizeof(expected) - 1 &&
           memcmp(string_c_str(output), expected, sizeof(expected) - 1) == 0;
    TEST_ASSERT_TRUE(ok && same, "canonically equivalent XML names remain distinct");
    string_free(output);
    xml_free(doc);
    xml_reader_free(reader);
    const char mismatched[] = "<e\xcc\x81></\xc3\xa9>";
    reader = xml_reader_new(NULL, NULL, NULL);
    ok = xml_reader_feed(reader, mismatched, sizeof(mismatched) - 1) && xml_reader_finish(reader);
    TEST_ASSERT_TRUE(!ok, "different Unicode spellings cannot close each other's tags");
    xml_reader_free(reader);
    doc = parse("<r a='a\r\nb\tc\nd'>one\r\ntwo\rthree</r>");
    string_t *key = string_new_with("a");
    const xml_t *root = xml_child_at(doc, 0);
    TEST_ASSERT_STR_EQ(string_c_str(xml_get_attribute(root, key)), "a b c d");
    TEST_ASSERT_STR_EQ(string_c_str(xml_text(xml_child_at(root, 0))), "one\ntwo\nthree");
    string_free(key);
    xml_free(doc);
}


static void test_xml_file_failures_and_empty_results(void)
{
    string_t *path = string_new_with(test_case_temp_path("xml-preserve.xml"));
    string_t *original = string_new_with("preserve me");
    file_t *file = file_new(path);
    TEST_ASSERT_TRUE(file_write_all_text(file, original), "prepare existing file");
    xml_t *empty = xml_new_document();
    TEST_ASSERT_INT_EQ(xml_to_file(empty, path), -1);
    string_t *after = file_read_all_text(file);
    TEST_ASSERT_TRUE(after && string_compare(original, after) == 0, "validation precedes truncation");
    TEST_ASSERT_TRUE(xml_from_file(path) == NULL, "malformed file rejected");
    string_t *type = NULL, *encoding = NULL;
    void *data = NULL;
    size_t length = 123;
    TEST_ASSERT_TRUE(!xml_serialize(empty, &type, &encoding, &data, &length), "empty document cannot serialise");
    TEST_ASSERT_TRUE(!type && !encoding && !data && length == 0, "failure outputs cleared");
    TEST_ASSERT_TRUE(!xml_serialize(empty, NULL, &encoding, &data, &length), "required output");
    TEST_ASSERT_TRUE(xml_from_text(NULL) == NULL && xml_from_file(NULL) == NULL, "NULL inputs");
    TEST_ASSERT_TRUE(xml_to_string(NULL) == NULL && !xml_write(empty, NULL, NULL), "NULL outputs");
    TEST_ASSERT_TRUE(!xml_reader_feed(NULL, NULL, 0) && !xml_reader_finish(NULL), "NULL reader");
    TEST_ASSERT_TRUE(xml_reader_error(NULL) == NULL && xml_reader_take_document(NULL) == NULL, "NULL reader access");
    xml_reader_free(NULL);
    xml_free(NULL);
    TEST_ASSERT_TRUE(file_delete(file), "delete test file");
    TEST_ASSERT_TRUE(xml_from_file(path) == NULL, "missing file rejected");
    file_free(file); xml_free(empty);
    string_free(after); string_free(original); string_free(path);
}

int tests_main(void)
{
    TEST_SECTION("XML");
    TEST_RUN_IN_GROUP(test_xml_file_failures_and_empty_results, tests, NULL);
    TEST_RUN_IN_GROUP(test_xml_unicode_spelling_and_normalisation, tests, NULL);
    TEST_RUN_IN_GROUP(test_xml_tree_and_dictionary, tests, NULL);
    TEST_RUN_IN_GROUP(test_xml_entities_and_content, tests, NULL);
    TEST_RUN_IN_GROUP(test_xml_rejects_invalid, tests, NULL);
    TEST_RUN_IN_GROUP(test_xml_namespaces, tests, NULL);
    TEST_RUN_IN_GROUP(test_xml_chunk_boundaries, tests, NULL);
    TEST_RUN_IN_GROUP(test_xml_invalid_utf8_and_truncation, tests, NULL);
    TEST_RUN_IN_GROUP(test_xml_stream_events_and_limits, tests, NULL);
    TEST_RUN_IN_GROUP(test_xml_build_clone_and_validation, tests, NULL);
    TEST_RUN_IN_GROUP(test_xml_writer_limits_and_failure, tests, NULL);
    TEST_RUN_IN_GROUP(test_xml_files_and_storage, tests, NULL);
    TEST_SECTION("README Output Examples");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_xml_dictionary, readme_examples, "xml,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_xml_build, readme_examples, "xml,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_xml_stream, readme_examples, "xml,readme,output");
    return TEST_EXIT_CODE();
}
