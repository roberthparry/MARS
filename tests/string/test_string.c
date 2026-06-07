#include "ustring.h"
#include "test_string.h"
#include <stdio.h>
#include <stdarg.h>

TEST_SUITE_CONFIG(TEST_CONFIG_NONE);

/* ------------------------------------------------------------------------- */
/* Tests                                                                     */
/* ------------------------------------------------------------------------- */

static void test_split_basic(void)
{
    string_t *s = string_new_with("alpha, beta , gamma ,delta");

    size_t n = 0;
    string_t **parts = string_split(s, ",", &n);

    ASSERT_EQ(n, 4);

    // Trim each part
    for (size_t i = 0; i < n; i++)
        string_trim(parts[i]);

    ASSERT_STREQ(string_c_str(parts[0]), "alpha");
    ASSERT_STREQ(string_c_str(parts[1]), "beta");
    ASSERT_STREQ(string_c_str(parts[2]), "gamma");
    ASSERT_STREQ(string_c_str(parts[3]), "delta");

    string_split_free(parts, n);
    string_free(s);
}

static void test_join_basic(void)
{
    string_t *s = string_new_with("alpha, beta , gamma ,delta");

    size_t n = 0;
    string_t **parts = string_split(s, ",", &n);

    for (size_t i = 0; i < n; i++)
        string_trim(parts[i]);

    string_t *joined = string_join(parts, n, " | ");

    ASSERT_STREQ(string_c_str(joined), "alpha | beta | gamma | delta");

    string_free(joined);
    string_split_free(parts, n);
    string_free(s);
}

static void test_split_edge_cases(void)
{
    // Leading, trailing, repeated delimiters
    string_t *s = string_new_with(",a,,b,");

    size_t n = 0;
    string_t **parts = string_split(s, ",", &n);

    ASSERT_EQ(n, 2);

    ASSERT_STREQ(string_c_str(parts[0]), "a");
    ASSERT_STREQ(string_c_str(parts[1]), "b");

    string_split_free(parts, n);
    string_free(s);
}

static void test_join_empty_fields(void)
{
    // Join empty strings
    string_t *a = string_new_with("");
    string_t *b = string_new_with("");
    string_t *c = string_new_with("");

    string_t *arr[3] = { a, b, c };

    string_t *joined = string_join(arr, 3, ",");

    ASSERT_STREQ(string_c_str(joined), ",,");

    string_free(joined);
    string_free(a);
    string_free(b);
    string_free(c);
}

static void test_string_replace(void)
{
    string_t *s = string_new_with("the cat sat on the mat");
    string_replace(s, "at", "oodle");

    ASSERT_STREQ(string_c_str(s), "the coodle soodle on the moodle");

    string_free(s);
}

static void test_string_new_wide(void)
{
    string_t *s = string_new_wide(L"hello \u03C0 \U0001F642");
    wchar_t invalid_wide[] = { (wchar_t)0xD800u, L'\0' };
    string_t *replacement = string_new_wide(invalid_wide);

    ASSERT_NOT_NULL(s);
    ASSERT_STREQ(string_c_str(s), "hello π 🙂");
    ASSERT_EQ(string_length(s), 9);

    ASSERT_NOT_NULL(replacement);
    ASSERT_STREQ(string_c_str(replacement), "�");
    ASSERT_EQ(string_length(replacement), 1);
    ASSERT_NULL(string_new_wide(NULL));

    string_free(replacement);
    string_free(s);
}

static string_t *test_string_vsprintf_helper(const char *fmt, ...)
{
    string_t *out;
    va_list ap;

    va_start(ap, fmt);
    out = string_vsprintf(fmt, ap);
    va_end(ap);
    return out;
}

static void test_append_format(void)
{
    string_t *s = string_new_with("Hello");
    string_t *name = string_new_with("MARS");
    string_t *greek = string_new_with("αβ");
    string_t *created;
    string_view_t name_view = string_view(name, 1u, 2u);

    string_append_char(s, ' ');
    string_append_cstr(s, "world");
    string_append_format(s, " %d + %d = %d", 2, 3, 5);

    ASSERT_STREQ(string_c_str(s), "Hello world 2 + 3 = 5");

    string_replace(s, "world", "universe");
    ASSERT_STREQ(string_c_str(s), "Hello universe 2 + 3 = 5");

    string_clear(s);
    string_append_format(s,
                         "%S:%W:%R:%04d:%s",
                         name,
                         name_view,
                         string_at(greek, 0u),
                         7,
                         "ready");
    ASSERT_STREQ(string_c_str(s), "MARS:AR:α:0007:ready");

    created = string_sprintf("%S/%W/%R", name, name_view, string_at(greek, 1u));
    ASSERT_NOT_NULL(created);
    ASSERT_STREQ(string_c_str(created), "MARS/AR/β");
    string_free(created);

    created = test_string_vsprintf_helper("%S/%d", name, 42);
    ASSERT_NOT_NULL(created);
    ASSERT_STREQ(string_c_str(created), "MARS/42");
    string_free(created);

    string_free(greek);
    string_free(name);
    string_free(s);
}

static void test_starts_with_ends_with(void)
{
    string_t *s = string_new_with("Hello universe!");

    ASSERT_TRUE(string_starts_with(s, "Hello"));
    ASSERT_TRUE(string_ends_with(s, "verse!"));

    ASSERT_EQ(string_find(s, "uni"), 6);

    string_t *sub = string_substr(s, 6, 8);
    ASSERT_STREQ(string_c_str(sub), "universe");

    string_free(sub);
    string_free(s);
}

static void test_to_upper_and_to_lower(void)
{
    string_t *s = string_new_with("Hello World");

    string_to_lower(s);
    ASSERT_STREQ(string_c_str(s), "hello world");

    string_to_upper(s);
    ASSERT_STREQ(string_c_str(s), "HELLO WORLD");

    string_t *a = string_new_with("apple");
    string_t *b = string_new_with("banana");

    ASSERT_TRUE(string_compare(a, b) < 0);

    string_free(a);
    string_free(b);
    string_free(s);
}

static void test_embedded_nul_is_real_string_content(void)
{
    string_t *s = string_new_with("ab");
    string_t *prefix = string_new_with("ab");
    string_t *clone;
    string_t *formatted;
    rune_t nul;

    ASSERT_EQ(string_append_char(s, '\0'), 0);
    ASSERT_EQ(string_append_cstr(s, "cd"), 0);

    nul = string_at(s, 2u);
    ASSERT_FALSE(rune_is_none(nul));
    ASSERT_EQ(rune_value(nul), 0u);
    ASSERT_EQ(rune_value(string_at(s, 3u)), 'c');
    ASSERT_EQ((long)string_length(s), 5L);

    clone = string_clone(s);
    ASSERT_NOT_NULL(clone);
    ASSERT_EQ(string_compare(s, clone), 0);
    ASSERT_TRUE(string_compare(s, prefix) > 0);
    ASSERT_EQ(rune_value(string_at(clone, 3u)), 'c');

    formatted = string_sprintf("%S", s);
    ASSERT_NOT_NULL(formatted);
    ASSERT_EQ(string_compare(s, formatted), 0);

    string_free(formatted);
    string_free(clone);
    string_free(prefix);
    string_free(s);
}

typedef struct {
    string_builder_t *out;
    size_t count;
} each_char_state_t;

static int collect_character(rune_t rune, size_t index, void *user)
{
    each_char_state_t *state = user;

    (void)index;
    state->count++;
    return string_append_rune(state->out, rune);
}

static void test_text_character_api(void)
{
    string_t *s = string_new_with("👨‍👩‍👧‍👦 café 🇬🇧");
    rune_t family = string_at(s, 0);
    rune_t missing = string_at(s, 99);
    string_t *word = string_substring(s, 2, 4);
    string_t *family_copy = rune_to_string(family);
    string_builder_t *collected = string_builder_new();
    each_char_state_t state = { collected, 0u };

    ASSERT_EQ((long)string_length(s), 8L);
    ASSERT_FALSE(rune_is_none(family));
    ASSERT_TRUE(rune_is_none(missing));
    ASSERT_STREQ(string_c_str(family_copy), "👨‍👩‍👧‍👦");
    ASSERT_STREQ(string_c_str(word), "café");

    string_reverse(s);
    string_reverse(s);
    ASSERT_STREQ(string_c_str(s), "👨‍👩‍👧‍👦 café 🇬🇧");

    ASSERT_EQ(string_each(s, collect_character, &state), 0);
    ASSERT_EQ((long)state.count, 8L);
    ASSERT_STREQ(string_c_str(collected), "👨‍👩‍👧‍👦 café 🇬🇧");

    string_builder_free(collected);
    string_free(family_copy);
    string_free(word);
    string_free(s);
}

static void test_string_view(void)
{
    string_t *s = string_new_with("Hello Universe");
    string_t *expected = string_new_with("Universe");
    string_t *lower_prefix = string_new_with("uni");
    string_t *exact_prefix = string_new_with("Uni");

    string_reverse(s);
    string_reverse(s); /* restore */

    string_view_t v = string_view(s, 6, 8);
    string_view_t expected_view = string_view_all(expected);
    string_view_t lower_prefix_view = string_view_all(lower_prefix);
    string_t *sub = string_from_view(&v);

    ASSERT_STREQ(string_c_str(sub), "Universe");

    ASSERT_TRUE(string_view_equals_view(v, expected_view));
    ASSERT_TRUE(string_view_starts_with(v, exact_prefix, false));
    ASSERT_FALSE(string_view_starts_with(v, lower_prefix, false));
    ASSERT_TRUE(string_view_starts_with(v, lower_prefix, true));
    ASSERT_TRUE(string_view_starts_with_view(v, lower_prefix_view, true));

    string_free(exact_prefix);
    string_free(lower_prefix);
    string_free(expected);
    string_free(s);
    string_free(sub);

}

static void test_string_cursor_view_rune_values(void)
{
    string_t *s = string_new_with("Aπ🙂");
    string_view_t view = string_view_all(s);
    string_cursor_t *cursor = string_cursor_new_view(view);
    unsigned char ascii;
    uint32_t value;
    string_pos_t start;
    string_pos_t end;
    string_pos_t next;
    rune_t rune;

    ASSERT_TRUE(cursor != NULL);
    ASSERT_TRUE(string_view_peek_ascii(view, 0u, &ascii));
    ASSERT_EQ(ascii, 'A');
    ASSERT_FALSE(string_view_peek_ascii(view, 1u, &ascii));
    ASSERT_TRUE(string_view_peek_rune_value(view, 1u, &value, &next));
    ASSERT_EQ(value, 0x03C0u);
    ASSERT_EQ(next, 3u);

    start = string_cursor_position(cursor);
    rune = string_cursor_peek(cursor);
    ASSERT_EQ(rune_value(rune), 'A');
    ASSERT_EQ(string_cursor_next(cursor), 0);
    end = string_cursor_position(cursor);
    ASSERT_EQ(end - start, 1);

    start = string_cursor_position(cursor);
    rune = string_cursor_peek(cursor);
    ASSERT_EQ(rune_value(rune), 0x03C0u);
    ASSERT_EQ(string_cursor_next(cursor), 0);
    end = string_cursor_position(cursor);
    ASSERT_EQ(end - start, 2);

    rune = string_cursor_peek(cursor);
    ASSERT_EQ(rune_value(rune), 0x1F642u);
    ASSERT_EQ(string_cursor_next(cursor), 0);
    ASSERT_TRUE(string_cursor_done(cursor));

    string_cursor_free(cursor);
    string_free(s);
}

static void test_string_cursor_parsing_api(void)
{
    string_t *s = string_new_with("sqrt(α)+1");
    string_cursor_t *cursor = string_cursor_new(s);
    string_pos_t start;
    string_t *name;
    string_t *rune_text;
    rune_t rune;

    ASSERT_NOT_NULL(cursor);
    ASSERT_TRUE(string_cursor_match(cursor, "sqrt"));
    ASSERT_TRUE(string_cursor_consume(cursor, "sqrt"));
    ASSERT_TRUE(string_cursor_consume(cursor, "("));

    start = string_cursor_position(cursor);
    rune = string_cursor_peek(cursor);
    ASSERT_FALSE(rune_is_none(rune));
    rune_text = rune_to_string(rune);
    ASSERT_NOT_NULL(rune_text);
    ASSERT_STREQ(string_c_str(rune_text), "α");
    ASSERT_EQ(string_cursor_next(cursor), 0);

    name = string_cursor_extract(start, cursor);
    ASSERT_NOT_NULL(name);
    ASSERT_STREQ(string_c_str(name), "α");

    ASSERT_TRUE(string_cursor_consume(cursor, ")"));
    ASSERT_TRUE(string_cursor_consume(cursor, "+"));
    ASSERT_TRUE(string_cursor_consume(cursor, "1"));
    ASSERT_TRUE(string_cursor_done(cursor));

    string_free(rune_text);
    string_free(name);
    string_cursor_free(cursor);
    string_free(s);
}

static bool test_parser_identifier_rune(rune_t rune)
{
    return rune_is_alpha_numeric(rune) || rune_is_equal(rune, '_');
}

typedef enum {
    TEST_TOKEN_ID,
    TEST_TOKEN_NUMBER,
    TEST_TOKEN_OPERATOR
} test_parser_token_kind_t;

static void test_parser_append_token(string_t *out,
                                     test_parser_token_kind_t kind,
                                     const string_t *text)
{
    if (string_length(out) > 0u)
        string_append_cstr(out, " ");

    switch (kind) {
        case TEST_TOKEN_ID:
            string_append_cstr(out, "id[");
            break;
        case TEST_TOKEN_NUMBER:
            string_append_cstr(out, "number[");
            break;
        case TEST_TOKEN_OPERATOR:
            string_append_cstr(out, "op[");
            break;
    }

    string_append_format(out, "%S]", text);
}

static bool test_parser_read_identifier(string_cursor_t *cursor, string_t *out)
{
    string_pos_t start = string_cursor_position(cursor);
    rune_t rune = string_cursor_peek(cursor);
    string_t *token;

    if (!test_parser_identifier_rune(rune) || rune_is_digit(rune))
        return false;

    do {
        string_cursor_next(cursor);
        rune = string_cursor_peek(cursor);
    } while (!rune_is_none(rune) && test_parser_identifier_rune(rune));

    token = string_cursor_extract(start, cursor);
    if (!token)
        return false;
    test_parser_append_token(out, TEST_TOKEN_ID, token);
    string_free(token);
    return true;
}

static bool test_parser_read_number(string_cursor_t *cursor, string_t *out)
{
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
    if (!token)
        return false;
    test_parser_append_token(out, TEST_TOKEN_NUMBER, token);
    string_free(token);
    return true;
}

static void test_parser_read_operator(string_cursor_t *cursor, string_t *out)
{
    string_t *token = rune_to_string(string_cursor_peek(cursor));

    ASSERT_NOT_NULL(token);
    test_parser_append_token(out, TEST_TOKEN_OPERATOR, token);
    string_free(token);
    string_cursor_next(cursor);
}

static string_t *test_parser_tokenise(const string_t *source)
{
    string_cursor_t *cursor = string_cursor_new(source);
    string_t *out = string_new();

    if (!cursor || !out) {
        string_cursor_free(cursor);
        string_free(out);
        return NULL;
    }

    while (!string_cursor_done(cursor)) {
        string_cursor_skip_spaces(cursor);
        if (string_cursor_done(cursor))
            break;
        if (test_parser_read_identifier(cursor, out))
            continue;
        if (test_parser_read_number(cursor, out))
            continue;
        test_parser_read_operator(cursor, out);
    }

    string_cursor_free(cursor);
    return out;
}

static void test_string_cursor_parser_example(void)
{
    string_t *source = string_new_with("cdf(α_1) + 355/113");
    string_t *tokens = test_parser_tokenise(source);

    ASSERT_NOT_NULL(tokens);
    ASSERT_STREQ(string_c_str(tokens),
                 "id[cdf] op[(] id[α_1] op[)] op[+] number[355/113]");

    string_free(tokens);
    string_free(source);
}

static void test_string_object_first_api(void)
{
    string_t *s = string_new_with("alpha");
    string_t *suffix = string_new_with("gamma");
    string_t *middle = string_new_with(",beta,");
    string_t *needle = string_new_with("beta");
    string_t *replacement = string_new_with("BETA");
    string_t *comma = string_new_with(",");
    string_t *prefix = string_new_with("alpha");
    size_t n = 0u;

    ASSERT_EQ(string_append_string(s, suffix), 0);
    ASSERT_STREQ(string_c_str(s), "alphagamma");

    ASSERT_EQ(string_insert_string(s, 5u, middle), 0);
    ASSERT_STREQ(string_c_str(s), "alpha,beta,gamma");

    ASSERT_TRUE(string_starts_with_string(s, prefix));
    ASSERT_TRUE(string_ends_with_string(s, suffix));
    ASSERT_EQ(string_find_string(s, needle), 6);

    ASSERT_EQ(string_replace_string(s, needle, replacement), 0);
    ASSERT_STREQ(string_c_str(s), "alpha,BETA,gamma");

    string_t **parts = string_split_string(s, comma, &n);
    ASSERT_EQ(n, 3);
    ASSERT_STREQ(string_c_str(parts[0]), "alpha");
    ASSERT_STREQ(string_c_str(parts[1]), "BETA");
    ASSERT_STREQ(string_c_str(parts[2]), "gamma");

    string_t *joined = string_join_string(parts, n, comma);
    ASSERT_STREQ(string_c_str(joined), "alpha,BETA,gamma");

    string_builder_t *builder = string_builder_new();
    ASSERT_EQ(string_builder_append_string(builder, joined), 0);
    ASSERT_STREQ(string_c_str(builder), "alpha,BETA,gamma");

    char buf[64];
    string_buffer_t sb;
    string_buffer_init(&sb, buf, sizeof(buf));
    ASSERT_EQ(string_buffer_append_string(&sb, joined), 0);
    ASSERT_STREQ(string_buffer_c_str(&sb), "alpha,BETA,gamma");

    string_builder_free(builder);
    string_free(joined);
    string_split_free(parts, n);
    string_free(comma);
    string_free(prefix);
    string_free(replacement);
    string_free(needle);
    string_free(middle);
    string_free(suffix);
    string_free(s);
}

static void test_string_builder(void)
{
    string_t *s = string_new_with("alpha,beta,gamma,delta");

    size_t n;
    string_t **parts = string_split(s, ",", &n);

    ASSERT_EQ(n, 4);

    string_split_free(parts, n);

    char buf[64];
    string_buffer_t sb;
    string_buffer_init(&sb, buf, sizeof(buf));
    string_buffer_append(&sb, "Hello");
    string_buffer_append_char(&sb, '!');

    ASSERT_STREQ(string_buffer_c_str(&sb), "Hello!");
    ASSERT_EQ((long)string_buffer_length(&sb), 6L);

    string_builder_t *b = string_builder_new();
    string_t *label = string_new_with("Builder");

    string_builder_format(b, "Pi approx = %.3f", 3.14159);

    ASSERT_STREQ(string_c_str(b), "Pi approx = 3.142");

    string_clear(b);
    string_builder_format(b, "%S says %s", label, "hello");
    ASSERT_STREQ(string_c_str(b), "Builder says hello");

    string_free(label);
    string_builder_free(b);
    string_free(s);
}

static void test_utf8_stuff(void)
{
    string_t *s = string_new_with("Héllo 🌍");

    ASSERT_EQ((long)string_length(s), 7L);

    string_reverse(s);
    string_to_upper(s);

    string_free(s);
}

static void test_character_count_and_reverse(void)
{
    string_t *s = string_new_with("👩‍👩‍👧‍👦 café 🇬🇧");

    ASSERT_TRUE(string_length(s) > 0);

    string_reverse(s);

    string_free(s);
}

static void test_character_reverse_and_substring(void)
{
    string_t *s = string_new_with("👩‍👩‍👧‍👦 café 🇬🇧");

    size_t g = string_length(s);
    ASSERT_TRUE(g > 0);

    string_reverse(s);

    string_t *sub = string_substring(s, 1, 3);
    ASSERT_TRUE(sub != NULL);

    string_free(sub);
    string_free(s);
}

static void test_text_ascii_stays_stable(void)
{
    string_t *s = string_new_with("Hello World");

    ASSERT_STREQ(string_c_str(s), "Hello World");

    string_free(s);
}

static void test_text_construction_canonicalises_combining_marks(void)
{
    string_t *s1 = string_new_with("é");
    string_t *s2 = string_new_with("é");

#ifdef HAVE_UNISTRING
    ASSERT_STREQ(string_c_str(s1), "é");
    ASSERT_STREQ(string_c_str(s2), "é");
#else
    ASSERT_STREQ(string_c_str(s1), "é");
    ASSERT_STREQ(string_c_str(s2), "é");
#endif

    string_free(s1);
    string_free(s2);
}

static void test_text_construction_canonicalises_hangul(void)
{
    string_t *s1 = string_new_with("가");
    string_t *s2 = string_new_with("가");

#ifdef HAVE_UNISTRING
    ASSERT_STREQ(string_c_str(s1), "가");
    ASSERT_STREQ(string_c_str(s2), "가");
#else
    ASSERT_STREQ(string_c_str(s1), "가");
    ASSERT_STREQ(string_c_str(s2), "가");
#endif

    string_free(s1);
    string_free(s2);
}

static void test_text_emoji_stays_stable(void)
{
    const char *emoji = "👩‍👩‍👧‍👦 🌍 🇬🇧";
    string_t *s = string_new_with(emoji);

    ASSERT_STREQ(string_c_str(s), emoji);

    string_free(s);
}

static void test_text_empty_stays_stable(void)
{
    string_t *s = string_new_with("");

    ASSERT_STREQ(string_c_str(s), "");

    string_free(s);
}

static void test_text_mutation_canonicalises_when_needed(void)
{
    string_t *append = string_new_with("e");
    string_t *insert = string_new_with("caf");

    string_append_cstr(append, "́");
    string_insert(insert, 3, "é");

#ifdef HAVE_UNISTRING
    ASSERT_STREQ(string_c_str(append), "é");
    ASSERT_STREQ(string_c_str(insert), "café");
#else
    ASSERT_STREQ(string_c_str(append), "é");
    ASSERT_STREQ(string_c_str(insert), "café");
#endif

    string_free(insert);
    string_free(append);
}

static void test_readme_example_Basic_UTF_8_Manipulation(void) {
    string_t *s = string_new_with("Héllo");

    /* Append UTF‑8 text */
    string_append_cstr(s, " 🌍");

    /* Insert at character index */
    string_insert(s, 1, "🙂");

    string_printf("%S\n", s);

    string_free(s);

}

static int print_character(rune_t rune, size_t index, void *user)
{
    (void)user;
    return string_printf("[%zu] %R\n", index, rune) < 0 ? -1 : 0;
}

static void test_readme_example_Character_Iteration(void) {
    string_t *s = string_new_with("👨‍👩‍👧‍👦 family");

    size_t count = string_length(s);

    string_printf("Characters: %zu\n", count);

    string_each(s, print_character, NULL);

    string_free(s);
}

static void test_readme_example_Using_the_Builder_API(void) {
    string_builder_t *b = string_builder_new();
    string_builder_append(b, "Hello");
    string_builder_append(b, ", ");
    string_builder_append(b, "世界");

    string_t *out = b;

    string_printf("%S\n", out);

    string_free(out);

}

static void test_readme_example_Escaping_Wide_C_Strings(void) {
    string_t *s = string_new_wide(L"hello \u03C0");

    string_printf("%S\n", s);

    string_free(s);
}

static void example_readme_examples(void) {
    test_readme_example_Basic_UTF_8_Manipulation();
    test_readme_example_Escaping_Wide_C_Strings();
    test_readme_example_Character_Iteration();
    test_readme_example_Using_the_Builder_API();
}

int tests_main(void)
{
    TEST_SECTION("Core");
    TEST_RUN_CASE(test_split_basic, NULL);
    TEST_RUN_CASE(test_join_basic, NULL);
    TEST_RUN_CASE(test_split_edge_cases, NULL);
    TEST_RUN_CASE(test_join_empty_fields, NULL);
    TEST_RUN_CASE(test_string_replace, NULL);
    TEST_RUN_CASE(test_string_new_wide, NULL);

    TEST_SECTION("Builder And Views");
    TEST_RUN_CASE(test_append_format, NULL);
    TEST_RUN_CASE(test_starts_with_ends_with, NULL);
    TEST_RUN_CASE(test_to_upper_and_to_lower, NULL);
    TEST_RUN_CASE(test_embedded_nul_is_real_string_content, NULL);
    TEST_RUN_CASE(test_text_character_api, NULL);
    TEST_RUN_CASE(test_string_view, NULL);
    TEST_RUN_CASE(test_string_cursor_view_rune_values, NULL);
    TEST_RUN_CASE(test_string_cursor_parsing_api, NULL);
    TEST_RUN_CASE(test_string_cursor_parser_example, NULL);
    TEST_RUN_CASE(test_string_object_first_api, NULL);
    TEST_RUN_CASE(test_string_builder, NULL);
    TEST_RUN_CASE(test_utf8_stuff, NULL);
    TEST_RUN_CASE(test_character_count_and_reverse, NULL);
    TEST_RUN_CASE(test_character_reverse_and_substring, NULL);

    TEST_SECTION("Text Intelligence");
    TEST_RUN_CASE(test_text_ascii_stays_stable, NULL);
    TEST_RUN_CASE(test_text_construction_canonicalises_combining_marks, NULL);
    TEST_RUN_CASE(test_text_construction_canonicalises_hangul, NULL);
    TEST_RUN_CASE(test_text_emoji_stays_stable, NULL);
    TEST_RUN_CASE(test_text_empty_stays_stable, NULL);
    TEST_RUN_CASE(test_text_mutation_canonicalises_when_needed, NULL);

    TEST_SECTION("README");
    TEST_RUN_OUTPUT_TAGS(example_readme_examples, "string,readme,output");

    return TEST_EXIT_CODE();
}
