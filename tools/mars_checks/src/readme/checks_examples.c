/**
 * @file checks_examples.c
 * @brief Parse Markdown fences into owned, stable README programme records.
 *
 * Follows the original fence-index identifiers and output association rules.
 * Headings and later C blocks end the search for expected output. This scanner
 * intentionally recognises the original triple-backtick grammar, not all CommonMark.
 */
#include <ctype.h>
#include <stdlib.h>

#include "checks_readme.h"

struct checks_example {
    string_t *id;
    string_t *path;
    string_t *code;
    string_t *expected;
    size_t line;
    bool has_main;
};

struct checks_examples {
    checks_example_t *values;
    size_t count;
    size_t capacity;
};

struct fence {
    size_t start;
    size_t end;
    size_t line;
    string_t *language;
    string_t *code;
};

static bool checks_examples_word(unsigned char c)
{
    return isalnum(c) || c == '_' || c >= 128;
}

static string_t *checks_examples_identifier(const string_t *path, size_t index)
{
    string_t *id = checks_text("");
    size_t length = string_byte_length(path);
    if (string_ends_with(path, ".md"))
        length -= 3;
    bool separator = false;
    for (size_t i = 0; i < length;) {
        if (!checks_examples_word(checks_byte(path, i))) {
            if (!separator)
                string_append_char(id, '_');
            separator = true;
            ++i;
        } else {
            size_t start = i++;
            while (i < length && checks_examples_word(checks_byte(path, i)))
                ++i;
            string_t *part = checks_slice(path, start, i - start);
            checks_append(id, part);
            string_free(part);
            separator = false;
        }
    }
    string_append_format(id, "_%03zu", index);
    return id;
}

static string_t *checks_examples_remove_typedefs(const string_t *source)
{
    string_t *result = checks_text("");
    size_t length = string_byte_length(source), start = 0;
    for (size_t i = 0; i < length; ++i) {
        if (!checks_at(source, i, "typedef") || (i && checks_examples_word(checks_byte(source, i - 1))) ||
            checks_examples_word(checks_byte(source, i + 7)))
            continue;
        size_t end = i + 7, depth = 0;
        bool valid = true;
        for (; end < length; ++end) {
            unsigned char c = checks_byte(source, end);
            if (c == '{' && ++depth > 1) {
                valid = false;
                break;
            }
            if (c == '}') {
                if (!depth) {
                    valid = false;
                    break;
                }
                --depth;
            }
            if (c == ';' && !depth)
                break;
        }
        if (!valid || end == length)
            continue;
        string_t *part = checks_slice(source, start, i - start);
        checks_append(result, part);
        string_free(part);
        start = end + 1;
        i = end;
    }
    string_t *tail = checks_slice(source, start, length - start);
    checks_append(result, tail);
    string_free(tail);
    return result;
}

static bool checks_examples_executable(const string_t *code, bool has_main)
{
    if (has_main)
        return true;
    string_t *clean = checks_strip_c(code, false), *declarations_removed = checks_examples_remove_typedefs(clean);
    bool result = string_find(clean, "#include") >= 0 ||
                  checks_match(declarations_removed, "(^|[^=!<>])=([^=]|$)", 0, NULL) ||
                  checks_match(declarations_removed, "^[[:space:]]*[[:alnum:]_]+[[:space:]]*\\(", 0, NULL) ||
                  checks_match(declarations_removed, "[[:alnum:]_]+[[:space:]]*\\([^;{}]*\\)[[:space:]]*\\{", 0, NULL);
    string_free(declarations_removed);
    string_free(clean);
    return result;
}

static struct fence *checks_examples_fences(const string_t *text, size_t *count)
{
    struct fence *blocks = NULL;
    size_t capacity = 0, length = string_byte_length(text), line = 1;
    *count = 0;
    for (size_t i = 0; i < length;) {
        size_t start = i, opening_line = line;
        while (i < length && checks_byte(text, i) != '\n')
            ++i;
        size_t line_end = i;
        if (i < length) {
            ++i;
            ++line;
        }
        if (!checks_at(text, start, "```") || line_end == length)
            continue;
        size_t body = i, closing = length, end = length;
        for (size_t j = i; j < length;) {
            size_t candidate = j;
            while (j < length && checks_byte(text, j) != '\n')
                ++j;
            if (checks_at(text, candidate, "```")) {
                size_t k = candidate + 3;
                while (k < j && (checks_byte(text, k) == ' ' || checks_byte(text, k) == '\t'))
                    ++k;
                if (k == j) {
                    closing = candidate;
                    end = j;
                    break;
                }
            }
            if (j < length)
                ++j;
        }
        if (closing == length)
            continue;
        if (*count == capacity) {
            capacity = capacity ? capacity * 2 : 16;
            struct fence *grown = realloc(blocks, capacity * sizeof(*blocks));
            if (!grown)
                checks_fatal("allocating Markdown fences");
            blocks = grown;
        }
        struct fence *block = &blocks[(*count)++];
        *block = (struct fence){.start = start,
                                .end = end,
                                .line = opening_line,
                                .language = checks_slice(text, start + 3, line_end - start - 3),
                                .code = checks_slice(text, body, closing - body)};
        string_trim(block->language);
        while (i < end) {
            if (checks_byte(text, i) == '\n')
                ++line;
            ++i;
        }
        if (i < length) {
            ++i;
            ++line;
        }
    }
    return blocks;
}

/* Discover complete and incomplete executable C examples. */
checks_examples_t *checks_examples_parse(const string_t *path, const string_t *text)
{
    checks_examples_t *examples = calloc(1, sizeof(*examples));
    if (!examples)
        checks_fatal("allocating README examples");
    size_t count = 0, index = 0;
    struct fence *blocks = checks_examples_fences(text, &count);
    for (size_t i = 0; i < count; ++i) {
        struct fence *block = &blocks[i];
        if (!checks_equal(block->language, "c"))
            continue;
        ++index;
        bool has_main = checks_match(block->code, "(^|[^[:alnum:]_])main[[:space:]]*\\(", 0, NULL);
        if (!checks_examples_executable(block->code, has_main))
            continue;
        if (examples->count == examples->capacity) {
            size_t capacity = examples->capacity ? examples->capacity * 2 : 16;
            checks_example_t *grown = realloc(examples->values, capacity * sizeof(*grown));
            if (!grown)
                checks_fatal("growing README examples");
            examples->values = grown;
            examples->capacity = capacity;
        }
        checks_example_t *example = &examples->values[examples->count++];
        *example = (checks_example_t){.id = checks_examples_identifier(path, index),
                                      .path = string_clone(path),
                                      .code = string_clone(block->code),
                                      .line = block->line,
                                      .has_main = has_main};
        for (size_t j = i + 1; j < count; ++j) {
            string_t *between = checks_slice(text, block->end, blocks[j].start - block->end);
            bool stop = checks_equal(blocks[j].language, "c") || checks_match(between, "^#{1,6} ", 0, NULL);
            string_free(between);
            if (stop)
                break;
            if (checks_equal(blocks[j].language, "text") || checks_equal(blocks[j].language, "json") ||
                checks_equal(blocks[j].language, "xml")) {
                example->expected = string_clone(blocks[j].code);
                break;
            }
        }
    }
    for (size_t i = 0; i < count; ++i) {
        string_free(blocks[i].language);
        string_free(blocks[i].code);
    }
    free(blocks);
    return examples;
}

/* Release all owned example fields. */
void checks_examples_free(checks_examples_t *examples)
{
    if (!examples)
        return;
    for (size_t i = 0; i < examples->count; ++i) {
        checks_example_t *example = &examples->values[i];
        string_free(example->id);
        string_free(example->path);
        string_free(example->code);
        string_free(example->expected);
    }
    free(examples->values);
    free(examples);
}

/* Report discovered executable count. */
size_t checks_examples_count(const checks_examples_t *examples)
{
    return examples->count;
}

/* Borrow a single parsed example. */
const checks_example_t *checks_examples_get(const checks_examples_t *examples, size_t index)
{
    return index < examples->count ? &examples->values[index] : NULL;
}

/* Borrow the stable configuration identifier. */
const string_t *checks_example_id(const checks_example_t *example)
{
    return example->id;
}

/* Borrow the originating path. */
const string_t *checks_example_path(const checks_example_t *example)
{
    return example->path;
}

/* Borrow programme source. */
const string_t *checks_example_code(const checks_example_t *example)
{
    return example->code;
}

/* Borrow expected output, if supplied. */
const string_t *checks_example_expected(const checks_example_t *example)
{
    return example->expected;
}

/* Report the opening fence's source line. */
size_t checks_example_line(const checks_example_t *example)
{
    return example->line;
}

/* Report the programme entry-point classification. */
bool checks_example_has_main(const checks_example_t *example)
{
    return example->has_main;
}

/* Match Python's line-wise trailing-whitespace output comparison. */
string_t *checks_output_normalise(const string_t *text)
{
    string_t *result = checks_text("");
    size_t length = string_byte_length(text), start = 0, pending = 0;
    for (size_t i = 0; i <= length; ++i) {
        unsigned char c = checks_byte(text, i);
        if (i != length && c != '\n' && c != '\r' && c != '\v' && c != '\f')
            continue;
        size_t end = i;
        while (end > start && isspace(checks_byte(text, end - 1)))
            --end;
        if (end > start) {
            if (string_byte_length(result))
                for (size_t j = 0; j <= pending; ++j)
                    string_append_char(result, '\n');
            string_t *part = checks_slice(text, start, end - start);
            checks_append(result, part);
            string_free(part);
            pending = 0;
        } else if (string_byte_length(result)) {
            ++pending;
        }
        if (c == '\r' && checks_byte(text, i + 1) == '\n')
            ++i;
        start = i + 1;
    }
    return result;
}
