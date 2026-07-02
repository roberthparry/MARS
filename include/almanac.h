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
 * @brief One computed almanac position.
 *
 * Values of this type are plain value objects.
 */
typedef struct _almanac_entry_t {
    almanac_body_id_t body_id;
    almanac_body_kind_t body_kind;
    double moment_jd;
    double gha_aries_degrees;
    double sha_degrees;
    double declination_degrees;
    double right_ascension_hours;
    double geocentric_distance_au;
    double heliocentric_distance_au;
    double phase_angle_degrees;
    double visual_magnitude;
} almanac_entry_t;

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
 * @brief One exact Moon phase event.
 */
typedef struct _almanac_moon_phase_event_t {
    almanac_moon_phase_kind_t kind;
    double moment_jd;
    double phase_angle_degrees;
    double illuminated_fraction;
} almanac_moon_phase_event_t;

/**
 * @brief One local civil event time.
 */
typedef struct _almanac_event_time_t {
    bool valid;
    double jd;
    short year;
    month_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    double second;
} almanac_event_time_t;

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
 * @brief One local sunrise or sunset result from the almanac ephemeris.
 *
 * The @c jd member is the absolute event Julian date used for the ephemeris;
 * @c local_time contains the jurisdiction-local civil date and time.
 */
typedef struct _almanac_sun_event_t {
    almanac_rise_set_status_t status;
    double jd;
    almanac_event_time_t local_time;
    double azimuth_degrees;
} almanac_sun_event_t;

/**
 * @brief Sunrise and sunset circumstances for one local civil day.
 */
typedef struct _almanac_sun_times_t {
    almanac_sun_event_t sunrise;
    almanac_sun_event_t sunset;
} almanac_sun_times_t;

/**
 * @brief One local moonrise or moonset result from the almanac ephemeris.
 */
typedef struct _almanac_moon_event_t {
    almanac_rise_set_status_t status;
    double jd;
    almanac_event_time_t local_time;
    double azimuth_degrees;
} almanac_moon_event_t;

/**
 * @brief Moonrise and moonset circumstances for one local civil day.
 */
typedef struct _almanac_moon_times_t {
    almanac_moon_event_t moonrise;
    almanac_moon_event_t moonset;
} almanac_moon_times_t;

/**
 * @brief Broad solar eclipse classification.
 */
typedef enum _almanac_solar_eclipse_kind_t {
    ALMANAC_SOLAR_ECLIPSE_PARTIAL = 0,
    ALMANAC_SOLAR_ECLIPSE_ANNULAR,
    ALMANAC_SOLAR_ECLIPSE_TOTAL
} almanac_solar_eclipse_kind_t;

/**
 * @brief One solar eclipse event approximation.
 */
typedef struct _almanac_solar_eclipse_t {
    almanac_solar_eclipse_kind_t kind;
    almanac_event_time_t first_contact_time;
    double first_contact_jd;
    almanac_event_time_t second_contact_time;
    double second_contact_jd;
    almanac_event_time_t greatest_eclipse_time;
    double greatest_eclipse_jd;
    almanac_event_time_t third_contact_time;
    double third_contact_jd;
    almanac_event_time_t fourth_contact_time;
    double fourth_contact_jd;
    double separation_degrees;
    double magnitude;
    double totality_percent;
    double sun_semi_diameter_degrees;
    double moon_semi_diameter_degrees;
    bool central;
} almanac_solar_eclipse_t;

/**
 * @brief Nearest known observing point for solar totality.
 */
typedef struct _almanac_solar_totality_location_t {
    bool found;
    almanac_event_time_t greatest_eclipse_time;
    double greatest_eclipse_jd;
    double latitude_degrees;
    double longitude_degrees;
    double distance_km;
    double magnitude;
    double totality_percent;
} almanac_solar_totality_location_t;

/**
 * @brief Broad lunar eclipse classification.
 */
typedef enum _almanac_lunar_eclipse_kind_t {
    ALMANAC_LUNAR_ECLIPSE_PENUMBRAL = 0,
    ALMANAC_LUNAR_ECLIPSE_PARTIAL,
    ALMANAC_LUNAR_ECLIPSE_TOTAL
} almanac_lunar_eclipse_kind_t;

/**
 * @brief One lunar eclipse event approximation.
 */
typedef struct _almanac_lunar_eclipse_t {
    almanac_lunar_eclipse_kind_t kind;
    almanac_event_time_t p1_contact_time;
    double p1_contact_jd;
    almanac_event_time_t u1_contact_time;
    double u1_contact_jd;
    almanac_event_time_t u2_contact_time;
    double u2_contact_jd;
    almanac_event_time_t greatest_eclipse_time;
    double greatest_eclipse_jd;
    almanac_event_time_t u3_contact_time;
    double u3_contact_jd;
    almanac_event_time_t u4_contact_time;
    double u4_contact_jd;
    almanac_event_time_t p4_contact_time;
    double p4_contact_jd;
    double opposition_error_degrees;
    double umbral_magnitude;
    double penumbral_magnitude;
    double totality_percent;
    double umbral_radius_degrees;
    double penumbral_radius_degrees;
    double moon_semi_diameter_degrees;
} almanac_lunar_eclipse_t;

/**
 * @brief One Mercury or Venus solar transit event approximation.
 */
typedef struct _almanac_solar_transit_t {
    almanac_body_id_t body_id;
    almanac_event_time_t first_contact_time;
    double first_contact_jd;
    almanac_event_time_t second_contact_time;
    double second_contact_jd;
    almanac_event_time_t greatest_transit_time;
    double greatest_transit_jd;
    almanac_event_time_t third_contact_time;
    double third_contact_jd;
    almanac_event_time_t fourth_contact_time;
    double fourth_contact_jd;
    double separation_degrees;
    double solar_semi_diameter_degrees;
    double planet_semi_diameter_degrees;
    double chord_distance_fraction;
    bool interior;
} almanac_solar_transit_t;

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
 * @brief Compute SHA and declination for one catalogued body by enum id.
 *
 * The returned entry includes the matching GHA Aries for the same moment.
 *
 * @param almanac open almanac engine.
 * @param body_id stable body identifier such as @c ALMANAC_BODY_ID_SUN.
 * @param moment civil moment to evaluate.
 * @param out output entry to fill.
 * @return @c true on success, otherwise @c false.
 */
bool almanac_lookup_body(almanac_t *almanac,
                         almanac_body_id_t body_id,
                         const datetime_t *moment,
                         almanac_entry_t *out);

/**
 * @brief Compute SHA and declination for one catalogued body by legacy code.
 *
 * This is a compatibility wrapper over @c almanac_lookup_body(); database and
 * runtime model lookups are performed with @c almanac_body_id_t.
 *
 * @param almanac open almanac engine.
 * @param body_code body code such as @c SUN, @c MARS, or @c SIRIUS.
 * @param moment civil moment to evaluate.
 * @param out output entry to fill.
 * @return @c true on success, otherwise @c false.
 */
bool almanac_lookup(almanac_t *almanac,
                    const char *body_code,
                    const datetime_t *moment,
                    almanac_entry_t *out);

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
 * @brief Find the nearest place where a solar eclipse is total.
 *
 * The search is intended for UI guidance around a local partial eclipse. It
 * returns the nearest point found on Earth where the same eclipse is total,
 * together with the local greatest-eclipse instant for that point.
 *
 * @param almanac open almanac engine.
 * @param observer original observer location.
 * @param eclipse local solar eclipse event to use as the search seed.
 * @param out output location to fill.
 * @return @c true when the search ran, otherwise @c false.
 */
bool almanac_nearest_solar_totality(almanac_t *almanac,
                                    const almanac_observer_t *observer,
                                    const almanac_solar_eclipse_t *eclipse,
                                    almanac_solar_totality_location_t *out);

/**
 * @brief Find the nearest land location where a solar eclipse is total.
 *
 * This is a UI-oriented companion to @c almanac_nearest_solar_totality().
 * It searches probable land regions and returns the nearest land point found
 * where the same eclipse is total.
 *
 * @param almanac open almanac engine.
 * @param observer original observer location.
 * @param eclipse local solar eclipse event to use as the search seed.
 * @param out output location to fill.
 * @return @c true when the search ran, otherwise @c false.
 */
bool almanac_nearest_solar_totality_land(almanac_t *almanac,
                                         const almanac_observer_t *observer,
                                         const almanac_solar_eclipse_t *eclipse,
                                         almanac_solar_totality_location_t *out);

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
 * The returned array contains @c almanac_entry_t values sorted by configured
 * catalog order. Destroy the returned array with @c array_destroy() when
 * finished.
 *
 * @param almanac open almanac engine.
 * @param moment civil moment to evaluate.
 * @return newly allocated array of @c almanac_entry_t values, or @c NULL on
 *         failure.
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
