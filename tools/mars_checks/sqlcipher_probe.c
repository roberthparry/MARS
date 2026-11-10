/**
 * @file sqlcipher_probe.c
 * @brief Check the linked SQLCipher version and SQL features before building or updating databases.
 *
 * This standalone bootstrap probe links directly to the same SQLCipher flags as MARS; it cannot depend on the
 * library whose prerequisites it checks. It reports the runtime cipher and SQLite versions, checks the SQLite
 * headers, and exercises FTS5 with the packaged Unicode tokenizer and window functions in an in-memory database.
 * Unsupported releases and missing features print copyable Debian/Ubuntu source-build instructions.
 * No installed database, settings, credentials or command-line SQLCipher programme is required or accessed.
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <sqlcipher/sqlite3.h>

static void probe_install_instructions(void)
{
    fputs("\nDebian/Ubuntu: close MARS Lab, then run these commands on the affected computer.\n"
          "They install SQLCipher 4.13.0 with FTS5 under /usr/local and keep the packaged library.\n\n"
          "sudo apt update\n"
          "sudo apt install build-essential git pkg-config libssl-dev tcl-dev\n"
          "(\n"
          "    set -eu\n"
          "    sqlcipher_build=$(mktemp -d)\n"
          "    trap 'rm -rf \"$sqlcipher_build\"' EXIT\n"
          "    git clone --depth 1 --branch v4.13.0 \\\n"
          "        https://github.com/sqlcipher/sqlcipher.git \"$sqlcipher_build/source\"\n"
          "    cd \"$sqlcipher_build/source\"\n"
          "    ./configure --prefix=/usr/local --libdir=/usr/local/lib \\\n"
          "        --includedir=/usr/local/include/sqlcipher --disable-tcl \\\n"
          "        --with-tempstore=yes --enable-fts5 --enable-rtree \\\n"
          "        CFLAGS=\"-O2 -DSQLITE_HAS_CODEC -DSQLITE_EXTRA_INIT=sqlcipher_extra_init "
          "-DSQLITE_EXTRA_SHUTDOWN=sqlcipher_extra_shutdown\" \\\n"
          "        LDFLAGS=\"-lcrypto\"\n"
          "    make -j2 libsqlite3.DLL.basename=libsqlcipher \\\n"
          "        LDFLAGS.libsqlite3.soname=-Wl,-soname,libsqlcipher.so.0 so\n"
          "    sudo make -j1 libsqlite3.DLL.basename=libsqlcipher \\\n"
          "        LDFLAGS.libsqlite3.soname=-Wl,-soname,libsqlcipher.so.0 install-dll install-headers\n"
          "    sudo install -d /usr/local/lib/pkgconfig\n"
          "    sed -e 's/^Name: SQLite$/Name: SQLCipher/' \\\n"
          "        -e 's/-lsqlite3/-lsqlcipher/g' sqlite3.pc \\\n"
          "        | sudo tee /usr/local/lib/pkgconfig/sqlcipher.pc >/dev/null\n"
          "    sudo ldconfig\n"
          ")\n"
          "export PKG_CONFIG_PATH=\"/usr/local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}\"\n"
          "make check-native-deps\n\n"
          "Run make check-native-deps from the MARS checkout; after it passes, rerun make release.\n"
          "Other systems: build the same SQLCipher release with OpenSSL, FTS5 and window functions.\n"
          "Full instructions: docs/building.md#updating-sqlcipher-on-debian-or-ubuntu\n", stderr);
}

static bool probe_version(sqlite3 *db, bool quiet)
{
    bool runtime = sqlite3_libversion_number() >= 3027000;
    bool headers = SQLITE_VERSION_NUMBER >= 3027000;
    sqlite3_stmt *query = NULL;
    bool found = sqlite3_prepare_v2(db, "PRAGMA cipher_version", -1, &query, NULL) == SQLITE_OK &&
                 sqlite3_step(query) == SQLITE_ROW && sqlite3_column_text(query, 0) &&
                 *sqlite3_column_text(query, 0);
    bool cipher = false;
    if (found) {
        const char *release = (const char *)sqlite3_column_text(query, 0);
        unsigned major = 0, minor = 0;
        cipher = sscanf(release, "%u.%u", &major, &minor) == 2 && (major > 4 || (major == 4 && minor >= 1));
        if (!quiet || !cipher || !runtime || !headers) {
            fprintf(stderr, "SQLCipher %s; SQLite runtime %s; SQLite headers %s.\n",
                   release, sqlite3_libversion(), SQLITE_VERSION);
            fflush(stderr);
        }
        if (!cipher)
            fprintf(stderr, "SQLCipher %s is unsupported; MARS requires SQLCipher 4.1.0 or newer.\n", release);
    } else
        fputs("The linked library does not report a SQLCipher version.\n", stderr);
    sqlite3_finalize(query);
    if (!runtime)
        fprintf(stderr, "SQLCipher embeds SQLite %s; MARS requires SQLite 3.27.0 or newer.\n", sqlite3_libversion());
    if (!headers)
        fprintf(stderr, "SQLCipher headers use SQLite %s; MARS requires SQLite 3.27.0 or newer.\n", SQLITE_VERSION);
    return cipher && runtime && headers;
}

static bool probe_features(sqlite3 *db)
{
    static const struct {
        const char *name;
        const char *sql;
    } features[] = {
        {"FTS5 with Unicode accent removal", "CREATE VIRTUAL TABLE temp.mars_fts5_probe USING "
            "fts5(name,tokenize='unicode61 remove_diacritics 2');"
            "INSERT INTO mars_fts5_probe VALUES('Lüderitz');"
            "SELECT name FROM mars_fts5_probe WHERE mars_fts5_probe MATCH '\"Luder\"*' ORDER BY rank;"},
        {"SQLite window functions", "SELECT row_number() OVER ()"}
    };
    bool ok = true;
    /* Execute every independent requirement, so one failure cannot conceal another missing feature. */
    for (size_t i = 0; i < sizeof features / sizeof *features; ++i) {
        char *error = NULL;
        if (sqlite3_exec(db, features[i].sql, NULL, NULL, &error) != SQLITE_OK) {
            fprintf(stderr, "SQLCipher lacks required %s: %s.\n", features[i].name,
                    error ? error : sqlite3_errmsg(db));
            ok = false;
        }
        sqlite3_free(error);
    }
    return ok;
}

/* Validate the selected runtime without opening a user database or changing the installed library. */
int main(int argc, char **argv)
{
    bool quiet = argc == 2 && !strcmp(argv[1], "--quiet");
    if (argc != 1 && !quiet) {
        fputs("Usage: sqlcipher_probe [--quiet]\n", stderr);
        return 2;
    }
    sqlite3 *db = NULL;
    if (sqlite3_open(":memory:", &db) != SQLITE_OK) {
        fputs("Cannot open SQLCipher's in-memory dependency probe.\n", stderr);
        sqlite3_close(db);
        return 1;
    }
    bool version = probe_version(db, quiet);
    bool features = probe_features(db);
    if (quiet && version && !features) (void)probe_version(db, false);
    bool closed = sqlite3_close(db) == SQLITE_OK;
    if (!version || !features || !closed) {
        fputs("Install or rebuild the SQLCipher library and development headers with SQLite >= 3.27.0,\n"
              "SQLITE_ENABLE_FTS5 enabled, and window functions enabled; then rerun make check-native-deps.\n"
              "Updating the separate sqlite3 package does not update the SQLCipher library linked by MARS.\n",
              stderr);
        probe_install_instructions();
        return 1;
    }
    if (!quiet) fputs("SQLCipher version and required SQL features verified.\n", stderr);
    return 0;
}
