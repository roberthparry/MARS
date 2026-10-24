# `globe_t`

The opaque geographic controller in `include/globe-t.h` owns the shoreline,
country and river readers behind a single public interface. Implementations live
under `src/globe`; there are no separate public shoreline or river headers or guides.
SVG and WebGL rendering remain application responsibilities.

## Named inland waters

`globe_water_name_at` returns an owned geographic name independently of country
identification. It currently recognises the Caspian Sea. Indexed ray intersections
check that the selected point and a known reference point occupy the same shoreline
polygon; a rectangular region is only an early rejection optimisation. Land,
islands, nearby separate lakes and the coastal uncertainty band are excluded.
Invalid input, unrecognised water and unavailable or corrupt geometry return
`NULL`. Release a returned name with `string_free`.

The lookup reads the installed shoreline data without a database migration or
network request. It is not a complete gazetteer and does not assign nationality,
maritime jurisdiction or navigation rights. Surface classification remains
independent: recognising a water body's name does not change its inland-water
classification.

On a custom globe selection, the Lab suggests longitude-based nautical time only
for inland water with no identified geographic jurisdiction, and only when the
country-boundary layer is available. Ocean points, land, coastal uncertainty and
inland water belonging to an identified jurisdiction keep their existing zone.
The zone control and summary identify its value as a UTC offset: positive east,
negative west, the opposite sign from a traditional nautical *zone description*.
Defaults use 15-degree bands, with half-band ties rounded away from Greenwich;
the two representations of the date line retain offsets of +12 and -12.
This is not a civil-time or daylight-saving lookup. The editable value persists
through evaluation and relaunch; a new selection may offer a new default, but a
pending geographic reply never overwrites a zone edited while that reply was in
flight. Cached geographic replies retain their suggestion without another lookup.
The band convention is described in [NGA Bowditch, volume 2, table 26](https://msi.nga.mil/api/publications/download?key=16693975%2FSFH00000%2FBowditch_Vol_2_LoRes_2019.pdf);
the control uses UTC-offset signs rather than that table's zone-description signs.

With a configured globe database whose shoreline includes the Caspian Sea:

```c
#include <stdio.h>

#include "globe-t.h"
#include "string-t.h"

int main(void)
{
    globe_t *globe = globe_open_configured(NULL);
    string_t *name = globe_water_name_at(globe, 41, 51);
    printf("%s\n", name ? string_c_str(name) : "Unnamed water");
    int status = name ? 0 : 1;
    string_free(name);
    globe_close(globe);
    return status;
}
```

Output:

```text
Caspian Sea
```

## Live SVG shoreline display

MARS Lab's map surface uses filled SVG paths, not a shoreline image uploaded to
a WebGL texture. The native calendar endpoint selects and clips installed shoreline
polygons into geographic patches no larger than thirty degrees, preserving land,
lake and island nesting. WebAssembly owns these polygons in `array_t` storage,
projects them with the globe camera, clips the hidden hemisphere and constructs
the displayed SVG. JavaScript only requests geometry and inserts the native output.
The cache is a serially dispatched singleton; replacement is atomic and rejected
input leaves the previous geometry intact.

Cold regional reads intersect opposing spatial ray selections before decoding
complete rings, retaining enclosing land while excluding distant rings on either
side. A bounded 64 KiB page buffer reuses compressed database pages across their
coordinate chunks. Thirty-degree transport patches avoid repeatedly reading the
same continental rings for unnecessarily small cells; the WASM projector still
subdivides edges before horizon clipping.

A file-backed regional reader retains one padded local window of exact clipped
coordinates, bounded to 524,288 events (approximately 16 MiB). Nearby requests
inside it re-clip that bounded stream at their own display tolerance, avoiding
repeated continental reads without retaining a coarse zoom's simplification.
File and WAL identity changes invalidate it; anonymous databases do not share
this cache. Oversized windows fall back to streamed selection. Cold loads still
pay the database-selection and decoding cost; cached timing is not cold timing.

`globe_reader_new`, `globe_reader_get` and `globe_reader_free` provide an explicitly
owned, lazy configured session. `get` borrows a controller, reusing its configured
connection while the resolved path/key and database/WAL metadata are unchanged.
Missing files, changed keys or changed metadata discard the old connection before
attempting a replacement. The borrowed controller expires on the next `get` or
`free`; callers must serialise access and create sessions after forking. The Lab's
single geometry service owns one such session and closes it on service exit.

Closed display rings reduced to one or two vertices by simplification are discarded
without rejecting the remaining coastline. Unclosed rings, invalid coordinates and
invalid nesting levels still reject the replacement atomically. This distinction
prevents tiny collapsed islands from forcing a whole regional view back to enlarged
photographic imagery.

Transport is bounded to 500,000 events and the 8 MiB Protobuf envelope budget,
and generated SVG to 32 MiB. Live coordinates are packed binary64, not decimal
text; the native projector validates the byte records before accepting them.
The drawing worker receives its HTTP response through bounded WASM-owned storage
and decodes Protobuf directly into native geometry admission. JavaScript provides
browser Fetch and the unavoidable chunk copy, not coordinate parsing or a relay
through the main page. Retained geometry remains projectable during the download;
failed, cancelled and stale replacements cannot publish a partial response.
Display simplification follows the requested geographic extent; it does not change
the installed data or improve its source accuracy. Photographic wide views remain
raster imagery, and gridlines and rivers remain WebGL overlays. While requested
vector data is unavailable, the last rendered view is held with selection paused;
the client does not substitute photographic ice or render missing coverage as sea.
Gridlines default to off and change only through the user's toggle.

Views centred at or beyond 60 degrees latitude use vectors at every zoom. Native
SVG gradients fade Antarctic land colour between 55 and 80 degrees of absolute
latitude, independently of zoom. The earlier decorative screen-sized floes have
been removed: they were not geographically anchored ice coverage. This display
represents the installed land/ice-front geometry, not current offshore sea ice.
The photographic composite is not used to infer ice extent. The imagery and
installed geographic database remain unchanged.

## Allocation-free quaternion rotations

`quaternion-t.h` provides caller-owned `quaternion_t` values: four binary64
coefficients in `[x,y,z,w]` order. It requires no allocation, global state or
JavaScript arithmetic. The same `src/quaternion` implementation builds natively
and into the Lab's WASM module, replacing its private quaternion algebra,
matrix conversion and spherical interpolation. Camera-specific screen mapping
and persistence remain application responsibilities.

`quaternion_identity`, `quaternion_normalise` and `quaternion_is_unit` initialise,
normalise and validate values. `quaternion_multiply` is the unnormalised Hamilton
product; `quaternion_conjugate` and `quaternion_inverse` retain general algebraic
semantics. `quaternion_axis_angle` and `quaternion_from_vectors` construct unit
rotations. `quaternion_matrix3` produces a row-major matrix for column vectors,
`quaternion_rotate` applies it, and `quaternion_slerp` interpolates along the
shortest rotation arc. Angles are radians; multiplication applies the right
rotation first. Directions of exactly opposite vectors use a deterministic
perpendicular axis. Interpolation accepts only fractions from zero to one.

Inputs may overlap output storage of the documented extent. Boolean operations
reject NULL, invalid inputs and non-finite results without changing the output.
Zero is valid for algebraic multiplication and conjugation, but not as a rotation
or inverse. Normalisation scales extreme magnitudes to avoid norm overflow or
underflow. Precision is binary64, not arbitrary precision; this geometry API does
not add symbolic expression operations. No operation guarantees a browser frame
time. See the public header for individual parameter and failure contracts.

The general Hamilton product uses 16 multiplications and 12 additions/subtractions,
without an extra norm or implicit normalisation. Only pointers are checked before
arithmetic: every input coefficient contributes to every output coefficient, so
the final finite-result check also rejects non-finite inputs. Conjugation likewise
needs only one finite-result check. Neither operation publishes a partial result.

The developer benchmark `bench/quaternion/bench_quaternion.c` compares this kernel
with the eight-product Howell–Lafon factorisation, including exact small-integer
checks and floating-point counterexamples. Its eight variable multiplications
require 28 additions/subtractions and four halvings: 40 ordinary scalar arithmetic
operations when each halving is a multiplication. That is fewer variable products,
not fewer total operations. Preparatory sums can also overflow or cancel tiny
rotation components. The checked direct kernel remains the library implementation;
timings are machine-dependent. The alternative is described in
[Algorithm 41 of Dumas, Pernet and Sedoglavic](https://arxiv.org/abs/2101.01025).

This README example rotates the positive x axis onto the positive y axis:

```c
#include <math.h>
#include <stdio.h>

#include "quaternion-t.h"

int main(void)
{
    quaternion_t rotation;
    double axis[] = {0, 0, 1}, vector[] = {1, 0, 0}, result[3];
    bool ok = quaternion_axis_angle(rotation, axis, acos(-1) / 2) && quaternion_rotate(result, rotation, vector);
    if (ok) printf("Rotated: %.0f %.0f %.0f\n", result[0], result[1], result[2]);
    return ok ? 0 : 1;
}
```

Output:

```text
Rotated: 0 1 0
```

## Native GPU coverage plans

The `webgl-t.h` API in `src/webgl` supplies a small, portable coverage-plan builder,
not a browser context implementation. `webgl_mask_new` sets the vertex budget;
`webgl_mask_begin`, `webgl_mask_point` and `webgl_mask_end` accept simple projected
rings in nesting order. `webgl_mask_vertices` and `webgl_mask_batches` expose
borrowed binary32 XY buffers and uint32 first/count/colour commands.
`webgl_mask_reset` reuses allocations; `webgl_mask_free` releases them. A failed
or incomplete frame is not readable until reset and successful reconstruction.
`webgl_mask_vertex_shader` and `webgl_mask_fragment_shader` supply immutable
WebGL 1 GLSL sources.

The backend uses signed non-zero stencil winding, normalises ring orientation,
and limits each batch to 127 rings to prevent eight-bit wraparound with overlapping
simple polygons. Concave rings need no expensive triangulation; successive levels
paint land, lakes and islands in order. Self-intersecting rings are outside this
simple-ring contract. The Lab bounds the plan to 1,048,576 vertices and 16,384
batches, and transfers binary buffers from its isolated WASM projection worker.

Visible coastlines remain SVG. Their coverage texture is drawn directly from the
same native projection into a GPU framebuffer; there is no SVG Blob, image decode
or image upload in the production mask path. Horizon arcs are subdivided to
subpixel error. Reusable GPU buffers and double-buffered textures keep the previous
complete frame until replacement succeeds. Close-view gestures transform retained
SVG and coverage rather than rebuilding geometry each animation frame. Clipboard
capture still composites the visible SVG, labels and GPU image into a PNG.

Globe compositing shaders also live in native application code. Browser code
compiles GLSL once per context, caches uniform locations and submits numeric state.
Binary geometry is not binary shader code, and WASM still uses browser WebGL APIs.
Projection and CPU submission timings are separate from completed GPU frame time;
neither cached geometry nor shader ownership guarantees a particular frame rate.

## Outstanding delivery priority — 22 October 2026

The user's active implementation priority is to deliver and install the worldwide coastline and rivers with an
**8 m simplification tolerance**. Extraction or staging alone is not completion.
This tolerance is not a guarantee of source positional accuracy or point spacing.
The targeted flag/country cache and dictionary memory checks have now passed;
they are no longer a prerequisite. Resume this delivery work before unrelated improvements.

- Resolve the restored coastline candidate's cross-ring contacts against the
  original geometry; validate waterway preservation, nesting, quantisation and
  the final point budget while retaining existing inland waters.
- Package and integrate the validated coastline, install it, and verify visible
  before/after improvement at Málaga and representative other locations.
- Recover the timed-out river staging safely, add resumable checkpoints, and
  finish simplification at the same tolerance. Prioritise larger rivers worldwide,
  including the Severn; omit narrow sections rather than entire rivers. Keep the
  final river database footprint within the authorised **3.8 GB** budget.
- The user explicitly requested on 22 October that the work not time out.
  For these offline preparation jobs, replace expired wall-clock deadlines with
  `RuntimeMaxSec=infinity` when launching the next validated stage, and verify the
  effective service setting. Preserve existing CPU, memory, no-swap, file-size
  and disk-I/O caps; this is not permission to remove resource safeguards.
  Do not restart the existing monolithic river import unchanged: it commits only
  after simplification. First implement and test durable phase/batch checkpoints
  and resumable staging, keeping incomplete candidates unpublishable. Preserve
  the failed candidate and journal until recovery has been assessed safely.
  Removing a runtime deadline does not prevent crashes, OOM or shutdown; durable
  checkpoints are required as well. Report the actual launched unit and verified
  limits rather than implying that recording this task starts a background job.
- Run the relevant native and almanac browser tests sequentially, install the
  validated river data, and verify that both layers are actually displayed.

As of this checkpoint neither replacement is installed. The coastline review
finished with 530 contacts requiring investigation. The river service timed out
at 17:00 on 22 October; input and junction processing completed, but final
simplification was not confirmed. Recheck live files and service status on resume;
do not treat these snapshots as current progress or promise an unmeasured ETA.

The offline `globe-stage-rivers` command now records versioned checkpoints and
the source SHA-256 in `rivers_stage`. `globe-resume-rivers` takes the same input
and existing candidate paths; a mismatched source or older staging format is
rejected. Input checkpoints commit every 10,000 records, and junction/simplification
checkpoints every 1,000 chunks, with geometry and cursor committed together.
Input-phase resume streams the compressed source from its start but skips already
validated records without reinserting nodes or geometry. Later phases resume by
indexed chunk ID. Dataset publication metadata remains absent until simplification
finishes. The ordinary transactional importer remains all-or-nothing.
Compaction, integrity checks, the 3.8 GB limit and geographic acceptance are still
required; a successful resume alone does not install or approve the dataset.
The local operational launcher is `build/mapdetail-source/run-rivers-checkpointed.sh`.
It uses a separate `rivers-261012-selected-8m-checkpointed.sqlite3` candidate and
`mars-rivers-checkpointed-20261022.service`, with no wall-clock deadline and an
8 GiB disk-space reserve. It verifies kernel resource caps, never installs data,
and requires administrator authentication. Its existence does not mean it is running.
The eight focused river regressions passed after this change, including a forced
junction-phase interruption after chunk 1,000, successful resume, input replay without
duplicate records and rejection of a different source hash. The configuration build
and compliance check also passed. The original test configuration was restored.
The broad configuration suite was stopped after its river checks passed, during
unrelated calendar-installation fixtures; it was not recorded as a complete suite pass.
Live status checked at 22:47 BST on 22 October: the checkpointed service is active,
started at 22:20:11, with `RuntimeMaxUSec=infinity`. Its latest input checkpoint is
860,000 records and 26,306,022 points; this is ingestion, not completed simplification.
The latest 800,000-record summary retained 792,009 sections and omitted 7,991 narrow
sections. Memory was about 779 MiB and free disk space 62 GiB. No coastline service
was active; its completed review still requires the 530 contacts to be resolved.
Neither candidate is installed. The old river candidate and journal remain untouched.

## Selected-position countries

`globe_country_at(globe, latitude, longitude)` returns an owned `string_t` country
name, released with `string_free`, or NULL if unavailable or ambiguous. The
administrative polygons determine coverage independently of shoreline availability
and land/water classification. Harbour water, inland water and the 500-metre
near-shore band do not suppress a covered country's name or flag code. Missing,
uncovered or overlapping administrative geometry still returns no country; there
is no nearest-country fallback or invented maritime buffer. This does not change
the selected jurisdiction or the land/water classification.
`globe_has_layer(globe, GLOBE_LAYER_COUNTRIES)` reports administrative-layer availability.
`globe_country_code_at()` returns the independently resolved ISO alpha-2 code, or a UK constituent-country code
such as `GB-WLS` or `GB-SCT`.
The Lab joins that code to jurisdiction-owned country names and flag artwork;
it never substitutes the form's selected jurisdiction for a geographic result.

Natural Earth 5.1.1 admin-0 map units are public-domain data at **1:10 million
map scale**, not ten-metre accuracy. The packaged Ukraine and Russia polygons
use the source's Ukrainian viewpoint so that Crimea, including Sevastopol, is
identified as Ukraine, consistent with
[UN General Assembly resolution 68/262](https://digitallibrary.un.org/record/767883).
Other map units retain the source's de facto boundary policy; this is not a
worldwide conversion to UN-recognised boundaries. Names are indicative and are
not legal or navigational determinations. This country correction does not
alter civil time-zone rules or reinterpret occupation as sovereignty.
The detailed shoreline remains unchanged. Small islands and border locations
can lack an answer at this administrative scale.

`globe_import_countries(db, shapes, attributes)` imports open SHP/DBF streams in
a savepoint, preserving unrelated tables and rolling back malformed input.
It creates `country_name`, `country_chunk` and `country_bounds` (an integer
R-tree). Each chunk contains at most 256 edges; lookup selects eastward-ray
candidates and combines ring parity, including holes. Reads are bounded to
10,000 chunks; exceeding that bound yields no name. Connections require the
same serialised ownership as other globe operations.

The imported 5.1.1 layer measured approximately **9.3 MB including shared borders
and spatial indexes** in the isolated SQLite test. Encryption/page layout may change this
slightly; no world-sized country geometry cache is allocated by lookup.

New globe installations include the layer. Existing databases acquire it with
`make update-globe-db`, preserving other layers, or `make install-globe-db` for a fresh database.
Restart MARS Lab afterwards. `/almanac-country` performs only the independent
geographic lookup. The browser requests it asynchronously on selection changes,
cancels superseded requests, ignores stale replies and caches at most 64 results,
with a 1 MiB budget for country-name and flag string contents.
Ordinary pointer movement never starts a country lookup. Shift-hover shows coordinates
immediately, then requests country metadata after a 220 ms pause. Each new point
cancels obsolete work and clears its old label; releasing Shift, dragging or leaving
the globe cancels pending hover work. Hover and selection share the bounded cache,
but retain separate request ownership. Only unidentified positions lack country
metadata; water and coastal classifications do not erase a verified jurisdiction.
Almanac evaluation returns the same verified country to the observer dropdown as
to the result card, replacing an empty or stale selection even if the earlier
asynchronous lookup failed. A compatible regional jurisdiction is retained.
An unidentified evaluation clears stale country and town selections without moving
the observer. Lookup failures leave coordinates usable and never trigger Almanac calculations.

### Optional country borders

The **Borders** button is off by default. It requests only the visible region's
shared land borders asynchronously, using `globe_border_segments()` and the
`/almanac-borders` endpoint. Import matches identical quantised edges belonging
to different map units; unmatched administrative coastlines are omitted. This
deliberately favours omission over invented connections where source topology
does not match. Borders are indicative and use the same country policy as lookup:
there is no international border between Crimea and mainland Ukraine.

`country_border_chunk` stores at most 256 segments per BLOB; `country_border_bounds`
is an integer R-tree. Queries handle the date line and cap output at 250,000
segments. Import matches edges using a bounded ordinary-heap buffer of public
source coordinates, rather than a large SQLCipher temporary table. The buffer
accepts at most 1,048,576 edges (20 MiB of records, plus sorting workspace) and is
released before publication. This avoids exhausting the usual 8 MiB locked-memory
allowance during border matching, without disabling SQLCipher memory protection,
raising system resource limits or writing plaintext scratch tables.
No worldwide vertex cache is needed at runtime. A single 1024×1024 RGBA texture
uses about 4 MiB; new regional imagery replaces it rather than accumulating tiles.
Borders fade between 24× and 64× zoom and unload at 64×, or when disabled. Returning
to a wider view reloads them. Superseded requests are cancelled and stale responses
cannot publish. The detailed shoreline remains untouched.

The 298 map units distinguish England, Wales, Scotland and Northern Ireland.
Shared Welsh and Scottish land borders are included. The Isle of Man has its own
identity and flag; its island edge uses the detailed shoreline, not an invented
administrative or maritime border. Existing installations migrate to revision 4
with `make update-mars-db` (also run by `make release` when Lab is installed).
Revision 4 replaces the country layer atomically and rebuilds shared borders;
databases already at revision 3 retain their shoreline without reimporting it.
River data and database credentials remain unchanged. Older databases also receive
the previously required shoreline update. Failed migrations retain the preceding
data and revision. Restart the Lab and reload the page to discard cached country results.

Source: [Natural Earth admin-0 map units](https://naturalearth.s3.amazonaws.com/10m_cultural/ne_10m_admin_0_map_units.zip),
[public-domain terms](https://www.naturalearthdata.com/about/terms-of-use/).
The DBF attribute stream remains unchanged. The SHP retains all 298 map-unit
records and their ordering; only the UA and RU geometries are replaced from the
pinned [5.1.1 Ukrainian viewpoint SHP](https://raw.githubusercontent.com/nvkelso/natural-earth-vector/v5.1.1/10m_cultural/ne_10m_admin_0_countries_ukr.shp)
and [matching DBF](https://raw.githubusercontent.com/nvkelso/natural-earth-vector/v5.1.1/10m_cultural/ne_10m_admin_0_countries_ukr.dbf).
`tools/mars_config/country_compose.c` performs this offline composition using the
file module. Its five positional arguments are the original map-unit SHP and DBF,
the viewpoint SHP and DBF, and a new output SHP. It matches `ISO_A2_EH`, requires
exactly one UA and RU record in each input, and refuses an existing output. Reuse
the original map-unit DBF and compress the resulting SHP with Zstandard level 19.
It neither downloads data nor opens installed databases. The globe importer and
country regression tests validate the composed polygons before publication.

Original map-unit archive SHA-256: `45bebe2aaf8bf42b9daf4428594925bcb11eabd32fe9dc6e0acf681438053eb5`.
Original compressed SHP SHA-256: `58c3b3b36ebb1ef48d9e375f7c743db90a8b82fae093c64c1d1e269a808a7729`.
Viewpoint SHP SHA-256: `ec923b81a27e36efce7bb293c45ce87134f967330ef137d04e1be9c4940b6d82`.
Viewpoint DBF SHA-256: `4fe3184c24f5cb7516aee1c20ebc6326a3ad6c2158cf844bcb5a1620c3670862`.
Corrected compressed SHP SHA-256: `b7deb7448e69864e898728530a5df345d492fd2db7d4a6e8f4531e12ad7cb66c`.
Packaged DBF SHA-256: `49b3ac0ff0d35a70d6a2d9502a32f45d2df460c7224988e4fe34942401c3c99f`.

Town batches use the same elevation fallback as the jurisdiction catalogue:
missing elevations and legacy `-9999` DEM no-data markers become `0` metres.
Valid negative elevations remain unchanged. The fallback is an assumption for
observer selection, not a measured altitude; users should supply their actual height.

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
An empty or absent key selects plaintext SQLite; a non-empty key selects encrypted SQLCipher.
An explicitly empty environment key overrides a saved key. A failed keyed open never retries without encryption.
It opens only an existing database, with read queries enabled, and closes its
owned connection after releasing layer readers. The almanac's surface APIs and
the Lab's river adapter use this entry point; neither reads legacy geometry from
the astronomical or jurisdiction database. Missing globe storage does not disable
the jurisdiction town catalogue.

### Installation targets

`make install-globe-db` creates a separate shoreline and country database
from the complete validated OSM/GSHHG shoreline and Natural Earth country sources,
with no download. Its default location is
`$MARS_HOME/globe/globe.db` (normally `~/.mars/globe/globe.db`); settings are saved
privately in `config/globe-db.env`. `MARS_GLOBE_DB_PATH` and `MARS_GLOBE_DB_KEY`
override saved values. Fresh installations default to plaintext, without password prompts;
non-empty configured keys retain encryption. Every installation builds a fresh staged database from the packaged
sources, with import progress shown, even when a database already exists. The completed database replaces the
destination only after validation. Existing river and custom tables are removed on successful reinstallation;
failed builds leave the installed database unchanged. The selected path, storage mode and saved river staging
key are retained. Separate river candidate files are not modified.
Staged import, integrity checking and shoreline availability checks must succeed
before publication. Missing packaged geometry is an error, not an empty installation.

`make check-globe-db` checks integrity and shoreline availability with the saved
settings. It does not prove positional accuracy or import other geographic layers.
`make update-globe-db` replaces only the shoreline and country tables in an existing globe
database, using the packaged complete sources without geometry repairs. Both layer imports commit
together; source preparation failures leave the database untouched and
import failures roll back. The saved key, configuration and other layers are
preserved. Stop MARS Lab before updating and restart it afterwards. This is
not a river installation command. `make uninstall-globe-db` still refuses changes.
`make install-mars-db` uses each database's own configured storage mode, defaulting to
plaintext when no key is supplied. Calendar questions may still appear, but passwords
are not prompted for. Standalone installers use the same rules. Installation creates a new database in the
selected storage mode; it does not open or decrypt the previous database. Updating requires its correct key.
The three installations run sequentially, stopping on failure. This is not a transaction spanning three files:
an earlier successful installation remains if a later one fails. Stop the Lab
before rebuilding databases and restart it afterwards. New almanac installations
contain no shoreline geometry; existing copies are not deleted merely by opening
the configured globe.

### Plaintext conversion

Stop the Lab and all database writers before running `mars_config databases-plaintext`. The command processes
globe, jurisdiction and almanac sequentially, stopping at the first failure. Remove path/key environment overrides
first: conversion deliberately uses saved configuration only and refuses overrides that would mask the new settings.
Each encrypted database is exported to a new private file with `.plain.db` appended to its existing pathname.
Schema, spatial indexes, data, `user_version` and `application_id` are copied; integrity is checked before selection.
Only then are the saved path and key changed. No geographic sources are reimported and no river tables are removed.
Other saved settings, including calendar choices, survive. Already-plaintext stores are verified and skipped.

The original encrypted file remains intact. Its old configuration is retained alongside the configuration file
with an `.encrypted-backup` suffix. Both outputs are owner-only. Existing export or backup paths are never overwritten;
inspect retained files after a failed conversion before retrying. Switching configuration is atomic per file, not
across all three databases. Restart the Lab after conversion. For rollback, stop database users and restore the old
configuration; subsequent changes made only to the plaintext copy will not be present in the encrypted original.
Conversion retains the globe's old key as `MARS_GLOBE_RIVER_DB_KEY` for unfinished encrypted river candidates.
River staging/resume and candidate reading use that separate key when present; publication uses the installed
globe's own mode. An explicitly empty `MARS_GLOBE_RIVER_DB_KEY` environment value selects plaintext candidates.
Changing the installed globe does not convert or interrupt separate staging files.

`globe_has_layer()` reports availability through `globe_layer_t`. Shoreline and
future detailed geometry belong in the globe database; towns remain in the
jurisdiction database. New installations include the complete validated shoreline.
Managed globe revision 3 updates existing installations through `make update-mars-db`
or the database-update stage of `make release` when the Lab launcher is installed.
Subsequent release builds skip the already-current globe revision. Standalone
`make update-globe-db` explicitly refreshes the data using the saved settings; `make install-globe-db`
instead rebuilds a fresh database, removing other installed layers.

The complete artifact contains **841,233 rings and 35,346,818 points**. The archive
is packaged as seven sequential `packaging/globe-db/shoreline-20261023.cf64.zst.00`
through `.06` files: six 64 MiB parts and one 14,916,563-byte part. These are byte
segments of one archive, not geographic layers or a runtime source composition.
The installer reassembles and verifies its SHA-256, decompresses at most 602,563,348
bytes, verifies the decoded SHA-256 and imports the lossless `MARSCF64` coordinates.
Missing, damaged or substituted parts fail before geometry replacement. It does
not call offline repair tools, load the historical Sydney correction or silently
fall back to GSHHG. Staging needs approximately 1.02 GB for the archive and decoded
stream, in addition to database and rollback-journal space.

Holding **Shift** while hovering over the globe shows the latitude and longitude
beneath the pointer in degrees and decimal minutes, even with the coordinate grid
disabled. Pressing or releasing Shift updates the tooltip without moving the mouse.
When enabled, the grid retains every line at the selected zoom spacing. Stroke
width is capped relative to that spacing so converging polar meridians cannot
merge into a pale wash. Subtle dark strokes on bright ice and light strokes on
darker surfaces improve contrast without replacing shoreline geometry or imagery.
Hovering does not change the observer or trigger calculations; leaving the globe
or switching windows clears the tooltip.
Town-marker tooltips also include coordinates alongside the full name and country.
Hovering the observer crosshair shows its exact selected coordinates without Shift,
even when the grid is disabled; the crosshair remains transparent to dragging.

The browser uses a directly indexed, jurisdiction-qualified long-name exception
table for display labels. Currently Llanfairpwllgwyngyll is the only entry.
Its full name appears in the tooltip at every zoom level and on the globe from
12× zoom when 430 pixels of label space are available. Narrow views retain the
short label. Expanded labels reserve their full width for decluttering; neither
catalogue identities nor selection values change.

`globe_towns_jurisdiction_batch()` uses indexed membership to read only one
jurisdiction, with the same cursor and callback ownership as `globe_towns_batch()`.
It does not implicitly include parent or child jurisdictions.

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

`globe_hit_test_radius()` uses the same exact geometry with a caller-selected
proximity band from zero to 20,000 metres. It returns `GLOBE_COAST` near a physical
boundary on either side, ignoring virtual polygon closures. Distances use a local
tangent-plane approximation. Invalid radii and unavailable or corrupt data return
`GLOBE_UNKNOWN`. This does not change the default 500-metre classification, alter
polygons, establish harbour membership or assign maritime jurisdiction.

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
Cold regions spanning at most 90° on each axis read indexed candidate rings directly;
they do not first decode the global dataset or build its display resolutions. Candidate
selection includes eastward ray crossings so enclosing land and lakes remain filled.
Existing resident geometry is reused, and wider requests still prepare the shared cache.

Bounds may cross the date line: west is between −360° and 180°, east is greater
than west by at most 360°, and latitude bounds lie within −90° to 90° with north
greater than south. Display tolerance is between 0° and 1°; zero retains every
vertex. Clipping intersections retain the exact bound coordinate, and simplification
preserves transitions onto and off clipping boundaries to avoid artificial slivers.
Clipped concave polygons may contain retraced, zero-width joins: these contribute
no filled area and must not be interpreted as physical coastline strokes.
This tolerance never changes hit-test classification. Discard partial
output when a query fails or its visitor cancels.

`globe_import_shoreline()` transactionally replaces shoreline tables from an
open GSHHG 2.3.7 binary stream or a lossless `MARSCF64` stream. It retains unrelated tables and rolls back
malformed input. Its savepoint also permits a caller-owned transaction, so the
installer publishes geometry and correction provenance atomically.
Stored uncompressed coordinate chunks use BLOB pages and an
integer R-tree; older compressed chunks remain readable. Levels 1–5 are retained,
using the Antarctic ice front rather than the alternative grounding line.

Lossless streams begin with the eight ASCII bytes `MARSCF64`, followed by the
same 44-byte polygon headers and big-endian IEEE binary64 longitude/latitude
pairs in degrees. Header bounds are outward-rounded integer microdegrees; they
do not reduce the coordinate precision. Version 2 datasets use raw binary64
chunks (codec 2) or bounded Zstandard frames of the same pairs (codec 3).
Version 1 and its integer codecs remain readable. Each chunk still contains
at most 257 points and each page at most 64 KiB; binary64 chunks are at most
4,112 bytes. Non-finite or out-of-range coordinates are rejected. Import checks
encoding and bounds, not topology: the source still requires geographic validation.

For an exactly preserved polar-complete polygon, flag bit 24 marks an explicit
virtual closure. Header words at byte offsets 28 and 32 give its first edge and
edge count, wrapping modulo the number of non-duplicated ring vertices. This
extension requires a closed level-1 polygon spanning both dateline edges and
reaching 90°S. The importer splits chunks at physical/virtual transitions without
moving vertices: virtual edges contribute to land filling but not coastline
classification. Legacy level-5 ice-front streams retain their existing implicit
polar-cap behaviour. Older importers do not understand the explicit marker and
must not be used to install these new artifacts.

Indexed hit-testing and resident full-resolution paths now retain coordinates
in degrees without rounding through the microdegree grid. Spatial indexes use
outward-rounded bounds, so sub-microdegree islands are not excluded by truncation.
This addresses the 35 rounding failures without removing rings or moving their
vertices: the already validated source coordinates can be retained exactly.
Binary64 uses 16 raw bytes per coordinate pair rather than 8, and resident
vertices use the same width. Installation and the resident point budget must
account for that increase; the lossless codec alone is not installation approval.
The optional native `check_shoreline_precision` diagnostic reads a completed
quantisation-failure audit and staging database read-only, then checks bit-exact
encoding and decoding of every original vertex in every rejected ring. The
ordinary shoreline tests cover tiny-ring preservation through import and paths,
full-sized chunks, compressed reads, cold and resident queries, malformed-input
rollback and legacy world-data compatibility.
The read-only check of the 23 October audit passed for all 35 rejected rings:
7,600,134 original coordinate pairs survived native binary64 encoding and decoding
bit-for-bit. This removes the need to alter those rings to fit a coarse integer
grid. The installed globe is still unchanged; the exporter must use the lossless
format and the remaining installation acceptance checks still apply.

Offline `coastline_export` now streams the effective eight-metre candidate into
an exclusive `MARSCF64` intermediate. It consumes the completed boundary review's
chunks in their indexed order and compares every coordinate, including periodic
date-line copies, with the geometry being exported. It rejects stale geometry,
missing or extra chunks, mismatched totals and unsuccessful reviews. Original
binary64 coordinate bits are written unchanged; polygon identifiers are assigned
sequentially for the native stream. The exporter performs no geometry repair.
Its self-test covers exact tiny-ring preservation, the effective repair overlay,
native import compatibility, rejection of stale and incomplete reviews, and
refusal to overwrite an existing output. Failed outputs remain incomplete and
must not be installed. The initial coastline-only intermediate required inland-water
composition, nesting and polar-boundary acceptance before packaging. The complete
accepted artifact described below includes those stages. The normal importer consumes a finished validated
artifact, not invoke these offline preparation tools or repair bad geometry.
The exporter accepts completed boundary reviews produced before the optional
`invalid_rings` quantisation-diagnostics table was introduced. When that table
exists it must be empty; missing required review tables, unsuccessful status,
non-zero contacts and geometry mismatches still reject the candidate. The first
worldwide export stopped at preflight because it incorrectly required this newer
table in the older fourth-pass review. Its empty output is retained; the retry
uses a distinct output and log without modifying the accepted review database.

The retry completed: 833,417 rings and 34,485,968 coastline coordinate pairs,
verified against 924,803 reviewed chunks, occupy 588,445,844 bytes in the
coastline-only intermediate. This is not yet the installed shoreline.
Offline `coastline_compose` combines this intermediate with the existing GSHHG
level-two to level-four inland rings in a separate, exclusively created staging
database. It preserves closed coordinate sequences, closes open legacy rings
without moving vertices, and records source identifiers, levels, orientation,
area, polar bounds and invalid-ring diagnostics. It never repairs geometry or
opens the installed globe. Its targeted self-test covers nested levels, invalid
rings, exact binary64 coordinates, legacy longitude wrapping, level filtering,
open legacy closure and truncated inputs (including skipped levels).
Composition success means only that the candidate is ready for cross-ring and
nesting review; it does not approve installation. The bounded background worker
runs composition and the boundary reviewer sequentially, records input and
executable checksums, and stops on failure. Nesting, polar interpretation and
native-reader resource acceptance remain separate installation gates.

The first composition completed with 841,524 rings and 35,377,402 coordinate
pairs, but boundary review found 556 crossing-chunk records across 337 ring
pairs. All involved legacy inland rings meeting the new coastline: 174 legacy
rings and 255 coastline rings. No individual ring was invalid. The candidate
was rejected and not installed.

Offline `coastline_reconcile` implements the subsequently authorised policy that
the reviewed coastline takes precedence at source conflicts. It works on an
exclusive copy, supersedes crossing legacy rings, and records their original WKB,
metadata and the corresponding coastline ring identifiers. This is whole-ring
source replacement, not vertex snapping or a claim that every old water outline
survives. Geographic coverage review of these replacements remains mandatory.
All retained coordinate sequences remain unchanged. The tool builds nesting
using R-tree envelope queries and prepared containment tests, including periodic
longitude copies. Since crossing rings have been excluded, an interior sample
establishes containment among the remaining non-crossing rings. Equal land/water
classifications identify redundant legacy coverage. Nested coastline land rings
require an additional full-polygon coverage proof: the enclosing high-resolution
land must cover the entire inner polygon, including periodic longitude handling.
Only then can its redundant inner outline be omitted without changing the union
of land coverage. Its geometry and the containment proof remain in the audit,
and its lakes and islands are reparented rather than discarded. No ring is flipped
to water, no coordinate is moved, and older data does not override the new coastline.
Nested levels are recomputed, and unsupported
depth or inconsistent coastline orientation fails preparation. Removed ancestors
are bypassed through indexed parent links so their useful descendants survive.
Self-tests cover both directions of duplicate nesting, preserved islands and
ponds, periodic longitudes, exact retained coordinates, original-geometry audit,
full-polygon containment, preservation of descendants of redundant coastline land,
and rejection of partial coverage, same-source crossings and inconsistent coastline nesting.
The first reconciliation stopped at the first of 63 land-inside-land coastline
pairs. An independent ray test confirmed that all nine vertices of ring 66168
lie within ring 28012; this was not a containment-test error. The next pass requires
the stronger full-polygon proof for each such redundancy and retains the failed
candidate separately. Geographic acceptance remains outstanding.
The second pass progressed through redundant land coverage but stopped at five
clockwise, uncontained coastline shells. They map to closed source coastline ways
1567122037, 1567122042, 1567122046, 1567136415 and 1567136428 from the 12 October
extract; none came from the repair overlay. Their source edits date to 10 October.
The preparer now treats an uncontained coastline shell as land independently of
winding, recording the old and new classification in `classification_overrides`.
This is a topology-based interpretation, not independent geographic verification.
Coordinates and original winding remain unchanged, and enclosed clockwise water
rings are not reclassified. Tests cover clockwise outer land containing a lake,
exact geometry preservation, audit records and continued rejection of inconsistent
nested water. The third pass uses new output paths; earlier failed candidates
remain available for comparison. The normal importer performs none of this work.

The third reconciliation and boundary review passed with 841,226 rings and
35,340,423 points. Subsequent acceptance found the Antarctic polygon closed at
approximately 85.051129°S rather than the South Pole. Offline `coastline_polar`
therefore makes one candidate with the newer polygon retained north of its exact
coverage limit and the packaged GSHHG ice-front coverage retained south of it.
It checks equality of land coverage on each side and requires a valid single
polygon without holes. Original geometry, source identifier and cutoff are
recorded in `polar_completion`; a fresh global boundary review is required.
The legacy source contains an irrelevant self-intersection near 75°S. Its ring
is clipped to the required southern half-plane before validation, excluding that
northern defect. The actual retained legacy fallback is a five-coordinate polar
cap. No normal import performs this merge. Final export must distinguish synthetic
polar closure from physical coastline, and a complete native trial import remains
required before installation.

The first polar completion correctly rejected a disconnected union: the source's
synthetic southern closure slopes from 85.0511289°S to 85.05°S. Using its minimum
latitude made the fallback meet it at only one point. The preparer now recognises
the connected, full-width synthetic closure and uses its northernmost latitude,
85.05°S, as the complete-coverage cutoff. It rejects an unrecognised closure rather
than guessing. The northern and southern coverage comparisons still require zero
area difference; no tolerance was relaxed and no coastline vertex was snapped.
A regression reproduces the disconnected result and checks the corrected union.
The read-only `--check-merge` diagnostic passed on the actual Antarctic geometry:
one valid polygon, no holes, and equal regional coverage. The second full polar
run completed successfully with 841,226 rings, 35,340,425 points and zero boundary
contacts, retaining the failed first output.

The offline exporter now supports complete compositions through its `--complete`
mode, preserving nesting levels and positive legacy longitudes. It checks each
coordinate against the fresh review and recognises the single artificial polar
closure without changing the reviewed polygon. The optional `coastline_trial`
tool imports into an exclusive encrypted database with an unrelated sentinel
table, compares every stored coordinate and boundary flag against the artifact,
checks integrity and representative geography, and prepares the full native
surface cache. The capped export/trial worker has no runtime deadline and does
not install anything. Successful export and trial results are required
before packaging the artifact and changing the managed database revision; the
second marine composition completed these checks on 23 October 2026.

The first complete export and native import passed, including exact readback of
all 35,340,425 points. Geographic acceptance then exposed an incorrect expectation:
the Málaga centre probe is on land but 375.4 metres from the mapped coastline,
inside the reader's intentional 500-metre uncertainty band. Acceptance now checks
strict containment independently and verifies proximity-aware classification
against that result; a coastal classification alone cannot pass the land/water
check. All probe sites use this distinction. Small nested-polygon regressions
cover both sides of land, lake and island boundaries before and after cache
preparation. Read-only `--resume` mode repeats exact readback, integrity checks,
geographic acceptance and cache checks on the existing trial database. No import
or source preparation is repeated, and no installation occurs.

The resumed trial passed Málaga and the polar checks but exposed missing Sydney
Harbour water coverage. Both failed water probes lie inside the unsimplified OSM
coastline shell as well as the simplified candidate. The separately retained OSM
marine-water relation 1252425, version 123, covers both points. Its geometry was
not part of the coastline-only source selection; the old Sydney correction is
not automatically carried into a replacement coastline.

The optional offline `coastline_water` preparer incorporates a supplied valid
marine-water GeoJSON polygon into an exclusive candidate copy. It spatially
selects intersecting level-1 shells, subtracts the water and assigns resulting
shells and holes their nesting levels using a spatial tree. It rejects overlapping
nested layers, polar edits, invalid geometry and nesting deeper than four levels.
Source GeoJSON, original geometry and replacement identifiers are retained in
`marine_source`, `marine_original` and `marine_replacements`. The input coordinates
are not snapped or simplified; computed intersection coordinates retain binary64
precision. A repeated-overlay coverage check permits only discrepancy area below
1e-14 square degrees that is wholly within 1e-9 degrees (at most 0.12 mm) of the
original input boundaries. This numerical verification allowance never changes
output coordinates and is separate from the eight-metre simplification ceiling.

The first full marine review rejected one contact: simplification had replaced
the harbour-mouth source path with a straight edge, creating a false triangular
island when the unsimplified water polygon was subtracted. The optional source-join
GeoJSON restores the original nodes across exactly one matching simplified edge,
in either direction. Every restored node must lie within a conservative eight-metre
bound of that edge; endpoints must match exactly, and the resulting polygon must
be valid. This is an offline source restoration, not snapping or removal of small
islands. The source path is retained in `marine_join`; the original land geometry
remains in `marine_original`. Sydney uses original OSM ways 576802264, 899661018
and the first four nodes of 153496653. The preparer also rejects touching output
boundaries locally, before the mandatory global review.

Read-only Sydney composition with that restoration passes, replacing one mainland
ring with eight rings. The second full run completed successfully on 23 October
2026: global boundary review, lossless export, exact native readback of all
35,346,818 points, all 16 geographic probes, full surface-cache construction and
all 16 cached probes passed. Compression and archive verification also passed.
Small regressions cover enclosed water with an island, sea-connected water with
an island, database storage, preservation of unrelated tables and rollback of an
unsupported nested-layer intersection, together with source-join direction,
displacement and endpoint checks and rejection of touching components.
The accepted complete candidate passed fresh
global boundary review, complete export, exact native readback and geographic and
surface-cache acceptance. All these stages ran sequentially under the capped
marine-composition worker without a runtime deadline; normal import performs none
of this source composition or repair work.

The resident reader now permits up to 64 million stored chunk vertices only within
a 1.5 GiB geometry-array budget. This includes vertices, boundary flags, ring
metadata, tree compaction and simultaneous old/new display-index allocations.
The budget is not a process RSS guarantee: SQLite, allocator overhead, other
retained geometries and application working memory are additional. Targeted tests
cover the higher-resolution count, oversized display-index growth and arithmetic
overflow, alongside existing lossless, nested-ring and packaged-world regressions.
The capped background worker then runs a fresh boundary review. Neither tool
is called by the normal importer, and neither approves or performs installation.

See [almanac data provenance](almanac-data-provenance.md#shoreline-data) for
source, checksums and licence notices.

### Reviewed local shoreline corrections

The former GSHHG installer applied `packaging/globe-db/shoreline-corrections.json` to a
temporary copy of the upstream binary stream before import. This historical
correction and its tests are retained for provenance, but revision 3 does not use
them: Sydney harbour water is already present in the complete artifact. The original archive
is unchanged. Corrections are sorted by polygon ID and specify an original point
count, a zero-based inclusive vertex span and replacement microdegree coordinates. Both
endpoints must match the original exactly; coordinates must remain within its
bounds. Unknown revisions, missing polygons, changed source counts and invalid
endpoints fail before database import. Preparation uses a 64 KiB copy buffer,
at most 32 MB for a corrected source ring, and a bounded correction document.

Revision 1 corrects the southern Sydney Harbour waterfront from Woolloomooloo
through the CBD, Darling Harbour and Rozelle Bay to Balmain. GSHHG placed the
unchanged Sydney catalogue coordinate (33.8678° S, 151.2073° E) outside its land polygon.
The replacement uses [OSM Sydney Harbour relation 1252425](https://www.openstreetmap.org/relation/1252425),
version 123, retrieved on 21 October 2026. Harbour banks are represented by a
water multipolygon, not necessarily by `natural=coastline` ways; coastline-only
extraction misses them. No town-coordinate offset or town-specific rendering
rule is applied.

The eastern join intersects the original edge. The western join retains the
original Balmain endpoint, about 18 metres from the OSM bank. The retained OSM
vertices are rounded to integer microdegrees, without smoothing or downsampling.
This local seam is not a claim of surveyed accuracy or a worldwide coastline
upgrade. The corrected stream adds 1,365 vertices (10,920 raw coordinate bytes),
bringing its imported point count to 10,589,219. Geographic regressions check
Sydney and nearby land, plus water in Darling Harbour, Circular Quay and
Woolloomooloo Bay at two display scales, as well as cold and resident hit-tests.

The correction is © OpenStreetMap contributors, available under the
[Open Database Licence 1.0](https://www.openstreetmap.org/copyright).
GSHHG's existing licence and attribution remain applicable to the base source.

### OSM coastline candidate: detail budget and acceptance

The optional offline `tools/mars_config/coastline_stage.c` preparer joins the
audited way-reference OPL file to coordinate-bearing OPL (or a GeoJSON sequence)
by OSM way ID. Coordinate-bearing OPL is preferred: a lines-only GeoJSON export
can omit closed coastline ways tagged as areas, and area construction can change
ring direction. The completeness audit rejects an export with missing ways.
Its SQLite staging file retains unmodified WKB geometry, original point counts
and first/last OSM node identities. Unique endpoint indexes reject branching or
duplicate joins; indexed audits reject missing geometry and unmatched endpoints.
Coincident coordinates never manufacture a connection between different nodes.
The output starts marked `incomplete`. Following the coordinate and endpoint
audit, indexed traversal assigns every way to a directed closed ring and records
ring point counts without loading worldwide coordinates into memory. Successful
staging marks it `closed_ring_topology_checked`, **not** polygon-valid or installable.
The optional build target accepts `GEOS_CFLAGS` for the GEOS C headers and links
GEOS only into this preparation executable, never into MARS at runtime. Its
`--self-test` checks sequence parsing, source-point reconciliation and rejection
of disconnected node identities. Existing staging files are never overwritten.
The separate `--validate-rings` pass records individual polygon validity and
diagnostics in `ring_validation`; it does not approve publication. It unwraps
date-line crossings and checks coordinate agreement at node joins. Rings above
two million points, polar closures and construction failures are explicitly
deferred for separate bounded processing, never silently accepted. Cross-ring
relationships and simplification still require further validation. The native
self-test also checks a valid triangle and rejection of a self-crossing ring.
The separate `--validate-large-rings` pass permits up to eight million points per
ring under an external memory cap and retries deferred rings. Completed validation
batches are checkpointed. The complete repaired extract was assembled into
833,418 directed rings containing 78,988,079 points after shared way endpoints
were removed. All passed individual-ring validity on 22 October 2026, including
the two continental rings of 7,109,768 and 7,274,308 points. This supersedes the
earlier inability to complete worldwide ring construction, but does not establish
cross-ring relationships, simplification safety or installation readiness.

`--simplify-rings` accepts a staged file and a tolerance in metres, restricted to
0.5–40 m. It writes separately keyed, resumable `simplified_rings` candidates,
retaining source coordinates. The angular tolerance uses a conservative
111,700 metres per degree for both axes; it is not a positional-accuracy claim.
Each candidate must remain a valid polygon without holes and retain its source
orientation, otherwise the original ring is retained. Global cross-ring checks,
waterway checks, point-budget measurements and visual comparisons are still
required before publication. This command never updates the installed globe.
For rings above 500,000 points, boundary-only Douglas–Peucker simplification
avoids the topology-preserving simplifier's large segment index. It does not run
polygon repair: invalid or orientation-changing results fall back to the source.
The review tool's `--classify-contacts` mode reads an existing review database without
changing it and separates chunk-level touches, crossings, overlaps and other
contacts. These are diagnostic categories, not installation approval: chunk endpoints
can affect the classification, and source geometry still needs comparison.
The first local-section repair restored 76 small rings and six large-ring regions.
Its complete review reduced 530 contacts to 64 (8 touches, 38 crossings, 16 overlaps
and 2 other contacts), with 34,477,698 candidate points. It is not installed or approved.
Of the 23 contacts between unchanged source rings, 22 lie on the artificial date-line
split and one is a genuine crossing near Vancouver Island. Neither category is silently
waived by the validator. The other 41 contacts involve simplified boundaries.
Later repair passes must retain earlier repair regions. The native review tool's
`--merge-contacts NEW-regions.sqlite3 OLD-review.sqlite3 NEW-review.sqlite3` mode copies
contact geometry into an exclusive new, transactional cumulative repair input. It rejects
incomplete reviews, leaves inputs unchanged and marks the output as repair regions only,
not a fresh validation. This prevents a later pass from undoing earlier local restoration.
The second and third cumulative passes reduced the contact counts to 29 and 27
respectively. The third candidate has 34,485,970 points. Its remaining contacts
comprise 26 date-line joins and one crossing between unchanged source island rings.
The fourth pass uses a coverage-preserving source repair overlay and a seam-aware
review; it keeps the memory, CPU, no-swap and I/O caps with no runtime deadline.
That pass completed successfully on 23 October 2026: 833,417 effective rings,
34,485,968 points, 768,862 neighbour comparisons and zero unresolved boundary
contacts. The 26 complementary date-line joins are recorded separately. Both
original island records remain in the repair overlay. The result is explicitly
`boundary_contacts_clear_not_install_approved`, not a delivered shoreline update.
At that stage the installer still imported GSHHG plus the Sydney correction.
The subsequent complete-artifact work described above preserved inland waters,
checked nesting, adopted lossless coordinates instead of microdegree quantisation,
and raised the reader and decompression budgets before managed revision 3.
Visual verification in the user's rebuilt Lab remains separate from the native
acceptance checks.
The review tool's optional final `--microdegrees` flag rounds the effective
candidate to the globe's stored integer-microdegree grid before checking validity
and cross-ring contacts. It retains the exact reviewed geometry in `quantized_rings`
for subsequent export, without changing the staging or installed databases. A
collapsed or invalid rounded ring leaves the review incomplete; rounding-induced
contacts fail the review. Native tests cover exact coordinate conversion, collapse
and a contact introduced by rounding. The capped installation-check launcher is
prepared for this global review; it is not an installation command.
The first stored-coordinate review failed on ring 2955146 after microdegree
conversion, before completing the boundary comparison. The installed globe is
unchanged. The next audit records every invalid rounded ring, its GEOS reason
and error coordinates in `invalid_rings`, rather than stopping at the first one.
Any such ring gives the result `quantization_invalid_not_install_approved` and
prevents the boundary comparison from running on an incomplete subset. An I/O
or database failure still stops the run as incomplete; it is not a geometry repair.
The full second microdegree audit completed on 23 October 2026 and rejected 35
of 833,417 effective rings: 33 self-intersections, one ring self-intersection and
one collapsed component with too few points. Diagnostics are retained in
`coastline-261012-microdegree-audit-r2-8m.sqlite3`. This is a completed failure
audit, not a timeout; no global boundary comparison or installation was approved.
The next repair must address stored-coordinate topology, including the collapsed
component, without silently dropping rings or closing water passages.
Installation remains blocked until the contacts, nesting, water-preservation and export
checks pass; a successful repair pass alone does not authorise the candidate.
The same cross-ring and water-preservation acceptance checks still apply. Rings
above 100,000 points are individually checkpointed so a resource-limit failure
does not discard all preceding continental work.
If a large boundary-only candidate is invalid, independent sections of at most
16,385 vertices use topology-preserving simplification. Original section endpoints
are retained. Whole-ring validity checks locate any remaining contacts and an
STRtree finds affected sections, which are restored from the original geometry.
After at most 256 attempts the tool falls back to the complete original ring;
it never repairs invalid polygons by inventing components. The separate
`--refine-large-rings` mode retries only previously retained large source rings,
preserving all other candidate records and checkpointing replacements individually.

The optional offline `coastline_review` executable accepts a completed staging
database, its simplification tolerance and a new review-database path. It refuses
incomplete candidates and never overwrites an existing review. It validates each
candidate ring and divides boundaries into overlapping chunks of at most 256
vertices. An R-tree restricts cross-ring contact tests to spatial neighbours;
periodic copies also test contacts across the date line. Results and offending
chunk/ring pairs are recorded separately from the read-only source. A clear result
is deliberately labelled `boundary_contacts_clear_not_install_approved`: it does
not establish unchanged nesting, water preservation, acceptable point counts or
export correctness. Its native self-test covers ordinary and date-line crossings,
separated rings and rejection of a self-crossing polygon. No mutable global state
or installed-database mutation is involved.

The reviewer records complementary date-line joins separately in `seam_contacts`.
Both complete polygon envelopes must lie on opposite sides of exactly the same
±180-degree boundary, and their entire intersection must lie on that boundary.
Same-side contacts, near-seam contacts and crossings remain errors. This does not
assert global seam completeness or replace nesting and water-preservation checks.

The offline `coastline_repair` tool takes a staging database, tolerance, completed
contact review and exclusive new overlay path. It unions only overlapping original
fallback polygons of at most 500,000 points each, with equal orientation, and rejects
holes, disconnected unions and reused repair members. The overlay stores both original
WKB records, the replacement and the absorbed-ring mapping; the source database is
unchanged. The review tool accepts this overlay as its optional fourth argument,
checks its original WKB against the current candidate and validates the effective
geometry rather than counting an absorbed island twice. Stale overlays are rejected.
An overlay is not an installed shoreline: export must consume the same effective
geometry, and all other acceptance checks remain required. Native self-tests cover
coverage preservation, reversed orientation, separated polygons, overlay provenance,
effective candidate selection and stale-overlay rejection.

`coastline_stage --restore-small-contacts` takes the staging database, tolerance
and a completed contact-review database. For contacting rings of at most 500,000
source points it restores valid original geometry, archiving the replaced
candidate rows in `contact_restoration_prior`. Larger rings remain unchanged.
This is a conservative preparation step, not automatic approval: the entire
candidate must undergo a fresh boundary review and point-budget check afterwards.

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
The importer validates each bounded record's JSON syntax before decoding metadata
with the ordinary JSON module. Coordinate and node-reference arrays are read
directly into a reused native buffer, avoiding arbitrary-precision number trees
for geographic values. Coordinate conversion uses a private C numeric locale;
node IDs retain their exact positive 64-bit values. The existing 100,000-vertex
per-way ceiling remains, with a 128-level JSON nesting limit. Reordered and escaped
field names and last-value-wins duplicate fields retain their normal interpretation.
Input and junction passes report elapsed times separately from simplification.
Record validation or storage failures report the one-based input record number
and byte length before rolling back, allowing bounded diagnostic replays without
changing installed data. Unicode names, including leading combining marks, remain
valid metadata and do not cause river sections to be discarded.
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
