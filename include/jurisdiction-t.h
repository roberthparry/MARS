/**
 * @file jurisdiction-t.h
 * @brief Jurisdiction-aware holidays, working days and local-time rules.
 *
 * The configured jurisdiction engine combines rule data with datetime_t values to
 * answer public-holiday, observance, weekend and working-day queries. It also
 * provides jurisdiction-specific timezone and daylight-saving information, including
 * dated exceptions represented in the installed catalogue.
 * Read-only visitors enumerate jurisdiction choices, representative locations and
 * towns, retaining stored coordinate precision and explicit default-town markers.
 * A separate visitor exposes the machine's installed calendar origin without substituting a selected jurisdiction.
 * Indexed country presentation metadata supplies catalogue-owned names and offline flag artwork by ISO code.
 * Geographic country identification belongs to globe_t; choosing a jurisdiction does not establish a point's country.
 * A geographic adapter opens independent globe_t readers on the same catalogue;
 * river geometry remains separate from calendar policy and shoreline classification.
 *
 * Use this module for business calendars and calculations whose result depends on
 * local rules, rather than calendar arithmetic alone. Results depend on the
 * coverage and currency of the configured database; they do not establish legal
 * requirements independently of that data. Use datetime_t for the underlying
 * date representation and ordinary date calculations.
 */

#ifndef MARS_JURISDICTION_H
#define MARS_JURISDICTION_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "array-t.h"
#include "datetime-t.h"
#include "globe-t.h"

/**
 * @brief Opaque jurisdiction policy engine backed by configured rule data.
 *
 * A @c jurisdiction_t owns the private resources needed to resolve holiday
 * rules, observance shifts, weekend policy, timezone defaults, daylight-saving
 * behaviour, and dated exceptions for a requested jurisdiction.
 */
typedef struct _jurisdiction_t jurisdiction_t;

/**
 * @brief Open an owned globe controller borrowing this engine's configured jurisdiction connection.
 * @param jurisdiction Borrowed open engine, which must outlive the returned reader.
 * @return Owned controller to release with globe_close(), or NULL on invalid input or allocation failure.
 * @note Query globe_has_layer() for availability; withdrawn WDBII geometry is never exposed.
 */
globe_t *jurisdict_globe_open(jurisdiction_t *jurisdiction);

/**
 * @brief Read presentation metadata by uppercase ISO country or subdivision code.
 * Subdivisions without artwork fall back to their parent country's name and flag.
 * Required name and flag outputs receive owned strings, released with string_free. A missing optional
 * flag gives NULL while the country name remains available. False clears both outputs. This does not
 * infer a country from the selected jurisdiction, coordinates or a nearby town.
 */
bool jurisdict_country(jurisdiction_t *jurisdiction, const char *code, string_t **name, string_t **flag_svg);

/** @brief Borrowed geographic place record; its definition is owned by globe-t.h. */
typedef globe_place_t jurisdict_place_t;

/**
 * @brief Visit a jurisdiction choice; return false to stop successfully.
 * @param code Borrowed jurisdiction code, valid during this callback only.
 * @param label Borrowed name, prefixed with the parent name for subdivisions.
 * @param context Caller-owned context passed unchanged.
 * @return True to continue, false to stop.
 */
typedef bool (*jurisdict_choice_visit_fn)(const char *code, const char *label, void *context);

/**
 * @brief Visit a location or town; return false to stop successfully.
 * @param place Borrowed record valid during this callback only.
 * @param context Caller-owned context passed unchanged.
 * @return True to continue, false to stop.
 */
typedef bool (*jurisdict_place_visit_fn)(const jurisdict_place_t *place, void *context);

/**
 * @brief Visit the machine origin saved by calendar setup, independently of selected jurisdictions.
 * @param jurisdiction Open engine using the configured jurisdiction database.
 * @param visitor Callback receiving one borrowed place; its return value is ignored.
 * @param context Caller-owned callback context.
 * @return True when a complete setup origin was visited; false when missing or unreadable.
 */
bool jurisdict_setup_origin(jurisdiction_t *jurisdiction, jurisdict_place_visit_fn visitor, void *context);

/**
 * @brief Enumerate database jurisdiction choices in case-insensitive label order.
 * @param jurisdiction Open engine; enumeration covers the whole database, not just its selected code.
 * @param visitor Required callback; must not close or re-enter this engine.
 * @param context Caller-owned context, optionally NULL.
 * @return True on completion or requested early stop, false on invalid arguments or database failure.
 */
bool jurisdict_each_choice(jurisdiction_t *jurisdiction, jurisdict_choice_visit_fn visitor, void *context);

/**
 * @brief Enumerate database default locations in jurisdiction-code order.
 * @param jurisdiction Open engine; enumeration covers all complete database location records.
 * @param visitor Required callback; must not close or re-enter this engine.
 * @param context Caller-owned context, optionally NULL.
 * @return True on completion or requested early stop, false on invalid arguments or database failure.
 */
bool jurisdict_each_location(jurisdiction_t *jurisdiction, jurisdict_place_visit_fn visitor, void *context);

/**
 * @brief Enumerate database towns by jurisdiction, default first, then case-insensitive name.
 * @param jurisdiction Open engine; enumeration covers all complete database town records.
 * @param visitor Required callback; must not close or re-enter this engine.
 * @param context Caller-owned context, optionally NULL.
 * @return True on completion or requested early stop, false on invalid arguments or database failure.
 */
bool jurisdict_each_town(jurisdiction_t *jurisdiction, jurisdict_place_visit_fn visitor, void *context);

/**
 * @brief Visit an indexed batch of towns in ascending database-ID order.
 * @param jurisdiction Open catalogue engine.
 * @param after_id Last ID from the previous batch, or zero initially.
 * @param limit Maximum rows, from 1 to 1024.
 * @param visitor Borrowed-record callback; false stops this batch early.
 * @param context Caller-owned callback context.
 * @param next_id Last visited ID, or zero for an empty batch. Continue until zero.
 * @return True on success. Cursors are valid only while the installed database is unchanged.
 */
bool jurisdict_each_town_batch(jurisdiction_t *jurisdiction, int64_t after_id, size_t limit,
                               jurisdict_place_visit_fn visitor, void *context, int64_t *next_id);

/**
 * @brief Enumerate one jurisdiction's towns in bounded, indexed ID pages.
 *
 * The code must be non-empty; parent and child jurisdictions are not included implicitly.
 * Cursor, limit and borrowed callback semantics follow jurisdict_each_town_batch().
 */
bool jurisdict_each_town_jurisdiction_batch(jurisdiction_t *jurisdiction, const char *code, int64_t after_id, size_t limit,
                                            jurisdict_place_visit_fn visitor, void *context, int64_t *next_id);

/**
 * @brief Enumerate an ascending-ID town page within five degrees of a centre on both axes.
 * Latitude must be finite and within [-90,90], longitude within [-180,180]. Longitude
 * wraps at the date line. This is a rectangular priority window, not a distance ranking.
 * Limit, borrowed callback records and next_id follow jurisdict_each_town_batch().
 * Keep the centre unchanged while paging; restart at ID zero for another centre.
 * Packaged databases index numeric latitude; older databases remain readable.
 */
bool jurisdict_each_town_near_batch(jurisdiction_t *jurisdiction, double latitude, double longitude,
                                    int64_t after_id, size_t limit, jurisdict_place_visit_fn visitor,
                                    void *context, int64_t *next_id);

/**
 * @brief One holiday occurrence.
 *
 * When values of this type are yielded to @c jurisdict_visit_fn, the date
 * handle and string pointers are borrowed and remain valid only for the
 * duration of the callback.
 *
 * When values of this type are returned from
 * @c jurisdict_holidays_between() inside an @c array_t, each element owns
 * its own date and text storage. Destroy the returned array with
 * @c array_destroy() when you are finished with it.
 */
typedef struct _holiday_event_t {
    int holiday_id;
    int rule_id;
    int event_year;
    const datetime_t *holiday_date;
    const char *holiday_name;
    const char *holiday_class;
    bool derived_from_observance;
} holiday_event_t;

/**
 * @brief Visitor callback used when enumerating holiday occurrences.
 *
 * Return @c true to continue enumeration, or @c false to stop early without
 * treating the walk as an error.
 *
 * @param event borrowed event view for the current holiday occurrence.
 * @param ctx caller-supplied context pointer passed through unchanged.
 * @return @c true to continue, @c false to stop iteration early.
 */
typedef bool (*jurisdict_visit_fn)(const holiday_event_t *event, void *ctx);

/**
 * @brief Open a jurisdiction engine over the configured rule source.
 *
 * The engine resolves its own configured backing store and owns all resources
 * needed for jurisdiction-aware calendar lookups.
 *
 * @param jurisdiction_code jurisdiction code such as @c GB-ENG, @c ZA, or
 *        @c NL, or @c NULL to use the machine default jurisdiction.
 *
 * When @p jurisdiction_code is @c NULL or empty, the engine assumes a default
 * jurisdiction derived from the local machine configuration, with @c GB mapped
 * to @c GB-ENG. If no usable default can be derived, @c GB-ENG is used as the
 * final fallback.
 *
 * An empty or absent configured database key selects plaintext SQLite. A non-empty key selects encrypted
 * storage and never falls back to plaintext after failure. Existing files are not converted when opening.
 * @return newly allocated jurisdiction engine, or @c NULL on allocation or database-open failure.
 */
jurisdiction_t *jurisdict_open(const char *jurisdiction_code);

/**
 * @brief Destroy a jurisdiction engine.
 *
 * This releases the engine's private resources.
 *
 * @param jurisdiction jurisdiction engine to destroy, or @c NULL.
 */
void jurisdict_close(jurisdiction_t *jurisdiction);

/**
 * @brief Return the last error message recorded by the jurisdiction engine.
 *
 * The returned pointer is borrowed from the engine and remains valid until the
 * next jurisdiction API call on the same engine or until jurisdict_close().
 *
 * @param jurisdiction jurisdiction engine to query.
 * @return borrowed error string, or @c NULL when unavailable.
 */
const char *jurisdict_last_error(const jurisdiction_t *jurisdiction);

/**
 * @brief Return a representative default location for this jurisdiction.
 *
 * The returned latitude and longitude are intended as a practical observation
 * point for UI defaults, typically a national or subdivision capital.
 *
 * @param jurisdiction open jurisdiction engine.
 * @param latitude output latitude pointer.
 * @param longitude output longitude pointer.
 * @return @c true when a default location is available, otherwise @c false.
 */
bool jurisdict_default_location(jurisdiction_t *jurisdiction, double *latitude, double *longitude);

/**
 * @brief Return the jurisdiction-local GMT offset for a date.
 *
 * This resolves the representative timezone configured for the jurisdiction
 * and returns the local GMT offset, including daylight saving when applicable,
 * for the supplied date.
 *
 * @param jurisdiction open jurisdiction engine.
 * @param date date to evaluate.
 * @param offset_hours output offset pointer, in hours east of GMT.
 * @return @c true when the offset is available, otherwise @c false.
 */
bool jurisdict_default_gmt_offset(jurisdiction_t *jurisdiction, const datetime_t *date, double *offset_hours);

/**
 * @brief Return the daylight-saving transition moments for a jurisdiction year.
 *
 * When the jurisdiction observes daylight saving in the requested year,
 * @p clocks_forward and/or @p clocks_back receive newly allocated
 * @c datetime_t values naming the local date and time at which the offset
 * changes.
 * The caller owns any returned dates and must destroy them with
 * @c datetime_dealloc().
 *
 * Jurisdictions without daylight saving return @c true with both outputs left
 * as @c NULL.
 *
 * @param jurisdiction open jurisdiction engine.
 * @param year calendar year to inspect.
 * @param clocks_forward optional output for the local moment clocks move
 *        forward.
 * @param clocks_back optional output for the local moment clocks move back.
 * @return @c true when the query succeeds, otherwise @c false.
 */
bool jurisdict_dst_transition_datetimes(jurisdiction_t *jurisdiction, int year, datetime_t **clocks_forward,
                                        datetime_t **clocks_back);

/**
 * @brief Return daylight-saving transition moments together with offset change.
 *
 * This is the detailed counterpart to
 * @c jurisdict_dst_transition_datetimes(). When a transition exists, the
 * returned moment describes when the change occurs in local civil time, while
 * the paired offset outputs describe the GMT offset immediately before and
 * immediately after the change.
 *
 * @param jurisdiction open jurisdiction engine.
 * @param year calendar year to inspect.
 * @param clocks_forward optional output for the local moment clocks move
 *        forward.
 * @param forward_from_offset_hours optional output for the GMT offset before
 *        the forward transition.
 * @param forward_to_offset_hours optional output for the GMT offset after the
 *        forward transition.
 * @param clocks_back optional output for the local moment clocks move back.
 * @param back_from_offset_hours optional output for the GMT offset before the
 *        backward transition.
 * @param back_to_offset_hours optional output for the GMT offset after the
 *        backward transition.
 * @return @c true when the query succeeds, otherwise @c false.
 */
bool jurisdict_dst_transition_details(jurisdiction_t *jurisdiction, int year, datetime_t **clocks_forward,
                                      double *forward_from_offset_hours, double *forward_to_offset_hours,
                                      datetime_t **clocks_back, double *back_from_offset_hours,
                                      double *back_to_offset_hours);

/**
 * @brief Return all holidays in the inclusive range [@p start, @p end].
 *
 * The returned array contains @c holiday_event_t values and owns all event
 * storage. Destroy the array with @c array_destroy() when finished.
 *
 * @param jurisdiction open jurisdiction engine.
 * @param start inclusive start date.
 * @param end inclusive end date.
 * @return newly allocated array of @c holiday_event_t values, or @c NULL on
 *         failure.
 */
array_t *jurisdict_holidays_between(jurisdiction_t *jurisdiction, const datetime_t *start, const datetime_t *end);

/**
 * @brief Return whether @p date falls on a weekend in this jurisdiction.
 *
 * @param jurisdiction open jurisdiction engine.
 * @param date date to test.
 * @return @c true when @p date is a weekend day, otherwise @c false.
 */
bool jurisdict_is_weekend(jurisdiction_t *jurisdiction, const datetime_t *date);

/**
 * @brief Return whether @p date is a national holiday in this jurisdiction.
 *
 * @param jurisdiction open jurisdiction engine.
 * @param date date to test.
 * @return @c true when @p date is a national holiday, otherwise @c false.
 */
bool jurisdict_is_national_holiday(jurisdiction_t *jurisdiction, const datetime_t *date);

/**
 * @brief Count working days in the inclusive range [@p start, @p end].
 *
 * A working day is any date in the range that is neither a weekend nor a
 * holiday in this jurisdiction.
 *
 * @param jurisdiction open jurisdiction engine.
 * @param start inclusive start date.
 * @param end inclusive end date.
 * @return number of working days, or -1 on failure.
 */
long jurisdict_working_days_between(jurisdiction_t *jurisdiction, const datetime_t *start, const datetime_t *end);

/**
 * @brief Enumerate holidays in the inclusive range [@p start, @p end].
 *
 * @param jurisdiction open jurisdiction engine.
 * @param start inclusive start date.
 * @param end inclusive end date.
 * @param visitor callback invoked once per holiday occurrence in ascending date
 *        order.
 * @param ctx caller-owned context pointer passed to @p visitor.
 * @return @c true on success, or @c false if rule loading/evaluation failed.
 */
bool jurisdict_each_holiday_between(jurisdiction_t *jurisdiction, const datetime_t *start, const datetime_t *end,
                                    jurisdict_visit_fn visitor, void *ctx);

/**
 * @brief Serialise a jurisdiction engine into a SQLite-ready payload.
 *
 * The payload stores the active jurisdiction code, not the private database
 * handle or cached rule state. On success, the caller owns @p out_type,
 * @p out_encoding, and @p out_data and must release them with
 * @c string_free() and @c free().
 *
 * @param jurisdiction Jurisdiction engine to serialise.
 * @param out_type Receives a newly allocated type label.
 * @param out_encoding Receives a newly allocated encoding label.
 * @param out_data Receives a newly allocated payload buffer.
 * @param out_len Receives the payload length in bytes.
 * @return @c true on success, otherwise @c false.
 */
bool jurisdict_serialize(const jurisdiction_t *jurisdiction, string_t **out_type, string_t **out_encoding,
                         void **out_data, size_t *out_len);

/**
 * @brief Reconstruct a jurisdiction engine from a serialised payload.
 *
 * @param data Serialised payload bytes.
 * @param len Payload length in bytes.
 * @param type Stored type label.
 * @param encoding Stored encoding label.
 * @return Newly allocated jurisdiction engine on success, otherwise @c NULL.
 */
jurisdiction_t *jurisdict_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding);

#endif /* MARS_JURISDICTION_H */
