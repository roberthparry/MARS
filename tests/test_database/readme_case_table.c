/**
 * @file readme_case_table.c
 * @brief Runnable README example: add a mathematical case row to a disposable in-memory table.
 *
 * Kept byte-for-byte in docs/testing.md. The README checker compiles that documented programme and compares
 * its output. This example uses a minimal case-table fixture and never opens the shared test catalogue.
 */
#include <stdio.h>
#include <stdlib.h>

#include "sqlite-t.h"
#include "string-t.h"

/* Add and inspect one isolated case without changing shared test definitions. */
int main(void)
{
    int status = EXIT_FAILURE;
    string_t *path = string_new_with(":memory:");
    sqlite_t *database = path ? sqlite_open_plain(path) : NULL;
    string_free(path);
    sqlite_stmt_t *query = NULL;
    if (!database || !sqlite_exec_cstr(database,
        "CREATE TABLE expression_cases (\n"
        "    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,\n"
        "    precision INTEGER NOT NULL DEFAULT 32, enabled INTEGER NOT NULL DEFAULT 1\n"
        ") WITHOUT ROWID;\n"
        "INSERT INTO expression_cases(name, operation, input, expected)\n"
        "VALUES ('another_sum', 'evaluate', '1/2+1/3', '5/6');"))
        goto done;
    query = sqlite_stmt_prepare(database,
        "SELECT name, expected FROM expression_cases WHERE name = 'another_sum'");
    if (query && sqlite_stmt_step(query) == SQLITE_STEP_ROW &&
        printf("%s|%s\n", sqlite_stmt_column_text(query, 0), sqlite_stmt_column_text(query, 1)) >= 0 &&
        sqlite_stmt_step(query) == SQLITE_STEP_DONE)
        status = EXIT_SUCCESS;
done:
    sqlite_stmt_finalize(query);
    sqlite_close(database);
    if (status != EXIT_SUCCESS)
        fputs("Could not add or read the example case.\n", stderr);
    return status;
}
