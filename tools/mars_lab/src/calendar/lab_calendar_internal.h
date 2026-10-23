/**
 * @file lab_calendar_internal.h
 * @brief Shared private facilities for the native Lab calendar adapters.
 *
 * Calendar-owned translation units share JSON ownership helpers, validated
 * request options, bounded worker execution and UI presentation here.
 * The server should include lab_calendar.h instead.
 */
#ifndef MARS_LAB_CALENDAR_INTERNAL_H
#define MARS_LAB_CALENDAR_INTERNAL_H

#if !defined(MARS_LAB_CALENDAR_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_LAB_CALENDAR_INTERNAL_ACCESS."
#endif

#include <stdbool.h>
#include <time.h>

#include "lab_calendar.h"

/** @brief Render current setup-origin sky information at a supplied UTC instant for native tests and the header route. */
string_t *lab_cal_home_markup(const json_t *origin, time_t now);

/** @brief Read the installed setup origin and return the current independent home-sky header. */
json_t *lab_cal_home_sky(void);

/** @brief Resolve submitted town metadata or the packaged catalogue to an owned IANA zone name. */
string_t *lab_cal_town_timezone(const json_t *options);

/**
 * @brief Set observer classification and sea-horizon policy from the native shoreline hit-test.
 * @param options Borrowed mutable request options with validated latitude and longitude.
 * @note Missing or uncertain shoreline data do not enable horizon dip; no weather request is made.
 */
void lab_cal_surface(json_t *options);

/**
 * @brief Render bounded shoreline imagery for the observer globe.
 * @param payload Borrowed west/south/east/north degree bounds.
 * @param status Optional HTTP status output.
 * @return Owned response containing an image or an explicit availability error.
 */
json_t *lab_cal_globe(const json_t *payload, unsigned *status);

/** @brief Return country metadata for a land point or an explicit catalogue ISO code, without astronomical evaluation. */
json_t *lab_cal_country(const json_t *payload, unsigned *status);

/** @brief Render bounded shared land borders without drawing administrative coastlines. */
json_t *lab_cal_borders(const json_t *payload, unsigned *status);

/** @brief Return an owned close-view river SVG response from jurisdiction data; set the optional HTTP status. */
json_t *lab_cal_rivers(const json_t *payload, unsigned *status);

/**
 * @brief Convert a UTC Julian date using a validated zoneinfo file, restoring TZ afterwards.
 * Invalid zones return false without modifying outputs. Only single-threaded workers may call this function.
 */
bool lab_cal_timezone_at(const string_t *zone, double jd, struct tm *local, double *offset);

/** @brief Resolve a civil date's noon offset in a validated IANA zone; preserve outputs on failure. */
bool lab_cal_timezone_noon(const string_t *zone, const string_t *date, double *offset);

/** @brief Borrow an object member by its ASCII key. */
const json_t *lab_cal_get(const json_t *object, const char *key);

/** @brief Borrow string or number text; absent and other values yield an empty string. */
const char *lab_cal_text(const json_t *object, const char *key);

/** @brief Parse a finite decimal using a string cursor, rejecting trailing syntax. */
bool lab_cal_number(const char *text, double *value);

/** @brief Split on a literal delimiter, preserving empty columns through string views. */
string_t **lab_cal_split(const string_t *text, const char *delimiter, size_t *count);

/** @brief Parse an unsigned decimal integer with a cursor and an explicit bound. */
bool lab_cal_integer(const char *text, unsigned maximum, unsigned *value);

/** @brief Copy a string into an object. */
void lab_cal_set(json_t *object, const char *key, const char *value);

/** @brief Copy a value into an object and release the supplied owned value. */
void lab_cal_take(json_t *object, const char *key, json_t *value);

/** @brief Append a copy of an owned array item and release it. */
void lab_cal_append(json_t *array, json_t *value);

/** @brief Build an error response and set its HTTP status. */
json_t *lab_cal_error(unsigned *status, unsigned code, const char *message);

/** @brief Run one native Lab worker with bounded output; return parsed fields on success. */
json_t *lab_cal_run(const char *name, const json_t *options, unsigned timeout, string_t **diagnostic);

/** @brief Read an environment variable or a named MARS configuration file value. */
string_t *lab_cal_config(const char *filename, const char *key);

/** @brief Add a browser label/value row with an English tooltip for recognised non-English observances. */
void lab_cal_row(json_t *rows, const char *label, const char *value);

/** @brief Add a browser section, copying the supplied rows. */
void lab_cal_section(json_t *sections, const char *title, bool open, const json_t *rows);

/** @brief Convert sections to their plain-text companion field. */
void lab_cal_sections(json_t *response, const char *name, json_t *sections);

/** @brief Prepare the DateTime cards from native fields. */
json_t *lab_cal_datetime(json_t *fields);

/** @brief Prepare a jurisdiction-location response from successful native fields and named-town metadata. */
json_t *lab_cal_location_fields(const json_t *fields, const json_t *options);

/** @brief Prepare weather cards using a bounded native HTTPS request. */
json_t *lab_cal_weather(const json_t *options);

/**
 * @brief Prepare approximate observing conditions using optional weather or a seasonal fallback.
 * @param options Validated observer date, coordinates and elevation; receives temperature,
 *                pressure and a readable provenance note.
 */
void lab_cal_atmosphere(json_t *options);

/**
 * @brief Select weather for the UTC observation hour and prepare native atmospheric inputs.
 * @param options Validated date, UTC time, latitude and elevation; receives conditions and provenance.
 * @param weather Borrowed WeatherAPI response, or NULL to use explicit fallback estimates.
 */
void lab_cal_observing_conditions(json_t *options, const json_t *weather);

/**
 * @brief Resolve an observer's UTC date and time to Unix seconds.
 * @param options Borrowed validated date and time strings.
 * @return UTC epoch seconds, or NaN when the moment cannot be read.
 */
double lab_cal_weather_epoch(const json_t *options);

/**
 * @brief Retain a recent provider observation in the bounded location cache shared by calendar modes.
 * @param options Borrowed request latitude and longitude.
 * @param weather Borrowed provider response; only its current observation is retained.
 * @param now Retrieval time in Unix seconds.
 */
void lab_cal_weather_remember(const json_t *options, const json_t *weather, time_t now);

/**
 * @brief Retrieve a location-matched temperature estimate younger than 48 hours.
 * @param options Borrowed observer coordinates and UTC moment.
 * @param now Current Unix time used to reject expired and future cache records.
 * @return Owned cached observation or NULL; unavailable caches never block evaluation.
 */
json_t *lab_cal_weather_recent(const json_t *options, time_t now);

/** @brief Prepare navigation body rows and upcoming events. */
json_t *lab_cal_almanac(json_t *fields, const json_t *options, unsigned *status);

/**
 * @brief Attach native all/visible worksheet and event presentation variants.
 * @param response Borrowed mutable almanac response; original fields remain intact.
 * @return True on success, false on allocation failure or excessive row/event counts.
 * @details Builds metadata privately before publishing it. Limited to 256 bodies
 * and 64 events; exact action payloads are copied without mathematical rewriting.
 */
bool lab_cal_almanac_presentation(json_t *response);

/** @brief Append a body glyph or phase disc using borrowed Sun and worksheet observer data for orientation. */
bool lab_cal_body_symbol(string_t *out, const json_t *row, const json_t *sun, const json_t *observer);

/** @brief Escape variable text for native HTML content or quoted attributes. */
bool lab_cal_markup_text(string_t *out, const char *value);

/** @brief Build a hidden native sky-strip template from completed result data; caller frees the string. */
string_t *lab_cal_sky_markup(const json_t *response, bool almanac);

/** @brief Append a maximum-eclipse SVG from native event geometry; omit it if geometry is unavailable. */
bool lab_cal_eclipse_symbol(string_t *out, const json_t *event);

/**
 * @brief Render a prepared visibility variant as escaped accessible HTML.
 * @param response Borrowed worksheet headings.
 * @param rows Borrowed prepared body rows, at most 256.
 * @param events Borrowed prepared events, at most 64.
 * @param visible True for the visible-only variant without the visibility column.
 * @return Caller-owned markup, or NULL on allocation or bound failure; release with string_free.
 */
string_t *lab_cal_almanac_markup(const json_t *response, const json_t *rows, const json_t *events, bool visible);

/**
 * @brief Render escaped nearest-totality text and an optional location action.
 * @param text Borrowed display text.
 * @param action Borrowed action fields; NULL or a blank town suppresses the button.
 * @return Caller-owned markup, or NULL on allocation failure; release with string_free.
 */
string_t *lab_cal_totality_markup(const char *text, const json_t *action);

/**
 * @brief Render one calendar or weather section with escaped labels and values.
 * @param title Borrowed section title; empty selects Calendar.
 * @param open Whether the details element starts expanded.
 * @param rows Borrowed label/value rows; empty arrays produce empty markup.
 * @return Caller-owned markup, or NULL on allocation failure; release with string_free.
 */
string_t *lab_cal_section_markup(const char *title, bool open, const json_t *rows);

/** @brief Search named land totality locations for requested events. */
json_t *lab_cal_totality(const json_t *payload, const json_t *options, unsigned *status);

#endif
