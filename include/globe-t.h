/**
 * @file globe-t.h
 * @brief Unified geographic controller for shorelines, countries, rivers, towns and photographic surface layers.
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
 * A serially owned globe_reader_t session lazily retains a configured connection, revalidates
 * configuration and file metadata on acquisition, supplies opaque identities for derived-image caches,
 * and must itself be created after forking.
 *
 * Cold hit-tests use indexed chunks; resident hit-tests use exact cached coordinates, never display simplifications.
 * Older compressed chunks remain readable. The 500-metre coastal uncertainty
 * band is a planning policy, not a surveyed accuracy guarantee. Context queries may choose a wider proximity band
 * without altering polygons or ownership. These data are not nautical charts and cannot
 * establish safe navigation or public access to land. Callers own the database and input file; they must serialise
 * queries on an instance and close instances before replacing their dataset. Missing, malformed or unreadable
 * data produce an unknown result.
 * Optional administrative polygons supply selected-position country names through bounded indexed queries.
 * Their generalised boundaries never replace the detailed shoreline or establish legal jurisdiction.
 * Independent cultural display polygons supply regional flag identifiers, never country ownership or time zones.
 * Named-water lookup currently recognises the Caspian Sea through exact shoreline containment, independently
 * of country ownership; it is not a comprehensive water-name gazetteer.
 * Photographic textures are indexed JPEG blobs with dimensions and SHA-256 identities. Metadata reads do not
 * decode or load the image; applications can validate cached responses before requesting the bounded payload.
 */
#ifndef MARS_GLOBE_H
#define MARS_GLOBE_H

#include "file-t.h"
#include "sqlite-t.h"

/** @brief Opaque geographic controller, borrowing its database connections. */
typedef struct _globe_t globe_t;

/** @brief Validated image metadata; level 0 is the overview, 1 global detail and 2 the twelve-by-six tile grid. */
typedef struct _globe_texture_info_t {
    unsigned width; /**< JPEG width in pixels. */
    unsigned height; /**< JPEG height in pixels. */
    size_t byte_count; /**< Encoded JPEG size, at most 16 MiB. */
    char sha256[65]; /**< Lower-case hexadecimal content digest, including its terminating NUL. */
} globe_texture_info_t;

/** @brief Receive a regional JPEG slot and its west, south, east, north bounds; false stops enumeration. */
typedef bool (*globe_texture_region_fn)(unsigned slot, const double bounds[4], void *context);

/** @brief Visit installed regional photographs intersecting a non-wrapping window; JPEGs are 1800 or 3600 square. */
bool globe_texture_regions(globe_t *globe, const double bounds[4], globe_texture_region_fn callback, void *context);

/**
 * @brief Read indexed texture metadata without loading its JPEG payload.
 * @param globe Borrowed controller; calls must be serialised.
 * @param level Zero for the overview, one for global detail, two for NASA tiles, three for regional photographs.
 * @param column Zero for global images; 0..11 for NASA tiles; 0..63 regional slot.
 * @param row Zero for global and regional images; 0..5 north to south for NASA tiles.
 * @param info Required output, cleared on failure.
 * @return True for installed, structurally valid metadata; false for missing data or invalid arguments.
 */
bool globe_texture_metadata(globe_t *globe, unsigned level, unsigned column, unsigned row, globe_texture_info_t *info);

/**
 * @brief Copy and checksum-verify one installed JPEG matching previously read metadata.
 * @param globe Borrowed controller; calls must be serialised.
 * @param level Texture level, as for globe_texture_metadata.
 * @param column Texture column, as for globe_texture_metadata.
 * @param row Texture row, as for globe_texture_metadata.
 * @param expected Required metadata from globe_texture_metadata; changed identities are rejected.
 * @return Owned byte array released with array_destroy, or NULL for missing, changed or corrupt data.
 * @note No decompression, filesystem fallback or whole-dataset cache is performed.
 */
array_t *globe_texture_jpeg(globe_t *globe, unsigned level, unsigned column, unsigned row,
                            const globe_texture_info_t *expected);

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
 * @brief Stream clipped vector polygons from indexed regional data or a shared resident geometry cache.
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
 * @note Cold regions up to 90 degrees on each axis stream indexed candidate rings without a global cache.
 * Wider queries lazily retain exact coordinates, a spatial index and four display resolutions in native memory.
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
 * @return Owned controller, or NULL if configuration or the existing database cannot be read in its selected mode.
 * Reads MARS_GLOBE_DB_PATH and MARS_GLOBE_DB_KEY before literal settings in MARS_HOME/config/globe-db.env.
 * MARS_HOME defaults to ~/.mars; the default database path is MARS_HOME/globe/globe.db.
 * Empty or absent keys select plaintext; non-empty keys select encryption without fallback on failure.
 * The geometry connection is owned and closed by the controller. Opening never creates a missing database
 * and never falls back to geography in the almanac or jurisdiction database.
 */
globe_t *globe_open_configured(sqlite_t *town_db);

/** @brief Serially owned configured geometry session; not shared across threads or forks. */
typedef struct _globe_reader_t globe_reader_t;

/** @brief Allocate a lazy configured geometry session; release with globe_reader_free. */
globe_reader_t *globe_reader_new(void);

/**
 * @brief Borrow the session's controller until the next get or free, or NULL on configuration/read failure.
 * Reuses the selected-mode connection while configuration and database/WAL metadata remain unchanged.
 * Changes close the old controller before opening the replacement, including invalid keys and missing files.
 * Callers must serialise all access and must not close the borrowed controller or retain it across this call.
 */
globe_t *globe_reader_get(globe_reader_t *reader);

/**
 * @brief Return an opaque cache identity for the reader's current configured database and journal state.
 * @param reader Borrowed serially owned reader, refreshed before obtaining its identity.
 * @return Owned hexadecimal identity, or NULL when storage is unavailable; release with string_free().
 * @note This metadata fingerprint invalidates derived imagery after replacement or writes; it is not a content checksum.
 * It contains no database path or credentials. Revalidate before publishing work derived from an earlier identity.
 */
string_t *globe_reader_identity(globe_reader_t *reader);

/** @brief Close a session's owned connection and release the session; accepts NULL. */
void globe_reader_free(globe_reader_t *reader);

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
 * @brief Classify with a caller-selected distance from physical shoreline boundaries.
 * @param globe Borrowed controller; serialise access as for globe_hit_test().
 * @param latitude_degrees Finite latitude in [-90,90].
 * @param longitude_degrees Finite longitude in [-180,180].
 * @param radius_metres Finite proximity radius from zero to 20,000 metres, inclusive.
 * @return GLOBE_COAST within the radius; otherwise the ordinary surface type, or GLOBE_UNKNOWN on failure.
 * @note Distances use a local tangent-plane approximation, not a surveyed or geodesic distance guarantee.
 * Virtual polygon-closing edges are excluded. A wider radius denotes proximity on either side of a boundary,
 * not harbour membership, maritime ownership, shelter or a clear horizon. The default 500-metre API is unchanged.
 */
globe_surface_t globe_hit_test_radius(globe_t *globe, double latitude_degrees, double longitude_degrees,
                                      double radius_metres);

/**
 * @brief Identify a recognised inland water without assigning a country.
 * @param globe Borrowed geographic controller; serialise access as for surface classification.
 * @param latitude_degrees Latitude in degrees, between -90 and 90.
 * @param longitude_degrees Longitude in degrees, between -180 and 180.
 * @return Owned name, or NULL for unrecognised waters, land, coastal uncertainty, invalid input or unavailable data.
 * @note Currently recognises the Caspian Sea by its shoreline polygon, not merely a rectangular region.
 * Other lakes and islands are excluded. Names do not establish legal jurisdiction or navigational safety.
 */
string_t *globe_water_name_at(globe_t *globe, double latitude_degrees, double longitude_degrees);

/**
 * @brief Return an owned country name for a covered point, or NULL when unavailable or ambiguous.
 * Uses indexed Natural Earth administrative boundaries. Water and near-shore classifications do not suppress
 * administrative coverage. For uncovered points only, detailed shoreline-confirmed land can bridge an outline
 * gap of up to one kilometre if exactly one country occurs within that distance. Missing shoreline, water,
 * administrative border points and competing countries cannot use this fallback. Distances use a local tangent
 * plane; these generalised polygons and the gap policy are not legal or navigational boundary determinations.
 * The caller releases the result with string_free; access to the borrowed database must be serialised.
 */
string_t *globe_country_at(globe_t *globe, double latitude_degrees, double longitude_degrees);

/**
 * @brief Return an owned ISO country or UK constituent-country code for a covered point; release with string_free.
 * Shares globe_country_at() coverage and its bounded, confirmed-land outline-gap policy. Returns NULL for
 * missing or ambiguous administrative geometry; never extends country coverage over uncovered water.
 */
string_t *globe_country_code_at(globe_t *globe, double latitude_degrees, double longitude_degrees);

/** @brief Visit one shared land-border segment in longitude/latitude degrees; false stops with failure. */
typedef bool (*globe_border_segment_fn)(double ax, double ay, double bx, double by, void *context);

/**
 * @brief Stream indexed shared administrative borders for a finite viewport, never administrative coastlines.
 * Bounds may wrap the date line with west in [-360,180] and east-west in (0,360]. Latitude is [-90,90].
 * Output is bounded to 250,000 segments; false means missing data, invalid bounds, or visitor/budget failure.
 * Generalised boundaries are for overview display only and do not establish legal jurisdiction.
 */
bool globe_border_segments(globe_t *globe, double west, double south, double east, double north,
                           globe_border_segment_fn visitor, void *context);

/**
 * @brief Transactionally replace country boundaries from open Natural Earth polygon SHP and UTF-8 DBF streams.
 * Both streams must start at byte zero. Other globe layers are preserved; malformed input rolls back replacement.
 * Border matching accepts at most 1,048,576 source edges in a temporary ordinary-heap geometry buffer; no large
 * SQLCipher temporary table is used. The buffer and sorting workspace are released before returning.
 */
bool globe_import_countries(sqlite_t *db, file_t *shapes, file_t *attributes);

/**
 * @brief Import a bounded GeoJSON collection of independent cultural display regions atomically.
 * Features require Polygon or MultiPolygon geometry and a properties.code display identifier.
 * The input is limited to 16 MiB, 1,023 features and one million vertices. Coordinates are retained
 * at integer-microdegree precision. Country, shoreline and river tables are never modified.
 * @return True on complete publication; malformed input and write failures retain the previous layer.
 */
bool globe_import_regions(sqlite_t *db, file_t *source);

/**
 * @brief Return an owned cultural display-region code for a coordinate, independently of jurisdiction.
 * Queries the optional indexed region layer, including polygon holes. Ambiguous overlaps, exact
 * boundaries, invalid coordinates and unavailable data return NULL; no nearest-region inference is made.
 * Display codes are local presentation identifiers, not assertions of sovereignty or ISO registration.
 * Release a returned string with string_free(). This operation does not change country identification.
 */
string_t *globe_region_code_at(globe_t *globe, double latitude, double longitude);

/**
 * @brief Transactionally replace shoreline tables from legacy GSHHG or lossless binary64 rings.
 * @param db Borrowed writable database; unrelated tables are retained.
 * @param source Borrowed open binary input positioned at its first polygon header or MARSCF64 signature.
 * @return True after publication; false rolls back the replacement without closing either resource.
 * @note Uses a savepoint: a caller-owned transaction remains open and controls final publication.
 * Keeps levels 1–5, using the Antarctic ice front rather than the alternative grounding line.
 * The lossless format starts with the eight bytes MARSCF64, then uses GSHHG-shaped 44-byte headers
 * and big-endian binary64 degree coordinate pairs. Header bounds are outward-rounded microdegrees.
 * Geometry must already have passed topology and nesting validation; this importer checks encoding,
 * coordinate ranges and storage integrity, not geographic topology. Version 2 readers retain coordinate
 * precision through indexed queries and full-resolution paths; legacy version 1 data remain readable.
 */
bool globe_import_shoreline(sqlite_t *db, file_t *source);

/** @brief Geographic layers; unavailable detail must never be inferred from another layer. */
typedef enum _globe_layer_t {
    GLOBE_LAYER_SHORELINE,
    GLOBE_LAYER_RIVERS,
    GLOBE_LAYER_TOWNS,
    GLOBE_LAYER_ROADS,
    GLOBE_LAYER_BUILDINGS,
    GLOBE_LAYER_COUNTRIES
} globe_layer_t;

/** @brief Return whether a controller has a usable installed implementation for this layer. */
bool globe_has_layer(const globe_t *globe, globe_layer_t layer);

/** @brief Consume one clipped river segment; return false to cancel. Coordinates are longitude/latitude degrees. */
typedef bool (*globe_river_segment_fn)(int64_t id, unsigned kind, double ax, double ay, double bx, double by, void *context);

/** @brief Read a recorded width in metres; zero means unknown. False indicates invalid or corrupt data. */
bool globe_river_width_metres(globe_t *globe, int64_t id, double *width);

/**
 * @brief Consume one mapped water polygon, with an outer ring followed by holes.
 * @param coordinates Borrowed interleaved longitude/latitude degrees, unwrapped to the query window.
 * @param ring_sizes Borrowed point counts for each consecutively stored, closed ring.
 * @param ring_count Number of rings. Arrays remain valid only during the callback.
 * @param context Caller-owned context. Return false to cancel and discard partial output.
 */
typedef bool (*globe_river_bank_fn)(const double *coordinates, const size_t *ring_sizes, size_t ring_count, void *context);

/** @brief Stream indexed mapped water polygons; absent bank data give an empty successful result. */
bool globe_river_banks(globe_t *globe, double west, double south, double east, double north,
                       globe_river_bank_fn visit, void *context);

/** @brief Whether a window lies wholly within an explicitly complete mapped open-water survey.
 * Missing coverage returns false; absence of a water polygon alone never establishes land.
 */
bool globe_river_water_complete(globe_t *globe, double west, double south, double east, double north);

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
    const char *elevation;         /**< Elevation in metres, or "0" when absent or the source uses -9999 for no data. */
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

/**
 * @brief Page through towns belonging to one non-empty jurisdiction code using indexed membership.
 *
 * Cursor, limit, callback ownership and return semantics follow globe_towns_batch().
 * Does not include parent or child jurisdictions implicitly.
 */
bool globe_towns_jurisdiction_batch(globe_t *globe, const char *jurisdiction, int64_t after_id, size_t limit,
                                    globe_place_visit_fn visit, void *context, int64_t *next_id);

#endif
