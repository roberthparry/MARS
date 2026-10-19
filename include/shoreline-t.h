/**
 * @file shoreline-t.h
 * @brief Spatially indexed and optionally resident shorelines for geographic planning.
 *
 * Classifies coordinates and streams clipped vector polygons using GSHHG rings in a borrowed SQLite
 * database. The importer stores uncompressed coordinate chunks in BLOB pages with an integer R-tree.
 * The opaque shoreline_t owns prepared queries and borrows its database connection. Applications may share these
 * tables with other data, including the encrypted almanac catalogue. No astronomical
 * engine, browser or network service is required. Display queries lazily retain immutable exact coordinates,
 * a spatial tree and four display resolutions; callers can prepare these before native workers fork.
 *
 * Cold hit-tests use indexed chunks; resident hit-tests use exact cached coordinates, never display simplifications.
 * Older compressed chunks remain readable. The 500-metre coastal uncertainty
 * band is a planning policy, not a surveyed accuracy guarantee. These data are not nautical charts and cannot
 * establish safe navigation or public access to land. Callers own the database and input file; they must serialise
 * queries on an instance and close instances before replacing their dataset. Missing, malformed or unreadable
 * data produce an unknown result.
 */
#ifndef MARS_SHORELINE_H
#define MARS_SHORELINE_H

#include "file-t.h"
#include "sqlite-t.h"

/** @brief Opaque indexed shoreline reader, borrowing its database connection. */
typedef struct _shoreline_t shoreline_t;

/** @brief Events in a clipped shoreline polygon, with a nesting level supplied separately. */
typedef enum _shoreline_path_event_t {
    SHORELINE_PATH_MOVE, /**< Begin a polygon at this longitude/latitude. */
    SHORELINE_PATH_LINE, /**< Continue the current polygon. */
    SHORELINE_PATH_CLOSE /**< Close the current polygon; coordinates are ignored. */
} shoreline_path_event_t;

/**
 * @brief Consume one clipped vector-path event.
 * @param event Move, line or close instruction.
 * @param longitude Geographic longitude, unwrapped into the requested window.
 * @param latitude Geographic latitude.
 * @param level Nesting level 1..4; odd levels are land, even levels inland water.
 * @param antarctic True for the Antarctic ice-front polygon, including clipped portions of that polygon.
 * @param context Borrowed caller context.
 * @return True to continue, false to abort the query.
 */
typedef bool (*shoreline_path_fn)(shoreline_path_event_t event, double longitude, double latitude,
                                  unsigned level, bool antarctic, void *context);

/**
 * @brief Stream clipped vector polygons from a shared resident geometry cache.
 * @param shoreline Borrowed reader; serialise calls on it.
 * @param west Western longitude, between -360 and 180 degrees.
 * @param south Southern latitude, at least -90 degrees.
 * @param east Eastern longitude, greater than west by at most 360 degrees.
 * @param north Northern latitude, greater than south and at most 90 degrees.
 * @param tolerance Display simplification distance in degrees, between 0 and 1; zero retains all vertices.
 * @param visit Required synchronous visitor; streamed coordinates are borrowed values.
 * @param context Borrowed visitor context.
 * @return True on success, false for invalid input, corrupt data or visitor cancellation. Discard partial output.
 * @note Group polygons by level and fill levels 1..4 over water. This API does not apply the hit-test uncertainty band.
 * @note Lazily retains exact coordinates, a spatial index and four display resolutions in native memory.
 */
bool shoreline_paths(shoreline_t *shoreline, double west, double south, double east, double north,
                     double tolerance, shoreline_path_fn visit, void *context);

/**
 * @brief Prepare resident exact coordinates and display resolutions, optionally before workers fork.
 * @param shoreline Borrowed reader; serialise calls on it.
 * @return True when the complete validated dataset is resident, false on missing/corrupt data or allocation failure.
 * @note File-backed readers share one immutable native cache, replaced when database or WAL metadata changes.
 * Anonymous databases retain a reader-local cache. The cache never contains an open database connection.
 */
bool shoreline_prepare(shoreline_t *shoreline);

/** @brief Geographic classification, including an explicit uncertainty state. */
typedef enum _shoreline_surface_t {
    SHORELINE_UNKNOWN = 0, /**< Invalid input, unavailable data or a read failure. */
    SHORELINE_LAND,        /**< Land, including islands within inland water. */
    SHORELINE_SEA,         /**< Ocean outside the mapped land polygons. */
    SHORELINE_INLAND_WATER, /**< Mapped lake or river-lake. */
    SHORELINE_COAST        /**< Within 500 metres of a mapped boundary. */
} shoreline_surface_t;

/**
 * @brief Open a reader for an installed shoreline dataset.
 * @param db Borrowed database containing the shoreline tables; may be read-only.
 * @return Owned reader, or NULL for missing/unsupported data or allocation failure.
 * @note The connection must outlive the reader. No schema or data are modified.
 */
shoreline_t *shoreline_open(sqlite_t *db);

/**
 * @brief Release a shoreline reader without closing its borrowed database.
 * @param shoreline Owned reader, or NULL; invalid after this call.
 */
void shoreline_close(shoreline_t *shoreline);

/**
 * @brief Classify a coordinate using spatially selected chunks or resident exact coordinates.
 * @param shoreline Borrowed reader, or NULL for unavailable data.
 * @param latitude_degrees Finite latitude in [-90,90].
 * @param longitude_degrees Finite longitude in [-180,180].
 * @return Surface classification, or SHORELINE_UNKNOWN on invalid input or unavailable data.
 */
shoreline_surface_t shoreline_hit_test(shoreline_t *shoreline, double latitude_degrees, double longitude_degrees);

/**
 * @brief Transactionally replace shoreline tables from a GSHHG 2.3.7 binary stream.
 * @param db Borrowed writable database with no active transaction; unrelated tables are retained.
 * @param source Borrowed open binary input positioned at its first polygon header.
 * @return True after publication; false rolls back the replacement without closing either resource.
 * @note Keeps levels 1–5, using the Antarctic ice front rather than the alternative grounding line.
 */
bool shoreline_import(sqlite_t *db, file_t *source);

#endif
