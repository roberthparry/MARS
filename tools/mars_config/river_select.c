/**
 * @file river_select.c
 * @brief Optional offline preparation of a compact main-river candidate.
 *
 * Accepts explicit source and new output locations for public, plaintext geographic data. No installed database
 * or observer state is opened or changed. Existing output files and missing input files are rejected.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file-t.h"
#include "sqlite-t.h"
#include "string-t.h"
#include "cfg_storage.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

/* Run a bounded native selection without invoking a shell or a full world re-extraction. */
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--help")) {
        puts("Usage: river_select INPUT.db NEW-OUTPUT.db");
        return EXIT_SUCCESS;
    }
    if (argc != 3) return EXIT_FAILURE;
    string_t *input = string_new_with(argv[1]), *output = string_new_with(argv[2]);
    file_t *file = input ? file_new(input) : NULL;
    bool exists = false;
    sqlite_t *source = file && cfg_storage_safe_regular(file, &exists) && exists ? sqlite_open_plain(input) : NULL;
    bool ok = source && sqlite_exec_cstr(source, "pragma cache_size=-8192") &&
              cfg_database_select_main_rivers(source, output);
    sqlite_close(source); file_free(file); string_free(input); string_free(output);
    puts(ok ? "Main-river candidate ready; installed globe unchanged." : "River selection failed.");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
