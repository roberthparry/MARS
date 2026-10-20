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

The browser uses a directly indexed, jurisdiction-qualified long-name exception
table for display labels. Currently Llanfairpwllgwyngyll is the only entry.
Its full name appears in the tooltip at every zoom level and on the globe from
12× zoom when 430 pixels of label space are available. Narrow views retain the
short label. Expanded labels reserve their full width for decluttering; neither
catalogue identities nor selection values change.

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

### OSM coastline candidate: detail budget and acceptance

The 12 October 2026 planet coastline extract is an offline candidate, not an
installed replacement. Its 555,379,506-byte PBF contains 78,154,276 nodes and
1,291,616 ways. A complete readability scan and a node-reference audit passed:
no node referenced by a coastline way was missing. This does not establish
polygon validity or positional accuracy. The worldwide polygon builder exceeded
a 2 GiB memory cap before completing validation.
A separate endpoint audit checked all 2,583,232 way endpoints and found six
unmatched endpoints, no branching endpoint joins and no two-endpoint direction
mismatches. These six open ends must be investigated rather than automatically
bridged. Endpoint checks do not detect interior intersections or invalid rings.
They form three geographically close pairs near (longitude, latitude)
(-70.239, 41.726), (122.412, 30.622), and (67.378, 24.109), separated by
approximately 118 m, 2.8 m, and 4.27 km respectively. Proximity alone does not
authorise joining them, particularly the multi-kilometre pair.

An independent repaired candidate applies upstream OSM corrections retrieved on
20 October 2026, rather than artificial straight-line bridges. The inputs are
ways 375613734 v20, 1442623440 v3, 1567136426 v2, 1567301894 v3,
1567301892 v3 and 1567557982 v3, with their referenced nodes from the official
OSM API. The corrected ways contain **zero unmatched endpoint joins**. The full
candidate contains 78,154,662 nodes and 1,291,618 ways and passes the missing-node
reference check. Its SHA-256 is
`1a6790b1f0da07d8da891d3e57bb4db71aafaf1d5e6799c5568dca3bff427d3a`.
Both the original extract and downloaded repair inputs remain separate;
these checks still do not establish worldwide polygon validity.

Four regional exports succeeded without geometry-export errors. Unsimplified
comparisons on 20 October 2026 measured:

| Region | Current clipped GSHHG vertices | OSM vertices in window | Median OSM segment spacing |
| --- | ---: | ---: | ---: |
| Limfjord | 769 | 20,205 | 17.82 m |
| Severn estuary | 1,101 | 11,640 | 12.22 m |
| Pembrokeshire | 1,871 | 60,133 | 7.79 m |
| Cape Peninsula | 1,021 | 7,095 | 19.20 m |

These counts include way endpoints, and the sources do not cover precisely the
same features. Segment spacing is not positional accuracy. The OSM coastline
sample ends below the upper Severn outline present in GSHHG: riverbank geometry
must supply that continuation before replacing the old water outline.

The target is approximately two to three times the current effective detail,
not a compulsory per-region vertex quota. Diagnostic per-way simplification
retained 1,665 Limfjord vertices and 4,414 Pembrokeshire vertices at 40 m, and
2,316 Cape Peninsula vertices at 10 m. These measurements use a local metric
approximation and preserve way endpoints, but do **not** prove topology safety.
They are not production tolerances or permission to discard narrow waterways.

Whole-region GEOS topology-preserving trials subsequently retained 1,745
Limfjord, 2,540 Severn, 5,590 Pembrokeshire and 2,338 Cape Peninsula vertices,
using respective tolerances of 40, 8, 40 and 10 metres. All four retained their
line-component counts, way endpoints and closed-way status and passed multiline
simplicity checks. These totals approach the requested two-to-three-times budget,
but **the candidates are not approved for installation**.

A directional guard found source spans where simplified chords would move land
over existing water, by up to nearly the selected tolerance. Restoring those
spans retained 10,026 Severn and 5,914 Cape Peninsula vertices without crossings.
The mixed Limfjord and Pembrokeshire results introduced crossings and were
rejected in favour of their original regional geometry. Thus the current
experiment has not met the detail budget while satisfying the water-preservation
requirement. GEOS is used only by the offline diagnostic, not as a new MARS
runtime dependency.

Regional polygonisation now provides a checked, unsimplified baseline for all
four sample windows. Each produced valid land polygons with no polygoniser
cuts, dangling edges or invalid rings. Probes one millimetre to either side
of source segment midpoints, in the local metric projection, confirmed the
OSM land-left convention:

| Region | Polygon coordinates | Land/water-side probes | Mismatches |
| --- | ---: | ---: | ---: |
| Limfjord | 20,188 | 40,190 | 0 |
| Severn estuary | 11,563 | 23,098 | 0 |
| Pembrokeshire | 59,549 | 118,240 | 0 |
| Cape Peninsula | 6,953 | 13,860 | 0 |

The offline `land-source-<region>.geojson` files retain these polygons.
Polygon coordinate totals differ from raw way totals because clipping,
joining and ring closure change the representation. These checks validate
regional construction against the source, not surveyed accuracy or worldwide
coverage.

An inward polygon-hull trial avoided adding land over water and retained
component and hole counts, but still failed the display requirements. Sampled
boundary displacement reached approximately 3,145 m in the Limfjord, 207 m
in the Severn estuary, 532 m in Pembrokeshire and 140 m in the Cape Peninsula;
the Limfjord regional seam also changed. These are sampled diagnostics, not
guaranteed maximum error bounds. All four candidates were rejected.

A subsequent trial intersected ten-metre topology-preserving simplification
with the original land and restored a twenty-metre regional-edge collar.
Regional seams were unchanged, but land-component counts changed, strict
source-containment checks failed, and the Limfjord failed a whole-boundary
twenty-metre buffer check. None of these results is approved for installation.
Further simplification must constrain displacement and preserve regional joins
as well as polygon validity; meeting a vertex quota alone is insufficient.

Acceptance requires coherent closed land/water polygons, preserved small islands
and channel connectivity, checked river-mouth joins, and no newly introduced
crossings or invalid rings. Detailed source geometry must remain recoverable
while overview and regional display layers use bounded, spatially indexed
queries. The full OSM source must not be loaded into browser memory.

## Rivers

OpenStreetMap conversion belongs to the database installation tools, not the globe
runtime. The private `cfg_database_import_osm_rivers()` implementation lives in
`tools/mars_config/src/database/cfg_database_globe.c`; the staging command uses
it when creating or updating a dataset. Neither `globe-t.h` nor `libmars` exposes
or contains that OSM importer. The runtime reads indexed database chunks and applies
display-only simplification with shared junctions fixed. The legacy WDBII importer
below remains solely for its existing format fixtures; it is not the OSM import path.

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
storing derived geometry in spatially indexed globe tables. Worldwide source
availability does not mean every region has already been imported. Track imported
coverage separately from successful empty queries.

Worldwide river extraction was started on 20 October 2026 from this verified
planet snapshot. Its staging filter selects ways and relations tagged
`waterway=river,stream,canal,tidal_channel,riverbank`, `natural=water`, or
`water=river,stream,canal`, retaining referenced nodes and relation members.
Water areas include lakes and reservoirs so that later processing can preserve
connections through them. Selection is not proof that every selected object is
a river, a valid polygon or a navigable waterway; classification and geometry
validation remain required.

The extraction is capped at 3 GiB RAM, with no swap and one CPU core. The output
`build/mapdetail-source/rivers-261012.partial.osm.pbf` remains staging data until
the job succeeds and readability, references and representative river geometry
have been checked. It is not installed by the database installer. The replacement
OSM importer must publish spatially indexed river and water-area geometry into
the **globe database**, preserving its shoreline tables transactionally; neither
the jurisdiction nor almanac database is its destination. The existing legacy
binary importer cannot consume this PBF.

The extraction completed on 20 October 2026 at approximately 20:54 BST after
164 minutes 48 seconds. Its output is **11,966,197,014 bytes**. The separate full
decode completed with CRC32 `fd3bc392`, ordered objects and no duplicate versions; neither
successful extraction nor a CRC calculation alone establishes reference integrity
or geographic correctness. The installed globe database remains unchanged.

The staged export configuration `packaging/globe-db/osm-water-export.json` retains
OSM object types, 64-bit IDs and way-node IDs alongside water classification tags.
Where present, names, Wikidata identifiers and stated/estimated widths are retained
for later selection and review; they are not proof of a complete river course or
surveyed channel width.
These IDs are required to identify shared junctions before reducing stored or displayed detail.
Water areas must remain distinct from centreline features. The configuration is
not itself a packaged replacement dataset. The initial worldwide export was stopped:
its 26 GB sparse coordinate index under a 3 GiB memory cap caused excessive random
disk reads. The verified 12 GB water extract and partial export are retained.
A centreline-only derivative was prepared from that completed extract, retaining
all worldwide river, stream, canal and tidal-channel ways and their referenced nodes.
Water-area boundaries remain available in the original extract but do not need to
inflate the centreline conversion index. This does not filter by river width or region.
The previous waiting import has been stopped. Preparation retained the 3 GiB RAM cap,
no swap and one CPU core. It completed in 27 minutes 56 seconds on 20 October 2026,
producing **6,148,008,561 bytes**. A subsequent full decode completed successfully,
reporting 831,471,832 nodes and 32,410,488 ways, ordered without duplicate versions.
This decode did not calculate a checksum and does not prove reference completeness
or geographic correctness. The installed database is not automatically replaced.

Conversion resumed at 23:11 BST on 20 October 2026 using `dense_file_array`, the
installed Osmium manual's recommended disk index for very large extracts. The
direct index avoids the binary searches of the earlier sparse index. Its address
range is approximately 114.2 GB because OSM node IDs are retained; this is temporary
disk storage, not installed river data or a RAM allocation. The run retains the
3 GiB RAM cap, no swap and one CPU core, with a six-hour safety timeout rather
than a promised completion time. Index files are limited to 128 GiB and compressed
output to 32 GiB. Output is streamed to
`build/mapdetail-source/rivers-261012-centrelines.geojsonseq.zst`, followed by a
Zstandard integrity check. The source and previous attempt's files remain intact.
Export geometry errors are logged for review; completion alone is not geographic
acceptance, and no installation is queued.

The 23:11 conversion was interrupted by a machine reboot at 23:21 BST, leaving
an incomplete 35.1 GB index and no GeoJSON output. The source remains intact.
Pre-reboot logs record memory pressure but do not establish the crash's cause.
A lower-resource recovery was stopped before index construction because the user
service manager did not expose `io.max`: accepted unit properties alone did not
prove disk throttling. A system-managed transient job running the converter as
the ordinary user was then launched with local administrator authentication. Its
limits are 1.5 GiB RAM, no swap, half a CPU core, 64 MB/s reads and 16 MB/s writes;
the recovery launcher checks the actual kernel controls and stops the job if
verification fails. The running job's kernel controls were verified. It preserves
interrupted files and does not change permanent system configuration or install
river data.

The installed river layer has a **3,800,000,000-byte total budget** (decimal
3.8 GB), including tables, metadata and spatial/feature indexes. This supersedes
the earlier 3.8 MB limit after clarification of the intended units. Coverage is to
prioritise larger rivers worldwide, with the Severn explicitly retained, rather
than being restricted to selected sailing regions; the larger allowance avoids
the drastic coverage reduction required by the previous limit. The complete
source extract remains separate. The installable derivative has not yet been
prepared or geographically accepted. If further selection is needed to meet
the limit, it must not pick arbitrary first-arriving way fragments. Width filtering
is section-by-section: a narrow upper reach must not discard wider downstream reaches.

The installer-owned staging command `globe-stage-rivers` accepts an exported
GeoJSON sequence, either plain or Zstandard-compressed, and a new candidate path. It refuses existing destinations,
uses mode 0600 and checks SQLite integrity. The full-resolution extract remains
separate. Compressed input is detected by its frame signature and streamed through
64 KiB buffers directly into the record parser, without an expanded staging file.
It must contain exactly one complete frame with a window no larger than 8 MiB.
Truncation, checksum failures, trailing data, concatenated frames and a missing
final record newline roll back the import. Both forms have a 128 GiB decoded-input
safety ceiling, distinct from the final database budget. Progress reports every
100,000 input records include retained and omitted section counts.
After identifying shared junctions with disk-backed node counts, the
installer reduces stored centrelines with an eight-metre maximum displacement
budget, chosen conservatively from the finest regional shoreline trial tolerance.
This is a candidate detail target, not a claim of surveyed accuracy or a uniform
shoreline resolution. Geographic-axis distance uses a conservative 112 km per
degree bound, retaining extra detail at high latitudes. Junctions, mouths and chunk
endpoints remain fixed; small loops are not collapsed. The separate width filter
uses an initial sixteen-metre minimum; this is a coverage choice, not a consequence
of the eight-metre simplification error ceiling. Only source ways with a usable
width below the cutoff are omitted. Wider sections of the same named river remain,
and no replacement segment bridges an omitted section. Rivers, streams, canals
and tidal channels use the same width rule; canal classification alone never
causes exclusion.

Width handling follows the [OSM width convention](https://wiki.openstreetmap.org/wiki/Key:width):
bare decimal values are metres. The importer also recognises explicit metre,
centimetre, millimetre, kilometre, foot, inch, yard and mile units, and feet/inches
notation. It uses `est_width` only when `width` is absent. Missing, malformed,
range-valued or unsupported widths remain unknown and are retained by this filter;
they are not inferred from the name or waterway class. A malformed explicit width
does not fall back to an estimate that could silently exclude the section.
These tags describe whole OSM ways: detecting a narrowing within a way without
separate width tags would require further riverbank geometry analysis. Neither
tagged width nor inclusion in the globe establishes navigability.

Shortcuts are checked against retained segments and spatially indexed neighbouring
chunks, including across the date line. Conflicts or more than 128 neighbouring
chunks cause the affected chunk to retain its original geometry. These checks do
not prove that a centreline stays within riverbanks: bank polygons remain a separate
acceptance requirement. Each chunk holds at most 257 points. Temporary node counts
are removed and the candidate is compacted before measuring its size. Metadata
records source and retained point totals and the storage tolerance; staging prints
the totals instead of extrapolating a worldwide size from incomplete export data.
Neither the PBF nor the complete network is held in application memory.
After compaction, staging rejects a candidate larger than 3,800,000,000 bytes.
The limit applies to the installed derivative, not temporary conversion files,
the retained source extract or the existing shoreline layer.

After geographic review, `globe-install-rivers` accepts the candidate path. It
reuses the configured globe key, validates every copied coordinate and anchor,
rebuilds spatial bounds and checks feature/point totals and storage-detail metadata.
Candidates without that metadata or with a tolerance above eight metres are rejected.
Publication also rejects an oversized source candidate and measures the destination
river tables as they are rebuilt: different database page sizes and encryption
overhead can change their footprint. The check counts newly occupied pages even
when they reuse free space, reserving two additional pages for shared header/schema
overhead. An over-budget replacement rolls back; unrelated shoreline pages are
not charged to the river budget. These checks enforce size, not coverage or
geographic correctness, and do not themselves select larger rivers. The checks
use 64-bit sizes; regressions use a smaller explicit private budget to exercise
rollback without allocating gigabytes and verify that production now accepts
layers exceeding the former 3.8 MB limit.
Only river tables are
replaced, in one transaction; failures preserve the previous river tables and
all unrelated shoreline data. It does not reinstall towns or almanac data.
This path currently installs river, stream, canal and tidal-channel centrelines,
not filled water-area polygons. The original extract retains those polygons for
later work. River strokes do not establish channel width, depth or navigability.

Display simplification should use zoom-dependent error bounds, preserve junctions
and shared boundaries. Retain the original extract separately for future finer
derivatives and geographic checks. Fewer points must not erase narrow waterways or disconnect river
mouths from coastal water. Unconstrained B-spline interpolation is unsuitable:
smooth curves can cut across land or overshoot a bend. Any future curve fitting
must respect the source geometry's error bounds and connectivity; it is not a
substitute for missing coordinates. The new centreline reader implements bounded
deviation with fixed shared-node anchors; bank-crossing and water-area topology
validation remain separate work and are not implied by centreline simplification.

#### River display-detail target

The replacement river display should roughly match the new shoreline's detail
at the same zoom level, rather than drawing every extracted OSM vertex.
Retain the full-resolution source separately; simplify the stored candidate as
described above, not the extraction. Shoreline and river display levels share a spatial
error budget, constrained by screen scale. The Lab now passes the same tolerance,
the smaller viewport angular span divided by 2048, to both shoreline and river
display queries. `globe_river_display_segments()` keeps source geometry unchanged,
retains junctions and chunk endpoints, and bounds the deviation of omitted points.
Zero display tolerance requests the stored candidate geometry, not the unsimplified
OSM source. The shoreline's two-to-three-times
detail target is not a river vertex quota or an instruction to drop small rivers.

Keep confluences, river mouths, feature endpoints and regional boundaries fixed.
Simplify between these anchors, preserving bends and network connectivity.
Riverbank polygons must preserve waterways and their connections to coastal
water; do not simplify opposite banks independently if that can close a channel.
Reject candidates that cross banks, detach a tributary or alter a shared mouth.
Where these constraints require more points, retain them instead of forcing the
nominal detail budget. Close views can request a finer derived level through the
existing asynchronous regional-loading mechanism. Import, publication rollback
and shared-junction simplification have native regressions. Worldwide geographic
acceptance and installation are still pending; the withdrawn legacy dataset is
not re-enabled by these changes.

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
