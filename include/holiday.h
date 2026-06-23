#ifndef MARS_HOLIDAY_H
#define MARS_HOLIDAY_H

#include <stdbool.h>

#include "array.h"
#include "datetime.h"

/**
 * @brief Opaque holiday rule engine backed by configured holiday rule data.
 *
 * A @c holiday_t owns the private resources needed to resolve holiday rules,
 * observance shifts, and dated exceptions for a jurisdiction over a requested
 * date range.
 */
typedef struct _holiday_t holiday_t;

/**
 * @brief One holiday occurrence.
 *
 * When values of this type are yielded to @c holiday_visit_fn, the date handle
 * and string pointers are borrowed and remain valid only for the duration of
 * the callback.
 *
 * When values of this type are returned from @c holiday_between() inside an
 * @c array_t, each element owns its own date and text storage. Destroy the
 * returned array with @c array_destroy() when you are finished with it.
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
typedef bool (*holiday_visit_fn)(const holiday_event_t *event, void *ctx);

/**
 * @brief Open a holiday engine over the configured holiday rule source.
 *
 * The engine resolves its own configured backing store and owns all resources
 * needed for holiday lookup.
 *
 * @param jurisdiction jurisdiction code such as @c GB-ENG, @c ZA, or @c NL,
 *        or @c NULL to use the machine default jurisdiction.
 *
 * When @p jurisdiction is @c NULL or empty, the engine assumes a default
 * jurisdiction derived from the local machine configuration, with @c GB mapped
 * to @c GB-ENG. If no usable default can be derived, @c GB-ENG is used as the
 * final fallback.
 *
 * @return newly allocated holiday engine, or @c NULL on allocation failure.
 */
holiday_t *holiday_open(const char *jurisdiction);

/**
 * @brief Destroy a holiday engine.
 *
 * This releases the engine's private resources.
 *
 * @param holiday holiday engine to destroy, or @c NULL.
 */
void holiday_close(holiday_t *holiday);

/**
 * @brief Return the last error message recorded by the holiday engine.
 *
 * The returned pointer is borrowed from the engine and remains valid until the
 * next holiday API call on the same engine or until holiday_close().
 *
 * @param holiday holiday engine to query.
 * @return borrowed error string, or @c NULL when unavailable.
 */
const char *holiday_last_error(const holiday_t *holiday);

/**
 * @brief Return all holidays in the inclusive range [@p start, @p end].
 *
 * The returned array contains @c holiday_event_t values and owns all event
 * storage. Destroy the array with @c array_destroy() when finished.
 *
 * @param holiday open holiday engine.
 * @param start inclusive start date.
 * @param end inclusive end date.
 * @return newly allocated array of @c holiday_event_t values, or @c NULL on
 *         failure.
 */
array_t *holiday_between(holiday_t *holiday,
                         const datetime_t *start,
                         const datetime_t *end);

/**
 * @brief Return whether @p date falls on a weekend in this jurisdiction.
 *
 * @param holiday open holiday engine.
 * @param date date to test.
 * @return @c true when @p date is a weekend day, otherwise @c false.
 */
bool holiday_is_weekend(holiday_t *holiday, const datetime_t *date);

/**
 * @brief Return whether @p date is a national holiday in this jurisdiction.
 *
 * @param holiday open holiday engine.
 * @param date date to test.
 * @return @c true when @p date is a national holiday, otherwise @c false.
 */
bool holiday_is_national_holiday(holiday_t *holiday, const datetime_t *date);

/**
 * @brief Count working days in the inclusive range [@p start, @p end].
 *
 * A working day is any date in the range that is neither a weekend nor a
 * holiday in this jurisdiction.
 *
 * @param holiday open holiday engine.
 * @param start inclusive start date.
 * @param end inclusive end date.
 * @return number of working days, or -1 on failure.
 */
long holiday_working_days_between(holiday_t *holiday,
                                  const datetime_t *start,
                                  const datetime_t *end);

/**
 * @brief Enumerate holidays in the inclusive range [@p start, @p end].
 *
 * @param holiday open holiday engine.
 * @param start inclusive start date.
 * @param end inclusive end date.
 * @param visitor callback invoked once per holiday occurrence in ascending date
 *        order.
 * @param ctx caller-owned context pointer passed to @p visitor.
 * @return @c true on success, or @c false if rule loading/evaluation failed.
 */
bool holiday_each_between(holiday_t *holiday,
                          const datetime_t *start,
                          const datetime_t *end,
                          holiday_visit_fn visitor,
                          void *ctx);

#endif /* MARS_HOLIDAY_H */
