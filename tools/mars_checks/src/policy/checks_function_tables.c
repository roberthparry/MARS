/**
 * @file checks_function_tables.c
 * @brief Source-level expression registry layout and perfect-hash audits.
 *
 * Reads registry declarations without including expression implementation
 * headers. Recomputes both unsigned 32-bit hashes with public UTF-8 views,
 * checks unique slots and names, displacement ranges, sign aliases and aligned
 * single-line parser entries. Unicode columns count scalar values, as in the
 * original policy; terminal display widths are intentionally not inferred.
 */
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "checks_policy.h"

static void policy_table_error(checks_strings_t *errors, const char *message)
{
    checks_strings_add(errors, checks_text(message));
}

static bool policy_number(const string_t *source, const char *pattern, uint32_t *value)
{
    string_t *capture = NULL;
    bool ok = checks_match(source, pattern, 1, &capture);
    if (ok) {
        char *end = NULL;
        unsigned long number = strtoul(string_c_str(capture), &end, 0);
        ok = end && !*end && number <= UINT32_MAX;
        if (ok)
            *value = (uint32_t)number;
    }
    string_free(capture);
    return ok;
}

static string_t *policy_table_body(const string_t *source, const char *pattern)
{
    string_t *header = NULL;
    if (!checks_match(source, pattern, 0, &header))
        return NULL;
    const char *s = string_c_str(source);
    const char *start = strstr(s, string_c_str(header)) + string_byte_length(header);
    const char *end = strstr(start, "\n};");
    string_free(header);
    return end ? checks_slice(source, (size_t)(start - s), (size_t)(end - start)) : NULL;
}

static size_t policy_columns(const char *start, const char *end)
{
    size_t columns = 0;
    for (; start < end; ++start)
        columns += ((unsigned char)*start & 0xc0u) != 0x80u;
    return columns;
}

static bool policy_row_columns(const string_t *row, size_t slot, size_t columns[9])
{
    const char *s = string_c_str(row);
    string_t *prefix = string_sprintf("    [%3zu] = { ", slot);
    bool ok = string_starts_with(row, string_c_str(prefix)) && string_ends_with(row, " },") && !strchr(s, '\t');
    string_free(prefix);
    size_t fields = 0;
    /* Nine fixed field classes; dispatch is bounded independently of table size. */
    for (const char *p = s; *p; ++p) {
        if (*p == '=' && (p == s || p[-1] != ' ' || p[1] != ' '))
            ok = false;
        if (*p != '.')
            continue;
        size_t key = 9, width = 0;
        if (!strncmp(p, ".kw = ", 6)) {
            key = 0;
            width = 6;
        } else if (!strncmp(p, ".arity = ", 9)) {
            key = 1;
            width = 9;
        } else if (!strncmp(p, ".ops = ", 7)) {
            key = 3;
            width = 7;
        } else if (p[1] && strchr("ubtsv", p[1]) && !strncmp(p + 2, "fn = ", 5)) {
            const char *handlers = "ubtsv";
            key = 4 + (size_t)(strchr(handlers, p[1]) - handlers);
            width = 7;
        }
        if (!width)
            continue;
        if (fields == 2)
            key = 2;
        ++fields;
        size_t column = policy_columns(s, p);
        if (columns[key] == SIZE_MAX)
            columns[key] = column;
        ok = columns[key] == column && ok;
    }
    return ok && fields >= 3 && policy_columns(s, s + strlen(s)) <= (fields == 5 ? 140u : 130u);
}

static uint32_t policy_sample(const string_t *keyword, size_t position)
{
    uint32_t value = 0;
    string_pos_t next = 0;
    if (position < string_byte_length(keyword) &&
        ((unsigned char)string_c_str(keyword)[position] & 0xc0u) != 0x80u)
        string_view_peek_rune_value(string_view_all(keyword), position, &value, &next);
    return value;
}

static uint32_t policy_inline_hash(const string_t *keyword, uint32_t seed)
{
    size_t length = string_byte_length(keyword);
    uint32_t value = (seed ^ (uint32_t)length) * UINT32_C(16777619);
    const size_t positions[] = {0, length - 1, length - 3, 1, 3, 2};
    for (size_t i = 0; i < sizeof(positions) / sizeof(*positions); ++i)
        value = (value ^ policy_sample(keyword, positions[i])) * UINT32_C(16777619);
    return value;
}

static uint32_t policy_binding_hash(const string_t *keyword, uint32_t seed, uint32_t size)
{
    uint32_t value = seed ^ (uint32_t)string_byte_length(keyword), index = 1, rune = 0;
    string_pos_t position = 0, next = 0;
    while (string_view_peek_rune_value(string_view_all(keyword), position, &rune, &next)) {
        value ^= rune + UINT32_C(0x9e3779b9) + index++ * UINT32_C(0x85ebca6b);
        value *= UINT32_C(16777619);
        value ^= value >> 13;
        position = next;
    }
    return value % size;
}

static bool policy_shifts(const string_t *body, uint32_t *shifts, size_t count)
{
    const char *p = string_c_str(body);
    size_t index = 0;
    while (*p) {
        if (isspace((unsigned char)*p) || *p == ',') {
            ++p;
            continue;
        }
        if (!isdigit((unsigned char)*p) || index == count)
            return false;
        char *end = NULL;
        unsigned long value = strtoul(p, &end, 10);
        if (value > 255)
            return false;
        shifts[index++] = (uint32_t)value;
        p = end;
        if (*p == 'u')
            ++p;
    }
    return index == count;
}

static bool policy_binding_seeds(const string_t *source, uint32_t seeds[2])
{
    checks_strings_t *lines = checks_split(source, '\n');
    size_t count = 0;
    for (size_t i = 0; i < checks_strings_count(lines); ++i) {
        uint32_t value = 0;
        if (policy_number(checks_strings_get(lines, i),
                          "return binding_func_hash_values\\(kw, (0x[0-9a-f]+)u\\);", &value)) {
            if (count < 2)
                seeds[count] = value;
            ++count;
        }
    }
    checks_strings_free(lines);
    return count == 2;
}

/* Check the declared table independently of the library's lookup implementation. */
void checks_policy_function_table(const string_t *source, bool binding, checks_strings_t *errors)
{
    uint32_t size = 0, buckets = 0, max_bytes = 0, seeds[2] = {0};
    const char *size_pattern = binding ? "#define BINDING_FUNC_TABLE_SIZE ([0-9]+)" :
                                         "#define FUNC_TABLE_SIZE ([0-9]+)";
    bool metadata = policy_number(source, size_pattern, &size) && size > 0 && size <= 1048576;
    if (binding) {
        buckets = size;
        metadata = policy_binding_seeds(source, seeds) && metadata;
    } else {
        metadata = policy_number(source, "#define FUNC_HASH_BUCKETS ([0-9]+)", &buckets) && metadata;
        metadata = policy_number(source, "#define FUNC_KEYWORD_MAX_BYTES ([0-9]+)", &max_bytes) && metadata;
        metadata = policy_number(source, "unsigned hash = \\(([0-9]+)u \\^", &seeds[0]) && metadata;
    }
    if (!metadata || !buckets || buckets > 1048576) {
        policy_table_error(errors, "missing or invalid function-table sizes or hash seeds");
        return;
    }
    string_t *body = policy_table_body(source, binding ?
        "static const [[:alnum:]_]+ s_binding_funcs\\[[[:alnum:]_]+\\] = \\{\n" :
        "static const [[:alnum:]_]+ s_funcs\\[[[:alnum:]_]+\\] = \\{\n");
    string_t *displacements = policy_table_body(source, binding ?
        "s_binding_func_displacements\\[[[:alnum:]_]+\\] = \\{\n" :
        "s_func_displacements\\[[[:alnum:]_]+\\] = \\{\n");
    if (!body || !displacements) {
        policy_table_error(errors, "missing or malformed function registry or displacement table");
        string_free(body);
        string_free(displacements);
        return;
    }
    uint32_t *shifts = calloc(buckets, sizeof(*shifts));
    bool *used = calloc(buckets, sizeof(*used)), *slots = calloc(size, sizeof(*slots));
    if (!shifts || !used || !slots)
        checks_fatal("allocating registry audit");
    bool shifts_ok = policy_shifts(displacements, shifts, buckets);
    if (!shifts_ok)
        policy_table_error(errors, "displacement count or byte range is invalid");
    checks_strings_t *rows = checks_split(body, '\n'), *keywords = checks_strings_new();
    size_t columns[9];
    for (size_t i = 0; i < 9; ++i)
        columns[i] = SIZE_MAX;
    unsigned aliases = 0;
    size_t row_count = checks_strings_count(rows);
    if (!binding && row_count != size)
        policy_table_error(errors, "inline table must contain one entry per slot");
    for (size_t i = 0; i < row_count; ++i) {
        const string_t *row = checks_strings_get(rows, i);
        string_t *slot_text = NULL, *keyword = NULL, *fields = NULL;
        const char *pattern = "^[[:space:]]*\\[[[:space:]]*([0-9]+)\\] = \\{ \\.kw = \"([^\"]+)\",(.*) \\},$";
        bool valid = checks_match(row, pattern, 1, &slot_text) &&
                     checks_match(row, pattern, 2, &keyword) && checks_match(row, pattern, 3, &fields);
        if (!valid) {
            checks_strings_add(errors, string_sprintf("registry row %zu: expected one complete entry", i + 1));
        } else {
            unsigned long slot = strtoul(string_c_str(slot_text), NULL, 10);
            if (slot >= size || slots[slot])
                checks_strings_add(errors, string_sprintf("registry row %zu: duplicate or out-of-range slot", i + 1));
            else
                slots[slot] = true;
            uint32_t hash = binding ? policy_binding_hash(keyword, seeds[1], size) :
                                      policy_inline_hash(keyword, seeds[0]);
            uint32_t bucket = binding ? policy_binding_hash(keyword, seeds[0], size) : (hash >> 16) % buckets;
            used[bucket] = true;
            if (shifts_ok && (hash % size + shifts[bucket]) % size != slot)
                checks_strings_add(errors, string_sprintf("keyword %s: perfect-hash slot mismatch", string_c_str(keyword)));
            if (!binding && string_byte_length(keyword) > max_bytes)
                policy_table_error(errors, "keyword exceeds FUNC_KEYWORD_MAX_BYTES");
            if (!binding && !policy_row_columns(row, i, columns))
                checks_strings_add(errors, string_sprintf("registry row %zu: misaligned columns or excessive width", i + 1));
            unsigned alias = checks_equal(keyword, "sgn") ? 1u : checks_equal(keyword, "sign") ? 2u :
                             checks_equal(keyword, "signum") ? 4u : 0u;
            if (alias) {
                aliases |= alias;
                const char *handler = binding ? "\\.is_binary = false,[[:space:]]+\\.ops = &ops_sgn$" :
                    "\\.arity = 1u,[[:space:]]+\\.ufn = expr_sgn,[[:space:]]+\\.ops = &ops_sgn$";
                if (!checks_match(fields, handler, 0, NULL))
                    policy_table_error(errors, "sign alias has the wrong arity or handler");
            }
            checks_strings_add(keywords, keyword);
            keyword = NULL;
        }
        string_free(slot_text);
        string_free(keyword);
        string_free(fields);
    }
    if (aliases != 7u)
        policy_table_error(errors, "registry must contain sgn, sign and signum aliases");
    checks_strings_sort(keywords);
    for (size_t i = 1; i < checks_strings_count(keywords); ++i)
        if (!string_compare(checks_strings_get(keywords, i - 1), checks_strings_get(keywords, i)))
            policy_table_error(errors, "duplicate registry keyword");
    if (!binding && shifts_ok)
        for (size_t i = 0; i < buckets; ++i)
            if (!used[i] && shifts[i])
                policy_table_error(errors, "unused hash bucket has non-zero displacement");
    checks_strings_free(keywords);
    checks_strings_free(rows);
    free(shifts);
    free(used);
    free(slots);
    string_free(body);
    string_free(displacements);
}
