#ifndef MARS_JURISDICTION_H
#define MARS_JURISDICTION_H

#include <stdbool.h>

#include "array.h"
#include "datetime.h"

/**
 * @brief Opaque jurisdiction policy engine backed by configured rule data.
 *
 * A @c jurisdiction_t owns the private resources needed to resolve holiday
 * rules, observance shifts, weekend policy, timezone defaults, daylight-saving
 * behaviour, and dated exceptions for a requested jurisdiction.
 */
typedef struct _jurisdiction_t jurisdiction_t;

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
 * @return newly allocated jurisdiction engine, or @c NULL on allocation
 *         failure.
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
bool jurisdict_default_location(jurisdiction_t *jurisdiction,
                                double *latitude,
                                double *longitude);

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
bool jurisdict_default_gmt_offset(jurisdiction_t *jurisdiction,
                                  const datetime_t *date,
                                  double *offset_hours);

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
array_t *jurisdict_holidays_between(jurisdiction_t *jurisdiction,
                                    const datetime_t *start,
                                    const datetime_t *end);

/**
 * @brief Return whether @p date falls on a weekend in this jurisdiction.
 *
 * @param jurisdiction open jurisdiction engine.
 * @param date date to test.
 * @return @c true when @p date is a weekend day, otherwise @c false.
 */
bool jurisdict_is_weekend(jurisdiction_t *jurisdiction,
                          const datetime_t *date);

/**
 * @brief Return whether @p date is a national holiday in this jurisdiction.
 *
 * @param jurisdiction open jurisdiction engine.
 * @param date date to test.
 * @return @c true when @p date is a national holiday, otherwise @c false.
 */
bool jurisdict_is_national_holiday(jurisdiction_t *jurisdiction,
                                   const datetime_t *date);

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
long jurisdict_working_days_between(jurisdiction_t *jurisdiction,
                                    const datetime_t *start,
                                    const datetime_t *end);

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
bool jurisdict_each_holiday_between(jurisdiction_t *jurisdiction,
                                    const datetime_t *start,
                                    const datetime_t *end,
                                    jurisdict_visit_fn visitor,
                                    void *ctx);

#endif /* MARS_JURISDICTION_H */
