# `globe_t`

The opaque geographic controller in `include/globe-t.h` owns the shoreline and
river readers behind a single public interface. Implementations live under
`src/globe`; there are no separate public shoreline or river headers or guides.
SVG and WebGL rendering remain application responsibilities.

## Ownership and layer availability

`globe_open()` accepts borrowed shoreline and jurisdiction SQLite connections,
either of which may be NULL. It returns an owned controller unless both
connections are NULL or allocation fails. Missing datasets do not prevent a
controller from opening. `globe_close()` accepts NULL and releases readers,
never their connections. Both connections must outlive the controller; serialise
calls and close controllers before replacing their datasets.

`globe_open_databases()` separates the shoreline, map-geometry and town
connections. It permits an independent globe database alongside the jurisdiction
town catalogue, while keeping the almanac shoreline connection during migration.
After migration, both geometry arguments can use the same globe connection.
Each layer reads only its designated store; missing geometry never falls back
to tables accidentally left in the town database. All connections are borrowed.
The original two-connection API retains its existing behaviour for current callers.

`globe_open_configured()` owns an independent geometry connection and optionally
borrows a jurisdiction connection for towns. It resolves `MARS_GLOBE_DB_PATH` and
`MARS_GLOBE_DB_KEY` before literal values in `MARS_HOME/config/globe-db.env`.
It opens only an existing database, with read queries enabled, and closes its
owned connection after releasing layer readers. The almanac's surface APIs and
the Lab's river adapter use this entry point; neither reads legacy geometry from
the astronomical or jurisdiction database. Missing globe storage does not disable
the jurisdiction town catalogue.

### Installation targets

`make install-globe-db` creates a separate encrypted shoreline-only database from
the packaged GSHHG source, with no download. Its default location is
`$MARS_HOME/globe/globe.db` (normally `~/.mars/globe/globe.db`); settings are saved
privately in `config/globe-db.env`. `MARS_GLOBE_DB_PATH` and `MARS_GLOBE_DB_KEY`
override saved values. A terminal may prompt for a new password. Non-interactive
installation requires a key. Existing destinations are validated and preserved,
never rebuilt or silently replaced.
Staged import, integrity checking and shoreline availability checks must succeed
before publication. Missing packaged geometry is an error, not an empty installation.

`make check-globe-db` checks integrity and shoreline availability with the saved
settings. It does not prove positional accuracy or import other geographic layers.
`make update-globe-db` and `make uninstall-globe-db` currently reserve the command
interface only: both fail explicitly without changing files. Safe incremental
updates and coordinated removal remain to be implemented.
`make install-mars-db` asks for one shared password before starting, with confirmation
when entering a new password. An empty answer retains the saved almanac key.
That selected key is passed directly to the globe, almanac and jurisdiction
installers without further password prompts; calendar questions may still appear.
Non-interactive invocation uses the configured almanac key. Standalone installers
retain their separate configuration behaviour. An existing globe encrypted with a
different key is rejected safely, not silently overwritten or re-keyed.
The three installations run sequentially, stopping on failure. This is not a transaction spanning three files:
an earlier successful installation remains if a later one fails. Stop the Lab
before rebuilding databases and restart it afterwards. New almanac installations
contain no shoreline geometry; existing copies are not deleted merely by opening
the configured globe.

`globe_has_layer()` reports availability through `globe_layer_t`. Shoreline and
future detailed geometry belong in the globe database; towns remain in the
jurisdiction database. Only the packaged GSHHG shoreline is currently installed.

`globe_towns_batch()` reads bounded, ascending-ID pages of at most 1024 towns
through `globe_place_visit_fn`. Its borrowed `globe_place_t` fields retain stored
coordinate precision and are valid only during the callback. An optional nearby
filter prioritises a five-degree window on both axes, wrapping longitude. Pass
the last returned ID to continue, stopping when the next ID is zero. A visitor
may stop a page early. The jurisdiction catalogue's paged API delegates to this
controller, preserving existing client behaviour without retaining the catalogue
in the native controller.

Roads and buildings remain explicitly unavailable; their import and query
implementations are not yet installed. An unavailable layer is not evidence
that a region has no features.

## Shoreline classification

`globe_hit_test()` accepts finite WGS-84 decimal degrees: latitude from −90 to
90 and longitude from −180 to 180. Its `globe_surface_t` result is `GLOBE_LAND`,
`GLOBE_SEA`, `GLOBE_INLAND_WATER`, `GLOBE_COAST` or `GLOBE_UNKNOWN`.
`GLOBE_COAST` represents the 500-metre uncertainty band around a mapped boundary;
this is a planning policy, not a surveyed accuracy guarantee. Invalid input,
missing geometry and read failures are unknown, never implicitly sea.

Cold hit-tests use indexed chunks; resident hit-tests use exact coordinates,
not simplified display geometry. Islands within lakes retain their nesting.

## Shoreline paths and resident geometry

`globe_surface_prepare()` makes exact shoreline geometry, its spatial index and
four display resolutions resident. File-backed readers share immutable geometry;
anonymous databases use reader-local geometry. The cache does not own an open
database connection.

`globe_surface_paths()` streams clipped polygons through `globe_path_fn`, using
`globe_path_event_t` move, line and close events. Odd nesting levels are land;
even levels are inland water. The Antarctic ice-front flag allows a renderer
to colour Antarctica separately. Fill levels 1–4 over water in nesting order.

Bounds may cross the date line: west is between −360° and 180°, east is greater
than west by at most 360°, and latitude bounds lie within −90° to 90° with north
greater than south. Display tolerance is between 0° and 1°; zero retains every
vertex. This tolerance never changes hit-test classification. Discard partial
output when a query fails or its visitor cancels.

`globe_import_shoreline()` transactionally replaces shoreline tables from an
open GSHHG 2.3.7 binary stream. It retains unrelated tables and rolls back
malformed input. Stored uncompressed coordinate chunks use BLOB pages and an
integer R-tree; older compressed chunks remain readable. Levels 1–5 are retained,
using the Antarctic ice front rather than the alternative grounding line.

See [almanac data provenance](almanac-data-provenance.md#shoreline-data) for
source, checksums and licence notices.

## Rivers

`globe_river_segments()` streams rectangularly clipped segments selected by an
integer R-tree. Bounds follow the longitude and latitude rules above. The
`globe_river_segment_fn` visitor receives source feature IDs, classes and two
longitude/latitude endpoints, unwrapped into the requested window. Segment
order is unspecified; each segment can be drawn independently. False from a
visitor cancels the query; discard partial output on any failure. Recursive
queries on the private river reader are rejected.

Invalid bounds, corrupt selected geometry, database failures and unavailable
data return false. An empty successful query is distinct from unavailable data.
Native queries read intersecting chunks, not a resident worldwide river network.

`globe_import_rivers()` retains the legacy WDBII binary importer for format
fixtures. It replaces only river tables transactionally and preserves previous
data on failure. Uncompressed chunks contain at most 257 points, overlapping
by one endpoint; no closing edges are invented. Source classes are raw bytes,
not navigability ratings. The legacy format supplies neither river names nor
surveyed widths, depths or navigability.

### Withdrawn dataset and replacement work

WDBII rivers were withdrawn because their geometry omits the Severn's town-centre
meander at Shrewsbury. The globe controller refuses that source even in older
installed databases. `make install-jurisdiction-db` no longer imports it;
reinstallation removes the retired data. Towns, calendars and shoreline data
are unaffected. Historical licensing and provenance remain in the
[third-party notices](../THIRD_PARTY_NOTICES.md#river-data).

No replacement is packaged yet. An independent rendering of the 19 October 2026
[Geofabrik Shropshire extract](https://download.geofabrik.de/europe/united-kingdom/england/shropshire.html)
confirms that OpenStreetMap river centrelines preserve the missing Shrewsbury
loop. OpenStreetMap also represents riverbank areas. British streets and
waterways were the initial integration scope; the river source scope now covers
the worldwide OpenStreetMap dataset. Coverage and ODbL redistribution requirements
must be checked during integration.
Buildings are also planned, not currently provided by the controller.

### Worldwide source and regional derivatives

The selected upstream snapshot is
[OpenStreetMap planet, 12 October 2026](https://planet.openstreetmap.org/pbf/planet-261012.osm.pbf),
95,380,942,829 bytes compressed. Download staging belongs in the ignored
`build/mapdetail-source` directory, alongside the upstream checksum. A partial
download is not an importable dataset; verify the completed file before extraction.
Keep the original snapshot unchanged and retain source attribution in derivatives.
The source file is not the installed jurisdiction database and must not be loaded
wholesale into application memory.

The planned extraction pipeline will select regions and feature classes as needed,
storing derived geometry in spatially indexed jurisdiction tables. Worldwide source
availability does not mean every region has already been imported. Track imported
coverage separately from successful empty queries.

Display simplification should use zoom-dependent error bounds, preserve junctions
and shared boundaries, and retain original geometry for close views and geographic
classification. Fewer points must not erase narrow waterways or disconnect river
mouths from coastal water. Unconstrained B-spline interpolation is unsuitable:
smooth curves can cut across land or overshoot a bend. Any future curve fitting
must respect the source geometry's error bounds and connectivity; it is not a
substitute for missing coordinates. These extraction and simplification policies
are planned, not yet implemented by the legacy river importer.

## Application loading and memory

The existing MARS Lab river overlay requests independently at 12× zoom or closer,
when longitude and latitude spans are each at most 30°. Movement is debounced;
obsolete requests are cancelled and late responses ignored. This mechanism
does not make withdrawn or uninstalled data available.

The browser retains one regional texture, about 16 MiB, replacing it when a new
region loads. Zooming out hides it immediately and releases its texture storage
after a two-second grace period. Returning during that interval reuses it;
returning later requests the region again. Graphics drivers may retain freed
allocations internally. Towns remain resident and observer selection is
unaffected. This browser policy does not evict the separate native shoreline
cache or establish a memory budget for the planned road and building layers.

River strokes use the same local water-colour sampling, lighting and water-only
fallback as sea and fjord fills. They do not invent surveyed channel widths or
change shoreline classification or observer corrections. See
[MARS Lab](mars-lab.md) for the other globe controls.

## Limitations

These layers are geographic planning data, not nautical charts. They cannot
establish safe navigation, water depth, legal access or surveyed coastal position.
Do not interpret a rendered waterway as proof that it is navigable.
