#ifndef EXPR_PARSE_TEXT_H
#define EXPR_PARSE_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ustring.h"

bool expr_parse_cursor_at_identifier_boundary(const string_cursor_t *cursor,
                                              string_pos_t pos);
bool expr_parse_cursor_consume_char(string_cursor_t *cursor, char ch);
bool expr_parse_cursor_peek_value_at(const string_cursor_t *cursor,
                                     string_pos_t pos,
                                     uint32_t *out,
                                     size_t *width_out);
bool expr_parse_cursor_peek_value(const string_cursor_t *cursor,
                                  uint32_t *out,
                                  size_t *width_out);
bool expr_parse_view_peek_ascii(string_view_t view,
                                string_pos_t pos,
                                unsigned char *out);
bool expr_parse_view_peek_value(string_view_t view,
                                string_pos_t pos,
                                uint32_t *out,
                                size_t *width_out);

bool expr_parse_is_superscript_digit(uint32_t value);
int expr_parse_superscript_digit_value(uint32_t value);
bool expr_parse_is_subscript_digit(uint32_t value);
bool expr_parse_is_fraction_glyph(uint32_t value);

size_t expr_parse_scan_unicode_fraction_len(string_view_t view,
                                            string_pos_t pos);
size_t expr_parse_scan_special_number_len(string_view_t view,
                                          string_pos_t pos,
                                          bool include_unicode_infinity,
                                          bool require_identifier_boundary);
size_t expr_parse_scan_decimal_len(string_view_t view, string_pos_t pos);
size_t expr_parse_scan_number_atom_len(string_view_t view,
                                       string_pos_t pos,
                                       bool include_special_numbers);

bool expr_parse_view_starts_with_text(string_view_t view,
                                      const char *text,
                                      bool case_insensitive);

string_t *expr_parse_read_name(string_cursor_t *cursor,
                               bool allow_plain_letters_after_first);
int expr_parse_read_superscript_int(string_cursor_t *cursor);

#endif /* EXPR_PARSE_TEXT_H */
