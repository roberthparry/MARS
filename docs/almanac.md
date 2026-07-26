# Almanac

The almanac layer opens the configured ephemeris database, computes GHA Aries,
and resolves SHA and declination for catalogued bodies such as `SUN`, `MOON`,
`MARS`, `JUPITER`, or named stars such as `SIRIUS`.

It is the higher-precision ephemeris layer in MARS. Where `datetime_t` provides
calendar and light astronomical helpers such as sunrise, sunset, and moon
phase, `almanac_t` provides catalogue-backed apparent place data for navigation
and observational work.

Event-search algorithms in this module draw on the *Explanatory Supplement to
the Astronomical Almanac* (ESAA). Subsequent references in this document use
ESAA.

## Capabilities

- open the configured encrypted ephemeris database
- compute Greenwich Hour Angle of Aries for a civil moment
- resolve SHA, declination, and right ascension for one catalogued body
- return snapshot rows for all enabled bodies in configured catalogue order
- derive observer-relative altitude, azimuth, semi-diameter, and horizon visibility
- derive illuminated fraction and broad phase class from planetary and lunar phase angle
- find the next exact new Moon, first quarter, full Moon, or last quarter
- search civil time windows for solar eclipses, lunar eclipses, and Mercury/Venus solar transits
- report geocentric distance, heliocentric distance, phase angle, and visual magnitude where they apply
- expose engine errors through `almanac_last_error()`
- serialise the configured engine for storage through `sqlite_t`

## Configuration

`almanac_open()` resolves the backing store from the current configuration.

It prefers:

- `MARS_ALMANAC_DB_PATH`
- `MARS_ALMANAC_DB_KEY`

If those are not set in the environment, it also checks the standard MARS
configuration file:

- `~/.mars/config/almanac-db.env`

The default database location is:

- `~/.mars/almanac/almanac.db`

Opening fails when the engine cannot find both a usable database path and the
decryption key. In that case, inspect `almanac_last_error()` for the recorded
failure reason.

## Ownership

`almanac_open()` returns a heap-allocated engine that the caller must release
with `almanac_close()`.

`almanac_entry_t` is opaque. Create standalone body entries with
`almanac_new_body_entry()` or `almanac_new_entry()` and release them with
`almanac_entry_dealloc()`.

`almanac_snapshot()` returns a newly allocated `array_t` containing
`almanac_entry_t` values. Entries obtained with `array_get()` are borrowed from
the array. Destroy the array with `array_destroy()` when you are finished with
it.

`almanac_last_error()` returns a borrowed pointer owned by the engine. It
remains valid until the next almanac API call on the same engine or until
`almanac_close()`.

## Body Kinds

`almanac_entry_body_kind()` returns an `almanac_body_kind_t`:

| Value | Meaning |
|---|---|
| `ALMANAC_BODY_STAR` | Fixed star from the catalogue |
| `ALMANAC_BODY_PLANET` | Planetary body with modelled apparent place |
| `ALMANAC_BODY_SUN` | The Sun |
| `ALMANAC_BODY_MOON` | The Moon |

## Entry Accessors

Use the accessors below to inspect an `almanac_entry_t`:

- `almanac_entry_body_id()` — stable enum identifier for the resolved body
- `almanac_entry_body_kind()` — one of the `almanac_body_kind_t` values above
- `almanac_entry_moment_jd()` — Julian date for the requested civil moment
- `almanac_entry_gha_aries_degrees()` — apparent GHA of Aries for the requested civil moment
- `almanac_entry_sha_degrees()` — sidereal hour angle of the body, normalised to `[0, 360)`
- `almanac_entry_declination_degrees()` — declination in degrees
- `almanac_entry_right_ascension_hours()` — apparent right ascension in hours
- `almanac_entry_geocentric_distance_au()` — distance from Earth in astronomical units when applicable
- `almanac_entry_heliocentric_distance_au()` — distance from the Sun in astronomical units when applicable
- `almanac_entry_phase_angle_degrees()` — illuminated-geometry phase angle when applicable
- `almanac_entry_visual_magnitude()` — apparent visual magnitude when available

Some fields are not meaningful for every body. Fixed stars, for example, do not
report a finite geocentric distance, and some brightness-related values may be
`NaN` when they do not apply.

## Main Entry Points

- `almanac_gha_aries()` to compute Greenwich Hour Angle of Aries for a moment
- `almanac_new_body_entry()` to resolve one catalogued body enum into an `almanac_entry_t`
- `almanac_observables()` to derive observer-relative horizon observables from one entry
- `almanac_body_geographical_position()` to derive the Earth position where one body is directly overhead
- `almanac_body_rise_set()` to find almanac-based local rise and set for one body, observer, and jurisdiction
- `almanac_sunrise_sunset()` to find almanac-based local sunrise and sunset for an observer and jurisdiction
- `almanac_moonrise_moonset()` to find almanac-based local moonrise and moonset for an observer and jurisdiction
- `almanac_phase_details()` to derive illuminated fraction and a broad phase class
- `almanac_next_moon_phase_exact()` to find the next exact lunar phase event
- `almanac_find_solar_eclipses()` to search a window for solar eclipses visible from an observer
- `almanac_find_lunar_eclipses()` to search a window for lunar eclipses visible from an observer
- `almanac_find_solar_transits_for_body()` to search a window for Mercury or Venus transits visible from an observer
- `almanac_snapshot()` to return all enabled bodies in catalogue order
- `almanac_last_error()` to inspect the most recent engine error message

## Observer And Observables

`almanac_observer_t` describes one observer location:

- `latitude_degrees` — observer latitude in degrees in the range `[-90, 90]`
- `longitude_degrees` — observer longitude in degrees; east-positive values are added to GHA to form local hour angle
- `elevation_metres` — observer elevation above sea level in metres; used for topocentric observer position and almanac sunrise/sunset horizon dip

`almanac_observables_t` returns one set of horizon-style values:

- `altitude_degrees` — apparent altitude of the body's centre in degrees
- `azimuth_degrees` — azimuth in degrees in the range `[0, 360)`
- `semi_diameter_degrees` — angular semi-diameter in degrees when MARS has a radius model for that body, otherwise `NaN`
- `above_horizon` — `true` when the body's centre is above the geometric horizon
- `visible` — `true` when the body's centre or upper limb is above the horizon in this first implementation

This first `almanac_observables()` implementation derives observables from the
apparent geocentric place already stored in `almanac_entry_t`. It does not yet
apply topocentric parallax, atmospheric refraction, or twilight policy rules.
That means `visible` is a practical horizon flag, not a full naked-eye
visibility model.

`almanac_body_rise_set()` computes rise/set times for one local civil day using
the almanac ephemeris and observer topocentric geometry. It uses the supplied
jurisdiction to convert each absolute event instant into local civil time,
including daylight-saving rules. The horizon crossing is the standard apparent
upper-limb event: geometric centre altitude plus body semi-diameter, standard
refraction, and elevation horizon dip. For the Moon, topocentric parallax is
already included in the observer-relative vector.

`almanac_rise_set_t` contains two `almanac_rise_set_event_t` values:

- `rise` — rise status, local civil event time, absolute event JD, and azimuth
- `set` — set status, local civil event time, absolute event JD, and azimuth

Rise/set events use `ALMANAC_RISE_SET_OK`,
`ALMANAC_RISE_SET_NOT_ON_DATE`, `ALMANAC_RISE_SET_NEVER_RISES`,
`ALMANAC_RISE_SET_NEVER_SETS`, or `ALMANAC_RISE_SET_UNAVAILABLE` as their
status. When status is `ALMANAC_RISE_SET_OK`, `time` is valid, `time.jd` is
the absolute event Julian date, and `time.local_jd` is the jurisdiction-local
civil Julian date. Use `almanac_event_time_datetime()` to initialise a
`datetime_t` for display.

## Phases And Event Searches

`almanac_phase_details_t` derives one phase snapshot from an `almanac_entry_t`:

- `phase_angle_degrees` — copied from the body entry
- `illuminated_fraction` — illuminated fraction in the range `[0, 1]` when available
- `phase_class` — broad classification such as `NEW`, `CRESCENT`, `QUARTER`, `GIBBOUS`, or `FULL`

`almanac_next_moon_phase_exact()` searches for the next exact:

- `ALMANAC_MOON_PHASE_NEW`
- `ALMANAC_MOON_PHASE_FIRST_QUARTER`
- `ALMANAC_MOON_PHASE_FULL`
- `ALMANAC_MOON_PHASE_LAST_QUARTER`

The returned `almanac_moon_phase_event_t` stores its event time in
`time`. Exact Moon phases are not observer-local, so `time.jd` and
`time.local_jd` are the same.

`almanac_find_solar_eclipses()`, `almanac_find_lunar_eclipses()`, and
`almanac_find_solar_transits_for_body()` each return an `array_t` of plain value
records. These searches require an `almanac_observer_t`; the returned arrays
only include events that occur locally for that observer, with the Sun above
the horizon for solar eclipses and solar transits, or the Moon above the
horizon for lunar eclipses. Their event times are recorded as
`almanac_event_time_t` values:

- `greatest_eclipse` for eclipse records
- `greatest_transit` for solar transit records
- contact fields with an absolute Julian date and a local civil Julian date
- `totality_percent` for eclipse records, as an apparent-disc area percentage
- `first_contact` through `fourth_contact` for solar eclipse and solar transit records
- `p1_contact`, `u1_contact`, `u2_contact`, `u3_contact`, `u4_contact`, and `p4_contact` for lunar eclipse records

Use `almanac_event_time_datetime()` when you want to present an
`almanac_event_time_t` as a local civil `datetime_t`.

The event searches use the short periodic estimates described in ESAA to select
candidate lunations or inferior conjunctions, reject eclipse candidates far
from a lunar node, and then refine the surviving candidates against the
packaged almanac ephemeris:

- exact Moon phases start from a mean lunation estimate and are refined from the almanac geometry
- solar eclipses are detected with observer-local topocentric geometry
- solar eclipse `totality_percent` is the approximate percentage of the Sun's apparent disc covered by the Moon at greatest eclipse
- lunar eclipse contacts, greatest eclipse, and magnitudes use ESAA geocentric shadow-cone geometry; the observer is used only to decide local visibility
- lunar eclipse `totality_percent` is the approximate percentage of the Moon's apparent disc inside Earth's umbra
- solar transits use the Mercury or Venus synodic cycle to estimate inferior conjunctions before ephemeris refinement
- contact times use bracketed inverse interpolation and greatest-event times use parabolic interpolation of squared separation
- solar transits currently support `ALMANAC_BODY_ID_MERCURY` and `ALMANAC_BODY_ID_VENUS`
- Besselian elements, atmospheric refraction, and full central-line path
  modelling are not yet exposed

## Quick Example

Use `almanac_new_body_entry(almanac, ALMANAC_BODY_ID_SUN, ...)` or
`almanac_new_body_entry(almanac, ALMANAC_BODY_ID_MOON, ...)` when you need a
single body entry. For solar eclipses, prefer the dedicated local event search:
`almanac_find_solar_eclipses()`.

```c
#include <stdio.h>

#include "almanac.h"
#include "array.h"
#include "datetime.h"

static datetime_t *datetime_from_event_time(const almanac_event_time_t *event_time)
{
    datetime_t *dttm = datetime_alloc();

    if (!dttm)
        return NULL;
    if (!almanac_event_time_datetime(event_time, dttm)) {
        datetime_dealloc(dttm);
        return NULL;
    }
    return dttm;
}

int main(void)
{
    datetime_t *start = NULL;
    datetime_t *end = NULL;
    almanac_t *almanac = NULL;
    array_t *events = NULL;
    const almanac_solar_eclipse_t *event;
    almanac_event_time_t greatest;
    almanac_event_time_t first;
    almanac_event_time_t fourth;
    datetime_t *greatest_time = NULL;
    datetime_t *first_time = NULL;
    datetime_t *fourth_time = NULL;
    almanac_solar_eclipse_kind_t event_kind;
    almanac_observer_t observer = {52.7073, -2.7540, 75.0};
    const char *kind;
    int status = 1;

    start = datetime_alloc();
    end = datetime_alloc();
    if (!start || !end)
        goto done;
    if (!datetime_init_ymdt(start, 2026, DT_August, 1, 0, 0, 0.0))
        goto done;
    if (!datetime_init_ymdt(end, 2026, DT_August, 20, 0, 0, 0.0))
        goto done;

    almanac = almanac_open();
    if (!almanac)
        goto done;
    events = almanac_find_solar_eclipses(almanac, &observer, start, end);
    if (!events)
        goto done;

    printf("Shrewsbury eclipse watch\n");
    if (array_size(events) == 0u) {
        printf("No local solar eclipse found in August 2026.\n");
        status = 0;
        goto done;
    }

    event = array_get(events, 0u);
    if (!event)
        goto done;
    if (!almanac_solar_eclipse_time(event, ALMANAC_EVENT_TIME_GREATEST, &greatest))
        goto done;
    if (!almanac_solar_eclipse_time(event, ALMANAC_EVENT_TIME_FIRST_CONTACT, &first))
        goto done;
    if (!almanac_solar_eclipse_time(event, ALMANAC_EVENT_TIME_FOURTH_CONTACT, &fourth))
        goto done;
    greatest_time = datetime_from_event_time(&greatest);
    first_time = datetime_from_event_time(&first);
    fourth_time = datetime_from_event_time(&fourth);
    if (!greatest_time || !first_time || !fourth_time)
        goto done;
    event_kind = almanac_solar_eclipse_kind(event);
    kind = event_kind == ALMANAC_SOLAR_ECLIPSE_TOTAL ? "total" :
           event_kind == ALMANAC_SOLAR_ECLIPSE_ANNULAR ? "annular" :
           "partial";
    printf("Local solar eclipse found in August 2026.\n");
    printf("Greatest local circumstance: %04d-%02d-%02d %02u:%02u:%04.1f local time\n",
           datetime_year(greatest_time),
           (int)datetime_month(greatest_time),
           datetime_day(greatest_time),
           datetime_hour(greatest_time),
           datetime_minute(greatest_time),
           datetime_second(greatest_time));
    printf("Kind: %s\n", kind);
    printf("Magnitude: %.3f\n", almanac_solar_eclipse_magnitude(event));
    printf("Obscuration: %.1f%%\n", almanac_solar_eclipse_totality_percent(event));
    printf("First contact: %04d-%02d-%02d %02u:%02u:%04.1f local time\n",
           datetime_year(first_time),
           (int)datetime_month(first_time),
           datetime_day(first_time),
           datetime_hour(first_time),
           datetime_minute(first_time),
           datetime_second(first_time));
    printf("Fourth contact: %04d-%02d-%02d %02u:%02u:%04.1f local time\n",
           datetime_year(fourth_time),
           (int)datetime_month(fourth_time),
           datetime_day(fourth_time),
           datetime_hour(fourth_time),
           datetime_minute(fourth_time),
           datetime_second(fourth_time));

    status = 0;

done:
    array_destroy(events);
    almanac_close(almanac);
    datetime_dealloc(fourth_time);
    datetime_dealloc(first_time);
    datetime_dealloc(greatest_time);
    datetime_dealloc(start);
    datetime_dealloc(end);
    return status;
}
```

For a Shrewsbury eclipse-watching example, the test suite asks
`almanac_find_solar_eclipses()` for local events in August 2026 and prints the
first local result returned by the almanac.

Example output from `example_almanac_shrewsbury_eclipse_watch`:

```text
Shrewsbury eclipse watch
Local solar eclipse found in August 2026.
Greatest local circumstance: 2026-08-12 18:11:30.5 local time
Kind: partial
Magnitude: 0.931
Obscuration: 92.2%
First contact: 2026-08-12 17:14:43.5 local time
Fourth contact: 2026-08-12 19:05:13.7 local time
```

## Oracle Output

The oracle regression prints `expected`, `got`, the rounded arcsecond error,
and a navigation-grade decision for each axis. A case counts as navigation
grade `PASS` when the rounded SHA and declination errors are both `<= 6`
arcseconds; otherwise it is a `FAIL`.

## Serialisation And SQLite Storage

`almanac_t` can be stored through the `sqlite_t` object-store API with
`almanac_serialize()` and `almanac_deserialise()`.

This serialisation does not dump live database handles or cached model state.
Instead, it records that the engine should be reopened from the current
configuration when it is loaded again. In practice, that means deserialisation
still depends on the same configured almanac database and key being available.

The serialised form uses:

- type: `almanac_t`
- encoding: `mars/configured-engine-v1`

## API Reference

All declarations are in `include/almanac.h`.

### Allocation

**`almanac_t *almanac_open(void)`**

Open the configured almanac engine.

Returns a newly allocated engine on success, or `NULL` if the database or key
cannot be resolved or the backing store cannot be opened.

**`void almanac_close(almanac_t *almanac)`**

Destroy an almanac engine.

- `almanac` — engine to destroy, or `NULL`

### Error Reporting

**`const char *almanac_last_error(const almanac_t *almanac)`**

Return the last recorded error message for an engine.

- `almanac` — engine to query

Returns a borrowed string pointer, or `NULL` when no message is available. The
returned pointer stays valid until the next almanac API call on the same engine
or until `almanac_close()`.

### Position Queries

**`bool almanac_gha_aries(almanac_t *almanac, const datetime_t *moment, double *gha_aries_degrees)`**

Compute the apparent Greenwich Hour Angle of Aries for a civil moment.

- `almanac` — open almanac engine
- `moment` — moment to evaluate
- `gha_aries_degrees` — output pointer that receives GHA Aries in degrees in the range `[0, 360)`

Returns `true` on success. Returns `false` on failure and records an engine
error.

**`almanac_entry_t *almanac_new_body_entry(almanac_t *almanac, almanac_body_id_t body_id, const datetime_t *moment)`**

Create one populated almanac entry for a catalogue body enum at a given moment.

- `almanac` — open almanac engine
- `body_id` — stable body enum such as `ALMANAC_BODY_ID_SUN`, `ALMANAC_BODY_ID_MOON`, `ALMANAC_BODY_ID_MARS`, `ALMANAC_BODY_ID_JUPITER`, or `ALMANAC_BODY_ID_SIRIUS`
- `moment` — civil moment to evaluate

Returns a newly allocated entry on success. The caller owns it and must release
it with `almanac_entry_dealloc()`. Returns `NULL` if the body id is unsupported,
the moment cannot be evaluated, or the engine encounters a database or model
error.

On success, the entry contains the resolved body metadata, GHA Aries, SHA,
declination, right ascension, and any applicable distance or brightness fields.

**`almanac_entry_t *almanac_new_entry(almanac_t *almanac, const char *body_code, const datetime_t *moment)`**

Compatibility constructor for legacy body-code callers.

- `almanac` — open almanac engine
- `body_code` — catalogue code such as `SUN`, `MOON`, `MARS`, `JUPITER`, or `SIRIUS`
- `moment` — civil moment to evaluate

New code should prefer `almanac_new_body_entry()` so runtime lookups stay on
the stable `almanac_body_id_t` enum path.

**`void almanac_entry_dealloc(almanac_entry_t *entry)`**

Release an entry allocated by an almanac entry constructor. Passing `NULL` is
safe.

**`bool almanac_observables(almanac_t *almanac, const almanac_entry_t *body, const almanac_observer_t *observer, almanac_observables_t *out)`**

Derive observer-relative observables from one computed almanac entry.

- `almanac` — open almanac engine, used for error reporting
- `body` — previously computed almanac entry for the target body
- `observer` — observer location and elevation
- `out` — caller-provided observables structure to fill on success

On success, `out` receives altitude, azimuth, semi-diameter where available,
and two horizon flags.

Returns `true` on success. Returns `false` if any pointer is invalid or the
observer latitude/longitude is out of range.

**`bool almanac_body_geographical_position(const almanac_entry_t *body, almanac_geographical_position_t *out)`**

Derive the geographical position, or GP, for one computed body entry. This is
the point on Earth where the body is directly overhead.

- `body` — previously computed almanac entry for the target body
- `out` — caller-provided geographical position structure to fill on success

On success, `out->latitude_degrees` is the body's declination and
`out->longitude_degrees` is the body longitude in signed east-positive degrees.

Returns `true` on success. Returns `false` if any pointer is invalid or the
entry does not contain finite GHA/declination values.

**`bool almanac_body_rise_set(almanac_t *almanac, jurisdiction_t *jurisdiction, almanac_body_id_t body_id, const datetime_t *date, const almanac_observer_t *observer, almanac_rise_set_t *out)`**

Find accurate local rise and set times for one supported body and civil day.

- `almanac` — open almanac engine
- `jurisdiction` — open jurisdiction engine used for local GMT/DST offset
- `body_id` — body enum identifier
- `date` — local civil date to evaluate; the time component is ignored
- `observer` — latitude, longitude, and elevation
- `out` — caller-provided result structure to fill

Returns `true` when the day was evaluated. Each result carries its own status,
so a body that does not rise or set on that date is reported without treating
the call as an error.

**`bool almanac_sunrise_sunset(almanac_t *almanac, jurisdiction_t *jurisdiction, const datetime_t *date, const almanac_observer_t *observer, almanac_sun_times_t *out)`**

Convenience wrapper for `almanac_body_rise_set()` with
`ALMANAC_BODY_ID_SUN`.

- `almanac` — open almanac engine
- `jurisdiction` — open jurisdiction engine used for local GMT/DST offset
- `date` — local civil date to evaluate; the time component is ignored
- `observer` — latitude, longitude, and elevation
- `out` — caller-provided result structure to fill

When an event occurs, `time` contains the jurisdiction-local civil date and
time, and `time.jd` is the absolute event Julian date.
The returned `almanac_sun_times_t` uses `rise` for sunrise and `set` for
sunset.

**`bool almanac_moonrise_moonset(almanac_t *almanac, jurisdiction_t *jurisdiction, const datetime_t *date, const almanac_observer_t *observer, almanac_moon_times_t *out)`**

Convenience wrapper for `almanac_body_rise_set()` with
`ALMANAC_BODY_ID_MOON`.

- `almanac` — open almanac engine
- `jurisdiction` — open jurisdiction engine used for local GMT/DST offset
- `date` — local civil date to evaluate; the time component is ignored
- `observer` — latitude, longitude, and elevation
- `out` — caller-provided result structure to fill

When an event occurs, `time` contains the jurisdiction-local civil date and
time, and `time.jd` is the absolute event Julian date.
The returned `almanac_moon_times_t` uses `rise` for moonrise and `set` for
moonset.

**`bool almanac_phase_details(const almanac_entry_t *body, almanac_phase_details_t *out)`**

Derive illuminated fraction and broad phase class from a body entry.

- `body` — previously computed almanac entry for the target body
- `out` — caller-provided phase-details structure to fill

Returns `true` on success. Returns `false` if either pointer is invalid.

**`bool almanac_next_moon_phase_exact(almanac_t *almanac, const datetime_t *after, almanac_moon_phase_kind_t kind, almanac_moon_phase_event_t *out)`**

Find the next exact Moon phase after a starting moment.

- `almanac` — open almanac engine
- `after` — starting moment after which to search
- `kind` — exact lunar phase kind to search for
- `out` — caller-provided event structure to fill

Returns `true` on success. On success, `out->time` stores the event time. Exact
Moon phases are not observer-local, so `out->time.jd` and
`out->time.local_jd` are the same.

**`array_t *almanac_find_solar_eclipses(almanac_t *almanac, const almanac_observer_t *observer, const datetime_t *start, const datetime_t *end)`**

Search a civil time window for solar eclipses visible from an observer.

- `almanac` — open almanac engine
- `observer` — observer latitude, longitude, and elevation for local circumstances
- `start` — inclusive window start
- `end` — inclusive window end

Returns a newly allocated `array_t` of opaque `almanac_solar_eclipse_t` records
on success, or `NULL` on failure. Destroy the returned array with
`array_destroy()` when finished. Only locally visible eclipses are included.
Use `almanac_solar_eclipse_kind()`, `almanac_solar_eclipse_time()`,
`almanac_solar_eclipse_magnitude()`, and
`almanac_solar_eclipse_totality_percent()` to inspect each record. Contact
times use accepted eclipse terminology: first contact, greatest eclipse, and
fourth contact are populated for detected eclipses; second and third contact
are populated for total or annular eclipses.

**`array_t *almanac_find_lunar_eclipses(almanac_t *almanac, const almanac_observer_t *observer, const datetime_t *start, const datetime_t *end)`**

Search a civil time window for lunar eclipses visible from an observer.

- `almanac` — open almanac engine
- `observer` — observer latitude, longitude, and elevation for local visibility
- `start` — inclusive window start
- `end` — inclusive window end

Returns a newly allocated `array_t` of opaque `almanac_lunar_eclipse_t` records
on success, or `NULL` on failure. Destroy the returned array with
`array_destroy()` when finished. Only locally visible eclipses are included.
The event circumstances are geocentric and therefore do not change with the
observer; the observer's topocentric Moon altitude controls whether the event
is included.
Use `almanac_lunar_eclipse_kind()`, `almanac_lunar_eclipse_time()`,
`almanac_lunar_eclipse_umbral_magnitude()`,
`almanac_lunar_eclipse_penumbral_magnitude()`, and
`almanac_lunar_eclipse_totality_percent()` to inspect each record. Contact
times use the standard lunar eclipse labels: `P1` and `P4` for penumbral
contacts, `U1` and `U4` for umbral partial contacts, and `U2` and `U3` for
totality contacts.

**`array_t *almanac_find_solar_transits_for_body(almanac_t *almanac, almanac_body_id_t body_id, const almanac_observer_t *observer, const datetime_t *start, const datetime_t *end)`**

Search a civil time window for Mercury or Venus transits of the Sun visible
from an observer.

- `almanac` — open almanac engine
- `body_id` — `ALMANAC_BODY_ID_MERCURY` or `ALMANAC_BODY_ID_VENUS`
- `observer` — observer latitude, longitude, and elevation for local circumstances
- `start` — inclusive window start
- `end` — inclusive window end

Returns a newly allocated `array_t` of opaque `almanac_solar_transit_t` records
on success, or `NULL` on failure. Destroy the returned array with
`array_destroy()` when finished. Only locally visible transits are included.
Use `almanac_solar_transit_body_id()`, `almanac_solar_transit_time()`, and the
solar-transit geometry accessors to inspect each record. Contact times use the
accepted transit terminology: first and fourth contact are the external ingress
and egress contacts, while second and third contact are the internal ingress
and egress contacts.

**`array_t *almanac_find_solar_transits(almanac_t *almanac, const char *body_code, const almanac_observer_t *observer, const datetime_t *start, const datetime_t *end)`**

Compatibility wrapper that searches for locally visible Mercury or Venus
transits of the Sun by legacy body code.

- `almanac` — open almanac engine
- `body_code` — currently `MERCURY` or `VENUS`
- `observer` — observer latitude, longitude, and elevation for local circumstances
- `start` — inclusive window start
- `end` — inclusive window end

Returns the same result shape as `almanac_find_solar_transits_for_body()`.

**`array_t *almanac_snapshot(almanac_t *almanac, const datetime_t *moment)`**

Compute almanac entries for every enabled catalogue body.

- `almanac` — open almanac engine
- `moment` — civil moment to evaluate

Returns a newly allocated `array_t` of `almanac_entry_t` values on success, or
`NULL` on failure. Entries are returned in configured catalogue order. Pointers
returned by `array_get()` are borrowed from the array and must not be freed
individually. Destroy the returned array with `array_destroy()` when finished.

### Persistence

**`bool almanac_serialize(const almanac_t *almanac, string_t **out_type, string_t **out_encoding, void **out_data, size_t *out_len)`**

Serialise an almanac engine into a SQLite-ready payload.

- `almanac` — engine to serialise
- `out_type` — receives a newly allocated type label
- `out_encoding` — receives a newly allocated encoding label
- `out_data` — receives a newly allocated payload buffer
- `out_len` — receives the payload length in bytes

Returns `true` on success. On success, the caller owns `*out_type`,
`*out_encoding`, and `*out_data` and must release them with `string_free()` and
`free()`.

**`almanac_t *almanac_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding)`**

Reconstruct an almanac engine from a serialised payload.

- `data` — payload bytes previously produced by `almanac_serialize()`
- `len` — payload length in bytes
- `type` — stored type label
- `encoding` — stored encoding label

Returns a newly allocated almanac engine on success, or `NULL` if the payload
does not match the expected almanac serialisation format or the configured
engine cannot be reopened.
