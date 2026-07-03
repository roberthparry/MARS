#ifndef MARS_ALMANAC_H
#define MARS_ALMANAC_H

#include <stdbool.h>

#include "array.h"
#include "datetime.h"

/**
 * @brief Opaque astronomical almanac engine backed by configured ephemeris data.
 *
 * An @c almanac_t owns the private resources needed to resolve catalogued stars,
 * planets, and Greenwich Hour Angle of Aries from a configured encrypted
 * almanac database.
 */
typedef struct _almanac_t almanac_t;
typedef struct _jurisdiction_t jurisdiction_t;

/**
 * @brief Catalogued body kind supported by the almanac.
 */
typedef enum _almanac_body_kind_t {
    ALMANAC_BODY_STAR = 0,
    ALMANAC_BODY_PLANET,
    ALMANAC_BODY_SUN,
    ALMANAC_BODY_MOON
} almanac_body_kind_t;

/**
 * @brief Stable body identifier for supported almanac bodies.
 */
typedef enum _almanac_body_id_t {
    ALMANAC_BODY_ID_UNKNOWN = 0,
    ALMANAC_BODY_ID_SUN,
    ALMANAC_BODY_ID_MOON,
    ALMANAC_BODY_ID_MERCURY,
    ALMANAC_BODY_ID_VENUS,
    ALMANAC_BODY_ID_MARS,
    ALMANAC_BODY_ID_JUPITER,
    ALMANAC_BODY_ID_SATURN,
    ALMANAC_BODY_ID_URANUS,
    ALMANAC_BODY_ID_NEPTUNE,
    ALMANAC_BODY_ID_ACAMAR,
    ALMANAC_BODY_ID_ACHERNAR,
    ALMANAC_BODY_ID_ACRUX,
    ALMANAC_BODY_ID_ADHARA,
    ALMANAC_BODY_ID_ALNAIR,
    ALMANAC_BODY_ID_ALDEBARAN,
    ALMANAC_BODY_ID_ALIOTH,
    ALMANAC_BODY_ID_ALKAID,
    ALMANAC_BODY_ID_ALNILAM,
    ALMANAC_BODY_ID_ALPHARD,
    ALMANAC_BODY_ID_ALPHECCA,
    ALMANAC_BODY_ID_ALPHERATZ,
    ALMANAC_BODY_ID_ALTAIR,
    ALMANAC_BODY_ID_ANKAA,
    ALMANAC_BODY_ID_ANTARES,
    ALMANAC_BODY_ID_ARCTURUS,
    ALMANAC_BODY_ID_ATRIA,
    ALMANAC_BODY_ID_AVIOR,
    ALMANAC_BODY_ID_BELLATRIX,
    ALMANAC_BODY_ID_BETELGEUSE,
    ALMANAC_BODY_ID_CANOPUS,
    ALMANAC_BODY_ID_CAPELLA,
    ALMANAC_BODY_ID_DENEB,
    ALMANAC_BODY_ID_DENEBOLA,
    ALMANAC_BODY_ID_DIPHDA,
    ALMANAC_BODY_ID_DUBHE,
    ALMANAC_BODY_ID_ELNATH,
    ALMANAC_BODY_ID_ELTANIN,
    ALMANAC_BODY_ID_ENIF,
    ALMANAC_BODY_ID_FOMALHAUT,
    ALMANAC_BODY_ID_GACRUX,
    ALMANAC_BODY_ID_GIENAH,
    ALMANAC_BODY_ID_HADAR,
    ALMANAC_BODY_ID_HAMAL,
    ALMANAC_BODY_ID_KAUS_AUSTRALIS,
    ALMANAC_BODY_ID_KOCHAB,
    ALMANAC_BODY_ID_MARKAB,
    ALMANAC_BODY_ID_MENKAR,
    ALMANAC_BODY_ID_MENKENT,
    ALMANAC_BODY_ID_MIAPLACIDUS,
    ALMANAC_BODY_ID_MIRFAK,
    ALMANAC_BODY_ID_NUNKI,
    ALMANAC_BODY_ID_PEACOCK,
    ALMANAC_BODY_ID_POLARIS,
    ALMANAC_BODY_ID_POLLUX,
    ALMANAC_BODY_ID_PROCYON,
    ALMANAC_BODY_ID_RASALHAGUE,
    ALMANAC_BODY_ID_REGULUS,
    ALMANAC_BODY_ID_RIGEL,
    ALMANAC_BODY_ID_RIGIL_KENTAURUS,
    ALMANAC_BODY_ID_SABIK,
    ALMANAC_BODY_ID_SCHEDAR,
    ALMANAC_BODY_ID_SHAULA,
    ALMANAC_BODY_ID_SIRIUS,
    ALMANAC_BODY_ID_SPICA,
    ALMANAC_BODY_ID_SUHAIL,
    ALMANAC_BODY_ID_VEGA,
    ALMANAC_BODY_ID_ZUBENELGENUBI,
    ALMANAC_BODY_ID_SIGMA_OCTANTIS,
    ALMANAC_BODY_ID_COUNT
} almanac_body_id_t;

/**
 * @brief Opaque computed almanac position.
 */
typedef struct _almanac_entry_t almanac_entry_t;

/**
 * @brief Parse a body code into its stable enum identifier.
 *
 * This is intended for user-interface and compatibility boundaries. Runtime
 * almanac lookups should carry the returned @c almanac_body_id_t.
 *
 * @param body_code body code such as @c SUN, @c MOON, or @c SIRIUS.
 * @return matching body id, or @c ALMANAC_BODY_ID_UNKNOWN.
 */
almanac_body_id_t almanac_body_id_from_code(const char *body_code);

/**
 * @brief Return the canonical body code for an enum identifier.
 *
 * @param body_id stable body identifier.
 * @return borrowed canonical code, or @c NULL when @p body_id is invalid.
 */
const char *almanac_body_code(almanac_body_id_t body_id);

/**
 * @brief Return a display label for an enum identifier.
 *
 * @param body_id stable body identifier.
 * @return borrowed display label, or @c NULL when @p body_id is invalid.
 */
const char *almanac_body_display_name(almanac_body_id_t body_id);

/**
 * @brief Create a populated almanac entry for one catalogued body by enum id.
 *
 * @param almanac open almanac engine.
 * @param body_id stable body identifier such as @c ALMANAC_BODY_ID_SUN.
 * @param moment civil moment to evaluate.
 * @return newly allocated entry, or @c NULL on failure.
 */
almanac_entry_t *almanac_new_body_entry(almanac_t *almanac,
                                        almanac_body_id_t body_id,
                                        const datetime_t *moment);

/**
 * @brief Create a populated almanac entry for one catalogued body by legacy code.
 *
 * This is a compatibility wrapper over @c almanac_new_body_entry(); database
 * and runtime model lookups are performed with @c almanac_body_id_t.
 *
 * @param almanac open almanac engine.
 * @param body_code body code such as @c SUN, @c MARS, or @c SIRIUS.
 * @param moment civil moment to evaluate.
 * @return newly allocated entry, or @c NULL on failure.
 */
almanac_entry_t *almanac_new_entry(almanac_t *almanac,
                                   const char *body_code,
                                   const datetime_t *moment);

/**
 * @brief Free an almanac entry object allocated by an entry constructor.
 */
void almanac_entry_dealloc(almanac_entry_t *entry);

/**
 * @brief Return the body id stored in an almanac entry.
 */
almanac_body_id_t almanac_entry_body_id(const almanac_entry_t *entry);

/**
 * @brief Return the body kind stored in an almanac entry.
 */
almanac_body_kind_t almanac_entry_body_kind(const almanac_entry_t *entry);

double almanac_entry_moment_jd(const almanac_entry_t *entry);
double almanac_entry_gha_aries_degrees(const almanac_entry_t *entry);
double almanac_entry_sha_degrees(const almanac_entry_t *entry);
double almanac_entry_declination_degrees(const almanac_entry_t *entry);
double almanac_entry_right_ascension_hours(const almanac_entry_t *entry);
double almanac_entry_geocentric_distance_au(const almanac_entry_t *entry);
double almanac_entry_heliocentric_distance_au(const almanac_entry_t *entry);
double almanac_entry_phase_angle_degrees(const almanac_entry_t *entry);
double almanac_entry_visual_magnitude(const almanac_entry_t *entry);

/**
 * @brief One observer location for horizon-style almanac calculations.
 */
typedef struct _almanac_observer_t {
    double latitude_degrees;
    double longitude_degrees;
    double elevation_metres;
} almanac_observer_t;

/**
 * @brief One set of observer-relative almanac observables.
 */
typedef struct _almanac_observables_t {
    double altitude_degrees;
    double azimuth_degrees;
    double semi_diameter_degrees;
    bool above_horizon;
    bool visible;
} almanac_observables_t;

/**
 * @brief Exact phase event kind for the Moon.
 */
typedef enum _almanac_moon_phase_kind_t {
    ALMANAC_MOON_PHASE_NEW = 0,
    ALMANAC_MOON_PHASE_FIRST_QUARTER,
    ALMANAC_MOON_PHASE_FULL,
    ALMANAC_MOON_PHASE_LAST_QUARTER
} almanac_moon_phase_kind_t;

/**
 * @brief Broad phase classification for the Moon and planets.
 */
typedef enum _almanac_phase_class_t {
    ALMANAC_PHASE_UNKNOWN = 0,
    ALMANAC_PHASE_NEW,
    ALMANAC_PHASE_CRESCENT,
    ALMANAC_PHASE_QUARTER,
    ALMANAC_PHASE_GIBBOUS,
    ALMANAC_PHASE_FULL
} almanac_phase_class_t;

/**
 * @brief One set of phase details derived from an almanac body entry.
 */
typedef struct _almanac_phase_details_t {
    double phase_angle_degrees;
    double illuminated_fraction;
    almanac_phase_class_t phase_class;
} almanac_phase_details_t;

/**
 * @brief One local rise, set, contact, or greatest-event time.
 *
 * The @c jd member is the absolute event Julian date used for ephemeris work;
 * @c local_jd is the civil Julian date to use when presenting the event in
 * local time. For non-local events, both values are the same.
 */
typedef struct _almanac_event_time_t {
    bool valid;
    double jd;
    double local_jd;
} almanac_event_time_t;

/**
 * @brief Initialise a datetime from an almanac event's local civil time.
 *
 * @param event_time event time to convert.
 * @param out preallocated datetime to initialise.
 * @return @c true on success, otherwise @c false.
 */
bool almanac_event_time_datetime(const almanac_event_time_t *event_time,
                                 datetime_t *out);

/**
 * @brief One exact Moon phase event.
 */
typedef struct _almanac_moon_phase_event_t {
    almanac_moon_phase_kind_t kind;
    almanac_event_time_t time;
    double phase_angle_degrees;
    double illuminated_fraction;
} almanac_moon_phase_event_t;

/**
 * @brief Status for an almanac rise/set calculation.
 */
typedef enum _almanac_rise_set_status_t {
    ALMANAC_RISE_SET_OK = 0,
    ALMANAC_RISE_SET_NOT_ON_DATE,
    ALMANAC_RISE_SET_NEVER_RISES,
    ALMANAC_RISE_SET_NEVER_SETS,
    ALMANAC_RISE_SET_UNAVAILABLE
} almanac_rise_set_status_t;

/**
 * @brief One local rise or set result from the almanac ephemeris.
 */
typedef struct _almanac_rise_set_event_t {
    almanac_rise_set_status_t status;
    almanac_event_time_t time;
    double azimuth_degrees;
} almanac_rise_set_event_t;

/**
 * @brief Rise and set circumstances for one body on one local civil day.
 */
typedef struct _almanac_rise_set_t {
    almanac_rise_set_event_t rise;
    almanac_rise_set_event_t set;
} almanac_rise_set_t;

/**
 * @brief Sunrise and sunset circumstances for one local civil day.
 */
typedef almanac_rise_set_t almanac_sun_times_t;

/**
 * @brief Moonrise and moonset circumstances for one local civil day.
 */
typedef almanac_rise_set_t almanac_moon_times_t;

/**
 * @brief Broad solar eclipse classification.
 */
typedef enum _almanac_solar_eclipse_kind_t {
    ALMANAC_SOLAR_ECLIPSE_PARTIAL = 0,
    ALMANAC_SOLAR_ECLIPSE_ANNULAR,
    ALMANAC_SOLAR_ECLIPSE_TOTAL
} almanac_solar_eclipse_kind_t;

/**
 * @brief Broad lunar eclipse classification.
 */
typedef enum _almanac_lunar_eclipse_kind_t {
    ALMANAC_LUNAR_ECLIPSE_PENUMBRAL = 0,
    ALMANAC_LUNAR_ECLIPSE_PARTIAL,
    ALMANAC_LUNAR_ECLIPSE_TOTAL
} almanac_lunar_eclipse_kind_t;

/**
 * @brief Named event time within an eclipse or solar-transit record.
 *
 * Solar eclipses and solar transits use first, second, greatest, third, and
 * fourth contact. Lunar eclipses use P1, U1, U2, greatest, U3, U4, and P4.
 */
typedef enum _almanac_event_time_kind_t {
    ALMANAC_EVENT_TIME_FIRST_CONTACT = 0,
    ALMANAC_EVENT_TIME_SECOND_CONTACT,
    ALMANAC_EVENT_TIME_GREATEST,
    ALMANAC_EVENT_TIME_THIRD_CONTACT,
    ALMANAC_EVENT_TIME_FOURTH_CONTACT,
    ALMANAC_EVENT_TIME_P1_CONTACT,
    ALMANAC_EVENT_TIME_U1_CONTACT,
    ALMANAC_EVENT_TIME_U2_CONTACT,
    ALMANAC_EVENT_TIME_U3_CONTACT,
    ALMANAC_EVENT_TIME_U4_CONTACT,
    ALMANAC_EVENT_TIME_P4_CONTACT
} almanac_event_time_kind_t;

/**
 * @brief Opaque solar eclipse event approximation.
 */
typedef struct _almanac_solar_eclipse_t almanac_solar_eclipse_t;

/**
 * @brief Opaque lunar eclipse event approximation.
 */
typedef struct _almanac_lunar_eclipse_t almanac_lunar_eclipse_t;

/**
 * @brief Opaque Mercury or Venus solar-transit event approximation.
 */
typedef struct _almanac_solar_transit_t almanac_solar_transit_t;

/**
 * @brief Open the configured almanac engine.
 *
 * The engine resolves its own configured backing store, using
 * @c MARS_ALMANAC_DB_PATH and @c MARS_ALMANAC_DB_KEY when available, otherwise
 * falling back to @c ~/.mars/almanac/almanac.db and any configured key file.
 *
 * @return newly allocated almanac engine, or @c NULL on failure.
 */
almanac_t *almanac_open(void);

/**
 * @brief Destroy an almanac engine.
 *
 * @param almanac almanac engine to destroy, or @c NULL.
 */
void almanac_close(almanac_t *almanac);

/**
 * @brief Return the last error message recorded by the almanac engine.
 *
 * The returned pointer is borrowed from the engine and remains valid until the
 * next almanac API call on the same engine or until @c almanac_close().
 *
 * @param almanac almanac engine to query.
 * @return borrowed error string, or @c NULL when unavailable.
 */
const char *almanac_last_error(const almanac_t *almanac);

/**
 * @brief Compute Greenwich Hour Angle of Aries for a moment.
 *
 * @param almanac open almanac engine.
 * @param moment civil moment to evaluate.
 * @param gha_aries_degrees output pointer for GHA Aries in degrees [0, 360).
 * @return @c true on success, otherwise @c false.
 */
bool almanac_gha_aries(almanac_t *almanac,
                       const datetime_t *moment,
                       double *gha_aries_degrees);

/**
 * @brief Derive observer-relative observables from one computed almanac entry.
 *
 * This uses the apparent place already resolved into @p body together with the
 * observer location. The first implementation derives horizon observables from
 * the geocentric apparent place and geocentric distance already stored in the
 * entry.
 *
 * @param almanac open almanac engine, used for error reporting.
 * @param body computed almanac entry for the body of interest.
 * @param observer observer location in degrees/metres.
 * @param out output observables to fill.
 * @return @c true on success, otherwise @c false.
 */
bool almanac_observables(almanac_t *almanac,
                         const almanac_entry_t *body,
                         const almanac_observer_t *observer,
                         almanac_observables_t *out);

/**
 * @brief Find accurate local rise and set for one supported body and day.
 *
 * This uses the almanac ephemeris, topocentric observer position, standard
 * apparent-horizon refraction, body semi-diameter, and observer elevation. The
 * supplied jurisdiction is used only to convert the absolute event instants to
 * local civil date/time, including daylight-saving rules.
 *
 * @param almanac open almanac engine.
 * @param jurisdiction open jurisdiction engine for local civil time.
 * @param body_id supported body identifier.
 * @param date local civil date to evaluate; the time component is ignored.
 * @param observer observer latitude, longitude, and elevation.
 * @param out output rise and set circumstances.
 * @return @c true when the day was evaluated, otherwise @c false.
 */
bool almanac_body_rise_set(almanac_t *almanac,
                           jurisdiction_t *jurisdiction,
                           almanac_body_id_t body_id,
                           const datetime_t *date,
                           const almanac_observer_t *observer,
                           almanac_rise_set_t *out);

/**
 * @brief Find accurate local sunrise and sunset for one observer and day.
 *
 * This uses the almanac Sun ephemeris, topocentric observer position,
 * standard apparent-horizon refraction, solar semi-diameter, and observer
 * elevation. The supplied jurisdiction is used only to convert the absolute
 * event instants to local civil date/time, including daylight-saving rules.
 *
 * @param almanac open almanac engine.
 * @param jurisdiction open jurisdiction engine for local civil time.
 * @param date local civil date to evaluate; the time component is ignored.
 * @param observer observer latitude, longitude, and elevation.
 * @param out output sunrise and sunset circumstances.
 * @return @c true when the day was evaluated, otherwise @c false.
 */
bool almanac_sunrise_sunset(almanac_t *almanac,
                            jurisdiction_t *jurisdiction,
                            const datetime_t *date,
                            const almanac_observer_t *observer,
                            almanac_sun_times_t *out);

/**
 * @brief Find accurate local moonrise and moonset for one observer and day.
 *
 * This uses the almanac Moon ephemeris, topocentric observer position,
 * standard apparent-horizon refraction, lunar semi-diameter, and observer
 * elevation. The supplied jurisdiction is used only to convert the absolute
 * event instants to local civil date/time, including daylight-saving rules.
 *
 * @param almanac open almanac engine.
 * @param jurisdiction open jurisdiction engine for local civil time.
 * @param date local civil date to evaluate; the time component is ignored.
 * @param observer observer latitude, longitude, and elevation.
 * @param out output moonrise and moonset circumstances.
 * @return @c true when the day was evaluated, otherwise @c false.
 */
bool almanac_moonrise_moonset(almanac_t *almanac,
                              jurisdiction_t *jurisdiction,
                              const datetime_t *date,
                              const almanac_observer_t *observer,
                              almanac_moon_times_t *out);

/**
 * @brief Derive illuminated fraction and phase classification for a body entry.
 *
 * @param body computed almanac entry for the body of interest.
 * @param out output phase details to fill.
 * @return @c true on success, otherwise @c false.
 */
bool almanac_phase_details(const almanac_entry_t *body,
                           almanac_phase_details_t *out);

/**
 * @brief Find the next exact Moon phase after a civil moment.
 *
 * The returned event moment is recorded as a local civil Julian Day.
 *
 * @param almanac open almanac engine.
 * @param after starting moment after which to search.
 * @param kind exact phase kind to search for.
 * @param out output event to fill.
 * @return @c true on success, otherwise @c false.
 */
bool almanac_next_moon_phase_exact(almanac_t *almanac,
                                   const datetime_t *after,
                                   almanac_moon_phase_kind_t kind,
                                   almanac_moon_phase_event_t *out);

/**
 * @brief Find solar eclipses within a civil time window.
 *
 * The returned array contains @c almanac_solar_eclipse_t values sorted by
 * event time. Destroy it with @c array_destroy() when finished.
 *
 * @param almanac open almanac engine.
 * @param observer observer location for local eclipse circumstances.
 * @param start inclusive window start.
 * @param end inclusive window end.
 * @return newly allocated array of @c almanac_solar_eclipse_t values, or
 *         @c NULL on failure.
 */
array_t *almanac_find_solar_eclipses(almanac_t *almanac,
                                     const almanac_observer_t *observer,
                                     const datetime_t *start,
                                     const datetime_t *end);

/**
 * @brief Return the classification for a solar eclipse event.
 */
almanac_solar_eclipse_kind_t almanac_solar_eclipse_kind(const almanac_solar_eclipse_t *event);

/**
 * @brief Copy a named time from a solar eclipse event.
 *
 * Valid time kinds are first contact, second contact, greatest, third contact,
 * and fourth contact.
 */
bool almanac_solar_eclipse_time(const almanac_solar_eclipse_t *event,
                                almanac_event_time_kind_t time_kind,
                                almanac_event_time_t *out);

double almanac_solar_eclipse_separation_degrees(const almanac_solar_eclipse_t *event);
double almanac_solar_eclipse_magnitude(const almanac_solar_eclipse_t *event);
double almanac_solar_eclipse_totality_percent(const almanac_solar_eclipse_t *event);
double almanac_solar_eclipse_sun_semi_diameter_degrees(const almanac_solar_eclipse_t *event);
double almanac_solar_eclipse_moon_semi_diameter_degrees(const almanac_solar_eclipse_t *event);
bool almanac_solar_eclipse_is_central(const almanac_solar_eclipse_t *event);

/**
 * @brief Test whether a solar eclipse is locally in progress.
 *
 * The calculation uses the observer-relative topocentric solar-eclipse
 * geometry, so lunar parallax is included in the same way as the local eclipse
 * event search.
 *
 * @param almanac open almanac engine.
 * @param observer observer location for local eclipse circumstances.
 * @param moment civil moment to test.
 * @return @c true when the Sun is above the apparent horizon and the lunar
 *         and solar discs overlap for the observer.
 */
bool almanac_solar_eclipse_in_progress(almanac_t *almanac,
                                       const almanac_observer_t *observer,
                                       const datetime_t *moment);

/**
 * @brief Find lunar eclipses within a civil time window.
 *
 * The returned array contains @c almanac_lunar_eclipse_t values sorted by
 * event time. Destroy it with @c array_destroy() when finished.
 *
 * @param almanac open almanac engine.
 * @param observer observer location for local eclipse visibility.
 * @param start inclusive window start.
 * @param end inclusive window end.
 * @return newly allocated array of @c almanac_lunar_eclipse_t values, or
 *         @c NULL on failure.
 */
array_t *almanac_find_lunar_eclipses(almanac_t *almanac,
                                     const almanac_observer_t *observer,
                                     const datetime_t *start,
                                     const datetime_t *end);

/**
 * @brief Return the classification for a lunar eclipse event.
 */
almanac_lunar_eclipse_kind_t almanac_lunar_eclipse_kind(const almanac_lunar_eclipse_t *event);

/**
 * @brief Copy a named time from a lunar eclipse event.
 *
 * Valid time kinds are P1, U1, U2, greatest, U3, U4, and P4.
 */
bool almanac_lunar_eclipse_time(const almanac_lunar_eclipse_t *event,
                                almanac_event_time_kind_t time_kind,
                                almanac_event_time_t *out);

double almanac_lunar_eclipse_opposition_error_degrees(const almanac_lunar_eclipse_t *event);
double almanac_lunar_eclipse_umbral_magnitude(const almanac_lunar_eclipse_t *event);
double almanac_lunar_eclipse_penumbral_magnitude(const almanac_lunar_eclipse_t *event);
double almanac_lunar_eclipse_totality_percent(const almanac_lunar_eclipse_t *event);
double almanac_lunar_eclipse_umbral_radius_degrees(const almanac_lunar_eclipse_t *event);
double almanac_lunar_eclipse_penumbral_radius_degrees(const almanac_lunar_eclipse_t *event);
double almanac_lunar_eclipse_moon_semi_diameter_degrees(const almanac_lunar_eclipse_t *event);

/**
 * @brief Find Mercury or Venus transits of the Sun by enum id.
 *
 * The returned array contains @c almanac_solar_transit_t values sorted by
 * event time. Destroy it with @c array_destroy() when finished.
 *
 * @param almanac open almanac engine.
 * @param body_id target planet id, currently expected to be
 *                @c ALMANAC_BODY_ID_MERCURY or @c ALMANAC_BODY_ID_VENUS.
 * @param observer observer location for local transit circumstances.
 * @param start inclusive window start.
 * @param end inclusive window end.
 * @return newly allocated array of @c almanac_solar_transit_t values, or
 *         @c NULL on failure.
 */
array_t *almanac_find_solar_transits_for_body(almanac_t *almanac,
                                              almanac_body_id_t body_id,
                                              const almanac_observer_t *observer,
                                              const datetime_t *start,
                                              const datetime_t *end);

/**
 * @brief Return the transiting body for a solar transit event.
 */
almanac_body_id_t almanac_solar_transit_body_id(const almanac_solar_transit_t *event);

/**
 * @brief Copy a named time from a solar transit event.
 *
 * Valid time kinds are first contact, second contact, greatest, third contact,
 * and fourth contact.
 */
bool almanac_solar_transit_time(const almanac_solar_transit_t *event,
                                almanac_event_time_kind_t time_kind,
                                almanac_event_time_t *out);

double almanac_solar_transit_separation_degrees(const almanac_solar_transit_t *event);
double almanac_solar_transit_solar_semi_diameter_degrees(const almanac_solar_transit_t *event);
double almanac_solar_transit_planet_semi_diameter_degrees(const almanac_solar_transit_t *event);
double almanac_solar_transit_chord_distance_fraction(const almanac_solar_transit_t *event);
bool almanac_solar_transit_is_interior(const almanac_solar_transit_t *event);

/**
 * @brief Find Mercury or Venus transits of the Sun by legacy body code.
 *
 * This is a compatibility wrapper over
 * @c almanac_find_solar_transits_for_body().
 *
 * @param almanac open almanac engine.
 * @param body_code target planet code, currently expected to be @c MERCURY or
 *                  @c VENUS.
 * @param observer observer location for local transit circumstances.
 * @param start inclusive window start.
 * @param end inclusive window end.
 * @return newly allocated array of @c almanac_solar_transit_t values, or
 *         @c NULL on failure.
 */
array_t *almanac_find_solar_transits(almanac_t *almanac,
                                     const char *body_code,
                                     const almanac_observer_t *observer,
                                     const datetime_t *start,
                                     const datetime_t *end);

/**
 * @brief Compute SHA and declination for every enabled catalogued body.
 *
 * The returned array owns opaque @c almanac_entry_t entries sorted by
 * configured catalog order. Entries returned by @c array_get() are borrowed
 * from the array. Destroy the returned array with @c array_destroy() when
 * finished.
 *
 * @param almanac open almanac engine.
 * @param moment civil moment to evaluate.
 * @return newly allocated array of opaque @c almanac_entry_t entries, or
 *         @c NULL on failure.
 */
array_t *almanac_snapshot(almanac_t *almanac, const datetime_t *moment);

/**
 * @brief Serialise an almanac engine into a SQLite-ready payload.
 *
 * The payload records that the configured almanac engine should be reopened;
 * it does not attempt to serialise private database handles or cached model
 * state. On success, the caller owns @p out_type, @p out_encoding, and
 * @p out_data and must release them with @c string_free() and @c free().
 *
 * @param almanac Almanac engine to serialise.
 * @param out_type Receives a newly allocated type label.
 * @param out_encoding Receives a newly allocated encoding label.
 * @param out_data Receives a newly allocated payload buffer.
 * @param out_len Receives the payload length in bytes.
 * @return @c true on success, otherwise @c false.
 */
bool almanac_serialize(const almanac_t *almanac,
                       string_t **out_type,
                       string_t **out_encoding,
                       void **out_data,
                       size_t *out_len);

/**
 * @brief Reconstruct an almanac engine from a serialised payload.
 *
 * @param data Serialised payload bytes.
 * @param len Payload length in bytes.
 * @param type Stored type label.
 * @param encoding Stored encoding label.
 * @return Newly allocated almanac engine on success, otherwise @c NULL.
 */
almanac_t *almanac_deserialise(const void *data,
                               size_t len,
                               const string_t *type,
                               const string_t *encoding);

#endif /* MARS_ALMANAC_H */
