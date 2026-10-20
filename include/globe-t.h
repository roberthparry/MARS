/**
 * @file globe-t.h
 * @brief Unified geographic controller for shorelines, rivers, towns and regional map layers.
 *
 * The controller borrows independently selectable shoreline, map and town SQLite connections and owns layer readers.
 * Roads and buildings have explicit unavailable states until their regional dataset is implemented and installed.
 * No database migration, browser dependency or background network download is implied by opening a controller.
 * Classifies coordinates and streams clipped vector polygons using GSHHG rings in a borrowed SQLite
 * database. The importer stores uncompressed coordinate chunks in BLOB pages with an integer R-tree.
 * The opaque globe_t owns prepared queries and borrows its database connection. Applications may share these
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
#ifndef MARS_GLOBE_H
#define MARS_GLOBE_H

#include "file-t.h"
#include "sqlite-t.h"

/** @brief Opaque geographic controller, borrowing its database connections. */
typedef struct _globe_t globe_t;

/** @brief Events in a clipped shoreline polygon, with a nesting level supplied separately. */
typedef enum _globe_path_event_t {
    GLOBE_PATH_MOVE, /**< Begin a polygon at this longitude/latitude. */
    GLOBE_PATH_LINE, /**< Continue the current polygon. */
    GLOBE_PATH_CLOSE /**< Close the current polygon; coordinates are ignored. */
} globe_path_event_t;

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
typedef bool (*globe_path_fn)(globe_path_event_t event, double longitude, double latitude,
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
bool globe_surface_paths(globe_t *shoreline, double west, double south, double east, double north,
                     double tolerance, globe_path_fn visit, void *context);

/**
 * @brief Prepare resident exact coordinates and display resolutions, optionally before workers fork.
 * @param shoreline Borrowed reader; serialise calls on it.
 * @return True when the complete validated dataset is resident, false on missing/corrupt data or allocation failure.
 * @note File-backed readers share one immutable native cache, replaced when database or WAL metadata changes.
 * Anonymous databases retain a reader-local cache. The cache never contains an open database connection.
 */
bool globe_surface_prepare(globe_t *shoreline);

/** @brief Geographic classification, including an explicit uncertainty state. */
typedef enum _globe_surface_t {
    GLOBE_UNKNOWN = 0, /**< Invalid input, unavailable data or a read failure. */
    GLOBE_LAND,        /**< Land, including islands within inland water. */
    GLOBE_SEA,         /**< Ocean outside the mapped land polygons. */
    GLOBE_INLAND_WATER, /**< Mapped lake or river-lake. */
    GLOBE_COAST        /**< Within 500 metres of a mapped boundary. */
} globe_surface_t;

/**
 * @brief Open a geographic controller over installed layer datasets.
 * @param db Borrowed shoreline database, or NULL.
 * @param detail_db Borrowed jurisdiction database for towns and regional detail, or NULL.
 * @return Owned controller, or NULL if both connections are NULL or allocation fails.
 * @note Missing layers remain unavailable; query availability before rendering them.
 * @note The connection must outlive the reader. No schema or data are modified.
 */
globe_t *globe_open(sqlite_t *db, sqlite_t *detail_db);

/**
 * @brief Open independent geometry and town stores without migrating their contents.
 * @param shore_db Borrowed shoreline database; may be NULL or the same connection as map_db.
 * @param map_db Borrowed globe database containing river geometry; may be NULL.
 * @param town_db Borrowed jurisdiction database containing towns; may be NULL.
 * @return Owned controller, or NULL if all connections are NULL or allocation fails.
 * @note Connections must outlive the controller. Missing layers remain unavailable; no cross-store fallback occurs.
 * Pass the globe connection for both geometry arguments after shoreline migration, or keep the existing almanac
 * connection for shore_db during transition. Opening a controller never creates, copies or deletes database tables.
 */
globe_t *globe_open_databases(sqlite_t *shore_db, sqlite_t *map_db, sqlite_t *town_db);

/**
 * @brief Open the configured independent globe database, optionally borrowing a town catalogue.
 * @param town_db Borrowed jurisdiction connection, or NULL; must outlive the controller.
 * @return Owned controller, or NULL if configuration, the existing database or its key is unavailable.
 * Reads MARS_GLOBE_DB_PATH and MARS_GLOBE_DB_KEY before literal settings in MARS_HOME/config/globe-db.env.
 * MARS_HOME defaults to ~/.mars; the default database path is MARS_HOME/globe/globe.db.
 * The geometry connection is owned and closed by the controller. Opening never creates a missing database
 * and never falls back to geography in the almanac or jurisdiction database.
 */
globe_t *globe_open_configured(sqlite_t *town_db);

/**
 * @brief Release a geographic controller without closing its borrowed databases.
 * @param shoreline Owned reader, or NULL; invalid after this call.
 */
void globe_close(globe_t *shoreline);

/**
 * @brief Classify a coordinate using spatially selected chunks or resident exact coordinates.
 * @param shoreline Borrowed reader, or NULL for unavailable data.
 * @param latitude_degrees Finite latitude in [-90,90].
 * @param longitude_degrees Finite longitude in [-180,180].
 * @return Surface classification, or GLOBE_UNKNOWN on invalid input or unavailable data.
 */
globe_surface_t globe_hit_test(globe_t *shoreline, double latitude_degrees, double longitude_degrees);

/**
 * @brief Transactionally replace shoreline tables from a GSHHG 2.3.7 binary stream.
 * @param db Borrowed writable database with no active transaction; unrelated tables are retained.
 * @param source Borrowed open binary input positioned at its first polygon header.
 * @return True after publication; false rolls back the replacement without closing either resource.
 * @note Keeps levels 1–5, using the Antarctic ice front rather than the alternative grounding line.
 */
bool globe_import_shoreline(sqlite_t *db, file_t *source);

/** @brief Geographic layers; unavailable detail must never be inferred from another layer. */
typedef enum _globe_layer_t {
    GLOBE_LAYER_SHORELINE,
    GLOBE_LAYER_RIVERS,
    GLOBE_LAYER_TOWNS,
    GLOBE_LAYER_ROADS,
    GLOBE_LAYER_BUILDINGS
} globe_layer_t;

/** @brief Return whether a controller has a usable installed implementation for this layer. */
bool globe_has_layer(const globe_t *globe, globe_layer_t layer);

/** @brief Consume one clipped river segment; return false to cancel. Coordinates are longitude/latitude degrees. */
typedef bool (*globe_river_segment_fn)(int64_t id, unsigned kind, double ax, double ay, double bx, double by, void *context);

/** @brief Stream bounded river segments; false means unavailable, invalid, corrupt or cancelled. Discard partial output. */
bool globe_river_segments(globe_t *globe, double west, double south, double east, double north,
                          globe_river_segment_fn visit, void *context);

/**
 * @brief Stream river display segments using a maximum deviation in angular degrees, between zero and one.
 * @note Zero retains exact geometry. Shared nodes, feature and chunk endpoints remain fixed; source data are unchanged.
 * Invalid or unavailable data and cancelled queries return false; discard partial output.
 */
bool globe_river_display_segments(globe_t *globe, double west, double south, double east, double north,
                                  double tolerance, globe_river_segment_fn visit, void *context);

/** @brief Import a legacy river binary transactionally for format tests; withdrawn WDBII is not displayed by controllers. */
bool globe_import_rivers(sqlite_t *db, file_t *source);

/**
 * @brief Borrowed database location or town, valid only during its visitor callback.
 *
 * Coordinate text retains the database's precision. Default locations have elevation
 * "0" when no altitude is stored. Do not free or retain these pointers.
 */
typedef struct _globe_place_t {
    const char *jurisdiction_code; /**< Owning jurisdiction code. */
    const char *name;              /**< Locality or town name. */
    const char *latitude;          /**< Latitude in decimal degrees, as stored. */
    const char *longitude;         /**< Longitude in decimal degrees, as stored. */
    const char *elevation;         /**< Elevation in metres, or "0" when unspecified. */
    const char *timezone;          /**< IANA timezone name. */
    bool is_default;               /**< Whether this is the jurisdiction's selected default. */
} globe_place_t;


/** @brief Consume a borrowed town record; false stops the current page successfully. */
typedef bool (*globe_place_visit_fn)(const globe_place_t *place, void *context);

/**
 * @brief Read an ascending-ID town page without retaining the catalogue.
 * @param globe Borrowed controller with jurisdiction storage; do not re-enter or close it from the visitor.
 * @param after_id Last ID from the previous page, or zero initially.
 * @param limit Maximum returned records, from 1 to 1024.
 * @param nearby Restrict the page to five degrees either side of the centre, wrapping longitude.
 * @param latitude Finite centre latitude in [-90,90]; ignored for global pages.
 * @param longitude Finite centre longitude in [-180,180]; ignored for global pages.
 * @param visit Required callback; borrowed fields remain valid only during the call.
 * @param context Borrowed visitor state.
 * @param next_id Last visited ID, or zero for an empty page; valid only while the database is unchanged.
 * @return True on success or visitor stop, false on unavailable data, invalid input or query failure.
 */
bool globe_towns_batch(globe_t *globe, int64_t after_id, size_t limit, bool nearby, double latitude, double longitude,
                       globe_place_visit_fn visit, void *context, int64_t *next_id);

#endif
