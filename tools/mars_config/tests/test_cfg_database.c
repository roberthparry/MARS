/**
 * @file test_cfg_database.c
 * @brief Isolated native database installation, streaming and failure regressions.
 *
 * Synthetic SQL fixtures exercise SQLCipher imports, recursive source reads,
 * trigger completeness, private publication and literal credential precedence.
 * Packaged almanac coverage checks the complete coefficient imports and encrypted
 * reopening. The jurisdiction fixture checks saved choices and failed replacements.
 * One complete plaintext globe fixture distinguishes fresh reinstallation from layer-preserving updates.
 * A compact encrypted fixture checks key routing, preserved layers and failed replacement independently.
 * Fresh-install checks use the shared compact PNG catalogue rather than the production imagery collection.
 * Release preparation restores missing stores while retaining existing data, paths and credentials.
 * Every fixture runs beneath the
 * disposable test_cfg_isolated home; no user's configuration is accessed.
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "file-t.h"
#include "almanac-t.h"
#include "globe-t.h"
#include "jurisdiction-t.h"
#include "sqlite-t.h"
#include "mars_settings.h"
#include "cfg_storage.h"
#include "cfg_database.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"
#include "test_harness.h"
#include "test_cfg_support.h"
#include "test_cfg_calendar_fixture.h"

static file_t *test_cfg_database_file(const char *directory, const char *name)
{
    string_t *path = string_sprintf("%s/%s", directory, name);
    file_t *file = path ? file_new(path) : NULL;
    string_free(path);
    return file;
}

static bool test_cfg_database_texture_fixture(void)
{
    const char *root = test_cfg_texture_root();
    string_t *textures = root ? string_sprintf("%s/packaging/globe-db/textures", root) : NULL;
    bool ok = textures && !setenv("MARS_GLOBE_TEXTURE_DIR", string_c_str(textures), 1);
    string_free(textures);
    return ok;
}

static bool test_cfg_database_write(const char *directory, const char *name, const char *text)
{
    file_t *file = test_cfg_database_file(directory, name);
    string_t *body = string_new_with(text);
    bool ok = file && body && file_write_all_text(file, body);
    string_free(body);
    file_free(file);
    return ok;
}

static bool test_cfg_database_fixture(const char *directory, const char *sql)
{
    file_t *folder = test_cfg_database_file(directory, "packaging/almanac-db");
    bool ok = folder && file_create_directory(folder, 0700, true) && !setenv("MARS_ROOT", directory, 1) &&
              test_cfg_database_write(directory, "packaging/almanac-db/mars_almanac.sql", sql);
    file_free(folder);
    return ok;
}

static bool test_cfg_database_text(const string_t *actual, const char *expected)
{
    return actual && string_view_equals_literal(string_view_all(actual), expected);
}

static bool test_cfg_database_setting(file_t *file, const char *name, const char *expected)
{
    (void)file;
    string_t *value = mars_settings_read(name);
    bool ok = test_cfg_database_text(value, expected);
    string_free(value);
    return ok;
}

static sqlite_t *test_cfg_database_open(file_t *file, const char *password)
{
    string_t *path = string_new_with(file_path(file)), *key = string_new_with(password);
    sqlite_t *db = path && key ? cfg_database_open(path, key) : NULL;
    if (db && !sqlite_set_cache_budget(db, NULL, 8192)) {
        sqlite_close(db);
        db = NULL;
    }
    string_free(key);
    string_free(path);
    return db;
}

static bool test_cfg_database_query(sqlite_t *db, const char *query, const char *expected)
{
    sqlite_stmt_t *statement = db ? sqlite_stmt_prepare(db, query) : NULL;
    bool ok = statement && sqlite_stmt_step(statement) == SQLITE_STEP_ROW;
    const char *value = ok ? sqlite_stmt_column_text(statement, 0) : NULL;
    if (!value || strcmp(value, expected))
        fprintf(stderr, "Catalogue query: %s; expected %s, got %s\n", query, expected, value ? value : "<none>");
    ok = ok && value && !strcmp(value, expected) && sqlite_stmt_step(statement) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(statement);
    return ok;
}

/* Check the packaged town rows without importing unrelated calendar or ephemeris data. */
static bool test_cfg_database_towns_fixture(const char *directory)
{
    file_t *source = file_new_cstr(MARS_CONFIG_ROOT_DIR "/packaging/jurisdiction-db/town_catalogue_seed.sql");
    string_t *sql = source ? file_read_all_text(source) : NULL;
    string_t *seed = sql ? string_clone(sql) : NULL;
    string_t *path = string_sprintf("%s/towns.db", directory), *key = string_new_with("town-catalogue-fixture");
    sqlite_t *db = path && key ? sqlite_open_encrypted(path, key) : NULL;
    string_t *seed_root = string_new_with(directory), *seed_name = string_new_with("town_seed.sql");
    bool ok = seed && db && seed_root && seed_name &&
              sqlite_exec_cstr(db, "BEGIN") &&
              test_cfg_database_write(directory, "town_seed.sql", string_c_str(seed)) &&
              cfg_database_import(db, seed_root, seed_name) &&
              test_cfg_database_query(db,
                  "SELECT count(*) FROM jurisdiction_town_seed WHERE jurisdiction_id='IT' "
                  "AND CAST(latitude AS REAL) BETWEEN 38.8 AND 41.4 "
                  "AND CAST(longitude AS REAL) BETWEEN 8 AND 10 AND timezone_code=263 "
                  "AND CAST(elevation_metres AS INTEGER) BETWEEN 0 AND 1000 AND is_default='N'", "54") &&
              test_cfg_database_query(db,
                  "SELECT count(*) FROM jurisdiction_town_seed WHERE jurisdiction_id='IT' "
                  "AND town_name IN ('Cagliari','Sassari','Olbia','Alghero','Oristano','Nuoro','Bosa','Tortolì')", "8") &&
              test_cfg_database_query(db,
                  "SELECT latitude||'|'||longitude||'|'||elevation_metres FROM jurisdiction_town_seed "
                  "WHERE jurisdiction_id='IT' AND town_name='Cagliari'", "39.23054|9.11917|41") &&
              test_cfg_database_query(db,
                  "SELECT latitude||'|'||longitude FROM jurisdiction_town_seed "
                  "WHERE jurisdiction_id='IT' AND town_name='La Maddalena'", "41.21424|9.40833") &&
              test_cfg_database_query(db,
                  "SELECT count(*) FROM (SELECT town_name FROM jurisdiction_town_seed WHERE jurisdiction_id='IT' "
                  "GROUP BY town_name HAVING count(*)>1)", "0") &&
              test_cfg_database_query(db,
                  "SELECT count(*) FROM jurisdiction_town_seed WHERE jurisdiction_id='FR' "
                  "AND town_name IN ('Ajaccio','Bastia','Biguglia','Borgo','Calvi','Corte','Porto-Vecchio') "
                  "AND CAST(latitude AS REAL) BETWEEN 41.3 AND 43 "
                  "AND CAST(longitude AS REAL) BETWEEN 8.5 AND 9.6 AND timezone_code=259 "
                  "AND CAST(elevation_metres AS INTEGER) BETWEEN 0 AND 500 AND is_default='N'", "7") &&
              test_cfg_database_query(db,
                  "SELECT latitude||'|'||longitude||'|'||elevation_metres FROM jurisdiction_town_seed "
                  "WHERE jurisdiction_id='FR' AND town_name='Ajaccio'", "41.91886|8.73812|12") &&
              test_cfg_database_query(db,
                  "SELECT count(*) FROM (SELECT town_name FROM jurisdiction_town_seed WHERE jurisdiction_id='FR' "
                  "GROUP BY town_name HAVING count(*)>1)", "0");
    string_t *root = string_new_with(MARS_CONFIG_ROOT_DIR);
    string_t *world = string_new_with("packaging/jurisdiction-db/mars_jurisdiction_world_towns.sql");
    /* The pinned Europe/Simferopol group covers 45 Crimean towns. Their UA membership is independent
     * of civil-clock rules; verify both seeds and the normalised installer output below. */
    ok = ok && root && world && cfg_database_import(db, root, world) &&
         test_cfg_database_query(db, "SELECT count(*) FROM jurisdiction_town_seed", "50004") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town_seed WHERE timezone_code=268 AND jurisdiction_id='UA'", "45") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town_seed WHERE timezone_code=268 AND jurisdiction_id<>'UA'", "0") &&
         test_cfg_database_query(db,
             "SELECT jurisdiction_id FROM jurisdiction_town_seed WHERE town_name='Chornomorske (GeoNames 710687)' "
             "AND latitude='45.50657' AND longitude='32.69776'", "UA") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town_seed WHERE jurisdiction_id IN('AU','AU-QLD') "
             "AND town_name='Mooloolaba' AND latitude='-26.68164' AND longitude='153.11925' "
             "AND elevation_metres='7' AND timezone_code=222 AND is_default='N'", "2") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town_seed WHERE jurisdiction_id IN('AU','AU-QLD') "
             "AND town_name='Maroochydore' AND latitude='-26.66008' AND longitude='153.09953' "
             "AND elevation_metres='6' AND timezone_code=222 AND is_default='N'", "2") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town_seed WHERE jurisdiction_id='ZA' AND timezone_code=24 "
             "AND is_default='N' AND CAST(latitude AS REAL) BETWEEN -34.3 AND -33.8 "
             "AND CAST(longitude AS REAL) BETWEEN 18.2 AND 18.5 "
             "AND town_name IN('Fish Hoek','Noordhoek','Camps Bay','Hout Bay','Clovelly','Kalk Bay',"
             "'Kommetjie','Llandudno','Scarborough','Glencairn','Saint James','Clifton','Sea Point',"
             "'Bantry Bay','Bakoven','Mouille Point')", "16") &&
         test_cfg_database_query(db,
             "SELECT latitude||'|'||longitude||'|'||elevation_metres FROM jurisdiction_town_seed "
             "WHERE jurisdiction_id='ZA' AND town_name='Fish Hoek'", "-34.13419|18.42484|15") &&
         test_cfg_database_query(db,
             "SELECT latitude||'|'||longitude||'|'||elevation_metres FROM jurisdiction_town_seed "
             "WHERE jurisdiction_id='ZA' AND town_name='Kalk Bay'", "-34.12752|18.44819|9") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town_seed WHERE jurisdiction_id='GB-WLS'", "110") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town_seed WHERE jurisdiction_id='GB-ENG' "
             "AND town_name IN('Colchester','West Mersea') AND timezone_code=250", "2") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town_seed WHERE jurisdiction_id='GB' "
             "AND town_name IN('Colchester','West Mersea')", "0") &&
         test_cfg_database_query(db,
             "SELECT latitude||'|'||longitude||'|'||elevation_metres FROM jurisdiction_town_seed "
             "WHERE jurisdiction_id='GB-WLS' AND town_name='Fishguard' AND timezone_code=250 AND is_default='N'",
             "51.99376|-4.97631|63") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town_seed WHERE jurisdiction_id='IE'", "137") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town_seed WHERE jurisdiction_id='IE' AND town_name='Wexford' "
             "AND latitude='52.33417' AND longitude='-6.4575'", "1") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town_seed WHERE jurisdiction_id='DK' "
             "AND town_name IN('Nykøbing Falster','Nykøbing Mors','Nykøbing Sjælland')", "3") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town_seed WHERE jurisdiction_id='NL' "
             "AND town_name IN('Castricum','Alkmaar')", "2") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town_seed WHERE jurisdiction_id='PT' "
             "AND town_name='Madalena (GeoNames 3372908)' AND latitude='38.53642' AND longitude='-28.5266'", "1") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM (SELECT jurisdiction_id,town_name FROM jurisdiction_town_seed "
             "GROUP BY jurisdiction_id,town_name HAVING count(*)>1)", "0") &&
         test_cfg_database_query(db,
             "SELECT count(DISTINCT substr(jurisdiction_id,1,2))>200 FROM jurisdiction_town_seed", "1") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town_seed WHERE jurisdiction_id='US-IL' "
             "AND town_name='Springfield' AND latitude='39.80172' AND longitude='-89.64371'", "1") &&
         test_cfg_database_query(db,
             "SELECT count(*)>20 FROM jurisdiction_town_seed WHERE jurisdiction_id='PH'", "1") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town_seed WHERE jurisdiction_id='GB' AND town_name='London' "
             "AND latitude='51.5074' AND longitude='-0.1278' AND is_default='Y'", "1");
    ok = ok && sqlite_exec_cstr(db, "COMMIT");
    sqlite_close(db);
    /* Exercise the same nested source entry point as installation, without publishing it. */
    ok = ok && test_cfg_calendar_sources(directory);
    string_t *complete_root = ok ? string_new_with(getenv("MARS_ROOT")) : NULL;
    /* This case verifies the complete normalised town count; menu/publication fixtures need only core towns. */
    file_t *world_source = test_cfg_database_file(MARS_CONFIG_ROOT_DIR, string_c_str(world));
    file_t *world_copy = complete_root ?
        test_cfg_database_file(string_c_str(complete_root), string_c_str(world)) : NULL;
    ok = ok && world_source && world_copy && file_copy(world_source, world_copy, true);
    file_free(world_copy); file_free(world_source);
    file_t *complete = test_cfg_database_file(directory, "complete.db");
    db = ok && complete ? test_cfg_database_open(complete, "town-catalogue-fixture") : NULL;
    string_t *entry = string_new_with("packaging/jurisdiction-db/mars_holiday_rules.sql");
    ok = ok && db && entry && complete_root &&
         sqlite_exec_cstr(db, "PRAGMA cache_size=-16384; PRAGMA temp.cache_size=-512; BEGIN;") &&
         cfg_database_import(db, complete_root, entry) &&
         test_cfg_database_query(db, "SELECT count(*) FROM jurisdiction_town", "50036") &&
         sqlite_exec_cstr(db,
             "INSERT INTO jurisdiction_town(jurisdiction_id,town_name) VALUES "
             "('DK','Kolindsund (former lake)'),('DK','Tastum Sø (former lake)'),"
             "('DK','Gårdbo Sø (former lake)'),('DK','Sjørring Sø (former lake)'),"
             "('DK','Sperring Sø (former lake)');") &&
         cfg_database_source(db, root, "packaging/jurisdiction-db/location_additions.sql", false) &&
         test_cfg_database_query(db, "SELECT count(*) FROM jurisdiction_town", "50036") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town WHERE jurisdiction_id='DK' "
             "AND town_name LIKE '% (former lake)'", "0") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town t JOIN jurisdiction_town_timezone z USING(jurisdiction_town_id) "
             "WHERE t.jurisdiction_id='ZA' AND t.town_name IN('Robben Island','Seal Island','Duiker Island') "
             "AND z.timezone_code=24", "3") &&
         test_cfg_database_query(db,
             "SELECT a.latitude||'|'||b.longitude FROM jurisdiction_town t "
             "JOIN jurisdiction_town_latitude a USING(jurisdiction_town_id) "
             "JOIN jurisdiction_town_longitude b USING(jurisdiction_town_id) "
             "WHERE t.jurisdiction_id='ZA' AND t.town_name='Duiker Island'", "-34.05861|18.32694") &&
         test_cfg_database_query(db,
             "SELECT a.latitude||'|'||b.longitude FROM jurisdiction_town t "
             "JOIN jurisdiction_town_latitude a USING(jurisdiction_town_id) "
             "JOIN jurisdiction_town_longitude b USING(jurisdiction_town_id) "
             "WHERE t.jurisdiction_id='ZA' AND t.town_name='Seal Island'", "-34.1375|18.5825") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town t JOIN jurisdiction_town_timezone z USING(jurisdiction_town_id) "
             "WHERE z.timezone_code=268 AND t.jurisdiction_id='UA'", "45") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town t JOIN jurisdiction_town_timezone z USING(jurisdiction_town_id) "
             "WHERE z.timezone_code=268 AND t.jurisdiction_id<>'UA'", "0") &&
         test_cfg_database_query(db,
             "SELECT jurisdiction_id FROM jurisdiction_town WHERE town_name='Chornomorske (GeoNames 710687)'", "UA") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town t JOIN jurisdiction_town_timezone z USING(jurisdiction_town_id) "
             "WHERE jurisdiction_id='PT-20' AND z.timezone_code=211", "10") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town WHERE jurisdiction_id='PT-20' "
             "AND town_name IN('Horta','Madalena (GeoNames 3372908)')", "2") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town t JOIN jurisdiction_town_timezone z USING(jurisdiction_town_id) "
             "WHERE jurisdiction_id='PT-30' AND z.timezone_code=216", "19") &&
         test_cfg_database_query(db,
             "SELECT group_concat(jurisdiction_id||':'||town_name,'|') FROM "
             "(SELECT d.jurisdiction_id,t.town_name FROM jurisdiction_default_town d "
             "JOIN jurisdiction_town t USING(jurisdiction_town_id) "
             "WHERE d.jurisdiction_id IN('PT','PT-20','PT-30') ORDER BY d.jurisdiction_id)",
             "PT:Lisbon|PT-20:Ponta Delgada|PT-30:Funchal") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town WHERE jurisdiction_id IN('PT-20','PT-30') "
             "AND town_name='Lisbon'", "0") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town_elevation WHERE elevation_metres='-9999'", "0") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town t LEFT JOIN jurisdiction_town_elevation e "
             "USING(jurisdiction_town_id) WHERE e.jurisdiction_town_id IS NULL", "12") &&
         test_cfg_database_query(db,
             "SELECT latitude||'|'||longitude||'|'||coalesce(elevation_metres,'0') FROM jurisdiction_town "
             "JOIN jurisdiction_town_latitude USING(jurisdiction_town_id) "
             "JOIN jurisdiction_town_longitude USING(jurisdiction_town_id) "
             "LEFT JOIN jurisdiction_town_elevation USING(jurisdiction_town_id) "
             "WHERE jurisdiction_id='MU' AND town_name='Baie du Tombeau'", "-20.1194|57.4914|0") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town t JOIN jurisdiction_town_timezone z "
             "ON z.jurisdiction_town_id=t.jurisdiction_town_id WHERE jurisdiction_id IN('AU','AU-QLD') "
             "AND town_name IN('Mooloolaba','Maroochydore') AND z.timezone_code=222", "4");
    ok = ok && sqlite_exec_cstr(db, "COMMIT");
    sqlite_close(db);
    file_free(complete);
    string_free(complete_root);
    string_free(entry);
    string_free(root);
    string_free(world);
    string_free(key);
    string_free(path);
    string_free(seed);
    string_free(seed_root);
    string_free(seed_name);
    string_free(sql);
    file_free(source);
    return ok;
}

static void test_cfg_database_sardinia(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_towns_fixture),
                     "Worldwide and island towns, unique names, coordinates, timezones and preserved defaults");
}

static bool test_cfg_database_mode(file_t *file, unsigned mode)
{
    file_info_t *info = file_get_info(file);
    bool ok = info && file_info_permissions(info) == mode;
    file_info_free(info);
    return ok;
}

static bool test_cfg_database_equal(file_t *left, file_t *right)
{
    bool ok = file_open_read(left) && file_open_read(right);
    char left_bytes[16384], right_bytes[16384];
    size_t left_count = 0, right_count = 0;
    while (ok) {
        ok = file_read(left, left_bytes, sizeof(left_bytes), &left_count) &&
             file_read(right, right_bytes, sizeof(right_bytes), &right_count) && left_count == right_count &&
             !memcmp(left_bytes, right_bytes, left_count);
        if (!ok || !left_count)
            break;
    }
    if (file_is_open(left))
        ok = file_close(left) && ok;
    if (file_is_open(right))
        ok = file_close(right) && ok;
    return ok;
}

static bool test_cfg_database_stream_fixture(const char *directory)
{
    const char *sql = "-- root source\nBEGIN TRANSACTION;\n"
                      "CREATE TABLE source(value TEXT);\nCREATE TABLE audit(value TEXT);\n"
                      "CREATE TRIGGER source_audit AFTER INSERT ON source BEGIN\n"
                      " INSERT INTO audit VALUES(CASE WHEN new.value <> '' THEN 'semi;colon' ELSE 'empty' END);\n"
                      " INSERT INTO audit VALUES('it''s another; statement');\nEND;\n"
                      "/* comment before nested read; */\n"
                      ".read 'packaging/almanac-db/nested file.sql'\n"
                      "ROLLBACK;\n"
                      "CREATE TABLE retained(value TEXT);\n"
                      "INSERT INTO retained VALUES('committed; text');\n";
    bool ok = test_cfg_database_fixture(directory, sql) &&
              test_cfg_database_write(directory, "packaging/almanac-db/nested file.sql",
                                      "INSERT INTO source VALUES('a; quoted value');\n"
                                      ".read packaging/almanac-db/nested.sql\n") &&
              test_cfg_database_write(directory, "packaging/almanac-db/nested.sql",
                                      "INSERT INTO source VALUES('Québec');\n") &&
              test_cfg_database_write(directory, "packaging/almanac-db/mars_almanac_chebyshev.sql",
                                      "INSERT INTO retained VALUES('supplement one');\n") &&
              test_cfg_database_write(directory, "packaging/almanac-db/mars_almanac_frame_rotation.sql",
                                      "INSERT INTO retained VALUES('supplement two')") &&
              !cfg_database_run(false, NULL, "fixture-'\"-$(false)-`false`-\\-clé", NULL, NULL, false);
    file_t *database = test_cfg_database_file(directory, "almanac/almanac.db");
    file_t *configuration = test_cfg_database_file(directory, "mars.db");
    file_t *folder = test_cfg_database_file(directory, "almanac");
    sqlite_t *db = ok ? test_cfg_database_open(database, "fixture-'\"-$(false)-`false`-\\-clé") : NULL;
    ok = ok && db && test_cfg_database_query(db, "SELECT count(*) FROM retained", "3") &&
         test_cfg_database_query(db, "SELECT count(*) FROM sqlite_master WHERE name IN ('source','audit')", "0") &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_KEY", "fixture-'\"-$(false)-`false`-\\-clé") &&
         test_cfg_database_mode(database, 0600) && test_cfg_database_mode(configuration, 0600) &&
         test_cfg_database_mode(folder, 0700);
    sqlite_close(db);
    /* Reload the apostrophe-containing key exclusively from stored configuration. */
    ok = ok && !cfg_database_run(false, NULL, NULL, NULL, NULL, false);
    file_free(folder);
    file_free(configuration);
    file_free(database);
    return ok;
}

static bool test_cfg_database_precedence_fixture(const char *directory)
{
    bool ok = test_cfg_database_fixture(directory, "CREATE TABLE fixture(value); INSERT INTO fixture VALUES(7);\n");
    file_t *configuration = test_cfg_database_file(directory, "mars.db");
    string_t *legacy = string_sprintf("%s/almanac.db", directory);
    string_t *default_path = string_sprintf("%s/almanac/almanac.db", directory);
    string_t *explicit_path = string_sprintf("%s/explicit.db", directory);
    string_t *environment_path = string_sprintf("%s/environment.db", directory);
    ok = ok && configuration && legacy && default_path && explicit_path && environment_path &&
         !setenv("MARS_ALMANAC_DB_KEY", "environment-key", 1) &&
         !setenv("MARS_ALMANAC_DB_PATH", string_c_str(environment_path), 1) &&
         !cfg_database_run(false, string_c_str(explicit_path), " explicit-key ", NULL, NULL, false) &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_PATH", string_c_str(explicit_path)) &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_KEY", "explicit-key") &&
         !cfg_database_run(false, NULL, NULL, NULL, NULL, false) &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_PATH", string_c_str(environment_path)) &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_KEY", "environment-key") &&
         !unsetenv("MARS_ALMANAC_DB_KEY") && !unsetenv("MARS_ALMANAC_DB_PATH") &&
         !cfg_database_run(false, NULL, NULL, NULL, NULL, false) &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_KEY", "environment-key") &&
         !cfg_database_run(false, string_c_str(legacy), NULL, NULL, NULL, false) &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_PATH", string_c_str(legacy));
    file_t *old = legacy ? file_new(legacy) : NULL;
    ok = ok && old && file_exists(old) && !file_last_error(old);
    file_free(old);
    string_free(legacy);
    string_free(default_path);
    string_free(explicit_path);
    string_free(environment_path);
    file_free(configuration);
    return ok;
}

static bool test_cfg_database_failure_fixture(const char *directory)
{
    bool ok = test_cfg_database_fixture(directory, "CREATE TABLE fixture(value); INSERT INTO fixture VALUES(7);\n") &&
              cfg_database_run(false, "", NULL, NULL, NULL, false) == 1 &&
              !cfg_database_run(false, NULL, "original-key", NULL, NULL, false);
    file_t *database = test_cfg_database_file(directory, "almanac/almanac.db");
    file_t *configuration = test_cfg_database_file(directory, "mars.db");
    file_t *snapshot = test_cfg_database_file(directory, "snapshot.db");
    file_t *saved_config = test_cfg_database_file(directory, "settings-snapshot.db");
    ok = ok && database && configuration && snapshot && saved_config && file_copy(database, snapshot, false) &&
         file_copy(configuration, saved_config, false);
    static const char *const failures[] = {
        "CREATE TABLE broken(;\n",
        ".read packaging/almanac-db/missing.sql\n",
        ".read packaging/almanac-db/mars_almanac.sql\n",
        ".shell touch must-not-exist\n",
        "BEGIN; CREATE TABLE unfinished(value);\n",
        "CREATE TABLE unfinished(value TEXT); INSERT INTO unfinished VALUES('unterminated;\n"};
    for (size_t i = 0; i < sizeof(failures) / sizeof(*failures) && ok; ++i)
        ok = test_cfg_database_write(directory, "packaging/almanac-db/mars_almanac.sql", failures[i]) &&
             cfg_database_run(false, NULL, "replacement-key", NULL, NULL, false) == 1 &&
             test_cfg_database_equal(database, snapshot) && test_cfg_database_equal(configuration, saved_config);
    ok = ok && cfg_database_run(false, NULL, "bad\nkey", NULL, NULL, false) == 1 &&
         test_cfg_database_equal(database, snapshot) && test_cfg_database_equal(configuration, saved_config);
    /* A settings failure after data replacement must restore the previous database bytes. */
    string_t *parent = string_new_with(directory);
    string_t *staging = parent ? cfg_database_stage(parent) : NULL;
    file_t *replacement = staging ? test_cfg_database_file(string_c_str(staging), "database.db") : NULL;
    string_t *invalid = string_new_with("{\"INVALID_KEY\":\"rejected\"}");
    ok = ok && replacement && invalid &&
         test_cfg_database_write(string_c_str(staging), "database.db", "replacement fixture") &&
         !cfg_database_publish(replacement, database, staging, invalid) &&
         test_cfg_database_equal(database, snapshot) && test_cfg_database_equal(configuration, saved_config);
    file_free(replacement); string_free(invalid);
    cfg_database_cleanup(staging); string_free(staging); string_free(parent);
    file_free(saved_config);
    file_free(snapshot);
    file_free(configuration);
    file_free(database);
    return ok;
}

static bool test_cfg_database_bound_fixture(const char *directory)
{
    bool ok = test_cfg_database_fixture(directory, "CREATE TABLE fixture(value);\n") &&
              !cfg_database_run(false, NULL, "fixture-key", NULL, NULL, false);
    file_t *sql = test_cfg_database_file(directory, "packaging/almanac-db/mars_almanac.sql");
    file_t *database = test_cfg_database_file(directory, "almanac/almanac.db");
    file_t *snapshot = test_cfg_database_file(directory, "snapshot.db");
    ok = ok && sql && database && snapshot && file_copy(database, snapshot, false) && file_open_write(sql);
    char chunk[16384];
    memset(chunk, ' ', sizeof(chunk));
    /* The source exceeds the bounded line size; no unbounded read is permitted. */
    for (size_t i = 0; i < 65 && ok; ++i) {
        size_t written = 0;
        ok = file_write(sql, chunk, sizeof(chunk), &written) && written == sizeof(chunk);
    }
    if (sql && file_is_open(sql))
        ok = file_close(sql) && ok;
    ok = ok && cfg_database_run(false, NULL, NULL, NULL, NULL, false) == 1 &&
         test_cfg_database_equal(database, snapshot);
    file_free(snapshot);
    file_free(database);
    file_free(sql);
    return ok;
}

static bool test_cfg_database_large_source(const char *directory, bool rollback, bool broken)
{
    bool ok = test_cfg_database_fixture(directory, "BEGIN; CREATE TABLE coefficients(value BLOB);\n"
                                                   "INSERT INTO coefficients(value) VALUES\n");
    file_t *sql = test_cfg_database_file(directory, "packaging/almanac-db/mars_almanac.sql");
    ok = ok && sql && file_open(sql, FILE_MODE_APPEND, FILE_ACCESS_WRITE);
    char tuple[519];
    tuple[0] = '(';
    tuple[1] = 'X';
    tuple[2] = '\'';
    memset(tuple + 3, '0', 512);
    tuple[515] = '\'';
    tuple[516] = ')';
    tuple[517] = ',';
    tuple[518] = '\n';
    /* One 19.8 MiB INSERT exceeds the general SQL buffer, as the real Chebyshev file does. */
    for (size_t i = 0; i < 40000 && ok; ++i) {
        if (i == 500) {
            string_t *comment = string_new_with("\n  -- attribution between flushed batches; 'quoted' text\n\t\n");
            ok = comment && file_write_text(sql, comment);
            string_free(comment);
            if (!ok)
                break;
        }
        if (i == 39999)
            tuple[517] = broken ? ',' : ';';
        size_t written = 0;
        ok = file_write(sql, tuple, 519, &written) && written == 519;
    }
    string_t *tail =
        string_new_with(broken     ? "invalid tuple;\n"
                        : rollback ? "ROLLBACK; CREATE TABLE retained(value); INSERT INTO retained VALUES(9);\n"
                                   : "COMMIT;\n");
    ok = ok && tail && file_write_text(sql, tail);
    if (sql && file_is_open(sql))
        ok = file_close(sql) && ok;
    string_free(tail);
    file_free(sql);
    return ok;
}

static bool test_cfg_database_large_fixture(const char *directory)
{
    bool ok = test_cfg_database_large_source(directory, false, false) &&
              !cfg_database_run(false, NULL, "large-fixture-key", NULL, NULL, false);
    file_t *database = test_cfg_database_file(directory, "almanac/almanac.db");
    file_t *snapshot = test_cfg_database_file(directory, "snapshot.db");
    sqlite_t *db = ok ? test_cfg_database_open(database, "large-fixture-key") : NULL;
    ok = ok && db && test_cfg_database_query(db, "SELECT count(*) FROM coefficients", "40000") &&
         test_cfg_database_query(db, "SELECT min(length(value)) FROM coefficients", "256");
    sqlite_close(db);
    ok = ok && file_copy(database, snapshot, false) && test_cfg_database_large_source(directory, false, true) &&
         cfg_database_run(false, NULL, NULL, NULL, NULL, false) == 1 && test_cfg_database_equal(database, snapshot) &&
         test_cfg_database_large_source(directory, true, false) &&
         !cfg_database_run(false, NULL, NULL, NULL, NULL, false);
    db = ok ? test_cfg_database_open(database, "large-fixture-key") : NULL;
    ok = ok && db && test_cfg_database_query(db, "SELECT value FROM retained", "9") &&
         test_cfg_database_query(db, "SELECT count(*) FROM sqlite_master WHERE name='coefficients'", "0");
    sqlite_close(db);
    file_free(snapshot);
    file_free(database);
    return ok;
}

static bool test_cfg_database_conflict_source(const char *directory, bool broken)
{
    bool ok =
        test_cfg_database_fixture(directory, "BEGIN;\nCREATE TABLE conflict_rows(id INTEGER PRIMARY KEY,value TEXT);\n"
                                             "INSERT INTO conflict_rows VALUES(1,'original');\n"
                                             "InSeRt\n Or\tIgNoRe\n InTo conflict_rows\n (id, value)\n VaLuEs\n");
    file_t *source = test_cfg_database_file(directory, "packaging/almanac-db/mars_almanac.sql");
    ok = ok && source && file_open(source, FILE_MODE_APPEND, FILE_ACCESS_WRITE);
    const char ignored[] = "(1, 'ignore decomposed e\xcc\x81; it''s literal'),\n";
    const char replaced[] = "(2, 'replace decomposed e\xcc\x81; it''s literal'),\n";
    const char middle[] = "(2, 'inserted');\n"
                          "INSERT\nOR\nREPLACE\nINTO conflict_rows\n(id, value)\nVALUES\n";
    const char tail[] = "(2, 'final e\xcc\x81');\n"
                        "CREATE TABLE literals(value TEXT);\n"
                        "INSERT INTO literals VALUES('semi;colon\n.read never-executed.sql\nquote'';tail');\n"
                        "INSERT INTO literals VALUES('trailing comment'); /* multiple\n"
                        "comment lines ;\n*/\n"
                        "/* a complete comment before a recursive source */\n"
                        ".read packaging/almanac-db/conflict_tail.sql\nCOMMIT;\n";
    const char invalid[] = "(2, 'unterminated\n";
    for (size_t i = 0; i < 4096 && ok; ++i) {
        size_t written = 0;
        ok = file_write(source, ignored, sizeof(ignored) - 1, &written) && written == sizeof(ignored) - 1;
    }
    size_t written = 0;
    ok = ok && file_write(source, middle, sizeof(middle) - 1, &written) && written == sizeof(middle) - 1;
    for (size_t i = 0; i < 4096 && ok; ++i) {
        written = 0;
        ok = file_write(source, replaced, sizeof(replaced) - 1, &written) && written == sizeof(replaced) - 1;
    }
    size_t length = broken ? sizeof(invalid) - 1 : sizeof(tail) - 1;
    ok = ok && file_write(source, broken ? invalid : tail, length, &written) && written == length;
    if (source && file_is_open(source))
        ok = file_close(source) && ok;
    file_free(source);
    /* A valid expression must leave the literal fast path before the first batch and execute unchanged. */
    return ok && test_cfg_database_write(directory, "packaging/almanac-db/conflict_tail.sql",
                                         "INSERT OR IGNORE INTO conflict_rows VALUES\n(3, 2+3);\n");
}

static bool test_cfg_database_conflict_fixture(const char *directory)
{
    bool ok = test_cfg_database_conflict_source(directory, false);
    file_t *source = test_cfg_database_file(directory, "packaging/almanac-db/mars_almanac.sql");
    file_t *saved_source = test_cfg_database_file(directory, "source_snapshot.sql");
    file_t *database = test_cfg_database_file(directory, "almanac/almanac.db");
    file_t *configuration = test_cfg_database_file(directory, "mars.db");
    file_t *snapshot = test_cfg_database_file(directory, "snapshot.db");
    file_t *saved_config = test_cfg_database_file(directory, "settings-snapshot.db");
    ok = ok && source && saved_source && database && configuration && snapshot && saved_config &&
         file_copy(source, saved_source, false) &&
         !cfg_database_run(false, NULL, "conflict-fixture", NULL, NULL, false) &&
         test_cfg_database_equal(source, saved_source);
    sqlite_t *db = ok ? test_cfg_database_open(database, "conflict-fixture") : NULL;
    ok = ok && db && test_cfg_database_query(db, "SELECT count(*) FROM conflict_rows", "3") &&
         test_cfg_database_query(db, "SELECT value FROM conflict_rows WHERE id=1", "original") &&
         test_cfg_database_query(db, "SELECT hex(value) FROM conflict_rows WHERE id=2", "66696E616C2065CC81") &&
         test_cfg_database_query(db, "SELECT value FROM conflict_rows WHERE id=3", "5") &&
         test_cfg_database_query(db, "SELECT count(*) FROM literals", "2") &&
         test_cfg_database_query(db, "SELECT value FROM literals WHERE rowid=1",
                                 "semi;colon\n.read never-executed.sql\nquote';tail");
    sqlite_close(db);
    ok = ok && file_copy(database, snapshot, false) && file_copy(configuration, saved_config, false) &&
         test_cfg_database_conflict_source(directory, true) &&
         cfg_database_run(false, NULL, "replacement-key", NULL, NULL, false) == 1 &&
         test_cfg_database_equal(database, snapshot) && test_cfg_database_equal(configuration, saved_config);
    file_free(saved_config);
    file_free(snapshot);
    file_free(configuration);
    file_free(database);
    file_free(saved_source);
    file_free(source);
    return ok;
}

static bool test_cfg_database_path_fixture(const char *directory)
{
    bool ok = test_cfg_database_fixture(directory, "CREATE TABLE fixture(value);\n") &&
              !cfg_database_run(false, NULL, "fixture-key", NULL, NULL, false);
    file_t *database = test_cfg_database_file(directory, "almanac/almanac.db");
    file_t *configuration = test_cfg_database_file(directory, "mars.db");
    file_t *snapshot = test_cfg_database_file(directory, "snapshot.db");
    file_t *saved_config = test_cfg_database_file(directory, "settings-snapshot.db");
    file_t *link = test_cfg_database_file(directory, "link.db");
    ok = ok && database && configuration && snapshot && saved_config && link && file_copy(database, snapshot, false) &&
         file_copy(configuration, saved_config, false) && file_create_symlink(database, link) &&
         cfg_database_run(false, file_path(link), "replacement-key", NULL, NULL, false) == 1 &&
         test_cfg_database_equal(database, snapshot) && test_cfg_database_equal(configuration, saved_config) &&
         cfg_database_run(false, file_path(configuration), "replacement-key", NULL, NULL, false) == 1 &&
         test_cfg_database_equal(configuration, saved_config);
    file_free(link);
    file_free(saved_config);
    file_free(snapshot);
    file_free(configuration);
    file_free(database);
    return ok;
}

static bool test_cfg_database_almanac_coverage(sqlite_t *db, const char *family, const char *expected_series)
{
    /* Each indexed series range must contain every declared segment and correctly sized coefficient blobs. */
    string_t *query = string_sprintf("SELECT count(*) FROM ("
                                     "SELECT s.series_id FROM almanac_%s_series s "
                                     "JOIN almanac_%s_series_segment_count c USING(series_id) "
                                     "JOIN almanac_%s_series_degree d USING(series_id) "
                                     "JOIN almanac_%s_series_component_count n USING(series_id) "
                                     "LEFT JOIN almanac_%s_segment p USING(series_id) "
                                     "GROUP BY s.series_id "
                                     "HAVING count(p.segment_index)=c.segment_count "
                                     "AND min(p.segment_index)=0 AND max(p.segment_index)=c.segment_count-1 "
                                     "AND min(length(p.coefficient_blob))=8*(d.degree+1)*n.component_count "
                                     "AND max(length(p.coefficient_blob))=8*(d.degree+1)*n.component_count)",
                                     family, family, family, family, family);
    bool ok = query && test_cfg_database_query(db, string_c_str(query), expected_series);
    string_free(query);
    return ok;
}

static bool test_cfg_database_packaged_almanac_fixture(const char *directory)
{
    const char *key = "packaged-almanac-'fixture";
    bool ok = !setenv("MARS_ROOT", MARS_CONFIG_ROOT_DIR, 1) && !cfg_database_run(false, NULL, key, NULL, NULL, false);
    file_t *database = test_cfg_database_file(directory, "almanac/almanac.db");
    file_t *configuration = test_cfg_database_file(directory, "mars.db");
    ok = ok && database && configuration && test_cfg_database_mode(database, 0600) &&
         test_cfg_database_mode(configuration, 0600) &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_KEY", key) &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_PATH", file_path(database));
    sqlite_t *db = ok ? test_cfg_database_open(database, key) : NULL;
    ok = ok && db && test_cfg_database_query(db, "SELECT count(*)>0 FROM almanac_body", "1") &&
         test_cfg_database_query(db, "SELECT count(*) FROM almanac_chebyshev_position_series", "7") &&
         test_cfg_database_query(db, "SELECT count(*) FROM almanac_chebyshev_position_segment", "188332") &&
         test_cfg_database_query(db, "SELECT count(*) FROM almanac_frame_rotation_series", "1") &&
         test_cfg_database_query(db, "SELECT count(*) FROM almanac_frame_rotation_segment", "1100") &&
         test_cfg_database_query(db, "SELECT count(*) FROM sqlite_master WHERE name LIKE 'shoreline_%'", "0") &&
         test_cfg_database_almanac_coverage(db, "chebyshev_position", "7") &&
         test_cfg_database_almanac_coverage(db, "frame_rotation", "1");
    sqlite_close(db);
    /* Opening with an incorrect key must fail; it must not prevent a later correct-key reopen. */
    db = ok ? test_cfg_database_open(database, "incorrect-packaged-almanac-key") : NULL;
    ok = ok && !db;
    sqlite_close(db);
    db = ok ? test_cfg_database_open(database, key) : NULL;
    ok = ok && db && test_cfg_database_query(db, "SELECT count(*) FROM almanac_chebyshev_position_segment", "188332");
    sqlite_close(db);
    file_free(configuration);
    file_free(database);
    return ok;
}

struct test_cfg_country_search {
    const char *code;
    bool found;
};

static bool test_cfg_country_search_place(const jurisdict_place_t *place, void *context)
{
    struct test_cfg_country_search *search = context;
    search->found = place && place->jurisdiction_code && !strncmp(place->jurisdiction_code, search->code, 2) &&
                    place->name && *place->name && place->latitude && *place->latitude &&
                    place->longitude && *place->longitude && place->timezone && *place->timezone;
    return false;
}

/* Exercise every shipped country name through the public API, including uninhabited territories and namesakes. */
static bool test_cfg_country_searches(sqlite_t *db)
{
    /* Library callers load the resolved snapshot; saving installer choices does not export credentials. */
    jurisdiction_t *engine = mars_settings_load() ? jurisdict_open(NULL) : NULL;
    sqlite_stmt_t *countries = db ? sqlite_stmt_prepare(db,
        "SELECT jurisdiction_id,name FROM jurisdiction WHERE jurisdiction_type='country' ORDER BY name") : NULL;
    bool ok = engine && countries;
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    size_t checked = 0;
    while (ok && (step = sqlite_stmt_step(countries)) == SQLITE_STEP_ROW) {
        string_t *name = string_new_with(sqlite_stmt_column_text(countries, 1));
        struct test_cfg_country_search search = {.code = sqlite_stmt_column_text(countries, 0)};
        ok = name && jurisdict_search_towns(engine, name, 1, test_cfg_country_search_place, &search) && search.found;
        if (!ok) fprintf(stderr, "Country search failed: %s\n", sqlite_stmt_column_text(countries, 1));
        string_free(name);
        ++checked;
    }
    sqlite_stmt_finalize(countries);
    jurisdict_close(engine);
    return ok && step == SQLITE_STEP_DONE && checked == 249;
}

static bool test_cfg_database_jurisdiction_mode(const char *directory, const char *password)
{
    bool ok = !setenv("MARS_ROOT", MARS_CONFIG_ROOT_DIR, 1) && !unsetenv("MARS_CALENDAR_LOCATION_ARGUMENT") &&
              !unsetenv("MARS_CALENDAR_LANGUAGE_ARGUMENT") && !setenv("LANG", "en_GB.UTF-8", 1) &&
              test_cfg_calendar_sources(directory) &&
              !cfg_database_run(true, NULL, password, "limmen", NULL, false);
    file_t *database = test_cfg_database_file(directory, "jurisdiction/mars_jurisdiction_rules.db");
    file_t *configuration = test_cfg_database_file(directory, "mars.db");
    file_t *snapshot = test_cfg_database_file(directory, "snapshot.db");
    file_t *saved_config = test_cfg_database_file(directory, "settings-snapshot.db");
    sqlite_t *db = ok ? test_cfg_database_open(database, password) : NULL;
    ok = ok && db && test_cfg_database_setting(configuration, "MARS_CALENDAR_LOCATION", "Limmen, NL") &&
         test_cfg_database_setting(configuration, "MARS_JURISDICTION_DB_KEY", password) &&
         test_cfg_database_setting(configuration, "MARS_CALENDAR_LANGUAGE", "nl_NL") &&
         test_cfg_database_query(db,
                                 "SELECT location||'|'||jurisdiction||'|'||timezone||'|'||locale "
                                 "FROM calendar_local_settings",
                                 "Limmen|NL|Europe/Amsterdam|nl_NL") &&
         test_cfg_database_query(db,
                                 "SELECT [Day Name]||'|'||[Month Name Abbrev] FROM calendar_local "
                                 "WHERE FullDateAlternateKey='2024-06-21'",
                                 "vrijdag|jun") &&
         test_cfg_database_query(db,
                                 "SELECT CAST(utc_offset_hours AS INTEGER) FROM calendar_local_days "
                                 "WHERE calendar_date='2024-01-01'",
                                 "1") &&
         test_cfg_database_query(db,
                                 "SELECT CAST(utc_offset_hours AS INTEGER) FROM calendar_local_days "
                                 "WHERE calendar_date='2024-06-21'",
                                 "2") &&
         test_cfg_database_query(db,
                                 "SELECT count(*)>0 FROM jurisdiction_town_search "
                                 "WHERE jurisdiction_town_search MATCH '\"Luderitz\"*'", "1") &&
         test_cfg_database_query(db,
                                 "SELECT (SELECT count(*) FROM jurisdiction_town_search)="
                                 "(SELECT count(*) FROM jurisdiction_town_name)", "1") &&
         test_cfg_database_mode(database, 0600) && test_cfg_database_mode(configuration, 0600) &&
         (*password || test_cfg_country_searches(db));
    sqlite_close(db);
    ok = ok && !cfg_database_run(true, NULL, NULL, NULL, NULL, false) &&
         test_cfg_database_setting(configuration, "MARS_CALENDAR_LOCATION", "Limmen, NL") &&
         test_cfg_database_setting(configuration, "MARS_CALENDAR_LANGUAGE", "nl_NL") &&
         file_copy(database, snapshot, false) && file_copy(configuration, saved_config, false) &&
         cfg_database_run(true, NULL, NULL, "no-such-town-9182", NULL, false) == 1 &&
         test_cfg_database_equal(database, snapshot) && test_cfg_database_equal(configuration, saved_config) &&
         cfg_database_run(true, NULL, NULL, "limmen", "Latin", false) == 1 &&
         test_cfg_database_equal(database, snapshot) && test_cfg_database_equal(configuration, saved_config);
    file_free(saved_config);
    file_free(snapshot);
    file_free(configuration);
    file_free(database);
    return ok;
}

static bool test_cfg_database_jurisdiction_fixture(const char *directory)
{
    return test_cfg_database_jurisdiction_mode(directory, "jurisdiction-'fixture");
}

static bool test_cfg_database_jurisdiction_plain_fixture(const char *directory)
{
    return test_cfg_database_jurisdiction_mode(directory, "");
}

static void test_cfg_database_jurisdiction_plain(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_jurisdiction_plain_fixture),
                     "plaintext jurisdiction installation supports calendar generation and rollback");
}

static bool test_cfg_database_almanac_plain_fixture(const char *directory)
{
    bool ok = test_cfg_database_fixture(directory, "create table fixture(value);insert into fixture values(7);\n") &&
              !cfg_database_run(false, NULL, "saved-encrypted-key", NULL, NULL, false) &&
              !setenv("MARS_ALMANAC_DB_KEY", "", 1) && !cfg_database_run(false, NULL, NULL, NULL, NULL, false);
    file_t *database = test_cfg_database_file(directory, "almanac/almanac.db");
    sqlite_t *db = ok ? test_cfg_database_open(database, "") : NULL;
    ok = ok && db && test_cfg_database_query(db, "select value from fixture", "7");
    sqlite_close(db); file_free(database);
    almanac_t *almanac = ok ? almanac_open() : NULL;
    ok = ok && almanac && !*almanac_last_error(almanac);
    almanac_close(almanac);
    return ok;
}

static void test_cfg_database_almanac_plain(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_almanac_plain_fixture),
                     "an explicitly empty key overrides saved encryption for a new installation");
}

static void test_cfg_database_stream(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_stream_fixture),
                     "native recursive imports preserve triggers, transactions, supplements and literal keys");
}

static void test_cfg_database_precedence(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_precedence_fixture),
                     "CLI, environment and saved database settings retain precedence and migrate legacy paths");
}

static void test_cfg_database_failure(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_failure_fixture),
                     "SQL, recursive includes and incomplete transactions cannot replace existing private data");
}

static void test_cfg_database_bounds(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_bound_fixture),
                     "an excessive SQL line fails with the previous encrypted database unchanged");
}

static void test_cfg_database_jurisdiction(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_jurisdiction_fixture),
                     "native jurisdiction installation persists choices and preserves files after invalid selections");
}

static void test_cfg_database_large(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_large_fixture),
                     "large literal INSERTs use bounded batches and preserve rollback and failed replacements");
}

static void test_cfg_database_paths(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_path_fixture),
                     "symlink database destinations and aliased configuration paths cannot replace private files");
}

static void test_cfg_database_packaged_almanac(void)
{
    TEST_ASSERT_TRUE(
        test_cfg_content_isolated(test_cfg_database_packaged_almanac_fixture),
        "actual packaged almanac imports complete coefficient coverage and rejects incorrect encryption keys");
}

static void test_cfg_database_conflicts(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_conflict_fixture),
                     "multiline conflict headers retain conflict policies, exact SQL text and failure preservation");
}

static void test_cfg_database_transaction_finish(void)
{
    string_t *path = string_new_with(":memory:");
    sqlite_t *db = path ? sqlite_open_plain(path) : NULL;
    bool ok = db && sqlite_exec_cstr(db,
        "pragma foreign_keys=on;"
        "create table parent(id integer primary key);"
        "create table child(parent_id integer references parent(id) deferrable initially deferred);"
        "insert into parent values(7);begin;insert into parent values(8)") &&
        cfg_database_finish_transaction(db, true) &&
        test_cfg_database_query(db, "select count(*) from parent", "2") &&
        sqlite_exec_cstr(db, "begin;insert into parent values(9)") &&
        !cfg_database_finish_transaction(db, false) &&
        test_cfg_database_query(db, "select count(*) from parent", "2");
    /* A deferred constraint fails at commit, so completion must restore the preceding state. */
    ok = ok && sqlite_exec_cstr(db, "begin;insert into child values(999)") &&
        !cfg_database_finish_transaction(db, true) &&
        test_cfg_database_query(db, "select count(*) from child", "0") &&
        test_cfg_database_query(db, "select count(*) from parent", "2") &&
        sqlite_exec_cstr(db, "begin") && cfg_database_finish_transaction(db, true) &&
        !cfg_database_finish_transaction(NULL, true);
    sqlite_close(db); string_free(path);
    TEST_ASSERT_TRUE(ok, "reported transaction completion commits valid work and restores failed work or commits");
}

static void test_cfg_database_public_ddl(void)
{
    string_t *path = string_new_with(":memory:");
    sqlite_t *db = path ? sqlite_open_plain(path) : NULL;
    bool ok = db && sqlite_exec_cstr(db, "create table private_sentinel(value);insert into private_sentinel values(7)");
    static const char *const modes[] = {
        [0] = "pragma secure_delete=off",
        [1] = "pragma secure_delete=on",
        [2] = "pragma secure_delete=fast"
    };
    for (unsigned mode = 0; ok && mode < 3; ++mode) {
        ok = sqlite_exec_cstr(db, modes[mode]) && sqlite_exec_cstr(db, "create table public_asset(value)") &&
             cfg_database_public_ddl(db, "drop table public_asset") &&
             !cfg_database_public_ddl(db, "drop table missing_public_asset");
        sqlite_stmt_t *q = sqlite_stmt_prepare(db, "pragma secure_delete");
        ok = ok && q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int(q, 0) == (int)mode;
        sqlite_stmt_finalize(q);
        ok = ok && test_cfg_database_query(db, "select value from private_sentinel", "7");
    }
    sqlite_close(db); string_free(path);
    TEST_ASSERT_TRUE(ok, "public-asset DDL restores every secure-delete mode after success and failure");
}

static void test_cfg_database_trace(void)
{
    static const char *const sources[] = {
        "packaging/almanac-db/mars_almanac.sql",
        "packaging/almanac-db/mars_almanac_chebyshev.sql",
        "packaging/almanac-db/mars_almanac_frame_rotation.sql",
        "packaging/globe-db/jurisdiction_subdivisions.sql",
        "packaging/globe-db/mapped_water.sql",
        "packaging/jurisdiction-db/flag_emblems.sql",
        "packaging/jurisdiction-db/flag_regions.sql",
        "packaging/jurisdiction-db/location_additions.sql",
        "packaging/jurisdiction-db/location_subdivisions.sql",
        "packaging/jurisdiction-db/town_search.sql",
        "packaging/jurisdiction-db/town_catalogue_refresh.sql",
        "packaging/jurisdiction-db/town_catalogue_seed.sql",
        "packaging/jurisdiction-db/mars_generated_holiday_instances.sql",
        "packaging/jurisdiction-db/mars_manual_holiday_instance_overrides.sql",
        "packaging/jurisdiction-db/mars_holiday_rules_queries.sql",
        "packaging/jurisdiction-db/mars_holiday_rules.sql",
        "packaging/jurisdiction-db/mars_country_jurisdictions.sql",
        "packaging/jurisdiction-db/mars_target_subdivisions.sql",
        "packaging/jurisdiction-db/mars_timezone_rules.sql",
        "packaging/jurisdiction-db/mars_jurisdiction_location_defaults.sql",
        "packaging/jurisdiction-db/mars_jurisdiction_towns.sql",
        "packaging/jurisdiction-db/mars_jurisdiction_world_towns.sql",
        "packaging/jurisdiction-db/mars_generated_first_class_rules.sql",
        "packaging/jurisdiction-db/mars_manual_first_class_rules.sql",
        "packaging/jurisdiction-db/mars_holiday_localized_names.sql",
        "packaging/jurisdiction-db/mars_calendar_locale_names.sql",
        "packaging/jurisdiction-db/mars_calendar_local.sql",
    };
    bool recognised = true, redacted = true;
    for (size_t i = 0; i < sizeof(sources) / sizeof(*sources); ++i) {
        string_t *path = string_new_with(sources[i]);
        recognised = recognised && path && strcmp(cfg_database_trace_label(path), sources[i]) == 0;
        recognised = recognised && path && strcmp(cfg_database_progress_label(path), "Importing custom SQL") != 0;
        string_free(path);
        /* A similar basename must not disclose a caller-controlled source path. */
        path = string_sprintf("X%s", sources[i] + 1);
        redacted = redacted && path && strcmp(cfg_database_trace_label(path), "unlisted SQL source") == 0;
        redacted = redacted && path && strcmp(cfg_database_progress_label(path), "Importing custom SQL") == 0;
        string_free(path);
    }
    static const char *const unknown[] = {
        "", "short.sql", "packaging/almanac-db/mars_almanac.sql.extra",
        "packaging/jurisdiction-db/mars_jurisdiction_location_defaults.sql.extra",
        "packaging/almanac-db/mars_almanac.éql"
    };
    for (size_t i = 0; i < sizeof(unknown) / sizeof(*unknown); ++i) {
        string_t *path = string_new_with(unknown[i]);
        redacted = redacted && path && strcmp(cfg_database_trace_label(path), "unlisted SQL source") == 0;
        string_free(path);
    }
    TEST_ASSERT_TRUE(recognised, "every allowlisted SQL source has its exact diagnostic label");
    TEST_ASSERT_TRUE(redacted, "colliding, unknown and out-of-range source paths remain redacted");
    TEST_ASSERT_TRUE(strcmp(cfg_database_trace_label(NULL), "unlisted SQL source") == 0,
                     "a missing source path remains redacted");
    static const char *const updates[][2] = {
        {"packaging/globe-db/jurisdiction_subdivisions.sql", "Updating globe country and subdivision identifiers"},
        {"packaging/jurisdiction-db/location_additions.sql", "Adding named reference locations"},
        {"packaging/jurisdiction-db/location_subdivisions.sql", "Updating Kaliningrad towns and location defaults"},
        {"packaging/jurisdiction-db/town_search.sql", "Indexing worldwide place names"}
    };
    bool described = true;
    for (size_t i = 0; i < sizeof updates / sizeof *updates; ++i) {
        string_t *path = string_new_with(updates[i][0]);
        described = described && path && !strcmp(cfg_database_progress_label(path), updates[i][1]);
        string_free(path);
    }
    TEST_ASSERT_TRUE(described, "managed updates describe their actual geographic operations");
    TEST_ASSERT_TRUE(!strcmp(cfg_database_progress_label(NULL), "Importing custom SQL"),
                     "missing custom input has an explicit progress description");
}

static bool test_cfg_database_rivers_fixture(const char *directory)
{
    file_t *database = test_cfg_database_file(directory, "rivers.db");
    sqlite_t *db = test_cfg_database_open(database, "river-installer-fixture");
    string_t *root = string_new_with(MARS_CONFIG_ROOT_DIR), *staging = string_new_with(directory);
    bool ok = db && root && staging && sqlite_exec_cstr(db,
        "pragma cache_size=-512;pragma temp.cache_size=-512;create table retained(value);insert into retained values(7);"
        "create table rivers_dataset(id,source);insert into rivers_dataset values(1,'WDBII / GSHHG 2.3.7');"
        "create table rivers_chunk(id);create virtual table rivers_bounds using rtree_i32(id,min_x,max_x,min_y,max_y)") &&
        cfg_database_rivers(db, root, staging) &&
        test_cfg_database_query(db, "select count(*) from sqlite_master where name like 'rivers_%'", "0") &&
        cfg_database_rivers(db, root, staging) &&
        sqlite_exec_cstr(db, "create table rivers_dataset(id,source);insert into rivers_dataset values(1,'replacement fixture')") &&
        cfg_database_rivers(db, root, staging) &&
        test_cfg_database_query(db, "select source from rivers_dataset", "replacement fixture") &&
        test_cfg_database_query(db, "select value from retained", "7");
    file_t *plain = test_cfg_database_file(directory, "rivers.bin");
    ok = ok && plain && !file_exists(plain);
    file_free(plain);
    sqlite_close(db);
    string_free(root); string_free(staging); file_free(database);
    return ok;
}

static void test_cfg_database_rivers(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_rivers_fixture),
                     "retired rivers removed without changing unrelated data");
}

static bool test_cfg_country_flags_fixture(const char *directory)
{
    file_t *database = test_cfg_database_file(directory, "flags.db");
    sqlite_t *db = database ? test_cfg_database_open(database, "country-flags-fixture") : NULL;
    string_t *root = string_new_with(MARS_CONFIG_ROOT_DIR);
    bool ok = db && root && sqlite_exec_cstr(db,
        "create table jurisdiction(jurisdiction_id text primary key,jurisdiction_type text,iso_country_code text,"
        "cldr_region_code text,name text);create table retained(value);insert into retained values(42)") &&
        cfg_database_source(db, root, "packaging/jurisdiction-db/mars_country_jurisdictions.sql", false) &&
        sqlite_exec_cstr(db, "insert into jurisdiction values('GB-ENG','subdivision','GB','GB','England'),"
                             "('GB-WLS','subdivision','GB','GB','Wales'),"
                             "('GB-SCT','subdivision','GB','GB','Scotland'),"
                             "('GB-NIR','subdivision','GB','GB','Northern Ireland'),"
                             "('RU-KGD','subdivision','RU','RU','Kaliningrad Oblast')") &&
        cfg_database_flags(db, root) && cfg_database_flags(db, root) &&
        test_cfg_database_query(db, "select count(*) from jurisdiction_country_flag", "268") &&
        test_cfg_database_query(db, "select count(*) from jurisdiction_flag_region", "20") &&
        test_cfg_database_query(db, "select name from jurisdiction_flag_emblem where code='ZZ-SEA'", "Offshore") &&
        test_cfg_database_query(db, "select count(*) from jurisdiction where jurisdiction_id='ZZ-SEA'", "0") &&
        test_cfg_database_query(db, "select source from jurisdiction_country_flag_source where id=3", "OFFSHORE-NOTICES.txt") &&
        test_cfg_database_query(db, "select value from retained", "42") &&
        test_cfg_database_query(db, "select version from jurisdiction_country_flag_source where id=1", "7.3.2");
    sqlite_stmt_t *q = ok ? sqlite_stmt_prepare(db,
        "select sum(length(svg)) from jurisdiction_country_flag") : NULL;
    if (q && sqlite_stmt_step(q) == SQLITE_STEP_ROW)
        printf("Offline jurisdiction flag SVG bytes: %lld\n", (long long)sqlite_stmt_column_int64(q, 0));
    sqlite_stmt_finalize(q);
    file_t *incomplete = test_cfg_database_file(directory, "packaging/jurisdiction-db/flags");
    string_t *incomplete_root = string_new_with(directory);
    ok = ok && incomplete && incomplete_root && file_create_directory(incomplete, 0700, true) &&
         test_cfg_database_write(directory, "packaging/jurisdiction-db/flags/LICENSE", "fixture licence") &&
         test_cfg_database_write(directory, "packaging/jurisdiction-db/flags/OFFSHORE-NOTICES.txt", "fixture notice") &&
         test_cfg_database_write(directory, "packaging/jurisdiction-db/flag_emblems.sql",
             "insert into jurisdiction_flag_emblem values('ZZ-MIS','Missing artwork')") &&
         !cfg_database_flags(db, incomplete_root) &&
         test_cfg_database_query(db, "select count(*) from jurisdiction_flag_emblem where code='ZZ-MIS'", "0") &&
         test_cfg_database_query(db, "select count(*) from jurisdiction_country_flag", "268") &&
         test_cfg_database_query(db, "select value from retained", "42");
    file_free(incomplete); string_free(incomplete_root);
    sqlite_close(db); string_free(root);
    ok = ok && !setenv("MARS_JURISDICTION_DB_PATH", file_path(database), 1) &&
         !setenv("MARS_JURISDICTION_DB_KEY", "country-flags-fixture", 1);
    jurisdiction_t *j = ok ? jurisdict_open("FR") : NULL;
    string_t *name = NULL, *svg = NULL;
    ok = ok && jurisdict_country(j, "GB", &name, &svg) && name && svg &&
         strstr(string_c_str(svg), "<svg") && !strcmp(string_c_str(name), "Britain (UK)");
    string_free(name); string_free(svg);
    const char *const codes[] = {"GB-WLS", "GB-SCT", "IM", "GB-ENG", "GB-NIR", "JE", "RU-KGD"};
    const char *const names[] = {"Wales", "Scotland", "Isle of Man", "England", "Northern Ireland", "Jersey",
                               "Kaliningrad Oblast"};
    const char *const artwork[] = {"flag-icons-gb-wls", "flag-icons-gb-sct", "flag-icons-im",
                                    "flag-icons-gb-eng", "flag-icons-gb-nir", "flag-icons-je", "flag-icons-ru"};
    for (unsigned i = 0; i < sizeof codes / sizeof *codes; ++i) {
        name = NULL; svg = NULL;
        ok = ok && jurisdict_country(j, codes[i], &name, &svg) && name && svg &&
             !strcmp(string_c_str(name), names[i]) && strstr(string_c_str(svg), artwork[i]) &&
             !strstr(string_c_str(svg), "@keyframes");
        string_free(name); string_free(svg);
    }
    const char *const regions[] = {"GB-CON", "FR-BRE", "FR-COR", "IT-88", "NL-FR", "DE-OFR", "DE-NFR",
                                    "ES-CT", "ES-VC", "ES-IB", "ES-PV", "ES-NA", "FR-BAS", "FR-CAT", "PL-KAS", "FI-SAP",
                                    "DE-SAT", "DE-SOR", "MN-KAZ", "CY-NCY"};
    for (unsigned i = 0; i < sizeof regions / sizeof *regions; ++i) {
        name = NULL; svg = NULL;
        bool valid = jurisdict_country(j, regions[i], &name, &svg) && name && svg &&
                     strstr(string_c_str(svg), "regionalFlag 15s ease-in-out infinite") &&
                     strstr(string_c_str(svg), "0%,33.333333%,100%{opacity:1}50%,83.333333%{opacity:0}") &&
                     strstr(string_c_str(svg), "prefers-reduced-motion:reduce") &&
                     !strstr(string_c_str(svg), "<image");
        if (!valid) fprintf(stderr, "Regional SVG failed: %s\n", regions[i]);
        if (!strcmp(regions[i], "PL-KAS"))
            valid = valid && !strcmp(string_c_str(name), "Kashubia, Poland") &&
                    strstr(string_c_str(svg), "#ffdf00") && strstr(string_c_str(svg), "flag-icons-pl");
        if (!strcmp(regions[i], "FI-SAP"))
            valid = valid && !strcmp(string_c_str(name), "Sámi homeland, Finland") &&
                    strstr(string_c_str(svg), "#0035ad") && strstr(string_c_str(svg), "#dc241f") &&
                    strstr(string_c_str(svg), "flag-icons-fi");
        if (!strcmp(regions[i], "DE-SAT"))
            valid = valid && !strcmp(string_c_str(name), "Saterland, Germany") &&
                    strstr(string_c_str(svg), "flag-icons-de");
        if (!strcmp(regions[i], "DE-SOR"))
            valid = valid && !strcmp(string_c_str(name), "Sorbian Lusatia, Germany") &&
                    strstr(string_c_str(svg), "#00f") && strstr(string_c_str(svg), "#f00") &&
                    strstr(string_c_str(svg), "#fff") && strstr(string_c_str(svg), "flag-icons-de");
        if (!strcmp(regions[i], "MN-KAZ"))
            valid = valid && !strcmp(string_c_str(name), "Kazakh cultural area, Mongolia") &&
                    strstr(string_c_str(svg), "flag-icons-kz") && strstr(string_c_str(svg), "flag-icons-mn");
        if (!strcmp(regions[i], "CY-NCY"))
            valid = valid && !strcmp(string_c_str(name), "Northern Cyprus") &&
                    strstr(string_c_str(svg), "#e30a17") && strstr(string_c_str(svg), "flag-icons-cy") &&
                    !strstr(string_c_str(svg), "flag-icons-tr");
        ok = valid && ok;
        string_free(name); string_free(svg);
    }
    ok = !jurisdict_country(j, "../GB", &name, &svg) && !name && !svg && ok;
    ok = ok && jurisdict_country(j, "ZZ-SEA", &name, &svg) && name && svg &&
         !strcmp(string_c_str(name), "Offshore") && strstr(string_c_str(svg), "<path") &&
         !strstr(string_c_str(svg), "<image") && !strstr(string_c_str(svg), "@keyframes");
    file_t *source = file_new_cstr(MARS_CONFIG_ROOT_DIR "/packaging/jurisdiction-db/flags/zz-sea.svg");
    string_t *expected = source ? file_read_all_text(source) : NULL;
    ok = ok && expected && !string_compare(svg, expected);
    string_free(expected); file_free(source); string_free(name); string_free(svg);
    jurisdict_close(j); file_free(database);
    return ok;
}

static void test_cfg_country_flags(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_country_flags_fixture),
                     "offline flags and country names belong to the catalogue");
}

static bool test_cfg_database_update_fixture(const char *directory)
{
    bool ok = !unsetenv("MARS_GLOBE_DB_PATH") && !unsetenv("MARS_GLOBE_DB_KEY") &&
              !setenv("MARS_ROOT", directory, 1) && cfg_databases_update() == 0;
    file_t *database = test_cfg_database_file(directory, "existing.db");
    sqlite_t *db = ok ? test_cfg_database_open(database, "saved-update-key") : NULL;
    ok = ok && db && sqlite_exec_cstr(db,
        "create table jurisdiction(jurisdiction_id text primary key,jurisdiction_type text,iso_country_code text,"
        "parent_jurisdiction_id text,iso_subdivision_code text,cldr_region_code text,name text);"
        "insert into jurisdiction(jurisdiction_id,jurisdiction_type,iso_country_code) values('GB','country','GB'),"
        "('ZA','country','ZA'),('RU','country','RU'),('AU','country','AU');"
        "insert into jurisdiction(jurisdiction_id,jurisdiction_type,iso_country_code,parent_jurisdiction_id) values"
        "('AU-ACT','subdivision','AU','AU'),('AU-NSW','subdivision','AU','AU'),"
        "('AU-NT','subdivision','AU','AU'),('AU-QLD','subdivision','AU','AU'),"
        "('AU-SA','subdivision','AU','AU'),('AU-TAS','subdivision','AU','AU'),"
        "('AU-VIC','subdivision','AU','AU'),('AU-WA','subdivision','AU','AU');"
        "create table jurisdiction_name(jurisdiction_id text primary key,name text not null);"
        "insert into jurisdiction_name select jurisdiction_id,coalesce(name,jurisdiction_id) from jurisdiction;"
        "create table jurisdiction_town_entity(jurisdiction_town_id integer primary key);"
        "create table jurisdiction_town_name(jurisdiction_town_id integer primary key,town_name);"
        "create table jurisdiction_town_jurisdiction_id(jurisdiction_town_id integer primary key,jurisdiction_id);"
        "create view jurisdiction_town as select jurisdiction_town_id,jurisdiction_id,town_name "
        "from jurisdiction_town_name join jurisdiction_town_jurisdiction_id using(jurisdiction_town_id);"
        "create trigger jurisdiction_town_insert instead of insert on jurisdiction_town begin "
        "insert into jurisdiction_town_name values(new.jurisdiction_town_id,new.town_name);"
        "insert into jurisdiction_town_jurisdiction_id values(last_insert_rowid(),new.jurisdiction_id);end;"
        "create table jurisdiction_town_latitude(jurisdiction_town_id integer primary key,latitude);"
        "create table jurisdiction_town_longitude(jurisdiction_town_id integer primary key,longitude);"
        "create table jurisdiction_town_elevation(jurisdiction_town_id integer primary key,elevation_metres);"
        "create table jurisdiction_town_timezone(jurisdiction_town_id integer primary key,timezone_code);"
        "create table timezone_code(timezone_name,timezone_code);"
        "create table timezone_definition(timezone_name text primary key);"
        "create table timezone_canonical(timezone_name text primary key,canonical_timezone_name);"
        "create table timezone_era(timezone_name,sequence_no,gmtoff_minutes,rules_kind,format_text);"
        "insert into timezone_code values('Africa/Johannesburg',24);"
        "insert into timezone_code values('Europe/Kaliningrad',246),('Europe/Moscow',257);"
        "insert into timezone_code values('Australia/Adelaide',221),('Australia/Brisbane',222),"
        "('Australia/Darwin',223),('Australia/Hobart',224),('Australia/Melbourne',225),"
        "('Australia/Perth',226),('Australia/Sydney',227);"
        "insert into jurisdiction_town values(101,'RU','Kaliningrad'),(102,'RU','Moscow');"
        "insert into jurisdiction_town values(103,'AU','Armidale');"
        "insert into jurisdiction_town_latitude values(103,'-30.5');"
        "insert into jurisdiction_town_longitude values(103,'151.6');"
        "insert into jurisdiction_town_elevation values(103,'999');"
        "insert into jurisdiction_town_timezone values(103,227);"
        "insert into jurisdiction_town_latitude values(101,'54.70639'),(102,'55.7522');"
        "insert into jurisdiction_town_longitude values(101,'20.51102'),(102,'37.6156');"
        "insert into jurisdiction_town_timezone values(101,246),(102,257);"
        "create table jurisdiction_default_town(jurisdiction_id text primary key,jurisdiction_town_id);"
        "insert into jurisdiction_default_town values('AU',103);"
        "create table jurisdiction_location_default(jurisdiction_id text primary key);"
        "create table jurisdiction_location_default_latitude(jurisdiction_id text primary key,latitude);"
        "create table jurisdiction_location_default_longitude(jurisdiction_id text primary key,longitude);"
        "create table jurisdiction_location_default_timezone(jurisdiction_id text primary key,timezone_code);"
        "create table jurisdiction_location_default_locality(jurisdiction_id text primary key,locality_name);"
        "create table retained(value);insert into retained values(42)");
    sqlite_close(db);
    file_t *config_directory = test_cfg_database_file(directory, "config");
    string_t *settings = string_sprintf("{\"MARS_JURISDICTION_DB_PATH\":\"%s\","
                                         "\"MARS_JURISDICTION_DB_KEY\":\"saved-update-key\"}", file_path(database));
    ok = ok && config_directory && file_create_directory(config_directory, 0700, true) && settings &&
         mars_settings_sync(settings) &&
         cfg_databases_update() == 1; /* Missing migration source must not stamp success. */
    db = test_cfg_database_open(database, "saved-update-key");
    ok = ok && db && test_cfg_database_query(db,
        "select count(*) from sqlite_master where name='mars_database_revision'", "0") &&
        test_cfg_database_query(db, "select value from retained", "42") &&
        sqlite_exec_cstr(db, "alter table jurisdiction rename to saved_jurisdiction");
    sqlite_close(db);
    ok = ok && !setenv("MARS_ROOT", MARS_CONFIG_ROOT_DIR, 1) && cfg_databases_update() == 1;
    db = test_cfg_database_open(database, "saved-update-key");
    ok = ok && db && test_cfg_database_query(db,
        "select count(*) from sqlite_master where name in ('mars_database_revision','jurisdiction_country_flag',"
        "'jurisdiction_country_flag_source')", "0") &&
        sqlite_exec_cstr(db, "alter table saved_jurisdiction rename to jurisdiction") &&
        cfg_database_revision(db, "jurisdiction", 3);
    sqlite_close(db);
    ok = ok && cfg_databases_update() == 0 &&
         !setenv("MARS_ROOT", directory, 1) && cfg_databases_update() == 0; /* No source access on a current DB. */
    db = test_cfg_database_open(database, "saved-update-key");
    ok = ok && db && test_cfg_database_query(db, "select revision from mars_database_revision", "21") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_area_search WHERE jurisdiction_area_search MATCH '\"GB\"*'", "1") &&
         test_cfg_database_query(db,
             "SELECT count(*) FROM jurisdiction_town_search WHERE jurisdiction_town_search MATCH '\"Kaliningrad\"*'",
             "1") &&
         test_cfg_database_query(db, "select count(*) from jurisdiction_country_flag", "30") &&
         test_cfg_database_query(db, "select jurisdiction_id from jurisdiction_town where town_name='Kaliningrad'", "RU-KGD") &&
         test_cfg_database_query(db, "select locality_name from jurisdiction_location_default_locality where jurisdiction_id='RU'", "Moscow") &&
         test_cfg_database_query(db, "select count(*) from jurisdiction_town where jurisdiction_id='ZA' "
                                   "and town_name in ('Robben Island','Seal Island','Duiker Island')", "3") &&
         test_cfg_database_query(db, "select count(*) from jurisdiction_town where jurisdiction_id='AU'", "261") &&
         test_cfg_database_query(db, "select count(distinct town_name) from jurisdiction_town "
                                   "where substr(jurisdiction_id,1,2)='AU'", "277") &&
         test_cfg_database_query(db,
             "select latitude||'|'||longitude||'|'||elevation_metres from jurisdiction_town_latitude "
             "join jurisdiction_town_longitude using(jurisdiction_town_id) "
             "join jurisdiction_town_elevation using(jurisdiction_town_id) where jurisdiction_town_id=103",
             "-30.5|151.6|999") &&
         test_cfg_database_query(db, "select jurisdiction_town_id from jurisdiction_default_town "
                                   "where jurisdiction_id='AU'", "103") &&
         test_cfg_database_query(db, "select count(*) from jurisdiction_town where jurisdiction_id='AU' "
                                   "and town_name in ('Byron Bay','Broome','Yamba')", "3") &&
         test_cfg_database_query(db, "select count(*) from jurisdiction_town_search "
                                   "where jurisdiction_town_search match '\"Perth\"*'", "2") &&
         test_cfg_database_query(db, "select count(*) from jurisdiction_town_search "
                                   "where jurisdiction_town_search match '\"Pretoria\"*'", "1") &&
         test_cfg_database_query(db, "select count(*) from jurisdiction where jurisdiction_id='ZZ-SEA'", "0") &&
         test_cfg_database_query(db, "select value from retained", "42") &&
         cfg_database_revision(db, "jurisdiction", 17);
    sqlite_close(db);
    ok = ok && !setenv("MARS_ROOT", MARS_CONFIG_ROOT_DIR, 1) && cfg_databases_update() == 0;
    db = test_cfg_database_open(database, "saved-update-key");
    ok = ok && db && test_cfg_database_query(db, "select revision from mars_database_revision", "21") &&
         test_cfg_database_query(db, "select count(*) from jurisdiction_town where jurisdiction_id='AU'", "261") &&
         test_cfg_database_query(db, "select count(distinct town_name) from jurisdiction_town "
                                   "where substr(jurisdiction_id,1,2)='AU'", "277") &&
         test_cfg_database_query(db, "select count(*) from jurisdiction_town where town_name='Kaliningrad'", "1") &&
         test_cfg_database_query(db, "select jurisdiction_town_id from jurisdiction_default_town "
                                   "where jurisdiction_id='AU'", "103") &&
         sqlite_exec_cstr(db,
             "INSERT INTO jurisdiction(jurisdiction_id,jurisdiction_type,iso_country_code,name) "
             "VALUES('IT','country','IT','Italy'),('FR','country','FR','France'),"
             "('BV','country','BV','Bouvet Island'),('HM','country','HM','Heard Island & McDonald Islands');"
             "INSERT INTO jurisdiction_name VALUES('IT','Italy'),('FR','France'),"
             "('BV','Bouvet Island'),('HM','Heard Island & McDonald Islands');"
             "INSERT INTO jurisdiction_town_entity VALUES(1048576),(1048577);"
             "INSERT INTO jurisdiction_town VALUES(1048576,'IT','Cagliari'),(1048577,'FR','Ajaccio');"
             "INSERT INTO jurisdiction_town_latitude VALUES(1048576,'39.23054'),(1048577,'41.91886');"
             "INSERT INTO jurisdiction_town_longitude VALUES(1048576,'9.11917'),(1048577,'8.73812');"
             "INSERT INTO jurisdiction_town_elevation VALUES(1048576,'41'),(1048577,'12');"
             "INSERT INTO timezone_code VALUES('Europe/Rome',263),('Europe/Paris',259),('Indian/Kerguelen',287);"
             "INSERT INTO jurisdiction_town_timezone VALUES(1048576,263),(1048577,259);") &&
         cfg_database_revision(db, "jurisdiction", 18);
    sqlite_close(db);
    ok = ok && cfg_databases_update() == 0;
    db = test_cfg_database_open(database, "saved-update-key");
    ok = ok && db && test_cfg_database_query(db, "select revision from mars_database_revision", "21") &&
         test_cfg_database_query(db, "SELECT count(*) FROM jurisdiction_place_alias "
             "WHERE name IN ('Sardinia','Sardegna','Corsica','Corse')", "4") &&
         test_cfg_database_query(db, "SELECT count(*) FROM jurisdiction_place_search "
             "WHERE jurisdiction_place_search MATCH '\"Sardinia\"*'", "1") &&
         test_cfg_database_query(db, "SELECT count(*) FROM jurisdiction_town WHERE jurisdiction_id='IT'", "1") &&
         test_cfg_database_query(db, "SELECT count(*) FROM jurisdiction_town WHERE jurisdiction_id='FR'", "1") &&
         test_cfg_database_query(db, "SELECT elevation_metres FROM jurisdiction_town_elevation "
             "WHERE jurisdiction_town_id=1048576", "41") &&
         test_cfg_database_query(db, "SELECT jurisdiction_town_id FROM jurisdiction_default_town "
             "WHERE jurisdiction_id='AU'", "103") &&
         test_cfg_database_query(db, "SELECT value FROM retained", "42") &&
         test_cfg_database_query(db, "SELECT latitude||'|'||longitude FROM jurisdiction_location_default_latitude "
             "JOIN jurisdiction_location_default_longitude USING(jurisdiction_id) WHERE jurisdiction_id='HM'",
             "-53.01888867|73.39205128") &&
         test_cfg_database_query(db, "SELECT count(*) FROM jurisdiction_location_default_timezone "
             "WHERE jurisdiction_id IN ('BV','HM')", "2") &&
         test_cfg_database_query(db, "SELECT count(*) FROM timezone_era WHERE timezone_name='Etc/UTC' "
             "AND gmtoff_minutes=0 AND rules_kind='none'", "1") &&
         cfg_database_revision(db, "jurisdiction", 19);
    sqlite_close(db);
    ok = ok && cfg_databases_update() == 0;
    db = test_cfg_database_open(database, "saved-update-key");
    ok = ok && db && test_cfg_database_query(db, "select revision from mars_database_revision", "21") &&
         test_cfg_database_query(db, "SELECT count(*) FROM timezone_code WHERE timezone_name='Etc/UTC'", "1") &&
         cfg_database_revision(db, "jurisdiction", 20) && sqlite_exec_cstr(db,
             "delete from jurisdiction_flag_region where code='CY-NCY';"
             "delete from jurisdiction_country_flag where iso_country_code='CY-NCY'");
    sqlite_close(db);
    /* A missing flag package must leave the previous managed revision intact. */
    ok = ok && !setenv("MARS_ROOT", directory, 1) && cfg_databases_update() == 1;
    db = test_cfg_database_open(database, "saved-update-key");
    ok = ok && db && test_cfg_database_query(db, "select revision from mars_database_revision", "20");
    sqlite_close(db);
    ok = ok && !setenv("MARS_ROOT", MARS_CONFIG_ROOT_DIR, 1) && cfg_databases_update() == 0;
    db = test_cfg_database_open(database, "saved-update-key");
    ok = ok && db && test_cfg_database_query(db, "select revision from mars_database_revision", "21") &&
         test_cfg_database_query(db, "select parent_code from jurisdiction_flag_region where code='CY-NCY'", "CY") &&
         test_cfg_database_query(db,
             "select count(*) from jurisdiction_country_flag where iso_country_code in ('CY','CY-NCY')", "2") &&
         cfg_database_revision(db, "jurisdiction", 22);
    sqlite_close(db);
    ok = ok && cfg_databases_update() == 1 && !setenv("MARS_JURISDICTION_DB_KEY", "wrong-key", 1) &&
         cfg_databases_update() == 1;
    file_t *config = test_cfg_database_file(directory, "mars.db");
    string_t *after = mars_settings_read("MARS_JURISDICTION_DB_KEY");
    ok = ok && test_cfg_database_text(after, "saved-update-key");
    string_free(after); string_free(settings); file_free(config); file_free(config_directory); file_free(database);
    return ok;
}

static void test_cfg_database_update(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_update_fixture),
                     "migration-only updates preserve keys and unrelated rows, skip current/missing stores and fail safely");
}

static bool test_cfg_database_ensure_fixture(const char *directory)
{
    static const char *const paths[] = {
        "globe/globe.db", "custom almanac.db", "jurisdiction/mars_jurisdiction_rules.db"
    };
    static const char *const configurations[] = {
        "mars.db", "mars.db", "mars.db"
    };
    static const char *const keys[] = {"", "", ""};
    file_t *almanac = test_cfg_database_file(directory, paths[1]);
    bool ok = test_cfg_database_texture_fixture() && almanac &&
              !unsetenv("MARS_GLOBE_DB_PATH") && !unsetenv("MARS_GLOBE_DB_KEY") &&
              !unsetenv("MARS_HOLIDAY_JURISDICTION") &&
              !unsetenv("MARS_CALENDAR_LOCATION_ARGUMENT") && !unsetenv("MARS_CALENDAR_LANGUAGE_ARGUMENT") &&
              !setenv("LANG", "en_GB.UTF-8", 1) && !setenv("LC_ALL", "en_GB.UTF-8", 1) &&
              !setenv("MARS_ALMANAC_DB_PATH", file_path(almanac), 1) &&
              !setenv("MARS_ALMANAC_DB_KEY", keys[1], 1) && !setenv("MARS_ROOT", directory, 1) &&
              cfg_databases_ensure() == 1;
    for (size_t i = 0; ok && i < 3; ++i) {
        file_t *file = test_cfg_database_file(directory, paths[i]);
        ok = file && !file_exists(file);
        file_free(file);
    }
    /* Exercise a completely absent installation through the same native operation as release. */
    ok = ok && !setenv("MARS_ROOT", MARS_CONFIG_ROOT_DIR, 1) && cfg_databases_ensure() == 0 &&
         !unsetenv("MARS_ALMANAC_DB_PATH") && !unsetenv("MARS_ALMANAC_DB_KEY") && cfg_databases_ensure() == 0;
    string_t *saved[3] = {0};
    for (size_t i = 0; ok && i < 3; ++i) {
        file_t *file = test_cfg_database_file(directory, paths[i]);
        file_t *config = test_cfg_database_file(directory, configurations[i]);
        sqlite_t *db = file ? test_cfg_database_open(file, keys[i]) : NULL;
        saved[i] = config ? file_sha256(config) : NULL;
        ok = db && saved[i] && sqlite_exec_cstr(db,
            "create table release_retained(value);insert into release_retained values(42)");
        sqlite_close(db); file_free(config); file_free(file);
    }
    /* Current databases must not need package files or lose custom data on repeated releases. */
    ok = ok && !setenv("MARS_ROOT", directory, 1) && cfg_databases_ensure() == 0 &&
         test_cfg_database_write(directory, "mars_lab_history.db", "history fixture") &&
         test_cfg_database_write(directory, "river-checkpoint.db", "river fixture") && file_delete(almanac) &&
         cfg_databases_ensure() == 1 && !file_exists(almanac) &&
         test_cfg_database_fixture(directory, "create table restored(value);insert into restored values(7);") &&
         cfg_databases_ensure() == 0;
    for (size_t i = 0; ok && i < 3; ++i) {
        file_t *file = test_cfg_database_file(directory, paths[i]);
        file_t *config = test_cfg_database_file(directory, configurations[i]);
        string_t *after = config ? file_sha256(config) : NULL;
        sqlite_t *db = file ? test_cfg_database_open(file, keys[i]) : NULL;
        ok = db && after && !string_compare(saved[i], after) &&
             test_cfg_database_query(db, i == 1 ? "select value from restored" : "select value from release_retained",
                                     i == 1 ? "7" : "42");
        sqlite_close(db); string_free(after); file_free(config); file_free(file);
    }
    /* Invalid credentials are an error, never a reason to reinstall an existing database. */
    file_t *snapshot = test_cfg_database_file(directory, "almanac-snapshot.db");
    ok = ok && snapshot && file_copy(almanac, snapshot, false) &&
         !setenv("MARS_ALMANAC_DB_KEY", "incorrect-key", 1) && cfg_databases_ensure() == 1 &&
         test_cfg_database_equal(almanac, snapshot);
    static const char *const retained[][2] = {
        {"mars_lab_history.db", "history fixture"}, {"river-checkpoint.db", "river fixture"}
    };
    for (size_t i = 0; ok && i < 2; ++i) {
        file_t *file = test_cfg_database_file(directory, retained[i][0]);
        string_t *value = file ? file_read_all_text(file) : NULL;
        ok = test_cfg_database_text(value, retained[i][1]);
        string_free(value); file_free(file);
    }
    for (size_t i = 0; i < 3; ++i) string_free(saved[i]);
    file_free(snapshot); file_free(almanac);
    return ok;
}

static void test_cfg_database_ensure(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_ensure_fixture),
                     "release restores missing stores, reuses saved paths and keys, and preserves existing data");
}

static bool test_cfg_database_globe_fixture(const char *directory)
{
    bool ok = !setenv("MARS_ROOT", directory, 1) && !setenv("MARS_GLOBE_DB_KEY", "fixture-only", 1) &&
              !unsetenv("MARS_GLOBE_DB_PATH");
    ok = ok && cfg_globe_run("globe-update") == 1 && cfg_globe_run("globe-uninstall") == 0 &&
         cfg_globe_run("globe-check") == 1 && cfg_globe_run("globe-install") == 1;
    ok = ok && !setenv("MARS_ALMANAC_DB_KEY", "shared-fixture", 1) && cfg_databases_run() == 1;
    file_t *almanac = test_cfg_database_file(directory, "almanac/almanac.db");
    file_t *jurisdiction = test_cfg_database_file(directory, "jurisdiction/mars_jurisdiction_rules.db");
    ok = ok && almanac && jurisdiction && !file_exists(almanac) && !file_exists(jurisdiction);
    file_free(almanac);
    file_free(jurisdiction);
    string_t *home = cfg_storage_home();
    string_t *path = home ? string_sprintf("%s/globe/globe.db", string_c_str(home)) : NULL;
    file_t *database = path ? file_new(path) : NULL;
    ok = ok && database && !file_exists(database);
    file_free(database);
    string_free(path);
    string_free(home);
    return ok;
}

static void test_cfg_database_globe(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_globe_fixture),
                     "globe setup fails safely without shoreline source");
}

static bool test_cfg_kashubian_coverage(globe_t *globe)
{
    static const struct { const char *name; double latitude, longitude; bool regional; } places[] = {
        {"Kartuzy", 54.334, 18.197, true},
        {"Kościerzyna", 54.122, 17.981, true},
        {"Bytów", 54.170, 17.491, true},
        {"Puck", 54.718, 18.409, true},
        {"Hel", 54.608, 18.801, true},
        {"Wejherowo", 54.605, 18.235, true},
        {"Gdańsk", 54.352, 18.646, true},
        {"Gdynia", 54.519, 18.531, true},
        {"Sopot", 54.441, 18.560, true},
        {"Słupsk", 54.464, 17.028, true},
        {"Łeba", 54.760, 17.555, true},
        {"Chojnice", 53.695, 17.558, true},
        {"Koczała", 53.904, 17.066, true},
        {"Przywidz", 54.195, 18.321, true},
        {"Kolbudy", 54.270, 18.465, true},
        {"Tczew", 54.092, 18.778, false},
        {"Starogard Gdański", 53.963, 18.526, false},
        {"Malbork", 54.036, 19.038, false},
        {"Pruszcz Gdański", 54.262, 18.636, false},
        {"Koszalin", 54.194, 16.172, false},
        {"Warsaw", 52.230, 21.012, false},
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof places / sizeof *places; ++i) {
        string_t *code = globe_region_code_at(globe, places[i].latitude, places[i].longitude);
        /* National coverage is checked separately: its coarser outline omits the Hel peninsula. */
        bool valid = places[i].regional ? code && !strcmp(string_c_str(code), "PL-KAS") : !code;
        if (!valid) fprintf(stderr, "Kashubian coverage mismatch: %s (%s)\n", places[i].name,
                            code ? string_c_str(code) : "no regional flag");
        ok = valid && ok;
        string_free(code);
    }
    return ok;
}

static bool test_cfg_sami_coverage(globe_t *globe)
{
    static const struct { const char *name; double latitude, longitude; bool regional; } places[] = {
        {"Hetta, Enontekiö", 68.386, 23.638, true},
        {"Kilpisjärvi", 69.045, 20.790, true},
        {"Inari", 68.906, 27.028, true},
        {"Ivalo", 68.659, 27.540, true},
        {"Utsjoki", 69.908, 27.029, true},
        {"Vuotso", 68.086, 27.123, true},
        {"Sodankylä centre", 67.416, 26.590, false},
        {"Rovaniemi", 66.503, 25.729, false},
        {"Kittilä", 67.655, 24.914, false},
        {"Muonio", 67.958, 23.680, false},
        {"Kiruna, Sweden", 67.855, 20.225, false},
        {"Karasjok, Norway", 69.471, 25.511, false},
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof places / sizeof *places; ++i) {
        string_t *code = globe_region_code_at(globe, places[i].latitude, places[i].longitude);
        bool valid = places[i].regional ? code && !strcmp(string_c_str(code), "FI-SAP") : !code;
        if (places[i].regional) {
            string_t *country = globe_country_code_at(globe, places[i].latitude, places[i].longitude);
            valid = valid && country && !strcmp(string_c_str(country), "FI");
            string_free(country);
        }
        if (!valid) fprintf(stderr, "Sámi coverage mismatch: %s (%s)\n", places[i].name,
                            code ? string_c_str(code) : "no regional flag");
        ok = valid && ok;
        string_free(code);
    }
    return ok;
}

static bool test_cfg_german_region_coverage(globe_t *globe)
{
    static const struct { const char *name, *code; double latitude, longitude; } places[] = {
        {"Emden", "DE-OFR", 53.367, 7.207},
        {"Aurich", "DE-OFR", 53.470, 7.483},
        {"Husum", "DE-NFR", 54.478, 9.050},
        {"Westerland", "DE-NFR", 54.908, 8.308},
        {"Helgoland", "DE-NFR", 54.182, 7.889},
        {"Ramsloh", "DE-SAT", 53.104, 7.681},
        {"Scharrel", "DE-SAT", 53.073, 7.709},
        {"Bautzen", "DE-SOR", 51.181, 14.424},
        {"Crostwitz", "DE-SOR", 51.238, 14.243},
        {"Hoyerswerda", "DE-SOR", 51.437, 14.249},
        {"Schleife", "DE-SOR", 51.541, 14.532},
        {"Cottbus", "DE-SOR", 51.756, 14.334},
        {"Burg", "DE-SOR", 51.835, 14.148},
        {"Lübbenau", "DE-SOR", 51.869, 13.969},
        {"Cloppenburg", NULL, 52.847, 8.045},
        {"Pinneberg", NULL, 53.660, 9.800},
        {"Dresden", NULL, 51.051, 13.738},
        {"Görlitz", NULL, 51.152, 14.988},
        {"Berlin", NULL, 52.520, 13.405},
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof places / sizeof *places; ++i) {
        string_t *code = globe_region_code_at(globe, places[i].latitude, places[i].longitude);
        bool valid = places[i].code ? code && !strcmp(string_c_str(code), places[i].code) : !code;
        if (!valid) fprintf(stderr, "German regional coverage mismatch: %s (%s)\n", places[i].name,
                            code ? string_c_str(code) : "no regional flag");
        ok = valid && ok;
        string_free(code);
    }
    return ok;
}

static bool test_cfg_kazakh_coverage(globe_t *globe)
{
    static const struct { const char *name; double latitude, longitude; bool regional; } places[] = {
        {"Ölgii", 48.968, 89.962, true},
        {"Tsengel", 48.944, 89.145, true},
        {"Khovd city", 48.005, 91.641, true},
        {"Khovd soum", 48.200, 91.100, true},
        {"Buyant, Khovd", 48.060, 91.820, true},
        {"Nalaikh", 47.773, 107.253, true},
        {"Ulaangom", 49.982, 92.066, false},
        {"Central Ulaanbaatar", 47.918, 106.917, false},
        {"Jargalant, Töv", 48.450, 105.860, false},
        {"Altai city", 46.373, 96.258, false},
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof places / sizeof *places; ++i) {
        string_t *code = globe_region_code_at(globe, places[i].latitude, places[i].longitude);
        string_t *country = globe_country_code_at(globe, places[i].latitude, places[i].longitude);
        bool valid = (places[i].regional ? code && !strcmp(string_c_str(code), "MN-KAZ") : !code) &&
                     country && !strcmp(string_c_str(country), "MN");
        if (!valid) fprintf(stderr, "Mongolian Kazakh coverage mismatch: %s (%s)\n", places[i].name,
                            code ? string_c_str(code) : "no regional flag");
        ok = valid && ok;
        string_free(code); string_free(country);
    }
    return ok;
}

static bool test_cfg_cyprus_coverage(globe_t *globe)
{
    static const struct { double latitude, longitude; bool north; } places[] = {
        {35.3360, 33.3200, true}, {35.1250, 33.9400, true}, {35.2100, 32.9950, true},
        {35.1850, 33.3650, true}, {35.1750, 32.6200, true}, {35.1400, 33.3650, false},
        {34.6750, 33.0450, false},
        {34.9170, 33.6300, false}, {34.7650, 32.4200, false}
    };
    bool ok = globe != NULL;
    for (size_t i = 0; i < sizeof places / sizeof *places; ++i) {
        string_t *region = globe_region_code_at(globe, places[i].latitude, places[i].longitude);
        string_t *country = globe_country_code_at(globe, places[i].latitude, places[i].longitude);
        bool matches = country && !strcmp(string_c_str(country), "CY") &&
                       (places[i].north ? region && !strcmp(string_c_str(region), "CY-NCY") : !region);
        if (!matches) fprintf(stderr, "Cyprus %.4f %.4f: country=%s region=%s\n",
            places[i].latitude, places[i].longitude, country ? string_c_str(country) : "none",
            region ? string_c_str(region) : "none");
        ok = matches && ok;
        string_free(region); string_free(country);
    }
    return ok;
}

static bool test_cfg_country_migration_mode(const char *directory, const char *key)
{
    /* Normal release initialises the central settings store before preparing content databases. */
    string_t *settings = string_new_with("{}");
    bool configured = settings && mars_settings_sync(settings);
    string_free(settings);
    file_t *database = test_cfg_database_file(directory, "country_migration.db");
    sqlite_t *db = database ? test_cfg_database_open(database, key) : NULL;
    bool ok = configured && db && sqlite_exec_cstr(db,
        "create table shoreline_dataset(id,format_version,point_count);"
        "insert into shoreline_dataset values(1,2,7);"
        "create table shoreline_blob(id,vertices);"
        "create table shoreline_chunk(id,level,codec,point_count,byte_offset,byte_count,boundary,blob_id);"
        "create virtual table shoreline_bounds using rtree(id,min_x,max_x,min_y,max_y);"
        "create table rivers_dataset(id,source);insert into rivers_dataset values(1,'retained');"
        "create table country_name(id,name,iso_country_code);insert into country_name values(1,'old','RU');") &&
        cfg_database_revision(db, "globe", 3);
    sqlite_close(db);
    ok = ok && !setenv("MARS_GLOBE_DB_PATH", file_path(database), 1) && !setenv("MARS_GLOBE_DB_KEY", key, 1) &&
         !setenv("MARS_ROOT", directory, 1) && cfg_databases_update() == 1;
    db = ok ? test_cfg_database_open(database, key) : NULL;
    ok = ok && db && test_cfg_database_query(db, "select revision from mars_database_revision", "3") &&
         test_cfg_database_query(db, "select name from country_name", "old");
    sqlite_close(db);
    file_t *folder = test_cfg_database_file(directory, "packaging/globe-db");
    ok = ok && folder && file_create_directory(folder, 0700, true);
    file_free(folder);
    const char *extensions[] = {"shp", "dbf"};
    for (unsigned i = 0; ok && i < 2; ++i) {
        string_t *relative = string_sprintf("packaging/globe-db/map-units-5.1.1.%s.zst", extensions[i]);
        file_t *source = relative ? test_cfg_database_file(MARS_CONFIG_ROOT_DIR, string_c_str(relative)) : NULL;
        file_t *target = relative ? test_cfg_database_file(directory, string_c_str(relative)) : NULL;
        ok = source && target && file_copy(source, target, false);
        file_free(source); file_free(target); string_free(relative);
    }
    file_t *region_source = test_cfg_database_file(MARS_CONFIG_ROOT_DIR, "packaging/globe-db/flag_regions.geojson");
    file_t *region_target = test_cfg_database_file(directory, "packaging/globe-db/flag_regions.geojson");
    ok = ok && region_source && region_target && file_copy(region_source, region_target, false);
    file_free(region_source); file_free(region_target);
    file_t *subdivision_source = test_cfg_database_file(MARS_CONFIG_ROOT_DIR,
        "packaging/globe-db/jurisdiction_subdivisions.sql");
    file_t *subdivision_target = test_cfg_database_file(directory, "packaging/globe-db/jurisdiction_subdivisions.sql");
    ok = ok && subdivision_source && subdivision_target && file_copy(subdivision_source, subdivision_target, false);
    file_free(subdivision_source); file_free(subdivision_target);
    file_t *water_source = test_cfg_database_file(MARS_CONFIG_ROOT_DIR, "packaging/globe-db/mapped_water.sql");
    file_t *water_target = test_cfg_database_file(directory, "packaging/globe-db/mapped_water.sql");
    ok = ok && water_source && water_target && file_copy(water_source, water_target, false);
    file_free(water_source); file_free(water_target);
    /* No shoreline archive exists here: update countries, display regions and images independently. */
    ok = ok && test_cfg_copy_textures(directory) && !cfg_databases_update();
    db = ok ? test_cfg_database_open(database, key) : NULL;
    globe_t *globe = db ? globe_open_databases(db, db, NULL) : NULL;
    string_t *code = globe_country_code_at(globe, 44.95, 34.1);
    ok = ok && code && !strcmp(string_c_str(code), "UA") &&
         test_cfg_database_query(db, "select revision from mars_database_revision", "19") &&
         test_cfg_database_query(db, "select count(distinct iso_country_code) from region_name", "20") &&
         test_cfg_database_query(db, "select point_count from shoreline_dataset", "7") &&
         test_cfg_database_query(db, "select source from rivers_dataset", "retained") &&
         test_cfg_database_query(db, "select count(*) from country_name", "299");
    string_t *kaliningrad = globe_country_code_at(globe, 54.634833, 21.3865);
    string_t *poland = globe_country_code_at(globe, 54.35, 18.65);
    string_t *lithuania = globe_country_code_at(globe, 55.7, 21.15);
    string_t *moscow = globe_country_code_at(globe, 55.75, 37.62);
    ok = ok && kaliningrad && !strcmp(string_c_str(kaliningrad), "RU-KGD") &&
         poland && !strcmp(string_c_str(poland), "PL") && lithuania && !strcmp(string_c_str(lithuania), "LT") &&
         moscow && !strcmp(string_c_str(moscow), "RU");
    string_free(kaliningrad); string_free(poland); string_free(lithuania); string_free(moscow);
    ok = ok && test_cfg_kashubian_coverage(globe) && test_cfg_sami_coverage(globe) &&
         test_cfg_german_region_coverage(globe) && test_cfg_kazakh_coverage(globe) && test_cfg_cyprus_coverage(globe);
    string_free(code); globe_close(globe); sqlite_close(db);
    /* Geometry and historical revision variants are storage-independent and run once in plaintext below.
     * The encrypted path has already checked failed migration, preserved layers and the complete country import. */
    if (*key) {
        string_t *missing = string_sprintf("%s/missing", directory);
        ok = ok && missing && !setenv("MARS_ROOT", string_c_str(missing), 1) && !cfg_databases_update();
        string_free(missing); file_free(database);
        return ok;
    }
    /* Revision six retains geometry while refreshing countries and adding regional photographs. */
    db = ok ? test_cfg_database_open(database, key) : NULL;
    ok = ok && db && cfg_database_revision(db, "globe", 6) && sqlite_exec_cstr(db,
        "update country_name set iso_country_code='' where iso_country_code='ZA'");
    sqlite_close(db);
    file_t *manifest = test_cfg_database_file(directory, "packaging/globe-db/textures/manifest.json");
    ok = ok && manifest && !cfg_databases_update();
    file_free(manifest);
    db = ok ? test_cfg_database_open(database, key) : NULL;
    globe = db ? globe_open_databases(db, db, NULL) : NULL;
    code = globe_country_code_at(globe, -33.8064, 18.3688);
    ok = ok && code && !strcmp(string_c_str(code), "ZA") &&
         test_cfg_database_query(db, "select revision from mars_database_revision", "19") &&
         test_cfg_database_query(db, "select source from rivers_dataset", "retained");
    string_free(code); globe_close(globe); sqlite_close(db);
    /* Revision eight adds display coverage and regional photographs, retaining installed countries. */
    db = ok ? test_cfg_database_open(database, key) : NULL;
    ok = ok && db && cfg_database_revision(db, "globe", 8) &&
         sqlite_exec_cstr(db, "delete from region_name where iso_country_code in "
                             "('PL-KAS','FI-SAP','DE-SAT','DE-SOR','MN-KAZ')");
    sqlite_close(db);
    ok = ok && !cfg_databases_update();
    db = ok ? test_cfg_database_open(database, key) : NULL;
    globe = db ? globe_open_databases(db, db, NULL) : NULL;
    ok = ok && test_cfg_kashubian_coverage(globe) && test_cfg_sami_coverage(globe) &&
         test_cfg_german_region_coverage(globe) && test_cfg_kazakh_coverage(globe) &&
         test_cfg_database_query(db, "select revision from mars_database_revision", "19");
    globe_close(globe); sqlite_close(db);
    /* Revision seventeen needs flags and country metadata only, without replacing photographs. */
    db = ok ? test_cfg_database_open(database, key) : NULL;
    ok = ok && db && cfg_database_revision(db, "globe", 17) && sqlite_exec_cstr(db,
        "delete from region_name where iso_country_code='CY-NCY';"
        "update country_name set iso_country_code='' where name='Turkish Republic of Northern Cyprus';"
        "create trigger retain_photographs before delete on globe_texture begin "
        "select raise(abort,'flag migration must retain photographs');end");
    sqlite_close(db);
    ok = ok && !cfg_databases_update();
    db = ok ? test_cfg_database_open(database, key) : NULL;
    globe = db ? globe_open_databases(db, db, NULL) : NULL;
    ok = ok && test_cfg_cyprus_coverage(globe) &&
         test_cfg_database_query(db, "select revision from mars_database_revision", "19") &&
         test_cfg_database_query(db, "select point_count from shoreline_dataset", "7") &&
         test_cfg_database_query(db, "select source from rivers_dataset", "retained") &&
         sqlite_exec_cstr(db, "drop trigger retain_photographs");
    globe_close(globe); sqlite_close(db);
    /* A current database must remain usable even when the migration sources are no longer available. */
    string_t *missing = string_sprintf("%s/missing", directory);
    ok = ok && missing && !setenv("MARS_ROOT", string_c_str(missing), 1) && !cfg_databases_update();
    string_free(missing); file_free(database);
    return ok;
}

static bool test_cfg_country_migration_plain(const char *directory)
{
    return test_cfg_country_migration_mode(directory, "");
}

static bool test_cfg_country_migration_encrypted(const char *directory)
{
    return test_cfg_country_migration_mode(directory, "country-fixture-key");
}

static void test_cfg_country_migration(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_country_migration_plain),
                     "plaintext country-only migration and rollback");
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_country_migration_encrypted),
                     "encrypted country-only migration and rollback");
}

typedef struct {
    double x, y, first_x, first_y, last_x, last_y;
    bool inside, land;
} cfg_coast_probe_t;

static bool test_cfg_coast_path(globe_path_event_t event, double x, double y, unsigned level,
                                bool antarctic, void *context)
{
    (void)antarctic;
    cfg_coast_probe_t *p = context;
    if (event == GLOBE_PATH_MOVE) {
        p->first_x = p->last_x = x;
        p->first_y = p->last_y = y;
        p->inside = false;
        return true;
    }
    if (event == GLOBE_PATH_CLOSE) { x = p->first_x; y = p->first_y; }
    if ((p->last_y > p->y) != (y > p->y) &&
        p->x < p->last_x + (p->y - p->last_y) * (x - p->last_x) / (y - p->last_y)) p->inside = !p->inside;
    p->last_x = x; p->last_y = y;
    if (event == GLOBE_PATH_CLOSE && p->inside) p->land = level % 2;
    return true;
}

static bool test_cfg_coast_sydney(sqlite_t *db)
{
    globe_t *globe = globe_open(db, NULL);
    bool ok = globe && globe_hit_test(globe, -33.8678, 151.2073) == GLOBE_LAND;
    const struct { double lat, lon; bool land; } sites[] = {
        {-33.8678, 151.2073, true},  /* Catalogue Sydney, unchanged. */
        {-33.8688, 151.2093, true},  /* Default NSW observer, unchanged. */
        {-33.8700, 151.1920, true},  /* Pyrmont. */
        {-33.8610, 151.2020, true},  /* Barangaroo. */
        {-33.8665, 151.2010, false}, /* Darling Harbour. */
        {-33.8550, 151.2130, false}, /* Circular Quay. */
        {-33.8630, 151.2190, false}  /* Woolloomooloo Bay. */
    };
    for (size_t i = 0; ok && i < sizeof sites / sizeof *sites; ++i) {
        for (unsigned zoom = 0; ok && zoom < 2; ++zoom) {
            double span = zoom ? .03 : .12;
            cfg_coast_probe_t p = {.x = sites[i].lon, .y = sites[i].lat};
            ok = globe_surface_paths(globe, p.x - span, p.y - span, p.x + span, p.y + span,
                                      span / 1024, test_cfg_coast_path, &p) && p.land == sites[i].land;
        }
    }
    ok = ok && globe_hit_test(globe, -33.8678, 151.2073) == GLOBE_LAND &&
         globe_hit_test(globe, 52.7077, -2.7541) == GLOBE_LAND;
    globe_close(globe);
    return ok;
}

static bool test_cfg_coast_correction_fixture(const char *directory)
{
    file_t *input = test_cfg_database_file(directory, "input.bin");
    file_t *output = test_cfg_database_file(directory, "output.bin");
    file_t *patch = test_cfg_database_file(directory, "corrections.json");
    const uint32_t words[] = {7, 5, 3841, 0, 10, 0, 10, 0, 0, 0, 0, 0, 0, 10, 0, 10, 10, 0, 10, 0, 0};
    unsigned char bytes[sizeof words];
    for (size_t i = 0; i < sizeof words / sizeof *words; ++i)
        for (unsigned j = 0; j < 4; ++j) bytes[i * 4 + j] = (unsigned char)(words[i] >> ((3 - j) * 8));
    bool ok = input && output && patch && file_write_all_bytes(input, bytes, sizeof bytes);
    const char *cases[] = {
        "{\"revision\":1,\"patches\":[{\"polygon\":7,\"source_points\":5,\"first\":1,\"last\":2,"
        "\"coordinates\":[[10,0],[8,5],[10,10]]}]}",
        "{\"revision\":1,\"patches\":[{\"polygon\":7,\"source_points\":5,\"first\":1,\"last\":2,"
        "\"coordinates\":[[9,0],[8,5],[10,10]]}]}",
        "{\"revision\":1,\"patches\":[{\"polygon\":7,\"source_points\":5,\"first\":1,\"last\":2,"
        "\"coordinates\":[[10,0],[12,5],[10,10]]}]}",
        "{\"revision\":1,\"patches\":[{\"polygon\":8,\"source_points\":5,\"first\":1,\"last\":2,"
        "\"coordinates\":[[10,0],[8,5],[10,10]]}]}",
        "{\"revision\":1,\"patches\":[{\"polygon\":7,\"source_points\":6,\"first\":1,\"last\":2,"
        "\"coordinates\":[[10,0],[8,5],[10,10]]}]}",
        "{\"revision\":2,\"patches\":[]}", "{broken"
    };
    for (size_t i = 0; ok && i < sizeof cases / sizeof *cases; ++i) {
        string_t *text = string_new_with(cases[i]);
        ok = text && file_write_all_text(patch, text) && cfg_database_coast_correct(input, output, patch) == (i == 0);
        string_free(text);
        if (ok && i == 0) {
            unsigned char result[sizeof bytes + 8];
            size_t count;
            ok = file_open_read(output) && file_read(output, result, sizeof result, &count) &&
                 count == sizeof result && result[7] == 6 && result[63] == 8 && result[67] == 5;
            file_close(output);
        }
        if (file_exists(output)) ok = file_delete(output) && ok;
    }
    unsigned char unchanged[sizeof bytes];
    size_t count;
    ok = ok && file_open_read(input) && file_read(input, unchanged, sizeof unchanged, &count) &&
         count == sizeof unchanged && !memcmp(bytes, unchanged, sizeof bytes);
    file_close(input);
    file_free(input); file_free(output); file_free(patch);
    return ok;
}

static void test_cfg_database_coast_corrections(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_coast_correction_fixture),
                     "bounded shoreline corrections preserve source and reject invalid spans and endpoints");
}

static bool test_cfg_database_globe_plain_fixture(const char *directory)
{
    const char *key = "";
    string_t *settings = string_new_with("{\"MARS_GLOBE_RIVER_DB_KEY\":\"separate-staging-key\"}");
    bool configured = settings && mars_settings_sync(settings);
    string_free(settings);
    file_t *folder = test_cfg_database_file(directory, "globe");
    file_t *database = test_cfg_database_file(directory, "globe/globe.db");
    bool ok = configured && folder && database && file_create_directory(folder, 0700, true);
    sqlite_t *db = ok ? test_cfg_database_open(database, key) : NULL;
    ok = ok && db && sqlite_exec_cstr(db,
        "CREATE TABLE retained(value); INSERT INTO retained VALUES(99);"
        "CREATE TABLE rivers_dataset(id,source); INSERT INTO rivers_dataset VALUES(1,'obsolete');");
    sqlite_close(db);
    /* A single full import replaces an old database, proving fresh installation without repeating the import. */
    ok = ok && test_cfg_database_texture_fixture() && !setenv("MARS_ROOT", MARS_CONFIG_ROOT_DIR, 1) &&
              !setenv("MARS_GLOBE_DB_KEY", "ignored-per-database-key", 1) && !unsetenv("MARS_GLOBE_DB_PATH") &&
              !cfg_globe_install_key(key) && !unsetenv("MARS_GLOBE_DB_KEY") && !cfg_globe_run("globe-check");
    db = ok ? test_cfg_database_open(database, key) : NULL;
    ok = ok && db && test_cfg_database_mode(database, 0600) &&
         test_cfg_database_query(db, "SELECT count(*) FROM country_name", "299") &&
         test_cfg_database_query(db, "SELECT point_count FROM shoreline_dataset", "35346818") &&
         test_cfg_database_query(db, "SELECT revision FROM mars_database_revision WHERE name='globe'", "19") &&
         test_cfg_database_query(db, "SELECT count(*) FROM sqlite_master WHERE name IN"
                                    "('almanac_body','jurisdiction_town','retained','rivers_dataset')", "0") &&
         test_cfg_coast_sydney(db) && sqlite_exec_cstr(db,
             "UPDATE mars_database_revision SET revision=17 WHERE name='globe';"
             "create table retained(value);insert into retained values(42);"
             "create table rivers_dataset(id,source);insert into rivers_dataset values(1,'retained fixture')");
    sqlite_close(db);
    ok = ok && !cfg_databases_update() && !cfg_databases_update();
    db = ok ? test_cfg_database_open(database, key) : NULL;
    ok = ok && db && test_cfg_coast_sydney(db) &&
         test_cfg_database_query(db, "select value from retained", "42") &&
         test_cfg_database_query(db, "select source from rivers_dataset", "retained fixture") &&
         test_cfg_database_query(db, "select point_count from shoreline_dataset", "35346818") &&
         test_cfg_database_query(db, "select revision from mars_database_revision where name='globe'", "19");
    sqlite_close(db);
    /* Country metadata is independent of town selection and land/water classification. */
    db = ok ? test_cfg_database_open(database, key) : NULL;
    globe_t *globe = db ? globe_open_databases(db, db, NULL) : NULL;
    string_t *country = globe_country_at(globe, 52.7077, -2.7541);
    string_t *sea_country = globe_country_at(globe, 0, -30);
    ok = ok && country && !strcmp(string_c_str(country), "England") && !sea_country;
    string_free(country); string_free(sea_country); globe_close(globe); sqlite_close(db);
    file_t *config = test_cfg_database_file(directory, "mars.db");
    /* Failed installation must preserve the installed database and all existing settings. */
    ok = ok && !setenv("MARS_ROOT", directory, 1) && cfg_globe_run("globe-update") == 1 &&
         cfg_globe_run("globe-install") == 1;
    db = ok ? test_cfg_database_open(database, key) : NULL;
    ok = ok && db && test_cfg_coast_sydney(db) &&
         test_cfg_database_query(db, "select value from retained", "42") &&
         test_cfg_database_query(db, "select source from rivers_dataset", "retained fixture") &&
         test_cfg_database_setting(config, "MARS_GLOBE_RIVER_DB_KEY", "separate-staging-key");
    sqlite_close(db);
    ok = ok && test_cfg_database_mode(database, 0600) &&
         test_cfg_database_setting(config, "MARS_GLOBE_DB_KEY", key) &&
         test_cfg_database_setting(config, "MARS_GLOBE_DB_PATH", file_path(database)) &&
         test_cfg_database_setting(config, "MARS_GLOBE_RIVER_DB_KEY", "separate-staging-key");
    file_free(config);
    file_free(folder);
    file_free(database);
    return ok;
}

static bool test_cfg_database_globe_packaged_fixture(const char *directory)
{
    /* Encryption and publication do not require importing the same 35-million-point coastline twice. */
    file_t *database = test_cfg_database_file(directory, "encrypted-globe.db");
    file_t *source = test_cfg_database_file(directory, "small-coast.bin");
    file_t *snapshot = test_cfg_database_file(directory, "saved-globe.db");
    file_t *configuration = test_cfg_database_file(directory, "mars.db");
    file_t *saved = test_cfg_database_file(directory, "saved-settings.db");
    const char *key = "packaged-globe-fixture";
    string_t *settings = string_sprintf("{\"MARS_GLOBE_DB_PATH\":\"%s\",\"MARS_GLOBE_DB_KEY\":\"%s\"}",
                                       file_path(database), key);
    bool ok = database && source && snapshot && configuration && saved && settings && mars_settings_sync(settings);
    sqlite_t *db = ok ? test_cfg_database_open(database, key) : NULL;
    /* A genuine five-vertex GSHHG ring, encoded in the public shoreline import format. */
    const int32_t fields[] = {1, 5, 15 * 256 + 1, 0, 1000000, 0, 1000000, 0, 0, 0, 0,
                             0, 0, 1000000, 0, 1000000, 1000000, 0, 1000000, 0, 0};
    unsigned char bytes[sizeof fields];
    for (size_t i = 0; i < sizeof fields / sizeof *fields; ++i) {
        uint32_t value = (uint32_t)fields[i];
        bytes[4 * i] = value >> 24;
        bytes[4 * i + 1] = value >> 16;
        bytes[4 * i + 2] = value >> 8;
        bytes[4 * i + 3] = value;
    }
    size_t written = 0;
    string_t *root = string_new_with(MARS_CONFIG_ROOT_DIR);
    ok = ok && db && root && test_cfg_database_texture_fixture() &&
         file_open(source, FILE_MODE_CREATE, FILE_ACCESS_WRITE) &&
         file_write(source, bytes, sizeof bytes, &written) && written == sizeof bytes && file_close(source) &&
         file_open_read(source) && globe_import_shoreline(db, source) &&
         cfg_database_textures(db, root) && cfg_database_revision(db, "globe", CFG_GLOBE_DATA_REVISION) &&
         sqlite_exec_cstr(db, "CREATE TABLE retained(value); INSERT INTO retained VALUES(42);");
    file_close(source);
    sqlite_close(db);
    ok = ok && file_chmod(database, 0600) && !setenv("MARS_ROOT", directory, 1) &&
         !unsetenv("MARS_GLOBE_DB_PATH") && !unsetenv("MARS_GLOBE_DB_KEY") &&
         !cfg_globe_run("globe-check") && !cfg_databases_update() &&
         file_copy(database, snapshot, false) && file_copy(configuration, saved, false) &&
         cfg_globe_run("globe-update") == 1 && cfg_globe_run("globe-install") == 1 &&
         test_cfg_database_equal(database, snapshot) &&
         test_cfg_database_equal(configuration, saved);
    db = ok ? test_cfg_database_open(database, key) : NULL;
    ok = ok && db && test_cfg_database_query(db, "SELECT value FROM retained", "42") &&
         test_cfg_database_query(db, "SELECT point_count FROM shoreline_dataset", "5") &&
         test_cfg_database_setting(configuration, "MARS_GLOBE_DB_KEY", key);
    sqlite_close(db);
    string_free(root);
    string_free(settings);
    file_free(saved);
    file_free(configuration);
    file_free(snapshot);
    file_free(source);
    file_free(database);
    return ok;
}

static void test_cfg_database_globe_packaged(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_globe_packaged_fixture),
                     "encrypted globe keys reopen a compact valid catalogue; updates and failed installs preserve data");
}

static void test_cfg_database_globe_plain(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_globe_plain_fixture),
                     "plaintext globe reinstallation builds afresh and removes obsolete river key settings");
}

static bool test_cfg_database_uninstall_fixture(const char *directory)
{
    file_t *folder = test_cfg_database_file(directory, "custom");
    bool ok = folder && file_create_directory(folder, 0700, true) &&
              test_cfg_database_write(directory, "custom/world.db", "content") &&
              test_cfg_database_write(directory, "custom/world.db-journal", "journal") &&
              test_cfg_database_write(directory, "mars_lab_history.db", "retained history");
    string_t *settings = string_sprintf("{\"MARS_GLOBE_DB_PATH\":\"%s/custom/world.db\"}", directory);
    ok = ok && settings && mars_settings_sync(settings);
    file_t *configuration = test_cfg_database_file(directory, "mars.db");
    file_t *snapshot = test_cfg_database_file(directory, "settings-before.db");
    ok = ok && configuration && snapshot && file_copy(configuration, snapshot, false) &&
         cfg_database_uninstall("unknown") == 2 &&
         !setenv("MARS_GLOBE_DB_PATH", file_path(configuration), 1) &&
         cfg_database_uninstall("globe") == 1 && test_cfg_database_equal(configuration, snapshot) &&
         !unsetenv("MARS_GLOBE_DB_PATH") && !cfg_database_uninstall("globe") &&
         !cfg_database_uninstall("globe") && test_cfg_database_equal(configuration, snapshot);
    file_t *data = test_cfg_database_file(directory, "custom/world.db");
    file_t *journal = test_cfg_database_file(directory, "custom/world.db-journal");
    file_t *history = test_cfg_database_file(directory, "mars_lab_history.db");
    string_t *text = history ? file_read_all_text(history) : NULL;
    ok = ok && data && !file_exists(data) && journal && !file_exists(journal) &&
         test_cfg_database_text(text, "retained history");
    string_free(text); string_free(settings);
    file_free(history); file_free(journal); file_free(data);
    file_free(snapshot); file_free(configuration); file_free(folder);
    return ok;
}

static void test_cfg_database_uninstall(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(test_cfg_database_uninstall_fixture),
                     "configured removal is idempotent and protects settings and history");
}

/* Register native database regressions before documented examples. */
void test_cfg_database_cases(void)
{
    /* Prepare in the parent so its exit cleanup also covers fixtures used by isolated children. */
    (void)test_cfg_texture_root();
    TEST_RUN_IN_GROUP(test_cfg_database_uninstall, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_almanac_plain, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_jurisdiction_plain, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_globe, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_country_migration, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_country_flags, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_update, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_ensure, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_coast_corrections, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_globe_packaged, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_globe_plain, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_trace, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_sardinia, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_stream, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_precedence, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_failure, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_bounds, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_large, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_conflicts, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_transaction_finish, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_public_ddl, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_paths, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_packaged_almanac, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_jurisdiction, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_rivers, tests, NULL);
}
