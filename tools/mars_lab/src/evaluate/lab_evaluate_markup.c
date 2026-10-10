/**
 * @file lab_evaluate_markup.c
 * @brief Escaped native result markup for lexical text and matrix layouts.
 *
 * Uses prepared matrix terms without changing their algebra or source cells.
 * Grid dimensions are native integer counts; all authored text is escaped.
 * Shared text escaping also serves Function syntax spans. Browser code installs
 * the resulting markup while CSS selects the available-width layout.
 */
#include "lab_evaluate_internal.h"
#include "lab_presentation_internal.h"
#include "ustring.h"

/* Escape text nodes without changing Unicode or carriage returns. */
bool lab_pres_html_text(string_t *html, const string_t *text)
{
    string_cursor_t *cursor = string_cursor_new(text);
    if (!cursor)
        return false;
    bool ok = true;
    while (ok && !string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);
        unsigned value = rune_value(rune);
        const char *replacement = value == '&'    ? "&amp;"
                                  : value == '<'  ? "&lt;"
                                  : value == '>'  ? "&gt;"
                                  : value == '\r' ? "&#13;"
                                                  : NULL;
        ok = !(replacement ? string_append_cstr(html, replacement) : string_append_rune(html, rune));
        string_cursor_next(cursor);
    }
    string_cursor_free(cursor);
    return ok;
}

/* Build one complete layout or an empty fragment for the plain-text fallback. */
string_t *lab_pres_matrix_markup(const json_t *terms)
{
    string_t *html = string_new();
    if (!html || !json_array_size(terms))
        return html;
    bool ok = !string_append_cstr(html, "<span class=\"matrix-sum-display\">");
    for (size_t i = 0; ok && i < json_array_size(terms); ++i) {
        const json_t *term = json_array_get(terms, i), *rows = lab_eval_get(term, "rows");
        const string_t *factor = json_string_value(lab_eval_get(term, "factor"));
        if (i)
            ok = !string_append_cstr(html, "<span class=\"matrix-sum-operator\">+</span>");
        ok = ok && !string_append_cstr(html, "<span class=\"matrix-term-display\">");
        if (factor && string_byte_length(factor))
            ok = ok && !string_append_cstr(html, "<span class=\"matrix-factor\">") &&
                 lab_pres_html_text(html, factor) &&
                 !string_append_cstr(html, "</span><span class=\"matrix-product-operator\">·</span>");
        size_t width = json_array_size(json_array_get(rows, 0));
        ok = ok && width &&
             string_append_format(html,
                                  "<span class=\"matrix-display\"><span class=\"matrix-bracket\">(</span>"
                                  "<span class=\"matrix-grid\" style=\"--matrix-columns:%zu\">",
                                  width) >= 0;
        for (size_t r = 0; ok && r < json_array_size(rows); ++r) {
            const json_t *row = json_array_get(rows, r);
            ok = json_array_size(row) == width;
            for (size_t c = 0; ok && c < width; ++c) {
                const string_t *cell = json_string_value(json_array_get(row, c));
                ok = cell && !string_append_cstr(html, "<span class=\"matrix-cell\">") &&
                     lab_pres_html_text(html, cell) && !string_append_cstr(html, "</span>");
            }
        }
        ok = ok && !string_append_cstr(html, "</span><span class=\"matrix-bracket\">)</span></span></span>");
    }
    ok = ok && !string_append_cstr(html, "</span>");
    if (!ok) {
        string_free(html);
        return NULL;
    }
    return html;
}
