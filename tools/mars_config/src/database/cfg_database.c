/**
 * @file cfg_database.c
 * @brief Shared plaintext or encrypted database installation, versioned migrations and bounded SQL services.
 *
 * Owns setting precedence, explicit storage-mode selection, streaming SQL imports, staging and
 * rollback-safe publication. Dispatches to the almanac and jurisdiction policies;
 * the combined installer preserves each database's individually configured mode.
 *
 * Fixed-size file-module reads preserve UTF-8 SQL in string_t. SQLCipher checks
 * statement completeness, including quotes, comments and trigger bodies. Input
 * lines are limited to 1 MiB, pending statements to 16 MiB and .read nesting to
 * 32 files; relative includes resolve against the repository. Complete literal
 * VALUES tuples, including INSERT OR IGNORE/REPLACE, use savepoint-protected
 * batches without changing enclosing transactions. Unsupported tuple syntax
 * falls back to ordinary parsing before batching, or fails once batching starts.
 * MARS_CONFIG_IMPORT_TRACE=1 reports timings using allowlisted source labels,
 * never arbitrary paths, SQL text or database diagnostics containing secrets.
 *
 * Database and configuration staging stay on their destination filesystems.
 * Each publication rename is atomic, but the pair is not a crash-atomic
 * transaction. A hard-link backup survives until configuration publication
 * succeeds; failed rollback retains it for recovery. Cleanup removes only known
 * installer-owned staging filenames.
 */
#define _POSIX_C_SOURCE 200809L
#include <sqlcipher/sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cfg_progress.h"
#include "cfg_prompt.h"
#include "cfg_storage.h"
#include "cfg_database.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

typedef struct cfg_database_scan {
    string_view_t view;
    size_t pos;
} cfg_database_scan;

/* Keep mode selection explicit, with no retry after an encrypted open fails. */
sqlite_t *cfg_database_open(const string_t *path, const string_t *key)
{
    return key ? (string_byte_length(key) ? sqlite_open_encrypted(path, key) : sqlite_open_plain(path)) : NULL;
}

/* Keep migration revisions explicit: changing an importer alone must not silently rewrite installed data. */
typedef struct cfg_database_migration {
    const char *name, *path_variable, *key_variable, *default_path;
    int revision;
    bool (*apply)(sqlite_t *, const string_t *, const string_t *);
} cfg_database_migration;

bool cfg_database_revision(sqlite_t *db, const char *name, int revision)
{
    bool ok = sqlite_exec_cstr(db,
        "create table if not exists mars_database_revision(name text primary key,revision integer not null)");
    sqlite_stmt_t *stmt = ok ? sqlite_stmt_prepare(db,
        "insert into mars_database_revision values(?1,?2) on conflict(name) do update set revision=excluded.revision") : NULL;
    ok = stmt && sqlite_stmt_bind_text(stmt, 1, name) && sqlite_stmt_bind_int64(stmt, 2, revision) &&
         sqlite_stmt_step(stmt) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(stmt);
    return ok;
}

/* Read the managed revision without creating metadata on legacy databases. */
bool cfg_database_current_revision(sqlite_t *db, const char *name, int64_t *revision)
{
    *revision = 0;
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db,
        "select 1 from sqlite_master where type='table' and name='mars_database_revision'");
    sqlite_step_result_t step = stmt ? sqlite_stmt_step(stmt) : SQLITE_STEP_ERROR;
    sqlite_stmt_finalize(stmt);
    if (step != SQLITE_STEP_ROW) return step == SQLITE_STEP_DONE;
    stmt = sqlite_stmt_prepare(db, "select revision from mars_database_revision where name=?1");
    bool ok = stmt && sqlite_stmt_bind_text(stmt, 1, name);
    step = ok ? sqlite_stmt_step(stmt) : SQLITE_STEP_ERROR;
    if (step == SQLITE_STEP_ROW) *revision = sqlite_stmt_column_int64(stmt, 0);
    sqlite_stmt_finalize(stmt);
    return (step == SQLITE_STEP_ROW || step == SQLITE_STEP_DONE) && *revision >= 0;
}

static bool cfg_database_jurisdiction_refresh(sqlite_t *db, const string_t *root, const string_t *staging)
{
    (void)staging;
    /* Unlike optional installation assets, a declared migration must not succeed without its source. */
    string_t *path = string_sprintf("%s/packaging/jurisdiction-db/flags/LICENSE", string_c_str(root));
    file_t *source = path ? file_new(path) : NULL;
    bool ok = source && file_exists(source) && cfg_database_flags(db, root);
    sqlite_stmt_t *emblem = ok ? sqlite_stmt_prepare(db,
        "select 1 from jurisdiction_flag_emblem e join jurisdiction_country_flag f on f.iso_country_code=e.code "
        "where e.code='ZZ-SEA' and length(f.svg)>0 "
        "and exists(select 1 from jurisdiction_country_flag_source where id=3)") : NULL;
    ok = ok && emblem && sqlite_stmt_step(emblem) == SQLITE_STEP_ROW;
    sqlite_stmt_finalize(emblem);
    file_free(source); string_free(path);
    return ok && cfg_database_source(db, root, "packaging/jurisdiction-db/location_additions.sql", false) &&
           cfg_database_source(db, root, "packaging/jurisdiction-db/location_subdivisions.sql", false);
}

static bool cfg_database_update_one(const cfg_database_migration *migration)
{
    string_t *home = cfg_storage_home();
    string_t *config_path = home ? string_sprintf("%s/config/%s-db.env", string_c_str(home), migration->name) : NULL;
    string_t *fallback = home ? string_sprintf("%s/%s", string_c_str(home), migration->default_path) : NULL;
    file_t *config = config_path ? file_new(config_path) : NULL;
    string_t *raw = config ? cfg_database_setting(config, NULL, migration->path_variable, fallback) : NULL;
    string_t *path = raw ? cfg_storage_expand_path(raw) : NULL;
    file_t *destination = path ? file_new(path) : NULL;
    bool exists = false;
    bool ok = destination && cfg_storage_safe_regular(destination, &exists);
    string_t *key = ok && exists ? cfg_database_setting(config, NULL, migration->key_variable, NULL) : NULL;
    sqlite_t *db = cfg_database_open(path, key);
    int64_t revision = 0;
    if (ok && exists)
        ok = db && sqlite_exec_cstr(db, "PRAGMA cache_size=-512; PRAGMA temp.cache_size=-512;") &&
             cfg_database_current_revision(db, migration->name, &revision) && revision <= migration->revision;
    if (ok && exists && revision < migration->revision) {
        const char *environment = getenv("MARS_ROOT");
        string_t *root = string_new_with(environment && *environment ? environment : MARS_CONFIG_ROOT_DIR);
        string_t *parent = cfg_storage_parent(path);
        string_t *staging = parent ? cfg_database_stage(parent) : NULL;
        bool transaction = root && staging && sqlite_exec_cstr(db, "begin immediate");
        /* Recheck under the writer lock so simultaneous releases cannot apply a migration twice. */
        ok = transaction && cfg_database_current_revision(db, migration->name, &revision) &&
             revision <= migration->revision;
        if (ok && revision < migration->revision) {
            printf("Updating %s database to managed-data revision %d...\n", migration->name, migration->revision);
            ok = migration->apply && migration->apply(db, root, staging) &&
                 cfg_database_revision(db, migration->name, migration->revision);
        }
        if (ok) ok = sqlite_exec_cstr(db, "commit");
        if (!ok && transaction) sqlite_exec_cstr(db, "rollback");
        cfg_database_cleanup(staging);
        string_free(staging); string_free(parent); string_free(root);
    }
    if (ok) printf("%s database: %s.\n", migration->name, exists ? "up to date" : "not installed; skipped");
    else fprintf(stderr, "%s database update failed; check saved credentials, source data and database availability.\n",
                 migration->name);
    sqlite_close(db); file_free(destination); file_free(config);
    string_free(key); string_free(path); string_free(raw); string_free(fallback); string_free(config_path); string_free(home);
    return ok;
}

/* Explicit migrations, not wholesale installer rebuilds; no settings or keys are rewritten. */
int cfg_databases_update(void)
{
    static const cfg_database_migration migrations[] = {
        {"globe", "MARS_GLOBE_DB_PATH", "MARS_GLOBE_DB_KEY", "globe/globe.db", 9, cfg_database_globe_refresh},
        {"almanac", "MARS_ALMANAC_DB_PATH", "MARS_ALMANAC_DB_KEY", "almanac/almanac.db", 0, NULL},
        {"jurisdiction", "MARS_JURISDICTION_DB_PATH", "MARS_JURISDICTION_DB_KEY",
         "jurisdiction/mars_jurisdiction_rules.db", 8, cfg_database_jurisdiction_refresh},
    };
    /* Three independent stores, in installation order; stop before later stores on failure. */
    for (size_t i = 0; i < sizeof(migrations) / sizeof(migrations[0]); ++i)
        if (!cfg_database_update_one(&migrations[i])) return 1;
    return 0;
}

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
    /* A standalone comment or blank line between tuples is valid SQL, even after
     * a batch has been flushed. Keep it in pending for SQLCipher's own lexer. */
    cfg_database_scan scan = {.view = string_view_all(line)};
    cfg_database_scan_spaces(&scan);
    if (scan.pos == string_view_length(scan.view) ||
        (cfg_database_scan_char(&scan, '-') && cfg_database_scan_char(&scan, '-'))) {
        *handled = true;
        return true;
    }
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

static const size_t cfg_database_line_limit = 1024 * 1024;
static const size_t cfg_database_statement_limit = 16 * 1024 * 1024;

typedef struct cfg_database_lexer {
    unsigned char quote;
    bool block_comment;
    bool line_comment;
    bool has_sql;
    bool terminator;
} cfg_database_lexer;

static bool cfg_database_sql_file(sqlite_t *db, const string_t *root, const string_t *path, unsigned depth,
                                  cfg_database_values *parent, cfg_progress_t *progress);

typedef struct _cfg_database_source_label_t {
    const char *path;
    const char *operation;
} cfg_database_source_label_t;

/* Sorted catalogue: binary lookup avoids a fragile hash tied to a fixed set of filenames. */
static const cfg_database_source_label_t *cfg_database_source_label(const string_t *path)
{
    static const cfg_database_source_label_t sources[] = {
        {"packaging/almanac-db/mars_almanac.sql", "Installing almanac tables"},
        {"packaging/almanac-db/mars_almanac_chebyshev.sql", "Installing almanac ephemeris coefficients"},
        {"packaging/almanac-db/mars_almanac_frame_rotation.sql", "Installing almanac frame rotations"},
        {"packaging/globe-db/jurisdiction_subdivisions.sql", "Identifying Kaliningrad Oblast on the globe"},
        {"packaging/jurisdiction-db/flag_emblems.sql", "Installing flag emblems"},
        {"packaging/jurisdiction-db/flag_regions.sql", "Installing regional flag definitions"},
        {"packaging/jurisdiction-db/location_additions.sql", "Adding South African island locations"},
        {"packaging/jurisdiction-db/location_subdivisions.sql", "Updating Kaliningrad towns and location defaults"},
        {"packaging/jurisdiction-db/mars_calendar_local.sql", "Installing local calendar settings"},
        {"packaging/jurisdiction-db/mars_calendar_locale_names.sql", "Installing translated calendar names"},
        {"packaging/jurisdiction-db/mars_country_jurisdictions.sql", "Installing country jurisdictions"},
        {"packaging/jurisdiction-db/mars_generated_first_class_rules.sql", "Installing generated calendar rules"},
        {"packaging/jurisdiction-db/mars_generated_holiday_instances.sql", "Installing generated holiday dates"},
        {"packaging/jurisdiction-db/mars_holiday_localized_names.sql", "Installing translated holiday names"},
        {"packaging/jurisdiction-db/mars_holiday_rules.sql", "Installing jurisdiction and holiday tables"},
        {"packaging/jurisdiction-db/mars_holiday_rules_queries.sql", "Running packaged holiday queries"},
        {"packaging/jurisdiction-db/mars_jurisdiction_location_defaults.sql", "Installing jurisdiction location defaults"},
        {"packaging/jurisdiction-db/mars_jurisdiction_towns.sql", "Installing town catalogue"},
        {"packaging/jurisdiction-db/mars_jurisdiction_world_towns.sql", "Installing worldwide town locations"},
        {"packaging/jurisdiction-db/mars_manual_first_class_rules.sql", "Installing maintained calendar rules"},
        {"packaging/jurisdiction-db/mars_manual_holiday_instance_overrides.sql", "Installing holiday date overrides"},
        {"packaging/jurisdiction-db/mars_target_subdivisions.sql", "Installing jurisdiction subdivisions"},
        {"packaging/jurisdiction-db/mars_timezone_rules.sql", "Installing time zone rules"},
    };
    if (!path)
        return NULL;
    size_t first = 0, end = sizeof sources / sizeof *sources;
    while (first < end) {
        size_t middle = first + (end - first) / 2;
        int order = strcmp(string_c_str(path), sources[middle].path);
        if (order < 0)
            end = middle;
        else if (order > 0)
            first = middle + 1;
        else
            return string_view_equals_literal(string_view_all(path), sources[middle].path) ? &sources[middle] : NULL;
    }
    return NULL;
}

/* Expose only repository-owned filenames in diagnostic output. */
const char *cfg_database_trace_label(const string_t *path)
{
    const cfg_database_source_label_t *source = cfg_database_source_label(path);
    return source ? source->path : "unlisted SQL source";
}

/* Describe the operation while keeping arbitrary caller paths out of the progress display. */
const char *cfg_database_progress_label(const string_t *path)
{
    const cfg_database_source_label_t *source = cfg_database_source_label(path);
    return source ? source->operation : "Importing custom SQL";
}

static double cfg_database_trace_seconds(clockid_t clock)
{
    struct timespec time;
    return clock_gettime(clock, &time) == 0 ? (double)time.tv_sec + (double)time.tv_nsec / 1e9 : -1;
}

static bool cfg_database_sql_space(unsigned char ch)
{
    return ch == ' ' || (ch >= '\t' && ch <= '\r');
}

static void cfg_database_lex_line(cfg_database_lexer *lexer, const string_t *line)
{
    string_view_t view = string_view_all(line);
    for (size_t i = 0; i < string_view_length(view); ++i) {
        unsigned char ch = 0, next = 0;
        bool ascii = string_view_peek_ascii(view, i, &ch);
        string_view_peek_ascii(view, i + 1, &next);
        if (lexer->line_comment) {
            if (ch == '\n')
                lexer->line_comment = false;
        } else if (lexer->block_comment) {
            if (ch == '*' && next == '/') {
                lexer->block_comment = false;
                ++i;
            }
        } else if (lexer->quote) {
            if (ch == lexer->quote) {
                if (lexer->quote != ']' && next == lexer->quote)
                    ++i;
                else
                    lexer->quote = 0;
            }
        } else if (ch == '-' && next == '-') {
            lexer->line_comment = true;
            ++i;
        } else if (ch == '/' && next == '*') {
            lexer->block_comment = true;
            ++i;
        } else if (!ascii || !cfg_database_sql_space(ch)) {
            lexer->has_sql = true;
            lexer->terminator = ch == ';';
            if (ch == '\'' || ch == '"' || ch == '`' || ch == '[')
                lexer->quote = ch == '[' ? ']' : ch;
        }
    }
}

static string_t *cfg_database_read_argument(const string_t *line)
{
    string_cursor_t *cursor = string_cursor_new(line);
    if (!cursor)
        return NULL;
    string_cursor_skip_spaces(cursor);
    bool ok = string_cursor_consume(cursor, ".read") &&
              (string_cursor_match(cursor, " ") || string_cursor_match(cursor, "\t"));
    string_cursor_skip_spaces(cursor);
    const char *quote = string_cursor_consume(cursor, "\"") ? "\"" : string_cursor_consume(cursor, "'") ? "'" : NULL;
    string_pos_t start = string_cursor_position(cursor);
    while (!string_cursor_done(cursor) &&
           !(quote ? string_cursor_match(cursor, quote)
                   : (string_cursor_match(cursor, " ") || string_cursor_match(cursor, "\t") ||
                      string_cursor_match(cursor, "\r") || string_cursor_match(cursor, "\n"))))
        string_cursor_next(cursor);
    string_t *argument = string_cursor_slice_between(start, string_cursor_position(cursor), cursor);
    if (quote)
        ok = string_cursor_consume(cursor, quote) && ok;
    string_cursor_skip_spaces(cursor);
    ok = ok && argument && string_byte_length(argument) && string_cursor_done(cursor);
    string_cursor_free(cursor);
    if (!ok) {
        string_free(argument);
        return NULL;
    }
    return argument;
}

static bool cfg_database_sql_line(sqlite_t *db, const string_t *root, string_t *pending, cfg_database_values *values,
                                  cfg_database_lexer *lexer, const char *bytes, size_t length, unsigned depth,
                                  cfg_progress_t *progress)
{
    string_t *line = string_new();
    bool ok = line && !string_append_utf8_exact(line, bytes, length);
    string_view_t view = string_view_all(line);
    unsigned char first = 0;
    size_t start = 0;
    while (string_view_peek_ascii(view, start, &first) && cfg_database_sql_space(first))
        ++start;
    bool directive = string_view_peek_ascii(view, start, &first) && first == '.';
    if (ok && directive && !values->prefix && !lexer->has_sql && !lexer->block_comment && !lexer->line_comment) {
        string_t *argument = cfg_database_read_argument(line);
        ok = argument && cfg_database_sql_file(db, root, argument, depth + 1, values, progress);
        string_free(argument);
        string_clear(pending);
    } else if (ok) {
        ok = length <= cfg_database_statement_limit - string_byte_length(pending) &&
             !string_append_utf8_exact(pending, bytes, length);
        bool handled = false;
        if (ok)
            ok = cfg_database_values_line(db, values, pending, line, &handled);
        if (ok && handled && !values->prefix)
            *lexer = (cfg_database_lexer){0};
        if (ok && !handled)
            cfg_database_lex_line(lexer, line);
        if (ok && !handled && lexer->terminator && !lexer->quote && !lexer->block_comment &&
            sqlite3_complete(string_c_str(pending))) {
            ok = cfg_database_execute(db, values, string_c_str(pending));
            string_clear(pending);
            *lexer = (cfg_database_lexer){0};
        } else if (ok && !handled && !lexer->quote && !lexer->block_comment && cfg_database_values_header(pending)) {
            values->prefix = string_clone(pending);
            ok = values->prefix != NULL;
        } else if (ok && !handled && !lexer->has_sql && !lexer->block_comment) {
            /* Discard complete comments between statements, never comments inside SQL. */
            string_clear(pending);
        }
    }
    string_free(line);
    return ok;
}

static bool cfg_database_sql_file(sqlite_t *db, const string_t *root, const string_t *path, unsigned depth,
                                  cfg_database_values *parent, cfg_progress_t *progress)
{
    if (depth >= 32)
        return false;
    const char *trace_option = getenv("MARS_CONFIG_IMPORT_TRACE");
    bool trace = trace_option && trace_option[0] == '1' && trace_option[1] == '\0';
    const char *label = cfg_database_trace_label(path);
    const char *progress_label = cfg_database_progress_label(path);
    double wall_start = trace ? cfg_database_trace_seconds(CLOCK_MONOTONIC) : 0;
    double cpu_start = trace ? cfg_database_trace_seconds(CLOCK_PROCESS_CPUTIME_ID) : 0;
    if (trace) {
        fprintf(stderr, "Import begin depth=%u source=%s\n", depth, label);
        fflush(stderr);
    }
    string_t *resolved = string_starts_with(path, "/")
                             ? string_clone(path)
                             : string_sprintf("%s/%s", string_c_str(root), string_c_str(path));
    file_t *file = resolved ? file_new(resolved) : NULL;
    file_info_t *info = file ? file_get_info(file) : NULL;
    uint64_t total = file_info_size(info);
    file_info_free(info);
    cfg_progress_update(progress, progress_label, 0, total);
    string_t *pending = string_new();
    cfg_database_values values = {.trace = trace};
    cfg_database_lexer lexer = {0};
    char *line = malloc(cfg_database_line_limit);
    bool ok = file && pending && line && file_open_read(file);
    char chunk[16384];
    size_t used = 0, count = 0, bytes_read = 0, line_number = 1;
    while (ok) {
        ok = file_read(file, chunk, sizeof(chunk), &count);
        if (!ok || !count)
            break;
        bytes_read += count;
        /* One bounded pass over stream bytes locates line endings without decoding SQL. */
        for (size_t i = 0; i < count && ok; ++i) {
            if (!chunk[i] || used == cfg_database_line_limit) {
                ok = false;
                break;
            }
            line[used++] = chunk[i];
            if (chunk[i] == '\n') {
                ok = cfg_database_sql_line(db, root, pending, &values, &lexer, line, used, depth, progress);
                if (ok)
                    ++line_number;
                used = 0;
            }
        }
        cfg_progress_update(progress, progress_label, bytes_read - used, total);
    }
    if (ok && used)
        ok = cfg_database_sql_line(db, root, pending, &values, &lexer, line, used, depth, progress);
    if (values.prefix)
        ok = false;
    /* SQLite accepts a final statement without a terminating semicolon. */
    if (ok && string_byte_length(pending))
        ok = cfg_database_execute(db, &values, string_c_str(pending));
    if (file && file_is_open(file))
        ok = file_close(file) && ok;
    if (!ok)
        fprintf(stderr, "SQL import failed: source=%s line=%zu (read, syntax or import-limit failure)\n",
                label, line_number);
    free(line);
    cfg_database_values_free(db, &values);
    string_free(pending);
    file_free(file);
    string_free(resolved);
    if (trace) {
        double wall_end = cfg_database_trace_seconds(CLOCK_MONOTONIC);
        double cpu_end = cfg_database_trace_seconds(CLOCK_PROCESS_CPUTIME_ID);
        double wall_elapsed = wall_start >= 0 && wall_end >= 0 ? wall_end - wall_start : -1;
        double cpu_elapsed = cpu_start >= 0 && cpu_end >= 0 ? cpu_end - cpu_start : -1;
        double parser_cpu = cpu_elapsed >= values.execution_cpu ? cpu_elapsed - values.execution_cpu : -1;
        fprintf(stderr,
                "Import %s depth=%u source=%s bytes=%zu wall=%.3fs cpu=%.3fs "
                "sql_cpu=%.3fs parser_io_cpu=%.3fs sql_calls=%zu (includes nested reads)\n",
                ok ? "done" : "failed", depth, label, bytes_read, wall_elapsed, cpu_elapsed, values.execution_cpu,
                parser_cpu, values.execution_calls);
        fflush(stderr);
    }
    if (parent) {
        parent->execution_cpu += values.execution_cpu;
        parent->execution_calls += values.execution_calls;
    }
    return ok;
}

/* Import without adding, splitting or committing the source's transactions. */
bool cfg_database_import(sqlite_t *db, const string_t *root, const string_t *path)
{
    if (!db || !root || !path)
        return false;
    cfg_progress_t *progress = cfg_progress_start("Importing packaged SQL");
    bool ok = cfg_database_sql_file(db, root, path, 0, NULL, progress);
    cfg_progress_finish(progress, ok);
    return ok;
}

static file_t *cfg_database_stage_file(const string_t *directory, const char *name)
{
    string_t *path = string_sprintf("%s/%s", string_c_str(directory), name);
    file_t *file = path ? file_new(path) : NULL;
    string_free(path);
    return file;
}

/* mkdtemp provides exclusive directory creation; all content I/O uses the file module. */
string_t *cfg_database_stage(const string_t *directory)
{
    string_t *pattern = string_sprintf("%s/.mars-database-XXXXXX", string_c_str(directory));
    char *buffer = pattern ? strdup(string_c_str(pattern)) : NULL;
    bool created = buffer && mkdtemp(buffer);
    string_t *result = created ? string_new_with(buffer) : NULL;
    if (created && !result) {
        file_t *folder = file_new_cstr(buffer);
        if (folder)
            file_remove_directory(folder);
        file_free(folder);
    }
    free(buffer);
    string_free(pattern);
    return result;
}

/* Do not delete backup.db: its presence means rollback needs manual recovery. */
void cfg_database_cleanup(const string_t *staging)
{
    if (!staging)
        return;
    static const char *const names[] = {"database.db", "database.db-journal", "database.db-wal", "database.db-shm",
                                        "configuration.env", "shoreline.bin", "shoreline.zst", "shoreline-corrected.bin", "rivers.bin"};
    for (size_t i = 0; i < sizeof(names) / sizeof(*names); ++i) {
        file_t *file = cfg_database_stage_file(staging, names[i]);
        if (file)
            file_delete(file);
        file_free(file);
    }
    file_t *folder = file_new(staging);
    if (folder)
        file_remove_directory(folder);
    file_free(folder);
}

static bool cfg_database_no_sidecars(file_t *destination)
{
    static const char *const suffixes[] = {"-journal", "-wal", "-shm"};
    bool ok = true;
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(*suffixes) && ok; ++i) {
        string_t *path = string_sprintf("%s%s", file_path(destination), suffixes[i]);
        file_t *sidecar = path ? file_new(path) : NULL;
        bool exists = false;
        ok = sidecar && cfg_storage_safe_regular(sidecar, &exists) && !exists;
        file_free(sidecar);
        string_free(path);
    }
    return ok;
}

static string_t *cfg_database_canonical_destination(file_t *file)
{
    string_t *path = string_new_with(file_path(file));
    string_t *parent = path ? cfg_storage_parent(path) : NULL;
    file_t *folder = parent ? file_new(parent) : NULL;
    file_t *resolved = folder ? file_resolve(folder) : NULL;
    string_cursor_t *cursor = path ? string_cursor_new(path) : NULL;
    string_pos_t start = 0;
    while (cursor && !string_cursor_done(cursor)) {
        if (string_cursor_consume(cursor, "/"))
            start = string_cursor_position(cursor);
        else
            string_cursor_next(cursor);
    }
    string_t *leaf = cursor ? string_cursor_slice_between(start, string_cursor_position(cursor), cursor) : NULL;
    string_t *result = resolved && leaf ? string_sprintf("%s/%s", file_path(resolved), string_c_str(leaf)) : NULL;
    string_free(leaf);
    string_cursor_free(cursor);
    file_free(resolved);
    file_free(folder);
    string_free(parent);
    string_free(path);
    return result;
}

/* Complete both outputs before replacing either destination; recover on rename failure. */
bool cfg_database_publish(file_t *database, file_t *destination, file_t *configuration,
                          const string_t *configuration_directory, const string_t *staging, const string_t *body)
{
    string_t *config_stage = cfg_database_stage(configuration_directory);
    file_t *config_file = config_stage ? cfg_database_stage_file(config_stage, "configuration.env") : NULL;
    file_t *backup = cfg_database_stage_file(staging, "backup.db");
    string_t *database_path = cfg_database_canonical_destination(destination);
    string_t *configuration_path = cfg_database_canonical_destination(configuration);
    bool existed = false, configuration_existed = false;
    bool ok = config_file && backup && database_path && configuration_path &&
              string_compare(database_path, configuration_path) && cfg_storage_safe_regular(destination, &existed) &&
              cfg_storage_safe_regular(configuration, &configuration_existed) &&
              cfg_database_no_sidecars(destination) && cfg_storage_publish(config_file, config_stage, body) &&
              file_chmod(database, 0600) && file_open_read(database) && file_sync(database, false);
    if (database && file_is_open(database))
        ok = file_close(database) && ok;
    bool backed_up = ok && existed && file_create_hard_link(destination, backup);
    ok = ok && (!existed || backed_up);
    bool replaced = ok && file_move(database, destination, true);
    ok = replaced && file_move(config_file, configuration, true);
    bool recovered = true;
    if (replaced && !ok)
        recovered = existed ? file_move(backup, destination, true) : file_delete(destination);
    if (backed_up && recovered)
        file_delete(backup);
    if (!recovered)
        fputs("Database rollback failed; the previous database is retained in its private staging backup.\n", stderr);
    cfg_database_cleanup(config_stage);
    file_free(backup);
    file_free(config_file);
    string_free(database_path);
    string_free(configuration_path);
    string_free(config_stage);
    return ok;
}

static void cfg_database_options_free(cfg_database_options *options)
{
    string_free(options->home);
    string_free(options->configuration_directory);
    string_free(options->configuration_path);
    file_free(options->configuration);
    string_free(options->path);
    string_free(options->expanded_path);
    string_free(options->key);
    string_free(options->requested_location);
    string_free(options->saved_location);
    string_free(options->requested_language);
    string_free(options->saved_language);
}

string_t *cfg_database_setting(file_t *configuration, const char *explicit_value, const char *name,
                               const string_t *fallback)
{
    string_t *saved = cfg_storage_setting(configuration, name);
    string_t *value = cfg_storage_value(explicit_value);
    if (value && !explicit_value) {
        string_free(value);
        value = cfg_storage_value(getenv(name));
    }
    if (value && !explicit_value && !getenv(name)) {
        string_free(value);
        value = saved ? string_clone(string_byte_length(saved) || !fallback ? saved : fallback) : NULL;
    }
    if (!saved) {
        string_free(value);
        value = NULL;
    }
    string_free(saved);
    return value;
}

static bool cfg_database_options_read(cfg_database_options *options, const cfg_database_profile *profile, const char *path,
                                      const char *key, const char *location, const char *language)
{
    options->home = cfg_storage_home();
    if (!options->home)
        return false;
    options->configuration_directory = string_sprintf("%s/config", string_c_str(options->home));
    options->configuration_path =
        string_sprintf("%s/config/%s-db.env", string_c_str(options->home), profile->name);
    options->configuration = options->configuration_path ? file_new(options->configuration_path) : NULL;
    string_t *default_path = string_sprintf("%s/%s", string_c_str(options->home), profile->default_path);
    if (!options->configuration_directory || !options->configuration || !default_path) {
        string_free(default_path);
        return false;
    }
    options->path = cfg_database_setting(options->configuration, path, profile->path_variable, default_path);
    options->key = cfg_database_setting(options->configuration, key, profile->key_variable, NULL);
    options->expanded_path = options->path ? cfg_storage_expand_path(options->path) : NULL;
    bool ok = options->path && options->key && options->expanded_path &&
              profile->read_options(options, default_path, location, language);
    string_free(default_path);
    return ok;
}

bool cfg_database_export(string_t *body, const char *name, const string_t *value)
{
    string_t *quoted = cfg_storage_quote(value);
    bool ok = quoted && string_append_format(body, "export %s=", name) >= 0 &&
              !string_append_utf8_exact(body, string_c_str(quoted), string_byte_length(quoted)) &&
              !string_append_char(body, '\n');
    string_free(quoted);
    return ok;
}

static string_t *cfg_database_configuration(const cfg_database_options *options, const cfg_database_profile *profile,
                                            const string_t *location, const string_t *language)
{
    string_t *body = string_sprintf("# Generated by mars_config %s\n", profile->name);
    bool ok = body && cfg_database_export(body, profile->path_variable, options->path) &&
              cfg_database_export(body, profile->key_variable, options->key);
    if (ok && profile->export_settings)
        ok = profile->export_settings(body, location, language);
    if (!ok) {
        string_free(body);
        body = NULL;
    }
    return body;
}

bool cfg_database_source(sqlite_t *db, const string_t *root, const char *name, bool optional)
{
    string_t *path = string_new_with(name);
    string_t *full = string_sprintf("%s/%s", string_c_str(root), name);
    file_t *file = full ? file_new(full) : NULL;
    bool exists = file && file_exists(file);
    bool ok = path && file &&
              ((optional && !exists && !file_last_error(file)) || (exists && cfg_database_import(db, root, path)));
    if (!ok) {
        const string_t *error = sqlite_last_error(db);
        fprintf(stderr, "Database source %s failed: %s\n", name,
                error && string_byte_length(error) ? string_c_str(error) : "see SQL import diagnostic above");
    }
    file_free(file);
    string_free(full);
    string_free(path);
    return ok;
}

static bool cfg_database_build(cfg_database_options *options, const cfg_database_profile *profile, bool interactive)
{
    string_t *parent = cfg_storage_parent(options->expanded_path);
    bool ok = parent && cfg_storage_private_directory(options->home, true) &&
              cfg_storage_private_directory(parent, true) &&
              cfg_storage_private_directory(options->configuration_directory, true);
    string_t *staging = ok ? cfg_database_stage(parent) : NULL;
    string_t *temporary_path = staging ? string_sprintf("%s/database.db", string_c_str(staging)) : NULL;
    file_t *temporary = temporary_path ? file_new(temporary_path) : NULL;
    file_t *destination = file_new(options->expanded_path);
    string_t *root = cfg_storage_value(getenv("MARS_ROOT"));
    if (root && !string_byte_length(root)) {
        string_free(root);
        root = string_new_with(MARS_CONFIG_ROOT_DIR);
    }
    cfg_progress_t *progress = cfg_progress_start("Preparing database");
    sqlite_t *db = temporary && root ? cfg_database_open(temporary_path, options->key) : NULL;
    /* Bound both working caches as catalogues grow; their pages are locked by SQLCipher. */
    ok = ok && db && destination &&
         sqlite_exec_cstr(db, "PRAGMA foreign_keys = ON; PRAGMA cache_size=-512; PRAGMA temp.cache_size=-512;");
    cfg_progress_finish(progress, ok);
    if (ok) {
        fprintf(stderr, "Building %s %s database...\n",
                string_byte_length(options->key) ? "encrypted" : "plaintext", profile->name);
        ok = profile->import_sources(db, root, staging);
    }
    /* Commit for calendar's separate reader, then release the import cache so its locked pages do not
     * compete with calendar generation. Keep SQLCipher memory security enabled throughout. */
    if (ok)
        ok = sqlite_exec_cstr(db, "BEGIN; COMMIT; PRAGMA shrink_memory;");
    string_t *chosen_location = NULL, *chosen_language = NULL;
    if (ok && profile->populate)
        ok = profile->populate(db, temporary_path, options, interactive, &chosen_location, &chosen_language);
    /* An unfinished source transaction must fail instead of being rolled back silently on close. */
    progress = ok ? cfg_progress_start("Finalising and publishing database") : NULL;
    if (ok)
        ok = sqlite_exec_cstr(db, "BEGIN; COMMIT; PRAGMA wal_checkpoint(TRUNCATE); PRAGMA journal_mode=DELETE;");
    sqlite_close(db);
    string_t *body = ok ? cfg_database_configuration(options, profile, chosen_location, chosen_language) : NULL;
    ok = ok && body &&
         cfg_database_publish(temporary, destination, options->configuration, options->configuration_directory, staging,
                              body);
    cfg_progress_finish(progress, ok);
    if (ok) {
        printf("Built %s database at %s\nWrote %s database configuration to %s\n",
               profile->name, string_c_str(options->path),
               profile->name, string_c_str(options->configuration_path));
        if (profile->populate)
            printf("calendar_local language: %s\ncalendar_local location: %s\n", string_c_str(chosen_language),
                   string_c_str(chosen_location));
    }
    string_free(body);
    string_free(chosen_location);
    string_free(chosen_language);
    file_free(destination);
    file_free(temporary);
    string_free(temporary_path);
    cfg_database_cleanup(staging);
    string_free(staging);
    string_free(parent);
    string_free(root);
    return ok;
}

/* Configure only the selected private database, using literal settings and native SQLCipher. */
static int cfg_database_install(const cfg_database_profile *profile, const char *path, const char *key, const char *location,
                                const char *language, bool interactive)
{
    cfg_database_options options = {0};
    bool ok = cfg_database_options_read(&options, profile, path, key, location, language);
    interactive = interactive && isatty(STDIN_FILENO);
    string_t *quoted_key = ok ? cfg_storage_quote(options.key) : NULL;
    string_t *quoted_path = ok ? cfg_storage_quote(options.path) : NULL;
    ok = ok && quoted_key && quoted_path && string_byte_length(options.path);
    string_free(quoted_key);
    string_free(quoted_path);
    if (ok)
        ok = cfg_database_build(&options, profile, interactive);
    if (!ok)
        fprintf(stderr, "Failed to configure the %s database; check settings, private storage and packaged SQL.\n",
                profile->name);
    cfg_database_options_free(&options);
    return ok ? 0 : 1;
}

/* Use saved storage mode and preserve interactive calendar prompting. */
int cfg_database_run(bool jurisdiction, const char *path, const char *key, const char *location, const char *language,
                     bool interactive)
{
    const cfg_database_profile *profile = jurisdiction ? &cfg_database_jurisdiction : &cfg_database_almanac;
    return cfg_database_install(profile, path, key, location, language, interactive);
}

/* Install each database in its saved mode; fresh configurations default to plaintext. */
int cfg_databases_run(void)
{
    bool interactive = isatty(STDIN_FILENO);
    bool ok = cfg_globe_run("globe-install") == 0;
    if (ok)
        ok = cfg_database_install(&cfg_database_almanac, NULL, NULL, NULL, NULL, interactive) == 0;
    if (ok)
        ok = cfg_database_install(&cfg_database_jurisdiction, NULL, NULL, NULL, NULL, interactive) == 0;
    if (!ok)
        fputs("Combined database installation stopped; later databases were not changed.\n", stderr);
    return ok ? 0 : 1;
}
