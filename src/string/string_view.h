#ifndef STRING_VIEW_H
#define STRING_VIEW_H

#include <stddef.h>

#include "ustring.h"

/* Raw byte bridges are for parsers that are still being migrated onto
   cursor operations. They do not expose string_t internals or view layout. */
string_view_t string_view_from_chars(const char *data, size_t len);
const char *string_view_data(string_view_t view);
const char *string_view_end(string_view_t view);

/* View-based operations shared inside the string implementation. The public
   API exposes string_t and C-string boundary helpers instead. */
int string_append_view(string_t *s, string_view_t suffix);
int string_insert_view(string_t *s, size_t pos, string_view_t text);
string_offset_t string_find_view(const string_t *s, string_view_t needle);
bool string_starts_with_view(const string_t *s, string_view_t prefix);
bool string_ends_with_view(const string_t *s, string_view_t suffix);
int string_replace_view(string_t *s, string_view_t search, string_view_t replace);
string_t **string_split_by_view(const string_t *s, string_view_t delim, size_t *out_count);
string_t *string_join_with_view(string_t **arr, size_t count, string_view_t sep);
string_view_t *string_split_view_by_view(const string_t *s, string_view_t delim, size_t *out_count);
void string_split_view_free(string_view_t *views);
int string_builder_append_view(string_builder_t *b, string_view_t s);

#endif
