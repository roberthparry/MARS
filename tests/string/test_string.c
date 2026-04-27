#include "ustring.h"
#include "test_string.h"

/* ------------------------------------------------------------------------- */
/* Tests                                                                     */
/* ------------------------------------------------------------------------- */

int test_split_basic(void)
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
    return 0;
}

int test_join_basic(void)
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
    return 0;
}

int test_split_edge_cases(void)
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
    return 0;
}

int test_join_empty_fields(void)
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
    return 0;
}

int test_string_replace(void)
{
    string_t *s = string_new_with("the cat sat on the mat");
    string_replace(s, "at", "oodle");

    ASSERT_STREQ(string_c_str(s), "the coodle soodle on the moodle");

    string_free(s);
    return 0;
}

int test_append_format(void)
{
    string_t *s = string_new_with("Hello");

    string_append_char(s, ' ');
    string_append_cstr(s, "world");
    string_append_format(s, " %d + %d = %d", 2, 3, 5);

    ASSERT_STREQ(string_c_str(s), "Hello world 2 + 3 = 5");

    string_replace(s, "world", "universe");
    ASSERT_STREQ(string_c_str(s), "Hello universe 2 + 3 = 5");

    string_free(s);
    return 0;
}

int test_starts_with_ends_with(void)
{
    string_t *s = string_new_with("Hello universe!");

    ASSERT_TRUE(string_starts_with(s, "Hello"));
    ASSERT_TRUE(string_ends_with(s, "verse!"));

    ASSERT_EQ(string_find(s, "uni"), 6);

    string_t *sub = string_substr(s, 6, 8);
    ASSERT_STREQ(string_c_str(sub), "universe");

    string_free(sub);
    string_free(s);
    return 0;
}

int test_to_upper_and_to_lower(void)
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
    return 0;
}

int test_string_view(void)
{
    string_t *s = string_new_with("Hello Universe");

    string_reverse(s);
    string_reverse(s); /* restore */

    string_view_t v = string_view(s, 6, 8);
    string_t *sub = string_from_view(&v);

    ASSERT_STREQ(string_c_str(sub), "Universe");

    ASSERT_TRUE(string_view_equals(&v, "Universe"));

    string_free(s);
    string_free(sub);

    return 0;
}

int test_string_builder(void)
{
    string_t *s = string_new_with("alpha,beta,gamma,delta");

    size_t n;
    string_view_t *views = string_split_view(s, ",", &n);

    ASSERT_EQ(n, 4);

    string_split_view_free(views);

    char buf[64];
    string_buffer_t sb;
    string_buffer_init(&sb, buf, sizeof(buf));
    string_buffer_append(&sb, "Hello");
    string_buffer_append_char(&sb, '!');

    ASSERT_STREQ(sb.data, "Hello!");

    string_builder_t *b = string_builder_new();
    string_builder_format(b, "Pi approx = %.3f", 3.14159);

    ASSERT_STREQ(string_c_str(b), "Pi approx = 3.142");

    string_builder_free(b);
    string_free(s);
    return 0;
}

int test_utf8_stuff(void)
{
    string_t *s = string_new_with("Héllo 🌍");

    ASSERT_EQ(string_utf8_length(s), 7);

    string_utf8_reverse(s);
    string_utf8_to_upper(s);

    string_free(s);
    return 0;
}

int test_grapheme(void)
{
    string_t *s = string_new_with("👩‍👩‍👧‍👦 café 🇬🇧");

    ASSERT_TRUE(string_grapheme_count(s) > 0);

    string_grapheme_reverse(s);

    string_free(s);
    return 0;
}

int test_grapheme_reverse_and_substr(void)
{
    string_t *s = string_new_with("👩‍👩‍👧‍👦 café 🇬🇧");

    size_t g = string_grapheme_count(s);
    ASSERT_TRUE(g > 0);

    string_grapheme_reverse(s);

    string_t *sub = string_grapheme_substr(s, 1, 3);
    ASSERT_TRUE(sub != NULL);

    string_free(sub);
    string_free(s);
    return 0;
}

int test_norm_ascii_inplace(void)
{
    string_t *s = string_new_with("Hello World");

    int r = string_normalize(s, STRING_NORM_NFC);
    ASSERT_EQ(r, 0);
    ASSERT_STREQ(string_c_str(s), "Hello World");

    string_free(s);
    return 0;
}

int test_norm_combining_inplace(void)
{
    string_t *s1 = string_new_with("é");   // NFD
    string_t *s2 = string_new_with("é");    // NFC

    int r1 = string_normalize(s1, STRING_NORM_NFC);
    int r2 = string_normalize(s2, STRING_NORM_NFD);

    ASSERT_EQ(r1, 0);
    ASSERT_EQ(r2, 0);

#ifdef HAVE_UNISTRING
    ASSERT_STREQ(string_c_str(s1), "é");    // NFD → NFC
    ASSERT_STREQ(string_c_str(s2), "é");   // NFC → NFD
#else
    ASSERT_STREQ(string_c_str(s1), "é");   // no-op fallback
    ASSERT_STREQ(string_c_str(s2), "é");
#endif

    string_free(s1);
    string_free(s2);
    return 0;
}

int test_norm_hangul_inplace(void)
{
    string_t *s1 = string_new_with("가");  // NFD
    string_t *s2 = string_new_with("가");  // NFC

    int r1 = string_normalize(s1, STRING_NORM_NFC);
    int r2 = string_normalize(s2, STRING_NORM_NFD);

    ASSERT_EQ(r1, 0);
    ASSERT_EQ(r2, 0);

#ifdef HAVE_UNISTRING
    ASSERT_STREQ(string_c_str(s1), "가");
    ASSERT_STREQ(string_c_str(s2), "가");
#else
    ASSERT_STREQ(string_c_str(s1), "가");
    ASSERT_STREQ(string_c_str(s2), "가");
#endif

    string_free(s1);
    string_free(s2);
    return 0;
}

int test_norm_emoji_inplace(void)
{
    const char *emoji = "👩‍👩‍👧‍👦 🌍 🇬🇧";
    string_t *s = string_new_with(emoji);

    int r = string_normalize(s, STRING_NORM_NFC);
    ASSERT_EQ(r, 0);
    ASSERT_STREQ(string_c_str(s), emoji);

    string_free(s);
    return 0;
}

int test_norm_empty_inplace(void)
{
    string_t *s = string_new_with("");

    int r = string_normalize(s, STRING_NORM_NFC);
    ASSERT_EQ(r, 0);
    ASSERT_STREQ(string_c_str(s), "");

    string_free(s);
    return 0;
}

int test_norm_invalid_form_inplace(void)
{
    string_t *s = string_new_with("test");

    int r = string_normalize(s, (string_norm_form_t)999);
    ASSERT_EQ(r, -1);
    ASSERT_STREQ(string_c_str(s), "test");

    string_free(s);
    return 0;
}

int test_readme_example_Basic_UTF_8_Manipulation(void) {
    string_t *s = string_new_with("Héllo");

    /* Append UTF‑8 text */
    string_append_cstr(s, " 🌍");

    /* Insert at grapheme index */
    string_insert(s, 1, "🙂");

    /* Normalize to NFC */
    string_normalize(s, STRING_NORM_NFC);

    printf("%s\n", string_c_str(s));

    string_free(s);

    return 0;
}

int test_readme_example_Grapheme_Iteration(void) {
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

int test_readme_example_Using_the_Builder_API(void) {
    string_builder_t *b = string_builder_new();
    string_builder_append(b, "Hello");
    string_builder_append(b, ", ");
    string_builder_append(b, "世界");

    string_t *out = b;

    printf("%s\n", string_c_str(out));

    string_free(out);

    return 0;
}

int test_readme_examples(void) {
    test_readme_example_Basic_UTF_8_Manipulation();
    test_readme_example_Grapheme_Iteration();
    test_readme_example_Using_the_Builder_API();
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int main(void)
{
    TEST(test_split_basic);
    TEST(test_join_basic);
    TEST(test_split_edge_cases);
    TEST(test_join_empty_fields);
    TEST(test_string_replace);

    TEST(test_append_format);
    TEST(test_starts_with_ends_with);
    TEST(test_to_upper_and_to_lower);
    TEST(test_string_view);
    TEST(test_string_builder);
    TEST(test_utf8_stuff);
    TEST(test_grapheme);
    TEST(test_grapheme_reverse_and_substr);

    TEST(test_norm_ascii_inplace);
    TEST(test_norm_combining_inplace);
    TEST(test_norm_hangul_inplace);
    TEST(test_norm_emoji_inplace);
    TEST(test_norm_empty_inplace);
    TEST(test_norm_invalid_form_inplace);

     printf("Running README examples...\n");
     TEST(test_readme_examples);

    return tests_failed;
}

