/**
 * @file cfg_database_values.c
 * @brief Bounded batching of the packaged coefficient INSERT statements.
 *
 * The Chebyshev source has a single 100 MiB literal VALUES insert. Ordinary
 * unquoted INSERT headers, including OR IGNORE/REPLACE, and complete literal tuples are recognised structurally,
 * without naming a particular table. Batches execute inside one savepoint so
 * enclosing source transactions remain intact. Small or non-literal statements
 * fall back to SQLCipher's ordinary parser before any batch has been executed.
 * Once batching begins, unsupported tuple syntax fails the staging import.
 */
#define _POSIX_C_SOURCE 200809L
#include <time.h>

#include "cfg_database_internal.h"

typedef struct cfg_database_scan {
    string_view_t view;
    size_t pos;
} cfg_database_scan;

static bool cfg_database_space(unsigned char ch)
{
    return ch == ' ' || (ch >= '\t' && ch <= '\r');
}

static bool cfg_database_scan_spaces(cfg_database_scan *scan)
{
    size_t start = scan->pos;
    unsigned char ch;
    while (string_view_peek_ascii(scan->view, scan->pos, &ch) && cfg_database_space(ch))
        ++scan->pos;
    return scan->pos != start;
}

static bool cfg_database_scan_char(cfg_database_scan *scan, unsigned char expected)
{
    unsigned char ch;
    if (!string_view_peek_ascii(scan->view, scan->pos, &ch) || ch != expected)
        return false;
    ++scan->pos;
    return true;
}

static bool cfg_database_keyword(cfg_database_scan *scan, const char *word)
{
    size_t pos = scan->pos;
    for (size_t i = 0; word[i]; ++i, ++pos) {
        unsigned char ch;
        if (!string_view_peek_ascii(scan->view, pos, &ch))
            return false;
        if (ch >= 'A' && ch <= 'Z')
            ch += 'a' - 'A';
        if (ch != (unsigned char)word[i])
            return false;
    }
    scan->pos = pos;
    return true;
}

static bool cfg_database_identifier(cfg_database_scan *scan)
{
    unsigned char ch = 0;
    if (!string_view_peek_ascii(scan->view, scan->pos, &ch) ||
        !((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_'))
        return false;
    do {
        ++scan->pos;
    } while (string_view_peek_ascii(scan->view, scan->pos, &ch) &&
             ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_'));
    return true;
}

/* Inspect only header tokens and the trailing keyword, never lowercase or copy accumulated SQL. */
bool cfg_database_values_header(const string_t *sql)
{
    cfg_database_scan scan = {.view = string_view_all(sql)};
    size_t end = string_view_length(scan.view);
    unsigned char ch;
    while (end && string_view_peek_ascii(scan.view, end - 1, &ch) && cfg_database_space(ch))
        --end;
    cfg_database_scan tail = {.view = scan.view, .pos = end >= 6 ? end - 6 : 0};
    if (end < 6 || !cfg_database_keyword(&tail, "values"))
        return false;
    cfg_database_scan_spaces(&scan);
    if (!cfg_database_keyword(&scan, "insert") || !cfg_database_scan_spaces(&scan))
        return false;
    if (cfg_database_keyword(&scan, "or")) {
        if (!cfg_database_scan_spaces(&scan) ||
            !(cfg_database_keyword(&scan, "ignore") || cfg_database_keyword(&scan, "replace")) ||
            !cfg_database_scan_spaces(&scan))
            return false;
    }
    if (!cfg_database_keyword(&scan, "into") || !cfg_database_scan_spaces(&scan) || !cfg_database_identifier(&scan))
        return false;
    bool separated = cfg_database_scan_spaces(&scan);
    if (cfg_database_scan_char(&scan, '(')) {
        do {
            cfg_database_scan_spaces(&scan);
            if (!cfg_database_identifier(&scan))
                return false;
            cfg_database_scan_spaces(&scan);
        } while (cfg_database_scan_char(&scan, ','));
        if (!cfg_database_scan_char(&scan, ')'))
            return false;
        cfg_database_scan_spaces(&scan);
    } else if (!separated)
        return false;
    return cfg_database_keyword(&scan, "values") && scan.pos == end;
}

static bool cfg_database_quoted_literal(cfg_database_scan *scan)
{
    if (!cfg_database_scan_char(scan, '\''))
        return false;
    while (scan->pos < string_view_length(scan->view)) {
        if (cfg_database_scan_char(scan, '\'')) {
            if (cfg_database_scan_char(scan, '\''))
                continue;
            return true;
        }
        /* SQL delimiters are ASCII bytes; UTF-8 text is copied verbatim without grapheme work. */
        ++scan->pos;
    }
    return false;
}

static bool cfg_database_digits(cfg_database_scan *scan)
{
    size_t start = scan->pos;
    unsigned char ch;
    while (string_view_peek_ascii(scan->view, scan->pos, &ch) && ch >= '0' && ch <= '9')
        ++scan->pos;
    return scan->pos != start;
}

static bool cfg_database_literal(cfg_database_scan *scan)
{
    unsigned char ch;
    if (string_view_peek_ascii(scan->view, scan->pos, &ch) && ch == '\'')
        return cfg_database_quoted_literal(scan);
    if (cfg_database_scan_char(scan, 'X') || cfg_database_scan_char(scan, 'x'))
        return cfg_database_quoted_literal(scan);
    if (cfg_database_keyword(scan, "null"))
        return true;
    if (!cfg_database_scan_char(scan, '+'))
        cfg_database_scan_char(scan, '-');
    bool digits = cfg_database_digits(scan);
    if (cfg_database_scan_char(scan, '.'))
        digits = cfg_database_digits(scan) || digits;
    if (!digits)
        return false;
    if (cfg_database_scan_char(scan, 'e') || cfg_database_scan_char(scan, 'E')) {
        if (!cfg_database_scan_char(scan, '+'))
            cfg_database_scan_char(scan, '-');
        if (!cfg_database_digits(scan))
            return false;
    }
    /* SQLCipher remains responsible for numeric range and blob-hex validation. */
    return true;
}

static bool cfg_database_tuple(const string_t *line, bool *last)
{
    cfg_database_scan scan = {.view = string_view_all(line)};
    cfg_database_scan_spaces(&scan);
    bool ok = cfg_database_scan_char(&scan, '(');
    while (ok) {
        cfg_database_scan_spaces(&scan);
        ok = cfg_database_literal(&scan);
        cfg_database_scan_spaces(&scan);
        if (!ok || !cfg_database_scan_char(&scan, ','))
            break;
    }
    ok = ok && cfg_database_scan_char(&scan, ')');
    cfg_database_scan_spaces(&scan);
    *last = cfg_database_scan_char(&scan, ';');
    ok = ok && (*last || cfg_database_scan_char(&scan, ','));
    cfg_database_scan_spaces(&scan);
    ok = ok && scan.pos == string_view_length(scan.view);
    return ok;
}

/* Time only native execution, leaving parser and file work in the trace's residual CPU count. */
bool cfg_database_execute(sqlite_t *db, cfg_database_values *values, const char *sql)
{
    struct timespec start, end;
    bool timed = values->trace && clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start) == 0;
    bool ok = sqlite_exec_cstr(db, sql);
    if (values->trace)
        ++values->execution_calls;
    if (timed && clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end) == 0)
        values->execution_cpu += (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1e9;
    return ok;
}

/* Split only complete literal tuples and preserve one atomic INSERT savepoint. */
bool cfg_database_values_line(sqlite_t *db, cfg_database_values *values, string_t *pending, const string_t *line,
                              bool *handled)
{
    *handled = false;
    if (!values->prefix)
        return true;
    bool last = false;
    if (!cfg_database_tuple(line, &last)) {
        if (values->transaction)
            return false;
        string_free(values->prefix);
        values->prefix = NULL;
        return true;
    }
    *handled = true;
    if (!last && string_byte_length(pending) < 65536)
        return true;
    if (!values->transaction) {
        if (!cfg_database_execute(db, values, "SAVEPOINT mars_config_literal_insert;"))
            return false;
        values->transaction = true;
    }
    string_view_t view = string_view_all(pending);
    size_t end = string_view_length(view);
    unsigned char ch;
    while (end && string_view_peek_ascii(view, end - 1, &ch) && cfg_database_space(ch))
        --end;
    string_view_t body_view = string_view_slice(view, 0, end - 1);
    string_t *batch = string_from_view(&body_view);
    bool ok = batch && !string_append_char(batch, ';') && cfg_database_execute(db, values, string_c_str(batch));
    string_free(batch);
    if (!ok)
        return false;
    string_clear(pending);
    if (last) {
        if (!cfg_database_execute(db, values, "RELEASE mars_config_literal_insert;"))
            return false;
        values->transaction = false;
        string_free(values->prefix);
        values->prefix = NULL;
        return true;
    }
    return !string_append_utf8_exact(pending, string_c_str(values->prefix), string_byte_length(values->prefix));
}

/* Release or roll back only the importer-owned savepoint, never an enclosing transaction. */
void cfg_database_values_free(sqlite_t *db, cfg_database_values *values)
{
    if (values->transaction)
        cfg_database_execute(db, values, "ROLLBACK TO mars_config_literal_insert; RELEASE mars_config_literal_insert;");
    string_free(values->prefix);
    values->prefix = NULL;
    values->transaction = false;
}
