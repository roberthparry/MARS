/**
 * @file tba_local.c
 * @brief Bounded local ingestion and request projection for the private Lab.
 *
 * Recognises quoted delimiters, doubled quotes, CRLF and embedded line breaks.
 * Raw documents and unselected cells remain in the browser. Requests contain
 * selected columns only and never include filenames or local paths.
 */
#include "tba_ui.h"
#include "tba_local.h"

/* Parse browser-owned strings through scoped text capabilities. */
jsref tba_local_parse(jsref text)
{
    unsigned length = tba_dom_length(text);
    if (!length || length > 16u * 1024u * 1024u)
        return 0;
    int document = tba_dom_object(0), rows = tba_dom_object(1), index = tba_dom_object(0);
    unsigned position = tba_dom_char(text, 0) == 0xfeff ? 1 : 0, width = 0, cells = 0;
    while (position < length) {
        unsigned scope = tba_dom_mark();
        int row = tba_dom_object(1);
        int more = 1;
        while (more) {
            unsigned field_scope = tba_dom_mark(), start = position;
            int parts = tba_dom_object(1), quoted = tba_dom_char(text, position) == '"', closed = !quoted;
            unsigned field_length = 0;
            if (quoted)
                start = ++position;
            while (position < length) {
                unsigned ch = tba_dom_char(text, position);
                if (!ch)
                    return 0;
                if (quoted && ch == '"') {
                    tba_dom_push(parts, tba_dom_slice(text, start, position));
                    ++position;
                    if (tba_dom_char(text, position) == '"') {
                        tba_dom_push(parts, tba_dom_string("\""));
                        start = ++position;
                        if (++field_length > 256)
                            return 0;
                        continue;
                    }
                    closed = 1;
                    start = position;
                    break;
                }
                if (!quoted && (ch == ',' || ch == '\r' || ch == '\n'))
                    break;
                if (!quoted && ch == '"')
                    return 0;
                if (++field_length > 256)
                    return 0;
                ++position;
            }
            if (!closed)
                return 0;
            tba_dom_push(parts, tba_dom_slice(text, start, position));
            tba_dom_push(row, tba_dom_join(parts, ""));
            tba_dom_release(field_scope);
            if (++cells > 1000000 || tba_dom_count(row) > 256)
                return 0;
            unsigned ch = tba_dom_char(text, position);
            if (position < length && ch != ',' && ch != '\r' && ch != '\n')
                return 0;
            more = position < length && ch == ',';
            if (position < length)
                ++position;
            if (ch == '\r' && tba_dom_char(text, position) == '\n')
                ++position;
        }
        if (tba_dom_count(row) == 1 && !tba_dom_truth(tba_dom_item(row, 0))) {
            tba_dom_release(scope);
            continue;
        }
        if (!width) {
            width = tba_dom_count(row);
            int headers = tba_dom_object(1);
            for (unsigned c = 0; c < width; ++c) {
                int name = tba_dom_clean(tba_dom_item(row, c), 0);
                if (!tba_dom_truth(name) || tba_dom_truth(tba_dom_key_get(index, name)))
                    return 0;
                tba_dom_key_set(index, name, tba_dom_numeric(c + 1));
                tba_dom_push(headers, name);
            }
            tba_dom_set(document, "headers", headers);
        } else {
            if (tba_dom_count(row) != width || tba_dom_count(rows) >= 100000)
                return 0;
            tba_dom_push(rows, row);
        }
        tba_dom_release(scope);
    }
    tba_dom_set(document, "rows", rows);
    tba_dom_set(document, "index", index);
    return width > 1 && tba_dom_count(rows) ? document : 0;
}

/* Keep the local heading list independent from the reduced network projection. */
jsref tba_local_columns(int driver)
{
    int document = tba_ui_state(driver ? "local_driver" : "local_target");
    int headers = tba_dom_get(document, "headers"), result = tba_dom_object(1);
    int date = tba_ui_value(driver ? "xreg_date_column" : "target_date_column");
    for (unsigned c = 0; c < tba_dom_count(headers); ++c) {
        unsigned scope = tba_dom_mark();
        int name = tba_dom_item(headers, c);
        if (!tba_dom_equal(name, date))
            tba_dom_push(result, name);
        tba_dom_release(scope);
    }
    return result;
}

/* Project only the requested columns; headers and cells stay opaque until native validation. */
jsref tba_local_project(int driver)
{
    int document = tba_ui_state(driver ? "local_driver" : "local_target");
    if (!document)
        return 0;
    int index = tba_dom_get(document, "index"), source = tba_dom_get(document, "rows");
    int headers = tba_dom_object(1), selected = tba_dom_object(1), rows = tba_dom_object(1);
    int date = tba_ui_value(driver ? "xreg_date_column" : "target_date_column");
    int position = tba_ui_integer(tba_dom_key_get(index, date));
    if (!position)
        return 0;
    tba_dom_push(headers, date);
    tba_dom_push(selected, tba_dom_numeric(position - 1));
    int names = driver ? tba_dom_split(tba_ui_value("xreg_columns"), ",") : tba_dom_object(1);
    if (!driver)
        tba_dom_push(names, tba_ui_value("target_value_column"));
    for (unsigned c = 0; c < tba_dom_count(names); ++c) {
        int name = tba_dom_clean(tba_dom_item(names, c), 0);
        if (!tba_dom_truth(name))
            continue;
        int value = tba_ui_integer(tba_dom_key_get(index, name));
        if (!value || tba_dom_equal(name, date) || tba_dom_count(selected) >= 33)
            return 0;
        tba_dom_push(headers, name);
        tba_dom_push(selected, tba_dom_numeric(value - 1));
    }
    /* Metadata for an unselected second document only needs its dates. */
    for (unsigned r = 0; r < tba_dom_count(source); ++r) {
        unsigned scope = tba_dom_mark();
        int row = tba_dom_item(source, r), copy = tba_dom_object(1);
        for (unsigned c = 0; c < tba_dom_count(selected); ++c)
            tba_dom_push(copy, tba_dom_item(row, tba_ui_integer(tba_dom_item(selected, c))));
        tba_dom_push(rows, copy);
        tba_dom_release(scope);
    }
    int result = tba_dom_object(0);
    tba_dom_set(result, "headers", headers);
    tba_dom_set(result, "rows", rows);
    return result;
}
