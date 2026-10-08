/**
 * @file checks_distribution.c
 * @brief Prevent private ESAA reference paths entering the public Git index.
 *
 * Both full-index and staged ACMR checks preserve original path spelling in
 * diagnostics. Normalisation handles Windows separators and repeated ./ prefixes.
 * Only the three bounded policy patterns are compared; Git paths are sorted.
 */
#include <stdio.h>

#include "checks_process.h"
#include "checks_controls.h"

/* Apply the private-reference path policy. */
bool checks_forbidden_path(const string_t *path)
{
    string_t *normalised = string_clone(path);
    string_replace(normalised, "\\", "/");
    size_t start = 0;
    while (checks_at(normalised, start, "./"))
        start += 2;
    string_t *trimmed = checks_slice(normalised, start, string_byte_length(normalised) - start);
    string_free(normalised);
    string_to_lower(trimmed);
    /* Only folds producing ASCII can match the ASCII-only reserved paths. The
     * remaining multi-character Latin folds have this fixed, bounded inventory. */
    static const struct {
        const char *source;
        const char *folded;
    } folds[] = {{"ß", "ss"}, {"ẞ", "ss"},  {"ſ", "s"},   {"K", "k"},  {"ﬀ", "ff"}, {"ﬁ", "fi"},
                 {"ﬂ", "fl"}, {"ﬃ", "ffi"}, {"ﬄ", "ffl"}, {"ﬅ", "st"}, {"ﬆ", "st"}};
    for (size_t i = 0; i < sizeof(folds) / sizeof(*folds); ++i)
        string_replace(trimmed, folds[i].source, folds[i].folded);
    bool forbidden = string_starts_with(trimmed, "docs/books/esaa/") ||
                     checks_equal(trimmed, "docs/explanatory supplement to the astronomical almanac.md") ||
                     checks_equal(trimmed, "src/almanac/explanatory supplement to the astronomical almanac.pdf");
    string_free(trimmed);
    return forbidden;
}

/* Check index paths and emit deterministic diagnostics. */
int checks_public_distribution(const string_t *root, bool staged)
{
    checks_strings_t *paths = checks_git_paths(root, staged);
    if (!paths) {
        fprintf(stderr, "public-distribution check could not inspect the Git index\n");
        return 2;
    }
    bool failed = false;
    for (size_t i = 0; i < checks_strings_count(paths); ++i) {
        const string_t *path = checks_strings_get(paths, i);
        if (!checks_forbidden_path(path))
            continue;
        if (!failed)
            fprintf(stderr, "private ESAA reference material must not be included in the public repository:\n");
        string_fprintf(stderr, "  %s\n", string_c_str(path));
        failed = true;
    }
    if (failed)
        fprintf(stderr, "keep the local files in their ignored locations and remove them from the Git index\n");
    checks_strings_free(paths);
    return failed ? 1 : 0;
}
