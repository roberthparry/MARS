# MARS Lab

The Almanac's Location cell combines an identified land country and its flag,
with the town name when selected; there is no separate Country cell. Offshore
custom points read “Custom — Offshore”. The scientific surface classification
remains “Sea” for horizon-dip calculations. Map clicks clear stale town/country
selections immediately, then asynchronously select an identified land country
without replacing the clicked coordinates with a town's coordinates. Uncertain
or offshore points leave the country menu at “Country not identified”. Offshore
evaluation replies also clear stale country and town selections; an empty returned
jurisdiction never restores a default country. An authored
time zone and observer altitude are preserved; a country alone cannot establish
a precise time zone or elevation.

Shift-hover coordinates also show the independently resolved land country and its
offline flag after a short pointer pause. Requests are asynchronous and cancelled
on movement or release. Sea or uncertain locations retain coordinates only;
hovering never changes the observer or runs calculations.

The optional globe borders include Wales and Scotland. Wales, Scotland and the
Isle of Man use their own jurisdiction-owned flags in town and selected-position
tooltips and the Navigation Almanac card. The Isle of Man's island edge remains
the detailed shoreline. Existing databases receive these additions through
`make update-mars-db`, also invoked by `make release` when Lab is installed.

Town labels on the globe and Town/Location dropdown rows use the same asynchronous
flag tooltip cache, including keyboard focus. Rows carry small jurisdiction codes,
not copies of flag artwork. Globe redraws retain the custom accessible tooltip
without reintroducing a competing plain browser title.
Town dropdown labels and tooltips omit GeoNames disambiguation IDs; stored town identities and source
attribution remain unchanged. Other descriptive parts of a town name are retained.

Country/jurisdiction dropdowns also show available flags beside visible entries
and the selected label, as well as in their tooltips. Opening or focusing the menu
starts lazy observation: artwork loads sequentially for visible rows, sharing the
32-entry, 1 MiB tooltip cache. Concurrent requests for the same country share one
asynchronous fetch. Cached flags are restored immediately after selection, even
while a different country's artwork is loading. A shared request
is cancelled when its last consumer leaves. Artwork decoding
is asynchronous, and textual controls remain usable while flags arrive. Database
lookups use the existing indexed country code, without a separate BLOB-offset format.
Hidden row images are released. Missing artwork
leaves text-only labels; subdivision entries use the catalogue's flag fallback.

MARS Lab is the browser-based graphical client supplied with MARS. It provides
one workspace for expressions, equations, differential equations, matrices,
symbolic and numerical integration, civil date calculations and the
astronomical almanac. The browser is the presentation layer: mathematical work
is sent to the native server's built-in calculation modes, which use MARSlib.

## Database updates during release builds

After a successful build, `make release` runs `make update-mars-db` if the configured
MARS Lab launcher is installed and executable. Without an installed launcher the
release remains build-only. The updater checks existing globe, almanac and
jurisdiction databases sequentially, using their saved paths and keys without
password prompts or configuration changes. Missing databases are skipped; install
them explicitly with `make install-mars-db`.

Updates are explicit, versioned migrations, not automatic reruns of the database
installers. The current migrations refresh packaged shoreline/country geometry and
jurisdiction flag metadata; the almanac has no pending migration. Future changes
must register a migration and increase its managed-data revision. Source edits alone
do not trigger a rebuild. Imported rivers, calendar settings and unrelated tables
are preserved. River candidates are never published by a release build.

Each database commits its changes and revision together. Current databases require
no source import. Failed migrations roll back and fail the release; earlier successful
database updates remain committed and are skipped when retrying. Invalid credentials,
locked databases and revisions newer than this build are errors, never reasons to
recreate a database. Close and reopen MARS Lab after updating so its caches are refreshed;
the updater does not stop running applications automatically.

## Native calculation control

Each built-in calculation process enters a fresh [`computation_t`](computation.md)
scope. Expression evaluation and symbolic checkpoints, and nested numerical
integration, share cancellation, a monotonic deadline and optional work accounting.
Set `MARS_LAB_WORK_LIMIT` in the server environment to a non-negative integer;
the default zero means unlimited work. This algorithm-dependent allowance is not
the Integrator card's subdivision ceiling and is not a wall-clock performance measure.
Invalid configuration fails the calculation explicitly.

When the parent observes cancellation or timeout, it sends SIGTERM and allows
up to 250 ms for checkpoint-driven cleanup before terminating and reaping the
process group. The child's signal handler only sets a flag. Existing output limits
and hard process supervision remain necessary for unchecked operations and numerical
primitives. Diagnostic executable overrides retain immediate termination and do not
receive the built-in computation scope.

The server polls the request socket during calculation supervision. Browser aborts
that close the connection therefore request cancellation; this is best-effort
through reverse proxies, which may retain the upstream connection. Lab requests
must not half-close their sending side while awaiting a calculation response.
This policy is opt-in at the Lab route layer, not a general webserver restriction.

Stopped calculations return distinct `ECANCELED`, `ETIMEDOUT`, `E2BIG` (work budget)
or `EIO` (control failure) error codes through the existing Protobuf response.
Partially produced mathematical output is not published as a successful result.
Native progress callbacks are available to library callers; this does not add
streaming browser progress or promise a percentage-complete estimate.

## Installing and starting the Lab

MARS Lab runs as a native C server with calculation modes linked into that same
executable. It launches itself with `--worker-control TIMEOUT_MS WORK_LIMIT MODE` for isolated calculations;
no separate scratch worker binaries are required.

The browser runs C/WebAssembly with TypeScript-authored browser adapters and HTML/CSS, served from
`tools/mars_lab/assets/index.html`, with its stylesheet in `tools/mars_lab/assets/index.css`
(served as `/index.css`), an ordered import list for the component styles in `assets/css/`.
Browser source lives in `tools/mars_lab/typescript/`. The native TypeScript 7 compiler
emits JavaScript into `build/typescript/`; those generated files are ignored by Git and
must not be edited. The server maps the existing `/js/` URLs to that build directory,
so `assets/` contains authored assets only. The formatted
`src/page/lab_page_catalogue.c` contains packaged worksheet defaults and presentation constants as native C tables.
Mathematical processing remains in the native C server and workers. The stylesheet is served
with `text/css; charset=utf-8` and the same private-access and no-cache rules as
the page. No Python interpreter is launched by the Lab.
Database and weather installation use the native `mars_config` application.
`make install-mars-lab` labels its desktop installation, cache refresh, database
and weather-configuration stages. Database installation displays live source-byte
progress bars and elapsed-time activity indicators for unmeasured work, so long
imports and calendar generation remain visibly active. When output is redirected,
plain status lines replace animated bars. Password and selection prompts remain
unobstructed; a failed stage does not display successful completion.

Install the native TypeScript 7 compiler as `tsc` on PATH, or supply its absolute
path through the Make variable `TSC`; `LAB_TSC` overrides it for MARS Lab alone.
The native compiler requires neither Node.js nor npm. Run
`make typescript-install-help` for [copyable installation commands](building.md#native-typescript-compiler);
the Debian/Ubuntu `node-typescript` package is not the native compiler.
`make check-deps` and
`make check-lab-deps` check compiler availability and version; compilation and
typechecking targets run the same preflight check. Missing or unusable compilers
produce an actionable error, while successful compiler checks print the version.
`make check-deps` also ends with an explicit confirmation that all its dependency
checks passed, including the C development libraries and the WASM C library/toolchain.
Lab builds compile the browser sources automatically;
`lab-typecheck` checks both the application and browser tests strictly without
emitting JavaScript. Browser tests are authored in `tests/browser/*.ts`;
`lab-test-typescript` compiles them into the ignored `build/typescript_checks/`
directory. The browser-test target performs this compilation automatically before
the native harness loads the generated scripts.
The editor discovers `tests/browser/tsconfig.json`, which inherits the same test
configuration with emission disabled. This includes the application declarations
when editing a browser fixture rather than treating it as an isolated script.
Compilation errors prevent publication of new generated files. There is no
bundler, package-manager install step or JavaScript server runtime.

Browser resource owners are classes: startup manages bootstrap data, observers and
periodic work; the transport bridge scopes native handles; request controllers own
cancellation resources; and plot controllers own rendering and refinement resources.
The HTTP client receives explicit network and response-decoding capabilities. Codec
and worker communication use typed ES modules. Worksheet namespaces group related
controller classes and their application instances; behaviour belongs to those
instances rather than a collection of namespace functions or static methods.
These definition scripts require no bundler. Controllers receive typed capabilities,
with live accessors where bootstrap ordering or current native state requires them.
Application instances carry explicit class types so mutually dependent initialisers
do not lose their types in the editor's TypeScript checker. Compile-time regression
checks guard the transport, request, result and plot owners against this loss of typing.
Pure validation and formatting helpers remain functions rather than artificial
utility classes. Native code continues to own mathematical interpretation and
worksheet policy.

The native workflow runtime combines domain capability providers into an immutable,
directly indexed registry and rejects duplicate service names. It executes native
continuations without taking ownership of editor, calendar, history or result state.
Persistence and history adapters retain native-selected validation, snapshot timing
and request guards. Browser fixtures can construct isolated controllers with fake
capabilities; integration fixtures replace methods on their actual owners, not on
`window`.

Worksheet buffer transfer, UTF-8 validation, snapshot contracts and native-state
views are grouped in `typescript/workspace_native.ts`. Integrator reference metadata
belongs to its controller rather than a shared mutable global. Browser capabilities
use explicit request options and service signatures; opaque values are retained only
at the native transport boundary. Application disposal releases fallback resize
listeners, native event subscriptions, codec workers, observers and owned timers.
Plot-view disposal is separate from permanent renderer shutdown so changing a result
can release geometry without losing the application's resize subscriptions.

Browser-only presentation and binding state belong to their respective controllers;
the worksheet session retains only editor timestamps and calendar data. These typed
owners are independent of the native-state views and do not duplicate the native
mathematical model. TypeScript file headers
and public APIs carry Doxygen-style comments documenting their responsibilities.

Install the external TeX rendering tools and freestanding WebAssembly toolchain as follows:

```sh
sudo apt install texlive-latex-base dvisvgm clang lld wasi-libc
make mars-lab
```

```text
MARS Lab (native C) running at http://127.0.0.1:<port>/
```

The port is selected automatically. Use `make mars-lab ARGS='--help'` to see the
command-line options, including a fixed numeric host or port, `--workers`
(one through eight; default four), and `--no-browser`. The default binding is
IPv4 loopback; `--host ::` explicitly enables dual-stack private-network access.
To install the desktop launcher and its private jurisdiction
database, run `make install-mars-lab` from the repository root. Installation builds
the C server and all native calculation workers before publishing the desktop
launcher. The launcher retains the installation's `DEBUG` setting and starts
only `tools/mars_lab/build/release/mars_lab` (or `build/debug/mars_lab` for `DEBUG=1`),
directly; there is no Python server fallback. Installation also runs entirely
in C, using the existing MARS string, file, database and calendar modules.

The `make mars-lab` target builds `native-lab`, including the server and all
calculation helpers, before starting it. `make native-lab` builds without
launching. The Lab also has its own Makefile: `make -C tools/mars_lab` builds it,
`make -j1 -C tools/mars_lab test` runs its regression suite, and
`make -C tools/mars_lab run` launches it. Both entry points use the same source list,
build rules, library dependencies and compiler options. A normal `make` at the
repository root includes the Lab Makefile and builds `native-lab` directly after
its shared prerequisites are ready, without a redundant recursive build.
Use `DEBUG=1` for a
separate debug build. The superseded Python server and repository shell launcher
have been removed. Reinstall an older desktop launcher once so it references the
native executable directly.

Child processes use argument vectors rather than a shell, with deadlines,
bounded captured output and process-group cancellation. State retains the
existing `MARS_HOME` and `MARS_LAB_STATE_FILE` locations, uses private files and
atomic replacement, and serialises concurrent updates with an advisory lock.
The existing object-store key in `MARS_HOME/config/mars-lab.env` is reused;
first-run key creation is locked and atomic, and never replaces an existing key.
The native server uses four separate request processes by default, not threads.
Named-town time zones use the installed Linux zoneinfo data, including daylight
saving at each event's instant. Jurisdiction choices, default locations and town
menus come from the configured jurisdiction database through `/jurisdictions`.
Search-result buttons remain stationary while pressed, keeping the same pointer
hit area through release, including when filtering leaves only one town.
Selecting a town updates only the previous and new selection rows, indexed by the
native option position. Search filtering runs when opening, rebuilding or editing
the search, not on every selection. Busy-state updates identify the four location
menus once rather than searching their descendants again for each town button.
Cards containing an open selection menu allow the popup to extend beyond their
edges, so lower results remain visible and clickable; closing restores clipping.
Button tooltips size to their text within the viewport and wrap long unbroken
names on narrow screens, rather than truncating them.
That startup response contains jurisdiction metadata only. Towns arrive afterwards
through indexed `/towns?after=…&jurisdiction=…` pages of at most 512 records, without delaying maths
startup. Each page updates menus and globe markers immediately, with duplicate
records suppressed. A separate request with `lat` and `lon` parameters prioritises
a five-degree window around the observer and then the current globe centre.
After rotation settles, an obsolete local request is cancelled and the new centre
takes priority. Each local run is capped at eight pages, and moving within one
degree of the last requested centre avoids repeating that window. Only selected
jurisdictions (and their parent fallback) and viewed areas are fetched: there is
no world-catalogue download. Changing country cancels obsolete country requests.
Already fetched records are retained for reuse during the session.
A failed later page leaves already-loaded towns and observer coordinates usable,
with an incomplete-loading notice. Nearby-request failures do not stop country loading.
Routine town loading is silent; only loading failures display a notice.
Interactive Almanac requests publish the observer card and body positions before
the annual eclipse/transit search. The existing asynchronous weather refresh then
supplies the upcoming events for the same captured selection; stale replies cannot
replace a newer location. Until then the event section explicitly says it is being
calculated rather than reporting that no events exist. Direct API requests retain
complete results unless `defer_events` is explicitly set to `"true"`.
Fractional gridline tooltip coordinates use degrees and decimal minutes to two
decimal places; whole degrees stay compact, and minute rounding carries into degrees.
There is no duplicated geographic catalogue in the Lab assets. Database changes
appear on the next page load; restart workers to refresh their indexed native
town-to-timezone lookup. A missing or unreadable database disables the country
selectors with an explanatory notice, while mathematical worksheets remain usable.
The packaged worksheet defaults are a snapshot of the saved Lab settings,
including the selected mode, precision, dates and observer location. A complete
snapshot is honoured literally rather than replaced with today's date or the
machine locale. Existing saved worksheets still take precedence. Synchronisation
timestamps are not copied into the defaults.
The server rejects public-network peers, unrecognised Host names and mismatched
browser Origins; forwarded address headers do not grant access. Public funnel
switching remains disabled. TeX rendering additionally rejects non-mathematical
commands and file-access primitives; unsupported TeX produces a rendering
diagnostic rather than discarding the calculation.

Multi-card TeX rendering uses [`executor_t`](executor.md): a request-local pool
has two native workers and three queue slots, handling at most three independent
cards. Each job owns a copied TeX input and supervises its own isolated
`latex`/`dvisvgm` processes and private temporary directory. Mathematical workers
remain separate processes; number precision and mathematical graphs are not shared
between rendering threads. Browser workers are unchanged.

Request cancellation is explicitly copied to each rendering thread. The pool is
joined before the request context is released, then the request thread publishes
SVGs and errors in card order. Workers never mutate shared response objects.
Single-card work stays synchronous; pool allocation or submission failure falls
back to synchronous rendering. Existing subprocess deadlines, output limits and
process-group cleanup still apply. No executor is kept alive across server forks.
Renderer failures use retained `diagnostic_t` records, with child-output details
as a cause; the existing browser-facing error text remains available.

Native failure responses also carry `failure_diagnostic` through the existing
Protobuf value envelope. It contains `domain`, numeric `code`, `message` and a
flat `causes` array, ordered from immediate cause to deepest cause. Flattening
preserves all 64 permitted diagnostic records without exceeding the wire's
nesting limit. This field is separate from any native mathematical `diagnostic`
text. Existing `error`, `error_code`, `timeout_ms` and worker return-code fields
retain their meanings; diagnostic allocation failure does not mask those fields.

| Domain | Meaning of the code |
| --- | --- |
| `lab.request` | HTTP validation or response-contract status, such as 400 or 502. |
| `lab.process` | Saved errno: missing executable, timeout, cancellation, output limit or transport failure. |
| `lab.worker` | Reaped native child exit status, including a status retained beneath a collection failure. |
| `lab.render` | Rendering failure code; its cause may retain renderer output. |

`lab_proc_diagnostic()` builds an owned snapshot from a run's explicit result,
saved errno, exit status and timeout. It preserves errno and does not retain
arguments, input, environment variables or captured output. Calculation, Function,
integrand-binding and calendar adapters use it; calendar callers keep their
existing textual error interface. There is no shared last-error record. Existing
raw calculation output and renderer logs remain potentially sensitive and must
not be treated as safe public logging merely because an error is structured.

Matrix display refinement applies each supplied binding to every cloned symbol
occurrence while retaining the symbolic formula and exact displayed bindings.
Equation conjugate pairing compares structural left-hand-side identity, not
allocation addresses, in both ordinary text and TeX output.

The C regression suite lives in `tools/mars_lab/tests/`. `make -j1 -C tools/mars_lab test`
(or root `make -j1 test_lab_native`) checks worker execution and cancellation, mathematical
response contracts, TeX restrictions, state persistence, cache-key preservation,
time zones and real loopback HTTP routes. Function-card cases execute scalar,
matrix and equation programmes, check rejected source and round-trip generated
cards. The documented Function programmes run last as README examples. The
browser checks also exercise RUN diagnostics, complete source submission and
discarding stale responses. Its filesystem fixtures use private
temporary directories, not the user's worksheets or cache.
The executable is `tools/mars_lab/build/<configuration>/tests/test_lab_native`.
Use `make -j1 -C tools/mars_lab memtest` for Valgrind checks. The suite uses the shared
MARS C test harness and `tests/test_config.json`; its entries retain their
`tools/mars_lab/tests/` paths so individual groups can be selected for bounded,
sequential memory runs.

The optional browser suite is also driven by C, in `tools/mars_lab/tests/browser/`.
Run `make -j1 -C tools/mars_lab browser-test` after the ordinary tests; it requires
Firefox, but no Node.js, Python, WebDriver or external service. `FIREFOX=/path/to/firefox`
selects a particular executable. The suite starts an ephemeral loopback server
and a private browser profile, injects browser assertions into a temporary copy
of the page, and reports through that fixture's state endpoint. It checks native
evaluation and TeX output, authored input, mode controls, history, result cards,
the date picker, persistence, mobile metadata and startup failure reporting.
The lifecycle checks exercise all seven modes, partial native results, exceptions,
history suppression and supersession during binding preparation, fetch and solver
rendering. DOM checks use the actual WASM imports and browser geometry to verify
card projection, accessible expansion, calendar boundary cells, delegated clicks
and recovery after failed or oversized capability calls. Retired script routes
must return 404, and no loader may request them.
Test fixtures use named scenario helpers, explicit record interfaces and ordinary
loops for setup and cleanup. `test_support.ts` provides small timer, paint and
manually completed response helpers; the tests do not need a generic fixture
framework. `native_adapters.ts` maps historical fixture names to owner paths in a
plain table and resolves owners lazily, preserving startup order and temporary
service replacements. Compile-time checks deliberately retain the few generic
contracts needed to detect lost controller types and match the native bridge.
It never uses the running Lab or the user's browser profile. Like the C suite,
it uses `tests/test_config.json`; the browser test has its own source-path entry.

For an opt-in performance report, run `MARS_LAB_PROFILE=1 make -j1 DEBUG=0 lab-browser-test`
from the repository root. Output includes `BROWSER PASS: PROFILE` followed by a
JSON report: startup request counts, encoded message sizes, median encode/decode
times and host-callback counts for scalar, native-result, table and long-text
workloads, including larger table and text messages. It also reports synchronous
decode time, asynchronous submission time and total asynchronous completion time,
and counts requests for a repeated evaluation. This uses the same
private fixture, not the running Lab. Timings are observations rather than test
thresholds; run the ordinary browser suite separately to verify correctness.

The bridge combines child lookup with type lookup, copies UTF-8 directly into
WebAssembly memory and creates/attaches decoded values in one host call. Returned
wire buffers remain owned copies so subsequent operations cannot overwrite an
in-flight request. Town display metadata arrives with the native catalogue, so
selector population needs no additional formatting request or browser cache.
Stale-selection guards still protect asynchronous legacy saved-town matching.

### Background codec jobs

Response bodies of at least 128 KiB are decoded by a dedicated module Worker;
smaller responses and synchronous local-state operations retain the direct codec
path. The initial profile measured roughly 34 ms per encode/decode round trip for
a 601,026-byte structured message, compared with less than 1 ms for a typical small
result on the development machine. Timings vary by hardware and workload: worker
dispatch improves UI responsiveness, not necessarily total completion time.

`wasm/src/lab_jobs.c` owns admission, FIFO selection and non-reused completion
tokens. At most eight queued/running jobs and 8 MiB of retained input are admitted,
with an 8 MiB per-message transport limit. Queue overload rejects explicitly.
Successful plot replies carry the SVG only in `presentation.calculus.plot.svg`;
the redundant top-level `plot_svg` and raw worker transcript are omitted at the
HTTP boundary. Native in-process results remain available for diagnostics and tests.
If a response still cannot be encoded, the server returns HTTP 500 with a bounded
Protobuf diagnostic instead of an untyped failure response. Worksheet storage
retains its separate 4 MiB limit; codec worker memory is bounded to 32 MiB.
Each job has a 30-second deadline including queueing and worker startup. Abort
signals cancel queued work; cancelling running work terminates its worker. Late
messages from a replaced worker cannot complete a newer request. Startup errors,
decode errors and deadlines reject promises and release their admission budgets.

The thin `js/jobs.js` adapter owns browser Workers, promises, timers and transfers.
`wasm/src/lab_job_worker.c` selects startup, serial dispatch, completion,
cancellation and closure through an opaque per-pool phase and explicit host
effects. It rejects out-of-order replies, duplicate readiness announcements and
non-matching tokens without truncating fractional or oversized identifiers.
An invalid reply fails the running and queued jobs; cancelling just the running
job preserves queued work and starts a replacement worker when needed. Closed
controllers cannot be reopened by late events. JavaScript retains the Worker
identity check so callbacks from a replaced browser object cannot enter the
current controller.
`js/codec.js` supplies the shared host-value bridge; all Protobuf validation remains
in C. `js/codec_worker.js` loads the codec-only `wasm/lab_codec.wasm`, with fixed
32 MiB private linear memory and no DOM imports. There is one lazily started worker
per page, reused between jobs; no `SharedArrayBuffer`, atomics, pthreads or blocking
UI waits are introduced. Input buffers remain caller-owned and are not detached.
Transferred decoded objects are still subject to the existing request-token checks
before publication. The admission limit bounds input retention, not total browser
heap usage. All native codec node and depth limits remain in force.

No payloads are persisted or sent to a service by the worker. It fetches only its
same-origin code/module assets. A browser that cannot start module Workers reports
an error for large responses rather than silently decoding them on the UI thread.
Page teardown closes the queue; back/forward-cache suspension preserves it.
The browser tests exercise queue/byte limits, stale tokens, FIFO order, malformed
messages, cancellation, worker startup failure, deadlines, shutdown and recovery.

### Browser implementation layout

`index.html` holds the page structure; startup configuration is fetched from
`/bootstrap` over Protobuf rather than embedded in a JSON script block.
`index.css` imports the styles in their deliberate cascade order;
keep that order when maintaining the component files. No CSS bundler is required.
The styles are divided into:

- `theme.css` and `layout.css`: colours, decorative background, page shell and editors.
- `worksheets.css`: calendar/almanac layouts, tables and basic mode fields.
- `dates.css` and `forms.css`: date picker, local calendar results, input states and selection menus.
- `bindings.css`, `mobile.css` and `buttons.css`: binding controls, mobile access and shared buttons.
- `results.css` and `help.css`: result rendering, help cards and visibility utilities.
- `responsive.css`: tablet and phone overrides, loaded last.

On phones the seven mode tabs form a compact, fully visible three-column grid
with equal-width tabs. Precision controls sit at the top left beside Expression
on the right, with the other six modes in two rows below. A compact, labelled precision
slider shows the significant-digit count on both phones and wider screens.
The header and decorative background are reduced, editors
use a 16-pixel font and phone card action buttons use a compact 36-pixel minimum height
with rounded rectangular styling. Worksheet actions use a smaller 32-pixel minimum
height with reduced horizontal padding and wrap at their natural widths,
and binding actions sit beside the value rather than adding a full-width row.
Date-picker buttons use a clear vector calendar icon and flat styling, integrated
with the date field on narrow screens; they and editable fields retain their
larger touch targets. Result cards
are never removed merely because the screen is narrow: Expression, Function,
Run output, numerical values, diagnostics and calendar cards remain available
when their native result state calls for them. Copy, zoom and expansion controls
wrap inside each card. Wide mathematical content and almanac tables scroll
within their own containers rather than widening the entire page.

Phone layouts omit the introductory Equation and Differential Equation paragraphs;
their notation, binding and starting-value guidance remains available in Help.
Larger screens retain the inline hints. Phones also omit the Integrator's
Bound/Free explanatory paragraph and inline examples without hiding its variable
rows or work budget.

Datetime starts with the selected date, followed by the holiday jurisdiction,
date range and observer. Its short introduction replaces the decorative banner;
on narrow screens, lightly separated field groups and rectangular controls avoid
nested panels and oversized pill-shaped selectors. Julian Day Number and calendar
year share a row on tablets; phones hide these secondary inputs and their labels,
retaining the selected date and every calendar result. Larger screens retain both fields.
Phone layouts also omit the introductory sentence and GMT offset explanation,
while keeping the offset control. They use quieter sentence-case labels
and compact From/To rows. Date inputs incorporate a vector calendar button with
an accessible name. Touch layouts suppress lingering hover tooltips while
retaining hints for keyboard-focused buttons.
On phones the calendar popup places the month and its navigation arrows on the
first row, with an editable year and year-step buttons on a separate second row
so neither button is clipped. The wider-screen popup layout is unchanged.
Phone navigation, calendar group headings and field labels share a 12-pixel type
size; non-editable calendar selector buttons use 14 pixels. Editable fields stay
at 16 pixels to avoid focus-triggered zoom on iPhone.
Almanac evaluates automatically when a control selection changes or a field edit is
committed (for example, by pressing Tab). Its Evaluate button is hidden; other modes
retain theirs. Globe location selections also evaluate automatically.

The left-hand Almanac card includes an observer globe, initially centred on the
entered town or coordinates. Drag or use arrow keys to rotate; scroll, pinch or
use the plus/minus controls to zoom. Town markers appear at closer scales. Select
a marker to choose its town, or a point on the globe to use custom coordinates.
Enter selects the centre point. These selections preserve observer altitude,
camera orientation and zoom, including after asynchronous country lookup and
evaluation. Selecting a location through the dropdown still centres the globe.
“Centre on observer” returns to the entered location without resetting zoom.
“Borders: off/on” optionally adds indicative shared country land borders. Visible
regions load asynchronously; borders fade at close zoom and their texture is
released at 64× zoom or when switched off. Administrative coastlines are not drawn.
The detailed shoreline remains unchanged. These generalised boundaries are not
surveyed or legally definitive.

When the selected position is confidently on land and its country can be identified,
the observer tooltip and AstroNav Navigation Almanac card show its country name
and an offline flag. Both come from the jurisdiction catalogue, joined by the
geographically resolved ISO code, not the form's jurisdiction choice. Missing
flags leave the name readable; uncertain or sea positions do not inherit a country.
Town-marker tooltips also display an offline flag, using the town's catalogue
jurisdiction country code rather than a coastal geometry hit-test. Hover and keyboard
focus load artwork asynchronously; text appears immediately. The tooltip controller
keeps at most 32 country entries within a 1 MiB string budget, cancels obsolete
requests and never attaches a flag to every town in the catalogue. Missing artwork
leaves the textual tooltip usable. Custom observer positions still require the
independent geographic lookup described above.
Custom-point selection, its marker and Shift-hover share an in-flight country
lookup for identical coordinates, including the resolved jurisdiction code. A
cached point can update the dropdown without another database request. Cancellation
of one consumer does not interrupt another consumer of the same lookup.
Update globe geometry with `make update-globe-db`, and refresh flag metadata with
`make update-mars-db`, then restart the Lab. See the
[jurisdiction guide](jurisdiction.md#country-names-and-flags) for ownership and licensing.

“Natural Earth / Map” switches between the photographic reference texture and a
filled shoreline map. The map loads visible-window vector detail as you zoom;
it does not magnify the photographic texture. Switching views preserves the observer,
camera and grid setting. Map mode requires the installed shoreline database;
photographic detail loads separately on zoom. Space is black with faint stars.
“Lat/long: off/on” toggles a subtle latitude/longitude grid, initially off. Grid spacing
becomes finer as the globe is zoomed in; toggling it does not change the observer or evaluate.
One left click on a town immediately applies its observer location; one left click on
the globe selects a custom point. Both start evaluation once and preserve entered
observer elevation. Small pointer movement is tolerated; dragging, cancelled gestures
and right clicks do not select a location.

The surface uses NASA's Blue Marble land, ocean-colour and sea-ice composite,
published in 2002. The unmodified 2048 × 1024 JPEG is bundled locally (266,599 bytes),
with no external imagery requests. Vegetation, deserts and irregular ice boundaries
come from that reference image, not latitude-based colour bands. It is not current
or date-specific ice coverage and does not affect land/sea classification.
See the [imagery notice](../THIRD_PARTY_NOTICES.md#nasa-blue-marble-globe-imagery)
for source, credit, reuse terms and checksum.
At 2× zoom, Natural Earth asynchronously loads the bundled 8192 × 4096 version
(24,147,269 bytes), replacing the overview only after a successful GPU upload.
That provides four times the linear resolution and sixteen times the pixels.
The upgrade is requested once per globe instance, not during the initial overview
or in Map mode. Graphics limits or allocation failure select 4096 × 2048 where
possible, otherwise the existing overview remains available. The full texture uses
128 MiB of GPU storage, plus temporary image decoding memory; no extra WASM memory
or build-time image-processing dependency is introduced. This is still global
reference imagery, not street-level satellite tiles.
At 12× zoom or closer, once the requested longitude and latitude spans are each
at most 30°, an independent asynchronous request loads a river SVG overlay from
the **jurisdiction database**. The initial globe, towns and observer selection
never wait for this layer. Requests are debounced after movement, cancelled when
the view changes and time-limited; obsolete responses cannot replace the current
overlay. Only one regional river texture is retained (about 16 MiB). Zooming out
below 12× hides it immediately and releases its storage after a two-second grace
period; zooming back in during that interval cancels the release and reuses it.
Returning after release loads the visible region again asynchronously. Towns
remain resident to preserve immediate selection; this river policy does not
evict the selected town or introduce a town-cache memory cap.
Only spatially indexed chunks intersecting the view are read. Missing
river data leave the globe usable. The WDBII dataset has been withdrawn because
it misses important town-scale meanders. Updated builds reject it even in older
databases; no replacement river dataset is currently packaged.

River strokes use the same local photographic water-colour sampling and lighting
as sea and fjord fills, including the water-only fallback where narrow channels
are unresolved in the photograph. There is no separate bright-blue river palette.
This is a centreline overlay, not surveyed channel width,
depth or evidence of navigability, and it does not change observer corrections.

Country and town selectors in both Almanac and Datetime remain usable during
asynchronous calculations, including their search fields and option buttons.
Busy-state presentation does not disable these selectors or override a selector
already disabled because its catalogue is unavailable.
Background town batches refresh only menus whose native-selected catalogue has
changed, including country-level fallback lists. Nearby and worldwide arrivals
share a serial refresh queue, coalescing overlapping updates and yielding to
browser input between menu rebuilds. Duplicate or unrelated batches do not rebuild
the menus; the existing coordinate-selection policy remains native-owned.

The photograph remains visible while the initial shoreline mask loads, including
when map mode is selected manually. A failed mask request leaves the photograph
in place; subsequent map requests can retry. Regional map masks are not blended
into the photograph while the map base is unavailable.

At 8× zoom the photographic view automatically switches to the native filled
shoreline map. Simplified fills use locally sampled photographic RGB colours,
including differing greens, deep blue and turquoise water, and white ice, rather
than a fixed palette. A continuously moving, bounded weighted neighbourhood of
photographic samples smooths raster detail without forming colour tiles and
rejects opposite-surface colours at shorelines. These are approximate historical colours, not current snow or ice
observations; they never change shoreline geometry or land/sea classification.
Sample eligibility and the fallback blend fade continuously near photographic
colour thresholds, avoiding abrupt colour patches when the last matching sample
disappears. This smoothing affects only fill colours: mapped waterways and
shoreline boundaries remain unchanged, and ice-coloured samples apply only to land.
Between 8× and 32× zoom, sampling gradually centres on the viewed location.
At town scale, land and water each use a consistent locally sampled colour
across the view, instead of magnifying photographic variations into bands or
patches. The palette follows the view as it moves; it is not a fixed world palette.
Small features absent from the photograph cannot acquire additional photographic
detail from this sampling. When no matching photographic samples exist, the fill
retains the shoreline mask's water or land colour, never the opposite surface's
photographic colour. Filled masks use nearest-neighbour texture sampling so their
land and water colours are not interpolated together. The GPU uses the existing texture without another
image download or additional WASM storage.
Visible-region SVG geometry is regenerated as the view changes instead of
magnifying the photograph. Zooming out restores the photograph; the explicit
Map toggle still selects map colours at any zoom. The photographic GPU texture
is retained for that return, not freed at each zoom transition.
The image stays on disk until requested; the server does not retain an imagery
cache. Its HTTP body budget is 32 MiB to transfer this asset, while calculation
payloads remain limited to 8 MiB and other static assets to 4 MiB.
A blue-black background carries sparse decorative stars,
and gentle view-relative lighting and blue atmospheric limb shading give the globe depth.
Lighting and stars are illustrative, not a calculated Sun/sky simulation.
Native C streams transparent shoreline SVG outlines from the indexed Almanac database.
These fade in at close zoom without replacing the surface colours; the compact reference
texture is not street-level imagery. WebGL combines the layers and WASM owns
camera projection and picking. The browser retains town-marker elements and prevents overlapping labels.
The globe retains a normalised quaternion orientation, composing screen-space
drag rotations with the shared WASM `lab_surface_quaternion_multiply` helper.
Vertical drags starting in the outer fifth of either side of the globe panel roll
the view about its centre; the two sides turn in opposite directions, following
the pointer. Roll sensitivity stays constant when zoomed in. Central drags retain
the ordinary globe rotation, and two-finger gestures retain pinch zoom.
It can rotate through either pole without latitude clamping or longitude-rate
singularities. The shader, marker projection, picking and grid hover use the same
rotation; extracted centre coordinates only determine geographic coverage.
“Centre on observer” explicitly restores a north-up view of the selected location.
Using an eclipse location also recentres the globe without starting an additional
evaluation. Almanac history retains five camera numbers (quaternion and zoom), not
textures or geographical data. **Back** and **Forward** zoom out, rotate along the
shortest quaternion path, then restore the saved zoom and roll over 3.6 seconds.
The departing location retains the latest zoom and rotation, including adjustments
made after selecting it. Camera adjustments refresh that entry without adding
history steps or clearing the forward branch.
Regional shoreline, river and town requests wait until the flight settles; only
the selected marker is drawn during flight. New navigation or manual globe input
cancels the previous flight. Reduced-motion preferences skip the animation, and
older history entries without camera data centre the observer at the current zoom.
The native `lab_globe_flight()` bridge validates the five-number camera records and
interpolates zoom and quaternion orientation; the browser only schedules frames.
Saving an unchanged observer does not reset a rotated view.

In Almanac mode, **Back**, **Forward**, **Help** and **Clear** sit directly below
the globe, above the worksheet introduction. Their former area below the observer
fields remains empty to leave room for country and town dropdowns. Other modes
retain the usual action-row position; the same buttons and handlers are reused.

The selected town keeps a gold label beside the observer marker, including at overview zoom;
it takes priority over nearby labels while its location is visible on the globe.
Globe town buttons and navigation remain available during an Almanac refresh.
Selecting another town starts a replacement refresh, cancelling superseded
evaluation and background requests; stale results cannot replace the latest town.
Town labels retain their position while pressed so a single click stays on target.
Their hover tooltips show the town and full country name in British English,
rather than only the internal jurisdiction code.
Wheel zoom covers the entire globe viewport, including town labels; moving over
a label during a zoom gesture does not hand the gesture back to page scrolling.
With latitude/longitude lines enabled, hovering near a line displays its coordinate
in degrees with a hemisphere label. Intersections show both coordinates. Labels
include an outlined marker snapped to the highlighted line or intersection,
and the grab cursor becomes a crosshair while the coordinate tooltip is visible.
The GPU coordinate conversion uses a range-reduced inverse-tangent series to
keep close-up shorelines and gridlines aligned with native picking and markers.
Labels follow the displayed grid spacing. Both tooltip and marker disappear
during dragging or when the grid is switched off; they do not change the observer
location.
The browser reuses overscanned detail imagery during small pans. Detail requests are serialised, and
WebGL backing resolution is capped at 1536 pixels and 1.5 device pixels per CSS pixel.
Finer visible-window geometry loads asynchronously after zoom or rotation;
late responses never move the camera, and detail imagery is only sampled within its
geographic extent. No online map service is used.
The native server prepares a resident shoreline cache before forking request
workers. Full-resolution coordinates and a spatial tree are shared read-only;
four display resolutions accelerate wide views without changing hit-testing.
Newly built shoreline database chunks are uncompressed, while older compressed
databases remain supported. These native allocations do not enlarge the WASM module.
Missing shoreline data or unavailable WebGL leaves the ordinary observer fields
usable, with a status message. This display is not a nautical chart.

Almanac uses the same compact field styling, with GMT time and zone on one row
and latitude and longitude on another. On phones its repeated introduction and
ephemeris coverage/accuracy note are omitted; all input fields and result cards
remain available. Larger screens retain the coverage/accuracy note.

The browser regression suite inventories live worksheet and help cards at
320, 390, 430, 768 and 844 pixels, including portrait and landscape dimensions,
and checks visibility, page overflow and control geometry across all modes.
These responsive checks complement, rather than replace, testing on a real
iPhone, particularly for Safari's keyboard, safe areas and touch gestures.

`app.ts` (served as generated `js/app.js`) loads the C browser module, bootstrap configuration, database catalogue
and definition scripts, then loads `workspace.js` to initialise browser references
and views of native state. Native modules register controls through the generic
event bridge before startup begins the initial evaluation. Startup errors are
shown in the result pane rather than leaving an
apparently ready but unusable worksheet.

The worksheet adapters retain the existing shared lexical state and explicitly
controlled classic-script loading. Codec, queue and worker code use ES modules;
type-only imports do not alter runtime loading. This preserves the worksheet's
behaviour without adding a bundler or framework. Maintained source boundaries are:

- `transport.ts`: browser value/network bridge to the bounded C Protobuf codec and scoped DOM capabilities for C.
- `api.ts`: network resources and callback capabilities for C-owned request workflows.
- `state.ts`: storage adapters for C-owned history and persistence.
- `workspace.ts`: page-instance wiring, browser references and ordered worksheet initialisation.
- `workspace_controllers.ts`: mode, integrator, precision and presentation owners with explicit contracts in `workspace_controller_types.d.ts`.
- `bindings.ts`: native editor views, binding controls and delegated card events.
- `bindings_controller.ts`: input commit sequencing, refresh ownership and native binding service callbacks.
- `results.ts`: result-card controllers, tooltips, clipboard feedback and fitting lifetimes.
- `results_services.ts`: named browser service adapters for C-owned result and evaluation workflows.
- `locations.ts`: browser location, clock and date-control conversion adapters.
- `surface*.ts`: retained plot resources, camera adapters, refinement scheduling and SVG export.
- `codec.ts`, `jobs.ts`, `codec_worker.ts` and `codec_types.ts`: codec instances, worker lifecycle and validated messages.
- `api.ts` also applies native private-network access metadata; the separate `mobile.js` adapter is removed.

TypeScript functions, methods, parameters, local variables and browser-owned fields
use camelCase; classes and types use PascalCase. Named uppercase constants retain
their existing spelling. C/WASM import and export names, native callback keys and
wire-format fields retain their exact native spelling, including snake_case.
An import table can therefore map a snake_case native key to a camelCase browser
method without changing the ABI. Source filenames and module URLs remain unchanged.

`bindings_types.d.ts` describes explicit browser capabilities and the three native
event payload shapes: binding cards, integrator inputs and integrator row edits.
Common native calls, binding controls and panel values are declared once; each
owner's dependency list names only the additional capabilities it uses. Data
members have short inline descriptions, while callable APIs retain Doxygen comments.
Binding controllers use named callback methods and workflow identifiers. Per-request
callbacks retain the admitted text and request; refresh and feedback work remains
cancellable by its owner. Each file creates its own page instances with lazy
dependencies, allowing concurrent definition loading before worksheet initialisation.
Mathematical interpretation and binding policy stay in C/WASM.

Workspace controller definitions load before `workspace.ts` creates the page owners.
The contracts name each borrowed capability directly, and asynchronous operations use
named workflow identifiers. Browser code retains result snapshots, event subscriptions
and animation frames. A single native `lab_workspace_dom_calculus` call formats the
variable labels and renders their calculus controls, reusing the same C binding-label
formatter as individual labels. This replaces one browser/WASM round trip per label;
the existing explicit-label rendering entry remains available. Calculus clicks use one
owned handler per presenter and resolve the current request methods when clicked.

`application_types.ts` groups bootstrap fields by responsibility and describes the
native catalogue without reshaping it: jurisdiction options are code/label pairs,
and towns are named objects grouped by jurisdiction. Coordinates remain exact
text; the integrator interval cap is a number. Startup promises carry completion,
not result data. These contracts and their compile-time regression checks introduce
no browser behaviour or additional WebAssembly work.

`results_types.d.ts` gives result controllers explicit capability contracts rather
than selecting methods from global implementations. Native response records are
named at their consuming boundary, and result workflow IDs have readable enum
names. `results.ts` keeps page-instance wiring together, with lazy owner lookups
so bootstrap does not access the workspace before it is ready. Frame completion
releases its callback before invoking it; clipboard feedback and status timers
remain owned and cancellable. Native service names and argument ordering are
unchanged by the split into `results_services.ts`. Mathematics, card selection
and workflow policy remain in C/WASM. Forwarding adapters use named argument
tuples where omitted optional arguments must remain omitted at the receiving owner.

`api_types.ts` describes request dependencies as explicit inputs and operations,
not selections from application globals. Each request controller has its own
service contract, checked where its capabilities are assembled. Continuation
frames are plain records crossing the C/browser boundary: the browser captures
inputs, C selects and advances the workflow, and browser callbacks supply I/O
results. Their comments identify the native consumer and when later fields become
available. They do not implement a second workflow in TypeScript.

Runtime services are separated into request ownership, native execution and error
propagation contracts. Their checked service groups are assembled into the flat
registry consumed by C. Evaluation uses one `fetch` operation with a named mode
and mode-specific inputs; C selects the mode and continues to own sequencing.
Request `run` and `runUI` use one type parameter to preserve the callback's result
type, including asynchronous results and `undefined` when admission is refused.
Opaque native results, effects and thrown values remain `unknown`: the browser
must not invent a shape or reinterpret values owned by those boundaries.

The request composition root uses compact, signature-checked callback tables.
Already available services are passed directly; callbacks defer access to worksheet
controllers loaded by later scripts and retain their receivers. Getters are kept
only for late-created state and forward references. Constructors list detachable
public methods for a shared binding helper, without inspecting or binding private
methods. Plot dragging separates local preview, remote sampling and native
completion, retaining revision checks and request cleanup around asynchronous work.

`state.ts` uses named methods for persistence, save timers, restoration and history
cursors. Its native execution contracts use explicit operation signatures rather
than generic result-type calculations. Methods passed as callbacks retain their
owner, while service tables resolve the current application owners when invoked.
Each capability group has responsibility-based sections. Stateless helpers live
in a plain callback object; delayed save and location callbacks receive their
runtime explicitly. The runtime's operation guide summarises the checked
operation/frame overloads without replacing their compile-time checks.
`state_types.d.ts` retains implementation-linked types for complete forwarding
capabilities, with briefs spelling out their inputs and results. Narrow wrappers
declare their actual signatures explicitly rather than advertising optional
arguments they do not forward. The declaration file also includes the runtime's
operation/frame guide and emits no runtime code.
Local fallback storage uses `Codec.encodeStored()` and `Codec.decodeStored()`;
`lab_browser_storage.c` owns both base64 conversion and Protobuf validation.
Existing padded base64 records remain compatible. Missing, corrupt, oversized or
inaccessible records return no fallback. Conversion reuses bounded native buffers,
without browser base64 calls, byte-to-string loops or spread-argument chunks.
Write and encoding errors remain visible to callers, while asynchronous server
publication failures remain non-fatal. C owns saved-record selection, recovery
ordering, stale-save rejection and history contents.
Plain history-cursor pushes use flags zero. Recorded snapshot pushes use flags
three: deduplicate adjacent snapshots and clear forward history, including when
the new snapshot is blank or a duplicate. These operations are not interchangeable.

The redraw scheduler in `surface_volume.ts` uses named methods and concrete data
records rather than generic utility types or nested callbacks. Its control flow
is `schedule()` → `redrawWhenReady()` → `finishRedraw()`: capture the request
context, wait until interaction ends, then await native sampling. Browser event
handlers are bound once so disposal can remove the same listeners. Native C
continues to own range calculations and clipping.

`surface_hover.ts` follows the same style. `inspect()` captures the pointer,
`updateHover()` checks the chart, `findPoint()` asks native C for the nearest
sample, and `showPoint()` displays the marker and tooltip. Tooltip placement
uses explicit boundary checks. `hide()` cancels queued inspection and removes
both overlays; `dispose()` also removes the inspector's event subscriptions.

`surface_camera.ts` uses a plain native interface and named operation numbers
matching the C camera entry point. Each method forwards its arguments and identifies
the native result type; camera mathematics and per-plot orientation remain unchanged.

`surface_types.ts` groups documented geometry records, retained view state and
controller dependencies. Camera angles are a fixed three-number tuple ordered
azimuth, elevation and roll. Initial views have an optional volume; ready views extend
that interface with a required volume after native initialisation. Drawing callbacks,
cached plans, resize observers and settle timers retain their separate optional lifetimes.
Named text and tick records avoid inline
object declarations, and renderer dependencies name their controller classes directly.
These declarations add no runtime state or mathematical processing.

`surface.ts` keeps browser resource ownership separate from native geometry.
View creation separates DOM and drawing-resource allocation from assembling the
retained view; observers become active only after native volume initialisation.
The draw method labels its five stages, from preparation through rendering to
presentation and settling. Original SVG geometry is hidden once per overlay
cycle and its saved visibility is restored when that cycle ends; the settled SVG
layer is managed separately.
Named methods handle hover, deferred drawing, idle SVG replacement, resize and
context-loss recovery; bound event handlers can be removed during shutdown.
Snapshot capture restores the retained overlay even if capture fails. WebGL
allocation, Canvas drawing and browser pixel sizing stay in TypeScript, while
projection, clipping, mesh preparation and painter ordering use the native APIs
described below.

`workspace_native.ts` uses plain capability interfaces and a descriptor factory
for live editor, precision and source properties. The factory selects the field
kind once; reads and writes borrow the current native exports without caching
another copy of those values. The frozen source-name tuple also defines the
writable TypeScript source keys; regression checks verify its native ABI order.
The mode overloads retain precise editor-string and precision-number types.
Workspace and request controllers share the transport callback-binding helper,
with explicit public method lists rather than binding every prototype member.
Zero-length snapshots return null before reading native memory; negative native
lengths remain errors.
`lab_workspace_snapshot_validate()` validates decoded snapshot envelopes, required
fields and exact membership in the supplied catalogue of at most seven modes.
It returns the original state object (including opaque extra fields), null for an
explicitly empty state, or a distinct envelope/state error code. TypeScript keeps
UTF-8 transfer and error reporting, without interpreting mathematical text.

`jobs.ts` uses named callbacks for queue submission, worker messages, cancellation
and deadlines. Each pending job owns its copied bytes, timer and abort subscription;
retirement releases those resources before resolving or rejecting its promise.
The native queue continues to own admission limits, FIFO selection, token validation
and worker lifecycle transitions. Browser code owns the Worker, promises and event
subscriptions. Ordinary typed Promise, Map and byte-array declarations remain;
there are no custom generic helpers or nested arrow callbacks in this adapter.

`codec_worker.ts` separates message receipt, codec startup, decoding, error replies
and input cleanup into named methods. Its message contracts have named interfaces.
Boundary validation remains in TypeScript because messages can arrive before the
worker's WebAssembly instance exists. A per-call JSON validator uses an explicit
stack, rejects cycles and accessor properties without invoking getters, and accepts
shared noncyclic data without repeatedly walking it. Binary parsing and decoding
continue to run in C/WebAssembly.

`codec.ts` binds named browser capability methods to the native imports. Explicit
container-snapshot helpers preserve enumerable data properties without invoking
getters or accepting sparse arrays. Native decoding validates the complete message,
including node types, finite numbers and depth limits, while host callbacks create
fresh JSON containers and check UTF-8 and duplicate keys. The browser therefore
checks the returned root without repeating a full-tree validation pass. Worker
replies retain their separate validation because they cross a message boundary.

`app.ts` separates each script load into its own resource owner, with named handlers
for success, failure and cancellation. The application registers named startup
capabilities and releases its subscriptions and controllers in reverse ownership
order. Native bootstrap still selects assets and controls startup ordering; script
elements, promises, observers and page-lifetime events remain browser resources.

Retained geometry preparation and painter ordering run in C/WebAssembly. Native
code decodes the renderer's normalised SVG coordinates, validates units, expands
colours and triangulates polygon fans before returning browser-owned vertex arrays.
This reads display metadata only; it does not parse mathematical expressions.
`lab_surface_polygons()` reads a projection frame once, clips the primitives and
returns stable far-to-near drawing order. Canvas fallback and SVG export share
this plan, computed lazily and invalidated when the camera frame changes; WebGL
interaction continues to use its depth buffer without requesting a software plan.
Temporary bridge handles are released per primitive and sorting step rather than
retained for the whole scene.

Curve inspection likewise submits the retained geometry in one native call.
`lab_surface_hover()` owns traversal, clipping and nearest-point selection, with
a 65,536-primitive admission limit and bounded temporary handle usage. TypeScript
retains pointer scheduling and DOM overlays, not sampling or distance decisions.

`surface_export.ts` creates and serialises SVG elements from a native vector plan.
`lab_surface_svg_plan()` constructs path commands, closes mesh faces and batches
only adjacent curve primitives of the same colour. Each segment starts a new
subpath, so batching cannot bridge domain gaps or change painter order. Axis path
commands also come from C; browser code applies literal attributes and labels,
clones the figure and strips internal metadata. Fragment arrays are joined once
per completed path through a browser string capability, with temporary handles
released throughout construction. The TypeScript exporter uses plain interfaces
and named methods, without generic helpers or nested callbacks.

`evaluation.js` and its route are removed. Its evaluation, goal-seek and auxiliary
workflows are C continuations, not JavaScript workflows relocated into another
script. `wasm/src/lab_flow.c` supplies the shared native continuation dispatcher;
`lab_flow_evaluation.c`, `lab_flow_goal.c` and `lab_flow_aux.c` own their respective
workflow decisions. The generic host runner executes allowlisted callbacks and
awaits browser operations outside the synchronous WASM scope. Browser-only
request, result and binding callbacks remain in the existing `api.js`, `results.js`
and `bindings.js` modules. Retired `/js/evaluation.js` requests return 404, and
startup no longer loads that script.

The same native continuation mechanism also owns saved-state restoration, history
navigation, worksheet changes, binding commits and edits, location refreshes,
calculus and Function requests, optional render requests, clipboard completion,
and transferring results back into the editor. These responsibilities are split
between `lab_flow_state.c`, `lab_flow_binding.c`, `lab_flow_binding_edit.c`,
`lab_flow_location.c`, `lab_flow_request.c` and `lab_flow_result.c` under
`tools/mars_lab/wasm/src/`, with private declarations in `wasm/include/`.
Synchronous save and restoration plans live in `lab_flow_state_sync.c`; binding
projection policy lives in `lab_binding_projection.c`. Exact editor metadata
selection and retained-source recovery live in `lab_editor.c`: compact display
text is never treated as a new mathematical expression or reparsed in the browser.
`lab_flow_transport.c` owns POST publication, stale-error suppression and ordered
UI recovery, while browser callbacks retain the actual fetches and request resources.
`lab_flow_state_forms.c` owns worksheet clearing and integrator bounds/form
restoration, including revision checks before applying asynchronous responses.
`lab_bootstrap.c` selects definition scripts and validates the native bootstrap
and jurisdiction catalogue before creating the workspace.

Workflow stages use bounds-checked, directly indexed C function-pointer tables,
including synchronous state plans and bootstrap. Each handler owns one continuation
step; shared destinations use the same handler, and immediate transitions call the
next handler explicitly. Missing table entries retain the workflow's completion
behaviour. Sparse terminal stages are handled separately rather than allocating
large, mostly empty tables. Calculus and plot request sequencing lives in
`lab_flow_request_calculus.c`, separate from the other request workflows.
Handler contexts are stack-local to a single native call: they never retain DOM
handles across a browser await or share mutable state between requests.

`transport.js` supplies the shared synchronous and promise interpreters for these continuations. Each
request has its own browser-owned frame; C chooses the next operation, await
boundary, stale-result guard and recovery destination. Scoped DOM handles never
survive a yield. Exceptions remain browser objects, and their display text is
converted lazily, so discarded requests need not inspect exception properties.
The native and browser sides still communicate exclusively through the existing
Protobuf messages; mathematical expressions are not interpreted in the client.

The remaining JavaScript adapters provide browser capabilities: WebAssembly
instantiation and byte staging, fetch streams and abort controllers, promises,
DOM references and events, timers and animation frames, storage and clipboard
access, and browser `Date`/`Intl` services. Keeping these adapters in separate
responsibility-based files does not imply separate JavaScript implementations
of the native workflows. Moving these capabilities themselves would still require
JavaScript imports and would not remove the browser bridge.

The separate `result_text.js` script and route are removed. Native C supplies
escaped Function highlighting and matrix-heading markup alongside the exact
source and lexical spans. User text is escaped, including carriage returns so
HTML parsing cannot normalise them; class names come only from native token
kinds. `src/evaluate/lab_evaluate_numbers.c` formats numeric display tokens
without rounding, evaluating or changing the saved source. The same native
markup supplies symbolic cards, matrix cells, calculus values, equation
solutions and integrator estimates; Function execution receives separate
display text. Oversized presentations omit markup and display literal source instead.
`results.js` installs these presentations and the native calendar markup;
the Function Run browser handler lives with the other requests in `api.js`.

`editor.js` and its route are also removed following migration of source state
and matching into WASM. Presentation metadata caches now sit with result
installation, editor preparation with requests, and field synchronisation with
binding controls. Pending editor preparation is shared only within the same
native request context; switching away and back cannot reuse stale work, and an
old finaliser cannot remove a newer request for the same text.

`integrator.js` and its route are removed too. C/WASM owns row selection,
reconciliation, editing, structured row-text formatting and monotonically
numbered form revisions. Formatting copies names and bounds verbatim rather
than parsing their mathematics; transfer buffers remain bounded to 8 MiB.
The whole row batch now crosses into C once: C normalises each structured row,
copies its UTF-8 fields, inserts notation and newline separators, and publishes
one completed string. Batches are limited to 256 rows and 4 MiB of output;
invalid Unicode or an oversized row rejects the batch without publishing partial
text. Authored whitespace, embedded NUL bytes and supplementary Unicode remain
unchanged. The host bridge only performs bounded UTF-8 conversion.
Stale preparations and changed expressions cannot claim the current revision.
Remaining integrator DOM fields live with binding controls, request transport
with the other API helpers, and row-plan adapters with worksheet state.

`events.js` and its route are removed. `wasm/src/lab_event_dom.c` registers worksheet
controls and owns keyboard, calendar, zoom and lifecycle event decisions. The
generic browser bridge installs listeners and invokes a fixed set of asynchronous
services after the scoped WASM call returns. Repeated registration does not add
duplicate listeners. Clipboard, timers, fetch and browser observers remain in the
JavaScript adapters that use them.

Binding refresh policy now runs in C too. `wasm/src/lab_events.c` turns input
changes into ordered save, source-update and refresh actions; host services run
only after the scoped WASM call has returned. Live workspace views remain owned
by their existing setters, and unchanged input cancels a pending refresh before
marking its binding metadata reusable.
`wasm/src/lab_binding_sync.c` constructs binding request records and projects
accepted editor metadata. It preserves authored values, nullish defaults and
sparse-array positions without interpreting mathematical text. Browser promises
still check request ownership and unchanged input before applying a reply.
`wasm/src/lab_binding_commit.c` builds binding-merge requests and orders accepted
value edits and variable/constant toggles. Exact source and binding records remain
opaque. Expression commits normalise all captured controls before updating the
authored-value Map; duplicate names retain their original update order, and unset
values delete their cache entries. Other worksheets apply the native replacement,
start their existing background refresh, then update history and persistence.
Expression kind changes still await their native evaluation. Request, editor,
source and per-input freshness checks remain at the browser's asynchronous boundary.

`wasm/src/lab_function.c` owns Function-card availability, running/empty states,
output selection and diagnostics. It reads only the stored full programme, never
the abbreviated display. Output keeps its leading whitespace and is installed as
text, not HTML. Failed requests retain useful output alongside their diagnostic;
stale replies do not touch the card. Cancellation, network I/O and request ownership
remain in the browser adapter. The generic text bridge supplies trailing-whitespace
trimming so C uses the browser's Unicode whitespace conventions.

`wasm/src/lab_binding_integrator_state.c` selects editable parameters using exact
bound-name membership, then orders editor, binding-card and saved-source updates.
It uses server-authored editor metadata to recognise wrapped expressions; it
does not infer their structure from display text. Temporary handles are released
per binding so response size does not grow the synchronous handle table.

`wasm/src/lab_evaluation_cards.c` selects the native result representations for each
mode and composes diagnostic and weather sections without interpreting mathematics.
`wasm/src/lab_storage.c` owns saved-control normalisation, recovery precedence and
history restoration plans; browser storage access and awaited editor preparation
retain their cancellation checks in the host.

Automatic evaluations after user edits to DateTime or Almanac controls record the
previous committed worksheet in that mode's Back history. This includes town,
jurisdiction, coordinates, altitude, date and time changes. Back restores those
controls and enables Forward; a new edit after going Back discards the abandoned
forward branch. Unchanged evaluations do not add duplicate entries. History
restoration and presentation-only refreshes still suppress new history entries.

`almanac.js` and its route are removed. Native calendar markup is installed by
`results.js`, request transport lives in `api.js`, and shared observer controls
live in `locations.js`. `wasm/src/lab_profile.c` owns the directly indexed DateTime
and almanac field schemas: field order, DOM IDs, bootstrap default keys, dependent
date/year fallbacks, restore/capture/fill/reset choices and change-event actions.
Server state, local Protobuf fallback and history restoration use the same
schema. Browser code supplies strings and validation flags and applies the
returned choices; it does not maintain a second field-policy table.

Restoration retains authored DateTime whitespace while capture trims values.
Clear preserves town selection and the almanac jurisdiction. Named-town offsets
and manually edited DateTime offsets retain priority over jurisdiction replies.
The codec and worker admission share a native 8 MiB message limit. The browser
stream and buffer adapters obtain this bound through `lab_browser_message_limit()`;
they still reject oversized input before copying it into WebAssembly memory.

The codec also supplies host container and temporary-handle bounds through
`lab_browser_member_limit()` and `lab_browser_handle_capacity()`. Saved-state
admission uses `lab_browser_base64_limit()`, derived from the encoded message
bound including base64 padding. Host checks reject oversized text before staging;
native code independently checks the base64 alphabet, padding, decoded byte limit
and Protobuf envelope. Non-canonical padding, whitespace and unpadded text are
rejected; saved records have always used canonical padded base64. The shared input
buffer accommodates the base64 expansion, while binary messages remain limited
to 8 MiB. The adapter clears staged storage data after success or failure.

The browser ABI is version 43; mismatched assets require a rebuild/restart
rather than falling back to old JavaScript policy.
The native `/bootstrap` response also carries its compiled browser ABI. Startup
checks that it matches WASM before loading worksheet code. This catches an old
server process serving new assets from disk: restart with `make mars-lab-restart`
and reload the page. Without this check, newly required native presentation
metadata could be absent and structured calendar cards could degrade to text.
Calendar result installation also rejects missing native markup. An incomplete
Almanac response reports an error rather than publishing an empty worksheet with
`Ready` status; structured DateTime sections cannot silently fall back to text
when their markup is missing. Failed Almanac installation retains the previous
accepted worksheet data for subsequent visibility controls.

`result_layout.js` and its route are removed. `wasm/src/lab_layout.c` now drives
zoom projection, SVG frame sizing, exclusive card expansion, accessible button
labels, compact/wrapped selection and error-state projection. Native SVG and TeX
are used unchanged. Browser services measure CSS/SVG geometry; the browser's SVG
parser still handles units and view boxes. The asynchronous wrapped-solver fetch
remains in `api.js`, guarded by native request ownership and unchanged source
identity, and completion rechecks current geometry before choosing a variant.
Zoom projection reads inherited base settings before writing styles. The browser
bridge skips unchanged inline-style and disabled-state assignments; it still clears
stale CSS priorities. Busy-state queries stay within their owning control panels,
avoiding traversal of large result SVGs while still finding newly inserted controls.
`wasm/src/lab_solver_view.c` now owns request preparation, deduplication,
snapshot comparison and publication. Its snapshot retains browser values for the
card, source, parent and restored-result owner, never temporary integer handles.
The JavaScript adapter only begins, awaits and finishes the requested operation;
stale responses cannot update either cached SVG or the visible card.
Restored solver cards receive a fresh browser identity tied to the current mode
generation instead of reusing an expired evaluation token. Their wrapping requests
use the same cancellable solver channel and 45-second deadline. Replacement,
mode changes and a new evaluation invalidate obsolete replies, even when their
TeX is identical. Differential-equation and calculus cards enable fitting of
their supplied wrapped representations directly.
Error colours remain in the stylesheet rather than being duplicated as inline CSS.

The scoped DOM bridge in `transport.js` lends node, string and structured-value handles only for
one synchronous C call. C never retains them. A call is limited to 4,095 handles,
rejects re-entry, and releases its handle table even after exceptions. Browser
animation callbacks capture their required DOM node separately, never a scoped
handle. Selector/property strings crossing from C are bounded to 4,095 bytes.
Only native presentation is installed as HTML; error messages remain text.

The WebAssembly C sources follow the same project C rules as the native library.
Headers in `wasm/include/` are application module APIs, not installed library
headers; callers include them without implementation-access macros. Binding-only
value helpers live in `wasm/src/lab_binding_internal.h`, and request continuation
helpers live in `wasm/src/lab_flow_request_internal.h`. Each private include needs
its owning access macro immediately above it. Private-header checks remain active
inside translation units even when IntelliSense is defined.

Native predicate results and true/false options use C `bool`. At the WebAssembly
boundary these remain `i32` values: browser callers pass exactly `0` or `1` for
boolean options, and predicate results arrive as numeric `0` or `1`. The scoped
DOM bridge still boxes JavaScript booleans as value handles; a boolean option
must therefore be converted explicitly with `Number(flag)` before `labDOM.call`.
Handles referring to boolean values remain integer handles, not C `bool`. Counts, indices, status codes
and action masks keep their numeric or enum types wherever more than two values
have distinct meanings.

The WASM APIs, their helpers and callback tables use named `_jsref` types rather
than plain integers for borrowed browser objects. `jsref` accepts any
scoped browser value; `node_jsref`, `event_jsref`, `chart_jsref`,
`gesture_jsref`, `frame_jsref`, `view_jsref` and `snapshot_jsref` identify
more specific roles. `quaternion_jsref` denotes a browser quaternion array in
`[x, y, z, w]` order. Reference-returning functions use these types too.
The `_jsref` suffix denotes an opaque integer reference to a JavaScript value, not a callback or pointer.

The Protobuf bridge uses `codec_jsref` for its separate host-value table.
DOM and codec handles must not be exchanged, retained between synchronous calls,
or treated as pointers into WebAssembly memory. The aliases preserve the existing
`i32` ABI; C typedefs document roles but do not enforce distinct integer types.
Counts, byte lengths, comparison results and packed dates retain numeric types.
The plot-drag dispatcher's event operand remains `int` because its action tag
selects either an event handle or a numeric wheel-timer revision.

The browser projection is split into native modules by responsibility:

- `lab_workspace_dom.c` applies mode panels, help, busy controls, result snapshots,
  calculus buttons and measured editor sizing.
- `lab_binding.c` constructs binding/integrator controls and chooses interaction
  actions. Constant labels use stable merge sorting with browser locale collation;
  post-commit focus uses an indexed name/kind lookup.
- `lab_binding_rows.c` supplies integrator defaults, candidate names, reference
  retention and bound reconciliation. `lab_binding_editor.c` selects visible
  bindings and goal-start precedence from native editor metadata.
- `lab_location.c` projects jurisdiction/town catalogues, calendar defaults and
  returned fields. Named-town restoration checks option identities across awaits.
  C issues a fresh identity token when options are populated or selection changes;
  asynchronous restoration must still own both that token and its option snapshot.
  C selects exact-key, native compatibility-match, coordinate and empty fallbacks
  in that order. JavaScript retains only the awaited request and timezone services.
- `lab_result.c` owns presentation-cache limits, copy sources, digit toggles and
  calendar/result installation. Pending rendering uses request identities and is
  invalidated when another result or restored worksheet replaces the card.
- `lab_evaluation_dom.c` installs supplied solver representations without parsing
  or rewriting TeX, SVG or mathematical source.
- `lab_persist_dom.c` assembles history and local/server save records from the
  native schemas; browser storage failures cannot suppress server saves.
- `lab_payload.c` builds Protobuf request records and applies background mobile
  and eclipse replies after the asynchronous owner has accepted them.
- `lab_events.c` selects editor-refresh, clear, page-inactivity and precision
  actions; calendar precision cannot accidentally trigger integration.
- `lab_widgets.c` owns date-picker opening, navigation, date commits, Today
  selection and measured popup placement, as well as constructing the grid.

The host exposes generic DOM, object, Map, Unicode and geometry capabilities.
Returned values retain their actual browser objects, never transient integer
handles. Native loops release temporary handles per rendered item. JavaScript
continues to deliver DOM events, manage promises, fetch/abort, timers, clipboard,
storage and browser date/time services. Asynchronous adapters retain explicit
freshness checks after awaits; moving display decisions into C does not weaken
request ownership. Mathematical evaluation remains in the native server.

`controls.js` and its route are removed. `wasm/src/lab_select.c` constructs rounded
selection menus and owns filtering, selected/disabled state, opening/closing,
keyboard navigation and change decisions. Town detail columns, accents and
non-Latin labels remain searchable; the browser supplies Unicode normalisation
and string comparison primitives. Native select options remain the source of
truth. Keyboard navigation skips disabled choices, wraps at the ends, and does
not process a search-field arrow key twice through bubbling.

The same C module registers menu, label, keyboard and outside-click subscriptions.
The host bridge attaches listeners idempotently for each DOM node and context;
rebuilding a menu does not duplicate change notifications.

`wasm/src/lab_almanac_events.c` also registers worksheet visibility and totality
actions. It validates server presentation before replacing a card, ignores an
unchanged visibility choice, and orders visibility updates, persistence, rerendering
and totality refresh. Without a retained worksheet it requests a fresh evaluation
without adding history. Browser services run after native dispatch returns;
repeated binding does not duplicate listeners or retain expired WASM handles.
The server's HTML and copy text pass through unchanged.

Menu rendering visits each option but releases temporary handles after each row,
so a large town catalogue does not exhaust the scoped bridge table. Focus,
scroll and change effects retain DOM nodes separately and run only after the C
call releases its handles; resulting event handlers can safely call WASM again.
Failed calls discard their queued effects. Date adapters sit in
`locations.js`, and tooltip adapters in `results.js`, both loaded before event
wiring. Browser regressions exercise 1,500-option rebuilds, keyboard/focus
behaviour, native disabled states, Unicode searches and single change delivery.

`wasm/src/lab_tooltip.c` owns control-hint labels, authored-title precedence, result-card
names, accessibility descriptions and DOM placement. Its fixed ID catalogue uses
binary search. JavaScript only forwards events and retains the active DOM node;
WASM never retains borrowed handles. Repeated hover/focus refreshes do not replace
the original `aria-describedby`, and hiding restores absent, empty or populated
attributes exactly, even for detached controls. Explicitly clearing an authored
title clears its cached hint. Labels are inserted as text, never HTML. Browser
tests cover label precedence, Unicode, focus/hover transitions and restoration.

`wasm/src/lab_widgets.c` builds date-picker month options, weekday headings and all
42 date buttons directly from the existing C calendar model. It handles selected,
today and outside-month classes, zero-padded dates and disabled year-boundary
cells. Invalid displayed months leave the DOM unchanged. Browser code supplies
clock readings and structured dates, positions the popup and delegates button
clicks through one listener instead of allocating 42 listeners per render.

`wasm/src/lab_evaluation.c` owns per-mode preparation, failure-recovery and response
action plans plus status labels. `wasm/src/lab_flow_evaluation.c` coordinates all
seven evaluation modes through the shared continuation runner: C chooses history,
recovery and completion steps, while the host performs requests and awaits their
results. There is no separate JavaScript evaluation workflow or mode-policy loop.
`wasm/src/lab_evaluation_install.c` owns their directly indexed completion plans;
six dedicated JavaScript installers have been replaced by a shared service adapter.
Partial Expression results,
Integrator diagnostics/bindings, DateTime weather follow-ups and awaited
differential-equation solver rendering retain their individual policies.
Preparation flags, diagnostic selection and recovery ordering are decoded only
in C. Named preparation records and response plans replace the duplicate JavaScript
flag catalogue. A plan names allowlisted synchronous browser services; these run
after the WASM handle scope has closed, in native-defined order. Explicit exception
text, partial results and integration raw-error precedence remain distinct.
Native continuations select request-ownership checks at asynchronous boundaries;
the host executes those checks against the native request controller.

`transport.ts` groups its native imports by responsibility. Named field and
measurement indices label the numeric dispatch tables and match the ABI documented
in `wasm/include/lab_dom.h`. Synchronous and asynchronous runners repeatedly ask C
for a plan, dispatch its services, save the final result when requested and stop
at a completion plan. Only plans marked for waiting await their service result;
cancellation prevents later dispatch or publication. Declared failure stages let
C choose recovery without retaining scoped handles.

Plot-control dispatch uses `lab_plot_control_action_t` for change, input, timer,
Auto-click and pointer-down events. Its explicit values retain the browser ABI
(`0` through `4`). `lab_plot_slices_edit()` returns `lab_plot_slice_edit_t` to
distinguish unrelated events, sampling requests and control-only changes; its
target is a scoped browser element handle, not an element index or pointer.

The native browser interfaces use enums for finite choices: worksheet modes,
request operations and outcomes, workflow kinds, editor projections, plot actions
and DOM accessors. `lab_mode_t` in `wasm/include/lab_mode.h` is the shared worksheet
mode contract, including the mode passed to `lab_binding_projection_update()`.
`lab_value_kind_t` names the value classifications shared by the codec and DOM
bridges; their scoped handles remain separate. Explicit enum values preserve the
existing numeric browser ABI. Lengths, indices, handles, bitmasks and packed
dispatcher codes remain numeric, and boundary validation still rejects invalid
selectors before indexing dispatch tables.

Enums also name choices that previously looked like boolean flags: server/local
storage origin (`lab_persist_origin_t`), calendar navigation units
(`lab_forms_period_t`), polygon/segment topology (`lab_surface_primitive_t`) and
absolute/stepped card zoom (`lab_layout_zoom_operation_t`). Their values remain
zero and one. APIs that historically treated every non-zero value as the second
choice retain that behaviour; strict schema-name lookups still reject unknown
origins. Ordinary on/off flags and mode-specific schema field indices are not
enum selectors.

Native action routing uses bounded, directly indexed tables for workflow families,
browser events, worker lifecycle operations and camera actions. Reserved entries
retain their existing no-op, completion or rejection behaviour. Individual handlers
keep ownership, validity and sequencing checks explicit: a table selects an action,
but does not replace the checks that make that action safe. DOM hit-testing and
ordered fallback rules remain conditional where their priority is significant.

`transport_types.ts` groups the numeric ABI, browser service plans, lazy module
contracts and DOM capabilities into separate sections. Plain lookup declarations
replace nested utility types; each service call has a named record and each plan
field documents its role. The service registry accepts callbacks with different
parameter lists, but only the checked dispatcher supplies their native-selected
arguments. Module aliases retain the codec and job controllers' own contracts
without loading them or adding a runtime module wrapper.

Deferred DOM effects capture browser nodes, not native handle numbers. A failed
native call discards its effects; successful calls release their handle scope
before executing them. An effect's event handler may therefore start a fresh
native call. Calling back into the bridge while a native scope is still active
remains forbidden. Effect exceptions propagate after cleanup and stop the rest
of that effect queue. The shared DOM and wire instances use lazy constructor
callbacks so neither accesses the other before both have been initialised.

`wasm/src/lab_evaluation_setup.c` also owns request preparation stages. Visible
bindings are captured before their commit; editor text is captured afterwards.
Only after binding assembly completes does C choose between the authored source
and the last exact input used for a precision change. Equation and
differential-equation inputs are trimmed, whereas Matrix and Integrator sources
keep their supplied whitespace. Calendar snapshots use the existing location
services. The service bridge returns its last
callback's result without awaiting it; a preparation plan explicitly marks that
final callback as asynchronous, and the host checks ownership before proceeding.
Getter-backed views are read lazily and are never overwritten.

`wasm/src/lab_evaluation_weather.c` selects weather completion services. Stale
replies do nothing; failed requests and presentation exceptions retain the current
overview and report weather unavailability. Accepted weather uses the native
section merger before reporting readiness. Native continuations select the
parent-request guards; network I/O remains a browser capability.

Obsolete JavaScript entry points for result-text formatting, binding-value policy
and workspace snapshots have been removed. Their browser regression assertions
call the owning C exports directly; production no longer carries wrappers solely
for test access.

Completion has two phases: initial card/editor projection, then persistence and
derivative controls. The second phase reads the live editor after the first has
finished, rather than saving a pre-projection snapshot. Differential equations
await their optional SVG request between phases; current rendering failures keep
the plain derivation, while stale success and failure replies skip completion.
Services stop on the first exception. In particular, Almanac markup must be
accepted before replacing its retained worksheet data, and DateTime saves observe
the evaluated fields. Getter-backed workspace views are never replaced with plain
data properties.
`wasm/src/lab_goal.c` similarly orders successful goal-seek completion and failure
recovery. `wasm/src/lab_flow_goal.c` coordinates the preparation and request
continuations around these plans. History is captured before editor replacement;
browser callbacks retain execution-time getters, request resources and asynchronous
binding preparation. `wasm/src/lab_flow_aux.c` supplies the auxiliary workflow
continuations through the same runner. Native source, target, binding values and
mathematical renderings are used unchanged.

`wasm/src/lab_persist.c` owns the mathematical worksheets' save schemas, local
storage keys, empty-value rules and independent deferred-save tokens. A single
browser adapter captures values and performs local storage and Protobuf I/O.
Expression and Equation autosaves defer by 250 milliseconds when requested;
other saves remain immediate. Superseded callbacks cannot publish old snapshots
or cancel another mode's save. Blank Expression input preserves a pending useful
save, whereas blank Equation input is sent to the server. Empty editors and
integrator bounds retain the last useful local copy; empty Matrix operands clear
it. Local storage failures do not prevent server saves. Token exhaustion fails
closed until the page reloads, rather than reusing a stale token.
Server and local editor restoration also share this schema and a C action plan:
empty or abbreviated saved matrix, differential-equation and integrator editors
cannot overwrite useful text. Server text is trimmed; local editor whitespace
is preserved. Binding-aware restoration obtains canonical text from native MARS,
not from a browser-side mathematical parser.

The freestanding C implementations live in `wasm/src/`, with separate
module interfaces in `wasm/include/`. Each implementation includes its own
header and only the interfaces it uses; there is no umbrella header.
`lab_browser.h` covers the Protobuf codec and scalar limits, while
`lab_host.h` and `lab_dom.h` declare the JavaScript host imports for codec values
and scoped DOM access respectively. Worksheet state, requests, bindings,
presentation and the other browser modules have their own matching headers.
These interfaces are tool-private, not installed MARS library headers.

The retained surface renderer separates display geometry from mathematical coordinates:

- `lab_surface_math.c` provides bounded display arithmetic and quaternion camera operations,
  using the standard `sin`, `cos`, `atan2`, `exp2` and `log2` implementations from WASI libc.
  The library is compiled for WebAssembly and linked into the page module, not called
  through JavaScript. The zero vector still has direction zero; binary zoom exponents
  remain clamped to -1022 through 1023, and invalid gesture ratios still return zero.
- `lab_surface_geometry.c` supplies projection, six-plane clipping, axis placement and
  uniform viewport fitting for WebGL, the canvas fallback and vector export.
- `lab_surface_volume.c` retains exact endpoint text and constructs native zoom-bound
  recipes from a dimensionless binary exponent. It does not evaluate absolute bounds
  using browser doubles.

These C/WebAssembly operations use double-precision display values, not the native
arbitrary-precision matrix or number modules. Native MARS evaluates the range recipes
and performs precision-aware sampling; normalised mesh coordinates then cross the
browser/GPU boundary. JavaScript supplies DOM and pointer-event bindings, asynchronous
request coordination and drawing API access. Pixel layout and approximate readouts
are not the authority for mathematical coordinates. See
[3-D surface interaction](#3-d-surface-interaction) for controls and precision limits.

All page implementations compile into `lab_browser.wasm`. The codec and job
policy also link into the separate `lab_codec.wasm` worker asset;
that smaller module has no DOM imports or worksheet state.
Both compile the allocation-free MARSlib wire core from
`src/protobuf/protobuf_wire.c`, using the public `protobuf_wire-t.h` interface.
Varints, fixed-width numeric fields and bounded field reads are shared with the
native Protobuf codec. Only this source is included: no owning message records,
dynamic arrays, allocator or threading code is pulled into WASM. The shared
helpers have hidden visibility and do not expand the browser ABI. Object
traversal, schema checks, nesting limits and fixed buffers stay in the Lab adapter.
See [Protobuf wire operations](protobuf.md#allocation-free-wire-operations).
Both modules link standard `memset`, `memcpy`, `memmove` and `memcmp` from WASI libc,
without JavaScript shims. Worksheet history compaction and codec message compaction
use `memmove` because their ranges can overlap. Disjoint transfers use `memcpy`,
and length-checked byte equality uses `memcmp`. Static label lengths use `strlen`;
host strings retain their browser Unicode semantics. Fixed buffer capacities,
validation and stable depth sorting remain owned by the application.
Integrator row formatting also uses `memcpy` between its separate, preflighted
workspace buffers. Angle wrapping uses `fmod`, retaining each caller's zero and
negative-angle convention. Two-coordinate screen distances use `hypot` to avoid
overflow or underflow from squaring the coordinates before taking a square root.
Finite checks and special values use `isfinite`, `NAN` and `INFINITY` from
`<math.h>`. The `sqrt`, `fabs`, `floor` and `ceil` compiler builtins remain
intentional: they generate direct WASM instructions, whereas the freestanding
`-fno-builtin` build emits function calls for the ordinary library spellings.
The `wasi-libc` build dependency supplies C headers and a static archive;
its musl maths implementation resides in `libc.a`, not the empty compatibility `libm.a`.
Only required archive members are linked; no WASI system imports are permitted.
Application symbols use default visibility and the linker exports them with
`--export-dynamic`; do not use `--export-all`, which also retains library internals.
The codec worker links memory operations but no maths routines. `LAB_WASM_INCLUDE` and `LAB_WASM_LIBC`
override the packaged header directory and archive path. The dependency probe compiles
and links calls to all five maths functions, the four memory operations and `strlen`
without allowing unresolved imports. Browser regressions also reject unexpected
imports in both modules and check overlapping, unaligned and zero-length memory operations.
Only browser bridge functions listed in `wasm/lab_browser.imports`
(declared in `lab_host.h` and `lab_dom.h`) may remain unresolved. Keep this
allowlist synchronised when changing the bridge. The worker has its own smaller
`wasm/lab_codec.imports` allowlist; other missing functions fail the link
rather than producing a module that fails during browser startup. This also
catches compiler/toolchain differences when building on another machine.
The WASM compiler checks that exported functions have prior declarations, and
generated dependency files rebuild the implementations affected by a header change.
Objects and dependency files mirror the source layout under
`build/<configuration>/wasm/src/`; the shared wire object and its optional
assembly listing live under `build/<configuration>/wasm/marslib/`. Both browser assets live in
`build/<configuration>/wasm/`.
The optional `make lab-wasm-asm` target writes per-source `.s` assembly listings
beside the objects, with separate `.s.d` dependencies. See
[inspecting WebAssembly assembly](building.md#inspecting-webassembly-assembly)
for the command, output and debug-build paths. Normal builds do not generate listings.
Native result-markup generation in `src/calendar/lab_calendar_markup.c` and
`src/evaluate/lab_evaluate_markup.c` runs on the server, not in WebAssembly.

The freestanding sources in `wasm/src/` separate binary transport, worksheet state,
request policy, view policy and form arithmetic. `lab_workspace` owns editor bytes, committed
snapshots, mode selection, precision and back/forward history. It also owns editor
full/display text, last evaluation input and goal-seek source/target bytes in the
same bounded arena. These active fields are pinned, not evictable history.
Exact display matching and source recovery happen in C without interpreting
mathematics; browser properties are read/write views, not duplicate storage in
globals or DOM attributes. The workspace controller also owns editor
capture and restoration policy: blank mathematical input retains the previous
editor, empty saved editors display catalogue defaults, and calendar summaries
always use their form defaults. Restoration does not overwrite saved text.
Binding-aware versus plain-text restoration is selected in C from the mode and
native editor metadata; the browser only applies the resulting DOM update.
Saved result-card metadata is also normalised in C. Each restored variable list
is an independent array, missing labels default to empty text, and only explicit
`false` disables differentiability. Restoring reusable input clears bindings from
the previous mode. Browser code assigns the returned state before refreshing
derivative controls and scheduling layout; absent snapshots use the existing clear
path instead.
The view controller owns each registered result card's zoom and the single
expanded-card selection. DOM attributes and CSS classes are outputs, not state
stores. Changing expansion preserves zoom, and hiding an expanded card collapses
it through the same controller. Registration is bounded to 64 cards; invalid
card operations leave existing state unchanged. Browser adapters retain only
DOM references and apply native decisions, including accessibility attributes.
`lab_requests`
owns request identity, endpoint selection, busy state and stale-response rejection,
including mode changes away and back. Operation names, endpoint paths and expression action names come from
its own policy tables. The browser builds a direct name index once at startup
instead of maintaining a duplicate routing catalogue. Unknown operations fail
closed; inactive requests expose no endpoint.
`lab_forms` owns Gregorian calendar
arithmetic, numeric limits and the date-picker controller. Its open/closed state
and visible month/year live only in WASM. Month/year selection preserves the
input day where possible, clamps leap days and saturates at the supported year
limits. Invalid navigation leaves state unchanged; closed pickers reject edits.
The former `date_picker.js` and `worksheet.js` scripts and their HTTP routes are
removed. `locations.js` retains browser input conversion and clock access;
its TypeScript source, `locations.ts`, uses named service methods and explicit
calendar records. Controller methods retain their owner when passed as callbacks,
and browser dependencies are resolved when an action runs, not during script
loading. Stateless host providers and ownership guards use plain objects, and
the location service table is sectioned by responsibility. The calendar host port
exposes only calendar flags, not editor or result session state.
Rounded-select registries index controllers by element in a `WeakMap`, without
attaching callback properties to DOM elements. Each registry dispatches native
rebuild/synchronisation plans to its own controllers; individual disposal removes
that controller's registration, while registry disposal closes its remaining menus.
Browser number inputs deliberately perform form-value conversion so blank or
invalid text remains `NaN`, rather than becoming zero through `Number()`.
Browser `Intl` supplies time-zone and daylight-saving data; C continues
to own date validation, arithmetic, town matching and picker policy.
`workspace.js` retains browser references and read-only views of the native
year/month. `lab_workspace_dom_references` supplies the control catalogue as
browser-owned references and array snapshots without changing worksheet state.
`wasm/src/lab_widget_events.c` owns opener toggles, navigation, commits,
outside-click handling, tooltip event boundaries and subscription registration.
The generic event bridge deduplicates subscriptions by target, event, dispatcher,
action and browser context, retaining actual objects rather than scoped handles.
Native dispatch returns ordered browser services; commits and focus changes run
after the WASM call returns, so nested browser events can safely enter WASM.
The date picker builds its complete 42-cell grid in one WASM call, including
valid-date, outside-month, today and selected flags. C projects that grid through
the DOM bridge. Invalid months leave the previous buffer unchanged,
and cells beyond years 1–9999 are disabled. `lab_rows` plans active integrator
bounds and reconciles result bounds with retained free parameters in batched
calls. The C binding-row adapter supplies presence/reference flags and applies
returned indices; JavaScript passes structured rows and native reference metadata.
Plans retain authored row values unchanged and enforce the native 256-row limit. The same
module now plans add/remove/toggle edits as index/flag pairs, protects the last
bound, clears bounds when changing a row to Free and appends a replacement bound
when needed. One browser edit handler awaits binding commits and applies the
plan without interpreting mathematical text. Failed plans
leave the previous output intact. Reference detection remains in native MARS,
with stale or incomplete reference metadata conservatively retaining rows. `lab_view` owns
mode/card titles and visibility, control availability, zoom levels, matrix fitting
and compact-versus-wrapped result selection, along with date-picker popup sizing
and placement. It also decides tooltip placement and conditional editor resizing:
overflow enables resizing, deliberate manual sizes survive content changes, and
unused manual sizing resets. The extra-height budget is shared between visible
editors and bounded between 96 and 320 pixels in total. The host supplies measured geometry;
the browser's SVG API resolves intrinsic units and view boxes. Invalid geometry
cannot initiate a wrapped-render request, and non-finite zoom indices select 100%.
JavaScript keeps
DOM events, browser storage, fetch/abort resources, timezone API access, syntax
highlight spans and the application of layout decisions; no mathematical engine is shipped
to the browser.

Convenience adapters used only by browser regression checks live in
`tools/mars_lab/tests/browser/native_adapters.ts`, not in the production scripts.
The private browser fixture injects them and initialises their native-dependent
values after startup; they have no public asset route.

The native jurisdiction catalogue supplies each town's selection key and formatted
coordinate detail directly. `lab_forms_town_presentation` shares the legacy town
formatter when building that catalogue. Town selectors apply those fields without
posting the catalogue back for formatting; the former browser detail cache and
preparation requests are removed. Legacy saved-town matching still uses `/forms`.

Text preparation uses native `string_t` helpers through `/forms` and
`/presentation`. These handle integrator bounds and symbol references, time and
town input, authored binding edits, integration-constant removal, exact goal-seek
starts, matrix layout, Function-card lexical spans and numerical display metadata. Async edits and restoration
are checked for staleness before applying results. Domain conditions and exact
binding values are retained; metadata is installed only after an accepted response.
Function highlighting is limited to 1,024 lexical spans per variant and 64 KiB
of source. Larger programmes retain their full text without colouring; execution
and copying still use the unchanged complete source.

Native presentation metadata also supplies equation solution text, integrator
detail/value text, matrix section-heading spans and normalised calculus cards.
`lab_evaluate_calculus.c` selects derivative/integral expressions, exact and
abbreviated Function variants, numerical values and compact/wrapped TeX. One
browser request handler in `api.js` applies both kinds of card through `results.js`;
scalar matrix calculus uses the same native projection. Native editor preparation
constructs matrix calculus input with validated variable names and retains the
authored bindings and conditions. The former `calculus.js` and its route are
removed, and ordinary matrix evaluation shares the matrix result renderer.
Function text falls back to the native expression
when no programme was returned, and absent numerical values keep Value hidden.
The browser does not infer
these from English status messages or parse the returned mathematics. Almanac
responses contain both all-body and visible-body variants, including exact
clipboard text, compact event times, visibility labels and land-search eligibility.
`lab_calendar_markup.c` supplies escaped HTML for both worksheet variants,
event tables, deferred totality actions and DateTime/weather sections. The browser
installs this native markup and binds DOM events; it no longer reconstructs these
tables or sections in JavaScript. Variable text and action attributes are escaped
before publication, retaining exact Unicode and carriage returns. Browser layout
tests consume native fixtures through Protobuf, including compact clocks,
accessible scrolling and mobile event cards.
Matrix layout requests likewise supply escaped native HTML with exact factors and
cells. CSS uses native column counts to choose intrinsic-width or fitted grids;
JavaScript no longer builds matrix terms, brackets or cells. Ragged input retains
the native plain-text fallback.
Changing the body filter selects a supplied variant without another astronomy
evaluation. Metadata construction is bounded to 256 bodies and 64 events and is
published only after successful preparation.

The remaining JavaScript supplies browser capabilities and asynchronous adapters:
event delivery, measured geometry, clipboard and storage access, Unicode/locale
operations, network cancellation and bridge value marshalling. C owns control
and result projection, while JavaScript still coordinates promises and checks
request freshness at asynchronous boundaries. Local display updates stay
synchronous. WebAssembly cannot directly call browser APIs; moving these
capability wrappers into more WASM-to-host calls would not remove that dependency.

All browser API requests, bootstrap data and application errors use typed
Protobuf envelopes exclusively. JSON POST requests receive HTTP 415 and the
browser rejects non-Protobuf API responses. `response.labData()` decodes the
binary response; it is not a JSON parser. Calendar fallback snapshots in browser
local storage use the same C/WASM Protobuf codec, with base64 solely because
local storage accepts strings. Versioned `.protobuf` keys replace the former
JSON snapshots; old keys are neither read nor deleted. The server's saved state
remains authoritative. Missing, corrupt or oversized browser snapshots are
ignored independently. Scalar browser settings remain ordinary strings, and
the native saved-state file still uses JSON. The schema, binary64 policy
and resource limits are documented in the [Protobuf guide](protobuf.md#mars-lab-browser-wire-contract).
The Makefile probes Clang and `wasm-ld` by compiling and linking a small module.
No Emscripten, Python, npm or JavaScript Protobuf generator is required. TypeScript
compilation produces the host adapters; the Protobuf codec remains native C. Rebuild and
restart the Lab together: an incompatible browser-module ABI reports a startup
error, not a silent JavaScript fallback.

The server registers individual asset routes; it does not expose arbitrary files
under `assets/`. JavaScript uses `text/javascript; charset=utf-8`; the browser
installation manifest uses its standard `application/manifest+json` format.
All assets retain private-access checks, `no-store` and
`nosniff`. `lab_page_catalogue()` builds an owned settings object from native C tables;
`/catalogue` returns it as Protobuf. The former JSON asset and `/catalogue.json`
route are removed. Edit the native tables and rebuild to change packaged defaults;
the existing saved worksheet state still takes precedence.
`lab_page_jurisdictions()` reads configured database storage through the public
jurisdiction visitors and returns owned `options`, `locations` and `towns`
collections, with an explicit availability flag. The native timezone index uses
that complete representation. The browser uses `lab_page_jurisdiction_index()`
for metadata and `lab_page_town_batch()` for bounded pages instead, keeping both
the byte and value-node transport limits intact. A zero `next` cursor marks the
empty final page. Metadata failures fall back to unavailable geography so that
ordinary maths remains usable; metadata requests have a three-second timeout.
`lab_page_defaults()` returns the owned worksheet-default snapshot and
`lab_page_render()` renders the escaped template with request-specific state.
`lab_page_bootstrap()` returns owned startup configuration for the Protobuf wire format.
`MARS_LAB_ASSET_FILE` selects only a custom HTML template; defaults are compiled in
and scripts use the packaged asset directory. Jurisdiction data always comes from the configured database, not from
that custom template directory.

### Native implementation layout

The Lab follows the library's public-interface/private-implementation layout.
These interfaces are private to the Lab application, not installed MARSlib APIs.

```text
tools/mars_lab/
  Makefile          Local entry points, source inventory and shared build rules
  include/          Documented interfaces between Lab modules
  tests/            C regression suite and private test fixtures
  src/
    app/            Command-line options and worker-process supervision
    server/         Opaque server ownership, routes and access checks
    evaluate/       Mathematical worker records, native editor metadata and TeX rendering
    forms/          Native string_t form parsing and symbolic reference metadata
    calendar/       DateTime, almanac, weather and town time zones
    page/           Client template, defaults and escaped substitutions
    mobile/         Private-network discovery and separate QR encoder
    process/        Independent child execution core and built-in calculation adapter
    runtime/        Startup cache configuration and key preservation
    state/          Locked, atomic worksheet persistence
    wire/           Native typed Protobuf adaptation
    worker/         Built-in calculation modes, including Ophelia
    internal/       Controlled white-box test façades
  assets/           HTML template, CSS import list, css/ styles and js/ scripts
  proto/            Versioned Lab wire schema
  wasm/             Freestanding C browser modules
  build/release/    Generated mars_lab, module objects and dependencies
```

Debug output uses `build/debug/`; generated output is ignored by the root
`.gitignore`. The single Lab `Makefile` supplies its source inventory and build
rules when included by the root build. Invoked directly, it delegates commands
to that same root graph, keeping compiler flags and dependencies consistent.
`make -C tools/mars_lab clean` removes only Lab build output, leaving the
library and saved worksheets intact. All ten calculation modes are compiled from
`tools/mars_lab/src/worker/` and linked into the single
`tools/mars_lab/build/<configuration>/mars_lab` executable. There are no separate
calculation binaries. A request starts an isolated instance of that executable
with `--worker MODE`, before listener, browser or cache initialisation; the child
receives only the calculation's arguments and optional standard input. This keeps
timeouts, cancellation, output limits and mathematical global state isolated from
the request handler without depending on scratch console applications.

`make native-lab` builds that same server executable, and `make mars-lab` starts
the web application. Calculation modes are an internal `--worker` interface,
not separate Make targets. Explicit per-mode executable
environment overrides remain available for diagnostics and failure-injection
tests, but are not required in normal operation. The repository's `scratch/`
directory retains standalone examples and experiments, not production Lab workers.

The reusable process core in `lab_process.c` has no calculation-registry dependency.
Server-specific command construction is isolated in `lab_process_worker.c`, so
consumers of generic process execution need not link the calculation modules.

Both module APIs and static implementation functions use module-specific
prefixes: `lab_cal_` for calendar, `lab_eval_` for evaluation, `lab_proc_` for
process handling and `lab_svr_` for the server. Other modules retain prefixes
such as `lab_page_` and `lab_mobile_`. Filenames and opaque type names are
unchanged. The opaque `lab_server_t`
owns its listener and route set: callers create, serve, query its port and free
it without accessing transport internals. Stateless adapters accept borrowed
strings or JSON and return caller-owned results; they do not need artificial
handle types. Module-private headers stay beside their implementation, and
tests needing white-box access use the controlled `src/internal/` façades.

The desktop icon calls the installed launcher, which starts the already-built
`tools/mars_lab/build/release/mars_lab`; its name, icon and browser address
are unchanged. Direct binaries use their compiled repository location unless
`MARS_ROOT` is supplied.

Use `make mars-lab-stop` to stop a Lab process belonging to the current user,
or `make mars-lab-restart` after changing the native helper or client.
These targets also recognise older Lab binary names and locations. Restart waits
for the old processes to exit; if shutdown takes more than ten seconds, it stops
with an error instead of launching a competing instance.

### Result-card conventions

**Help** opens mode-specific guidance across the full workspace width, temporarily
hiding the editor and result cards. **Return to worksheet** restores them without
changing the input and returns keyboard focus to the Help button. The mode tabs
remain available while Help is open. Cards cover current binding roles, symbolic
evaluation, Function RUN, numerical notation, equation branches, transforms,
matrix operations, integration limits, calendars and almanac tables.

Each mode retains its most recent editor text, binding values and controls
between sessions. Input events save an in-progress edit as well as a submitted
calculation, so moving between modes or restarting the Lab does not restore an
older expression or equation.
The precision slider changes the working precision used by the mathematical
helpers. Its native-defined positions start at 53 bits (binary64), then 106 bits,
then 192 bits, followed by equal 64-bit steps through 6,656 bits. The maximum displays 2,004
digits after rounding up; it is the first step above 2,000 digits. Binary and decimal
precision are not exactly equivalent. Clicking the track moves one step towards the click;
dragging the thumb previews the
significant-digit count; releasing the thumb or committing a keyboard change saves
the selection and refreshes the calculation once. The accessible value and tooltip
also report bits. The `…` button opens a custom-precision editor with bits and
decimal-digit units, a conversion preview and Apply/Cancel controls. Enter applies
the draft; Escape or clicking outside dismisses it without changing precision.
Custom entries accept 53–262,144 bits or 17–78,913 requested decimal digits;
decimal requests round upwards to whole bits. This is the current application
ceiling, not MPFR's theoretical limit. Existing calculation time limits still apply.
The current result stays visible while a calculation is pending.
Each mode retains its own precision. Values above the slider maximum show a
Custom readout; moving the slider selects a value within its ordinary range again.
Loading a saved precision does not round it to a slider step, although values
outside the custom-entry bit limits are clamped. Precision controls are disabled
while a calculation is busy. The result cards provide independent zoom, expansion and copy
controls; **Use as input** returns a suitable result to the editor.
When numerical results use powers of ten, the mantissa is joined directly to
`×10` and its superscript exponent, without an intervening space. Real and
imaginary components use the same notation. Native display markup applies this
consistently to Expression, Equation, Differential equation, Matrix and Integrator
cards, including calculus values, matrix cells, bounds and error estimates.
Small numbers retain ordinary decimals through the fourth decimal place;
smaller magnitudes use superscript exponents. Practical positive exponents are
expanded to ordinary decimals, with a 4096-place bound on expansion.
Function cards and their Run Output use the same display typography, but copying, running and reusing
their source preserves the original executable notation. Identifiers, quoted
strings and comments are not rewritten; TeX remains independently rendered.
Compact displays retain the exponent when shortening a mantissa. The original
full source remains available through expansion and the card's source metadata.

On a desktop-sized screen, each mode now follows the natural page height rather
than forcing the editor and result panels into the remaining viewport. This
keeps the controls directly below their editor and avoids large artificial
blank areas. A resize grip appears only when an editor genuinely runs out of
space. Result cards still scroll or expand independently when their content
requires it.

## Expression mode

### Native function plots

Single-real-input functions with non-zero imaginary samples have four linked
views inside the Plot card: a rotatable three-dimensional curve with input, real
and imaginary axes; an equal-unit complex-plane projection; real and imaginary
components; and amplitude. One adaptive native sample supplies all four views,
including their domain breaks. Amplitude uses the native hypotenuse operation on
the real and imaginary components, equivalent to complex absolute value without
forming an avoidable squared intermediate.

Interior zero-coordinate guides are dashed, distinguishing them from solid data
curves. Domain breaks remain disconnected in each projection. Near singularities,
the last finite sampled point is not a finite limit of the function.

The views form a two-column grid on wide displays and stack on narrow displays.
The four-view grid uses a single narrow outer gutter and compact gaps between
cards. Cards fit their content instead of stretching to match a neighbouring camera
panel. Each plot scales to fit its card without horizontal scrolling. Expanding a view
retains the expression title, which is rendered natively in all four projections, and
replaces the entire grid with that projection; collapsing restores all four views.
The outer Plot card retains its own expansion. All four chart viewports are height-limited
to fit the browser viewport, rather than growing indefinitely with the card width.
Expanded curve views use thin,
non-scaling strokes so enlarging the chart does not magnify line thickness.
Camera controls are docked directly below the 3-D curve, inside its own card;
they follow that view through expansion and are hidden when another projection
is expanded. The curve's display height is limited to leave viewport space for
its controls. Compact camera sliders fill their fields, with smaller desktop rows
and larger touch targets on coarse-pointer devices.
The controls continue to work after docking, expansion and mode restoration.
Range gestures update only their targeted complex-view card; sibling charts and
the retained 3-D camera are not replaced. Wheel gestures outside the individual
cards do not fall through to the first chart in the grid.
Releasing a camera gesture on a coarse preview requests full native detail;
the preview notice disappears when that full-detail result replaces it.
Evaluate (including the editor's keyboard shortcut) resets plot bounds, automatic
height, camera orientation, zoom and slice controls, even for unchanged input.
It also clears individual plot expansion and restores the complete four-view grid.
Internal plot redraws and precision-only reevaluations retain their current view.
The input range panel has Auto and Manual choices, independently of vertical
scaling. Auto is the default and lets the native plotter choose the interval;
Manual reveals the From/To fields. Returning to Auto clears explicit input bounds,
including retained zoom coordinates. Evaluate restores Auto. Restored or
gesture-selected input bounds select Manual.
Automatic complex sampling initially
uses the input interval from −10 to 10; manual input limits override it. The complex-plane view preserves
equal coordinate units in a square native viewport, centred vertically within its card.
Amplitude starts at zero to avoid magnifying numerical
round-off in an otherwise constant magnitude.
In Auto or Fit all, wheel, button and pinch zoom in Components and Amplitude
change the input interval while retaining native automatic height. Manual limits
enable two-axis zoom, which can intentionally move the curve outside the viewport.
Components use gold for the real part and teal for the imaginary part, with a
boxed native legend and numerical vertical ticks but no redundant “Value” title.
Dragging inside Components or Amplitude pans its input and
value axes; these two views share the manual value range and all projections
share the newly sampled input interval. Mouse and touch gestures use the same
native pan path. Amplitude retains its input-axis label and numerical ticks,
without repeating the card's amplitude heading beside the vertical axis.
During an ordinary drag, the existing full-detail SVG is translated locally;
native resampling runs on release rather than replacing the chart mid-gesture.
Ticks, tick labels, grid lines and zero guides move with their corresponding
coordinate axis during the preview; out-of-frame ticks are hidden until the
native redraw supplies the new tick layout. Axis titles and the frame stay fixed.
Hover inspection is suspended during dragging, and the gesture reuses its initial
screen scale instead of repeatedly measuring the chart after moving its curves.
The Complex Plane and space curve use Gothic ℜ and ℑ axis titles. Dragging the Complex Plane pans
its real/imaginary viewport independently of the sampled input interval and the
other projections. Its zoom and Reset controls also operate on that viewport;
equal coordinate units are preserved, so circles remain circular.
The space curve returns to vector SVG after interaction stops, including its
axis arrows and labels. Dragging resumes the retained local preview immediately;
the idle SVG uses the same C/WebAssembly projection without resampling or resetting
the camera. Surface meshes retain their existing renderer.
For 3-D curves, zoom narrows or widens the sampled input interval about its
centre and requests fresh native samples. For example, 200% selects half the
original input span; it does not magnify and crop the existing image. The real
and imaginary ranges are fitted to the new samples, preserving the camera
orientation and the coordinate box's original proportions. Narrowing the input
interval spreads the selected section across the x-axis rather than shortening
that axis. Only the 3-D card is replaced; the other projections retain their
views. Reset restores the input range from before zooming.
The zoom slider retains its track while the thumb moves; wheel or pinch gestures
can extend it, and Reset restores the original track. During refinement, the preview
uses the actual sampled interval, including offsets caused by missing finite samples.
Failed or superseded refinement restores the last accepted curve and zoom setting;
it does not leave an unrefined preview or overwrite a sibling plot's newer controls.
An interval containing only real values remains a 3-D curve with zero imaginary
component; zooming does not switch that card to a different plot type.
Rotation uses a fixed enclosing-sphere fit to avoid changes of apparent scale
as the camera turns. The initial drawing uses that same fit, avoiding a framing
jump on the first camera interaction. The settled SVG
batches same-colour segments while preserving their native gaps.
Hover near a space-curve segment to show a marker and its approximate input,
real and imaginary coordinates. Inspection interpolates the displayed native
samples, never across a domain gap, and follows the current camera projection.
The marker and tooltip disappear during gestures and are not included in copied SVG.
The cursor changes from a dragging hand to a crosshair on a curve hit. The outlined
marker stays above both the canvas and tooltip, including while the plot settles
back to SVG; moving between the figure and its curve paths does not dismiss it.
Pointer entry also restores inspection when a replacement SVG arrives beneath
a stationary mouse, without requiring an extra mouse movement.
Retained 3-D viewports clip overlays instead of creating scrollbars, so inspection
cannot resize the plot repeatedly. Idle redraws re-pick the stationary pointer;
leaving the plot or starting a gesture clears that pointer state.

Real-only samples retain the existing real-curve presentation. Distributional
Fourier results retain their components and labelled impulse weights, with an
explicit notice that the full distribution has no pointwise 3-D, complex-plane or
amplitude view; they are not passed to the ordinary complex-function sampler.
Plotting just the non-impulse curve in those views would omit part of the result.

Supported Fourier distributions use real and imaginary curves for the regular
part and thin dotted-tip stems for Dirac impulse weights. A stem is not a finite
function value. A boxed legend pairs curve lines and matching impulse stems with their
labels, using separate colours for all four component/series combinations. It sits
in a reserved strip above the axes, without covering curves or impulses. The
vertical axis reads **Value / weight**. The spectrum title displays the authored transform;
identically zero sampled components do not add duplicate curves or legend entries.
The explanatory caption sits beneath the figure, aligned with the vertical axis.
Distribution spectra hide the plot-type selector because ordinary curve/surface
selection does not apply to them; input and vertical range controls remain available.

The native transform display retains one complete identity, including its Dirac
impulses. Principal-value and finite-part qualifications remain explicit; the compact
TeX operator `Fp` denotes the Hadamard finite part; the half-line case uses a unit cutoff. Curve-only
pointwise exclusions are not appended to this distributional identity.
The complete distribution remains in the copyable Expression and Function output;
it is not replaced internally by a punctured ordinary function. Unsupported or
infinite-series forms retain their complete distribution notation.
For mixed curve-and-impulse spectra the default frequency window is −10 to 10, with editable bounds;
automatic height limits retain impulse weights as well as the regular curves.
Infinite impulse sums show the first 100 terms, with no term count in the title.
Spectrum titles retain the authored mathematical transform rather than replacing it with the impulse sum.
Pure impulse spectra use uniformly thin, equal-height stems with dotted upper
sections and no arrowheads. Their heights are schematic; hover titles give the
coordinates and weights. Only horizontal-range controls apply, including zoom.
Installing a spectrum clears retained surface and second-variable selections;
zoom changes only its frequency interval, and Reset restores the preceding view.
Automatic bounds include zero and fit the preview's support. These conventions
give two-sided spectra a small margin beyond both endpoint impulses; the zero
axis then lies inside the chart rather than at its right edge. They
apply to all pure impulse spectra, not just the zeta transform.
The complete series remains in the symbolic result. Delta derivatives and
nonlinear products of distributions are not approximated as narrow peaks.

**Evaluate** in Expression, Equation, Differential Equation and Integrator modes installs the
mathematical result cards first, then generates a separate **Plot** card asynchronously.
Expression results without a non-constant plotting variable, or with an unresolved
transform or undefined function that prevents numerical evaluation, leave the Plot
card hidden. Native operation metadata determines this before scheduling a plot;
an unsuccessful automatic range search alone does not make a result ineligible.
Supported numerical integrals and sums remain eligible even without an elementary
closed form. There is no separate Plot button.
Plot failures remain local to that card; they do
not hide successful mathematical results. New evaluations, editor changes and mode
changes invalidate obsolete plot work. Plotting does not keep Evaluate busy.

**Plot type** offers **Auto**, **2-D curve** and **3-D surface**.
Auto chooses the type automatically. Automatic selection draws
a surface when another non-constant variable is unset; otherwise
it draws a curve. The first eligible remaining variable becomes the second
axis; **Second variable** can override this choice. Additional unset symbols
receive plot-local slice controls rather than becoming extra axes.
Explicit 3-D mode can sample two variables even when their worksheet values
are already supplied. Constant bindings remain parameters, never automatic axes.

#### Parameter slices

Up to eight additional unset parameters share a compact vertical slider panel
to the right of the chart, keeping the surface visible while adjusting a slice.
Vertically stacked radio buttons select the parameter when there is more than one; only
one slider is visible at a time. Switching preserves each parameter's value and
limits without resampling. The panel is hidden when there are no extra parameters.
Each parameter has an exact-value field and **Auto**/**Manual** range choices.
Every **Evaluate** selects Auto, including re-evaluation of unchanged input.
Auto hides the lower and upper limits; Manual reveals them for editing. Returning
to Auto requests fresh native limits, retaining the current value when it lies
within them and otherwise choosing the native starting value. Range mode is
remembered independently for each parameter. Supplied
worksheet bindings remain fixed; slice controls never rewrite the worksheet.
The plot title identifies the current parameter values, including in exports.

Native C selects initial limits. A limited structural domain analysis handles
principal inverse sine and cosine with affine parameter dependence and bounded
real trigonometric residuals. Other expressions use bounded real-value probes
on a fixed 25-point axis grid; this is a useful starting range, not a proof of
the complete domain. Zero is preferred when suitable. Limits remain fixed while
dragging and can be edited when a different range is needed.

For example, evaluate `acos(sin(2-x*y)+z)` with all three variables unset.
The output is an `x,y` surface with a `z` slider initially at zero, ranging from
−2 to 2. The accompanying README test prints:

```text
Surface slice ready: x,y axes; z = 0; slider limits: -2 to 2
```

The slider sends integer positions over its selected interval. Native numerical
arithmetic calculates the parameter value at sufficient precision; JavaScript
does not interpolate mathematical coordinates. Typed exact values are retained
independently of the rounded handle position. During dragging, coarse previews
are requested one at a time, with only the newest pending position retained;
release requests full detail. WebGL displays the sampled mesh, but does not
interpolate between mathematically different slices. An empty slice keeps the
controls available; when a previous plot is retained, an explicit notice marks it
as the previous view.

#### Surface axes

For example, enter `x^2 + y^2`, leave both variables unset, and press **Evaluate**.
With **From** −2 and **To** 2 in each variable group, the output is a shaded
paraboloid titled \(u(x,y)=x^2+y^2\). At the default view, the rightward base axis is
the first variable, the leftward base axis is the second, and height is the formula value.
Arrows at the positive ends identify the two variable axes by name and the
height axis by `u` (the formula value). The native renderer chooses an unused
name: `u`, then `v`, then `w`, then numbered `u` names, avoiding all existing
variable and constant bindings. The title uses the selected first and second
variables in that order; the height axis uses the same function name.
They follow the camera; their text stays
upright. No surrounding box is drawn. The formula title, expansion and copying
work as for curves.

The accompanying README test prints:

```text
Surface plot ready: u(x,y) = x^2 + y^2; height axis: u
```

Plot type, variables, ranges, Y scale and camera controls live inside the
**Plot** card, below the image, including when the card is expanded. Changes
redraw automatically after a short pause; no update button is needed.
Choose variables using the compact radio buttons populated from native binding
metadata. Automatic plotting uses the selected first variable, initially the
first available variable in the native catalogue;
the second is chosen automatically unless an explicit radio choice is selected. The same variable
cannot occupy both axes. Each variable keeps its **From/To** pair together.
The vertical-range controls are available for both curves and surfaces; the
3-D camera controls are displayed only for surfaces.
Complex 3-D views are automatically resampled at lower detail when their SVG
would exceed a 6 MiB chart budget, leaving room within the 8 MiB response limit
for the title and other fields. The complete requested coordinate range is
retained; facets are not truncated. Sampling detail remains available in plot metadata,
without an annotation over the chart.
Zoom in for finer sampling. This safeguard does not impose a fixed zoom-out limit.
For equal sine/cosine expressions, the native ordinary-precision mesher separates
the two periodic phase families before triangulation. This avoids joining the
wrong sheets at crossings. Extremely crowded views that exceed the bounded sheet
or output budget are rejected with a request to narrow the bounds, rather than
silently omitting sheets. Other expressions and high-precision local frames retain
residual sampling; finite sampling cannot guarantee discovery of every component.
Plot settings use compact button-style radio choices, not dropdowns. Choosing
**Auto** for the range clears explicit bounds and hides all bound-entry fields
in Expression, Equation and Differential Equation plots. For a single variable,
the whole range panel, including its heading and border, is hidden until **Manual**
is selected. Axis choices remain available when there are multiple variables.
The separate **Manual limits** setting reveals vertical bounds. Blank horizontal bounds
remain automatic. Manual vertical limits wait until both fields are filled.
Delayed edits are discarded after a worksheet change or newer explicit request.
Incomplete or invalid live bounds leave the last valid plot visible and show
validation feedback beside the controls; correcting the bounds redraws the plot.
Failed plot redraws also retain the previous coordinate-bearing view and show a
separate diagnostic. Zoom out or use **Reset** to recover; the zoom buttons never
switch to enlarging the diagnostic while a previous plot is available.

#### 3-D surface interaction

The compact **3-D view** controls come first, directly beneath the chart. The **Zoom** slider
starts with a 12.5% to 6400% coordinate-volume range and extends as zoom continues,
with 100% as the initial ranges. Its readout follows wheel, pinch and button zoom,
using a binary exponent at extreme scales. Horizontal spans change by the
same factor about their current centres; height follows the policy described below.
Axes remain fitted to the chart.
Gesture instructions are in **Help**, rather than beneath the sliders.
The **Azimuth** slider in **3-D view** rotates around the surface; **Elevation** tilts the view;
**Roll** turns the view clockwise in the screen plane. All three support mouse,
touch and keyboard adjustment, with live degree readouts.
The defaults are 315°, 30° and 0°, respectively: both horizontal axes initially
increase from left to right, and the height axis increases from bottom to top.
Rotating the view changes their screen directions without reversing the values.
Azimuth and roll span a full turn; elevation accepts −90° to 90°.
For the example above, set azimuth to 120° and elevation to
40°: the output is the same paraboloid viewed from the other side, with its
three ticked axes and formula title retained. Orientation controls update the retained
WebGL view live and remain local when released: no sampling or SVG redraw is
requested merely because the camera moved.

Alternatively, **left-button drag** or **drag with one finger** over a 3-D plot
to turn a virtual trackball. Dragging can pass over either pole and turn the plot
upside down. Hold **Shift** when starting the drag for screen-plane roll.
The initial pointer position and unit quaternion anchor the gesture; orientation
is never reconstructed from rounded slider readouts during dragging.
Azimuth, elevation and roll remain useful explicit controls. At a pole, their
Euler representation is ambiguous: the readout retains the previous azimuth,
while quaternion rotation remains unrestricted. Crossing a pole can change the
equivalent Euler readouts abruptly without a discontinuity in the camera.
The projected volume is fitted uniformly to the available chart space during rotation.
Height grids and implicit meshes preserve equal coordinate units on all three axes;
shorter numerical ranges occupy proportionately shorter edges, without axis stretching.

The HTML plot container reserves touch gestures before movement starts, including
touches near an axis. Page scrolling remains available outside the plot.
The complete native mesh stays in a persistent WebGL viewport, with depth testing.
Geometry and label canvases are redrawn at their displayed device-pixel size when
the plot is resized or expanded, rather than stretching a fixed-resolution image.
Backing dimensions are capped at 4096 pixels per side and the WebGL device limits.
Mode snapshots retain clean native SVG and the camera/volume state, not canvas
pixels or GPU resources. Returning to a mode recreates its renderer without
requiring another evaluation.
A depth-sorted canvas is the fallback when WebGL is unavailable or its context is
lost. HTML overlays align the canvases to the fitted plot and retain the native
TeX heading. Pointer movements are coalesced at animation-frame boundaries.
Releasing a gesture leaves the same viewport and geometry visible, ready for the
next gesture. Escape, losing browser focus or a cancelled pointer restores the
starting orientation; a superseding plot invalidates an unfinished gesture.

Sampling and camera presentation are separate. Native MARS still supplies the
normalised coordinates, colours, tick labels, precision checks and domain gaps.
No expressions are evaluated or algebra rewritten in the client. Initial and
replacement geometry currently arrive in the native SVG transport; the client
does not request new SVG for camera movement. Editing coordinate bounds requests
new native samples. The old view remains available while that request runs.

3-D wheel zoom, pinch gestures, the Zoom slider and the zoom buttons change all
three coordinate spans uniformly in Manual height and for implicit meshes, not the on-screen size of the axes.
For explicit height grids in Auto, zoom instead changes the two horizontal spans;
the preview retains its sampled height and native refinement selects a fresh Auto
height for that region. The complete box is refitted uniformly, preserving equal
coordinate units even when its proportions change. The selected
volume stays centred on the starting volume. The retained mesh is transformed and
clipped at all six faces immediately; tick labels follow the selected bounds.
On release or after a short pause, native MARS samples that volume at full detail,
subject to the chart budget and its range-aware precision policy. The preview
remains interactive meanwhile.
A new zoom or rotation cancels an obsolete refinement; late replies cannot restore
old bounds. Rotation preserves both the selected volume and quaternion orientation.

Zoom is represented by a binary exponent rather than rounded absolute browser
coordinates. C/WebAssembly owns quaternion rotation, projection, clipping and
gesture mathematics. It sends a fixed-size native range recipe based on the
original endpoints, retaining authored symbolic bounds where supplied and native
round-trip decimal bounds for automatic ranges; native MARS evaluates that recipe and samples using
its arbitrary-precision number module. Successive gestures do not nest recipes
or rebase through double precision. The slider follows the current exponent,
allowing continued zoom beyond its initial range. The controller permits binary
exponents from −1024 to 32000; native plotting retains its 65,536-bit resource
ceiling and may reject a request requiring more precision. GPU coordinates are
normalised before conversion to display precision. Reset restores the original range controls
and height policy, including Automatic. Editing bounds manually establishes a new
zoom baseline after the native result arrives. Empty volumes retain their axes
and controls, allowing zoom out or Reset to recover the surface.

Auto height includes the full finite sampled extent unless extreme tails trigger
the robust range policy. Equal coordinate units do not require equal axis ranges:
the entire volume is fitted uniformly inside the card, with space for labels.
Selecting Auto or Fit all discards a clipped zoom preview even when the returned
mesh is unchanged. Manual height and implicit meshes retain strict coordinate-volume
zoom: refinement does not expand the height again or undo a microscopic selection.
Fit all also uses strict volume zoom; select Auto for an explicit surface whose
height should follow the horizontally selected region. Auto's extreme-tail safeguards
remain in effect for poles and unusually large values.

Zoom-generated range fields use interval-aware precision: broad intervals show
short bounds, while narrow intervals retain more digits, without trailing zeroes.
The C/WebAssembly formatter covers all three axes. Displayed values do not replace
the exact sampling recipes; manually entered bounds retain their authored precision.
During microscopic zoom, controls retain the recipe until native decimal labels arrive.
Preview tick labels that cannot be represented safely are omitted until refinement finishes.
For a very large single zoom jump, axes remain interactive while old geometry outside
the safe relative GPU range is hidden until fresh native samples arrive.

**Copy** generates SVG on demand from the current mesh, orientation and zoom,
including the native TeX title and projected axes. SVG uses depth-sorted triangles
rather than a GPU depth buffer, so intersecting triangles can have different
occlusion in the vector export.

This also works when expanded. Right-click keeps its normal behaviour.
Two-dimensional plots retain SVG rendering; dragging pans and Ctrl+wheel zooms
their sampled ranges. A second finger on a 3-D plot starts pinch zoom; further
fingers are ignored. Touch and pen gestures do not change page scrolling outside
the plot.

Over a 3-D plot, **Shift+wheel** adjusts elevation and **Alt+wheel** adjusts azimuth.
Wheel down increases the angle; wheel up decreases it in 5° steps.
Wheel rotation uses the same persistent viewport and does not request new samples
when the wheel stops. Ctrl takes precedence for zoom; Shift+Alt together does
not rotate. Wheel rotation is inactive during a drag.
Axis numbers are spaced to avoid collisions at shared corners and during
rotation; crowded numbers may be omitted while their tick marks remain visible.

Surfaces start with 70×70 uniformly sampled vertices (9,522 triangles for a complete grid)
and an adjustable oblique view, with
blue/teal/gold height shading. Automatic height scaling crops extreme spikes near
poles so they do not flatten the rest of the surface. **Height range** in the Plot
card offers **Fit all** or **Manual limits**; enter a finite **Minimum** and
**Maximum**; the plot updates automatically. **Auto** restores automatic cropping.
The range applies to the height axis named in the formula title (usually `u`),
not either independent variable. Rotation previews and animation use the same
selected policy; manual limits remain fixed. Clipped facets stop at the chosen
heights rather than creating flat caps. Full-quality plots also sample edge
midpoints and cell centres. Cells whose interpolation error exceeds 0.2% of the
sampled height span subdivide locally, up to two levels and 10,000 leaf cells.
Cells containing both real and non-real samples also subdivide before being
discarded, preserving more of the valid surface near domain boundaries. Unresolved
boundary cells remain omitted at the refinement limit rather than bridging detected
non-real samples.
Shared boundary vertices join coarse and fine patches without cracks. The native
sampler retains precise coordinates; only normalised error estimates use doubles.
The original uniform grid still determines robust height statistics, while new
extrema extend an untrimmed automatic range. Manual limits remain unchanged.
Preview plots skip this extra work. If the refined response is too large, the
sampler retries the complete 70×70 uniform grid before considering a coarser grid;
optional refinement must not lower the baseline resolution. Individual facets
are never discarded to meet the response-size limit.
Refinement is bounded rather than a guarantee that all oscillations are resolved;
narrow the axes for finer detail. Steep finite facets are not deleted merely for
their height differences. Missing real values leave holes. Opposite-sign edges
receive up to eight bisection probes: non-real/non-finite interiors or sustained
growth beyond eight times the endpoint magnitudes mark sampled gaps. This is a
bounded singularity heuristic, not a domain proof. Coarse
7×7 higher-precision checks increase the working precision when needed; the
normal starting mantissa is 53 bits, independent of worksheet precision.
Neither sampled binding is changed in the worksheet. Local refinement does not
guarantee detection of every pole or narrow feature. See
[surface rendering limitations](plot.md#height-surfaces).

Evaluate an expression with a variable to generate its plot. **From** and **To** select the parameter
range; bounds accept native mathematical text, including
`-@pi` and `@pi`. Other parameters must have numerical bindings. The chosen
variable is sampled independently of its worksheet value, which is preserved.

Blank horizontal bounds mean **Automatic**, independently for each endpoint and
each surface variable. The native worker probes the expression at bounded
coordinate scales, including intermediate scales between powers of ten, looking
for enough finite real values and visible variation. Well-populated discrete
levels count as variation even when there are no intermediate heights;
it trims invalid outer margins only where no bound was supplied. An entered
endpoint is never moved. This is an expression-dependent viewing heuristic, not
a fixed −10-to-10 range or a proof of the complete real domain. It can miss narrow
or distant features; when no useful window is found it reports that automatic
range selection failed and asks for explicit bounds. That failure does not prove
the expression is unplottable. Explicit ranges bypass this range search.
Plots retain gaps inside the chosen window.

For example, enter `asin(x)`, leave both bounds blank, and press **Evaluate**.
The output is the inverse-sine curve over its real interval:

```text
Automatic curve: asin(x), x from -1 to 1
```

The **Vertical range** (curves) or **Height range** (surfaces) sub-card offers
**Auto**, **Fit all** and **Manual limits**. Auto is the default.
Automatic scaling
uses a horizontally distributed range estimate for curves, or finite grid heights
for surfaces, to keep extreme values near poles from flattening the rest of the plot.
It is a viewing heuristic, not a proof of divergence. Cropping adds no visible annotation.
Fit all includes every finite sampled value.
For Manual limits, supply finite **Minimum** and **Maximum**, with minimum
strictly below maximum. Changes update the plot automatically.
Tick labels use rounded decimal steps, never fractions; very
large or small labels use `×10` with superscript exponents.
Solid bottom and left axes sit beside the numerical tick labels, with the top
and right edges open and no axis titles. Interior zero guides remain visible.
Teal curves, muted green-grey axes and tick labels, and a faint dashed grid
complement the gold TeX heading. These native SVG colours are preserved when
the plot is expanded or copied.

The **Plot** card receives native SVG generated dynamically in memory by
[`sampler_t`](sampler.md) and [`plot_t`](plot.md). Curves display that SVG directly;
surfaces retain its mesh in the interactive renderer described above. Its title is the
entered formula rendered in TeX, without numerical binding substitutions.
Recognised ellipsis series use sigma notation: entering
`1/1^x + 1/2^x + 1/3^x + ...` gives the title
\(\sum_{n=1}^{\infty}\frac{1}{n^x}\), not just the closed-form function name.
With X bounds −5 and 5, this example draws only the convergent part, where
\(x>1\); the rest is blank. It does not draw the analytic continuation of zeta.
If no finite real samples exist, the error asks you to check the domain,
convergence and unset parameters, without displaying internal worker records.
The existing restricted TeX renderer supplies the title glyphs, using its bounded
temporary-file workflow; native code combines title and curve into one in-memory
SVG. If title rendering is unavailable, an escaped plain-text formula remains
alongside a rendering diagnostic. The image travels in the existing Protobuf
response. No external plotting programme or browser-side mathematical evaluation
is involved.
The Expression card retains the formula and bindings. The mathematical rendering,
Function and any available numerical Value or Solutions cards remain independent of the plot.

**Expand** uses both workspace columns, temporarily hiding the editor and the
other result cards. At 100% zoom the plot fits the available width; **Collapse**
restores the two-column workspace. Mode switches restore that mode's own result
headings and its separate plot card rather than carrying a plotting heading into another mode.

#### Curve navigation and inspection

Hold **Ctrl** and use the mouse wheel over a plot to zoom in (up)
or out (down). Shift+wheel and Alt+wheel rotate 3-D plots as described above;
otherwise, without Ctrl, wheel events retain their normal browser behaviour
and do not change plot zoom. For 2-D plots, Ctrl+wheel changes both axis ranges
around the pointer and requests fresh native samples. Surface controls instead use
the [coordinate-volume zoom described above](#3-d-surface-interaction).
For 2-D plots, **−** and **+** change the sampled ranges, and **Reset** restores the
range controls and height policy captured before zooming. Pointer coordinates use the
SVG screen transformation, so nested headings, clipping and fitting do not offset
hover readings or gesture anchors. Vertical and horizontal hover guides meet at
the reported curve point, marked by a small outlined circle. The marker and
horizontal guide are hidden when no visible curve
crosses the pointer's x coordinate. The tooltip shows
the pointer's x coordinate and the plotted curve's approximate y value at that x,
interpolated from visible native line segments without bridging gaps. Where several
branches cross that x, the selected DE curve is preferred; otherwise the crossing
nearest the pointer is used. No y value is shown when there is no visible crossing.
The tooltip uses renderer-owned viewport metadata
and includes the native integration-constant values when a solution-family curve is hovered.
Each coordinate and each constant appear on separate lines. The guide disappears
outside the chart. It does not obscure curve-identity tooltips or
appear in copied SVG output. Native SVG hover titles are suppressed in the interactive
curve plot to avoid duplicate tooltips; their text remains accessible and is preserved
in copied SVG output. Curve inspection and gesture arithmetic use browser-sized
coordinates; non-finite or collapsed coordinate ranges require manual native bounds.
Curve zoom changes sampled bounds rather than magnifying the SVG.
Drag a 2-D plot with the mouse or a finger to pan horizontally and vertically
without changing its scale. Existing curves move immediately inside clipped
plot windows while moves are coalesced into native redraws. Drag previews use
an adaptive sampling budget of 513 evaluations per curve instead of 8193, with fewer subdivisions and
no derivative construction; numerical precision checks remain enabled. Release
finishes at full detail. Escape or pointer cancellation restores the starting
view. Reset restores the view from before zooming or panning.
Spread two fingers to zoom in and bring them together to zoom out. Curves track
the midpoint by changing sampled ranges, with coarse previews and full sampling
when either finger lifts. Further fingers are ignored.
Lift the remaining finger before starting another drag. Pointer
cancellation restores the pre-gesture view, and Reset undoes the zoom.
For curve plots, Fit all retains the complete sampled range of continuous curves.
When the sampler marks discontinuities, it protects the view from extreme tails
near asymptotes instead of flattening all other curves. This uses the same robust
range policy as Auto; manual limits always take precedence. It does not assert
that the function is bounded or that every discontinuity is a pole.
The **2-D curve** choice is hidden while another plotting variable is unset.
Fix the other variable's value to make a curve available; clearing that value
returns an incompatible curve selection to **Auto**.
Curve range fields use interval-aware formatting without trailing zeroes.
Sampling retains the unrounded gesture coordinates until a bound is edited;
manually entered bounds keep their authored precision.
Scrolling outside the plot image, including over its controls, behaves normally;
other result cards still require Ctrl or Command with the wheel to zoom.

#### Native sampling and precision

Plotting normally uses **hardware doubles** (a 53-bit mantissa, roughly 16 decimal
digits), independently of the worksheet precision. Shared native evaluation plans
accelerate supported curve values, curve derivatives and surface samples.
Unsupported operations retain the ordinary native evaluator; higher-precision
reference passes never use the double plan. Coordinates outside the safe double
exponent range select native arbitrary-precision arithmetic as well. X bounds and manual Y
bounds are parsed with guard precision first; narrow ranges at large offsets
automatically request more bits. A private copy of the authored formula and
bindings is parsed at the plotting precision, leaving worksheet output unchanged.

Before the full sample, 17 distributed points are compared with higher-precision
evaluations. Disagreement triggers a higher-precision retry, including when
cancellation would otherwise erase a small result. These checks are heuristic,
not a guarantee of numerical accuracy everywhere. They include additional guard
precision for very small ranges and up to 4,096 bits of the worksheet precision.
The plotting limit is 32,768 working bits, with bounds and reference checks
limited to 65,536 bits; unresolved precision requirements produce an error.

Equation mode generates a plot asynchronously after evaluation. It draws the left and right
sides of a one-free-variable equation as separately coloured curves on common axes; their intersections
represent real solutions within the plotted range. Select one equation variable
and bind any remaining parameters. The equation in the editor is unchanged.
With exactly two unset variable bindings, it instead draws the real zero contour
of the residual in their coordinate plane. For example, evaluating `x^2+y^2=1`
produces a unit circle centred at the origin, with both coordinate ranges initially
about −1.2 to 1.2. This is a curve, not a height surface. The first-axis choice
selects the horizontal coordinate; the vertical limits control the other coordinate.
Bound parameters stay fixed. Three free coordinates produce a triangular isosurface
of the residual, with the existing camera controls; more than three require bindings.
The isosurface sampler uses a 25-point grid per axis (13 during interaction), caches
shared edge intersections, and stops safeguarded edge refinement once its residual
is small enough for plotting. This limits repeated evaluations without extending
the worker timeout. Coarse meshes can miss small features; narrower bounds improve
their resolution.
The volume search starts at −2 to 2 for omitted coordinate ranges. If no surface
is found, it makes at most eight expanding 13-point-grid searches, changing only
omitted endpoints. Once found, full-quality requests resample the expanded volume
at the normal grid density, and automatic bounds fit the mesh with padding.
When a discovered surface touches an automatic boundary, up to three additional
coarse searches expand the affected endpoints to look for a complete bounded
surface. If it remains open or the search exceeds its budget, the original volume
is retained, so unbounded surfaces do not force unlimited expansion. The selected
volume is then sampled at the requested detail. Edge refinement allows for
floating-point round-off near roots without treating poles as surface crossings.
Explicit endpoints are never expanded. An empty fully specified volume retains
its axes and zoom controls with a “No surface found in this view” notice. Implicit meshes use
equal coordinate units and a uniform camera scale, so a sphere remains spherical
at every viewing angle; explicit ranges remain untouched.
The contour search refines cells whose curvature could hide a closed component,
including small curves inside a tall manually selected viewport. A new expression
starts with automatic ranges rather than inheriting a different expression's zoom.
The bounded grid searches reject non-finite samples and pole-like crossings,
but cannot certify completeness or guarantee detection of tiny components, isolated
points or zero sets without a sign change. Explicit bounds can focus the search.

```text
Implicit curve: x^2+y^2=1; x and y from -1.2 to 1.2
```

The **Solutions** card stays visible below the plot, including during live range
updates. Its values are computed from the current equation using the same native
solver and numerical formatter as Evaluate, rather than retained from an older result.
Structurally opposite symbolic branches share a single ± row in plain text and
rendered mathematics. Unavailable or non-finite numerical approximations are omitted;
the symbolic solutions remain visible when their parameters are unset.
There is no generic left/right legend; hovering over a curve identifies its native
expression. Automatic equation framing samples both sides and their residual,
using connected crossings and detected turning points to choose padded horizontal
and vertical limits. Large outer tails do not flatten the root region. This is a
bounded viewing heuristic inside the initial domain, not a guarantee of finding
every solution. Without usable features, the ordinary automatic policy remains.
Explicit horizontal endpoints are preserved, manual vertical limits take precedence,
and **Fit all** bypasses feature cropping.
Evaluating again after a plot restores the mathematical result heading and the
Solutions card, including the numerical roots; plotting does not remove solver results.

Differential Equation mode generates asynchronous plots for explicit ordinary differential-equation
solutions, using the declared independent variable and the problem's initial
conditions. All returned explicit branches are drawn (up to sixteen). The plot
title shows the solved equation or equations; a single solution has no redundant
legend, while multiple branches retain numbered legends. With no or incomplete
initial conditions, up to two free integration constants per branch produce representative
family curves: one selects five values; two select a nine-member Cartesian grid.
Private copies promote the generated constants to sampling variables. Adaptive
sweeps use first and second derivatives with respect to each constant over
−4 to 4, at x = −2, 0 and 2 and companion-constant values −1 and 1.
Values are selected by quantiles of bounded angular ordinate sensitivity, with
a uniform floor to retain quieter regions. This bounded exploratory heuristic
is not a global search; when no usable sensitivity exists, fixed seeds are retained.
Authored parameters are never varied. The general solution stays in the title, marked as representative real
members; hover over a curve to see its selected constants in a tooltip, without
an overlapping family legend. Reaching a curve keeps it at full brightness and
dims the other solution curves; moving into empty plot space or leaving the plot
restores the whole family. These are illustrations,
not additional initial conditions or an exhaustive family. Non-real members in
the selected viewport are omitted. At most sixteen curves are drawn across all
branches; larger families require more initial conditions.

For fully automatic horizontal family bounds, a bounded sampler pass measures
ordinate changes in evenly spaced horizontal bins. It frames the central 80% of
that activity with padding, retains located stationary points, and leaves
edge-dominated growth alone. Extreme ordinates are limited for this framing
estimate so that poles do not dominate it. This is a viewing heuristic, not a
complete feature search. Explicit horizontal endpoints disable this refinement.
Live updates apply the selected ranges in Equation and Differential Equation
modes as well as Expression mode; manual vertical limits override automatic scaling.

Authored parameters are never varied automatically: bind any unresolved problem
parameters first. Implicit solutions, arbitrary functions and series-only results
are not guessed or silently replaced; use an explicit expression in Expression
mode where appropriate. The plot card explains unsupported implicit,
series-only and unsupported-dimensional results without removing the solver cards. A PDE with two independent variables and
one explicit solution is plotted as a refinable surface, including solutions constant
along either coordinate. The surface retains the usual camera controls, touch
gestures, previews and automatic/manual height limits. Higher-dimensional problems
and multiple PDE branches currently require selecting a concrete expression or
slice in Expression mode; no branch is silently chosen. Both solver modes share
native sampling and SVG rendering with Expression mode. Plot requests use `/eval` with
`action: "plot"` and `plot_source_mode: "equation"` or `"diffequation"`; the
unchanged mathematical source remains in `expression`.

The existing server keeps a bounded, shared-memory cache for Equation and
Differential Equation plots. Repeated identical requests reuse completed native
results; changed viewports reuse solved curves and representative family choices
but still sample the new view. Source, mode, precision and executable changes
invalidate the relevant records. Displayed numerical solutions retain worksheet
precision; heuristic family-parameter exploration is capped at 128 bits. Curve
sampling retains its independent precision validation and escalation policy.
There is no additional server or persistent cache. Restarting the server clears
the records; `MARS_LAB_PLOT_CACHE=0` disables reuse for profiling. Requests with an
explicit non-zero calculation work limit bypass the cache.

Implicit 2-D contours and 3-D isosurfaces use the same guarded native double plans.
Uncertain values and unsupported operations fall back to a separately parsed
equation with at least 128 bits and additional coordinate guard bits. Narrow or
out-of-double-range coordinate boxes use local unit coordinates for grid and edge
interpolation, then reconstruct native world coordinates before rendering. Thus a
large offset does not erase the distance between adjacent mesh vertices. Authored
bounds are parsed before conversion; grid density and edge-refinement limits are
unchanged. This is not a certified error bound or a guarantee of resolving arbitrary
ill-conditioning. Curve gestures and hover interpolation remain limited to
browser-sized coordinates. Surface zoom instead retains exact endpoint text and
sends native range recipes, so display precision does not limit the mathematical
bounds. Its C/WebAssembly camera and normalised GPU geometry still use display
precision, with the preview safeguards described under
[3-D surface interaction](#3-d-surface-interaction).
Symbolic solutions and worksheet precision
are unaffected. The expression guide describes the bounded sampling plan and its
numerical limitations.

Title composition stream-validates native chart SVG, then copies its body
verbatim into a small XML-built wrapper with the rendered TeX heading. This avoids
retaining, cloning and reserialising every triangle or curve segment. Geometry,
hover metadata and body whitespace are preserved; malformed XML remains rejected.
Documents with prologues or other non-native root layouts retain the general XML
tree path, and failed TeX rendering still produces an escaped plain-text heading.

Complete plot records larger than 1 MiB can now be compressed into the existing
1 MiB cache slots, with an 8 MiB decoded-record limit. Compression and decompression
run outside the shared cache lock; records that cannot fit still evaluate normally.
Reuse restores the exact worker output, including SVG coordinates and labels.

Adaptive curves use native first derivatives and repeated differentiation for
second derivatives. Tangent/secant and curvature checks supplement the value
probes; bounded bisection investigates first-derivative sign changes. Located
stationary heights with resolved neighbouring edges expand the automatic vertical
range, so they are not discarded merely as percentile outliers. Unsupported or
undefined derivatives retain value-only checks, and explicit limits still win.

Full automatic-height surfaces additionally inspect interior grid extrema. Up to
eight candidates receive at most twelve local Newton steps using the gradient and
Hessian (including mixed second derivatives), confined to neighbouring grid cells.
Finite small-gradient candidates with a definite Hessian contribute height limits;
saddles and unresolved or unsupported candidates do not. Preview and manual-height
plots skip this search. Extrema search changes the height range, independently of
midpoint-driven mesh refinement; flat or unsampled extrema can still be missed.

The final curve sampling pass uses 64 initial intervals, at most 8,193 evaluations and
eight adaptive subdivision levels. Validation probes additionally consume the
request's shared calculation budget. Detected domain gaps and
unresolved edges are not joined. Detection is heuristic: narrow features and
oscillations can still be missed. Finite samples near poles are clipped in native
precision before conversion to drawing coordinates, so large off-screen values
do not abort a manually bounded plot. Narrow the range for difficult curves; a
budget failure is reported rather than publishing a partial image. The current
Lab control draws one real-valued curve with selectable vertical bounds; the
library also supports multiple series, measured points and explicit viewports.

Expression mode parses, simplifies and evaluates scalar or matrix-valued
expressions. When variables are present, the buttons beneath the editor can
differentiate or integrate with respect to each variable. **Goal seek** finds a
numeric value for a selected variable.

Native display probes for complex-number formatting do not numerically evaluate
integrals or summations. Numerical quadrature belongs to the Value calculation,
not to the checks that choose a symbolic presentation. High-precision quadrature
can still exceed the Lab's calculation timeout; this separation does not reduce
the requested numerical precision.

The Expression-mode integral action recognises Gaussian cumulative integrals
directly, including affine upper endpoints and infinite lower endpoints. Native
integration produces an `erf`/`erfc` primitive with symbolic parameters and its
Gaussian decay condition; it does not numerically integrate the cumulative
integral again or increase the worker timeout. Evaluation of the original
probability and integration of that probability with respect to its endpoint
remain separate operations. See [symbolic integration](expression.md#symbolic-integration).
The native simplifier cancels common Gaussian factors and places convergence
conditions after the complete formula. For display, supplied real parameter signs
can remove roots of squares while keeping the parameters symbolic. A positive
width binding therefore cancels its root-to-width ratio; negative bindings use
the opposite sign. Conditions proved by the current parameter values are omitted.
Unknown or complex parameter signs retain their generic roots; conditions remain
unless the supplied values prove them.
The active integration variable stays free. These display choices are recomputed
on each evaluation and do not change the generic antiderivative.
The native [`expr_context_t` module](expr_context.md) owns these binding snapshots
and symbol roles, including the matching Function Run display path. No
assumption inference or algebraic rewriting is performed in the browser.
Equation solutions, differential-equation solutions and symbolic matrix results
also use the shared context simplifier in their native workers. Supplied named
constants can justify sign and domain simplifications; free-variable values remain
evaluation data, not assumptions. This does not change solver branch selection or
matrix pivot rules, and unknown restrictions remain visible.
The generated integration constant is the last term of the displayed formula,
before any domain condition; conditions remain after the binding bar in the
Expression card.

Supported elementary functions with an explicit symbolic complex argument are
presented in Cartesian `p + qi` form. The complete set comprises `exp`, `ln`,
`lg`; the circular functions `sin`, `cos`, `tan`, `sec`, `cosec`, `cot`;
their hyperbolic counterparts; and all twelve inverse circular and inverse
hyperbolic functions. This applies to inputs written with both parts—
`exp(x + iy)` is displayed as `exp(x)·cos(y) + exp(x)·sin(y)·i`—and to
pure-imaginary inputs, for which `sin(iy)` is displayed as
`sinh(y)·i`. A zero real component is omitted rather than displayed as `0 +`.

When the symbolic integrator has no supported closed-form primitive, the integral
button retains a formal integral with an arbitrary constant instead of reporting
an error. The Function card still calls `integral(integrand, variable)`; this
operation generates the constant and can be differentiated back to the integrand.

For example, enter `gamma(a+[time]i)` and select the *time* integral button.
The resulting antiderivative family, without its bindings, is:

```text
∫^[time] Γ(t·i + a)·dt + C
```

Here `t` is a dummy integration variable; `[time]` remains the result's free variable.

Logarithm input follows the calculator convention: `log(x)`, `log10(x)`, and
`lg(x)` all mean the base-10 logarithm, while `ln(x)` means the natural
logarithm. Result cards use `lg` and `ln` consistently, regardless of which
accepted alias was entered.

The derivative and integral buttons use the same separated Cartesian algebra
for differentiation or integration with respect to either component. Their
Rendered TeX, Expression and Function cards do not fall back to the original
unsplit function call. The imaginary unit is the final factor of the imaginary
term, and indefinite integrals place their constant of integration last in all
three representations.

Explicit fractional powers retain their complete root family. Their
derivatives likewise show every Cartesian branch; when bindings permit numeric
evaluation, the result card is titled **Values** and contains one value per
branch. Named `sqrt`, `cubrt` and `root` calls remain principal and
single-valued.

Additive ellipses are interpreted by the native expression parser. It first
tries exact geometric and inverse-index-power models, then uses a Lagrange
polynomial fitted to the supplied coefficient terms and verified against the
terminal term. The Rendered TeX derivation shows the inferred sigma before the
simplified formula and value. The browser neither extrapolates nor rewrites the
series.

The same native recognition covers finite `sin`, `cos`, `sinh` and `cosh`
progressions, including inputs such as
`cos(x)+cos(2x)+cos(3x)+...+cos(nx)`. MARS returns the corresponding geometric-
series closed form and uses its continuous value at removable singularities
such as `x = 0`. Integrating the cosine progression produces

$\quad\begin{array}{l}\displaystyle \frac{H_n(e^{ix})-H_n(e^{-ix})}{2i}+C, \qquad H_n(z)=\sum_{k=1}^{n}\frac{z^k}{k}.\end{array}$

Expression input accepts `Hn(n,z)`, `harmonic_poly(n,z)`, and `harmonicpoly(n,z)`. The Function card
uses `harmonicpoly`, and the same native node supports repeated symbolic
differentiation and direct integration.

Formal finite sums and products use `@Z_(k=1)^n term` and `@P_(k=1)^n term`.
The lower-bound parentheses may be omitted, as in `@Z_k=1^n term`;
`@Z_(k=1)^n` is the ASCII replacement for `Σ_(k=1)^n`.
Omitting the upper bound produces the corresponding infinite operator, as in
`@Z_k=1 term` or `@P_k=1 term`. The operator index is local: it does not create
a binding control. Mathematical output renders sums with Σ; Function output
uses `sum(k, 1, n, term)` when the summation itself remains the simplified
result.

Formal `sin(kx)`, `cos(kx)`, `sinh(kx)` and `cosh(kx)` sums from 1 to `n`
use the same native geometric-series identities as recognised ellipsis input.
For example, with supplied `x` and `n`, `@Z_(k=1)^n sin(kx)` displays

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\sin(kx) =\frac{\sin(nx/2)\sin((n+1)x/2)}{\sin(x/2)},\end{array}$

and supplies its numerical Value without iterating through a large upper
bound.

The exponential progression is geometric as well:

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}e^{kx} =\frac{e^x}{e^x-1}\left(e^{nx}-1\right).\end{array}$

For `{ @Z_(k=1)^n exp(kx) | x=2; n=100000 }`, MARS Lab displays that
identity, Expression style emits
`exp(x)/(exp(x) - 1)·(exp(nx) - 1)`, and Function style evaluates the same
formula using a shared `exp(x)` temporary. Its Value is
`9.110304914770879911502042940141264278041407847643843263784059825E+86858`.
At `x = 0`, MARS evaluates the removable limit directly and returns `n`.

The exponential-sine progression is the imaginary part of a complex geometric
series, so MARS also reduces it without iterating through its terms:

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}e^{kx}\sin(kx) =\frac{e^x\left(\sin x-e^{nx}\sin((n+1)x)+e^{(n+1)x}\sin(nx)\right)} {1-2e^x\cos x+e^{2x}}.\end{array}$

For `{ @Z_(k=1)^n exp(kx)sin(kx) | x=1; n=100000000 }`, the Value begins
`1.804482674473709321302888821113364953E+43429448` at 128-digit precision.

Products inside logarithms reduce the corresponding logarithmic progression:

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\ln(kx)=n\ln(x)+\ln\Gamma(n+1).\end{array}$

For `{ @Z_(k=1)^n ln(kx) | x=2; n=100000 }`, Expression style emits
`n·ln(x) + lnΓ(n + 1)`, Function style emits `n.ln(x) + lgamma(n + 1)`, and the
Value is
`1120613.939955116396071001320351928512056894536584146906526018233`.

Because `log` denotes the common logarithm, it remains distinct from `ln`:

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\lg(kx) =n\lg(x)+\frac{\ln\Gamma(n+1)}{\ln(10)}.\end{array}$

For `{ @Z_(k=1)^n log(kx) | x=2; n=100000 }`, the parseable result is
`n·lg(x) + lnΓ(n + 1)/ln(10)` and the Value is
`486676.4504663690278820372835671954350107214367484924045390786799`.

The summand function now owns any exact finite-progression reducer. This keeps
the recogniser independent of function names and lets related functions reuse
the same mathematics. For example,

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\operatorname{versin}(kx) =n-\frac{\sin(nx/2)\cos((n+1)x/2)}{\sin(x/2)}.\end{array}$

For `{ @Z_(k=1)^n versin(kx) | x=2; n=100000 }`, Expression style emits
`n - sin(nx/2)·cos(x/2·(n + 1))/sin(x/2)` and the Value is
`100000.0242173437116803434689891041295735315676686381885043434219`.
The other seven versed and haversed functions reduce through the same sine and
cosine progression operations.

The progression step may itself be a symbolic product. The recogniser removes
the local index wherever it occurs as one multiplicative factor, so `kax`,
`akx`, and `axk` all have step `ax`. For
`{ @Z_(k=1)^n sin(kax) | a=2; x=3; n=100000 }`, Expression style outputs
`sin(nax/2)·sin(ax/2·(n + 1))/sin(ax/2)` with the supplied bindings,
Function style introduces `v1 = a.x.` and evaluates
`v2 = v1/2.` before returning
`sin(n.v1/2).sin((n + 1).v1/2)/sin(v2)`, and the Value is
`-0.1868614750758504223995060240052491962444814290852506553280866826`.
Rendered TeX shows the original sigma followed by that identity. A nonlinear
argument such as `k²ax` retains another `k` after extraction and therefore
remains a formal sum.

Positive integer scaling also gives exact homogeneous progressions. In
particular,

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\sqrt{kx} =\sqrt{x}\left(\zeta(-1/2)-\zeta(-1/2,n+1)\right).\end{array}$

For `{ @Z_(k=1)^n sqrt(kx) | x=2; n=100000 }`, Expression style emits
`√(x)·(ζ(-1/2) - ζ(-1/2, n + 1))` and the Value is
`29814463.01298576613569741465397922838928192939324835606473553721`.
The analogous cube-root reducer uses exponent `-1/3`; `abs(kx)` and `conj(kx)`
use the triangular number `n·(n + 1)/2`.

Floor and ceiling use an arithmetic progression for a proved integral step. For
`{ @Z_(k=1)^n floor(kx) | x=2; n=100000 }`, all result cards retain the native
domain-required specialisation `x·n/2·(n + 1)` and the Value is `10000100000`.

A proved small rational step `x = p/q` has a repeating residue pattern. Writing
`m = floor(n/q)` and `r = mod(n, q)`, MARS reduces

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\left\lfloor\frac{pk}{q}\right\rfloor =m\left(\frac{pq(m-1)}{2}+p+\frac{(p-1)(q-1)}{2}+pr\right) +\sum_{s=1}^{r}\left\lfloor\frac{ps}{q}\right\rfloor .\end{array}$

The native expression expands the last sum into no more than `q - 1` residue
indicators, so it never conceals a large term-by-term loop. For
`{ @Z_(k=1)^n floor(0.6k) | n=100000 }`, Expression style outputs
`{ ⌊n/5⌋·(15/2·(⌊n/5⌋ - 1) + 7 + 3·mod(n, 5)) + (⌊1/5·(mod(n, 5) + 3)⌋ + ⌊1/5·(mod(n, 5) + 2)⌋ + 2·⌊1/5·(mod(n, 5) + 1)⌋) | n = 100000 }`
and the Value is `2999990000`. Rendered TeX shows the original sum followed by
this equality, while Function style emits the same period formula with
temporaries for `n/5`, `floor(n/5)`, and `mod(n, 5)` rather than
`sum(k, 1, n, ...)`.

Non-integral rationals currently require reduced denominator `q <= 32` and
`|p| <= 1000000`. MARS leaves an unset, irrational, or larger rational step
formal rather than claiming a reduction it has not proved. Ceiling uses the
corresponding ceiling residues.

The weighted hyperbolic sums `@Z_k=1^n sinh(kx)/k` and
`@Z_k=1^n cosh(kx)/k` have native Lerch-transcendent forms built from Li₁ and
Φ. MARS Lab always displays the applicable form, including when a stable
large-bound evaluator supplies the Value, so the interface never suggests that
an enormous number of terms was summed directly. TeX stays on one line when
space permits, Expression output uses `Li1` and the capital symbol `Φ`, and
Function output uses `li1` and `lerchphi`. **Use as input** copies that exact
parseable Expression representation back to the editor while retaining the
supplied bindings. Differentiating the copied weighted-sinh form recognises the
identity from which it came and returns

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\cosh(kx) =\frac{\sinh(nx/2)\cosh((n+1)x/2)}{\sinh(x/2)}.\end{array}$

The Value card evaluates the combined weighted sum through a stable native
path rather than exposing the large cancelling imaginary parts of individual
principal-branch terms. It also avoids iterating through every term for a very
large finite upper bound. The browser receives the simplified algebra,
renderings and value from MARSlib; it does not parse, simplify or substitute
the mathematics itself.

Weighted circular sums use the corresponding unit-complex Lerch form. For
example, the input

```text
{ -Σ_(k=1)^n cos(kx)/k | x = 2; n = 100000 }
```

has this Expression output:

```text
{ -½·(Li1(exp(ix)) - exp(ix)^(n + 1)·Φ(exp(ix), 1, n + 1) +
  (Li1(exp(-ix)) - exp(-ix)^(n + 1)·Φ(exp(-ix), 1, n + 1))) | x = 2; n = 100000 }
```

Its Function implementation reuses the reciprocal unit exponential:

```text
expression expr(x, const n) {
    const c1 = n + 1.

    v1 = i.x.
    v2 = exp(v1).
    v3 = 1/v2.

    return -1/2.(li1(v2) - v2^c1.lerchphi(v2, 1, c1) +
                  (li1(v3) - v3^c1.lerchphi(v3, 1, c1))).
}

x = 2.
const n = 100000.
output(expr(x, n)).
```

The accompanying Value is approximately
`0.52053867649950756146719704452337066`. An outer sign is retained in the
formula and numerical result instead of hiding recognition of the underlying
weighted sum.

The captured editor input is `1+1/2^p+1/3^p+...+1/n^p`; its binding boxes
supply `p = 2.5` and `n = 100`. MARS recognises

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\frac{1}{k^p} =\zeta(p)-\zeta(p,n+1),\end{array}$

and the supplied bindings produce approximately
`1.3408255697514640082147074818471`. At `p = 1`, MARS uses the harmonic
formula `digamma(n + 1) + gamma` consistently in the Rendered TeX, Expression
and Function cards, with the sigma summand shown as `1/k`; non-positive integer
exponents use the corresponding Faulhaber polynomial. With `n = inf`, `p = 2`
simplifies exactly to `pi^2/6`,
while another real `p > 1` evaluates to `zeta(p)`,
while a divergent infinite series has no finite Value card. A literal infinite
terminal term such as `1/inf^2` is also accepted.

Riemann zeta accepts `zeta(s)` or `ζ(s)`. Hurwitz zeta accepts the two-argument
forms `zeta(s,a)` and `ζ(s,a)`, as well as `zetah(s,a)` and `zeta2(s,a)`.
Their first-argument derivatives use the trailing-`p` names `zetap`,
`zatahp`, and `zeta2p`.

A finite tangent progression is shown with its q-digamma identity rather than
pretending that a large explicit summation was performed symbolically. MARS
uses the reduced identity

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\tan(kx) =in+\frac{\psi_{e^{4ix}}(1)-\psi_{e^{4ix}}(n+1)-\psi_{e^{2ix}}(1)+\psi_{e^{2ix}}(n+1)}{x}.\end{array}$

The Expression and Function cards show the same q-digamma formula. The Value
card evaluates the recognised finite combination by a stable native real sum:
the separate q-digamma terms are not assigned artificial values on the unit
circle. Using the displayed formula as input recovers the same summation,
identity, bindings and value.

Inverse circular and inverse hyperbolic progressions use log-gamma identities
where those identities are genuinely shorter than the original sum. Define

$\quad\begin{array}{l}\displaystyle D_n(s)=\ln\Gamma(n+1+s)-\ln\Gamma(1+s) +\ln\Gamma(1-s)-\ln\Gamma(n+1-s).\end{array}$

Then MARS uses

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\operatorname{acot}(kx)=\frac{D_n(i/x)}{2i}, \qquad \sum_{k=1}^{n}\operatorname{acoth}(kx)=\frac{D_n(1/x)}{2},\end{array}$

and

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\operatorname{atanh}(kx) =\frac{n\left(\ln x-\ln(-x)\right)+D_n(1/x)}{2}.\end{array}$

For real non-zero `x`, arctangent uses the sign-aware complex-shift log-gamma identity

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\operatorname{atan}(kx) =\frac{n\pi x}{2|x|}+\frac{i}{2}\left( \ln\Gamma\left(n+1+\frac{i}{x}\right)-\ln\Gamma\left(1+\frac{i}{x}\right) +\ln\Gamma\left(1-\frac{i}{x}\right)-\ln\Gamma\left(n+1-\frac{i}{x}\right) \right).\end{array}$

This uses `ln(-ix) - ln(ix) = -iπx/|x|`, keeps the imaginary unit out of the
denominator, and preserves the principal-logarithm branches for positive and
negative real `x`. The Value card evaluates the supplied real terms directly at
the active precision, while the reduced identity remains visible in every
non-Value card. At `x = 0`, the finite sum evaluates directly to zero. Other
displayed log-gamma formulae may likewise use their finite terms to cross a
removable numerical singularity without hiding the symbolic identity.

Differentiating the arctangent progression uses the compact conjugate-pair form

$\quad\begin{array}{l}\displaystyle \frac{\psi(n+1+i/x)-\psi(1+i/x)+\psi(n+1-i/x)-\psi(1-i/x)}{2x^2} =\sum_{k=1}^{n}\frac{k}{1+k^2x^2}.\end{array}$

MARS evaluates its complex digamma terms through the active arbitrary-precision
backend and returns the mathematically real result. At 386 requested digits,
`{ @Z_(k=1)^n atan(kx) | x=1; n=1000000000 }` therefore returns a 386-digit
derivative Value beginning `20.6286155168239341793067112756040338`, without a
spurious imaginary residue.

The other inverse circular, inverse versed and inverse hyperbolic functions
have no shorter identity in MARS's current special-function vocabulary. Their
Rendered TeX and Expression cards therefore keep the sigma visible, and the
Function card explicitly retains `sum(...)`. With supplied finite bounds of at
most one million terms, the Value card evaluates that displayed finite sum
directly. The unchanged sigma and `sum(...)` make this numerical route visible;
they do not imply an undisplayed closed form. When a sum exceeds the safe direct
evaluation limit and has no supported shortcut, the Value card instead displays
`Value not computed: the finite sum exceeds the safe direct-evaluation limit and
has no supported numerical shortcut.`

Integrating an inverse-function progression likewise operates term by term and
retains the finite sigma when the resulting weighted logarithmic sum has no
shorter supported form. MARS does not attempt a large numerical sum merely to
populate the integral Value.

An exact inverse-function pole remains a numerical result rather than becoming
an absent Value. For example,
`{ @Z_(k=1)^n acoth(kx) | x=1; n=1000000000 }` contains
`acoth(1)` as its first term, so its Value is `∞`; changing `x` to `-1` gives
`-∞`. The same signed-infinity handling applies when a finite `atanh(kx)`
progression reaches `kx = 1` or `kx = -1`.

The corresponding hyperbolic-tangent progression uses a real q-digamma
identity. With `q = exp(-2x)`, MARS uses

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\tanh(kx) =n-\frac{2}{\log q}\left(\psi_q(1)-\psi_q(n+1)\right) +\frac{4}{\log(q^2)}\left(\psi_{q^2}(1)-\psi_{q^2}(n+1)\right).\end{array}$

For example, `{ @Z_(k=1)^n tanh(kx) | x=2; n=100000 }` displays this
identity in Rendered TeX, uses `ψq` in Expression style and `qdigamma` in
Function style, and produces
`99999.96334436219613677993759997968616482017617755510022062027824`.
Using that displayed Expression as input recovers the sum identity and the
same value.

The cotangent, secant and cosecant families use the same native q-digamma
machinery. Write

$\quad\begin{array}{l}\displaystyle D_q(a,n)=\psi_q(a)-\psi_q(a+n).\end{array}$

Then the cotangent pair is

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\cot(kx)=-in-\frac{2i}{\log q}D_q(1,n),\qquad q=e^{2ix},\end{array}$

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\coth(kx)=n+\frac{2}{\log q}D_q(1,n),\qquad q=e^{-2x}.\end{array}$

The cosecant pair follows from
`cosec(y) = cot(y/2) - cot(y)` and
`cosech(y) = coth(y/2) - coth(y)`:

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\operatorname{cosec}(kx) =-\frac{2i}{\log q}D_q(1,n)+\frac{2i}{\log(q^2)}D_{q^2}(1,n), \qquad q=e^{ix},\end{array}$

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\operatorname{cosech}(kx) =\frac{2}{\log q}D_q(1,n)-\frac{2}{\log(q^2)}D_{q^2}(1,n), \qquad q=e^{-x}.\end{array}$

Finally, let `a = pi*i/(2*ln(q))`. The secant identities are

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\sec(kx) =\frac{i}{\log q}\left(D_q(1-a,n)-D_q(1+a,n)\right), \qquad q=e^{ix},\end{array}$

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\operatorname{sech}(kx) =\frac{i}{\log q}\left(D_q(1-a,n)-D_q(1+a,n)\right), \qquad q=e^{-x}.\end{array}$

MARS displays each identity in Rendered TeX, writes `ψq` in Expression style
and `qdigamma` in Function style, and recovers the original finite sum when the
displayed Expression is used as input. Circular values and the
shifted-argument `sech` value are evaluated through the recognised real finite
sum, avoiding unsupported unit-circle terms and spurious complex cancellation;
the remaining real-q hyperbolic forms are evaluated directly.

The result cards deliberately show different representations of the native
calculation:

- **Rendered TeX** shows the simplified mathematical result without bindings.
- **Expression** shows the parseable MARS expression, including variable and
  constant bindings when the input has them. Riemann and Hurwitz zeta use the
  shared mathematical symbol `ζ`, distinguished by their one- and two-argument
  forms.
- **Function** shows MARS function source. Its **RUN** control executes the
  initial scalar, equation and matrix Ophelia subset described below. Reused expression-DAG nodes
  are named once as intermediate constants or variables before the return
  expression. A shared subexpression such as `x/2` is assigned once and reused.
  When both `exp(x)` and `exp(-x)` are needed, the second temporary reuses the
  first as `v2 = 1/v1`.
  For a top-level Laplace, Fourier or inverse-transform request, the programme
  preserves the original transform operation: RUN performs it instead of returning
  a precomputed formula. TeX and Expression continue to show the simplified result.
- **Value** appears whenever supplied bindings allow a numerical result. It is
  the only card that substitutes those bindings; it also appears when
  simplification proves a binding-independent value despite an unset binding.

**Use as input** takes its source from the native parseable Expression result,
not from TeX or Function presentation. Its binding-aware transfer preserves
the expression body's order and notation while applying the result bindings to
the editor controls.

Binding controls retain the native Expression spelling internally when
editing, clearing or transferring bindings. Their visible name labels,
tooltips and derivative/integral buttons omit enclosing square brackets.
The calculus buttons italicise only the variable name, leaving the action word
upright. Multi-character names keep their square brackets in Expression syntax, while ordinary Ophelia
identifiers are unbracketed in Function source. Rendered TeX also omits these
brackets, presenting each multi-character name as a single italic identifier.
Native TeX inserts a small centred dot between these names and adjacent factors
to make multiplication clear.
Products of single-letter symbols retain their existing notation.

The current Expression capture shows `acos(sin(2-x*y))` with both plotting
variables from −3 to 3. The output is the principal-value surface
`u(x,y) = acos(sin(2-x*y))`, with equal coordinate units and the compact
azimuth, elevation, roll and coordinate-volume zoom controls beneath it.

[![MARS Lab expression mode plotting the principal-value inverse-cosine surface with compact 3-D controls](images/mars-lab/expression.png?v=20261016-full)](images/mars-lab/expression.png?v=20261016-full)

Function cards use MARS syntax rather than C syntax. A full stop terminates a
statement, `.` within a statement denotes multiplication, and `/` is printed
without surrounding spaces. Constants use `const`; arrays use `array`; and
generated intermediate values use compact names such as `c1` and `v1`.
The source annotation above a generated function is enclosed by single
backticks, so it remains one valid delimited comment even when the card wraps
it over several visual lines. Two backticks remain the separate line-comment
syntax.
Typeable named constants use `@pi`, `@phi`, `@gamma`, `@tau` and `@inf`, with
their own syntax colour. Values edited in Expression-mode binding controls are
committed to the expression before evaluation, differentiation or integration.
Each populated binding input has an × button inside its right edge. It clears
the value using the same rules as deleting it manually. For ordinary bindings,
this leaves the value unset and returns focus to the input. The button is also
accessible by keyboard.
Descriptive `$[...]` identifiers remain accepted as input aliases for
bracketed names and use the subdued off-white italic styling of variable and
constant names, but result cards do not generate the `$[...]` form.
The syntax colouring distinguishes keywords, functions, variables, numbers and
comments. Function-call brackets use the same gold hue as operators without
bold weight, while grouping brackets retain the ordinary text colour. The
colouring does not alter the copyable Function text.

### Running Function cards

Every card titled **Function** has a **RUN** button, including the cards in
Expression, Equation and Matrix modes. RUN submits the card's complete,
unabbreviated source at the selected precision to the native Ophelia prototype.
It does not re-evaluate the editor input, execute JavaScript or change the
Expression and Value cards. Output and source-position diagnostics appear in
the Function card's **Run output** area. Replacing the result or switching modes
clears this area and prevents an older response from appearing on a newer card.
Evaluate again after editing bindings before pressing RUN.

The first supported subset accepts one expression, equation or matrix function with scalar
parameters, variable and constant assignments, generated intermediate values,
returns, numerically decidable comparisons, symbolic scalar domain guards, and
top-level output calls. Paired-backtick comments and double-backtick line
comments are accepted. A full stop followed by whitespace or the end of the
statement terminates it; an internal full stop multiplies. Commas can separate
ordinary assignments on one line. Function-local constant definitions retain
their exact algebra, including surds, rather than appearing as generated names
with decimal bindings in symbolic results. External parameters and unset local
constants remain bindings; mutable scalar assignments retain their live values.

Bracketed multi-character symbolic names are supported in expressions,
declarations, function parameters and transform coordinates. Their contents are
one name, not a product of letters or an array literal.

For example, RUN executes this Function source:

```text
expression expr(x, y) {
    return sqrt(x^2 + y^2).
}
x = 3.
y = 4.
output(expr(x, y)).
```

Output:

```text
5
```

Derivative and integral Function cards retain the requested operation, so RUN
performs the calculation. This applies both to authored calculus requests and
to the derivative/integral buttons. The TeX and Expression cards still show the
evaluated algebra.

Nested operations remain nested in both the Function programme and the
left-hand side of the transform's TeX identity. An unresolved transform also
retains those operations in TeX, without claiming an evaluated equality.
For input `@L{@S^t sin(x) dx}`,
the Function card contains:

```text
expression expr(s) {
    if (realpart(s) > 0) {
        return laplace(integral(sin(x), x, t), t, s).
    } else {
        return @nan.
    }
}
s = ?.
output(expr(s)).
```

RUN output:

```text
{ -s/(s² + 1) | s = ?; Re(s) > 0 }
```

The inner integral remains visible on the left of the TeX identity; it is not
replaced there by its evaluated primitive. The integration coordinates `x` and
`t` are local to their calls, not programme input bindings.
The Function card carries the same domain restrictions as the result, as an
`if` guard with an `@nan` fallback. Supplying a valid numerical binding does not
remove this guard from the generated function. With unset bindings, RUN retains
the condition on its symbolic result; outside the domain it returns `NAN`.

For an indefinite integral:

```text
expression expr(x, const C) {
    return integral(sin(x), x).
}
x = ?.
const C = ?.
output(expr(x, C)).
```

RUN output:

```text
{ -cos(x) + C | x = ?; C = ? }
```

The two-argument call generates the integration constant itself. Its parameter
allows a supplied value of that constant to be used; the programme does not
append `+ C`. Definite integrals retain the four-argument form
`integral(expression, variable, lower, upper)` and generate no arbitrary constant.
Symbolic RUN output places the integration constant last. Like the main integral
cards, it uses supplied constant bindings to simplify parameter square roots and
omit conditions they prove, without replacing parameter names by numbers. Free
variables remain symbolic; unknown conditions remain visible. Each run rebuilds
this display from the current bindings.
Supplied bindings are preserved in the executable programme, including exact
symbolic values. With the indefinite integral above, setting `x` to `pi` and `C`
to zero produces:

```text
expression expr(x, const C) {
    return integral(sin(x), x).
}
x = @pi.
const C = 0.
output(expr(x, C)).
```

RUN output:

```text
1
```

The integrand always comes first and the integration variable second. For example:

```text
output(integral(sin(x), x, 0, @pi)).
```

Output:

```text
2
```

An explicit upper endpoint selects a primitive without adding an arbitrary
constant. Thus `@S^z sin(x) dx` generates this programme, and RUN agrees with
the TeX and Expression cards:

```text
expression expr(z) {
    return integral(sin(x), x, z).
}
z = ?.
output(expr(z)).
```

RUN output:

```text
{ -cos(z) | z = ? }
```

This is not a definite integral from zero: at `z = 0`, this chosen primitive
is `-1`, whereas the definite integral with both limits zero is `0`.

For a derivative:

```text
expression expr(x) {
    return derivative(sin(x), x, 1).
}
x = ?.
output(expr(x)).
```

RUN output:

```text
{ cos(x) | x = ? }
```

For an inverse Laplace request, the Function card keeps the calculation executable:

```text
expression expr(t) {
    return inverselaplace(5.(s + 3 + 10/s^2)/(s^2 + 4.s + 5), s, t).
}
t = ?.
output(expr(t)).
```

RUN output:

```text
{ 10t + exp(-2t)·(13·cos(t) + 11·sin(t)) - 8 | t = ? }
```

Here `s` is the transform's local source coordinate, not an uninitialised function
argument. Changing `t` to a numerical binding allows RUN to evaluate the result.

An unset binding may be written as `?`. Ordinary scalar output falls back to
native algebraic expression output when no numerical value is available;
`outputa` explicitly requests algebraic output. Stored expressions retain their
variable dependencies when numeric bindings are subsequently changed.
RUN displays unset variable and constant bindings as `name = ?`, matching the
Expression card; a standalone undefined numerical result remains `NAN`.
Generated scalar domain guards can retain an unset binding symbolically when
their `else` block returns `@nan`. Supported predicates are strict real-part
lower bounds, nonzero comparisons, real-variable checks and conjunctions of these
predicates. RUN preserves the native domain conditions alongside the returned
expression; it does not assume that an unknown condition is true or false.
For example:

```text
expression expr(s) {
    if (realpart(s) > 0) {
        return 1/s.
    } else {
        return @nan.
    }
}
s = ?.
output(expr(s)).
```

RUN output:

```text
{ 1/s | s = ?; Re(s) > 0 }
```

Supplying a binding evaluates the condition normally; values outside the domain
return `NAN`. Expressions constructed with symbolic guards retain their variable
dependencies for subsequent binding changes. General symbolic branching, including
an unknown condition whose branches return different ordinary expressions, is not
yet supported and produces a diagnostic. Boolean combinations retain normal
short-circuit behaviour, including when a known operand determines their result.

Equation functions return native equation values. `solve` accepts one equation
value and returns the native solver's solution set; output prints every returned
solution rather than choosing one root. Generated equation programmes use
`outputa()` to retain exact fractions and surds; an explicit `output()` still
requests numerical evaluation. For example:

```text
equation equ(Y) {
    return equation(26.Y = 320/9).
}
`` Y = ?
outputa(solve(equ(Y))).
```

Output:

```text
Y = ¹⁶⁰⁄₁₁₇
```

The commented hint leaves `Y` as an implicit uninitialised variable. Constant
parameters remain constants during solving. Equation output preserves the
left-hand side and evaluates the right-hand side when possible; `outputa`
keeps it algebraic. Unsuccessful solving reports that no solution was established,
not that none exists, unless the native solver proves an empty solution set.
Any native search limitations or separate solution-family notes are also printed.

Matrix functions execute the native generated return body, including shared
scalar temporaries, a common scalar prefactor and an additive constant matrix.
Scalar parameters may be real or complex. Matrix Function initialisers retain
authored exact bindings, including named constants, fractions and surds;
decimal input remains decimal input.
Named constants such as pi use mathematical symbols in both Expression layouts
and their Function aliases in Function initialisers, rather than decimal
approximations. `output` evaluates every entry when possible; otherwise it retains
the symbolic matrix and its bindings. `outputa`
always requests algebraic output. Long symbolic results use the native multiline
matrix layout, with shared non-vanishing powers factored out and their bindings
retained. For example:

```text
matrix mat(x) {
    const scale = 2.
    v1 = x^2.
    return scale.(v1, x; x + 1, v1).
}
x = 3.
output(mat(x)).
```

RUN output:

```text
(18, 6; 8, 18)
```

Generated matrix functions use the same principal complex powers as Matrix
mode. Fractional powers of negative eigenvalues can therefore produce complex
entries; they are not coerced to real values.

This is not yet the full Ophelia language. Matrix-valued parameters and assignments,
arrays, convolution, loops, recursion, multiple function definitions, nested
user-function calls, in-programme precision changes and undeclared captures
from an outer scope are not implemented. Equation and solution-set assignments
and the two-argument `solve(e, variable)` design are not implemented yet;
the generated one-argument solve-and-output form is supported. Native mathematical calls retain their
current expression semantics; an unresolved transform or formal derivative can
remain symbolic. Unsupported statement forms report an error rather than being
sent to a shell or silently ignored.

Ophelia is a built-in server mode. The command
`tools/mars_lab/build/release/mars_lab --worker ophelia` reads programme source
from standard input and accepts an optional decimal-precision argument after
`ophelia`. Lab requests have a 30-second
execution timeout. The prototype limits source to 64 KiB, scope size to 256
symbols, parameters to 64, nesting to 32, executed statements to 2048 and output
calls to 128. Shell, network and file-access operations are not language features.
The wider design remains in the
[Ophelia design notes](design-notes/expression-language.md).

## Equation mode

Numerical solution rows normally use ordinary decimal notation at the selected
significant-digit precision. Values below `0.0001` use `×10` with a superscript
exponent instead of `E` notation. Real and imaginary components follow the same
rule, including genuinely tiny imaginary components. Positive exponents are
expanded to decimal notation up to exponent 4096; larger magnitudes retain the
compact superscript form to bound output size. Exact symbolic solutions and
Function-card source are not rewritten by this display policy.

Zeta equations with an unset unknown use a bounded multi-start Newton search.
For a zero target, the cards show up to forty numerically verified non-trivial
zeros in the critical strip up to imaginary magnitude eighty; the trivial
negative-even family is labelled separately. Both **Rendered TeX** and
**Solutions** combine numerical conjugates into one `±` row, so twenty rows
can show forty roots at the selected precision. The native backend compares
the actual numbers before pairing; it does not merge rounded display values
or sampled symbolic families. Complex roots whose real and imaginary components
are exact rationals use `=`, including their numerical evaluations; inexact
components retain `≈`. Exact algebraic conjugates are paired structurally,
retaining surds and shared denominators with `=` rather than `≈`. Certified
rational factors preserve short cubic and quartic surds. General exact
real-coefficient cubics use coupled Cardano cube roots; non-degenerate quartics
use Ferrari radicals when no shorter factorisation is certified. RUN output
retains these exact forms too. Long solutions use shared, collision-free
single-letter definitions: the cards show definitions and root rows, while
algebraic RUN output puts the definitions after a binding bar in a curly-braced
solution block. Degenerate cases retain their existing handling, and inexact or
complex coefficients retain the numerical fallback. See the
[equation guide](equation.md) for details and examples.
Both cards omit a unit coefficient before the imaginary unit in conjugate
pairs. The search is not exhaustive. A literal
inverse-power series starting at one retains its convergence domain and, when
equated to zero, reports no solutions in that domain. It does not acquire
analytic-continuation zeros. **Solutions** says only **No solutions** for a
proved empty solution set; an unsuccessful numerical search remains a distinct
status. With no solution rows, **Rendered TeX** retains the entered equation in
summation notation. For a recognised infinite inverse-power sum, it also shows
the native zeta identity with the restriction that the real part of its argument
exceeds one. The identity uses the actual index, exponent and lower bound.
The editor, saved input, bindings, history, Equation and Function cards retain
the original problem. **Evaluate** does not start a separate zeta solve, and
there is no additional root card.
The native backend supplies the mathematics; the client only displays it.
Restart a running Lab server and reload the page after updating the client.

Equation mode tries symbolic isolation first and uses the numeric solver when a
symbolic result is unavailable. Bindings after `|` distinguish variables from
constants and supply starting values for numeric solving.
Equation calculations may run for up to 40 seconds before the Lab reports a
timeout. Other modes retain their own time limits.

The captured input is `(sqrt(x^2+y^2)-2)^2+z^2=1`. The symbolic output is
`z = ±sqrt(1-(sqrt(x^2+y^2)-2)^2)`; the automatic implicit plot shows the
complete torus, with equal coordinate units and the retained 3-D controls.

[![MARS Lab equation mode showing a torus and its symbolic height branches in automatic mode](images/mars-lab/equation.png?v=20261016-full)](images/mars-lab/equation.png?v=20261016-full)

## Differential-equation mode

Differential-equation mode accepts ordinary and partial derivatives,
polynomial differential operators, differential forms and optional initial or
boundary conditions. MARSlib selects a matching rule and, when derivations are
enabled, returns the rule-derived working shown in the **Solver** card.

The captured input is `y''+6y*y'+4y^3=0; y'(0)=0`. The modified Emden
solver returns the one-constant family

$\quad\begin{array}{l}\displaystyle y = \frac{C_1(2C_1x+1)}{2C_1x(C_1x+1)+1}.\end{array}$

The zero-slope condition removes one arbitrary constant. The compact TeX
fraction, representative real solution curves and solver working are shown together.

[![MARS Lab differential-equation mode displaying the modified Emden solution family, representative curves and solver working](images/mars-lab/differential-equation.png?v=20261016-full)](images/mars-lab/differential-equation.png?v=20261016-full)

Select **Help** in this mode for the accepted prime, `D`, partial-derivative and
differential-form notation. Arbitrary constants are preserved when the problem
has no conditions.

## Matrix mode

Matrix mode accepts complete numeric and symbolic matrix expressions. Spaces
separate columns and semicolons separate rows in compact input; comma-separated
entries are also accepted. Write matrix functions directly in the expression,
so `sin(1 2; 4 5)` means the sine of that complete matrix. The **Matrix
operation** selector provides **Evaluate expression**, **Inverse**, **Multiply
by another matrix**, **Eigenvalues**, **Eigendecompose**, **Characteristic
polynomial**, **Determinant**, **Trace**, **Rank**, **Simplify symbolic matrix**
and **Solve A X = B**. Functions such as the inverse, logarithm and
trigonometric families require a square matrix.

MARS Lab passes the entered text unchanged to
`mat_expression_from_string(...)`. MARSlib owns the complete grammar and
performs all matrix parsing and evaluation; neither the browser nor the native
MARS Lab helper interprets matrix-expression syntax.

Rendered matrix values automatically fit the available Value-card width at the
default zoom. Zoom and expansion recalculate that fit, so a wide matrix remains
visible without changing the native matrix output.

### Matrix-expression notation

Matrix expressions may be grouped and composed directly. `.` is matrix
multiplication, while `+` and `-` combine equally sized matrices. Integer,
fractional and symbolic powers use `^`; for example,
`((1 2; 3 4) - lambdaI)^x` is accepted. Where the matrix order is clear,
`lambdaI`, `lambda.I` and `lambda*I` all mean the scalar `lambda` multiplied by
the identity matrix. Matrix division is deliberately not defined because the
side on which an inverse should act would be ambiguous.

The structural function names and their aliases are:

| Operation | Accepted notation |
|---|---|
| Inverse | `inverse(A)`, `inv(A)` |
| Determinant | `det(A)`, `determinant(A)`, `|A|`, `||A||`, `‖A‖` |
| Trace | `trace(A)`, `tr(A)` |
| Transpose | `transpose(A)`, `trans(A)` |
| Conjugate transpose | `hermitian(A)`, `adjoint(A)`, `ctranspose(A)`, `conjtrans(A)`, `conjugate_transpose(A)`, `A^dagger`, `A^H`, `A^*`, `A^†`, `A†` |

Any unary scalar function supported by the native expression registry may be
written around a square matrix. This includes exponential, logarithmic,
trigonometric, inverse-trigonometric, hyperbolic, error, gamma, normal-density,
Lambert W and exponential-integral families. The parser reports the canonical
function name even when an alias such as `log`, `log10`, `Γ` or `productlog`
was entered. In particular, `log` and `log10` report the canonical base-10 name
`lg`, whereas natural logarithms report `ln`. Exact symbolic matrices are
supported where MARSlib has an exact
structured rule; otherwise, bind the entries to obtain a numeric matrix before
applying a general numeric matrix function. Symbolic exponents on supported
constant diagonalizable numeric square matrices are retained for any matrix
order; the exact `1 x 1` and eligible `2 x 2` cases additionally retain exact
spectral projectors.

Determinant bars must be paired: an input beginning with `|` or `||` without
the corresponding closing delimiter is rejected. A determinant is a scalar,
not a one-by-one matrix. The function vocabulary is recognised by MARSlib's
native collision-free lookup table; the browser does not recognise or rewrite
these names.

Scalar entries use the expression grammar. `conj(z)` and `conjugate(z)` are
equivalent to the postfix form `z^*`. Likewise, `abs(z)` and `|z|` denote the
same scalar absolute value. For a complex expression this is the modulus
`sqrt(z*z^*)`. Context distinguishes scalar absolute-value bars from the
determinant bars surrounding a matrix.

`sqrt(z)`, `cubrt(z)`, and `root(z,n)` return one principal scalar root. Exact
complex arguments are written in Cartesian `a + bi` surd form when such a form
is available. Explicit fractional-power syntax such as `z^(1/n)` instead
denotes the complete family of `n` roots when MARS Lab presents an expression
result.

Greek names may be entered as Unicode or through their ASCII aliases. Thus
`lambda`, `@lambda` and `λ` identify the same symbol and are normalised to `λ`
in output. This also applies inside compact matrix literals and identity
multiples.

Entrywise calculus uses `Dx(A)` for differentiation. Write `@S(A)dx` for an
indefinite entrywise integral with an additive constant matrix, or `@S^x(A)dx`
for the corresponding antiderivative without additive constants. Repeating a
derivative variable requests higher-order calculus, as in `Dxx(A)`, while a
suffix containing distinct variables requests ordered mixed calculus, as in
`Dxy(A)`. The variable buttons
below the editor invoke the corresponding first-order native operation. A
matrix antiderivative is displayed as `A(x) + C`, where `C` is a constant
matrix with entries such as `C₁₁`, `C₁₂`, `C₂₁` and `C₂₂`.

| Input | Output |
|---|---|
| `Dxx(x^3 xy; y^2 x^2y)` | `(6x, 0; 0, 2y)` |
| `Dxy(x^2y x*y^2; y^3 x^3y)` | `(2x, 2y; 0, 3x²)` |

For a symbolic matrix-power example, enter `A^x` with `A = (1 2; 3 4)`.
Writing

- `λ₊ = (5 + √33)/2` and `λ₋ = (5 - √33)/2`, and
- `P₊ = (A - λ₋I)/√33` and `P₋ = (λ₊I - A)/√33`,

the evaluated expression is `A^x = λ₊^x P₊ + λ₋^x P₋`. The **x derivative**
button returns
`ln(λ₊)λ₊^x P₊ + ln(λ₋)λ₋^x P₋`. The **x integral** button returns
`λ₊^x P₊/ln(λ₊) + λ₋^x P₋/ln(λ₋) + C`, where `C` is the independent constant
matrix. MARS Lab expands these projector expressions into a `2 x 2` matrix in
the result cards while retaining the exact `√33` terms.

MARSlib applies the spectral scalar rule before reconstructing the matrix. In
compact notation, `d(A^x)/dx = A^x ln(A)` and, when `ln(A)` is invertible,
`∫A^x dx = A^x inverse(ln(A)) + C`. An eigenvalue-one projector is integrated
as a linear term rather than divided by zero. The same machinery supports
ordered higher and mixed derivatives and iterated integrals, and is not limited
to `2 x 2` matrices.

<figure>
  <a href="images/mars-lab/matrix.png?v=20261016-full"><img src="images/mars-lab/matrix.png?v=20261016-full" alt="MARS Lab evaluating the symbolic matrix power (1 2; 3 4) raised to x"></a>
  <figcaption><em>After clicking <strong>Evaluate</strong>.</em></figcaption>
</figure>

<br>

<figure>
  <a href="images/mars-lab/matrix-power-derivative.png?v=20261016-full"><img src="images/mars-lab/matrix-power-derivative.png?v=20261016-full" alt="MARS Lab differentiating the symbolic matrix power with respect to x"></a>
  <figcaption><em>After clicking <strong>x derivative</strong>, with the TeX card at 67% to show the complete matrix.</em></figcaption>
</figure>

<br>

<figure>
  <a href="images/mars-lab/matrix-power-integral.png?v=20261016-full"><img src="images/mars-lab/matrix-power-integral.png?v=20261016-full" alt="MARS Lab integrating the symbolic matrix power with respect to x and displaying the constant matrix"></a>
  <figcaption><em>After clicking <strong>x integral</strong>.</em></figcaption>
</figure>

Result cards have distinct purposes:

- **Rendered TeX** shows the exact symbolic result. Long decimal mantissas are
  abbreviated with an ellipsis by default. Large integers use 23 significant
  digits followed by an ellipsis and an `e` exponent in the Expression and
  Function cards, for example `1.3322938598456934859438...e+45`.
  **Show more digits** reveals the complete exact integer in those cards.
  Rendered TeX instead uses a multiplication sign and a power of ten. The
  Value card is never abbreviated and wraps its complete numerical value.
- **Expression** shows the same native simplified matrix expression as a
  bracketed grid. Variable and constant bindings are placed on a separate row
  beneath the matrix, and unset bindings are shown as `?`. Copy still returns
  the complete native expression in reusable curly-brace notation.
- **Function** shows the same result as a native MARS matrix function, followed
  by the declarations and initialisations for its bindings. Repeated
  calculations shared by several entries are assigned once to intermediate
  variables and reused throughout the returned matrix.
- **Value** appears only when every matrix entry can be evaluated numerically.
  It remains hidden while a binding or integration constant is unresolved, and
  its complete numerical entries wrap within their columns rather than being
  abbreviated. For example, setting `lambda` to `3` evaluates
  `(1 2; 3 4) - lambdaI` to `(-2 2; 3 1)` without replacing the symbolic
  algebra in the other cards.

MARSlib creates all four representations from the same simplified matrix. The
browser transports and displays the native full or abbreviated fields; it does not parse,
simplify or reinterpret the matrix mathematics.

Symbolic matrix calculus constructs expression DAGs. It does not depend on the
numeric automatic-differentiation mode: the scalar expression evaluator has a
reverse-mode gradient path, while a numeric forward-mode JVP path is a separate
future facility. The symbolic Jacobian is therefore not described as either a
JVP or a VJP.

**Use as input** copies the reusable result expression back into the Matrix
editor. **Back** and **Forward** navigate the Lab's Matrix workspace history;
the editor, selected operation and right-hand operand are retained between
sessions.

The direct symbolic forms use the same editor:

| Input | Output |
|---|---|
| `inverse(a b; c d)` | `1/(ad - bc).(d, -b; -c, a)` |
| `det((1 2; 3 4) - lambdaI)` | `(1-λ)(4-λ)-6` |
| `tr(a b; c d)` | `a+d` |
| `(a b; c d)^dagger` | `(conj(a) conj(c); conj(b) conj(d))` |
| `(a b; c d).(e f; g h)` | `(ae+bg, af+bh; ce+dg, cf+dh)` |
| `inverse(a b; c d).(x; y)` | `1/(ad - bc).(dx - by; ay - cx)` |
| `Dx(ax+b cx+d; y xy)` | `(a, c; 0, y)` |
| `Dxx(x^3 xy; y^2 x^2y)` | `(6x, 0; 0, 2y)` |
| `Dxy(x^2y x*y^2; y^3 x^3y)` | `(2x, 2y; 0, 3x²)` |
| `@S(ax+b cx+d; y xy)dx` | `(½(ax²+2bx), ½(cx²+2dx); xy, ½x²y) + (C₁₁, C₁₂; C₂₁, C₂₂)` |

## Integrator mode

Integrator mode also generates an independent asynchronous Plot card on Evaluate.
For one integration variable it draws the integrand, not the antiderivative.
The plot title renders the integrand in TeX without its worksheet bindings.
Finite definite limits are marked with dashed lines and the interval is shaded
between the curve and zero: teal for positive contributions, gold for negative
ones. Reversing the integration limits reverses the contribution colours.
Plot zoom and range edits do not change the integration limits or recompute the
integral. The curve uses drawing precision independently of the requested result
precision. Gaps are not filled, and shading does not assert convergence.
Infinite integration limits shade the visible part of the interval; only finite
endpoints receive dashed markers. Indefinite and unresolved-limit integrals show
an unshaded curve; multidimensional integrands report that their plots are not yet available.

Integrator mode combines exact symbolic antiderivatives with the numerical
integrator. Add one bound row for each variable to be integrated. Leave both
bounds blank for an antiderivative, or enter lower and upper bounds for a
definite integral. Mark a symbol **Free** when it is a parameter rather than an
integration variable. The work-budget selector limits numerical fallback.

The calculation deadline is separate from that numerical work ceiling. In
seconds it is the smaller of 120 and the sum of three allowances: a 30-second
base, the work ceiling divided by 500 and rounded down, and 10 seconds for each
additional block of 96 requested decimal digits beyond the first. Partial
precision blocks count as complete blocks. This gives high-precision requests
extra time even with a small work ceiling, while every calculation remains
bounded by two minutes. Arithmetic saturates at the upper limit before large
precision or work-ceiling values can overflow.

When that worker deadline expires, the evaluation response has HTTP status 422,
`ok: false`, `error_code: "ETIMEDOUT"`, the deadline in `timeout_ms`, and a
timeout-specific `error` message. Worker availability and output-limit failures
retain a separate generic diagnostic. This deadline applies to the integration
worker; subsequent binding and rendering work has its own time limits. Increasing
the deadline does not change the requested precision, numerical tolerance or
work ceiling.

The captured input is `exp(-x^2/2)` with `x` from `-inf` to `2`.
MARS returns the exact definite result
`sqrt(pi/2)*(erf(sqrt(2))+1)`, approximately `2.44960215063810845`.
The antiderivative is `sqrt(pi/2)*erf(x/sqrt(2))`. Both exact-result cards
use the same native beautification pass, including rational radical cancellation.
The infinite endpoint has reduced to its exact limit; the plot shades the
visible portion of the integration interval under the Gaussian curve.

[![MARS Lab integrator mode showing the Gaussian improper integral with its simplified endpoint and shaded plot](images/mars-lab/integrator.png?v=20261016-simplified)](images/mars-lab/integrator.png?v=20261016-simplified)

## Datetime mode

Datetime mode combines civil-calendar, jurisdiction, solar, lunar and optional
weather calculations. Enter a selected date, a date range and an observer
location. A Julian day number may be used in place of the selected civil date.
The GMT offset includes daylight saving when applicable.

The captured request uses 16 October 2026 in Shrewsbury, with a date range
from 1 January 2026 to 1 January 2027. Its output includes the weekday,
sunrise and sunset, moonrise and moonset, moon phase, clock changes, the
selected date range and the year's calendar observances. The optional Weather
section is collapsed in this capture.

The ten calendar sections retain datetime's observance calculations, including
estimated festivals and local sunset-start times. Jurisdiction rules continue
to supply local public holidays and time-zone policy. Older cached results that
lack the full calendar sections are bypassed and recomputed; no jurisdiction
database reinstall or deletion of existing cache records is required.

Non-English observance names keep their original labels and show an English name
or explanation on hover. This covers the Ethiopian, Hindu, Buddhist, Muslim,
Mayan and Aztec sections, including local sunset-start rows. Descriptions explain
the observance rather than claiming a literal translation of every proper name.
The native presentation layer supplies and escapes these tooltips for fresh and
cached results alike; copied text, dates and estimation qualifiers are unchanged.
No database migration is needed, and unknown names receive no guessed translation.

[![MARS Lab datetime mode showing Shrewsbury calendar and astronomical results for 16 October 2026](images/mars-lab/datetime.png?v=20261016-full)](images/mars-lab/datetime.png?v=20261016-full)

MARS supplies no shared WeatherAPI account or key. Weather is shown only when
the person installing MARS Lab creates their own WeatherAPI account, configures
that account's key during desktop installation, and the service can be reached.
The installer runs `tools/mars_config/build/release/mars_config weather`, without
starting a listener, browser or object cache. This option can also be run later.
Interactive setup offers an opt-out and hidden, confirmed key entry.
`--noninteractive` suppresses prompts: `--weather-key KEY` takes precedence over
`MARS_WEATHER_API_KEY`, then `WEATHERAPI_KEY`, then the saved configuration.
Prefer the interactive prompt or environment to putting a secret in command-line
history. With no key, non-interactive setup creates nothing.
The key is saved atomically in `$MARS_HOME/config/weather.env` (by default
`~/.mars/config/weather.env`), with directory mode 0700 and file mode 0600.
Final symlinks, non-regular files and multiply-linked destinations are refused.
Keys are limited to 4096 printable ASCII characters excluding apostrophes;
outer whitespace is trimmed. Invalid input leaves existing contents intact.
Setup prints only a confirmation or generic error, never the key. The C installer
regressions are in `tools/mars_config/tests/test_cfg_weather.c`.
The calendar and astronomical results do not depend on that optional service.
They are displayed as soon as the native datetime helper completes; weather is
fetched asynchronously and updates its own card afterwards, without delaying
or replacing those results.

Each lookup sends the configured key, selected date, latitude and longitude
from the local Lab server to WeatherAPI.com over HTTPS. The key is not sent to
the browser. MARS retains only validated current temperatures in a bounded local
cache for up to 48 hours, but reuses them only within one hour of both the current
time and selected moment when a later request fails;
it does not cache complete responses, forecast arrays, pressure readings or API
keys. The date and coordinates remain in private local Lab state so that its
inputs can be restored. The weather card identifies WeatherAPI.com as its
source and displays the required end-user warning. Forecasts and conditions are
probabilistic and may be inaccurate for an exact place or time. Do not use them
as the sole basis for personal safety, aviation, marine navigation, emergency
planning or any other safety-critical decision; consult official
meteorological services and authorities. See the [MARS privacy notice](privacy.md),
[WeatherAPI privacy policy](https://www.weatherapi.com/privacy.aspx) and
[WeatherAPI terms](https://www.weatherapi.com/terms.aspx).

## Almanac mode

Almanac mode is the AstroNav worksheet. Enter a GMT date and time, time-zone
offset, latitude, longitude and altitude. The packaged ephemeris covers
1550–2649 GMT. Results include declination, Greenwich hour angle, right
ascension and observer-relative altitude and azimuth for the navigational
bodies.

The Almanac Help screen explains every body-table coordinate and abbreviation:
declination, GHA, RA, altitude, azimuth, semi-diameter and apparent visual magnitude.
It gives angle units and reference directions, distinguishes RA hours from clock
time and angular altitude from observer elevation, and explains the brightness scale.
The event glossary distinguishes diameter-based eclipse magnitude from area-based
obscuration, including negative lunar umbral magnitudes and penumbral eclipses with
zero umbral obscuration. It explains First, Greatest and Fourth, the local/GMT time
conventions, and the fact that lunar First/Fourth are P1/P4 rather than totality contacts.
Solar and lunar partial/total phases, annular and hybrid eclipses, the umbra and
penumbra, and date-only planetary transits are described separately, with links to
USNO and NASA references.

Observer elevation (−500 to 11000 metres above sea level) is passed to the native
topocentric calculation, including the Moon's parallax; it is not replaced by sea
level. Body-table altitudes include an approximate refraction correction. The
existing optional WeatherAPI configuration supplies temperature and pressure for
the observation hour, matched by UTC timestamp rather than provider-local time.
Current observations require the same UTC hour and a separation of at most thirty
minutes from the selected moment. Otherwise valid hourly temperatures bracketing
the selected GMT timestamp are linearly interpolated, including across day arrays.
Gaps exceeding one hour are not interpolated. A lone value for the containing hour
is labelled as an hourly value, not a live measurement; extrapolation is not used.
Daily minimum and maximum values remain in the daily weather card: without the
times of those extremes they do not define a trustworthy daily temperature curve.
Cached observations must be at most one hour old and within one hour of the
selected moment, matching coordinates rounded to three decimal places. Their
source label includes their age and separation from the selected time. Estimates
never renew cache age. Only temperature is reused; pressure retains its independent fallback.
The cache uses at most 64 small files under `MARS_HOME/lab/weather-cache` (normally
`~/.mars/lab/weather-cache`), contains no credentials, and uses non-blocking locks
between workers. Unavailable or corrupt entries simply fall back to the seasonal
model. Without a recent reading, a deliberately rough latitude/season estimate is reduced by
6.5 °C per kilometre of elevation. Its northern seasonal maximum is in July,
reversed in the southern hemisphere. This has no time-of-day information: it is
labelled **Rough seasonal assumption** and displayed as an approximate whole-degree
value, not a local weather prediction. Missing pressure is estimated from elevation
using a standard atmosphere; available provider pressure is not adjusted a second time.
[WeatherAPI describes its pressure field as station pressure](https://blog.weatherapi.com/density-altitude-real-time-calculation-weatherapi/).
The provider's observation site may differ from the observer's exact location or
elevation. The summary card shows temperature and atmospheric pressure on their
own line, identifying whether each input comes from weather or an estimate. A
separate line explains the altitude corrections; both lines are included in
copied text for the All and Visible body lists. Missing or invalid temperature
and pressure fall back independently.
The initial table uses estimated conditions and never waits for weather. A separate
background request uses the existing four-second provider deadline and refreshes
the native altitudes when it completes. Failure leaves the estimated table usable;
responses belonging to superseded evaluations or another mode cannot replace it.
The current All/Visible filter is preserved during weather refresh. No weather API key is required.
The summary uses nine separate cells in three columns (two on phones): GMT moment,
location, zone, latitude, longitude, observer altitude, temperature, pressure and
weather source. Coordinates display degrees and decimal minutes to two decimal
places with compass directions; the calculation retains the original precision.
Corrections appear below a subtle divider; copying retains the plain-text summary.
Editing observer altitude still switches a mismatched named town to Custom.
Evaluate and background weather refresh retain that entered altitude; returned
calculation fields cannot overwrite it. Selecting a town or an explicit location
action can still supply a new observer altitude.

Temperature changes refraction, not parallax. Below −0.575° the displayed altitude
receives no refraction correction. The native `shoreline_t` hit-test classifies the
observer's latitude and longitude using spatially indexed GSHHG data in the
almanac database. Town and Custom selections use the same geographical rule.
Confirmed sea uses sea mode: the entered altitude is height of eye above sea level
(non-negative), and geometric horizon dip is added to the predicted body-centre
altitude above the assumed unobstructed sea horizon. Land, inland water, the
500-metre uncertain coastal band and unavailable data do not receive sea-horizon
dip. The summary identifies the classification and
corrections. This is not a sextant sight reduction, for which dip is subtracted
from a measured altitude. Waves and terrestrial refraction are not modelled.
Thus the worksheet corrects predicted sighting altitudes for the observer's
topocentric position and refraction everywhere, and adds sea-horizon dip only
where the shoreline data confirms open sea. It deliberately withholds that dip
near coasts and on land: an entered elevation is not evidence of an unobstructed
sea horizon.
Eclipse contact searches and rise/set calculations retain their existing
geometric/standard-atmosphere conventions, rather than applying today's weather
to future events. The estimates are not intended for precision sight reduction.

Wide worksheet tables scroll within their own keyboard-focusable areas rather
than overflowing the result card. Event contacts show compact times; their full
dates and GMT offsets remain in tooltips and copied worksheet text. On mobile,
events use stacked, labelled fields instead of tightly squeezed columns. At phone
widths (up to 560 pixels), each navigational body has a compact heading with its
symbol, name and visibility indicator, followed by all seven measurements in two
labelled columns. No measurement is dropped or rounded by the browser. The native
table retains its accessible row/column roles and clipboard text; desktop and
tablet layouts continue to use a horizontally scrollable table. Both All and
Visible filters use the same responsive presentation.

In both **All** and **Visible** views, the Sun and planets carry their conventional
astronomical symbols beside their names: ☉ Sun, ☿ Mercury, ♀ Venus, ♂ Mars,
♃ Jupiter, ♄ Saturn, ⛢ Uranus and ♆ Neptune.
Stars have warm pale-gold SVG star shapes sized by apparent visual magnitude: brighter
(lower-magnitude) stars are larger. The native renderer uses the unrounded
magnitude, mapping the radius to `8 − magnitude` SVG units, limited to 3–10
within a fixed-width slot so names remain aligned. This is a schematic scale,
not stellar angular diameter or proportional light flux. Missing or invalid
magnitudes use a neutral radius of 6 and an explicit unavailable tooltip.
Tooltips and accessible labels identify the magnitude used.
All body symbols are native-generated inline SVG geometry, not bitmap images
or font glyphs, so they remain crisp at any zoom. The symbols use gold for the
Sun, purple for Mercury, yellow for Venus, red
for Mars, coral for Jupiter, amber for Saturn, cyan for Uranus and violet for
Neptune; the Moon uses muted blue-grey illumination with a dark grey shadow and
grey outline. Uranus uses the circle-and-upward-arrow symbol.
These inline icons retain their own colours and compact spacing inside the result
card; formula-only SVG recolouring, scaling and bottom margins do not apply to them.
The Moon has an in-memory SVG phase disc calculated from the selected instant's
native phase angle. Its bright limb points towards the Sun, rotated for the
selected latitude, longitude and time with the local zenith upwards. This varies
continuously across both hemispheres and the equator, including tilted crescents;
it is not a fixed northern/southern flip. Its tooltip and accessible label give
the illuminated percentage and orientation. This is a schematic disc, without
lunar terrain, topocentric parallax or atmospheric refraction. At the zenith or
nadir, where horizon-relative orientation is undefined, or if observer data is
missing, it explicitly falls back to celestial north up (east to the left).
The Sun's coordinates remain available for this diagram even when the Sun is
excluded by the visibility filter. Missing phase or coordinate data falls back
to ☾, not a guessed phase. Copied worksheet text retains the ordinary body names.

Solar and lunar eclipse names also have inline SVG previews at the event's
**greatest eclipse** time. Native angular radii and separation determine the
overlap: solar previews distinguish partial, annular and total coverage; lunar
previews show the penumbra in grey and umbra in muted reddish brown. The solar
disc uses the same gold as the Sun's body symbol. Colours
and sharp shadow edges are schematic, not a prediction of atmospheric colour,
brightness or lunar surface detail. Magnitude is not substituted for area coverage.
Solar obscuration measures the covered area of the Sun, not its covered diameter.
Lunar obscuration is explicitly labelled **umbral**: a penumbral eclipse can have
zero umbral coverage and negative umbral magnitude while still showing grey
penumbral shading in its preview. Cell tooltips explain both measurements.
The named-totality search uses `jurisdict_each_town()` and the jurisdiction
module's installed configuration, including database-backed town coordinates and
timezones. Catalogue failures are reported as failures, not as an absence of
totality. A single catalogue pass screens every town against shared, two-minute
Sun/Moon samples over the same twelve-hour window used by local refinement.
An initial 0.1-degree apparent-separation screen is followed by a chord-interpolated
check against the apparent radius difference, with a 0.001-degree allowance.
Only possible totality locations are retained and distance-sorted; there is no first-1,024-towns cut-off. Surviving
candidates undergo the exact local-maximum calculation and a shoreline land
check before becoming actionable. The approximately 17 KiB screening grid is
released after each event; rejected towns are not retained. This remains a
sampled named-location search, not a guarantee of the nearest point on all land.
The existing 20-second worker timeout and asynchronous request ownership remain.
The preview is rotated for the selected observer at the event time, with the
local zenith upwards, including hemisphere, longitude and time-dependent tilt.
Solar geometry is topocentric; lunar shadow sizes use the engine's geocentric
convention, with the shadow direction projected into the observer's local view.
Undefined orientations are explicitly labelled; missing geometry omits the
preview rather than inventing a shape. Changing to a nearest-totality location
requires a new evaluation, just as the contact times do.
The nearest-totality **Use** action supplies the destination's greatest-eclipse
date and time in GMT, matching the worksheet inputs. Its label and event contact
times remain local civil time, and the destination's zone remains selected for
display. A non-zero zone must not shift the observation away from totality.

The captured request is for London at `2026-10-16 12:00:00` GMT, at an observer
elevation of 25 metres. The globe and observer controls appear beside the
navigational-body table, including bodies below the horizon. The summary identifies
London as land and shows the weather inputs used for refraction; sea-horizon dip
is therefore excluded. Weather values reflect the captured response and may vary
when the request is repeated.

[![Full-page MARS Lab Almanac showing the London observer globe, land classification, weather inputs, corrected body altitudes and upcoming events](images/mars-lab/almanac.png?v=20261019-full-page)](images/mars-lab/almanac.png?v=20261019-full-page)

## Mobile and private remote access

The access card is available on desktop and hidden in the mobile layout (viewports
up to 900 pixels wide), including its URL, QR code and copy button.

When MARS Lab listens on a wildcard address (as the desktop launcher does), its **Mobile** control
shows the best private access route currently available:

- with Tailscale active, the QR code contains the private Tailscale URL;
- otherwise, it contains a local Wi-Fi URL when one is available;
- when neither route is reachable, no mobile URL is advertised.

For access away from the local network, connect both the MARS computer and the
mobile device to the same Tailscale network, start MARS Lab, open **Mobile** and
scan the displayed code. MARS Lab recognises an existing private Tailscale Serve
configuration; it does not change Tailscale settings or enable public Funnel access. A phone
that is not connected to the same Tailscale network cannot use the private QR
code.

## Troubleshooting

- **No rendered mathematics:** install `texlive-latex-base` and `dvisvgm`, then
  restart the Lab. The renderer accepts native mathematical spacing, including
  `\mkern`, but rejects file access and macro-definition commands. If a native
  result reports an unsupported command, rebuild and restart the Lab to ensure
  its renderer matches the library's current output.
- **A helper is missing:** run `make release` and restart the Lab from the
  repository root.
- **A mobile QR code is absent or unreachable:** confirm that MARS Lab is using
  the wildcard host, then check that the phone is on the same Wi-Fi network or
  Tailscale network as the computer.
- **A result is too wide:** use the result card's zoom controls. Long rendered
  mathematics is broken over lines where possible and continues vertically.
  Lines that still exceed the card width have a horizontal scrollbar, including
  in expanded cards.
- **A previous result is still visible:** evaluate the new input or use
  **Clear**. **Back** and **Forward** navigate the Lab's own workspace history.
